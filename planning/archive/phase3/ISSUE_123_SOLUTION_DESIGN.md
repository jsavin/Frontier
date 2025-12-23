# Issue #123: External Table Address Migration Fix - Solution Design

**Date**: 2025-12-18
**Issue**: #123 - Table content access fails post-migration (P0 Blocker)
**Status**: Root Cause Identified - Reader/Writer Fork Issue

**UPDATE 2025-12-18 Evening**: After implementing materialization fix and testing, discovered the actual root cause is a **reader/writer fork issue**. Root table unpacks with legacy 32-bit reader (`use64=0`) despite v7 database format (`use64=1`). Child tables correctly use modern reader. Migration writes correct 64-bit addresses, but root table unpacking uses wrong reader path. Problem is in `hashunpacktable` not respecting database format mode for root table. See "Actual Root Cause" section below.

---

## Executive Summary

**Problem**: After v6→v7 migration, external table content is inaccessible due to invalid database addresses.

**Root Cause**: External tables with `flinmemory=0` store v6 database addresses that aren't converted during migration, causing `dbnormalizeaddress()` failures in v7.

**Recommended Solution**: Force all external tables into memory (`flinmemory=1`) during migration. This is simple, correct, and handles typical database sizes (1.5-6MB) well.

**Estimated Implementation**: 6-8 hours total (4 hours code + 2-3 hours testing + 1 hour docs)

---

## Root Cause Analysis

### The Problem

During v6→v7 migration:
- External tables with `flinmemory=0` (on-disk) store database addresses in `variabledata` field
- These v6 addresses (e.g., 0x62b8d9) are written into v7 without conversion
- When v7 tries to access them, `dbnormalizeaddress()` fails - addresses don't point to valid v7 blocks

### Why This Happens

**Code Flow**:
```
migrate_internal() (db_format.c:1286-1500)
  → langhash_materialize_disk_values() (line 1397)
    → tableverbinmemory() loads tables from v6 SOURCE
    → Tables are loaded into memory successfully
  → Root table saved to v7 DESTINATION
    → External variables packed with their addresses
    → BUT: Addresses are v6 format, not converted to v7
```

**Result**: External tables with `flinmemory=0` have v6 addresses stored in v7 database.

### Why In-Memory Tables Work

Tables with `flinmemory=1` use memory pointers (not DB addresses), so:
- No address normalization needed
- No dependency on database block structure
- Always accessible

**Example**:
- `system.paths` (flinmemory=1): ✅ Works - `sizeOf()` returns 14
- `system.verbs.globals` (flinmemory=0): ❌ Fails - `dbnormalizeaddress failed for adr=0x62b8d9`

---

## Solution Options Evaluated

### Option 1: Force All Externals to In-Memory (RECOMMENDED)

**Approach**: During migration, force-load all `flinmemory=0` tables into memory and set `flinmemory=1`.

**Complexity**: 2/5
**Risk**: 3/5
**Correctness**: 5/5

**Pros**:
- Simple, clean implementation
- All data guaranteed accessible post-migration
- Consistent with existing `langhash_materialize_disk_values()` pattern
- Handles typical database sizes (1.5-6MB) easily

**Cons**:
- Could hit memory limits on very large databases (unlikely)
- Changes external semantics from on-disk to in-memory
- Not reversible

### Option 2: Re-Save External Tables with V7 Addresses

**Approach**: Load each external table, save it to v7, get new v7 address, update reference.

**Complexity**: 4/5
**Risk**: 4/5
**Correctness**: 5/5

**Pros**:
- Preserves on-disk table semantics
- More memory-efficient for large databases

**Cons**:
- Complex implementation with error paths
- Must track save order (children before parents)
- Higher risk of bugs

### Option 3: Address Translation Map

**Approach**: Build v6→v7 address map, translate during pack/unpack.

**Complexity**: 5/5
**Risk**: 5/5
**Correctness**: 3/5

**Rejected**: Too complex, high risk, hard to maintain.

### Option 4: Lazy Materialization on Access

**Approach**: Keep v6 database, reload on first access.

**Complexity**: 2/5
**Risk**: 5/5
**Correctness**: 1/5

**Rejected**: Requires keeping v6, doesn't solve fundamental issue.

---

## Recommended Solution: Option 1 (Force In-Memory)

### Why This Solution

1. **Correctness First**: Migration is one-time. Correctness > complexity.
2. **Typical DB Sizes**: 1.5-6MB fits comfortably in memory
3. **Clean Architecture**: Eliminates v6 on-disk format complexity
4. **Existing Precedent**: Consistent with `langhash_materialize_disk_values()`
5. **Low Risk**: In-memory table paths are well-tested

### Implementation Plan

#### Phase 1: Add Force-Materialize Function (3-4 hours)

