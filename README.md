# Frontier Refactoring Project (develop branch status)

**Last updated:** 2025-10-11  
**State:** Modernization wave 1 delivered; headless + 64-bit aligned  
**Primary contacts:** planning/INDEX.md (owners per phase)

This repository is actively modernising the Frontier runtime and toolchain. The
`develop` branch now builds and tests with 64-bit alignment on both `arm64` and
`x86_64`, includes a portable/headless runtime layer, and routes headless
UserTalk `file.*` verbs through the external function processor (EFP) table so
tests can exercise real UserTalk without `system.verbs.*` being loaded.

## Highlights

- **64-bit/ARM readiness** – All core builds and tests compile cleanly on both
  architectures. Database headers were revved for 64-bit alignment; migration
  coverage lives in `tests/save_migration_tests`.
- **Portable/headless runtime** – The `portable/` layer + headless stubs power
  CLI/testing without UI dependencies (see planning/EFP_HEADLESS_NOTES.md for
  scope/removal criteria).
- **Modernised test harness** – Cross-platform C test suite with sanitiser
  presets (`SANITIZE=1 make -C tests`). New test binaries:
  - `file_portable_tests`
  - `file_readline_tests`
  - `file_verb_tests` (UserTalk `file.*` exercises headless EFP routing)
- **Branch hygiene** – Large Codex session logs moved off `develop` and live in
  the dedicated `codex-sessions` branch (see below for how to fetch).

## Quick start

```bash
# build + run headless tests (multi-arch ready)
make -C tests file_verb_tests && ./tests/file_verb_tests
./tests/file_portable_tests
./tests/file_readline_tests

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
- **Windows:** install the MariaDB Connector/C package separately and set the
  environment variable `MYSQL_CLIENT_DIR` to the root of the installation (the
  VC projects look for headers under `include\mysql` and libraries under
  `lib`). Copying the Windows libraries into `Common\MySQL\` works as well, but
  they remain untracked by git.

See `docs/mysql_client_setup.md` for detailed guidance.
```

> `make -C tests test` currently hits a pre-existing duplicate-symbol linker
> issue in `runtime_tests`; tracked in planning/ISSUES.md.

## Planning & docs (read these first)

- `planning/INDEX.md` – roadmap + ownership
- `planning/DECISIONS.md` – current decisions/TBDs
- `planning/EFP_HEADLESS_NOTES.md` – headless shim, success criteria, removal
  plan
- `planning/adr/ADR-0010-headless-efp-routing.md` – decision record for dotted
  call routing
- `planning/Frontier_Refactoring_Plan.md` – original modernisation plan
- `codex_sessions/README.md` – how to fetch/view Codex transcript logs

For daily notes and context, see the Codex session branch (instructions below).

## Current status matrix

| Area                | Status | Notes |
|---------------------|:------:|-------|
| 64-bit alignment    | ✅     | DB header rev complete; save/migration tests green |
| arm64 build         | ✅     | `make -C frontier-cli` builds universal binary |
| Headless runtime    | ✅     | Portable stubs cover runtime/IO; EFP shim in place |
| Tests (targeted)    | ✅     | `file_portable`, `file_readline`, `file_verb` |
| Tests (full suite)  | ⚠️     | `runtime_tests` link failure (known issue) |
| Docs/Planning       | ✅     | Planning/ADR files updated alongside code |
| Codex transcripts   | ✅     | Stored on `codex-sessions` branch/worktree |

## Codex session logs

Large transcript files live on the `codex-sessions` branch. Fetch once and keep
them in a separate worktree so they do not clutter `develop`:

```bash
git fetch origin codex-sessions
git worktree add ../Frontier-codex-sessions codex-sessions   # once
```

Drop new transcripts into `../Frontier-codex-sessions/codex_sessions/`, commit
there, and push. Details are in `codex_sessions/README.md`.

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

1. Branch from `develop` and keep changes small.
2. Update/consult planning docs before coding (PRs reference the appropriate
   ADR/decision where possible).
3. Run targeted tests locally; note known failures when applicable.
4. Update docs and tests alongside code; add Codex notes if significant.
5. Open PRs against `develop` (multi-arch + headless tests should remain green).

## Next milestone snapshot

- Remove temporary headless shim once `Frontier.root` can load in headless
  builds (restores classic `system.verbs.* → kernelcall → EFP` routing).
- Fix duplicate-symbol linker issue in `runtime_tests` so `make -C tests test`
  is green.
- Bring CI online (provider TBD) and enable coverage/static analysis once tool
  chain is finalised.

For day-by-day progress see the `codex-sessions` branch and
planning/INDEX.md.
