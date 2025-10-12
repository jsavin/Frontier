# Third‑Party Dependencies

Status
- State: In Progress
- Phase: Cross-Cutting
- Last Updated: 2025-10-12
- Notes: Inventory of bundled or external libs to track as we modernise the runtime and tests.

Related Docs
- planning/phase3/0.5.23_runtime_test_plan.md
- tests/README.md
- planning/phase_overview.md

Dependencies
- **PCRE** (`Common/PCRE`): legacy regex engine used by `langregexp`.
  - Status: still required; evaluate modern replacements or platform regex once headless parity is stable.
- **SQLite/MySQL snapshots** (`Common/sqlite3`, `Common/MySQL`): historical client bundles.
  - Status: retained for compatibility; avoid linking into headless builds unless a test explicitly exercises DB clients.
- **Paige / QuickDraw resources** (indirect dependency for rich text).
  - Status: targeted for Phase 4 string/text modernization; keep notes here as interfaces change.

Planned Actions
- Document precise versions and licence constraints for each bundled dependency.
- Audit DB client usage and identify opportunities to decouple optional transports in Phase 3/4.
- Track blockers that would force migration (e.g., unsupported toolchains, security advisories).

Change Log
- 2025-10-12: Updated references and broadened dependency notes.
- 2025-09-29: Initial draft.
