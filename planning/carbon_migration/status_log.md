# 2025-11-09
- Rebuilt `tableexternal_common.c` to consume the documented legacy layout (`[outer merge][inner header+records][strings][formats]`) so migrated tables unpack without guesswork; captured a golden `system.verbs.globals` payload under `planning/carbon_migration/data/` for future tests.
- Updated `frontier-cli/cli_executor.c` to run every inline script through `langrun` and format results via `hashgetvaluestring`, fixing the old `langrunstringnoerror` coercion failure (`clock.now()` no longer dies because it returns a date value).
- Re-ran `tests/cli_system_defined`: `defined(system.verbs.globals)` now passes, but `clock.now()` still fails because the search-path lookup never resolves verbs such as `script.getText`. Next step is to instrument `langsearchpathvisit` and confirm `system.paths` hydration before wiring the regression into `RUN_PREBUILT`.
- Finished the portable header guard cleanup: both `portable/standard_portable.h` and `Common/headers/osincludes_portable.h` now wrap every typedef with explicit feature macros so shared builds no longer trip Str255/bigstring redefinitions; validated via `make -C tests runtime_tests -B` (only legacy db warnings remain).
- Wired the YAML-backed `strings_compiler` into both `tests/Makefile` and `frontier-cli/Makefile`; the builds now auto-generate `generated/strings_tables.{c,h}` before compiling, pass `-I../generated`, and define `FRONTIER_PORTABLE_STRINGS` so headless string lookups can migrate away from the Carbon Resource Manager.
- Headless tablestructure now skips hydrations of `system.menus`/`system.menus.bar`; desktop builds still link them, but headless builds leave the pointers nil to avoid the unsupported `idmenuprocessor` externals while we focus on runtime verbs (`clock.*`, `system.verbs.*`).

# 2025-11-08
- Documented the verified legacy table layout (two nested `mergehandles`, no sentinel) in `docs/database_architecture.md` and `_CURRENT_STATUS.md` so future work no longer relies on the incorrect `[header][strings][records]` heuristic.
- Added `scripts/extract_legacy_table.py` and captured `planning/carbon_migration/data/system_verbs_globals_legacy.bin` as the first golden v6 payload (system.verbs.globals) for use in converter tests.
- Hydration now rewrites `views[0]` to point directly at the packed root table and no longer emits the legacy Cancoon record; `read_root_table_address()` still understands old files, but clean v7 roots skip the compatibility shim entirely.
- Headless table packing no longer serializes format/font/window metadata, so v7 saves contain pure data/scripts; a future milestone will introduce a per-user store for view preferences.

# 2025-11-07
- Fixed the system-root loader regression: `dbflushheader` now emits the modern 64-bit header so Cancoon view₀ survives hydration/reload, unblocking `read_root_table_address` on v7 databases. CLI regression still pending because `frontier-cli` currently fails to compile against the legacy UI headers; rerun `tests/cli_system_defined` once the QuickDraw cleanup enables that build path.
- Discovered the remaining blocker for `tests/cli_system_defined`: the legacy table converter (`tableexternal_common.c`) only handles `[header][strings][records][sentinel]`, while migrated `system` payloads keep extra bytes after the string pool. Update the converter to mirror the real v6 layout before promoting the CLI regression.
- Investigated the `0x5d158b` payload produced during hydration: the so-called “string pool” actually embeds QuickDraw font blobs, so the `ixkey < strings_len` heuristic only recovers three entries. Next step is to document the true `[header][font blobs][symbol records][format tail]` layout and update the converter/tests before re-running `tests/cli_system_defined`.
- Captured follow-up tasks (prioritised): P0 extend v7 headers with explicit offsets; P0 embed a format-version byte near the table header; P0 document the on-disk layout; P1 add binary fixtures for regression tests.

# 2025-10-31
- Captured call-site maps for STR# string lists, filespec/alias helpers, QuickDraw utilities, and AppleEvent IPC under [`maps/`](maps/). Updated `_CURRENT_STATUS.md` with the completed Step 1 milestone.
- Documented the Carbon refactor branch order (memory → strings/resources → filespec/alias → QuickDraw → AppleEvents → header cleanup) in `_CURRENT_STATUS.md`.
- Added tracer bullet milestones for each branch in [`tracer_bullets.md`](tracer_bullets.md).
- Extended `portable/classic_handle.c` to reuse the shared allocator, track `MemError`/`MaxBlock`, and guarded the headless stubs; `make -C tests handle_tests && ./handle_tests` now covers these paths.
- Completed Win32 parity review; documented IPC reuse, feature flags, and resource table notes in [`windows_parity_review.md`](windows_parity_review.md).
- Drafted STR# replacement plan (`strings_replacement_plan.md`) centered on a bison-driven YAML compiler, outlining source format, tooling, migration steps, and tests.
- Implemented the initial `tools/strings_compiler/` utility (bison/flex) and verified it generates C/H/manifest outputs for sample YAML tables.
- Vendored libyaml 0.2.5 under `third_party/libyaml/` and updated `tools/strings_compiler/` to load YAML via libyaml; bison/flex work deferred to Phase 2.

# 2025-10-30
- Documented remaining platform-specific dependencies in [`platform_legacy_audit.md`](platform_legacy_audit.md) and updated `_CURRENT_STATUS.md` with the refactor roadmap.

# Carbon Migration Status Log

Chronological breadcrumbs for this project. Add an entry whenever we complete a meaningful milestone (phase finished, major PR merged, blocker discovered, etc.). Keep entries short and link to PRs when available.

## 2025-10-29
- Created Carbon migration plan (`planning/carbon_migration/`).
- Updated status/index docs to point at the new workstream.
- Inventory seeded from current headless build failures.

<!-- Add newest entries to the top -->
