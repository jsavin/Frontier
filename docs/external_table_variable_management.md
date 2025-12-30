# External Table Variable Management in Frontier

**Date**: 2025-12-18
**Status**: Comprehensive Reference Documentation
**Related Issues**: #123 (external table address migration bug)

---

## Overview

This document explains how Frontier manages external table variables, the relationship between memory addresses and database addresses, and how database migration works across format versions.

**Key concept**: External table variables can exist in two states - in-memory (fast access) or on-disk (lazy-loaded). Understanding the transitions between these states is critical for database operations.

---

## 1. External Table Variable Structure

Defined in `Common/headers/langexternal.h`:

```c
typedef struct tyexternalvariable {
    unsigned short id;              /* tyexternalid (e.g., idtableprocessor = 3) */
    unsigned short flinmemory: 1;   /* CRITICAL: in-memory vs on-disk state */
    unsigned short flmayaffectdisplay: 1;
    unsigned short flpacked: 1;
    unsigned short flscript: 1;
    unsigned short flsystemtable: 1;
    long variabledata;              /* Context-dependent: memory handle OR dbaddress */
    hdldatabaserecord hdatabase;    /* Database this variable belongs to */
    dbaddress oldaddress;           /* Last known database address */
} tyexternalvariable;
```

### Key Fields

**`flinmemory` (1 bit flag)**
- `1`: Table is loaded in RAM (fast access, uses memory)
- `0`: Table is on-disk (lazy-loaded, saves memory)

**`variabledata` (context-dependent pointer)**
- When `flinmemory=1`: Points to `hdlhashtable` in RAM (memory handle)
- When `flinmemory=0`: Contains `dbaddress` (disk location)
- **Critical**: Same field, different interpretation based on `flinmemory`

**`oldaddress` (database address)**
- Last known database address where table was stored
- Used for incremental saves (update existing block vs allocate new)
- `nildbaddress` (0) means "never saved" or "force new allocation"

**`hdatabase` (database handle)**
- Which database this table belongs to
- Critical for multi-database operations (migration, save-as)

---

## 2. Two States of External Table Variables

### State A: In-Memory (flinmemory=1)

```
flinmemory:    1
variabledata:  0x600001234000 (memory handle pointing to hdlhashtable)
oldaddress:    0x00A45678 (last saved location, or nildbaddress if new)
hdatabase:     (ptr to database record)
```

**Characteristics:**
- Table content is in RAM, directly accessible
- Fast access - no disk I/O required
- Uses memory proportional to table size
- Modified tables stay in-memory until saved

**Use cases:**
- Tables being actively edited
- Newly created tables
- Frequently accessed system tables
- During database migration (all tables forced in-memory)

### State B: On-Disk (flinmemory=0)

```
flinmemory:    0
variabledata:  0x00A45678 (database address pointing to disk location)
oldaddress:    0x00A45678 (usually same as variabledata)
hdatabase:     (ptr to database record)
```

**Characteristics:**
- Table content on disk, not in RAM
- Requires `dbrefhandle()` to load before access
- Saves memory (only metadata in RAM)
- Lazy loading - loads on first access

**Use cases:**
- Large tables not currently being edited
- Memory conservation for large databases
- Legacy databases with many tables

---

## 3. Lifecycle: Saving Tables (Packing)

### Function: `tableverbpack()`
**Location**: `Common/source/tablepack.c` lines 287-451

#### Case 1: In-Memory Table (flinmemory=1)

```
INPUT:
  flinmemory = 1
  variabledata = (hdlhashtable) 0x600001234000
  oldaddress = 0x00A45678 (or nildbaddress)

PROCESS:
1. tablepacktable(ht, false, &hpackedtable)
   → Serialize hashtable to binary format

2. dbsavehandle(hpackedtable, &adr)
   → Write to database, get new address
   → Logic: if (fldatabasesaveas || adr == nildbaddress)
             dballocate(...)    // NEW block
           else
             dbassign(adr, ...) // UPDATE existing block

3. Append address trailer to packed data (BE32 or BE64)

4. Update metadata:
   oldaddress = adr           // remember new location
   ht->fldirty = false        // table is clean

OUTPUT:
  flinmemory = 1 (unchanged - still in memory)
  variabledata = 0x600001234000 (unchanged)
  oldaddress = (new database address from dbsavehandle)
```

