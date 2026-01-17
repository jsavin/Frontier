# POSIX Networking Architecture for Frontier CLI

**Target Platform**: macOS (POSIX-compliant, BSD sockets)
**Future**: Linux support (same POSIX API)
**Windows**: Separate implementation using existing WinSockNetEvents.c patterns

**Date**: 2026-01-16
**Status**: Architecture design for implementation

---

## Executive Summary

This document defines the POSIX-compliant networking architecture for Frontier CLI. The design prioritizes:

1. **Thread Safety**: Eliminate global mutable state for collaborative ODB
2. **Simplicity**: Start with blocking I/O, add async patterns only if needed
3. **POSIX Compliance**: Use standard BSD sockets API
4. **Modern DNS**: Use `getaddrinfo()` instead of deprecated `gethostbyname()`
5. **Resource Safety**: Proper cleanup, no connection leaks
6. **Test-Driven**: Each phase independently testable

---

## 1. Core Data Structures

### 1.1 Stream Record (Connection State)

```c
// Common/headers/tcpverbs.h

typedef enum {
    STREAM_INVALID     = -1,  // Uninitialized slot
    STREAM_CONNECTING  =  0,  // Connection in progress
    STREAM_CONNECTED   =  1,  // Active client connection
    STREAM_LISTENING   =  2,  // Server listen socket
    STREAM_ACCEPTED    =  3,  // Accepted server connection
    STREAM_CLOSING     =  4,  // Graceful shutdown in progress
    STREAM_CLOSED      =  5   // Closed (slot can be reused)
} stream_state_t;

typedef struct tcp_stream {
    int             sockfd;        // POSIX socket file descriptor (-1 if unused)
    stream_state_t  state;         // Current stream state
    uint16_t        local_port;    // Local port (host byte order)
    uint16_t        remote_port;   // Remote port (host byte order)
    uint32_t        remote_addr;   // Remote IPv4 address (host byte order)

    // For listen sockets only
    bigstring       callback;      // UserTalk callback script name
    Handle          callback_tree; // Pre-compiled callback tree
    long            refcon;        // User data for callback
    long            max_depth;     // Listen backlog (max queued connections)
    long            listen_id;     // Unique listen socket ID

    // For accepted connections (from listen socket)
    long            parent_listen_id;  // Listen socket that accepted this connection

    // Threading (for listen callbacks)
    pthread_t       accept_thread; // Thread handle (0 if not threaded)
    boolean         thread_active; // Thread is running

    // Timestamps (for debugging/monitoring)
    time_t          created_at;    // When stream was created
    time_t          last_activity; // Last read/write timestamp
} tcp_stream_t;
```

### 1.2 TCP Context (Per-Process State)

```c
// Common/headers/tcpverbs.h

#define TCP_MAX_STREAMS 256  // Maximum concurrent connections

typedef struct tcp_context {
    // Stream table
    tcp_stream_t    streams[TCP_MAX_STREAMS];
    int             active_count;      // Number of active streams

    // Thread safety
    pthread_mutex_t mutex;             // Protects entire context
    pthread_cond_t  activity_cond;     // Signals stream activity

    // Listen socket tracking
    int             listen_count;      // Number of active listeners
    long            next_listen_id;    // Monotonic listen ID generator

    // Configuration
    int             default_timeout_sec;  // Default operation timeout
    boolean         initialized;          // Context has been initialized

    // Statistics (for monitoring)
    long            total_connections;    // Lifetime connection count
    long            failed_connections;   // Lifetime failure count
} tcp_context_t;
```

### 1.3 Global Context Instance

```c
// Common/source/tcpverbs.c

static tcp_context_t g_tcp_context;  // Single global instance

#define TCP_LOCK()   pthread_mutex_lock(&g_tcp_context.mutex)
#define TCP_UNLOCK() pthread_mutex_unlock(&g_tcp_context.mutex)
```

**Note**: While this is still a global, it's **protected by mutex** and will be refactored to thread-local or explicit parameter passing in future phases.

