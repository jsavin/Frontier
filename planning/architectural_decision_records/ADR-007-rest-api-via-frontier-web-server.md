# ADR-007: REST API via Frontier's Built-in Web Server

**Status**: Accepted
**Date**: 2026-01-12
**Author**: TPM/CTO
**Relates to**: REPL Interactive Mode (Phase 3), GUI Development, Frontier 2.0
**Supersedes**: N/A
**Dependencies**: Long-running process support (prerequisite), JSON parsing verbs (future)

## Executive Summary

Frontier needs a REST API to enable external processes (GUI applications, web dashboards, REPL clients) to communicate with a running frontier-cli instance. This ADR establishes the architectural decision to use **Frontier's built-in web server** rather than embedding an external HTTP library (libmicrohttpd, civetweb, etc.).

**Recommended Solution**: Leverage Frontier's existing HTTP server with UserTalk-based endpoint handlers, enabling deep integration with the runtime and user extensibility without external dependencies.

## Context

### The Problem

**Requirement**: Enable inter-process communication for:
1. **REPL client mode** - Connect terminal REPL to running server (`/connect`)
2. **GUI applications** - Native desktop apps (SwiftUI, Qt, WPF, Electron) controlling Frontier
3. **Web dashboards** - Browser-based monitoring/control interfaces
4. **Parallel development** - GUI team can start work immediately once API is defined

