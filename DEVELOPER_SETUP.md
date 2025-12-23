# Frontier Development Environment Setup

This document describes the tools needed for Frontier development, particularly for code quality, static analysis, and understanding the codebase structure.

## Quick Setup

### Option 1: Automated Installation (Recommended)

```bash
# Run the setup script from the repo root
./scripts/install_dev_tools.sh

# Or download and run directly
curl -fsSL https://raw.githubusercontent.com/jsavin/Frontier/develop/scripts/install_dev_tools.sh | bash
```

### Option 2: Manual Installation

Install each tool individually with Homebrew:

```bash
# Core analysis tools
brew install cflow                  # Call graph analysis
brew install universal-ctags        # Symbol indexing and cross-reference
brew install clang-tools            # LLVM static analysis tools
brew install ripgrep                # Fast pattern search (rg)
brew install tree                   # Directory structure visualization
brew install bear                   # Compilation database generator
```

### Verify Installation

```bash
cflow --version
ctags --version
clang-check --version
rg --version
tree --version
bear --version
```

---

## Tools Reference

### 1. `cflow` - Call Graph Analysis

**Purpose**: Identify dead code by analyzing which functions call which other functions.

**Installation**: `brew install cflow`

**Common Usage**:

```bash
# Show all functions and their callers (simple tree view)
cflow Common/source/*.c | head -50

# Find leaf functions (never called by anything in the project)
cflow Common/source/*.c | grep "^[a-z_]*$"

# Analyze specific file
cflow Common/source/db_format.c | grep "db_format"

# Generate graphviz output for visualization
cflow --format=posix --output-file=callgraph.txt Common/source/*.c
```

**Output interpretation**:
- Functions with no indent are never called
- Indented functions are called by the function above them
- Leaf nodes at the end of branches are candidates for dead code

**Limitations**:
- Can't distinguish between public API and internal functions
- Doesn't understand preprocessor-based conditional compilation
- Function pointers and callbacks may not be detected

---

### 2. `universal-ctags` - Symbol Indexing

**Purpose**: Build symbol database for fast searching and cross-reference analysis.

**Installation**: `brew install universal-ctags`

**Common Usage**:

```bash
# Generate tags file
ctags -R Common/

# Find all references to a function
grep "^db_format_mode_apply" tags

# List all functions in a file
ctags -f - Common/source/db_format.c | grep "^"

# Integration with editors (vim/neovim)
ctags -R --fields=+l Common/
# Then in vim: Ctrl-] to jump to definition, Ctrl-T to go back
```

**Integration with scripts**:
```bash
# Find all callers of a function
ctags -R --fields=+K Common/ && \
grep "^.*\<function_name\>" tags | grep -v "^function_name"
```

---

### 3. `clang-tools` - Static Analysis

**Purpose**: Compiler-based static analysis to find potential bugs and dead code.

**Installation**: `brew install clang-tools`

**Tools included**:
- `clang-check` - AST-based semantic checking
- `clang-format` - Code formatting
- `clang-modernize` - Modernization suggestions
- `clang-tidy` - Linting and static analysis

**Common Usage**:

```bash
# Check for unused functions (requires compilation database)
bear make -C tests  # Generate compile_commands.json
clang-tidy -checks=readability-avoid-const-params-in-decls Common/source/db_format.c

# Format code
clang-format -i Common/source/db_format.c

# Check with specific analysis
clang-check -ast-print Common/source/db_format.c
```

**Compilation Database**:
```bash
# Generate compile_commands.json for clang-tidy
bear make -C tests

# Now clang-tidy has full type information
clang-tidy -checks='*' Common/source/db.c -- -I Common/headers -I portable
```

---

### 4. `ripgrep` (rg) - Fast Pattern Search

**Purpose**: Ultra-fast regex-based searching across codebase.

**Installation**: `brew install ripgrep`

**Common Usage**:

```bash
# Find all fprintf calls
rg "fprintf\(stderr" --type c

# Find logging-related patterns
rg "\[headless\]|\[diag\]|\[debug\]" --type c

# Find specific function calls with context
rg "db_format_mode_apply" -B 2 -A 2 --type c

# Find GUI-only code
rg "window|menu|dialog|GUI" --type c

# Find TODO/FIXME/XXX comments
rg "TODO|FIXME|XXX|HACK" --type c

# Count occurrences
rg "fprintf" --type c | wc -l
```

**Advantages over grep**:
- Respects `.gitignore` by default
- Much faster (uses SIMD)
- Better regex syntax
- Automatic file type detection

---

### 5. `tree` - Directory Visualization

**Purpose**: Visualize project structure to understand organization.

**Installation**: `brew install tree`

**Common Usage**:

```bash
# Show overall structure (limited depth)
tree -L 2 -d

# Show structure of specific directory
tree -L 3 Common/source/

# Show with file sizes
tree -h Common/

# Show only .c files
tree --include='*.c' Common/source/ | head -50

# Generate HTML output for documentation
tree -h --du -o structure.html
```

