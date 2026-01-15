# Integration Test Framework Design for UserTalk Verb Bindings

**Status**: Design Complete, Ready for Review
**Created**: 2025-12-31
**Author**: system-architect agent
**Purpose**: Enable systematic testing of UserTalk verb bindings (string, table, file operations)

---

## Executive Summary

I'm proposing a **two-phase architecture** that starts simple (Phase 1: one-shot execution) while enabling future evolution to a persistent process model (Phase 2). The framework uses **Python-based test harness** with structured JSON output from frontier-cli, making it easy to verify complex return values and distinguish success/error cases.

**Key Decision**: Use Python for test harness (not C, not pure shell) because:
- Rich string/JSON/XML manipulation capabilities
- Easy to write expressive test cases
- Can evolve to IPC client when moving to persistent process
- Faster development iteration than C tests

---

## 1. Architecture Proposal

### Phase 1: One-Shot Execution (Implement Now)

```
┌─────────────────────────────────────────────────────────┐
│ Python Test Harness (tests/integration/runner.py)      │
│  - Reads test cases from YAML/JSON files               │
│  - Spawns frontier-cli -e "<script>" for each test     │
│  - Parses JSON output from frontier-cli                │
│  - Verifies return values and error conditions         │
└─────────────────────────────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────┐
│ frontier-cli --headless -e "<UserTalk script>"         │
│  - Executes script in headless mode                     │
│  - Returns structured JSON to stdout:                   │
│    {                                                     │
│      "success": true/false,                             │
│      "result": "serialized return value",               │
│      "error": "scriptError message if failed",          │
│      "exit_code": 0                                     │
│    }                                                     │
└─────────────────────────────────────────────────────────┘
```

### Phase 2: Persistent Process (Future Evolution)

```
┌─────────────────────────────────────────────────────────┐
│ Python Test Harness                                      │
│  - Starts frontier-cli --server mode once               │
│  - Sends commands over stdin/socket                     │
│  - Receives JSON responses                              │
│  - Maintains session state between tests                │
└─────────────────────────────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────┐
│ frontier-cli --server (persistent process)              │
│  - Listens for commands on stdin/socket                 │
│  - Maintains UserTalk runtime state                     │
│  - Returns JSON responses                               │
│  - Supports session management commands                 │
└─────────────────────────────────────────────────────────┘
```

**Migration Path**: Same Python test harness, swap execution backend from `subprocess.run()` to persistent client.

---

## 2. Output Format Design

### frontier-cli JSON Output Format

```json
{
  "success": true,
  "result": "5",
  "result_type": "long",
  "error": null,
  "exit_code": 0
}
```

**Error case (scriptError):**
```json
{
  "success": false,
  "result": null,
  "result_type": null,
  "error": "Can't evaluate the object because its name hasn't been defined yet (string)",
  "error_type": "script_error",
  "exit_code": 1
}
```

**Script returns false (not an error):**
```json
{
  "success": true,
  "result": "false",
  "result_type": "boolean",
  "error": null,
  "exit_code": 0
}
```

**C-level crash:**
```json
{
  "success": false,
  "result": null,
  "error": "Segmentation fault (core dumped)",
  "error_type": "crash",
  "exit_code": 139
}
```

**Assertion failure:**
```json
{
  "success": false,
  "result": null,
  "error": "Assertion failed: (ptr != NULL), function foo, file bar.c, line 42",
  "error_type": "assertion",
  "exit_code": 134
}
```

**Table/structured result case:**
```json
{
  "success": true,
  "result": "<?xml version=\"1.0\"?>\n<table>\n  <name>test</name>\n</table>",
  "result_type": "string",
  "serialization": "xml",
  "error": null,
  "exit_code": 0
}
```

### Implementation Strategy

**Option A: Modify frontier-cli to emit JSON** (RECOMMENDED)
- Add `--output-json` flag to frontier-cli
- When headless + this flag, wrap output in JSON envelope
- Capture scriptError and return value separately

**Option B: Parse existing output** (Fallback if A is too invasive)
- Use structured markers in stdout: `===RESULT===\n{json}\n===END===`
- Parse stderr for error messages
- More fragile but doesn't require CLI changes

I recommend **Option A** - clean separation of concerns, easy to test, forward-compatible.

---

