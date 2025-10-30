# Carbon Migration – Current Status

**Last Updated**: October 30, 2025  \
**Branches in flight**: `feature/carbon-migration-plan`, `feature/headless-system-bootstrap`

## Updated Plan — October 29, 2025

We pivoted from incremental shims to a comprehensive Carbon-dependency retirement. The canonical plan now lives under [`planning/carbon_migration/`](carbon_migration/README.md).

1. **Planning skeleton (done)**: Created the Carbon migration directory with README, inventory, phases, decision log, and status log.
2. **Doc refresh (done)**: Updated the planning index/README, restored in-progress phase docs (with status tracking such as in `langhash_portable_missing_types.md`), and pointed the archive only at completed notes.
3. **Execution phases**: See [`carbon_migration/phases.md`](carbon_migration/phases.md) for subsystem milestones (header hygiene → runtime primitives → encoding → AppleEvents → cleanup).

## Longer-Term Goal
Run Frontier without any Classic Mac / Carbon APIs while keeping the headless and desktop builds unified. Completing the Carbon plan is now the primary Phase 3 objective.

## Quick Status — October 30, 2025
- Completed a platform legacy audit covering memory manager hooks, STR#/resource forks, filespec/alias usage, QuickDraw helpers, AppleEvents, and header duplication. See [`platform_legacy_audit.md`](carbon_migration/platform_legacy_audit.md).
- First pass at compiling core modules headless confirmed the audit findings: `memory.c` still depends on `MemError`/`MaxBlock`, `strings.c` references QuickDraw geometry, and `land.h` drags in AppleEvents.
- Portable headers (`standard_portable.h`, `osincludes_portable.h`) still need guard cleanup (no more duplicate typedefs) before the refactor branches land.

## Progress Snapshot
- ✅ Added stdio-backed file layer shared by headless tests/CLI (eliminated legacy file stubs).
- ✅ Seeded Carbon migration plan, inventory, and decision log.
- 🔄 Working on header hygiene: ensuring `frontier.h` and related headers bring in the correct portable definitions.

## Immediate Next Steps
1. **Document call-site maps** (Assistant): Using the audit as a checklist, generate per-category call-site lists (`getstringlist`, `filespectoalias`, QuickDraw helpers, AppleEvent entry points) to scope each refactor (see “Detailed Task Breakdown” in the audit for sub-steps).
2. **Sequence refactor PRs** (Assistant): Draft the order of themed branches (memory layer → strings/resources → filespec/alias → QuickDraw/UI split → AppleEvent isolation → header cleanup) and update `status_log.md` accordingly, following the sub-tasks captured in the audit.
3. **Windows parity review** (Assistant): Study the Win32 project (`shell.win.h`, `frontierwindows.c`, `langwinipc.c`) for existing solutions we can adopt when replacing classic Mac APIs.
4. **Prepare resource fork replacement plan** (Assistant): Decide on the new string/error table format so subsequent branches can drop STR# usage confidently.
5. **Portable header guard cleanup** (Assistant): Introduce feature macros so `osincludes_portable.h` and `headless_stubs.h` no longer duplicate typedefs, paving the way for the refactor branches.

Progress and blockers should continue to be logged in the Carbon migration status log and decisions documented in the decision log.