---

## 2. Socket Management

### 2.1 Stream ID Allocation

```c
// Internal function: Find free stream slot
// Must be called with TCP_LOCK() held
static int tcp_alloc_stream_id(void) {
    for (int i = 1; i < TCP_MAX_STREAMS; i++) {  // Start at 1 (0 is invalid)
        if (g_tcp_context.streams[i].sockfd == -1) {
            // Initialize stream record
            memset(&g_tcp_context.streams[i], 0, sizeof(tcp_stream_t));
            g_tcp_context.streams[i].sockfd = -1;  // Will be set by caller
            g_tcp_context.streams[i].state = STREAM_INVALID;
            g_tcp_context.streams[i].created_at = time(NULL);
            return i;
        }
    }
    return -1;  // No free slots
}

// Internal function: Free stream slot
// Must be called with TCP_LOCK() held
static void tcp_free_stream_id(int stream_id) {
    if (stream_id < 1 || stream_id >= TCP_MAX_STREAMS)
        return;

    tcp_stream_t *stream = &g_tcp_context.streams[stream_id];

    // Close socket if still open
    if (stream->sockfd >= 0) {
        close(stream->sockfd);
        stream->sockfd = -1;
    }

    // Mark slot as free
    stream->state = STREAM_INVALID;
    g_tcp_context.active_count--;
}
```

### 2.2 Stream Lookup and Validation

```c
// Validate stream ID and return pointer to stream record
// Must be called with TCP_LOCK() held
static tcp_stream_t* tcp_get_stream(int stream_id) {
    if (stream_id < 1 || stream_id >= TCP_MAX_STREAMS)
        return NULL;

    tcp_stream_t *stream = &g_tcp_context.streams[stream_id];

    if (stream->state == STREAM_INVALID || stream->sockfd < 0)
        return NULL;

    return stream;
}
```

---

## 3. Core Operations

### 3.1 Connection Establishment

#### tcp.openNameStream(hostname, port)

```c
// Common/source/tcpverbs.c

boolean tcp_open_stream_name(bigstring hostname, long port, long *stream_id_out) {
    struct addrinfo hints, *result, *rp;
    int sockfd = -1;
    int stream_id = -1;
    char hostname_cstr[256];
    char port_str[16];

    // Convert Pascal string to C string
    nullterminate(hostname);
    copyptocstring(hostname, hostname_cstr);
    snprintf(port_str, sizeof(port_str), "%ld", port);

    // Setup hints for getaddrinfo
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;        // IPv4 only for now
    hints.ai_socktype = SOCK_STREAM;  // TCP
    hints.ai_protocol = IPPROTO_TCP;

    // DNS resolution
    if (getaddrinfo(hostname_cstr, port_str, &hints, &result) != 0) {
        // DNS lookup failed
        return false;
    }

    // Try each address until we successfully connect
    for (rp = result; rp != NULL; rp = rp->ai_next) {
        sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sockfd == -1)
            continue;

        // Set socket options
        int optval = 1;
        setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

        // Attempt connection (blocking)
        if (connect(sockfd, rp->ai_addr, rp->ai_addrlen) == 0) {
            // Success!
            break;
        }

        close(sockfd);
        sockfd = -1;
    }

    freeaddrinfo(result);

    if (sockfd == -1) {
        // All connection attempts failed
        return false;
    }

    // Allocate stream ID
    TCP_LOCK();
    stream_id = tcp_alloc_stream_id();
    if (stream_id < 0) {
        TCP_UNLOCK();
        close(sockfd);
        return false;  // No free stream slots
    }

    // Initialize stream record
    tcp_stream_t *stream = &g_tcp_context.streams[stream_id];
    stream->sockfd = sockfd;
    stream->state = STREAM_CONNECTED;
    stream->remote_port = (uint16_t)port;
    // TODO: Extract remote_addr from connected socket

    g_tcp_context.active_count++;
    TCP_UNLOCK();

    *stream_id_out = stream_id;
    return true;
}
```

