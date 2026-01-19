# thread.call() - Legacy Implementation Analysis

**Date**: 2026-01-18
**Source**: Legacy Frontier codebase at `/Users/jake/dev/tedchoward/Frontier`
**Context**: Understanding `thread.call()` for headless implementation (PR #318)

---

## Executive Summary

**Key Finding**: `thread.call()` is a **UserTalk wrapper** around the kernel verb `thread.callScript()`.

- **Kernel Implementation**: `thread.callScript()` in `shellsysverbs.c`
- **Code Block Syntax**: The `{ code }` syntax is standard UserTalk - the block is compiled to a code tree before being passed to the verb
- **No Special Magic**: The "code block" parameter is just a compiled code value (codevaluetype), treated like any other parameter

---

## Architecture Overview

### Three-Level Structure

```
1. UserTalk Wrapper:     thread.call(name, { code })
                                  ↓
2. Kernel Verb:          thread.callScript(name, params, context)
                                  ↓
3. C Implementation:     threadcallscriptverb() in shellsysverbs.c
```

### UserTalk Layer: `thread.call()`

**Purpose**: Convenience wrapper that provides cleaner syntax for the common case of calling a code block.

**Expected Implementation**: (Not found in C codebase - likely in system.verbs.builtins.thread table)
```usertalk
// Pseudocode for thread.call wrapper (UserTalk level)
on call (threadName, codeBlock) {
    local params = {}  // Empty parameter record
    return thread.callScript(threadName, codeBlock, params)
}
```

**Note**: The actual implementation would be stored in the Frontier.root database at `system.verbs.builtins.thread.call` (or similar path). We didn't find this in the C source because **it's UserTalk code stored in the ODB**, not C code.

---

## Kernel Verb: `thread.callScript()`

### C Implementation Location

**File**: `/Users/jake/dev/tedchoward/Frontier/Common/source/shellsysverbs.c`
**Function**: `threadcallscriptverb()` (lines 930-1088)
**Token**: `callscriptfunc` (line 182 in `tythreadtoken` enum)
**Dispatcher**: `threadfunctionvalue()` case `callscriptfunc` (line 1212)

### Function Signature

```c
static boolean threadcallscriptverb (
    bigstring bsscriptname,    // Thread name for identification
    tyvaluerecord vparams,     // Parameters (list or record)
    hdlhashtable hcontext,     // Optional context table (can be nil)
    tyvaluerecord *v           // Returned: thread ID (long)
)
```

### UserTalk API

```usertalk
thread.callScript(
    scriptName,    // string: Thread name (for system.compiler.threads table)
    params,        // list or record: Parameters to pass to script
    context        // optional table: Context variables (local namespace)
) -> long        // Returns: thread ID
```

### Parameters Explained

1. **`scriptName`** (string)
   - Thread name for identification in `system.compiler.threads` table
   - Used to identify the thread in debugging/introspection
   - **Important**: This is NOT a path to a script - it's just a label

2. **`params`** (list or record)
   - Can be:
     - **Code block**: `{ x = 1; return x + 1 }`
     - **List**: `{1, 2, 3}`
     - **Record**: `{a: 1, b: 2}`
   - Automatically coerced to list if not already record/list
   - **Key insight**: The code block IS the parameter, not a separate thing

3. **`context`** (optional table)
   - Optional context table providing local variables
   - If provided, packed/unpacked to make independent copy for thread
   - Thread owns its copy (disposed when thread ends)

### Return Value

- **Thread ID** (long integer)
- Used for thread introspection: `thread.exists(id)`, `thread.wake(id)`, etc.
- Returns 0 on failure

---

## How Code Blocks Work

### The "Magic" of `{ code }` Syntax

**There is no magic.** The code block syntax is standard UserTalk:

1. **Parser Stage**: `{ x = 1; return x + 1 }` is parsed as a **code block** token
2. **Compilation**: Code block is compiled to a syntax tree (hdltreenode)
3. **Value Wrapping**: Tree wrapped in `tyvaluerecord` with `valuetype = codevaluetype`
4. **Parameter Passing**: Code value passed as normal parameter to `thread.callScript()`

### Code Block as Parameter

```usertalk
// This:
thread.call("worker", {
    system.test.flag = true
})

// Is equivalent to:
local codeBlock = { system.test.flag = true }
thread.callScript("worker", codeBlock, nil)
```

The code block is just a **compiled code value**, treated exactly like:
- String values: `"hello"`
- Number values: `42`
- Table values: `@system.test`

---

## C Implementation Details

### Core Flow (threadcallscriptverb)

```c
// 1. Expand dotted path to find handler (e.g., "system.test.handler")
langexpandtodotparams(bsscriptname, &htable, bsverb)

// 2. Look up handler node in hash table
hashtablelookupnode(htable, bsverb, &handlernode)
vhandler = (**handlernode).val

// 3. Handle code values vs external values
if (vhandler.valuetype == codevaluetype) {
    hcode = vhandler.data.codevalue  // Already compiled
}
else if ((**htable).valueroutine == nil) {
    langexternalvaltocode(vhandler, &hcode)  // Unpack from ODB
    if (hcode == nil)
        langcompilescript(handlernode, &hcode)  // Compile on demand
}

// 4. Build function call tree
pushfunctionreference(val, &hfunctioncall)
langbuildparamlist(&vparams, &hparamlist)  // Parameters from vparams
pushfunctioncall(hfunctioncall, hparamlist, &hcode)

// 5. Launch separate process
addnewprocess(hcode, true, nil, 0)
scheduleprocess(hp, &hthread)

// 6. Set up context (if provided)
if (hcontext != nil) {
    // Pack/unpack to create independent copy
    tablepacktable(hcontext, true, &hpacked, &fldummy)
    tableunpacktable(hpacked, true, &hcontext)
    (**hp).hcontext = hcontext
    (**hp).processkilledroutine = &threaddisposecontext
}

// 7. Return thread ID
return setlongvalue(getthreadid(hthread), v)
```

### Key Implementation Notes

1. **Process Model**: Each thread is a separate "process" (cooperative multitasking)
2. **Context Isolation**: Context table is **copied** (packed then unpacked) so threads don't share mutable state
3. **Thread Identification**: Thread name (`bsscriptname`) copied to `(**hp).bsname` for debugging
4. **Callback Hook**: `(**hp).processstartedroutine = &threadverbprocessstarted` prevents menu dimming
5. **Cleanup**: `(**hp).processkilledroutine = &threaddisposecontext` ensures context disposal

---

## Relationship to Other Verbs

### `thread.evaluate()` vs `thread.callScript()`

**`thread.evaluate(textString)`** (line 1187):
- Takes **unparsed text** as string
- Calls `processruntext(htext)` to parse and compile
- Simpler API: no parameters, no context
- Use case: Run arbitrary UserTalk code string

**`thread.callScript(scriptName, params, context)`** (line 1212):
- Takes **script name** to look up in ODB
- Supports parameter passing and context
- More powerful: full function call semantics
- Use case: Call specific handler with parameters

### `callScript()` (language verb)

**Location**: `langverbs.c`, function `callscriptverb()` (line 1364)

**Difference from `thread.callScript()`**:
- **Synchronous**: Runs in current thread, blocks until complete
- **No threading**: Just calls `langrunscript()` directly
- Same parameters: `scriptname, params, context`
- Use case: Call script synchronously, get return value immediately

**Code**:
```c
static boolean callscriptverb (hdltreenode hparam1, tyvaluerecord *vreturned) {
    bigstring bsscriptname;
    tyvaluerecord vparams;
    hdlhashtable hcontext = nil;

    getstringvalue(hparam1, 1, bsscriptname);
    getparamvalue(hparam1, 2, &vparams);

    if (langgetparamcount(hparam1) > 2)
        gettablevalue(hparam1, 3, &hcontext);

    return langrunscript(bsscriptname, &vparams, hcontext, vreturned);
}
```

---

## Integration Test Pattern (PR #318)

### Current Test Usage

```usertalk
thread.call("basicThread", {
    system.test.threadFlags.basicThreadRan = true
})

// Give thread time to execute (cooperative multitasking)
for i = 0; i < 10; i++ {
    sys.systemTask()  // Yield to allow thread execution
}

return system.test.threadFlags.basicThreadRan
```

### What This Tests

1. **Thread Creation**: `thread.call()` successfully creates new thread
2. **Code Execution**: Code block executes in separate thread
3. **ODB Access**: Thread can modify ODB (set `system.test.threadFlags.basicThreadRan`)
4. **Cooperative Scheduling**: `sys.systemTask()` yields to allow thread to run
5. **Return Value**: Thread ID returned (implicitly tested)

### Expected Stub Implementation (Headless)

For headless Frontier, `thread.call()` stub needs to:

1. **Accept Parameters**:
   - Thread name (string)
   - Code block (codevaluetype)

2. **Create Thread Context**:
   - Call `threadcallscriptverb()` (already exists in codebase)
   - Or provide stub that logs and returns dummy thread ID

3. **Return Thread ID**:
   - Return valid thread ID for introspection verbs
   - Or return 0 to indicate "not implemented"

---

## Implementation Recommendations

### For Headless Frontier

**Option 1: Full Implementation** (if threading infrastructure exists)
```c
// In Common/source/shellsysverbs.c, case callscriptfunc:
// Already implemented! Just ensure:
// 1. Process management works in headless mode
// 2. Thread scheduling works without GUI event loop
// 3. sys.systemTask() or equivalent yields properly
```

**Option 2: Stub Implementation** (if threading not ready)
```c
case callscriptfunc: {
    bigstring bsscriptname;
    tyvaluerecord vparams;

    getstringvalue(hparam1, 1, bsscriptname);
    getparamvalue(hparam1, 2, &vparams);

    // TODO: Threading not yet implemented in headless mode
    log_debug(LOG_COMP_THREAD, "thread.callScript stub: %s", bsscriptname);

    return setlongvalue(0, v);  // Return 0 = no thread created
}
```

**Option 3: Synchronous Fallback**
```c
case callscriptfunc: {
    // Run code synchronously instead of in separate thread
    // Useful for testing without full threading support
    return callscriptverb(hparam1, v);  // Call langverbs.c version
}
```

### UserTalk Wrapper

The `thread.call()` wrapper can be defined in UserTalk:

```usertalk
// system.verbs.builtins.thread.call
on call (threadName, codeBlock) {
    «Call the kernel verb thread.callScript
    «Note: codeBlock is already compiled codevaluetype at this point
    return thread.callScript(threadName, codeBlock)
}
```

Or even simpler - just **alias**:
```usertalk
system.verbs.builtins.thread.call = @thread.callScript
```

---

## Open Questions

### 1. Where is the UserTalk wrapper defined?

**Answer**: Likely in `Frontier.root` database at:
- `system.verbs.builtins.thread.call`
- Or: `builtins.thread.call`
- Or: Part of thread verb table initialization

**How to verify**: Open legacy `Frontier.root` in original Frontier app, navigate to thread verb table.

### 2. Does `thread.call()` support context parameter?

**Probable answer**: No. Looking at the test usage:
```usertalk
thread.call("name", { code })  // Only 2 parameters
```

Whereas `thread.callScript()` supports 3:
```usertalk
thread.callScript("name", code, context)  // 3 parameters
```

**Conclusion**: `thread.call()` is the **simplified API** - no context, just name + code.

### 3. How does the code block get compiled?

**Answer**: By the time `thread.callScript()` receives the parameter:
1. Parser has already identified `{ code }` as code block
2. Compiler has generated syntax tree
3. Parameter evaluation wraps tree in `tyvaluerecord` with `valuetype = codevaluetype`
4. The verb receives a **fully compiled code tree**, not source text

This is standard UserTalk parameter passing - no special handling needed.

---

## Summary for PR #318 Implementation

### What You Need to Implement

1. **Kernel Verb**: `thread.callScript()`
   - Already exists in `shellsysverbs.c`!
   - Just needs to work in headless mode (cooperative scheduling)

2. **UserTalk Wrapper**: `thread.call()`
   - Define in system ODB or as alias
   - Simple wrapper around `thread.callScript()`
   - Only passes 2 parameters (name, code), not context

3. **Process Infrastructure**
   - Thread scheduling (`scheduleprocess()`)
   - Cooperative multitasking (`sys.systemTask()`)
   - Thread registry (PR #317 foundation)

### What Already Works

- ✅ C implementation of `thread.callScript()` exists
- ✅ Code block compilation (standard UserTalk parser/compiler)
- ✅ Parameter passing (standard value records)
- ✅ Thread identification (thread ID return value)

### What Needs Adaptation

- **Event Loop**: GUI event loop → headless cooperative scheduler
- **Thread Yielding**: `sys.systemTask()` must work headless
- **Process Management**: Ensure `addnewprocess()` / `scheduleprocess()` work without GUI

---

## References

### Source Files (Legacy Frontier)

- **Thread Verbs**: `/Users/jake/dev/tedchoward/Frontier/Common/source/shellsysverbs.c`
  - Enum: `tythreadtoken` (line 176-218)
  - Implementation: `threadcallscriptverb()` (line 930-1088)
  - Dispatcher: `threadfunctionvalue()` (line 1115-1408)

- **Language Verbs**: `/Users/jake/dev/tedchoward/Frontier/Common/source/langverbs.c`
  - Synchronous version: `callscriptverb()` (line 1364-1397)

- **Thread Infrastructure**: `/Users/jake/dev/tedchoward/Frontier/Common/headers/threads.h`
  - Thread primitives (sleep, wake, yield)

### Current Work (Headless Frontier)

- **PR #318**: Deterministic thread testing foundation
- **PR #317**: Thread registry foundation (POSIX thread safety)
- **Issue #135**: Thread safety, outline context refactoring

---

## Conclusion

**`thread.call()` is not mysterious** - it's a straightforward UserTalk wrapper around the kernel verb `thread.callScript()`, which already has a full C implementation. The "code block syntax" is standard UserTalk compilation, not special magic.

**For headless implementation**, the main challenge is **not** implementing `thread.call()` itself, but ensuring the underlying **process management and cooperative scheduling** infrastructure works without a GUI event loop.

The integration tests in PR #318 will validate that:
1. Threads can be created
2. Code blocks execute asynchronously
3. Threads can access/modify ODB
4. Thread introspection verbs work (exists, count, getCurrentID, etc.)

**Next steps**: Verify process management works headless, then implement/stub the thread verbs based on available infrastructure.