**Design Constraints**:
- Must work cross-platform (macOS, Linux, Windows)
- Should enable parallel GUI development (don't block on server implementation)
- Must integrate deeply with Frontier runtime (evaluate UserTalk, access ODB, etc.)
- Should be extensible by users without recompiling

### Traditional Approach: Embedded HTTP Library

Initial consideration was to embed a C HTTP server library:
- **libmicrohttpd** - Small, widely used
- **civetweb** - Well maintained, supports WebSockets
- **mongoose** - Single-file embedded server
- **h2o** - Fast HTTP/2 server

**REST API Design Pattern**:
```
POST /api/v1/eval        - Evaluate UserTalk code
GET  /api/v1/workspace   - List workspace variables
POST /api/v1/workspace   - Set variable
GET  /api/v1/odb/*       - Read ODB path
POST /api/v1/odb/*       - Write ODB path
WS   /api/v1/events      - Event stream (WebSocket)
```

### Alternative: Frontier's Built-in Web Server

**Discovery**: Frontier already has a built-in web server designed for exactly this use case.

**Current Capabilities**:
- HTTP 1.0 support (sufficient for REST)
- URL routing to UserTalk handlers
- Hooks into `system.verbs.apps.*` callback layer
- Used in legacy Frontier for web scripting and API endpoints

**Current Limitations**:
- HTTP 1.0 only (no HTTP/1.1 keep-alive, chunked transfer)
- No TLS/SSL support (cleartext HTTP only)
- No WebSocket support
- May need adaptation for headless mode (unknown current state)
- Requires long-running process support (prerequisite work)

### Strategic Requirements

**North Star: Parallel GUI Development**

GUI development should start **immediately** once API spec is documented, without waiting for full server implementation. Allows:
- GUI mockups and UX iteration
- Client library development (JavaScript, Swift, C#, Python)
- Integration testing against stub server

**North Star: User Extensibility**

Users should be able to define custom API endpoints in UserTalk without rebuilding frontier-cli:
```usertalk
// system.verbs.apps.myAPI.handlers.GET.custom.endpoint
on handleRequest(requestTable) {
  // Custom business logic in UserTalk
  return json.stringify({"status": "ok", "data": myData})
}
```

**North Star: Deep Runtime Integration**

API handlers need full access to:
- UserTalk evaluation (`lang.runScript()`)
- ODB traversal (table paths, external objects)
- Kernel verbs (file, string, date, etc.)
- Workspace and context management
- Frontier's existing web server provides this naturally

## Architectural Options

### Option 1: Embed External HTTP Library (REJECTED)

**Description**: Link civetweb or libmicrohttpd into frontier-cli, implement REST handlers in C.

**Advantages**:
- ✅ Modern HTTP features (HTTP/1.1, HTTP/2, WebSocket)
- ✅ TLS/SSL support out of the box
- ✅ Well-documented, battle-tested libraries
- ✅ No dependency on long-running process work

**Disadvantages**:
- ❌ External dependency (new library to maintain)
- ❌ REST handlers written in C (complex, requires recompile to change)
- ❌ Users cannot extend API without rebuilding
- ❌ Duplicates functionality Frontier already has
- ❌ Miss opportunity to use Frontier's strengths (UserTalk scripting)
- ❌ More complex to integrate deeply with runtime

**Example Handler** (in C):
```c
// frontier-cli/api_handlers.c
static int handle_eval(struct mg_connection *conn, void *cbdata) {
    // Parse JSON request body (need JSON parser library)
    // Call lang.runScript() via internal API
    // Build JSON response (need JSON generator library)
    // Send HTTP response
}
```

### Option 2: Frontier's Built-in Web Server (RECOMMENDED)

**Description**: Use Frontier's existing HTTP server with UserTalk-based endpoint handlers.

**Advantages**:
- ✅ Zero external dependencies (Frontier already has HTTP server)
- ✅ REST handlers written in **UserTalk** (users can extend)
- ✅ Deep integration with runtime (native UserTalk evaluation)
- ✅ Plays to Frontier's strengths (web scripting platform)
- ✅ Consistent with legacy Frontier architecture
- ✅ Users can customize/extend API without recompiling
- ✅ Simpler implementation (leverage existing code)

**Disadvantages**:
- ❌ HTTP 1.0 only (acceptable for REST, just more verbose)
- ❌ No TLS (workaround: reverse proxy like nginx/Caddy)
- ❌ No WebSocket (workaround: long-polling or Server-Sent Events)
- ❌ Requires long-running process support (prerequisite work)
- ❌ May need adaptation for headless builds
- ❌ Unknown current state in headless (exploration needed)

**Example Handler** (in UserTalk):
```usertalk
// system.verbs.apps.apiServer.handlers.POST.api.v1.eval
on handleEval(requestTable) {
  local(body = json.parse(requestTable.body));
  local(code = body.code);

  try {
    local(result);
    if not lang.runScript(code, @result) {
      return json.stringify({
        "success": false,
        "error": lang.getError()
      })
    };

    return json.stringify({
      "success": true,
      "value": string(result),
      "type": string(typeOf(result))
    })
  }
}
```

**Architecture**:
```
┌─────────────────────────────────────────────────┐
│  frontier-cli --server --port 5555               │
├─────────────────────────────────────────────────┤
│  Frontier Web Server (HTTP 1.0)                  │
│    - Listens on TCP port (localhost or 0.0.0.0) │
│    - Routes URLs to UserTalk handlers            │
│    - Hooks into system.verbs.apps.* layer        │
├─────────────────────────────────────────────────┤
│  REST API Handlers (UserTalk in system.root)     │
│    POST /api/v1/eval → evalHandler()            │
│    GET  /api/v1/workspace → workspaceHandler()  │
│    GET  /api/v1/odb/* → odbHandler()            │
│                                                  │
│    Written in UserTalk - users can extend!      │
└─────────────────────────────────────────────────┘
          ▲
          │ HTTP 1.0
          │
┌─────────┴───────────┐  ┌─────────────────┐  ┌─────────────────┐
│ REPL Client         │  │ GUI Application │  │ Web Dashboard   │
│ (frontier-cli       │  │ (Swift/Qt/      │  │ (via nginx      │
│  --connect)         │  │  Electron)      │  │  reverse proxy) │
└─────────────────────┘  └─────────────────┘  └─────────────────┘
```

### Option 3: Hybrid Approach (DEFERRED)

**Description**: Start with Frontier's web server, add external library later if needed.

**Rationale**:
- Get started quickly with built-in server
- Add modern HTTP features (TLS, WebSocket) later if UserTalk-based approach proves insufficient
- Allows learning from production usage before committing to external dependency

**Decision**: Not needed. Option 2 is sufficient for MVP and future needs.

## Decision

**Choose Option 2: Frontier's Built-in Web Server**

### Rationale

1. **Architectural Consistency**: Use Frontier's existing web server rather than duplicating functionality
2. **User Extensibility**: UserTalk-based handlers allow users to extend API without recompiling
3. **Deep Integration**: Native access to evaluation engine, ODB, and kernel verbs
4. **Zero Dependencies**: No external HTTP library to maintain
5. **Plays to Strengths**: Frontier was designed as a web scripting platform

### Addressing Limitations

**HTTP 1.0 Only**:
- Acceptable for REST API (just requires explicit Connection: close headers)
- No keep-alive means one request per connection (slight performance hit)
- Not a blocking issue for typical API usage patterns

**No TLS**:
- For local development: HTTP over localhost is acceptable
- For production: Reverse proxy pattern (nginx/Caddy/Apache with TLS termination)
- Document recommended deployment architecture
- Future enhancement: Add TLS support to Frontier's server (if needed)

**No WebSocket**:
- Phase 1: No real-time events (polling for results)
- Phase 2: Long-polling for `msg()` output and events
- Phase 3: Server-Sent Events (SSE) if feasible with HTTP 1.0
- Future enhancement: Add WebSocket support to Frontier's server

**Long-Running Process Prerequisite**:
- REST API implementation depends on frontier-cli supporting long-running server mode
- This is already part of REPL roadmap (Phase 4)
- Natural sequencing: REPL → Server Mode → REST API

## Implementation Phases

### Phase 0: Prerequisites (Before REST API Work)

**Goal**: Enable long-running frontier-cli process.

- [ ] Implement REPL main loop (Phase 1-3 from REPL design)
- [ ] Add `--server` mode that runs indefinitely without REPL UI
- [ ] Verify Frontier web server works in headless builds
- [ ] Document web server configuration (port, bind address, etc.)

**Out of Scope**: No REST API yet, just foundation work.

### Phase 1: Web Server Exploration (Week 1)

**Goal**: Understand current state and requirements.

- [ ] Explore Frontier's web server implementation (file locations, architecture)
- [ ] Understand URL routing mechanism (how requests map to UserTalk handlers)
- [ ] Document current capabilities and limitations
- [ ] Identify adaptation needed for headless mode
- [ ] Check JSON parsing/generation support (may need to port from OPML Editor)

**Deliverable**: `planning/phase3/FRONTIER_WEB_SERVER_ANALYSIS.md`

### Phase 2: JSON Support (Week 2)

**Goal**: Enable JSON request/response handling in UserTalk.

**Note**: JSON parsers exist in Dave Winer's OPML Editor app but not yet available in Frontier runtime.

Options:
1. Port JSON verbs from OPML Editor codebase
2. Implement minimal JSON parser in C (json.parse, json.stringify kernel verbs)
3. Start with simple text-based protocol, add JSON later

**Deliverable**:
- `json.parse(jsonString)` → table/value
- `json.stringify(value)` → jsonString

### Phase 3: REST API Specification (Week 3)

**Goal**: Document API contract for parallel GUI development.

- [ ] Define REST endpoints (POST /api/v1/eval, GET /api/v1/workspace, etc.)
- [ ] Document request/response schemas (OpenAPI/Swagger spec)
- [ ] Write example client code (curl, JavaScript, Python)
- [ ] Create stub server (returns mock data) for GUI development
- [ ] Write integration tests (API contract tests)

**Deliverable**:
- `docs/REST_API_SPECIFICATION.md` (OpenAPI format)
- `tools/api_stub_server.py` (mock server for GUI development)

**GUI team can start work at this point!**

### Phase 4: UserTalk Handler Implementation (Weeks 4-6)

**Goal**: Implement core REST endpoints in UserTalk.

- [ ] Create `system.verbs.apps.apiServer` suite
- [ ] Implement eval endpoint (POST /api/v1/eval)
- [ ] Implement workspace endpoints (GET/POST /api/v1/workspace)
- [ ] Implement ODB read/write endpoints (GET/POST /api/v1/odb/*)
- [ ] Add error handling and validation
- [ ] Write integration tests against live server

**Example**: `system.verbs.apps.apiServer.handlers.POST.api.v1.eval`

### Phase 5: Security and Deployment (Week 7)

**Goal**: Production-ready deployment patterns.

- [ ] Authentication token support (Bearer tokens)
- [ ] Rate limiting (prevent abuse)
- [ ] `--bind 0.0.0.0` explicit flag (default: localhost only)
- [ ] Document reverse proxy setup (nginx/Caddy for TLS)
- [ ] Security audit checklist
- [ ] Production deployment guide

**Deliverable**: `docs/REST_API_DEPLOYMENT.md`

### Phase 6: Events and Real-time (Future)

**Goal**: Enable real-time `msg()` output and event streaming.

- [ ] Long-polling endpoint (GET /api/v1/events?since=timestamp)
- [ ] Server-Sent Events (SSE) investigation (feasible with HTTP 1.0?)
- [ ] WebSocket support (future enhancement to Frontier's server)

## Success Metrics

- [ ] GUI team can start development after Phase 3 (API spec + stub server)
- [ ] Users can define custom endpoints in UserTalk without recompiling
- [ ] REPL client can connect to running server (`/connect localhost:5555`)
- [ ] Native GUI apps (SwiftUI/Qt/Electron) can control Frontier via REST API
- [ ] Web dashboards can monitor/control Frontier via HTTP
- [ ] No external HTTP library dependencies
- [ ] Production deployment with TLS (via reverse proxy) documented and tested

## Future Enhancements

### TLS Support

If reverse proxy pattern proves insufficient, add TLS directly to Frontier's web server:
- Integrate OpenSSL or mbedTLS
- Add `--tls-cert` and `--tls-key` flags
- Support modern TLS 1.3

### WebSocket Support

For real-time bidirectional communication:
- Implement WebSocket handshake (HTTP Upgrade)
- Add frame parsing/generation
- Enable push notifications (server → client)
- Live ODB update streaming

### HTTP/1.1 Support

Upgrade to HTTP/1.1 for:
- Keep-alive connections (performance)
- Chunked transfer encoding
- Better caching semantics

### Platform-Specific IPC (Optional)

Add optimized local IPC for same-machine communication:
- **macOS/Linux**: Unix domain sockets (faster than TCP localhost)
- **Windows**: Named pipes (Windows native IPC)
- **Linux**: D-Bus integration (desktop environment integration)

Still use HTTP protocol over these transports (HTTP over Unix socket is standard practice).

## References

- [REPL Interactive Mode Design](../phase4/REPL_INTERACTIVE_MODE_DESIGN.md) - Context for server mode
- [Frontier Web Server Documentation](TBD) - Legacy Frontier server architecture
- [REST API Specification](TBD) - OpenAPI/Swagger spec (Phase 3 deliverable)
- [Dave Winer's OPML Editor](https://github.com/scripting/opmlEditor) - JSON parser source

## Related ADRs

- ADR-005: Parameter State Thread-Safety - Prerequisite for multi-client server
- ADR-006: Outline Context Refactoring - Foundation for concurrent ODB access

## Open Questions

1. **Current State**: What is the state of Frontier's web server in headless builds?
   - **Answer (2026-01-12)**: Unknown - requires exploration in Phase 1
2. **Routing Mechanism**: How are URLs mapped to UserTalk handlers? (hierarchical, table, pattern matching?)
   - **Answer (2026-01-12)**: "It's complicated ;-)" - requires exploration in Phase 1
3. **JSON Support**: Should we port from OPML Editor or implement fresh?
   - **Answer (2026-01-12)**: Asked Dave Winer for OPML Editor code - will port if available
4. **Authentication**: Token-based auth sufficient, or need OAuth/JWT?
   - **Answer (2026-01-12)**: Open to modern best practices, but reluctant to build full identity system. Simple token-based auth likely sufficient for MVP. Discuss requirements before implementation.
5. **Rate Limiting**: Implement in C or UserTalk?
   - **Answer**: TBD - depends on performance requirements

## Notes

- This ADR establishes direction but not implementation details (requires exploration)
- Web server exploration (Phase 1) may reveal need for adjustments
- JSON support is gating factor (without it, API is text-only)
- Long-running process support is prerequisite (REPL Phase 4)
- TLS via reverse proxy is production-ready pattern (not a limitation)
