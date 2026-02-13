# Frontier - Current TODO List

Status: In Progress (Updated 2026-02-10)

## Recently Completed Milestones

### Cooperative Threading Infrastructure - ✅ MERGED (Feb 10)
**Resolution**: PR #404 merged
- Thread registry with main thread registration and iteration
- Cooperative globals save/restore isolates C globals per-thread
- Fire-and-forget semantics: scriptError in spawned thread stays contained
- `thread.evaluate()`, `callscript()`, `getCurrentID()`, `getCount()`, `exists()`, `kill()`, `sleep()`, `wake()`, `getNthID()` all operational
- 10 integration tests + 19 unit tests (all passing)
- Filed issue #406 (increase MAX_THREADS beyond 64)

### Startup Hang & BIGSTRING Fix - ✅ MERGED (Feb 10)
**Resolution**: PR #403 merged
- Fixed 3 wrong BIGSTRING length prefixes making `string.innerCaseName`, `macRomanToUtf8`, `utf8ToMacRoman` unreachable
- Root cause chain: unreachable verb → error → semaphore not unlocked → 2-hour busy-wait
- Added defensive `langreleaseallsemaphores` auto-cleanup after startup/REPL execution

### Callback Infrastructure Fix - ✅ MERGED (Feb 10)
**Resolution**: PR #402 merged
- Fixed segfault (undefined `langnewtable` symbol), double-free crashes, "too many parameters" errors
- All 14 callback tests now pass (was: 1 pass + segfault + 12 failures)

### Dist Startup Stability - ✅ MERGED (Feb 10)
**Resolution**: PR #401 merged
- Restored `langexternalsetdatabase()` (was no-op, breaking cross-database hdatabase assignment)
- Fixed `getoutlinefromtarget()` for menu externals (was interpreting menu record as packed outline)
- NULL guards for `param1` in `langfunctioncall()` and 8 outline traversal functions
- Eliminated 6,800+ lines of PACK diagnostic noise per save

### Guest Database Context Fix - ✅ MERGED (Feb 8)
**Resolution**: PR #400 merged
- Op verbs now use variable database context instead of system root for guest DB operations

### window.isOpen() - ✅ MERGED (Feb 7-8)
**Resolution**: PRs #398, #399 merged
- Address path and file path resolution against system root and guest DBs
- Unblocks `Frontier.openDataFile()`; raises script errors for path failures

### Portable fileloop & Outline Callbacks - ✅ MERGED (Feb 7)
**Resolution**: PR #396 merged
- POSIX fileloop implementation, fixed `macfilespecisvalid` stub, NULL callback safety
- Startup script now completes successfully

### fileMenu.saveAs/saveCopy - ✅ MERGED (Feb 7)
**Resolution**: PR #394 merged
- Save-then-copy approach for both system root and guest databases
- Restored `fldatabasesaveas` guard in menu persistence
- 28 filemenu integration tests total; closes issues #392 and #395

### Guest Database Lifecycle (fileMenu Verbs) - ✅ MERGED (Feb 5-7)
**Resolution**: PR #391 merged
- `fileMenu.open/close/closeall/save` implemented for headless mode
- Four database corruption bugs fixed
- 22 integration tests (all passing)

### Earlier Completions (Feb 1-5)
- **Startup Scripts & Path-Based File Verbs**: PRs #378, #382, #389
- **Menu System Stabilization**: PRs #383-385, #387-388, #390
- **Compiler Warning Elimination**: PRs #372, #373 (zero warnings)
- **Build & Distribution**: PRs #377, #381 (`make dist`, `--migrate`)
- **GUI Application Planning**: 8 specification docs in `planning/gui/`
- **Quality Improvements**: Logging demotions, result truncation removal, OPML export

---

## P0 Architectural Decisions (Strategic - Block Launch)

These require design/planning before implementation can proceed.

### Issue #86 (P0): Global runtime context & lifecycle
**Status**: Design/decision needed
**Scope**: Large - impacts all concurrent CLI/runtime clients
**Blocks**:
- Issue #94 (concurrency model)
- Issue #97 (remote runtime)
- Issue #87 (EFP routing parity)
- Phase 4 P0a (global state elimination)

**Context**: Related to Phase 4 P0a global state elimination work

### Issue #87 (P0): Headless EFP routing parity
**Status**: Design/decision needed
**Scope**: Large - requires equivalence with UI-mode event flow
**Depends on**: Issue #86 (runtime context)
**Impact**: CLI stability, proper testing infrastructure

