# Current Status

Last Updated: 2026-03-08

## Current Focus: Startup Flow Stabilization -- mainResponder, Manila, Web Setup

**Status**: Integration tests at **0 failures** (1,920 tests, 8-worker parallel execution in ~37s). 302 unit tests. databasedata global elimination complete (Phases 1-10). Startup bootstrap partially stabilized. System root saves on exit. Webserver startup attempted but blocked by inetd.startOne script dependencies.

**Latest Release**: **v1.0.0-alpha.7** (February 16, 2026)

**Verb Coverage**: **68% (482/710 verbs)** - TCP at 100%, all core processors complete. fileMenu verbs: 7/10 implemented (open, close, closeall, save, saveAs, saveCopy, new).

## Recent Achievements (February 28 - March 8, 2026)

### EFP Fast-Path Regression Fix (PR #469) - MERGED
- Removed stale headless EFP fast-path in `langgethandlercode()` that checked `efptable` BEFORE `system.paths` for dotted verbs, violating the documented search order
- The fast-path (added Oct 2025 as a "temporary shim") returned success with `hnode=nil` for UserTalk scripts under EFP-named tables, blocking database fallback
- Broke `inetd.startOne`, `inetd.isDaemonRunning`, and other UserTalk scripts under EFP-named tables
- Added 7 regression tests (inetd + tcp namespaces, positive and negative cases)

### Integration Test Reliability (PR #468) - MERGED
- Fixed 19 consistently-failing integration tests
- Root causes: corrupt handle guards for pack-on-exit (SIGSEGV), protocol executor process death without recovery
- Added `kMinValidPointer` constant, `hashpackguard_corrupt_handle()` shared helper
- Protocol executor: restart-on-failure in `reset()`, per-process fallback retry, stderr capture to temp file
- Hardened `getaddressparts` NULL guards, `fldontsave` skip logging, `resolve_indexed_node` error context

### CLI State Persistence (PRs #462, #464) - MERGED
- **PR #462**: Save system root database on CLI exit for state persistence
- **PR #464**: Always save system root on exit instead of checking dbdirtymask (the mask was unreliable)
- In-memory changes now persist across CLI sessions

### GIL & Threading Fixes (PRs #463, #467) - MERGED
- **PR #463**: Resolve GIL deadlock preventing HTTP callback dispatch (TCP accept handler was blocking without yielding)
- **PR #467**: Enable GIL yielding in blocking REPL mode (readline blocks without yielding, starving background threads)

### File Path Fix (PR #466) - MERGED
- Handle trailing path separators in `portable_filefrompath` (was returning empty filename for paths like `/foo/bar/`)

## Earlier Achievements (February 16-27, 2026)

## Earlier Achievements (February 10-16, 2026)

### Integration Test Reliability -- 0 Failures - MERGED
- **PRs #428-#433**: NDJSON protocol mode, parallel test execution, test fixes
- Fixed `langerrordisable` leak in `langgethandlercode()` headless fast-path -- made ALL headless EFP verb errors uncatchable by try/else
- Per-worker database isolation for parallel test workers (each gets own .root7 copy)
- 40 quick-win test fixes, pexpect PTY harness, unit test segfault fix
- **Result: 1,881 tests, 0 failures, 189 skipped (8 workers, ~40s)**

