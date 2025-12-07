# Processor Audit: `tcp`

**Status:** ✅ **Ready for Implementation** (Critical for Network Operations)
**Audit Date:** 2025-12-05
**Auditor:** Claude (Sonnet 4.5)

---

## Basic Information

| Property | Value |
|----------|-------|
| **Processor Name** | `tcp` |
| **EFP ID** | 1015 (lang block) |
| **Verb Count** | 23 kernel verbs (+many script verbs) |
| **Window Required** | NO |
| **Documentation** | [tcp/](../../../docs/usertalk/docserver.userland.com/tcp/index.html) |
| **Stub Implementation** | [headless_tcp_verbs.c](../../../tests/headless_tcp_verbs.c) |

---

## Category Assessment

**Category:** ✅ **Core Network Functionality**

**Rationale:**
Low-level TCP socket operations forming the foundation for all network functionality. Provides stream-based I/O, DNS resolution, and connection management. Many high-level protocols (HTTP, FTP, SMTP, DNS) are implemented as UserTalk scripts built on these primitives.

**Headless Compatibility:** ✅ **Full**

**Blocking Verbs:** None (all verbs are network I/O - headless compatible)

---

## Verb Inventory

### Core Socket Operations (23 kernel verbs)

**Connection Management (6 verbs):**
| # | Verb Name | Signature | Description |
|---|-----------|-----------|-------------|
| 1 | `openaddrstream` | `tcp.openAddrStream(ip, port, timeout) -> streamID` | Open TCP connection by IP |
| 2 | `opennamestream` | `tcp.openNameStream(hostname, port, timeout) -> streamID` | Open TCP connection by hostname |
| 3 | `closestream` | `tcp.closeStream(streamID) -> boolean` | Close TCP connection |
| 4 | `abortstream` | `tcp.abortStream(streamID) -> boolean` | Abort connection (hard close) |
| 5 | `listenstream` | `tcp.listenStream(port, handler) -> listenerID` | Listen for incoming connections |
| 6 | `closelisten` | `tcp.closeListen(listenerID) -> boolean` | Stop listening on port |

**Data Transfer (8 verbs):**
| # | Verb Name | Signature | Description |
|---|-----------|-----------|-------------|
| 7 | `readstream` | `tcp.readStream(streamID, maxBytes) -> data` | Read bytes from stream |
| 8 | `writestream` | `tcp.writeStream(streamID, data) -> boolean` | Write bytes to stream |
| 9 | `writestringtostream` | `tcp.writeStringToStream(streamID, string) -> boolean` | Write string to stream |
| 10 | `writefiletostream` | `tcp.writeFileToStream(streamID, filepath) -> boolean` | Write file contents to stream |
| 11 | `readstreamuntil` | `tcp.readStreamUntil(streamID, delimiter) -> string` | Read until delimiter |
| 12 | `readstreambytes` | `tcp.readStreamBytes(streamID, count) -> data` | Read exact byte count |
| 13 | `readstreamuntilclosed` | `tcp.readStreamUntilClosed(streamID) -> data` | Read all data until close |

**Connection Info (4 verbs):**
| # | Verb Name | Signature | Description |
|---|-----------|-----------|-------------|
| 14 | `statusstream` | `tcp.statusStream(streamID) -> status` | Get connection status |
| 15 | `getpeeraddress` | `tcp.getPeerAddress(streamID) -> ipAddress` | Get remote IP address |
| 16 | `getpeerport` | `tcp.getPeerPort(streamID) -> port` | Get remote port number |
| 17 | `getstats` | `tcp.getStats() -> statsTable` | Get TCP statistics |

**DNS Resolution (4 verbs):**
| # | Verb Name | Signature | Description |
|---|-----------|-----------|-------------|
| 18 | `nametoaddress` | `tcp.nameToAddress(hostname) -> ipAddress` | DNS lookup (name → IP) |
| 19 | `addresstoname` | `tcp.addressToName(ipAddress) -> hostname` | Reverse DNS (IP → name) |
| 20 | `myaddress` | `tcp.myAddress() -> ipAddress` | Get local IP address |

