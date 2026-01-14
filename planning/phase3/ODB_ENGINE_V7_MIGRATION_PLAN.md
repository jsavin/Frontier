# ODB Engine v7 Format Migration - Execution Plan

**Status**: Planning
**Created**: 2026-01-07
**Priority**: High - Blocks guest database functionality with v7 format

## Executive Summary

The ODB Engine (`odbengine.c`) currently only supports v6 format databases with Cancoon records. This blocks all guest database operations (`db.new()`, `db.open()`) from working with v7 format databases. This plan details the migration of the ODB Engine to support v7 format with auto-migration capabilities, following the proven patterns established in the system root migration code.

**Migration Strategy**: Two-phase approach with temporary `.root7` extension during development, transitioning to standard `.root` extension once v7 format is stable.

### Key Findings

**What Works:**
- System root migration (v6→v7) via `dbOpenFile()` in `db.c`
- Migration infrastructure in `db_format.c` (functions around lines 874-1054)
- v7 reader/writer modules (`db_reader_v7.c`, `db_writer_v7.c`)
- Database layer properly handles v7 format detection and loading

**What's Broken:**
- `odbNewFile()` creates v6-style 442-byte Cancoon records (version 2 or 3)
- `odbOpenFile()` expects Cancoon records and fails on v7 databases
- `odbSaveFile()` still writes Cancoon records (though has v7 migration hooks)
- All ODB engine operations fail on v7 format databases

**Root Cause:**
- ODB engine predates v7 format (built for Mac Classic Frontier with Cancoon UI)
- `tyversion2cancoonrecord` structure (442 bytes) is UI-centric (fonts, window positions, agents popup)
- v7 databases point `views[0]` directly at root table, no Cancoon record
- ODB engine hasn't been updated to follow v7 format patterns

## Two-Phase Migration Strategy

### Overview

To safely transition from v6 to v7 format while preserving production stability, we use a two-phase approach with temporary file extensions.

### Phase 1: Development/Stabilization (Current)

**File Extension Strategy:**
- `.root` = v6 original (preserved, untouched)
- `.root7` = v7 migrated (temporary extension during development)

**Benefits:**
- Production UserTalk code continues working unchanged (still uses `.root`)
- Both formats coexist during development
- No breaking changes to existing scripts
- Easy rollback if issues discovered
- Clear visual distinction between v6 and v7 files

**Implementation Changes:**
- `db.new()` creates `.root7` files (not `.root`)
- `db.open()` detects extension and opens appropriate format
- Test code updated to use `.root7` paths
- Migration creates `.root7` alongside original `.root`

**Database Naming Examples:**
- System root: `Frontier.root` (original) + `Frontier.root7` (migrated)
- Guest databases: `test.root` (v6) + `test.root7` (v7)
- Test databases: `{FRONTIER_TEST_TMP_DIR}/mydb.root7`

### Phase 2: Production (Once Stable, Future)

**File Extension Strategy:**
- `.root` = v7 canonical (back to standard extension)
- `.root.v6.bkp` = v6 backup created during migration

**Benefits:**
- Standard `.root` extension restored
- Production UserTalk code works unchanged (still uses `.root`, now opens v7)
- Clear backup naming for v6 archives
- Single source of truth (`mydb.root` is always current version)

**Migration Flow:**
```
Before: mydb.root (v6 format)
After:  mydb.root (v7 format) + mydb.root.v6.bkp (v6 backup)
```

**Implementation Changes:**
- `db.new()` creates `.root` files (v7 format)
- `db.open()` opens `.root` as v7, creates `.root.v6.bkp` on first v6→v7 migration
- Test code returns to using `.root` paths
- Migration renames original `.root` to `.root.v6.bkp`, creates new `.root` (v7)

### Phase 1 → Phase 2 Transition Plan

**Trigger Conditions** (all must be met):
1. v7 format proven stable in production for 30+ days
2. All integration tests passing consistently
3. No critical bugs reported in v7 format handling
4. User approval for production transition

**Transition Steps:**

1. **Update ODB Engine Code**
   - Change `odbNewFile()` to create `.root` (not `.root7`)
   - Update migration logic to create `.root.v6.bkp` backups
   - Update `odbOpenFile()` to prefer `.root`, fallback to `.root.v6.bkp`

2. **Update Test Infrastructure**
   - Change test database paths from `.root7` to `.root`
   - Update test fixtures to use standard `.root` extension
   - Verify all tests pass with new naming

3. **Update Documentation**
   - Update `docs/GUEST_DATABASES.md` with Phase 2 behavior
   - Update `docs/TESTING_GUIDE.md` with `.root` paths
   - Update migration documentation with `.root.v6.bkp` backup pattern

4. **Production Database Migration**
   - Provide migration script: `tools/migrate_root7_to_root.sh`
   - Script renames `*.root7` → `*.root`, preserving `.root.v6.bkp`
   - User runs migration during maintenance window

**Rollback Plan** (if Phase 2 has issues):
- Restore `.root.v6.bkp` files to `.root`
- Revert ODB engine code to Phase 1 behavior
- Resume using `.root7` extension temporarily

### Why Two Phases?

**Problem with Direct v7 Migration:**
- Immediately replacing `.root` with v7 format risks breaking production code
- No coexistence of v6/v7 during stabilization period
- Difficult rollback if v7 issues discovered
- Test code changes mixed with production code changes

**Phase 1 Solution:**
- `.root7` extension signals "this is experimental v7 format"
- Original `.root` files remain untouched
- Production UserTalk code continues working (opens `.root`, gets v6)
- Test code explicitly opts into v7 (uses `.root7`)

**Phase 2 Completion:**
- Once v7 proven stable, return to standard `.root` extension
- Backup strategy preserves v6 files as `.root.v6.bkp`
- Production transition is a simple rename + code update

## Technical Context

### Cancoon Records (v6 Legacy)

```c
typedef struct tyversion2cancoonrecord {
    short versionnumber;                    // = 0x0003
    dbaddress adrroottable;                 // Points to real root table
    tycancoonwindowinfo windowinfo[6];      // UI state (442 bytes total)
    dbaddress adrscriptstring;
    unsigned short flags;
    short ixprimaryagent;
    short waste[28];
} tyversion2cancoonrecord;
```

**Purpose**: Stores UI state for Mac Classic Frontier's "About/Agents" window (Cancoon window). Not relevant for headless/collaborative ODB.

**v6 Pattern**: `views[0]` → Cancoon record → `adrroottable` → real root table
**v7 Pattern**: `views[0]` → real root table (no Cancoon record)

### Current ODB Engine Flow

**db.new()**:
1. `odbNewFile()` calls `dbnew(fnum)` to create empty database
2. Creates `tyversion2cancoonrecord` with `versionnumber = 3`
3. Calls `dbassign()` to write Cancoon record
4. Sets `views[0]` to point at Cancoon record via `dbsetview(cancoonview, adr)`
5. Result: v6 database with 442-byte Cancoon record at `views[0]`

**db.open()**:
1. `odbOpenFile()` has v6→v7 pre-migration hook (lines 424-456)
   - Checks header byte 1 for version ≤ 6
   - Calls `migrate_32bit_to_64bit(path)` if legacy
   - Reopens with migrated file
2. Calls `dbopenfile(fnum, flreadonly)` to open database
3. Reads `views[0]` to get `adr` of Cancoon record
4. Reads `versionnumber` from Cancoon record (expects 2 or 3)
5. Calls `loadversion2cancoonfile()` to parse Cancoon record
6. Loads root table from `adr.adrroottable`

**Problem**: Step 4 fails on v7 databases because `views[0]` points to a table header, not a Cancoon record. Reading the first 2 bytes of a table header as `versionnumber` gives 0 or garbage.

### Migration Code Reference

The working migration code (`db_format.c:2123-2177`) provides the pattern:

