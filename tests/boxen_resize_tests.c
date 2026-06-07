/*
 * boxen_resize_tests.c -- unit tests for boxen A.4 (resize/move state machine).
 *
 * Covers:
 *   1. set_movable / set_resizable / set_min_size round-trip (getters verify via rect/behavior)
 *   2. Mouse drag on title bar moves a movable window
 *   3. Mouse drag on BR corner resizes a resizable window
 *   4. Resize drag clamps to min_size when dragged past the minimum
 *   5. Dragging title bar of an immovable window does not move it
 *   6. Dragging corner of an unresizable window does not resize it
 *   7. BOXEN_EV_RESIZE clamps windows to fit the new terminal dimensions
 *   8. BOXEN_EV_RESIZE respects min_size -- window stays at min dimensions
 *
 * Links: boxen_log.c, boxen_wcwidth.c, backend_mock.c, boxen.c
 * No Frontier runtime dependency.
 *
 * Run: make -C tests boxen_resize_tests && ./tests/boxen_resize_tests
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
 * Test helpers
 * ---------------------------------------------------------------------- */

static void setup(void) {
	boxen_mock_reset(80, 24);
	boxen_result_t r = boxen_init(boxen_mock_backend(), NULL, NULL);
	assert(r == BOXEN_OK);
}

static void teardown(void) {
	boxen_shutdown();
}

/* Push a mouse press event at (x, y). */
static void push_press(int x, int y) {
	boxen_mock_push_mouse(x, y, 1, true, 0);
}

/* Push a mouse release event at (x, y). */
static void push_release(int x, int y) {
	boxen_mock_push_mouse(x, y, 1, false, 0);
}

/* Push a mouse motion event at (x, y) -- button 0, not pressed. */
static void push_motion(int x, int y) {
	boxen_mock_push_mouse(x, y, 0, false, 0);
}

/* Drain one event from the queue by polling and dispatching it. */
static void drain_one(void) {
	boxen_event_t ev;
	boxen_result_t r = boxen_poll_event(&ev, 0);
	if (r == BOXEN_OK) {
		boxen_dispatch_event(&ev);
	}
}

/* -------------------------------------------------------------------------
 * Test 1: set_movable / set_resizable / set_min_size round-trip
 *
 * Verifies the setters do not crash and that the fields have observable
 * effect: a movable window's drag should move it; an immovable one should
 * not. (The behavioral tests below cover this more thoroughly; this test
 * just ensures the API itself is sane and the defaults are correct.)
 * ---------------------------------------------------------------------- */

static void test_set_movable_resizable_min_size_round_trip(void) {
	setup();

	boxen_window_t *win = boxen_window_open("W", (boxen_rect_t){5, 5, 20, 10}, NULL);
	assert(win != NULL);

	/* Default: not movable, not resizable, min_size = (1, 1) per spec. */
	/* Enable movable and resizable, then set a min size. */
	boxen_window_set_movable(win, true);
	boxen_window_set_resizable(win, true);
	boxen_window_set_min_size(win, 6, 4);

	/* Re-disable movable and resizable -- should not crash. */
	boxen_window_set_movable(win, false);
	boxen_window_set_resizable(win, false);

	/* Re-enable for a final check -- API must be idempotent. */
	boxen_window_set_movable(win, true);
	boxen_window_set_resizable(win, true);

	/* NULL guard: these must not crash. */
	boxen_window_set_movable(NULL, true);
	boxen_window_set_resizable(NULL, true);
	boxen_window_set_min_size(NULL, 5, 5);

	/* Window rect must be unchanged from open time by the setters alone. */
	boxen_rect_t r = boxen_window_get_rect(win);
	assert(r.x == 5);
	assert(r.y == 5);
	assert(r.w == 20);
	assert(r.h == 10);

	boxen_window_close(win);
	teardown();
}

/* -------------------------------------------------------------------------
 * Test 2: Mouse drag on title bar moves a movable window
 *
 * A movable window has its topmost row as the title bar. A press there
 * followed by motion events should move the window, and a release should
 * end the drag.
 *
 * Window: {10, 5, 20, 8} -- title row is y=5, x in [10..29].
 * Press at (15, 5): title bar hit.
 * Move to (18, 7): delta (+3, +2) -> expected new position (13, 7).
 * Release at (18, 7).
 * ---------------------------------------------------------------------- */

