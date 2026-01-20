/* tcpverbs.c - POSIX TCP networking implementation for Frontier CLI
 *
 * Phase 1A: Core TCP socket operations (blocking I/O, client-side)
 * - tcp.openAddrStream(addr, port) - Direct IP connection
 * - tcp.readStream(stream, bytes) - Non-blocking read
 * - tcp.writeStream(stream, data) - Blocking write
 * - tcp.closeStream(stream) - Graceful close (FIN)
 * - tcp.abortStream(stream) - Immediate close (RST)
 * - tcp.countConnections() - Active stream count
 *
 * Phase 1B: DNS and address operations
 * - tcp.addressEncode(ipString) - Convert dotted decimal to long
 * - tcp.addressDecode(addr) - Convert long to dotted decimal
 * - tcp.nameToAddress(hostname) - DNS lookup (blocking)
 * - tcp.addressToName(addr) - Reverse DNS (blocking)
 * - tcp.openNameStream(hostname, port) - DNS + connect
 *
 * Platform: macOS/Linux (POSIX-compliant BSD sockets)
 * Thread Safety: Mutex-protected context for Phase 1
 *
 * Date: 2026-01-20
 */

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>

#include "frontier.h"
#include "standard.h"
#include "tcpverbs.h"
#include "langexternal.h"
#include "langinternal.h"
#include "memory.h"
#include "strings.h"
#include "logging.h"

/* Global TCP Context */
static tcp_context_t g_tcp_context;

/* Mutex Macros */
#define TCP_LOCK()   pthread_mutex_lock(&g_tcp_context.mutex)
#define TCP_UNLOCK() pthread_mutex_unlock(&g_tcp_context.mutex)

/* ========================================================================
 * Internal Helper Functions
 * ======================================================================== */

/* Map POSIX errno to tcp_error_t */
tcp_error_t tcp_map_errno(int err) {
    switch (err) {
        case ENOMEM:
            return TCP_ERR_NO_MEMORY;
        case ETIMEDOUT:
            return TCP_ERR_TIMEOUT;
        case ECONNREFUSED:
        case EHOSTUNREACH:
        case ENETUNREACH:
            return TCP_ERR_CONNECTION_FAILED;
        default:
            return TCP_ERR_SOCKET_ERROR;
    }
}

/* Set script error with descriptive message */
void tcp_set_error(tcp_error_t err, const char *detail) {
    char error_msg[256];

    switch (err) {
        case TCP_ERR_INVALID_STREAM:
            snprintf(error_msg, sizeof(error_msg), "Invalid stream ID: %s", detail);
            break;
        case TCP_ERR_CONNECTION_FAILED:
            snprintf(error_msg, sizeof(error_msg), "Connection failed: %s", detail);
            break;
        case TCP_ERR_DNS_FAILED:
            snprintf(error_msg, sizeof(error_msg), "DNS lookup failed: %s", detail);
            break;
        case TCP_ERR_NO_MEMORY:
            snprintf(error_msg, sizeof(error_msg), "Out of memory: %s", detail);
            break;
        case TCP_ERR_TIMEOUT:
            snprintf(error_msg, sizeof(error_msg), "Operation timed out: %s", detail);
            break;
        case TCP_ERR_SOCKET_ERROR:
            snprintf(error_msg, sizeof(error_msg), "Socket error: %s", detail);
            break;
        case TCP_ERR_NO_FREE_STREAMS:
            snprintf(error_msg, sizeof(error_msg), "No free stream slots: %s", detail);
            break;
        case TCP_ERR_ALREADY_CLOSED:
            snprintf(error_msg, sizeof(error_msg), "Stream already closed: %s", detail);
            break;
        default:
            snprintf(error_msg, sizeof(error_msg), "Unknown error: %s", detail);
            break;
    }

    langerrormessage((unsigned char*)error_msg);
}

/* Internal function: Find free stream slot
 * Must be called with TCP_LOCK() held */
