/*
 * boxen_repl.c -- boxen-native Frontier REPL.
 *
 * C.0 tracer-bullet: minimum-viable boxen REPL.
 *
 * What C.0 ships:
 *   - boxen_init + two-window layout (output pane + input bar) + footer hint
 *   - Scrollback ring buffer (BOXEN_REPL_SCROLLBACK_SIZE lines)
 *   - Character-by-character input; Enter submits, Backspace deletes,
 *     Escape clears, Ctrl-C exits
 *   - Slash-prefixed lines dispatched through dispatch_slash_command
 *     (exposed from repl.c via repl_slash_dispatch.h); other lines dispatched
 *     through repl_eval_script
 *   - GIL discipline mirrored verbatim from debugger_tui_main
 *     (snapshot/restore around boxen_poll_event; ADR-014)
 *   - Heap-allocated state; drain-before-free contract (mirrors B.6/PR #722)
 *   - BOXEN_REPL_OMIT_MAIN guard isolates GIL symbols from test builds
 *     (same pattern as DEBUGGER_TUI_OMIT_MAIN)
 *
 * GIL discipline:
 *   - Snapshot hthreadglobals before yielding; restore after poll returns.
 *   - Pattern copied verbatim from debugger_tui.c:2912-2918.
 *   - Per DEBUGGER_TUI_HANDOFF.md and ADR-014.
 *
 * 2026-06-08 JES Phase C.0 #691
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Frontier contributors
 */

#include "boxen_repl.h"
#include "boxen_repl_internal.h"

#include "boxen/boxen.h"
#include "../Common/headers/logging.h"

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>

/* -------------------------------------------------------------------------
 * Forward declarations for draw and input callbacks
 * ---------------------------------------------------------------------- */
static void draw_output_pane(boxen_window_t *win, void *user_data);
static void draw_input_bar(boxen_window_t *win, void *user_data);
static void draw_footer_hint(boxen_window_t *win, void *user_data);
static void on_input(boxen_window_t *win, const boxen_event_t *ev,
                     void *user_data);

/* -------------------------------------------------------------------------
 * 2026-06-08 JES Phase C.0 #691: layout constants.
 *
 * Two-region layout (bottom-up):
 *   row (th-1): footer hint (borderless, pinned)
 *   row (th-2): input bar   (borderless, pinned)
 *   rows 0..(th-3): output pane (scrollback)
 *
 * Minimum terminal height: 4 rows.
 * ---------------------------------------------------------------------- */

#define REPL_INPUT_PROMPT "> "

#define REPL_FOOTER_TEXT "Ctrl-C: quit  Enter: run  Esc: clear  /help: commands"

/* Width of the "> " prompt prefix in the input bar */
#define REPL_INPUT_PROMPT_LEN 2

/* -------------------------------------------------------------------------
 * 2026-06-08 JES Phase C.0 #691: repl_build_layout.
 *
 * Creates the three boxen windows.  Any pre-existing windows are closed
 * first.  Called from boxen_repl_state_init and the resize path in
 * boxen_repl_run_one_tick.
 *
 * Window geometry (mirrors debugger_tui.c::tui_build_layout pattern):
 *   - footer:  borderless, pinned to bottom, 1 row
 *   - input:   borderless, pinned to bottom, 1 row (above footer)
 *   - output:  takes the rest
 * ---------------------------------------------------------------------- */
static void repl_build_layout(boxen_repl_state_t *s, int tw, int th) {
	/* Close any existing windows */
	if (s->footer_win != NULL) { boxen_window_close(s->footer_win); s->footer_win = NULL; }
	if (s->input_win  != NULL) { boxen_window_close(s->input_win);  s->input_win  = NULL; }
	if (s->output_win != NULL) { boxen_window_close(s->output_win); s->output_win = NULL; }

	if (tw < 4) tw = 4;
	if (th < 4) th = 4;

	/* Footer: bottom row, borderless */
	boxen_rect_t footer_rect = { 0, th - 1, tw, 1 };
	s->footer_win = boxen_window_open("footer", footer_rect, NULL);
	if (s->footer_win != NULL) {
		boxen_window_set_borders(s->footer_win, false);
		boxen_window_set_pinned(s->footer_win, BOXEN_PIN_BOTTOM);
	}

	/* Input bar: row above footer, borderless */
	boxen_rect_t input_rect = { 0, th - 2, tw, 1 };
	s->input_win  = boxen_window_open("input", input_rect, NULL);
	if (s->input_win != NULL) {
		boxen_window_set_borders(s->input_win, false);
	}

	/* Output pane: everything above the input bar */
	int output_h = th - 2;
	if (output_h < 1) output_h = 1;
	boxen_rect_t output_rect = { 0, 0, tw, output_h };
	s->output_win = boxen_window_open("output", output_rect, NULL);
	if (s->output_win != NULL) {
		boxen_window_set_borders(s->output_win, false);
	}
}

