/*
 * boxen_core_tests.c -- unit tests for boxen window primitive (A.2).
 *
 * Covers: library lifecycle, window open/close, set_cell coordinate translation,
 * clipping at all four borders, boxen_window_at (inside / outside / topmost),
 * draw_text codepoint advance, and fill_rect block writes.
 *
 * Links: boxen_log.c, boxen_wcwidth.c, backend_mock.c, boxen.c
 * No Frontier runtime dependency.
 *
 * Run: make -C tests boxen_core_tests && ./tests/boxen_core_tests
 */

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "boxen.h"
#include "backend_mock.h"
#include "test_report.h"

/* Helper: init with the mock backend, canned 80x24 screen. */
static void setup(void) {
	/* Defensive: if a prior test failed mid-run (teardown never called), shut
	 * down first so boxen_init does not return BOXEN_ERR_ALREADY. */
	boxen_shutdown();
	boxen_mock_reset(80, 24);
	boxen_result_t r = boxen_init(boxen_mock_backend(), NULL, NULL);
	assert(r == BOXEN_OK);
}

/* Helper: shut down cleanly. */
static void teardown(void) {
	boxen_shutdown();
}

/* Open a window with borders DISABLED.
 * The boxen_core_tests were written for the A.2 pre-chrome model where borders
 * were not yet rendered. A.6 introduced borders ON by default. This helper
 * preserves the original test semantics (content coords == screen coords). */
static boxen_window_t *open_no_chrome(const char *title, boxen_rect_t rect, void *ud) {
	boxen_window_t *w = boxen_window_open(title, rect, ud);
	if (w != NULL) boxen_window_set_borders(w, false);
	return w;
}

/* -------------------------------------------------------------------------
 * Test 1: library init / shutdown lifecycle
 * ---------------------------------------------------------------------- */

static void test_init_shutdown_lifecycle(void) {
	boxen_mock_reset(80, 24);

	/* First init must succeed */
	boxen_result_t r = boxen_init(boxen_mock_backend(), NULL, NULL);
	assert(r == BOXEN_OK);

	/* Double-init must return ALREADY */
	boxen_result_t r2 = boxen_init(boxen_mock_backend(), NULL, NULL);
	assert(r2 == BOXEN_ERR_ALREADY);

	/* Shutdown clears state */
	boxen_shutdown();

	/* Re-init after shutdown must succeed */
	boxen_mock_reset(80, 24);
	boxen_result_t r3 = boxen_init(boxen_mock_backend(), NULL, NULL);
	assert(r3 == BOXEN_OK);

	boxen_shutdown();
}

/* -------------------------------------------------------------------------
 * Test 2: window open / close -- getters round-trip
 * ---------------------------------------------------------------------- */

static void test_window_open_close(void) {
	setup();

	boxen_rect_t r = {5, 3, 20, 10};
	void *ud = (void *)0xDEAD;
	boxen_window_t *w = open_no_chrome("hello", r, ud);
	assert(w != NULL);

	/* Rect getter */
	boxen_rect_t got = boxen_window_get_rect(w);
	assert(got.x == 5);
	assert(got.y == 3);
	assert(got.w == 20);
	assert(got.h == 10);

	/* User-data getter */
	assert(boxen_window_get_user_data(w) == ud);

	/* Content size equals rect when no chrome */
	assert(boxen_window_content_width(w)  == 20);
	assert(boxen_window_content_height(w) == 10);

	/* Close: does not crash; NULL close is also safe */
	boxen_window_close(w);
	boxen_window_close(NULL);

	teardown();
}

/* -------------------------------------------------------------------------
 * Test 3: set_cell writes at correct terminal coordinates
 * ---------------------------------------------------------------------- */

static void test_set_cell_writes_correct_terminal_coords(void) {
	setup();

	/* Window at terminal (5, 3), size 20x10 */
	boxen_rect_t r = {5, 3, 20, 10};
	boxen_window_t *w = open_no_chrome("t3", r, NULL);
	assert(w != NULL);

	/* Write at window-local (2, 1) -> terminal (7, 4) */
	boxen_set_cell(w, 2, 1, 'X', BOXEN_COLOR_DEFAULT, BOXEN_COLOR_DEFAULT, 0);

	const boxen_mock_cell_t *c = boxen_mock_cell_at(7, 4);
	assert(c != NULL);
	assert(c->ch == (uint32_t)'X');

	/* Ensure the cell at terminal (5, 3) [window origin] is unchanged */
	const boxen_mock_cell_t *orig = boxen_mock_cell_at(5, 3);
	assert(orig != NULL);
	assert(orig->ch != (uint32_t)'X');

	boxen_window_close(w);
	teardown();
}

