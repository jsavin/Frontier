/*
 * boxen_repl_tests.c -- behavioral unit tests for the C.0 boxen-native REPL.
 *
 * Uses the boxen mock backend (no real terminal) and the tick-based API
 * exposed via boxen_repl_internal.h to drive the REPL one event at a time.
 *
 * Test harness: test_report.h TR_RUN/TR_SUMMARY/TR_EXIT_CODE (same as all
 * other Frontier unit tests).
 *
 * Pattern mirrors tests/debugger_tui_tests.c exactly -- mock backend, log_write
 * stub, setup/teardown helpers, behavioral assertions.
 *
 * 2026-06-08 JES Phase C.0 #691
 */

#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

/* boxen substrate */
#include "../frontier-cli/boxen/boxen.h"
#include "../frontier-cli/boxen/backend_mock.h"

/* REPL under test */
#include "../frontier-cli/boxen_repl_internal.h"

/* Test harness */
#include "test_report.h"

/* -------------------------------------------------------------------------
 * log_write stub -- same pattern as debugger_tui_tests.c.
 *
 * boxen_repl.c calls log_warn() from hot paths.  Without a stub, log_write
 * resolves to NULL via -Wl,-undefined,dynamic_lookup and crashes at first
 * call.  A no-op stub here satisfies the linker.
 * ---------------------------------------------------------------------- */
#include "../Common/headers/logging.h"
void log_write(log_level_t level, log_component_t component,
               const char *file, int line, const char *fmt, ...) {
	(void)level; (void)component; (void)file; (void)line; (void)fmt;
	/* No-op in test builds. */
}

/* -------------------------------------------------------------------------
 * Helpers
 * ---------------------------------------------------------------------- */

#define TEST_WIDTH  80
#define TEST_HEIGHT 24

static boxen_repl_state_t g_state;

/* Captured output from hook_slash_dispatch and hook_repl_eval */
static char g_slash_result[256];
static char g_eval_result[256];

/* Seam: slash dispatch hook -- records that dispatch was called */
static bool hook_slash_dispatch(const char *line, bool *running) {
	(void)running;
	/* Copy the line so the test can assert on it */
	snprintf(g_slash_result, sizeof(g_slash_result), "SLASH:%s", line);
	/* Append a fake scrollback entry so the ring-size test can verify it */
	boxen_repl_append_scrollback(&g_state, g_slash_result);
	return true;
}

/* Seam: eval hook -- records that eval was called with the expression */
static bool hook_repl_eval(const char *expr, char *result_out,
                           size_t result_cap, char *error_out,
                           size_t error_cap) {
	(void)error_out; (void)error_cap;
	snprintf(g_eval_result, sizeof(g_eval_result), "EVAL:%s", expr);
	/* Return "2" for "1 + 1" so the behavioral test can check the value */
	if (strcmp(expr, "1 + 1") == 0) {
		snprintf(result_out, result_cap, "2");
	} else {
		snprintf(result_out, result_cap, "ok");
	}
	return true;
}

static void setup(void) {
	boxen_mock_reset(TEST_WIDTH, TEST_HEIGHT);
	boxen_init(boxen_mock_backend(), NULL, NULL);
	boxen_repl_state_init(&g_state, TEST_WIDTH, TEST_HEIGHT);

	/* Wire test seams */
	g_state.slash_dispatch_hook = hook_slash_dispatch;
	g_state.repl_eval_hook      = hook_repl_eval;

	/* Clear capture buffers */
	g_slash_result[0] = '\0';
	g_eval_result[0]  = '\0';
}

static void teardown(void) {
	boxen_repl_state_teardown(&g_state);
	boxen_shutdown();
}

/* Build a KEY event with a printable character */
static boxen_event_t make_char_event(char ch) {
	boxen_event_t ev;
	memset(&ev, 0, sizeof(ev));
	ev.type    = BOXEN_EV_KEY;
	ev.key.key = BOXEN_KEY_NONE;
	ev.key.ch  = (uint32_t)ch;
	ev.key.mod = BOXEN_MOD_NONE;
	return ev;
}

/* Build a special-key event */
static boxen_event_t make_key_event(boxen_key_t key) {
	boxen_event_t ev;
	memset(&ev, 0, sizeof(ev));
	ev.type    = BOXEN_EV_KEY;
	ev.key.key = key;
	ev.key.ch  = 0;
	ev.key.mod = BOXEN_MOD_NONE;
	return ev;
}

/* -------------------------------------------------------------------------
 * Test 1: test_input_line_accumulates_chars
 *
 * Feed printable chars 'a', 'b', 'c' to the input callback; assert the
 * state's input_buf contains "abc" and cursor position advanced.
 * ---------------------------------------------------------------------- */
static void test_input_line_accumulates_chars(void) {
	setup();

	boxen_event_t a = make_char_event('a');
	boxen_event_t b = make_char_event('b');
	boxen_event_t c = make_char_event('c');

	boxen_repl_run_one_tick(&g_state, &a);
	boxen_repl_run_one_tick(&g_state, &b);
	boxen_repl_run_one_tick(&g_state, &c);

	assert(strcmp(g_state.input_buf, "abc") == 0);
	assert(g_state.input_len == 3);
	assert(g_state.input_cursor_pos == 3);

	teardown();
}

/* -------------------------------------------------------------------------
 * Test 2: test_enter_dispatches_slash_command
 *
 * Pre-load input_buf with "/help", simulate Enter, assert the scrollback
 * ring got a new entry (hook was called).
 * ---------------------------------------------------------------------- */
static void test_enter_dispatches_slash_command(void) {
	setup();

	/* Pre-load the input buffer directly */
	strncpy(g_state.input_buf, "/help",
	        sizeof(g_state.input_buf) - 1);
	g_state.input_len        = (int)strlen(g_state.input_buf);
	g_state.input_cursor_pos = g_state.input_len;

	int ring_before = g_state.scrollback_count;

	/* Simulate Enter */
	boxen_event_t enter = make_key_event(BOXEN_KEY_ENTER);
	boxen_repl_run_one_tick(&g_state, &enter);

	/* The hook appends one entry; the dispatch path also echoes input.
	 * Either way, the ring must have grown. */
	assert(g_state.scrollback_count > ring_before);

	teardown();
}

/* -------------------------------------------------------------------------
 * Test 3: test_enter_dispatches_expression
 *
 * Pre-load input_buf with "1 + 1", simulate Enter, assert the scrollback
 * ring got an entry containing "2".
 * ---------------------------------------------------------------------- */
static void test_enter_dispatches_expression(void) {
	setup();

	strncpy(g_state.input_buf, "1 + 1",
	        sizeof(g_state.input_buf) - 1);
	g_state.input_len        = (int)strlen(g_state.input_buf);
	g_state.input_cursor_pos = g_state.input_len;

	/* Simulate Enter */
	boxen_event_t enter = make_key_event(BOXEN_KEY_ENTER);
	boxen_repl_run_one_tick(&g_state, &enter);

	/* Scan the scrollback ring for an entry containing "2" */
	bool found = false;
	for (int i = 0; i < g_state.scrollback_count && i < BOXEN_REPL_SCROLLBACK_SIZE; i++) {
		int idx = (g_state.scrollback_head - g_state.scrollback_count + i +
		           BOXEN_REPL_SCROLLBACK_SIZE) % BOXEN_REPL_SCROLLBACK_SIZE;
		if (g_state.scrollback[idx] != NULL &&
		    strstr(g_state.scrollback[idx], "2") != NULL) {
			found = true;
			break;
		}
	}
	assert(found);

	teardown();
}

/* -------------------------------------------------------------------------
 * Test 4: test_backspace_deletes_char
 *
 * Pre-load input_buf with "abcd", feed Backspace, assert "abc".
 * Feed 3 more, assert "". Feed one more, no underflow.
 * ---------------------------------------------------------------------- */
static void test_backspace_deletes_char(void) {
	setup();

	strncpy(g_state.input_buf, "abcd",
	        sizeof(g_state.input_buf) - 1);
	g_state.input_len        = 4;
	g_state.input_cursor_pos = 4;

	boxen_event_t bs = make_key_event(BOXEN_KEY_BACKSPACE);

	boxen_repl_run_one_tick(&g_state, &bs);
	assert(strcmp(g_state.input_buf, "abc") == 0);
	assert(g_state.input_len        == 3);
	assert(g_state.input_cursor_pos == 3);

	boxen_repl_run_one_tick(&g_state, &bs);
	boxen_repl_run_one_tick(&g_state, &bs);
	boxen_repl_run_one_tick(&g_state, &bs);
	assert(strcmp(g_state.input_buf, "") == 0);
	assert(g_state.input_len        == 0);
	assert(g_state.input_cursor_pos == 0);

	/* Extra backspace on empty: no underflow */
	boxen_repl_run_one_tick(&g_state, &bs);
	assert(strcmp(g_state.input_buf, "") == 0);
	assert(g_state.input_len        == 0);
	assert(g_state.input_cursor_pos == 0);

	teardown();
}

/* -------------------------------------------------------------------------
 * Test 5: test_scrollback_ring_drops_oldest_when_full
 *
 * Append BOXEN_REPL_SCROLLBACK_SIZE + 5 entries, assert the ring shows the
 * last BOXEN_REPL_SCROLLBACK_SIZE entries and the first 5 are gone.
 * ---------------------------------------------------------------------- */
static void test_scrollback_ring_drops_oldest_when_full(void) {
	setup();

	int total = BOXEN_REPL_SCROLLBACK_SIZE + 5;
	char buf[64];

	for (int i = 0; i < total; i++) {
		snprintf(buf, sizeof(buf), "line%d", i);
		boxen_repl_append_scrollback(&g_state, buf);
	}

	/* Ring must report exactly BOXEN_REPL_SCROLLBACK_SIZE entries */
	assert(g_state.scrollback_count == BOXEN_REPL_SCROLLBACK_SIZE);

	/* The oldest visible entry should be "line5" (entries 0..4 were dropped) */
	int oldest_idx = (g_state.scrollback_head - g_state.scrollback_count +
	                  BOXEN_REPL_SCROLLBACK_SIZE) % BOXEN_REPL_SCROLLBACK_SIZE;
	assert(g_state.scrollback[oldest_idx] != NULL);
	assert(strcmp(g_state.scrollback[oldest_idx], "line5") == 0);

	/* The newest entry should be the last one appended */
	int newest_idx = (g_state.scrollback_head - 1 + BOXEN_REPL_SCROLLBACK_SIZE)
	                 % BOXEN_REPL_SCROLLBACK_SIZE;
	char expected_last[64];
	snprintf(expected_last, sizeof(expected_last), "line%d", total - 1);
	assert(g_state.scrollback[newest_idx] != NULL);
	assert(strcmp(g_state.scrollback[newest_idx], expected_last) == 0);

	teardown();
}

/* -------------------------------------------------------------------------
 * Test 6: test_stdout_capture_routes_to_scrollback
 *
 * 2026-06-08 JES #691 Phase C.0 round 2 fix-loop: P0-2 TDD test.
 *
 * Verifies that drain_stdout_into_scrollback reads bytes written to a pipe
 * fd, splits on newlines, and appends each complete line to the scrollback
 * ring.  This is the behavioral contract the P0-2 stdout-capture fix must
 * satisfy.
 *
 * The test directly exercises drain_stdout_into_scrollback with a pipe pair:
 *   1. Create pipe.
 *   2. Write "captured\n" to the write end.
 *   3. Call drain_stdout_into_scrollback(state, pipefd[0]).
 *   4. Assert scrollback ring has an entry containing "captured".
 *
 * In test builds (BOXEN_REPL_OMIT_MAIN defined), the production code does
 * not set up the stdout dup2 path -- tests must NOT have their own stdout
 * hijacked by default.  Only the drain helper is called here.
 * ---------------------------------------------------------------------- */
#include <unistd.h>   /* pipe, write, close, getpid */
#include <fcntl.h>    /* fcntl, F_SETFL, O_NONBLOCK */
#include <sys/stat.h> /* stat, st_mode -- 2026-06-09 JES #691 Phase C.0.1 */

static void test_stdout_capture_routes_to_scrollback(void) {
	setup();

	int pipefd[2];
	int rc = pipe(pipefd);
	assert(rc == 0);

	/* Set read end non-blocking so drain_stdout_into_scrollback doesn't block */
	fcntl(pipefd[0], F_SETFL, O_NONBLOCK);

	/* Write a line to the write end (simulates stdout output from slash dispatch) */
	const char *msg = "captured\n";
	ssize_t written = write(pipefd[1], msg, strlen(msg));
	assert(written == (ssize_t)strlen(msg));
	close(pipefd[1]);

	int ring_before = g_state.scrollback_count;

	/* Call the drain helper -- should read "captured\n" and append "captured"
	 * to scrollback */
	drain_stdout_into_scrollback(&g_state, pipefd[0]);
	close(pipefd[0]);

	/* Ring must have grown */
	assert(g_state.scrollback_count > ring_before);

	/* Find "captured" in the ring */
	bool found = false;
	for (int i = 0; i < g_state.scrollback_count && i < BOXEN_REPL_SCROLLBACK_SIZE; i++) {
		int idx = (g_state.scrollback_head - g_state.scrollback_count + i +
		           BOXEN_REPL_SCROLLBACK_SIZE) % BOXEN_REPL_SCROLLBACK_SIZE;
		if (g_state.scrollback[idx] != NULL &&
		    strstr(g_state.scrollback[idx], "captured") != NULL) {
			found = true;
			break;
		}
	}
	assert(found);

	teardown();
}

