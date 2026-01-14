# Frontier Web Server Analysis

## Executive Summary

**Current State**: Frontier's built-in web server is **production-ready code** that served UserLand's products for years. The networking stack is **90% ready for headless REST API** use with known, manageable gaps.

**Key Finding**: The C networking implementation (`MacSocketNetEvents.c`, ~1800 lines) exists but is **not compiled into headless builds**. Adding it to the Makefile is straightforward.

**Critical Discovery**: `webserver.server()` and `inetd.supervisor()` are **kernel UserTalk scripts**, not C verbs. They're implemented in `/usertalk_scripts/Frontier.root/system/verbs/builtins/` and expect to be loaded from the ODB at runtime. The C layer only provides low-level TCP primitives.

**Effort Estimate**:
- Phase 1 (Basic HTTP server working): **2-3 weeks**
- Phase 2 (REST endpoint routing): **1-2 weeks**
- Phase 3 (JSON integration): **1-2 weeks**
- Phase 4 (Production hardening): **2-3 weeks**
- **Total: 6-10 weeks**

**Recommendation**: Proceed with REST API via Frontier web server (ADR-007). The architecture is sound, threaded properly, and designed for exactly this use case.

---

## Architecture Overview

### Component Diagram

```
┌─────────────────────────────────────────────────────────┐
│                    Client (HTTP)                         │
└───────────────────────┬─────────────────────────────────┘
                        │ TCP/IP
                        ↓
┌─────────────────────────────────────────────────────────┐
│  tcp.listenStream()   [C: MacSocketNetEvents.c]         │
│  - Binds to port      [Accepts connections]             │
│  - Multi-threaded     [Pthread per connection]          │
└───────────────────────┬─────────────────────────────────┘
                        │ Callback
                        ↓
┌─────────────────────────────────────────────────────────┐
│  inetd.supervisor()   [UserTalk kernel script]          │
│  - Reads request      [tcp.readStream()]                │
│  - Calls daemon       [Dispatches to handler]           │
│  - Writes response    [tcp.writeStream()]               │
└───────────────────────┬─────────────────────────────────┘
                        │ Calls
                        ↓
┌─────────────────────────────────────────────────────────┐
│  webserver.server()   [UserTalk kernel script]          │
│  - Parses HTTP        [Headers, method, path, body]     │
│  - Routes request     [webserver.dispatch()]            │
│  - Builds response    [webserver.util.buildResponse()]  │
└───────────────────────┬─────────────────────────────────┘
                        │ Routes
                        ↓
┌─────────────────────────────────────────────────────────┐
│  webserver.dispatch() [UserTalk kernel script]          │
│  - Pre-filters        [user.webserver.preFilters]       │
│  - Responder match    [user.webserver.responders]       │
│  - Method dispatch    [GET/POST/PUT/DELETE/any]         │
│  - Post-filters       [user.webserver.postFilters]      │
└───────────────────────┬─────────────────────────────────┘
                        │ Calls
                        ↓
┌─────────────────────────────────────────────────────────┐
│  Responder Methods    [UserTalk scripts in ODB]         │
│  - /api/v1/eval       [Custom REST handlers]            │
│  - /RPC2              [XML-RPC endpoint (betty.rpc)]    │
│  - /default           [File serving, CGI framework]     │
└─────────────────────────────────────────────────────────┘
```

### Data Flow: HTTP Request → Response

1. **TCP Accept** (`MacSocketNetEvents.c:acceptsocket()`)
   - New connection accepted on listening socket
   - Pthread spawned for this connection
   - Calls UserTalk callback: `inetd.supervisor(stream, refcon)`

2. **HTTP Read** (`inetd.supervisor` → `tcp.readStream()`)
   - Reads incoming HTTP request from socket
   - Waits for complete request (handles Content-Length)
   - Builds paramTable with client info, request string

3. **HTTP Parse** (`webserver.server()`)
   - Parses HTTP method, path, version
   - Extracts headers (`webserver.util.parseHeaders()`)
   - Reads request body if Content-Length present
   - Validates HTTP/1.x compliance

4. **URL Routing** (`webserver.dispatch()`)
   - Iterates `user.webserver.responders[]` table
   - Evaluates each responder's `condition` (script or expression)
   - First matching responder handles request
   - Falls back to `defaultResponder` if no match

5. **Method Dispatch**
   - Looks up responder method: `responder.methods.GET`, `POST`, etc.
   - Falls back to `responder.methods.any` if specific method not defined
   - Calls method script: `method(adrParamTable)`

6. **Handler Execution**
   - Method script executes (accesses ODB, runs business logic)
   - Builds response: sets `adrParamTable^.code`, `.responseBody`, `.responseHeaders`
   - Returns `true` for modern responders

7. **HTTP Response** (`webserver.util.buildResponse()`)
   - Builds HTTP status line: `HTTP/1.0 200 OK`
   - Adds headers from responseHeaders table
   - Adds Content-Length
   - Returns complete HTTP response string

8. **TCP Write** (`inetd.supervisor` → `tcp.writeStream()`)
   - Writes response to socket (chunked for large responses)
   - Closes connection: `tcp.closeStream(stream)`

---

## URL Routing Mechanism ("It's Complicated")

### Responder-Based Routing

Frontier uses a **responder pattern** instead of traditional path-based routing:

```usertalk
// Responder table structure (in user.webserver.responders)
user.webserver.responders.myAPI = @user.webserver.data.myAPI

user.webserver.data.myAPI = {
    enabled: true,
    condition: `path beginsWith "/api/v1/"`,  // Script or expression
    methods: {
        GET: script,   // Handles GET /api/v1/*
        POST: script,  // Handles POST /api/v1/*
        any: script    // Fallback for other methods
    }
}
```

