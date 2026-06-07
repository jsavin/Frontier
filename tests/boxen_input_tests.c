/*
 * boxen_input_tests.c -- unit tests for boxen A.3 (z-order, redraw, focus, input dispatch, modal).
 *
 * Covers:
 *   - boxen_present() calls draw_fn back-to-front and skips zero-size windows
 *   - boxen_window_raise() / boxen_window_lower() reorders the window stack
 *   - boxen_window_focus() sets the focused flag and raises the window
 *   - Key events dispatched to the focused window only
 *   - Modal input blocking: non-modal windows do not receive input when a modal is present
 *   - Mouse events dispatched to the topmost window at the event's (x, y)
 *   - boxen_run() loop terminates when boxen_quit() is called from an input callback
 *   - boxen_dispatch_event() dispatches correctly when called manually
 *
 * Links: boxen_log.c, boxen_wcwidth.c, backend_mock.c, boxen.c
 * No Frontier runtime dependency.
 *
 * Run: make -C tests boxen_input_tests && ./tests/boxen_input_tests
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

/* -------------------------------------------------------------------------
 * Shared callback tracking globals.
 * Reset at the top of each test that uses them.
 * ---------------------------------------------------------------------- */

static int g_draw_order[8];   /* records indices of windows in the order draw_fn fires */
static int g_draw_count = 0;

static bool g_input_a_fired = false;
static bool g_input_b_fired = false;

/* draw callbacks that record call order by writing a tag int */
static void draw_fn_tag(boxen_window_t *win, void *user_data) {
	(void)win;
	if (g_draw_count < 8) {
		g_draw_order[g_draw_count++] = *(int *)user_data;
	}
}

/* input callbacks that flip a flag */
static void input_fn_a(boxen_window_t *win, const boxen_event_t *ev, void *user_data) {
	(void)win; (void)ev; (void)user_data;
	g_input_a_fired = true;
}

static void input_fn_b(boxen_window_t *win, const boxen_event_t *ev, void *user_data) {
	(void)win; (void)ev; (void)user_data;
	g_input_b_fired = true;
}

/* input callback that calls boxen_quit(), used by test_quit_terminates_run_loop */
static void input_fn_quit(boxen_window_t *win, const boxen_event_t *ev, void *user_data) {
	(void)win; (void)ev; (void)user_data;
	boxen_quit();
}

/* -------------------------------------------------------------------------
 * Test 1: present() calls draw_fn back-to-front
 *
 * Open window A (back), then window B (front).  A.draw_fn must fire before
 * B.draw_fn when boxen_present() is called.
 * ---------------------------------------------------------------------- */

static void test_present_walks_windows_back_to_front(void) {
	setup();

	g_draw_count = 0;
	memset(g_draw_order, 0, sizeof(g_draw_order));

	static int tag_a = 1;
	static int tag_b = 2;

	boxen_window_t *a = boxen_window_open("A", (boxen_rect_t){0, 0, 20, 10}, NULL);
	assert(a != NULL);
	boxen_window_set_draw(a, draw_fn_tag);
	boxen_window_set_user_data(a, &tag_a);

	boxen_window_t *b = boxen_window_open("B", (boxen_rect_t){20, 0, 20, 10}, NULL);
	assert(b != NULL);
	boxen_window_set_draw(b, draw_fn_tag);
	boxen_window_set_user_data(b, &tag_b);

	boxen_present();

	/* A (tag 1) must have been drawn before B (tag 2) */
	assert(g_draw_count == 2);
	assert(g_draw_order[0] == 1);  /* A first */
	assert(g_draw_order[1] == 2);  /* B second */

	teardown();
}

/* -------------------------------------------------------------------------
 * Test 2: present() skips zero-size windows
 *
 * A window with rect.w == 0 must not have its draw_fn called.
 * ---------------------------------------------------------------------- */

static void test_present_skips_zero_size_window(void) {
	setup();

	g_draw_count = 0;
	memset(g_draw_order, 0, sizeof(g_draw_order));

	static int tag_normal = 10;
	static int tag_zero   = 20;

	/* Normal window */
	boxen_window_t *normal = boxen_window_open("N", (boxen_rect_t){0, 0, 20, 10}, NULL);
	assert(normal != NULL);
	boxen_window_set_draw(normal, draw_fn_tag);
	boxen_window_set_user_data(normal, &tag_normal);

	/* Zero-width window -- draw_fn must NOT fire */
	boxen_window_t *zero = boxen_window_open("Z", (boxen_rect_t){30, 0, 0, 10}, NULL);
	assert(zero != NULL);
	boxen_window_set_draw(zero, draw_fn_tag);
	boxen_window_set_user_data(zero, &tag_zero);

	boxen_present();

	/* Only the normal window's draw_fn should have fired */
	assert(g_draw_count == 1);
	assert(g_draw_order[0] == 10);

	teardown();
}

