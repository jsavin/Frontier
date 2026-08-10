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

#include <dirent.h>
#include <errno.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

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
 * Shared modal event loop
 *
 * Every boxen_ui_* entry point that opens a modal window runs the same
 * GIL-yield / poll / drain / present skeleton.  Centralizing it here
 * means any future modal (or any GIL-discipline fix) lands in one place
 * instead of N+1 parallel copies.  Each caller passes its own ctx,
 * done-flag pointer, and event-handling callbacks.
 *
 * Skeleton each iteration (matches headless_backgroundtask:693-728):
 *   1. tcp_process_callbacks()                  -- pump pending TCP
 *   2. save threadglobals + unlock GIL + broadcast gil_available
 *   3. boxen_poll_event(&ev, 100ms)
 *   4. lock GIL + restore threadglobals
 *   5. check flthreadkilled: set *done = true; break
 *   6. host->drain_capture_pipe (keep scrollback current)
 *   7. dispatch event: timeout -> present + continue;
 *      RESIZE -> on_resize cb; KEY -> on_key cb; other -> ignore
 *
 * 2026-06-25 JES #691 Phase C.0.7g Phase 2B gate-fix (#100):
 * Replaces the four parallel mini-loops that grew during Phase 1, 2A,
 * and 2B.  Reviewers flagged each new copy; this is the consolidation.
 * ---------------------------------------------------------------------- */

typedef void (*boxen_ui_resize_fn)(void *ctx, int sw, int sh);
typedef void (*boxen_ui_key_fn)(void *ctx, const boxen_event_t *ev);