**File**: `/Users/jake/dev/jsavin/Frontier/Common/source/db_format.c`

**Add new function before `migrate_internal()`**:

```c
static boolean db_format_force_materialize_external_tables_recursive(
    hdlhashtable htable,
    const db_context *context,
    int depth
) {
    if (htable == nil || depth > 50) /* prevent infinite recursion */
        return true;

    long ix = 0;
    hdlhashnode hnode = nil;

    while (hashgetnthnode(htable, ix++, &hnode)) {
        if (hnode == nil)
            continue;

        tyvaluerecord *val = &(**hnode).val;
        if (val->valuetype != externalvaluetype)
            continue;

        hdlexternalvariable hv = (hdlexternalvariable) val->data.externalvalue;
        if (hv == nil || (**hv).id != idtableprocessor)
            continue;

        /* Skip if already in memory */
        if ((**hv).flinmemory)
            continue;

        dbaddress v6_adr = (dbaddress) (**hv).variabledata;
        bigstring bsname;
        gethashkey(hnode, bsname);

        fprintf(stderr, "[headless] force-materializing '%.*s' v6_adr=0x%llx\n",
                (int) bsname[0], (char *) &bsname[1],
                (unsigned long long) v6_adr);

        /* Force load into memory - reads from v6 via source_context */
        if (!tableverbinmemory(hv, hnode)) {
            fprintf(stderr, "[headless] force-materialize failed for '%.*s'\n",
                    (int) bsname[0], (char *) &bsname[1]);
            return false;
        }

        /* Verify it's now in memory */
        if (!(**hv).flinmemory) {
            fprintf(stderr, "[headless] flinmemory not set for '%.*s'\n",
                    (int) bsname[0], (char *) &bsname[1]);
            return false;
        }

        /* Clear oldaddress to force new allocation in v7 */
        (**hv).oldaddress = nildbaddress;

        /* Recurse into newly-loaded table */
        hdlhashtable child = (hdlhashtable) (**hv).variabledata;
        if (!db_format_force_materialize_external_tables_recursive(
                child, context, depth + 1))
            return false;
    }
    return true;
}

static boolean db_format_force_materialize_external_tables(
    hdlhashtable hroot,
    const db_context *context
) {
    return db_format_force_materialize_external_tables_recursive(
        hroot, context, 0);
}
```

**Insert call in `migrate_internal()` after line 1397**:

```c
fail_step = "force_materialize_external_tables(root)";
if (!db_format_force_materialize_external_tables(hroot, &source_context))
    goto cleanup;
```

#### Phase 2: Update Sanitize Function (1 hour)

**File**: `/Users/jake/dev/jsavin/Frontier/Common/source/db_format.c`

**Modify `db_format_sanitize_root_externals()` at line 1241-1251**:

```c
/* All tables should now be in memory from force-materialize step */
if (!(**hv).flinmemory) {
    fprintf(stderr, "[headless] WARNING: external not in memory name='%.*s'\n",
            (int) bsname[0], (char *) &bsname[1]);
    /* Continue trying to load anyway */
}
```

#### Phase 3: Testing (2-3 hours)

**Test cases**:
1. Clean migration: `rm test_save_migration* && make -C tests save_migration_tests && ./tests/save_migration_tests`
2. Verify external access: `FRONTIER_HEADLESS_SKIP_STARTUP=1 ./frontier-cli/frontier-cli --system-root ./test_save_migration-v7.root -e "sizeOf(system.verbs.globals)"`
3. Verify v6 unchanged: `md5sum databases/Frontier-v6.root` before/after
4. Run full test suite: `./tools/run_headless_tests.sh`

**Expected results**:
- `sizeOf(system.verbs.globals)` returns a number (not error)
- `sizeOf(workspace)` returns a number
- All external tables accessible
- v6 checksum unchanged

#### Phase 4: Documentation (1 hour)

**Add to `db_format.c` header**:

```c
/*
 * Migration Strategy for External Tables:
 *
 * V6 databases may contain external table variables with flinmemory=0,
 * meaning their content is stored on-disk at a v6 database address.
 * During v6→v7 migration, these addresses become invalid because:
 * 1. V7 uses different address space (64-bit BE vs 32-bit)
 * 2. Block allocation changes during migration
 *
 * Solution: Force all external tables into memory before saving to v7.
 * Implemented in db_format_force_materialize_external_tables().
 *
 * See: planning/phase3/database_architecture/ISSUE_123_SOLUTION_DESIGN.md
 */
```

**Update `planning/phase3/MIGRATION_VALIDATION_REPORT.md`**:
- Mark Issue #123 as RESOLVED
- Document the fix approach

---

## Risk Mitigation

