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

/* =========================================================================
 * Phase B.2 tests -- stack/locals pane.
 *
 * 2026-06-07 JES Phase B.2 #691
 *
 * Three behavioral tests per EXECUTION_PLAN.md B.2 "Tests" section:
 *   test_stack_pane_draws_frames          -- 3 frames render with script names
 *   test_frame_selection_updates_script_pane -- DOWN arrow advances selected_frame
 *                                              and script path is updated
 *   test_locals_render                    -- name = value pairs appear in pane
 *
 * Additional robustness tests (applying B.1 round-1 lessons):
 *   test_getstack_response_parses         -- write_line handles debug/getStack JSON
 *   test_getlocals_response_parses        -- write_line handles debug/getLocals JSON
 *   test_stack_frame_count_capped         -- oversized frame array is truncated
 *   test_locals_count_capped              -- oversized locals array is truncated
 *   test_suspended_fires_getstack_getlocals -- debug/suspended triggers load of
 *                                             both stack and locals state
 * ========================================================================= */

/* Helper: populate state frame fields with N synthetic frames.
 * Frames: level=1..N, script="frame.script.N", line=N*10.
 * selected_frame is set to the innermost (N-1, 0-based). */
static void fill_frames(tui_state_t *s, int count) {
	int cap = count < TUI_MAX_FRAMES ? count : TUI_MAX_FRAMES;
	s->frame_count    = cap;
	s->selected_frame = cap - 1; /* innermost */
	for (int i = 0; i < cap; i++) {
		snprintf(s->frame_scripts[i], sizeof(s->frame_scripts[i]),
		         "frame.script.%d", i + 1);
		s->frame_lines[i] = (long)(i + 1) * 10;
	}
}

/* Helper: free heap locals arrays. */
static void free_locals(tui_state_t *s) {
	if (s->local_names != NULL) {
		for (int i = 0; i < s->local_count; i++) {
			free(s->local_names[i]);
			free(s->local_values[i]);
		}
		free(s->local_names);
		free(s->local_values);
		s->local_names  = NULL;
		s->local_values = NULL;
		s->local_count  = 0;
	}
}

/* -------------------------------------------------------------------------
 * test_stack_pane_draws_frames
 *
 * Spec: Populate state.frame_scripts with 3 synthetic frames; verify three
 * rows render with correct script names in the stack pane.
 * ---------------------------------------------------------------------- */
static void test_stack_pane_draws_frames(void) {
	setup();

	fill_frames(&g_state, 3);

	boxen_window_invalidate(g_state.stack_win);
	boxen_present();

	/* All three frame script names must appear somewhere in the cell grid */
	assert(boxen_mock_has_text("frame.script.1"));
	assert(boxen_mock_has_text("frame.script.2"));
	assert(boxen_mock_has_text("frame.script.3"));

	teardown();
}

/* -------------------------------------------------------------------------
 * test_frame_selection_updates_script_pane
 *
 * Spec: Start with innermost frame selected (selected_frame = frame_count-1).
 * Inject UP arrow on the stack pane (moving toward outermost).
 * Verify selected_frame decreased by 1 and script pane's script_path was
 * updated to the newly selected frame's script.
 *
 * Frame ordering in B.2: outermost first (matching debug/getStack order).
 * selected_frame = 0 is the outermost; selected_frame = frame_count-1 is
 * the innermost. UP arrow moves toward outermost (lower index). DOWN moves
 * toward innermost (higher index).
 *
 * We start at selected_frame = 2 (innermost of 3 frames), inject UP, and
 * expect selected_frame = 1 and script_path = "frame.script.2".
 * ---------------------------------------------------------------------- */
