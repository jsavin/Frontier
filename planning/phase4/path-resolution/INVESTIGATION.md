# Path Resolution Investigation - string(123) Regression

**Status**: Complete - Specification Ready
**Started**: 2026-01-25
**Issue**: `string(123)` broken after PR #336

---

## Executive Summary

Git bisect identified that PR #336 ("Fix system.paths migration and address value initialization") introduced a regression where `string(123)` stopped working. The error indicates that `string` is resolving to a TABLE instead of a SCRIPT.

**Root Cause**: PR #336 fixed incorrect path handling, but this uncovered an underlying bug in path resolution that was previously masked. The fix to `langgettableval()` changed it to search inside the provided table first, but it uses `tablevaltotable()` which rejects non-table types. This causes `string` (a SCRIPT) to be rejected, continuing the search until it finds the TABLE at `system.verbs.builtins.string`.

**Solution**: Modify `langgettableval()` to accept ANY type for final components while maintaining table-only requirement for intermediate components.

**Specification**: See [`SPECIFICATION.md`](./SPECIFICATION.md) for complete technical specification.

---

## Git Bisect Results

### Timeline

| Commit | Status | Notes |
|--------|--------|-------|
| `dc14a40c` | ✅ GOOD | feat: Implement P0a general-purpose parameterized callback infrastructure |
| `b55865ab` | ✅ GOOD | Fix: Runtime Error Halting for Table Index Operations |
| `ddfff279` | ❌ **FIRST BAD** | Fix system.paths migration and address value initialization (#336) |
| `8b7824a7` | ❌ BAD | fix: Correct path entry name matching in langsearchpathvisit() |
| `f11a1a46` | ❌ BAD | Fix monitor script documentation inconsistency |
| `358db4a6` | ❌ BAD | feat: Support positional .root/.root7 arguments in CLI (current develop) |

### Test Command

```bash
./frontier-cli/frontier-cli -e "string(123)"
```

**Expected**: `"123"`
**Actual (after ddfff279)**: Error - "Can't call string because it isn't a script"

---

## Error Analysis

### Before PR #336 (✅ Working)

```bash
# Commit b55865ab
./frontier-cli/frontier-cli -e "string(123)"
# Output: "123"
```

Path resolution:
1. Search system.paths in order
2. path01 → `@system.verbs.globals` (priority 1)
3. Find `string` as SCRIPT → execute coercion
4. Returns "123"

### After PR #336 (❌ Broken)

```bash
# Commit ddfff279
./frontier-cli/frontier-cli -e "string(123)"
# Output: [lang-ERROR] Can't call string because it isn't a script.
```

Path resolution:
1. Search changed due to `langgettableval()` modifications
2. `string` resolves to `system.verbs.builtins.string` (TABLE, not SCRIPT)
3. Attempt to CALL a table fails
4. Error: "Can't call string because it isn't a script"

---

## PR #336 Changes

Commit `ddfff279` modified three key areas:

### 1. Address Value Migration (tablestructure.c)

**File**: `Common/source/tablestructure.c:367-402`

**Change**: Fix address value structure during migration - dispose old value and create new with correct local name + htable.

**Impact on string(123)**: Likely none - this is database migration logic.

### 2. system.paths Initialization (tablestructure.c)

**File**: `Common/source/tablestructure.c:467-484`

**Change**: Only populate system.paths if newly created, don't pollute existing entries from migration.

**Impact on string(123)**: Possibly relevant - affects which paths are searched.

### 3. langgettableval() Search Order (langvalue.c) ⚠️ **LIKELY CULPRIT**

**File**: `Common/source/langvalue.c:3651-3673`

**Change**: Search inside provided table FIRST, then fall back to `langexternalgettable()`.

**Impact on string(123)**: HIGH - This directly affects nested table child resolution.

---

## Detailed Analysis: langgettableval() Changes

### Original Behavior (Before PR #336)

```c
static boolean langgettableval(hdlhashtable htable, bigstring bsname, hdlhashtable *hval) {
    pushhashtable(htable);
    boolean fl = langexternalgettable(bsname, hval);  // Only external lookup
    pophashtable();
    return fl;
}
```

**Search Pattern**:
- Only searched external EFP tables via `langexternalgettable()`
- Did NOT search inside the provided `htable`

### New Behavior (After PR #336)

```c
static boolean langgettableval(hdlhashtable htable, bigstring bsname, hdlhashtable *hval) {
    pushhashtable(htable);

    // First: search INSIDE the provided table
    if (hashtablelookup(htable, bsname, &val, &hnode)) {
        fl = tablevaltotable(val, hval, hnode);  // ❌ PROBLEM: Rejects non-tables
    }
    else {
        // Fallback: external lookup (preserves backward compatibility)
        fl = langexternalgettable(bsname, hval);
    }

    pophashtable();
    return fl;
}
```

**Search Pattern**:
- First searches INSIDE the provided table
- Falls back to external lookup if not found
- **BUG**: Uses `tablevaltotable()` which rejects non-table types

---

## Investigation Steps Completed

### ✅ Step 1: Trace langgettableval() Call Chain

**Finding**: `langgettableval()` has **ONLY ONE call site** at line 4047 in `langgetdotparams()`.

**Context**:
- `langgetdotparams()` recursively traverses dot-paths
- When `hsubtable != nil`, it calls `langgettableval()` to find the next component
- This is the intermediate/final component traversal scenario

### ✅ Step 2: Understand Resolution Flow

**Decision Tree Created**: Complete specification of name/path resolution in [`SPECIFICATION.md`](./SPECIFICATION.md)

**Key Insights**:
- Resolution has 6 search steps (locals → with → system root → system.paths → guest DBs → not found)
- Usage dispatch (call/assign/etc.) is EXTERNAL to `langgettableval()`
- The bug is in type checking, not in search order

### ✅ Step 3: Identify the Real Bug

**Root Cause**: `langgettableval()` uses `tablevaltotable()` which rejects non-table types.

**Why It's Wrong**:
- For FINAL components: Should accept ANY type (script, table, scalar, etc.)
- For INTERMEDIATE components: Should accept ONLY tables/records (to allow traversal)

### ✅ Step 4: Design Proper Fix

**Fix Strategy**: Modify `langgettableval()` to distinguish between final and intermediate components.

**Options**:
1. Add parameter to indicate final vs intermediate
2. Create separate functions
3. Check at call site

**See [`SPECIFICATION.md`](./SPECIFICATION.md) for complete implementation strategy.**

---

## Next Actions

1. **Pass SPECIFICATION.md to system-architect agent**
   - Complete technical specification ready
   - Includes decision tree, root cause analysis, and implementation strategy

2. **Agent will perform code deep-dive**
   - Analyze `langgetdotparams()` recursion
   - Determine best fix approach
   - Implement the fix

3. **Validation**
   - All test cases in SPECIFICATION.md
   - Ensure PR #336 compatibility

---

## Files for Reference

| File | Purpose |
|------|---------|
| [`SPECIFICATION.md`](./SPECIFICATION.md) | Complete technical specification for implementation |
| `bisect_string_verb.sh` | Git bisect script to find first bad commit |
| `docs/VERB_RESOLUTION_ARCHITECTURE.md` | Existing architecture documentation |

---

## References

- **Git bisect script**: `bisect_string_verb.sh`
- **First bad commit**: `ddfff279`
- **PR #336**: Commit ddfff279 - "Fix system.paths migration and address value initialization"
- **Previous investigation** (failed approach): `planning/phase4/verb-resolution-failed/`
- **Technical Specification**: [`SPECIFICATION.md`](./SPECIFICATION.md)
