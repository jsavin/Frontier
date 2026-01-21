# Frontier Callback API - C Developer Guide

**Date**: 2026-01-20
**Status**: Production Ready
**Phase**: Phase 4 P0a - Critical Thread-Safety

---

## Overview

Frontier provides a **general-purpose parameterized callback infrastructure** for invoking UserTalk scripts from C code with type-safe parameter passing. This enables:

- **Network callbacks**: TCP connection handlers with stream ID, remote address, port
- **Window callbacks**: Window lifecycle events with window title or object address
- **Database callbacks**: Object lifecycle notifications with table addresses
- **System callbacks**: Startup, shutdown, suspend, resume with context
- **Future callbacks**: Any parameterized callback requirement

---

## API Function

### langruncallbackwithparams()

**Location**: `Common/source/lang.c:1253-1381`
**Header**: `Common/headers/lang.h:766`

```c
/**
 * Execute a UserTalk callback with parameters
 *
 * @param htable         Hash table containing the callback script
 * @param callback_name  Name of the callback script (bigstring)
 * @param param_count    Number of parameters to pass
 * @param params         Array of tyvaluerecord parameters
 * @param result         Pointer to receive return value (can be nil)
 * @return               true if callback executed successfully, false on error
 */
extern boolean langruncallbackwithparams (
    hdlhashtable htable,
    bigstring callback_name,
    short param_count,
    tyvaluerecord *params,
    tyvaluerecord *result
);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `htable` | `hdlhashtable` | Hash table containing the callback script (usually root table) |
| `callback_name` | `bigstring` | Name of callback script (e.g., `BIGSTRING("\psystem.callbacks.tcpConnection")`) |
| `param_count` | `short` | Number of parameters to pass (0 for parameterless callbacks) |
| `params` | `tyvaluerecord *` | Array of parameter values (can be nil if `param_count == 0`) |
| `result` | `tyvaluerecord *` | Pointer to store return value (can be nil if return value not needed) |

### Return Value

- `true` - Callback executed successfully
- `false` - Error occurred (callback not found, parameter marshalling failure, execution error)

### Parameter Access in UserTalk

Parameters are accessible in the callback script as **param1, param2, param3, etc.**

```usertalk
on myCallback(stream_id, remote_addr, remote_port) {
    // param1 = stream_id
    // param2 = remote_addr
    // param3 = remote_port
    return true
}
```

---

## Parameter Types Supported

All Frontier value types are supported:

| C Type | Value Type | Set Function | UserTalk Access |
|--------|------------|--------------|-----------------|
| `long` | `longvaluetype` | `setlongvalue(value, &param)` | `param1` (number) |
| `bigstring` | `stringvaluetype` | `setstringvalue(bs, &param)` | `param1` (string) |
| `boolean` | `booleanvaluetype` | `setbooleanvalue(flag, &param)` | `param1` (boolean) |
| `double` | `doublevaluetype` | `setdoublevalue(dbl, &param)` | `param1` (double) |
| Table address | `addressvaluetype` | `setaddressvalue(ht, bs, &param)` | `param1^` (dereference) |
| Handle | Heap value types | `setheapvalue(h, type, &param)` | `param1` (varies) |

**Example - Mixed parameter types**:
```c
tyvaluerecord params[4];
setlongvalue(123, &params[0]);                    // param1 = 123
setstringvalue(BIGSTRING("\ptest"), &params[1]);  // param2 = "test"
setbooleanvalue(true, &params[2]);                // param3 = true
setaddressvalue(htable, bsname, &params[3]);      // param4 = @system.some.object

langruncallbackwithparams(htable, BIGSTRING("\pmyCallback"), 4, params, nil);
```

---

## Memory Management

### Parameter Array Ownership

**IMPORTANT**: The caller retains ownership of the `params` array and must manage its lifecycle.

- **Parameter values are COPIED**: The callback infrastructure uses `hashtableassign()` to copy parameter values into the local scope, not reference them.
- **Caller retains ownership**: The `params` array and its contents remain owned by the caller.
- **Disposal responsibility**: Caller must dispose `params` array elements after `langruncallbackwithparams()` returns.

**Example - Proper parameter disposal**:
```c
// Create parameters
tyvaluerecord params[3];
setlongvalue(stream_id, &params[0]);
setlongvalue(remote_addr, &params[1]);
setlongvalue(remote_port, &params[2]);

