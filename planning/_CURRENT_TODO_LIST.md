# Frontier - Current TODO List

Status: In Progress (Updated 2026-01-31)

## 🎉 Recently Completed Milestones

### Webserver & inetd Working - ✅ RELEASED (v1.0.0-alpha.4)
**Resolution**: PRs #366, #363 merged (2026-01-31)
- `webserver.init()` and `inetd.supervisor(true)` functional in headless mode
- Full HTTP request/response handling via UserTalk callbacks
- Fixed `grabthreadglobals()` to return success in headless mode
- Initialized Frontier verbs in headless `sysinitverbs()`
- Integration tests validate full request/response cycle

### REPL Transformation - ✅ RELEASED (v1.0.0-alpha.4)
**Resolution**: PRs #371, #370, #368 merged (2026-01-31)
- **Persistent Variables**: Variables survive across evaluations in `system.temp.FrontierREPL.variables`
- **Navigation Commands**: `/jump` and `/list` for database exploration
- **Event Loop**: Non-blocking REPL with concurrent TCP callback processing
- **Focus Tracking**: Current location in `system.temp.FrontierREPL.focus`
- **Tab Completion**: Path completion for `/list` command

### TCP Verbs 100% Coverage - ✅ COMPLETE
**Resolution**: PR #361 merged (2026-01-28)
- All 23 TCP verbs implemented
- Migrated from legacy `fwsNetEvent*` to `tcp_*` API
- Removed 2,406 lines of legacy code
- Security hardening: ARM64 type safety, DoS protection

### Issue #347 (P0): NULL context audit - ✅ CLOSED
**Resolution**: PR #360 merged (2026-01-28)
- Fixed 51 NULL context calls to *verbinmemory functions
- Added explicit `db_context` structs with hard assertions

---

## 📋 P0 Architectural Decisions (Strategic - Block Launch)

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

---

## 📊 Current Work Status

### Verb Coverage: 68% (482/710) ✅

**Complete Processors** (100%):
- base64, clock, crypt, date, db, dialog, file, html, inetd
- kb, lang, launch, mainwindow, math, op, point, rectangle
- rgb, script, search, semaphore, string, sys, table, target
- **tcp**, webserver, xml

**Partial Processors**:
- thread: 64% (11/17) - Phase 1 foundation complete
- searchengine: 20% (1/5)

**Not Started** (0%):
- bit, clipboard, dll, editmenu, filemenu, frontier, htmlcontrol
- menu, mouse, mrcalendar, mysql, opattributes, osa, pict
- python, re, rez, speaker, sqlite, statusbar, window

Reference: `reports/coverage/verb-binding/2026-01-27-01.md`

### Integration Tests: 1,698 total (1,451 passing)

---

## 🚀 Next Milestones

### 1. GUI Application Development
**Status**: In Planning
**Goal**: Native macOS application with documented API for third-party connections
**What's Ready**:
- Planning documents in `docs/planning/gui/`
- CLI proves the runtime works end-to-end
- Webserver validates complex subsystem integration

**Approach**:
1. Finalize GUI architecture document
2. Document connection API for third-party apps
3. Build native macOS prototype
4. Iterate based on real usage

### 2. REPL Function Persistence
**Status**: Known limitation, future work
**Goal**: Allow function definitions to persist across evaluations
**Challenge**: Code values (codevaluetype) require special handling for tree copying
**Current Behavior**: Functions defined but not callable in subsequent evaluations

### 3. system.startup.startupScript Analysis
**Status**: Not started
**Goal**: Ensure all critical-path kernel verbs for daemon mode are available
**Prerequisites for**: Long-running HTTP process, daemon mode
**What it validates**:
- Bootstrap sequence is understood
- All verbs in startup critical path are identified
- Gaps in kernel verb coverage are surfaced

---

## 🎯 Recommended Work Priority

### Tier 1: GUI Application (User-Facing Value)
1. **GUI Architecture Finalization**
   - Document API for third-party connections
   - Design native macOS application structure
   - Begin prototype implementation

### Tier 2: Strategic Decisions (Gate Major Features)
2. **Resolve Issue #86** (Runtime context architecture)
   - Unblocks concurrency model, remote runtime, EFP parity
   - Required for Phase 4 P0a (global state elimination)
   - Major architectural decision

3. **Resolve Issue #88** (HTTP security model)
   - Required before broad CLI distribution
   - TCP is secure; need HTTP-level policies

### Tier 3: Major Feature Work (After Decisions)
4. **Phase 4 P0a** (Global State Elimination)
   - 3-week effort, launch blocking
   - Hash table context migration
   - Parser state migration
   - Control flow & error state cleanup
   - Depends on: Issue #86 resolution
   - Reference: planning/phase4/p0a-critical-thread-safety/README.md