#### tcp.openAddrStream(addr, port)

```c
boolean tcp_open_stream_addr(long addr, long port, long *stream_id_out) {
    struct sockaddr_in server_addr;
    int sockfd;
    int stream_id;

    // Create socket
    sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sockfd < 0)
        return false;

    // Setup address structure
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons((uint16_t)port);
    server_addr.sin_addr.s_addr = htonl((uint32_t)addr);  // Convert to network byte order

    // Connect (blocking)
    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        close(sockfd);
        return false;
    }

    // Allocate stream ID
    TCP_LOCK();
    stream_id = tcp_alloc_stream_id();
    if (stream_id < 0) {
        TCP_UNLOCK();
        close(sockfd);
        return false;
    }

    // Initialize stream record
    tcp_stream_t *stream = &g_tcp_context.streams[stream_id];
    stream->sockfd = sockfd;
    stream->state = STREAM_CONNECTED;
    stream->remote_addr = (uint32_t)addr;
    stream->remote_port = (uint16_t)port;

    g_tcp_context.active_count++;
    TCP_UNLOCK();

    *stream_id_out = stream_id;
    return true;
}
```

### 3.2 Data Transfer

#### tcp.readStream(stream, bytesToRead)

```c
boolean tcp_read_stream(long stream_id, long bytes_to_read, Handle *data_out) {
    tcp_stream_t *stream;
    Handle hdata;
    ssize_t bytes_read;
    char *buffer;

    TCP_LOCK();
    stream = tcp_get_stream(stream_id);
    if (!stream || stream->state != STREAM_CONNECTED) {
        TCP_UNLOCK();
        return false;
    }

    int sockfd = stream->sockfd;
    TCP_UNLOCK();

    // Allocate buffer
    if (!newhandle(bytes_to_read, &hdata))
        return false;

    lockhandle(hdata);
    buffer = *hdata;

    // Read from socket (may return less than requested)
    bytes_read = recv(sockfd, buffer, bytes_to_read, 0);

    unlockhandle(hdata);

    if (bytes_read < 0) {
        // Error
        disposehandle(hdata);
        return false;
    }

    if (bytes_read == 0) {
        // Connection closed
        disposehandle(hdata);
        *data_out = NULL;  // Return empty handle
        return true;
    }

    // Resize handle to actual bytes read
    sethandlesize(hdata, bytes_read);

    // Update activity timestamp
    TCP_LOCK();
    stream = tcp_get_stream(stream_id);
    if (stream)
        stream->last_activity = time(NULL);
    TCP_UNLOCK();

    *data_out = hdata;
    return true;
}
```

#### tcp.writeStream(stream, data)

```c
boolean tcp_write_stream(long stream_id, Handle hdata) {
    tcp_stream_t *stream;
    ssize_t bytes_written;
    long data_size;
    char *buffer;

    TCP_LOCK();
    stream = tcp_get_stream(stream_id);
    if (!stream || stream->state != STREAM_CONNECTED) {
        TCP_UNLOCK();
        return false;
    }

    int sockfd = stream->sockfd;
    TCP_UNLOCK();

    data_size = gethandlesize(hdata);
    if (data_size == 0)
        return true;  // Nothing to write

    lockhandle(hdata);
    buffer = *hdata;

    // Write all data (loop until complete)
    long total_written = 0;
    while (total_written < data_size) {
        bytes_written = send(sockfd, buffer + total_written,
                            data_size - total_written, 0);

        if (bytes_written < 0) {
            unlockhandle(hdata);
            return false;  // Error
        }

        total_written += bytes_written;
    }

    unlockhandle(hdata);

    // Update activity timestamp
    TCP_LOCK();
    stream = tcp_get_stream(stream_id);
    if (stream)
        stream->last_activity = time(NULL);
    TCP_UNLOCK();

    return true;
}
```

### 3.3 Connection Teardown

#### tcp.closeStream(stream)

