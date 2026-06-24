/*
 * boxen_ui.c -- UI primitives for UserTalk verbs inside the boxen REPL.
 *
 * 2026-06-23 JES #691: Phase C.0.7g / boxen<->UserTalk UI bridge phase 1.
 * See planning/phase_c/BOXEN_USERTALK_UI_BRIDGE.md.
 *
 * Bridge for dialog verbs (dialog.ask / dialog.getString / dialog.getInt
 * / dialog.getPassword) that need to compose with boxen instead of doing
 * raw terminal IO.  Opens a centered boxen modal window for the prompt
 * and runs a mini event loop releasing the GIL across boxen_poll_event,
 * mirroring the discipline of the main REPL loop
 * (boxen_repl.c:1887-1939).
 */

#include "boxen_ui.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "boxen/boxen.h"
#include "headless_threading.h"     /* frontier_gil, gil_available, hthreadglobals, save/restore */
#include "../Common/headers/threadregistry.h" /* hdlthreadglobals */
#include "../Common/headers/tcpverbs.h"       /* tcp_process_callbacks */

/* -------------------------------------------------------------------------
 * Host registration
 * ---------------------------------------------------------------------- */

/* Set to non-NULL by boxen_repl_main once the REPL has finished bringing
 * boxen up; reset to NULL during teardown.  Linenoise mode never touches
 * it.  Single-writer (the REPL main thread).  Atomic so a future thread
 * that survives debug_join_all_threads cannot observe a torn pointer if
 * it races teardown; today the invariant is "no live threads when set
 * to NULL," but encoding it in the type is cheap insurance for the long
 * haul. */
static _Atomic(const boxen_ui_host_t *) s_host = NULL;

void boxen_ui_set_active(const boxen_ui_host_t *host) {
	atomic_store_explicit(&s_host, host, memory_order_release);
}

bool boxen_ui_is_active(void) {
	return atomic_load_explicit(&s_host, memory_order_acquire) != NULL;
}

/* Local helper for the bridge to snapshot the host pointer once per
 * entry-point so we don't risk read-then-deref on a torn store. */
static const boxen_ui_host_t *load_host(void) {
	return atomic_load_explicit(&s_host, memory_order_acquire);
}

/* -------------------------------------------------------------------------
 * Modal context shared by the input primitives
 *
 * Each primitive constructs one of these on its stack, hands it to the
 * mini event loop, then reads out the result.  The cell-rendering and
 * key-handling code keys off the `mode` field so we can share the loop
 * across string / int / password.
 * ---------------------------------------------------------------------- */

#define BOXEN_UI_BUF_MAX 256

typedef enum {
	BOXEN_UI_MODE_STRING,
	BOXEN_UI_MODE_INT,
	BOXEN_UI_MODE_PASSWORD,
} boxen_ui_mode_t;

typedef struct {
	boxen_ui_mode_t mode;
	const char *prompt;       /* prompt label drawn above the input field */

	char  buf[BOXEN_UI_BUF_MAX];
	int   len;                /* current logical length of buf */
	int   cursor;             /* 0 .. len */

	bool  done;               /* user submitted (Enter) or cancelled (Esc/Ctrl-C) */
	bool  cancelled;          /* if done: was it a cancel? */

	boxen_window_t *win;
	int   content_w;          /* cached content-area width */
} ui_ctx_t;

/* -------------------------------------------------------------------------
 * Geometry: place the modal centered horizontally and roughly 1/3 down
 * from the top of the screen.  Decision #1 in the design doc.
 * ---------------------------------------------------------------------- */

static boxen_rect_t place_modal(int screen_w, int screen_h, int min_field_w) {
	/* Window content: prompt label on one line, input field on the next.
	 * With borders the chrome adds 2 cols + 2 rows. */
	int content_w = min_field_w;
	if (content_w < 40) content_w = 40;
	int max_w = screen_w - 4;
	if (max_w < 20) max_w = 20;
	if (content_w > max_w) content_w = max_w;

	int win_w = content_w + 2; /* + 2 for L/R borders */
	int win_h = 5;             /* top border + prompt + blank + field + bottom border = 5 */

	int x = (screen_w - win_w) / 2;
	if (x < 0) x = 0;
	int y = screen_h / 3;
	if (y + win_h > screen_h - 1) y = screen_h - win_h - 1;
	if (y < 0) y = 0;

	boxen_rect_t r = { x, y, win_w, win_h };
	return r;
}

/* -------------------------------------------------------------------------
 * Render
 *
 * boxen calls the registered draw callback whenever a present cycle
 * needs to repaint this window.  We do all cell writes from the
 * callback; the mini event loop signals "state changed" by calling
 * boxen_window_invalidate(ctx->win).
 * ---------------------------------------------------------------------- */

