# Issue #136 Execution Plan: Eliminate Push/Pop Anti-Patterns in External Object Loading

**Created**: 2025-12-24
**Status**: Ready for Execution
**Model**: Haiku 4.5
**Effort**: 6-8 hours (5 phases)
**Risk Level**: Low (pattern validated on production databases)

---

## Executive Summary

Issue #136 addresses the need to audit and refactor external object loading code to eliminate scattered `db_format_mode_push/pop()` and `db_context_apply()` calls that create implicit global state and cascading failures. The architectural documentation has been completed and the v6→v7 migration work (PR #162) has established the **single decision point pattern** where mode management happens exclusively in `langexternalpack_internal()`.

**Current State**: Progress is substantial but incomplete. The core architecture is in place:
- `langexternalpack_internal()` has been refactored to be the single decision point
- `ensure_external_in_memory()` type dispatcher exists and calls context-aware loaders
- Five `*verbpack_internal()` functions exist and accept `const db_context *ctx` parameter
- **Critical Gap**: `menuverbpack_internal()` does not exist; menuverbpack still uses isolated push/pop

**The Solution**: Complete the refactoring by extracting `menuverbpack_internal()` following the exact pattern of `opverbpack_internal()` (the gold standard implementation).

**Scope**: Single external object type (menus) across one source file. Refactoring is surgical and low-risk because the pattern has been validated on real databases and 4 identical examples already exist.

---

## Agent Assessment & Execution Strategy

### System Architect Recommendation

**Overall**: Direct implementation with Haiku 4.5 for all phases.

**Rationale**:
- Pattern is well-established (4 identical examples: opverbpack_internal, wpverbpack_internal, tableverbpack_internal, pictverbpack_internal)
- Detailed execution plan with step-by-step guidance
- Single file, isolated scope (menuverbs.c only)
- Low-risk structural refactoring (no behavior change)
- Clear success criteria and test validation

**Confidence Level**: 90% direct implementation will succeed without agent escalation.

**Escalation Points**:
- Phase 1: If menuverbpack() reveals unexpected complexity → escalate to refactoring-consultant
- Phase 4: If tests fail unexpectedly → escalate to frontier-sdet
- Otherwise: Proceed directly

---

## Phase Breakdown (5 Phases)

### PHASE 0: Preconditions & Validation (30 min)

**Goal**: Verify environment is ready for refactoring

**0.1 - Create Feature Branch**
- [ ] Verify on `develop` branch: `git status`
- [ ] Pull latest: `git pull origin develop`
- [ ] Create feature branch: `git checkout -b refactor/menuverbpack-single-decision-point`

**0.2 - Establish Baseline**
- [ ] Run full test suite: `./tools/run_headless_tests.sh`
- [ ] Expected: All tests pass, zero failures
- [ ] Capture baseline output for comparison

**0.3 - Verify Code State**
- [ ] Confirm menuverbs.c has menuverbpack() function (lines ~362-445)
- [ ] Verify opverbpack_internal() exists in opverbs.c (reference pattern)
- [ ] Confirm langexternal.c calls menuverbpack_internal() (verification step in Phase 2)

**Success Criteria**:
- Feature branch created and checked out
- Baseline test suite passes (clean state)
- Code locations identified and verified

**Testing**: `./tools/run_headless_tests.sh`
- Expected: All tests pass ✓

---

### PHASE 1: Extract menuverbpack_internal Function (2-3 hours)

**Goal**: Create context-aware `menuverbpack_internal()` function following exact pattern of opverbpack_internal()

**1.1 - Analyze Pattern Reference**
- [ ] Read `Common/source/opverbs.c` lines 794-914 (opverbpack_internal)
- [ ] Study function signature, context application, error handling
- [ ] Note: Function applies context->mode at start, NO push/pop calls anywhere

**1.2 - Extract menuverbpack_internal()**
- [ ] Create function signature:
  ```c
  boolean menuverbpack_internal (const db_context *ctx, hdlexternalvariable h,
                                  Handle *hpacked, boolean *flnewdbaddress)
  ```
