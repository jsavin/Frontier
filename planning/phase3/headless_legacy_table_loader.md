# Headless Legacy Table Loader

Status
- State: In Progress
- Phase: 3 (Headless Runtime)
- Last Updated: 2025-10-30
- Notes: Focused on decoding legacy external payloads during the v6→v7 migration so headless builds hydrate real data.

Related Docs
- `planning/phase3/pascal_runtime_modernization.md`
- `planning/phase3/langhash_portable_missing_types.md`
- `docs/legacy_frontier_bootstrap.md`

Change Log
- 2025-10-30: Restored from archive and reformatted; added outstanding external-type work.
- 2025-10-22: Documented current loader status and script storage decision.

Overview
- The headless CLI now unpacks v7 roots, but legacy v6 payloads still rely on Pascal-era layouts (string pools + record arrays).
- This plan tracks the remaining conversion work needed so external value types (scripts, menus, outlines, etc.) can round-trip cleanly after migration.

Current Progress
- ✅ Header migration lands root `views[0]` at the correct block in `Frontier-v6.root7`.
- ✅ `tableexternal_common.c` detects the legacy `[header][strings][records]` layout and converts tables to the modern merged representation.
- ✅ The migrated system table hydrates, allowing UserTalk scripts to run after load.
- ✅ Format notes captured in `docs/legacy_frontier_bootstrap.md`.

Outstanding External Types
- `scriptvaluetype`: metadata + outline converted; compiled code intentionally dropped (v7 recompiles on demand).
- `outlinevaluetype`: hierarchical outline format still needs byte-level documentation.
- `wordvaluetype` (wptext): classic Mac rich text buffer; confirm encoding expectations and size limits.
- `menuvaluetype`: menu definitions with resource-style handles.
- `pictvaluetype`: QuickDraw PICT payloads.
- `listvaluetype` / `recordvaluetype`: recursive collections that may contain any other type (including additional externals).

Details
- For each type, document the on-disk representation, provide a converter that emits the modern merged format, and add regression tests using real v6 fixtures.
- Complex containers (lists/records) require recursive traversal and may reintroduce Pascal strings (`Str255`) inside nested structures.

Decisions
- v7 no longer stores compiled script code; source + outline are canonical and recompilation happens on first execution.

Open Questions
- Which external types actually ship in `Frontier.root` today? Finish the database survey to prioritize conversion work.
- Should converters live only in the headless migrator, or be factored into a shared serialization library for both desktop and CLI builds?
- Do we normalize legacy payloads during migration, or leave them as-is and convert lazily at load time?

Next Steps
- Enumerate every external type present in the sample v6 roots and update this document with status per type.
- Reverse-engineer the remaining layouts (menus, outlines, wptext, pict) and capture documentation in both this plan and the code comments.
- Implement conversion helpers with unit tests and integrate them into the migrator/headless loader.
- Add integration tests that hydrate v6-derived roots and validate representative data (e.g., scripts, menus, rich text) end-to-end.