/* -------------------------------------------------------------------------
 * Test 7: test_launch_transport_heap_alloc_and_teardown
 *
 * 2026-06-08 JES #691 Phase C.0 round 3: P0 UAF fix -- TDD guard.
 *
 * Verifies that boxen_repl_state_t carries a launch_transport pointer field,
 * and that after boxen_repl_state_teardown() the field is NULL (i.e., any
 * heap-allocated transport was freed and the pointer cleared).
 *
 * This test targets the state-machine contract only -- it does NOT call
 * boxen_repl_main (which requires GIL + debug_handler symbols absent in the
 * test build).  It exercises boxen_repl_state_init + a manual field write to
 * simulate what boxen_repl_main does, then calls teardown to confirm cleanup.
 *
 * RED expectation before fix: compile error (no launch_transport field) OR
 * teardown does not NULL the field (assert fires).
 * ---------------------------------------------------------------------- */
static void test_launch_transport_heap_alloc_and_teardown(void) {
	setup();

	/* Simulate what boxen_repl_main does: heap-allocate a transport and
	 * store it in state->launch_transport.  In production, boxen_repl_main
	 * does this; here we do it directly so teardown can be tested in isolation
	 * without the production dependencies. */
	transport_t *lt = (transport_t *)calloc(1, sizeof(transport_t));
	assert(lt != NULL);
	g_state.launch_transport = lt;

	/* teardown must free the transport and NULL the field. */
	boxen_repl_state_teardown(&g_state);

	assert(g_state.launch_transport == NULL);

	/* boxen_shutdown to pair with setup's boxen_init (teardown already called,
	 * but we skip the duplicate teardown since it was already called above). */
	boxen_shutdown();
}

/* -------------------------------------------------------------------------
 * History test helpers
 * 2026-06-09 JES #691 Phase C.0.1
 * ---------------------------------------------------------------------- */
static boxen_event_t make_up_event(void)   { return make_key_event(BOXEN_KEY_UP); }
static boxen_event_t make_down_event(void) { return make_key_event(BOXEN_KEY_DOWN); }

static char g_tmp_history_path[256];
static void make_tmp_history_path(void) {
	snprintf(g_tmp_history_path, sizeof(g_tmp_history_path),
	         "/tmp/boxen_repl_test_history_%d.txt", (int)getpid());
}

/* -------------------------------------------------------------------------
 * Test 8: test_history_up_arrow_loads_previous
 *
 * 2026-06-09 JES #691 Phase C.0.1: history ring navigation.
 *
 * Inject three entries into the history ring directly (oldest -> newest:
 * "cmd_a", "cmd_b", "cmd_c").  Then simulate:
 *   1. UP: input_buf shows "cmd_c" (most recent), nav_idx = 0.
 *   2. UP: input_buf shows "cmd_b", nav_idx = 1.
 *   3. UP: input_buf shows "cmd_a" (oldest), nav_idx = 2.
 *   4. UP again: stays at "cmd_a" (clamp at oldest).
 *   5. DOWN: input_buf shows "cmd_b", nav_idx = 1.
 *   6. DOWN: input_buf shows "cmd_c", nav_idx = 0.
 *   7. DOWN: input_buf restored to saved typing, nav_idx = -1.
 * ---------------------------------------------------------------------- */
static void test_history_up_arrow_loads_previous(void) {
	/* Point history at a nonexistent file so state_init's load is a no-op.
	 * Without this, a real ~/.frontier_history would shift history_head and
	 * break the ring-index assertions below. */
	boxen_repl_set_history_path_for_test("/tmp/boxen_repl_test_nosuchfile_nav.txt");
	setup();

	/* Pre-load 3 commands into the ring by calling the append function. */
	boxen_repl_history_append(&g_state, "cmd_a");
	boxen_repl_history_append(&g_state, "cmd_b");
	boxen_repl_history_append(&g_state, "cmd_c");

	/* Simulate user having typed "partial" into the input bar */
	strncpy(g_state.input_buf, "partial", sizeof(g_state.input_buf) - 1);
	g_state.input_len        = (int)strlen("partial");
	g_state.input_cursor_pos = g_state.input_len;

	boxen_event_t up   = make_up_event();
	boxen_event_t down = make_down_event();

	/* UP x1: most recent = cmd_c */
	boxen_repl_run_one_tick(&g_state, &up);
	assert(strcmp(g_state.input_buf, "cmd_c") == 0);
	assert(g_state.history_nav_idx == 0);

	/* UP x2: cmd_b */
	boxen_repl_run_one_tick(&g_state, &up);
	assert(strcmp(g_state.input_buf, "cmd_b") == 0);
	assert(g_state.history_nav_idx == 1);

	/* UP x3: cmd_a (oldest) */
	boxen_repl_run_one_tick(&g_state, &up);
	assert(strcmp(g_state.input_buf, "cmd_a") == 0);
	assert(g_state.history_nav_idx == 2);

	/* UP x4: clamp at oldest -- still cmd_a */
	boxen_repl_run_one_tick(&g_state, &up);
	assert(strcmp(g_state.input_buf, "cmd_a") == 0);
	assert(g_state.history_nav_idx == 2);

	/* DOWN x1: cmd_b */
	boxen_repl_run_one_tick(&g_state, &down);
	assert(strcmp(g_state.input_buf, "cmd_b") == 0);
	assert(g_state.history_nav_idx == 1);

	/* DOWN x2: cmd_c */
	boxen_repl_run_one_tick(&g_state, &down);
	assert(strcmp(g_state.input_buf, "cmd_c") == 0);
	assert(g_state.history_nav_idx == 0);

	/* DOWN x3: restore saved typing */
	boxen_repl_run_one_tick(&g_state, &down);
	assert(strcmp(g_state.input_buf, "partial") == 0);
	assert(g_state.history_nav_idx == -1);

	boxen_repl_set_history_path_for_test(NULL);  /* clear override */
	teardown();
}

/* -------------------------------------------------------------------------
 * Test 9: test_history_persists_via_file_roundtrip
 *
 * 2026-06-09 JES #691 Phase C.0.1: on-disk persistence.
 *
 * 1. Write a file with "older1\nolder2\n" to g_tmp_history_path.
 * 2. Point the history override seam at that path.
 * 3. Call boxen_repl_history_load: ring should contain older1 then older2.
 * 4. Append "session_cmd" to the ring.
 * 5. Call boxen_repl_history_save: merged file written.
 * 6. Re-read the file and assert it contains "older1", "older2",
 *    "session_cmd" in that order (older entries first, session entry last).
 * 7. Clean up temp file.
 * ---------------------------------------------------------------------- */
static void test_history_persists_via_file_roundtrip(void) {
	make_tmp_history_path();
	/* Remove any leftover from a prior run */
	(void)remove(g_tmp_history_path);

	/* Step 1: write pre-existing history file */
	FILE *fout = fopen(g_tmp_history_path, "w");
	assert(fout != NULL);
	fprintf(fout, "older1\n");
	fprintf(fout, "older2\n");
	fclose(fout);

	/* Step 2: redirect history path to temp file BEFORE setup() so that
	 * boxen_repl_state_init's load reads from our controlled file (not
	 * from the real ~/.frontier_history, which would give unpredictable
	 * history_count). */
	boxen_repl_set_history_path_for_test(g_tmp_history_path);

	setup();

	/* Step 3: after setup, ring should have 2 entries from the file */
	assert(g_state.history_count == 2);

	/* Step 4: append a session command */
	boxen_repl_history_append(&g_state, "session_cmd");
	assert(g_state.history_count == 3);

	/* Step 5: save -- merge and write back */
	boxen_repl_history_save(&g_state);

	/* Step 5a: verify file was created with mode 0600 (P1-1 regression guard).
	 * 2026-06-09 JES #691 Phase C.0.1 */
	{
		struct stat st;
		assert(stat(g_tmp_history_path, &st) == 0);
		assert((st.st_mode & 0777) == 0600);
	}

	/* Step 6: re-read and verify order: older1, older2, session_cmd */
	FILE *fin = fopen(g_tmp_history_path, "r");
	assert(fin != NULL);

	char lines[10][256];
	int  line_count = 0;
	while (line_count < 10 && fgets(lines[line_count], sizeof(lines[0]), fin)) {
		/* Strip trailing newline */
		size_t len = strlen(lines[line_count]);
		if (len > 0 && lines[line_count][len - 1] == '\n') {
			lines[line_count][len - 1] = '\0';
		}
		line_count++;
	}
	fclose(fin);

	assert(line_count == 3);
	assert(strcmp(lines[0], "older1")      == 0);
	assert(strcmp(lines[1], "older2")      == 0);
	assert(strcmp(lines[2], "session_cmd") == 0);

	/* Step 7: clear override + clean up */
	boxen_repl_set_history_path_for_test(NULL);
	(void)remove(g_tmp_history_path);

	teardown();
}

/* -------------------------------------------------------------------------
 * Test 10: test_history_load_drops_overlong_lines
 *
 * 2026-06-09 JES #691 Phase C.0.1 P1-2 regression guard.
 *
 * A 2KB entry written by linenoise exceeds BOXEN_REPL_INPUT_MAX (1024).
 * Without the fix, fgets would split it into two ring entries and both
 * would be saved back, permanently corrupting the shared history file.
 * With the fix, overlong lines are drained and skipped entirely.
 *
 * This test writes a file with:
 *   - one 2048-char line  (overlong: must be dropped)
 *   - one normal "ok_cmd" (must be kept)
 *
 * After load: history_count == 1 and the entry is "ok_cmd".
 * ---------------------------------------------------------------------- */
static void test_history_load_drops_overlong_lines(void) {
	char tmp_path[256];
	snprintf(tmp_path, sizeof(tmp_path),
	         "/tmp/boxen_repl_test_overlong_%d.txt", (int)getpid());
	(void)remove(tmp_path);

	FILE *fout = fopen(tmp_path, "w");
	assert(fout != NULL);
	/* Write a 2048-char line (no newline until the very end). */
	for (int i = 0; i < 2048; i++) fputc('x', fout);
	fputc('\n', fout);
	/* Write a normal command that must survive. */
	fprintf(fout, "ok_cmd\n");
	fclose(fout);

	boxen_repl_set_history_path_for_test(tmp_path);
	setup();

	/* Overlong line dropped; only "ok_cmd" loaded. */
	assert(g_state.history_count == 1);
	/* The one entry in the ring must be "ok_cmd". */
	int idx = (g_state.history_head - 1 + BOXEN_REPL_HISTORY_SIZE)
	          % BOXEN_REPL_HISTORY_SIZE;
	assert(strcmp(g_state.history[idx], "ok_cmd") == 0);

	boxen_repl_set_history_path_for_test(NULL);
	(void)remove(tmp_path);
	teardown();
}

/* -------------------------------------------------------------------------
 * Test 11 (P0 regression guard): test_history_down_arrow_preserves_input_when_empty
 *
 * 2026-06-09 JES #691 Phase C.0.1 P0 regression guard.
 *
 * First-run scenario: no history file, so history_load is a no-op and
 * history_count == 0.  Before the fix, boxen_repl_state_init left
 * history_nav_idx == 0 (memset zero) instead of -1.  Pressing DOWN
 * before any append caused history_nav_down to skip its early-return
 * guard (nav_idx == 0, not -1), take the "restore saved input" branch,
 * copy history_saved_input (all zeros from memset) into input_buf, and
 * wipe whatever the user had typed.
 *
 * This test verifies:
 *   1. After init with no history file, history_nav_idx == -1.
 *   2. Typing "hello" works normally.
 *   3. Pressing DOWN does NOT clear input_buf.
 *   4. history_nav_idx remains -1 after the spurious DOWN.
 * ---------------------------------------------------------------------- */
static void test_history_down_arrow_preserves_input_when_empty(void) {
	/* Point at a file that does not exist -- load is a silent no-op. */
	boxen_repl_set_history_path_for_test(
	    "/tmp/boxen_repl_test_nosuchfile_p0_down.txt");
	setup();

	/* After init with no history file: count == 0, nav_idx must be -1. */
	assert(g_state.history_count == 0);
	assert(g_state.history_nav_idx == -1);

	/* Type "hello" one char at a time. */
	const char *word = "hello";
	for (const char *p = word; *p != '\0'; p++) {
		boxen_event_t ev = make_char_event(*p);
		boxen_repl_run_one_tick(&g_state, &ev);
	}
	assert(strcmp(g_state.input_buf, "hello") == 0);

	/* Press DOWN -- must be a no-op when history is empty and nav == -1. */
	boxen_event_t down = make_down_event();
	boxen_repl_run_one_tick(&g_state, &down);

	/* Input must be preserved. */
	assert(strcmp(g_state.input_buf, "hello") == 0);
	assert(g_state.history_nav_idx == -1);

	boxen_repl_set_history_path_for_test(NULL);
	teardown();
}

