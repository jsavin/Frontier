# REPL Testing Guide

**Status**: Test infrastructure design for Phase 1
**Date**: 2026-01-13
**Reference**: planning/phase4/REPL_INTERACTIVE_MODE_DESIGN.md

---

## Overview

This guide describes the testing strategy for REPL (Read-Eval-Print Loop) interactive mode. The tests are designed to run **before implementation** (TDD approach) and will fail until REPL is fully implemented.

---

## Test Files

### 1. repl_basic.yaml
**Purpose**: Core REPL functionality tests

**Coverage**:
- Basic expression evaluation (arithmetic, strings, booleans)
- Workspace variable persistence
- Value display for different types
- Error handling (syntax errors, runtime errors, undefined variables)
- Type coercion
- Multi-statement evaluation
- Edge cases

**Test Count**: ~70 tests

**Example**:
```yaml
- name: "REPL - workspace variable arithmetic"
  description: "Arithmetic on workspace variables should work"
  script: |
    workspace.a = 10;
    workspace.b = 20;
    workspace.a + workspace.b
  expected_success: true
  expected_result: 30
```

**Status**: Tests ready, implementation pending

---

### 2. repl_commands.yaml
**Purpose**: REPL special commands (/help, /vars, /clear, /exit)

**Coverage**:
- `/help` - Display available commands and usage
- `/vars` - List workspace variables with values
- `/clear` - Clear workspace variables
- `/exit` - Exit REPL cleanly
- Unknown command handling
- Command error cases
- Interleaving commands with code

**Test Count**: ~50 tests

**Example**:
```yaml
- name: "REPL /vars - multiple variables"
  description: "/vars should list all workspace variables"
  repl_mode: true
  stdin_input: |
    workspace.x = 42
    workspace.y = "hello"
    workspace.z = true
    /vars
    /exit
  expected_output_contains:
    - "x = 42"
    - "y = \"hello\""
    - "z = true"
  expected_success: true
```

**Status**: Tests ready, requires stdin simulation support in runner

---

### 3. repl_sessions.yaml
**Purpose**: REPL session lifecycle and integration

**Coverage**:
- System root auto-loading
- Workspace isolation from system tables
- Error recovery across evaluations
- Workspace persistence within session
- Function definitions in workspace
- Startup/shutdown messages
- Prompt display
- Session cleanup

**Test Count**: ~40 tests

**Example**:
```yaml
- name: "REPL - continues after runtime error"
  description: "Runtime errors shouldn't crash REPL"
  repl_mode: true
  stdin_input: |
    1 / 0
    42
    /exit
  expected_output_contains:
    - "division by zero"
    - "42"
  expected_success: true
```

**Status**: Tests ready, requires REPL mode detection in runner

---

## Test Infrastructure Requirements

### 1. REPL Mode Detection

The test runner must detect `repl_mode: true` flag and invoke frontier-cli differently:

**Batch Mode (current)**:
```bash
frontier-cli --output-json -e "script"
```

**REPL Mode (new)**:
```bash
echo "script\n/exit\n" | frontier-cli --interactive
```

### 2. Stdin Simulation

Tests using `stdin_input` field must feed input to frontier-cli stdin:

```python
def execute_repl_test(test):
    stdin_input = test.get('stdin_input', '')
    result = subprocess.run(
        ['frontier-cli', '--interactive'],
        input=stdin_input,
        capture_output=True,
        text=True,
        timeout=10
    )
    return parse_repl_output(result.stdout, result.stderr)
```

### 3. Output Validation

REPL output includes prompts, results, and messages. Validation must handle:

**expected_output_contains**: List of strings that must appear in output
```yaml
expected_output_contains:
  - "x = 42"
  - "Workspace variables"
```

**expected_output_not_contains**: List of strings that must NOT appear
```yaml
expected_output_not_contains:
  - "old = 1"  # After /clear
```

**expected_result**: Exact result value (for batch-mode tests)
```yaml
expected_result: 42
```

### 4. Test Runner Modifications

**File**: `tests/integration/runner.py`

**Changes needed**:

1. **Detect repl_mode flag**:
```python
def run_test(test):
    if test.get('repl_mode', False):
        return run_repl_test(test)
    else:
        return run_batch_test(test)
```

