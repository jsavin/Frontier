# Parameter State Thread-Safety Migration - Quick Reference

**Date**: 2026-01-02
**Issue**: `flnextparamislast` global persistence bug + thread-safety for collaborative ODB
**Solution**: ADR-005 - Thread-Local Storage Migration
**Status**: Documented, ready for implementation

---

## Problem Summary

### Current Bug
`flnextparamislast = true` set in one verb call persists to next verb call, causing "too many parameters" errors.

**Example**:
```c
// file.open() with 1 param (2 allowed)
flnextparamislast = true;  // Mark 2nd param as optional-last
getstringvalue(hparam1, 2, filename);  // Skips because only 1 param

// Next verb call (unrelated)
// getparam() sees stale flag → "too many parameters" error!
```

### Strategic Issue
Global mutable state violates thread-safety requirement for collaborative ODB (Phase 6+). Multiple users executing scripts concurrently would corrupt each other's parameter state.

---

## Recommended Solution: Thread-Local Storage

**Pattern**: Migrate all parameter-related globals to `tythreadglobals` structure (ADR-005)

**Why This Solution**:
- ✅ True thread-safety for collaborative ODB
- ✅ Proven pattern (flscriptrunning, outlinedata already use this)
- ✅ Zero API changes (backward-compatible macros)
- ✅ Minimal code changes (3 files touched)
- ✅ Template for future global migrations

---

## Globals to Migrate (9 total)

**From Common/source/langvalue.c**:
1. `flnextparamislast` - Parameter flag state
2. `flparamerrorenabled` - Error control
3. `flcoerceexternaltostring` - Type coercion control
4. `flinhibitnilcoercion` - Type coercion control
5. `fllocaldotparamsonly` - Lookup control (static)
6. `bsfunctionname` - Error message state
7. `functiontoken` - Error message state (static)

**From Common/headers/lang.h**:
8. `fllanghashassignprotect` - Hash assignment guard
9. `fllangexternalvalueprotect` - External value guard

---

## Implementation Checklist

### 1. Add Fields to tythreadglobals
**File**: `Common/headers/processinternal.h` (around line 190)

```c
typedef struct tythreadglobals {
    // ... existing fields ...

    // ADR-005: Parameter handling state (Phase 3 migration)
    boolean flnextparamislast;
    boolean flparamerrorenabled;
    boolean flcoerceexternaltostring;
    boolean flinhibitnilcoercion;
    boolean fllocaldotparamsonly;
    bigstring bsfunctionname;
    tyfunctype functiontoken;

    // ADR-005: Value protection flags (Phase 3 migration)
    boolean fllanghashassignprotect;
    boolean fllangexternalvalueprotect;

    // Reserved for future parameter state (Phase 6+)
    void *param_reserved[4];

} tythreadglobals;
```

---

### 2. Update copythreadglobals()
**File**: `Common/source/process.c` (around line 1503)

```c
void copythreadglobals (hdlthreadglobals hglobals) {
    register hdlthreadglobals hg = hglobals;

    // ... existing code ...

    // ADR-005: Parameter state migration
    (**hg).flnextparamislast = flnextparamislast;
    (**hg).flparamerrorenabled = flparamerrorenabled;
    (**hg).flcoerceexternaltostring = flcoerceexternaltostring;
    (**hg).flinhibitnilcoercion = flinhibitnilcoercion;
    (**hg).fllocaldotparamsonly = fllocaldotparamsonly;
    moveleft(bsfunctionname, (**hg).bsfunctionname, sizeof(bigstring));
    (**hg).functiontoken = functiontoken;
    (**hg).fllanghashassignprotect = fllanghashassignprotect;
    (**hg).fllangexternalvalueprotect = fllangexternalvalueprotect;
}
```

---

### 3. Update swapinthreadglobals()
**File**: `Common/source/process.c` (around line 1620)

