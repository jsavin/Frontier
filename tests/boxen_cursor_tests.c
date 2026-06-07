/*
 * boxen_cursor_tests.c -- unit tests for boxen A.7 (public window-aware
 *                         cursor positioning API).
 *
 * Covers:
 *   1. test_set_cursor_in_bounds_translates
 *      With borders on (default) and a focused window at (2,3,10,6),
 *      content (0,0) maps to terminal (3,4); visible=true.
 *   2. test_set_cursor_chrome_offset_with_borders_off
 *      Same focused window with borders off: content (0,0) maps to
 *      terminal (rect.x, rect.y) = (2,3).
 *   3. test_set_cursor_respects_scroll
 *      Content size > viewport; scroll non-zero. set_cursor at content
 *      (scroll_x, scroll_y) lands at terminal (rect.x + border, rect.y +
 *      border) and is visible.
 *   4. test_set_cursor_clipped_hides
 *      Content coord outside the visible viewport causes visible=false;
 *      the call must succeed (no crash).
 *   5. test_modal_blocks_non_modal_from_cursor
 *      When a modal B exists, the focus gate redirects all cursor
 *      ownership to the modal. Non-modal A's set_cursor calls are
 *      silently ignored regardless of target position; modal B itself
 *      can still drive the cursor.
 *   6. test_set_cursor_ignored_for_non_focused_non_modal
 *      Focus gate: non-focused, non-modal window cannot move the cursor.
 *      Deliberate footgun-prevention policy -- documented as such.
 *   7. test_set_cursor_visible_independent_of_position
 *      Toggling visibility does not affect the last-set position; both
 *      set_cursor and set_cursor_visible obey the focus gate.
 *
 * Links: boxen_log.c, boxen_wcwidth.c, backend_mock.c, boxen.c
 * No Frontier runtime dependency.
 *
 * Run: make -C tests boxen_cursor_tests && ./tests/boxen_cursor_tests
 */

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "boxen.h"
#include "backend_mock.h"
#include "test_report.h"

/* -------------------------------------------------------------------------
 * Helpers
 * ---------------------------------------------------------------------- */

static void setup(void) {
	/* Defensive: if a prior test failed mid-run (teardown never called), shut
	 * down first so boxen_init doesn't return BOXEN_ERR_ALREADY. */
	boxen_shutdown();
	boxen_mock_reset(80, 24);
	boxen_result_t r = boxen_init(boxen_mock_backend(), NULL, NULL);
	assert(r == BOXEN_OK);
}

static void teardown(void) {
	boxen_shutdown();
}

/* -------------------------------------------------------------------------
 * Test 1: in-bounds content coord translates with chrome offset
 *
 * Focused window at rect (2, 3, 10, 6), borders on (default).
 * Content (0, 0) maps to terminal (rect.x + 1, rect.y + 1) = (3, 4).
 * Visibility is set true.
 * ---------------------------------------------------------------------- */

static void test_set_cursor_in_bounds_translates(void) {
	setup();

	boxen_window_t *win = boxen_window_open("W",
		(boxen_rect_t){2, 3, 10, 6}, NULL);
	assert(win != NULL);
	boxen_window_focus(win);

	boxen_window_set_cursor(win, 0, 0);

	int cx = -99, cy = -99;
	boxen_mock_get_cursor(&cx, &cy);
	assert(cx == 3);
	assert(cy == 4);
	assert(boxen_mock_cursor_visible() == true);

	boxen_window_close(win);
	teardown();
}

/* -------------------------------------------------------------------------
 * Test 2: borders off -- content origin is rect origin
 *
 * Same focused 10x6 window at (2, 3). After borders=false, content (0,0)
 * lands at terminal (2, 3).
 * ---------------------------------------------------------------------- */

static void test_set_cursor_chrome_offset_with_borders_off(void) {
	setup();

	boxen_window_t *win = boxen_window_open("W",
		(boxen_rect_t){2, 3, 10, 6}, NULL);
	assert(win != NULL);
	boxen_window_set_borders(win, false);
	boxen_window_focus(win);

	boxen_window_set_cursor(win, 0, 0);

	int cx = -99, cy = -99;
	boxen_mock_get_cursor(&cx, &cy);
	assert(cx == 2);
	assert(cy == 3);
	assert(boxen_mock_cursor_visible() == true);

	boxen_window_close(win);
	teardown();
}

/* -------------------------------------------------------------------------
 * Test 3: scroll offsets the content -> screen mapping
 *
 * Focused 10x6 window at (2, 3), borders on. content_size 20x20, scroll
 * (5, 4). Content (5, 4) - scroll (5, 4) = local (0, 0) = terminal (3, 4).
 * ---------------------------------------------------------------------- */