```c
boolean migrate_32bit_to_64bit(const char *db_path) {
    // 1. Detect v6 header (check byte 1)
    // 2. Load with legacy reader (32-bit addresses)
    // 3. Set format mode to v7 (use_64bit_format = true)
    // 4. Repack all tables with v7 writer (64-bit addresses)
    // 5. Write new v7 database
    // 6. Return path to migrated file
}
```

## Requirements

### Functional Requirements

1. **Auto-Migration**: Opening a v6 guest database should auto-migrate it to v7 in memory
   - Read v6 format (32-bit addresses, Cancoon record)
   - Expand to v7 format in memory (64-bit BE addresses, no Cancoon)
   - Write v7 format on save

2. **v7 Creation**: `db.new()` should create proper v7 databases
   - No Cancoon record
   - `views[0]` points directly to root table
   - 64-bit BE format throughout

3. **Backward Compatibility**: Must open existing v6 `.root` files
   - Detect Cancoon record presence
   - Parse `adrroottable` from Cancoon record
   - Auto-migrate on open

4. **Save Behavior**: `db.save()` should write v7 format
   - No Cancoon record creation
   - Use v7 writer for all table packing
   - 64-bit BE addresses

5. **Error Handling**: Clear errors when format detection fails
   - Detect corrupted databases
   - Report version mismatches
   - Handle migration failures gracefully

### Non-Functional Requirements

1. **Consistency**: Follow existing v7 migration patterns from `db_format.c`
2. **Testing**: Comprehensive test coverage at each checkpoint
3. **Documentation**: Update `docs/GUEST_DATABASES.md` with v7 behavior
4. **Safety**: No data loss during migration, backup on destructive operations
5. **Logging**: Structured logging for format detection and migration steps

## Scope Assessment

### Files Requiring Changes

#### Core ODB Engine (`Common/source/odbengine.c`)

**Functions requiring updates:**

1. **`odbNewFile()`** (lines 380-413)
   - **Change**: Remove Cancoon record creation
   - **New behavior**: Create empty root table, set `views[0]` directly
   - **Impact**: v7 creation path

2. **`odbOpenFile()`** (lines 416-502)
   - **Change**: Detect Cancoon vs direct root table
   - **New behavior**:
     - If Cancoon present: parse `adrroottable`, continue as before
     - If direct root table: load from `views[0]` directly
   - **Impact**: v7 read path, backward compat for v6

3. **`odbSaveFile()`** (lines 505-589)
   - **Change**: Remove Cancoon record writing
   - **New behavior**: Save root table directly, set `views[0]`
   - **Impact**: v7 write path
   - **Note**: Already has v7 migration hooks (lines 513-519)

4. **`loadversion2cancoonfile()`** (lines 257-273)
   - **Change**: None (keep for v6 backward compat)
   - **New function needed**: `loadv7rootfile()` for direct root table loading

**Helper functions needed:**

5. **`odb_detect_cancoon_record()`** (new)
   - Detect if `views[0]` points to Cancoon record or root table
   - Heuristics:
     - Read first 2 bytes as `versionnumber`
     - If 2 or 3: likely Cancoon
     - If 0 or > 6: likely table header
   - Return: boolean + optional `adrroottable` if Cancoon

6. **`odb_create_root_table()`** (new)
   - Create minimal empty root table
   - Return dbaddress for `views[0]`

#### Migration Support (`Common/source/db_format.c`)

**Functions to expose:**

7. **`migrate_32bit_to_64bit()`** (line 2123)
   - **Status**: Already exposed, used in `odbOpenFile()` pre-migration hook
   - **Change**: None needed

8. **`db_format_load_legacy_adapter()`** (line 1043)
   - **Status**: Already called by `dbOpenFile()`
   - **Change**: None needed

9. **`db_format_force_strict_v7_reader()`** (line 2196)
   - **Status**: Already called by `dbOpenFile()`
   - **Change**: None needed

#### Testing Infrastructure

**New test files needed:**

10. **`tests/unit/test_odb_v7.c`** (new)
    - Unit tests for ODB engine v7 format support
    - Test Cancoon detection
    - Test v7 creation
    - Test v6→v7 auto-migration

11. **`tests/integration/test_db_verbs_v7.yaml`** (new)
    - Integration tests for `db.new()`, `db.open()`, `db.save()`
    - Test with v6 and v7 databases
    - Test auto-migration behavior

**Test databases needed:**

12. **v6 test database** (exists: `tests/fixtures/v6/test.root`)
    - Confirmed v6 format (byte 1 = 0x06)
    - Has Cancoon record at `views[0]`

13. **v7 test database** (create new)
    - Pure v7 format (byte 1 = 0x07)
    - No Cancoon record
    - `views[0]` points directly to root table

### Functions NOT Requiring Changes

The following ODB engine functions operate on opened databases and don't interact with format-specific code:

- `odbDefined()` - uses `langexpandtodotparams()`, format-agnostic
- `odbDelete()` - uses `hashtabledelete()`, format-agnostic
- `odbGetType()` - uses `langsymbolreference()`, format-agnostic
- `odbGetValue()` - uses `langsymbolreference()`, format-agnostic
- `odbSetValue()` - uses `hashtableassign()`, format-agnostic
- `odbNewTable()` - creates in-memory table, format-agnostic
- `odbCountItems()` - uses `hashcountitems()`, format-agnostic
- `odbGetNthItem()` - uses `hashgetiteminfo()`, format-agnostic
- `odbGetModDate()` - reads table metadata, format-agnostic
- `odbDisposeValue()` - disposes in-memory value, format-agnostic

These functions work with in-memory `hdlhashtable` structures that are already loaded by `odbOpenFile()`. Once the database is opened correctly (v6 or v7), these operations work identically.

## Detailed Design

### Phase 1: Format Detection Infrastructure

**Goal**: Reliably detect Cancoon records vs direct root tables

#### New Helper Function: `odb_detect_cancoon_record()`

```c
// In odbengine.c, before odbOpenFile()

/**
 * Detect if views[0] points to a Cancoon record or directly to root table.
 *
 * Heuristics:
 * - Cancoon record starts with versionnumber (short) = 2 or 3
 * - Root table starts with tydisktablerecord header:
 *   - v4+ header: [uint32 outer_len][uint32 inner_len][tydisktablerecord]
 *   - legacy header: [tydisktablerecord] directly
 *   - header.version = 0-6 typically
 *
 * @param adr Address from views[0]
 * @param is_cancoon Output: true if Cancoon record detected
 * @param adrroottable Output: if Cancoon, the adrroottable field value
 * @return true if detection succeeded, false on read error
 */
static boolean odb_detect_cancoon_record(
    dbaddress adr,
    boolean *is_cancoon,
    dbaddress *adrroottable
) {
    short versionnumber;

    // Read first 2 bytes as potential versionnumber
    if (!dbreference(adr, sizeof(versionnumber), &versionnumber))
        return false;

    disktomemshort(versionnumber);

    // Cancoon versionnumber is 2 or 3
    if (versionnumber == 2 || versionnumber == 3) {
        *is_cancoon = true;

        // Read adrroottable from Cancoon record (offset 2)
        dbaddress rootadr;
        if (!dbreference(adr + sizeof(short), sizeof(rootadr), &rootadr))
            return false;

        disktomemlong(rootadr);
        *adrroottable = rootadr;

        log_debug(LOG_COMP_DB,
            "odb_detect_cancoon_record: Cancoon v%d detected, adrroottable=0x%llx",
            (int)versionnumber, (unsigned long long)*adrroottable);

        return true;
    }

    // Not a Cancoon record - assume direct root table
    *is_cancoon = false;
    *adrroottable = adr;  // views[0] is the root table

    log_debug(LOG_COMP_DB,
        "odb_detect_cancoon_record: Direct root table at 0x%llx",
        (unsigned long long)adr);

    return true;
}
```

