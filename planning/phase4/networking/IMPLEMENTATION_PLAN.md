# TCP Networking Implementation Plan

**Date**: 2026-01-16
**Status**: Ready for execution
**Process**: Follow `/doit` workflow (TDD + sub-agents + PR + monitoring)

---

## Executive Summary

This plan implements POSIX-compliant TCP networking for Frontier CLI in **4 phases**, each with its own PR cycle. The implementation enables the existing UserTalk HTTP client/server scripts to work in the CLI.

**Total Scope**: 22 kernel verbs in C
**Estimated PRs**: 4-5 (one per phase, plus potential dependency PR)

---

## Testing Strategy and Progression

### Phase 1-2: External Dependencies (Current)

**Problem**: Testing TCP networking requires actual network connections, but external dependencies create fragility:
- DNS resolution failures
- Network timeouts and latency
- Firewall restrictions in CI/CD
- Tests fail for reasons unrelated to code changes

**Solution**: Split tests into two suites:

**Suite 1: Local Tests** (`tcp_verbs.yaml`)
- 13 tests with no external network dependencies
- Tests error handling, address encoding/decoding, parameter validation
- Always run in all test contexts (CI/CD, air-gapped environments)
- Fast, deterministic, reliable

**Suite 2: Network Tests** (`tcp_verbs_network.yaml`)
- 9 tests requiring external connectivity (connect to example.com:80)
- Validate actual TCP connectivity and protocol behavior
- Opt-in via `FRONTIER_RUN_NETWORK_TESTS=1 make test-integration`
- Skipped by default to avoid test fragility
- Used for manual verification during development

### Phase 3: Self-Contained Deterministic Tests (Future)

Once `tcp.listenStream()` is implemented, network tests will be **migrated** to become self-contained:

**Pattern**:
1. Launch Frontier-based test server within integration test harness
2. Client tests connect to localhost (127.0.0.1) instead of external servers
3. Tests become fully deterministic with zero external dependencies
4. Network tests migrate from `tcp_verbs_network.yaml` to `tcp_verbs.yaml`
5. Eventually deprecate `tcp_verbs_network.yaml` (no longer needed)

**Example self-contained test**:
```yaml
- name: "HTTP GET request - self-contained"
  setup_script: |
    # Start test HTTP server on localhost
    on httpHandler(stream, refcon) {
      local(request = tcp.readStream(stream, 1024));
      tcp.writeStream(stream, "HTTP/1.0 200 OK\r\n\r\nOK");
      tcp.closeStream(stream)
    };
    tcp.listenStream(8080, 5, @httpHandler, 0)

  script: |
    # Client connects to test server (localhost)
    local (stream = tcp.openAddrStream("127.0.0.1", 8080));
    tcp.writeStream(stream, "GET / HTTP/1.0\r\n\r\n");
    local (response = tcp.readStream(stream, 1024));
    tcp.closeStream(stream);
    return response contains "OK"
  expected_success: true
```

**Why Phase 3 Self-Contained Tests Are Superior**:
- **Zero external dependencies** - No DNS, no internet required
- **Fully deterministic** - No network timeouts, DNS failures, or connection errors
- **Complete control** - Test server responses are under test control
- **Error simulation** - Can simulate server crashes, malformed responses, etc.
- **CI/CD friendly** - Works in air-gapped environments, behind firewalls
- **Performance** - Tests run at full speed without network latency
- **Reliability** - Tests never fail due to external service outages

### Testing Progression Summary

| Phase | Test Strategy | External Deps | CI/CD Safe | Notes |
|-------|---------------|---------------|------------|-------|
| **Phase 1-2** | Split suites (local + network) | Yes (opt-in) | Yes (local only) | Network tests skipped by default |
| **Phase 3** | Self-contained (localhost server) | No | Yes (all tests) | Migrate network tests to localhost |
| **Result** | All tests deterministic | No | Yes | Deprecate external dependency tests |

**See also**: `docs/TCP_ARCHITECTURE.md` - Testing Strategy section

---

## Critical Dependencies

### Blocking Dependency: Thread Callback Infrastructure

**`tcp.listenStream` requires Frontier's threading/agent infrastructure** to execute UserTalk callbacks in separate threads for each accepted connection.

From docserver documentation:
> "tcp.listenStream returns immediately and calls the callback script **asynchronously in separate threads** for each incoming connection."

