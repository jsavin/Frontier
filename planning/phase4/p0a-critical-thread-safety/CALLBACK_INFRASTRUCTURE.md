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

## Legacy Implementation Analysis

**Source**: `/Users/jake/dev/tedchoward/Frontier` - Original Frontier codebase review

### 1. How Legacy Code Invoked Callbacks WITH Parameters

Legacy Frontier used **two distinct mechanisms** for parameterized callbacks:

#### Mechanism 1: String Template Substitution (`parsedialogstring`)

**Pattern**: Build callback script string with parameter substitution using `^0`, `^1`, `^2`, `^3` placeholders.

**Function**: `parsedialogstring()` (Common/source/strings.c:1385-1451)

**How It Worked**:
```c
// Window close callback - from shellwindow.c:1871
tyvaluerecord val;
bigstring bsname, bsscript, bsresult;

// Get callback script template: "system.callbacks.closeWindow(^0)"
getsystemtablescript(idclosewindowscript, bsscript);

// Convert window handle to string (title or ODB address)
setwinvalue(pwindow, &val);
coercetostring(&val);
pullstringvalue(&val, bsname);

// Substitute parameter into script template
parsedialogstring(bsscript, bsname, nil, nil, nil, bsscript);
// Result: "system.callbacks.closeWindow("Document Title")"

// Execute substituted script
langrunstringnoerror(bsscript, bsresult);
```

**Callback Template Examples** (from `Common/resources/Mac/lang.r`):
```c
"system.callbacks.closeWindow(�^0�)"           // 1 parameter
"system.callbacks.saveWindow(�^0�,^1)"         // 2 parameters
"system.callbacks.control2click(�^0�)"         // 1 parameter
"system.callbacks.compileChangedScript(address(�^0�))" // 1 parameter wrapped
```

**Advantages**:
- ✅ Simple string substitution
- ✅ Works with existing infrastructure
- ✅ Template defined in resources (easy to customize)
- ✅ Handles up to 4 parameters (`^0` through `^3`)

**Disadvantages**:
- ❌ String escaping complexity (quotes, special characters)
- ❌ Limited to string-coercible parameters
- ❌ No type safety
- ❌ Parameter limit (4 max)
- ❌ Can't pass complex types (handles, addresses) reliably

#### Mechanism 2: List-Based Parameter Passing (`langrunscriptcode`)

**Pattern**: Build parameter list as `tyvaluerecord` list, pass to script execution.

**Function**: `langrunscriptcode()` (Common/source/lang.c:1375-1461)

**How It Worked** (from regexp callback - `langregexp.c:1582-1663`):
```c
// 1. Create local variable table for parameters
hdlhashtable htlocals = nil;
langpushlocalchain(&htlocals);

// 2. Assign parameters to local table
hashtableassign(htlocals, STR_matchInfo, vmatchinfo);
hashtableassign(htlocals, STR_replacementString, vrepl);

// 3. Build parameter list referencing locals
hdllistrecord hparams;
opnewlist(&hparams, false);
setheapvalue((Handle)hparams, listvaluetype, &vparams);
langpushlistaddress(hparams, htlocals, STR_matchInfo);
langpushlistaddress(hparams, htlocals, STR_replacementString);

// 4. Get code tree for callback script
hdltreenode hcode;
getcodetreefromscriptaddress(adrcallback.ht, adrcallback.bs, &hcode);

// 5. Execute callback with parameter list
langrunscriptcode(adrcallback.ht, adrcallback.bs, hcode, &vparams, nil, &vresult);

// 6. Clean up local table
langpoplocalchain(htlocals);
```

**Key Functions**:
- `langpushlocalchain()` - Create temporary local variable scope
- `langpushlistaddress()` - Build list of address parameters (Common/source/langlist.c:123)
- `langrunscriptcode()` - Execute script with parameter list (Common/source/lang.c:1375)
- `langbuildparamlist()` - Convert list value to parameter tree (Common/source/lang.c:1307)
- `langpoplocalchain()` - Dispose temporary scope

**Advantages**:
- ✅ Type-safe parameter passing
- ✅ Handles complex types (handles, addresses, tables)
- ✅ No parameter limit
- ✅ No string escaping issues
- ✅ Parameters accessible as named variables in callback
- ✅ Can pass parameters by address (local variables)

