# Kernel Verbs Code Generation Plan

## Current State

We have successfully implemented **Option B** with manual verb registration:
- `headless_frontier_verbs.c` - 14 frontier verbs manually registered
- `headless_file_verbs.c` - 86 file verbs manually registered
- Both use hardcoded token enums and ADD_VERB macros

## Goal: Automatic Generation from kernelverbs.rc

Generate `kernel_verbs_init.c` that calls all verb initialization functions automatically.

## Source Data: kernelverbs.rc Format

The `.rc` file uses Windows resource format with EFP (External Function Processor) blocks:

```
1016 /*idfrontierverbs*/ EFP DISCARDABLE
    BEGIN
    1,                      // Number of "blocks" in this resource
        "frontier\0",       // Function Processor Name
        true,               // Window required
            14,             // Count of verbs
            "getprogrampath\0",
            "getfilepath\0",
            ...
    END

1007 /* idfileverbs */ EFP DISCARDABLE
    BEGIN
    1,                      // Number of "blocks" in this resource
        "file\0",           // Function Processor Name
        false,              // Window required
            86,             // Count of verbs
            "created\0",
            "modified\0",
            ...
    END
```

## What We Need to Generate

### Target Output Structure

```c
/* generated/kernel_verbs_init.c - Auto-generated from kernelverbs.rc */

#include "frontier.h"
#include "standard.h"
#include "langinternal.h"

/* Forward declarations for verb initialization functions */
extern boolean frontierinitverbs(void);
extern boolean fileinitverbs(void);
extern boolean opinitverbs(void);
extern boolean stringinitverbs(void);
/* ... more processors ... */

boolean headless_init_kernel_verbs(void) {
    /* Initialize all kernel verb processors */
    if (!frontierinitverbs())
        return false;

    if (!fileinitverbs())
        return false;

    if (!opinitverbs())
        return false;

    /* ... more processors ... */

    return true;
}
```

## Implementation Approaches

### Approach 1: Simple Python Parser (Recommended)

**Effort: ~2-4 hours**

**What to build:**
1. Python script that parses kernelverbs.rc
2. Extracts EFP blocks with processor names
3. Generates C code with initialization calls

**Files to create:**
- `tools/kernelverbs_parser/parse_kernelverbs.py` (200-300 lines)
- `tools/kernelverbs_parser/README.md` (documentation)

**Parser complexity:**
- **Low** - Simple text parsing with regex
- Input: Windows .rc format (text file)
- Output: C source file

**Example parsing logic:**
```python
import re

def parse_efp_blocks(rc_content):
    """Extract EFP blocks from kernelverbs.rc"""
    processors = []

    # Match: 1016 /*comment*/ EFP DISCARDABLE
    efp_pattern = r'(\d+)\s+/\*.*?\*/\s+EFP\s+DISCARDABLE'

    # Match processor name: "frontier\0",
    name_pattern = r'"([^"]+)\\0"'

    for match in re.finditer(efp_pattern, rc_content):
        efp_id = match.group(1)
        # Find the processor name in the following lines
        start = match.end()
        name_match = re.search(name_pattern, rc_content[start:start+200])
        if name_match:
            proc_name = name_match.group(1)
            processors.append({
                'id': efp_id,
                'name': proc_name,
                'init_func': f'{proc_name}initverbs'
            })

    return processors

def generate_init_c(processors):
    """Generate kernel_verbs_init.c from processor list"""
    lines = [
        "/* Auto-generated from kernelverbs.rc - DO NOT EDIT */",
        "#include \"frontier.h\"",
        "#include \"standard.h\"",
        "",
        "/* Forward declarations */",
    ]

    # Add forward declarations
    for proc in processors:
        lines.append(f"extern boolean {proc['init_func']}(void);")

    lines.extend([
        "",
        "boolean headless_init_kernel_verbs(void) {",
    ])

    # Add initialization calls
    for proc in processors:
        lines.extend([
            f"    if (!{proc['init_func']}())",
            f"        return false;",
            ""
        ])

    lines.extend([
        "    return true;",
        "}",
        ""
    ])

    return "\n".join(lines)
```

**Integration with Makefile:**
```makefile
# Add to frontier-cli/Makefile
KERNELVERBS_RC = ../Common/resources/Win32/kernelverbs.rc
KERNELVERBS_PARSER = ../tools/kernelverbs_parser/parse_kernelverbs.py

$(KERNEL_VERBS_C): $(KERNELVERBS_RC) $(KERNELVERBS_PARSER)
	@mkdir -p $(GENERATED_DIR)
	python3 $(KERNELVERBS_PARSER) $(KERNELVERBS_RC) $(KERNEL_VERBS_C)

# Make kernel_verbs_generated depend on actual generation
kernel_verbs_generated: $(KERNEL_VERBS_C)
```

**Pros:**
- Very simple implementation
- No external dependencies beyond Python 3
- Easy to maintain and extend
- Consistent with existing `strings_compiler` approach

**Cons:**
- Doesn't validate that implementation functions exist
- Requires creating implementation files separately

### Approach 2: Full Code Generator with Stubs

**Effort: ~1-2 weeks**

**What to build:**
1. Parser for kernelverbs.rc (same as Approach 1)
2. Generator for stub implementation files
3. Token enum generator
4. Validation that all verbs are implemented

**Would generate:**
- `generated/kernel_verbs_init.c` - Initialization dispatcher
- `generated/frontier_verbs.h` - Token enums for frontier processor
- `generated/file_verbs.h` - Token enums for file processor
- `generated/frontier_verbs_stub.c` - Template implementations
- `generated/file_verbs_stub.c` - Template implementations

