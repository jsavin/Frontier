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
 * Phase B.1 tests -- script pane and source loading.
 *
 * 2026-06-07 JES Phase B.1 #691
 *
 * Three behavioral tests per EXECUTION_PLAN.md B.1 "Tests" section:
 *   test_script_pane_draws_source        -- populate state, verify cell render
 *   test_script_pane_autoscrolls         -- long source, current near end
 *   test_load_source_parses_response     -- inject JSON via write_line, check state
 * ---------------------------------------------------------------------- */

/* Helper: free script_lines array (mirrors teardown logic in debugger_tui.c) */
static void free_script_lines(tui_state_t *s) {
	if (s->script_lines != NULL) {
		for (int i = 0; i < s->script_line_count; i++) {
			free(s->script_lines[i]);
		}
		free(s->script_lines);
		s->script_lines      = NULL;
		s->script_line_count = 0;
	}
}

/* Helper: populate state->script_lines with N synthetic lines.
 * Each line is "line N" (e.g. "line 1", "line 2", ...).
 * Caller must call free_script_lines() when done. */
static void fill_script_lines(tui_state_t *s, int count) {
	free_script_lines(s);
	s->script_lines = (char **)malloc((size_t)count * sizeof(char *));
	assert(s->script_lines != NULL);
	s->script_line_count = count;
	for (int i = 0; i < count; i++) {
		char buf[32];
		snprintf(buf, sizeof(buf), "line %d", i + 1);
		s->script_lines[i] = strdup(buf);
		assert(s->script_lines[i] != NULL);
	}
}

/* -------------------------------------------------------------------------
 * test_script_pane_draws_source
 *
 * Spec (EXECUTION_PLAN.md B.1 Tests):
 *   Setup: boxen_mock_reset(80, 24); open script window at {0,0,48,22};
 *   populate state.script_lines with 5 synthetic lines; set current_line = 3.
 *   GREEN assertion: boxen_mock_has_text("line 1") and boxen_mock_has_text("line 3");
 *   verify row 2 (content row for line 3) has REVERSE attribute on at least one cell.
 *
 * Off-by-one sentinel (EXECUTION_PLAN.md B.1 Sentinels):
 *   ODB line numbers are 1-based; boxen content rows are 0-based.
 *   current_line == 3 -> content row 2.
 *   The test checks the cell at content row 2, not row 3.
 * ---------------------------------------------------------------------- */

static void test_script_pane_draws_source(void) {
	setup();

	fill_script_lines(&g_state, 5);
	g_state.current_line = 3;   /* 1-based: line 3 is at content row 2 */

	/* Drive a present to flush draw callbacks to the cell grid */
	boxen_window_invalidate(g_state.script_win);
	boxen_present();

	/* Text content check */
	assert(boxen_mock_has_text("line 1"));
	assert(boxen_mock_has_text("line 3"));

	/* The script window occupies columns 0..47 (60% of 80 = 48 wide).
	 * Content area starts at col 1, row 1 (inside border).
	 * Content row 2 (line 3) is at screen row 1 (border) + 2 = row 3.
	 * Walk the script window's horizontal span looking for REVERSE. */
	bool found_reverse = false;
	boxen_rect_t r = boxen_window_get_rect(g_state.script_win);
	/* Content row 2 is screen row (r.y + 1 + 2) due to top border */
	int screen_row = r.y + 1 + 2;
	for (int col = r.x; col < r.x + r.w && !found_reverse; col++) {
		const boxen_mock_cell_t *cell = boxen_mock_cell_at(col, screen_row);
		if (cell != NULL && (cell->attr & BOXEN_ATTR_REVERSE)) {
			found_reverse = true;
		}
	}
	assert(found_reverse);

	free_script_lines(&g_state);
	teardown();
}

/* -------------------------------------------------------------------------
 * test_script_pane_autoscrolls_to_current_line
 *
 * Spec: Populate 40 lines; set current_line = 35; trigger draw.
 * After draw: verify the scroll position reflects auto-scroll to the current
 * line.
 *
 * Deviation from plan spec (EXECUTION_PLAN.md B.1 Tests): the plan asserts
 * "scroll_y >= 20", but with the actual 80x24 layout the script window's
 * content viewport is 21 rows tall. With 40-line content the maximum scroll
 * is max_y = 40 - 21 = 19. A scroll_y of 19 means line 40 is at the bottom
 * of the viewport -- line 35 is certainly visible (at row 34-19=15 from top).
 * The achievable range for ensure_visible(row=34) in a 21-row viewport is
 * scroll_y in [14, 19]. We assert scroll_y >= 14 (line 35 is scrolled INTO
 * view -- the minimum ensure_visible guarantee) rather than the
 * unobtainable >= 20 from the plan.
 * ---------------------------------------------------------------------- */

static void test_script_pane_autoscrolls_to_current_line(void) {
	setup();

	fill_script_lines(&g_state, 40);
	g_state.current_line = 35;

	boxen_window_set_content_size(g_state.script_win, 46, 40);
	boxen_window_invalidate(g_state.script_win);
	boxen_present();

	int sx = 0, sy = 0;
	boxen_window_get_scroll(g_state.script_win, &sx, &sy);

	/* Line 35 (content row 34, 0-based) should be visible after auto-scroll.
	 * Minimum: scroll_y >= 14 (so that row 34 falls within [scroll_y,
	 * scroll_y + viewport_height - 1]). */
	assert(sy >= 14);

	free_script_lines(&g_state);
	teardown();
}