static void render_modal_cb(boxen_window_t *win, void *user_data) {
	(void)win;
	ui_ctx_t *ctx = (ui_ctx_t *)user_data;
	if (ctx == NULL || ctx->win == NULL) return;

	int w = ctx->content_w;

	/* Row 0: prompt label, clipped to width. */
	{
		const char *p = ctx->prompt ? ctx->prompt : "";
		int i;
		for (i = 0; i < w; i++) {
			char ch = p[i];
			if (ch == '\0') break;
			boxen_set_cell(ctx->win, i, 0, (uint32_t)(unsigned char)ch,
			               BOXEN_COLOR_DEFAULT, BOXEN_COLOR_DEFAULT,
			               BOXEN_ATTR_BOLD);
		}
		for (; i < w; i++) {
			boxen_set_cell(ctx->win, i, 0, ' ',
			               BOXEN_COLOR_DEFAULT, BOXEN_COLOR_DEFAULT,
			               BOXEN_ATTR_NONE);
		}
	}

	/* Row 1: blank spacer. */
	for (int i = 0; i < w; i++) {
		boxen_set_cell(ctx->win, i, 1, ' ',
		               BOXEN_COLOR_DEFAULT, BOXEN_COLOR_DEFAULT,
		               BOXEN_ATTR_NONE);
	}

	/* Row 2: input field.  Show "> " prefix, then the buffer with
	 * password masking if applicable. */
	{
		const char *prefix = "> ";
		int prefix_len = (int)strlen(prefix);
		for (int i = 0; i < prefix_len && i < w; i++) {
			boxen_set_cell(ctx->win, i, 2, (uint32_t)(unsigned char)prefix[i],
			               BOXEN_COLOR_DEFAULT, BOXEN_COLOR_DEFAULT,
			               BOXEN_ATTR_DIM);
		}

		int field_x0 = prefix_len;
		int field_w = w - prefix_len;
		if (field_w < 1) field_w = 1;

		/* Horizontal scroll: keep the cursor in view. */
		int scroll = 0;
		if (ctx->cursor >= field_w) {
			scroll = ctx->cursor - field_w + 1;
		}

		for (int i = 0; i < field_w; i++) {
			int buf_idx = scroll + i;
			char ch = ' ';
			if (buf_idx < ctx->len) {
				ch = (ctx->mode == BOXEN_UI_MODE_PASSWORD) ? '*'
				                                           : ctx->buf[buf_idx];
			}
			boxen_set_cell(ctx->win, field_x0 + i, 2,
			               (uint32_t)(unsigned char)ch,
			               BOXEN_COLOR_DEFAULT, BOXEN_COLOR_DEFAULT,
			               BOXEN_ATTR_NONE);
		}

		/* Cursor: place at logical cursor column within the field. */
		int cursor_col = field_x0 + (ctx->cursor - scroll);
		if (cursor_col < 0) cursor_col = 0;
		if (cursor_col >= w) cursor_col = w - 1;
		boxen_window_set_cursor(ctx->win, cursor_col, 2);
		boxen_window_set_cursor_visible(ctx->win, true);
	}
}

/* -------------------------------------------------------------------------
 * Key handling
 * ---------------------------------------------------------------------- */

static bool ch_is_acceptable_int(char c) {
	return (c >= '0' && c <= '9') || c == '-' || c == '+';
}

static void insert_char(ui_ctx_t *ctx, char c) {
	if (ctx->len + 1 >= BOXEN_UI_BUF_MAX) return; /* full */
	if (ctx->mode == BOXEN_UI_MODE_INT && !ch_is_acceptable_int(c)) return;
	/* Shift tail right. */
	if (ctx->cursor < ctx->len) {
		memmove(&ctx->buf[ctx->cursor + 1], &ctx->buf[ctx->cursor],
		        (size_t)(ctx->len - ctx->cursor));
	}
	ctx->buf[ctx->cursor] = c;
	ctx->cursor++;
	ctx->len++;
	ctx->buf[ctx->len] = '\0';
}

static void backspace(ui_ctx_t *ctx) {
	if (ctx->cursor == 0) return;
	if (ctx->cursor < ctx->len) {
		memmove(&ctx->buf[ctx->cursor - 1], &ctx->buf[ctx->cursor],
		        (size_t)(ctx->len - ctx->cursor));
	}
	ctx->cursor--;
	ctx->len--;
	ctx->buf[ctx->len] = '\0';
}

static void delete_forward(ui_ctx_t *ctx) {
	if (ctx->cursor >= ctx->len) return;
	memmove(&ctx->buf[ctx->cursor], &ctx->buf[ctx->cursor + 1],
	        (size_t)(ctx->len - ctx->cursor - 1));
	ctx->len--;
	ctx->buf[ctx->len] = '\0';
}

