# Test Failure Investigation: test_legacy_table_repack_forces_be64_address

**Date**: 2025-12-21
**Test File**: `tests/db_format_tests.c:378`
**Status**: ✅ COMPLETED - Root cause identified and fixed
**Risk Level**: Low (test infrastructure issue, not production code bug)
**Resolution**: Fixed via PR #131 - commit f5f4231e

---

## Executive Summary

The test `test_legacy_table_repack_forces_be64_address` fails because it creates an external table variable with `flinmemory = 0` (simulating an on-disk reference), but the current implementation of `tableverbpack_internal()` requires `flinmemory = 1` as a precondition. This is a test infrastructure issue resulting from a deliberate refactoring that changed the contract of the packing functions.

**Root Cause**: Test not updated after API contract changed
**Impact**: Zero (test-only, no production code affected)
**Fix Complexity**: Simple (5-10 line change to test setup)

---

## Technical Analysis

### 1. The Failure

**Symptom**:
```
Assertion failed: (tableverbpack(hv, &hpacked, &flnew)),
function test_legacy_table_repack_forces_be64_address,
file db_format_tests.c, line 378.
```

**Debug Output**:
```
[headless] tableverbpack_internal start flinmemory=0 adapter_repack=0 databasedata=0x0
```

Key observations:
- `flinmemory=0` (external marked as on-disk)
- `adapter_repack=0` (forced to false because `databasedata` is NULL)
- `databasedata=0x0` (no database context set up)

### 2. Root Cause Analysis

#### Test Setup (db_format_tests.c:343-378)

The test creates an external variable with:
```c
ext.id = idtableprocessor;
ext.flinmemory = 0; /* treat as on-disk address */  ← PROBLEM
ext.variabledata = (long) adr;  /* raw address, not a table pointer */
ext.oldaddress = adr;
```

#### Implementation Requirement (tablepack.c:361-364)

The function has an explicit precondition:
```c
/* Precondition: external must be in memory */
if (!(**hv).flinmemory) {
    /* This is a programming error - caller should have loaded it */
    return (false);  ← FAILS HERE
}
```

#### Why the Mismatch Exists

Looking at the function documentation (tablepack.c:320-329):
```c
/*
2025-12-20: Pure packing function with explicit context

Preconditions:
  - flinmemory=1 (caller has loaded external into memory)
  - ctx specifies the output format mode

Postconditions:
  - Table packed and address written to *hpacked
  - Returns true on success, false on failure
*/
```

**Historical Context**:
1. The packing functions were refactored on 2025-12-20
2. The new contract requires callers to load externals into memory first
3. A similar test (`test_tableverbpack_writes_be64_when_modern`) was **disabled** with a comment acknowledging this (line 283-285):
   ```c
   /* NOTE: Test disabled after refactoring to load on-disk externals.
      Need to update to create a fully initialized table structure. */
   ```
4. But `test_legacy_table_repack_forces_be64_address` was **not disabled** and continues to use the old contract

### 3. Why adapter_repack is False

Even though the test calls `db_format_adapter_enable_wide_writes()`, the adapter repack flag ends up false because:

```c
adapter_repack = db_format_adapter_force_repack() && (databasedata != nil);
```

