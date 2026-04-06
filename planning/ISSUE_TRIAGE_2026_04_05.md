# Issue Triage — April 2026

**Date**: 2026-04-05
**Open issues**: 132 (closed 13 stale/resolved during triage)
**Prioritization principles**: Security > Maintainability > Extensibility > Readability

This document organizes all 132 open GitHub issues by category and priority. Work items are ordered by impact within each category. Check off items as they're completed.

---

## Data-Loss Risks (Do First)

These can corrupt or destroy user data. Fix before all other work.

- [ ] #264: db.close() + db.open() in same script causes segfault
- [ ] #270: Prevent duplicate database opens (concurrent write corruption risk)
- [ ] #271: TOCTOU race condition in auto-migration allows concurrent database corruption

---

## Quick Wins (< 30 minutes each)

Trivial fixes that can be batched into a single session:

- [ ] #471: Frontier.isPowerPC() returns true on ARM — fix return value
- [ ] #473: system.environment.isCarbon returns true on headless CLI — fix return value
- [ ] #472: Implement Frontier.isARM / Frontier.isAppleSilicon verb
- [ ] #504: Add pthread_attr error checks in debug thread spawning
- [ ] #369: REPL memory leak — line not freed on error path
- [ ] #207: Add table.countVisibleRows() glue script
- [ ] #112: Add 24-hour max sleep limit to clock verbs
- [ ] #247: Remove outdated /tmp sandbox restriction from docs

---

## P0: Security & Data Integrity (Remaining)

Other security and stability issues. Fix after data-loss risks and quick wins.

- [ ] #365: Fix callers that ignore grabthreadglobals() return value (crash risk)
- [ ] #228: Document security model and threat context in docs/SECURITY.md
- [ ] #88: Networking architecture & security (strategic tracker)

---

## P0: Architecture — Launch Blockers

Global state elimination work required before Frontier can run reliably in multi-tenant or long-running scenarios.

- [ ] #296: Global state elimination for outline context (oppushoutline/oppopoutline)
- [ ] #274: Phase 2 — Eliminate global database state with explicit context threading
- [ ] #262: Migrate currenthashtable to thread-local storage (Phase 3 foundation)
- [ ] #292: Refactor processor table lifecycle to eliminate efptable workaround
- [ ] #86: Global runtime context & lifecycle (strategic tracker)
- [ ] #85: UI boundary via Ports & Adapters (strategic tracker)
- [ ] #87: Headless EFP routing parity (strategic tracker)
- [ ] #84: Memory management audit — rolling (strategic tracker)

---

## P0: Consolidation & Cleanup

Structural improvements that reduce ongoing maintenance burden.

- [ ] #482: Consolidate op_handler.c to single JSON-building mechanism (cJSON)
- [ ] #367: Cleanup project directory structure — move CLI stubs out of tests/
- [ ] #179: UserTalk integration test suite — comprehensive coverage (meta-tracker)
- [ ] #230: Implement file.getRelativePath() and frontier.getRelativePath() verbs
- [ ] #250: Connect lang.edit() to UI application when implemented

---

## P1: Runtime Correctness

Bugs and correctness issues that affect script execution.

- [ ] #184: Bare verb resolution fails (address value string/htable mismatch) — blocks other features
- [ ] #465: Startup script brace-matching bug prevents user.databases save
- [ ] #397: opinitcallbacks not idempotent — unconditional calls cause segfaults
- [ ] #355: Memory leak in evaluatereadonlyparam dereferenceop case
- [ ] #339: Pascal string logging displays garbage characters
- [ ] #307: Audit ostypevaluetype coercion and arithmetic operations
- [ ] #475: bigstring 255-byte limit truncates long file paths in verb return values
- [ ] #364: Thread cleanup race condition in webserver stop test

---

## P1: Verb Implementation

Missing or broken verb implementations that affect UserTalk script compatibility.

