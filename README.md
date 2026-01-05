# Frontier Refactoring Project (develop branch status)

**Last updated:** 2026-01-05
**State:** Phase 1 kernel verb implementation underway; file verbs 100% complete (86/86); lang verbs in progress (10/61); overall 37% coverage (264/710 verbs); headless + 64-bit aligned; v7 on-disk format stable
**Primary contacts:** planning/INDEX.md (owners per phase)

This repository is actively modernizing the Frontier runtime and toolchain. The `develop` branch now builds and tests with 64-bit alignment on both `arm64` and `x86_64`, includes a portable/headless runtime layer, and routes headless UserTalk `file.*` verbs through the external function processor (EFP) table so tests can exercise real UserTalk without `system.verbs.*` being loaded.

## Highlights

- **Comprehensive kernel verb implementation** – Phase 1 complete: **file verbs 100% (86/86)** with full support for volume ops, locking, timestamps, creator/type handling. **Lang verbs in progress (10/61 implemented)** with focus on type conversion, utility, and core operations. **Overall 37% coverage (264/710 verbs)** across 51 processors. All implementations tested via integration test framework (52+ file tests, 220+ lang tests).
- **64-bit/ARM + big-endian v7** – Core builds/tests compile on `arm64`/`x86_64`; v7 headers/trailers and table addresses write big-endian for cross-arch parity. Hash pack/unpack hardened with explicit 16-byte BE buffers, bounds checks, header detection, and optional logging. Migration coverage in `tests/save_migration_tests` and `tests/runtime_tests`.
- **Portable/headless + Paige-free** – The `portable/` layer + headless stubs power CLI/testing without UI deps; wptext uses Paige-free extractor/RTF path. v6→v7 migration complete with proper timestamp handling (64-bit frontier_time_t, Year 2038 safe).
- **Automated kernel verb generation** – Python-based parser (`tools/kernelverbs_parser/`) automatically generates `kernel_verbs_init.c` from `kernelverbs.rc`, extracting all 51 EFP processor definitions (707 verbs). Next phase: automatic implementation detection via static analysis (roadmap in `planning/phase3/kernel_verb_porting/automatic_verb_binding_architecture.md`).
- **Modernized test harness** – Cross-platform C test suite with sanitizer presets (`SANITIZE=1 make -C tests`). Integration test framework supports YAML-based verb testing with path templating for sandbox safety. All 52 file verb tests passing; 220+ lang verb tests passing.
- **Critical architectural documentation** – typeof() OSType code behavior documented to prevent future regressions (commit 5456c5eb). ADR-005 thread-local parameter state fully integrated. Database context debugging patterns captured in CLAUDE.md.
- **Repository hygiene** – 78 branches cleaned up (88→3 active local branches). All zombie branches (merged PRs) and stale/superseded work removed. Permanent archive branches preserved (archive/codex-sessions for session transcripts).

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
| arm64 build        |   ✅    | `make -C frontier-cli` builds universal binary; all tests pass on arm64/x86_64          |
| Headless runtime   |   ✅    | Portable stubs cover runtime/IO; EFP shim stable; parameter state thread-local          |
| File verbs         |   ✅    | 100% complete (86/86); all integration tests passing (52/52); locking/volumes working  |
| Lang verbs         |   🚧    | 16% complete (10/61); type conversion, utility, core operations prioritized            |
| Tests (integrated) |   ✅    | YAML-based integration framework; 272+ tests passing (file+lang); sandbox-safe paths   |
| Tests (runtime/db) |   ✅    | `runtime_tests`, `db_format_tests`; full `SANITIZE=1` passes; Year 2038 safe          |
| CLI runtime        |   ⚠️   | `cli_runtime_tests` passes; clock/date stubs still pending (lower priority)            |
| Docs/Planning      |   ✅    | Architecture docs updated; typeof() critical lesson documented; branch cleanup logged  |
| Repository state   |   ✅    | 78 branches cleaned up; only active work + permanent archives remain                   |

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

**Immediate priorities (next 2 weeks):**
1. **Complete Phase 1 lang verb implementations** (~15 more verbs) – Focus on table, outline, and script operations needed for CLI runtime integration
2. **Automatic verb binding architecture** (static analysis + metadata generation, 1–2 weeks) – Eliminates manual whitelist maintenance across 707 verbs in 51 processors; design complete, implementation phase 1 (analyzer core) ready to start
3. **Clock and date verb stubs** – Complete remaining date/time operations for CLI runtime
4. **Integration test expansion** – Add verb tests for remaining Phase 1 verbs as they're implemented

**Short-term (2–4 weeks):**
- Implement automatic verb binding phases 2–3 (verification, test infrastructure, full codebase integration)
- Begin Phase 2 verb implementations (table/outline/window operations with selective headless support)
- Refactor BE pack/unpack helpers to reduce manual memcpy repetition (issue #77)

**Medium-term (1–2 months):**
- Add cross-arch BE64 serialization verification with golden blobs (issue #78)
- Expand headless verb coverage to include dialogue, file selection, and other interactive operations
- Design and implement collaborative ODB foundation (Phase 2.0) for multi-user support

**CI/Infrastructure:**
- Bring CI online (provider TBD) with verb coverage tracking
- Automated regression testing for verb implementations
- Performance benchmarking for database operations

For detailed planning see `planning/INDEX.md` and current work in `planning/_CURRENT_STATUS.md`.