#### Case 2: On-Disk Table (flinmemory=0)

**Normal Path** (non-migration):
```
INPUT:
  flinmemory = 0
  variabledata = 0x00A45678 (dbaddress)

PROCESS:
  Just push the address as-is:
  adr = (dbaddress) variabledata;
  goto pushaddress;  // write address to packed data

OUTPUT:
  flinmemory = 0 (unchanged)
  variabledata = 0x00A45678 (unchanged)
  oldaddress = 0x00A45678 (unchanged)
```

**Migration Path** (`adapter_repack=true` or `fldatabasesaveas=true`):
```
INPUT:
  flinmemory = 0
  variabledata = 0x00A45678 (v6 dbaddress)

PROCESS:
1. tableverbinmemory(hv, HNoNode)
   → Load from SOURCE database
   → Sets flinmemory = 1
   → Sets variabledata = (memory handle)

2. tablepacktable() + dbsavehandle()
   → Serialize and save to DESTINATION database
   → Get NEW v7 address

3. oldaddress = (new v7 address)

OUTPUT:
  flinmemory = 1 (CHANGED)
  variabledata = (memory handle)
  oldaddress = (new v7 address)
```

---

## 4. Lifecycle: Loading Tables (Unpacking)

### Function: `tableverbunpack()`
**Location**: `Common/source/tablepack.c` lines 454-502

```
INPUT:
  hpacked = Handle containing serialized table + address trailer

PROCESS:
1. Read address from trailer (last 4 or 8 bytes):
   rawadr = db_format_read_be32(trailer)  // v6
   OR
   rawadr = db_format_read_be64(trailer)  // v7

2. Create lazy external variable:
   newtablevariable(false, rawadr, &hvariable, flxml)

   Internally calls:
   langnewexternalvariable(false, rawadr, ...)
   {
       item.flinmemory = false;        // NOT loaded yet
       item.variabledata = rawadr;     // just store the address
       item.hdatabase = databasedata;  // current database
       item.oldaddress = nildbaddress; // not materialized
   }

OUTPUT:
  flinmemory = 0 (lazy reference created)
  variabledata = (database address from trailer)
  oldaddress = nildbaddress
  hdatabase = (current database being loaded)
```

**Critical insight**: Unpacking creates a **lazy reference**. Table content is NOT loaded - only the address is stored. Actual table loading happens on-demand.

---

## 5. Lazy Loading: Materialization on Access

### Function: `tableverbinmemory()`
**Location**: `Common/source/tableexternal_common.c` lines 298-505

**Trigger**: Code needs table content, but `flinmemory=0`

```
INPUT:
  flinmemory = 0
  variabledata = 0x00A45678 (dbaddress)
  hdatabase = (source database handle)

PROCESS:
1. dbpushdatabase(hdatabase)
   → Switch context to correct database

2. adr = (dbaddress) variabledata;

3. dbrefhandle(adr, &hpacked)
   → Read serialized table from disk
   → Handles legacy format conversion if needed

4. tableunpacktable(hpacked, false, &htable)
   → Deserialize into in-memory hashtable

5. Update variable state:
   flinmemory = true              // NOW in memory
   variabledata = (long) htable   // memory handle
   oldaddress = adr               // remember source

6. dbpopdatabase()

OUTPUT:
  flinmemory = 1 (CHANGED from 0→1)
  variabledata = (hdlhashtable memory handle)
  oldaddress = 0x00A45678 (address we loaded from)
```

---

