/*
 * boxen_repl_internal.h -- internals exposed for unit testing only.
 *
 * NEVER include this from production code outside boxen_repl.c itself.
 * Only tests/boxen_repl_tests.c and boxen_repl.c should include this header.
 *
 * The tick function, state struct, and scrollback helper are exposed here so
 * tests can drive the REPL one event at a time without calling the blocking
 * boxen_repl_main() loop.
 *
 * Pattern mirrors debugger_tui_internal.h exactly.
 *
 * 2026-06-08 JES Phase C.0 #691
 */

#ifndef BOXEN_REPL_INTERNAL_H
#define BOXEN_REPL_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>
#include "boxen/boxen.h"

/* Note: headless_threading.h (GIL symbols) is intentionally NOT included here.
 * The internal tick functions do not touch the GIL; only boxen_repl_main()
 * does, and it includes headless_threading.h locally.  This keeps the test
 * build free of GIL symbol dependencies. */

/* -------------------------------------------------------------------------
 * Return values from boxen_repl_run_one_tick()
 * ---------------------------------------------------------------------- */
#define REPL_CONTINUE  0   /* event processed; loop should keep running */
#define REPL_QUIT      1   /* user requested exit (Ctrl-C) */

/* -------------------------------------------------------------------------
 * Scrollback ring constants
 * ---------------------------------------------------------------------- */

/* 2026-06-08 JES Phase C.0 #691: ring buffer capacity.
 * 1024 lines is generous and avoids malloc overhead for each entry.
 * Tests access BOXEN_REPL_SCROLLBACK_SIZE directly for ring-overflow assertions. */
#define BOXEN_REPL_SCROLLBACK_SIZE 1024

/* Maximum length of a single scrollback line (including NUL). */
#define BOXEN_REPL_SCROLLBACK_LINE_MAX 1024

/* -------------------------------------------------------------------------
 * Input buffer constants
 * ---------------------------------------------------------------------- */

/* Maximum length of the input line buffer (including NUL). */
#define BOXEN_REPL_INPUT_MAX 1024

/* -------------------------------------------------------------------------
 * Hook function-pointer typedefs
 *
 * These seams allow tests to intercept dispatch without linking the full
 * Frontier runtime.  Production code sets them to the real implementations;
 * tests override with mocks.
 *
 * Pattern mirrors tui_state_t::odb_fetch_hook from B.8.
 * ---------------------------------------------------------------------- */

/*
 * slash_dispatch_fn_t -- process a "/" prefixed command.
 *
 * Production: boxen_repl_real_slash_dispatch (calls dispatch_slash_command)
 * Tests:      test-supplied mock
 *
 * Parameters mirror dispatch_slash_command exactly:
 *   line    -- the full input line starting with '/'
 *   running -- caller sets *running = false to signal exit
 *
 * Returns true if the REPL should keep running (same convention as
 * dispatch_slash_command).
 */
typedef bool (*slash_dispatch_fn_t)(const char *line, bool *running);

/*
 * repl_eval_fn_t -- evaluate a UserTalk expression and return result string.
 *
 * Production: boxen_repl_real_eval (calls repl_eval_script)
 * Tests:      test-supplied mock
 *
 * Parameters:
 *   expr        -- NUL-terminated expression to evaluate
 *   result_out  -- output buffer for the result string
 *   result_cap  -- capacity of result_out in bytes
 *   error_out   -- output buffer for the error string on failure
 *   error_cap   -- capacity of error_out in bytes
 *
 * Returns true on success (result_out filled), false on error (error_out filled).
 */
typedef bool (*repl_eval_fn_t)(const char *expr,
                               char *result_out, size_t result_cap,
                               char *error_out,  size_t error_cap);

/* -------------------------------------------------------------------------
 * REPL state struct
 *
 * Heap-allocated in production (boxen_repl_main).  Stack-allocated in tests.
 * All fields are zeroed on entry by boxen_repl_state_init.
 *
 * C.0 fields only.  C.1+ will add editor-window handles, etc.
 * ---------------------------------------------------------------------- */
