# TCP Verbs - Complete API Analysis

**Generated**: 2026-01-16
**Status**: Comprehensive analysis for POSIX implementation planning

## Executive Summary

Frontier's TCP networking API consists of **27 documented verbs** organized into 5 functional categories:
1. **Core Stream Operations** (7 verbs) - Connect, read, write, close
2. **Server Operations** (2 verbs) - Listen, close listener
3. **Stream Utilities** (6 verbs) - Buffered reads, file transfer, status
4. **DNS/Address Operations** (9 verbs) - DNS lookup, address conversion
5. **High-Level Helpers** (3 verbs) - HTTP client, mail sending (UserTalk-implemented)

**Key Finding**: Most primitives require kernel (C) implementation. Only high-level helpers like `httpClient` and `sendMail` are pure UserTalk scripts.

---

## 1. Core Stream Operations

### tcp.openStream(adr, port)

**Signature**: `tcp.openStream(adr, port) → streamID`

**Parameters**:
- `adr` (string or long): DNS name, dotted IP address, or encoded 4-byte IP address
- `port` (long): TCP port number

**Returns**: Stream ID (long) for use with other stream verbs

**Implementation**: UserTalk glue that dispatches to kernel based on address type:
```usertalk
on openStream (adr, port) {
    on kernelOpenName (adr, port) {
        kernel (tcp.openNameStream)};  // DNS name or dotted IP
    on kernelOpenAddr (adr, port) {
        kernel (tcp.openAddrStream)};  // 4-byte encoded address

    if typeOf (adr) != longType {
        return (kernelOpenName (adr, port))}
    else {
        return (kernelOpenAddr (adr, port))}
}
```

**Kernel Verbs Required**:
- `tcp.openNameStream(name, port)` - DNS resolution + connect
- `tcp.openAddrStream(addr, port)` - Direct IP connect

**Behavior**:
- Blocking connect operation
- Returns immediately with stream ID on success
- Raises script error on failure (connection refused, timeout, etc.)

---

### tcp.readStream(stream, bytesToRead)

**Signature**: `tcp.readStream(stream, bytesToRead) → data`

**Parameters**:
- `stream` (long): Stream ID from `tcp.openStream`
- `bytesToRead` (long): Maximum bytes to read

**Returns**: Binary data (string type) read from connection

**Implementation**: Direct kernel verb `kernel (tcp.readStream)`

**Behavior**:
- Reads up to `bytesToRead` bytes from stream
- Returns immediately with whatever data is available
- Returns empty string if no data available (non-blocking)
- Raises error if stream closed or invalid

---

### tcp.writeStream(stream, data)

**Signature**: `tcp.writeStream(stream, data) → true`

**Parameters**:
- `stream` (long): Stream ID
- `data` (string): Binary data to write

**Returns**: Boolean true on success

**Implementation**: Direct kernel verb `kernel (tcp.writestream)` (note: lowercase 's')

**Behavior**:
- Writes all data to stream (blocking until complete)
- Raises error if stream closed, connection lost, or timeout

---

### tcp.closeStream(stream)

**Signature**: `tcp.closeStream(stream) → true`

**Parameters**:
- `stream` (long): Stream ID to close

**Returns**: Boolean true

**Implementation**: Direct kernel verb `kernel (tcp.closeStream)`

**Behavior**:
- Graceful close (sends FIN)
- Waits for remote acknowledgment
- Deallocates stream ID
- Safe to call multiple times on same stream

---

### tcp.abortStream(stream)

**Signature**: `tcp.abortStream(stream) → true`

**Parameters**:
- `stream` (long): Stream ID to abort

**Returns**: Boolean true

**Implementation**: Direct kernel verb `kernel (tcp.abortStream)`

**Behavior**:
- Immediate close (sends RST)
- Does NOT wait for remote acknowledgment
- Use when connection is hung or needs immediate termination
- Deallocates stream ID

**Use Case**: From `tcp.httpClient`:
```usertalk
try {
    tcp.readStreamUntil(stream, pat, timeOutTicks/60, @httpResult)}
else {
    tcp.abortStream(stream);  // Immediate close on timeout/error
    scriptError(tryerror)};
```

