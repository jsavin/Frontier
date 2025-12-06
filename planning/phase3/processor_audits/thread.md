# Processor Audit: `thread`

**Status:** ⚠️ **GUI-Dependent** (17 Kernel Verbs + 5 Scripts)
**Audit Date:** 2025-12-06
**Auditor:** Claude (Haiku 4.5)

---

## Basic Information

| Property | Value |
|----------|-------|
| **Processor Name** | `thread` |
| **EFP ID** | 1018 |
| **Verb Count** | 17 (kernel verbs) + 5 (script utilities) |
| **Window Required** | YES ⚠️ |
| **Documentation** | [thread/](../../../docs/usertalk/docserver.userland.com/thread/index.html) |
| **Script Implementation** | `system.verbs.builtins.thread` (Frontier.root) |

---

## Category Assessment

**Category:** ⚠️ **Threading/Concurrency** (GUI-Dependent)

**Rationale:**
Thread processor provides multithreading and asynchronous script execution capabilities. However, the kernelverbs.rc file indicates `Window required = true`, suggesting GUI context dependencies. Thread processor is primarily used for launching scripts in separate threads for concurrent execution, event handling, and GUI responsiveness.

**Headless Compatibility:** ⚠️ **Partial** (Marked as window-required, but core functionality may work without GUI)

**GUI Blocking Verbs:**
- Likely most verbs require event loop or window context for proper thread scheduling
- Not recommended for pure headless operation

---

## Verb Inventory

### Kernel Verbs (17 from kernelverbs.rc)

| # | Verb Name | Signature | Description |
|---|-----------|-----------|-------------|
| 1 | `exists` | `thread.exists(id) -> boolean` | Check if thread with ID exists |
| 2 | `evaluate` | `thread.evaluate(scriptString) -> result` | Execute script in separate thread |
| 3 | `callscript` | `thread.callScript(adrScript, paramList, adrParamTable) -> boolean` | **KERNEL** - Call script in thread with parameters |
| 4 | `getcurrentid` | `thread.getCurrentID() -> id` | Get current thread ID |
| 5 | `getcount` | `thread.getCount() -> integer` | Get total active thread count |
| 6 | `getnthid` | `thread.getNthID(n) -> id` | Get Nth thread ID |
| 7 | `sleep` | `thread.sleep(id)` | Put thread to sleep (pause execution) |
| 8 | `sleepfor` | `thread.sleepFor(id, ticks)` | Sleep for specified tick duration |
| 9 | `sleepticks` | `thread.sleepTicks(ticks)` | Sleep current thread for ticks |
| 10 | `issleeping` | `thread.isSleeping(id) -> boolean` | Check if thread is sleeping |
| 11 | `wake` | `thread.wake(id)` | Resume sleeping thread |
| 12 | `kill` | `thread.kill(id)` | Terminate thread |
| 13 | `gettimeslice` | `thread.getTimeSlice(id) -> ticks` | Get thread's time slice quantum |
| 14 | `settimeslice` | `thread.setTimeSlice(id, ticks)` | Set thread's time slice |
| 15 | `getdefaulttimeslice` | `thread.getDefaultTimeSlice() -> ticks` | Get default time slice for new threads |
| 16 | `setdefaulttimeslice` | `thread.setDefaultTimeSlice(ticks)` | Set default time slice |
| 17 | `getstats` | `thread.getStats(adrStatsTable) -> boolean` | Get detailed thread statistics |

### Script Utilities (5 UserTalk implementations)

| # | Script Name | Signature | Description |
|---|-----------|-----------|-------------|
| 1 | `thread.easyCall` | `on easyCall(routineName, paramList)` | Convenience wrapper for launching thread with parameters |
| 2 | `thread.evaluateTo` | `on evaluateTo(scriptString, adrResult)` | Execute script in thread, store result at address |
| 3 | `thread.getStackDump` | `on getStackDump(adrTable)` | Get thread stack trace in table format |
| 4 | `thread.wrapper` | `on wrapper(s, adr)` | Helper for thread.evaluateTo (executes script, stores result) |
| 5 | `thread.wake` | Script version | Possibly extends kernel verb |

---

## Implementation Analysis

### Complexity: **MEDIUM-HIGH** (GUI-Dependent)

### Dependencies

- **Other Processors:**
  - system.verbs.builtins (for callback/event handling)
  - UI event loop (for thread scheduling)
