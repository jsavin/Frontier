# Post-PR-43 Accomplishments

_Last updated: 2025-11-11_

> 2025-11-11 Codex: Initial summary capturing all work completed after PR #43 landed on origin.

## Planning & Documentation
- Reorganized the entire planning tree: archived the legacy phase folders, introduced `planning/carbon_migration/` (README, inventory, phases, decision/status logs), refreshed `planning/INDEX.md`, and created `_CURRENT_STATUS.md` as the hand-off source of truth.
- Added durable guidance to `AGENTS.md`, new design docs (`kernel_userTalk_bridge.md`, `outline_script_payload.md`, `wptext_format.md`, `wptext_rtf_tracker.md`), and ensured every significant discovery is mirrored in both AGENTS and the current-status doc.
- Captured the Carbon/Paige modernization direction, page-by-page, so future contributors can follow the same roadmap without rediscovery.

## Table/Outline Serializer Work
- Introduced the v4 table header with a 1 KB reserved block plus regression coverage in `tests/save_migration_tests`.
- Rebuilt the headless outline/script serializer to drop QuickDraw window metadata, pad the new reserved region, and keep the migrator aligned with the legacy format expectations.
- Hardened the legacy table converter (split merged handles deterministically, handle 32-bit externals, guard format linking) so migrated v7 roots hydrate without patching.

## Headless Runtime & CLI Fixes
- Instrumented `langsearchpath*`, `langexternal*`, and table loaders so `defined(system.verbs)` and `clock.now()` debugging has full visibility.
- Updated the CLI evaluator to feed inline scripts through `langrun`, render structured results, and keep the search-path stack accurate in headless mode.
- Ensured headless bootstrap stops recreating `system.*` tables by wiring the real tablestructure pointers after migration.

## Paige & WPText Enablement
- Vendorized CMake 3.29.6 and the open-source Paige repo, added a macOS universal build (`libpaige.a`), and created the UNIX platform layer (`PGUNX`) with malloc-backed handles, POSIX file I/O, and no-op graphics/clipboard shims.
- Added `wp_portable_init()` plus accessors for the Paige globals, hooked both CLI and tests to initialize Paige on startup, and removed the legacy `headless_wp_stubs.c`.
- Implemented a Paige-backed headless WP runtime (`portable/wptext_runtime.c`) that hydrates real documents, reads legacy trailers, and mirrors Save/Save As packing semantics so the migrator can repack WPTexts once the new serializer lands.
- Extended the WPText tracker with the detailed RTF conversion plan (portable header spec, exporter/importer, migrator gating, validation steps).

## Test & Tooling Improvements
- Updated `tests/runtime_tests` to build/link with Paige, ensuring the serializer code paths are executed during CI-like runs.
- Captured new fixtures (`scripts/extract_legacy_table.py`, `planning/carbon_migration/data/` seeds) for future regression coverage across table, outline, and WPText payloads.

## Miscellaneous Highlights
- Documented the kernel↔UserTalk “limbic system,” clarified the legacy table bootstrap path, and recorded outstanding gaps (menubar, PICT, WPText) so downstream agents have focused TODOs.
- Added guidance to ignore classic Mac resource forks, Filespec APIs, and other Carbon/QuickDraw dependencies when touching headless/portable modules.