/* -------------------------------------------------------------------------
 * Test 3: raise() moves a window to the top of the z-order
 *
 * Open A then B (B is on top by creation order).  Raise A.  Verify A is
 * now on top by calling boxen_window_at() at the overlapping point.
 * ---------------------------------------------------------------------- */

static void test_raise_moves_window_to_top(void) {
	setup();

	/* Both windows overlap at (5, 5) */
	boxen_window_t *a = boxen_window_open("A", (boxen_rect_t){0, 0, 20, 20}, NULL);
	assert(a != NULL);
	boxen_window_t *b = boxen_window_open("B", (boxen_rect_t){0, 0, 20, 20}, NULL);
	assert(b != NULL);

	/* Before raise: B is on top (created last) */
	int cx, cy;
	boxen_window_t *top = boxen_window_at(5, 5, &cx, &cy);
	assert(top == b);

	/* Raise A: A should now be on top */
	boxen_window_raise(a);
	top = boxen_window_at(5, 5, &cx, &cy);
	assert(top == a);

	teardown();
}

/* -------------------------------------------------------------------------
 * Test 4: lower() moves a window to the bottom of the z-order
 *
 * Open A then B.  Lower B.  A should now be on top.
 * ---------------------------------------------------------------------- */

static void test_lower_moves_window_to_bottom(void) {
	setup();

	/* Both windows overlap */
	boxen_window_t *a = boxen_window_open("A", (boxen_rect_t){0, 0, 20, 20}, NULL);
	assert(a != NULL);
	boxen_window_t *b = boxen_window_open("B", (boxen_rect_t){0, 0, 20, 20}, NULL);
	assert(b != NULL);

	/* Before lower: B is on top */
	int cx, cy;
	assert(boxen_window_at(5, 5, &cx, &cy) == b);

	/* Lower B: A should now be on top */
	boxen_window_lower(b);
	assert(boxen_window_at(5, 5, &cx, &cy) == a);

	teardown();
}

/* -------------------------------------------------------------------------
 * Test 5: focus() sets the focused flag on the target window
 *
 * Verify w->focused is set after boxen_window_focus(w).  We probe this
 * indirectly through input dispatch: only the focused window receives the key.
 * ---------------------------------------------------------------------- */

static void test_focus_sets_focused_flag(void) {
	setup();

	g_input_a_fired = false;
	g_input_b_fired = false;

	boxen_window_t *a = boxen_window_open("A", (boxen_rect_t){0, 0, 40, 24}, NULL);
	assert(a != NULL);
	boxen_window_set_input(a, input_fn_a);

	boxen_window_t *b = boxen_window_open("B", (boxen_rect_t){0, 0, 40, 24}, NULL);
	assert(b != NULL);
	boxen_window_set_input(b, input_fn_b);

	/* Focus A */
	boxen_window_focus(a);

	/* Push and dispatch a key event */
	boxen_event_t ev;
	memset(&ev, 0, sizeof(ev));
	ev.type     = BOXEN_EV_KEY;
	ev.key.key  = BOXEN_KEY_F1;
	boxen_dispatch_event(&ev);

	/* Only A should have received the event */
	assert(g_input_a_fired == true);
	assert(g_input_b_fired == false);

	teardown();
}

/* -------------------------------------------------------------------------
 * Test 6: focus() raises the target window to the top
 *
 * Open A then B (B on top).  Focus A.  A must now be topmost.
 * ---------------------------------------------------------------------- */

static void test_focus_raises_window(void) {
	setup();

	boxen_window_t *a = boxen_window_open("A", (boxen_rect_t){0, 0, 20, 20}, NULL);
	assert(a != NULL);
	boxen_window_t *b = boxen_window_open("B", (boxen_rect_t){0, 0, 20, 20}, NULL);
	assert(b != NULL);

	/* Sanity: B is on top before focus */
	int cx, cy;
	assert(boxen_window_at(5, 5, &cx, &cy) == b);

	/* Focus A: must raise A to top */
	boxen_window_focus(a);
	assert(boxen_window_at(5, 5, &cx, &cy) == a);

	teardown();
}

