# Database Migration Fix - Status Report

## What We Fixed

### Issue: Destination Routing During Migration
**Problem**: During v6→v7 migration, `dbassign()` was writing to the source database instead of the destination.

**Root Cause**: `dbassign()` called `db_context_refresh_default()` which captures the current database (source) rather than routing to the destination when save-as is active.

**Fix** (commit 7208ae60):
```c
// Common/source/db.c:2345
boolean dbassign (dbaddress *padr, long newsize, ptrvoid pdata) {
    db_context ctx_storage;
    boolean using_destination = false;
    db_context *ctx = db_context_for_saveas_destination(&ctx_storage, &using_destination);
    if (ctx == NULL)
        ctx = db_context_refresh_default();
    return dbassign_context(ctx, padr, newsize, pdata);
}
```

**Verification**:
- ✅ Source file stays v6 format after migration (header: `00 06`, size: 5.8M)
- ✅ Destination file created as v7 format (header: `00 07`, size: 9.9M)
- ✅ No in-place corruption of source database
- ✅ `save_migration_tests` passes

## What Still Doesn't Work

### Issue: Database Hydration / Table Access
**Problem**: Both v6 and v7 databases load successfully but table operations fail.

**Symptoms**:
```bash
# Works:
FRONTIER_HEADLESS_SKIP_STARTUP=1 ./frontier-cli/frontier-cli --system-root databases/Frontier-v7.root -e "1+1"
# Output: 2

FRONTIER_HEADLESS_SKIP_STARTUP=1 ./frontier-cli/frontier-cli --system-root databases/Frontier-v7.root -e "typeOf(system)"
# Output: tabl

FRONTIER_HEADLESS_SKIP_STARTUP=1 ./frontier-cli/frontier-cli --system-root databases/Frontier-v7.root -e "sizeOf(system)"
# Output: 12

# Fails (no output or error):
FRONTIER_HEADLESS_SKIP_STARTUP=1 ./frontier-cli/frontier-cli --system-root databases/Frontier-v7.root -e "sizeOf(system.verbs.globals)"
# Output: (nothing)

FRONTIER_HEADLESS_SKIP_STARTUP=1 ./frontier-cli/frontier-cli --system-root databases/Frontier-v7.root -e 'workspace.test="hello"; workspace.test'
# Output: (nothing)

FRONTIER_HEADLESS_SKIP_STARTUP=1 ./frontier-cli/frontier-cli --system-root databases/Frontier-v7.root -e 'defined(workspace.test)'
# Output: (nothing)
```

**Analysis**:
1. Database loads successfully - addresses are valid v7 format
2. Top-level table access works (`system` table)
3. Nested table access fails (`system.verbs.globals`)
4. Writing to tables fails (`workspace.test = "hello"`)
5. **This affects BOTH v6 and v7 databases** - not migration-specific

**Investigation Notes**:
- Address reads from v7 are successful (dbrefhandle logs show valid reads)
- Addresses like `0x9ebe15`, `0x92f0c1` are valid v7 addresses being read correctly
- The Explore agent suggested v6 addresses weren't being converted, but logging shows v7 addresses ARE being used
- The issue appears to be at a higher level - possibly with how table operations are implemented or how the specific database was constructed

## Possible Root Causes (To Investigate)

1. **Empty Database**: The Frontier-v6.root may be a minimal/bootstrap database without much content
2. **Missing Verb Implementations**: Table assignment/access verbs may not be fully implemented in headless mode
3. **Table Structure Issues**: The database structure may be incomplete or malformed
4. **Silent Error Handling**: Errors may be occurring but not being displayed by the CLI

## Next Steps

1. **Verify database contents**: Use a database inspection tool to see what's actually in the database
2. **Test with a fuller database**: Try migration with a database known to have content
3. **Enable more logging**: Add logging to table access operations to see where failures occur
4. **Check verb bindings**: Verify that table access verbs are properly bound in headless mode

## Summary

**Migration Fix: ✅ COMPLETE**
- Source files no longer corrupted during migration
- V7 files created correctly with proper format

**Database Hydration: ❌ INCOMPLETE**
- This appears to be a separate, pre-existing issue
- Affects both v6 and v7 databases equally
- Not caused by our migration routing fix
- Requires further investigation into table operations or database content
