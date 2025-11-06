# Portable DB Access Plan
**Date:** October 31, 2025  \
**Owner:** Codex  \
**Status:** Draft – in progress

## North Star Alignment
To keep the Frontier core permanently headless, the database migration helpers
(`db_format.c`, save-as flows, table hydration) must stop linking against the
desktop-only database stack. The goal is to service all file/database
operations through the portable runtime while the legacy UI and Carbon code is
retired.

## Objectives
1. Provide a `portable/db_portable` module exposing just the symbols required
   by the migration tooling (`openfile`, `dbopenfile`, `dbstartsaveas`,
   `dbassign*`, `tableloadsystemtable`, etc.).
2. Implement those helpers using the existing headless-safe building blocks
   (stdio-backed file layer, table unpack/pack helpers, `db_format` conversion
   routines) so they compile without Carbon or UI types.
3. Update the migration tests/CLI to link against the portable wrappers,
   ensuring we no longer depend on `macconv.h` or other desktop headers to read
   and rewrite `.root` files.

## Plan of Record
1. **Shim design** – Capture the exact function list needed by
   `db_format.c`, `save_migration_tests`, and future headless database flows.
   Define `portable/db_portable.h` with this minimal API and document how each
   call maps to modern code paths.
2. **Implementation** – Build `portable/db_portable.c` with stdio-backed file
   access, block allocation helpers, and table load/save routines directly
   adapted from the headless-safe portions of `db.c`. Any shared logic should
   be extracted into neutral helpers so the desktop build can continue to use
   the original modules.
3. **Runtime integration** – Patch `db_format.c`, the migration tests, and
   `frontier-cli` bootstrap to include `db_portable.h` instead of the full
   `db.h` interface whenever `FRONTIER_HEADLESS` is defined. Update the test
   makefile to link the new module, and remove the temporary headless stubs for
   the affected functions.
4. **Follow-up cleanup** – Remove the now-unused stubs from
   `headless_mac_compat.c`/`headless_stubs.h`, update `_CURRENT_STATUS.md` and
   the Carbon migration status log, and create regression coverage for the
   portable db shim.

## Dependencies & Risks
- Requires the remaining Carbon helpers (handle math, byte-order utilities,
  etc.) to live in portable headers. Those tasks are tracked in
  `header_cleanup_plan.md` follow-ups.
- Desktop builds must continue to compile against the existing `db.c` entry
  points until a dedicated desktop UI is designed.
- The shim should stay thin; any logic that needs to be shared should move into
  a common helper rather than growing a parallel implementation.

## Next Actions
- 2025-11-05: Stub removed — `save_migration_tests` exercises `db_portable` against a migrated root (`db_portable_getview` + `db_portable_refhandle`). Keep expanding coverage to include writes/save-as paths.
- Finalise any remaining helper exports and ensure callers (CLI/tests) rely exclusively on `db_portable.h` when headless.
- Extract or share block read/write helpers where duplication remains, then update `_CURRENT_STATUS.md` as new milestones land.
