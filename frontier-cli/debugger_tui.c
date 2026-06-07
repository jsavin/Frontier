/*
 * debugger_tui.c -- boxen-based UserTalk debugger TUI.
 *
 * Milestone B.0: tracer-bullet skeleton.
 * planning/phase_b/EXECUTION_PLAN.md, section "Milestone B.0".
 *
 * What B.0 ships:
 *   - boxen_init + three-window placeholder layout (script, stack, footer)
 *   - Event loop with GIL release/reacquire around boxen_poll_event()
 *   - Stub transport_t (write_line is a no-op; B.1 wires notifications)
 *   - 'q', Escape, Ctrl-C to quit
 *   - BOXEN_EV_RESIZE handling: rebuild layout proportionally on terminal resize
 *   - debugger_tui_state_init / run_one_tick / state_teardown for unit tests
 *
 * GIL discipline:
 *   - Snapshot hthreadglobals before yielding; restore after poll returns.
 *   - Pattern copied verbatim from protocol_handler.c:348-354 and 411-421.
 *   - Per DEBUGGER_TUI_HANDOFF.md and ADR-014.
 *
 * Transport sentinel:
 *   - Heap-allocated to outlive lazy-attached callScript threads.
 *   - On teardown: drain -> NULL-clear -> free (same as protocol_main).
 *   - B.6 wires debug_set_attach_transport; B.0 stub skips it (no debug
 *     runtime wiring yet per EXECUTION_PLAN.md B.0 "Out of scope").
 *
 * 2026-06-06 JES Phase B.0 #691
 * 2026-06-06 JES Phase B.0 #734 round 1: terminal size + resize + dead code + quit-key
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Frontier contributors
 */

#include "debugger_tui.h"
#include "debugger_tui_internal.h"

#include "boxen/boxen.h"
#include "op_handler.h"

/* headless_threading.h is only needed for debugger_tui_main() (GIL symbols).
 * It is NOT included at file scope to keep the test-visible internal functions
 * (state_init, run_one_tick, state_teardown) free of GIL symbol dependencies.
 * The production-only functions (debugger_tui_main) include it locally. */

#include "../Common/headers/logging.h"

/* 2026-06-07 JES Phase B.1 #691: cJSON for parsing debug/getSource responses.
 * cJSON has no Frontier runtime dependencies; safe in both production and test. */
#include "../third_party/cJSON/cJSON.h"

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* -------------------------------------------------------------------------
 * 2026-06-07 JES Phase B.1 #691: source state helpers.
 *
 * tui_free_script_lines -- release the heap array of source line strings.
 * tui_store_source     -- parse a debug/getSource result object into state.
 * ---------------------------------------------------------------------- */

static void tui_free_script_lines(tui_state_t *s) {
	if (s->script_lines != NULL) {
		for (int i = 0; i < s->script_line_count; i++) {
			free(s->script_lines[i]);
		}
		free(s->script_lines);
		s->script_lines      = NULL;
		s->script_line_count = 0;
	}
}

/* Parse a debug/getSource result JSON object (the "result" sub-object) into
 * tui_state_t. On success, updates script_path, script_lines,
 * script_line_count, current_line, bp_lines, bp_line_count. */
