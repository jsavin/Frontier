# Carbon Migration – Current Status

Status
- State: In Progress
- Phase: Carbon Migration / Runtime Modernization
- Last Updated: 2025-11-20 (Evening)
- Owner: Codex
- Notes: Primary hand-off summary; update whenever major milestones land.

**Last Updated**: November 23, 2025  \
**Branches in flight**: `feature/carbon-migration-plan`, `feature/headless-system-bootstrap`

- Headless builds no longer include or link any Paige headers: `portable/wptext_portable.{c,h}` collapsed to no-op bootstrap stubs, and the entire `portable/wptext_runtime.c` path now uses the new `paige_text_extractor` + UTF‑8⇄RTF helpers for packed blobs.
- `wp_portable_state_pack_portable` now caches real RTF payloads derived from the extractor (or copies RTF payloads for existing `WPRT` blobs) and wraps them with the portable header without ever touching `pg*` APIs. The same helper feeds `wpverbpack` so v7 saves always emit portable `WPRT` handles.
- `wp_portable_extract_plaintext` decodes the new RTF payloads back into UTF‑8 handles (with a logged fallback if an unexpected control word shows up). `langhash_prepare_wordprocessor_value` and `wpverbpacktotext` now consume the Paige-free parser exclusively, and `langhash_materialize_external` converts `wptext` externals during table loads so migrator/CLI callers see plain strings immediately.
- `paige_text_extractor` now resizes handles through the runtime memory API (`gethandlesize/sethandlesize`) and guards against overflow, so chunk concatenation works even when we run against OS-managed handles. `tests/paige_text_tests` covers the real Paige fixtures (`hello_macroman`, `examples_testText`) with the new logic.
- Phase 2 RTF emission is live: the extractor now captures Paige style/paragraph/font metadata, `wptext_emit_rtf_from_paige_blob` streams proper RTF (fonts, inline styling) without Paige, and `wp_portable_state_cache_rtf` uses it to populate `WPRT` handles. Added a CLI helper (`tools/wptext_dump_rtf.c`) plus a regression test that verifies the emitted RTF for the `hello_macroman` fixture.
- `FRONTIER_REGEN_ROOT=databases/Frontier-v6.root ./tests/runtime_tests` now completes end-to-end; latest log lives at `/tmp/runtime_migrate_run2.log` and produced `databases/Frontier-v6-v7.root` without tripping the Paige extractor fallback path.
- Table serialization now writes a v4 header with the reserved 1 KB padding. `hashunpacktable` skips the reserved block when reading, so both 32‑bit and 64‑bit tables now satisfy the new `table_header_regression` checks.
- `wp_portable_diskheader`/`wp_portable_header` switched to fixed-width integer fields, which fixed the `utf8bytelen` bookkeeping and unblocked all WP text smoke tests. Added `wp_portable_load_portable_blob_for_test` so the runtime regression harness can validate `WPRT` blobs directly.
- Regression coverage: `tests/paige_text_tests` and `tests/runtime_tests` both pass locally (see `/tmp/runtime_tests.log` for the latest run). The migrator now succeeds on the canonical `Frontier-v6.root` sample.
- Serializer round-trip logging tightened: `hashunpacktable` now checks remaining bytes before calling `loadfromhandle`, so the expected end-of-records path no longer emits `[headless] loadfromhandle fail` noise. Re-ran the full migrator afterward; fresh logs live at `/tmp/runtime_tests.log`.
- Database portability gap: v7 headers now write with explicit big-endian helpers. `dbflushheader` serializes `tydatabaserecord_64` via `db_format_write_header64` (runtime-only fields zeroed), `tableverbpack/tableverbunpack` emit/consume big-endian dbaddresses regardless of host endianness, and block headers/trailers now store 64-bit big-endian sizes/links (avail nodes write/read BE64 links). `make -C tests db_format_tests -B` and `make -C tests runtime_tests`/`cli_runtime_tests` pass locally (warnings only). Follow-ups (tracked in `planning/big_endian_portability_audit.md`):
  1. Extend table/record packers to emit big-endian lengths/addresses (the decoder already does this for legacy payloads). **Table dbaddress packing is fixed; double-check any remaining record-length writers that still rely on `memtodisklong`.**
  2. Add regression tests that open a root written on one architecture and verify the header/record bytes match a golden big-endian reference (include >4 GB free-span simulation/sparse file and clean up artifacts on success). In-memory >4 GB free-span encode/decode added to `tests/db_format_tests`.
  3. Update docs (`docs/database_architecture.md`, `planning/TODO_future_improvements.md`) once the on-disk format is guaranteed portable.
- CLI regression: `tests/cli_runtime_tests` currently skips the `clock.now()` check because `frontier-cli --system-root databases/Frontier-v6-v7.root` fails to load v7 databases (same endianness issue above). Once the header writes are fixed, re-run the CLI tests so the `clock.now()` path becomes a real regression instead of a skip.

### Migrator status – `Frontier-v6.root`
- Repro: `FRONTIER_REGEN_ROOT=databases/Frontier-v6.root ./tests/runtime_tests`
- Outcome: Success as of Nov 20. See `/tmp/runtime_migrate_run2.log` for the full transcript; the run emitted `databases/Frontier-v6-v7.root` with every wptext external converted through `paige_text_extractor`.
- Follow-ups:
  * Keep `/tmp/runtime_migrate.log` around for comparison (that log still shows the pre-fix crash for reference).
  * Spot-check the generated `Frontier-v6-v7.root` in follow-on tests once the v7 reader path is wired up.

## Medium-Term Goals
- Finish the wptext plain-text conversion so every `langhash_prepare_wordprocessor_value` either returns UTF-8 text or a clearly logged placeholder, then ensure the migrator packs those values without leaning on Paige.
- Confirm runtime parity between the headless bootstrap and UI routes (e.g., `system.verbs → kernelcall → EFP`) once wptext and doc-info no longer destabilize the migrator.
- Keep the automation/IPC boundary documented so headless builds can safely expose JSON-RPC while UI builds retain OSA, as tracked in `planning/TODO_future_improvements.md`.

## Long-Term Goals
- Finish the Phase 2 runtime context refactor so simultaneous CLI/headless clients share `FrontierContext` backend handles without touching globals.
- Land the Phase 3 concurrency/task-context plan, widen paging/tracing to multi-threaded guard-malloc tests, and keep `planning/TODO_future_improvements.md` aligned with heading priorities.
- Expand developer tooling (OSS compliance, doc server, LSP work) so future IDE/bridge projects and release automation can rely on the documented TODO backlog.

## Next Steps
- Wire the v7 reader/materializer to consume `WPRT` externals directly, so downstream tooling can hydrate RTF payloads without any Paige hooks (document behavior in `docs/database_architecture.md` once done).
- Audit additional legacy roots (e.g., customer saves) with `FRONTIER_REGEN_ROOT=… ./tests/runtime_tests` and stash each log under `/tmp` with a timestamp so regressions stand out quickly.
- Keep expanding fixture coverage for wptext parsing—every time we dump a new Paige blob, add it to `tests/fixtures/wptext/` and update `tests/components/paige_text_tests.c`.
- Continue running `make -C tests runtime_tests` + the migrator smoke after each change, attaching the latest log paths here so the next session can pick up immediately.
- Re-run the full CLI/runtime suites once the big-endian header changes land in the reader path to confirm `clock.now()` and v7 opens behave the same on arm64/x86.
- New plan doc: `planning/big_endian_portability_audit.md` tracks the remaining BE audit (avail list serialization, record metadata, reader parity, cross-arch regression, docs).
