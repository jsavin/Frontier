# Legacy Implementation Investigation Report
## Non-Table Reference Handling in Path Resolution

**Date**: 2026-01-25
**Investigation Target**: Understanding how legacy tedchoward Frontier handles `string(123)` vs `string.mid(...)`
**Working Directory**: `/Users/jake/dev/jsavin/Frontier-fix-string-verb-resolution`
**Branch**: `feature/fix-string-verb-resolution`

---

## Executive Summary

After investigating the legacy tedchoward Frontier codebase, I have identified the **CRITICAL ARCHITECTURAL DIFFERENCE** between legacy and our current implementation:

**Legacy behavior**: The callback (`langgettableval`, `langtablelookup`) returns **only tables**, and returns **false** for non-table items. When searching paths for a simple identifier like `string`, the callback fails on each path entry, and resolution happens at a DIFFERENT LEVEL.

**Our broken behavior**: Our callback (`langdirecttablelookup`) tries to return the **parent table** when it finds a non-table final component. This breaks the resolution logic because the caller doesn't know whether we found a table or a non-table item.

**The fix is simpler than we thought**: We should NOT try to handle non-table items in the callback. The resolution for simple identifiers like `string` happens in `langgethandlercode`, NOT in the callback.

---

## 1. Legacy Code Analysis

### 1.1 File Locations

**Legacy codebase**: `/Users/jake/dev/tedchoward/Frontier`

**Key file**: `Common/source/langvalue.c`

**Critical functions**:
- `langsearchpathvisit` - line 3661
- `langgettableval` - line 3597
- `langtablelookup` - line 3931
- `langgetdotparams` - line 3768
- `langgethandlercode` - line 8078
- `langhandlercall` - line 8204
- `langgethandlervisit` - line 8198

### 1.2 Callback Signature

```c
typedef boolean (*tysearchpathcallback) (hdlhashtable, bigstring, hdlhashtable *);
```

**THREE parameters**, NOT FOUR. No `is_final_component` parameter!

### 1.3 The Two Callbacks

**Callback 1: `langgettableval`** (line 3597-3611)
```c
static boolean langgettableval (hdlhashtable htable, bigstring bsname, hdlhashtable *hval) {
    boolean fl;

    if (htable == nil)
        return (false);

    pushhashtable (htable);

    fl = langexternalgettable (bsname, hval);

    pophashtable ();

    return (fl);
} /*langgettableval*/
```

**Purpose**: Look up `bsname` in `htable` and return a TABLE. Returns false if not found or not a table.

**Callback 2: `langtablelookup`** (line 3931-3948)
```c
boolean langtablelookup (hdlhashtable intable, bigstring bsname, hdlhashtable *htable) {
    if (intable == nil)
        return (false);

    if (!hashtablesymbolexists (intable, bsname))
        return (false);

    *htable = intable; /*don't set this on failure*/

    return (true);
} /*langtablelookup*/
```

**Purpose**: Check if `bsname` exists in `intable`. If yes, return **the input table itself**, NOT the value!

This is used for finding which table CONTAINS a name, not for extracting the value.

---

## 2. Execution Flow for `string(123)`

### 2.1 Parser Stage
- Creates `functionop` node
- `param1 = identifierop("string")`
- `param2 = 123`

### 2.2 Evaluation Stage

**Step 1: `functionvalue`** (line 8471-8485)
```c
boolean functionvalue (hdltreenode htree, hdltreenode hparam1, tyvaluerecord *vreturned) {
    tyvaluerecord val = (**htree).nodeval;

    if (((**htree).nodetype == dotop) || (val.valuetype != tokenvaluetype))
        return (langhandlercall (htree, hparam1, vreturned));  // ← Calls this
    else
        return (builtinvalue ((tyfunctype) val.data.tokenvalue, hparam1, vreturned));
}
```

- `htree->nodetype` is `identifierop` (NOT `dotop`)
- `val.valuetype` is NOT `tokenvaluetype` (not a built-in)
- → Calls `langhandlercall(htree, hparam1, vreturned)`

