# YAML Integration Test Patterns

Pulled in when: you're writing or debugging a yaml integration test, doing burndown, or you see a test pass with a wrong-looking result. This is the deep dive that the primer's Section 4 points to.

The framework reference (file layout, runner flags, fixture rules) is `../TESTING_GUIDE.md`. The cross-agent rules (sequential, needs_guest_dbs, system.temp isolation) are in `../AI_SHARED_GUIDELINES.md`. This file is the **UserTalk-author's view of writing scripts that yaml tests will actually run correctly**.

---

## 1. The `with`-wrapper trap (tracked: #624)

Protocol mode is the default test executor. It wraps every `script:` block in:

```
with system.temp.FrontierREPL.variables {
    «your script here»}
```

That outer `with` block is a load-bearing source of subtle failures. Five concrete bites:

### 1.1 Locals don't survive `\r` to a later `return`

A multi-line script that declares a `local` on one line and `return`s it on the next silently returns `true` (the `with` block's status) instead of the intended value:

```yaml
- name: "local survives same-line"
  script: 'local (x = 5); return x'
  expected_success: true
  expected_result: "5"                  # ✓ passes — x = 5

- name: "local dropped across CR"
  script: |
    local (x = 5)
    return x
  expected_success: true
  expected_result: "5"                  # ✗ FAILS — script returns true (boolean)
```

The `with` block's exit status bubbles up when the locals' scope is lost between statements. Filed as **#624 P0**.

Until #624 is fixed, two workarounds:

- **Same-line + `;`**: collapse `local` and `return` into one logical statement.
- **`repl_mode: true`**: opt out of the protocol wrapper entirely for this test.

```yaml
- name: "multi-line OK in repl_mode"
  repl_mode: true
  script: |
    local (x = 5)
    return x
  expected_success: true
  expected_result: "5"                  # ✓ passes
```

### 1.2 `@addr` output parameters fail inside the wrapper

Verbs that write back via an address argument can't reach the caller's local through the `with` block:

```yaml
- name: "unixshell capture — broken in protocol mode"
  script: 'local (stdout); sys.unixShellCommand ("echo hi", @stdout); return stdout'
  expected_success: true
  expected_result: "hi\r"                # ✗ FAILS — stdout never written

- name: "unixshell capture — fixed under repl_mode"
  repl_mode: true
  script: |
    local (stdout)
    sys.unixShellCommand ("echo hi", @stdout)
    return stdout
  expected_success: true
  expected_result: "hi\r"                # ✓ passes
```

Any verb whose signature includes an `@addr` output parameter is suspect under protocol mode. When in doubt, run it `repl_mode: true`.

### 1.3 `thread.evaluate` doesn't work in the wrapper

Spawned threads run outside the `with` scope, so they can't see `local`s from the wrapped block and the wrapped block can't see what the thread wrote. Use `repl_mode: true` for any test that calls `thread.evaluate` or `thread.spawn`.

### 1.4 Empty-string returns serialize as `null`

A script that returns `""` comes back through the protocol JSON layer as `null`, not `""`. If you're testing a verb that legitimately returns an empty string, expect:

```yaml
expected_result: "null"                  # not "\"\"" — the protocol drops the empty string
```

Or — again — `repl_mode: true` to bypass the serialization.

### 1.5 `target.set ()` / `target.clear ()` interference

`with` establishes an implicit target context. Tests that explicitly set or clear the target inside the script may see those operations either masked by the outer `with` or interact with it in ways that depend on call order. If a target-manipulating test gives confusing results, switch to `repl_mode: true` and re-run.

---

## 2. `expected_success` vs `expected_result` — the boolean quoting trap

This bites every burndown session. The two fields look symmetric and are not:

```yaml
expected_success: true                   # yaml BOOLEAN — matches protocol's success field
expected_result: "true"                  # yaml STRING — compared to script's stringified return
```

`expected_result` is compared as a **string** against the protocol's stringification of the script's return value. If yaml parses the value as a non-string and the runner stringifies it inconsistently, the comparison silently fails.

**Always quote `expected_result`.** Concrete forms:

| What the script returns | `expected_result` value |
|---|---|
| Boolean `true` | `"true"` |
| Long `5` | `"5"` |
| String `"hello"` | `"hello"` |
| OSType `'TEXT'` (e.g. from `typeof`) | `"TEXT"` |
| OSType `'tabl'` | `"tabl"` |
| Address `@x.y` | `"x.y"` |
| Empty string `""` | `"null"` (see §1.4) |

### The OSType result_type quirk

When a script returns an OSType, the protocol response has `result_type: "unknown"` because JSON has no 4-byte-OSType type. The string form goes in `result.value`. So in tests:

```yaml
script: 'return typeof ("hello")'
expected_success: true
expected_result: "TEXT"                  # not "'TEXT'", not "stringType" — just the 4-byte code as string
```

Don't try to match `result_type` for OSType returns. Match the stringified `result.value`.

---

## 3. `repl_mode: true` — when and why

Default test executor is protocol mode (`with`-wrapped, one persistent process per worker, NDJSON-multiplexed). `repl_mode: true` opts a single test out of that and runs it as a fresh REPL session: a new process, stdin-fed script, stdout-captured result.

**Set `repl_mode: true` when**:

- The verb takes `@addr` output parameters (§1.2)
- The script has `local`/`return` on separate lines and you can't collapse to `;` (§1.1)
- The script involves `thread.evaluate` / `thread.spawn` (§1.3)
- You're testing `target.set ()` / `target.clear ()` behavior (§1.5)
- The test depends on a specific empty-string vs null distinction (§1.4)

**Costs**:

- Slower (per-test process spawn vs. shared executor)
- Doesn't get the protocol debugger's read-only safety net
- More resource-hungry under parallel test runs

**Convention**: when you set `repl_mode: true`, put a one-line comment in the test `description:` saying why — future-you (or another agent on burndown) will see why this test is special and not "fix" it by removing the flag:

```yaml
- name: "thread.evaluate completes"
  description: "repl_mode because thread.evaluate doesn't see with-block locals (#624)"
  repl_mode: true
  script: ...
```

---

## 4. File-level metadata

These go at YAML root, BEFORE the `tests:` array. Each one trades performance for correctness on a specific axis:

```yaml
needs_guest_dbs: true
sequential: true
protocol_mode: false
tests:
  - name: ...
```

### `sequential: true`

File runs in the main process, not a parallel worker. Set when:

- Tests bind a TCP port (only one binder at a time)
- Tests interact with the REPL's stdin/stdout
- Tests have process-level side effects that another test in the same file might observe

Cost: the whole file serializes against the rest of the suite. Use sparingly — file-level granularity, so one sequential test forces all tests in that file sequential.

### `needs_guest_dbs: true`

Runner copies sibling `.root` files and `Guest Databases/` into the worker's tmp dir before running. Set when:

- The script calls `fileMenu.open ()` or any verb that resolves paths relative to `Frontier.getFilePath ()`
- The test opens a database other than the system root

Without this, the worker has no `Guest Databases/` directory and path-resolution falls back to the system root, masking the bug or producing confusing "file not found" errors.

### `protocol_mode: false`

Skip the protocol executor entirely; spawn a new process per test. Set when:

- Tests block waiting for UI input (the protocol executor can't drive a blocking dialog)
- Tests need fresh process state per case (caches, globals, daemon threads)
- Tests deliberately exercise startup / shutdown paths

This is heavier than `repl_mode: true` on individual tests because it applies file-wide. Prefer per-test `repl_mode: true` if only some cases need the fresh process.

---

## 5. `system.temp` ONLY — never `workspace`

Test isolation rule. UserTalk has top-level tables (`system`, `workspace`, `user`, etc.) that persist across the test session. Mutating any of them outside `system.temp` corrupts the next test (and possibly the suite for the rest of the run).

```
new (tableType, @system.temp.fixture);   // ✓ scratch — cleared per process
new (tableType, @workspace);             // ✗ DESTROYS workspace — never write this
new (tableType, @workspace.fixture);     // ✗ pollutes a persistent table
```

`system.temp.*` is cleared per process start, so cleanup is technically optional. Cleanup is still good practice for readability:

```yaml
script: |
  new (tableType, @system.temp.myFixture);
  system.temp.myFixture.x = 42;
  local (result = system.temp.myFixture.x);
  delete (@system.temp.myFixture);
  return result
```

The cross-agent rule (see `../AI_SHARED_GUIDELINES.md`) treats `new (tableType, @workspace)` as a tripwire — flag it in review.

---

## 6. Comments inside `script:` blocks

The "no `//` inside blocks" caution from older docs is overstated. Verified recent probes:

```
if true {        // this comment is fine
    return 42}
```

works. The `//` consumes to end-of-line and the `}` on the next physical line closes the block cleanly.

What's actually unsafe:

- **`/* ... */`** — not a UserTalk comment at all. Parses as `/` followed by `*` followed by `/`, silently miscompiling as a division-multiplication expression. NEVER use. (Primer Section 3.)
- **`«…»`** in yaml `script:` blocks — the 0xC7 / 0xC8 bytes for the guillemets get mangled by YAML's encoding handling. The script fails to compile or compiles to something different from what you wrote. Safe in `.ut` files (preserved encoding), unsafe in yaml.
- **`#`** inside `script:` block content — UserTalk has no `#` comment. `#` is a yaml comment only at the yaml indentation level, never inside the script text.

Reference: `~/.claude/projects/-Users-jake-dev-jsavin-Frontier/memory/feedback_usertalk_comments.md`.

When in doubt: put the `//` on its own line, not on the same line as a closing brace. That dodges any remaining edge cases in places where `//` and `}` might collide structurally.

---

## 7. The eval-trap history (#618)

Before fix **#618**, `langruntraperror` returned `true` for compile errors. The consequence:

- A script with a syntax error compiled, failed silently, and reported `success: true` to the protocol layer.
- Roughly **108 tests** were silently passing — they "succeeded" only because the bug masked the compile failure.

After #618:

- Compile errors surface as `success: false` with a useful error string.
- The previously-silent-passing tests now correctly fail, and **#620** is the umbrella tracking their cleanup or correction (most are skipped pending burndown).

What this means for you on burndown:

- If a test was "passing" before #618 and "failing" after, the after state is the truth. Don't restore the old expected_success by adjusting the expectation — fix the script.
- If a test is in the #620-skip set, opening it means re-deriving the intent from the script, fixing the syntax, and confirming the *real* behavior is what the test wants to assert.

---

## 8. Behavioral assertions, not source inspection

Tests must assert on **the computed behavior of the verb**, not on the **text of the script**. The forbidden pattern:

```yaml
# ✗ DO NOT do this — passes against a no-op compile, proves nothing
script: |
  local (src = string (system.verbs.builtins.help.showHelp));
  return indexOf (src, "stdout") > 0
expected_result: "true"
```

This test passes if the source contains the substring — even if the verb is broken, has been deleted, or never executes. A no-op compile still passes the assertion.

The correct form drives the verb and checks the **result or side effect**:

```yaml
# ✓ behavioral — calls the verb, asserts on what it actually does
script: |
  local (output);
  help.showHelp (@output);
  return indexOf (output, "Usage:") > 0
expected_result: "true"
```

Forbidden patterns to flag on review:

- `indexOf (string (someVerb), "...") > 0`
- Regex matching against the script body via `string.patternMatch` or `regex.*`
- Any assertion that would still pass if the verb were replaced with `on f () {return true}`

The general principle: the test should fail if the verb stops doing its job. Source inspection passes regardless of the verb's job.

---

## 9. Handler scripts need end-to-end execution tests

UserTalk verb resolution is **late-binding**: a handler that calls a non-existent verb compiles cleanly and only fails at the point of the call, at runtime.

```
on showHelp () {
    stdout ("Usage: ...")}        // ← `stdout` doesn't exist as a verb; compiles fine
```

A test that only confirms the handler **compiles** is worthless for this class of bug:

```yaml
# ✗ insufficient — script.compile returns true even if the handler will crash on first call
script: 'return script.compile (string (system.verbs.builtins.help.showHelp))'
expected_result: "true"
```

The handler must be **executed through its production dispatch path** — REPL, menu dispatcher, slash command system, whatever invokes it in the real product — and the test must assert on an observable side effect of that execution.

**Motivating incident**: PR #582 shipped a `help.ut` handler that called a non-existent `stdout` verb. The integration tests verified each handler compiled but never executed any of them. The bug shipped. PR #583 caught it during end-to-end testing.

**Reference**: `tests/integration/test_cases/repl_palette.yaml` for an example of end-to-end handler testing — it drives the REPL through its real dispatch path and asserts on emitted output.

For any verb you add or modify that's invoked through a dispatcher: at least one test must exercise the **dispatch + execution** chain, not just the compile.

---

## 10. Quick checklist for a new yaml test

Before declaring a test done, walk this list:

- [ ] `expected_result` is **quoted** (string), even for `"true"` / `"5"` / `"TEXT"`
- [ ] No `/* ... */` anywhere in the script
- [ ] No `«…»` in yaml `script:` blocks
- [ ] Multi-line scripts: either same-line + `;`, or `repl_mode: true`
- [ ] All scratch state goes under `system.temp.*`
- [ ] If the verb takes `@addr` output: `repl_mode: true`
- [ ] If the test opens a guest database: `needs_guest_dbs: true` at file root
- [ ] If the test binds a port / drives REPL stdin: `sequential: true` at file root
- [ ] Assertion is on the verb's computed value or side effect, not on the script text
- [ ] For dispatched handlers: at least one end-to-end execution path covered

---

## See also

- `CLAUDE_PRIMER.md` — entry-point primer; Section 4 is the fast-path on yaml integration tests
- `records_and_tables.md` — when fixture data uses tables/records and you hit type confusion
- `debugging_workflow.md` — protocol UserTalk debugger when a test misbehaves and you need to step through it
- `../TESTING_GUIDE.md` — full test framework reference (file layout, runner flags, fixture rules)
- `../AI_SHARED_GUIDELINES.md` — cross-agent rules: `system.temp` isolation, integration-test expectations for verb changes
- `~/.claude/projects/-Users-jake-dev-jsavin-Frontier/memory/feedback_usertalk_comments.md` — `«…»` parse errors in yaml tests
