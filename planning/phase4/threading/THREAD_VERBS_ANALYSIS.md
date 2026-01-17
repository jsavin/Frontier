# Thread Verbs Analysis

**Date**: 2026-01-16
**Purpose**: Complete analysis of thread.* verb API surface for POSIX threading implementation

---

## Executive Summary

Frontier's threading model is a **cooperative, green thread** system managed entirely in userspace by the `process.c` scheduler. Thread IDs are **not OS thread IDs** but rather pointers to `tythreadglobals` structures cast to `long`.

**Critical Insight**: The current implementation uses `hdlprocessthread` (which is `hdlthreadglobals`, a handle) as the thread identifier. The "thread ID" exposed to UserTalk is the address of this handle structure.

**Threading Model**: Cooperative multitasking with time-slice-based scheduling. Threads yield control voluntarily or when their time slice expires.

---

## Complete Thread Verb Inventory

### Core Thread Management

#### thread.sleep(id) → boolean
- **Kernel**: `sleepfunc` in `shellsysverbs.c:1516`
- **Implementation**: `processsleep(hthread, -1)` (sleeps indefinitely)
- **Behavior**: Puts thread to sleep until awakened by `thread.wake()` or killed
- **Returns**: `true` when awakened

#### thread.wake(id) → boolean
- **Kernel**: `wakefunc` in `shellsysverbs.c:1574`
- **Implementation**: `wakeprocessthread(hthread)`
- **Returns**: `true` if thread was sleeping, `false` if already awake

#### thread.kill(id) → boolean
- **Kernel**: `killfunc` in `shellsysverbs.c:1585`
- **Implementation**: `killprocessthread(hthread)`
- **Behavior**: Marks thread for termination on next swap-in. Auto-wakes sleeping threads.
- **Returns**: `true` (always)
- **Note**: Killing the agent thread auto-recreates it after 1 second

#### thread.sleepFor(minutes) → boolean
- **Kernel**: `sleepforfunc` in `shellsysverbs.c:1527`
- **Implementation**: `processsleep(getcurrentthread(), n * 60)` (60-tick seconds)
- **Behavior**: Sleeps current thread for specified minutes (converted to ticks: `minutes * 60 * 60`)
- **Returns**: `true` when awakened

#### thread.sleepTicks(ticks) → boolean
- **Kernel**: `sleepticksfunc` in `shellsysverbs.c:1545`
- **Implementation**: `processsleep(getcurrentthread(), n)` (ticks directly)
- **Behavior**: Sleeps current thread for specified ticks (1 tick = 1/60th second)
- **Returns**: `true` when awakened

#### thread.isSleeping(id) → boolean
- **Kernel**: `issleepingfunc` in `shellsysverbs.c:1563`
- **Implementation**: `processissleeping(hthread)`
- **Returns**: `true` if thread is sleeping

---

### Thread Introspection

#### thread.getCurrentID() → long
- **Kernel**: `getcurrentfunc` in `shellsysverbs.c`
- **Implementation**: Returns `(long)getcurrentthread()` (address of `hdlthreadglobals`)
- **Returns**: Numeric ID of current thread

#### thread.getCount() → long
- **Kernel**: `getcountfunc` in `shellsysverbs.c`
- **Implementation**: Returns `ctprocessthreads` (global counter)
- **Returns**: Number of threads in thread list
- **Note**: Counts sleeping threads; agent thread always exists unless disabled

#### thread.getNthID(n) → long
- **Kernel**: `getnththreadfunc` in `shellsysverbs.c`
- **Implementation**: `nthprocessthread(n)` - iterates thread list
- **Parameters**: `n` is 1-based index
- **Returns**: Thread ID of nth thread
- **Note**: Agent thread is always index 1

#### thread.exists(id) → boolean
- **Kernel**: `existsfunc` in `shellsysverbs.c`
- **Implementation**: `goodthread(getprocessthread(id))`
- **Returns**: `true` if thread ID exists in thread list

---

### Thread Execution

#### thread.evaluate(script_string) → long
- **Kernel**: `evaluatefunc` in `shellsysverbs.c`
- **Glue Script**: `thread/evaluate.ut`
- **Behavior**: Compiles and runs UserTalk script in new thread
- **Returns**: Thread ID of newly created thread
- **Note**: Cannot retrieve result directly; use `thread.evaluateTo()` instead