static void tui_store_source(tui_state_t *s, cJSON *result) {
	cJSON *script_j = cJSON_GetObjectItemCaseSensitive(result, "script");
	cJSON *curline_j = cJSON_GetObjectItemCaseSensitive(result, "currentLine");
	cJSON *lines_j  = cJSON_GetObjectItemCaseSensitive(result, "lines");

	if (!cJSON_IsString(script_j) || script_j->valuestring == NULL) return;
	if (!cJSON_IsArray(lines_j)) return;

	/* Store script path */
	strncpy(s->script_path, script_j->valuestring, sizeof(s->script_path) - 1);
	s->script_path[sizeof(s->script_path) - 1] = '\0';

	/* Store current line (-1 if not present) */
	s->current_line = cJSON_IsNumber(curline_j) ? (long)curline_j->valuedouble : -1;

	/* Count lines */
	int count = cJSON_GetArraySize(lines_j);
	if (count <= 0) return;

	/* Release old source */
	tui_free_script_lines(s);

	/* Allocate new array */
	s->script_lines = (char **)malloc((size_t)count * sizeof(char *));
	if (s->script_lines == NULL) return;
	s->script_line_count = count;

	/* Reset breakpoints from this source load */
	s->bp_line_count = 0;

	int i = 0;
	cJSON *lineobj = NULL;
	cJSON_ArrayForEach(lineobj, lines_j) {
		cJSON *text_j = cJSON_GetObjectItemCaseSensitive(lineobj, "text");
		cJSON *num_j  = cJSON_GetObjectItemCaseSensitive(lineobj, "num");
		cJSON *bp_j   = cJSON_GetObjectItemCaseSensitive(lineobj, "breakpoint");

		const char *text = (cJSON_IsString(text_j) && text_j->valuestring != NULL)
		                   ? text_j->valuestring : "";
		s->script_lines[i] = strdup(text);
		if (s->script_lines[i] == NULL) s->script_lines[i] = strdup("");

		/* Record breakpoint lines */
		if (cJSON_IsTrue(bp_j) && cJSON_IsNumber(num_j) &&
			s->bp_line_count < TUI_MAX_BREAKPOINTS) {
			s->bp_lines[s->bp_line_count++] = (unsigned long)num_j->valuedouble;
		}

		i++;
	}
}

/* -------------------------------------------------------------------------
 * 2026-06-07 JES Phase B.1 #691: transport write_line -- parse NDJSON responses.
 *
 * Handles two cases:
 *   1. Notification: {"id":null,"op":"debug/suspended","params":{...}}
 *      -> stores pending_thread_id and current_line; triggers source load
 *         by storing the script path and invalidating the script pane.
 *         In B.1 the source load is done inline using the stored source; the
 *         full op_dispatch path for live sessions is wired in B.6.
 *
 *   2. Response to debug/getSource: {"id":N,"result":{"script":...,"lines":[...]}}
 *      -> parses and stores source lines via tui_store_source(); invalidates
 *         the script pane.
 *
 * Three-parameter signature per op_handler.h:45 (NOT the 2-param form the
 * handoff doc incorrectly listed; see EXECUTION_PLAN.md 2.1 Claim 1).
 * ---------------------------------------------------------------------- */

static void tui_write_line(void *ctx, const char *json, size_t len) {
	/* 2026-06-07 JES Phase B.1 #691: parse NDJSON responses/notifications. */
	tui_state_t *s = (tui_state_t *)ctx;
	if (s == NULL || json == NULL || len == 0) return;

	cJSON *root = cJSON_ParseWithLength(json, len);
	if (root == NULL) return;

	cJSON *op_j     = cJSON_GetObjectItemCaseSensitive(root, "op");
	cJSON *result_j = cJSON_GetObjectItemCaseSensitive(root, "result");

	if (cJSON_IsString(op_j) && op_j->valuestring != NULL &&
		strcmp(op_j->valuestring, "debug/suspended") == 0) {
		/* Case 1: suspended notification */
		cJSON *params_j  = cJSON_GetObjectItemCaseSensitive(root, "params");
		cJSON *tid_j     = params_j ? cJSON_GetObjectItemCaseSensitive(params_j, "threadId") : NULL;
		cJSON *line_j    = params_j ? cJSON_GetObjectItemCaseSensitive(params_j, "line")     : NULL;
		cJSON *script_j2 = params_j ? cJSON_GetObjectItemCaseSensitive(params_j, "script")   : NULL;

		if (cJSON_IsNumber(tid_j))
			s->pending_thread_id = (long)tid_j->valuedouble;
		if (cJSON_IsNumber(line_j))
			s->current_line = (long)line_j->valuedouble;
		if (cJSON_IsString(script_j2) && script_j2->valuestring != NULL) {
			strncpy(s->script_path, script_j2->valuestring,
			        sizeof(s->script_path) - 1);
			s->script_path[sizeof(s->script_path) - 1] = '\0';
		}

		/* Invalidate the script pane so the next present() redraws it */
		if (s->script_win != NULL)
			boxen_window_invalidate(s->script_win);

	} else if (cJSON_IsObject(result_j)) {
		/* Case 2: response containing a "result" object -- treat as getSource
		 * response if it has a "lines" array. */
		cJSON *lines_j = cJSON_GetObjectItemCaseSensitive(result_j, "lines");
		if (cJSON_IsArray(lines_j)) {
			tui_store_source(s, result_j);

			/* Auto-scroll to current line if suspended */
			if (s->current_line > 0 && s->script_win != NULL) {
				boxen_window_ensure_visible(s->script_win, 0,
				                            (int)(s->current_line - 1));
			}
			if (s->script_win != NULL)
				boxen_window_invalidate(s->script_win);
		}
	}

	cJSON_Delete(root);
}

