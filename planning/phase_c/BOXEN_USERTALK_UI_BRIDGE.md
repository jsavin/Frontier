# Boxen ↔ UserTalk UI Bridge

**Status:** Phase 1 SHIPPED 2026-06-23.  Phase 2 design.
**Authors:** JES + system-architect agent investigation (2026-06-22), revised via `/gate` 2026-06-23.
**Scope:** Make all interactive UserTalk verbs (`dialog.*`, `file.*`, `msg`) compose correctly with the boxen REPL.

> **Reader note — Phase 1 actually-shipped vs. design.**  This document is the original design plus phase-history annotations.  The Architecture section below still references API surfaces (`boxen_ui_ask`, `boxen_ui_script_aborted`, `boxen_ui_alert`, button selectors) that are Phase 2 work, not Phase 1.  See the **Phasing** section near the bottom for the canonical list of what landed in Phase 1.  When the architecture section and the phasing section disagree, the phasing section wins — it's the post-implementation reality.

---

## Problem

The boxen REPL (`--debug-tui`, target default per Phase C.6) composites the terminal via boxen panes/windows. UserTalk scripts dispatched from the boxen REPL invoke legacy interactive verbs that were written for the linenoise REPL. Those verbs do raw terminal IO that bypasses boxen, producing broken UX:

- `/R J` (Jump): `dialog.ask("Jump to table:", @result)` — prompt invisible. User reports "left-arrow makes it appear."
- `/R K` (Key codes): same shape.
- `/R H`, `/R C`, `/R E`: work — these handlers don't call dialog verbs.

The class of problem is "boxen ↔ UserTalk-modal-IO composition," not per-verb. PR #791 and PR #792 fixed lifetime bugs and got dispatch working at all. This bridge resolves how dispatched scripts produce and receive interactive UI.

### Verified failure mode

1. `on_palette_done` dispatches the menu script via `meuserselected_headless` (synchronous, holding the GIL).
2. The script calls `dialog.ask("Jump to table:")` → `dialog_twoway()` in `frontier-cli/dialog_prompts.c:131`.
3. `dialog_twoway` writes the prompt via `fprintf(stderr, ...)`. stderr is dup2'd to the stdout-capture pipe at `boxen_repl.c:1815`, so the bytes go into the pipe buffer.
4. `dialog_twoway` calls `terminal_read_key()` — reads stdin directly, bypassing boxen.
5. The boxen event loop is blocked: dispatch is synchronous and runs on the event-loop thread. The drain of the capture pipe + `boxen_present()` never run while the script is in the dialog.
6. The prompt bytes sit in the pipe, unrendered.
7. The user presses left-arrow. `terminal_read_key()` consumes it (toggles button selection in `dialog_twoway`), then re-emits the prompt with `\r\033[K` redraw + reprint. Still no present.
8. Only when the user finishes the dialog (Enter), `dialog_twoway` returns, the script returns, `meuserselected_headless` returns, control returns to the event loop, which drains the pipe and presents — at which point the entire accumulated cycle (prompt + arrow-redraw + typed input) lands all at once.

The "left-arrow makes the prompt appear" symptom is the user observing the final batch-render, correlated with the first keypress.

### Surface map

All verbs that do interactive terminal IO from inside a dispatched script:

| Verb | C entry | Pattern |
|---|---|---|
| `dialog.ask()` | `dialog_ask` → `dialog_twoway` | Yes/No button selector |
| `dialog.getString()` | `dialog_get_string` | string input |
| `dialog.getInt()` | `dialog_get_int` | integer input |
| `dialog.getPassword()` | `dialog_get_password` | masked input |
| `dialog.alert()` | `dialog_alert` | info + Enter to dismiss |
| `dialog.notify()` | `dialog_notify` | info + Enter, no beep |
| internal | `dialog_twoway` | 2-button selector |
| internal | `dialog_threeway` | 3-button selector |
| `file.getFileDialog()` etc. | `file_browser.c` (~1000 LOC, full-screen TUI) | file picker |
| `repl.printKeyCodes()` | `replverbhost_print_key_codes` (`repl.c:3544`) | debug tool, ESC-x3 to exit |

`msg()` already works in boxen mode: it goes through the `langcallbacks.msgverbcallback` → stdout → capture pipe → scrollback. No bridging needed.

---

## Goals + non-goals

### Goals