// Execute callback (parameters are COPIED internally)
boolean success = langruncallbackwithparams(htable, callback_name, 3, params, nil);

// Clean up parameters (caller's responsibility)
for (short i = 0; i < 3; i++) {
    disposevaluerecord(params[i], false);
}
```

### Return Value Ownership

If `result != nil`, the caller receives a **copy** of the callback's return value and owns it.

- **Copied value**: The return value is copied via `copyvaluerecord()`, not referenced.
- **Caller must dispose**: Caller must call `disposevaluerecord(*result, false)` when done with the result.
- **nil result allowed**: If `result == nil`, the callback's return value is discarded (no cleanup needed).

**Example - Handling return value**:
```c
tyvaluerecord result;

// Execute callback and capture return value
if (langruncallbackwithparams(htable, callback_name, 0, nil, &result)) {
    // Use result...
    if (result.valuetype == booleanvaluetype) {
        boolean accept = result.data.flvalue;
        // ... handle boolean result
    }

    // CRITICAL: Dispose result when done (caller owns it)
    disposevaluerecord(result, false);
}
```

### Memory Safety Rules

1. **Never pass stack-allocated strings as parameters without copying**: Use `setstringvalue()` which copies the string.
2. **Never assume parameter lifetime**: Parameters are copied, so modifying the original after the call has no effect.
3. **Always dispose heap values**: Any heap-allocated parameter (strings, lists, tables) must be disposed by the caller.
4. **nil result pointer is safe**: If you don't need the return value, pass `result = nil` (no cleanup needed).

---

## Thread-Safety Guarantees

**CRITICAL**: This function is **thread-safe** and can be called from worker threads.

### Thread-Safety Pattern

The implementation follows this pattern:

```c
boolean langruncallbackwithparams(...) {
    // 1. Acquire thread context
    grabthreadglobals();
    oppushoutline(op_get_outlinedata());

    // 2. Execute callback in isolated context
    // ... (parameter setup, script execution)

    // 3. Release thread context
    oppopoutline();
    releasethreadglobals();

    return success;
}
```

### What This Guarantees

- **Isolated execution**: Each callback runs in its own thread context
- **No global state pollution**: Thread-local variables are isolated
- **Safe from worker threads**: Can be called from TCP accept thread, background threads
- **No explicit locking required**: Thread-safety is handled internally

### Example - TCP Accept Thread

```c
// This code runs in TCP accept thread (NOT main thread)
void tcp_accept_thread(void *context) {
    while (listening) {
        int client_fd = accept(server_fd, ...);

        // Safe to call from worker thread
        tyvaluerecord params[3];
        setlongvalue(stream_id, &params[0]);
        setlongvalue(remote_addr, &params[1]);
        setlongvalue(remote_port, &params[2]);

        langruncallbackwithparams(
            htable,
            BIGSTRING("\psystem.callbacks.tcpConnection"),
            3,
            params,
            nil  // Don't need return value
        );
    }
}
```

---

## Usage Examples

### Example 1: TCP Connection Callback

**C Code** (from TCP accept thread):
```c
// Accept incoming connection
int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
long stream_id = register_stream(client_fd);

// Build parameters
tyvaluerecord params[3];
setlongvalue(stream_id, &params[0]);
setlongvalue(ntohl(client_addr.sin_addr.s_addr), &params[1]);
setlongvalue(ntohs(client_addr.sin_port), &params[2]);

// Invoke callback
tyvaluerecord result;
boolean success = langruncallbackwithparams(
    htable,
    BIGSTRING("\psystem.callbacks.tcpConnection"),
    3,
    params,
    &result
);

if (!success) {
    log_error(LOG_COMP_NETWORK, "TCP callback failed");
    tcp_close_stream(stream_id);
}
```

**UserTalk Callback**:
```usertalk
on tcpConnection(stream, remote_addr, remote_port) {
    local(peer_ip = tcp.addressDecode(remote_addr));
    log.add("Connection from " + peer_ip + ":" + remote_port);

    // Simple HTTP response
    local(response = "HTTP/1.0 200 OK\r\n\r\nHello, World!");
    tcp.writeStream(stream, response);
    tcp.closeStream(stream);

    return true
}
```

### Example 2: Window Close Callback

**C Code** (from window manager):
```c
// User requested window close - ask callback for permission
tyvaluerecord param;
setstringvalue(window_title, &param);