static void test_frame_selection_updates_script_pane(void) {
	setup();

	fill_frames(&g_state, 3);
	/* Start at innermost (index 2) */
	g_state.selected_frame = 2;
	/* Prime script_path with innermost so we can detect the change */
	strncpy(g_state.script_path, g_state.frame_scripts[2],
	        sizeof(g_state.script_path) - 1);
	g_state.script_path[sizeof(g_state.script_path) - 1] = '\0';

	/* Inject UP arrow directed at the stack pane */
	boxen_event_t ev;
	memset(&ev, 0, sizeof(ev));
	ev.type    = BOXEN_EV_KEY;
	ev.key.key = BOXEN_KEY_UP;
	ev.key.ch  = 0;
	ev.key.mod = BOXEN_MOD_NONE;

	/* Route the event through the stack pane's input callback by dispatching
	 * directly to the stack window rather than via run_one_tick (which sends
	 * to the focused window, currently script_win). */
	boxen_window_focus(g_state.stack_win);
	int rc = debugger_tui_run_one_tick(&g_state, &ev);

	assert(rc == TUI_CONTINUE);
	assert(g_state.selected_frame == 1);
	/* script_path must now reflect the selected frame's script */
	assert(strcmp(g_state.script_path, "frame.script.2") == 0);

	teardown();
}

/* -------------------------------------------------------------------------
 * test_locals_render
 *
 * Spec: Populate state.local_names = ["x","y"], state.local_values = ["42","true"].
 * Verify right pane content contains "x = 42" and "y = true".
 * ---------------------------------------------------------------------- */
static void test_locals_render(void) {
	setup();

	/* Build two locals manually (not via fill_locals to match exact plan values) */
	free_locals(&g_state);
	g_state.local_names  = (char **)calloc(2, sizeof(char *));
	g_state.local_values = (char **)calloc(2, sizeof(char *));
	assert(g_state.local_names != NULL && g_state.local_values != NULL);
	g_state.local_count     = 2;
	g_state.local_names[0]  = strdup("x");
	g_state.local_values[0] = strdup("42");
	g_state.local_names[1]  = strdup("y");
	g_state.local_values[1] = strdup("true");
	assert(g_state.local_names[0] && g_state.local_values[0]);
	assert(g_state.local_names[1] && g_state.local_values[1]);

	boxen_window_invalidate(g_state.stack_win);
	boxen_present();

	assert(boxen_mock_has_text("x = 42"));
	assert(boxen_mock_has_text("y = true"));

	free_locals(&g_state);
	teardown();
}

/* -------------------------------------------------------------------------
 * test_getstack_response_parses
 *
 * Feed a synthetic debug/getStack response JSON through write_line.
 * Verify frame_count == 2 and frame_scripts[0] == "outer.script".
 *
 * Actual response shape from debug_handler.c:2011 (code-verified):
 *   {"id":1,"result":{"frames":[
 *     {"level":1,"script":"outer.script","line":10},
 *     {"level":2,"script":"inner.script","line":3}
 *   ]},"success":true}
 * ---------------------------------------------------------------------- */
static void test_getstack_response_parses(void) {
	setup();

	const char *resp =
		"{\"id\":1,\"result\":{\"frames\":["
		"{\"level\":1,\"script\":\"outer.script\",\"line\":10},"
		"{\"level\":2,\"script\":\"inner.script\",\"line\":3}"
		"]},\"success\":true}";

	assert(g_state.transport != NULL);
	g_state.transport->write_line(g_state.transport->ctx, resp, strlen(resp));

	assert(g_state.frame_count == 2);
	assert(strcmp(g_state.frame_scripts[0], "outer.script") == 0);
	assert(g_state.frame_lines[0] == 10);
	assert(strcmp(g_state.frame_scripts[1], "inner.script") == 0);
	assert(g_state.frame_lines[1] == 3);

	teardown();
}

/* -------------------------------------------------------------------------
 * test_getlocals_response_parses
 *
 * Feed a synthetic debug/getLocals response JSON through write_line.
 * Verify local_count == 2 and local_names[0] == "myVar".
 *
 * Actual response shape from debug_handler.c:1900 (code-verified):
 *   {"id":2,"result":{"locals":[
 *     {"name":"myVar","value":"99","type":"integer"},
 *     {"name":"flag","value":"true","type":"boolean"}
 *   ],"script":"test.script","line":5},"success":true}
 * ---------------------------------------------------------------------- */