- [ ] Copy function body from menuverbpack() (lines 391-441)
- [ ] Add precondition comments matching opverbpack_internal style
- [ ] Apply context at function start (following opverbpack_internal pattern lines 822-826):
  ```c
  if (ctx != NULL) {
      if (ctx->database != nil)
          databasedata = ctx->database;
      db_format_mode_apply(&ctx->mode);
  }
  ```
- [ ] Add precondition assertion:
  ```c
  if (!(**hv).flinmemory) {
      /* This is a programming error - caller should have loaded it */
      return (false);
  }
  ```
- [ ] Remove all `db_format_mode_push()`, `db_format_mode_pop()`, and isolated `db_format_mode_apply()` calls
- [ ] Verify NO push/pop calls remain in extracted function

**1.3 - Update Backward-Compatibility Wrapper**
- [ ] Modify existing menuverbpack() to call new function:
  ```c
  boolean menuverbpack (hdlexternalvariable h, Handle *hpacked, boolean *flnewdbaddress) {
      /* Wrapper for backward compatibility - uses global mode state */
      return menuverbpack_internal (NULL, h, hpacked, flnewdbaddress);
  }
  ```
- [ ] Add comment explaining wrapper purpose
- [ ] Order functions: menuverbpack_internal first, wrapper after

**1.4 - Verify Pattern Compliance**
- [ ] Compare extracted function side-by-side with opverbpack_internal()
- [ ] Checklist:
  - [ ] Function signature matches pattern: `boolean function_internal(const db_context *ctx, ...)`
  - [ ] Context applied exactly once at function start
  - [ ] NO db_format_mode_push/pop calls anywhere
  - [ ] NO db_context_apply calls (except initial context application)
  - [ ] Precondition assertion for flinmemory present
  - [ ] Comments and documentation clear

**Success Criteria**:
- menuverbpack_internal() function created and compiles
- Function signature matches opverbpack_internal pattern exactly
- Zero push/pop calls in extracted function
- Backward-compatible wrapper in place
- Pattern compliance validated

**Testing**: After editing, run tests:
- `make -C Common` (compilation check)
- `./tools/run_headless_tests.sh` (functional validation)
- Expected: All tests pass, mode changes isolated to langexternalpack_internal()

---

### PHASE 2: Verify Dispatcher Integration (30 min)

**Goal**: Confirm langexternalpack_internal() already calls menuverbpack_internal() correctly

**2.1 - Verify Call Site**
- [ ] Read `Common/source/langexternal.c` line ~901
- [ ] Confirm calls: `menuverbpack_internal(&working_context, hv, hpacked, flnewdbaddress)`
- [ ] Verify context parameter is passed: `&working_context`

**2.2 - Verify Single Decision Point**
- [ ] Confirm langexternalpack_internal() is THE ONLY place calling `db_context_apply()`
- [ ] Check mode transitions at lines 857-878:
  - Set v6 read mode for loading
  - Switch to v7 write mode for packing
  - NO intermediate mode changes
- [ ] Verify all six external types dispatched with explicit context:
  - opverbpack_internal() ✓
  - wpverbpack_internal() ✓
  - tableverbpack_internal() ✓
  - menuverbpack_internal() ✓
  - pictverbpack_internal() ✓

**2.3 - Anti-Pattern Audit**
- [ ] Run: `grep -n "db_format_mode_push\|db_format_mode_pop" Common/source/langexternal.c`
- [ ] Expected: ZERO matches (single decision point pattern)

**Success Criteria**:
- langexternalpack_internal() correctly calls menuverbpack_internal()
- All six types receive explicit context parameter
- Single decision point pattern verified intact

**Testing**: No code changes, verification only
- `./tools/run_headless_tests.sh`
- Expected: All tests pass (no changes to test logic)

---

### PHASE 3: Compile & Link Validation (45 min)

**Goal**: Ensure new code compiles and links correctly