## 3. Test Case Structure

### Test Case Definition Format (YAML)

**File**: `tests/integration/test_cases/string_verbs.yaml`

```yaml
test_cases:
  - name: "string.length - simple case"
    script: 'string.length("hello")'
    expect:
      success: true
      result: "5"
      result_type: "long"

  - name: "string.length - empty string"
    script: 'string.length("")'
    expect:
      success: true
      result: "0"
      result_type: "long"

  - name: "string.length - missing parameter"
    script: 'string.length()'
    expect:
      success: false
      error_contains: "Can't coerce"

  - name: "string.replaceAll - multi-line script"
    script: |
      local (s = "hello world");
      string.replaceAll(@s, "world", "frontier");
      return s
    expect:
      success: true
      result: "hello frontier"
      result_type: "string"

  - name: "table.assign - verify assignment"
    script: |
      local (t);
      new(tableType, @t);
      table.assign(@t.x, 5);
      return table.xml(@t)
    expect:
      success: true
      result_type: "string"
      serialization: "xml"
      xml_contains:
        - "<x>5</x>"
```

### Python Test Runner Example

**File**: `tests/integration/runner.py`

```python
#!/usr/bin/env python3
"""Integration test runner for UserTalk verb bindings."""

import subprocess
import json
import yaml
import sys
from pathlib import Path
from typing import Dict, Any, List
import xml.etree.ElementTree as ET

class FrontierCLI:
    """Wrapper for frontier-cli execution."""

    def __init__(self, cli_path: str, system_root: str):
        self.cli_path = cli_path
        self.system_root = system_root

    def execute(self, script: str) -> Dict[str, Any]:
        """Execute UserTalk script, return parsed JSON result."""
        cmd = [
            self.cli_path,
            "--system-root", self.system_root,
            "--output-json",  # New flag to emit JSON
            "-e", script
        ]

        env = {"FRONTIER_HEADLESS_SKIP_STARTUP": "1"}

        result = subprocess.run(
            cmd,
            env=env,
            capture_output=True,
            text=True
        )

        # Parse JSON output
        try:
            output = json.loads(result.stdout)
        except json.JSONDecodeError:
            # Fallback for non-JSON output (shouldn't happen with --output-json)
            output = {
                "success": result.returncode == 0,
                "result": result.stdout.strip(),
                "error": result.stderr.strip() if result.returncode != 0 else None,
                "exit_code": result.returncode
            }

        return output

class TestCase:
    """Represents a single integration test case."""

    def __init__(self, name: str, script: str, expect: Dict[str, Any]):
        self.name = name
        self.script = script
        self.expect = expect

    def verify(self, actual: Dict[str, Any]) -> tuple[bool, str]:
        """Verify actual output matches expectations."""

        # Check success/failure
        if "success" in self.expect:
            if actual["success"] != self.expect["success"]:
                return False, f"Expected success={self.expect['success']}, got {actual['success']}"

        # Check result value
        if "result" in self.expect:
            if actual["result"] != self.expect["result"]:
                return False, f"Expected result='{self.expect['result']}', got '{actual['result']}'"

        # Check result type
        if "result_type" in self.expect:
            if actual.get("result_type") != self.expect["result_type"]:
                return False, f"Expected type={self.expect['result_type']}, got {actual.get('result_type')}"

        # Check error contains substring
        if "error_contains" in self.expect:
            if not actual.get("error") or self.expect["error_contains"] not in actual["error"]:
                return False, f"Expected error containing '{self.expect['error_contains']}', got '{actual.get('error')}'"

        # Check XML structure
        if "xml_contains" in self.expect:
            if not actual.get("result"):
                return False, "Expected XML result, got None"

            for xpath in self.expect["xml_contains"]:
                if xpath not in actual["result"]:  # Simple substring check
                    return False, f"Expected XML containing '{xpath}', not found"

        return True, "OK"

class TestRunner:
    """Main test runner."""

    def __init__(self, cli: FrontierCLI):
        self.cli = cli
        self.passed = 0
        self.failed = 0
        self.failures: List[tuple[str, str]] = []

    def load_test_file(self, path: Path) -> List[TestCase]:
        """Load test cases from YAML file."""
        with open(path) as f:
            data = yaml.safe_load(f)

        return [
            TestCase(tc["name"], tc["script"], tc["expect"])
            for tc in data["test_cases"]
        ]

    def run_test(self, test: TestCase) -> bool:
        """Run a single test case."""
        print(f"  {test.name}... ", end="", flush=True)

        try:
            actual = self.cli.execute(test.script)
            ok, msg = test.verify(actual)

            if ok:
                print("✓ PASS")
                self.passed += 1
                return True
            else:
                print(f"✗ FAIL: {msg}")
                self.failures.append((test.name, msg))
                self.failed += 1
                return False

        except Exception as e:
            print(f"✗ ERROR: {e}")
            self.failures.append((test.name, str(e)))
            self.failed += 1
            return False

    def run_test_file(self, path: Path):
        """Run all tests in a file."""
        print(f"\n{path.stem}:")
        tests = self.load_test_file(path)
        for test in tests:
            self.run_test(test)

    def summary(self):
        """Print test summary."""
        print(f"\n{'='*60}")
        print(f"Passed: {self.passed}")
        print(f"Failed: {self.failed}")

        if self.failures:
            print(f"\nFailures:")
            for name, msg in self.failures:
                print(f"  - {name}: {msg}")

        print(f"{'='*60}")
        return self.failed == 0

def main():
    # Configuration
    cli_path = "./frontier-cli/frontier-cli"
    system_root = "./databases/Frontier.root7"
    test_dir = Path("tests/integration/test_cases")

    # Initialize
    cli = FrontierCLI(cli_path, system_root)
    runner = TestRunner(cli)

    # Run all test files
    test_files = sorted(test_dir.glob("*.yaml"))

    if not test_files:
        print(f"No test files found in {test_dir}")
        return 1

    for test_file in test_files:
        runner.run_test_file(test_file)

    # Summary
    success = runner.summary()
    return 0 if success else 1

if __name__ == "__main__":
    sys.exit(main())
```

