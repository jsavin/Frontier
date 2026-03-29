# Plan: Protocol-Based UserTalk Debugger

**Status:** Planning
**Author:** Claude + Jake
**Date:** 2026-03-28

---

## Motivation

AI agents working on Frontier need to debug UserTalk scripts. Currently the only options are `msg()` calls or dropping to LLDB for C-level tracing. A protocol-based debugger would let agents set breakpoints, step through code, and inspect variables — the same capabilities the legacy GUI debugger provided, but accessible over the NDJSON protocol.

## Research Findings

### Existing Infrastructure (Almost Everything We Need)

**The interpreter already has debug hooks.** `langevaluate.c` calls `langdebuggercall(hnode)` at 8 strategic points in the interpreter loop. This callback receives the current AST node being executed.

**Three debugger backends exist:**
1. **Classic UI debugger** (`scripts.c:2323`) — Full stepping, breakpoints, pause/resume. State in `tydebuggerrecord`.
2. **OSA debugger** (`osacomponent.c`) — Hooks for external debuggers via AppleScript hosting.
3. **Headless no-op** (`langstartup.c:1167`) — `cb_noop_treenode()` returns true. This is what we replace.

**Breakpoints are stored on outline nodes.** Each `hdlheadrecord` has a `flbreakpoint` bit (op.h:86). This flag is preserved through pack/unpack (oppack_v7.c saves it as flag `0x0200`). Breakpoints persist with the script in the ODB.

**The call stack is an ODB table.** `system.compiler.stack` contains named subtables (`"level 01"`, `"level 02"`, etc.) with local variables at each frame. Created/destroyed by `scriptpushtable()`/`scriptpoptable()`.

**Thread state is in `system.compiler.threads`.** Per-thread globals in `tythreadglobals` include `hprocess`, `herrornode`, `flscriptrunning`, `fllangerror`, `flthreadkilled`.

**The pause/resume pattern already yields the GIL.** The legacy debugger's `scriptdebuggereventloop()` uses `processsleep(10)` when threads are available, allowing other threads to run while a script is paused at a breakpoint.

### Debug Mode is Per-Thread

