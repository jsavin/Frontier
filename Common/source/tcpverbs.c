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
#include <sys/ioctl.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <pthread.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <stdlib.h>

#include "frontier.h"
#include "standard.h"
#include "tcpverbs.h"
#include "langexternal.h"
#include "langinternal.h"
#include "lang.h"  /* functionop, moduleop, setaddressvalue */
#include "process.h"  /* newprocess, addprocess */
#include "threadregistry.h"  /* headless_spawn_callback_thread */
#include "memory.h"
#include "strings.h"
#include "logging.h"
#include "file.h"  /* openfile, closefile, fileread */

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

/* ========================================================================
 * Callback Work Queue
 *
 * Accept threads cannot safely invoke UserTalk callbacks directly because
 * ODB (Object Database) operations are not thread-safe. Instead, accept
 * threads enqueue callback requests, and the main thread processes them.
 *
 * Queue is a circular buffer protected by a mutex. Main thread must call
 * tcp_process_callbacks() periodically (e.g., in REPL loop or idle handler).
 * ======================================================================== */

#define TCP_CONNECT_TIMEOUT_SECS 10   /* Non-blocking connect timeout */
#define TCP_SOCKET_TIMEOUT_SECS  30   /* Send/receive timeout */

#define TCP_CALLBACK_QUEUE_SIZE 64

typedef struct tcp_callback_item {
    hdlhashtable    callback_table;    /* Hash table containing callback */
    bigstring       callback_name;     /* Callback script name */
    long            stream_id;         /* Accepted stream ID */
    long            refcon;            /* User refcon data */
    boolean         valid;             /* Item is valid (not consumed) */
} tcp_callback_item_t;

static tcp_callback_item_t g_callback_queue[TCP_CALLBACK_QUEUE_SIZE];
static int g_callback_queue_head = 0;  /* Next write position */
static int g_callback_queue_tail = 0;  /* Next read position */
static pthread_mutex_t g_callback_queue_mutex = PTHREAD_MUTEX_INITIALIZER;

#define CALLBACK_QUEUE_LOCK()   pthread_mutex_lock(&g_callback_queue_mutex)
#define CALLBACK_QUEUE_UNLOCK() pthread_mutex_unlock(&g_callback_queue_mutex)

/* Enqueue a callback request (called from accept thread)
 * Returns true if queued, false if queue is full */
static boolean tcp_enqueue_callback(hdlhashtable htable, bigstring callback_name,
                                     long stream_id, long refcon) {
    boolean result = false;

    CALLBACK_QUEUE_LOCK();

    /* Check if queue is full */
    int next_head = (g_callback_queue_head + 1) % TCP_CALLBACK_QUEUE_SIZE;
    if (next_head != g_callback_queue_tail) {
        tcp_callback_item_t *item = &g_callback_queue[g_callback_queue_head];
        item->callback_table = htable;
        copystring(callback_name, item->callback_name);
        item->stream_id = stream_id;
        item->refcon = refcon;
        item->valid = true;

        g_callback_queue_head = next_head;
        result = true;

        log_debug(LOG_COMP_LANG, "tcp_enqueue_callback: queued callback stream_id=%ld refcon=%ld (queue depth=%d)",
                 stream_id, refcon, (g_callback_queue_head - g_callback_queue_tail + TCP_CALLBACK_QUEUE_SIZE) % TCP_CALLBACK_QUEUE_SIZE);
    } else {
        log_error(LOG_COMP_LANG, "tcp_enqueue_callback: queue full, dropping callback for stream_id=%ld", stream_id);
    }

    CALLBACK_QUEUE_UNLOCK();
    return result;
}

/* Process pending callbacks (called from main thread)
 *
 * Builds a proper function call AST for each callback, following the legacy
 * fwsruncallback pattern (WinSockNetEvents.c:1585-1637). This creates:
 *   module(function(callback_address, [stream_param, refcon_param]), nil)
 *
 * The AST-based approach is required because callbacks like inetd.supervisor
 * are `on handler(stream, refcon)` functions — the formal parameter binding
 * only works when params are in the function call tree (hparam1), not as
 * local variables.
 *
 * Execution model differs by build target:
 *
 * Full Frontier: Each callback is queued as a one-shot process via
 * newprocess + addprocess, matching the legacy fwsruncallback behavior.
 *
 * Headless mode: The process scheduler (newprocess/addprocess) is not
 * available. Instead, each callback is spawned as a GIL-aware POSIX thread
 * via headless_spawn_callback_thread(). The thread blocks on GIL acquisition
 * until the main thread yields at langbackgroundtask(), then executes the
 * callback with full error cleanup (fifcloseallfiles/langreleasesemaphores).
 * See ADR-014 for the GIL cooperative threading model.
 *
 * In both cases, callbacks can yield at langbackgroundtask() and
 * thread.sleepTicks() points, allowing concurrent request handling.
 *
 * Returns number of callbacks processed (dequeued, whether or not
 * they were successfully dispatched) */
int tcp_process_callbacks(void) {
    int processed = 0;

    while (1) {
        tcp_callback_item_t item;
        boolean have_item = false;

        /* Dequeue under lock */
        CALLBACK_QUEUE_LOCK();
        if (g_callback_queue_tail != g_callback_queue_head) {
            item = g_callback_queue[g_callback_queue_tail];
            g_callback_queue[g_callback_queue_tail].valid = false;
            g_callback_queue_tail = (g_callback_queue_tail + 1) % TCP_CALLBACK_QUEUE_SIZE;
            have_item = true;
        }
        CALLBACK_QUEUE_UNLOCK();

        if (!have_item)
            break;

        log_debug(LOG_COMP_LANG, "tcp_process_callbacks: enqueueing callback '%.*s' stream_id=%ld refcon=%ld",
                 stringlength(item.callback_name), item.callback_name + 1,
                 item.stream_id, item.refcon);

        /* Build function call AST: callback(stream_id, refcon)
         * Following fwsruncallback pattern from WinSockNetEvents.c.
         * Thread globals needed for setaddressvalue/pushfunctionreference
         * which access ODB structures. */

        if (!grabthreadglobals()) {
            log_warn(LOG_COMP_LANG, "tcp_process_callbacks: grabthreadglobals failed for stream_id=%ld", item.stream_id);
            processed++;
            continue;
        }

        /* Build callback address → function reference */
        tyvaluerecord addrval;
        hdltreenode hfunctionref;

        if (!setaddressvalue(item.callback_table, item.callback_name, &addrval)) {
            log_warn(LOG_COMP_LANG, "tcp_process_callbacks: setaddressvalue failed for stream_id=%ld", item.stream_id);
            releasethreadglobals();
            processed++;
            continue;
        }

        if (!pushfunctionreference(addrval, &hfunctionref)) {
            /* Don't dispose addrval here — pushfunctionreference always calls
             * exemptfromtmpstack first, removing addrval from auto-cleanup.
             * If newconstnode succeeded, addrval is owned by the tree node
             * and disposed by pushunaryoperation on its failure path. */
            log_warn(LOG_COMP_LANG, "tcp_process_callbacks: pushfunctionreference failed for stream_id=%ld", item.stream_id);
            releasethreadglobals();
            processed++;
            continue;
        }

        /* Build parameter nodes: [stream_id, refcon] */
        tyvaluerecord val;
        hdltreenode hparam1;
        hdltreenode hparam2;

        setlongvalue(item.stream_id, &val);

        if (!newconstnode(val, &hparam1)) {
            langdisposetree(hfunctionref);
            releasethreadglobals();
            processed++;
            continue;
        }

        setlongvalue(item.refcon, &val);

        if (!newconstnode(val, &hparam2)) {
            langdisposetree(hfunctionref);
            langdisposetree(hparam1);
            releasethreadglobals();
            processed++;
            continue;
        }

        /* Link params: hparam1 → hparam2 */
        pushlastlink(hparam2, hparam1);

        /* Build function call: function(callback_ref, param_list) */
        hdltreenode hfunctioncall;

        if (!pushbinaryoperation(functionop, hfunctionref, hparam1, &hfunctioncall)) {
            log_warn(LOG_COMP_LANG, "tcp_process_callbacks: pushbinaryoperation(functionop) failed for stream_id=%ld", item.stream_id);
            releasethreadglobals();
            processed++;
            continue;
        }

        /* Wrap in module: module(functioncall, nil) */
        hdltreenode hcode;

        if (!pushbinaryoperation(moduleop, hfunctioncall, nil, &hcode)) {
            log_warn(LOG_COMP_LANG, "tcp_process_callbacks: pushbinaryoperation(moduleop) failed for stream_id=%ld", item.stream_id);
            releasethreadglobals();
            processed++;
            continue;
        }

        #if defined(FRONTIER_HEADLESS)
        /* Headless mode: spawn a GIL-aware thread per callback.
         * The process scheduler (newprocess/addprocess) is not available
         * in the headless build. Each callback runs in its own POSIX thread
         * that acquires the GIL cooperatively (see ADR-014), allowing
         * concurrent request handling without blocking the REPL or other
         * threads. headless_spawn_callback_thread() takes ownership of hcode. */

        if (!headless_spawn_callback_thread(hcode, item.stream_id)) {
            log_warn(LOG_COMP_LANG, "tcp_process_callbacks: headless_spawn_callback_thread failed for stream_id=%ld", item.stream_id);
        }

        releasethreadglobals();
        #else
        /* Full Frontier: queue as one-shot process for the scheduler.
         * This allows the callback to yield at langbackgroundtask() and
         * thread.sleepTicks() points, matching legacy fwsruncallback behavior. */
        hdlprocessrecord hprocess;

        if (!newprocess(hcode, true, nil, 0, &hprocess)) {
            log_warn(LOG_COMP_LANG, "tcp_process_callbacks: newprocess failed for stream_id=%ld", item.stream_id);
            langdisposetree(hcode);
            releasethreadglobals();
            processed++;
            continue;
        }

        releasethreadglobals();

        if (!addprocess(hprocess)) {
            log_warn(LOG_COMP_LANG, "tcp_process_callbacks: addprocess failed for stream_id=%ld", item.stream_id);
            disposeprocess(hprocess);
        }
        #endif

        processed++;
    }

    return processed;
}

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

    /* Convert C string to Pascal string for langerrormessage */
    bigstring bs;
    copyctopstring(error_msg, bs);
    langerrormessage(bs);
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

