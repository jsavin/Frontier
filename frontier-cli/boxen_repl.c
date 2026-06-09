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
#include <unistd.h>    /* dup, dup2, pipe, close, read */
#include <fcntl.h>     /* fcntl, F_SETFL, O_NONBLOCK */
#include <errno.h>     /* EAGAIN, EWOULDBLOCK */

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

	/* Printable ASCII only (0x20..0x7E).
	 * 2026-06-08 JES #691 Phase C.0 round 2 P2-14: the range 0x20..0x7E
	 * is intentional.  Non-ASCII code points (0x7F and above) and control
	 * characters (< 0x20) are dropped here.  Non-ASCII input would require
	 * multi-byte handling and a Unicode-aware input cursor; that is deferred
	 * to a future milestone. */
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

	/* Stdout capture fields: -1 means not active (same sentinel as open(2)
	 * returns on failure, so teardown can guard with fd >= 0). */
	s->saved_stdout    = -1;
	s->saved_stderr    = -1;
	s->pipe_read_fd    = -1;
	s->partial_line[0] = '\0';
	s->partial_line_len = 0;

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

	/* Close the capture pipe read end if it is still open.
	 * Production teardown (boxen_repl_main) closes it before calling here;
	 * this guard is a safety net for error paths. */
	if (s->pipe_read_fd >= 0) {
		close(s->pipe_read_fd);
		s->pipe_read_fd = -1;
	}
	s->saved_stdout     = -1;
	s->saved_stderr     = -1;
	s->partial_line[0]  = '\0';
	s->partial_line_len = 0;

	/* 2026-06-08 JES #691 Phase C.0 round 3 P0: free heap-allocated launch transport.
	 * boxen_repl_main must call debug_kill_all_threads + debug_join_all_threads
	 * BEFORE calling boxen_repl_state_teardown, ensuring no thread holds a
	 * reference to this pointer when it is freed here. */
	if (s->launch_transport != NULL) {
		free(s->launch_transport);
		s->launch_transport = NULL;
	}
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
 * 2026-06-08 JES #691 Phase C.0 round 2: drain_stdout_into_scrollback.
 *
 * Reads bytes from fd (must be non-blocking) until EAGAIN/EOF, splits on
 * newlines, and appends each complete line to the scrollback ring.
 * Incomplete lines are held in s->partial_line until the next call.
 *
 * No production runtime dependencies -- compiled in both production and
 * test builds.  Tests call it directly with a test-supplied pipe fd.
 * ---------------------------------------------------------------------- */
