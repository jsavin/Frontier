# Verb Callback Mechanism and Resolution

**Date**: 2025-12-25
**Issue**: #166 - UserTalk integration tests for table mutation tracking
**Status**: RESOLVED for `lang.new()` - bare `new()` requires additional work

## Overview

Frontier's verb dispatch system uses callback functions stored in hash tables to route UserTalk verb calls to their implementations. This document captures the mechanism and gotchas discovered during fixing verb registration in headless mode.

## Architecture

### Verb Family Registration

Verb families (like `lang`, `string`, `table`, `file`, etc.) are registered via `efptable` (External Function Processor table):

1. **GUI Mode**: `loadfunctionprocessor(id, callback)` loads verb definitions from resources
2. **Headless Mode**: `init_efp_*()` functions (e.g., `init_efp_1005()` for lang) register verbs programmatically

### Callback Dispatch Chain

```
UserTalk: lang.new(tableType, @t)
  ↓
Name resolution finds "new" in system.verbs.lang
  ↓
Gets token from keyword table (e.g., lanv_new = 1)
  ↓
Calls (**ht).valueroutine(token=1, hparam1, vreturned)
  ↓
lang_valueproc(1, ...) in headless_lang_verbs.c
  ↓
switch(1) → case lanv_new:
  ↓
newvaluefunc() - actual implementation
```

### Critical Components

- **valueroutine**: Function pointer on hash table that handles verb dispatch
- **Keyword registration**: `langaddkeyword()` adds verb names with token values
- **Processor creation**: `newfunctionprocessor()` creates the verb table and sets valueroutine

## The Bug

### Problem: Callback Overwriting

In headless mode, the initialization sequence was:

```c
// Step 1: langinitverbs() in headless_lang_verbs.c
newfunctionprocessor(bsname, &lang_valueproc, false, &htable)
langaddkeyword(...)  // Register "new" verb with token=1
// → Sets (**htable).valueroutine = lang_valueproc

// Step 2: headless_init_kernel_verbs() in kernel_verbs_headless.c
init_efp_1005(&langfunctionvalue)  // Called AFTER Step 1!
// → Overwrites with (**htable).valueroutine = langfunctionvalue
// → Breaks the dispatch chain!
```

### Root Cause

The auto-generated `headless_init_kernel_verbs()` was calling all `init_efp_*()` functions, including `init_efp_1005()`, which re-registered the lang processor with a different callback.

## The Fix

**Solution**: Skip re-registration of verbs that were already initialized by `langinitverbs()`.

In `Common/source/kernel_verbs_headless.c`, commented out the call to `init_efp_1005()` since `langinitverbs()` already handles lang verb registration with the correct callback.

**Changes**:
- Modified `kernel_verbs_headless.c` to prevent duplicate lang processor registration
- Added `langinitresources_headless()` wrapper in `langstartup.c` to ensure keywords are initialized before verb registration
- Modified `db_format.c` to call `langinitresources_headless()` before `langinitverbs()` in headless mode

## Current Status

✅ **FIXED**: `lang.new(tableType, @t)` now works correctly
- Keywords are properly initialized
- Verb callbacks are preserved through initialization
- The dispatch chain works end-to-end

❌ **TODO**: Bare `new()` without `lang.` prefix
- Currently `new()` is registered in the lang verb family, not globally
- Calling `new(tableType, @t)` without `lang.` prefix doesn't resolve
- Needs either:
  - Registration in global builtin table, OR
  - Global keyword registration, OR
  - Name resolution enhancement to search verb families

## Testing

```bash
# Works ✓
FRONTIER_HEADLESS_SKIP_STARTUP=1 ./frontier-cli/frontier-cli \
  -e "local (t); lang.new(tableType, @t); t.val=1; return (t.val + sizeOf(t))"
# Output: 2

# Doesn't work ✗ (yet)
FRONTIER_HEADLESS_SKIP_STARTUP=1 ./frontier-cli/frontier-cli \
  -e "local (t); new(tableType, @t); t.val=1; return (t.val + sizeOf(t))"
# Error: Script execution failed
```

## Lessons Learned

1. **Verb Callback Initialization Order Matters**
   - Don't call duplicate registrations with different callbacks
   - Verify callback preservation through the full initialization sequence

2. **Resource Installation Must Precede Verb Registration**
   - Keywords, constants, and built-ins must be initialized first
   - They're referenced by verb dispatch code
   - Use explicit dependencies, not implicit ordering

3. **Headless vs GUI Initialization Differences**
   - GUI: Resources loaded from Mac resource forks
   - Headless: Programmatic registration via init_efp_*() functions
   - Need separate initialization paths for each mode

4. **Debug Strategy**
   - Use logging to track valueroutine callback assignments
   - Verify callbacks are preserved after each initialization step
   - Test with minimal cases (e.g., `1+1`) before complex features

## Files Modified

- `Common/source/langstartup.c` - Added `langinitresources_headless()`
- `Common/headers/lang.h` - Added declaration with FRONTIER_HEADLESS guard
- `Common/source/db_format.c` - Call resource initialization before verb registration
- `Common/source/langexternal.c` - Fixed structural issues in headless fallback code
- `Common/source/kernel_verbs_headless.c` - Prevent duplicate lang processor registration
- `frontier-cli/Makefile` - Ensured headless_lang_verbs.c is compiled

## Next Steps

For complete bare `new()` support:
1. Decide on registration approach (global builtin vs keyword)
2. Implement registration in appropriate table
3. Test and verify with Issue #166 integration test suite
4. Document the chosen approach in USER_TALK_BUILTIN_FUNCTIONS.md

## References

- `Issue #166` - UserTalk integration tests for table mutation tracking
- `langvalue.c:7586` - kernelfunctionvalue() verb dispatch point
- `langverbs.c:818-891` - newvaluefunc() actual implementation
- `headless_lang_verbs.c:83-86` - lang_valueproc() callback function
