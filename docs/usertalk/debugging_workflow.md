# UserTalk Debugging Workflow

Pulled in when: a yaml test returns the wrong value, a handler doesn't seem to dispatch, you need to know what's actually in a table at runtime, or you're tracking a verb to its source. The primer's Section 5 names the tools; this file is the burndown-time deep dive.

The framing: **the protocol UserTalk debugger is the primary tool** for "why?" questions. This is the UserTalk-side cousin of the global "debugger-first for control flow" rule (which in C-land means LLDB). Logging-style probes (`msg ()`, return-and-inspect) are the fallback, not the default.

The full protocol reference is `../DEBUGGING_GUIDE.md` under "UserTalk Debugging via Protocol". This file curates that reference for integration-test burndown and adds worked examples.

---

## 1. When to reach for which tool

A decision tree before you start probing:

| Question | First move | Fallback |
|---|---|---|
| "Why did this test return the wrong value?" | `script/eval` to repro in isolation (5-second probe) | `debug/run` + `debug/getLocals` if the eval still surprises you |
| "Why isn't this verb being called?" | `debug/setBreakpoint` on line 1 of the handler, then drive the dispatch path | Grep the dispatch table; check `parentOf ()` of the verb name |
| "What's actually in this table at runtime?" | `script/eval` with `for adrM in @t {...}` walker | `debug/getLocals` if it's a local of a suspended thread |
| "Where is this verb defined?" | `string (verbValue)` to dump the source via `script/eval` | grep `.ut` files; compare with the ODB-resident copy |
| "Did this handler dispatch at all?" | `debug/setBreakpoint` line 1, drive the trigger | `msg ()` at handler entry — but only if you can't get the debugger to bite |
| "What's the call stack at this point?" | `debug/getStack` | inspect `system.compiler.stack` (legacy approach, fragile) |
| "Does this variable change unexpectedly?" | `debug/setWatchpoint` on the variable name | re-read with `script/eval` after each suspected mutation |

The shape of the rule: **eval first if it's a value question, breakpoint first if it's a control-flow question**. Don't bring the debugger to a one-line "what does `nameOf` return for this?" check.

---

## 2. Protocol mode setup

Default-RW session:

```bash
./frontier-cli/frontier-cli --protocol --skip-startup --system-root databases/Frontier.root
```

NDJSON in (one JSON object per line on stdin), NDJSON out (responses + unsolicited notifications on stdout). The protocol stream carries both `script/*` and `debug/*` ops — same session, same connection.