static void test_getlocals_response_parses(void) {
	setup();

	const char *resp =
		"{\"id\":2,\"result\":{\"locals\":["
		"{\"name\":\"myVar\",\"value\":\"99\",\"type\":\"integer\"},"
		"{\"name\":\"flag\",\"value\":\"true\",\"type\":\"boolean\"}"
		"],\"script\":\"test.script\",\"line\":5},\"success\":true}";

	assert(g_state.transport != NULL);
	g_state.transport->write_line(g_state.transport->ctx, resp, strlen(resp));

	assert(g_state.local_count == 2);
	assert(g_state.local_names  != NULL);
	assert(g_state.local_values != NULL);
	assert(strcmp(g_state.local_names[0],  "myVar") == 0);
	assert(strcmp(g_state.local_values[0], "99")    == 0);
	assert(strcmp(g_state.local_names[1],  "flag")  == 0);
	assert(strcmp(g_state.local_values[1], "true")  == 0);

	free_locals(&g_state);
	teardown();
}

/* -------------------------------------------------------------------------
 * test_stack_frame_count_capped
 *
 * Feed a debug/getStack response with more than TUI_MAX_FRAMES frames.
 * Verify frame_count is capped at TUI_MAX_FRAMES.
 *
 * Uses a small synthetic count: TUI_MAX_FRAMES + 5 frames.
 * Building a JSON string of 200+ frames inline is impractical; instead we
 * build it dynamically on the stack.
 * ---------------------------------------------------------------------- */
static void test_stack_frame_count_capped(void) {
	setup();

	/* Build a JSON array with TUI_MAX_FRAMES + 5 frame objects */
	int overflow_count = TUI_MAX_FRAMES + 5;
	/* Each frame is ~50 bytes; total ~10250 bytes for 205 frames. Use heap. */
	int bufsize = overflow_count * 60 + 64;
	char *buf   = (char *)malloc((size_t)bufsize);
	assert(buf != NULL);

	int pos = 0;
	pos += snprintf(buf + pos, (size_t)(bufsize - pos),
	                "{\"id\":3,\"result\":{\"frames\":[");
	for (int i = 0; i < overflow_count; i++) {
		if (i > 0) pos += snprintf(buf + pos, (size_t)(bufsize - pos), ",");
		pos += snprintf(buf + pos, (size_t)(bufsize - pos),
		                "{\"level\":%d,\"script\":\"s%d\",\"line\":%d}",
		                i + 1, i + 1, i + 1);
	}
	pos += snprintf(buf + pos, (size_t)(bufsize - pos),
	                "]},\"success\":true}");

	g_state.transport->write_line(g_state.transport->ctx, buf, (size_t)pos);
	free(buf);

	assert(g_state.frame_count == TUI_MAX_FRAMES);

	teardown();
}

/* -------------------------------------------------------------------------
 * test_locals_count_capped
 *
 * Feed a debug/getLocals response with more than TUI_MAX_LOCALS locals.
 * Verify local_count is capped at TUI_MAX_LOCALS.
 * ---------------------------------------------------------------------- */
static void test_locals_count_capped(void) {
	setup();

	int overflow_count = TUI_MAX_LOCALS + 5;
	int bufsize = overflow_count * 60 + 128;
	char *buf   = (char *)malloc((size_t)bufsize);
	assert(buf != NULL);

	int pos = 0;
	pos += snprintf(buf + pos, (size_t)(bufsize - pos),
	                "{\"id\":4,\"result\":{\"locals\":[");
	for (int i = 0; i < overflow_count; i++) {
		if (i > 0) pos += snprintf(buf + pos, (size_t)(bufsize - pos), ",");
		pos += snprintf(buf + pos, (size_t)(bufsize - pos),
		                "{\"name\":\"v%d\",\"value\":\"%d\",\"type\":\"integer\"}",
		                i, i);
	}
	pos += snprintf(buf + pos, (size_t)(bufsize - pos),
	                "],\"script\":\"s\",\"line\":1},\"success\":true}");

	g_state.transport->write_line(g_state.transport->ctx, buf, (size_t)pos);
	free(buf);

	assert(g_state.local_count == TUI_MAX_LOCALS);

	free_locals(&g_state);
	teardown();
}

