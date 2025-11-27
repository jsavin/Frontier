# Carbon Migration – Current Status

Status
- State: In Progress
- Phase: Carbon Migration / Runtime Modernization
- Last Updated: 2025-11-26 (Afternoon)
- Owner: Codex
- Notes: Primary hand-off summary for active work only.
- 2025-11-26 (Codex): Reader/writer split into `db_reader_legacy.c`, `db_reader_modern.c`, `db_writer_modern.c`; `make -C tests db_format_tests` passes with the longstanding `__builtin_return_address` warning and an unused helper in `db.c`.
- 2025-11-26 (Codex): Modern writer path still emits the legacy Cancoon/view block when regenerating v7 roots; `clock.now`/migrator smoke not rerun since the split. Need separate legacy read + modern write contexts and a BE64 view0 with Cancoon dropped.
- 2025-11-26 (Codex): Adapter address marking no longer flips `use_64bit_format`; legacy reads stay 32-bit until wide writes are explicitly enabled.
- 2025-11-26 (Codex): Detailed split plan lives at `planning/phase3/modern_reader_writer_split.md`.
- 2025-11-26 (Codex): Migrator default path (`migrate_32bit_to_64bit`) now drops Cancoon like the explicit drop helper; v7 outputs should be Cancoon-free by default once writer logic is fixed.
- 2025-11-26 (Codex): Legacy-vs-modern use_64bit_format now tracks the active database handle (adapter keeps legacy reads 32-bit; destination writes are BE64). `langexternalpack` forces legacy reads while materializing externals. `FRONTIER_REGEN_ROOT=databases/Frontier-v6.root ./tests/runtime_tests` now completes and emits `databases/Frontier-v6-v7.root` (warnings unchanged).
- 2025-11-26 (Codex): Payload widening round-trip is **critical**: plan updated to add synthetic legacy→modern→modern-read equality tests, file-level migrated root validation, and BE64 payload widening (tables/records/externals) before claiming the split complete. See `planning/phase3/modern_reader_writer_split.md`.
- 2025-11-26 (Codex): Design principle: keep modern BE64 code branch-free—fork legacy vs modern logic into separate functions/files instead of runtime format branches.
- 2025-11-26 (Codex): Naming decision: modern packers keep canonical names; legacy packers move to `legacy_*/` dirs with `_legacy` entry points so modern remains the default surface.

**Branches in flight**: `feature/carbon-migration-plan`, `feature/headless-system-bootstrap`, `feature/legacy_adapter_widening_and_v7_reader`

## Open Items
- Modern writer must drop the legacy Cancoon/view block and emit BE64-only headers/blocks; runtime/CLI v7 loads still fail on the legacy view/variance.
- Legacy adapter widening for tables/records remains; runtime/CLI v7 opens still use the legacy reader until the strict path lands.
- Outstanding warnings: `__builtin_return_address` in `Common/source/memory.c` and unused `db_prepare_modern_header` in `db.c`.
- Tests beyond `make -C tests db_format_tests` not rerun post-split; migrator/CLI smoke pending.

## Next Steps
- Finish the split modern writer path: ensure view0 header writes BE64 without legacy Cancoon, keep legacy reads separate from modern writes, and rerun the migrator/CLI smoke to confirm the variance/view corruption is gone.
- Finish the legacy adapter widening for tables/records and route runtime/CLI v7 opens through the strict reader; rerun `make -C tests runtime_tests` and `make -C tests cli_runtime_tests`, logging output paths.
- Add byte-level regressions for the forked readers/writer (modern header/block writes, adapter widening output) to guard the new files.
- Re-run `FRONTIER_REGEN_ROOT=… ./tests/runtime_tests` on additional legacy roots once the modern writer is fixed; stash logs under `/tmp` with timestamps.
- Update `docs/database_architecture.md` and phase3 docs once the modern read/write split is stable; keep `planning/phase3/big_endian_portability_audit.md` aligned with any new BE checks.
