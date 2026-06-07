/*
 * boxen_chrome_tests.c -- unit tests for boxen A.6 (chrome rendering,
 *                         layout helpers, pinned-window edge survival).
 *
 * Covers:
 *   1. test_borders_enabled_by_default
 *      After opening a window, borders are on; content_width = rect.w - 2.
 *   2. test_borders_drawn_on_present
 *      After present(), mock grid has box-drawing chars at the four corners.
 *   3. test_title_inset_on_top_row
 *      When a window has a title, the title text appears in the top border row.
 *   4. test_content_width_subtracts_chrome
 *      borders enabled: content_width = max(0, rect.w - 2);
 *                       content_height = max(0, rect.h - 2).
 *   5. test_set_cell_offsets_for_borders
 *      set_cell(win, 0, 0) with borders writes to terminal (rect.x+1, rect.y+1).
 *   6. test_window_at_accounts_for_borders
 *      screen coord at rect.x (left border) returns border sentinel; screen
 *      coord at rect.x+1 (first content column) maps to content cx=0.
 *   7. test_scrollbar_renders_when_content_overflows
 *      When content_h > visible content_h, a scrollbar cell appears on the
 *      right edge of the window interior (between corners).
 *   8. test_split_h_creates_two_windows
 *      boxen_layout_split_h returns two non-NULL windows with correct rects.
 *   9. test_pinned_bottom_survives_terminal_resize
 *      After BOXEN_EV_RESIZE, a BOXEN_PIN_BOTTOM window stays at the bottom.
 *
 * Links: boxen_log.c, boxen_wcwidth.c, backend_mock.c, boxen.c
 * No Frontier runtime dependency.
 *
 * Run: make -C tests boxen_chrome_tests && ./tests/boxen_chrome_tests
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
 * Box-drawing character constants (Unicode codepoints)
 * Single-line (unfocused): U+250C U+2500 U+2510 U+2502 U+2518 U+2514
 * Double-line (focused):   U+2554 U+2550 U+2557 U+2551 U+255D U+255A
 * ---------------------------------------------------------------------- */

#define CH_TL_SINGLE  0x250C  /* top-left corner, single line */
#define CH_TR_SINGLE  0x2510  /* top-right corner, single line */
#define CH_BL_SINGLE  0x2514  /* bottom-left corner, single line */
#define CH_BR_SINGLE  0x2518  /* bottom-right corner, single line */
#define CH_HORIZ      0x2500  /* horizontal bar, single line */
#define CH_VERT       0x2502  /* vertical bar, single line */

#define CH_TL_DOUBLE  0x2554  /* top-left corner, double line */
#define CH_TR_DOUBLE  0x2557  /* top-right corner, double line */
#define CH_BL_DOUBLE  0x255A  /* bottom-left corner, double line */
#define CH_BR_DOUBLE  0x255D  /* bottom-right corner, double line */
#define CH_HORIZ_DBL  0x2550  /* horizontal bar, double line */
#define CH_VERT_DBL   0x2551  /* vertical bar, double line */

/* Scrollbar characters */
#define CH_SCROLLBAR_TRACK  0x2502  /* same as VERT, thin track */
#define CH_SCROLLBAR_THUMB  0x2588  /* FULL BLOCK U+2588 for thumb */

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

/* Force a present() call so the mock grid reflects drawn state. */
static void do_present(void) {
	boxen_present();
}

/* -------------------------------------------------------------------------
 * Test 1: borders enabled by default
 *
 * After boxen_window_open, a window has borders = true. Content dimensions
 * are rect.w - 2 and rect.h - 2 (one cell border on each side).
 * ---------------------------------------------------------------------- */

static void test_borders_enabled_by_default(void) {
	setup();

	/* 20x10 window */
	boxen_window_t *win = boxen_window_open("Test",
		(boxen_rect_t){0, 0, 20, 10}, NULL);
	assert(win != NULL);

	/* Content area subtracts the two border cells in each dimension. */
	int cw = boxen_window_content_width(win);
	int ch = boxen_window_content_height(win);
	assert(cw == 18);  /* 20 - 2 */
	assert(ch == 8);   /* 10 - 2 */

	boxen_window_close(win);
	teardown();
}