| Risk | Mitigation |
|------|-----------|
| Stack overflow from deep recursion | Depth limit (50 levels) in recursive function |
| Memory exhaustion | Typical DBs are small; add memory check if needed |
| Partial migration on error | Migration uses temp file + atomic rename |
| Infinite loop from circular refs | Visited-table tracking (can add if needed) |
| V6 database corruption | Always read-only, never written |

---

## Success Criteria

**Migration succeeds when**:
1. ✅ All external tables in Frontier-v6.root migrate successfully
2. ✅ No `dbnormalizeaddress()` failures post-migration
3. ✅ `sizeOf(system.verbs.globals)` returns valid number
4. ✅ `sizeOf(workspace)` returns valid number
5. ✅ `./tools/run_headless_tests.sh` passes
6. ✅ V6 database unchanged (checksum verification)

---

## Alternative: Option 2 (If Memory Becomes Issue)

If very large databases (>100MB) cause memory pressure:

**Implement**: Re-save external tables to v7 with new addresses
**Complexity**: Higher (4/5 vs 2/5)
**Benefits**: Preserves on-disk semantics, more memory-efficient

**Key steps**:
1. Load each external table via `tableverbinmemory()`
2. Pack and save to v7: `hashpacktable_context()` → `dbassignhandle_context()`
3. Update `(**hv).variabledata` with new v7 address
4. Unload from memory via `tableverbdispose()`
5. Keep `flinmemory=0`

**When to use**: Only if Option 1 causes memory issues in practice.

---

## Critical Files

| File | Changes | Lines |
|------|---------|-------|
| `Common/source/db_format.c` | Add force-materialize function + call | +60 lines |
| `Common/source/db_format.c` | Update sanitize function | ~10 modified |
| `planning/phase3/MIGRATION_VALIDATION_REPORT.md` | Document resolution | Update |

---

## Next Steps

1. **Review this solution design**
2. **Approve or request modifications**
3. **Implement Phase 1-2** (code changes)
4. **Test Phase 3** (verification)
5. **Document Phase 4** (completion)
6. **Close Issue #123** with fix details

---

## Actual Root Cause (Discovered 2025-12-18 Evening)

### The Real Problem

After implementing the materialization fix and extensive testing, discovered the issue is **NOT** with address conversion during migration. The problem is a **reader/writer fork bug** in how the root table is unpacked.

### Evidence

**Migration phase (WRITING):**
```
[headless] tableverbpack dbsavehandle adr: 0x0 → 0x62bb33  ✓ Correct v7 address allocated
[headless] tableverbpack pushaddress adr=0x62bb33           ✓ Correct address written
```

**Reopening v7 database (READING):**
```
[headless] dbopenfile version=7 use64=true                  ✓ Database detected as v7
[headless] hashunpacktable name='system' use64=0            ✗ ROOT table uses LEGACY reader!
[headless] hashunpacktable name='verbs' use64=1             ✓ Child tables use MODERN reader
```

**Access attempt:**
```
[headless] tableverbinmemory flinmemory=0 adr=0x62bb33
[headless] dbnormalizeaddress failed for adr=0x62bb33       ✗ Valid v7 address treated as invalid
```

### Why It Happens

1. Database opens correctly in v7 mode: `dbopenfile` detects `version=7` and sets `use64=true`
2. Root table loads via `tableloadsystemtable()` → `hashunpacktable()`
3. **BUG**: Root table unpacking uses `use64=0` (legacy 32-bit reader) instead of `use64=1`
4. External variable addresses are read as 32-bit and stored incorrectly
5. Child tables (system.verbs, etc.) correctly use `use64=1` when unpacked
6. Later access to external tables fails because addresses are corrupt from root unpack

### The Fix Needed

Not address conversion or materialization - those work correctly. The fix is:

**Ensure `hashunpacktable()` respects the database format mode when unpacking the root table.**

The root table unpack path needs to check `db_format_mode_current().use_64bit_format` or the database version and use the modern v7 reader path, not the legacy v6 path.

### Materialization Fix Status

The materialization code implemented IS correct and necessary:
- ✓ All external tables loaded into memory during migration
- ✓ All `oldaddress` fields cleared to force new v7 allocations
- ✓ New v7 addresses correctly allocated and written
- ✓ `adapter_repack` flag correctly set for migration context

The problem is simply that the ROOT table uses the wrong READER when the v7 file is opened.

---

## References

- Issue #123: https://github.com/jsavin/Frontier/issues/123
- PR #117: External handle mismatch fix (prerequisite)
- ADR-001: Multi-database context management
- `planning/phase3/MIGRATION_VALIDATION_REPORT.md`: Root cause validation
- `planning/phase3/modern_reader_writer_split.md`: Reader/writer fork architecture
- `docs/external_table_variable_management.md`: External table lifecycle documentation
