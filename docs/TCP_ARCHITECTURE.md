# TCP Architecture Documentation

This document explains key architectural decisions in the TCP Phase 1A/1B/3 implementation.

**Status**: Phase 1A, 1B, and 3 complete (as of 2026-01-24)
**PRs**: #327 (Phase 1A), #330 (Phase 1B + Phase 3)
**Last Updated**: 2026-01-24

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

## Testing Strategy

### Test Suite Organization

TCP tests are organized into separate suites based on network dependencies:

**Local Tests (Always Run)**:
- `tests/integration/test_cases/tcp_verbs.yaml` - 27 local-only tests (as of 2026-01-24)
- No external network connectivity required
- Tests error handling, address encoding/decoding, parameter validation
- Safe for CI/CD environments with restricted network access
- Run automatically in all test execution contexts

**Network Tests (Opt-In)**:
- `tests/integration/test_cases/tcp_verbs_network.yaml` - 20 network-dependent tests (as of 2026-01-24)
- Require external connectivity (connect to example.com:80)
- Run manually via `FRONTIER_RUN_NETWORK_TESTS=1 make test-integration`
- Validate actual TCP connectivity and protocol behavior
- Not run by default to avoid test fragility

**Server Operation Tests** (Phase 3):
- `tests/integration/test_cases/tcp_server_verbs.yaml` - 37 tests for server operations
- `tests/integration/test_cases/tcp_client_verbs.yaml` - 27 tests for client operations
- Tests for `tcp.listenStream()`, `tcp.closeListen()` callback infrastructure

### Testing Progression (Phase 1 → Phase 3)

**Phase 1A/1B (Complete)**: External dependency tests
- Network tests connect to `example.com:80` for validation
- Tests are opt-in and skipped by default
- Enables manual verification of TCP implementation

**Phase 2 (Planned)**: Buffered I/O with external dependencies
- Continue pattern of separate network test suite
- Add tests for `tcp.readStreamUntil`, `tcp.readStreamBytes`, etc.
- Remain opt-in via `FRONTIER_RUN_NETWORK_TESTS=1`

**Phase 3 (Complete as of PR #330)**: Self-contained deterministic tests
- `tcp.listenStream()` and `tcp.closeListen()` implemented
- Tests can launch Frontier-based test server within integration test harness
- Client tests can connect to localhost instead of external servers
- Enables fully deterministic and CI/CD-friendly network tests
- No external dependencies required for server operation testing

**Example Phase 3 Self-Contained Test**:
```yaml
# Future test pattern using tcp.listenStream (Phase 3)
tests:
  - name: "tcp.openStream - connect to local test server"
    setup_script: |
      # Start test server on localhost:8080
      on serverHandler(stream, refcon) {
        local(data = tcp.readStream(stream, 1024));
        tcp.writeStream(stream, "Echo: " + data);
        tcp.closeStream(stream)
      };
      tcp.listenStream(8080, 5, @serverHandler)

    script: |
      # Client connects to test server
      local(stream = tcp.openAddrStream("127.0.0.1", 8080));
      tcp.writeStream(stream, "Hello");
      local(response = tcp.readStream(stream, 1024));
      tcp.closeStream(stream);
      return response == "Echo: Hello"

    expected_success: true
    expected_result: "true"
```

**Why Phase 3 Self-Contained Tests Are Superior**:
- No dependency on external internet connectivity
- Deterministic behavior (no DNS flakiness, no network timeouts)
- Complete control over test server responses
- Can test error conditions by simulating server failures
- Safe for air-gapped CI/CD environments
- Tests run at full speed without network latency

### Running Network Tests

```bash
# Run all tests (local tests only, network tests skipped)
cd tests && make test-integration

# Run with network tests enabled (manual verification)
FRONTIER_RUN_NETWORK_TESTS=1 cd tests && make test-integration
```

**See also**: `planning/phase4/networking/IMPLEMENTATION_PLAN.md` - Phase 2/3 testing roadmap

---

## Completed Phases

### Phase 1A: Core Socket Operations (PR #327)
- 6 fundamental TCP verbs for client connectivity
- Thread-safe with mutex protection
- Rate limiting and security features (SSRF protection)

### Phase 1B: Address Encoding (PR #330)
- 5 public address handling verbs
- DNS forward and reverse lookup
- IP address encoding/decoding

### Phase 3: Server Operations (PR #330)
- `tcp.listenStream()` - Accept connections with callback dispatch
- `tcp.closeListen()` - Stop listening on port
- Listener registry with per-listener accept threads
- Callback infrastructure via `langruncallbackwithparams()`
- **Self-contained integration tests** now possible using localhost servers

**Current Status**: 11 TCP verbs implemented and tested (Phase 1A: 6, Phase 1B: 5, Phase 3: 3 server operations)

---

## Future Enhancements

### Phase 2 Considerations (Planned)
- Configurable timeouts for DNS and I/O operations
- Persistent non-blocking mode with `select()/poll()`
- `MSG_NOSIGNAL` to prevent SIGPIPE
- Per-stream write timeout detection
- Buffered I/O operations (`tcp.readStreamUntil`, `tcp.readStreamBytes`)

### Phase 4+ Requirements (Long-term)
- Per-stream locking for higher concurrency
- `SO_REUSEPORT` for multi-threaded listeners
- Lock-free reference counting optimization
- Thread lifecycle management (graceful shutdown, join, cancellation)
- Migration from global `g_tcp_context` to thread-local storage

**Phase 4+ Considerations:**
- Current global mutex may create contention bottleneck at scale
- Global state migration needed for full collaborative editing support
- Requires architectural refactoring per `docs/THREAD_LOCAL_GLOBALS_PATTERN.md`

---

**Document Version**: 1.2
**Last Updated**: 2026-01-24
**PRs**: #327 (TCP Phase 1A), #330 (TCP Phase 1B + Phase 3)
