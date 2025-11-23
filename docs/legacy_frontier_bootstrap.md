# Legacy Frontier Bootstrap
<!-- 2025-10-27 Codex: Added 64-bit migration notes for persisted tables. -->

## Purpose
- Capture the sequence the classic Frontier desktop application follows to initialise the language runtime, hydrate the persisted object database, and expose the `system.verbs.builtins` glue that forwards into the kernel.
- Clarify which stages rely on state stored in the root database (`databases/Guest Databases/Frontier.root`) versus transient tables constructed at runtime, so the headless loader can mirror the behaviour.

## Bootstrap Timeline
- **Runtime scaffolding** – the host app initialises the language core and creates the shared infrastructure tables before any user database is opened.
- **Root database hydration** – the serialized root table is pulled into memory, global pointers are reset, and canonical system sub-tables (`system`, `system.verbs`, etc.) are located.
- **Runtime table linking** – in-memory-only tables (compiler, environment, temp) are linked under `system` to match how the desktop keeps them available without persisting them.
- **System scripts pass** – `system.startup` and the contents of `system.agents` are executed so glue and agents compile.
- **Kernel glue** – the persisted `system.verbs.builtins` table supplies `kernel(...)` shims whose symbols line up with the kernel token table populated from `'EFP#'` resources.

The following sections walk these phases in more detail.

## Runtime Scaffolding (pre-database)
- `initlang()` sets up the core callback table, error stacks, and hash table stack used by the evaluator (`Common/source/shell.c:1007`).
- `inittablestructure()` constructs the global scaffolding tables (`internaltable`, `efptable`, `environmenttable`, `charsetstable`, etc.) under a temporary root so the language can answer lookups before any database is opened (`Common/source/langstartup.c:522`).
- `langinitverbs()` loads constants, keywords (including the `kernel` token), and the kernel function processor definitions from resources (`Common/source/langstartup.c:969`).
- `langinitbuiltins()` seeds the kernel’s token table by reading the `'EFP#'` resource with id `idlangverbs` and registering each verb with the `langfunctionvalue` handler (`Common/source/langverbs.c:3562`).

### Guest Databases and `system.compiler.files`
- Frontier 5 introduced “Guest Databases” (additional `.root` files that stay open beside the system root). Each guest is mirrored under `system.compiler.files`, where the subtable name is the full path to the guest and the entries mirror the guest’s top-level tables.
- When compiled code runs, the kernel consults `system.compiler.files` so guest tables behave as if they were in scope globally. The headless loader must preserve that behaviour—whether by reconstructing the table exactly or by substituting an equivalent registry—so compiled scripts continue to resolve guest data without extra plumbing.

At this point the evaluator knows how to resolve `kernel` tokens, but there is no user database loaded and `roottable` still points at the transient scaffolding built by `inittablestructure()`.

## Loading the Root Database
- When the user opens a Frontier root file the desktop calls `ccloadfile()`, which drives `loadversion2cancoonfile()` to hydrate the persisted `tycancoonrecord` header and pull the root table address (`Common/source/cancoon.c:680`). (Note: The classic Frontier desktop application automatically opens Frontier.root in the same directory as the Frontier executable, if present.)
- `ccloadsystemtable()` loads the serialized root table from disk via `tableloadsystemtable()`, returning the table variable and its hash so the caller can install it as the live root (`Common/source/cancoon.c:493`, `Common/source/tablestructure.c:386`).
- `settablestructureglobals()` clears previous pointers, sets `rootvariable`/`roottable`, and invokes `checktablestructure()` to locate (or create, when bootstrapping an empty root) the canonical subtables: `system`, `system.verbs`, `system.verbs.builtins`, `system.agents`, etc. (`Common/source/tablestructure.c:657`).
- `tableverbinmemory()` is called as part of `tableloadsystemtable()` to ensure the table variable’s data block is resident in memory before the global pointers are wired, so the serialized `system.verbs.builtins` table is available immediately (`Common/source/tablestructure.c:433`).

Because the sanitized repository root already contains `system.verbs.builtins` (and the glue scripts beneath it), nothing in this path regenerates those entries—they simply become reachable once `checktablestructure()` resolves the subtable handles.

## Table Payload Format (v6 and v7 Databases)

Table payloads are stored using a two-level merged handle structure created by `tablepacktable()` and unpacked by `tableunpacktable()` (`Common/source/tablepack.c`):

### Modern (v7) Two-Level Merged Format
```
[outer_size: 4 bytes, big-endian]     ← outer merge prefix
  [inner_merged_table]                 ← created by hashpacktable()
  [formats_data]                       ← optional table formatting info
```