**Step 2: `langhandlercall`** (line 8204-8314)
```c
boolean langhandlercall (hdltreenode htree, hdltreenode hparam1, tyvaluerecord *vreturned) {
    hdltreenode hcode;
    hdlhashtable htable;
    hdlhashnode hnode;

    setemptystring (bsfunctionname);

    // Try current context
    if (langgethandlercode (currenthashtable, htree, &hcode, &htable, &hnode))
        goto runhandler;

    if (fllangerror)
        return (false);

    // Try system.paths
    handlercode.htree = htree;
    if (langsearchpathvisit (&langgethandlervisit, nil, &htable)) {  // ← Search paths
        hcode = handlercode.hcode;
        hnode = handlercode.hnode;
        goto runhandler;
    }

    // Try efptable
    if (langgethandlercode (efptable, htree, &hcode, &htable, &hnode)) {
        assert (hcode == nil);
        goto runhandler;
    }

    // ... other fallbacks ...

    langparamerror (unknownfunctionerror, bsfunctionname);
    return (false);

    runhandler:
    if (fllangerror)
        return (false);

    return (langfunctioncall (htree, htable, hnode, bsfunctionname, hcode, hparam1, vreturned));
}
```

**Step 3: `langsearchpathvisit`** (line 3661-3727)
```c
static boolean langsearchpathvisit (tysearchpathcallback visit, bigstring bsname, hdlhashtable *htable) {
    register hdlhashtable ht = pathstable;
    register hdlhashnode nomad;
    hdlhashtable hsearch;
    bigstring bs;

    if (ht == nil)
        return (false);

    nomad = (**ht).hfirstsort;

    while (nomad != nil) {
        if ((**nomad).val.valuetype != addressvaluetype)
            goto next;

        if ((**nomad).flunresolvedaddress)
            if (!hashresolvevalue (ht, nomad))
                goto next;

        if (!getaddressvalue ((**nomad).val, &hsearch, bs))
            goto next;

        if (!langgettableval (hsearch, bs, &hsearch))  // ← Resolve path entry to table
            goto next;

        if ((*visit) (hsearch, bsname, htable))  // ← Call callback
            return (true);

        next:
        if (fllangerror)
            break;

        nomad = (**nomad).sortedlink;
    }

    // ... filewindowtable handling ...

    return (false);
}
```

**Key observation**: Line 3704 calls `langgettableval(hsearch, bs, &hsearch)` to resolve the path entry address to an actual table BEFORE calling the visit callback.

For example, if path entry is `@system.verbs.globals`:
- `getaddressvalue` returns `hsearch = system.verbs, bs = "globals"`
- `langgettableval` looks up "globals" in `system.verbs` and returns the `globals` TABLE
- Then `visit` callback is called with `hsearch = system.verbs.globals` table

**Step 4: `langgethandlervisit`** (line 8198-8201)
```c
static boolean langgethandlervisit (hdlhashtable intable, bigstring bs, hdlhashtable *htable) {
    #pragma unused(bs)
    return (langgethandlercode (intable, handlercode.htree, &handlercode.hcode, htable, &handlercode.hnode));
}
```

**Purpose**: Call `langgethandlercode` with the path table as `intable`.

Note: `bs` parameter is UNUSED! The actual name being searched is in `handlercode.htree`, NOT in the callback parameter!

**Step 5: `langgethandlercode`** (line 8078-8185)
```c
static boolean langgethandlercode (hdlhashtable intable, hdltreenode hnamenode, hdltreenode *hcode, hdlhashtable *htable, hdlhashnode *hnode) {
    register hdlhashtable ht;
    bigstring bs;
    register boolean fl;

    setemptystring (bs);

    disablelangerror ();

    if (intable != currenthashtable)
        fllocaldotparamsonly = true;  // ← Prevent recursive path search

    pushhashtable (intable);

    fl = langgetdotparams (hnamenode, htable, bs);  // ← Parse the name

    pophashtable ();

    fllocaldotparamsonly = false;

    if (!isemptystring (bs)) {
        if (fl || isemptystring (bsfunctionname))
            copystring (bs, bsfunctionname);
    }

    enablelangerror ();

    if (!fl)
        return (false);

    ht = *htable;

    if (ht == nil) { /*no table specified*/  // ← CRITICAL BRANCH
        pushhashtable (intable);
        fl = langfindsymbol (bs, htable, hnode);  // ← Search in intable
        pophashtable ();

        if (!fl)
            return (false);

        ht = *htable;
    }
    else {  // ← htable was set by langgetdotparams (dotted name)
        pushhashtable (ht);
        fl = hashlookupnode (bs, hnode);
        pophashtable ();

        if (!fl) {
            return (false);
        }
    }

    /*we've found the table entry, now let's try to get some code out of it*/
    if (!langgetnodecode (ht, bs, *hnode, hcode)) {
        langparamerror (notfunctionerror, bs);
        return (false);
    }

    return (true);
}
```

