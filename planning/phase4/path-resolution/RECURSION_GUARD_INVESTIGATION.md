# Recursion Guard Investigation - string(123) Failure

**Date**: 2026-01-25
**Issue**: `string(123)` fails with "Can't call string because it isn't a script"
**Root Cause**: Architectural conflict between handler search and name resolution due to recursion guard

---

## Executive Summary

The `string(123)` failure is caused by a recursion guard (`fllocaldotparamsonly`) that prevents legitimate path searches during handler resolution. PR #342 (2026-01-24) added a check that blocks path searches when already in a path search, but this prevents `string` from being found in `system.verbs.globals`.

**Key Discovery**: The recursion guard was likely added in response to automated code review feedback about "dead code" without fully understanding the architectural implications.

---

## Problem Timeline

### 1. Original State (Before PR #336)
- `langdirecttablelookup()` didn't exist
- System worked but had issues with address resolution in system.paths

### 2. PR #336 (Commit ddfff279) - First Bad Commit
- Added `langdirecttablelookup()` function
- Function incorrectly used `tablevaltotable()` which rejects non-table types
- This caused `string(123)` to skip path01 (globals.string SCRIPT) and find path03 (builtins.string TABLE)
- Result: "Can't call string because it isn't a script"

### 3. PR #342 (Commit 13b62bfd7) - Attempted Fix That Made Things Worse
- Tried to fix `defined(webserver.init)` returning false
- Added recursion guard check: `if (!fllocaldotparamsonly)` before path search
- This was added after bot review feedback about "dead code"
- Now `string(123)` can't find "string" at all during handler lookups

---

## Architecture Deep Dive

### Call Flow for string(123)

```
1. User types: string(123)
2. langhandlercall() is invoked to handle the function call
3. langhandlercall() searches for "string" handler:
   └─> langsearchpathvisit(&langgethandlervisit, ...)
       ├─> Sets fllocaldotparamsonly = true (recursion guard)
       └─> Calls langgethandlervisit for each path
           └─> langgethandlercode()
               └─> langgetdotparams() to resolve "string"
                   ├─> Wants to search paths: langsearchpathvisit()
                   └─> BLOCKED by: if (!fllocaldotparamsonly)
```

### The Recursion Guard Problem

The guard `fllocaldotparamsonly` serves two conflicting purposes:

1. **Prevents infinite recursion**: When resolving addresses like `@system.verbs.globals`, we need to resolve "system", then "verbs", then "globals". Each resolution could trigger another path search, causing infinite loops.

2. **Blocks legitimate searches**: When `langhandlercall` searches for handlers, it prevents `langgetdotparams` from searching paths for the initial name like "string".

### Why Removing the Guard Causes Stack Overflow

When we removed the guard check (line 4037), we got infinite recursion because:

```
langgetdotparams("string")
└─> langsearchpathvisit()  [searching system.paths]
    └─> For each path like @system.verbs.globals:
        └─> Must resolve the address
            └─> Calls langgetdotparams("system")
                └─> langsearchpathvisit() again!
                    └─> Infinite recursion!
```

---

## Fixes Applied (Partially Working)

### Fix 1: JIT Compiler Initialization ✅
**Location**: `Common/source/db_format.c:234`
```c
#ifdef FRONTIER_HEADLESS
    extern void headless_init_script_compiler(void);
    headless_init_script_compiler();
#endif
```
**Status**: WORKING - Scripts can now be compiled

### Fix 2: langdirecttablelookup Accept Any Type ✅
**Location**: `Common/source/langvalue.c:3783-3803`
```c
/* Accept ANY type for final component resolution.
 * Return the parent table so caller can perform final lookup. */
*hresult = htable;
return (true);
```
**Status**: WORKING - Accepts scripts, not just tables

### Fix 3: Recursion Guard Removal ❌
**Attempted**: Remove `if (!fllocaldotparamsonly)` check
**Result**: Stack overflow due to infinite recursion
**Status**: FAILED - Guard is necessary but too restrictive

---

## Key Insights from Investigation

### 1. JIT Compilation Was Completely Broken
- `headless_init_script_compiler()` existed but was NEVER called
- Script compilation callback was a no-op (`cb_noop_hashnode_treenode`)
- All UserTalk wrapper scripts would fail to compile

### 2. Two "string" Objects Exist
- `system.verbs.globals.string` = SCRIPT (for `string(123)`)
- `system.verbs.builtins.string` = TABLE (for `string.mid()`)
- Path resolution must find the right one based on context

### 3. Callback Contract
- Callbacks return the PARENT table, not the value itself
- Caller must perform final lookup using `langsymbolreference()`
- This pattern is used consistently (e.g., `langtablelookup()`)

