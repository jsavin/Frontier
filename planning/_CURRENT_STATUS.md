# Carbon Migration – Current Status

Status
- State: In Progress
- Phase: Carbon Migration / Runtime Modernization + Verb Porting
- Last Updated: 2025-12-18
- Owner: Codex / Claude
- Notes: Active snapshot only; older entries moved to `_STATUS_ARCHIVE.md`.

Recent Updates
- **2025-12-18 (Issue #123 investigation)**: Identified root cause of external table access failures post-migration. Problem: ROOT table unpacked with legacy 32-bit reader (`use64=0`) despite v7 database format (`use64=1`). Child tables correctly use modern reader. Migration writes correct 64-bit addresses, but root table unpacking uses wrong reader path. This is a reader/writer fork issue in `hashunpacktable` - needs fix to respect database format mode for root table. All external tables successfully materialized during migration with new v7 addresses. Comprehensive documentation added: `docs/external_table_variable_management.md` (lifecycle, states, migration patterns) and `planning/phase3/ISSUE_123_SOLUTION_DESIGN.md` (solution design). Next: Fix root table unpacking to use modern v7 reader path.
- **2025-12-15 (PR #109 merged)**: Completed Phase 3.E-F automatic verb binding improvements. Phase 3.E: Added 7 missing exception table entries and normalized verb names to lowercase in parser, improving coverage from 56% → 67% (477 verbs detected). Phase 3.F: Implemented `sys.getenvironmentvariable()` and `sys.setenvironmentvariable()` with POSIX cross-platform support and buffer overflow protection (length check for >255 char values). Added platform-specific documentation and updated planning docs. See `planning/phase3/kernel_verb_porting/automatic_verb_binding_phase3_plan.md` for full Phase 3.E-F details. Coverage now 68% (479/707 verbs).
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
