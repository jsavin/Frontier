# Getting Started with Frontier

This guide walks you through setting up Frontier from scratch, building the CLI, and running your first UserTalk code.

---

## Table of Contents

1. [System Requirements](#system-requirements)
2. [Installing Prerequisites](#installing-prerequisites)
3. [Getting the Code](#getting-the-code)
4. [Building the CLI](#building-the-cli)
5. [Your First UserTalk Code](#your-first-usertalk-code)
6. [Using the Interactive REPL](#using-the-interactive-repl)
7. [Running Tests](#running-tests)
8. [Working with Databases](#working-with-databases)
9. [Next Steps](#next-steps)

---

## System Requirements

| Requirement | Minimum | Recommended |
|-------------|---------|-------------|
| Operating System | macOS 11.0 (Big Sur) | macOS 12.0+ (Monterey or later) |
| Architecture | Intel x86_64 or Apple Silicon (arm64) | Either (universal binary) |
| Disk Space | 500 MB | 1 GB+ |
| RAM | 4 GB | 8 GB+ |

**Note**: Linux support is in progress. Windows support is planned but not yet available.

---

## Installing Prerequisites

### Xcode Command Line Tools (Required)

The CLI build requires a C compiler (clang) and standard build tools.

```bash
# Install Xcode Command Line Tools
xcode-select --install
```

A dialog will appear. Click "Install" and wait for completion (~5-10 minutes on first install).

**Verify installation:**

```bash
clang --version
# Should show Apple clang version 14.0 or later
```

### Optional: Development Tools

For code analysis and contribution work:

```bash
# Install Homebrew if you don't have it
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Install development tools (optional but recommended for contributors)
brew install ripgrep tree universal-ctags
```

### Optional: Python for Integration Tests

Integration tests require Python 3 and PyYAML:

```bash
# Python 3 is pre-installed on macOS, but verify:
python3 --version
# Should show Python 3.9 or later

# Install PyYAML for integration tests
pip3 install pyyaml
```

---

## Getting the Code

### Clone the Repository

```bash
# Clone from GitHub
git clone https://github.com/jsavin/Frontier.git
cd Frontier

# Verify you're on the develop branch
git branch --show-current
# Output: develop
```

### Repository Structure

```
Frontier/
├── frontier-cli/         # CLI source and Makefile (build this first!)
├── Common/               # Core Frontier runtime source code
├── portable/             # Portable/headless runtime layer
├── databases/            # Frontier.root database files
├── tests/                # Test suites (C unit tests, integration tests)
├── tools/                # Build and analysis tools
├── docs/                 # Documentation (you are here)
└── planning/             # Architecture decisions, roadmaps
```

---

## Building the CLI

### Quick Build

```bash
# From the Frontier root directory
make -C frontier-cli
```

This builds a **universal binary** that runs natively on both Intel and Apple Silicon Macs.

**Expected output:**

```
clang ... -o frontier-cli
```

Build typically completes in 30-60 seconds.

### Verify the Build

```bash
# Check that the executable exists
ls -la frontier-cli/frontier-cli

# Check the version
./frontier-cli/frontier-cli --version
# Output: frontier-cli version 1.0.0-... (or similar)

# Run a simple test
./frontier-cli/frontier-cli -e "1 + 1"
# Output: 2
```

If you see `2`, the build is successful.

### Build Options

```bash
# Build with Address Sanitizer (for debugging memory issues)
SANITIZE=1 make -C frontier-cli

# Clean and rebuild
make -C frontier-cli clean
make -C frontier-cli

# Build specific architecture only (not usually needed)
ARCHES=arm64 make -C frontier-cli
```

---

## Your First UserTalk Code

### Inline Execution

The `-e` flag executes UserTalk code directly:

```bash
# Basic arithmetic
./frontier-cli/frontier-cli -e "2 + 3 * 4"
# Output: 14

# String operations
./frontier-cli/frontier-cli -e 'string.upper("hello world")'
# Output: HELLO WORLD

# Working with variables
./frontier-cli/frontier-cli -e "local(x = 5); local(y = 10); return x + y"
# Output: 15
```

### Multi-line Scripts

Use bash `$'...'` syntax for newlines:

```bash
./frontier-cli/frontier-cli -e $'local(sum = 0);
for i = 1 to 10 {
    sum = sum + i
};
return sum'
# Output: 55
```

### Script Files

Create a file `hello.usertalk`:

```usertalk
// hello.usertalk
local(name = "World");
msg("Hello, " + name + "!");
return "Done"
```

Run it:

```bash
./frontier-cli/frontier-cli hello.usertalk
# Output: Hello, World!
# Done
```

---

## Using the Interactive REPL

Launch the Read-Eval-Print Loop for interactive exploration:

```bash
./frontier-cli/frontier-cli
```

### REPL Commands

| Command | Description |
|---------|-------------|
| `/help` | Show help and usage examples |
| `/exit` | Exit the REPL |
| `expression` | Evaluate any UserTalk expression |

### Example Session

```
Frontier REPL (type /help for commands, /exit to quit)
> 1 + 1
2
> string.length("Frontier")
8
> typeof(42)
longType
> /exit
```

### Variable Persistence

The REPL follows the **QuickScript model** - local variables don't persist between evaluations:

```
> x = 5
5
> x + 1
Error: Can't find variable named "x"
```

Use `system.temp.*` for session-scoped persistence:

```
> system.temp.counter = 0
0
> system.temp.counter = system.temp.counter + 1
1
> system.temp.counter = system.temp.counter + 1
2
```

For full REPL documentation, see [CLI_USAGE_GUIDE.md](CLI_USAGE_GUIDE.md#repl-quickscript-model).

---

## Running Tests

### Unit Tests (C Test Suite)

```bash
# Run all unit tests
./tools/run_headless_tests.sh
```

### Integration Tests (UserTalk Tests)

Integration tests verify UserTalk verb behavior using YAML test definitions:

```bash
# Install Python dependency (one time)
pip3 install pyyaml

# Run integration tests
cd tests && make test-integration

# Verbose output
cd tests && make test-integration-verbose
```

### All Tests

```bash
cd tests && make test-all
```

### Expected Results

- **Unit tests**: Should show pass/fail summary
- **Integration tests**: Should show 99%+ pass rate (1,100+ tests)

For comprehensive testing documentation, see [TESTING_GUIDE.md](TESTING_GUIDE.md).

---

## Working with Databases

Frontier uses Object Database (ODB) files with `.root` (v6) or `.root7` (v7) extensions.

### Loading a Database

```bash
# Load system database and run code
./frontier-cli/frontier-cli --system-root databases/Frontier.root7 -e "sizeOf(system)"

# Shorthand: pass database as positional argument
./frontier-cli/frontier-cli databases/Frontier.root7 -e "sizeOf(system)"
```

### Database Migration

The CLI automatically migrates v6 databases to v7 format:

```bash
# This creates Frontier.root7 if it doesn't exist
./frontier-cli/frontier-cli --system-root databases/Frontier.root -e "1"
```

### Manual Migration

```bash
./frontier-cli/frontier-cli --system-root databases/Frontier.root --upgrade-system-root
# Output: System root upgraded to v7 format (written to): databases/Frontier.root7
```

---

## Next Steps

### Documentation

| Guide | Description |
|-------|-------------|
| [CLI Usage Guide](CLI_USAGE_GUIDE.md) | Complete CLI reference (900+ lines) |
| [Testing Guide](TESTING_GUIDE.md) | Writing and running tests |
| [Contributing](../CONTRIBUTING.md) | Branching, commits, PR workflow |

### For Contributors

1. **Read the architecture**: `planning/INDEX.md` for roadmap and ADRs
2. **Set up dev tools**: `./scripts/install_dev_tools.sh`
3. **Use worktrees**: Feature development uses git worktrees (see CONTRIBUTING.md)
4. **Run tests before PRs**: Both unit and integration tests must pass

### Getting Help

- **GitHub Issues**: https://github.com/jsavin/Frontier/issues
- **Project Status**: See [STATUS.md](../STATUS.md) for current progress
- **Planning Docs**: `planning/` directory for architecture decisions

---

## Troubleshooting

### "xcode-select: error: command line tools are already installed"

This is fine - you already have the tools installed. Proceed with the build.

### "make: clang: No such file or directory"

Xcode Command Line Tools aren't installed. Run `xcode-select --install`.

### Build fails with architecture errors

Try forcing a single architecture:

```bash
ARCHES=$(uname -m) make -C frontier-cli clean
ARCHES=$(uname -m) make -C frontier-cli
```

### "Can't call [verb] because it isn't implemented"

Not all UserTalk verbs are implemented yet. Check implementation status:

```bash
cd tools/kernelverbs_parser && python3 cli.py report
```

### Integration tests fail with "ModuleNotFoundError: No module named 'yaml'"

Install PyYAML:

```bash
pip3 install pyyaml
```

---

**Welcome to Frontier!** If you run into issues not covered here, please open an issue on GitHub.