---

## 4. Implementation Plan

### File Structure

```
tests/integration/
├── runner.py                     # Main test runner (above)
├── test_cases/                   # Test case definitions
│   ├── string_verbs.yaml         # String operation tests
│   ├── table_verbs.yaml          # Table operation tests
│   ├── file_verbs.yaml           # File operation tests
│   └── error_cases.yaml          # Error handling tests
├── fixtures/                     # Test data files (for file verbs)
│   ├── sample.txt
│   └── test_data/
├── README.md                     # Integration test documentation
└── requirements.txt              # Python dependencies (pyyaml)
```

```
tools/
└── run_integration_tests.sh      # Wrapper script
```

**Changes to frontier-cli:**
```
frontier-cli/
└── frontier-cli.c                # Add --output-json flag
```

### Implementation Steps

#### Step 1: Add JSON Output to frontier-cli (2-3 hours)

**File**: `frontier-cli/frontier-cli.c`

```c
// Add global flag
static boolean fl_output_json = false;

// In parse_args():
else if (strcmp(argv[i], "--output-json") == 0) {
    fl_output_json = true;
}

// In execute_expression():
boolean execute_expression(char *expression, tyvaluerecord *vreturned) {
    boolean success = true;
    char error_msg[1024] = {0};

    // Execute script
    success = langrunstring(expression, vreturned);

    if (!success && langgeterrorstring(error_msg, sizeof(error_msg))) {
        // Capture error
    }

    if (fl_output_json) {
        print_json_result(success, vreturned, error_msg);
    } else {
        // Existing output logic
    }

    return success;
}

// New function:
void print_json_result(boolean success, tyvaluerecord *val, char *error) {
    printf("{\n");
    printf("  \"success\": %s,\n", success ? "true" : "false");

    if (success && val) {
        char result_str[4096];
        coercetostring(val);  // Convert to string
        copyheapstring(val->data.stringvalue, result_str);

        printf("  \"result\": \"%s\",\n", json_escape(result_str));
        printf("  \"result_type\": \"%s\",\n", get_type_name(val->valuetype));
    } else {
        printf("  \"result\": null,\n");
    }

    if (error && strlen(error) > 0) {
        printf("  \"error\": \"%s\",\n", json_escape(error));
    } else {
        printf("  \"error\": null,\n");
    }

    printf("  \"exit_code\": %d\n", success ? 0 : 1);
    printf("}\n");
}
```