#### thread.evaluateTo(script_string, adr_result) → long
- **Kernel**: `evaluatetofunc` in `shellsysverbs.c`
- **Glue Script**: `thread/evaluateTo.ut`
- **Behavior**: Compiles script, runs in new thread, stores result at `adr_result`
- **Returns**: Thread ID of newly created thread

#### thread.callScript(adr_script, param_list, adr_context=nil) → boolean
- **Kernel**: `callscriptfunc` in `shellsysverbs.c`
- **Glue Script**: `thread/callScript.ut`
- **Parameters**:
  - `adr_script`: String address of script to call
  - `param_list`: List of parameters to pass
  - `adr_context`: Optional context table (copied for new thread)
- **Behavior**: Calls script with parameters in new thread
- **Returns**: `true` (always)
- **Note**: More robust replacement for `thread.easyCall()`

---

### Time Slice Management

#### thread.setTimeSlice(ticks) → boolean
- **Kernel**: `settimeslicefunc` in `shellsysverbs.c:1604`
- **Glue Script**: `thread/setTimeSlice.ut`
- **Implementation**: `setprocesstimeslice(ticks)`
- **Behavior**: Sets time slice for current thread
- **Returns**: `true`

#### thread.setDefaultTimeSlice(ticks) → boolean
- **Kernel**: `setdefaulttimeslicefunc` in `shellsysverbs.c`
- **Glue Script**: `thread/setDefaultTimeSlice.ut`
- **Implementation**: `setdefaulttimeslice(ticks)`
- **Behavior**: Sets default time slice for newly created threads
- **Returns**: `true`

---

### Legacy/Undocumented Verbs

#### thread.getStats() → table
- **Kernel**: `statsfunc` in `shellsysverbs.c`
- **Glue Script**: `thread/getStats.ut`
- **Implementation**: `processgetstats(htable)` - fills table with thread statistics
- **Returns**: Table with thread performance data

#### thread.wrapper(...)
- **Glue Script**: `thread/wrapper.ut`
- **Purpose**: Internal wrapper function (implementation unclear without reading code)

#### thread.easyCall(...)
- **Glue Script**: `thread/easyCall.ut`
- **Status**: Deprecated, replaced by `thread.callScript()`

#### thread.getGlobalAddress() → address
- **Glue Script**: `thread/getGlobalAddress.ut`
- **Purpose**: Returns address of thread-specific global (unclear specifics)

#### thread.getStackDump() → ?
- **Glue Script**: `thread/getStackDump.ut`
- **Purpose**: Diagnostic function for thread state inspection

---

## Verb Categorization by Implementation Strategy

### Pure Kernel Verbs (No UserTalk Dependencies)
- `thread.sleep(id)`
- `thread.wake(id)`
- `thread.kill(id)`
- `thread.sleepFor(minutes)`
- `thread.sleepTicks(ticks)`
- `thread.isSleeping(id)`
- `thread.getCurrentID()`
- `thread.getCount()`
- `thread.getNthID(n)`
- `thread.exists(id)`
- `thread.setTimeSlice(ticks)`
- `thread.setDefaultTimeSlice(ticks)`

### Complex Execution Verbs (Require UserTalk Runtime Integration)
- `thread.evaluate(script)`
- `thread.evaluateTo(script, adr)`
- `thread.callScript(adr, params, context)`
- `thread.getStats()`

---

## Thread ID Semantics

**CRITICAL**: Thread IDs are **NOT** pthread thread IDs. They are:
1. Internally: `hdlthreadglobals` (handle to thread-local globals structure)
2. Externally (UserTalk): `(long)hthread` - address of the handle
3. Stored in: `tythreadglobals.idthread` (type `hdlthread`, which is opaque)

**Lookup Mechanism** (`getprocessthread()`):
```c
hdlprocessthread getprocessthread (long id) {
    register hdlthreadglobals hg;
    for (hg = (**processthreadlist).hfirst; hg != nil; hg = (**hg).hnextglobals) {
        if ((long) (**hg).idthread == id)
            return (hg);
    }
    return (nil);
}
```