/* Reset the tcp connection rate-limit sliding window and telemetry counter.
 *
 * Intended to be called from script/clearContext (a test-runner reset
 * boundary) so the long-lived protocol subprocess does not accumulate
 * cross-test rate-limit pressure. Without this reset, tests in earlier
 * yaml files that fire many connection attempts can poison the window for
 * ~1 second, causing subsequent tcp tests in the same process to
 * spuriously fail with "Connection rate limit exceeded" until the
 * timestamps age out. Issue #130.
 *
 * Does NOT touch:
 *   - connections_per_sec: user-set policy that must persist
 *   - streams[] / active_count / listeners[]: structural resources managed
 *     by their own acquire/release/close lifecycle
 *   - total_connections / failed_connections: lifetime telemetry, not
 *     behavior-gating
 *
 * DOES reset rate_limited_count because it counts window trips, not lifetime
 * trips. Keeping it across the reset would misrepresent the cleared window
 * (the counter would suggest the limiter has fired, even though the window
 * is now empty).
 *
 * Thread safety: takes TCP_LOCK. Safe to call from any thread that does
 * not already hold TCP_LOCK. */
void tcp_reset_rate_limit_window(void) {
    TCP_LOCK();
    memset(g_tcp_context.connection_timestamps_us, 0,
           sizeof(g_tcp_context.connection_timestamps_us));
    g_tcp_context.timestamp_write_pos = 0;
    g_tcp_context.rate_limited_count = 0;
    TCP_UNLOCK();
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

/* Connect with timeout using non-blocking socket + select().
 * Returns 0 on success, -1 on failure/timeout. */
static int tcp_connect_with_timeout(int sockfd, const struct sockaddr *addr,
                                     socklen_t addrlen, int timeout_secs) {
    int flags, ret, err;
    socklen_t len;
    fd_set wfds;
    struct timeval tv;

    /* Set non-blocking for connect */
    flags = fcntl(sockfd, F_GETFL, 0);
    if (flags < 0)
        return -1;
    if (fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) < 0)
        return -1;

    ret = connect(sockfd, addr, addrlen);
    if (ret == 0) {
        /* Connected immediately (e.g. localhost) — restore blocking */
        fcntl(sockfd, F_SETFL, flags);
        return 0;
    }
    if (errno != EINPROGRESS) {
        fcntl(sockfd, F_SETFL, flags);
        return -1;
    }

    /* Wait for connect to complete or timeout */
    FD_ZERO(&wfds);
    FD_SET(sockfd, &wfds);
    tv.tv_sec = timeout_secs;
    tv.tv_usec = 0;

    ret = select(sockfd + 1, NULL, &wfds, NULL, &tv);
    if (ret <= 0) {
        /* Timeout (ret==0) or error (ret<0) */
        fcntl(sockfd, F_SETFL, flags);
        return -1;
    }

    /* Check actual connection result */
    err = 0;
    len = sizeof(err);
    if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &err, &len) < 0 || err != 0) {
        fcntl(sockfd, F_SETFL, flags);
        return -1;
    }

    /* Restore blocking mode */
    fcntl(sockfd, F_SETFL, flags);
    return 0;
}