Where `inner_merged_table` is itself a merged handle:
```
[inner_size: 4 bytes, big-endian]     ← inner merge prefix
  [header: 16 bytes]                   ← tydisktablerecord
  [records: 10 bytes each]             ← tydisksymbolrecord array
  [sentinel: 10 zero bytes]            ← marks end of records
[strings]                              ← Pascal string pool
```

The table header (`tydisktablerecord`, 16 bytes total):
- `version` (2 bytes): table disk version (0x03 for v5.0+)
- `sortorder` (2 bytes): sort order flags
- `timecreated` (4 bytes): creation timestamp
- `timelastsave` (4 bytes): last save timestamp
- `flags` (4 bytes): XML and other flags

Each symbol record (`tydisksymbolrecord`, 10 bytes):
- `ixkey` (4 bytes, big-endian): offset into string pool for symbol name
- `valuetype` (1 byte): type of the value (string, int, table, etc.)
- `version` (1 byte): record version
- `data` (4 bytes): value data or offset into string pool

### Legacy v6 Format Differences

Legacy v6 databases store table payloads in Pascal-era format WITHOUT merge prefixes:
```
[header: 16 bytes]                    ← tydisktablerecord
[strings]                             ← Pascal string pool
[records: 10 bytes each]              ← tydisksymbolrecord array
[sentinel: 10 zero bytes]             ← all zeros
```

Key differences from modern format:
1. **No merge prefixes** – the payload is a direct concatenation with no size headers
2. **Strings before records** – the order is reversed (legacy has strings first, modern has records first in the inner merge)
3. **Single-level** – legacy format is flat, while modern uses two nested merges

_2025-11-07 update_: Capturing the migrated `system` table (`adr = 0x5d158b`) revealed that the “strings” section in production roots often embeds QuickDraw font blobs and table format runs ahead of the actual `tydisksymbolrecord` array. Those blobs are persisted verbatim from the legacy desktop builds, so the splitter must tolerate `[header][font blobs][records][format tail]` instead of assuming a pure Pascal string pool. The converter/debug docs should use real payload dumps (see `/tmp/frontier_legacy_dump_*.bin`) as fixtures when refining the parser.

### Headless Loader Conversion

The headless runtime detects legacy payloads by checking if the first 4 bytes form an invalid merge prefix (value < 16 or > payload_size - 4). When detected, `tableexternal_common.c:headless_convert_legacy_table_payload()` converts the legacy format:

1. **Find the split point** – scan for record-aligned boundaries where all record `ixkey` values are valid offsets into the preceding string pool
2. **Reorganize the data** – copy to `[header+records][strings]` order
3. **Create inner merge** – use `mergehandles()` to create the inner structure
4. **Create outer merge** – merge again with empty formats handle to match the two-level structure

This conversion allows both v6 and v7 databases to load correctly in the headless runtime without modifying the on-disk format.

### References
- Table packing: `Common/source/tablepack.c:tablepacktable()`
- Hash table packing: `Common/source/langhash.c:hashpacktable()`
- Hash table unpacking: `Common/source/langhash.c:hashunpacktable()`
- Merge utilities: `Common/source/memory.c:mergehandles()`, `unmergehandles()`
- Legacy conversion: `Common/source/tableexternal_common.c:headless_convert_legacy_table_payload()`

## Linking Runtime-Only Tables
- After the persisted structure is in place, the desktop links the shared runtime tables created earlier into the newly loaded root via `linksystemtablestructure()` (`Common/source/tablestructure.c:233`).
- This inserts `system.compiler`, `system.environment`, and `system.charsets` as non-saving externals, and creates an in-memory `system.temp` that is flagged as transient (`Common/source/tablestructure.c:259`–`269`).
- The link step ensures that any glue expecting `system.compiler` or `system.environment` to exist finds the same handles regardless of which root database is opened.

## Startup Scripts and Agents
- `ccinstalltablestructure()` runs once the root is live; it switches the process list to the database’s list, validates serial numbers in the classic app, and finally calls `loadsystemscripts()` (`Common/source/cancoon.c:437`).
- `loadsystemscripts()` executes `system.startup` and iterates `system.agents`, compiling and instantiating each agent (`Common/source/scripts.c:675`).
- This is the stage where glue scripts are compiled into runnable code and any agents that reference `system.verbs.builtins` handlers make their initial calls.