### GIL-Based Threading - MERGED
- **PR #410**: Real POSIX threads with Global Interpreter Lock
- Yield points at `langbackgroundtask()` and `thread.sleepTicks()`
- Extends cooperative foundation (PR #404)

### REPL & UX Improvements - MERGED
- **PR #424**: Ranger-style two-pane file browser with arrow key navigation
- **PR #425**: File browser and dialog UX improvements
- **PR #411**: Guest database REPL navigation and prompt display
- **PR #409**: `[n]` index syntax and relative paths in `/list` and `/jump`

### Runtime Improvements - MERGED
- **PR #413**: Per-component log levels (`FRONTIER_LOG=comp:level` and `--log` flag)
- **PR #419, #420**: Consolidate verb registration + real `wp.getText()`/`wp.setText()`
- **PR #416, #417**: `filemenu.new`, window verb no-ops, named parameters
- **PR #426**: Real platform detection + unified version system
- **PR #427**: Migration segfault fix for v6 guest databases
- **PR #414**: Stack overflow fix in portable file verbs dispatcher
- **PR #418**: Startup bootstrap fixes (random() params, log corruption)
- **PR #412**: Materialize all external types during guest DB loading

### Stability & Bug Fixes - MERGED
- **PR #408**: Resolve startup segfault from context guard and tmp stack bugs
- **PR #421**: Suppress verb error logging inside UserTalk try blocks
- **PR #422**: Add script path to error logs, fix getFileDialog parameter count

### Documentation - MERGED
- **PR #407**: Centralize shared AI workflow guidance (`docs/AI_SHARED_GUIDELINES.md`)

## Earlier Achievements (February 7-10, 2026)

### Cooperative Threading Infrastructure - MERGED
- **PR #404**: Add cooperative threading with thread registry for headless mode
- Thread registry with `register_main_thread()`, `get_nth_thread_id()` for iterating active threads
- Cooperative globals save/restore (`headless_save_threadglobals`/`headless_restore_threadglobals`) isolates C globals (fllangerror, flreturn, flbreak, flcontinue, flscriptrunning, etc.)
- Main thread gets ID 2 (`idapplicationthread`), spawned threads start at 3+
- `scriptError()` in spawned thread stops that thread only -- fire-and-forget semantics
- `thread.evaluate()`, `thread.callscript()`, `thread.getCurrentID()`, `getCount()`, `exists()`, `kill()`, `sleep()`, `wake()`, `getNthID()` all operational
- 10 integration tests + 19 unit tests (all passing)
- Filed issue #406 (increase MAX_THREADS beyond 64)

### Startup Hang Fix - MERGED
- **PR #403**: Fix startup hang caused by wrong BIGSTRING length prefixes
- Fixed 3 wrong BIGSTRING prefixes in `headless_string_verbs.c` making `string.innerCaseName`, `string.macRomanToUtf8`, `string.utf8ToMacRoman` unreachable
- Root cause: `uninstallSubMenu.ut` called unreachable verb -> error -> semaphore not unlocked -> `installSubMenu` busy-wait for 2 hours
- Added defensive `langreleaseallsemaphores` auto-cleanup after startup script and REPL execution
- Verified with macOS `sample` command (832/832 samples in `locksemaphoreverb` busy-wait)

### Callback Infrastructure Fix - MERGED
- **PR #402**: Fix callback infrastructure segfault and test failures
- Replaced undefined `langnewtable` symbol (NULL crash) with `tablenewtablevalue`
- Fixed double-free crashes: deep-copy parameter values with `exemptfromtmpstack`
- Fixed "too many parameters" errors in callback param passing
- Added `fllangerror = false` reset in TEST macro to prevent error cascade
- All 14 callback tests now pass (was: 1 pass, segfault, 12 failures)

### Dist Startup Stability - MERGED
- **PR #401**: Fix dist startup crashes (second run segfault and log spew)
- Restored `langexternalsetdatabase()` (was turned into a no-op, breaking cross-database hdatabase assignment)
- Fixed `getoutlinefromtarget()` for menu externals -- was interpreting `tysavedmenuinfo` as packed outline
- Added NULL guard for `param1` in `langfunctioncall()` for corrupt/uninitialized code trees
- Added NULL safety to 8 outline traversal functions in `opvisit.c`
- Downgraded PACK diagnostic logging (eliminated 6,800+ lines of noise per save)

### Guest Database Context Fix - MERGED
- **PR #400**: Use variable database context in `getoutlinefromtarget()`
- Fixed op verbs reading from system root instead of guest DB -- caused segfault in `oprecursivelyvisit()`

### window.isOpen() Implementation - MERGED
- **PR #398**: Implement `window.isOpen()` for headless mode
- Path A (address): checks if address resolves to root table of any opened database
- Path B (string/file path): compares paths using `realpath()` normalization
- **PR #399**: Follow-up -- raise script errors for `realpath()` failures instead of silent false
- 10 integration tests (all passing)

### Portable fileloop & Outline Callback Fixes - MERGED
- **PR #396**: Implement portable fileloop and fix outline callback crashes
- POSIX `opendir`/`readdir`/`closedir` fileloop implementation replacing stubs
- Fixed `macfilespecisvalid` stub, NULL callback pointer crashes in outline operations
- Startup script now completes successfully

### fileMenu.saveAs/saveCopy - MERGED
- **PR #394**: Implement `fileMenu.saveAs(path)` and `fileMenu.saveCopy(path)`, restore `fldatabasesaveas` guard
- Save-then-copy approach for both system root and guest databases
- 28 filemenu integration tests total; closes issues #392 and #395

### opdisposelist Handle Safety - MERGED
- Guard against disposed handles in `opdisposelist` to prevent startup segfault

## Earlier Achievements (February 1-7, 2026)

### Guest Database Lifecycle (fileMenu Verbs) - MERGED
- **PR #391**: Implement fileMenu verbs with v7 save format and db corruption fixes
- `fileMenu.open/close/closeall/save` implemented for headless mode
- Four corruption bugs fixed, 22 integration tests

### Startup Scripts & Menu System - MERGED
- **PRs #378, #382, #383-385, #387-390**: Startup scripts, path-based file verbs, menu system stabilization
- Full startup sequence operational in headless mode

### Compiler Warning Elimination - COMPLETE
- **PRs #372, #373**: Zero compiler warnings achieved

### Build & Distribution - MERGED
- **PRs #377, #381**: `make dist`, `--migrate` flag

### GUI Application Planning - DOCUMENTED
- Complete planning directory: `planning/gui/`
- Architecture, protocol, and all editor specifications documented

### Quality & Documentation - MERGED
- Logging demotions, result truncation removal, OPML test export

## Active Development Status

### Verb Implementation Coverage
- **Overall: 68% (482/710 verbs)**
- File verbs: 100% (86/86)
- String verbs: 100% (60/60)
- Lang verbs: 100% (61/61)
- Op verbs: 100% (45/45)
- Table verbs: 100% (18/18)
- Date verbs: 100% (30/30)
- DB verbs: 100% (13/13)
- **TCP verbs: 100% (23/23)** - COMPLETE
- **fileMenu verbs**: 7/10 implemented (open, close, closeall, save, saveAs, saveCopy, new) + 3 stubs
- Thread verbs: Cooperative threading operational -- `evaluate`, `callscript`, `getCurrentID`, `getCount`, `exists`, `kill`, `sleep`, `wake`, `getNthID` all working
- Many other processors complete (dialog, html, xml, sys, webserver, inetd, etc.)

Reference: `reports/coverage/verb-binding/2026-01-27-01.md`

### Integration Test Status
- **Current**: 1,920 tests total
- **Passed**: 1,713 (was 1,704)
- **Skipped**: 189
- **Failed**: **0**
- Execution: 8 workers, parallel batch mode, ~37 seconds
- NDJSON protocol mode eliminates ~210ms startup cost per test

All tests running via:
- `./tools/run_headless_tests.sh` - C unit tests
- `cd tests && make test-integration` - Integration tests (Python/YAML)
- `cd tests && make test-all` - Full suite

### Known P0 Issues

**Architectural Decisions:**
- **Issue #86** (P0): Global runtime context & lifecycle
  - Blocks: Concurrency model (#94), remote runtime (#97), EFP routing parity (#87)
  - Status: Design/decision needed

- **Issue #87** (P0): Headless EFP routing parity
  - Depends on: #86 (runtime context)
  - Status: Design/decision needed

- **Issue #88** (P0): Networking architecture & security
  - Status: Design/decision needed before broad CLI distribution
  - Note: TCP layer is secure; this is about HTTP-level security model

- **Issue #367** (P0): Cleanup project directory structure - move CLI stubs out of tests/

### Known Issues (New)

- **Issue #406**: Increase MAX_THREADS beyond legacy 64-thread limit
- **Issue #397**: opinitcallbacks is not idempotent -- unconditional calls cause segfaults

### Queued Work

**GUI Application** (Planning Complete - Ready for Implementation):
- Full planning documents in `planning/gui/`
- Protocol specification, all editor specs documented
- Native macOS application with documented API
- Third-party UI connection support
- Reference: planning/gui/ARCHITECTURE.md, planning/gui/PROTOCOL.md

**Phase 4 P0a: Global State Elimination** (Queued - Launch Blocking):
- Hash table context migration
- Parser state migration
- Control flow & error state cleanup
- Reference: planning/phase4/INDEX.md, planning/phase4/p0a-critical-thread-safety/README.md

**REPL Enhancements** (Future):
- Function persistence (requires code tree copying)
- Custom slash commands in `system.temp.FrontierREPL.commands`

## Next Steps (Priority Order)

### Immediate Priorities

1. **Startup Flow Stabilization -- First Run to Web Setup** (IMMEDIATE)
   - Goal: Full first-run experience from clean dist build
   - Run startup diagnostics, identify remaining failures, fix iteratively
   - Key deliverables: StartupTasks.root loads, mainResponder installs, manila installs, HTTP server starts, browser opens setupFrontier page
   - Reference: planning/phase4/STARTUP_STABILIZATION_PLAN.md

2. **Guest Database Save Verification**
   - Verify save works for system root AND guest databases
   - Verify databasedata elimination hasn't broken any save paths
   - End-to-end: open guest DB -> modify -> save -> reopen -> verify

3. **GUI Application Prototype** (after startup works)
   - Planning complete; begin with protocol layer + table browser
   - Native macOS app using specs in `planning/gui/`

4. **Threading Phase 2** (depends on P0a)
   - Cooperative threading foundation now in place (PR #404)
   - Next: increase MAX_THREADS (Issue #406)
   - Depends on Phase 4 P0a global state work for full thread safety

### Strategic Decisions Required

Before resuming major infrastructure work, need decisions on:
- Issue #86: Runtime context architecture
- Issue #88: HTTP-level security model (TCP is already secure)

## Reference Documentation

### Planning Documents
- **Phase 4 Overview**: planning/phase4/INDEX.md
- **Startup Stabilization**: planning/phase4/STARTUP_STABILIZATION_PLAN.md
- **Threading Plan**: planning/phase4/threading/README.md
- **Networking Plan**: planning/phase4/networking/INDEX.md
- **GUI Planning**: planning/gui/README.md
- **GUI Architecture**: planning/gui/ARCHITECTURE.md
- **GUI Protocol**: planning/gui/PROTOCOL.md
- **CRDT Foundation**: planning/phase6/CRDT_FOUNDATION_ROADMAP.md

### Implementation Guides
- **Getting Started**: docs/GETTING_STARTED.md
- **Verb Implementation**: docs/VERB_IMPLEMENTATION_GUIDE.md
- **Testing Guide**: docs/TESTING_GUIDE.md
- **CLI Usage**: docs/CLI_USAGE_GUIDE.md
- **Logging Standards**: docs/LOGGING_STANDARDS.md
- **Verb Resolution Architecture**: docs/VERB_RESOLUTION_ARCHITECTURE.md

### Architecture Decisions
- **ADR-002**: Context-Based Format Versioning
- **ADR-003**: Two-Phase Address Value Resolution
- **ADR-004**: Dynamic Verb Binding Architecture
- **ADR-005**: Parameter State Thread-Safety
- **ADR-010**: Three-Phase Thread Implementation Roadmap
- **ADR-013**: REPL Event Loop Architecture

### Progress Reports
- **Latest**: reports/progress/2026-02-27-databasedata-elimination-and-startup-stabilization.md (covers Feb 16-27)
- **Previous**: reports/progress/2026-02-16-threading-protocol-and-test-reliability.md (covers Feb 5-16)
- **Earlier**: reports/progress/2026-02-05-startup-scripts-menus-and-gui-planning.md (covers Feb 1-5)

### Historical Context
- **Status Archive**: planning/_STATUS_ARCHIVE.md (entries before 2026-01-27)
- **TODO Archive**: Historical completed work (see _CURRENT_TODO_LIST.md)
