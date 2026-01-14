# Frontier CLI Usage Guide

**Version:** 1.0.0
**Last Updated:** 2025-12-30

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

---

## Overview

`frontier-cli` is the command-line interface for executing UserTalk scripts in headless mode. It provides a modern, scriptable way to run Frontier code without requiring the full GUI application.

### Key Features

- **Inline Script Execution**: Run UserTalk code directly from the command line
- **Script File Execution**: Execute `.usertalk` script files
- **Database Support**: Load and interact with Frontier database files (`.root`)
- **Database Migration**: Automatically upgrade v6 databases to v7 format
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
| | `--upgrade-system-root` | | Upgrade system root to v7 format (use with `--system-root`) |
| `-b` | `--batch` | | Batch mode (disable interactive prompts) |
| | `--non-interactive` | | Alias for `--batch` |
| `-v` | `--verbose` | | Enable verbose output |
| | `--debug` | | Enable debug mode |
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
./frontier-cli/frontier-cli --system-root databases/Frontier-v6.root7 -e "sizeOf(system)"
```

**Automatic Migration:**

If you specify a v6 database, the CLI will automatically migrate it to v7 format and use the migrated version:

```bash
# This will create Frontier-v6.root7 if it doesn't exist
./frontier-cli/frontier-cli --system-root databases/Frontier-v6.root -e "1"
```

#### `--upgrade-system-root`

Upgrade a database to v7 format without loading or executing any scripts. Must be used with `--system-root`.

**Usage:**
```bash
./frontier-cli/frontier-cli --system-root databases/Frontier-v6.root --upgrade-system-root
```

**Output:**
```
System root upgraded to v7 format (written to): databases/Frontier-v6.root7
```

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

**Example:**
```bash
./frontier-cli/frontier-cli --system-root databases/Frontier-v6.root7 -e "sizeOf(system.verbs)"
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
./frontier-cli/frontier-cli --system-root databases/Frontier-v6.root7 -e "1"
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

Enable execution of `system.startup` scripts when loading a database (default: skipped). Set to 1 to run startup scripts. The default behavior skips startup scripts for faster CLI execution and testing.

**Example (run startup scripts):**
```bash
export FRONTIER_HEADLESS_RUN_STARTUP=1
./frontier-cli/frontier-cli --system-root databases/Frontier-v6.root7 -e "1"
```

**Default behavior (startup scripts skipped):**
```bash
# No env var needed - startup scripts are skipped by default
./frontier-cli/frontier-cli --system-root databases/Frontier-v6.root7 -e "1"
```

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
./frontier-cli/frontier-cli --system-root databases/Frontier-v6.root7 -e "defined(system)"
```

### Automatic Migration

When you specify a v6 database, the CLI automatically:

1. Detects the database format
2. Creates a migrated v7 copy (e.g., `Frontier-v6.root` → `Frontier-v6.root7`)
3. Loads the v7 database
4. Leaves the v6 database untouched

**Example:**
```bash
# First run: migrates Frontier-v6.root → Frontier-v6.root7
./frontier-cli/frontier-cli --system-root databases/Frontier-v6.root -e "1"

# Subsequent runs: uses existing Frontier-v6.root7
./frontier-cli/frontier-cli --system-root databases/Frontier-v6.root -e "1"
```

### Manual Migration

To migrate a database without executing scripts:

```bash
./frontier-cli/frontier-cli --system-root databases/Frontier-v6.root --upgrade-system-root
```

**Output:**
```
System root upgraded to v7 format (written to): databases/Frontier-v6.root7
```

### Accessing Database Contents

Once a database is loaded, you can access its tables and scripts:

```bash
# Check if system table exists
./frontier-cli/frontier-cli --system-root databases/Frontier-v6.root7 -e "defined(system)"

# Get size of system.verbs table
./frontier-cli/frontier-cli --system-root databases/Frontier-v6.root7 -e "sizeOf(system.verbs)"

# List top-level tables
./frontier-cli/frontier-cli --system-root databases/Frontier-v6.root7 -e "getTableNames()"
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

### Database Queries

```bash
# Check system table size
./frontier-cli/frontier-cli --system-root databases/Frontier-v6.root7 -e "sizeOf(system)"

# List system.verbs subtables
./frontier-cli/frontier-cli --system-root databases/Frontier-v6.root7 -e "sizeOf(system.verbs)"
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
./frontier-cli/frontier-cli --system-root databases/Frontier-v6.root --upgrade-system-root

# Use the migrated database
./frontier-cli/frontier-cli --system-root databases/Frontier-v6.root7 -e "defined(system)"
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
FRONTIER_LOG_COMPONENT=DB FRONTIER_LOG_LEVEL=DEBUG ./frontier-cli/frontier-cli --system-root databases/Frontier-v6.root7 -e "1"

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

./frontier-cli/frontier-cli --system-root databases/Frontier-v6.root7 -e "sizeOf(system)"
```

### Database Testing

Test database integrity:

```bash
#!/bin/bash

# Test that system table exists
if ! ./frontier-cli/frontier-cli --system-root databases/Frontier-v6.root7 -e "defined(system)" > /dev/null; then
    echo "ERROR: System table not found in database"
    exit 1
fi

# Test that system.verbs exists
if ! ./frontier-cli/frontier-cli --system-root databases/Frontier-v6.root7 -e "defined(system.verbs)" > /dev/null; then
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
    ./frontier-cli/frontier-cli --system-root "$db" --upgrade-system-root
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

### 1.0.0 (2025-12-30)

- Initial release
- Inline script execution (`-e` flag)
- Script file execution
- Database loading (`--system-root`)
- Automatic v6→v7 migration
- Manual database upgrade (`--upgrade-system-root`)
- Environment variable configuration
- Verbose and debug logging modes
- Exit code support for shell scripting

---

## Getting Help

- **Command-line help**: `./frontier-cli/frontier-cli --help`
- **Version info**: `./frontier-cli/frontier-cli --version`
- **GitHub Issues**: https://github.com/jsavin/Frontier/issues
- **Project Documentation**: `/docs` directory

---

**Happy scripting with Frontier!** 🚀
