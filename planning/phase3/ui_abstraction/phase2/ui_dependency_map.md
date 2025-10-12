# UI Dependency Map → UIServices Mapping

Status
- State: Draft
- Phase: 2
- Last Updated: 2025-09-29
- Notes: High-level map from legacy UI subsystems to boundary endpoints.

Related Docs
- planning/ui_abstraction/phase2/architecture.md
- planning/ui_abstraction/phase2/migration_plan.md
- planning/ui_abstraction/phase2/carbon_dependency_audit.md

Change Log
- 2025-09-29: Initial draft.

Subsystems → Proposed UIServices
- Event Loop / Yield
  - `yield()`, `scheduleTimer(ms)`
- Messaging / Alerts
  - `logInfo(msg)`, `logError(msg)`, `alert(title, message)`
- File Dialogs
  - `fileOpen(outPath, len)`, `fileSave(outPath, len)`
- Menus
  - `menuEnable(menuId, itemId, enable)`
- Clipboard
  - `clipboardSetText(utf8, len)`, `clipboardGetText(out, len)`
- Automation (AppleEvents)
  - `automationSend(req, reqLen, resp, respLen)`
- Text Editing Hooks (Future)
  - Minimal callbacks for text selection, if needed by scripts

Legacy Areas to Migrate
- Windowing / Outlines
  - Replace direct calls with neutral operations via `UIServices` or move entirely into adapters.
- Menus & Dock
  - Translate enable/disable/query to `menuEnable` or adapter-resident logic.
- AppleEvents / OSA
  - Bridge calls via `automationSend` (UI adapter). Headless: explicit errors.

Notes
- Keep core ignorant of platform types like `WindowPtr`/`HWND`/AppKit classes.
- Expand `UIServices` only when a concrete core use case needs it.

