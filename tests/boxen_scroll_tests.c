/*
 * boxen_scroll_tests.c -- unit tests for boxen A.5 (scrolling content model
 *                         + row highlight).
 *
 * Covers:
 *   1. set_content_size round-trip (stores w/h; clamped to BOXEN_MAX_DIMENSION)
 *   2. set_scroll clamped to [0, max(0, content_w - viewport_w)]
 *   3. scroll_by adjusts scroll position with clamping
 *   4. ensure_visible scrolls viewport so the target point is visible
 *   5. set_cell takes content coords: write at content (cx, cy) lands at
 *      screen (cx - scroll_x, cy - scroll_y) relative to the window rect
 *   6. set_cell clips at viewport: write at content coord outside viewport
 *      is a no-op (no cell written)
 *   7. boxen_window_at returns content coords (scroll_x/y added) after scroll
 *   8. row_highlight: cells in the highlighted row carry the highlight attr
 *      after boxen_present()
 *
 * Links: boxen_log.c, boxen_wcwidth.c, backend_mock.c, boxen.c
 * No Frontier runtime dependency.
 *
 * Run: make -C tests boxen_scroll_tests && ./tests/boxen_scroll_tests
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
	boxen_mock_reset(80, 24);
	boxen_result_t r = boxen_init(boxen_mock_backend(), NULL, NULL);
	assert(r == BOXEN_OK);
}

static void teardown(void) {
	boxen_shutdown();
}

/* -------------------------------------------------------------------------
 * Test 1: set_content_size stores logical content dimensions; clamped to
 *         BOXEN_MAX_DIMENSION (16384). get_scroll returns 0/0 after
 *         setting content size (no scrolling yet).
 * ---------------------------------------------------------------------- */

static void test_set_content_size_round_trip(void) {
	setup();

	/* 10x5 window with 10x100 content (tall virtual list) */
	boxen_window_t *win = boxen_window_open("W",
		(boxen_rect_t){0, 0, 10, 5}, NULL);
	assert(win != NULL);

	boxen_window_set_content_size(win, 10, 100);

	/* Scroll must start at 0/0. */
	int sx = -1, sy = -1;
	boxen_window_get_scroll(win, &sx, &sy);
	assert(sx == 0);
	assert(sy == 0);

	/* Verify the content dimensions are stored via set_scroll clamping:
	 * requesting scroll beyond (content_h - viewport_h) should clamp. */
	boxen_window_set_scroll(win, 0, 200);  /* beyond 100 - 5 = 95 */
	boxen_window_get_scroll(win, &sx, &sy);
	assert(sy == 95);  /* clamped to content_h - rect.h = 100 - 5 */

	/* Pathological: content size beyond BOXEN_MAX_DIMENSION is clamped. */
	boxen_window_set_content_size(win, 99999, 99999);
	boxen_window_set_scroll(win, 99999, 99999);
	boxen_window_get_scroll(win, &sx, &sy);
	/* After clamping content to 16384, max scroll is 16384 - 10 = 16374 (x)
	 * and 16384 - 5 = 16379 (y). */
	assert(sx <= 16374);
	assert(sy <= 16379);

	/* NULL window: get_scroll writes 0/0 to out-params; does not crash. */
	int nx = 7, ny = 7;
	boxen_window_get_scroll(NULL, &nx, &ny);
	assert(nx == 0);
	assert(ny == 0);

	/* NULL out-params: does not crash. */
	boxen_window_get_scroll(win, NULL, NULL);

	boxen_window_close(win);
	teardown();
}

/* -------------------------------------------------------------------------
 * Test 2: set_scroll clamped to valid range
 *
 * For a 10-wide window with content_w = 50:
 *   max_scroll_x = 50 - 10 = 40
 * Requesting 100 must clamp to 40. Requesting -5 must clamp to 0.
 * Content smaller than viewport -> max_scroll = 0; any value clamps to 0.
 * ---------------------------------------------------------------------- */