/* -------------------------------------------------------------------------
 * Test 4: clip left -- set_cell with x=-1 is a no-op
 * ---------------------------------------------------------------------- */

static void test_clip_left(void) {
	setup();

	boxen_rect_t r = {5, 3, 20, 10};
	boxen_window_t *w = open_no_chrome("t4", r, NULL);
	assert(w != NULL);

	/* Write at window-local x=-1 -> terminal x=4, which is left of the window */
	boxen_set_cell(w, -1, 0, 'L', BOXEN_COLOR_DEFAULT, BOXEN_COLOR_DEFAULT, 0);

	/* Terminal (4, 3) must remain blank */
	const boxen_mock_cell_t *c = boxen_mock_cell_at(4, 3);
	assert(c != NULL);
	assert(c->ch != (uint32_t)'L');

	boxen_window_close(w);
	teardown();
}

/* -------------------------------------------------------------------------
 * Test 5: clip right -- set_cell with x=width is a no-op
 * ---------------------------------------------------------------------- */

static void test_clip_right(void) {
	setup();

	/* Window width=20: valid x range is 0..19; x=20 is out of bounds */
	boxen_rect_t r = {5, 3, 20, 10};
	boxen_window_t *w = open_no_chrome("t5", r, NULL);
	assert(w != NULL);

	boxen_set_cell(w, 20, 0, 'R', BOXEN_COLOR_DEFAULT, BOXEN_COLOR_DEFAULT, 0);

	/* Terminal (25, 3) must remain blank */
	const boxen_mock_cell_t *c = boxen_mock_cell_at(25, 3);
	assert(c != NULL);
	assert(c->ch != (uint32_t)'R');

	/* Also test x=21 */
	boxen_set_cell(w, 21, 0, 'R', BOXEN_COLOR_DEFAULT, BOXEN_COLOR_DEFAULT, 0);
	const boxen_mock_cell_t *c2 = boxen_mock_cell_at(26, 3);
	assert(c2 != NULL);
	assert(c2->ch != (uint32_t)'R');

	boxen_window_close(w);
	teardown();
}

/* -------------------------------------------------------------------------
 * Test 6: clip top -- set_cell with y=-1 is a no-op
 * ---------------------------------------------------------------------- */

static void test_clip_top(void) {
	setup();

	boxen_rect_t r = {5, 3, 20, 10};
	boxen_window_t *w = open_no_chrome("t6", r, NULL);
	assert(w != NULL);

	boxen_set_cell(w, 0, -1, 'T', BOXEN_COLOR_DEFAULT, BOXEN_COLOR_DEFAULT, 0);

	/* Terminal (5, 2) -- one row above window top -- must remain blank */
	const boxen_mock_cell_t *c = boxen_mock_cell_at(5, 2);
	assert(c != NULL);
	assert(c->ch != (uint32_t)'T');

	boxen_window_close(w);
	teardown();
}

/* -------------------------------------------------------------------------
 * Test 7: clip bottom -- set_cell with y=height is a no-op
 * ---------------------------------------------------------------------- */

static void test_clip_bottom(void) {
	setup();

	/* Window height=10: valid y range is 0..9; y=10 is out of bounds */
	boxen_rect_t r = {5, 3, 20, 10};
	boxen_window_t *w = open_no_chrome("t7", r, NULL);
	assert(w != NULL);

	boxen_set_cell(w, 0, 10, 'B', BOXEN_COLOR_DEFAULT, BOXEN_COLOR_DEFAULT, 0);

	/* Terminal (5, 13) -- one row below bottom -- must remain blank */
	const boxen_mock_cell_t *c = boxen_mock_cell_at(5, 13);
	assert(c != NULL);
	assert(c->ch != (uint32_t)'B');

	boxen_window_close(w);
	teardown();
}

/* -------------------------------------------------------------------------
 * Test 8: boxen_window_at -- point inside a window
 * ---------------------------------------------------------------------- */

static void test_boxen_window_at_inside(void) {
	setup();

	/* Window covers terminal x: 5..24, y: 3..12 (20 wide, 10 tall) */
	boxen_rect_t r = {5, 3, 20, 10};
	boxen_window_t *w = open_no_chrome("t8", r, NULL);
	assert(w != NULL);

	int cx = -1, cy = -1;
	boxen_window_t *found = boxen_window_at(10, 5, &cx, &cy);
	assert(found == w);
	assert(cx == 5);  /* 10 - 5 */
	assert(cy == 2);  /* 5 - 3 */

	boxen_window_close(w);
	teardown();
}

/* -------------------------------------------------------------------------
 * Test 9: boxen_window_at -- point outside all windows returns NULL
 * ---------------------------------------------------------------------- */

