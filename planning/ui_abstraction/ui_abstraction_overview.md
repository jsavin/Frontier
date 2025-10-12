# UI Abstraction Overview

Status
- State: In Progress
- Phase: 2
- Last Updated: 2025-09-29
- Notes: Targets headless-first runtime and adapter-based UI separation.

Related Docs
- planning/ui_abstraction/PHASES.md
- planning/ui_abstraction/phase2/analysis.md
- planning/ui_abstraction/phase2/architecture.md
- planning/ui_abstraction/phase2/migration_plan.md
- planning/ui_abstraction/phase2/patterns_and_choices.md

Change Log
- 2025-09-29: Initialized template sections (Status/Related Docs/Change Log).

## Motivation

Frontier’s runtime is tightly coupled to Carbon/QuickDraw windowing code. To
ship a modern headless core and future cross-platform UI, we need a clean
interface between the runtime and any user interface.

## Objectives

1. **Define a shell API** that captures all UI interactions the runtime needs
   (window lifecycle, menu events, alerts, callbacks).
2. **Implement platform-specific shells**: macOS (Carbon replacement), Windows
   (existing code), and a headless/no-op shell for server mode and automated
   tests.
3. **Ensure runtime modules only depend on the shell API**, not directly on
   Carbon/QuickDraw headers.
4. **Document the interface** so new clients (e.g., a remote REST UI) can be
   built against it.

## Deliverables

- `shell_api.h` (proposed interface)
- Headless shell implementation for tests/server mode
- Updated macOS shell implementation behind the new API
- Migration notes for each runtime module refactored

## Risks & Considerations

- UserTalk callbacks and window scripts currently assume in-process UI access;
  we need a strategy for forwarding/bridging those in remote scenarios.
- Existing Windows/macOS shells may diverge slightly; the abstraction must cover
  both variants without regressions.
- Ensure headless tests continue to work as we migrate modules.

## Next Steps

1. Inventory all runtime-to-UI entry points (starting with shellwindow.*,
   osamenus.*, osacomponent.*, etc.).
2. Draft a minimal `shell_api.h` and circulate for review.
3. Implement the headless shell module declaring the same symbols.
4. Incrementally refactor Carbon files to comply with the new interface.
5. Update documentation and tests after each milestone.


## Guiding Tenets

- **Headless-first runtime:** Any code path that requires a UI must route through
  the shell API. Headless builds provide a no-op shell that raises `ScriptError`
  for unsupported interactions rather than blocking.
- **Shared data model:** Stack frames and debugger state remain transient tables
  so the headless runtime and any client (IDE or remote REST UI) share the same
  primitives for inspection/editing.
- **Client-agnostic UI:** The shell interface must be implementable by both
  in-process GUIs (macOS/Windows) and out-of-process clients (REST/WebSocket).
- **Fail fast:** A script that invokes a UI-only verb in headless mode should
  fail immediately, making it obvious where UI-specific code still leaks into
  server deployments.

- **Preserve Frontier's global runtime model:** The goal is to encapsulate UI
  interactions without dismantling dot-addressing, stack-table exposure, or the
  global data semantics that made Frontier unique. Headless mode should behave
  exactly like the historical server deployments, with UI requests routed
  through an explicit shell layer instead of being implicitly available.