```c
boolean tcp_close_stream(long stream_id) {
    tcp_stream_t *stream;
    int sockfd;

    TCP_LOCK();
    stream = tcp_get_stream(stream_id);
    if (!stream) {
        TCP_UNLOCK();
        return false;
    }

    sockfd = stream->sockfd;
    stream->state = STREAM_CLOSING;
    TCP_UNLOCK();

    // Graceful shutdown
    shutdown(sockfd, SHUT_RDWR);  // Send FIN
    close(sockfd);

    // Free stream record
    TCP_LOCK();
    tcp_free_stream_id(stream_id);
    TCP_UNLOCK();

    return true;
}
```

#### tcp.abortStream(stream)

```c
boolean tcp_abort_stream(long stream_id) {
    tcp_stream_t *stream;
    int sockfd;

    TCP_LOCK();
    stream = tcp_get_stream(stream_id);
    if (!stream) {
        TCP_UNLOCK();
        return false;
    }

    sockfd = stream->sockfd;

    // Set SO_LINGER to 0 for immediate RST
    struct linger linger_opt = {1, 0};  // on, timeout=0
    setsockopt(sockfd, SOL_SOCKET, SO_LINGER, &linger_opt, sizeof(linger_opt));

    close(sockfd);  // Sends RST

    tcp_free_stream_id(stream_id);
    TCP_UNLOCK();

    return true;
}
```

---

## 4. DNS and Address Operations

### 4.1 DNS Resolution

#### tcp.nameToAddress(domainName)

```c
boolean tcp_name_to_address(bigstring domain_name, long *addr_out) {
    struct addrinfo hints, *result;
    char hostname_cstr[256];

    nullterminate(domain_name);
    copyptocstring(domain_name, hostname_cstr);

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(hostname_cstr, NULL, &hints, &result) != 0)
        return false;

    // Get first IPv4 address
    struct sockaddr_in *addr = (struct sockaddr_in*)result->ai_addr;
    *addr_out = (long)ntohl(addr->sin_addr.s_addr);  // Convert to host byte order

    freeaddrinfo(result);
    return true;
}
```

#### tcp.addressToName(addr)

```c
boolean tcp_address_to_name(long addr, bigstring name_out) {
    struct sockaddr_in sa;
    char hostname[NI_MAXHOST];

    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = htonl((uint32_t)addr);

    if (getnameinfo((struct sockaddr*)&sa, sizeof(sa),
                    hostname, sizeof(hostname),
                    NULL, 0, 0) != 0) {
        // Reverse lookup failed, return IP as string
        tcp_address_decode(addr, name_out);
        return true;
    }

    copyctopstring(hostname, name_out);
    return true;
}
```

### 4.2 Address Conversion (No Network I/O)

#### tcp.addressEncode(ipString)

```c
boolean tcp_address_encode(bigstring ip_string, long *addr_out) {
    char ip_cstr[16];
    struct in_addr addr;

    nullterminate(ip_string);
    copyptocstring(ip_string, ip_cstr);

    if (inet_pton(AF_INET, ip_cstr, &addr) != 1)
        return false;

    *addr_out = (long)ntohl(addr.s_addr);
    return true;
}
```

#### tcp.addressDecode(addr)

```c
boolean tcp_address_decode(long addr, bigstring ip_string_out) {
    struct in_addr in_addr;
    char ip_cstr[INET_ADDRSTRLEN];

    in_addr.s_addr = htonl((uint32_t)addr);

    if (inet_ntop(AF_INET, &in_addr, ip_cstr, sizeof(ip_cstr)) == NULL)
        return false;

    copyctopstring(ip_cstr, ip_string_out);
    return true;
}
```

---

## 5. Server Operations (Listen/Accept)

### 5.1 Listen Socket

#### tcp.listenStream(port, depth, callback, refcon, addr)

**This is the most complex verb - requires threading and callback execution.**