**Required Infrastructure**:
1. **Thread-local database context** (`hdldatabaserecord hdatabase` per thread)
2. **Script execution from C thread** (invoke UserTalk callback with parameters)
3. **Thread-safe global state** (protect `sockstack[]` with mutex)

**Options**:
- **Option A**: Implement Phase 3 (listen/accept) after threading infrastructure exists
- **Option B**: Create separate "Threading Foundation" workstream first
- **Option C**: Implement blocking listen-in-main-thread first, add async later

**Recommendation**: Proceed with Phases 1-2 immediately (client-side networking). These have no threading dependency. Phase 3 can wait for threading infrastructure or be implemented with Option C as interim solution.

---

## Phase Overview

| Phase | Scope | Verbs | Dependency | Model |
|-------|-------|-------|------------|-------|
| **Phase 1A** | Core Socket Primitives | 7 | None | 🟢 Haiku |
| **Phase 1B** | DNS/Address Operations | 6 | None | 🟢 Haiku |
| **Phase 2** | Buffered I/O | 4 | Phase 1 | 🟡 Sonnet |
| **Phase 3** | Server Operations | 3 | Threading infra | 🟡 Sonnet |
| **Phase 4** | Advanced Features | 2 | Phase 1-2 | 🟢 Haiku |

---

## Phase 1A: Core Socket Primitives

**Goal**: Basic TCP client connectivity
**PR Scope**: Create new files, implement 7 verbs, unit tests
**Model**: 🟢 **Haiku** - Pattern-following implementation

### Tasks

#### 1.1 Create File Structure
- 🟢 **Haiku**: Create `Common/headers/tcpverbs.h`
- 🟢 **Haiku**: Create `Common/source/tcpverbs.c`
- 🟢 **Haiku**: Update CMakeLists.txt / Makefile

#### 1.2 Implement Data Structures
- 🟢 **Haiku**: Define `tcp_stream_t` struct
- 🟢 **Haiku**: Define `tcp_context_t` struct
- 🟢 **Haiku**: Implement stream ID allocation/deallocation
- 🟢 **Haiku**: Implement stream lookup/validation

#### 1.3 Implement Core Verbs

| Verb | Signature | Notes |
|------|-----------|-------|
| `tcp.openNameStream` | `(hostname, port) → streamID` | DNS + connect |
| `tcp.openAddrStream` | `(addr, port) → streamID` | IP + connect |
| `tcp.readStream` | `(stream, bytes) → data` | Non-blocking recv |
| `tcp.writeStream` | `(stream, data) → true` | Blocking send |
| `tcp.closeStream` | `(stream) → true` | Graceful shutdown |
| `tcp.abortStream` | `(stream) → true` | Immediate RST |
| `tcp.countConnections` | `() → count` | Active stream count |

**Implementation per verb** (🟢 Haiku):
1. Add enum value to `tcp_verb_token`
2. Implement C function (`tcp_open_stream_name`, etc.)
3. Add case in `tcpfunctionvalue()` dispatcher
4. Write unit test
5. Write integration test

#### 1.4 Register Verb Processor
- 🟢 **Haiku**: Define `idtcpverbs` constant
- 🟢 **Haiku**: Implement `tcpinitverbs()`
- 🟢 **Haiku**: Register in verb initialization chain

### Tests (TDD - Write First)

**Test Organization Strategy**:

Phase 1A/1B tests are split into two files:
1. `tcp_verbs.yaml` - Local tests only (always run)
2. `tcp_verbs_network.yaml` - Network tests (opt-in via `FRONTIER_RUN_NETWORK_TESTS=1`)

**Local Tests (No External Network)**:
```yaml
# tests/integration/test_cases/tcp_verbs.yaml

- name: "tcp.addressEncode - valid IP address"
  script: |
    local(encoded = tcp.addressEncode("192.168.1.1"));
    return encoded != 0
  expected_success: true
  expected_result: "true"

- name: "tcp.addressDecode - roundtrip"
  script: |
    local(encoded = tcp.addressEncode("192.168.1.1"));
    local(decoded = tcp.addressDecode(encoded));
    return decoded == "192.168.1.1"
  expected_success: true
  expected_result: "true"

- name: "tcp.openAddrStream - invalid stream ID"
  script: |
    tcp.closeStream(-1)
  expected_success: false
  expected_error_type: "script_error"
```

