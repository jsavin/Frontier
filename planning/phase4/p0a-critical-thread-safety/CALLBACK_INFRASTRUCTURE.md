# P0a Component: General-Purpose Parameterized Callback Infrastructure

**Date**: 2026-01-20
**Status**: P0a component - Not TCP-specific, foundation for ALL callbacks
**Priority**: HIGH - Unblocks TCP, window operations, and all parameterized callbacks

---

## Executive Summary

**CRITICAL FINDING**: P0a callback work is NOT just for TCP networking - it's a **general-purpose callback infrastructure** that will support ALL parameterized callbacks throughout Frontier, including:

- **Network callbacks**: `tcp.listenStream()` with (stream_id, remote_addr, remote_port)
- **Window callbacks**: `window.close` with (title)
- **Outline callbacks**: Already exist but could be extended with parameters
- **System callbacks**: startup, shutdown, suspend, resume (currently parameterless)
- **Future callbacks**: Any new callback that needs parameters

This makes P0a **MORE VALUABLE** and **MORE CRITICAL** than a TCP-only solution.

---

## Current State: Existing Callback Infrastructure

### 1. System Callbacks Delegation Pattern

Frontier has an **established callback system** (`system.callbacks.*`) with 22+ callbacks defined:

| Category | Callbacks | Current Parameters |
|----------|-----------|-------------------|
| **Window** | openWindow, saveWindow, closeWindow | String (title), various |
| **Outline** | opCollapse, opExpand, opInsert, opReturnKey, opRightClick, opCursorMoved, opStruct2Click | None (parameterless) |
| **UI** | cmd2Click, control2Click, option2Click, systemTrayIconRightClick | String (object path) |
| **System** | suspend, resume, compileChangedScript | None / various |
| **Network** | tcp.setOffline | None |

**Evidence**: These are defined in `Common/resources/Mac/lang.r` and `Common/resources/Win32/WinLand.rc`:

```c
// Window callbacks WITH parameters
"system.callbacks.closeWindow(^0)"         // String parameter: window title
"system.callbacks.saveWindow(^0,^1)"       // Two parameters

// UI callbacks WITH parameters
"system.callbacks.cmd2click(^0)"           // String parameter: object path
"system.callbacks.control2click(^0)"       // String parameter: object path
"system.callbacks.option2click(^0)"        // String parameter: object path

// Outline callbacks WITHOUT parameters (current limitation)
"if defined(system.callbacks.opExpand){return(system.callbacks.opExpand())}else{return(false)}"
"if defined(system.callbacks.opCollapse){return(system.callbacks.opCollapse())}else{return(false)}"
"if defined(system.callbacks.opInsert){return(system.callbacks.opInsert())}else{return(false)}"
```

### 2. Current C→UserTalk Callback Mechanism

**Function**: `langopruncallbackscripts()` (Common/source/lang.c:1171-1199)

**Current Capabilities**:
- Executes UserTalk callback scripts from C code
- Thread-safe pattern: `grabthreadglobals()` → `oppushoutline()` → `langrunstringnoerror()` → `oppopoutline()` → `releasethreadglobals()`
- Returns boolean result only
- **LIMITATION**: Only supports PARAMETERLESS callbacks

**Current Implementation**:
```c
boolean langopruncallbackscripts (short idscript) {
    boolean fl, flresult = false;
    bigstring bsscript, bsresult;

    if (getsystemtablescript (idscript, bsscript)) {
        grabthreadglobals ();
        oppushoutline (op_get_outlinedata());
        fl = langrunstringnoerror (bsscript, bsresult);
        oppopoutline ();
        releasethreadglobals ();

        if (fl)
            stringisboolean (bsresult, &flresult);
    }

    return (flresult);
}
```

**Used By**:
- Outline operations: `Common/source/op.c`, `opstructure.c`, `opverbs.c`
- Currently limited to callbacks like `opExpand()`, `opCollapse()`, `opInsert()` that take no parameters

### 3. The Gap: Parameterized Callbacks

**What's Missing**: The ability to pass PARAMETERS to callbacks from C code.

**Required for**:
- `tcp.listenStream(port, depth, callback, refcon)` → callback receives `(stream_id, remote_addr, remote_port)`
- `window.close` callbacks with `(title)` parameter
- Future: Outline callbacks with context (e.g., `opInsert(direction, text)`)
- Future: Any parameterized callback needs

**What Already Works**:
- Some callbacks already receive parameters via the string template mechanism (see `closeWindow(^0)`)
- The infrastructure exists but is not generalized for arbitrary parameter passing

---

## Required Enhancement: Parameterized Callback Pattern