**Testing checkpoint**: Unit test verifying Cancoon detection
- Test with v6 database (Cancoon present)
- Test with v7 database (no Cancoon)
- Test with corrupted data (error handling)

### Phase 2: v7 Database Creation (`odbNewFile`)

**Goal**: Create v7 databases with no Cancoon record using `.root7` extension (Phase 1)

#### Updated `odbNewFile()` Implementation (Phase 1)

```c
pascal boolean odbNewFile(hdlfilenum fnum) {
    /*
     * Create a new v7 format guest database with .root7 extension.
     *
     * Phase 1 behavior (Development/Stabilization):
     * - Creates .root7 files (temporary extension)
     * - Original .root files remain v6 (unchanged)
     * - Production UserTalk code continues working with .root
     *
     * v7 format:
     * - No Cancoon record (UI-centric legacy structure)
     * - views[0] points directly to root table
     * - 64-bit BE addresses throughout
     * - Database header version = 7
     */

    setemptystring(bserror);

    // NOTE: In Phase 1, caller must provide .root7 path
    // (Path normalization happens in db.new() verb implementation)

    // Create new empty database file (writes v7 header)
    if (!dbnew(fnum))
        return false;

    // Create minimal empty root table
    hdlhashtable hroottable;
    if (!newtablestack(&hroottable)) {
        dbdispose();
        return false;
    }

    // Pack root table to database
    dbaddress adrroottable = nildbaddress;
    {
        // Ensure v7 packing mode
        db_format_mode mode = {true, true, false};  // 64-bit, adapter_repack, no drop_cancoon
        db_format_mode_push(&mode);

        boolean fl = hashpacktable(hroottable, &adrroottable, false);

        db_format_mode_pop();

        if (!fl) {
            tablereleasememory(hroottable, false);
            dbdispose();
            return false;
        }
    }

    // Set views[0] to point directly at root table (no Cancoon)
    dbsetview(cancoonview, adrroottable);

    log_debug(LOG_COMP_DB,
        "odbNewFile: Created v7 database (.root7), views[0]=0x%llx",
        (unsigned long long)adrroottable);

    // Clean up in-memory table
    tablereleasememory(hroottable, false);

    // Close database (flushes header)
    if (!dbclose()) {
        dbdispose();
        return false;
    }

    cancoonglobals = nil;
    dbdispose();

    return true;
}
```

**Path Handling in `db.new()` Verb** (Phase 1):

The `db.new()` UserTalk verb implementation must append `.root7` extension:

```c
// In dbverbs.c, newnewfunc() or similar
boolean newnewfunc(hdltreenode hparam1, tyvaluerecord *vreturned) {
    bigstring bspath;

    // Get path parameter
    flnextparamislast = true;
    if (!getstringvalue(hparam1, 1, bspath))
        return false;

    // Phase 1: Ensure .root7 extension
    // If user provides "mydb.root", change to "mydb.root7"
    // If user provides "mydb.root7", keep as-is
    if (!pathendswith(bspath, "\p.root7")) {
        if (pathendswith(bspath, "\p.root")) {
            // Replace .root with .root7
            setstringlength(bspath, stringlength(bspath) - 5);  // Remove ".root"
        }
        pushstring("\p.root7", bspath);  // Append .root7
    }

    log_debug(LOG_COMP_DB, "db.new: Creating v7 database at %s", bspath);

    // Continue with file creation...
}
```

**Testing checkpoint**: Integration test for `db.new()`
- Verify `.root7` extension used
- Verify v7 header written (byte 1 = 0x07)
- Verify no Cancoon record
- Verify `views[0]` points to root table
- Verify can re-open created database with `.root7` path

### Phase 3: v7 Database Opening (`odbOpenFile`)

**Goal**: Open both v6 (`.root`) and v7 (`.root7`) databases with format detection (Phase 1)

#### Updated `odbOpenFile()` Implementation (Phase 1)

```c
pascal boolean odbOpenFile(hdlfilenum fnum, odbref *odb, boolean flreadonly) {
    hdlcancoonrecord hc = nil;
    dbaddress cancoonview_adr;
    boolean is_cancoon = false;
    dbaddress adrroottable = nildbaddress;

    setemptystring(bserror);

    // Phase 1: Migration hook detects v6 databases and creates .root7
    // - Input: "mydb.root" (v6 format)
    // - Migration creates: "mydb.root7" (v7 format)
    // - Original "mydb.root" preserved (unchanged)
    // - Reopens with "mydb.root7" path
#if defined(FRONTIER_HEADLESS)
    {
        extern const char* headless_fnum_path(hdlfilenum fnum);
        extern boolean headless_reopen_fnum(hdlfilenum fnum, const char *path, boolean flreadonly);
        const char *path = headless_fnum_path(fnum);
        if (path != NULL) {
            FILE *fp = fopen(path, "rb");
            if (fp) {
                unsigned char hdr[2];
                if (fread(hdr, 1, 2, fp) == 2) {
                    log_debug(LOG_COMP_DB, "odbOpenFile pre-open header: path=%s ver=%u",
                        path, (unsigned)hdr[1]);
                    if (hdr[1] <= 6) {
                        fclose(fp);

                        // Phase 1: Auto-migrate v6 to v7 with .root7 extension
                        // migrate_32bit_to_64bit() creates .root7 file
                        if (!migrate_32bit_to_64bit(path))
                            return false;

                        // Get migrated path (should be .root7)
                        char migrated_path[1024];
                        if (!db_format_last_backup_path(migrated_path, sizeof migrated_path))
                            return false;

                        log_debug(LOG_COMP_DB,
                            "odbOpenFile: Migrated v6 %s -> v7 %s",
                            path, migrated_path);

                        // Reopen with .root7 path
                        if (!headless_reopen_fnum(fnum, migrated_path, flreadonly))
                            return false;
                        path = migrated_path;
                    }
                    else {
                        fclose(fp);
                    }
                }
                else {
                    fclose(fp);
                }
            }
        }
    }
#endif

    // Open database (handles v7 format detection)
    if (!dbopenfile(fnum, flreadonly))
        return false;

    // Get views[0] address
    dbgetview(cancoonview, &cancoonview_adr);

    // Detect Cancoon record vs direct root table
    if (!odb_detect_cancoon_record(cancoonview_adr, &is_cancoon, &adrroottable))
        goto error;

    // Create ODB record
    if (!newcancoonrecord(&cancoonglobals))
        goto error;

    hc = cancoonglobals;
    (**hc).hdatabase = databasedata;

    // Load root table based on format
    if (is_cancoon) {
        // v6 format with Cancoon record - use existing loader
        log_debug(LOG_COMP_DB, "odbOpenFile: Loading v6 format (Cancoon record)");

        // Verify Cancoon versionnumber is valid
        short versionnumber;
        if (!dbreference(cancoonview_adr, sizeof(versionnumber), &versionnumber))
            goto error;
        disktomemshort(versionnumber);

        if (versionnumber != 2 && versionnumber != 3) {
            alertdialog((ptrstring)"\x59"
                "The version number of this database file is not recognized by this version of Frontier.");
            goto error;
        }

        // Load using existing v6 loader
        if (!loadversion2cancoonfile(cancoonview_adr, hc))
            goto error;
    }
    else {
        // v7 format - load root table directly
        log_debug(LOG_COMP_DB, "odbOpenFile: Loading v7 format (direct root table)");

        if (!ccloadsystemtable(hc, adrroottable))
            goto error;
    }

    *odb = (odbref)hc;

    log_debug(LOG_COMP_DB,
        "odbOpenFile: Opened %s database, root table loaded at 0x%llx",
        is_cancoon ? "v6 (Cancoon)" : "v7 (direct)",
        (unsigned long long)adrroottable);

    return true;

error:
    dbdispose();
    disposecancoonrecord(hc);
    clearcancoonglobals();
    return false;
}
```

