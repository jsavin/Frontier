# UserTalk Primer for Claude

**Load this before touching `.ut` files, yaml integration tests, or UserTalk runtime debugging.** ~250 lines, eager-load cost is small. Deeper docs in this directory are linked from here and pulled in only when a task needs them.

This primer's job: stop you from probing-and-experimenting for basic idioms during integration-test burndown. The verb-by-verb reference is in `docs/usertalk/docserver/`; the `usertalk-engineer` sub-agent has a comprehensive reference loaded as its system prompt. This file is the **language tour with idioms and gotchas** — what those don't teach.

---

## 1. Mental model — this is not Python or C

| What you'd assume from Python/C | What's actually true in UserTalk |
|---|---|
| Strings literal: `'hello'` or `"hello"` | **Double quotes only.** `'X'` is a character constant; `'TEXT'` is a 4-byte OSType. `'hello'` is a syntax error. |
| `string.contains(a, b)` is a method call | `contains` is an **infix operator**: `a contains b`. There is no `string.contains` verb. |
| `a.b` is field access on any value | `a.b` reads a sub-table member from a `tableType`. Records (`recordType`) don't support dotted-name access — see records_and_tables.md. |
| `defined (@x.y.z)` checks if the object exists | `defined ()` actually checks **address validity** — true if `@x.y.z` is well-formed (either points to a real object, OR points to a creatable location where the parent path exists). To check whether the object actually exists *right now*, use `defined (adr^)`. See section 2.2. (Tracked: #625.) |
| `f(x, y)` always takes positional values | Many verbs take **addresses** (`@var`) so they can write back. `dialog.ask ("?", @result)` writes into `result`. |
| `for x in collection` always yields values | Two forms: `for v in listOrRecordValue` yields **values** (works on records and lists). `for adrM in @table` yields **addresses** (works on tables; pass the address, not the value). Dereference with `adrM^`. |
| `/* comment */` works | **Not a UserTalk comment.** It silently parses as a division-multiplication expression and can corrupt your script's return value. Use `//` or `«…»`. |
| Verb calls: `f(x)` | UserLand style: **space before `(`**: `f (x)`. Both compile; only the spaced form is idiomatic. |
| Closing brace on its own line | Idiomatically **inline at the end of the last statement**: `... ; return x}`. This matches outline ↔ text round-trip. |
| Method-style verb evolution | UserTalk is dispatch-by-name through the ODB. The "verb" `string.upper` is a script at `system.verbs.builtins.string.upper` — a glue script that calls a C kernel implementation. |

If something feels surprising, check this table before assuming. The rest of the primer expands each row.

---

## 2. The five idioms you'll write 90% of the time

### 2.1 Substring check

```
if userInput contains "yes" {...}    // ✓ infix operator, no verb call
```

NOT `string.contains (userInput, "yes")` (no such verb), NOT `string.patternMatch ("*yes*", userInput)` (pattern matching is for wildcards, not substring).

### 2.2 Existence check

`defined ()` returns true if its argument is a **valid address** — either pointing to a real object, OR pointing to a creatable location (a path whose parent exists). It is NOT a simple "does this object exist" check.

```
defined (@workspace)                          // true — points to a real object
defined (@workspace.notYetCreated)            // true — workspace exists; the address is creatable
defined (@workspace.notYetCreated.deeper)     // false — invalid address (parent path doesn't resolve)
defined (adrThing^)                           // true iff there's a real object at that address right now
defined (path.to.thing)                       // resolves the path; runtime error if it doesn't exist (wrap in try)
```

The form you reach for depends on intent:

| Question | Use |
|---|---|
| Is this address well-formed enough to write into? | `defined (@x.y.z)` |
| Does the object actually exist right now? | `defined (adr^)` (dereference; doesn't error on missing) |
| Does this name resolve in current scope? | `defined (name)` (no `@`) |

### 2.3 Reading a script body

`string ()` produces the text source of a script object. It takes the **value**, not the address:

```
local (s = string (string.length))            // ✓ value form: argument is the script object itself
local (s = string (adrScript^))               // ✓ dereference an address to get the value
local (s = string (@string.length))           // ✗ argument is an address VALUE; you get back "@string.length", NOT the script source
```

Common mistake: writing `string (@some.script.path)` thinking it dereferences. It doesn't — `string ()` stringifies whatever it's given, and an address stringifies to its dotted path.

The same rule applies to `typeof ()`, `sizeOf ()`, etc.: they operate on the value, not the address. Always dereference with `^` when you have an address and want to operate on the target.

### 2.4 Reading a table member

```
local (val = examples.colors.red)             // dotted name — works for tables
local (val = examples.colors.[someVarName])   // bracket form for dynamic names
local (val = examples.colors.["stringType"])  // bracket form when name shadows a keyword/global
```

**For records**, dotted-name access doesn't exist — use ordinal iteration. See records_and_tables.md.

### 2.5 Iteration

Two forms depending on what you're iterating:

```
// Tables — iterate over the ADDRESS of the table; visitor is the address of each member.
for adrMember in @examples.colors {
    log (nameOf (adrMember^) + " = " + adrMember^)}

// Lists and records — iterate over the value; visitor is the value of each member.
for n in {1, 2, 3} {
    log (n)}                                              // n is 1, then 2, then 3

local (rec = {"alpha": 1, "beta": 2});
for v in rec {
    log (v)}                                              // v is 1, then 2 (insertion order)
```

For tables: pass the address (`@examples.colors`), not the value (`examples.colors`). Passing the value gives `This operation is not supported for table values.`

`nameOf (adrMember^)` gets the member name; `nameOf (adrMember)` gets the local variable's name (`"adrMember"`), which is rarely what you want.

---

## 3. Failure-mode decoder — when you see this error

| Symptom | Likely cause | Fix |
|---|---|---|
| `Can't compile this script because of a syntax error.` on `/* ... */` | `/* */` isn't a UserTalk comment | Use `//` or `«…»` |
| Script `returns` a value but yaml test sees `true` (boolean) | Multi-statement script missing `;` separators — `\r`/`\n` are whitespace in UserTalk, not statement terminators | The protocol layer now normalizes bare `\r`/`\n` to `;` (#628), but explicit `;` is clearest |
| `Character constant isnt correctly specified. Must be of the form 'c'.` | Used `'foo'` for a string | Use `"foo"` |
| `Can't find a sub-table named X` | Tried `r.X` on a value that's NOT a `tableType` (probably `recordType` or scalar) | `typeof (r)` to check; if it's a record, you must iterate ordinally |
| `Can't call the script because the name X hasn't been defined.` | Verb doesn't exist, OR you forgot the system root, OR name collides with a global | Check verb name; confirm `--system-root` is loaded; try bracket form for the leaf |
| `Cant evaluate the expression because the name X hasnt been defined.` | Referenced a name that doesn't resolve in current scope | `defined (X)` first; check spelling and scope |
| `Cant coerce the string X to a character.` | Passed multi-char string to verb expecting `char` | Single-char `"X"`, or use `'X'` for a true char constant |
| `Cant compile` on a path like `@x.y.stringType` | Leaf collides with a global constant/keyword | Use bracket form: `@x.y.["stringType"]` |
| `This operation is not supported for table values.` | `for x in tableValue` (passed value, should be address) | `for adrM in @table` instead |
| yaml test passes with `success: true` but `result_type: unknown` for an OSType | Test framework JSON-serializes OSType codes as strings | Use `"TEXT"` etc. in test expectations, not `stringType` |
| `[lang-ERROR] langcallbacks.c:208:` (empty error line after success) | Harmless — known protocol bookkeeping noise | Ignore |
| `expected_result: true` mismatch even though script returns `true` | yaml parsed `true` as a boolean, but test framework compares string | Quote it: `expected_result: "true"` |
| `string.patternMatch` returns 0 even though strings look equal | `string.patternMatch` is exact equality, not a glob/substring match — `*` and `?` are literals | Use `contains` for substring, `==` for equality |
| `Can't coerce X value to a record.` on `record contains val` | `contains` on records is not supported | Use `contains` only on strings or lists; iterate records ordinally |

---

## 4. Authoring UserTalk in yaml integration tests

This is where most of your UserTalk writing happens during burndown. The constraints:

### Quoting in yaml `expected_*` fields

```yaml
expected_success: true        # yaml boolean — comparison is to the protocol success field
expected_result: "true"       # MUST be quoted string — compared against script's stringified return
```

`expected_result` is a **string comparison** against the script's return value as serialized by the protocol layer. If the yaml parses `true` as a boolean, the comparison silently fails — quote it.

### The `with`-wrapper trap (tracked: #624)

Protocol mode (default for yaml integration tests) wraps every `script:` block in `with system.temp.FrontierREPL.variables { ... }`. This causes:

- **`\r`/`\n` are whitespace in UserTalk, not statement terminators.** The protocol layer normalizes bare newlines to `;` (#628), so multi-line scripts work. Explicit `;` is still clearest and most portable.
- Verbs taking `@addr` output parameters often fail inside the wrapper. **Fix**: `repl_mode: true`.
- `thread.evaluate` doesn't work inside the wrapper. **Fix**: `repl_mode: true`.
- Empty string returns may serialize as `null`.

Full details: `testing_patterns.md`.

### Comments in yaml `script:` blocks

- `//` is OK between statements inside `script:` blocks (it consumes to end of line).
- `«…»` works but YAML may mangle the bytes — avoid in yaml scripts; OK in `.ut` files.
- `/* */` is **NEVER OK** — not a UserTalk comment at all.
- `#` is a yaml comment but **NOT inside `script:` block content** — UserTalk doesn't have `#` comments.

### `system.temp` only

Integration tests must isolate state under `system.temp.*`. Never mutate `workspace.*` or any top-level system table directly. `system.temp` is cleared per process start, but cleanup with `delete (@system.temp.foo)` is still good practice for readability.

Specifically forbidden: `new (tableType, @workspace)` — destroys the entire workspace table.

### Handler scripts need end-to-end execution tests

A handler that compiles cleanly can still fail at runtime if it calls a name that doesn't resolve (late binding). Tests must drive the handler through its production dispatch path (REPL, menu, slash command) and assert on observable side effects — not just `script.compile (s) == true`.

---

## 5. Tools for investigation

### Protocol UserTalk debugger — the primary tool for "why?"

The protocol exposes a real debugger (since #588 / protocol expansion): `debug/setBreakpoint`, `debug/run`, `debug/continue`, `debug/step`, `debug/getLocals`, `debug/getSource`, `debug/getStack`, `debug/setWatchpoint`, `debug/listThreads`, `debug/kill`. Suspension reasons: `entry`, `breakpoint`, `step`, `interrupted`, `watchpoint`. Conditional breakpoints supported.

**Use the debugger first** for "why isn't this called?" / "what values flow through?" — matches the global "debugger-first for control flow" rule.

Full walkthrough with burndown-relevant examples: `debugging_workflow.md`.

### `script/eval` as a fast probe

```bash
./frontier-cli/frontier-cli --protocol --skip-startup --system-root databases/Frontier.root
```

Then NDJSON in, NDJSON out. One-shot expressions; great for "what does this verb return on this input?" Per-eval, the protocol wraps in the `with` block — same caveats as yaml tests.

For ODB **edits**, add `--allow-mutate`. `--protocol --system-root` defaults to read-only since #588.

### `string ()`, `typeof ()`, `nameOf ()` for inspection

```
typeof (x)                       // 'TEXT', 'tabl', 'reco', 'list', 'long', 'bool', etc.
string (scriptObject)            // script source text
nameOf (adrMember^)               // member name (NOT adrMember)
```

### `dialog.alert` doesn't work in headless

Use `return` to surface values, or write to a temp object you can read back via protocol.

---

## 6. Deeper docs (load when topic comes up)

- **records_and_tables.md** — when you hit "Can't find sub-table" or need to walk an unknown structure
- **operators_and_idioms.md** — when an operator behaves unexpectedly (`contains`, `==`, `^`, `@`, word vs symbol forms)
- **strings_and_text.md** — when truncation hits at 255 bytes, when escaping fights you, Pascal-string vs `Handle` distinction
- **verb_invocation_patterns.md** — when a verb errors with "wrong number of parameters" — kernel verb vs `.ut` glue script vs headless overload
- **testing_patterns.md** — yaml integration tests in depth: the `with`-wrapper trap, `repl_mode`, eval-trap history, behavioral vs source-inspection assertions
- **debugging_workflow.md** — protocol debugger with worked examples
- **values_and_types.md** — type taxonomy reference (stub)
- **scripts_vs_handlers.md** — `on foo () {...}` vs top-level statements; how scripts get auto-loaded (stub)
- **comments_and_whitespace.md** — `«…»` vs `//`, outline-vs-text storage of comments, `\r` as canonical line terminator (stub)

The verb-by-verb reference (75+ categories) is at `docs/usertalk/docserver/`. The `usertalk-engineer` sub-agent has a comprehensive language reference loaded as its system prompt — delegate to it for deeper kernel-side work.

Cross-cutting memory files (`feedback_usertalk_*` in your memory/) still apply — they're tight rules-each, not language tour.
