# Processor Audit: `webserver`

**Status:** ⏳ **Needs Script Review** (UserTalk Implementation)
**Audit Date:** 2025-12-05
**Auditor:** Claude (Sonnet 4.5)

---

## Basic Information

| Property | Value |
|----------|-------|
| **Processor Name** | `webserver` |
| **EFP ID** | 1009 (lang block) |
| **Verb Count** | 7 (mostly/all UserTalk scripts) |
| **Window Required** | NO |
| **Documentation** | [webserver/](../../../docs/usertalk/docserver.userland.com/webserver/index.html) |
| **Stub Implementation** | [headless_webserver_verbs.c](../../../tests/headless_webserver_verbs.c) |
| **Script Implementation** | `system.verbs.builtins.webserver` (Frontier-v6.root) |

---

## Category Assessment

**Category:** ✅ **Core Network Functionality** (HTTP Server)

**Rationale:**
HTTP web server implementation built on tcp and inetd processors. According to user feedback, most or almost all functionality is implemented as UserTalk scripts in `system.verbs.builtins.webserver`, not as C kernel verbs. These scripts handle HTTP request parsing, routing, response building, and error pages.

**Headless Compatibility:** ✅ **Full** (web server is inherently headless)

**Blocking Verbs:** None

---

## Verb Inventory

### Kernel Verbs (7 verbs - likely thin wrappers to UserTalk)

| # | Verb Name | Signature | Description |
|---|-----------|-----------|-------------|
| 1 | `server` | `webserver.server() -> ?` | Main HTTP server entry point |
| 2 | `dispatch` | `webserver.dispatch(request) -> response` | Route request to handler |
| 3 | `parseheaders` | `webserver.parseHeaders(headerText) -> table` | Parse HTTP headers |
| 4 | `parsecookies` | `webserver.parseCookies(cookieHeader) -> table` | Parse Cookie header |
| 5 | `buildresponse` | `webserver.buildResponse(status, headers, body) -> string` | Build HTTP response |
| 6 | `builderrorpage` | `webserver.buildErrorPage(statusCode, message) -> html` | Generate error page HTML |
| 7 | `getserverstring` | `webserver.getServerString() -> string` | Get "Server:" header value |

**Note:** Based on user feedback, these are primarily UserTalk script implementations located in `system.verbs.builtins.webserver` in the Frontier-v6.root database.

---

## Implementation Analysis

### Complexity: **LOW** (UserTalk scripts exist)

### Dependencies

- **Other Processors:**
  - tcp (socket I/O)
  - inetd (connection dispatcher)
  - file (serve static files)
  - string (text processing)
  - date (HTTP date headers)
- **External Services:** None
- **OS-Specific Functionality:** NO (uses tcp abstraction)
- **GUI/Window Context:** NO

### Key Implementation Notes

**Architecture:**
```
Client → tcp → inetd.supervisor → webserver.server → webserver.dispatch → handler
                                              ↓
                                      webserver.parseHeaders
                                      webserver.parseCookies
                                              ↓
                                      [User's handler script]
                                              ↓
                                      webserver.buildResponse
                                      ← tcp ← Client
```

**HTTP Request Flow:**
```usertalk
on webserver.server()
    // Called by inetd when HTTP connection arrives

    // 1. Read request line
    local(requestLine = tcp.readStreamUntil(streamID, "\r\n"))
    // "GET /index.html HTTP/1.1"

    // 2. Parse headers
    local(headerText = tcp.readStreamUntil(streamID, "\r\n\r\n"))
    local(headers = webserver.parseHeaders(headerText))

    // 3. Parse cookies
    if defined(headers.cookie)
        local(cookies = webserver.parseCookies(headers.cookie))

    // 4. Dispatch to handler
    local(response = webserver.dispatch(request))

    // 5. Send response
    tcp.writeStringToStream(streamID, response)
    tcp.closeStream(streamID)
```

**Response Building:**
```usertalk
on webserver.buildResponse(statusCode, headers, body)
    // Build HTTP/1.1 response
    local(response = "HTTP/1.1 " + statusCode + "\r\n")

    // Add headers
    for header in headers
        response += header.name + ": " + header.value + "\r\n"

    // Add Server header
    response += "Server: " + webserver.getServerString() + "\r\n"

    // Blank line, then body
    response += "\r\n" + body

    return response
```