### Design Goals

1. **Extend existing pattern**: Build on `langopruncallbackscripts()` infrastructure
2. **Preserve thread-safety**: Maintain grabthreadglobals/releasethreadglobals + oppushoutline/oppopoutline pattern
3. **Support arbitrary parameters**: Pass multiple parameters of different types (long, string, address, etc.)
4. **Backward compatible**: Existing parameterless callbacks continue to work
5. **General-purpose**: Works for ALL callbacks (TCP, window, outline, system, future)

### Proposed API Extension

**New Function Signature**:
```c
boolean langruncallbackwithparams(
    bigstring callback_script,       // UserTalk callback script address
    short param_count,               // Number of parameters
    tyvaluerecord *params,           // Array of parameter values
    tyvaluerecord *result            // Returned value (optional)
);
```

**Thread-Safety Pattern** (already established):
```c
boolean langruncallbackwithparams(...) {
    grabthreadglobals();
    oppushoutline(op_get_outlinedata());

    // Build parameter list
    // Execute callback script with parameters
    // Extract result

    oppopoutline();
    releasethreadglobals();

    return success;
}
```

### Parameter Passing Mechanism

**Option 1: String Templating** (current approach for some callbacks):
- Build script string with parameter substitutions: `callback(param1, param2, param3)`
- Advantages: Simple, works with existing infrastructure
- Disadvantages: String escaping complexity, limited type safety

**Option 2: Stack-Based Parameter Passing**:
- Push parameters onto evaluation stack before calling script
- Script accesses parameters via stack
- Advantages: Type-safe, handles complex types (handles, addresses)
- Disadvantages: More invasive, requires stack infrastructure changes

**Option 3: Temporary Local Variables**:
- Create temporary local table with parameter values
- Pass table address to callback script
- Script accesses via `params.stream_id`, `params.remote_addr`, etc.
- Advantages: Clean API, type-safe, easy debugging
- Disadvantages: Requires table creation/cleanup

**Recommendation**: **Option 3** (Temporary Local Variables) - Most flexible and debuggable.

---

## Use Cases Enabled by This Infrastructure

### 1. TCP Network Callbacks

**tcp.listenStream()** requires passing connection information to callback:

```usertalk
on handleConnection(stream, remote_addr, remote_port, refcon) {
    local(peer_ip = tcp.addressDecode(remote_addr));
    log.add("Accepted connection from " + peer_ip + ":" + remote_port);

    local(data = tcp.readStream(stream, 1024));
    tcp.writeStream(stream, "HTTP/1.0 200 OK\r\n\r\nHello");
    tcp.closeStream(stream);
}

tcp.listenStream(8080, 5, @handleConnection, 0);
```

**C-side invocation** (from accept thread):
```c
// After accepting connection
tyvaluerecord params[4];
setlongvalue(stream_id, &params[0]);          // stream ID
setlongvalue(remote_addr, &params[1]);        // remote IP address
setlongvalue(remote_port, &params[2]);        // remote port
setlongvalue(refcon, &params[3]);             // user refcon

tyvaluerecord result;
langruncallbackwithparams(callback_script, 4, params, &result);
```

### 2. Window Close Callbacks

**window.close** with window title parameter:

```usertalk
on closeWindow(title) {
    if title == "Important Document" {
        if not dialog.confirm("Really close " + title + "?") {
            return false  // Cancel close
        }
    };
    return true  // Allow close
}
```

**C-side invocation**:
```c
tyvaluerecord params[1];
setstringvalue(window_title, &params[0]);

tyvaluerecord result;
langruncallbackwithparams(BIGSTRING("\psystem.callbacks.closeWindow"), 1, params, &result);
boolean allow_close = result.data.flvalue;
```

### 3. Enhanced Outline Callbacks (Future)

**opInsert** with direction and text parameters:

```usertalk
on opInsert(direction, text) {
    log.add("Inserted \"" + text + "\" in direction " + direction);
    return true
}
```

### 4. System Lifecycle Callbacks (Future)

**startup** with command-line arguments:

```usertalk
on startup(args_table) {
    log.add("Starting with args: " + args_table.count + " items");
    // Process startup arguments
}
```

---

## Implementation Roadmap

### Phase 1: Core Infrastructure (Week 1-2)

**Tasks**:
1. Design and implement `langruncallbackwithparams()` function
2. Extend thread-safety pattern to handle parameter setup
3. Implement parameter marshalling (tyvaluerecord array → UserTalk script parameters)
4. Write unit tests for parameter passing
5. Document new API in `docs/CALLBACK_INFRASTRUCTURE.md`