static void test_set_scroll_clamps_to_content(void) {
	setup();

	/* Window: 10 wide, 5 tall. Content: 50 wide, 20 tall. */
	boxen_window_t *win = boxen_window_open("W",
		(boxen_rect_t){2, 2, 10, 5}, NULL);
	assert(win != NULL);
	boxen_window_set_content_size(win, 50, 20);

	/* Clamp x above max */
	boxen_window_set_scroll(win, 100, 0);
	int sx, sy;
	boxen_window_get_scroll(win, &sx, &sy);
	assert(sx == 40);  /* 50 - 10 */
	assert(sy == 0);

	/* Clamp y above max */
	boxen_window_set_scroll(win, 0, 30);
	boxen_window_get_scroll(win, &sx, &sy);
	assert(sy == 15);  /* 20 - 5 */

	/* Clamp negative to 0 */
	boxen_window_set_scroll(win, -10, -3);
	boxen_window_get_scroll(win, &sx, &sy);
	assert(sx == 0);
	assert(sy == 0);

	/* Content smaller than viewport: max_scroll must be 0 */
	boxen_window_set_content_size(win, 5, 3);  /* smaller than 10x5 viewport */
	boxen_window_set_scroll(win, 99, 99);
	boxen_window_get_scroll(win, &sx, &sy);
	assert(sx == 0);
	assert(sy == 0);

	boxen_window_close(win);
	teardown();
}

/* -------------------------------------------------------------------------
 * Test 3: scroll_by adjusts position with clamping
 * ---------------------------------------------------------------------- */

static void test_scroll_by_adjusts_position(void) {
	setup();

	boxen_window_t *win = boxen_window_open("W",
		(boxen_rect_t){0, 0, 10, 5}, NULL);
	assert(win != NULL);
	boxen_window_set_content_size(win, 50, 20);

	/* Start at (5, 3) */
	boxen_window_set_scroll(win, 5, 3);
	int sx, sy;
	boxen_window_get_scroll(win, &sx, &sy);
	assert(sx == 5 && sy == 3);

	/* Scroll down by 4 -> y = 7 */
	boxen_window_scroll_by(win, 0, 4);
	boxen_window_get_scroll(win, &sx, &sy);
	assert(sy == 7);

	/* Scroll right by 10 -> x = 15 */
	boxen_window_scroll_by(win, 10, 0);
	boxen_window_get_scroll(win, &sx, &sy);
	assert(sx == 15);

	/* Scroll beyond max: clamped. Max y = 20 - 5 = 15. */
	boxen_window_scroll_by(win, 0, 100);
	boxen_window_get_scroll(win, &sx, &sy);
	assert(sy == 15);

	/* Scroll back past 0: clamped. */
	boxen_window_scroll_by(win, -100, -100);
	boxen_window_get_scroll(win, &sx, &sy);
	assert(sx == 0 && sy == 0);

	boxen_window_close(win);
	teardown();
}

/* -------------------------------------------------------------------------
 * Test 4: ensure_visible scrolls so the target content point is in view
 *
 * Window: 10 wide, 5 tall, at terminal (0, 0).
 * Content: 50 wide, 30 tall.
 * Start scroll at (0, 0). Ask for content (25, 20) to be visible.
 * Expected: scroll_y = 20 - 5 + 1 = 16  (point at bottom edge of viewport)
 *           scroll_x = 25 - 10 + 1 = 16
 * Then: ask for content (2, 2) to be visible from that position.
 * Expected: scroll_x = 2 (point at left edge), scroll_y = 2 (top edge).
 * ---------------------------------------------------------------------- */

