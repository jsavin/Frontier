# Processor Audit: `inetd`

**Status:** ✅ **Ready for Implementation** (Critical for Web Server)
**Audit Date:** 2025-12-05
**Auditor:** Claude (Sonnet 4.5)

---

## Basic Information

| Property | Value |
|----------|-------|
| **Processor Name** | `inetd` |
| **EFP ID** | 1021 (lang block) |
| **Verb Count** | 1 kernel verb (+5 script verbs) |
| **Window Required** | NO |
| **Documentation** | [inetd/](../../../docs/usertalk/docserver.userland.com/inetd/index.html) |
| **Stub Implementation** | [headless_inetd_verbs.c](../../../tests/headless_inetd_verbs.c) |

---

## Category Assessment

**Category:** ✅ **Core Network Functionality**

**Rationale:**
Internet daemon functionality for Frontier's web server architecture. The single kernel verb (`supervisor`) is called by the kernel when clients connect to registered network services. Other inetd verbs are UserTalk scripts for daemon management.

**Headless Compatibility:** ✅ **Full**

**Blocking Verbs:** None

---

## Verb Inventory

### Kernel Verb (1 verb - C Implementation Required)

| # | Verb Name | Signature | Description |
|---|-----------|-----------|-------------|
| 1 | `supervisor` | `inetd.supervisor(port, clientAddress, daemonName) -> boolean` | Called by kernel when client connects to inetd server |

### Script Verbs (5 verbs - UserTalk Implementation)

These are likely implemented as UserTalk scripts built on the kernel verb:
- `inetd.start()` - Start all registered daemons
- `inetd.stop()` - Stop all registered daemons
- `inetd.startOne(daemonName)` - Start specific daemon
- `inetd.stopOne(daemonName)` - Stop specific daemon
- `inetd.isDaemonRunning(daemonName)` - Check if daemon is running

**Note:** Only `supervisor` is in kernelverbs.rc - others are UserTalk scripts

---

## Implementation Analysis

### Complexity: **MEDIUM-HIGH** (Network daemon management)

### Dependencies

- **Other Processors:** tcp (for network connections)
- **External Services:** None (built-in daemon system)
- **OS-Specific Functionality:** YES (socket listening, port binding)
- **GUI/Window Context:** NO

### Key Implementation Notes

**inetd Concept:**
inetd (Internet Daemon) is a Unix superserver that:
1. Listens on multiple network ports
2. Launches service handlers when connections arrive
3. Reduces resource usage (services only run when needed)
4. Used by Frontier for HTTP server, XML-RPC, etc.

**Frontier's inetd Architecture:**
```
1. User registers daemon: inetd.config.http = {port:80, handler:"http.handleRequest"}
2. Kernel starts listener on port 80
3. Client connects to port 80
4. Kernel calls: inetd.supervisor(80, "192.168.1.100", "http")
5. supervisor dispatches to http.handleRequest()
6. Request processed, response sent
7. Connection closed
```

**Modern Implementation Approach:**
```c
// Kernel maintains table of registered daemons
typedef struct {
    int port;
    char daemonName[256];
    char handlerScript[256];
    int socketfd;
    bool running;
} tyinetddaemon;

// When client connects:
// 1. accept() the connection
// 2. Look up daemon by port
// 3. Call inetd.supervisor(port, clientIP, daemonName)
// 4. supervisor calls the UserTalk handler script
// 5. Handler processes request and sends response
```

**supervisor Verb Behavior:**
```c
// inetd.supervisor is called BY THE KERNEL, not by user scripts
// Parameters:
//   - port: Port number client connected to
//   - clientAddress: Client IP address
//   - daemonName: Name of daemon (e.g., "http", "xmlrpc")

// Implementation:
boolean inetdsupervisor(int port, char* clientIP, char* daemonName) {
    // 1. Look up handler script for this daemon
    //    e.g., system.inetd.config[daemonName].handler

    // 2. Set up connection context
    //    - Store socket fd globally
    //    - Store client IP
    //    - Store port

    // 3. Call handler script
    //    e.g., http.handleRequest()

    // 4. Handler reads request, sends response via tcp.* verbs

    // 5. Clean up connection

    return true;
}
```

**Integration with tcp Processor:**
The tcp processor will provide the low-level socket operations:
- `tcp.read()` - Read from client connection
- `tcp.write()` - Write to client connection
- `tcp.close()` - Close connection

inetd provides the daemon registration and dispatch mechanism.

**Daemon Configuration:**
Likely stored in database table:
```
system.inetd.config
    http
        port = 80
        handler = "http.handleRequest"
        enabled = true
    xmlrpc
        port = 5335
        handler = "xmlrpc.handleRequest"
        enabled = true
```