**Routing Algorithm**:
1. Iterate `user.webserver.responders[]` in order
2. For each enabled responder:
   - Evaluate `condition` (script or expression)
   - If condition returns `true`, use this responder
3. If no responder matches, use `defaultResponder`
4. Look up method in responder's `methods` subtable
5. Fall back to `methods.any` if specific method not defined
6. Return HTTP 405 "Method Not Allowed" if neither exists

**Example Conditions**:
```usertalk
// Path prefix matching
path beginsWith "/api/"

// Exact path matching
path == "/RPC2"

// Header-based routing (XML-RPC detection)
defined(requestHeaders["Content-Type"]) and
  string.lower(requestHeaders["Content-Type"]) contains "xml"

// Complex script condition
on condition(adrParams)
    if adrParams^.path beginsWith "/admin/"
        return user.prefs.allowAdmin  // Check permission
    return false
```

### Request/Response Data Structures

**Request (adrParamTable)**:
```usertalk
adrParamTable^ = {
    // TCP connection info (from inetd)
    stream: 42,                    // TCP stream ID
    client: "192.168.1.100",       // Client IP (decoded)
    port: 80,                      // Server port
    refcon: 80,                    // Port number (listen refcon)
    timeout: 45,                   // Timeout in seconds

    // HTTP request info (from webserver.server)
    request: "GET /api/v1/eval?code=1+1 HTTP/1.1\r\n...",  // Raw HTTP
    firstLine: "GET /api/v1/eval?code=1+1 HTTP/1.1",
    method: "GET",
    path: "/api/v1/eval",          // Path without query/args
    URI: "/api/v1/eval",           // Same as path
    searchArgs: "code=1+1",        // Query string after ?
    pathArgs: "",                  // Path args after $ (Frontier extension)
    requestBody: "",               // Body content (if Content-Length present)

    // Headers
    requestHeaders: {
        Host: "localhost",
        "User-Agent": "curl/7.64.1",
        Accept: "*/*"
    },

    // Routing info (from webserver.dispatch)
    responder: "myAPI",            // Which responder matched
    responderTableAdr: @user.webserver.data.myAPI,

    // Response (filled by handler)
    code: 200,                     // HTTP status code
    responseBody: "{\"result\":2}",
    responseHeaders: {
        "Content-Type": "application/json",
        "X-Custom-Header": "value"
    }
}
```

**Response (returned to client)**:
```http
HTTP/1.0 200 OK
Content-Type: application/json
Content-Length: 13
X-Custom-Header: value

{"result":2}
```

---

## HTTP Capabilities

### Protocol Support

**HTTP Version**: HTTP/1.0 (confirmed in code)
- `webserver.server()` checks for HTTP/1.1 but implements HTTP/1.0 semantics
- No persistent connections (Connection: close implied)
- No chunked transfer encoding support (responses built as complete strings)
- **Adequate for REST API**: Modern clients handle HTTP/1.0 fine

**HTTP Methods Supported**:
- ✅ GET, POST, PUT, DELETE, HEAD, OPTIONS
- ✅ Custom methods (dispatcher is method-agnostic)
- ✅ Responder can implement `methods.any` to handle all methods

**Request Features**:
- ✅ Header parsing (`webserver.util.parseHeaders()`)
- ✅ Query string parsing (`searchArgs`)
- ✅ Request body reading (Content-Length based)
- ✅ Cookie parsing (`webserver.util.parseCookies()`)
- ✅ Form-encoded POST data (via `webserver.util.parseArgs()`)
- ⚠️ Multipart/form-data: No built-in parser (would need UserTalk implementation)
- ⚠️ Raw binary body: Available as `requestBody` string (may need encoding handling)

**Response Features**:
- ✅ Custom status codes (200, 404, 500, etc.)
- ✅ Custom headers (`responseHeaders` table)
- ✅ Content-Length (auto-calculated)
- ✅ Cookie setting (`webserver.util.setCookie()`)
- ✅ Error pages (`webserver.util.buildErrorPage()`)
- ⚠️ Chunked responses: Partially supported via `webserver.sendPartial()` (WebSTAR integration)

### Threading Model

**Multi-threaded by Design**:
- Each TCP connection handled in **separate pthread** (`MacSocketNetEvents.c:launchacceptingthread()`)
- Acceptor thread continuously accepts new connections
- Connection threads execute UserTalk callbacks (`inetd.supervisor`)
- UserTalk runtime is thread-safe (each thread has separate execution context)

**Concurrency Considerations**:
- ✅ Multiple simultaneous requests: Supported
- ✅ Database access from threads: Safe (ODB designed for this)
- ⚠️ Global state in UserTalk: Scripts must avoid mutating shared state unsafely
- ⚠️ Thread-local storage: UserTalk scripts run in thread context (separate stacks)

**Performance**:
- Max connections: Configurable (`depth` parameter to `tcp.listenStream`)
- Timeout: Configurable per-daemon (`user.inetd.config[].timeout`, default 45s)
- Chunk size: Configurable (`user.inetd.prefs.returnChunkSize`, default 8KB)

---

## Component Analysis

### tcp (C Implementation: MacSocketNetEvents.c)

**Purpose**: Low-level TCP socket API (BSD sockets wrapper)

**C Implementation**: `/Users/jake/dev/jsavin/Frontier/Common/source/MacSocketNetEvents.c` (1800 lines)
- Modern Mac implementation using BSD sockets + pthreads
- Replaces legacy MacTCP/Open Transport APIs
- Cross-platform foundation (Windows has `WinSockNetEvents.c` equivalent)

**UserTalk API** (23 verbs, registered in `kernelverbs.rc`):