/* -------------------------------------------------------------------------
 * 2026-06-08 JES Phase C.0 #691: repl_wire_callbacks.
 * ---------------------------------------------------------------------- */
static void repl_wire_callbacks(boxen_repl_state_t *s) {
	if (s->output_win != NULL) {
		boxen_window_set_draw(s->output_win, draw_output_pane);
		boxen_window_set_user_data(s->output_win, s);
	}
	if (s->input_win != NULL) {
		boxen_window_set_draw(s->input_win,  draw_input_bar);
		boxen_window_set_input(s->input_win, on_input);
		boxen_window_set_user_data(s->input_win, s);
	}
	if (s->footer_win != NULL) {
		boxen_window_set_draw(s->footer_win, draw_footer_hint);
		boxen_window_set_user_data(s->footer_win, s);
	}

	/* Focus the input bar so key events route to on_input */
	if (s->input_win != NULL) {
		boxen_window_focus(s->input_win);
	}
}

/* -------------------------------------------------------------------------
 * 2026-06-08 JES Phase C.0 #691: draw callbacks.
 * ---------------------------------------------------------------------- */

static void draw_output_pane(boxen_window_t *win, void *user_data) {
	boxen_repl_state_t *s = (boxen_repl_state_t *)user_data;
	if (s == NULL) return;

	int w = boxen_window_content_width(win);
	int h = boxen_window_content_height(win);
	if (w <= 0 || h <= 0) return;

	/* Clear the pane */
	{
		boxen_rect_t r = { 0, 0, w, h };
		boxen_fill_rect(win, r, ' ',
		                BOXEN_COLOR_DEFAULT, BOXEN_COLOR_DEFAULT, BOXEN_ATTR_NONE);
	}

	/* Render scrollback lines bottom-justified: most-recent at row (h-1).
	 * Show the last `show_count` entries where show_count = min(count, h). */
	int count = s->scrollback_count;
	if (count > h) count = h;

	char linebuf[BOXEN_REPL_SCROLLBACK_LINE_MAX];
	int  cap = (w < (int)(sizeof(linebuf) - 1)) ? w : (int)(sizeof(linebuf) - 1);

	for (int i = 0; i < count; i++) {
		int ring_idx = (s->scrollback_head - count + i + BOXEN_REPL_SCROLLBACK_SIZE)
		               % BOXEN_REPL_SCROLLBACK_SIZE;
		const char *line = s->scrollback[ring_idx];
		if (line == NULL) continue;

		int row = h - count + i;  /* bottom-justify */
		if (row < 0 || row >= h) continue;

		/* Truncate to width and space-pad for clean drawing */
		int n = snprintf(linebuf, (size_t)(cap + 1), "%-*.*s", cap, cap, line);
		(void)n;
		boxen_draw_text(win, 0, row, linebuf,
		                BOXEN_COLOR_DEFAULT, BOXEN_COLOR_DEFAULT, BOXEN_ATTR_NONE);
	}
}

