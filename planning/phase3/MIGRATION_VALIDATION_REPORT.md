# Migration Validation Report

**Date**: 2025-12-17
**Status**: PARTIAL PASS - External handles fixed, but table access has data format issue
**Risk #3 Assessment**: Complete - Need follow-up investigation

---

## Executive Summary

The v6→v7 database migration is **partially working**:
- ✅ Migration completes successfully
- ✅ Source database remains unchanged
- ✅ External objects are defined and visible
- ✅ External handle mismatch fixed (PR #117)
- ❌ Reading table contents fails with address normalization errors

**New Risk Identified**: Table address conversion in v7 format may have issues beyond the external handle problem.

---

## Test Results

### 1. Migration Completion (✅ PASS)

```bash
$ ./tests/save_migration_tests
save_migration_tests: migration applied (v6 -> v7) output=test_save_migration-v7.root
```

- Source database: `databases/Frontier-v6.root` (v6 format)
- Migrated database: `test_save_migration-v7.root` (v7 format)
- Migration function: `migrate_32bit_to_64bit()` (called via test harness)

**Status**: Migration process works correctly.

---

### 2. Source Database Integrity (✅ PASS)

```bash
Before migration: b993ad8fbe5dd7c1decd88adb08a1d66  databases/Frontier-v6.root
After migration:  b993ad8fbe5dd7c1decd88adb08a1d66  databases/Frontier-v6.root
```

**Status**: Source database unchanged (as intended).

---

### 3. External Object Definition (✅ PASS)

```bash
$ FRONTIER_HEADLESS_SKIP_STARTUP=1 ./frontier-cli/frontier-cli \
    --system-root tests/test_save_migration-v7.root \
    -e "defined(system.verbs.globals)"

Result: true
```

**Status**: External objects are accessible to the runtime. The external handle fix in PR #117 is working - external objects now correctly point to v7 database instead of v6.

**Evidence from logs**:
```
[hl] langfindsymbol enter globals current=0x6000010a1418
[hl] langfindsymbol inspect table=0x6000010a1418 name=globals
[hl] langfindsymbol hit globals table=0x6000010a1418 node=0x6000010a1d18
Result: true
```

---

### 4. External Object Content Access (❌ FAIL)

```bash
$ FRONTIER_HEADLESS_SKIP_STARTUP=1 ./frontier-cli/frontier-cli \
    --system-root tests/test_save_migration-v7.root \
    -e "sizeOf(system.verbs.globals)"

ERROR: Execution error: Script execution failed
```

**Error Details**:
```
[headless] tableverbinmemory dbpush database=0x6000031dcdf8 adr=0x91f819
[headless] dbnormalizeaddress failed for adr=0x91f819
[headless] dbread seek failed fnum=2 adr=0x91f819 bytes=12 saveas=0 source=0x0 current=0x6000031dcdf8 dest=0x0
[headless] dbrefhandle failed adr=0x91f819
```

**Affected operations**:
- `sizeOf(system.verbs.globals)` - fails with address normalization error
- `workspace.test="hello"; workspace.test` - same error
- Any table access attempt fails at `dbnormalizeaddress()`

**Status**: Tables defined but contents inaccessible.

---

## Root Cause Analysis

The error pattern shows:
1. External object **definition** works (returns `true`)
2. External object **navigation** works (finds the symbol in the table)
3. External object **content access** fails (address `0x91f819` not normalizable in v7 DB)

**Hypothesis**: The packed table records in the v7 database contain 64-bit addresses that reference content OUTSIDE the v7 file format expectations, or the address translation layer has a bug.

**Related Code**:
- `db_format.c`: `db_format_fixup_external_handles()` - Fixed external handle references (commit 7208ae60)
- `langhash.c`: Address normalization and table unpacking
- `db_reader_modern.c`: v7 database address resolution
- `tablepack.c`: Table packing/unpacking (may need format audit)

---

## Impact Assessment

**Current Status**: Migration is **partially functional**.

**For verb porting (Phase 3 priority)**:
- File verbs still risky (#87 - file routing) - tables accessible would be critical
- System verbs partially accessible (namespace works, but data access fails)
- External access pattern works but actual data reading fails

**What's working**:
- Database format upgrade (v6 → v7)
- External object structure recognition
- Namespace resolution
- External handle routing (our PR #117 fix)

**What's broken**:
- Reading table contents from externals
- Any verb that needs to access migrated database data

---

## Required Follow-Up

**P0 Investigation Task**: Identify why table address conversion fails in v7 format

Potential causes:
1. Table record format conversion incomplete during migration
2. Address translation layer broken for migrated content
3. String pack/unpack issues with v7 format
4. File header offset calculations incorrect

**Related Issues**:
- Issue #116 was about external handle mismatch → **FIXED by PR #117**
- This new data access issue might be part of #116 or a new issue

---

## Conclusion

The external handle mismatch that blocked v7 database access has been **successfully fixed**. However, a new issue has been discovered: table content cannot be read from the migrated database due to address normalization failures. This needs investigation before verb implementation can proceed with confidence.

**Recommendation**: Create GitHub issue to track table content access failures (P0, blocks verb porting). Do not proceed with verb implementation until this is resolved.

---

## Test Commands

To reproduce:
```bash
# Build and run migration test
make -C tests save_migration_tests
./tests/save_migration_tests

# Test external definition (works)
FRONTIER_HEADLESS_SKIP_STARTUP=1 ./frontier-cli/frontier-cli \
  --system-root tests/test_save_migration-v7.root \
  -e "defined(system.verbs.globals)"

# Test external content access (fails)
FRONTIER_HEADLESS_SKIP_STARTUP=1 ./frontier-cli/frontier-cli \
  --system-root tests/test_save_migration-v7.root \
  -e "sizeOf(system.verbs.globals)"
```

---

## Files Involved

- `Common/source/db_format.c`: Migration logic and external handle fixup
- `tests/save_migration_tests.c`: Migration validation test
- `frontier-cli/frontier-cli`: CLI tool for testing
- `tests/test_save_migration-v7.root`: Test artifact (generated)
