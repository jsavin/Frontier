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

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* -------------------------------------------------------------------------
 * 2026-06-06 JES Phase B.0 #691: stub transport write_line.
 *
 * B.0 does not wire any debug runtime notifications. The callback accepts
 * the three-parameter signature per op_handler.h:45 -- NOT the 2-param form
 * that the handoff doc incorrectly listed. B.1 fills in the body to parse
 * debug/suspended etc. and refresh pane content.
 * ---------------------------------------------------------------------- */

static void tui_write_line(void *ctx, const char *json, size_t len) {
	/* 2026-06-06 JES Phase B.0 #691: no-op stub.
	 * B.1 parses debug/suspended, debug/completed notifications here. */
	(void)ctx;
	(void)json;
	(void)len;
}

/* -------------------------------------------------------------------------
 * 2026-06-06 JES Phase B.0 #691: draw callbacks (placeholder content).
 * ---------------------------------------------------------------------- */

static void draw_script(boxen_window_t *win, void *ud) {
	(void)ud;
	int w = boxen_window_content_width(win);
	if (w <= 0) return;
	/* B.1 will render actual source lines. B.0 shows a placeholder. */
	boxen_draw_text(win, 0, 0,
	                "Script source will appear here (B.1)",
	                BOXEN_COLOR_DEFAULT, BOXEN_COLOR_DEFAULT, BOXEN_ATTR_DIM);
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
		boxen_window_set_draw(s->script_win,  draw_script);
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