static void handle_key_event(ui_ctx_t *ctx, const boxen_event_t *ev) {
	if (ev->type != BOXEN_EV_KEY) return;

	switch (ev->key.key) {
	case BOXEN_KEY_ENTER:
		ctx->done = true;
		break;

	case BOXEN_KEY_ESCAPE:
	case BOXEN_KEY_CTRL_C:
		ctx->done = true;
		ctx->cancelled = true;
		break;

	case BOXEN_KEY_BACKSPACE:
	case BOXEN_KEY_CTRL_H:
		backspace(ctx);
		break;

	case BOXEN_KEY_DELETE:
	case BOXEN_KEY_CTRL_D:
		delete_forward(ctx);
		break;

	case BOXEN_KEY_LEFT:
		if (ctx->cursor > 0) ctx->cursor--;
		break;

	case BOXEN_KEY_RIGHT:
		if (ctx->cursor < ctx->len) ctx->cursor++;
		break;

	case BOXEN_KEY_HOME:
	case BOXEN_KEY_CTRL_A:
		ctx->cursor = 0;
		break;

	case BOXEN_KEY_END:
	case BOXEN_KEY_CTRL_E:
		ctx->cursor = ctx->len;
		break;

	case BOXEN_KEY_CTRL_U:
		/* Kill to beginning of line. */
		if (ctx->cursor > 0) {
			memmove(ctx->buf, &ctx->buf[ctx->cursor],
			        (size_t)(ctx->len - ctx->cursor));
			ctx->len -= ctx->cursor;
			ctx->cursor = 0;
			ctx->buf[ctx->len] = '\0';
		}
		break;

	case BOXEN_KEY_CTRL_K:
		/* Kill to end of line. */
		ctx->len = ctx->cursor;
		ctx->buf[ctx->len] = '\0';
		break;

	default:
		/* Printable character?  (key.key == BOXEN_KEY_NONE indicates a
		 * printable Unicode codepoint in key.ch.) */
		if (ev->key.key == BOXEN_KEY_NONE && ev->key.ch >= 0x20 && ev->key.ch < 0x7F) {
			insert_char(ctx, (char)ev->key.ch);
		}
		break;
	}
}

/* -------------------------------------------------------------------------
 * Mini event loop
 *
 * Pattern mirrors the main REPL loop at boxen_repl.c:1887-1939: save
 * threadglobals -> unlock GIL -> poll -> lock GIL -> restore globals,
 * then drain the capture pipe so the scrollback ring stays current
 * across the wait.
 * ---------------------------------------------------------------------- */

static void run_modal(ui_ctx_t *ctx, const boxen_ui_host_t *host) {
	/* Initial paint before entering the wait loop. */
	boxen_window_invalidate(ctx->win);
	if (host && host->drain_capture_pipe) {
		host->drain_capture_pipe(host->drain_ctx);
	}
	boxen_present();

	while (!ctx->done) {
		boxen_event_t ev;
		memset(&ev, 0, sizeof(ev));

		/* 2026-06-23 JES #691 Phase C.0.7g: pump pending TCP callbacks
		 * before yielding so socket activity arriving during a long
		 * modal does not accumulate in the queue.  Mirrors
		 * headless_backgroundtask:693. */
		tcp_process_callbacks();

		hdlthreadglobals saved = hthreadglobals;
		headless_save_threadglobals(saved);
		pthread_mutex_unlock(&frontier_gil);
		/* Broadcast that the GIL is available so sibling threads
		 * waiting on gil_available cond don't stall for the duration
		 * of the modal.  Mirrors headless_backgroundtask:699. */
		pthread_cond_broadcast(&gil_available);
		boxen_result_t rc = boxen_poll_event(&ev, 100 /* ms */);
		pthread_mutex_lock(&frontier_gil);
		headless_restore_threadglobals(saved);

		/* Check if this script's thread was killed while we yielded
		 * the GIL.  Mirrors headless_backgroundtask:728.  Without
		 * this, thread.kill on the dispatched script would never
		 * break the modal out of the wait loop. */
		if ((**saved).flthreadkilled) {
			ctx->cancelled = true;
			ctx->done = true;
			break;
		}

		if (host && host->drain_capture_pipe) {
			host->drain_capture_pipe(host->drain_ctx);
		}

		if (rc == BOXEN_ERR_TIMEOUT) {
			/* No input arrived; loop again.  The main REPL's
			 * timeout path also presents on each tick to flush any
			 * scrollback updates the drain produced. */
			boxen_present();
			continue;
		}
		if (rc != BOXEN_OK) {
			ctx->cancelled = true;
			ctx->done = true;
			break;
		}

		if (ev.type == BOXEN_EV_RESIZE) {
			/* Re-center the modal against the new screen dimensions.
			 * Phase 1 keeps content_w fixed; clamp to the new screen
			 * width if necessary. */
			int sw = ev.resize.w, sh = ev.resize.h;
			if (sw < 20) sw = 20;
			if (sh < 6)  sh = 6;
			boxen_rect_t r = place_modal(sw, sh, ctx->content_w);
			boxen_window_set_rect(ctx->win, r);
			ctx->content_w = boxen_window_content_width(ctx->win);
			boxen_window_invalidate(ctx->win);
			boxen_present();
			continue;
		}

		if (ev.type == BOXEN_EV_KEY) {
			handle_key_event(ctx, &ev);
			boxen_window_invalidate(ctx->win);
			boxen_present();
		}
		/* Other event types (mouse): ignored in Phase 1.  The modal
		 * has focus; the user can still scroll the background with
		 * the wheel via boxen's compositor, but click-through into
		 * other windows is suppressed by the modal flag. */
	}
}

