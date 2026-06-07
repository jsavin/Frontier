# boxen Phase B -- Debugger TUI Execution Plan

Detailed sequencing for the UserTalk debugger TUI. Consumes the boxen substrate
shipped in Phase A (PRs #724-#731) and the runtime debug contract from PR #722
(thread-debug-attach). Closes issue #691.

**Status**: Phase A complete (2026-06-06). Phase B not yet started.

---

## 0. Relationship to other docs

Read these first; this document takes them as given and provides the concrete
implementation steps:

- `planning/boxen/OVERVIEW.md` -- UX motivation, surface inventory, Phase B sketch
- `planning/boxen/EXECUTION_PLAN.md` -- boxen API surface (the substrate Phase B consumes)
- `planning/boxen/DEBUGGER_TUI_HANDOFF.md` -- runtime contract; GIL discipline; transport lifetime

This plan does NOT redesign boxen. If a boxen gap is found during Phase B, it is
flagged as "boxen-side follow-up" at the relevant milestone. The substrate is frozen.

---

## 1. Goal of Phase B

Phase B delivers a working UserTalk debugger TUI that closes issue #691. The user
enters TUI debug mode via a new `--debug-tui` CLI flag. The terminal switches to a
two-pane layout: the left pane shows the currently-suspended script source with a
highlighted current line and breakpoint markers in the gutter; the right pane shows
the call stack and frame-local variables. A pinned footer shows standard keybinds.
Standard debugger controls (F5 continue, F9 toggle breakpoint, F10 step-over, F11
step-into, Shift-F11 step-out) are wired to the existing `handle_debug_*` functions
in `debug_handler.c` via a direct in-process `transport_t`. The "killer feature" from
`OVERVIEW.md` -- cmd-double-click on a script-pane identifier to resolve it to a frame
address -- is implemented using `BOXEN_MOUSE_DOUBLE_CLICK` and `BOXEN_MOD_META`.

Headless behavioral coverage ships alongside the implementation: a mock-backend TUI
test suite in `tests/debugger_tui_tests.c` (C unit tests using `backend_mock.h`) plus
a shell integration test in `tests/debugger_tui_test.sh` that exercises the full TUI
entry/exit lifecycle with a real binary.

The existing `--protocol` debug path is unchanged. The TUI and the protocol path are
mutually exclusive at runtime; both call `debug_set_attach_transport` and the CLI
enforces that only one can be active at a time.

---

## 2. Non-Goals (explicitly deferred)

- **WS-attached TUI debugging.** Per `DEBUGGER_TUI_HANDOFF.md`: the per-message stack
  transport in `ws_server.c` does not satisfy the lifetime contract for lazy-attach.
  UAF risk. Deferred entirely. The TUI attaches only in-process.
- **Outline editor TUI.** Phase C is the second boxen consumer.
- **Boxen API churn.** Phase A is frozen. If a surface need reveals a substrate gap,
  the gap is filed as a boxen follow-up, not fixed inline in Phase B.
- **Step-into across `thread.callScript` boundaries.** Today's step-into stops at C
  verb boundaries. Cross-thread step-into is not supported and is deferred.
- **Watchpoint UX for table-typed variables.** Wire format truncates for non-scalar
  values. Scalar watchpoints are in scope; table-typed watchpoints are deferred.
- **Conditional-breakpoint timeout UI.** The condition evaluator can stall. A
  "condition timeout" warning in the TUI is v2 territory.
- **Standalone extraction.** Phase E. Phase B ships the TUI in-tree as a second
  consumer of boxen.

---

## 2.1 Code Verification: Handoff Doc vs Reality

The following claims in `DEBUGGER_TUI_HANDOFF.md` were verified against the actual
source before writing this plan. Divergences are flagged.

**Claim 1: `transport_t` carries only `(ctx, write_line)`.** Handoff doc says
`write_line` takes `(ctx, line)`. DIVERGENCE: actual signature at
`frontier-cli/op_handler.h:45` is:

```c
void (*write_line)(void *ctx, const char *json, size_t len);
```

Three parameters, not two. The TUI's `write_line` implementation MUST accept `len`.
The handoff doc is wrong on this point. Flag for correction.

**Claim 2: `handle_debug_*` functions take `transport_t *`.** CONFIRMED. All
handlers in `debug_handler.h:139-153` have the signature
`void handle_debug_*(int id, const char *json_line, transport_t *transport)`.
The TUI dispatches to them directly by constructing a JSON request string and
calling `op_dispatch` with its own transport, OR by calling the handlers directly
with a synthesized JSON params string. Both patterns work; `op_dispatch` is cleaner
(single dispatch point, handles the `id`/`op` routing).

**Claim 3: `debug/getSource` returns lines, not raw source text.** CONFIRMED and
clarified. `handle_debug_getsource` at `debug_handler.c:2082` returns a JSON array
of line objects: `{ "num": N, "text": "...", "breakpoint": true/false, "current": true/false }`.
The text per line is the outline-to-text expansion via `opgetlangtext`, using CR
as the line separator. Importantly, the text is the compiled script outline rendered
as text -- it is the same representation the UserTalk editor uses, suitable for
display. No gap here; `debug/getSource` is adequate for Phase B's script pane.

**Claim 4: hglobals snapshot/restore pattern is around `protocol_handler.c:395-420`.**
CONFIRMED. Lines 411-421 of `frontier-cli/protocol_handler.c` show the canonical
pattern (added in PR #722):

```c
{
    hdlthreadglobals main_hglobals = hthreadglobals;
    debug_wait_lazy_threads_drained();
    headless_restore_threadglobals(main_hglobals);
}
debug_set_attach_transport(NULL);
free(transport);
```

The TUI's teardown sequence must mirror this exactly. See milestone B.0.

**Claim 5: Call stack retrieval exists as `debug/getStack`.** CONFIRMED.
`handle_debug_getstack` at `debug_handler.c:2011` exists. It returns a `frames`
array from outermost caller (`script_stack[0]`) to innermost (`current_script`).
Each frame has `level`, `script`, and `line`. No gap.

**Additional: `debug/getLocals` exists.** CONFIRMED at `debug_handler.c:1900`.
Returns locals of a suspended thread. Required for the frame-locals display in
the right pane. The TUI will call this when a frame is selected.

---

## 3. Threat Model / Risk Register

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| Transport write_line signature mismatch (3 params vs 2) | Detected | High | Documented above in 2.1; all TUI write_line impls must accept len |
| GIL yield overwrites C globals (hglobals clobber) | Medium | High | Mirror protocol_handler.c:411-421 pattern exactly; covered in B.0 |
| Lazy-attach transport lifetime UAF | Medium | High | Heap-allocate transport; drain before free; sentinel in write_line |
| `debug_wait_lazy_threads_drained()` blocks indefinitely | Low | Medium | Has internal 5s+500ms timeout; TUI teardown inherits this; acceptable |
| boxen_run() used instead of poll+present | Medium | Medium | boxen_run() holds GIL permanently (per boxen.h:481-483); TUI MUST use poll+present loop with manual GIL release |
| `boxen_window_set_row_highlight` erases characters (A.5 known limitation) | High | Low | Documented in boxen.h:430-436; highlight row shows spaces with reverse-video attr; acceptable for current-line indicator |
| getSource CR line separator not matching display newlines | Low | Low | opgetlangtext uses CR; line-splitting logic in TUI must split on CR |
| F-key delivery on various terminals | Medium | Medium | Use boxen_key_t portables (BOXEN_KEY_F5 etc.); backend_tb2 handles translation |
| Double-click timing on slow terminals | Low | Low | boxen's 500ms DOUBLE_CLICK_MS window covers most cases; boxen_mock_push_double_click for tests |
| `--debug-tui` flag conflicts with `--protocol` | Medium | High | CLI enforces mutual exclusion in B.0; both call debug_set_attach_transport |
| Stale ctx access after TUI session frees state | Low | High | Zero ctx before free, or sentinel check in write_line; covered in B.0 |
| Script pane line-number off-by-one (1-based ODB vs 0-based boxen content rows) | Medium | Low | Translation at boundary: line N -> content row (N-1); covered in B.1 |
| Step direction enum mapping | Low | Low | handle_debug_step reads "direction" param string: "into", "out", "over"; TUI must construct correct JSON |

---

## 4. Test Infrastructure

### 4.1 C unit tests (mock backend, no real terminal)

File: `tests/debugger_tui_tests.c`

Links against `boxen.c`, `backend_mock.c`, and `debugger_tui.c` (with the debug
handlers replaced by a mock debug backend). Uses `backend_mock.h` for cell inspection
and event injection. Uses `test_report.h` TR_RUN/TR_SUMMARY/TR_EXIT_CODE harness
(same as all other Frontier unit tests).

Mock debug backend pattern: define a `mock_debug_transport_t` that records calls to
`handle_debug_*` and queues synthetic `debug/suspended` notifications. The TUI's
`write_line` target writes the NDJSON string to a ring buffer; test code inspects
the ring buffer to verify what the TUI would render.

This is the correct test shape for behavioral coverage of:
- Correct pane layout (verify cell content via `boxen_mock_cell_at`)
- Correct keybind routing (inject BOXEN_KEY_F5, verify continue was called)
- Correct scroll/highlight behavior on `debug/suspended`
- Modal overlays appear and dismiss correctly

### 4.2 Shell integration tests (real binary, no real terminal)

File: `tests/debugger_tui_test.sh`

Uses the `--debug-tui` flag with `--skip-startup` to enter TUI mode with a staged
database, then immediately sends a quit signal or uses a sentinel script that exits
cleanly. Verifies:
- The binary starts and exits with status 0
- No crash or leaked file descriptors
- The lazy-attach transport drain sequence completes

This is a shallow sanity test -- the deep behavioral coverage lives in the C unit
tests. The shell test exists to catch integration failures (wrong mode dispatch,
missing init/teardown, signal handling) that unit tests cannot cover.

### 4.3 When to use which

Use C unit tests for: all behavioral assertions about rendering, keybinds, modal
overlays, scroll, highlight, cmd-click resolution. These run headless without a
terminal via the mock backend.

Use shell tests for: lifecycle (entry, exit, crash-on-startup, signal delivery).

Do NOT use the `debug_protocol_test.sh` tests for Phase B. Those are the executable
spec for the runtime debug surface (PR #722); they remain unchanged. Phase B adds
a new test file at the TUI layer, not at the protocol layer.

---

## 5. Milestones

---

### Milestone B.0 -- Tracer-bullet skeleton

**Goal**: Prove that boxen can be initialized inside the Frontier runtime context with
correct GIL discipline, entry/exit, and the lazy-attach transport lifecycle.

#### Files to create

`frontier-cli/debugger_tui.h` (new):
- Single public function: `int debugger_tui_main(const cli_options_t *opts)`
- Matches the shape of `protocol_main` in `protocol_handler.h`

`frontier-cli/debugger_tui.c` (new):
- Skeleton: `boxen_init`, layout creation (empty windows with titles), event loop
  with GIL release/reacquire, `boxen_shutdown`, transport allocation/drain/free
- Follow `protocol_handler.c` for the GIL yield pattern (lines 349-354)
- Follow `protocol_handler.c:411-421` for the hglobals snapshot/restore on teardown
- Transport `write_line` must accept `(ctx, json, len)` -- 3 parameters, per
  `op_handler.h:45`. This is the ACTUAL signature; the handoff doc is wrong on this
- Heap-allocate `transport_t` (same reason as `protocol_main`: lazy-attach threads
  may call `write_line` after the function that created the transport returns)

#### Files to modify

`frontier-cli/cli_parser.h`:
- Add `boolean tui_mode;` field to `cli_options_t`

`frontier-cli/cli_parser.c` (or wherever getopt is processed):
- Add `--debug-tui` to the long options table
- Set `opts->tui_mode = true` when matched
- Verify mutual exclusion: if both `--protocol` and `--debug-tui` are set, error out
  with a clear message ("--debug-tui and --protocol are mutually exclusive")

`frontier-cli/main.c`:
- Add `else if (g_cli_options.tui_mode)` branch in the dispatch block (lines 850-860)
  before the REPL fallthrough
- `exit_code = debugger_tui_main(&g_cli_options)`

`frontier-cli/Makefile`:
- Add `debugger_tui.c` to `SRCS`

#### Tests

**C unit (RED-then-GREEN)**:
`tests/debugger_tui_tests.c` -- first test: `test_tui_init_and_quit`.
- RED: file does not exist yet; test suite won't compile
- GREEN after implementation:
  ```
  boxen_mock_reset(80, 24);
  boxen_init(boxen_mock_backend(), NULL, NULL);
  // inject a 'q' keypress so the event loop exits
  boxen_mock_push_key(BOXEN_KEY_NONE, 'q', BOXEN_MOD_NONE);
  int rc = debugger_tui_run_one_tick(&state); // (see note)
  assert(rc == TUI_QUIT);
  boxen_shutdown();
  ```
  Note: `debugger_tui_main` is a blocking loop unsuitable for direct unit testing.
  Export a `debugger_tui_run_one_tick(tui_state_t *s)` internal function
  (visible via `debugger_tui_internal.h` test-only header) that processes one
  event and returns. Tests drive the TUI by calling tick repeatedly.

**Shell integration**:
`tests/debugger_tui_test.sh` -- first test: `test_tui_startup_and_exit`.
- Starts `frontier-cli --debug-tui --skip-startup --system-root $DB`
- Sends SIGTERM after 200ms
- Verifies exit status is 0 (no crash)
- Verifies no coredump in `tests/tmp/`

#### Verification gate

Unit tests: `./tools/run_headless_tests.sh` passes.
Integration: `cd tests && make test-integration` passes (the shell test must be added
to the integration suite via the YAML test runner, marked `sequential: true`).
Main-session integration verification: YES -- this milestone touches GIL paths.
`--protocol` and `--debug-tui` mutual exclusion must be verified by hand once.

#### Dependencies

Phase A complete (boxen substrate available). PR #722 merged (transport lifecycle
APIs available: `debug_set_attach_transport`, `debug_wait_lazy_threads_drained`).

#### Out of scope

Any actual debug functionality. No source loading, no debug ops called. The TUI opens
three empty windows (script, stack, footer) and immediately accepts `q` to quit.

#### Sentinels / fragile assumptions

SENTINEL: `debugger_tui_main` must call `debug_set_attach_transport(my_transport)`
on entry and follow the drain-then-clear-then-free pattern on exit. Forgetting the
drain allows lazy threads to call `write_line` on a freed transport. See
`protocol_handler.c:379-424` for the exact sequence and the comment block explaining
why the order matters.

SENTINEL: `boxen_run()` must NOT be called from `debugger_tui_main`. See
`boxen.h:474-482`: `boxen_run()` holds the GIL for its entire duration. The TUI
must use `boxen_poll_event` + `boxen_dispatch_event` + `boxen_present` directly,
with GIL release/reacquire around each `boxen_poll_event` call.

SENTINEL: `write_line` signature is `(ctx, json, len)` -- three params. The handoff
doc says two. The actual header `op_handler.h:45` is authoritative.

---

### Milestone B.1 -- Script pane and source loading

**Goal**: Display script source in the left pane with a current-line highlight when a
thread is suspended.

#### What `debug/getSource` returns (code-verified)

`handle_debug_getsource` (`debug_handler.c:2082`) returns:

```json
{
  "result": {
    "script": "path.to.script",
    "currentLine": 5,
    "lines": [
      { "num": 1, "text": "local x", "breakpoint": false },
      { "num": 5, "text": "  msg(x)", "breakpoint": false, "current": true }
    ]
  }
}
```

Line numbers are 1-based. `currentLine` is only present if a `threadId` was passed
and that thread is currently suspended in this script. The text per line is the
outline-to-text expansion of the compiled script using CR as line separator.

#### Layout

The left pane occupies approximately 60% of terminal width. It has a border and a
title showing the script path. The gutter (column 0 of content) shows:
- `>` for the current execution line
- `*` for a line with a breakpoint
- ` ` (space) otherwise

Content columns 1..N show the source text, truncated to fit.

#### Data model in TUI state

Add to `tui_state_t` (internal struct in `debugger_tui.c`):
- `char script_path[256]` -- currently-loaded script dotted path
- `char **script_lines` -- heap array of line text strings
- `int script_line_count`
- `long current_line` -- 1-based; -1 if not suspended or not in this script
- `unsigned long bp_lines[256]` -- 1-based line numbers with breakpoints; count field

Source is loaded by calling `op_dispatch` with a synthesized `debug/getSource` JSON
request through the TUI's own transport. The transport's `write_line` then receives
the response and updates `tui_state_t` synchronously (the TUI holds the GIL during
this call).

#### Current-line highlight

Call `boxen_window_set_row_highlight(script_win, current_line - 1, BOXEN_ATTR_REVERSE, fg, bg)`
after drawing. Note the off-by-one: ODB line numbers are 1-based; boxen content rows
are 0-based. `current_line - 1` is the content row.

Known limitation (from `boxen.h:430-436`): the highlight writes spaces over the cell
characters. The gutter and source text will appear as a reversed-video empty row. The
character content (gutter marker, source text) is not preserved by the highlight. Two
options:
1. Draw the highlight row manually in the draw callback using `BOXEN_ATTR_REVERSE` on
   every cell instead of calling `boxen_window_set_row_highlight`. This preserves
   characters.
2. Use `boxen_window_set_row_highlight` and accept that the current line shows
   reversed-video blank space (visible but loses text).

Recommended: option 1 (draw highlighted row manually). It is more code but gives the
expected UX (highlighted row shows the actual source text). Flag `boxen_window_set_row_highlight`
as "useful for future surfaces that don't need character preservation; for debugger,
draw manually." This is not a boxen API gap -- the limitation is documented. Log it
as a follow-up comment in the draw callback code.

#### Auto-scroll to current line

After updating `current_line`, call `boxen_window_ensure_visible(script_win, 0, current_line - 1)`
to scroll the window so the current line is visible.

#### Files to modify

`frontier-cli/debugger_tui.c`:
- Add `tui_state_t` fields listed above
- Add `load_source(tui_state_t *s, const char *script_path, long thread_id)` --
  constructs the `debug/getSource` JSON request, calls `op_dispatch`, parses the
  response into `s->script_lines`
- Add `draw_script_pane(boxen_window_t *win, void *ud)` -- the draw callback for
  the left window
- Wire `debug/suspended` notification handling: when `write_line` receives a
  `debug/suspended` notification, extract `script` and `line`, call `load_source`,
  call `boxen_window_ensure_visible`, `boxen_window_invalidate`

No new files. All changes to `debugger_tui.c`.

#### Tests

**C unit (RED-then-GREEN)**:
`test_script_pane_draws_source`:
- RED: `draw_script_pane` does not exist yet
- Setup: `boxen_mock_reset(80, 24)`; open script window at `{0, 0, 48, 22}`;
  populate `state.script_lines` with 5 synthetic lines; set `current_line = 3`
- GREEN assertion: `boxen_mock_has_text("line 1")` and `boxen_mock_has_text("line 3")`;
  verify row 2 (content row for line 3) has REVERSE attribute on at least one cell

`test_script_pane_autoscrolls_to_current_line`:
- Populate 40 lines; set `current_line = 35`; trigger draw
- After draw: verify `boxen_window_get_scroll` returns scroll_y >= 20 (line 35 out
  of 40 should scroll to be visible)

`test_load_source_parses_response`:
- Feed a synthetic `debug/getSource` response JSON through the TUI's `write_line`
- Verify `state.script_line_count == 5` and `state.script_lines[0]` matches line 1

#### Verification gate

Unit: `./tools/run_headless_tests.sh` passes.
Integration: `cd tests && make test-integration` passes.
Main-session: YES -- source loading calls ODB path resolution under the GIL.

#### Dependencies

B.0 complete. `handle_debug_getsource` exists and is verified (`debug_handler.c:2082`).

#### Out of scope

Stack/locals pane. Breakpoint toggles. Step commands.

#### Sentinels / fragile assumptions

SENTINEL: Line numbers from `debug/getSource` are 1-based. Boxen content rows are
0-based. Translation: content_row = line_num - 1. Off-by-one here will put the
highlight on the wrong line. Test B.1's `test_script_pane_draws_source` must
explicitly check the cell at content row (current_line - 1), not (current_line).

SENTINEL: `opgetlangtext` uses CR (`\r`) as line separator, NOT `\n`. The JSON
serialization in `handle_debug_getsource` splits on `\r` or `\n` (code at
`debug_handler.c:2263`). The JSON `text` fields will NOT contain embedded newlines.
No action needed -- just confirming the line text is clean.

---

### Milestone B.2 -- Stack/locals pane

**Goal**: Display call stack in the right pane; show selected frame's locals below
the stack.

#### What `debug/getStack` returns (code-verified)

`handle_debug_getstack` (`debug_handler.c:2011`) returns:

```json
{
  "result": {
    "frames": [
      { "level": 1, "script": "outer.script", "line": 10 },
      { "level": 2, "script": "inner.script", "line": 3 }
    ]
  }
}
```

Frames are ordered outermost (level 1) to innermost (highest level). `current_script`
is the innermost and appears last. Note: `line` may be absent if the frame's line
number is 0 (e.g., for the outermost frame before any statement fires).

#### What `debug/getLocals` returns

`handle_debug_getlocals` (`debug_handler.c:1900`) returns the local variables of the
suspended thread as a flat JSON object. The right pane renders this below the stack
list when a frame is selected. Note: `debug/getLocals` returns locals of the currently
suspended frame, not a frame-by-frame query. The TUI shows locals of the innermost
(active) frame; frame selection in the stack list updates the script pane's highlight
position but does NOT change which locals are shown (the runtime has no
frame-by-frame locals API; this is a known limitation per the handoff doc).

#### Right pane layout

Split vertically at approximately 50% of the right half's height:
- Top section: call stack list (each frame on one line: `  L2  inner.script:3`)
- Separator line: `--- Locals ---` (drawn as a horizontal rule)
- Bottom section: flat list of `varname = value` for the suspended thread's locals

Selecting a frame in the stack list (Up/Down arrows when the right pane is focused):
- Highlights the selected frame row
- Updates the SCRIPT PANE to load that frame's script and scroll to that frame's line
  (requires a cross-pane action: B.2 must invoke B.1's `load_source` with the selected
  frame's script and line)

#### Data model

Add to `tui_state_t`:
- `int frame_count`
- `char frame_scripts[32][256]`
- `long frame_lines[32]`
- `int selected_frame` -- 0-based index into frames array
- `char **local_names` and `char **local_values` -- heap arrays for locals
- `int local_count`

#### Files to modify

`frontier-cli/debugger_tui.c`:
- Add `load_stack(tui_state_t *s, long thread_id)` -- dispatches `debug/getStack`
- Add `load_locals(tui_state_t *s, long thread_id)` -- dispatches `debug/getLocals`
- Add `draw_stack_pane(boxen_window_t *win, void *ud)` -- draw callback for right window
- Extend `debug/suspended` handler to call `load_stack` and `load_locals` after
  `load_source`
- Add frame-selection input handling to the right pane's input callback

#### Tests

`test_stack_pane_draws_frames`:
- Populate `state.frame_scripts` with 3 synthetic frames; verify three rows render
  with correct script names

`test_frame_selection_updates_script_pane`:
- Start with frame 0 selected (innermost); `current_line` = frame 0's line
- Inject DOWN arrow on the right pane
- Verify `state.selected_frame == 1` (but note: since the stack is outermost-first
  and the convention is to start at the innermost, test must verify the UX direction)
- Verify script pane shows the selected frame's script path

`test_locals_render`:
- Populate `state.local_names = ["x", "y"]`, `state.local_values = ["42", "true"]`
- Verify right pane content contains "x = 42" and "y = true"

#### Verification gate

Unit + integration. Main-session: YES -- `debug/getLocals` and `debug/getStack`
call into the ODB under GIL.

#### Dependencies

B.1 complete.

#### Out of scope

Keybind step commands. Breakpoint/watchpoint modals.

#### Sentinels / fragile assumptions

SENTINEL: `debug/getLocals` returns locals of the suspended frame at the moment of
the call, not a snapshot. If the implementer tries to add a "per-frame locals" feature
by calling `debug/getLocals` on frame selection, the runtime has no such API. The
planned behavior (locals always show the innermost suspended frame) is the correct
scope for Phase B.

SENTINEL: Frame selection in the stack pane drives the SCRIPT PANE scroll/highlight
(a cross-pane side effect). This is the expected UX. Implement it by having the stack
pane's input callback call the same `load_source` function that B.1 introduced with
the selected frame's script and line. Do not add a new dispatch path.

---

### Milestone B.3 -- Keybinds and step/continue

**Goal**: Wire F5/F10/F11/Shift-F11 to runtime debug operations; update both panes
on `debug/suspended` after each step.

#### Keybind table

| Key | boxen check | Debug op | JSON params |
|-----|-------------|----------|-------------|
| F5 | `ev.key.key == BOXEN_KEY_F5` | `debug/continue` | `{"op":"debug/continue","id":N,"params":{"threadId":TID}}` |
| F10 | `ev.key.key == BOXEN_KEY_F10` | `debug/step` | `{"op":"debug/step","id":N,"params":{"threadId":TID,"direction":"over"}}` |
| F11 (no shift) | `ev.key.key == BOXEN_KEY_F11 && !(mod & BOXEN_MOD_SHIFT)` | `debug/step` | `..., "direction":"into"` |
| Shift-F11 | `ev.key.key == BOXEN_KEY_F11 && (mod & BOXEN_MOD_SHIFT)` | `debug/step` | `..., "direction":"out"` |
| F9 | `ev.key.key == BOXEN_KEY_F9` | toggle breakpoint (B.4) | -- |

All keybinds check `ev.type == BOXEN_EV_KEY` first.

`handle_debug_step` reads the `"direction"` param as a string: `"into"`, `"out"`,
or `"over"` (verified `debug_handler.c:1449-1457`). Construct the JSON string
accordingly.

#### Dispatch pattern

The TUI dispatches debug ops by calling `op_dispatch` with a synthesized JSON string
and the TUI's own transport. This is simpler and more consistent than calling
`handle_debug_continue` directly: `op_dispatch` routes by `"op"` field, handles
`"id"` tracking, and calls the right `handle_debug_*` function. The TUI never needs
to know which C function to call for which op -- it just constructs JSON.

```c
/* Example: F5 continue */
static void tui_do_continue(tui_state_t *s) {
    char req[256];
    snprintf(req, sizeof(req),
        "{\"op\":\"debug/continue\",\"id\":1,\"params\":{\"threadId\":%ld}}",
        s->current_thread_id);
    op_dispatch(req, strlen(req), s->transport);
    /* Response arrives asynchronously via write_line when thread suspends again */
}
```

After a step/continue, the panes do NOT update immediately -- the runtime is now
running. They update when the next `debug/suspended` notification arrives via
`write_line`. The footer should reflect "running..." state until the next suspension.

#### Footer update

The pinned footer (already created in B.0) needs to reflect current TUI state:
- When a thread is suspended: `F5:continue  F9:bp  F10:step-over  F11:step-in  S-F11:step-out  q:quit`
- When running (after continue/step, before next suspension): `[running...]  q:quit`
- When no debug thread is active: `q:quit`

Add `tui_debug_state_t` enum to `tui_state_t`:
```c
typedef enum { TUI_STATE_IDLE, TUI_STATE_SUSPENDED, TUI_STATE_RUNNING } tui_debug_state_t;
```

#### Files to modify

`frontier-cli/debugger_tui.c`:
- Add keybind dispatch in the shared input callback (or script pane's input callback;
  either works since modal management is not complex at this point)
- Add `tui_do_continue`, `tui_do_step_over`, `tui_do_step_into`, `tui_do_step_out`
- Update `draw_footer` to use `state->debug_state`
- Add `tui_debug_state_t` to `tui_state_t`

#### Tests

`test_f5_dispatches_continue`:
- Set up suspended state; inject F5
- Verify `op_dispatch` was called with JSON containing `"op":"debug/continue"`
  (intercept via a mock write_line that records received JSON, or mock `op_dispatch`)

`test_shift_f11_dispatches_step_out`:
- Inject F11 with BOXEN_MOD_SHIFT
- Verify dispatched JSON has `"direction":"out"`

`test_f10_dispatches_step_over`:
- Inject F10; verify `"direction":"over"`

`test_footer_shows_running_after_continue`:
- Set `state.debug_state = TUI_STATE_RUNNING`; trigger draw
- Verify footer content contains "running"

`test_panes_refresh_on_suspended`:
- Inject a synthetic `debug/suspended` notification via `write_line`
- Verify the script pane's `script_path` and `current_line` are updated

#### Verification gate

Unit + integration. Main-session: YES -- `op_dispatch` calls into the runtime.

#### Dependencies

B.2 complete.

#### Out of scope

Breakpoint gutter rendering (B.4). Modal overlays (B.4, B.5).

#### Sentinels / fragile assumptions

SENTINEL: After F10/F11/Shift-F11, the thread resumes running. The TUI must NOT
attempt to call `debug/getStack` or `debug/getSource` while the thread is running
(the thread holds the GIL and the TUI cannot do ODB operations concurrently). Only
call these on `debug/suspended` notification. The `tui_debug_state_t` flag guards
this: check `state->debug_state == TUI_STATE_SUSPENDED` before dispatching any
read-only debug ops.

SENTINEL: `handle_debug_step` writes a `debug/suspended` notification to the
transport when the step completes (the thread hits the next statement and suspends).
This arrives via `write_line` in the background. The TUI's write_line function
runs under the GIL (because all boxen ops are under GIL). The suspended notification
triggers a redraw. This is the same path as F5 + breakpoint hit -- no special case.

---

### Milestone B.4 -- Breakpoints UI

**Goal**: F9 toggles a breakpoint on the current line; gutter renders breakpoint
markers; modal overlay for conditional breakpoints.

#### Breakpoint toggle logic

When F9 is pressed:
1. Get `current_line` and `script_path` from `tui_state_t`.
2. Check if a breakpoint exists at `(script_path, current_line)` in the local
   `bp_lines` cache (populated from `debug/getSource` response).
3. If no breakpoint: dispatch `debug/setBreakpoint` with script + line.
4. If breakpoint exists: dispatch `debug/clearBreakpoints` filtered to that line,
   OR use a convention (see note below).
5. On response, re-call `debug/listBreakpoints` to refresh the local cache, then
   invalidate the script pane.

Note on clear: `handle_debug_clearbreakpoints` (`debug_handler.h:146`) clears ALL
breakpoints. There is no "clear one breakpoint" op. The toggle-off path must:
- Collect all current breakpoints from the local cache
- Call `debug/clearBreakpoints`
- Re-add all breakpoints EXCEPT the toggled one

This is an N-1 round trip. For a small breakpoint set (practical limit is a few
dozen), it is acceptable. Flag as a known UX concern in a code comment: a future
`debug/clearBreakpoint` (singular) op would be cleaner.

Alternatively: check whether `debug/setBreakpoint` with an already-set line is a
toggle (it isn't -- it silently overwrites). The toggle-off path using
clear-and-readd is the correct approach for the current runtime.

#### Gutter rendering

The draw_script_pane callback (from B.1) already has a gutter column. Update it:
- Check `state->bp_lines` for the current line number
- Render `*` for breakpoint, `>` for current-line (or `B` + `>` if both)

#### Conditional breakpoint modal

When the user presses Shift-F9 (or a dedicated key -- propose `c` when focused on
a breakpoint line), open a modal overlay using `boxen_window_set_modal`.

The modal is a small centered window (approximately 60x8) with:
- Title: "Set Condition"
- A single-line text input area showing the current condition (if any)
- `Enter` to confirm, `Escape` to cancel

Text input in boxen: boxen does not provide a text widget (per `OVERVIEW.md` section
3.3). The modal implements its own single-line input: maintain a `char condition_buf[256]`
in the modal state; handle printable key events (`ev.key.ch != 0`) by appending to
the buffer; handle BACKSPACE by trimming; render the buffer with a visible cursor via
`boxen_window_set_cursor_visible(true)` and `boxen_set_cursor` (check boxen API -- if
no `boxen_set_cursor` on a window, use `backend->set_cursor` directly or implement
cursor via a highlighted cell -- VERIFY boxen.h has a cursor API for this).

CHECK: `boxen.h` exposes `set_cursor_visible` via the backend vtable but NOT a
public `boxen_window_set_cursor` function. The correct approach is to draw a
highlighted cell at the cursor position using `BOXEN_ATTR_REVERSE` on the character
cell. This gives a "block cursor" visual without needing a dedicated cursor API.
Flag as boxen follow-up: a `boxen_window_set_cursor(win, cx, cy)` API would simplify
inline text editing for all future surfaces.

#### Files to modify

`frontier-cli/debugger_tui.c`:
- Add `tui_toggle_breakpoint(tui_state_t *s, long line)` -- handles the clear-and-readd
  sequence
- Add `tui_open_condition_modal(tui_state_t *s)` -- opens a modal window
- Add `draw_condition_modal(boxen_window_t *win, void *ud)` and `input_condition_modal`
- Extend F9 handler in keybind dispatch
- Add `bp_condition_buf[256]` to modal state (stack-allocated per modal invocation)

#### Tests

`test_f9_toggles_breakpoint_on`:
- State: no breakpoints; `current_line = 3`; inject F9
- Verify dispatched JSON contains `"op":"debug/setBreakpoint"` with `"line":3`

`test_f9_toggles_breakpoint_off`:
- State: breakpoint at line 3; `current_line = 3`; inject F9
- Verify dispatched sequence: `debug/clearBreakpoints` then `debug/setBreakpoint`
  for every other breakpoint

`test_gutter_renders_breakpoint_marker`:
- Set `state.bp_lines[0] = 5`; set `state.script_line_count = 10`; draw
- Verify cell at content (0, 4) has ch == '*'

`test_condition_modal_appears_on_shift_f9`:
- Inject Shift-F9 with `current_line = 5`
- Verify a modal window is now the topmost window (`boxen_window_at` returns
  the modal for its center cell)

`test_condition_modal_enter_confirms`:
- Open modal; push characters 'x', '>', '0'; push Enter
- Verify the modal is closed
- Verify dispatched `debug/setBreakpoint` JSON contains `"condition":"x>0"`

#### Verification gate

Unit + integration. Main-session: YES -- `debug/setBreakpoint` modifies runtime state.

#### Dependencies

B.3 complete.

#### Out of scope

Watchpoints (B.5). Cmd-click identifier resolution (B.5).

#### Sentinels / fragile assumptions

SENTINEL: The clear-and-readd pattern for breakpoint toggle-off issues multiple
`op_dispatch` calls in sequence. All of these happen under the GIL with the thread
suspended. The responses arrive synchronously via `write_line` (because the handlers
run under the GIL on the calling thread). There is no async complication here --
clarify with a code comment.

SENTINEL: `boxen.h` does not expose `boxen_set_cursor` on a window. The "block
cursor" approach (reverse-video cell at cursor position) is the correct workaround.
DO NOT reach into the termbox2 backend directly to call `tb_set_cursor`. That
violates the backend abstraction discipline that Phase A established.

---

### Milestone B.5 -- Cmd-double-click identifier resolution and watchpoints

**Goal**: Cmd-double-click on a script-pane identifier resolves it to a frame address
and jumps to that value in the right pane; watchpoint modal wires to `debug/setWatchpoint`.

#### Cmd-double-click identifier resolution

This is the "killer feature" from `OVERVIEW.md`. The gesture is:
Meta (Cmd on macOS) + double-click on a word in the script pane.

Detection in the script pane's input callback:
```c
if (ev->type == BOXEN_EV_MOUSE
    && ev->mouse.button == 1 /* left */
    && (ev->mouse.mod & BOXEN_MOD_META)
    && (ev->mouse.flags & BOXEN_MOUSE_DOUBLE_CLICK)
    && !ev->mouse.pressed == false /* it IS pressed */) {
    // resolve identifier
}
```

Wait -- the check must be on `ev->mouse.pressed == true`. Clarify: the double-click
event arrives as a second press (`pressed == true`) with `BOXEN_MOUSE_DOUBLE_CLICK`
set in flags (per `boxen.h:133` and `EXECUTION_PLAN.md` section 12). The input
callback checks `ev->mouse.pressed` is true before acting.

Identifier extraction: use `boxen_window_at(sx, sy, &cx, &cy)` to get the content
coordinate. `cx` is the column within the content area; `cy` is the row (0-based).
Row `cy` corresponds to source line `cy + scroll_y + 1` (1-based). Extract the
identifier at column `cx` in that line by scanning the line text for word boundaries.

Address resolution: construct a UserTalk expression
`string(@path.to.identifier)` and dispatch via `script/eval`, OR use
`debug/getLocals` and search by name. The simpler path for v1 is: look up the
extracted identifier in `state->local_names` (already loaded from `debug/getLocals`).
If found, display the value in a status bar or a small popup. If not found, dispatch
`script/eval` with the identifier as the expression against the TUI's transport.

For Phase B, the minimum viable version is: look up in locals, show result in a
one-line popup at the bottom of the script pane. The full ODB address resolution
(jumping to the value in a table editor) is Phase C territory.

#### Watchpoint modal

Triggered by a dedicated key (propose `w` when the right pane is focused with the
cursor on a local variable). The watchpoint modal is structurally identical to the
condition breakpoint modal from B.4: a centered window, single-line text input for
the variable path, Enter confirms, Escape cancels.

On confirm: dispatch `debug/setWatchpoint`:
```json
{"op":"debug/setWatchpoint","id":N,"params":{"path":"varname","threadId":TID}}
```

`handle_debug_setwatchpoint` (`debug_handler.h:151`) implements this. The
notification when the watchpoint fires is `debug/suspended` with
`"reason":"watchpoint"` -- same notification path as breakpoints; no special handling
in the TUI required.

#### Files to modify

`frontier-cli/debugger_tui.c`:
- Add identifier extraction helper: `extract_identifier_at(const char *line, int col, char *out, int out_max)`
- Add `tui_resolve_identifier(tui_state_t *s, int row, int col)` -- looks up in locals, shows popup
- Add watchpoint modal (shares most code with condition modal from B.4; refactor
  into a generic single-line input modal helper)
- Add `w` keybind handler in right pane input callback

#### Tests

`test_cmd_double_click_extracts_identifier`:
- Script line "  local myVar = 5" at content row 2; set scroll to 0
- Inject a Meta + double-click mouse event at content coordinates (8, 2)
  (column 8 is inside "myVar")
- Verify `extract_identifier_at` returns "myVar"

`test_cmd_double_click_shows_local_value`:
- Populate `state.local_names = ["myVar"]`, `state.local_values = ["5"]`
- Simulate the identifier resolution with "myVar"
- Verify the right pane (or popup) shows "myVar = 5"

`test_watchpoint_modal_dispatches_setwatchpoint`:
- Open watchpoint modal with suggested path "myVar"
- Confirm; verify dispatched JSON contains `"op":"debug/setWatchpoint"` and
  `"path":"myVar"`

`test_boxen_mock_double_click_carries_meta`:
- Use `boxen_mock_push_double_click(10, 5, BOXEN_MOD_META)` to inject the event
- Verify the delivered event has `flags & BOXEN_MOUSE_DOUBLE_CLICK` and
  `mod & BOXEN_MOD_META`

#### Verification gate

Unit + integration. Main-session: YES -- identifier resolution and watchpoint
dispatch call into the runtime.

#### Dependencies

B.4 complete.

#### Out of scope

Full ODB address navigation (table editor). Watchpoints on table-typed variables
(deferred per handoff doc section "What's already known to be missing or rough").

#### Sentinels / fragile assumptions

SENTINEL: `boxen_window_at` returns `BOXEN_HIT_CHROME` (value: INT_MIN) for `cx`/`cy`
when the click lands on the window border. Check for `BOXEN_HIT_CHROME` before
treating the coordinates as content positions. The constant is defined in `boxen.h:406`.

SENTINEL: `BOXEN_MOUSE_DOUBLE_CLICK` is set on the SECOND press event. The first
press event does not carry the flag. The input callback MUST check `pressed == true`
AND `flags & BOXEN_MOUSE_DOUBLE_CLICK` to fire the gesture. A check on `pressed == false`
(release) will never match.

SENTINEL: The `extract_identifier_at` helper should handle the edge cases: click at
column 0 (gutter), click on whitespace (no identifier), click beyond line length.
Return empty string for all non-identifier positions.

---

### Milestone B.6 -- CLI dispatch polish, lazy-attach wiring, integration test pass

**Goal**: Close all remaining integration gaps; ensure the TUI enters and exits cleanly
under all conditions; pass the full test suite including the shell integration test.

#### Items in scope

**Lazy-attach setter call sequence (per handoff doc):**

On TUI debug mode entry (in `debugger_tui_main`, after `boxen_init`):
```c
transport_t *transport = calloc(1, sizeof(transport_t));
transport->ctx = s;
transport->write_line = tui_write_line;
debug_set_attach_transport(transport);
```

On TUI debug mode exit (before `boxen_shutdown`):
```c
{
    hdlthreadglobals main_hglobals = hthreadglobals;
    debug_wait_lazy_threads_drained();
    headless_restore_threadglobals(main_hglobals);
}
debug_set_attach_transport(NULL);
free(transport);
transport = NULL;
```

This is the exact sequence from `protocol_handler.c:412-424`. Mirror it verbatim.

**Signal handling:**

The TUI must handle SIGTERM and SIGINT cleanly. Termbox2 installs its own signal
handlers that restore the terminal on exit. Frontier's existing cleanup path
(in `cleanup_frontier_runtime`) handles the runtime side. Verify that SIGTERM from
a shell integration test leaves no orphaned terminal state. The shell test
`tests/debugger_tui_test.sh` tests this.

**`q` / Escape / Ctrl-C to quit:**

The TUI event loop exits on `q`, `BOXEN_KEY_ESCAPE`, or `BOXEN_KEY_CTRL_C`. This
was stubbed in B.0; verify it works after the full TUI is assembled.

**UX pass:**

- Verify the two-pane layout ratio (60/40 left/right) looks right at 80x24 and 220x50
- Verify the footer is readable at minimum terminal width (40 columns)
- Verify no rendering artifacts at terminal edges

**Final integration test:**

Add a YAML integration test in `tests/integration/`:
- `debugger_tui_lifecycle_test.yaml` (or `.sh` -- use shell since this is lifecycle,
  not a UserTalk eval test)
- Uses `--debug-tui --skip-startup`; installs a breakpoint; triggers entry; verifies
  the binary exits cleanly

**Acceptance gate for #691:**

Issue #691 is closed when ALL of the following are true:
1. `frontier-cli --debug-tui [--system-root DB]` enters TUI mode
2. Script source loads in the left pane on `debug/suspended`
3. Call stack and locals load in the right pane on `debug/suspended`
4. F5/F10/F11/Shift-F11/F9 work
5. Cmd-double-click resolves a local identifier (minimum: shows value in right pane)
6. `q` exits cleanly, terminal is restored
7. `./tools/run_headless_tests.sh` passes
8. `cd tests && make test-integration` passes
9. No crash on SIGTERM

#### Files to modify

`frontier-cli/debugger_tui.c`:
- Final teardown sequence (lazy-attach drain, per above)
- Signal handling glue if needed
- UX polish pass

`tests/debugger_tui_test.sh`:
- Full lifecycle test
- SIGTERM test

#### Tests

`test_tui_full_lifecycle`:
- Mock-backend version of the full session: init, suspended notification, F5
  continue, second suspended notification, q-quit
- Verify all state transitions are correct and no memory errors

Shell: `tests/debugger_tui_test.sh`:
- Startup + SIGTERM test (already written in B.0; expand)
- `--debug-tui --skip-startup` exits status 0 after `q` injected via a PTY script
  (or via `expect` if available; or just SIGTERM after brief delay)

#### Verification gate

Both test layers must pass. This is the closing PR for Phase B / issue #691.
Main-session verification: YES -- full runtime + TUI integration test.

#### Dependencies

B.5 complete.

#### Out of scope

Outline editor TUI (Phase C). Boxen extraction to standalone repo (Phase E).

#### Sentinels / fragile assumptions

SENTINEL: `debug_wait_lazy_threads_drained()` is documented to release and reacquire
the GIL on each 10ms poll cycle. This means other threads may overwrite C globals
while the drain is running. The hglobals snapshot/restore pattern (above) is the fix.
This pattern was wrong in the original PR #722 submission and caught in round 2.
Do not skip it.

SENTINEL: `boxen_shutdown()` calls the backend's `shutdown()` which calls
`tb_shutdown()` in termbox2. This restores the terminal. MUST be called even on
error paths. Use a cleanup label or a flag variable: `bool boxen_initialized = false`;
set to `true` after `boxen_init` succeeds; check before calling `boxen_shutdown`.

---

## 6. Closing Notes

### Closing condition for issue #691

Issue #691 is closed by the final acceptance gate in milestone B.6 above.
"Interactive UserTalk debugger TUI" is the deliverable. The protocol-mode debug
path (PR #722) was the enabler; Phase B is the consumer.

### Toward Phase C (outline editor as second consumer)

Phase C is the validation that boxen's API is actually clean across two surfaces
with different needs. The outline editor needs: scrollable nested content,
in-place text editing, focus model for "edit this node's text" vs "navigate structure."
Phase B's debugger TUI exercises: split panes, pinned footer, modal overlays,
cmd-click, row highlight, scroll-to-position. The union covers most of the
surface inventory.

If Phase B reveals a boxen API gap (e.g., cursor management for inline text editing,
per the B.4 sentinel on condition modal text input), that gap should be documented as
a "boxen-side follow-up" comment in the relevant milestone's code, then addressed in
a small boxen PR before Phase C begins. Phase B should NOT accumulate boxen changes
inline -- the substrate is frozen. One exception: if a boxen gap prevents the TUI
from being usable at all (not just suboptimal), it is a Phase B blocker and gets its
own sub-PR before the affected milestone PR.

### Transport signature divergence (handoff doc correction needed)

The handoff doc (`DEBUGGER_TUI_HANDOFF.md`) states `write_line` takes `(ctx, line)`.
The actual signature in `op_handler.h:45` is `(ctx, json, len)`. Update the handoff
doc to reflect the correct 3-parameter signature. The correction is low-priority
(the handoff doc is not executed); but it prevents confusion for Phase C authors.

### Extraction readiness

After Phase B, boxen has two consumers: the debugger TUI and the `split_demo` example.
The debugger TUI is the more complex consumer and provides the real API validation
that the OVERVIEW's extraction roadmap requires. Phase C (outline editor) is the
second distinct surface. Once both Phase B and Phase C are complete, Phase D (backend
interface freeze) and Phase E (standalone extraction) are straightforward execution.

---

## 7. Quick-Reference Index

| Milestone | Primary files | Key dependencies |
|-----------|--------------|-----------------|
| B.0 -- skeleton | `debugger_tui.{c,h}` (new), `cli_parser.{c,h}`, `main.c` | `boxen.h`, `debug_handler.h`, `protocol_handler.c` (pattern) |
| B.1 -- script pane | `debugger_tui.c` | `debug_handler.c:2082` (`handle_debug_getsource`) |
| B.2 -- stack/locals | `debugger_tui.c` | `debug_handler.c:2011` (`handle_debug_getstack`), `debug_handler.c:1900` (`handle_debug_getlocals`) |
| B.3 -- keybinds | `debugger_tui.c` | `debug_handler.c:1401` (`handle_debug_step`), `op_handler.h` (`op_dispatch`) |
| B.4 -- breakpoints | `debugger_tui.c` | `debug_handler.h:144-145` (`handle_debug_setbreakpoint`, `handle_debug_clearbreakpoints`) |
| B.5 -- cmd-click + watchpoints | `debugger_tui.c` | `boxen.h:406` (`BOXEN_HIT_CHROME`), `backend_mock.h:95` (`boxen_mock_push_double_click`), `debug_handler.h:151` (`handle_debug_setwatchpoint`) |
| B.6 -- polish + integration | `debugger_tui.c`, `tests/debugger_tui_test.sh` | `debug_handler.h:214,226` (`debug_set_attach_transport`, `debug_wait_lazy_threads_drained`), `protocol_handler.c:411-424` (teardown pattern) |
| Tests (all milestones) | `tests/debugger_tui_tests.c` (new), `tests/debugger_tui_test.sh` (new) | `frontier-cli/boxen/backend_mock.h`, `tests/test_report.h` |

---

## 8. Summary Table: All Decisions

| Decision | Resolution | Section |
|----------|------------|---------|
| CLI flag name | `--debug-tui` (not `--tui`; avoids ambiguity with future TUI surfaces) | B.0 |
| Mutual exclusion enforcement | CLI error if both `--protocol` and `--debug-tui` are set | B.0 |
| Transport write_line signature | `(ctx, json, len)` -- 3 params; handoff doc wrong | 2.1 |
| Dispatch to debug ops | Via `op_dispatch` with synthesized JSON (not direct handle_debug_* calls) | B.3 |
| GIL release pattern | Mirror `protocol_handler.c:349-354` exactly | B.0 |
| Teardown pattern | Mirror `protocol_handler.c:411-424` exactly | B.0, B.6 |
| Transport allocation | Heap-allocated; not stack-local | B.0 |
| Row highlight for current line | Draw manually with BOXEN_ATTR_REVERSE (not boxen_window_set_row_highlight) | B.1 |
| Line number translation | ODB 1-based to boxen 0-based: content_row = line_num - 1 | B.1 |
| Stack display order | Outermost to innermost (matches debug/getStack) | B.2 |
| Frame-local selection | Locals always show innermost suspended frame; no per-frame locals API | B.2 |
| Breakpoint toggle-off | Clear all, readd all except toggled line | B.4 |
| Inline text cursor | Reverse-video cell (block cursor); no boxen cursor API for windows | B.4 |
| Cmd-double-click gesture | Meta + BOXEN_MOUSE_DOUBLE_CLICK on pressed==true | B.5 |
| Identifier resolution scope | Look up in locals first; fallback to script/eval | B.5 |
| Watchpoint for non-scalar types | Deferred (Phase B scalar-only) | Non-goals |
| WS-attached TUI debug | Deferred (UAF risk) | Non-goals |
| boxen API churn | Zero; Phase A is frozen; gaps filed as follow-ups | Non-goals |
| TUI unit test harness | C unit tests via backend_mock + test_report.h TR_RUN | 4.1 |
| TUI integration test | Shell test for lifecycle; `sequential: true` in runner | 4.2 |
