# ADR-005: Parameter State Thread-Safety Architecture

**Status**: `[IN PROGRESS]` - Pattern established, foundation for ongoing global elimination (used by ADR-006, ADR-009)
**Date**: 2026-01-02
**Author**: System Architect
**Relates to**: Collaborative ODB (Phase 6+)

## Executive Summary

Frontier's verb parameter handling uses a global mutable flag (`flnextparamislast`) that creates race conditions in multi-threaded environments and causes bugs even in single-threaded code. This ADR establishes the pattern for migrating ALL parameter-related globals to thread-local storage, creating a foundation for collaborative ODB editing where multiple users execute scripts concurrently.

**Recommended Solution**: **Option 1 - Thread-Local Storage Migration** using existing `tythreadglobals` infrastructure.

## Context

### The Problem

**Current Bug**: When verb processors set `flnextparamislast = true` but don't consume all parameters (e.g., `file.open()` with 1 param when 2 are allowed), the flag persists to the next verb call, causing spurious "too many parameters" errors.

**Root Cause**: `flnextparamislast` is a global variable shared across ALL verb calls:

```c
// Common/source/langvalue.c:79
boolean flnextparamislast = false;

// Verb processors set the flag:
flnextparamislast = true;  // Signal next param is last
getstringvalue(hparam1, 2, filename);  // If this fails/skips, flag persists!

// Next verb call sees stale flag:
// langvalue.c:4312 - getparam() captures and resets
boolean fllastparam = flnextparamislast;
flnextparamislast = false; // Too late - already caused error
```

**Impact**:
- Affects 48+ files: all verb processors (string, table, file, sys, math, date, etc.)
- Breaks legitimate 1-param calls after optional-param verb calls
- Will cause race conditions in multi-threaded collaborative ODB
- Band-aid fix (reset at processor entry) doesn't scale

### Related Globals

All of these are thread-safety violations for collaborative ODB:

```c
// Common/source/langvalue.c
boolean flnextparamislast = false;        // ❌ Parameter flag state
boolean flparamerrorenabled = true;       // ❌ Error control
boolean flcoerceexternaltostring = false; // ❌ Coercion control
boolean flinhibitnilcoercion = false;     // ❌ Coercion control
static boolean fllocaldotparamsonly = false; // ❌ Lookup control
bigstring bsfunctionname;                 // ❌ Error message state

// Common/headers/lang.h (other globals)
extern boolean flscriptrunning;           // ✅ Already in tythreadglobals
extern boolean flscriptresting;           // ✅ Already in tythreadglobals
extern boolean fllanghashassignprotect;   // ❌ Needs migration
extern boolean fllangexternalvalueprotect; // ❌ Needs migration
```

### Strategic Requirements

**North Star: Collaborative ODB Editing (Phase 6+)**

From CLAUDE.md:
> Frontier should support Google Docs/Sheets-style collaborative editing of ODB objects:
> - Multiple users edit different ODB objects simultaneously
> - Developers write functionally single-threaded code (no concurrency awareness required)
> - Runtime handles all concurrency, locking, and conflict resolution transparently

**Launch Requirement**: Thread-safety is non-negotiable for Automattic partnership and multi-user 2.0.

## Architectural Options

### Option 1: Thread-Local Storage Migration (RECOMMENDED)

**Description**: Migrate parameter-related globals into existing `tythreadglobals` structure (`Common/headers/processinternal.h`).

**Design**:

```c
// Common/headers/processinternal.h (ADD to tythreadglobals)
typedef struct tythreadglobals {
    // ... existing fields ...

    // Parameter handling state (Phase 3 migration)
    boolean flnextparamislast;
    boolean flparamerrorenabled;
    boolean flcoerceexternaltostring;
    boolean flinhibitnilcoercion;
    boolean fllocaldotparamsonly;
    bigstring bsfunctionname;

    // Protection flags (Phase 3 migration)
    boolean fllanghashassignprotect;
    boolean fllangexternalvalueprotect;

    // Reserved for future parameter state (Phase 6+)
    void *param_reserved[4];

} tythreadglobals;
```

**Implementation Pattern**:

```c
// 1. Add getter/setter macros for backward compatibility
#define flnextparamislast ((**hthreadglobals).flnextparamislast)
#define flparamerrorenabled ((**hthreadglobals).flparamerrorenabled)
// ... etc for all migrated globals

// 2. Update copythreadglobals() and swapinthreadglobals()
void copythreadglobals (hdlthreadglobals hglobals) {
    // ... existing code ...

    (**hg).flnextparamislast = flnextparamislast;
    (**hg).flparamerrorenabled = flparamerrorenabled;
    (**hg).flcoerceexternaltostring = flcoerceexternaltostring;
    (**hg).flinhibitnilcoercion = flinhibitnilcoercion;
    (**hg).fllocaldotparamsonly = fllocaldotparamsonly;
    moveleft(bsfunctionname, (**hg).bsfunctionname, sizeof(bigstring));
    (**hg).fllanghashassignprotect = fllanghashassignprotect;
    (**hg).fllangexternalvalueprotect = fllangexternalvalueprotect;
}

void swapinthreadglobals (hdlthreadglobals hglobals) {
    // ... existing code ...

    flnextparamislast = (**hg).flnextparamislast;
    flparamerrorenabled = (**hg).flparamerrorenabled;
    flcoerceexternaltostring = (**hg).flcoerceexternaltostring;
    flinhibitnilcoercion = (**hg).flinhibitnilcoercion;
    fllocaldotparamsonly = (**hg).fllocaldotparamsonly;
    moveleft((**hg).bsfunctionname, bsfunctionname, sizeof(bigstring));
    fllanghashassignprotect = (**hg).fllanghashassignprotect;
    fllangexternalvalueprotect = (**hg).fllangexternalvalueprotect;
}

// 3. Initialize new fields in newthreadglobals()
boolean newthreadglobals (hdlthreadglobals *hglobals) {
    // ... existing code ...

    (**hg).flnextparamislast = false;
    (**hg).flparamerrorenabled = true;
    (**hg).flcoerceexternaltostring = false;
    (**hg).flinhibitnilcoercion = false;
    (**hg).fllocaldotparamsonly = false;
    setemptystring((**hg).bsfunctionname);
    (**hg).fllanghashassignprotect = false;
    (**hg).fllangexternalvalueprotect = false;

    return true;
}
```

**Strengths**:
- ✅ Leverages existing, proven thread-local infrastructure
- ✅ Zero API changes - macros provide transparent backward compatibility
- ✅ Automatic thread safety for collaborative ODB
- ✅ Pattern already used for `flscriptrunning`, `flscriptresting` (lines 1456-1458 in process.c)
- ✅ Minimal code churn - only touch 3 files (processinternal.h, process.c, langvalue.c)
- ✅ Works in both legacy Mac builds and headless/portable builds

**Weaknesses**:
- ⚠️ Requires careful testing to ensure thread swap functions work correctly
- ⚠️ Adds ~40 bytes to thread globals structure (acceptable overhead)
- ⚠️ Doesn't help if code runs without thread context (but that's already broken for collaborative ODB)

**Migration Complexity**: **LOW** - Single PR, ~100 lines of code changes

---

### Option 2: Explicit Context Parameter

**Description**: Add `lang_context_t *ctx` parameter to ALL verb callback signatures and thread context through parameter chain.

**Design**:

```c
// Common/headers/lang.h
typedef struct lang_context_t {
    boolean flnextparamislast;
    boolean flparamerrorenabled;
    // ... all parameter state ...
} lang_context_t;

// Verb signature changes from:
boolean filefunctionvalue(short token, hdltreenode hparam1, tyvaluerecord *vreturned);

// To:
boolean filefunctionvalue(lang_context_t *ctx, short token, hdltreenode hparam1, tyvaluerecord *vreturned);
```

**Strengths**:
- ✅ Explicit, type-safe context passing
- ✅ No reliance on global state at all
- ✅ Easy to reason about parameter flow

**Weaknesses**:
- ❌ MASSIVE API break - changes every verb processor signature
- ❌ Requires updating 48+ files with verb callbacks
- ❌ Breaks backward compatibility with existing verb implementations
- ❌ `db_context` pattern already proven as overkill for this use case
- ❌ Doesn't solve thread-local needs for other globals (flscriptrunning, etc.)