/* -------------------------------------------------------------------------
 * Common entry: build a modal, run the loop, return the buffer (or NULL
 * on cancel).  Returns NULL when the boxen UI host is not active or when
 * window allocation fails -- caller falls back to the legacy path.
 * ---------------------------------------------------------------------- */

static char *run_input(boxen_ui_mode_t mode, const char *prompt,
                       const char *default_val) {
	/* 2026-06-23 JES #691 Phase C.0.7g: snapshot the host pointer once
	 * up front so all subsequent uses see a consistent view, even if a
	 * concurrent boxen_ui_set_active(NULL) lands while we're mid-modal
	 * (today: prevented by GIL discipline + teardown ordering; the
	 * snapshot is belt-and-suspenders). */
	const boxen_ui_host_t *host = load_host();
	if (!host) return NULL;

	int sw = 80, sh = 24;
	boxen_get_screen_size(&sw, &sh);
	if (sw < 20) sw = 20;
	if (sh < 6)  sh = 6;

	/* Make the field wide enough for the prompt + reasonable input. */
	int prompt_w = prompt ? (int)strlen(prompt) : 0;
	int field_w = prompt_w + 8;
	if (field_w < 50) field_w = 50;

	ui_ctx_t ctx;
	memset(&ctx, 0, sizeof(ctx));
	ctx.mode = mode;
	ctx.prompt = prompt;

	if (default_val && default_val[0]) {
		size_t dvlen = strlen(default_val);
		if (dvlen >= BOXEN_UI_BUF_MAX) dvlen = BOXEN_UI_BUF_MAX - 1;
		memcpy(ctx.buf, default_val, dvlen);
		ctx.buf[dvlen] = '\0';
		ctx.len = (int)dvlen;
		ctx.cursor = ctx.len;
	}

	boxen_rect_t rect = place_modal(sw, sh, field_w);
	boxen_window_t *win = boxen_window_open(NULL /* no title bar */,
	                                        rect, &ctx);
	if (!win) return NULL;

	ctx.win = win;
	ctx.content_w = boxen_window_content_width(win);

	boxen_window_set_borders(win, true);
	boxen_window_set_modal(win, true);
	boxen_window_set_movable(win, false);
	boxen_window_set_resizable(win, false);
	boxen_window_set_draw(win, render_modal_cb);
	boxen_window_focus(win);
	boxen_window_raise(win);

	run_modal(&ctx, host);

	boxen_window_set_cursor_visible(win, false);
	boxen_window_close(win);

	/* 2026-06-23 JES #691 Phase C.0.7g: hand focus back to the host's
	 * default (typically the REPL input bar).  boxen does NOT maintain a
	 * focus stack; without this, after our modal closes nothing is
	 * focused and keystrokes go unrouted until the user clicks
	 * something.  Nested modals: the host's restore_focus typically
	 * focuses input_win, which is wrong if an outer modal is still up.
	 * The outer's mini-loop will re-focus its own window on its next
	 * iteration via boxen_window_invalidate + present, but that takes
	 * up to one 100ms tick.  Acceptable for Phase 1.  Phase 2 candidate:
	 * proper modal-stack in boxen core. */
	if (host && host->restore_focus) {
		host->restore_focus(host->restore_focus_ctx);
	}

	boxen_present();

	if (ctx.cancelled) return NULL;

	/* strdup the buffer so the caller owns lifetime. */
	char *out = (char *)malloc((size_t)ctx.len + 1);
	if (!out) return NULL;
	memcpy(out, ctx.buf, (size_t)ctx.len);
	out[ctx.len] = '\0';
	return out;
}

/* -------------------------------------------------------------------------
 * Public primitives
 * ---------------------------------------------------------------------- */

char *boxen_ui_get_string(const char *prompt, const char *default_val) {
	return run_input(BOXEN_UI_MODE_STRING, prompt, default_val);
}

bool boxen_ui_get_int(const char *prompt, long default_val, long *out) {
	if (!out) return false;
	char defbuf[32];
	snprintf(defbuf, sizeof(defbuf), "%ld", default_val);

	char *result = run_input(BOXEN_UI_MODE_INT, prompt, defbuf);
	if (!result) return false;

	/* Empty input -> accept default. */
	if (result[0] == '\0') {
		free(result);
		*out = default_val;
		return true;
	}

	/* Parse; reject anything not a clean integer. */
	char *endp = NULL;
	long v = strtol(result, &endp, 10);
	if (endp == result || (endp && *endp != '\0')) {
		free(result);
		/* Treat parse failure as cancel for Phase 1 -- the script sees a
		 * false return and can re-prompt itself.  Phase 2 candidate:
		 * inline validation with a hint row. */
		return false;
	}
	free(result);
	*out = v;
	return true;
}

