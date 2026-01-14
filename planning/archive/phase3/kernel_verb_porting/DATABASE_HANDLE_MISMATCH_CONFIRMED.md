# Database Handle Mismatch - CONFIRMED ROOT CAUSE

**Date**: 2025-12-17
**Status**: ROOT CAUSE IDENTIFIED
**Priority**: CRITICAL - Blocks all external object access post-migration

---

## Executive Summary

**The hypothesis from HANDLE_ROUTING_PLAN.md is CONFIRMED with definitive evidence.**

External objects (outlines, tables, menus, etc.) are capturing the **v6 source database handle** during migration instead of the **v7 destination handle**. This causes all external object access to fail after migration because they attempt to read v7 addresses from the v6 database.

---

## Definitive Evidence

### Migration Diagnostic Log Analysis

From `full_migration_log.txt` run on 2025-12-17:

**1. Database handles during save-as migration:**
```
[headless] dbstartsaveas BEGIN: source_db=0x600003bdacf8 dest_db=0x0
[headless] dbstartsaveas END: after final swap, databasedata=0x600003bdacf8 databasedestination=0x600003b1cc18 fl=1
```

**Identification:**
- **Source database (v6)**: `0x600003bdacf8`
- **Destination database (v7)**: `0x600003b1cc18`
- After save-as setup, `databasedata` points back to SOURCE

**2. External objects capturing database handles:**
```
[headless] langnewexternalvariable: flinmemory=0 variabledata=0x5d0ee9 captured_db=0x600003bdacf8 (current=0x600003bdacf8)
[headless] langnewexternalvariable: flinmemory=0 variabledata=0x83b1 captured_db=0x600003bdacf8 (current=0x600003bdacf8)
[headless] langnewexternalvariable: flinmemory=0 variabledata=0x8a59 captured_db=0x600003bdacf8 (current=0x600003bdacf8)
[headless] langnewexternalvariable: flinmemory=0 variabledata=0x58a24 captured_db=0x600003bdacf8 (current=0x600003bdacf8)
[headless] langnewexternalvariable: flinmemory=0 variabledata=0x55f831 captured_db=0x600003bdacf8 (current=0x600003bdacf8)
```

**SMOKING GUN**: All external objects capture `0x600003bdacf8` - THE V6 SOURCE DATABASE!

**3. The Problem Sequence:**

1. Migration starts, source=v6, destination=v7
2. Tables are unpacked from v6 database
3. During unpack, external objects call `langnewexternalvariable()`
4. `langnewexternalvariable()` does: `item.hdatabase = databasedata;`
5. At this point, `databasedata` = **v6 SOURCE** (`0x600003bdacf8`)
6. External object now has **v6 database handle** stored
7. External gets written to v7 database with **v7 address**
8. Later, when loading external from v7 database:
   - External object has `hdatabase = 0x600003bdacf8` (v6 source)
   - External object has `variabledata = <v7 address>`
   - Code does: `dbpushdatabase(0x600003bdacf8)` (switches to v6!)
   - Code tries: `dbrefhandle(<v7 address>, ...)` (read v7 address from v6 DB!)
   - **FAILURE**: v7 address doesn't exist in v6 database

---

## Impact Assessment

### What Breaks

**ALL external object access after migration fails:**
- ✗ Can't read outline/script externals
- ✗ Can't read table externals
- ✗ Can't read menu externals
- ✗ Can't read picture externals
- ✗ Can't read word processor externals

**Specific failures from MIGRATION_FIX_STATUS.md:**
```bash
# Fails silently:
FRONTIER_HEADLESS_SKIP_STARTUP=1 ./frontier-cli --system-root Frontier-v7.root \
  -e "sizeOf(system.verbs.globals)"
# Output: (nothing)

FRONTIER_HEADLESS_SKIP_STARTUP=1 ./frontier-cli --system-root Frontier-v7.root \
  -e 'workspace.test="hello"; workspace.test'
# Output: (nothing)
```

### Why It Happens

**Code locations:**

1. **`Common/source/langexternal.c:2602`**
   ```c
   item.hdatabase = databasedata; // Captures CURRENT database
   ```

2. **`Common/source/opverbs.c:363`**
   ```c
   item.hdatabase = databasedata; // Captures CURRENT database
   ```

