# Carbon Migration – Current Status

Status
- State: In Progress
- Phase: Carbon Migration / Runtime Modernization + Verb Processor Automation + Phase 3 Verb Porting Prep
- Last Updated: 2025-12-06 (Evening)
- Owner: Codex / Claude
- Notes: Primary hand-off summary for active work only.
- **2025-12-06 (Claude - Processor Audits & Numeric Type System)**: Completed comprehensive processor audits for 40 verbs across 9 audit files with accurate docserver-extracted function signatures. Merged PR #64 with 21 commits containing 14,816 additions. Enhanced audits: xml.md (14 verbs), html.md (34 verbs), clipboard.md (4 verbs), searchengine.md (20 verbs), wp.md (27 verbs), plus corrections to point.md, rectangle.md, rgb.md, semaphore.md. All verbs now have complete UserTalk signatures, parameter lists, and return types. Documented numeric_type_system_modernization.md as Phase 3 planning doc covering floating-point context requirements, type coercion rules, bitwise operations, and 64-bit upgrade strategy. Key findings: 98% headless compatibility across 700+ documented verbs; identified searchengine use of GUI file dialogs as only substantive headless limitation (85% compatible with workaround).
- **2025-12-05 (Claude - Phase 3 Verb Porting Preparation)**: Completed comprehensive preparation for Phase 3 kernel verb implementation. Created three major planning documents: (1) `planning/phase3/kernel_verbs_implementation_plan.md` - 818-line master plan covering UserTalk documentation, Frontier runtime architecture, parameter handling, operator precedence, and SCNS (Simple Cross-Network Scripting). (2) `planning/phase3/processor_audit.md` - template/skeleton for Stage 1 categorization of all 37 stub processors with innovative stdio alternatives for GUI verbs (dialogs, file pickers). Analyzed external service dependencies and found none - all 37 processors are self-contained. Split platform-specific into Required (sys, file, launch) and Optional (python, dll, osa - gracefully degrade). (3) Updated implementation plan with correct UserTalk syntax (no spaces in param=value), accurate parameter mixing rules (ordered must start with first param, named after), and comprehensive operator precedence. Key insight: Many GUI-dependent verbs can be implemented using stdio (alert→println, file picker→directory listing), greatly expanding headless viability. Ready to begin actual processor audit in next session.
- **2025-12-04 (Claude - Kernel Verbs Automation)**: Completed automated kernel verb initialization system. Created `tools/kernelverbs_parser/parse_kernelverbs.py` that extracts all 51 EFP processor definitions (707 total verbs) from `kernelverbs.rc`. Generates `kernel_verbs_init.c` with whitelist-based filtering (currently 2 implemented: file 86 verbs, frontier 14 verbs). Integrated into Makefile with auto-regeneration. Addressed all critical code review issues. Ready for merge as PR #59. See `planning/progress_reports/2025-12-04-kernel-verbs-automation-milestone.md` for full details.
- **2025-12-04 (Claude - Critical Fix)**: Fixed structure alignment bug in v7 database header. Both `tydatabaserecord` and `tydatabaserecord_64` now have explicit 2-byte padding after `flags` field to ensure `views` array starts at offset 16 (8-byte aligned). Updated sizes: tydatabaserecord=118 bytes, tydatabaserecord_64=90 bytes. All database documentation updated to reflect corrected format.
- 2025-12-01 (Codex): Linked migration tests to the real table layer (HEADLESS_LINKS_REAL_DB), dropped format unpacking in headless, and fixed writable opens so `test_migration` passes end-to-end on the v6 fixture (no more Save As/dbclose crash).
- 2025-11-30 (Codex): Headless migration test now builds with portable file helpers and a local `copyctopstring` stub; heavy migration cases are temporarily skipped until the full runtime is linked (no crashes; backup/header checks pass).
- 2025-11-30 (Codex): Added a lightweight v6 fixture header check in `test_migration` (fixture file now present under `tests/fixtures/v6/test.root`); migrate/ensure/fixture paths still skip until the runtime slice is wired.
- 2025-11-30 (Codex): Save As swaps now run through context-aware guards (`dbswapglobals_context`) so allocations/view updates respect scoped Save As state without ambient globals; release-stack push/flush/zero now wrap default contexts; `db_format_tests` remain green.
- 2025-11-30 (Codex): Scoped Save As state into `db_context` (save-as snapshots + guards), refreshed default-context wrappers, and reworked migrator Save As to rely on its context destination handle; `make -C tests db_format_tests` and `./db_format_tests` pass after the refactor.
- 2025-11-29 (Codex): Context sweep for DB/adapter: stack/release helpers wrapped, internal assign/copy helpers exposed, TLS `use_64bit_format` shim removed (mode tracked via `g_mode_state`), and two-context regression added to `db_format_tests`.
- 2025-11-28 (Codex): Added `db_context` pack/assign/ref wrappers and dropped `use_64bit_format` from packers/tests; canonical v7 headers validated with a new header regression.
- 2025-11-27 (Codex): Legacy packers forked to `legacy_*` with modern packers defaulting to BE64; format-mode stack (`db_format_mode_push/pop`) in place and core suites (`db_format_tests`, `runtime_tests`, `cli_runtime_tests`) green.
- 2025-11-26 (Codex): Reader/writer split into legacy vs modern modules; modern headers serialized in BE64 with validation hooks; payload widening + round-trip tests still pending (see plan).
- 2025-11-30 (Codex): Added v6 fixture plan + file (`tests/fixtures/v6/test.root`) under `testData` namespace; migration test added (fails to build `test_migration` target on headless stubs due to legacy QuickDraw/timedate symbols—left as known issue).
- 2025-11-30 (Codex): Started `feature/v6-root-fixture`; drafted `planning/phase3/v6_root_fixture_plan.md` defining a legacy-authored v6 root covering all datatypes for migration regression. Pending: user to author the fixture in the legacy Windows app.

