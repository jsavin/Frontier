# Kernelverbs Parser & Stub Generator

Automatically generates C stub files for all 51 verb processors and central initialization code from the Windows resource file `kernelverbs.rc`.

## Overview

Frontier's kernel verbs are defined in `Common/resources/Win32/kernelverbs.rc` using the EFP (External Function Processor) resource format. This toolset:

1. **`parse_kernelverbs.py`** - Parses the RC file and generates `kernel_verbs_init.c` which initializes all registered processors
2. **`generate_processor_stubs.py`** - Generates skeleton `headless_<processor>_verbs.c` files for all unimplemented processors
3. **Unit Tests** - Comprehensive tests for both tools with 37 test cases

## Tools at a Glance

| Tool | Purpose | Input | Output |
|------|---------|-------|--------|
| `parse_kernelverbs.py` | Discover all processors & generate init calls | `kernelverbs.rc` | `kernel_verbs_init.c` |
| `generate_processor_stubs.py` | Create stub implementations for verbs | `kernelverbs.rc` + processor list | `headless_<proc>_verbs.c` files |
| `test_parse_kernelverbs.py` | Unit tests for parser | N/A | Test results |
| `test_generate_stubs.py` | Unit tests for stub generator | N/A | Test results |

## Quick Start

### Using the Tools

The Makefile automatically runs both tools during the build process:

```bash
make -C frontier-cli
```

This will:
1. Run `parse_kernelverbs.py` to generate `kernel_verbs_init.c`
2. Run `generate_processor_stubs.py` to create/update all processor stub files
3. Compile everything

### Manual Usage

#### Parse kernelverbs.rc (generate init code)

```bash
python3 tools/kernelverbs_parser/parse_kernelverbs.py \
    Common/resources/Win32/kernelverbs.rc \
    generated/kernel_verbs_init.c
```

#### Generate processor stubs (all 51 at once)

```bash
python3 tools/kernelverbs_parser/generate_processor_stubs.py \
    Common/resources/Win32/kernelverbs.rc \
    tests
```

This creates/updates `headless_<processor>_verbs.c` files in the `tests/` directory.

#### Run unit tests

```bash
cd tools/kernelverbs_parser
python3 -m unittest test_parse_kernelverbs test_generate_stubs -v
```

Expected output: **37 tests pass** with real kernelverbs.rc file

## What It Does

1. Parses `kernelverbs.rc` to find all EFP blocks (51 processors, 707 verbs)
2. Extracts processor names, verb counts, and other metadata
3. Filters to only processors with headless implementations (whitelist in `HEADLESS_IMPLEMENTED`)
4. Generates `kernel_verbs_init.c` with:
   - Forward declarations for implemented `<processor>initverbs()` functions
   - A `headless_init_kernel_verbs()` function that calls only implemented processors

## Whitelist Approach

The parser uses a **whitelist** to only generate code for processors that have headless implementations. This prevents link errors for unimplemented processors.

Currently implemented processors (in `HEADLESS_IMPLEMENTED` set):
- `file` - File system operations (86 verbs)
- `frontier` - Application-level operations (14 verbs)

To add a processor:
1. Implement `tests/headless_<processor>_verbs.c`
2. Add processor name to `HEADLESS_IMPLEMENTED` in `parse_kernelverbs.py`
3. Run `make` to regenerate

## Generated Output

The generated file looks like:

```c
/* Auto-generated from kernelverbs.rc - DO NOT EDIT BY HAND */

/* Forward declarations for IMPLEMENTED verb processor initialization functions */
extern boolean fileinitverbs(void);        /* EFP 1007: file (86 verbs) */
extern boolean frontierinitverbs(void);    /* EFP 1016: frontier (14 verbs) */

/**
 * headless_init_kernel_verbs - Initialize all kernel verb processors
 *
 * Implemented processors: 2 of 51 total
 * Implemented verbs: 100 of 707 total
 */
boolean headless_init_kernel_verbs(void) {
    if (!fileinitverbs())
        return false;

    if (!frontierinitverbs())
        return false;

    return true;
}
```

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

## Implementation Requirements

To add a new processor to the headless implementation:

1. Create `tests/headless_<processor>_verbs.c` with the init function
2. Add the file to `frontier-cli/Makefile` HEADLESS_STUBS section
3. Add processor name to `HEADLESS_IMPLEMENTED` in `parse_kernelverbs.py`
4. Run `make` to regenerate `kernel_verbs_init.c`

Example for the `file` processor:

```c
/* tests/headless_file_verbs.c */

boolean fileinitverbs(void) {
    hdlhashtable htable = nil;
    bigstring bsname;

    copystring(BIGSTRING("\pfile"), bsname);

    if (!newfunctionprocessor(bsname, &file_valueproc, false, &htable))
        return false;

    pushhashtable(htable);

    /* Register all verbs using ADD_VERB macro */
    ADD_VERB(BIGSTRING("\pcreated"), fv_created);
    /* ... 85 more verbs ... */

    pophashtable();
    return true;
}
```

## Safety and Error Prevention

The whitelist approach prevents common errors:

- **No link errors**: Only implemented processors are included in generated code
- **Explicit opt-in**: Processors must be added to `HEADLESS_IMPLEMENTED` to be included
- **Clear diagnostics**: Parser output shows implemented vs. unimplemented processors
- **Build safety**: If you forget to add a processor to the whitelist, it simply won't be initialized (no crash)

## Conditional Compilation

Some processors in `kernelverbs.rc` are wrapped in `#ifdef` directives (e.g., `#ifdef flregexpverbs`). The parser does **not** preprocess these directives - it reads the file as-is.

### How the Whitelist Approach Handles This

The whitelist-based design provides automatic safety for conditionally compiled processors:

1. **Parser discovers all processors**: Regardless of `#ifdef` blocks, the parser finds all processor definitions in the source file
2. **Whitelist controls output**: Only processors in `HEADLESS_IMPLEMENTED` are included in generated code
3. **Safe by default**: Conditionally compiled processors won't cause link errors unless explicitly whitelisted
4. **No preprocessing needed**: The parser doesn't need to preprocess the RC file

### Handling Strategies for Future Needs

If you need more sophisticated conditional compilation handling:

#### Strategy 1: Conditional Whitelist (Recommended)
Use Python to conditionally populate `HEADLESS_IMPLEMENTED` based on environment variables or build flags:

```python
# In parse_kernelverbs.py
if os.getenv('ENABLE_REGEX_VERBS'):
    HEADLESS_IMPLEMENTED.add('regex')
```

Then build with: `ENABLE_REGEX_VERBS=1 make`

#### Strategy 2: Multiple Whitelist Files
Create variant whitelist files for different configurations:

```python
# parse_kernelverbs.py could accept a whitelist config file
WHITELIST_FILE = os.getenv('WHITELIST_CONFIG', 'whitelist_headless.txt')
```

#### Strategy 3: Preprocessor Support
If RC file preprocessing is needed, preprocess before parsing:

```bash
# In Makefile
cpp -E Common/resources/Win32/kernelverbs.rc | \
    python3 tools/kernelverbs_parser/parse_kernelverbs.py - generated/kernel_verbs_init.c
```

This would require updating the parser to read from stdin.

#### Strategy 4: Dynamic Registration
Register processors at runtime rather than compile-time:

Instead of generating init calls, register all discovered processors dynamically:

```c
// In headless_init_kernel_verbs()
for (each processor in discovered_list) {
    if (implementation_exists(processor)) {
        register_processor(processor);
    }
}
```

### Current Behavior

Currently, the parser:
- ✅ Discovers all 51 processors regardless of `#ifdef` blocks
- ✅ Only generates code for whitelisted processors (currently file and frontier)
- ✅ Prevents link errors for unimplemented processors automatically
- ❌ Does not preprocess or evaluate `#ifdef` conditions

This is the correct default behavior for a headless implementation.

## Common Tasks

### Add a New Processor to Headless Implementation

1. **Implement the processor**: Create or edit `tests/headless_<processor>_verbs.c`
   - Implement the `<processor>_valueproc` function
   - Implement the `<processor>initverbs()` function
   - Register verbs using the ADD_VERB macro