---

### tcp.statusStream(stream, @bytesPending)

**Signature**: `tcp.statusStream(stream, @bytesPending) → status`

**Parameters**:
- `stream` (long): Stream ID
- `@bytesPending` (address): ODB location to store pending byte count

**Returns**: Status code (implementation-defined)

**Implementation**: Direct kernel verb `kernel (tcp.statusStream)`

**Behavior**:
- Checks stream status without blocking
- Sets `bytesPending` to number of bytes available to read
- Returns status indicating connection state

---

### tcp.countConnections()

**Signature**: `tcp.countConnections() → count`

**Parameters**: None

**Returns**: Number of active TCP connections (long)

**Implementation**: Direct kernel verb `kernel (tcp.countconnections)` (note: lowercase)

**Behavior**:
- Returns count of open streams (both client and server connections)
- Useful for monitoring resource usage
- Used in connection leak detection

---

## 2. Server Operations

### tcp.listenStream(port, depth, callback, refcon, ip)

**Signature**: `tcp.listenStream(port, depth, callback, refcon, ip) → listenID`

**Parameters**:
- `port` (long): TCP port to listen on
- `depth` (long): Maximum connection queue depth
- `callback` (string): Name of UserTalk script to handle connections
- `refcon` (long, optional): User data passed to callback (default: 0)
- `ip` (long, optional): Bind to specific IP address (default: 0 = INADDR_ANY)

**Returns**: Listen ID for use with `tcp.closeListen`

**Implementation**: UserTalk glue with version compatibility:
```usertalk
on listenStream (port, depth, callback, refcon = 0, ip=0) {
    on kernelCall (port, depth, callback, refcon, addr) {
        kernel (tcp.listenStream)};
    on oldKernelCall (port, depth, callback, refcon) {
        kernel (tcp.listenStream)};  // Pre-6.2a3 signature

    if date.versionLessThan (Frontier.version (), "6.2a3") {
        return (oldKernelCall (port, depth, callback, refcon))}
    else {
        local (addr);
        if (ip != 0) {
            // Platform-specific IP binding logic
            addr = tcp.addressEncode (ip)}
        else {
            addr = 0};  // INADDR_ANY
        return (kernelCall (port, depth, callback, refcon, addr))}}
```

**Callback Signature**:
```usertalk
on myCallback (stream, refcon) {
    // stream = accepted connection stream ID
    // refcon = user data from listenStream call
    // Handle the connection...
    tcp.closeStream (stream)}
```

**Behavior**:
- Returns immediately after starting listener
- Calls `callback` asynchronously in **separate thread** for each incoming connection
- `depth` parameter controls accept queue (backlog)
- Callback must close `stream` when done, or connection leaks

**Threading Model** (from WinSockNetEvents.c):
```c
#ifdef ACCEPT_IN_SEPARATE_THREAD
    // Each accepted connection runs callback in dedicated pthread
    long idthread;
    hdldatabaserecord hdatabase;  // Thread-local database context
#endif
```

---

### tcp.closeListen(listenID)

**Signature**: `tcp.closeListen(listenID) → true`

**Parameters**:
- `listenID` (long): Listener ID from `tcp.listenStream`

**Returns**: Boolean true

**Implementation**: Direct kernel verb `kernel (tcp.closeListen)`

**Behavior**:
- Stops accepting new connections
- Does NOT close existing accepted connections
- Those must be closed separately with `tcp.closeStream`

**Note**: Listen ID is NOT a stream ID - cannot use `tcp.closeStream` on it.

---

## 3. Stream Utilities

### tcp.readStreamUntil(stream, pattern, timeOutSecs, @buffer)

**Signature**: `tcp.readStreamUntil(stream, pattern, timeOutSecs, @buffer) → true`

**Parameters**:
- `stream` (long): Stream ID
- `pattern` (string): Pattern to read until (e.g., `"\r\n\r\n"`)
- `timeOutSecs` (long): Timeout in seconds
- `@buffer` (address): ODB location to store accumulated data

**Returns**: Boolean true on success