**3.1 - Compilation Check**
- [ ] Run: `make -C Common`
- [ ] Check for compilation errors: should be zero
- [ ] Check for compilation warnings related to menuverbs.c
- [ ] Verify function signature is type-correct

**3.2 - Link Validation**
- [ ] Run: `make -C tests clean && make -C tests test`
- [ ] Check for link errors involving menuverbpack_internal
- [ ] Confirm all test binaries link successfully
- [ ] Verify symbol resolution is correct

**Success Criteria**:
- `make -C Common` completes with zero errors
- `make -C tests test` completes with zero link errors
- All test binaries created successfully

**Testing**: Build system validation
- `make -C Common` (library build)
- `make -C tests test` (test binary build)
- Expected: Clean compilation and linking

---

### PHASE 4: Unit Testing & Validation (2-3 hours)

**Goal**: Validate external object packing works correctly with new architecture

**4.1 - Migration Test**
- [ ] Run: `make -C tests clean && make -C tests save_migration_tests`
- [ ] Execute: `./tests/save_migration_tests`
- [ ] Expected output:
  - Phase 1 (Load v6): PASS
  - Phase 2 (Migrate): PASS
  - Phase 3 (Validate format): PASS
  - Phase 4 (External access): PASS
  - Overall: ALL VALIDATIONS PASSED

**4.2 - Headless Test Suite**
- [ ] Run: `./tools/run_headless_tests.sh`
- [ ] Expected: All tests pass, zero regressions
- [ ] Check logs for mode management cleanliness:
  - Should see mode changes ONLY in langexternalpack_internal()
  - Should NOT see repeated mode flip-flopping

**4.3 - Menu-Specific Verification**
- [ ] Verify migration logs show menuverbpack_internal called (search logs)
- [ ] Confirm no push/pop calls in menuverbpack_internal path
- [ ] Verify context applied exactly once per menu external
- [ ] Check for any memory corruption or double-free issues

**Success Criteria**:
- All migration test phases pass (4/4)
- v6→v7 database valid and accessible
- Full headless test suite passes (zero regressions)
- Mode management isolated to single decision point
- No memory issues or corruption

**Testing**: Comprehensive integration testing
- Phase 4.1: Migration test
- Phase 4.2: Full headless test suite
- Phase 4.3: Check logs for correctness
- Expected: 100% pass rate, zero failures

**Re-run at End**: Before proceeding to PR, re-run both:
- `./tests/save_migration_tests`
- `./tools/run_headless_tests.sh`
- Expected: Same passing state

---

### PHASE 5: Anti-Pattern Elimination Audit (Moved to PR Review)

**Note**: Phase 5 pattern validation moved to PR bot code review. The system architect and user feedback determined that:
- PR bot provides comprehensive code review feedback
- Phase 4 testing validates correctness
- Additional manual validation is redundant
- Focus on getting to PR for bot feedback

---

### PHASE 6: Git Workflow & Commit (30-45 min)

**Goal**: Prepare clean commit for feature branch

**6.1 - Verify Changes**
- [ ] Run: `git status`
- [ ] Expected: Only `Common/source/menuverbs.c` modified
- [ ] No extraneous changes

**6.2 - Review Changes**
- [ ] Run: `git diff Common/source/menuverbs.c`
- [ ] Visually verify:
  - menuverbpack_internal() function added
  - menuverbpack() wrapper updated
  - No unintended modifications

**6.3 - Stage Changes**
- [ ] Run: `git add Common/source/menuverbs.c`
- [ ] Verify: `git status` shows file staged

