# Verb Processor Implementation Plan - Sledge-Hammer Approach

## Status
- State: Implementation Plan
- Phase: 3
- Last Updated: 2025-12-21
- Notes: Plan for processor verb implementation


> 2025-12-08 Codex: This plan assumed full auto-registration from PR #60. That approach was rolled back after stability issues; headless currently relies on the curated `kernel_verbs_headless.c` registration list plus a few linked stubs. Treat the steps below as forward-looking guidance and refresh them after the next stable registration pass.

## Executive Summary

**Goal**: Implement all 51 verb processors in the headless environment to maximize visibility into what works, what breaks, and what needs special handling.

**Strategy**: Create stub implementations for all 49 remaining processors, compile them into the headless runtime, then systematically test and fix each verb to discover real incompatibilities versus implementation gaps.

**Expected Outcome**: Complete visibility into the headless verb processor landscape, enabling data-driven decisions about which features to prioritize.

---

## Phase Overview

### Phase 1: Generation & Compilation (Week 1)
Create all 49 processor stub files, update build system, verify compilation.

### Phase 2: Systematic Testing (Week 2-3)
Test each processor's verbs, document what works/breaks, categorize issues.

### Phase 3: Triage & Fix (Week 4+)
Fix issues, remove GUI-dependent features, implement missing functionality, iterate.

---

## Phase 1: Stub Generation & Compilation

### 1.1 Discover All Processors

First, extract the complete list of all 51 processors from kernelverbs.rc to understand what we're implementing:

```bash
python3 tools/kernelverbs_parser/parse_kernelverbs.py \
    Common/resources/Win32/kernelverbs.rc \
    /tmp/all_processors.txt
```

This will list all 51 processors with their verb counts and EFP IDs.

### 1.2 Create Stub Files for 49 Remaining Processors

For each processor not yet implemented, create a file `tests/headless_<processor>_verbs.c` following this template:

```c
#include "frontier.h"
#include "standard.h"
#include "lang.h"
#include "langinternal.h"

/* Token enum for all verbs in this processor */
enum {
    pv_verb1 = 0,
    pv_verb2 = 1,
    /* ... all verb_count tokens ... */
};

static boolean <processor>_valueproc(short token, hdltreenode hparam1,
                                     tyvaluerecord *vreturned,
                                     bigstring bserror) {
    switch(token) {
        case pv_verb1:
            // TODO: Implement or call original code
            return false;  // Mark as not implemented
        case pv_verb2:
            // TODO: Implement or call original code
            return false;
        default:
            return false;
    }
}

boolean <processor>initverbs(void) {
    hdlhashtable htable = nil;
    bigstring bsname;

    copystring(BIGSTRING("\\p<processor>"), bsname);

    if (!newfunctionprocessor(bsname, &<processor>_valueproc, false, &htable))
        return false;

    pushhashtable(htable);

    #define ADD_VERB(name, tok) do { \
        bigstring bs; \
        copystring(name, bs); \
        if (!langaddkeyword(bs, tok)) { \
            pophashtable(); \
            return false; \
        } \
    } while(0)

    // Register all verbs (in order from kernelverbs.rc)
    ADD_VERB(BIGSTRING("\\pverb1"), pv_verb1);
    ADD_VERB(BIGSTRING("\\pverb2"), pv_verb2);
    /* ... add all verbs ... */

    #undef ADD_VERB

    pophashtable();
    return true;
}
```

**Process for each processor**:
1. Extract verb names and count from kernelverbs.rc (can script this)
2. Generate enum with tokens 0 to (verb_count - 1)
3. Generate switch cases for all tokens (all returning false initially)
4. Generate ADD_VERB calls for all verbs
5. Save as tests/headless_<processor>_verbs.c

**Automation**: Write a Python script to generate all 49 files at once by parsing kernelverbs.rc.

### 1.3 Update HEADLESS_IMPLEMENTED Whitelist

Edit `tools/kernelverbs_parser/parse_kernelverbs.py` to add all 51 processors to the whitelist.

### 1.4 Update Makefile

Add all 49 new processor files to HEADLESS_STUBS in `frontier-cli/Makefile`.

### 1.5 Verify Compilation

```bash
cd frontier-cli
make clean
make
```

**Expected result**: Clean compilation with all 51 processors initialized at startup.

---

## Phase 2: Systematic Testing & Discovery

### 2.1 Test Infrastructure

Create test harness `tests/test_all_verbs.c` that:
1. Initializes all verb processors
2. For each processor and each verb, attempts a minimal call
3. Records success/failure/error message
4. Generates a report of results

