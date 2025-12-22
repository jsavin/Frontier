# DB Context Completion Plan (Save As, Free-List, Headless)

Goal: Finish deglobalizing DB operations by routing Save As, free-list/release-stack logic, and headless callers through `db_context`, eliminating ambient `databasedata`/`databasedestination` reliance and the `fldatabasesaveas` swap assumptions.

## Status
- State: Completed - Core infrastructure in place
- Phase: 3 (DB Context Completion)
- Last Updated: 2025-12-22
- Owner: Codex
- Notes: db_context infrastructure complete with context-aware shims for pack/unpack and core DB APIs. Remaining work (dbverbs/db.c full adoption) deferred. Preserved in archive for reference.

## Current State
- Context wrappers exist for core ops (`dbassign/ref/copy`, adapter enable, Save As start/end, push/pop, release stack, shadow avail list). Default context (`g_default_db_context`) is in place for public wrappers.
- TLS `use_64bit_format` shim removed; mode is tracked in thread-local `g_mode_state`.
- Two-context regressions added in `db_format_tests`; suite passes.
- Save As state now lives in `db_context` snapshots/guards; default wrappers refresh before use, and the migrator binds its destination handle directly instead of querying global Save As state.
- Remaining globals: Save As swap (`dbswapglobals`), free-list/release-stack/shadow-avail logic, and some public db.c flows still mutate `databasedata`/`databasedestination` directly. Headless callers on these paths still assume globals.

## Plan (systematic steps)
1) **Inventory remaining global touchpoints in db.c** — Done
   - `dbswapglobals`/`fldatabasesaveas` usage in Save As flows. — Done
   - Free-list/shadow helpers (`dbrelease_internal`, `dbwriteshadowavaillist`, `dbclearshadowavaillist`, release stack). — Done
   - Public wrappers (`dbpushdatabase/dbpopdatabase`, Save As start/end, dballocate/dbreference_handle, etc.) that still rely on ambient globals. — Done

2) **Introduce context defaults and migrate public wrappers** — Done
   - Ensure all public db.c entry points (`dbassign`, `dbcopy`, `dbreference`, `dbpushdatabase`, `dbpopdatabase`, Save As start/end, release-stack helpers) delegate to context versions using `g_default_db_context`. — Done
   - Provide context-aware wrappers for free-list/shadow/release-stack and Save As swap (`dbswapglobals_context`). — Done

3) **Refactor internal logic to accept/apply context** — Done
   - Thread `dbswapglobals` and Save As swap/release-stack/shadow/free-block paths to operate on the active context instead of global `databasedata`/`databasedestination`. — Done
   - Keep a thin default-context shim to preserve ABI for legacy callers. — Done

4) **Update callers** — In Progress
   - Replace Save As/free-list/release-stack call sites (including migrator, adapter paths, and any db.c internal callers) with context-aware variants. — Done
   - Update headless stubs/tests that still assume globals on these paths to use context wrappers or apply the default context explicitly. — Pending: add two-context Save As integration test for coverage.

5) **Cleanup** — Pending
   - Remove or deprecate legacy no-context wrappers once callers are migrated. — Pending
   - Confirm no remaining references to `databasedata`/`databasedestination` in context-ready code paths (allow only inside default-context shims). — Pending (manual scan shows only core DB internals remain)

6) **Validation** — In Progress
   - Run `make -C tests db_format_tests`, `runtime_tests`, `cli_runtime_tests` (as feasible), and headless suites if available. — Done
   - Add/extend two-context regressions to cover Save As/free-list behavior (e.g., legacy source + modern dest, concurrent contexts). — Pending (Save As integration test still to add)

## Deliverables
- Refactored db.c with context-aware Save As and free-list/release-stack paths.
- Headless/tests updated to context APIs.
- Docs updated (`planning/_CURRENT_STATUS.md` referencing this plan) and tests logged.***
