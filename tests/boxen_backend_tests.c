/*
 * boxen_backend_tests.c -- unit tests for the boxen mock backend (A.1).
 *
 * Tests the mock backend's cell grid, event queue, and double-click synthesis.
 * Also verifies boxen_wcwidth() behavior.
 *
 * Links: boxen_wcwidth.c, boxen_log.c, backend_mock.c (no backend_tb2.c).
 * No Frontier runtime dependency.
 *
 * Run: make -C tests boxen_backend_tests && ./tests/boxen_backend_tests
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
 * Static timestamp helpers for double-click tests
 * ---------------------------------------------------------------------- */

static uint64_t g_fake_now_ms = 0;

static uint64_t fake_clock(void) {
	return g_fake_now_ms;
}

/* -------------------------------------------------------------------------
 * Test 1: init and dimensions
 * ---------------------------------------------------------------------- */

static void test_mock_init_and_dimensions(void) {
	boxen_mock_reset(80, 24);

	const boxen_backend_t *b = boxen_mock_backend();
	assert(b != NULL);
	assert(b->width() == 80);
	assert(b->height() == 24);

	/* Corner cells should be blank (codepoint 0 or space, no color) */
	const boxen_mock_cell_t *tl = boxen_mock_cell_at(0, 0);
	assert(tl != NULL);
	assert(tl->ch == 0 || tl->ch == (uint32_t)' ');

	const boxen_mock_cell_t *br = boxen_mock_cell_at(79, 23);
	assert(br != NULL);
	assert(br->ch == 0 || br->ch == (uint32_t)' ');

	/* Out-of-bounds returns NULL */
	assert(boxen_mock_cell_at(-1, 0) == NULL);
	assert(boxen_mock_cell_at(80, 0) == NULL);
	assert(boxen_mock_cell_at(0, 24) == NULL);
}

/* -------------------------------------------------------------------------
 * Test 2: set_cell writes the grid
 * ---------------------------------------------------------------------- */

static void test_mock_set_cell_writes_grid(void) {
	boxen_mock_reset(40, 20);

	const boxen_backend_t *b = boxen_mock_backend();
	b->set_cell(3, 4, 'A', 2, 5, BOXEN_ATTR_BOLD);

	const boxen_mock_cell_t *c = boxen_mock_cell_at(3, 4);
	assert(c != NULL);
	assert(c->ch   == (uint32_t)'A');
	assert(c->fg   == 2);
	assert(c->bg   == 5);
	assert(c->attr == BOXEN_ATTR_BOLD);

	/* Neighboring cell must be untouched */
	const boxen_mock_cell_t *nbr = boxen_mock_cell_at(4, 4);
	assert(nbr != NULL);
	assert(nbr->ch != (uint32_t)'A');
}

/* -------------------------------------------------------------------------
 * Test 3: event queue is FIFO; third poll returns TIMEOUT
 * ---------------------------------------------------------------------- */

static void test_mock_event_queue_fifo(void) {
	boxen_mock_reset(80, 24);

	boxen_mock_push_key(BOXEN_KEY_NONE, 'A', BOXEN_MOD_NONE);
	boxen_mock_push_key(BOXEN_KEY_NONE, 'B', BOXEN_MOD_NONE);

	const boxen_backend_t *b = boxen_mock_backend();
	boxen_event_t ev;

	/* First event: 'A' */
	int r1 = b->poll_event(&ev, 0);
	assert(r1 == BOXEN_OK);
	assert(ev.type == BOXEN_EV_KEY);
	assert(ev.key.ch == 'A');

	/* Second event: 'B' */
	int r2 = b->poll_event(&ev, 0);
	assert(r2 == BOXEN_OK);
	assert(ev.type == BOXEN_EV_KEY);
	assert(ev.key.ch == 'B');

	/* Third poll: queue empty -> TIMEOUT */
	int r3 = b->poll_event(&ev, 0);
	assert(r3 == BOXEN_ERR_TIMEOUT);
}

