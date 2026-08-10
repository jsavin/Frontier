/* tcpverbs.h - POSIX TCP networking API for Frontier CLI
 *
 * Phase 1A: Core TCP socket operations (blocking I/O, client-side)
 * Phase 1B: DNS operations (name resolution)
 * Phase 2: Buffered I/O and timeouts
 * Phase 3: Server operations (listen/accept with threading)
 *
 * Platform: macOS/Linux (POSIX-compliant BSD sockets)
 * Thread Safety: Protected by mutex for Phase 1
 *
 * Date: 2026-01-20
 */

#ifndef __TCPVERBS_H__
#define __TCPVERBS_H__

#include <pthread.h>
#include <stdint.h>
#include <time.h>

#ifndef __FRONTIER_H__
#include "frontier.h"
#endif

/* Forward declarations */
struct tyhashtable;
typedef struct tyhashtable **hdlhashtable;
struct tyfilespec;
typedef struct tyfilespec *ptrfilespec;

/* Stream States */
typedef enum {
    STREAM_INVALID     = -1,  /* Uninitialized slot */
    STREAM_CONNECTING  =  0,  /* Connection in progress */
    STREAM_CONNECTED   =  1,  /* Active client connection */
    STREAM_LISTENING   =  2,  /* Server listen socket (Phase 3) */
    STREAM_ACCEPTED    =  3,  /* Accepted server connection (Phase 3) */
    STREAM_CLOSING     =  4,  /* Graceful shutdown in progress */
    STREAM_CLOSED      =  5   /* Closed (slot can be reused) */
} stream_state_t;

/* TCP Stream Record (Connection State) */
typedef struct tcp_stream {
    /* Socket */
    int             sockfd;        /* POSIX socket file descriptor (-1 if unused) */
    stream_state_t  state;         /* Current stream state */
    int             refcount;      /* Reference count for TOCTOU protection */
    unsigned long   generation;    /* Slot-reuse counter: incremented on every
                                    * allocation and preserved across the slot
                                    * memset. Code that resumes after releasing
                                    * the GIL must treat (slot, generation) as
                                    * the stream identity: states and fd numbers
                                    * get recycled, generations do not. */

    /* Address info */
    uint16_t        local_port;    /* Local port (host byte order) */
    uint16_t        remote_port;   /* Remote port (host byte order) */
    uint32_t        remote_addr;   /* Remote IPv4 address (host byte order) */

    /* Listen socket fields (Phase 3) */
    bigstring       callback;      /* UserTalk callback script name */
    Handle          callback_tree; /* Pre-compiled callback tree */
    long            refcon;        /* User data for callback */
    long            max_depth;     /* Listen backlog (max queued connections) */
    long            listen_id;     /* Unique listen socket ID */

    /* Accepted connection fields (Phase 3) */
    long            parent_listen_id;  /* Listen socket that accepted this */

    /* Threading (Phase 3) */
    pthread_t       accept_thread; /* Thread handle (0 if not threaded) */
    boolean         thread_active; /* Thread is running */

    /* Timestamps (for debugging/monitoring) */
    time_t          created_at;    /* When stream was created */
    time_t          last_activity; /* Last read/write timestamp */
} tcp_stream_t;

/* Configuration Constants */
#define TCP_MAX_STREAMS 256           /* Maximum concurrent connections */
#define TCP_MAX_LISTENERS 32          /* Maximum concurrent listen sockets (Phase 3) */
#define TCP_MAX_READ_BYTES (16*1024*1024)  /* 16MB max read size */
#define TCP_FIRST_STREAM_ID 1         /* Stream IDs start at 1 (0 reserved) */
#define MAX_HOSTNAME_LEN 255          /* Maximum DNS hostname length */
#define MAX_IPV4_STRING_LEN 15        /* "xxx.xxx.xxx.xxx" max length */
#define TCP_RATE_LIMIT_WINDOW 64      /* Max connection timestamps to track */
#define TCP_DEFAULT_RATE_LIMIT 10     /* Default connections per second */

/* TCP Context (Per-Process State) */
typedef struct tcp_context {
    /* Stream table */
    tcp_stream_t    streams[TCP_MAX_STREAMS];
    int             active_count;      /* Number of active streams */

    /* Thread safety */
    pthread_mutex_t mutex;             /* Protects entire context */
    pthread_cond_t  activity_cond;     /* Signals stream activity */

    /* Listen socket tracking (Phase 3) */
    int             listen_count;      /* Number of active listeners */
    long            next_listen_id;    /* Monotonic listen ID generator */

    /* Configuration */
    int             default_timeout_sec;  /* Default operation timeout */
    boolean         initialized;          /* Context has been initialized */

    /* Rate Limiting (SECURITY FIX Issue #7: High-resolution timestamps to prevent burst attacks) */
    uint64_t        connection_timestamps_us[TCP_RATE_LIMIT_WINDOW];  /* Sliding window (microseconds) */
    int             timestamp_write_pos;   /* Next position to write in circular buffer */
    int             connections_per_sec;   /* Maximum connections per second (configurable) */

    /* Statistics (for monitoring) */
    long            total_connections;    /* Lifetime connection count */
    long            failed_connections;   /* Lifetime failure count */
    long            rate_limited_count;   /* Connections rejected due to rate limit */
} tcp_context_t;

