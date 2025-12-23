# Carbon Migration / Runtime Modernization – Active TODO

Status: In Progress (Updated 2025-12-22)
Owner: Codex

## Approved Architectural Decisions

**Code Cleanup Strategy (IFDEF Cleanup - from IFDEF_CLEANUP_STRATEGY.md)**
- ✅ **PIKE variant removal**: APPROVED - Remove all 29 PIKE ifdefs (Frontier-only codebase, Pike was separate product)
  - Implementation: Phase 4, Week 13 of ifdef cleanup roadmap
  - Files affected: progressbar.c and 28 others
  - Impact: Simplifies codebase significantly
  - Update docs: Remove Pike references from README.md, build documentation

- ✅ **Optional database backends**: APPROVED - Keep as optional compile-time features (MySQL, SQLite, Python)
  - Current: FRONTIER_MYSQL, FRONTIER_SQLITE, FRONTIER_PYTHON (3 patterns each, 8 total blocks)
  - Approach: Compile-time flags via Makefile/CMake configuration
  - Note: If we want to support Python integration or other optional backends going forward, will need architectural rethinking to avoid ifdef proliferation
  - Action: Document in build system instead of hardcoding

---

## P0s – Critical Blockers (Architectural Decisions Required)
These block deployment and major system decisions. All require design/planning before implementation.

- **Issue #86**: P0: Global runtime context & lifecycle
  - Scope: Large - impacts all concurrent CLI/runtime clients
  - Status: Design/decision needed
  - Blocks: #94 (concurrency model), #97 (remote runtime), #87 (EFP routing parity)

- **Issue #87**: P0: Headless EFP routing parity
  - Scope: Large - requires equivalence with UI-mode event flow
  - Depends on: #86 (runtime context)
  - Blocks: CLI stability, proper testing infrastructure

- **Issue #85**: P0: UI boundary via Ports & Adapters
  - Scope: Large - architectural refactor
  - Depends on: #86 (runtime context)
  - Status: Design/decision needed