## Kernel Glue Handshake
- The glue scripts that bridge to the kernel live under `system.verbs.builtins` inside the root database. For example, the sanitized `Frontier.root` ships with entries such as `system.verbs.builtins.file.open` that call `kernel("file.open", adr, mode, false)`. These scripts are persisted user data, not rebuilt at runtime.
- When a glue script executes `kernel(address(...))`, the evaluator routes through `kernelcall()` → `kernelfunctionvalue()`, extracting the `system.verbs.builtins` table handle and verb name (`Common/source/langvalue.c:7595`, `Common/source/langvalue.c:7488`).
- `kernelfunctionvalue()` looks up the verb inside the hashtable that `loadfunctionprocessor()` populated from the `'EFP#'` resource and obtains the token that identifies the C verb (`Common/source/langstartup.c:188`, `Common/source/langvalue.c:7488`).
- The token is dispatched to `langfunctionvalue()`, which contains the switch statement that calls the actual C implementation of the verb (`Common/source/langverbs.c:3520`).
- Because both the script side (`system.verbs.builtins.<verb>`) and the kernel side (`efptable.<processor>.<verb>`) were prepared during the bootstrap stages above, no special casing is required—the script’s `kernel(...)` call lands in the appropriate C routine automatically.

### Persistence vs. Runtime Summary
- **Persisted in the root:** `system`, `system.verbs`, `system.verbs.builtins`, `system.agents`, and their glue scripts. These must already exist in the `.root` file; the loader simply hydrates them.
- **Constructed every launch:** `internaltable`, `efptable`, `system.environment`, `system.charsets`, `system.temp`, and the kernel verb token table.
- **Executed every launch:** `system.startup` and `system.agents` handlers that may themselves touch the kernel glue.

## Troubleshooting Notes
- If `system.verbs.builtins` appears empty after loading a root, confirm that `settablestructureglobals()` is being called with `flcreatesubs == false`. Passing `true` would create a fresh table and discard the serialized one.
- Missing kernel verbs usually indicate `langinitverbs()` (or the underlying `loadfunctionprocessor()`) was skipped, leaving the kernel token table empty even though the script side exists.
- When porting the headless loader, mirror the desktop order: initialise the language (`initlang()` → `inittablestructure()` → `langinitverbs()`), then open the database and call `tableloadsystemtable()`, `settablestructureglobals()`, `linksystemtablestructure()`, and finally `loadsystemscripts()`. Skipping any phase breaks the handshake between the persisted glue and the C kernel.
- Add follow-up doc that serves as a comprehensive guide to the `.root` file format: physical block layout, headers, payload records, address/variance handling, and the v6 → v7 migration specifics (including byte order and pointer width changes). This should be detailed enough for a new developer to reason about the on-disk structures while inspecting or migrating databases.

## References
- Runtime init chain: `Common/source/shell.c:1007`
- Initial scaffolding tables: `Common/source/langstartup.c:522`
- Root hydration: `Common/source/cancoon.c:493`, `Common/source/tablestructure.c:386`
- Global wiring: `Common/source/tablestructure.c:657`
- Runtime linking: `Common/source/tablestructure.c:233`
- Startup scripts: `Common/source/scripts.c:675`
- Kernel dispatch: `Common/source/langstartup.c:188`, `Common/source/langverbs.c:3562`, `Common/source/langvalue.c:7488`

## Migration Considerations (2025-10-27 Codex)
- `Common/source/db_format.c:migrate_32bit_to_64bit()` currently rewrites only the database header (bumping it to version 7) and streams the remainder of the v6 file into the output unchanged. The resulting root keeps every table payload in the legacy 32-bit layout.
- Once the runtime sees the v7 header it enables `use_64bit_format`, so routines like `tableverbunpack()` expect 8-byte `dbaddress` fields. When they encounter copied 4-byte payloads they spill past the record unless patched with ad hoc fallbacks.
- The long-term fix is to have the migrator load each legacy table, flip `use_64bit_format = true`, and save it back via `tableverbpack()`/`hashpacktable()` before writing it to the new file. That emits widened addresses, refreshed block sizes, and lets the CLI/runtime operate without special cases for migrated roots.
- Hydration now updates `views[0]` via `dbsetview()` to point directly at the packed root table (the legacy Cancoon record is intentionally omitted), so `read_root_table_address()` can always rediscover the entrypoint without special-casing the compatibility shim.

### Mac-Specific Glue (`system.macintosh.*`)
- The `system.macintosh` hierarchy is dedicated to OSA/AppleEvent compliance on classic macOS shells. For example, `system.macintosh.required.quitApplication` handles the `kAEQuitApplication` event by relaying to `finderEvent(...)`, and the constants it needs live under `system.macintosh.constants`.
- Headless/CLI builds must *not* rely on these tables: if a headless feature appears to require `system.macintosh.*`, treat it as a bug and refactor the caller to use platform-neutral pathways (kernel verbs, RPC adapters, etc.). Only the desktop UI layer should wire AppleEvents into these scripts.
