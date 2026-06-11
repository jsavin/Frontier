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
/* op_handler.h defines transport_t; it has no runtime or GIL dependencies,
 * so it is safe to include in test builds (BOXEN_REPL_OMIT_MAIN defined). */
#include "op_handler.h"
/* 2026-06-09 JES #691 Phase C.0.2: completion popup module. */
#include "boxen_completion_popup.h"

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

/* 2026-06-10 JES #691 Phase C.0.5: multi-line accumulator capacity.
 * 8 KB covers realistic multi-line scripts while keeping the struct footprint
 * modest (the struct is heap-allocated in production and stack-allocated in
 * tests -- 8 KB is negligible in either case). */
#define BOXEN_REPL_MULTILINE_MAX 8192

/* -------------------------------------------------------------------------
 * History ring constants
 *
 * 2026-06-09 JES #691 Phase C.0.1: persistent command history.
 *
 * MUST stay byte-identical with repl.c:85-86 until linenoise REPL is removed
 * in milestone C.6 (see planning/phase_c/REPL_SCHISM_EXECUTION_PLAN.md).
 * Both REPLs read/write the same ~/.frontier_history file.
 * ---------------------------------------------------------------------- */
#define BOXEN_REPL_HISTORY_SIZE 1000

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

/*
 * repl_completion_fn_t -- provide tab-completion candidates for the current input.
 *
 * 2026-06-09 JES #691 Phase C.0.2: completion hook seam.
 * Production wires this to boxen_repl_real_completion (which calls
 * repl_complete_slash_command_path); tests inject synthetic mocks.
 *
 * Parameters:
 *   buf            -- current input buffer (NUL-terminated), cursor is at buf_len
 *   buf_len        -- number of characters typed (cursor position)
 *   candidates     -- output array; hook fills candidates[0..count-1]
 *   max_candidates -- maximum entries the hook may write
 *
 * Returns the number of candidates written (0 = no completions).
 */
typedef int (*repl_completion_fn_t)(const char *buf, size_t buf_len,
                                    char candidates[][BOXEN_COMPLETION_CANDIDATE_MAX],
                                    int max_candidates);

/*
 * 2026-06-10 JES #691 Phase C.0.3b: palette hook seams.
 *
 * Forward-declare palette_state_t as an opaque type so the test binary
 * (which does NOT link palette.c) can hold a pointer to it without
 * including palette.h.  The struct definition lives in palette.h; only
 * production code that actually calls palette_* functions needs to include
 * that header.
 *
 * palette_open_fn_t  -- called when '/' is typed at empty input.
 *   Return true on success (palette_state was populated), false on failure.
 *
 * palette_dispatch_fn_t -- called on PALETTE_DONE_EXECUTE.
 *   script_handle: the opaque script handle from palette_state.exec_script.
 *   exec_arg:      the argument string (may be empty, never NULL).
 *   Return true on successful dispatch.
 */
struct palette_state;
typedef struct palette_state palette_state_t;

/* palette_open_fn_t: called when '/' is typed at empty input.
 *   Parameter is a boxen_repl_state_t * passed as void * to avoid a circular
 *   typedef dependency (the hook type is used inside the struct definition
 *   and boxen_repl_state_t is an anonymous struct with no tag).
 *   Implementations cast to boxen_repl_state_t * before use.
 *   Returns true on success (palette was opened), false on failure. */