**6.4 - Create Commit**
- [ ] Commit message (see template below):
  ```
  fix(issue-136): Extract menuverbpack_internal for single decision point pattern

  Create context-aware menuverbpack_internal() function to eliminate
  scattered db_format_mode_push/pop calls in type-specific packing code.
  Follows identical pattern as opverbpack_internal, wpverbpack_internal,
  tableverbpack_internal, and pictverbpack_internal.

  This refactoring completes issue #136 audit of external object processing
  and eliminates the anti-pattern where mode management was scattered across
  individual verb packing functions.

  Changes:
  - Extract menuverbpack_internal(const db_context *ctx, ...) function
  - Apply context->mode at function start, NO push/pop calls
  - Update menuverbpack() wrapper for backward compatibility
  - All six external types now follow single decision point pattern

  Validation:
  - Compilation: clean, no new warnings
  - Migration tests: all 4 phases pass
  - Headless tests: full suite passes, zero regressions
  - Anti-pattern audit: grep confirms zero push/pop in verb functions

  References:
  - planning/architectural_decision_records/external-object-loading-architecture.md
  - planning/architectural_decision_records/mode_management_single_decision_point.md
  - Issue #136
  ```

**Success Criteria**:
- Feature branch has one clean, atomic commit
- Commit message references issue #136 and planning docs
- Commit is ready to push

**Testing**: Final verification before push
- `./tests/save_migration_tests` (quick validation)
- `./tools/run_headless_tests.sh` (comprehensive)
- Expected: Same passing state as Phase 4

---

## File Modification Summary

| File | Changes | Lines Affected | Status |
|------|---------|-----------------|--------|
| Common/source/menuverbs.c | Extract menuverbpack_internal(), update menuverbpack() wrapper | ~100-150 | **MODIFY** |
| Common/source/langexternal.c | None (already correct) | N/A | **VERIFY ONLY** |
| Common/source/opverbs.c | None (reference pattern) | N/A | **REFERENCE** |
| Common/source/wpverbs.c | None (reference pattern) | N/A | **REFERENCE** |
| Common/source/tablepack.c | None (reference pattern) | N/A | **REFERENCE** |
| Common/source/pictverbs.c | None (reference pattern) | N/A | **REFERENCE** |

---

## Before/After Code Pattern

### BEFORE (Anti-Pattern: Push/Pop in Type-Specific Function)

```c
// Common/source/menuverbs.c (lines 362-445)
boolean menuverbpack (hdlexternalvariable hvariable, Handle *hpacked, boolean *flnewdbaddress) {
    register hdlmenuvariable hv = (hdlmenuvariable) hvariable;
    // ...

    if (adapter_repack) {
        (*flnewdbaddress) = true;
        (**hm).fldirty = true;
        // ❌ WRONG: Push mode here (type-specific function)
        db_format_mode_push(&working_mode);    // ← ANTI-PATTERN
    }

    fl = mesavemenurecord(hm, ...);

    // ❌ WRONG: Pop mode here (violates single decision point)
    if (adapter_repack)
        db_format_mode_pop();           // ← ANTI-PATTERN

    return (pushlongondiskhandle(adr, *hpacked));
}
```

### AFTER (Correct Pattern: Context Passing, No Mode Management in Type-Specific Function)

```c
// Common/source/menuverbs.c (new menuverbpack_internal function)
boolean menuverbpack_internal (const db_context *ctx, hdlexternalvariable h,
                                Handle *hpacked, boolean *flnewdbaddress) {
    /*
    Pure packing function with explicit context

    Preconditions:
      - flinmemory=1 (caller has loaded external into memory)
      - ctx specifies the output format mode

    Postconditions:
      - Menu packed and address written to *hpacked
      - Returns true on success, false on failure
    */

    register hdlmenuvariable hv = (hdlmenuvariable) h;
    register hdlmenurecord hm;
    dbaddress adr;

    /* ✓ CORRECT: Apply context once at start, NO mode management in function */
    if (ctx != NULL) {
        if (ctx->database != nil)
            databasedata = ctx->database;
        db_format_mode_apply(&ctx->mode);
    }

    adapter_repack = db_format_adapter_force_repack();

    /* Precondition: external must be in memory */
    if (!(**hv).flinmemory) {
        return (false);
    }

    adr = (**hv).oldaddress;
    hm = (hdlmenurecord) (**hv).variabledata;

    if (adapter_repack) {
        (*flnewdbaddress) = true;
        (**hm).fldirty = true;
        /* Mode already set by caller via db_format_mode_apply above - no push/pop! */
    }

    fl = mesavemenurecord(hm, ...);

    // ... rest of function ...

    return (pushlongondiskhandle(adr, *hpacked));
}

// Backward-compatible wrapper (updated)
boolean menuverbpack (hdlexternalvariable h, Handle *hpacked, boolean *flnewdbaddress) {
    /* Wrapper for backward compatibility - uses global mode state */
    return menuverbpack_internal (NULL, h, hpacked, flnewdbaddress);
}
```