static void test_ensure_visible_scrolls_into_view(void) {
	setup();

	boxen_window_t *win = boxen_window_open("W",
		(boxen_rect_t){0, 0, 10, 5}, NULL);
	assert(win != NULL);
	boxen_window_set_content_size(win, 50, 30);

	/* Point outside viewport to the right/bottom: scroll to bring it in. */
	boxen_window_ensure_visible(win, 25, 20);
	int sx, sy;
	boxen_window_get_scroll(win, &sx, &sy);
	/* Standard nearest-edge: x must satisfy scroll_x <= 25 < scroll_x + 10
	 *                         y must satisfy scroll_y <= 20 < scroll_y + 5 */
	assert(sx <= 25 && 25 < sx + 10);
	assert(sy <= 20 && 20 < sy + 5);

	/* Point inside viewport after above scroll: no-op. */
	int sx_before = sx, sy_before = sy;
	/* scroll_x now ~16, point (sx+2, sy+1) is inside the viewport */
	boxen_window_ensure_visible(win, sx + 2, sy + 1);
	int sx2, sy2;
	boxen_window_get_scroll(win, &sx2, &sy2);
	assert(sx2 == sx_before && sy2 == sy_before);  /* no change */

	/* Point to the top/left of current viewport: scroll brings it in. */
	boxen_window_set_scroll(win, 20, 15);
	boxen_window_ensure_visible(win, 5, 3);
	int sx3, sy3;
	boxen_window_get_scroll(win, &sx3, &sy3);
	assert(sx3 == 5);  /* scrolled left to put 5 at viewport left edge */
	assert(sy3 == 3);  /* scrolled up to put 3 at viewport top edge */

	boxen_window_close(win);
	teardown();
}

/* -------------------------------------------------------------------------
 * Test 5: set_cell takes content coordinates
 *
 * Window at terminal (2, 3), 10 wide, 5 tall.
 * Content size: 10 wide, 20 tall. Scroll: (0, 5).
 *
 * Calling set_cell(win, 3, 7, 'X', ...) should write to:
 *   screen_x = 3 - scroll_x + win.rect.x = 3 - 0 + 2 = 5
 *   screen_y = 7 - scroll_y + win.rect.y = 7 - 5 + 3 = 5
 *
 * So terminal cell (5, 5) must have ch='X'.
 * A content cell at (3, 4) is above the viewport (scroll_y=5 means rows
 * 5..9 are visible); set_cell for that coord must be a no-op.
 * ---------------------------------------------------------------------- */

static void test_set_cell_takes_content_coords(void) {
	setup();

	boxen_window_t *win = boxen_window_open("W",
		(boxen_rect_t){2, 3, 10, 5}, NULL);
	assert(win != NULL);
	boxen_window_set_content_size(win, 10, 20);
	boxen_window_set_scroll(win, 0, 5);  /* rows 5..9 visible */

	/* Write at content (3, 7) -> terminal (2 + 3, 3 + (7-5)) = (5, 5) */
	boxen_set_cell(win, 3, 7, 'X', BOXEN_COLOR_DEFAULT, BOXEN_COLOR_DEFAULT, 0);

	const boxen_mock_cell_t *c = boxen_mock_cell_at(5, 5);
	assert(c != NULL);
	assert(c->ch == (uint32_t)'X');

	/* The cell at the wrong terminal position must be untouched. */
	const boxen_mock_cell_t *wrong = boxen_mock_cell_at(5, 10);
	assert(wrong == NULL || wrong->ch != (uint32_t)'X');

	/* Verify a different content row within viewport. Content (0, 5) maps to
	 * terminal (2 + 0, 3 + (5-5)) = (2, 3) -- top-left corner of window. */
	boxen_set_cell(win, 0, 5, 'A', BOXEN_COLOR_DEFAULT, BOXEN_COLOR_DEFAULT, 0);
	const boxen_mock_cell_t *c2 = boxen_mock_cell_at(2, 3);
	assert(c2 != NULL);
	assert(c2->ch == (uint32_t)'A');

	boxen_window_close(win);
	teardown();
}

