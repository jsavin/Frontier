# Patterns and Choices — MVC, MVVM, Hexagonal

Status
- State: Draft
- Phase: 2
- Last Updated: 2025-09-29
- Notes: UI-internal patterns; core boundary remains hexagonal.

Related Docs
- planning/ui_abstraction/phase2/analysis.md
- planning/ui_abstraction/phase2/architecture.md

Change Log
- 2025-09-29: Initialized template sections (Status/Related Docs/Change Log).

Separation of Concerns
- Use a hexagonal (ports-and-adapters) boundary between the runtime (core) and any UI shell.
- Inside each adapter, use the platform-appropriate presentation pattern:
  - macOS AppKit: MVC or MVP; for SwiftUI shells, MVVM.
  - Windows: MVP or MVC over Win32/.NET wrappers.
  - Web: MVU/Redux-style in the frontend; the adapter is transport.

Guidance
- Do not expose presentation patterns to the core; the core knows only `UIServices`.
- Keep the `UIServices` surface minimal and stable; presentation patterns should adapt to it, not shape it.
- Prefer message/command style interfaces (log, alert, menuEnable, automationSend) to avoid leaking UI object lifecycles into core.

Longer-Term Structure
- Multiple sub-projects are expected under `ui_adapters/` for native, headless, and web shells.
- Keep build artifacts separate to avoid accidental UI linkage in headless/core builds.