2. **REPL test executor**:
```python
def run_repl_test(test):
    stdin_input = test.get('stdin_input', '/exit\n')
    no_system_root = test.get('no_system_root', False)

    cmd = [cli_path, '--interactive']
    if no_system_root:
        cmd.append('--no-system-root')

    result = subprocess.run(
        cmd,
        input=stdin_input,
        capture_output=True,
        text=True,
        timeout=test.get('timeout', 10)
    )

    return validate_repl_output(test, result)
```

3. **Output validation**:
```python
def validate_repl_output(test, result):
    output = result.stdout + result.stderr  # Combine for checking

    # Check expected_output_contains
    for substring in test.get('expected_output_contains', []):
        if substring not in output:
            return TestResult(test['name'], False,
                            f"Expected substring not found: {substring}")

    # Check expected_output_not_contains
    for substring in test.get('expected_output_not_contains', []):
        if substring in output:
            return TestResult(test['name'], False,
                            f"Unexpected substring found: {substring}")

    return TestResult(test['name'], True)
```

---

## Running REPL Tests

### Prerequisites

1. **Build frontier-cli**:
```bash
make -C frontier-cli
```

2. **System root available** (for most tests):
```bash
ls databases/Frontier.root  # Should exist (v7 format)
```

### Run All REPL Tests

```bash
cd tests/integration
python3 runner.py --filter repl_*.yaml
```

### Run Specific Test Suite

```bash
# Basic REPL tests only
python3 runner.py test_cases/repl_basic.yaml

# Command tests only
python3 runner.py test_cases/repl_commands.yaml

# Session tests only
python3 runner.py test_cases/repl_sessions.yaml
```

### Run Single Test

```bash
python3 runner.py test_cases/repl_basic.yaml --test "REPL - basic arithmetic"
```

### Expected Output (Before Implementation)

All tests should **FAIL** until REPL is implemented:

```
Running REPL tests...

repl_basic.yaml:
  ✗ REPL - basic arithmetic (frontier-cli: unknown option --interactive)
  ✗ REPL - workspace variable persistence (frontier-cli: unknown option --interactive)
  ...

Total: 0 passed, 160 failed

IMPLEMENTATION REQUIRED: These tests will pass once REPL Phase 1 is complete.
```

### Expected Output (After Implementation)

```
Running REPL tests...

repl_basic.yaml:
  ✓ REPL - basic arithmetic
  ✓ REPL - workspace variable persistence
  ✓ REPL - workspace variable arithmetic
  ...

repl_commands.yaml:
  ✓ REPL /help - displays available commands
  ✓ REPL /vars - empty workspace
  ✓ REPL /clear - clears workspace variables
  ...

repl_sessions.yaml:
  ✓ REPL - system root auto-loaded
  ✓ REPL - continues after runtime error
  ✓ REPL - workspace intact after error
  ...

Total: 160 passed, 0 failed
```

---

## Test-Driven Development Workflow

### Phase 1: Write Tests First (COMPLETE)

1. ✅ Create `repl_basic.yaml` (~70 tests)
2. ✅ Create `repl_commands.yaml` (~50 tests)
3. ✅ Create `repl_sessions.yaml` (~40 tests)
4. ✅ Document test infrastructure requirements

### Phase 2: Update Test Runner

1. Add `repl_mode` flag detection
2. Add stdin simulation support
3. Add output validation for `expected_output_contains`
4. Add `--interactive` flag to frontier-cli invocation

### Phase 3: Implement REPL (Iterative)

1. **Iteration 1: Minimal REPL**
   - Add `--interactive` flag to cli_parser.c
   - Implement basic read-eval-print loop
   - Run tests: Expect ~20% to pass (basic evaluation)

2. **Iteration 2: Workspace Persistence**
   - Implement `system.repl.workspace` table
   - Set `currenthashtable` to workspace before eval
   - Run tests: Expect ~40% to pass

3. **Iteration 3: Commands**
   - Implement `/exit`, `/help`, `/vars`, `/clear`
   - Add command detection in REPL loop
   - Run tests: Expect ~70% to pass

4. **Iteration 4: Error Recovery**
   - Add error handling without crashing
   - Ensure workspace persists after errors
   - Run tests: Expect ~90% to pass

5. **Iteration 5: Polish**
   - Welcome/goodbye messages
   - Prompt display
   - Edge case handling
   - Run tests: Expect 100% to pass

---

## Test Categories and Priorities

### Priority 0: Must-Pass for Phase 1 Completion

**Basic evaluation** (repl_basic.yaml):
- Basic arithmetic
- String operations
- Workspace variable persistence
- Simple error handling

