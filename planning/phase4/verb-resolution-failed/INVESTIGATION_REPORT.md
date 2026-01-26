# String Verb Resolution Investigation Report

**Date**: 2026-01-25
**Issue**: `string(123)` fails with "unknown function" error
**Working Directory**: `/Users/jake/dev/jsavin/Frontier-fix-string-verb-resolution`
**Branch**: `feature/fix-string-verb-resolution`

---

## Problem Statement

When calling verbs with terse (non-dotted) names, Frontier fails to find them even when they exist in `system.verbs.globals` or other path entries.

**Failing Cases**:
```usertalk
string(123)              // Error: Unknown function "string"
defined(string)          // Returns false (should be true)
defined(webserver.init)  // Incorrect behavior
```

**Working Cases**:
```usertalk
string.mid("hello", 1, 3)  // Works - finds table, then verb
system.verbs.globals.string(123)  // Works - fully qualified
```

---

## Initial Investigation

### Symptom Analysis

The difference between working and failing cases:
- **Dotted names** (`string.mid`) work because they search for a TABLE named "string"
- **Simple names** (`string`) fail because they search for a script named "string"
- Path search callback (`langdirecttablelookup`) only returns tables, not scripts

### Code Path Exploration

**For `string.mid(...)`**:
1. Parse: `functionop(dotop(identifierop("string"), identifierop("mid")), params)`
2. `langgetdotparams` searches paths for "string" TABLE
3. Callback finds `system.verbs.builtins.string` table
4. Returns table to caller
5. Caller looks up "mid" in that table
6. ✅ Success

**For `string(...)`**:
1. Parse: `functionop(identifierop("string"), params)`
2. `langhandlercall` searches for handler code
3. Path search tries to find "string" in path entries
4. Callback finds "string" in `system.verbs.globals` but it's a SCRIPT, not a table
5. Callback returns false (not a table)
6. ❌ Path search fails, "unknown function" error

### Root Cause Hypothesis

The callback `langdirecttablelookup` is designed to return tables only. When it encounters a non-table value (like a script), it returns false, causing path searches to skip over valid script verbs.

---

## Initial Fix Approach (Later Proven Wrong)

### Strategy

We initially hypothesized that we needed to:
1. Add an `is_final_component` parameter to the callback
2. Have the callback return the **parent table** when it finds a non-table final component
3. Let the caller look up the value in the parent table

### Proposed Implementation

**Callback signature change**:
```c
// Before (3 parameters)
typedef boolean (*tysearchpathcallback) (hdlhashtable, bigstring, hdlhashtable *);

// After (4 parameters)
typedef boolean (*tysearchpathcallback) (hdlhashtable, bigstring, hdlhashtable *, boolean is_final);
```

**Callback logic change**:
```c
static boolean langdirecttablelookup (
    hdlhashtable htable,
    bigstring bsname,
    hdlhashtable *hresult,
    boolean is_final_component
) {
    // ... lookup code ...

    /* Try to convert to table */
    if (tablevaltotable (val, hresult, hnode)) {
        return (true);  // Found a table
    }

    /* NEW: If final component and non-table, return parent */
    if (is_final_component) {
        *hresult = htable;  // Return parent table
        return (true);
    }

    /* Intermediate component must be table */
    return (false);
}
```

### Call Site Updates

1. `langgetdotparams` - Pass `is_final` based on dot position
2. `langhandlercall` - Pass `is_final=true` when searching for handlers

---

## Update: Initial Fix Approach Was Incorrect

**Status**: The 4-parameter approach described above was fundamentally wrong.

### What We Misunderstood

**Wrong assumption**: The callback needs to handle both table and non-table final components.

**Reality**: The callback should ONLY return tables. The lookup for non-table items happens at a DIFFERENT LEVEL in the call stack.

### The Architectural Problem

By trying to return the parent table for non-table items, we were:

1. **Breaking separation of concerns**: The callback's job is to find tables, not to handle non-table lookups
2. **Confusing two mechanisms**: Path search (callback's domain) vs. identifier search (caller's domain)
3. **Violating the design**: The callback was never meant to return parent tables

### How It Actually Works (Legacy Behavior)

**For `string(123)`**:
1. `langhandlercall` searches paths using `langgethandlervisit` callback
2. `langgethandlervisit` calls `langgethandlercode` with `intable` = path table
3. `langgethandlercode` calls `langgetdotparams` with identifierop("string")
4. `langgetdotparams` returns `htable=nil, bsname="string"` (no table specified)
5. **Critical**: `langgethandlercode` sees `htable==nil` and searches for "string" in `intable`
6. Finds the script! ✅