static int tcp_alloc_stream_id(void) {
    for (int i = 1; i < TCP_MAX_STREAMS; i++) {  /* Start at 1 (0 is invalid) */
        if (g_tcp_context.streams[i].sockfd == -1) {
            /* Initialize stream record */
            memset(&g_tcp_context.streams[i], 0, sizeof(tcp_stream_t));
            g_tcp_context.streams[i].sockfd = -1;  /* Will be set by caller */
            g_tcp_context.streams[i].state = STREAM_INVALID;
            g_tcp_context.streams[i].created_at = time(NULL);
            return i;
        }
    }
    return -1;  /* No free slots */
}

/* Internal function: Free stream slot
 * Must be called with TCP_LOCK() held */
static void tcp_free_stream_id(int stream_id) {
    if (stream_id < 1 || stream_id >= TCP_MAX_STREAMS)
        return;

    tcp_stream_t *stream = &g_tcp_context.streams[stream_id];

    /* Close socket if still open */
    if (stream->sockfd >= 0) {
        close(stream->sockfd);
        stream->sockfd = -1;
    }

    /* Mark slot as free */
    stream->state = STREAM_INVALID;
    g_tcp_context.active_count--;
}

/* Validate stream ID and return pointer to stream record
 * Must be called with TCP_LOCK() held */
static tcp_stream_t* tcp_get_stream(int stream_id) {
    if (stream_id < 1 || stream_id >= TCP_MAX_STREAMS)
        return NULL;

    tcp_stream_t *stream = &g_tcp_context.streams[stream_id];

    /* Reject invalid, closing, or closed streams to prevent race conditions */
    if (stream->state == STREAM_INVALID ||
        stream->state == STREAM_CLOSING ||
        stream->sockfd < 0)
        return NULL;

    return stream;
}

/* ========================================================================
 * Phase 1B: Address Operations (No Network I/O)
 * ======================================================================== */

/* tcp.addressEncode(ipString) -> addr
 * Convert dotted decimal IP string to 32-bit long (host byte order) */
boolean tcp_address_encode(bigstring ip_string, long *addr_out) {
    char ip_cstr[16];
    struct in_addr addr;

    /* Convert Pascal string to C string */
    if (stringlength(ip_string) > 15) {
        tcp_set_error(TCP_ERR_SOCKET_ERROR, "IP address too long");
        return false;
    }

    copyptocstring(ip_string, ip_cstr);

    /* Parse IP address */
    if (inet_pton(AF_INET, ip_cstr, &addr) != 1) {
        tcp_set_error(TCP_ERR_SOCKET_ERROR, "Invalid IP address format");
        return false;
    }

    /* Return in host byte order */
    *addr_out = (long)ntohl(addr.s_addr);
    return true;
}

/* tcp.addressDecode(addr) -> ipString
 * Convert 32-bit long (host byte order) to dotted decimal IP string */
boolean tcp_address_decode(long addr, bigstring ip_string_out) {
    struct in_addr in_addr;
    char ip_cstr[INET_ADDRSTRLEN];

    /* Convert to network byte order */
    in_addr.s_addr = htonl((uint32_t)addr);

    /* Convert to dotted decimal */
    if (inet_ntop(AF_INET, &in_addr, ip_cstr, sizeof(ip_cstr)) == NULL) {
        tcp_set_error(TCP_ERR_SOCKET_ERROR, "Address decode failed");
        return false;
    }

    copyctopstring(ip_cstr, ip_string_out);
    return true;
}

/* ========================================================================
 * Phase 1A: Core Socket Operations
 * ======================================================================== */

/* tcp.openAddrStream(addr, port) -> streamID
 * Open TCP connection to specified IP address (as long) and port */
