# Frontier Refactoring Project (develop branch status)

**Last updated:** 2026-01-25

This repository is actively modernizing the Frontier runtime and toolchain. The `develop` branch now builds and tests with 64-bit alignment on both `arm64` and `x86_64`, includes a portable/headless runtime layer, and routes headless UserTalk `file.*` verbs through the external function processor (EFP) table so tests can exercise real UserTalk without `system.verbs.*` being loaded.

## What's New (Jan 16-25, 2026)

Recent 10-day infrastructure sprint delivered transformative networking and thread-safety foundations:

- **Production-Ready TCP Networking** – 11 socket verbs (client + server operations) with 93 integration tests, SSRF/DNS rebinding protection, and thread-safe stream registry. Frontier can now act as both TCP client and server platform.
- **Thread-Safety Foundation** – Thread registry infrastructure established, deterministic testing framework operational, 11 of 17 thread verbs working. ADR-010 documents roadmap for eliminating global mutable state (Phase 4 requirement).
- **Database Migration Fixes** – Resolved system.paths corruption, path entry name matching, and builtins priority issues. Path resolution and namespace lookups now work correctly.
- **Pre-Release Distribution (v1.0.0-alpha.2)** – Universal binary (arm64+x86_64), automatic system root discovery, professional installer, GitHub Actions automation. Download: [GitHub Releases](https://github.com/jsavin/Frontier/releases/tag/v1.0.0-alpha.2)
- **Infrastructure Improvements** – OPML hierarchical export (eliminates merge conflicts), /doit workflow integration, modular context architecture, comprehensive CLI documentation.

**Overall Progress:** 67% verb coverage (480/710 verbs), 28 processors at 100%, 1,100+ integration tests passing (99% pass rate).

For comprehensive status details, see [STATUS.md](STATUS.md). For the full progress report, see [reports/progress/2026-01-25-networking-foundation-and-thread-safety.md](reports/progress/2026-01-25-networking-foundation-and-thread-safety.md).

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
├── portable/             # Portable runtime layer + stubs
├── frontier-cli/         # Multi-arch CLI build
├── tests/                # Cross-platform C test suite
├── tools/                # Build tools (kernelverbs_parser, strings_compiler)
├── planning/             # Roadmap, ADRs, decisions, quickstarts
├── codex_sessions/       # README pointer (actual logs in codex-sessions branch, no longer used)
├── reports/              # Static analysis and progress reports (generated)
└── build_Xcode_modern/   # Xcode build configuration (multi-arch)
```

---

## Planning & Documentation (Read These First)

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

From recent progress (Jan 16-25):

**Security and Testing:**
- Security hardening built in from start (SSRF protection, DNS rebinding protection for TCP networking)
- Comprehensive test coverage (99% pass rate, 1,100+ tests)
- Integration tests required for all verb implementations

**Thread Safety:**
- Thread-local storage pattern established (ADR-005)
- Deterministic testing infrastructure for concurrent operations
- Global mutable state elimination roadmap documented (ADR-010)

**Database Integrity:**
- v6→v7 migration validated with extensive testing
- Y2038-safe 64-bit timestamps throughout
- Context guard pattern for safe concurrent database operations

**Documentation:**
- ADRs document architectural decisions
- Progress reports capture strategic context
- Planning docs updated alongside code changes

For comprehensive status details, verb coverage, and milestone snapshots, see [STATUS.md](STATUS.md).

---

## Historical Progress

Historical session summaries live under `planning/progress_reports/README.md`.