static void test_mouse_drag_title_moves_window(void) {
	setup();

	boxen_window_t *win = boxen_window_open("Drag", (boxen_rect_t){10, 5, 20, 8}, NULL);
	assert(win != NULL);
	boxen_window_set_movable(win, true);
	boxen_window_focus(win);

	/* Press on title bar: (15, 5) -- within [10..29] x [5] row */
	push_press(15, 5);
	drain_one();

	/* Drag to (18, 7): delta is (+3, +2) from anchor (15, 5) */
	push_motion(18, 7);
	drain_one();

	/* Verify position updated */
	boxen_rect_t r = boxen_window_get_rect(win);
	assert(r.x == 13);  /* 10 + (18-15) */
	assert(r.y == 7);   /* 5  + (7-5)  */
	assert(r.w == 20);  /* size unchanged */
	assert(r.h == 8);

	/* Release ends the drag */
	push_release(18, 7);
	drain_one();

	/* Position should be stable after release */
	r = boxen_window_get_rect(win);
	assert(r.x == 13);
	assert(r.y == 7);

	/* Additional motion after release: no further movement */
	push_motion(22, 9);
	drain_one();
	r = boxen_window_get_rect(win);
	assert(r.x == 13);
	assert(r.y == 7);

	boxen_window_close(win);
	teardown();
}

/* -------------------------------------------------------------------------
 * Test 3: Mouse drag on BR corner resizes a resizable window
 *
 * Window: {5, 3, 20, 10} -- BR corner is at (24, 12) [rect.x+w-1, rect.y+h-1].
 * Press at (24, 12): BR corner hit.
 * Motion to (27, 14): delta (+3, +2) -> new size (23, 12).
 * Release at (27, 14).
 * ---------------------------------------------------------------------- */

static void test_mouse_drag_corner_resizes_window(void) {
	setup();

	boxen_window_t *win = boxen_window_open("Resize", (boxen_rect_t){5, 3, 20, 10}, NULL);
	assert(win != NULL);
	boxen_window_set_resizable(win, true);
	boxen_window_focus(win);

	/* BR corner is at (5+20-1, 3+10-1) = (24, 12) */
	push_press(24, 12);
	drain_one();

	/* Drag to (27, 14): delta (+3, +2) */
	push_motion(27, 14);
	drain_one();

	boxen_rect_t r = boxen_window_get_rect(win);
	assert(r.x == 5);   /* origin unchanged for BR drag */
	assert(r.y == 3);
	assert(r.w == 23);  /* 20 + 3 */
	assert(r.h == 12);  /* 10 + 2 */

	/* Release ends the drag */
	push_release(27, 14);
	drain_one();

	r = boxen_window_get_rect(win);
	assert(r.w == 23);
	assert(r.h == 12);

	boxen_window_close(win);
	teardown();
}

/* -------------------------------------------------------------------------
 * Test 4: Resize drag clamps to min_size
 *
 * Window: {10, 5, 20, 10}, min_size (5, 3).
 * Drag BR corner inward past the minimum.
 * Window should stop at min_size, not go below.
 * ---------------------------------------------------------------------- */

static void test_resize_clamps_to_min_size(void) {
	setup();

	boxen_window_t *win = boxen_window_open("Clamp", (boxen_rect_t){10, 5, 20, 10}, NULL);
	assert(win != NULL);
	boxen_window_set_resizable(win, true);
	boxen_window_set_min_size(win, 5, 3);
	boxen_window_focus(win);

	/* BR corner: (10+20-1, 5+10-1) = (29, 14) */
	push_press(29, 14);
	drain_one();

	/* Drag far inward: to (10, 5) which would give size (1, 1) -- below min */
	push_motion(10, 5);
	drain_one();

	boxen_rect_t r = boxen_window_get_rect(win);
	assert(r.x == 10);  /* origin unchanged for BR */
	assert(r.y == 5);
	assert(r.w >= 5);   /* clamped to min_w */
	assert(r.h >= 3);   /* clamped to min_h */
	assert(r.w == 5);
	assert(r.h == 3);

	push_release(10, 5);
	drain_one();

	boxen_window_close(win);
	teardown();
}

/* -------------------------------------------------------------------------
 * Test 5: Dragging title of an immovable window does not move it
 *
 * Window: {10, 5, 20, 8}, movable = false (default).
 * Press on title bar, drag, release.
 * Window rect must remain unchanged.
 * ---------------------------------------------------------------------- */