**For `string.mid(...)`**:
1. `langgetdotparams` is called directly (not through handler search)
2. Searches paths for "string" TABLE using `langgettableval` callback
3. Callback finds table, returns it
4. `langgetdotparams` then extracts "mid" from param2
5. Returns `htable=<string table>, bsname="mid"`
6. Caller looks up "mid" in the table ✅

### The Key Insight

The callback is NOT responsible for identifier lookup. It's only responsible for:
- **Dotted names**: Finding intermediate tables
- **Simple names**: The callback is used indirectly through `langgethandlervisit`, but the actual identifier lookup happens in `langgethandlercode` using the `intable` parameter

### Legacy Verification

We investigated the legacy tedchoward Frontier codebase and confirmed:
- Callback signature: 3 parameters (no `is_final_component`)
- Callback semantics: Return table only, fail on non-table
- Identifier lookup: Happens in `langgethandlercode` with `intable` parameter

**Complete analysis**: See `LEGACY_INVESTIGATION_REPORT.md` in the repository root.

---

## Correct Fix Approach

Based on legacy investigation, the correct fix is:

1. **Keep** 3-parameter callback signature (don't add `is_final_component`)
2. **Restore** legacy callback semantics: return table only, fail on non-table
3. **Trust** the architecture: `langgethandlercode` handles identifier lookup through `intable`

**Implementation details**: See `LEGACY_INVESTIGATION_REPORT.md` section 7.

---

## Testing Strategy

### Test Cases

1. **Simple identifier**: `string(123)`
   - Should find script in `system.verbs.globals`
   - Should return "123"

2. **Dotted name**: `string.mid("hello", 1, 3)`
   - Should find table `system.verbs.builtins.string`
   - Should find verb "mid" in that table
   - Should return "ell"

3. **defined() with identifier**: `defined(string)`
   - Should return true

4. **defined() with dotted name**: `defined(string.mid)`
   - Should return true

5. **Complex path**: `defined(webserver.init)`
   - Should resolve correctly through paths

### Verification Commands

```bash
cd /Users/jake/dev/jsavin/Frontier-fix-string-verb-resolution

# Before fix (should fail)
./frontier-cli/frontier-cli -e 'string(123)'
./frontier-cli/frontier-cli -e 'defined(string)'

# After fix (should succeed)
./frontier-cli/frontier-cli -e 'string(123)'
# Expected: "123"

./frontier-cli/frontier-cli -e 'defined(string)'
# Expected: true

# Integration tests
cd tests && make test-integration
# All tests should pass
```

---

## Lessons Learned

### Investigate Legacy First

We should have started with legacy investigation instead of trying to fix based on assumptions. The legacy code is the proven implementation that worked for 25+ years.

### Don't Add Complexity

We tried to add a parameter (`is_final_component`) to solve the problem. The correct solution was simpler: trust the existing architecture and restore legacy semantics.

### Separation of Concerns

The architecture has clear separation:
- **Callback**: Finds tables during path iteration
- **Caller** (`langgethandlercode`): Looks up identifiers in tables

Don't try to merge these concerns.

### Parameter Passing Context

For simple identifiers, the callback is NOT called with the identifier name directly. It's called with path entry names, and the identifier lookup happens through the `intable` parameter to `langgethandlercode`.

Understanding this flow is critical to understanding why the callback shouldn't handle non-table items.

---

## References

- `LEGACY_INVESTIGATION_REPORT.md` - Complete legacy code analysis and correct implementation approach
- `EXECUTION_PLAN.md` - Initial (incorrect) fix approach (deprecated)
- `STRING_VERB_RESOLUTION_FAILURE.md` - Problem statement and resolution summary
- Legacy codebase: `/Users/jake/dev/tedchoward/Frontier`
- Key files:
  - `Common/source/langvalue.c` - Path search and identifier lookup
  - Lines 3661-3727: `langsearchpathvisit`
  - Lines 8078-8185: `langgethandlercode`
  - Lines 3768-3874: `langgetdotparams`

---

## Conclusion

The initial investigation correctly identified the problem (callback only returning tables) but proposed the wrong solution (adding `is_final_component` parameter and returning parent tables).

The legacy investigation revealed the correct approach: restore legacy callback semantics and trust the existing architecture to handle identifier lookups through the `intable` parameter.

**Next Steps**: Implement the fix according to `LEGACY_INVESTIGATION_REPORT.md` section 7.

---

**Report End**