boolean tcp_open_stream_addr(long addr, long port, long *stream_id_out) {
    struct sockaddr_in server_addr;
    int sockfd;
    int stream_id;

    log_debug(LOG_COMP_LANG, "tcp_open_stream_addr: addr=%ld port=%ld", addr, port);

    /* Validate parameters */
    if (addr <= 0) {
        tcp_set_error(TCP_ERR_CONNECTION_FAILED, "Invalid address");
        return false;
    }
    if (port <= 0 || port > 65535) {
        tcp_set_error(TCP_ERR_CONNECTION_FAILED, "Invalid port");
        return false;
    }

    /* Create socket */
    sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sockfd < 0) {
        tcp_set_error(tcp_map_errno(errno), "Could not create socket");
        return false;
    }

    /* Set socket options */
    int optval = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    /* Setup address structure */
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons((uint16_t)port);
    server_addr.sin_addr.s_addr = htonl((uint32_t)addr);  /* Convert to network byte order */

    /* Connect (blocking) */
    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        int saved_errno = errno;
        close(sockfd);
        tcp_set_error(tcp_map_errno(saved_errno), "Connection refused");
        return false;
    }

    log_debug(LOG_COMP_LANG, "tcp_open_stream_addr: connected, allocating stream ID");

    /* Allocate stream ID */
    TCP_LOCK();
    stream_id = tcp_alloc_stream_id();
    if (stream_id < 0) {
        TCP_UNLOCK();
        close(sockfd);
        tcp_set_error(TCP_ERR_NO_FREE_STREAMS, "Too many connections");
        return false;
    }

    /* Initialize stream record */
    tcp_stream_t *stream = &g_tcp_context.streams[stream_id];
    stream->sockfd = sockfd;
    stream->state = STREAM_CONNECTED;
    stream->remote_addr = (uint32_t)addr;
    stream->remote_port = (uint16_t)port;
    stream->last_activity = time(NULL);

    g_tcp_context.active_count++;
    g_tcp_context.total_connections++;
    TCP_UNLOCK();

    log_info(LOG_COMP_LANG, "tcp_open_stream_addr: stream_id=%d allocated", stream_id);

    *stream_id_out = stream_id;
    return true;
}

/* tcp.readStream(stream, bytesToRead) -> data
 * Non-blocking read from stream. Returns empty string if no data available. */
boolean tcp_read_stream(long stream_id, long bytes_to_read, Handle *data_out) {
    tcp_stream_t *stream;
    Handle hdata;
    ssize_t bytes_read;
    char *buffer;
    int sockfd;

    log_debug(LOG_COMP_LANG, "tcp_read_stream: stream_id=%ld bytes=%ld", stream_id, bytes_to_read);

    /* Validate parameters */
    if (bytes_to_read <= 0) {
        *data_out = nil;
        tcp_set_error(TCP_ERR_SOCKET_ERROR, "Invalid byte count");
        return false;
    }

    /* Get stream */
    TCP_LOCK();
    stream = tcp_get_stream(stream_id);
    if (!stream || stream->state != STREAM_CONNECTED) {
        TCP_UNLOCK();
        *data_out = nil;
        tcp_set_error(TCP_ERR_INVALID_STREAM, "Stream not connected");
        return false;
    }

    sockfd = stream->sockfd;
    TCP_UNLOCK();

    /* Allocate buffer */
    if (!newhandle(bytes_to_read, &hdata)) {
        *data_out = nil;
        tcp_set_error(TCP_ERR_NO_MEMORY, "Could not allocate buffer");
        return false;
    }

    lockhandle(hdata);
    buffer = *hdata;

    /* Set socket to non-blocking mode for this read */
    int flags = fcntl(sockfd, F_GETFL, 0);
    fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);

    /* Read from socket (may return less than requested or EAGAIN) */
    bytes_read = recv(sockfd, buffer, bytes_to_read, 0);

    /* Restore blocking mode */
    fcntl(sockfd, F_SETFL, flags);

    unlockhandle(hdata);

    if (bytes_read < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            /* No data available - return empty handle */
            disposehandle(hdata);
            if (!newhandle(0, data_out)) {
                tcp_set_error(TCP_ERR_NO_MEMORY, "Could not allocate empty buffer");
                return false;
            }
            log_debug(LOG_COMP_LANG, "tcp_read_stream: no data available (non-blocking)");
            return true;
        }

        /* Real error */
        disposehandle(hdata);
        *data_out = nil;  /* Prevent caller from accessing freed memory */
        tcp_set_error(TCP_ERR_SOCKET_ERROR, "Read failed");
        return false;
    }

    if (bytes_read == 0) {
        /* Connection closed by peer */
        disposehandle(hdata);
        if (!newhandle(0, data_out)) {
            tcp_set_error(TCP_ERR_NO_MEMORY, "Could not allocate empty buffer");
            return false;
        }
        log_debug(LOG_COMP_LANG, "tcp_read_stream: connection closed by peer");
        return true;
    }

    /* Resize handle to actual bytes read */
    sethandlesize(hdata, bytes_read);

    /* Update activity timestamp */
    TCP_LOCK();
    stream = tcp_get_stream(stream_id);
    if (stream)
        stream->last_activity = time(NULL);
    TCP_UNLOCK();

    log_debug(LOG_COMP_LANG, "tcp_read_stream: read %zd bytes", bytes_read);

    *data_out = hdata;
    return true;
}