/* -------------------------------------------------------------------------
 * Test 7: key event dispatched to focused window only
 *
 * Open A and B.  Focus A.  Push a key event via boxen_dispatch_event.
 * A's input_fn fires; B's does not.
 * ---------------------------------------------------------------------- */

static void test_key_event_dispatches_to_focused(void) {
	setup();

	g_input_a_fired = false;
	g_input_b_fired = false;

	boxen_window_t *a = boxen_window_open("A", (boxen_rect_t){0, 0, 40, 24}, NULL);
	assert(a != NULL);
	boxen_window_set_input(a, input_fn_a);

	boxen_window_t *b = boxen_window_open("B", (boxen_rect_t){0, 0, 40, 24}, NULL);
	assert(b != NULL);
	boxen_window_set_input(b, input_fn_b);

	boxen_window_focus(a);

	boxen_event_t ev;
	memset(&ev, 0, sizeof(ev));
	ev.type    = BOXEN_EV_KEY;
	ev.key.key = BOXEN_KEY_ENTER;
	boxen_dispatch_event(&ev);

	assert(g_input_a_fired == true);
	assert(g_input_b_fired == false);

	teardown();
}

/* -------------------------------------------------------------------------
 * Test 8: modal window blocks input to non-modal windows
 *
 * Open non-modal window A (with input_fn) and modal window B.
 * Push a key event.  B's input_fn fires; A's does not.
 * ---------------------------------------------------------------------- */

static void test_modal_blocks_input_to_non_modal(void) {
	setup();

	g_input_a_fired = false;
	g_input_b_fired = false;

	/* A is the background, non-modal window */
	boxen_window_t *a = boxen_window_open("A", (boxen_rect_t){0, 0, 80, 24}, NULL);
	assert(a != NULL);
	boxen_window_set_input(a, input_fn_a);
	boxen_window_focus(a);  /* A is focused */

	/* B is the modal overlay */
	boxen_window_t *b = boxen_window_open("B", (boxen_rect_t){20, 5, 40, 14}, NULL);
	assert(b != NULL);
	boxen_window_set_input(b, input_fn_b);
	boxen_window_set_modal(b, true);

	/* Key event -- must go to modal B, not to A */
	boxen_event_t ev;
	memset(&ev, 0, sizeof(ev));
	ev.type    = BOXEN_EV_KEY;
	ev.key.key = BOXEN_KEY_ESCAPE;
	boxen_dispatch_event(&ev);

	assert(g_input_a_fired == false);  /* modal blocked A */
	assert(g_input_b_fired == true);   /* modal received it */

	teardown();
}

/* -------------------------------------------------------------------------
 * Test 9: mouse event dispatched to topmost window at click point
 *
 * Open A (left half) and B (right half), non-overlapping.  Push a mouse
 * press at a point inside B.  B's input_fn fires; A's does not.
 * ---------------------------------------------------------------------- */

static void test_mouse_event_dispatches_to_window_at_point(void) {
	setup();

	g_input_a_fired = false;
	g_input_b_fired = false;

	/* A occupies the left half: x=[0,39], y=[0,23] */
	boxen_window_t *a = boxen_window_open("A", (boxen_rect_t){0, 0, 40, 24}, NULL);
	assert(a != NULL);
	boxen_window_set_input(a, input_fn_a);

	/* B occupies the right half: x=[40,79], y=[0,23] */
	boxen_window_t *b = boxen_window_open("B", (boxen_rect_t){40, 0, 40, 24}, NULL);
	assert(b != NULL);
	boxen_window_set_input(b, input_fn_b);

	/* No modal; focus A, but click is inside B */
	boxen_window_focus(a);

	/* Mouse press at (60, 10) -- inside B's rect */
	boxen_event_t ev;
	memset(&ev, 0, sizeof(ev));
	ev.type          = BOXEN_EV_MOUSE;
	ev.mouse.x       = 60;
	ev.mouse.y       = 10;
	ev.mouse.button  = 1;
	ev.mouse.pressed = true;
	boxen_dispatch_event(&ev);

	assert(g_input_a_fired == false);
	assert(g_input_b_fired == true);

	teardown();
}