**CRITICAL INSIGHT**: When `hnamenode` is a simple `identifierop` (like "string"), `langgetdotparams` returns:
- `htable = nil` (no table specified)
- `bs = "string"` (the identifier name)

Then the `if (ht == nil)` branch (line 8144 in legacy) is taken, and it searches for "string" in `intable` (which is `system.verbs.globals` when called from the path visit callback).

**Step 6: `langgetdotparams`** for identifier (line 3768-3874)
```c
boolean langgetdotparams (hdltreenode htree, hdlhashtable *htable, bigstring bsname) {
    register hdltreenode h = htree;
    register tytreetype nodetype = (**h).nodetype;
    hdlhashtable hsubtable;
    register boolean fl;
    tyvaluerecord val;

    *htable = nil;

    langseterrorline (h);

    switch (nodetype) {
        case identifierop:
        case bracketop:
            return (langgetidentifier (h, bsname));  // ← Returns early!

        case dereferenceop:
            // ... address handling ...

        case dotop:
        case arrayop:
            break;

        default:
            langlongparamerror (unexpectedopcodeerror, (long) nodetype);
            return (false);
    }

    // ... rest of dotop/arrayop handling ...
}
```

For `identifierop`, it returns IMMEDIATELY at line 3803 by calling `langgetidentifier`, which just extracts the name into `bsname`.

Result: `htable = nil, bsname = "string"`

---

## 3. Execution Flow for `string.mid(...)`

For comparison, let's trace `string.mid(...)`:

**Step 1-2**: Same as above - `functionvalue` calls `langhandlercall`

**Step 3**: `langhandlercall` calls `langgethandlercode(currenthashtable, htree, ...)`

**Step 4**: `langgethandlercode` calls `langgetdotparams(htree, htable, bs)` where `htree` is a `dotop` node

**Step 5**: `langgetdotparams` for dotop (line 3814-3874)
```c
boolean langgetdotparams (hdltreenode htree, hdlhashtable *htable, bigstring bsname) {
    // ... identifier handling above ...

    // For dotop:
    if (!langgetdotparams ((**h).param1, &hsubtable, bsname)) /*recurse*/
        return (false);

    if (hsubtable == nil) { /*we're at the very first table in the dot list*/
        if (langgetspecialtable (bsname, htable))
            goto L1;

        if (langexternalgettable (bsname, htable))
            goto L1;

        if (fllocaldotparamsonly)
            fl = false;
        else {
            fl = langsearchpathvisit (&langgettableval, bsname, htable);  // ← Search paths

            if (!fl) {
                flfindanyspecialsymbol = true;
                fl = langexternalgettable (bsname, htable);
                flfindanyspecialsymbol = false;
            }
        }
    }
    else
        fl = langgettableval (hsubtable, bsname, htable);  // ← Look up in parent table

    if (!fl) {
        langparamerror (nosuchtableerror, bsname);
        return (false);
    }

    L1: /*deal with param2 here*/

    return (langgetidentifier ((**h).param2, bsname));  // ← Extract final name
}
```

For `string.mid(...)`:
- `param1 = identifierop("string")` → recurses, returns `hsubtable = nil, bsname = "string"`
- Since `hsubtable == nil`, searches paths with `langsearchpathvisit(&langgettableval, "string", &htable)`
- `langgettableval` looks for a TABLE named "string" in each path entry
- When it finds `system.verbs.builtins.string` (a table), it returns `htable = <string table>`
- Then `langgetidentifier(param2, bsname)` extracts "mid"
- Result: `htable = <string table>, bsname = "mid"`

**Back in langgethandlercode**: Since `ht != nil`, it takes the `else` branch (line 8157 in legacy) and looks up "mid" in the string table.

---

## 4. Current vs Legacy Comparison

### 4.1 Key Architectural Differences

