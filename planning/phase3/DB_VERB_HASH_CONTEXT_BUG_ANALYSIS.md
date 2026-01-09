# DB Verb Hash Table Context Management Bug Analysis

**Date:** 2026-01-09
**Status:** Root cause identified, fix strategy proposed
**Severity:** P0 - Blocks db verb integration testing
**Commits Verified:** Pre-existing at 91c242d6 (before ADR-006 accessor refactoring)

## Executive Summary

Sequential db verb execution (`db.new()` followed by `db.open()`) triggers assertion failure in `langevaluate.c:1955`. Root cause: `shellpushdefaultglobals()`/`shellpopglobals()` save/restore shell window state but do NOT save/restore `currenthashtable`, causing hash table context corruption during nested verb execution.

## Symptom

```bash
TMPDIR=$(./tools/get_test_temp_path.sh)
./frontier-cli/frontier-cli -e "db.new(\"$TMPDIR/test.root\"); return db.open(\"$TMPDIR/test.root\", false)"

# Output:
Assertion failed: (hlocals == currenthashtable), function evaluatelist, line 1955.
Abort trap: 6
```

## Root Cause Analysis

### Call Flow

1. **Script Execution Begins**
   - `evaluatelist()` in `langevaluate.c` captures `hlocals = currenthashtable`
   - This establishes the expected hash table context for the script execution

2. **First Verb: `db.new()`**
   - Calls `dbnewverb()` → `shellpushdefaultglobals()`
   - **BUG**: `shellpushdefaultglobals()` saves shell window state but NOT `currenthashtable`
   - Calls `opennewfile()`, `odbnewfile()` (ODB operations)
   - Calls `shellpopglobals()`
   - **BUG**: Pop restores shell window, but `currenthashtable` may now differ from saved state

3. **Second Verb: `db.open()`**
   - Calls `dbopenverb()` → No push/pop (just direct ODB calls)
   - Returns to `evaluatelist()`

4. **Assertion Failure**
   - `evaluatelist()` checks `if (hlocals != currenthashtable)` at line 1953
   - **FAILURE**: `currenthashtable` was modified by nested operations but not restored
   - Assertion triggers: `assert (hlocals == currenthashtable);`

### Critical Code Locations

**Crash Site** (`Common/source/langevaluate.c:1953-1962`):
```c
if (hlocals != currenthashtable) { /*should never happen*/

    assert (hlocals == currenthashtable); /*context change in background destroyed our state*/

    langerror (undefinederror);

    currenthashtable = hlocals;

    fl = false;
}
```

**Shell Globals Push/Pop** (`Common/source/shellcallbacks.c`):
```c
boolean shellpushdefaultglobals (void) {
    return (shellpushconfigglobals (iddefaultconfig));
}

boolean shellpushconfigglobals (short configresnum) {
    // Saves shell globals (windowholder, dataholder, config)
    if (!shellpushglobals (nil)) /*save off old state*/
        return (false);
    shellglobals = globalsarray [ix];
    config = shellglobals.config;
    return (true);
}

boolean shellpopglobals (void) {
    // Restores shell window from stack
    shellsetglobals (globalsstack.stack [--globalsstack.top]);
    // BUT: Does NOT restore currenthashtable!
    return (true);
}
```

**DB Verb Implementation** (`Common/source/dbverbs.c:552-589`):
```c
static boolean dbnewverb (hdltreenode hparam1, tyvaluerecord *vreturned) {
    tyodbrecord odbrec;
    boolean fl;

    flnextparamislast = true;
    if (!getfilespecvalue (hparam1, 1, &odbrec.fs))
        return (false);

    shellpushdefaultglobals (); // BUG: Doesn't save currenthashtable

    fl = opennewfile (&odbrec.fs, config.filecreator, config.filetype, &odbrec.fref);

    shellpopglobals (); // BUG: Doesn't restore currenthashtable

    if (!fl) return (false);

    fl = odbnewfile (odbrec.fref);
    closefile (odbrec.fref);

    if (odberror (fl)) {
        deletefile (&odbrec.fs);
        return (false);
    }

    return (setbooleanvalue (true, vreturned));
}
```

**Hash Table Chain Management** (`Common/source/langhash.c:946-978`):
```c
void chainhashtable (hdlhashtable htable) {
    // Pushes new table onto currenthashtable stack
    (**htable).prevhashtable = currenthashtable;
    (**htable).flchained = true;
    currenthashtable = htable;
}

void unchainhashtable (void) {
    // Pops currenthashtable stack
    hdlhashtable ht = currenthashtable;
    hdlhashtable hprev = (**ht).prevhashtable;
    (**ht).prevhashtable = nil;
    (**ht).flchained = false;
    currenthashtable = hprev;
}
```

