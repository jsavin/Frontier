# Paige → Portable WPText Milestone

Status
- State: Completed
- Phase: Carbon Migration / Runtime Modernization
- Last Updated: 2025-11-20
- Notes: Summarizes everything shipped since the 2025-11-11 report, culminating in Paige-free wptexts and a successful v6→v7 migration run.

## Runtime & Storage
- Replaced Paige runtime calls with the new C-based extractor (`portable/paige_text_extractor.{c,h}`) that reads packed blobs, reconstructs style/paragraph/font data, and streams real RTF.
- Updated `portable/wptext_runtime.c` so `wp_portable_extract_plaintext`, `wpverbpack`, and `wpverbpacktotext` operate on the portable `WPRT` header/payload exclusively—Paige is now conversion-only.
- Added caching and packing helpers that wrap RTF payloads with the portable disk header, ensuring all newly saved wptexts are canonical `WPRT` externals.
- Materialized wptexts during table loads (`langhash_prepare_wordprocessor_value`, `langhash_materialize_external`) so migrator/CLI callers see UTF-8 strings immediately.

## Fixtures, Tools & Tests
- Captured genuine Paige blobs (`hello_macroman`, `examples_testText`) plus a binary-to-RTF dumper (`tools/wptext_dump_rtf`) for manual validation.
- Expanded `tests/components/paige_text_tests` with regression coverage for plaintext extraction, RTF emission, and style assertions (bold/italic runs stay accurate).
- Extended `tests/runtime_tests` with the `wptext RTF smoke test` and ran `FRONTIER_REGEN_ROOT=databases/Frontier.root ./tests/runtime_tests` to completion—latest log saved at `/tmp/runtime_tests.log`.

## Planning & Documentation
- Marked Phase 1/Phase 2 of `planning/phase3/paige_text_extractor.md` as completed and recorded the Phase 3 (font tables) plan as `Planned (P1)`.
- Updated `_CURRENT_STATUS.md` with the extractor milestone, portable header validation, and migrator success so future sessions can resume quickly.
- Documented the handling of loadfromhandle EOF reads, cleaned up serializer warnings, and noted where logs are stored for future triage.

## Next Focus
- Wire upcoming system-verb validation (starting with `clock.now()`) on top of the WPRT-backed runtime.
- Continue trimming Carbon headers per the `planning/phase3/carbon_migration/` plan and keep planning docs synchronized with each milestone.