2. **Add to whitelist**: Edit `parse_kernelverbs.py`
   ```python
   HEADLESS_REGISTERED.add('myprocessor')
   ```

3. **Rebuild**: Run `make -C frontier-cli`
   - The parser will auto-discover your processor
   - Generated code will include initialization calls

### Regenerate All Stubs (After RC Changes)

If `kernelverbs.rc` is modified:

```bash
cd tools/kernelverbs_parser
python3 generate_processor_stubs.py \
    ../../Common/resources/Win32/kernelverbs.rc \
    ../../tests
make -C ../../frontier-cli
```

### Debug Verb Name Issues

If verbs aren't registering correctly:

1. **Check extraction warnings**:
   ```bash
   python3 generate_processor_stubs.py \
       ../../Common/resources/Win32/kernelverbs.rc \
       ../../tests 2>&1 | grep -i warning
   ```

2. **Verify verb names in generated file**:
   ```bash
   grep "enum {" -A 5 ../../tests/headless_<processor>_verbs.c
   ```

3. **Look for placeholder names**: If you see `verb0`, `verb1`, extraction failed
   - The RC file may have missing verb definitions
   - Check for duplicate processor names in RC file

### Debug Parser Issues

If `kernel_verbs_init.c` isn't generating correctly:

1. **Check if processors are discovered**:
   ```bash
   python3 parse_kernelverbs.py \
       ../../Common/resources/Win32/kernelverbs.rc \
       /tmp/test_output.c 2>&1
   ```

2. **Check generated code**:
   ```bash
   head -30 /tmp/test_output.c
   ```

3. **Verify whitelist**: Check that your processor is in `HEADLESS_REGISTERED`

## Troubleshooting

### Tests Fail

**Problem**: Tests fail with "kernelverbs.rc not found"
- **Solution**: Ensure you're running from `tools/kernelverbs_parser` directory and `kernelverbs.rc` is at `Common/resources/Win32/kernelverbs.rc`

**Problem**: Stub generation creates files with placeholder verb names
- **Cause**: `extract_verb_names()` couldn't find actual verb names in RC file
- **Solution**:
  1. Verify RC file structure for that processor
  2. Check for duplicate verb names (generator warns about these)
  3. Ensure processor definition includes proper `true/false` and verb count

**Problem**: "redefinition of enumerator" compilation error
- **Cause**: RC file has duplicate verb names for a processor
- **Solution**:
  1. Check RC file for duplicate entries
  2. Run generator with stderr redirected to see warnings
  3. The generator will auto-rename duplicates with `_1`, `_2` suffix

### Build Issues

**Problem**: Processors don't initialize on startup
- **Check**: Is processor in `HEADLESS_REGISTERED` whitelist?
- **Check**: Does stub file exist and compile?
- **Check**: Does `kernel_verbs_init.c` have init call?

**Problem**: "undefined reference" linker error
- **Cause**: Processor stub file not in Makefile
- **Solution**: Add to `HEADLESS_STUBS` in `frontier-cli/Makefile`

## Maintenance

The tools are designed to be simple and robust:

**parse_kernelverbs.py**:
- ~300 lines of Python with type hints
- Uses regex to extract processor definitions
- Whitelist-based filtering for safety
- No external dependencies

**generate_processor_stubs.py**:
- ~150 lines of Python with type hints
- Extracts real verb names from RC content
- Auto-detects and handles duplicates
- Generates syntactically valid C code
- No external dependencies

**Testing**:
- 37 comprehensive unit tests
- Tests both tools with real kernelverbs.rc
- All tests pass in ~10ms
- No external test framework needed (uses stdlib unittest)


## Integration with Build System

The Makefile target looks like:

```makefile
$(KERNEL_VERBS_C): $(KERNELVERBS_RC) $(KERNELVERBS_PARSER)
    @mkdir -p $(GENERATED_DIR)
    python3 $(KERNELVERBS_PARSER) $(KERNELVERBS_RC) $(KERNEL_VERBS_C)
```

This ensures the generated file is rebuilt whenever:
- `kernelverbs.rc` changes
- The parser script changes