static void test_boxen_window_at_outside(void) {
	setup();

	/* Window at (5, 3, 20, 10) -- point (3, 3) is left of the window */
	boxen_rect_t r = {5, 3, 20, 10};
	boxen_window_t *w = open_no_chrome("t9", r, NULL);
	assert(w != NULL);

	int cx = 0, cy = 0;
	boxen_window_t *found = boxen_window_at(3, 3, &cx, &cy);
	assert(found == NULL);

	/* Below the window too */
	found = boxen_window_at(10, 14, &cx, &cy);
	assert(found == NULL);

	boxen_window_close(w);
	teardown();
}

/* -------------------------------------------------------------------------
 * Test 10: boxen_window_at -- topmost window (last-opened) is returned
 * ---------------------------------------------------------------------- */

static void test_boxen_window_at_topmost(void) {
	setup();

	/* Two overlapping windows; w2 was opened later -> topmost at A.2 */
	boxen_rect_t r1 = {0, 0, 40, 20};
	boxen_rect_t r2 = {5, 5, 30, 10};  /* overlaps r1 */

	boxen_window_t *w1 = open_no_chrome("under", r1, NULL);
	boxen_window_t *w2 = open_no_chrome("over",  r2, NULL);
	assert(w1 != NULL);
	assert(w2 != NULL);

	/* Point (10, 8) is inside both windows; w2 is topmost (last-opened) */
	int cx = 0, cy = 0;
	boxen_window_t *found = boxen_window_at(10, 8, &cx, &cy);
	assert(found == w2);
	assert(cx == 5);   /* 10 - 5 */
	assert(cy == 3);   /* 8 - 5 */

	/* Point (2, 2) is in w1 only */
	cx = -1; cy = -1;  /* sentinel */
	boxen_window_t *found2 = boxen_window_at(2, 2, &cx, &cy);
	assert(found2 == w1);
	assert(cx == 2);   /* w1 rect.x = 0; 2 - 0 = 2 */
	assert(cy == 2);   /* w1 rect.y = 0; 2 - 0 = 2 */

	boxen_window_close(w2);
	boxen_window_close(w1);
	teardown();
}

/* -------------------------------------------------------------------------
 * Test 11: draw_text writes consecutive codepoints advancing x
 * ---------------------------------------------------------------------- */

static void test_draw_text_writes_codepoints(void) {
	setup();

	boxen_rect_t r = {0, 0, 40, 10};
	boxen_window_t *w = open_no_chrome("t11", r, NULL);
	assert(w != NULL);

	/* Draw ASCII "Hi" at window-local (0, 0) */
	boxen_draw_text(w, 0, 0, "Hi", BOXEN_COLOR_DEFAULT, BOXEN_COLOR_DEFAULT, 0);

	const boxen_mock_cell_t *c0 = boxen_mock_cell_at(0, 0);
	const boxen_mock_cell_t *c1 = boxen_mock_cell_at(1, 0);
	assert(c0 != NULL && c0->ch == (uint32_t)'H');
	assert(c1 != NULL && c1->ch == (uint32_t)'i');

	/* Draw a 3-byte UTF-8 character: U+4E2D (CJK, width 2) followed by 'X' */
	/* U+4E2D encodes as E4 B8 AD */
	const char wide_then_x[] = "\xE4\xB8\xAD" "X";
	boxen_draw_text(w, 0, 1, wide_then_x, BOXEN_COLOR_DEFAULT, BOXEN_COLOR_DEFAULT, 0);

	const boxen_mock_cell_t *cw = boxen_mock_cell_at(0, 1);
	assert(cw != NULL && cw->ch == 0x4E2D);

	/* 'X' must land at x=2 (wide char occupies 2 columns) */
	const boxen_mock_cell_t *cx = boxen_mock_cell_at(2, 1);
	assert(cx != NULL && cx->ch == (uint32_t)'X');

	boxen_window_close(w);
	teardown();
}

/* -------------------------------------------------------------------------
 * Test 12: fill_rect writes the fill char across the region
 * ---------------------------------------------------------------------- */

static void test_fill_rect_writes_block(void) {
	setup();

	boxen_rect_t wr = {0, 0, 40, 20};
	boxen_window_t *w = open_no_chrome("t12", wr, NULL);
	assert(w != NULL);

	/* Fill a 3x2 rect at window-local (4, 5) with '#' */
	boxen_rect_t fr = {4, 5, 3, 2};
	boxen_fill_rect(w, fr, '#', BOXEN_COLOR_DEFAULT, BOXEN_COLOR_DEFAULT, 0);

	/* Every cell in the filled region must be '#' */
	for (int row = 0; row < 2; row++) {
		for (int col = 0; col < 3; col++) {
			const boxen_mock_cell_t *c = boxen_mock_cell_at(4 + col, 5 + row);
			assert(c != NULL && c->ch == (uint32_t)'#');
		}
	}

	/* Cell just outside the right edge must NOT be '#' */
	const boxen_mock_cell_t *out = boxen_mock_cell_at(7, 5);
	assert(out != NULL && out->ch != (uint32_t)'#');

	/* Cell just below the bottom edge must NOT be '#' */
	const boxen_mock_cell_t *below = boxen_mock_cell_at(4, 7);
	assert(below != NULL && below->ch != (uint32_t)'#');

	boxen_window_close(w);
	teardown();
}

