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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "boxen/boxen.h"
#include "headless_threading.h"     /* frontier_gil, hthreadglobals, save/restore */
#include "../Common/headers/threadregistry.h" /* hdlthreadglobals */

/* -------------------------------------------------------------------------
 * Host registration
 * ---------------------------------------------------------------------- */

/* Set to non-NULL by boxen_repl_main once the REPL has finished bringing
 * boxen up; reset to NULL during teardown.  Linenoise mode never touches
 * it.  Single-writer (the REPL main thread); the bridge entry points are
 * called with the GIL held so reads are safe. */
static const boxen_ui_host_t *s_host = NULL;

void boxen_ui_set_active(const boxen_ui_host_t *host) {
	s_host = host;
}

bool boxen_ui_is_active(void) {
	return s_host != NULL;
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

static void run_modal(ui_ctx_t *ctx) {
	/* Initial paint before entering the wait loop. */
	boxen_window_invalidate(ctx->win);
	if (s_host && s_host->drain_capture_pipe) {
		s_host->drain_capture_pipe(s_host->drain_ctx);
	}
	boxen_present();

	while (!ctx->done) {
		boxen_event_t ev;
		memset(&ev, 0, sizeof(ev));

		hdlthreadglobals saved = hthreadglobals;
		headless_save_threadglobals(saved);
		pthread_mutex_unlock(&frontier_gil);
		boxen_result_t rc = boxen_poll_event(&ev, 100 /* ms */);
		pthread_mutex_lock(&frontier_gil);
		headless_restore_threadglobals(saved);

		if (s_host && s_host->drain_capture_pipe) {
			s_host->drain_capture_pipe(s_host->drain_ctx);
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
	if (!s_host) return NULL;

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

	run_modal(&ctx);

	boxen_window_set_cursor_visible(win, false);
	boxen_window_close(win);
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
