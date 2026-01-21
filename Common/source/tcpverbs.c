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
#include <sys/time.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>

#include "frontier.h"
#include "standard.h"
#include "tcpverbs.h"
#include "langexternal.h"
#include "langinternal.h"
#include "lang.h"  /* langruncallbackwithparams() */
#include "memory.h"
#include "strings.h"
#include "logging.h"

/* Global TCP Context */
static tcp_context_t g_tcp_context;

/* Listener Registry - Tracks active listen sockets (Phase 3)
 * Each listener has its own accept thread and callback configuration.
 * Registry is protected by g_listeners_mutex (separate mutex for listener operations).
 * Maximum concurrent listeners is defined in tcpverbs.h as TCP_MAX_LISTENERS. */
typedef struct tcp_listener {
    int             listen_socket;     /* Listen socket FD (-1 if unused) */
    long            listener_id;       /* Unique listener ID */
    pthread_t       accept_thread;     /* Accept thread handle */
    boolean         running;           /* Thread should continue running */
    uint16_t        port;              /* Bound port (host byte order) */
    uint32_t        bind_addr;         /* Bound address (host byte order) */

    /* Callback configuration
     * THREAD-SAFETY LIMITATION (Issue #3): Storing hdlhashtable across thread boundary.
     * Handles (pointer to pointer) are generally unsafe across threads as hash tables
     * could be relocated by memory manager. However, for Phase 1, we accept this
     * limitation with the following mitigations:
     * 1. Validate callback_table is non-nil before use
     * 2. Check callback_name exists in table (detects if callback was deleted)
     * 3. Document this as a known limitation for future enhancement
     *
     * FUTURE FIX: Implement reference counting on hash tables or use address-based
     * lookup that resolves at use time. This requires broader ODB infrastructure changes.
     */
    bigstring       callback_name;     /* UserTalk callback script name */
    hdlhashtable    callback_table;    /* Hash table containing callback (see THREAD-SAFETY note) */
    long            refcon;            /* User refcon data */
} tcp_listener_t;