---

## Testing Validation Strategy

### After Each Phase: Run Tests

**Phase 0-1 Testing**:
```bash
./tools/run_headless_tests.sh
```
Expected: All tests pass

**Phase 3-4 Testing**:
```bash
make -C tests clean && make -C tests save_migration_tests
./tests/save_migration_tests
./tools/run_headless_tests.sh
```
Expected:
- Migration: all 4 phases pass
- Headless: all tests pass
- Zero regressions

### Before PR: Final Comprehensive Test

```bash
# Run both migration AND headless tests
./tests/save_migration_tests
./tools/run_headless_tests.sh
```
Expected:
- Migration test: ALL VALIDATIONS PASSED
- Headless test: All tests pass, zero regressions

---

## Risk Assessment

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|-----------|
| Function signature mismatch | Low | Medium | Compare carefully with opverbpack_internal |
| Context parameter not applied | Medium | Medium | Tests validate context mode handling |
| Backward compatibility broken | Low | High | Wrapper function ensures old callers work |
| Database corruption during migration | Low | Critical | Comprehensive test suite covers this |
| Push/pop calls not fully removed | Medium | Medium | Final grep audit before PR |
| Unintended file modifications | Low | Medium | Review `git diff` carefully |

---

## Success Criteria Checklist

### Code Quality
- [ ] menuverbpack_internal() compiles without errors
- [ ] Function signature matches opverbpack_internal pattern
- [ ] Zero db_format_mode_push/pop calls in extracted function
- [ ] Zero db_context_apply calls (except initial application)
- [ ] Precondition assertion for flinmemory present
- [ ] Code is well-documented with clear comments

### Testing
- [ ] Compilation: `make -C Common` clean
- [ ] Linking: `make -C tests test` successful
- [ ] Migration: all 4 phases pass
- [ ] Headless: all tests pass
- [ ] Regression: zero new failures

### Architectural Compliance
- [ ] Single decision point pattern intact
- [ ] All six external types follow identical pattern
- [ ] Mode management only in langexternalpack_internal()
- [ ] Explicit context passing throughout

### Git Quality
- [ ] Feature branch created with descriptive name
- [ ] Only menuverbs.c modified
- [ ] One atomic commit with clear message
- [ ] Commit references issue #136 and planning docs

---

## Model & Execution Notes

**Model**: Haiku 4.5 (cost-effective, pattern-following tasks)

**Confidence**: 90% direct execution will succeed without escalation

**Escalation Policy**:
- Phase 1 unexpected complexity → escalate to refactoring-consultant
- Phase 4 test failure → escalate to frontier-sdet
- Otherwise → continue with direct execution

**Branch Name**: `refactor/menuverbpack-single-decision-point`
- Describes what's being done (menuverbpack refactor)
- References architectural principle (single-decision-point)
- Distinct from just "issue-136"

---

## Summary

This execution plan refactors one external object type (menus) to complete the single decision point architectural pattern for all external object packing. The refactoring is:

- **Low-risk**: Pattern validated on 4 identical examples
- **Isolated**: Single file (menuverbs.c), no cross-cutting changes
- **Well-tested**: Comprehensive test suite covers all code paths
- **Well-documented**: Clear before/after patterns and step-by-step guidance

Total estimated effort: **6-8 hours**

Ready to execute.

---

**Document Status**: Ready for execution
**Last Updated**: 2025-12-24
**Next Step**: Create feature branch and begin Phase 0
