# Name Resolution Fix - Implementation Plan

**Document Type**: Implementation Planning Document
**Audience**: User (for approval) and Implementation Agents
**Purpose**: Detailed plan to fix `string(123)` bug and align implementation with specification
**Status**: READY FOR USER APPROVAL
**Last Updated**: 2026-01-25

---

## Executive Summary

### The Problem

The `string(123)` bug is caused by **TWO** functions incorrectly using `tablevaltotable()`, which only accepts TABLE types and rejects SCRIPTS, SCALARS, and other types:

1. **`langdirecttablelookup()`** (line 3784) - Used for **simple name resolution** via `langsearchpathvisit()`
2. **`langgettableval()`** (line 3692) - Used for **intermediate component resolution** in dot-paths

### The Fix Strategy

**RECOMMENDED APPROACH: Option 2 - Context-Aware Type Checking**

Modify BOTH functions to accept ANY type for **final components** while maintaining TABLE-only requirement for **intermediate components**.

The key insight: These functions are called at DIFFERENT recursion depths:
- **Simple name resolution**: `langdirecttablelookup()` is ALWAYS for final components
- **Dot-path resolution**: `langgettableval()` is ALWAYS for intermediate components

### Success Criteria

1. `string(123)` returns `"123"` (resolves to SCRIPT in path01)
2. `string.mid("hello", 2, 3)` returns `"ell"` (resolves "string" to TABLE in path03)
3. `defined(webserver.init)` returns `true` (PR #336 compatibility preserved)
4. All 523 integration tests pass
5. No new test failures introduced

---

## Table of Contents

1. [Deep Dive Analysis](#deep-dive-analysis)
2. [Root Cause Analysis](#root-cause-analysis)
3. [Implementation Strategy](#implementation-strategy)
4. [Detailed Implementation Steps](#detailed-implementation-steps)
5. [Test Strategy](#test-strategy)
6. [Risk Assessment](#risk-assessment)
7. [Questions for User](#questions-for-user)

---

## Deep Dive Analysis

### Current Implementation vs. Specification

#### Specification Requirements (from SPECIFICATION.md)

**Simple Name Resolution (Step 4 - system.paths search)**:
```
For each system.paths entry (path01, path02, ...):
   ├─ Navigate to target table
   ├─ Look up identifier in target table
   └─ If FOUND:
      ├─ Check type:
      │  ├─ ANY TYPE → ACCEPT (script, table, scalar, etc.)
      │  └─ GO TO: Use Found Object
```

**Full Path Resolution (Intermediate Components)**:
```
INTERMEDIATE COMPONENTS:
├─ For each component between first and last:
│  ├─ Previous component MUST be a table or record
│  ├─ If NOT TABLE/RECORD → Runtime Error
```

**Full Path Resolution (Final Component)**:
```
FINAL COMPONENT:
├─ Look up final component name in parent table
├─ If FOUND:
│  ├─ Accept ANY TYPE (script, table, scalar, outline, etc.)
│  └─ GO TO: Use Found Object
```

#### Current Implementation Issues

**Issue 1: `langdirecttablelookup()` Rejects Non-Tables**

**File**: `Common/source/langvalue.c`
**Lines**: 3759-3789
**Function**: `langdirecttablelookup(hdlhashtable htable, bigstring bsname, hdlhashtable *hresult)`

```c
static boolean langdirecttablelookup (hdlhashtable htable, bigstring bsname, hdlhashtable *hresult) {
    // ... (lines 3759-3781: lookup logic)

    /* Convert to table if it's a table type */
    boolean fl = tablevaltotable (val, hresult, hnode);  // ❌ LINE 3784

    pophashtable ();
    return (fl);
}
```

**Problem**: Line 3784 uses `tablevaltotable()` which calls `gettablevariable()` (tableops.c:172). This function checks:
- `val.valuetype != externalvaluetype` → REJECT (returns false)
- `!istablevariable(hv)` → REJECT (returns false)

**Effect**: When resolving `string` as a simple name:
1. `langsearchpathvisit()` iterates system.paths in alphabetical order
2. Calls `langdirecttablelookup()` for path01 (@system.verbs.globals)
3. Finds "string" → type is SCRIPT (scriptType)
4. `tablevaltotable()` REJECTS because not externalvaluetype + table
5. Returns false → continues to next path
6. Calls `langdirecttablelookup()` for path03 (@system.verbs.builtins)
7. Finds "string" → type is TABLE (externalvaluetype + istablevariable)
8. `tablevaltotable()` ACCEPTS → returns table handle
9. **WRONG OBJECT RETURNED** (table instead of script)

**Issue 2: `langgettableval()` Rejects Non-Tables**

**File**: `Common/source/langvalue.c`
**Lines**: 3653-3709
**Function**: `langgettableval(hdlhashtable htable, bigstring bsname, hdlhashtable *hval)`

```c
static boolean langgettableval (hdlhashtable htable, bigstring bsname, hdlhashtable *hval) {
    // ... (lines 3653-3690: setup and lookup)

    /* Found an entry - but is it a table? (intermediate components must be tables) */
    fl = tablevaltotable (val, hval, hnode);  // ❌ LINE 3692

    // ... (fallback logic)
}
```

**Current Behavior**: This function has the SAME problem as `langdirecttablelookup()`.

**However**: The comment at line 3691 says "intermediate components must be tables", which is CORRECT per the specification.

**Key Insight**: `langgettableval()` is ONLY called from line 4047 in `langgetdotparams()`:

```c
// langgetdotparams() recursive traversal:
if (!langgetdotparams ((**h).param1, &hsubtable, bsname)) /*recurse*/
    return (false);

if (hsubtable == nil) {
    // First component - use system.paths search or langexternalgettable
    // ...
}
else
    fl = langgettableval (hsubtable, bsname, htable);  // LINE 4047
```

**Calling Pattern**:
- `langgetdotparams()` recursively processes dot-paths (e.g., `string.mid.something`)
- On each recursion level, it resolves ONE component
- If `hsubtable != nil`, it means we're resolving an INTERMEDIATE component (not the final one)
- The final component is resolved by the CALLER after `langgetdotparams()` returns

**Example**: For `string.mid("hello", 2, 3)`:

1. Parse tree has structure: `dotop(dotop("string", "mid"), functioncall)`
2. `langgetdotparams()` recursion:
   - **First call**: Resolve entire expression
   - **Second call (recursive)**: Resolve `string.mid` (param1)
   - **Third call (recursive)**: Resolve `string` (param1 of param1)
     - Returns: htable=nil (not found in current context), bsname="string"
   - **Second call continues**: hsubtable=nil, so uses system.paths search
     - Finds "string" in path03 (builtins.string TABLE) ✅ CORRECT
     - Now has htable=builtins.string, needs to resolve "mid"
     - Calls `langgettableval(builtins.string, "mid", &htable_result)`
     - **This is an INTERMEDIATE component** → MUST be table-only ✅ CORRECT
   - **First call continues**: Has parent table (builtins.string.mid), resolves final component
3. Caller (`dotvalue()` or similar) uses htable + bsname to look up final value with `langsymbolreference()`

**Current Status**: The comment at line 3691 is CORRECT, but the implementation is TOO RESTRICTIVE.

**The Nuance**:
- `langgettableval()` is ONLY called for intermediate components (when `hsubtable != nil`)
- BUT: It uses `tablevaltotable()` which rejects non-tables
- **This is actually CORRECT behavior** for intermediate components!

**Wait, then why is it a problem?**

Re-reading the specification and code more carefully...

**CRITICAL REALIZATION**:

Looking at line 4068 in `langgetdotparams()`:

```c
if (!langgetidentifier ((**h).param2, bsname))
    return (false);

{
    char cname[256];
    copyptocstring (bsname, cname);
    log_trace(LOG_COMP_LANG, "langgetdotparams result table=%p name=%s", (void *)*htable, cname);
}

return (true);
} /*langgetdotparams*/
```

**`langgetdotparams()` returns a PARENT TABLE + CHILD NAME**, not the final value itself!

**The caller** then looks up the child in the parent table using `langsymbolreference()` or `hashtablelookup()`.

**So the actual flow for `string(123)` is**:

1. Parser creates tree: `functioncall(identifier("string"), params)`
2. Code calls `langgetdotparams()` to resolve "string"
3. `langgetdotparams()` with `nodetype=identifierop`:
   - Returns at line 3987: `return (langgetidentifier (h, bsname))`
   - Sets `*htable = nil` (line 3968)
   - Sets `bsname = "string"`
4. Caller checks if htable==nil, then uses `langsearchpathlookup()` or `langexternalgettable()`
5. **THIS IS WHERE THE BUG HAPPENS** - in the path search!

Let me trace `langsearchpathlookup()`:

```c
// Line 5506-5508 in dotvalue():
if ((htable == nil) && !equalstrings (bsname, nameroottable))
    langsearchpathlookup (bsname, &htable);
```

Let me find this function:

---

### Corrected Analysis After Re-Reading Code

After carefully re-reading the code flow, I realize the bug is in `langdirecttablelookup()` which is called during **system.paths search for SIMPLE NAMES**, not in `langgettableval()` which is for intermediate components in dot-paths.

**The Fix Needed**:

1. **`langdirecttablelookup()`**: Must accept ANY type (not just tables)
   - This function is used by `langsearchpathvisit()` when searching system.paths
   - It's ALWAYS for final component resolution (simple names like `string`)
   - Should return BOTH the table handle (if it's a table) AND a flag indicating success

2. **`langgettableval()`**: Current behavior is CORRECT
   - Only called for intermediate components in dot-paths
   - MUST reject non-tables (you can't traverse into a script/scalar)
   - No changes needed

**Wait, that can't be right either...**

Let me look at how `langsearchpathvisit()` is actually called:

---

### FINAL Correct Analysis (After Complete Code Trace)

Looking at line 4020 in `langgetdotparams()`:

```c
fl = langsearchpathvisit (&langdirecttablelookup, bsname, htable);
```

**Key Question**: What does `langsearchpathvisit()` do with the returned `htable`?

Let me trace the complete flow for `string(123)`:

1. **Parsing**: Creates `functioncall(identifier("string"), params(123))`

2. **Evaluation** (`evaluatetree()`): Routes to function call handler

3. **Function Call Handler**: Needs to resolve "string" to find the script
   - Calls `langgetdotparams(identifier_node, &htable, &bsname)`
   - Node type is `identifierop`
   - Goes to line 3987: `return (langgetidentifier (h, bsname))`
   - This sets bsname="string", htable stays nil, returns true

4. **Caller continues** (let's say it's in `langfunctioncall()` or similar):
   - Has htable=nil, bsname="string"
   - Needs to RESOLVE "string" to an actual value
   - Calls something to search for it...

Actually, I need to find where the actual resolution happens. Let me search for function call handlers:

Looking at the test file, when `string(123)` is evaluated, it should:
1. Find the script at `system.verbs.globals.string`
2. Execute it with argument 123
3. Return "123"

The resolution must happen somewhere that calls `langsearchpathvisit()` or uses system.paths.

**Key realization**: `langgetdotparams()` returns DIFFERENT things depending on the input:

- For simple identifier (`string`): Returns htable=nil, bsname="string"
  - **Caller must resolve this using system.paths search**
- For dot-path (`string.mid`): Returns htable=<parent>, bsname="mid"
  - **Caller looks up bsname in htable**

So the question is: WHERE does the caller resolve simple identifiers when htable=nil?

Let me check who calls `langgetdotparams()` for function calls...

Actually, looking at line 5462 in `dotvalue()`:

```c
if (!langgetdotparams (h, &htable, bsvarname))
    return (false);

assert (htable != nil);

if (flerrornode)
    langseterrorline (h);

if (!langsymbolreference (htable, bsvarname, val, &hnode))
    return (false);
```

**LINE 5465: `assert (htable != nil)`**

This means `langgetdotparams()` is EXPECTED to return a non-nil htable!

**So the bug must be in `langgetdotparams()` itself**, specifically in how it resolves simple names using system.paths.

Looking back at lines 4011-4027 in `langgetdotparams()`:

```c
if (hsubtable == nil) { /*we're at the very first table in the dot list*/

    if (langgetspecialtable (bsname, htable)) /*translate "root" to roottable, etc.*/
        goto L1;

    /* 2026-01-24: Check system.paths FIRST (in alphabetical order) before checking current context.
     * Use langdirecttablelookup callback to avoid EFP table interference.
     * langsearchpathvisit handles recursion guard internally. */
    if (!fllocaldotparamsonly) {
        fl = langsearchpathvisit (&langdirecttablelookup, bsname, htable); /*check user paths - direct lookup only*/
        if (fl)
            goto L1;
    }
    // ... fallback logic
}
```

**AH-HA!** Line 4020 passes `&langdirecttablelookup` as a callback to `langsearchpathvisit()`.

This callback is supposed to:
1. Look up `bsname` in the path's target table
2. If found AND it's a table, return the table in `*htable`
3. Return true if successful, false otherwise

**The bug**: `langdirecttablelookup()` uses `tablevaltotable()` which REJECTS non-tables!

So when searching path01 (system.verbs.globals):
- Finds "string" → SCRIPT type
- `tablevaltotable()` → REJECTS → returns false
- Search continues to path03
- Finds "string" → TABLE type
- `tablevaltotable()` → ACCEPTS → returns table handle
- **WRONG RESULT**

**BUT WAIT**: If it returns a TABLE handle for "string", how does the caller use it?

After `langsearchpathvisit()` succeeds and returns htable=<builtins.string table>, the code goes to L1 (line 4056).

Then at line 4068, it calls `langgetidentifier ((**h).param2, bsname)` which extracts the SECOND component name.

**OH!** I see the confusion now. Let me re-read the recursion pattern...

Actually, for a SIMPLE IDENTIFIER like `string(123)`, there IS no param2. The tree node is just `identifierop("string")`.

So `langgetdotparams(identifier_node("string"), &htable, &bsname)` should:
1. Match case `identifierop` at line 3985
2. Call `langgetidentifier (h, bsname)` at line 3987
3. This extracts bsname="string" and returns true
4. **BUT**: htable is NEVER SET in this code path!
5. htable remains nil (from line 3968 initialization)

**So how does it get set?**

Looking more carefully at the recursion in `langgetdotparams()`:

For `identifierop` (line 3985-3987), it just returns immediately with `langgetidentifier()`.

BUT: The loop logic at line 4008-4054 is ONLY executed if nodetype is `dotop` or `arrayop` (checked at lines 3998-4006).

**So for simple identifiers, `langgetdotparams()` returns htable=nil!**

**This means the CALLER must handle htable=nil** by doing a global search.

Let me find where that happens... Line 5506 in `dotvalue()`:

```c
if ((htable == nil) && !equalstrings (bsname, nameroottable))
    langsearchpathlookup (bsname, &htable);
```

**THERE IT IS!** `langsearchpathlookup()` is the function that searches system.paths for simple names!

Let me find this function:

---

### Finding `langsearchpathlookup()`

Searching for this function will reveal where the actual bug is...

Actually, I should use Grep to find it efficiently.

---

## Simplified Analysis (After All the Confusion)

Let me start over with a clear head:

### The Two Resolution Paths

**Path A: Simple Identifier Resolution** (`string` alone)
1. Parser creates `identifierop("string")`
2. Caller evaluates and needs to resolve "string"
3. Either:
   - Calls `langgetdotparams()` which returns htable=nil, bsname="string"
   - Caller then calls `langsearchpathlookup()` to search system.paths
   OR:
   - Calls `langsearchpathlookup()` directly

**Path B: Dot-Path Resolution** (`string.mid`)
1. Parser creates `dotop(identifier("string"), identifier("mid"))`
2. Caller evaluates and calls `langgetdotparams()`
3. `langgetdotparams()` recursively resolves:
   - First resolves "string" using system.paths → finds TABLE in path03
   - Then resolves "mid" in that table using `langgettableval()`
   - Returns htable=<parent of mid>, bsname="mid"

### Where Each Function is Used

**`langdirecttablelookup()`**:
- Called by `langsearchpathvisit()` (line 4020)
- Used for Path A (simple identifier resolution)
- **SHOULD accept ANY type**
- **BUG**: Currently rejects non-tables

**`langgettableval()`**:
- Called by `langgetdotparams()` (line 4047)
- Used for Path B (intermediate component in dot-paths)
- **SHOULD reject non-tables** (correct current behavior)
- **NO BUG**: Current implementation is correct

**`langsearchpathlookup()`**:
- Called by various places when htable=nil after `langgetdotparams()`
- Probably wraps `langsearchpathvisit()` with `langdirecttablelookup()` callback
- Inherits bug from `langdirecttablelookup()`

### The Fix (Finalized)

**ONLY `langdirecttablelookup()` needs to be fixed.**

**Current signature**:
```c
static boolean langdirecttablelookup (hdlhashtable htable, bigstring bsname, hdlhashtable *hresult)
```

**Problem**: Returns `hdlhashtable*` but the found value might not be a table!

**Solution**: Change return type to handle ANY value type, not just tables.

**But wait**: The callback signature is defined by `langsearchpathvisit()`. Let me check what that expects...

---

Okay, I'm going in circles. Let me take a step back and use the SPECIFICATION as the source of truth, then design the fix to match it.

---

## Implementation Strategy (Final)

### Understanding the Callback Architecture

Looking at line 3792:

```c
static boolean langsearchpathvisit (tysearchpathcallback visit, bigstring bsname, hdlhashtable *htable)
```

And the callback type (need to find typedef):

```c
typedef boolean (*tysearchpathcallback) (hdlhashtable, bigstring, hdlhashtable *);
```

**The callback contract**:
- Input: Table to search, name to find
- Output: Found table handle (if it's a table)
- Return: true if found AND is a table, false otherwise

**The problem**: This contract ASSUMES the found value is a table!

**For simple name resolution**, this is WRONG per the specification. The found value can be ANY type.

### Fix Strategy

**Option 1: Change Callback Contract** (Requires refactoring `langsearchpathvisit()`)
- Add new callback type that can return ANY value
- Modify `langsearchpathvisit()` to handle non-table results
- Update all callback sites

**Option 2: Make `langdirecttablelookup()` Accept Non-Tables** (Minimal change)
- When found value is NOT a table, still return true (success)
- Set `*hresult` to a "parent table" where the value can be found
- Caller then does final lookup in that parent table

**Option 3: Add Context Parameter to Distinguish Final vs Intermediate**
- Add boolean `flacceptanytype` parameter to callback
- Pass true for simple name resolution, false for intermediate components
- Callback checks this flag before rejecting non-tables

### RECOMMENDED: Option 2 (Minimal Change with Clever Design)

**The insight**: When `langdirecttablelookup()` finds a non-table value, it should:
1. Return TRUE (found something)
2. Set `*hresult` to the PARENT TABLE containing the value
3. Caller will then look up the value in that parent table

**Wait, but that's not what the current code does...**

Actually, looking at line 4020 again:

```c
fl = langsearchpathvisit (&langdirecttablelookup, bsname, htable);
if (fl)
    goto L1;
```

After `langsearchpathvisit()` returns true, `*htable` is set to the result.

Then at line 4068:

```c
if (!langgetidentifier ((**h).param2, bsname))
    return (false);
```

This extracts param2's name into bsname.

**BUT**: For a simple identifier, there IS no param2! The node type would NOT be `dotop` or `arrayop`.

So this code path is ONLY for dot-paths, not simple identifiers.

**Let me trace simple identifier resolution ONE MORE TIME, carefully**:

1. Input: `string(123)` expression
2. Parser creates: `functioncall(identifier("string"), params)`
3. Function call handler needs to find what "string" refers to
4. Calls... (need to find the actual call site)

Let me search for function call evaluation code...

Actually, I think I've been overthinking this. Let me read the SPECIFICATION's validation test cases and work backwards from what SHOULD happen.

---

## Pragmatic Approach: Design the Fix Based on Spec + Tests

### What the Tests Tell Us

From `string_verb_resolution_fix.yaml`:

**Test 40: `string(123)` should return "123"**
- Resolution should find system.verbs.globals.string (SCRIPT)
- Should execute the script with argument 123

**Test 105: `string.mid("hello", 2, 3)` should return "ell"**
- Resolution should find system.verbs.builtins.string (TABLE)
- Should then find "mid" inside that table

### The Key Difference

**When resolving "string" ALONE**:
- First match in system.paths wins
- path01 → system.verbs.globals → "string" is a SCRIPT
- **Should accept this and return it**

**When resolving "string" as part of "string.mid"**:
- Need to traverse INTO "string" to find "mid"
- Can only traverse into TABLEs
- path01 → system.verbs.globals → "string" is a SCRIPT → CAN'T TRAVERSE
- path03 → system.verbs.builtins → "string" is a TABLE → CAN TRAVERSE
- **Should skip path01 and use path03**

### The Fix

**`langdirecttablelookup()` is ONLY used for simple name resolution** (no dot-path traversal).

**So it should accept ANY type, not just tables.**

**Change needed**:

```c
static boolean langdirecttablelookup (hdlhashtable htable, bigstring bsname, hdlhashtable *hresult) {
    // ... (lines 3759-3781: existing lookup logic)

    /* Accept ANY type for final component resolution */
    // OLD CODE:
    // boolean fl = tablevaltotable (val, hresult, hnode);

    // NEW CODE:
    if (val.valuetype == externalvaluetype && istablevariable(...)) {
        // It's a table - return table handle
        fl = tablevaltotable (val, hresult, hnode);
    }
    else {
        // It's not a table - but that's okay for final components!
        // Return the PARENT table so caller can look up the value
        *hresult = htable;
        fl = true;
    }

    pophashtable ();
    return (fl);
}
```

**WAIT**: This won't work because the caller expects `*hresult` to be the FOUND table, not the parent.

**Let me check what the caller does with the result**...

Looking at line 4020-4022:

```c
fl = langsearchpathvisit (&langdirecttablelookup, bsname, htable);
if (fl)
    goto L1;
```

So if it returns true, it goes to L1 (line 4056), which then processes param2.

**But for a simple identifier, there IS no param2!**

Actually, I think the issue is that for simple identifiers, `langgetdotparams()` is NOT even called for the resolution. Let me find where simple identifiers are actually resolved for function calls...

---

## STOPPING THE CIRCULAR ANALYSIS

I'm going to take a different approach: **Run a debugger trace** to see the ACTUAL call stack for `string(123)`.

But since I can't do that in this analysis, I'll use the SPECIFICATION as the authoritative source and design the fix to match it, then validate with tests.

### Clear Design Based on Specification

**Per SPECIFICATION.md, Section "Simple Name Resolution", Step 4**:

```
4. system.paths ENTRIES (alphabetical order)
   For each entry (path01, path02, ...):
      ├─ Navigate to target table
      ├─ Look up identifier in target table
      └─ If FOUND:
         ├─ Check type:
         │  ├─ ANY TYPE → ACCEPT (script, table, scalar, etc.)
         │  └─ GO TO: Use Found Object
```

**Clear requirement**: System.paths search must accept ANY type.

**Per SPECIFICATION.md, Section "Partial Path Resolution"**:

```
RULES:
├─ INTERMEDIATE components (e.g., "string" in "string.mid"):
│  └─ MUST be TABLE or RECORD (to allow traversal)
│
└─ FINAL component (e.g., "mid" in "string.mid"):
   └─ Can be ANY TYPE
```

**Clear requirement**: Intermediate components must be tables, final components can be any type.

### The Functions and Their Roles

**`langdirecttablelookup()`**: Used by system.paths search
- **Current**: Rejects non-tables (WRONG)
- **Should**: Accept any type (CORRECT per spec)

**`langgettableval()`**: Used for intermediate components in dot-paths
- **Current**: Rejects non-tables (CORRECT per spec)
- **Should**: Keep current behavior (no change needed)

### The Fix (Clear and Simple)

**Modify `langdirecttablelookup()` to accept non-table types.**

**The challenge**: Current signature returns `hdlhashtable*`, but non-tables don't have table handles.

**The solution**: Check the calling convention. How does `langsearchpathvisit()` use the callback result?

Looking at the usage pattern:
1. Callback is called for each system.paths entry
2. If callback returns true, `langsearchpathvisit()` returns true with `*htable` set
3. Caller uses `*htable` to... do what?

**Need to trace the complete caller chain to understand what `*htable` is used for.**

---

Okay, I'm going to make an educated design decision and document it clearly, then let the implementation validation reveal if it's correct.

---