1. All interactive UserTalk verbs work correctly inside the boxen REPL — prompt visible immediately, input received, result returned.
2. The same verb bodies work in both linenoise mode and boxen mode. Branch lives inside the bridge (one place), not per-verb (many places).
3. The bridge provides first-class primitives that future UserTalk-driven UI can build on: modal string input, modal yes/no, modal N-way button, modal info/wait. Extensibility hook for more.
4. No new GIL complexity. Bridge composes with the existing single-owner GIL model.

### Non-goals

1. True parallelism. No new threads in this work.
2. Replacement of the existing stdout capture pipe. Bridge bypasses it for interactive UI but leaves it in place for normal `print`/`msg` output.
3. Changing the UserTalk surface of any verb. Callers don't see the bridge; they see the same verb signatures.
4. `linenoisePrintKeyCodes` — debug-only tool, not a UserTalk verb. Gated separately (one-line guard with a "use linenoise mode" scrollback message).

---

## Decisions (JES, 2026-06-22)

| # | Decision | Rationale |
|---|---|---|
| 1 | **Modal position: center screen.** | Visually distinct from the palette (which anchors near the top). Users won't confuse a dialog with a menu. |
| 2 | **File dialogs INCLUDED in this work, but as Phase 2 PR.** Phase 1 ships the 4 input prompts (`ask`/`getString`/`getInt`/`getPassword`). Phase 2 ships the file picker as a separate PR after Phase 1 lands. | Splits review burden. `/R J` working unblocks user immediately; file picker is its own substantial scope (~1000 LOC TUI rewrite). |
| 3 | **Ctrl-C in modal: cancel dialog ONLY; do NOT interrupt the dispatched script.** Cooperative cancel — modal returns NULL/false, the dispatched script receives the cancel return and decides what to do. (Revised 2026-06-23 from the original "cancel AND interrupt" position after JES + system-architect investigation revealed the hard-kill path needed careful reset semantics and the legacy mechanism was acknowledged-imprecise; see the followup discussion of 2026-06-23.) | The dispatched menu scripts we have (jump, keycodes, etc.) all check the dialog return and abort gracefully. No observed script needs hard kill. Hard kill via `flthreadkilled` is recorded as a follow-up to revisit when a real script demonstrates the need. |
| 4 | **Stdout pipe drain inside the mini event loop: YES.** Each mini-loop iteration drains the capture pipe before painting. | Other GIL-yielding work (background threads) could produce output during the dialog wait. Keeps scrollback current under the modal. |
| 5 | **Resize during modal:** modal repositions itself on `BOXEN_EV_RESIZE` (recompute center rect, call `boxen_window_set_rect`). Background windows resize via their existing handlers since `boxen_dispatch_event` runs every event. | Verify during Phase 1 implementation. |
| 6 | **Bridge state: no `boxen_repl_state_t` pointer in `boxen_ui.c`.** `boxen_ui_set_active()` takes only the pieces the bridge needs (screen-size getter, drain fd, present fn pointer). | Lifetime hygiene. We just spent two PRs untangling cross-module state bugs. |

---

## Architecture

### Core insight: dispatch suspension

The boxen event loop is single-threaded. When a script is dispatched, the event loop is blocked for the script's duration. The palette solved this by completing user interaction BEFORE the script runs — palette finishes, fires `on_palette_done`, script dispatches into a non-modal terminal state.

Dialog verbs fire FROM INSIDE a running script (mid-call). The script is already running under `meuserselected_headless`. The event loop is already blocked.

The fix: when a dialog verb fires under the boxen REPL, the verb must:

1. Render its UI directly to the boxen framebuffer (bypass the stdout capture pipe).
2. Release the GIL and pump the boxen event loop in a **mini-loop** until the user completes the interaction.
3. Return the result to the UserTalk caller, GIL reacquired.

This is the same GIL-release / poll / reacquire pattern the main event loop uses (`boxen_repl.c:1891-1896`), with a tighter `done` condition local to the modal.

### Architectural diagram

