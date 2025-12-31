# Frontier Testing Guide

Complete guide for testing the headless Frontier runtime, including CLI usage, database migration, and testing patterns.

---

## Table of Contents

1. [Running frontier-cli](#running-frontier-cli)
2. [Database Migration (v6→v7)](#database-migration-v6v7)
3. [Testing Patterns](#testing-patterns)
4. [UserTalk Syntax Guide](#usertalk-syntax-guide)
5. [System Dependencies](#system-dependencies)

---

## Running frontier-cli

The frontier-cli executable must be run from the project root directory (NOT from within frontier-cli/ or tests/).

### Basic Syntax

```bash
# Execute inline UserTalk code (no database):
./frontier-cli/frontier-cli -e "1+1"

# Execute with system root database loaded:
./frontier-cli/frontier-cli --system-root databases/Frontier-v7.root -e "sizeOf(system)"

# Skip startup scripts (use when testing bootstrapping):
FRONTIER_HEADLESS_SKIP_STARTUP=1 ./frontier-cli/frontier-cli -e "1+1"
```

### Multi-line UserTalk Scripts

Multi-line scripts work using bash `$'...'` syntax for proper newline handling:

```bash
# Multi-line script with $'...\n...' syntax:
FRONTIER_HEADLESS_SKIP_STARTUP=1 ./frontier-cli/frontier-cli -e $'lang.new(tableType, @t);\nt.key1 = "hello";\nt.key2 = 42;\nreturn "size:" + sizeOf(t) + " key1:" + t.key1'

# Output: size:2 key1:hello

# Single-line works too (statements separated by semicolons):
FRONTIER_HEADLESS_SKIP_STARTUP=1 ./frontier-cli/frontier-cli -e "lang.new(tableType, @t); t.key1 = \"hello\"; return t.key1"
```

### Testing lang.new() Verb

The `lang.new()` verb creates new UserTalk objects (tables, outlines, scripts, etc.) in memory:

```bash
# Create a table and verify it exists:
FRONTIER_HEADLESS_SKIP_STARTUP=1 ./frontier-cli/frontier-cli -e "lang.new(tableType, @t); return defined(t)"
# Output: true

# Create a table, add data, and read it back:
FRONTIER_HEADLESS_SKIP_STARTUP=1 ./frontier-cli/frontier-cli -e "lang.new(tableType, @t); t.key1 = \"hello\"; t.key2 = 42; t.key3 = true; return \"size:\" + sizeOf(t) + \" key1:\" + t.key1 + \" key2:\" + t.key2"
# Output: size:3 key1:hello key2:42

# Test with different object types:
FRONTIER_HEADLESS_SKIP_STARTUP=1 ./frontier-cli/frontier-cli -e "lang.new(tableType, @myTable); return typeof(myTable)"
# Output: tableType
```

**Known issues:**
- Empty error message `[lang-ERROR] langcallbacks.c:208:` may appear after successful execution (harmless, can be ignored)
- Multi-line scripts passed as plain strings (without `$'...'`) will fail due to shell parsing

---

## Database Migration (v6→v7)

**IMPORTANT: Always use clean migration before testing!**

Old migrated databases may be corrupted artifacts from earlier broken migrations. Always delete existing v7 databases and run a fresh migration before running tests.

### Quick Migration Command

```bash
# Clean migration workflow (ALWAYS do this before testing):
rm -f databases/Frontier-v7.root

# Run CLI with v6 database - creates v7 output file automatically
FRONTIER_HEADLESS_SKIP_STARTUP=1 ./frontier-cli/frontier-cli \
  --system-root databases/Frontier-v6.root -e "1"

# Output: databases/Frontier-v7.root (new file created by migration)
```

### What Happens During Migration

1. CLI opens `databases/Frontier-v6.root` and detects v6 format
2. Migration creates NEW output file: `databases/Frontier-v7.root`
3. Original `databases/Frontier-v6.root` is **never modified** (preserved)
4. Pattern: Version suffix is stripped, then `-v7` added: `Frontier-v6.root` → `Frontier-v7.root`

### Verification

```bash
# Check database version (first 2 bytes should be 0007 for v7)
xxd -l 2 databases/Frontier-v7.root
# Expected output: 00000000: 0007  ..
```

### Alternative: Using save_migration_tests

For testing the migration process itself:

```bash
# Clean rebuild and run migration test:
make -C tests clean && make -C tests save_migration_tests
./tests/save_migration_tests

# Output: test_save_migration-v7.root (v7 migrated database)
```

### Testing Migrated Database

```bash
# Test database loads and system table is accessible:
FRONTIER_HEADLESS_SKIP_STARTUP=1 ./frontier-cli/frontier-cli \
  --system-root test_save_migration-v7.root -e "defined(system)"

# Test external table variables (critical - tests Issue #123 fix):
FRONTIER_HEADLESS_SKIP_STARTUP=1 ./frontier-cli/frontier-cli \
  --system-root test_save_migration-v7.root -e "sizeOf(system.verbs.globals)"

# Test workspace access:
FRONTIER_HEADLESS_SKIP_STARTUP=1 ./frontier-cli/frontier-cli \
  --system-root test_save_migration-v7.root -e "defined(workspace)"
```

### Full Integration Test Suite

```bash
# Runs migration + all headless tests:
./tools/run_headless_tests.sh
```

See `planning/phase3/MIGRATION_VALIDATION_REPORT.md` for detailed test procedures and known issues.

---

## Testing Patterns

### Run Headless Test Suite

```bash
# Full test suite (rebuilds CLI, migrates DB, runs all tests):
./tools/run_headless_tests.sh
```

### Verb Coverage Analysis

```bash
# Show current verb detection (implemented vs stubbed):
cd tools/kernelverbs_parser && python3 cli.py analyze

# Generate detailed coverage reports:
python3 cli.py report

# Output to stdout:
python3 cli.py report -o -
```

### Common Test Commands

```bash
# Test basic arithmetic:
./frontier-cli/frontier-cli -e "1 + 1"

# Test string operations:
./frontier-cli/frontier-cli -e "string.upper(\"test\")"

# Test with database loaded:
./frontier-cli/frontier-cli --system-root databases/Frontier-v7.root -e "sizeOf(system)"

# Test table operations:
./frontier-cli/frontier-cli -e "lang.new(tableType, @t); t.a = 1; return sizeOf(t)"
```

---

## UserTalk Syntax Guide

**IMPORTANT: UserTalk has unique syntax rules that differ from JavaScript, Python, and most modern languages.**

### String Literals (DIFFERENT FROM JS/Python/etc)

**Double quotes ("...") are for strings**:
- `"hello"` → string
- `sizeOf("hello")` → 5 ✓

**Single quotes ('...') are for character constants** (NOT strings!):
- `'A'` (1 char) → Character constant ✓
- `'TEXT'` (4 chars) → OSType (Mac file type) ✓
- `'hello'` (5 chars) → SYNTAX ERROR

**This is opposite of many modern languages where 'x' and "x" are equivalent!**

### Common Mistakes for AI Assistants

WRONG (JavaScript/Python style):
```javascript
sizeOf('hello')  // FAILS - single quotes not valid for strings in UserTalk
```

CORRECT (UserTalk style):
```usertalk
sizeOf("hello")  // Works - double quotes for strings
```

### Testing UserTalk Code

When testing built-in functions or verbs, ALWAYS use double quotes for string literals:
- ✓ `string.upper("test")`
- ✗ `string.upper('test')`  // Will fail!

### Error Messages

When single quotes are used incorrectly for strings, UserTalk produces:
```
"Character constant isnt correctly specified. Must be of the form 'c'."
```

This error indicates you tried to use single quotes for a multi-character string, which is invalid syntax.

### Quick Reference

| Syntax | UserTalk | JavaScript/Python |
|--------|----------|-------------------|
| String | `"hello"` | `"hello"` or `'hello'` |
| Character constant | `'A'` | N/A (just use `"A"`) |
| OSType (4-char) | `'TEXT'` | N/A (Mac-specific) |
| Multi-char with single quotes | ERROR | Works (string) |

**See also:** `docs/USERTALK_SYNTAX_REFERENCE.md` for comprehensive syntax guide.

---

## System Dependencies

### xxd (hex dump utility)

**Required for:** Database version verification and corruption detection

The `xxd` command is used by `tools/run_headless_tests.sh` to verify database file formats and detect corruption. The test runner automatically checks for `xxd` and will fail with a clear error message if it's not installed.

**Installation:**
- **macOS:** `brew install vim` (xxd is included with vim)
- **Linux (Debian/Ubuntu):** `apt-get install vim-common`
- **Linux (Red Hat/CentOS):** `yum install vim-common`

**Usage in Frontier:**
```bash
# Check database version (first 2 bytes)
xxd -l 2 -p databases/Frontier-v6.root
# Expected for v6: 0006
# Expected for v7: 0007
```

**See also:** `planning/DATABASE_CORRUPTION_PREVENTION.md` for database protection details

---

## Related Documentation

- `docs/VERB_IMPLEMENTATION_GUIDE.md` - Implementing kernel verbs in C
- `docs/CLI_USAGE_GUIDE.md` - Complete CLI reference (600+ lines)
- `docs/LOGGING_STANDARDS.md` - Logging infrastructure
- `CLAUDE.md` - Quick reference and development guidelines
