# Processor Audit: `db`

**Status:** ✅ **HEADLESS-COMPATIBLE** (13 Kernel Verbs + 2 Scripts)
**Audit Date:** 2025-12-06
**Auditor:** Claude (Haiku 4.5)

---

## Basic Information

| Property | Value |
|----------|-------|
| **Processor Name** | `db` |
| **EFP ID** | 1019 |
| **Verb Count** | 13 (kernel) + 2 (utility scripts) = 15 total |
| **Window Required** | NO ✅ |
| **Documentation** | [db/](../../../docs/usertalk/docserver.userland.com/db/index.html) |
| **Script Implementation** | `system.verbs.builtins.db` (Frontier.root) |

---

## Category Assessment

**Category:** ✅ **Core Database Management** (HEADLESS-COMPATIBLE)

**Rationale:**
Database processor provides operations for opening, closing, and manipulating Frontier databases. Operations include CRUD on tables, metadata queries, and navigation. Window requirement is explicitly `false` in kernelverbs.rc, making this fully headless-compatible.

**Headless Compatibility:** ✅ **FULL** (13/13 kernel verbs are headless-compatible)

---

## Verb Inventory

### Kernel Verbs (13 from kernelverbs.rc)

| # | Verb | Signature | Description |
|---|------|-----------|-------------|
| 1 | `new` | `db.new(path) -> dbRef` | Create new database file |
| 2 | `open` | `db.open(path, readOnly=false) -> dbRef` | Open existing database |
| 3 | `save` | `db.save(dbRef)` | Save database to disk |
| 4 | `close` | `db.close(dbRef)` | Close database connection |
| 5 | `defined` | `db.defined(dbRef) -> boolean` | Check if database is open/valid |
| 6 | `getValue` | `db.getValue(dbRef, path) -> value` | Get value from database |
| 7 | `setValue` | `db.setValue(dbRef, path, value)` | Set value in database |
| 8 | `delete` | `db.delete(dbRef, path)` | Delete item from database |
| 9 | `newTable` | `db.newTable(dbRef, path) -> tableRef` | Create new table in database |
| 10 | `isTable` | `db.isTable(dbRef, path) -> boolean` | Check if path points to table |
| 11 | `countItems` | `db.countItems(dbRef, path) -> integer` | Get item count in table/database |
| 12 | `getNthItem` | `db.getNthItem(dbRef, path, index) -> item` | Get Nth item by index |
| 13 | `getModDate` | `db.getModDate(dbRef, path) -> date` | Get last modification date |

### Utility Scripts (2 UserTalk implementations)

| # | Script | Type | Description |
|---|--------|------|-------------|
| 1 | `db.get` | UserTalk | Get value (alias or convenience wrapper) |
| 2 | `db.set` | UserTalk | Set value (alias or convenience wrapper) |

---

## Implementation Analysis

### Complexity: **LOW-MEDIUM** (Core Database Operations)

### Dependencies

- **Other Processors:** NONE (foundational)
- **OS-Specific:** NO (but uses file system for database files)
- **GUI/Window Context:** NO ✅

### Key Implementation Notes

**Architecture:**

Frontier databases are hierarchical data structures:
```
database.root.table.item = value
database.root.nested.table.scalar = "text"
```

Operations work on paths:
- `db.getValue(@db, "table.item")`
- `db.setValue(@db, "table.item", newValue)`
- `db.newTable(@db, "table.nested")`

**Core Concepts:**

1. **Database References** - In-memory or file-based database handles
2. **Paths** - Dot-separated paths to items (e.g., "system.verbs.builtins.webserver")
3. **CRUD Operations** - Create, read, update, delete on database items
4. **Table Navigation** - Iterate through database hierarchy
5. **Metadata** - Modification dates, item counts, type checks

**Database File Format:**
- Binary format (Frontier proprietary)
- File extension: typically .root
- In-memory caching (open database = loaded in memory)
- Atomic save operation (safe transaction model)

---

## Implementation Status

**Current State:**
- ✅ 13 kernel verbs defined in kernelverbs.rc
- ✅ 2 UserTalk utility scripts (get/set aliases)
- ⏳ Kernel verb implementations (database engine)
- ⏳ Database file format support (read/write)
- ⏳ Path parsing and hierarchical access

**What Needs Implementation:**

**Tier 1 - Essential:**
1. **db.new()** - Create new database file
2. **db.open()** - Open/load database file
3. **db.close()** - Close database connection
4. **db.save()** - Persist database to disk
5. **db.defined()** - Check if database is valid

**Tier 2 - Core Operations:**
1. **db.getValue()** - Read value from database
2. **db.setValue()** - Write value to database
3. **db.delete()** - Remove item from database
4. **db.newTable()** - Create table in database

**Tier 3 - Query/Navigation:**
1. **db.isTable()** - Type checking
2. **db.countItems()** - Size queries
3. **db.getNthItem()** - Index-based access
4. **db.getModDate()** - Metadata queries

**Implementation Pattern:**
```c
// Database reference management
typedef struct {
    char* dbPath;
    void* data;           // In-memory database structure
    boolean isModified;
    boolean readOnly;
    time_t lastModified;
} DatabaseHandle;

// Core operations
boolean dbnew(bigstring path) {
    // Create new database file
    // Initialize in-memory structure
    // Return success
}

boolean dbopen(bigstring path, boolean readOnly) {
    // Load database file into memory
    // Set up access permissions
    // Register database handle
}

boolean dbgetvalue(bigstring path, void* resultValue) {
    // Parse dot-separated path
    // Navigate hierarchical structure
    // Return value at path
}
```

---

## Testing Requirements

**Basic Operations:**
```usertalk
// Create and save
db.new (@myDb, "/tmp/test.root")
db.setValue (@myDb, "item", "value")
db.save (@myDb)

// Read back
local (val = db.getValue (@myDb, "item"))  // "value"

// Tables
db.newTable (@myDb, "config")
db.setValue (@myDb, "config.name", "test")

// Navigation
db.countItems (@myDb, "config")           // 1
db.getNthItem (@myDb, "config", 1)        // name
```

**Advanced Cases:**
- Nested table hierarchies (5+ levels deep)
- Large tables (1000+ items)
- Mixed types (scalars + tables + scripts)
- Read-only access restrictions
- Concurrent access (if supported)
- Database file persistence

---

## Priority & Sequencing

**Priority:** 🏆 **CRITICAL** (Tier 1 - Foundation)

**Recommended Implementation Order:** FIRST (before any other processors)

**Rationale:**
- All other processors depend on databases
- Tables processor requires db operations
- File processor uses databases
- WebServer uses databases for configuration

---

## Headless Compatibility Analysis

**Fully Compatible:** ✅ YES (13/13 verbs)

- Window required = false ✅
- No GUI dependencies
- Pure data operations
- Perfect for headless use

---

## References

**Kernel Definitions:**
- `Common/resources/Win32/kernelverbs.rc` (lines 910-929)
- ID 1019: db (13 verbs)

**Scripts:**
- `system.verbs.builtins.db/` (15 scripts)

---

## Summary

**Status:** ✅ **CRITICAL + HEADLESS-COMPATIBLE**

This processor is foundational and must be implemented first. All other processors depend on database operations. The 13 kernel verbs are straightforward database operations without GUI dependencies.

---