**Branches in flight**: `feature/carbon-migration-plan`, `feature/headless-system-bootstrap`, `feature/legacy_adapter_widening_and_v7_reader`, `feature/v6-root-fixture`

## Open Items
- Payload widening/round-trip still pending: need synthetic legacy→modern→modern-read equality tests and file-level validation on migrated roots (tables/records/externals), plus strict modern reader routing for v7 opens.
- Migration runtime is now linked in `test_migration`; keep an eye on headless-only logging volume and rerun broader suites with HEADLESS_LINKS_REAL_DB to ensure no UI symbol leaks.
- Remaining warnings: none in tests; runtime still logs headless traces. (Keep an eye on any new warnings after payload widening work.)
- Re-run migrator/CLI smoke on additional legacy roots once payload widening lands; stash logs.

## Next Steps

### Verb Porting (Phase 3 - Top Priority)
- **Immediate:** Begin Stage 1 (Processor Audit) - systematically review all 37 stub processors using `planning/phase3/processor_audit.md` template. Fill in categorization, complexity estimates, and dependencies for each processor.
- **Stage 1 Output:** Populated `processor_audit.md` with full assessment of all processors, quick-win identification, and implementation sequencing recommendations.
- **Stage 2:** Set up comprehensive test framework (test templates, CI integration, coverage tracking).
- **Stage 3:** Implement quick-win processors (base64, bit, clock, rgb, point, rectangle, semaphore - estimated 5-10 low-complexity processors).
- **Key Resource:** Refer to `planning/phase3/kernel_verbs_implementation_plan.md` (818 lines) for UserTalk semantics, operator precedence, parameter handling, Frontier runtime architecture, and SCNS details.
- **GUI Verbs Strategy:** Implement dialog/file picker verbs using stdio alternatives (printf/readln for alerts, directory listing for file pickers) to maximize headless functionality.

### Database Migration (Ongoing)
- Follow `planning/phase3/db_context_completion_plan.md`: migrate Save As swap/free-list/release-stack paths and callers (incl. headless) to explicit `db_context`, then trim legacy wrappers and validate with broader test suites.
- Build payload widening + round-trip tests: synthetic legacy payloads repacked via modern writer, then decoded via modern reader; assert logical equality and BE64-only encodings.
- Route runtime/CLI v7 opens through the strict modern reader once payload widening is ready; rerun `make -C tests runtime_tests` and `make -C tests cli_runtime_tests`.
- Add byte-level regressions for adapter-widened payloads (table/record/externals) to guard the new split.
- Re-run `FRONTIER_REGEN_ROOT=… ./tests/runtime_tests` on additional legacy roots after widening; stash logs under `/tmp` with timestamps.
- Debug `test_migration` fixture regression: migrated v7 root opens with a zero-item system table (see `/tmp/test_migration6.log`); verify view addresses/write targets and ensure table packer writes BE64 payloads into the destination.
