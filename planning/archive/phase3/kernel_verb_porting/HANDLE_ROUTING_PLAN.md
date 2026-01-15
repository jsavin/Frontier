# Handle Routing Investigation Plan
## Post-Migration External Object Access Failure

**Date**: 2025-12-17
**Status**: Investigation Phase
**Related Docs**: MIGRATION_FIX_STATUS.md, DEBUG_NOTES.md

---

## Executive Summary

While the `dbassign()` destination routing fix (commit 7208ae60) solved the immediate migration corruption issue, external objects still can't be accessed from the v7 database post-migration. Analysis reveals **the root cause is likely database handle mismatch**: external objects are capturing references to the v6 source database during unpacking, then trying to use those references with v7 addresses.

---

## Critical Issues Identified

### Issue #1: Database Reference Capture During Unpack (HIGHEST PRIORITY)

**Location**: `Common/source/opverbs.c:353-366` (`newoutlinevariable`)

**The Problem**:
```c
static boolean newoutlinevariable (boolean flinmemory, long variabledata, hdloutlinevariable *h) {
    tyoutlinevariable item;
    clearbytes (&item, sizeof (item));
    item.flinmemory = flinmemory;
    item.variabledata = variabledata;
    item.hdatabase = databasedata; // <-- CAPTURES CURRENT DATABASE
    return (newfilledhandle (&item, sizeof (item), (Handle *) h));
}
```

**What Happens**:
1. During v6→v7 migration, tables are unpacked from the source database
2. When `opverbunpack()` is called, it calls `newoutlinevariable(false, rawadr, ...)`
3. The new outline variable captures `databasedata` into its `hdatabase` field
4. **If `databasedata` still points to the v6 source**, the external now has a v6 database reference
5. Later, when loading the external from the v7 database, `opverbinmemory()` does:
   ```c
   dbpushdatabase ((**hv).hdatabase);  // Pushes v6 database!
   adr = (dbaddress) (**hv).variabledata;  // Gets v7 address!
   fl = dbrefhandle (adr, &hpackedoutline);  // Tries to read v7 address from v6 database!
   ```

**Impact**: Every external object read fails because it's trying to read v7 addresses from the v6 database.

**Evidence**:
- DEBUG_NOTES.md (lines 122-127) shows source database was being modified even after fixes
- MIGRATION_FIX_STATUS.md shows table operations fail on both v6 and v7 databases
- The `dbpushdatabase()` call in `opverbinmemory:580` is a smoking gun

**Similar Issue May Affect**:
- `wpverbunpack()` (word processor externals)
- `tableverbunpack()` (table externals)
- `menuverbunpack()` (menu externals)
- `pictverbunpack()` (picture externals)

All of these likely use similar patterns that capture `databasedata`.

---

### Issue #2: Missing Destination Routing in `dbcopy()`

**Location**: `Common/source/db.c:2460-2463`

**The Problem**:
```c
boolean dbcopy (dbaddress adrorig, dbaddress *adrcopy) {
    db_context *ctx = db_context_refresh_default();  // Doesn't check save-as!
    return dbcopy_context(ctx, adrorig, adrcopy);
}
```

**Current Impact**: LOW (not currently called during adapter_repack migration)

`dbcopy()` uses `db_context_refresh_default()` instead of `db_context_for_saveas_destination()`. However, in `opverbpack:826-828`, the call to `dbcopy()` is skipped when `adapter_repack` is true, so this doesn't affect current migrations.

**Future Risk**: If other code paths use `dbcopy()` during save-as operations, they'll have the same issue that `dbassign()` had.

**Fix**: Same pattern as `dbassign()` and `dballocate()` - check for save-as destination first.

---

### Issue #3: Other Potential Unrouted Write Paths

**Evidence**: DEBUG_NOTES.md lines 122-127 show source database was modified during testing even after the `dbassign()` fix was applied.

**Investigation Needed**:
- Search for all database write operations:
  - `dbassign*()` variants
  - `dballocate*()` variants
  - `dbrelease*()` variants
  - `dbwrite*()` functions
- Verify each one has proper destination routing during save-as

**Key Functions to Check**:
- `dbassignhandle()` (called from `opverbpack:864`)
- `dbreleasestring()` (Common/source/db.c:2492)
- `dbassignheapstring()` (Common/source/db.c:2521)
- Any other function that modifies the database file

---

## Diagnostic Plan

### Phase 1: Add Comprehensive Logging (IMMEDIATE)

Add logging to understand **which database handle** is being used when:

1. **In `newoutlinevariable()`** and similar functions:
   ```c
   fprintf(stderr, "[headless] newoutlinevariable: databasedata=%p variabledata=0x%llx\n",
           (void*)databasedata, (unsigned long long)variabledata);
   ```

2. **In `opverbinmemory()` before `dbpushdatabase()`**:
   ```c
   fprintf(stderr, "[headless] opverbinmemory: pushing hdatabase=%p (current databasedata=%p) adr=0x%llx\n",
           (void*)(**hv).hdatabase, (void*)databasedata, (unsigned long long)adr);
   ```

3. **In `dbpushdatabase()`** itself:
   ```c
   fprintf(stderr, "[headless] dbpushdatabase: old=%p new=%p\n",
           (void*)databasedata, (void*)newdb);
   ```

### Phase 2: Test and Analyze (IMMEDIATE)

1. Run migration with new logging:
   ```bash
   ./tools/run_headless_tests.sh 2>&1 | tee migration_log.txt
   ```