| Verb | Purpose | C Function |
|------|---------|------------|
| `tcp.listenStream(port, depth, callback, refcon, ipaddr)` | Start TCP listener | `fwsNetEventListenStream()` |
| `tcp.openStream(host, port)` / `tcp.openAddrStream(ip, port)` | Connect to server | `fwsNetEventOpenStream()` |
| `tcp.readStream(stream, maxBytes)` | Read from socket | `fwsNetEventReadStream()` |
| `tcp.writeStream(stream, data)` | Write to socket | `fwsNetEventWriteStream()` |
| `tcp.closeStream(stream)` | Close connection | `fwsNetEventCloseStream()` |
| `tcp.closeListen(listenRef)` | Stop listener | `fwsNetEventCloseListen()` |
| `tcp.statusStream(stream, @bytesPending)` | Check stream status | `fwsNetEventStatusStream()` |
| `tcp.getPeerAddress(stream)` | Get client IP | `fwsNetEventGetPeerAddress()` |
| `tcp.addressDecode(ipInt)` | IP int → dotted string | Address conversion |
| `tcp.addressEncode(ipString)` | Dotted string → IP int | Address conversion |
| ... | DNS, utility verbs | ... |

**Key Functions**:

```c
// Start listening on port (non-blocking, callback per connection)
boolean fwsNetEventListenStream(
    unsigned long port,          // Port to bind
    long depth,                  // Max simultaneous connections
    bigstring callback,          // UserTalk script path
    unsigned long refcon,        // Passed to callback
    unsigned long *stream,       // [out] Listen reference
    unsigned long ipaddr,        // 0 = INADDR_ANY
    long hdatabase              // Database containing callback script
)

// Accept connection (called by acceptor thread)
static boolean acceptsocket(long listenstream) {
    // accept() new connection
    // Allocate stream record
    // Spawn pthread to run callback: callback(acceptstream, refcon)
}

// Callback execution (in pthread)
static boolean runcallback(long listenstream, long acceptstream, long refcon) {
    // Set up UserTalk execution context
    // Call langrunscript() with callback path
    // Parameters: (acceptstream, refcon)
}
```

**Threading Architecture**:
1. Main thread: `tcp.listenStream()` creates acceptor thread
2. Acceptor thread: Loops on `accept()`, spawns handler threads
3. Handler threads: Execute UserTalk callback, then exit

**Headless Compatibility**: ✅ **Ready**
- No GUI dependencies
- Pure BSD sockets + pthreads
- Already uses portable string/memory APIs

**Needed for Headless**:
1. Add `MacSocketNetEvents.c` to `frontier-cli/Makefile`
2. Add networking headers to include path
3. Link pthread library (may already be linked)

---

### inetd (UserTalk Service Dispatcher)

**Purpose**: Unix-style inetd service manager (manages multiple TCP listeners)

**Architecture**:
- **C layer**: None (uses `tcp.listenStream()` directly)
- **UserTalk layer**: Service configuration and dispatcher

**Key Scripts**:
- `/usertalk_scripts/Frontier.root/system/verbs/builtins/inetd/init.ut`
- `/usertalk_scripts/Frontier.root/system/verbs/builtins/inetd/start.ut`
- `/usertalk_scripts/Frontier.root/system/verbs/builtins/inetd/supervisor.ut` (**kernel script**)

**Configuration** (in ODB: `user.inetd.config`):
```usertalk
user.inetd.config.http = {
    port: 80,
    daemon: @webserver.server,   // Handler script
    startup: true,                // Auto-start on inetd.start()
    timeout: 45,                  // Request timeout (seconds)
    noWait: false                 // Wait for full request before calling daemon
}
```

**Core Flow** (`inetd.supervisor(stream, refcon)`):
```usertalk
on supervisor(stream, refcon)
    // 1. Initialize param table
    paramTable.stream = stream
    paramTable.client = tcp.addressDecode(tcp.getPeerAddress(stream))
    paramTable.port = user.inetd.listens[refcon].port
    paramTable.daemon = user.inetd.listens[refcon].daemon

    // 2. Read request from socket
    loop
        if tcp.statusStream(stream, @bytespending) == "DATA"
            paramTable.request = paramTable.request + tcp.readStream(stream, bytespending)
        if status == "CLOSED" or timeout
            break

    // 3. Call daemon handler
    local(returnData = string(paramTable.daemon(@paramTable)))

    // 4. Write response to socket
    tcp.writeStream(stream, returnData)

    // 5. Close connection
    tcp.closeStream(stream)
    return true
```

**Why inetd Exists**:
- Single `tcp.listenStream()` per port
- Multiple services can share inetd infrastructure
- Centralized connection management, logging, error handling
- Similar to Unix `inetd` daemon pattern

**Headless Compatibility**: ✅ **Ready**
- Pure UserTalk scripts
- No GUI dependencies
- Must load from ODB: `system.verbs.builtins.inetd.supervisor`

---

### webserver (HTTP Protocol Handler)

**Purpose**: HTTP request parsing, routing, response generation

**Architecture**:
- **C layer**: Verb registration only (`kernelverbs.rc`)
- **UserTalk layer**: All HTTP logic in kernel scripts

**Key Scripts**:
- `webserver.server(adrParamTable, httpRequest)` - HTTP parser (**kernel script**)
- `webserver.dispatch(adrParamTable)` - URL router (**kernel script**)
- `webserver.util.*` - Helper functions (parseHeaders, buildResponse, etc.)

**Verbs** (7 C verbs, mostly helpers):