**Threading Considerations:**
- Each connection should run in its own thread
- supervisor verb must be thread-safe
- Handler scripts run in isolated thread context

**Security Considerations:**
- **Port Binding:** Requires privileges for ports <1024
- **DOS Protection:** Limit concurrent connections
- **IP Filtering:** Support allow/deny lists
- **Resource Limits:** Timeout inactive connections
- **Handler Isolation:** Prevent handler crashes from affecting other connections

---

## UserTalk Documentation Notes

From docserver.userland.com/inetd/:

**inetd.supervisor(port, clientAddress, daemonName)**
- Called by kernel when client connects to registered inetd server
- **NOT called by user scripts** - this is a kernel callback
- Parameters:
  - `port` - Port number client connected to
  - `clientAddress` - Client IP address string
  - `daemonName` - Name of registered daemon
- Looks up handler script and dispatches request
- Handler script uses tcp.* verbs to communicate with client

**Script Verbs (UserTalk implementations):**

**inetd.start()**
- Starts all registered daemons
- Opens listening sockets for each enabled daemon
- Registers supervisor callback with kernel

**inetd.stop()**
- Stops all running daemons
- Closes all listening sockets
- Cleans up connections

**inetd.startOne(daemonName)**
- Starts specific daemon by name
- Example: `inetd.startOne("http")`

**inetd.stopOne(daemonName)**
- Stops specific daemon by name
- Example: `inetd.stopOne("xmlrpc")`

**inetd.isDaemonRunning(daemonName)**
- Returns true if daemon is currently running
- Example: `inetd.isDaemonRunning("http")` → true

---

## Testing Requirements

**Test Scenarios:**

**Kernel Verb Testing:**
```usertalk
// supervisor is called by kernel, so test via full integration:

// 1. Register test daemon
system.inetd.config.testDaemon = {
    port: 8080,
    handler: "testDaemon.handleRequest",
    enabled: true
}

// 2. Start daemon
inetd.startOne("testDaemon")

// 3. Connect from external client
// (use telnet, curl, or custom TCP client)
// telnet localhost 8080

// 4. Verify supervisor called
// - Check logs for supervisor invocation
// - Verify handler script executed
// - Verify response sent to client

// 5. Stop daemon
inetd.stopOne("testDaemon")
```

**Integration Testing:**
```usertalk
// Test HTTP daemon (primary use case)
inetd.start()
// External: curl http://localhost/
inetd.isDaemonRunning("http") → true
inetd.stop()
inetd.isDaemonRunning("http") → false
```

**Error Handling:**
- Port already in use
- Permission denied (ports <1024)
- Invalid daemon configuration
- Handler script errors
- Connection timeouts
- Resource exhaustion (too many connections)

**Security Testing:**
- Bind to privileged ports (<1024) - should fail without root
- Concurrent connection limits
- Handler crash recovery
- IP filtering (allow/deny lists)

**Platform Differences:**
- Unix: Standard socket APIs
- Windows: WinSock API
- Port binding permissions vary by OS

---

## Implementation Effort

**Estimated Time:** 12-16 hours

**Breakdown:**
- Daemon registration system: 3-4 hours
- Socket listening/accept loop: 3-4 hours
- supervisor verb implementation: 2-3 hours
- Thread management: 2-3 hours
- Testing & integration: 2-3 hours

**Confidence:** MEDIUM - Complex network daemon management, but well-defined pattern

**Dependencies:**
- Must implement tcp processor first (for low-level socket operations)
- Requires threading support in kernel
- Needs database access for daemon configuration

---

## Priority & Sequencing

**Priority:** 🏆 **HIGH** (Tier 1 - Critical for Web Server)

**Recommended Implementation Order:** 18 (after tcp, before webserver)

**Blockers/Prerequisites:**
- tcp processor (for socket operations)
- Threading support
- Database access (for daemon configuration)

**Implementation Sequence:**
1. Implement tcp processor (socket I/O)
2. Create daemon registration table structure
3. Implement socket listener with accept loop
4. Implement supervisor verb (dispatch mechanism)
5. Add thread management for concurrent connections
6. Implement script verbs (start, stop, etc.)
7. Test with HTTP daemon
8. Security hardening

---

## Headless Compatibility Analysis

**Fully Compatible:** ✅ YES (1/1 kernel verb)

- ✅ `supervisor` - Network daemon dispatch (headless-compatible)

**Script Verbs:** ✅ YES (5/5 verbs)
- All daemon management scripts are headless-compatible
- Essential for web server operation

**Recommendation:** Implement as high priority for web server functionality

---

## Related Processors

- **tcp** - Low-level socket operations (prerequisite)
- **webserver** - HTTP server implementation (uses inetd)
- **http** - HTTP client/server utilities
- **xmlrpc** - XML-RPC server (uses inetd)