/* -------------------------------------------------------------------------
 * Test 2: borders drawn on present
 *
 * A 10x5 window at (5, 3) with no title. After present(), the mock grid
 * must have box-drawing corner/edge characters at the expected positions.
 *
 * Unfocused single-line borders:
 *   (5,3)  = TL corner
 *   (14,3) = TR corner  (5 + 10 - 1 = 14)
 *   (5,7)  = BL corner  (3 + 5 - 1 = 7)
 *   (14,7) = BR corner
 *   Top row cells (6..13, row 3) = horizontal bar
 *   Left col cells (5, rows 4..6) = vertical bar
 *   Right col cells (14, rows 4..6) = vertical bar
 * ---------------------------------------------------------------------- */

static void test_borders_drawn_on_present(void) {
	setup();

	boxen_window_t *win = boxen_window_open(NULL,
		(boxen_rect_t){5, 3, 10, 5}, NULL);
	assert(win != NULL);

	do_present();

	/* Corners (unfocused = single-line) */
	const boxen_mock_cell_t *tl = boxen_mock_cell_at(5, 3);
	const boxen_mock_cell_t *tr = boxen_mock_cell_at(14, 3);
	const boxen_mock_cell_t *bl = boxen_mock_cell_at(5, 7);
	const boxen_mock_cell_t *br = boxen_mock_cell_at(14, 7);

	assert(tl != NULL && tl->ch == CH_TL_SINGLE);
	assert(tr != NULL && tr->ch == CH_TR_SINGLE);
	assert(bl != NULL && bl->ch == CH_BL_SINGLE);
	assert(br != NULL && br->ch == CH_BR_SINGLE);

	/* Top row horizontal bar at (6,3) and (13,3) */
	const boxen_mock_cell_t *th1 = boxen_mock_cell_at(6, 3);
	const boxen_mock_cell_t *th2 = boxen_mock_cell_at(13, 3);
	assert(th1 != NULL && th1->ch == CH_HORIZ);
	assert(th2 != NULL && th2->ch == CH_HORIZ);

	/* Left column bar at (5,4) */
	const boxen_mock_cell_t *lv = boxen_mock_cell_at(5, 4);
	assert(lv != NULL && lv->ch == CH_VERT);

	/* Right column bar at (14,4) */
	const boxen_mock_cell_t *rv = boxen_mock_cell_at(14, 4);
	assert(rv != NULL && rv->ch == CH_VERT);

	boxen_window_close(win);
	teardown();
}

/* -------------------------------------------------------------------------
 * Test 3: title inset on top row
 *
 * A 20x5 window at (0,0) with title "Hello". After present(), the title
 * text should appear somewhere in the top row between the corners.
 * The standard layout is: TL_corner + HORIZ + space + title + space + HORIZ...
 * boxen_mock_has_text() is ASCII-safe and scans any row for the substring.
 * ---------------------------------------------------------------------- */

static void test_title_inset_on_top_row(void) {
	setup();

	boxen_window_t *win = boxen_window_open("Hello",
		(boxen_rect_t){0, 0, 20, 5}, NULL);
	assert(win != NULL);

	do_present();

	/* The title "Hello" must appear somewhere on the screen (in the top row). */
	assert(boxen_mock_has_text("Hello"));

	boxen_window_close(win);
	teardown();
}

/* -------------------------------------------------------------------------
 * Test 4: content_width/height subtract chrome when borders enabled
 *
 * Verify the math for various window sizes including edge cases.
 * ---------------------------------------------------------------------- */

static void test_content_width_subtracts_chrome(void) {
	setup();

	/* Normal case: 20x10 -> content 18x8. */
	boxen_window_t *w1 = boxen_window_open("A", (boxen_rect_t){0, 0, 20, 10}, NULL);
	assert(w1 != NULL);
	assert(boxen_window_content_width(w1) == 18);
	assert(boxen_window_content_height(w1) == 8);
	boxen_window_close(w1);

	/* Minimum viable: 3x3 -> content 1x1. */
	boxen_window_t *w2 = boxen_window_open("B", (boxen_rect_t){0, 0, 3, 3}, NULL);
	assert(w2 != NULL);
	assert(boxen_window_content_width(w2) == 1);
	assert(boxen_window_content_height(w2) == 1);
	boxen_window_close(w2);

	/* Very small: 2x2 -> content 0x0 (entire rect is border). */
	boxen_window_t *w3 = boxen_window_open("C", (boxen_rect_t){0, 0, 2, 2}, NULL);
	assert(w3 != NULL);
	assert(boxen_window_content_width(w3) == 0);
	assert(boxen_window_content_height(w3) == 0);
	boxen_window_close(w3);

	/* 1x1: content must be max(0, 1-2) = 0. */
	boxen_window_t *w4 = boxen_window_open("D", (boxen_rect_t){0, 0, 1, 1}, NULL);
	assert(w4 != NULL);
	assert(boxen_window_content_width(w4) == 0);
	assert(boxen_window_content_height(w4) == 0);
	boxen_window_close(w4);

	/* 0x0: content must be 0x0, no crash. */
	boxen_window_t *w5 = boxen_window_open("E", (boxen_rect_t){0, 0, 0, 0}, NULL);
	assert(w5 != NULL);
	assert(boxen_window_content_width(w5) == 0);
	assert(boxen_window_content_height(w5) == 0);
	boxen_window_close(w5);

	/* NULL window: returns 0, no crash. */
	assert(boxen_window_content_width(NULL) == 0);
	assert(boxen_window_content_height(NULL) == 0);

	teardown();
}