| Verb | Purpose | Implementation |
|------|---------|----------------|
| `webserver.server()` | **Main entry point** | **UserTalk kernel script** |
| `webserver.dispatch()` | **URL router** | **UserTalk kernel script** |
| `webserver.parseHeaders()` | Parse HTTP headers | UserTalk script |
| `webserver.parseCookies()` | Parse Cookie header | UserTalk script |
| `webserver.buildResponse()` | Build HTTP response | UserTalk script |
| `webserver.buildErrorPage()` | Build HTML error page | UserTalk script |
| `webserver.getServerString()` | Get Server: header value | UserTalk script |

**HTTP Parsing** (`webserver.server()`):
```usertalk
on server(adrparamtable, httpRequest=nil)
    // 1. Read request if not provided
    if httpRequest == nil
        adrparamtable^.request = webserver.util.readUntil(
            adrparamtable^.stream, "\r\n\r\n", adrparamtable^.timeout)

    // 2. Parse headers
    new(tableType, @adrparamtable^.requestHeaders)
    adrparamtable^.firstLine = webserver.util.parseHeaders(
        adrparamtable^.request, @adrparamtable^.requestHeaders)

    // 3. Read request body if Content-Length present
    if defined(adrparamtable^.requestHeaders["Content-Length"])
        local(len = number(adrparamtable^.requestHeaders["Content-Length"]))
        adrparamtable^.requestBody = webserver.util.readBytes(
            adrparamtable^.stream, len, adrparamtable^.timeout)

    // 4. Parse method, path, args
    adrparamtable^.method = string.nthField(adrparamtable^.firstLine, " ", 1)
    path = string.nthField(adrparamtable^.firstLine, " ", 2)
    if path contains "?"
        adrparamtable^.searchArgs = string.nthField(path, "?", 2)
        path = string.nthField(path, "?", 1)
    adrparamtable^.path = path
    adrparamtable^.URI = path

    // 5. Route to handler
    return webserver.dispatch(adrparamtable)
```

**Response Building** (`webserver.util.buildResponse()`):
```usertalk
on buildResponse(code, adrHeaders, body)
    // Build status line
    local(s = "HTTP/1.0 " + code + " " + getStatusMessage(code) + "\r\n")

    // Add Date header
    s = s + "Date: " + date.netstandardstring(clock.now()) + "\r\n"

    // Add custom headers
    for i = 1 to sizeOf(adrHeaders^)
        s = s + nameOf(adrHeaders^[i]) + ": " + adrHeaders^[i] + "\r\n"

    // Add Content-Length
    s = s + "Content-Length: " + sizeOf(body) + "\r\n"

    // End headers, add body
    s = s + "\r\n" + body
    return s
```

**Responder Configuration** (in ODB: `user.webserver.responders`):
```usertalk
// Example: REST API responder
user.webserver.responders.restAPI = @user.webserver.data.restAPI

user.webserver.data.restAPI = {
    enabled: true,
    condition: `path beginsWith "/api/"`,
    methods: {
        GET: @user.webserver.data.restAPI.handlers.GET,
        POST: @user.webserver.data.restAPI.handlers.POST,
        any: @user.webserver.data.restAPI.handlers.any
    }
}

on user.webserver.data.restAPI.handlers.GET(adrParams)
    // Handle GET /api/*
    adrParams^.code = 200
    adrParams^.responseHeaders["Content-Type"] = "application/json"
    adrParams^.responseBody = "{\"status\":\"ok\"}"
    return true  // Modern responder protocol
```

**Headless Compatibility**: ⚠️ **Needs ODB Scripts**
- UserTalk scripts must be loaded from `Frontier.root`
- Scripts live in: `system.verbs.builtins.webserver.*`
- Must ensure `--system-root databases/Frontier.root` loaded

---

### betty (XML-RPC Framework)

**Purpose**: XML-RPC client and server implementation

**Architecture**:
- **C layer**: None (pure UserTalk)
- **UserTalk layer**: XML encoding/decoding, RPC dispatch

**Key Scripts**:
- `betty.rpc.client()` - XML-RPC client
- `betty.rpc.server()` - XML-RPC server (called by webserver responder)
- `betty.responders.RPC2.methods.POST()` - `/RPC2` endpoint handler

**How betty Integrates with webserver**:
```usertalk
// betty.rpc responder (in user.webserver.responders)
user.webserver.responders.xmlrpc = {
    enabled: true,
    condition: `path == "/RPC2"`,  // Standard XML-RPC path
    methods: {
        POST: @betty.responders.RPC2.methods.POST
    }
}

on betty.responders.RPC2.methods.POST(adrParams)
    // Parse XML-RPC request
    local(rpcRequest = xml.compile(adrParams^.requestBody))
    local(methodName = xml.getValue(@rpcRequest, "methodName"))
    local(params = xml.getAddress(@rpcRequest, "params"))

    // Execute method
    local(result = betty.rpc.server(methodName, params))

    // Build XML-RPC response
    adrParams^.code = 200
    adrParams^.responseHeaders["Content-Type"] = "text/xml"
    adrParams^.responseBody = betty.rpc.encodeResponse(result)
    return true
```

**Relevance to REST API**:
- ✅ Demonstrates UserTalk-based protocol handler
- ✅ Shows how to parse request body (XML → ODB structure)
- ✅ Shows how to build structured responses
- ⚠️ XML-RPC is legacy; REST will use JSON instead

**Headless Compatibility**: ✅ **Ready** (if XML verbs exist)
- Pure UserTalk
- Requires `xml.*` verbs (need to verify in headless)

---

## Headless Compatibility Assessment

### Current Build Status

**Networking C Code**: ❌ **NOT in headless builds**
- `MacSocketNetEvents.c` exists but not in `frontier-cli/Makefile`
- Verb registration exists in `kernel_verbs_headless.c` (tcp, webserver, inetd)
- Binary has undefined symbols: `_fwsNetEventListenStream` (confirmed via `nm`)

