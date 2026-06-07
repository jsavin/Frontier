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
 * 2026-06-07 JES Phase B.3 #691: tui_debug_state_t, dispatch capture, keybind fields
 * 2026-06-07 JES Phase B.4 #691: breakpoint UI, condition modal, dispatch log
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
 * 2026-06-07 JES Phase B.3 #691: debug session state machine.
 *
 * TUI_DEBUG_IDLE      -- no debug thread attached; no step/continue keybinds active.
 * TUI_DEBUG_SUSPENDED -- a thread is suspended; step/continue keybinds are active.
 * TUI_DEBUG_RUNNING   -- thread is executing (after continue/step, before next
 *                        suspension); step/continue keybinds are disabled to prevent
 *                        double-dispatch while the thread holds the GIL.
 *
 * State transitions:
 *   IDLE       -> SUSPENDED: debug/suspended notification arrives via write_line
 *   SUSPENDED  -> RUNNING:   F5/F10/F11/Shift-F11 dispatches a debug op
 *   RUNNING    -> SUSPENDED: next debug/suspended notification arrives
 *   any        -> IDLE:      (future: debug session ends / thread killed)
 *
 * SENTINEL (EXECUTION_PLAN.md B.3): NEVER call debug/getStack or debug/getSource
 * while debug_state == TUI_DEBUG_RUNNING (thread holds GIL; ODB ops would deadlock).
 */
typedef enum {
	TUI_DEBUG_IDLE      = 0,
	TUI_DEBUG_SUSPENDED = 1,
	TUI_DEBUG_RUNNING   = 2
} tui_debug_state_t;

/*
 * TUI state -- the data shared between the event loop and draw callbacks.
 *
 * B.0: placeholder layout only.
 * B.1: script source fields added (script_path, script_lines, script_line_count,
 *      current_line, bp_lines, bp_line_count, pending_thread_id).
 * B.2: stack/locals fields added.
 * B.3: debug_state, dispatch_capture_buf/cap for keybind tests.
 */

/* Maximum number of breakpoints tracked in TUI state. */
#define TUI_MAX_BREAKPOINTS 256

/* 2026-06-07 JES Phase B.4 #691: maximum length of a conditional breakpoint
 * expression string (including NUL terminator).  256 bytes is generous for
 * a UserTalk expression; the modal input is capped at (TUI_BP_CONDITION_MAX - 1)
 * printable characters before accepting Enter. */
#define TUI_BP_CONDITION_MAX 256

/* 2026-06-07 JES Phase B.4 #691: multi-dispatch log capacity for tests.
 * Tests that verify sequences (clearBreakpoints -> setBreakpoint for each
 * surviving line) set dispatch_log to a stack array of this many slots. */
#define TUI_DISPATCH_LOG_COUNT 32

/* 2026-06-07 JES Phase B.1 #737 round 1 P1-B: cap source lines to prevent
 * a DoS via a malicious/oversized debug/getSource response.  ODB scripts are
 * typically <1000 lines; 10000 is generous and avoids the 80MB+ allocation
 * that an uncapped count from cJSON_GetArraySize() would trigger. */
#define TUI_MAX_SOURCE_LINES 10000

/* 2026-06-07 JES Phase B.2 #691: caps for frame stack and locals arrays.
 * Call stacks in UserTalk scripts are shallow (typically < 20 deep).
 * 200 frames is a generous cap that prevents malloc overflow from a malformed
 * debug/getStack response.  500 locals is similarly conservative. */
#define TUI_MAX_FRAMES  200
#define TUI_MAX_LOCALS  500

/* 2026-06-07 JES Phase B.2 #739 round 1 P2: per-local string length caps.
 * UserTalk types coerced to display form can be multi-MB (strings, RTF blobs).
 * With TUI_MAX_LOCALS=500, an uncapped strdup could drive heap to 500 * len(value).
 * Cap name at 256 bytes (dotted UT identifier budget) and value at 4096 bytes
 * (enough to show the meaningful start of any display-form value). */
#define TUI_LOCAL_NAME_MAX   256
#define TUI_LOCAL_VALUE_MAX  4096