### Tier 4: Quality & Stability
5. **P1 Bug Fixes** (As discovered)
   - Issue #339: Pascal string logging garbage
   - Issue #323: Thread-safe FD table initialization
   - Issue #332: ODB reference counting (foundational for threading)
   - See "P1 Issues" section below

### Tier 5: Ongoing Improvements
6. **Verb Coverage Expansion** (Ongoing)
   - Complete remaining thread verbs (6 remaining)
   - Implement processors at 0%: bit, clipboard, mysql, sqlite, etc.

---

## 🔍 P1 Issues - High Priority

### Testing & Quality
- **Issue #357** (P2): Add integration tests for REPL word navigation
- **Issue #346** (P2): Add C unit tests for CLI extension detection
- **Issue #339** (P1): Pascal string logging displays garbage characters
- **Issue #333** (P2): Investigate integration test timing issue (TCP echo test)

### Runtime Correctness
- **Issue #355** (P2): Memory leak in evaluatereadonlyparam dereferenceop case
- **Issue #332** (P1): Implement ODB reference counting for hash tables across threads
  - Scope: Large, status: decision needed
  - Related to Phase 4 threading work

### Code Quality & Refactoring
- **Issue #334** (P2): Optimize defined() and metadata verbs to avoid loading external tables
- **Issue #331** (P1): Implement UserTalk linter to validate syntax and enforce style
- **Issue #308** (P1): Systematic type handling cleanup for Phase 2.0
  - Scope: Large, comprehensive coercion and operation semantics
- **Issue #307** (P1): Audit ostypevaluetype coercion and arithmetic operations
- **Issue #305** (P1): Complete hashtablestack macro migration after bootstrap refactoring

### Feature Implementations
- **Issue #323** (P1): File portable: Add thread-safe FD table initialization
- **Issue #322** (P2): Thread test harness: Add mutex protection for race conditions
- **Issue #320** (P2): Implement robust sys.getapppath() with platform-specific detection
- **Issue #319** (P2): Implement robust sys.appisrunning() with cross-platform detection
- **Issue #312** (P2): Implement cross-platform file locking support
- **Issue #299** (P2): Implement proper process management verbs for headless mode

### User Experience
- **Issue #316** (P2): Consider directing REPL prompt to stderr for cleaner stdout piping
  - Status: Deferred

---

## 🎉 Recently Completed (Jan 25-31, 2026)

### Webserver & inetd
- **PR #366**: Enable webserver Hello World in headless mode
- **PR #363**: Webserver Hello World initial implementation

### REPL Transformation
- **PR #371**: Persistent variables, focus tracking, tab completion
- **PR #370**: Navigation commands (`/jump`, `/list`)
- **PR #368**: Event loop architecture and display improvements

### TCP & Bug Fixes
- **PR #361**: 100% TCP verbs coverage and legacy API migration
- **PR #360**: Fix 51 NULL context calls (Issue #347)
- **PR #359**: Fix defined() error suppression (Issue #325)

### Earlier Completions (Jan 19-28)
See planning/_STATUS_ARCHIVE.md for:
- TCP Networking Phase 1A/1B/3 (PRs #327, #329, #330)
- Thread Registry & Testing Foundation (PRs #317, #318)
- Database Migration & Path Resolution Fixes (PRs #336, #337, #342)
- REPL improvements (PRs #356, #358)
- Infrastructure Improvements (PRs #338, #340, #343, #326)

---

## 📚 Reference Documentation

### Planning Documents
- **Phase 4 Overview**: planning/phase4/INDEX.md
- **Threading Plan**: planning/phase4/threading/README.md
- **Networking Plan**: planning/phase4/networking/INDEX.md
- **GUI Architecture**: docs/planning/gui/ARCHITECTURE.md
- **CRDT Foundation**: planning/CRDT_FOUNDATION_ROADMAP.md

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

## 🗑️ Deferred / Design-Dependent (Phase 2+)

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

## 📝 Notes

### Strategic Context
- **Current focus**: GUI application development, documentation
- **Major milestone achieved**: Webserver works, REPL transformed
- **Verb coverage**: 68% (482/710) - all core processors complete
- **Test health**: 1,698 integration tests (1,451 passing)
- **Latest release**: v1.0.0-alpha.4 (January 31, 2026)

### Workstream Status
- **Webserver**: ✅ WORKING - Full web application layer functional
- **REPL**: ✅ TRANSFORMED - Persistent variables, navigation, event loop
- **TCP Networking**: ✅ 100% COMPLETE (23/23 verbs)
- **Threading**: Phase 1 foundation complete (11 verbs), P0a queued
- **GUI**: In planning - next major focus
- **Documentation**: Strong - comprehensive guides in place