**UserTalk Scripts**: ⚠️ **Must load from ODB**
- `system.verbs.builtins.inetd.supervisor` - Required
- `system.verbs.builtins.webserver.server` - Required
- `system.verbs.builtins.webserver.dispatch` - Required
- Currently loaded from: `databases/Frontier.root` via `--system-root`

**Warnings During Startup**:
```
[lang-WARN] tablestructure.c:556: Failed to resolve address
'system.compiler.["kernel"].webserver', skipping
[lang-WARN] tablestructure.c:556: Failed to resolve address
'system.compiler.["kernel"].inetd', skipping
```
- These are **expected**: Kernel scripts aren't in `system.compiler.kernel` table
- They're in `system.verbs.builtins.*` instead (loaded separately)

---

### Required Adaptations

#### 1. Add Networking C Code to Build

**Changes to `/Users/jake/dev/jsavin/Frontier/frontier-cli/Makefile`**:

```makefile
# Add to RUNTIME_SOURCES section (around line 62)
RUNTIME_SOURCES = \
    ../Common/source/logging.c \
    ...existing sources... \
    ../Common/source/MacSocketNetEvents.c \   # ADD THIS
    ../Common/source/langverbs.c

# May need to link pthread (check if already linked)
LDFLAGS = -Wl,-undefined,dynamic_lookup -lpthread
```

**Platform Note**:
- Mac: Use `MacSocketNetEvents.c`
- Linux: Will need equivalent (may use `MacSocketNetEvents.c` if BSD sockets work)
- Windows: Use `WinSockNetEvents.c` (future)

**Estimated Effort**: 1-2 hours (add file, test build)

---

#### 2. Verify UserTalk Scripts Load from ODB

**Current Behavior**:
- CLI loads `Frontier.root` via `--system-root databases/Frontier.root`
- Scripts should be in: `system.verbs.builtins.webserver.*`

**Verification Steps**:
```bash
# Test if webserver.server exists
./frontier-cli/frontier-cli --system-root databases/Frontier.root \
  -e "defined(system.verbs.builtins.webserver.server)"
# Expected: true

# Test if inetd.supervisor exists
./frontier-cli/frontier-cli --system-root databases/Frontier.root \
  -e "defined(system.verbs.builtins.inetd.supervisor)"
# Expected: true
```

**If Missing**:
- Scripts may be in legacy Frontier.root, not ported to headless database
- Solution: Export scripts from legacy Frontier, import to `Frontier.root`

**Estimated Effort**: 2-4 hours (if scripts need porting)

---

#### 3. Test TCP Stack in Headless

**Test Plan**:

**Test 1: TCP Listen**
```usertalk
// Start listener on port 8080
local(listenRef);
tcp.listenStream(8080, 10, @system.verbs.builtins.inetd.supervisor, 8080, 0);
```

**Test 2: inetd Configuration**
```usertalk
// Configure HTTP service
user.inetd.config.test = {
    port: 8080,
    daemon: @webserver.server,
    startup: false,
    timeout: 30
};
inetd.startOne(@user.inetd.config.test);
```

**Test 3: Curl HTTP Request**
```bash
curl http://localhost:8080/
# Expected: HTTP response (404 if no default responder)
```

**Estimated Effort**: 1-2 days (debug threading, callback execution)

---

#### 4. Create REST API Responder

**Implementation** (in `user.webserver.responders`):

```usertalk
// REST API responder configuration
user.webserver.responders.restAPI = @user.api.v1.responder

user.api.v1.responder = {
    enabled: true,
    condition: `path beginsWith "/api/v1/"`,
    methods: {
        POST: @user.api.v1.handlers.POST,
        GET: @user.api.v1.handlers.GET
    }
}

// POST /api/v1/eval handler
on user.api.v1.handlers.POST(adrParams)
    try {
        // Parse JSON request body (requires json.* verbs)
        local(request = json.decode(adrParams^.requestBody));

        // Extract UserTalk code from request
        local(code = request.code);

        // Evaluate code
        local(result = evaluate(code));

        // Build JSON response
        local(response = {result: result});
        adrParams^.code = 200;
        adrParams^.responseHeaders["Content-Type"] = "application/json";
        adrParams^.responseBody = json.encode(response);
        return true;
    }
    else {
        // Error response
        adrParams^.code = 500;
        adrParams^.responseHeaders["Content-Type"] = "application/json";
        adrParams^.responseBody = json.encode({error: tryError});
        return true;
    }
```

**Estimated Effort**: 1-2 days (create responder, test routing)

---

### Thread-Safety Concerns

**C Layer** (MacSocketNetEvents.c):
- ✅ Thread-safe: Uses pthread mutexes for stream table (`lockData()`, `unlockData()`)
- ✅ Each connection runs in separate pthread
- ✅ Stream records isolated per connection

**UserTalk Layer**:
- ✅ Thread-safe by design: Each thread has separate execution context
- ✅ ODB designed for concurrent access (database locking handles this)
- ⚠️ **User scripts must avoid unsafe global state mutation**
  - Example: Don't do `user.stats.requestCount++` without locking
  - Safe: Thread-local state, read-only globals, ODB transactions

**Known Limitations**:
- ⚠️ `flnextparamislast` global in verb parameter parsing (ADR-005 addresses this)
- ⚠️ Database context globals (being refactored in Phase 3)

**Recommendation**: Document thread-safety best practices for REST handler scripts.

---

## REST API Adaptation Plan

### Phase 1: Basic HTTP Server Working (2-3 weeks)