tyvaluerecord result;
boolean success = langruncallbackwithparams(
    htable,
    BIGSTRING("\psystem.callbacks.closeWindow"),
    1,
    &param,
    &result
);

boolean allow_close = true;  // Default: allow close
if (success && result.valuetype == booleanvaluetype) {
    allow_close = result.data.flvalue;
}

disposevaluerecord(result, false);

if (allow_close) {
    // Close window
    close_window(window);
} else {
    // User vetoed close
    log_info(LOG_COMP_SHELL, "Window close vetoed by callback");
}
```

**UserTalk Callback**:
```usertalk
on closeWindow(title) {
    if title == "Important Document" {
        if not dialog.confirm("Really close " + title + "? Unsaved changes will be lost.") {
            return false  // Veto close
        }
    };
    return true  // Allow close
}
```

### Example 3: Database Object Callback (Address Parameter)

**C Code** (from database manager):
```c
// Notify callback before closing database object
tyvaluerecord param;
setaddressvalue(htable, object_name, &param);

tyvaluerecord result;
boolean success = langruncallbackwithparams(
    htable,
    BIGSTRING("\psystem.callbacks.closeObject"),
    1,
    &param,
    &result
);

if (success) {
    log_trace(LOG_COMP_DB, "closeObject callback returned: %d",
              (result.valuetype == booleanvaluetype) ? result.data.flvalue : 0);
    disposevaluerecord(result, false);
}
```

**UserTalk Callback**:
```usertalk
on closeObject(object_address) {
    // object_address is a table address - dereference to access
    local(obj_type = typeof(object_address^));
    local(obj_name = nameof(object_address));

    log.add("Closing object: " + obj_name + " (type: " + obj_type + ")");

    // Perform cleanup if needed
    if obj_type == tableType {
        local(count = sizeOf(object_address^));
        log.add("  Table has " + count + " items");
    };

    return true
}
```

### Example 4: Backward Compatibility (No Parameters)

**C Code** (from system startup):
```c
// Invoke parameterless callback
tyvaluerecord result;
boolean success = langruncallbackwithparams(
    htable,
    BIGSTRING("\psystem.callbacks.startup"),
    0,         // No parameters
    nil,       // params can be nil when param_count == 0
    &result
);

if (!success) {
    log_warn(LOG_COMP_SYSTEM, "Startup callback failed");
}
```

**UserTalk Callback**:
```usertalk
on startup() {
    log.add("System startup complete at " + clock.now());
    return true
}
```

### Example 5: Multiple Parameters with Mixed Types

**C Code**:
```c
tyvaluerecord params[5];
setlongvalue(request_id, &params[0]);               // param1: long
setstringvalue(BIGSTRING("\pGET"), &params[1]);     // param2: string
setaddressvalue(htable, bspath, &params[2]);        // param3: address
setbooleanvalue(use_ssl, &params[3]);               // param4: boolean
setdoublevalue(timeout_secs, &params[4]);           // param5: double

tyvaluerecord result;
langruncallbackwithparams(
    htable,
    BIGSTRING("\psystem.callbacks.httpRequest"),
    5,
    params,
    &result
);
```

**UserTalk Callback**:
```usertalk
on httpRequest(request_id, method, path_address, use_ssl, timeout) {
    local(path = nameof(path_address));
    log.add("HTTP " + method + " request #" + request_id + " to " + path);
    log.add("  SSL: " + use_ssl + ", timeout: " + timeout + " seconds");
    return true
}
```

---

## Error Handling

### Return Value

The function returns `false` on any error:
- Callback script not found in hash table
- Parameter marshalling failure
- Script compilation failure
- Script execution error

### Best Practices

**Always check return value**:
```c
if (!langruncallbackwithparams(htable, callback_name, param_count, params, &result)) {
    // Error occurred - check logs for details
    log_error(LOG_COMP_LANG, "Callback execution failed: %.*s",
              (int)callback_name[0], &callback_name[1]);
    return false;
}
```

**Clean up result value**:
```c
tyvaluerecord result;
if (langruncallbackwithparams(..., &result)) {
    // Use result
    if (result.valuetype == booleanvaluetype) {
        boolean success = result.data.flvalue;
    }

    // Always dispose result when done
    disposevaluerecord(result, false);
}
```

**Handle missing callbacks gracefully**:
```c
// Optional callback - don't fail if not defined
tyvaluerecord result;
if (langruncallbackwithparams(htable, BIGSTRING("\psystem.callbacks.optional"), 0, nil, &result)) {
    // Callback exists and executed successfully
    log_trace(LOG_COMP_SYSTEM, "Optional callback executed");
    disposevaluerecord(result, false);
} else {
    // Callback not defined or failed - continue anyway
    log_trace(LOG_COMP_SYSTEM, "Optional callback not defined");
}
```

---

## Integration with tcp.listenStream

The primary use case for this API is TCP server callbacks.

### tcp.listenStream() Usage

**UserTalk**:
```usertalk
on handleConnection(stream, remote_addr, remote_port) {
    local(peer_ip = tcp.addressDecode(remote_addr));
    log.add("Accepted connection from " + peer_ip + ":" + remote_port);

    local(data = tcp.readStream(stream, 1024));
    tcp.writeStream(stream, "HTTP/1.0 200 OK\r\n\r\nHello");
    tcp.closeStream(stream);
}