char *boxen_ui_get_password(const char *prompt) {
	return run_input(BOXEN_UI_MODE_PASSWORD, prompt, NULL);
}

/* -------------------------------------------------------------------------
 * 2026-06-23 JES #691 Phase C.0.7g Phase 2A: info-and-wait + button-
 * selector modals.  Shape parallels run_input but with different
 * content and key handling, so the modal context and render callback
 * are separate.  Kept inline in this TU rather than abstracted into a
 * shared "generic modal" loop -- the variants diverge enough that a
 * shared abstraction would cost more clarity than it'd save.
 * ---------------------------------------------------------------------- */

typedef struct {
	const char *message;
	int   content_w;
	boxen_window_t *win;
	bool  done;
	bool  cancelled;
	/* Host snapshot stashed for future renderer use.  Today the
	 * renderer doesn't touch the host, but if it ever grows
	 * host-mediated logic (a status line that drains the capture pipe,
	 * a host-driven theme accessor), reading the file-static s_host
	 * raw would be a race risk.  Phase-1-pattern parity. */
	const boxen_ui_host_t *host;
} alert_ctx_t;

#define BOXEN_UI_ALERT_HINT "[Enter] dismiss"

static void render_alert_cb(boxen_window_t *win, void *user_data) {
	(void)win;
	alert_ctx_t *ctx = (alert_ctx_t *)user_data;
	if (ctx == NULL || ctx->win == NULL) return;

	int w = ctx->content_w;

	/* Row 0: message, clipped to width. */
	const char *m = ctx->message ? ctx->message : "";
	int i;
	for (i = 0; i < w; i++) {
		char ch = m[i];
		if (ch == '\0') break;
		boxen_set_cell(ctx->win, i, 0, (uint32_t)(unsigned char)ch,
		               BOXEN_COLOR_DEFAULT, BOXEN_COLOR_DEFAULT,
		               BOXEN_ATTR_NONE);
	}
	for (; i < w; i++) {
		boxen_set_cell(ctx->win, i, 0, ' ',
		               BOXEN_COLOR_DEFAULT, BOXEN_COLOR_DEFAULT,
		               BOXEN_ATTR_NONE);
	}

	/* Row 1: blank spacer. */
	for (int j = 0; j < w; j++) {
		boxen_set_cell(ctx->win, j, 1, ' ',
		               BOXEN_COLOR_DEFAULT, BOXEN_COLOR_DEFAULT,
		               BOXEN_ATTR_NONE);
	}

	/* Row 2: dismiss hint, dim. */
	const char *hint = BOXEN_UI_ALERT_HINT;
	int hint_len = (int)strlen(hint);
	int hint_x = (w - hint_len) / 2;
	if (hint_x < 0) hint_x = 0;
	for (int k = 0; k < w; k++) {
		char ch = (k >= hint_x && (k - hint_x) < hint_len) ? hint[k - hint_x] : ' ';
		boxen_set_cell(ctx->win, k, 2, (uint32_t)(unsigned char)ch,
		               BOXEN_COLOR_DEFAULT, BOXEN_COLOR_DEFAULT,
		               BOXEN_ATTR_DIM);
	}

	boxen_window_set_cursor_visible(ctx->win, false);
}

static boxen_rect_t place_centered_modal(int screen_w, int screen_h,
                                       int min_field_w) {
	int content_w = min_field_w;
	if (content_w < 30) content_w = 30;
	int max_w = screen_w - 4;
	if (max_w < 20) max_w = 20;
	if (content_w > max_w) content_w = max_w;

	int win_w = content_w + 2;
	int win_h = 5;

	int x = (screen_w - win_w) / 2;
	if (x < 0) x = 0;
	int y = screen_h / 3;
	if (y + win_h > screen_h - 1) y = screen_h - win_h - 1;
	if (y < 0) y = 0;

	boxen_rect_t r = { x, y, win_w, win_h };
	return r;
}

