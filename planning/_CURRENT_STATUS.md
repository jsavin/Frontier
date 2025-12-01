# Carbon Migration – Current Status

Status
- State: In Progress
- Phase: Carbon Migration / Runtime Modernization
- Last Updated: 2025-11-30 (Midday)
- Owner: Codex
- Notes: Primary hand-off summary for active work only.
- 2025-11-30 (Codex): Save As swaps now run through context-aware guards (`dbswapglobals_context`) so allocations/view updates respect scoped Save As state without ambient globals; release-stack push/flush/zero now wrap default contexts; `db_format_tests` remain green.
- 2025-11-30 (Codex): Scoped Save As state into `db_context` (save-as snapshots + guards), refreshed default-context wrappers, and reworked migrator Save As to rely on its context destination handle; `make -C tests db_format_tests` and `./db_format_tests` pass after the refactor.
- 2025-11-29 (Codex): Context sweep for DB/adapter: stack/release helpers wrapped, internal assign/copy helpers exposed, TLS `use_64bit_format` shim removed (mode tracked via `g_mode_state`), and two-context regression added to `db_format_tests`.
- 2025-11-28 (Codex): Added `db_context` pack/assign/ref wrappers and dropped `use_64bit_format` from packers/tests; canonical v7 headers validated with a new header regression.
- 2025-11-27 (Codex): Legacy packers forked to `legacy_*` with modern packers defaulting to BE64; format-mode stack (`db_format_mode_push/pop`) in place and core suites (`db_format_tests`, `runtime_tests`, `cli_runtime_tests`) green.
- 2025-11-26 (Codex): Reader/writer split into legacy vs modern modules; modern headers serialized in BE64 with validation hooks; payload widening + round-trip tests still pending (see plan).
- 2025-11-30 (Codex): Added v6 fixture plan + file (`tests/fixtures/v6/test.root`) under `testData` namespace; migration test added (fails to build `test_migration` target on headless stubs due to legacy QuickDraw/timedate symbols—left as known issue).
- 2025-11-30 (Codex): Started `feature/v6-root-fixture`; drafted `planning/phase3/v6_root_fixture_plan.md` defining a legacy-authored v6 root covering all datatypes for migration regression. Pending: user to author the fixture in the legacy Windows app.

**Branches in flight**: `feature/carbon-migration-plan`, `feature/headless-system-bootstrap`, `feature/legacy_adapter_widening_and_v7_reader`

## Open Items
- Payload widening/round-trip still pending: need synthetic legacy→modern→modern-read equality tests and file-level validation on migrated roots (tables/records/externals), plus strict modern reader routing for v7 opens.
- Remaining warnings: none in tests; runtime still logs headless traces. (Keep an eye on any new warnings after payload widening work.)
- Re-run migrator/CLI smoke on additional legacy roots once payload widening lands; stash logs.

## Next Steps
- Follow `planning/phase3/db_context_completion_plan.md`: migrate Save As swap/free-list/release-stack paths and callers (incl. headless) to explicit `db_context`, then trim legacy wrappers and validate with broader test suites.
- Build payload widening + round-trip tests: synthetic legacy payloads repacked via modern writer, then decoded via modern reader; assert logical equality and BE64-only encodings.
- Route runtime/CLI v7 opens through the strict modern reader once payload widening is ready; rerun `make -C tests runtime_tests` and `make -C tests cli_runtime_tests`.
- Add byte-level regressions for adapter-widened payloads (table/record/externals) to guard the new split.
- Re-run `FRONTIER_REGEN_ROOT=… ./tests/runtime_tests` on additional legacy roots after widening; stash logs under `/tmp` with timestamps.