**Migration Complexity**: **VERY HIGH** - Multiple PRs, 5000+ lines of code changes

---

### Option 3: Central Reset in Dispatcher

**Description**: Reset `flnextparamislast` at single point in `langfunctionvalue()` (verb dispatcher) before calling each verb processor.

**Design**:

```c
// Common/source/langvalue.c - langfunctionvalue()
boolean langfunctionvalue(hdltreenode hparam1, tyvaluerecord *vreturned) {
    // ALWAYS reset flag before calling ANY verb processor
    flnextparamislast = false;

    // Dispatch to verb processor...
    switch (token) {
        case filetoken:
            return filefunctionvalue(token, hparam1, vreturned);
        // ... etc
    }
}
```

**Strengths**:
- ✅ Extremely simple - single line of code
- ✅ Fixes current bug immediately
- ✅ Zero API changes

**Weaknesses**:
- ❌ Doesn't solve thread-safety for collaborative ODB
- ❌ Still relies on global mutable state
- ❌ Doesn't address other parameter-related globals
- ❌ Race conditions in multi-threaded environment (Phase 6+)
- ❌ Not a long-term architectural solution

**Migration Complexity**: **TRIVIAL** - But not acceptable for Phase 6+ requirements

---

### Option 4: Per-Processor Reset (Current Band-Aid)

**Description**: Add `flnextparamislast = false` at the start of EVERY verb processor function.

**Current Implementation**:
```c
// portable/fileverbs_portable.c:243
boolean portable_filefunctionvalue(...) {
    flnextparamislast = false;  // Reset flag on entry
    // ... rest of function
}
```

**Strengths**:
- ✅ Simple to implement
- ✅ Fixes immediate bug
- ✅ Defensive coding

**Weaknesses**:
- ❌ Doesn't solve thread-safety
- ❌ Requires updating 48+ files
- ❌ Easy to forget in new verb processors
- ❌ Doesn't address architectural problem

**Migration Complexity**: **MEDIUM** - But wrong direction architecturally

---

## Decision

**Adopt Option 1: Thread-Local Storage Migration**

### Rationale

1. **Thread-Safety for Collaborative ODB**: Only Option 1 provides true thread-safety required for Phase 6+ multi-user editing.

2. **Proven Pattern**: The `tythreadglobals` infrastructure already manages similar globals (`flscriptrunning`, `flscriptresting`, `outlinedata`) successfully. This is a natural extension, not new architecture.

3. **Backward Compatibility**: Macro-based accessors mean ZERO changes to existing verb processor code. The 48+ files that use these globals continue working without modification.

4. **Minimal Risk**: Changes isolated to 3 files (processinternal.h, process.c, langvalue.c). Existing thread swap logic is well-tested.

5. **Scalability**: This pattern applies to ALL globals we'll encounter during "BURN THE GLOBALS WITH FIRE" refactoring (see CLAUDE.md). Establishes template for future work.

6. **Performance**: ~40 bytes per thread is negligible overhead. Thread swaps are already expensive operations (copying entire context), adding a few more booleans doesn't change performance profile.

### Why Not Other Options

- **Option 2 (Explicit Context)**: Too invasive for the value gained. Thread-local globals work fine for per-thread state. Explicit context makes sense for per-operation state (like `db_context`), not per-thread state.

- **Option 3 (Central Reset)**: Doesn't solve the strategic problem (thread-safety). Would need to be ripped out in Phase 6+ anyway.

- **Option 4 (Per-Processor Reset)**: Also doesn't solve thread-safety, and requires more code changes than Option 1.

## Implementation Plan

### Phase 1: Foundation (Immediate - This PR)

**Files Modified**:
1. `Common/headers/processinternal.h` - Add fields to `tythreadglobals`
2. `Common/source/process.c` - Update swap functions and newthreadglobals()
3. `Common/source/langvalue.c` - Change declarations from `boolean` to macros
4. `Common/headers/lang.h` - Change extern declarations to macros

**Testing Strategy**:
1. Run full headless test suite (`./tools/run_headless_tests.sh`)
2. Test verb parameter handling specifically:
   - Optional parameters (file.open with 1 vs 2 params)
   - Required last parameter enforcement
   - Error messages with correct function names
