# Migration Playbook — Moving Code Behind UIServices

Status
- State: Draft
- Phase: 2
- Last Updated: 2025-09-29
- Notes: Checklist for migrating one feature at a time.

Related Docs
- planning/ui_abstraction/phase2/architecture.md
- planning/ui_abstraction/phase2/migration_plan.md

Change Log
- 2025-09-29: Initialized template sections (Status/Related Docs/Change Log).

Checklist
1. Identify the feature and all call sites (use ripgrep and headers).
2. Define minimal `UIServices` functions needed (log/alert/menu/etc.).
3. Implement headless behavior in the headless adapter (documented semantics).
4. Switch core call sites to call `UIServices` instead of UI APIs.
5. Remove/limit `#if FRONTIER_HEADLESS` in core; keep guards in adapters only.
6. Build headless target; verify no UI frameworks linked.
7. Add/adjust tests to cover headless code path.
8. If applicable, implement the native adapter behavior (AppKit/Win32).

Tips
- Keep interface small; add functions only when a real core use case requires it.
- Prefer returning explicit errors over silent no-ops for script-visible verbs.

References
- planning/ui_abstraction/phase2/architecture.md
- planning/ui_abstraction/phase2/migration_plan.md
- planning/no_ui_linkage_policy.md