/* Set send/receive timeouts on a connected socket */
static void tcp_set_socket_timeouts(int sockfd, int timeout_secs) {
    struct timeval tv;
    tv.tv_sec = timeout_secs;
    tv.tv_usec = 0;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
        log_debug(LOG_COMP_LANG, "tcp_set_socket_timeouts: SO_RCVTIMEO failed (errno=%d)", errno);
    if (setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0)
        log_debug(LOG_COMP_LANG, "tcp_set_socket_timeouts: SO_SNDTIMEO failed (errno=%d)", errno);
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

    /* Connect with timeout */
    if (tcp_connect_with_timeout(sockfd, (struct sockaddr*)&server_addr,
                                  sizeof(server_addr), TCP_CONNECT_TIMEOUT_SECS) < 0) {
        close(sockfd);
        tcp_set_error(TCP_ERR_CONNECTION_FAILED, "Connection failed or timed out");
        return false;
    }

    /* Set send/receive timeouts */
    tcp_set_socket_timeouts(sockfd, TCP_SOCKET_TIMEOUT_SECS);

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

    /* Accept both STREAM_CONNECTED (client-initiated connect) and STREAM_ACCEPTED
     * (server-side accept inside a listener callback). Both represent a fully-
     * established TCP socket where read/write are valid. tcp_status_stream (~line
     * 1874) follows the same pattern. Without this, scripts using tcp.listenStream
     * callbacks cannot read incoming bytes or write responses, rendering the
     * listener primitive useless. Issue #129. */
    if (!stream || (stream->state != STREAM_CONNECTED && stream->state != STREAM_ACCEPTED)) {
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
    if (GetHandleSize(hdata) < (size_t) bytes_to_read) {
        disposehandle(hdata);
        tcp_stream_release(stream);
        *data_out = nil;
        tcp_set_error(TCP_ERR_NO_MEMORY, "Buffer allocation size mismatch");
        return false;
    }

    lockhandle(hdata);
    buffer = (char *) *hdata;

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

    /* Accept both STREAM_CONNECTED and STREAM_ACCEPTED states. See the matching
     * check in tcp_read_stream (~line 849) for the full rationale. Issue #129. */
    if (!stream || (stream->state != STREAM_CONNECTED && stream->state != STREAM_ACCEPTED)) {
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
    buffer = (char *) *hdata;

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

        /* Yield to other threads every 64KB */
        if ((total_written % 65536) < bytes_written && !langbackgroundtask(false)) {
            /* User cancelled */
            unlockhandle(hdata);
            tcp_stream_release(stream);
            tcp_set_error(TCP_ERR_SOCKET_ERROR, "Write cancelled");
            return false;
        }
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

        /* Attempt connection with timeout */
        if (tcp_connect_with_timeout(sockfd, rp->ai_addr, rp->ai_addrlen,
                                      TCP_CONNECT_TIMEOUT_SECS) == 0) {
            /* Success! Set send/receive timeouts */
            tcp_set_socket_timeouts(sockfd, TCP_SOCKET_TIMEOUT_SECS);
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
 * Accepts incoming connections and enqueues callback requests for main thread.
 *
 * THREAD-SAFETY DESIGN:
 * This thread does NOT invoke UserTalk callbacks directly because ODB operations
 * are not thread-safe. Instead, it:
 *   1. Accepts connections (pure socket operations - thread-safe)
 *   2. Allocates stream IDs (mutex-protected - thread-safe)
 *   3. Enqueues callback requests (mutex-protected queue - thread-safe)
 *
 * The main thread must call tcp_process_callbacks() to drain the queue.
 */
static void *tcp_accept_thread(void *arg) {
    tcp_listener_t *listener = (tcp_listener_t *)arg;
    struct sockaddr_in client_addr;
    socklen_t addr_len;
    int client_sock;
    int stream_id;

    log_info(LOG_COMP_LANG, "tcp_accept_thread: started for listener_id=%ld port=%d",
             listener->listener_id, listener->port);
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

        log_debug(LOG_COMP_LANG, "tcp_accept_thread: allocated stream_id=%d for connection", stream_id);

        /* Enqueue callback for main thread to process.
         *
         * THREAD-SAFETY: ODB operations are not thread-safe. The accept thread cannot
         * directly invoke UserTalk callbacks because they trigger database access
         * (script loading, compilation, etc.) which crashes when accessed concurrently.
         *
         * Solution: Enqueue callback requests to a thread-safe work queue. The main
         * thread must call tcp_process_callbacks() to drain the queue and invoke
         * callbacks safely. This is typically done in the REPL loop or idle handler.
         */
        if (stringlength(listener->callback_name) > 0 && listener->callback_table != nil) {
            log_debug(LOG_COMP_LANG, "tcp_accept_thread: enqueueing callback for stream_id=%d refcon=%ld",
                     stream_id, listener->refcon);

            if (!tcp_enqueue_callback(listener->callback_table, listener->callback_name,
                                      stream_id, listener->refcon)) {
                /*
                 * Queue overflow: Clean up the stream to prevent resource leaks.
                 * Without this, the socket stays open, the stream slot stays allocated,
                 * and the client hangs indefinitely waiting for a response that never comes.
                 */
                log_error(LOG_COMP_LANG, "tcp_accept_thread: queue full, closing stream_id=%d to prevent resource leak", stream_id);
                close(client_sock);
                TCP_LOCK();
                stream->state = STREAM_CLOSED;
                stream->sockfd = -1;
                g_tcp_context.active_count--;
                TCP_UNLOCK();
            }
        }
    }

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

/* ========================================================================
 * Phase 2: Status and Peer Information Verbs
 * ======================================================================== */

/* tcp.statusStream(stream) -> status, bytesPending
 * Get status of a TCP stream and number of bytes available to read.
 * Status values: "DATA", "OPEN", "INACTIVE", "CLOSED", "CLOSING", "UNKNOWN",
 * "LISTENING", "STOPPED".
 *
 * The input ID may be either a stream reference (from tcp.openStream) or a
 * listener reference (from tcp.listenStream). The listener-ID overload is a
 * legacy shared-ID-space behavior used by scripts such as
 * inetd.isDaemonRunning() to check whether a listener is still active; it
 * returns "LISTENING" (listener still running) or "STOPPED" (listener slot
 * still in registry but no longer running).
 *
 * Parameters:
 *   stream_id       - Stream or listener ID to check
 *   status_out      - Output: Status string (Pascal string)
 *   bytes_pending   - Output: Number of bytes available to read (may be NULL)
 *
 * Returns:
 *   true on success, false on error (invalid stream)
 */
boolean tcp_status_stream(long stream_id, bigstring status_out, long *bytes_pending_out) {
    tcp_stream_t *stream;
    int sockfd;
    fd_set readset;
    struct timeval tv;
    int select_result;
    int bytes_pending = 0;  /* Must be int for ioctl(FIONREAD) */

    log_debug(LOG_COMP_LANG, "tcp_status_stream: stream_id=%ld", stream_id);

    /* NULL pointer validation */
    if (!status_out) {
        tcp_set_error(TCP_ERR_SOCKET_ERROR, "NULL output pointer");
        return false;
    }

    /* Initialize output */
    setemptystring(status_out);
    if (bytes_pending_out)
        *bytes_pending_out = 0;

    /* Acquire stream reference (TOCTOU protection) */
    stream = tcp_stream_acquire(stream_id);
    if (!stream) {
        /* Stream not found - check if this is a listener ID instead.
         * Legacy behavior: both listeners and streams share the same ID space,
         * so tcp.statusStream() on a listener should return "LISTENING".
         * This is used by scripts like inetd.isDaemonRunning() to check
         * if a listener is still active. */
        LISTENERS_LOCK();
        for (int i = 0; i < TCP_MAX_LISTENERS; i++) {
            if (g_tcp_listeners[i] != NULL &&
                g_tcp_listeners[i]->listener_id == stream_id) {
                /* Found a listener with this ID */
                if (g_tcp_listeners[i]->running) {
                    copyctopstring("LISTENING", status_out);
                } else {
                    copyctopstring("STOPPED", status_out);
                }
                LISTENERS_UNLOCK();
                return true;
            }
        }
        LISTENERS_UNLOCK();

        /* Neither stream nor listener found - return INACTIVE (not an error) */
        copyctopstring("INACTIVE", status_out);
        return true;  /* Query succeeded - stream is inactive */
    }

    sockfd = stream->sockfd;

    /* Handle stream states */
    switch (stream->state) {
        case STREAM_INVALID:
            /* Stream exists but is in invalid state - also not an error for queries */
            copyctopstring("INACTIVE", status_out);
            tcp_stream_release(stream);
            return true;  /* Query succeeded - stream is inactive */

        case STREAM_CONNECTING:
            copyctopstring("UNKNOWN", status_out);
            break;

        case STREAM_CONNECTED:
        case STREAM_ACCEPTED:
            /* Check for data availability using select() with zero timeout */
            FD_ZERO(&readset);
            FD_SET(sockfd, &readset);
            tv.tv_sec = 0;
            tv.tv_usec = 0;

            select_result = select(sockfd + 1, &readset, NULL, NULL, &tv);

            if (select_result < 0) {
                /* select() error - stream may be in bad state */
                log_warn(LOG_COMP_LANG, "tcp_status_stream: select() failed: %s", strerror(errno));
                copyctopstring("INACTIVE", status_out);
            } else if (select_result == 0) {
                /* No data available - stream is open but idle */
                copyctopstring("OPEN", status_out);
            } else {
                /* Data might be available - check with ioctl */
                if (ioctl(sockfd, FIONREAD, &bytes_pending) < 0) {
                    log_warn(LOG_COMP_LANG, "tcp_status_stream: ioctl(FIONREAD) failed: %s", strerror(errno));
                    copyctopstring("INACTIVE", status_out);
                } else if (bytes_pending == 0) {
                    /* select() returned readable but no bytes - connection closed by peer */
                    copyctopstring("INACTIVE", status_out);
                } else {
                    /* Data available */
                    copyctopstring("DATA", status_out);
                    if (bytes_pending_out)
                        *bytes_pending_out = bytes_pending;
                }
            }
            break;

        /* Note: STREAM_LISTENING is declared in the tcp_stream_state_t enum
         * but is never assigned to a stream's state field — listeners live in
         * g_tcp_listeners[], not in g_tcp_context.streams[]. The "LISTENING"
         * status is returned only by the listener-ID lookup branch above. */

        case STREAM_CLOSING:
            copyctopstring("CLOSING", status_out);
            break;

        case STREAM_CLOSED:
            copyctopstring("CLOSED", status_out);
            break;

        default:
            copyctopstring("UNKNOWN", status_out);
            break;
    }

    tcp_stream_release(stream);

    log_debug(LOG_COMP_LANG, "tcp_status_stream: status=%s bytes_pending=%ld",
              stringbaseaddress(status_out), bytes_pending);

    return true;
}

/* tcp.getPeerAddress(stream) -> addr
 * Get the remote IP address of a connected stream.
 *
 * Parameters:
 *   stream_id  - Stream ID to query
 *   addr_out   - Output: Remote IP address (host byte order)
 *
 * Returns:
 *   true on success, false on error
 */
boolean tcp_get_peer_address(long stream_id, long *addr_out) {
    tcp_stream_t *stream;
    struct sockaddr_in peer_addr;
    socklen_t addr_len = sizeof(peer_addr);

    log_debug(LOG_COMP_LANG, "tcp_get_peer_address: stream_id=%ld", stream_id);

    /* NULL pointer validation */
    if (!addr_out) {
        tcp_set_error(TCP_ERR_SOCKET_ERROR, "NULL output pointer");
        return false;
    }

    /* Acquire stream reference (TOCTOU protection) */
    stream = tcp_stream_acquire(stream_id);
    if (!stream) {
        tcp_set_error(TCP_ERR_INVALID_STREAM, "Invalid stream");
        return false;
    }

    /* Use getpeername() for accurate peer info */
    if (getpeername(stream->sockfd, (struct sockaddr *)&peer_addr, &addr_len) < 0) {
        tcp_stream_release(stream);
        tcp_set_error(TCP_ERR_SOCKET_ERROR, "getpeername() failed");
        return false;
    }

    tcp_stream_release(stream);

    /* Convert to host byte order */
    *addr_out = (long)ntohl(peer_addr.sin_addr.s_addr);

    log_debug(LOG_COMP_LANG, "tcp_get_peer_address: addr=%ld", *addr_out);

    return true;
}

/* tcp.getPeerPort(stream) -> port
 * Get the remote port of a connected stream.
 *
 * Parameters:
 *   stream_id  - Stream ID to query
 *   port_out   - Output: Remote port number (host byte order)
 *
 * Returns:
 *   true on success, false on error
 */
boolean tcp_get_peer_port(long stream_id, long *port_out) {
    tcp_stream_t *stream;
    struct sockaddr_in peer_addr;
    socklen_t addr_len = sizeof(peer_addr);

    log_debug(LOG_COMP_LANG, "tcp_get_peer_port: stream_id=%ld", stream_id);

    /* NULL pointer validation */
    if (!port_out) {
        tcp_set_error(TCP_ERR_SOCKET_ERROR, "NULL output pointer");
        return false;
    }

    /* Acquire stream reference (TOCTOU protection) */
    stream = tcp_stream_acquire(stream_id);
    if (!stream) {
        tcp_set_error(TCP_ERR_INVALID_STREAM, "Invalid stream");
        return false;
    }

    /* Use getpeername() for accurate peer info */
    if (getpeername(stream->sockfd, (struct sockaddr *)&peer_addr, &addr_len) < 0) {
        tcp_stream_release(stream);
        tcp_set_error(TCP_ERR_SOCKET_ERROR, "getpeername() failed");
        return false;
    }

    tcp_stream_release(stream);

    /* Convert to host byte order */
    *port_out = (long)ntohs(peer_addr.sin_port);

    log_debug(LOG_COMP_LANG, "tcp_get_peer_port: port=%ld", *port_out);

    return true;
}

/* tcp.myAddress() -> addr
 * Get the local machine's primary IP address.
 * Returns first non-loopback, up, IPv4 interface address.
 * Falls back to 127.0.0.1 if no suitable address found.
 *
 * Parameters:
 *   addr_out   - Output: Local IP address (host byte order)
 *
 * Returns:
 *   true on success (always succeeds with fallback)
 */
boolean tcp_my_address(long *addr_out) {
    struct ifaddrs *myaddrs, *ifa;

    log_debug(LOG_COMP_LANG, "tcp_my_address: getting local address");

    /* NULL pointer validation */
    if (!addr_out) {
        tcp_set_error(TCP_ERR_SOCKET_ERROR, "NULL output pointer");
        return false;
    }

    /* Default to loopback */
    *addr_out = 0x7F000001;  /* 127.0.0.1 */

    if (getifaddrs(&myaddrs) != 0) {
        log_warn(LOG_COMP_LANG, "tcp_my_address: getifaddrs() failed: %s", strerror(errno));
        /* Return loopback as fallback */
        return true;
    }

    for (ifa = myaddrs; ifa != NULL; ifa = ifa->ifa_next) {
        /* Skip entries without addresses */
        if (ifa->ifa_addr == NULL)
            continue;

        /* Skip interfaces that are down */
        if (!(ifa->ifa_flags & IFF_UP))
            continue;

        /* Skip loopback interfaces */
        if (ifa->ifa_flags & IFF_LOOPBACK)
            continue;

        /* Only IPv4 */
        if (ifa->ifa_addr->sa_family == AF_INET) {
            struct sockaddr_in *s4 = (struct sockaddr_in *)ifa->ifa_addr;
            *addr_out = (long)ntohl(s4->sin_addr.s_addr);
            log_info(LOG_COMP_LANG, "tcp_my_address: found address on interface %s", ifa->ifa_name);
            break;
        }
    }

    freeifaddrs(myaddrs);

    log_debug(LOG_COMP_LANG, "tcp_my_address: addr=%ld", *addr_out);

    return true;
}

/* ========================================================================
 * Phase 3: Buffered I/O with Timeouts
 * ======================================================================== */

/* Timeout infrastructure using microsecond precision
 *
 * Implements both idle timeout and minimum throughput protection:
 * - Idle timeout: Resets on each successful read (user-specified)
 * - Throughput window: Requires at least 1 byte per 35-second window
 *
 * The throughput window prevents slow-trickle attacks where a malicious
 * peer sends data just often enough to reset the idle timeout but slow
 * enough to hold resources indefinitely. The 35-second interval provides
 * margin over typical WebSocket ping intervals (25-30 seconds).
 */
#define TCP_THROUGHPUT_WINDOW_SECS 35   /* Minimum throughput window */
#define TCP_MIN_BYTES_PER_WINDOW 1      /* Must receive at least 1 byte per window */

typedef struct {
    uint64_t idle_deadline_us;          /* Resets on any activity (user timeout) */
    uint64_t throughput_window_start_us; /* Start of current throughput window */
    long bytes_in_window;               /* Bytes received in current window */
} tcp_timeout_t;

static uint64_t tcp_get_now_us(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000ULL + tv.tv_usec;
}

static void tcp_timeout_init(tcp_timeout_t *t, long timeout_secs) {
    uint64_t now_us = tcp_get_now_us();
    t->idle_deadline_us = now_us + (uint64_t)timeout_secs * 1000000ULL;
    t->throughput_window_start_us = now_us;
    t->bytes_in_window = 0;
}

static void tcp_timeout_reset_idle(tcp_timeout_t *t, long timeout_secs) {
    t->idle_deadline_us = tcp_get_now_us() + (uint64_t)timeout_secs * 1000000ULL;
}

static void tcp_timeout_record_bytes(tcp_timeout_t *t, long bytes_received) {
    uint64_t now_us = tcp_get_now_us();
    uint64_t window_elapsed_us = now_us - t->throughput_window_start_us;

    /* If window has elapsed, start a new window */
    if (window_elapsed_us >= (uint64_t)TCP_THROUGHPUT_WINDOW_SECS * 1000000ULL) {
        t->throughput_window_start_us = now_us;
        t->bytes_in_window = bytes_received;
    } else {
        t->bytes_in_window += bytes_received;
    }
}

static boolean tcp_timeout_expired(tcp_timeout_t *t) {
    uint64_t now_us = tcp_get_now_us();

    /* Check idle timeout */
    if (now_us >= t->idle_deadline_us)
        return true;

    /* Check throughput window - if a full window has passed with insufficient data */
    uint64_t window_elapsed_us = now_us - t->throughput_window_start_us;
    if (window_elapsed_us >= (uint64_t)TCP_THROUGHPUT_WINDOW_SECS * 1000000ULL) {
        if (t->bytes_in_window < TCP_MIN_BYTES_PER_WINDOW) {
            log_debug(LOG_COMP_LANG, "tcp_timeout_expired: throughput too low (%ld bytes in %d sec window)",
                      t->bytes_in_window, TCP_THROUGHPUT_WINDOW_SECS);
            return true;
        }
        /* Window passed with sufficient data - reset for next window */
        t->throughput_window_start_us = now_us;
        t->bytes_in_window = 0;
    }

    return false;
}

__attribute__((unused))
static long tcp_timeout_remaining_ms(tcp_timeout_t *t) {
    uint64_t now_us = tcp_get_now_us();
    if (now_us >= t->idle_deadline_us)
        return 0;
    return (long)((t->idle_deadline_us - now_us) / 1000);
}

/* Default timeout and chunk size for buffered I/O */
#define TCP_DEFAULT_TIMEOUT_SECS 60
#define TCP_DEFAULT_CHUNK_SIZE 8192

/* Timeout for subsequent reads after first packet (inetd-style) */
#define TCP_INETD_SUBSEQUENT_TIMEOUT_SECS 1

/* Read condition types for unified read implementation */
typedef enum {
    TCP_READ_UNTIL_CLOSED,
    TCP_READ_UNTIL_BYTES,
    TCP_READ_UNTIL_PATTERN
} tcp_read_condition_t;

/* Unified read implementation
 * Reads from stream until condition is met or timeout expires.
 *
 * Timeout behavior:
 * - Idle timeout: Resets on each successful read (user-specified via timeout_secs)
 * - Throughput protection: Connection must transfer at least 1 byte per 35 seconds
 *
 * Pattern matching optimization:
 * - For TCP_READ_UNTIL_PATTERN, we only scan newly-read bytes plus overlap
 * - This provides O(n) performance instead of O(n²) for large buffers
 */
static boolean tcp_read_until_condition(
    long stream_id,
    Handle hbuffer,
    tcp_read_condition_t condition,
    long target_bytes,
    Handle hpattern,
    long timeout_secs)
{
    tcp_stream_t *stream;
    int sockfd;
    tcp_timeout_t timeout;
    long current_size;
    long bytes_read_total;
    long pattern_scan_start;  /* For O(n) pattern matching optimization */
    long pattern_len;

    log_debug(LOG_COMP_LANG, "tcp_read_until_condition: stream_id=%ld condition=%d target=%ld timeout=%ld",
              stream_id, condition, target_bytes, timeout_secs);

    /* Validate timeout */
    if (timeout_secs <= 0)
        timeout_secs = TCP_DEFAULT_TIMEOUT_SECS;

    /* Acquire stream reference (TOCTOU protection) */
    stream = tcp_stream_acquire(stream_id);

    /* Issue #666: gate on stream state in addition to !stream. Accept
     * STREAM_CONNECTED (client-initiated connect) and STREAM_ACCEPTED
     * (server-side accept inside a listener callback); reject everything
     * else (notably STREAM_LISTENING and STREAM_CONNECTING). Mirrors the
     * post-#667 gate in tcp_read_stream (~line 889) and tcp_write_stream. */
    if (!stream || (stream->state != STREAM_CONNECTED && stream->state != STREAM_ACCEPTED)) {
        if (stream)
            tcp_stream_release(stream);
        tcp_set_error(TCP_ERR_INVALID_STREAM, "Stream not connected");
        return false;
    }

    sockfd = stream->sockfd;

    /* Initialize timeout */
    tcp_timeout_init(&timeout, timeout_secs);

    /* Get initial buffer size (may have prior data) */
    current_size = gethandlesize(hbuffer);
    bytes_read_total = current_size;

    /* Initialize pattern matching optimization */
    pattern_len = (condition == TCP_READ_UNTIL_PATTERN && hpattern) ? gethandlesize(hpattern) : 0;
    pattern_scan_start = 0;  /* First scan starts at beginning */

    while (true) {
        fd_set readset;
        struct timeval tv;
        int select_result;
        int bytes_available;  /* Must be int for ioctl(FIONREAD) */
        long bytes_to_read;
        ssize_t recv_result;

        /* Check for timeout */
        if (tcp_timeout_expired(&timeout)) {
            tcp_stream_release(stream);
            tcp_set_error(TCP_ERR_TIMEOUT, "Read timeout");
            return false;
        }

        /* Check if condition is met */
        if (condition == TCP_READ_UNTIL_PATTERN) {
            /* O(n) optimization: only scan from pattern_scan_start, not from 0
             * This avoids O(n²) behavior when reading large amounts of data.
             * We need overlap of (pattern_len - 1) to catch patterns spanning reads.
             */
            if (searchhandle(hbuffer, hpattern, pattern_scan_start, current_size) >= 0) {
                /* Pattern found */
                break;
            }
            /* Update scan start for next iteration - back up by pattern overlap */
            if (current_size > pattern_len) {
                pattern_scan_start = current_size - pattern_len + 1;
            }
        } else if (condition == TCP_READ_UNTIL_BYTES) {
            if (bytes_read_total >= target_bytes) {
                /* Read enough bytes */
                break;
            }
        }
        /* TCP_READ_UNTIL_CLOSED continues until recv returns 0 */

        /* Use select() with short timeout to check for data */
        FD_ZERO(&readset);
        FD_SET(sockfd, &readset);
        tv.tv_sec = 0;
        tv.tv_usec = 100000;  /* 100ms poll interval */

        select_result = select(sockfd + 1, &readset, NULL, NULL, &tv);

        if (select_result < 0) {
            if (errno == EINTR)
                continue;  /* Interrupted, retry */
            tcp_stream_release(stream);
            tcp_set_error(TCP_ERR_SOCKET_ERROR, "select() failed");
            return false;
        }

        if (select_result == 0) {
            /* No data available yet, continue waiting */
            continue;
        }

        /* Data available - check how many bytes */
        if (ioctl(sockfd, FIONREAD, &bytes_available) < 0) {
            tcp_stream_release(stream);
            tcp_set_error(TCP_ERR_SOCKET_ERROR, "ioctl(FIONREAD) failed");
            return false;
        }

        if (bytes_available == 0) {
            /* select() returned readable but no bytes - connection closed */
            if (condition == TCP_READ_UNTIL_CLOSED) {
                /* This is expected for READ_UNTIL_CLOSED */
                break;
            }
            /* For other conditions, this is an error (connection closed prematurely) */
            tcp_stream_release(stream);
            tcp_set_error(TCP_ERR_SOCKET_ERROR, "Connection closed unexpectedly");
            return false;
        }

        /* Limit bytes to read for BYTES condition */
        bytes_to_read = bytes_available;
        if (condition == TCP_READ_UNTIL_BYTES) {
            long remaining = target_bytes - bytes_read_total;
            if (bytes_to_read > remaining)
                bytes_to_read = remaining;
        }

        /* Expand buffer to hold new data */
        if (!sethandlesize(hbuffer, current_size + bytes_to_read)) {
            tcp_stream_release(stream);
            tcp_set_error(TCP_ERR_NO_MEMORY, "Could not expand buffer");
            return false;
        }

        /* Read data into buffer */
        lockhandle(hbuffer);
        recv_result = recv(sockfd, (*hbuffer) + current_size, bytes_to_read, 0);
        unlockhandle(hbuffer);

        if (recv_result < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                continue;
            tcp_stream_release(stream);
            tcp_set_error(TCP_ERR_SOCKET_ERROR, "recv() failed");
            return false;
        }

        if (recv_result == 0) {
            /* Connection closed */
            if (condition == TCP_READ_UNTIL_CLOSED) {
                /* Shrink buffer to actual size */
                sethandlesize(hbuffer, current_size);
                break;
            }
            tcp_stream_release(stream);
            tcp_set_error(TCP_ERR_SOCKET_ERROR, "Connection closed unexpectedly");
            return false;
        }

        /* Adjust buffer if we read less than expected */
        if (recv_result < bytes_to_read) {
            sethandlesize(hbuffer, current_size + recv_result);
        }

        current_size = gethandlesize(hbuffer);
        bytes_read_total = current_size;

        /* Reset idle timeout and record throughput on successful read */
        tcp_timeout_reset_idle(&timeout, timeout_secs);
        tcp_timeout_record_bytes(&timeout, recv_result);

        log_trace(LOG_COMP_LANG, "tcp_read_until_condition: read %zd bytes, total=%ld",
                  recv_result, bytes_read_total);
    }

    /* Update activity timestamp */
    TCP_LOCK();
    stream->last_activity = time(NULL);
    TCP_UNLOCK();

    tcp_stream_release(stream);

    log_debug(LOG_COMP_LANG, "tcp_read_until_condition: completed, total=%ld", bytes_read_total);

    return true;
}

/* tcp.readStreamUntil(stream, buffer, pattern, timeout) -> true
 * Read from stream until pattern is found or timeout.
 * Pattern is searched in buffer (which may have prior data).
 *
 * Parameters:
 *   stream_id    - Stream ID to read from
 *   hbuffer      - Handle to buffer (data appended, may have prior data)
 *   hpattern     - Handle containing pattern to search for
 *   timeout_secs - Idle timeout in seconds (0 = default 60s)
 *
 * Returns:
 *   true if pattern found, false on error or timeout
 */
boolean tcp_read_stream_until(long stream_id, Handle hbuffer, Handle hpattern, long timeout_secs) {
    log_debug(LOG_COMP_LANG, "tcp_read_stream_until: stream_id=%ld timeout=%ld", stream_id, timeout_secs);

    if (!hbuffer) {
        tcp_set_error(TCP_ERR_SOCKET_ERROR, "NULL buffer handle");
        return false;
    }

    if (!hpattern || gethandlesize(hpattern) == 0) {
        tcp_set_error(TCP_ERR_SOCKET_ERROR, "Empty or NULL pattern");
        return false;
    }

    return tcp_read_until_condition(stream_id, hbuffer, TCP_READ_UNTIL_PATTERN,
                                    0, hpattern, timeout_secs);
}

/* tcp.readStreamBytes(stream, buffer, count, timeout) -> true
 * Read exactly 'count' bytes from stream.
 *
 * Parameters:
 *   stream_id    - Stream ID to read from
 *   hbuffer      - Handle to buffer (data appended)
 *   count        - Number of bytes to read
 *   timeout_secs - Idle timeout in seconds (0 = default 60s)
 *
 * Returns:
 *   true if all bytes read, false on error or timeout
 */
boolean tcp_read_stream_bytes(long stream_id, Handle hbuffer, long count, long timeout_secs) {
    log_debug(LOG_COMP_LANG, "tcp_read_stream_bytes: stream_id=%ld count=%ld timeout=%ld",
              stream_id, count, timeout_secs);

    if (!hbuffer) {
        tcp_set_error(TCP_ERR_SOCKET_ERROR, "NULL buffer handle");
        return false;
    }

    if (count <= 0) {
        tcp_set_error(TCP_ERR_SOCKET_ERROR, "Invalid byte count");
        return false;
    }

    return tcp_read_until_condition(stream_id, hbuffer, TCP_READ_UNTIL_BYTES,
                                    count, nil, timeout_secs);
}

/* tcp.readStreamUntilClosed(stream, buffer, timeout) -> true
 * Read from stream until remote closes connection.
 *
 * Parameters:
 *   stream_id    - Stream ID to read from
 *   hbuffer      - Handle to buffer (data appended)
 *   timeout_secs - Idle timeout in seconds (0 = default 60s)
 *
 * Returns:
 *   true if stream closed gracefully, false on error or timeout
 */
boolean tcp_read_stream_until_closed(long stream_id, Handle hbuffer, long timeout_secs) {
    log_debug(LOG_COMP_LANG, "tcp_read_stream_until_closed: stream_id=%ld timeout=%ld",
              stream_id, timeout_secs);

    if (!hbuffer) {
        tcp_set_error(TCP_ERR_SOCKET_ERROR, "NULL buffer handle");
        return false;
    }

    return tcp_read_until_condition(stream_id, hbuffer, TCP_READ_UNTIL_CLOSED,
                                    0, nil, timeout_secs);
}

/* tcp.readStreamInetd(stream, buffer, timeout) -> true
 * Read from stream with two-stage timeout behavior for inetd-style requests.
 *
 * This implements special inetd read semantics:
 * - Wait 'timeout_secs' for first packet
 * - After first packet received, reduce timeout to TCP_INETD_SUBSEQUENT_TIMEOUT_SECS
 * - Continue reading until timeout or connection closed
 * - Return SUCCESS on timeout (not error) - graceful termination
 *
 * This is used by webserver.inetd for reading complete HTTP requests.
 *
 * Parameters:
 *   stream_id    - Stream ID to read from
 *   hbuffer      - Handle to buffer (data appended)
 *   timeout_secs - Initial timeout in seconds for first packet
 *
 * Returns:
 *   true if data was read (or timeout after receiving data), false on error
 */
boolean tcp_read_stream_inetd(long stream_id, Handle hbuffer, long timeout_secs) {
    tcp_stream_t *stream = NULL;
    int sockfd;
    long current_size;
    boolean first_packet_received = false;
    long effective_timeout;
    time_t start_time, last_read_time;
    boolean result = false;

    log_debug(LOG_COMP_LANG, "tcp_read_stream_inetd: stream_id=%ld timeout=%ld",
              stream_id, timeout_secs);

    if (!hbuffer) {
        tcp_set_error(TCP_ERR_SOCKET_ERROR, "NULL buffer handle");
        return false;
    }

    /* Validate timeout */
    if (timeout_secs <= 0)
        timeout_secs = TCP_DEFAULT_TIMEOUT_SECS;

    /* Acquire stream reference (TOCTOU protection) */
    stream = tcp_stream_acquire(stream_id);

    /* Issue #666: gate on stream state. Accept STREAM_CONNECTED /
     * STREAM_ACCEPTED only; reject STREAM_LISTENING / STREAM_CONNECTING
     * up-front rather than surfacing a kernel error from recv() on a
     * non-readable socket. Mirrors tcp_read_stream (~line 889). */
    if (!stream || (stream->state != STREAM_CONNECTED && stream->state != STREAM_ACCEPTED)) {
        if (stream)
            tcp_stream_release(stream);
        tcp_set_error(TCP_ERR_INVALID_STREAM, "Stream not connected");
        return false;
    }

    sockfd = stream->sockfd;

    /* Get initial buffer size */
    current_size = gethandlesize(hbuffer);
    start_time = time(NULL);
    last_read_time = start_time;
    effective_timeout = timeout_secs;

    while (true) {
        fd_set readset;
        struct timeval tv;
        int select_result;
        int bytes_available;
        ssize_t recv_result;
        time_t now = time(NULL);
        long elapsed;

        /* Check timeout with bounds checking to prevent underflow */
        if (first_packet_received) {
            /* After first packet, use short timeout from last read */
            elapsed = (long)(now - last_read_time);
            if (elapsed >= TCP_INETD_SUBSEQUENT_TIMEOUT_SECS) {
                /* Timeout after receiving data - this is SUCCESS for inetd */
                log_debug(LOG_COMP_LANG, "tcp_read_stream_inetd: timeout after data, total=%ld", current_size);
                result = true;
                goto cleanup;
            }
            effective_timeout = TCP_INETD_SUBSEQUENT_TIMEOUT_SECS;
        } else {
            /* Before first packet, use full timeout with bounds check */
            elapsed = (long)(now - start_time);
            if (elapsed >= timeout_secs) {
                /* Timeout before any data - also SUCCESS for inetd (empty request) */
                log_debug(LOG_COMP_LANG, "tcp_read_stream_inetd: timeout before data");
                result = true;
                goto cleanup;
            }
            /* Bounds check: ensure effective_timeout is positive */
            effective_timeout = timeout_secs - elapsed;
            if (effective_timeout <= 0)
                effective_timeout = 1;  /* Minimum 1 second */
        }

        /* Use select() with remaining timeout */
        FD_ZERO(&readset);
        FD_SET(sockfd, &readset);
        tv.tv_sec = effective_timeout;
        tv.tv_usec = 0;

        select_result = select(sockfd + 1, &readset, NULL, NULL, &tv);

        if (select_result < 0) {
            if (errno == EINTR)
                continue;
            tcp_set_error(TCP_ERR_SOCKET_ERROR, "select() failed");
            goto cleanup;
        }

        if (select_result == 0) {
            /* Timeout - this is SUCCESS for inetd */
            log_debug(LOG_COMP_LANG, "tcp_read_stream_inetd: select timeout, total=%ld", current_size);
            result = true;
            goto cleanup;
        }

        /* Data available - check how many bytes */
        if (ioctl(sockfd, FIONREAD, &bytes_available) < 0) {
            tcp_set_error(TCP_ERR_SOCKET_ERROR, "ioctl(FIONREAD) failed");
            goto cleanup;
        }

        if (bytes_available == 0) {
            /* Connection closed - this is SUCCESS for inetd */
            log_debug(LOG_COMP_LANG, "tcp_read_stream_inetd: connection closed, total=%ld", current_size);
            result = true;
            goto cleanup;
        }

        /* Expand buffer to hold new data */
        if (!sethandlesize(hbuffer, current_size + bytes_available)) {
            tcp_set_error(TCP_ERR_NO_MEMORY, "Could not expand buffer");
            goto cleanup;
        }

        /* Read data into buffer */
        lockhandle(hbuffer);
        recv_result = recv(sockfd, (*hbuffer) + current_size, bytes_available, 0);
        unlockhandle(hbuffer);

        if (recv_result < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                continue;
            tcp_set_error(TCP_ERR_SOCKET_ERROR, "recv() failed");
            goto cleanup;
        }

        if (recv_result == 0) {
            /* Connection closed - shrink buffer and succeed */
            sethandlesize(hbuffer, current_size);
            log_debug(LOG_COMP_LANG, "tcp_read_stream_inetd: recv=0, total=%ld", current_size);
            result = true;
            goto cleanup;
        }

        /* Adjust buffer if we read less than expected */
        if (recv_result < bytes_available) {
            sethandlesize(hbuffer, current_size + recv_result);
        }

        current_size = gethandlesize(hbuffer);
        first_packet_received = true;
        last_read_time = time(NULL);

        log_trace(LOG_COMP_LANG, "tcp_read_stream_inetd: read %zd bytes, total=%ld",
                  recv_result, current_size);
    }

    result = true;

cleanup:
    if (stream) {
        /* Update activity timestamp */
        TCP_LOCK();
        stream->last_activity = time(NULL);
        TCP_UNLOCK();

        tcp_stream_release(stream);
    }

    log_debug(LOG_COMP_LANG, "tcp_read_stream_inetd: completed, total=%ld, result=%d",
              current_size, result);

    return result;
}

/* tcp.writeStringToStream(stream, data, chunksize, timeout) -> true
 * Write data to stream in chunks, with timeout between chunks.
 *
 * Parameters:
 *   stream_id    - Stream ID to write to
 *   hdata        - Handle containing data to write
 *   chunk_size   - Size of each chunk (0 = default 8KB)
 *   timeout_secs - Timeout for each chunk write (0 = default 60s)
 *
 * Returns:
 *   true if all data written, false on error or timeout
 */
boolean tcp_write_string_to_stream(long stream_id, Handle hdata, long chunk_size, long timeout_secs) {
    tcp_stream_t *stream;
    int sockfd;
    long data_size;
    long bytes_written = 0;

    log_debug(LOG_COMP_LANG, "tcp_write_string_to_stream: stream_id=%ld chunk=%ld timeout=%ld",
              stream_id, chunk_size, timeout_secs);

    /* Validate parameters */
    if (!hdata) {
        tcp_set_error(TCP_ERR_SOCKET_ERROR, "NULL data handle");
        return false;
    }

    if (chunk_size <= 0)
        chunk_size = TCP_DEFAULT_CHUNK_SIZE;

    if (timeout_secs <= 0)
        timeout_secs = TCP_DEFAULT_TIMEOUT_SECS;

    data_size = gethandlesize(hdata);
    if (data_size == 0) {
        /* Nothing to write */
        return true;
    }

    /* Acquire stream reference (TOCTOU protection) */
    stream = tcp_stream_acquire(stream_id);

    /* Issue #666: gate on stream state. Accept STREAM_CONNECTED /
     * STREAM_ACCEPTED only; reject STREAM_LISTENING / STREAM_CONNECTING
     * up-front rather than surfacing a kernel error from send() on a
     * non-writable socket. Mirrors tcp_write_stream (~line 961). */
    if (!stream || (stream->state != STREAM_CONNECTED && stream->state != STREAM_ACCEPTED)) {
        if (stream)
            tcp_stream_release(stream);
        tcp_set_error(TCP_ERR_INVALID_STREAM, "Stream not connected");
        return false;
    }

    sockfd = stream->sockfd;

    lockhandle(hdata);

    while (bytes_written < data_size) {
        fd_set writeset;
        struct timeval tv;
        int select_result;
        long this_chunk;
        ssize_t send_result;

        /* Calculate chunk size for this iteration */
        this_chunk = data_size - bytes_written;
        if (this_chunk > chunk_size)
            this_chunk = chunk_size;

        /* Wait for socket to be writable */
        FD_ZERO(&writeset);
        FD_SET(sockfd, &writeset);
        tv.tv_sec = timeout_secs;
        tv.tv_usec = 0;

        select_result = select(sockfd + 1, NULL, &writeset, NULL, &tv);

        if (select_result < 0) {
            if (errno == EINTR)
                continue;
            unlockhandle(hdata);
            tcp_stream_release(stream);
            tcp_set_error(TCP_ERR_SOCKET_ERROR, "select() failed");
            return false;
        }

        if (select_result == 0) {
            /* Timeout */
            unlockhandle(hdata);
            tcp_stream_release(stream);
            tcp_set_error(TCP_ERR_TIMEOUT, "Write timeout");
            return false;
        }

        /* Send chunk */
        send_result = send(sockfd, (*hdata) + bytes_written, this_chunk, 0);

        if (send_result < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                continue;
            if (errno == EINTR)
                continue;
            unlockhandle(hdata);
            tcp_stream_release(stream);
            tcp_set_error(TCP_ERR_SOCKET_ERROR, "send() failed");
            return false;
        }

        bytes_written += send_result;

        log_trace(LOG_COMP_LANG, "tcp_write_string_to_stream: wrote %zd bytes, total=%ld/%ld",
                  send_result, bytes_written, data_size);
    }

    unlockhandle(hdata);

    /* Update activity timestamp */
    TCP_LOCK();
    stream->last_activity = time(NULL);
    TCP_UNLOCK();

    tcp_stream_release(stream);

    log_debug(LOG_COMP_LANG, "tcp_write_string_to_stream: completed, wrote %ld bytes", bytes_written);

    return true;
}

/* tcp.writeFileToStream(stream, prefix, suffix, filespec) -> true
 * Write file contents to stream with optional prefix and suffix.
 *
 * Parameters:
 *   stream_id - Stream ID to write to
 *   hprefix   - Optional prefix data (may be NULL)
 *   hsuffix   - Optional suffix data (may be NULL)
 *   fs        - File specification to read from
 *
 * Returns:
 *   true if all data written, false on error
 */
boolean tcp_write_file_to_stream(long stream_id, Handle hprefix, Handle hsuffix, ptrfilespec fs) {
    tcp_stream_t *stream;
    int sockfd;
    hdlfilenum fnum;
    static const long kFileBufferSize = 32768L;
    char *buffer;
    boolean success = true;

    log_debug(LOG_COMP_LANG, "tcp_write_file_to_stream: stream_id=%ld", stream_id);

    (void)sockfd;  /* Reserved for future socket-level operations */

    /* Validate parameters */
    if (!fs) {
        tcp_set_error(TCP_ERR_SOCKET_ERROR, "NULL filespec");
        return false;
    }

    /* Acquire stream reference (TOCTOU protection) */
    stream = tcp_stream_acquire(stream_id);

    /* Issue #666: gate on stream state. Accept STREAM_CONNECTED /
     * STREAM_ACCEPTED only; reject STREAM_LISTENING / STREAM_CONNECTING
     * up-front so we don't attempt file I/O whose bytes can never flow
     * through this socket. Mirrors tcp_write_stream (~line 961). */
    if (!stream || (stream->state != STREAM_CONNECTED && stream->state != STREAM_ACCEPTED)) {
        if (stream)
            tcp_stream_release(stream);
        tcp_set_error(TCP_ERR_INVALID_STREAM, "Stream not connected");
        return false;
    }

    sockfd = stream->sockfd;

    /* Write prefix if provided */
    if (hprefix != nil && gethandlesize(hprefix) > 0) {
        if (!tcp_write_string_to_stream(stream_id, hprefix, 0, 0)) {
            tcp_stream_release(stream);
            return false;
        }
    }

    /* Allocate file buffer */
    buffer = (char *)malloc(kFileBufferSize);
    if (buffer == NULL) {
        tcp_stream_release(stream);
        tcp_set_error(TCP_ERR_NO_MEMORY, "Could not allocate file buffer");
        return false;
    }

    /* Open file */
    if (!openfile(fs, &fnum, true)) {
        free(buffer);
        tcp_stream_release(stream);
        /* openfile sets its own error */
        return false;
    }

    /* Read and transmit file in chunks */
    while (true) {
        long bytes_read = kFileBufferSize;

        if (!filereaddata(fnum, bytes_read, &bytes_read, buffer)) {
            success = false;
            break;
        }

        if (bytes_read == 0)
            break;  /* EOF */

        /* Create temporary handle for the chunk */
        Handle hchunk;
        if (!newfilledhandle(buffer, bytes_read, &hchunk)) {
            success = false;
            tcp_set_error(TCP_ERR_NO_MEMORY, "Could not allocate chunk handle");
            break;
        }

        /* Write chunk to stream */
        if (!tcp_write_string_to_stream(stream_id, hchunk, 0, 0)) {
            disposehandle(hchunk);
            success = false;
            break;
        }

        disposehandle(hchunk);
    }

    closefile(fnum);
    free(buffer);

    /* Write suffix if provided and no errors so far */
    if (success && hsuffix != nil && gethandlesize(hsuffix) > 0) {
        if (!tcp_write_string_to_stream(stream_id, hsuffix, 0, 0)) {
            tcp_stream_release(stream);
            return false;
        }
    }

    /* Update activity timestamp */
    TCP_LOCK();
    stream->last_activity = time(NULL);
    TCP_UNLOCK();

    tcp_stream_release(stream);

    log_debug(LOG_COMP_LANG, "tcp_write_file_to_stream: completed success=%d", success);

    return success;
}

/* tcp.getStats(listenID) -> stats
 * Get statistics for a listener (count of streams by state).
 *
 * Parameters:
 *   listener_id - Listener ID to get stats for
 *   stats_out   - Output: Statistics string (Pascal string)
 *
 * Returns:
 *   true on success, false if listener not found
 */
boolean tcp_get_stats(long listener_id, bigstring stats_out) {
    unsigned long ct_connected = 0;
    unsigned long ct_accepted = 0;
    unsigned long ct_closing = 0;
    unsigned long ct_closed = 0;
    char stats_cstr[256];

    log_debug(LOG_COMP_LANG, "tcp_get_stats: listener_id=%ld", listener_id);

    /* NULL pointer validation */
    if (!stats_out) {
        tcp_set_error(TCP_ERR_SOCKET_ERROR, "NULL output pointer");
        return false;
    }

    setemptystring(stats_out);

    /* Count streams associated with this listener */
    TCP_LOCK();

    for (int i = TCP_FIRST_STREAM_ID; i < TCP_MAX_STREAMS; i++) {
        tcp_stream_t *stream = &g_tcp_context.streams[i];

        if (stream->parent_listen_id == listener_id) {
            switch (stream->state) {
                case STREAM_CONNECTED:
                    ct_connected++;
                    break;
                case STREAM_ACCEPTED:
                    ct_accepted++;
                    break;
                case STREAM_CLOSING:
                    ct_closing++;
                    break;
                case STREAM_CLOSED:
                    ct_closed++;
                    break;
                default:
                    break;
            }
        }
    }

    TCP_UNLOCK();

    /* Format stats string */
    snprintf(stats_cstr, sizeof(stats_cstr),
             "connected=%lu,accepted=%lu,closing=%lu,closed=%lu",
             ct_connected, ct_accepted, ct_closing, ct_closed);

    copyctopstring(stats_cstr, stats_out);

    log_debug(LOG_COMP_LANG, "tcp_get_stats: %s", stats_cstr);

    return true;
}

/* ========================================================================
 * Shutdown and Cleanup
 * ======================================================================== */

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