static void test_mouse_drag_immovable_window_does_not_move(void) {
	setup();

	boxen_window_t *win = boxen_window_open("Fixed", (boxen_rect_t){10, 5, 20, 8}, NULL);
	assert(win != NULL);
	/* Do NOT set movable -- default is false */
	boxen_window_focus(win);

	/* Press on title bar */
	push_press(15, 5);
	drain_one();

	/* Drag */
	push_motion(20, 8);
	drain_one();

	boxen_rect_t r = boxen_window_get_rect(win);
	assert(r.x == 10);
	assert(r.y == 5);
	assert(r.w == 20);
	assert(r.h == 8);

	push_release(20, 8);
	drain_one();

	r = boxen_window_get_rect(win);
	assert(r.x == 10);
	assert(r.y == 5);

	boxen_window_close(win);
	teardown();
}

/* -------------------------------------------------------------------------
 * Test 6: Dragging corner of an unresizable window does not resize it
 *
 * Window: {5, 3, 20, 10}, resizable = false (default).
 * Press on BR corner, drag, release.
 * Window rect must remain unchanged.
 * ---------------------------------------------------------------------- */

static void test_mouse_drag_unresizable_window_does_not_resize(void) {
	setup();

	boxen_window_t *win = boxen_window_open("Fixed", (boxen_rect_t){5, 3, 20, 10}, NULL);
	assert(win != NULL);
	/* Do NOT set resizable -- default is false */
	boxen_window_focus(win);

	/* BR corner: (24, 12) */
	push_press(24, 12);
	drain_one();

	push_motion(27, 15);
	drain_one();

	boxen_rect_t r = boxen_window_get_rect(win);
	assert(r.x == 5);
	assert(r.y == 3);
	assert(r.w == 20);
	assert(r.h == 10);

	push_release(27, 15);
	drain_one();

	r = boxen_window_get_rect(win);
	assert(r.w == 20);
	assert(r.h == 10);

	boxen_window_close(win);
	teardown();
}

/* -------------------------------------------------------------------------
 * Test 7: BOXEN_EV_RESIZE clamps windows to new terminal dimensions
 *
 * Open two windows that together fill an 80x24 terminal.
 * Push a BOXEN_EV_RESIZE {w=60, h=20}.
 * Both windows should clamp so no part extends beyond (59, 19).
 * ---------------------------------------------------------------------- */

static void test_terminal_resize_clamps_windows(void) {
	setup();

	/* Window A: fills left half */
	boxen_window_t *a = boxen_window_open("A", (boxen_rect_t){0, 0, 40, 24}, NULL);
	assert(a != NULL);

	/* Window B: fills right half */
	boxen_window_t *b = boxen_window_open("B", (boxen_rect_t){40, 0, 40, 24}, NULL);
	assert(b != NULL);

	/* Resize event: shrink to 60x20 */
	boxen_mock_width_set(60);
	boxen_mock_height_set(20);

	boxen_event_t ev = {0};
	ev.type        = BOXEN_EV_RESIZE;
	ev.resize.w    = 60;
	ev.resize.h    = 20;
	boxen_mock_push_event(&ev);
	drain_one();

	/* Window A: started at {0,0,40,24} -- height should clamp to 20 */
	boxen_rect_t ra = boxen_window_get_rect(a);
	assert(ra.x + ra.w <= 60);
	assert(ra.y + ra.h <= 20);

	/* Window B: started at {40,0,40,24} -- right edge was 80; now must be <= 60 */
	boxen_rect_t rb = boxen_window_get_rect(b);
	assert(rb.x + rb.w <= 60);
	assert(rb.y + rb.h <= 20);

	boxen_window_close(a);
	boxen_window_close(b);
	teardown();
}

/* -------------------------------------------------------------------------
 * Test 8: BOXEN_EV_RESIZE respects min_size
 *
 * Open a window with min_size (5, 3).
 * Push a resize event smaller than the min_size.
 * Window should stay at min_size, not shrink below it.
 * ---------------------------------------------------------------------- */

static void test_terminal_resize_respects_min_size(void) {
	setup();

	boxen_window_t *win = boxen_window_open("Min", (boxen_rect_t){0, 0, 20, 10}, NULL);
	assert(win != NULL);
	boxen_window_set_min_size(win, 5, 3);

	/* Resize event: terminal shrinks to 3x2 (smaller than min_size) */
	boxen_mock_width_set(3);
	boxen_mock_height_set(2);

	boxen_event_t ev = {0};
	ev.type        = BOXEN_EV_RESIZE;
	ev.resize.w    = 3;
	ev.resize.h    = 2;
	boxen_mock_push_event(&ev);
	drain_one();

	boxen_rect_t r = boxen_window_get_rect(win);
	assert(r.w >= 5);  /* cannot go below min_w */
	assert(r.h >= 3);  /* cannot go below min_h */
	assert(r.w == 5);
	assert(r.h == 3);

	boxen_window_close(win);
	teardown();
}