2. Analyze the log for:
   - What is `databasedata` when externals are being unpacked?
   - What database handle gets stored in `hdatabase`?
   - When loading externals later, which database is being pushed?
   - Are v7 addresses being read from v6 database (or vice versa)?

### Phase 3: Verify Database References (AFTER PHASE 2)

If logging confirms the database handle mismatch:

1. Check how table unpacking happens during migration
2. Identify where `databasedata` should be switched to destination
3. Determine if unpacking should happen in destination context

---

## Proposed Fixes (PENDING DIAGNOSTIC CONFIRMATION)

### Fix 1: Ensure Externals Capture Correct Database Reference

**Option A**: Fix at unpack time - make sure `databasedata` points to destination when unpacking during migration.

**Option B**: Fix at pack time - store destination database reference in packed data, restore it at unpack time.

**Option C**: Fix at load time - in `opverbinmemory()`, use the database that owns the table entry instead of `hdatabase`.

**Recommendation**: WAIT for diagnostic results before choosing an approach.

### Fix 2: Add Destination Routing to `dbcopy()`

**Change**: `db.c:2460-2463`
```c
boolean dbcopy (dbaddress adrorig, dbaddress *adrcopy) {
    db_context ctx_storage;
    boolean using_destination = false;
    db_context *ctx = db_context_for_saveas_destination(&ctx_storage, &using_destination);
    if (ctx == NULL)
        ctx = db_context_refresh_default();
    return dbcopy_context(ctx, adrorig, adrcopy);
}
```

**Priority**: MEDIUM (doesn't affect current migrations but should be fixed for consistency)

### Fix 3: Audit All Database Write Operations

**Action**: Systematic review of all database write functions to ensure they check for save-as destination.

**Method**:
1. `grep -n "databasedata\|databasedestination" Common/source/db.c`
2. For each write function, verify it uses destination routing
3. Add TODO comments for any that don't
4. Create tracking issue for each missing routing check

**Priority**: HIGH (prevents future regressions)

---

## Testing Strategy

### Pre-Fix Verification

1. Add logging from Phase 1
2. Run migration and confirm database handle mismatch
3. Document exact sequence of events leading to failure

### Post-Fix Verification

1. Run `./tools/run_headless_tests.sh` to verify no regressions
2. Test specific external access scenarios:
   ```bash
   # Test outline external access
   FRONTIER_HEADLESS_SKIP_STARTUP=1 ./frontier-cli/frontier-cli \
     --system-root databases/Frontier-v7.root \
     -e "typeOf(system.verbs.builtins.file.isFolder)"

   # Test table assignment
   FRONTIER_HEADLESS_SKIP_STARTUP=1 ./frontier-cli/frontier-cli \
     --system-root databases/Frontier-v7.root \
     -e 'workspace.test="hello"; workspace.test'

   # Test nested table access
   FRONTIER_HEADLESS_SKIP_STARTUP=1 ./frontier-cli/frontier-cli \
     --system-root databases/Frontier-v7.root \
     -e "sizeOf(system.verbs.globals)"
   ```
3. Verify source database is never modified:
   ```bash
   md5sum databases/Frontier.root  # Before
   # Run migration
   md5sum databases/Frontier.root  # After - should match!
   ```

---

## Next Steps

### Immediate Actions (Today)

1. **Add logging** from Phase 1 to these files:
   - `Common/source/opverbs.c` (`newoutlinevariable`, `opverbinmemory`)
   - `Common/source/db.c` (`dbpushdatabase`)
   - Any other external unpack functions

2. **Run migration** with logging and capture full output

3. **Analyze logs** to confirm or refute the database handle mismatch hypothesis

4. **Report findings** and decide on fix approach

### Follow-Up Actions (This Week)

1. Implement chosen fix for database handle issue
2. Add destination routing to `dbcopy()` for consistency
3. Begin systematic audit of database write operations
4. Update MIGRATION_FIX_STATUS.md with latest findings

### Long-Term Actions

1. Complete audit of all database operations
2. Add automated tests for migration scenarios
3. Document database handle lifecycle in planning docs
4. Consider architectural improvements to prevent future handle mismatches

---

## Open Questions

1. **When exactly are externals unpacked during migration?**
   - During table unpack?
   - On-demand when first accessed?
   - Need to trace the exact call sequence

2. **Should external unpacking happen in destination context?**
   - If yes, how to ensure that?
   - If no, how should database references be fixed up?

3. **Are there other data structures that capture `databasedata`?**
   - Tables themselves?
   - Other external types?
   - Need comprehensive search

4. **Why does the original code update `oldaddress` outside save-as?**
   - Is there a reason for this design?
   - Check legacy Frontier source at `/Users/jake/dev/tedchoward/Frontier/`

---

## Success Criteria

✅ External objects can be loaded from v7 database
✅ Table assignments work in v7 database
✅ Nested table access works in v7 database
✅ Source v6 database is NEVER modified during migration
✅ All `./tools/run_headless_tests.sh` tests pass
✅ No handle routing issues remain in codebase

---

## References

- **MIGRATION_FIX_STATUS.md**: Documents `dbassign()` fix and current table operation failures
- **DEBUG_NOTES.md**: Details of `dbassign()` fix, source database modification warnings
- **db.c**: Database allocation and assignment functions
- **opverbs.c**: Outline external pack/unpack/load functions
- **langexternal.c**: External variable pack/unpack dispatch