---

### 6. `bear` - Compilation Database Generator

**Purpose**: Generate `compile_commands.json` for advanced static analysis tools.

**Installation**: `brew install bear`

**Common Usage**:

```bash
# Generate compilation database from build
cd /path/to/Frontier
bear make -C tests

# Now you have compile_commands.json
# Use it with clang-tidy, clang-check, etc.
clang-tidy -p . Common/source/db.c

# Generate for specific target
bear make -C tests save_migration_tests
```

**Why this matters**:
- Many static analysis tools need compilation database
- Tells tools about compiler flags, include paths, macro definitions
- Enables cross-file type checking and analysis

---

## Workflow Examples

### Finding Dead Code

**Complete workflow to identify unused functions**:

```bash
# 1. Generate call graph
cflow Common/source/*.c Common/headers/*.h > callgraph.txt

# 2. Find leaf functions (never called)
grep "^[a-z_]*$" callgraph.txt > potential_dead_code.txt

# 3. For each candidate, verify with ctags
ctags -R Common/
for func in $(cat potential_dead_code.txt); do
  count=$(grep -c "\\b$func\\b" tags || true)
  if [ $count -le 1 ]; then  # Only definition, no calls
    echo "$func might be dead"
  fi
done

# 4. Manual review for each candidate
# - Check if it's a public API that external code might use
# - Check for function pointers (cflow might miss these)
# - Check for preprocessor-conditional calls
```

### Analyzing Logging Patterns

**Find all logging statements to understand current patterns**:

```bash
# Count logging by pattern
echo "=== fprintf(stderr patterns ==="
rg "fprintf\(stderr" --type c | wc -l

echo "=== [headless] debug patterns ==="
rg "\[headless\]" --type c | wc -l

echo "=== By file ==="
rg "fprintf\(stderr" --type c -l | sort | uniq -c | sort -rn

# Sample some logging to understand patterns
echo "=== Sample logging statements ==="
rg "fprintf\(stderr" --type c -A 0 | head -20

# Find conditional logging (current pattern)
rg "#if defined\(FRONTIER_HEADLESS\)" --type c -A 3
```

### Understanding Module Dependencies

**Map how modules depend on each other**:

```bash
# Generate call graph for specific module
cflow Common/source/db_format.c | head -100

# Find what calls db_format_mode_apply
rg "db_format_mode_apply" --type c | grep -v "^.*:.*\/\*"

# Trace backwards to understand call chain
# For each caller, find what calls that function
# (Can be automated but manual review is often clearer)
```

### GUI Code Isolation

**Find GUI-related code mixed with data logic**:

```bash
# Find all window/menu/dialog related code
rg "window|menu|dialog|GUI|display|shell" --type c -l | sort | uniq

# For each file, see how many GUI vs. data references
rg "window" --type c Common/source/shell.c | wc -l
rg "window" --type c Common/source/db.c | wc -l

# Find functions that reference both GUI and database operations
rg "db.*window|window.*db" --type c
```

---

## Integration with CI/CD

These tools can be integrated into continuous integration:

```bash
# In your CI pipeline:

# 1. Build and generate compilation database
bear make -C tests

# 2. Run static analysis
clang-tidy -checks='readability-*,performance-*' Common/source/*.c

# 3. Check for compiler warnings
make CFLAGS="-Wall -Wextra -Wunused-function" 2>&1 | grep warning

# 4. Generate code complexity metrics (optional)
cflow Common/source/*.c | wc -l
```

---

## Troubleshooting

**Q: cflow shows too much output, how do I filter?**

```bash
# Show only functions starting with specific prefix
cflow Common/source/*.c | grep "^[[:space:]]*db_"

# Show only at depth 2 or less
cflow Common/source/*.c | sed -n '/^[^ ]/,/^$/p'
```

**Q: ctags not finding symbols?**

```bash
# Regenerate tags with more options
ctags -R --fields=+l --extra=+f Common/

# Verify it worked
grep "^db_format_mode_apply" tags
```

**Q: clang-tidy errors about missing includes?**

```bash
# Make sure you've generated compilation database
bear make -C tests

# Check that compile_commands.json exists
ls -la compile_commands.json

# Try again with verbose output
clang-tidy -checks='*' -p . Common/source/db.c 2>&1 | head -50
```

---

## Next Steps

Once tools are installed, see the documentation in `planning/` and `docs/` directories for:
- Dead code removal strategy: `planning/phase3/code-cleanup/` directory (includes DEAD_CODE_REMOVAL_STRATEGY.md)
- Code analysis reports: `reports/` directory (includes static-analysis/ and progress/ subdirectories)
- Phase 3 planning: `planning/phase3/` for active work on headless runtime and code cleanup

---

## References

- cflow manual: `man cflow` or `cflow --help`
- ctags documentation: https://docs.ctags.io/
- clang-tools: https://clang.llvm.org/
- ripgrep: https://github.com/BurntSushi/ripgrep
- bear: https://github.com/rizsotto/Bear
