# CRITICAL: Verb Dispatch Regression Investigation

**Status:** ✅ RESOLVED - Issue was missing error strings, not verb dispatch
**Date Identified:** 2025-12-29
**Date Resolved:** 2025-12-29
**Priority:** P0 - CRITICAL PATH (was blocking)
**GitHub Issue:** #208 (Resolved)
**Branch:** `feature/table-verbs-phase2`
**Resolution Commit:** acaa5784 "feat: Complete Phase 3 table verb implementations and critical error infrastructure"

---

## RESOLUTION SUMMARY

**Initial Diagnosis:** Appeared that all verb dispatch was broken (verbs returning errors)

**Actual Root Cause:** Missing error message strings in `langerrorlist.yaml` (only 2/165 entries present)

**What Actually Happened:**
- Verbs were working correctly and dispatching properly
- Syntax errors (like using single quotes `'hello'` for multi-char strings) generated valid error codes
- Error message lookup failed because langerrorlist.yaml was incomplete
- Result: Empty error messages made it appear verbs were broken

**The Fix:**
1. Created `tools/generate_error_yaml.py` to extract all 165 error messages from legacy Mac resources
2. Generated complete `langerrorlist.yaml` with proper MacRoman → UTF-8 encoding
3. Implemented `getstringlist()` in `headless_lang_runtime_more_stubs.c`
4. All UserTalk errors now display correctly

**Key Learning:** UserTalk requires double quotes `"..."` for strings, single quotes `'...'` only for character constants (1 or 4 chars). This differs from JavaScript/TypeScript.

**See:** [TABLE_VERB_DISPATCH_POSTMORTEM.md](../../planning/archive/phase3/TABLE_VERB_DISPATCH_POSTMORTEM.md) for complete analysis of hash lookup API issues discovered during investigation.

---

## ORIGINAL INVESTIGATION (Preserved for Historical Context)

**Initial Status (INCORRECT):** 🔴 BLOCKING - All verb dispatch broken

**Initial Executive Summary (INCORRECT):**

**ALL verb dispatch is broken** in the current build. Only arithmetic expressions work. This is a critical regression introduced during Phase 3 table verb implementation work.

**Initial Impact Assessment (INCORRECT):**
- ❌ `table.*` verbs fail (e.g., `table.goto()`)
- ❌ `string.*` verbs fail (e.g., `string.upper()`)
- ❌ Built-in functions fail (e.g., `sizeOf()`)
- ❌ Most `lang.*` verbs likely fail
- ✅ Only arithmetic works (1+1, 2*3)

**NOTE:** The above was incorrect. Verbs were working, but error messages were broken, making syntax errors appear as verb failures.

---

## Test Evidence

```bash
# WORKS - Basic arithmetic
$ FRONTIER_HEADLESS_SKIP_STARTUP=1 ./frontier-cli/frontier-cli -e "1+1"
2  ✅

# FAILS - All verbs
$ FRONTIER_HEADLESS_SKIP_STARTUP=1 ./frontier-cli/frontier-cli -e "sizeOf('hello')"
[lang-ERROR] langcallbacks.c:208: [general-ERROR] cli_utils.c:26: Execution error
❌

$ FRONTIER_HEADLESS_SKIP_STARTUP=1 ./frontier-cli/frontier-cli -e "string.upper('test')"
[lang-ERROR] langcallbacks.c:208: [general-ERROR] cli_utils.c:26: Execution error
❌

$ FRONTIER_HEADLESS_SKIP_STARTUP=1 ./frontier-cli/frontier-cli -e "lang.new(tableType, @t); t.a=1; table.goto(1)"
[lang-ERROR] langcallbacks.c:208: [general-ERROR] cli_utils.c:26: Execution error
❌
```

---

## Timeline

| Date | Commit | Status |
|------|--------|--------|
| Dec 25 | 472ea6c9 | ✅ Last known working (Phase 2 merged) |
| Dec 29 | 130576c9 | ⚠️ Phase 3 implementation started |
| Dec 29 | 3a0aefd6 | ⚠️ langexternalsetdatabase fix |
| Dec 29 | d7ff0b32 | ❌ Verb dispatch broken (current) |

**Hypothesis:** One of the Dec 29 commits broke verb dispatch.

---

## Investigation Plan (CRITICAL PATH)

### Step 1: Git Bisect ⬅️ **START HERE**
```bash
# Find the exact breaking commit
git bisect start
git bisect bad HEAD
git bisect good 472ea6c9  # Last known good (Phase 2 merged)

# Test script for bisect:
FRONTIER_HEADLESS_SKIP_STARTUP=1 ./frontier-cli/frontier-cli -e "sizeOf('hello')" \
  && echo "GOOD" || echo "BAD"

# Automate:
git bisect run ./tools/bisect_verb_dispatch_test.sh
```

### Step 2: Test Last Known Good
```bash
# Verify 472ea6c9 actually works
git checkout 472ea6c9
make clean && env SANITIZE=1 make -C frontier-cli
FRONTIER_HEADLESS_SKIP_STARTUP=1 ./frontier-cli/frontier-cli -e "sizeOf('hello')"
# Expected: Should work
```

### Step 3: Identify Root Cause
Once bisect finds breaking commit, analyze:
- What files changed?
- What verb registration code was modified?
- Is `generated/kernel_verbs_init.c` stale? (last modified Dec 25)
- Did `tableinitverbs()` shadow existing registration?
- Did callback wiring break?