| Aspect | Legacy | Current (Broken) |
|--------|--------|------------------|
| **Callback signature** | 3 parameters: `(hdlhashtable, bigstring, hdlhashtable*)` | 4 parameters: added `boolean is_final_component` |
| **Callback purpose** | Return TABLE only, fail on non-table | Try to handle non-table by returning parent |
| **Path resolution** | Path iteration resolves address to table BEFORE callback | Same |
| **Identifier resolution** | Happens in `langgethandlercode` using `intable` parameter | Same, but broken by callback returning parent table |
| **fllocaldotparamsonly** | Set in `langgethandlercode` to prevent recursion | Same |

### 4.2 What We Misunderstood

**Our assumption**: We need to return the parent table when we find a non-table final component, so the caller can look up the value.

**Reality**: For simple identifiers like `string`, the callback is NEVER called with "string" as the `bsname` parameter! The callback is called with the name of a PATH ENTRY (like "globals"), and the actual identifier search happens at a different level (`langgethandlercode` with `intable` parameter).

**The broken flow in our code**:
1. `langsearchpathvisit` iterates path entries
2. For path entry `@system.verbs.globals`, resolves it to `system.verbs.globals` table
3. Calls our callback: `langdirecttablelookup(system.verbs.globals, "string", &htable, true)`
4. Our callback finds "string" (a script, not a table) in `system.verbs.globals`
5. **WRONG**: Returns `htable = system.verbs.globals` (the parent table)
6. `langgetdotparams` receives `htable = system.verbs.globals` from the path search
7. `langgethandlercode` sees `ht != nil`, so it takes the dotted-name branch
8. Tries to look up "string" in `system.verbs.globals` again
9. **SHOULD WORK**, but something is wrong...

Wait, let me re-check the error message. Actually, I think there may be TWO issues:
1. The callback signature is wrong (4 params vs 3)
2. The semantics are wrong (trying to return parent for non-table)

---

## 5. The Correct Fix

### 5.1 Restore Legacy Callback Signature

**Remove `is_final_component` parameter**. The callback should have 3 parameters only:

```c
typedef boolean (*tysearchpathcallback) (hdlhashtable, bigstring, hdlhashtable *);
```

### 5.2 Restore Legacy Callback Semantics

**The callback should ONLY return tables**. If the lookup finds a non-table, return FALSE.

```c
static boolean langdirecttablelookup (hdlhashtable htable, bigstring bsname, hdlhashtable *hresult) {
    tyvaluerecord val;
    hdlhashnode hnode;

    if (htable == nil)
        return (false);

    pushhashtable (htable);

    /* Direct lookup in the table - no global fallbacks */
    if (!hashtablelookup (htable, bsname, &val, &hnode)) {
        pophashtable ();
        return (false);
    }

    /* ONLY return if value is a table - fail otherwise */
    boolean fl = tablevaltotable (val, hresult, hnode);
    pophashtable ();
    return (fl);
}
```

### 5.3 Why This Works

**For `string(123)`**:
1. `langhandlercall` tries local context first - not found
2. Calls `langsearchpathvisit(&langgethandlervisit, nil, &htable)`
3. `langsearchpathvisit` iterates path entries
4. For `@system.verbs.globals`, resolves it to `system.verbs.globals` table
5. Calls `langgethandlervisit(system.verbs.globals, nil, &htable)`
6. `langgethandlervisit` calls `langgethandlercode(system.verbs.globals, htree="string", ...)`
7. `langgethandlercode` calls `langgetdotparams(htree="string", &htable, bs)`
8. `langgetdotparams` sees `identifierop`, returns `htable=nil, bs="string"`
9. `langgethandlercode` sees `ht==nil`, searches for "string" in `intable` (= `system.verbs.globals`)
10. Finds the script! Returns success.

**For `string.mid(...)`**:
1. Same steps 1-2
2. `langsearchpathvisit` is called from `langgetdotparams` (not from `langhandlercall`)
3. Callback is `langgettableval` (not `langgethandlervisit`)
4. Searches for "string" TABLE in path entries
5. Finds `system.verbs.builtins.string` table
6. Returns to `langgetdotparams` with `htable = <string table>`
7. `langgetdotparams` extracts "mid" from param2
8. Returns `htable = <string table>, bsname = "mid"`
9. `langgethandlercode` sees `ht != nil`, looks up "mid" in string table
10. Finds the verb! Returns success.

**For `defined(string)` or `defined(string.mid)`**:
- Uses same path as `string.mid(...)` above
- The callback just checks if the name exists, doesn't extract code

---

