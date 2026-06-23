/*
 * boxen_ui.h -- UI primitives for UserTalk verbs inside the boxen REPL.
 *
 * 2026-06-23 JES #691: Phase C.0.7g / boxen<->UserTalk UI bridge phase 1.
 * See planning/phase_c/BOXEN_USERTALK_UI_BRIDGE.md.
 *
 * Dialog verbs (dialog.ask, dialog.getString, dialog.getInt,
 * dialog.getPassword) implemented in dialog_prompts.c do raw terminal IO
 * (fprintf(stderr) + tcsetattr + read(STDIN_FILENO)) that bypasses boxen
 * entirely.  Inside the boxen REPL this is broken: the captured stderr
 * never drains while the dispatched script blocks waiting for input, so
 * the prompt is invisible until the script completes.
 *
 * This bridge gives those verbs an alternative path that opens a centered
 * boxen modal window and runs a mini event loop (GIL release / poll /
 * lock / drain pipe) until the user submits or cancels.  When the boxen
 * REPL is NOT active (linenoise mode), boxen_ui_is_active() returns false
 * and the existing raw-terminal path runs unchanged.
 *
 * GIL: every entry point must be called with the GIL held.  Each entry
 * releases and reacquires the GIL inside its mini event loop, matching
 * the discipline of the main boxen_repl event loop.
 */

#ifndef BOXEN_UI_H
#define BOXEN_UI_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------
 * Host registration
 *
 * The boxen REPL calls boxen_ui_set_active(host) once at startup (after
 * boxen_init succeeds and the stdout capture pipe is up) and
 * boxen_ui_set_active(NULL) at shutdown.  Linenoise mode never calls
 * this; boxen_ui_is_active() returns false there.
 *
 * The host struct gives the bridge exactly the dependencies it needs --
 * screen-size lookup and the capture-pipe read fd -- without pulling in
 * boxen_repl_internal.h.  Lifetime hygiene: the bridge stores the host
 * pointer, never the host struct itself (caller-owned).
 * ---------------------------------------------------------------------- */

typedef struct boxen_ui_host {
	/* Read end of the stdout/stderr capture pipe so the mini event loop
	 * can keep the scrollback ring current while waiting for input.
	 * -1 means no capture pipe is active. */
	int capture_pipe_fd;

	/* Drain callback: bridge invokes this once per mini-loop iteration
	 * to forward bytes from capture_pipe_fd into the scrollback ring.
	 * Pass-through to boxen_repl's drain_stdout_into_scrollback so the
	 * bridge stays out of boxen_repl_state_t internals. */
	void (*drain_capture_pipe)(void *ctx);
	void *drain_ctx;

	/* Restore-focus callback: bridge invokes this after closing a modal
	 * to hand keyboard focus back to whichever window the host considers
	 * the "default" caller (typically the REPL input bar).  Nested
	 * modals don't need this -- closing the inner one leaves the outer
	 * still marked modal and the boxen layer picks "last modal wins".
	 * The callback is invoked unconditionally; the host should no-op if
	 * the default focus target is gone. */
	void (*restore_focus)(void *ctx);
	void *restore_focus_ctx;
} boxen_ui_host_t;

void boxen_ui_set_active(const boxen_ui_host_t *host);
bool boxen_ui_is_active(void);

/* -------------------------------------------------------------------------
 * Input primitives
 *
 * Each primitive opens a centered boxen modal window, renders the
 * prompt, and runs a mini event loop releasing the GIL across
 * boxen_poll_event so other GIL-yielding threads (TCP callbacks etc.)
 * keep making progress.
 *
 * Return semantics:
 *   - On Enter: input committed, returns the result.  String-returning
 *     primitives heap-allocate; caller must free().
 *   - On Esc / Ctrl-C: cancelled.  Returns NULL or false.  Cooperative
 *     cancel only -- the dispatched script receives the cancel return
 *     and decides what to do (Phase 1 deliberately does NOT hard-kill
 *     the script via flthreadkilled; see
 *     planning/phase_c/BOXEN_USERTALK_UI_BRIDGE.md decision #3 and the
 *     2026-06-23 followup that ratified cooperative-only).
 *
 * Must be called with the GIL held.
 * ---------------------------------------------------------------------- */

/* String input with an optional default value.  Returns NULL on cancel. */
char *boxen_ui_get_string(const char *prompt, const char *default_val);

/* Integer input with a default value.  Returns true on success and
 * stores the value in *out; returns false on cancel (out unchanged). */
bool boxen_ui_get_int(const char *prompt, long default_val, long *out);

/* Masked password input (asterisks per char).  Returns NULL on cancel. */
char *boxen_ui_get_password(const char *prompt);

/* 2026-06-23 JES #691 Phase C.0.7g Phase 2A: info-and-wait modal.
 * Shows `message` in a centered modal with a "Press Enter to continue"
 * hint.  If `beep` is true, rings the terminal bell on open.  Returns
 * true on Enter, false on Esc / Ctrl-C.
 *
 * Bridges dialog.alert(message) -> beep=true and dialog.notify(message)
 * -> beep=false. */
bool boxen_ui_alert(const char *message, bool beep);

/* 2026-06-23 JES #691 Phase C.0.7g Phase 2A: button-selector modal.
 * Shows `prompt` above a horizontal row of buttons; Left/Right move
 * the selection; Enter chooses; Esc / Ctrl-C cancel.  Hotkey: a typed
 * letter matching the first character of a button label (case-
 * insensitive) selects + activates that button immediately, matching
 * the legacy dialog_twoway/threeway behavior.
 *
 * `buttons` is an array of `count` C strings (must be 2..4).  Returns
 * the 1-based index of the chosen button, or 0 on cancel.
 *
 * Bridges dialog.twoway (count=2) and dialog.threeway (count=3). */
int boxen_ui_button_select(const char *prompt,
                           const char *const *buttons, int count);

#ifdef __cplusplus
}
#endif

#endif /* BOXEN_UI_H */
