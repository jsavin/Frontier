# Script Object Architecture

## Overview

In Frontier, **scripts are outline objects** with a special processor type (`idscriptprocessor`). This document explains how script objects are structured, stored, and executed in the Frontier Object Database (ODB).

## Script-as-Outline Pattern

### Key Concept

Scripts are not simple text strings - they are **outline data structures** with:
1. **Source text** stored in the outline's summit (root) headline
2. **Compiled bytecode** linked via the script object's external references
3. **Outline structure** that can be manipulated via op verbs (outline processor)

### Why Outlines?

This design provides:
- **Unified storage model**: Scripts use the same ODB storage as other outline objects
- **Structured editing**: Scripts can be manipulated using outline operations
- **Compilation integration**: Compiled code is linked to the outline object
- **Database persistence**: Scripts are saved with the rest of the ODB

## Script Object Structure

### Internal Representation

```c
// Variable record for script object
tyvaluerecord scriptvar;
scriptvar.valuetype = externalvaluetype;
scriptvar.data.externalvalue = hdloutlinerecord;  // Pointer to outline

// Outline record (simplified)
typedef struct tyoutlinerecord {
    hdlheadrecord hsummit;        // Root headline (contains source text)
    hdlhashtable htable;          // Symbol table (unused for scripts)
    boolean fldirty;              // Modified flag
    // ... other fields
} tyoutlinerecord;

// Summit headline (root node)
typedef struct tyheadrecord {
    Handle headstring;            // SOURCE TEXT stored here
    hdlheadrecord headlinkleft;   // Left sibling
    hdlheadrecord headlinkright;  // Right sibling
    hdlheadrecord headlinkdown;   // First child
    // ... other fields
} tyheadrecord;
```

### Script Lifecycle

1. **Creation** (`lang.new(scriptType, @addr)`)
   - Creates outline object with empty summit headline
   - Sets external processor to `idscriptprocessor`
   - Initializes outline structure

2. **Set Source** (`script.setsource(@addr, @sourcevar)` or direct assignment)
   - Stores source text in summit headline's `headstring`
   - Clears any existing compiled code
   - Marks outline as dirty

3. **Compilation** (`script.compile(@addr)`)
   - Reads source from summit headline
   - Parses and compiles to bytecode
   - Links compiled code via `opverblinkcode()`
   - Returns true/false for success/failure

4. **Execution** (Production Patterns)
   - **Direct evaluation**: `@myscript()` - Call compiled script directly
   - **Parameterized**: `lang.callscript(@myscript, params)` - Pass parameters
   - **Inline**: `lang.evaluate(sourcetext)` - Evaluate source string
   - Note: `script.run(@addr)` is NOT production code (never implemented)

5. **Persistence**
   - Script saved to ODB as external outline value
   - Source text persists in outline structure
   - Compiled code typically NOT saved (recompiled on load)

## Source Storage Details

### Summit Headline Text

Scripts store their source in the **summit (root) headline** of the outline:

```c
// Getting source text
hdloutlinerecord ho = (hdloutlinerecord)((**hv).variabledata);
hdlheadrecord hsummit = (**ho).hsummit;
Handle hsource = (**hsummit).headstring;  // Source text here

// Setting source text
disposehandle((**hsummit).headstring);    // Dispose old source
(**hsummit).headstring = hsourcetext;     // Set new source
(**ho).fldirty = true;                    // Mark dirty
```

### Empty Summit Baseline

**IMPORTANT**: All new outlines (including scripts) start with **one empty summit headline**.

```usertalk
lang.new(scriptType, @myscript);  // Creates outline with 1 empty headline
// Summit exists but headstring is empty or nil initially
```

This is fundamental to Frontier's outline implementation (see `docs/OUTLINE_STRUCTURE.md`).

## Compiled Code Linking

### Code Storage

Compiled bytecode is stored separately from source and linked via the script object:

```c
// Linking compiled code to script
boolean opverblinkcode(hdlexternalvariable hv, hdltreenode hcode) {
    // Associates compiled tree with script external variable
    // Code is NOT stored in outline structure
    // Code is stored in variable's external data
}

// Getting compiled code
hdltreenode hcode = nil;
if (langexternalgetcode(hv, &hcode)) {
    // hcode now points to compiled bytecode tree
}
```

### Compilation Flow

1. **Parse source** → Abstract Syntax Tree (AST)
2. **Compile AST** → Bytecode tree (`hdltreenode`)
3. **Link bytecode** → Associate with script variable
4. **Execute** → Interpret bytecode tree

### Code Invalidation

When source is modified:
- Clear existing compiled code: `opverblinkcode(hv, nil)`
- Next execution requires recompilation
- This ensures source/code consistency

## Script Verbs Implementation

### Key Functions

Located in `tests/headless_script_verbs.c`:

```c
// script.compile(@addr) - Compile source to bytecode
boolean script_compile(hdlexternalvariable hv);

// script.getsource(@addr, @dest) - Get source text
boolean script_getsource(hdlexternalvariable hv, hdlhashtable htable, bigstring varname);

// script.setsource(@addr, @source) - Set source text
boolean script_setsource(hdlexternalvariable hv, hdlhashtable htable, bigstring varname);

// script.getcode(@addr, @dest) - Get compiled code
boolean script_getcode(hdlexternalvariable hv, hdlhashtable htable, bigstring varname);

// script.setcode(@addr, @code) - Set compiled code
boolean script_setcode(hdlexternalvariable hv, hdlhashtable htable, bigstring varname);

// NOTE: script.run(@addr) is NOT a production verb - use direct evaluation:
//   @myscript()                              // Direct call
//   lang.callscript(@myscript, params)       // With parameters
//   lang.evaluate(sourcetext)                // Inline evaluation
```

### Helper Functions

```c
// Get script parameter (validates script type)
static boolean script_getscriptparam(hdltreenode hfirst, short pnum,
                                      hdlexternalvariable *hvreturned);

// Push outline context (required for op operations)
// NOTE: Uses global state - see Issue #296 for thread-safety refactoring
oppushoutline(ho);
// ... operations on outline ...
oppopoutline();
```

## Related Source Files

### Core Implementation

- **`Common/source/langexternal.c`** - External variable management (scripts are external values)
- **`Common/source/op.c`** - Outline processor (oppushoutline, oppopoutline, opverblinkcode)
- **`Common/source/langrun.c`** - Script execution runtime
- **`Common/source/langcompile.c`** - UserTalk compiler (source → bytecode)
- **`tests/headless_script_verbs.c`** - Script processor verb implementations

### Related Headers

- **`Common/headers/langexternal.h`** - External value API
- **`Common/headers/op.h`** - Outline processor types and functions
- **`Common/headers/lang.h`** - Language runtime types

## Architecture Gotchas

### 1. Global State for Outline Context ⚠️

**CRITICAL**: `oppushoutline()` and `oppopoutline()` use **global mutable state** (`outlinedata`, `outlinestack`).

**Issue**: This is a thread-safety problem for collaborative ODB editing (Frontier 2.0 vision).

**Fix**: See **Issue #296** - Global state elimination for outline context (launch-blocking).

**Pattern**: Use thread-local storage or explicit context for outline operations.

### 2. Script Objects Require Workspace Table

**Current Limitation**: Integration tests use `@workspace.testScript` pattern, which requires system root database.

**Why**: The workspace table is provided by the system root database (`Frontier.root`).

**Future**: Refactor tests to use self-contained table structures:
```usertalk
lang.new(tableType, @local.testTable);
lang.new(scriptType, @local.testTable.script);
```

See `tests/integration/test_cases/script_processor_verbs.yaml` header comment.

### 3. Compiled Code Persistence

**Current Behavior**: Compiled code is typically NOT saved to disk with the script.

**Reason**:
- Bytecode format may change between Frontier versions
- Source is canonical - recompile on load ensures compatibility
- Reduces ODB storage overhead

**Exception**: Future optimization may cache compiled code with version checking.

### 4. Empty Summit Baseline

All new scripts start with **one empty summit headline** (see `docs/OUTLINE_STRUCTURE.md`):

```usertalk
lang.new(scriptType, @s);     // Creates outline with 1 empty headline
// op.countlines(@s) returns 1, not 0
```

**Implication**: Source text replaces the empty summit, doesn't add to it.

## Testing

### Integration Tests

See `tests/integration/test_cases/script_processor_verbs.yaml`:
- Script compilation and execution
- Source and code get/set operations
- Error handling and recovery
- Script lifecycle tests

### Unit Tests

C unit tests in `tests/headless_script_verbs.c`:
- Parameter validation
- Script type checking
- Source/code manipulation

## Future Enhancements

### 1. Thread-Safe Outline Context (Issue #296)

**Goal**: Eliminate global state for `oppushoutline()`/`oppopoutline()`.

**Approach**: Thread-local storage or explicit context.

**Benefit**: Enables concurrent script operations for collaborative ODB.

### 2. Self-Contained Test Scripts

**Goal**: Remove dependency on system root database for testing.

**Approach**: Use `@local.testTable.script` pattern instead of `@workspace.script`.

**Benefit**: Tests run without requiring `Frontier.root` to be loaded.

### 3. Bytecode Caching

**Goal**: Persist compiled code to reduce recompilation overhead.

**Approach**:
- Add version tag to compiled code
- Save/load bytecode with version check
- Recompile if version mismatch

**Benefit**: Faster script execution, especially for large scripts.

## References

- **`docs/OUTLINE_STRUCTURE.md`** - Outline data structure details
- **`docs/VERB_IMPLEMENTATION_GUIDE.md`** - Kernel verb implementation patterns
- **Issue #296** - Global state elimination for outline context (launch-blocking)
- **Issue #135** - Outline context refactoring (original issue)
- **`planning/phase3/op_verb_implementation_plan.md`** - Op verb roadmap

## Summary

Scripts in Frontier are **outline objects** with source stored in the summit headline and compiled code linked externally. This design:
- ✅ Provides unified ODB storage for all object types
- ✅ Enables structured editing via outline operations
- ✅ Separates source (canonical) from bytecode (transient)
- ⚠️ Uses global state for outline context (Issue #296 - launch-blocking fix required)

Understanding this architecture is essential for:
- Implementing script processor verbs
- Debugging script compilation/execution issues
- Planning thread-safety refactoring (Frontier 2.0 collaborative editing)