- **External Services:** None
- **OS-Specific Functionality:** YES (threading, synchronization)
  - Requires OS thread primitives (Windows/Mac)
  - Event loop integration (GUI event dispatch)
- **GUI/Window Context:** YES ⚠️ - Window required for proper thread management

### Key Implementation Notes

**Architecture:**

```
User Script
    ↓
thread.evaluate() or thread.callScript()
    ↓
Kernel creates new thread (OS-level)
    ↓
Thread runs in parallel with main event loop
    ↓
Results available via getStats, getStackDump
    ↓
Main loop checks for sleeping/completed threads
```

**Thread Lifecycle:**
1. **Create**: `thread.evaluate()` or `thread.callScript()` creates thread
2. **Execute**: Script runs in separate OS thread
3. **Sleep**: `thread.sleep()` pauses execution (voluntary)
4. **Wake**: `thread.wake()` resumes paused thread
5. **Kill**: `thread.kill()` terminates thread
6. **Stats**: `thread.getStats()` provides execution information

**Core Verbs:**

- **evaluate**: Takes script string, executes in separate thread, returns immediately (async)
- **callScript**: Takes script address + parameters, calls with params in thread context
- **getStats**: Returns table with thread info including:
  - Thread ID
  - Status (running, sleeping, etc.)
  - Stack trace (array of call frames)
  - Time slice quantum
  - Statistics (CPU time, etc.)

**Script Utilities:**

- **easyCall**: Wraps callScript for convenience
  - Takes routine name + parameter list
  - Builds script string: `"routineName(...)"`
  - Calls `thread.evaluate()`

- **evaluateTo**: Evaluates script, stores result
  - Uses `thread.wrapper` to execute and store result
  - Pattern: `thread.evaluate("thread.wrapper(\"...\", @adr)")`

- **getStackDump**: Extracts stack trace from getStats
  - Formats call stack as table with level numbers
  - Each level includes function name, line number, file info

---

## Implementation Status

**Current State:**
- ✅ 17 kernel verbs defined in kernelverbs.rc
- ✅ 5 UserTalk script utilities implemented and documented
- ✅ Scripts are headless-compatible (pure logic, no GUI calls)
- ⏳ Kernel verb implementations (depends on OS threading)
- ⚠️ GUI event loop integration needed

**What Needs Implementation:**
1. **OS-Level Threading:**
   - Create/manage OS threads (Windows threading API)
   - Thread ID management and tracking
   - Thread state transitions (running → sleeping → dead)
   - Context switching and time slice management

2. **Kernel Verb Implementations:**
   - `thread.evaluate(scriptString)` - Parse and execute in thread
   - `thread.callScript(adr, params, resultAdr)` - Call script with parameters
   - `thread.sleep()`, `thread.wake()` - Sleep/resume threads
   - `thread.getStats()` - Return thread statistics table
   - Other status/management verbs

3. **Event Loop Integration:**
   - Thread scheduler integration with main event loop
   - Signal/callback mechanism for thread completion
   - Stack trace capture mechanism

4. **Synchronization:**
   - Thread safety for kernel data structures
   - May need mutex/semaphore for thread-safe operations

**Implementation Pattern:**
```c
// Thread ID management
typedef struct {
    HANDLE handle;           // OS thread handle
    DWORD threadId;          // OS thread ID
    boolean isSleeping;      // Sleep state
    long timeSlice;          // Time quantum in ticks
    char* scriptString;      // Script to execute
    table* parameters;       // Parameter table
    table* resultTable;      // Results table
    unsigned long stackTrace[MAX_STACK_DEPTH];
} ThreadInfo;

// Kernel verb implementation
boolean threadexecute(hdltreenode scriptNode) {
    bigstring scriptString;
    // Get script string from node
    // Create OS thread
    // Add to thread table
    // Return immediately (async)
    return true;
}
```

---

## Testing Requirements

**Basic Threading:**
```usertalk
thread.evaluate("beep()")          // Execute in thread
// Main script continues, beep happens async
```

**Thread Tracking:**
```usertalk
local (id = thread.evaluate("sleep(5); dialog.ask('Done!')"))
thread.exists(id)                  // true
thread.isSleeping(id)              // true while sleeping
thread.wait(id)                    // Wait for completion (if exists)
```

**Parameter Passing:**
```usertalk
thread.callScript(@myScript, {"arg1", "arg2"}, @results)
// Results populated after execution
```

