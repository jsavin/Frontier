# Carbon Migration – Current Status

**Last Updated**: November 9, 2025  \
**Branches in flight**: `feature/carbon-migration-plan`, `feature/headless-system-bootstrap`

## Recent Updates — November 9, 2025
- Legacy table converter now reconstructs modern merged handles using the documented `[outer merge][inner header+records][strings][formats]` layout. We retired the heuristic splitter, so every legacy table (including `system.verbs.*`) hydrates without manual trimming.
- `frontier-cli` no longer relies on `langrunstringnoerror`; we feed every inline script through `langrun`, then render results via `hashgetvaluestring`. That fixes “silent” failures for non-string values and gets `defined(system.verbs.globals)` working end-to-end.
- `tests/cli_system_defined` still fails at `clock.now()` because the script resolution path never reaches the real implementation—`langrun` returns false with an empty error. The logs show repeated `langsearchpathlookup` misses (e.g., `script`, `scriptions`), so the remaining work is to confirm that `pathstable` is populated before evaluation and that `langsearchpathvisit` walks those addresses in headless mode.
- Next actions: instrument `langsearchpathvisit` to prove which tables are visited, verify `system.paths` entries resolve to real tables, and unblock `clock.now()` so we can move `tests/cli_system_defined` into `RUN_PREBUILT`.
- Added headless-only logging inside `langsearchpathvisit/langsearchpathlookup` (see `Common/source/langvalue.c`). Running `script.getText(...)` now shows each `path##` entry visit plus the final hit, proving that the search path wiring works even though deeper script helpers still fail.
- Captured the date/time modernization plan (Mac epoch ↔ POSIX ms with timezone heuristics) under “Phase 3 — Date/Time Representation Modernization” in `planning/TODO_future_improvements.md`. We’ll implement the converter once `clock.*` executes cleanly in headless mode.
- Authored `planning/carbon_migration/time_portable_migration.md`, outlining the portable time module, database storage changes (Unix epoch ms + timezone), and the migration phases needed to drop all Carbon dependencies for date/time verbs.

## Updated Plan — October 29, 2025

We pivoted from incremental shims to a comprehensive Carbon-dependency retirement. The canonical plan now lives under [`planning/carbon_migration/`](carbon_migration/README.md).

1. **Planning skeleton (done)**: Created the Carbon migration directory with README, inventory, phases, decision log, and status log.
2. **Doc refresh (done)**: Updated the planning index/README, restored in-progress phase docs (with status tracking such as in `langhash_portable_missing_types.md`), and pointed the archive only at completed notes.
3. **Execution phases**: See [`carbon_migration/phases.md`](carbon_migration/phases.md) for subsystem milestones (header hygiene → runtime primitives → encoding → AppleEvents → cleanup).

## Longer-Term Goal
Run Frontier without any Classic Mac / Carbon APIs while keeping the headless and desktop builds unified. Completing the Carbon plan is now the primary Phase 3 objective.

