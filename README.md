# Frontier Refactoring Project (develop branch status)

**Last updated:** 2026-01-16
**State:** v1.0.0-alpha.2 released; 23 verb processors at 100% (file, db, lang, op, sys, string, table, target, xml, html, script, dialog, date, clock, crypt, math, kb, mainwindow, base64, semaphore, point, rectangle, rgb); overall 61% coverage (438/710 verbs); headless + 64-bit aligned; v7 format stable; universal binary (arm64+x86_64)
**Primary contacts:** planning/INDEX.md (owners per phase)

This repository is actively modernizing the Frontier runtime and toolchain. The `develop` branch now builds and tests with 64-bit alignment on both `arm64` and `x86_64`, includes a portable/headless runtime layer, and routes headless UserTalk `file.*` verbs through the external function processor (EFP) table so tests can exercise real UserTalk without `system.verbs.*` being loaded.

## Highlights

- **Pre-release distribution (v1.0.0-alpha.2)** – First packaged release for early adopters with universal binary (arm64+x86_64), automatic system root discovery, professional installer, and GitHub Actions automation. Release includes v7 database, SHA-256 checksums, and comprehensive documentation. Alpha.2 fixes critical upgrade bug that would destroy user data. Download: [GitHub Releases](https://github.com/jsavin/Frontier/releases/tag/v1.0.0-alpha.2)
- **Complete ODB Engine API (db.* verbs 13/13)** – Guest database operations fully functional with transparent v6→v7 auto-migration, 64-bit timestamp handling (Y2038-safe), and context guard pattern for safe concurrent system root + guest database use. 32/32 integration tests passing (100%). Production-ready for external database manipulation.
- **Comprehensive kernel verb implementation** – **61% coverage (438/710 verbs)** with 23 processors at 100%: file (86), db (13), lang (61), op (45), sys (16), string (60), table (18), target (3), xml (14), html (23), script (13), dialog (19), date (30), clock (7), crypt (5), math (3), kb (4), mainwindow (7), base64 (2), semaphore (2), point (2), rectangle (2), rgb (2). All implementations tested via YAML-based integration framework (304+ tests passing).
- **64-bit/ARM + big-endian v7** – Core builds/tests compile on `arm64`/`x86_64`; v7 headers/trailers and table addresses write big-endian for cross-arch parity. Hash pack/unpack hardened with explicit 16-byte BE buffers, bounds checks, header detection. Migration coverage complete with Y2038-safe 64-bit timestamps throughout.
- **Portable/headless + Paige-free** – The `portable/` layer + headless stubs power CLI/testing without UI deps; wptext uses Paige-free extractor/RTF path. v6→v7 migration complete with proper timestamp handling (64-bit frontier_time_t). System root auto-discovery eliminates need for --system-root flag.
- **Automated kernel verb generation** – Python-based parser (`tools/kernelverbs_parser/`) automatically generates `kernel_verbs_init.c` from `kernelverbs.rc`, extracting all 51 EFP processor definitions (707 verbs). Next phase: automatic implementation detection via static analysis.
- **Modernized test harness** – Cross-platform C test suite with sanitizer presets (`SANITIZE=1 make -C tests`). Integration test framework supports YAML-based verb testing with path templating for sandbox safety. 304+ integration tests passing (file, db, lang verbs).
- **Critical architectural documentation** – typeof() OSType code behavior documented. ADR-005 thread-local parameter state integrated. Database context debugging patterns captured. Op verb semantics validated against docserver reference (PR #314).
- **Repository hygiene** – 78 branches cleaned up (88→3 active local branches). All zombie branches (merged PRs) and stale/superseded work removed. Permanent archive branches preserved.

## Quick start

```bash
# build + run headless tests (multi-arch ready)
make -C tests file_verb_tests && ./tests/file_verb_tests
./tests/file_portable_tests
./tests/file_readline_tests
make -C tests runtime_tests

# sanitizer run
SANITIZE=1 make -C tests test

# CLI build (multi-arch)
make -C frontier-cli

# build MySQL client libraries (installs into Common/MySQL)
scripts/build_mysql_client.sh

## MySQL client setup

Frontier links against the MySQL/MariaDB C client library for legacy database
integration. Prebuilt binaries are no longer stored in the repo.

- **macOS / Linux:** run `scripts/build_mysql_client.sh` to fetch and compile
  MariaDB Connector/C into `Common/MySQL/` (or override the install location via
  `MYSQL_CLIENT_PREFIX`). The script builds both `arm64` and `x86_64` static
  libraries and drops compatibility symlinks (`libmysqlclient.a` and
  `include/mysql/`).
- **Windows:** legacy Visual Studio project files have been removed. Updated
  instructions will accompany the next iteration of Windows support.

See `docs/mysql_client_setup.md` for detailed guidance.
```

## Planning & docs (read these first)

- `planning/INDEX.md` – roadmap + ownership
- `planning/DECISIONS.md` – current decisions/TBDs
- `planning/EFP_HEADLESS_NOTES.md` – headless shim, success criteria, removal plan
- `planning/adr/ADR-0010-headless-efp-routing.md` – decision record for dotted call routing
- `planning/Frontier_Refactoring_Plan.md` – original modernisation plan
- `planning/phase3/headless_daemon_vision.md` – target architecture for the headless daemon/service core
- `planning/phase3/kernel_verb_porting/` – kernel verb porting guides and automatic binding architecture
- `planning/big_endian_portability_audit.md` – current BE v7 portability audit/tasks
- `codex_sessions/README.md` – how to fetch/view Codex transcript logs

For in-flight work/status, see `planning/_CURRENT_STATUS.md`. Historical session context lives in `planning/progress_reports/README.md`.

## Current status matrix

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
| Overall coverage   |   🚧    | 61% complete (438/710); 23 processors at 100%                                           |
| Tests (integrated) |   ✅    | YAML-based framework; 304+ tests passing; sandbox-safe paths                            |
| Tests (runtime/db) |   ✅    | Full `SANITIZE=1` passes; Year 2038 safe                                                |
| REPL interactive   |   ✅    | Basic read-eval-print loop; dialog prompts; file dialogs; batch mode                    |
| Docs/Planning      |   ✅    | ADR-009; OUTLINE_STRUCTURE.md; typeof() documented; repository clean                    |
| GitHub Actions     |   ✅    | Automated releases on tags; universal binary builds; SHA-256 checksums                  |

## Remaining Work

**272 verbs remaining (38% of total)** across 28 processors:

**Large processors (>20 verbs):**
- window (31) - Window management, UI operations
- mysql (27) - MySQL database integration
- tcp (23) - TCP/IP networking

**Medium processors (10-19 verbs):**
- sqlite (17), thread (17) - Database and threading
- editmenu (16), rez (15) - Edit menu operations, resource management
- frontier (14), menu (14) - Frontier core, menu operations
- mrcalendar (11) - Calendar widget
- filemenu (10), re (10) - File menu, regular expressions

**Small processors (<10 verbs):**
- bit (8), htmlcontrol (8) - Bit operations, HTML controls
- webserver (7), search (6) - Web server, search operations
- opattributes (5), statusbar (5), launch (5) - Outline attributes, status bar, app launching
- dll (4), pict (4) - DLL operations, picture handling
- speaker (3) - Audio/beep operations
- clipboard (2), mouse (2), osa (2) - Clipboard, mouse, OSA scripting
- inetd (1), python (1) - Internet daemon, Python integration

**Partial completion:**
- searchengine (20%, 4/5 remaining) - Search engine operations

Most stubbed verbs are platform-specific (GUI operations, resource forks) or legacy integrations (MySQL, Python, OSA) that may not be needed for headless operation.

## Historical progress

Historical session summaries live under `planning/progress_reports/README.md`.

## Repository layout (quick tour)

```
Frontier/
├── app_resources/        # App bundles/resources (Frontier, OPML, Radio)
├── Common/               # Legacy Frontier sources/headers
├── databases/            # Frontier.root + guest databases (test fixtures)
├── portable/             # Portable runtime layer + stubs
├── frontier-cli/         # Multi-arch CLI build
├── tests/                # Cross-platform C test suite
├── tools/                # Build tools (kernelverbs_parser, strings_compiler)
├── planning/             # Roadmap, ADRs, decisions, quickstarts
├── codex_sessions/       # README pointer (actual logs in codex-sessions branch, no longer used)
├── reports/              # Static analysis and progress reports (generated)
└── build_Xcode_modern/   # Xcode build configuration (multi-arch)
```

## Contribution workflow

1. Read `CONTRIBUTING.md` for branching, commit, and testing expectations.
2. Branch from `develop` and keep changes small.
3. Update/consult planning docs before coding (PRs reference the appropriate
   ADR/decision where possible).
4. Run targeted tests locally; note known failures when applicable.
5. Update docs and tests alongside code; add Codex notes if significant.
6. Open PRs against `develop` (multi-arch + headless tests should remain green).

## Next milestone snapshot

**Immediate priorities (next 1-2 weeks):**
1. **Early adopter feedback** - Monitor v1.0.0-alpha.2 usage and address reported issues
2. **Complete remaining dialog verbs** (15/19 remaining) - Interactive operations
4. **REPL enhancements** - Command history persistence, tab completion, syntax highlighting
5. **Homebrew tap distribution** - Add `brew install jsavin/frontier/frontier-cli` support

**Short-term (2–4 weeks):**
- Window verb implementations (headless-compatible subset)
- Search, menu, and other UI-adjacent processors (selective headless support)
- Automatic verb binding phases 2–3 (verification, test infrastructure)
- v1.0.0-beta.1 release with 70%+ verb coverage

**Medium-term (1–2 months):**
- v1.0.0 stable release (target: 80%+ verb coverage)
- Linux distribution and packaging
- Collaborative ODB foundation (Phase 2.0) for multi-user support
- Performance benchmarking and optimization

**CI/Infrastructure:**
- GitHub Actions workflow expansion (test automation, coverage tracking)
- Automated integration test runs on PRs
- Performance regression detection

For detailed planning see `planning/INDEX.md` and current work in `planning/_CURRENT_STATUS.md`.