**Implementation**: Direct kernel verb `kernel (tcp.readstreamuntil)` (lowercase)

**Behavior**:
- Reads data until `pattern` found or timeout
- Appends all read data (including pattern) to `buffer`
- Blocks until pattern found or timeout expires
- Raises error on timeout or connection closed

**Use Case**: Reading HTTP headers:
```usertalk
local (httpResult = "");
tcp.readStreamUntil (stream, "\r\n\r\n", timeOutTicks/60, @httpResult)
// httpResult now contains full HTTP header
```

---

### tcp.readStreamBytes(stream, bytesToRead, timeOutSecs, @buffer)

**Signature**: `tcp.readStreamBytes(stream, bytesToRead, timeOutSecs, @buffer) → true`

**Parameters**:
- `stream` (long): Stream ID
- `bytesToRead` (long): Exact number of bytes to read
- `timeOutSecs` (long): Timeout in seconds
- `@buffer` (address): ODB location to store data

**Returns**: Boolean true on success

**Implementation**: Direct kernel verb `kernel (tcp.readstreambytes)` (lowercase)

**Behavior**:
- Reads EXACTLY `bytesToRead` bytes
- Blocks until all bytes received or timeout
- Appends data to `buffer`
- Raises error if timeout or connection closed before all bytes received

**Use Case**: Reading known-length HTTP body:
```usertalk
local (lenContent = number (headerTable.["Content-Length"]));
tcp.readStreamBytes (stream, lenTotal, timeOutTicks/60, @httpResult)
```

---

### tcp.readStreamUntilClosed(stream, timeOutSecs, @buffer)

**Signature**: `tcp.readStreamUntilClosed(stream, timeOutSecs, @buffer) → true`

**Parameters**:
- `stream` (long): Stream ID
- `timeOutSecs` (long): Timeout in seconds
- `@buffer` (address): ODB location to store all data

**Returns**: Boolean true on success

**Implementation**: Direct kernel verb `kernel (tcp.readStreamUntilClosed)`

**Behavior**:
- Reads all data until remote closes connection
- Blocks until EOF (FIN received) or timeout
- Appends all data to `buffer`
- Automatically closes stream when done

**Use Case**: Reading response with no Content-Length:
```usertalk
// HTTP 1.0 responses without Content-Length
tcp.readStreamUntilClosed (stream, timeOutTicks/60, @httpResult)
```

---

### tcp.writeStringToStream(stream, data, chunksize, timeout)

**Signature**: `tcp.writeStringToStream(stream, data, chunksize, timeout) → true`

**Parameters**:
- `stream` (long): Stream ID
- `data` (string): Data to write
- `chunksize` (long): Size of chunks to write (e.g., 5120 bytes)
- `timeout` (long): Timeout in seconds (not ticks!)

**Returns**: Boolean true on success

**Implementation**: Direct kernel verb `kernel (tcp.writeStringToStream)`

**Behavior**:
- Writes data in `chunksize` chunks
- Yields to other threads/system between chunks
- Allows cancellation during long writes
- Raises error on timeout or connection failure

**Use Case**: Sending large HTTP request:
```usertalk
try {
    tcp.writeStringToStream (stream, httpCommand, 5 * 1024, timeOutTicks/60)}
else {
    tcp.abortStream (stream);
    scriptError (tryError)};
```

---

### tcp.writeFileToStream(stream, f, prefix, suffix)

**Signature**: `tcp.writeFileToStream(stream, f, prefix, suffix) → true`

**Parameters**:
- `stream` (long): Stream ID
- `f` (filespec): File to send
- `prefix` (string, optional): Data to send before file
- `suffix` (string, optional): Data to send after file

**Returns**: Boolean true on success

**Implementation**: Direct kernel verb `kernel (tcp.writeFileToStream)`

**Behavior**:
- Efficiently streams file contents to socket
- Sends `prefix`, then file, then `suffix`
- Handles large files without loading into memory
- Useful for FTP uploads, HTTP POST of files

---

## 4. DNS/Address Operations

### tcp.nameToAddress(domainName)

