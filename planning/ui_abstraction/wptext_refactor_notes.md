# WPText Refactor Notes

Status
- State: Planned
- Phase: 3+ (post UI separation)
- Last Updated: 2025-09-29
- Notes: Rich text modernizations align with UI modernization phases.

Related Docs
- planning/ui_abstraction/PHASES.md
- planning/legacy_glossary.md

Change Log
- 2025-09-29: Initialized template sections (Status/Related Docs/Change Log).

## Historical constraints

- **32 KB size limit:** WPText objects grew out of the Classic Mac OS word processing widgetry, which carried a hard 32 KB ceiling due to Toolbox data structures that mirrored Pascal string limits. The runtime still assumes that ceiling.
- **Platform character sets:** The code path remains rooted in the old Mac/Windows encodings rather than a Unicode-aware text model. Rich text authored in the IDE therefore depends on the legacy character maps.

## Future work reminders

- Remove the 32 KB ceiling during the WPText refactor so modern documents are not silently truncated.
- Transition WPText storage and editing to UTF-8/Unicode, retiring the historical per-platform encodings.
- Revisit how the word-processor window is surfaced in headless builds vs. UI shells, ensuring the runtime’s rich-text data operations remain usable without a visible window.

Additional considerations (migrated from “Future Considerations”):
- Add CLI tooling for WPText conversion/validation post Phase 2.
- Provide format migration utilities with data integrity checks and rollbacks.
