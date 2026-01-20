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
#define TCP_MAX_READ_BYTES (16*1024*1024)  /* 16MB max read size */
#define TCP_FIRST_STREAM_ID 1         /* Stream IDs start at 1 (0 reserved) */
#define MAX_HOSTNAME_LEN 255          /* Maximum DNS hostname length */
#define MAX_IPV4_STRING_LEN 15        /* "xxx.xxx.xxx.xxx" max length */

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

    /* Statistics (for monitoring) */
    long            total_connections;    /* Lifetime connection count */
    long            failed_connections;   /* Lifetime failure count */
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
boolean tcp_address_encode(bigstring ip_string, long *addr_out);
boolean tcp_address_decode(long addr, bigstring ip_string_out);

/* Phase 3: Server Operations (Listen/Accept) */
boolean tcp_listen_stream(long port, long depth, bigstring callback,
                          long refcon, long bind_addr, long *listen_id_out);
boolean tcp_close_listen(long listen_id);

/* Initialization and Shutdown */
boolean tcp_init_context(void);
boolean tcp_shutdown_context(void);

/* Internal Helpers (not exposed to UserTalk) */
tcp_error_t tcp_map_errno(int err);
void tcp_set_error(tcp_error_t err, const char *detail);

#endif /* __TCPVERBS_H__ */
