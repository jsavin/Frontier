# Pattern C & D Solution Proposal

> **Note (2026-02):** The `HEADLESS_REGISTERED` whitelist in `parse_kernelverbs.py` has been replaced. Registration is now derived from `tests/headless_verbs.mk` filenames. See PR #419.

**Date:** 2025-12-14
**Status:** Awaiting Approval
**Approach:** Hybrid - Exception Tables + Strategic C Refactoring

---

## Executive Summary

After deep analysis, we've identified that Pattern C and D affect **13 processors** with **~130 verbs**. The agent analysis recommends **NOT doing wholesale C refactoring** but instead using **exception tables** to handle the minority of problematic cases.

**Proposed Approach:**
1. **Pattern C (3 processors, ~60 verbs):** Use exception tables for 3-5 verbs per processor
2. **Pattern D (10 processors, ~70 verbs):** Use whitelist + transform rules + exception tables
3. **Expected automation:** ~95% with minimal manual maintenance

---

## Pattern C: Inconsistent Naming

### Affected Processors
- **op** (45 verbs) - 3 exceptions
- **pict** (4 verbs) - 1 exception
- Possibly 1-2 more (needs full scan)

### The Problem

**op processor example:**
- RC: "getlinetext" → Expected: `getlinetextfunc` → **Actual:** `linetextfunc` (stripped "get")
- RC: "subsexpanded" → Expected: `subsexpandedfunc` → **Actual:** `getexpandedfunc` (completely different!)
- RC: "getselection" → Expected: `getselectionfunc` → **Actual:** `getselectfunc` (truncated)

Only 3 verbs out of 45 are problematic. The other 42 follow the standard `{verb}func` pattern.

### Proposed Solution: Exception Table

**Don't refactor the C code.** These names are stable and used throughout the codebase. Instead, codify the exceptions:

```python
# Pattern C: Inconsistent naming exceptions
PATTERN_C_EXCEPTIONS = {
    'op': {
        'getlinetext': 'linetextfunc',
        'subsexpanded': 'getexpandedfunc',
        'getselection': 'getselectfunc',
    },
    'pict': {
        'expressions': 'evalfunc',
    },
}
```

**Lookup algorithm:**
1. Try standard pattern: `{verb}func`
2. Check exception table if not found
3. Search enum for partial matches as last resort

**Effort:** Low - just a Python dict, ~10 lines per processor

---

## Pattern D: Multi-Processor Consolidation

### Affected Processors

**10 small processors** consolidated into `Common/source/langverbs.c`:
- dialog (19 verbs)
- clock (7 verbs)
- date (30 verbs)
- kb (4 verbs)
- mouse (2 verbs)
- point (2 verbs)
- rectangle (2 verbs)
- rgb (2 verbs)
- speaker (3 verbs)
- target (3 verbs)

**Total:** ~70 verbs

### The Problem

**Architecture mismatch:**
```
RC file: Lists as separate processors
         ↓
C code: All in ONE file (langverbs.c)
         ↓
Enum tokens: Include processor name as infix
         ↓
Example: dialog.alert → alertdialogfunc
```

**Helper files exist** (langdialog.c, langdate.c, etc.) but contain NO dispatch logic - just implementation helpers.

### Transformation Patterns

**Most common (Pattern D1):** Add processor name as infix
```
RC: dialog.run → rundialogfunc
RC: dialog.getvalue → getdialogvaluefunc
RC: kb.scan → scankbfunc
RC: mouse.click → clickmousefunc
```

**Less common (Pattern D2):** Prefix processor name
```
RC: dialog.alert → alertdialogfunc
RC: dialog.twoway → twowaydialogfunc
```

**Exceptions (Pattern D3):** Complete mismatch
```
RC: dialog.notify → notifytdialogfunc (typo in C!)
RC: dialog.getpassword → askpassworddialogfunc (different verb!)
```

### Proposed Solution: Whitelist + Transform Rules + Exception Table

**Step 1: Whitelist Pattern D processors**
```python
PATTERN_D_PROCESSORS = {
    'dialog', 'clock', 'date', 'kb', 'mouse',
    'point', 'rectangle', 'rgb', 'speaker', 'target'
}
```

**Step 2: File discovery override**
```python
def find_implementation_file(processor_name):
    if processor_name in PATTERN_D_PROCESSORS:
        return 'Common/source/langverbs.c'
    # ... normal logic
```