typedef struct {
	/* Window handles */
	boxen_window_t *output_win;   /* scrollback output pane (top region) */
	boxen_window_t *input_win;    /* single-line input bar (bottom row) */
	boxen_window_t *footer_win;   /* optional hint footer (one row above input) */

	/* Input line state */
	char  input_buf[BOXEN_REPL_INPUT_MAX]; /* typed characters, NUL-terminated */
	int   input_cursor;                    /* number of characters in input_buf */

	/* Scrollback ring buffer.
	 * Ring head points at the slot for the NEXT append.
	 * scrollback_count tracks how many valid entries exist (<= BOXEN_REPL_SCROLLBACK_SIZE).
	 * Oldest entry: (head - count + SIZE) % SIZE
	 * Newest entry: (head - 1 + SIZE) % SIZE */
	char *scrollback[BOXEN_REPL_SCROLLBACK_SIZE]; /* heap-strdup'd strings */
	int   scrollback_head;                         /* next write position */
	int   scrollback_count;                        /* valid entries */

	/* Loop control */
	bool  should_quit;  /* set by Ctrl-C handler */

	/* Dispatch seams -- overridable in tests */
	slash_dispatch_fn_t slash_dispatch_hook; /* NULL -> real dispatch */
	repl_eval_fn_t      repl_eval_hook;      /* NULL -> real eval */

	/* 2026-06-08 JES #691 Phase C.0 round 2: stdout capture pipe machinery.
	 *
	 * In production (boxen_repl_main), stdout and stderr are redirected to a
	 * pipe so that slash dispatch / repl_eval output (which calls fputs/printf
	 * to stdout) is captured and routed into the scrollback ring rather than
	 * written directly to the terminal in raw mode (which would corrupt the
	 * boxen display).
	 *
	 * Invariants:
	 *   - saved_stdout / saved_stderr: dup'd originals; -1 means not saved.
	 *   - pipe_read_fd: read end of the capture pipe; -1 means no pipe.
	 *   - partial_line / partial_line_len: hold incomplete lines between drain
	 *     calls (i.e. bytes not yet terminated by '\n').
	 *
	 * Test builds (BOXEN_REPL_OMIT_MAIN): boxen_repl_main is not compiled, so
	 * the dup2 setup never runs. Tests call drain_stdout_into_scrollback
	 * directly with a test pipe fd. */
	int    saved_stdout;                         /* dup'd original STDOUT_FILENO; -1 = not saved */
	int    saved_stderr;                         /* dup'd original STDERR_FILENO; -1 = not saved */
	int    pipe_read_fd;                         /* read end of capture pipe; -1 = no pipe */
	char   partial_line[1024];                   /* incomplete line pending newline */
	size_t partial_line_len;                     /* bytes in partial_line */
} boxen_repl_state_t;

/* -------------------------------------------------------------------------
 * Internal API (used by tests and by boxen_repl.c itself)
 * ---------------------------------------------------------------------- */

/*
 * boxen_repl_state_init -- zero-fill state and build the initial layout.
 *
 * Requires boxen_init() to have been called first (same contract as
 * debugger_tui_state_init).
 *
 * Wires production hooks (boxen_repl_real_slash_dispatch,
 * boxen_repl_real_eval) unless BOXEN_REPL_OMIT_MAIN is defined (test
 * build), in which case both hooks start as NULL and tests supply mocks.
 */
void boxen_repl_state_init(boxen_repl_state_t *s, int tw, int th);

/*
 * boxen_repl_state_teardown -- close all windows and free scrollback heap.
 */
void boxen_repl_state_teardown(boxen_repl_state_t *s);

/*
 * boxen_repl_run_one_tick -- process a single boxen event.
 *
 * Returns REPL_CONTINUE or REPL_QUIT.
 */
int boxen_repl_run_one_tick(boxen_repl_state_t *s, const boxen_event_t *ev);

/*
 * boxen_repl_append_scrollback -- append one line to the ring buffer.
 *
 * Called from the dispatch paths and directly from tests (for ring-overflow
 * testing).  Drops oldest entry when the ring is full.
 *
 * Thread-safety: not thread-safe; must be called with the GIL held.
 */
void boxen_repl_append_scrollback(boxen_repl_state_t *s, const char *line);

/*
 * drain_stdout_into_scrollback -- read bytes from fd and append complete lines
 * to the scrollback ring.
 *
 * 2026-06-08 JES #691 Phase C.0 round 2: P0-2 stdout capture helper.
 *
 * Reads available bytes from fd in a loop until EAGAIN / EOF.  Splits the
 * buffered data on newlines; each complete line is appended to the scrollback
 * ring via boxen_repl_append_scrollback.  Incomplete lines (no terminating
 * '\n') are held in s->partial_line until the next drain call completes them.
 *
 * In production: called after every boxen_poll_event returns (event OR
 * timeout) to drain the stdout-capture pipe into the scrollback.
 *
 * In test builds: called directly with a test-supplied pipe fd to verify
 * the drain behavior without touching real stdout.
 *
 * Parameters:
 *   s  -- REPL state; partial_line / partial_line_len accumulate across calls
 *   fd -- read end of pipe (must be non-blocking: O_NONBLOCK set on fd)
 *
 * Thread-safety: not thread-safe; must be called with the GIL held.
 */
void drain_stdout_into_scrollback(boxen_repl_state_t *s, int fd);

/* -------------------------------------------------------------------------
 * Production hook implementations (defined inside #ifndef BOXEN_REPL_OMIT_MAIN
 * in boxen_repl.c; declared here so submit_input can call them without
 * a forward declaration inside a #ifndef block).
 *
 * NOT available in test builds (BOXEN_REPL_OMIT_MAIN is defined).
 * ---------------------------------------------------------------------- */
#ifndef BOXEN_REPL_OMIT_MAIN
bool boxen_repl_real_slash_dispatch(const char *line, bool *running);
bool boxen_repl_real_eval(const char *expr,
                          char *result_out, size_t result_cap,
                          char *error_out,  size_t error_cap);
#endif

#endif /* BOXEN_REPL_INTERNAL_H */