bool boxen_ui_alert(const char *message, bool beep) {
	const boxen_ui_host_t *host = load_host();
	if (!host) {
		/* Reachable only on a set_active(NULL) race that GIL discipline
		 * prevents today, but if it ever happens we still need to honor
		 * the documented contract: dialog.alert / dialog.notify always
		 * return true once the user has "dismissed" the prompt.  No
		 * prompt was shown here, but the verb's bool return must not
		 * differ from the legacy "always true" semantics or scripts
		 * branching on a falsy result will misbehave. */
		return true;
	}

	int sw = 80, sh = 24;
	boxen_get_screen_size(&sw, &sh);
	if (sw < 20) sw = 20;
	if (sh < 6)  sh = 6;

	int msg_w = message ? (int)strlen(message) : 0;
	int hint_w = (int)strlen(BOXEN_UI_ALERT_HINT);
	int field_w = msg_w > hint_w ? msg_w : hint_w;
	field_w += 4; /* margin */
	if (field_w < 40) field_w = 40;

	alert_ctx_t ctx;
	memset(&ctx, 0, sizeof(ctx));
	ctx.message = message;
	ctx.host = host;

	boxen_rect_t rect = place_centered_modal(sw, sh, field_w);
	boxen_window_t *win = boxen_window_open(NULL, rect, &ctx);
	if (!win) return false;

	ctx.win = win;
	ctx.content_w = boxen_window_content_width(win);

	boxen_window_set_borders(win, true);
	boxen_window_set_modal(win, true);
	boxen_window_set_movable(win, false);
	boxen_window_set_resizable(win, false);
	boxen_window_set_draw(win, render_alert_cb);
	boxen_window_focus(win);
	boxen_window_raise(win);

	if (beep && host->ring_bell) {
		/* Route through the host so the bell reaches the real terminal
		 * fd rather than getting absorbed by the boxen stdout/stderr
		 * capture pipe (which would render the byte as a visible ^G
		 * glyph in the scrollback rather than ringing the bell). */
		host->ring_bell(host->ring_bell_ctx);
	}

	/* Mini event loop (parallel to run_modal). */
	boxen_window_invalidate(win);
	if (host->drain_capture_pipe) host->drain_capture_pipe(host->drain_ctx);
	boxen_present();

	while (!ctx.done) {
		boxen_event_t ev;
		memset(&ev, 0, sizeof(ev));

		tcp_process_callbacks();

		hdlthreadglobals saved = hthreadglobals;
		headless_save_threadglobals(saved);
		pthread_mutex_unlock(&frontier_gil);
		pthread_cond_broadcast(&gil_available);
		boxen_result_t rc = boxen_poll_event(&ev, 100);
		pthread_mutex_lock(&frontier_gil);
		headless_restore_threadglobals(saved);

		if ((**saved).flthreadkilled) {
			ctx.cancelled = true;
			ctx.done = true;
			break;
		}

		if (host->drain_capture_pipe) host->drain_capture_pipe(host->drain_ctx);

		if (rc == BOXEN_ERR_TIMEOUT) { boxen_present(); continue; }
		if (rc != BOXEN_OK) { ctx.cancelled = true; ctx.done = true; break; }

		if (ev.type == BOXEN_EV_RESIZE) {
			int new_sw = ev.resize.w, new_sh = ev.resize.h;
			if (new_sw < 20) new_sw = 20;
			if (new_sh < 6)  new_sh = 6;
			boxen_rect_t r = place_centered_modal(new_sw, new_sh, ctx.content_w);
			boxen_window_set_rect(win, r);
			ctx.content_w = boxen_window_content_width(win);
			boxen_window_invalidate(win);
			boxen_present();
			continue;
		}

		if (ev.type == BOXEN_EV_KEY) {
			if (ev.key.key == BOXEN_KEY_ENTER) {
				ctx.done = true;
			} else if (ev.key.key == BOXEN_KEY_ESCAPE
			           || ev.key.key == BOXEN_KEY_CTRL_C) {
				ctx.cancelled = true;
				ctx.done = true;
			}
			/* All other keys ignored -- alert is dismiss-only. */
			boxen_window_invalidate(win);
			boxen_present();
		}
	}

	boxen_window_close(win);
	if (host->restore_focus) host->restore_focus(host->restore_focus_ctx);
	boxen_present();

	/* Both Enter and Esc/Ctrl-C return true today (matches the legacy
	 * dialog_alert / dialog_notify behavior of always returning true
	 * once dismissed -- there's no "I disagree with this message"
	 * outcome).  ctx.cancelled remains tracked inside the loop in case
	 * a future caller wants the "user bailed via Esc" distinction;
	 * unused in the current return. */
	return true;
}

/* -------------------------------------------------------------------------
 * Button selector (dialog.twoway / dialog.threeway)
 * ---------------------------------------------------------------------- */

#define BOXEN_UI_BUTTON_MAX 4

typedef struct {
	const char *prompt;
	const char *labels[BOXEN_UI_BUTTON_MAX];
	int   count;
	int   selection;     /* 0..count-1 */
	int   content_w;
	boxen_window_t *win;
	bool  done;
	bool  cancelled;
	const boxen_ui_host_t *host;	/* see alert_ctx_t.host */
} button_ctx_t;

/* Total width of the button row laid out with single-space gutters. */
static int button_row_width(const button_ctx_t *ctx) {
	int total = 0;
	for (int i = 0; i < ctx->count; i++) {
		const char *l = ctx->labels[i] ? ctx->labels[i] : "";
		total += 4 + (int)strlen(l); /* "[ " + label + " ]" */
		if (i < ctx->count - 1) total += 2; /* gutter */
	}
	return total;
}