/* -------------------------------------------------------------------------
 * 2026-06-09 JES #691 Phase C.0.2: completion mock hooks.
 *
 * Three static hooks that inject synthetic completion candidates so the
 * completion tests never need to link the full Frontier ODB runtime.
 *
 * Signature matches repl_completion_fn_t:
 *   int (*)(const char *buf, size_t buf_len,
 *           char candidates[][BOXEN_COMPLETION_CANDIDATE_MAX], int max_candidates)
 * ---------------------------------------------------------------------- */
#include "../frontier-cli/boxen_completion_popup.h"

static int hook_completion_single(const char *buf, size_t bl,
                                  char cand[][BOXEN_COMPLETION_CANDIDATE_MAX], int max) {
	(void)bl; (void)max;
	if (strncmp(buf, "/h", 2) == 0) {
		strcpy(cand[0], "/help");
		return 1;
	}
	return 0;
}

static int hook_completion_multi(const char *buf, size_t bl,
                                 char cand[][BOXEN_COMPLETION_CANDIDATE_MAX], int max) {
	(void)bl; (void)max;
	if (strcmp(buf, "/") == 0) {
		strcpy(cand[0], "/help");
		strcpy(cand[1], "/jump");
		strcpy(cand[2], "/list");
		return 3;
	}
	return 0;
}

static int hook_completion_odb(const char *buf, size_t bl,
                               char cand[][BOXEN_COMPLETION_CANDIDATE_MAX], int max) {
	(void)bl; (void)max;
	if (strncmp(buf, "system.te", 9) == 0) {
		strcpy(cand[0], "system.test");
		return 1;
	}
	return 0;
}

/* -------------------------------------------------------------------------
 * Test 12: test_tab_completes_single_slash_command
 *
 * 2026-06-09 JES #691 Phase C.0.2.
 *
 * Type "/h", press Tab with hook_completion_single wired.  The hook
 * returns exactly one candidate ("/help"), so the REPL should replace
 * input_buf inline (no popup opened).
 * ---------------------------------------------------------------------- */
static void test_tab_completes_single_slash_command(void) {
	setup();
	g_state.completion_hook = hook_completion_single;

	/* Type "/h" */
	boxen_event_t ev_slash = make_char_event('/');
	boxen_event_t ev_h     = make_char_event('h');
	boxen_repl_run_one_tick(&g_state, &ev_slash);
	boxen_repl_run_one_tick(&g_state, &ev_h);
	assert(strcmp(g_state.input_buf, "/h") == 0);

	/* Press Tab */
	boxen_event_t ev_tab = make_key_event(BOXEN_KEY_TAB);
	boxen_repl_run_one_tick(&g_state, &ev_tab);

	/* Single candidate: inline replace, no popup. */
	assert(strcmp(g_state.input_buf, "/help") == 0);
	assert(g_state.input_len        == 5);
	assert(g_state.input_cursor_pos == 5);
	assert(g_state.completion_popup == NULL);

	teardown();
}

/* -------------------------------------------------------------------------
 * Test 13: test_tab_opens_popup_with_multiple_candidates
 *
 * 2026-06-09 JES #691 Phase C.0.2.
 *
 * Type "/", press Tab with hook_completion_multi wired.  The hook returns
 * 3 candidates, so a completion popup should be open.  input_buf stays "/".
 * ---------------------------------------------------------------------- */
static void test_tab_opens_popup_with_multiple_candidates(void) {
	setup();
	g_state.completion_hook = hook_completion_multi;

	/* Type "/" */
	boxen_event_t ev_slash = make_char_event('/');
	boxen_repl_run_one_tick(&g_state, &ev_slash);
	assert(strcmp(g_state.input_buf, "/") == 0);

	/* Press Tab */
	boxen_event_t ev_tab = make_key_event(BOXEN_KEY_TAB);
	boxen_repl_run_one_tick(&g_state, &ev_tab);

	/* Multiple candidates: popup should be open. */
	assert(g_state.completion_popup != NULL);
	assert(boxen_completion_popup_is_open(g_state.completion_popup));
	assert(boxen_completion_popup_count(g_state.completion_popup) == 3);

	/* input_buf should be unchanged. */
	assert(strcmp(g_state.input_buf, "/") == 0);

	/* Close popup to avoid leak in teardown */
	boxen_completion_popup_close(g_state.completion_popup);
	g_state.completion_popup = NULL;

	teardown();
}

/* -------------------------------------------------------------------------
 * Test 14: test_tab_completes_odb_path_prefix
 *
 * 2026-06-09 JES #691 Phase C.0.2.
 *
 * Type "system.te", press Tab with hook_completion_odb wired.  Single
 * candidate "system.test" -> inline replace, no popup.
 * ---------------------------------------------------------------------- */
static void test_tab_completes_odb_path_prefix(void) {
	setup();
	g_state.completion_hook = hook_completion_odb;

	/* Type "system.te" character by character */
	const char *prefix = "system.te";
	for (const char *p = prefix; *p != '\0'; p++) {
		boxen_event_t ev = make_char_event(*p);
		boxen_repl_run_one_tick(&g_state, &ev);
	}
	assert(strcmp(g_state.input_buf, "system.te") == 0);

	/* Press Tab */
	boxen_event_t ev_tab = make_key_event(BOXEN_KEY_TAB);
	boxen_repl_run_one_tick(&g_state, &ev_tab);

	/* Single candidate: inline replace, no popup. */
	assert(strcmp(g_state.input_buf, "system.test") == 0);
	assert(g_state.input_len        == 11);
	assert(g_state.input_cursor_pos == 11);
	assert(g_state.completion_popup == NULL);

	teardown();
}

/* -------------------------------------------------------------------------
 * Test 15: test_escape_dismisses_popup_without_accept
 *
 * 2026-06-09 JES #691 Phase C.0.2.
 *
 * Open a multi-candidate popup (same as test 13), then press Escape.
 * The popup should close and input_buf should remain "/".
 * ---------------------------------------------------------------------- */
static void test_escape_dismisses_popup_without_accept(void) {
	setup();
	g_state.completion_hook = hook_completion_multi;

	/* Type "/" and Tab to open the popup */
	boxen_event_t ev_slash = make_char_event('/');
	boxen_event_t ev_tab   = make_key_event(BOXEN_KEY_TAB);
	boxen_repl_run_one_tick(&g_state, &ev_slash);
	boxen_repl_run_one_tick(&g_state, &ev_tab);

	/* Popup must be open first */
	assert(g_state.completion_popup != NULL);

	/* Press Escape while popup open */
	boxen_event_t ev_esc = make_key_event(BOXEN_KEY_ESCAPE);
	boxen_repl_run_one_tick(&g_state, &ev_esc);

	/* Popup should be closed */
	assert(g_state.completion_popup == NULL);

	/* input_buf must still be "/" */
	assert(strcmp(g_state.input_buf, "/") == 0);

	teardown();
}

/* -------------------------------------------------------------------------
 * 2026-06-17 JES #691 Phase C.0.7a: mock time source for slash debounce tests.
 *
 * g_mock_now_ms is a simple counter incremented explicitly by tests.
 * Inject via g_state.now_ms_hook = mock_now_ms to give the debounce logic
 * a deterministic clock.
 *
 * Tests that do NOT set now_ms_hook leave it NULL (= real clock), which is
 * fine for non-debounce behavior.
 * ---------------------------------------------------------------------- */
static uint64_t g_mock_now_ms;

static uint64_t mock_now_ms(void) {
	return g_mock_now_ms;
}

/* -------------------------------------------------------------------------
 * Test 16: test_slash_key_defers_palette_open
 *
 * 2026-06-17 JES #691 Phase C.0.7a: '/' at empty input DEFERS palette open.
 *
 * Previously (C.0.3b) typing '/' opened the palette immediately.  The
 * debounce fix changes the contract: after '/', palette_state is still NULL
 * (not opened yet) but slash_pending_until_ms is non-zero (debounce armed).
 * The palette fires only after the debounce window elapses (tested separately
 * in test_slash_then_timeout_opens_palette).
 *
 * Asserts:
 *   - palette_state is NULL immediately after '/' (NOT opened yet)
 *   - mock_palette_open_count is 0
 *   - input_buf is empty ('/' was NOT inserted)
 *   - input_cursor is 0
 *   - slash_pending_until_ms is non-zero (debounce armed)
 * ---------------------------------------------------------------------- */

/* Capture counter for mock palette open calls. */
static int g_mock_palette_open_count;

static bool mock_palette_open(void *s_opaque) {
	boxen_repl_state_t *s = (boxen_repl_state_t *)s_opaque;
	g_mock_palette_open_count++;
	/* Install a sentinel -- tests never dereference this pointer. */
	s->palette_state = (palette_state_t *)0x1;
	return true;
}

static bool mock_palette_dispatch(void *script_handle, const char *exec_arg) {
	(void)script_handle;
	(void)exec_arg;
	return true;
}

/* Captured args from the last mock_palette_dispatch call. */
static void  *g_captured_script_handle;
static char   g_captured_exec_arg[256];

static bool mock_palette_dispatch_capture(void *script_handle,
                                          const char *exec_arg) {
	g_captured_script_handle = script_handle;
	snprintf(g_captured_exec_arg, sizeof(g_captured_exec_arg),
	         "%s", exec_arg ? exec_arg : "");
	return true;
}

static void test_slash_key_defers_palette_open(void) {
	setup();

	g_mock_now_ms             = 1000;  /* arbitrary start time */
	g_mock_palette_open_count = 0;
	g_state.palette_open_hook     = mock_palette_open;
	g_state.palette_dispatch_hook = mock_palette_dispatch;
	g_state.now_ms_hook           = mock_now_ms;

	/* Type '/' at empty input. */
	boxen_event_t ev = make_char_event('/');
	boxen_repl_run_one_tick(&g_state, &ev);

	/* Palette must NOT have opened immediately (debounce in progress). */
	assert(g_mock_palette_open_count == 0);
	assert(g_state.palette_state == NULL);
	/* '/' must NOT have been inserted into input_buf. */
	assert(g_state.input_buf[0] == '\0');
	assert(g_state.input_len        == 0);
	assert(g_state.input_cursor_pos == 0);
	/* Debounce must be armed. */
	assert(g_state.slash_pending_until_ms != 0);

	/* Clean up sentinel before teardown (teardown would try to call
	 * palette_close if palette_state != NULL and the production guard
	 * is compiled in; in the test build BOXEN_REPL_OMIT_MAIN is defined
	 * so the guard body is skipped, but clear it anyway to stay clean). */
	g_state.palette_state = NULL;

	teardown();
}

/* -------------------------------------------------------------------------
 * Test 17: test_palette_close_returns_focus_to_repl_input
 *
 * 2026-06-10 JES #691 Phase C.0.3b: after palette closes, input events go
 * back to the REPL input window.
 *
 * 2026-06-17 JES #691 Phase C.0.7a: updated to advance mock time past the
 * debounce window before asserting palette_state != NULL, since '/' now
 * defers the open rather than firing immediately.
 * ---------------------------------------------------------------------- */
static void test_palette_close_returns_focus_to_repl_input(void) {
	setup();

	g_mock_now_ms             = 1000;
	g_mock_palette_open_count = 0;
	g_state.palette_open_hook     = mock_palette_open;
	g_state.palette_dispatch_hook = mock_palette_dispatch;
	g_state.now_ms_hook           = mock_now_ms;

	/* Type '/' -- this arms the debounce, does NOT open palette yet. */
	boxen_event_t ev_slash = make_char_event('/');
	boxen_repl_run_one_tick(&g_state, &ev_slash);
	assert(g_state.palette_state == NULL);  /* still pending */

	/* Advance mock time past the debounce window. */
	g_mock_now_ms += BOXEN_REPL_SLASH_DEBOUNCE_MS + 1;
	boxen_repl_check_pending_slash(&g_state);

	/* Now the palette must be open. */
	assert(g_state.palette_state != NULL);

	/* Simulate close from outside (production path: backend->close + refocus). */
	boxen_repl_close_palette_for_test(&g_state);
	assert(g_state.palette_state == NULL);

	/* Now a printable key must land in input_buf. */
	boxen_event_t ev_a = make_char_event('a');
	boxen_repl_run_one_tick(&g_state, &ev_a);
	assert(strcmp(g_state.input_buf, "a") == 0);
	assert(g_state.input_len        == 1);
	assert(g_state.input_cursor_pos == 1);

	teardown();
}

/* -------------------------------------------------------------------------
 * Test 18: test_palette_dispatch_hook_contract
 *
 * 2026-06-10 JES #691 Phase C.0.3b: validate the dispatch hook signature
 * contract (script_handle + exec_arg round-trip).
 *
 * Directly invoke the dispatch hook with fixture args and assert the
 * captures match.  This validates the contract shape that production code
 * must preserve when wiring boxen_repl_real_palette_dispatch.
 * ---------------------------------------------------------------------- */
static void test_palette_dispatch_hook_contract(void) {
	setup();

	g_state.palette_dispatch_hook = mock_palette_dispatch_capture;
	g_captured_script_handle      = NULL;
	g_captured_exec_arg[0]        = '\0';

	/* Direct invocation: simulate what on_palette_done calls on DONE_EXECUTE. */
	void *fixture_handle = (void *)0xABCD;
	const char *fixture_arg = "fixture-arg";
	g_state.palette_dispatch_hook(fixture_handle, fixture_arg);

	assert(g_captured_script_handle == fixture_handle);
	assert(strcmp(g_captured_exec_arg, "fixture-arg") == 0);

	teardown();
}