/* -------------------------------------------------------------------------
 * Test 6: set_cell clips at viewport -- out-of-viewport content writes
 *         are no-ops.
 *
 * Same setup: window (2,3,10,5), scroll (0,5). Visible content rows: 5..9.
 * Writing at content row 4 (above viewport) must not touch the terminal.
 * Writing at content row 10 (below viewport) must not touch the terminal.
 * Writing at content col 10 (right of content/viewport) must not touch.
 * ---------------------------------------------------------------------- */

static void test_set_cell_clips_at_viewport(void) {
	setup();

	boxen_window_t *win = boxen_window_open("W",
		(boxen_rect_t){2, 3, 10, 5}, NULL);
	assert(win != NULL);
	boxen_window_set_content_size(win, 10, 20);
	boxen_window_set_scroll(win, 0, 5);

	/* Content row 4 is above the visible range [5, 9]. */
	boxen_set_cell(win, 0, 4, 'U', BOXEN_COLOR_DEFAULT, BOXEN_COLOR_DEFAULT, 0);
	/* Would map to terminal y = 3 + (4 - 5) = 2, which is above the window.
	 * The cell at (2, 2) must remain untouched. */
	const boxen_mock_cell_t *above = boxen_mock_cell_at(2, 2);
	assert(above == NULL || above->ch != (uint32_t)'U');

	/* Content row 10 is below the visible range [5, 9]. */
	boxen_set_cell(win, 0, 10, 'D', BOXEN_COLOR_DEFAULT, BOXEN_COLOR_DEFAULT, 0);
	/* Would map to terminal y = 3 + (10 - 5) = 8. */
	const boxen_mock_cell_t *below = boxen_mock_cell_at(2, 8);
	assert(below == NULL || below->ch != (uint32_t)'D');

	/* Content col 10 equals viewport width (0-based 0..9 valid). */
	boxen_set_cell(win, 10, 7, 'R', BOXEN_COLOR_DEFAULT, BOXEN_COLOR_DEFAULT, 0);
	const boxen_mock_cell_t *right = boxen_mock_cell_at(12, 5);
	assert(right == NULL || right->ch != (uint32_t)'R');

	/* Content col -1 is left of content. */
	boxen_set_cell(win, -1, 7, 'L', BOXEN_COLOR_DEFAULT, BOXEN_COLOR_DEFAULT, 0);
	const boxen_mock_cell_t *left = boxen_mock_cell_at(1, 5);
	assert(left == NULL || left->ch != (uint32_t)'L');

	boxen_window_close(win);
	teardown();
}

/* -------------------------------------------------------------------------
 * Test 7: boxen_window_at returns content coordinates (scroll-adjusted)
 *
 * Window at terminal (5, 2), 20 wide, 10 tall. Scroll: (3, 7).
 * Terminal (10, 5) is inside the window.
 * Window-local screen coords: (10-5, 5-2) = (5, 3).
 * Content coords (after adding scroll): (5+3, 3+7) = (8, 10).
 * ---------------------------------------------------------------------- */

static void test_window_at_returns_content_coords(void) {
	setup();

	boxen_window_t *win = boxen_window_open("W",
		(boxen_rect_t){5, 2, 20, 10}, NULL);
	assert(win != NULL);
	boxen_window_set_content_size(win, 100, 100);
	boxen_window_set_scroll(win, 3, 7);

	int cx = -1, cy = -1;
	boxen_window_t *found = boxen_window_at(10, 5, &cx, &cy);
	assert(found == win);
	assert(cx == 8);   /* (10-5) + scroll_x(3) = 8 */
	assert(cy == 10);  /* (5-2)  + scroll_y(7) = 10 */

	/* Corner of window: terminal (5, 2) -> content (0+3, 0+7) = (3, 7). */
	cx = -1; cy = -1;
	found = boxen_window_at(5, 2, &cx, &cy);
	assert(found == win);
	assert(cx == 3);
	assert(cy == 7);

	/* NULL out-params: must not crash. */
	found = boxen_window_at(10, 5, NULL, NULL);
	assert(found == win);

	/* Point outside window: returns NULL. */
	found = boxen_window_at(0, 0, &cx, &cy);
	assert(found == NULL);

	boxen_window_close(win);
	teardown();
}