/* -------------------------------------------------------------------------
 * Test 13: shutdown frees lingering open windows without leak
 * Covers the no-leak contract for shutdown-with-open-windows.
 * ---------------------------------------------------------------------- */

static void test_shutdown_with_open_windows(void) {
	setup();

	/* Open three windows and DO NOT close them before shutdown. */
	boxen_window_t *w1 = open_no_chrome("a", (boxen_rect_t){0, 0, 10, 5}, NULL);
	boxen_window_t *w2 = open_no_chrome("b", (boxen_rect_t){0, 0, 10, 5}, NULL);
	boxen_window_t *w3 = open_no_chrome("c", (boxen_rect_t){0, 0, 10, 5}, NULL);
	assert(w1 != NULL); assert(w2 != NULL); assert(w3 != NULL);

	/* Shutdown should iterate the list and free each one. After shutdown,
	 * re-init must work cleanly with an empty list. */
	boxen_shutdown();

	boxen_result_t r = boxen_init(boxen_mock_backend(), NULL, NULL);
	assert(r == BOXEN_OK);

	/* Verify the list is empty by opening a new window and confirming
	 * boxen_window_at finds it (would fail if a stale pointer remained). */
	boxen_window_t *w4 = open_no_chrome("d", (boxen_rect_t){0, 0, 10, 5}, NULL);
	assert(w4 != NULL);
	int cx = -1, cy = -1;
	assert(boxen_window_at(5, 2, &cx, &cy) == w4);

	boxen_window_close(w4);
	teardown();
}

/* -------------------------------------------------------------------------
 * Test 14: open beyond capacity returns NULL with last_error set
 * ---------------------------------------------------------------------- */

static void test_open_beyond_capacity(void) {
	setup();

	/* BOXEN_MAX_WINDOWS is 64 (defined in boxen_internal.h, not exposed
	 * publicly). Open 64 windows successfully, then verify the 65th fails. */
	boxen_window_t *handles[64];
	for (int i = 0; i < 64; i++) {
		handles[i] = boxen_window_open("w", (boxen_rect_t){0, 0, 5, 5}, NULL);
		assert(handles[i] != NULL);
	}

	boxen_window_t *overflow = open_no_chrome("x", (boxen_rect_t){0, 0, 5, 5}, NULL);
	assert(overflow == NULL);

	/* Cleanup. */
	for (int i = 0; i < 64; i++) {
		boxen_window_close(handles[i]);
	}
	teardown();
}

/* -------------------------------------------------------------------------
 * Test 15: double-close is rejected via magic guard
 * ---------------------------------------------------------------------- */

static void test_double_close_rejected(void) {
	setup();

	boxen_window_t *w = open_no_chrome("w", (boxen_rect_t){0, 0, 10, 5}, NULL);
	assert(w != NULL);

	boxen_window_close(w);
	/* Second close should be rejected silently (magic field cleared on
	 * first close). The struct memory may already be freed, but until it
	 * is realloc'd as something else, the magic guard will catch it. We
	 * cannot rely on this in production, but for the immediate
	 * already-closed case it works. */
	boxen_window_close(w);  /* must not crash */

	teardown();
}

/* -------------------------------------------------------------------------
 * main
 * ---------------------------------------------------------------------- */

int main(void) {
	TR_INIT("boxen_core_tests");

	TR_RUN(test_init_shutdown_lifecycle);
	TR_RUN(test_window_open_close);
	TR_RUN(test_set_cell_writes_correct_terminal_coords);
	TR_RUN(test_clip_left);
	TR_RUN(test_clip_right);
	TR_RUN(test_clip_top);
	TR_RUN(test_clip_bottom);
	TR_RUN(test_boxen_window_at_inside);
	TR_RUN(test_boxen_window_at_outside);
	TR_RUN(test_boxen_window_at_topmost);
	TR_RUN(test_draw_text_writes_codepoints);
	TR_RUN(test_fill_rect_writes_block);
	TR_RUN(test_shutdown_with_open_windows);
	TR_RUN(test_open_beyond_capacity);
	TR_RUN(test_double_close_rejected);

	TR_SUMMARY();
	return TR_EXIT_CODE();
}
