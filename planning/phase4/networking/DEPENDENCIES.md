# TCP Networking Dependencies

**Date**: 2026-01-16
**Status**: Analysis complete

---

## System Dependencies

### POSIX Headers Required

```c
#include <sys/socket.h>    // socket(), connect(), bind(), listen(), accept()
#include <netinet/in.h>    // sockaddr_in, INADDR_ANY, htons/htonl
#include <arpa/inet.h>     // inet_pton(), inet_ntop()
#include <netdb.h>         // getaddrinfo(), getnameinfo(), freeaddrinfo()
#include <unistd.h>        // close(), read(), write()
#include <errno.h>         // errno, EINTR, EAGAIN, etc.
#include <pthread.h>       // pthread_create(), pthread_mutex_*
#include <sys/select.h>    // select(), fd_set (for timeouts)
#include <fcntl.h>         // fcntl(), O_NONBLOCK (optional)
#include <sys/ioctl.h>     // ioctl(), FIONREAD (for statusStream)
```

### Library Linkage

```makefile
# Already linked in most POSIX systems
LDFLAGS += -lpthread

# No additional libraries required for BSD sockets
# (socket functions are in libc on macOS/Linux)
```

### Platform Availability

| Header | macOS | Linux | FreeBSD | Notes |
|--------|-------|-------|---------|-------|
| `<sys/socket.h>` | ✅ | ✅ | ✅ | Core socket API |
| `<netdb.h>` | ✅ | ✅ | ✅ | DNS resolution |
| `<pthread.h>` | ✅ | ✅ | ✅ | Threading |
| `<sys/select.h>` | ✅ | ✅ | ✅ | I/O multiplexing |

---

## Frontier Internal Dependencies

### Required Headers

```c
#include "frontier.h"      // Core definitions
#include "standard.h"      // boolean, bigstring, etc.
#include "memory.h"        // Handle management
#include "langinternal.h"  // Script error handling
#include "langexternal.h"  // Verb registration
```

### Required Functions

| Function | Header | Purpose |
|----------|--------|---------|
| `loadfunctionprocessor()` | langexternal.h | Register verb processor |
| `getstringvalue()` | langinternal.h | Extract string params |
| `getlongvalue()` | langinternal.h | Extract long params |
| `setlongvalue()` | langinternal.h | Return long value |
| `setbooleanvalue()` | langinternal.h | Return boolean value |
| `setheapvalue()` | langinternal.h | Return handle as string |
| `newhandle()` | memory.h | Allocate memory handle |
| `disposehandle()` | memory.h | Free memory handle |
| `lockhandle()` | memory.h | Lock handle for access |
| `unlockhandle()` | memory.h | Unlock handle |
| `copyctopstring()` | standard.h | C string → Pascal string |
| `copyptocstring()` | standard.h | Pascal string → C string |
| `langerrormessage()` | langinternal.h | Set script error |

### Required Constants

```c
// Add to Common/headers/kernelverbdefs.h
#define idtcpverbs 1028  // Unique verb processor ID
```

---

## Build System Changes

### CMakeLists.txt

```cmake
# Add to source list
set(COMMON_SOURCES
    ...
    Common/source/tcpverbs.c
)

# Pthread should already be linked, but verify:
find_package(Threads REQUIRED)
target_link_libraries(frontier-cli Threads::Threads)
```

### Makefile (if not using CMake)

```makefile
# Add source file
COMMON_SOURCES += Common/source/tcpverbs.c

# Link pthread (usually already present)
LDFLAGS += -lpthread
```

---

## Threading Infrastructure Dependency (Phase 3)

### Required for `tcp.listenStream`

Phase 3 (server operations) requires the ability to:

1. **Create worker threads** for accept loop
2. **Execute UserTalk scripts from C threads**
3. **Maintain thread-local database context**

### Current State (from WinSockNetEvents.c analysis)

```c
#ifdef ACCEPT_IN_SEPARATE_THREAD
    long                idthread;     // Thread ID
    hdldatabaserecord   hdatabase;    // Thread-local DB context
#endif
```

### Required Infrastructure

| Component | Status | Notes |
|-----------|--------|-------|
| pthread_create/join | ✅ Available | Standard POSIX |
| Thread-local globals | ⚠️ Check tythreadglobals | May need additions |
| Script execution from C | ⚠️ Investigate | `langrunscript()` or similar |
| Database context per thread | ⚠️ Investigate | `hdldatabaserecord` handling |

### Recommendation

Before Phase 3 implementation:
1. Audit existing threading code in Frontier
2. Document how to execute UserTalk from worker threads
3. Verify thread-local database context pattern

---

## Test Infrastructure Dependencies

### Network Access

Integration tests require:
- Outbound TCP connections (port 80, 443)
- DNS resolution
- Optional: localhost listener for server tests

### Test Targets

Reliable external hosts for testing:
- `httpbin.org` - HTTP testing service
- `dns.google` - Reliable DNS host (8.8.8.8)
- `example.com` - Stable test endpoint

### Sandbox Restrictions

⚠️ **macOS Sandbox Note**: frontier-cli runs in sandbox. Verify:
- Outbound socket connections allowed
- No `/tmp` file restrictions for test artifacts (use `tests/tmp/`)

---

## Windows Future Support

### Existing Code Reference

`Common/source/WinSockNetEvents.c` contains Windows implementation:

```c
#ifdef WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    // WinSock-specific implementation
#endif
```

### Platform Abstraction Strategy

```c
// Common/headers/tcpverbs.h

#ifdef _WIN32
    // Windows: Use existing WinSockNetEvents patterns
    #include "tcpverbs_win.h"
#else
    // POSIX: New implementation
    typedef int SOCKET;
    #define INVALID_SOCKET (-1)
    #define closesocket(s) close(s)
#endif
```

### Windows-specific Requirements

- Link `ws2_32.lib` (WinSock2)
- Call `WSAStartup()` / `WSACleanup()`
- Use `SOCKET` type instead of `int`
- Map WinSock error codes

---

## Documentation Dependencies

### Files to Update

| File | Change |
|------|--------|
| `docs/VERB_IMPLEMENTATION_GUIDE.md` | Add TCP verb examples |
| `docs/usertalk/docserver/tcp/` | Verify accuracy |
| `CLAUDE.md` | Add TCP testing notes |

### New Documentation

| File | Content |
|------|---------|
| `docs/TCP_VERBS.md` | Usage guide for TCP verbs |
| `planning/phase4/networking/` | This planning folder |

---

## Summary Checklist

### Before Phase 1

- [ ] Verify `pthread` linked in build
- [ ] Confirm `idtcpverbs` constant available
- [ ] Review `loadfunctionprocessor()` usage pattern
- [ ] Create `tests/tmp/` directory for test artifacts
- [ ] Verify outbound network access in test environment

### Before Phase 3

- [ ] Document thread-local database context pattern
- [ ] Implement/verify `langrunscript()` from worker threads
- [ ] Add threading-related fields to `tcp_stream_t` if needed
- [ ] Design thread cleanup on listener close

### For Windows Support (Future)

- [ ] Extract platform-agnostic code to shared header
- [ ] Create `tcpverbs_posix.c` and `tcpverbs_win.c`
- [ ] Add Windows CI build target
- [ ] Test with existing WinSockNetEvents patterns
