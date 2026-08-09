# Agent Debugging Guide — UserTalk over the Stdio Protocol

How an AI agent (or any programmatic client) debugs UserTalk scripts by
driving `frontier-cli --protocol`. This is the operational playbook: exact
op sequences with real request/response transcripts. Every JSON line in
this document was captured from a live session against `Virgin.root`.

Full per-op contracts (all 22 ops, request/response/error shapes) live in
[`planning/gui/STDIO_PROTOCOL.md`](../planning/gui/STDIO_PROTOCOL.md). This
guide covers the debugging *workflow*; consult the protocol spec for field
details.

Automated coverage of everything in this guide:
`tests/integration/agent_debug_session_test.py` (run via
`make -C tests test-agent-debug`).

---

## 1. Session setup

Always work against a **staged copy** of the database, never the canonical
file:

```bash
STAGE="$(mktemp -d)"
cp databases/Virgin.root "$STAGE/Virgin.root"
frontier-cli --protocol --skip-startup --system-root "$STAGE/Virgin.root"
```

Transport rules:

- One JSON object per line on stdin; one per line on stdout. stderr is
  logging, not protocol data.
- Every request needs an integer `id`; the response echoes it.
- **Notifications** (`debug/suspended`, `debug/completed`) are unsolicited
  lines with `"id": null` plus an `op` field. They interleave with
  responses — a notification can arrive BEFORE the response to the request
  that caused it. Your reader must match responses by `id` and collect
  notifications separately. Real ordering from a live session:

```text
-> {"op": "script/eval", "id": 7, "params": {"expression": "thread.callScript(@system.temp.u14Main, {})"}}
<- {"id":null,"op":"debug/suspended","params":{"threadId":3,"line":1,"reason":"breakpoint","script":"system.temp.u14SumTo"}}
<- {"id":7,"result":{"value":"3","type":"long"},"success":true}
```

**Use a dedicated process for debug sessions.** Do not run debug threads on
a long-lived session shared with other work: a debug thread on a shared
executor has been observed to corrupt cached script code trees later in the
session (see the dedicated-executor notes in
`tests/integration/test_cases/protocol_contract_tests.yaml`). One debug
session = one `frontier-cli --protocol` process, shut down when done.

---

## 2. Addressing scripts

- Breakpoint/getSource `script` params take the **fully qualified dotted
  path** (`system.temp.myFunc`). A leading `@` is stripped if present.
  Unqualified names are rejected:

```text
-> {"op": "debug/getSource", "id": 5, "params": {"script": "unqualified"}}
<- {"id":5,"error":{"code":"bad_params","message":"Script path must be fully qualified (e.g. system.temp.myFunc)"},"success":false}
```

- **Guest databases** (opened with `fileMenu.open`): use the bare dotted
  path from that database's root table — **no file prefix** — even though
  address VALUES display in file-qualified form. In the transcript below,
  the breakpoint is set with `StartupTasksSuite.u14GuestDemo` while the
  same object's address renders as
  `@["/tmp/.../StartupTasks.root"].StartupTasksSuite.u14GuestDemo`:

```text
-> {"op": "script/eval", "id": 1, "params": {"expression": "fileMenu.open(\"/tmp/stage/StartupTasks.root\", true)"}}
<- {"id":1,"result":{"value":"true","type":"boolean"},"success":true}
-> {"op": "debug/setBreakpoint", "id": 4, "params": {"script": "StartupTasksSuite.u14GuestDemo", "line": 2}}
<- {"id":4,"result":{"action":"set","script":"StartupTasksSuite.u14GuestDemo","line":2},"success":true}
<- {"id":null,"op":"debug/suspended","params":{"threadId":3,"line":2,"reason":"breakpoint","script":"StartupTasksSuite.u14GuestDemo"}}
-> {"op": "debug/getLocals", "id": 6, "params": {"threadId": 3}}
<- {"id":6,"result":{"locals":[{"name":"this","value":"@[\"/tmp/stage/StartupTasks.root\"].StartupTasksSuite.u14GuestDemo","type":"address"},{"name":"x","value":"1","type":"long"}],"script":"StartupTasksSuite.u14GuestDemo","line":2},"success":true}
```

---

## 3. Installing or editing a script under test