**Migration Path Handling** (Phase 1):

The `migrate_32bit_to_64bit()` function must be updated to create `.root7` output:

```c
// In db_format.c, migrate_32bit_to_64bit()
boolean migrate_32bit_to_64bit(const char *db_path) {
    // ... existing migration logic ...

    // Phase 1: Create .root7 output path
    // Input: "mydb.root" (v6)
    // Output: "mydb.root7" (v7)
    char output_path[1024];
    snprintf(output_path, sizeof(output_path), "%s7", db_path);

    // If input was already .root7, use .root7.migrated to avoid collision
    if (strstr(db_path, ".root7") != NULL) {
        snprintf(output_path, sizeof(output_path), "%s.migrated", db_path);
    }

    log_debug(LOG_COMP_DB, "migrate_32bit_to_64bit: %s -> %s", db_path, output_path);

    // Write v7 database to output_path
    // ... existing write logic ...

    // Store output path for db_format_last_backup_path()
    db_format_set_last_backup_path(output_path);

    return true;
}
```

**Testing checkpoint**: Integration test for `db.open()`
- Open v6 database (`.root`) - verify auto-migration creates `.root7`
- Open v7 database (`.root7`) - verify direct load, no migration
- Verify original `.root` preserved (unchanged)
- Verify root table accessible after open
- Test read-only vs read-write modes

### Phase 4: v7 Database Saving (`odbSaveFile`)

**Goal**: Save databases in v7 format (no Cancoon record)

#### Updated `odbSaveFile()` Implementation

```c
pascal boolean odbSaveFile(odbref odb) {
    hdlcancoonrecord hc = (hdlcancoonrecord) odb;
    dbaddress adrroottable = nildbaddress;

    setemptystring(bserror);

    // Ensure v7 format (existing code, lines 513-519)
    if (!db_format_mode_current().use_64bit_format) {
        extern boolean db_migrate_reopen_if_legacy(odbref *podb);
        if (!db_migrate_reopen_if_legacy(&odb))
            return false;
        hc = (hdlcancoonrecord) odb;  // Update handle after migration
    }

    setcancoonglobals(hc);

    // If accessing window (not standalone ODB), use shell save
    if ((**hc).accesssing) {
        return shellsave((**hc).shellwindow);
    }

    // Save root table in v7 format
    {
        db_format_mode mode = {true, true, false};  // 64-bit, adapter_repack, no drop_cancoon
        db_format_mode_push(&mode);

        boolean fl = tablesavesystemtable((**hc).hrootvariable, &adrroottable);

        db_format_mode_pop();

        if (!fl)
            return false;
    }

    // Set views[0] directly to root table (no Cancoon record)
    dbsetview(cancoonview, adrroottable);

    log_debug(LOG_COMP_DB,
        "odbSaveFile: Saved v7 database, views[0]=0x%llx",
        (unsigned long long)adrroottable);

    // Flush release stack
    {
        db_context ctx;
        db_context_init(&ctx);
        dbflushreleasestack_context(&ctx);
    }

    return true;
}
```

**Testing checkpoint**: Integration test for `db.save()`
- Create new database, add data, save, verify v7 format
- Open v6 database, modify, save, verify v7 format (migration)
- Verify no Cancoon record in saved file
- Verify can re-open saved database

### Phase 5: Testing & Validation

#### Unit Tests (`tests/unit/test_odb_v7.c`)

```c
// Test 1: Cancoon record detection
void test_odb_detect_cancoon_v6(void) {
    // Open v6 test database
    // Verify Cancoon detected
    // Verify adrroottable extracted correctly
}

void test_odb_detect_cancoon_v7(void) {
    // Open v7 test database
    // Verify no Cancoon detected
    // Verify views[0] used as root table address
}

// Test 2: v7 database creation
void test_odb_new_creates_v7(void) {
    // Create new database with odbNewFile()
    // Verify header version = 7
    // Verify no Cancoon record
    // Verify views[0] points to root table
}

// Test 3: v6 auto-migration
void test_odb_open_v6_auto_migrates(void) {
    // Open v6 database
    // Verify migration triggered
    // Verify opened as v7 in memory
    // Verify can access root table
}

// Test 4: v7 opening
void test_odb_open_v7_direct(void) {
    // Open v7 database
    // Verify no migration
    // Verify root table loaded
    // Verify can access root table
}

// Test 5: v7 saving
void test_odb_save_writes_v7(void) {
    // Create new database
    // Add some data
    // Save with odbSaveFile()
    // Close and reopen
    // Verify v7 format
    // Verify no Cancoon record
    // Verify data preserved
}
```

#### Integration Tests (`tests/integration/test_db_verbs_v7.yaml`)

```yaml
test_suite: "DB Verbs v7 Format Support (Phase 1 - .root7 extension)"

setup:
  description: "Prepare test databases"
  steps:
    - Ensure v6 test database exists (tests/fixtures/v6/test.root)
    - Create clean test directory

tests:
  - name: "db.new() creates v7 database with .root7 extension"
    script: |
      local(testdb = "{FRONTIER_TEST_TMP_DIR}/new_v7.root7");
      file.delete(testdb);  /* ensure clean slate */
      db.new(testdb);  /* Should create .root7 file */
      /* Verify .root7 file exists */
      local(exists = file.exists(testdb));
      return exists;
    expected_success: true
    expected_result: true

  - name: "db.new() with .root path creates .root7 file"
    script: |
      local(testdb_root = "{FRONTIER_TEST_TMP_DIR}/test.root");
      local(testdb_root7 = "{FRONTIER_TEST_TMP_DIR}/test.root7");
      file.delete(testdb_root);
      file.delete(testdb_root7);
      db.new(testdb_root);  /* User provides .root, should create .root7 */
      /* Verify .root7 file created, NOT .root */
      local(root7_exists = file.exists(testdb_root7));
      local(root_exists = file.exists(testdb_root));
      return root7_exists and not root_exists;
    expected_success: true
    expected_result: true

  - name: "db.open() opens v6 database, creates .root7 migration"
    script: |
      local(v6db = "tests/fixtures/v6/test.root");
      local(v7db = "tests/fixtures/v6/test.root7");
      file.delete(v7db);  /* Clean previous migration */
      local(odbref);
      if db.open(v6db, @odbref, false) {
        /* Should have created test.root7 */
        local(v7exists = file.exists(v7db));
        local(defined = db.defined(odbref, "myTable"));
        db.close(odbref);
        return v7exists and defined;
      };
      return false;
    expected_success: true
    expected_result: true

  - name: "db.open() preserves original v6 .root file"
    script: |
      local(v6db = "tests/fixtures/v6/test.root");
      local(v7db = "tests/fixtures/v6/test.root7");
      file.delete(v7db);
      local(odbref);
      if db.open(v6db, @odbref, false) {
        db.close(odbref);
        /* Verify original .root still exists and unchanged */
        return file.exists(v6db);
      };
      return false;
    expected_success: true
    expected_result: true

  - name: "db.open() opens v7 database (direct, no migration)"
    script: |
      local(testdb = "{FRONTIER_TEST_TMP_DIR}/test_v7.root7");
      /* Create v7 database first */
      db.new(testdb);
      local(odbref);
      if db.open(testdb, @odbref, false) {
        db.close(odbref);
        return true;
      };
      return false;
    expected_success: true
    expected_result: true

  - name: "db.save() writes v7 format to .root7 file"
    script: |
      local(testdb = "{FRONTIER_TEST_TMP_DIR}/save_v7.root7");
      file.delete(testdb);
      db.new(testdb);
      local(odbref);
      if db.open(testdb, @odbref, false) {
        db.newTable(odbref, "testTable");
        db.save(odbref);
        db.close(odbref);
        /* Reopen to verify persistence */
        if db.open(testdb, @odbref, true) {
          local(exists = db.defined(odbref, "testTable"));
          db.close(odbref);
          return exists;
        };
      };
      return false;
    expected_success: true
    expected_result: true

  - name: "db.open() v6 database, modify, save as v7 (.root7)"
    script: |
      local(v6db = "tests/fixtures/v6/test.root");
      local(tmpdir = "{FRONTIER_TEST_TMP_DIR}");
      local(v6copy = tmpdir + "/test.root");
      local(v7db = tmpdir + "/test.root7");
      file.delete(v6copy);
      file.delete(v7db);
      file.copy(v6db, v6copy);  /* Copy v6 to temp */
      local(odbref);
      if db.open(v6copy, @odbref, false) {  /* Opens v6, creates .root7 */
        db.newTable(odbref, "newTable");
        db.save(odbref);  /* Should save to .root7 */
        db.close(odbref);
        /* Verify .root7 exists and has new table */
        if db.open(v7db, @odbref, true) {
          local(has_new = db.defined(odbref, "newTable"));
          db.close(odbref);
          return has_new;
        };
      };
      return false;
    expected_success: true
    expected_result: true
```