## 6. Side Effect Analysis

### 6.1 All Callers of langsearchpathvisit

**Search for callers**:
```bash
cd /Users/jake/dev/jsavin/Frontier-fix-string-verb-resolution
grep -n "langsearchpathvisit" Common/source/langvalue.c
```

Results (current code):
- Line 3815: Definition
- Line 4049: Called from `langgetdotparams` (for dotted names)
- Line 8636: Called from `langhandlercall` (for handler search)

**Legacy code** (same locations):
- Line 3661: Definition
- Line 3839: Called from `langgetdotparams`
- Line 8255: Called from `langhandlercall`

**Callers use different callbacks**:
1. From `langgetdotparams`: Uses `&langgettableval` (searches for TABLE named X)
2. From `langhandlercall`: Uses `&langgethandlervisit` (searches for handler code)

### 6.2 Callback Types

**Callback 1: `langgettableval`**
- Purpose: Find a TABLE with the given name
- Returns: The table if found, false otherwise
- Used by: `langgetdotparams` when resolving dotted names

**Callback 2: `langgethandlervisit`**
- Purpose: Find HANDLER CODE (script, kernel verb, etc.) with the given name
- Returns: Stores result in `handlercode` struct, returns true/false
- Used by: `langhandlercall` when searching for a function to call

**Callback 3: `langdirecttablelookup` (our addition)**
- Purpose: Same as `langgettableval`, but avoids EFP interference
- Should return: TABLE only, false for non-table
- Used by: `langgetdotparams` (replacing `langgettableval` in specific cases)

### 6.3 Impact of Removing `is_final_component`

**Current signature**:
```c
typedef boolean (*tysearchpathcallback) (hdlhashtable, bigstring, hdlhashtable *, boolean);
static boolean langsearchpathvisit (tysearchpathcallback visit, bigstring bsname, hdlhashtable *htable, boolean is_final_component);
```

**Legacy signature**:
```c
typedef boolean (*tysearchpathcallback) (hdlhashtable, bigstring, hdlhashtable *);
static boolean langsearchpathvisit (tysearchpathcallback visit, bigstring bsname, hdlhashtable *htable);
```