/* -------------------------------------------------------------------------
 * Test 4: double-click synthesized within window
 * ---------------------------------------------------------------------- */

static void test_mock_double_click_synthesized(void) {
	boxen_mock_reset(80, 24);
	g_fake_now_ms = 0;
	boxen_mock_set_now_ms_fn(fake_clock);

	/* First press at t=0 */
	g_fake_now_ms = 0;
	boxen_mock_push_mouse(10, 5, 1, true, BOXEN_MOD_NONE);

	/* Second press at t=100ms (within 500ms window) */
	g_fake_now_ms = 100;
	boxen_mock_push_mouse(10, 5, 1, true, BOXEN_MOD_NONE);

	const boxen_backend_t *b = boxen_mock_backend();
	boxen_event_t ev;

	/* First event: no double-click flag */
	int r1 = b->poll_event(&ev, 0);
	assert(r1 == BOXEN_OK);
	assert(ev.type == BOXEN_EV_MOUSE);
	assert(ev.mouse.pressed == true);
	assert((ev.mouse.flags & BOXEN_MOUSE_DOUBLE_CLICK) == 0);

	/* Second event: double-click flag set */
	int r2 = b->poll_event(&ev, 0);
	assert(r2 == BOXEN_OK);
	assert(ev.type == BOXEN_EV_MOUSE);
	assert(ev.mouse.pressed == true);
	assert((ev.mouse.flags & BOXEN_MOUSE_DOUBLE_CLICK) != 0);

	boxen_mock_set_now_ms_fn(NULL);  /* restore default */
}

/* -------------------------------------------------------------------------
 * Test 5: double-click NOT synthesized when too slow (>500ms)
 * ---------------------------------------------------------------------- */

static void test_mock_double_click_too_slow(void) {
	boxen_mock_reset(80, 24);
	g_fake_now_ms = 0;
	boxen_mock_set_now_ms_fn(fake_clock);

	g_fake_now_ms = 0;
	boxen_mock_push_mouse(10, 5, 1, true, BOXEN_MOD_NONE);

	g_fake_now_ms = 600;  /* 600ms > 500ms threshold */
	boxen_mock_push_mouse(10, 5, 1, true, BOXEN_MOD_NONE);

	const boxen_backend_t *b = boxen_mock_backend();
	boxen_event_t ev;

	b->poll_event(&ev, 0);  /* first press -- consume */

	int r = b->poll_event(&ev, 0);
	assert(r == BOXEN_OK);
	assert(ev.type == BOXEN_EV_MOUSE);
	assert(ev.mouse.pressed == true);
	assert((ev.mouse.flags & BOXEN_MOUSE_DOUBLE_CLICK) == 0);

	boxen_mock_set_now_ms_fn(NULL);
}

/* -------------------------------------------------------------------------
 * Test 6: double-click NOT synthesized when too far away
 * ---------------------------------------------------------------------- */

static void test_mock_double_click_too_far(void) {
	boxen_mock_reset(80, 24);
	g_fake_now_ms = 0;
	boxen_mock_set_now_ms_fn(fake_clock);

	g_fake_now_ms = 0;
	boxen_mock_push_mouse(10, 5, 1, true, BOXEN_MOD_NONE);

	g_fake_now_ms = 100;
	boxen_mock_push_mouse(10 + 5, 5, 1, true, BOXEN_MOD_NONE);  /* 5 cells away */

	const boxen_backend_t *b = boxen_mock_backend();
	boxen_event_t ev;

	b->poll_event(&ev, 0);  /* consume first */

	int r = b->poll_event(&ev, 0);
	assert(r == BOXEN_OK);
	assert(ev.type == BOXEN_EV_MOUSE);
	assert((ev.mouse.flags & BOXEN_MOUSE_DOUBLE_CLICK) == 0);

	boxen_mock_set_now_ms_fn(NULL);
}