**Deliverable**: General-purpose callback infrastructure

### Phase 2: TCP Integration (Week 2-3)

**Tasks**:
1. Refactor `tcp.listenStream()` to use new callback infrastructure
2. Implement accept thread with callback invocation
3. Write integration tests for TCP callbacks
4. Validate thread-safety with ThreadSanitizer

**Deliverable**: TCP callbacks working end-to-end

### Phase 3: Additional Callbacks (Future)

**Tasks**:
1. Migrate `window.close` callbacks to new infrastructure
2. Enhance outline callbacks with parameters (if needed)
3. Expand system lifecycle callbacks with parameters

---

## Testing Strategy

### Unit Tests (C Level)

```c
// Test parameterless callback (backward compatibility)
test_callback_no_params();

// Test single parameter callback
test_callback_one_param_long();
test_callback_one_param_string();

// Test multiple parameters
test_callback_multiple_params();

// Test thread-safety
test_callback_from_worker_thread();
```

### Integration Tests (UserTalk Level)

```yaml
# tests/integration/test_cases/callbacks.yaml

- name: "Callback with long parameter"
  setup_script: |
    on testCallback(value) {
      return value * 2
    };
    system.callbacks.testCallback = @testCallback

  script: |
    # Invoke from C code (via test harness)
    local(result = internal.invokeCallback("testCallback", 21));
    return result == 42
  expected_success: true
  expected_result: "true"

- name: "Callback with multiple parameters"
  setup_script: |
    on testCallback(a, b, c) {
      return a + b + c
    };
    system.callbacks.testCallback = @testCallback

  script: |
    local(result = internal.invokeCallback("testCallback", 1, 2, 3));
    return result == 6
  expected_success: true
  expected_result: "true"
```

---

## Architectural Benefits

### 1. Foundation for Concurrent Operations

Parameterized callbacks enable **asynchronous operations** throughout Frontier:
- Network I/O callbacks (TCP, HTTP, WebSockets)
- File system callbacks (async file operations)
- Timer callbacks (scheduled tasks with context)
- Database callbacks (change notifications with affected objects)

### 2. Collaborative ODB Alignment

This infrastructure is **essential for multi-user collaborative ODB**:
- Change notification callbacks: `onChange(table_path, changed_keys)`
- Conflict resolution callbacks: `onConflict(local_value, remote_value, merge_strategy)`
- Connection state callbacks: `onConnect(user_id, session_id)`

See: `planning/CRDT_FOUNDATION_ROADMAP.md` - Requires callback infrastructure for event notifications

### 3. Single-Threaded Developer Model

Callbacks preserve the **single-threaded illusion** for UserTalk developers:
- C runtime handles threading complexity
- UserTalk callbacks execute in clean thread context
- No explicit locking required in UserTalk code
- Matches collaborative ODB vision (runtime handles concurrency transparently)

---

## Summary: Why P0a Is More Than TCP

**Original Understanding**: P0a was callback pattern for `tcp.listenStream()` only

**Actual Scope**: P0a is **general-purpose parameterized callback infrastructure** supporting:
- ✅ TCP networking callbacks (stream_id, remote_addr, remote_port)
- ✅ Window lifecycle callbacks (title, state)
- ✅ Enhanced outline callbacks (direction, text, context)
- ✅ System lifecycle callbacks (args, state)
- ✅ Future: All parameterized callbacks across Frontier

**Impact**: This makes P0a **MUCH MORE VALUABLE** than TCP-only solution:
- Unblocks TCP Phase 3 (server operations)
- Unblocks enhanced window management
- Enables future callback-based features
- Foundation for collaborative ODB event notifications
- Aligns with single-threaded developer model vision

**Recommendation**: Treat P0a as **PLATFORM CAPABILITY**, not TCP-specific work. The investment here pays dividends across the entire system.

---

## Reference Implementation

**Current Callback Pattern**: `langopruncallbackscripts()` - Common/source/lang.c:1171-1199

**Thread-Safety Pattern**:
- `grabthreadglobals()` / `releasethreadglobals()` - Acquire/release per-thread state
- `oppushoutline()` / `oppopoutline()` - Save/restore outline context
- `langrunstringnoerror()` - Execute UserTalk script without throwing errors

**Existing Callbacks**: 22+ callbacks defined in:
- `Common/resources/Mac/lang.r` (Mac resource definitions)
- `Common/resources/Win32/WinLand.rc` (Windows resource definitions)

---

**Last Updated**: 2026-01-20
**Status**: Architecture documented, implementation not started
**Next Step**: Design detailed API for `langruncallbackwithparams()` and parameter marshalling strategy