3. Verify no regression in single-threaded behavior
4. Document thread-safety guarantees in comments

**Success Criteria**:
- ✅ All existing tests pass
- ✅ No direct references to global `flnextparamislast` (only macro usage)
- ✅ Thread swap functions preserve parameter state correctly
- ✅ Zero API changes visible to verb processor implementations

### Phase 2: Additional Globals (Follow-up PR)

Migrate remaining parameter-related globals using same pattern:
- `fllanghashassignprotect`
- `fllangexternalvalueprotect`
- Any other globals discovered during "BURN THE GLOBALS" audit

### Phase 3: Documentation & Pattern Reuse

**Create Documentation**:
1. Update this ADR with "Implemented" status and lessons learned
2. Create `docs/THREAD_LOCAL_GLOBALS_PATTERN.md` - Template for future migrations
3. Update CLAUDE.md with reference to this ADR

**Pattern Template** for future globals:

```markdown
# Migrating Globals to Thread-Local Storage

## When to Use This Pattern
- Global affects per-thread execution state (not shared data)
- Global causes bugs due to cross-call contamination
- Global would create race conditions in multi-threaded environment

## Steps
1. Add field to tythreadglobals (processinternal.h)
2. Update copythreadglobals() to save state
3. Update swapinthreadglobals() to restore state
4. Update newthreadglobals() to initialize
5. Replace global declaration with macro accessor
6. Test thread swap behavior

## See ADR-005 for reference implementation
```

### Phase 6+: Verification for Collaborative ODB

When implementing multi-user editing:
1. Verify ALL parameter-related globals are thread-local
2. Add stress tests with concurrent verb execution
3. Document thread-safety guarantees for verb implementers
4. Consider if additional context fields needed (e.g., user_id, session_id)

## Code Changes

### Common/headers/processinternal.h

```c
typedef struct tythreadglobals {
    // ... existing fields (line ~86) ...

    // Parameter handling state (ADR-005: Phase 3 migration)
    boolean flnextparamislast;
    boolean flparamerrorenabled;
    boolean flcoerceexternaltostring;
    boolean flinhibitnilcoercion;
    boolean fllocaldotparamsonly;
    bigstring bsfunctionname;

    // Value protection flags (ADR-005: Phase 3 migration)
    boolean fllanghashassignprotect;
    boolean fllangexternalvalueprotect;

    // Reserved for future parameter state (Phase 6+)
    void *param_reserved[4];

} tythreadglobals;
```

### Common/source/process.c

Add to `copythreadglobals()` (around line 1503):
```c
// ADR-005: Parameter state migration
(**hg).flnextparamislast = flnextparamislast;
(**hg).flparamerrorenabled = flparamerrorenabled;
(**hg).flcoerceexternaltostring = flcoerceexternaltostring;
(**hg).flinhibitnilcoercion = flinhibitnilcoercion;
(**hg).fllocaldotparamsonly = fllocaldotparamsonly;
moveleft(bsfunctionname, (**hg).bsfunctionname, sizeof(bigstring));
(**hg).fllanghashassignprotect = fllanghashassignprotect;
(**hg).fllangexternalvalueprotect = fllangexternalvalueprotect;
```

Add to `swapinthreadglobals()` (around line 1620):
```c
// ADR-005: Parameter state migration
flnextparamislast = (**hg).flnextparamislast;
flparamerrorenabled = (**hg).flparamerrorenabled;
flcoerceexternaltostring = (**hg).flcoerceexternaltostring;
flinhibitnilcoercion = (**hg).flinhibitnilcoercion;
fllocaldotparamsonly = (**hg).fllocaldotparamsonly;
moveleft((**hg).bsfunctionname, bsfunctionname, sizeof(bigstring));
fllanghashassignprotect = (**hg).fllanghashassignprotect;
fllangexternalvalueprotect = (**hg).fllangexternalvalueprotect;
```