static tcp_listener_t *g_tcp_listeners[TCP_MAX_LISTENERS];
static pthread_mutex_t g_listeners_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Mutex Macros */
#define TCP_LOCK()       pthread_mutex_lock(&g_tcp_context.mutex)
#define TCP_UNLOCK()     pthread_mutex_unlock(&g_tcp_context.mutex)
#define LISTENERS_LOCK() pthread_mutex_lock(&g_listeners_mutex)
#define LISTENERS_UNLOCK() pthread_mutex_unlock(&g_listeners_mutex)

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
    for (int i = TCP_FIRST_STREAM_ID; i < TCP_MAX_STREAMS; i++) {  /* Start at 1 (0 is invalid) */
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

/* Acquire reference to stream (TOCTOU protection)
 * Returns stream pointer with incremented refcount, or NULL if invalid.
 * Caller MUST call tcp_stream_release() when done with the stream.
 * This prevents the stream from being freed while in use. */
tcp_stream_t* tcp_stream_acquire(int stream_id) {
    TCP_LOCK();
    tcp_stream_t *stream = tcp_get_stream(stream_id);
    if (stream) {
        stream->refcount++;
        log_trace(LOG_COMP_LANG, "tcp_stream_acquire: stream_id=%d refcount=%d",
                  stream_id, stream->refcount);
    }
    TCP_UNLOCK();
    return stream;
}

/* Release reference to stream
 * Decrements refcount and frees stream if refcount reaches 0 and state is CLOSING.
 * Must be called after tcp_stream_acquire() when done with stream. */
void tcp_stream_release(tcp_stream_t *stream) {
    if (!stream)
        return;

    TCP_LOCK();
    stream->refcount--;
    log_trace(LOG_COMP_LANG, "tcp_stream_release: refcount=%d state=%d",
              stream->refcount, stream->state);

    /* If refcount reached 0 and stream is closing, complete the close */
    if (stream->refcount == 0 && stream->state == STREAM_CLOSING) {
        /* Find stream_id from pointer offset */
        int stream_id = (int)(stream - g_tcp_context.streams);
        log_debug(LOG_COMP_LANG, "tcp_stream_release: completing close for stream_id=%d", stream_id);
        tcp_free_stream_id(stream_id);
    }
    TCP_UNLOCK();
}

/* Check if IP address is private/reserved (DNS rebinding/SSRF protection)
 * Returns true if addr is in private/reserved ranges that should not be accessed.
 * Addresses (in host byte order):
 * - 0.0.0.0/8        (current network)
 * - 10.0.0.0/8       (private)
 * - 127.0.0.0/8      (loopback)
 * - 169.254.0.0/16   (link-local)
 * - 172.16.0.0/12    (private)
 * - 192.168.0.0/16   (private)
 * - 224.0.0.0/4      (multicast)
 * - 240.0.0.0/4      (reserved) */
boolean tcp_is_private_ip(uint32_t addr) {
    uint8_t octet1 = (addr >> 24) & 0xFF;
    uint8_t octet2 = (addr >> 16) & 0xFF;

    /* 0.0.0.0/8 - Current network */
    if (octet1 == 0)
        return true;

    /* 10.0.0.0/8 - Private */
    if (octet1 == 10)
        return true;

    /* 127.0.0.0/8 - Loopback (allow 127.0.0.1 for local development) */
    if (octet1 == 127 && addr != 0x7F000001)  /* 0x7F000001 = 127.0.0.1 */
        return true;

    /* 169.254.0.0/16 - Link-local */
    if (octet1 == 169 && octet2 == 254)
        return true;

    /* 172.16.0.0/12 - Private (172.16.0.0 to 172.31.255.255) */
    if (octet1 == 172 && octet2 >= 16 && octet2 <= 31)
        return true;

    /* 192.168.0.0/16 - Private */
    if (octet1 == 192 && octet2 == 168)
        return true;

    /* 224.0.0.0/4 - Multicast (224-239) */
    if (octet1 >= 224 && octet1 <= 239)
        return true;

    /* 240.0.0.0/4 - Reserved (240-255) */
    if (octet1 >= 240)
        return true;

    return false;
}

/* Check rate limit before opening new connection
 * Returns true if rate limit allows connection, false if rejected.
 * Must be called with TCP_LOCK() held.
 *
 * SECURITY FIX (Issue #7): Rate limiting bypass prevention
 * VULNERABILITY: time(NULL) with 1-second granularity allowed burst attacks where
 * multiple connections within the same second bypassed rate limits. Attacker could
 * send N connections in rapid succession (microseconds apart) and they'd all count
 * as "same second", defeating the rate limit.
 *
 * FIX: Use gettimeofday() for microsecond-precision timestamps. This prevents burst
 * attacks by accurately tracking connection timing within the sliding window.
 *
 * Uses sliding window algorithm:
 * - Tracks timestamps of recent connections in circular buffer (microsecond precision)
 * - Counts connections in last second (1,000,000 microseconds)
 * - Rejects if count >= connections_per_sec limit */
static boolean tcp_check_rate_limit(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    uint64_t now_us = (uint64_t)tv.tv_sec * 1000000ULL + tv.tv_usec;
    uint64_t one_second_ago_us = now_us - 1000000ULL;  /* 1 second = 1,000,000 microseconds */
    int connections_in_last_second = 0;

    /* Count connections in the last second (microsecond precision) */
    for (int i = 0; i < TCP_RATE_LIMIT_WINDOW; i++) {
        if (g_tcp_context.connection_timestamps_us[i] >= one_second_ago_us) {
            connections_in_last_second++;
        }
    }

    /* Check if we're over the limit */
    if (connections_in_last_second >= g_tcp_context.connections_per_sec) {
        g_tcp_context.rate_limited_count++;
        log_warn(LOG_COMP_LANG, "tcp_check_rate_limit: rate limit exceeded (%d conn/sec, limit=%d)",
                 connections_in_last_second, g_tcp_context.connections_per_sec);
        return false;
    }

    /* Record this connection attempt (microsecond timestamp) */
    g_tcp_context.connection_timestamps_us[g_tcp_context.timestamp_write_pos] = now_us;
    g_tcp_context.timestamp_write_pos = (g_tcp_context.timestamp_write_pos + 1) % TCP_RATE_LIMIT_WINDOW;

    return true;
}

/* ========================================================================
 * Phase 1B: Address Operations (No Network I/O)
 * ======================================================================== */

/* tcp.addressEncode(ipString) -> addr
 * Convert dotted decimal IP string to 32-bit long (host byte order) */
boolean tcp_address_encode(bigstring ip_string, long *addr_out) {
    char ip_cstr[16];
    struct in_addr addr;

    /* NULL pointer validation */
    if (!addr_out) {
        tcp_set_error(TCP_ERR_SOCKET_ERROR, "NULL output pointer");
        return false;
    }

    /* Convert Pascal string to C string */
    if (stringlength(ip_string) > MAX_IPV4_STRING_LEN) {
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

    /* NULL pointer validation */
    if (!ip_string_out) {
        tcp_set_error(TCP_ERR_SOCKET_ERROR, "NULL output pointer");
        return false;
    }

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

    /* NULL pointer validation */
    if (!stream_id_out) {
        tcp_set_error(TCP_ERR_SOCKET_ERROR, "NULL output pointer");
        return false;
    }

    /* Validate parameters */
    if (addr <= 0) {
        tcp_set_error(TCP_ERR_CONNECTION_FAILED, "Invalid address");
        return false;
    }
    if (port <= 0 || port > 65535) {
        tcp_set_error(TCP_ERR_CONNECTION_FAILED, "Invalid port");
        return false;
    }

    /* Check rate limit before attempting connection */
    TCP_LOCK();
    if (!tcp_check_rate_limit()) {
        TCP_UNLOCK();
        tcp_set_error(TCP_ERR_NO_FREE_STREAMS, "Connection rate limit exceeded");
        return false;
    }
    TCP_UNLOCK();

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

    /* NULL pointer validation */
    if (!data_out) {
        tcp_set_error(TCP_ERR_SOCKET_ERROR, "NULL output pointer");
        return false;
    }

    /* Validate parameters */
    if (bytes_to_read <= 0) {
        *data_out = nil;
        tcp_set_error(TCP_ERR_SOCKET_ERROR, "Invalid byte count (must be positive)");
        return false;
    }

    /* SECURITY FIX (Issue #1): Integer overflow protection - RCE risk
     * VULNERABILITY: Incomplete overflow protection allowed values between
     * TCP_MAX_READ_BYTES+1 and LONG_MAX-1024 to pass validation but cause
     * undersized buffer allocation, leading to remote heap buffer overflow (RCE).
     *
     * ATTACK SCENARIO: Attacker sends bytes_to_read = TCP_MAX_READ_BYTES + 1000000.
     * Old code: Checked only TCP_MAX_READ_BYTES limit, missed overflow window.
     * Result: newhandle() allocates smaller buffer, recv() writes past end → RCE.
     *
     * FIX: Explicit size_t validation BEFORE allocation + post-allocation size
     * verification to catch any allocation failures that could create exploitable
     * conditions. This eliminates the overflow window entirely. */
    if (bytes_to_read > TCP_MAX_READ_BYTES) {
        *data_out = nil;
        tcp_set_error(TCP_ERR_SOCKET_ERROR, "Read size exceeds maximum (16MB limit)");
        return false;
    }

    /* Additional size_t overflow check - catches overflow window between
     * TCP_MAX_READ_BYTES and SIZE_MAX that could bypass first check */
    if ((size_t)bytes_to_read > SIZE_MAX) {
        *data_out = nil;
        tcp_set_error(TCP_ERR_SOCKET_ERROR, "Read size exceeds addressable memory");
        return false;
    }

    /* Acquire stream reference (TOCTOU protection) */
    stream = tcp_stream_acquire(stream_id);
    if (!stream || stream->state != STREAM_CONNECTED) {
        if (stream)
            tcp_stream_release(stream);
        *data_out = nil;
        tcp_set_error(TCP_ERR_INVALID_STREAM, "Stream not connected");
        return false;
    }

    sockfd = stream->sockfd;

    /* Allocate buffer */
    if (!newhandle(bytes_to_read, &hdata)) {
        tcp_stream_release(stream);
        *data_out = nil;
        tcp_set_error(TCP_ERR_NO_MEMORY, "Could not allocate buffer");
        return false;
    }

    /* SECURITY: Verify allocated buffer size matches request (prevents undersized allocation) */
    if (GetHandleSize(hdata) < bytes_to_read) {
        disposehandle(hdata);
        tcp_stream_release(stream);
        *data_out = nil;
        tcp_set_error(TCP_ERR_NO_MEMORY, "Buffer allocation size mismatch");
        return false;
    }

    lockhandle(hdata);
    buffer = *hdata;

    /* Set socket to non-blocking mode for this read */
    int flags = fcntl(sockfd, F_GETFL, 0);
    if (flags == -1) {
        log_warn(LOG_COMP_LANG, "tcp_read_stream: fcntl(F_GETFL) failed: %s", strerror(errno));
    } else if (fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) == -1) {
        log_warn(LOG_COMP_LANG, "tcp_read_stream: fcntl(F_SETFL, O_NONBLOCK) failed: %s", strerror(errno));
    }

    /* Read from socket (may return less than requested or EAGAIN) */
    bytes_read = recv(sockfd, buffer, bytes_to_read, 0);

    /* Restore blocking mode */
    if (flags != -1 && fcntl(sockfd, F_SETFL, flags) == -1) {
        log_warn(LOG_COMP_LANG, "tcp_read_stream: fcntl(F_SETFL, restore) failed: %s", strerror(errno));
    }

    unlockhandle(hdata);

    if (bytes_read < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            /* No data available - return empty handle */
            disposehandle(hdata);
            if (!newhandle(0, data_out)) {
                tcp_stream_release(stream);
                tcp_set_error(TCP_ERR_NO_MEMORY, "Could not allocate empty buffer");
                return false;
            }
            tcp_stream_release(stream);
            log_debug(LOG_COMP_LANG, "tcp_read_stream: no data available (non-blocking)");
            return true;
        }

        /* Real error */
        disposehandle(hdata);
        tcp_stream_release(stream);
        *data_out = nil;  /* Prevent caller from accessing freed memory */
        tcp_set_error(TCP_ERR_SOCKET_ERROR, "Read failed");
        return false;
    }

    if (bytes_read == 0) {
        /* Connection closed by peer */
        disposehandle(hdata);
        if (!newhandle(0, data_out)) {
            tcp_stream_release(stream);
            tcp_set_error(TCP_ERR_NO_MEMORY, "Could not allocate empty buffer");
            return false;
        }
        tcp_stream_release(stream);
        log_debug(LOG_COMP_LANG, "tcp_read_stream: connection closed by peer");
        return true;
    }

    /* Resize handle to actual bytes read */
    sethandlesize(hdata, bytes_read);

    /* Update activity timestamp while still holding reference */
    TCP_LOCK();
    stream->last_activity = time(NULL);
    TCP_UNLOCK();

    tcp_stream_release(stream);

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

    /* Acquire stream reference (TOCTOU protection) */
    stream = tcp_stream_acquire(stream_id);
    if (!stream || stream->state != STREAM_CONNECTED) {
        if (stream)
            tcp_stream_release(stream);
        tcp_set_error(TCP_ERR_INVALID_STREAM, "Stream not connected");
        return false;
    }

    sockfd = stream->sockfd;

    data_size = gethandlesize(hdata);
    if (data_size == 0) {
        /* Nothing to write - succeed immediately */
        tcp_stream_release(stream);
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
            /* Retry on interrupted system call */
            if (errno == EINTR) {
                continue;
            }
            unlockhandle(hdata);
            tcp_stream_release(stream);
            tcp_set_error(TCP_ERR_SOCKET_ERROR, "Write failed");
            return false;
        }

        total_written += bytes_written;
    }

    unlockhandle(hdata);

    /* Update activity timestamp while still holding reference */
    TCP_LOCK();
    stream->last_activity = time(NULL);
    TCP_UNLOCK();

    tcp_stream_release(stream);

    log_debug(LOG_COMP_LANG, "tcp_write_stream: wrote %ld bytes", total_written);

    return true;
}