### Issue #88 (P0): Networking architecture & security
**Status**: Design/decision needed
**Scope**: Medium - HTTP-level security model
**Timeline**: Before broad CLI distribution
**Note**: TCP layer is already secure; this is about HTTP-level policies

### Issue #367 (P0): Cleanup project directory structure
**Status**: Open
**Scope**: Medium - move CLI stubs out of tests/

---

## Current Work Status

### Verb Coverage: 68% (482/710)

**Complete Processors** (100%):
- base64, clock, crypt, date, db, dialog, file, html, inetd
- kb, lang, launch, mainwindow, math, op, point, rectangle
- rgb, script, search, semaphore, string, sys, table, target
- **tcp**, webserver, xml

**Partial Processors**:
- thread: Cooperative threading operational — `evaluate`, `callscript`, `getCurrentID`, `getCount`, `exists`, `kill`, `sleep`, `wake`, `getNthID` all working
- searchengine: 20% (1/5)
- filemenu: 60% (6/10) - open, close, closeall, save, saveAs, saveCopy implemented; 4 stubs remain (Issue #393: new, revert, print, quit)

**Not Started** (0%):
- bit, clipboard, dll, editmenu, frontier, htmlcontrol
- menu, mouse, mrcalendar, mysql, opattributes, osa, pict
- python, re, rez, speaker, sqlite, statusbar, window

Reference: `reports/coverage/verb-binding/2026-01-27-01.md`

### Integration Tests: ~1,830+ total (up from 1,804)
- ~1,617+ passed (non-skip), ~172 skipped, ~39 failed (pre-existing)
- New tests: cooperative threading (10), fileMenu saveAs/saveCopy (7), window.isOpen (10+), string verb reachability, callback infrastructure (14 unit tests fixed)

---

## Next Milestones

### 1. Startup Script Hardening / Dist Stability
**Status**: Startup completes, dist mode stable across multiple runs
**Goal**: Validate all critical-path kernel verbs for daemon mode
**Recent progress**: Startup hang fixed (PR #403), dist crashes fixed (PR #401), fileloop working (PR #396)
**What remains**:
- Long-running HTTP process testing
- Identify and fix any remaining unreachable kernel verbs (BIGSTRING audit)
- Validate daemon-mode workflows end-to-end

### 2. GUI Application Prototype
**Status**: Planning complete - ready for implementation
**Goal**: Native macOS application communicating with frontier-cli
**What's Ready**:
- Full planning directory: `planning/gui/`
- Protocol specification (JSON over stdin/stdout or socket)
- All editor specifications (table browser, script, outline, menu, wptext, console)
- Architecture with multi-user model, authentication, federation

**Approach**:
1. Implement protocol layer in frontier-cli
2. Build table browser (core ODB navigation)
3. Add script editor with debugging
4. Iterate on remaining editors

### 3. Threading Phase 2
**Status**: Cooperative foundation in place (PR #404)
**Goal**: Real POSIX concurrency with proper thread safety
**Depends on**: Phase 4 P0a (global state elimination), Issue #86 (runtime context)
**Next steps**:
- Increase MAX_THREADS beyond 64 (Issue #406)
- Move from cooperative to real concurrent execution
- Thread-safe FD table initialization (Issue #323)

### 4. REPL Function Persistence
**Status**: Known limitation, future work
**Goal**: Allow function definitions to persist across evaluations
**Challenge**: Code values (codevaluetype) require special handling for tree copying
**Current Behavior**: Functions defined but not callable in subsequent evaluations

---

## Recommended Work Priority

### Tier 1: Stability & Distribution (Immediate Value)
1. **Startup Script Hardening**
   - Validate daemon-mode workflows
   - Long-running HTTP process testing
   - BIGSTRING audit for other unreachable verbs

### Tier 2: GUI Application (User-Facing Value)
2. **GUI Prototype Implementation**
   - Protocol layer in frontier-cli
   - Table browser as first editor
   - Specs ready in `planning/gui/`

### Tier 3: Strategic Decisions (Gate Major Features)
3. **Resolve Issue #86** (Runtime context architecture)
   - Unblocks concurrency model, remote runtime, EFP parity
   - Required for Phase 4 P0a (global state elimination)
   - Major architectural decision

4. **Resolve Issue #88** (HTTP security model)
   - Required before broad CLI distribution
   - TCP is secure; need HTTP-level policies

### Tier 4: Major Feature Work (After Decisions)
5. **Phase 4 P0a** (Global State Elimination)
   - Launch blocking
   - Hash table context migration
   - Parser state migration
   - Control flow & error state cleanup
   - Depends on: Issue #86 resolution
   - Reference: planning/phase4/p0a-critical-thread-safety/README.md

### Tier 5: Quality & Stability
6. **P1 Bug Fixes** (As discovered)
   - Issue #339: Pascal string logging garbage
   - Issue #323: Thread-safe FD table initialization
   - Issue #332: ODB reference counting (foundational for threading)
   - See "P1 Issues" section below

### Tier 6: Ongoing Improvements
7. **Verb Coverage Expansion** (Ongoing)
   - Complete remaining fileMenu stubs (4 remaining: new, revert, print, quit)
   - Implement processors at 0%: bit, clipboard, mysql, sqlite, etc.

---

## P1 Issues - High Priority

### Testing & Quality
- **Issue #357** (P2): Add integration tests for REPL word navigation
- **Issue #346** (P2): Add C unit tests for CLI extension detection
- **Issue #339** (P1): Pascal string logging displays garbage characters
- **Issue #333** (P2): Investigate integration test timing issue (TCP echo test)

### Runtime Correctness
- **Issue #397** (P1): opinitcallbacks is not idempotent — unconditional calls cause segfaults
- **Issue #355** (P2): Memory leak in evaluatereadonlyparam dereferenceop case
- **Issue #332** (P1): Implement ODB reference counting for hash tables across threads
  - Scope: Large, status: decision needed
  - Related to Phase 4 threading work

### Code Quality & Refactoring
- **Issue #380** (P1): Expand bigstring max length beyond 255 characters
- **Issue #379** (P1): Standardize error reporting pattern to use langerrormessage() everywhere
- **Issue #334** (P2): Optimize defined() and metadata verbs to avoid loading external tables
- **Issue #331** (P1): Implement UserTalk linter to validate syntax and enforce style
- **Issue #308** (P1): Systematic type handling cleanup for Phase 2.0
  - Scope: Large, comprehensive coercion and operation semantics
- **Issue #307** (P1): Audit ostypevaluetype coercion and arithmetic operations
- **Issue #305** (P1): Complete hashtablestack macro migration after bootstrap refactoring

### Guest Database / fileMenu
- **Issue #393** (P1): Implement remaining fileMenu stub verbs for headless mode (new, revert, print, quit)

### Threading
- **Issue #406**: Increase MAX_THREADS beyond legacy 64-thread limit

### Feature Implementations
- **Issue #323** (P1): File portable: Add thread-safe FD table initialization
- **Issue #322** (P2): Thread test harness: Add mutex protection for race conditions
- **Issue #320** (P2): Implement robust sys.getapppath() with platform-specific detection
- **Issue #319** (P2): Implement robust sys.appisrunning() with cross-platform detection
- **Issue #312** (P2): Implement cross-platform file locking support
- **Issue #299** (P2): Implement proper process management verbs for headless mode

### Build & Infrastructure
- **Issue #376**: Worktree setup requires manual copy of third_party/Paige/build-headless
- **Issue #374**: Enable stricter compiler warning flags (-Wconversion, -Wsign-conversion, -Wcast-qual)

### User Experience
- **Issue #316** (P2): Consider directing REPL prompt to stderr for cleaner stdout piping
  - Status: Deferred

---

## Recently Completed (Feb 7-10, 2026)

### Cooperative Threading & Stability (Feb 7-10)
- **PR #404**: Add cooperative threading with thread registry for headless mode
- **PR #403**: Fix startup hang caused by wrong BIGSTRING length prefixes
- **PR #402**: Fix callback infrastructure segfault and test failures
- **PR #401**: Fix dist startup crashes (second run segfault and log spew)
- **PR #400**: Use variable database context in getoutlinefromtarget()
- **PR #399**: Raise script errors for realpath failures in window.isOpen
- **PR #398**: Implement window.isOpen() for headless mode
- **PR #396**: Implement portable fileloop and fix outline callback crashes
- **PR #394**: Implement fileMenu.saveAs/saveCopy and restore fldatabasesaveas guard

### Guest Database Lifecycle (Feb 5-7)
- **PR #391**: Implement fileMenu verbs with v7 save format and db corruption fixes

### Startup Scripts & Menus (Feb 1-5)
- **PR #390**: Fix menubar (mbar) value copying for headless mode
- **PR #389**: Fix startup warnings #2 and #3 (menupack and startup script)
- **PR #388**: Fix op.outlineToXml for headless mode
- **PR #387**: Fix int32_t for disk struct fields on 64-bit
- **PR #385**: Fix V6 menu loading during migration and byte-swap linkage
- **PR #384**: Comprehensive menu integration tests for headless mode
- **PR #383**: Add proper menubarType data access for headless mode
- **PR #382**: Startup scripts and path-based file verbs
- **PR #381**: Add make dist target for legacy-compatible distribution
- **PR #378**: Enable system.startup.startupScript execution in headless mode
- **PR #377**: Add --migrate flag for standalone database migration

### Compiler & Quality
- **PR #373**: Eliminate remaining 24 compiler warnings (Phase 2)
- **PR #372**: Reduce compiler warnings from 154 to 24 (84% reduction)

### Earlier Completions (Jan 25-31)
See planning/_STATUS_ARCHIVE.md for:
- Webserver & inetd working (PRs #366, #363)
- REPL transformation (PRs #371, #370, #368)
- TCP verbs 100% coverage (PR #361)
- NULL context audit (PR #360)
- Error suppression fix (PR #359)

---

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
- **Verb Resolution**: docs/VERB_RESOLUTION_ARCHITECTURE.md
- **Debugging**: docs/DEBUGGING_GUIDE.md

### Architecture Decisions
- **ADR-002**: Context-Based Format Versioning
- **ADR-003**: Two-Phase Address Value Resolution
- **ADR-004**: Dynamic Verb Binding Architecture
- **ADR-005**: Parameter State Thread-Safety
- **ADR-010**: Three-Phase Thread Implementation Roadmap
- **ADR-013**: REPL Event Loop Architecture

### Status & History
- **Current Status**: planning/_CURRENT_STATUS.md
- **Status Archive**: planning/_STATUS_ARCHIVE.md (historical achievements)
- **Progress Reports**: reports/progress/

---

## Deferred / Design-Dependent (Phase 2+)

### Major Architectural Decisions (Not Blocking Current Work)
- **Issue #85** (P1): UI boundary via Ports & Adapters (depends on #86)
- **Issue #89** (P1): OSA / IPC strategy
- **Issue #90** (P1): File I/O & path policy
- **Issue #91** (P1): Unicode strategy
- **Issue #93** (P1): Hash table modernization (post-1.0)
- **Issue #94** (P1): Concurrency model & task contexts (depends on #86)
- **Issue #97** (P1): Remote runtime + local guest databases (depends on #86, #88)
- **Issue #102** (P1): Developer experience improvements (ongoing)
- **Issue #105** (P1): Modernize file.getSystemFolderPath (depends on #90)
- **Issue #106** (P1): Design permission/sandboxing model (depends on #90)
- **Issue #81** (P1): Unvendor temporary build dependencies (Phase 2+)

### P2 Code Cleanup
- **Issue #139** (P2): Cleanup path duplication
- **Issue #141** (P2): Final cleanup restructuring
- **Issue #142** (P2): Error path test coverage
- **Issue #143** (P2): cleanup_migration_database error scenarios
- **Issue #76** (P2): Add corruption/bounds tests for hash unpack
- **Issue #132** (P2): Investigate hash table disposal in unit test environment
- **Issue #73** (P2): Document magic sizes (path buffers, menu padding)

---

## Notes

### Strategic Context
- **Current focus**: Startup hardening, dist stability, GUI prototype, threading Phase 2
- **Planning complete**: Full GUI specs documented (architecture, protocol, all editors)
- **Verb coverage**: 68% (482/710) - all core processors complete
- **Test health**: ~1,830+ integration tests (~39 pre-existing failures)
- **Latest release**: v1.0.0-alpha.4 (January 31, 2026)
- **Compiler warnings**: Zero (fully eliminated)

### Workstream Status
- **Cooperative Threading**: ✅ OPERATIONAL - Registry, globals isolation, fire-and-forget semantics
- **Startup Scripts**: ✅ WORKING - Full startup completes, hang fixed, semaphore cleanup added
- **Dist Mode**: ✅ STABLE - Multi-run stability, cross-database assignment fixed
- **Menu System**: ✅ STABILIZED - Migration, headless access, value copying, integration tests
- **GUI Planning**: ✅ COMPLETE - Full specs ready for implementation
- **Webserver**: ✅ WORKING - Full web application layer functional
- **REPL**: ✅ TRANSFORMED - Persistent variables, navigation, event loop
- **TCP Networking**: ✅ 100% COMPLETE (23/23 verbs)
- **Compiler Warnings**: ✅ ELIMINATED - Zero warnings
- **Build/Dist**: ✅ IMPROVED - make dist, --migrate flag
- **Callback Infrastructure**: ✅ FIXED - All 14 tests passing (was segfaulting)
- **GUI Implementation**: Ready to begin - planning complete
- **Documentation**: Strong - comprehensive guides and GUI specs in place