**Request Routing (dispatch):**
```usertalk
on webserver.dispatch(request)
    // Look up handler for URL path
    // e.g., /api/foo → system.handlers.api.foo()

    // If found, call handler with request table
    // Handler returns {statusCode, headers, body}

    // If not found, return 404
    return webserver.buildErrorPage(404, "Not Found")
```

**Script Location:**
All UserTalk implementations are in the Frontier-v6.root database:
- Path: `system.verbs.builtins.webserver`
- Contains actual script implementations
- Kernel verbs are likely just thin wrappers calling these scripts

---

## Implementation Status

**Current State:**
- Kernel verb stubs exist (`tests/headless_webserver_verbs.c`)
- UserTalk scripts exist in Frontier-v6.root
- Need to:
  1. Access and review UserTalk scripts
  2. Ensure kernel verbs call UserTalk implementations
  3. Verify scripts are headless-compatible
  4. Test with real HTTP requests

**Likely Implementation Pattern:**
```c
// C kernel verb (thin wrapper)
boolean webserverparseheaders(bigstring headerText, hdltreenode *result) {
    // Call UserTalk script:
    // system.verbs.builtins.webserver.parseHeaders(headerText)
    return callUserTalkScript("system.verbs.builtins.webserver.parseHeaders",
                              headerText, result);
}
```

---

## Testing Requirements

**Test Scenarios:**

**Basic HTTP GET:**
```bash
curl http://localhost:8080/
# Should return 200 OK with content

curl -v http://localhost:8080/nonexistent
# Should return 404 Not Found
```

**Header Parsing:**
```usertalk
local(headers = webserver.parseHeaders("Host: example.com\r\nUser-Agent: curl\r\n"))
headers.host → "example.com"
headers.["user-agent"] → "curl"
```

**Cookie Parsing:**
```usertalk
local(cookies = webserver.parseCookies("session=abc123; user=john"))
cookies.session → "abc123"
cookies.user → "john"
```

**Response Building:**
```usertalk
local(response = webserver.buildResponse(
    "200 OK",
    {{"Content-Type", "text/html"}},
    "<html><body>Hello</body></html>"
))
// Verify proper HTTP/1.1 format
```

**Error Pages:**
```usertalk
local(html = webserver.buildErrorPage(404, "Not Found"))
// Should contain 404 and "Not Found" in HTML
```

**Integration Test:**
```usertalk
// 1. Start HTTP server via inetd
system.inetd.config.http = {
    port: 8080,
    handler: "webserver.server"
}
inetd.startOne("http")

// 2. Make request from external client
// curl http://localhost:8080/

// 3. Verify response received
```

**Edge Cases:**
- Malformed HTTP requests
- Very large requests (> 1MB)
- Missing headers
- Invalid cookie format
- Concurrent requests
- Slow clients (timeout handling)

---

## Implementation Effort

**Estimated Time:** 8-12 hours

**Breakdown:**
- Review UserTalk scripts: 2-3 hours
- Implement kernel verb wrappers: 2-3 hours
- Integration testing: 2-3 hours
- Error handling & edge cases: 2-3 hours

**Confidence:** HIGH - Scripts already exist, just need wiring

**Dependencies:**
- tcp processor (for socket I/O)
- inetd processor (for connection management)
- Access to Frontier-v6.root to review scripts

---

## Priority & Sequencing

**Priority:** 🏆 **HIGH** (Tier 1 - Core Use Case)

**Recommended Implementation Order:** 19 (after tcp, inetd)

**Blockers/Prerequisites:**
- tcp processor (socket I/O)
- inetd processor (daemon management)
- Access to UserTalk scripts in Frontier-v6.root

**Implementation Sequence:**
1. Review UserTalk scripts in system.verbs.builtins.webserver
2. Ensure scripts are headless-compatible
3. Implement kernel verb wrappers (call UserTalk)
4. Test header/cookie parsing
5. Test response building
6. Integration test with inetd + tcp
7. Performance testing & optimization

---

## Headless Compatibility Analysis

**Fully Compatible:** ✅ YES (7/7 verbs)