**Testing**:
```bash
./frontier-cli/frontier-cli --output-json -e "1+1"
# Should output valid JSON
```

#### Step 1.5: Verify No Regressions (15-30 mins)

**CRITICAL**: Before proceeding to Python test runner, verify CLI still works correctly.

**Tests to run**:
```bash
# Build with changes
make clean && make

# Run full headless test suite
./tools/run_headless_tests.sh

# Manual smoke tests - existing behavior (no --output-json)
./frontier-cli/frontier-cli -e "1+1"  # Should output "2"
./frontier-cli/frontier-cli -e 'string.length("hello")'  # Should work (if bound)

# New flag - verify JSON output works
./frontier-cli/frontier-cli --output-json -e "1+1"  # Should output valid JSON
./frontier-cli/frontier-cli --output-json -e "true"  # Should output boolean
./frontier-cli/frontier-cli --output-json -e "5+3"  # Should output long
```

**Success criteria**:
- ✅ All existing tests pass
- ✅ CLI without --output-json works exactly as before
- ✅ CLI with --output-json produces valid JSON
- ✅ No segfaults or crashes
- ✅ Error cases handled correctly

**If tests fail**:
- Debug Step 1 changes
- Do NOT proceed to Step 2 until regression is fixed
- Commit fixes before moving forward

**What to verify**:
```bash
# Test 1: Normal output unchanged
./frontier-cli/frontier-cli -e "1+1"
# Expected: "2" (or whatever current format is)

# Test 2: JSON output valid
./frontier-cli/frontier-cli --output-json -e "1+1" | python3 -m json.tool
# Expected: Valid JSON parses without error

# Test 3: Error handling
./frontier-cli/frontier-cli --output-json -e "undefined_var"
# Expected: JSON with success=false, error message present
```

#### Step 2: Create Python Test Runner (3-4 hours)

1. Create `tests/integration/runner.py` (code above)
2. Create `tests/integration/requirements.txt`:
   ```
   pyyaml>=6.0
   ```
3. Test basic execution:
   ```bash
   cd tests/integration
   pip3 install -r requirements.txt
   python3 runner.py
   ```

#### Step 3: Create Initial Test Cases (2-3 hours)

**File**: `tests/integration/test_cases/string_verbs.yaml`

Start with 5-10 test cases for string verbs:
- `string.length()`
- `string.lower()`
- `string.upper()`
- Error cases (missing params, wrong types)

#### Step 4: Integration with Existing Test Suite (1-2 hours)

**File**: `tools/run_integration_tests.sh`

```bash
#!/bin/bash
# Integration test runner wrapper

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

cd "$PROJECT_ROOT"

# Build frontier-cli if needed
if [ ! -f "frontier-cli/frontier-cli" ]; then
    echo "Building frontier-cli..."
    make frontier-cli
fi

# Check Python dependencies
if ! python3 -c "import yaml" 2>/dev/null; then
    echo "Installing Python dependencies..."
    pip3 install -r tests/integration/requirements.txt
fi

# Run integration tests
echo "Running integration tests..."
cd tests/integration
python3 runner.py "$@"
```

Make executable:
```bash
chmod +x tools/run_integration_tests.sh
```

**Update**: `tools/run_headless_tests.sh`

Add integration tests to the suite:
```bash
# At end of run_headless_tests.sh:
echo ""
echo "Running integration tests..."
./tools/run_integration_tests.sh
```

#### Step 5: Makefile Integration (30 mins)

**File**: `Makefile` (add target)

```makefile
.PHONY: test-integration
test-integration: frontier-cli
	@./tools/run_integration_tests.sh

.PHONY: test-all
test-all: test test-integration
	@echo "All tests passed!"
```

Usage:
```bash
make test-integration  # Just integration tests
make test-all          # Unit + integration
```

### Time Estimates

| Task | Time | Deliverable |
|------|------|-------------|
| Step 1: JSON output in CLI | 2-3h | frontier-cli --output-json works |
| Step 1.5: Regression testing | 15-30m | All existing tests pass |
| Step 2: Python test runner | 3-4h | runner.py executes test cases |
| Step 3: Initial test cases | 2-3h | string_verbs.yaml with 10 tests |
| Step 4: Shell script wrapper | 1-2h | ./tools/run_integration_tests.sh |
| Step 5: Makefile integration | 30m | make test-integration works |
| **Total** | **9.25-13h** | **Full framework operational** |

