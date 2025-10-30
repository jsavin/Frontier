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
- First pass at compiling `langhash.c` headless revealed deeper Classic Mac ties: `memory.c` still depends on `MemError`/`MaxBlock`, `strings.c` drags in QuickDraw helpers, and `land.h` insists on AppleEvent symbols.
- Portable headers (`standard_portable.h`, `osincludes_portable.h`) now have the minimal Pascal-string helpers `langhash.c` needs, but we’re rolling those edits back until the wider refactor lands.
- Planning/doc structure is up to date; in-progress phase docs stay under `planning/phase*/`, and the archive now holds completed material only.

## Progress Snapshot
- ✅ Added stdio-backed file layer shared by headless tests/CLI (eliminated legacy file stubs).
- ✅ Seeded Carbon migration plan, inventory, and decision log.
- 🔄 Working on header hygiene: ensuring `frontier.h` and related headers bring in the correct portable definitions.

## Immediate Next Steps
1. **Study Windows port** (Assistant): Catalogue how the Win32 build replaces `MemError`/`MaxBlock`, keeps `strings.c` free of QuickDraw helpers, and fences off AppleEvents so we can mirror that structure.
2. **Memory layer refactor** (Assistant): Move `memory.c` onto the portable handle/runtime helpers (no direct `MemError`/`MaxBlock`) and update callers.
3. **Strings refactor** (Assistant): Extract QuickDraw/UI helpers into a desktop-only module so `strings.c` remains OS-neutral for headless builds.
4. **LAND / AppleEvents isolation** (Assistant): Gate `land.h`/`processinternal.h` usage for headless builds, aligning with the Windows approach to AppleEvents.
5. **Portable header cleanup** (Assistant): Trim `osincludes_portable.h` to ANSI/POSIX essentials and relocate legacy Mac structs/macros to desktop-only headers.

Progress and blockers should continue to be logged in the Carbon migration status log and decisions documented in the decision log.
