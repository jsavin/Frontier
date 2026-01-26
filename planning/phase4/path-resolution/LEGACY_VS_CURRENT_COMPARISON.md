# Legacy vs Current Implementation Comparison

**Date**: 2026-01-25
**Purpose**: Understanding how the production legacy code worked vs. current broken implementation

---

## Critical Architectural Differences

### 1. Search Order in langgetdotparams

#### Legacy (WORKED for years in production)
```c
// Line 3827-3839 in legacy langvalue.c
if (hsubtable == nil) { /*we're at the very first table in the dot list*/

    if (langgetspecialtable (bsname, htable))  /* 1. Special tables first */
        goto L1;

    if (langexternalgettable (bsname, htable))  /* 2. External/current context SECOND */
        goto L1;

    if (fllocaldotparamsonly)
        fl = false;
    else {
        fl = langsearchpathvisit (&langgettableval, bsname, htable); /* 3. Paths LAST */
    }
}
```

#### Current (BROKEN after PR #342)
```c
// Line 4031-4058 in current langvalue.c
if (hsubtable == nil) {

    if (langgetspecialtable (bsname, htable))  /* 1. Special tables first */
        goto L1;

    /* PR #342: Check system.paths FIRST before current context */
    if (!fllocaldotparamsonly) {
        fl = langsearchpathvisit (&langdirecttablelookup, bsname, htable); /* 2. Paths SECOND */
        if (fl)
            goto L1;
    }

    if (langexternalgettable (bsname, htable))  /* 3. External/current context LAST */
        goto L1;
}
```

**KEY DIFFERENCE**: PR #342 reversed the order of path search and external lookup!

---

### 2. Recursion Guard Implementation

#### Legacy
```c
// Line 76 in legacy langvalue.c
static boolean fllocaldotparamsonly = false; /* Static variable */

// langsearchpathvisit DOES NOT SET the guard
// Only langgethandlercode sets it (lines 8120-8129)
```

#### Current
```c
// In processinternal.h
boolean fllocaldotparamsonly;  /* Thread global */
#define fllocaldotparamsonly ((**hthreadglobals).fllocaldotparamsonly)

// langsearchpathvisit SETS the guard (line 3867)
saved_fllocaldotparamsonly = fllocaldotparamsonly;
fllocaldotparamsonly = true;  /* Prevent recursive path search */
```

**KEY DIFFERENCE**: Current code has TWO places setting the guard, legacy had ONE!

---

### 3. langdirecttablelookup Function

#### Legacy
**DOES NOT EXIST** - This function was added in PR #336

#### Current
```c
static boolean langdirecttablelookup (hdlhashtable htable, bigstring bsname, hdlhashtable *hresult) {
    /* New function added to avoid EFP table interference */
    /* Uses direct hashtablelookup instead of langexternalgettable */
}
```

---

## Why Legacy Code Worked

### For string(123):

1. **langhandlercall** searches for "string"
2. Calls **langgethandlercode** with currenthashtable
3. **langgethandlercode** calls **langgetdotparams**
4. **langgetdotparams** tries:
   - `langgetspecialtable` - fails
   - `langexternalgettable` - **FINDS "string" in globals via current context**
   - Never needs to search paths!

### Key: langexternalgettable Could Find It

In the legacy code, `langexternalgettable` successfully found `globals.string` because:
- It searches the current hash table context
- Globals are accessible through the current context
- No need for explicit path search

---

## Why Current Code Breaks

### After PR #342:

1. **langhandlercall** searches for "string"
2. **langsearchpathvisit** sets `fllocaldotparamsonly = true`
3. **langgethandlercode** is called
4. **langgetdotparams** tries:
   - `langgetspecialtable` - fails
   - `langsearchpathvisit` - **BLOCKED** by `if (!fllocaldotparamsonly)`
   - `langexternalgettable` - fails to find "string"
5. Result: "string" is never found!

### The Double Guard Problem

Current code has TWO places setting the guard:
1. `langsearchpathvisit` (line 3867) - Sets it for path iteration
2. `langgethandlercode` (line 8580) - Sets it for non-default scope

Legacy code had ONLY:
1. `langgethandlercode` (line 8120) - Sets it for non-default scope

---

## The EFP Table Issue

### What PR #342 Was Trying to Fix

PR #342 wanted to fix `defined(webserver.init)` returning false because:
- `langexternalgettable` was finding EFP stub (7 items) instead of full table (21 items)
- Solution: Search paths BEFORE external lookup

### Why This Broke Everything

By moving path search before external lookup AND having the recursion guard:
- Handler searches can't find simple names like "string"
- The guard blocks legitimate searches
- The architecture doesn't support this search order

---

## Recommended Fix

### Option 1: Revert to Legacy Order (Simplest)

1. Move path search AFTER `langexternalgettable` (like legacy)
2. Remove the guard set in `langsearchpathvisit`
3. Keep only the guard in `langgethandlercode`

**Pros**:
- Matches production code that worked for years
- Simple and proven

**Cons**:
- `defined(webserver.init)` might fail again
- Need different fix for EFP issue

### Option 2: Fix langexternalgettable

Keep current order but make `langexternalgettable` smarter:
- Check if found item is EFP stub
- If so, continue searching for full table
- Prioritize non-EFP results

### Option 3: Conditional Guard

Only set guard in `langsearchpathvisit` when called from another path search:
- Add parameter to indicate if this is a recursive call
- Don't set guard for first-level searches

---

## Test Script from Legacy Days

The legacy code would have handled these correctly:
```usertalk
string(123)  // Returns "123"
string.mid("hello", 2, 3)  // Returns "ell"
defined(webserver.init)  // Returns true
```

All three are currently broken in different ways due to the architectural changes.

---

## Conclusion

The production legacy code used a simpler, more straightforward approach:
1. Check external/current context first
2. Fall back to path search only if needed
3. Single recursion guard point

PR #342's attempt to prioritize paths over external lookup created an architectural conflict that the guard system wasn't designed to handle. The simplest fix is likely to revert to the legacy search order and find a different solution for the EFP stub issue.