```c
boolean tcp_listen_stream(long port, long depth, bigstring callback,
                          long refcon, long bind_addr, long *listen_id_out) {
    int sockfd;
    struct sockaddr_in server_addr;
    int stream_id;
    pthread_t accept_thread;

    // Create socket
    sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sockfd < 0)
        return false;

    // Set SO_REUSEADDR to allow quick restart
    int optval = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    // Bind to address/port
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons((uint16_t)port);
    server_addr.sin_addr.s_addr = (bind_addr == 0) ? INADDR_ANY : htonl((uint32_t)bind_addr);

    if (bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        close(sockfd);
        return false;
    }

    // Start listening
    if (listen(sockfd, (int)depth) < 0) {
        close(sockfd);
        return false;
    }

    // Allocate stream ID
    TCP_LOCK();
    stream_id = tcp_alloc_stream_id();
    if (stream_id < 0) {
        TCP_UNLOCK();
        close(sockfd);
        return false;
    }

    // Initialize listen stream record
    tcp_stream_t *stream = &g_tcp_context.streams[stream_id];
    stream->sockfd = sockfd;
    stream->state = STREAM_LISTENING;
    stream->local_port = (uint16_t)port;
    stream->max_depth = depth;
    stream->refcon = refcon;
    stream->listen_id = g_tcp_context.next_listen_id++;
    copystring(callback, stream->callback);

    // TODO: Pre-compile callback tree
    // stream->callback_tree = compile_usertalk_script(callback);

    g_tcp_context.listen_count++;
    g_tcp_context.active_count++;

    long listen_id = stream->listen_id;
    TCP_UNLOCK();

    // Start accept thread
    pthread_create(&accept_thread, NULL, tcp_accept_thread_proc, (void*)(intptr_t)stream_id);

    TCP_LOCK();
    stream->accept_thread = accept_thread;
    stream->thread_active = true;
    TCP_UNLOCK();

    *listen_id_out = listen_id;
    return true;
}
```

### 5.2 Accept Thread

```c
static void* tcp_accept_thread_proc(void *arg) {
    int listen_stream_id = (int)(intptr_t)arg;
    tcp_stream_t *listen_stream;
    int listen_sockfd;
    bigstring callback;
    long refcon;

    // Get listen socket info
    TCP_LOCK();
    listen_stream = tcp_get_stream(listen_stream_id);
    if (!listen_stream || listen_stream->state != STREAM_LISTENING) {
        TCP_UNLOCK();
        return NULL;
    }
    listen_sockfd = listen_stream->sockfd;
    copystring(listen_stream->callback, callback);
    refcon = listen_stream->refcon;
    TCP_UNLOCK();

    // Accept loop
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        // Accept connection (blocking)
        int client_sockfd = accept(listen_sockfd, (struct sockaddr*)&client_addr, &client_len);

        if (client_sockfd < 0) {
            // Error or listener closed
            break;
        }

        // Allocate stream ID for accepted connection
        TCP_LOCK();
        int client_stream_id = tcp_alloc_stream_id();
        if (client_stream_id < 0) {
            // No free slots - reject connection
            TCP_UNLOCK();
            close(client_sockfd);
            continue;
        }

        // Initialize accepted stream record
        tcp_stream_t *client_stream = &g_tcp_context.streams[client_stream_id];
        client_stream->sockfd = client_sockfd;
        client_stream->state = STREAM_ACCEPTED;
        client_stream->remote_addr = ntohl(client_addr.sin_addr.s_addr);
        client_stream->remote_port = ntohs(client_addr.sin_port);
        client_stream->parent_listen_id = listen_stream->listen_id;

        g_tcp_context.active_count++;
        TCP_UNLOCK();

        // Invoke UserTalk callback
        // TODO: Execute callback script with parameters (client_stream_id, refcon)
        // For now, just log
        printf("Accepted connection %d from %u.%u.%u.%u:%u\\n",
               client_stream_id,
               (client_stream->remote_addr >> 24) & 0xFF,
               (client_stream->remote_addr >> 16) & 0xFF,
               (client_stream->remote_addr >> 8) & 0xFF,
               client_stream->remote_addr & 0xFF,
               client_stream->remote_port);
    }

    return NULL;
}
```