**Core commands** (repl_commands.yaml):
- `/exit` works
- `/help` shows commands
- `/vars` lists variables
- `/clear` removes variables

**Session lifecycle** (repl_sessions.yaml):
- REPL starts and exits cleanly
- System root auto-loads
- Errors don't crash REPL

**Count**: ~50 P0 tests

### Priority 1: Important for Production Use

**Advanced workspace** (repl_basic.yaml):
- Function definitions
- Nested tables
- Type coercion
- Complex expressions

**Command robustness** (repl_commands.yaml):
- Unknown command handling
- Command case sensitivity
- Interleaving commands and code

**Error recovery** (repl_sessions.yaml):
- Multiple consecutive errors
- Workspace intact after errors
- Long-running sessions

**Count**: ~60 P1 tests

### Priority 2: Nice-to-Have

**Edge cases** (repl_basic.yaml):
- Very long strings
- Very long variable names
- Unicode handling
- Empty/whitespace input

**Advanced features** (repl_commands.yaml):
- EOF (Ctrl-D) handling
- Large workspace display
- Nested table display

**Optional scenarios** (repl_sessions.yaml):
- REPL without system root
- Unicode variable names

**Count**: ~50 P2 tests

---

## Known Test Limitations

### 1. Single-Script Simulation

**Issue**: Tests in `repl_basic.yaml` simulate workspace persistence **within a single script**, not across multiple REPL evaluations.

**Example**:
```yaml
script: |
  workspace.x = 10;
  workspace.x = workspace.x + 1;
  workspace.x
```

This tests that variables persist **within the script**, but doesn't test persistence **between separate REPL inputs**.

**Mitigation**: `repl_commands.yaml` and `repl_sessions.yaml` use `stdin_input` with multiple lines to simulate true REPL session behavior.

### 2. Command Tests Require Special Runner

**Issue**: Command tests use `expected_output_contains` which requires parsing stdout/stderr, not JSON output.

**Current**: Test runner expects JSON from `--output-json` flag.

**Fix**: REPL mode tests must NOT use `--output-json`, and must parse text output.

### 3. Prompt Output Interference

**Issue**: REPL prompts like `[root]> ` appear in stdout, making exact output matching difficult.

**Solution**: Use `expected_output_contains` for substring matching, not exact equality.

### 4. Skipped Tests

Some tests are marked `skip: true` because they require infrastructure not yet available:

```yaml
- name: "REPL - /vars command with empty workspace"
  skip: true  # Enable when REPL command testing ready
```

**Fix**: Remove `skip: true` once test runner supports REPL mode.

---

## Manual Testing Checklist

Before marking Phase 1 complete, manually verify:

### Basic REPL Usage
```bash
$ ./frontier-cli
Frontier REPL - Interactive UserTalk Environment
Type /help for commands, /exit to quit

[root]> 1 + 1
2
[root]> "Hello, " + "world!"
"Hello, world!"
[root]> /exit
Goodbye!
```

### Workspace Persistence
```bash
[root]> workspace.x = 42
42
[root]> workspace.y = workspace.x * 2
84
[root]> workspace.x + workspace.y
126
```

### /vars Command
```bash
[root]> workspace.a = 1
[root]> workspace.b = "hello"
[root]> /vars
Workspace variables (2):
  a = 1
  b = "hello"
```

### /clear Command
```bash
[root]> workspace.x = 42
[root]> /clear
Workspace cleared
[root]> /vars
(empty)
```

### Error Recovery
```bash
[root]> 1 / 0
Error: Division by zero
[root]> workspace.x = 42
42
[root]> workspace.y
Error: Can't find a variable named "y"
[root]> workspace.x
42
```

### /help Command
```bash
[root]> /help
Available commands:
  /exit          Exit the REPL
  /help          Show this help message
  /clear         Clear workspace variables
  /vars          Show workspace variables
...
```

---

## Success Criteria

Phase 1 is **COMPLETE** when:

1. ✅ All P0 tests pass (~50 tests)
2. ✅ 90%+ of P1 tests pass (~54/60 tests)
3. ✅ Manual testing checklist verified
4. ✅ No memory leaks detected (run with valgrind)
5. ✅ Documentation updated (CLI_USAGE_GUIDE.md)

---

## Phase 2 Tests (Future)

Phase 2 will add tests for:
- Backslash continuation (multi-line input)
- Readline/libedit integration (history)
- Enhanced value formatting (tables, binaries)

