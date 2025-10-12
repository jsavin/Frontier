# Phase Gates

Status
- State: Draft
- Phase: All
- Last Updated: 2025-09-29
- Notes: Exit criteria per phase to align delivery.

Related Docs
- planning/INDEX.md
- planning/Frontier_Refactoring_Plan.md

Change Log
- 2025-09-29: Initialized template sections (Status/Related Docs/Change Log).

Phase 1 — Headless/CLI
- Build: Headless CLI builds without AppKit/Carbon/Win32 linkages.
- Runtime: CLI executes scripts and DB ops in headless mode.
- Tests: Core/runtime tests pass on CI (headless), db_format tests green.
- Docs: CLI usage and test README up to date.

Phase 2 — UI Abstraction
- Boundary: Core compiles/links with no UI framework reference.
- Interface: UIServices v0 implemented (headless + one native adapter).
- Migration: High-traffic `#if FRONTIER_HEADLESS` sites replaced with interface calls.
- Build: `frontier-cli` links core + headless adapter only.
- Tests: Headless parity validated; UI shell smoke test passes.

Phase 3 — Hash Tables
- Design: Modern hash structure and migration plan approved.
- Migration: Online upgrade path from legacy tables with rollback.
- Performance: Demonstrated improvement on large tables under tests.
- Compatibility: Backward compatibility mode supported where needed.