// Start listening on port 8080
tcp.listenStream(8080, 5, @handleConnection, 0)
```

### C Implementation (tcpverbs.c)

```c
static boolean tcplistenstreamfunc(tyvaluerecord *vparams, tyvaluerecord *vreturned) {
    // Extract parameters
    long port, backlog, refcon;
    tyvaluerecord callback_address;

    getlongvalue(hparam1, 1, &port);
    getlongvalue(hparam2, 2, &backlog);
    copyvaluerecord(vparams[2], &callback_address);  // @handleConnection
    flnextparamislast = true;
    getlongvalue(hparam4, 4, &refcon);

    // Create server socket
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    bind(server_fd, ...);
    listen(server_fd, backlog);

    // Start accept thread
    tcp_start_accept_thread(server_fd, callback_address, refcon);

    return setbooleanvalue(true, vreturned);
}

// Accept thread function
void tcp_accept_thread(void *context) {
    tcp_server_context *ctx = (tcp_server_context *)context;

    while (ctx->listening) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);

        int client_fd = accept(ctx->server_fd, (struct sockaddr *)&client_addr, &addr_len);
        if (client_fd < 0) continue;

        // Register stream
        long stream_id = tcp_register_stream(client_fd);

        // Build callback parameters
        tyvaluerecord params[3];
        setlongvalue(stream_id, &params[0]);
        setlongvalue(ntohl(client_addr.sin_addr.s_addr), &params[1]);
        setlongvalue(ntohs(client_addr.sin_port), &params[2]);

        // Invoke user callback
        hdlhashtable htable = ctx->callback_address.ht;
        bigstring callback_name;
        copystring(ctx->callback_address.bs, callback_name);

        tyvaluerecord result;
        boolean success = langruncallbackwithparams(
            htable,
            callback_name,
            3,
            params,
            &result
        );

        if (!success) {
            log_error(LOG_COMP_NETWORK, "TCP callback failed");
            tcp_close_stream(stream_id);
        } else {
            disposevaluerecord(result, false);
        }
    }
}
```

---

## Best Practices

### 1. Always Use Thread-Safe Pattern

The function handles thread-safety internally. DO NOT add extra locks:

```c
// ✅ CORRECT - Function is thread-safe
langruncallbackwithparams(htable, callback_name, param_count, params, &result);

// ❌ WRONG - Don't add locks (redundant and can cause deadlocks)
pthread_mutex_lock(&lock);
langruncallbackwithparams(...);
pthread_mutex_unlock(&lock);
```

### 2. Clean Up Result Values

Always dispose result values to prevent memory leaks:

```c
tyvaluerecord result;
if (langruncallbackwithparams(..., &result)) {
    // Use result
    boolean success = result.data.flvalue;

    // ✅ ALWAYS dispose when done
    disposevaluerecord(result, false);
}
```

### 3. Use Appropriate Parameter Types

Match C types to UserTalk expectations:

```c
// ✅ CORRECT - Use appropriate type functions
setlongvalue(stream_id, &param);           // Numbers
setstringvalue(BIGSTRING("\ptext"), &param);  // Strings
setbooleanvalue(true, &param);             // Booleans
setaddressvalue(htable, bs, &param);       // ODB addresses