/* Error Codes */
typedef enum {
    TCP_ERR_SUCCESS           = 0,
    TCP_ERR_NO_MEMORY         = 1,
    TCP_ERR_INVALID_STREAM    = 2,
    TCP_ERR_CONNECTION_FAILED = 3,
    TCP_ERR_TIMEOUT           = 4,
    TCP_ERR_DNS_FAILED        = 5,
    TCP_ERR_SOCKET_ERROR      = 6,
    TCP_ERR_NO_FREE_STREAMS   = 7,
    TCP_ERR_ALREADY_CLOSED    = 8,
    TCP_ERR_CANCELLED         = 9,  /* Thread killed while parked in a GIL yield */
} tcp_error_t;

/* Function Prototypes - Verb Implementations */

/* Phase 1A: Core Socket Operations */
boolean tcp_open_stream_addr(long addr, long port, long *stream_id_out);
boolean tcp_read_stream(long stream_id, long bytes_to_read, Handle *data_out);
boolean tcp_write_stream(long stream_id, Handle hdata);
boolean tcp_close_stream(long stream_id);
boolean tcp_abort_stream(long stream_id);
long tcp_count_connections(void);

/* Phase 1B: DNS and Address Operations */
boolean tcp_open_stream_name(bigstring hostname, long port, long *stream_id_out);
boolean tcp_name_to_address(bigstring domain_name, long *addr_out);
boolean tcp_address_to_name(long addr, bigstring name_out);
/* #716 item 5: testable helper -- packs a resolved C-string hostname into
   the bigstring, falling back to dotted-decimal IP if the hostname exceeds
   the 255-byte bigstring limit. Returns true if the hostname fit, false if
   the IP fallback was used. */
boolean tcp_address_to_name_pack(long addr, const char *hostname, bigstring name_out);
boolean tcp_address_encode(bigstring ip_string, long *addr_out);
boolean tcp_address_decode(long addr, bigstring ip_string_out);

/* Phase 3: Server Operations (Listen/Accept) */
boolean tcp_listen_stream(long port, long depth, hdlhashtable callback_htable,
                          bigstring callback_name, long refcon, long bind_addr,
                          long *listen_id_out);
boolean tcp_close_listen(long listen_id);

/* Phase 2: Status and Peer Information */
boolean tcp_status_stream(long stream_id, bigstring status_out, long *bytes_pending_out);
boolean tcp_get_peer_address(long stream_id, long *addr_out);
boolean tcp_get_peer_port(long stream_id, long *port_out);
boolean tcp_my_address(long *addr_out);

/* Phase 3: Buffered I/O with Timeouts */
boolean tcp_read_stream_until(long stream_id, Handle hbuffer, Handle hpattern, long timeout_secs);
boolean tcp_read_stream_bytes(long stream_id, Handle hbuffer, long count, long timeout_secs);
boolean tcp_read_stream_until_closed(long stream_id, Handle hbuffer, long timeout_secs);
boolean tcp_read_stream_inetd(long stream_id, Handle hbuffer, long timeout_secs);
boolean tcp_write_string_to_stream(long stream_id, Handle hdata, long chunk_size, long timeout_secs);
boolean tcp_write_file_to_stream(long stream_id, Handle hprefix, Handle hsuffix, ptrfilespec fs);
boolean tcp_get_stats(long listener_id, bigstring stats_out);

/* Initialization and Shutdown */
boolean tcp_init_context(void);
boolean tcp_shutdown_context(void);

/* Reset the tcp connection rate-limit sliding window. Called by
 * frontier-cli's script/clearContext handler so the long-lived protocol
 * subprocess does not accumulate cross-test rate-limit pressure. See
 * tcpverbs.c for the full contract. Issue #130. */
void tcp_reset_rate_limit_window(void);

/* Callback Queue Processing
 * Must be called periodically from main thread to process TCP callbacks.
 * Each callback is enqueued as a one-shot process via newprocess/addprocess.
 * Returns number of callbacks dequeued (includes failed enqueue attempts). */
int tcp_process_callbacks(void);

/* Internal Helpers (not exposed to UserTalk) */
tcp_error_t tcp_map_errno(int err);
void tcp_set_error(tcp_error_t err, const char *detail);
tcp_stream_t* tcp_stream_acquire(int stream_id);
void tcp_stream_release(tcp_stream_t *stream);
boolean tcp_is_private_ip(uint32_t addr);

#ifdef FRONTIER_TESTS
/* Test-only hooks for pinning the slot allocator and post-yield
 * revalidation invariants without network connectivity or timing
 * dependence (tests/tcp_phase1a_unit_tests.c). Compiled only into the
 * unit-test binaries (-DFRONTIER_TESTS); not present in frontier-cli. */
int tcp_test_alloc_stream_id(void);              /* TCP_LOCK-wrapped alloc */
tcp_stream_t *tcp_test_slot(int stream_id);      /* raw slot, no validation */
boolean tcp_test_revalidate(tcp_stream_t *stream, unsigned long generation);
boolean tcp_test_revalidate_or_error(tcp_stream_t *stream, unsigned long generation);
void tcp_test_record_error(tcp_error_t err, const char *msg);
tcp_error_t tcp_test_last_error_code(void);
const char *tcp_test_last_error_message(void);
#endif

#endif /* __TCPVERBS_H__ */
