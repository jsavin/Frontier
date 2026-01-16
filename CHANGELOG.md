# Changelog

All notable changes to Frontier CLI will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

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

## Version History Template

For future releases, use this template:

```markdown
## [X.Y.Z] - YYYY-MM-DD

### Added
- New features

### Changed
- Changes to existing functionality

### Deprecated
- Soon-to-be removed features

### Removed
- Removed features

### Fixed
- Bug fixes

### Security
- Security vulnerability fixes
```

---

[Unreleased]: https://github.com/jsavin/Frontier/compare/v1.0.0-alpha.2...HEAD
[1.0.0-alpha.2]: https://github.com/jsavin/Frontier/compare/v1.0.0-alpha.1...v1.0.0-alpha.2
[1.0.0-alpha.1]: https://github.com/jsavin/Frontier/releases/tag/v1.0.0-alpha.1