## Why This Happens

### The Mismatch

`shellpushdefaultglobals()` is designed to:
- Save current shell window state (WindowPtr, data handles, config)
- Switch to default config context (for file operations that need specific file creator codes)
- Allow nested operations to run with correct config settings

BUT: It assumes `currenthashtable` won't change during nested operations.

### The Reality

During nested operations:
- ODB operations may trigger background tasks (`langbackgroundtask()`)
- Background tasks may push/pop local hash table contexts
- Script evaluation may chain/unchain hash tables for local variables
- Any of these can modify `currenthashtable` global

### Historical Context

From the crash site comment (line 1945):
```c
/*
1/31/97 dmb: below is the site of a major osamenusharing bug. It can fail!
I've seen it myself. But it's also been reported by Timothy Paustian
<paustian@bact.wisc.edu> [About the Frontier menu in Web Warrior], Tattoo Mabonzo K.
<vip052@pophost.eunet.be> [db.save verb] and at least one more person.

5.0b18 dmb: if this does trigger, make some attempt to exit cleanly
*/
```

**This is a known, longstanding bug in the original Frontier codebase.**

## Global Mutable State Pattern

This bug is a **textbook example** of why global mutable state is dangerous:

1. **Global Variable**: `currenthashtable` is a global that tracks the current hash table context
2. **Non-Local Modification**: Nested operations can modify it without caller knowledge
3. **No Atomicity**: No mechanism to save/restore state across nested calls
4. **Fragile Assumptions**: Code assumes globals won't change during certain operations

## Fix Strategy

### Option 1: Save/Restore currenthashtable in Shell Push/Pop ✅ **RECOMMENDED**

**Approach**: Extend `shellpushglobals()`/`shellpopglobals()` to save/restore `currenthashtable`.

**Implementation**:
```c
// In shellcallbacks.c
typedef struct {
    WindowPtr window;
    hdlhashtable hashtable;  // NEW: Save hash table context
} tyglobalsstackentry;

boolean shellpushglobals (WindowPtr wpush) {
    if (globalsstack.top >= ctglobals) {
        shellinternalerror (idglobalsstackfull, STR_globals_stack_overflow);
        return (false);
    }

    // Save both window AND hash table context
    globalsstack.stack [globalsstack.top].window = shellwindow;
    globalsstack.stack [globalsstack.top].hashtable = currenthashtable;  // NEW
    globalsstack.top++;

    if (getwindowinfo (wpush, &hinfo))
        (**hinfo).ctpushes++;

    if (shellsetglobals (wpush))
        (*shellglobals.pushroutine) ();

    return (true);
}

boolean shellpopglobals (void) {
    WindowPtr w = shellwindow;
    hdlwindowinfo hinfo;

    if (globalsstack.top <= 0) {
        shellsetglobals (nil);
        return (false);
    }

    if (shellwindow != nil) {
        (*shellglobals.poproutine) ();
        *shellglobals.dataholder = nil;
        *shellglobals.infoholder = nil;
        (*shellglobals.setglobalsroutine) ();
    }

    // Restore BOTH window and hash table context
    globalsstack.top--;
    shellsetglobals (globalsstack.stack[globalsstack.top].window);
    currenthashtable = globalsstack.stack[globalsstack.top].hashtable;  // NEW

    if (getwindowinfo (w, &hinfo)) {
        (**hinfo).ctpushes--;
        if ((**hinfo).fldisposewhenpopped)
            disposeshellwindow ((**hinfo).macwindow);
    }

    return (true);
}
```