**Address Utilities (3 verbs):**
| # | Verb Name | Signature | Description |
|---|-----------|-----------|-------------|
| 21 | `addressencode` | `tcp.addressEncode(a, b, c, d) -> ipAddress` | Encode IP (192, 168, 1, 1) → "192.168.1.1" |
| 22 | `addressdecode` | `tcp.addressDecode(ipAddress) -> {a, b, c, d}` | Decode IP "192.168.1.1" → [192, 168, 1, 1] |
| 23 | `countconnections` | `tcp.countConnections() -> count` | Count active connections |

---

## Script-Based Verbs (UserTalk Implementations)

Many high-level protocols are implemented as UserTalk scripts:

**HTTP:**
- `tcp.httpGet(url)` - HTTP GET request
- `tcp.httpReadUrl(url)` - Read URL content
- `tcp.httpClient(...)` - Full HTTP client

**FTP:**
- `tcp.ftpOpenConnection(host, user, pass)`
- `tcp.ftpWriteFile(...)`
- `tcp.ftpCloseConnection()`

**Email:**
- `tcp.getMail(...)` - Retrieve email

**DNS:**
- `tcp.dnsGetDomainName(...)`
- `tcp.dnsGetDottedId(...)`
- `tcp.dnsGetMyDomainName()`
- `tcp.dnsGetMyDottedId()`

**Utilities:**
- `tcp.equalNames(...)` - Compare hostnames
- `tcp.getCurrentTime()` - Get network time

These scripts are built on the 23 kernel verbs above.

---

## Implementation Analysis

### Complexity: **MEDIUM-HIGH**

### Dependencies

- **Other Processors:** None (foundational)
- **External Services:** None (OS sockets)
- **OS-Specific Functionality:** YES (socket APIs)
- **GUI/Window Context:** NO

### Key Implementation Notes

**Socket Abstraction:**
```c
// Stream ID = handle to connection context
typedef struct {
    int socketfd;
    char remoteIP[16];
    int remotePort;
    bool isConnected;
    bool isListener;
} tytcpstream;

// Global stream table
tytcpstream* streams[MAX_STREAMS];
```

**Connection Flow:**
```c
// Client connection:
int streamID = tcp.openNameStream("example.com", 80, 30);  // 30 sec timeout
tcp.writeStringToStream(streamID, "GET / HTTP/1.0\r\n\r\n");
string response = tcp.readStreamUntilClosed(streamID);
tcp.closeStream(streamID);

// Server listening:
int listenerID = tcp.listenStream(8080, "handleConnection");
// Kernel calls handleConnection(streamID) for each new connection

on handleConnection(streamID)
    local(request = tcp.readStreamUntil(streamID, "\r\n\r\n"))
    tcp.writeStringToStream(streamID, "HTTP/1.0 200 OK\r\n\r\nHello!")
    tcp.closeStream(streamID)
```

**Platform Socket APIs:**
```c
// POSIX (Unix/Linux/macOS)
socket(), connect(), bind(), listen(), accept()
send(), recv(), close()
getaddrinfo(), getnameinfo()

// Windows
WSAStartup() - required initialization
WSACleanup() - cleanup
Same socket APIs but with WinSock quirks
```

**DNS Resolution:**
```c
// tcp.nameToAddress("example.com") → "93.184.216.34"
struct addrinfo* result;
getaddrinfo("example.com", NULL, &hints, &result);
// Extract IP from result

// tcp.addressToName("93.184.216.34") → "example.com"
getnameinfo(...);  // Reverse DNS
```

**Timeout Handling:**
```c
// tcp.openNameStream timeout:
select() with timeout on connect()
// OR setsockopt(SO_SNDTIMEO, SO_RCVTIMEO)
```

**Error Handling:**
- Connection refused
- Timeout
- DNS resolution failure
- Network unreachable
- Connection reset by peer
- Broken pipe (write to closed socket)

---

## UserTalk Documentation Notes

**Connection:**

**tcp.openNameStream(hostname, port, timeout)**
- Opens TCP connection to hostname:port
- timeout in seconds
- Returns streamID (integer handle)
- Blocks until connected or timeout
- Example: `tcp.openNameStream("www.example.com", 80, 30)`

**tcp.openAddrStream(ipAddress, port, timeout)**
- Same but with IP address instead of hostname
- Faster (no DNS lookup)
- Example: `tcp.openAddrStream("93.184.216.34", 80, 30)`