**Thread List Structure**:
- Singly-linked list via `tythreadglobals.hnextglobals`
- Head stored in global `processthreadlist->hfirst`
- Traversed linearly for lookup and enumeration

---

## Agent Thread Behavior

**Agent Thread Properties**:
- Always created as first thread when agents are enabled
- Thread ID returned by `thread.getNthID(1)` is always agent thread
- Runs `agentthreadmain()` → `agentscheduler()` in loop
- Sleeps between scheduler runs (`agentthreadsleeping` flag)
- Auto-recreated 1 second after kill (unless `Frontier.enableAgents(false)`)

**Agent Scheduler**:
- Executes all non-oneshot processes in `processlist`
- Time slices agents cooperatively
- Processes wake on timeout or explicit `thread.wake()`

---

## Implementation Dependencies

### C Functions Called by Thread Verbs

**Process Management** (`process.c`):
- `processsleep(hthread, ticks)`
- `processissleeping(hthread)`
- `wakeprocessthread(hthread)`
- `killprocessthread(hthread)`
- `getcurrentthread()`
- `nthprocessthread(n)`
- `processthreadcount()`
- `goodthread(hthread)`
- `setprocesstimeslice(ticks)`
- `getprocesstimeslice(&ticks)`
- `setdefaulttimeslice(ticks)`
- `getdefaulttimeslice(&ticks)`

**Thread Primitives** (`threads.h` / platform-specific):
- `newthread(callback, params, globals, &idthread)`
- `threadsleep(hthread)`
- `threadwake(hthread, boolean)`
- `threadissleeping(hthread)`

---

## Backward Compatibility Requirements

**MUST MAINTAIN**:
1. Thread ID type: `long` (UserTalk integer)
2. Thread ID semantics: Opaque numeric identifier
3. Agent thread auto-creation behavior
4. Cooperative time-slice scheduling
5. Thread list iteration order (agent thread first)
6. Sleep/wake semantics (indefinite sleep until wake)

**CAN CHANGE** (internal implementation):
1. Thread ID generation mechanism
2. Thread list data structure (linked list → hashtable, etc.)
3. Underlying OS thread implementation (green threads → POSIX threads)
4. Time slice timing precision

---

## Key Insights for POSIX Migration

### Current Model: Cooperative Green Threads
- Single-threaded event loop with manual context switching
- Threads yield via `processyield()` or time slice expiry
- No true parallelism - only concurrency
- All threads share same process stack space

### POSIX Model Requirements
- **One pthread per Frontier thread** (for true parallelism)
- **Thread-local `tythreadglobals`** (already exists via `pthread_setspecific()`)
- **Mutex-protected shared state** (process list, agent scheduler)
- **Condition variables for sleep/wake** (replace time-based polling)
- **Thread-safe ODB access** (database operations, hash tables)

### Migration Challenges
1. **Thread ID Mapping**: Map opaque `long` IDs to `pthread_t` handles
2. **Sleep/Wake Implementation**: Use `pthread_cond_wait()` / `pthread_cond_signal()`
3. **Agent Scheduler**: Convert cooperative loop to preemptive execution
4. **Time Slice Enforcement**: Use `pthread_setschedparam()` or voluntary yield
5. **Thread-Local State**: Already thread-local via `tythreadglobals` - preserve this

---

## Test Coverage Requirements

**For Each Verb**:
1. Basic functionality test
2. Error cases (invalid thread ID, etc.)
3. Edge cases (sleep 0 ticks, wake already-awake thread)
4. Concurrent operation test (multiple threads interacting)

**Integration Tests**:
1. Agent thread lifecycle (enable/disable, kill/recreate)
2. Thread enumeration consistency
3. Sleep/wake race conditions
4. Time slice fairness

---

## Open Questions

1. **What is `thread.wrapper()` used for?** (No documentation found)
2. **What does `thread.getGlobalAddress()` return?** (Requires code inspection)
3. **Are there undocumented kernel verbs?** (Check for additional `case` statements)
4. **What happens to sleeping threads on shutdown?** (Cleanup behavior)
5. **Can threads be nested?** (Thread creates thread creates thread - depth limit?)

---

## Next Steps

See `IMPLEMENTATION_PLAN.md` for phased execution strategy.