static void draw_input_bar(boxen_window_t *win, void *user_data) {
	boxen_repl_state_t *s = (boxen_repl_state_t *)user_data;
	if (s == NULL) return;

	int w = boxen_window_content_width(win);
	if (w <= 0) return;

	/* Build the full input row: "> " + typed text, padded to width */
	char linebuf[BOXEN_REPL_INPUT_MAX + REPL_INPUT_PROMPT_LEN + 4];
	int  cap = (w < (int)(sizeof(linebuf) - 1)) ? w : (int)(sizeof(linebuf) - 1);

	int n = snprintf(linebuf, (size_t)(cap + 1), "%-*.*s",
	                 cap, cap,
	                 REPL_INPUT_PROMPT);
	(void)n;

	/* Overlay typed text starting at REPL_INPUT_PROMPT_LEN */
	int text_len = s->input_cursor;
	int text_avail = cap - REPL_INPUT_PROMPT_LEN;
	if (text_len > text_avail) text_len = text_avail;
	if (text_len > 0 && text_avail > 0) {
		memcpy(linebuf + REPL_INPUT_PROMPT_LEN, s->input_buf, (size_t)text_len);
		/* Re-pad trailing part */
		int pad_start = REPL_INPUT_PROMPT_LEN + text_len;
		if (pad_start < cap) {
			memset(linebuf + pad_start, ' ', (size_t)(cap - pad_start));
		}
		linebuf[cap] = '\0';
	}

	boxen_draw_text(win, 0, 0, linebuf,
	                BOXEN_COLOR_DEFAULT, BOXEN_COLOR_DEFAULT, BOXEN_ATTR_NONE);

	/* Position the cursor at the end of the typed text */
	int cursor_col = REPL_INPUT_PROMPT_LEN + s->input_cursor;
	if (cursor_col >= cap) cursor_col = cap - 1;
	boxen_window_set_cursor(win, cursor_col, 0);
}

static void draw_footer_hint(boxen_window_t *win, void *user_data) {
	(void)user_data;
	int w = boxen_window_content_width(win);
	if (w <= 0) return;

	char linebuf[512];
	int cap = (w < (int)(sizeof(linebuf) - 1)) ? w : (int)(sizeof(linebuf) - 1);
	int n = snprintf(linebuf, (size_t)(cap + 1), "%-*.*s", cap, cap, REPL_FOOTER_TEXT);
	(void)n;
	boxen_draw_text(win, 0, 0, linebuf,
	                BOXEN_COLOR_DEFAULT, BOXEN_COLOR_DEFAULT, BOXEN_ATTR_DIM);
}

/* -------------------------------------------------------------------------
 * 2026-06-08 JES Phase C.0 #691: boxen_repl_append_scrollback.
 *
 * Ring-buffer append.  When the ring is full, the entry at scrollback_head
 * is the oldest; free it and reuse the slot.
 * ---------------------------------------------------------------------- */
void boxen_repl_append_scrollback(boxen_repl_state_t *s, const char *line) {
	if (s == NULL || line == NULL) return;

	int idx = s->scrollback_head;

	/* Free the slot being overwritten (oldest entry when ring is full) */
	if (s->scrollback[idx] != NULL) {
		free(s->scrollback[idx]);
		s->scrollback[idx] = NULL;
	}

	s->scrollback[idx] = strdup(line);
	s->scrollback_head = (idx + 1) % BOXEN_REPL_SCROLLBACK_SIZE;

	/* Count grows until the ring is full */
	if (s->scrollback_count < BOXEN_REPL_SCROLLBACK_SIZE) {
		s->scrollback_count++;
	}

	if (s->output_win != NULL) {
		boxen_window_invalidate(s->output_win);
	}
}

/* -------------------------------------------------------------------------
 * 2026-06-08 JES Phase C.0 #691: submit_input.
 *
 * Called on Enter.  Echoes the input line to the scrollback, dispatches,
 * and clears the input buffer.
 * ---------------------------------------------------------------------- */