**Disadvantages**:
- ❌ More complex C API
- ❌ Requires local table creation/cleanup
- ❌ More invasive to thread-safety model

**This is the CORRECT pattern for modern implementation.**

### 2. Window Handles vs ODB Addresses - Critical Distinction

**User's Warning**: Many legacy callbacks take an `adr` parameter that refers to a WINDOW HANDLE, not an ODB object address.

**Evidence from `setwinvalue()`** (shellwindowverbs.c:188-251):

```c
boolean setwinvalue(WindowPtr pwindow, tyvaluerecord *val) {
    /*
    set val to the address of the database object contained by pwindow

    5.0b16 dmb: for database objects, return an address, not a string
    */

    hdlexternalvariable hvariable = nil;
    hdlhashtable htable;
    bigstring bs;

    // Try to find database object behind window
    fl = (*shellglobals.getvariableroutine)(&hvariable);
    fl = langexternalfindvariable(hvariable, &htable, bs);

    if (fl) {
        // Window represents a database object - return ODB address
        return setaddressvalue(htable, bs, val);
    } else {
        // Window doesn't represent DB object - return title string
        shellgetwindowtitle(hinfo, bs);
        return setstringvalue(bs, val);
    }
}
```

**Key Insight**: Legacy code converted window handles to EITHER:
1. **ODB address** - If window displayed a database object (table, script, outline)
2. **String (title)** - If window was NOT backed by database object (file, standalone window)

**Modern CLI Adaptation Required**:

Since modern CLI is **headless** (no GUI windows):

| Legacy Callback | Window Parameter | CLI Adaptation |
|----------------|------------------|----------------|
| `openWindow(adr)` | Window title or ODB path | **Map to ODB address** - CLI opens database objects, not windows |
| `closeWindow(adr)` | Window title or ODB path | **Map to ODB address** - CLI closes database objects |
| `saveWindow(adr, flag)` | Window title + save flag | **Map to ODB address** - CLI saves database objects |

**Recommendation for Modern Implementation**:
- ✅ **Keep callbacks** - Still useful for lifecycle hooks
- ✅ **Replace window handle with ODB address** - CLI operates on database objects
- ✅ **Use string path for file operations** - When object isn't in ODB

**Example Modern Mapping**:
```c
// Legacy: closeWindow(window_handle) → converts to title or ODB address
// Modern CLI: closeWindow(odb_address_string)

// When closing a table object:
tyvaluerecord param;
setaddressvalue(htable, bsname, &param);
langruncallbackwithparams(BIGSTRING("\psystem.callbacks.closeWindow"), 1, &param, &result);
```

### 3. GUI-Only vs Platform-Wide Callbacks

**Analysis**: Which callbacks are GUI-specific vs headless-relevant?

| Callback | Category | CLI Relevance | Notes |
|----------|----------|---------------|-------|
| **openWindow** | Window | ✅ ADAPT | Map to "open database object" |
| **closeWindow** | Window | ✅ ADAPT | Map to "close database object" |
| **saveWindow** | Window | ✅ ADAPT | Map to "save database object" |
| **cmd2click** | UI | ❌ GUI-ONLY | Command-click on object (GUI interaction) |
| **control2click** | UI | ❌ GUI-ONLY | Control-click on object (GUI interaction) |
| **option2click** | UI | ❌ GUI-ONLY | Option-click on object (GUI interaction) |
| **systemTrayIconRightClick** | UI | ❌ GUI-ONLY | System tray interaction (Windows GUI) |
| **opExpand** | Outline | ⚠️ PARTIAL | Relevant if headless outline operations exist |
| **opCollapse** | Outline | ⚠️ PARTIAL | Relevant if headless outline operations exist |
| **opInsert** | Outline | ⚠️ PARTIAL | Relevant if headless outline operations exist |
| **opCursorMoved** | Outline | ❌ GUI-ONLY | Visual cursor movement |
| **opStruct2Click** | Outline | ❌ GUI-ONLY | Double-click in outline |
| **opRightClick** | Outline | ❌ GUI-ONLY | Right-click in outline |
| **opReturnKey** | Outline | ❌ GUI-ONLY | Return key in outline editor |
| **suspend** | System | ✅ KEEP | Application suspend (relevant for server) |
| **resume** | System | ✅ KEEP | Application resume (relevant for server) |
| **compileChangedScript** | System | ✅ KEEP | Script recompilation hook |
| **tcp.setOffline** | Network | ✅ KEEP | Network state change |