static void render_button_cb(boxen_window_t *win, void *user_data) {
	(void)win;
	button_ctx_t *ctx = (button_ctx_t *)user_data;
	if (ctx == NULL || ctx->win == NULL) return;

	int w = ctx->content_w;

	/* Row 0: prompt, bold. */
	const char *p = ctx->prompt ? ctx->prompt : "";
	int i;
	for (i = 0; i < w; i++) {
		char ch = p[i];
		if (ch == '\0') break;
		boxen_set_cell(ctx->win, i, 0, (uint32_t)(unsigned char)ch,
		               BOXEN_COLOR_DEFAULT, BOXEN_COLOR_DEFAULT,
		               BOXEN_ATTR_BOLD);
	}
	for (; i < w; i++) {
		boxen_set_cell(ctx->win, i, 0, ' ',
		               BOXEN_COLOR_DEFAULT, BOXEN_COLOR_DEFAULT,
		               BOXEN_ATTR_NONE);
	}

	/* Row 1: blank spacer. */
	for (int j = 0; j < w; j++) {
		boxen_set_cell(ctx->win, j, 1, ' ',
		               BOXEN_COLOR_DEFAULT, BOXEN_COLOR_DEFAULT,
		               BOXEN_ATTR_NONE);
	}

	/* Row 2: button row, centered.  Selected button drawn in reverse
	 * video.  Each button rendered as "[ Label ]" with the label's
	 * first letter (the hotkey) bold + underlined when non-selected
	 * and just underlined when selected (reverse video makes bold
	 * redundant). */
	int row_w = button_row_width(ctx);
	int x = (w - row_w) / 2;
	if (x < 0) x = 0;
	/* Clear the row first. */
	for (int j = 0; j < w; j++) {
		boxen_set_cell(ctx->win, j, 2, ' ',
		               BOXEN_COLOR_DEFAULT, BOXEN_COLOR_DEFAULT,
		               BOXEN_ATTR_NONE);
	}
	for (int b = 0; b < ctx->count; b++) {
		const char *l = ctx->labels[b] ? ctx->labels[b] : "";
		int ll = (int)strlen(l);
		bool sel = (b == ctx->selection);
		uint16_t base = sel ? BOXEN_ATTR_REVERSE : BOXEN_ATTR_NONE;

		if (x < w) boxen_set_cell(ctx->win, x++, 2, '[',
		                          BOXEN_COLOR_DEFAULT,
		                          BOXEN_COLOR_DEFAULT, base);
		if (x < w) boxen_set_cell(ctx->win, x++, 2, ' ',
		                          BOXEN_COLOR_DEFAULT,
		                          BOXEN_COLOR_DEFAULT, base);
		for (int k = 0; k < ll && x < w; k++, x++) {
			uint16_t a = base | ((k == 0 && !sel) ?
			                     (BOXEN_ATTR_BOLD | BOXEN_ATTR_UNDERLINE)
			                     : (k == 0 ? BOXEN_ATTR_UNDERLINE : 0));
			boxen_set_cell(ctx->win, x, 2,
			               (uint32_t)(unsigned char)l[k],
			               BOXEN_COLOR_DEFAULT, BOXEN_COLOR_DEFAULT, a);
		}
		if (x < w) boxen_set_cell(ctx->win, x++, 2, ' ',
		                          BOXEN_COLOR_DEFAULT,
		                          BOXEN_COLOR_DEFAULT, base);
		if (x < w) boxen_set_cell(ctx->win, x++, 2, ']',
		                          BOXEN_COLOR_DEFAULT,
		                          BOXEN_COLOR_DEFAULT, base);
		if (b < ctx->count - 1) {
			/* Gutter. */
			if (x < w) boxen_set_cell(ctx->win, x++, 2, ' ',
			                          BOXEN_COLOR_DEFAULT,
			                          BOXEN_COLOR_DEFAULT, BOXEN_ATTR_NONE);
			if (x < w) boxen_set_cell(ctx->win, x++, 2, ' ',
			                          BOXEN_COLOR_DEFAULT,
			                          BOXEN_COLOR_DEFAULT, BOXEN_ATTR_NONE);
		}
	}

	boxen_window_set_cursor_visible(ctx->win, false);
}

static int hotkey_match(const button_ctx_t *ctx, uint32_t ch) {
	if (ch == 0) return -1;
	/* Case-insensitive first-letter match against each label. */
	char up = (char)ch;
	if (up >= 'a' && up <= 'z') up = (char)(up - 'a' + 'A');
	for (int i = 0; i < ctx->count; i++) {
		const char *l = ctx->labels[i];
		if (!l || !l[0]) continue;
		char first = l[0];
		if (first >= 'a' && first <= 'z') first = (char)(first - 'a' + 'A');
		if (first == up) return i;
	}
	return -1;
}