/* -------------------------------------------------------------------------
 * 2026-06-07 JES Phase B.1 #691: draw callbacks.
 *
 * draw_script_pane -- renders source lines with current-line highlight.
 *
 * Current-line highlight: we draw highlighted manually (BOXEN_ATTR_REVERSE
 * on every cell of that content row) rather than calling
 * boxen_window_set_row_highlight(). Reason: set_row_highlight writes spaces
 * over cell characters (A.5 known limitation documented in boxen.h:466-482),
 * erasing the gutter marker and source text. Drawing the row manually with
 * BOXEN_ATTR_REVERSE preserves the character content.
 *
 * Gutter column (content col 0):
 *   '>' current execution line
 *   '*' line with a breakpoint
 *   ' ' otherwise
 *
 * Off-by-one (EXECUTION_PLAN.md B.1 Sentinels):
 *   ODB line numbers are 1-based; boxen content rows are 0-based.
 *   content_row = line_num - 1.
 *
 * Auto-scroll: boxen_window_ensure_visible is called here so that every
 * redraw keeps the current line visible. This covers both the suspended-
 * notification path (via write_line -> invalidate -> redraw) and any
 * explicit script pane invalidation that could move the viewport.
 * ---------------------------------------------------------------------- */

static void draw_script_pane(boxen_window_t *win, void *ud) {
	tui_state_t *s = (tui_state_t *)ud;
	int w = boxen_window_content_width(win);
	if (w <= 0) return;

	if (s == NULL || s->script_lines == NULL || s->script_line_count <= 0) {
		/* No source loaded yet -- show placeholder */
		boxen_draw_text(win, 0, 0,
		                "Script source will appear here (B.1)",
		                BOXEN_COLOR_DEFAULT, BOXEN_COLOR_DEFAULT, BOXEN_ATTR_DIM);
		return;
	}

	/* Tell boxen the total content height so it can compute scroll limits and
	 * render the scrollbar. Must be set before ensure_visible. */
	boxen_window_set_content_size(win, w, s->script_line_count);

	/* Auto-scroll: ensure the current line is visible in the viewport */
	if (s->current_line > 0) {
		boxen_window_ensure_visible(win, 0, (int)(s->current_line - 1));
	}

	/* Render each line */
	for (int i = 0; i < s->script_line_count; i++) {
		int content_row = i;      /* 0-based content row */
		long line_num   = i + 1; /* 1-based line number */

		/* Determine gutter character */
		char gutter = ' ';
		if (line_num == s->current_line) {
			gutter = '>';
		} else {
			for (int b = 0; b < s->bp_line_count; b++) {
				if (s->bp_lines[b] == (unsigned long)line_num) {
					gutter = '*';
					break;
				}
			}
		}

		bool is_current = (line_num == s->current_line);
		uint16_t attr   = is_current ? BOXEN_ATTR_REVERSE : BOXEN_ATTR_NONE;
		uint16_t fg     = BOXEN_COLOR_DEFAULT;
		uint16_t bg     = BOXEN_COLOR_DEFAULT;

		/* Draw gutter */
		boxen_set_cell(win, 0, content_row, (uint32_t)gutter, fg, bg, attr);

		/* Draw source text (truncated to available width) */
		const char *text = s->script_lines[i];
		int text_len = (int)strlen(text);
		int avail    = w - 1; /* one column reserved for gutter */
		if (avail <= 0) continue;

		/* Build a padded/truncated line buffer for uniform cell coverage */
		char linebuf[512];
		int copy = text_len < avail ? text_len : avail;
		memcpy(linebuf, text, (size_t)copy);
		/* Pad remainder with spaces so REVERSE attr covers the full row */
		memset(linebuf + copy, ' ', (size_t)(avail - copy));
		linebuf[avail] = '\0';

		boxen_draw_text(win, 1, content_row, linebuf, fg, bg, attr);
	}
}

