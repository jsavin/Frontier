# Carbon Migration – Current Status

Status
- State: In Progress
- Phase: Carbon Migration / Runtime Modernization + Verb Porting
- Last Updated: 2025-12-13
- Owner: Codex / Claude
- Notes: Active snapshot only; older entries moved to `_STATUS_ARCHIVE.md`.

Recent Updates
- **2025-12-13 (PR #75 merged)**: Hardened hash pack/unpack with explicit 16-byte BE buffers, bounds-checked `hashunpackstring`, header detection guards, optional logging (`FRONTIER_HASHUNPACK_LOG`), and compile-time layout asserts. Table globals now reset cleanly after migration/load; CLI always hydrates the system root and clears globals before/after migrations. Docs updated (`docs/database_architecture.md` v7 hash record layout). Full `SANITIZE=1 make -C tests test` passes; CLI still reports exit=1 for stubbed verbs (clock.*) but harness marks tests as passed.
- **2025-12-09 (migration text encoding fix)**: Guarded TEC converter disposal in `converttextencoding`; `save_migration_tests` now clean under ASan/UBSan.
- **2025-12-08 (headless verb coverage)**: Added headless `string.upper/lower/length` and `math.random` (with bounds checks); CLI inline `-e` path now uses `langrunhandle`.

Open Items (active)
- Implement remaining headless/kernel verbs needed for CLI runtime (`clock.now`, `clock.ticks`, etc.) so `cli_runtime_tests` no longer exit=1.
- Follow-ups filed:
  - #76: Add corruption/bounds tests for hash unpack (OOB name index, truncated records, header edge cases).
  - #77: Factor BE pack/unpack helpers (reduce manual memcpy repetition).
  - #78: Cross-arch BE64 serialization verification (golden blobs on x86_64/arm64).
- Continue Phase 3 verb porting per processor audits; prioritize quick wins (clock/date/dialog stubs to reduce CLI gaps).

Next Steps
- Address CLI verb gaps (clock/date) and re-run `cli_runtime_tests` expecting clean exit codes.
- Add targeted hash unpack corruption tests (issue #76) once helper macros land or in parallel.
- Plan a cross-arch sanity check for v7 hash/table records (issue #78) after helper refactor (#77).