**tcp.closeStream(streamID)**
- Gracefully closes connection
- Sends TCP FIN packet
- Always call to avoid resource leaks

**tcp.abortStream(streamID)**
- Hard close (TCP RST)
- Use for immediate termination

**Data I/O:**

**tcp.writeStringToStream(streamID, string)**
- Writes string to socket
- Common for text protocols (HTTP, SMTP, etc.)

**tcp.readStreamUntil(streamID, delimiter)**
- Reads until delimiter found
- Example: `tcp.readStreamUntil(streamID, "\r\n")` for line
- Commonly used for line-based protocols

**tcp.readStreamBytes(streamID, count)**
- Reads exact number of bytes
- Blocks until count bytes received
- For binary protocols

**tcp.readStreamUntilClosed(streamID)**
- Reads all data until remote closes
- Returns accumulated data
- Common for HTTP 1.0 (no Content-Length)

**Server:**

**tcp.listenStream(port, handlerScript)**
- Starts listening on port
- Calls handlerScript(streamID) for each connection
- Returns listenerID
- Example: `tcp.listenStream(8080, "http.handleRequest")`

**DNS:**

**tcp.nameToAddress(hostname)**
- DNS forward lookup
- Returns IP address string
- Example: `tcp.nameToAddress("google.com")` → "142.250.185.46"

**tcp.myAddress()**
- Returns local machine's IP address
- Useful for server identification

**Utilities:**

**tcp.addressEncode(a, b, c, d)**
- Constructs IP address from octets
- Example: `tcp.addressEncode(192, 168, 1, 1)` → "192.168.1.1"

**tcp.addressDecode(ipAddress)**
- Parses IP into octets
- Returns list [a, b, c, d]

---

## Testing Requirements

**Test Scenarios:**

**Basic Client:**
```usertalk
// HTTP GET
local(stream = tcp.openNameStream("example.com", 80, 30))
tcp.writeStringToStream(stream, "GET / HTTP/1.0\r\nHost: example.com\r\n\r\n")
local(response = tcp.readStreamUntilClosed(stream))
tcp.closeStream(stream)
// Verify response contains "HTTP/1.0 200"
```

**Basic Server:**
```usertalk
// Echo server
on echoHandler(streamID)
    local(data = tcp.readStreamUntil(streamID, "\n"))
    tcp.writeStringToStream(streamID, "ECHO: " + data + "\n")
    tcp.closeStream(streamID)

local(listener = tcp.listenStream(9999, "echoHandler"))
// Connect from external client: telnet localhost 9999
tcp.closeListen(listener)
```

**DNS:**
```usertalk
tcp.nameToAddress("google.com")          → valid IP
tcp.addressToName("8.8.8.8")             → "dns.google"
tcp.myAddress()                           → local IP
```

**Error Handling:**
```usertalk
tcp.openNameStream("nonexistent.invalid", 80, 5)  → error/timeout
tcp.openAddrStream("192.0.2.1", 9999, 5)          → connection refused
```

**Concurrent Connections:**
- Open multiple streams simultaneously
- Verify each has independent state
- Test connection limits

**Large Data Transfer:**
- Send/receive >1MB data
- Verify no corruption
- Test chunked reading

**Platform Differences:**
- IPv6 support varies
- Windows vs Unix error codes
- DNS resolver behavior

---

## Implementation Effort

**Estimated Time:** 30-40 hours

**Breakdown:**
- Socket abstraction layer: 6-8 hours
- Connection management: 6-8 hours
- Data I/O operations: 8-10 hours
- DNS resolution: 4-6 hours
- Server listening (accept loop): 4-6 hours
- Error handling & cleanup: 4-6 hours
- Testing: 6-8 hours

**Confidence:** MEDIUM - Standard socket programming, but cross-platform abstraction adds complexity

**Platform-Specific Work:**
- Windows: WSAStartup/WSACleanup initialization
- Timeout handling (platform variations)
- Error code mapping (errno vs WSAGetLastError)
- IPv4 vs IPv6 support

---

## Priority & Sequencing

**Priority:** 🏆 **CRITICAL** (Tier 0 - Foundational for Network)

