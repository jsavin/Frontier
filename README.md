# Frontier Refactoring Project (develop branch status)

**Last updated:** 2025-11-23  
**State:** Modernization wave 2 in progress; headless + 64-bit aligned; v7 on-disk format moving to portable big-endian  
**Primary contacts:** planning/INDEX.md (owners per phase)

This repository is actively modernising the Frontier runtime and toolchain. The
`develop` branch now builds and tests with 64-bit alignment on both `arm64` and
`x86_64`, includes a portable/headless runtime layer, and routes headless
UserTalk `file.*` verbs through the external function processor (EFP) table so
tests can exercise real UserTalk without `system.verbs.*` being loaded.

## Highlights

- **64-bit/ARM + big-endian v7** – Core builds/tests compile on `arm64`/`x86_64`; v7 headers/trailers and table addresses now write big-endian for cross-arch parity (see `docs/database_architecture.md`). Migration coverage lives in `tests/save_migration_tests` and `tests/runtime_tests`.
- **Portable/headless + Paige-free** – The `portable/` layer + headless stubs power CLI/testing without UI deps; wptext now uses the Paige-free extractor/RTF path while still allowing tests to link the real Paige for parity checks.
- **Modernised test harness** – Cross-platform C test suite with sanitiser presets (`SANITIZE=1 make -C tests`). Key binaries: `file_portable_tests`, `file_readline_tests`, `file_verb_tests`, `runtime_tests`, `db_format_tests`, `cli_runtime_tests`.
- **Branch hygiene** – Large Codex session logs live on the `codex-sessions` branch; planning docs capture status/decisions in `planning/_CURRENT_STATUS.md`, `planning/DECISIONS.md`, and `planning/big_endian_portability_audit.md`.

## Quick start

```bash
# build + run headless tests (multi-arch ready)
make -C tests file_verb_tests && ./tests/file_verb_tests
./tests/file_portable_tests
./tests/file_readline_tests
make -C tests runtime_tests

# sanitiser run
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
- `planning/EFP_HEADLESS_NOTES.md` – headless shim, success criteria, removal
  plan
- `planning/adr/ADR-0010-headless-efp-routing.md` – decision record for dotted
  call routing
- `planning/Frontier_Refactoring_Plan.md` – original modernisation plan
- `planning/phase3/headless_daemon_vision.md` – target architecture for the headless daemon/service core
- `planning/big_endian_portability_audit.md` – current BE v7 portability audit/tasks
- `codex_sessions/README.md` – how to fetch/view Codex transcript logs

For in-flight work/status, see `planning/_CURRENT_STATUS.md`. Historical session context lives in `planning/progress_reports/README.md`.

## Current status matrix

| Area                | Status | Notes |
|---------------------|:------:|-------|
| 64-bit alignment    | ✅     | DB header rev complete; save/migration tests green |
| arm64 build         | ✅     | `make -C frontier-cli` builds universal binary |
| Headless runtime    | ✅     | Portable stubs cover runtime/IO; EFP shim in place |
| Tests (targeted)    | ✅     | `file_portable`, `file_readline`, `file_verb` |
| Tests (runtime/db)  | ✅     | `runtime_tests`, `db_format_tests` (warnings remain) |
| CLI runtime         | ⚠️     | `cli_runtime_tests` built; re-enable `clock.now()` once v7 BE work finishes |
| Docs/Planning       | ✅     | Planning/ADR files updated alongside code |
| Codex transcripts   | ✅     | Stored on `codex-sessions` branch/worktree |

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
├── planning/             # Roadmap, ADRs, decisions, quickstarts
├── codex_sessions/       # README pointer (actual logs in codex-sessions branch)
└── build_*               # Build scaffolding (Xcode/GNU)
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

- Finish big-endian v7 portability (header/trailer/table/avail) and add cross-arch goldens.
- Re-enable full CLI runtime coverage (e.g., `clock.now()` regression).
- Bring CI online (provider TBD) and enable coverage/static analysis once tool chain is finalised.

For day-by-day progress see the `codex-sessions` branch and
planning/INDEX.md.