static void submit_input(boxen_repl_state_t *s) {
	if (s->input_cursor == 0) {
		if (s->input_win != NULL) boxen_window_invalidate(s->input_win);
		return;
	}

	/* Echo with "> " prefix */
	char echo_buf[BOXEN_REPL_INPUT_MAX + 4];
	snprintf(echo_buf, sizeof(echo_buf), "> %s", s->input_buf);
	boxen_repl_append_scrollback(s, echo_buf);

	bool running = true;

	if (s->input_buf[0] == '/') {
		/* Slash command: route through the hook (or production implementation) */
		if (s->slash_dispatch_hook != NULL) {
			s->slash_dispatch_hook(s->input_buf, &running);
		}
#ifndef BOXEN_REPL_OMIT_MAIN
		else {
			boxen_repl_real_slash_dispatch(s->input_buf, &running);
		}
#endif
	} else {
		/* Expression evaluation */
		char result_buf[BOXEN_REPL_SCROLLBACK_LINE_MAX];
		char error_buf[BOXEN_REPL_SCROLLBACK_LINE_MAX];
		result_buf[0] = '\0';
		error_buf[0]  = '\0';

		bool ok;
		if (s->repl_eval_hook != NULL) {
			ok = s->repl_eval_hook(s->input_buf,
			                       result_buf, sizeof(result_buf),
			                       error_buf,  sizeof(error_buf));
		} else {
#ifndef BOXEN_REPL_OMIT_MAIN
			ok = boxen_repl_real_eval(s->input_buf,
			                          result_buf, sizeof(result_buf),
			                          error_buf,  sizeof(error_buf));
#else
			ok = false;
			snprintf(error_buf, sizeof(error_buf), "(no eval hook in test build)");
#endif
		}

		if (ok) {
			if (result_buf[0] != '\0') {
				boxen_repl_append_scrollback(s, result_buf);
			}
		} else {
			char err_line[BOXEN_REPL_SCROLLBACK_LINE_MAX + 8];
			snprintf(err_line, sizeof(err_line), "Error: %s", error_buf);
			boxen_repl_append_scrollback(s, err_line);
		}
	}

	if (!running) {
		s->should_quit = true;
	}

	/* Clear input */
	s->input_buf[0] = '\0';
	s->input_cursor = 0;

	if (s->input_win  != NULL) boxen_window_invalidate(s->input_win);
	if (s->output_win != NULL) boxen_window_invalidate(s->output_win);
}

/* -------------------------------------------------------------------------
 * 2026-06-08 JES Phase C.0 #691: on_input (key event handler).
 * ---------------------------------------------------------------------- */
static void on_input(boxen_window_t *win, const boxen_event_t *ev,
                     void *user_data) {
	(void)win;
	boxen_repl_state_t *s = (boxen_repl_state_t *)user_data;
	if (s == NULL || ev == NULL || ev->type != BOXEN_EV_KEY) return;

	/* Ctrl-C: exit */
	if (ev->key.key == BOXEN_KEY_CTRL_C) {
		s->should_quit = true;
		return;
	}

	/* Escape: clear input line */
	if (ev->key.key == BOXEN_KEY_ESCAPE) {
		s->input_buf[0] = '\0';
		s->input_cursor = 0;
		if (s->input_win != NULL) boxen_window_invalidate(s->input_win);
		return;
	}

	/* Enter or Ctrl-M: submit */
	if (ev->key.key == BOXEN_KEY_ENTER || ev->key.key == BOXEN_KEY_CTRL_M) {
		submit_input(s);
		return;
	}

	/* Backspace: delete last character */
	if (ev->key.key == BOXEN_KEY_BACKSPACE) {
		if (s->input_cursor > 0) {
			s->input_cursor--;
			s->input_buf[s->input_cursor] = '\0';
		}
		/* Underflow guard: cursor is already 0 when input is empty, nothing to do */
		if (s->input_win != NULL) boxen_window_invalidate(s->input_win);
		return;
	}

	/* Printable ASCII */
	if (ev->key.key == BOXEN_KEY_NONE && ev->key.ch >= 0x20 && ev->key.ch < 0x7F) {
		if (s->input_cursor < BOXEN_REPL_INPUT_MAX - 1) {
			s->input_buf[s->input_cursor]     = (char)ev->key.ch;
			s->input_buf[s->input_cursor + 1] = '\0';
			s->input_cursor++;
		}
		if (s->input_win != NULL) boxen_window_invalidate(s->input_win);
		return;
	}
}

/* -------------------------------------------------------------------------
 * 2026-06-08 JES Phase C.0 #691: public internal API
 * ---------------------------------------------------------------------- */

void boxen_repl_state_init(boxen_repl_state_t *s, int tw, int th) {
	memset(s, 0, sizeof(boxen_repl_state_t));

	/* Production hooks (only set when not in test build).
	 * BOXEN_REPL_OMIT_MAIN guards the production-only symbols (repl_eval_script,
	 * dispatch_slash_command) from the test binary. */
#ifndef BOXEN_REPL_OMIT_MAIN
	s->slash_dispatch_hook = boxen_repl_real_slash_dispatch;
	s->repl_eval_hook      = boxen_repl_real_eval;
#endif

	repl_build_layout(s, tw, th);
	repl_wire_callbacks(s);
}