static void test_set_cursor_respects_scroll(void) {
	setup();

	boxen_window_t *win = boxen_window_open("W",
		(boxen_rect_t){2, 3, 10, 6}, NULL);
	assert(win != NULL);
	boxen_window_focus(win);
	boxen_window_set_content_size(win, 20, 20);
	boxen_window_set_scroll(win, 5, 4);

	boxen_window_set_cursor(win, 5, 4);

	int cx = -99, cy = -99;
	boxen_mock_get_cursor(&cx, &cy);
	assert(cx == 3);
	assert(cy == 4);
	assert(boxen_mock_cursor_visible() == true);

	boxen_window_close(win);
	teardown();
}

/* -------------------------------------------------------------------------
 * Test 4: out-of-viewport content coord hides the cursor
 *
 * Focused window at (2, 3, 10, 6), borders on. set_cursor at content
 * (100, 100) lies far outside the visible viewport. Visibility must
 * become false. Position is intentionally unspecified when hidden;
 * the call must not crash.
 * ---------------------------------------------------------------------- */

static void test_set_cursor_clipped_hides(void) {
	setup();

	boxen_window_t *win = boxen_window_open("W",
		(boxen_rect_t){2, 3, 10, 6}, NULL);
	assert(win != NULL);
	boxen_window_focus(win);

	boxen_window_set_cursor(win, 100, 100);

	assert(boxen_mock_cursor_visible() == false);

	boxen_window_close(win);
	teardown();
}

/* -------------------------------------------------------------------------
 * Test 5: modal presence blocks non-modal callers from cursor ownership
 *
 * A is focused and covers (0, 0, 20, 10). B is modal at (5, 2, 10, 6).
 *
 * When a modal exists, cursor_owner_allowed redirects all cursor
 * ownership to the modal regardless of target position; non-modal
 * callers are silently ignored before any translation or
 * modal-occlusion check runs. This test asserts that A's calls do not
 * touch the mock cursor at all -- get_cursor still returns (-1, -1) --
 * which catches a regression where the focus gate accepts the call but
 * the cursor is updated anyway.
 *
 * The modal-occlusion branch inside boxen_window_set_cursor exists as
 * defense-in-depth for future gate-policy changes, but is currently
 * unreachable from the public API, so this test does not exercise it
 * directly. The positive assertion that modal B itself can drive the
 * cursor anchors that the API does work for the right caller.
 * ---------------------------------------------------------------------- */

static void test_modal_blocks_non_modal_from_cursor(void) {
	setup();

	boxen_window_t *a = boxen_window_open("A",
		(boxen_rect_t){0, 0, 20, 10}, NULL);
	assert(a != NULL);
	boxen_window_focus(a);

	boxen_window_t *b = boxen_window_open("B",
		(boxen_rect_t){5, 2, 10, 6}, NULL);
	assert(b != NULL);
	boxen_window_set_modal(b, true);

	/* From A, target content (6, 3) -> would-be screen (7, 4), inside B's
	 * rect. Focus gate rejects before translation; cursor untouched. */
	boxen_window_set_cursor(a, 6, 3);
	assert(boxen_mock_cursor_visible() == false);
	int cx = -99, cy = -99;
	boxen_mock_get_cursor(&cx, &cy);
	assert(cx == -1);
	assert(cy == -1);

	/* From A, target content (17, 5) -> would-be screen (18, 6), outside
	 * B's rect. Same: focus gate rejects before any check. */
	boxen_window_set_cursor(a, 17, 5);
	assert(boxen_mock_cursor_visible() == false);
	cx = -99; cy = -99;
	boxen_mock_get_cursor(&cx, &cy);
	assert(cx == -1);
	assert(cy == -1);

	/* The modal itself owns the cursor: a normal in-bounds call from
	 * B at content (0, 0) (borders on -> screen (5+1, 2+1) = (6, 3))
	 * succeeds. This anchors that the API works for the right caller. */
	boxen_window_set_cursor(b, 0, 0);
	assert(boxen_mock_cursor_visible() == true);
	boxen_mock_get_cursor(&cx, &cy);
	assert(cx == 6);
	assert(cy == 3);

	boxen_window_close(b);
	boxen_window_close(a);
	teardown();
}

