# TCP Architecture Documentation

This document explains key architectural decisions in the TCP Phase 1A/1B implementation.

## Stream Slot Design

### Why is Stream Slot 0 Reserved?

Stream slot 0 is reserved as a **sentinel/invalid value** to enable robust error detection.

**Rationale:**
1. **Error Detection**: Using 0 as "invalid" allows simple validation checks. Stream IDs returned to UserTalk are `long` values, and 0 serves as a natural error indicator (similar to NULL pointers in C).

2. **Frontier Convention**: Follows established pattern where resource IDs (file refs, handles, etc.) use 0 or negative values to indicate invalid/uninitialized state.

3. **Validation Simplification**: All validation functions use `if (stream_id < 1 || stream_id >= TCP_MAX_STREAMS)` without special-casing the lower bound.

4. **Future Expansion**: Reserving slot 0 leaves room for special semantics (e.g., broadcast stream) without breaking existing code.

**Code References:**
- `tcpverbs.h:66` - `TCP_FIRST_STREAM_ID` constant definition
- `tcpverbs.c:112` - Allocation loop starts at 1
- `tcpverbs.c:147` - Validation checks `stream_id < 1`

---

## Connection Lifecycle and Cleanup

### What Happens to Active Connections on Process Exit?

Active TCP connections are **explicitly closed** during shutdown, with the OS performing final cleanup.

**Normal Shutdown Sequence:**
1. `tcp_shutdown_context()` called during Frontier exit
2. Loops through all stream slots (1-255)
3. Closes each active socket with `close(sockfd)`
4. OS sends FIN packets for graceful shutdown
5. Marks all streams as `STREAM_INVALID`
6. Destroys mutex and condition variables

**Abnormal Termination (Crash/SIGKILL):**
1. `tcp_shutdown_context()` may not run
2. OS automatically reclaims all file descriptors
3. Peers receive RST packets (abrupt close)
4. No application cleanup, but no file descriptor leaks

**Two-Level Cleanup Strategy:**
- **Application Level**: Explicit socket close and resource cleanup
- **OS Level**: Automatic fd reclamation on process exit

**Code References:**
- `tcpverbs.c:894-920` - `tcp_shutdown_context()` implementation
- `tcpverbs.c:901-906` - Loop closing all active sockets
- `tcpverbs.c:556-593` - Graceful close pattern (FIN)
- `tcpverbs.c:597-634` - Immediate close pattern (RST)

---

## Stream Slot Reuse

### Are Stream Slots Reusable After Close?

**YES** - stream slots are fully reusable after close, with deferred cleanup for thread safety.

**Lifecycle State Machine:**
```
STREAM_INVALID → STREAM_CONNECTED → STREAM_CLOSING → STREAM_INVALID
                       ↑                                      ↓
                       └──────────── (slot reused) ──────────┘
```

**Two-Phase Close Protocol:**

**Phase 1: Close Request**
- `tcp_close_stream()` sets `state = STREAM_CLOSING`
- Socket fd closed immediately
- `active_count` decremented

**Phase 2: Deferred Cleanup**
- If `refcount == 0`: Slot marked `STREAM_INVALID` immediately (reusable)
- If `refcount > 0`: Cleanup deferred until last `tcp_stream_release()`
- This prevents use-after-free bugs

**Allocation Strategy:**
- `tcp_alloc_stream_id()` scans slots 1-255 for first `sockfd == -1`
- Any freed slot can be reused (no reservation or affinity)
- Slot reinitializes with `memset()` before reuse

**Thread Safety:**
```c
// ✅ SAFE - acquire/release prevents TOCTOU
stream = tcp_stream_acquire(stream_id);  // refcount++
if (stream) {
    /* Use stream - protected from close/reuse */
    tcp_stream_release(stream);          // refcount--, frees if CLOSING
}
```

**Code References:**
- `tcpverbs.c:111-123` - `tcp_alloc_stream_id()` finds reusable slots
- `tcpverbs.c:127-142` - `tcp_free_stream_id()` marks slots available
- `tcpverbs.c:556-593` - `tcp_close_stream()` deferred cleanup
- `tcpverbs.c:180-197` - `tcp_stream_release()` completes cleanup

**Safety Guarantees:**
- Slots NOT reused while `refcount > 0` (active operations in progress)
- Closed streams rejected by `tcp_get_stream()` validation
- Reference counting prevents use-after-free vulnerabilities

---

## Rate Limiting

### Connection Rate Limiting Design

**Purpose**: Prevent resource exhaustion attacks where malicious scripts rapidly exhaust all 256 stream slots.

**Algorithm**: Sliding window approach
- Tracks timestamps of recent connections in circular buffer (size 64)
- Counts connections in the last second
- Rejects new connections if count >= configured limit

**Default Limit**: 10 connections per second

**Configuration:**
- `TCP_RATE_LIMIT_WINDOW` (64) - Window size
- `TCP_DEFAULT_RATE_LIMIT` (10) - Default connections/sec

