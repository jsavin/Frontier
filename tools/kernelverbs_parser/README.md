# Kernelverbs Parser & Verb Binding Analyzer

Automatically analyzes Frontier's kernel verb implementations and generates comprehensive coverage reports. Includes tools for parsing the RC file, analyzing verb implementations, and generating stub files.

## Overview

Frontier's kernel verbs are defined in `Common/resources/Win32/kernelverbs.rc` using the EFP (External Function Processor) resource format. This toolset:

1. **`parse_kernelverbs.py`** - Parses the RC file to extract processor and verb definitions
2. **`analyzer.py`** - Analyzes C source code to determine which verbs are implemented vs. stubbed
3. **`cli.py`** - Command-line interface for analysis and reporting
4. **`verb_exceptions.py`** - Exception tables for inconsistent verb naming patterns
5. **Unit Tests** - Comprehensive tests with 31 test cases

## Tools at a Glance

| Tool | Purpose | Input | Output |
|------|---------|-------|--------|
| `parse_kernelverbs.py` | Discover all processors and verbs | `kernelverbs.rc` | Processor metadata (51 processors, 707 verbs) |
| `analyzer.py` | Analyze C source for implementations | C source files + RC data | VerbImplementation records with status |
| `cli.py` | Command-line interface for analysis | C source files | Reports, JSON, verification |
| `verb_exceptions.py` | Exception tables for naming patterns | N/A | Pattern C/D mappings |
| `test_analyzer.py` | Unit tests for analyzer | N/A | Test results (31/31 passing) |

## Quick Start

### Analyze Verb Implementations

```bash
cd tools/kernelverbs_parser

# Analyze all processors (suppress verbose output)
python3 cli.py analyze

# Generate coverage report (auto-dated filename)
python3 cli.py report

# Generate report to specific file
python3 cli.py report -o my_report.md

# Check whitelist consistency
python3 cli.py verify

# Show changes without applying
python3 cli.py dry-run

# Export metadata to JSON
python3 cli.py analyze --json verb_metadata.json
```

### Command Line Help

View all available commands and options:

```bash
python3 cli.py --help
```

View help for a specific subcommand:

```bash
python3 cli.py analyze --help
python3 cli.py report --help
python3 cli.py verify --help
python3 cli.py dry-run --help
```

### Common Usage Examples

**Example 1: Analyze and see results**
```bash
python3 cli.py analyze
# Output: Shows all 51 processors with detected implementations
```

**Example 2: Generate a dated coverage report**
```bash
python3 cli.py report
# Output: Report written to: reports/coverage/verb-binding/2025-12-14-01.md
```

**Example 3: Save report to custom file**
```bash
python3 cli.py report -o my_analysis.md
# Output: Report written to: my_analysis.md
```

**Example 4: Print report to console**
```bash
python3 cli.py report -o -
# Output: Prints markdown table directly to stdout
```

**Example 5: Verify analyzer consistency**
```bash
python3 cli.py verify
# Exit 0: Current state is consistent
# Exit 1: Whitelist is out of sync with analyzer output
```

**Example 6: See proposed changes without applying**
```bash
python3 cli.py dry-run
# Shows processors to add/remove from whitelist
```

### Run Unit Tests

```bash
cd tools/kernelverbs_parser
python3 -m unittest test_analyzer -v
```

Expected output: **31 tests pass** in ~0.25 seconds

## What The Analyzer Does

The analyzer automatically determines which kernel verbs are implemented vs. stubbed by:

1. **Parsing `kernelverbs.rc`** - Discovers 51 processors with 707 total verbs
2. **Extracting verb names** - Gets canonical names from RC file
3. **Finding implementation files** - Locates C source files using pattern matching
4. **Extracting enum tokens** - Parses C enum definitions and case statements
5. **Matching verbs to implementations** - Maps RC verbs to C implementations using:
   - **Pattern A**: Standard `{processor}{verb}func` naming (e.g., `filecreatedfunc`)
   - **Pattern B**: Simple `{verb}func` naming (e.g., `movefunc`)
   - **Pattern C**: Exception tables for inconsistent naming (op, pict, frontier, sys)
   - **Pattern D**: Multi-processor consolidation in `langverbs.c` (10 processors)
6. **Generating reports** - Creates detailed coverage reports with verb-by-verb status

## Coverage Results

**As of 2025-12-14:**
- **27/51 processors detected** (53%)
- **400/707 verbs implemented** (56%)
- **9/51 GUI-dependent processors** correctly stubbed for headless
- **19/51 non-GUI processors** needing investigation

### Processors by Status

**Fully Detected (100%):**
- frontier, kb, math, mouse, pict, point, rectangle, rgb, speaker

**Mostly Detected (70-99%):**
- op (97%), xml (92%), html (91%), string (90%), date (86%), menu (85%), clock (85%), db (84%), lang (74%), dialog (73%), file (69%)

**GUI-Dependent (Correctly Stubbed):**
- window, editmenu, filemenu, statusbar, htmlcontrol, mainwindow, clipboard, launch, mrcalendar