- [ ] #219: Implement file.getSpecialFolderPath and file.getSystemFolderPath
- [ ] #240: file.writeLine() should preserve original line ending format
- [ ] #393: Implement remaining fileMenu stub verbs for headless mode
- [ ] #298: Support direct string assignment to script and outline objects
- [ ] #191: Update system root glue scripts for enhanced sys.unixshellcommand
- [ ] #190: *(closed)* — stderr parameter implemented
- [ ] #362: Enable runtime adjustment of TCP_THROUGHPUT_WINDOW_SECS

---

## P1: Infrastructure & Developer Experience

Improvements that make the project easier to work on.

- [ ] #323: File portable — add thread-safe FD table initialization
- [ ] #305: Complete hashtablestack macro migration after bootstrap refactoring
- [ ] #379: Standardize error reporting pattern to use langerrormessage() everywhere
- [ ] #380: Expand bigstring max length beyond 255 characters (see also #423, #475)
- [ ] #423: Refactor bigstring to remove 255-character length limit
- [ ] #406: Increase MAX_THREADS beyond legacy 64-thread limit
- [ ] #415: Refactor large verb switch statements into smaller function groups
- [ ] #437: Eliminate hexternalpackdatabase file-scoped global in langhash.c
- [ ] #331: Implement UserTalk linter to validate syntax and enforce coding style
- [ ] #211: Audit and eliminate GUI code still executing in headless mode
- [ ] #216: Audit and potentially remove hdlintarray and related array handle types
- [ ] #81: Unvendor temporary build dependencies (CMake + Paige)
- [ ] #227: Create LOG_COMP_FILE logging component

---

## P1: Design Decisions Needed

Issues blocked on architectural decisions. Resolve the design, then implement.

- [ ] #259: Design improved outline cursor addressing system
- [ ] #273: Design consistent node identifiers for outline verbs
- [ ] #272: op.getExpansionState/setExpansionState — persistent node IDs vs line numbers
- [ ] #284: Cross-platform support for AppleEvent and legacy integration verbs
- [ ] #332: ODB reference counting for hash tables across thread boundaries
- [ ] #89: OSA / IPC strategy (strategic tracker)

---

## P2: Debugger Follow-ups

Minor cleanup from the debugger implementation (Phases 1-7 complete).

- [ ] #502: Move headless_thread_verbs.c from tests/ to frontier-cli/
- [ ] #503: Replace param_reserved[0] with dedicated debug state field in tythreadglobals
- [ ] #504: Add pthread_attr error checks in debug thread spawning (also in Quick Wins)

---

## P2: Testing Gaps

Test coverage improvements. Not blocking but reduce regression risk.

- [ ] #166: Integration tests for table context mutation tracking
- [ ] #172: Integration tests for string operations (partial — 3 files exist)
- [ ] #173: Integration tests for file operations (partial — 3 files exist)
- [ ] #174: Integration tests for control flow (0 files exist)
- [ ] #175: Integration tests for error handling (partial — 4 files exist)
- [ ] #226: Security-focused integration tests for file verb path validation
- [ ] #235: Integration tests for thread-local parameter state isolation (ADR-005)
- [ ] #142: Migration error path test coverage
- [ ] #143: Error-path testing for cleanup_migration_database()
- [ ] #118: External object access post-migration tests
- [ ] #72: v6→v7 migration round-trip integration tests
- [ ] #346: C unit tests for CLI extension detection
- [ ] #357: Integration tests for REPL word navigation
- [ ] #333: TCP echo test framework timing issue
- [ ] #70: Bitwise ops — verify bit bounds and test bit 63
- [ ] #71: Verify headless verb registration has no ordering dependencies
- [ ] #78: Cross-arch BE64 serialization verification
- [ ] #79: Extended type bounds tests for hash unpack

---

## P2: Database Architecture Refactoring

Cleanup work from the mode stack refactor and migration system.