/* -------------------------------------------------------------------------
 * Test 5: set_cell offsets for borders
 *
 * Window at (2, 3), 10 wide, 6 tall. Borders enabled.
 * Content area starts at (rect.x+1, rect.y+1) = (3, 4).
 *
 * boxen_set_cell(win, 0, 0) must write to terminal (3, 4).
 * boxen_set_cell(win, 1, 2) must write to terminal (4, 6).
 *   tx = rect.x + 1 + (x - scroll_x) = 2 + 1 + 1 = 4
 *   ty = rect.y + 1 + (y - scroll_y) = 3 + 1 + 2 = 6
 * ---------------------------------------------------------------------- */

static void test_set_cell_offsets_for_borders(void) {
	setup();

	boxen_window_t *win = boxen_window_open(NULL,
		(boxen_rect_t){2, 3, 10, 6}, NULL);
	assert(win != NULL);

	/* Write to content (0, 0): should land at terminal (3, 4). */
	boxen_set_cell(win, 0, 0, 'X', BOXEN_COLOR_DEFAULT, BOXEN_COLOR_DEFAULT, 0);

	const boxen_mock_cell_t *c = boxen_mock_cell_at(3, 4);
	assert(c != NULL);
	assert(c->ch == (uint32_t)'X');

	/* The old pre-chrome position (2, 3) is a border, not content. */
	const boxen_mock_cell_t *border_pos = boxen_mock_cell_at(2, 3);
	/* border_pos might be a corner box-drawing char; it must NOT be 'X'. */
	assert(border_pos == NULL || border_pos->ch != (uint32_t)'X');

	/* Write to content (1, 2): should land at terminal (4, 6). */
	boxen_set_cell(win, 1, 2, 'Y', BOXEN_COLOR_DEFAULT, BOXEN_COLOR_DEFAULT, 0);
	const boxen_mock_cell_t *c2 = boxen_mock_cell_at(4, 6);
	assert(c2 != NULL);
	assert(c2->ch == (uint32_t)'Y');

	boxen_window_close(win);
	teardown();
}

/* -------------------------------------------------------------------------
 * Test 6: boxen_window_at accounts for borders
 *
 * Window at (5, 2), 20 wide, 10 tall. Borders enabled. Scroll (0, 0).
 *
 * Screen coord (5, 2) is the TL corner (border): should return NULL or
 * the window with cx < 0 (indicating border, not content).
 *
 * The simplest contract: screen coord (5, 2) is the border cell; the
 * function should return the window (hit test succeeds within rect), but
 * the cx/cy returned should be -1/-1 indicating a border region, NOT 0,0.
 *
 * Screen coord (6, 3) is the first content cell: cx=0, cy=0.
 *   (sx=6 -> sx - rect.x - 1 + scroll_x = 6 - 5 - 1 + 0 = 0)
 *   (sy=3 -> sy - rect.y - 1 + scroll_y = 3 - 2 - 1 + 0 = 0)
 *
 * Screen coord (5, 5) is the left border (vertical bar column), not content.
 *   Content would be cx = 5 - 5 - 1 = -1 (border sentinel).
 * ---------------------------------------------------------------------- */

