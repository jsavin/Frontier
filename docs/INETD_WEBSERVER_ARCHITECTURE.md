# Frontier inetd and Webserver Architecture

This document explains how Frontier's built-in networking stack works, from TCP socket handling through HTTP request/response processing.

## Overview

Frontier implements a layered networking architecture:

```
┌─────────────────────────────────────────────────────────────────┐
│                        User Application                          │
│                  (CGI scripts, responders, handlers)             │
├─────────────────────────────────────────────────────────────────┤
│                      webserver.* verbs                           │
│        (dispatch, handler, server, responders, filters)          │
├─────────────────────────────────────────────────────────────────┤
│                        inetd.* verbs                             │
│           (supervisor, listeners, daemon routing)                │
├─────────────────────────────────────────────────────────────────┤
│                        tcp.* verbs                               │
│      (listenStream, readStream, writeStream, statusStream)       │
├─────────────────────────────────────────────────────────────────┤
│                     POSIX BSD Sockets                            │
│               (socket, bind, listen, accept, etc.)               │
└─────────────────────────────────────────────────────────────────┘
```

## Layer 1: TCP Verbs (Kernel Level)

The TCP verbs provide raw socket operations implemented in C (`tcpverbs.c`). These are the foundation for all networking.

**Key verbs:**
- `tcp.listenStream(port, depth, callback, refcon, bindAddr)` - Start listening on a port
- `tcp.closeListen(listenId)` - Stop a listener
- `tcp.readStream(stream, bytes)` - Read bytes from connection
- `tcp.writeStream(stream, data)` - Write data to connection
- `tcp.statusStream(stream, @bytesPending)` - Check connection status ("DATA", "OPEN", "CLOSED", etc.)
- `tcp.readStreamUntil(stream, pattern, timeout, @buffer)` - Read until pattern found
- `tcp.closeStream(stream)` - Close connection gracefully

## Layer 2: inetd (Internet Daemon)

inetd is Frontier's generic internet services daemon, inspired by Unix inetd. It manages multiple listeners and routes incoming connections to appropriate handlers.

### Initialization (`inetd.init`)

Creates the `user.inetd` table structure:

```
user.inetd
├── config/           # Daemon configurations (http, http2, etc.)
├── listens/          # Active listener references
├── prefs/
│   ├── returnChunkSize = 8192    # Chunk size for responses
│   └── defaultTimeoutSecs = 45   # Default connection timeout
└── shutdown = false  # Global shutdown flag
```

### Starting Listeners (`inetd.start`, `inetd.startOne`)

`inetd.start()` iterates through `user.inetd.config` and starts each daemon that has `startup = true`.

`inetd.startOne(daemonTableAdr)` starts a single listener:

```usertalk
tcp.listenStream(port, count, @inetd.supervisor, port, addr)
```

The key insight: **`inetd.supervisor` is the callback for ALL inetd listeners.** The `refcon` parameter (the port number) is used to look up the specific daemon configuration.

### The Supervisor (`inetd.supervisor`)

This is the core routing function. When a connection arrives:

1. **Initialize paramTable** with stream info, client IP, refcon
2. **Look up daemon config** via `user.inetd.listens.[refcon].adrTable`
3. **Read request data** in a loop using `tcp.statusStream` and `tcp.readStream`
4. **Call the daemon script** specified in the config
5. **Write response** back to the client in chunks
6. **Close the stream**

The supervisor is implemented as a kernel verb in Frontier 6.1 for performance, but the UserTalk version in the comments shows the logic.

### Daemon Configuration

Each daemon in `user.inetd.config` is a table:

```
user.inetd.config.http
├── port = 80              # Port to listen on
├── count = 50             # Backlog depth
├── daemon = @webserver.server   # Script to call
├── startup = true         # Auto-start on Frontier launch
├── timeout = 45           # Connection timeout
├── noWait = false         # If true, don't wait for full request
└── ip = 0                 # Bind address (0 = all interfaces)
```

## Layer 3: Webserver

The webserver is the HTTP-specific layer built on top of inetd. It understands HTTP protocol, dispatches to responders, and handles filters.

### Initialization (`webserver.init`)

1. Calls `inetd.init()` first
2. Creates `user.inetd.config.http` and `user.inetd.config.http2` if not defined
3. Sets up `user.webserver` table structure:

```
user.webserver
├── prefs/
│   ├── defaultResponder = "default"
│   ├── fldebug = false
│   ├── hostName = ""
│   ├── chunkSize = 24        # KB for chunked responses
│   ├── flStats = true
│   └── ...
├── callbacks/
│   ├── filterRequest/        # Pre-dispatch filters
│   ├── handleRequest/        # Request interceptors
│   └── filterPage/           # Post-render filters
├── responders/               # Responder table references
├── preFilters/               # Pre-processing filters
├── postFilters/              # Post-processing filters
├── actions/                  # Action scripts
├── cgis/                     # CGI scripts
└── stats/                    # Runtime statistics
```

### Request Flow (`webserver.server`)

Entry point for HTTP requests. Called by inetd.supervisor.

1. **Wait for startup** (optional) - If `flWaitDuringStartup` is true, wait until Frontier finishes starting
2. **Read HTTP request** using `webserver.util.readUntil(stream, "\r\n\r\n", timeout)`
3. **Parse headers** into `adrParamTable.requestHeaders`
4. **Read body** if `Content-Length` header present
5. **Validate request** (check HTTP version, Host header for HTTP/1.1, etc.)
6. **Parse URL** - Extract `searchArgs` (query string), `pathArgs` ($ parameters)
7. **Parse cookies** via `webserver.util.parseCookies`
8. **Call dispatcher** - `webserver.dispatch(adrParamTable)`
9. **Update stats** if enabled
10. **Return response**

### Dispatch (`webserver.dispatch`)

Routes requests to the appropriate responder:

1. **Run pre-filters** - Each script in `user.webserver.preFilters` gets a chance to modify the request
2. **Find matching responder** - Iterate through `user.webserver.responders`, evaluate each responder's `condition` (can be a script or expression)
3. **Initialize response fields** - Set `code = 200`, empty `responseBody` and `responseHeaders`
4. **Call responder method** - Find method handler (GET, POST, etc.) or fall back to `any`
5. **Run post-filters** - Each script in `user.webserver.postFilters` gets a chance to modify the response
6. **Build response** - Call `webserver.util.buildResponse(code, headers, body)`

### Responders

Responders are modular request handlers. Each responder has:

```
user.webserver.responders.{name}
├── condition       # When to use this responder (script or expression)
├── enabled = true  # Whether responder is active
├── data/           # Responder-specific data
└── methods/
    ├── GET.ut      # Handle GET requests
    ├── POST.ut     # Handle POST requests
    ├── HEAD.ut     # Handle HEAD requests
    └── any.ut      # Fallback for any method
```

**Built-in responders:**
- `default` - Serves files from ODB or filesystem
- `echo` - Debug responder that echoes request parameters
- `admin` - Administrative interface
- `server` - Server OPTIONS handling
- `websiteFramework` - Manila/website framework integration
- `wormDefense` - Blocks common worm/exploit requests

**Responder condition examples:**
```usertalk
// Match specific path
adrParamTable^.path beginsWith "/admin"

// Match by host header
adrParamTable^.requestHeaders.Host == "api.example.com"

// Script-based condition
on condition(pta) {
    return (pta^.path contains "/api/")
}
```

### Filters

**Pre-filters** (`user.webserver.preFilters`):
- Run before responder dispatch
- Can modify request parameters
- Used for authentication, logging, request transformation

**Post-filters** (`user.webserver.postFilters`):
- Run after responder generates response
- Can modify response body or headers
- Used for compression, logging, statistics

**Callback hooks** (more fine-grained):
- `callbacks.filterRequest` - Early request modification
- `callbacks.handleRequest` - Request interception (can short-circuit normal handling)
- `callbacks.filterPage` - Page content modification

## The param Table

Central data structure passed through the entire request lifecycle:

```
paramTable
├── stream              # TCP stream ID
├── client              # Client IP address (string)
├── port                # Server port
├── timeout             # Request timeout
├── request             # Raw HTTP request text
├── requestBody         # POST/PUT body
├── firstLine           # "GET /path HTTP/1.1"
├── method              # "GET", "POST", etc.
├── path                # "/some/path"
├── URI                 # URL-encoded path
├── searchArgs          # Query string after ?
├── pathArgs            # Path info after $
├── host                # Host header value
├── requestHeaders/     # Parsed request headers
├── responder           # Name of handling responder
├── responderTableAdr   # Address of responder table
├── code                # Response status code (200, 404, etc.)
├── responseBody        # Response content
├── responseHeaders/    # Response headers to send
└── stats/              # Timing/performance data
```

## Hello World Example

To serve a simple "Hello World" response:

### Option 1: CGI Script

Place in `user.webserver.cgis.hello`:

```usertalk
on hello(adrParams) {
    return ("Hello, World!")
}
```

