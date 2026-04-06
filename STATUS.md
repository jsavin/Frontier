# Frontier Status Report

**Last Updated:** 2026-04-06
**State:** v1.0.0-alpha.7 released (2026-02-16), +105 commits unreleased; 68% coverage (482/710 verbs); 2,222 tests passing (0 failures); universal binary (arm64+x86_64)

---

## Current Status Matrix

| Area               | Status | Notes                                                                                   |
| ------------------ | :----: | --------------------------------------------------------------------------------------- |
| 64-bit alignment   |   ✅    | DB header alignment stable (90-byte v7 header); all structure tests passing            |
| Hash serialization |   ✅    | Hash pack/unpack hardened with explicit BE buffers, bounds checks, corruption tests    |
| arm64 build        |   ✅    | Universal binary (arm64+x86_64); all tests pass on both architectures                   |
| Headless runtime   |   ✅    | Portable stubs cover runtime/IO; EFP shim stable; parameter state thread-local          |
| Pre-release dist   |   ✅    | v1.0.0-alpha.7 released; universal binary, auto-discovery, professional installer       |
| File verbs         |   ✅    | 100% complete (86/86); all integration tests passing                                    |
| DB verbs           |   ✅    | 100% complete (13/13); v6->v7 auto-migration; Y2038-safe                               |
| Lang verbs         |   ✅    | 100% complete (61/61); all type operations, utilities, core functions                   |
| Op verbs           |   ✅    | 100% complete (45/45); outline operations, external variable scope                      |
| String verbs       |   ✅    | 100% complete (60/60); all string manipulation operations                               |
| Table verbs        |   ✅    | 100% complete (18/18); table operations                                                 |
| XML verbs          |   ✅    | 100% complete (14/14); XML parsing and generation                                       |
| Date/clock verbs   |   ✅    | 100% complete (37/37); 64-bit timestamps, Y2038-safe                                    |
| Math/crypt verbs   |   ✅    | 100% complete (8/8); math (3) + crypt (5)                                               |
| Small processors   |   ✅    | 100% complete (27/27); kb, mainwindow, target, base64, semaphore, point, rectangle, rgb |
| Sys verbs          |   ✅    | 100% complete (16/16); environment variables, script processor, platform-specific stubs |
| HTML verbs         |   ✅    | 100% complete (23/23); Phase 1-2 implemented, 3 script-implemented                     |
| Script verbs       |   ✅    | 100% complete (13/13); 2 C-implemented, 11 script-implemented                           |
| Dialog verbs       |   ✅    | 100% complete (19/19); 4 CLI prompts, 7 platform-specific, 4 twoway/threeway, 4 ghost  |
| TCP verbs          |   ✅    | Phase 1A/1B/3 complete (11/23); client sockets, server listen, address operations       |
| Thread verbs       |   ✅    | GIL-based cooperative threading with real POSIX threads; 11/17 verbs operational        |
| UserTalk debugger  |   ✅    | Full protocol-based debugger: breakpoints, stepping, watchpoints, variable inspection   |
| Overall coverage   |   🚧    | 68% complete (482/710); 22 processors at 100%                                          |
| Tests (unit)       |   ✅    | 302 unit tests passing; full SANITIZE=1 support; Y2038-safe                             |
| Tests (integration)|   ✅    | 1,920 integration tests; YAML-based framework; 8-worker parallel (~37s); 0 failures    |
| REPL interactive   |   ✅    | Read-eval-print loop; dialog prompts; file dialogs; batch mode; word navigation         |
| Docs/Planning      |   ✅    | ADRs, architecture guides, verb implementation docs; repository clean                   |
| GitHub Actions     |   ✅    | Automated releases on tags; universal binary builds; SHA-256 checksums                  |
| Lint infrastructure|   ✅    | clang-tidy, ruff, shellcheck configs for code quality enforcement                       |

---

## Highlights

### Protocol-Based UserTalk Debugger (NEW)

Full-featured debugger for UserTalk scripts, accessible through the NDJSON protocol interface. Seven implementation phases delivered:

- **Breakpoints** -- set, clear, list, and hit breakpoints by script path and line number
- **Stepping** -- step into, step over, step out with full call stack tracking
- **Session breakpoints** -- breakpoints scoped to debugging sessions
- **Watchpoints** -- watch expressions that trigger on value changes
- **Multi-thread debugging** -- debug across cooperative threads with GIL awareness
- **Conditional breakpoints** -- breakpoints with UserTalk expression conditions
- **Variable inspection** -- inspect locals, globals, and evaluate expressions at breakpoints

### Release History (alpha.3 through alpha.7)

- **alpha.3** (2026-01-27): Nested parentOf() fixes, Intel Mac compatibility, REPL word navigation, webserver Hello World, TCP migration
- **alpha.4** (2026-01-31): Compiler warnings 154->24, CLI state persistence, guest database support, startup stabilization
- **alpha.5** (2026-02-07): GIL-based cooperative threading with real POSIX threads, guest database navigation, callback infrastructure
- **alpha.6** (2026-02-11): Integration test failures 755->23, 8-worker parallel execution, NDJSON protocol mode, PTY harness
- **alpha.7** (2026-02-16): Test failures->0, per-worker DB isolation, verb processor consolidation, WP headless support

### Unreleased (105 commits since alpha.7)