/* -------------------------------------------------------------------------
 * test_load_source_parses_response
 *
 * Spec: Feed a synthetic debug/getSource response JSON through the TUI's
 * write_line. Verify state.script_line_count == 5 and state.script_lines[0]
 * matches line 1 text.
 *
 * The response shape (code-verified at debug_handler.c:2082 per plan 2.1):
 *   {"id":1,"result":{"script":"test.script","currentLine":2,
 *     "lines":[
 *       {"num":1,"text":"local x","breakpoint":false},
 *       {"num":2,"text":"  x := 42","breakpoint":false,"current":true},
 *       {"num":3,"text":"  msg(x)","breakpoint":false},
 *       {"num":4,"text":"local y","breakpoint":false},
 *       {"num":5,"text":"  return y","breakpoint":false}
 *     ]}}
 * ---------------------------------------------------------------------- */

static void test_load_source_parses_response(void) {
	setup();

	/* Construct the synthetic getSource response */
	const char *resp =
		"{\"id\":1,\"result\":{"
		"\"script\":\"test.script\","
		"\"currentLine\":2,"
		"\"lines\":["
		"{\"num\":1,\"text\":\"local x\",\"breakpoint\":false},"
		"{\"num\":2,\"text\":\"  x := 42\",\"breakpoint\":false,\"current\":true},"
		"{\"num\":3,\"text\":\"  msg(x)\",\"breakpoint\":false},"
		"{\"num\":4,\"text\":\"local y\",\"breakpoint\":false},"
		"{\"num\":5,\"text\":\"  return y\",\"breakpoint\":false}"
		"]}}";

	/* Deliver the response through the stub transport's write_line.
	 * This simulates what op_dispatch would do after a debug/getSource request. */
	assert(g_state.transport != NULL);
	g_state.transport->write_line(g_state.transport->ctx, resp, strlen(resp));

	/* Verify state was updated */
	assert(g_state.script_line_count == 5);
	assert(g_state.script_lines != NULL);
	assert(strcmp(g_state.script_lines[0], "local x") == 0);

	/* Verify current_line is set from currentLine in response */
	assert(g_state.current_line == 2);

	/* Verify script_path is set */
	assert(strcmp(g_state.script_path, "test.script") == 0);

	free_script_lines(&g_state);
	teardown();
}

/* -------------------------------------------------------------------------
 * test_store_source_preserves_current_line_when_field_absent
 *
 * Regression test for P1-D (PR #737 round 1).
 *
 * Sequence:
 *   1. Inject a debug/suspended notification that sets current_line = 5.
 *   2. Inject a debug/getSource response that has NO "currentLine" field.
 *   3. Assert current_line is still 5 (not reset to -1).
 *
 * Before the fix, tui_store_source unconditionally evaluated
 *   s->current_line = cJSON_IsNumber(curline_j) ? ... : -1;
 * so an absent "currentLine" field would silently zero the marker and disable
 * autoscroll.  The fix guards the update: only assign if the field is present.
 *
 * 2026-06-07 JES Phase B.1 #737 round 1 P1-D
 * ---------------------------------------------------------------------- */

static void test_store_source_preserves_current_line_when_field_absent(void) {
	setup();

	/* Step 1: inject a debug/suspended notification to set current_line = 5 */
	const char *suspended =
		"{\"id\":null,\"op\":\"debug/suspended\","
		"\"params\":{\"threadId\":1,\"line\":5,\"script\":\"test.path\"}}";
	assert(g_state.transport != NULL);
	g_state.transport->write_line(g_state.transport->ctx,
	                              suspended, strlen(suspended));

	/* Verify the suspended notification took effect */
	assert(g_state.current_line == 5);

	/* Step 2: inject a debug/getSource response WITHOUT a "currentLine" field */
	const char *src_no_curline =
		"{\"id\":2,\"result\":{"
		"\"script\":\"test.path\","
		"\"lines\":["
		"{\"num\":1,\"text\":\"local x\",\"breakpoint\":false},"
		"{\"num\":2,\"text\":\"  x := 42\",\"breakpoint\":false},"
		"{\"num\":3,\"text\":\"  return x\",\"breakpoint\":false}"
		"]}}";
	g_state.transport->write_line(g_state.transport->ctx,
	                              src_no_curline, strlen(src_no_curline));

	/* Step 3: current_line must be unchanged (5, not -1) */
	assert(g_state.current_line == 5);

	/* Source lines should have been populated normally */
	assert(g_state.script_line_count == 3);

	free_script_lines(&g_state);
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

	/* B.1 tests */
	TR_RUN(test_script_pane_draws_source);
	TR_RUN(test_script_pane_autoscrolls_to_current_line);
	TR_RUN(test_load_source_parses_response);

	/* B.1 round-1 review regression tests */
	TR_RUN(test_store_source_preserves_current_line_when_field_absent);

	TR_SUMMARY();
	return TR_EXIT_CODE();
}
