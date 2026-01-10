# ODB Context Protection Root Cause Analysis
**Date**: 2026-01-07
**Issue**: Assertion failure in `evaluatelist()` after implementing ODB context protection
**Status**: Root cause identified, fix in progress

## Executive Summary

The ODB context protection implementation is failing because:
1. **`getcurrentthreadglobals()` returns nil** on first `db.open()` call
2. **`copythreadglobals(nil)` is called**, but the function doesn't handle nil gracefully
3. **`odbOpenFile()` modifies `currenthashtable`** (sets it to 0x0) during ODB engine operations
4. **`swapinthreadglobals(htg)` attempts to restore** from uninitialized `htg->htable` (contains 0x0)
5. **Assertion fires** when `evaluatelist()` detects `currenthashtable` was corrupted

## Debug Trace Evidence

```
[lang-DEBUG] dbverbs.c:166: odbopenfile ENTRY: currenthashtable=0x600003753638, htg=0x0, timestarted=0
[lang-DEBUG] dbverbs.c:171: odbopenfile AFTER copythreadglobals: htg->htable=0x0
[lang-DEBUG] dbverbs.c:176: odbopenfile AFTER swapinthreadglobals(nil): currenthashtable=0x600003753638
[lang-DEBUG] dbverbs.c:181: odbopenfile AFTER odbOpenFile: currenthashtable=0x0
[lang-DEBUG] dbverbs.c:188: odbopenfile AFTER swapinthreadglobals(htg): currenthashtable=0x0, EXIT
[lang-ERROR] langevaluate.c:1957: evaluatelist ASSERTION: hlocals=0x60000197c3b8 != currenthashtable=0x0
```

## Step-by-Step Analysis

### 1. Initial State
- `currenthashtable = 0x600003753638` (valid locals table)
- `htg = getcurrentthreadglobals() = 0x0` **(PROBLEM #1: nil returned)**
- `timestarted = 0` (first swap, never been swapped in before)

### 2. `copythreadglobals(htg)` Called
- **Expected**: Save `currenthashtable` to `htg->htable`
- **Actual**: `htg` is nil (0x0), so function **either crashes or does nothing**
- Result: `htg->htable` remains uninitialized (0x0 or garbage)

Looking at `process.c:1460-1462`:
```c
(**hg).hccglobals = cancoonglobals;  // ← Would CRASH if hg is nil!

if ((**hg).timestarted != 0) {
    (**hg).htable = currenthashtable;  // ← Only saves if timestarted != 0
}
```

**Key Finding**: If `htg` is truly nil, line 1460 would crash. Since we didn't crash, either:
- A) `htg` is NOT nil, but `htg->htable` was never initialized (likely)
- B) There's a nil check before line 1460 we haven't seen yet

### 3. `swapinthreadglobals(nil)` Called
- **Expected**: Give ODB engine a "clean slate" by clearing globals
- **Actual**: Function **returns immediately** without doing anything (line 1584-1585)

Looking at `process.c:1584-1590`:
```c
hthreadglobals = hg; /*so current thread can easily access its globals*/

if (hg == nil) {
    log_debug(..., "RETURNING EARLY (no restore)");
    return;  // ← EXITS WITHOUT RESTORING ANYTHING
}
```

**Interpretation**: The pattern is NOT "save → clear → call → restore". It's "save → call → restore".
The `swapinthreadglobals(nil)` does **nothing** - `currenthashtable` remains unchanged.

### 4. `odbOpenFile()` Called
- `currenthashtable` was `0x600003753638` before the call
- `currenthashtable` is `0x0` after the call
- **ODB engine modifies `currenthashtable` during its operations!**

This is the ACTUAL problem. The ODB engine (`odbengine.c:odbOpenFile()`) calls `setcancoonglobals()`, which modifies global state including `currenthashtable`.

### 5. `swapinthreadglobals(htg)` Called (Restore)
- **Expected**: Restore `currenthashtable` from saved value in `htg->htable`
- **Actual**: Restores 0x0 because `htg->htable` was never initialized

Looking at `process.c:1612`:
```c
currenthashtable = (**hg).htable;  // ← Restores garbage (0x0)
```

### 6. Assertion Fires
- Control returns to `evaluatelist()` in `langevaluate.c`
- Line 1901 had saved `hlocals = currenthashtable` (0x600003753638)
- Line 1953 checks `if (hlocals != currenthashtable)` → FAILS because `currenthashtable = 0x0`

## Root Cause: `getcurrentthreadglobals()` Returns Nil

The fundamental issue is that `getcurrentthreadglobals()` returns nil on the first `db.open()` call.

Looking at `process.c:1413-1416`:
```c
hdlthreadglobals getcurrentthreadglobals (void) {
    return (hthreadglobals);  // ← Returns global variable
} /*getcurrentthreadglobals*/
```

**Question**: When is `hthreadglobals` initialized?

This is a global variable that should be initialized during Frontier startup. If it's nil, it means:
- Thread globals haven't been initialized yet
- OR we're in a context where thread globals aren't used (unlikely for CLI)

