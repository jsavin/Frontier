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

1. Parses `kernelverbs.rc` to find all EFP blocks
2. Extracts processor names, verb counts, and other metadata
3. Generates `kernel_verbs_init.c` with:
   - Forward declarations for all `<processor>initverbs()` functions
   - A `headless_init_kernel_verbs()` function that calls them all

## Generated Output

The generated file looks like:

```c
/* Auto-generated from kernelverbs.rc - DO NOT EDIT BY HAND */

extern boolean opinitverbs(void);          /* EFP 1000: op (45 verbs) */
extern boolean fileinitverbs(void);        /* EFP 1007: file (86 verbs) */
extern boolean frontierinitverbs(void);    /* EFP 1016: frontier (14 verbs) */
/* ... 48 more processors ... */

boolean headless_init_kernel_verbs(void) {
    if (!opinitverbs())
        return false;

    if (!fileinitverbs())
        return false;

    /* ... */

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

For each processor found, you need to create a corresponding init function:

1. Create `tests/headless_<processor>_verbs.c`
2. Implement `<processor>initverbs()` function
3. Add the file to `frontier-cli/Makefile` HEADLESS_STUBS

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

## Error Handling

If a processor init function is missing at link time, you'll get an "undefined symbol" error:

```
Undefined symbols for architecture arm64:
  "_opinitverbs", referenced from:
      _headless_init_kernel_verbs in kernel_verbs_init.o
```

This means you need to implement `opinitverbs()` in `tests/headless_op_verbs.c`.

## Maintenance

The parser is designed to be simple and robust:
- ~200 lines of Python
- Uses regex to extract processor definitions
- Handles comments and variable whitespace
- No external dependencies beyond Python 3

To update:
1. Modify `kernelverbs.rc` as needed
2. Run `make` - the parser runs automatically
3. Implement any new processor init functions

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
