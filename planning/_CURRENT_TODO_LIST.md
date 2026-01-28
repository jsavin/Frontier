# Frontier - Current TODO List

Status: In Progress (Updated 2026-01-28)

## 🎉 Recently Completed Milestones

### TCP Verbs 100% Coverage - ✅ COMPLETE
**Resolution**: PR #361 merged (2026-01-28)
- Implemented 10 remaining TCP verbs (Phase 2 buffered I/O)
- Migrated all 22 TCP verb dispatches from legacy `fwsNetEvent*` to `tcp_*` API
- Removed 2,406 lines of legacy code (MacSocketNetEvents.c, WinSockNetEvents.h)
- Added security hardening: ARM64 type safety fix, slow-trickle DoS protection, O(n) pattern matching
- New issue #362 filed for configurable throughput window (P1)

### Issue #347 (P0): NULL context audit - ✅ CLOSED
**Resolution**: PR #360 merged (2026-01-28)
- Fixed 51 NULL context calls to *verbinmemory functions
- Added explicit `db_context` structs with hard assertions
- Narrowed context scope to block level for better code hygiene
- Bonus fix: Thread registry test calling non-existent function

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
**Scope**: Medium - HTTP/WebSocket scaffolding with secure defaults
**Timeline**: Before broad CLI distribution
**Related to**: TCP Networking workstream (Phase 1 complete, Phase 2 queued)

---

## 📊 Current Work Status

### Verb Coverage: 68% (491/720) ✅

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

### Integration Tests: 1,124+ passing

---

## 🚀 Tracer Bullet Milestones

### 1. Webserver "Hello World" E2E Test
**Status**: Not started
**Goal**: Prove the full networking stack works end-to-end
**What it validates**:
- TCP networking kernel verbs (tcp.listen, tcp.accept, etc.)
- inetd UserTalk implementation
- webserver UserTalk implementation
- Request → Handler → Response cycle

**Approach**:
1. Identify webserver/inetd code paths and required kernel verbs
2. Verify those verbs are implemented and have test coverage
3. Create minimal "Hello World" test case
4. Run E2E and fix any gaps discovered

### 2. system.startup.startupScript Analysis
**Status**: Not started
**Goal**: Ensure all critical-path kernel verbs for daemon mode are available
**Prerequisites for**: Long-running HTTP process, daemon mode
**What it validates**:
- Bootstrap sequence is understood
- All verbs in startup critical path are identified
- Gaps in kernel verb coverage are surfaced before they cause runtime failures

**Approach**:
1. Read and analyze startupScript UserTalk code
2. Trace dependencies (what it calls, including inetd/webserver init)
3. Cross-reference against verb coverage report
4. Create issue/task list for any missing verbs

---

## 🎯 Recommended Work Priority

### Tier 1: Tracer Bullets (Validate E2E)
1. **Webserver "Hello World"** - Prove networking stack works
   - Quick validation of existing implementation
   - Surfaces gaps before deeper investment

### Tier 2: Strategic Decisions (Gate Major Features)
2. **Resolve Issue #86** (Runtime context architecture)
   - Unblocks concurrency model, remote runtime, EFP parity
   - Required for Phase 4 P0a (global state elimination)
   - Major architectural decision

3. **Resolve Issue #88** (Networking security model)
   - Required before broad CLI distribution

### Tier 3: Major Feature Work (After Decisions)
4. **Phase 4 P0a** (Global State Elimination)
   - 3-week effort, launch blocking
   - Hash table context migration
   - Parser state migration
   - Control flow & error state cleanup
   - Depends on: Issue #86 resolution
   - Reference: planning/phase4/p0a-critical-thread-safety/README.md

### Tier 4: Quality & Stability
6. **P1 Bug Fixes** (As discovered)
   - Issue #339: Pascal string logging garbage
   - Issue #323: Thread-safe FD table initialization
   - Issue #332: ODB reference counting (foundational for threading)
   - See "P1 Issues" section below

### Tier 4: Ongoing Improvements
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

## 🎉 Recently Completed (Jan 25-28, 2026)

### TCP Verbs 100% Coverage
- **PR #361**: Implement 100% TCP verbs coverage and migrate from legacy API
  - 10 new verbs: statusStream, getPeerAddress, getPeerPort, myAddress, readStreamUntil, readStreamBytes, readStreamUntilClosed, writeStringToStream, writeFileToStream, getStats
  - Removed 2,406 lines legacy code (MacSocketNetEvents.c, WinSockNetEvents.h)
  - Security: ARM64 ioctl type fix, slow-trickle DoS protection (35s throughput window), O(n) pattern matching
  - Filed Issue #362 for configurable throughput window (P1)

### P0 Blocker Resolution
- **PR #360**: Fix 51 NULL context calls to *verbinmemory functions (Issue #347)

### Critical Bug Fixes
- **PR #359**: Fix defined() error suppression (Issue #325) - langerrormessage() now respects error suppression state
- **PR #358**: Suppress v6→v7 migration log spew during REPL startup
- **PR #356**: Add Option+Arrow word navigation to REPL
- **PR #354**: Enable nested parentOf() function calls (fixed crash)
- **PR #352**: Remove explicit EFP table search to fix introspection bugs
- **PR #351**: Exclude script-only processors from EFP whitelist
- **PR #353**: Fix Intel Mac compatibility with universal cmake binary
- **PR #348**: Keep database open after successful hydration

### Documentation & Infrastructure
- **PR #350**: Add Getting Started guide
- **PR #345**: Support positional .root/.root7 arguments in CLI
- **PR #344**: Update planning docs to reflect TCP Phase 1 completion

### Earlier Completions (Jan 19-24)
See planning/_STATUS_ARCHIVE.md for:
- TCP Networking Phase 1A/1B/3 (PRs #327, #329, #330)
- Thread Registry & Testing Foundation (PRs #317, #318)
- Database Migration & Path Resolution Fixes (PRs #336, #337, #342)
- Infrastructure Improvements (PRs #338, #340, #343, #326)

---

## 📚 Reference Documentation

### Planning Documents
- **Phase 4 Overview**: planning/phase4/INDEX.md
- **Threading Plan**: planning/phase4/threading/README.md
- **Networking Plan**: planning/phase4/networking/INDEX.md
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

### Status & History
- **Current Status**: planning/_CURRENT_STATUS.md
- **Status Archive**: planning/_STATUS_ARCHIVE.md (historical achievements)

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
- **Current focus**: Stability, correctness, bug fixes
- **Verb coverage**: 68% (491/720) - massive progress from 37% in early January
- **Test health**: 1,124+ integration tests passing
- **P0 blockers resolved**: Issue #347 (NULL context audit) closed via PR #360
- **Next major milestones**:
  1. Make architectural decisions (#86, #88)
  2. Resume feature work (Phase 4 P0a)

### Workstream Status
- **TCP Networking**: ✅ 100% COMPLETE (23/23 verbs) - PR #361 merged
- **Threading**: Phase 1 foundation complete (11 verbs), P0a queued
- **Verb Porting**: Ongoing - 68% coverage achieved
- **Bug Fixes**: Active - recent focus on verb resolution and REPL improvements
- **Documentation**: Strong - comprehensive guides in place