## 6. Save-As Pattern: fldatabasesaveas

**Global flag**: `fldatabasesaveas` in `Common/source/db.c` line 197

**Purpose**: Indicates "Save As" operation (saving to different database)

### Behavior in tableverbpack()

```c
if (fldatabasesaveas) {
    fltempload = !(**hv).flinmemory;

    if (!tableverbinmemory(hv, HNoNode))
        return (false);

    *flnewdbaddress = true;  // FORCE new address allocation
}
```

### Effect on dbassign_internal()

```c
if (fldatabasesaveas || (adr == nildbaddress)) {
    dballocate(newsize, pdata, padr);  // Allocate NEW block
} else {
    dbassign(adr, newsize, *h);        // Update existing block
}
```

**Key insight**: Save-As ALWAYS allocates new blocks. This is critical for migration because it ensures tables get new v7 addresses instead of reusing v6 addresses.

---

## 7. Database Address Formats Across Versions

### V6 (Legacy)

- **Size**: 32-bit addresses
- **Byte order**: Little-endian (native)
- **Example**: `0x0062b8d9`
- **File size**: Typically 1.5-6MB
- **Functions**: `db_format_read_be32()`, `db_format_write_be32()`

### V7 (Modern)

- **Size**: 64-bit addresses
- **Byte order**: Big-endian (portable)
- **Example**: `0x000000000062b8d9`
- **File size**: Varies (supports larger databases)
- **Functions**: `db_format_read_be64()`, `db_format_write_be64()`

### Critical Incompatibility

**Problem**: v6 address `0x0062b8d9` in v6 database ≠ v7 address `0x0062b8d9` in v7 database

- Different block allocation strategies
- Different header sizes (v6: 84 bytes, v7: 90 bytes)
- Different address space organization
- **Result**: v6 addresses are INVALID in v7 files

---

## 8. Migration Problem (Issue #123)

### The Bug

During v6→v7 migration, external tables with `flinmemory=0` caused access failures:

```
SCENARIO:
1. V6 database has: system.verbs.globals
   - flinmemory=0
   - variabledata=0x62b8d9 (v6 address)

2. Migration process:
   - Loads v6 database
   - Creates v7 destination
   - Packs external variables with their addresses
   - Writes v7 file

3. Result in v7 file:
   - system.verbs.globals stored with variabledata=0x62b8d9
   - But 0x62b8d9 doesn't point to valid v7 block!

4. Later access attempt:
   - tableverbinmemory() calls dbrefhandle(0x62b8d9)
   - dbnormalizeaddress(0x62b8d9) FAILS
   - Error: "dbnormalizeaddress failed for adr=0x62b8d9"
```

### Root Cause

- V6 and V7 have different block allocators and address spaces
- Simply copying v6 addresses into v7 doesn't make them valid
- Migration wasn't forcing tables into memory before save

### The Fix (Commit 62850013)

**Function**: `db_format_force_materialize_external_tables()`
**Location**: `Common/source/db_format.c` lines 1286-1359

```c
static boolean db_format_force_materialize_external_tables_recursive(
    hdlhashtable htable,
    const db_context *context,
    int depth
) {
    // Recursively walk all hash nodes
    while (hashgetnthnode(htable, ix++, &hnode)) {
        // Find external table variables
        if (val->valuetype == externalvaluetype) {
            hdlexternalvariable hv = (hdlexternalvariable) val->data.externalvalue;

            // Load from v6 if not already in memory
            if (!(**hv).flinmemory) {
                tableverbinmemory(hv, hnode);  // Loads from SOURCE db
            }

            // Force new allocation in v7
            (**hv).oldaddress = nildbaddress;

            // Recurse into child tables
            hdlhashtable child = (hdlhashtable) (**hv).variabledata;
            db_format_force_materialize_external_tables_recursive(child, context, depth + 1);
        }
    }
}
```

**Called from**: `migrate_internal()` after `langhash_materialize_disk_values()`

