# Headless Thread Verb Implementation Plan

**Date**: 2026-01-18
**Purpose**: Implement headless thread verb stubs for PR #318 integration tests
**Status**: Planning
**Context**: Part of PR #318 (Deterministic Thread Testing Foundation)

---

## Executive Summary

PR #318 adds deterministic testing infrastructure for thread operations. The integration tests require working thread verb implementations in the headless build. Currently, all thread verb stubs in `tests/headless_thread_verbs.c` return "not implemented".

**Goal**: Implement the headless thread verb stubs to delegate to the existing cooperative threading model in `process.c`, enabling the 10 integration tests to pass.

**Non-Goal**: This is NOT implementing POSIX threading (that's the larger Phase 1-5 plan). This is implementing thin wrappers around the existing green thread system.

---

## Current Architecture

### Call Chain (UserTalk → C)

```
UserTalk Test Script
    ↓
thread.sleepFor(60)  // UserTalk wrapper function
    ↓
kernel(thread.sleepFor)  // Kernel binding directive
    ↓
headless_thread_verbs.c: thread_valueproc()  // Dispatcher
    ↓
case thrv_sleepfor:  // Switch case
    ↓
processsleep(getcurrentthread(), seconds * 60)  // Actual implementation in process.c
```

### Components

1. **UserTalk Wrapper Scripts**: `/Users/jake/dev/jsavin/Frontier/usertalk_scripts/Frontier.root/system/verbs/builtins/thread/*.ut`
   - Example: `sleepFor.ut` contains `on sleepFor(seconds) { kernel(thread.sleepFor) }`
   - Already exist and work correctly

2. **Kernel Verb Bindings**: `/Users/jake/dev/jsavin/Frontier/tests/headless_thread_verbs.c`
   - Token enum: `thrv_sleepfor`, `thrv_wake`, `thrv_kill`, etc.
   - Currently: All cases return `false` with `"not implemented"` error
   - **THIS IS WHAT WE NEED TO IMPLEMENT**

3. **Process Management**: `/Users/jake/dev/jsavin/Frontier/Common/source/process.c`
   - Functions: `processsleep()`, `wakeprocessthread()`, `killprocessthread()`, etc.
   - Already implemented and working (cooperative green threads)
   - We just need to call these from the stubs

---

## Implementation Strategy

### Pattern: Delegate to Existing Code

For most thread verbs, the implementation is straightforward delegation to `process.c` functions.

**Established Pattern** (from `shellsysverbs.c`):
```c
case sleepforfunc: {
    long n;
    flnextparamislast = true;

    if (!getlongvalue(hparam1, 1, &n))
        return false;

    boolean fl = processsleep(getcurrentthread(), n * 60);
    return setbooleanvalue(fl, vreturned);
}
```

**Headless Implementation** (in `headless_thread_verbs.c`):
```c
case thrv_sleepfor: {
    long seconds;
    flnextparamislast = true;

    if (!getlongvalue(hparam1, 1, &seconds))
        return false;

    boolean fl = processsleep(getcurrentthread(), seconds * 60);
    return setbooleanvalue(fl, vreturned);
}
```

---

## Verb-by-Verb Implementation

### Simple Delegation Verbs (Straightforward)

These verbs have direct 1:1 mappings to `process.c` functions:

#### 1. thread.sleepFor(seconds)
- **Extract**: `long seconds` from `hparam1`
- **Call**: `processsleep(getcurrentthread(), seconds * 60)`
- **Return**: `boolean` (true when awakened)
- **Pattern**: Extract param → call function → return boolean

#### 2. thread.sleepTicks(ticks)
- **Extract**: `long ticks` from `hparam1`
- **Call**: `processsleep(getcurrentthread(), ticks)`
- **Return**: `boolean`
- **Note**: Similar to sleepFor but no unit conversion

#### 3. thread.wake(threadID)
- **Extract**: `long threadID` from `hparam1`
- **Convert**: `hdlprocessthread hthread = getprocessthread(threadID)`
- **Call**: `wakeprocessthread(hthread)`
- **Return**: `boolean` (true if thread was sleeping)
- **Pattern**: Extract ID → lookup handle → call function → return boolean

#### 4. thread.kill(threadID)
- **Extract**: `long threadID` from `hparam1`
- **Convert**: `hdlprocessthread hthread = getprocessthread(threadID)`
- **Call**: `killprocessthread(hthread)`
- **Return**: `boolean` (always true)
- **Pattern**: Same as wake

#### 5. thread.isSleeping(threadID)
- **Extract**: `long threadID` from `hparam1`
- **Convert**: `hdlprocessthread hthread = getprocessthread(threadID)`
- **Call**: `processissleeping(hthread)`
- **Return**: `boolean`

#### 6. thread.getCurrentID()
- **No params**: `langcheckparamcount(hparam1, 0)`
- **Call**: `getthreadid(getcurrentthread())`
- **Return**: `long` thread ID
- **Pattern**: No params → call function → return long

#### 7. thread.getCount()
- **No params**: `langcheckparamcount(hparam1, 0)`
- **Call**: `processthreadcount()`
- **Return**: `long` count
- **Pattern**: Same as getCurrentID

#### 8. thread.getNthID(n)
- **Extract**: `short n` from `hparam1`
- **Call**: `getthreadid(nthprocessthread(n))`
- **Return**: `long` thread ID
- **Pattern**: Extract param → call function → return long

#### 9. thread.exists(threadID)
- **Extract**: `long threadID` from `hparam1`
- **Convert**: `hdlprocessthread hthread = getprocessthread(threadID)`
- **Call**: `goodthread(hthread)`
- **Return**: `boolean`

---

### Complex Verb: thread.callScript()

This is the critical verb that creates new threads and executes UserTalk code.

**Challenge**: `threadcallscriptverb()` in `shellsysverbs.c` is a static function (not exported).

**Options**:

#### Option A: Expose Existing Function (RECOMMENDED)
1. **Remove `static` keyword** from `threadcallscriptverb()` in `shellsysverbs.c`
2. **Add declaration** to a shared header (e.g., `Common/headers/shellthreads.h`)
3. **Call directly** from headless stub:
   ```c
   case thrv_callscript:
       return threadcallscriptverb(bsscriptname, vparams, hcontext, vreturned);
   ```

**Pros**:
- Reuses existing, proven code
- Maintains single source of truth
- No code duplication
- Thread-safe (uses existing synchronization)

**Cons**:
- Makes internal function public (but this is fine - it's a legitimate API)
- Couples headless to shellsysverbs.c (but we're already coupled via process.c)

#### Option B: Wrapper Function
1. **Create public wrapper** in `shellsysverbs.c`:
   ```c
   boolean thread_callscript_kernel(bigstring bsscriptname,
                                     tyvaluerecord vparams,
                                     hdlhashtable hcontext,
                                     tyvaluerecord *vreturned) {
       return threadcallscriptverb(bsscriptname, vparams, hcontext, vreturned);
   }
   ```
2. **Declare in header**: `Common/headers/shellthreads.h`
3. **Call from stub**: `return thread_callscript_kernel(...)`

**Pros**:
- Keeps `threadcallscriptverb()` private
- Clear API boundary (public wrapper vs internal impl)
- Easier to refactor later

**Cons**:
- Extra indirection (negligible performance impact)
- More code to maintain

#### Option C: Duplicate Implementation
**NOT RECOMMENDED** - Violates DRY, creates maintenance burden, risks divergence.

---

### Recommended Approach: Option B (Wrapper Function)

**Rationale**:
- **Maintainability**: Clear separation between internal implementation and public API
- **Thread Safety**: Reuses existing synchronization logic from `threadcallscriptverb()`
- **Future-Proof**: When POSIX threading is implemented, we can update the wrapper to use the new infrastructure without changing callsites
- **Clean API**: `thread_callscript_kernel()` is a clear, documented entry point

**Implementation Steps**:

1. **Create Header**: `Common/headers/shellthreads.h`
   ```c
   #ifndef SHELLTHREADS_H
   #define SHELLTHREADS_H

   #include "standard.h"
   #include "lang.h"

   /* Public API for thread.callScript kernel verb */
   boolean thread_callscript_kernel(
       bigstring bsscriptname,
       tyvaluerecord vparams,
       hdlhashtable hcontext,
       tyvaluerecord *vreturned
   );

   #endif /* SHELLTHREADS_H */
   ```

2. **Add Wrapper in shellsysverbs.c**:
   ```c
   boolean thread_callscript_kernel(bigstring bsscriptname,
                                     tyvaluerecord vparams,
                                     hdlhashtable hcontext,
                                     tyvaluerecord *vreturned) {
       return threadcallscriptverb(bsscriptname, vparams, hcontext, vreturned);
   }
   ```

3. **Call from headless stub**:
   ```c
   #include "shellthreads.h"

   case thrv_callscript: {
       bigstring bsscriptname;
       tyvaluerecord vparams;
       hdlhashtable hcontext = nil;

       // Extract parameters
       flnextparamislast = false;
       if (!getstringvalue(hparam1, 1, bsscriptname))
           return false;

       flnextparamislast = false;
       if (!getparamvalue(hparam1, 2, &vparams))
           return false;

       flnextparamislast = true;
       if (!getaddressparam(hparam1, 3, &hcontext))
           hcontext = nil;  // Optional parameter

       return thread_callscript_kernel(bsscriptname, vparams, hcontext, vreturned);
   }
   ```

---

## Parameter Extraction Patterns

### Common Patterns Used

1. **Single long parameter**:
   ```c
   long value;
   flnextparamislast = true;
   if (!getlongvalue(hparam1, 1, &value))
       return false;
   ```

2. **Thread ID parameter** (convert to handle):
   ```c
   long threadID;
   flnextparamislast = true;
   if (!getlongvalue(hparam1, 1, &threadID))
       return false;

   hdlprocessthread hthread = getprocessthread(threadID);
   if (!goodthread(hthread)) {
       langerrormessage(BIGSTRING("\pThe thread ID does not exist"));
       return false;
   }
   ```

3. **No parameters**:
   ```c
   if (!langcheckparamcount(hparam1, 0))
       return false;
   ```

4. **Multiple parameters**:
   ```c
   flnextparamislast = false;
   if (!getparam1(hparam1, 1, &param1))
       return false;

   flnextparamislast = false;
   if (!getparam2(hparam1, 2, &param2))
       return false;

   flnextparamislast = true;  // Last parameter
   if (!getparam3(hparam1, 3, &param3))
       return false;
   ```

5. **Optional parameter**:
   ```c
   flnextparamislast = true;
   if (!getoptionalparam(hparam1, 3, &optional))
       optional = default_value;  // Use default if not provided
   ```

---

## Integration Test Compatibility

### Current Tests (PR #318)

The tests use `thread.call()` syntax:
```usertalk
thread.call("basicThread", {
  system.test.threadFlags.basicThreadRan = true
})
```

**Problem**: This appears to call a verb named `thread.call`, but the kernel verb is `thread.callScript`.

**Investigation Needed**:
1. Check if `thread.call.ut` wrapper exists in UserTalk scripts
2. If not, check if tests should be using `thread.callScript()` instead
3. If `thread.call` is a new wrapper, we need to create it

**Likely Resolution**:
- The tests may have been written assuming a simplified API
- We may need to update tests to use `thread.callScript()` correctly:
  ```usertalk
  thread.callScript("basicThread", {
    system.test.threadFlags.basicThreadRan = true
  }, @system.temp)  // Use system.temp for context
  ```

**Alternative**: Create `thread.call.ut` wrapper:
```usertalk
on call(name, codeblock) {
    return thread.callScript(name, codeblock, nil)
}
```

---

## Testing Strategy

### Unit Tests (C-level)

Create `tests/headless_thread_verbs_tests.c`:
```c
// Test simple verb delegation
void test_sleep_for() {
    // Setup: Create thread
    // Execute: Call thread.sleepFor(1) via kernel
    // Verify: Thread goes to sleep
    // Cleanup: Wake thread
}

void test_wake_thread() {
    // Setup: Create sleeping thread
    // Execute: Call thread.wake(id) via kernel
    // Verify: Thread wakes up
    // Cleanup: Kill thread
}

void test_get_current_id() {
    // Setup: None
    // Execute: Call thread.getCurrentID() via kernel
    // Verify: Returns valid thread ID
}
```

### Integration Tests (UserTalk-level)

Already exist in `/Users/jake/dev/jsavin/Frontier-thread-testing-phase1/tests/integration/test_cases/thread_verbs_deterministic.yaml`

**Updates Needed**:
1. Change `system.test` to `system.temp` for temporary storage
2. Verify `thread.call()` vs `thread.callScript()` usage
3. Ensure tests match actual verb signatures

---

## Dependencies and Prerequisites

### Functions from process.c (Already Available)

```c
// Thread management
hdlprocessthread getcurrentthread(void);
hdlprocessthread getprocessthread(long id);
hdlprocessthread nthprocessthread(short n);
boolean goodthread(hdlprocessthread hthread);
long getthreadid(hdlprocessthread hthread);
short processthreadcount(void);

// Thread operations
boolean processsleep(hdlprocessthread hthread, unsigned long timeout);
boolean processissleeping(hdlprocessthread hthread);
boolean wakeprocessthread(hdlprocessthread hthread);
boolean killprocessthread(hdlprocessthread hthread);

// Time slice management
boolean getprocesstimeslice(long *ticks);
boolean setprocesstimeslice(long ticks);
boolean getdefaulttimeslice(long *ticks);
boolean setdefaulttimeslice(long ticks);
```

### Helper Functions from lang.c (Already Available)

```c
// Parameter extraction
boolean getlongvalue(hdltreenode hp, short n, long *val);
boolean getstringvalue(hdltreenode hp, short n, bigstring bs);
boolean getparamvalue(hdltreenode hp, short n, tyvaluerecord *val);
boolean getaddressparam(hdltreenode hp, short n, hdlhashtable *htable);
boolean langcheckparamcount(hdltreenode hp, short expected);

// Return value setting
boolean setbooleanvalue(boolean fl, tyvaluerecord *val);
boolean setlongvalue(long n, tyvaluerecord *val);

// Error handling
void langerrormessage(bigstring bserror);
```

### New Dependencies Needed

1. **Header File**: `Common/headers/shellthreads.h` (to declare public wrapper)
2. **Include**: `#include "shellthreads.h"` in `headless_thread_verbs.c`
3. **Link**: Ensure `shellsysverbs.c` is linked in headless build (likely already is)

---

## Implementation Checklist

### Phase 1: Simple Verbs (Day 1)
- [ ] Implement `thread.sleepFor()`
- [ ] Implement `thread.sleepTicks()`
- [ ] Implement `thread.getCurrentID()`
- [ ] Implement `thread.getCount()`
- [ ] Write unit tests for these 4 verbs
- [ ] Verify tests pass

### Phase 2: Thread Control Verbs (Day 1-2)
- [ ] Implement `thread.wake()`
- [ ] Implement `thread.kill()`
- [ ] Implement `thread.isSleeping()`
- [ ] Implement `thread.exists()`
- [ ] Implement `thread.getNthID()`
- [ ] Write unit tests for these 5 verbs
- [ ] Verify tests pass

### Phase 3: Thread Creation (Day 2-3)
- [ ] Create `Common/headers/shellthreads.h`
- [ ] Add `thread_callscript_kernel()` wrapper in `shellsysverbs.c`
- [ ] Implement `thread.callScript()` stub
- [ ] Investigate `thread.call()` vs `thread.callScript()` in tests
- [ ] Update integration tests if needed (system.test → system.temp)
- [ ] Verify all 10 integration tests pass

### Phase 4: Final Polish (Day 3)
- [ ] Review error handling (all verbs return appropriate errors)
- [ ] Add logging for debugging (optional, via log_trace)
- [ ] Update PR #318 description with implementation details
- [ ] Run full test suite (`./tools/run_headless_tests.sh`)
- [ ] Run integration tests (`cd tests && make test-integration`)
- [ ] Code review with code-review-bar-raiser agent
- [ ] Address feedback and push final version

---

## Timeline Estimate

**Total Effort**: 2-3 days

| Phase | Duration | Tasks |
|-------|----------|-------|
| Phase 1: Simple Verbs | 4-6 hours | 4 verbs + tests |
| Phase 2: Control Verbs | 4-6 hours | 5 verbs + tests |
| Phase 3: Thread Creation | 6-8 hours | callScript wrapper + integration tests |
| Phase 4: Polish | 2-4 hours | Testing, review, fixes |

---

## Success Criteria

### Code Quality
- [ ] All headless thread verb stubs implemented
- [ ] No code duplication (reuses existing process.c functions)
- [ ] Thread-safe (delegates to existing thread-safe code)
- [ ] Error handling matches shellsysverbs.c patterns
- [ ] Logging compliant with LOGGING_STANDARDS.md

### Testing
- [ ] All 10 integration tests in PR #318 pass
- [ ] Unit tests pass (`./tools/run_headless_tests.sh`)
- [ ] Integration tests pass (`cd tests && make test-integration`)
- [ ] No regressions in existing tests

### Documentation
- [ ] Code comments explain parameter extraction
- [ ] Function documentation for public wrapper
- [ ] Integration test syntax matches verb signatures
- [ ] PR #318 description updated with implementation summary

---

## Risk Mitigation

### Risk 1: thread.callScript() Complexity
**Mitigation**: Use wrapper function approach to delegate to proven code

### Risk 2: Integration Test Incompatibility
**Mitigation**: Update tests to match actual verb signatures if needed

### Risk 3: Thread Safety Issues
**Mitigation**: Reuse existing thread management code (already thread-safe)

### Risk 4: Parameter Extraction Errors
**Mitigation**: Follow established patterns from shellsysverbs.c exactly

---

## Open Questions

1. **thread.call() vs thread.callScript()**:
   - Do we need to create a `thread.call.ut` wrapper?
   - Or update tests to use `thread.callScript()` directly?
   - **Action**: Investigate UserTalk scripts to determine correct approach

2. **system.test vs system.temp**:
   - User recommended using `system.temp` instead of `system.test`
   - **Action**: Update all integration tests to use `system.temp`

3. **Build System Integration**:
   - Issue #2 from code review: Add threadtestharness.c/h to build system
   - **Action**: Determine if this is Makefile or something else (Xcode removed)

---

## References

**Frontier Source Files**:
- `/Users/jake/dev/jsavin/Frontier/Common/source/shellsysverbs.c` - Reference implementation
- `/Users/jake/dev/jsavin/Frontier/Common/source/process.c` - Thread management functions
- `/Users/jake/dev/jsavin/Frontier/tests/headless_thread_verbs.c` - Stubs to implement
- `/Users/jake/dev/tedchoward/Frontier/` - Legacy codebase for reference

**Planning Documents**:
- `/Users/jake/dev/jsavin/Frontier/planning/phase4/threading/README.md` - Threading roadmap
- `/Users/jake/dev/jsavin/Frontier/planning/phase4/threading/THREAD_VERBS_ANALYSIS.md` - Verb API reference
- `/Users/jake/dev/jsavin/Frontier/planning/architectural_decision_records/ADR-010-DETERMINISTIC_THREAD_TESTING.md` - Test harness design

**Code Review**:
- PR #318 code review identified 6 issues
- Issue #4 (tests fail) is what this implementation addresses

---

**Document Version**: 1.0
**Last Updated**: 2026-01-18
**Status**: Implementation Plan Ready for Review
