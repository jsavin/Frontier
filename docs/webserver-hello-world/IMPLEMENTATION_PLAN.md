# Webserver Hello World - Implementation Plan

**Goal:** Get a basic HTTP request/response working through Frontier's webserver stack.

**Success Criteria:** `curl http://localhost:8080/helloworld` returns `<html><body>Hello World!</body></html>`

**Branch:** `feature/webserver-hello-world`

---

## Current State Assessment

### What's Already Done
- ✅ All 23 TCP verbs implemented in `tcpverbs.c` (PR #361)
- ✅ Webserver kernel verbs wired up in `langhtml.c` (lines 9954-10095)
- ✅ Implementation functions exist: `inetdsupervisor()`, `webserverserver()`, `webserverdispatch()`
- ✅ Built-in helloWorld responder exists at `webserver.data.responders.helloWorld`

### The Gap
The webserver kernel implementations in `langhtml.c` call `fwsNetEvent*` functions, which are **stubbed out in headless mode** (lines 63-70):

```c
#define fwsNetEventReadStreamUntil(a,b,c,d) (false)
#define fwsNetEventReadStreamBytes(a,b,c,d) (false)
#define fwsNetEventCloseStream(a) (false)
#define fwsNetEventGetPeerAddress(a,b,c) (false)
#define fwsNetEventAddressDecode(a,b) (false)
#define fwsNetEventInetdRead(a,b,c) (false)
#define fwsNetEventWriteHandleToStream(a,b,c,d) (false)
#define fwsNetEventAbortStream(a) ((void)0)
```

These need to be migrated to use the new `tcp_*` API from `tcpverbs.h`.

---

## Implementation Phases

### Phase 1: Investigation & Test Setup
**Model:** Sonnet (code exploration, test writing)

#### Task 1.1: Audit fwsNetEvent* calls in langhtml.c
- Map each `fwsNetEvent*` call to its `tcp_*` equivalent
- Document parameter differences
- Identify any missing functionality

**Mapping (preliminary):**

| fwsNetEvent* | tcp_* equivalent | Notes |
|--------------|------------------|-------|
| `fwsNetEventCloseStream(stream)` | `tcp_close_stream(stream)` | Direct replacement |
| `fwsNetEventAbortStream(stream)` | `tcp_abort_stream(stream)` | Direct replacement |
| `fwsNetEventGetPeerAddress(stream, &addr, &port)` | `tcp_get_peer_address(stream, &addr)` + `tcp_get_peer_port(stream, &port)` | Split into two calls |
| `fwsNetEventAddressDecode(addr, bs)` | `tcp_address_decode(addr, bs)` | Direct replacement |
| `fwsNetEventReadStreamUntil(stream, h, hpattern, timeout)` | `tcp_read_stream_until(stream, h, hpattern, timeout)` | Check buffer semantics |
| `fwsNetEventReadStreamBytes(stream, h, count, timeout)` | `tcp_read_stream_bytes(stream, h, count, timeout)` | Check buffer semantics |
| `fwsNetEventInetdRead(stream, h, timeout)` | Need to investigate | May be custom read loop |
| `fwsNetEventWriteHandleToStream(stream, h, chunk, timeout)` | `tcp_write_string_to_stream(stream, h, chunk, timeout)` | Direct replacement |

#### Task 1.2: Create integration test for webserver Hello World
- Write YAML test case that starts listener, sends HTTP request, validates response
- Test should fail initially (proves the gap exists)
- Location: `tests/integration/test_cases/webserver_hello_world.yaml`

**Test outline:**
```yaml
- name: "webserver Hello World responds with HTML"
  setup: |
    webserver.init()
    webserver.data.responders.helloWorld.enabled = true
    inetd.startOne(@user.inetd.config.http)
  script: |
    # Use tcp verbs to connect and send HTTP request
    local stream = tcp.openNameStream("localhost", 80)
    tcp.writeStream(stream, "GET /helloworld HTTP/1.1\r\nHost: localhost\r\n\r\n")
    local response = ""
    tcp.readStreamUntil(stream, @response, "\r\n\r\n", 5)
    tcp.closeStream(stream)
    return (response contains "Hello World")
  expected: true
```

#### Task 1.3: Create unit test for fwsNetEvent* migration
- Test each migrated function individually
- Location: `tests/headless_webserver_verbs.c`

---

### Phase 2: Core Migration
**Model:** Opus (architectural changes, careful refactoring)

#### Task 2.1: Create tcp_* wrapper functions for langhtml.c
Instead of modifying the existing code inline, create clean wrapper functions that match the `fwsNetEvent*` signatures but call `tcp_*` internally.

**File:** `Common/source/langhtml.c` (or new `webserver_tcp.c`)

```c
#ifdef FRONTIER_HEADLESS
/* TCP wrapper for headless mode */
static boolean fwsNetEventCloseStream_tcp(long stream) {
    return tcp_close_stream(stream);
}
#define fwsNetEventCloseStream(a) fwsNetEventCloseStream_tcp(a)
// ... etc
#endif
```

#### Task 2.2: Migrate inetdsupervisor() TCP calls
Lines to modify in `inetdsupervisor()`:
- Line 6695: `fwsNetEventCloseStream(stream)`
- Line 6725: `fwsNetEventGetPeerAddress(stream, &peeraddress, &peerport)`
- Line 6728: `fwsNetEventAddressDecode(peeraddress, bspeeraddress)`
- Line 6827: `fwsNetEventInetdRead(stream, hrequest, timeout)`
- Line 6856: `fwsNetEventWriteHandleToStream(stream, vreturndata.data.stringvalue, chunksize, timeout)`
- Line 6863: `fwsNetEventCloseStream(stream)`
- Line 6886: `fwsNetEventAbortStream(stream)`

#### Task 2.3: Migrate webserverserver() TCP calls
Lines to modify in `webserverserver()`:
- Line 6006: `fwsNetEventReadStreamUntil(stream, h, hpattern, timeout)`
- Line 6084: `fwsNetEventReadStreamBytes(stream, h, ctfullrequest, timeout)`

---

### Phase 3: Callback Infrastructure
**Model:** Opus (complex callback/threading integration)

#### Task 3.1: Verify tcp.listenStream callback invocation
- Confirm that `tcp_listen_stream()` properly invokes the UserTalk callback
- The callback should receive `(stream, refcon)` parameters
- Trace through `tcpverbs.c` accept thread → callback dispatch

#### Task 3.2: Test inetd.supervisor callback path
- Verify `@inetd.supervisor` can be resolved from callback
- Test that `inetdsupervisor()` C function is invoked correctly
- Check parameter passing (stream ID, refcon)

---

### Phase 4: End-to-End Testing
**Model:** Sonnet (testing, validation)

#### Task 4.1: Run integration test
- Execute the webserver Hello World test from Phase 1
- Debug any failures
- Iterate until passing

#### Task 4.2: Manual validation
```bash
# Terminal 1: Start Frontier with webserver
./frontier-cli --system-root databases/Frontier.root -e "
    webserver.init();
    webserver.data.responders.helloWorld.enabled = true;
    user.inetd.config.http.port = 8080;  // Non-privileged port
    inetd.startOne(@user.inetd.config.http);
    dialog.alert('Webserver running on port 8080')
"

# Terminal 2: Test with curl
curl -v http://localhost:8080/helloworld
```

#### Task 4.3: Add regression tests
- Test error cases (invalid path, timeout, connection refused)
- Test multiple concurrent requests
- Test large responses

---

### Phase 5: Cleanup & Documentation
**Model:** Haiku (simple edits, documentation)

#### Task 5.1: Remove dead code
- Remove or comment out unused `fwsNetEvent*` stub macros if fully migrated
- Clean up any debugging code

#### Task 5.2: Update documentation
- Update `docs/webserver-hello-world/ARCHITECTURE.md` with implementation notes
- Add troubleshooting section
- Document any known limitations

#### Task 5.3: Create PR
- Summarize changes
- Include test results
- Reference this plan

---

## Task Summary with Model Recommendations

| Phase | Task | Model | Rationale |
|-------|------|-------|-----------|
| 1.1 | Audit fwsNetEvent* calls | **Sonnet** | Code exploration, pattern matching |
| 1.2 | Create integration test | **Sonnet** | Test writing, YAML |
| 1.3 | Create unit tests | **Sonnet** | Test writing, C boilerplate |
| 2.1 | Create tcp_* wrappers | **Opus** | Architectural decision, API design |
| 2.2 | Migrate inetdsupervisor() | **Opus** | Critical path, error handling |
| 2.3 | Migrate webserverserver() | **Opus** | Critical path, buffer semantics |
| 3.1 | Verify callback invocation | **Opus** | Threading, complex control flow |
| 3.2 | Test callback path | **Opus** | Integration debugging |
| 4.1 | Run integration test | **Sonnet** | Test execution, debugging |
| 4.2 | Manual validation | **Sonnet** | Simple verification |
| 4.3 | Add regression tests | **Sonnet** | Test writing |
| 5.1 | Remove dead code | **Haiku** | Simple deletion |
| 5.2 | Update documentation | **Haiku** | Markdown editing |
| 5.3 | Create PR | **Haiku** | PR description writing |

---

## Risk Assessment

### High Risk
- **Callback threading model**: The TCP accept thread invokes UserTalk callbacks. Need to ensure thread safety and proper context setup.
- **Buffer ownership semantics**: `fwsNetEvent*` and `tcp_*` may have different expectations about who owns/frees handles.

### Medium Risk
- **Error handling differences**: Error codes and messages may differ between old and new implementations.
- **Timeout behavior**: Ensure timeout semantics match (idle vs total time).

### Low Risk
- **Simple function mappings**: Most `fwsNetEvent*` → `tcp_*` mappings are straightforward.

---

## Dependencies

- **PR #361** (TCP 100% coverage) - ✅ Merged
- **Issue #362** (Configurable throughput window) - Not blocking, P1 follow-up

---

## Estimated Effort

| Phase | Effort |
|-------|--------|
| Phase 1 (Investigation) | 2-4 hours |
| Phase 2 (Migration) | 4-8 hours |
| Phase 3 (Callbacks) | 2-4 hours |
| Phase 4 (Testing) | 2-4 hours |
| Phase 5 (Cleanup) | 1-2 hours |
| **Total** | **11-22 hours** |

---

## Success Metrics

1. ✅ Integration test passes: `webserver_hello_world.yaml`
2. ✅ Manual curl test returns expected HTML
3. ✅ No regressions in existing TCP verb tests
4. ✅ Code review approved
5. ✅ Documentation updated

---

## References

- `docs/webserver-hello-world/ARCHITECTURE.md` - System architecture
- `Common/source/langhtml.c` - Webserver kernel implementations
- `Common/source/tcpverbs.c` - TCP verb implementations
- `Common/headers/tcpverbs.h` - TCP API definitions
- `usertalk_scripts/Frontier.root/system/verbs/builtins/webserver/` - UserTalk webserver code
