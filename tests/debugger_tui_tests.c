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
 * Test: resize_rebuilds_layout
 *
 * Injecting a BOXEN_EV_RESIZE event must:
 *   1. Return TUI_CONTINUE (not TUI_QUIT).
 *   2. Leave all three windows non-NULL (rebuilt, not destroyed).
 *   3. Update window geometry to the new dimensions: the footer must sit at
 *      row (new_h - 1) and span the full new width.
 *   4. Produce fresh window pointers (the old windows were closed and new
 *      ones were opened for the new size).
 *
 * 2026-06-06 JES Phase B.0 #734 round 1 P1-2 behavioral test.
 * ---------------------------------------------------------------------- */

static void test_resize_rebuilds_layout(void) {
	setup();

	/* Verify the initial 80x24 footer geometry before resize */
	boxen_rect_t before = boxen_window_get_rect(g_state.footer_win);
	assert(before.y == 23);   /* th - 1 == 24 - 1 */
	assert(before.w == 80);

	/* Tell the mock backend that the terminal is now 120x40 */
	boxen_mock_width_set(120);
	boxen_mock_height_set(40);

	/* Inject a resize event */
	boxen_event_t ev;
	memset(&ev, 0, sizeof(ev));
	ev.type      = BOXEN_EV_RESIZE;
	ev.resize.w  = 120;
	ev.resize.h  = 40;

	int rc = debugger_tui_run_one_tick(&g_state, &ev);

	/* Must return TUI_CONTINUE (resize is not a quit signal) */
	assert(rc == TUI_CONTINUE);

	/* All three windows must be non-NULL after the rebuild */
	assert(g_state.script_win != NULL);
	assert(g_state.stack_win  != NULL);
	assert(g_state.footer_win != NULL);

	/* Footer geometry must reflect the new terminal size.
	 * Note: pointer identity is NOT checked -- the allocator may return the
	 * same address after close+open. The geometry change is the behavioral
	 * contract: footer moved to row 39 and spans 120 columns. */
	boxen_rect_t footer_rect = boxen_window_get_rect(g_state.footer_win);
	assert(footer_rect.y == 39);   /* th - 1 == 40 - 1 */
	assert(footer_rect.w == 120);  /* full new width */
	assert(footer_rect.h == 1);    /* still one row tall */

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
	TR_RUN(test_resize_rebuilds_layout);

	TR_SUMMARY();
	return TR_EXIT_CODE();
}