void boxen_repl_state_teardown(boxen_repl_state_t *s) {
	if (s->footer_win != NULL) { boxen_window_close(s->footer_win); s->footer_win = NULL; }
	if (s->input_win  != NULL) { boxen_window_close(s->input_win);  s->input_win  = NULL; }
	if (s->output_win != NULL) { boxen_window_close(s->output_win); s->output_win = NULL; }

	for (int i = 0; i < BOXEN_REPL_SCROLLBACK_SIZE; i++) {
		free(s->scrollback[i]);
		s->scrollback[i] = NULL;
	}
	s->scrollback_head  = 0;
	s->scrollback_count = 0;
	s->input_buf[0]     = '\0';
	s->input_cursor     = 0;
	s->should_quit      = false;
	s->slash_dispatch_hook = NULL;
	s->repl_eval_hook      = NULL;
}

int boxen_repl_run_one_tick(boxen_repl_state_t *s, const boxen_event_t *ev) {
	/* Resize: rebuild layout and re-wire callbacks */
	if (ev->type == BOXEN_EV_RESIZE) {
		int nw = (ev->resize.w > 0) ? ev->resize.w : 80;
		int nh = (ev->resize.h > 0) ? ev->resize.h : 24;
		repl_build_layout(s, nw, nh);
		repl_wire_callbacks(s);
		return REPL_CONTINUE;
	}

	boxen_dispatch_event(ev);

	return s->should_quit ? REPL_QUIT : REPL_CONTINUE;
}

/* -------------------------------------------------------------------------
 * 2026-06-08 JES Phase C.0 #691: production-only section.
 *
 * headless_threading.h and debug_handler.h are included ONLY inside this
 * #ifndef guard so the test binary never references frontier_gil /
 * hthreadglobals at the dyld level (same isolation as DEBUGGER_TUI_OMIT_MAIN).
 * ---------------------------------------------------------------------- */

#ifndef BOXEN_REPL_OMIT_MAIN

#include "debug_handler.h"
#include "headless_threading.h"
#include "repl_eval.h"
#include "../Common/headers/strings.h"   /* copyptocstring */
#include <pthread.h>

/* repl_slash_dispatch.h declares the de-static'd dispatch_slash_command
 * from repl.c.  Only boxen_repl.c consumers include this header. */
#include "repl_slash_dispatch.h"

/* -------------------------------------------------------------------------
 * 2026-06-08 JES Phase C.0 #691: production hook implementations.
 * ---------------------------------------------------------------------- */

bool boxen_repl_real_slash_dispatch(const char *line, bool *running) {
	boolean keep_running = dispatch_slash_command(line, (boolean *)running);
	*running = (bool)keep_running;
	return (bool)keep_running;
}

bool boxen_repl_real_eval(const char *expr,
                          char *result_out, size_t result_cap,
                          char *error_out,  size_t error_cap) {
	bigstring result_bs;
	bigstring error_bs;
	result_bs[0] = '\0';
	error_bs[0]  = '\0';

	boolean ok = repl_eval_script(expr, result_bs, error_bs);

	if (ok) {
		/* copyptocstring converts Pascal length-prefixed string to C string.
		 * Uses a 256-byte tmp buffer (matches bigstring/lenbigstring+1 max). */
		char tmp[256];
		copyptocstring(result_bs, tmp);
		size_t n = strlen(tmp);
		if (n >= result_cap) n = result_cap - 1;
		memcpy(result_out, tmp, n);
		result_out[n] = '\0';
	} else {
		char tmp[256];
		copyptocstring(error_bs, tmp);
		size_t n = strlen(tmp);
		if (n >= error_cap) n = error_cap - 1;
		memcpy(error_out, tmp, n);
		error_out[n] = '\0';
	}

	return (bool)ok;
}

/* -------------------------------------------------------------------------
 * 2026-06-08 JES Phase C.0 #691: boxen_repl_main -- public entry point.
 *
 * GIL yield/restore mirrors debugger_tui_main (debugger_tui.c:2905-2933).
 * Teardown mirrors debugger_tui_main (debugger_tui.c:2960-2975).
 * ---------------------------------------------------------------------- */
