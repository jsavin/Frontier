# Frontier - Current TODO List

Status: In Progress (Updated 2026-01-24)

## 🚀 IMMEDIATE PRIORITY – TCP Networking Phase 2 (Buffered I/O)

**Strategic Decision**: Complete TCP Phase 1A-1B-3 BEFORE Phase 4 P0a (Global State Elimination)

**Status Update (2026-01-24)**:
- ✅ **TCP Phase 1A COMPLETE** (PR #327) - Core socket operations (6 verbs)
- ✅ **TCP Phase 1B COMPLETE** (PR #330) - DNS and address handling (5 verbs)
- ✅ **TCP Phase 3 COMPLETE** (PR #330) - Server-side operations (2 verbs)
- 🚀 **TCP Phase 2 STARTING** - Buffered I/O (4 verbs) → HTTP client milestone
- ⏸️ **Phase 4 P0a QUEUED** - Weeks 7-9 (after TCP Phase 2)

**Rationale (Validated)**:
- ✅ **No P0a dependency confirmed** - TCP Phase 1A-1B-3 completed without P0a
- ✅ **No rework needed** - TCP verbs use hash table APIs with identical signatures
- ✅ **Faster validation** - Working HTTP client in 6 weeks (vs 9 weeks if P0a first)
- ✅ **Same total effort** - TCP implementation identical whether done before or after P0a
- ✅ **Server ops complete** - Phase 3 merged alongside 1A/1B (listener infrastructure ready)

**Dependency Analysis (Confirmed)**:
```
TCP Phase 1A (Core sockets)      → ✅ COMPLETE (no P0a dependency)
TCP Phase 1B (DNS/address)       → ✅ COMPLETE (no P0a dependency)
TCP Phase 3 (Server operations)  → ✅ COMPLETE (listener thread infrastructure works)
TCP Phase 2 (Buffered I/O)       → 🚀 NEXT (no P0a dependency)
Phase 4 P0a (Thread-safety)      → ⏸️ QUEUED (weeks 7-9)
```

**Execution Sequence (Updated)**:
1. ✅ **Weeks 1-2**: TCP Phase 1A (Core sockets) - 6 verbs → Basic connectivity
2. ✅ **Weeks 3-4**: TCP Phase 1B (DNS/address) - 5 verbs → Name resolution
3. ✅ **Weeks 3-4**: TCP Phase 3 (Server operations) - 2 verbs → Listener infrastructure
4. 🚀 **Weeks 5-6**: TCP Phase 2 (Buffered I/O) - 4 verbs → **HTTP client works** 🎯
5. ⏸️ **Weeks 7-9**: Phase 4 P0a (Hash table thread-safety) → Launch blocking work
6. **Weeks 10-13**: TCP Phase 4 (Advanced features) - Optional enhancements

**Context**: TCP Phase 1A/1B/3 complete and merged (PRs #327, #329, #330). Thread registry infrastructure complete (PR #317). Database migration fixes complete (PRs #336, #337, #342). Ready for TCP Phase 2 (buffered I/O).

---

## Strategic Timeline (Next 13 Weeks)

**Optimized for**: Fastest path to working HTTP client, then launch readiness

| Weeks | Phase | Status | Milestone |
|-------|-------|--------|-----------|
| **1-2** | TCP Phase 1A (Core Sockets) | ✅ DONE | Basic connectivity |
| **3-4** | TCP Phase 1B (DNS) | ✅ DONE | Name resolution |
| **3-4** | TCP Phase 3 (Server ops) | ✅ DONE | Listener infrastructure |
| **5-6** | TCP Phase 2 (Buffered I/O) | 🚀 NEXT | **HTTP client works** 🎯 |
| **7-9** | Phase 4 P0a (Hash table thread-safety) | Queued | Launch blocking complete |
| **10-12** | Phase 4 P0b (System context) | Queued | **LAUNCH READY** 🚀 |
| **13+** | TCP Phase 4 (Advanced features) | Queued | Optional enhancements |

**Key Dependencies (Updated)**:
- TCP Phase 1A-1B-3 **COMPLETE** without P0a dependency (validated approach)
- TCP Phase 2 **independent** from P0a (client-side buffered I/O)
- P0a-P0b **queued** for weeks 7-12 (launch blocking work)

**Why This Order (Validated)**:
1. ✅ TCP Phase 1A-1B-3 complete → Server infrastructure ready (weeks 1-4)
2. 🚀 TCP Phase 2 next → HTTP client working in 6 weeks (week 6 milestone)
3. ⏸️ P0a-P0b after → Launch readiness in 12 weeks (weeks 7-12)
4. TCP Phase 4 last → Advanced features (optional, weeks 13+)

---

## Large In-Flight Workstreams

### Workstream 1: Phase 4 Threading Foundation & Global State Elimination

**Status**: Phase 1 complete (PR #318), Thread Registry complete (PR #317), P0a QUEUED for weeks 7-9

**Completed**:
- ✅ **Phase 1: Deterministic Thread Testing Foundation** (PR #318 merged 2026-01-18)
  - Tickcount-based scheduling with millisecond precision
  - Test-controlled tickcount via FRONTIER_TEST_TICKCOUNT
  - Thread ID assignment with collision detection
  - 10 integration tests, 11 of 17 thread verbs working
  - ADR-010 documenting three-phase implementation roadmap

- ✅ **Thread Registry Infrastructure** (PR #317 merged 2026-01-22)
  - Thread ID allocation and tracking
  - Per-thread ID lookup with collision detection
  - Foundation for eliminating global mutable state
  - Pattern proven for thread-local storage migration

**Queued Work** (starts Week 7, after TCP Phase 2):
- ⏸️ **P0a (Weeks 7-9): Hash Table Context Migration** - LAUNCH BLOCKING
  - Priority changed: TCP Phase 2 first (HTTP client milestone)
  - Migrate hash table operations from global state to thread-local
  - Week 1: Hash table context (currenthashtable, hmagictable) - 🟡 Sonnet
  - Week 2: Parser state (yylval, yyval, langparser_result) - 🟡 Sonnet
  - Week 3: Control flow & error state (flbreak, flcontinue) - 🟢 Haiku
  - Reference: planning/phase4/p0a-critical-thread-safety/README.md

**Why Queued**:
- ✅ TCP Phase 1A-1B-3 completed without P0a (validated approach)
- 🚀 TCP Phase 2 next priority (HTTP client milestone in week 6)
- ⏸️ P0a starts week 7 (launch blocking work, 3-week effort)
- Optimizes for faster HTTP client validation milestone (week 6 vs week 9)

**Upcoming** (after P0a):
- **P0b (Weeks 10-12)**: System context migration, shell config thread-safety
- **P1a-P1b (Weeks 13-18)**: Multi-user collaborative ODB foundation
- **P2 (Weeks 19-24)**: Comprehensive cleanup and optimization

**Reference Documentation**:
- planning/phase4/INDEX.md - Phase 4 roadmap overview
- planning/phase4/threading/README.md - POSIX threading plan (5 phases)
- planning/phase4/threading/HEADLESS_THREAD_VERBS_IMPLEMENTATION.md - Thread verb implementation details
- planning/CRDT_FOUNDATION_ROADMAP.md - Collaborative ODB foundation

---

### Workstream 2: TCP Networking Implementation

**Status**: Phases 1A/1B/3 COMPLETE ✅, Starting Phase 2 🚀

**Completed Phases**:

**Phase 1A: Core Socket Primitives (Weeks 1-2)** - ✅ MERGED (PR #327)
- **Verbs Implemented** (6 total):
  1. `tcp.openAddrStream(addr, port)` → streamID - Direct IP connection
  2. `tcp.openNameStream(hostname, port)` → streamID - DNS + connect wrapper
  3. `tcp.readStream(stream, bytes)` → data - Non-blocking recv
  4. `tcp.writeStream(stream, data)` → true - Blocking send
  5. `tcp.closeStream(stream)` → true - Graceful shutdown (FIN)
  6. `tcp.abortStream(stream)` → true - Immediate close (RST)
  7. `tcp.countConnections()` → count - Active stream count
- **Implementation**: `Common/source/tcpverbs.c` (710 lines), POSIX BSD sockets
- **Thread Safety**: Mutex protection for stream registry
- **Tests**: 28 C unit tests (PR #329)

**Phase 1B: Address & DNS Handling (Weeks 3-4)** - ✅ MERGED (PR #330)
- **Verbs Implemented** (5 total):
  1. `tcp.addressEncode(ipString)` → addr - "192.168.1.1" → 3232235777
  2. `tcp.addressDecode(addr)` → ipString - 3232235777 → "192.168.1.1"
  3. `tcp.nameToAddress(hostname)` → addr - DNS forward lookup
  4. `tcp.addressToName(addr)` → hostname - Reverse DNS lookup
- **Purpose**: General-purpose IP utilities for network applications
- **Tests**: 23 integration tests, all passing

**Phase 3: Server Operations (Weeks 3-4)** - ✅ MERGED (PR #330)
- **Verbs Implemented** (2 total):
  1. `tcp.listenStream(address, port, callback, table)` → listenerID - Accept connections
  2. `tcp.closeListen(listenerID)` → true - Stop listening
- **Infrastructure**: Listener registry, per-listener accept threads, callback dispatch
- **Impact**: Transforms Frontier from network client to server platform
- **Tests**: 34 integration tests

**Test Suite (PR #329)** - ✅ MERGED
- 28 C unit tests (stream lifecycle, thread-safety, address encoding, security)
- 29 local integration tests (verb functionality)
- 20 network integration tests (actual connectivity, optional)
- Security features validated (SSRF/DNS rebinding protection)

**Current Phase: 2 - Buffered I/O (Weeks 5-6)** - 🚀 STARTING

**Model**: 🟢 **Haiku** - Pattern-following implementation

**Verbs to Implement** (4 total):
1. `tcp.flushStream(stream)` → true - Flush write buffer
2. `tcp.setStreamBuffer(stream, size)` → true - Configure buffer size
3. `tcp.getStreamBuffer(stream)` → size - Query buffer size
4. `tcp.drainStream(stream)` → true - Read and discard buffered data

**Milestone**: **HTTP client works** 🎯 (week 6)

**Upcoming Phases**:
- **Phase 4 (Weeks 11-14)**: Advanced features (optional enhancements)

**Why This Sequence (Validated)**:
- ✅ TCP Phase 1A-1B-3 completed without P0a (approach validated)
- 🚀 TCP Phase 2 next (HTTP client milestone in week 6)
- ⏸️ P0a queued for weeks 7-9 (launch blocking work)
- No rework needed (verb APIs unchanged before/after P0a)

**Reference Documentation**:
- planning/phase4/networking/INDEX.md - TCP networking roadmap
- planning/phase4/networking/IMPLEMENTATION_PLAN.md - Detailed execution plan (~450 lines)
- planning/phase4/networking/TCP_VERBS_ANALYSIS.md - Complete API analysis (22 verbs)
- planning/phase4/networking/NETWORKING_ARCHITECTURE.md - POSIX C architecture (~1100 lines)
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

## Recently Completed (Jan 19-24, 2026)

### TCP Networking Phase 1A/1B/3 - MERGED ✅
- **PR #327**: Phase 1A core socket operations (6 verbs, 710 lines C)
- **PR #329**: Comprehensive test suite (93 total tests)
- **PR #330**: Phase 1B address/DNS handling (5 verbs) + Phase 3 server operations (2 verbs)
- **Impact**: Production-ready TCP networking layer supporting client-server architecture
- **Milestone**: Network server platform ready, foundation for HTTP client and server applications

### Database Migration & Path Resolution Fixes - MERGED ✅
- **PR #336**: System.paths migration fix (address value corruption, table overwriting)
- **PR #337**: Path entry name matching fix (`defined(webserver)` behavior)
- **PR #342**: Builtins priority fix (`defined(webserver.init)` regression)
- **Impact**: Critical path resolution now works correctly, lookups find complete tables
- **Tests**: 1,124+ integration tests passing

### Thread Registry & Testing Foundation - MERGED ✅
- **PR #317**: Thread registry infrastructure (thread ID allocation, collision detection)
- **PR #318**: Deterministic thread testing foundation (10 integration tests, 11 of 17 verbs working)
- **Impact**: Infrastructure for POSIX thread safety and testing before launch
- **Documentation**: ADR-010 three-phase implementation roadmap

### Infrastructure Improvements - MERGED ✅
- **PR #338**: Modular context architecture (CLAUDE.md reduced from 1,033 to 937 lines)
- **PR #340**: /doit workflow integration (Frontier-specific agent mappings)
- **PR #343**: CLI documentation (600+ lines comprehensive reference)
- **PR #326**: Hierarchical OPML export (14,002-line file → 26-file structure)
- **PRs #324, #321**: Build fixes, file descriptor table initialization
- **Impact**: Cleaner documentation, better workflow, reduced technical friction

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
