# Carbon Migration – Current Status

Status
- State: In Progress
- Phase: Carbon Migration / Runtime Modernization
- Last Updated: 2025-11-27 (Late night)
- Owner: Codex
- Notes: Primary hand-off summary for active work only.
- 2025-11-27 (Codex): Added migrate breadcrumbs; `FRONTIER_REGEN_ROOT=databases/Frontier-v6.root ./tests/runtime_tests` now fails in `tablesavesystemtable` because `hashpackexternal(menu)` hits a free block at adr=0xf709 (Cancoon-drop path). No v7 file emitted; see `/tmp/migrate.log` for the failing run. Next: ensure menu/applescripts/suites externals are materialized or skipped before packing when running under the modern writer.
- 2025-11-27 (Codex): Seeded legacy packer fork (`Common/source/legacy/*_legacy.c`) and adjusted modern `tablepack.c` to default BE64. Added thread-local format mode stack (`db_format_mode_push/pop`) and removed `use_64bit_format` flips across packers/tests/stubs; all tests rebuilt and pass (`db_format_tests`, `runtime_tests`, `cli_runtime_tests`). BE64 view0 serialization now covered by unit test. Outstanding warnings trimmed to none in tests (runtime retains known logs only).
- 2025-11-27 (Codex): Migration drop path zeros all views and points view0 to the new root when Cancoon is dropped; modern header/writer still needs E2E validation on real v7 outputs.
- 2025-11-26 (Codex): Reader/writer split into `db_reader_legacy.c`, `db_reader_modern.c`, `db_writer_modern.c`; `make -C tests db_format_tests` passes with the longstanding `__builtin_return_address` warning and an unused helper in `db.c`.
- 2025-11-26 (Codex): Detailed split plan lives at `planning/phase3/modern_reader_writer_split.md`.
- 2025-11-26 (Codex): Payload widening round-trip is **critical**: plan updated to add synthetic legacy→modern→modern-read equality tests, file-level migrated root validation, and BE64 payload widening (tables/records/externals) before claiming the split complete. See `planning/phase3/modern_reader_writer_split.md`.
- 2025-11-26 (Codex): Design principle: keep modern BE64 code branch-free—fork legacy vs modern logic into separate functions/files instead of runtime format branches.
- 2025-11-26 (Codex): Naming decision: modern packers keep canonical names; legacy packers move to `legacy_*/` dirs with `_legacy` entry points so modern remains the default surface.

**Branches in flight**: `feature/carbon-migration-plan`, `feature/headless-system-bootstrap`, `feature/legacy_adapter_widening_and_v7_reader`

## Open Items
- Modern writer: Cancoon drop is in place, but migration fails while repacking the root because menu/applescripts/suites externals point at a freed block (adr=0xf709). Need to materialize/repair those externals before packing or guard against free-block references.
- Legacy adapter widening for tables/records remains; runtime/CLI v7 opens still use the legacy reader until the strict path lands.
- Remaining warnings: none in tests; runtime still logs headless traces. (Keep an eye on any new warnings after writer fixes.)
- Re-run migrator/CLI smoke on migrated roots once writer fixes land; stash logs.

## Next Steps
- Fix migration pack failure: ensure menu/applescripts/suites externals are loaded into handles (or skipped) before `tablesavesystemtable` under the modern writer so free-block addresses are not repacked. Re-run `FRONTIER_REGEN_ROOT=databases/Frontier-v6.root ./tests/runtime_tests` and confirm v7 output (view0 BE64, Cancoon absent).
- Finish the split modern writer path: ensure view0 header writes BE64 without legacy Cancoon, keep legacy reads separate from modern writes, and rerun the migrator/CLI smoke to confirm the variance/view corruption is gone.
- Finish the legacy adapter widening for tables/records and route runtime/CLI v7 opens through the strict reader; rerun `make -C tests runtime_tests` and `make -C tests cli_runtime_tests`, logging output paths.
- Add byte-level regressions for the forked readers/writer (modern header/block writes, adapter widening output) to guard the new files.
- Re-run `FRONTIER_REGEN_ROOT=… ./tests/runtime_tests` on additional legacy roots once the modern writer is fixed; stash logs under `/tmp` with timestamps.
- Update `docs/database_architecture.md` and phase3 docs once the modern read/write split is stable; keep `planning/phase3/big_endian_portability_audit.md` aligned with any new BE checks.