int boxen_repl_main(const cli_options_t *opts) {
	boxen_repl_state_t *state = calloc(1, sizeof(boxen_repl_state_t));
	if (state == NULL) {
		log_error(LOG_COMP_GENERAL, "boxen_repl: out of memory allocating state");
		return 1;
	}

	/* 2026-06-08 JES Phase C.0 #691: boxen_init before querying terminal size.
	 * Same rationale as debugger_tui_main: tb_width/tb_height return
	 * TB_ERR_NOT_INIT (negative) before tb_init(). */
	const boxen_backend_t *be = boxen_tb2_backend();
	bool boxen_initialized = false;
	boxen_result_t rc = boxen_init(be, NULL, NULL);
	if (rc != BOXEN_OK) {
		log_error(LOG_COMP_GENERAL, "boxen_repl: boxen_init failed: %s",
		          boxen_last_error_str());
		free(state);
		return 1;
	}
	boxen_initialized = true;

	int tw = (be->width  && be->width()  > 0) ? be->width()  : 80;
	int th = (be->height && be->height() > 0) ? be->height() : 24;

	boxen_repl_state_init(state, tw, th);

	/* 2026-06-08 JES Phase C.0 #691: auto-dispatch /debug on startup script.
	 *
	 * Preserves PR #756 launch UX: "frontier-cli --debug-tui @path" starts
	 * the REPL and immediately dispatches "/debug <path>" as if the user
	 * typed it.  This keeps B.8's launch-entry-point contract intact while
	 * the interactive entry point changes from debugger_tui_main to this.
	 *
	 * The script_file takes precedence over inline_script; if both are set
	 * the cli_parser.c validation rejects the combination before we get here
	 * (see the mutual-exclusion check added in B.8). */
	if (opts != NULL && (opts->script_file != NULL || opts->inline_script != NULL)) {
		const char *launch_path = (opts->script_file != NULL)
		                          ? opts->script_file
		                          : opts->inline_script;
		char auto_cmd[BOXEN_REPL_INPUT_MAX];
		snprintf(auto_cmd, sizeof(auto_cmd), "/debug %s", launch_path);
		strncpy(state->input_buf, auto_cmd, sizeof(state->input_buf) - 1);
		state->input_cursor = (int)strlen(state->input_buf);
		submit_input(state);
	}

	log_info(LOG_COMP_GENERAL, "Boxen REPL: entering event loop (Ctrl-C to quit)");

	/* 2026-06-08 JES Phase C.0 #691: event loop.
	 * GIL yield pattern mirrors debugger_tui_main exactly:
	 *   snapshot -> release GIL -> poll -> reacquire -> restore -> dispatch. */
	while (!state->should_quit) {
		boxen_event_t ev;
		memset(&ev, 0, sizeof(ev));

		hdlthreadglobals main_globals = hthreadglobals;
		headless_save_threadglobals(main_globals);
		pthread_mutex_unlock(&frontier_gil);
		boxen_result_t poll_rc = boxen_poll_event(&ev, 100 /* ms timeout */);
		pthread_mutex_lock(&frontier_gil);
		headless_restore_threadglobals(main_globals);

		if (poll_rc == BOXEN_ERR_TIMEOUT) {
			boxen_present();
			continue;
		}
		if (poll_rc != BOXEN_OK) {
			log_warn(LOG_COMP_GENERAL, "boxen_repl: poll error, exiting");
			break;
		}

		boxen_repl_run_one_tick(state, &ev);
		boxen_present();
	}

	/* 2026-06-08 JES Phase C.0 #691: drain-before-free teardown.
	 *
	 * Mirrors debugger_tui_main teardown order exactly:
	 *   1. debug_wait_lazy_threads_drained() -- drain /debug child threads
	 *   2. headless_restore_threadglobals()  -- restore main-thread context
	 *   3. (no transport to NULL-clear; C.0 has no attached transport)
	 *   4. boxen_repl_state_teardown()       -- close windows, free ring
	 *   5. free(state)
	 */
	{
		hdlthreadglobals main_hglobals = hthreadglobals;
		debug_wait_lazy_threads_drained();
		headless_restore_threadglobals(main_hglobals);
	}

	boxen_repl_state_teardown(state);
	if (boxen_initialized) {
		boxen_shutdown();
	}
	free(state);
	state = NULL;

	log_info(LOG_COMP_GENERAL, "Boxen REPL: exited cleanly");
	return 0;
}

#endif /* !BOXEN_REPL_OMIT_MAIN */