/* -------------------------------------------------------------------------
 * Test 9: closing a window mid-drag clears g_drag (security P0 regression)
 *
 * Without the close-time drag-clear, the next mouse event after close
 * would dereference g_drag.win (freed). We can't observe the UAF directly
 * but we can verify that after close, a subsequent press on a fresh
 * window starts a NEW drag rather than continuing the old one's state.
 * ---------------------------------------------------------------------- */
static void test_close_during_drag_clears_state(void) {
	setup();
	boxen_rect_t r = {10, 10, 20, 10};
	boxen_window_t *w = boxen_window_open("w", r, NULL);
	assert(w != NULL);
	boxen_window_set_movable(w, true);

	/* Start a drag on title (y=10 is top row). */
	boxen_mock_push_mouse(15, 10, 1, true, 0);
	drain_one();
	/* Close the window mid-drag. */
	boxen_window_close(w);

	/* Open another window. Push a mouse motion that, pre-fix, would have
	 * tried to update the dead window's rect (UAF). Post-fix, drag state
	 * is cleared at close, so this motion event is just delivered. */
	boxen_window_t *w2 = boxen_window_open("w2", (boxen_rect_t){0, 0, 5, 5}, NULL);
	assert(w2 != NULL);
	boxen_mock_push_mouse(20, 15, 0, false, 0);
	drain_one();
	/* If we reach here without crashing, the close-time clear worked. */
	boxen_rect_t r2 = boxen_window_get_rect(w2);
	assert(r2.x == 0 && r2.y == 0);  /* w2 untouched */

	boxen_window_close(w2);
	teardown();
}

/* -------------------------------------------------------------------------
 * Test 10: drag respects BOXEN_MAX_DIMENSION cap (security P1 regression)
 *
 * Without clamp_rect_to_bounds() applied to drag updates, an extreme
 * mouse coordinate would drive a window dimension past 16384, causing
 * signed-int overflow in downstream arithmetic.
 * ---------------------------------------------------------------------- */
static void test_drag_respects_max_dimension(void) {
	setup();
	boxen_rect_t r = {5, 5, 20, 10};
	boxen_window_t *w = boxen_window_open("w", r, NULL);
	assert(w != NULL);
	boxen_window_set_movable(w, true);

	/* Start drag on title. */
	boxen_mock_push_mouse(10, 5, 1, true, 0);
	drain_one();
	/* Motion to an extreme x. */
	boxen_mock_push_mouse(99999, 5, 0, false, 0);
	drain_one();

	boxen_rect_t r_after = boxen_window_get_rect(w);
	/* Must be clamped to BOXEN_MAX_DIMENSION (16384) -- not at the extreme. */
	assert(r_after.x <= 16384);
	assert(r_after.x >= -16384);

	boxen_window_close(w);
	teardown();
}

/* -------------------------------------------------------------------------
 * Test 11: terminal resize during active drag cancels the drag
 *
 * Without this, start_rect is stale post-clamp and the next motion event
 * would jump the window by the pre-clamp delta.
 * ---------------------------------------------------------------------- */
static void test_terminal_resize_cancels_active_drag(void) {
	setup();
	boxen_rect_t r = {5, 5, 20, 10};
	boxen_window_t *w = boxen_window_open("w", r, NULL);
	assert(w != NULL);
	boxen_window_set_movable(w, true);

	boxen_mock_push_mouse(15, 5, 1, true, 0);  /* start drag */
	drain_one();

	/* Terminal resize while drag is active. */
	boxen_event_t ev = {0};
	ev.type     = BOXEN_EV_RESIZE;
	ev.resize.w = 40;
	ev.resize.h = 20;
	boxen_mock_width_set(40);
	boxen_mock_height_set(20);
	boxen_mock_push_event(&ev);
	drain_one();

	/* Now push a motion event. Pre-fix this would jump the window by the
	 * stale-delta. Post-fix the drag was cancelled so the motion is
	 * routed normally (no rect mutation). */
	boxen_rect_t before = boxen_window_get_rect(w);
	boxen_mock_push_mouse(25, 5, 0, false, 0);
	drain_one();
	boxen_rect_t after = boxen_window_get_rect(w);

	assert(after.x == before.x);
	assert(after.y == before.y);

	boxen_window_close(w);
	teardown();
}

