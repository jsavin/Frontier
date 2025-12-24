# Carbon Migration – Current Status

Status
- State: In Progress
- Phase: Carbon Migration / Runtime Modernization + Verb Porting
- Last Updated: 2025-12-23
- Owner: Codex / Claude
- Notes: Active snapshot only; older entries moved to `_STATUS_ARCHIVE.md`.

Recent Updates
- **2025-12-23 (Mode Stack Refactor - Phase 1 NEAR COMPLETE)**: Substantial progress on Phase 1 execution with 8 commits over 2 sessions. Session 1 (5 commits): (1) Mode stack corruption fix - cleared mode depth before v7 apply to prevent stacked v6 override; (2) External materialization for tables/outlines/scripts/wptext/pictures with correct recursion (tables only, not leaf nodes); (3) Picture externals refactored with `pictverbinmemory()` and `pictverbpack_internal()` taking explicit context; (4) Fixed `langexternalpack_internal()` to set v7 write mode for ALL externals (not just loaded-from-disk); (5) Removed push/pop from `wp_portable_state_dbref()`. Session 2 (3 commits + ADR): (6) Created **ADR-002: Context-Based Format Versioning** documenting architectural decision with rationale, implementation patterns, pitfalls, and validation criteria; (7) Eliminated `dbpushdatabase` from langexternal.c, opverbs.c, langhash.c (4 patterns: disposehashnode, hashassign, hashresolvevalue, hashpackscalar); (8) Eliminated `dbpushdatabase` from claycallbacks.c, menueditor.c, menupack.c (menu/clay functions). **Progress**: Reduced from 22 → 5 remaining calls (77% complete). Remaining 4 calls deferred to Phase 2 (require helper function refactoring: copyvaluerecord_context, meloadmenurecord_context). Branch: `refactor/explicit-context-no-mode-stack`. Phase 2 planning in progress. References: `ADR-002-context-based-format-versioning.md`, `MODE_STACK_REFACTOR_PROGRESS.md`, `MODE_STACK_REFACTOR_PHASE1_DETAILED_v2.md`.
- **2025-12-23 (Phase 1 Mode Stack Refactor Plan - Production Ready)**: Refreshed and enhanced the Phase 1 detailed plan for the Mode Stack Refactor based on comprehensive system-architect feedback. Plan is now production-ready for autonomous execution by Claude Sonnet. Critical improvements: added infrastructure audit step (Step 0.1), explicit coding conventions, granular substeps for helper function conversion, byte-level validation (table headers), Issue #123 regression tests, pre-commit validation, and emergency rollback procedures. Plan explicitly addresses root cause of migration segfault (writing legacy-format v4 headers into v7 database due to mode stack inheritance). Each step 30min-3hrs, clear success/failure criteria, determinism testing at every step. Reference: `planning/phase3/mode_stack_refactor/MODE_STACK_REFACTOR_PHASE1_DETAILED_v2.md`
- **2025-12-23 (Logging Infrastructure COMPLETE - 100% Migration)**: Completed all 6 phases of logging infrastructure migration! Summary: migrated 377 fprintf(stderr) statements to structured logging across all user-facing code. Phases 3.2-3.6 completed:
  - PR #155: Phase 3.2 (13 statements) - trivial files with single/few logs
  - PR #157: Phase 3.3 (18 statements) - simple multi-statement files
  - PR #158: Phase 3.4 (50 statements) - medium complexity with hex dumps, conditional removal, ifdef removal
  - PR #160: Phase 3.5 (16 statements) - special cases: macro replacements (HEADLESS_LOG, OP_HEADLESS_TRACE) and Bison parser source migration
  - Phase 3.6: Meta-logging exemption added to check_fprintf.sh for logging.c (8 intentional statements for bootstrap/logging system diagnostics)
  - All tests passing; check_fprintf.sh shows zero violations in migrated files. Logging.c exempted as it IS the logging system itself. **All fprintf migration work complete!**