static void run_modal_loop(boxen_window_t *win,
                           const boxen_ui_host_t *host,
                           void *ctx,
                           bool *done,
                           bool *cancelled,	/* optional; flthreadkilled/poll-err set if non-NULL */
                           boxen_ui_resize_fn on_resize,
                           boxen_ui_key_fn on_key) {
	/* Initial paint before entering the wait loop. */
	boxen_window_invalidate(win);
	if (host && host->drain_capture_pipe) {
		host->drain_capture_pipe(host->drain_ctx);
	}
	if (host && host->drain_debug) {
		host->drain_debug(host->drain_debug_ctx);
	}
	boxen_present();

	while (!*done) {
		boxen_event_t ev;
		memset(&ev, 0, sizeof(ev));

		tcp_process_callbacks();

		hdlthreadglobals saved = hthreadglobals;
		headless_save_threadglobals(saved);
		pthread_mutex_unlock(&frontier_gil);
		pthread_cond_broadcast(&gil_available);
		boxen_result_t rc = boxen_poll_event(&ev, 100 /* ms */);
		pthread_mutex_lock(&frontier_gil);
		headless_restore_threadglobals(saved);

		/* thread.kill on the dispatched script must break the modal.
		 * Mirrors headless_backgroundtask:728. */
		if ((**saved).flthreadkilled) {
			if (cancelled) *cancelled = true;
			*done = true;
			break;
		}

		if (host && host->drain_capture_pipe) {
			host->drain_capture_pipe(host->drain_ctx);
		}

		/* 2026-08-10 JES unit 2.4 hardening: drain debug notifications
		 * queued by suspended runtime threads while this modal is open,
		 * so the suspension renders behind the dialog instead of parking
		 * invisibly until the modal closes. */
		if (host && host->drain_debug) {
			host->drain_debug(host->drain_debug_ctx);
		}

		if (rc == BOXEN_ERR_TIMEOUT) { boxen_present(); continue; }
		if (rc != BOXEN_OK) {
			if (cancelled) *cancelled = true;
			*done = true;
			break;
		}

		if (ev.type == BOXEN_EV_RESIZE) {
			if (on_resize) {
				on_resize(ctx, ev.resize.w, ev.resize.h);
				boxen_window_invalidate(win);
				boxen_present();
			}
			continue;
		}

		if (ev.type == BOXEN_EV_KEY) {
			if (on_key) {
				on_key(ctx, &ev);
				boxen_window_invalidate(win);
				boxen_present();
			}
		}
		/* Other event types (mouse): ignored.  Modal flag prevents
		 * click-through; the user can still scroll the background with
		 * the wheel via boxen's compositor. */
	}
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

/* String-input modal resize / key adapters for the shared loop. */
static void run_modal_resize_cb(void *vctx, int sw, int sh) {
	ui_ctx_t *ctx = (ui_ctx_t *)vctx;
	if (sw < 20) sw = 20;
	if (sh < 6)  sh = 6;
	boxen_rect_t r = place_modal(sw, sh, ctx->content_w);
	boxen_window_set_rect(ctx->win, r);
	ctx->content_w = boxen_window_content_width(ctx->win);
}

static void run_modal_key_cb(void *vctx, const boxen_event_t *ev) {
	handle_key_event((ui_ctx_t *)vctx, ev);
}

static void run_modal(ui_ctx_t *ctx, const boxen_ui_host_t *host) {
	run_modal_loop(ctx->win, host, ctx, &ctx->done, &ctx->cancelled,
	               run_modal_resize_cb, run_modal_key_cb);
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

/* Alert modal resize / key adapters for the shared loop. */
static void alert_resize_cb(void *vctx, int sw, int sh) {
	alert_ctx_t *ctx = (alert_ctx_t *)vctx;
	if (sw < 20) sw = 20;
	if (sh < 6)  sh = 6;
	boxen_rect_t r = place_centered_modal(sw, sh, ctx->content_w);
	boxen_window_set_rect(ctx->win, r);
	ctx->content_w = boxen_window_content_width(ctx->win);
}

static void alert_key_cb(void *vctx, const boxen_event_t *ev) {
	alert_ctx_t *ctx = (alert_ctx_t *)vctx;
	if (ev->key.key == BOXEN_KEY_ENTER) {
		ctx->done = true;
	} else if (ev->key.key == BOXEN_KEY_ESCAPE
	           || ev->key.key == BOXEN_KEY_CTRL_C) {
		ctx->cancelled = true;
		ctx->done = true;
	}
	/* All other keys ignored -- alert is dismiss-only. */
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

	run_modal_loop(win, host, &ctx, &ctx.done, &ctx.cancelled,
	               alert_resize_cb, alert_key_cb);

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

/* Button-select modal resize / key adapters for the shared loop. */
static void button_resize_cb(void *vctx, int sw, int sh) {
	button_ctx_t *ctx = (button_ctx_t *)vctx;
	if (sw < 20) sw = 20;
	if (sh < 6)  sh = 6;
	boxen_rect_t r = place_centered_modal(sw, sh, ctx->content_w);
	boxen_window_set_rect(ctx->win, r);
	ctx->content_w = boxen_window_content_width(ctx->win);
}

static void button_key_cb(void *vctx, const boxen_event_t *ev) {
	button_ctx_t *ctx = (button_ctx_t *)vctx;
	switch (ev->key.key) {
	case BOXEN_KEY_ENTER:
		ctx->done = true;
		break;
	case BOXEN_KEY_ESCAPE:
	case BOXEN_KEY_CTRL_C:
		ctx->cancelled = true;
		ctx->done = true;
		break;
	case BOXEN_KEY_LEFT:
		if (ctx->selection > 0) ctx->selection--;
		break;
	case BOXEN_KEY_RIGHT:
		if (ctx->selection < ctx->count - 1) ctx->selection++;
		break;
	case BOXEN_KEY_HOME:
		ctx->selection = 0;
		break;
	case BOXEN_KEY_END:
		ctx->selection = ctx->count - 1;
		break;
	default:
		/* Reaching `default` means ev->key.key == BOXEN_KEY_NONE
		 * (every non-NONE key is named in a case above), so
		 * ev->key.ch holds a Unicode codepoint.  Hotkey match:
		 * printable ASCII first-letter == label's first letter
		 * (case-insensitive) selects + activates that button. */
		if (ev->key.ch >= 0x20 && ev->key.ch < 0x7F) {
			int idx = hotkey_match(ctx, ev->key.ch);
			if (idx >= 0) {
				ctx->selection = idx;
				ctx->done = true;
			}
		}
		break;
	}
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

	run_modal_loop(win, host, &ctx, &ctx.done, &ctx.cancelled,
	               button_resize_cb, button_key_cb);

	int result = ctx.cancelled ? 0 : (ctx.selection + 1);
	boxen_window_close(win);
	if (host->restore_focus) host->restore_focus(host->restore_focus_ctx);
	boxen_present();
	return result;
}

/* -------------------------------------------------------------------------
 * 2026-06-24 JES #691 Phase C.0.7g Phase 2B: file picker
 * (file.getFileDialog / putFileDialog / getFolderDialog / getDiskDialog).
 *
 * Single-pane list with a breadcrumb header per JES decision
 * 2026-06-24.  PUT_FILE adds a filename input row at the bottom; all
 * other modes are list-only.  Type filter (GET_FILE) dims non-matching
 * files but lets the user select them anyway -- "display-only filter"
 * per JES decision.
 *
 * Layout (in window-content coords):
 *
 *   row 0          : prompt text (bold)
 *   row 1          : breadcrumb of current path
 *   row 2          : separator (horizontal line)
 *   row 3..N-3     : visible slice of entries (scrolled, cursor highlighted)
 *   row N-2        : separator (only for PUT_FILE)
 *   row N-1        : filename input row (only for PUT_FILE)
 *   row N          : hint row (Enter/Tab/Esc keys)
 *
 * Where N = content_h - 1.  GET_FILE / GET_FOLDER / GET_DISK skip the
 * separator + filename row.
 *
 * GIL discipline: same shape as run_modal/run_input/run_alert/
 * run_button_select -- see those for the inline rationale.
 * ---------------------------------------------------------------------- */

#define PICK_MAX_ENTRIES   2048
#define PICK_PATH_MAX      1024
#define PICK_NAME_MAX      256
#define PICK_FILENAME_MAX  256

typedef struct {
	char name[PICK_NAME_MAX];
	bool is_dir;
	bool matches_filter;	/* GET_FILE only; true otherwise */
} pick_entry_t;

typedef struct {
	boxen_ui_pick_mode_t mode;
	const char *prompt;
	const char *type_filter;	/* GET_FILE only; NULL otherwise */

	char cur_dir[PICK_PATH_MAX];	/* always absolute, no trailing / except root */

	pick_entry_t *entries;
	int entry_count;
	int entry_cap;

	int cursor;		/* index into entries[] */
	int scroll_top;	/* first visible entry index */

	/* PUT_FILE only.  filename_focus == true: keyboard goes to the
	 * filename input row instead of the list. */
	bool filename_focus;
	char filename[PICK_FILENAME_MAX];
	int  filename_len;
	int  filename_cursor;

	/* Output. */
	char *out_path;
	size_t out_cap;

	boxen_window_t *win;
	int content_w;
	int content_h;
	bool done;
	bool committed;	/* true: out_path populated; false: cancel */
	/* Set when the most recent pick_load_directory() couldn't open
	 * cur_dir (permission, missing, etc.).  The renderer surfaces
	 * "(directory unreadable)" so the user understands an empty list. */
	bool load_failed;
	const boxen_ui_host_t *host;
} pick_ctx_t;

/* -- directory enumeration & sort -- */

static int pick_entry_cmp(const void *a, const void *b) {
	const pick_entry_t *ea = (const pick_entry_t *)a;
	const pick_entry_t *eb = (const pick_entry_t *)b;
	/* Directories first, then files, both alpha within their group. */
	if (ea->is_dir != eb->is_dir) return ea->is_dir ? -1 : 1;
	return strcasecmp(ea->name, eb->name);
}

static bool name_matches_filter(const char *name, const char *filter) {
	if (filter == NULL || filter[0] == '\0') return true;
	size_t nlen = strlen(name);
	size_t flen = strlen(filter);
	if (nlen <= flen + 1) return false;	/* need at least "x.ext" */
	if (name[nlen - flen - 1] != '.') return false;
	return strcasecmp(name + nlen - flen, filter) == 0;
}

/* Populate ctx->entries from ctx->cur_dir.  Returns true on success.
 * On failure (e.g. dir unreadable) leaves entries empty; caller can
 * navigate back up. */
/* Returns "" if d ends in '/' (already separator-terminated), else "/".
 * Safe for empty strings (returns "/" without indexing).  Used to join
 * a directory path to a name without producing a double-slash. */
static const char *dir_sep(const char *d) {
	if (!d || d[0] == '\0') return "/";
	size_t n = strlen(d);
	return (d[n - 1] == '/') ? "" : "/";
}

static bool pick_load_directory(pick_ctx_t *ctx) {
	ctx->entry_count = 0;
	ctx->cursor = 0;
	ctx->scroll_top = 0;
	ctx->load_failed = false;

	DIR *dp = opendir(ctx->cur_dir);
	if (!dp) {
		ctx->load_failed = true;
		return false;
	}

	bool at_root = (strcmp(ctx->cur_dir, "/") == 0);

	struct dirent *de;
	while ((de = readdir(dp)) != NULL && ctx->entry_count < ctx->entry_cap) {
		/* Skip "." always.  Hide ".." at filesystem root (Enter on
		 * ".." there just realpath()'s back to "/" which looks like a
		 * cosmetic flicker).  Hide hidden dotfiles by default for
		 * cleaner UX; a future toggle could expose them. */
		if (de->d_name[0] == '.' && strcmp(de->d_name, "..") != 0) continue;
		if (at_root && strcmp(de->d_name, "..") == 0) continue;

		size_t nlen = strlen(de->d_name);
		if (nlen >= PICK_NAME_MAX) continue;

		/* Stat to determine is_dir reliably across filesystems (some
		 * don't fill d_type).  Use the full path to handle symlinks. */
		char full[PICK_PATH_MAX];
		int n = snprintf(full, sizeof(full), "%s%s%s",
		                 ctx->cur_dir,
		                 dir_sep(ctx->cur_dir),
		                 de->d_name);
		if (n < 0 || (size_t)n >= sizeof(full)) continue;

		struct stat st;
		bool is_dir = false;
		if (stat(full, &st) == 0) {
			is_dir = S_ISDIR(st.st_mode);
		} else if (de->d_type == DT_DIR) {
			is_dir = true;
		}

		pick_entry_t *e = &ctx->entries[ctx->entry_count++];
		memcpy(e->name, de->d_name, nlen + 1);
		e->is_dir = is_dir;
		/* Filter: GET_FILE applies to files only; dirs always match. */
		if (ctx->mode == BOXEN_UI_PICK_GET_FILE && !is_dir) {
			e->matches_filter = name_matches_filter(de->d_name, ctx->type_filter);
		} else {
			e->matches_filter = true;
		}
	}
	closedir(dp);

	qsort(ctx->entries, (size_t)ctx->entry_count, sizeof(pick_entry_t),
	      pick_entry_cmp);
	return true;
}

/* Navigate into the directory named `child` (relative) or absolute
 * path.  Updates cur_dir and reloads entries. */
static void pick_navigate(pick_ctx_t *ctx, const char *child) {
	if (!child || child[0] == '\0') return;

	char joined[PICK_PATH_MAX];
	if (child[0] == '/') {
		/* Absolute. */
		if (snprintf(joined, sizeof(joined), "%s", child) >= (int)sizeof(joined))
			return;
	} else if (strcmp(child, "..") == 0) {
		/* Parent. */
		if (snprintf(joined, sizeof(joined), "%s", ctx->cur_dir) >= (int)sizeof(joined))
			return;
		char *slash = strrchr(joined, '/');
		if (slash == NULL) return;
		if (slash == joined) joined[1] = '\0';	/* "/" */
		else *slash = '\0';
	} else {
		const char *sep = dir_sep(ctx->cur_dir);
		if (snprintf(joined, sizeof(joined), "%s%s%s",
		             ctx->cur_dir, sep, child) >= (int)sizeof(joined))
			return;
	}

	/* Canonicalize: resolve symlinks and "." / ".." components.
	 * realpath returns NULL on permission denied, missing directory,
	 * ELOOP (symlink loop), etc.  Beep so the user notices the keypress
	 * was seen but the navigation didn't take. */
	char resolved[PICK_PATH_MAX];
	if (realpath(joined, resolved) == NULL) {
		if (ctx->host && ctx->host->ring_bell) {
			ctx->host->ring_bell(ctx->host->ring_bell_ctx);
		}
		return;
	}

	if (strlen(resolved) >= sizeof(ctx->cur_dir)) return;
	strcpy(ctx->cur_dir, resolved);
	pick_load_directory(ctx);
}

/* -- rendering -- */

static void pick_render_cb(boxen_window_t *win, void *user_data) {
	(void)win;
	pick_ctx_t *ctx = (pick_ctx_t *)user_data;
	if (!ctx || !ctx->win) return;

	int w = ctx->content_w;
	int h = ctx->content_h;
	if (w < 10 || h < 6) return;

	/* Clear the entire content area first so stale glyphs don't leak. */
	for (int y = 0; y < h; y++) {
		for (int x = 0; x < w; x++) {
			boxen_set_cell(ctx->win, x, y, ' ',
			               BOXEN_COLOR_DEFAULT, BOXEN_COLOR_DEFAULT,
			               BOXEN_ATTR_NONE);
		}
	}

	/* Row 0: prompt (bold). */
	const char *p = ctx->prompt ? ctx->prompt : "";
	for (int i = 0; i < w; i++) {
		if (p[i] == '\0') break;
		boxen_set_cell(ctx->win, i, 0, (uint32_t)(unsigned char)p[i],
		               BOXEN_COLOR_DEFAULT, BOXEN_COLOR_DEFAULT,
		               BOXEN_ATTR_BOLD);
	}

	/* Row 1: breadcrumb (dim).  Show the full cur_dir; if too wide,
	 * truncate the leading part with "...". */
	{
		const char *bc = ctx->cur_dir;
		size_t bclen = strlen(bc);
		const char *show = bc;
		char truncated[PICK_PATH_MAX + 4];
		/* w > 3 is guaranteed by the w < 10 early-return above, but
		 * spell out the dependency: w - 3 must not underflow size_t. */
		if (w > 3 && (int)bclen > w) {
			size_t keep = (size_t)w - 3;
			snprintf(truncated, sizeof(truncated), "...%s",
			         bc + (bclen - keep));
			show = truncated;
		}
		for (int i = 0; i < w; i++) {
			if (show[i] == '\0') break;
			boxen_set_cell(ctx->win, i, 1, (uint32_t)(unsigned char)show[i],
			               BOXEN_COLOR_DEFAULT, BOXEN_COLOR_DEFAULT,
			               BOXEN_ATTR_DIM);
		}
	}

	/* Row 2: separator. */
	for (int i = 0; i < w; i++) {
		boxen_set_cell(ctx->win, i, 2, 0x2500 /* light horizontal */,
		               BOXEN_COLOR_DEFAULT, BOXEN_COLOR_DEFAULT,
		               BOXEN_ATTR_DIM);
	}

	/* Compute the list region. */
	int list_top = 3;
	int hint_row = h - 1;
	int filename_row = -1;
	int sep2_row = -1;
	if (ctx->mode == BOXEN_UI_PICK_PUT_FILE) {
		filename_row = hint_row - 1;
		sep2_row = filename_row - 1;
	}
	int list_bottom = (sep2_row >= 0) ? sep2_row - 1 : hint_row - 1;
	int list_h = list_bottom - list_top + 1;
	if (list_h < 1) list_h = 1;

	/* Adjust scroll_top so cursor is visible. */
	if (ctx->cursor < ctx->scroll_top) ctx->scroll_top = ctx->cursor;
	if (ctx->cursor >= ctx->scroll_top + list_h) {
		ctx->scroll_top = ctx->cursor - list_h + 1;
	}
	if (ctx->scroll_top < 0) ctx->scroll_top = 0;

	/* Render visible entries.  ".." (when present in the filesystem's
	 * readdir output) appears as an ordinary entry that the user can
	 * Enter to navigate up; Backspace from the list does the same.
	 * The hint row advertises both. */
	for (int row = 0; row < list_h; row++) {
		int idx = ctx->scroll_top + row;
		int y = list_top + row;
		if (idx >= ctx->entry_count) break;

		const pick_entry_t *e = &ctx->entries[idx];
		bool selected = (idx == ctx->cursor) && !ctx->filename_focus;
		uint16_t base = selected ? BOXEN_ATTR_REVERSE : BOXEN_ATTR_NONE;
		/* Dim non-matching files (GET_FILE display-only filter). */
		if (!e->matches_filter && !selected) base |= BOXEN_ATTR_DIM;
		uint16_t fg = BOXEN_COLOR_DEFAULT;

		/* Render: "  name" with directories appended with '/'. */
		int x = 0;
		/* Two-cell indent so selection highlight has a clear edge. */
		if (x < w) boxen_set_cell(ctx->win, x++, y, ' ',
		                          fg, BOXEN_COLOR_DEFAULT, base);
		if (x < w) boxen_set_cell(ctx->win, x++, y, ' ',
		                          fg, BOXEN_COLOR_DEFAULT, base);
		for (int k = 0; e->name[k] != '\0' && x < w; k++, x++) {
			boxen_set_cell(ctx->win, x, y,
			               (uint32_t)(unsigned char)e->name[k],
			               fg, BOXEN_COLOR_DEFAULT, base);
		}
		if (e->is_dir && x < w) {
			boxen_set_cell(ctx->win, x++, y, '/',
			               fg, BOXEN_COLOR_DEFAULT, base);
		}
		/* Pad selection highlight to full width. */
		while (selected && x < w) {
			boxen_set_cell(ctx->win, x++, y, ' ',
			               fg, BOXEN_COLOR_DEFAULT, base);
		}
	}

	/* Empty directory hint: explain why the list is blank.  Distinguishes
	 * "really empty" from "unreadable / permission denied" -- the latter
	 * is the more confusing case for the user since the picker offers no
	 * other feedback for an enumeration failure. */
	if (ctx->entry_count == 0 && list_h >= 1) {
		const char *msg = ctx->load_failed
			? "(directory unreadable)"
			: "(empty directory)";
		int mlen = (int)strlen(msg);
		int mx = (w - mlen) / 2;
		if (mx < 0) mx = 0;
		for (int i = 0; i < w; i++) {
			char ch = (i >= mx && (i - mx) < mlen) ? msg[i - mx] : ' ';
			boxen_set_cell(ctx->win, i, list_top,
			               (uint32_t)(unsigned char)ch,
			               BOXEN_COLOR_DEFAULT, BOXEN_COLOR_DEFAULT,
			               BOXEN_ATTR_DIM);
		}
	}

	/* PUT_FILE: separator + filename input row. */
	if (ctx->mode == BOXEN_UI_PICK_PUT_FILE) {
		for (int i = 0; i < w; i++) {
			boxen_set_cell(ctx->win, i, sep2_row, 0x2500,
			               BOXEN_COLOR_DEFAULT, BOXEN_COLOR_DEFAULT,
			               BOXEN_ATTR_DIM);
		}
		/* "> filename" with cursor block.  Reverse highlight on the
		 * whole row when filename has focus. */
		uint16_t fbase = ctx->filename_focus ? BOXEN_ATTR_REVERSE : BOXEN_ATTR_NONE;
		const char *prefix = "> ";
		int x = 0;
		for (int k = 0; prefix[k] != '\0' && x < w; k++, x++) {
			boxen_set_cell(ctx->win, x, filename_row,
			               (uint32_t)(unsigned char)prefix[k],
			               BOXEN_COLOR_DEFAULT, BOXEN_COLOR_DEFAULT,
			               fbase | BOXEN_ATTR_DIM);
		}
		int field_x0 = x;
		int field_w = w - field_x0;
		if (field_w < 1) field_w = 1;
		int scroll = 0;
		if (ctx->filename_cursor >= field_w) {
			scroll = ctx->filename_cursor - field_w + 1;
		}
		for (int i = 0; i < field_w; i++) {
			int bi = scroll + i;
			char ch = (bi < ctx->filename_len) ? ctx->filename[bi] : ' ';
			boxen_set_cell(ctx->win, field_x0 + i, filename_row,
			               (uint32_t)(unsigned char)ch,
			               BOXEN_COLOR_DEFAULT, BOXEN_COLOR_DEFAULT,
			               fbase);
		}
		if (ctx->filename_focus) {
			int cc = field_x0 + (ctx->filename_cursor - scroll);
			if (cc < field_x0) cc = field_x0;
			if (cc >= w) cc = w - 1;
			boxen_window_set_cursor(ctx->win, cc, filename_row);
			boxen_window_set_cursor_visible(ctx->win, true);
		} else {
			boxen_window_set_cursor_visible(ctx->win, false);
		}
	} else {
		boxen_window_set_cursor_visible(ctx->win, false);
	}

	/* Hint row. */
	const char *hint;
	if (ctx->mode == BOXEN_UI_PICK_PUT_FILE) {
		hint = ctx->filename_focus
		       ? "Enter:save  Tab:list  Esc:cancel"
		       : "Enter:open  Bksp:up  Tab:filename  Esc:cancel";
	} else if (ctx->mode == BOXEN_UI_PICK_GET_FOLDER) {
		hint = "Enter:open  Space:select  Bksp:up  Esc:cancel";
	} else if (ctx->mode == BOXEN_UI_PICK_GET_DISK) {
		hint = "Enter:select  Bksp:up  Esc:cancel";
	} else {
		hint = "Enter:open/select  Bksp:up  Esc:cancel";
	}
	for (int i = 0; i < w; i++) {
		char ch = (hint[i] == '\0') ? ' ' : hint[i];
		boxen_set_cell(ctx->win, i, hint_row, (uint32_t)(unsigned char)ch,
		               BOXEN_COLOR_DEFAULT, BOXEN_COLOR_DEFAULT,
		               BOXEN_ATTR_DIM);
		if (hint[i] == '\0') {
			/* Continue clearing rest of row. */
			for (int j = i + 1; j < w; j++) {
				boxen_set_cell(ctx->win, j, hint_row, ' ',
				               BOXEN_COLOR_DEFAULT, BOXEN_COLOR_DEFAULT,
				               BOXEN_ATTR_DIM);
			}
			break;
		}
	}
}

/* -- commit logic -- */

/* PUT_FILE commit outcome.  Returned by pick_try_commit_put_file so the
 * caller knows whether to set committed/done flags or just keep the
 * picker open (e.g. "Choose different name" returns to filename row). */
typedef enum {
	PICK_PUT_COMMITTED,	/* out_path populated; close + return true */
	PICK_PUT_KEEP_OPEN,	/* user rejected overwrite; keep picker up */
	PICK_PUT_CANCEL_ALL,	/* user cancelled the whole picker */
	PICK_PUT_NO_FILENAME,	/* empty filename; no-op (no beep) */
} pick_put_result_t;

/* Build the full output path for the current selection.  Returns true
 * on success (out_path populated), false otherwise. */
static bool pick_build_output(pick_ctx_t *ctx) {
	if (ctx->mode == BOXEN_UI_PICK_PUT_FILE) {
		if (ctx->filename_len == 0) return false;
		const char *sep = dir_sep(ctx->cur_dir);
		if (snprintf(ctx->out_path, ctx->out_cap, "%s%s%s",
		             ctx->cur_dir, sep, ctx->filename) >= (int)ctx->out_cap) {
			return false;
		}
		return true;
	}

	/* List-driven modes: take the entry at cursor. */
	if (ctx->cursor < 0 || ctx->cursor >= ctx->entry_count) return false;
	const pick_entry_t *e = &ctx->entries[ctx->cursor];

	if (ctx->mode == BOXEN_UI_PICK_GET_FOLDER) {
		/* Only commit when cursor is on a directory.  ".." commits the
		 * current dir; named directories commit themselves. */
		if (!e->is_dir) return false;
		if (strcmp(e->name, "..") == 0) {
			/* Selecting ".." == picking the parent. */
			pick_navigate(ctx, "..");
			if (snprintf(ctx->out_path, ctx->out_cap, "%s",
			             ctx->cur_dir) >= (int)ctx->out_cap) {
				return false;
			}
			return true;
		}
		const char *sep = dir_sep(ctx->cur_dir);
		if (snprintf(ctx->out_path, ctx->out_cap, "%s%s%s",
		             ctx->cur_dir, sep, e->name) >= (int)ctx->out_cap) {
			return false;
		}
		return true;
	}

	/* GET_FILE / GET_DISK: must be a file (not a directory). */
	if (e->is_dir) return false;
	const char *sep = dir_sep(ctx->cur_dir);
	if (snprintf(ctx->out_path, ctx->out_cap, "%s%s%s",
	             ctx->cur_dir, sep, e->name) >= (int)ctx->out_cap) {
		return false;
	}
	return true;
}

/* -- key handling -- */

/* PUT_FILE commit with overwrite-confirm.  Matches the data-loss
 * protection in the legacy file_browser put-mode (file_browser.c:485-650).
 *
 * Builds the candidate path; if a file already exists at that path,
 * opens a 3-button modal ("Overwrite" / "Cancel" / "Choose different
 * name") and routes by user choice.  Boxen modal reentrancy is
 * supported -- the inner button modal runs its own mini event loop
 * under the picker's stack frame; the outer picker's window stays
 * marked modal and reclaims focus when the inner closes (via the
 * host's restore_focus on close + Phase 1's known one-tick delay). */
static pick_put_result_t pick_try_commit_put_file(pick_ctx_t *ctx) {
	if (ctx->filename_len == 0) return PICK_PUT_NO_FILENAME;
	if (!pick_build_output(ctx)) return PICK_PUT_NO_FILENAME;

	struct stat st;
	if (stat(ctx->out_path, &st) != 0) {
		/* Doesn't exist -- clear to write. */
		return PICK_PUT_COMMITTED;
	}

	/* File or directory already exists at the target.  Prompt the user.
	 * Wipe out_path so a downstream caller that observes KEEP_OPEN /
	 * CANCEL_ALL doesn't see a stale value if they forget to check the
	 * return. */
	const char *buttons[3] = { "Overwrite", "Cancel", "Choose different name" };
	int choice = boxen_ui_button_select("File already exists.  What do you want to do?",
	                                     buttons, 3);
	switch (choice) {
	case 1: /* Overwrite */
		return PICK_PUT_COMMITTED;
	case 3: /* Choose different name */
		ctx->out_path[0] = '\0';
		return PICK_PUT_KEEP_OPEN;
	case 2: /* Cancel */
	case 0: /* Esc/Ctrl-C on the modal == Cancel */
	default:
		ctx->out_path[0] = '\0';
		return PICK_PUT_CANCEL_ALL;
	}
}

static void pick_filename_insert(pick_ctx_t *ctx, char c) {
	if (ctx->filename_len + 1 >= PICK_FILENAME_MAX) return;
	if (ctx->filename_cursor < ctx->filename_len) {
		memmove(&ctx->filename[ctx->filename_cursor + 1],
		        &ctx->filename[ctx->filename_cursor],
		        (size_t)(ctx->filename_len - ctx->filename_cursor));
	}
	ctx->filename[ctx->filename_cursor++] = c;
	ctx->filename_len++;
	ctx->filename[ctx->filename_len] = '\0';
}

static void pick_filename_backspace(pick_ctx_t *ctx) {
	if (ctx->filename_cursor == 0) return;
	if (ctx->filename_cursor < ctx->filename_len) {
		memmove(&ctx->filename[ctx->filename_cursor - 1],
		        &ctx->filename[ctx->filename_cursor],
		        (size_t)(ctx->filename_len - ctx->filename_cursor));
	}
	ctx->filename_cursor--;
	ctx->filename_len--;
	ctx->filename[ctx->filename_len] = '\0';
}

static void pick_handle_key(pick_ctx_t *ctx, const boxen_event_t *ev) {
	if (ev->type != BOXEN_EV_KEY) return;

	/* Filename-row focus path (PUT_FILE only). */
	if (ctx->filename_focus) {
		switch (ev->key.key) {
		case BOXEN_KEY_ENTER: {
			pick_put_result_t r = pick_try_commit_put_file(ctx);
			if (r == PICK_PUT_COMMITTED) {
				ctx->committed = true;
				ctx->done = true;
			} else if (r == PICK_PUT_CANCEL_ALL) {
				ctx->done = true;
			}
			/* PICK_PUT_KEEP_OPEN / PICK_PUT_NO_FILENAME: stay open. */
			return;
		}
		case BOXEN_KEY_ESCAPE:
		case BOXEN_KEY_CTRL_C:
			ctx->done = true;
			return;
		case BOXEN_KEY_TAB:
			ctx->filename_focus = false;
			return;
		case BOXEN_KEY_BACKSPACE:
		case BOXEN_KEY_CTRL_H:
			pick_filename_backspace(ctx);
			return;
		case BOXEN_KEY_LEFT:
			if (ctx->filename_cursor > 0) ctx->filename_cursor--;
			return;
		case BOXEN_KEY_RIGHT:
			if (ctx->filename_cursor < ctx->filename_len) ctx->filename_cursor++;
			return;
		case BOXEN_KEY_HOME:
		case BOXEN_KEY_CTRL_A:
			ctx->filename_cursor = 0;
			return;
		case BOXEN_KEY_END:
		case BOXEN_KEY_CTRL_E:
			ctx->filename_cursor = ctx->filename_len;
			return;
		case BOXEN_KEY_CTRL_U:
			ctx->filename_len = 0;
			ctx->filename_cursor = 0;
			ctx->filename[0] = '\0';
			return;
		default:
			if (ev->key.ch >= 0x20 && ev->key.ch < 0x7F) {
				pick_filename_insert(ctx, (char)ev->key.ch);
			}
			return;
		}
	}

	/* List focus path. */
	switch (ev->key.key) {
	case BOXEN_KEY_ESCAPE:
	case BOXEN_KEY_CTRL_C:
		ctx->done = true;
		return;
	case BOXEN_KEY_UP:
		if (ctx->cursor > 0) ctx->cursor--;
		return;
	case BOXEN_KEY_DOWN:
		if (ctx->cursor + 1 < ctx->entry_count) ctx->cursor++;
		return;
	case BOXEN_KEY_HOME:
		ctx->cursor = 0;
		return;
	case BOXEN_KEY_END:
		if (ctx->entry_count > 0) ctx->cursor = ctx->entry_count - 1;
		return;
	case BOXEN_KEY_PGUP: {
		int list_h = ctx->content_h - 4;
		if (ctx->mode == BOXEN_UI_PICK_PUT_FILE) list_h -= 2;
		if (list_h < 1) list_h = 1;
		ctx->cursor -= list_h;
		if (ctx->cursor < 0) ctx->cursor = 0;
		return;
	}
	case BOXEN_KEY_PGDN: {
		int list_h = ctx->content_h - 4;
		if (ctx->mode == BOXEN_UI_PICK_PUT_FILE) list_h -= 2;
		if (list_h < 1) list_h = 1;
		ctx->cursor += list_h;
		if (ctx->cursor >= ctx->entry_count)
			ctx->cursor = ctx->entry_count - 1;
		/* Empty directory: clamp negative cursor.  Without this PgDn at
		 * entry_count==0 leaves cursor at -1 and the UP/DOWN guards
		 * never recover. */
		if (ctx->cursor < 0) ctx->cursor = 0;
		return;
	}
	case BOXEN_KEY_BACKSPACE:
		/* Backspace from list: navigate up one level. */
		pick_navigate(ctx, "..");
		return;
	case BOXEN_KEY_TAB:
		if (ctx->mode == BOXEN_UI_PICK_PUT_FILE) {
			ctx->filename_focus = true;
		}
		return;
	case BOXEN_KEY_ENTER: {
		if (ctx->cursor < 0 || ctx->cursor >= ctx->entry_count) return;
		const pick_entry_t *e = &ctx->entries[ctx->cursor];
		if (e->is_dir) {
			pick_navigate(ctx, e->name);
		} else {
			/* GET_FOLDER on a file: ignore. */
			if (ctx->mode == BOXEN_UI_PICK_GET_FOLDER) return;
			/* PUT_FILE on a file: pre-populate the filename field with
			 * the chosen name and route through the overwrite-confirm
			 * helper.  Pure list-row "save as" gesture without having
			 * to Tab to the filename row first. */
			if (ctx->mode == BOXEN_UI_PICK_PUT_FILE) {
				size_t nlen = strlen(e->name);
				if (nlen >= PICK_FILENAME_MAX) return;
				memcpy(ctx->filename, e->name, nlen + 1);
				ctx->filename_len = (int)nlen;
				ctx->filename_cursor = ctx->filename_len;
				pick_put_result_t r = pick_try_commit_put_file(ctx);
				if (r == PICK_PUT_COMMITTED) {
					ctx->committed = true;
					ctx->done = true;
				} else if (r == PICK_PUT_CANCEL_ALL) {
					ctx->done = true;
				}
				return;
			}
			if (pick_build_output(ctx)) {
				ctx->committed = true;
				ctx->done = true;
			}
		}
		return;
	}
	default:
		/* Space picks the current dir in GET_FOLDER mode (matches the
		 * hint text). */
		if (ev->key.ch == ' '
		    && ctx->mode == BOXEN_UI_PICK_GET_FOLDER) {
			if (snprintf(ctx->out_path, ctx->out_cap, "%s",
			             ctx->cur_dir) < (int)ctx->out_cap) {
				ctx->committed = true;
				ctx->done = true;
			}
		}
		return;
	}
}

/* -- modal placement + entry point -- */

static boxen_rect_t place_picker_modal(int screen_w, int screen_h) {
	int win_w = screen_w - 4;
	int win_h = screen_h - 4;
	if (win_w < 40) win_w = 40;
	if (win_h < 10) win_h = 10;
	if (win_w > screen_w) win_w = screen_w;
	if (win_h > screen_h) win_h = screen_h;
	int x = (screen_w - win_w) / 2;
	int y = (screen_h - win_h) / 2;
	if (x < 0) x = 0;
	if (y < 0) y = 0;
	boxen_rect_t r = { x, y, win_w, win_h };
	return r;
}

/* Picker resize / key adapters for the shared modal loop. */
static void pick_resize_cb(void *vctx, int sw, int sh) {
	pick_ctx_t *ctx = (pick_ctx_t *)vctx;
	if (sw < 20) sw = 20;
	if (sh < 10) sh = 10;
	boxen_rect_t r = place_picker_modal(sw, sh);
	boxen_window_set_rect(ctx->win, r);
	ctx->content_w = boxen_window_content_width(ctx->win);
	ctx->content_h = boxen_window_content_height(ctx->win);
}

static void pick_key_cb(void *vctx, const boxen_event_t *ev) {
	pick_handle_key((pick_ctx_t *)vctx, ev);
}

bool boxen_ui_pick_file(boxen_ui_pick_mode_t mode,
                        const char *prompt,
                        const char *start_path,
                        const char *type_filter,
                        char *out_path, size_t out_cap) {
	if (!out_path || out_cap < 2) return false;
	out_path[0] = '\0';

	const boxen_ui_host_t *host = load_host();
	if (!host) return false;

	pick_ctx_t ctx;
	memset(&ctx, 0, sizeof(ctx));
	ctx.mode = mode;
	ctx.prompt = (prompt && prompt[0]) ? prompt : "Select a file:";
	ctx.type_filter = type_filter;
	ctx.out_path = out_path;
	ctx.out_cap = out_cap;
	ctx.host = host;

	/* Determine starting directory.  PUT_FILE: a start_path with a
	 * trailing filename component is split -- dir part becomes
	 * cur_dir, basename pre-populates filename. */
	const char *initial_dir = start_path;
	char split_dir[PICK_PATH_MAX];
	if (mode == BOXEN_UI_PICK_PUT_FILE && start_path && start_path[0]) {
		struct stat st;
		if (stat(start_path, &st) == 0 && S_ISDIR(st.st_mode)) {
			initial_dir = start_path;
		} else {
			/* Treat as path-with-filename. */
			const char *slash = strrchr(start_path, '/');
			if (slash && slash != start_path) {
				size_t dlen = (size_t)(slash - start_path);
				if (dlen < sizeof(split_dir)) {
					memcpy(split_dir, start_path, dlen);
					split_dir[dlen] = '\0';
					initial_dir = split_dir;
				}
				size_t blen = strlen(slash + 1);
				if (blen < sizeof(ctx.filename)) {
					memcpy(ctx.filename, slash + 1, blen);
					ctx.filename[blen] = '\0';
					ctx.filename_len = (int)blen;
					ctx.filename_cursor = ctx.filename_len;
				}
			} else {
				/* Bare filename, no dir component. */
				size_t blen = strlen(start_path);
				if (blen < sizeof(ctx.filename)) {
					memcpy(ctx.filename, start_path, blen);
					ctx.filename[blen] = '\0';
					ctx.filename_len = (int)blen;
					ctx.filename_cursor = ctx.filename_len;
				}
			}
		}
	}

	/* GET_DISK on macOS browses /Volumes/.  On other platforms fall
	 * back to "/".  We hard-code this rather than enumerating mount
	 * points via getfsstat (legacy file_browser does that) because
	 * /Volumes/ already presents the mounted disks as directory
	 * entries on macOS. */
	if (mode == BOXEN_UI_PICK_GET_DISK) {
#ifdef __APPLE__
		initial_dir = "/Volumes";
#else
		initial_dir = "/";
#endif
	}

	if (initial_dir && initial_dir[0] == '/') {
		char resolved[PICK_PATH_MAX];
		if (realpath(initial_dir, resolved) != NULL
		    && strlen(resolved) < sizeof(ctx.cur_dir)) {
			strcpy(ctx.cur_dir, resolved);
		}
	}
	if (ctx.cur_dir[0] == '\0') {
		if (getcwd(ctx.cur_dir, sizeof(ctx.cur_dir)) == NULL) {
			strcpy(ctx.cur_dir, "/");
		}
	}

	/* Allocate the entries array on the heap so it can hold a sizable
	 * directory without exploding the stack. */
	ctx.entry_cap = PICK_MAX_ENTRIES;
	ctx.entries = (pick_entry_t *)calloc((size_t)ctx.entry_cap,
	                                     sizeof(pick_entry_t));
	if (!ctx.entries) return false;

	if (!pick_load_directory(&ctx)) {
		/* Couldn't read initial dir; fall back to "/". */
		strcpy(ctx.cur_dir, "/");
		pick_load_directory(&ctx);
	}

	/* Open window. */
	int sw = 80, sh = 24;
	boxen_get_screen_size(&sw, &sh);
	if (sw < 20) sw = 20;
	if (sh < 10) sh = 10;

	boxen_rect_t rect = place_picker_modal(sw, sh);
	boxen_window_t *win = boxen_window_open(NULL, rect, &ctx);
	if (!win) {
		free(ctx.entries);
		return false;
	}

	ctx.win = win;
	ctx.content_w = boxen_window_content_width(win);
	ctx.content_h = boxen_window_content_height(win);

	boxen_window_set_borders(win, true);
	boxen_window_set_modal(win, true);
	boxen_window_set_movable(win, false);
	boxen_window_set_resizable(win, false);
	boxen_window_set_draw(win, pick_render_cb);
	boxen_window_focus(win);
	boxen_window_raise(win);

	/* Picker uses the shared modal loop -- no cancelled out-param here
	 * because the picker's commit/cancel is represented via
	 * ctx.committed (set only on a successful commit; loop-driven
	 * cancellation paths simply leave it false). */
	run_modal_loop(win, host, &ctx, &ctx.done, NULL,
	               pick_resize_cb, pick_key_cb);

	boxen_window_set_cursor_visible(win, false);
	boxen_window_close(win);
	if (host->restore_focus) host->restore_focus(host->restore_focus_ctx);
	boxen_present();

	free(ctx.entries);
	return ctx.committed;
}