**Thread Management:**
```usertalk
thread.getCount()                  // Active thread count
thread.getCurrentID()              // Current thread ID
thread.getStats(@stats)            // Detailed statistics
thread.kill(id)                    // Terminate thread
```

**Stack Trace:**
```usertalk
thread.getStackDump(@dump)         // Get formatted stack
dump.level01                       // First stack frame
```

**Error Handling:**
```usertalk
thread.evaluate("1/0")             // Division by zero in thread
// Thread terminates with error
thread.getStats(@stats)            // Shows error info
```

**Edge Cases:**
- Create many threads (500+)
- Kill thread while executing
- Sleep/wake cycles
- Evaluate invalid script
- Nested thread.evaluate calls
- Memory cleanup on thread termination

---

## Implementation Effort

**Estimated Time:** 16-20 hours

**Breakdown:**
- OS thread abstraction layer: 4-5 hours
- Kernel verb implementations: 4-5 hours
- Event loop integration: 3-4 hours
- Stack trace capture mechanism: 2-3 hours
- Testing & debugging: 3-5 hours

**Confidence:** MEDIUM (threading is complex, GUI integration adds complexity)

**Blockers:**
- GUI event loop must be available
- Window/event context required for proper operation
- OS-specific threading APIs (Windows/Mac)

---

## Priority & Sequencing

**Priority:** 🟡 **MEDIUM** (Tier 2 - Advanced Use Case)

**Recommended Implementation Order:** Late (after core I/O processors)

**Prerequisites:**
- Event loop implementation
- Window context availability
- OS-specific threading APIs

**Notes:**
- Recommended NOT to implement for pure headless operation
- Threading in headless context is questionable (no event loop)
- May cause race conditions in headless mode
- Consider disabling for headless builds

---

## Headless Compatibility Analysis

**Fully Compatible:** ❌ NO (0/17 verbs)

**Incompatibilities:**
- Window required (kernelverbs.rc says true)
- Event loop dependency for thread scheduling
- Thread scheduling tied to UI event dispatch
- Time slicing coordination with GUI updates
- Stack trace capture may depend on GUI context

**Recommendation:** ⚠️ **Skip for Headless**
- Threading without event loop is unreliable
- Race conditions likely in headless context
- Consider disabling thread processor for headless builds
- Alternative: Simple inline execution only (no true threading)

---

## Related Processors

- **script** - Script execution (called by thread.evaluate)
- **dialog** - User interaction (blocked threads may show dialogs)
- **file** - File I/O from threads
- **db** - Database access from threads (may have locking issues)

---

## Special Considerations

**Thread Safety:**
- Database access from multiple threads (race conditions possible)
- Global state modifications (thread-safe access needed)
- Frontier's internal data structures (not thread-safe)

**Performance:**
- Context switching overhead
- Time slice tuning (default 60 ticks per thread)
- Too many threads = thrashing

**GUI Blocking:**
- Long-running scripts in threads block event loop
- Beware of blocking the main thread

**Error Handling:**
- Exceptions in threads don't propagate to main thread
- getStats shows error information
- No error callback mechanism visible

**Deadlock Risks:**
- Threads accessing shared data without locks
- Scripts calling scripts across threads
- Potential circular wait conditions

---

## Implementation Status Summary

**Status:** ⚠️ **NOT RECOMMENDED FOR HEADLESS**

- Window requirement makes it unsuitable for headless operation
- GUI event loop integration is fundamental
- Threading without proper event handling is unreliable
- Consider skipping for headless Frontier builds

**Alternative for Headless:**
- Single-threaded async execution (via scheduler)
- Deferred script execution (via inetd for network)
- No true concurrency (not possible without event loop)

---

## References

**Documentation:**
- DocServer: `docs/usertalk/docserver.userland.com/thread/`

**Implementation:**
- Kernel verbs: `Common/resources/Win32/kernelverbs.rc` (lines 866-889)
- Scripts: `system.verbs.builtins.thread` (Frontier.root)

**Threading Standards:**
- Windows: CreateThread, WaitForSingleObject (kernel32.dll)
- Mac: pthread, dispatch_async
- POSIX: pthread_create, pthread_join

---

## Audit Notes

- **Key Finding**: kernelverbs.rc marks window as `true` - GUI context required
- **Script Count Mismatch**: 17 kernel verbs but 22 .ut files (5 utilities/helpers)
- **Architecture**: Full multithreading with thread pool management
- **Recommendation**: Low priority for headless implementation

---
