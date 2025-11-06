# Carbon Migration – Current Status

**Last Updated**: October 31, 2025  \
**Branches in flight**: `feature/carbon-migration-plan`, `feature/headless-system-bootstrap`

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
- Portable headers (`standard_portable.h`, `osincludes_portable.h`) still need guard cleanup (no more duplicate typedefs) before the refactor branches land.

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
- ✅ Header hygiene Step A/B/C/D: Step D rerun (`make -C tools/strings_compiler`, `make -C tests all`, `make -C frontier-cli`) now passes after adding `portable/appleevent_portable.c`, folding the stdio/db shims into runtime builds, and trimming duplicate stubs. Remaining work is warning cleanup (pointer-sign noise in `findinfile.c`, variance casts in `db.c`).
- ✅ AppleEvent shim decoupled from `osincludes_portable.h`; headless stubs now defer to the inline implementations provided by `appleevent_portable_shim.h` and the TEC shim.
- ✅ Portable text-encoding bridge now lives in `portable/text_encoding_portable.{h,c}`, returning a small set of canonical encodings (UTF-8/MacRoman/Windows-1252) so `initCharsetsTable` can populate headless builds without Carbon. Missing names fall back cleanly.
- 🆕 Logged follow-up work for db I/O wrappers and lingering Carbon helpers in [`header_cleanup_plan.md`](carbon_migration/header_cleanup_plan.md); warnings from the green Step D rebuild are tracked there.
- 🆕 Captured the portable database shim roadmap in [`db_portable_plan.md`](carbon_migration/db_portable_plan.md); this is the path to keep the core permanently headless while migration tooling stays functional.
- 🆕 Added a headless string-table bridge (`strings_portable.c`) that reads the generated YAML tables; missing entries currently fall back to `[list:id]` placeholders and log stderr warnings.

## Immediate Next Steps
1. **Document call-site maps** (Assistant) — ✅: Outputs captured under `planning/carbon_migration/maps/` for STR# strings, filespec/alias, QuickDraw helpers, and AppleEvents. Use these tables to size each refactor.
2. **Sequence refactor PRs** (Assistant) — ✅: Drafted the branch order above and linked tracer bullets in `tracer_bullets.md`; log entry added to `status_log.md`. Refer to this sequencing when planning upcoming PRs.
3. **Windows parity review** (Assistant) — ✅: Findings captured in [`windows_parity_review.md`](carbon_migration/windows_parity_review.md); use it as input for the AppleEvent and string-table work.
4. **Prepare resource fork replacement plan** (Assistant) — ✅: Plan captured in [`strings_replacement_plan.md`](carbon_migration/strings_replacement_plan.md) with a bison-based compiler design; next action is to backfill YAML tables and generator integration.
5. **Portable header guard cleanup** (Assistant) — 🔄: Following [`header_cleanup_plan.md`](carbon_migration/header_cleanup_plan.md): (a) QuickDraw/UI extraction **done**, (b) TextEncoding shim sourced via `portable/text_encoding_portable.h`, (c) AppleEvent shim decoupled and guarded (next task: corral remaining AE typedefs in `macconv.h`/Windows stubs), (d) include audit/regression rebuild to rerun once the final AppleEvent cleanups land.
6. **Portable DB shim** (Assistant) — ✅: `portable/db_portable.(h|c)` fronts the legacy calls across tests/CLI, the stub (`tests/db_portable_stub.c`) is gone, and `save_migration_tests` now opens the migrated root via the shim (verifying `db_portable_getview`/`db_portable_refhandle`). Follow-up: broaden coverage (writes/save-as flow) as we continue the migration plan.
7. **Integrate strings compiler** (Assistant) — ✅: libyaml-backed generator (`tools/strings_compiler/`) now drives `make strings_generated`; extend tests later as we add more tables.
8. **Include audit & rebuild** (Assistant) — ✅: Step D commands now pass (`make -C tools/strings_compiler`, `make -C tests all`, `make -C frontier-cli`). Follow-up: scrub residual warnings (pointer-sign mismatches, db variance casts) and keep the follow-up list in `header_cleanup_plan.md` current.

Progress and blockers should continue to be logged in the Carbon migration status log and decisions documented in the decision log.
