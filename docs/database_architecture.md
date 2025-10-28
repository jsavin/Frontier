# Frontier Database Architecture
<!-- 2025-10-27 Codex: Documented v6→v7 migration constraints and 32-bit payload carry-over. -->

## Database Structure

### Physical File Layout

Each `.root` file is a database. The file structure is:

```
database-file.root (physical file)
  └─ [root table] (minimal, typically 1-2 entries)
      ├─ tablename1 (user-visible top-level table)
      ├─ tablename2 (user-visible top-level table)
      └─ ...
```

### Key Concept: The Root Table is Internal

The **root table** (what we scan at the database header's view[0] address) is an **internal entry point**, not the user-visible namespace.

Users interact with the **child tables** of the root table, which we call "top-level tables" in UserTalk.

### Legacy warning tables (v6 compatibility shim)

All pristine v6 databases we ship (`Frontier-v6.root`, `prefs.root`, `manila.root`, `test.root`) place a compatibility payload at `views[0]`. Characteristics:

- fixed 442-byte payload
- it is **not** a table; it is the serialized `tyversion2cancoonrecord` struct that drives the Frontier “About/Agents” window (also known as the Cancoon window)
- the first 2 bytes are the struct’s version (0x0003 in Frontier 6); the next 4 bytes (`adrroottable`) point at the true root table (e.g. 0x0000031e inside `tests/test.root`)
- the remainder of the struct stores font/window metadata for the About window plus the text buffer used by `msg()`/agents

Classic Frontier builds (pre-v6) load this table to display a “created with a newer version” warning rather than crash. Modern builds skip it and register the true top-level tables by following external references into the **modern** merged blocks elsewhere in the file (e.g. block `0x031e` in `databases/test.root`).

When writing scanners or migration tools:

1. Check whether the block at `views[0]` is the 442-byte Cancoon record.
2. If so, parse the first 6 bytes to extract `version` and `adrroottable`.
3. Continue scanning at `adrroottable`, which is a normal table stored using the merged (modern) format.

See `databases/test-root-contents.png` for the intended UI view of `test.root` once the Cancoon record is resolved and real tables like `myTable` are traversed.

## UserTalk Addressing

### Full Address Notation

To reference a table in a database file:

```usertalk
@["DISK-PATH-TO-ROOT-FILE.root"].tablename
```

Example: (macOS version)
```usertalk
@["Macintosh HD:Users:jake:dev:jsavin:Frontier:databases:Guest Databases:www:prefs.root"].prefs
```

This syntax:
- `@[...]` creates a database reference
- `.tablename` accesses a child of the database's root table

*Note*: Our North Star vision for the runtime is to migrate to POSIX paths with relative paths where possible. This is not yet designed or implemented.

### Short Form (In-Scope Access)

When databases are opened, their top-level tables are automatically in scope:

```usertalk
prefs.foo = 1
```

Instead of the full: (macOS version)
```usertalk
@["Macintosh HD:Users:jake:dev:jsavin:Frontier:databases:Guest Databases:www:prefs.root"].prefs.foo = 1
```

## Runtime Database Registry

### system.temp.databases

The runtime maintains a registry of opened databases at `system.temp.databases`:

- **Keys**: Full on-disk paths to each opened database file (excluding Frontier.root/system root)
- **Values**: Database references like `@["DISK-PATH-TO-ROOT-FILE.root"]`

Example entries:
```
system.temp.databases["/path/to/prefs.root"] = @["/path/to/prefs.root"]
system.temp.databases["/path/to/manila.root"] = @["/path/to/manila.root"]
```

**Note**: The system root (Frontier.root) is NOT listed in `system.temp.databases`.

### Path Reliance

Currently, the system relies on **full on-disk paths** for database identification. This is a known limitation to be improved/abstracted in the future.

## Examples

### Example 1: prefs.root

File: `/path/to/prefs.root`

Physical structure:
```
prefs.root (6MB file)
  └─ [root table] (442 bytes, 1-2 entries)
      └─ prefs (external table value, contains actual preferences data)
```

UserTalk access:
```usertalk
prefs.windowPosition = {100, 100}           // Short form
@["/path/to/prefs.root"].prefs.theme = "dark"  // Full form
```

### Example 2: manila.root

File: `/path/to/manila.root`

Physical structure:
```
manila.root (7.9MB file)
  └─ [root table] (442 bytes, minimal entries)
      ├─ manila (main application table)
      ├─ stories (content table)
      └─ ... (other top-level tables)
```

UserTalk access:
```usertalk
manila.version                              // Short form
@["/path/to/manila.root"].manila.version      // Full form
```

## Implementation Implications

### For Database Scanner

When scanning databases:
1. The root table entries (1-2 items) are the **top-level tables** users work with
2. To see actual content, follow the external table references (type=13)
3. Each external table value contains the nested structure users interact with

### For Migration Code

When migrating v6→v7:
1. The minimal root table structure is **correct and expected**
2. Migration must preserve the external table references
3. Legacy payloads store 32-bit `dbaddress` values (and 10-byte `tydisksymbolrecord` entries). Simply copying those bytes forward leaves the migrated root in a “v7 header + v6 body” state that still depends on 32-bit readers.
4. A true v7 database must be re-serialized with `use_64bit_format == true` so that every external record and table payload widens to 64-bit addresses. This requires loading each table with the legacy reader, flipping the format flag, and saving back through `tableverbpack()`/`hashpacktable()` before writing the new file.

#### Migration Pitfalls (32-bit payloads)

- The sample migrator in `Common/source/db_format.c:874-1054` only writes a new 116-byte header and then copies the remainder of the legacy file byte-for-byte into the v7 output. All block payloads—including `system`, `system.verbs`, and `system.verbs.builtins`—remain 32-bit serialized.
- Runtime code such as `tableverbunpack()` decides how many bytes to consume based on the global `use_64bit_format` flag (set once the v7 header is seen). When a copied legacy payload arrives, the loader tries to read an 8-byte address, overruns the 4-byte legacy field, and errors out unless additional heuristics patch things up.
- To avoid accruing more compatibility shims, the migrator must take ownership of widening those payloads. The recommended approach is:
  1. Load each legacy table/verb through the existing 32-bit readers.
  2. Set `use_64bit_format = true` before packing.
  3. Re-pack with `tableverbpack()` or `hashpacktable()` so all nested addresses are emitted as 64-bit values.
  4. Write the resulting blocks into the new file, ensuring block headers/trailers match their new sizes.

Once this reserialization is in place, no runtime path outside the migrator should need to special-case 32-bit layouts.

### For CLI/Runtime

When loading databases:
1. Open the database file
2. Load the root table (minimal)
3. Register top-level table entries in the namespace
4. Load external tables on-demand as accessed
5. Register database in `system.temp.databases` (except system root)

### External Table Value Record (v6 Disk Format)

When a v6 table stores a child table (value type 13) the string pool slot referenced by `dataval` encodes the external record:

```
[u32 length][u16 externaldiskversionnumber][u8 tyexternalid][u8 flags][u32 dbaddress]
```

- `length` is written by `hashpackexternal()` and covers the remaining payload bytes.
- `externaldiskversionnumber` is currently `1`.
- `tyexternalid` identifies the processor (`idtableprocessor` = `3` for table values).
- `flags` is the on-disk copy of the `tyexternalvariable` bitfield (`flinmemory`, `flpacked`, `flsystemtable`, etc.). Frontier does not clear these bits before persisting the record, so whatever combination was active during save is recorded verbatim.
- `dbaddress` is the big-endian address of the child table block that `tableverbpack()` appended after the header.

At load time `langexternalunpack()` consumes the version/id pair, ignores the flag byte, and passes the address to `tableverbunpack()` which rehydrates the child table.

## Scanner Findings Re-Interpreted

Original scanner results now make perfect sense:

### Frontier.root
```
Root table: 1 entry
  - key="" (empty string - possibly system marker)
  - type=ostype
  - data=0x20000006
```

This single entry likely represents the system table tree root.

### prefs.root / manila.root
```
Root table: 1 entry
  - key="..." (table name, possibly encoded)
  - type=noval or external
```

This entry is the reference to the main application table.

## Future Improvements

### Path Abstraction (Future Work)

Currently using full disk paths for database identity. Future improvements could:
- Use database IDs or symbolic names
- Support relative paths
- Abstract file system dependencies
- Enable database portability

**Status**: Documented for future work, not current scope.

## References

- Legacy format documented in: `Common/source/tableexternal_common.c` (commit fff861f)
- Original working code: commit d37f634 (Oct 2004 - Frontier 10.0a1 Open Source release)
- Scanner implementation: `scripts/scan_database_types.py`
- Migration implementation: `Common/source/db_format.c`

---

*Generated during scanner debugging session, Oct 23 2025*
