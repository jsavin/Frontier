# Existing TCP Implementation Notes

**Source**: Analysis of `Common/source/WinSockNetEvents.c` (3722 lines)
**Date**: 2026-01-16

## Key Data Structures

### Socket Record (Connection State)

```c
typedef struct tysockRecord {
    SOCKET          sockID;              // OS socket file descriptor
    tysocktypeid    typeID;              // Socket state (see below)
    long            refcon;              // User data for callbacks
    bigstring       callback;            // UserTalk callback script name
    long            maxdepth;            // For listen sockets: max queue depth
    long            listenReference;     // For accepted connections: parent listener ID
    long            currentListenDepth;  // Current number of queued connections
    boolean         flNotification;      // Notification flag
    #ifdef ACCEPT_CONN_WITHOUT_GLOBALS
        Handle      hcallbacktree;       // Compiled callback tree
    #endif
    #ifdef ACCEPT_IN_SEPARATE_THREAD
        long                idthread;    // Thread ID for accept thread
        hdldatabaserecord   hdatabase;   // Database context for thread
    #endif
} sockRecord;
```

### Socket States

```c
#define SOCKTYPE_INVALID      -1  // Invalid/uninitialized
#define SOCKTYPE_UNKNOWN       0  // Unknown state
#define SOCKTYPE_OPEN          1  // Connected client socket
#define SOCKTYPE_DATA          2  // Socket with data available
#define SOCKTYPE_LISTENING     3  // Listening server socket
#define SOCKTYPE_CLOSED        4  // Closed socket
#define SOCKTYPE_LISTENSTOPPED 5  // Listener stopped
#define SOCKTYPE_INACTIVE      6  // Inactive socket slot
```

### Connection Management

```c
#define FRONTIER_MAX_STREAM 256  // Maximum concurrent connections

static sockRecord sockstack[FRONTIER_MAX_STREAM];  // Global connection table
static short frontierWinSockCount = 0;             // Number of active connections
static boolean frontierWinSockLoaded = false;      // TCP subsystem initialized

static short sockListenCount = 0;                  // Number of listeners
static short sockListenList[FRONTIER_MAX_STREAM];  // Listener index list
```

## Platform Support

The file supports both:
1. **Windows**: WinSock 1.1/2.0 API
2. **Mac**: GUSI (Grand Unified Socket Interface) - both GUSI and GUSI_2

### POSIX Compatibility Layer (Mac)

```c
#ifdef FRONTIER_GUSI_2
    #include <netdb.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <sys/socket.h>
    #include <pthread.h>

    // Error code mappings
    #define WSAEWOULDBLOCK EAGAIN
    #define WSAENOTCONN ENOTCONN
    #define WSAETIMEDOUT ETIMEDOUT
    #define WSAECONNABORTED ECONNABORTED
    #define WSAGetLastError() (errno == EINTR? userCanceledErr : errno)

    // Socket operation wrappers
    #define closesocket(foo) close(foo)
    #define ioctlsocket(d,request,argp) ioctl(d,request,argp)
    typedef int SOCKET;
#endif
```

## Error Handling

80+ predefined error strings in `tcperrorstrings[]` array, covering:
- DNS errors (host not found, etc.)
- Connection errors (timeout, refused, reset)
- Socket errors (would block, not connected, etc.)
- Network errors (unreachable, down, etc.)

## Threading Model

### Listener Callbacks (Mac/GUSI_2)

```c
#define ACCEPT_IN_SEPARATE_THREAD  1  // Accept connections in dedicated thread
```

When `ACCEPT_IN_SEPARATE_THREAD` is defined:
- Listen socket creates separate pthread for each incoming connection
- Thread stores: `idthread` (thread ID), `hdatabase` (database context)
- Callback runs in isolated thread context

### Critical Sections

```c
#define _entercriticalsockstacksection()  // No-op in current implementation
#define _leavecriticalsockstacksection()  // No-op in current implementation
```

**CRITICAL FOR POSIX**: These need real mutex implementation for thread safety.

## Debug/Logging Infrastructure

Configurable TCP tracker with 3 levels:
```c
#define TCPTRACKER 1  // Output to about window
#define TCPTRACKER 2  // Error output to tcpfile.txt
#define TCPTRACKER 3  // Full output to tcpfile.txt
```

Tracks:
- Function entry/exit
- Stream ID, socket FD, type, depth, refcon
- Timing information (ticks between calls)
- Thread ID for multi-threaded operations

## Key Observations

### 1. Stream ID Management
- Stream IDs are indices into `sockstack[]` array (1-255)
- 0 is invalid, FRONTIER_MAX_STREAM-1 is max
- Linear search for free slots when opening new connection

### 2. Global State Issues
- `sockstack[]` is global mutable array
- `frontierWinSockCount` is global counter
- **NOT THREAD-SAFE** without critical sections
- Need refactoring for collaborative ODB requirements

### 3. Callback Mechanism
- UserTalk callback script name stored as Pascal string
- Callback tree can be pre-compiled (`hcallbacktree`)
- Callbacks run in separate threads (Mac) or synchronously (Windows)

### 4. Platform Abstraction
- Already has POSIX layer for Mac (GUSI)
- Error codes mapped to POSIX errno
- Socket operations wrapped with defines

## Questions for Architecture Design

1. **Event Loop**: How does existing code handle async I/O? Select/poll/event-based?
2. **Timeout Implementation**: How are read/write timeouts implemented?
3. **DNS Resolution**: Uses `gethostbyname()` or `getaddrinfo()`?
4. **Buffer Management**: How are read/write buffers allocated and managed?
5. **Connection Cleanup**: When are closed sockets removed from sockstack[]?

## Next Steps for POSIX Implementation

1. Extract the GUSI_2 code paths as reference implementation
2. Implement proper mutex protection for sockstack[]
3. Design event loop integration (select/poll/epoll)
4. Modernize DNS resolution to getaddrinfo()
5. Eliminate global mutable state (move to context struct)
6. Implement proper buffer management (avoid static buffers)
