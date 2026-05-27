# Frontier CLI

Status
- State: In Progress
- Phase: 1 → 2 transition
- Last Updated: 2025-10-12
- Notes: Headless script execution works; database and network features remain disabled until Phase 3 follow-up milestones.

Related Docs
- planning/DEVELOPER_QUICKSTART_HEADLESS.md
- planning/ui_abstraction/PHASES.md
- planning/ui_abstraction/phase2/analysis.md
- planning/no_ui_linkage_policy.md

Change Log
- 2025-10-12: Refresh documentation to reflect headless script-only build status.
- 2025-09-29: Initialized template sections (Status/Related Docs/Change Log).

## Overview

Frontier CLI is a command-line client for running UserTalk scripts without the legacy GUI. The current headless build focuses on script execution so we can exercise the core runtime in Phase 3 automation work. Database operations, migrations, and network server modes exist in the codebase but remain gated until the surrounding runtime wiring and tests land.

## Features

### Current Capabilities
- Execute UserTalk scripts from files or inline code.
- Headless operation using the shared test adapters (`FRONTIER_HEADLESS`, `shell_api_headless`).
- Load the canonical system root database via `--system-root` to expose runtime tables for scripts.
- Verbose and debug logging toggles.
- Buildable with sanitizers for development use.

### Planned Additions (Phase 3+)
- Database queries, migration helpers, and automated backups.
- HTTP/WebSocket server modes for remote script execution.
- Rich CLI diagnostics for database workflows.
- Hardened error handling around `kernelcall`/`system.verbs` routing.

## Building

### Prerequisites
- macOS 10.15 or later
- Xcode Command Line Tools (`clang`)

### Commands

```bash
# Build the CLI executable (script execution only)
make -C frontier-cli

# Optional: enable ASan/UBSan when available in the toolchain
SANITIZE=1 make -C frontier-cli

# Clean artifacts
make -C frontier-cli clean
```

## Usage

### Script Execution

```bash
# Execute a script file
./frontier-cli/frontier-cli test_script.usertalk

# Execute inline UserTalk
./frontier-cli/frontier-cli -e "local(x = 5); x * 2"

# Enable verbose logging while running inline code
./frontier-cli/frontier-cli -v -e "clock.now()"
```

### Help and Version

```bash
./frontier-cli/frontier-cli --help
./frontier-cli/frontier-cli --version
```

### Disabled Modes

Passing database (`-d`, `-q`, `--migrate`) or server (`--server`, `--websocket`, `--port`) switches currently returns a descriptive error. These paths are reserved for later Phase 3 work once database access and system verb bootstrapping are in place.

### Loading the System Root (optional)

```bash
# Load Frontier.root (or another compatible system root) before executing a script
./frontier-cli/frontier-cli --system-root databases/Guest\ Databases/Frontier.root \
    -e "1 + 1"
```

The system root is opened read-write by default. Use `--lock-opened-roots` (or `FRONTIER_LOCK_OPENED_ROOTS=1` in the environment) to suppress save-on-exit for inspection-only sessions — in-memory mutations still evaluate, but they aren't persisted back to disk. The CLI will log descriptive warnings if the file cannot be located, read, or if optional tables (e.g., `system.misc`, `system.menus`) are missing. The headless loader hydrates the tables it needs in memory so script execution can continue, but the warnings are useful cues that the legacy database still needs migration work.

## Examples

```bash
# Simple arithmetic
./frontier-cli/frontier-cli -e "local(x = 10, y = 20); x + y"

# Run the sample script and capture output
./frontier-cli/frontier-cli test_script.usertalk > /tmp/frontier_cli_output.txt
```

## Command Line Options

| Option | Description | Status |
| ------ | ----------- | ------ |
| `-e, --execute SCRIPT` | Execute inline UserTalk script | Available |
| *(script file argument)* | Execute a script from disk | Available |
| `-v, --verbose` | Verbose logging | Available |
| `--debug` | Enable debug logging | Available |
| `-h, --help` | Show help message | Available |
| `--version` | Show build/version info | Available |
| `--system-root PATH` | Load a system root database before running scripts | Available |
| `--lock-opened-roots` | Suppress save-on-exit for every loaded-from-disk DB; in-memory mutations still work. Also `FRONTIER_LOCK_OPENED_ROOTS=1` | Available |
| `-d, --database FILE` | Select database for operations | Disabled (planned) |
| `-q, --query QUERY` | Execute database query | Disabled (planned) |
| `-m, --migrate` | Migrate database in place | Disabled (planned) |
| `--server` | Start HTTP server | Disabled (planned) |
| `--websocket` | Start WebSocket server | Disabled (planned) |
| `-p, --port PORT` | Override server port | Disabled (planned) |

## Development

### Project Structure

```
frontier-cli/
├── main.c                 # CLI entry point / mode selection
├── cli_parser.c           # Command-line parsing and validation
├── cli_executor.c         # Script compilation and evaluation
├── cli_database.c         # Database helpers (gated; not invoked yet)
├── cli_network.c          # HTTP/WebSocket scaffolding (gated)
├── cli_utils.c            # Logging and utility helpers
├── *.h                    # Shared headers
├── Makefile               # Script-only headless build
├── test_script.usertalk   # Sample script for manual testing
└── README.md              # This document
```

### Building for Development

```bash
# Rebuild with sanitizers enabled
SANITIZE=1 make -C frontier-cli clean all

# Run with verbose + debug to observe runtime init logging
./frontier-cli/frontier-cli --debug -v -e "user.now()"
```

### Testing

For automated coverage, reuse the headless test harness in `tests/` (see `planning/phase3/DEVELOPER_QUICKSTART_HEADLESS.md`). CLI smoke testing today consists of invoking inline scripts and script files as shown above; database and network tests will be added when those modes are enabled.

## Troubleshooting

- **"Database operations are not yet supported"** – Expected when using `-d`, `-q`, or `--migrate`. Phase 3 will enable these switches once database smoke tests exist.
- **"Network server modes are not yet supported"** – Expected when using `--server`, `--websocket`, or `--port`. Server hosting is blocked on system verb bootstrapping.
- **"Failed to load system root database"** – Check the path passed to `--system-root`, confirm the file is readable, and ensure the canonical tables (e.g. `system.verbs`) exist in the database.
- **Script errors** – Review CLI output; rerun with `--debug -v` to capture additional runtime logs.

## Phase 1 Status

✅ **Completed**
- Headless binary that links without AppKit/Carbon/Win32.
- Inline and file-based script execution.
- Verbose/debug logging toggles and shared headless adapters.

🔄 **In Progress**
- Wiring database helpers into the headless runtime.
- Aligning CLI smoke tests with runtime test expansion.
- Preparing network/server paths for system verb bootstrapping.

📋 **Planned**
- Database migration automation (`--auto-migrate`).
- Remote execution modes (HTTP/WebSocket).
- Hardened error handling and regression tests.

## Next Steps

1. Integrate real database open/read/write flows into the headless CLI and add matching tests.
2. Implement system verb bootstrap so CLI/server modes can exercise `kernelcall`.
3. Document and gate database/network usage once functional, then expand runtime test coverage.

For broader context on the modernization effort, see `planning/INDEX.md` and the Phase 3 planning documents.
