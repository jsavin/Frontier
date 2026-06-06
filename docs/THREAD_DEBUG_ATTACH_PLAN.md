# Thread-spawn unification + debugger attach for menu-handler threads

Status: IN PROGRESS. Author: Claude + JES, 2026-06-01.
Decision: JES chose to do this BEFORE fixing File>Open, because it removes
debugging churn structurally for all future menu-handler work and also fixes
a latent #706 hang in `debug/run`.

## Build sequence status (2026-06-06)

- **PR 1 (#706 isolated `debug/run` fix)**: SHIPPED as **PR #692**
  (commit `ec4483270`, 2026-06-01). `debug_handler.c::debug_thread_entry`
  now uses `newclearhandle(sizeof(tytablestack), ...)` + `hcurrenthashtable
  = roottable` (current anchors: ~:1126 and ~:1139). Regression guard at
  `tests/debug_protocol_test.sh:833-876` ("Test 20: debug/run roots the
  spawned thread at roottable").
- **PR 2 (spawn unification + lazy attach)**: SHIPPED as **PR #722**
  (commit `d36db3ac8`, 2026-06-06). `headless_spawn_script_thread` in
  new `frontier-cli/headless_spawn.{c,h}` is now the single spawn primitive;
  both `debug/run` and `thread.callScript`/`evaluate` go through it. Lazy
  breakpoint-driven attach via `tls_current_script` + atomic
  `g_debug_attach_transport` + `debug_register_thread` in the breakpoint
  callback. Heap-allocated transport + `g_lazy_attached_count` drain
  counter + 5s drain timeout in `protocol_main`. callScript threads
  carry `fldetached`; `debug_kill_all_threads`/`join_all` skip detached.
  Test 21 (lazy attach RED-then-GREEN), Test 21b (zero-breakpoint hot
  path), Test 22 (session close after thread completes), Test 22b
  (session close while thread suspended -> drain timeout fires).
  4 rounds of /gate fix-loop; 8 P0/P1 items addressed; 9 P2 polish items
  deferred to follow-up issue #723.

## Status

**Both PRs SHIPPED. Menu-handler threads are now debuggable when a
protocol debug client is attached.** File>Open is the next downstream
work: a normal debuggable bug now that the structural blocker is gone.

#691 itself remains OPEN for the broader question of bare-interactive
REPL debugging without a protocol/WS client. That's likely subsumed by
the boxen-based TUI debugger plan (`planning/boxen/OVERVIEW.md`), where
the TUI itself becomes the debug UI and no transport is needed in the
classic sense.

## Problem

Headless menu commands (File/Edit/View leaves in the palette menubar) dispatch
through:

```
palette leaf -> meuserselected_headless (synchronous, REPL thread)
             -> runFileMenuScript(item)
             -> thread.callScript(@commands.[item], {})   <-- SPAWNS a thread
             -> commands.[item] runs on a detached POSIX thread
```

That spawned thread is **structurally invisible to the headless debugger**.
You cannot set a breakpoint in `commands.open` and have it fire when the item
is triggered from the menu. Debugging menu handlers today means PTY
screen-scraping instead of breakpoints.

## Root cause (verified, with anchors)

There are two separate thread-spawn paths that duplicate ~90% of their
boilerplate but diverge in the parts that matter:

| | `debug/run` | `thread.callScript` (menu path) |
|---|---|---|
| Handler | `debug_handler.c` ~:1050-1207 | `headless_thread_verbs.c` `headless_thread_callscript` ~:712-862 |
| Entry fn | `debug_thread_entry` :925-983 | `thread_entry_point` :237-298 |
| Debug-registered? | yes (`debug_register_thread` :1131) | **no** |
| `(**hglobals).debugstate` set? | yes (:949) | **no** (stays calloc-zeroed NULL) |
| Table stack | **old deep-copy** `newfilledhandle` (:1116), rooted `currenthashtable` (:1128) | `newclearhandle` fresh, rooted `roottable` (:800,:809) -- PR #689 fix |
| pthread | JOINABLE (:1179), stores pthread_id | DETACHED (:860) |
| Run dispatch | `langruncode(hcode)` (:976) | `langrunscriptcode(...)` (:256) |

The breakpoint callback (`debug_handler.c` ~:438-540) reads
`state = (**hthreadglobals).debugstate` (:447) and **returns early if
`state == NULL`** (:449). callScript threads have NULL debugstate, so they are
no-ops in the callback. Confirmed: callScript spawn never calls
`debug_register_thread` and never sets `debugstate`.

Bonus defect: `debug/run` at :1116 still uses the pre-#689 deep-copy of the
caller's table stack. Debugging a deeply-nested dispatch (exactly the menu
chain: leaf -> runFileMenuScript -> commands.open) would reproduce the #706
stack-overflow / GIL-starvation hang. PR #689 only converted the
callScript/evaluate paths.

## The hard constraint: interactive REPL has no debug transport

`tydebugstate` reports breakpoint hits over a `transport_t`
(`op_handler.h:43-46` = `{ctx, write_line}`). Where transports come from:

- **Protocol mode**: `protocol_handler.c:156-159` builds
  `{ctx=NULL, write_line=stdio_write_line}` (NDJSON to stdout), threaded
  through `op_dispatch` -> `handle_debug_run` (`op_handler.c:943`) and stored
  in the state at register time. All `debug_send_*` write via
  `state->transport->write_line`.
- **WebSocket-attached REPL**: `repl.c:3846` `ws_server_handle_events` calls
  `op_dispatch` with a ws-backed transport. Debug works here.
- **Plain interactive REPL (linenoise, no WS): NO transport exists.** The
  linenoise path (`repl.c:3850`) never builds a `transport_t`. A breakpoint
  hit on this thread has no channel to report over; `debug_send_suspended`
  would have nowhere to write.

**Design consequence:** making callScript threads "debuggable" only has meaning
when a protocol or WS debug client is attached. In a bare interactive session
there is nothing to attach a debugger *with*. So the feature is: "menu-handler
threads are debuggable **when a debug client is connected**," not "always."

## Recommended design

### 1. Unify the spawn paths

Introduce one primitive:

```c
// run_spec selects langrunscriptcode (named handler) vs langruncode (anon block)
// debug_opts == NULL  -> fire-and-forget: detached, no register, no suspend
// debug_opts != NULL  -> joinable, register, optional start_suspended
boolean headless_spawn_script_thread(Handle hcode,
                                     const thread_run_spec *run_spec,
                                     const thread_debug_opts *debug_opts /*nullable*/,
                                     long *out_threadid);
```

Absorbs the common sequence (thread record alloc, `headless_new_threadglobals`,
idthread/rec wiring, table-stack handle alloc, `hcurrenthashtable`,
`headless_register_thread`, params packaging, `pthread_attr_init` + spawn +
error cleanup). Diverges only on the five orthogonal flags above -- all
parameters, not tangled control flow. Per the trace, this is clean.

While unifying: **default the table stack to the `newclearhandle` + `roottable`
form** and make `debug/run` adopt it. That fixes `debug/run`'s #706 exposure
for free.

### 2. Lazy, breakpoint-driven attach (don't register every menu click)

We do NOT want every menu click to register a debug thread. Instead:

- Spawn callScript threads normally (detached, no debugstate).
- Add a process-global `g_debug_attach_transport` (set when a protocol/WS debug
  session is active; NULL otherwise).
- In the breakpoint callback, the existing fast-path `g_has_breakpoints`
  (:515) already guards the common (no-breakpoints) case at near-zero cost.
  When `state == NULL && g_has_breakpoints && <breakpoint matches> &&
  g_debug_attach_transport != NULL`: lazily `debug_register_thread(threadid,
  g_debug_attach_transport)`, store into `(**my_hglobals).debugstate`, set
  `flsuspended`, and fall into the EXISTING wait loop (the wait loop at
  ~:864-882 is transport-agnostic -- it only polls flsuspended/flkill + yields
  the GIL; transport is only needed to *notify*, which we now have).

This keeps the zero-breakpoint menu path exactly as cheap as today (one relaxed
atomic load), and only pays registration cost when a breakpoint actually
matches AND a debug client is attached.

### 3. Initial-suspend stays debug/run-only

`debug_thread_entry`'s forced initial suspend (:955-983) is the only behavioral
divergence beyond debugstate wiring. It becomes
`if (debug_opts && debug_opts->start_suspended)`. callScript threads pass
`start_suspended = false` (fire-and-forget), so they never pause on entry --
only on a matching breakpoint via the lazy path above.

## What this unblocks

After this lands, File>Open is a normal debuggable bug: attach a protocol/WS
debug client, breakpoint `commands.open`, trigger File>Open from the menu, and
step through the `file.getFileDialog` -> spawned-thread-stdin conflict (the
actual File>Open defect, separately documented). No more PTY screen-scraping.

## Build sequence (when approved) -- two PRs

### PR 1: isolated debug/run #706 fix

1. TDD red: deep-nesting `debug/run` test mirroring the #706 reproduction
   (dispatch a script via `debug/run` from many scope frames deep; assert it
   does not hang). Today this overflows on the deep-copied stack.
2. Convert `debug_handler.c:1116`/:1128 from `newfilledhandle` deep-copy +
   `currenthashtable` to `newclearhandle` + `roottable`.
3. Red test green; existing debug/run tests stay green.
4. Full unit + integration suites green. No Virgin.root change expected.

### PR 2: spawn unification + lazy attach (builds on PR 1)

1. TDD red: protocol-mode integration test -- set a breakpoint in a script,
   dispatch it via `thread.callScript`, assert the debugger suspends + reports
   the hit over the protocol transport. (Today this would never fire.)
2. Extract `headless_spawn_script_thread` from the two existing paths; prove
   parity (existing callScript + debug/run tests stay green).
3. Add `g_debug_attach_transport` + lazy attach in the breakpoint callback.
4. Make the red test from step 1 green.
5. Full unit + integration suites green. No Virgin.root change expected (pure
   C + C/protocol tests) -- standing gate: if Virgin.root is touched, pause
   for live-test approval.

Bare interactive REPL debugging is OUT OF SCOPE for both PRs -- tracked as
**issue #691** (P1).

## Decisions (JES, 2026-06-01)

- **Scope = protocol + WS only** for this work. Bare interactive REPL
  debugging is deferred and tracked as a P1 follow-up: **issue #691**
  (provide a debug transport for bare linenoise sessions).
- **`debug/run`'s `newclearhandle` / #706 fix ships as its own separate PR**,
  not bundled into the unification. It is an isolated fix and JES prefers it
  decoupled. The unification PR then builds on top of (or alongside) it.

This splits the build sequence below into two PRs:

- **PR 1 (isolated #706 fix)**: convert `debug/run`'s table-stack copy from
  the pre-#689 `newfilledhandle` deep-copy (`debug_handler.c:1116`, rooted
  `currenthashtable` :1128) to `newclearhandle` rooted at `roottable`,
  matching the callScript/evaluate paths. Add a deep-nesting `debug/run` test
  mirroring the #706 reproduction.
- **PR 2 (unification + lazy attach)**: extract `headless_spawn_script_thread`,
  add `g_debug_attach_transport` + lazy breakpoint-driven attach, make
  callScript threads debuggable when a protocol/WS client is connected.
