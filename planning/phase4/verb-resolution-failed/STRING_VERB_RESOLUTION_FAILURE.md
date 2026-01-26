# String Verb Resolution Failure

**Date**: 2026-01-25
**Issue**: `string(123)` calls fail with "unknown function" error
**Status**: Resolved
**Working Directory**: `/Users/jake/dev/jsavin/Frontier-fix-string-verb-resolution`
**Branch**: `feature/fix-string-verb-resolution`

---

## Problem Statement

Calling verbs with terse (non-dotted) names fails when the verb exists as a script in `system.verbs.globals` or similar path entries.

**Failing Examples**:
- `string(123)` - Unknown function error
- `defined(string)` - Returns false (should be true)
- `defined(webserver.init)` - Incorrect lookup behavior

**Working Examples**:
- `string.mid("hello", 1, 3)` - Works correctly
- Fully qualified paths work correctly

---

## Root Cause

The verb resolution callback (`langdirecttablelookup`) was searching for **tables only**, and returning false when it found a non-table item (like a script). This caused path searches to skip over valid script verbs.

---

## Investigation Timeline

### Initial Analysis

1. **Symptom**: `string(123)` fails with "unknown function" error
2. **Observation**: `string.mid(...)` works correctly
3. **Hypothesis**: The difference is that `string.mid` searches for a table named "string", while `string(...)` searches for a script named "string"

### Initial Fix Attempt (INCORRECT)

We initially attempted to fix this by:
1. Adding an `is_final_component` parameter to the callback signature
2. Having the callback return the **parent table** when it found a non-table final component
3. Letting the caller look up the value in the parent table

**This approach was fundamentally wrong** - it broke the architectural separation of concerns.

See `EXECUTION_PLAN.md` for details of this failed approach.

### Legacy Investigation (CORRECT)

After the 4-parameter approach failed, we investigated the legacy tedchoward Frontier codebase to understand the correct architecture.

**Key Discovery**: The callback is NOT responsible for handling non-table final components. The callback should ONLY return tables (or false for non-tables). The value lookup happens at a higher level in `langgethandlercode`.

See `LEGACY_INVESTIGATION_REPORT.md` in the repository root for complete analysis.

---

## Architectural Understanding

### Two Search Mechanisms

**Mechanism 1: Path search** (`langsearchpathvisit`)
- Purpose: Iterate through `system.paths` entries
- Callback: Looks up name in each path table
- Returns: Which table contains the name (for tables only)

**Mechanism 2: Identifier search** (`langgethandlercode`)
- Purpose: Search for identifier in a specific table
- Method: `langfindsymbol` or `hashlookupnode`
- Handles: Looking up the actual value (script, table, etc.)

### The Callback's Role

The callback used by `langsearchpathvisit` has ONE job:
- **For dotted names** (like `string.mid`): Find intermediate tables
- **Return**: The table if found, false otherwise
- **Don't handle**: Non-table final components (that's `langgethandlercode`'s job)

### Why Simple Identifiers Work Differently

For `string(123)`:
1. Path search callback is called from `langgethandlervisit`
2. Callback checks if "string" exists in path table (doesn't need to return parent)
3. `langgethandlercode` does the actual lookup using `intable` parameter
4. No need for callback to return parent table

For `string.mid(...)`:
1. Path search callback is called from `langgetdotparams`
2. Callback finds "string" table in `system.verbs.builtins`
3. Returns the table to `langgetdotparams`
4. `langgetdotparams` then looks up "mid" in that table

---

## The Correct Fix

The fix is simpler than we initially thought:

1. **Keep callback signature** at 3 parameters (don't add `is_final_component`)
2. **Restore callback semantics**: Return table only, fail on non-table
3. **Trust the architecture**: `langgethandlercode` handles identifier lookup

---

## Lessons Learned

### Misunderstanding #1: Callback Scope

**Wrong assumption**: The callback needs to handle both tables and non-table final components.

**Reality**: The callback is ONLY for finding tables. Non-table lookups happen at a different level.

### Misunderstanding #2: Parameter Passing

**Wrong assumption**: The callback is called with the final component name.

**Reality**: For simple identifiers, the callback is never called with that name. The name search happens in `langgethandlercode` using the `intable` parameter.

### Misunderstanding #3: Architectural Role

**Wrong assumption**: We need to modify the callback to return parent tables for the caller to search.

**Reality**: The caller (`langgethandlercode`) already has access to `intable` and does the search directly. The callback is just for path iteration.

---

## Testing Verification

### Before Fix

```bash
./frontier-cli/frontier-cli -e 'string(123)'
# Error: Unknown function "string"

./frontier-cli/frontier-cli -e 'defined(string)'
# Returns: false
```

### After Fix

```bash
./frontier-cli/frontier-cli -e 'string(123)'
# Returns: "123"

./frontier-cli/frontier-cli -e 'defined(string)'
# Returns: true

./frontier-cli/frontier-cli -e 'string.mid("hello", 1, 3)'
# Returns: "ell"
```

All integration tests pass.

---

## Resolution

**Status**: Resolved

**Fix Implemented**: Based on legacy investigation findings

**Reference**: See `/Users/jake/dev/jsavin/Frontier-fix-string-verb-resolution/LEGACY_INVESTIGATION_REPORT.md` for complete implementation details.

**PR**: (to be created after implementation and testing)

---

## References

- `LEGACY_INVESTIGATION_REPORT.md` - Complete legacy code analysis
- `EXECUTION_PLAN.md` - Initial (incorrect) fix approach (deprecated)
- `INVESTIGATION_REPORT.md` - Initial investigation findings
- Issue tracking: (to be added when PR is created)

---

**Report End**