- **2025-12-22 (PR #137 MERGED - Migration Double-Free Fix)**: Fixed critical segmentation fault in `save_migration_tests` caused by double-free during cleanup. Root cause: `dbendsaveas_context()` internally calls `dbdispose()`, but cleanup code unconditionally called it again. Solution: Set `databasedata = nil` immediately after `dbendsaveas*()` calls to prevent double-free. Additional fixes: removed context guards from `dbzeroreleasestack()` (guard patterns during disposal cause dangling pointers), added defensive `validhandle()` check before dereferencing in cleanup, extracted `cleanup_migration_database()` helper function to improve testability and document three distinct cleanup scenarios. Added cleanup state validation test (`save_migration_tests.c` now validates `fldatabasesaveas` flag properly reset). All commits include planning doc references per CLAUDE.md guidelines. Follow-up issues filed: #138 (make disposal explicit in dbendsaveas), #139 (cleanup path duplication), #140 (audit context guards in disposal paths), #141 (final cleanup restructuring), #142 (error path test coverage), #143 (cleanup_migration_database error scenarios). **PR #137** merged with 7 commits addressing comprehensive code review feedback across multiple rounds.
- **2025-12-19 (Mode Stack Refactor - Planning Complete)**: Completed comprehensive planning for full mode stack to explicit context refactor. Decision made: pursue Option B (full refactor) instead of Option A (incremental) because stability is paramount and incremental approach just kicks the pebble down the road. Created detailed implementation plan (`planning/phase3/MODE_STACK_REFACTOR_PLAN.md`) covering 5 phases over 8-13 days: Phase 1 (core serialization), Phase 2 (DB operations), Phase 3 (format readers/writers), Phase 4 (migration code), Phase 5 (cleanup). Also created quick-start guide (`MODE_STACK_REFACTOR_QUICKSTART.md`) for easy resumption. Root cause analysis: mode stack is global state that implicitly propagates through recursive operations, causing every Issue #123 bug (reader fork, writer fork, invalid addresses). New architecture uses explicit `db_context` structure passed through all operations, making mode deterministic and preventing inheritance bugs. Ready to begin implementation Phase 1: converting langhash.c and tablepack.c to context-based (~25 call sites).
- **2025-12-18 Evening (Issue #123 RESOLVED - PR #124)**: Fixed external table access failures post-migration. Root cause was **reader/writer fork issue**: root table unpacked with legacy 32-bit reader (`use64=0`) despite v7 database format (`use64=1`), while child tables correctly used modern reader. Two critical bugs fixed: (1) `hashunpacktable()` reader selection logic now respects database format mode (lines 4034-4036 in langhash.c); (2) `tableverbpack()` mode stack issue - now explicitly pushes modern mode before packing to prevent inherited legacy mode (lines 376-381 in tablepack.c). Enhanced `save_migration_tests.c` with 4-phase validation: version check, migration execution, format validation, and external table accessibility testing. All 6/6 validation checks passing. Comprehensive documentation created: `docs/external_table_variable_management.md` (498 lines, lifecycle/states/migration), updated `planning/phase3/ISSUE_123_SOLUTION_DESIGN.md` with actual root cause analysis and evidence, and enhanced `CLAUDE.md` with new "Architectural Patterns to Avoid" section documenting mode stack issues and reader/writer fork gotchas. **PR #124** (`fix/issue-123-migration-validation` branch) submitted with all commits and awaits bot review. Mode stack push/pop mechanism flagged as architectural debt for future refactoring (Issue #123 repeatedly exposed this pattern causing bugs).
- **2025-12-15 (PR #109 merged)**: Completed Phase 3.E-F automatic verb binding improvements. Phase 3.E: Added 7 missing exception table entries and normalized verb names to lowercase in parser, improving coverage from 56% → 67% (477 verbs detected). Phase 3.F: Implemented `sys.getenvironmentvariable()` and `sys.setenvironmentvariable()` with POSIX cross-platform support and buffer overflow protection (length check for >255 char values). Added platform-specific documentation and updated planning docs. See `planning/phase3/kernel_verb_porting/automatic_verb_binding_phase3_plan.md` for full Phase 3.E-F details. Coverage now 68% (479/707 verbs).
- **2025-12-13 (PR #75 merged)**: Hardened hash pack/unpack with explicit 16-byte BE buffers, bounds-checked `hashunpackstring`, header detection guards, optional logging (`FRONTIER_HASHUNPACK_LOG`), and compile-time layout asserts. Table globals now reset cleanly after migration/load; CLI always hydrates the system root and clears globals before/after migrations. Docs updated (`docs/database_architecture.md` v7 hash record layout). Full `SANITIZE=1 make -C tests test` passes; CLI still reports exit=1 for stubbed verbs (clock.*) but harness marks tests as passed.
- **2025-12-09 (migration text encoding fix)**: Guarded TEC converter disposal in `converttextencoding`; `save_migration_tests` now clean under ASan/UBSan.
- **2025-12-08 (headless verb coverage)**: Added headless `string.upper/lower/length` and `math.random` (with bounds checks); CLI inline `-e` path now uses `langrunhandle`.

Open Items (active)
- **Issue #123 (RESOLVED in PR #124)**: External table access post-migration. Root cause: reader/writer fork issue where root table unpacking used legacy 32-bit reader despite v7 database format. Fixed reader selection in `hashunpacktable()` and mode stack corruption in `tableverbpack()`. Awaiting bot review on PR #124.
- **MODE STACK REFACTOR (CRITICAL - PLANNED)**: Full refactor of mode stack to explicit context passing. Planning complete (see `planning/phase3/MODE_STACK_REFACTOR_PLAN.md`). Ready to begin Phase 1 implementation. This addresses the architectural debt that caused all Issue #123 bugs. Decision: pursue full refactor (Option B) for long-term stability. LOE: 8-13 days (2-3 weeks). No blockers.
- Implement remaining headless/kernel verbs needed for CLI runtime (`clock.now`, `clock.ticks`, etc.) so `cli_runtime_tests` no longer exit=1.
- Follow-ups filed:
  - #76: Add corruption/bounds tests for hash unpack (OOB name index, truncated records, header edge cases).
  - #77: Factor BE pack/unpack helpers (reduce manual memcpy repetition).
  - #78: Cross-arch BE64 serialization verification (golden blobs on x86_64/arm64).
- Continue Phase 3 verb porting per processor audits; prioritize quick wins (clock/date/dialog stubs to reduce CLI gaps).

Next Steps (Recommended Priority Order)

**Immediate** (Foundational & unblocking):
0. **Logging Infrastructure Refactor** (PRIORITIZED - ~2-3 days)
   - Create logging.h/logging.c with runtime log level control
   - Replace 76 debug ifdef blocks incrementally
   - Migrate database layer debug ifdefs first (highest-impact subsystem)
   - Reference: `planning/phase3/code-cleanup/IFDEF_CLEANUP_STRATEGY.md` (Sections 2A-2B)
   - Reason: Foundational infrastructure improving all future work; enables runtime debugging without rebuild

1. **Phase 1 Mode Stack Refactor Prerequisites**: Complete Issues #135 & #136
   - Issue #135: Refactor outline (op) management from push/pop to deterministic context (~3-5 days)
   - Issue #136: Audit external object processing for push/pop anti-patterns (~1-2 days)
   - Reason: Blocks clean mode stack refactor Phase 1; prevents same push/pop anti-pattern bugs

2. **PR #137 Follow-Ups**: Complete Issues #138 & #140
   - Issue #138: Make dbendsaveas disposal explicit (~4-6 hours)
   - Issue #140: Audit db_context_guard usage in disposal paths (~2-4 hours)
   - Reason: Code quality, prevents future double-free regressions

**Short-term** (Code cleanup & quick wins):
3. **Phase 1 Code Cleanup Completion**: ~3 blocks remaining
   - Remove `oldMACVERSION` blocks (3 in langhash.c)
   - Remove commented `WIN95VERSION` blocks (already mostly cleaned)
   - ~50 lines total; low-risk cleanup

4. **Issue #121**: Implement 28 error stubs for remaining verbs (~3-4 hours)
   - Quick win; unblocks CLI testing
   - Can be done in parallel with other work

**Medium-term** (Testing & infrastructure):
5. **Issue #77**: Add BE pack/unpack helper macros (~4-6 hours)
   - Foundation for #78 and #79
   - Reduces manual memcpy repetition

6. **Issue #78**: Cross-arch BE64 serialization verification (~6-8 hours, depends on #77)
   - Ensures v7 database format portability

7. **Issue #79**: Extended type bounds tests for hash unpack (~6-8 hours)
   - Comprehensive edge case coverage

**Later** (Major refactors - requires decisions):
- Begin full mode stack refactor Phase 1 (after prerequisites complete; ~2-3 days)
  - Reference: `planning/phase3/MODE_STACK_REFACTOR_QUICKSTART.md`
  - Branch: `refactor/explicit-context-no-mode-stack`
- Continue mode stack Phases 2-5 (8-13 days total)
- Address remaining architectural P0s (#86, #87, #85, #88) requiring design decisions
- Deferred: PIKE removal implementation (approved; scheduled for Phase 4, Week 13)
