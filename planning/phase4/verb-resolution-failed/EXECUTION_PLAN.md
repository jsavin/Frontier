# DEPRECATED - See Legacy Investigation

**This execution plan is OBSOLETE and should not be followed.**

The approach documented here was based on a fundamental misunderstanding of the architecture. After investigating the legacy tedchoward Frontier codebase, we discovered that this approach breaks the separation of concerns between path search and identifier lookup.

**For the correct implementation approach**, see:
- `/Users/jake/dev/jsavin/Frontier-fix-string-verb-resolution/LEGACY_INVESTIGATION_REPORT.md`

**For the problem statement and resolution**, see:
- `STRING_VERB_RESOLUTION_FAILURE.md` in this directory

**This document is preserved for historical reference only.**

---

# String Verb Resolution Fix - Initial Execution Plan (DEPRECATED)

**Date**: 2026-01-25 (Initial attempt)
**Status**: DEPRECATED - DO NOT FOLLOW
**Superseded By**: LEGACY_INVESTIGATION_REPORT.md

---

## Problem Statement

Calling verbs with terse (non-dotted) names fails when the verb exists as a script in `system.verbs.globals`.

**Example**:
- `string(123)` fails - "unknown function"
- `string.mid("hello", 1, 3)` works - finds table, then verb

---

## Initial (Incorrect) Approach

### Phase 1: Add `is_final_component` Parameter

**Goal**: Let the callback know whether it's looking at a final component or an intermediate one.

**Changes**:

1. Update callback typedef:
   ```c
   typedef boolean (*tysearchpathcallback) (
       hdlhashtable intable,
       bigstring bsname,
       hdlhashtable *hresult,
       boolean is_final_component  // NEW PARAMETER
   );
   ```

2. Update `langsearchpathvisit` signature:
   ```c
   static boolean langsearchpathvisit (
       tysearchpathcallback visit,
       bigstring bsname,
       hdlhashtable *htable,
       boolean is_final_component  // NEW PARAMETER
   );
   ```

3. Update all call sites to pass `is_final` parameter

### Phase 2: Update Callback Logic

**Goal**: When `is_final_component=true` and we find a non-table, return the parent table so the caller can look up the value.

**Changes to `langdirecttablelookup`**:
```c
static boolean langdirecttablelookup (
    hdlhashtable htable,
    bigstring bsname,
    hdlhashtable *hresult,
    boolean is_final_component  // NEW
) {
    tyvaluerecord val;
    hdlhashnode hnode;

    if (htable == nil)
        return (false);

    pushhashtable (htable);

    /* Direct lookup in the table */
    if (!hashtablelookup (htable, bsname, &val, &hnode)) {
        pophashtable ();
        return (false);
    }

    /* Try to convert to table */
    if (tablevaltotable (val, hresult, hnode)) {
        pophashtable ();
        return (true);
    }

    /* NEW: If final component and non-table, return parent */
    if (is_final_component) {
        *hresult = htable;  // Return parent table
        pophashtable ();
        return (true);
    }

    /* Intermediate component must be table */
    pophashtable ();
    return (false);
}
```

### Phase 3: Update Call Sites

1. **`langgetdotparams`**: Pass `is_final` based on whether we're at the last dot
2. **`langhandlercall`**: Pass `is_final=true` (searching for handler)

---

## Why This Approach Failed

### Fundamental Misunderstanding

**We thought**: The callback needs to return something useful for both table and non-table final components.

**Reality**: The callback is ONLY responsible for returning tables. The identifier lookup for non-table items happens at a DIFFERENT LEVEL (`langgethandlercode` with `intable` parameter).

### Architectural Violation

By having the callback return the parent table for non-table items, we were:
1. Breaking the separation of concerns
2. Confusing two different search mechanisms (path search vs. identifier search)
3. Making the callback try to do something it was never designed to do

### The Real Flow

For `string(123)`:
1. `langhandlercall` searches paths using `langgethandlervisit` callback
2. Callback finds that "string" exists in `system.verbs.globals` table
3. **Critical**: The callback doesn't need to return anything - `langgethandlercode` does the actual lookup using `intable`

For `string.mid(...)`:
1. `langgetdotparams` searches paths using `langgettableval` callback
2. Callback finds "string" TABLE in `system.verbs.builtins`
3. Returns the table to caller
4. Caller then looks up "mid" in that table

**The key difference**: In the first case, the callback is checking existence. In the second case, it's returning an intermediate table. But in NEITHER case should it return a parent table for a non-table item.

---

## Correct Approach

See `LEGACY_INVESTIGATION_REPORT.md` for the correct implementation approach, which is:

1. **Keep** 3-parameter callback signature (no `is_final_component`)
2. **Restore** legacy semantics: return table only, fail on non-table
3. **Trust** the architecture: `langgethandlercode` handles identifier lookup

---

## Lessons Learned

### Don't Add Parameters Without Understanding

We added `is_final_component` thinking it would help, but:
- The callback doesn't need this information
- It led us down the wrong path architecturally
- The legacy code worked for 25+ years without it

### Separation of Concerns

The architecture has clear separation:
- **Path search**: Find which table contains a name (callback's job)
- **Identifier lookup**: Find the actual value (caller's job)

Don't try to merge these concerns.

### Investigate Legacy First

We should have started with legacy investigation instead of trying to fix based on assumptions. The legacy code is the source of truth for how this should work.

---

## References

- `LEGACY_INVESTIGATION_REPORT.md` - Correct implementation approach
- `STRING_VERB_RESOLUTION_FAILURE.md` - Problem statement and resolution
- Legacy codebase: `/Users/jake/dev/tedchoward/Frontier`

---

**Document End**
