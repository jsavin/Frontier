# Frontier Refactoring Project (develop branch status)

**Last updated:** 2026-06-07

Frontier is being brought back to life. This project is modernizing the classic UserTalk scripting environment and object database into a contemporary cross-platform tool. The headless CLI is now fully functional — you can explore databases, write scripts, debug UserTalk programs, and serve web applications, all from the command line. Two debugger interfaces ship in the same binary: a machine-driven NDJSON protocol that any IDE or external tool can drive (`--protocol`), and a full-screen terminal UI (`--debug-tui`) for direct interactive use. A portable terminal window-manager substrate (boxen) underlies the TUI and is the foundation for the upcoming Phase C native REPL. A native GUI application with a documented API is being planned in parallel. The goal: preserve everything that made Frontier powerful while making it accessible to a new generation of developers, tinkerers, bloggers, writers, podcasters, and product builders.

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

Through the alpha.7 release window:

- **Protocol-Based UserTalk Debugger (7 phases)** — Full debugger accessible via the NDJSON protocol: set/clear/list breakpoints, step into/over/out, watchpoints with fire-on-change, conditional breakpoints with UserTalk expressions, multi-thread debugging with thread listing, and variable inspection at any scope. Enables any GUI or IDE to provide a debugging experience.
- **Data-Loss Risk Hardening** — Duplicate open guards prevent concurrent modification of the same database file, fread size validation catches truncated reads, migration locking prevents partial writes during v6→v7 conversion.
- **databasedata Global Elimination (Phases 1-10)** — All runtime save/swap/restore of the `databasedata` global eliminated from pack/unpack/save/load paths. Explicit DB handle threading throughout the wrapper layer. Zero runtime mutation achieved.
- **Startup & Threading Fixes** — GIL deadlock blocking HTTP callbacks resolved, 19 consistently-failing integration tests fixed, guest DB script execution stabilized.
- **CLI Enhancements** — `system.environment.args` exposes CLI arguments to scripts, `sys.openUrl` kernel verb, `--browser` flag for automation tools.
- **Lint Infrastructure** — clang-tidy (bug-finding checks), ruff (Python), shellcheck (bash) configs with Makefile targets.

### In Development Since alpha.7 (Apr 7 - Jun 7, 2026)

The two months since alpha.7 have focused on interactive debugging and the substrate for a native REPL. Headlines:

- **Boxen — portable terminal window manager (Phase A)** — A `termbox2`-based window/pane substrate with modal windows, cursor positioning, mouse events (including Cmd-double-click), and a stable input/draw callback model. Shared between the TUI debugger today and the planned Phase C REPL. Lives in `portable/boxen/`.
- **TUI Debugger — `--debug-tui` (Phase B)** — A full-screen interactive debugger for UserTalk scripts. Two-pane source/state view with a keybind footer, F-key step/continue/breakpoint controls, conditional breakpoints (Shift-F9), watchpoints (modal + Cmd-double-click on a local identifier), and a `:` scratch-eval pane that runs `script/eval` against the suspended frame. Hooks the lazy-attach transport so menu-triggered code that runs on `thread.callScript` threads (previously invisible to LLDB) is now debuggable inline. See [`docs/TUI_DEBUGGER_GUIDE.md`](docs/TUI_DEBUGGER_GUIDE.md) for usage and [`docs/TUI_DEBUGGER_ARCHITECTURE.md`](docs/TUI_DEBUGGER_ARCHITECTURE.md) for internals.
- **Lazy-attach debugger transport (PR #722)** — Global atomic transport pointer with a drain-before-free teardown contract. Lets the TUI (and any future in-process attacher) hook the debugger runtime safely, without the use-after-free surface that prevented WebSocket-attached debugging.
- **String safety — `copyctopstring` clamp** — Length-validated copy-to-Pascal-string with explicit truncation surfaced at user-facing callsites. Closes a class of latent overflow bugs in the legacy C surface.
- **Test infrastructure** — Test staging via `cp -R` with md5 drift check protects the canonical `databases/` from test mutation. File-level YAML metadata (`sequential`, `needs_guest_dbs`, `protocol_mode`) drives parallel runner behavior. PTY-driven Python lifecycle tests for TUI clean-exit verification.

**Current Test Status:** ~753 unit tests + 2,186 integration tests passing (21 known-baseline integration failures in html/tcp/startup, unchanged across the window).

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

### Debug a UserTalk Script

Frontier ships two debugger interfaces in the same binary:

```bash
# Interactive: full-screen terminal UI with F-key controls
./frontier-cli/frontier-cli --debug-tui

# Machine-driven: NDJSON over stdio for IDEs and external tools
./frontier-cli/frontier-cli --protocol
```

The two modes are mutually exclusive — pick one per session. The TUI is TTY-only (it opens `/dev/tty` directly); `--protocol` runs over piped stdio.

**TUI keybinds at a glance:** F5 continue, F9 toggle breakpoint, Shift-F9 conditional breakpoint, F10 step-over, F11 step-into, Shift-F11 step-out, `:` open scratch-eval pane, q quit. Cmd-double-click a local identifier in the script pane to set a watchpoint or inspect its value.

Full reference: [`docs/TUI_DEBUGGER_GUIDE.md`](docs/TUI_DEBUGGER_GUIDE.md). For the NDJSON protocol surface, see [`docs/CLI_USAGE_GUIDE.md`](docs/CLI_USAGE_GUIDE.md).

### Next Steps

- **[Getting Started Guide](docs/GETTING_STARTED.md)** - Complete newcomer guide with detailed setup
- **[CLI Usage Guide](docs/CLI_USAGE_GUIDE.md)** - Comprehensive CLI and REPL documentation
- **[TUI Debugger Guide](docs/TUI_DEBUGGER_GUIDE.md)** - Interactive `--debug-tui` reference (keybinds, workflows, limitations)
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
- Security hardening built in from start (SSRF protection, DNS rebinding, data-loss guards, length-clamped Pascal-string conversion)
- ~2,940 tests (~753 unit + 2,186 integration) running in 8-worker parallel, with 21 known-baseline integration failures tracked character-for-character
- Integration tests required for all verb implementations; PTY-driven lifecycle tests for TTY interfaces

**Thread Safety and Database Integrity:**
- GIL-based cooperative threading with real POSIX threads
- databasedata global elimination complete — zero runtime mutation
- v6→v7 migration validated, Y2038-safe 64-bit timestamps throughout
- Context guard pattern for safe concurrent database operations

For comprehensive status details, verb coverage, and milestone snapshots, see [STATUS.md](STATUS.md).

---

## Historical Progress

Historical session summaries live under `planning/progress_reports/README.md`.