Install scripts with `new(scriptType, ...)` + `script.newScriptObject`
through `script/eval`. Multi-line source uses **UserTalk-level escape
sequences** — `\r` between lines, `\t` for outline indentation — as
two-character backslash sequences inside the UserTalk string literal.
Literal control characters inside the literal are a parse error:

```text
-> {"op": "script/eval", "id": 2, "params": {"expression": "script.newScriptObject(\"local (items = 4)\rlocal (total = 0)...\", @system.temp.u14SumTo)"}}
<- {"id":2,"error":{"code":"script_error","message":"Script evaluation failed","location":{"script":"<eval>","line":1,"column":24,"tokenStart":23,"tokenEnd":23},"stack":[{"script":"<eval>","line":1,"column":24}]},"success":false}
```

Correct form (JSON `\\r` decodes to the two characters `\` `r`, which the
UserTalk string parser interprets):

```text
-> {"op": "script/eval", "id": 2, "params": {"expression": "script.newScriptObject(\"local (items = 4)\\rlocal (total = 0)\\rlocal (i)\\rfor i = 1 to items - 1 {\\r\\ttotal = total + i}\\rreturn (total)\", @system.temp.u14SumTo)"}}
<- {"id":2,"result":{"value":"true","type":"boolean"},"success":true}
```

Indentation-with-braces follows canonical UserTalk outline style: the loop
header ends with `{`, the (tab-indented) last body line ends with `}`.

After installing, fetch the source back with `debug/getSource` to confirm
what compiled AND to discover the line numbers you will set breakpoints on
— the stored canonical form differs from your input (semicolons appended,
brace spacing normalized):

```text
-> {"op": "debug/getSource", "id": 4, "params": {"script": "system.temp.u14SumTo"}}
<- {"id":4,"result":{"script":"system.temp.u14SumTo","lines":[{"num":1,"text":"local (items = 4);","breakpoint":false},{"num":2,"text":"local (total = 0);","breakpoint":false},{"num":3,"text":"local (i);","breakpoint":false},{"num":4,"text":"for i = 1 to items - 1{","breakpoint":false},{"num":5,"text":"total = total + i};","breakpoint":false},{"num":6,"text":"system.temp.u14Result = total;","breakpoint":false},{"num":7,"text":"return (total)","breakpoint":false}]},"success":true}
```

---

## 4. Two ways to run code under the debugger

### 4a. `debug/run` — you invoke the entry point

Use when you can call the code yourself. The thread starts **suspended
before its first statement** (`reason: "entry"`, `line: 0`); set
breakpoints, then `debug/continue`:

```text
-> {"op": "debug/setBreakpoint", "id": 1, "params": {"script": "system.temp.u14SumTo", "line": 6}}
<- {"id":1,"result":{"action":"set","script":"system.temp.u14SumTo","line":6},"success":true}
-> {"op": "debug/run", "id": 2, "params": {"expression": "system.temp.u14Main()"}}
<- {"id":2,"result":{"threadId":3,"status":"started"},"success":true}
<- {"id":null,"op":"debug/suspended","params":{"threadId":3,"line":0,"reason":"entry"}}
-> {"op": "debug/continue", "id": 3, "params": {"threadId": 3}}
<- {"id":3,"result":{"threadId":3,"status":"running"},"success":true}
<- {"id":null,"op":"debug/suspended","params":{"threadId":3,"line":6,"reason":"breakpoint","script":"system.temp.u14SumTo"}}
```

### 4b. Breakpoint + `thread.callScript` — lazy attach

Use for code dispatched on its own thread (menu commands, background
tasks — the `thread.callScript` family). Order matters: **set breakpoints
first**, then trigger the dispatch. The spawned thread attaches to the
debugger lazily when it hits a matching breakpoint; with no matching
breakpoint it runs to completion unobserved.

```text
-> {"op": "debug/setBreakpoint", "id": 6, "params": {"script": "system.temp.u14SumTo", "line": 1}}
<- {"id":6,"result":{"action":"set","script":"system.temp.u14SumTo","line":1},"success":true}
-> {"op": "script/eval", "id": 7, "params": {"expression": "thread.callScript(@system.temp.u14Main, {})"}}
<- {"id":null,"op":"debug/suspended","params":{"threadId":3,"line":1,"reason":"breakpoint","script":"system.temp.u14SumTo"}}
<- {"id":7,"result":{"value":"3","type":"long"},"success":true}
```

**Always take `threadId` from the `debug/suspended` notification.** Do not
assume thread IDs; all subsequent debug ops for this thread use that value.

Caller frames of a lazily attached thread carry no line numbers in
`debug/getStack` (the pre-attach frames are reconstructed without them);
the innermost frame always has one.

---

## 5. Inspecting a suspended thread

All inspection requires the thread to be suspended (`bad_state` otherwise).

`debug/getStack` — outermost caller first, current script last:

```text
-> {"op": "debug/getStack", "id": 4, "params": {"threadId": 3}}
<- {"id":4,"result":{"frames":[{"level":2,"script":"system.temp.u14Main"},{"level":3,"script":"system.temp.u14SumTo","line":6}]},"success":true}
```

`debug/getLocals` — the innermost frame's variables as display strings
(truncated at 255 chars). `this` is the address of the executing script:

```text
-> {"op": "debug/getLocals", "id": 5, "params": {"threadId": 3}}
<- {"id":5,"result":{"locals":[{"name":"this","value":"@system.temp.u14SumTo","type":"address"},{"name":"items","value":"4","type":"long"},{"name":"total","value":"6","type":"long"},{"name":"i","value":"3","type":"long"}],"script":"system.temp.u14SumTo","line":6},"success":true}
```

That transcript is a bug diagnosis in miniature: suspended at the `return`
line, `items` is `4` but `total` is `6` — the sum of `1..3`, not `1..4`.
The loop header (`for i = 1 to items - 1`) is off by one. Locals evidence
is usually enough; you rarely need to read the source to *localize* the
bug once you can compare expected against actual values at a breakpoint.

`debug/getSource` with `threadId` adds a `currentLine` field and per-line
`breakpoint`/`current` flags — useful to re-orient after several steps.

`debug/listThreads` shows all registered debug threads and their state:

```text
-> {"op": "debug/listThreads", "id": 8, "params": {}}
<- {"id":8,"result":{"threads":[{"threadId":3,"suspended":true,"line":0}]},"success":true}
```

---

## 6. Stepping

`debug/step` takes optional `direction`: `"over"` (default), `"into"`,
`"out"`. A `reason: "step"` suspension follows:

```text
-> {"op": "debug/step", "id": 9, "params": {"threadId": 3}}
<- {"id":9,"result":{"threadId":3,"status":"stepping"},"success":true}
<- {"id":null,"op":"debug/suspended","params":{"threadId":3,"line":4,"reason":"step","script":"system.temp.u14SumTo"}}
```

Expectations to calibrate against:

- **Local declarations are not steppable.** Stepping from line 1 of a
  script whose lines 1-3 are `local (...)` declarations lands on line 4.
  Breakpoints DO fire on declaration lines; steps skip them.
- After resuming from a suspension, breakpoints on the same line are
  suppressed until the line number changes (so a multi-node source line
  doesn't re-trigger). A loop returning to the line fires normally.
- Stepping off the end of a script completes the thread
  (`debug/completed`) rather than suspending again.

---

## 7. Evaluating expressions — what works and what doesn't

**KNOWN GAP: there is no in-frame evaluation op.** The dispatch table has
no `debug/evaluate`. `script/eval` while a thread is suspended runs in the
REPL context, NOT in the suspended frame, so frame locals are invisible to
it — captured live while suspended inside `u14SumTo`, whose frame holds
`total`:

```text
-> {"op": "script/eval", "id": 15, "params": {"expression": "defined(total)"}}
<- {"id":15,"result":{"value":"false","type":"boolean"},"success":true}
```

What you CAN do while a thread is suspended:

- `script/eval` any expression over **globals and the ODB** (the suspended
  thread yields the GIL, so evals proceed): read `system.*` state, table
  contents, verb results.
- `debug/getLocals` for the frame's variables (values as display strings).
- Conditional breakpoints: `debug/setBreakpoint` accepts a `condition` of
  the simple form `"varname op value"` (`==`, `!=`, `>`, `<`, `>=`, `<=`)
  evaluated against the frame's variable — a limited in-frame predicate.
- `debug/setWatchpoint` on a variable name to suspend when its value
  changes.

If you need a frame value in an expression, read it via `debug/getLocals`
and substitute the value client-side.

---

## 8. Resume, completion, detach

`debug/continue` resumes; normal completion emits `debug/completed` with
`success: true`; `debug/kill` wakes and terminates a thread, emitting
`success: false`:

```text
-> {"op": "debug/continue", "id": 16, "params": {"threadId": 3}}
<- {"id":16,"result":{"threadId":3,"status":"running"},"success":true}
<- {"id":null,"op":"debug/completed","params":{"threadId":3,"success":true}}
```

After completion, the thread is unregistered — further ops on its
`threadId` return `not_found`. Side effects persist in the (in-memory)
database and are readable via `script/eval`; verifying an expected side
effect is the standard "did it really finish" check.

Detach sequence at end of session:

1. `debug/clearBreakpoints` (and `debug/clearWatchpoints` if used) — the
   result reports how many were cleared; verify it matches what you set.
2. Delete any fixture objects you installed
   (`script/eval` + `delete(@system.temp.myFixture)`).
3. `shutdown` — bare ack, then the process exits.

**Breakpoints toggle.** Setting a breakpoint at an existing script+line
CLEARS it (`"action": "cleared"`). Check the `action` field of every
`setBreakpoint` response instead of assuming "set".

---

## 9. Error handling

Branch on `error.code` (stable), never on `error.message` (not part of the
contract). Codes are documented in STDIO_PROTOCOL.md section 5. Real
examples of the ones you will actually hit while debugging:

```text
-> {"op": "debug/step", "id": 1, "params": {}}
<- {"id":1,"error":{"code":"bad_params","message":"Missing 'threadId' in params"},"success":false}
-> {"op": "debug/step", "id": 2, "params": {"threadId": 999}}
<- {"id":2,"error":{"code":"not_found","message":"No debug thread with that ID"},"success":false}
-> {"op": "debug/pause", "id": 7, "params": {"threadId": 3}}
<- {"id":7,"error":{"code":"bad_state","message":"Thread 3 is already suspended"},"success":false}
-> {"op": "debug/getSource", "id": 4, "params": {"script": "noSuchTable.noSuchScript"}}
<- {"id":4,"error":{"code":"not_found","message":"Table not found in path"},"success":false}
```

**KNOWN GAP: bare runtime errors report a generic message.** A runtime
failure in `script/eval` (outside `try`/`else`) currently loses the real
error text — you get `"Script evaluation failed"` — but `error.location`
and `error.stack` are still populated and are the useful part:

```text
-> {"op": "script/eval", "id": 10, "params": {"expression": "nonexistentVariable"}}
<- {"id":10,"error":{"code":"script_error","message":"Script evaluation failed","location":{"script":"<eval>","line":1,"column":19,"tokenStart":0,"tokenEnd":19},"stack":[{"script":"<eval>","line":1,"column":19}]},"success":false}
```

To recover a real message, wrap the expression in `try`/`else` and return
`tryError` — the error text then comes back as a normal string result:

```text
-> {"op": "script/eval", "id": 1, "params": {"expression": "try { nonexistentVariable } else { return (\"ERR: \" + tryError) }"}}
<- {"id":1,"result":{"value":"ERR: Can't evaluate the expression because the name nonexistentVariable hasn't been defined.","type":"string"},"success":true}
```

---

## 10. Recipe: diagnose a bug end-to-end

The complete sequence, as exercised by
`tests/integration/agent_debug_session_test.py`:

1. Start a dedicated `--protocol` process on a staged DB copy (section 1).
2. `debug/getSource` the suspect script — confirm content, note line
   numbers (section 3).
3. `debug/setBreakpoint` where the state should be interesting: loop
   exits, return lines, just-before the failing write (section 4).
4. Trigger the code path — `debug/run` if you can call it,
   breakpoints + `thread.callScript` if it runs on its own thread
   (sections 4a/4b).
5. On each `debug/suspended`: `debug/getStack` for where-am-I,
   `debug/getLocals` for the state; compare actual against expected values
   (section 5).
6. `debug/step` to watch state evolve across statements (section 6).
7. `debug/continue` to completion; verify side effects via `script/eval`
   (section 8).
8. Clear breakpoints, delete fixtures, `shutdown` (section 8).

The diagnosis comes from comparing expected vs. actual variable values at
well-chosen suspension points — not from staring at source.
