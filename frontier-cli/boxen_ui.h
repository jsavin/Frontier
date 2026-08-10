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
#include <stddef.h>		/* size_t */

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

	/* 2026-08-10 JES unit 2.4 hardening: debug-notification drain.
	 * Invoked next to drain_capture_pipe each mini-loop iteration so a
	 * debug/suspended notification that lands while a dialog modal is
	 * open renders immediately (scrollback line + footer hint) instead
	 * of sitting in the REPL's debug queue until the dialog closes.
	 * Pass-through to boxen_repl_drain_debug_notifications.  NULL is
	 * allowed (no-op) for hosts without a debug surface. */
	void (*drain_debug)(void *ctx);
	void *drain_debug_ctx;

	/* Restore-focus callback: bridge invokes this after closing a modal
	 * to hand keyboard focus back to whichever window the host considers
	 * the "default" caller (typically the REPL input bar).  Nested
	 * modals don't need this -- closing the inner one leaves the outer
	 * still marked modal and the boxen layer picks "last modal wins".
	 * The callback is invoked unconditionally; the host should no-op if
	 * the default focus target is gone. */
	void (*restore_focus)(void *ctx);
	void *restore_focus_ctx;

	/* Ring-bell callback: bridge invokes this when the user-facing
	 * modal needs to produce an audible cue (currently only
	 * dialog.alert).  The host writes \a (or equivalent) directly to
	 * the real terminal fd, bypassing the boxen stdout/stderr capture
	 * pipe -- if the bridge wrote \a to its own captured stderr the
	 * byte would be drained into the scrollback as a visible ^G glyph
	 * rather than ringing the terminal bell.  NULL is allowed (no-op
	 * = silent). */
	void (*ring_bell)(void *ctx);
	void *ring_bell_ctx;
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

/* 2026-06-24 JES #691 Phase C.0.7g Phase 2B: file-picker modal.
 *
 * Bridges the four UserTalk file dialog verbs to a boxen-native
 * single-pane list picker.  Layout: prompt + breadcrumb header, list
 * of entries (directories first, then files), separator, hint row;
 * PUT_FILE mode adds a filename input row above the hint.
 *
 * `mode` selects the dialog flavour:
 *   - BOXEN_UI_PICK_GET_FILE   -- file.getFileDialog: pick existing file
 *   - BOXEN_UI_PICK_PUT_FILE   -- file.putFileDialog: pick directory + type filename
 *   - BOXEN_UI_PICK_GET_FOLDER -- file.getFolderDialog: pick existing folder
 *   - BOXEN_UI_PICK_GET_DISK   -- file.getDiskDialog: pick a mount point
 *
 * `start_path` is the initial directory (NULL = current working dir).
 *   PUT_FILE accepts a path with trailing filename component as the
 *   default filename.  GET_DISK ignores it (browses /Volumes/ on
 *   macOS).
 *
 * `type_filter` (GET_FILE only) is a file extension WITHOUT the dot
 *   (e.g. "txt").  Non-matching files are dimmed but still selectable
 *   per JES decision 2026-06-24.  NULL means no filter.
 *
 * `out_path` receives the absolute path of the chosen file/folder/
 *   volume; for PUT_FILE this is the directory joined with the
 *   filename input.  `out_cap` is the buffer capacity (caller-sized).
 *
 * Returns true on commit, false on Esc/Ctrl-C cancel (out_path
 * untouched).
 *
 * Must be called with the GIL held.  Cooperative cancel only -- see
 * the 2026-06-23 followup to decision #3 in the design doc.  */
typedef enum {
	BOXEN_UI_PICK_GET_FILE,
	BOXEN_UI_PICK_PUT_FILE,
	BOXEN_UI_PICK_GET_FOLDER,
	BOXEN_UI_PICK_GET_DISK,
} boxen_ui_pick_mode_t;

bool boxen_ui_pick_file(boxen_ui_pick_mode_t mode,
                        const char *prompt,
                        const char *start_path,
                        const char *type_filter,
                        char *out_path, size_t out_cap);

#ifdef __cplusplus
}
#endif

#endif /* BOXEN_UI_H */