- **Issue #84**: P0: Memory management audit (rolling)
  - Scope: Ongoing - continuous verification
  - Status: Active (completed in PR #137 for migration cleanup paths)
  - Note: Continue periodic audits as new code lands

- **Issue #88**: P0: Networking architecture & security
  - Scope: Medium - HTTP/WebSocket scaffolding with secure defaults
  - Status: Design/decision needed, deferred until Phase 2
  - Timeline: Before broad CLI distribution
  - Reference: `planning/1.0_phase1_cli_implementation_plan.md`

## P1s – High Priority (Recommended Work Order)

### Phase 1: Mode Stack Refactor Prerequisites (Blocks continued architecture work)
**Recommended order:** Do these before starting mode stack refactor Phase 1

- **Issue #135** (P1): Refactor outline (op) management from push/pop to deterministic context model
  - Scope: Medium - Similar push/pop pattern to mode stack
  - LOE: ~3-5 days (multiple file changes, similar refactor pattern to mode stack)
  - Interdependency: Same push/pop anti-pattern as mode stack; fixing now prevents future bugs
  - Files affected: `Common/source/op.c`, op management throughout codebase
  - Related issue: #136 (audit external object processing)

- **Issue #136** (P1): Audit external object processing for push/pop anti-patterns
  - Scope: Medium - Code review + planning
  - LOE: ~1-2 days
  - Dependency: Complements #135; identifies all similar patterns
  - Output: Planning doc listing all external object push/pop patterns and refactor plan

### Phase 2: PR #137 Follow-Ups (Code quality & documentation)

- **Issue #138** (P1): Refactor dbendsaveas to make implicit disposal explicit
  - Scope: Small - Documentation + minor API improvement
  - LOE: ~4-6 hours
  - Files affected: `Common/source/db.c`, `Common/headers/db.h`
  - Note: Add clear documentation about internal `dbdispose()` call; prevents future double-free bugs

- **Issue #140** (P1): Audit db_context_guard usage in database disposal paths
  - Scope: Small - Code review + verification
  - LOE: ~2-4 hours
  - Files affected: `Common/source/db.c`, `Common/source/db_format.c`
  - Depends on: #138 (context for disposal patterns)
  - Output: Confirmation that context guards have been properly removed from disposal paths

### Phase 3: Quick Wins & Verb Porting

- **Issue #121** (P1): Implement 28 error stubs for remaining verbs
  - Scope: Quick win - 3-4 hours
  - Files affected: Shell verb implementations in `Common/source/shell*.c`
  - Impact: Unblocks CLI testing by stubbing error returns instead of crashes
  - Note: Can be done in parallel with other work

- **Issue #122** (P1): Implement TCP/Socket abstraction layer for networking verbs
  - Scope: Medium - 6-8 hours
  - Depends on: #88 (networking architecture decision - may block)
  - Status: Decision-dependent; defer until architecture P0 resolved

### Phase 4: Testing & Infrastructure (Supporting mode stack refactor)

- **Issue #77** (P1): Add helper macros for BE pack/unpack in langhash
  - Scope: Medium - Code quality improvement
  - LOE: ~4-6 hours
  - Files affected: `Common/source/langhash.c`, new helpers in `Common/headers/db_format.h`
  - Impact: Reduces manual memcpy repetition, improves maintainability
  - Related: Supports Issues #78, #79 (testing follow-ups)

- **Issue #78** (P1): Cross-arch BE64 serialization verification
  - Scope: Medium - Adding regression tests with golden blobs
  - LOE: ~6-8 hours
  - Depends on: #77 (BE helpers)
  - Output: Procedural + file-based regression tests for x86_64/arm64 compatibility
  - Impact: Ensures v7 database format portability across architectures

- **Issue #79** (P1): Add extended type bounds tests for hash unpack (lists, tables, records)
  - Scope: Medium - Comprehensive testing
  - LOE: ~6-8 hours
  - Files affected: `tests/db_format_tests.c`
  - Impact: Catches edge cases in deserialization before they hit production
  - Related: Issue #76 (corruption tests), #75 (hash hardening PR)

- **Issue #132** (P1): Investigate hash table disposal in unit test environment
  - Scope: Small - Debugging + verification
  - LOE: ~2-4 hours
  - Impact: Ensures test harness doesn't leak handles between tests

- **Issue #73** (P1): Document magic sizes (path buffers, menu padding)
  - Scope: Small - Documentation
  - LOE: ~2-3 hours
  - Output: Adds comments/docs to `Common/headers/shell.h` and related files
  - Impact: Improves maintainability, prevents size mistakes in future refactors

### Phase 5: Major Architectural Decisions (Decision-Dependent, Blocks other work)

- **Issue #89** (P1): OSA / IPC strategy
  - Scope: Large - Architectural decision
  - Blocks: UI/headless bridge, cross-process communication
  - Status: Decision needed
  - Related docs: `planning/TODO_future_improvements.md`

- **Issue #90** (P1): File I/O & path policy
  - Scope: Large - Security/access control decisions
  - Blocks: File verb implementations, sandboxing model (#106)
  - Status: Decision needed
  - Dependency: Related to #106 (permission/sandboxing)

- **Issue #91** (P1): Unicode strategy
  - Scope: Medium - Encoding/platform decisions
  - Blocks: Comprehensive verb porting (date/file/string verbs)
  - Status: Decision needed
  - Note: Likely affects multiple verb families

- **Issue #93** (P1): Hash table modernization
  - Scope: Large - Data structure refactor
  - Blocks: Long-term performance improvements
  - Status: Design phase
  - Note: Post-1.0 work

- **Issue #94** (P1): Concurrency model & task contexts
  - Scope: Large - Runtime architecture
  - Depends on: #86 (global runtime context)
  - Status: Design/decision needed
  - Blocks: Multi-threaded operation, background tasks

- **Issue #97** (P1): Remote runtime + local guest databases
  - Scope: Large - Multi-tier architecture
  - Depends on: #86 (runtime context), #88 (networking)
  - Status: Design/decision needed
  - Timeline: Phase 2+

- **Issue #102** (P1): Developer experience improvements
  - Scope: Medium - Tooling & documentation
  - Status: Ongoing
  - Examples: Better error messages, improved debugging support

- **Issue #105** (P1): Modernize file.getSystemFolderPath for multi-platform support
  - Scope: Medium - Cross-platform file system access
  - LOE: ~4-6 hours
  - Depends on: #90 (file I/O & path policy)
  - Files affected: `Common/source/file*.c`
  - Impact: Enables proper cross-platform file operations

- **Issue #106** (P1): Design permission/sandboxing model for daemon vs userspace execution
  - Scope: Large - Security model
  - Depends on: #90 (file I/O & path policy)
  - Status: Design/decision needed
  - Blocks: Daemon/service deployment

- **Issue #81** (P1): Unvendor temporary build dependencies (CMake + Paige)
  - Scope: Medium - Build system refactor
  - LOE: ~2-3 weeks
  - Status: Deferred until Phase 2 (post v6→v7 migration)
  - Note: Vendor Paige statically for now to unblock testing

## Runtime / CLI Stabilization
- [x] Implement headless/kernel clock verbs (`clock.now`, `clock.ticks`, `clock.milliseconds`, `clock.sleepfor`, `clock.waitseconds`, `clock.waitsixtieths`).
  - Implemented using portable time layer (`frontier_time_*` functions) for cross-platform compatibility
  - All clock verbs tested and integrated with CLI runtime
  - Existing `cli_runtime_tests` covers `clock.now()` and `clock.ticks()` validation
  - Date coercions working through `timenow()` with Mac epoch offset (2,082,844,800 seconds)
- [x] Confirm `make -C tests cli_runtime_tests` passes with real clock verb implementations (no more exit=1 skips).
- [ ] Implement remaining date verbs (`date.*` functions) for full CLI coverage.
  - Related: `docs/verb_implementation_status.md` references date verb roadmap

## P2 Follow-Ups (Medium Priority - Nice to Have)

### Migration / Cleanup Follow-Ups (PR #137)
- [ ] Issue #139: Cleanup path duplication (factor out repeated cleanup logic across cancoon.c and other files).
- [ ] Issue #141: Final cleanup restructuring (consolidate all cleanup scenarios into consistent patterns).
- [ ] Issue #142: Error path test coverage (add tests for migration failures at various points in the flow).
- [ ] Issue #143: cleanup_migration_database error scenarios (add tests for scenarios 2 and 3 - error with Save As active, error with allocated destination).

### Code Cleanup Follow-Ups (IFDEF Cleanup - Approved Strategy)
**Reference**: `planning/phase3/code-cleanup/IFDEF_CLEANUP_STRATEGY.md` (approved)

**Phase 1: Quick Wins (LOW RISK)** - Ready to start
- [ ] Remove "xxx"-prefixed dead code blocks (~15 blocks, ~150 lines)
  - `xxxWIN95VERSION`, `xxxPIKE`, `xxxfldebug`, `xxxver`, etc.
  - Files: strings.c, shellwindow.c, langpack.c, shellwindowmenu.c, others
  - Impact: Clean up disabled code by convention

- [ ] Remove explicit dead code markers (~3 blocks)
  - `OBSOLETE` (whirlpool.c: ~1000 lines of obsolete crypto tables)
  - `NEVER` (langevaluate.c: error reporting code)
  - **Keep**: `NeverDefine_For_Reference` (defensive guard pattern)

- [ ] Remove obsolete platform code (~5 blocks)
  - `oldMACVERSION` (3 blocks - v7 format doesn't use Mac aliases)
  - Commented `WIN95VERSION` blocks (2 blocks)

**Phase 2-3: Debug Infrastructure Migration (MEDIUM RISK)** - Design first
- [ ] Create logging infrastructure (logging.h/logging.c)
  - Runtime-controlled log levels via environment variables
  - Component-based filtering (DB, Hash, Table, Pack, etc.)
  - Replace 76 debug ifdef blocks incrementally

- [ ] Migrate database layer debug ifdefs (db.c, db_format.c, tablepack.c)
  - Estimated 55+ `fldebug` blocks → `log_debug()` calls
  - Phase 2 of logging migration

**Phase 4: Feature Flag Cleanup (APPROVED DECISIONS)**
- ✅ **PIKE removal** (29 blocks): APPROVED - will implement Week 13 per roadmap
- ✅ **Optional database backends**: Keep as compile-time flags (MySQL, SQLite, Python)
  - Document via Makefile/CMake instead of hardcoding
  - If future Python integration needed, will require architectural rethinking

**Deferred to later**:
- Threading/networking ifdef cleanup (defer until architecture stabilized)
- SMART_DB_OPENING, xmlfeature, and miscellaneous flags (audit separately)

### Additional Hash / Serialization Follow-Ups (P2)
- [ ] Issue #76: Add corruption/bounds tests for hash unpack (OOB name index, truncated records, header edge cases).
- [ ] Consider small gating for verbose hash unpack logging to keep perf predictable when enabled.

## Numeric Type System (mostly done)
- [x] 64-bit widening for int/long/date in `tyvaluedata`; arithmetic/bitwise paths operate on 64-bit.
- [x] Infinity constant set to INT64_MAX; constants seeded at startup.
- [ ] Add targeted tests for 64-bit infinity usage (e.g., `string.mid(..., infinity)`), and date range around 2040+ per `planning/phase3/date_time_format_standard.md`.
- [ ] (Deferred) Drop double handle indirection; only when confident it won’t affect runtime semantics.

## Docs / Planning
- [x] Document v7 hash record layout and logging env var in `docs/database_architecture.md`.
- [ ] Keep `_STATUS_ARCHIVE.md` and `_CURRENT_STATUS.md` in sync with future milestones; note any new env vars or tooling expectations.
- [ ] Update documentation to remove Pike product references (from approved PIKE removal decision)
  - Files: README.md, build documentation, architecture docs
  - Emphasize: Frontier-only codebase, Pike was a separate product
- [ ] Document optional database backend flags in build system documentation
  - Note: FRONTIER_MYSQL, FRONTIER_SQLITE, FRONTIER_PYTHON compile-time flags
  - Update: Makefile/CMake build documentation
  - Caveat: If Python integration wanted in future, will need architectural rethinking

## Nice-to-Haves / Future
- [ ] Add small helper macros for BE packing in other packers if duplication grows.
- [ ] Add path validation tests for logging env vars to guard against unsafe paths (headless only).
