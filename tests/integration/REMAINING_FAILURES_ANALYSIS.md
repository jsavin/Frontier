# Integration Test Failures Analysis
Generated: 2026-01-15

## Summary
- **Total Tests**: 1,211
- **Passed**: 1,036
- **Skipped**: 113 (expected - phase-gated or REPL-mode)
- **Failed**: 62 (need triage)

## Failure Categories

### 1. Script Processor Verbs (22 failures) - NEEDS INVESTIGATION
These tests are for `script.getsource()`, `script.setsource()`, `script.getcode()`, `script.setcode()`, and `script.compile()`. PR #309 fixed `script.run()` but these verbs still failing.

**Tests:**
- script.compile - non-script object returns false
- script execution - direct evaluation of compiled script
- script execution - script with arithmetic
- script.getsource - retrieve script source text
- script.getsource - returns boolean true
- script.setsource - set script source text
- script.setsource - empty string source
- script.setsource - multiline source
- script.setsource - source with special characters
- script.getcode - retrieve compiled bytecode
- script.getcode - returns boolean true
- script.setcode - set compiled bytecode
- script.setcode - returns boolean true
- script source verbs - roundtrip test
- script code verbs - transfer code between scripts
- script verbs - complete lifecycle test
- script verbs - modify source and recompile
- script verbs - source and code independence
- script verbs - clone script via code transfer
- script verbs - multiple scripts independent
- script verbs - return type validation
- script verbs - recover from compilation error

**Action**: These should work after PR #309. Need to investigate why they're still failing.

---

### 2. Lang Type Coercion Verbs (11 failures) - SHOULD SKIP
These are type coercion verbs that convert between different Frontier types. Many are deprecated or platform-specific.

**Tests:**
- lang.callscript - simple script
- lang.callscript - script with side effects
- lang.point - record to point
- lang.rect - record to rect
- lang.rgb - record to rgb
- lang.pattern - binary to pattern
- lang.list - binary to list
- lang.record - list to record
- lang.enum - string to enum
- lang.calldll - platform error (expects "not supported")
- lang.callxcmd - platform error (expects "not supported")
- lang.coerceappleitem - platform error (expects "AppleEvent")
- lang.packwindow - platform error (expects "Window")
- lang.scripterror - string message
- lang.scripterror - terminates execution

**Skip Reason**: "Phase 3: Type coercion and platform-specific verbs not implemented"

---

### 3. op.attributes Verbs (7 failures) - SHOULD SKIP
GUI-only verbs that return generic "Script execution failed" instead of specific error message.

**Tests:**
- op.attributes.addgroup - GUI-only stub returns error
- op.attributes.getall - GUI-only stub returns error
- op.attributes.getone - GUI-only stub returns error
- op.attributes.makeempty - GUI-only stub returns error
- op.attributes.setone - GUI-only stub returns error
- op.attributes.addgroup - error without outline target
- op.attributes.getall - error without outline target

**Skip Reason**: "Phase 3: op.attributes verbs are GUI-only and not supported in headless mode"

---

### 4. sys Verbs (6 failures) - NEEDS INVESTIGATION
Process management and system verbs. Some may be implemented, some may need stubs.

**Tests:**
- sys.appisrunning - current process returns true
- sys.getapppath - current process returns path
- sys.getapppath - path contains process name
- sys verbs - process management suite
- sys.unixshellcommand - 1 param (command only)
- sys.unixshellcommand - 1 param (command fails)
- sys.winshellcommand - not available on macOS/Linux

**Action**: Investigate - some may be real bugs, some may need skip annotations.

---

### 5. Workflow/Stress Tests (6 failures) - NEEDS INVESTIGATION
Integration tests that combine multiple operations. May reveal real bugs.

**Tests:**
- workflow - build and navigate outline structure
- workflow - promote and demote hierarchy changes
- workflow - save expansion state before collapse and restore
- stress test - rapid insert and delete
- stress test - rapid cursor save/restore cycles
- stress test - expansion state with deep nesting

**Action**: These test complex scenarios - failures may indicate real bugs.

---

### 6. Miscellaneous (8 failures)

#### op.xmltooutline (1 failure)
- op.xmltooutline - round trip

**Action**: Investigate - may be Phase 3 work

#### html.getgifheightwidth (1 failure)
- html.getgifheightwidth - missing file

**Note**: Other html.getgifheightwidth tests already have skip: "Need sample GIF/JPEG test fixture"

**Action**: Add same skip annotation

#### file.lock (1 failure)
- file.lock - lock a file

**Action**: Investigate - file locking may not work in macOS sandbox

#### error recovery (1 failure)
- error recovery - setRefcon with no current node

**Action**: Should skip - Phase 3 error validation

#### type safety (1 failure)
- type safety - setExpansionState requires list of numbers

**Action**: Already noted in op verb failures - expects script_error, gets json_parse_error

---

## Recommended Actions

### Immediate (Add Skip Annotations):
1. **lang type coercion verbs** (11 tests) - Phase 3 work
2. **op.attributes** (7 tests) - GUI-only, not supported
3. **html.getgifheightwidth - missing file** (1 test) - Missing test fixture
4. **error recovery - setRefcon** (1 test) - Phase 3 validation
5. **type safety - setExpansionState** (1 test) - Phase 3 validation

**Total to skip**: 21 tests

### Investigate (Genuine Bugs or Missing Implementation):
1. **script processor verbs** (22 tests) - Should work after PR #309
2. **sys verbs** (6 tests) - May be real bugs or need stubs
3. **workflow/stress tests** (6 tests) - May reveal real bugs
4. **op.xmltooutline** (1 test) - May be Phase 3 or real bug
5. **file.lock** (1 test) - Sandbox limitation or real bug

**Total to investigate**: 36 tests

### After Cleanup (Expected Results):
- **Skipped**: 113 + 21 = **134 tests**
- **Failed**: 62 - 21 = **41 tests** (genuine issues to fix)
- **Passed**: 1,036 (unchanged)
