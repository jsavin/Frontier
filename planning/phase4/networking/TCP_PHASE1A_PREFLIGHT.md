# TCP Phase 1A Pre-Flight Checklist

**Date**: 2026-01-20
**Status**: ✅ READY TO START
**Agent ID**: a809f22 (resume Explore agent for additional questions)

---

## Executive Summary

**All critical blockers resolved. Green light for TCP Phase 1A implementation.**

Key findings:
- ✅ Network operations work in integration test framework
- ✅ Starting from clean slate (no legacy code to work around)
- ✅ Verb registration pattern well-established
- ✅ POSIX libraries available (no external dependencies)
- ✅ Comprehensive documentation exists
- ✅ Build system ready (no config changes needed)

---

## Critical Findings

### 1. Testing Infrastructure - ✅ READY

**Network testing is fully supported**:
- Integration tests can make actual network connections (no sandbox restrictions)
- Use `{FRONTIER_TEST_TMP_DIR}` template for any file operations
- Can test against external servers (e.g., httpbin.org) or localhost
- Thread sanitizer available: `make TSAN=1`

**Test Strategy**:
```yaml
# tests/integration/test_cases/tcp_verbs.yaml
tests:
  - name: "tcp.openAddrStream - basic connectivity"
    script: |
      local (stream = tcp.openAddrStream("127.0.0.1", 80));
      tcp.closeStream(stream)
    expected_success: true
```

---

### 2. Existing Code - ⚠️ CLEAN SLATE

**No existing POSIX socket implementation**:
- `tests/headless_tcp_verbs.c` has verb registration stubs only
- All TCP verbs currently return "not implemented"
- **This is good**: No legacy patterns to work around

**Starting point**:
- Follow architecture in `planning/phase4/networking/NETWORKING_ARCHITECTURE.md`
- Reference file verbs (`Common/source/fileverbs.c`) for verb patterns

---

### 3. Verb Registration Pattern - ✅ ESTABLISHED

**Standard pattern** (from fileverbs.c):

```c
// 1. Define token enum
enum {
    tcpv_openaddrstream = 0,
    tcpv_readstream = 1,
    // ... 5 more verbs
    tcpv_countconnections = 6
};

// 2. Implement verb processor
static boolean tcp_valueproc(short token, hdltreenode hparam1,
                             tyvaluerecord *vreturned,
                             bigstring bserror) {
    switch(token) {
        case tcpv_openaddrstream:
            return tcp_open_addr_stream(hparam1, vreturned, bserror);
        // ... 6 more cases
        default:
            return false;
    }
}

// 3. Register verbs
boolean tcpinitverbs(void) {
    hdlhashtable htable = nil;
    bigstring bsname;

    copystring(BIGSTRING("\ptcp"), bsname);
    if (!newfunctionprocessor(bsname, &tcp_valueproc, false, &htable))
        return false;

    pushhashtable(htable);
    ADD_VERB(BIGSTRING("\popenaddrstream"), tcpv_openaddrstream);
    // ... 6 more verbs
    pophashtable();
    return true;
}
```

**Integration point**: Call `tcpinitverbs()` from verb initialization chain (likely in `kernel_verbs_headless.c`)

---

### 4. Build System - ✅ NO CHANGES NEEDED

**POSIX networking is built-in on macOS**:
- Socket API: `<sys/socket.h>` (part of libc)
- Network headers: `<netinet/in.h>`, `<arpa/inet.h>` (standard)
- DNS: `<netdb.h>` with `getaddrinfo()` (standard)
- Threads: `-lpthread` already linked (used by thread registry)

**No external dependencies required.**

**Add to Makefile** (when implementing):
```makefile
# frontier-cli/Makefile - Add tcpverbs.c to sources
TCP_SOURCES = Common/source/tcpverbs.c

# tests/Makefile - Already has -lpthread for stream ID mutex
```

---

### 5. API Documentation - ✅ COMPREHENSIVE

**Definitive reference**: `planning/phase4/networking/TCP_VERBS_ANALYSIS.md`

**Phase 1A Verbs** (7 total):