/* -------------------------------------------------------------------------
 * Test 7: double-click NOT synthesized when different button
 * ---------------------------------------------------------------------- */

static void test_mock_double_click_different_button(void) {
	boxen_mock_reset(80, 24);
	g_fake_now_ms = 0;
	boxen_mock_set_now_ms_fn(fake_clock);

	g_fake_now_ms = 0;
	boxen_mock_push_mouse(10, 5, 1, true, BOXEN_MOD_NONE);  /* LEFT */

	g_fake_now_ms = 100;
	boxen_mock_push_mouse(10, 5, 3, true, BOXEN_MOD_NONE);  /* RIGHT */

	const boxen_backend_t *b = boxen_mock_backend();
	boxen_event_t ev;

	b->poll_event(&ev, 0);  /* consume first */

	int r = b->poll_event(&ev, 0);
	assert(r == BOXEN_OK);
	assert(ev.type == BOXEN_EV_MOUSE);
	assert((ev.mouse.flags & BOXEN_MOUSE_DOUBLE_CLICK) == 0);

	boxen_mock_set_now_ms_fn(NULL);
}

/* -------------------------------------------------------------------------
 * Test 8: boxen_wcwidth for ASCII
 * ---------------------------------------------------------------------- */

static void test_wcwidth_ascii_letter(void) {
	assert(boxen_wcwidth('A') == 1);
	assert(boxen_wcwidth('z') == 1);
	assert(boxen_wcwidth(' ') == 1);
	assert(boxen_wcwidth('0') == 1);
	assert(boxen_wcwidth('~') == 1);
}

/* -------------------------------------------------------------------------
 * Test 9: boxen_wcwidth for control characters returns 0
 *
 * Kuhn mk_wcwidth returns -1 for non-NUL controls.
 * boxen_wcwidth MUST coerce -1 -> 0.
 * ---------------------------------------------------------------------- */

static void test_wcwidth_control_returns_zero(void) {
	assert(boxen_wcwidth(0x00) == 0);    /* NUL */
	assert(boxen_wcwidth(0x07) == 0);    /* BEL (Kuhn returns -1 -> coerced to 0) */
	assert(boxen_wcwidth(0x1B) == 0);    /* ESC (Kuhn returns -1 -> coerced to 0) */
	assert(boxen_wcwidth(0x01) == 0);    /* SOH */
	assert(boxen_wcwidth(0x1F) == 0);    /* US */
	assert(boxen_wcwidth(0x7F) == 0);    /* DEL (Kuhn returns -1) */
}

/* -------------------------------------------------------------------------
 * Test 10: boxen_wcwidth for CJK returns 2
 * ---------------------------------------------------------------------- */

static void test_wcwidth_cjk_returns_two(void) {
	/* U+4E2D CJK UNIFIED IDEOGRAPH-4E2D (Chinese: middle) */
	assert(boxen_wcwidth(0x4E2D) == 2);

	/* U+3042 HIRAGANA LETTER A */
	assert(boxen_wcwidth(0x3042) == 2);

	/* U+AC00 HANGUL SYLLABLE GA */
	assert(boxen_wcwidth(0xAC00) == 2);
}

/* -------------------------------------------------------------------------
 * main
 * ---------------------------------------------------------------------- */

int main(void) {
	TR_INIT("boxen_backend_tests");

	TR_RUN(test_mock_init_and_dimensions);
	TR_RUN(test_mock_set_cell_writes_grid);
	TR_RUN(test_mock_event_queue_fifo);
	TR_RUN(test_mock_double_click_synthesized);
	TR_RUN(test_mock_double_click_too_slow);
	TR_RUN(test_mock_double_click_too_far);
	TR_RUN(test_mock_double_click_different_button);
	TR_RUN(test_wcwidth_ascii_letter);
	TR_RUN(test_wcwidth_control_returns_zero);
	TR_RUN(test_wcwidth_cjk_returns_two);

	TR_SUMMARY();
	return TR_EXIT_CODE();
}
