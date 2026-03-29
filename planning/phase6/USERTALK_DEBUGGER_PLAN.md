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

### Session Model

The REPL and protocol are the first two "sessions" — separate entry points that can run concurrently:

| Session | Entry Point | Description |
|---------|------------|-------------|
| 0 | Internal | System/startup threads (no owner) |
| 1 | REPL | Local interactive user |
| 2 | Protocol (stdio) | AI agent or automation |
| 3+ | WebSocket connections | Future: additional users/agents |

Each session has "root access" — full read/write to the ODB. Session identity is tracked via `tythreadglobals.sessionId` on every thread. For now this is informational; later it becomes an access control hook for multi-user isolation.

Threads spawned by a session inherit its `sessionId`. This means:
- REPL commands execute on session 1 threads
- Protocol `script/eval` and `debug/eval` spawn session 2 threads
- Each WebSocket connection gets its own session ID

### Per-Thread Stack Frames

The legacy `system.compiler.stack` uses flat naming (`"level 01"`, `"level 02"`) — only one thread can have stack frames at a time. For multi-thread debugging, we scope frames under per-thread subtables:

```
system.compiler.stack
    thread_3
        level 01  →  {locals for frame 1}
        level 02  →  {locals for frame 2}
    thread_5
        level 01  →  {locals for frame 1}
```

This requires modifying `scriptpushtable()`/`scriptpoptable()` to create/destroy frames under a thread-specific subtable. Single-thread execution still works (just one subtable). Scripts that read `system.compiler.stack` see the same data, one level deeper.

---

## Protocol Operations

### debug/eval — Execute Script in Debug Mode

```json
{"op":"debug/eval","id":1,"params":{
  "expression":"mainResponder.respond(\"GET / HTTP/1.1\")"
}}
```

Non-blocking: spawns the script on a new thread with debug mode enabled. Returns immediately with a thread ID. The script runs until it hits a breakpoint, completes, or is killed.

```json
{"id":1,"result":{"threadId":3,"status":"running"}}
```

The client receives `debug/suspended` notifications when the script pauses.

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

### debug/pause — Interrupt a Running Thread

```json
{"op":"debug/pause","id":4,"params":{"threadId":3}}
```

Interrupts a running thread (debug mode or not) and suspends it at the next statement. Sets an interrupt flag that the debugger callback checks. Useful for debugging infinite loops or long-running scripts. The thread suspends at the next `langdebuggercall` hook point and sends a `debug/suspended` notification with `"reason":"interrupted"`.

If the thread is not in debug mode, `debug/pause` promotes it to debug mode before suspending.

### debug/kill — Kill Script

```json
{"op":"debug/kill","id":5,"params":{"threadId":3}}
```

Sets `flscriptkilled = true`. Script aborts at next yield point.

### debug/setBreakpoint — Set or Clear Breakpoint

```json
{"op":"debug/setBreakpoint","id":5,"params":{
  "script":"@mainResponder.respond",
  "line":5,
  "persistent":false
}}
```

Two types of breakpoints:

- **Session-scoped** (`"persistent":false`, the default): Stored in the session's breakpoint list in memory. Only triggers for threads in this session. Disappears when the session ends. Does not modify the ODB.
- **Persistent** (`"persistent":true`): Stored on the outline node's `flbreakpoint` flag in the ODB. Survives across sessions. Visible to all sessions. Modifies the database.

**Persistent breakpoints in the ODB are ignored by the headless debugger** unless explicitly loaded into the session via `debug/loadBreakpoints`. This prevents agents from hitting legacy breakpoints left over from years ago.

### debug/listBreakpoints — List All Breakpoints

```json
{"op":"debug/listBreakpoints","id":6,"params":{}}
```

Returns both session and persistent breakpoints, distinguished by type.

### debug/getSource — View Script with Line Numbers

```json
{"op":"debug/getSource","id":7,"params":{
  "script":"@mainResponder.respond",
  "threadId":3
}}
```

Returns script source with line numbers, breakpoint markers, and (if `threadId` specified and the thread is suspended) the current execution line. Works both inside and outside debug sessions.

```json
{"id":7,"result":{
  "script":"@mainResponder.respond",
  "currentLine":3,
  "lines":[
    {"num":1,"text":"on respond (adrParamTable)","breakpoint":false},
    {"num":2,"text":"\tlocal (method = adrParamTable^.method)","breakpoint":false},
    {"num":3,"text":"\tlocal (path = adrParamTable^.path)","breakpoint":false,"current":true},
    {"num":4,"text":"\tlocal (adrpage)","breakpoint":false},
    {"num":5,"text":"\tif not mainResponder.dispatch(method, path, @adrpage)","breakpoint":true}
  ]
}}
```

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

## REPL Commands

The REPL provides the same capabilities as the protocol, with human-friendly commands. The REPL prompt changes when a debug session is active.

### Source Viewing