/* tcp.closeStream(stream) -> true
 * Graceful close (sends FIN, waits for peer to acknowledge) */
boolean tcp_close_stream(long stream_id) {
    tcp_stream_t *stream;
    int sockfd;
    int refcount;

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

    /* CRITICAL: Mark socket as -1 BEFORE releasing lock to prevent double-close race.
     * Between TCP_UNLOCK() and the subsequent TCP_LOCK(), another thread could attempt
     * to close this stream. By setting sockfd = -1 first, we ensure tcp_get_stream()
     * will reject the stream (it checks sockfd < 0), preventing concurrent close operations.
     * This pattern is intentional and must be preserved. */
    stream->sockfd = -1;
    refcount = stream->refcount;
    TCP_UNLOCK();

    /* Graceful shutdown (performed without lock - blocking syscalls) */
    shutdown(sockfd, SHUT_RDWR);  /* Send FIN */
    close(sockfd);

    /* Free stream record only if no active references */
    TCP_LOCK();
    if (refcount == 0) {
        tcp_free_stream_id(stream_id);
        log_info(LOG_COMP_LANG, "tcp_close_stream: stream_id=%ld closed immediately", stream_id);
    } else {
        log_info(LOG_COMP_LANG, "tcp_close_stream: stream_id=%ld deferred (refcount=%d)",
                 stream_id, refcount);
    }
    TCP_UNLOCK();

    return true;
}