During migration, `databasedata` points to the v6 source database, so all externals capture the wrong handle.

---

## Root Cause Analysis

### The Architecture

The external variable system was designed for the original Frontier where:
- There's only ONE database open at a time
- `databasedata` always points to "the" current database
- External objects capture `databasedata` to remember which database they belong to

### The Migration Problem

During save-as migration:
- TWO databases are open: source (v6) AND destination (v7)
- `databasedata` points to source for reading
- `databasedestination` points to destination for writing
- Externals unpacked from source capture SOURCE database handle
- Those externals get written to DESTINATION with DESTINATION addresses
- Result: External has SOURCE handle but DESTINATION address = MISMATCH

### Why Existing Code Doesn't Handle This

The `db_context_for_saveas_destination()` pattern (used in `dbassign()` and `dballocate()`) routes WRITES to the destination database, but external object CREATION happens at a higher level during table unpacking, where no special routing exists.

---

## Proposed Fix

### Option A: Route External Creation to Destination Context (RECOMMENDED)

**When**: During table unpacking in save-as migration
**Where**: `Common/source/tablepack.c` or migration-specific unpack path
**How**: Ensure `databasedata` points to destination when unpacking externals

**Implementation sketch:**
```c
// In table unpack during migration
if (fldatabasesaveas && databasedestination != nil) {
    hdldatabaserecord saved = databasedata;
    databasedata = databasedestination;  // Temporarily switch to destination

    // Unpack external - will now capture DESTINATION handle
    langexternalunpack(hpacked, &hexternalvar);

    databasedata = saved;  // Restore
}
```

**Pros:**
- Fixes root cause at creation time
- Externals automatically get correct database handle
- Clean, surgical fix
- No changes to external load logic needed

**Cons:**
- Need to find ALL external creation points during migration
- Must ensure this doesn't break non-migration unpack

---

### Option B: Fix Up Handles After Unpack

**When**: After external is unpacked but before writing to destination
**Where**: In migration-specific code path
**How**: Manually update `hdatabase` field to point to destination

**Implementation sketch:**
```c
// After unpacking external during migration
if (fldatabasesaveas && databasedestination != nil) {
    (**hexternalvar).hdatabase = databasedestination;
}
```

**Pros:**
- Simple, explicit fix
- Easy to understand and audit
- Minimal code changes

**Cons:**
- Must be applied at every external unpack site
- Easy to miss one
- Feels like a band-aid rather than fixing the root cause

---

### Option C: Override Database at Load Time

**When**: When loading external from disk (in `opverbinmemory()`, etc.)
**Where**: All `*verbinmemory()` functions
**How**: Use the database that owns the table entry instead of stored `hdatabase`

**Implementation sketch:**
```c
// In opverbinmemory()
hdldatabaserecord db_to_use = (**hv).hdatabase;

// Override with owning database if available
if (hnode != nil && (**hnode).owning_database != nil) {
    db_to_use = (**hnode).owning_database;
}

dbpushdatabase(db_to_use);
```

**Pros:**
- Fixes issue at runtime, no migration-specific code
- Robust against future similar issues
- Could help with other database-switching scenarios

**Cons:**
- Requires tracking owning database in hash nodes
- More complex change
- Doesn't fix the root cause, just works around it
- May have performance implications

---

## Recommended Solution

**Use Option A: Route External Creation to Destination Context**

This is the cleanest fix that addresses the root cause. Implementation plan:

### Phase 1: Identify External Creation Points

Search for all calls to:
- `langexternalunpack()`
- `opverbunpack()`
- `tableverbunpack()`
- `menuverbunpack()`
- `pictverbunpack()`
- `wpverbunpack()`

During migration code paths (save-as operations).

### Phase 2: Add Destination Context Routing

For each unpack call during save-as:
1. Save current `databasedata`
2. Switch `databasedata` to `databasedestination`
3. Perform unpack (external captures destination)
4. Restore original `databasedata`

### Phase 3: Add Safety Checks

- Assert that `databasedestination` is valid when switching
- Add logging to verify correct handles are captured
- Ensure this only happens during save-as, not normal unpack

### Phase 4: Test Thoroughly