/* -------------------------------------------------------------------------
 * Test 19: test_slash_mid_input_still_inserts
 *
 * 2026-06-10 JES #691 Phase C.0.3b: '/' mid-input (cursor != 0) must
 * insert normally; palette must NOT open.
 * ---------------------------------------------------------------------- */
static void test_slash_mid_input_still_inserts(void) {
	setup();

	g_mock_palette_open_count = 0;
	g_state.palette_open_hook     = mock_palette_open;
	g_state.palette_dispatch_hook = mock_palette_dispatch;

	/* Type 'abc' first. */
	boxen_event_t ev_a = make_char_event('a');
	boxen_event_t ev_b = make_char_event('b');
	boxen_event_t ev_c = make_char_event('c');
	boxen_repl_run_one_tick(&g_state, &ev_a);
	boxen_repl_run_one_tick(&g_state, &ev_b);
	boxen_repl_run_one_tick(&g_state, &ev_c);
	assert(strcmp(g_state.input_buf, "abc") == 0);

	/* Now type '/'. cursor != 0, so palette must NOT open. */
	boxen_event_t ev_slash = make_char_event('/');
	boxen_repl_run_one_tick(&g_state, &ev_slash);

	assert(strcmp(g_state.input_buf, "abc/") == 0);
	assert(g_state.palette_state == NULL);
	assert(g_mock_palette_open_count == 0);

	teardown();
}

/* -------------------------------------------------------------------------
 * test_stdout_drain_during_typing_does_not_corrupt_input
 *
 * 2026-06-10 JES #691 Phase C.0.4: regression guard for async-input invariant.
 *
 * Verifies that calling drain_stdout_into_scrollback() while the input bar
 * holds in-progress typed characters leaves input_buf and input_cursor
 * completely untouched.  Also confirms that scrollback_count grows by exactly
 * one (the single "async-msg\n" line written to the test pipe).
 * ---------------------------------------------------------------------- */
static void test_stdout_drain_during_typing_does_not_corrupt_input(void) {
	setup();

	/* Type "abc" into the input bar */
	boxen_event_t a = make_char_event('a');
	boxen_event_t b = make_char_event('b');
	boxen_event_t c = make_char_event('c');
	boxen_repl_run_one_tick(&g_state, &a);
	boxen_repl_run_one_tick(&g_state, &b);
	boxen_repl_run_one_tick(&g_state, &c);

	/* Create a test pipe, write one async line, then drain it */
	int pipefd[2];
	assert(pipe(pipefd) == 0);
	/* 2026-06-10 JES #691 Phase C.0.4: assert fcntl so a silent O_NONBLOCK
	 * failure surfaces as a clear assertion rather than a read() hang. */
	assert(fcntl(pipefd[0], F_SETFL, O_NONBLOCK) == 0);
	const char *msg = "async-msg\n";
	ssize_t w = write(pipefd[1], msg, strlen(msg));
	assert(w == (ssize_t)strlen(msg));
	close(pipefd[1]);

	int ring_before = g_state.scrollback_count;

	drain_stdout_into_scrollback(&g_state, pipefd[0]);
	close(pipefd[0]);

	/* Input bar must be completely untouched */
	assert(strcmp(g_state.input_buf, "abc") == 0);
	assert(g_state.input_len        == 3);
	assert(g_state.input_cursor_pos == 3);

	/* Scrollback grew by exactly one line */
	assert(g_state.scrollback_count == ring_before + 1);
	int newest = (g_state.scrollback_head - 1 + BOXEN_REPL_SCROLLBACK_SIZE) %
	             BOXEN_REPL_SCROLLBACK_SIZE;
	assert(g_state.scrollback[newest] != NULL);
	assert(strstr(g_state.scrollback[newest], "async-msg") != NULL);

	teardown();
}

/* -------------------------------------------------------------------------
 * test_long_output_does_not_deadlock_event_loop
 *
 * 2026-06-10 JES #691 Phase C.0.4: regression guard for no-hang invariant.
 *
 * Writes >100 KB to a non-blocking pipe and drains on backpressure.  Proves
 * that the consumer (drain_stdout_into_scrollback) makes room promptly enough
 * that the producer never loops more than 1000 times, and that scrollback
 * does grow (data actually reached the ring).
 *
 * This mirrors the 10 Hz drain guarantee in production: the read end is
 * O_NONBLOCK, so drain loops until EAGAIN without blocking the event loop.
 * A full pipe triggers EAGAIN / EWOULDBLOCK on the write side; draining first
 * makes room for the next write.
 *
 * Realistic ceiling is ~3 drain iterations (100 KB target / 64 KB pipe / 256 B
 * chunks).  The 1000 cap below is the deadlock guard, NOT a tight bound --
 * hitting it would mean drain itself stopped consuming the pipe.  Future
 * maintainers seeing flake at the cap should suspect a regression in
 * drain_stdout_into_scrollback, not raise the cap.
 * ---------------------------------------------------------------------- */
static void test_long_output_does_not_deadlock_event_loop(void) {
	setup();

	int pipefd[2];
	assert(pipe(pipefd) == 0);
	/* 2026-06-10 JES #691 Phase C.0.4: assert fcntls so silent O_NONBLOCK
	 * failures surface as clear assertions rather than as a producer-side
	 * write() hang (test 2 would deadlock silently otherwise). */
	assert(fcntl(pipefd[0], F_SETFL, O_NONBLOCK) == 0);
	assert(fcntl(pipefd[1], F_SETFL, O_NONBLOCK) == 0);

	const size_t target = 100 * 1024;
	char chunk[256];
	memset(chunk, 'x', sizeof(chunk));
	chunk[sizeof(chunk) - 1] = '\n';  /* ensure every chunk terminates a line */

	size_t total_written = 0;
	int    drain_iters   = 0;
	int    ring_before   = g_state.scrollback_count;

	while (total_written < target && drain_iters < 1000) {
		ssize_t nw = write(pipefd[1], chunk, sizeof(chunk));
		if (nw < 0) {
			/* Pipe full -- drain and retry; proves consumer makes room */
			drain_stdout_into_scrollback(&g_state, pipefd[0]);
			drain_iters++;
			continue;
		}
		total_written += (size_t)nw;
	}
	/* Final drain: consume whatever remains in the pipe */
	drain_stdout_into_scrollback(&g_state, pipefd[0]);
	close(pipefd[1]);
	close(pipefd[0]);

	assert(total_written >= target);
	assert(drain_iters < 1000);
	assert(g_state.scrollback_count > ring_before);

	teardown();
}

/* -------------------------------------------------------------------------
 * test_backslash_continuation_accumulates
 *
 * 2026-06-10 JES #691 Phase C.0.5: trailing '\' accumulates across Enter
 * presses; only the terminator line (no trailing '\') dispatches eval.
 *
 * Sequence:
 *   "foo\" Enter  -> multiline_lines == 1, eval NOT called
 *   "bar\" Enter  -> multiline_lines == 2, eval NOT called
 *   "baz"  Enter  -> eval called with "foo\nbar\nbaz"; state reset
 * ---------------------------------------------------------------------- */
static void test_backslash_continuation_accumulates(void) {
	setup();

	/* Line 1: "foo\" + Enter */
	boxen_event_t ev;
	const char *line1 = "foo\\";
	for (const char *p = line1; *p; p++) {
		ev = make_char_event(*p);
		boxen_repl_run_one_tick(&g_state, &ev);
	}
	ev = make_key_event(BOXEN_KEY_ENTER);
	boxen_repl_run_one_tick(&g_state, &ev);

	assert(g_state.multiline_lines == 1);
	assert(g_eval_result[0] == '\0');  /* eval not called yet */

	/* Line 2: "bar\" + Enter */
	const char *line2 = "bar\\";
	for (const char *p = line2; *p; p++) {
		ev = make_char_event(*p);
		boxen_repl_run_one_tick(&g_state, &ev);
	}
	ev = make_key_event(BOXEN_KEY_ENTER);
	boxen_repl_run_one_tick(&g_state, &ev);

	assert(g_state.multiline_lines == 2);
	assert(g_eval_result[0] == '\0');  /* still not called */

	/* Line 3: "baz" + Enter (terminator) */
	const char *line3 = "baz";
	for (const char *p = line3; *p; p++) {
		ev = make_char_event(*p);
		boxen_repl_run_one_tick(&g_state, &ev);
	}
	ev = make_key_event(BOXEN_KEY_ENTER);
	boxen_repl_run_one_tick(&g_state, &ev);

	assert(strcmp(g_eval_result, "EVAL:foo\nbar\nbaz") == 0);
	assert(g_state.multiline_lines == 0);
	assert(g_state.multiline_len   == 0);

	teardown();
}

/* -------------------------------------------------------------------------
 * test_ctrl_c_in_multiline_discards_buffer
 *
 * 2026-06-10 JES #691 Phase C.0.5: Ctrl-C in multi-line mode discards the
 * accumulated buffer and resets to single-line prompt WITHOUT setting
 * should_quit.  Eval is never called.
 * ---------------------------------------------------------------------- */
static void test_ctrl_c_in_multiline_discards_buffer(void) {
	setup();

	/* Accumulate 2 continuation lines */
	boxen_event_t ev;
	const char *line1 = "foo\\";
	for (const char *p = line1; *p; p++) {
		ev = make_char_event(*p);
		boxen_repl_run_one_tick(&g_state, &ev);
	}
	ev = make_key_event(BOXEN_KEY_ENTER);
	boxen_repl_run_one_tick(&g_state, &ev);

	const char *line2 = "bar\\";
	for (const char *p = line2; *p; p++) {
		ev = make_char_event(*p);
		boxen_repl_run_one_tick(&g_state, &ev);
	}
	ev = make_key_event(BOXEN_KEY_ENTER);
	boxen_repl_run_one_tick(&g_state, &ev);

	assert(g_state.multiline_lines == 2);

	/* Send Ctrl-C: should discard accumulator, NOT quit */
	ev = make_key_event(BOXEN_KEY_CTRL_C);
	boxen_repl_run_one_tick(&g_state, &ev);

	assert(g_state.multiline_lines == 0);
	assert(g_state.multiline_len   == 0);
	assert(g_state.multiline_buf[0] == '\0');
	assert(g_state.input_buf[0]        == '\0');
	assert(g_state.input_len           == 0);
	assert(g_state.input_cursor_pos    == 0);
	assert(g_state.should_quit         == false);  /* critical: must NOT exit */
	assert(g_eval_result[0]            == '\0');   /* eval never called */

	teardown();
}

/* =========================================================================
 * 2026-06-17 JES Phase C.0.7e #691: cursor-based editing tests.
 *
 * All seven tests below reference input_cursor_pos and input_len.
 * They will not compile until the rename is applied to boxen_repl_internal.h
 * and boxen_repl.c -- that is the intended RED state.
 * ========================================================================= */

/* -------------------------------------------------------------------------
 * Test 24: test_left_arrow_moves_cursor_left_within_buffer
 *
 * Pre-load "abc" (len 3, caret at 3).  Press LEFT twice.  Assert
 * input_cursor_pos == 1, input_len == 3, input_buf unchanged.
 * ---------------------------------------------------------------------- */
static void test_left_arrow_moves_cursor_left_within_buffer(void) {
	setup();

	strncpy(g_state.input_buf, "abc", sizeof(g_state.input_buf) - 1);
	g_state.input_len        = 3;
	g_state.input_cursor_pos = 3;

	boxen_event_t left = make_key_event(BOXEN_KEY_LEFT);

	boxen_repl_run_one_tick(&g_state, &left);
	assert(g_state.input_cursor_pos == 2);
	assert(g_state.input_len        == 3);
	assert(strcmp(g_state.input_buf, "abc") == 0);

	boxen_repl_run_one_tick(&g_state, &left);
	assert(g_state.input_cursor_pos == 1);
	assert(g_state.input_len        == 3);

	teardown();
}

/* -------------------------------------------------------------------------
 * Test 25: test_right_arrow_moves_cursor_right_within_buffer
 *
 * Pre-load "abc" with caret at 1.  Press RIGHT twice.  Assert
 * input_cursor_pos == 3, input_len == 3, input_buf unchanged.
 * ---------------------------------------------------------------------- */
static void test_right_arrow_moves_cursor_right_within_buffer(void) {
	setup();

	strncpy(g_state.input_buf, "abc", sizeof(g_state.input_buf) - 1);
	g_state.input_len        = 3;
	g_state.input_cursor_pos = 1;

	boxen_event_t right = make_key_event(BOXEN_KEY_RIGHT);

	boxen_repl_run_one_tick(&g_state, &right);
	assert(g_state.input_cursor_pos == 2);
	assert(g_state.input_len        == 3);
	assert(strcmp(g_state.input_buf, "abc") == 0);

	boxen_repl_run_one_tick(&g_state, &right);
	assert(g_state.input_cursor_pos == 3);
	assert(g_state.input_len        == 3);

	teardown();
}

/* -------------------------------------------------------------------------
 * Test 26: test_printable_mid_buffer_inserts_at_cursor
 *
 * Pre-load "ac" (len 2, caret at 1 between 'a' and 'c').
 * Type 'b'.  Assert input_buf == "abc", input_len == 3,
 * input_cursor_pos == 2.
 * ---------------------------------------------------------------------- */
