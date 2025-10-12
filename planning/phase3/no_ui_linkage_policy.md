# No‑UI Linkage Policy (Headless Builds)

Status
- State: Draft
- Phase: 1–2
- Last Updated: 2025-09-29
- Notes: Prevents accidental linkage to UI frameworks in headless targets.

Related Docs
- planning/INDEX.md
- planning/ui_abstraction/phase2/architecture.md
- planning/ui_abstraction/phase2/migration_plan.md

Change Log
- 2025-09-29: Initialized template sections (Status/Related Docs/Change Log).

Policy
- Headless targets (tests, CLI) must not link against AppKit/Carbon/Win32 UI libs.
- Any required symbols from UI modules must be satisfied by headless adapters/stubs.

Enforcement (Future CI)
- Build `libfrontier_core` with `-Wl,--no-undefined` (or platform equivalent); fail on UI symbols.
- Scan linked binaries for forbidden symbols/frameworks (e.g., `otool -L` / `ldd`).
- Grep core sources for platform UI headers in CI and fail on detection.

Remediation
- Replace platform includes with calls to the boundary interface (planned UIServices).
- Move conditional compilation into adapters.

References
- planning/ui_abstraction/phase2/migration_plan.md
- planning/phase_gates.md
