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

### Built-in helloWorld Responder (Recommended for Testing)

Frontier includes a built-in helloWorld responder at `webserver.data.responders.helloWorld` that is **disabled by default**. It returns a simple Hello World page for requests to `/helloworld`.

**To enable for integration testing:**
```usertalk
webserver.data.responders.helloWorld.enabled = true
```

Then access via: `http://localhost/helloworld`

This is the recommended approach for validating the webserver stack end-to-end.

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
	- webserver.data.inetd.config.http is a simple set of default configurations for Frontier's primary webserver on port 80. Here's what that table contains:
		- count: 10 // max number of concurrent connections (I think)
		- daemon: @system.verbs.builtins.webserver.server // primary dispatch route for http
		- noWait: true
		- port: 80
		- startup: true // the system will try to start this service when Frontier launches
		- timeout: 10

2. **How does the `kernel()` directive work?** Many scripts end with `kernel(webserver.server)` or similar. This appears to replace the UserTalk implementation with a C kernel verb. What's the exact mechanism? Is it a compile-time substitution or runtime dispatch?
	- This is a core runtime verb dispatch mechanism. It's already implemented.
	- You shouldn't need to worry about details unless you need to "wire up" newly enabled verbs.
	- We already have well-established patterns for how this works.

3. **Thread model:** The code references `thread.sleepFor(1)` for yielding. How does threading work with the webserver? One thread per connection? How does `inetd.supervisor` get invoked - is it called from an accept thread in the kernel?
	- This is a legacy requirement for cooperative multi-tasking environments.
	- In frontier-cli, thread.yield() is a noop, and thread.sleepFor() simply pauses thread execution.
	- I think inetd.supervisor is called from the kernel, but I don't know the details.

4. **How are responder conditions evaluated?** The dispatch code does `evaluate(responderTableAdr^.condition)` - does the `with adrParamTable^, adrParamTable^.requestHeaders` scope apply to the evaluate call?
	- adrParamTable is the thread-local table where the indetd and webserver (mainly webserver) store data as a request is being processed. It exists for the lifetime of the request/response thread.
	- The condition in this case is a snipped of UserTalk code that implements the condition used to determine how incoming requests are dispatched in the UserTalk layer.

5. **What is the relationship between `webserver.handler` and `webserver.server`/`webserver.dispatch`?** Handler seems to be an older API for CGI/WebSTAR compatibility. When is it used vs the newer dispatch system?
	- Frontier initially didn't have its own internal webserver, and people who wanted to use it in the context of a website implemented CGI scripts. I don't know for sure, but I suspect that dispatch is an older, no longer used dispatch mechanism.
	- Code in webserver.server is the top-level inetd handler which then uses webserver.handler to dispatch to the correct responder based on a condition.

6. **Where is `webserver.data.*` defined?** The init script references `webserver.data.prefs`, `webserver.data.responders`, etc. These appear to be stored in the binary ODB, not exported. What are the default values?
	- I don't think you need to be concerned with this in order to get the core webserver working.
	- All of this data is consumed in the UserTalk layer, and should "just work" once the low-level parts are all working.

7. **What kernel verbs are required?** Several scripts have `kernel()` directives indicating C implementations. Which TCP and webserver operations are kernel-level vs pure UserTalk?
	- Anywhere that you see `kernel(VERB)` in UserTalk code, those are calling into kernel verb implementations.
	- I don't know the full set of required verbs, but anything that the webserver or inetd do in the process of responding to a request is going to be required.

## Hello World Request Trace

Complete path for a `GET /helloworld` request using the built-in helloWorld responder:

### 1. TCP Layer (C Kernel)
```
tcp.listenStream(80, 10, @inetd.supervisor, 80, 0)
  └── accept() on socket
      └── spawn thread for connection
          └── invoke callback: inetd.supervisor(stream, refcon=80)
```

### 2. inetd Layer
```
inetd.supervisor(stream, refcon)     [kernel verb]
  ├── lookup user.inetd.listens.[80].adrTable → daemon config
  ├── tcp.getPeerAddress(stream)     [kernel verb - get client IP]
  ├── tcp.addressDecode(addr)        [kernel verb - IP to string]
  ├── tcp.statusStream(stream, @bytes) [kernel verb - check for data]
  ├── tcp.readStream(stream, bytes)  [kernel verb - read request]
  ├── call daemon: webserver.server(@paramTable)
  ├── tcp.writeStream(stream, response) [kernel verb - send response]
  └── tcp.closeStream(stream)        [kernel verb - cleanup]
```

### 3. Webserver Layer
```
webserver.server(adrParamTable)      [kernel verb]
  ├── webserver.util.readUntil(stream, "\r\n\r\n", timeout)
  │     └── uses tcp.statusStream + tcp.readStream internally
  ├── webserver.util.parseHeaders(request, @headers) [pure UserTalk]
  ├── webserver.util.parseCookies(adrParamTable)     [pure UserTalk]
  └── webserver.dispatch(adrParamTable)              [kernel verb]
```

### 4. Dispatch Layer
```
webserver.dispatch(adrParamTable)    [kernel verb]
  ├── run preFilters (user.webserver.preFilters)
  ├── find responder: evaluate conditions
  │     └── helloWorld.condition: adrParamTable^.path == "/helloworld"
  ├── call responder method: helloWorld.methods.GET(adrParamTable)
  │     └── sets responseBody = "<html><body>Hello World!</body></html>"
  ├── run postFilters (user.webserver.postFilters)
  └── webserver.util.buildResponse(200, headers, body) [kernel verb]
```

### 5. Response Path
```
webserver.buildResponse(code, headers, body) [kernel verb]
  └── returns: "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n..."

Response flows back through:
  webserver.dispatch → webserver.server → inetd.supervisor
    └── tcp.writeStream(stream, response)
    └── tcp.closeStream(stream)
```

### Required Kernel Verbs for Hello World

| Verb | Used By | Purpose |
|------|---------|---------|
| `tcp.listenStream` | inetd.startOne | Start listener |
| `tcp.statusStream` | inetd.supervisor, webserver.util.readUntil | Check connection |
| `tcp.readStream` | inetd.supervisor, webserver.util.readUntil | Read data |
| `tcp.writeStream` | inetd.supervisor | Send response |
| `tcp.closeStream` | inetd.supervisor | Close connection |
| `tcp.getPeerAddress` | inetd.supervisor | Get client IP |
| `tcp.addressDecode` | inetd.supervisor | IP to string |
| `inetd.supervisor` | tcp callback | Route to daemon |
| `webserver.server` | inetd.supervisor | HTTP entry point |
| `webserver.dispatch` | webserver.server | Route to responder |
| `webserver.buildResponse` | webserver.dispatch | Build HTTP response |

**Note:** The `kernel()` directive at the end of UserTalk scripts indicates that a C kernel implementation exists. The UserTalk code in comments shows the original logic before kernel optimization.

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
