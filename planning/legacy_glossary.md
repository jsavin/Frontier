# Legacy Concepts Glossary

Status
- State: Draft
- Phase: Cross-Cutting
- Last Updated: 2025-09-29
- Notes: Map legacy terms/APIs to modern equivalents and status.

Related Docs
- planning/ui_abstraction/phase2/langsystem7_headless_refactor.md
- planning/ui_abstraction/ui_abstraction_overview.md

Change Log
- 2025-09-29: Initialized template sections (Status/Related Docs/Change Log).

Entries
- Handle (Mac OS): Movable memory handle; retained semantics via portable runtime. Status: Supported (portable handles).
- FSSpec/Aliases: Classic file spec and alias manager. Status: UI/macOS path only; headless plans to modernize (see langsystem7 plan).
- AppleEvents/OSA: Classic automation IPC. Status: UI/macOS only; headless disabled.
- Gestalt: OS capability query. Status: Replace with modern platform queries per OS.
- Paige (text): Legacy text engine. Status: UI-only; evaluate replacements (Core Text/DirectWrite) in future.
- WPText: Rich text object with legacy limits. Status: Planned modernization post UI separation.

References
- planning/0.5.26_langsystem7_headless_refactor.md
- planning/ui_abstraction/ui_abstraction_overview.md