- Run migration tests with logging
- Verify externals capture destination handle
- Test external access post-migration
- Ensure source database is never modified

---

## Testing Strategy

### Pre-Fix Validation

Run with current logging to confirm issue:
```bash
./save_migration_tests 2>&1 | grep "langnewexternalvariable.*captured_db"
```

Should show all captures pointing to source database.

### Post-Fix Validation

After implementing fix:
```bash
./save_migration_tests 2>&1 | grep "langnewexternalvariable.*captured_db"
```

Should show all captures pointing to destination database.

### Functional Tests

```bash
# Should work after fix:
FRONTIER_HEADLESS_SKIP_STARTUP=1 ./frontier-cli \
  --system-root databases/Frontier-v7.root \
  -e "sizeOf(system.verbs.globals)"

FRONTIER_HEADLESS_SKIP_STARTUP=1 ./frontier-cli \
  --system-root databases/Frontier-v7.root \
  -e 'workspace.test="hello"; workspace.test'
```

### Regression Tests

Ensure normal (non-migration) database operations still work:
```bash
# Test with non-migrated database
FRONTIER_HEADLESS_SKIP_STARTUP=1 ./frontier-cli \
  --system-root databases/Frontier.root \
  -e "sizeOf(system.verbs.globals)"
```

---

## Files To Modify

### Primary Changes

1. **`Common/source/tablepack.c`** - Add destination routing during table unpack
2. **`Common/source/langexternal.c`** - Possibly add helper for destination-aware unpack
3. **Migration-specific code** - Wherever externals are unpacked during save-as

### Verification Changes

1. **Keep diagnostic logging** (at least temporarily):
   - `newoutlinevariable()`
   - `langnewexternalvariable()`
   - `dbstartsaveas()`

---

## Related Issues

### Issue #1: Source Database Still Being Modified

From DEBUG_NOTES.md lines 122-127, the source v6 database was being modified during migration even after the `dbassign()` fix. This database handle mismatch may explain why - code may be pushing the wrong database and inadvertently writing to it.

**After this fix**, verify source database is NEVER modified:
```bash
md5sum databases/Frontier.root  # Before
# Run migration
md5sum databases/Frontier.root  # After - should match!
```

### Issue #2: Missing `dbcopy()` Destination Routing

From HANDLE_ROUTING_PLAN.md, `dbcopy()` still lacks destination routing. Should be fixed in same commit for consistency.

---

## Success Criteria

✅ External objects capture DESTINATION database handle during migration
✅ External objects load successfully from v7 database post-migration
✅ Table operations work (assignment, nested access)
✅ Source v6 database is NEVER modified during migration
✅ All headless tests pass
✅ No regressions in non-migration database operations

---

## Next Steps

1. **Implement Option A fix** in tablepack.c and related unpack paths
2. **Test with diagnostic logging** to verify correct handles captured
3. **Run full test suite** to ensure no regressions
4. **Update MIGRATION_FIX_STATUS.md** with results
5. **Create commit** with fix and reference this document
6. **Clean up diagnostic logging** (or keep for future debugging)

---

## Commit Message Template

```
fix(migration): route external creation to destination database

During v6→v7 migration, external objects were capturing the v6 source
database handle instead of the v7 destination handle. This caused all
external object access to fail post-migration because code would try
to read v7 addresses from the v6 database.

Root cause: External objects call `langnewexternalvariable()` which
captures `databasedata`. During migration, `databasedata` pointed to
the source database even though the destination database should be
used.

Fix: Temporarily switch `databasedata` to `databasedestination` during
external unpacking in save-as operations, ensuring externals capture
the correct destination handle.

Testing: Diagnostic logging confirms externals now capture destination
handle (0x<DEST>) instead of source handle (0x<SOURCE>).

Fixes: External object access post-migration
See: planning/phase3/kernel_verb_porting/DATABASE_HANDLE_MISMATCH_CONFIRMED.md
```

---

## References

- **MIGRATION_FIX_STATUS.md**: Documents `dbassign()` fix and table operation failures
- **DEBUG_NOTES.md**: Details of migration debugging process
- **HANDLE_ROUTING_PLAN.md**: Original hypothesis and investigation plan
- **Diagnostic logs**: `full_migration_log.txt` with evidence
