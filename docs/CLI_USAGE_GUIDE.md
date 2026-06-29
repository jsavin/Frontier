# Frontier CLI Usage Guide

**Version:** 1.2.0
**Last Updated:** 2026-01-31

---

## Table of Contents

1. [Overview](#overview)
2. [Installation](#installation)
3. [Command Line Options](#command-line-options)
4. [Execution Modes](#execution-modes)
5. [Environment Variables](#environment-variables)
6. [Working with Databases](#working-with-databases)
7. [Examples](#examples)
8. [Troubleshooting](#troubleshooting)
9. [Advanced Usage](#advanced-usage)
10. [REPL Keybindings (Boxen REPL)](#repl-keybindings-boxen-repl)
11. [REPL Navigation and Guest Databases](#repl-navigation-and-guest-databases)

---

## Overview

`frontier-cli` is the command-line interface for executing UserTalk scripts in headless mode. It provides a modern, scriptable way to run Frontier code without requiring the full GUI application.

### Key Features

- **Inline Script Execution**: Run UserTalk code directly from the command line
- **Script File Execution**: Execute `.usertalk` script files
- **Database Support**: Load and interact with Frontier database files (`.root`)
- **Database Migration**: Upgrade v6 databases to v7 format (in-place with `.v6.root` backup)
- **Configurable Logging**: Control verbosity and debug output via environment variables
- **Exit Codes**: Returns 0 on success, 1 on failure for shell scripting integration

---

## Installation

The CLI is built as part of the Frontier project:

```bash
# From the project root directory
make -C frontier-cli

# The executable will be at: ./frontier-cli/frontier-cli
```

### Running from Anywhere

To run `frontier-cli` from any directory, either:

1. Add the frontier-cli directory to your PATH:
   ```bash
   export PATH="/path/to/Frontier/frontier-cli:$PATH"
   ```

2. Create a symlink in a directory that's already in your PATH:
   ```bash
   ln -s /path/to/Frontier/frontier-cli/frontier-cli /usr/local/bin/frontier-cli
   ```

---

## Command Line Options

### Core Options

| Option | Long Form | Argument | Description |
|--------|-----------|----------|-------------|
| `-e` | `--execute` | `SCRIPT` | Execute inline UserTalk script |
| | `--system-root` | `PATH` | Load system root database before executing scripts |
| | `--migrate` | `PATH` | Migrate v6 database to v7 format in-place and exit |
| | `--output` | `PATH` | Output path for migrated database (use with `--migrate`) |
| `-f` | `--force` | | Force overwrite if output file exists (use with `--migrate`) |
| `-b` | `--batch` | | Batch mode (disable interactive prompts) |
| | `--non-interactive` | | Alias for `--batch` |
| `-v` | `--verbose` | | Enable verbose output |
| | `--debug` | | Enable debug mode |
| | `--debug-tui` | `[PATH]` | **DEPRECATED no-op alias** for the default (boxen REPL).  Originally the opt-in to boxen; preserved so existing scripts and aliases keep working.  Emits a one-line deprecation warning at startup.  Will be removed in a future release. |
| | `--plain` | | Launch the **legacy linenoise REPL** instead of the default boxen REPL.  Single-line prompt, raw terminal IO.  Mutually exclusive with `--debug-tui` and `--protocol`. |
| | `--lock-opened-roots` | | Suppress save-on-exit for every loaded-from-disk DB; in-memory mutations still work. Also `FRONTIER_LOCK_OPENED_ROOTS=1` |
| `-h` | `--help` | | Show help message |
| | `--version` | | Show version information |

### Option Details

#### `-e, --execute SCRIPT`

Execute UserTalk code directly from the command line. The script is passed as a string argument.

**Usage:**
```bash
./frontier-cli/frontier-cli -e "1 + 1"
./frontier-cli/frontier-cli --execute "msg('Hello, World!')"
```

**Multi-line Scripts:**

For multi-line scripts, use bash `$'...'` syntax:

```bash
./frontier-cli/frontier-cli -e $'local(x = 5);\nlocal(y = 10);\nreturn x + y'
```

Or use semicolons to separate statements:

```bash
./frontier-cli/frontier-cli -e "local(x = 5); local(y = 10); return x + y"
```

#### `--system-root PATH`

Load a Frontier database file before executing scripts. This makes all tables and scripts in the database available to your code.

**Usage:**
```bash
./frontier-cli/frontier-cli --system-root databases/Frontier.root -e "sizeOf(system)"
```

**Positional Database Loading:**

You can also load a database by passing it as a positional argument (without the `--system-root` flag):

```bash
./frontier-cli/frontier-cli databases/Frontier.root -e "sizeOf(system)"
```

Files ending in `.root` are automatically treated as system root databases. (`.root7` is also recognized for backward compatibility but is deprecated.) You cannot use both a positional database argument and the `--system-root` flag in the same command.

**Automatic Migration:**

If you specify a v6 database, the CLI will automatically migrate it to v7 format in-place (backing up the v6 file to `.v6.root`):

```bash
# This will migrate Frontier.root in-place if it's v6
./frontier-cli/frontier-cli --system-root databases/Frontier.root -e "1"

# Same behavior with positional argument
./frontier-cli/frontier-cli databases/Frontier.root -e "1"
```

#### `--migrate PATH`

Migrate a v6 database to v7 format and exit. This is a standalone operation that doesn't load the system root or execute any scripts.

**Basic Usage** (in-place: renames v6 to `.v6.root`, writes v7 to the original `.root` path):
```bash
./frontier-cli/frontier-cli --migrate databases/Frontier.root
```

**Output:**
```
Migrated: databases/Frontier.root (v6 backed up to databases/Frontier.v6.root)
```

**With Custom Output Path** (leaves v6 untouched, writes v7 to PATH):
```bash
./frontier-cli/frontier-cli --migrate legacy/Frontier.root --output databases/Frontier.root
```

**Force Overwrite Existing File:**
```bash
./frontier-cli/frontier-cli --migrate Frontier.root --output /tmp/Frontier.root -f
```

**Notes:**
- Without `--output`: v6 is renamed to `.v6.root` backup, v7 is written to the original `.root` path
- With `--output`: v6 file is left untouched, v7 is written to the specified path
- If the input is already v7 format, prints "Already v7 format" and exits
- Exit code 0 on success, 1 on error
- Use `--force` (`-f`) to overwrite an existing output file

#### `-b, --batch, --non-interactive`

Force batch (non-interactive) mode, disabling all interactive prompts. This is useful for:
- Automated testing and CI/CD pipelines
- Scripted workflows that should never prompt for user input
- Reproducible builds where interactive input would cause inconsistency
- Running in non-TTY environments (pipes, redirects, daemons)

**Behavior:**
- Interactive dialog verbs (`dialog.ask`, `dialog.getPassword`, etc.) return errors instead of prompting
- File dialog verbs (`file.getFileDialog`, `file.putFileDialog`, etc.) return errors instead of prompting
- Prevents scripts from hanging waiting for user input

**Auto-Detection:**
By default, the CLI automatically detects whether it's running in an interactive terminal:
- **Interactive mode** (TTY detected): Interactive prompts are allowed
- **Batch mode** (no TTY or `CI` environment variable set): Interactive prompts return errors

The `--batch` flag explicitly forces batch mode even when running from a terminal.

**Usage:**
```bash
# Force batch mode (even from terminal)
./frontier-cli/frontier-cli --batch -e "dialog.ask('Continue?')"
# Error: Interactive prompts not available in batch mode

# Short form
./frontier-cli/frontier-cli -b -e "1 + 1"

# Longer alias (GNU style)
./frontier-cli/frontier-cli --non-interactive -e "1 + 1"
```

**Use Cases:**
```bash
# CI/CD pipeline (auto-detected)
CI=true ./frontier-cli/frontier-cli -e "run_tests()"

# Automated testing (explicit batch mode)
./frontier-cli/frontier-cli --batch -e "test_suite()"

# Piped input (auto-detected as batch)
echo "1 + 1" | ./frontier-cli/frontier-cli -e -

# Terminal interactive (default)
./frontier-cli/frontier-cli -e "dialog.ask('Continue?')"
# Prompts for input
```

**See Also:** `planning/phase3/HEADLESS_INTERACTIVE_MODE.md` for complete interactive vs batch mode documentation.

#### `-v, --verbose`

Enable verbose logging output. Shows INFO-level messages and above.

**Usage:**
```bash
./frontier-cli/frontier-cli -v -e "1 + 1"
```

#### `--debug`

Enable debug mode. Shows DEBUG-level messages and above.

**Usage:**
```bash
./frontier-cli/frontier-cli --debug -e "1 + 1"
```

#### REPL selection (`--plain` / `--debug-tui` / default)

**As of milestone C.6 (2026-06-25)**, `frontier-cli` (no flag) launches the **boxen REPL** — a multi-pane terminal UI with composited windows, native dialog modals, and a slash-menu palette.  The previous default (legacy linenoise REPL) is now opt-in via `--plain`.

**When to use which REPL.**

`frontier-cli` (default, **boxen**):
- Multi-window UX (editor windows can coexist with the REPL — required for `/edit @path`)
- Composited stdout/stderr so script output never corrupts the prompt
- Native modal dialog prompts (`dialog.*` and `file.*` render as boxen modals rather than raw-terminal prompts)
- Slash-menu palette as a boxen modal
- Best for almost all interactive use today.  Full feature comparison: [`BOXEN_REPL_PARITY.md`](BOXEN_REPL_PARITY.md)

`frontier-cli --plain` (legacy linenoise):
- Single-line prompt, raw terminal IO
- Use when boxen can't run cleanly (extremely small terminals, unusual ttys) or when you want the historical behavior
- Preserved indefinitely as the explicit fallback; will be deleted only after a release cycle of post-C.6 soak

`frontier-cli --debug-tui` (**DEPRECATED no-op alias**):
- Originally the opt-in for boxen (Phase B.0 through C.5); now the default
- Still accepted so existing scripts and aliases keep working
- Emits a one-line deprecation warning at startup
- Will be removed in a future release.  Drop the flag from new scripts.

`frontier-cli --protocol`: see the protocol-mode docs.  No REPL is launched.

**Usage:**
```bash
# Default: boxen REPL.
./frontier-cli/frontier-cli

# With a system root loaded.
./frontier-cli/frontier-cli --system-root databases/Frontier.root

# Auto-spawn a suspended debug thread on a script (positional path).
./frontier-cli/frontier-cli --system-root databases/Frontier.root @workspace.foo
./frontier-cli/frontier-cli --system-root databases/Frontier.root path/to/script.ut

# Explicit legacy fallback.
./frontier-cli/frontier-cli --plain --system-root databases/Frontier.root
```

**Key bindings inside the boxen REPL.**

| Key | Action |
|-----|--------|
| `/` | Open the slash-menu palette after a brief debounce (350 ms) |
| `Esc` | Close any open modal (palette, dialog prompt) without dispatching |
| `Ctrl-C` | Cancel the current modal if one is open; otherwise quit the REPL |
| `Enter` | Submit input — either the input bar or the active modal |
| Arrow keys | Navigate within the input bar; navigate within a modal |

**Known limitations as of 2026-06-25** (track via the issues in [`BOXEN_REPL_PARITY.md`](BOXEN_REPL_PARITY.md)):

- Dialog input fields are capped at 256 bytes (#795).  Long paths and passphrases are silently truncated.
- `repl.printKeyCodes()` corrupts the boxen framebuffer if invoked (#794).  Use `--plain` if you need it.
- Non-ASCII codepoints are dropped on input (#796).
- Outer modal loses focus for ~100ms when an inner modal closes (#798).

---

## Execution Modes

### Inline Script Execution

Execute UserTalk code directly from the command line using the `-e` or `--execute` option.

**Basic Example:**
```bash
./frontier-cli/frontier-cli -e "1 + 1"
```

**Working with Variables:**
```bash
./frontier-cli/frontier-cli -e "local(x = 42); return x * 2"
```

**Calling Verbs:**
```bash
./frontier-cli/frontier-cli -e "string.upper('hello world')"
```

### Script File Execution

Execute UserTalk code from a `.usertalk` file.

**Create a Script File:**
```usertalk
// myscript.usertalk
local(x = 5);
local(y = 10);
msg("The sum is: " + (x + y));
return x + y
```

**Execute the Script:**
```bash
./frontier-cli/frontier-cli myscript.usertalk
```

### Database-Backed Execution

Load a Frontier database and execute scripts that interact with its contents.

**Using --system-root flag:**
```bash
./frontier-cli/frontier-cli --system-root databases/Frontier.root -e "sizeOf(system.verbs)"
```

**Using positional argument:**
```bash
# Database before -e flag
./frontier-cli/frontier-cli databases/Frontier.root -e "sizeOf(system.verbs)"

# Database after -e flag (argument order is flexible)
./frontier-cli/frontier-cli -e "sizeOf(system.verbs)" databases/Frontier.root
```

The CLI displays which database was loaded at startup:
```
[startup-INFO] Loaded system root: databases/Frontier.root
16
```

If a v6 database is migrated automatically, you'll see:
```
[startup-INFO] Loaded system root: databases/Frontier.root (migrated from v6 to v7)
```

---

## Environment Variables

The CLI respects several environment variables for controlling logging and behavior.

### `FRONTIER_LOG_LEVEL`

Set the minimum log level to display.

**Values:** `TRACE`, `DEBUG`, `INFO`, `WARN`, `ERROR`

**Example:**
```bash
export FRONTIER_LOG_LEVEL=DEBUG
./frontier-cli/frontier-cli -e "1 + 1"
```

### `FRONTIER_LOG_COMPONENT`

Filter logs to show only specific components.

**Values:** `DB`, `HASH`, `TABLE`, `LANG`, `GENERAL`, `STARTUP`, etc.

**Example:**
```bash
export FRONTIER_LOG_COMPONENT=DB
./frontier-cli/frontier-cli --system-root databases/Frontier.root -e "1"
```

**Multiple Components:**
```bash
export FRONTIER_LOG_COMPONENT=DB,LANG
```

### `FRONTIER_LOG_FORMAT`

Control log message formatting.

**Values:** `text` (default), `json`

**Example:**
```bash
export FRONTIER_LOG_FORMAT=json
./frontier-cli/frontier-cli -e "1 + 1"
```

### `FRONTIER_HEADLESS_RUN_STARTUP`

Control execution of `system.startup` scripts when loading a database.

**Default behavior:** Startup scripts **run by default** (matching legacy Frontier behavior).

To skip startup scripts, use the `--skip-startup` CLI flag or set `FRONTIER_HEADLESS_RUN_STARTUP=0`:

**Skip startup scripts:**
```bash
# Using CLI flag (recommended)
./frontier-cli/frontier-cli --skip-startup --system-root databases/Frontier.root -e "1"

# Or using environment variable
export FRONTIER_HEADLESS_RUN_STARTUP=0
./frontier-cli/frontier-cli --system-root databases/Frontier.root -e "1"
```

**Default behavior (startup scripts run):**
```bash
# Startup scripts execute automatically - matches legacy Frontier
./frontier-cli/frontier-cli --system-root databases/Frontier.root -e "1"
```

> **Note:** Prior to v1.0.0-alpha.5, startup scripts were skipped by default. The default was changed to match legacy Frontier behavior where `system.startup` scripts always run on launch.

### `FRONTIER_FORCE_COLOR`

Force ANSI color in REPL error rendering even when stderr is not a TTY.

The interactive REPL's structured error display (header + per-frame source-context window) emits ANSI bold/underline sequences when stderr is a TTY and plain `>>>` / `^^^` markers otherwise. Set `FRONTIER_FORCE_COLOR=1` to force the ANSI path regardless of TTY detection — useful when piping the REPL into a pager that interprets ANSI (`less -R`), or when capturing colored output for review.

```bash
FRONTIER_FORCE_COLOR=1 ./frontier-cli/frontier-cli | less -R
```

Any value other than the literal string `1` falls through to the `isatty(stderr)` check (so unset and `0` are equivalent: no force, isatty wins).

---

## Working with Databases

### Database Formats

Frontier uses two database formats:

- **v6** (Legacy): 32-bit addresses, used by classic Frontier
- **v7** (Modern): 64-bit addresses, used by headless Frontier

The CLI **only loads v7 databases** but will automatically migrate v6 databases when needed.

### Loading a Database

Use the `--system-root` option to load a database:

```bash
./frontier-cli/frontier-cli --system-root databases/Frontier.root -e "defined(system)"
```

### Automatic Migration

When you specify a v6 database, the CLI automatically:

1. Detects the database format
2. Renames the v6 file to `.v6.root` (backup)
3. Writes the migrated v7 database to the original `.root` path
4. Loads the v7 database

**Example:**
```bash
# First run: migrates in-place, v6 backed up to Frontier.v6.root
./frontier-cli/frontier-cli --system-root databases/Frontier.root -e "1"

# Subsequent runs: uses existing v7 Frontier.root directly
./frontier-cli/frontier-cli --system-root databases/Frontier.root -e "1"
```

### Manual Migration

To migrate a database without executing scripts:

```bash
./frontier-cli/frontier-cli --migrate databases/Frontier.root
```

**Output:**
```
Migrated: databases/Frontier.root (v6 backed up to databases/Frontier.v6.root)
```

### Accessing Database Contents

Once a database is loaded, you can access its tables and scripts:

```bash
# Check if system table exists
./frontier-cli/frontier-cli --system-root databases/Frontier.root -e "defined(system)"

# Get size of system.verbs table
./frontier-cli/frontier-cli --system-root databases/Frontier.root -e "sizeOf(system.verbs)"

# List top-level tables
./frontier-cli/frontier-cli --system-root databases/Frontier.root -e "getTableNames()"
```

---

## Examples

### Basic Arithmetic

```bash
./frontier-cli/frontier-cli -e "1 + 1"
# Output: 2

./frontier-cli/frontier-cli -e "42 * 2"
# Output: 84
```

### String Operations

```bash
./frontier-cli/frontier-cli -e "string.upper('hello')"
# Output: HELLO

./frontier-cli/frontier-cli -e "string.length('Frontier')"
# Output: 8
```

### Working with Variables

```bash
./frontier-cli/frontier-cli -e "local(x = 5); local(y = 10); return x + y"
# Output: 15
```

### Multi-line Scripts

```bash
./frontier-cli/frontier-cli -e $'local(x = 5);\nlocal(y = 10);\nreturn x * y'
# Output: 50
```

### Creating Tables

```bash
./frontier-cli/frontier-cli -e $'lang.new(tableType, @t);\nt.key1 = "hello";\nt.key2 = 42;\nreturn sizeOf(t)'
# Output: 2
```

### Database Loading

**Positional Database Argument:**

You can load a database by passing it as a positional argument:

```bash
# Basic usage
./frontier-cli/frontier-cli databases/Frontier.root -e "sizeOf(system)"
# Output:
# [startup-INFO] Loaded system root: databases/Frontier.root
# 16
```

**Flexible Argument Ordering:**

The database can appear before or after the `-e` flag:

```bash
# Database before -e
./frontier-cli/frontier-cli databases/Frontier.root -e "1+1"

# Database after -e
./frontier-cli/frontier-cli -e "1+1" databases/Frontier.root
```

**Auto-Migration from v6 to v7:**

When loading a v6 database, automatic migration occurs in-place and is indicated in the output:

```bash
./frontier-cli/frontier-cli databases/Frontier.root -e "1+1"
# Output:
# [startup-INFO] Loaded system root: databases/Frontier.root (migrated from v6 to v7)
# 2
```

**Traditional --system-root Flag:**

The traditional flag syntax still works:

```bash
./frontier-cli/frontier-cli --system-root databases/Frontier.root -e "sizeOf(system)"
```

### Database Queries

```bash
# Check system table size (positional argument)
./frontier-cli/frontier-cli databases/Frontier.root -e "sizeOf(system)"

# List system.verbs subtables (traditional flag)
./frontier-cli/frontier-cli --system-root databases/Frontier.root -e "sizeOf(system.verbs)"
```

### Script Files

**Create `example.usertalk`:**
```usertalk
local(result = 0);
local(i);

for i = 1 to 10 {
    result = result + i
};

msg("Sum of 1 to 10 is: " + result);
return result
```

**Execute:**
```bash
./frontier-cli/frontier-cli example.usertalk
```

### Database Migration

```bash
# Migrate and verify
./frontier-cli/frontier-cli --migrate databases/Frontier.root

# Use the migrated database
./frontier-cli/frontier-cli --system-root databases/Frontier.root -e "defined(system)"
```

### Interactive Mode

**Dialog Prompts:**
```bash
# Yes/No prompt (arrow keys to select, Enter to confirm)
./frontier-cli/frontier-cli -e 'dialog.ask("Continue?")'

# Integer input (Enter accepts default [10])
./frontier-cli/frontier-cli -e 'dialog.getInt("Count?", 10)'

# String input (Enter accepts default)
./frontier-cli/frontier-cli -e 'dialog.getString("Name?", "default")'

# Password input (shows dots: ••••••)
./frontier-cli/frontier-cli -e 'dialog.getPassword("Password:")'
```

**File Dialogs:**
```bash
# Select existing file (tab completion enabled)
./frontier-cli/frontier-cli -e 'file.getFileDialog("/tmp")'

# Browse and create new file
./frontier-cli/frontier-cli -e 'file.putFileDialog("/tmp")'

# Select folder
./frontier-cli/frontier-cli -e 'file.getFolderDialog("/tmp")'

# Select disk volume
./frontier-cli/frontier-cli -e 'file.getDiskDialog()'
```

**Batch Mode (disable prompts):**
```bash
# Force errors instead of prompting
./frontier-cli/frontier-cli --batch -e 'dialog.ask("Continue?")'
# Error: Can't use dialog verbs in batch mode
```

**Auto-detection:** Interactive prompts work when stdin/stdout are TTYs. Use `--batch` to force errors in CI/CD or scripts.

---

## Troubleshooting

### Common Issues

#### Script Execution Fails with "No execution mode specified"

**Problem:** You forgot to specify a script or inline code.

**Solution:** Provide either `-e "code"` or a script file path:
```bash
./frontier-cli/frontier-cli -e "1 + 1"
# or
./frontier-cli/frontier-cli myscript.usertalk
```

#### "System root does not exist"

**Problem:** The database file path is incorrect.

**Solution:** Verify the path and use an absolute path if needed:
```bash
./frontier-cli/frontier-cli --system-root /absolute/path/to/database.root -e "1"
```

#### "Error: System root already specified via --system-root"

**Problem:** You used both a positional database argument and the `--system-root` flag in the same command.

**Solution:** Use only one method to specify the database:
```bash
# Use positional argument only
./frontier-cli/frontier-cli databases/Frontier.root -e "1+1"

# OR use --system-root flag only
./frontier-cli/frontier-cli --system-root databases/Frontier.root -e "1+1"

# NOT both (this will error)
./frontier-cli/frontier-cli --system-root databases/A.root databases/B.root -e "1"
```

#### Database Migration Errors

**Problem:** Migration fails with database errors.

**Solution:**
1. Check the v6 database is valid and not corrupted
2. Check file permissions (v6 database should be readable)
3. Ensure sufficient disk space for the v7 database
4. Check logs with `--debug` for details

#### "Can't call [verb] because it isn't implemented"

**Problem:** The verb you're calling isn't implemented in headless mode yet.

**Solution:** Check the verb implementation status:
```bash
cd tools/kernelverbs_parser
python3 cli.py report
```

### Debugging with Logs

Enable detailed logging to diagnose issues:

```bash
# Show all DEBUG messages
FRONTIER_LOG_LEVEL=DEBUG ./frontier-cli/frontier-cli -e "1 + 1"

# Show only database-related messages
FRONTIER_LOG_COMPONENT=DB FRONTIER_LOG_LEVEL=DEBUG ./frontier-cli/frontier-cli --system-root databases/Frontier.root -e "1"

# Trace-level logging (very verbose)
FRONTIER_LOG_LEVEL=TRACE ./frontier-cli/frontier-cli -e "1 + 1"
```

### Exit Codes

The CLI uses standard Unix exit codes:

- **0**: Success
- **1**: Failure (error during execution)

**Example:**
```bash
./frontier-cli/frontier-cli -e "1 + 1"
echo $?  # Prints: 0

./frontier-cli/frontier-cli -e "invalid syntax"
echo $?  # Prints: 1
```

---

## Advanced Usage

### Shell Scripting Integration

Use `frontier-cli` in shell scripts:

```bash
#!/bin/bash

# Execute UserTalk and capture output
result=$(./frontier-cli/frontier-cli -e "42 * 2")
echo "The answer is: $result"

# Check exit code
if ./frontier-cli/frontier-cli -e "1 + 1" > /dev/null; then
    echo "Script succeeded"
else
    echo "Script failed"
    exit 1
fi
```

### Conditional Logging

Control logging based on environment:

```bash
#!/bin/bash

if [ "$DEBUG" = "1" ]; then
    export FRONTIER_LOG_LEVEL=DEBUG
else
    export FRONTIER_LOG_LEVEL=WARN
fi

./frontier-cli/frontier-cli --system-root databases/Frontier.root -e "sizeOf(system)"
```

### Database Testing

Test database integrity:

```bash
#!/bin/bash

# Test that system table exists
if ! ./frontier-cli/frontier-cli --system-root databases/Frontier.root -e "defined(system)" > /dev/null; then
    echo "ERROR: System table not found in database"
    exit 1
fi

# Test that system.verbs exists
if ! ./frontier-cli/frontier-cli --system-root databases/Frontier.root -e "defined(system.verbs)" > /dev/null; then
    echo "ERROR: system.verbs table not found"
    exit 1
fi

echo "Database integrity checks passed"
```

### Automated Migration

Automatically migrate all v6 databases in a directory:

```bash
#!/bin/bash

for db in databases/*-v6.root; do
    echo "Migrating: $db"
    ./frontier-cli/frontier-cli --migrate "$db"
done
```

### JSON Output Processing

Use JSON log format for parsing:

```bash
# Run with JSON logging
FRONTIER_LOG_FORMAT=json ./frontier-cli/frontier-cli -e "1 + 1" 2> logs.json

# Parse with jq
cat logs.json | jq 'select(.level == "ERROR")'
```

### Performance Testing

Measure script execution time:

```bash
time ./frontier-cli/frontier-cli -e "local(i); for i = 1 to 1000 {i * 2}"
```

---

## Related Documentation

- **Migration Guide**: `planning/DATABASE_CORRUPTION_PREVENTION.md` - Database migration and corruption prevention
- **Development Guide**: `CLAUDE.md` - Project structure and development guidelines
- **Logging Standards**: `docs/LOGGING_STANDARDS.md` - Logging infrastructure documentation
- **UserTalk Reference**: (Coming soon) - Complete UserTalk language reference

---

## Version History

### 1.1.0 (2026-01-25)

- Added positional database argument support (`.root` files)
- Flexible argument ordering (database can appear before or after `-e` flag)
- Output messages showing which database was loaded and if migration occurred
- Conflict detection between positional and `--system-root` arguments

### 1.0.0 (2025-12-30)

- Initial release
- Inline script execution (`-e` flag)
- Script file execution
- Database loading (`--system-root`)
- Automatic v6→v7 migration
- Manual database migration (`--migrate`)
- Environment variable configuration
- Verbose and debug logging modes
- Exit code support for shell scripting

---

## REPL QuickScript Model

The REPL follows the **QuickScript model** from legacy Frontier: each evaluation runs
independently in its own thread context, with automatic cleanup after completion.

### Variable Persistence Scopes

Understanding how variables persist is key to effective REPL usage:

**1. Local Variables** (Evaluation-scoped - No Persistence)
```
> x = 5
5
> x + 1
Error: Can't find variable named "x"
```

Local variables are thread-scoped and cleaned up immediately after evaluation completes.
They do NOT persist between Enter presses.

**2. Session-Scoped Variables** (`system.temp.*`)

Persists across evaluations, cleared when frontier-cli exits:
```
> system.temp.counter = 0
0
> system.temp.counter = system.temp.counter + 1
1
> system.temp.counter = system.temp.counter + 1  // Next evaluation
2
```

**3. Disk-Scoped Variables** (`workspace.*` or other root tables)

Saved to database, survives restarts:
```
> workspace.prefs.theme = "dark"
"dark"
// Still available after restarting frontier-cli
```

### Why QuickScript?

This follows proven Frontier patterns:
- **Matches legacy behavior**: QuickScript window worked the same way
- **Clean architecture**: No workarounds or state management needed
- **Let users manage data**: Users choose appropriate persistence scope
- **Thread-safe by design**: Each evaluation is isolated

### Available Commands

```
/exit              Exit the REPL
/help              Show help message with persistence examples
/list [path]       List children of current table (or relative path)
/jump [path]       Navigate to a table (absolute, relative, or ..)
/jump root         Return to system root from anywhere
```

### Tips for REPL Usage

1. **Quick Calculations**: Use locals for throwaway values
   ```
   > 42 * 1.5
   63
   ```

2. **Session State**: Use `system.temp.*` for values needed across evaluations
   ```
   > system.temp.lastResult = someExpression()
   ```

3. **Persistent Configuration**: Use `workspace.*` for settings to save
   ```
   > workspace.config.apiKey = "abc123"
   ```

4. **Inspect Database**: Check what's stored
   ```
   > sizeOf(system.temp)
   > sizeOf(workspace)
   ```

For technical details about the QuickScript architecture, see:
- `planning/architectural_decision_records/ADR-009-repl-hash-table-stack-management.md`

---

## REPL Keybindings (Boxen REPL)

The default REPL (`frontier-cli` with no flag) is the multi-pane boxen REPL.  The following keys are recognized while focus is on the input bar:

| Key | Action |
|-----|--------|
| `Enter` / `Ctrl-M` | Submit the current line for evaluation |
| `Escape` | Clear the input buffer |
| `Ctrl-C` | Quit the REPL (or cancel a pending multi-line continuation) |
| `Left` / `Right` | Move the caret one character within the input |
| `Ctrl-A` / `Ctrl-E` | Jump caret to start / end of line |
| `Backspace` / `Delete` | Delete character before / after the caret |
| `Up` / `Down` | Navigate command history |
| `Tab` | Trigger completion (slash commands or ODB paths) |
| `PgUp` / `PgDn` | Scroll the output pane back / forward through scrollback |
| `/` (at start of line) | Open the slash-menu palette (after a short debounce) |
| `\` (at end of line) | Continue the expression on the next line (multi-line input) |

### Output-pane scrollback (PgUp/PgDn)

The boxen REPL owns the terminal's alternate screen buffer, so the terminal's own scrollback (the scroll wheel / shift-PgUp at the OS terminal level) cannot reach output that has scrolled off the top of the visible region.  Use **PgUp** to walk the output pane view back through up to 1024 lines of scrollback, and **PgDn** to return toward the newest line.

Each key press scrolls by one page (the output pane height minus one line of context overlap), matching `less` / `man` conventions.

Behavior notes:

- **Submitting a new expression** (Enter) automatically snaps the view back to the bottom -- you immediately see the new result.  This is "scroll-on-output" behavior.
- **Async output from background scripts** does NOT snap the view back.  You can review old content while a long-running script keeps printing to the REPL.
- **Modals** (file picker, dialog prompts) do not disturb the scroll position; closing a modal returns you to wherever the view was scrolled to.

If you need true terminal-native scrollback (e.g. unbounded scrollback in your OS terminal application), use `--plain` to launch the legacy linenoise REPL instead.

---

## REPL Navigation and Guest Databases

The REPL supports navigating into both the system root database and any open guest databases (databases opened via `window.open()` or similar verbs). The `/list` and `/jump` commands let you browse the object database hierarchy interactively.

### Basic Navigation

```
> /list
  [1] system                  tableType
  [2] user                    tableType
  [3] workspace               tableType

> /jump system
[system]> /list
  [1] verbs                   tableType
  [2] compiler                tableType
  ...

> /jump ..
>
```

### Navigating into Guest Databases

When a guest database is open, its top-level tables appear alongside system root tables in `/list`. You can `/jump` into any guest database table just like a system table.

Once inside a guest database, the prompt changes to show the database name and your current path using `::` notation:

```
> /list
  [1] system                  tableType
  [2] mainResponder           tableType    ← guest DB table
  ...

> /jump mainResponder
[mainResponder.root]> /list
  [1] responderSuite          tableType
  [2] data                    tableType
  ...

> /jump responderSuite
[mainResponder.root::responderSuite]> /list
  [1] handlers                tableType
  [2] callbacks               tableType
  ...
```

The prompt format is `[dbname::innerpath]>`, where:

- **dbname** is the guest database filename (e.g., `mainResponder.root`)
- **innerpath** is your location within that database (omitted when at the database root)

### Relative Paths Inside Guest Databases

Relative paths work inside guest databases the same way they work in the system root:

```
[radioCommunityServer.root]> /list background
  [1] everyMinute             scriptType
  [2] everyFiveMinutes        scriptType

[radioCommunityServer.root]> /jump background
[radioCommunityServer.root::background]>
```

### Going Up with `..`

Using `..` navigates up one level within the guest database. When you are already at the guest database root, `..` exits the guest database and returns to the system root:

```
[mainResponder.root::responderSuite]> /jump ..
[mainResponder.root]> /jump ..
>
```

### Returning to the System Root

From anywhere -- whether deep inside a guest database or in the system root hierarchy -- you can return to the system root immediately:

```
[radioCommunityServer.root::data::prefs]> /jump
>

[radioCommunityServer.root::data::prefs]> /jump root
>

[radioCommunityServer.root::data::prefs]> /jump @root
>
```

All three forms (`/jump`, `/jump root`, `/jump @root`) return to the system root.

---

## Getting Help

- **Command-line help**: `./frontier-cli/frontier-cli --help`
- **Version info**: `./frontier-cli/frontier-cli --version`
- **GitHub Issues**: https://github.com/jsavin/Frontier/issues
- **Project Documentation**: `/docs` directory

---

**Happy scripting with Frontier!** 🚀