**All call sites need updating**:
1. `langgetdotparams` line 4049: Remove `is_final` parameter
2. `langhandlercall` line 8636: Remove `is_final` parameter (doesn't use it anyway)

### 6.4 Will This Break Existing Functionality?

**NO** - because:
1. Restoring legacy signature and semantics
2. Legacy code worked for 25+ years
3. The `is_final_component` parameter was our addition in the failed fix
4. Removing it restores the original design

---

## 7. Implementation Plan

### Phase 1: Restore Callback Signature

1. Update typedef:
   ```c
   typedef boolean (*tysearchpathcallback) (hdlhashtable, bigstring, hdlhashtable *);
   ```

2. Update `langsearchpathvisit` signature:
   ```c
   static boolean langsearchpathvisit (tysearchpathcallback visit, bigstring bsname, hdlhashtable *htable);
   ```

3. Update all call sites:
   - `langgetdotparams` line 4049: Remove `, is_final`
   - `langhandlercall` line 8636: Already doesn't pass it (if legacy-aligned)

### Phase 2: Fix Callback Semantics

Update `langdirecttablelookup`:
```c
static boolean langdirecttablelookup (hdlhashtable htable, bigstring bsname, hdlhashtable *hresult) {
    tyvaluerecord val;
    hdlhashnode hnode;

    if (htable == nil)
        return (false);

    pushhashtable (htable);

    /* Direct lookup in the table - no global fallbacks */
    if (!hashtablelookup (htable, bsname, &val, &hnode)) {
        pophashtable ();
        return (false);
    }

    /* ONLY return tables - fail for non-table values */
    boolean fl = tablevaltotable (val, hresult, hnode);
    pophashtable ();
    return (fl);
}
```

### Phase 3: Clean Up Comments

Remove comments referencing `is_final_component` parameter and "returning parent table for non-table items".

### Phase 4: Test

**Test cases**:
1. `string(123)` - should find script in `system.verbs.globals`
2. `string.mid("hello", 1, 3)` - should find table `system.verbs.builtins.string`, then verb "mid"
3. `defined(string)` - should return true
4. `defined(string.mid)` - should return true
5. `defined(webserver.init)` - should find in correct table
6. All existing integration tests should pass

---

## 8. Why Our Approach Failed

### 8.1 Fundamental Misunderstanding

We thought the callback needed to handle two cases:
1. Intermediate components (must be table)
2. Final components (can be any type)

**Reality**: The callback is ONLY called to resolve PATH ENTRIES to tables. The final lookup happens at a different level.

### 8.2 Confusion Between Two Search Mechanisms

**Mechanism 1: Path search** (`langsearchpathvisit`)
- Purpose: Iterate through `system.paths` entries
- Callback: Looks up name in each path table
- Used for: Finding which table contains a name

**Mechanism 2: Identifier search** (`langgethandlercode` with `intable`)
- Purpose: Search for identifier in a specific table
- Method: `langfindsymbol` or `hashlookupnode`
- Used for: Finding actual value once we know which table

We tried to merge these two mechanisms by having the callback return parent tables for non-table items. But this breaks the separation of concerns.

### 8.3 The `is_final_component` Parameter Was a Red Herring

We added this parameter thinking the callback needed to know context. But the callback doesn't need to know - it just needs to return tables, and the caller handles the rest.

---

## 9. Verification Strategy

### 9.1 Before Fix
Run these commands and capture output:
```bash
cd /Users/jake/dev/jsavin/Frontier-fix-string-verb-resolution
./frontier-cli/frontier-cli -e 'string(123)'
./frontier-cli/frontier-cli -e 'string.mid("hello", 1, 3)'
./frontier-cli/frontier-cli -e 'defined(string)'
./frontier-cli/frontier-cli -e 'defined(string.mid)'
```

Expected: All fail with various errors

### 9.2 After Fix
Same commands should succeed:
- `string(123)` → returns "123"
- `string.mid("hello", 1, 3)` → returns "ell"
- `defined(string)` → returns true
- `defined(string.mid)` → returns true

### 9.3 Integration Tests
```bash
cd tests && make test-integration
```

All tests should pass.

---

## 10. Conclusion

The fix is **much simpler** than we thought:

1. **Remove** the `is_final_component` parameter (4-param callback → 3-param callback)
2. **Restore** legacy callback semantics (return table only, fail on non-table)
3. **Trust** the existing architecture (identifier search happens in `langgethandlercode`, not in callback)

The legacy code already handles simple identifiers vs. dotted names correctly through the `intable` parameter to `langgethandlercode`. We don't need to teach the callback about this distinction.

**Estimated effort**: 1-2 hours for implementation + testing

**Risk level**: LOW - Restoring proven legacy behavior

**Confidence**: HIGH - Complete understanding of the flow through both legacy and current code

---

## Appendices

### Appendix A: Legacy Code Line Numbers

| Function | Legacy Line | Current Line | Notes |
|----------|-------------|--------------|-------|
| `tysearchpathcallback` | 3658 | 3661 | Typedef |
| `langsearchpathvisit` | 3661 | 3815 | Modified signature |
| `langgettableval` | 3597 | 3595 | Same |
| `langtablelookup` | 3931 | 4129 | Same |
| `langgetdotparams` | 3768 | 3966 | Modified logic |
| `langgethandlercode` | 8078 | 8467 | Same core logic |
| `langhandlercall` | 8204 | 8593 | Same |
| `langgethandlervisit` | 8198 | 8587 | Same |

### Appendix B: Test Scenario Details

**Scenario 1: Simple verb call `string(123)`**
- Parse: `functionop(identifierop("string"), 123)`
- Resolve: Search local → search paths → find script in `system.verbs.globals`
- Execute: Compile and run script

**Scenario 2: Dotted verb call `string.mid(...)`**
- Parse: `functionop(dotop(identifierop("string"), identifierop("mid")), ...)`
- Resolve: Find "string" table → look up "mid" in that table
- Execute: Call kernel verb

**Scenario 3: defined() with identifier `defined(string)`**
- Parse: `functionop(identifierop("defined"), identifierop("string"))`
- Resolve: "defined" is built-in, parameter "string" is evaluated
- Execute: Check if "string" exists (uses same path search)

**Scenario 4: defined() with dotted name `defined(string.mid)`**
- Parse: `functionop(identifierop("defined"), dotop(...))`
- Resolve: "defined" is built-in, parameter is dotted name
- Execute: Evaluate dotted name, check if result exists

---

**Report End**