Access via: `http://localhost/hello.fcgi`

### Option 2: Custom Responder

1. Create responder structure:
```usertalk
new(tableType, @user.webserver.responders.hello)
user.webserver.responders.hello.enabled = true
user.webserver.responders.hello.condition = "adrParamTable^.path == \"/hello\""
new(tableType, @user.webserver.responders.hello.methods)
```

2. Create GET handler at `user.webserver.responders.hello.methods.GET`:
```usertalk
on GET(adrParamTable) {
    adrParamTable^.responseHeaders.["Content-Type"] = "text/plain"
    adrParamTable^.responseBody = "Hello, World!"
    return (true)
}
```

Access via: `http://localhost/hello`

### Option 3: Action Script

Place in `user.webserver.actions.hello`:
```usertalk
on hello(adrParams) {
    return ("Hello, World!")
}
```

Access via: `http://localhost/?action=hello`

## Starting the Webserver

```usertalk
// Initialize tables
webserver.init()

// Start the HTTP listener
inetd.startOne(@user.inetd.config.http)

// Or start all configured daemons
inetd.start()
```

## Key Utility Functions

| Function | Purpose |
|----------|---------|
| `webserver.util.readUntil(stream, pattern, timeout)` | Read until pattern found |
| `webserver.util.readBytes(stream, count, timeout)` | Read exact byte count |
| `webserver.util.parseHeaders(request, @headers)` | Parse HTTP headers |
| `webserver.util.buildResponse(code, headers, body)` | Build HTTP response |
| `webserver.util.buildErrorPage(title, message)` | Generate error HTML |
| `webserver.util.getResponderTableAdr(name)` | Get responder by name |
| `webserver.util.getMethodAdr(responder, method)` | Get method handler |
| `webserver.httpHeader(status, contentType)` | Build HTTP header string |

## Startup Sequence

The webserver starts as part of Frontier's boot sequence in `system.startup.startupScript`:

1. Create `system.temp.Frontier` table, set `startingUp = true`
2. Open StartupTasks.root if present
3. Initialize scheduler
4. Open guest databases with `openOnStartup = true`
5. Call `html.init()`, `webserver.init()`, `betty.init()`, etc.
6. Build menus
7. **If `flEnableHttpServer` is true:**
   - Create `user.inetd.listens` table
   - Call `inetd.start()` to start all daemons with `startup = true`
8. Set `startingUp = false` (webserver.server waits for this if `flWaitDuringStartup` is true)

## Open Questions

1. **What is the format of `webserver.data.inetd.config.http`?** The init script copies this to `user.inetd.config.http` if not defined. This appears to be stored in the binary ODB format, not as exported `.ut` files. What are the exact fields?

2. **How does the `kernel()` directive work?** Many scripts end with `kernel(webserver.server)` or similar. This appears to replace the UserTalk implementation with a C kernel verb. What's the exact mechanism? Is it a compile-time substitution or runtime dispatch?

3. **Thread model:** The code references `thread.sleepFor(1)` for yielding. How does threading work with the webserver? One thread per connection? How does `inetd.supervisor` get invoked - is it called from an accept thread in the kernel?

4. **How are responder conditions evaluated?** The dispatch code does `evaluate(responderTableAdr^.condition)` - does the `with adrParamTable^, adrParamTable^.requestHeaders` scope apply to the evaluate call?

5. **What is the relationship between `webserver.handler` and `webserver.server`/`webserver.dispatch`?** Handler seems to be an older API for CGI/WebSTAR compatibility. When is it used vs the newer dispatch system?

6. **Where is `webserver.data.*` defined?** The init script references `webserver.data.prefs`, `webserver.data.responders`, etc. These appear to be stored in the binary ODB, not exported. What are the default values?

7. **What kernel verbs are required?** Several scripts have `kernel()` directives indicating C implementations. Which TCP and webserver operations are kernel-level vs pure UserTalk?

## Historical Notes

- The webserver was developed over 12-15 years, starting in the late 1990s
- Originally designed for WebSTAR integration on Mac (hence the CGI compatibility layer)
- TCP verbs were implemented as kernel verbs in Frontier 6.1 (1999) for performance
- The responder pattern was added later, inspired by web frameworks like Apache modules
- Pre/post filter system added for extensibility without modifying core code

## See Also

- `docs/usertalk/docserver/tcp/` - TCP verb documentation
- `docs/usertalk/docserver/inetd/` - inetd verb documentation
- `docs/usertalk/docserver/webserver/` - webserver verb documentation