Protocol mode opens the system root read-write by default (issue #127 restored the legacy default). To evaluate mutations in memory without persisting them to disk (useful for probes, tests, and inspection-only sessions), add `--lock-opened-roots`.

Suspension reasons you'll see in `debug/suspended` notifications:

| Reason | Means |
|---|---|
| `entry` | Thread started via `debug/run` and is parked at the first statement, waiting for you to `debug/continue` or step |
| `breakpoint` | Hit a line where `debug/setBreakpoint` was set |
| `step` | Completed one `debug/step` (into / over / out) |
| `watchpoint` | A `debug/setWatchpoint`-tracked variable changed value — the notification carries `oldValue` and `newValue` |
| `interrupted` | A `debug/pause` request landed |

---

## 3. The 15 debugger operations

This is the full inventory as of `../DEBUGGING_GUIDE.md`. Suspension and completion arrive as unsolicited notifications, not as `result` fields on the originating request — you read them off the stream as they come.

### 3.1 `debug/run` — start a debug session

```
{"op":"debug/run","id":1,"params":{"expression":"examples.target ()"}}
```

Returns `{"threadId": N, "status": "started"}` immediately. The thread **suspends at entry** (first statement). You must `debug/continue` (or step) before it actually runs.

### 3.2 `debug/continue` — resume

```
{"op":"debug/continue","id":2,"params":{"threadId":1}}
```

Resumes until the next stop event (breakpoint, watchpoint, step boundary, or completion).

### 3.3 `debug/pause` — interrupt a running thread

```
{"op":"debug/pause","id":3,"params":{"threadId":1}}
```

Forces the thread to suspend at the next statement. Useful for runaway scripts.

### 3.4 `debug/kill` — terminate

```
{"op":"debug/kill","id":4,"params":{"threadId":1}}
```

Hard-stops the thread. No locals returned, no completion success — gone.

### 3.5 `debug/step` — single-step

```
{"op":"debug/step","id":5,"params":{"threadId":1,"direction":"over"}}
```

`direction` is one of `"into"`, `"over"`, `"out"`. Standard stepper semantics. The thread re-suspends with `reason: "step"`.

### 3.6 `debug/setBreakpoint`

```
{"op":"debug/setBreakpoint","id":6,"params":{
    "script":"system.temp.examples.target","line":5}}
```

Set or **toggle** a breakpoint. Calling `setBreakpoint` again on the same (script, line) clears it — there is no separate single-breakpoint clear op.

**Line numbering is 1-based from the `on handlerName ()` declaration line.** The declaration itself is line 1; the first statement inside the handler body is line 2. Use `debug/getSource` to see the numbered lines before setting breakpoints — it shows the exact layout the debugger uses.

Optional `condition`: `"varname op value"` where `op` is `==`, `!=`, `>`, `<`, `>=`, `<=`. Numeric compare if both sides parse as numbers, string compare otherwise.

### 3.7 `debug/listBreakpoints`

```
{"op":"debug/listBreakpoints","id":7}
```

Returns all active breakpoints.

### 3.8 `debug/clearBreakpoints`

```
{"op":"debug/clearBreakpoints","id":8}
```

Clears all of them. There is **no per-id clear** — to clear one, toggle by re-issuing the same `setBreakpoint`.

### 3.9 `debug/setWatchpoint`

```
{"op":"debug/setWatchpoint","id":9,"params":{"variable":"x"}}
```

Watches a variable. When the value changes, the next suspension carries `reason: "watchpoint"` plus `variable`, `oldValue`, `newValue`. Toggle semantics — re-issue to clear that watchpoint specifically.

### 3.10 `debug/listWatchpoints`

```
{"op":"debug/listWatchpoints","id":10}
```

### 3.11 `debug/clearWatchpoints`

```
{"op":"debug/clearWatchpoints","id":11}
```

All-clear; no per-id clear.

### 3.12 `debug/getLocals`

```
{"op":"debug/getLocals","id":12,"params":{"threadId":1}}
```

Returns the locals of the suspended thread as a list of `{name, value, type}` records (value is stringified — same caveats as `string ()` on a value). Scope chain is locals → enclosing scopes → globals.

### 3.13 `debug/getStack`

```
{"op":"debug/getStack","id":13,"params":{"threadId":1}}
```

Returns the call-stack frames. Each frame includes the script name and current line.

### 3.14 `debug/getSource`

```
{"op":"debug/getSource","id":14,"params":{"script":"system.temp.examples.target","threadId":1}}
```

Returns the source with line numbers, breakpoint markers, and current-line marker. Loads from disk on demand if the script isn't in memory yet. `threadId` is optional — pass it to get the current-line marker.

### 3.15 `debug/listThreads`

```
{"op":"debug/listThreads","id":15}
```

Returns all active debug threads. Multi-thread debugging: each `debug/run` makes an independent thread; address them by `threadId`.

**Verify before relying on subtle behavior**: the op list above matches `../DEBUGGING_GUIDE.md` "All Protocol Operations" table at the time of writing. If headless adds a `debug/clearBreakpoint` (singular) or a `debug/reset`, the source of truth is that table.

---

## 4. Worked example: yaml test returning the wrong value

You wrote this test:

```yaml
- name: "addition returns 5"
  script: |
    local (x = 2 + 3)
    return x
  expected_result: "5"
```

The test fails with `actual_result: "true"`. Classic #624 with-wrapper trap — but let's walk it as if we didn't know.

### Step 1 — probe with `script/eval`

```
{"op":"script/eval","id":1,"params":{"expression":"local (x = 2 + 3)\rreturn x"}}
```

The response shows `result: "true"`, not `"5"`. Confirms the value is wrong at the same layer the yaml runner uses.

### Step 2 — collapse to one line

```
{"op":"script/eval","id":2,"params":{"expression":"local (x = 2 + 3); return x"}}
```

This returns `result: "5"`. Now you've isolated the trigger: the multi-line form fails, the single-statement form works.

### Step 3 — recognize the pattern

The protocol wraps every `script:` block in `with system.temp.FrontierREPL.variables { ... }`. The `\r` between `local` and `return` opens a new top-level statement inside the `with`, so `x` is out of scope by the time `return` runs. `return x` then evaluates an undefined name, and the wrapper layer surfaces that as `true` (it doesn't surface the error — see issue #624).

### Step 4 — fix the test

Two options:

```yaml
# Option A: keep it as protocol mode, same-line with ;
- name: "addition returns 5"
  script: |
    local (x = 2 + 3); return x
  expected_result: "5"

# Option B: switch to repl_mode, multi-line OK
- name: "addition returns 5"
  repl_mode: true
  script: |
    local (x = 2 + 3)
    return x
  expected_result: "5"
```

You did not need the debugger here. The `script/eval` probe was enough to reproduce, and the fix was at the test-shape level. **This is the common case for burndown** — most yaml mismatches resolve in one or two evals.

---

## 5. Worked example: "this handler doesn't run"

Scenario: a slash-command handler at `system.handlers.slashCommands.help` is supposed to fire when the REPL receives `/help`, but you see no output. The handler compiles cleanly. So: is it being dispatched at all?

### Step 1 — set a breakpoint at the entry

```
{"op":"debug/setBreakpoint","id":1,"params":{
    "script":"system.handlers.slashCommands.help","line":1}}
```

### Step 2 — drive the dispatch path

In a separate request (or via the actual REPL), trigger the slash command. If the dispatch goes through `script/eval`, you can fire it inline:

```
{"op":"debug/run","id":2,"params":{
    "expression":"system.handlers.dispatchSlashCommand (\"/help\")"}}
```

You get `{"threadId":2, "status":"started"}` and a `debug/suspended` notification with `reason: "entry"`. Continue:

```
{"op":"debug/continue","id":3,"params":{"threadId":2}}
```

### Step 3 — read the evidence

Two outcomes:

- **A `debug/suspended` arrives with `reason: "breakpoint"` at line 1 of the handler.** Dispatch is working. Step into with `debug/step direction=into` and watch what does or doesn't happen — likely the handler is running but its output isn't reaching where you expected. Inspect with `debug/getLocals`. Walk with `debug/step`.
- **No breakpoint hit, completion fires instead.** The handler is not being dispatched. Bug is in the dispatch table or name resolution, not the handler body. Now switch to inspecting `system.handlers.slashCommands` to confirm the entry exists, and breakpoint on the dispatch function itself.

### Step 4 — if it's a dispatch bug, breakpoint earlier

```
{"op":"debug/setBreakpoint","id":4,"params":{
    "script":"system.handlers.dispatchSlashCommand","line":1}}
{"op":"debug/run","id":5,"params":{
    "expression":"system.handlers.dispatchSlashCommand (\"/help\")"}}
```

Step through, watch the name lookup, see whether it finds `help` in its handler table.

The point: the debugger answered "did this run?" definitively. No `msg ("here")` sprinkling.

---

## 6. Worked example: "what's in this table?"

You're staring at `examples.target` and you have no idea what shape it is. The fastest dump:

```
{"op":"script/eval","id":1,"params":{
    "expression":"local (s = \"\"); for adrM in @examples.target {s = s + nameOf (adrM^) + \"=\" + adrM^ + \"; \"}; return s"}}
```

The result is a flat key=value dump.

Caveats:

- **Polymorphic members.** If `adrM^` is itself a table or record, `+` concatenation will fail or produce `'tabl'`-style garbage. To handle that, branch on `typeof (adrM^)`:

```
local (s = "");
for adrM in @examples.target {
    local (t = typeof (adrM^));
    if t == 'tabl' {s = s + nameOf (adrM^) + "=<table>; "}
    else {s = s + nameOf (adrM^) + "=" + adrM^ + "; "}};
return s
```

- **Records.** If you suspect the value is a record, table iteration won't work. Switch to value iteration: `for v in examples.target { ... }`. See `records_and_tables.md`.
- **Suspended-thread locals.** If the table you want is a `local` inside a suspended thread, `debug/getLocals` gets you the top level. To walk a table local in depth, set a breakpoint after it's populated and step.

---

## 7. Worked example: "where is this verb defined?"

You suspect a glue script has been modified or you want to see what's between the `on` line and the `kernel ()` call. Use `string ()` on the **value**, not the address:

```
{"op":"script/eval","id":1,"params":{"expression":"string (string.upper)"}}
```

Response: `"on upper (s) {\r\tkernel (string.upper)}"` — the script source, complete with `\r` line terminators.

**The common mistake**: `string (@string.upper)`. That returns the literal string `"@string.upper"` — the stringified form of the address value. `string ()` operates on its argument; an address stringifies to its dotted path. To dereference an address and inspect what's there, use `string (adr^)`.

If the verb's source is enormous or you want line numbers, `debug/getSource` is better:

```
{"op":"debug/getSource","id":2,"params":{"script":"string.upper"}}
```

Returns each line with `num`, `text`, and any breakpoint/current markers.

---

## 8. Common pitfalls during debugging

- **`dialog.alert ()` doesn't work in headless.** Don't waste cycles on it. Surface values via `return` or write to a temp object you can read back via `script/eval`.
- **`string (@addr)` doesn't dereference.** Always `string (adr^)` or `string (value)`. This is the same gotcha as in the primer's Section 2.3 — it shows up in debugging just as often.
- **`log_info ()` is C-kernel logging, not UserTalk-side.** You cannot call `log_info ()` from a script. From UserTalk, use `msg ()` (writes to the status line) or — better — `return` the value and read it via the protocol layer.
- **The debugger suspends threads on entry by default.** A `debug/run` that you never `debug/continue` will sit there forever. If you wanted "just run this script in debug mode and let it complete," you still need an explicit continue after the entry suspension.
- **Toggle vs clear.** `debug/setBreakpoint` is a toggle. There is no per-id clear op for breakpoints or watchpoints — only `clearBreakpoints` / `clearWatchpoints` (all of them) or re-issuing the same `setBreakpoint` / `setWatchpoint` to flip that specific one off.
- **`getLocals` returns stringified values.** `value` is `string ()`-form, not a typed object. To inspect a table local in depth, breakpoint after population and probe with `script/eval` using the actual path.
- **`script` paths use dotted notation.** Leading `@` is stripped automatically by the debugger, but be consistent — `system.temp.examples.target` and `@system.temp.examples.target` are equivalent in `debug/setBreakpoint`. The dotted form is conventional.
- **Variable lookups search the full scope chain.** `getLocals`, conditional breakpoints, and watchpoint variable resolution all search locals → enclosing scopes → globals. A watchpoint on `x` will fire on any `x` in scope at the suspension point.

---

## 9. When the debugger isn't enough

A handful of cases where the debugger doesn't help and you need older tools:

1. **Pre-compilation inspection.** The debugger needs a compiled script. To see what the source looks like before compilation, read the `.ut` file directly and compare against the ODB-resident form via `string (script.path)`.
2. **Outline-level investigation.** When you suspect post-export reformatting is hiding something — comments dropped, indentation lost, brace placement changed — walk the raw outline node-by-node:
   ```
   op.firstSummit ();
   op.fullExpand ();
   op.go (flatdown, 1);
   local (txt = op.getLineText ())
   ```
   This bypasses the export pass entirely. Useful when the exported source looks fine but the runtime behavior is off, or when you're debugging the exporter itself.
3. **C-kernel crashes.** If the debugger session itself dies or you're seeing SIGSEGV from a verb body, you're past UserTalk and into LLDB territory. See `../DEBUGGING_GUIDE.md` Sections 1-4 for the LLDB workflow.
4. **Cross-thread state inspection.** The protocol debugger exposes one thread at a time; if a bug is racy, you may need C-level instrumentation (logging at GIL acquire/release, or LLDB watchpoints in the runtime).

---

## 10. Cross-references

- `../DEBUGGING_GUIDE.md` — the full protocol debugger reference (Section "UserTalk Debugging via Protocol"); also LLDB / git-bisect / C-level patterns. This file is the UserTalk-focused subset.
- `CLAUDE_PRIMER.md` — entry-point primer; Section 5 names the debugger and points here.
- `testing_patterns.md` — yaml-test-specific debugging (the `with`-wrapper trap, `repl_mode`, eval-trap history).
- `records_and_tables.md` — when the table-walker dump from Section 6 hits a polymorphic member and you need to recurse correctly.