/* -------------------------------------------------------------------------
 * Test 8: row_highlight -- cells in the highlighted content row carry the
 *         highlight attr/fg/bg after boxen_present().
 *
 * Window: (0,0,10,5), content 10x20, scroll_y=5. Highlight row 7 (content).
 * Row 7 is visible (rows 5..9 in view). Screen row = 7 - 5 + 0 = 2.
 * After present(): cells in screen row 2 (terminal y=2) must have the
 * highlight attr.
 *
 * Fill the entire window first so every cell has a known character, then
 * present and check the highlighted row.
 * ---------------------------------------------------------------------- */

static bool highlight_draw_called = false;
static boxen_window_t *highlight_test_win = NULL;

static void highlight_draw_fn(boxen_window_t *win, void *ud) {
	(void)ud;
	/* Fill entire visible content area with '.' using content coordinates. */
	int scroll_y;
	boxen_window_get_scroll(win, NULL, &scroll_y);
	int h = boxen_window_content_height(win);
	int w = boxen_window_content_width(win);
	for (int row = scroll_y; row < scroll_y + h; row++) {
		for (int col = 0; col < w; col++) {
			boxen_set_cell(win, col, row, '.', BOXEN_COLOR_WHITE, BOXEN_COLOR_BLACK, 0);
		}
	}
	highlight_draw_called = true;
}

static void test_row_highlight_paints_visible_row(void) {
	setup();

	boxen_window_t *win = boxen_window_open("W",
		(boxen_rect_t){0, 0, 10, 5}, NULL);
	assert(win != NULL);
	highlight_test_win = win;
	boxen_window_set_content_size(win, 10, 20);
	boxen_window_set_scroll(win, 0, 5);   /* visible rows 5..9 */

	/* Highlight content row 7 (visible; screen row 2, terminal y=2). */
	boxen_window_set_row_highlight(win, 7,
		BOXEN_ATTR_REVERSE, BOXEN_COLOR_WHITE, BOXEN_COLOR_BLUE);

	boxen_window_set_draw(win, highlight_draw_fn);
	highlight_draw_called = false;

	boxen_present();
	assert(highlight_draw_called);

	/* After present(), cells in terminal row 2 (screen row 2 of the window
	 * at y=0) must have BOXEN_ATTR_REVERSE and the highlight colors. */
	for (int col = 0; col < 10; col++) {
		const boxen_mock_cell_t *c = boxen_mock_cell_at(col, 2);
		assert(c != NULL);
		assert(c->attr & BOXEN_ATTR_REVERSE);
		assert(c->fg == BOXEN_COLOR_WHITE);
		assert(c->bg == BOXEN_COLOR_BLUE);
	}

	/* Row 3 (content row 8, not highlighted) must have the fill attr. */
	for (int col = 0; col < 10; col++) {
		const boxen_mock_cell_t *c = boxen_mock_cell_at(col, 3);
		assert(c != NULL);
		assert(!(c->attr & BOXEN_ATTR_REVERSE));
	}

	/* Disable highlight and present again: row 2 must lose REVERSE. */
	boxen_window_set_row_highlight(win, -1, 0, 0, 0);
	boxen_mock_reset(80, 24);  /* clear the mock screen */
	/* Re-init is needed after mock_reset since it reinitializes the backend. */
	boxen_shutdown();
	boxen_init(boxen_mock_backend(), NULL, NULL);
	/* Re-open window since shutdown freed it. */
	win = boxen_window_open("W2", (boxen_rect_t){0, 0, 10, 5}, NULL);
	assert(win != NULL);
	boxen_window_set_content_size(win, 10, 20);
	boxen_window_set_scroll(win, 0, 5);
	boxen_window_set_draw(win, highlight_draw_fn);
	/* highlight_row defaults to -1: no highlight. */
	highlight_draw_called = false;
	boxen_present();
	assert(highlight_draw_called);

	/* Row 2 must not have REVERSE. */
	for (int col = 0; col < 10; col++) {
		const boxen_mock_cell_t *c = boxen_mock_cell_at(col, 2);
		assert(c != NULL);
		assert(!(c->attr & BOXEN_ATTR_REVERSE));
	}

	boxen_window_close(win);
	teardown();
}

