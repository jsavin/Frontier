# Carbon Migration Phases

Status
- State: In Progress
- Phase: Carbon Migration
- Last Updated: 2025-11-20
- Owner: Codex
- Notes: High-level execution order; update as phases complete or shift.

Each phase delivers a reviewable chunk of work that leaves the tree compiling (and tests passing) in the headless configuration. Phases can overlap if needed, but try to land them sequentially so reviews stay focused.

## Phase 0 – Planning (✅)
- Publish this plan (`README.md`, `inventory.md`, `decision_log.md`, `status_log.md`).
- Update `_CURRENT_STATUS.md` and `planning/INDEX.md` to point at the new workstream.

## Phase 1 – Header hygiene
- Ensure `frontier.h` pulls the correct portable headers when `FRONTIER_HEADLESS` is defined.
- Expand `osincludes_portable.h`/`standard_portable.h` until the portable build compiles through preprocessing (no missing typedefs/macros).
- Update existing phase docs with deprecation notices directing readers to this plan.

## Phase 2 – Runtime primitives
- Replace remaining Moveable Handle helpers (`MemError`, `MaxBlock`, direct Handle APIs) with portable equivalents.
- Remove QuickDraw geometry helpers (`recttodiskrect` etc.) or provide platform-neutral implementations.
- Verify `tests/test_migration` links without Carbon stubs.

## Phase 3 – Text encoding & strings
- Remove TEC dependencies by providing UTF-8/UTF-16 converters in portable code.
- Replace `GetString`/Resource Manager calls with table-driven or file-based lookups.
- Ensure `strings.c` compiles without Classic constants.

## Phase 4 – AppleEvent/Alias removal
- Refactor `langhash.c` and friends to avoid AppleEvent descriptor structs in headless mode.
- Decide whether to keep alias support; if yes, provide cross-platform implementation.
- Remove remaining references to `land.h` structs from headless build path.

## Phase 5 – Cleanup & validation
- Sweep for dead headers/includes (drop `frontier_compat.h` once unused).
- Re-run full test matrix (portable + desktop) to ensure no regressions.
- Archive this plan with a final summary in `status_log.md` and `_CURRENT_STATUS.md`.

> We may add more phases as we discover new clusters of Carbon code. When that happens, update this file and record the decision in `decision_log.md`.