/* -------------------------------------------------------------------------
 * Test 12: keyboard drag mode arrow keys move the focused window
 *
 * Covers the previously-untested DRAG_MOVING_KB code path. Note: the
 * BOXEN_KEY_CTRL_M entry chord is unreachable through the real tb2
 * backend (which translates ASCII 0x0D to BOXEN_KEY_ENTER instead).
 * This test exercises the state machine via direct event injection.
 * ---------------------------------------------------------------------- */
static void test_keyboard_drag_arrows_move_window(void) {
	setup();
	boxen_rect_t r = {10, 10, 20, 10};
	boxen_window_t *w = boxen_window_open("w", r, NULL);
	assert(w != NULL);
	boxen_window_set_movable(w, true);
	boxen_window_focus(w);

	/* Enter keyboard-move mode via Ctrl-M chord (synthetic event). */
	boxen_mock_push_key(BOXEN_KEY_CTRL_M, 0, 0);
	drain_one();

	/* Arrow keys: 2x RIGHT, 1x DOWN. */
	boxen_mock_push_key(BOXEN_KEY_RIGHT, 0, 0);
	boxen_mock_push_key(BOXEN_KEY_RIGHT, 0, 0);
	boxen_mock_push_key(BOXEN_KEY_DOWN,  0, 0);
	drain_one(); drain_one(); drain_one();

	boxen_rect_t r2 = boxen_window_get_rect(w);
	assert(r2.x == 12);  /* started at 10, +2 right */
	assert(r2.y == 11);  /* started at 10, +1 down */

	/* Escape exits the mode. */
	boxen_mock_push_key(BOXEN_KEY_ESCAPE, 0, 0);
	drain_one();

	/* After exit, arrow keys should NOT move the window. */
	boxen_mock_push_key(BOXEN_KEY_RIGHT, 0, 0);
	drain_one();
	boxen_rect_t r3 = boxen_window_get_rect(w);
	assert(r3.x == 12);  /* unchanged after exit */

	boxen_window_close(w);
	teardown();
}

/* -------------------------------------------------------------------------
 * Test 13: drag ends on release with button=0 (terminals that lose button
 * info on release; bar-raiser P1 regression)
 *
 * Some terminals encode mouse release as (pressed=false, button=0) rather
 * than (pressed=false, button=PREVIOUS). The press-to-release transition
 * tracker ensures we end the drag either way.
 * ---------------------------------------------------------------------- */
static void test_drag_ends_on_button_zero_release(void) {
	setup();
	boxen_rect_t r = {10, 10, 20, 10};
	boxen_window_t *w = boxen_window_open("w", r, NULL);
	assert(w != NULL);
	boxen_window_set_movable(w, true);

	/* Start drag on title with button=1 press. */
	boxen_mock_push_mouse(15, 10, 1, true, 0);
	drain_one();

	/* Release encoded as pressed=false with button=0. */
	boxen_mock_push_mouse(16, 11, 0, false, 0);
	drain_one();

	/* A SECOND motion event must NOT continue the drag (would jump the
	 * window). The release on the previous event ended it. */
	boxen_rect_t before = boxen_window_get_rect(w);
	boxen_mock_push_mouse(99, 99, 0, false, 0);
	drain_one();
	boxen_rect_t after = boxen_window_get_rect(w);

	assert(after.x == before.x);
	assert(after.y == before.y);

	boxen_window_close(w);
	teardown();
}

/* -------------------------------------------------------------------------
 * main
 * ---------------------------------------------------------------------- */

int main(void) {
	TR_INIT("boxen_resize_tests");

	TR_RUN(test_set_movable_resizable_min_size_round_trip);
	TR_RUN(test_mouse_drag_title_moves_window);
	TR_RUN(test_mouse_drag_corner_resizes_window);
	TR_RUN(test_resize_clamps_to_min_size);
	TR_RUN(test_mouse_drag_immovable_window_does_not_move);
	TR_RUN(test_mouse_drag_unresizable_window_does_not_resize);
	TR_RUN(test_terminal_resize_clamps_windows);
	TR_RUN(test_terminal_resize_respects_min_size);
	TR_RUN(test_close_during_drag_clears_state);
	TR_RUN(test_drag_respects_max_dimension);
	TR_RUN(test_terminal_resize_cancels_active_drag);
	TR_RUN(test_keyboard_drag_arrows_move_window);
	TR_RUN(test_drag_ends_on_button_zero_release);

	TR_SUMMARY();
	return TR_EXIT_CODE();
}