#### Test Database Setup (Phase 1)

**v6 Test Database** (`tests/fixtures/v6/test.root`):
- Already exists (verified: byte 1 = 0x06)
- Contains Cancoon record at `views[0]`
- Has sample tables for testing
- **Never modified** - preserved as reference v6 database

**v7 Test Database** (create new: `tests/fixtures/v7/test.root7`):
- Created using `odbNewFile()` after implementation
- **NOTE**: Uses `.root7` extension (Phase 1 naming)
- No Cancoon record
- `views[0]` points directly to root table
- Has same sample tables as v6 version

**Creation script** (`tools/create_v7_test_database.sh`) - Phase 1 version:
```bash
#!/bin/bash
# Create v7 test database for ODB engine tests (Phase 1 - .root7 extension)

set -e

V7_DB="tests/fixtures/v7/test.root7"
mkdir -p "$(dirname "$V7_DB")"

# Clean up any existing .root7 file
rm -f "$V7_DB"

# Create v7 database using frontier-cli
# NOTE: db.new() should automatically create .root7 file in Phase 1
./frontier-cli/frontier-cli -e "
db.new('$V7_DB');
local(odbref);
db.open('$V7_DB', @odbref, false);
db.newTable(odbref, 'myTable');
db.setValue(odbref, 'myTable.x', 42);
db.setValue(odbref, 'myTable.name', 'test');
db.save(odbref);
db.close(odbref);
return true;
"

# Verify v7 format and .root7 extension
if [ -f "$V7_DB" ]; then
    echo "Created v7 test database: $V7_DB"
    xxd "$V7_DB" | head -5
else
    echo "ERROR: Expected .root7 file not created!"
    exit 1
fi
```

**Phase 2 Migration** (Future):
- After transitioning to Phase 2, rename `test.root7` → `test.root`
- Update script to create `test.root` (v7 format)
- Original v6 database preserved as `test.root.v6.bkp`

### Phase 6: Documentation Updates

#### Update `docs/GUEST_DATABASES.md`

Create new documentation file detailing v7 guest database behavior with two-phase migration strategy:

```markdown
# Guest Databases - v7 Format Guide

## Overview

Guest databases are user-created databases opened with `db.new()` and `db.open()`.
As of v7 format migration, guest databases follow the same format as system databases.

**IMPORTANT**: We are currently in **Phase 1 (Development/Stabilization)** of the v7 migration.
This means temporary `.root7` file extensions are used during development.

## Two-Phase Migration Strategy

### Phase 1: Development/Stabilization (Current)

**File Extension Behavior:**
- `.root` = v6 format (original, preserved)
- `.root7` = v7 format (temporary extension during development)

**Benefits:**
- Existing `.root` files remain untouched
- Production UserTalk code continues working (opens `.root` as v6)
- Test code explicitly opts into v7 (uses `.root7` paths)
- Easy rollback if issues discovered

**Why .root7?**
The temporary `.root7` extension signals experimental v7 format during stabilization.
This allows both formats to coexist safely without breaking production code.

### Phase 2: Production (Future)

**File Extension Behavior:**
- `.root` = v7 format (canonical)
- `.root.v6.bkp` = v6 backup (created during migration)

**Transition:**
Once v7 is proven stable (30+ days, all tests passing, user approval):
- `db.new()` will create `.root` files (v7 format)
- Migration will rename `.root` → `.root.v6.bkp`, create new `.root` (v7)
- Production code continues working unchanged (opens `.root`, now gets v7)

## Format Changes (v6 → v7)

### v6 Format (Legacy)
- Contains 442-byte Cancoon record at `views[0]`
- Cancoon record stores UI state (fonts, window positions, agents)
- `adrroottable` field points to actual root table
- 32-bit addresses throughout

### v7 Format (Current)
- No Cancoon record
- `views[0]` points directly to root table
- 64-bit big-endian addresses
- UI state removed (headless-compatible)

## Auto-Migration (Phase 1)

Opening a v6 guest database (`.root`) automatically migrates it to v7 (`.root7`):
- Detects v6 format (header byte 1 ≤ 6)
- Creates `.root7` file with migrated v7 content
- Original `.root` file preserved (unchanged)
- Subsequent operations use `.root7` file

**Example:**
```usertalk
db.open("mydb.root", @odbref, false)
/* Result: mydb.root7 created, mydb.root preserved */
```

## API Behavior (Phase 1)

### db.new(path)
Creates new v7 format database with `.root7` extension:

```usertalk
db.new("mydb.root")       /* Creates mydb.root7 (v7 format) */
db.new("mydb.root7")      /* Creates mydb.root7 (v7 format) */
```

**Behavior:**
- If path ends with `.root`, replaces with `.root7`
- If path ends with `.root7`, uses as-is
- No Cancoon record created
- Empty root table initialized
- `views[0]` points directly to root table

### db.open(path, @odbref, readonly)
Opens v6 (`.root`) or v7 (`.root7`) database:

```usertalk
db.open("mydb.root", @odbref, false)    /* Opens v6, creates mydb.root7 */
db.open("mydb.root7", @odbref, false)   /* Opens v7 directly */
```

**Behavior:**
- Detects format automatically (reads header byte 1)
- v6 detected: creates `.root7` migration, opens migrated file
- v7 detected: opens directly, no migration
- Original `.root` file never modified
- Returns ODB reference handle

### db.save(odbref)
Saves in v7 format to `.root7` file:

```usertalk
db.save(odbref)  /* Saves to .root7 file, v7 format */
```

**Behavior:**
- No Cancoon record written
- Uses v7 writer (64-bit BE addresses)
- Always writes to `.root7` file (Phase 1)
- Previous format irrelevant (always outputs v7)

## Migration Workflow Example

**Starting with v6 database:**
```usertalk
/* Initial state: mydb.root (v6 format) exists */

local(odbref);
db.open("mydb.root", @odbref, false);
/* Result: mydb.root7 created (v7), mydb.root preserved */

db.newTable(odbref, "settings");
db.save(odbref);
/* Result: mydb.root7 updated with new table */

db.close(odbref);

/* Files on disk:
   - mydb.root (original v6, unchanged)
   - mydb.root7 (v7 format, has settings table)
*/
```

**Creating new v7 database:**
```usertalk
db.new("config.root");
/* Result: config.root7 created (v7 format) */

local(odbref);
db.open("config.root7", @odbref, false);
db.setValue(odbref, "version", "1.0");
db.save(odbref);
db.close(odbref);