/* tcp.writeStream(stream, data) -> true
 * Blocking write to stream. Waits until all data is sent. */
boolean tcp_write_stream(long stream_id, Handle hdata) {
    tcp_stream_t *stream;
    ssize_t bytes_written;
    long data_size;
    char *buffer;
    int sockfd;

    log_debug(LOG_COMP_LANG, "tcp_write_stream: stream_id=%ld", stream_id);

    /* Get stream */
    TCP_LOCK();
    stream = tcp_get_stream(stream_id);
    if (!stream || stream->state != STREAM_CONNECTED) {
        TCP_UNLOCK();
        tcp_set_error(TCP_ERR_INVALID_STREAM, "Stream not connected");
        return false;
    }

    sockfd = stream->sockfd;
    TCP_UNLOCK();

    data_size = gethandlesize(hdata);
    if (data_size == 0) {
        /* Nothing to write - succeed immediately */
        return true;
    }

    lockhandle(hdata);
    buffer = *hdata;

    /* Write all data (loop until complete) */
    long total_written = 0;
    while (total_written < data_size) {
        bytes_written = send(sockfd, buffer + total_written,
                            data_size - total_written, 0);

        if (bytes_written < 0) {
            unlockhandle(hdata);
            tcp_set_error(TCP_ERR_SOCKET_ERROR, "Write failed");
            return false;
        }

        total_written += bytes_written;
    }

    unlockhandle(hdata);

    /* Update activity timestamp */
    TCP_LOCK();
    stream = tcp_get_stream(stream_id);
    if (stream)
        stream->last_activity = time(NULL);
    TCP_UNLOCK();

    log_debug(LOG_COMP_LANG, "tcp_write_stream: wrote %ld bytes", total_written);

    return true;
}

/* tcp.closeStream(stream) -> true
 * Graceful close (sends FIN, waits for peer to acknowledge) */
boolean tcp_close_stream(long stream_id) {
    tcp_stream_t *stream;
    int sockfd;

    log_debug(LOG_COMP_LANG, "tcp_close_stream: stream_id=%ld", stream_id);

    TCP_LOCK();
    stream = tcp_get_stream(stream_id);
    if (!stream) {
        TCP_UNLOCK();
        tcp_set_error(TCP_ERR_INVALID_STREAM, "Invalid stream");
        return false;
    }

    sockfd = stream->sockfd;
    stream->state = STREAM_CLOSING;
    TCP_UNLOCK();

    /* Graceful shutdown */
    shutdown(sockfd, SHUT_RDWR);  /* Send FIN */
    close(sockfd);

    /* Free stream record */
    TCP_LOCK();
    tcp_free_stream_id(stream_id);
    TCP_UNLOCK();

    log_info(LOG_COMP_LANG, "tcp_close_stream: stream_id=%ld closed", stream_id);

    return true;
}

/* tcp.abortStream(stream) -> true
 * Immediate close (sends RST, no graceful shutdown) */