void drain_stdout_into_scrollback(boxen_repl_state_t *s, int fd) {
	if (s == NULL || fd < 0) return;

	char buf[512];
	ssize_t n;

	while ((n = read(fd, buf, sizeof(buf))) > 0) {
		const char *p   = buf;
		const char *end = buf + n;

		while (p < end) {
			const char *nl = NULL;
			/* Find next newline in this chunk */
			for (const char *q = p; q < end; q++) {
				if (*q == '\n') { nl = q; break; }
			}

			if (nl != NULL) {
				/* We have a complete line: partial_line (if any) + this segment */
				size_t seg_len = (size_t)(nl - p);
				size_t avail   = sizeof(s->partial_line) - s->partial_line_len - 1;

				/* 2026-06-08 JES #691 Phase C.0 round 3 P2-5: truncation warning.
				 * When a single line exceeds the partial buffer, append a visible
				 * marker so the user knows content was dropped. */
				if (seg_len > avail) {
					static const char trunc_marker[] = "...[truncated]";
					size_t marker_len = sizeof(trunc_marker) - 1;
					/* Ensure we have room for the marker at the end of avail */
					if (avail > marker_len) {
						memcpy(s->partial_line + s->partial_line_len, p, avail - marker_len);
						s->partial_line_len += avail - marker_len;
					}
					/* Stamp the marker; avail - marker_len may be 0 if buffer was
					 * already full -- in that case we overwrite the last marker_len
					 * bytes of whatever is already accumulated. */
					size_t stamp_pos = (s->partial_line_len > marker_len)
					                   ? (s->partial_line_len - marker_len)
					                   : 0;
					memcpy(s->partial_line + stamp_pos, trunc_marker, marker_len);
					s->partial_line_len = stamp_pos + marker_len;
					seg_len = avail; /* mark as consumed (clamp applied) */
				} else {
					memcpy(s->partial_line + s->partial_line_len, p, seg_len);
					s->partial_line_len += seg_len;
				}
				s->partial_line[s->partial_line_len] = '\0';

				/* Append to scrollback (even if empty -- echoes a blank line) */
				boxen_repl_append_scrollback(s, s->partial_line);

				/* Reset partial buffer */
				s->partial_line[0]  = '\0';
				s->partial_line_len = 0;

				p = nl + 1; /* advance past the newline */
			} else {
				/* No newline in remaining chunk -- accumulate into partial_line */
				size_t seg_len = (size_t)(end - p);
				size_t avail   = sizeof(s->partial_line) - s->partial_line_len - 1;
				if (seg_len > avail) seg_len = avail;

				memcpy(s->partial_line + s->partial_line_len, p, seg_len);
				s->partial_line_len += seg_len;
				s->partial_line[s->partial_line_len] = '\0';

				p = end; /* consumed all of this chunk */
			}
		}
	}
	/* n == 0 (EOF) or n < 0 with errno == EAGAIN/EWOULDBLOCK: drain complete */
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
#include "repl.h"                        /* repl_install_verb_host, repl_uninstall_verb_host, repl_reset_exit_flag */
#include "../Common/headers/strings.h"   /* copyptocstring */
#include <pthread.h>

/* repl_slash_dispatch.h declares the de-static'd dispatch_slash_command
 * from repl.c.  Only boxen_repl.c consumers include this header.
 *
 * 2026-06-08 JES #691 Phase C.0 round 2 P2-18: note on g_repl_exit_requested
 * side effect -- dispatch_slash_command may set g_repl_exit_requested (via the
 * replverbhost_exit kernel-verb host adapter) when the /exit command is
 * processed.  The boxen REPL drives exit from the return value (*running set
 * false -> s->should_quit = true), not by polling g_repl_exit_requested
 * directly. */
#include "repl_slash_dispatch.h"

/* -------------------------------------------------------------------------
 * 2026-06-08 JES Phase C.0 #691: production hook implementations.
 * ---------------------------------------------------------------------- */

bool boxen_repl_real_slash_dispatch(const char *line, bool *running) {
	/* 2026-06-08 JES #691 Phase C.0 round 2 P1-5: avoid strict aliasing violation.
	 * dispatch_slash_command takes (boolean *) which may have different storage
	 * size/alignment than C99 bool.  Use explicit local variables and cast. */
	boolean br = (boolean)*running;
	boolean keep_running = dispatch_slash_command(line, &br);
	*running = (bool)br;
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
 * 2026-06-08 JES #691 Phase C.0 round 2: no-op transport write_line stub.
 *
 * The launch transport used by debug_launch_from_options below needs a
 * write_line callback.  In C.0 the boxen REPL does not yet render debug
 * notifications (suspended/completed) -- those are dropped here.  C.1 will
 * wire in a real callback that queues notifications into the scrollback ring.
 * ---------------------------------------------------------------------- */
static void boxen_repl_noop_write_line(void *ctx, const char *line, size_t len) {
	(void)ctx; (void)line; (void)len;
	/* C.0: debug notifications are intentionally discarded until C.1 wires
	 * the notification -> scrollback path. */
}

/* -------------------------------------------------------------------------
 * 2026-06-08 JES #691 Phase C.0 round 2: boxen_repl_main -- public entry point.
 *
 * GIL yield/restore mirrors debugger_tui_main (debugger_tui.c:2905-2933).
 * Teardown mirrors debugger_tui_main (debugger_tui.c:2960-2975).
 *
 * Round 2 fixes applied (issue #691 /gate P0/P1):
 *   P2-12: NULL check on opts.
 *   P1-8:  reset g_repl_exit_requested before installing verb host.
 *   P1-3:  install/uninstall repl verb host adapter.
 *   P0-2:  stdout capture pipe -- all slash dispatch / eval output is routed
 *           into the scrollback ring instead of written raw to the terminal.
 *   P0-1:  debug_launch_from_options replaces broken /debug slash synthesis.
 *   P1-6:  debug_set_attach_transport(NULL) in teardown.
 * ---------------------------------------------------------------------- */
int boxen_repl_main(const cli_options_t *opts) {
	/* 2026-06-08 JES #691 Phase C.0 rounds 2+3 P2-12: guard against NULL opts. */
	if (opts == NULL) {
		log_error(LOG_COMP_GENERAL, "boxen_repl: opts is NULL");
		return 1;
	}

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

	/* 2026-06-08 JES #691 Phase C.0 round 2 P1-8 / round 3 P1: clear stale exit
	 * flag from any prior REPL session before installing the verb host adapter. */
	repl_reset_exit_flag();

	/* 2026-06-08 JES #691 Phase C.0 round 2 P1-3: install repl.* verb host.
	 * Mirrors protocol_handler.c:149 and repl.c:repl_main exactly. */
	repl_install_verb_host();

	/* 2026-06-08 JES #691 Phase C.0 round 3 P1: publish g_repl_active so that
	 * repl.isActive() returns true during this REPL session. */
	repl_set_active(true);

	boxen_repl_state_init(state, tw, th);

	/* 2026-06-08 JES #691 Phase C.0 round 2 P0-2: stdout capture setup.
	 *
	 * Redirect stdout and stderr to a pipe so that slash dispatch / eval output
	 * (which calls fputs/printf to stdout) is captured and routed into the
	 * scrollback ring rather than written raw to the terminal in raw mode
	 * (which would corrupt the boxen display).
	 *
	 * Pipe write end stays BLOCKING (see spec): the producer (slash dispatch)
	 * runs synchronously between boxen_poll_event calls, so the drain happens
	 * immediately after.  Long output (> 64 KB) could stall the producer, but
	 * that is acceptable for C.0.  A future fix can add a separate drain thread
	 * if needed.
	 *
	 * On any pipe setup failure, we fall through without capture (display will
	 * be corrupted by raw stdout writes, but the REPL is still functional). */
	int pipefd[2];
	bool capture_active = false;
	if (pipe(pipefd) == 0) {
		/* Read end non-blocking so drain never blocks the event loop.
		 * 2026-06-08 JES #691 Phase C.0 round 3 P2-7: check fcntl return.
		 * If F_SETFL fails, drain would block -- fall through without capture. */
		if (fcntl(pipefd[0], F_SETFL, O_NONBLOCK) < 0) {
			log_warn(LOG_COMP_GENERAL,
			         "boxen_repl: fcntl(O_NONBLOCK) failed; skipping capture");
			close(pipefd[0]);
			close(pipefd[1]);
		} else {
			int saved_out = dup(STDOUT_FILENO);
			int saved_err = dup(STDERR_FILENO);

			if (saved_out >= 0 && saved_err >= 0) {
				/* 2026-06-08 JES #691 Phase C.0 round 3 P2-6: check dup2 returns.
				 * If either dup2 fails, restore and skip capture so the display is
				 * not left in a half-redirected state. */
				int r1 = dup2(pipefd[1], STDOUT_FILENO);
				int r2 = dup2(pipefd[1], STDERR_FILENO);
				if (r1 < 0 || r2 < 0) {
					/* Restore whichever dup2 succeeded */
					dup2(saved_out, STDOUT_FILENO);
					dup2(saved_err, STDERR_FILENO);
					close(saved_out);
					close(saved_err);
					close(pipefd[0]);
					close(pipefd[1]);
					log_warn(LOG_COMP_GENERAL,
					         "boxen_repl: dup2 failed; skipping capture");
				} else {
					close(pipefd[1]);  /* write end is now duplicated into stdout/stderr */

					state->saved_stdout = saved_out;
					state->saved_stderr = saved_err;
					state->pipe_read_fd = pipefd[0];
					capture_active = true;
				}
			} else {
				/* dup failed -- clean up without capturing */
				if (saved_out >= 0) close(saved_out);
				if (saved_err >= 0) close(saved_err);
				close(pipefd[0]);
				close(pipefd[1]);
				log_warn(LOG_COMP_GENERAL, "boxen_repl: stdout dup failed; display may corrupt");
			}
		}
	} else {
		log_warn(LOG_COMP_GENERAL, "boxen_repl: pipe() failed; display may corrupt");
	}

	/* 2026-06-08 JES #691 Phase C.0 round 2 P0-1: startup-script launch.
	 *
	 * Replaces the broken "/debug <path>" slash synthesis (which had no
	 * registered /debug command in the REPL menubar).
	 * debug_launch_from_options handles: bare ODB @path, freeform inline
	 * expression, and script file paths.  It calls op_dispatch -> handle_debug_run
	 * which spawns the debug thread suspended.
	 *
	 * GIL is held throughout this auto-launch; debug_launch_from_options requires
	 * the GIL for langcompiletext and headless_spawn_script_thread.
	 *
	 * 2026-06-08 JES #691 Phase C.0 round 3 P0: heap-allocate the transport so
	 * it outlives boxen_repl_main's stack frame.  The joinable debug/run thread
	 * (fldetached=false) caches a pointer to this transport via
	 * debug_register_thread; that pointer must remain valid until
	 * debug_join_all_threads() returns.  See debug_handler.h:196-214. */
	transport_t *launch_transport = calloc(1, sizeof(transport_t));
	if (launch_transport == NULL) {
		log_error(LOG_COMP_GENERAL,
		          "boxen_repl_main: failed to allocate launch_transport; skipping auto-launch");
		/* Continue into the REPL without auto-launch; not fatal. */
	} else {
		launch_transport->write_line = boxen_repl_noop_write_line;
		launch_transport->ctx = NULL;
		state->launch_transport = launch_transport;

		/* Register the lazy-attach transport BEFORE spawning the debug thread.
		 * Mirrors debugger_tui_main:2882 and protocol_handler.c:194. */
		debug_set_attach_transport(launch_transport);

		if (opts->script_file != NULL || opts->inline_script != NULL) {
			debug_launch_from_options(opts, launch_transport);
		}
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

		/* 2026-06-08 JES #691 Phase C.0 round 2 P0-2: drain captured stdout
		 * after every poll (event or timeout) so slash dispatch output reaches
		 * the scrollback ring promptly. */
		if (capture_active) {
			drain_stdout_into_scrollback(state, state->pipe_read_fd);
		}

		/* 2026-06-08 JES #691 Phase C.0 round 3 P1: poll repl.exit() flag.
		 * replverbhost_exit() sets g_repl_exit_requested (via repl.exit() from
		 * non-slash UserTalk code).  The slash path already sets should_quit via
		 * submit_input's running=false path; this catches the direct-call path. */
		if (repl_is_exit_requested()) {
			state->should_quit = true;
		}

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

	/* 2026-06-08 JES #691 Phase C.0 round 3: teardown sequence (revised).
	 *
	 * Round 2 teardown drained lazy threads but did NOT kill/join the joinable
	 * debug/run thread spawned by debug_launch_from_options.  That thread
	 * (fldetached=false) caches &launch_transport in its debug state record;
	 * if it runs after boxen_repl_main returns (or after the transport is freed)
	 * it dereferences a dangling pointer.
	 *
	 * 2026-06-08 JES #691 Phase C.0 round 3 P1 (concurrency note): clear the
	 * attach transport BEFORE kill+join, not after.  Mirrors protocol_handler.c
	 * which NULLs the global transport pointer first so the lazy-attach fast
	 * path gates on NULL during the kill/join window.  Without this reorder,
	 * a callScript thread that lazy-attaches between the drain (step 2) and
	 * the eventual NULL (was step 6) could register against launch_transport
	 * AFTER drain returned 0, then be freed beneath itself by the state
	 * teardown.  Inert in C.0 (no breakpoints set), real concern for C.1+.
	 *
	 * Corrected order (mirrors main.c:1286-1298 for kill+join):
	 *
	 *   1. Snapshot main hglobals before the drain/kill/join cycle.
	 *   2. debug_wait_lazy_threads_drained() -- drain detached lazy threads.
	 *   3. headless_restore_threadglobals(main_hglobals) -- restore main context.
	 *   4. debug_set_attach_transport(NULL) -- prevent NEW lazy registrations
	 *      during the kill/join window.  Mirrors protocol_handler.c:422.
	 *   5. debug_kill_all_threads() -- signal joinable threads to stop.
	 *   6. Release GIL, debug_join_all_threads(), reacquire GIL, restore hglobals.
	 *      (mirrors main.c:1291-1298 exactly)
	 *   7. Restore stdout/stderr; final drain of capture pipe.
	 *   8. repl_set_active(false) -- P1: publish end of REPL session.
	 *   9. repl_uninstall_verb_host() -- P1-3.
	 *  10. boxen_repl_state_teardown() -- frees launch_transport (P0) + windows.
	 *  11. boxen_shutdown(), free(state).
	 */
	{
		/* Step 1+2+3: drain lazy (detached) threads, restore main hglobals. */
		hdlthreadglobals main_hglobals = hthreadglobals;
		headless_save_threadglobals(main_hglobals);
		debug_wait_lazy_threads_drained();
		headless_restore_threadglobals(main_hglobals);

		/* Step 4: clear attach transport BEFORE kill+join so no new lazy thread
		 * can register against launch_transport during the kill/join window. */
		debug_set_attach_transport(NULL);

		/* Step 5: signal joinable debug threads to stop. */
		debug_kill_all_threads();

		/* Step 6: release GIL so killed threads can finish cleanup, then join.
		 * Mirrors main.c:1291-1298 verbatim. */
		{
			hdlthreadglobals saved = hthreadglobals;
			headless_save_threadglobals(saved);
			pthread_mutex_unlock(&frontier_gil);
			debug_join_all_threads();
			pthread_mutex_lock(&frontier_gil);
			headless_restore_threadglobals(saved);
		}
	}

	/* Step 7: restore stdout/stderr; final drain before display clears. */
	if (capture_active) {
		drain_stdout_into_scrollback(state, state->pipe_read_fd);
		dup2(state->saved_stdout, STDOUT_FILENO);
		dup2(state->saved_stderr, STDERR_FILENO);
		close(state->saved_stdout); state->saved_stdout = -1;
		close(state->saved_stderr); state->saved_stderr = -1;
		close(state->pipe_read_fd); state->pipe_read_fd = -1;
	}

	/* Step 8: mark REPL session inactive (P1). */
	repl_set_active(false);

	/* Step 9: uninstall verb host before teardown (P1-3). */
	repl_uninstall_verb_host();

	/* Step 10: teardown windows, ring, and launch_transport (P0 free). */
	boxen_repl_state_teardown(state);
	if (boxen_initialized) {
		boxen_shutdown();
	}

	/* Step 11: free state. */
	free(state);
	state = NULL;

	log_info(LOG_COMP_GENERAL, "Boxen REPL: exited cleanly");
	return 0;
}

#endif /* !BOXEN_REPL_OMIT_MAIN */