### Step 4: Fix Root Cause
Based on bisect findings:
- Revert problematic changes if necessary
- Fix verb registration if broken
- Regenerate `kernel_verbs_init.c` if stale
- Fix callback wiring if incorrect

### Step 5: Verify Fix
```bash
# Test all verb families
FRONTIER_HEADLESS_SKIP_STARTUP=1 ./frontier-cli/frontier-cli -e "sizeOf('hello')"
FRONTIER_HEADLESS_SKIP_STARTUP=1 ./frontier-cli/frontier-cli -e "string.upper('test')"
FRONTIER_HEADLESS_SKIP_STARTUP=1 ./frontier-cli/frontier-cli -e "lang.new(tableType, @t)"
FRONTIER_HEADLESS_SKIP_STARTUP=1 ./frontier-cli/frontier-cli -e "math.max(1,2)"
```

### Step 6: Create Test Gate
Add pre-commit hook to prevent future regressions:
```bash
# .git/hooks/pre-commit
#!/bin/bash
if ! FRONTIER_HEADLESS_SKIP_STARTUP=1 ./frontier-cli/frontier-cli -e "sizeOf('hello')" >/dev/null 2>&1; then
    echo "ERROR: Verb dispatch broken!"
    exit 1
fi
```

---

## Potential Root Causes (Hypotheses)

### Hypothesis 1: Generated File Stale ⭐ **MOST LIKELY**
- `generated/kernel_verbs_init.c` last modified Dec 25 (4 days ago)
- Phase 3 changes may require regeneration
- **Test:** Regenerate and rebuild

### Hypothesis 2: tableinitverbs() Conflict
- New `tests/headless_table_verbs.c` defines `tableinitverbs()`
- May shadow or conflict with existing verb init
- **Test:** Check symbol table for duplicate definitions

### Hypothesis 3: Callback Registration Broken
- `newfunctionprocessor()` calls may have wrong signature
- Callback table may be corrupted
- **Test:** Add debug logging to verb registration

### Hypothesis 4: Initialization Order
- Table verb init may run at wrong time
- May corrupt existing verb processors
- **Test:** Check `kernel_verbs_init.c` initialization sequence

---

## Files Modified During Phase 3

| File | Purpose | Risk Level |
|------|---------|-----------|
| `Common/source/tableverbs.c` | Table verb implementations | Medium |
| `Common/source/langverbs.c` | Auto-set current table | **HIGH** |
| `tests/headless_table_verbs.c` | NEW - Callback dispatcher | **HIGH** |
| `Common/source/headless_selection.c` | Selection helpers | Low |
| `Common/source/langexternal.c` | Bug fix | Medium |
| `generated/kernel_verbs_init.c` | STALE (Dec 25) | **HIGH** |

**High-risk files:**
- `langverbs.c` - Modification may have side effects
- `headless_table_verbs.c` - New file, may conflict with existing init
- `kernel_verbs_init.c` - Stale generated file

---

## Prevention Strategy (Post-Fix)

### Immediate (This Session)
1. ✅ Create this planning document
2. ✅ Update _CURRENT_STATUS.md
3. ✅ Update _CURRENT_TODO_LIST.md
4. ⬜ Add git bisect script (`tools/bisect_verb_dispatch_test.sh`)
5. ⬜ Create pre-commit hook for verb sanity check
6. ⬜ Document findings in postmortem

### Short-Term (Before PR)
1. Add integration test suite for verb dispatch
2. Document verb registration architecture
3. Add CI test stage for verb sanity checks
4. Create developer checklist for verb changes

### Long-Term (Next Sprint)
1. Centralize verb registration (single source of truth)
2. Add type-safe callback registration macros
3. Auto-generate verification tests
4. Document initialization sequence dependencies

---

## Success Criteria

- [ ] Git bisect identifies exact breaking commit
- [ ] Root cause identified with evidence
- [ ] All verb families working (table, string, lang, math, etc.)
- [ ] Pre-commit hook prevents future breakage
- [ ] Postmortem document created
- [ ] Prevention strategy implemented

---

## Related Documents

- [TABLE_VERB_DISPATCH_POSTMORTEM.md](../../TABLE_VERB_DISPATCH_POSTMORTEM.md) - Previous dispatch investigation (incomplete/incorrect)
- [TABLE_VERBS_HEADLESS_SELECTION_MODEL.md](TABLE_VERBS_HEADLESS_SELECTION_MODEL.md) - Phase 3 design spec
- [_CURRENT_STATUS.md](../_CURRENT_STATUS.md) - Current project status
- [_CURRENT_TODO_LIST.md](../_CURRENT_TODO_LIST.md) - Active task tracking

---

## Notes for Future Sessions

**If picking this up later:**
1. Start with git bisect (Step 1 above)
2. Don't try to fix without identifying root cause
3. This is CRITICAL PATH - everything else is blocked
4. Test fix thoroughly across all verb families before proceeding

**Warning Signs to Watch:**
- Empty error messages from `langcallbacks.c:208`
- Basic arithmetic works but verbs fail
- Generated files out of sync with source
- Multiple definitions of initialization functions

---

**Last Updated:** 2025-12-29
**Next Action:** Run git bisect to find breaking commit