```
[root]> /source @mainResponder.respond
  1: on respond (adrParamTable) {
  2:     local (method = adrParamTable^.method)
  3:     local (path = adrParamTable^.path)
  4:     local (adrpage)
● 5:     if not mainResponder.dispatch(method, path, @adrpage) {
  6:         return mainResponder.notFound(path)}
  7:     return mainResponder.renderPage(adrpage)}
```

`/source` works both inside and outside debug mode. Shows line numbers and breakpoint markers (`●`). In debug mode, shows current execution line (`>`).

### Breakpoints

```
/break @mainResponder.respond 5              # set session-scoped breakpoint
/break @mainResponder.respond 5 --save       # set persistent breakpoint (saved to ODB)
/break @mainResponder.respond 5              # toggle: clears if already set
/breaks                                      # list all breakpoints
/break clear                                 # clear all session breakpoints
```

`/breaks` output distinguishes session vs persistent:

```
[root]> /breaks
  @mainResponder.respond line 5   (session)
  @mainResponder.respond line 12  (persistent)
  @mainResponder.dispatch line 3  (session)
```

### Debug Session

```
[root]> /debug mainResponder.respond("GET / HTTP/1.1")
[debug thread 3] Suspended at @mainResponder.respond line 5 (breakpoint)
[debug 3]> /step                              # step over
[debug thread 3] line 6: return mainResponder.notFound(path)
[debug 3]> /step into                         # step into function call
[debug thread 3] Suspended at @mainResponder.notFound line 1
[debug 3]> /step out                          # step out to caller
[debug thread 3] Suspended at @mainResponder.respond line 6
[debug 3]> /locals                            # inspect local variables
  method = "GET"
  path = "/"
  adrpage = @websites.default.index
[debug 3]> /stack                             # view call stack
  1: @mainResponder.respond line 6
[debug 3]> path                               # evaluate expression in debug context
"/"
[debug 3]> /go                                # continue to next breakpoint or completion
```

### Interrupting a Running Thread

```
[root]> /pause 3                              # interrupt thread 3, suspend at next statement
[debug thread 3] Suspended at @worker.process line 47 (interrupted)
[debug 3]> /source
> 47:     loop                                # infinite loop found!
  48:         x = x + 1
[debug 3]> /kill                              # kill the runaway thread
[debug thread 3] Completed: true
[root]>
```

### Multiple Debug Threads

```
[debug 3]> /debug thread.callScript(@worker.process)
[debug thread 5] Running...
[debug 3]> /switch 5                          # switch to thread 5
[debug 5]> /locals
  ...
[debug 5]> /switch 3                          # switch back
[debug 3]>
```

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
| `Common/headers/processinternal.h` | Add `sessionId` + debug state fields to `tythreadglobals` |
| `Common/source/scripts.c` | Modify `scriptpushtable`/`scriptpoptable` for per-thread stack subtables |

---

## Security Considerations (Multi-User Future)

### Current State: Trusted Sessions

Both the REPL and protocol sessions have full access to the ODB and all threads. This is appropriate for the current single-developer use case.

### Design for Future Isolation

`tythreadglobals.sessionId` tracks which session spawned each thread. This is a general-purpose field — not debug-specific — that applies to all threads (script execution, background tasks, debug sessions).

When multi-user support is needed, `sessionId` becomes the enforcement hook:
- **Thread access control:** Debug/kill/inspect commands restricted to threads owned by the requesting session
- **Session-scoped breakpoints:** Ephemeral breakpoints that only trigger for the owning session's threads (avoid "my breakpoint affects your execution")
- **ODB access control:** Future concern — would require per-table or per-path ACLs, which is a much larger architectural change

### Principle

Design the data model now (sessionId on every thread), enforce access control later. All sessions are "root" for now.

---

## Open Questions

1. ~~**Should `debug/eval` block the protocol channel?**~~ **Resolved:** Non-blocking. Returns immediately with thread ID; script runs on spawned thread.

2. **Breakpoint storage vs. session-scoped breakpoints:** The legacy model stores breakpoints in the ODB (persistent, visible to all sessions). Should we also support ephemeral breakpoints that only exist for the current session? This avoids one user's breakpoints affecting another's execution.

3. **Error breakpoints:** Should we support "break on error" mode? The infrastructure exists (`fllangerror` flag). Would be valuable for AI debugging — pause at the first error instead of adding try/catch everywhere.

4. **Conditional breakpoints:** Not in the legacy debugger, but would be valuable for AI agents. Could evaluate a UserTalk expression at each breakpoint and only suspend if it returns true.

---

## References

- Legacy debugger: `/Users/jake/dev/tedchoward/Frontier/Common/source/scripts.c` (lines 2233-2518)
- Interpreter callback sites: `Common/source/langevaluate.c` (8 call sites)
- Debugger state: `tydebuggerrecord` in `scripts.c` (lines 120-177)
- Call stack: `system.compiler.stack` via `scriptpushtable()`/`scriptpoptable()` in `scripts.c`
- Thread globals: `tythreadglobals` in `Common/headers/processinternal.h`
- Breakpoint flag: `flbreakpoint` in `Common/headers/op.h:86`