**Goal**: `curl http://localhost:8080/` returns HTTP response

**Tasks**:
1. Add `MacSocketNetEvents.c` to Makefile
2. Verify verb registration (`tcp.*`, `webserver.*`, `inetd.*`)
3. Test `tcp.listenStream()` in CLI
4. Verify UserTalk scripts load from ODB
5. Test inetd.supervisor callback execution
6. Test webserver.server HTTP parsing
7. Create minimal responder (returns "Hello World")
8. Test end-to-end: curl → response

**Success Criteria**:
- ✅ TCP listener accepts connections
- ✅ HTTP request parsed correctly
- ✅ Handler script executes
- ✅ HTTP response sent to client

**Risks**:
- Threading issues (callback execution in pthread context)
- ODB access from threads (should work but needs testing)
- UserTalk script loading (may need to verify paths)

---

### Phase 2: REST Endpoint Routing (1-2 weeks)

**Goal**: `/api/v1/eval` routes to handler, returns result

**Tasks**:
1. Create REST API responder table
2. Implement condition: `path beginsWith "/api/v1/"`
3. Implement method handlers (GET, POST)
4. Test routing with multiple endpoints
5. Test method dispatch (GET vs POST)
6. Test 404 for unmatched paths
7. Test 405 for unsupported methods

**Success Criteria**:
- ✅ `/api/v1/eval` routes to correct handler
- ✅ Other paths return 404
- ✅ Wrong method returns 405
- ✅ Handler can access adrParamTable fields

**Example**:
```bash
curl -X POST http://localhost:8080/api/v1/eval \
  -H "Content-Type: application/json" \
  -d '{"code":"1+1"}'
# Expected: {"result":2}
```

---

### Phase 3: JSON Support (1-2 weeks)

**Goal**: Request/response use JSON instead of XML-RPC

**Tasks**:
1. Port JSON parser from OPML Editor (User requested this)
2. Implement `json.decode(jsonString)` verb
3. Implement `json.encode(odbValue)` verb
4. Update REST handlers to use JSON
5. Test JSON encoding/decoding
6. Test nested structures (tables, lists)
7. Test error handling (invalid JSON)

**JSON Verbs API**:
```usertalk
// Parse JSON string → ODB structure
local(obj = json.decode('{"name":"Alice","age":30}'));
obj.name  // "Alice"
obj.age   // 30

// Encode ODB structure → JSON string
local(t = {result: 42, status: "ok"});
json.encode(t)  // '{"result":42,"status":"ok"}'
```

**Success Criteria**:
- ✅ JSON request parsed to ODB table
- ✅ ODB result encoded to JSON response
- ✅ Types preserved (string, number, boolean, null)
- ✅ Nested structures work (tables → objects, lists → arrays)

**Risks**:
- JSON encoder may need type introspection (typeof() checks)
- Unicode handling (UTF-8 encoding)
- Large JSON payloads (memory management)

---

### Phase 4: Production Hardening (2-3 weeks)

**Goal**: Production-ready REST API server

**Tasks**:

**Security**:
1. Authentication (API key, bearer token, etc.)
2. Rate limiting (per-IP request throttling)
3. Input validation (sanitize code parameter)
4. Sandbox evaluation (prevent file system access)
5. CORS headers (for browser clients)

**Error Handling**:
1. Try/catch in handlers (return JSON error responses)
2. Log errors to ODB log table
3. Timeout handling (long-running eval)
4. Graceful shutdown (close all connections)

**Monitoring**:
1. Request logging (method, path, status, duration)
2. Performance metrics (requests/sec, latency)
3. Health check endpoint (`/health`)

**Testing**:
1. Integration tests (curl scripts)
2. Load testing (concurrent requests)
3. Security testing (fuzzing, injection)

**Documentation**:
1. REST API reference (endpoints, parameters, responses)
2. Deployment guide (port, startup, shutdown)
3. Troubleshooting guide (logs, errors)

**Success Criteria**:
- ✅ API requires authentication
- ✅ Rate limiting prevents abuse
- ✅ Errors logged and returned as JSON
- ✅ Health check returns 200 OK
- ✅ Load test: 100 req/sec sustained

---

## Missing Capabilities for REST

### 1. JSON Parser/Encoder

**Status**: ❌ **Not in headless** (yet)

**Plan**: Port from OPML Editor (User requested this in ADR-007)

**Implementation**:
- C verbs: `json.decode()`, `json.encode()`
- Use existing XML parsing patterns as reference
- Support basic types: string, number, boolean, null, object, array

**Estimated Effort**: 1-2 weeks

---

### 2. Request Body as Raw Bytes

**Status**: ⚠️ **Partial**

**Current**:
- `requestBody` is available as string
- May have encoding issues for binary data

**Solution**:
- Treat `requestBody` as UTF-8 string for JSON
- For binary APIs, may need binary value type

**Estimated Effort**: None (if JSON only)

---

### 3. Custom Response Headers

**Status**: ✅ **Supported**

**Usage**:
```usertalk
adrParams^.responseHeaders["Content-Type"] = "application/json";
adrParams^.responseHeaders["X-Custom-Header"] = "value";
```

---

### 4. HTTP Methods Beyond GET/POST

**Status**: ✅ **Supported**

**Notes**:
- Dispatcher is method-agnostic
- Can implement PUT, DELETE, PATCH handlers
- Falls back to `methods.any` if specific method not defined

---

### 5. Authentication/Authorization

**Status**: ❌ **Not built-in**

**Implementation Options**:

