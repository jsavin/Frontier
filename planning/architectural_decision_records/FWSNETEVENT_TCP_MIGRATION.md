# ADR: fwsNetEvent* to tcp_* API Migration Strategy

## Status
**Accepted** - Implemented in PR #363 (merged 2026-01-30)

## Context

The Frontier webserver subsystem was originally implemented using the `fwsNetEvent*` API family, which was designed for the Windows File Sharing (WFS) networking layer. When porting to headless/CLI mode, we needed to enable webserver functionality using the modern `tcp_*` API.

### The Challenge

The webserver code in `langhtml.c` makes extensive calls to `fwsNetEvent*` functions:
- `fwsNetEventOpenStream()`
- `fwsNetEventCloseStream()`
- `fwsNetEventReadStream()`
- `fwsNetEventWriteStream()`
- `fwsNetEventGetPeerAddress()`
- And others...

These functions were stubbed out in headless builds, causing webserver functionality to silently fail.

### Alternatives Considered

1. **Direct Code Changes**: Replace all `fwsNetEvent*` calls with `tcp_*` calls throughout `langhtml.c`
   - Pros: Direct, obvious
   - Cons: Large diff, error-prone, harder to review, mixes platform-specific and portable code

2. **Conditional Compilation**: Use `#ifdef HEADLESS_MODE` around every call
   - Pros: Preserves original code path
   - Cons: Code duplication, maintenance burden, conditional spaghetti

3. **Macro-Based Abstraction Layer**: Define macros that map `fwsNetEvent*` to `tcp_*` in headless builds (CHOSEN)
   - Pros: Minimal code changes, clean separation, easy to review, maintains compatibility
   - Cons: Additional indirection, macros can be hard to debug

## Decision

We chose **Option 3: Macro-Based Abstraction Layer**.

### Implementation

In `langhtml.c` (lines 62-102), we define macros that redirect `fwsNetEvent*` calls to `tcp_*` implementations:

```c
#ifdef FRONTIER_HEADLESS
/* Map fwsNetEvent* to tcp_* for headless builds */
#define fwsNetEventOpenStream(addr, port) \
    ((long)tcp_open_stream((long)(addr), (long)(port)))
#define fwsNetEventCloseStream(stream) \
    ((void)tcp_close_stream((long)(stream)))
#define fwsNetEventReadStreamBytes(stream, buffer, count, timeout) \
    tcp_read_stream_bytes((long)(stream), (buffer), (count), (timeout))
/* ... additional mappings ... */
#endif
```

### Special Cases

1. **fwsNetEventGetPeerAddress**: This function takes combined output parameters for address and port, but the `tcp_*` API uses separate calls. We implemented an inline wrapper function:

```c
static inline boolean fwsNetEventGetPeerAddress(
    unsigned long stream,
    unsigned long *peeraddress,
    unsigned long *peerport) {
    long addr, port;
    if (!tcp_get_peer_address((long)stream, &addr))
        return false;
    if (!tcp_get_peer_port((long)stream, &port))
        return false;
    *peeraddress = (unsigned long)addr;
    *peerport = (unsigned long)port;
    return true;
}
```

2. **tcp_read_stream_inetd**: New function implementing two-stage timeout semantics required by the webserver's inetd-style request reading:
   - Wait `timeout_secs` for first packet
   - After first packet, reduce timeout to 1 second for subsequent reads
   - Return success on timeout (graceful termination)

## Consequences

### Positive
- **Minimal diff**: Changes concentrated in one location (macro definitions)
- **Easy to review**: Clear mapping from old API to new API
- **Maintains compatibility**: Non-headless builds continue using original implementation
- **Clean separation**: Platform-specific code isolated from business logic
- **Enables future refactoring**: Could later replace macros with function pointers for runtime switching

### Negative
- **Indirection**: Debugging requires understanding macro expansion
- **Type conversions**: `unsigned long` to `long` casts needed (documented as safe for IP addresses and ports)
- **Two implementations to maintain**: Both `fwsNetEvent*` (Windows) and `tcp_*` (headless) paths

### Neutral
- **Performance**: Macro expansion has zero runtime cost
- **Testing**: Integration tests verify the abstraction works correctly (10/11 tests pass)

## Related

- **Issue #364**: Thread cleanup race condition in webserver stop test (known limitation)
- **Planning docs**: `planning/phase4/webserver/` contains detailed implementation plans
- **Test file**: `tests/integration/test_cases/webserver_hello_world.yaml`

## Notes

The `flnextparamislast` global flag pattern used in several places is existing technical debt unrelated to this migration. Future refactoring should address this anti-pattern separately.