Debug mode is opt-in per execution. Breakpoints on scripts are ignored unless the script is running in debug mode. Each thread has its own debugger state, so multiple threads can be debugged simultaneously (the legacy app didn't expose this, but the architecture supports it).

### What We Need to Build

The classic debugger's event loop waits for GUI button clicks. We replace that with waiting for protocol messages. The flag-based model maps naturally:

| Legacy GUI | Protocol Equivalent |
|-----------|-------------------|
| Click "Step" button | `{"op":"debug/step","params":{"direction":"over"}}` |
| Click "Step Into" button | `{"op":"debug/step","params":{"direction":"into"}}` |
| Click "Step Out" button | `{"op":"debug/step","params":{"direction":"out"}}` |
| Click "Go" button | `{"op":"debug/continue"}` |
| Click "Kill" button | `{"op":"debug/kill"}` |
| View Locals window | `{"op":"debug/getLocals"}` |
| Breakpoint hit notification | `{"id":null,"op":"debug/suspended","params":{"threadId":3,"line":12}}` |

---

## Architecture

### Components

```
Protocol Client (Claude/AI agent)
    │
    ▼
Protocol Handler (op_handler.c / new debug_handler.c)
    │
    ├─ debug/eval       → Start script in debug mode
    ├─ debug/step       → Set step direction, resume
    ├─ debug/continue   → Clear stepping, resume
    ├─ debug/kill       → Kill script
    ├─ debug/setBreakpoint → Toggle breakpoint on script line
    ├─ debug/getLocals  → Read system.compiler.stack
    ├─ debug/getStack   → Read call stack frames
    └─ debug/getSource  → Read script source with current line
    │
    ▼
Debugger Callback (replaces cb_noop_treenode)
    │
    ├─ Check: is this thread in debug mode?
    ├─ Check: breakpoint on current line?
    ├─ Check: stepping and step condition met?
    │
    ├─ If suspending:
    │   ├─ Send debug/suspended notification to protocol client
    │   ├─ Set flscriptsuspended = true
    │   └─ while (flscriptsuspended) { processsleep(10); }
    │   │   (yields GIL, other threads + protocol handler run)
    │   └─ Resume when protocol command clears flscriptsuspended
    │
    └─ If continuing: return true (no pause)
```

### Threading / GIL Interaction

1. Script thread hits breakpoint → sets `flscriptsuspended = true`
2. Script thread enters sleep loop → yields GIL every 10ms
3. Protocol handler thread (or main thread) processes `debug/step` command
4. Command handler sets step direction, clears `flscriptsuspended`
5. Script thread wakes up, sees `flscriptsuspended == false`, resumes

**Critical:** The protocol handler must be able to process messages while a script thread is suspended. Since the suspended thread yields the GIL, and the protocol handler runs on the main thread (or its own thread), this works naturally.

### Multi-Thread Debugging

Each thread's debug state is independent:
- Per-thread `tydebuggerrecord` (or equivalent state in `tythreadglobals`)
- Protocol commands include `threadId` to target a specific thread
- Notifications include `threadId` so the client knows which thread is paused
- Multiple threads can be simultaneously paused at breakpoints

---

## Protocol Operations

### debug/eval — Execute Script in Debug Mode

```json
{"op":"debug/eval","id":1,"params":{
  "expression":"mainResponder.respond(\"GET / HTTP/1.1\")"
}}
```

Like `script/eval` but enables debug mode on the executing thread. Returns when the script completes (or is killed).

### debug/step — Step Execution

```json
{"op":"debug/step","id":2,"params":{
  "threadId":3,
  "direction":"over"
}}
```

Directions: `"over"` (next line, same depth), `"into"` (descend into calls), `"out"` (return to caller).

### debug/continue — Resume Execution

```json
{"op":"debug/continue","id":3,"params":{"threadId":3}}
```

Clears stepping mode. Script runs freely until next breakpoint or completion.

### debug/kill — Kill Script

```json
{"op":"debug/kill","id":4,"params":{"threadId":3}}
```

Sets `flscriptkilled = true`. Script aborts at next yield point.

### debug/setBreakpoint — Toggle Breakpoint

```json
{"op":"debug/setBreakpoint","id":5,"params":{
  "script":"@system.verbs.builtins.sys.openUrl",
  "line":3
}}
```

Toggles `flbreakpoint` on the outline node at the specified line. Breakpoint persists with the script (saved to ODB).

### debug/getLocals — Inspect Local Variables

```json
{"op":"debug/getLocals","id":6,"params":{"threadId":3}}
```

Response:

```json
{"id":6,"result":{
  "locals":{"url":"https://example.com","ix":3,"found":true},
  "level":2
}}
```

Reads the current frame from `system.compiler.stack`.

### debug/getStack — Get Call Stack

```json
{"op":"debug/getStack","id":7,"params":{"threadId":3}}
```

Response:

```json
{"id":7,"result":{"frames":[
  {"level":1,"script":"@system.startup.startupScript","line":103},
  {"level":2,"script":"@mainResponder.respond","line":45},
  {"level":3,"script":"@mainResponder.dispatch","line":12}
]}}
```

### debug/getSource — Get Script Source with Current Line

```json
{"op":"debug/getSource","id":8,"params":{"threadId":3}}
```

Response includes script text and the current line number for display.

### Unsolicited Notification: debug/suspended

```json
{"id":null,"op":"debug/suspended","params":{
  "threadId":3,
  "script":"@mainResponder.respond",
  "line":12,
  "reason":"breakpoint"
}}
```

Sent when a thread pauses (breakpoint hit, step completed, or error).

---

## Implementation Phases

### Phase 1: Debugger Callback + Suspend/Resume (MVP)

1. Create `debug_handler.c` with protocol op dispatch
2. Replace `cb_noop_treenode` with a protocol-aware debugger callback
3. Implement `debug/eval` — run script in debug mode
4. Implement suspend loop with GIL yield (`processsleep`)
5. Implement `debug/continue` and `debug/kill`
6. Test: run script, kill it via protocol

### Phase 2: Stepping

1. Implement `debug/step` (over/into/out) using `stepdir` and `sourcesteplevel`
2. Implement `debug/suspended` notifications
3. Test: step through a simple script line by line

### Phase 3: Breakpoints

1. Implement `debug/setBreakpoint` — toggle `flbreakpoint` on outline nodes
2. Wire breakpoint checking into the callback
3. Test: set breakpoint, run script, verify it suspends at the right line

### Phase 4: Inspection

1. Implement `debug/getLocals` — read `system.compiler.stack` current frame
2. Implement `debug/getStack` — enumerate all frames
3. Implement `debug/getSource` — read script text + current line
4. Test: pause at breakpoint, inspect variables, verify values

### Phase 5: Multi-Thread Debugging

1. Per-thread debug state management
2. Thread-scoped protocol commands
3. Test: two threads debugging simultaneously

---

## Key Files to Modify

| File | Change |
|------|--------|
| `frontier-cli/debug_handler.c` | New: protocol op dispatch for debug/* |
| `frontier-cli/debug_handler.h` | New: declarations |
| `frontier-cli/op_handler.c` | Route debug/* ops to debug_handler |
| `Common/source/langstartup.c` | Replace `cb_noop_treenode` with protocol callback |
| `Common/source/scripts.c` | Reference: existing debugger logic to replicate |
| `Common/headers/processinternal.h` | May need debug state fields in `tythreadglobals` |

---

## Open Questions

1. **Should `debug/eval` block the protocol channel?** If so, the client can't send step/continue commands. Alternative: `debug/eval` returns immediately with a thread ID, and the script runs on a spawned thread.

2. **Breakpoint storage vs. protocol-only breakpoints:** The legacy model stores breakpoints in the ODB (persistent). Should we also support ephemeral breakpoints that don't modify the database?

3. **Error breakpoints:** Should we support "break on error" mode? The infrastructure exists (`fllangerror` flag).

4. **Conditional breakpoints:** Not in the legacy debugger, but would be valuable for AI agents. Could evaluate a UserTalk expression at each breakpoint.

---

## References

- Legacy debugger: `/Users/jake/dev/tedchoward/Frontier/Common/source/scripts.c` (lines 2233-2518)
- Interpreter callback sites: `Common/source/langevaluate.c` (8 call sites)
- Debugger state: `tydebuggerrecord` in `scripts.c` (lines 120-177)
- Call stack: `system.compiler.stack` via `scriptpushtable()`/`scriptpoptable()` in `scripts.c`
- Thread globals: `tythreadglobals` in `Common/headers/processinternal.h`
- Breakpoint flag: `flbreakpoint` in `Common/headers/op.h:86`