---

## Special Considerations

**Daemon Architecture:**
Frontier's inetd is similar to Unix inetd but integrated with UserTalk:
- Unix inetd: Launches external programs
- Frontier inetd: Calls UserTalk handler scripts

**Connection Flow:**
```
1. Kernel listens on port (e.g., 80)
2. Client connects
3. accept() creates new socket
4. supervisor(80, "192.168.1.100", "http") called
5. Handler script runs with access to connection via tcp.*
6. Script reads request: tcp.read()
7. Script sends response: tcp.write()
8. Connection closes: tcp.close()
```

**Thread Safety:**
Each connection runs in its own thread:
- supervisor verb must be reentrant
- Handler scripts run in isolated contexts
- Shared data needs locking (daemon config table)

**Resource Management:**
- Limit concurrent connections (prevent DOS)
- Timeout idle connections
- Clean up crashed handler threads
- Monitor port usage

**Security Hardening:**
1. **Port Binding:**
   - Warn if binding to privileged ports (<1024)
   - Drop privileges after binding if possible

2. **IP Filtering:**
   - Support allow/deny lists per daemon
   - Example: Only allow localhost connections during dev

3. **Rate Limiting:**
   - Limit connections per IP address
   - Prevent DOS attacks

4. **Handler Isolation:**
   - Catch and log handler script errors
   - Don't let handler crash affect other connections

5. **Resource Limits:**
   - Max concurrent connections per daemon
   - Connection timeout
   - Request size limits

**Configuration Storage:**
Daemon config likely in `system.inetd` table:
```
system.inetd.config.http
    .port = 80
    .handler = "http.handleRequest"
    .enabled = true
    .maxConnections = 100
    .timeout = 60
    .allowIPs = ["192.168.1.0/24"]  // optional
    .denyIPs = []  // optional
```

**Logging:**
Log all daemon activity:
- Daemon starts/stops
- Connection accepts
- Handler invocations
- Errors and exceptions
- Security events (denied IPs, etc.)

---

## Use Cases

**HTTP Web Server:**
```usertalk
// Configure HTTP daemon
system.inetd.config.http = {
    port: 8080,
    handler: "http.handleRequest",
    enabled: true
}

// Start HTTP server
inetd.startOne("http")

// Server now accepts HTTP requests
// supervisor calls http.handleRequest() for each connection
```

**XML-RPC Server:**
```usertalk
// Configure XML-RPC daemon
system.inetd.config.xmlrpc = {
    port: 5335,
    handler: "xmlrpc.handleRequest",
    enabled: true
}

inetd.startOne("xmlrpc")
```

**Custom Protocol:**
```usertalk
// Custom daemon
system.inetd.config.myprotocol = {
    port: 9999,
    handler: "myprotocol.handleConnection"
}

on myprotocol.handleConnection()
    local(request = tcp.readLine())
    local(response = "ECHO: " + request)
    tcp.writeLine(response)
    tcp.close()

inetd.startOne("myprotocol")
```

---

## References

**Documentation:**
- DocServer: `docs/usertalk/docserver.userland.com/inetd/`
- Individual verb pages: supervisor.tmp (plus script verbs)

**Implementation:**
- Stub: `tests/headless_inetd_verbs.c`
- Canonical verb list: `Common/resources/Win32/kernelverbs.rc` (lines 1020-1024)

**Standards:**
- Unix inetd: Traditional internet superserver
- POSIX sockets: socket(), bind(), listen(), accept()
- Threading: pthread (Unix), Windows threads

**Related:**
- xinetd (extended internet daemon on Linux)
- systemd socket activation (modern Linux)

---

## Next Steps

1. ✅ Audit complete - ready for implementation
2. ⏳ Implement tcp processor first (prerequisite)
3. ⏳ Design daemon registration table structure
4. ⏳ Implement socket listener with accept loop
5. ⏳ Implement supervisor verb (dispatcher)
6. ⏳ Add threading support for concurrent connections
7. ⏳ Implement UserTalk script verbs (start, stop, etc.)
8. ⏳ Security hardening (IP filtering, rate limiting)
9. ⏳ Integration testing with HTTP daemon
10. ⏳ Update implementation status

---

**Audit Status:** ✅ Complete and Approved for Implementation

**Implementation Strategy:**
- **Single kernel verb** (`supervisor`) plus 5 script verbs
- **Critical for web server** functionality
- **Implement after tcp processor** (dependency)
- **12-16 hours effort** for full daemon system
- **High priority** for Frontier's core use case

**Key Success Factors:**
1. Clean integration with tcp processor
2. Robust thread management
3. Comprehensive error handling
4. Security hardening (IP filtering, rate limiting)
5. Proper resource cleanup