static void test_printable_mid_buffer_inserts_at_cursor(void) {
	setup();

	strncpy(g_state.input_buf, "ac", sizeof(g_state.input_buf) - 1);
	g_state.input_len        = 2;
	g_state.input_cursor_pos = 1;

	boxen_event_t b_ev = make_char_event('b');
	boxen_repl_run_one_tick(&g_state, &b_ev);

	assert(strcmp(g_state.input_buf, "abc") == 0);
	assert(g_state.input_len        == 3);
	assert(g_state.input_cursor_pos == 2);

	teardown();
}

/* -------------------------------------------------------------------------
 * Test 27: test_backspace_mid_buffer_deletes_before_cursor
 *
 * Pre-load "abc" (len 3, caret at 2 between 'b' and 'c').
 * Press Backspace.  Assert input_buf == "ac", input_len == 2,
 * input_cursor_pos == 1.
 * ---------------------------------------------------------------------- */
static void test_backspace_mid_buffer_deletes_before_cursor(void) {
	setup();

	strncpy(g_state.input_buf, "abc", sizeof(g_state.input_buf) - 1);
	g_state.input_len        = 3;
	g_state.input_cursor_pos = 2;

	boxen_event_t bs = make_key_event(BOXEN_KEY_BACKSPACE);
	boxen_repl_run_one_tick(&g_state, &bs);

	assert(strcmp(g_state.input_buf, "ac") == 0);
	assert(g_state.input_len        == 2);
	assert(g_state.input_cursor_pos == 1);

	teardown();
}

/* -------------------------------------------------------------------------
 * Test 28: test_ctrl_a_jumps_cursor_to_start
 *
 * Pre-load "hello" (len 5, caret at 5).
 * Press Ctrl-A.  Assert input_cursor_pos == 0, input_len == 5,
 * input_buf unchanged.
 * ---------------------------------------------------------------------- */
static void test_ctrl_a_jumps_cursor_to_start(void) {
	setup();

	strncpy(g_state.input_buf, "hello", sizeof(g_state.input_buf) - 1);
	g_state.input_len        = 5;
	g_state.input_cursor_pos = 5;

	boxen_event_t ctrl_a = make_key_event(BOXEN_KEY_CTRL_A);
	boxen_repl_run_one_tick(&g_state, &ctrl_a);

	assert(g_state.input_cursor_pos == 0);
	assert(g_state.input_len        == 5);
	assert(strcmp(g_state.input_buf, "hello") == 0);

	teardown();
}

/* -------------------------------------------------------------------------
 * Test 29: test_ctrl_e_jumps_cursor_to_end
 *
 * Pre-load "hello" (len 5, caret at 2 mid-word).
 * Press Ctrl-E.  Assert input_cursor_pos == 5, input_len == 5,
 * input_buf unchanged.
 * ---------------------------------------------------------------------- */
static void test_ctrl_e_jumps_cursor_to_end(void) {
	setup();

	strncpy(g_state.input_buf, "hello", sizeof(g_state.input_buf) - 1);
	g_state.input_len        = 5;
	g_state.input_cursor_pos = 2;

	boxen_event_t ctrl_e = make_key_event(BOXEN_KEY_CTRL_E);
	boxen_repl_run_one_tick(&g_state, &ctrl_e);

	assert(g_state.input_cursor_pos == 5);
	assert(g_state.input_len        == 5);
	assert(strcmp(g_state.input_buf, "hello") == 0);

	teardown();
}

/* -------------------------------------------------------------------------
 * Test 30: test_cursor_stays_at_bounds
 *
 * Verify LEFT at col 0 is a no-op and RIGHT at end is a no-op.
 *
 * Also verify forward-delete (BOXEN_KEY_DELETE) at end is a no-op
 * and at mid-buffer removes the char at cursor without moving it.
 * ---------------------------------------------------------------------- */
static void test_cursor_stays_at_bounds(void) {
	setup();

	/* --- LEFT at column 0 is a no-op --- */
	strncpy(g_state.input_buf, "xy", sizeof(g_state.input_buf) - 1);
	g_state.input_len        = 2;
	g_state.input_cursor_pos = 0;

	boxen_event_t left = make_key_event(BOXEN_KEY_LEFT);
	boxen_repl_run_one_tick(&g_state, &left);
	assert(g_state.input_cursor_pos == 0);   /* clamped at 0 */
	assert(g_state.input_len        == 2);
	assert(strcmp(g_state.input_buf, "xy") == 0);

	/* --- RIGHT at end is a no-op --- */
	g_state.input_cursor_pos = 2;
	boxen_event_t right = make_key_event(BOXEN_KEY_RIGHT);
	boxen_repl_run_one_tick(&g_state, &right);
	assert(g_state.input_cursor_pos == 2);   /* clamped at len */
	assert(g_state.input_len        == 2);
	assert(strcmp(g_state.input_buf, "xy") == 0);

	/* --- DELETE at end is a no-op --- */
	boxen_event_t del = make_key_event(BOXEN_KEY_DELETE);
	boxen_repl_run_one_tick(&g_state, &del);
	assert(g_state.input_cursor_pos == 2);
	assert(g_state.input_len        == 2);
	assert(strcmp(g_state.input_buf, "xy") == 0);

	/* --- DELETE mid-buffer removes char at cursor --- */
	g_state.input_cursor_pos = 0;   /* caret before 'x'; delete removes 'x' */
	boxen_repl_run_one_tick(&g_state, &del);
	assert(strcmp(g_state.input_buf, "y") == 0);
	assert(g_state.input_len        == 1);
	assert(g_state.input_cursor_pos == 0);   /* cursor stays in place */

	teardown();
}

/* -------------------------------------------------------------------------
 * test_ctrl_c_in_multiline_with_popup_open_discards_both
 *
 * 2026-06-17 JES #691 Phase C.0.7d: Ctrl-C in multi-line mode WITH a
 * completion popup open must:
 *   - close the popup (completion_popup == NULL)
 *   - discard the accumulated multi-line buffer (multiline_lines == 0)
 *   - clear the input bar (input_buf[0] == '\0')
 *   - NOT exit the REPL (should_quit == false)
 *
 * Regression guard for the case where popup + multiline were both active
 * and Ctrl-C was falling through to should_quit = true.
 * ---------------------------------------------------------------------- */
static int hook_completion_multi_for_slash(const char *buf, size_t bl,
                                           char cand[][BOXEN_COMPLETION_CANDIDATE_MAX],
                                           int max) {
	(void)bl; (void)max;
	if (strcmp(buf, "/") == 0) {
		strcpy(cand[0], "/help");
		strcpy(cand[1], "/jump");
		strcpy(cand[2], "/list");
		return 3;
	}
	return 0;
}

static void test_ctrl_c_in_multiline_with_popup_open_discards_both(void) {
	setup();
	g_state.completion_hook = hook_completion_multi_for_slash;

	/* Step 1: enter multi-line mode by typing "foo\" then Enter */
	boxen_event_t ev;
	const char *line1 = "foo\\";
	for (const char *p = line1; *p; p++) {
		ev = make_char_event(*p);
		boxen_repl_run_one_tick(&g_state, &ev);
	}
	ev = make_key_event(BOXEN_KEY_ENTER);
	boxen_repl_run_one_tick(&g_state, &ev);

	/* Verify we are in multi-line mode */
	assert(g_state.multiline_lines == 1);
	assert(g_state.input_buf[0] == '\0');
	assert(g_state.input_len == 0);

	/* Step 2: type "/" to get a non-empty input buffer, then Tab to open popup.
	 * palette_open_hook is NULL in test setup, so '/' inserts normally. */
	ev = make_char_event('/');
	boxen_repl_run_one_tick(&g_state, &ev);
	assert(g_state.input_buf[0] == '/');

	ev = make_key_event(BOXEN_KEY_TAB);
	boxen_repl_run_one_tick(&g_state, &ev);

	/* Verify popup is open */
	assert(g_state.completion_popup != NULL);
	assert(boxen_completion_popup_is_open(g_state.completion_popup));

	/* Step 3: press Ctrl-C -- must discard both multiline state AND popup */
	ev = make_key_event(BOXEN_KEY_CTRL_C);
	boxen_repl_run_one_tick(&g_state, &ev);

	/* Popup must be gone */
	assert(g_state.completion_popup == NULL);

	/* Multi-line accumulator must be cleared */
	assert(g_state.multiline_lines == 0);
	assert(g_state.multiline_len   == 0);
	assert(g_state.multiline_buf[0] == '\0');

	/* Input bar must be cleared */
	assert(g_state.input_buf[0]  == '\0');
	assert(g_state.input_len  == 0);

	/* Critical: must NOT exit the REPL */
	assert(g_state.should_quit == false);

	/* Eval must never have been called */
	assert(g_eval_result[0] == '\0');

	teardown();
}

/* -------------------------------------------------------------------------
 * Test 24: test_slash_then_letter_within_window_inserts_both
 *
 * 2026-06-17 JES #691 Phase C.0.7a: debounce cancellation.
 *
 * Typing '/' then 'h' rapidly (mock time does NOT advance) must:
 *   - NOT open the palette
 *   - Insert "/h" into input_buf (the deferred '/' is inserted first, then 'h')
 *   - Clear slash_pending_until_ms (debounce cancelled)
 *
 * This is the primary regression guard: before the debounce fix, typing
 * '/h<Tab>' triggered the palette on '/' and the user never got to type 'h'.
 * ---------------------------------------------------------------------- */
static void test_slash_then_letter_within_window_inserts_both(void) {
	setup();

	g_mock_now_ms             = 1000;
	g_mock_palette_open_count = 0;
	g_state.palette_open_hook     = mock_palette_open;
	g_state.palette_dispatch_hook = mock_palette_dispatch;
	g_state.now_ms_hook           = mock_now_ms;

	/* Type '/' -- arms the debounce.  Mock time does NOT advance. */
	boxen_event_t ev_slash = make_char_event('/');
	boxen_repl_run_one_tick(&g_state, &ev_slash);

	/* Palette must NOT be open. */
	assert(g_mock_palette_open_count == 0);
	assert(g_state.palette_state == NULL);
	/* Debounce must be pending. */
	assert(g_state.slash_pending_until_ms != 0);
	/* input_buf still empty (the '/' is deferred). */
	assert(g_state.input_buf[0] == '\0');
	assert(g_state.input_len == 0);

	/* Type 'h' -- mock time still 1000ms, well within the debounce window.
	 * This must cancel the pending open and insert both '/' and 'h'. */
	boxen_event_t ev_h = make_char_event('h');
	boxen_repl_run_one_tick(&g_state, &ev_h);

	/* Palette must still be closed. */
	assert(g_mock_palette_open_count == 0);
	assert(g_state.palette_state == NULL);
	/* input_buf must now contain "/h". */
	assert(strcmp(g_state.input_buf, "/h") == 0);
	assert(g_state.input_len == 2);
	/* Debounce cleared. */
	assert(g_state.slash_pending_until_ms == 0);

	teardown();
}

/* -------------------------------------------------------------------------
 * Test 25: test_slash_then_timeout_opens_palette
 *
 * 2026-06-17 JES #691 Phase C.0.7a: debounce expiry fires palette.
 *
 * Type '/', advance mock time past BOXEN_REPL_SLASH_DEBOUNCE_MS, call
 * boxen_repl_check_pending_slash.  Assert palette opened and input_buf empty.
 *
 * This test exercises the primary user path: type '/' alone and wait for the
 * palette to appear.
 * ---------------------------------------------------------------------- */
static void test_slash_then_timeout_opens_palette(void) {
	setup();

	g_mock_now_ms             = 5000;  /* arbitrary start */
	g_mock_palette_open_count = 0;
	g_state.palette_open_hook     = mock_palette_open;
	g_state.palette_dispatch_hook = mock_palette_dispatch;
	g_state.now_ms_hook           = mock_now_ms;

	/* Type '/' -- arms the debounce. */
	boxen_event_t ev_slash = make_char_event('/');
	boxen_repl_run_one_tick(&g_state, &ev_slash);

	assert(g_mock_palette_open_count == 0);   /* not yet */
	assert(g_state.slash_pending_until_ms != 0);

	/* Advance time to just BEFORE the deadline -- palette must not fire. */
	g_mock_now_ms = g_state.slash_pending_until_ms - 1;
	boxen_repl_check_pending_slash(&g_state);
	assert(g_mock_palette_open_count == 0);   /* still waiting */

	/* Advance time PAST the deadline -- palette must fire on next check. */
	g_mock_now_ms = g_state.slash_pending_until_ms + 1;
	boxen_repl_check_pending_slash(&g_state);

	assert(g_mock_palette_open_count == 1);   /* fired */
	assert(g_state.palette_state != NULL);    /* sentinel installed */
	/* input_buf must still be empty (the '/' was never inserted). */
	assert(g_state.input_buf[0] == '\0');
	assert(g_state.input_len == 0);
	/* Debounce cleared. */
	assert(g_state.slash_pending_until_ms == 0);

	/* Clean up sentinel. */
	g_state.palette_state = NULL;
	teardown();
}