Test file: `repl_multiline.yaml` (to be created)

---

## Debugging Failed Tests

### Test Fails with "unknown option --interactive"

**Cause**: REPL mode not yet implemented in cli_parser.c

**Fix**: Add `--interactive` flag to cli_parser.c and main.c

### Test Fails with "Can't find a sub-table named 'repl'"

**Cause**: Workspace table not created under system.repl.workspace

**Fix**: Implement workspace initialization in repl_eval.c

### Test Fails with "division by zero" Crashes REPL

**Cause**: Error handling doesn't catch exceptions properly

**Fix**: Wrap `langrunscriptcode()` in error handling, continue REPL loop

### Test Expects Output Not Found

**Cause**: Output format mismatch

**Fix**: Adjust test expectations OR fix output formatting in repl_output.c

---

## References

**Planning Documents**:
- planning/phase4/REPL_INTERACTIVE_MODE_DESIGN.md - Complete REPL specification
- docs/CLI_USAGE_GUIDE.md - CLI usage patterns
- docs/TESTING_GUIDE.md - General testing patterns

**Test Files**:
- tests/integration/test_cases/repl_basic.yaml - Core functionality tests
- tests/integration/test_cases/repl_commands.yaml - Command tests
- tests/integration/test_cases/repl_sessions.yaml - Session lifecycle tests

**Implementation Files**:
- frontier-cli/repl.h, repl.c - Main REPL loop and the slash-command
  dispatcher (dispatch_slash_command). PR 7 retired the legacy
  hardcoded if/strcmp chain; slash commands now resolve through
  the menubar at system.menus.data.repl.REPL.
- frontier-cli/repl_eval.h, repl_eval.c - Workspace and evaluation
- frontier-cli/repl_output.h, repl_output.c - Value display
- frontier-cli/repl_slash_resolver.h, repl_slash_resolver.c -
  Menubar-walk resolver that maps a slash-command token to a leaf
  hashtable inside system.menus.data.repl.REPL.
- frontier-cli/repl_verbs.h, repl_verbs.c - Five repl.* kernel verbs
  (exit, clearVariables, jumpPath, printKeyCodes, list, help) plus
  the host adapter pattern that wires them to repl.c side effects.

---

## Cross-Thread Test Pattern

