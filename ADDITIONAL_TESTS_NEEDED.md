# Additional Integration Tests Needed

## Context
The fix prioritizes `builtins.PROCESSOR_NAME` over `langexternalgettable()` (which finds EFP stubs). This bypasses handle management issues but changes the resolution priority.

**Critical Question**: Does this break anything else?

---

## Test Categories

### 1. All Builtins Processors (Comprehensive Coverage)

**Purpose**: Verify the fix works for ALL processors in builtins, not just webserver and op.

**Processors to test** (from builtins table):
```yaml
tests:
  - name: "defined(target.set) - target processor"
    script: 'defined(target.set)'
    expected_success: true
    expected_result: "true"

  - name: "defined(table.assign) - table processor"
    script: 'defined(table.assign)'
    expected_success: true
    expected_result: "true"

  - name: "defined(string.length) - string processor"
    script: 'defined(string.length)'
    expected_success: true
    expected_result: "true"

  - name: "defined(file.exists) - file processor"
    script: 'defined(file.exists)'
    expected_success: true
    expected_result: "true"

  - name: "defined(db.open) - db processor"
    script: 'defined(db.open)'
    expected_success: true
    expected_result: "true"

  - name: "defined(xml.compile) - xml processor"
    script: 'defined(xml.compile)'
    expected_success: true
    expected_result: "true"

  - name: "defined(html.parseTag) - html processor"
    script: 'defined(html.parseTag)'
    expected_success: true
    expected_result: "true"

  - name: "defined(tcp.open) - tcp processor"
    script: 'defined(tcp.open)'
    expected_success: true
    expected_result: "true"
```

**Rationale**: The fix adds a lookup to builtins before every EFP lookup. Need to ensure this works consistently.

---

### 2. EFP-Only Processors (No Builtins Equivalent)

**Purpose**: Ensure processors that ONLY exist in EFP (not in builtins) still work.

**How to identify**: Look for processors in `system.compiler.kernel` that don't have builtins counterparts.

```yaml
tests:
  # Example: If "launch" or "sys" are EFP-only
  - name: "defined(launch.app) - EFP-only processor"
    script: 'defined(launch.app)'
    expected_success: true
    expected_result: "true"

  - name: "defined(sys.version) - EFP-only processor"
    script: 'defined(sys.version)'
    expected_success: true
    expected_result: "true"
```

**Rationale**: The fix checks builtins FIRST, then falls through to `langexternalgettable()`. Need to verify the fallthrough works.

---

### 3. Priority/Precedence Tests

**Purpose**: Verify that builtins tables take precedence over EFP stubs.

```yaml
tests:
  - name: "Table size - builtins.webserver vs EFP stub"
    description: |
      Verify we get the full builtins.webserver table (31 items),
      not the EFP stub (7 items).
    script: 'local(t=@webserver); return sizeOf(t)'
    expected_success: true
    expected_result: "31"  # Or actual size from builtins

  - name: "Table contents - builtins has 'init' script"
    description: |
      The EFP stub only has kernel verbs (dispatch, parseheaders, etc.).
      The builtins table has UserTalk scripts (init, data, etc.).
      Verify we're getting builtins.
    script: 'local(t=@webserver, i); for i=1 to sizeOf(t) {if nameOf(t[i])=="init" {return "found"}}; return "not found"'
    expected_success: true
    expected_result: "found"

  - name: "Verify EFP stub size for comparison"
    description: |
      Document what the EFP stub size is for reference.
      This should be the kernel verbs only (dispatch, etc.).
    script: 'local(t=@system.compiler.kernel.webserver); return sizeOf(t)'
    expected_success: true
    expected_result: "7"  # Or actual EFP stub size
```

**Rationale**: The core fix is about priority - need to explicitly verify the right table is being used.

---

### 4. Backwards Compatibility

**Purpose**: Ensure existing code patterns still work.

```yaml
tests:
  - name: "Direct builtins path still works"
    script: 'defined(builtins.webserver.init)'
    expected_success: true
    expected_result: "true"

  - name: "Full system path still works"
    script: 'defined(system.compiler.kernel.lang.new)'
    expected_success: true
    expected_result: "true"

  - name: "system.paths entries still resolve"
    script: 'defined(system.paths.webserver)'
    expected_success: true
    expected_result: "true"
```

---

### 5. Deep Nesting Tests

**Purpose**: Verify multi-level nested lookups work correctly.

```yaml
tests:
  - name: "3-level nested lookup"
    script: 'defined(webserver.data.responders)'
    expected_success: true
    expected_result: "true"  # If this path exists

  - name: "4-level nested lookup"
    script: 'defined(webserver.data.responders.admin)'
    expected_success: true
    expected_result: "true"  # If this path exists

  - name: "Deep nesting - final component doesn't exist"
    script: 'defined(webserver.data.nonexistent)'
    expected_success: true
    expected_result: "false"

  - name: "Deep nesting - intermediate component doesn't exist"
    script: 'defined(webserver.nonexistent.foo)'
    expected_success: false  # Should error, not just return false
```

**Rationale**: The fix only affects the first-level lookup. Need to ensure deeper nesting still works.

---

### 6. Mixed Type Resolution

**Purpose**: Verify different value types resolve correctly.

