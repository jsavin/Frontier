# Kernelverbs Parser

Automatically generates `kernel_verbs_init.c` from the Windows resource file `kernelverbs.rc`.

## Overview

Frontier's kernel verbs are defined in `Common/resources/Win32/kernelverbs.rc` using the EFP (External Function Processor) resource format. This tool parses that file and generates C initialization code that calls all the verb processor init functions.

## Usage

```bash
python3 parse_kernelverbs.py <input.rc> <output.c>
```

Example:
```bash
python3 tools/kernelverbs_parser/parse_kernelverbs.py \
    Common/resources/Win32/kernelverbs.rc \
    generated/kernel_verbs_init.c
```

The Makefile calls this automatically when building.

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

This means:
- If the `#ifdef` block is present in the source, the parser will find it
- The whitelist approach handles this safely - conditionally compiled processors won't be in `HEADLESS_IMPLEMENTED` unless explicitly added
- No link errors will occur from missing processors

If you need to handle conditional compilation:
1. Ensure the processor is only added to `HEADLESS_IMPLEMENTED` when it should be included
2. Or preprocess `kernelverbs.rc` before parsing (using `cpp` or similar)

## Maintenance

The parser is designed to be simple and robust:
- ~250 lines of Python with type hints
- Uses regex to extract processor definitions
- Handles comments and variable whitespace
- Whitelist-based filtering for safety
- No external dependencies beyond Python 3

To update:
1. Modify `kernelverbs.rc` as needed
2. Run `make` - the parser runs automatically
3. Implement any new processor init functions
4. Add processor names to `HEADLESS_IMPLEMENTED` whitelist

## Testing

The parser includes basic validation:
- Checks that input file exists
- Warns if no processors found
- Reports statistics (processor count, verb count)
- Creates output directory if needed

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
