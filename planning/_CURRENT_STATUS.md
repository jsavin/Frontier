# Carbon Migration – Current Status

Status
- State: In Progress
- Phase: Carbon Migration / Runtime Modernization
- Last Updated: 2025-11-28 (Midday)
- Owner: Codex
- Notes: Primary hand-off summary for active work only.
- 2025-11-28 (Codex): Migration runs now complete with canonical v7 headers: `version=7`, `headerLength=88`, view0 set to the new root, other views zeroed, Cancoon dropped. Free-block externals are skipped during packing so the packer no longer dies on stale addresses. Header logging is gated by `FRONTIER_DB_TRACE_HEADERS`. Added runtime header regression (migration path) and unit test for canonical header size/version.
- 2025-11-27 (Codex): Seeded legacy packer fork (`Common/source/legacy/*_legacy.c`) and adjusted modern `tablepack.c` to default BE64. Added thread-local format mode stack (`db_format_mode_push/pop`) and removed `use_64bit_format` flips across packers/tests/stubs; all tests rebuilt and pass (`db_format_tests`, `runtime_tests`, `cli_runtime_tests`). BE64 view0 serialization now covered by unit test. Outstanding warnings trimmed to none in tests (runtime retains known logs only).
- 2025-11-26 (Codex): Reader/writer split into `db_reader_legacy.c`, `db_reader_modern.c`, `db_writer_modern.c`; `make -C tests db_format_tests` passes with the longstanding `__builtin_return_address` warning and an unused helper in `db.c`.
- 2025-11-26 (Codex): Detailed split plan lives at `planning/phase3/modern_reader_writer_split.md`.
- 2025-11-26 (Codex): Payload widening round-trip is **critical**: plan updated to add synthetic legacy→modern→modern-read equality tests, file-level migrated root validation, and BE64 payload widening (tables/records/externals) before claiming the split complete. See `planning/phase3/modern_reader_writer_split.md`.
- 2025-11-26 (Codex): Design principle: keep modern BE64 code branch-free—fork legacy vs modern logic into separate functions/files instead of runtime format branches.
- 2025-11-26 (Codex): Naming decision: modern packers keep canonical names; legacy packers move to `legacy_*/` dirs with `_legacy` entry points so modern remains the default surface.

**Branches in flight**: `feature/carbon-migration-plan`, `feature/headless-system-bootstrap`, `feature/legacy_adapter_widening_and_v7_reader`

## Open Items
- Payload widening/round-trip still pending: need synthetic legacy→modern→modern-read equality tests and file-level validation on migrated roots (tables/records/externals), plus strict modern reader routing for v7 opens.
- Remaining warnings: none in tests; runtime still logs headless traces. (Keep an eye on any new warnings after payload widening work.)
- Re-run migrator/CLI smoke on additional legacy roots once payload widening lands; stash logs.

## Next Steps
- Build payload widening + round-trip tests: synthetic legacy payloads repacked via modern writer, then decoded via modern reader; assert logical equality and BE64-only encodings.
- Route runtime/CLI v7 opens through the strict modern reader once payload widening is ready; rerun `make -C tests runtime_tests` and `make -C tests cli_runtime_tests`.
- Add byte-level regressions for adapter-widened payloads (table/record/externals) to guard the new split.
- Re-run `FRONTIER_REGEN_ROOT=… ./tests/runtime_tests` on additional legacy roots after widening; stash logs under `/tmp` with timestamps.