boolean tcp_abort_stream(long stream_id) {
    tcp_stream_t *stream;
    int sockfd;

    log_debug(LOG_COMP_LANG, "tcp_abort_stream: stream_id=%ld", stream_id);

    TCP_LOCK();
    stream = tcp_get_stream(stream_id);
    if (!stream) {
        TCP_UNLOCK();
        tcp_set_error(TCP_ERR_INVALID_STREAM, "Invalid stream");
        return false;
    }

    sockfd = stream->sockfd;

    /* Set SO_LINGER to 0 for immediate RST */
    struct linger linger_opt = {1, 0};  /* on, timeout=0 */
    setsockopt(sockfd, SOL_SOCKET, SO_LINGER, &linger_opt, sizeof(linger_opt));

    close(sockfd);  /* Sends RST */

    tcp_free_stream_id(stream_id);
    TCP_UNLOCK();

    log_info(LOG_COMP_LANG, "tcp_abort_stream: stream_id=%ld aborted", stream_id);

    return true;
}

/* tcp.countConnections() -> count
 * Return number of active streams */
long tcp_count_connections(void) {
    long count;

    TCP_LOCK();
    count = g_tcp_context.active_count;
    TCP_UNLOCK();

    log_debug(LOG_COMP_LANG, "tcp_count_connections: count=%ld", count);

    return count;
}

/* ========================================================================
 * Phase 1B: DNS Operations (Blocking)
 * ======================================================================== */

/* tcp.nameToAddress(domainName) -> addr
 * DNS lookup: hostname -> IP address (blocking) */
boolean tcp_name_to_address(bigstring domain_name, long *addr_out) {
    struct addrinfo hints, *result;
    char hostname_cstr[256];

    log_debug(LOG_COMP_LANG, "tcp_name_to_address: looking up hostname");

    /* Convert Pascal string to C string */
    if (stringlength(domain_name) > 255) {
        tcp_set_error(TCP_ERR_DNS_FAILED, "Hostname too long");
        return false;
    }

    copyptocstring(domain_name, hostname_cstr);

    /* Setup hints for getaddrinfo */
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;        /* IPv4 only for Phase 1 */
    hints.ai_socktype = SOCK_STREAM;

    /* DNS resolution (blocking) */
    if (getaddrinfo(hostname_cstr, NULL, &hints, &result) != 0) {
        tcp_set_error(TCP_ERR_DNS_FAILED, "Could not resolve hostname");
        return false;
    }

    /* Get first IPv4 address */
    struct sockaddr_in *addr = (struct sockaddr_in*)result->ai_addr;
    *addr_out = (long)ntohl(addr->sin_addr.s_addr);  /* Convert to host byte order */

    freeaddrinfo(result);

    log_info(LOG_COMP_LANG, "tcp_name_to_address: resolved to %ld", *addr_out);

    return true;
}

/* tcp.addressToName(addr) -> domainName
 * Reverse DNS: IP address -> hostname (blocking) */
boolean tcp_address_to_name(long addr, bigstring name_out) {
    struct sockaddr_in sa;
    char hostname[NI_MAXHOST];

    log_debug(LOG_COMP_LANG, "tcp_address_to_name: addr=%ld", addr);

    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = htonl((uint32_t)addr);

    /* Reverse lookup (blocking) */
    if (getnameinfo((struct sockaddr*)&sa, sizeof(sa),
                    hostname, sizeof(hostname),
                    NULL, 0, 0) != 0) {
        /* Reverse lookup failed, return IP as string */
        tcp_address_decode(addr, name_out);
        return true;
    }

    copyctopstring(hostname, name_out);

    log_info(LOG_COMP_LANG, "tcp_address_to_name: resolved to hostname");

    return true;
}

/* tcp.openNameStream(hostname, port) -> streamID
 * DNS lookup + connect (blocking for both operations) */
