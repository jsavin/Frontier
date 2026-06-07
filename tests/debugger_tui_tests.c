/*
 * debugger_tui_tests.c -- behavioral unit tests for the B.0 TUI skeleton.
 *
 * Uses the boxen mock backend (no real terminal) and the tick-based API
 * exposed via debugger_tui_internal.h to drive the TUI one event at a time.
 *
 * Test harness: test_report.h TR_RUN/TR_SUMMARY/TR_EXIT_CODE (same as all
 * other Frontier unit tests).
 *
 * 2026-06-06 JES Phase B.0 #691
 */

#include <assert.h>
#include <stdbool.h>
#include <string.h>

/* boxen substrate */
#include "../frontier-cli/boxen/boxen.h"
#include "../frontier-cli/boxen/backend_mock.h"

/* TUI under test */
#include "../frontier-cli/debugger_tui_internal.h"

/* Test harness */
#include "test_report.h"

/* -------------------------------------------------------------------------
 * Helpers
 * ---------------------------------------------------------------------- */

/* Standard mock terminal size used across all tests */
#define TEST_WIDTH  80
#define TEST_HEIGHT 24

static tui_state_t g_state;

static void setup(void) {
	boxen_mock_reset(TEST_WIDTH, TEST_HEIGHT);
	boxen_init(boxen_mock_backend(), NULL, NULL);
	debugger_tui_state_init(&g_state, TEST_WIDTH, TEST_HEIGHT);
}

static void teardown(void) {
	debugger_tui_state_teardown(&g_state);
	boxen_shutdown();
}

/* -------------------------------------------------------------------------
 * Test: init_and_quit
 *
 * The spec (EXECUTION_PLAN.md B.0 Tests) says:
 *   1. boxen_mock_reset(80, 24)
 *   2. boxen_init(boxen_mock_backend(), NULL, NULL)
 *   3. inject 'q' keypress
 *   4. int rc = debugger_tui_run_one_tick(&state)
 *   5. assert(rc == TUI_QUIT)
 *   6. boxen_shutdown()
 *
 * This test verifies the minimal B.0 contract: the TUI enters, processes
 * a single 'q' event, and signals exit cleanly.
 * ---------------------------------------------------------------------- */

static void test_tui_init_and_quit(void) {
	setup();

	boxen_event_t ev;
	memset(&ev, 0, sizeof(ev));
	ev.type     = BOXEN_EV_KEY;
	ev.key.key  = BOXEN_KEY_NONE;
	ev.key.ch   = 'q';
	ev.key.mod  = BOXEN_MOD_NONE;

	int rc = debugger_tui_run_one_tick(&g_state, &ev);
	assert(rc == TUI_QUIT);

	teardown();
}

/* -------------------------------------------------------------------------
 * Test: escape_quits
 *
 * Escape key must also signal exit (plan B.6 spec; wired in B.0).
 * ---------------------------------------------------------------------- */

static void test_escape_quits(void) {
	setup();

	boxen_event_t ev;
	memset(&ev, 0, sizeof(ev));
	ev.type    = BOXEN_EV_KEY;
	ev.key.key = BOXEN_KEY_ESCAPE;
	ev.key.ch  = 0;
	ev.key.mod = BOXEN_MOD_NONE;

	int rc = debugger_tui_run_one_tick(&g_state, &ev);
	assert(rc == TUI_QUIT);

	teardown();
}

/* -------------------------------------------------------------------------
 * Test: ctrl_c_quits
 *
 * Ctrl-C must also signal exit.
 * ---------------------------------------------------------------------- */

static void test_ctrl_c_quits(void) {
	setup();

	boxen_event_t ev;
	memset(&ev, 0, sizeof(ev));
	ev.type    = BOXEN_EV_KEY;
	ev.key.key = BOXEN_KEY_CTRL_C;
	ev.key.ch  = 0;
	ev.key.mod = BOXEN_MOD_NONE;

	int rc = debugger_tui_run_one_tick(&g_state, &ev);
	assert(rc == TUI_QUIT);

	teardown();
}

/* -------------------------------------------------------------------------
 * Test: non_quit_key_continues
 *
 * An arbitrary printable key (not 'q', not Escape, not Ctrl-C) must return
 * TUI_CONTINUE so the event loop keeps running.
 * ---------------------------------------------------------------------- */

static void test_non_quit_key_continues(void) {
	setup();

	boxen_event_t ev;
	memset(&ev, 0, sizeof(ev));
	ev.type    = BOXEN_EV_KEY;
	ev.key.key = BOXEN_KEY_NONE;
	ev.key.ch  = 'a';
	ev.key.mod = BOXEN_MOD_NONE;

	int rc = debugger_tui_run_one_tick(&g_state, &ev);
	assert(rc == TUI_CONTINUE);

	teardown();
}

/* -------------------------------------------------------------------------
 * Test: layout_windows_open
 *
 * After state_init(), all three windows (script, stack, footer) must be
 * non-NULL. This is a structural correctness check, not source inspection:
 * it verifies that the layout construction actually worked.
 * ---------------------------------------------------------------------- */

static void test_layout_windows_open(void) {
	setup();

	assert(g_state.script_win != NULL);
	assert(g_state.stack_win  != NULL);
	assert(g_state.footer_win != NULL);

	teardown();
}

/* -------------------------------------------------------------------------
 * Test: footer_renders_quit_hint
 *
 * After state_init() and an initial present(), the footer must contain the
 * text "q:quit" (the minimal keybind hint for B.0).
 * ---------------------------------------------------------------------- */

static void test_footer_renders_quit_hint(void) {
	setup();

	/* Drive a present so the mock backend captures the cell output */
	boxen_present();

	assert(boxen_mock_has_text("q:quit"));

	teardown();
}

/* -------------------------------------------------------------------------
 * main
 * ---------------------------------------------------------------------- */

int main(void) {
	TR_INIT("debugger_tui_tests");

	TR_RUN(test_tui_init_and_quit);
	TR_RUN(test_escape_quits);
	TR_RUN(test_ctrl_c_quits);
	TR_RUN(test_non_quit_key_continues);
	TR_RUN(test_layout_windows_open);
	TR_RUN(test_footer_renders_quit_hint);

	TR_SUMMARY();
	return TR_EXIT_CODE();
}
