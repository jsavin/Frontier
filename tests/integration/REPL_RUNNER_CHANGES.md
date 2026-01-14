# REPL Test Runner Implementation

**Date**: 2026-01-13
**Status**: Complete

## Overview

Updated `tests/integration/runner.py` to support REPL mode tests alongside existing batch mode tests.

## Changes Made

### 1. TestCase Class Extensions

Added REPL-specific fields to `TestCase.__init__()`:

```python
self.repl_mode = data.get('repl_mode', False)
self.expected_output_contains = data.get('expected_output_contains', [])
self.expected_output_not_contains = data.get('expected_output_not_contains', [])
```

### 2. FrontierCLI.execute_repl() Method

New method for executing frontier-cli in REPL mode:

```python
def execute_repl(self, stdin_input: str, timeout: int = 30,
                 env: Optional[Dict[str, str]] = None) -> subprocess.CompletedProcess
```

**Behavior**:
- Invokes frontier-cli WITHOUT `--output-json` or `-e` flags
- Pipes `stdin_input` to REPL stdin
- Returns `CompletedProcess` with stdout, stderr, and returncode
- Handles timeouts gracefully (30s default)

### 3. TestCase.validate_repl_output() Method

Validates REPL text output (not JSON):

```python
def validate_repl_output(self, result: subprocess.CompletedProcess) -> Tuple[bool, Optional[str]]
```

**Checks**:
- All strings in `expected_output_contains` appear in stdout+stderr
- No strings in `expected_output_not_contains` appear in output
- Exit code matches `expected_success`

### 4. TestRunner Refactoring

Split `run_test()` into mode-specific executors:

- `run_test()` - Detects `repl_mode` and routes to appropriate executor
- `run_batch_test()` - Executes batch mode tests (existing logic)
- `run_repl_test()` - Executes REPL mode tests (new logic)

## Test Format Differences

### Batch Mode Test (existing)
```yaml
- name: "Basic arithmetic"
  script: "1 + 1"
  expected_result: 2
  expected_success: true
```

### REPL Mode Test (new)
```yaml
- name: "REPL /help command"
  repl_mode: true
  stdin_input: |
    /help
    /exit
  expected_output_contains:
    - "Available commands"
    - "/exit"
  expected_success: true
```

## Usage

### Run REPL Tests Only
```bash
cd tests/integration
python3 runner.py test_cases/repl_*.yaml --system-root ../../databases/Frontier-v6.root7
```

### Run Mixed Batch + REPL Tests
```bash
python3 runner.py test_cases/*.yaml --system-root ../../databases/Frontier-v6.root7
```

### Verbose Output
```bash
python3 runner.py test_cases/repl_commands.yaml --system-root ../../databases/Frontier-v6.root7 --verbose
```

## Test Results

With REPL implementation complete (as of 2026-01-13):

| Test Suite        | Passed | Total | Pass Rate |
|-------------------|--------|-------|-----------|
| repl_basic.yaml   | 49     | 64    | 76%       |
| repl_commands.yaml| 19     | 36    | 53%       |
| repl_sessions.yaml| 32     | 42    | 76%       |
| **Total**         | **100**| **142**| **70%**  |

## Failure Analysis

Common failure patterns (not runner issues, but REPL implementation details):

1. **Variable persistence**: Tests expect `workspace.x = 42` to be listed by `/vars`, but `/vars` shows `(empty)` because variables aren't persisting correctly
2. **Error messages**: Tests expect specific error text that doesn't match current implementation
3. **Prompt display**: Tests expect `[root]>` but prompts may be suppressed or formatted differently
4. **Boolean JSON encoding**: Batch tests expect `True` but get `'true'` (JSON encoding issue)

## Backward Compatibility

All existing batch mode tests continue to work without changes:
- string_verbs.yaml: 21/21 passed ✓
- file_verbs.yaml: Compatible ✓
- dialog_verbs.yaml: Compatible ✓

## Implementation Notes

### Why Two Execution Paths?

**Batch Mode** (`execute()`):
- Uses `--output-json` flag for structured results
- Uses `-e "script"` for inline script execution
- Returns parsed JSON dict

**REPL Mode** (`execute_repl()`):
- No `--output-json` (REPL prints text to stdout)
- No `-e` (REPL reads from stdin)
- Returns raw stdout/stderr text

### Timeout Defaults

- Batch tests: 10 seconds (configurable via `timeout` field)
- REPL tests: 30 seconds (REPL sessions may be longer)

### Environment Variables

Both modes support custom environment variables via `environment` field:
```yaml
environment:
  FRONTIER_LOG_LEVEL: "debug"
  FRONTIER_HEADLESS_RUN_STARTUP: "1"
```

## Future Enhancements

Phase 2 REPL tests may require:
- Multi-line input handling (backslash continuation)
- History navigation simulation
- Enhanced value formatting validation

## References

- **Planning**: planning/phase4/REPL_INTERACTIVE_MODE_DESIGN.md
- **Test Guide**: tests/integration/REPL_TESTING_GUIDE.md
- **Test Cases**:
  - tests/integration/test_cases/repl_basic.yaml
  - tests/integration/test_cases/repl_commands.yaml
  - tests/integration/test_cases/repl_sessions.yaml