**Signature**: `tcp.nameToAddress(domainName) → address`

**Parameters**:
- `domainName` (string): DNS name (e.g., "www.scripting.com")

**Returns**: 4-byte IP address as signed long (host byte order)

**Implementation**: Direct kernel verb `kernel (tcp.nameToAddress)`

**Example**:
```usertalk
tcp.nameToAddress ("www.scripting.com")
    → -825485308  // (signed representation)

tcp.addressDecode (tcp.nameToAddress ("www.scripting.com"))
    → "206.204.24.4"
```

**Behavior**:
- Performs DNS lookup (blocking)
- Returns first A record
- Raises error if name not found

---

### tcp.addressToName(adr)

**Signature**: `tcp.addressToName(adr) → domainName`

**Parameters**:
- `adr` (long): 4-byte IP address

**Returns**: Domain name (string)

**Implementation**: Direct kernel verb `kernel (tcp.addressToName)`

**Behavior**:
- Performs reverse DNS lookup (PTR record)
- Blocking operation
- May return IP address as string if no PTR record

---

### tcp.addressEncode(ipAddress)

**Signature**: `tcp.addressEncode(ipAddress) → encodedAddress`

**Parameters**:
- `ipAddress` (string): Dotted decimal IP (e.g., "206.204.24.4")

**Returns**: 4-byte IP address as long

**Implementation**: Direct kernel verb `kernel (tcp.addressEncode)`

**Behavior**:
- Converts dotted notation to 4-byte value
- No DNS lookup - pure conversion
- Fast, non-blocking operation

---

### tcp.addressDecode(encodedAdr)

**Signature**: `tcp.addressDecode(encodedAdr) → ipAddress`

**Parameters**:
- `encodedAdr` (long): 4-byte IP address

**Returns**: Dotted decimal IP string

**Implementation**: Direct kernel verb `kernel (tcp.addressDecode)`

**Behavior**:
- Converts 4-byte value to dotted notation
- No DNS lookup - pure conversion
- Fast, non-blocking operation

---

### tcp.myAddress()

**Signature**: `tcp.myAddress() → address`

**Parameters**: None

**Returns**: Local machine's IP address as 4-byte long

**Implementation**: Direct kernel verb `kernel (tcp.myAddress)`

**Behavior**:
- Returns primary network interface IP
- Fast, non-blocking operation

---

### tcp.myDottedID()

**Signature**: `tcp.myDottedID() → ipAddress`

**Parameters**: None

**Returns**: Local machine's IP as dotted decimal string

**Implementation**: Presumably direct kernel verb or glue

**Behavior**:
- Equivalent to `tcp.addressDecode(tcp.myAddress())`

---

### tcp.dns.getDomainName(adr)

**Signature**: `tcp.dns.getDomainName(adr) → domainName`

**Parameters**:
- `adr` (various): IP address (string or long)

**Returns**: Domain name (string)

**Implementation**: UserTalk glue (likely wraps `tcp.addressToName`)

---

### tcp.dns.getDottedId(name)

**Signature**: `tcp.dns.getDottedId(name) → ipAddress`

**Parameters**:
- `name` (string): DNS name

**Returns**: Dotted decimal IP address (string)

**Implementation**: UserTalk glue (likely wraps `tcp.nameToAddress` + `tcp.addressDecode`)

---

### tcp.dns.getMyDomainName()

**Signature**: `tcp.dns.getMyDomainName() → domainName`

**Parameters**: None

**Returns**: Local machine's domain name (string)

**Implementation**: UserTalk glue (wraps `tcp.addressToName(tcp.myAddress())`)

---

## 5. High-Level Helpers (UserTalk-Implemented)

### tcp.httpClient(...)

**Signature**: 18 parameters (all optional with defaults)

**Parameters** (most commonly used):
- `method` (string): HTTP method ("GET", "POST", etc.)
- `server` (string): Server domain or IP
- `port` (long): Port number (default: 80)
- `path` (string): URL path (default: "/")
- `data` (string): Request body for POST
- `datatype` (string): Content-Type for POST
- `username`, `password` (string): HTTP Basic Auth
- `adrHdrTable` (address): Additional HTTP headers
- `timeOutTicks` (long): Timeout in ticks (default: 1800 = 30 seconds)
- `ctFollowRedirects` (long): Max redirects to follow (default: 0)