Add to `newthreadglobals()` (find the initialization section):
```c
// ADR-005: Parameter state initialization
(**hg).flnextparamislast = false;
(**hg).flparamerrorenabled = true;
(**hg).flcoerceexternaltostring = false;
(**hg).flinhibitnilcoercion = false;
(**hg).fllocaldotparamsonly = false;
setemptystring((**hg).bsfunctionname);
(**hg).fllanghashassignprotect = false;
(**hg).fllangexternalvalueprotect = false;
```

### Common/source/langvalue.c

Replace global declarations (lines 77-85):
```c
// ADR-005: Thread-local parameter state (macros expand to thread globals)
// Legacy declarations removed, now accessed via tythreadglobals

// Note: These macros provide backward-compatible access to thread-local storage
// #define flparamerrorenabled ((**hthreadglobals).flparamerrorenabled)
// #define flnextparamislast ((**hthreadglobals).flnextparamislast)
// ... etc (defined in lang.h)
```

### Common/headers/lang.h

Replace extern declarations (around line 723) with macros:
```c
// ADR-005: Thread-local parameter state (backward-compatible macros)
#define flnextparamislast ((**hthreadglobals).flnextparamislast)
#define flparamerrorenabled ((**hthreadglobals).flparamerrorenabled)
#define flcoerceexternaltostring ((**hthreadglobals).flcoerceexternaltostring)
#define flinhibitnilcoercion ((**hthreadglobals).flinhibitnilcoercion)
#define fllocaldotparamsonly ((**hthreadglobals).fllocaldotparamsonly)
#define bsfunctionname ((**hthreadglobals).bsfunctionname)
#define fllanghashassignprotect ((**hthreadglobals).fllanghashassignprotect)
#define fllangexternalvalueprotect ((**hthreadglobals).fllangexternalvalueprotect)
```

## Consequences

### Positive

1. **Thread-Safety Achieved**: Multi-user collaborative ODB (Phase 6+) can execute verb calls concurrently without race conditions on parameter state.

2. **Bug Fixed**: The immediate `flnextparamislast` contamination bug is resolved without band-aid per-processor resets.

3. **Pattern Established**: This becomes the template for migrating all other problematic globals during "BURN THE GLOBALS WITH FIRE" refactoring.

4. **Zero API Impact**: Existing verb processors continue working without modification. This is a pure internal refactoring.

5. **Proven Infrastructure**: Uses existing `tythreadglobals` machinery that's been stable for 15+ years.

6. **Maintainability**: Future developers can easily find and understand thread-local state (all in one structure vs scattered globals).

### Negative

1. **Struct Size Growth**: `tythreadglobals` grows by ~40 bytes. Acceptable given thread creation is rare and memory is cheap.

2. **Swap Function Complexity**: `copythreadglobals()` and `swapinthreadglobals()` grow by ~8 lines each. Still manageable.

3. **Macro Indirection**: Accessing `flnextparamislast` now goes through macro → thread globals deref. Negligible performance impact (compiler optimization).

4. **Testing Surface**: Must verify thread swap functions correctly preserve new fields. But this is standard testing for ANY thread-local addition.

### Risks & Mitigation

**Risk 1: Forgot to update swap functions**
- Mitigation: Thread tests will fail immediately if state doesn't persist across swaps
- Testing: Run verb tests in threaded environment (already done in headless mode)

**Risk 2: Code paths without thread context**
- Mitigation: If `hthreadglobals` is nil, system already broken for collaborative ODB
- Testing: Verify all verb calls occur with valid thread context

**Risk 3: Macro conflicts**
- Mitigation: Macro names match existing global names exactly - no conflicts
- Testing: Compilation will fail if macro expansion incorrect

## Alternatives Considered But Rejected

### Thread-Local Storage (C11 `_Thread_local`)

Could use C11 `_Thread_local` keyword instead of manual thread globals:

```c
_Thread_local boolean flnextparamislast = false;
```

**Rejected Because**:
- Frontier targets C89/C99 compatibility for legacy Mac builds
- Would diverge from existing `tythreadglobals` pattern
- Harder to inspect/debug thread state (no central structure)
- Less portable across compilers

### Atomic Operations

Could use C11 atomics to make globals thread-safe:

```c
_Atomic boolean flnextparamislast = false;
```

**Rejected Because**:
- Doesn't solve the fundamental problem (cross-call contamination)
- Atomics useful for shared state, not per-thread state
- Overkill for boolean flags that should be thread-local

