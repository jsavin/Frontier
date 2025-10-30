# Carbon Migration – Current Status

**Last Updated**: October 30, 2025  \
**Branches in flight**: `feature/carbon-migration-plan`, `feature/headless-system-bootstrap`

## Updated Plan — October 29, 2025

We pivoted from incremental shims to a comprehensive Carbon-dependency retirement. The canonical plan now lives under [`planning/carbon_migration/`](carbon_migration/README.md).

1. **Planning skeleton (done)**: Created the Carbon migration directory with README, inventory, phases, decision log, and status log.
2. **Doc refresh (done)**: Updated navigation/index files and restored in-progress phase docs to their active directories so contributors land on the right plan.
3. **Execution phases**: See [`carbon_migration/phases.md`](carbon_migration/phases.md) for subsystem milestones (header hygiene → runtime primitives → encoding → AppleEvents → cleanup).

## Longer-Term Goal
Run Frontier without any Classic Mac / Carbon APIs while keeping the headless and desktop builds unified. Completing the Carbon plan is now the primary Phase 3 objective.

## Quick Status — October 30, 2025
- Headless build still fails when linking `langhash.c` and `strings.c` because Carbon-era helpers (`TEC*`, AppleEvents, alias manager) remain. The failures are recorded in the [inventory](carbon_migration/inventory.md).
- Portable headers (`osincludes_portable.h`, `standard_portable.h`) were expanded, but need further work to cover extended float, TEC APIs, and AE constants.
- Planning documents have been restructured; `_CURRENT_STATUS.md` now tracks the Carbon migration rather than the earlier v6→v7 database effort.
- In-progress legacy phase docs were moved back under `planning/phase*/` so they can continue to evolve; the archive now holds completed notes only.

## Progress Snapshot
- ✅ Added stdio-backed file layer shared by headless tests/CLI (eliminated legacy file stubs).
- ✅ Seeded Carbon migration plan, inventory, and decision log.
- 🔄 Working on header hygiene: ensuring `frontier.h` and related headers bring in the correct portable definitions.

## Immediate Next Steps
1. **Header hygiene** (Assistant):
   - Finish wiring `frontier.h`/`standard.h` to use portable equivalents under `FRONTIER_HEADLESS`.
   - Extend `standard_portable.h` and `osincludes_portable.h` until the portable build no longer complains about missing typedefs/macros (per `inventory.md`).
2. **Validation** (Assistant):
   - Re-run `make -C tests test_migration` after header fixes; log results in `planning/carbon_migration/status_log.md`.

Progress and blockers should continue to be logged in the Carbon migration status log and decisions documented in the decision log.