```c
void swapinthreadglobals (hdlthreadglobals hglobals) {
    register hdlthreadglobals hg = hglobals;

    // ... existing code ...

    // ADR-005: Parameter state migration
    flnextparamislast = (**hg).flnextparamislast;
    flparamerrorenabled = (**hg).flparamerrorenabled;
    flcoerceexternaltostring = (**hg).flcoerceexternaltostring;
    flinhibitnilcoercion = (**hg).flinhibitnilcoercion;
    fllocaldotparamsonly = (**hg).fllocaldotparamsonly;
    moveleft((**hg).bsfunctionname, bsfunctionname, sizeof(bigstring));
    functiontoken = (**hg).functiontoken;
    fllanghashassignprotect = (**hg).fllanghashassignprotect;
    fllangexternalvalueprotect = (**hg).fllangexternalvalueprotect;
}
```

---

### 4. Update newthreadglobals()
**File**: `Common/source/process.c` (find initialization section)

```c
boolean newthreadglobals (hdlthreadglobals *hglobals) {
    register hdlthreadglobals hg;

    // ... allocate and clear handle ...

    // ADR-005: Parameter state initialization
    (**hg).flnextparamislast = false;
    (**hg).flparamerrorenabled = true;
    (**hg).flcoerceexternaltostring = false;
    (**hg).flinhibitnilcoercion = false;
    (**hg).fllocaldotparamsonly = false;
    setemptystring((**hg).bsfunctionname);
    (**hg).functiontoken = 0;  // or appropriate default
    (**hg).fllanghashassignprotect = false;
    (**hg).fllangexternalvalueprotect = false;

    return (true);
}
```

---

### 5. Replace Global Declarations with Macros
**File**: `Common/source/langvalue.c` (lines 77-85)

**BEFORE**:
```c
boolean flparamerrorenabled = true;
boolean flnextparamislast = false;
bigstring bsfunctionname;
boolean flcoerceexternaltostring = false;
boolean flinhibitnilcoercion = false;
static boolean fllocaldotparamsonly = false;
static tyfunctype functiontoken;
```

**AFTER**:
```c
// ADR-005: Thread-local parameter state (macros expand to thread globals)
// Legacy declarations removed, now accessed via tythreadglobals
// See Common/headers/lang.h for macro definitions
```

---

### 6. Add Macro Definitions to Header
**File**: `Common/headers/lang.h` (around line 723)

**BEFORE**:
```c
extern boolean flnextparamislast;
```

**AFTER**:
```c
// ADR-005: Thread-local parameter state (backward-compatible macros)
#define flnextparamislast ((**hthreadglobals).flnextparamislast)
#define flparamerrorenabled ((**hthreadglobals).flparamerrorenabled)
#define flcoerceexternaltostring ((**hthreadglobals).flcoerceexternaltostring)
#define flinhibitnilcoercion ((**hthreadglobals).flinhibitnilcoercion)
#define fllocaldotparamsonly ((**hthreadglobals).fllocaldotparamsonly)
#define bsfunctionname ((**hthreadglobals).bsfunctionname)
#define functiontoken ((**hthreadglobals).functiontoken)
#define fllanghashassignprotect ((**hthreadglobals).fllanghashassignprotect)
#define fllangexternalvalueprotect ((**hthreadglobals).fllangexternalvalueprotect)
```

---

### 7. Remove Band-Aid Fix
**File**: `portable/fileverbs_portable.c` (line 243)

**BEFORE**:
```c
boolean portable_filefunctionvalue(...) {
    flnextparamislast = false;  // Band-aid reset
    // ... rest of function
}
```

**AFTER**:
```c
boolean portable_filefunctionvalue(...) {
    // ADR-005: Band-aid no longer needed (flag is thread-local)
    // ... rest of function
}
```

---

## Testing Strategy

### Test 1: Backward Compatibility
**Goal**: Verify existing verb calls work without modification

```bash
./tools/run_headless_tests.sh
# All tests should pass (no regressions)
```

### Test 2: Optional Parameter Handling
**Goal**: Verify `flnextparamislast` flag works correctly