**Recommendation**:
- **Implement for CLI**: openWindow, closeWindow, saveWindow, suspend, resume, compileChangedScript, tcp.* callbacks
- **Skip for CLI**: All click/UI callbacks, cursor movement, keyboard callbacks
- **Future consideration**: Outline operation callbacks IF headless outline manipulation is added

### 4. Design Patterns to Adopt

**Pattern 1: List-Based Parameter Passing** ✅ **RECOMMENDED**

**Why**: This is the proven approach for complex, type-safe parameter passing.

**Adoption Strategy**:
```c
boolean langruncallbackwithparams(
    bigstring callback_path,          // e.g., "system.callbacks.closeWindow"
    short param_count,                // Number of parameters
    tyvaluerecord *params,            // Array of parameter values
    tyvaluerecord *result             // Returned value (optional)
) {
    hdlhashtable htlocals = nil;
    hdllistrecord hparams = nil;
    tyvaluerecord vparams;
    hdltreenode hcode = nil;
    hdlhashtable htable;
    bigstring bsverb;
    boolean fl = false;

    // 1. Thread-safety: grab thread context
    grabthreadglobals();
    oppushoutline(op_get_outlinedata());

    // 2. Create local variable scope for parameters
    if (!langpushlocalchain(&htlocals))
        goto cleanup;

    // 3. Assign parameters to local table as param1, param2, etc.
    for (short i = 0; i < param_count; i++) {
        bigstring bsparamname;
        numtostring(i + 1, bsparamname);
        insertstring(BIGSTRING("\x05param"), bsparamname, bsparamname);

        if (!hashtableassign(htlocals, bsparamname, params[i]))
            goto cleanup;
    }

    // 4. Build parameter list
    if (!opnewlist(&hparams, false))
        goto cleanup;

    if (!setheapvalue((Handle)hparams, listvaluetype, &vparams))
        goto cleanup;

    for (short i = 0; i < param_count; i++) {
        bigstring bsparamname;
        numtostring(i + 1, bsparamname);
        insertstring(BIGSTRING("\x05param"), bsparamname, bsparamname);

        if (!langpushlistaddress(hparams, htlocals, bsparamname))
            goto cleanup;
    }

    // 5. Parse callback address
    if (!parsefullname(callback_path, &htable, bsverb))
        goto cleanup;

    // 6. Get code tree for callback script
    if (!getcodetreefromscriptaddress(htable, bsverb, &hcode))
        goto cleanup;

    // 7. Execute callback with parameters
    fl = langrunscriptcode(htable, bsverb, hcode, &vparams, nil, result);

cleanup:
    // 8. Clean up local scope
    if (htlocals != nil)
        langpoplocalchain(htlocals);

    // 9. Thread-safety: restore thread context
    oppopoutline();
    releasethreadglobals();

    return fl;
}
```

**Pattern 2: Thread-Safety Wrapper** ✅ **ALREADY ESTABLISHED**

The `grabthreadglobals()` → work → `releasethreadglobals()` + `oppushoutline()` → `oppopoutline()` pattern is **proven correct** and must be preserved.

**Pattern 3: Backward Compatibility** ✅ **REQUIRED**

Keep existing `langopruncallbackscripts()` for parameterless callbacks:
```c
boolean langopruncallbackscripts(short idscript) {
    bigstring bsscript;
    if (!getsystemtablescript(idscript, bsscript))
        return false;

    // Delegate to new function with zero parameters
    return langruncallbackwithparams(bsscript, 0, nil, nil);
}
```

### 5. Patterns to Avoid

**Anti-Pattern 1: String Template Substitution** ❌ **DON'T USE**

**Why**: Limited to 4 parameters, string escaping issues, no type safety.

**Exception**: May be useful for simple single-string-parameter callbacks where performance matters, but NOT as primary mechanism.

**Anti-Pattern 2: Global Parameter Passing** ❌ **NEVER USE**

Legacy code occasionally used globals to pass parameters. This is NOT thread-safe and must NEVER be adopted.

**Anti-Pattern 3: Direct Window Handle Passing** ❌ **NOT APPLICABLE TO CLI**

Window handles don't exist in headless environment. Always convert to ODB addresses or string paths.

