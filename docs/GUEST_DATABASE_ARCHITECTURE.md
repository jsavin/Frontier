# Guest Database Architecture

## Overview

In Frontier, the **system root** (`Frontier.root`) is the primary database opened at startup. It contains the UserTalk runtime, system tables, and built-in scripts. **Guest databases** are any additional ODB databases opened alongside the system root for script access.

Guest databases allow UserTalk scripts to read and modify external ODB files without affecting the system root. They can be opened in two ways, each providing different levels of access.

## How Guest Databases Are Opened

### fileMenu.open(path, hidden=false)

Opens a guest database and makes it available through both programmatic and bracket syntax access:

1. Opens the ODB file on disk (`openfile` + `odbOpenFile`)
2. Adds to the `hodblist` linked list (for `db.*` verb access)
3. Mounts the root table into `system.compiler.files` (for bracket syntax access)

In GUI mode, this also opens a window; the `hidden` parameter controls window visibility. In headless mode, the `hidden` parameter is accepted but ignored (no windows exist).

After opening, scripts can access the database using bracket syntax:

```usertalk
fileMenu.open("/path/to/MyDB.root")
local (x = ["/path/to/MyDB.root"].tableName.entryName)
```

### db.open(path, readonly=false)

Opens a guest database for programmatic-only access:

1. Opens the ODB file on disk
2. Adds to `hodblist` (for `db.*` verb access)
3. Does **NOT** mount into `system.compiler.files`
4. Does **NOT** support bracket syntax `[filepath]` access

### Key Difference

| Verb | hodblist | system.compiler.files | Bracket Syntax | db.* Verbs |
|------|----------|-----------------------|----------------|------------|
| `db.open()` | Yes | No | No | Yes |
| `fileMenu.open()` | Yes | Yes | Yes | Yes |

Use `db.open()` for programmatic-only access. Use `fileMenu.open()` when scripts need bracket syntax access to the database contents.

## Runtime Data Structures

### hodblist -- Guest Database Linked List

Global linked list of open guest databases. Defined in `Common/source/dbverbs.c`.

- Uses the **sentinel pattern**: `hodblist` itself is a permanently allocated sentinel handle. Real entries start at `(**hodblist).hnext`.
- The sentinel prevents `hodblist` from becoming a dangling pointer when the last guest database is closed.

Each entry is a `tyodbrecord` (typedef of `tyodblistrecord`):

```c
typedef struct tyodblistrecord {
    struct tyodblistrecord **hnext;  // next entry in linked list
    tyfilespec fs;                   // file path on disk
    hdlfilenum fref;                 // OS file reference
    boolean flreadonly;              // whether opened read-only
    odbref odb;                      // ODB reference (hdlcancoonrecord)
} tyodbrecord, *ptrodbrecord, **hdlodbrecord;
```

### system.compiler.files -- Mount Table

A global hash table (`filewindowtable`, declared in `Common/headers/tablestructure.h`) that maps file paths to guest database root tables.

- Created at bootstrap time, never saved to disk
- Each entry maps a file path (bigstring key) to an external value wrapping the guest database's root variable handle
- This is the mechanism that enables `[filepath]` bracket syntax in UserTalk
- Only populated by `fileMenu.open()`, not by `db.open()`

```
filewindowtable:
  "/path/to/MyDB.root"  -->  external value (hrootvariable of guest cancoon)
  "/path/to/Other.root" -->  external value (hrootvariable of guest cancoon)
```

### odbref -- Database Reference

The `odbref` type is actually `hdlcancoonrecord` -- a handle to a cancoon record. The cancoon record (`tycancoonrecord`, defined in `Common/headers/cancoon.h`) contains the full state of an open database:

```c
typedef struct tycancoonrecord {
    hdldatabaserecord hdatabase;     // db.c's low-level database record
    hdlhashtable hroottable;         // root hash table of the database
    Handle hrootvariable;            // root variable handle (used for mounting)
    hdlmenubarlist hmenubarlist;     // menubar values (GUI mode)
    hdlprocesslist hprocesslist;     // background processes
    Handle hscriptstring;            // quickscript dialog content
    // ... window info, message areas, flags ...
    boolean flguestroot;             // true if mounted into a host root
    boolean fldirty;                 // true if database has been modified
    // ...
} tycancoonrecord, *ptrcancoonrecord, **hdlcancoonrecord;
```

The `hrootvariable` field is the key linkage point: it is the handle that gets wrapped as an external value and stored in `filewindowtable` to enable bracket syntax access.

### odb_context_guard -- Context Protection

When operating on guest databases, the system root's global state must be saved and restored. The `odb_context_guard` (defined in `Common/source/db_format.c`) provides this protection.

**`odb_guard_enter()`** saves:
- `currenthashtable` -- the currently active hash table
- `databasedata` -- the current database record
- `hashtablestack` -- the stack of pushed hash tables
- `rootvariable` -- the system root variable handle
- `roottable` -- the system root hash table

**`odb_guard_exit()`** restores all of the above.