**Genuinely Stubbed (Headless Only):**
- script, thread, tcp, base64, bit, dll, re, rez, and 14 more

## Coverage Reports

The analyzer generates detailed markdown reports showing:

**Example from `COVERAGE_REPORT-2025-12-14-02.md`:**

```markdown
### frontier

**Status:** 14/14 verbs (100% implemented, 0 stubbed)

| # | Verb | Status | Location |
|---|------|--------|----------|
| 0 | `getprogrampath` | ✅ Implemented | shellsysverbs.c:712 |
| 1 | `getfilepath` | ✅ Implemented | shellsysverbs.c:721 |
| 2 | `enableagents` | ✅ Implemented | shellsysverbs.c:739 |
...
```

Each processor shows:
- Verb count with implementation percentage
- Per-verb status (✅ Implemented or ⬜ Stub)
- Source file location with line numbers
- GUI processor identification

## Discovered Processors

The parser currently finds **51 verb processors** with **707 total verbs**:

- op (45 verbs) - Outline processor
- table (18 verbs) - Table operations
- file (86 verbs) - File system operations
- frontier (14 verbs) - Application-level operations
- string (60 verbs) - String manipulation
- sys (16 verbs) - System operations
- lang (58 verbs) - Language runtime
- ... and 44 more

## Pattern Matching & Exception Tables

The analyzer handles four distinct verb naming patterns:

### Pattern A: Standard Direct Mapping
**Format:** `{processor}{verb}func`
- **Example:** `file` processor: `created` → `filecreatedfunc`
- **Processors:** file, string, html, xml, db, menu, table, and others
- **Automation:** 100%

### Pattern B: Type-Prefix Mapping
**Format:** `{verb}func`
- **Example:** `move` → `movefunc`
- **Automation:** 95%+

### Pattern C: Inconsistent Naming (Exception Tables)
**Format:** Special mappings required
- **Example:** `op.getlinetext` → `linetextfunc` (not `getlinetextfunc`)
- **Processors:** op (3 exceptions), pict (1), frontier (5), sys (3)
- **Exception tables:** See `verb_exceptions.py`
- **Automation:** 90%+ with tables

### Pattern D: Multi-Processor Consolidation
**Format:** Multiple processors in single file (langverbs.c)
- **Processors:** dialog, clock, date, kb, mouse, point, rectangle, rgb, speaker, target
- **Transformation rules:** Generate candidates and try combinations
- **Exception tables:** For special cases (e.g., dialog.notify → notifytdialogfunc)
- **Automation:** 70-100% depending on processor

## Using Exception Tables

Exception tables in `verb_exceptions.py` codify irregular verb-to-C mappings:

```python
PATTERN_C_EXCEPTIONS = {
    'op': {
        'getlinetext': 'linetextfunc',      # Missing "get" prefix
        'subsexpanded': 'getexpandedfunc',  # Completely different
        'getselection': 'getselectfunc',    # Truncated
    },
    'frontier': {
        'getprogrampath': 'programpathfunc',  # Missing "get"
        'ispowerpc': 'isnativefunc',          # Renamed
        'version': 'frontierversionfunc',     # Added prefix
    },
}

PATTERN_D_PROCESSORS = {
    'dialog', 'clock', 'date', 'kb', 'mouse',
    'point', 'rectangle', 'rgb', 'speaker', 'target'
}

PATTERN_D_EXCEPTIONS = {
    'dialog': {
        'notify': 'notifytdialogfunc',        # Typo in C code
        'getpassword': 'askpassworddialogfunc', # Different verb
    },
}
```

To add new exceptions:
1. Identify the RC verb and actual C token
2. Add entry to appropriate table in `verb_exceptions.py`
3. Run analyzer to verify detection
4. Update coverage report

## Adding New Exception Table Entries

When investigating a processor and discovering unmapped verbs, you can add them to the exception tables:

### For Pattern C Processors (op, pict, frontier, sys)

Edit `verb_exceptions.py` and add to the appropriate processor's dict in `PATTERN_C_EXCEPTIONS`:

```python
PATTERN_C_EXCEPTIONS = {
    'op': {
        'getlinetext': 'linetextfunc',  # Add new entries here
        'newverb': 'newverbfunc',
    },
}
```

Then verify with:
```bash
python3 -m unittest test_analyzer.TestExceptionTables -v
```

### For Pattern D Processors (dialog, clock, date, kb, mouse, point, rectangle, rgb, speaker, target)

Edit `verb_exceptions.py` and add to the appropriate processor's dict in `PATTERN_D_EXCEPTIONS`:

```python
PATTERN_D_EXCEPTIONS = {
    'dialog': {
        'notify': 'notifytdialogfunc',  # Add new entries here
        'newverb': 'newverbdialogfunc',
    },
}
```

The `generate_pattern_d_candidates()` function will automatically try your exception mapping first.

### Testing Your Changes

After adding exception table entries:

1. Run the analyzer on the processor:
   ```bash
   python3 cli.py analyze
   ```