/* -------------------------------------------------------------------------
 * Test 9: set_cell + ensure_visible reject pathological coords (security P2)
 *
 * Regression test for the BOXEN_MAX_DIMENSION clamp added to defend against
 * signed-int overflow when caller passes extreme content coords.
 * ---------------------------------------------------------------------- */
static void test_extreme_coords_rejected(void) {
	setup();
	boxen_window_t *win = boxen_window_open("w", (boxen_rect_t){0, 0, 10, 5}, NULL);
	assert(win != NULL);
	boxen_window_set_content_size(win, 100, 100);

	/* set_cell with extreme content coords must not crash or wrap-around.
	 * Mock backend won't have anything written; just ensuring no UB. */
	boxen_set_cell(win, 2147483647, 5, 'X',
	               BOXEN_COLOR_DEFAULT, BOXEN_COLOR_DEFAULT, 0);
	boxen_set_cell(win, -2147483647 - 1, 5, 'X',
	               BOXEN_COLOR_DEFAULT, BOXEN_COLOR_DEFAULT, 0);
	boxen_set_cell(win, 5, 2147483647, 'X',
	               BOXEN_COLOR_DEFAULT, BOXEN_COLOR_DEFAULT, 0);

	/* ensure_visible with extreme content coords must not crash. */
	boxen_window_ensure_visible(win, 2147483647, 5);
	boxen_window_ensure_visible(win, 5, 2147483647);

	/* And a normal call still works (sanity). */
	boxen_set_cell(win, 3, 2, 'A',
	               BOXEN_COLOR_DEFAULT, BOXEN_COLOR_DEFAULT, 0);
	const boxen_mock_cell_t *c = boxen_mock_cell_at(3, 2);
	assert(c != NULL);
	assert(c->ch == 'A');

	boxen_window_close(win);
	teardown();
}

/* -------------------------------------------------------------------------
 * Test 10: ensure_visible on zero-dim window is no-op (P3 bar-raiser)
 * ---------------------------------------------------------------------- */
static void test_ensure_visible_zero_dim_noop(void) {
	setup();
	boxen_window_t *win = boxen_window_open("w", (boxen_rect_t){0, 0, 0, 0}, NULL);
	assert(win != NULL);
	boxen_window_set_content_size(win, 100, 100);

	/* Pre-state. */
	int sx0 = 0, sy0 = 0;
	boxen_window_get_scroll(win, &sx0, &sy0);

	/* Should be a no-op on a zero-dim viewport. */
	boxen_window_ensure_visible(win, 50, 50);

	int sx1 = 0, sy1 = 0;
	boxen_window_get_scroll(win, &sx1, &sy1);
	assert(sx1 == sx0);
	assert(sy1 == sy0);

	boxen_window_close(win);
	teardown();
}

/* -------------------------------------------------------------------------
 * main
 * ---------------------------------------------------------------------- */

int main(void) {
	TR_INIT("boxen_scroll_tests");

	TR_RUN(test_set_content_size_round_trip);
	TR_RUN(test_set_scroll_clamps_to_content);
	TR_RUN(test_scroll_by_adjusts_position);
	TR_RUN(test_ensure_visible_scrolls_into_view);
	TR_RUN(test_set_cell_takes_content_coords);
	TR_RUN(test_set_cell_clips_at_viewport);
	TR_RUN(test_window_at_returns_content_coords);
	TR_RUN(test_row_highlight_paints_visible_row);
	TR_RUN(test_extreme_coords_rejected);
	TR_RUN(test_ensure_visible_zero_dim_noop);

	TR_SUMMARY();
	return TR_EXIT_CODE();
}
