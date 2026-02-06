# Frontier - Current TODO List

Status: In Progress (Updated 2026-02-05)

## Recently Completed Milestones

### Startup Scripts & Path-Based File Verbs - ✅ MERGED (Feb 1-5)
**Resolution**: PRs #378, #382, #389 merged
- `system.startup.startupScript` execution enabled in headless mode
- Path-based file verbs operational
- Startup warnings #2 and #3 fixed (menupack and startup script)
- Enables daemon-mode workflows

### Menu System Stabilization - ✅ MERGED (Feb 2-5)
**Resolution**: PRs #383, #384, #385, #387, #388, #390 merged
- Proper `menubarType` data access for headless mode
- Comprehensive menu integration tests added
- V6 menu loading fixed during migration (byte-swap linkage)
- Disk struct fields fixed to `int32_t` for 64-bit safety
- `op.outlineToXml` fixed for headless mode via window verb stubs
- Mbar value copying fixed — added `menuverbcopyvalue()` with proper v6/v7 format detection
- `langexternalcopyvalue()` now dispatches `idmenuprocessor` to dedicated copy path
- Static assertions added; disk struct anti-pattern documented

### Compiler Warning Elimination - ✅ COMPLETE (Feb 1)
**Resolution**: PRs #372, #373 merged
- Phase 1: 154 → 24 warnings (84% reduction)
- Phase 2: 24 → 0 warnings (100% elimination)
- Zero compiler warnings achieved

### Build & Distribution - ✅ MERGED (Feb 1)
**Resolution**: PRs #377, #381 merged
- `make dist` target for legacy-compatible distribution
- `--migrate` flag for standalone database migration
- Default Makefile target now builds `all` (with dist)

### GUI Application Planning - ✅ DOCUMENTED (Feb 2-5)
**Resolution**: 8 specification documents in `planning/gui/`
- Architecture, protocol, and all editor specifications complete
- Table browser, script editor, outline editor, menu editor, wptext editor, console
- External types and editor mapping documented

### Quality Improvements - ✅ MERGED (Feb 1-5)
- Removed 255-byte result truncation in CLI and REPL output
- Demoted noisy logging (system table snapshots, menu loading, caught try errors)
- OPML test export with pass/skip/fail statistics
- Window verb stub documentation improved

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

---

## Current Work Status

### Verb Coverage: 68% (482/710)

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

### Integration Tests: 1,802 total (up from 1,777)
- 152 skipped
- New tests for menus, mbar value copy, path resolution, startup scripts

---

## Next Milestones

### 1. GUI Application Prototype
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

### 2. Startup Script Hardening
**Status**: Startup scripts working, needs validation
**Goal**: Ensure all critical-path kernel verbs for daemon mode are available
**Prerequisites for**: Long-running HTTP process, daemon mode
**What it validates**:
- Bootstrap sequence is understood
- All verbs in startup critical path are identified
- Gaps in kernel verb coverage are surfaced

### 3. REPL Function Persistence
**Status**: Known limitation, future work
**Goal**: Allow function definitions to persist across evaluations
**Challenge**: Code values (codevaluetype) require special handling for tree copying
**Current Behavior**: Functions defined but not callable in subsequent evaluations

---

## Recommended Work Priority

### Tier 1: GUI Application (User-Facing Value)
1. **GUI Prototype Implementation**
   - Protocol layer in frontier-cli
   - Table browser as first editor
   - Specs ready in `planning/gui/`

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
   - Launch blocking
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

## P1 Issues - High Priority

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

## Recently Completed (Feb 1-5, 2026)

### Startup Scripts & Menus
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
- **Current focus**: GUI application prototype, startup script hardening
- **Planning complete**: Full GUI specs documented (architecture, protocol, all editors)
- **Verb coverage**: 68% (482/710) - all core processors complete
- **Test health**: 1,802 integration tests (152 skipped)
- **Latest release**: v1.0.0-alpha.4 (January 31, 2026)
- **Compiler warnings**: Zero (fully eliminated)

### Workstream Status
- **Startup Scripts**: ✅ WORKING - Boot-time execution in headless mode
- **Menu System**: ✅ STABILIZED - Migration, headless access, value copying, integration tests
- **GUI Planning**: ✅ COMPLETE - Full specs ready for implementation
- **Webserver**: ✅ WORKING - Full web application layer functional
- **REPL**: ✅ TRANSFORMED - Persistent variables, navigation, event loop
- **TCP Networking**: ✅ 100% COMPLETE (23/23 verbs)
- **Compiler Warnings**: ✅ ELIMINATED - Zero warnings
- **Build/Dist**: ✅ IMPROVED - make dist, --migrate flag
- **Threading**: Phase 1 foundation complete (11 verbs), P0a queued
- **GUI Implementation**: Ready to begin - planning complete
- **Documentation**: Strong - comprehensive guides and GUI specs in place