---

## 6. Error Handling

### 6.1 Error Code Mapping

```c
// Common/headers/tcpverbs.h

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

// Map POSIX errno to tcp_error_t
static tcp_error_t tcp_map_errno(int err) {
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
```

### 6.2 Error Reporting to UserTalk

```c
// Set script error with descriptive message
static void tcp_set_error(tcp_error_t err, const char *detail) {
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
        // ... other cases
    }

    langerrormessage(error_msg);
}
```

---

## 7. Verb Registration

### 7.1 Verb Enum Definition

```c
// Common/source/tcpverbs.c

typedef enum {
    tcp_openNameStreamfunc,
    tcp_openAddrStreamfunc,
    tcp_readStreamfunc,
    tcp_writeStreamfunc,
    tcp_closeStreamfunc,
    tcp_abortStreamfunc,
    tcp_listenStreamfunc,
    tcp_closeListenfunc,
    tcp_nameToAddressfunc,
    tcp_addressToNamefunc,
    tcp_addressEncodefunc,
    tcp_addressDecodefunc,
    tcp_myAddressfunc,
    tcp_getPeerAddressfunc,
    tcp_getPeerPortfunc,
    tcp_countConnectionsfunc,
    tcp_statusStreamfunc,
    tcp_readStreamUntilfunc,
    tcp_readStreamBytesfunc,
    tcp_readStreamUntilClosedfunc,
    tcp_writeStringToStreamfunc,
    tcp_writeFileToStreamfunc,

    tcp_ctverbs
} tcp_verb_token;
```

### 7.2 Verb Function Dispatcher

```c
boolean tcpfunctionvalue(short token, hdltreenode hparam1,
                         tyvaluerecord *vreturned, bigstring bserror) {
    hdltreenode hp1 = hparam1;
    tyvaluerecord *v = vreturned;

    setbooleanvalue(false, v);  // Default return value

    switch (token) {
        case tcp_openNameStreamfunc: {
            bigstring hostname;
            long port;
            long stream_id;

            if (!getstringvalue(hp1, 1, hostname))
                return false;

            flnextparamislast = true;
            if (!getlongvalue(hp1, 2, &port))
                return false;

            if (!tcp_open_stream_name(hostname, port, &stream_id)) {
                tcp_set_error(TCP_ERR_CONNECTION_FAILED, "Could not connect");
                return false;
            }

            return setlongvalue(stream_id, v);
        }

        case tcp_readStreamfunc: {
            long stream_id;
            long bytes_to_read;
            Handle hdata;

            if (!getlongvalue(hp1, 1, &stream_id))
                return false;

            flnextparamislast = true;
            if (!getlongvalue(hp1, 2, &bytes_to_read))
                return false;

            if (!tcp_read_stream(stream_id, bytes_to_read, &hdata)) {
                tcp_set_error(TCP_ERR_INVALID_STREAM, "Read failed");
                return false;
            }

            return setheapvalue(hdata, stringvaluetype, v);
        }

        // ... other cases
    }

    return false;
}
```

### 7.3 Initialization

```c
// Common/source/tcpverbs.c

boolean tcpinitverbs(void) {
    // Initialize TCP context
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

    // Register verb function processor
    if (!loadfunctionprocessor(idtcpverbs, &tcpfunctionvalue))
        return false;

    return true;
}

// Add to Common/headers/kernelverbdefs.h:
#define idtcpverbs 1028
```

---

## 8. Build System Integration

### 8.1 Source Files

```
Common/source/tcpverbs.c        - Verb implementations
Common/headers/tcpverbs.h       - Public API and data structures
```

### 8.2 Makefile Changes

```makefile
# Add to Common/Makefile

SOURCES += Common/source/tcpverbs.c

# Link pthread library (already included)
LDFLAGS += -lpthread
```

### 8.3 Header Includes