static void draw_stack(boxen_window_t *win, void *ud) {
	(void)ud;
	int w = boxen_window_content_width(win);
	if (w <= 0) return;
	/* B.2 will render call stack and locals. B.0 shows a placeholder. */
	boxen_draw_text(win, 0, 0,
	                "Call stack / locals (B.2)",
	                BOXEN_COLOR_DEFAULT, BOXEN_COLOR_DEFAULT, BOXEN_ATTR_DIM);
}

static void draw_footer(boxen_window_t *win, void *ud) {
	(void)ud;
	int w = boxen_window_content_width(win);
	if (w <= 0) return;
	const char *text = "q:quit";
	char buf[256];
	/* Pad/truncate to content width so the bar covers the full row */
	int n = snprintf(buf, sizeof(buf), "%-*.*s", w, w, text);
	(void)n;
	boxen_draw_text(win, 0, 0, buf,
	                BOXEN_COLOR_BLACK, BOXEN_COLOR_WHITE, BOXEN_ATTR_BOLD);
}

/* -------------------------------------------------------------------------
 * 2026-06-06 JES Phase B.0 #691: layout construction.
 * 2026-06-06 JES Phase B.0 #734 round 1 P1-2: renamed to tui_build_layout;
 *   now closes any existing windows before opening new ones so it can be
 *   called on both init and BOXEN_EV_RESIZE without leaking windows.
 *
 * Three-window layout:
 *   +-- Script (60%) --------+-- Stack (40%) --+
 *   | (source; B.1)          | (stack; B.2)    |
 *   +------------------------+-----------------+
 *   q:quit                                       <- pinned footer, 1 row
 *
 * Matches the plan's "three empty windows (script, stack, footer)" spec.
 * ---------------------------------------------------------------------- */

static void tui_build_layout(tui_state_t *s, int tw, int th) {
	/* Close any windows from a previous layout (resize path) */
	if (s->footer_win != NULL) { boxen_window_close(s->footer_win); s->footer_win = NULL; }
	if (s->stack_win  != NULL) { boxen_window_close(s->stack_win);  s->stack_win  = NULL; }
	if (s->script_win != NULL) { boxen_window_close(s->script_win); s->script_win = NULL; }

	/* Reserve one row for the pinned footer */
	int content_h = (th > 2) ? (th - 1) : 1;

	boxen_rect_t full = { 0, 0, tw, content_h };

	/* Horizontal split: script (60%) | stack (40%) */
	boxen_layout_split_h(full, 0.60f,
	                     "Script", &s->script_win,
	                     "Stack",  &s->stack_win);

	/* Pinned footer: full width, 1 row, borderless */
	boxen_rect_t footer_rect = { 0, th - 1, tw, 1 };
	s->footer_win = boxen_window_open("footer", footer_rect, NULL);
	if (s->footer_win != NULL) {
		boxen_window_set_borders(s->footer_win, false);
		boxen_window_set_pinned(s->footer_win, BOXEN_PIN_BOTTOM);
		boxen_window_set_draw(s->footer_win, draw_footer);
	}
}

/* -------------------------------------------------------------------------
 * 2026-06-06 JES Phase B.0 #691: input callback (shared by all panes).
 *
 * 'q', Escape, and Ctrl-C set quit_requested. All other events are ignored
 * in B.0; keybinds for debug ops are wired in B.3.
 * ---------------------------------------------------------------------- */

static void on_input(boxen_window_t *win, const boxen_event_t *ev, void *ud) {
	(void)win;
	tui_state_t *s = (tui_state_t *)ud;
	if (ev->type != BOXEN_EV_KEY) return;

	/* 2026-06-06 JES Phase B.0 #734 round 1 P1-4: accept q/Q regardless of
	 * ev->key.key value. Real termbox2 may set key != BOXEN_KEY_NONE for
	 * printable chars on some terminals; the BOXEN_KEY_NONE guard was too
	 * strict and would have silently ignored quit on those backends. */
	if (ev->key.key == BOXEN_KEY_ESCAPE ||
	    ev->key.key == BOXEN_KEY_CTRL_C ||
	    ev->key.ch == 'q' || ev->key.ch == 'Q') {
		s->quit_requested = true;
	}
}