/* Files on disk:
   - config.root7 (v7 format, has version value)
*/
```

## Testing

See `tests/integration/test_db_verbs_v7.yaml` for comprehensive test suite.

Test databases:
- v6: `tests/fixtures/v6/test.root` (never modified)
- v7: `tests/fixtures/v7/test.root7` (Phase 1 naming)

## Phase 2 Transition (Future)

When transitioning to Phase 2 (production):
1. Update `db.new()` to create `.root` files (v7 format)
2. Update migration to create `.root.v6.bkp` backups
3. Run migration script: `tools/migrate_root7_to_root.sh`
4. All `.root7` files renamed to `.root`
5. Original `.root` (v6) preserved as `.root.v6.bkp`

This documentation will be updated when Phase 2 begins.
```

## Migration Infrastructure Changes (Phase 1)

### Required Changes to `db_format.c`

The `migrate_32bit_to_64bit()` function must be updated to output `.root7` files instead of replacing the original:

**Current Behavior** (System Root):
- Input: `Frontier.root`
- Output: `Frontier.root7` (temporary backup path)
- Original: `Frontier.root` preserved

**Phase 1 Behavior** (Guest Databases):
- Input: `mydb.root` (v6 format)
- Output: `mydb.root7` (v7 format)
- Original: `mydb.root` preserved

**Code Changes Needed:**

```c
// In db_format.c, migrate_32bit_to_64bit()

boolean migrate_32bit_to_64bit(const char *db_path) {
    // ... existing setup ...

    // Phase 1: Determine output path
    char output_path[1024];

    // Check if input is system root (special case - existing behavior)
    if (strstr(db_path, "Frontier.root") != NULL ||
        strstr(db_path, "Frontier.root") != NULL) {
        // System root: use existing Frontier.root7 naming
        const char *dir = dirname_portable(db_path);
        snprintf(output_path, sizeof(output_path), "%s/Frontier.root7", dir);
    }
    // Guest database: append "7" to create .root7
    else {
        snprintf(output_path, sizeof(output_path), "%s7", db_path);

        // If input was already .root7, use .root7.migrated to avoid collision
        if (strstr(db_path, ".root7") != NULL) {
            snprintf(output_path, sizeof(output_path), "%s.migrated", db_path);
        }
    }

    log_info(LOG_COMP_DB,
        "migrate_32bit_to_64bit: Migrating v6 -> v7: %s -> %s",
        db_path, output_path);

    // ... existing migration logic ...
    // Load v6 database with legacy reader
    // Repack all tables with v7 writer
    // Write to output_path

    // Store output path for retrieval by odbOpenFile()
    db_format_set_last_backup_path(output_path);

    return true;
}
```

### Helper Functions Needed

**Path Management:**

```c
// New helper in db_format.c

static char last_migration_output[1024] = {0};

void db_format_set_last_backup_path(const char *path) {
    strncpy(last_migration_output, path, sizeof(last_migration_output) - 1);
    last_migration_output[sizeof(last_migration_output) - 1] = '\0';
}

boolean db_format_last_backup_path(char *buf, size_t bufsize) {
    if (last_migration_output[0] == '\0')
        return false;
    strncpy(buf, last_migration_output, bufsize - 1);
    buf[bufsize - 1] = '\0';
    return true;
}
```

**Directory Name Extraction:**

```c
// Portable dirname() helper
static const char* dirname_portable(const char *path) {
    static char dirbuf[1024];
    const char *last_slash = strrchr(path, '/');
    if (last_slash == NULL)
        return ".";
    size_t dirlen = last_slash - path;
    if (dirlen >= sizeof(dirbuf))
        dirlen = sizeof(dirbuf) - 1;
    memcpy(dirbuf, path, dirlen);
    dirbuf[dirlen] = '\0';
    return dirbuf;
}
```

### DB Verbs Path Handling (Phase 1)

**Update `dbverbs.c` for `.root7` extension:**

```c
// In dbverbs.c, db.new() implementation

boolean dbnewfunc(hdltreenode hparam1, tyvaluerecord *vreturned) {
    bigstring bspath;

    flnextparamislast = true;
    if (!getstringvalue(hparam1, 1, bspath))
        return false;

    // Phase 1: Ensure .root7 extension
    odb_ensure_root7_extension(bspath);  // Helper function

    log_debug(LOG_COMP_DB, "db.new: Creating v7 database at %s", bspath);

    // Create file, call odbNewFile()...
}
```

**Helper Function:**

```c
// In odbengine.c or dbverbs.c

static void odb_ensure_root7_extension(bigstring bspath) {
    // If path ends with ".root", replace with ".root7"
    // If path ends with ".root7", keep as-is
    // Otherwise, append ".root7"

    short len = stringlength(bspath);

    // Check if ends with ".root"
    if (len >= 5) {
        bigstring suffix;
        midstring(bspath, len - 4, 5, suffix);  // Get last 5 chars
        if (equalstrings(suffix, "\p.root")) {
            // Remove ".root"
            setstringlength(bspath, len - 5);
            // Append ".root7"
            pushstring("\p.root7", bspath);
            return;
        }
    }

    // Check if ends with ".root7"
    if (len >= 6) {
        bigstring suffix;
        midstring(bspath, len - 5, 6, suffix);
        if (equalstrings(suffix, "\p.root7")) {
            // Already has .root7, keep as-is
            return;
        }
    }

    // Neither .root nor .root7 - append .root7
    pushstring("\p.root7", bspath);
}
```

## Implementation Roadmap

### Milestone 1: Infrastructure (Days 1-2) - Phase 1

**Tasks:**
1. Add `odb_detect_cancoon_record()` helper function
2. Add `odb_ensure_root7_extension()` helper function (Phase 1 naming)
3. Update `migrate_32bit_to_64bit()` to create `.root7` output
4. Add path management helpers (`db_format_set_last_backup_path()`, etc.)
5. Write unit tests for Cancoon detection
6. Create v7 test database (`tools/create_v7_test_database.sh` - Phase 1 version)

**Testing:**
- Unit test: Cancoon detection with v6 database
- Unit test: Cancoon detection with v7 database
- Unit test: `.root7` extension handling
- Unit test: Migration creates `.root7` (not `.root`)

**Success Criteria:**
- Cancoon detection reliably distinguishes v6/v7 formats
- Can create minimal root tables
- Migration outputs `.root7` files (Phase 1)
- v7 test database exists as `test.root7` and verified
- Original `.root` files preserved during migration

### Milestone 2: v7 Creation (Days 3-4) - Phase 1

**Tasks:**
1. Update `odbNewFile()` to create v7 databases (no code change - already v7)
2. Update `db.new()` verb to enforce `.root7` extension (Phase 1)
3. Write integration test for `db.new()` with `.root7` paths
4. Verify v7 header written correctly
5. Verify no Cancoon record created

**Testing:**
- Integration test: `db.new("mydb.root")` creates `mydb.root7`
- Integration test: `db.new("mydb.root7")` creates `mydb.root7`
- Verify header (xxd check: byte 1 = 0x07)
- Verify no Cancoon record (views[0] points to table)
- Verify NO `.root` file created (Phase 1 requirement)
- Reopen created database with `.root7` path

**Success Criteria:**
- `db.new()` creates valid `.root7` databases (Phase 1)
- User provides `.root`, gets `.root7` file
- Created databases can be reopened
- No Cancoon record present
- No `.root` files created (only `.root7`)

### Milestone 3: v7 Opening (Days 5-6) - Phase 1

**Tasks:**
1. Update `odbOpenFile()` to detect and handle both formats
2. Test opening v6 databases (`.root`) - auto-migration to `.root7`
3. Test opening v7 databases (`.root7`) - direct load
4. Write integration tests for `db.open()` with Phase 1 paths
5. Verify original `.root` preservation during migration