/* -------------------------------------------------------------------------
 * Test 26: test_slash_then_enter_cancels_pending_palette
 *
 * 2026-06-17 JES #691 Phase C.0.7a P1: pressing Enter (or UP/DOWN/Tab) after
 * '/' must cancel the pending slash-debounce so a deferred palette doesn't
 * pop 350ms later.  Without the cancel, type '/' + Enter + wait, and the
 * palette unexpectedly opens at the now-empty prompt.
 *
 * This regression test was added in response to the C.0.7a /gate round-1 P1
 * finding.
 * ---------------------------------------------------------------------- */
static void test_slash_then_enter_cancels_pending_palette(void) {
	setup();

	g_mock_now_ms             = 5000;
	g_mock_palette_open_count = 0;
	g_state.palette_open_hook     = mock_palette_open;
	g_state.palette_dispatch_hook = mock_palette_dispatch;
	g_state.now_ms_hook           = mock_now_ms;

	/* Type '/' -- arms the debounce. */
	boxen_event_t ev_slash = make_char_event('/');
	boxen_repl_run_one_tick(&g_state, &ev_slash);
	assert(g_state.slash_pending_until_ms != 0);

	/* Press Enter -- must cancel the pending palette. */
	boxen_event_t ev_enter = make_key_event(BOXEN_KEY_ENTER);
	boxen_repl_run_one_tick(&g_state, &ev_enter);
	assert(g_state.slash_pending_until_ms == 0);  /* cancelled */

	/* Advance time past where the deadline would have been. */
	g_mock_now_ms = 5000 + BOXEN_REPL_SLASH_DEBOUNCE_MS + 100;
	boxen_repl_check_pending_slash(&g_state);

	/* Palette must NOT have fired -- the cancel held. */
	assert(g_mock_palette_open_count == 0);
	assert(g_state.palette_state == NULL);

	teardown();
}

/* -------------------------------------------------------------------------
 * Test 27: test_slash_then_up_cancels_pending_palette
 *
 * 2026-06-17 JES #691 Phase C.0.7a P1: same as test 26 but for UP arrow.
 * History nav must not be shadowed by a deferred palette pop.
 * ---------------------------------------------------------------------- */
static void test_slash_then_up_cancels_pending_palette(void) {
	setup();

	g_mock_now_ms             = 5000;
	g_mock_palette_open_count = 0;
	g_state.palette_open_hook     = mock_palette_open;
	g_state.palette_dispatch_hook = mock_palette_dispatch;
	g_state.now_ms_hook           = mock_now_ms;

	boxen_event_t ev_slash = make_char_event('/');
	boxen_repl_run_one_tick(&g_state, &ev_slash);
	assert(g_state.slash_pending_until_ms != 0);

	boxen_event_t ev_up = make_key_event(BOXEN_KEY_UP);
	boxen_repl_run_one_tick(&g_state, &ev_up);
	assert(g_state.slash_pending_until_ms == 0);

	g_mock_now_ms = 5000 + BOXEN_REPL_SLASH_DEBOUNCE_MS + 100;
	boxen_repl_check_pending_slash(&g_state);

	assert(g_mock_palette_open_count == 0);
	assert(g_state.palette_state == NULL);

	teardown();
}

/* -------------------------------------------------------------------------
 * Test 28: test_slash_then_tab_cancels_pending_palette
 *
 * 2026-06-17 JES #691 Phase C.0.7a P1: same as test 26 but for Tab.
 * Tab completion must not be shadowed by a deferred palette pop.
 * ---------------------------------------------------------------------- */
static void test_slash_then_tab_cancels_pending_palette(void) {
	setup();

	g_mock_now_ms             = 5000;
	g_mock_palette_open_count = 0;
	g_state.palette_open_hook     = mock_palette_open;
	g_state.palette_dispatch_hook = mock_palette_dispatch;
	g_state.now_ms_hook           = mock_now_ms;

	boxen_event_t ev_slash = make_char_event('/');
	boxen_repl_run_one_tick(&g_state, &ev_slash);
	assert(g_state.slash_pending_until_ms != 0);

	boxen_event_t ev_tab = make_key_event(BOXEN_KEY_TAB);
	boxen_repl_run_one_tick(&g_state, &ev_tab);
	assert(g_state.slash_pending_until_ms == 0);

	g_mock_now_ms = 5000 + BOXEN_REPL_SLASH_DEBOUNCE_MS + 100;
	boxen_repl_check_pending_slash(&g_state);

	assert(g_mock_palette_open_count == 0);
	assert(g_state.palette_state == NULL);

	teardown();
}

/* -------------------------------------------------------------------------
 * 2026-06-17 JES #691 Phase C.0.7b: crash regression tests.
 *
 * Tests 24-26 target three crashes reported from manual testing of the
 * boxen --debug-tui REPL:
 *
 *   Test 24: popup Enter -- press Enter on the completion popup to accept
 *     the selected candidate.  Must: replace input_buf, close popup, not crash.
 *
 *   Test 25: popup printable letter -- press a printable letter while popup
 *     is open (the "any other key" fallthrough).  Must: close popup, insert
 *     the letter into input_buf, not crash.  A sub-case: pressing '/' while
 *     the popup is open and input_cursor > 0 must NOT open the palette.
 *
 *   Test 26: popup '/' with empty input -- if the popup was somehow opened
 *     with an empty input buffer and the user types '/', the "any other key"
 *     fallthrough must NOT open the palette (palette guard requires
 *     input_cursor == 0 AND palette_open_hook != NULL -- but closing the
 *     popup and falling through should insert '/' normally, not open palette,
 *     because the '/' gate fires AFTER the cursor check).
 *     Actually: the '/' gate fires in the printable handler which checks
 *     input_cursor == 0. If cursor was 0 when popup was open AND the hook is
 *     wired, pressing '/' (which is a printable non-Enter/Esc/Tab key) would:
 *       1. close popup (completion_popup = NULL)
 *       2. call boxen_window_focus (input_win already focused -- no-op)
 *       3. fall through to the printable handler
 *       4. printable handler: ch=='/', cursor==0, palette_open_hook!=NULL,
 *          palette_state==NULL -> FIRES PALETTE OPEN
 *     This is the hotkey-letter crash: any letter that maps to the palette
 *     trigger does NOT need to be '/'; rather the existing mock_palette_open
 *     installs a sentinel that the test build's teardown skips safely.
 *     The real crash occurs in production because palette_open_hook is the
 *     real boxen_repl_real_palette_open and s->palette_state still NULL
 *     allows the assert(s_ctx.modal_win == NULL) in boxen_backend_open to fire
 *     IF s_ctx.modal_win was left non-NULL from a prior uncleaned session, OR
 *     more likely: the assert fires because the popup is closed WHILE the
 *     palette is supposedly already open (double-open).  The fix is to guard
 *     the '/' trigger in the popup fallthrough path so it does NOT open the
 *     palette when falling through from a closed popup.
 *
 * NOTE: Test 26 is intentionally a behavioural regression guard; it exercises
 * the specific path (popup open, cursor==0, '/' pressed, palette hook wired)
 * that could open the palette from the popup fallthrough.  The expected
 * behaviour AFTER the fix: '/' is inserted into input_buf normally (popup
 * closes, '/' inserted, no palette open).
 * ---------------------------------------------------------------------- */

/* -------------------------------------------------------------------------
 * Test 24: test_completion_popup_enter_accepts_selection_no_crash
 *
 * 2026-06-17 JES #691 Phase C.0.7b.
 *
 * Open a multi-candidate popup (same hook as test 13), then press Enter.
 * Expected: popup closes, selected candidate replaces input_buf, no crash.
 * ---------------------------------------------------------------------- */
static void test_completion_popup_enter_accepts_selection_no_crash(void) {
	setup();
	g_state.completion_hook = hook_completion_multi;

	/* Type "/" to get candidates when Tab is pressed */
	boxen_event_t ev_slash = make_char_event('/');
	boxen_repl_run_one_tick(&g_state, &ev_slash);
	assert(strcmp(g_state.input_buf, "/") == 0);

	/* Press Tab: multi hook returns 3 candidates -> popup opens */
	boxen_event_t ev_tab = make_key_event(BOXEN_KEY_TAB);
	boxen_repl_run_one_tick(&g_state, &ev_tab);
	assert(g_state.completion_popup != NULL);

	/* Press Enter: should accept selection (first candidate "/help"),
	 * close popup, update input_buf -- and NOT crash. */
	boxen_event_t ev_enter = make_key_event(BOXEN_KEY_ENTER);
	boxen_repl_run_one_tick(&g_state, &ev_enter);

	/* Popup must be gone */
	assert(g_state.completion_popup == NULL);

	/* input_buf must be the first candidate (selected == 0 on open) */
	assert(strcmp(g_state.input_buf, "/help") == 0);

	teardown();
}

/* -------------------------------------------------------------------------
 * Test 25: test_completion_popup_printable_letter_closes_popup_no_crash
 *
 * 2026-06-17 JES #691 Phase C.0.7b.
 *
 * Open a multi-candidate popup (with slash in input), then press a
 * printable letter 'x'.  The "any other key" path should close the popup
 * and insert 'x' into input_buf.  Must not crash.
 *
 * Note: we do NOT wire palette_open_hook here so that typing '/' at
 * empty input inserts normally (palette_open_hook == NULL skips the gate).
 * This isolates the "printable char in popup fallthrough inserts correctly"
 * behavior without the palette interaction.
 * ---------------------------------------------------------------------- */
static void test_completion_popup_printable_letter_closes_popup_no_crash(void) {
	setup();
	g_state.completion_hook = hook_completion_multi;
	/* NOTE: palette_open_hook intentionally NOT wired so '/' inserts normally. */

	/* Type "/" -> Tab -> popup opens (hook returns 3 candidates for "/") */
	boxen_event_t ev_slash = make_char_event('/');
	boxen_event_t ev_tab   = make_key_event(BOXEN_KEY_TAB);
	boxen_repl_run_one_tick(&g_state, &ev_slash);
	boxen_repl_run_one_tick(&g_state, &ev_tab);
	assert(g_state.completion_popup != NULL);
	assert(strcmp(g_state.input_buf, "/") == 0);

	/* Press 'x' while popup is open: popup should close, 'x' should be
	 * inserted (input_cursor was 1 for the "/", now becomes 2).
	 * After fix: the "any other key" branch inserts 'x' directly without
	 * going through the palette-open gate. */
	boxen_event_t ev_x = make_char_event('x');
	boxen_repl_run_one_tick(&g_state, &ev_x);

	/* Popup must be gone */
	assert(g_state.completion_popup == NULL);

	/* 'x' must have been inserted after "/" */
	assert(strcmp(g_state.input_buf, "/x") == 0);
	assert(g_state.input_len == 2);

	/* No palette state (hook not wired) */
	assert(g_state.palette_state == NULL);

	teardown();
}

/* -------------------------------------------------------------------------
 * Test 26: test_completion_popup_slash_with_empty_input_inserts_not_palette
 *
 * 2026-06-17 JES #691 Phase C.0.7b.
 *
 * This test targets the specific crash path: a popup can be open even with
 * input_cursor == 0 (e.g. if the completion hook fires for empty input, or
 * if the popup was left open after Backspace deleted the last char).
 * Simulate this by directly installing a popup on an empty input bar, then
 * press '/' (a printable key that is NOT Tab/Enter/Esc).
 *
 * With the palette_open_hook wired:
 *   BEFORE fix: popup closes, falls through, '/' gate fires because
 *     input_cursor==0 && palette_open_hook!=NULL && palette_state==NULL
 *     -> palette opens (wrong) OR assert fires (crash in production).
 *   AFTER fix: '/' closes popup and is inserted into input_buf normally
 *     (no palette open).
 *
 * The correct post-fix behaviour: '/' is inserted, palette_state stays NULL.
 * ---------------------------------------------------------------------- */
static int hook_completion_empty(const char *buf, size_t bl,
                                 char cand[][BOXEN_COMPLETION_CANDIDATE_MAX],
                                 int max) {
	(void)buf; (void)bl; (void)max;
	/* Returns 2 candidates even for an empty buffer to force popup open. */
	strcpy(cand[0], "alpha");
	strcpy(cand[1], "beta");
	return 2;
}

static void test_completion_popup_slash_with_empty_input_inserts_not_palette(void) {
	setup();

	/* Wire both hooks so the '/' gate CAN fire if the code has the bug. */
	g_state.completion_hook   = hook_completion_empty;
	g_state.palette_open_hook = mock_palette_open;
	g_mock_palette_open_count = 0;

	/* Open popup with empty input by pressing Tab (completion hook always
	 * returns 2 candidates regardless of buf content). */
	boxen_event_t ev_tab = make_key_event(BOXEN_KEY_TAB);
	boxen_repl_run_one_tick(&g_state, &ev_tab);

	/* Popup must be open */
	assert(g_state.completion_popup != NULL);
	/* input_buf must still be empty (Tab doesn't insert) */
	assert(g_state.input_len == 0);

	/* Now press '/': "any other key" in popup mode.
	 * Bug: with cursor==0 and palette_open_hook set, the '/' falls through
	 * and triggers the palette open gate.  Fix: the popup fallthrough must
	 * NOT open the palette; '/' should be inserted normally. */
	boxen_event_t ev_slash = make_char_event('/');
	boxen_repl_run_one_tick(&g_state, &ev_slash);

	/* Popup must be closed */
	assert(g_state.completion_popup == NULL);

	/* '/' must be inserted into input_buf (not swallowed by palette open) */
	assert(strcmp(g_state.input_buf, "/") == 0);
	assert(g_state.input_len == 1);

	/* Palette must NOT have opened -- this is the bug assertion */
	assert(g_state.palette_state == NULL);
	assert(g_mock_palette_open_count == 0);

	teardown();
}