**Effect**:
1. All external tables loaded into memory (flinmemory=0 → flinmemory=1)
2. All oldaddress cleared (forces new v7 allocations)
3. When saved to v7, tables get fresh v7 addresses
4. No stale v6 addresses remain in v7 database

---

## 9. Key Functions Reference

| Function | File | Purpose | State Change |
|----------|------|---------|--------------|
| `newtablevariable()` | tableops.c:255 | Create new external variable | Initializes all fields |
| `tableverbinmemory()` | tableexternal_common.c:298 | Load table from disk | flinmemory: 0→1 |
| `tableverbpack()` | tablepack.c:287 | Save table to disk | Updates oldaddress |
| `tableverbunpack()` | tablepack.c:454 | Create lazy reference | Creates flinmemory=0 variable |
| `dbassignhandle()` | db.c:2468 | Write data to database | Allocates/updates blocks |
| `dbrefhandle()` | db.c | Read data from database | Loads from disk |
| `hashpackexternal()` | langhash.c:2785 | Serialize external | Calls langexternalpack() |
| `hashunpackexternal()` | langhash.c:2826 | Deserialize external | Calls langexternalunpack() |

---

## 10. Critical Invariants

1. **Binary state**: External table is EITHER in-memory OR on-disk, never both
2. **Context-dependent pointer**: `variabledata` interpretation depends on `flinmemory`
3. **Lazy loading transparency**: On-disk tables auto-load on access (expensive but transparent)
4. **Address ownership**: `oldaddress` is NOT a backup - it's the canonical saved location
5. **Save-As forces new**: `fldatabasesaveas=1` always allocates new blocks
6. **Migration requires materialization**: v6 addresses don't map to v7 - must reload all tables
7. **Memory tradeoff**: Migration trades disk (lazy) for memory (all loaded) - acceptable for typical 1.5-6MB databases

---

## 11. Common Pitfalls

### Pitfall 1: Forgetting to check flinmemory

```c
// WRONG:
hdlhashtable ht = (hdlhashtable) (**hv).variabledata;
// This crashes if flinmemory=0!

// CORRECT:
if (!(**hv).flinmemory) {
    if (!tableverbinmemory(hv, hnode))
        return false;
}
hdlhashtable ht = (hdlhashtable) (**hv).variabledata;
```

### Pitfall 2: Copying addresses between databases

```c
// WRONG (during migration):
newvar.variabledata = oldvar.variabledata;
// v6 address doesn't work in v7!

// CORRECT:
if (!tableverbinmemory(oldvar, hnode))
    return false;
newvar.oldaddress = nildbaddress;  // Force new allocation
```

### Pitfall 3: Not setting oldaddress=nildbaddress

```c
// If oldaddress != nildbaddress, dbassign will try to UPDATE that address
// But in a new database, that address doesn't exist!

// Always clear when moving to new database:
(**hv).oldaddress = nildbaddress;
```

---

## 12. Related Documentation

- `planning/phase3/ISSUE_123_SOLUTION_DESIGN.md` - Migration fix design
- `planning/phase3/MIGRATION_VALIDATION_REPORT.md` - Test results
- `planning/architectural_decision_records/ADR-001-multi-database-context.md` - Multi-database patterns
- `docs/database_architecture.md` - Overall v7 database format

---

## 13. Implementation Files

**Primary**:
- `Common/headers/langexternal.h` - Data structures
- `Common/source/tablepack.c` - Pack/unpack logic
- `Common/source/tableexternal_common.c` - Lazy loading (tableverbinmemory)
- `Common/source/db.c` - Database block allocation/assignment
- `Common/source/db_format.c` - Migration and format conversion

**Secondary**:
- `Common/source/langhash.c` - Hash table serialization
- `Common/source/legacy/tablepack_legacy.c` - v6 format handling
- `Common/source/tableops.c` - Table operations

---