| Verb | Signature | Behavior |
|------|-----------|----------|
| `tcp.openNameStream` | `(hostname, port) → streamID` | DNS lookup + connect (blocking) |
| `tcp.openAddrStream` | `(addr, port) → streamID` | Direct IP connect (addr is signed long) |
| `tcp.readStream` | `(stream, bytes) → data` | Non-blocking read, returns empty string if no data |
| `tcp.writeStream` | `(stream, data) → true` | Blocking write until all data sent |
| `tcp.closeStream` | `(stream) → true` | Graceful shutdown (FIN) |
| `tcp.abortStream` | `(stream) → true` | Immediate close (RST) |
| `tcp.countConnections` | `() → count` | Active stream count |

**Key behavioral facts**:
- Stream IDs are positive long integers (0 is invalid)
- Addresses stored as signed long in host byte order
- DNS operations are blocking in Phase 1
- readStream returns empty string (not error) when no data available

---

### 6. Error Handling Pattern - ✅ STANDARD

**Use `langerrormessage()` for exceptions**:

```c
// Connection failure
if (connect(sockfd, ...) < 0) {
    langerrormessage(BIGSTRING("\pConnection refused"));
    return false;
}

// Invalid stream ID
if (stream_id <= 0 || stream_id >= MAX_STREAMS) {
    langerrormessage(BIGSTRING("\pInvalid stream ID"));
    return false;
}

// DNS lookup failure
if (getaddrinfo(hostname, ...) != 0) {
    langerrormessage(BIGSTRING("\pDNS lookup failed"));
    return false;
}
```

**Return types**:
```c
// Success/failure verbs
return setbooleanvalue(true, vreturned);

// Stream ID (open operations)
return setlongvalue(stream_id, vreturned);

// Binary data (read operations)
return setbinaryvalue(data_handle, vreturned);

// String data (address decode)
return setstringvalue(dotted_ip, vreturned);
```

---

### 7. UserTalk Glue Layer - ✅ DOCUMENTED

**Pattern from TCP_VERBS_ANALYSIS.md**:

```usertalk
// tcp.openStream dispatches based on parameter type
on openStream (adr, port) {
    if typeOf (adr) != longType {
        return tcp.openNameStream(adr, port)  // DNS name or dotted IP
    } else {
        return tcp.openAddrStream(adr, port)  // 4-byte encoded address
    }
}

// Error handling pattern
try {
    local (stream = tcp.openStream("example.com", 80));
    tcp.writeStream(stream, "GET / HTTP/1.0\r\n\r\n");
    local (data = tcp.readStream(stream, 1024));
    tcp.closeStream(stream);
    return data
} else {
    scriptError(tryerror)
}
```

**Callback pattern** (for Phase 3):
```usertalk
// Listen callback receives (stream, refcon)
on handleConnection (stream, refcon) {
    local (data = tcp.readStream(stream, 1024));
    tcp.writeStream(stream, "HTTP/1.0 200 OK\r\n\r\nHello");
    tcp.closeStream(stream)  // MUST close or leak connection
}

tcp.listenStream(8080, 5, @handleConnection, nil)
```

---

## Implementation Checklist

### Phase 1A Core Tasks

**Step 1: Create Data Structures** (1-2 hours)
- [ ] Create `Common/headers/tcpverbs.h`
- [ ] Define `tcp_stream_t` struct (socket fd, state, buffers)
- [ ] Define `tcp_context_t` struct (stream array, mutex, count)
- [ ] Add stream state enum (INVALID, CONNECTING, CONNECTED, CLOSING, CLOSED)

**Step 2: Write Integration Tests** (2-3 hours, TDD)
- [ ] Create `tests/integration/test_cases/tcp_verbs.yaml`
- [ ] Test: tcp.openAddrStream + closeStream (localhost or external)
- [ ] Test: tcp.readStream empty (non-blocking behavior)
- [ ] Test: tcp.writeStream + readStream (echo pattern)
- [ ] Test: tcp.abortStream (immediate close)
- [ ] Test: tcp.countConnections (stream lifecycle)
- [ ] Test: Invalid stream ID error handling

**Step 3: Implement Kernel Verbs** (6-8 hours)
- [ ] Create `Common/source/tcpverbs.c`
- [ ] Implement `tcp_open_addr_stream()` (socket + connect)
- [ ] Implement `tcp_read_stream()` (non-blocking recv)
- [ ] Implement `tcp_write_stream()` (blocking send)
- [ ] Implement `tcp_close_stream()` (shutdown + close)
- [ ] Implement `tcp_abort_stream()` (immediate close)
- [ ] Implement `tcp_count_connections()` (active count)
- [ ] Implement stream allocation/deallocation with mutex