/* -------------------------------------------------------------------------
 * 2026-06-06 JES Phase B.0 #691: exported for unit tests (internal API).
 * ---------------------------------------------------------------------- */

/* -------------------------------------------------------------------------
 * tui_wire_callbacks -- wire draw/input callbacks after every layout build.
 *
 * Called from debugger_tui_state_init and from the resize path in
 * debugger_tui_run_one_tick. Separated so tui_build_layout stays a pure
 * window-geometry function (no callback wiring) and both call sites use the
 * same wiring logic.
 * ---------------------------------------------------------------------- */
static void tui_wire_callbacks(tui_state_t *s) {
	if (s->script_win != NULL) {
		/* 2026-06-07 JES Phase B.1 #691: real draw callback replaces placeholder */
		boxen_window_set_draw(s->script_win,  draw_script_pane);
		boxen_window_set_input(s->script_win, on_input);
		boxen_window_set_user_data(s->script_win, s);
	}
	if (s->stack_win != NULL) {
		boxen_window_set_draw(s->stack_win,  draw_stack);
		boxen_window_set_input(s->stack_win, on_input);
		boxen_window_set_user_data(s->stack_win, s);
	}
	/* footer: draw already set in tui_build_layout; no input needed */

	/* Focus the script pane */
	if (s->script_win != NULL) {
		boxen_window_focus(s->script_win);
	}
}

void debugger_tui_state_init(tui_state_t *s, int tw, int th) {
	memset(s, 0, sizeof(tui_state_t));

	/* 2026-06-07 JES Phase B.1 #691: initialize source state sentinels */
	s->current_line    = -1;  /* -1 = not suspended */
	s->pending_thread_id = -1;

	/* Allocate heap transport (stub; B.6 wires debug_set_attach_transport) */
	s->transport = calloc(1, sizeof(transport_t));
	if (s->transport != NULL) {
		s->transport->ctx        = s;
		s->transport->write_line = tui_write_line;
	}

	tui_build_layout(s, tw, th);
	tui_wire_callbacks(s);
}

void debugger_tui_state_teardown(tui_state_t *s) {
	if (s->footer_win != NULL)  { boxen_window_close(s->footer_win);  s->footer_win  = NULL; }
	if (s->stack_win  != NULL)  { boxen_window_close(s->stack_win);   s->stack_win   = NULL; }
	if (s->script_win != NULL)  { boxen_window_close(s->script_win);  s->script_win  = NULL; }
	if (s->transport  != NULL)  { free(s->transport);                 s->transport   = NULL; }
	/* 2026-06-07 JES Phase B.1 #691: free source lines */
	tui_free_script_lines(s);
}

int debugger_tui_run_one_tick(tui_state_t *s, const boxen_event_t *ev) {
	/* 2026-06-06 JES Phase B.0 #734 round 1 P1-2: handle BOXEN_EV_RESIZE.
	 * Tear down and rebuild the layout at the new dimensions, then re-wire
	 * callbacks and re-focus. boxen_dispatch_event is NOT called for resize
	 * because the layout rebuild already repositions all windows; dispatching
	 * would deliver the event to the (now-closed) old focused window. */
	if (ev->type == BOXEN_EV_RESIZE) {
		int nw = (ev->resize.w > 0) ? ev->resize.w : 80;
		int nh = (ev->resize.h > 0) ? ev->resize.h : 24;
		tui_build_layout(s, nw, nh);
		tui_wire_callbacks(s);
		return TUI_CONTINUE;
	}

	boxen_dispatch_event(ev);
	if (s->quit_requested) {
		return TUI_QUIT;
	}
	return TUI_CONTINUE;
}