```
UserTalk script (holds GIL, running under meuserselected_headless)
    |
    v
dialog.ask("Jump to table:", @result)
    |
    v
[C verb implementation in dialog_prompts.c]
    |
    +-- boxen_ui_is_active()? --NO--> [existing linenoise path: fprintf(stderr) + terminal_read_key]
    |
    YES
    v
boxen_ui_ask(prompt)
    |
    +-- opens a boxen modal window (center screen, sized to prompt + buttons)
    +-- paints initial content via boxen_set_cell
    +-- drains capture pipe -> scrollback
    +-- boxen_present()
    |
    v
[mini event loop -- REPLACES the terminal_read_key() loop]
    |
    while (!ctx.done && !boxen_ui_script_aborted()):
        save hthreadglobals
        pthread_mutex_unlock(&frontier_gil)
        rc = boxen_poll_event(&ev, 100ms)
        pthread_mutex_lock(&frontier_gil)
        restore hthreadglobals
        |
        drain capture pipe -> scrollback
        if rc == TIMEOUT: continue
        if rc != OK: cancel
        |
        boxen_ui_feed_event(ctx, ev)
            -- arrow keys -> toggle selection / move cursor
            -- printable -> append to input buf
            -- Enter -> ctx.done = true
            -- Esc -> ctx.cancelled = true, ctx.done = true
            -- Ctrl-C -> ctx.cancelled = true, ctx.done = true,
                        boxen_ui_signal_script_abort()
            -- repaint via boxen_set_cell + present
    |
    v
boxen_ui_end_modal(ctx)
    |
    +-- closes the window
    +-- returns result to verb implementation
    |
    v
UserTalk receives return value
[script checks boxen_ui_script_aborted() at next langbackgroundtask yield]
```

### Public surface: `boxen_ui.h`

```c
/* boxen_ui.h -- UI primitives for UserTalk verbs inside the boxen REPL.
 *
 * All functions are no-ops (returning defaults) when boxen is not active.
 * Call boxen_ui_is_active() to check before creating modals.
 */

/* Returns true when a boxen REPL session owns the terminal. */
bool boxen_ui_is_active(void);

/* Returns true if the user pressed Ctrl-C inside a boxen modal.
 * UserTalk runtime checks this at yield points (langbackgroundtask,
 * thread.sleepTicks) and aborts the running script if set. Cleared
 * automatically when the abort propagates back to the dispatch boundary
 * (boxen_repl_real_palette_dispatch). */
bool boxen_ui_script_aborted(void);

/* Input primitives. Must be called with the GIL held.
 * Return NULL/false on cancel (Esc, Ctrl-C). Caller frees returned strings. */
char *boxen_ui_get_string(const char *prompt, const char *default_val);
bool  boxen_ui_get_int(const char *prompt, long default_val, long *out);
char *boxen_ui_get_password(const char *prompt);

/* Yes/No + N-way modal selectors. */
bool boxen_ui_ask(const char *prompt);
bool boxen_ui_twoway(const char *prompt, const char *b1, const char *b2);
int  boxen_ui_threeway(const char *prompt,
                       const char *b1, const char *b2, const char *b3);

/* Info modal -- waits for Enter. beep=true rings the terminal bell. */
void boxen_ui_alert(const char *message, bool beep);

/* File picker -- Phase 2. */
/* char *boxen_ui_get_file(const char *prompt, const char *default_path,
 *                         boxen_ui_file_mode_t mode); */
```

### Bridge wiring: `boxen_ui_set_active`

```c
typedef struct {
    void (*get_screen_size)(int *w, int *h);
    void (*present)(void);
    int  drain_fd; /* read end of the stdout capture pipe; -1 if none */
} boxen_ui_host_t;

/* Called by boxen_repl_main after boxen_init() and the capture pipe are up.
 * Cleared (boxen_ui_set_active(NULL)) before boxen_shutdown() in teardown. */
void boxen_ui_set_active(const boxen_ui_host_t *host);
```

`boxen_ui.c` holds a file-static `boxen_ui_host_t *` (or NULL). `boxen_ui_is_active()` returns `(host != NULL)`. The bridge calls `host->present()` and `host->get_screen_size()` rather than including `boxen_repl_internal.h` — keeps the bridge from depending on the REPL host struct.

### Mini event loop (sketch)

```c
static char *run_string_input_modal(const char *prompt, const char *def) {
    boxen_ui_string_ctx_t ctx = {0};
    ctx.prompt = prompt;
    strncpy(ctx.buf, def ? def : "", sizeof(ctx.buf) - 1);
    ctx.len    = strlen(ctx.buf);
    ctx.cursor = ctx.len;

    boxen_window_t *win = open_centered_input_window(prompt, &ctx);
    if (win == NULL) return def ? strdup(def) : strdup("");
    repaint_string_input(win, &ctx);
    drain_pipe();
    s_host->present();

    while (!ctx.done) {
        boxen_event_t ev = {0};

        hdlthreadglobals saved;
        headless_save_threadglobals(&saved);
        pthread_mutex_unlock(&frontier_gil);
        boxen_result_t rc = boxen_poll_event(&ev, 100);
        pthread_mutex_lock(&frontier_gil);
        headless_restore_threadglobals(&saved);

        drain_pipe(); /* per decision #4 -- keep scrollback current */

        if (rc == BOXEN_ERR_TIMEOUT) continue;
        if (rc != BOXEN_OK)         { ctx.cancelled = true; break; }

        feed_string_input_event(&ctx, &ev, win);
        repaint_string_input(win, &ctx);
        s_host->present();
    }

    boxen_window_close(win);
    if (ctx.cancelled) return NULL;
    return strdup(ctx.buf);
}
```

