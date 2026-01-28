# Current Status

Last Updated: 2026-01-28

## Current Focus: Stability & Quality Improvements 🔧

**Status**: Focused on bug fixes, REPL improvements, and verb resolution correctness. TCP Phase 2 (Buffered I/O) and Phase 4 P0a (Global State Elimination) both queued for future work.

**Verb Coverage**: **67% (481/710 verbs)** - up from 37% in early January! Major progress on lang, string, table, op, and other core processors.

## Recent Achievements (January 25-28, 2026)

### Context Passing Fix - MERGED ✅
- **PR #360**: Fix 51 NULL context calls to *verbinmemory functions (Issue #347)
  - Replaced all NULL context calls with explicit `db_context` structs
  - Added hard assertions to catch future regressions in debug builds
  - Narrowed context scope to block level for better code hygiene
  - Bonus fix: Thread registry test calling non-existent function
  - **Issue #347 CLOSED** - P0 blocker resolved

### REPL Improvements - MERGED ✅
- **PR #356**: Option+Arrow word navigation
  - Industry-standard keyboard shortcuts for REPL word movement
  - Matches behavior of terminal apps and IDEs
- **PR #358**: Suppress v6→v7 migration log spew during startup
  - Cleaner REPL startup experience
  - Migration logging now properly scoped

### Critical Bug Fixes - MERGED ✅
- **PR #359**: Fix defined() error suppression (Issue #325)
  - langerrormessage() now respects error suppression state
  - defined() can silently check for non-existent table entries
  - Improved table.move() test to verify source removal and destination creation
- **PR #354**: Enable nested parentOf() function calls
  - Fixed crash when parentOf() called within parentOf() evaluation
  - Root cause: Language evaluator not properly handling nested function calls
  - 15 new integration tests covering nested scenarios
- **PR #352**: Remove explicit EFP table search to fix introspection bugs
  - Fixed multiple verb resolution regressions (defined(), typeof(), etc.)
  - Eliminated duplicate EFP searches causing incorrect behavior
  - Comprehensive architectural documentation in VERB_RESOLUTION_ARCHITECTURE.md
- **PR #351**: Exclude script-only processors from EFP whitelist
  - Fixed false positives in verb dispatch
  - More accurate EFP routing for script-based processors
- **PR #348**: Keep database open after successful hydration
  - Fixed string verb resolution issues
  - Database lifecycle now properly managed

### Build & Compatibility - MERGED ✅
- **PR #353**: Intel Mac compatibility with universal cmake binary
  - Cross-architecture cmake support (arm64 + x86_64)
  - Ensures build system works on all Mac platforms

### Documentation - MERGED ✅
- **PR #350**: Getting Started guide
  - Complete newcomer onboarding documentation
  - Complements existing Quick Start guide
- **PR #345**: Support positional .root/.root7 arguments in CLI
  - More intuitive CLI usage patterns
- **PR #344**: Update planning and documentation to reflect TCP Phase 1 completion
  - Synchronized planning docs with actual state

## Active Development Status

### Verb Implementation Coverage
- **Overall: 67% (481/710 verbs)** ✅ - Major jump from 37%!
- File verbs: 100% (86/86) ✅
- String verbs: 100% (60/60) ✅
- Lang verbs: 100% (61/61) ✅
- Op verbs: 100% (45/45) ✅
- Table verbs: 100% (18/18) ✅
- Date verbs: 100% (30/30) ✅
- DB verbs: 100% (13/13) ✅
- TCP verbs: 56% (13/23) - Phase 1A/1B/3 complete
- Thread verbs: 64% (11/17) - Phase 1 foundation complete
- Many other processors complete (dialog, html, xml, sys, etc.)

Reference: `reports/coverage/verb-binding/2026-01-27-01.md`

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
  - Related to: TCP networking implementation

### Queued Work

**TCP Networking Phase 2** (Queued - Future Work):
- 4 buffered I/O verbs needed for HTTP client
- Reference: planning/phase4/networking/INDEX.md
- Status: Phase 1A/1B/3 complete (13 verbs), Phase 2 not yet started

**Phase 4 P0a: Global State Elimination** (Queued - Launch Blocking):
- Hash table context migration
- Parser state migration
- Control flow & error state cleanup
- Reference: planning/phase4/INDEX.md, planning/phase4/p0a-critical-thread-safety/README.md
- Timeline: 3-week effort when prioritized

## Integration Test Status

**Current**: 1,124+ tests passing (as of PR #342)

All tests running via:
- `./tools/run_headless_tests.sh` - C unit tests
- `cd tests && make test-integration` - Integration tests (Python/YAML)
- `cd tests && make test-all` - Full suite

## Next Steps (Priority Order)

### Immediate Priorities

1. **Continue Stability & Bug Fixes**
   - Address open P1/P2 issues as discovered
   - Maintain test suite health

2. **Verb Coverage Expansion** (Ongoing)
   - TCP Phase 2 (buffered I/O) - 4 verbs remaining
   - Thread verbs - 6 verbs remaining
   - Processors still at 0%: bit, clipboard, menu, mysql, sqlite, etc.

### Strategic Decisions Required

Before resuming major feature work (TCP Phase 2, P0a), need decisions on:
- Issue #86: Runtime context architecture
- Issue #88: Networking security model

## Reference Documentation

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
- **Verb Resolution Architecture**: docs/VERB_RESOLUTION_ARCHITECTURE.md

### Architecture Decisions
- **ADR-002**: Context-Based Format Versioning
- **ADR-003**: Two-Phase Address Value Resolution
- **ADR-004**: Dynamic Verb Binding Architecture
- **ADR-005**: Parameter State Thread-Safety
- **ADR-010**: Three-Phase Thread Implementation Roadmap

### Historical Context
- **Status Archive**: planning/_STATUS_ARCHIVE.md (entries before 2026-01-27)
- **TODO Archive**: Historical completed work (see _CURRENT_TODO_LIST.md)