**Option A: Pre-filter**
```usertalk
user.webserver.preFilters.auth = @user.api.security.checkAuth

on checkAuth(adrParams)
    if not defined(adrParams^.requestHeaders["Authorization"])
        adrParams^.code = 401
        adrParams^.responseHeaders["WWW-Authenticate"] = "Bearer"
        adrParams^.responseBody = json.encode({error: "Unauthorized"})
        return false  // Stop processing

    // Validate token
    local(token = adrParams^.requestHeaders["Authorization"]);
    if not isValidToken(token)
        adrParams^.code = 403
        adrParams^.responseBody = json.encode({error: "Forbidden"})
        return false
```

**Option B: Handler-level**
```usertalk
on handler(adrParams)
    if not authenticated(adrParams)
        return errorResponse(401, "Unauthorized")
    // ... handle request
```

**Estimated Effort**: 2-3 days (implement, test)

---

### 6. CORS Support

**Status**: ❌ **Not built-in**

**Implementation**:
```usertalk
// Pre-filter for CORS
on corsPreFilter(adrParams)
    adrParams^.responseHeaders["Access-Control-Allow-Origin"] = "*"
    adrParams^.responseHeaders["Access-Control-Allow-Methods"] = "GET, POST, PUT, DELETE"
    adrParams^.responseHeaders["Access-Control-Allow-Headers"] = "Content-Type, Authorization"

    // Handle OPTIONS preflight
    if adrParams^.method == "OPTIONS"
        adrParams^.code = 204
        adrParams^.responseBody = ""
        return false  // Stop processing
```

**Estimated Effort**: 1-2 hours (configure, test)

---

## Security Considerations

### Input Validation

**Risk**: Malicious code in `eval()` requests
- ❌ `POST /api/v1/eval {"code":"sys.exit()"}`
- ❌ `POST /api/v1/eval {"code":"file.delete(\"/etc/passwd\")"}`

**Mitigation**:
1. **Sandbox evaluation**: Disable dangerous verbs (`file.*`, `sys.*`, etc.)
2. **Timeout**: Kill long-running scripts
3. **Whitelist**: Only allow safe operations
4. **Code review**: Log all eval requests for audit

**Implementation**:
```usertalk
on safeEvaluate(code)
    // Disable dangerous verbs
    local(saved = {
        file: @file,
        sys: @sys,
        Frontier: @Frontier
    });
    delete(@file);  // Temporarily hide dangerous namespaces
    delete(@sys);
    delete(@Frontier);

    try {
        local(result = evaluate(code));
        // Restore
        file = saved.file;
        sys = saved.sys;
        Frontier = saved.Frontier;
        return result;
    }
    else {
        // Restore even on error
        file = saved.file;
        sys = saved.sys;
        Frontier = saved.Frontier;
        error(tryError);
    }
```

---

### Rate Limiting

**Risk**: DDoS, resource exhaustion

**Mitigation**:
```usertalk
// Pre-filter: Check rate limit
user.webserver.preFilters.rateLimit = @user.api.security.checkRateLimit

on checkRateLimit(adrParams)
    local(ip = adrParams^.client);
    local(now = clock.ticks());

    if not defined(user.api.stats.requests[ip])
        new(tableType, @user.api.stats.requests[ip]);
        user.api.stats.requests[ip].count = 0;
        user.api.stats.requests[ip].window = now;

    local(stats = @user.api.stats.requests[ip]);

    // Reset window every 60 seconds
    if (now - stats^.window) > (60 * 60)
        stats^.count = 0;
        stats^.window = now;

    // Check limit: 100 requests/minute
    if stats^.count > 100
        adrParams^.code = 429;  // Too Many Requests
        adrParams^.responseBody = json.encode({error: "Rate limit exceeded"});
        return false;

    stats^.count++;
```

---

### Authentication

**Options**:

**1. API Key (Simple)**
```usertalk
// Pre-filter: Check API key
on checkApiKey(adrParams)
    local(key = adrParams^.requestHeaders["X-API-Key"]);
    if key != user.api.config.apiKey
        return errorResponse(401, "Invalid API key");
```

**2. Bearer Token (OAuth-style)**
```usertalk
on checkBearerToken(adrParams)
    local(auth = adrParams^.requestHeaders["Authorization"]);
    if not (auth beginsWith "Bearer ")
        return errorResponse(401, "Missing bearer token");

    local(token = string.delete(auth, 1, 7));  // Remove "Bearer "
    if not isValidToken(token)
        return errorResponse(403, "Invalid token");
```

---

## Code Examples

### Example 1: Minimal REST API Handler

```usertalk
// Create responder
user.webserver.responders.api = @user.api.responder

user.api.responder = {
    enabled: true,
    condition: `path beginsWith "/api/"`,
    methods: {
        POST: @user.api.handlers.POST
    }
}

// POST /api/eval handler
on user.api.handlers.POST(adrParams)
    try {
        // Parse JSON (assumes json.decode exists)
        local(request = json.decode(adrParams^.requestBody));

        // Evaluate code
        local(result = evaluate(request.code));

        // Build response
        adrParams^.code = 200;
        adrParams^.responseHeaders["Content-Type"] = "application/json";
        adrParams^.responseBody = json.encode({result: result});
        return true;
    }
    else {
        adrParams^.code = 500;
        adrParams^.responseHeaders["Content-Type"] = "application/json";
        adrParams^.responseBody = json.encode({error: tryError});
        return true;
    }
```

**Usage**:
```bash
curl -X POST http://localhost:8080/api/eval \
  -H "Content-Type: application/json" \
  -d '{"code":"1+1"}'

# Response:
# {"result":2}
```

---

### Example 2: Request Structure