typedef bool (*palette_open_fn_t)(void *s);
typedef bool (*palette_dispatch_fn_t)(void *script_handle, const char *exec_arg);

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

	/*
	 * 2026-06-08 JES #691 Phase C.0 round 3 P0: heap-allocated launch transport.
	 *
	 * boxen_repl_main allocates this on the heap and stores the pointer here so
	 * boxen_repl_state_teardown can free it after debug_kill_all_threads /
	 * debug_join_all_threads have run.  NULL means no transport was allocated
	 * (auto-launch was skipped or allocation failed).
	 *
	 * Lifetime contract: the transport must outlive all joinable debug threads
	 * that were spawned with it.  debug_join_all_threads() must complete before
	 * this pointer is freed.  See debug_handler.h:196-214.
	 *
	 * In test builds the field exists but is never written by production code
	 * (BOXEN_REPL_OMIT_MAIN is defined).  The P0-fix behavioral test writes it
	 * directly to verify teardown NULLs it.
	 */
	transport_t *launch_transport;               /* heap-alloc'd; NULL if no auto-launch */

	/* 2026-06-09 JES #691 Phase C.0.1: persistent command history.
	 *
	 * Ring of past commands.  Shared on-disk format with repl.c
	 * (~/.frontier_history) so users can switch between boxen and linenoise
	 * REPLs without losing history.  See repl.c:1782+ for the merge-on-save
	 * pattern this mirrors.
	 *
	 * Sizing: BOXEN_REPL_HISTORY_SIZE * BOXEN_REPL_INPUT_MAX = ~1 MiB per
	 * state.  Acceptable for the single heap-allocated state used by
	 * boxen_repl_main; keeps the append hot path malloc-free.
	 *
	 * Navigation:
	 *   history_nav_idx == -1: input_buf shows freshly-typed input.
	 *   history_nav_idx ==  0: input_buf shows most-recent history entry.
	 *   history_nav_idx ==  N: input_buf shows the Nth-from-newest entry.
	 *
	 * Thread-safety: not thread-safe; must be accessed with the GIL held.
	 * Boxen REPL runs single-threaded; no other thread touches this struct. */
	char  history[BOXEN_REPL_HISTORY_SIZE][BOXEN_REPL_INPUT_MAX];
	int   history_count;          /* valid entries (0..BOXEN_REPL_HISTORY_SIZE) */
	int   history_head;           /* next write index */
	int   history_nav_idx;        /* -1 = not navigating; else nth-from-newest */
	char  history_saved_input[BOXEN_REPL_INPUT_MAX]; /* preserved typing during nav */

	/* 2026-06-09 JES #691 Phase C.0.2: Tab completion state.
	 *
	 * completion_popup: non-NULL while a multi-candidate popup is open.
	 *   Owned by the REPL state; closed/freed by the key handlers in on_input
	 *   and defensively in boxen_repl_state_teardown.
	 *
	 * completion_hook: function pointer called on Tab to generate candidates.
	 *   Production: boxen_repl_real_completion (set in boxen_repl_state_init
	 *   under #ifndef BOXEN_REPL_OMIT_MAIN).
	 *   Tests: inject a synthetic mock directly onto the field. */
	boxen_completion_popup_t *completion_popup;
	repl_completion_fn_t      completion_hook;

	/* 2026-06-10 JES #691 Phase C.0.3b: slash-menu palette as boxen modal.
	 *
	 * palette_state: heap-allocated when the palette modal is open; NULL
	 *   otherwise.  The pointer type is opaque (forward-declared below) so
	 *   the test binary does not need to link palette.c.
	 *
	 * palette_open_hook: called when '/' is typed at empty input.  Production
	 *   wires this to boxen_repl_real_palette_open under #ifndef
	 *   BOXEN_REPL_OMIT_MAIN.  Tests inject a synthetic mock.  NULL means
	 *   the '/' key opens no palette (existing 15 tests keep their existing
	 *   behaviour because they never install this hook).
	 *
	 * palette_dispatch_hook: called on PALETTE_DONE_EXECUTE.  Production
	 *   wires to boxen_repl_real_palette_dispatch.  Tests inject mocks. */
	palette_state_t     *palette_state;
	palette_open_fn_t    palette_open_hook;
	palette_dispatch_fn_t palette_dispatch_hook;

	/* 2026-06-10 JES #691 Phase C.0.5: multi-line input accumulator.
	 *
	 * When the user ends a line with a trailing '\', submit_input appends the
	 * line (without the backslash) plus a '\n' separator to multiline_buf and
	 * increments multiline_lines.  The next Enter without trailing '\' is the
	 * terminator: the current line is appended and the joined buffer is
	 * dispatched through the existing slash/eval path.
	 *
	 * History interaction: multi-line entries are NOT persisted to
	 * ~/.frontier_history because the on-disk format is line-oriented
	 * (one entry per fgets/fprintf line).  Embedded '\n' would corrupt both
	 * the boxen ring and the linenoise REPL's shared history file.  Deferred
	 * to a future milestone with an on-disk format upgrade.  boxen_repl_history_append
	 * is therefore skipped when multiline_lines > 0.
	 *
	 * Overflow recovery: if appending a continuation line would exceed
	 * BOXEN_REPL_MULTILINE_MAX, the trailing '\' is treated as a terminator
	 * and a warning is logged; the user sees their accumulated buffer
	 * dispatched as-is (possibly with the final line truncated to fit).
	 *
	 * Ctrl-C semantics: when multiline_lines > 0, Ctrl-C discards the
	 * accumulated buffer and returns to the single-line prompt -- it does NOT
	 * set should_quit.  The existing Ctrl-C-exits behavior is preserved for
	 * the single-line case (multiline_lines == 0).
	 *
	 * Zero-init by the existing memset in boxen_repl_state_init; no explicit
	 * initialisation needed. */
	char multiline_buf[BOXEN_REPL_MULTILINE_MAX]; /* lines joined with '\n' */
	int  multiline_len;                            /* bytes currently in multiline_buf */
	int  multiline_lines;                          /* number of continuation lines accumulated */
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