This guard is essential because `odbOpenFile()` and `odbCloseFile()` modify these globals as a side effect. Without the guard, opening or closing a guest database would corrupt the system root's state, causing data loss or crashes.

Usage pattern:

```c
odb_context_guard guard;

odb_guard_enter(&guard);          // save system root globals

odbOpenFile(fref, &odb, false);   // this mutates globals as side effect

cancoonglobals = nil;             // clear to prevent stale references
odb_guard_exit(&guard);           // restore system root globals
```

## Closing Guest Databases

### fileMenu.close()

Operates on the **current target** (set via `target.set(@address)` in UserTalk). The target must point to an entry in `system.compiler.files` for the close to take effect.

Closing sequence:

1. Call `langgettarget()` to get the current target's hash table and name
2. Verify the target table is `filewindowtable` (meaning it is a guest database)
3. Look up the target name (file path) in `hodblist`
4. If found, call `filemenu_close_guestdb()` which:
   - Removes entry from `system.compiler.files` (via `hashdelete`)
   - Closes the ODB file (via `odbCloseFile` with context guard)
   - Closes the OS file (via `closefile`)
   - Unlinks from `hodblist` (via `listunlink`)
   - Disposes the handle (via `disposehandle`)

If no target is set, or the target is not a guest database, `fileMenu.close()` is a no-op and returns true.

### fileMenu.closeall()

Walks the entire `hodblist` linked list and closes each guest database. Handles iteration safely by saving `hnext` before disposing the current entry:

```c
hodb = (**hodblist).hnext;
while (hodb != nil) {
    hnext = (**hodb).hnext;        // save next before dispose
    filemenu_close_guestdb(hodb);  // closes, unlinks, and disposes
    hodb = hnext;                  // advance using saved pointer
}
```

If closing one database fails, it logs the error and continues closing the remaining databases.

### db.close(f)

Closes a guest database by file path. Looks up the database in `hodblist` by filespec comparison. Does **NOT** remove from `system.compiler.files` because `db.open()` never mounted the database there in the first place.

## The Target System

The target system provides a way to specify which database or object a verb should operate on.

- **Storage**: Per-thread `_target_` variable stored in the outer local table
- **Set**: `target.set(@address)` -- stores an address value pointing to the target
- **Read**: `langgettarget(&htable, bsname)` -- returns the hash table containing the target and the target's name within that table
- **Clear**: `target.clear()` or automatic when the local scope exits

In `langgettarget()` (defined in `Common/source/langverbs.c`), the implementation:

1. Pushes the outer local table
2. Looks up the `_target_` symbol
3. Verifies it is an address value type
4. Resolves the address to a table and name pair

For `fileMenu.close()`, the target is expected to be an address into `system.compiler.files`. For example:

```usertalk
fileMenu.open("/path/to/MyDB.root")
target.set(@["/path/to/MyDB.root"])
fileMenu.close()  «-- closes MyDB.root
```

In GUI mode, the target typically represents the front window. In headless mode, there is no front window concept, so `target.set()` must be called explicitly.

## Headless Mode Differences

Headless mode (enabled via `FRONTIER_HEADLESS` compile flag) removes all GUI dependencies. This affects guest database handling in several ways:

- **No windows**: The `hidden` parameter in `fileMenu.open()` is accepted but ignored
- **No front window**: `fileMenu.close()` relies entirely on `target.set()`, not window focus. Without an explicit target, close is a no-op
- **Direct save path**: `fileMenu.save()` saves via `tablesavesystemtable` / `odbSaveFile` directly, bypassing window save logic
- **Menu verbs are no-ops**: `menu.addSubMenu`, `menu.buildMenuBar`, and similar verbs return true without performing any action

### Headless fileMenu.save() Behavior

`fileMenu.save()` has two modes in headless:

- **No parameters**: Saves the system root database (`filemenu_save_systemroot`)
- **With file path parameter**: Saves a specific guest database (`filemenu_save_guestdb`)

The system root save path uses `tablesavesystemtable()` to serialize the root table, flushes the release stack, updates `views[0]`, and calls `dbclose()` to flush to disk. The guest database save path searches `hodblist` by filespec and calls `odbSaveFile()`.

## Implementation Files

| File | Contains |
|------|----------|
| `tests/headless_filemenu_verbs.c` | fileMenu verb implementations for headless mode |
| `Common/source/menuverbs_headless.c` | Menu verb dispatcher for headless mode |
| `Common/source/dbverbs.c` | `hodblist`, `dbopenverb`, `dbclosefile`, lowercase ODB wrappers |
| `Common/source/langexternal.c` | `langexternalregisterwindow` (GUI mount pattern) |
| `Common/source/langverbs.c` | `langgettarget` implementation |
| `Common/headers/cancoon.h` | `tycancoonrecord` structure definition |
| `Common/headers/tablestructure.h` | `filewindowtable` declaration |
| `Common/source/db_format.c` | `odb_guard_enter` / `odb_guard_exit` context guard |