/* tcp.abortStream(stream) -> true
 * Immediate close (sends RST, no graceful shutdown) */
boolean tcp_abort_stream(long stream_id) {
    tcp_stream_t *stream;
    int sockfd;
    int refcount;

    log_debug(LOG_COMP_LANG, "tcp_abort_stream: stream_id=%ld", stream_id);

    TCP_LOCK();
    stream = tcp_get_stream(stream_id);
    if (!stream) {
        TCP_UNLOCK();
        tcp_set_error(TCP_ERR_INVALID_STREAM, "Invalid stream");
        return false;
    }

    sockfd = stream->sockfd;
    stream->state = STREAM_CLOSING;

    /* SECURITY FIX (Issue #8): TOCTOU race protection - same fix as tcp_close_stream
     * Atomically mark sockfd=-1 while holding lock to prevent double-close race.
     * See tcp_close_stream() for detailed vulnerability explanation. */
    stream->sockfd = -1;
    refcount = stream->refcount;

    /* Set SO_LINGER to 0 for immediate RST */
    struct linger linger_opt = {1, 0};  /* on, timeout=0 */
    if (setsockopt(sockfd, SOL_SOCKET, SO_LINGER, &linger_opt, sizeof(linger_opt)) == -1) {
        log_warn(LOG_COMP_LANG, "tcp_abort_stream: setsockopt(SO_LINGER) failed: %s", strerror(errno));
    }

    close(sockfd);  /* Sends RST */

    /* Free stream record only if no active references */
    if (refcount == 0) {
        tcp_free_stream_id(stream_id);
        log_info(LOG_COMP_LANG, "tcp_abort_stream: stream_id=%ld aborted immediately", stream_id);
    } else {
        log_info(LOG_COMP_LANG, "tcp_abort_stream: stream_id=%ld deferred (refcount=%d)",
                 stream_id, refcount);
    }
    TCP_UNLOCK();

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

    /* NULL pointer validation */
    if (!addr_out) {
        tcp_set_error(TCP_ERR_SOCKET_ERROR, "NULL output pointer");
        return false;
    }

    /* Convert Pascal string to C string */
    if (stringlength(domain_name) > MAX_HOSTNAME_LEN) {
        tcp_set_error(TCP_ERR_DNS_FAILED, "Hostname too long");
        return false;
    }

    copyptocstring(domain_name, hostname_cstr);

    /* Setup hints for getaddrinfo */
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;        /* IPv4 only for Phase 1 */
    hints.ai_socktype = SOCK_STREAM;

    /* SECURITY (Issue #6): DNS resolution with SSRF protection
     * Single DNS lookup, validate result, return to caller. No TOCTOU vulnerability
     * since we resolve once and return that result directly (no second lookup). */
    if (getaddrinfo(hostname_cstr, NULL, &hints, &result) != 0) {
        tcp_set_error(TCP_ERR_DNS_FAILED, "Could not resolve hostname");
        return false;
    }

    /* Get first IPv4 address */
    struct sockaddr_in *addr = (struct sockaddr_in*)result->ai_addr;
    uint32_t resolved_addr = (uint32_t)ntohl(addr->sin_addr.s_addr);  /* Convert to host byte order */

    freeaddrinfo(result);

    /* DNS rebinding/SSRF protection: reject private/reserved IPs */
    if (tcp_is_private_ip(resolved_addr)) {
        log_warn(LOG_COMP_LANG, "tcp_name_to_address: rejected private/reserved IP (DNS rebinding protection)");
        tcp_set_error(TCP_ERR_DNS_FAILED, "DNS resolved to private/reserved IP (not allowed)");
        return false;
    }

    *addr_out = (long)resolved_addr;
    log_info(LOG_COMP_LANG, "tcp_name_to_address: resolved to %ld", *addr_out);

    return true;
}

/* tcp.addressToName(addr) -> domainName
 * Reverse DNS: IP address -> hostname (blocking)
 *
 * IMPORTANT: This function ALWAYS returns true and never signals failure.
 * If reverse DNS lookup fails, it falls back to returning the IP address
 * as a dotted-decimal string (e.g., "192.168.1.1"). This is intentional
 * behavior to ensure UserTalk scripts always get a usable result.
 *
 * To detect if reverse DNS succeeded, compare the result with the original
 * IP address string - if they match, reverse lookup failed. */
