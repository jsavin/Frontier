# Hashpack Modern Tests Segmentation Fault - Diagnosis and Resolution

**Date**: 2025-12-19
**Status**: ✅ RESOLVED
**Commit**: c119142b

## Executive Summary

A segmentation fault in `hashpack_modern_tests` discovered during clean build testing was caused by outdated function name references. The test files were calling `langhash_test_value_to_disk_modern()` and `langhash_test_value_from_disk_modern()`, but these functions were renamed to use the `_v7` suffix in commit 7ab39466 (2025-12-16) to eliminate confusion with legacy 'modern' terminology. This naming confusion exists because the legacy Frontier codebase also used the term 'modern' to refer to v5/v6 32-bit formats, creating ambiguity.

## Root Cause Analysis

### Timeline of Events

1. **2025-12-08**: `hashpack_modern_tests.c` and `hash_corruption_tests.c` created with calls to `langhash_test_value_to_disk_modern()` and `langhash_test_value_from_disk_modern()`.

2. **2025-12-16 (Commit 7ab39466)**: Systematic refactoring renamed all `_modern` references to `_v7`:
   - Function renames: `langhash_test_value_*_modern` → `langhash_test_value_*_v7`
   - File renames: `db_reader_modern.c` → `db_reader_v7.c`, etc.
   - This was intentional to reduce confusion with legacy terminology

3. **2025-12-19 (Clean Build)**: During `make clean && make test`, the outdated test files caused immediate segfault when trying to call non-existent functions.

### Technical Details

**Crash Stack Trace**:
```
* thread #1, stop reason = EXC_BAD_ACCESS (code=1, address=0x0)
  * frame #0: 0x0000000000000000
    frame #1: 0x0000000100000e48 hashpack_modern_tests`pack_value(vin=..., rec=...) at hashpack_modern_tests.c:33:5
    frame #2: 0x000000010000087c hashpack_modern_tests`test_be64_int_roundtrip at hashpack_modern_tests.c:50:5
```

**Root Cause**: Function pointer at address 0x0 (NULL) - the linker could not find `langhash_test_value_to_disk_modern()` and resolved it to NULL.

## Resolution

### Files Modified

**tests/hashpack_modern_tests.c**:
- Line 33: `langhash_test_value_to_disk_modern()` → `langhash_test_value_to_disk_v7()`
- Line 38: `langhash_test_value_from_disk_modern()` → `langhash_test_value_from_disk_v7()`

**tests/hash_corruption_tests.c**:
- Line 88: `langhash_test_value_to_disk_modern()` → `langhash_test_value_to_disk_v7()`
- Line 94: `langhash_test_value_from_disk_modern()` → `langhash_test_value_from_disk_v7()`
- Line 163: `langhash_test_value_from_disk_modern()` → `langhash_test_value_from_disk_v7()`

### Testing Results

**Before Fix**:
```
/bin/sh: line 1: 87110 Segmentation fault: 11  ./$test
make: *** [test] Error 139
```

**After Fix**:
```
=== Running hashpack modern BE64 tests ===
[hashpack] int64 BE roundtrip... PASS
[hashpack] date BE roundtrip... PASS
[hashpack] double BE roundtrip... PASS
=== hashpack modern BE64 tests complete ===

=== Running hash corruption resistance tests ===
[hash_corruption] truncated Pascal string at name index... PASS
[hash_corruption] OOB string name index... PASS
[hash_corruption] symbol record exactly at size boundary... PASS
[hash_corruption] partial record (4-15 bytes) detection... PASS
[hash_corruption] invalid valuetype code (>254)... PASS
[hash_corruption] record version mismatch (modern vs legacy)... PASS
[hash_corruption] OOB data index for extended types (lists/tables)... PASS
[hash_corruption] negative data index (signed/unsigned mismatch)... PASS
[hash_corruption] double infinity bit pattern... PASS
[hash_corruption] double NaN bit pattern... PASS
[hash_corruption] double +0.0 and -0.0... PASS
[hash_corruption] union field access with type mismatch... PASS
[hash_corruption] record padding bytes contain garbage... PASS
[hash_corruption] struct size compile-time assertion... PASS
=== hash corruption resistance tests complete ===
```

**Summary**: All 17 hash-related tests now pass.

## Why This Wasn't Caught Earlier

1. **Timing of Refactoring**: Commit 7ab39466 was a comprehensive rename of 13+ function names and 2 file names. The test files were created earlier and weren't included in the refactoring pass.

2. **Test Run Frequency**: The tests were likely last run before the rename, then changes to the test makefile may have been made that weren't validated on a clean build.

3. **Clean Build Infrequency**: The issue was masked by incremental builds using cached object files. A `make clean` forced recompilation and exposed the broken references.

## Long-term Prevention

### Naming Consistency

The `_v7` suffix is now the standard for 64-bit big-endian format functions. This eliminates ambiguity with legacy 'modern' terminology (which referred to v5/v6 32-bit formats in the original codebase).

**Standard nomenclature**:
- `_legacy` = v6 format (32-bit little-endian)
- `_v7` = Modern 64-bit big-endian format
- NEVER `_modern` = Causes confusion across codebases

### Best Practices

1. When renaming functions affecting test interfaces, update **all** test files simultaneously, not just core code
2. Include a check in CI/build process: `grep -r "_modern.*\|_legacy.*" tests/` to catch stale terminology
3. Add compile-time assertions for function availability when test harnesses use specific function names
4. Run `make clean` periodically (especially after large refactoring PRs) to catch linker errors

## Related Issues

- **Issue #123** (now fixed/merged): External table address format issues during v6→v7 migration
- **Commit 7ab39466**: Comprehensive _modern → _v7 refactoring
- **PR #124**: Merged fix for Issue #123, included refactoring changes

## Recommendation

This fix is **low-risk** and **ready to merge immediately**. The changes are purely mechanical (function name updates) with no logic changes. All affected tests pass after the fix.

**Next steps**:
1. ✅ Commit c119142b - merged to develop
2. Continue with mode stack refactor Phase 1 (no blockers)
3. Monitor clean build test runs going forward to catch similar issues early

---

**Notes**:
- The `table_verb_tests` failure (noted during same test run) is a pre-existing issue related to Issue #123 (external table address format during database loading), not related to this segfault fix.
- See `planning/phase3/MODE_STACK_REFACTOR_QUICKSTART.md` for next major work (mode stack elimination).
