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
	assert(g_state.input_cursor == 3);

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
	g_state.input_cursor = (int)strlen(g_state.input_buf);

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
	g_state.input_cursor = (int)strlen(g_state.input_buf);

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
	g_state.input_cursor = 4;

	boxen_event_t bs = make_key_event(BOXEN_KEY_BACKSPACE);

	boxen_repl_run_one_tick(&g_state, &bs);
	assert(strcmp(g_state.input_buf, "abc") == 0);
	assert(g_state.input_cursor == 3);

	boxen_repl_run_one_tick(&g_state, &bs);
	boxen_repl_run_one_tick(&g_state, &bs);
	boxen_repl_run_one_tick(&g_state, &bs);
	assert(strcmp(g_state.input_buf, "") == 0);
	assert(g_state.input_cursor == 0);

	/* Extra backspace on empty: no underflow */
	boxen_repl_run_one_tick(&g_state, &bs);
	assert(strcmp(g_state.input_buf, "") == 0);
	assert(g_state.input_cursor == 0);

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
#include <unistd.h>   /* pipe, write, close */
#include <fcntl.h>    /* fcntl, F_SETFL, O_NONBLOCK */

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

	TR_SUMMARY();
	return TR_EXIT_CODE();
}