**Step 3: Transform rules**
```python
def generate_pattern_d_labels(processor, verb):
    """Generate possible enum tokens for Pattern D"""
    candidates = [
        f"{verb}{processor}func",      # Pattern D1: getvaluedialogfunc
        f"{processor}{verb}func",      # Pattern D2: alertdialogfunc
        f"{verb}func",                 # Fallback: standard pattern
    ]
    return candidates
```

**Step 4: Exception table**
```python
PATTERN_D_EXCEPTIONS = {
    'dialog': {
        'notify': 'notifytdialogfunc',       # Typo in C
        'getpassword': 'askpassworddialogfunc',  # Different verb
    },
    # Add more as discovered
}
```

**Effort:** Medium - ~50 lines of code, exception table grows as we find edge cases

---

## Alternative: Strategic C Refactoring

### If we wanted to refactor (NOT recommended)

**Option 1: Normalize Pattern C**
- Rename `linetextfunc` → `getlinetextfunc` in opverbs.c
- Rename `getexpandedfunc` → `subsexpandedfunc` in opverbs.c
- Rename `getselectfunc` → `getselectionfunc` in opverbs.c

**Pros:** Consistent pattern
**Cons:**
- Requires changing enum AND all case labels
- Risk of typos/bugs
- Minimal benefit (only 3 verbs)

**Option 2: Split Pattern D Processors**
- Create separate files: dialogverbs.c, clockverbs.c, etc.
- Move verbs out of langverbs.c
- Each gets its own enum following standard pattern

**Pros:** Clean separation
**Cons:**
- **LARGE effort** - touch 10 processors, ~70 verbs
- Requires moving code between files
- May break existing logic that assumes consolidation
- Historical precedent for consolidation (small processors grouped)

---

## Recommendation

### **Use Exception Tables (Hybrid Approach)**

**Rationale:**
1. **Pattern C affects <5% of verbs** - exception table is simpler than refactoring
2. **Pattern D is intentional architecture** - consolidation makes sense for small processors
3. **Exception tables are maintainable** - clear, documented, version-controlled
4. **No risk to existing C code** - analyzer adapts to codebase, not vice versa
5. **95% automation achieved** - good enough for our needs

**Trade-offs:**
- ✅ Zero risk to working C code
- ✅ Fast to implement (~1 day)
- ✅ Easy to extend as we find more exceptions
- ❌ Some manual mapping needed (but small)
- ❌ Exceptions must be maintained if C code changes (rare)

---

## Implementation Plan

### Phase 1: Pattern C Support (1-2 hours)
1. Add `PATTERN_C_EXCEPTIONS` dict to analyzer
2. Update `generate_case_labels()` to check exceptions first
3. Test on op processor

### Phase 2: Pattern D Support (2-3 hours)
1. Add `PATTERN_D_PROCESSORS` whitelist
2. Update `find_implementation_file()` to return langverbs.c for these processors
3. Add transform rules for Pattern D1/D2
4. Add `PATTERN_D_EXCEPTIONS` for edge cases
5. Test on dialog, clock, kb processors

### Phase 3: Full Scan (2-3 hours)
1. Run analyzer on all 51 processors
2. Identify any additional exceptions
3. Update exception tables
4. Document findings

### Phase 4: Validation (1 hour)
1. Generate whitelist from analyzer
2. Compare to current HEADLESS_REGISTERED
3. Verify 40+ processors detected (80%+ coverage)

**Total effort:** ~8 hours (1 day)

---

## Questions for Approval

1. **Do you approve the exception table approach?** (vs. C refactoring)
2. **Is 95% automation acceptable?** (vs. 100% with larger refactoring)
3. **Should we proceed with implementation?** (Phase 1-4 above)
4. **Any processors you'd prefer we DO refactor?** (e.g., op processor has only 3 exceptions - could be renamed)

---

## Supporting Documentation

Created by sub-agent analysis:
- `/Users/jake/dev/jsavin/Frontier/tools/kernelverbs_parser/COMPLETE_ANALYSIS_REPORT.md`
- `/Users/jake/dev/jsavin/Frontier/tools/kernelverbs_parser/op_processor_analysis.md`
- `/Users/jake/dev/jsavin/Frontier/tools/kernelverbs_parser/dialog_processor_analysis.md`
- `/Users/jake/dev/jsavin/Frontier/tools/kernelverbs_parser/PATTERN_ANALYSIS_SUMMARY.md`

---

## Next Steps

Awaiting your decision:
- ✅ **Approve exception table approach** → Proceed with implementation
- 🔄 **Request C refactoring instead** → Create refactoring plan
- 📋 **Need more information** → Investigate specific processors
