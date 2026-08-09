# Current Status

Last Updated: 2026-03-25

> ## ⚠️ Status correction (2026-08-09) — read this first
>
> **This document is stale as of 2026-03-25.** The direction of record is now:
>
> - `product/VISION_1_0.md` — the 1.0 vision and phase definitions
> - `product/plans/2026-08-09-phase-0-1-execution-plan.md` — the active execution plan (Phases 0–1)
>
> Everything below this banner is **historical** — a snapshot of March 2026 that has since been
> overtaken by merged work. Specific corrections applied inline below:
>
> - **TCP verbs**: 23/23 complete (PR #361), not 22 total with 13 done.
> - **Phase 4 P0a**: not "queued" — `currenthashtable` became thread-local in PR #536 (2026-04-15).
>   `hashtablestack` remains a global (bootstrap-ordering constraint); this split-brain caused bug #706.
> - **Integration test baseline**: not "0 failures". Current baseline is roughly **2186 passing /
>   21 known failures / 47 skipped** under umbrella issue #620 (eval-trap unmasking, PRs #618/#619).
>   The known-failures list currently lives only in `/tmp` — persisting it is scheduled in Phase 2
>   of the product execution plan.
>
> Do not treat the "Next Steps" section below as the current work queue.

## Current Focus: CLI Extensibility, Distribution Workflow & Test Gap Coverage

**Status** *(historical, 2026-03-25)*: Integration tests reported **0 failures** at the time (2,017 tests, 8-worker parallel execution in ~37s) — see the correction banner above for the current baseline. 302 unit tests. CLI arguments bridge to UserTalk via `system.environment.args`. Clean Virgin.root with `make clean-root` target. PSTRING compile-time validation active. ODB script editing workflow established via protocol.

**Latest Release**: **v1.0.0-alpha.7** (February 16, 2026)

**Verb Coverage**: **68% (482/710 verbs)** - TCP at 100%, all core processors complete. fileMenu verbs: 7/10 implemented (open, close, closeall, save, saveAs, saveCopy, new).

## Recent Achievements (March 13-25, 2026)

### CLI-to-UserTalk Argument Bridge (PRs #488, #489) - MERGED
- Any CLI flag now accessible from UserTalk scripts via `system.environment.args`
- Callback pattern preserves layering (Common does not depend on CLI)
- Two-pass argument parser: known flags separated from user-defined passthrough args
- `--browser agent-browser` routes `sys.openUrl` to AI browser agent for web testing

### sys.openUrl Kernel Verb (PR #490) - MERGED
- Registered `sys.openUrl` as kernel verb in headless build (previously only a glue script)
- Available before startup script runs for early-boot browser automation

### Clean Distribution Workflow (PRs #491, #493) - MERGED
- `userland.cleanRoot` works in headless mode with `realpath`-based path comparison
- Discovered and fixed: Virgin.root polluted with `user.databases` containing hardcoded absolute paths
- GUI verb no-ops for headless compatibility (clipboard, editmenu, window.quickScript, window.close)
- `make clean-root` target reproduces pre-release cleanup in one command

### BIGSTRING-to-PSTRING Audit (PR #492) - MERGED
- 37 hex-prefix string literals converted to PSTRING with compile-time length validation
- Caught 5 pre-existing wrong length bytes plus 3 additional during review

### Headless Startup Modernization (PR #486) - MERGED
- Modernized startup script flow for headless/CLI compatibility
- Moved `window.update` calls inside try blocks, fixed `--output v7` format detection

### ODB Script Editing Workflow - MERGED
- `script.newScriptObject` and `op.newOutlineObject` trim whitespace and normalize line endings
- Fixed trailing newline and double-indented comments in glue scripts
- Established Virgin.root as source-of-truth for ODB edits

### Integration Test Expansion (PRs #481, #483-#485) - MERGED
- +66 new integration tests: protocol ODB ops, persistence/save, webserver HTTP round-trip, error recovery, concurrency

## Earlier Achievements (February 28 - March 13, 2026)

### HTTP Server & mainResponder End-to-End (PRs #462-#480) - MERGED
- HTTP server starts via `inetd.startOne`, dispatches through GIL, serves pages via mainResponder
- Fixed GIL deadlock, database persistence, path handling, verb resolution, thread context corruption
- 10 distinct bugs across different subsystems fixed to complete first HTTP request pipeline

### Integration Test Reliability (PR #468) - MERGED
- Fixed 19 consistently-failing integration tests
- Workspace isolation across 12 test files (`workspace.*` migrated to `system.temp.*`)

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

**Corrected 2026-08-09** — the current baseline is approximately **2,186 passing / 21 known
failures / 47 skipped** under umbrella issue #620 (eval-trap unmasking, PRs #618/#619). The
known-failures baseline list currently lives only in `/tmp`; persisting it into the repo is
scheduled in Phase 2 of `product/plans/2026-08-09-phase-0-1-execution-plan.md`.

Historical March 2026 snapshot:

- **Total**: 2,017 tests
- **Passed**: 1,827 (was 1,761)
- **Skipped**: 190
- **Failed**: 0 *(no longer true — see correction above)*
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

**Phase 4 P0a: Global State Elimination** (Partially landed - Launch Blocking):
- Hash table context migration — **partially complete**: `currenthashtable` is thread-local as of
  PR #536 (2026-04-15); `hashtablestack` remains a global because bootstrap runs before
  `hthreadglobals` exists (macro commented out at `Common/headers/processinternal.h:295`). This
  split-brain caused bug #706.
- Parser state migration — not started
- Control flow & error state cleanup — not started
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
- **Latest**: reports/progress/2026-03-25-cli-extensibility-and-dist-workflow.md (covers Mar 13-25)
- **Previous**: reports/progress/2026-03-13-mainresponder-startup-and-http-serving.md (covers Feb 28-Mar 13)
- **Earlier**: reports/progress/2026-02-27-databasedata-elimination-and-startup-stabilization.md (covers Feb 16-27)

### Historical Context
- **Status Archive**: planning/_STATUS_ARCHIVE.md (entries before 2026-01-27)
- **TODO Archive**: Historical completed work (see _CURRENT_TODO_LIST.md)
