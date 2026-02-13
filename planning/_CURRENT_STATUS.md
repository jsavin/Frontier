# Current Status

Last Updated: 2026-02-10

## Current Focus: Cooperative Threading, Dist Stability & Startup Hardening

**Status**: Cooperative threading infrastructure operational with thread registry and globals save/restore. Distribution mode fully stable (multi-run). Startup script completes without hang. fileMenu verbs complete (including saveAs/saveCopy). GUI application planning complete.

**Latest Release**: **v1.0.0-alpha.4** (January 31, 2026)

**Verb Coverage**: **68% (482/710 verbs)** - TCP at 100%, all core processors complete. fileMenu verbs: 6/10 implemented (open, close, closeall, save, saveAs, saveCopy).

## Recent Achievements (February 7-10, 2026)

### Cooperative Threading Infrastructure - ✅ MERGED
- **PR #404**: Add cooperative threading with thread registry for headless mode
- Thread registry with `register_main_thread()`, `get_nth_thread_id()` for iterating active threads
- Cooperative globals save/restore (`headless_save_threadglobals`/`headless_restore_threadglobals`) isolates C globals (fllangerror, flreturn, flbreak, flcontinue, flscriptrunning, etc.)
- Main thread gets ID 2 (`idapplicationthread`), spawned threads start at 3+
- `scriptError()` in spawned thread stops that thread only — fire-and-forget semantics
- `thread.evaluate()`, `thread.callscript()`, `thread.getCurrentID()`, `getCount()`, `exists()`, `kill()`, `sleep()`, `wake()`, `getNthID()` all operational
- 10 integration tests + 19 unit tests (all passing)
- Filed issue #406 (increase MAX_THREADS beyond 64)

### Startup Hang Fix - ✅ MERGED
- **PR #403**: Fix startup hang caused by wrong BIGSTRING length prefixes
- Fixed 3 wrong BIGSTRING prefixes in `headless_string_verbs.c` making `string.innerCaseName`, `string.macRomanToUtf8`, `string.utf8ToMacRoman` unreachable
- Root cause: `uninstallSubMenu.ut` called unreachable verb → error → semaphore not unlocked → `installSubMenu` busy-wait for 2 hours
- Added defensive `langreleaseallsemaphores` auto-cleanup after startup script and REPL execution
- Verified with macOS `sample` command (832/832 samples in `locksemaphoreverb` busy-wait)

### Callback Infrastructure Fix - ✅ MERGED
- **PR #402**: Fix callback infrastructure segfault and test failures
- Replaced undefined `langnewtable` symbol (NULL crash) with `tablenewtablevalue`
- Fixed double-free crashes: deep-copy parameter values with `exemptfromtmpstack`
- Fixed "too many parameters" errors in callback param passing
- Added `fllangerror = false` reset in TEST macro to prevent error cascade
- All 14 callback tests now pass (was: 1 pass, segfault, 12 failures)

### Dist Startup Stability - ✅ MERGED
- **PR #401**: Fix dist startup crashes (second run segfault and log spew)
- Restored `langexternalsetdatabase()` (was turned into a no-op, breaking cross-database hdatabase assignment)
- Fixed `getoutlinefromtarget()` for menu externals — was interpreting `tysavedmenuinfo` as packed outline
- Added NULL guard for `param1` in `langfunctioncall()` for corrupt/uninitialized code trees
- Added NULL safety to 8 outline traversal functions in `opvisit.c`
- Downgraded PACK diagnostic logging (eliminated 6,800+ lines of noise per save)

### Guest Database Context Fix - ✅ MERGED
- **PR #400**: Use variable database context in `getoutlinefromtarget()`
- Fixed op verbs reading from system root instead of guest DB — caused segfault in `oprecursivelyvisit()`

### window.isOpen() Implementation - ✅ MERGED
- **PR #398**: Implement `window.isOpen()` for headless mode
- Path A (address): checks if address resolves to root table of any opened database
- Path B (string/file path): compares paths using `realpath()` normalization
- **PR #399**: Follow-up — raise script errors for `realpath()` failures instead of silent false
- 10 integration tests (all passing)

### Portable fileloop & Outline Callback Fixes - ✅ MERGED
- **PR #396**: Implement portable fileloop and fix outline callback crashes
- POSIX `opendir`/`readdir`/`closedir` fileloop implementation replacing stubs
- Fixed `macfilespecisvalid` stub, NULL callback pointer crashes in outline operations
- Startup script now completes successfully