**Recommended Implementation Order:** 17 (before inetd, webserver, http)

**Blockers/Prerequisites:** None (foundational processor)

**Implementation Sequence:**
1. Platform abstraction layer (Windows/Unix sockets)
2. Stream management (open/close)
3. Basic I/O (read/write)
4. DNS resolution
5. Server listening
6. Advanced I/O (readuntil, readbytes, etc.)
7. Connection info & stats
8. Testing & hardening

---

## Headless Compatibility Analysis

**Fully Compatible:** ✅ YES (23/23 kernel verbs)

All TCP socket operations are headless-compatible:
- Connection management (6 verbs)
- Data transfer (8 verbs)
- Connection info (4 verbs)
- DNS resolution (3 verbs)
- Address utilities (3 verbs)

**Script Verbs:** ✅ Mostly compatible
- HTTP, FTP, email clients work in headless
- Most high-level protocols are headless-compatible

---

## Related Processors

- **inetd** - Internet daemon (uses tcp for connections)
- **webserver** - HTTP server (built on tcp)
- **http** - HTTP utilities (built on tcp)
- **file** - File operations (tcp.writeFileToStream)

---

## Special Considerations

**Thread Safety:**
- Each stream should be thread-safe
- Concurrent reads/writes to same stream need synchronization
- Listener accept loop runs in separate thread

**Resource Management:**
- Limit max concurrent connections
- Clean up on script termination
- Detect and close stale connections
- Prevent socket descriptor leaks

**Security:**
- No SSL/TLS in base tcp processor
- Application-level encryption required
- Validate DNS responses (DNS spoofing)
- Rate limiting for server sockets

**IPv4 vs IPv6:**
- Modern systems support both
- Use getaddrinfo() for dual-stack support
- Address encoding handles IPv4 only (4 octets)
- May need separate IPv6 functions

**Timeouts:**
- Connect timeout (openNameStream, openAddrStream)
- Read timeout (prevent infinite blocking)
- Write timeout
- Idle connection timeout

**Error Codes:**
- Map OS error codes to meaningful messages
- Unix: errno (ECONNREFUSED, ETIMEDOUT, etc.)
- Windows: WSAGetLastError()

**Binary vs Text:**
- All I/O is binary
- writeStringToStream converts string → bytes
- readStreamUntil returns string
- Support both text and binary protocols

**Buffer Management:**
- Internal buffering for readStreamUntil
- Efficient memory handling for large transfers
- Stream data in chunks (don't load GB files into memory)

---

## References

**Documentation:**
- DocServer: `docs/usertalk/docserver.userland.com/tcp/`

**Implementation:**
- Stub: `tests/headless_tcp_verbs.c`
- Canonical verb list: `Common/resources/Win32/kernelverbs.rc` (lines 441-466)

**Standards:**
- POSIX Sockets: BSD socket API
- Windows: WinSock 2.2
- DNS: RFC 1035, getaddrinfo/getnameinfo
- TCP/IP: RFC 793 (TCP), RFC 791 (IP)

---

## Next Steps

1. ✅ Audit complete - ready for implementation
2. ⏳ Create cross-platform socket abstraction layer
3. ⏳ Implement stream management (open/close)
4. ⏳ Implement basic I/O (read/write)
5. ⏳ Implement DNS resolution
6. ⏳ Implement server listening (accept loop)
7. ⏳ Implement advanced I/O (delimiters, byte counts)
8. ⏳ Add connection info verbs
9. ⏳ Comprehensive testing (client, server, DNS)
10. ⏳ Performance optimization & resource cleanup
11. ⏳ Update implementation status

---

**Audit Status:** ✅ Complete and Approved for Implementation

**Implementation Strategy:**
- **23 kernel verbs** for low-level socket operations
- **Many script verbs** for high-level protocols (HTTP, FTP, etc.)
- **Critical foundation** for all network functionality
- **30-40 hours effort** for full implementation
- **Highest priority** (implement before inetd, webserver, http)

**Key Success Factors:**
1. Robust cross-platform abstraction
2. Proper error handling & timeouts
3. Resource cleanup (no socket leaks)
4. Thread-safe stream management
5. Comprehensive testing (client, server, edge cases)