### 6. Implementation Recommendations

**Phase 1: Core Infrastructure** (Week 1)

1. ✅ Implement `langruncallbackwithparams()` using list-based parameter passing
2. ✅ Extend thread-safety pattern (grabthreadglobals/oppushoutline)
3. ✅ Write unit tests for 0, 1, 2, N parameter cases
4. ✅ Document API in `docs/CALLBACK_INFRASTRUCTURE.md`

**Phase 2: TCP Integration** (Week 2)

1. ✅ Refactor `tcp.listenStream()` to use new callback infrastructure
2. ✅ Test with 3-parameter callback: (stream_id, remote_addr, remote_port)
3. ✅ Validate thread-safety with ThreadSanitizer

**Phase 3: Database Lifecycle Callbacks** (Week 3)

1. ✅ Implement `closeWindow` → `closeObject` callback with ODB address
2. ✅ Implement `openWindow` → `openObject` callback with ODB address
3. ⚠️ Consider renaming to `system.callbacks.closeObject` for clarity

**Phase 4: Future Enhancements** (Post-P0a)

1. ⏸️ Named parameter support (pass record instead of list)
2. ⏸️ Async callback queuing (post callback to main thread)
3. ⏸️ Callback error handling and retry logic

### 7. Reference Implementation Files

**Study These Legacy Files**:

| File | Purpose | Key Functions |
|------|---------|---------------|
| `Common/source/lang.c:1375-1461` | Script execution with parameters | `langrunscriptcode()` |
| `Common/source/lang.c:1307-1369` | Parameter list building | `langbuildparamlist()` |
| `Common/source/langregexp.c:1582-1663` | Real-world callback example | `regexprunreplacecallback()` |
| `Common/source/langlist.c:123-140` | List address pushing | `langpushlistaddress()` |
| `Common/source/shellwindow.c:1851-1901` | Window callback pattern | `shellrunwindowconfirmationscript()` |
| `Common/source/strings.c:1385-1451` | String template substitution | `parsedialogstring()` |

**Key Insight from Legacy Code**:

The **regexp callback implementation** (`regexprunreplacecallback`) is the **gold standard** for parameterized callbacks:
- ✅ Uses local variable table for parameters
- ✅ Builds list of addresses
- ✅ Calls `langrunscriptcode()` with parameter list
- ✅ Cleans up local scope properly
- ✅ Type-safe and robust

**This should be the template for modern implementation.**

---

---

## Implementation Complete

**Date**: 2026-01-20
**Status**: IMPLEMENTED - API ready for TCP Phase 3 integration
**Location**: `Common/source/lang.c:1253-1381`, `Common/headers/lang.h:766`

### C API Signature

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
 *
 * Parameters are accessible in UserTalk as param1, param2, param3, etc.
 *
 * Thread-Safety: This function is thread-safe. It uses grabthreadglobals/
 * releasethreadglobals and oppushoutline/oppopoutline wrappers.
 *
 * Implementation Pattern:
 *   1. Create local variable table for parameters
 *   2. Assign parameters to local table with names (param1, param2, ...)
 *   3. Build parameter list using langpushlistaddress()
 *   4. Execute callback with langrunscriptcode()
 *   5. Clean up local scope
 *   6. Wrap in thread-safety pattern (grabthreadglobals/oppushoutline)
 *
 * Based on legacy regexp callback pattern (langregexp.c:1591-1672).
 *
 * Reference: planning/phase4/p0a-critical-thread-safety/CALLBACK_INFRASTRUCTURE.md
 */
extern boolean langruncallbackwithparams (
    hdlhashtable htable,
    bigstring callback_name,
    short param_count,
    tyvaluerecord *params,
    tyvaluerecord *result
);
```

### Usage Examples

#### Example 1: TCP Connection Callback (3 parameters)

```c
// TCP accept thread invokes callback with connection parameters
tyvaluerecord params[3];
setlongvalue(stream_id, &params[0]);       // long: stream ID
setlongvalue(remote_addr, &params[1]);     // long: remote IP address (packed)
setlongvalue(remote_port, &params[2]);     // long: remote port number

tyvaluerecord result;
boolean success = langruncallbackwithparams(
    htable,
    BIGSTRING("\psystem.callbacks.tcpConnection"),
    3,
    params,
    &result
);

