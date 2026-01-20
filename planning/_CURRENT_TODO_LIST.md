# Frontier - Current TODO List

Status: In Progress (Updated 2026-01-19)

## 🚀 IMMEDIATE PRIORITY – Phase 4: Global State Elimination + TCP Networking

**Goal**: Begin Phase 4 P0a (Global State Elimination) and TCP Networking Phase 1A (Core Socket Implementation) - foundational work for Frontier's networking layer.

**Context**: Hierarchical OPML export complete (PR #326). Deterministic thread testing foundation complete (PR #318). Ready to start networking infrastructure.

---

## Large In-Flight Workstreams

### Workstream 1: Phase 4 Threading Foundation & Global State Elimination

**Status**: Phase 1 complete (PR #318), starting P0a

**Completed**:
- ✅ **Phase 1: Deterministic Thread Testing Foundation** (PR #318 merged)
  - Tickcount-based scheduling with millisecond precision
  - Test-controlled tickcount via environment variable
  - Thread ID assignment with collision detection
  - Comprehensive unit tests passing

**Active Work**:
- 🚀 **P0a (Weeks 1-6): Hash Table Context Migration** - LAUNCH BLOCKING
  - Migrate hash table operations from global state to explicit context
  - Thread-safe hash table access patterns
  - Reference: planning/phase4/INDEX.md
  - Timeline: ~2-3 weeks

**Upcoming**:
- **P0b (Weeks 7-12)**: Outline context migration, external object processing audit
- **P1a-P1b (Weeks 13-24)**: Multi-user collaborative ODB foundation
- **P2 (Weeks 25-36)**: Cleanup and optimization

**Reference Documentation**:
- planning/phase4/INDEX.md - Phase 4 roadmap overview
- planning/phase4/threading/README.md - POSIX threading plan (5 phases)
- planning/phase4/threading/HEADLESS_THREAD_VERBS_IMPLEMENTATION.md - Thread verb implementation details
- planning/CRDT_FOUNDATION_ROADMAP.md - Collaborative ODB foundation

---

### Workstream 2: TCP Networking Implementation

**Status**: Starting Phase 1A

**Active Work**:
- 🚀 **Phase 1A (Weeks 1-2): Core Socket Implementation**
  - Basic TCP socket verbs: tcp.open, tcp.close, tcp.send, tcp.receive
  - POSIX socket abstraction layer
  - Cross-platform socket handling (macOS, Linux)
  - Error handling and resource cleanup

**Upcoming**:
- **Phase 1B (Weeks 3-4)**: DNS resolution (tcp.resolvehostname, tcp.getaddress)
- **Phase 2 (Weeks 5-6)**: Buffered I/O (tcp.receiveline, tcp.sendline, tcp.setbuffer)
- **Phase 3 (Weeks 7-10)**: Server operations - REQUIRES THREADING FOUNDATION
  - tcp.listen, tcp.accept, tcp.setlistener (callback registration)
  - Depends on: Phase 4 P0a-P0b completion
- **Phase 4 (Weeks 11-14)**: Advanced features (timeouts, non-blocking, SSL/TLS)

**Reference Documentation**:
- planning/phase4/networking/INDEX.md - TCP networking roadmap
- planning/phase4/networking/SOCKET_IMPLEMENTATION_PLAN.md - Detailed socket implementation
- Issue #88 (P0): Networking architecture & security decisions

---

### Workstream 3: Verb Implementation Coverage

**Status**: Ongoing - 37% overall coverage (264/710 verbs)

**Coverage by Category**:
- ✅ File verbs: 100% (86/86) - COMPLETE
- 🔄 Lang verbs: 16% (ongoing)
- 🔄 String verbs: ~30% (in progress)
- 🔄 Table verbs: ~40% (in progress)
- 🔄 System verbs: ~25% (in progress)

**Active Work**:
- String verb bindings (string.length, string.mid, string.delete, string.insert, etc.)
- Table verb bindings (table.assign, table.goto, table.packtable, etc.)
- System verb bindings (sys.getenvironmentvariable, sys.setenvironmentvariable)

**Reference Documentation**:
- docs/VERB_IMPLEMENTATION_GUIDE.md - Verb implementation patterns
- tools/kernelverbs_parser/ - Verb coverage analysis tools
- planning/phase3/VERB_BINDING_QUICK_WINS.md - Quick win opportunities

---

### Workstream 4: Active Worktrees & Blocked Work

**Active Worktrees**:

1. **feature/table-sorting-and-settarget** (P1 - IN REVIEW)
   - Location: `/Users/jake/dev/jsavin/Frontier-table-sorting-and-settarget`
   - Branch: `feature/table-sorting-and-settarget` (1 commit ahead: 8d773bcd)
   - Status: Implementation complete, awaiting PR creation
   - Content:
     - Table sorting verbs: table.sortby(), table.getsortorder()
     - Per-table sort order stored in database
     - Target verbs: lang.gettarget(), lang.settarget(), lang.cleartarget()
     - Comprehensive test suite (8 test cases)
   - **Blocker**: Pre-existing UserTalk object test infrastructure build errors
   - **Next**: Fix test infrastructure, then create PR

2. **fix/usertalk-object-test-infrastructure** (P1 - NOT STARTED)
   - Location: `/Users/jake/dev/jsavin/Frontier-usertalk-object-test-infrastructure`
   - Branch: `fix/usertalk-object-test-infrastructure` (no commits yet)
   - Scope: Fix pre-existing build errors
   - Issues:
     - memory.c redefinitions (lockhandle, unlockhandle, etc.)
     - Undefined identifiers: chnul, chspace
   - **Impact**: Blocks table sorting PR from being tested
   - **Next**: Investigate and fix build errors

---

## P0 Issues - Critical Blockers (Architectural Decisions Required)

These block deployment and major system decisions. All require design/planning before implementation.

### Launch Blocking

- **Issue #86** (P0): Global runtime context & lifecycle
  - Scope: Large - impacts all concurrent CLI/runtime clients
  - Status: Design/decision needed
  - Blocks: #94 (concurrency model), #97 (remote runtime), #87 (EFP routing parity)
  - **Related to**: Phase 4 P0a global state elimination work

- **Issue #87** (P0): Headless EFP routing parity
  - Scope: Large - requires equivalence with UI-mode event flow
  - Depends on: #86 (runtime context)
  - Blocks: CLI stability, proper testing infrastructure

- **Issue #88** (P0): Networking architecture & security
  - Scope: Medium - HTTP/WebSocket scaffolding with secure defaults
  - Status: Design/decision needed
  - Timeline: Before broad CLI distribution
  - **Related to**: TCP Networking workstream (Phase 1A starting)

### Development Blockers

- **Issue #166** (P0): UserTalk integration tests for table context mutation tracking
  - Scope: Small - C + UserTalk integration testing
  - Status: Blocked on new() verb binding and table.getversion() verb implementation
  - Impact: Validates Phase 4A table_context_t works correctly from UserTalk
  - Timeline: Can proceed after new() and table.getversion() verbs are bound

- **Issue #84** (P0): Memory management audit (rolling)
  - Scope: Ongoing - continuous verification
  - Status: Active (completed in PR #137 for migration cleanup paths)
  - Note: Continue periodic audits as new code lands

---

## P1 Issues - High Priority (Recommended Work Order)

### Short-Term (1-2 weeks)

1. **Fix UserTalk Object Test Infrastructure** (~4-6 hours)
   - Fix memory.c redefinitions and undefined identifiers
   - Unblocks table sorting PR
   - Worktree: fix/usertalk-object-test-infrastructure

2. **Create PR for Table Sorting** (~2 hours)
   - Once test infrastructure fixed
   - Use pull-request agent for comprehensive PR
   - Worktree: feature/table-sorting-and-settarget

3. **Issue #121**: Implement 28 error stubs for remaining verbs (~3-4 hours)
   - Quick win - unblocks CLI testing
   - Files affected: Shell verb implementations in `Common/source/shell*.c`

### Medium-Term (2-4 weeks)

4. **Issue #135** (P1): Refactor outline (op) management from push/pop to deterministic context
   - Scope: Medium - Similar push/pop pattern to mode stack
   - LOE: ~3-5 days (multiple file changes)
   - Files affected: `Common/source/op.c`, op management throughout codebase
   - **Related to**: Phase 4 global state elimination

5. **Issue #136** (P1): Audit external object processing for push/pop anti-patterns
   - Scope: Medium - Code review + planning
   - LOE: ~1-2 days
   - Output: Planning doc listing all external object push/pop patterns

6. **Issue #138** (P1): Refactor dbendsaveas to make implicit disposal explicit
   - Scope: Small - Documentation + minor API improvement
   - LOE: ~4-6 hours
   - Files affected: `Common/source/db.c`, `Common/headers/db.h`

7. **Issue #140** (P1): Audit db_context_guard usage in database disposal paths
   - Scope: Small - Code review + verification
   - LOE: ~2-4 hours
   - Depends on: #138

### Testing & Infrastructure

8. **Issue #77** (P1): Add helper macros for BE pack/unpack in langhash
   - Scope: Medium - Code quality improvement
   - LOE: ~4-6 hours
   - Files affected: `Common/source/langhash.c`

9. **Issue #78** (P1): Cross-arch BE64 serialization verification
   - Scope: Medium - Adding regression tests with golden blobs
   - LOE: ~6-8 hours
   - Depends on: #77

10. **Issue #79** (P1): Add extended type bounds tests for hash unpack
    - Scope: Medium - Comprehensive testing
    - LOE: ~6-8 hours
    - Files affected: `tests/db_format_tests.c`

---

## P2 Issues - Nice to Have

### Code Cleanup

- **Issue #139** (P2): Cleanup path duplication (factor out repeated cleanup logic)
- **Issue #141** (P2): Final cleanup restructuring (consolidate cleanup patterns)
- **Issue #142** (P2): Error path test coverage (add tests for migration failures)
- **Issue #143** (P2): cleanup_migration_database error scenarios (add tests)

### Phase 1 Code Cleanup Completion (LOW RISK - ~1-2 hours)

- [ ] Remove obsolete platform code (~3 blocks remaining)
  - `oldMACVERSION` (3 blocks in langhash.c - v7 format doesn't use Mac aliases)
  - Commented `WIN95VERSION` blocks (already mostly cleaned)
  - ~50 lines total; quick cleanup

### Additional Follow-Ups

- **Issue #76** (P2): Add corruption/bounds tests for hash unpack
- **Issue #132** (P2): Investigate hash table disposal in unit test environment
- **Issue #73** (P2): Document magic sizes (path buffers, menu padding)

---

## Deferred / Design-Dependent (Phase 2+)

### Major Architectural Decisions

- **Issue #85** (P1): UI boundary via Ports & Adapters
  - Scope: Large - architectural refactor
  - Depends on: #86 (runtime context)
  - Status: Design/decision needed

- **Issue #89** (P1): OSA / IPC strategy
  - Scope: Large - Architectural decision
  - Blocks: UI/headless bridge, cross-process communication

- **Issue #90** (P1): File I/O & path policy
  - Scope: Large - Security/access control decisions
  - Blocks: File verb implementations, sandboxing model (#106)

- **Issue #91** (P1): Unicode strategy
  - Scope: Medium - Encoding/platform decisions
  - Blocks: Comprehensive verb porting (date/file/string verbs)

- **Issue #93** (P1): Hash table modernization
  - Scope: Large - Data structure refactor
  - Note: Post-1.0 work

- **Issue #94** (P1): Concurrency model & task contexts
  - Scope: Large - Runtime architecture
  - Depends on: #86 (global runtime context)
  - Blocks: Multi-threaded operation, background tasks

- **Issue #97** (P1): Remote runtime + local guest databases
  - Scope: Large - Multi-tier architecture
  - Depends on: #86 (runtime context), #88 (networking)
  - Timeline: Phase 2+

- **Issue #102** (P1): Developer experience improvements
  - Scope: Medium - Tooling & documentation
  - Status: Ongoing

- **Issue #105** (P1): Modernize file.getSystemFolderPath for multi-platform support
  - Scope: Medium - Cross-platform file system access
  - Depends on: #90 (file I/O & path policy)

- **Issue #106** (P1): Design permission/sandboxing model
  - Scope: Large - Security model
  - Depends on: #90 (file I/O & path policy)
  - Blocks: Daemon/service deployment

- **Issue #81** (P1): Unvendor temporary build dependencies (CMake + Paige)
  - Scope: Medium - Build system refactor
  - Status: Deferred until Phase 2 (post v6→v7 migration)

---

## Recently Completed (Last 2 Weeks)

### PR #326: Hierarchical OPML Export - MERGED ✅ (2026-01-19)
- Converted monolithic 14,002-line OPML to 26-file hierarchical structure
- OPML 2.0 transclusion with absolute GitHub URLs
- Flattened structure for Drummer compatibility
- All 1,247 integration tests preserved
- Commits: b151c674, 2afcb6c6, f9489d3f

### PR #318: Phase 1 - Deterministic Thread Testing Foundation - MERGED ✅ (2026-01-18)
- Tickcount-based thread scheduling with millisecond precision
- Test-controlled tickcount via FRONTIER_TEST_TICKCOUNT
- Thread ID collision detection and wraparound handling
- Comprehensive unit tests passing
- Documentation: planning/phase4/threading/HEADLESS_THREAD_VERBS_IMPLEMENTATION.md

---

## Reference Documentation

### Phase 4 Planning
- **planning/phase4/INDEX.md** - Phase 4 roadmap overview (global state elimination)
- **planning/phase4/threading/README.md** - POSIX threading implementation plan
- **planning/phase4/networking/INDEX.md** - TCP networking roadmap
- **planning/CRDT_FOUNDATION_ROADMAP.md** - Collaborative ODB foundation

### Implementation Guides
- **docs/VERB_IMPLEMENTATION_GUIDE.md** - Verb implementation patterns
- **docs/TESTING_GUIDE.md** - CLI usage, testing, database migration
- **docs/CLI_USAGE_GUIDE.md** - Complete frontier-cli reference
- **docs/LOGGING_STANDARDS.md** - Structured logging requirements

### Architecture Decisions
- **ADR-002**: Context-Based Format Versioning
- **ADR-003**: Two-Phase Address Value Resolution
- **ADR-004**: Dynamic Verb Binding Architecture
- **ADR-005**: Parameter State Thread-Safety

### Status & History
- **planning/_CURRENT_STATUS.md** - Recent achievements and current focus
- **planning/_STATUS_ARCHIVE.md** - Historical entries (before 2026-01-05)