```yaml
tests:
  - name: "Resolve to script type"
    script: 'typeof(webserver.init)'
    expected_success: true
    expected_result: "scpt"

  - name: "Resolve to table type"
    script: 'typeof(webserver.data)'
    expected_success: true
    expected_result: "tabl"

  - name: "Resolve to string type (if exists)"
    script: 'typeof(webserver.version)'  # Example, if it exists as a string
    expected_success: true
    expected_result: "TEXT"

  - name: "Resolve to number type (if exists)"
    script: 'typeof(webserver.port)'  # Example, if it exists as a number
    expected_success: true
    expected_result: "long"
```

---

### 7. Case Sensitivity Tests

**Purpose**: Verify case-insensitive matching still works.

```yaml
tests:
  - name: "Case insensitive - lowercase"
    script: 'defined(webserver)'
    expected_success: true
    expected_result: "true"

  - name: "Case insensitive - uppercase"
    script: 'defined(WEBSERVER)'
    expected_success: true
    expected_result: "true"

  - name: "Case insensitive - mixed case"
    script: 'defined(WebServer)'
    expected_success: true
    expected_result: "true"

  - name: "Case insensitive - nested"
    script: 'defined(WEBSERVER.INIT)'
    expected_success: true
    expected_result: "true"
```

---

### 8. Error Cases

**Purpose**: Verify appropriate errors are raised.

```yaml
tests:
  - name: "Undefined processor at top level"
    script: 'defined(nonexistentprocessor)'
    expected_success: true
    expected_result: "false"

  - name: "Undefined child of valid processor"
    script: 'defined(webserver.nonexistent)'
    expected_success: true
    expected_result: "false"

  - name: "Accessing child of non-table"
    script: 'defined(webserver.init.foo)'
    expected_success: false
    expected_error_contains: "not a table"
```

---

### 9. Performance/Regression Tests

**Purpose**: Ensure the extra lookup doesn't cause performance issues.

```yaml
tests:
  - name: "Repeated lookups don't degrade"
    description: |
      Test that repeated lookups of the same processor don't cause
      performance degradation or memory leaks.
    script: |
      local(i);
      for i = 1 to 100 {
        if not defined(webserver.init) {
          return "failed at iteration " + string(i)
        }
      };
      return "ok"
    expected_success: true
    expected_result: "ok"

  - name: "Large script execution with many path lookups"
    description: |
      Simulate a realistic script that makes many different path lookups.
    script: |
      local(results = "");
      if defined(webserver.init) { results = results + "w" };
      if defined(op.firstSummit) { results = results + "o" };
      if defined(table.assign) { results = results + "t" };
      if defined(string.length) { results = results + "s" };
      if defined(file.exists) { results = results + "f" };
      return results
    expected_success: true
    expected_result: "wotsf"
```

---

## Implementation Priority

### P0 (Must Have Before Merge)
1. ✅ **Regression tests** - Already implemented (webserver.init, op.firstSummit)
2. **All builtins processors** - Test at least 5-10 major processors
3. **Priority/precedence** - Verify correct table is used
4. **Backwards compatibility** - Ensure existing patterns work

### P1 (Should Have)
4. **EFP-only processors** - Verify fallthrough works
5. **Deep nesting** - Test 3-4 level nesting
6. **Error cases** - Verify appropriate errors

### P2 (Nice to Have)
7. **Mixed types** - Different value types resolve correctly
8. **Case sensitivity** - Case-insensitive matching works
9. **Performance** - Repeated lookups don't degrade

---

## Recommended Test Implementation

Create a new test file: `tests/integration/test_cases/builtins_priority.yaml`

This keeps the fix-specific comprehensive tests separate from the general path_resolution tests.

Structure:
```yaml
# Builtins Priority Tests
# Tests for the fix that prioritizes builtins.PROCESSOR_NAME over EFP stubs
# See: INVESTIGATION_SUMMARY.md for context

tests:
  # Section 1: All Builtins Processors
  # ... 10-15 tests covering major processors

  # Section 2: Priority/Precedence
  # ... 5 tests verifying correct table size/contents

  # Section 3: EFP-Only Fallthrough
  # ... 3-5 tests for EFP-only processors

  # Section 4: Backwards Compatibility
  # ... 5 tests for existing patterns

  # Section 5: Deep Nesting
  # ... 5 tests for multi-level paths

  # Section 6: Error Cases
  # ... 5 tests for expected failures
```

Total: ~35-40 new tests focused specifically on the builtins priority fix.

---

## Quick Smoke Tests (Run Manually First)

Before implementing full test suite, manually verify:

```bash
# Major processors work
./frontier-cli/frontier-cli -e 'defined(target.set)'       # true
./frontier-cli/frontier-cli -e 'defined(table.assign)'     # true
./frontier-cli/frontier-cli -e 'defined(string.length)'    # true
./frontier-cli/frontier-cli -e 'defined(op.firstSummit)'   # true

# Priority works (get builtins, not EFP stub)
./frontier-cli/frontier-cli -e 'local(t=@webserver); sizeOf(t)'  # Should be 31, not 7

# Nested lookups work
./frontier-cli/frontier-cli -e 'defined(webserver.data)'          # true
./frontier-cli/frontier-cli -e 'defined(webserver.data.prefs)'    # true (if exists)

# Case insensitive
./frontier-cli/frontier-cli -e 'defined(WEBSERVER.INIT)'          # true

# Errors work
./frontier-cli/frontier-cli -e 'defined(nonexistent.foo)'         # false
```

If all smoke tests pass, proceed with full test implementation.