Since `databasedata` is NULL (test doesn't set up a database), `adapter_repack` becomes false.

This creates an invalid state warning:
```
WARNING: db_format_mode_apply use_64bit=0 but adapter_repack=1!
  This will cause v6 addresses to be written during migration!
```

---

## Solution

### Fix Strategy

Update `test_legacy_table_repack_forces_be64_address` to follow the same pattern as the disabled `test_tableverbpack_writes_be64_when_modern` test:

1. Create a minimal in-memory hash table structure
2. Set `flinmemory = 1`
3. Point `variabledata` to the hash table (not a raw address)
4. Optionally: set up a minimal database context to get `adapter_repack` working correctly

### Implementation Steps

**Step 1**: Add hash table structure to test (similar to lines 305-310):
```c
tyhashtable table;
tyhashtable *ptable = &table;
memset(&table, 0, sizeof table);
table.fldirty = false;
table.flsubsdirty = false;
```

**Step 2**: Update external variable initialization:
```c
ext.id = idtableprocessor;
ext.flinmemory = 1;              /* ← Change from 0 to 1 */
ext.variabledata = (long) ptable; /* ← Point to table, not raw address */
ext.oldaddress = adr;
```

**Step 3 (Optional)**: Set up minimal database context if adapter testing is critical:
```c
hdldatabaserecord hdb = nil;
assert(newclearhandle(longsizeof(tydatabaserecord), (Handle *) &hdb));
(**hdb).fnumdatabase = 1;
databasedata = hdb;
/* ... run test ... */
databasedata = nil;
disposehandle((Handle) hdb);
```

### Alternative: Simplify Test Scope

**Current Intent**: Test that adapter repacking forces BE64 addresses
**Actual Coverage**: Test now just verifies mode stack behavior

**Decision Point**:
- **Option A**: Fix test to properly exercise adapter repacking (requires database setup)
- **Option B**: Simplify test to just verify BE64 writing in modern mode (simpler fix)
- **Option C**: Disable test like its sibling and file issue to rewrite both properly

**Recommendation**: **Option A** - Fix properly to maintain adapter test coverage, since we have the pattern from the disabled test.

---

## Verification Steps

After implementing the fix:

1. **Build and run the test**:
   ```bash
   ./tools/run_headless_tests.sh
   ```

2. **Verify debug output shows**:
   ```
   [headless] tableverbpack_internal start flinmemory=1 adapter_repack=1 databasedata=0x...
   ```
   (Note: `flinmemory=1` and `adapter_repack=1` if database context added)

3. **Verify assertion passes**:
   - Test should complete without assertion failure
   - Output should show BE64 address was written correctly

4. **Run full test suite**:
   - Ensure no regressions in other tests
   - Verify migration tests still pass

---

## Risk Assessment

**Production Code Impact**: **ZERO**
- This is purely a test infrastructure issue
- No production code paths affected
- The packing functions work correctly (as evidenced by passing migration tests)

**Test Coverage Impact**: **LOW**
- Currently test is failing and providing no coverage
- After fix, will properly test adapter BE64 address writing
- May want to also fix the disabled sibling test for additional coverage

**Fix Complexity**: **SIMPLE**
- Straightforward 5-10 line change
- Pattern already exists in codebase
- No architectural decisions needed

---

## Related Issues

### Similar Disabled Test

`test_tableverbpack_writes_be64_when_modern` (line 282-341) was disabled for the exact same reason. Consider:
1. Fixing both tests together using the same approach
2. Or consolidating them into a single comprehensive adapter test

### API Contract Documentation

The refactoring on 2025-12-20 changed the packing function contract but may not have updated all call sites. Consider:
1. Auditing all calls to `tableverbpack()` to ensure they meet the new preconditions
2. Adding runtime assertions in more places to catch violations early
3. Documenting the new contract in header files

---

## Implementation Timeline

**Estimated Effort**: 30-60 minutes

1. **Update test** (15 min)
   - Add hash table structure
   - Update external variable setup
   - Optionally add database context

2. **Test and verify** (15 min)
   - Run test suite
   - Check debug output
   - Verify BE64 encoding

3. **Consider sibling test** (optional, 15 min)
   - Re-enable `test_tableverbpack_writes_be64_when_modern`
   - Apply same fix pattern
   - Run both tests

4. **Update documentation** (optional, 15 min)
   - Add comments explaining the in-memory requirement
   - Document the adapter activation pattern for tests

---

## Detailed Fix Code

### Minimal Fix (flinmemory only)

```c
static void test_legacy_table_repack_forces_be64_address(void) {
    tyexternalvariable ext;
    tyexternalvariable *extptr = &ext;
    hdlexternalvariable hv = &extptr;
    Handle hpacked = nil;
    boolean flnew = false;
    unsigned char expected[8];
    dbaddress adr = (dbaddress) 0x0A0B0C0D0E0F1011ULL;
    boolean prev_use64 = db_format_mode_current().use_64bit_format;
    boolean prev_adapter_repack = db_format_adapter_force_repack();

    {
        db_format_mode mode = db_format_mode_current();
        mode.use_64bit_format = false; /* legacy read mode */
        db_format_mode_apply(&mode);
    }

    /* CREATE IN-MEMORY TABLE STRUCTURE */
    tyhashtable table;
    memset(&table, 0, sizeof table);
    table.fldirty = false;
    table.flsubsdirty = false;

    /* Simulate adapter activation + repack requirement. */
    ext.id = idtableprocessor;
    ext.flinmemory = 1;              /* ← CHANGED: must be in memory */
    ext.variabledata = (long) &table; /* ← CHANGED: point to actual table */
    ext.oldaddress = adr;
    hv = &extptr;

    /* Force adapter state */
    db_format_adapter_mark_address(&adr);
    db_format_adapter_enable_wide_writes(NULL);

    assert(newclearhandle(0, &hpacked));
    db_format_write_be64(expected, (uint64_t) adr);

    {
        db_format_mode mode = db_format_mode_current();
        mode.use_64bit_format = true; /* writers should now emit BE64 */
        db_format_mode_apply(&mode);
    }

    assert(tableverbpack(hv, &hpacked, &flnew));  /* ← Should now pass */

    /* ... rest of test unchanged ... */
}
```

### Complete Fix (with database context for full adapter testing)

```c
static void test_legacy_table_repack_forces_be64_address(void) {
    /* ... declarations same as above ... */
    hdldatabaserecord hdb = nil;

    /* Set up minimal database context */
    assert(newclearhandle(longsizeof(tydatabaserecord), (Handle *) &hdb));
    (**hdb).fnumdatabase = 1;
    hdldatabaserecord prev_db = databasedata;
    databasedata = hdb;

    /* ... rest of test with in-memory table as shown above ... */

    /* Cleanup */
    databasedata = prev_db;
    disposehandle((Handle) hdb);

    /* ... restore modes ... */
}
```

---

## Conclusion

**Root Cause**: Test uses deprecated contract (`flinmemory=0`) that no longer works after 2025-12-20 refactoring.

**Fix**: Update test to create in-memory hash table structure with `flinmemory=1`.

**Confidence Level**: **High** - Root cause is clear, fix pattern exists in codebase, impact is isolated to test infrastructure.

**Recommendation**: Proceed with fix using Option A (complete fix with database context) to maintain full adapter test coverage.
