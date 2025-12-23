# Test Fix Results - Intermediate Progress Notes

**Date**: 2025-12-21
**Status**: 🗂️ ARCHIVED - Intermediate debugging notes (see test_fix_final_status.md for final resolution)
**Tests Modified**:
- `test_tableverbpack_writes_be64_when_modern` (re-enabled)
- `test_legacy_table_repack_forces_be64_address` (fixed)

---

## Changes Made

### Test 1: test_tableverbpack_writes_be64_when_modern
**Change**: Removed early `return` statement to re-enable the test
- Line 285: Removed `return;`
- Updated comment to reflect test purpose

### Test 2: test_legacy_table_repack_forces_be64_address
**Changes**: Updated to use current packing API contract
1. Added in-memory hash table structure
2. Changed `ext.flinmemory` from `0` to `1`
3. Changed `ext.variabledata` to point to table structure instead of raw address
4. Added database context setup (`databasedata = hdb`)
5. Added cleanup code to restore `databasedata` and dispose handle

---

## Test Results

**Status**: ❌ SEGMENTATION FAULT

**Output**:
```
[headless] tableverbpack_internal start flinmemory=1 adapter_repack=0 databasedata=0x0
Segmentation fault: 11
```

**Analysis**:
- The test successfully passed the `flinmemory` check (debug output confirms `flinmemory=1`)
- But it crashes during the packing operation itself
- The crash occurs in `test_tableverbpack_writes_be64_when_modern` (runs first in test order)

---

## Root Cause Analysis

The minimal hash table structure created in the tests is insufficient for the packing code:

```c
tyhashtable table;
memset(&table, 0, sizeof table);
table.fldirty = false;
table.flsubsdirty = false;
```

The packing path (`tablepacktable_internal` → `hashpacktable_context`) likely requires:
- Properly initialized hash table internal structures
- Valid hash buckets/chains
- Properly allocated handles for internal data structures
- Other fields that are NULL/zero in our minimal test setup

---

## Issue

The tests need a **fully initialized hash table**, not just a zeroed-out structure. Creating a fully initialized hash table for testing requires understanding:

1. What minimum initialization is needed for packing to succeed?
2. Do we need to call `hashnewtable()` or similar initialization functions?
3. What invariants does the packing code expect?

---

## Options

### Option A: Minimal Initialization Research
Research what minimum fields need to be set for packing to work:
- Look at `hashnewtable()` to see what it initializes
- Determine minimum viable initialization for test purposes
- Risk: May still miss required invariants

### Option B: Use Real Initialization Functions
Use the actual hash table creation APIs:
```c
hdlhashtable htable = nil;
if (!hashnewtable(&htable))
    return false;  // test setup failure
```
This guarantees proper initialization but requires cleanup.

### Option C: Disable Both Tests Properly
Add clear comments explaining why they're disabled and file an issue:
- These tests require full hash table initialization
- Current packing API contract requires in-memory loaded tables
- Need proper test infrastructure for table packing tests
- File GitHub issue to track proper implementation

### Option D: Mock the Packing Path
If we only want to test the BE64 address writing (not the full packing):
- Skip the actual table packing
- Create a pre-packed handle with dummy data
- Only verify the address trailer bytes are written correctly
- This tests the BE64 encoding without needing full table infrastructure

---

## Recommendation

**Option B**: Use real initialization functions

Rationale:
1. Provides most complete test coverage
2. Tests the actual production code path
3. Avoids guessing at minimum initialization requirements
4. Makes tests more maintainable (use public APIs, not internal knowledge)

This requires:
- Calling `hashnewtable()` to create a valid table
- Properly disposing the table after the test
- May need to add some minimal data to make dirty flags work correctly

---

## Next Steps

**Before proceeding, discuss with user**:
1. Which option is preferred?
2. Is it worth the complexity to test table packing, or should we just test BE64 address encoding separately?
3. Are there existing test utilities for creating test hash tables we should use?

**No production code changes made** - only test infrastructure modified per user request.