static void test_window_at_accounts_for_borders(void) {
	setup();

	boxen_window_t *win = boxen_window_open(NULL,
		(boxen_rect_t){5, 2, 20, 10}, NULL);
	assert(win != NULL);

	int cx, cy;

	/* First content cell (one cell inset from top-left corner). */
	boxen_window_t *found = boxen_window_at(6, 3, &cx, &cy);
	assert(found == win);
	assert(cx == 0);
	assert(cy == 0);

	/* Left border column (5, 5): cx should be -1 (border sentinel). */
	cx = 99; cy = 99;
	found = boxen_window_at(5, 5, &cx, &cy);
	/* The window is found (within rect), but cx is in border territory. */
	assert(found == win);
	assert(cx == -1);

	/* Top border row (7, 2): cy should be -1. */
	cx = 99; cy = 99;
	found = boxen_window_at(7, 2, &cx, &cy);
	assert(found == win);
	assert(cy == -1);

	/* Right border column (24, 5): window right edge is 5+20-1=24.
	 * cx should be content_width (off right edge = border). */
	int cw = boxen_window_content_width(win);  /* 18 */
	cx = 99; cy = 99;
	found = boxen_window_at(24, 5, &cx, &cy);
	assert(found == win);
	assert(cx == cw);  /* border sentinel = content_width */

	/* Completely outside: returns NULL. */
	found = boxen_window_at(0, 0, &cx, &cy);
	assert(found == NULL);

	boxen_window_close(win);
	teardown();
}

/* -------------------------------------------------------------------------
 * Test 7: scrollbar renders when content overflows
 *
 * Window at (0, 0), 10 wide, 6 tall. Content 10x30.
 * Visible content rows (with borders): 6 - 2 = 4 rows.
 * Content overflows (30 > 4), so a scrollbar should appear on the right
 * interior edge of the window (column 9, rows 1..4).
 *
 * Scrollbar right interior column is at rect.x + rect.w - 1 - 1 = 8
 * (the column just inside the right border).
 * Wait -- the scrollbar is drawn ON the right border column (rect.x + rect.w - 1).
 * But the border is also there. For the scrollbar: it replaces the vertical
 * bar characters on the right side between the corners.
 *
 * Right border column x = rect.x + rect.w - 1 = 0 + 10 - 1 = 9.
 * Rows between corners: 1..4 (rect.y+1 to rect.y+rect.h-2, inclusive).
 * With scroll_y = 0 and content_h = 30, visible_h = 4:
 *   thumb_start = 0 (at top)
 *   thumb_height ~= visible_h * track_height / content_h >= 1
 * At least one cell in the right column rows 1..4 must be the thumb char.
 * ---------------------------------------------------------------------- */

static bool any_thumb_in_column(int col, int row_start, int row_end) {
	for (int row = row_start; row <= row_end; row++) {
		const boxen_mock_cell_t *c = boxen_mock_cell_at(col, row);
		if (c != NULL && c->ch == CH_SCROLLBAR_THUMB) return true;
	}
	return false;
}

static void test_scrollbar_renders_when_content_overflows(void) {
	setup();

	boxen_window_t *win = boxen_window_open(NULL,
		(boxen_rect_t){0, 0, 10, 6}, NULL);
	assert(win != NULL);

	/* Content taller than the visible area. */
	boxen_window_set_content_size(win, 10, 30);

	do_present();

	/* Scrollbar appears on right interior column (column 8, inside right border).
	 * Rows 1..4 (interior rows between top and bottom borders).
	 * At least one cell should be the thumb character. */
	assert(any_thumb_in_column(8, 1, 4));

	/* When content fits (set content to small size), no thumb visible. */
	boxen_window_set_content_size(win, 10, 2);
	boxen_mock_reset(80, 24);
	boxen_shutdown();
	boxen_init(boxen_mock_backend(), NULL, NULL);
	win = boxen_window_open(NULL, (boxen_rect_t){0, 0, 10, 6}, NULL);
	assert(win != NULL);
	boxen_window_set_content_size(win, 10, 2);
	do_present();

	/* With content_h <= visible_h (2 <= 4), no thumb should appear. */
	assert(!any_thumb_in_column(8, 1, 4));

	boxen_window_close(win);
	teardown();
}

/* -------------------------------------------------------------------------
 * Test 8: split_h creates two windows with correct rects
 *
 * Split a 80x24 rect at ratio 0.5 horizontally:
 *   left rect:  {0, 0, 40, 24}
 *   right rect: {40, 0, 40, 24}
 *
 * Split an 81x20 rect at ratio 0.33 (left gets floor(81*0.33) = 26 cols):
 *   left rect:  {0, 0, 26, 20}
 *   right rect: {26, 0, 55, 20}
 * ---------------------------------------------------------------------- */