```bash
# Test file.open with 1 param (2 allowed)
./frontier-cli/frontier-cli -e 'file.open("test.txt")'
# Should succeed, not "too many parameters" error

# Test subsequent verb call doesn't see stale flag
./frontier-cli/frontier-cli -e 'file.open("test.txt"); string.lower("TEST")'
# Should succeed
```

### Test 3: Thread Isolation (Manual)
**Goal**: Verify threads don't see each other's flag state

```c
// Create test that:
// 1. Thread 1 sets flnextparamislast = true
// 2. Thread 2 checks flag (should be false, not true)
// 3. Thread 1 still sees true after Thread 2 runs
```

### Test 4: Error Messages
**Goal**: Verify `bsfunctionname` works for error reporting

```bash
# Trigger parameter error in known verb
./frontier-cli/frontier-cli -e 'string.lower()'
# Error should say "string.lower" not wrong function name
```

---

## Success Criteria

- [ ] All 9 globals migrated to `tythreadglobals`
- [ ] Zero direct references to global variables (only macro usage)
- [ ] Thread swap functions updated (copy, swap, new)
- [ ] Macro definitions added to lang.h
- [ ] Band-aid fix removed from fileverbs_portable.c
- [ ] Full test suite passes (`./tools/run_headless_tests.sh`)
- [ ] Optional parameter verbs work correctly
- [ ] Error messages show correct function names
- [ ] No compilation warnings or errors

---

## Files Modified (Summary)

1. `Common/headers/processinternal.h` - Add fields to tythreadglobals
2. `Common/source/process.c` - Update 3 functions (copy, swap, new)
3. `Common/source/langvalue.c` - Remove global declarations, add comment
4. `Common/headers/lang.h` - Replace extern with macros
5. `portable/fileverbs_portable.c` - Remove band-aid fix (optional)

**Total Changes**: ~100 lines across 5 files

---

## Documentation Created

1. **ADR-005**: `planning/architectural_decision_records/ADR-005-parameter-state-thread-safety.md`
   - Full architectural decision record
   - Rationale, alternatives considered, consequences
   - Code examples and migration plan

2. **Pattern Guide**: `docs/THREAD_LOCAL_GLOBALS_PATTERN.md`
   - Step-by-step template for future globals
   - Common pitfalls and troubleshooting
   - Complete migration checklist

3. **Global Audit**: `planning/phase3/GLOBAL_STATE_AUDIT.md`
   - Comprehensive audit of ALL globals in Frontier
   - Categorization (thread-local, shared, read-only)
   - Migration priority and timeline

4. **CLAUDE.md Update**:
   - Added ADR-005 reference to "BURN THE GLOBALS" section
   - Documented Pattern 1 (thread-local) vs Pattern 2 (explicit context)

---

## Next Steps After Implementation

### Immediate Follow-up (Phase 4)
1. Audit static buffers in verb processors (48+ files)
2. Identify cache/lookup table globals
3. Add mutex to `processthreadlist` (shared state)

### Phase 5 Validation
1. Stress test with concurrent script execution
2. Thread sanitizer clean (no race conditions)
3. Document thread-safety guarantees

### Phase 6 Collaborative ODB
1. Verify thread-safety sufficient for multi-user
2. Add per-user context (user_id, session_id)
3. Enable Google Docs-style collaborative editing

---

## References

- **ADR-005**: Full architectural decision record
- **docs/THREAD_LOCAL_GLOBALS_PATTERN.md**: Migration template
- **planning/phase3/GLOBAL_STATE_AUDIT.md**: Comprehensive audit
- **CLAUDE.md**: Strategic context ("BURN THE GLOBALS WITH FIRE")

---

## Questions?

Consult:
1. ADR-005 for architectural rationale
2. THREAD_LOCAL_GLOBALS_PATTERN.md for how-to guide
3. GLOBAL_STATE_AUDIT.md for related globals

This migration establishes the pattern for all future global migrations during the "BURN THE GLOBALS WITH FIRE" refactoring.