**Status**: Established 2026-05-08 (issue #597, PR for chain 7)
**First example**: `tests/palette_arg_inject_concurrency_tests.c`

### When to use this pattern

Reach for the cross-thread regression pattern whenever a verb or helper
has any of these properties:

- **Touches a process-shared global between yield points.** The classic
  example is the original PR #584 design where `g_palette_pending_arg`
  was written by the dispatcher and read by the kernel verb across
  `langruncode()`'s GIL-yield boundary. Any sibling thread holding the
  GIL between those two points could read or stomp the value.
- **Has a single-shot scratch buffer that lives outside the call frame.**
  Static buffers, file-scope globals, or singleton arenas that the
  helper writes through and the caller later reads from are all hijack
  candidates under the GIL model. The hijack is structurally impossible
  only if every byte of the per-call data lives on the stack or in a
  per-call malloc'd heap allocation.
- **Returns a pointer into an internal cache.** If two callers can
  receive the same pointer to a shared cache slot, the second caller's
  write can clobber the first caller's read.

If none of those apply, a single-threaded unit test against the pure
helper is sufficient — the helper is structurally race-free.

### Pattern shape

The reusable test infrastructure is in
`tests/palette_arg_inject_concurrency_tests.c`. The shape:

1. **Synchronization barrier** (`barrier_t` — a single-shot mutex +
   condvar wrapper). Every worker `barrier_wait()`s at the start of its
   `run()` function; the main thread calls `barrier_release()` once all
   workers are spawned. This makes all workers start the workload
   simultaneously, maximizing the contention window for any hypothetical
   shared-state stomp.

   `pthread_barrier_t` is technically the right primitive but is
   OPTIONAL in POSIX and absent on macOS — hand-roll the
   `mutex + cond + bool` wrapper instead.

2. **Per-worker arguments** (`worker_args_t`). Each worker gets:
   - A unique id (0..N-1)
   - A pointer to the shared barrier
   - A thread-private input value (e.g., `"ARG-3"`)
   - A `failures` counter the worker increments per-iteration when its
     output doesn't match its own input

3. **Inner loop in the worker**. 1000-2000 iterations per worker is
   the smallest count that reliably catches a regression in
   fault-injection runs. Each iteration: call the helper with the
   worker's input, assert the output reflects ONLY this worker's
   value (using a strict containment check, not a substring match).

4. **Aggregate at the end**. `main()` joins all workers, sums their
   failure counts, asserts zero. On non-zero, log per-worker counts to
   stderr so a future failure points at which workers stomped.

5. **Sanity sub-tests**. Run a single-threaded version of the same
   workload first (proves the helper itself works), then a low-iteration
   harness smoke test (proves pthread_create / barrier / pthread_join
   all work). If those pass and the production stress test fails, the
   regression is in the production helper, not the harness.

### Verifying the test catches regressions

For any pattern instance, you MUST verify the test goes red on a
hypothetical regression before declaring it shipped. The cookbook:

1. Temporarily edit the production helper to introduce a static or
   file-scope buffer that the helper writes through (mimicking the
   `g_palette_pending_arg`-style hijack).
2. Rebuild and run the concurrency test — the test should fail with
   per-worker leakage counts in the high hundreds out of low thousands
   of iterations.
3. Revert the production-side regression patch.
4. Re-run — the test should pass with 0 failures.

For palette_arg_inject specifically, this verification was done during
PR construction. The pattern caught 2,980 / 16,000 leaked iterations
when a fake `g_regression_pending_arg` static buffer was wired in.

### Why this is C-level rather than YAML

A YAML-level cross-thread test (using `thread.evaluate("verb_x()")`
from a sibling thread) is the obvious shape for testing a production
verb under the GIL. It is the right shape eventually. It is NOT
available today because:

- `thread.exists / thread.evaluate / thread.callscript / ...` are
  defined in `frontier-cli/headless_thread_verbs.c::threadinitverbs()`
  but `threadinitverbs()` is not called in the headless build's
  startup. Confirmed by running `return thread.getcount()` against
  `frontier-cli` and observing `Can't call the script because the
  name thread hasn't been defined.`
- All seven tests in `tests/integration/test_cases/thread_verbs_foundation.yaml`
  carry `skip: "thread verbs not wired in headless mode"` for that
  reason.

When `threadinitverbs()` is wired into headless startup, the cross-
thread pattern can move up to YAML for the verbs whose entire dispatch
path is reachable from UserTalk (e.g., calling `repl.list("Y")` from a
sibling thread while the main thread is mid-palette-dispatch). Until
then, C-level pthread tests against pure helpers — like
palette_arg_inject_concurrency_tests — are the durable regression
guard.

### Underlying mechanism (when YAML cross-thread becomes available)

The thread verbs use a Global Interpreter Lock (GIL) model — see
`frontier-cli/headless_thread_verbs.c` and ADR-014. Spawned threads
block on `frontier_gil` and only run when the holding thread yields
via `langbackgroundtask()` or `thread.sleepTicks()`. A YAML cross-
thread test pattern would look like:

```yaml
- name: "verb X arg isolation under thread.evaluate"
  script: |
    new(tableType, @system.temp.observed);
    thread.evaluate(
      "system.temp.observed.sibling = verb.X(\"Y\")");
    local(main = verb.X("Z"));
    thread.sleepTicks(12);
    local(sibling = system.temp.observed.sibling);
    delete(@system.temp.observed);
    return main == "main got Z" and sibling == "sibling got Y"
```

The `thread.sleepTicks(12)` yields the GIL long enough for the
sibling to run `verb.X` to completion. If verb X has a process-shared
arg slot, the sibling's call would read the main thread's arg
(or vice versa), and one of the assertions would fail.

### Future use

When the next concurrency-sensitive verb lands — e.g., the
`palette modal blocking GIL during user idle` work tracked in PR #582
discussion — instantiate the same `barrier_t` + `worker_args_t` shape
in a new `tests/<verb>_concurrency_tests.c` file. Wire it into
`tests/Makefile`'s `RUN_BUILDABLE` list and add a corresponding
build rule (`-lpthread`).

If a verb's full dispatch path is reachable from pure C (like the
palette helpers) → use C-level pthreads.
If reachability requires UserTalk-level state (system tables, REPL
state, file handles) → wait for `threadinitverbs()` wiring and write
a YAML-level test instead.

---

**Document Status**: Complete - Ready for Implementation
**Next Step**: Update test runner to support repl_mode flag