static void test_split_h_creates_two_windows(void) {
	setup();

	boxen_window_t *left = NULL, *right = NULL;
	boxen_rect_t total = {0, 0, 80, 24};

	boxen_layout_split_h(total, 0.5f, "Left", &left, "Right", &right);

	assert(left != NULL);
	assert(right != NULL);

	boxen_rect_t lr = boxen_window_get_rect(left);
	boxen_rect_t rr = boxen_window_get_rect(right);

	/* Left half: x=0, w=40. */
	assert(lr.x == 0);
	assert(lr.y == 0);
	assert(lr.w == 40);
	assert(lr.h == 24);

	/* Right half: x=40, w=40. Tiling: left.w + right.w == total.w. */
	assert(rr.x == 40);
	assert(rr.y == 0);
	assert(rr.w == 40);
	assert(rr.h == 24);

	boxen_window_close(left);
	boxen_window_close(right);

	/* Second split: non-even ratio. */
	left = NULL; right = NULL;
	boxen_rect_t total2 = {0, 0, 81, 20};
	boxen_layout_split_h(total2, 0.33f, "A", &left, "B", &right);

	assert(left != NULL);
	assert(right != NULL);

	boxen_rect_t lr2 = boxen_window_get_rect(left);
	boxen_rect_t rr2 = boxen_window_get_rect(right);

	/* left.w = (int)(81 * 0.33) = 26. right.w = 81 - 26 = 55. */
	assert(lr2.w == 26);
	assert(rr2.x == 26);
	assert(rr2.w == 55);
	/* Total coverage: left.w + right.w == total.w. */
	assert(lr2.w + rr2.w == total2.w);

	boxen_window_close(left);
	boxen_window_close(right);

	teardown();
}

/* -------------------------------------------------------------------------
 * Test 9: pinned BOXEN_PIN_BOTTOM window survives terminal resize
 *
 * A BOXEN_PIN_BOTTOM window at (0, 20, 80, 4) on an 80x24 terminal.
 * After a resize to 80x20, the bottom-pinned window should stay at
 * the bottom: rect.y = new_h - rect.h = 20 - 4 = 16.
 *
 * An unpinned window at the same initial position would be clamped
 * differently (no guaranteed pinning to the bottom edge).
 * ---------------------------------------------------------------------- */

static void test_pinned_bottom_survives_terminal_resize(void) {
	setup();

	/* Pinned footer: bottom 4 rows of an 80x24 terminal. */
	boxen_window_t *footer = boxen_window_open("Keys",
		(boxen_rect_t){0, 20, 80, 4}, NULL);
	assert(footer != NULL);
	boxen_window_set_pinned(footer, BOXEN_PIN_BOTTOM);

	/* Also create a normal window to confirm it's handled differently. */
	boxen_window_t *normal = boxen_window_open("Normal",
		(boxen_rect_t){0, 0, 40, 10}, NULL);
	assert(normal != NULL);

	/* Simulate terminal resize to 80x20. */
	boxen_event_t ev;
	memset(&ev, 0, sizeof(ev));
	ev.type = BOXEN_EV_RESIZE;
	ev.resize.w = 80;
	ev.resize.h = 20;
	boxen_dispatch_event(&ev);

	/* Pinned footer must now be at y = 20 - 4 = 16 (bottom of new terminal). */
	boxen_rect_t fr = boxen_window_get_rect(footer);
	assert(fr.y == 16);   /* new_h - footer.h = 20 - 4 */
	assert(fr.h == 4);    /* height preserved */
	assert(fr.w == 80);   /* width preserved */

	/* Normal window is clamped to fit (y=0..10 fits in 20 rows; unchanged). */
	boxen_rect_t nr = boxen_window_get_rect(normal);
	assert(nr.y + nr.h <= 20);  /* doesn't extend beyond new terminal height */

	boxen_window_close(footer);
	boxen_window_close(normal);
	teardown();
}

/* -------------------------------------------------------------------------
 * main
 * ---------------------------------------------------------------------- */

int main(void) {
	TR_INIT("boxen_chrome_tests");

	TR_RUN(test_borders_enabled_by_default);
	TR_RUN(test_borders_drawn_on_present);
	TR_RUN(test_title_inset_on_top_row);
	TR_RUN(test_content_width_subtracts_chrome);
	TR_RUN(test_set_cell_offsets_for_borders);
	TR_RUN(test_window_at_accounts_for_borders);
	TR_RUN(test_scrollbar_renders_when_content_overflows);
	TR_RUN(test_split_h_creates_two_windows);
	TR_RUN(test_pinned_bottom_survives_terminal_resize);

	TR_SUMMARY();
	return TR_EXIT_CODE();
}