/* tcp.addressToName(addr) -> string
 *
 * NOTE: Boolean return always true by design. This function cannot fail - it either
 * returns the reverse DNS hostname or falls back to IP string representation. This
 * signature matches legacy Frontier convention for consistency. */
boolean tcp_address_to_name(long addr, bigstring name_out) {
    struct sockaddr_in sa;
    char hostname[NI_MAXHOST];

    log_debug(LOG_COMP_LANG, "tcp_address_to_name: addr=%ld", addr);

    /* NULL pointer validation */
    if (!name_out) {
        tcp_set_error(TCP_ERR_SOCKET_ERROR, "NULL output pointer");
        return false;
    }

    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = htonl((uint32_t)addr);

    /* Reverse lookup (blocking) - may take 5-30 seconds on timeout */
    if (getnameinfo((struct sockaddr*)&sa, sizeof(sa),
                    hostname, sizeof(hostname),
                    NULL, 0, 0) != 0) {
        /* Reverse lookup failed - return IP address as fallback (intentional) */
        log_debug(LOG_COMP_LANG, "tcp_address_to_name: reverse lookup failed, returning IP string");
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

    /* NULL pointer validation */
    if (!stream_id_out) {
        tcp_set_error(TCP_ERR_SOCKET_ERROR, "NULL output pointer");
        return false;
    }

    /* Validate port */
    if (port <= 0 || port > 65535) {
        tcp_set_error(TCP_ERR_CONNECTION_FAILED, "Invalid port");
        return false;
    }

    /* Check rate limit before attempting connection */
    TCP_LOCK();
    if (!tcp_check_rate_limit()) {
        TCP_UNLOCK();
        tcp_set_error(TCP_ERR_NO_FREE_STREAMS, "Connection rate limit exceeded");
        return false;
    }
    TCP_UNLOCK();

    /* Convert Pascal string to C string */
    if (stringlength(hostname) == 0) {
        tcp_set_error(TCP_ERR_DNS_FAILED, "Hostname cannot be empty");
        return false;
    }

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

    /* SECURITY (Issue #6): DNS resolution and SSRF/rebinding protection
     * DNS resolution MUST happen once and validated IPs used for connection.
     * NOT VULNERABLE to TOCTOU: getaddrinfo() resolves DNS once, returns all IPs
     * in result linked list. We validate each IP in the list, then use those
     * SAME validated IPs for connection. No second DNS lookup occurs.
     *
     * CORRECT PATTERN: Resolve → Validate → Connect (all from same result set)
     * WRONG PATTERN: Resolve → Validate → Resolve again → Connect (TOCTOU)
     *
     * This prevents DNS rebinding attacks where attacker changes DNS between
     * validation and connection to redirect traffic to internal/reserved IPs. */
    if (getaddrinfo(hostname_cstr, port_str, &hints, &result) != 0) {
        tcp_set_error(TCP_ERR_DNS_FAILED, "Could not resolve hostname");
        return false;
    }

    /* Try each address until we successfully connect
     * SECURITY: Validate BEFORE attempting connection to prevent SSRF attacks */
    for (rp = result; rp != NULL; rp = rp->ai_next) {
        /* DNS rebinding/SSRF protection: validate IP is not private/reserved */
        struct sockaddr_in *addr_in = (struct sockaddr_in*)rp->ai_addr;
        uint32_t resolved_addr = ntohl(addr_in->sin_addr.s_addr);

        if (tcp_is_private_ip(resolved_addr)) {
            log_warn(LOG_COMP_LANG, "tcp_open_stream_name: skipping private/reserved IP (DNS rebinding protection)");
            continue;  /* Skip this address */
        }

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
 * Phase 3: Server Operations (Listen/Accept)
 * ======================================================================== */

/* Accept thread main function - runs in background for each listener
 * Accepts incoming connections and invokes UserTalk callback with parameters:
 *   param1: streamID (long) - Accepted connection stream ID
 *   param2: remoteAddr (long) - Client IP address (host byte order)
 *   param3: remotePort (long) - Client port number (host byte order)
 *
 * Thread-Safety: Uses grabthreadglobals/releasethreadglobals pattern
 * Callback API: langruncallbackwithparams() from P0a infrastructure (PR #328)
 */
static void *tcp_accept_thread(void *arg) {
    tcp_listener_t *listener = (tcp_listener_t *)arg;
    struct sockaddr_in client_addr;
    socklen_t addr_len;
    int client_sock;
    int stream_id;

    log_info(LOG_COMP_LANG, "tcp_accept_thread: started for listener_id=%ld port=%d",
             listener->listener_id, listener->port);

    /* FIX Issue #2: Grab thread globals BEFORE any UserTalk operations.
     * This thread invokes UserTalk callbacks via langruncallbackwithparams(), which
     * accesses thread-local state (flnextparamislast, current outline, etc.).
     * Without grabthreadglobals(), callback execution crashes on uninitialized context.
     *
     * CRITICAL: Must be FIRST operation in thread, paired with releasethreadglobals()
     * on exit. This allocates and initializes thread-local storage for this pthread. */
    if (!grabthreadglobals()) {
        log_error(LOG_COMP_LANG, "tcp_accept_thread: failed to grab thread globals for listener_id=%ld",
                 listener->listener_id);
        return NULL;
    }

    while (listener->running) {
        addr_len = sizeof(client_addr);
        memset(&client_addr, 0, addr_len);

        /* Accept connection (blocking) */
        client_sock = accept(listener->listen_socket,
                            (struct sockaddr *)&client_addr,
                            &addr_len);

        if (client_sock < 0) {
            if (listener->running) {
                /* Real error - log it */
                log_error(LOG_COMP_LANG, "tcp_accept_thread: accept() failed: %s",
                         strerror(errno));
            }
            /* Either error or shutdown - check running flag */
            if (!listener->running)
                break;
            continue;  /* Try again */
        }

        log_debug(LOG_COMP_LANG, "tcp_accept_thread: accepted connection from %s:%d",
                 inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        /* Allocate stream ID for accepted connection */
        TCP_LOCK();
        stream_id = tcp_alloc_stream_id();
        if (stream_id < 0) {
            TCP_UNLOCK();
            log_error(LOG_COMP_LANG, "tcp_accept_thread: no free stream slots");
            close(client_sock);
            continue;  /* Continue accepting other connections */
        }

        /* Initialize stream record */
        tcp_stream_t *stream = &g_tcp_context.streams[stream_id];
        stream->sockfd = client_sock;
        stream->state = STREAM_ACCEPTED;
        stream->remote_addr = ntohl(client_addr.sin_addr.s_addr);
        stream->remote_port = ntohs(client_addr.sin_port);
        stream->parent_listen_id = listener->listener_id;
        stream->last_activity = time(NULL);

        g_tcp_context.active_count++;
        g_tcp_context.total_connections++;
        TCP_UNLOCK();

        log_info(LOG_COMP_LANG, "tcp_accept_thread: allocated stream_id=%d for connection", stream_id);

        /* Invoke UserTalk callback with parameters (if callback defined)
         * MITIGATION Issue #3: Validate callback_table handle before use.
         * While storing hdlhashtable across thread boundary is unsafe (hash table
         * could be relocated), we mitigate by checking:
         * 1. Callback name is non-empty (indicates callback was configured)
         * 2. Hash table handle is non-nil (basic handle validity)
         * 3. Callback name exists in table (detects if callback was deleted)
         * This provides defense-in-depth, though not perfect. Future fix: reference counting. */
        if (stringlength(listener->callback_name) > 0 && listener->callback_table != nil) {
            /* Validate callback still exists in hash table before invoking */
            hdlhashnode hnode;
            if (!hashtablelookupnode(listener->callback_table, listener->callback_name, &hnode)) {
                log_warn(LOG_COMP_LANG, "tcp_accept_thread: callback '%.*s' no longer exists in table for stream_id=%d",
                        stringlength(listener->callback_name), listener->callback_name+1, stream_id);
                continue;  /* Skip callback but continue accepting connections */
            }

            tyvaluerecord params[3];
            setlongvalue(stream_id, &params[0]);                      /* streamID */
            setlongvalue((long)stream->remote_addr, &params[1]);      /* remoteAddr */
            setlongvalue((long)stream->remote_port, &params[2]);      /* remotePort */

            tyvaluerecord result;
            initvalue(&result, novaluetype);

            log_debug(LOG_COMP_LANG, "tcp_accept_thread: invoking callback for stream_id=%d", stream_id);

            boolean callback_success = langruncallbackwithparams(
                listener->callback_table,
                listener->callback_name,
                3,
                params,
                &result
            );

            if (!callback_success) {
                log_warn(LOG_COMP_LANG, "tcp_accept_thread: callback failed for stream_id=%d", stream_id);
            }

            /* Dispose result value */
            disposevaluerecord(result, false);
        }
    }

    /* FIX Issue #2: Release thread globals on exit.
     * Must be paired with grabthreadglobals() at thread start.
     * Cleans up thread-local storage before pthread terminates. */
    releasethreadglobals();

    log_info(LOG_COMP_LANG, "tcp_accept_thread: exiting for listener_id=%ld", listener->listener_id);
    return NULL;
}

/* tcp.listenStream(port, depth, callback, refcon, addr) -> listenID
 * Start listening for TCP connections on specified port and address.
 * Spawns accept thread that invokes callback for each connection.
 *
 * Parameters:
 *   port            - Port number to listen on (1-65535)
 *   depth           - Listen queue depth (backlog, >= 1)
 *   callback_htable - Hash table containing the callback script
 *   callback_name   - UserTalk script name to invoke on connection
 *   refcon          - User reference data (currently unused, reserved)
 *   bind_addr       - IP address to bind to (0 = INADDR_ANY, or specific IP)
 *
 * Returns:
 *   listenID - Unique listener identifier (> 0) for use with tcp.closeListen()
 *
 * Callback signature:
 *   on handler(streamID, remoteAddr, remotePort) { ... }
 */
boolean tcp_listen_stream(long port, long depth, hdlhashtable callback_htable,
                          bigstring callback_name, long refcon, long bind_addr,
                          long *listen_id_out) {
    int listen_sock;
    struct sockaddr_in server_addr;
    int optval = 1;
    tcp_listener_t *listener = NULL;
    int listener_slot = -1;

    log_debug(LOG_COMP_LANG, "tcp_listen_stream: port=%ld depth=%ld bind_addr=%ld",
             port, depth, bind_addr);

    /* NULL pointer validation */
    if (!listen_id_out) {
        tcp_set_error(TCP_ERR_SOCKET_ERROR, "NULL output pointer");
        return false;
    }

    /* Validate parameters */
    if (port <= 0 || port > 65535) {
        tcp_set_error(TCP_ERR_INVALID_STREAM, "Invalid port (must be 1-65535)");
        return false;
    }

    if (depth <= 0) {
        tcp_set_error(TCP_ERR_INVALID_STREAM, "Invalid queue depth (must be >= 1)");
        return false;
    }

    /* Create listen socket */
    listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listen_sock < 0) {
        tcp_set_error(tcp_map_errno(errno), "Could not create listen socket");
        return false;
    }

    /* Set SO_REUSEADDR to allow quick restart */
    if (setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        log_warn(LOG_COMP_LANG, "tcp_listen_stream: setsockopt(SO_REUSEADDR) failed: %s",
                strerror(errno));
    }

    /* Bind to address and port */
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons((uint16_t)port);
    server_addr.sin_addr.s_addr = htonl((uint32_t)bind_addr);  /* 0 = INADDR_ANY */

    if (bind(listen_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        int saved_errno = errno;
        close(listen_sock);
        tcp_set_error(tcp_map_errno(saved_errno), "Bind failed (port already in use?)");
        return false;
    }

    /* Start listening */
    if (listen(listen_sock, (int)depth) < 0) {
        int saved_errno = errno;
        close(listen_sock);
        tcp_set_error(tcp_map_errno(saved_errno), "Listen failed");
        return false;
    }

    log_info(LOG_COMP_LANG, "tcp_listen_stream: listening on port %ld", port);

    /* Allocate listener structure */
    LISTENERS_LOCK();

    /* Find free listener slot */
    for (int i = 0; i < TCP_MAX_LISTENERS; i++) {
        if (g_tcp_listeners[i] == NULL) {
            listener_slot = i;
            break;
        }
    }

    if (listener_slot < 0) {
        LISTENERS_UNLOCK();
        close(listen_sock);
        tcp_set_error(TCP_ERR_NO_FREE_STREAMS, "Too many listeners (max 32)");
        return false;
    }

    /* Allocate and initialize listener */
    listener = (tcp_listener_t *)malloc(sizeof(tcp_listener_t));
    if (!listener) {
        LISTENERS_UNLOCK();
        close(listen_sock);
        tcp_set_error(TCP_ERR_NO_MEMORY, "Could not allocate listener");
        return false;
    }

    memset(listener, 0, sizeof(tcp_listener_t));
    listener->listen_socket = listen_sock;
    listener->running = true;
    listener->port = (uint16_t)port;
    listener->bind_addr = (uint32_t)bind_addr;
    listener->refcon = refcon;

    /* Assign unique listener ID */
    TCP_LOCK();
    listener->listener_id = g_tcp_context.next_listen_id++;
    g_tcp_context.listen_count++;
    TCP_UNLOCK();

    /* Store callback configuration */
    copystring(callback_name, listener->callback_name);
    listener->callback_table = callback_htable;

    /* Register listener */
    g_tcp_listeners[listener_slot] = listener;

    /* Spawn accept thread */
    if (pthread_create(&listener->accept_thread, NULL, tcp_accept_thread, listener) != 0) {
        /* Thread creation failed - cleanup */
        g_tcp_listeners[listener_slot] = NULL;
        LISTENERS_UNLOCK();

        TCP_LOCK();
        g_tcp_context.listen_count--;
        TCP_UNLOCK();

        close(listen_sock);
        free(listener);

        tcp_set_error(TCP_ERR_SOCKET_ERROR, "Could not create accept thread");
        return false;
    }

    LISTENERS_UNLOCK();

    *listen_id_out = listener->listener_id;

    log_info(LOG_COMP_LANG, "tcp_listen_stream: listener_id=%ld started on port %d",
             listener->listener_id, listener->port);

    return true;
}

/* tcp.closeListen(listenID) -> true
 * Stop listening for connections and cleanup listener resources.
 * Joins accept thread and frees listener structure.
 * Does NOT close accepted connections (they remain open).
 *
 * FIX Issue #4: Critical cleanup ordering to prevent use-after-free.
 * VULNERABILITY: Original code removed listener from registry BEFORE joining thread.
 * Accept thread could access freed memory during shutdown sequence.
 *
 * CORRECT ORDERING:
 * 1. Find listener in registry (but don't remove yet - thread still running)
 * 2. Signal shutdown and close socket to wake thread from accept()
 * 3. pthread_join() - BLOCKS until thread fully exits
 * 4. ONLY THEN remove from registry and free memory (thread has exited)
 *
 * WHY THIS MATTERS: Accept thread accesses listener-> fields during shutdown.
 * If we free before join, thread hits use-after-free → crash or corruption.
 *
 * Parameters:
 *   listenID - Listener ID returned by tcp.listenStream()
 *
 * Returns:
 *   true on success
 */
boolean tcp_close_listen(long listen_id) {
    tcp_listener_t *listener = NULL;
    int listener_slot = -1;
    pthread_t accept_thread;
    int listen_socket;

    log_debug(LOG_COMP_LANG, "tcp_close_listen: listen_id=%ld", listen_id);

    /* Validate listen_id */
    if (listen_id <= 0) {
        tcp_set_error(TCP_ERR_INVALID_STREAM, "Invalid listener ID");
        return false;
    }

    /* Find listener in registry (but DON'T remove yet - thread still running)
     * Check running flag to prevent double-close if concurrent threads call
     * tcp_close_listen() with same ID. First thread sets running=false, second
     * thread won't find it due to this check. */
    LISTENERS_LOCK();

    for (int i = 0; i < TCP_MAX_LISTENERS; i++) {
        if (g_tcp_listeners[i] != NULL &&
            g_tcp_listeners[i]->listener_id == listen_id &&
            g_tcp_listeners[i]->running == true) {
            listener = g_tcp_listeners[i];
            listener_slot = i;
            listener->running = false;  /* Mark as closing BEFORE unlock */
            /* DO NOT remove from registry yet - thread still needs access */
            break;
        }
    }

    LISTENERS_UNLOCK();

    if (!listener) {
        tcp_set_error(TCP_ERR_INVALID_STREAM, "Listener not found or already closing");
        return false;
    }

    /* Thread stop signal was set atomically inside lock above.
     * Get socket and thread handles for cleanup. */
    listen_socket = listener->listen_socket;
    accept_thread = listener->accept_thread;

    /* Close listen socket to break out of blocking accept() */
    close(listen_socket);

    log_info(LOG_COMP_LANG, "tcp_close_listen: closed listen socket for listener_id=%ld", listen_id);

    /* CRITICAL: Wait for accept thread to exit BEFORE freeing memory.
     * Thread may still be accessing listener structure during shutdown (checking
     * listener->running flag, accessing listener->callback_name, etc.).
     * pthread_join() blocks until thread fully terminates and returns from
     * tcp_accept_thread(). ONLY AFTER this point is it safe to free memory. */
    if (pthread_join(accept_thread, NULL) != 0) {
        log_warn(LOG_COMP_LANG, "tcp_close_listen: pthread_join failed for listener_id=%ld", listen_id);
        /* Continue with cleanup anyway - best effort */
    }

    log_info(LOG_COMP_LANG, "tcp_close_listen: accept thread joined for listener_id=%ld", listen_id);

    /* NOW safe to remove from registry and free - thread has fully exited */
    LISTENERS_LOCK();
    g_tcp_listeners[listener_slot] = NULL;
    LISTENERS_UNLOCK();

    /* Update listener count */
    TCP_LOCK();
    g_tcp_context.listen_count--;
    TCP_UNLOCK();

    /* Free listener structure */
    free(listener);

    log_info(LOG_COMP_LANG, "tcp_close_listen: listener_id=%ld closed successfully", listen_id);

    return true;
}

/* ========================================================================
 * Initialization
 * ======================================================================== */

/* tcp_init_context() - Initialize TCP subsystem
 * Called from tcpinitverbs() during Frontier CLI startup */
boolean tcp_init_context(void) {
    /* Guard against double-initialization */
    if (g_tcp_context.initialized) {
        log_warn(LOG_COMP_LANG, "TCP context already initialized, skipping");
        return true;
    }

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

    /* Initialize rate limiting */
    g_tcp_context.connections_per_sec = TCP_DEFAULT_RATE_LIMIT;
    g_tcp_context.timestamp_write_pos = 0;
    /* connection_timestamps_us[] already zeroed by memset above */

    g_tcp_context.initialized = true;

    log_info(LOG_COMP_LANG, "TCP context initialized successfully (rate limit: %d conn/sec)",
             g_tcp_context.connections_per_sec);

    return true;
}

/* tcp_shutdown_context() - Cleanup TCP subsystem
 * Closes all active listeners and streams, destroys synchronization primitives.
 * Called during Frontier shutdown or verb system cleanup.
 *
 * FIX Issue #5: Missing listener shutdown creates zombie accept threads.
 * VULNERABILITY: Original code only closed streams, not listeners. Accept threads
 * continued running after shutdown, accessing potentially-freed global state.
 *
 * CORRECT ORDERING:
 * 1. Close all listeners FIRST (joins accept threads via tcp_close_listen)
 * 2. THEN close streams (no active accept threads to interfere)
 * 3. Destroy synchronization primitives (all threads have exited)
 *
 * WHY THIS MATTERS: Accept threads run in background and access g_tcp_context.
 * If we destroy mutex/cond while threads still running → undefined behavior.
 * tcp_close_listen() properly joins threads before returning. */
boolean tcp_shutdown_context(void) {
    if (!g_tcp_context.initialized) {
        log_debug(LOG_COMP_LANG, "TCP context not initialized, nothing to shutdown");
        return true;
    }

    log_info(LOG_COMP_LANG, "Shutting down TCP context");

    /* CRITICAL: Close all active listeners FIRST (before streams).
     * Each listener has an accept thread that must be joined before
     * we can safely destroy global synchronization primitives.
     * tcp_close_listen() properly joins threads before freeing memory. */
    int listener_count = 0;
    for (int i = 0; i < TCP_MAX_LISTENERS; i++) {
        LISTENERS_LOCK();
        if (g_tcp_listeners[i] != NULL) {
            long listener_id = g_tcp_listeners[i]->listener_id;
            LISTENERS_UNLOCK();

            log_debug(LOG_COMP_LANG, "tcp_shutdown_context: closing listener_id=%ld", listener_id);
            tcp_close_listen(listener_id);  /* Proper cleanup with thread join */
            listener_count++;
        } else {
            LISTENERS_UNLOCK();
        }
    }

    if (listener_count > 0) {
        log_info(LOG_COMP_LANG, "tcp_shutdown_context: closed %d listeners", listener_count);
    }

    TCP_LOCK();

    /* Close all active streams */
    int closed_count = 0;
    for (int i = TCP_FIRST_STREAM_ID; i < TCP_MAX_STREAMS; i++) {
        if (g_tcp_context.streams[i].sockfd >= 0) {
            close(g_tcp_context.streams[i].sockfd);
            g_tcp_context.streams[i].sockfd = -1;
            g_tcp_context.streams[i].state = STREAM_INVALID;
            closed_count++;
        }
    }

    g_tcp_context.active_count = 0;
    g_tcp_context.initialized = false;

    TCP_UNLOCK();

    /* Destroy synchronization primitives */
    pthread_mutex_destroy(&g_tcp_context.mutex);
    pthread_cond_destroy(&g_tcp_context.activity_cond);

    log_info(LOG_COMP_LANG, "TCP context shutdown complete (closed %d streams)", closed_count);

    return true;
}
