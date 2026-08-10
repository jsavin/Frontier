# TUI Debugger Architecture

**Audience:** Frontier contributors working on the TUI, boxen substrate, or the debugger runtime.
**Last updated:** 2026-06-07

This document describes how `--debug-tui` is wired into the rest of `frontier-cli`. For the user-facing keybinds and workflow, see [`docs/TUI_DEBUGGER_GUIDE.md`](TUI_DEBUGGER_GUIDE.md).

The TUI is a Phase B deliverable, layered on top of:

- **boxen** (Phase A) — portable terminal window manager on `termbox2`.
- **Lazy-attach debugger transport** (PR #722) — the runtime emits debugger events through a global atomic transport pointer (`g_debug_attach_transport`) without an active session.

The TUI is a **consumer** of both. Its job is to (a) install itself as the lazy-attach transport, (b) translate boxen events into debugger protocol ops, and (c) render incoming protocol responses into the pane layout.

---

## File layout

| Path | Role |
|------|------|
| `frontier-cli/debugger_tui.h` | Public entry: `debugger_tui_main(opts)`. |
| `frontier-cli/debugger_tui_internal.h` | `tui_state_t` struct, internal helpers exported to tests. |
| `frontier-cli/debugger_tui.c` | All logic (~2600 lines). State machine, draw callbacks, input callbacks, transport callback, dispatch helpers. |
| `frontier-cli/cli_parser.c` | `--debug-tui` flag, mutual exclusion with `--protocol`. |
| `frontier-cli/main.c` | Dispatch: if `opts.debug_tui` is set, calls `debugger_tui_main(opts)` and returns its exit code. |
| `tests/debugger_tui_tests.c` | Unit tests (~750 cases). Uses `DEBUGGER_TUI_OMIT_MAIN` to exclude GIL/runtime symbols. |
| `tests/debugger_tui_test.sh` | Shell-level smoke tests (9 cases). |
| `tests/debugger_tui_pty_test.py` | PTY-driven lifecycle tests (4 cases). |

---

## Mutual exclusion with `--protocol`

Both modes own the same global transport slot and the same stdio. They cannot coexist. The check is in `cli_parser.c` at argument parse time; the binary exits with a usage error before any runtime initialization.

This isn't symmetric with regular `--protocol` though: the TUI is **TTY-only** (termbox2 opens `/dev/tty` directly), whereas `--protocol` runs over piped stdio. So the practical effect is "interactive vs. machine-driven" — pick one per session.

---

## State

All TUI state is heap-allocated in a single `tui_state_t` struct (`debugger_tui_internal.h`). `debugger_tui_main` allocates and zero-inits it via `calloc`, hands its address to the transport callback's `ctx` field, and frees it on exit.

Key fields (non-exhaustive):

| Field | Role |
|-------|------|
| `debug_state` | `TUI_DEBUG_RUNNING` or `TUI_DEBUG_SUSPENDED`. Gates all keybinds. |
| `transport` | The `transport_t` registered as the lazy-attach transport. Its `ctx` points back to the state. |
| `script_path`, `script_lines`, `script_line_count` | Currently loaded source from `debug/getSource`. |
| `stack_frames`, `locals` | Last-rendered stack and frame-local snapshots from `debug/getStack`, `debug/getLocals`. |
| `script_win`, `state_win`, `footer_win` | Boxen windows for the three regions. |
| `condition_modal_win`, `watchpoint_modal_win` | Modal windows; non-NULL when open. |
| `eval_pane_active`, `eval_input_buf`, `eval_history[]` | `:` scratch-eval pane state. |
| `current_line`, `current_frame` | The suspended location and frame being inspected. |
| `req_id_counter` | Per-session monotonic id for outbound requests. |

---

## Transport wiring (lazy-attach)

The runtime exposes a single atomic transport pointer:

```c
extern _Atomic(transport_t *) g_debug_attach_transport;
void debug_set_attach_transport(transport_t *t);
void debug_wait_lazy_threads_drained(void);
```

`transport_t` is `{ void *ctx; void (*write_line)(void *ctx, const char *json, size_t len); }` — three params, void return.

At startup, `debugger_tui_main`:

1. `headless_save_threadglobals()` to snapshot the parent's hglobals (the GIL holder's runtime context).
2. Allocates `tui_state_t` via `calloc`.
3. Builds the transport (`tui_write_line` as the callback, `ctx = state`).
4. `debug_set_attach_transport(&state->transport)` — runtime threads that hit breakpoints now route through us.
5. Enters the event loop.

Inbound (runtime -> TUI): when a runtime thread hits a breakpoint, watchpoint, or step boundary, `g_debug_attach_transport->write_line(ctx, json, len)` is called from that thread's stack. `tui_write_line` parses the JSON, dispatches based on the `method` or `id`, and updates `tui_state_t` fields. Rendering happens on the main thread (next tick), not synchronously inside `write_line`.

