# Debugger TUI — handoff doc (Phase A → Phase B)

Status: 2026-06-06. Author: Claude (session `f2-tui-debugger`) + JES.

This is the contract between **boxen Phase A** (substrate library, in flight in a parallel session) and **boxen Phase B** (debugger TUI as first consumer, the work that closes #691).

`OVERVIEW.md` describes what the debugger TUI looks like at a UX level. `EXECUTION_PLAN.md` describes the boxen-side API surface needed (row highlight, scroll readback, footer support, mouse mod for cmd-click, cursor visibility). **This doc describes the OTHER side of the Phase B build: the debug runtime contract the TUI must honor.**

If you are the Phase B implementer, read this BEFORE writing any debugger-side code. If you are the Phase A implementer, you can skip this — your contract is `EXECUTION_PLAN.md`. The point of this doc is that Phase B does not need to re-derive the runtime semantics from `frontier-cli/debug_handler.c` and `tests/debug_protocol_test.sh`; that was done in this session and captured here.

## What just shipped (PR #722, merged d36db3ac8 on 2026-06-06)

PR #722 unified the headless thread-spawn paths and added lazy breakpoint-driven debug attach for `thread.callScript`-spawned threads. The protocol-side debugger is now feature-complete for the cases the TUI needs:

- **Set breakpoint** by `(script_path, line)` or `(script_path, line, condition_expr)`
- **Set watchpoint** on a variable path with old/new value reporting
- **Suspended event** delivered with `{ script, line, threadId, reason }`
- **Continue** by `threadId`
- **List threads / list breakpoints / list watchpoints**
- **debug/run**: dispatch a script body in a debug-registered joinable thread, optionally suspended at entry
- **thread.callScript-spawned threads**: lazily debug-registered when a breakpoint matches and a protocol session is attached (this is the closed gap; pre-#722 menu-handler threads were invisible to the debugger)

Full verb reference: `tests/debug_protocol_test.sh` (81 tests, all green) is the executable spec.

## The "where does the debugger live" question

`#691`'s original framing was: "the plain interactive REPL has no debug transport; we need to give it one." That framing predates the boxen plan. The current answer is different:

- **The boxen-based debugger TUI IS the debug transport.** It runs in the same process as the runtime, owns the terminal in TUI mode, and talks to `debug_handler.c` through a **direct in-process transport** rather than over stdio NDJSON. This is the path that unblocks the bare-interactive-REPL use case from #691 — not by adding NDJSON to linenoise, but by replacing the entire interactive surface with the TUI when the user enters debug mode.
- **The protocol-mode debug (`--protocol`) keeps working unchanged.** Same `transport_t` plumbing, same NDJSON wire format. External clients (VS Code extension etc.) attach via protocol mode. The TUI is a sibling consumer, not a replacement.

## The transport_t contract

`frontier-cli/op_handler.h:43-46`:

```c
typedef struct {
    void *ctx;
    void (*write_line)(void *ctx, const char *json, size_t len);
} transport_t;
```

(Note: an earlier draft of this doc cited the signature as `int (*write_line)(void *ctx, const char *line)` -- 2 params, int return. That was wrong. The actual signature takes 3 params and returns void. Caught by the Phase B architect during plan verification; corrected 2026-06-06.)

That's the entire contract. The TUI builds one of these with:

- `ctx` = pointer to the TUI's debug-session state
- `write_line` = a function that takes the NDJSON line the runtime would have written to stdout and **dispatches it as a TUI event** (refresh script pane, refresh stack pane, etc.) rather than writing to a stream

The runtime never knows the transport is a TUI vs a stream. It calls `write_line` whenever it has a notification to emit (`debug/suspended`, `debug/completed`, etc.).

**Symmetric direction (TUI → runtime):** the TUI calls the same `debug/*` operations the protocol path uses, but bypasses the NDJSON-over-stdio layer. The cleanest way:

```c
/* Pseudo-code for the TUI's "user pressed F5 to continue" handler */
void tui_continue_thread(long thread_id) {
    /* directly invoke the handler that the protocol layer would invoke */
    handle_debug_continue(my_transport, thread_id);
}
```

Reuse the existing `handle_debug_*` functions in `debug_handler.c` — they already take a `transport_t *` and write notifications via it. The protocol layer just happens to dispatch to them via `op_dispatch(json_request, transport)`. The TUI dispatches to them directly with a struct-based request.

## The lazy-attach contract you MUST honor

PR #722 added a process-global `_Atomic(transport_t *) g_debug_attach_transport`. When non-NULL, breakpoint callbacks on `thread.callScript`-spawned threads will **lazily** register against it. The TUI's responsibilities:

1. **Set it on TUI-debug-mode entry:** `debug_set_attach_transport(my_tui_transport)`. The pointed-to `transport_t` must outlive any thread that could hit a breakpoint while the TUI is active. **Use a heap-allocated transport** (matching PR #722's `protocol_main` pattern) — DO NOT pass `&local_transport`.

2. **Clear and drain on TUI-debug-mode exit:** `debug_wait_lazy_threads_drained()` THEN `debug_set_attach_transport(NULL)` THEN free the transport. The drain has a 5s timeout that force-kills `fldetached` lazy threads if they don't exit voluntarily, then bails after 500ms grace. Wedged threads will still write through the transport pointer after the bail — for a TUI exiting in-process, this means the TUI's `write_line` may be invoked with a stale `ctx`. Either (a) zero out the `ctx` field before freeing the TUI session, or (b) have `write_line` defensively check a sentinel.

3. **Do NOT set `g_debug_attach_transport` while a protocol session is also active.** The atomic only holds one writer's pointer. The TUI and `--protocol` modes are mutually exclusive at any given moment. Enforce this at the CLI layer (the user cannot enter TUI debug mode if `--protocol` is in effect, and vice versa).

## The threading model the TUI lives inside

Frontier is single-GIL. Per ADR-014:

- Real POSIX threads, serialized by `frontier_gil`
- Only the GIL holder can access C globals
- Yield points: `langbackgroundtask()`, `thread.sleepTicks()`

The TUI's main loop must hold the GIL while it's processing UI events. When it blocks on `boxen_poll_event`, **it must release the GIL** so background-task threads, callback threads, and debug-target threads can run. The pattern mirrors `protocol_main`:

```c
/* TUI main loop sketch */
while (!tui_should_exit) {
    pthread_mutex_unlock(&frontier_gil);
    boxen_event_t ev;
    boxen_poll_event(&ev, /*timeout_ms=*/100);
    pthread_mutex_lock(&frontier_gil);

    handle_tui_event(&ev);
}
```

This matches what the boxen `EXECUTION_PLAN.md` already assumes (§ "the natural usage pattern for a TUI surface is: hold GIL, do UI work, yield GIL while blocking on input").

**Subtle gotcha** (caught in PR #722 round 2): if the TUI yields the GIL and a lazy callScript thread runs, that thread will overwrite the C globals `hthreadglobals`, `hashtablestack`, `currenthashtable`, `langcallbacks` with its own context, then dispose them on exit. When the TUI reacquires the GIL, those globals point to freed memory. Mitigation: **snapshot the main-thread `hglobals` before yielding the GIL; restore on reacquire**. See `frontier-cli/protocol_handler.c` lines ~395-420 for the canonical pattern (added in PR #722 commit `8f3c9eae0`).

## What the TUI actually needs to render

### Script pane
- Source text of the currently-suspended script (fetch via `debug/getSource`, returns lines for the script's compiled body)
- Current-line marker (highlight via `boxen_window_set_row_highlight` per `EXECUTION_PLAN.md`)
- Breakpoint markers in the gutter (gutter is the TUI's responsibility, not boxen's — boxen just gives cells)
- Scroll position; auto-scroll-to-current-line via `boxen_window_get_scroll` + `boxen_window_set_scroll`

### Stack/locals pane
- Current call stack (`debug/getStack` or equivalent — check `debug_handler.c` for the actual op name; if absent, this is a runtime gap to file)
- Selected frame's locals (`debug/getFrame(N)` or equivalent)
- Cmd-2-click on a script-pane identifier: resolve to a frame address via task #135's `cmd-2-click identifier resolution` runtime API (verdict GREEN per `planning/boxen/OVERVIEW.md` § Status — runtime side is ready)

### Keybind footer
- Standard: F5 continue, F10 step-over, F11 step-into, Shift-F11 step-out, F9 toggle breakpoint
- These are wired via the existing `debug/continue`, `debug/step{Over,Into,Out}`, `debug/setBreakpoint` ops

### Modal overlays
- Breakpoint configuration (set condition, set hit-count)
- Watchpoint configuration (set variable path)
- Both use `boxen` modal-window primitives per `EXECUTION_PLAN.md`

## What's already known to be missing or rough

These are **not blockers** for Phase B but will likely surface during implementation:

1. **`debug/getSource` granularity.** Returns lines, not source text with original whitespace. The TUI may want both views ("as parsed" vs "as displayed for the user's outline"). May need a runtime extension. Check `frontier-cli/debug_handler.c` for what's there now before designing.
2. **Step-into for `thread.callScript` boundaries.** Today's step-into stops at C verb boundaries. Stepping into a `thread.callScript` call **across thread spawn** is not currently supported and may not be worth implementing — see if the user finds it painful in real use first.
3. **Watchpoint UX for table-typed variables.** `debug/setWatchpoint` already supports variable paths; the old/new value reporting works for scalars. For table-typed values, the wire format truncates. Defer.
4. **Conditional breakpoint condition is a UserTalk expression evaluated under the GIL.** A pathological condition can stall the breakpoint thread arbitrarily long. The TUI should probably surface a "condition timeout" warning, not in v1.0.
5. **Lazy-attach false-negative on first statement** (verified non-issue in PR #722 round 3, but worth knowing): `tls_current_script` is populated by `debug_push_sourcecode` BEFORE the first `langdebuggercall` for any script's outermost frame, so line-1 breakpoints on callScript-spawned threads DO fire correctly. The verification trace lives in `frontier-cli/debug_handler.c` near the lazy-attach branch comment block.

## P2 polish items deferred from PR #722 (don't fix any of these in Phase B unless they bite you)

Filed as issue #723. Most relevant for Phase B awareness:
- TLS overflow counter asymmetry (only matters with >64 nested callScript frames)
- TLS update on resolution failure (defensive — operator-set breakpoints could be suppressed by an attacker triggering resolution failure)
- Bail-after-grace UAF on wedged threads (process-exit-only; bounded risk)
- Test 22 covers only happy path (the hard scenario is partially covered by Test 22b)

If Phase B touches any of these in passing, fold the fix into the PR. Don't go hunting for them.

## Files Phase B will touch

| File | Why |
|---|---|
| `frontier-cli/boxen/` | Consume the substrate library (Phase A's output) |
| `frontier-cli/debugger_tui.c` (new) | The TUI itself |
| `frontier-cli/debugger_tui.h` (new) | Public entry point |
| `frontier-cli/cli_parser.c` | Add CLI flag to enter TUI debug mode |
| `frontier-cli/main.c` | Dispatch to TUI debug mode based on flag |
| `frontier-cli/debug_handler.{c,h}` | EXPECT MINIMAL CHANGES. The transport is already abstracted; the TUI is just a new transport. If you find yourself extending `debug_handler.c` significantly, stop and surface — it likely means you're working around a runtime gap that should be its own commit. |
| `tests/debugger_tui_tests.c` or `tests/debugger_tui_test.sh` (new) | TUI-side tests. Use `boxen` test helpers (Phase A should ship them) for headless TUI testing without a real terminal. |

## What Phase B should NOT touch

- `ws_server.c` — WS-attached lazy-attach is intentionally deferred (UAF risk on per-message stack transport). Don't add WS-side TUI integration. If a user wants debugging over WS, they use the existing WS protocol path.
- `protocol_handler.c` — its lazy-attach setter is correct. Don't refactor.
- `databases/Virgin.root` — the TUI is a CLI-side feature. If you find yourself wanting to add UserTalk verbs to drive the TUI, surface and ask. The TUI is the consumer, not part of the kernel.

## Cross-references

- `docs/THREAD_DEBUG_ATTACH_PLAN.md` — original plan; both PRs now SHIPPED
- `tests/debug_protocol_test.sh` — executable spec for the runtime debug surface (81 tests)
- `frontier-cli/debug_handler.{c,h}` — runtime
- `frontier-cli/protocol_handler.c` — reference transport implementation (NDJSON stdio); the TUI is the second consumer
- `frontier-cli/headless_spawn.{c,h}` — unified thread-spawn primitive added by PR #722
- `planning/boxen/OVERVIEW.md` — substrate library motivation + UX
- `planning/boxen/EXECUTION_PLAN.md` — substrate library API surface + Phase A/B sequencing
- PR #722 review history (rounds 1-4) — captures the lifetime-contract derivations; consult if you need to re-derive transport ownership, drain semantics, or `fldetached` rationale from first principles