**Testing:**
- Integration test: Open `test.root` (v6) - verify `test.root7` created
- Integration test: Open `test.root7` (v7) - verify direct load, no migration
- Verify original `test.root` preserved (unchanged)
- Verify auto-migration triggered for v6
- Verify root table accessible after open
- Test read-only vs read-write modes

**Success Criteria:**
- Can open both v6 (`.root`) and v7 (`.root7`) databases
- Auto-migration creates `.root7` from `.root` (Phase 1)
- Original `.root` files never modified
- Root table accessible in both cases
- No regressions in existing ODB operations

### Milestone 4: v7 Saving (Days 7-8) - Phase 1

**Tasks:**
1. Update `odbSaveFile()` to write v7 format (already done)
2. Verify saves go to `.root7` files (Phase 1)
3. Write integration test for `db.save()` with `.root7` paths
4. Test saving new databases
5. Test saving migrated v6 databases

**Testing:**
- Integration test: Create `.root7`, modify, save to `.root7`
- Integration test: Open `.root` (v6), modify, save to `.root7`
- Verify v7 format persisted (no Cancoon)
- Verify saves go to `.root7`, NOT `.root` (Phase 1)
- Verify data preserved across save/reopen

**Success Criteria:**
- `db.save()` writes v7 format to `.root7` files
- No Cancoon record in saved files
- Data persists correctly
- Can reopen saved `.root7` databases
- Original `.root` files never modified by saves

### Milestone 5: Full Integration Testing (Days 9-10) - Phase 1

**Tasks:**
1. Run full integration test suite with `.root7` paths
2. Test with real guest databases (manila.root, prefs.root) - verify `.root7` creation
3. Performance testing (large databases)
4. Edge case testing (corrupted databases, extension handling)
5. Verify Phase 1 invariants (`.root` never modified, `.root7` always created)

**Testing:**
- Full integration test suite passes with `.root7` naming
- Real guest databases open/save correctly (creates `.root7`)
- Original `.root` files preserved (unchanged)
- Performance acceptable (no regressions)
- Error handling works correctly
- Extension handling correct (`.root` → `.root7`, `.root7` → `.root7`)

**Success Criteria:**
- All integration tests pass with Phase 1 naming
- Real-world databases work (create `.root7` files)
- No performance regressions
- Proper error messages for edge cases
- Phase 1 file coexistence works (`.root` + `.root7` side-by-side)

### Milestone 6: Documentation & Cleanup (Days 11-12) - Phase 1

**Tasks:**
1. Create `docs/GUEST_DATABASES.md` with Phase 1 two-phase strategy
2. Update inline comments in `odbengine.c` (Phase 1 `.root7` behavior)
3. Add logging statements for format detection and migration
4. Document Phase 1 → Phase 2 transition plan
5. Code review and cleanup

**Testing:**
- Documentation review (verify Phase 1 strategy clearly explained)
- Code review (check against logging standards)
- Final integration test run
- Verify all documentation mentions `.root7` (Phase 1)

**Success Criteria:**
- Documentation complete and accurate (Phase 1 focus)
- Two-phase strategy clearly explained (why `.root7`?)
- Code follows project standards
- All tests passing
- Ready for PR
- Phase 2 transition plan documented

### Milestone 7 (Future): Phase 1 → Phase 2 Transition

**Trigger:** After 30+ days stable, all tests passing, user approval

**Tasks:**
1. Update `db.new()` to create `.root` files (v7 format)
2. Update migration to create `.root.v6.bkp` backups
3. Update `odbOpenFile()` to handle `.root.v6.bkp` fallback
4. Create migration script: `tools/migrate_root7_to_root.sh`
5. Update all test databases from `.root7` to `.root`
6. Update documentation for Phase 2 behavior

**Testing:**
- All integration tests pass with `.root` naming
- Migration script successfully renames `.root7` → `.root`
- Backup creation works (`.root` → `.root.v6.bkp`)
- Rollback plan tested (restore from `.root.v6.bkp`)

**Success Criteria:**
- Production uses standard `.root` extension (v7 format)
- `.root.v6.bkp` backups created for v6 files
- All tests passing with new naming
- Rollback plan documented and tested

## Testing Strategy

### Test Pyramid

**Unit Tests** (Fast, Isolated):
- Cancoon record detection logic
- Root table creation
- Format detection heuristics
- Error handling paths

**Integration Tests** (Medium, End-to-End):
- `db.new()` creates v7 databases
- `db.open()` opens v6 and v7 databases
- `db.save()` writes v7 format
- Auto-migration behavior
- Data persistence

**Manual Tests** (Slow, Real-World):
- Open real guest databases (manila.root, prefs.root)
- Large database performance
- Migration of production data
- Cross-platform compatibility (if applicable)

### Test Databases

**Required Test Databases:**

1. **v6 Small** (`tests/fixtures/v6/test.root`)
   - Existing v6 database
   - Has Cancoon record
   - Small size for quick tests
   - Contains sample tables

2. **v7 Small** (`tests/fixtures/v7/test.root`)
   - Created by migration or `odbNewFile()`
   - No Cancoon record
   - Same structure as v6 version
   - For direct v7 open tests

3. **v6 Large** (Optional, `tests/fixtures/v6/large.root`)
   - Larger v6 database for performance testing
   - Multiple tables, deep nesting
   - Stress test migration code

4. **Corrupted** (Optional, `tests/fixtures/corrupted/`)
   - Invalid headers
   - Corrupted Cancoon records
   - Truncated files
   - For error handling tests

### Frequent Testing Checkpoints

**After Each Code Change:**
1. Run related unit tests (`make test-unit`)
2. Verify compilation warnings clean
3. Check logging output for format detection

**After Each Milestone:**
1. Run full unit test suite
2. Run integration tests for affected verbs
3. Manual test with real database
4. Code review checkpoint

**Before PR:**
1. Run all tests (`make test-all`)
2. Test with multiple databases
3. Performance regression check
4. Documentation review
5. Code cleanup (logging, comments)

## Risks & Mitigations

### Risk 1: Data Loss During Migration

**Probability**: Medium
**Impact**: High

**Mitigation**:
- Migration creates backup before modification (existing behavior in `migrate_32bit_to_64bit()`)
- Test with copies of databases, never originals
- Comprehensive integration tests verify data preservation
- Document backup recommendation in `docs/GUEST_DATABASES.md`

### Risk 2: Cancoon Detection False Positives

**Probability**: Low
**Impact**: Medium

