# Frontier Refactoring Project (develop branch status)

**Last updated:** 2026-04-07

Frontier is being brought back to life. This project is modernizing the classic UserTalk scripting environment and object database into a contemporary cross-platform tool. The headless CLI is now fully functional—you can explore databases, write scripts, debug UserTalk programs, and serve web applications, all from the command line. Recent work has shipped a full protocol-based debugger (breakpoints, stepping, watchpoints, variable inspection), hardened data integrity, and eliminated global mutable state from the database layer. A native GUI application with a documented API is being planned, and any developer will be able to connect their own apps and user interfaces to Frontier. The goal: preserve everything that made Frontier powerful while making it accessible to a new generation of developers, tinkerers, bloggers, writers, podcasters, and product builders.

## Latest Release: v1.0.0-alpha.7 (Feb 16, 2026)

**Download:** [GitHub Releases](https://github.com/jsavin/Frontier/releases/tag/v1.0.0-alpha.7)

**Zero test failures, real threading, and parallel test execution.** This release brings major infrastructure improvements:

- **0 Integration Test Failures** – 1,881 tests, all passing (was 755 failures two weeks ago)
- **GIL-Based Threading** – Real POSIX threads with a global interpreter lock and cooperative yield points
- **NDJSON Protocol Mode** – Persistent subprocess communication via `--protocol` flag; foundation for future GUI
- **8-Worker Parallel Tests** – Full test suite runs in ~40 seconds (was 5+ minutes sequential)
- **Per-Component Logging** – Fine-grained control via `FRONTIER_LOG=comp:level` and `--log` flag
- **Ranger-Style File Browser** – Two-pane file browser with arrow key navigation for `file.getFileDialog`

**Quick start:**
```usertalk
[root]> user.inetd.config.http.port = 8080
[root]> user.webserver.responders.helloWorld.enabled = true
[root]> inetd.startOne (@user.inetd.config.http)
# Visit http://localhost:8080/helloworld in your browser
```

**Overall Progress:** 68% verb coverage (482/710 verbs), 22 processors fully implemented, 302 unit tests + 1,920 integration tests — 0 failures.

For comprehensive status details, see [STATUS.md](STATUS.md). For release details, see the [v1.0.0-alpha.7 release notes](https://github.com/jsavin/Frontier/releases/tag/v1.0.0-alpha.7).

### Development Progress (Feb 16 - Apr 7, 2026)

Since the last release, 74 PRs have been merged:

- **Protocol-Based UserTalk Debugger (7 phases)** — Full debugger accessible via the NDJSON protocol: set/clear/list breakpoints, step into/over/out, watchpoints with fire-on-change, conditional breakpoints with UserTalk expressions, multi-thread debugging with thread listing, and variable inspection at any scope. Enables any GUI or IDE to provide a debugging experience.
- **Data-Loss Risk Hardening** — Duplicate open guards prevent concurrent modification of the same database file, fread size validation catches truncated reads, migration locking prevents partial writes during v6→v7 conversion.
- **databasedata Global Elimination (Phases 1-10)** — All runtime save/swap/restore of the `databasedata` global eliminated from pack/unpack/save/load paths. Explicit DB handle threading throughout the wrapper layer. Zero runtime mutation achieved.
- **Startup & Threading Fixes** — GIL deadlock blocking HTTP callbacks resolved, 19 consistently-failing integration tests fixed, guest DB script execution stabilized.
- **CLI Enhancements** — `system.environment.args` exposes CLI arguments to scripts, `sys.openUrl` kernel verb, `--browser` flag for automation tools.
- **Lint Infrastructure** — clang-tidy (bug-finding checks), ruff (Python), shellcheck (bash) configs with Makefile targets.

**Current Test Status:** 302 unit tests + 1,920 integration tests — **0 failures** (8-worker parallel, ~37s)

---

## Quick Start

### Prerequisites

- **macOS** (12.0 Monterey or later recommended)
- **Xcode Command Line Tools**: Install with `xcode-select --install`

### Build and Run

```bash
# 1. Clone the repository
git clone https://github.com/jsavin/Frontier.git
cd Frontier

# 2. Build the CLI (creates universal binary for arm64 + x86_64)
make -C frontier-cli

# 3. Verify the build works
./frontier-cli/frontier-cli -e "1 + 1"
# Output: 2

# 4. Launch the interactive REPL
./frontier-cli/frontier-cli
# Type UserTalk expressions, use /help for commands, /exit to quit
```

### Run Tests

```bash
# Unit tests (C test suite)
./tools/run_headless_tests.sh

# Integration tests (Python/YAML-based UserTalk tests)
cd tests && make test-integration

# Both unit and integration tests
cd tests && make test-all
```

### Next Steps

- **[Getting Started Guide](docs/GETTING_STARTED.md)** - Complete newcomer guide with detailed setup
- **[CLI Usage Guide](docs/CLI_USAGE_GUIDE.md)** - Comprehensive CLI and REPL documentation
- **[Testing Guide](docs/TESTING_GUIDE.md)** - Writing and running tests
- **[Palette Test Harness](docs/PALETTE_TEST_HARNESS.md)** - Four-layer (L1-L4) test model for the REPL slash-menu palette

## MySQL Client Setup

Frontier links against the MySQL/MariaDB C client library for legacy database integration. Prebuilt binaries are no longer stored in the repo.

- **macOS / Linux:** run `scripts/build_mysql_client.sh` to fetch and compile MariaDB Connector/C into `Common/MySQL/` (or override the install location via `MYSQL_CLIENT_PREFIX`). The script builds both `arm64` and `x86_64` static libraries and drops compatibility symlinks (`libmysqlclient.a` and `include/mysql/`).
- **Windows:** legacy Visual Studio project files have been removed. Updated instructions will accompany the next iteration of Windows support.

See `docs/mysql_client_setup.md` for detailed guidance.

---

## Repository Layout (Quick Tour)

```
Frontier/
├── app_resources/        # App bundles/resources (Frontier, OPML, Radio)
├── Common/               # Legacy Frontier sources/headers
├── databases/            # Frontier.root + guest databases (test fixtures)
├── docs/                 # Extensive documentation of Frontier's design and the UserTalk language and verbs
├── portable/             # Portable runtime layer + stubs
├── frontier-cli/         # Multi-arch CLI build
├── tests/                # Cross-platform C test suite
├── tools/                # Build tools (kernelverbs_parser, strings_compiler)
├── planning/             # Roadmap, ADRs, decisions, quickstarts
├── codex_sessions/       # README pointer (actual logs in codex-sessions branch, no longer used)
├── reports/              # Static analysis and progress reports (generated)
└── usertalk-scripts/     # Text file export of all the core UserTalk scripts in Frontier.root
```

---

## Planning & Documentation (Read These First)

- `planning/INDEX.md` – roadmap, active workstreams, ownership
- `planning/phase_overview.md` – overview of all phases
- `planning/architectural_decision_records/` – ADRs for key technical decisions
- `planning/Frontier_Refactoring_Plan.md` – original modernisation plan

For in-flight work/status, see [STATUS.md](STATUS.md). Historical session context lives in `planning/progress_reports/README.md`.

---

## Contribution Workflow

Frontier development follows a structured workflow designed to maintain code quality and enable parallel development:

1. **Read `CONTRIBUTING.md`** for branching, commit, and testing expectations.
2. **Branch from `develop`** and keep changes small. Use feature branches in separate worktrees for non-trivial work:
   ```bash
   cd /Users/jake/dev/jsavin/Frontier
   git worktree add ../Frontier-<feature-name> -b feature/<feature-name>
   cd ../Frontier-<feature-name>
   ```
3. **Update/consult planning docs** before coding (PRs reference the appropriate ADR/decision where possible).
4. **Run targeted tests locally**; note known failures when applicable:
   ```bash
   ./tools/run_headless_tests.sh              # Unit tests
   cd tests && make test-integration          # Integration tests
   ```
5. **Update docs and tests alongside code**; add Codex notes if significant.
6. **Open PRs against `develop`** (multi-arch + headless tests should remain green). Use the `/doit` workflow for feature development:
   - Creates feature branch in worktree
   - Designs implementation plan (with user approval)
   - Implements with specialized agents (in parallel where possible)
   - Writes and runs tests
   - Creates PR with background monitoring

For detailed workflow guidance, see `docs/WORKTREE_WORKFLOW.md` and `docs/PR_MONITOR_BLOCKING_ISSUE.md`.

### Key Development Principles

**Security and Testing:**
- Security hardening built in from start (SSRF protection, DNS rebinding, data-loss guards)
- 2,222 tests (302 unit + 1,920 integration), 0 failures, 8-worker parallel execution
- Integration tests required for all verb implementations

**Thread Safety and Database Integrity:**
- GIL-based cooperative threading with real POSIX threads
- databasedata global elimination complete — zero runtime mutation
- v6→v7 migration validated, Y2038-safe 64-bit timestamps throughout
- Context guard pattern for safe concurrent database operations

For comprehensive status details, verb coverage, and milestone snapshots, see [STATUS.md](STATUS.md).

---

## Historical Progress

Historical session summaries live under `planning/progress_reports/README.md`.