2. Check the coverage report:
   ```bash
   python3 cli.py report
   ```

3. Look for your processor in the output to verify detection improved

## Investigating Low-Coverage Processors

If a processor shows 0% or very low coverage, use these debugging steps:

### 1. Check if Implementation File Exists

```bash
python3 -c "from analyzer import VerbImplementationAnalyzer; a = VerbImplementationAnalyzer([]); print(a.find_implementation_file('processorname'))"
```

If this returns `None`, the implementation file doesn't exist in expected locations.

### 2. Analyze the Implementation File

Once you find the file, examine it for:

- **Enum definitions**: Look for `enum { ... }` blocks containing verb tokens
- **Case statements**: Search for `case` labels matching possible verb names
- **Pattern matching**: Check which of the 4 patterns the processor uses

### 3. Verify Verb Name Extraction

Extract verb names directly from the C source:

```python
from analyzer import VerbImplementationAnalyzer
analyzer = VerbImplementationAnalyzer([])
source = open('path/to/file.c').read()
verbs = analyzer.extract_verb_names_from_enum(source, 'processorname')
print(verbs)
```

### 4. Debug Case Extraction

If verbs are extracted but not matched to implementations:

```python
case_source, line = analyzer.extract_case_implementation(source, ['verbfunc', 'verb_func'])
print(f"Found: {line}")
```

### 5. Add Exception Table Entry

If the verb doesn't follow standard patterns, add an exception table entry (see above) and re-run.

## Running Unit Tests

Execute all 31 unit tests:

```bash
python3 -m unittest test_analyzer -v
```

Expected output:
```
Ran 31 tests in 0.254s
OK
```

Test categories:
- **TestExceptionTables** (7 tests): Exception table lookups and Pattern D candidate generation
- **TestFileDiscovery** (6 tests): Implementation file discovery for all processor patterns
- **TestEnumExtraction** (3 tests): Enum definition parsing from C source
- **TestCaseExtraction** (3 tests): Case statement extraction from switch blocks
- **TestVerbImplementationAnalysis** (3 tests): Stub detection and implementation analysis
- **TestPatternMatchers** (3 tests): Carbon API and UI adapter pattern detection
- **TestAnalyzerIntegration** (4 tests): Real-world processor analysis
- **TestMetadataWriter** (2 tests): Metadata statistics and whitelist generation

## Troubleshooting

### Analyzer Reports 0% Coverage for a Processor

1. **Check implementation file location**:
   ```bash
   python3 cli.py analyze 2>&1 | grep "processorname"
   ```
   Look for "Warning: No implementation file found"

2. **Verify file paths** match expected locations:
   - `Common/source/{processor}verbs.c`
   - `Common/source/lang{processor}.c`
   - `tests/headless_{processor}_verbs.c`
   - Special cases: `shellsysverbs.c`, `shellwindowverbs.c`

3. **Check for Pattern D** processors:
   - If processor is in `PATTERN_D_PROCESSORS`, it should be in `langverbs.c`
   - Verify `langverbs.c` exists at `Common/source/langverbs.c`

### Verbs Detected but No Implementations Found

1. **Enum extraction succeeded** but **case extraction failed**
   - The verb name was found in the enum but has no case statement
   - This is a legitimate stub - the verb is declared but not implemented

2. **Case extraction found something** but **marked as stub**:
   - The analyzer detected a stub marker in the implementation
   - Look for `/* not implemented */`, `/* TODO */`, or similar
   - Remove the stub marker if the implementation is complete

### False Positives (Implemented Verbs Marked as Stubs)

If real implementations are marked as stubs:

1. **Check for stub markers** in the case body that shouldn't be there
2. **Add annotation** to the implementation if heuristics fail:
   ```c
   case verbfunc: { /* @implemented */
       // Real implementation here
   }
   ```

### Coverage Report Missing Data

If the coverage report is incomplete or doesn't show a processor:

1. Run with verbose output:
   ```bash
   python3 cli.py analyze
   ```

2. Check the processor is discovered:
   ```bash
   python3 parse_kernelverbs.py ../../Common/resources/Win32/kernelverbs.rc /tmp/test.c
   ```

3. Verify `test_analyzer.py` passes:
   ```bash
   python3 -m unittest test_analyzer.TestAnalyzerIntegration -v
   ```

## Maintenance

The analyzer tools are designed to be simple and maintainable:

**analyzer.py**:
- ~460 lines of Python with type hints
- Pattern matching on C source without modification
- Exception table lookups for irregular mappings
- No external dependencies

**verb_exceptions.py**:
- ~194 lines of Python with exception table dicts
- Helper functions for pattern detection
- Easy to extend with new processor exceptions

**cli.py**:
- ~300 lines of Python
- Command-line interface for analysis, reporting, and verification
- Auto-dating report filenames with sequential numbering

**test_analyzer.py**:
- 31 comprehensive unit tests
- Tests all patterns, exceptions, and edge cases
- All tests pass in ~0.25 seconds
- Uses stdlib unittest (no external dependencies)