**Returns**: HTTP response body (string)

**Implementation**: **Pure UserTalk script** using TCP primitives

**Key Dependencies**:
- `tcp.openStream` - Connect to server
- `tcp.writeStringToStream` - Send HTTP request
- `tcp.readStreamUntil` - Read headers
- `tcp.readStreamBytes` - Read known-length body
- `tcp.readStreamUntilClosed` - Read unknown-length body
- `tcp.closeStream` - Clean close
- `tcp.abortStream` - Error cleanup

**Behavior**:
- Full HTTP/1.0 client implementation
- Supports proxy servers
- Cookie handling (if enabled)
- Automatic redirect following
- Progress messages (if enabled)
- Connection pooling avoidance (opens/closes for each request)

**Example**:
```usertalk
local (response = tcp.httpClient (
    method: "GET",
    server: "www.scripting.com",
    path: "/default.html",
    timeOutTicks: 60*60))  // 60 seconds
```

---

### tcp.httpReadUrl(url, username, password)

**Signature**: `tcp.httpReadUrl(url, username, password) → response`

**Parameters**:
- `url` (string): Full HTTP URL
- `username`, `password` (string, optional): HTTP Basic Auth

**Returns**: HTTP response body (string)

**Implementation**: UserTalk wrapper around `tcp.httpClient`

**Behavior**:
- Parses URL into server/port/path
- Calls `tcp.httpClient` with parsed parameters
- Simpler interface for common GET requests

---

### tcp.sendMail(...)

**Signature**: 11 parameters (all optional with defaults)

**Parameters**:
- `recipient` (string): To address
- `subject` (string): Subject line
- `message` (string): Email body
- `sender` (string): From address (default: user.prefs.mailAddress)
- `cc`, `bcc` (string): Carbon copy addresses
- `host` (string): SMTP server (default: user.prefs.mailHost)
- `mimeType` (string): Content type (default: "text/plain")
- `adrHdrTable` (address): Additional headers
- `timeOutTicks` (long): Timeout (default: 3600 = 60 seconds)
- `flMessages` (boolean): Show progress messages

**Returns**: Boolean true on success

**Implementation**: **Pure UserTalk script** using TCP primitives

**Key Dependencies**:
- `tcp.openStream` - Connect to SMTP server (port 25)
- `tcp.writeStream` / `tcp.readStream` - SMTP protocol exchange
- `tcp.closeStream` - Disconnect

**Behavior**:
- Full SMTP client implementation
- Sends MAIL FROM, RCPT TO, DATA commands
- Handles multi-recipient CC/BCC
- Raises error on SMTP failure

---

## Summary: Kernel vs UserTalk Implementation

### Must Be Implemented in C (Kernel Verbs)

**Core Primitives** (11 verbs):
1. `tcp.openNameStream` - Connect by DNS name
2. `tcp.openAddrStream` - Connect by IP address
3. `tcp.readStream` - Basic read
4. `tcp.writeStream` - Basic write
5. `tcp.closeStream` - Graceful close
6. `tcp.abortStream` - Immediate close
7. `tcp.listenStream` - Start listener with callback
8. `tcp.closeListen` - Stop listener
9. `tcp.statusStream` - Connection status
10. `tcp.getPeerAddress` - Remote IP
11. `tcp.countConnections` - Active connection count

**Buffered I/O** (4 verbs):
12. `tcp.readStreamUntil` - Read until pattern
13. `tcp.readStreamBytes` - Read exact byte count
14. `tcp.readStreamUntilClosed` - Read until EOF
15. `tcp.writeStringToStream` - Chunked write with yield

**File Transfer** (1 verb):
16. `tcp.writeFileToStream` - Stream file to socket

