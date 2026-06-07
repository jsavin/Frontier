/*
 * debugger_tui_internal.h -- internals exposed for unit testing only.
 *
 * NEVER include this from production code. Only tests/debugger_tui_tests.c
 * and debugger_tui.c itself should include this header.
 *
 * The tick function and state struct are here so tests can drive the TUI
 * one event at a time without calling the blocking debugger_tui_main() loop.
 *
 * 2026-06-06 JES Phase B.0 #691
 */

#ifndef DEBUGGER_TUI_INTERNAL_H
#define DEBUGGER_TUI_INTERNAL_H

#include <stdbool.h>
#include "boxen/boxen.h"
#include "op_handler.h"

/* Note: headless_threading.h (GIL symbols) is intentionally NOT included here.
 * The internal tick functions do not touch the GIL; only debugger_tui_main()
 * does, and it includes headless_threading.h locally. This keeps the test
 * build free of GIL symbol dependencies. */

/* Return value from debugger_tui_run_one_tick() */
#define TUI_CONTINUE  0   /* event processed; loop should keep running */
#define TUI_QUIT      1   /* user requested exit ('q', Escape, Ctrl-C) */

/*
 * TUI state -- the data shared between the event loop and draw callbacks.
 *
 * B.0: placeholder layout only. Source/stack fields added in B.1/B.2.
 */
typedef struct {
	boxen_window_t *script_win;    /* left pane: script source (B.1) */
	boxen_window_t *stack_win;     /* right pane: call stack + locals (B.2) */
	boxen_window_t *footer_win;    /* pinned bottom: keybind hints */
	transport_t    *transport;     /* heap-allocated in-process transport */
	bool            quit_requested;/* set by on_input when 'q' / Esc / Ctrl-C */
} tui_state_t;

/*
 * Process one event from the boxen event queue and update state.
 *
 * Does NOT release the GIL -- the caller (debugger_tui_main) is responsible
 * for releasing before boxen_poll_event() and reacquiring after.
 *
 * Returns TUI_QUIT if the user requested exit, TUI_CONTINUE otherwise.
 */
int debugger_tui_run_one_tick(tui_state_t *s, const boxen_event_t *ev);

/*
 * Initialize a tui_state_t for testing.
 * Opens the layout windows against the currently-initialized boxen backend.
 * Caller must have called boxen_init() before calling this.
 */
void debugger_tui_state_init(tui_state_t *s, int tw, int th);

/*
 * Tear down a tui_state_t (close windows, zero the struct).
 * Caller is responsible for calling boxen_shutdown() afterwards.
 */
void debugger_tui_state_teardown(tui_state_t *s);

#endif /* DEBUGGER_TUI_INTERNAL_H */