/* -------------------------------------------------------------------------
 * Test 6: focus gate -- non-focused, non-modal windows cannot move cursor
 *
 * Open A focused, B not focused and not modal. set_cursor from B must
 * be silently ignored: the mock cursor remains at its reset state
 * (-1, -1) with visible=false.
 *
 * This is deliberate footgun-prevention: two non-modal windows racing
 * to position the terminal cursor produces undefined behavior in a
 * terminal. Only the focused window OR a modal window may drive it.
 * ---------------------------------------------------------------------- */

static void test_set_cursor_ignored_for_non_focused_non_modal(void) {
	setup();

	boxen_window_t *a = boxen_window_open("A",
		(boxen_rect_t){0, 0, 40, 20}, NULL);
	assert(a != NULL);
	boxen_window_focus(a);

	boxen_window_t *b = boxen_window_open("B",
		(boxen_rect_t){40, 0, 40, 20}, NULL);
	assert(b != NULL);
	/* Note: opening B does NOT auto-focus it -- only explicit focus()
	 * sets the focused window. (The A.3 focus model only mutates focus
	 * on explicit focus() calls or focus-stealing dispatch paths.) */

	boxen_window_set_cursor(b, 0, 0);

	/* Mock cursor state untouched -- still at reset defaults. */
	int cx = 0, cy = 0;
	boxen_mock_get_cursor(&cx, &cy);
	assert(cx == -1);
	assert(cy == -1);
	assert(boxen_mock_cursor_visible() == false);

	boxen_window_close(b);
	boxen_window_close(a);
	teardown();
}

/* -------------------------------------------------------------------------
 * Test 7: visibility toggle independent of position; obeys focus gate
 *
 * Focused window: set_cursor places cursor at (3, 4); subsequent
 * set_cursor_visible(false) hides without changing position; (true)
 * shows again, still at the last position.
 *
 * Also verifies that set_cursor_visible respects the focus gate in
 * BOTH directions: a call from a non-focused, non-modal window does not
 * change the visibility flag whether attempting to hide or to show.
 * ---------------------------------------------------------------------- */

static void test_set_cursor_visible_independent_of_position(void) {
	setup();

	boxen_window_t *a = boxen_window_open("A",
		(boxen_rect_t){2, 3, 10, 6}, NULL);
	assert(a != NULL);
	boxen_window_focus(a);

	boxen_window_set_cursor(a, 1, 1);
	int cx = -99, cy = -99;
	boxen_mock_get_cursor(&cx, &cy);
	/* borders on: terminal (rect.x + 1 + 1, rect.y + 1 + 1) = (4, 5). */
	assert(cx == 4);
	assert(cy == 5);
	assert(boxen_mock_cursor_visible() == true);

	boxen_window_set_cursor_visible(a, false);
	assert(boxen_mock_cursor_visible() == false);
	boxen_mock_get_cursor(&cx, &cy);
	assert(cx == 4);  /* position preserved */
	assert(cy == 5);

	boxen_window_set_cursor_visible(a, true);
	assert(boxen_mock_cursor_visible() == true);
	boxen_mock_get_cursor(&cx, &cy);
	assert(cx == 4);
	assert(cy == 5);

	/* Focus gate: a non-focused, non-modal window cannot toggle. */
	boxen_window_t *b = boxen_window_open("B",
		(boxen_rect_t){40, 0, 10, 6}, NULL);
	assert(b != NULL);

	boxen_window_set_cursor_visible(b, false);
	/* Visibility flag unchanged -- still true from the prior call. */
	assert(boxen_mock_cursor_visible() == true);

	/* Symmetric: explicitly drop to false via focused A, then verify that
	 * non-focused B cannot flip it back on either. */
	boxen_window_set_cursor_visible(a, false);
	assert(boxen_mock_cursor_visible() == false);

	boxen_window_set_cursor_visible(b, true);
	/* Visibility flag unchanged -- still false, focus gate rejects show. */
	assert(boxen_mock_cursor_visible() == false);

	boxen_window_close(b);
	boxen_window_close(a);
	teardown();
}

/* -------------------------------------------------------------------------
 * main
 * ---------------------------------------------------------------------- */

int main(void) {
	TR_INIT("boxen_cursor_tests");

	TR_RUN(test_set_cursor_in_bounds_translates);
	TR_RUN(test_set_cursor_chrome_offset_with_borders_off);
	TR_RUN(test_set_cursor_respects_scroll);
	TR_RUN(test_set_cursor_clipped_hides);
	TR_RUN(test_modal_blocks_non_modal_from_cursor);
	TR_RUN(test_set_cursor_ignored_for_non_focused_non_modal);
	TR_RUN(test_set_cursor_visible_independent_of_position);

	TR_SUMMARY();
	return TR_EXIT_CODE();
}
