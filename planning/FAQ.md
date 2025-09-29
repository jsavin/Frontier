# Frontier Refactoring — FAQ

Status
- State: In Progress
- Phase: Cross-Cutting
- Last Updated: 2025-09-29
- Notes: Common questions about headless mode, UI boundary, and docs.

Related Docs
- planning/INDEX.md
- planning/ui_abstraction/PHASES.md
- planning/ui_abstraction/phase2/analysis.md
- planning/DEVELOPER_QUICKSTART_HEADLESS.md

Change Log
- 2025-09-29: Initial version.

Q: Why not keep using `#if FRONTIER_HEADLESS` and stubs?
- A: It doesn’t scale. Conditionals sprawl, duplicate logic, and cause divergence. Ports-and-adapters with a `UIServices` boundary centralizes behavior and keeps the core UI-agnostic.

Q: What does “ports-and-adapters” mean here?
- A: The core depends on an abstract `UIServices` interface (“port”). Implementations (“adapters”) provide headless, AppKit, Win32, or web behavior without changing the core.

Q: What works in headless mode?
- A: Language and database operations, and any verbs that don’t require UI. UI-only verbs either return a predictable error or no-op. See planning/headless_stubbed_behavior_matrix.md for details.

Q: Where should new UI-related docs go?
- A: Under `planning/ui_abstraction/phase2/` for separation work. Longer-term UI modernization (rich text, adapters) fits in Phase 3 docs.

Q: How do I keep docs healthy after moves?
- A: Run `python3 scripts/check_doc_links.py` to validate local links and anchors. Update references and add a Change Log entry.

Q: Which docs are authoritative for current work?
- A: Start with `planning/INDEX.md`. Phase 2 details live under `planning/ui_abstraction/phase2/`.

Q: How do I decide the Status value?
- A: Use Draft/In Progress/Completed/Deprecated/Archived. Phase 0 docs are generally Completed or historical; Phase 2 docs are In Progress or Draft.