```c
// Common/source/tcpverbs.c

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>
#include <errno.h>
#include <unistd.h>

#include "frontier.h"
#include "standard.h"
#include "tcpverbs.h"
#include "langexternal.h"
#include "langinternal.h"
```

---

## 9. Testing Strategy

### 9.1 Unit Tests (C-level)

```c
// tests/headless_tcp_tests.c

static boolean test_tcp_address_encode_decode(void) {
    bigstring ip_str;
    long encoded;
    bigstring decoded;

    copyctopstring("192.168.1.1", ip_str);

    if (!tcp_address_encode(ip_str, &encoded))
        return false;

    if (!tcp_address_decode(encoded, decoded))
        return false;

    return equalstrings(ip_str, decoded);
}

static boolean test_tcp_open_close_stream(void) {
    long stream_id;
    bigstring hostname;

    copyctopstring("www.google.com", hostname);

    if (!tcp_open_stream_name(hostname, 80, &stream_id))
        return false;

    if (!tcp_close_stream(stream_id))
        return false;

    return true;
}
```

### 9.2 Integration Tests (UserTalk)

```yaml
# tests/integration/tcp_tests.yaml

- name: "tcp.addressEncode and tcp.addressDecode"
  script: |
    local (ip = "192.168.1.1");
    local (encoded = tcp.addressEncode(ip));
    local (decoded = tcp.addressDecode(encoded));
    return decoded == ip
  expected_success: true
  expected_result: true

- name: "tcp.openStream and tcp.closeStream - Google"
  script: |
    local (stream = tcp.openStream("www.google.com", 80));
    tcp.closeStream(stream);
    return true
  expected_success: true
  expected_result: true
```

---

## 10. Performance Considerations

### 10.1 Connection Pooling

**Current Design**: No connection pooling. Each `tcp.openStream` creates new socket.

**Future Optimization**: Implement connection pool for HTTP keep-alive:
```c
// Keep pool of idle connections per (host, port)
// Reuse connections instead of connect/close cycle
```

### 10.2 Non-Blocking I/O

**Current Design**: Blocking I/O with threads for listen sockets.

**Future Optimization**: Use `select()`/`poll()`/`kqueue()` for async I/O:
```c
// Event loop for non-blocking operations
// Avoids thread-per-connection overhead
```

### 10.3 Buffer Management

**Current Design**: Allocate new Handle for each read.

**Future Optimization**: Reuse buffer pool to reduce allocations.

---

## 11. Migration from Existing Code

### 11.1 WinSockNetEvents.c Extraction

Extract GUSI_2 (POSIX) code paths from WinSockNetEvents.c as reference:
- Socket creation and connection logic
- Error handling patterns
- Thread management for accept

### 11.2 Platform Abstraction

```c
// Common/headers/tcpverbs.h

#ifdef _WIN32
    // Use WinSockNetEvents.c implementation
    #include "winsocknetevents.h"
#else
    // Use POSIX implementation (this architecture)
    #include "tcpverbs.h"
#endif
```

---

## 12. Open Questions for User

1. **Timeout Handling**: Should timeouts be per-operation or global default?
2. **IPv6 Support**: Should Phase 1 support IPv6 or IPv4-only?
3. **Event Loop**: Does Frontier CLI already have an event loop we should integrate with?
4. **Callback Execution**: How should UserTalk callbacks be invoked from C threads? (Need langinternal expertise)
5. **Error Granularity**: How detailed should error messages be? (Current plan: descriptive strings)

---

## 13. Next Steps

1. Implement Phase 1A (core socket operations) - **Haiku can do this**
2. Write unit tests for address encoding/decoding - **Haiku**
3. Write integration test for simple HTTP GET - **Haiku**
4. Implement Phase 1B (DNS operations) - **Haiku**
5. Implement Phase 2 (buffered I/O) - **Sonnet** (complex timeout logic)
6. Implement Phase 3 (listen/accept with threading) - **Sonnet** (complex threading + callbacks)