// ❌ WRONG - Don't mix types inappropriately
setlongvalue((long)string_ptr, &param);    // Don't cast pointers to long
```

### 4. Handle Callback Failures Gracefully

Don't crash on callback failures:

```c
if (!langruncallbackwithparams(...)) {
    // ✅ CORRECT - Log error and continue
    log_error(LOG_COMP_NETWORK, "Callback failed - closing connection");
    cleanup_connection(stream_id);
    return false;
}

// ❌ WRONG - Don't crash on failure
assert(langruncallbackwithparams(...));  // Crash on callback failure
```

### 5. Validate Result Types

Check result type before using:

```c
tyvaluerecord result;
if (langruncallbackwithparams(..., &result)) {
    // ✅ CORRECT - Check type before accessing
    if (result.valuetype == booleanvaluetype) {
        boolean allow = result.data.flvalue;
    } else {
        log_warn(LOG_COMP_LANG, "Callback returned unexpected type");
    }
    disposevaluerecord(result, false);
}
```

---

## Common Patterns

### Pattern 1: Optional Callbacks

```c
// Try to invoke callback, continue if not defined
tyvaluerecord result;
if (langruncallbackwithparams(htable, BIGSTRING("\psystem.callbacks.optional"), 0, nil, &result)) {
    log_trace(LOG_COMP_SYSTEM, "Optional callback executed");
    disposevaluerecord(result, false);
}
// Continue regardless of callback existence
```

### Pattern 2: Veto Pattern

```c
// Callback can veto operation by returning false
tyvaluerecord param;
setstringvalue(document_title, &param);

tyvaluerecord result;
boolean allow_operation = true;  // Default: allow

if (langruncallbackwithparams(htable, BIGSTRING("\psystem.callbacks.beforeClose"), 1, &param, &result)) {
    if (result.valuetype == booleanvaluetype) {
        allow_operation = result.data.flvalue;
    }
    disposevaluerecord(result, false);
}

if (allow_operation) {
    perform_operation();
} else {
    log_info(LOG_COMP_SHELL, "Operation vetoed by callback");
}
```

### Pattern 3: Callback Chain

```c
// Invoke multiple callbacks in sequence
bigstring callbacks[] = {
    BIGSTRING("\psystem.callbacks.beforeSave"),
    BIGSTRING("\psystem.callbacks.duringRave"),
    BIGSTRING("\psystem.callbacks.afterSave")
};

for (int i = 0; i < 3; i++) {
    tyvaluerecord param;
    setaddressvalue(htable, object_name, &param);

    tyvaluerecord result;
    if (!langruncallbackwithparams(htable, callbacks[i], 1, &param, &result)) {
        log_error(LOG_COMP_DB, "Callback %d failed - aborting save", i);
        return false;
    }
    disposevaluerecord(result, false);
}
```

---

## Reference Implementation

**Based on**: Legacy regexp callback pattern (`langregexp.c:1591-1672`)

**Implementation Steps**:
1. Create local variable table for parameters
2. Assign parameters to local table with names (param1, param2, ...)
3. Build parameter list using `langpushlistaddress()`
4. Execute callback with `langrunscriptcode()`
5. Clean up local scope
6. Wrap in thread-safety pattern (`grabthreadglobals`/`oppushoutline`)

**Key Functions**:
- `langpushlocalchain()` - Create temporary local variable scope
- `hashtableassign()` - Assign parameter values to local table
- `langpushlistaddress()` - Build list of address parameters
- `langrunscriptcode()` - Execute script with parameter list
- `langpoplocalchain()` - Dispose temporary scope

---

## See Also

- **[CALLBACK_INFRASTRUCTURE.md](../planning/phase4/p0a-critical-thread-safety/CALLBACK_INFRASTRUCTURE.md)** - Architecture and design
- **[phase3_server_operations.md](../planning/phase4/networking/phase3_server_operations.md)** - TCP integration
- **[VERB_IMPLEMENTATION_GUIDE.md](VERB_IMPLEMENTATION_GUIDE.md)** - C verb implementation patterns

---

**Last Updated**: 2026-01-20
**Status**: Production Ready
**Next Step**: Integrate with TCP Phase 3 server implementation