- **databasedata global elimination** (Phases 1-10) -- zero runtime mutation of the legacy databasedata global achieved, preparing for full removal
- **Data-loss risk hardening** -- duplicate open guard, fread size validation, migration file locking
- **19 consistently-failing integration tests fixed** -- all 2,222 tests now pass with 0 failures
- **GIL deadlock resolution** for HTTP callbacks
- **New kernel verbs** -- system.environment.args, sys.openUrl
- **Startup stabilization** fixes
- **Lint infrastructure** -- clang-tidy, ruff, and shellcheck configurations added

### Foundation (carried forward)

- **Comprehensive kernel verb implementation** -- **68% coverage (482/710 verbs)** with 22 processors at 100%. All implementations tested via YAML-based integration framework (2,222 tests passing).
- **GIL-based cooperative threading** -- Real POSIX threads serialized by a single mutex. Yield points at langbackgroundtask() and thread.sleepTicks(). Thread registry with deterministic test infrastructure.
- **Complete ODB Engine API (db.* verbs 13/13)** -- Guest database operations fully functional with transparent v6->v7 auto-migration, 64-bit timestamp handling (Y2038-safe), and context guard pattern for safe concurrent system root + guest database use.
- **64-bit/ARM + big-endian v7** -- Universal binary (arm64+x86_64); v7 headers/trailers and table addresses write big-endian for cross-arch parity.
- **Portable/headless + Paige-free** -- CLI/testing without UI dependencies; v6->v7 migration complete; system root auto-discovery.
- **8-worker parallel test execution** -- Full test suite runs in ~37 seconds with per-worker database isolation.
- **NDJSON protocol mode** -- Structured protocol interface for IDE integration, browser automation, and debugger communication.

---

## Remaining Work

**228 verbs remaining (32% of total)** across 23 processors. Most are platform-specific GUI operations or require C implementations.

**UserTalk-Implemented (0 remaining):**
All UserTalk-implemented processors now recognized: tcp (23), webserver (7), search (6), launch (5), inetd (1) = 42 verbs implemented in pure UserTalk.

**C Implementation Available, External Libraries Required:**
- **mysql** (27 verbs) - Full C implementation in Common/source/langmysql.c (1782 lines), requires libmysqlclient
- **sqlite** (17 verbs) - Full C implementation in Common/source/langsqlite.c (1198 lines), requires libsqlite3

**Needs C Implementation (146 verbs across 21 processors):**

**Priority: Fundamental operations:**
- **re** (10 verbs) - Regular expressions, fundamental for text processing
- **bit** (8 verbs) - Bitwise operations (shifts, masks, XOR)
- **thread** (6 remaining) - Threading and concurrency primitives

**GUI/Platform-Specific (likely stubbed in headless):**
- window (31), menu (14), editmenu (16), filemenu (10) = 71 verbs for window/menu management
- htmlcontrol (8), mrcalendar (11), statusbar (5) = 24 verbs for UI widgets
- clipboard (2), mouse (2), speaker (3), pict (4) = 11 verbs for platform I/O
- rez (15) - Resource fork operations (Mac Classic)

**Legacy/Special Purpose:**
- frontier (14) - Frontier-specific verbs
- dll (4), osa (2), python (1) = 7 verbs for external scripting
- opattributes (5) - Outline attribute operations
- searchengine (4 remaining) - Search engine operations

**Total breakdown:**
- 482 implemented (68%): 440 C + 42 UserTalk
- 44 C-implemented but not linked (mysql 27 + sqlite 17)
- 184 stubbed/unimplemented

---

## Next Milestone Snapshot

**Immediate priorities:**
1. **databasedata global removal** -- Zero mutation achieved; next step is removing the global variable entirely
2. **Debugger polish** -- Protocol-based debugger is feature-complete; stabilization and edge case handling
3. **Test suite maintenance** -- Keep 0-failure baseline as new features land

**Short-term:**
- TCP Phase 2: Buffered I/O for efficient HTTP client support
- Window verb implementations (headless-compatible subset for async callbacks)
- Database callbacks via parameterized callback infrastructure
- v1.0.0-beta.1 release with 70%+ verb coverage

**Medium-term:**
- v1.0.0 stable release (target: 80%+ verb coverage)
- Linux distribution and packaging
- Collaborative ODB foundation for multi-user support
- Performance benchmarking and optimization
- Full global mutable state elimination

**CI/Infrastructure:**
- GitHub Actions workflow expansion (test automation, coverage tracking)
- Automated integration test runs on PRs
- Performance regression detection

---

## Planning & Documentation

For detailed planning see:
- `planning/INDEX.md` -- roadmap + ownership
- `planning/DECISIONS.md` -- current decisions/TBDs
- `planning/phase_overview.md` -- overview of all phases
- `planning/phase6/CRDT_FOUNDATION_ROADMAP.md` -- collaborative ODB foundation roadmap
- `planning/architectural_decision_records/` -- ADRs for key technical decisions
- `docs/GETTING_STARTED.md` -- newcomer guide (build, run, test)
- `docs/CLI_USAGE_GUIDE.md` -- complete frontier-cli reference
- `docs/TESTING_GUIDE.md` -- testing patterns and strategies
- `docs/VERB_IMPLEMENTATION_GUIDE.md` -- implementing kernel verbs in C
- `docs/DEBUGGING_GUIDE.md` -- LLDB, investigation patterns, protocol debugger

**Open issues:** 30 (unprioritized). See [GitHub Issues](https://github.com/jsavin/Frontier/issues) for the current list.