/* 2026-06-09 JES #691 Phase C.0.1: history ring + persistence API. */
void boxen_repl_history_append(boxen_repl_state_t *s, const char *line);
void boxen_repl_history_load(boxen_repl_state_t *s);
void boxen_repl_history_save(boxen_repl_state_t *s);
#ifdef BOXEN_REPL_OMIT_MAIN
/* Test-only seam: redirects ~/.frontier_history resolution to `path`.
 * Pass NULL to clear the override and revert to $HOME. */
void boxen_repl_set_history_path_for_test(const char *path);
#endif

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
 * timeout) to drain the stdout-capture pipe into the scrollback, AND once
 * more immediately after boxen_repl_run_one_tick (Phase C.0.4 post-dispatch
 * drain) so slash / eval output reaches scrollback before the next
 * boxen_present().
 *
 * In test builds: called directly with a test-supplied pipe fd to verify
 * the drain behavior without touching real stdout.
 *
 * 2026-06-10 JES #691 Phase C.0.4: async-input invariant.
 * Drain only mutates state->scrollback + state->partial_line.  It NEVER
 * touches state->input_buf or state->input_cursor, so concurrent
 * typing (or any input-bar state) is preserved across drain calls.
 * Tested by test_stdout_drain_during_typing_does_not_corrupt_input.
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
/* 2026-06-09 JES #691 Phase C.0.2: production completion implementation.
 * Mirrors linenoise_completion_callback dispatch logic from repl.c. */
int  boxen_repl_real_completion(const char *buf, size_t buf_len,
                                char candidates[][BOXEN_COMPLETION_CANDIDATE_MAX],
                                int max_candidates);
/* 2026-06-10 JES #691 Phase C.0.3b: production palette hook implementations.
 * Parameters use void * (same reason as palette_open_fn_t). */
bool boxen_repl_real_palette_open(void *s);
bool boxen_repl_real_palette_dispatch(void *script_handle, const char *exec_arg);
#endif

#ifdef BOXEN_REPL_OMIT_MAIN
/* 2026-06-10 JES #691 Phase C.0.3b: test-only helper.
 *
 * Simulates the palette close path (NULL palette_state, refocus input_win)
 * without calling palette_close or any production runtime symbols.  Tests
 * use this after asserting the palette opened to verify that subsequent
 * keystrokes reach the REPL input window. */
void boxen_repl_close_palette_for_test(void *s);
#endif

#endif /* BOXEN_REPL_INTERNAL_H */