typedef struct {
	boxen_window_t *script_win;    /* left pane: script source (B.1) */
	boxen_window_t *stack_win;     /* right pane: call stack + locals (B.2) */
	boxen_window_t *footer_win;    /* pinned bottom: keybind hints */
	transport_t    *transport;     /* heap-allocated in-process transport */
	bool            quit_requested;/* set by on_input when 'q' / Esc / Ctrl-C */

	/* 2026-06-07 JES Phase B.1 #691: script source state */
	char   script_path[256];        /* dotted path of loaded script, or "" */
	char **script_lines;            /* heap array of strdup'd line text strings */
	int    script_line_count;       /* number of entries in script_lines */
	long   current_line;            /* 1-based; -1 if not suspended */
	long   pending_thread_id;       /* thread ID from last debug/suspended, or -1 */
	unsigned long bp_lines[TUI_MAX_BREAKPOINTS]; /* 1-based line numbers with breakpoints */
	int           bp_line_count;    /* number of valid entries in bp_lines */

	/* 2026-06-07 JES Phase B.2 #691: call stack state */
	int   frame_count;                           /* number of valid frames */
	char  frame_scripts[TUI_MAX_FRAMES][256];    /* dotted script path per frame */
	long  frame_lines[TUI_MAX_FRAMES];           /* 1-based line per frame; 0 if absent */
	int   selected_frame;                        /* 0-based index; 0 = outermost */

	/* 2026-06-07 JES Phase B.2 #691: locals state (innermost suspended frame) */
	char **local_names;   /* heap array of strdup'd local variable name strings */
	char **local_values;  /* heap array of strdup'd local variable value strings */
	int    local_count;   /* number of valid entries in local_names / local_values */

	/* 2026-06-07 JES Phase B.3 #691: debug session state machine */
	tui_debug_state_t debug_state;   /* IDLE / SUSPENDED / RUNNING */

	/* 2026-06-07 JES Phase B.3 #691: dispatch capture for unit tests.
	 *
	 * In production this field is always NULL and dispatch functions call
	 * op_dispatch() directly.  In unit tests (test build omits op_dispatch),
	 * tests set dispatch_capture_buf to a stack buffer and dispatch_capture_cap
	 * to its size; the tui_do_* functions then snprintf the JSON request into
	 * that buffer instead of calling op_dispatch().  This lets tests inspect
	 * what would have been sent to the runtime without linking the full Frontier
	 * runtime.
	 *
	 * Thread safety: dispatch capture is only used from the event loop (single-
	 * threaded with GIL held); no synchronization is needed.
	 */
	char *dispatch_capture_buf;  /* NULL in production; test buffer pointer in tests */
	int   dispatch_capture_cap;  /* capacity of dispatch_capture_buf (0 if NULL) */

	/* 2026-06-07 JES Phase B.4 #691: multi-dispatch log for sequence tests.
	 *
	 * Some B.4 operations issue multiple op_dispatch calls in sequence
	 * (e.g., clearBreakpoints then setBreakpoint for each surviving line).
	 * The single dispatch_capture_buf captures only the most-recent call.
	 * Tests that need to verify a dispatch sequence set dispatch_log to a
	 * stack-allocated array of TUI_DISPATCH_LOG_COUNT fixed-size string slots,
	 * and set dispatch_log_cap to the slot count.  Each tui_dispatch_json call
	 * appends to the log (up to cap); dispatch_log_count tracks how many
	 * calls were made.  In production all three are zero/NULL.
	 *
	 * The log is secondary to dispatch_capture_buf: when dispatch_capture_buf
	 * is non-NULL, the existing single-slot capture still runs; the log records
	 * all calls in addition.  Tests can use either or both.
	 */
	char (*dispatch_log)[256]; /* NULL in production; test array in tests */
	int   dispatch_log_cap;    /* number of slots in dispatch_log (0 if NULL) */
	int   dispatch_log_count;  /* number of calls logged so far */

	/* 2026-06-07 JES Phase B.4 #691: breakpoint condition modal state.
	 *
	 * condition_modal_win -- the open modal window; NULL when no modal is active.
	 *   Heap-allocated via boxen_window_open(); closed and set to NULL when
	 *   the user confirms (Enter) or cancels (Escape).
	 *
	 * bp_condition_buf -- the condition expression being typed.
	 *   Populated one character at a time by the modal's input callback.
	 *   Bounded to TUI_BP_CONDITION_MAX - 1 printable characters.
	 *   Cleared on modal open; used to construct "condition" JSON param on confirm.
	 *
	 * bp_condition_len -- current character count in bp_condition_buf.
	 */
	boxen_window_t *condition_modal_win; /* NULL when modal is closed */
	char  bp_condition_buf[TUI_BP_CONDITION_MAX]; /* condition expression buffer */
	int   bp_condition_len;              /* current character count, 0..TUI_BP_CONDITION_MAX-1 */
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