## References

### Internal Documentation
- **CLAUDE.md**: "Collaborative ODB Editing - North Star Vision" (lines 207-270)
- **CLAUDE.md**: "BURN THE GLOBALS WITH FIRE" (lines 498-530)
- **ADR: CONTEXT_PATTERN_FOR_ODB_COLLABORATION.md**: Operation context pattern for Phase 6+

### Related Issues
- Issue TBD: `flnextparamislast` persistence bug (to be filed)
- Issue #135: Outline context refactoring (reference pattern for context-based architecture)

### Code References
- `Common/headers/processinternal.h:86` - `tythreadglobals` structure definition
- `Common/source/process.c:1403` - `copythreadglobals()` implementation
- `Common/source/process.c:1523` - `swapinthreadglobals()` implementation
- `Common/source/langvalue.c:79` - Current global declarations
- `Common/source/langvalue.c:4312` - `getparam()` flag consumption

### Pattern Examples in Codebase
- `flscriptrunning` / `flscriptresting` (lines 1456-1458 in process.c) - Already uses this pattern
- `outlinedata` / `outlinestack` (lines 1469-1473 in process.c) - Thread-local outline state

## Success Metrics

### Implementation Complete When:
- [ ] All fields added to `tythreadglobals` structure
- [ ] `copythreadglobals()` preserves new fields
- [ ] `swapinthreadglobals()` restores new fields
- [ ] `newthreadglobals()` initializes new fields
- [ ] Macro definitions replace extern declarations
- [ ] No direct global variable references remain (only macros)
- [ ] Full test suite passes (`./tools/run_headless_tests.sh`)
- [ ] Optional parameter verbs work correctly (file.open, etc.)

### Thread-Safety Verified When (Phase 6+):
- [ ] Concurrent verb execution doesn't corrupt parameter state
- [ ] Each thread's parameter flags isolated from others
- [ ] Stress test: 10+ threads executing verbs simultaneously
- [ ] No race conditions detected by thread sanitizer

## Future Work

### Phase 4-5: Complete Global Migration
- Audit ALL remaining globals in `langvalue.c`, `langinternal.c`, etc.
- Migrate any thread-dependent state to `tythreadglobals`
- Document which globals are truly shared (locks, caches) vs thread-local

### Phase 6: Collaborative ODB Thread Safety
- Stress test with concurrent UserTalk execution
- Add per-user context fields (user_id, session_id) to `tythreadglobals`
- Implement optimistic locking using thread-local version tracking
- Consider if `lang_context_t` needed for per-operation (not per-thread) state

### Phase 7+: Global State Elimination
- Move shared mutable state (caches, buffers) to ref-counted structures
- Replace remaining globals with explicit context passing where appropriate
- Achieve "no mutable globals" milestone for full thread-safety

---

## Appendix: Global State Audit

**Thread-Local Globals (Already Migrated)**:
- ✅ `flscriptrunning` - Script execution flag
- ✅ `flscriptresting` - Script yield flag
- ✅ `outlinedata` - Current outline context
- ✅ `currentprocess` - Current process handle
- ✅ `currenthashtable` - Current scope

**Thread-Local Globals (This ADR - Phase 3)**:
- ⏳ `flnextparamislast` - Parameter flag state
- ⏳ `flparamerrorenabled` - Error control
- ⏳ `flcoerceexternaltostring` - Coercion control
- ⏳ `flinhibitnilcoercion` - Coercion control
- ⏳ `fllocaldotparamsonly` - Lookup control
- ⏳ `bsfunctionname` - Error message state
- ⏳ `fllanghashassignprotect` - Protection flag
- ⏳ `fllangexternalvalueprotect` - Protection flag

**Shared Globals (Legitimate - No Migration Needed)**:
- ✅ `hbuiltinfunctions` - Built-in function table (read-only after init)
- ✅ Database file handles - Managed by `db_context` pattern (see ADR-001, ADR-002)

**Unknown / Future Audit**:
- ❓ Static buffers in verb processors - Need case-by-case analysis
- ❓ Caches and lookup tables - Determine if truly shared or thread-local