**DNS/Address** (6 verbs):
17. `tcp.nameToAddress` - DNS lookup
18. `tcp.addressToName` - Reverse DNS
19. `tcp.addressEncode` - Dotted → 4-byte
20. `tcp.addressDecode` - 4-byte → dotted
21. `tcp.myAddress` - Local IP (4-byte)
22. `tcp.getPeerPort` - Remote port number

**Total Kernel Verbs**: 22

### Implemented in UserTalk (Glue + High-Level)

**Glue Scripts** (dispatch to kernel based on parameters):
- `tcp.openStream` - Dispatches to openNameStream or openAddrStream
- `tcp.myDottedID` - Likely wraps myAddress + addressDecode

**DNS Helpers**:
- `tcp.dns.getDomainName` - Wraps addressToName
- `tcp.dns.getDottedId` - Wraps nameToAddress + addressDecode
- `tcp.dns.getMyDomainName` - Wraps addressToName(myAddress())

**High-Level Protocols**:
- `tcp.httpClient` - Full HTTP client
- `tcp.httpReadUrl` - Simplified HTTP GET
- `tcp.sendMail` - Full SMTP client

---

## Implementation Priority for Phase 1

Based on dependency analysis, implement in this order:

**Phase 1A - Core Socket Operations**:
1. `tcp.openNameStream` / `tcp.openAddrStream`
2. `tcp.readStream` / `tcp.writeStream`
3. `tcp.closeStream` / `tcp.abortStream`
4. `tcp.countConnections`

**Phase 1B - DNS/Address Utilities**:
5. `tcp.addressEncode` / `tcp.addressDecode` (no network I/O)
6. `tcp.nameToAddress` / `tcp.addressToName` (blocking DNS)
7. `tcp.myAddress`

**Phase 2 - Buffered I/O**:
8. `tcp.readStreamUntil` (enables HTTP client)
9. `tcp.readStreamBytes`
10. `tcp.readStreamUntilClosed`
11. `tcp.writeStringToStream`

**Phase 3 - Server Operations**:
12. `tcp.listenStream` (complex: threading, callbacks)
13. `tcp.closeListen`
14. `tcp.getPeerAddress` / `tcp.getPeerPort`

**Phase 4 - Advanced Features**:
15. `tcp.statusStream`
16. `tcp.writeFileToStream`

---

## Test Cases Required

For each kernel verb, integration tests must verify:

1. **Success path**: Normal operation with valid parameters
2. **Error handling**: Invalid stream IDs, closed connections, timeouts
3. **Resource cleanup**: No stream leaks after close/abort
4. **Thread safety**: Concurrent operations on different streams
5. **Edge cases**: Empty reads, zero-length writes, DNS failures

**Critical test scenarios**:
- HTTP GET request using primitives (validates Phase 1+2)
- Simple echo server using listen callbacks (validates Phase 3)
- Connection timeout and retry logic
- Large data transfer (multi-MB file)
- Concurrent connections (stress test)

---

## Architecture Implications

### Global State Elimination

From WinSockNetEvents.c analysis:
```c
static sockRecord sockstack[FRONTIER_MAX_STREAM];  // PROBLEM: Global mutable
static short frontierWinSockCount = 0;             // PROBLEM: Global counter
```

**Solution**: Move to context structure:
```c
typedef struct tcp_context {
    sockRecord      streams[FRONTIER_MAX_STREAM];
    short           active_count;
    pthread_mutex_t lock;  // Thread safety
} tcp_context;
```

### Thread Safety Requirements

- `tcp.listenStream` callbacks run in **separate threads**
- Must protect `sockstack[]` with mutex
- Stream ID allocation must be atomic
- Callback script execution needs thread-local database context

### Event Loop Integration

**Options**:
1. **Blocking I/O** (simplest): Each operation blocks until complete
2. **Non-blocking + select/poll**: Event loop for async operations
3. **Hybrid**: Blocking for most ops, threads for listen callbacks

**Recommendation**: Start with blocking I/O (Phase 1-2), add event loop if needed.

---

## Next Steps

1. Create POSIX socket architecture document
2. Design verb registration and dispatch mechanism
3. Plan test infrastructure (integration test framework)
4. Implement Phase 1A (core socket operations)
5. Validate with simple HTTP GET test case