### 4. PR #342 Bot Review Origin
The recursion guard was likely added after automated code review feedback:
- Bot flagged "dead code" where `fl` was always false
- Developer wrapped path search in `if (!fllocaldotparamsonly)`
- This "fixed" the dead code warning but broke handler resolution

---

## Critical Code Sections

### langdirecttablelookup (Fixed)
```c
static boolean langdirecttablelookup (hdlhashtable htable, bigstring bsname, hdlhashtable *hresult) {
    // ... lookup code ...

    /* Accept ANY type - return parent table */
    *hresult = htable;
    return (true);
}
```

### langgetdotparams (Problematic Section)
```c
/* Line 4037 - The problematic guard */
if (!fllocaldotparamsonly) {
    fl = langsearchpathvisit (&langdirecttablelookup, bsname, htable);
    if (fl)
        goto L1;
}
```

### langsearchpathvisit (Sets the Guard)
```c
/* Line 3867 */
saved_fllocaldotparamsonly = fllocaldotparamsonly;
fllocaldotparamsonly = true;  /* Prevent recursive path search */
```

---

## Attempted Solutions

### 1. Remove Guard Check ❌
**Result**: Stack overflow from infinite recursion
**Why**: Path resolution needs to resolve addresses recursively

### 2. Temporarily Clear Guard ❌
**Code**:
```c
if (hsubtable == nil && fllocaldotparamsonly) {
    boolean saved_guard = fllocaldotparamsonly;
    fllocaldotparamsonly = false;
    fl = langsearchpathvisit(...);
    fllocaldotparamsonly = saved_guard;
}
```
**Result**: Still stack overflow
**Why**: `langsearchpathvisit` immediately sets the guard again

### 3. Different Guard Types (Not Attempted)
**Idea**: Use separate flags for handler searches vs. name searches
**Challenge**: Requires changing thread globals structure

---

## The Fundamental Conflict

PR #342 wanted to search paths BEFORE `langexternalgettable()` to avoid EFP contamination. But this creates an architectural conflict:

1. **Handler searches** must prevent recursive path searches (via guard)
2. **Name resolution** needs to search paths for simple names
3. Both use the same code path through `langgetdotparams`
4. The guard blocks ALL path searches, not just recursive ones

---

## Possible Solutions (Not Yet Implemented)

### Option 1: Fix langexternalgettable Instead
Rather than changing search order, make `langexternalgettable` smarter about prioritizing builtins over EFP stubs.

### Option 2: Separate Search Paths
Have different code paths for handler searches vs. direct name resolution.

### Option 3: Counter-Based Guard
Instead of boolean flag, use a counter to allow one level of path searching.

### Option 4: Revert PR #342 Changes
Accept that `defined(webserver.init)` doesn't work, fix `string(123)` first.

---

## Test Commands

```bash
# Primary failure case
./frontier-cli/frontier-cli -e "string(123)"
# Expected: "123"
# Actual: "Can't call string because it isn't a script"

# Verify script exists
./frontier-cli/frontier-cli -e "typeOf(@system.verbs.globals.string)"
# Returns: 'scpt' (correct)

# Other affected functionality
./frontier-cli/frontier-cli -e "defined(webserver.init)"
# Expected: true
# Actual: false
```

---

## Files Modified

1. **Common/source/db_format.c** - Added JIT compiler initialization
2. **Common/source/langvalue.c** - Fixed `langdirecttablelookup`, attempted guard fixes
3. **portable/script_portable.c** - Contains `headless_init_script_compiler()`

---

## Next Steps

1. **Investigate Alternative**: Can we fix `langexternalgettable` to prioritize correctly without changing search order?

2. **Understand EFP Contamination**: What exactly is the "EFP stub" problem PR #342 was trying to fix?

3. **Consider Architectural Refactor**: The recursion guard architecture may need fundamental changes to support both use cases.

4. **Consult Git History**: Check if there were other attempts to fix this that were reverted.

---

## Related Documents

- `planning/phase4/path-resolution/FINAL_RECOMMENDATION.md` - Original analysis
- `planning/phase4/path-resolution/FIX_ANALYSIS_REPORT.md` - Initial investigation
- PR #336 - Introduced `langdirecttablelookup` with type restriction bug
- PR #342 - Added recursion guard check that blocks handler resolution
- Issue #337 - Original regression that PR #342 tried to fix

---

**Status**: Investigation ongoing. The recursion guard is necessary to prevent infinite loops but is currently too restrictive. A more nuanced solution is needed that distinguishes between recursive path searches (should be blocked) and initial handler searches (should be allowed).