/* -------------------------------------------------------------------------
 * 2026-06-29 JES #803: output-pane scrollback (PgUp/PgDn).
 *
 * The boxen REPL owns the alternate screen buffer, so terminal-native
 * scrollback can't reach lines that scroll off the top of the output pane.
 * These tests pin the behavioral contract for in-app scrollback:
 *
 *   1. PgUp increments output_scroll_offset by (output_h - 1) "page" lines.
 *   2. PgDn decrements output_scroll_offset by the same page step,
 *      clamped at 0 (can't scroll past newest).
 *   3. Offset is clamped at (scrollback_count - 1) on the upward side
 *      (can't scroll past oldest -- at least one line stays visible).
 *   4. Submitting a new expression resets offset to 0 (scroll-on-output:
 *      typing/submitting unsticks the view).
 *   5. Appending output while scrolled does NOT reset offset (user can
 *      review old content while a background script prints).
 * ---------------------------------------------------------------------- */

/* Helper: fill the ring so PgUp has something to scroll into. */
static void fill_scrollback(int n) {
	char buf[64];
	for (int i = 0; i < n; i++) {
		snprintf(buf, sizeof(buf), "line%d", i);
		boxen_repl_append_scrollback(&g_state, buf);
	}
}

static void test_pgup_increments_output_scroll_offset(void) {
	setup();

	/* Fill with more lines than fit in the pane. Output pane height in
	 * test setup is TEST_HEIGHT - 2 = 22 rows. */
	fill_scrollback(50);
	assert(g_state.output_scroll_offset == 0);

	boxen_event_t ev = make_key_event(BOXEN_KEY_PGUP);
	boxen_repl_run_one_tick(&g_state, &ev);

	/* Page step is output_h - 1 = 21. */
	assert(g_state.output_scroll_offset == 21);

	boxen_repl_run_one_tick(&g_state, &ev);
	/* Second PgUp -> 42, still within (count=50 - 1=49) clamp. */
	assert(g_state.output_scroll_offset == 42);

	teardown();
}

static void test_pgdn_decrements_offset_and_clamps_at_zero(void) {
	setup();
	fill_scrollback(50);

	/* Manually advance the offset, then walk it back with PgDn. */
	g_state.output_scroll_offset = 30;

	boxen_event_t ev = make_key_event(BOXEN_KEY_PGDN);
	boxen_repl_run_one_tick(&g_state, &ev);
	assert(g_state.output_scroll_offset == 9);  /* 30 - 21 */

	boxen_repl_run_one_tick(&g_state, &ev);
	assert(g_state.output_scroll_offset == 0);  /* clamped, not -12 */

	/* Further PgDn at zero stays at zero. */
	boxen_repl_run_one_tick(&g_state, &ev);
	assert(g_state.output_scroll_offset == 0);

	teardown();
}

static void test_pgup_clamps_at_top_of_scrollback(void) {
	setup();
	fill_scrollback(10);  /* fewer lines than the pane height (22) */

	/* PgUp once: page step would be 21, but only 9 = (count-1) lines
	 * of scroll headroom exist (we keep one line visible). */
	boxen_event_t ev = make_key_event(BOXEN_KEY_PGUP);
	boxen_repl_run_one_tick(&g_state, &ev);
	assert(g_state.output_scroll_offset == 9);

	/* Additional PgUp doesn't push past the oldest entry. */
	boxen_repl_run_one_tick(&g_state, &ev);
	assert(g_state.output_scroll_offset == 9);

	teardown();
}

static void test_submit_resets_scroll_offset_to_zero(void) {
	setup();
	fill_scrollback(50);

	g_state.output_scroll_offset = 30;

	/* Pre-load input_buf with an expression and dispatch via Enter. */
	snprintf(g_state.input_buf, sizeof(g_state.input_buf), "1 + 1");
	g_state.input_len        = (int)strlen(g_state.input_buf);
	g_state.input_cursor_pos = g_state.input_len;

	boxen_event_t enter = make_key_event(BOXEN_KEY_ENTER);
	boxen_repl_run_one_tick(&g_state, &enter);

	/* After dispatch the view snaps back to the bottom. */
	assert(g_state.output_scroll_offset == 0);

	teardown();
}

static void test_append_while_scrolled_preserves_offset(void) {
	setup();
	fill_scrollback(50);

	g_state.output_scroll_offset = 25;

	/* Simulate async output landing in scrollback (e.g. background script
	 * stdout drained through boxen_repl_append_scrollback). The user's
	 * scrolled-back view must NOT snap to the bottom -- they need to keep
	 * reading old content.
	 *
	 * Ring is NOT full here (count=50 < SIZE), so no anchor-follows-
	 * content bump fires; the offset is genuinely unchanged. */
	boxen_repl_append_scrollback(&g_state, "async output");

	assert(g_state.output_scroll_offset == 25);
	assert(g_state.scrollback_count == 51);  /* ring still filling */

	teardown();
}

/* 2026-06-29 JES #803: bar-raiser P1.2 -- when the ring is full and the
 * user is scrolled back, every append overwrites the oldest slot.  The
 * user's anchor must follow the content so the lines they were reading
 * don't get silently overwritten under them.  Verifies the same line
 * stays at the user's view position across appends. */
static void test_append_when_ring_full_advances_offset_to_pin_content(void) {
	setup();

	/* Fill the ring exactly to capacity with marker lines. */
	char buf[64];
	for (int i = 0; i < BOXEN_REPL_SCROLLBACK_SIZE; i++) {
		snprintf(buf, sizeof(buf), "fill%d", i);
		boxen_repl_append_scrollback(&g_state, buf);
	}
	assert(g_state.scrollback_count == BOXEN_REPL_SCROLLBACK_SIZE);

	/* Scroll the user back 10 lines.  Capture the content at their
	 * anchor position so we can verify it stays the same across appends. */
	g_state.output_scroll_offset = 10;
	int anchor_idx_before =
	    (g_state.scrollback_head - g_state.output_scroll_offset - 1
	     + BOXEN_REPL_SCROLLBACK_SIZE)
	    % BOXEN_REPL_SCROLLBACK_SIZE;
	char anchor_content_before[64];
	snprintf(anchor_content_before, sizeof(anchor_content_before),
	         "%s", g_state.scrollback[anchor_idx_before]);

	/* Append 5 new "async" lines (e.g. background-thread stdout). */
	for (int i = 0; i < 5; i++) {
		snprintf(buf, sizeof(buf), "async%d", i);
		boxen_repl_append_scrollback(&g_state, buf);
	}

	/* Offset must have advanced by 5 to keep the anchor on the same
	 * logical content (newest moved 5 forward, so offset compensates). */
	assert(g_state.output_scroll_offset == 15);

	/* The ring slot at (head - offset - 1) should hold the SAME content
	 * the user was anchored to before any appends -- they kept reading
	 * the same line, not silently sliding to whatever overwrote it. */
	int anchor_idx_after =
	    (g_state.scrollback_head - g_state.output_scroll_offset - 1
	     + BOXEN_REPL_SCROLLBACK_SIZE)
	    % BOXEN_REPL_SCROLLBACK_SIZE;
	assert(g_state.scrollback[anchor_idx_after] != NULL);
	assert(strcmp(g_state.scrollback[anchor_idx_after],
	              anchor_content_before) == 0);

	teardown();
}

/* 2026-06-29 JES #803: anchor-follows-content must NOT fire when the
 * view is pinned to the bottom (offset == 0).  Pinned-to-bottom users
 * expect "always show newest"; auto-incrementing would silently scroll
 * them off-newest on every async append. */
static void test_append_when_pinned_to_bottom_does_not_bump_offset(void) {
	setup();

	/* Fill the ring to capacity. */
	char buf[64];
	for (int i = 0; i < BOXEN_REPL_SCROLLBACK_SIZE; i++) {
		snprintf(buf, sizeof(buf), "fill%d", i);
		boxen_repl_append_scrollback(&g_state, buf);
	}
	assert(g_state.output_scroll_offset == 0);

	boxen_repl_append_scrollback(&g_state, "async output");

	/* Pinned (offset == 0) must stay pinned. */
	assert(g_state.output_scroll_offset == 0);

	teardown();
}

/* -------------------------------------------------------------------------
 * 2026-06-29 JES #805: mouse-wheel scrolls output pane; Option-Left/Right
 * jumps cursor by one word.  Tests below verify both bindings behave
 * symmetrically with PgUp/PgDn (#803) on the wheel side, and match
 * readline M-b/M-f semantics on the word-jump side.
 * ---------------------------------------------------------------------- */

/* Build a wheel mouse event.  button: 4 = wheel up, 5 = wheel down.
 * Coordinates default to (0,0) -- the dispatch layer routes wheel events
 * to the topmost window at the pointer location, but boxen_repl_run_one_tick
 * is invoked directly in unit tests (bypassing window dispatch) so the
 * coordinates are irrelevant to the on_input handler's wheel branch. */
static boxen_event_t make_wheel_event(uint8_t button) {
	boxen_event_t ev;
	memset(&ev, 0, sizeof(ev));
	ev.type = BOXEN_EV_MOUSE;
	ev.mouse.button  = button;
	ev.mouse.pressed = true;
	ev.mouse.x = 0;
	ev.mouse.y = 0;
	return ev;
}

/* Build a printable-char event with the ALT modifier set.  Models how
 * termbox2 decodes the ESC-prefix sequences macOS Terminal.app sends for
 * Option-letter combos (e.g. Option-b -> ESC b -> {key=NONE, ch='b',
 * mod=ALT}). */
static boxen_event_t make_alt_char_event(char ch) {
	boxen_event_t ev;
	memset(&ev, 0, sizeof(ev));
	ev.type    = BOXEN_EV_KEY;
	ev.key.key = BOXEN_KEY_NONE;
	ev.key.ch  = (uint32_t)ch;
	ev.key.mod = BOXEN_MOD_ALT;
	return ev;
}

/* Build a special-key event with the ALT modifier set.  Models the
 * iTerm2 "Left/Right option as Esc+" CSI sequences (ESC[1;3D /
 * ESC[1;3C) which termbox2 decodes as ARROW_LEFT/ARROW_RIGHT with the
 * ALT mod bit set. */
static boxen_event_t make_alt_key_event(boxen_key_t key) {
	boxen_event_t ev;
	memset(&ev, 0, sizeof(ev));
	ev.type    = BOXEN_EV_KEY;
	ev.key.key = key;
	ev.key.ch  = 0;
	ev.key.mod = BOXEN_MOD_ALT;
	return ev;
}

static void test_wheel_up_scrolls_output_pane(void) {
	setup();
	fill_scrollback(50);
	assert(g_state.output_scroll_offset == 0);

	boxen_event_t wheel_up = make_wheel_event(4);
	boxen_repl_run_one_tick(&g_state, &wheel_up);
	/* Wheel step is 3 lines per tick (less / Terminal.app convention). */
	assert(g_state.output_scroll_offset == 3);

	boxen_repl_run_one_tick(&g_state, &wheel_up);
	assert(g_state.output_scroll_offset == 6);

	teardown();
}

static void test_wheel_down_scrolls_back_toward_newest(void) {
	setup();
	fill_scrollback(50);
	g_state.output_scroll_offset = 20;

	boxen_event_t wheel_down = make_wheel_event(5);
	boxen_repl_run_one_tick(&g_state, &wheel_down);
	assert(g_state.output_scroll_offset == 17);

	boxen_repl_run_one_tick(&g_state, &wheel_down);
	assert(g_state.output_scroll_offset == 14);

	teardown();
}

static void test_wheel_down_clamps_at_zero(void) {
	setup();
	fill_scrollback(50);
	g_state.output_scroll_offset = 2;  /* less than one tick from bottom */

	boxen_event_t wheel_down = make_wheel_event(5);
	boxen_repl_run_one_tick(&g_state, &wheel_down);
	/* Step is 3, but cannot go below 0. */
	assert(g_state.output_scroll_offset == 0);

	/* Further wheel-down at zero stays at zero. */
	boxen_repl_run_one_tick(&g_state, &wheel_down);
	assert(g_state.output_scroll_offset == 0);

	teardown();
}

static void test_wheel_up_clamps_at_top_of_scrollback(void) {
	setup();
	fill_scrollback(4);  /* count - 1 = 3 lines of headroom */

	boxen_event_t wheel_up = make_wheel_event(4);
	boxen_repl_run_one_tick(&g_state, &wheel_up);
	/* Step is 3, count-1 is 3, so first tick lands exactly at the cap. */
	assert(g_state.output_scroll_offset == 3);

	/* Further wheel-up does not push past the oldest entry. */
	boxen_repl_run_one_tick(&g_state, &wheel_up);
	assert(g_state.output_scroll_offset == 3);

	teardown();
}