Alternative: Write `tests/verb_processor_test.sh` (shell script) that:
1. Creates UserTalk test scripts for each verb
2. Runs them through the headless frontier-cli
3. Captures success/failure
4. Generates categorized report

### 2.2 Testing Strategy

For each verb, test:
- **Basic Call**: Does the verb initialize and accept a minimal call without crashing?
- **Parameter Types**: Does it handle expected parameter types?
- **Return Value**: Does it return the expected type?
- **Error Cases**: Does it handle invalid inputs gracefully?

### 2.3 Test Execution & Reporting

Generate summary report:
- **Processors by Status**: Working / Partially Working / Broken / Incompatible
- **Verbs by Category**: GUI-dependent / Logic-only / Missing implementation / Broken
- **Common Failure Patterns**: Missing dependencies, platform-specific code, GUI requirements

---

## Phase 3: Triage & Fix

### 3.1 Issue Categorization

For each broken verb, categorize:

1. **GUI-Dependent** (cannot fix)
   - Requires window/dialog features
   - Needs display/graphics functionality

2. **Missing Implementation** (can fix)
   - Logic is simple, just not implemented
   - Needs to call into original C code

3. **Platform-Specific** (needs adaptation)
   - Windows-only code needs macOS/Linux version
   - Path handling differences

4. **Dependency Chain** (may be fixable)
   - Requires other verbs that don't work
   - Might work if dependencies are fixed first

### 3.2 Priority Implementation Order

**High Priority** (enable most UserTalk scripts):
1. string (60 verbs) - Core language feature
2. lang (58 verbs) - Runtime operations
3. date (30 verbs) - Time/date handling
4. table (18 verbs) - Data structures

**Medium Priority** (enable advanced scripts):
5. database (varies) - Data persistence
6. file (already 12/86, complete it)
7. frontier (already 5/14, complete it)

**Low Priority / GUI-Dependent** (may be incompatible):
8. dialog (19 verbs) - User interaction
9. target (varies) - Window management
10. menu (varies) - Menu operations

### 3.3 Fix Strategy per Category

**For Missing Implementations**:
1. Look up verb specification in original Frontier docs
2. Implement the logic in C
3. Test with unit test case
4. Mark as working in test report

**For Platform-Specific Code**:
1. Identify the platform-specific section
2. Add #ifdef guards for macOS/Linux
3. Implement platform-specific version
4. Test on all platforms
5. Mark as working

**For GUI-Dependent Features**:
1. Document as incompatible
2. Make it return clear error: "Feature not available in headless mode"
3. Skip in test suite
4. Mark as "headless-incompatible" for future reference

**For Dependency Chains**:
1. Implement dependencies first
2. Then implement dependent verbs
3. Test end-to-end

### 3.4 Iterative Improvement

1. Fix high-priority verbs first
2. Re-run test suite
3. Generate new report
4. Fix next batch
5. Repeat until reaching acceptable coverage

---

## Success Criteria

### Phase 1 Success
- [ ] All 49 stub files created and committed
- [ ] Whitelist updated with all 51 processors
- [ ] Makefile includes all 49 new files
- [ ] `make clean && make` completes without errors
- [ ] frontier-cli starts successfully with all processors initialized
- [ ] `headless_init_kernel_verbs()` returns true

### Phase 2 Success
- [ ] Test harness created and working
- [ ] All 51 processors have test coverage
- [ ] Test results categorized (working / broken / incompatible)
- [ ] Report generated showing current state

### Phase 3 Success
- [ ] High-priority processors at 80%+ verb coverage
- [ ] Medium-priority processors at 50%+ verb coverage
- [ ] GUI-incompatible verbs documented
- [ ] Real UserTalk scripts execute without verb-not-found errors

---

## Risks & Mitigations

| Risk | Mitigation |
|------|-----------|
| Compilation errors from stub files | Careful code generation; verify on each processor |
| Incompatible verbs break headless runtime | Test in isolation; graceful failure handling |
| Too many verbs to realistically implement | Focus on high-impact verbs; accept "not implemented" as acceptable end state for others |
| Testing framework overhead | Use simple shell scripts + UserTalk tests, not complex C harness |
| GUI features cause runtime crashes | Return error message instead of attempting GUI call |

---

## Next Steps

1. **Generate verb list**: Run parser to get all processor names and verb counts
2. **Code generator**: Write script to generate all 49 stub files
3. **Build verification**: Compile and test
4. **Test harness**: Create testing infrastructure
5. **Systematic testing**: Run tests and generate report
6. **Iterative fixes**: Begin with highest-impact processors