**Example stub generation:**
```c
/* generated/frontier_verbs_stub.c */
enum {
    frv_getProgramPath = 0,
    frv_getFilePath = 1,
    /* ... auto-generated from kernelverbs.rc ... */
};

static boolean frontier_valueproc(short token, ...) {
    switch (token) {
        case frv_getProgramPath:
            /* TODO: Implement frontier.getProgramPath */
            langerrormessage(BIGSTRING("\pnot implemented"));
            return false;

        /* ... */
    }
}

boolean frontierinitverbs(void) {
    /* ... auto-generated registration ... */
}
```

**Pros:**
- Comprehensive solution
- Generates token enums automatically
- Can validate completeness
- Reduces manual work for new verb processors

**Cons:**
- Much more complex
- Takes longer to implement
- May over-engineer for current needs
- Requires more tooling maintenance

### Approach 3: Use Existing Legacy Tools

**Effort: Unknown (research required)**

Check if original Frontier had EFP compilation tools.

**Tasks:**
1. Search legacy codebase for EFP compiler
2. Port to modern C if found
3. Integrate into build system

**Risk: High** - May not exist or be too platform-specific

## Recommendation

**Use Approach 1: Simple Python Parser**

**Reasoning:**
1. **Minimal effort** - 2-4 hours vs 1-2 weeks
2. **Current need is simple** - Just need initialization dispatcher
3. **Proven pattern** - Mirrors existing `strings_compiler` tool
4. **Easy maintenance** - ~200 lines of Python vs complex code generator
5. **Good ROI** - Solves the immediate problem without over-engineering

**What we already have manually:**
- ✅ Token enums (in headless_frontier_verbs.c, headless_file_verbs.c)
- ✅ Implementation functions (frontierinitverbs, fileinitverbs)
- ✅ Verb registration logic (ADD_VERB macros)

**What we need automated:**
- 🎯 Call all init functions from one place
- 🎯 Ensure we don't miss any processors when adding new ones

## Implementation Plan (Approach 1)

### Step 1: Create Parser (1-2 hours)
```bash
mkdir -p tools/kernelverbs_parser
cd tools/kernelverbs_parser
```

Create `parse_kernelverbs.py`:
- Parse kernelverbs.rc for EFP blocks
- Extract processor names
- Generate initialization function calls

### Step 2: Update Makefile (30 min)
- Add dependency on kernelverbs.rc
- Call parser to regenerate when .rc changes
- Remove "No-op" from kernel_verbs_generated target

### Step 3: Create Implementation Files (2-3 hours)
For each processor in kernelverbs.rc that we want to support:
- Create `headless_<processor>_verbs.c`
- Implement the `<processor>initverbs()` function
- Add to Makefile HEADLESS_STUBS

**Priority processors to implement:**
1. ✅ frontier (14 verbs) - DONE
2. ✅ file (86 verbs) - DONE
3. 🔄 string (52 verbs) - Common operations
4. 🔄 op (45 verbs) - Outline processor
5. ⏳ sys (29 verbs) - System verbs
6. ⏳ table (18 verbs) - Table operations

### Step 4: Test (1 hour)
- Verify all init functions are called
- Confirm verbs work as expected
- Test build system regeneration

## Total Effort Estimate

**Approach 1 (Recommended):**
- **Initial setup: 4-5 hours**
  - Parser: 1-2 hours
  - Makefile integration: 30 min
  - Testing: 1 hour
- **Per additional processor: 1-2 hours**
  - Stub implementation
  - Token definitions
  - Integration

**Current status:**
- ✅ 2 processors done (frontier, file)
- 🎯 Code generation would save ~8-10 hours for remaining ~5-8 processors
- 🎯 Prevents human error in registration

## Benefits

1. **Automatic discovery** - Parser finds all EFP blocks
2. **No missed processors** - Generates call for every processor
3. **Single source of truth** - kernelverbs.rc drives generation
4. **Easy updates** - Modify .rc, rebuild, done
5. **Documentation** - Generated code shows all processors

## Next Steps

If you want to proceed with automatic generation:

1. **Quick win (today):**
   - Write the Python parser (~1 hour)
   - Generate kernel_verbs_init.c
   - Update Makefile to call it

2. **Medium term (this week):**
   - Implement 2-3 more processors (string, op, sys)
   - Validate the approach scales well

3. **Future (as needed):**
   - Add remaining processors
   - Consider Approach 2 if we need stub generation

## Questions to Consider

1. **How many processors do we need?**
   - All 20+ in kernelverbs.rc?
   - Just the core 5-10?

2. **Implementation strategy:**
   - Full implementations like frontier/file?
   - Stub "not implemented" for most?

3. **Testing approach:**
   - Unit tests for each verb?
   - Integration tests only?

## Files to Create

```
tools/kernelverbs_parser/
├── parse_kernelverbs.py      # Main parser script
├── README.md                  # Usage documentation
└── test_parser.py            # Unit tests for parser

generated/
└── kernel_verbs_init.c       # Auto-generated (replaces stub)
```

## Conclusion

**Bottom line:** Automatic generation is **very achievable** with **4-5 hours of work** using a simple Python parser. The ROI is excellent - it will save time, reduce errors, and make the system more maintainable.

The current manual approach for frontier and file verbs is perfectly fine and serves as a good foundation. Code generation would primarily help with:
1. **Initialization dispatcher** - Calling all init functions
2. **Scalability** - Adding new processors becomes trivial
3. **Maintenance** - Changes to kernelverbs.rc auto-propagate

**Recommendation: Proceed with Approach 1 when you're ready to add more verb processors.**