/* -------------------------------------------------------------------------
 * test_suspended_fires_getstack_getlocals
 *
 * A debug/suspended notification must trigger loading of both frame stack
 * and locals state.  In B.2, the TUI stores the pending_thread_id from the
 * suspended notification; the test verifies that the fields are set on the
 * state after delivering the notification and then delivering synthetic
 * getStack + getLocals responses through the same write_line path.
 *
 * This mirrors the B.1 test_load_source_parses_response pattern: the test
 * sends the responses directly through write_line rather than going through
 * op_dispatch (which would require a live runtime).
 * ---------------------------------------------------------------------- */
static void test_suspended_fires_getstack_getlocals(void) {
	setup();

	/* Step 1: deliver debug/suspended to establish pending_thread_id */
	const char *suspended =
		"{\"id\":null,\"op\":\"debug/suspended\","
		"\"params\":{\"threadId\":42,\"line\":7,\"script\":\"my.script\"}}";
	g_state.transport->write_line(g_state.transport->ctx,
	                              suspended, strlen(suspended));
	assert(g_state.pending_thread_id == 42);

	/* Step 2: deliver getStack response */
	const char *stack_resp =
		"{\"id\":10,\"result\":{\"frames\":["
		"{\"level\":1,\"script\":\"my.script\",\"line\":7}"
		"]},\"success\":true}";
	g_state.transport->write_line(g_state.transport->ctx,
	                              stack_resp, strlen(stack_resp));
	assert(g_state.frame_count == 1);
	assert(strcmp(g_state.frame_scripts[0], "my.script") == 0);

	/* Step 3: deliver getLocals response */
	const char *locals_resp =
		"{\"id\":11,\"result\":{\"locals\":["
		"{\"name\":\"z\",\"value\":\"999\",\"type\":\"integer\"}"
		"],\"script\":\"my.script\",\"line\":7},\"success\":true}";
	g_state.transport->write_line(g_state.transport->ctx,
	                              locals_resp, strlen(locals_resp));
	assert(g_state.local_count == 1);
	assert(strcmp(g_state.local_names[0], "z") == 0);

	free_locals(&g_state);
	teardown();
}

/* =========================================================================
 * Phase B.2 round-1 review regression tests (#739).
 *
 * 2026-06-07 JES Phase B.2 #739 round 1
 *
 * Four behavioral tests covering the three gate findings:
 *   P1 (bar-raiser):
 *     test_store_stack_clears_on_empty_array   -- frames=[] zeroes frame_count
 *     test_store_locals_clears_on_empty_array  -- locals=[] zeroes local_count
 *   P2 (security):
 *     test_store_locals_caps_long_value        -- 50KB value clamped to TUI_LOCAL_VALUE_MAX
 *   P2 (bar-raiser):
 *     test_frame_select_clears_source_when_script_differs -- source blanked on cross-script select
 * ========================================================================= */

/* -------------------------------------------------------------------------
 * test_store_stack_clears_on_empty_array
 *
 * Sequence:
 *   1. Load frames [a, b, c] via write_line (frame_count == 3).
 *   2. Inject {"result":{"frames":[]}} via write_line.
 *   3. Assert frame_count == 0.
 *
 * Before the fix, the count <= 0 early-return in tui_store_stack left the
 * old frame state on screen even when the runtime reported an empty stack.
 * ---------------------------------------------------------------------- */
