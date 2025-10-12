# Headless Stubbed Behavior Matrix

Status
- State: Draft
- Phase: 1–2
- Last Updated: 2025-09-29
- Notes: Summarizes behavior of UI-dependent features under headless.

Related Docs
- planning/DEVELOPER_QUICKSTART_HEADLESS.md
- planning/no_ui_linkage_policy.md
- planning/ui_abstraction/phase2/architecture.md

Legend
- ok = behaves normally
- noop = silently does nothing
- error = predictable error via runtime error reporting
- partial = subset of behavior available

Areas
- AppleEvents/Automation: error
  - Rationale: AE runtime not present; see Common/headers/headless_stubs.h
- Dialogs/Alerts: partial/noop
  - Rationale: Provide logs/return codes in headless; no UI display
- Menus/Menu Enabling: noop
  - Rationale: No menubar in headless; tests stub menu APIs
- Window/Outline Editing: error/noop
  - Rationale: Requires UI; headless stubs present in tests/headless_*.c
- Clipboard: noop/partial
  - Rationale: Optional; can map to in-memory stub if needed
- QuickTime Player: error
  - Rationale: Feature unsupported in headless
- Regex (legacy PCRE path): ok (with PCRE) / partial
  - Rationale: PCRE present; consider replacement later

Known Stubs (non-exhaustive)
- tests/headless_shell.c — shell UI callbacks
- tests/headless_menu_stubs.c — menu APIs
- tests/headless_op_stubs.c — outline/editor hooks
- tests/headless_wp_stubs.c — rich text selection stubs
- tests/headless_langipc_stub.c — IPC/automation
- Common/headers/headless_stubs.h — Carbon-era types/APIs for headless

Notes
- Prefer predictable errors to silent no-ops for script-visible verbs.
- This matrix will evolve as Phase 2 consolidates behavior behind UIServices.

Change Log
- 2025-09-29: Initialized template sections (Status/Related Docs/Change Log).