- New call-site maps document the legacy touch points: see [`maps/getstringlist_map.md`](carbon_migration/maps/getstringlist_map.md), [`maps/filespec_alias_map.md`](carbon_migration/maps/filespec_alias_map.md), [`maps/quickdraw_map.md`](carbon_migration/maps/quickdraw_map.md), and [`maps/appleevent_map.md`](carbon_migration/maps/appleevent_map.md).
- Tracer bullets for each branch (with exit criteria) live in [`tracer_bullets.md`](carbon_migration/tracer_bullets.md); treat them as acceptance gates before merging each PR.
- Classic handle shim now tracks `MemError`/`MaxBlock`, and `make -C tests handle_tests && ./handle_tests` verifies both success/failure paths.
- Windows parity review complete; see [`windows_parity_review.md`](carbon_migration/windows_parity_review.md) for reuse notes (IPC, feature flags, resource tables).
- STR# replacement plan updated: Phase 1 vendors libyaml for full parity now, Phase 2 revisits the bison pipeline later (see [`strings_replacement_plan.md`](carbon_migration/strings_replacement_plan.md)).
- Libyaml 0.2.5 is vendored under `third_party/libyaml`, and `tools/strings_compiler/` now wraps it (`strings_yaml_loader.c`) to generate C/H/manifest outputs.
- Completed map review confirms the priority order: headless must replace STR# strings, alias serialization, QuickDraw string metrics, and AppleEvent IPC before we can drop the portable stubs.
- Portable headers (`standard_portable.h`, `osincludes_portable.h`) now gate every typedef behind explicit feature macros, so headless builds no longer trip Str255/bigstring redefinitions when compiling the shared runtime (`make -C tests runtime_tests` rebuilt cleanly apart from existing db warnings).
- The YAML-backed `strings_compiler` now feeds both the test suite and CLI builds: `make -C tests` and `make -C frontier-cli` automatically invoke the generator, add `generated/strings_tables.c` to the runtime sources, and define `FRONTIER_PORTABLE_STRINGS` for the new lookup helpers. `headless_lang_runtime_more_stubs.c` now pulls from the generated tables (falling back to empty strings until more YAML data lands).
- Layering dependencies are tracked in [`carbon_migration/dependency_matrix.md`](carbon_migration/dependency_matrix.md); update that matrix whenever a layer flips state.
- Foundation slice now builds end-to-end: `portable/quickdraw_portable.h`, `portable/appleevent_portable.c`, `portable/text_encoding_portable.c`, and the updated test CFLAGS in `tests/Makefile` (adds `FRONTIER_PORTABLE_FILE_AVAILABLE`, `FRONTIER_PORTABLE_DB_AVAILABLE`, `HEADLESS_LINKS_REAL_DB`) allow `make -C tests runtime_tests` to link the real DB/file shims without including Carbon headers.
- Filespec/alias serialization now stores POSIX-style paths (see `langpackfileval`, `langunpackfileval`, `langhash.c`); headless builds never call `filespectoalias`/`aliastofilespec` at runtime.
- Hydration now writes view₀ directly to the packed root table and no longer emits the legacy Cancoon record; headless builds also skip persisting table format/font metadata so regenerated v7 roots contain only data/scripts (UI prefs will move to a per-user store later).
- Headless `checktablestructure()` no longer attempts to hydrate `system.menus`/`system.menus.bar` because the menu processor externals still depend on Carbon UI code. Desktop builds continue to link those tables; headless simply leaves the table pointer nil to avoid needless failures while we focus on runtime verbs (`clock.*`, `system.verbs.*`).
- Added a CLI regression harness at [`tests/cli_system_defined`](../tests/cli_system_defined) that hydrates a copy of the system root and runs `defined(system.verbs)`/`clock.now()`. Wiring into `RUN_PREBUILT` is currently blocked because the legacy table converter rejects the migrated `system` payload (current splitter assumes `[header][strings][records][sentinel]` but the v6→v7 roots keep font blobs and format data ahead of the records). Fix the converter, then promote the script.
- Latest payload inspection (`0x5d158b`) confirmed that the so-called “strings” section actually embeds QuickDraw font data, so the `ixkey < strings_len` heuristic only recovers three entries. We have now documented the real v6 packing order (see `docs/database_architecture.md`, “Table Payload Layout”), but still need to capture golden bytes under `planning/carbon_migration/` and teach the converter/tests to honor that structure.
- Captured the first golden payload for `system.verbs.globals` under `planning/carbon_migration/data/system_verbs_globals_legacy.bin` (using `scripts/extract_legacy_table.py`), so upcoming converter/tests can diff against a stable table that will survive the UI cleanup work.
- **Future hygiene (prioritised):**
  - P0: Extend the v7 header with explicit offsets/lengths for `records`, `strings`, and `format blob` so the loader doesn’t rely on heuristics.
  - P0: Add a small `formatVersion` byte near the table header so future migrations can select the right parser without guessing.
  - P0: Add a canonical `docs/db_format.md` describing every on-disk structure (tydisktablerecord, tydisksymbolrecord, tyversion2tablediskrecord) plus byte diagrams for v6/v7.
  - P1: Check in regression fixtures (captured table payloads) under `tests/data/` so migrator changes can diff against known-good binaries.