---

## 5. Integration with Existing Infrastructure

### Compatibility with run_headless_tests.sh

**Current**: `run_headless_tests.sh` runs C unit tests from `tests/` directory

**Integration Strategy**:
- Keep existing unit tests unchanged
- Add integration tests as separate phase
- Both run via `make test-all` or `./tools/run_headless_tests.sh --all`

**Modified run_headless_tests.sh**:
```bash
#!/bin/bash

# ... existing unit test logic ...

# New: Integration tests
if [ "$1" == "--all" ] || [ "$1" == "--integration" ]; then
    echo ""
    echo "Running integration tests..."
    ./tools/run_integration_tests.sh
fi
```

### Reusing Existing Test Patterns

**From C unit tests**, we can reuse:
- Database setup/teardown patterns (create clean Frontier.root7)
- Test fixture data (test databases with known tables)
- Error message validation patterns

**Example - Database Fixture**:

```python
class FrontierCLI:
    def __init__(self, cli_path: str, system_root: str, use_clean_db: bool = False):
        self.cli_path = cli_path

        if use_clean_db:
            # Copy reference database to temp location
            import shutil
            import tempfile
            self.temp_dir = tempfile.mkdtemp()
            self.system_root = f"{self.temp_dir}/test.root"
            shutil.copy(system_root, self.system_root)
        else:
            self.system_root = system_root

    def cleanup(self):
        if hasattr(self, 'temp_dir'):
            shutil.rmtree(self.temp_dir)
```

### Test Database Strategy