### fileMenu.saveAs/saveCopy - ✅ MERGED
- **PR #394**: Implement `fileMenu.saveAs(path)` and `fileMenu.saveCopy(path)`, restore `fldatabasesaveas` guard
- Save-then-copy approach for both system root and guest databases
- 28 filemenu integration tests total; closes issues #392 and #395

### opdisposelist Handle Safety - ✅ MERGED
- Guard against disposed handles in `opdisposelist` to prevent startup segfault

## Earlier Achievements (February 1-7, 2026)

### Guest Database Lifecycle (fileMenu Verbs) - ✅ MERGED
- **PR #391**: Implement fileMenu verbs with v7 save format and db corruption fixes
- `fileMenu.open/close/closeall/save` implemented for headless mode
- Four corruption bugs fixed, 22 integration tests

### Startup Scripts & Menu System - ✅ MERGED
- **PRs #378, #382, #383-385, #387-390**: Startup scripts, path-based file verbs, menu system stabilization
- Full startup sequence operational in headless mode

### Compiler Warning Elimination - ✅ COMPLETE
- **PRs #372, #373**: Zero compiler warnings achieved

### Build & Distribution - ✅ MERGED
- **PRs #377, #381**: `make dist`, `--migrate` flag

### GUI Application Planning - ✅ DOCUMENTED
- Complete planning directory: `planning/gui/`
- Architecture, protocol, and all editor specifications documented

### Quality & Documentation - ✅ MERGED
- Logging demotions, result truncation removal, OPML test export

## Active Development Status

### Verb Implementation Coverage
- **Overall: 68% (482/710 verbs)** ✅
- File verbs: 100% (86/86) ✅
- String verbs: 100% (60/60) ✅
- Lang verbs: 100% (61/61) ✅
- Op verbs: 100% (45/45) ✅
- Table verbs: 100% (18/18) ✅
- Date verbs: 100% (30/30) ✅
- DB verbs: 100% (13/13) ✅
- **TCP verbs: 100% (23/23)** ✅ - COMPLETE
- **fileMenu verbs**: 6/10 implemented (open, close, closeall, save, saveAs, saveCopy) + 4 stubs
- Thread verbs: Cooperative threading operational — `evaluate`, `callscript`, `getCurrentID`, `getCount`, `exists`, `kill`, `sleep`, `wake`, `getNthID` all working
- Many other processors complete (dialog, html, xml, sys, webserver, inetd, etc.)

Reference: `reports/coverage/verb-binding/2026-01-27-01.md`

### Integration Test Status
- **Current**: ~1,830+ tests total (up from 1,804)
- **Passed**: ~1,617+ non-skip
- **Skipped**: ~172
- **Failed**: ~39 (all pre-existing)
- New tests: cooperative threading (10), fileMenu saveAs/saveCopy (7), window.isOpen (10+), string verb reachability, callback infrastructure (14 unit tests fixed)

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
- **Issue #397**: opinitcallbacks is not idempotent — unconditional calls cause segfaults

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

1. **Startup Script Hardening / Dist Stability**
   - Startup script now completes, dist mode runs stably across multiple runs
   - Continue validating critical-path kernel verbs for daemon mode
   - Long-running HTTP process testing

2. **GUI Application Prototype**
   - Planning is complete; begin prototype implementation
   - Start with table browser and protocol layer
   - Native macOS app using specs in `planning/gui/`

3. **Threading Phase 2**
   - Cooperative threading foundation now in place (PR #404)
   - Next: real POSIX concurrency, increase MAX_THREADS (Issue #406)
   - Depends on Phase 4 P0a global state work for full thread safety

### Strategic Decisions Required

Before resuming major infrastructure work, need decisions on:
- Issue #86: Runtime context architecture
- Issue #88: HTTP-level security model (TCP is already secure)

## Reference Documentation

### Planning Documents
- **Phase 4 Overview**: planning/phase4/INDEX.md
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
- **Latest**: reports/progress/2026-02-05-startup-scripts-menus-and-gui-planning.md (covers Feb 1-5)
- **Previous**: reports/progress/2026-01-25-networking-foundation-and-thread-safety.md

### Historical Context
- **Status Archive**: planning/_STATUS_ARCHIVE.md (entries before 2026-01-27)
- **TODO Archive**: Historical completed work (see _CURRENT_TODO_LIST.md)