/* Regression guard for the #805 user-visible symptom: BEFORE the fix, the
 * up-arrow key code (which was what wheel events translated to under
 * Terminal.app) loaded prior history into the input bar.  AFTER the fix,
 * wheel events arrive as MOUSE events distinct from arrow keys and never
 * touch history -- this test would have failed with the old binding. */
static void test_wheel_does_not_navigate_history(void) {
	/* Point history at a nonexistent file so state_init's load is a no-op
	 * (same pattern as test_history_up_arrow_loads_previous above). */
	boxen_repl_set_history_path_for_test("/tmp/boxen_repl_test_wheel_nohist.txt");
	setup();

	/* Seed history. */
	boxen_repl_history_append(&g_state, "older");
	boxen_repl_history_append(&g_state, "newer");
	assert(g_state.history_count == 2);

	fill_scrollback(50);
	assert(g_state.input_len == 0);

	boxen_event_t wheel_up = make_wheel_event(4);
	boxen_repl_run_one_tick(&g_state, &wheel_up);

	/* The wheel must scroll output, NOT load history into the input bar. */
	assert(g_state.input_len == 0);
	assert(g_state.input_buf[0] == '\0');
	assert(g_state.history_nav_idx == -1);  /* no history-nav state engaged */
	assert(g_state.output_scroll_offset == 3);  /* but it DID scroll */

	teardown();
}

static void test_alt_b_jumps_word_back(void) {
	setup();

	/* Buffer "one two three", caret at end (13). */
	const char *line = "one two three";
	int line_len = (int)strlen(line);
	strncpy(g_state.input_buf, line, sizeof(g_state.input_buf) - 1);
	g_state.input_len        = line_len;
	g_state.input_cursor_pos = line_len;

	boxen_event_t alt_b = make_alt_char_event('b');

	boxen_repl_run_one_tick(&g_state, &alt_b);
	/* Caret should jump to start of "three" (position 8). */
	assert(g_state.input_cursor_pos == 8);
	assert(g_state.input_len == line_len);
	assert(strcmp(g_state.input_buf, line) == 0);

	boxen_repl_run_one_tick(&g_state, &alt_b);
	/* Start of "two" -> position 4. */
	assert(g_state.input_cursor_pos == 4);

	boxen_repl_run_one_tick(&g_state, &alt_b);
	/* Start of "one" -> position 0. */
	assert(g_state.input_cursor_pos == 0);

	/* Further M-b at position 0 stays at 0. */
	boxen_repl_run_one_tick(&g_state, &alt_b);
	assert(g_state.input_cursor_pos == 0);

	teardown();
}

static void test_alt_f_jumps_word_forward(void) {
	setup();

	const char *line = "one two three";
	int line_len = (int)strlen(line);
	strncpy(g_state.input_buf, line, sizeof(g_state.input_buf) - 1);
	g_state.input_len        = line_len;
	g_state.input_cursor_pos = 0;

	boxen_event_t alt_f = make_alt_char_event('f');

	boxen_repl_run_one_tick(&g_state, &alt_f);
	/* End of "one" -> position 3. */
	assert(g_state.input_cursor_pos == 3);

	boxen_repl_run_one_tick(&g_state, &alt_f);
	/* End of "two" -> position 7. */
	assert(g_state.input_cursor_pos == 7);

	boxen_repl_run_one_tick(&g_state, &alt_f);
	/* End of "three" -> position 13 (end of line). */
	assert(g_state.input_cursor_pos == 13);

	/* Further M-f at end stays at end. */
	boxen_repl_run_one_tick(&g_state, &alt_f);
	assert(g_state.input_cursor_pos == 13);

	/* Buffer is unchanged -- no letters got inserted. */
	assert(g_state.input_len == line_len);
	assert(strcmp(g_state.input_buf, line) == 0);

	teardown();
}

/* Regression guard for the #805 user-visible symptom: BEFORE the fix,
 * Option-b inserted a literal 'b' into the input buffer.  AFTER the fix,
 * the Alt modifier is honored and the cursor jumps instead -- this test
 * would have failed with the old behavior (input_buf would equal "b"). */
static void test_alt_b_does_not_insert_literal_b(void) {
	setup();
	assert(g_state.input_len == 0);

	boxen_event_t alt_b = make_alt_char_event('b');
	boxen_repl_run_one_tick(&g_state, &alt_b);

	assert(g_state.input_len == 0);
	assert(g_state.input_buf[0] == '\0');
	assert(g_state.input_cursor_pos == 0);

	teardown();
}

static void test_alt_f_does_not_insert_literal_f(void) {
	setup();
	assert(g_state.input_len == 0);

	boxen_event_t alt_f = make_alt_char_event('f');
	boxen_repl_run_one_tick(&g_state, &alt_f);

	assert(g_state.input_len == 0);
	assert(g_state.input_buf[0] == '\0');
	assert(g_state.input_cursor_pos == 0);

	teardown();
}

/* iTerm2 path: Option-Left and Option-Right arrive as ARROW_LEFT and
 * ARROW_RIGHT with the ALT modifier set, NOT as ESC-prefix printables. */
static void test_alt_left_jumps_word_back(void) {
	setup();

	const char *line = "alpha beta gamma";
	int line_len = (int)strlen(line);
	strncpy(g_state.input_buf, line, sizeof(g_state.input_buf) - 1);
	g_state.input_len        = line_len;
	g_state.input_cursor_pos = line_len;

	boxen_event_t alt_left = make_alt_key_event(BOXEN_KEY_LEFT);
	boxen_repl_run_one_tick(&g_state, &alt_left);
	/* Start of "gamma" -> position 11. */
	assert(g_state.input_cursor_pos == 11);

	boxen_repl_run_one_tick(&g_state, &alt_left);
	/* Start of "beta" -> position 6. */
	assert(g_state.input_cursor_pos == 6);

	teardown();
}

static void test_alt_right_jumps_word_forward(void) {
	setup();

	const char *line = "alpha beta gamma";
	int line_len = (int)strlen(line);
	strncpy(g_state.input_buf, line, sizeof(g_state.input_buf) - 1);
	g_state.input_len        = line_len;
	g_state.input_cursor_pos = 0;

	boxen_event_t alt_right = make_alt_key_event(BOXEN_KEY_RIGHT);
	boxen_repl_run_one_tick(&g_state, &alt_right);
	/* End of "alpha" -> position 5. */
	assert(g_state.input_cursor_pos == 5);

	boxen_repl_run_one_tick(&g_state, &alt_right);
	/* End of "beta" -> position 10. */
	assert(g_state.input_cursor_pos == 10);

	teardown();
}

/* Unmodified arrow keys must still do single-char movement (no regression
 * from the modifier branch). */
static void test_plain_left_still_moves_one_char(void) {
	setup();
	strncpy(g_state.input_buf, "abc", sizeof(g_state.input_buf) - 1);
	g_state.input_len        = 3;
	g_state.input_cursor_pos = 3;

	boxen_event_t left = make_key_event(BOXEN_KEY_LEFT);
	boxen_repl_run_one_tick(&g_state, &left);
	assert(g_state.input_cursor_pos == 2);

	boxen_repl_run_one_tick(&g_state, &left);
	assert(g_state.input_cursor_pos == 1);

	teardown();
}

/* Word boundaries with non-word separators: snake_case must split on '_',
 * matching bash readline's M-b/M-f behavior. */
static void test_word_jump_treats_underscore_as_separator(void) {
	setup();

	const char *line = "foo_bar_baz";
	int line_len = (int)strlen(line);
	strncpy(g_state.input_buf, line, sizeof(g_state.input_buf) - 1);
	g_state.input_len        = line_len;
	g_state.input_cursor_pos = line_len;

	boxen_event_t alt_b = make_alt_char_event('b');

	boxen_repl_run_one_tick(&g_state, &alt_b);
	assert(g_state.input_cursor_pos == 8);  /* start of "baz" */

	boxen_repl_run_one_tick(&g_state, &alt_b);
	assert(g_state.input_cursor_pos == 4);  /* start of "bar" */

	boxen_repl_run_one_tick(&g_state, &alt_b);
	assert(g_state.input_cursor_pos == 0);  /* start of "foo" */

	teardown();
}

/* Unrecognized Alt-printable (e.g. Option-x) must NOT insert the literal
 * char.  Without the swallow branch this would insert 'x' into input_buf,
 * which is the #805 bug class even though the user didn't see this exact
 * key combo (they typed Option-Left/Right, which yields 'b'/'f'). */
static void test_alt_other_letter_is_swallowed(void) {
	setup();
	assert(g_state.input_len == 0);

	boxen_event_t alt_x = make_alt_char_event('x');
	boxen_repl_run_one_tick(&g_state, &alt_x);

	assert(g_state.input_len == 0);
	assert(g_state.input_buf[0] == '\0');

	teardown();
}

/* -------------------------------------------------------------------------
 * main
 * ---------------------------------------------------------------------- */
int main(void) {
	TR_INIT("boxen_repl_tests");

	TR_RUN(test_input_line_accumulates_chars);
	TR_RUN(test_enter_dispatches_slash_command);
	TR_RUN(test_enter_dispatches_expression);
	TR_RUN(test_backspace_deletes_char);
	TR_RUN(test_scrollback_ring_drops_oldest_when_full);
	TR_RUN(test_stdout_capture_routes_to_scrollback);
	TR_RUN(test_launch_transport_heap_alloc_and_teardown);
	TR_RUN(test_history_up_arrow_loads_previous);
	TR_RUN(test_history_persists_via_file_roundtrip);
	TR_RUN(test_history_load_drops_overlong_lines);
	TR_RUN(test_history_down_arrow_preserves_input_when_empty);
	TR_RUN(test_tab_completes_single_slash_command);
	TR_RUN(test_tab_opens_popup_with_multiple_candidates);
	TR_RUN(test_tab_completes_odb_path_prefix);
	TR_RUN(test_escape_dismisses_popup_without_accept);
	TR_RUN(test_slash_key_defers_palette_open);
	TR_RUN(test_slash_then_enter_cancels_pending_palette);
	TR_RUN(test_slash_then_up_cancels_pending_palette);
	TR_RUN(test_slash_then_tab_cancels_pending_palette);
	TR_RUN(test_palette_close_returns_focus_to_repl_input);
	TR_RUN(test_palette_dispatch_hook_contract);
	TR_RUN(test_slash_mid_input_still_inserts);
	TR_RUN(test_stdout_drain_during_typing_does_not_corrupt_input);
	TR_RUN(test_long_output_does_not_deadlock_event_loop);
	TR_RUN(test_backslash_continuation_accumulates);
	TR_RUN(test_ctrl_c_in_multiline_discards_buffer);
	TR_RUN(test_ctrl_c_in_multiline_with_popup_open_discards_both);
	TR_RUN(test_slash_then_letter_within_window_inserts_both);
	TR_RUN(test_slash_then_timeout_opens_palette);
	TR_RUN(test_completion_popup_enter_accepts_selection_no_crash);
	TR_RUN(test_completion_popup_printable_letter_closes_popup_no_crash);
	TR_RUN(test_completion_popup_slash_with_empty_input_inserts_not_palette);
	TR_RUN(test_left_arrow_moves_cursor_left_within_buffer);
	TR_RUN(test_right_arrow_moves_cursor_right_within_buffer);
	TR_RUN(test_printable_mid_buffer_inserts_at_cursor);
	TR_RUN(test_backspace_mid_buffer_deletes_before_cursor);
	TR_RUN(test_ctrl_a_jumps_cursor_to_start);
	TR_RUN(test_ctrl_e_jumps_cursor_to_end);
	TR_RUN(test_cursor_stays_at_bounds);

	/* 2026-06-29 JES #803: output-pane scrollback */
	TR_RUN(test_pgup_increments_output_scroll_offset);
	TR_RUN(test_pgdn_decrements_offset_and_clamps_at_zero);
	TR_RUN(test_pgup_clamps_at_top_of_scrollback);
	TR_RUN(test_submit_resets_scroll_offset_to_zero);
	TR_RUN(test_append_while_scrolled_preserves_offset);
	TR_RUN(test_append_when_ring_full_advances_offset_to_pin_content);
	TR_RUN(test_append_when_pinned_to_bottom_does_not_bump_offset);

	/* 2026-06-29 JES #805: mouse-wheel scrolls output; Option-Left/Right
	 * jumps cursor by one word. */
	TR_RUN(test_wheel_up_scrolls_output_pane);
	TR_RUN(test_wheel_down_scrolls_back_toward_newest);
	TR_RUN(test_wheel_down_clamps_at_zero);
	TR_RUN(test_wheel_up_clamps_at_top_of_scrollback);
	TR_RUN(test_wheel_does_not_navigate_history);
	TR_RUN(test_alt_b_jumps_word_back);
	TR_RUN(test_alt_f_jumps_word_forward);
	TR_RUN(test_alt_b_does_not_insert_literal_b);
	TR_RUN(test_alt_f_does_not_insert_literal_f);
	TR_RUN(test_alt_left_jumps_word_back);
	TR_RUN(test_alt_right_jumps_word_forward);
	TR_RUN(test_plain_left_still_moves_one_char);
	TR_RUN(test_word_jump_treats_underscore_as_separator);
	TR_RUN(test_alt_other_letter_is_swallowed);

	TR_SUMMARY();
	return TR_EXIT_CODE();
}