All webserver verbs are headless-compatible:
- HTTP request/response handling (no GUI needed)
- Built on tcp/inetd (headless-compatible)
- Pure text protocol processing

**Script Implementation:**
- UserTalk scripts should be headless-compatible
- No GUI dependencies expected
- Need to verify scripts don't use GUI-only features

---

## Related Processors

- **inetd** - Daemon manager (calls webserver.server)
- **tcp** - Socket I/O (used by webserver)
- **http** - HTTP client utilities
- **file** - Serve static files
- **string** - Text processing
- **date** - HTTP date headers

---

## Special Considerations

**HTTP Protocol:**
- Support HTTP/1.0 and HTTP/1.1
- Handle various request methods (GET, POST, HEAD, etc.)
- Proper header parsing (case-insensitive)
- Cookie handling
- Content-Length calculation
- Transfer-Encoding: chunked (optional)

**Security:**
- **Path Traversal:** Prevent "../" attacks
- **Request Size Limits:** Prevent DOS
- **Timeout:** Handle slow clients
- **Input Validation:** Sanitize headers, cookies
- **Content-Type:** Proper MIME types

**Performance:**
- Keep-alive connections (HTTP/1.1)
- Caching (ETag, Last-Modified)
- Compression (gzip) - optional
- Static file serving efficiency

**Error Handling:**
- 400 Bad Request (malformed requests)
- 404 Not Found
- 500 Internal Server Error
- 503 Service Unavailable
- Custom error pages

**Configuration:**
Likely stored in database:
```
system.webserver.config
    .port = 8080
    .serverString = "Frontier/10.0"
    .maxRequestSize = 10485760  // 10MB
    .timeout = 30
    .handlers = {...}  // URL → handler mapping
```

---

## Next Steps - ACTION REQUIRED

**⚠️ Need Access to UserTalk Scripts:**
To complete this audit, need to:
1. Access `system.verbs.builtins.webserver` from Frontier-v6.root
2. Review actual UserTalk implementations
3. Verify headless compatibility
4. Document script behavior
5. Identify any missing functionality

**Methods to Access:**
- Build Frontier and query database
- Export database to text format
- Create database reader tool
- User provides script dumps

**Temporary Assessment:**
Based on architecture and user feedback:
- Scripts likely exist and work
- Kernel verbs are thin wrappers
- Implementation effort is LOW (scripts exist)
- Just needs wiring + testing

---

## References

**Documentation:**
- DocServer: `docs/usertalk/docserver.userland.com/webserver/` (if exists)

**Implementation:**
- Stub: `tests/headless_webserver_verbs.c`
- Scripts: `system.verbs.builtins.webserver` (Frontier-v6.root)
- Canonical verb list: `Common/resources/Win32/kernelverbs.rc` (lines 1009-1019)

**Standards:**
- HTTP/1.0: RFC 1945
- HTTP/1.1: RFC 2616 (obsoleted by RFC 7230-7235)
- Cookies: RFC 6265

---

## Audit Status

**Status:** ⏳ **INCOMPLETE** - Needs UserTalk Script Review

**What's Done:**
- ✅ Kernel verb list identified
- ✅ Architecture understood
- ✅ Dependencies mapped
- ✅ Testing strategy defined

**What's Needed:**
- ⏳ Access to `system.verbs.builtins.webserver` scripts
- ⏳ Review actual UserTalk implementations
- ⏳ Verify headless compatibility of scripts
- ⏳ Document script functionality
- ⏳ Complete implementation estimate

**Recommendation:**
Once UserTalk scripts are accessible:
1. Review all 7 verb implementations
2. Verify no GUI dependencies
3. Test with real HTTP requests
4. Update this audit with script details
5. Proceed with kernel verb wrapper implementation

---

**Implementation Strategy:**
- **7 kernel verbs** (thin wrappers to UserTalk)
- **UserTalk scripts** contain actual logic
- **8-12 hours effort** (mostly wiring + testing)
- **HIGH priority** for web server functionality
- **Implement after tcp and inetd**

**Key Success Factors:**
1. Access to UserTalk script implementations
2. Proper integration with inetd/tcp
3. Comprehensive HTTP protocol handling
4. Security hardening (path traversal, input validation)
5. Performance optimization (keep-alive, caching)