## The Original Pattern's Intent (5.0b17 Comment)

From `dbverbs.c:935`:
```c
/*
5.0b17 dmb: use swapinthreadglobals (nil) for all our odb calls to protect ours
*/
```

**Interpretation**: The pattern is meant to "protect ours" (protect caller's context from ODB engine modifications).

But the implementation doesn't match this intent:
1. `copythreadglobals(htg)` - saves current state to `htg`
2. `swapinthreadglobals(nil)` - **does nothing** (just returns early)
3. ODB call happens with **UNCHANGED globals**
4. `swapinthreadglobals(htg)` - restores state from `htg`

**This pattern ONLY works if `htg` is already initialized with valid state!**

## The "timestarted == 0" Issue

Looking at `copythreadglobals()` line 1462:
```c
if ((**hg).timestarted != 0) { /*context has been swapped in*/
    (**hg).htable = currenthashtable;  // ← ONLY saves if timestarted != 0
}
```

**Problem**: On first use (`timestarted == 0`), `currenthashtable` is NOT saved.

Then at line 1599-1600 in `swapinthreadglobals()`:
```c
if ((**hg).timestarted == 0) /*first time being swapped in*/
    (**hg).timestarted = ticksnow;  // ← Sets timestarted
```

**Pattern**:
- First swap: `timestarted == 0` → don't save state, just restore (garbage)
- Subsequent swaps: `timestarted != 0` → save and restore properly

**This pattern assumes `htg` was previously initialized with valid state!**

## Why This Worked Before

The ODB wrapper pattern has been in the codebase since 5.0b17 (1997). Why did it work?

**Hypothesis**: In the original Mac application:
1. Thread globals were initialized during app startup
2. `hthreadglobals` was NEVER nil during normal operation
3. `htg->htable` was pre-initialized with valid state
4. The first `copythreadglobals()` call didn't need to save because restore would use pre-initialized value

**In headless CLI**:
1. Thread globals initialization might be incomplete
2. `hthreadglobals` might be nil or partially initialized
3. `htg->htable` contains garbage (0x0)
4. First `copythreadglobals()` doesn't save, restore uses garbage

## The Fix

We have THREE options:

### Option 1: Initialize Thread Globals Properly (CORRECT)
Ensure `hthreadglobals` is initialized during CLI startup, BEFORE any db.open() calls.

**Pros**:
- Fixes root cause
- Matches original design intent
- No pattern changes needed

**Cons**:
- Requires finding and fixing initialization code

### Option 2: Handle Nil in copythreadglobals() (WORKAROUND)
Add nil check in `copythreadglobals()`:
```c
void copythreadglobals (hdlthreadglobals hglobals) {
    if (hglobals == nil)
        return;  // ← Early exit for nil
    // ... rest of function
}
```

**Pros**:
- Quick fix
- Prevents crash

**Cons**:
- Doesn't fix underlying problem (globals not initialized)
- Still allows `currenthashtable` corruption

### Option 3: Save Globals BEFORE Pattern (PROPER FIX)
Modify ODB wrapper to save/restore `currenthashtable` explicitly:
```c
boolean odbopenfile (hdlfilenum fnum, odbref *odb, boolean flreadonly) {
    boolean fl;
    hdlhashtable saved_currenthashtable = currenthashtable;  // ← Explicit save

    fl = odbOpenFile (fnum, odb, flreadonly);

    currenthashtable = saved_currenthashtable;  // ← Explicit restore
    return (fl);
}
```

**Pros**:
- Guarantees `currenthashtable` is protected
- Independent of thread globals state
- Simple and explicit

**Cons**:
- Replaces entire copythreadglobals/swapinthreadglobals pattern
- May need to save more globals (hashtablestack, databasedata, etc.)

## Recommended Approach

**Combine Option 1 + Option 3**:
1. Fix thread globals initialization (Option 1) for long-term correctness
2. Add explicit save/restore of critical globals (Option 3) as defense-in-depth

This ensures:
- Thread globals are properly initialized (fixes root cause)
- Critical globals are protected even if thread swap fails (defense-in-depth)
- Pattern matches intent: "protect ours from ODB engine modifications"

## Next Steps

1. Find where `hthreadglobals` should be initialized in CLI startup
2. Verify initialization happens before first script execution
3. Implement explicit save/restore of critical globals in ODB wrappers
4. Test with comprehensive scenarios:
   - First db.open() call
   - Multiple db.open() calls
   - Nested ODB operations
   - Concurrent operations (future)

## Files Modified

- `/Users/jake/dev/jsavin/Frontier-db-verbs/Common/source/dbverbs.c` - Added debug logging
- `/Users/jake/dev/jsavin/Frontier-db-verbs/Common/source/process.c` - Added debug logging
- `/Users/jake/dev/jsavin/Frontier-db-verbs/Common/source/langevaluate.c` - Added debug logging

## References

- Issue tracking this work: (to be created)
- Original 5.0b17 comment: `Common/source/dbverbs.c:935`
- Thread swap functions: `Common/source/process.c:1413-1700`
- Assertion site: `Common/source/langevaluate.c:1953-1959`