**Thread Safety**: All rate limit checks performed within `TCP_LOCK()` mutex

**Error Handling**: Returns `TCP_ERR_NO_FREE_STREAMS` with message "Connection rate limit exceeded"

**Code References:**
- `tcpverbs.h:69-70` - Rate limiting constants
- `tcpverbs.h:88-92` - Rate limiting fields in context
- `tcpverbs.c:249-281` - `tcp_check_rate_limit()` implementation
- `tcpverbs.c:354-361` - Rate limit check in `tcp_open_stream_addr()`
- `tcpverbs.c:754-761` - Rate limit check in `tcp_open_stream_name()`

---

## Logging Standards

### Logging Level Guidelines

The TCP implementation follows consistent logging patterns:

**log_debug()** - Detailed tracing and intermediate steps
- Function entry points
- Intermediate state transitions
- Connection progress details

**log_info()** - Significant lifecycle events
- Successful connection establishment
- Stream allocation/deallocation
- DNS resolution results
- Shutdown events

**log_warn()** - Warnings and fallback behaviors
- Rate limit exceeded
- DNS rebinding protection triggered
- fcntl() failures (non-fatal)
- Fallback to IP string on reverse DNS failure

**log_error()** - Critical errors (via `tcp_set_error()`)
- Connection failures
- Invalid parameters
- Resource exhaustion

**Examples:**
```c
// Connection lifecycle
log_debug(LOG_COMP_LANG, "tcp_open_stream_addr: addr=%ld port=%ld", ...);     // Entry
log_debug(LOG_COMP_LANG, "tcp_open_stream_addr: connected, allocating...");   // Progress
log_info(LOG_COMP_LANG, "tcp_open_stream_addr: stream_id=%d allocated", ...); // Success

// Rate limiting
log_warn(LOG_COMP_LANG, "tcp_check_rate_limit: rate limit exceeded...");      // Warning

// DNS rebinding protection
log_warn(LOG_COMP_LANG, "tcp_name_to_address: rejected private/reserved IP"); // Security
```

**References:** `docs/LOGGING_STANDARDS.md` for Frontier-wide logging requirements

---

## Security Features

### DNS Rebinding / SSRF Protection

**Purpose**: Prevent DNS rebinding attacks where malicious DNS returns private/internal IP addresses.

**Protected IP Ranges:**
- 0.0.0.0/8 (current network)
- 10.0.0.0/8 (private)
- 127.0.0.0/8 (loopback)
- 169.254.0.0/16 (link-local)
- 172.16.0.0/12 (private)
- 192.168.0.0/16 (private)
- 224.0.0.0/4 (multicast)
- 240.0.0.0/4 (reserved)

**Implementation:**
- `tcp_is_private_ip()` validates resolved IP addresses
- Applied after DNS resolution in `tcp_name_to_address()`
- Applied before connection in `tcp_open_stream_name()`
- Returns error if private/reserved IP detected

**Code References:**
- `tcpverbs.c:210-247` - `tcp_is_private_ip()` validation
- `tcpverbs.c:729-734` - Validation in `tcp_name_to_address()`
- `tcpverbs.c:832-837` - Validation in `tcp_open_stream_name()`

### TOCTOU Race Condition Protection

**Purpose**: Prevent Time-Of-Check-Time-Of-Use vulnerabilities in stream operations.

**Implementation**: Reference counting pattern
- `tcp_stream_acquire()` increments refcount while holding lock
- Operations use stream reference (not just stream_id)
- `tcp_stream_release()` decrements refcount after operation
- Stream cleanup deferred until refcount reaches 0

**Benefits:**
- Prevents use-after-free bugs
- Protects against double-close vulnerabilities
- Thread-safe stream operations

**Code References:**
- `tcpverbs.h:36` - `refcount` field in stream structure
- `tcpverbs.c:161-179` - `tcp_stream_acquire()` implementation
- `tcpverbs.c:180-197` - `tcp_stream_release()` implementation
- `tcpverbs.c:418-554` - Usage in `tcp_read_stream()` and `tcp_write_stream()`

---

## Future Enhancements

### Phase 2 Considerations
- Configurable timeouts for DNS and I/O operations
- Persistent non-blocking mode with `select()/poll()`
- `MSG_NOSIGNAL` to prevent SIGPIPE
- Per-stream write timeout detection

### Phase 3 Requirements
- Per-stream locking for higher concurrency
- `SO_REUSEPORT` for multi-threaded listeners
- Lock-free reference counting optimization
- Thread lifecycle management (graceful shutdown, join, cancellation)
- Migration from global `g_tcp_context` to thread-local storage

**Phase 3 Blockers:**
- Current global mutex will create contention bottleneck
- Global state incompatible with Frontier's collaborative editing vision
- Requires architectural refactoring per `docs/THREAD_LOCAL_GLOBALS_PATTERN.md`

---

**Document Version**: 1.0
**Last Updated**: 2026-01-20
**PR**: #327 (TCP Phase 1A/1B Implementation)