static void test_store_stack_clears_on_empty_array(void) {
	setup();

	/* Step 1: load three frames */
	const char *three_frames =
		"{\"id\":1,\"result\":{\"frames\":["
		"{\"level\":1,\"script\":\"a.script\",\"line\":1},"
		"{\"level\":2,\"script\":\"b.script\",\"line\":2},"
		"{\"level\":3,\"script\":\"c.script\",\"line\":3}"
		"]},\"success\":true}";
	g_state.transport->write_line(g_state.transport->ctx,
	                              three_frames, strlen(three_frames));
	assert(g_state.frame_count == 3);  /* RED: was 3, must stay 3 after load */

	/* Step 2: inject empty frames array -- runtime popped all frames */
	const char *empty_frames =
		"{\"id\":2,\"result\":{\"frames\":[]},\"success\":true}";
	g_state.transport->write_line(g_state.transport->ctx,
	                              empty_frames, strlen(empty_frames));

	/* Step 3: state must reflect the empty stack */
	assert(g_state.frame_count == 0);

	teardown();
}

/* -------------------------------------------------------------------------
 * test_store_locals_clears_on_empty_array
 *
 * Sequence:
 *   1. Load locals [x, y] via write_line (local_count == 2).
 *   2. Inject {"result":{"locals":[]}} via write_line.
 *   3. Assert local_count == 0.
 *
 * Before the fix, the count <= 0 early-return in tui_store_locals left the
 * old local state on screen when the runtime reported a frame with no locals.
 * ---------------------------------------------------------------------- */
static void test_store_locals_clears_on_empty_array(void) {
	setup();

	/* Step 1: load two locals */
	const char *two_locals =
		"{\"id\":3,\"result\":{\"locals\":["
		"{\"name\":\"x\",\"value\":\"1\",\"type\":\"integer\"},"
		"{\"name\":\"y\",\"value\":\"2\",\"type\":\"integer\"}"
		"],\"script\":\"s\",\"line\":1},\"success\":true}";
	g_state.transport->write_line(g_state.transport->ctx,
	                              two_locals, strlen(two_locals));
	assert(g_state.local_count == 2);  /* RED: was 2, must stay 2 after load */

	/* Step 2: inject empty locals array -- selected frame has no locals */
	const char *empty_locals =
		"{\"id\":4,\"result\":{\"locals\":[],"
		"\"script\":\"s\",\"line\":1},\"success\":true}";
	g_state.transport->write_line(g_state.transport->ctx,
	                              empty_locals, strlen(empty_locals));

	/* Step 3: state must reflect the empty locals */
	assert(g_state.local_count == 0);

	teardown();
}

/* -------------------------------------------------------------------------
 * test_store_locals_caps_long_value
 *
 * Inject a local with a 50KB value string.
 * Assert:
 *   - local_count == 1 (parsed successfully)
 *   - strlen(local_values[0]) == TUI_LOCAL_VALUE_MAX (clamped, not full 50KB)
 *   - No crash.
 *
 * Before the fix, strdup accepted arbitrary length; a hostile/malformed
 * debug/getLocals with a multi-MB value could drive heap to 500 * len(value).
 * ---------------------------------------------------------------------- */
static void test_store_locals_caps_long_value(void) {
	setup();

	/* Build a 50KB value string filled with 'A' */
	const int VALUE_LEN = 50 * 1024;
	char *big_value = (char *)malloc((size_t)(VALUE_LEN + 1));
	assert(big_value != NULL);
	memset(big_value, 'A', (size_t)VALUE_LEN);
	big_value[VALUE_LEN] = '\0';

	/* Build the JSON: {"id":5,"result":{"locals":[{"name":"x","value":"AAA..."}...]}} */
	int bufsize = VALUE_LEN + 128;
	char *buf   = (char *)malloc((size_t)bufsize);
	assert(buf != NULL);
	int pos = snprintf(buf, (size_t)bufsize,
	                   "{\"id\":5,\"result\":{\"locals\":["
	                   "{\"name\":\"x\",\"value\":\"%s\",\"type\":\"string\"}"
	                   "],\"script\":\"s\",\"line\":1},\"success\":true}",
	                   big_value);
	assert(pos > 0 && pos < bufsize);
	free(big_value);

	g_state.transport->write_line(g_state.transport->ctx, buf, (size_t)pos);
	free(buf);

	/* local was parsed */
	assert(g_state.local_count == 1);
	assert(g_state.local_values != NULL);
	assert(g_state.local_values[0] != NULL);
	/* value must be clamped to TUI_LOCAL_VALUE_MAX, not the full 50KB */
	assert((int)strlen(g_state.local_values[0]) == TUI_LOCAL_VALUE_MAX);

	free_locals(&g_state);
	teardown();
}