**Network Tests (External Connectivity Required)**:
```yaml
# tests/integration/test_cases/tcp_verbs_network.yaml
# Run with: FRONTIER_RUN_NETWORK_TESTS=1 make test-integration

- name: "tcp.openStream - basic connectivity"
  script: |
    local (stream = tcp.openStream("example.com", 80));
    tcp.closeStream(stream);
    return stream > 0
  expected_success: true
  expected_result: "true"
  skip: "Requires external network connectivity"

- name: "tcp.readStream - after HTTP request"
  script: |
    local (stream = tcp.openStream("example.com", 80));
    tcp.writeStream(stream, "GET / HTTP/1.0\r\nHost: example.com\r\n\r\n");
    local (response = tcp.readStream(stream, 1024));
    tcp.closeStream(stream);
    return sizeOf(response) > 0
  expected_success: true
  expected_result: "true"
  skip: "Requires external network connectivity"

- name: "tcp.countConnections"
  script: |
    local (before = tcp.countConnections());
    local (stream = tcp.openStream("example.com", 80));
    local (during = tcp.countConnections());
    tcp.closeStream(stream);
    local (after = tcp.countConnections());
    return during == before + 1 and after == before
  expected_success: true
  expected_result: "true"
  skip: "Requires external network connectivity"
```

**Why Split Tests?**:
- Local tests run in all contexts (CI/CD, air-gapped environments)
- Network tests are opt-in for manual verification
- Avoids test fragility from DNS/network issues
- Enables faster test cycles during development

### /doit Process

```
1. Create worktree: git worktree add ../Frontier-tcp-phase1a -b feature/tcp-phase1a
2. Write integration tests (TDD)
3. Implement tcpverbs.h/tcpverbs.c
4. Run unit tests: ./tools/run_headless_tests.sh
5. Run integration tests: cd tests && make test-integration
6. Create PR via pull-request agent
7. Monitor: ./tools/monitor_pr_review_bg.sh <PR_NUMBER>
```

### Definition of Done
- [ ] All 7 verbs implemented and tested
- [ ] Unit tests pass
- [ ] Integration tests pass
- [ ] Can make basic HTTP request using primitives
- [ ] PR merged to develop

---

## Phase 1B: DNS/Address Operations

**Goal**: DNS resolution and address conversion utilities
**PR Scope**: Add 6 verbs to existing tcpverbs.c
**Model**: 🟢 **Haiku** - Straightforward implementation

### Tasks

#### 1B.1 Implement Address Conversion (No Network I/O)

| Verb | Signature | Notes |
|------|-----------|-------|
| `tcp.addressEncode` | `(ipString) → addr` | "192.168.1.1" → long |
| `tcp.addressDecode` | `(addr) → ipString` | long → "192.168.1.1" |

- 🟢 **Haiku**: Use `inet_pton()` / `inet_ntop()`
- 🟢 **Haiku**: Handle host/network byte order correctly

#### 1B.2 Implement DNS Operations

| Verb | Signature | Notes |
|------|-----------|-------|
| `tcp.nameToAddress` | `(hostname) → addr` | DNS A record lookup |
| `tcp.addressToName` | `(addr) → hostname` | Reverse DNS (PTR) |
| `tcp.myAddress` | `() → addr` | Local machine IP |

- 🟢 **Haiku**: Use modern `getaddrinfo()` / `getnameinfo()`
- 🟢 **Haiku**: Handle DNS failure gracefully

#### 1B.3 Implement Peer Information

| Verb | Signature | Notes |
|------|-----------|-------|
| `tcp.getPeerAddress` | `(stream) → addr` | Remote IP of connection |
| `tcp.getPeerPort` | `(stream) → port` | Remote port of connection |

- 🟢 **Haiku**: Use `getpeername()` on socket

### Tests

**Local Tests** (added to `tcp_verbs.yaml`):
```yaml
- name: "tcp.addressEncode and addressDecode roundtrip"
  script: |
    local (ip = "192.168.1.1");
    local (encoded = tcp.addressEncode(ip));
    local (decoded = tcp.addressDecode(encoded));
    return decoded == ip
  expected_success: true
  expected_result: "true"

- name: "tcp.myAddress returns valid address"
  script: |
    local (addr = tcp.myAddress());
    local (dotted = tcp.addressDecode(addr));
    return sizeOf(dotted) > 6  // "0.0.0.0" is 7 chars
  expected_success: true
  expected_result: "true"
```