**Mitigation**:
- Conservative heuristics (only versions 2-3 are Cancoon)
- Additional validation (read adrroottable, verify it's valid)
- Logging for format detection decisions
- Fallback error handling if root table load fails

### Risk 3: Performance Regression

**Probability**: Low
**Impact**: Low

**Mitigation**:
- Follow existing migration patterns (already proven fast)
- Auto-migration happens once per database (cached in memory)
- No additional I/O compared to current v6 opening
- Performance testing with large databases

### Risk 4: Backward Compatibility Break

**Probability**: Low
**Impact**: High

**Mitigation**:
- Maintain v6 opening support (loadversion2cancoonfile)
- Auto-migration preserves v6 databases on disk (creates backup)
- Integration tests verify v6 databases still open
- Document v6→v7 migration behavior clearly

### Risk 5: Format Mode State Leakage

**Probability**: Medium
**Impact**: Medium

**Mitigation**:
- Use `db_format_mode_push/pop()` for scoped state changes
- Follow existing patterns from `odbSaveFile()` (lines 541-556)
- Verify mode state restored after operations
- Unit tests for mode stack discipline

## Open Questions

### Q1: Should we support mixed-format databases?

**Context**: Can a v7 database have a Cancoon record for backward compat?

**Answer**: No. v7 format explicitly removes Cancoon records. Backward compat achieved through auto-migration on open. Once saved as v7, no Cancoon.

**Decision**: Stick with clean v7 format (no Cancoon).

### Q2: What happens to UI state in migrated databases?

**Context**: Cancoon records store fonts, window positions, agents popup state.

**Answer**: UI state is dropped during migration. Headless builds don't use this data. Future: per-user preference store for UI state.

**Decision**: Document that UI state is not preserved in v7 migration. This is intentional for multi-user collaborative ODB.

### Q3: Should migration be automatic or require user confirmation?

**Context**: `odbOpenFile()` currently auto-migrates v6 databases.

**Answer**: Auto-migration is consistent with system root behavior. Creates backup, safe operation.

**Decision**: Keep auto-migration. Document clearly in `docs/GUEST_DATABASES.md`.

### Q4: How to test migration without modifying test databases?

**Context**: Running tests shouldn't migrate our reference v6 databases.

**Answer**: Copy v6 databases to temp directory before testing migration. Use `{FRONTIER_TEST_TMP_DIR}` template in integration tests.

**Decision**: Integration tests copy databases to temp before modification. Keep originals pristine.

### Q5: Should we remove Cancoon-related code after v7 migration?

**Context**: `loadversion2cancoonfile()`, `tyversion2cancoonrecord`, etc.

**Answer**: Keep for backward compat. v6 databases still exist in the wild (manila.root, prefs.root, etc.). Removing would break opening these files.

**Decision**: Keep v6 support code indefinitely. Mark as "legacy" in comments.

## Success Metrics

### Code Quality Metrics

- All unit tests pass (100% pass rate)
- Integration tests pass (100% pass rate)
- No compiler warnings introduced
- Logging standards compliance (no `fprintf(stderr)`)
- Code review approval (no major issues)

### Functional Metrics

- `db.new()` creates valid v7 databases (verified with xxd)
- `db.open()` opens v6 databases (auto-migration triggered)
- `db.open()` opens v7 databases (direct load, no migration)
- `db.save()` writes v7 format (no Cancoon record)
- Data preserved across v6→v7 migration (no data loss)

### Performance Metrics

- No performance regression vs current v6 opening
- Migration time < 5 seconds for typical guest database (6-8 MB)
- No memory leaks (valgrind clean, if applicable)

### Documentation Metrics

- `docs/GUEST_DATABASES.md` created and reviewed
- Inline comments added for format detection logic
- Integration test suite documented
- Migration behavior clearly explained

## Improvements & Future Work

### Beyond MVP Scope

These improvements are valuable but not required for initial v7 support:

1. **Cancoon Record Removal Tool**
   - Standalone utility to strip Cancoon records from v6 databases
   - Useful for bulk migration
   - Not needed for runtime functionality

2. **Format Validation Tool**
   - Verify database format correctness
   - Detect corrupted headers
   - Useful for debugging, not runtime

3. **Migration Progress Reporting**
   - Show progress for large database migrations
   - User-facing feature, not needed for headless

4. **UI State Preservation**
   - Per-user preference store for fonts/window positions
   - Future collaborative ODB feature
   - Requires design work

5. **Performance Optimization**
   - Lazy migration (migrate tables on access, not upfront)
   - Streaming migration for very large databases
   - Optimize only if performance issues arise

### Known Limitations

1. **No Cancoon record in v7**
   - UI state (fonts, window positions) dropped
   - Intentional for headless/collaborative use
   - Future: per-user preference store

2. **One-way migration**
   - v6→v7 is automatic
   - v7→v6 not supported (no downgrade path)
   - Document clearly: v7 is forward-only

3. **Format detection heuristics**
   - Cancoon detection uses versionnumber (2-3)
   - Could theoretically have false positives
   - Mitigated with validation, extremely unlikely

## References

### Code References

- **System Root Migration**: `Common/source/db_format.c:2123-2177` (`migrate_32bit_to_64bit()`)
- **v7 Reader**: `Common/source/db_reader_v7.c`
- **v7 Writer**: `Common/source/db_writer_v7.c`
- **Database Layer Open**: `Common/source/db.c:3082-3250` (`dbopenfile()`)
- **ODB Engine**: `Common/source/odbengine.c`
- **DB Verbs**: `Common/source/dbverbs.c`

### Documentation References

- **Database Architecture**: `docs/database_architecture.md`
- **Cancoon Record Format**: Lines 27-46 in database_architecture.md
- **v7 Format Details**: Lines 49-212 in database_architecture.md
- **Testing Guide**: `docs/TESTING_GUIDE.md`
- **CLI Usage**: `docs/CLI_USAGE_GUIDE.md`

### ADR References

- **ADR-005**: Parameter state thread-safety (pattern for mode stack discipline)
- Future ADR needed: "Guest Database v7 Migration Architecture"

### Issue References

- Related issues: TBD (check GitHub for db.new/db.open issues)
- Blocking issues: None (all dependencies implemented)

---

## Summary: Phase 1 Implementation Checklist

### File Extension Requirements (Phase 1)

- `.root` files = v6 format (original, preserved, NEVER modified)
- `.root7` files = v7 format (temporary extension during development)
- `db.new("mydb.root")` → creates `mydb.root7` (NOT `mydb.root`)
- `db.open("mydb.root")` → creates `mydb.root7`, opens migrated v7
- `db.save(odbref)` → writes to `.root7` file (NOT `.root`)

### Critical Invariants (Phase 1)

1. Original `.root` files are NEVER modified
2. Migration ALWAYS creates `.root7` output (appends "7" to input path)
3. `db.new()` ALWAYS creates `.root7` files (even if user provides `.root` path)
4. Both `.root` and `.root7` can coexist in same directory
5. Production UserTalk code using `.root` continues working (v6 format)

### Code Changes Required (Phase 1)

**db_format.c:**
- `migrate_32bit_to_64bit()`: Create `.root7` output (append "7" to input)
- Add `db_format_set_last_backup_path()` / `db_format_last_backup_path()`

**dbverbs.c:**
- `db.new()`: Add `odb_ensure_root7_extension()` call before file creation

**odbengine.c:**
- Add `odb_ensure_root7_extension()` helper function
- `odbOpenFile()`: Migration creates `.root7`, reopens with migrated path
- `odbNewFile()`: No changes (already creates v7 format)
- `odbSaveFile()`: No changes (already writes v7 format)

### Testing Requirements (Phase 1)

**Integration Tests:**
- `db.new("test.root")` creates `test.root7`, NOT `test.root`
- `db.open("test.root")` creates `test.root7`, preserves `test.root`
- `db.save(odbref)` writes to `.root7` file
- Original `.root` files never modified by any operation
- Both formats can coexist in same directory

**Test Databases:**
- v6: `tests/fixtures/v6/test.root` (never modified)
- v7: `tests/fixtures/v7/test.root7` (Phase 1 naming)

### Documentation Requirements (Phase 1)

**Create:**
- `docs/GUEST_DATABASES.md` - Full Phase 1/Phase 2 explanation

**Update:**
- Inline comments in `odbengine.c` - Phase 1 `.root7` behavior
- Integration test documentation - `.root7` paths
- Migration plan (this document) - Phase 1 focus

**Explain:**
- Why `.root7` extension? (safe coexistence during stabilization)
- How does Phase 2 work? (transition to `.root` + `.root.v6.bkp`)
- What triggers Phase 2? (30+ days stable, user approval)

### Phase 2 Transition (Future)

When stable and approved:
1. Update code to create `.root` files (v7 format)
2. Migration creates `.root.v6.bkp` backups
3. Run `tools/migrate_root7_to_root.sh` on production databases
4. Update all documentation for Phase 2 behavior

---

**Plan Version**: 2.0 (Updated for Two-Phase Migration Strategy)
**Last Updated**: 2026-01-07
**Status**: Ready for Implementation - Phase 1