## 14. Transient vs Persistable Tables (2025-12-29)

### Overview

Not all in-memory tables should be saved to disk. Frontier distinguishes between:
- **Transient tables**: Local variables, thread stacks, temporary scopes (never persist)
- **Persistable tables**: User-created tables that can be saved to database

This distinction is tracked at the **hash table level**, not the external variable level.

### Hash Table Flags

From `Common/headers/lang.h:444-476`:

```c
typedef struct tyhashtable {
    hdlhashnode hashbucket [ctbuckets];
    hdlhashnode hfirstsort;
    ...

    boolean fldirty: 1;                  /* has anything changed? */
    boolean fllocked: 1;                 /* are changes allowed? */
    boolean fllocaltable: 1;             /* ← LOCAL VARIABLE TABLE (transient) */
    boolean flchained: 1;                /* in the chain of local hashtables? */
    boolean fldisposewhenunchained: 1;   /* dispose when removed from stack? */
    boolean ctwithvalues: 3;             /* how many with statement values? */
    ...
}
```

### Semantics of `fllocaltable`

**`fllocaltable=1` → Transient table (NEVER persist)**
- Function parameter tables
- Local variable scopes
- Thread stack frames
- With-statement context tables
- Automatically disposed when scope exits

**`fllocaltable=0` → Persistable table (MAY persist)**
- User-created tables (`lang.new(tableType, @t)`)
- Database tables (`system.verbs`, user workspace)
- Can be saved to disk
- Lives beyond function/thread lifetime

### Where `fllocaltable` is Set

```c
// Script execution locals (scripts.c:2711)
(**ht).fllocaltable = true;

// Function parameter/local tables (lang.c:554)
(**ht).fllocaltable = true;

// With-statement context tables (langevaluate.c:799)
(**ht).fllocaltable = true;
```

### State Matrix: External Variables for Tables

| Table Type | flinmemory | oldaddress | hdatabase | fllocaltable | Meaning |
|------------|------------|------------|-----------|--------------|---------|
| **New local var** | 1 | nildbaddress | nil | 1 | Function local (transient) |
| **New user table** | 1 | nildbaddress | nil | 0 | Created by `lang.new()` (not yet saved) |
| **Loaded from disk** | 1 | non-nil | non-nil | 0 | Loaded into memory, can be saved back |
| **Lazy reference** | 0 | nildbaddress | non-nil | 0 | On-disk, not yet loaded |

### Critical Invariant for `hdatabase`

**RULE**: `hdatabase` should ONLY be set when an object comes from disk.

```c
// VALID STATES:
hdatabase=nil, oldaddress=nildbaddress        // New object (never saved)
hdatabase!=nil, oldaddress!=nildbaddress      // Loaded from disk

// INVALID STATE (causes crashes):
hdatabase!=nil, oldaddress=nildbaddress       // Ambiguous! Was it loaded or new?
```

**Why this matters**:
- `hdatabase!=nil` signals "this object belongs to a database"
- Runtime uses this to determine if object should be loaded from disk
- Setting `hdatabase` on new objects creates ambiguity: "Should I load this from disk?"
- Result: Runtime tries to dereference garbage `variabledata` as database address → segfault

### Provenance vs Current State

**`flinmemory` tracks CURRENT STATE** (where data is right now):
- `flinmemory=1` → "Data is currently in memory"
- `flinmemory=0` → "Data is currently on disk"

**`oldaddress` tracks HISTORY** (has it ever been saved?):
- `oldaddress=nildbaddress` → Never been saved to disk
- `oldaddress!=nildbaddress` → Has been saved (and this is where)

**`hdatabase` tracks PROVENANCE** (where did it come from?):
- `hdatabase=nil` → Created new (in-memory only, or not yet associated with database)
- `hdatabase!=nil` → Came from this database (loaded from disk)

**`fllocaltable` tracks LIFETIME** (should it ever persist?):
- `fllocaltable=1` → Transient (disposed when scope exits, never saved)
- `fllocaltable=0` → Persistable (may be saved to database)