**Network Tests** (added to `tcp_verbs_network.yaml`):
```yaml
- name: "tcp.nameToAddress resolves known host"
  script: |
    local (addr = tcp.nameToAddress("example.com"));
    return addr != 0
  expected_success: true
  expected_result: "true"
  skip: "Requires external network connectivity"

- name: "tcp.addressToName reverse DNS lookup"
  script: |
    local (addr = tcp.nameToAddress("example.com"));
    local (name = tcp.addressToName(addr));
    return sizeOf(name) > 0
  expected_success: true
  expected_result: "true"
  skip: "Requires external network connectivity"
```

### Definition of Done
- [ ] All 6 verbs implemented and tested
- [ ] Address encode/decode roundtrips correctly
- [ ] DNS lookups work for public hostnames
- [ ] PR merged to develop

---

## Phase 2: Buffered I/O Operations

**Goal**: Higher-level read operations with timeouts and patterns
**PR Scope**: Add 4 verbs with timeout logic
**Model**: 🟡 **Sonnet** - Complex timeout and pattern matching logic

### Why Sonnet?

These verbs require:
1. **Timeout handling** with `select()` or `poll()`
2. **Pattern matching** for `readStreamUntil`
3. **Chunked I/O** with yield points
4. **Error recovery** logic

### Tasks

#### 2.1 Implement Timeout Infrastructure
- 🟡 **Sonnet**: Add timeout parameter handling
- 🟡 **Sonnet**: Implement `select()`-based wait with timeout
- 🟡 **Sonnet**: Handle EINTR (signal interruption)

#### 2.2 Implement Buffered Read Verbs

| Verb | Signature | Notes |
|------|-----------|-------|
| `tcp.readStreamUntil` | `(stream, pattern, timeout, @buffer)` | Read until pattern found |
| `tcp.readStreamBytes` | `(stream, count, timeout, @buffer)` | Read exact byte count |
| `tcp.readStreamUntilClosed` | `(stream, timeout, @buffer)` | Read until EOF |

- 🟡 **Sonnet**: Pattern matching in read buffer
- 🟡 **Sonnet**: Accumulate data into ODB address
- 🟡 **Sonnet**: Handle partial reads and reconnect

#### 2.3 Implement Chunked Write

| Verb | Signature | Notes |
|------|-----------|-------|
| `tcp.writeStringToStream` | `(stream, data, chunk, timeout)` | Chunked write with yield |

- 🟡 **Sonnet**: Chunk data and yield between chunks
- 🟡 **Sonnet**: Allow cancellation mid-write

### Tests

**Network Tests** (added to `tcp_verbs_network.yaml`):
```yaml
- name: "tcp.readStreamUntil finds HTTP headers"
  script: |
    local (stream = tcp.openStream("example.com", 80));
    tcp.writeStream(stream, "GET / HTTP/1.0\r\nHost: example.com\r\n\r\n");
    local (headers = "");
    tcp.readStreamUntil(stream, "\r\n\r\n", 30, @headers);
    tcp.closeStream(stream);
    return headers contains "HTTP/1."
  expected_success: true
  expected_result: "true"
  skip: "Requires external network connectivity"

- name: "tcp.readStreamBytes gets exact count"
  script: |
    local (stream = tcp.openStream("example.com", 80));
    tcp.writeStream(stream, "GET / HTTP/1.0\r\nHost: example.com\r\n\r\n");
    local (response = "");
    tcp.readStreamBytes(stream, 100, 30, @response);
    tcp.closeStream(stream);
    return sizeOf(response) == 100
  expected_success: true
  expected_result: "true"
  skip: "Requires external network connectivity"
```

**Note**: Phase 2 has no local-only tests - all buffered I/O verbs require actual network connections to validate behavior

### Definition of Done
- [ ] All 4 verbs implemented with proper timeout handling
- [ ] `tcp.readStreamUntil` enables HTTP header parsing
- [ ] `tcp.httpClient` script works with these primitives
- [ ] PR merged to develop

---

## Phase 3: Server Operations (Listen/Accept)

**Goal**: TCP server functionality with callback support
**PR Scope**: Add 3 verbs with threading
**Model**: 🟡 **Sonnet** - Complex threading and callback execution