/* -------------------------------------------------------------------------
 * test_frame_select_clears_source_when_script_differs
 *
 * Setup:
 *   - Load source for "a.script" with 5 lines (script_line_count == 5).
 *   - Set up two frames: frame[0] at "b.script":10, frame[1] at "a.script":5.
 *   - selected_frame starts at 1 (innermost, "a.script").
 *   - script_path is "a.script" (matches loaded source).
 *
 * Action: inject UP arrow on stack pane -> tui_select_frame(0) -> "b.script".
 *
 * Assert:
 *   - script_line_count == 0 (source was blanked)
 *   - script_path == "b.script" (updated to the new frame's script)
 *
 * Before the fix, tui_select_frame overwrote script_path and current_line
 * but left script_lines pointing at "a.script"'s source, so the script pane
 * displayed wrong-source-with-frame-line.
 * ---------------------------------------------------------------------- */
static void test_frame_select_clears_source_when_script_differs(void) {
	setup();

	/* Load source for "a.script" with 5 lines */
	fill_script_lines(&g_state, 5);
	strncpy(g_state.script_path, "a.script", sizeof(g_state.script_path) - 1);
	g_state.script_path[sizeof(g_state.script_path) - 1] = '\0';
	g_state.current_line = 5;

	/* Set up two frames: outermost is "b.script", innermost is "a.script" */
	g_state.frame_count = 2;
	strncpy(g_state.frame_scripts[0], "b.script",
	        sizeof(g_state.frame_scripts[0]) - 1);
	g_state.frame_scripts[0][sizeof(g_state.frame_scripts[0]) - 1] = '\0';
	g_state.frame_lines[0] = 10;
	strncpy(g_state.frame_scripts[1], "a.script",
	        sizeof(g_state.frame_scripts[1]) - 1);
	g_state.frame_scripts[1][sizeof(g_state.frame_scripts[1]) - 1] = '\0';
	g_state.frame_lines[1] = 5;
	g_state.selected_frame = 1;  /* start at innermost: "a.script" */

	/* Inject UP arrow on the stack pane to select outermost ("b.script") */
	boxen_event_t ev;
	memset(&ev, 0, sizeof(ev));
	ev.type    = BOXEN_EV_KEY;
	ev.key.key = BOXEN_KEY_UP;
	ev.key.ch  = 0;
	ev.key.mod = BOXEN_MOD_NONE;

	boxen_window_focus(g_state.stack_win);
	int rc = debugger_tui_run_one_tick(&g_state, &ev);

	assert(rc == TUI_CONTINUE);
	assert(g_state.selected_frame == 0);
	/* Script pane must be blanked (no source for "b.script" yet) */
	assert(g_state.script_line_count == 0);
	/* script_path must reflect the newly selected frame */
	assert(strcmp(g_state.script_path, "b.script") == 0);

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

	/* B.2 tests */
	TR_RUN(test_stack_pane_draws_frames);
	TR_RUN(test_frame_selection_updates_script_pane);
	TR_RUN(test_locals_render);
	TR_RUN(test_getstack_response_parses);
	TR_RUN(test_getlocals_response_parses);
	TR_RUN(test_stack_frame_count_capped);
	TR_RUN(test_locals_count_capped);
	TR_RUN(test_suspended_fires_getstack_getlocals);

	/* B.2 round-1 review regression tests (#739) */
	TR_RUN(test_store_stack_clears_on_empty_array);
	TR_RUN(test_store_locals_clears_on_empty_array);
	TR_RUN(test_store_locals_caps_long_value);
	TR_RUN(test_frame_select_clears_source_when_script_differs);

	TR_SUMMARY();
	return TR_EXIT_CODE();
}