Outbound (TUI -> runtime): the TUI builds an NDJSON request line and dispatches it through the runtime's own debugger entry points (the same set `--protocol` mode uses). It does **not** write back through `g_debug_attach_transport` — that's the in-bound channel only.

---

## Event loop and GIL discipline

```
debugger_tui_main:
  hglobals_snapshot = save_threadglobals()
  state = calloc(...)
  register transport
  loop:
    headless_restore_threadglobals(NULL)         // drop GIL
    ev = boxen_poll_event(...)                   // blocks, no GIL needed
    headless_acquire_threadglobals(hglobals_snapshot)  // reacquire
    debugger_tui_run_one_tick(state, &ev)        // process under GIL
    if (state->should_quit) break
  drain + free + restore
```

This mirrors the GIL pattern from `protocol_handler.c` (PR #722). The TUI **must** release the GIL during `boxen_poll_event` — that call blocks waiting for keystrokes, and holding the GIL would freeze every other UserTalk thread (including the one whose breakpoint we're trying to debug).

The hglobals snapshot/restore pattern is the supported way to do this. Don't introduce alternative GIL-release patterns; reuse what's in place.

---

## Teardown — the drain-before-free contract

This is the canonical PR #722 contract and must be preserved exactly. From `debugger_tui_main`'s exit path:

```c
hglobals_snapshot = save_threadglobals();              // 1
debug_wait_lazy_threads_drained();                     // 2
headless_restore_threadglobals(hglobals_snapshot);     // 3
debug_set_attach_transport(NULL);                      // 4
debugger_tui_state_teardown(state);                    // 5 (frees transport)
free(state);                                           // 6
```

Step ordering matches `protocol_handler.c:412-424` step-for-step:

1. Snapshot hglobals (we'll restore right before freeing).
2. **Drain.** Wait for any runtime thread currently mid-callback through `g_debug_attach_transport` to return. Without this, freeing the transport while a callback is in flight is a use-after-free.
3. Restore hglobals so subsequent runtime calls have a valid context.
4. Atomically null the transport pointer. New threads that try to attach now find `NULL` and skip.
5. Free the transport struct (and the rest of state).
6. Free the state buffer itself.

Three reviewers (bar-raiser, security, concurrency) independently verified this at merge time. **Do not reorder these steps.** If you need to add cleanup work, slot it either between 1 and 2 (before drain — only safe if it doesn't touch the transport) or after 5 (post-free — only safe if it doesn't reference state).

The same drain contract applies to any future TUI exit path (Ctrl-C handler, signal, panic recovery). If you add one, it goes through the same six steps.

---

## Modal handling

Modals (condition breakpoint, watchpoint) are full boxen windows with `set_modal(true)`. Boxen routes all input to the modal until it closes. The TUI's outer `on_input` callback never sees keys while a modal is open.

Cursor positioning uses the A.7 API:
- `boxen_window_set_cursor(win, col, row)` to position.
- `boxen_window_set_cursor_visible(win, bool)` to show/hide.

The modal-gate auto-routing means cursor commands also route to the modal automatically; you don't have to track which window owns the cursor.

The eval pane (`:`) is **not** a modal — it's a state flag (`eval_pane_active`) plus a footer-window borrow-restore pattern (B.7). The footer window owns the cursor while active; on dismiss, the cursor is restored to invisible. This pattern transfers directly to the Phase C REPL pane.

---

## JSON discipline

All outbound JSON is built with `snprintf` into stack buffers (typically 512 bytes). User-supplied strings (condition expressions, watch paths, eval expressions) pass through `tui_json_escape` first, which handles `"`, `\\`, and control characters as `\u00XX` escapes.

Inbound JSON is parsed with `cJSON`. Response handlers verify the actual emit shape against `op_handler.c` — see `memory/feedback_wire_format_verification.md` for the discipline rule.

The two recurring wire-format bug shapes (caught during B.5 and B.7 reviews):

1. **Field name mismatch on request side** — e.g., dispatching `"path"` when the handler reads `"variable"`. Cure: quote the handler's parse line in the PR description.
2. **Response shape assumed instead of verified** — e.g., parsing `result` as a bare string when the handler emits `{"value": ..., "type": ...}`. Cure: quote the handler's emit line in the PR description.

Both classes are now caught by the boundary-crossing shell test (`tests/debugger_tui_test.sh`), but PR-author-side verification is still the front-line defense.

---

## Testing

| Layer | What it covers |
|-------|----------------|
| `tests/debugger_tui_tests.c` | Pure unit tests against `tui_state_t` and helpers. Compiled with `DEBUGGER_TUI_OMIT_MAIN` to exclude GIL/runtime symbols. Uses the `dispatch_capture_buf` seam to verify outbound JSON without a live runtime. |
| `tests/debugger_tui_test.sh` | Shell-level: launches `frontier-cli --debug-tui` (with no TTY — clean-exit paths only), verifies error messages, mutual exclusion, etc. |
| `tests/debugger_tui_pty_test.py` | PTY-driven lifecycle: uses `pty.fork()` to give the binary a real PTY so termbox2 can initialize. Verifies full launch -> keybind -> clean exit. |

The PTY tests use `pty.fork()` deliberately. `subprocess.Popen` with remapped fds does NOT work for the TUI because termbox2 opens `/dev/tty` directly, ignoring the remapped descriptors.

---

## Build flags

- `DEBUGGER_TUI_OMIT_MAIN` — defined in the unit test build. Excludes `debugger_tui_main` and any code that touches GIL globals or `headless_*` symbols. Lets the unit test binary link without pulling in the entire runtime.

There is no other build flag controlling the TUI. `--debug-tui` is always compiled in; it's just dormant unless the flag is passed.

---

## The default-REPL debug surface (unit 2.4)

The boxen REPL (`boxen_repl.c`, the default mode since C.6) carries a minimal debug surface built on the same two channels as the TUI, with a stricter threading split:

**Inbound (runtime -> REPL).** `boxen_repl_main` registers its heap-allocated launch transport as the lazy-attach transport with `boxen_repl_debug_write_line` as the callback and the REPL state as `ctx`. Unlike `tui_write_line`, this callback does **not** parse or update UI state on the caller's stack: it copies the raw NDJSON line into a fixed-size ring (`debug_queue` on `boxen_repl_state_t`) under `debug_queue_mutex` and returns. The main thread drains the queue each event-loop tick (`boxen_repl_drain_debug_notifications`), parses with cJSON, appends formatted `[debug]` lines to the scrollback, and maintains the suspended-thread set that drives the footer hint. Rationale: `boxen_repl_append_scrollback` frees/strdups ring entries with no synchronization and is main-thread-only, so the runtime-thread callback must never touch it. Queue overflow drops the newest line and the drain reports the drop count; a later `/threads` rebuilds the suspended set from the authoritative payload, so drift self-heals.

**Outbound (REPL -> runtime).** The `/bp`, `/continue`, `/step`, `/locals`, `/threads` slash commands (kernel intercepts in `submit_input`, same pattern as `/edit`) synthesize request JSON and call `op_dispatch` directly with a second transport (`debug_response_transport`). Op handlers respond synchronously on the main thread through it, so its `write_line` formats straight into the scrollback with no queue hop. Test builds capture the request JSON via `debug_dispatch_capture_buf` (the `dispatch_capture_buf` pattern from the TUI tests) instead of linking `op_dispatch`.

**Teardown.** The REPL follows the same drain-before-free contract as the TUI, plus one extra step: after transport-NULL + kill + join it calls `debug_wait_lazy_threads_drained()` a second time. A lazy thread that attached between the first drain and the transport-NULL is detached (never joined) and still holds the transport and `ctx` pointers; its final `write_line` (the `debug/completed` emit in `headless_spawn.c`) happens before it decrements the lazy count, so the second wait covers every thread that responds to the kill flag. This narrows the window rather than closing it: the drain has a bounded give-up path (~5s + 500ms grace, then log-and-proceed — the documented trade-off in `debug_handler.c`), so a thread wedged outside the interpreter callback loop can still outlive it on this process-exit path.

**Dialog modals.** `run_modal_loop` in `boxen_ui.c` drains the debug queue each iteration via the `drain_debug` slot on `boxen_ui_host_t` (wired to `boxen_repl_drain_debug_thunk`), so a breakpoint hit while a `dialog.*` modal is open renders behind the dialog instead of parking invisibly until the modal closes.

Coverage: `tests/boxen_repl_tests.c` (enqueue-only contract, drain rendering, command dispatch shapes, response rendering, footer hint) and `tools/tui-tests/tests/test_10_debug_surface.py` (real-binary `/bp` + `thread.callScript` + `/continue` round trip). User-facing command reference: [`CLI_USAGE_GUIDE.md`](CLI_USAGE_GUIDE.md).

---

## Where Phase C goes

Per `planning/phase_c/OVERVIEW.md`, Phase C is the boxen-native REPL. The B.7 eval pane is structurally the seed of the Phase C input line:

- The borrow-restore cursor pattern transfers directly.
- The eval history ring becomes the REPL output buffer.
- The `:` activation gate goes away (the REPL pane is always active).

When Phase C lands, the TUI debugger and the REPL will share window real-estate. The two-pane debugger layout from Phase B is expected to extend with a REPL pane below the footer; the exact layout is open per `planning/phase_c/OVERVIEW.md`.
