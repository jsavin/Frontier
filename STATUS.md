# Frontier Status Report

**Last Updated:** 2026-01-25
**State:** v1.0.0-alpha.2 released; 67% coverage (480/710 verbs); headless + 64-bit aligned; v7 format stable; universal binary (arm64+x86_64)

For detailed progress reports, see [reports/progress/2026-01-25-networking-foundation-and-thread-safety.md](reports/progress/2026-01-25-networking-foundation-and-thread-safety.md).

---

## Current Status Matrix

| Area               | Status | Notes                                                                                   |
| ------------------ | :----: | --------------------------------------------------------------------------------------- |
| 64-bit alignment   |   ✅    | DB header alignment stable (90-byte v7 header); all structure tests passing            |
| Hash serialization |   ✅    | Hash pack/unpack hardened with explicit BE buffers, bounds checks, corruption tests    |
| arm64 build        |   ✅    | Universal binary (arm64+x86_64); all tests pass on both architectures                   |
| Headless runtime   |   ✅    | Portable stubs cover runtime/IO; EFP shim stable; parameter state thread-local          |
| Pre-release dist   |   ✅    | v1.0.0-alpha.2 released; universal binary, auto-discovery, professional installer       |
| File verbs         |   ✅    | 100% complete (86/86); all integration tests passing (52/52)                            |
| DB verbs           |   ✅    | 100% complete (13/13); v6→v7 auto-migration; Y2038-safe; 32/32 tests passing           |
| Lang verbs         |   ✅    | 100% complete (61/61); all type operations, utilities, core functions                   |
| Op verbs           |   ✅    | 100% complete (45/45); outline operations, external variable scope                      |
| String verbs       |   ✅    | 100% complete (60/60); all string manipulation operations                               |
| Table verbs        |   ✅    | 100% complete (18/18); table operations                                                 |
| XML verbs          |   ✅    | 100% complete (14/14); XML parsing and generation                                       |
| Date/clock verbs   |   ✅    | 100% complete (37/37); 64-bit timestamps, Y2038-safe                                    |
| Math/crypt verbs   |   ✅    | 100% complete (8/8); math (3) + crypt (5)                                               |
| Small processors   |   ✅    | 100% complete (27/27); kb, mainwindow, target, base64, semaphore, point, rectangle, rgb|
| Sys verbs          |   ✅    | 100% complete (16/16); environment variables, script processor, platform-specific stubs |
| HTML verbs         |   ✅    | 100% complete (23/23); Phase 1-2 implemented, 3 script-implemented, 1 ghost cruft       |
| Script verbs       |   ✅    | 100% complete (13/13); 2 C-implemented, 11 script-implemented                           |
| Dialog verbs       |   ✅    | 100% complete (19/19); 4 CLI prompts, 7 platform-specific, 4 twoway/threeway, 4 ghost  |
| TCP verbs          |   ✅    | Phase 1A/1B/3 complete (11/23); client sockets, server listen, address operations       |
| Thread verbs       |   🚧    | Phase 1 complete (11/17); registry, deterministic testing, thread lifecycle             |
| Overall coverage   |   🚧    | 67% complete (480/710); 28 processors at 100%; TCP/thread foundations ready             |
| Tests (integrated) |   ✅    | YAML-based framework; 1,100+ tests passing; sandbox-safe paths; 99% pass rate           |
| Tests (runtime/db) |   ✅    | Full `SANITIZE=1` passes; Year 2038 safe                                                |\
| REPL interactive   |   ✅    | Basic read-eval-print loop; dialog prompts; file dialogs; batch mode                    |
| Docs/Planning      |   ✅    | ADR-009; OUTLINE_STRUCTURE.md; typeof() documented; repository clean                    |
| GitHub Actions     |   ✅    | Automated releases on tags; universal binary builds; SHA-256 checksums                  |

---

## Highlights