/* -------------------------------------------------------------------------
 * 2026-06-06 JES Phase B.0 #691: public entry point.
 *
 * GIL yield pattern mirrors protocol_handler.c:348-354.
 * Teardown snapshot/restore mirrors protocol_handler.c:411-421.
 *
 * NOTE: B.0 does NOT call debug_set_attach_transport(). That call is added
 * in B.6 when the debug runtime is wired. The transport is allocated and
 * freed here so the heap-lifetime pattern is established from the start,
 * but the global attach is deferred per EXECUTION_PLAN.md B.0 "Out of scope".
 *
 * GIL symbol isolation:
 *   headless_threading.h is included here (inside a #ifndef guard) rather
 *   than at file scope. This keeps the test build (which compiles only
 *   debugger_tui.c without the full Frontier runtime) free of undefined GIL
 *   symbols. The test binary's undefined symbol table must not contain
 *   frontier_gil / hthreadglobals; dyld aborts at load time even if those
 *   symbols are unreachable at runtime (flat-namespace lookup failure).
 * ---------------------------------------------------------------------- */

#ifndef DEBUGGER_TUI_OMIT_MAIN

/* These includes are intentionally at function scope (not file scope) to
 * prevent the GIL and hglobals symbols from appearing in the test binary's
 * undefined symbol table. Including them here means the compiler still
 * type-checks the code; the symbols just don't appear as undefined refs in
 * the object file unless this translation unit is compiled without
 * DEBUGGER_TUI_OMIT_MAIN. */
#include "headless_threading.h"
#include <pthread.h>

int debugger_tui_main(const cli_options_t *opts) {
	(void)opts;

	/* 2026-06-06 JES Phase B.0 #734 round 1 P1-1: boxen_init MUST be called
	 * BEFORE querying terminal dimensions. termbox2's tb_width()/tb_height()
	 * return TB_ERR_NOT_INIT (negative) when called before tb_init(); the
	 * original > 0 guard silently fell back to 80x24 on every terminal. */
	const boxen_backend_t *be = boxen_tb2_backend();
	boxen_result_t rc = boxen_init(be, NULL, NULL);
	if (rc != BOXEN_OK) {
		log_error(LOG_COMP_GENERAL, "debugger_tui: boxen_init failed: %s",
		          boxen_last_error_str());
		return 1;
	}

	/* Query terminal dimensions after init -- backend is now running */
	int tw = (be->width  && be->width()  > 0) ? be->width()  : 80;
	int th = (be->height && be->height() > 0) ? be->height() : 24;

	tui_state_t state;
	debugger_tui_state_init(&state, tw, th);

	log_info(LOG_COMP_GENERAL, "Debugger TUI: entering event loop (q/Esc/Ctrl-C to quit)");

	/* Event loop: release GIL around each poll so background threads can run */
	while (!state.quit_requested) {
		boxen_event_t ev;
		memset(&ev, 0, sizeof(ev));

		/* 2026-06-06 JES Phase B.0 #691: GIL yield pattern.
		 * Snapshot main-thread globals before yielding (lazy callScript threads
		 * can overwrite hthreadglobals during the poll). Restore after.
		 * Pattern: protocol_handler.c:348-354. */
		hdlthreadglobals main_globals = hthreadglobals;
		headless_save_threadglobals(main_globals);
		pthread_mutex_unlock(&frontier_gil);
		boxen_result_t poll_rc = boxen_poll_event(&ev, 100 /* ms timeout */);
		pthread_mutex_lock(&frontier_gil);
		headless_restore_threadglobals(main_globals);

		if (poll_rc == BOXEN_ERR_TIMEOUT) {
			/* No event this tick; still call present to handle redraws */
			boxen_present();
			continue;
		}
		if (poll_rc != BOXEN_OK) {
			/* Persistent I/O error -- exit cleanly */
			log_warn(LOG_COMP_GENERAL, "debugger_tui: poll error, exiting");
			break;
		}

		debugger_tui_run_one_tick(&state, &ev);
		boxen_present();
	}

	/* 2026-06-06 JES Phase B.0 #691: teardown.
	 * B.6 inserts debug_wait_lazy_threads_drained() + debug_set_attach_transport(NULL)
	 * here per protocol_handler.c:411-421. B.0 skips those calls because no
	 * debug_set_attach_transport() was made on entry. */
	debugger_tui_state_teardown(&state);
	boxen_shutdown();

	log_info(LOG_COMP_GENERAL, "Debugger TUI: exited cleanly");
	return 0;
}

#endif /* !DEBUGGER_TUI_OMIT_MAIN */
