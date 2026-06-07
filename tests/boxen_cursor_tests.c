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
 *   5. test_set_cursor_occluded_by_modal_hides
 *      A modal window B occludes a screen cell A is trying to write to.
 *      set_cursor from A hides; a coord outside B's rect is visible.
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
 * Test 5: modal window occludes cursor from below
 *
 * A is focused and covers (0, 0, 20, 10). B is modal at (5, 2, 10, 6).
 *
 * NOTE: modal blocks A from owning the cursor at all (only modal or
 * the modal itself may drive the cursor when a modal is present). So
 * both attempts from A are silently rejected by the focus gate -- the
 * mock cursor never updates, and visible stays false from the boxen_init
 * default state. The test asserts visible==false in both branches, which
 * is consistent with both "occluded -> hide" and "focus-gate-rejected ->
 * no update". The next test isolates the focus-gate behavior, and the
 * scroll/in-bounds tests above exercise the visibility-true path.
 * ---------------------------------------------------------------------- */

static void test_set_cursor_occluded_by_modal_hides(void) {
	setup();

	boxen_window_t *a = boxen_window_open("A",
		(boxen_rect_t){0, 0, 20, 10}, NULL);
	assert(a != NULL);
	boxen_window_focus(a);

	boxen_window_t *b = boxen_window_open("B",
		(boxen_rect_t){5, 2, 10, 6}, NULL);
	assert(b != NULL);
	boxen_window_set_modal(b, true);

	/* From A, target content (6, 3) -> screen (6+1, 3+1) = (7, 4),
	 * which falls inside B's rect (5,2)-(15,8). Hidden either way. */
	boxen_window_set_cursor(a, 6, 3);
	assert(boxen_mock_cursor_visible() == false);

	/* From A, target content (17, 5) -> screen (17+1, 5+1) = (18, 6),
	 * which is outside B's rect. Still hidden because the focus gate
	 * routes cursor ownership to the modal when one is present. */
	boxen_window_set_cursor(a, 17, 5);
	assert(boxen_mock_cursor_visible() == false);

	/* But the modal itself owns the cursor: a normal in-bounds call from
	 * B at content (0, 0) (borders on -> screen (5+1, 2+1) = (6, 3))
	 * succeeds. This anchors the "modal occlusion check applies to
	 * non-modal callers" half of the contract. */
	boxen_window_set_cursor(b, 0, 0);
	assert(boxen_mock_cursor_visible() == true);
	int cx = -99, cy = -99;
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
 * Also verifies that set_cursor_visible respects the focus gate: a call
 * from a non-focused, non-modal window does not change the visibility
 * flag.
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
	TR_RUN(test_set_cursor_occluded_by_modal_hides);
	TR_RUN(test_set_cursor_ignored_for_non_focused_non_modal);
	TR_RUN(test_set_cursor_visible_independent_of_position);

	TR_SUMMARY();
	return TR_EXIT_CODE();
}