int boxen_ui_button_select(const char *prompt,
                           const char *const *buttons, int count) {
	if (count < 2 || count > BOXEN_UI_BUTTON_MAX) return 0;
	if (!buttons) return 0;

	const boxen_ui_host_t *host = load_host();
	if (!host) return 0;

	int sw = 80, sh = 24;
	boxen_get_screen_size(&sw, &sh);
	if (sw < 20) sw = 20;
	if (sh < 6)  sh = 6;

	button_ctx_t ctx;
	memset(&ctx, 0, sizeof(ctx));
	ctx.prompt = prompt;
	ctx.count = count;
	for (int i = 0; i < count; i++) ctx.labels[i] = buttons[i];
	ctx.selection = 0;
	ctx.host = host;

	/* Field width: max(prompt, button row) + margin. */
	int prompt_w = prompt ? (int)strlen(prompt) : 0;
	int row_w = button_row_width(&ctx);
	int field_w = prompt_w > row_w ? prompt_w : row_w;
	field_w += 4;
	if (field_w < 40) field_w = 40;

	boxen_rect_t rect = place_centered_modal(sw, sh, field_w);
	boxen_window_t *win = boxen_window_open(NULL, rect, &ctx);
	if (!win) return 0;

	ctx.win = win;
	ctx.content_w = boxen_window_content_width(win);

	boxen_window_set_borders(win, true);
	boxen_window_set_modal(win, true);
	boxen_window_set_movable(win, false);
	boxen_window_set_resizable(win, false);
	boxen_window_set_draw(win, render_button_cb);
	boxen_window_focus(win);
	boxen_window_raise(win);

	boxen_window_invalidate(win);
	if (host->drain_capture_pipe) host->drain_capture_pipe(host->drain_ctx);
	boxen_present();

	while (!ctx.done) {
		boxen_event_t ev;
		memset(&ev, 0, sizeof(ev));

		tcp_process_callbacks();

		hdlthreadglobals saved = hthreadglobals;
		headless_save_threadglobals(saved);
		pthread_mutex_unlock(&frontier_gil);
		pthread_cond_broadcast(&gil_available);
		boxen_result_t rc = boxen_poll_event(&ev, 100);
		pthread_mutex_lock(&frontier_gil);
		headless_restore_threadglobals(saved);

		if ((**saved).flthreadkilled) {
			ctx.cancelled = true;
			ctx.done = true;
			break;
		}

		if (host->drain_capture_pipe) host->drain_capture_pipe(host->drain_ctx);

		if (rc == BOXEN_ERR_TIMEOUT) { boxen_present(); continue; }
		if (rc != BOXEN_OK) { ctx.cancelled = true; ctx.done = true; break; }

		if (ev.type == BOXEN_EV_RESIZE) {
			int new_sw = ev.resize.w, new_sh = ev.resize.h;
			if (new_sw < 20) new_sw = 20;
			if (new_sh < 6)  new_sh = 6;
			boxen_rect_t r = place_centered_modal(new_sw, new_sh, ctx.content_w);
			boxen_window_set_rect(win, r);
			ctx.content_w = boxen_window_content_width(win);
			boxen_window_invalidate(win);
			boxen_present();
			continue;
		}

		if (ev.type == BOXEN_EV_KEY) {
			switch (ev.key.key) {
			case BOXEN_KEY_ENTER:
				ctx.done = true;
				break;
			case BOXEN_KEY_ESCAPE:
			case BOXEN_KEY_CTRL_C:
				ctx.cancelled = true;
				ctx.done = true;
				break;
			case BOXEN_KEY_LEFT:
				if (ctx.selection > 0) ctx.selection--;
				break;
			case BOXEN_KEY_RIGHT:
				if (ctx.selection < ctx.count - 1) ctx.selection++;
				break;
			case BOXEN_KEY_HOME:
				ctx.selection = 0;
				break;
			case BOXEN_KEY_END:
				ctx.selection = ctx.count - 1;
				break;
			default: {
				/* Reaching `default` means ev.key.key == BOXEN_KEY_NONE
				 * (every non-NONE key is named in a case above), so
				 * ev.key.ch holds a Unicode codepoint.  Hotkey match:
				 * printable ASCII first-letter == label's first
				 * letter (case-insensitive) selects + activates that
				 * button immediately. */
				if (ev.key.ch >= 0x20 && ev.key.ch < 0x7F) {
					int idx = hotkey_match(&ctx, ev.key.ch);
					if (idx >= 0) {
						ctx.selection = idx;
						ctx.done = true;
					}
				}
				break;
			}
			}
			boxen_window_invalidate(win);
			boxen_present();
		}
	}

	int result = ctx.cancelled ? 0 : (ctx.selection + 1);
	boxen_window_close(win);
	if (host->restore_focus) host->restore_focus(host->restore_focus_ctx);
	boxen_present();
	return result;
}
