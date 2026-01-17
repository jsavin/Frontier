# TCP Networking - Preliminary Findings

**Status**: Analysis in progress - 4 agents running in parallel
**Date**: 2026-01-16

## Quick Facts

- **Total TCP Documentation Files**: 27 verbs documented
- **Implementation Location**: UserTalk glue in `usertalk_scripts/Frontier.root/system/verbs/builtins/tcp/`
- **Existing Kernel Code**: `Common/source/WinSockNetEvents.c` (Windows-specific, ~34k tokens)
- **Need**: POSIX-compliant implementation for macOS/Linux CLI

## Verb Categories (Preliminary)

### Core Stream Operations
- `tcp.openStream(adr, port)` - Connect to server (glue calls kernel: openNameStream or openAddrStream)
- `tcp.readStream(stream, bytesToRead)` - Read data (kernel verb)
- `tcp.writeStream(stream, data)` - Write data (kernel verb)
- `tcp.closeStream(stream)` - Close connection (kernel verb)
- `tcp.abortStream(stream)` - Immediate close (kernel verb)

### Server Operations
- `tcp.listenStream(port, depth, callback, refcon, ip)` - Listen for connections (kernel verb with callback)
- `tcp.closeListen(listenID)` - Stop listener (kernel verb)

### Stream Utilities
- `tcp.readStreamUntil(stream, pattern, timeOutSecs, @buffer)` - Read until pattern (kernel verb)
- `tcp.readStreamBytes(stream, bytesToRead, timeOutSecs, @buffer)` - Read with timeout (kernel verb)
- `tcp.readStreamUntilClosed(stream, timeOutSecs, @buffer)` - Read all (kernel verb)
- `tcp.writeStringToStream(stream, data, chunksize, timeout)` - Chunked write (kernel verb)
- `tcp.writeFileToStream(stream, f, prefix, suffix)` - File transfer (kernel verb)
- `tcp.statusStream(stream, @bytesPending)` - Connection status (kernel verb)

### DNS/Address Operations
- `tcp.nameToAddress(domainName)` - DNS lookup → 4-byte number (kernel verb)
- `tcp.addressToName(adr)` - Reverse DNS (kernel verb)
- `tcp.addressEncode(ipAddress)` - Dotted → 4-byte (kernel verb)
- `tcp.addressDecode(encodedAdr)` - 4-byte → dotted (kernel verb)
- `tcp.dns.getDomainName(adr)` - Domain name lookup
- `tcp.dns.getDottedId(name)` - Name → dotted IP
- `tcp.myAddress()` - Local address (kernel verb)
- `tcp.myDottedID()` - Local IP as string

### Connection Info
- `tcp.getPeerAddress(stream)` - Remote address (kernel verb)
- `tcp.getPeerPort(stream)` - Remote port (kernel verb)
- `tcp.countConnections()` - Active connection count (kernel verb)

### High-Level Helpers (Pure UserTalk)
- `tcp.httpClient(...)` - Full HTTP client (18 parameters, uses primitives)
- `tcp.httpReadUrl(url, username, password)` - Simple HTTP GET
- `tcp.sendMail(...)` - SMTP client

## Verb Implementation Pattern

Based on file.c pattern:

```c
// 1. Define verb enum
typedef enum tytcptoken {
    openNameStreamfunc,
    openAddrStreamfunc,
    readStreamfunc,
    writeStreamfunc,
    closeStreamfunc,
    // ...
} tytcptoken;

// 2. Implement function dispatcher
boolean tcpfunctionvalue(short token, hdltreenode hparam1, tyvaluerecord *vreturned, bigstring bserror) {
    switch (token) {
        case openNameStreamfunc: {
            // Extract params
            bigstring hostname;
            long port;
            
            if (!getstringvalue(hparam1, 1, hostname))
                return false;
            
            flnextparamislast = true;
            if (!getlongvalue(hparam1, 2, &port))
                return false;
            
            // Call implementation
            long streamid;
            if (!tcp_open_stream_name(hostname, port, &streamid))
                return false;
            
            return setlongvalue(streamid, vreturned);
        }
        // ...
    }
}

// 3. Register in tcpinitverbs()
boolean tcpinitverbs(void) {
    if (!loadfunctionprocessor(idtcpverbs, &tcpfunctionvalue))
        return false;
    return true;
}
```

## Key Design Questions

1. **Blocking vs Non-Blocking**: How should timeouts work in headless CLI?
2. **Event Loop Integration**: Does Frontier CLI have an event loop for async callbacks?
3. **Connection Table**: How to manage stream IDs → socket fd mapping?
4. **Thread Safety**: Listen callbacks run in separate threads - need thread-safe globals
5. **DNS Resolution**: Use getaddrinfo() or legacy gethostbyname()?

## Agents Running

1. **tcp-api-analysis** (Explore): Comprehensive verb inventory from UserTalk + docs
2. **existing-kernel-search** (Explore): Find existing Windows/Mac implementations
3. **networking-architecture-design** (system-architect): POSIX socket architecture design
4. **implementation-roadmap** (Plan): Phased implementation with Haiku/Sonnet markers

## Next Steps

- Wait for agent completion
- Integrate findings into formal planning documents
- Create detailed architecture based on system-architect agent output
- Create phased implementation plan with test strategy
