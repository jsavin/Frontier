# Legacy Frontier Bootstrap

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

At this point the evaluator knows how to resolve `kernel` tokens, but there is no user database loaded and `roottable` still points at the transient scaffolding built by `inittablestructure()`.

## Loading the Root Database
- When the user opens a Frontier root file the desktop calls `ccloadfile()`, which drives `loadversion2cancoonfile()` to hydrate the persisted `tycancoonrecord` header and pull the root table address (`Common/source/cancoon.c:680`). (Note: The classic Frontier desktop application automatically opens Frontier.root in the same directory as the Frontier executable, if present.)
- `ccloadsystemtable()` loads the serialized root table from disk via `tableloadsystemtable()`, returning the table variable and its hash so the caller can install it as the live root (`Common/source/cancoon.c:493`, `Common/source/tablestructure.c:386`).
- `settablestructureglobals()` clears previous pointers, sets `rootvariable`/`roottable`, and invokes `checktablestructure()` to locate (or create, when bootstrapping an empty root) the canonical subtables: `system`, `system.verbs`, `system.verbs.builtins`, `system.agents`, etc. (`Common/source/tablestructure.c:657`).
- `tableverbinmemory()` is called as part of `tableloadsystemtable()` to ensure the table variable’s data block is resident in memory before the global pointers are wired, so the serialized `system.verbs.builtins` table is available immediately (`Common/source/tablestructure.c:433`).

Because the sanitized repository root already contains `system.verbs.builtins` (and the glue scripts beneath it), nothing in this path regenerates those entries—they simply become reachable once `checktablestructure()` resolves the subtable handles.

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

## References
- Runtime init chain: `Common/source/shell.c:1007`
- Initial scaffolding tables: `Common/source/langstartup.c:522`
- Root hydration: `Common/source/cancoon.c:493`, `Common/source/tablestructure.c:386`
- Global wiring: `Common/source/tablestructure.c:657`
- Runtime linking: `Common/source/tablestructure.c:233`
- Startup scripts: `Common/source/scripts.c:675`
- Kernel dispatch: `Common/source/langstartup.c:188`, `Common/source/langverbs.c:3562`, `Common/source/langvalue.c:7488`