boolean tcp_open_stream_name(bigstring hostname, long port, long *stream_id_out) {
    struct addrinfo hints, *result, *rp;
    int sockfd = -1;
    int stream_id = -1;
    char hostname_cstr[256];
    char port_str[16];

    log_debug(LOG_COMP_LANG, "tcp_open_stream_name: hostname port=%ld", port);

    /* Validate port */
    if (port <= 0 || port > 65535) {
        tcp_set_error(TCP_ERR_CONNECTION_FAILED, "Invalid port");
        return false;
    }

    /* Convert Pascal string to C string */
    if (stringlength(hostname) > 255) {
        tcp_set_error(TCP_ERR_DNS_FAILED, "Hostname too long");
        return false;
    }

    copyptocstring(hostname, hostname_cstr);
    snprintf(port_str, sizeof(port_str), "%ld", port);

    /* Setup hints for getaddrinfo */
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;        /* IPv4 only for Phase 1 */
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    /* DNS resolution (blocking) */
    if (getaddrinfo(hostname_cstr, port_str, &hints, &result) != 0) {
        tcp_set_error(TCP_ERR_DNS_FAILED, "Could not resolve hostname");
        return false;
    }

    /* Try each address until we successfully connect */
    for (rp = result; rp != NULL; rp = rp->ai_next) {
        sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sockfd == -1)
            continue;

        /* Set socket options */
        int optval = 1;
        setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

        /* Attempt connection (blocking) */
        if (connect(sockfd, rp->ai_addr, rp->ai_addrlen) == 0) {
            /* Success! */
            break;
        }

        close(sockfd);
        sockfd = -1;
    }

    freeaddrinfo(result);

    if (sockfd == -1) {
        /* All connection attempts failed */
        tcp_set_error(TCP_ERR_CONNECTION_FAILED, "Could not connect to any address");
        return false;
    }

    /* Allocate stream ID */
    TCP_LOCK();
    stream_id = tcp_alloc_stream_id();
    if (stream_id < 0) {
        TCP_UNLOCK();
        close(sockfd);
        tcp_set_error(TCP_ERR_NO_FREE_STREAMS, "Too many connections");
        return false;
    }

    /* Initialize stream record */
    tcp_stream_t *stream = &g_tcp_context.streams[stream_id];
    stream->sockfd = sockfd;
    stream->state = STREAM_CONNECTED;
    stream->remote_port = (uint16_t)port;
    stream->last_activity = time(NULL);

    /* Extract remote_addr from connected socket */
    struct sockaddr_in peer_addr;
    socklen_t addr_len = sizeof(peer_addr);
    if (getpeername(sockfd, (struct sockaddr*)&peer_addr, &addr_len) == 0) {
        stream->remote_addr = ntohl(peer_addr.sin_addr.s_addr);
    } else {
        log_warn(LOG_COMP_LANG, "tcp_open_stream_name: getpeername() failed, remote_addr unavailable");
        stream->remote_addr = 0;  /* Mark as unavailable */
    }

    g_tcp_context.active_count++;
    g_tcp_context.total_connections++;
    TCP_UNLOCK();

    log_info(LOG_COMP_LANG, "tcp_open_stream_name: stream_id=%d allocated", stream_id);

    *stream_id_out = stream_id;
    return true;
}

/* ========================================================================
 * Initialization
 * ======================================================================== */

/* tcp_init_context() - Initialize TCP subsystem
 * Called from tcpinitverbs() during Frontier CLI startup */
boolean tcp_init_context(void) {
    log_info(LOG_COMP_LANG, "Initializing TCP context");

    /* Initialize TCP context */
    memset(&g_tcp_context, 0, sizeof(g_tcp_context));

    for (int i = 0; i < TCP_MAX_STREAMS; i++) {
        g_tcp_context.streams[i].sockfd = -1;
        g_tcp_context.streams[i].state = STREAM_INVALID;
    }

    pthread_mutex_init(&g_tcp_context.mutex, NULL);
    pthread_cond_init(&g_tcp_context.activity_cond, NULL);

    g_tcp_context.default_timeout_sec = 30;
    g_tcp_context.next_listen_id = 1;
    g_tcp_context.initialized = true;

    log_info(LOG_COMP_LANG, "TCP context initialized successfully");

    return true;
}