### ⚠️ Dependency: Threading Infrastructure

**This phase requires**:
1. Thread creation and management (pthread)
2. Thread-local database context
3. Executing UserTalk scripts from C threads
4. Thread-safe access to global state

**If threading infrastructure doesn't exist**:
- Create separate planning document for threading foundation
- Or implement "blocking listener" as interim solution

### Tasks

#### 3.1 Implement Listen Socket
- 🟡 **Sonnet**: Create `tcp.listenStream` verb
- 🟡 **Sonnet**: Bind to port with SO_REUSEADDR
- 🟡 **Sonnet**: Start accept thread

#### 3.2 Implement Accept Thread
- 🟡 **Sonnet**: Accept loop in pthread
- 🟡 **Sonnet**: Allocate stream for each connection
- 🟡 **Sonnet**: Invoke UserTalk callback with (stream, refcon)
- 🟡 **Sonnet**: Handle thread-local database context

#### 3.3 Implement Close Listener
- 🟡 **Sonnet**: Stop accept thread gracefully
- 🟡 **Sonnet**: Clean up resources

| Verb | Signature | Notes |
|------|-----------|-------|
| `tcp.listenStream` | `(port, depth, callback, refcon, ip)` | Start listener |
| `tcp.closeListen` | `(listenID)` | Stop listener |
| `tcp.statusStream` | `(stream, @bytes)` | Check pending data |

### Tests

**Phase 3 Tests Become Self-Contained** (no external dependencies):

Once `tcp.listenStream()` is implemented, all TCP tests can become **self-contained** with no external network dependencies:

```yaml
# tests/integration/test_cases/tcp_verbs.yaml (migrated from tcp_verbs_network.yaml)

- name: "tcp.listenStream accepts connection"
  setup_script: |
    # Start test server within test harness
    on echoHandler(stream, refcon) {
      local(data = tcp.readStream(stream, 1024));
      tcp.writeStream(stream, "ECHO: " + data);
      tcp.closeStream(stream)
    };
    tcp.listenStream(8080, 5, @echoHandler, 0)

  script: |
    # Client connects to localhost test server
    local (stream = tcp.openAddrStream("127.0.0.1", 8080));
    tcp.writeStream(stream, "TEST");
    local (response = tcp.readStream(stream, 1024));
    tcp.closeStream(stream);
    return response == "ECHO: TEST"
  expected_success: true
  expected_result: "true"

- name: "tcp.httpClient with local test server"
  setup_script: |
    on httpHandler(stream, refcon) {
      local(request = tcp.readStream(stream, 1024));
      tcp.writeStream(stream, "HTTP/1.0 200 OK\r\n\r\nTest Response");
      tcp.closeStream(stream)
    };
    tcp.listenStream(8081, 5, @httpHandler, 0)

  script: |
    local (stream = tcp.openAddrStream("127.0.0.1", 8081));
    tcp.writeStream(stream, "GET / HTTP/1.0\r\n\r\n");
    local (response = tcp.readStream(stream, 1024));
    tcp.closeStream(stream);
    return response contains "Test Response"
  expected_success: true
  expected_result: "true"
```

**Benefits of Phase 3 Self-Contained Tests**:
- No external network dependencies (no DNS, no internet required)
- Fully deterministic (no network timeouts or failures)
- Complete control over server responses
- Can simulate error conditions (server crashes, malformed responses)
- Safe for CI/CD in air-gapped environments
- Tests run at full speed without network latency

**Migration Strategy**:
1. Phase 1-2: Network tests in `tcp_verbs_network.yaml` (opt-in)
2. Phase 3: Migrate network tests to `tcp_verbs.yaml` using localhost test servers
3. Phase 3: Deprecate `tcp_verbs_network.yaml` (no longer needed)
4. Result: All TCP tests run by default with zero external dependencies

### Definition of Done
- [ ] Listen sockets can accept incoming connections
- [ ] Callbacks execute in separate threads
- [ ] No connection leaks
- [ ] Simple echo server works
- [ ] PR merged to develop

---

## Phase 4: Advanced Features

**Goal**: File transfer and additional utilities
**PR Scope**: Add 2 remaining verbs
**Model**: 🟢 **Haiku** - Straightforward file I/O

### Tasks