The GIL-release window (`unlock` → `poll_event` → `lock`) is identical to the main event loop. Other GIL-yielding threads can run during the yield — correct behavior.

### How `dialog_prompts.c` changes

Each public function gets a 2-3 line branch at the top:

```c
bool dialog_ask(const char *prompt) {
    if (boxen_ui_is_active()) {
        return boxen_ui_ask(prompt);
    }
    return dialog_twoway(prompt, "Yes", "No");
}

char *dialog_get_string(const char *prompt, const char *default_value) {
    if (boxen_ui_is_active()) {
        return boxen_ui_get_string(prompt, default_value);
    }
    if (!isInteractiveMode()) return NULL;
    /* ... existing fprintf(stderr) + read_line_with_editing() unchanged ... */
}
```

The branch is at the top. The linenoise path is untouched. When the linenoise REPL is active, `boxen_ui_set_active(NULL)` was never called, `boxen_ui_is_active()` returns false, the branch is skipped, and the code behaves exactly as it does today.

### Ctrl-C / script abort

Per decision #3: cooperative kill via a flag.

1. User presses Ctrl-C inside a boxen modal.
2. The mini event loop sees `BOXEN_KEY_CTRL_C` and:
   - Sets `ctx.cancelled = true` and `ctx.done = true` (the modal closes, the verb returns NULL/false).
   - Sets a file-static `s_script_abort_requested = true` in `boxen_ui.c`.
3. The verb returns NULL/false to the UserTalk caller. The UserTalk script may handle the cancel or propagate.
4. At the next UserTalk yield point (`langbackgroundtask`, `thread.sleepTicks`), the runtime checks `boxen_ui_script_aborted()` and aborts the running script (mechanism: same as legacy Esc — flag-driven script termination).
5. When the dispatched script terminates (normal completion or abort), `boxen_repl_real_palette_dispatch` clears the flag before returning to the event loop.

**Implementation note:** The mechanism for "abort the running script at the next yield point" needs a small hook into the UserTalk runtime — most likely in `langbackgroundtask()` and `thread.sleepTicks()`. The hook checks `boxen_ui_script_aborted()` and, if set, raises a UserTalk error that propagates out to the dispatch boundary. **This hook is Phase 1 work** since Phase 1 implements `boxen_ui_ask` and we want Ctrl-C to work correctly from day one.

**Caveat (acknowledged by JES):** in legacy Frontier this mechanism was imprecise once the runtime became multi-threaded — Esc would kill *a* thread, not necessarily the one the user meant. The boxen REPL today dispatches one menu script at a time, so this ambiguity doesn't apply yet. Future multi-script-dispatch scenarios will need a more targeted abort (e.g., per-thread abort flag keyed off the modal's owning script). Out of scope here.

### Reentrancy

A UserTalk script may call `dialog.ask()` from inside a `dialog.ask()` callback (legal, unlikely). Each `boxen_ui_*` call opens its own modal window and runs its own mini event loop. The modal stack is inherently serial under the GIL — the innermost mini loop has the GIL; outer loops are suspended in `boxen_poll_event` waiting for the inner loop to release. On return, modals close in LIFO order (z-order is automatic via boxen window stacking). No special machinery needed.

### The architectural contract

> **When `boxen_ui_is_active()` is true, no interactive UserTalk verb may call `fprintf(stderr, ...)`, `terminal_read_key()`, or `terminal_set_raw_mode()`. All terminal interaction must go through `boxen_ui_*` functions.**

Enforced by code review. A future static-analysis pass could detect direct `terminal_read_key` calls reachable from verb implementations and flag them.

The bridge does NOT enforce the stdout capture pipe bypass — it bypasses it implicitly because `boxen_ui_*` writes to `boxen_window` cells, not stdout/stderr. The pipe captures remain in place for the verb's normal print output; only the interactive UI goes through the bridge.

---

## Phasing

### Phase 1 — Bridge skeleton + input prompts

**Status: shipped 2026-06-23 (commit TBD on merge).** What actually landed:

**Shipped:**
- New TU: `frontier-cli/boxen_ui.c` + `boxen_ui.h`
- Public API: `boxen_ui_is_active`, `boxen_ui_set_active`, `boxen_ui_get_string`, `boxen_ui_get_int`, `boxen_ui_get_password`
- Branches in `dialog_prompts.c` for `dialog_get_string`, `dialog_get_int`, `dialog_get_password`
- **Branches in `tests/headless_dialog_verbs.c` for `diav_ask` / `diav_getint` / `diav_getuserinfo` / `diav_getpassword`** — necessary because the verb glue's `isInteractiveMode()` check would otherwise short-circuit the bridge in tmux/non-TTY environments before the `dialog_prompts.c` branch could fire (discovered via `/gate` 2026-06-23)
- `boxen_repl_main` wiring (set/clear the host) with `restore_focus` and `drain_capture_pipe` callbacks
- Mini event loop with GIL release / `pthread_cond_broadcast(&gil_available)` / `tcp_process_callbacks` / `flthreadkilled` check (mirrors `headless_backgroundtask`)
- `_Atomic` `s_host` with snapshot-once-per-entry pattern

**Deliberately deferred (NOT in Phase 1):**
- ~~`boxen_ui_script_aborted` + UserTalk-runtime Ctrl-C abort hook~~ — decision #3 revised to cooperative-cancel-only. The bridge sets nothing on `flthreadkilled`; dispatched scripts receive the dialog cancel and decide.
- ~~`repl.printKeyCodes()` guard~~ — orthogonal one-line guard, deferred to its own follow-up so this PR stays scoped to the bridge.
- ~~`dialog_ask` (yes/no) C function branch~~ — confirmed dead code at this commit. The `dialog.ask` UserTalk verb dispatches to `dialog_get_string` (text input with pre-fill), NOT `dialog_ask` (yes/no selector). The yes/no `dialog_ask` C function has no callers post-Phase 1.

**Smoke tests:**
- `/R J` in `--debug-tui`: dialog appears immediately, Enter returns input, `jump.ut` completes successfully ✓
- `/R J`: typed characters render live in the modal ✓
- `/R J`: Esc cancels cleanly ✓
- Linenoise mode regression check: dialog.* still work via the unchanged fall-through path ✓ (covered by absence of test_06_ui_bridge failures in linenoise builds)

**Known limitations in shipped form:**
- Input buffer is fixed 256 bytes; legacy reader grew unbounded via realloc. Silent truncation past 256 chars. Tracked as a Phase 1.1 follow-up; not blocking.
- Non-ASCII input codepoints dropped (printable check is `>= 0x20 && < 0x7F`). Tracked as a UTF-8 follow-up.
- `dialog_get_int` returns cancel on parse failure instead of re-prompting (legacy re-prompts in a loop). Acknowledged in code; not blocking.
- Nested modals: when an inner modal closes, focus restores to the host's default (REPL input bar), not the outer modal. The outer's mini-loop re-focuses on its next 100ms tick. Tracked as a Phase 2 candidate (proper modal-stack in boxen core).

### Phase 2 — Remaining dialog verbs + file picker

**Deliverable:**
- `boxen_ui_alert`, `boxen_ui_twoway`, `boxen_ui_threeway`
- `boxen_ui_get_file` (boxen-native file picker — substantial rewrite of `file_browser.c` rendering)
- Branches in `dialog_prompts.c` for `dialog_alert`, `dialog_notify`, `dialog_twoway`, `dialog_threeway`
- Branch in `file_browser.c` / `file_dialog.c` (TBD which file owns the entry point)

**Smoke tests:**
- Every dialog verb in `tests/PHASE2_INTERACTIVE_TESTS.md` exercised in `--debug-tui`
- File dialog: open / navigate / select / cancel all working

### Cleanup (post-C.6)

Once Phase C.6 flips boxen to the default and linenoise is deprecated, the linenoise-mode branches in `dialog_prompts.c` can be deleted. Mechanical removal; no design work.

---

## Open follow-ups (post-design)

These get addressed during implementation, not now:

- **Modal styling.** Border characters, color scheme, focus indicator. Inherit from palette? Bespoke?
- **Multi-script-dispatch abort precision** (when/if the boxen REPL ever dispatches multiple scripts concurrently). Per-thread abort flag keyed off owning modal.
- **Verb test injection.** Tests can't easily drive a `boxen_ui_get_string` directly — needs a test-build escape hatch (e.g., `boxen_ui_test_set_canned_response("foo")`) that returns the canned value without running the modal. Same shape as the palette's test hooks.
- **Modal accessibility for the harness.** The tui-tests harness should be able to detect "a boxen modal is open" and read its content for snapshot tests.