**Pros**:
- ✅ Fixes root cause (shell push/pop doesn't corrupt hash table state)
- ✅ Minimal code changes (only shellcallbacks.c modified)
- ✅ Transparent to callers (no API changes)
- ✅ Fixes ALL verbs that use shellpushdefaultglobals() (not just db verbs)

**Cons**:
- ❌ Increases globalsstack entry size (WindowPtr → struct with 2 fields)
- ❌ Doesn't address broader global mutable state problem

**Risk**: Low - This is a surgical fix to a well-understood problem.

### Option 2: Remove shellpushdefaultglobals() from db.new()

**Approach**: Don't push shell globals at all - just use current config.

**Implementation**:
```c
static boolean dbnewverb (hdltreenode hparam1, tyvaluerecord *vreturned) {
    tyodbrecord odbrec;
    boolean fl;

    flnextparamislast = true;
    if (!getfilespecvalue (hparam1, 1, &odbrec.fs))
        return (false);

    // REMOVED: shellpushdefaultglobals ();

    fl = opennewfile (&odbrec.fs, config.filecreator, config.filetype, &odbrec.fref);

    // REMOVED: shellpopglobals ();

    if (!fl) return (false);

    fl = odbnewfile (odbrec.fref);
    closefile (odbrec.fref);

    if (odberror (fl)) {
        deletefile (&odbrec.fs);
        return (false);
    }

    return (setbooleanvalue (true, vreturned));
}
```

**Pros**:
- ✅ Minimal change (remove 2 lines)
- ✅ No global state manipulation
- ✅ Simpler code

**Cons**:
- ❌ May break file creation if config.filecreator/filetype are wrong
- ❌ Doesn't fix other verbs that use shellpushdefaultglobals()
- ❌ May break Mac GUI (which relies on config switching)

**Risk**: Medium - Need to verify config is correct in headless context.

### Option 3: Thread-Local Hash Table Context (Long-term)

**Approach**: Migrate `currenthashtable` to thread-local storage (like `flnextparamislast` in ADR-005).

**Implementation**:
```c
// In processinternal.h
typedef struct tythreadglobals {
    // ... existing fields ...
    hdlhashtable currenthashtable;  // NEW: Thread-local hash table context
} tythreadglobals;

// In langhash.c - replace global with thread-local accessor
#define currenthashtable (getcurrentthreadglobals()->currenthashtable)
```

**Pros**:
- ✅ Eliminates global mutable state
- ✅ Thread-safe by design
- ✅ Foundation for collaborative ODB editing
- ✅ Follows ADR-005 pattern (proven to work)

**Cons**:
- ❌ Large refactoring (affects many files)
- ❌ Requires careful testing
- ❌ Overkill for immediate bug fix

**Risk**: High for immediate fix, but correct long-term direction.

## Recommended Approach

**SHORT-TERM (IMMEDIATE FIX)**:
- Implement **Option 1**: Save/restore `currenthashtable` in shell push/pop
- This is a surgical fix that addresses the root cause
- Minimal risk, transparent to callers
- Fixes ALL verbs that use `shellpushdefaultglobals()`, not just db verbs

**LONG-TERM (FOUNDATION)**:
- Migrate `currenthashtable` to thread-local storage (Option 3)
- Create ADR documenting the migration (following ADR-005 pattern)
- This is foundational work for collaborative ODB editing (Frontier 2.0)
- Can be done incrementally after immediate fix

## Files to Modify (Option 1)

### Immediate Changes
1. **Common/source/shellcallbacks.c**
   - Change `globalsstack.stack` from `WindowPtr` array to struct array
   - Modify `shellpushglobals()` to save `currenthashtable`
   - Modify `shellpopglobals()` to restore `currenthashtable`

2. **Common/headers/shell.h** (if stack is declared there)
   - Update `tyglobalsstackentry` typedef

### Testing
1. Verify db verb sequence works: `db.new() → db.open()`
2. Run full integration test suite (`cd tests && make test-integration`)
3. Run unit test suite (`./tools/run_headless_tests.sh`)
4. Test other verbs that use `shellpushdefaultglobals()`:
   - File verbs
   - Table verbs
   - Any verb that manipulates config

## Related Issues

- **Issue #135**: Outline context refactoring (similar global state problem)
- **ADR-005**: Parameter state thread-safety (thread-local pattern reference)
- **ADR-006**: Outline push/pop pattern elimination (just completed)

## Historical Bug Reports

From comment in `langevaluate.c:1945`:
- Timothy Paustian <paustian@bact.wisc.edu>: "About the Frontier menu in Web Warrior"
- Tattoo Mabonzo K. <vip052@pophost.eunet.be>: "db.save verb"
- Multiple other reports (unnamed)

**This bug has existed since at least 1997 (Frontier 5.0b18).**

## Next Steps

1. ✅ Document root cause (this file)
2. [ ] Implement Option 1 fix in `shellcallbacks.c`
3. [ ] Test db verb sequence
4. [ ] Run full test suite
5. [ ] Create ADR for long-term thread-local migration
6. [ ] File issue for Option 3 (thread-local `currenthashtable`)

## Conclusion

This is a **well-understood, fixable bug** with a clear root cause. The immediate fix (Option 1) is low-risk and addresses the root cause. The long-term fix (Option 3) aligns with our global state elimination strategy and Frontier 2.0 collaborative ODB vision.

**Recommendation**: Implement Option 1 immediately to unblock db verb testing, then plan Option 3 as foundational work for collaborative ODB editing.