if (!success) {
    // Callback execution failed
    log_error(LOG_COMP_NETWORK, "TCP callback failed");
}
```

**UserTalk callback script**:
```usertalk
on tcpConnection(stream, remote_addr, remote_port) {
    local(peer_ip = tcp.addressDecode(remote_addr));
    log.add("Connection from " + peer_ip + ":" + remote_port);

    local(data = tcp.readStream(stream, 1024));
    tcp.writeStream(stream, "HTTP/1.0 200 OK\r\n\r\nHello");
    tcp.closeStream(stream);

    return true
}
```

#### Example 2: Window Close Callback (1 parameter - string)

```c
// Window manager requests permission to close window
tyvaluerecord param;
setstringvalue(BIGSTRING("\pDocument 1"), &param);

tyvaluerecord result;
boolean success = langruncallbackwithparams(
    htable,
    BIGSTRING("\psystem.callbacks.closeWindow"),
    1,
    &param,
    &result
);

boolean allow_close = false;
if (success && result.valuetype == booleanvaluetype) {
    allow_close = result.data.flvalue;
}

if (!allow_close) {
    // User vetoed close - cancel operation
}
```

**UserTalk callback script**:
```usertalk
on closeWindow(title) {
    if title == "Important Document" {
        if not dialog.confirm("Really close " + title + "?") {
            return false  // Veto close
        }
    };
    return true  // Allow close
}
```

#### Example 3: Database Operation Callback (1 parameter - table address)

```c
// Notify callback before closing database object
tyvaluerecord param;
setaddressvalue(htable, bsname, &param);  // Table address

tyvaluerecord result;
boolean success = langruncallbackwithparams(
    htable,
    BIGSTRING("\psystem.callbacks.closeObject"),
    1,
    &param,
    &result
);
```

**UserTalk callback script**:
```usertalk
on closeObject(object_address) {
    local(obj_type = typeof(object_address^));
    log.add("Closing object: " + nameof(object_address) + " (type: " + obj_type + ")");
    return true
}
```

#### Example 4: Backward Compatibility (0 parameters)

```c
// Invoke parameterless callback (backward compatible)
tyvaluerecord result;
boolean success = langruncallbackwithparams(
    htable,
    BIGSTRING("\psystem.callbacks.startup"),
    0,         // No parameters
    nil,       // params can be nil when param_count == 0
    &result
);
```

**UserTalk callback script**:
```usertalk
on startup() {
    log.add("System startup complete");
    return true
}
```

### Thread-Safety Guarantees

**CRITICAL**: This function is thread-safe and can be called from worker threads (e.g., TCP accept thread).

**Thread-Safety Pattern**:
1. `grabthreadglobals()` - Acquires per-thread state
2. `oppushoutline()` - Saves outline context
3. Execute callback with local parameter scope
4. `oppopoutline()` - Restores outline context
5. `releasethreadglobals()` - Releases per-thread state

**Result**: Each callback execution runs in isolated thread context, preventing race conditions.

### Error Handling

The function returns `false` on any error:
- Callback script not found
- Parameter marshalling failure
- Script compilation failure
- Script execution error

**Example with error checking**:
```c
tyvaluerecord params[2];
setlongvalue(value1, &params[0]);
setstringvalue(BIGSTRING("\ptest"), &params[1]);

tyvaluerecord result;
if (!langruncallbackwithparams(htable, BIGSTRING("\psystem.callbacks.test"), 2, params, &result)) {
    // Error occurred - check logs for details
    log_error(LOG_COMP_LANG, "Callback execution failed");
    return false;
}

// Success - process result
if (result.valuetype == booleanvaluetype) {
    boolean callback_result = result.data.flvalue;
    // Use callback result
}

// Clean up result value
disposevaluerecord(result, false);
```

### Integration Points

**TCP Phase 3**:
- `tcp.listenStream()` will use this API to invoke user callbacks on incoming connections
- See: `planning/phase4/networking/phase3_server_operations.md`

**Future Use Cases**:
- Window lifecycle callbacks (open, close, save)
- Database change notifications
- Outline operation callbacks
- System lifecycle callbacks with parameters

---

**Last Updated**: 2026-01-20
**Status**: Implementation complete - Ready for TCP Phase 3 integration
**Next Step**: Integrate with `tcp.listenStream()` implementation
