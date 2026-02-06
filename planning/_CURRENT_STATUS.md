# Current Status

Last Updated: 2026-02-05

## Current Focus: Startup Scripts, Menu System & GUI Planning

**Status**: Startup script execution working in headless mode. Menu system stabilized with migration fixes. GUI application planning complete with full protocol and editor specifications. Compiler warnings eliminated.

**Latest Release**: **v1.0.0-alpha.4** (January 31, 2026)

**Verb Coverage**: **68% (482/710 verbs)** - TCP at 100%, all core processors complete.

## Recent Achievements (February 1-5, 2026)

### Startup Scripts & Path-Based File Verbs - ✅ MERGED
- **PR #378**: Enable `system.startup.startupScript` execution in headless mode
- **PR #382**: Startup scripts and path-based file verbs
- **PR #389**: Fix startup warnings #2 and #3 (menupack and startup script)
- Startup script now runs at boot, enabling daemon-mode workflows
- Path-based file verbs operational

### Menu System Stabilization - ✅ MERGED
- **PR #383**: Add proper `menubarType` data access for headless mode
- **PR #384**: Comprehensive menu integration tests for headless mode
- **PR #385**: Fix V6 menu loading during migration and byte-swap linkage
- **PR #387**: Fix `int32_t` for disk struct fields to ensure 4-byte size on 64-bit
- **PR #388**: Fix `op.outlineToXml` for headless mode via window verb stubs
- **PR #390**: Fix menubar (mbar) value copying for headless mode
- Static assertions added for disk struct sizes (anti-pattern documented)
- Mbar value copy now uses dedicated path with proper v6/v7 format detection

### Compiler Warning Elimination - ✅ COMPLETE
- **PR #372**: Reduce compiler warnings from 154 to 24 (84% reduction)
- **PR #373**: Eliminate remaining 24 warnings (Phase 2)
- Zero compiler warnings achieved

### Build & Distribution Improvements - ✅ MERGED
- **PR #381**: Add `make dist` target for legacy-compatible distribution
- **PR #377**: Add `--migrate` flag for standalone database migration
- Default Makefile target now builds `all` (with dist)

### GUI Application Planning - ✅ DOCUMENTED
- Complete planning directory: `planning/gui/`
- **ARCHITECTURE.md**: Overall GUI architecture, multi-user model, authentication, federation
- **PROTOCOL.md**: JSON protocol specification for client-server communication
- **TABLE_BROWSER.md**: Table browser / ODB navigator specification
- **SCRIPT_EDITOR.md**: Outline-based script editor with debugging
- **OUTLINE_EDITOR.md**: Outline editor with hoisting, attributes, render modes
- **MENU_EDITOR.md**: Menu bar and popup menu editor
- **WPTEXT_EDITOR.md**: Rich text (RTF) editor specification
- **CONSOLE.md**: Unified REPL and QuickScript console
- External types documented (table, script, outline, menubar, wptext, etc.)

### Quality & Documentation - ✅ MERGED
- Demoted system table snapshot logging from WARN to DEBUG
- Removed 255-byte result truncation in CLI and REPL output
- Suppressed error logging for caught try block errors
- Changed menu loading diagnostics from ERROR to DEBUG level
- Added disk struct audit findings to anti-patterns guide
- Window verb stub documentation improved
- OPML test export with pass/skip/fail statistics

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
- Thread verbs: 64% (11/17) - Phase 1 foundation complete
- Many other processors complete (dialog, html, xml, sys, webserver, inetd, etc.)

Reference: `reports/coverage/verb-binding/2026-01-27-01.md`

### Integration Test Status
- **Current**: 1,802 tests total (up from 1,777)
- **Skipped**: 152
- New tests added for menus, mbar value copy, path resolution, and startup scripts

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

1. **GUI Application Prototype**
   - Planning is complete; begin prototype implementation
   - Start with table browser and protocol layer
   - Native macOS app using specs in `planning/gui/`

2. **Startup Script Hardening**
   - Validate all critical-path kernel verbs for daemon mode
   - Long-running HTTP process testing

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
- **ADR-013**: REPL Event Loop Architecture

### Progress Reports
- **Latest**: reports/progress/2026-02-05-startup-scripts-menus-and-gui-planning.md
- **Previous**: reports/progress/2026-01-25-networking-foundation-and-thread-safety.md

### Historical Context
- **Status Archive**: planning/_STATUS_ARCHIVE.md (entries before 2026-01-27)
- **TODO Archive**: Historical completed work (see _CURRENT_TODO_LIST.md)
