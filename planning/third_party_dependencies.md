# Third‑Party Dependencies

Status
- State: Draft
- Phase: Cross-Cutting
- Last Updated: 2025-09-29
- Notes: Inventory of bundled or external libs and plans.

Related Docs
- planning/0.5.23_runtime_test_plan.md
- tests/README.md

Dependencies
- PCRE (Common/PCRE): Legacy regex engine port.
  - Status: Used by `langregexp`. Headless builds include PCRE; consider modern replacement in future.
- SQLite/MySQL snapshots (Common/MySQL, Common/sqlite3): Historical bundles.
  - Status: Retained for compatibility. Avoid linking in headless tests unless required.

Planned Actions
- Evaluate replacing PCRE with a modern, portable regex (or system regex where available).
- Audit DB client components for necessity; prefer decoupling from core tests.

References
- tests/README.md
- planning/0.5.23_runtime_test_plan.md

Change Log
- 2025-09-29: Initialized template sections (Status/Related Docs/Change Log).
