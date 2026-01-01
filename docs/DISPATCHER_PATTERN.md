# Dispatcher Pattern for Verb Bindings

**Status**: Established pattern (as of PR #221)
**Author**: String verb implementation (2025-12-31)
**Related**: [Issue #222](https://github.com/jsavin/Frontier/issues/222)

## Table of Contents

1. [Overview](#overview)
2. [When to Use](#when-to-use)
3. [Implementation Guide](#implementation-guide)
4. [Build Integration](#build-integration)
5. [Analyzer Detection](#analyzer-detection)
6. [Testing](#testing)
7. [Troubleshooting](#troubleshooting)
8. [Examples](#examples)

---

## Overview

The **dispatcher pattern** is a lightweight binding technique that forwards all verb calls from headless mode to existing implementations without duplicating code. This pattern was established in PR #221 for string verbs and reduced implementation from 591 lines of stubs to 223 lines of working code.

### Key Benefits

- **Zero code duplication**: Reuses existing implementations
- **Compile-time safety**: `_Static_assert` fails build if token enum drifts out of sync (mandatory as of PR #224)
- **Auto-detected**: Analyzer recognizes pattern and marks all verbs as implemented
- **Type-safe**: Compiler enforces token synchronization and parameter types
- **Fast to implement**: ~1-2 hours per processor vs days of per-verb implementations

### Architecture

```
UserTalk code
    ↓
Headless dispatcher callback (headless_<processor>_verbs_callback)
    ↓
Forward to real implementation (<processor>functionvalue)
    ↓
Existing C implementation (e.g., stringverbs.c)
```

---

## When to Use

### ✅ Use Dispatcher Pattern When:

1. **Complete implementation exists** - All or most verbs are implemented in a single `<processor>functionvalue()` function
2. **Token-based dispatch** - Original uses `switch(token)` for verb routing
3. **No headless-specific logic needed** - Verbs work the same in headless and windowed mode
4. **Portable APIs available** - Any platform-specific APIs have portable equivalents

**Examples**: string, table, date, math processors

### ❌ Don't Use Dispatcher Pattern When:

1. **Verbs need custom headless implementations** - Different behavior required for headless mode
2. **Heavy UI dependencies** - Verbs depend on windows, dialogs, or UI state
3. **Mixed implementation status** - Some verbs implemented, many unimplemented
4. **No central dispatch function** - Verbs scattered across multiple files

**Examples**: window, dialog, menu processors (need headless-specific stubs)

---

## Implementation Guide

### Step 1: Analyze Original Implementation

**Goal**: Understand the original processor's architecture

```bash
# Find the original implementation
grep -r "boolean.*functionvalue" Common/source/*verbs.c

# Example for string processor:
# Common/source/stringverbs.c:
#   boolean stringfunctionvalue(short token, hdltreenode hparam1,
#                               tyvaluerecord *vreturned, bigstring bserror)
```

**Check for**:
- Central dispatch function name
- Token enum type and range
- Parameter signature
- Platform-specific APIs that need wrapping

### Step 2: Create Token Enum with Compile-Time Verification

**File**: `tests/headless_<processor>_verbs.c`

```c
/* Token enum for all verbs in the <processor> processor
 *
 * CRITICAL: This enum MUST be kept in sync with ty<processor>token in <processor>verbs.c
 *
 * Verification:
 *   1. Token order must match exactly (0=verb1, 1=verb2, etc.)
 *   2. Token count must match ct<processor>verbs value
 *   3. Compile-time assertion below will fail if count mismatches
 */
enum {
    verb1func = 0,
    verb2func = 1,
    verb3func = 2,
    /* ... all tokens ... */
    lastverbfunc = N,

    /* Sentinel - must equal ct<processor>verbs from <processor>verbs.c */
    <processor>v_count
};

/* Compile-time verification that token count matches <processor>verbs.c
 * If this fails, the enum above is out of sync with ty<processor>token */
#define EXPECTED_<PROCESSOR>_VERB_COUNT (N+1)
_Static_assert(<processor>v_count == EXPECTED_<PROCESSOR>_VERB_COUNT,
               "Token enum out of sync with <processor>verbs.c - update headless_<processor>_verbs.c");
```

**Critical**: Token values MUST match the original implementation exactly. Mismatched enums cause undefined behavior.

**Compile-Time Safety** (REQUIRED):
- The `_Static_assert` ensures build fails if token count drifts out of sync
- This prevents silent dispatch errors where verbs route to wrong implementations
- Pattern introduced in PR #224 (table verbs) and is now **mandatory** for all dispatchers

**Manual Verification** (Belt-and-Suspenders):
```bash
# Compare token counts
grep -c "case.*func:" Common/source/<processor>verbs.c
# Should match EXPECTED_<PROCESSOR>_VERB_COUNT
```

### Step 3: Forward Declaration

```c
/* Forward declaration of the actual implementation in <processor>verbs.c */
extern boolean <processor>functionvalue(short token,
                                        hdltreenode hparam1,
                                        tyvaluerecord *vreturned,
                                        bigstring bserror);
```

**For non-static functions**: No changes needed to original file

**For static functions**: Remove `static` keyword from original implementation:

```c
// In Common/source/<processor>verbs.c
// Before:
static boolean <processor>functionvalue(...) { ... }

// After:
#ifdef FRONTIER_HEADLESS
boolean <processor>functionvalue(...) { ... }
#else
static boolean <processor>functionvalue(...) { ... }
#endif
```

### Step 4: Dispatcher Callback

```c
static boolean <processor>_valueproc(short token,
                                     hdltreenode hparam1,
                                     tyvaluerecord *vreturned,
                                     bigstring bserror) {
    /*
     * Dispatcher for <processor> verbs in headless mode.
     * Simply forwards all calls to the actual implementation.
     */
    boolean result;

    log_debug(LOG_COMP_LANG, "<processor>_valueproc: ENTRY token=%d", token);

    result = <processor>functionvalue(token, hparam1, vreturned, bserror);

    if (bserror && bserror[0] > 0) {
        char errmsg[256];
        copyptocstring(bserror, errmsg);
        log_debug(LOG_COMP_LANG, "<processor>_valueproc: EXIT token=%d result=%d bserror='%s'",
                  token, result, errmsg);
    } else {
        log_debug(LOG_COMP_LANG, "<processor>_valueproc: EXIT token=%d result=%d",
                  token, result);
    }

    return result;
}
```

**Logging**: Optional but recommended for debugging. Use `LOG_COMP_LANG` component.

### Step 5: Exported Callback

```c
/* Exported callback used by headless kernel verb bootstrap */
boolean headless_<processor>_verbs_callback(short token,
                                           hdltreenode hparam1,
                                           tyvaluerecord *vreturned,
                                           bigstring bserror) {
    return <processor>_valueproc(token, hparam1, vreturned, bserror);
}
```

**Naming convention**: MUST be `headless_<processor>_verbs_callback` for analyzer detection.

### Step 6: Headless Init Function

```c
/* Headless-specific <processor> verb initialization function
 *
 * This replaces <processor>initverbs() for headless mode, registering
 * the <processor> processor with headless_<processor>_verbs_callback.
 *
 * Returns: true if processor was successfully registered, false otherwise
 */
boolean <processor>initverbs(void) {
    hdlhashtable htable = nil;
    bigstring bsname;

    extern boolean newfunctionprocessor(bigstring, langvaluecallback, boolean, hdlhashtable*);
    extern boolean langaddkeyword(bigstring, short);
    extern boolean pushhashtable(hdlhashtable);
    extern boolean pophashtable(void);

    log_debug(LOG_COMP_LANG, "<processor>initverbs: registering headless processor");

    copystring(BIGSTRING("\0XX<processor>"), bsname);  // XX = length byte

    if (!newfunctionprocessor(bsname, &headless_<processor>_verbs_callback, true, &htable))
        return false;

    pushhashtable(htable);

    /* Register all verbs */
    #define ADD_VERB(name, tok) do { \
        bigstring bs; \
        copystring(name, bs); \
        if (!langaddkeyword(bs, tok)) { \
            pophashtable(); \
            return false; \
        } \
    } while(0)

    ADD_VERB(BIGSTRING("\004verb1"), verb1func);
    ADD_VERB(BIGSTRING("\004verb2"), verb2func);
    /* ... all verbs ... */

    pophashtable();

    log_debug(LOG_COMP_LANG, "<processor>initverbs: processor registered successfully");

    return true;
}
```

**Pascal strings**: Format is `\0XXname` where XX is hex length. Example: `\006string` = 6-char string "string"

**BIGSTRING macro**: Automatically handles Pascal string format

### Step 7: Annotation

Add to file header:

```c
/*
 * headless_<processor>_verbs.c - <Processor> verbs for headless mode
 *
 * @IMPLEMENTED - All N verbs forward to <processor>functionvalue() in <processor>verbs.c
 *
 * Created: YYYY-MM-DD - <Description>
 */
```

**@IMPLEMENTED**: Overrides stub detection, ensures analyzer marks all verbs as implemented

---

## Build Integration

### Step 1: Add Implementation to Makefile

**File**: `frontier-cli/Makefile`

Add the original implementation source to COMMON_SOURCES:

```makefile
COMMON_SOURCES = \
    ../Common/source/<processor>verbs.c \
    # ... other sources
```

**Do NOT add** `tests/headless_<processor>_verbs.c` - it's already linked via kernel verb init

### Step 2: Handle Duplicate Symbols

If the original implementation has helper functions that conflict with stubs:

**Option A - Disable stubs** (Preferred):

```c
// In tests/headless_lang_runtime_more_stubs.c
#if 0  /* Disabled - now provided by <processor>verbs.c */
boolean helperfunction(...) {
    return false;  // stub
}
#endif
```

**Option B - Make helpers static** (if only used internally):

```c
// In Common/source/<processor>verbs.c
static boolean helperfunction(...) { ... }
```

### Step 3: Wrap Platform-Specific APIs

If original implementation has Mac/Windows-specific APIs:

```c
// In Common/source/<processor>verbs.c
#ifdef FRONTIER_HEADLESS
    // Use portable equivalent
    portablefunction(args);
#else
    // Use platform-specific API
    MacFunction(args);
#endif
```

**Common replacements**:
- `UppercaseText()` → `uppertext()`
- `LowercaseText()` → `lowertext()`
- File APIs → `file_portable.c` equivalents

### Step 4: Conditional Init Function

Wrap windowed init function to prevent conflicts:

```c
// In Common/source/<processor>verbs.c
#ifndef FRONTIER_HEADLESS
boolean <processor>initverbs(void) {
    return (loadfunctionprocessor(id<processor>verbs, &<processor>functionvalue));
}
#endif /* !FRONTIER_HEADLESS */
```

**Reason**: Headless mode provides its own `<processor>initverbs()` in `headless_<processor>_verbs.c`

---

## Analyzer Detection

The verb analyzer automatically detects dispatcher patterns via:

### Detection Logic

**File**: `tools/kernelverbs_parser/analyzer.py` (lines 501-528)

```python
# Detect dispatcher pattern
dispatcher_pattern = re.search(rf'headless_{processor_name}_verbs_callback', source)
is_dispatcher = dispatcher_pattern is not None

if is_dispatcher:
    from matchers import detect_stub_verb
    file_is_stub = detect_stub_verb(source)
    if not file_is_stub:
        # Dispatcher with real implementation - all verbs are implemented
        return [
            VerbImplementation(
                processor=processor_name,
                verb_name=verb_names[i],
                token=i,
                is_implemented=True,
                impl_file=impl_file,
                impl_line=0,
                # ... other fields
            )
            for i in range(len(verb_names))
        ]
```

### Requirements

1. **Callback naming**: MUST match pattern `headless_<processor>_verbs_callback`
2. **Not a stub**: Must not contain stub indicators (`TODO`, `not implemented`, etc.)
3. **@IMPLEMENTED annotation**: Optional but recommended as insurance

### Verification

```bash
# Run analyzer to verify detection
cd tools/kernelverbs_parser
python3 cli.py report

# Should show processor as "Implemented" with all verbs bound
```

---

## Testing

### Integration Tests (TDD Approach)

**File**: `tests/integration/test_cases/<processor>_verbs.yaml`

**Write tests FIRST** before implementing dispatcher:

```yaml
tests:
  - name: "<processor>.verb1 - description"
    description: "What this verb does"
    script: '<processor>.verb1("arg")'
    expected_success: true
    expected_result: "expected output"

  - name: "<processor>.verb2 - error case"
    description: "Error handling test"
    script: '<processor>.verb2(invalid_arg)'
    expected_success: false
    expected_error_type: "script_error"
```

**Run tests**:
```bash
python3 tests/integration/runner.py tests/integration/test_cases/<processor>_verbs.yaml
```

### Manual Verification

```bash
# Test individual verbs
FRONTIER_HEADLESS_SKIP_STARTUP=1 ./frontier-cli/frontier-cli -e '<processor>.verb("test")'

# Test error handling
FRONTIER_HEADLESS_SKIP_STARTUP=1 ./frontier-cli/frontier-cli -e '<processor>.verb(invalid)'
```

### Build Verification

```bash
# Clean build
cd frontier-cli
make clean
make

# Should build with no errors, no duplicate symbols
```

### Test Suite

```bash
# Run full headless test suite
./tools/run_headless_tests.sh

# All tests should pass
```

---

## Troubleshooting

### Issue: Duplicate Symbol Errors

**Symptom**:
```
duplicate symbol '_helperfunc' in:
    <processor>verbs.o
    headless_stubs.o
```

**Solution**: Disable stub or make function static (see [Build Integration](#step-2-handle-duplicate-symbols))

---

### Issue: Token Mismatch - Wrong Verb Called

**Symptom**: `<processor>.verb1()` executes `verb2` logic

**Cause**: Token enum values don't match original

**Solution**:
1. Compare enum values: `diff -u <(sed -n '/enum/,/}/p' Common/source/<processor>verbs.c) <(sed -n '/enum/,/}/p' tests/headless_<processor>_verbs.c)`
2. Fix mismatched values
3. Rebuild and test

---

### Issue: Verbs Not Detected by Analyzer

**Symptom**: Analyzer reports verbs as "Not implemented" despite dispatcher

**Cause**: Callback naming doesn't match pattern

**Solution**:
1. Verify callback name: `grep "headless_.*_verbs_callback" tests/headless_<processor>_verbs.c`
2. Must match exactly: `headless_<processor>_verbs_callback`
3. Add `@IMPLEMENTED` annotation as insurance

---

### Issue: Platform API Unavailable in Headless

**Symptom**:
```
error: use of undeclared identifier 'MacFunction'
```

**Solution**: Wrap platform-specific code (see [Build Integration](#step-3-wrap-platform-specific-apis))

---

### Issue: Linker Error - Undefined Symbol

**Symptom**:
```
Undefined symbols for architecture x86_64:
  "_<processor>functionvalue", referenced from:
      _headless_<processor>_verbs_callback
```

**Solution**:
1. Verify implementation added to Makefile: `grep "<processor>verbs.c" frontier-cli/Makefile`
2. Verify function is not static: `grep "^boolean <processor>functionvalue" Common/source/<processor>verbs.c`
3. Rebuild: `make clean && make`

---

## Examples

### Example 1: String Verbs (Reference Implementation)

**Files**:
- Implementation: `tests/headless_string_verbs.c` (223 lines)
- Original: `Common/source/stringverbs.c`
- Integration tests: `tests/integration/test_cases/string_verbs.yaml`

**Results**:
- 60 verbs implemented
- 21/21 integration tests passing
- Code reduced from 591 stub lines to 223 dispatcher lines (62% reduction)

**See**: PR #221

---

### Example 2: Table Verbs (Upcoming)

**Files**:
- Implementation: `tests/headless_table_verbs.c` (to be created)
- Original: `Common/source/tableverbs.c`
- Integration tests: `tests/integration/test_cases/table_verbs.yaml` (TDD)

**Target**: 5 initial verbs (assign, getcursor, goto, emptytable, packtable)

**See**: `planning/phase3/VERB_BINDING_QUICK_WINS.md`

---

## Checklist

Use this checklist when implementing a new dispatcher:

- [ ] Analyzed original implementation location and signature
- [ ] Created token enum matching original (verified counts match)
- [ ] **Added compile-time assertion (`_Static_assert`) for token count** ⚠️ **REQUIRED**
- [ ] Added forward declaration (removed `static` if needed)
- [ ] Implemented dispatcher callback with logging
- [ ] Created exported `headless_<processor>_verbs_callback`
- [ ] Implemented init function with all verb registrations
- [ ] Added `@IMPLEMENTED` annotation to file header
- [ ] Added implementation to `frontier-cli/Makefile`
- [ ] Wrapped platform-specific APIs with `#ifdef FRONTIER_HEADLESS`
- [ ] Disabled conflicting stubs if needed
- [ ] Wrapped windowed init with `#ifndef FRONTIER_HEADLESS`
- [ ] Created integration test file (TDD)
- [ ] Verified `make clean && make` succeeds (confirms `_Static_assert` passes)
- [ ] Verified integration tests pass
- [ ] Verified full test suite passes (`./tools/run_headless_tests.sh`)
- [ ] Verified analyzer detects pattern (`python3 cli.py report`)
- [ ] Manual smoke tests for key verbs

---

## References

- **PR #221**: String verb dispatcher implementation (original pattern)
- **PR #224**: Table verb dispatcher (introduced `_Static_assert` pattern)
- **Issue #222**: This documentation
- **Issue #223**: Analyzer unit tests for dispatcher detection
- `tests/headless_string_verbs.c`: Reference implementation (original)
- `tests/headless_table_verbs.c`: Reference implementation (with compile-time assertions)
- `tools/kernelverbs_parser/analyzer.py`: Dispatcher detection logic
- `planning/phase3/VERB_BINDING_QUICK_WINS.md`: Phase 1 quick wins plan

---

**Last Updated**: 2026-01-01 (compile-time assertion pattern made mandatory)
**Maintainer**: Verb binding workstream