### Example Lifecycles

**Local Variable Table (Transient)**:
```c
// Function call creates local scope
local (x, y, z);
lang.new(tableType, @localVars);  // Internal, for locals

// State:
flinmemory=1                  // In memory
oldaddress=nildbaddress       // Never saved
hdatabase=nil                 // Not from database
fllocaltable=1                // ← TRANSIENT

// Function returns → table is DISPOSED (never saved)
```

**User Table (Persistable)**:
```c
// User creates table with system root loaded
lang.new(tableType, @myTable);

// State immediately after creation:
flinmemory=1                  // In memory
oldaddress=nildbaddress       // Not yet saved
hdatabase=nil                 // ← NOT SET (the fix!)
fllocaltable=0                // Persistable

// Later, user saves database:
tableverbpack(hv, ...)
  → Sets oldaddress=0x1234
  → Sets hdatabase=systemRootDB
  → Table now has provenance

// State after save:
flinmemory=1                  // Still in memory
oldaddress=0x1234             // Last saved location
hdatabase=systemRootDB        // Now affiliated with database
fllocaltable=0                // Persistable
```

**Loaded Table**:
```c
// Database loads system.verbs (lazy)
tableverbunpack(...)

// Initial state (lazy reference):
flinmemory=0                  // Not loaded yet
oldaddress=nildbaddress       // Not materialized
hdatabase=systemRootDB        // Came from this database
fllocaltable=0                // Persistable

// First access triggers load:
tableverbinmemory(hv, hnode)

// State after load:
flinmemory=1                  // Now in memory
oldaddress=0x1234             // Loaded from here
hdatabase=systemRootDB        // Still affiliated
fllocaltable=0                // Persistable
```

### The Bug Fixed in This Commit

**Problem**: `langexternalsetdatabase()` was setting `hdatabase` for newly created objects:

```c
// BUGGY CODE (causes crashes):
void langexternalsetdatabase(hdlexternalvariable hv, hdldatabaserecord hdb) {
    if ((**hv).flinmemory && (**hv).oldaddress == nildbaddress)
        (**hv).hdatabase = hdb;  // ← WRONG! Creates invalid state
}

// Result when system root is loaded:
lang.new(tableType, @t)
  → flinmemory=1, oldaddress=nildbaddress, hdatabase=systemRootDB
  → Invalid state! Runtime thinks it should load from disk
  → Tries to dereference variabledata as dbaddress
  → Segfault
```

**Fix**: Never set `hdatabase` for new objects. Only set when loading from disk:

```c
void langexternalsetdatabase(hdlexternalvariable hv, hdldatabaserecord hdb) {
    // REMOVED: Do not set hdatabase for new objects
    // hdatabase is ONLY set when object is loaded from disk
    // (happens in tableverbinmemory after successful load)

    log_debug(LOG_COMP_EXTERNAL,
        "langexternalsetdatabase: BLOCKED - hdatabase only set when loaded from disk");
}
```

### Related Files

**Hash table structure**:
- `Common/headers/lang.h:444-476` - `tyhashtable` definition with `fllocaltable`

**Local table creation**:
- `Common/source/scripts.c:2711` - Script locals
- `Common/source/lang.c:554` - Function parameters
- `Common/source/langevaluate.c:799` - With-statement contexts

**External variable management**:
- `Common/source/langexternal.c:2739` - `external_set_ondisk()`
- `Common/source/langexternal.c:2784` - `external_set_inmemory()`
- `Common/source/langexternal.c:289` - `langexternalsetdatabase()` (the fix)

---

**Document Status**: Comprehensive reference based on code analysis (2025-12-18)
**Updated**: 2025-12-29 - Added transient vs persistable semantics and `hdatabase` invariants
**Reviewed**: Pending
**Updates**: Will be maintained as implementation evolves
