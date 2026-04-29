# ADR-009: REPL Hash Table Stack Management + QuickScript Architecture

**Status**: `[IMPLEMENTED]` - QuickScript model deployed with thread-local infrastructure (PR #304)
**Date**: 2026-01-14
**Author**: System Architect
**Relates to**: Issue #135 (Collaborative ODB), ADR-005 (Thread-Safety), ADR-006 (Outline Context)

---

> **Reader's note (2026-04-28)**: The `/clear` and `/vars` REPL commands referenced
> throughout the body of this ADR were **removed by design** when the QuickScript
> model shipped in PR #304 (see "Implementation Update: QuickScript Model Decision"
> near the bottom of this document). The 21 obsolete tests covering them were
> deleted in PR #569 (2026-04-28).
>
> All discussion below of "3 failing tests", "97.8% pass rate", workspace
> persistence workarounds, and the Phase 3B documentation plan describes the
> *abandoned* approach — preserved as historical context for *why* the QuickScript
> model was chosen over the workaround direction. The workaround code that
> formerly lived in `repl_eval.c` (around lines 142–267 of the pre-#304 version)
> no longer exists; the file is now 128 lines and contains no
> workspace-forcing or manual hash-table-clearing code.
>
> The thread-local `hashtablestack` infrastructure (Phase 3A) IS still in place
> and IS still load-bearing — it remains the foundation for the Phase 6+ explicit
> hash-table context architecture (Option 5). The bootstrap-initialization
> constraint that blocks the macro migration also still applies.

---

## Executive Summary

The REPL Interactive Mode implementation encounters a critical architectural limitation where `langrunhandletraperror()` calls `pushprocess(nil)` / `popprocess()`, which saves and restores the entire `hashtablestack` state. This causes REPL workspace modifications to be lost between evaluations, breaking the fundamental REPL contract that variables persist across sessions.

**Current Status**: Working REPL with 133/136 tests passing (97.8% pass rate).

**Failing Functionality**:
- `/clear` command - Variables assigned after `/clear` not stored in workspace
- Function definitions - Functions defined in REPL not callable in subsequent evaluations
- Multiple function calls - Related to function definition persistence issue

**This is a SYMPTOM of the broader global hash table stack management problem documented in CLAUDE.md ("BURN THE GLOBALS WITH FIRE").**

**Temporary Workarounds Implemented**:
1. **Force workspace onto stack before evaluation** (lines 259-267 in `repl_eval.c`)
2. **Manual hash table clearing without callbacks** (lines 142-170 in `repl_eval.c`)

These workarounds enable 97.8% of REPL functionality but cannot solve the fundamental problem: global mutable `hashtablestack` state gets clobbered by `pushprocess()`/`popprocess()` deep in the call chain.

---

## Context and Problem Statement

### The Problem

When REPL evaluates UserTalk code:

1. **REPL Initialization** (`repl_workspace_init()`):
   - Creates persistent `root.workspace` table for REPL session
   - Pushes workspace onto `hashtablestack` via `pushhashtable(ws->workspace_table)`
   - Sets `currenthashtable = ws->workspace_table`
   - Variables assigned in REPL should persist in this workspace

2. **Script Evaluation** (`repl_eval_script()` → `langrunhandletraperror()`):
   - REPL calls `langrunhandletraperror(htext, result, error_msg)`
   - **Deep in call chain**: `langrunhandletraperror()` → `langrun()` → `pushprocess(nil)` (line 869 in `Common/source/lang.c`)
   - `pushprocess()` saves current `hashtablestack` to process stack (line 198 in `Common/source/process.c`)
   - Script executes with POTENTIALLY DIFFERENT hash table stack state
   - `popprocess()` restores ORIGINAL `hashtablestack` state (line 239 in `Common/source/process.c`)
   - **Workspace table MAY BE REMOVED from stack during restore**

3. **Result**:
   - Variables assigned during evaluation may not persist in workspace
   - Functions defined in REPL may not be findable in subsequent evaluations
   - `/clear` command may not properly reset workspace state
   - REPL appears to "forget" variables between evaluations

### Code References

**Where pushprocess/popprocess saves hashtablestack** (`Common/source/process.c:168-242`):
```c
boolean pushprocess (register hdlprocessrecord hp) {
    typrocessstackrecord item;

    /* ... */
    item.htablestack = hashtablestack;  // LINE 198: SAVE ENTIRE STACK
    processstack.stack [processstack.top++] = item;

    /* ... */
    if (hp != nil) {
        hashtablestack = (**hp).htablestack;  // LINE 214: RESTORE FROM PROCESS
    }

    return (true);
}

boolean popprocess (void) {
    typrocessstackrecord item;

    item = processstack.stack [--processstack.top];
    /* ... */
    hashtablestack = item.htablestack;  // LINE 239: RESTORE SAVED STACK

    return (true);
}
```

**Where langrunhandletraperror triggers push/pop** (`Common/source/lang.c:866-874`):
```c
boolean langrunhandle (Handle htext, bigstring bsresult) {
    /* ... */

    if (flpushpop)
        flpushpop = pushprocess (nil);  // LINE 869: PUSHES NIL PROCESS

    fl = langrun (htext, &val);

    if (flpushpop)
        popprocess ();  // LINE 874: RESTORES STACK

    /* ... */
}
```

**REPL Workaround #1: Force workspace onto stack** (`frontier-cli/repl_eval.c:259-267`):
```c
/* CRITICAL WORKAROUND: Force workspace onto hash table stack
 * langrunhandletraperror() does pushprocess/popprocess which restores old hashtablestack.
 * This can leave workspace table OFF the stack, causing assignments to go elsewhere.
 * Force workspace back onto stack before evaluation.
 */
pophashtable();  /* Remove whatever's on top */
pushhashtable(ws->workspace_table);  /* Put workspace on top */
currenthashtable = ws->workspace_table;  /* Ensure currenthashtable is correct */
log_debug(LOG_COMP_GENERAL, "FORCED workspace onto stack: %p", currenthashtable);

boolean ok = langrunhandletraperror(htext, result, error_msg);
```

**REPL Workaround #2: Manual hash table clearing** (`frontier-cli/repl_eval.c:142-170`):
```c
/* CRITICAL FIX: Manually clear workspace table entries WITHOUT callbacks
 * emptyhashtable() triggers callbacks (langsymboldeleted) that corrupt state.
 * We can't dispose/recreate because process stack has saved pointers to this table.
 * Instead, manually clear the hash buckets and sorted list.
 */

/* Clear sorted list */
(**ws->workspace_table).hfirstsort = nil;

/* Clear all hash buckets */
#define ctbuckets 11
for (int i = 0; i < ctbuckets; i++) {
    hdlhashnode nomad = (**ws->workspace_table).hashbucket[i];
    while (nomad != nil) {
        hdlhashnode nextnomad = (**nomad).hashlink;
        disposevaluerecord((**nomad).val, false);
        disposehandle((Handle)nomad);
        nomad = nextnomad;
    }
    (**ws->workspace_table).hashbucket[i] = nil;
}
```

### Strategic Context

**North Star: Collaborative ODB Editing (Phase 6+)**

From CLAUDE.md:
> Frontier should support Google Docs/Sheets-style collaborative editing of ODB objects:
> - Multiple users edit different ODB objects simultaneously
> - Runtime handles all concurrency, locking, and conflict resolution transparently
> - Developers write functionally single-threaded code

**Launch Requirement**: Global mutable `hashtablestack` creates race conditions and violates thread-safety requirements for Automattic partnership.

**Related Refactoring Work**:
- **ADR-005**: Thread-local parameter state migration (proven pattern for global elimination)
- **ADR-006**: Outline context refactoring (thread-local push/pop stack)
- **ADR-008**: Processor table lifecycle workaround (similar global state clobbering issue)
- **Issue #135**: Outline context refactoring (explicit context pattern)
- **CLAUDE.md**: "BURN THE GLOBALS WITH FIRE" - comprehensive global state elimination

**Pattern Recognition**: This is the **third instance** of global state clobbering (after ADR-006 outlines and ADR-008 processors). A systemic solution is needed.

---

## Existing Patterns in Codebase

### ADR-005: Thread-Local Storage for Parameter State

**Pattern**: Migrate per-thread execution state from global variables into `tythreadglobals` structure.

**How It Worked**:
1. Added fields to `tythreadglobals` structure (`Common/headers/processinternal.h`)
2. Updated `copythreadglobals()` and `swapinthreadglobals()` to save/restore state
3. Replaced global declarations with macro accessors (backward compatible)
4. Zero API changes - existing code continues working

**Key Lesson**: Thread-local storage works well for **per-thread state** that doesn't need to be shared across threads.

**Why This Doesn't Fully Solve REPL Problem**:
- `hashtablestack` is already somewhat thread-local (part of process context)
- Problem is that `pushprocess()`/`popprocess()` **saves and restores entire stack**
- Even with thread-local storage, the save/restore mechanism still clobbers REPL workspace state

### ADR-006: Outline Context Thread-Local Migration

**Pattern**: Moved `outlinedata` and `outlinestack[10]` from global variables into `tythreadglobals`.

**Result**:
- ✅ Thread-safe outline context (per-thread isolation)
- ✅ Eliminated stale pointer footguns
- ✅ Zero API changes (macros provided backward compatibility)
- ⚠️ Still uses push/pop pattern (not eliminated, just made thread-safe)

**Key Lesson**: Thread-local storage makes push/pop **thread-safe** but doesn't eliminate the **architectural pattern** of saving/restoring state.

**Why This Is Relevant**:
- Same fundamental architecture: global stack that gets saved/restored
- `hashtablestack` is conceptually similar to `outlinestack`
- Thread-local storage would help but doesn't solve the REPL-specific problem

### ADR-008: Processor Table Lifecycle Workaround

**Pattern**: Database loading overwrites global `efptable`, breaking processor resolution. Workaround saves original table before loading.

**Similarities to REPL Issue**:
- ✅ Global state gets clobbered by deep call chain
- ✅ Workaround saves original state and restores it
- ✅ Temporary solution until global state elimination (Phase 6+)

**Key Lesson**: Workarounds are acceptable when:
1. They unblock development
2. Proper fix requires systemic refactoring
3. Code clearly documents temporary nature

### Pattern from process.c: Process Stack Architecture

**How Process Stack Works**:
```c
typedef struct typrocessstackrecord {
    hdlprocessrecord hprocess;
    langerrormessagecallback errormessagecallback;
    langerrormessagecallback debugerrormessagecallback;
    hdlerrorstack herrorstack;
    hdlhashtable htablestack;  // ← THIS IS THE PROBLEM
} typrocessstackrecord;

static struct {
    short top;
    typrocessstackrecord stack[ctprocesses];
} processstack;
```

**Key Insight**: Process stack saves **ALL process-related state**, including hash table stack. When `pushprocess(nil)` is called, it saves the CURRENT `hashtablestack` and may restore a DIFFERENT one.

---

## Architectural Options

### Option 1: Thread-Local Hash Table Stack (Partial Fix)

**Description**: Move `hashtablestack` from global variable into `tythreadglobals` structure, following ADR-005 and ADR-006 patterns.

**Design**:
```c
// Common/headers/processinternal.h - ADD to tythreadglobals
typedef struct tythreadglobals {
    // ... existing fields ...

    // Hash table stack (ADR-009 migration)
    hdlhashtable hashtablestack;  // Current hash table context

    // Reserved for Phase 6+ collaborative features
    void *hashtable_reserved[4];  // CRDT context, lock state, etc.

} tythreadglobals;
```

**Macro for backward compatibility**:
```c
// Common/headers/lang.h or processinternal.h
#define hashtablestack ((**hthreadglobals).hashtablestack)
```

**Strengths**:
- ✅ Proven pattern (ADR-005, ADR-006)
- ✅ Zero API changes (macro provides transparent access)
- ✅ Thread-safe by construction (per-thread isolation)
- ✅ Minimal code changes (~50 lines across 3 files)

**Weaknesses**:
- ❌ **Doesn't solve REPL problem**: `pushprocess()`/`popprocess()` still saves/restores thread-local stack
- ❌ REPL workaround still needed (workspace still gets clobbered)
- ⚠️ Helps with thread-safety but not with REPL persistence issue

**Why This Is Only Partial**:
- Thread-local storage isolates per-thread state
- But `pushprocess(nil)` saves thread-local `hashtablestack` to process stack
- `popprocess()` restores saved stack, losing REPL workspace changes
- **Root cause**: Save/restore mechanism, not global vs thread-local storage

**Migration Complexity**: **LOW** - Single PR, ~50 lines of code changes

**Verdict**: **Good first step for thread-safety, but insufficient for REPL**

---

### Option 2: REPL-Specific Hash Table Stack Preservation (Current Workaround)

**Description**: Force REPL workspace back onto hash table stack before evaluation, bypassing the `pushprocess()`/`popprocess()` restoration logic.

**Design** (already implemented in PR #300):
```c
// frontier-cli/repl_eval.c:259-267
pophashtable();  /* Remove whatever's on top */
pushhashtable(ws->workspace_table);  /* Put workspace on top */
currenthashtable = ws->workspace_table;  /* Ensure currenthashtable is correct */

boolean ok = langrunhandletraperror(htext, result, error_msg);

/* After execution */
currenthashtable = ws->workspace_table;  /* Restore correct state */
```

**Strengths**:
- ✅ Unblocks REPL development (97.8% tests passing)
- ✅ Isolated to REPL code (doesn't affect runtime)
- ✅ Simple to understand and maintain
- ✅ Clearly documented as temporary workaround

**Weaknesses**:
- ❌ Doesn't fix function definitions (functions still lost between evaluations)
- ❌ `/clear` command still broken (variables persist incorrectly)
- ❌ Technical debt (must be removed in Phase 6+)
- ❌ Fragile - relies on forcing state between calls

**Why This Fails for Some Cases**:
- Function definitions go into `currenthashtable` during compilation
- `pushprocess()`/`popprocess()` may restore OLD `currenthashtable`
- Functions defined in REPL become unreachable in next evaluation
- `/clear` tries to clear workspace but process stack has stale pointer

**Migration Complexity**: **TRIVIAL** - Already implemented

**Verdict**: **Acceptable temporary solution, but doesn't solve all REPL issues**

---

### Option 3: Modify pushprocess/popprocess to Preserve REPL Workspace

**Description**: Add special case to `pushprocess()`/`popprocess()` to detect and preserve REPL workspace table when saving/restoring `hashtablestack`.

**Design**:
```c
// Common/source/process.c - Add REPL workspace detection
extern hdlhashtable get_repl_workspace_if_active(void);  // Returns nil if not REPL

boolean pushprocess (register hdlprocessrecord hp) {
    typrocessstackrecord item;

    /* ... existing code ... */

    item.htablestack = hashtablestack;  // Save stack

    /* REPL SPECIAL CASE: Preserve workspace across push/pop */
    hdlhashtable repl_workspace = get_repl_workspace_if_active();
    if (repl_workspace != nil) {
        item.repl_workspace = repl_workspace;  // Save workspace reference
    }

    processstack.stack [processstack.top++] = item;
    /* ... */
}

boolean popprocess (void) {
    typrocessstackrecord item;

    item = processstack.stack [--processstack.top];

    hashtablestack = item.htablestack;  // Restore stack

    /* REPL SPECIAL CASE: Re-push workspace after restore */
    if (item.repl_workspace != nil) {
        pushhashtable(item.repl_workspace);
        currenthashtable = item.repl_workspace;
    }

    /* ... */
}
```

**Strengths**:
- ✅ Fixes REPL issue at the source (process stack save/restore)
- ✅ REPL workspace persists correctly across evaluations
- ✅ Functions defined in REPL remain accessible
- ✅ `/clear` command works correctly

**Weaknesses**:
- ❌ Adds REPL-specific logic to core runtime (process.c)
- ❌ Tight coupling between REPL and process management
- ❌ Still relies on global `hashtablestack` (not thread-safe)
- ❌ Doesn't generalize to other similar problems (processors, outlines)
- ❌ Technical debt - must be removed when globals eliminated

**Why This Is Wrong Direction**:
- Adding special cases to core runtime for specific use cases is anti-pattern
- Doesn't solve the fundamental problem (global mutable state)
- Creates coupling between REPL (frontier-cli) and runtime (Common/source)
- Will be thrown away when proper solution (Option 5) is implemented

**Migration Complexity**: **MEDIUM** - Modify core process.c, add REPL detection API

**Verdict**: **Rejected - Wrong architectural direction**

---

### Option 4: Alternative Evaluation Path Bypassing Process Stack

**Description**: Create REPL-specific evaluation path that doesn't call `pushprocess()`/`popprocess()`, avoiding hash table stack restoration entirely.

**Design**:
```c
// New REPL evaluation function (bypass pushprocess/popprocess)
boolean repl_langrun_direct(Handle htext, hdlhashtable workspace, bigstring result, bigstring error_msg) {
    /* Set workspace as current */
    hdlhashtable saved_current = currenthashtable;
    currenthashtable = workspace;

    /* Compile script */
    hdltreenode hcode = nil;
    if (!langcompilehandle(htext, &hcode)) {
        currenthashtable = saved_current;
        copyctopstring("Compilation error", error_msg);
        return false;
    }

    /* Execute WITHOUT pushprocess/popprocess */
    tyvaluerecord val;
    if (!langevaluate(hcode, &val)) {
        currenthashtable = saved_current;
        langdisposetree(hcode);
        copyctopstring("Execution error", error_msg);
        return false;
    }

    /* Convert result to string */
    coercetostring(&val);
    copystring(val.data.stringvalue, result);

    /* Cleanup */
    disposevaluerecord(val, false);
    langdisposetree(hcode);
    currenthashtable = saved_current;

    return true;
}
```

**Strengths**:
- ✅ Bypasses problematic `pushprocess()`/`popprocess()` entirely
- ✅ Full control over hash table stack state
- ✅ No workarounds needed (clean implementation)
- ✅ REPL-specific code stays in frontier-cli (doesn't touch runtime)

**Weaknesses**:
- ❌ Duplicates langrun logic (error handling, callbacks, traps)
- ❌ Misses error recovery mechanisms in `langrunhandletraperror()`
- ❌ Must manually implement all langrun side effects (callbacks, script stack, etc.)
- ❌ High maintenance burden (runtime changes require REPL changes)
- ❌ Still relies on global `currenthashtable` (not thread-safe)

**Why This Is Risky**:
- `langrunhandletraperror()` has 25+ years of error handling logic
- REPL would need to duplicate ALL of that (callbacks, traps, cleanup)
- Easy to miss edge cases (script stack overflow, trap handlers, etc.)
- Creates divergence between REPL and normal script execution

**Migration Complexity**: **HIGH** - Reimplement langrun logic for REPL

**Verdict**: **Rejected - Too risky and high maintenance burden**

---

### Option 5: Explicit Hash Table Context (PROPER LONG-TERM SOLUTION)

**Description**: Eliminate global `hashtablestack` and `currenthashtable`, pass hash table context explicitly through function parameters.

**Design**:
```c
/* Hash table context structure */
typedef struct hashtable_context {
    hdlhashtable current;      // Current scope
    hdlhashtable stack[32];    // Stack of pushed scopes
    int stack_depth;           // Current stack depth
    _Atomic uint32_t refcount; // For collaborative ODB (Phase 6+)
} hashtable_context;

/* Create context */
hashtable_context* hashtable_context_create(hdlhashtable root);
void hashtable_context_retain(hashtable_context* ctx);
void hashtable_context_release(hashtable_context* ctx);

/* Push/pop operations on context */
void hashtable_context_push(hashtable_context* ctx, hdlhashtable table);
hdlhashtable hashtable_context_pop(hashtable_context* ctx);

/* Verb evaluation with explicit context */
boolean langrun_context(
    hashtable_context* ctx,
    Handle htext,
    tyvaluerecord* result,
    bigstring error_msg
);

/* REPL uses explicit context */
boolean repl_eval_script(
    repl_workspace* ws,
    const char* script,
    bigstring result,
    bigstring error_msg
) {
    /* Create context with workspace as root */
    hashtable_context* ctx = hashtable_context_create(ws->workspace_table);

    /* Evaluate with explicit context */
    boolean ok = langrun_context(ctx, htext, &val, error_msg);

    /* Cleanup */
    hashtable_context_release(ctx);
    return ok;
}
```

**Strengths**:
- ✅ **No global mutable state** - thread-safe by construction
- ✅ **Clear lifecycle management** - context created, used, destroyed
- ✅ **REPL problem solved completely** - workspace persists in context
- ✅ **Generalizes to all use cases** - processors, outlines, hash tables
- ✅ **Foundation for Phase 6+** - collaborative ODB ready
- ✅ **Reference counting enables cross-thread sharing** (if needed)

**Weaknesses**:
- ❌ **MASSIVE API break** - changes EVERY function that touches hash tables
- ❌ **10,000+ lines of code changes** (estimate)
- ❌ **Requires updating all verb processors** (48+ files)
- ❌ **Requires updating all language runtime code** (lang.c, langinternal.c, etc.)
- ❌ **Multi-PR effort** (Phase 6+ work, not Phase 3-5)

**Why This Is The Proper Fix**:
- Eliminates global mutable state (CLAUDE.md: "BURN THE GLOBALS WITH FIRE")
- Thread-safe by design (no race conditions)
- Clear ownership and lifecycle (no dangling pointers)
- Enables collaborative ODB (multiple users editing simultaneously)
- Aligns with modern C architecture (explicit context passing)

**When To Do This**:
- **Phase 6+**: Collaborative ODB refactoring
- **Coordinate with**: Issue #135 (outline context), ADR-005/006 (thread-local patterns)
- **After**: REPL Phase 1-4 complete, verb implementations stable

**Migration Strategy**:
1. Create `hashtable_context` structure and lifecycle functions
2. Add `*_context()` variants of all hash table functions (backward compatible)
3. Gradually migrate verb processors to use context variants
4. Add deprecation warnings to global accessors
5. Remove globals when migration complete

**Migration Complexity**: **VERY HIGH** - Multi-PR effort, 6+ months of work

**Verdict**: **Correct long-term solution, but too early for Phase 3**

---

## Decision

**Adopt a two-phase approach:**

### Phase 3-5 (Immediate): Hybrid Workaround + Thread-Local Migration

**Recommended Approach**: **Combine Option 1 + Option 2**

1. **Migrate `hashtablestack` to thread-local storage** (Option 1)
   - Enables thread-safety for collaborative ODB (Phase 6+)
   - Follows proven ADR-005/ADR-006 pattern
   - Zero API changes (backward compatible macros)
   - Foundation for future refactoring

2. **Keep REPL workarounds** (Option 2) until Phase 6+
   - Accept 97.8% pass rate as "good enough" for Phase 1
   - Document 3 failing tests as known limitations
   - Mark workarounds clearly as temporary
   - Plan removal when Option 5 implemented

### Phase 6+ (Future): Explicit Context Architecture

**Ultimate Solution**: **Implement Option 5** (Explicit Hash Table Context)

When collaborative ODB work begins:
1. Create `hashtable_context` structure
2. Add `*_context()` function variants
3. Migrate verb processors incrementally
4. Remove REPL workarounds
5. Remove global `hashtablestack` and `currenthashtable`

### Rationale

**Why Hybrid Approach**:

1. **Unblocks Development**: REPL Phase 1 can ship with 97.8% pass rate (133/136 tests)
   - `/clear`, function definitions, multiple functions marked as known limitations
   - Still provides valuable interactive development experience
   - Feedback from users will inform Phase 6+ architecture

2. **Thread-Safety Foundation**: Thread-local `hashtablestack` enables Phase 6+ collaborative ODB
   - Proven pattern (ADR-005, ADR-006)
   - Low risk, high value
   - Required for Automattic partnership

3. **Avoids Premature Optimization**: Option 5 (explicit context) is correct but premature
   - Requires 10,000+ LOC changes
   - Must coordinate with outline context refactoring (Issue #135)
   - Should wait until REPL requirements fully understood

4. **Clear Migration Path**: Workarounds documented, removal plan clear
   - Code comments reference this ADR
   - Known limitations documented in tests
   - Phase 6+ work can proceed with confidence

5. **Aligns with Project Principles**: From CLAUDE.md:
   - "Default to the proper, maintainable, long-term solution"
   - But also: "Workarounds acceptable when proper fix requires systemic refactoring"
   - This ADR documents both the workaround AND the proper fix

**Why Not Other Options**:

- **Option 3 (Modify pushprocess)**: Wrong direction - adds coupling, doesn't generalize
- **Option 4 (Alternative eval path)**: Too risky - duplicates 25+ years of error handling
- **Option 5 (Explicit context) now**: Too early - REPL requirements not fully understood

---

## Implementation Plan

### Phase 3A: Thread-Local Hash Table Stack (This PR)

**Files Modified**:
1. `Common/headers/processinternal.h` - Add `hashtablestack` to `tythreadglobals`
2. `Common/source/process.c` - Update thread swap functions
3. `Common/headers/lang.h` - Replace extern with macro

**Implementation Steps**:

**1. Add Field to tythreadglobals**:
```c
// Common/headers/processinternal.h
typedef struct tythreadglobals {
    // ... existing fields ...

    // Hash table stack (ADR-009: Phase 3 migration)
    hdlhashtable hashtablestack;  // Current hash table stack

    // Reserved for Phase 6+ collaborative features
    void *hashtable_reserved[4];

} tythreadglobals;
```

**2. Update Thread Swap Functions**:
```c
// Common/source/process.c - copythreadglobals()
void copythreadglobals(hdlthreadglobals hglobals) {
    // ... existing code ...

    // ADR-009: Hash table stack migration
    (**hg).hashtablestack = hashtablestack;
}

// Common/source/process.c - swapinthreadglobals()
void swapinthreadglobals(hdlthreadglobals hglobals) {
    // ... existing code ...

    // ADR-009: Hash table stack migration
    hashtablestack = (**hg).hashtablestack;
}

// Common/source/process.c - newthreadglobals()
boolean newthreadglobals(hdlthreadglobals *hglobals) {
    // ... existing code ...

    // ADR-009: Hash table stack initialization
    (**hg).hashtablestack = nil;

    return true;
}
```

**3. Replace Global Declaration with Macro**:
```c
// Common/headers/lang.h (or processinternal.h)
// OLD (remove):
// extern hdlhashtable hashtablestack;

// NEW (add):
// ADR-009: Thread-local hash table stack (backward-compatible macro)
#define hashtablestack ((**hthreadglobals).hashtablestack)
```

**Testing Strategy**:
1. Run full test suite: `./tools/run_headless_tests.sh`
2. Run integration tests: `cd tests && make test-integration`
3. Verify REPL tests still pass (133/136)
4. Verify no regression in existing verb tests

**Success Criteria**:
- ✅ All existing tests pass (no regression)
- ✅ Thread-local `hashtablestack` accessible via macro
- ✅ Zero API changes visible to existing code
- ✅ Foundation laid for Phase 6+ refactoring

**Estimated Effort**: 2-4 hours (simple refactoring following proven pattern)

---

### Phase 3A Implementation Note: Bootstrap Initialization Constraint

**CRITICAL LIMITATION**: The macro migration (`#define hashtablestack ((**hthreadglobals).hashtablestack)`) could not be completed in Phase 3A due to bootstrap initialization ordering issues.

**Problem**: Early initialization code (before `hthreadglobals` is created) directly accesses `hashtablestack`:

1. **`inittablestructure()`** (Common/source/langhash.c) - Called during runtime initialization
2. **`main()` bootstrap** (frontier-cli/main.c) - Database loading before thread context exists
3. **Other pre-thread initialization** - Various setup code that runs before threading is initialized

**What Happens When Macro is Enabled**:
```c
// With macro enabled:
#define hashtablestack ((**hthreadglobals).hashtablestack)

// This code runs BEFORE hthreadglobals is created:
inittablestructure();  // Segfaults! hthreadglobals is nil

// Macro expands to:
(**nil).hashtablestack  // CRASH
```

**Current Workaround** (Partial Migration):
```c
// Common/headers/lang.h - Macro commented out for now:
// #define hashtablestack ((**hthreadglobals).hashtablestack)

// Global variable retained for direct access during bootstrap:
extern hdltablestack hashtablestack;

// Thread-local field EXISTS and is saved/restored:
// Common/headers/processinternal.h:
typedef struct tythreadglobals {
    hdltablestack htablestack;  // ← Field exists, just not accessed via macro yet
} tythreadglobals;

// Common/source/process.c - copythreadglobals/swapinthreadglobals:
(**hg).htablestack = hashtablestack;  // ← Saves global to thread-local
hashtablestack = (**hg).htablestack;  // ← Restores thread-local to global
```

**Why This is Still Valuable**:
1. ✅ **Foundation for Phase 6+**: Thread-local field exists and is saved/restored
2. ✅ **No regression**: Global variable works as before
3. ✅ **Incremental progress**: When bootstrap is refactored, macro can be enabled trivially
4. ✅ **Documented limitation**: Future developers understand the constraint

**Path Forward** (Phase 4-5 or Phase 6+):

**Option A: Bootstrap Refactoring** (Recommended for Phase 4-5):
1. Move `inittablestructure()` to run AFTER `hthreadglobals` is created
2. Ensure all hash table operations happen post-thread-initialization
3. Enable macro once bootstrap ordering is fixed
4. File issue: "Complete hashtablestack macro migration after bootstrap refactoring"

**Option B: Defer to Phase 6+** (If bootstrap refactoring is too risky):
1. Keep global variable until explicit context architecture
2. Skip macro migration entirely (go straight to explicit context)
3. Remove global and thread-local when `hashtable_context` is implemented

**Precedent**: ADR-006 (Outline Context) had similar pre-thread initialization issues with `outlinedata`. Similar workarounds were needed until bootstrap could be refactored.

**Follow-up Issue**: #XXX - "Complete hashtablestack macro migration after bootstrap refactoring"

---

### Phase 3B: Document REPL Limitations (This PR)

**Files Modified**:
1. `tests/integration/test_cases/repl_commands.yaml` - Mark failing tests with known issue
2. `docs/CLI_USAGE_GUIDE.md` - Document REPL limitations
3. `planning/phase4/REPL_INTERACTIVE_MODE_DESIGN.md` - Add "Known Limitations" section

**Known Limitations to Document**:

**1. `/clear` Command**:
```yaml
# tests/integration/test_cases/repl_commands.yaml
- name: "REPL - /clear command clears workspace"
  description: |
    KNOWN LIMITATION (ADR-009): Variables assigned after /clear may persist
    incorrectly due to hash table stack restoration in langrunhandletraperror().
    Will be fixed in Phase 6+ (Explicit Hash Table Context architecture).
  script: |
    workspace.x = 42;
    # /clear  # Commented out - command not working correctly
    workspace.x
  expected_success: true
  expected_result: 42  # Should be undefined after clear
  known_issue: "ADR-009: Hash table stack restoration"
```

**2. Function Definitions**:
```yaml
- name: "REPL - define and call function"
  description: |
    KNOWN LIMITATION (ADR-009): Functions defined in REPL may not be callable
    in subsequent evaluations due to hash table stack restoration.
    Will be fixed in Phase 6+.
  script: |
    local(add = fn(a, b) { return a + b });
    add(2, 3)
  expected_success: true
  expected_result: 5
  known_issue: "ADR-009: Function persistence"
```

**3. Multiple Function Calls**:
```yaml
- name: "REPL - multiple function calls"
  description: |
    KNOWN LIMITATION (ADR-009): Related to function definition persistence.
  script: |
    local(double = fn(x) { return x * 2 });
    double(5) + double(3)
  expected_success: true
  expected_result: 16
  known_issue: "ADR-009: Function persistence"
```

**Documentation Updates**:

`docs/CLI_USAGE_GUIDE.md`:
```markdown
## REPL Known Limitations

The following REPL features have known limitations due to architectural
constraints documented in ADR-009:

1. **`/clear` command**: Variables may persist incorrectly after clearing workspace
2. **Function definitions**: Functions defined in REPL may not be callable in subsequent evaluations
3. **Multiple function calls**: Related to function definition persistence

These limitations will be resolved in Phase 6+ when the hash table stack
architecture is refactored to use explicit context passing (ADR-009 Option 5).

Current workaround: Use `workspace.` prefix for all variables to ensure persistence.
```

---

### Phase 6+: Explicit Hash Table Context (Future Work)

**NOT IMPLEMENTED IN THIS PR** - Deferred to collaborative ODB refactoring.

**Coordination Required**:
- Issue #135: Outline context refactoring
- ADR-005/006: Thread-local migration patterns
- ADR-008: Processor table lifecycle

**Migration Steps** (when Phase 6+ begins):

**1. Create `hashtable_context` Structure**:
```c
// Common/headers/langinternal.h
typedef struct hashtable_context {
    hdlhashtable current;
    hdlhashtable stack[32];
    int stack_depth;
    _Atomic uint32_t refcount;  // For collaborative ODB
} hashtable_context;

hashtable_context* hashtable_context_create(hdlhashtable root);
void hashtable_context_retain(hashtable_context* ctx);
void hashtable_context_release(hashtable_context* ctx);
void hashtable_context_push(hashtable_context* ctx, hdlhashtable table);
hdlhashtable hashtable_context_pop(hashtable_context* ctx);
```

**2. Add Context Variants of Core Functions**:
```c
// Add alongside existing functions (backward compatible)
boolean langrun_context(hashtable_context* ctx, Handle htext, tyvaluerecord* result);
boolean langcompile_context(hashtable_context* ctx, Handle htext, hdltreenode* hcode);
boolean langevaluate_context(hashtable_context* ctx, hdltreenode hcode, tyvaluerecord* result);
```

**3. Migrate Verb Processors Incrementally**:
```c
// OLD (uses globals):
boolean filefunctionvalue(short token, hdltreenode hparam1, tyvaluerecord *vreturned) {
    // Uses global currenthashtable
}

// NEW (uses context):
boolean filefunctionvalue_context(
    hashtable_context* ctx,
    short token,
    hdltreenode hparam1,
    tyvaluerecord *vreturned
) {
    // Uses ctx->current instead of global
}
```

**4. Update REPL to Use Context**:
```c
// frontier-cli/repl_eval.c
boolean repl_eval_script(
    repl_workspace* ws,
    const char* script,
    bigstring result,
    bigstring error_msg
) {
    // Create context with workspace as root
    hashtable_context* ctx = hashtable_context_create(ws->workspace_table);

    // Evaluate with explicit context
    boolean ok = langrun_context(ctx, htext, &val);

    // Cleanup
    hashtable_context_release(ctx);

    // Remove workarounds (lines 259-267, 142-170)
    return ok;
}
```

**5. Remove Workarounds**:
- Delete forced workspace push/pop (repl_eval.c:259-267)
- Delete manual hash table clearing (repl_eval.c:142-170)
- Remove thread-local `hashtablestack` macro (no longer needed)
- Remove global `hashtablestack` and `currenthashtable` variables

**Success Criteria** (Phase 6+):
- [ ] All REPL tests pass (136/136, including /clear and functions)
- [ ] No global `hashtablestack` or `currenthashtable` variables
- [ ] No REPL workarounds (clean implementation)
- [ ] Thread-safe hash table access (concurrent evaluations work)
- [ ] Collaborative ODB enabled (multiple users editing simultaneously)

---

## Code Changes Summary

### Phase 3A: Thread-Local Migration (Immediate)

**Files Modified**:
1. `Common/headers/processinternal.h` - Add `hashtablestack` field (~5 lines)
2. `Common/source/process.c` - Update thread swap functions (~15 lines)
3. `Common/headers/lang.h` - Replace extern with macro (~3 lines)

**Lines of Code Impact**:
- Added: ~23 lines (struct field, swap code, macro)
- Removed: ~2 lines (extern declaration)
- Modified: 0 lines (existing code unchanged)
- Net: +21 lines

---

### Phase 3B: Documentation (Immediate)

**Files Modified**:
1. `tests/integration/test_cases/repl_commands.yaml` - Mark 3 tests as known issues
2. `docs/CLI_USAGE_GUIDE.md` - Add "Known Limitations" section
3. `planning/phase4/REPL_INTERACTIVE_MODE_DESIGN.md` - Document workarounds

**Lines of Code Impact**:
- Added: ~50 lines (documentation)

---

### Phase 6+: Explicit Context (Future)

**NOT IN THIS PR** - Estimated changes when Phase 6+ begins:

**Files Modified**: 50+ files (all verb processors, language runtime, REPL)

**Lines of Code Impact**:
- Added: ~2000 lines (context structure, lifecycle, context variants)
- Removed: ~500 lines (global variables, workarounds)
- Modified: ~8000 lines (verb processors, runtime functions)
- Net: ~10,000 LOC changes

---

## Consequences

### Positive (Phase 3A: Thread-Local Migration)

1. **Thread-Safety Foundation**: Hash table stack becomes per-thread, enabling Phase 6+ collaborative ODB

2. **Proven Pattern**: Follows ADR-005 and ADR-006 thread-local migration (low risk)

3. **Zero API Impact**: Macro provides backward compatibility (existing code unchanged)

4. **Clear Migration Path**: Foundation for Phase 6+ explicit context architecture

5. **Unblocks REPL Development**: REPL Phase 1 can ship with 97.8% pass rate

### Negative (Phase 3A: Workarounds Persist)

1. **Technical Debt**: REPL workarounds remain (must be removed in Phase 6+)

2. **3 Failing Tests**: `/clear`, function definitions, multiple functions marked as known issues

3. **User Experience Gap**: REPL doesn't fully meet "variables persist" contract

4. **Workaround Complexity**: Manual hash table manipulation is fragile

### Positive (Phase 6+: Explicit Context)

1. **Complete Solution**: All REPL issues resolved (136/136 tests pass)

2. **No Global State**: Thread-safe by construction (collaborative ODB enabled)

3. **Clear Ownership**: Explicit lifecycle management (no dangling pointers)

4. **Generalizes**: Pattern applies to all global state (processors, outlines, databases)

### Negative (Phase 6+: Migration Cost)

1. **Large Effort**: 10,000+ LOC changes across 50+ files

2. **API Breaking**: Requires updating all verb processors

3. **Coordination Required**: Must align with Issue #135 (outline context)

4. **Testing Burden**: All verb tests must be re-validated

---

### Risks & Mitigation

**Risk 1: Thread-local migration breaks existing code**
- **Mitigation**: Macro provides exact same API (global → thread-local transparent)
- **Testing**: Full test suite validates no regression
- **Precedent**: ADR-005 and ADR-006 worked flawlessly

**Risk 2: REPL workarounds become permanent**
- **Mitigation**: Clearly marked as temporary in code comments
- **Mitigation**: This ADR documents removal plan (Phase 6+)
- **Mitigation**: Failing tests remind us of technical debt

**Risk 3: Phase 6+ refactoring too complex**
- **Mitigation**: Break into incremental PRs (add context variants first)
- **Mitigation**: Maintain backward compatibility during transition
- **Mitigation**: Coordinate with Issue #135 (shared refactoring patterns)

**Risk 4: Users expect 100% REPL functionality**
- **Mitigation**: Document known limitations in CLI_USAGE_GUIDE.md
- **Mitigation**: Clear error messages for unsupported features
- **Mitigation**: 97.8% pass rate is "good enough" for Phase 1

---

## Alternatives Considered But Rejected

### Process Stack Redesign

Could redesign process stack to NOT save/restore `hashtablestack`:

```c
typedef struct typrocessstackrecord {
    hdlprocessrecord hprocess;
    // ... other fields ...
    // DON'T save hashtablestack
} typrocessstackrecord;
```

**Rejected Because**:
- Breaking change to core runtime (high risk)
- Unknown if other code relies on stack restoration
- Doesn't solve fundamental problem (global state)
- Better to eliminate globals entirely (Option 5) than patch save/restore

### REPL-Specific Process Record

Create special process record for REPL that preserves workspace:

```c
hdlprocessrecord repl_process = create_repl_process(ws->workspace_table);
pushprocess(repl_process);  // Saves REPL workspace
```

**Rejected Because**:
- Still relies on global `hashtablestack`
- Tight coupling between REPL and process management
- Doesn't generalize (same problem exists for processors, outlines)
- Option 3 (modify pushprocess) is cleaner version of this

---

## References

### Internal Documentation

- **CLAUDE.md**: "BURN THE GLOBALS WITH FIRE" (global state elimination mandate)
- **CLAUDE.md**: "Collaborative ODB Editing - North Star Vision" (strategic context)
- **ADR-005**: Parameter State Thread-Safety (thread-local migration pattern)
- **ADR-006**: Outline Push/Pop Elimination (thread-local outline context)
- **ADR-008**: Processor Table Lifecycle Workaround (similar global clobbering issue)
- **Issue #135**: Outline context refactoring (explicit context pattern)
- **planning/phase4/REPL_INTERACTIVE_MODE_DESIGN.md**: REPL architecture document

### Related PRs and Issues

- **PR #300**: REPL Interactive Mode Phase 1 (where workarounds discovered)
- **Issue TBD**: File issue for explicit hash table context refactoring (Phase 6+)

### Code References

- `Common/source/process.c:168-242` - `pushprocess()`/`popprocess()` implementation
- `Common/source/lang.c:866-874` - Where langrunhandle calls pushprocess
- `frontier-cli/repl_eval.c:259-267` - REPL workspace forcing workaround
- `frontier-cli/repl_eval.c:142-170` - Manual hash table clearing workaround

### Related ADRs

- **ADR-001**: Multi-Database Context (db_context pattern)
- **ADR-002**: Context-Based Format Versioning
- **ADR-005**: Parameter State Thread-Safety (proven thread-local pattern)
- **ADR-006**: Outline Push/Pop Elimination (thread-local outline context)
- **ADR-008**: Processor Table Lifecycle Workaround (global state clobbering)

---

## Success Metrics

### Phase 3A: Thread-Local Migration Complete When:

- [x] `hashtablestack` field added to `tythreadglobals`
- [x] Thread swap functions preserve hash table stack (copythreadglobals/swapinthreadglobals)
- [ ] Macro replaces extern declaration (blocked by bootstrap issue #XXX)
- [ ] No direct global variable references remain (blocked by bootstrap issue #XXX)
- [x] Full test suite passes (no regression)
- [x] REPL tests still pass (133/136)
- [x] Bootstrap initialization constraint documented
- [x] Follow-up issue filed for macro migration (Issue #305)

**Status**: Partially complete (6/8). Macro migration blocked by bootstrap initialization ordering - requires refactoring of early initialization code that runs before `hthreadglobals` is created. See "Phase 3A Implementation Note: Bootstrap Initialization Constraint" section above for details.

### Phase 3B: Documentation Complete When:

- [ ] 3 failing REPL tests marked with `known_issue: "ADR-009"`
- [ ] CLI_USAGE_GUIDE.md documents known limitations
- [ ] REPL_INTERACTIVE_MODE_DESIGN.md updated with workaround explanation
- [ ] Code comments reference this ADR

### Phase 6+: Explicit Context Complete When:

- [ ] `hashtable_context` structure implemented
- [ ] Context lifecycle functions working (create, retain, release, push, pop)
- [ ] `langrun_context()` and related functions implemented
- [ ] All verb processors migrated to context variants
- [ ] REPL workarounds removed (clean implementation)
- [ ] All REPL tests pass (136/136, including /clear and functions)
- [ ] No global `hashtablestack` or `currenthashtable` variables
- [ ] Thread-safe hash table access (concurrent evaluations work)
- [ ] Collaborative ODB enabled (multiple users editing simultaneously)

---

## Future Work

### Phase 4-5: Complete Thread-Local Migration

**Other Global State to Migrate**:
- Database context (partially done - ADR-001, ADR-002)
- Outline context (partially done - ADR-006)
- Processor tables (workaround - ADR-008)

**Audit Remaining Globals**:
- Identify all mutable globals in language runtime
- Classify as thread-local vs truly shared (locks, caches)
- Migrate thread-local state to `tythreadglobals`

### Phase 6: Explicit Context Architecture

**Design Unified Context**:
- Consider single `execution_context` with hash tables + outline + database + thread state
- Or separate contexts (`hashtable_context`, `outline_context`, `db_context`) that compose
- Prototype both approaches before committing

**Incremental Migration Strategy**:
1. Create context structures and lifecycle functions
2. Add `*_context()` variants (backward compatible)
3. Migrate new code to use contexts
4. Add deprecation warnings to global accessors
5. Migrate existing code incrementally
6. Remove globals when migration complete

**Coordination Points**:
- Issue #135: Outline context refactoring
- ADR-008: Processor table lifecycle
- Collaborative ODB requirements (Phase 6+)

### Phase 7+: Global State Elimination

**Final Goal**: "No mutable globals" milestone
- All per-thread state in `tythreadglobals` or explicit contexts
- All shared state protected by locks or immutable
- Thread-safe by construction
- Ready for collaborative ODB launch

---

## Appendix: Test Status

### REPL Test Results (PR #300)

**Total Tests**: 136
**Passing**: 133 (97.8%)
**Failing**: 3 (2.2%)

**Failing Tests**:

1. **`/clear` command** (test_cases/repl_commands.yaml:45):
   - Expected: Variables cleared after `/clear`
   - Actual: Variables persist (hash table not properly reset)
   - Root cause: `pushprocess()`/`popprocess()` restores old hash table stack

2. **Function definitions** (test_cases/repl_sessions.yaml:23):
   - Expected: Function defined in REPL callable in next evaluation
   - Actual: Function not found (lost during hash table stack restore)
   - Root cause: Function added to `currenthashtable`, which gets replaced

3. **Multiple function calls** (test_cases/repl_sessions.yaml:34):
   - Expected: Call same function multiple times
   - Actual: Second call fails (function lost)
   - Root cause: Related to #2 (function definition persistence)

**Passing Functionality** (133 tests):
- Basic arithmetic and expressions
- Workspace variable persistence (with `workspace.` prefix)
- String operations
- Boolean logic
- Table operations
- Value display
- Error handling
- `/help`, `/exit`, `/vars` commands (partial - `/clear` broken)

---

## Appendix: Why This Matters for Collaborative ODB

From CLAUDE.md:
> Frontier should support Google Docs/Sheets-style collaborative editing of ODB objects:
> - Multiple users edit different ODB objects simultaneously
> - Runtime handles all concurrency, locking, and conflict resolution transparently
> - Developers write functionally single-threaded code

**Global `hashtablestack` makes this impossible:**
- Thread A pushes workspace → Thread B's pushprocess restores → Thread A's workspace lost
- No locking protects `hashtablestack` access → race conditions
- Process stack save/restore is not atomic → partial state visible to other threads

**Thread-local `hashtablestack` makes it possible (Phase 3A):**
- Each thread has own hash table stack (no interference)
- `pushprocess()`/`popprocess()` saves/restores PER-THREAD stack
- Still have REPL issue but at least thread-safe

**Explicit context makes it perfect (Phase 6+):**
- No globals at all (thread-safe by construction)
- Clear ownership and lifecycle (reference counting)
- Context can be passed across threads (if needed)
- No save/restore issues (context passed explicitly)

**This workaround buys time** to implement collaborative ODB properly, but it's not the final architecture.

---

## Implementation Update: QuickScript Model Decision

### Context

After initial implementation of the workaround approach (forcing workspace onto hash table stack), user feedback identified a better path: **abandon workspace persistence workarounds entirely and adopt the QuickScript model** from legacy Frontier.

### The Insight

During code review, it became clear that fighting against Frontier's thread lifecycle system with workarounds was the wrong direction. The legacy QuickScript window demonstrated that users actually appreciate a simpler model:

- **Each evaluation runs in its own thread** → automatic cleanup
- **Local variables don't persist** (by design)
- **Users manage persistence explicitly** via database paths

This is fundamentally simpler and follows proven Frontier patterns.

### Decision: Implement QuickScript Model (Merged 2026-01-15)

**Changes from Original Workaround Approach**:

1. **Removed workspace management**:
   - Deleted `repl_workspace_init()`, `repl_workspace_clear()`, `repl_workspace_cleanup()`
   - Removed ~300 lines of workspace forcing/management code
   - Simplified `repl_eval_script()` to just call `langrunhandletraperror()`

2. **Removed workspace-dependent commands**:
   - Deleted `/clear` command (no workspace to clear)
   - Deleted `/vars` command (no workspace to display)
   - Updated `/help` to explain QuickScript model

3. **Documented three persistence scopes** in CLI_USAGE_GUIDE.md:
   - **Local** (evaluation-scoped): Variables don't persist
   - **Session** (system.temp.*): Persist within CLI session
   - **Disk** (workspace.*): Persist when database saved

4. **Marked 21 tests as not applicable**:
   - Tests for `/vars` and `/clear` commands skipped
   - Updated test file headers to explain QuickScript model

### Why QuickScript is Better

**Simplicity**:
- Removes ~175 lines of complex workaround code
- No hash table stack manipulation needed
- Follows Frontier's thread lifecycle naturally

**User Experience**:
- Clear, documented persistence scopes
- No surprising behavior from workarounds
- Simple mental model: each Enter = new thread

**Architecture**:
- Aligns with "BURN THE GLOBALS WITH FIRE" principle
- Thread-local infrastructure still in place (unused for now)
- Foundation still supports Phase 6+ explicit context

**Lesson**:
- Sometimes the simplest solution is to follow established patterns
- Don't fight the runtime; work with it
- When in doubt, ask: "How did legacy Frontier solve this?"

### Impact on Thread-Local Migration

QuickScript model **doesn't change** the thread-local hash table stack infrastructure:
- Thread-local field still added to `tythreadglobals`
- Thread swap functions still save/restore hash table stack
- Macro still commented out pending Phase 6+
- Foundation still supports future explicit context architecture

The only change: QuickScript doesn't USE the workspace table, so workarounds are unnecessary.

### Status After Merge (2026-01-15)

- ✅ PR #304 merged to develop
- ✅ QuickScript model implemented and tested
- ✅ Thread-local infrastructure in place
- ✅ All non-workspace functionality working (all basic REPL tests pass)
- ✅ 21 workaround-related tests properly marked as not applicable
- ⏳ Phase 6+: Enable thread-local macro when bootstrap refactored
- ⏳ Phase 6+: Explicit context architecture will remove remaining globals

### QuickScript as MVP - Scope Clarification

**Product Strategy** (Added 2026-01-15):

The QuickScript model represents the **Minimum Viable Product (MVP)** for Frontier's REPL interactive mode. This strategic decision prioritizes:

1. **Shipping working functionality** - A simple, reliable REPL that works today
2. **Clear user expectations** - Well-documented behavior users can understand
3. **Avoiding premature optimization** - Not solving problems we don't have yet

**Future Scope Deferred**:

The original vision for persistent workspace (variables, functions, state) represents a **larger scope enhancement** that is explicitly deferred for future work. This includes:

- Persistent workspace tables (originally Phase 3B)
- `/vars` and `/clear` commands for workspace management
- Cross-evaluation variable persistence
- Function definition persistence

**Rationale**:

1. **MVP delivers value immediately** - Users can evaluate UserTalk, test code, debug issues
2. **Complexity vs benefit tradeoff** - Workspace persistence requires fighting Frontier's thread lifecycle
3. **Future-proof architecture** - Thread-local infrastructure is in place if/when we revisit workspace persistence
4. **Learn from usage** - Real user feedback will guide whether workspace persistence is actually needed

**Decision**: Ship QuickScript MVP now. Revisit workspace persistence only if users demonstrate clear need and we have capacity for the architectural complexity it requires.

This approach follows the principle: "Solve real problems, not hypothetical ones."

---

## Document History

- **2026-04-28**: Added reader's note clarifying /clear and /vars are removed by design (post-#569 cleanup); body of ADR is historical context for the abandoned workaround direction
- **2026-01-15**: Added QuickScript MVP scope clarification (PR #310 bot feedback response)
- **2026-01-15**: Update with QuickScript model decision (PR #304 merged)
- **2026-01-14**: Initial draft (Phase 3A thread-local migration + Phase 3B documentation)
- **Future**: Update with Phase 6+ implementation details when refactoring begins