| Verb | Signature | Notes |
|------|-----------|-------|
| `tcp.writeFileToStream` | `(stream, file, prefix, suffix)` | Stream file to socket |
| `tcp.statusStream` | `(stream, @bytes)` | Query pending bytes |

- 🟢 **Haiku**: Implement efficient file streaming (sendfile if available)
- 🟢 **Haiku**: Status query using `ioctl(FIONREAD)`

### Definition of Done
- [ ] File streaming works for large files
- [ ] Status query returns correct pending byte count
- [ ] PR merged to develop

---

## Summary: Haiku vs Sonnet Tasks

### 🟢 Haiku (Pattern-following, straightforward)

- **Phase 1A**: All 7 core socket verbs
- **Phase 1B**: All 6 DNS/address verbs
- **Phase 4**: Both advanced verbs
- **Total**: 15 verbs (68%)

### 🟡 Sonnet (Complex logic, architecture decisions)

- **Phase 2**: All 4 buffered I/O verbs (timeout logic)
- **Phase 3**: All 3 server verbs (threading, callbacks)
- **Total**: 7 verbs (32%)

---

## Execution Order

```
Week 1: Phase 1A (Haiku) → PR #1
Week 2: Phase 1B (Haiku) → PR #2
Week 3: Phase 2 (Sonnet) → PR #3
        ↓
        tcp.httpClient should work now!
        ↓
Week 4+: Phase 3 (Sonnet) → Requires threading infrastructure
Week 5:  Phase 4 (Haiku) → PR #5
```

---

## Validation Milestones

### Milestone 1: Basic HTTP GET (After Phase 2)

```usertalk
// This should work after Phase 2
local (stream = tcp.openStream("httpbin.org", 80))
tcp.writeStringToStream(stream, "GET /get HTTP/1.0\r\nHost: httpbin.org\r\n\r\n", 1024, 30)
local (response = "")
tcp.readStreamUntil(stream, "\r\n\r\n", 30, @response)
tcp.closeStream(stream)
msg(response)
```

### Milestone 2: Full HTTP Client (After Phase 2)

```usertalk
// The existing tcp.httpClient script should work
local (response = tcp.httpClient(
    server: "httpbin.org",
    path: "/get",
    method: "GET"))
msg(response)
```

### Milestone 3: Echo Server (After Phase 3)

```usertalk
// Simple echo server
on echoHandler(stream, refcon) {
    local (data = tcp.readStream(stream, 1024))
    tcp.writeStream(stream, data)
    tcp.closeStream(stream)
}

local (listener = tcp.listenStream(8080, 5, "echoHandler"))
// ... accept connections ...
tcp.closeListen(listener)
```

---

## Risk Mitigation

### Risk 1: Threading Infrastructure Gap

**Mitigation**:
- Phases 1-2 have no threading dependency
- Can ship HTTP client without server support
- Document threading requirements for Phase 3 planning

### Risk 2: DNS Resolution Failures in Tests

**Mitigation**:
- Use reliable DNS targets (dns.google, 8.8.8.8)
- Add fallback tests with IP addresses
- Document network requirements for test environment

### Risk 3: Platform Differences (Linux vs macOS)

**Mitigation**:
- Use standard POSIX APIs only
- Test on both platforms in CI
- Document any platform-specific code paths

---

## Files to Create/Modify

### New Files
- `Common/headers/tcpverbs.h` - Public API
- `Common/source/tcpverbs.c` - Implementation
- `tests/headless_tcp_tests.c` - C unit tests
- `tests/integration/tcp_tests.yaml` - Integration tests

### Modified Files
- `Common/headers/kernelverbdefs.h` - Add `idtcpverbs`
- `Common/source/langstartup.c` - Call `tcpinitverbs()`
- `CMakeLists.txt` or `Makefile` - Add source file
- `docs/VERB_IMPLEMENTATION_GUIDE.md` - Add TCP verb examples

---

## Open Questions for User

1. **Phase 3 Timing**: Should we create a separate "Threading Foundation" workstream, or proceed with Phases 1-2 first and plan Phase 3 later?

2. **IPv6 Support**: Should Phase 1 support IPv6, or is IPv4-only acceptable initially?

3. **Test Environment**: Do integration tests have internet access, or should we mock network calls?

4. **Windows**: Should we plan Windows support in parallel using existing WinSockNetEvents.c code, or focus on POSIX first?