### Refactor Branch Sequencing (draft)
| Order | Scope | Key Deliverables | Dependencies | Notes |
| --- | --- | --- | --- | --- |
| 1 | Memory layer cleanup | Extend portable handle API (`classic_handle.c`) to cover `MemError/MaxBlock`, refactor `memory.c` and callers to use it. | None (prereq for later builds) | Unlocks headless build without Classic Memory Manager. |
| 2 | Strings / resource fork replacement | Replace STR# loaders with C tables or modern data files; update all core call sites listed in [`getstringlist_map.md`](carbon_migration/maps/getstringlist_map.md). | 1 | Enables removal of resource fork APIs from headless builds. |
| 3 | Filespec / alias abstraction | Design cross-platform path value representation, rewrite `langhash.c`, `langpack.c`, and related verbs per [`filespec_alias_map.md`](carbon_migration/maps/filespec_alias_map.md). | 2 | Removes dependency on Alias Manager for runtime features. |
| 4 | QuickDraw extraction | Provide headless-safe string width heuristics, move remaining QuickDraw helpers behind desktop-only modules as per [`quickdraw_map.md`](carbon_migration/maps/quickdraw_map.md). | 2 | Allows headless builds to drop QuickDraw includes entirely. |
| 5 | AppleEvent isolation | Split `langipc.c` / `osacomponent.c` into macOS-only codepaths, implement headless dispatch (inspired by Windows build) following [`appleevent_map.md`](carbon_migration/maps/appleevent_map.md). | 3 | Eliminates AppleEvent stubs from portable headers. |
| 6 | Header / include cleanup | After subsystems migrate, prune `osincludes_portable.h` and `headless_stubs.h`, enforce clean include graph, refresh tests. | 4 & 5 | Final polish before broader refactors. |

## Progress Snapshot
- ✅ Added stdio-backed file layer shared by headless tests/CLI (eliminated legacy file stubs).
- ✅ Seeded Carbon migration plan, inventory, and decision log.
- ✅ Portable handle shim now reuses `frontierAlloc` and records `MemError`/`MaxBlock`; regression coverage lives in `tests/handle_tests.c`.
- ✅ Scaffolded `strings_compiler` (bison/flex) to translate YAML string tables into generated headers/C sources.
- 🔄 Working on header hygiene: ensuring `frontier.h` and related headers bring in the correct portable definitions.

## Immediate Next Steps
1. **Document call-site maps** (Assistant) — ✅: Outputs captured under `planning/carbon_migration/maps/` for STR# strings, filespec/alias, QuickDraw helpers, and AppleEvents. Use these tables to size each refactor.
2. **Sequence refactor PRs** (Assistant) — ✅: Drafted the branch order above and linked tracer bullets in `tracer_bullets.md`; log entry added to `status_log.md`. Refer to this sequencing when planning upcoming PRs.
3. **Windows parity review** (Assistant) — ✅: Findings captured in [`windows_parity_review.md`](carbon_migration/windows_parity_review.md); use it as input for the AppleEvent and string-table work.
4. **Prepare resource fork replacement plan** (Assistant) — ✅: Plan captured in [`strings_replacement_plan.md`](carbon_migration/strings_replacement_plan.md) with a bison-based compiler design; next action is to backfill YAML tables and generator integration.
5. **Portable header guard cleanup** (Assistant) — ✅ 2025-11-09: Added explicit feature macros around every typedef in `portable/standard_portable.h` and `Common/headers/osincludes_portable.h`, eliminating the Str255/bigstring redefinition spam and giving downstream refactor branches a stable include base.
6. **Integrate strings compiler** (Assistant) — ✅ 2025-11-09: Tests and the CLI now depend on `generated/strings_tables.{c,h}`; both Makefiles build the generator, add `../generated` to the include path, and define `FRONTIER_PORTABLE_STRINGS` so the headless runtime can start consuming the emitted tables.
7. **Layer 2 follow-up** (Assistant): Harden the legacy table converter so the CLI regression can hydrate migrated roots without crashing. Documented the `0x5d158b` payload layout (done; see docs), now update the splitter to handle `[header][records][strings][tyversion2tablediskrecord ...]` and add a regression in `tests/cli_system_defined`. Once the script passes locally, promote it into `RUN_PREBUILT` and log the verification in `carbon_migration/status_log.md`. (P0) *Status:* in progress — `defined(system.verbs)` works, but `clock.now()` still fails because the hydrator currently mangles the `system.paths` search entries (names look garbled in the current logs) and we still need a deterministic reproduction to finish the converter and link the real paths.

   *Follow-on once `clock.now()` is green:* (a) wire a headless-stub logger so every unimplemented external (menu, QuickDraw, clock, etc.) emits a one-time warning identifying the missing API; (b) keep the real `langcallbacks.errormessagecallback` active for CLI inline scripts so failures like `clock.now()` report an actual message instead of the generic “Script execution failed.” Those diagnostic hooks will make similar regressions much easier to chase.

Progress and blockers should continue to be logged in the Carbon migration status log and decisions documented in the decision log.