- **Production-ready TCP networking (Phase 1A/1B/3)** – 11 socket verbs implemented (tcp.openAddrStream, tcp.readStream, tcp.writeStream, tcp.listenStream, etc.) enabling client-server architecture. 93 integration tests, security hardened (SSRF/DNS rebinding protection). Foundation for networked applications and HTTP server support.
- **Thread-safety foundation (Phase 1)** – Thread registry established, deterministic test infrastructure for controlled timing, 11 of 17 thread verbs operational. ADR-010 documents roadmap for eliminating global mutable state before launch (Phase 4 blocker).
- **Pre-release distribution (v1.0.0-alpha.2)** – First packaged release for early adopters with universal binary (arm64+x86_64), automatic system root discovery, professional installer, and GitHub Actions automation. Release includes v7 database, SHA-256 checksums, and comprehensive documentation. Alpha.2 fixes critical upgrade bug that would destroy user data. Download: [GitHub Releases](https://github.com/jsavin/Frontier/releases/tag/v1.0.0-alpha.2)
- **Complete ODB Engine API (db.* verbs 13/13)** – Guest database operations fully functional with transparent v6→v7 auto-migration, 64-bit timestamp handling (Y2038-safe), and context guard pattern for safe concurrent system root + guest database use. 32/32 integration tests passing (100%). Production-ready for external database manipulation.
- **Comprehensive kernel verb implementation** – **67% coverage (480/710 verbs)** with 28 processors at 100%: file (86), db (13), lang (61), op (45), sys (16), string (60), table (18), target (3), xml (14), html (23), script (13), dialog (19), date (30), clock (7), crypt (5), math (3), kb (4), mainwindow (7), base64 (2), semaphore (2), point (2), rectangle (2), rgb (2), inetd (1), launch (5), search (6), tcp (23), webserver (7). All implementations tested via YAML-based integration framework (1,100+ tests passing).
- **Database migration fixes** – Resolved system.paths corruption, path entry name matching, and builtins priority issues (PR #336-#342). Path resolution now works correctly; lookups find complete tables instead of partial stubs.
- **64-bit/ARM + big-endian v7** – Core builds/tests compile on `arm64`/`x86_64`; v7 headers/trailers and table addresses write big-endian for cross-arch parity. Hash pack/unpack hardened with explicit 16-byte BE buffers, bounds checks, header detection. Migration coverage complete with Y2038-safe 64-bit timestamps throughout.
- **Portable/headless + Paige-free** – The `portable/` layer + headless stubs power CLI/testing without UI deps; wptext uses Paige-free extractor/RTF path. v6→v7 migration complete with proper timestamp handling (64-bit frontier_time_t). System root auto-discovery eliminates need for --system-root flag.
- **CLI documentation complete (PR #343)** – Comprehensive frontier-cli reference with usage patterns, database operations, UserTalk scripting, testing strategies.
- **Automated kernel verb generation** – Python-based parser (`tools/kernelverbs_parser/`) automatically generates `kernel_verbs_init.c` from `kernelverbs.rc`, extracting all 51 EFP processor definitions (707 verbs). Next phase: automatic implementation detection via static analysis.
- **Modernized test harness** – Cross-platform C test suite with sanitizer presets (`SANITIZE=1 make -C tests`). Integration test framework supports YAML-based verb testing with path templating for sandbox safety. 1,100+ integration tests passing (file, db, lang, tcp, thread verbs).
- **Critical architectural documentation** – typeof() OSType code behavior documented. ADR-005 thread-local parameter state integrated. Database context debugging patterns captured. Op verb semantics validated against docserver reference. TCP testing strategy and callback infrastructure documented.
- **Repository hygiene** – 78 branches cleaned up (88→3 active local branches). All zombie branches (merged PRs) and stale/superseded work removed. Permanent archive branches preserved.

**Note on test counts**: Framework contains ~1,495 total tests. Default test run (~1,100) excludes optional network-dependent tests. TCP-specific tests (150+) are subset of total. See `docs/TEST_STATUS_SUMMARY.md` for breakdown by category.

---

## Remaining Work

**230 verbs remaining (33% of total)** across 23 processors. Most are platform-specific GUI operations or require C implementations.

**UserTalk-Implemented (0 remaining):**
All UserTalk-implemented processors now recognized: tcp (23), webserver (7), search (6), launch (5), inetd (1) = 42 verbs implemented in pure UserTalk.

**C Implementation Available, External Libraries Required:**
- **mysql** (27 verbs) - Full C implementation in Common/source/langmysql.c (1782 lines), requires libmysqlclient
- **sqlite** (17 verbs) - Full C implementation in Common/source/langsqlite.c (1198 lines), requires libsqlite3

**Needs C Implementation (146 verbs across 21 processors):**

**Priority: Fundamental operations:**
- **re** (10 verbs) - Regular expressions, fundamental for text processing
- **bit** (8 verbs) - Bitwise operations (shifts, masks, XOR)
- **thread** (17 verbs) - Threading and concurrency primitives

**GUI/Platform-Specific (likely stubbed in headless):**
- window (31), menu (14), editmenu (16), filemenu (10) = 71 verbs for window/menu management
- htmlcontrol (8), mrcalendar (11), statusbar (5) = 24 verbs for UI widgets
- clipboard (2), mouse (2), speaker (3), pict (4) = 11 verbs for platform I/O
- rez (15) - Resource fork operations (Mac Classic)

**Legacy/Special Purpose:**
- frontier (14) - Frontier-specific verbs
- dll (4), osa (2), python (1) = 7 verbs for external scripting
- opattributes (5) - Outline attribute operations
- searchengine (4 remaining) - Search engine operations (1/5 implemented)

**Total breakdown:**
- 480 implemented (67%): 438 C + 42 UserTalk
- 44 C-implemented but not linked (mysql 27 + sqlite 17)
- 186 stubbed/unimplemented (146 need C + 40 external libs not linked)

---

## Next Milestone Snapshot

**Immediate priorities (next 1-2 weeks):**
1. **TCP Phase 2: Buffered I/O** (#345) - Add `tcp.writeBuffer()`, `tcp.flushBuffer()`, `tcp.readLine()` for efficient HTTP client support. Blocks HTTP networking features.
2. **Thread Phase 2: Controlled Timing** - Wire up `gettickcount()` interception for deterministic multi-thread testing (15 tests with explicit scheduling).
3. **Early adopter feedback** - Monitor v1.0.0-alpha.2 usage and address reported issues
4. **REPL enhancements** (#315) - Command history persistence, tab completion, syntax highlighting
5. **Metadata optimization** (#334) - Optimize `defined()` to check metadata without disk loading

**Short-term (2–4 weeks):**
- TCP Phase 4: HTTP client support (connection pooling, chunked encoding, keepalive)
- Window verb implementations (headless-compatible subset for async callbacks via P0a infrastructure)
- Database callbacks via parameterized callbacks infrastructure
- Search, menu, and other UI-adjacent processors (selective headless support)
- Automatic verb binding phases 2–3 (verification, test infrastructure)
- v1.0.0-beta.1 release with 70%+ verb coverage

**Medium-term (1–2 months):**
- v1.0.0 stable release (target: 80%+ verb coverage)
- Linux distribution and packaging
- Collaborative ODB foundation (Phase 2.0) for multi-user support with thread-safe database operations
- Performance benchmarking and optimization
- Global mutable state elimination (Phase 4 preparatory work)

**CI/Infrastructure:**
- GitHub Actions workflow expansion (test automation, coverage tracking)
- Automated integration test runs on PRs
- Performance regression detection
- Homebrew tap distribution for macOS (`brew install jsavin/frontier/frontier-cli`)

---

## Planning & Documentation

For detailed planning see:
- `planning/INDEX.md` – roadmap + ownership
- `planning/DECISIONS.md` – current decisions/TBDs
- `planning/EFP_HEADLESS_NOTES.md` – headless shim, success criteria, removal plan
- `planning/adr/ADR-0010-headless-efp-routing.md` – decision record for dotted call routing
- `planning/Frontier_Refactoring_Plan.md` – original modernisation plan
- `planning/phase3/headless_daemon_vision.md` – target architecture for the headless daemon/service core
- `planning/phase3/kernel_verb_porting/` – kernel verb porting guides and automatic binding architecture
- `planning/big_endian_portability_audit.md` – current BE v7 portability audit/tasks
- `codex_sessions/README.md` – how to fetch/view Codex transcript logs

For in-flight work/status, see `planning/_CURRENT_STATUS.md`. Historical session context lives in `planning/progress_reports/README.md`. For recent accomplishments (Jan 16-25), see `docs/WORK_SUMMARY_2026_01_16_TO_NOW.md`.