**Option A: Shared read-only database** (RECOMMENDED for Phase 1)
- Use `databases/Frontier.root7` for all tests
- Tests are read-only (don't modify database)
- Fast, simple, low overhead

**Option B: Per-test database copy**
- Copy clean database for each test
- Allows testing write operations
- Slower, but isolated

**Recommendation**: Start with Option A. Add Option B when testing write operations (file verbs, table mutations).

---

## 6. Future Evolution Path to Long-Running Process

### Phase 2 Architecture Changes

**Current (Phase 1)**:
```python
def execute(self, script: str) -> Dict[str, Any]:
    result = subprocess.run([self.cli_path, "-e", script], ...)
    return json.loads(result.stdout)
```

**Future (Phase 2)**:
```python
class FrontierSession:
    """Persistent frontier-cli session."""

    def __init__(self, cli_path: str, system_root: str):
        self.process = subprocess.Popen(
            [cli_path, "--server", "--system-root", system_root],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            text=True
        )

    def execute(self, script: str) -> Dict[str, Any]:
        # Send command
        cmd = {
            "action": "eval",
            "script": script,
            "request_id": self.next_id()
        }
        self.process.stdin.write(json.dumps(cmd) + "\n")
        self.process.stdin.flush()

        # Read response
        response = self.process.stdout.readline()
        return json.loads(response)

    def close(self):
        self.process.stdin.write('{"action": "quit"}\n')
        self.process.wait()
```

### Migration Steps (Future)

1. **Add --server mode to frontier-cli**:
   - Read JSON commands from stdin
   - Write JSON responses to stdout
   - Maintain runtime state between commands

2. **Update FrontierCLI class**:
   - Add `mode` parameter: "oneshot" vs "session"
   - Factory method creates appropriate backend

3. **Test cases remain unchanged**:
   - Same YAML files
   - Same verification logic
   - Just swap execution backend

### Benefits of This Architecture

✅ **Incremental migration**: Phase 1 works today, Phase 2 is additive
✅ **Test reuse**: Same test cases work with both backends
✅ **Performance**: Session mode is faster (no startup overhead per test)
✅ **State testing**: Session mode enables testing stateful scenarios
✅ **Clean abstraction**: `FrontierCLI` interface hides implementation

---

## 7. Example Test Cases

### Example 1: Simple String Operation

```yaml
- name: "string.length - basic"
  script: 'string.length("hello")'
  expect:
    success: true
    result: "5"
    result_type: "long"
```

**Expected CLI output**:
```json
{
  "success": true,
  "result": "5",
  "result_type": "long",
  "error": null,
  "exit_code": 0
}
```

### Example 2: Multi-Line Table Operation

```yaml
- name: "table.assign - create and assign"
  script: |
    local (t);
    new(tableType, @t);
    table.assign(@t.name, "test");
    table.assign(@t.value, 42);
    return t.name + "=" + string(t.value)
  expect:
    success: true
    result: "test=42"
    result_type: "string"
```

### Example 3: Error Case - Missing Parameter

```yaml
- name: "string.length - missing param"
  script: 'string.length()'
  expect:
    success: false
    error_contains: "Can't coerce"
```

**Expected CLI output**:
```json
{
  "success": false,
  "result": null,
  "result_type": null,
  "error": "Can't coerce the empty value to a string because it isn't of a compatible type.",
  "exit_code": 1
}
```

### Example 4: Structured XML Verification

```yaml
- name: "table.xml - verify structure"
  script: |
    local (t);
    new(tableType, @t);
    table.assign(@t.x, 5);
    table.assign(@t.y, "test");
    return table.xml(@t)
  expect:
    success: true
    result_type: "string"
    serialization: "xml"
    xml_contains:
      - "<x>5</x>"
      - "<y>test</y>"
```

**Verification logic**:
```python
# In TestCase.verify():
if "xml_contains" in self.expect:
    xml_result = actual["result"]
    for expected_fragment in self.expect["xml_contains"]:
        if expected_fragment not in xml_result:
            return False, f"XML missing '{expected_fragment}'"
```

---

## 8. Trade-offs and Risks

### Architectural Trade-offs

| Aspect | Decision | Trade-off |
|--------|----------|-----------|
| **Language** | Python | ✅ Fast development, rich libraries<br>⚠️ Adds Python dependency |
| **Output Format** | JSON | ✅ Structured, easy to parse<br>⚠️ Requires CLI changes |
| **Test Definition** | YAML | ✅ Human-readable, easy to add tests<br>⚠️ Requires YAML parser |
| **Execution Model** | One-shot first | ✅ Simple, immediate value<br>⚠️ Slower than persistent session |

### Risks and Mitigations

**Risk 1: frontier-cli JSON output breaks existing tools**
- **Mitigation**: JSON output only when `--output-json` flag is set
- **Validation**: Test existing CLI usage still works

**Risk 2: Python dependency adds setup complexity**
- **Mitigation**: Auto-install in `run_integration_tests.sh`
- **Fallback**: Provide manual setup instructions in README

**Risk 3: Test cases become hard to maintain**
- **Mitigation**: Clear YAML structure, self-documenting test names
- **Example**: Include comments in YAML for complex tests

**Risk 4: Error messages change, breaking tests**
- **Mitigation**: Use `error_contains` (substring match) instead of exact match
- **Future**: Add regex support for flexible matching

**Risk 5: JSON serialization harder than expected** (IDENTIFIED BY USER)
- **Challenge**: Frontier's internal data format is complex - simple `coercetostring()` may not be sufficient
- **Mitigation Strategy**:
  - **Phase 1A**: Start with simple scalar types (long, boolean, string) - these should serialize easily
  - **Phase 1B**: Use existing UserTalk serialization (e.g., `table.xml()`) for complex types instead of JSON
  - **Defer complex JSON**: Don't attempt to serialize tables/outlines/complex types directly to JSON in Step 1
  - **Fallback**: If JSON proves too difficult, fall back to structured text markers with type hints
- **Implementation approach**:
  ```c
  // Simple types: serialize directly
  if (val->valuetype == longvaluetype) {
      printf("  \"result\": \"%ld\",\n", val->data.longvalue);
  }
  // Complex types: delegate to UserTalk
  else if (val->valuetype == externalvaluetype) {
      printf("  \"result_type\": \"external\",\n");
      printf("  \"serialization\": \"deferred\",\n");
      // Or call table.xml() from C if available
  }
  ```

---

## 9. Success Metrics

**Short-term (after implementation)**:
- ✅ Can verify 10+ string verbs work end-to-end
- ✅ Tests run in <5 seconds
- ✅ Clear pass/fail output with helpful error messages
- ✅ Easy to add new test cases (5 mins per test)

**Medium-term (after binding all 22 verbs)**:
- ✅ 50+ integration test cases covering all verb bindings
- ✅ Tests catch regressions in glue script changes
- ✅ CI runs integration tests on every PR

**Long-term (Phase 2 migration)**:
- ✅ Persistent session mode reduces test time by 50%
- ✅ Stateful test scenarios (multi-step workflows)
- ✅ Framework supports performance testing

---

## 10. Next Steps

### Immediate Action Items

1. **Review this design** with user - get approval on architecture
2. **Implement Step 1** (JSON output in frontier-cli) - validates core approach
3. **Prototype runner.py** with 3 test cases - proves integration works
4. **Iterate** based on findings

### Questions for User - ANSWERED 2025-12-31

1. **JSON output format**: ✅ Acceptable, but **RISK IDENTIFIED**: Disentangling Frontier's internal data format for JSON serialization might be more work than it looks. User is okay with this risk but it's noted as a potential blocker.

2. **Test organization**: ✅ Group by category (string/table/file). Matches docserver documentation and system verb table organization.

3. **Error handling**: ✅ YES - Must distinguish between ALL of the following:
   - Script returns false (normal execution, false result)
   - Script has runtime error (scriptError message)
   - C-level runtime crash (segfault, etc.)
   - Assertion failures (C assert)

4. **Performance**: ✅ Not a concern. 10 tests/sec is plenty fast for now. Monitor but don't optimize prematurely.

5. **Python dependency**: ✅ No concerns. Python 3 is available on all major platforms. User is open to alternatives if needed.

### Estimated Timeline

- **Week 1**: JSON output + basic runner (Steps 1-2)
- **Week 2**: String verb test cases (Step 3)
- **Week 3**: Table/file verb test cases + full integration (Steps 4-5)
- **Total**: 2-3 weeks to full operational framework

---

## Appendix: Alternative Architectures Considered

### Alternative A: Pure Shell Script

**Pros**: No Python dependency, simple
**Cons**: Hard to parse JSON, limited error handling, difficult to maintain

**Verdict**: ❌ Too fragile for structured output verification

### Alternative B: C-Based Test Framework

**Pros**: No external dependencies, fast execution
**Cons**: Slower development, harder to write expressive tests, poor string manipulation

**Verdict**: ❌ Good for unit tests, wrong tool for integration tests

### Alternative C: Expect/TCL

**Pros**: Built for interactive process testing
**Cons**: Complex syntax, steep learning curve, not commonly used

**Verdict**: ❌ Overkill for one-shot execution, better for Phase 2

### Why Python Won

- ✅ **Rich ecosystem**: JSON, YAML, XML parsing built-in
- ✅ **Rapid development**: Write tests quickly
- ✅ **Maintainable**: Clear syntax, self-documenting
- ✅ **Evolves gracefully**: Handles both one-shot and persistent modes
- ✅ **Team familiarity**: Most developers know Python

---

## Known Limitations & Future Work

### String Encoding (UTF-8 Migration)

**Current State**: JSON output escaping assumes UTF-8 encoded strings. Frontier's runtime currently uses Pascal strings (bigstring/pstring) which are length-prefixed byte arrays, typically ASCII or MacRoman encoding.

**Limitation**: Multi-byte UTF-8 sequences are passed through directly (valid per JSON RFC 8259), but invalid UTF-8 sequences may produce malformed JSON output.

**Future Roadmap**: Runtime string migration to UTF-8 is planned (estimated: several months out). When implemented, this will:
- Unify all runtime strings to UTF-8 encoding
- Eliminate encoding ambiguity
- Ensure robust JSON escaping for all character sets
- Improve international character support

**Current Mitigation**:
- JSON escaping handles ASCII and valid UTF-8 correctly
- Control characters are properly escaped
- Test suite includes UTF-8 test cases to validate current behavior
- Edge cases documented in code comments

See `frontier-cli/cli_executor.c` (cli_print_json_escaped_string) for detailed documentation.

---

## Summary

This design provides:
1. **Immediate value**: Start testing verb bindings today with one-shot execution
2. **Future-proof**: Clean migration path to persistent session model
3. **Developer-friendly**: Easy to add tests, clear failure messages
4. **Production-ready**: Integrates with existing test suite and CI

**Total implementation time: 9-12 hours** to fully operational framework.

Ready to proceed with implementation?