/* -------------------------------------------------------------------------
 * Test 10: boxen_run() terminates when boxen_quit() is called
 *
 * Pre-load one key event, then call boxen_run().  The input_fn calls
 * boxen_quit().  boxen_run() must return promptly.
 *
 * Because boxen_run() uses timeout_ms=100 internally, the mock backend
 * will return BOXEN_ERR_TIMEOUT after the pre-loaded event queue drains,
 * at which point the quit flag should already be set and the loop exits.
 * ---------------------------------------------------------------------- */

static void test_quit_terminates_run_loop(void) {
	setup();

	boxen_window_t *w = boxen_window_open("W", (boxen_rect_t){0, 0, 80, 24}, NULL);
	assert(w != NULL);
	boxen_window_set_input(w, input_fn_quit);
	boxen_window_focus(w);

	/* Pre-load a key event so the first poll returns immediately */
	boxen_mock_push_key(BOXEN_KEY_ESCAPE, 0, 0);

	/* boxen_run() should dispatch the event, call boxen_quit(), and exit.
	 * The loop polls with a short timeout; after the event is consumed and
	 * g_should_quit is set, it exits on the next timeout iteration. */
	boxen_run();

	/* If we reach here, the loop terminated. */

	teardown();
}

/* -------------------------------------------------------------------------
 * Test 11: focus rejects stale (closed) pointer
 *
 * Regression test for the security review P1: boxen_window_focus must not
 * plant g_focused_window with a pointer to a closed/freed window, because
 * the next dispatch would deref it and UAF. We open a window, close it,
 * then call focus(stale) and verify dispatch with KEY routes to NO window
 * (input_fn not invoked anywhere).
 * ---------------------------------------------------------------------- */

static int g_stale_input_count = 0;
static void stale_input_fn(boxen_window_t *win, const boxen_event_t *ev,
                           void *user_data) {
	(void)win; (void)ev; (void)user_data;
	g_stale_input_count++;
}

static void test_focus_rejects_stale_pointer(void) {
	setup();
	g_stale_input_count = 0;

	/* Open a live window, then open + close another. */
	boxen_rect_t r = {0, 0, 10, 5};
	boxen_window_t *live  = boxen_window_open("live",  r, NULL);
	boxen_window_t *stale = boxen_window_open("stale", r, NULL);
	assert(live != NULL);
	assert(stale != NULL);
	boxen_window_set_input(live,  stale_input_fn);
	boxen_window_set_input(stale, stale_input_fn);

	boxen_window_close(stale);
	/* `stale` is now a dangling pointer (struct freed). */

	/* Attempt to focus the stale pointer: must reject silently. */
	boxen_window_focus(stale);

	/* Dispatch a key event. With the P1 fix, g_focused_window is unchanged
	 * (still NULL because we never focused `live`), so no input_fn fires.
	 * Pre-fix behavior: g_focused_window would point at freed memory and
	 * dispatch would deref it (UAF). */
	boxen_event_t ev = {0};
	ev.type     = BOXEN_EV_KEY;
	ev.key.key  = BOXEN_KEY_ENTER;
	boxen_dispatch_event(&ev);

	assert(g_stale_input_count == 0);

	/* Sanity: focusing a LIVE window still works. */
	boxen_window_focus(live);
	boxen_dispatch_event(&ev);
	assert(g_stale_input_count == 1);

	boxen_window_close(live);
	teardown();
}

/* -------------------------------------------------------------------------
 * main
 * ---------------------------------------------------------------------- */

int main(void) {
	TR_INIT("boxen_input_tests");

	TR_RUN(test_present_walks_windows_back_to_front);
	TR_RUN(test_present_skips_zero_size_window);
	TR_RUN(test_raise_moves_window_to_top);
	TR_RUN(test_lower_moves_window_to_bottom);
	TR_RUN(test_focus_sets_focused_flag);
	TR_RUN(test_focus_raises_window);
	TR_RUN(test_key_event_dispatches_to_focused);
	TR_RUN(test_modal_blocks_input_to_non_modal);
	TR_RUN(test_mouse_event_dispatches_to_window_at_point);
	TR_RUN(test_quit_terminates_run_loop);
	TR_RUN(test_focus_rejects_stale_pointer);

	TR_SUMMARY();
	return TR_EXIT_CODE();
}