**Step 4: Register Verbs** (1 hour)
- [ ] Update `tests/headless_tcp_verbs.c` with tcpinitverbs()
- [ ] Add verb token mappings
- [ ] Add to verb initialization chain (kernel_verbs_headless.c)
- [ ] Add `idtcpverbs` constant to `Common/headers/kernelverbdefs.h`

**Step 5: Build & Test** (1-2 hours)
- [ ] Update `frontier-cli/Makefile` with tcpverbs.c
- [ ] Build: `make -C frontier-cli`
- [ ] Run unit tests: `./tools/run_headless_tests.sh`
- [ ] Run integration tests: `cd tests && make test-integration`
- [ ] Fix any test failures
- [ ] Verify thread sanitizer clean: `make TSAN=1`

**Step 6: Create PR** (1 hour)
- [ ] Commit all changes with clear messages
- [ ] Push to feature branch
- [ ] Use pull-request agent to create PR
- [ ] Run PR monitor: `./tools/monitor_pr_review.sh <PR_NUMBER>`
- [ ] Address bot feedback

---

## Key Reference Files

**Must Read Before Starting**:
1. `planning/phase4/networking/NETWORKING_ARCHITECTURE.md` - Implementation template (~1100 lines)
2. `planning/phase4/networking/TCP_VERBS_ANALYSIS.md` - API reference (~800 lines)
3. `Common/source/fileverbs.c` - Verb registration pattern reference

**Reference During Implementation**:
- `tests/headless_tcp_verbs.c` - Verb registration stub (update this)
- `Common/headers/kernelverbdefs.h` - Add idtcpverbs constant here
- `frontier-cli/Makefile` - Add tcpverbs.c source here
- `tests/integration/test_cases/file_verbs.yaml` - Integration test examples

---

## Known Constraints

**Phase 1A Specific**:
1. DNS operations are **blocking** (Phase 1B will improve with async)
2. tcp.readStream is **non-blocking** (returns empty string if no data)
3. tcp.writeStream is **blocking** (waits until all data sent)
4. Stream ID allocation uses **global mutex** (acceptable for Phase 1)
5. Maximum concurrent connections: **256** (configurable constant)

**Testing Constraints**:
- Integration tests can use network but **SHOULD NOT depend on internet**
- Use localhost for server tests (Phase 3)
- External server tests should skip gracefully if unavailable

**Phase 3 Blockers** (not applicable to Phase 1A):
- Listen/accept requires threading infrastructure (deferred)
- Callbacks need thread-local database context (Phase 4 P0a work)

---

## Success Criteria

**Phase 1A Complete When**:
- ✅ All 7 verbs implemented and tested
- ✅ Integration tests passing (tcp_verbs.yaml)
- ✅ Unit tests passing (if C-level tests added)
- ✅ Thread sanitizer clean
- ✅ Memory leak tests clean
- ✅ PR approved and merged
- ✅ Documentation updated (CLAUDE.md references)

**Validation Milestone**:
```usertalk
// This UserTalk script should work:
local (stream = tcp.openAddrStream("127.0.0.1", 80));
if stream > 0 {
    tcp.writeStream(stream, "GET / HTTP/1.0\r\n\r\n");
    local (response = tcp.readStream(stream, 1024));
    tcp.closeStream(stream);
    return response  // Should contain HTTP response
}
```

---

## Next Steps

**Ready to begin implementation**:

1. **Create worktree**:
   ```bash
   cd /Users/jake/dev/jsavin
   git worktree add Frontier-tcp-phase1a -b feature/tcp-phase1a
   cd Frontier-tcp-phase1a
   ```

2. **Follow `/doit` workflow**:
   - Write integration tests FIRST (TDD)
   - Implement kernel verbs
   - Run full test suite
   - Create PR via pull-request agent
   - Monitor PR with background script

3. **Estimated timeline**: 1-2 working sessions (8-16 hours)

---

**Last Updated**: 2026-01-20
**Status**: All blockers resolved, ready to start implementation
**Resume Agent**: `a809f22` (Explore agent for additional questions)