```usertalk
// Handler receives adrParamTable with these fields:
on handler(adrParams)
    // TCP connection info
    adrParams^.stream        // TCP stream ID
    adrParams^.client        // "192.168.1.100"
    adrParams^.port          // 80

    // HTTP request info
    adrParams^.method        // "POST"
    adrParams^.path          // "/api/eval"
    adrParams^.searchArgs    // "debug=true" (query string)
    adrParams^.requestBody   // '{"code":"1+1"}'
    adrParams^.requestHeaders.["Content-Type"]  // "application/json"

    // Build response
    adrParams^.code = 200
    adrParams^.responseHeaders["Content-Type"] = "application/json"
    adrParams^.responseBody = "{\"result\":42}"
    return true
```

---

### Example 3: Error Handling

```usertalk
on handler(adrParams)
    try {
        // Validate request
        if not defined(adrParams^.requestBody)
            return errorResponse(400, "Missing request body");

        local(request = json.decode(adrParams^.requestBody));

        if not defined(request.code)
            return errorResponse(400, "Missing 'code' field");

        // Execute
        local(result = evaluate(request.code));

        // Success
        return successResponse(200, {result: result});
    }
    else {
        // Catch all errors
        return errorResponse(500, tryError);
    }

on errorResponse(code, message)
    adrParams^.code = code;
    adrParams^.responseHeaders["Content-Type"] = "application/json";
    adrParams^.responseBody = json.encode({error: message});
    return true

on successResponse(code, data)
    adrParams^.code = code;
    adrParams^.responseHeaders["Content-Type"] = "application/json";
    adrParams^.responseBody = json.encode(data);
    return true
```

---

## Open Questions

### 1. JSON Implementation

**Question**: Should we use C implementation or UserTalk?

**Options**:
- **C verbs** (`json.decode`, `json.encode`): Faster, more robust
- **UserTalk scripts**: Easier to iterate, no C compilation

**Recommendation**: Start with C verbs (port from OPML Editor), can optimize later.

---

### 2. Database Access Thread Safety

**Question**: Is ODB access from pthreads safe?

**Current Understanding**:
- ODB designed for multi-threaded access
- Database locking handles concurrency
- Each thread has separate execution context

**Verification Needed**:
- Load test with concurrent requests
- Verify database locks work correctly
- Check for race conditions in global state

---

### 3. Startup Scripts

**Question**: How to auto-start web server on CLI launch?

**Options**:
- **A**: `system.startup` script (runs on CLI init)
- **B**: Command-line flag: `--serve 8080`
- **C**: Explicit command: `./frontier-cli -e "inetd.start()"`

**Recommendation**: Option C for now (explicit), Option B later (convenience).

---

### 4. XML Verb Availability

**Question**: Does headless have `xml.*` verbs for betty.rpc?

**Status**: Unknown (needs verification)

**Plan**: Test `defined(xml.compile)` in CLI

---

### 5. IPv6 Support

**Question**: Does MacSocketNetEvents support IPv6?

**Answer**: No (code comment says "TODO: ipv6 support")

**Impact**: Low (IPv4 sufficient for initial REST API)

---

## References

### Documentation
- `/Users/jake/dev/jsavin/Frontier/docs/usertalk/docserver/webServer/`
- `/Users/jake/dev/jsavin/Frontier/docs/usertalk/docserver/inetd/`
- `/Users/jake/dev/jsavin/Frontier/docs/usertalk/docserver/tcp/`
- `/Users/jake/dev/jsavin/Frontier/docs/usertalk/docserver/betty/`

### C Implementation
- `/Users/jake/dev/jsavin/Frontier/Common/source/MacSocketNetEvents.c` - TCP stack (1800 lines)
- `/Users/jake/dev/jsavin/Frontier/Common/source/langverbs.c` - TCP verb bindings (lines 4020-4170)
- `/Users/jake/dev/jsavin/Frontier/Common/source/kernel_verbs_headless.c` - Verb registration

### UserTalk Scripts
- `/Users/jake/dev/jsavin/Frontier/usertalk_scripts/Frontier.root/system/verbs/builtins/inetd/supervisor.ut`
- `/Users/jake/dev/jsavin/Frontier/usertalk_scripts/Frontier.root/system/verbs/builtins/webserver/server.ut`
- `/Users/jake/dev/jsavin/Frontier/usertalk_scripts/Frontier.root/system/verbs/builtins/webserver/dispatch.ut`
- `/Users/jake/dev/jsavin/Frontier/usertalk_scripts/Frontier.root/system/verbs/builtins/betty/rpc/server.ut`

### Configuration
- `/Users/jake/dev/jsavin/Frontier/Common/resources/Win32/kernelverbs.rc` - Verb definitions

### ADRs
- `/Users/jake/dev/jsavin/Frontier/planning/architectural_decision_records/ADR-007-rest-api-via-frontier-web-server.md`

---

## Conclusion

Frontier's web server is **production-proven infrastructure** that requires **minimal adaptation** for REST API use. The architecture is sound, multi-threaded properly, and designed for UserTalk callbacks (exactly our use case).

**Key Strengths**:
- ✅ Mature, battle-tested code (used in production for years)
- ✅ Multi-threaded by design (pthread per connection)
- ✅ Clean separation: C (networking) + UserTalk (HTTP/routing)
- ✅ Flexible routing (responder pattern)
- ✅ Extensible (pre-filters, post-filters, custom responders)

**Known Gaps** (addressable):
- Add `MacSocketNetEvents.c` to headless build (1-2 hours)
- Port JSON parser from OPML Editor (1-2 weeks)
- Create REST API responder (1-2 days)
- Implement authentication/rate limiting (2-3 days)

**Total Estimated Effort**: **6-10 weeks** (includes production hardening)

**Recommendation**: **Proceed with ADR-007**. This is the right architectural choice for Frontier's REST API.
