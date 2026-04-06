# Changelog

All notable changes to Frontier CLI will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- **Protocol-based UserTalk debugger** - Full debugging support across 7 phases: breakpoints, step into/over/out, watchpoints, multi-thread debugging, conditional breakpoints, and variable inspection
- **system.environment.args** - CLI argument access from UserTalk scripts
- **sys.openUrl kernel verb** - Open URLs from UserTalk
- **Lint infrastructure** - Added clang-tidy, ruff, and shellcheck to CI pipeline

### Changed

- **databasedata global elimination (Phases 1-10)** - Zero runtime mutation of the databasedata global, removing a major source of shared mutable state

### Fixed

- **Data-loss risk mitigations** - Duplicate open guard, fread size validation, and migration lock prevent corruption scenarios
- **19 consistently-failing integration tests** - Resolved long-standing test failures
- **GIL deadlock blocking HTTP callback dispatch** - Fixed deadlock where HTTP callbacks could not acquire the GIL

## [1.0.0-alpha.7] - 2026-02-16

### Added

- **Per-worker database isolation** for parallel test execution
- **Per-component logging** via `FRONTIER_LOG=comp:level` environment variable

### Changed

- **Integration test failures 23 to 0** - All 1,881 integration tests passing
- **Verb processor consolidation** and WP headless support

### Fixed

- **Script path error logging** - Improved error messages for script resolution failures
- **Concurrent access patterns** - Fixed race conditions in shared state access

## [1.0.0-alpha.6] - 2026-02-11

### Added

- **NDJSON protocol mode** - `--protocol` flag enables structured subprocess communication
- **8-worker parallel test execution** - Test suite runs in ~40s, down from 5+ minutes
- **PTY harness** for interactive integration tests
- **Ranger-style file browser** for `file.getFileDialog`

### Changed

- **Integration test failures 755 to 23** - Massive reduction in test failures through systematic fixes
- **Concurrent debugging infrastructure** and platform detection improvements

## [1.0.0-alpha.5] - 2026-02-07

### Added

- **GIL-based cooperative threading** with real POSIX threads
- **Guest database navigation** and REPL index syntax
- **Callback infrastructure** for async operations

### Fixed

- **Startup segfault** - Fixed crash on application launch
- **Thread registry** - Corrected thread lifecycle management
- **stringerrorlist YAML resource migration** - Moved from compiled-in data to external resource

## [1.0.0-alpha.4] - 2026-01-31

### Added

- **CLI state persistence** - System root saves on exit
- **Guest database support** - Open, navigate, and close external databases

### Changed

- **Compiler warnings reduced 154 to 24** - Major cleanup of build warnings

### Fixed

- **Startup stabilization** - Resolved intermittent startup failures
- **Menu loading** - Fixed menu system initialization
- **Integration test hardening** - Improved test reliability

## [1.0.0-alpha.3] - 2026-01-27

### Added

- **Webserver Hello World endpoint** and TCP migration to portable sockets
- **Intel Mac (x86_64) compatibility fixes**

### Changed

- **Database file extension** - Distribution builds now use `.root` extension instead of `.root7`. The v7 format is detected by header magic, not file extension.

### Fixed

- **Critical: Fix dist startup crashes** - Three independent root causes: restored `langexternalsetdatabase()` for cross-database assignment, fixed menu external handling in `getoutlinefromtarget()`, added NULL `param1` guard in `langfunctioncall()`
- **NULL safety in outline traversal** - All 8 traversal functions in `opvisit.c` now check for NULL link pointers
- **Pack logging noise** - Moved PACK diagnostics after dirty check and downgraded to trace level, eliminating ~6,800 log lines per save
- **Nested parentOf() resolution** - Fixed incorrect results for chained parentOf() calls
- **REPL word navigation** - Corrected cursor movement behavior
- **HTTP verb handling** - Fixed request processing issues

## [1.0.0-alpha.2] - 2026-01-15

### Fixed

- **Critical: Preserve user data on upgrade** - Installation script now checks if system root database already exists and prompts before overwriting (default: preserve existing). This prevents data loss when users upgrade to newer versions.

## [1.0.0-alpha.1] - 2026-01-15

### Added - Packaging and Distribution

- **Universal Binary Build**: Support for both Apple Silicon (arm64) and Intel (x86_64) in a single binary
- **Automatic System Root Discovery**: CLI now automatically finds system root database without requiring `--system-root` argument
  - Search paths: `$FRONTIER_ROOT` env var → `~/Library/Application Support/Frontier/` → `~/.frontier/` → current directory
- **Version Embedding**: Git-based version strings embedded at build time via `git describe`
- **Professional Installation**:
  - `install.sh` script for one-command installation
  - Automatic PATH management for zsh/bash
  - Support for both sudo and non-sudo installation
- **Release Automation**:
  - `tools/package_release.sh` - Creates distribution archives
  - GitHub Actions workflow for automated releases on git tags
  - SHA-256 checksums for all release artifacts
- **Documentation**:
  - `INSTALL.md` - Complete installation guide
  - `CHANGELOG.md` - Version history tracking

### Changed

- **System Root Search**: Enhanced auto-discovery replaces hardcoded `databases/Frontier.root` paths
- **Copyright Notice**: Updated to "1992-2026 UserLand Software, Inc. and Contributors"
- **Installation Location**: Default to macOS-standard `~/Library/Application Support/Frontier/` for data

### Technical Details

- Binary size: ~2.8MB (universal), ~1.4MB (single architecture)
- System root database: ~5.8MB (v6), ~9.9MB (v7 migrated)
- Supports macOS 11.0 (Big Sur) and later

### Breaking Changes

None - this is the first pre-release for external distribution.

### Migration Notes

For developers upgrading from local builds:
- System root will auto-migrate from v6 to v7 format on first run
- Database migration creates `.root7` file alongside original `.root` file
- Original v6 files remain unchanged (read-only protected)

---

[Unreleased]: https://github.com/jsavin/Frontier/compare/v1.0.0-alpha.7...HEAD
[1.0.0-alpha.7]: https://github.com/jsavin/Frontier/compare/v1.0.0-alpha.6...v1.0.0-alpha.7
[1.0.0-alpha.6]: https://github.com/jsavin/Frontier/compare/v1.0.0-alpha.5...v1.0.0-alpha.6
[1.0.0-alpha.5]: https://github.com/jsavin/Frontier/compare/v1.0.0-alpha.4...v1.0.0-alpha.5
[1.0.0-alpha.4]: https://github.com/jsavin/Frontier/compare/v1.0.0-alpha.3...v1.0.0-alpha.4
[1.0.0-alpha.3]: https://github.com/jsavin/Frontier/compare/v1.0.0-alpha.2...v1.0.0-alpha.3
[1.0.0-alpha.2]: https://github.com/jsavin/Frontier/compare/v1.0.0-alpha.1...v1.0.0-alpha.2
[1.0.0-alpha.1]: https://github.com/jsavin/Frontier/releases/tag/v1.0.0-alpha.1