- [ ] #141: Refactor migration to set databasedata=nil at final cleanup
- [ ] #201: Convert loading paths to use external_set_inmemory()
- [ ] #140: Audit db_context_guard usage in disposal paths
- [ ] #139: Migration cleanup duplication elimination
- [ ] #138: Refactor dbendsaveas to make implicit disposal explicit
- [ ] #135: Refactor outline management from push/pop to deterministic context
- [ ] #68: Verify ensure_database_modern/db_format_mode_apply idempotence
- [ ] #69: Audit 64-bit promotion in setintvalue/setlongvalue call sites

---

## P2: Polish & Future Features

Nice-to-have improvements and strategic exploration.

- [ ] #494: WebSocket server max clients limit too low (8)
- [ ] #316: Direct REPL prompt to stderr for cleaner stdout piping
- [ ] #145: JSON escaping incomplete for control characters in logging
- [ ] #156: Native logging timestamps for headless mode
- [ ] #127: Error context tracking in db_context
- [ ] #229: Path validation with realpath() canonicalization
- [ ] #334: Optimize defined() to avoid loading external tables from disk
- [ ] #312: Cross-platform file locking (file.lock, file.unlock, file.islocked)
- [ ] #374: Stricter compiler warning flags
- [ ] #375: Document type relationships for FSCopyAliasInfo casts
- [ ] #376: Worktree setup — manual copy of Paige build
- [ ] #322: Thread test harness mutex protection
- [ ] #449: test_table_sorting segfault in headless test harness
- [ ] #213: Extract fragile log filtering to helper in table_operations_integration
- [ ] #214: Auto-install xxd dependency in test runner
- [ ] #132: Hash table disposal investigation in unit tests

---

## P2: Strategic Trackers (Long-lived)

These are epic-level tracking issues that span multiple phases. Update periodically.

- [ ] #100: Performance optimizations
- [ ] #101: User experience enhancements
- [ ] #102: Developer experience improvements
- [ ] #103: UserTalk language server & bridge (Phase 5+)
- [ ] #105: Modernize file.getSystemFolderPath (blocked by #106)
- [ ] #106: Design permission/sandboxing model
- [ ] #111: Timezone-aware datetime support
- [ ] #129: Runtime log level control via REST/UserTalk
- [ ] #176: Generate UserTalk test coverage reports
- [ ] #177: Performance benchmarking for UserTalk test runner
- [ ] #178: Run UserTalk integration tests in CI pipeline
- [ ] #168: Table context reference counting (Phase 6+)
- [ ] #169: Callback guards with table symbol mutation tracking (Phase 5+)
- [ ] #90: File I/O & path policy (strategic tracker)
- [ ] #91: Unicode strategy (strategic tracker)
- [ ] #92: File verb enhancements — settype/setcreator cross-platform
- [ ] #93: Hash table modernization (strategic tracker)
- [ ] #94: Concurrency model & task contexts (strategic tracker)
- [ ] #95: Headless migration options
- [ ] #97: Remote runtime + local guest databases
- [ ] #98: Rich text type (future)
- [ ] #99: WPText → RTF migration
- [ ] #194: sys.winshellcommand (Windows-only)
- [ ] #191: sys.winshellcommand glue scripts (Windows-only)
- [ ] #299: Process management verbs for headless mode
- [ ] #308: Systematic type handling cleanup
- [ ] #319: Robust sys.appisrunning() cross-platform
- [ ] #320: Robust sys.getapppath() cross-platform

---

## Recommended Next Actions

1. **Fix data-loss risks** (#264, #270, #271) — protect user data first
2. **Batch the Quick Wins** — single session, commit directly to develop
3. **Consolidation** (#482, #367, #179) — cleaner codebase benefits all subsequent work
4. **Fix P1 runtime bugs** (#184, #465, #397) — these affect script execution
5. **Strategic architecture** (#296, #274, #262) — plan before implementing
