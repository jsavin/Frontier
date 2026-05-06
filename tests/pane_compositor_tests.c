/*
 * pane_compositor_tests.c - Unit tests for the REPL pane compositor.
 *
 * Verifies the public API surface of pane.{c,h} (see plan
 * humming-painting-hejlsberg.md §1.3a):
 *   - pane lifecycle (init/destroy/clear/move/resize)
 *   - pane content writes (putc/puts) with buffer-bound clipping
 *   - compositor registry (register/unregister, idempotency)
 *   - back-to-front composition into the framebuffer
 *   - z-order (raise) and front-to-back hit testing
 *   - framebuffer reallocation on resize
 *
 * Tests are behavioural — they exercise the framebuffer state via the
 * test-only inspection hooks compositor_test_fb_at() / _size(), not by
 * scraping the SGR byte stream (whose exact form is an implementation
 * detail of pane.c that PR 5 will swap out).
 */

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pane.h"
#include "test_report.h"

/* Test-only framebuffer inspection — exposed by pane.c for unit tests. */
extern const cell_t *compositor_test_fb_at(int x, int y);
extern void compositor_test_fb_size(int *rows, int *cols);
extern void compositor_test_reset(void);

static void test_pane_init_creates_buffer(void) {
	pane_t p;
	pane_init(&p, 5, 7, 20, 10);
	assert(p.x == 5);
	assert(p.y == 7);
	assert(p.w == 20);
	assert(p.h == 10);
	assert(p.buf != NULL);
	/* Newly initialised buffer is zeroed. */
	for (int i = 0; i < 20 * 10; ++i) {
		assert(p.buf[i].ch == 0);
		assert(p.buf[i].fg == 0);
		assert(p.buf[i].bg == 0);
		assert(p.buf[i].attr == 0);
	}
	pane_destroy(&p);
}

static void test_pane_putc_and_puts(void) {
	pane_t p;
	pane_init(&p, 0, 0, 10, 4);
	pane_putc(&p, 3, 1, 'X', 7);
	assert(p.buf[1 * 10 + 3].ch == (uint32_t)'X');
	assert(p.buf[1 * 10 + 3].attr == 7);

	pane_puts(&p, 0, 2, "hi", 3);
	assert(p.buf[2 * 10 + 0].ch == (uint32_t)'h');
	assert(p.buf[2 * 10 + 1].ch == (uint32_t)'i');
	assert(p.buf[2 * 10 + 0].attr == 3);

	/* Out-of-bounds writes are silently clipped, not crashes. */
	pane_putc(&p, -1, 0, 'A', 0);
	pane_putc(&p, 10, 0, 'A', 0);
	pane_putc(&p, 0, 4, 'A', 0);
	pane_puts(&p, 8, 3, "abcdef", 0);
	assert(p.buf[3 * 10 + 8].ch == (uint32_t)'a');
	assert(p.buf[3 * 10 + 9].ch == (uint32_t)'b');
	/* The 'c' onward fell off the right edge — no crash, no corruption. */

	pane_destroy(&p);
}

static void test_pane_clear_zeros_buffer(void) {
	pane_t p;
	pane_init(&p, 0, 0, 4, 3);
	pane_puts(&p, 0, 0, "ABCD", 5);
	pane_puts(&p, 0, 1, "EFGH", 5);
	pane_clear(&p);
	for (int i = 0; i < 4 * 3; ++i) {
		assert(p.buf[i].ch == 0);
		assert(p.buf[i].attr == 0);
	}
	pane_destroy(&p);
}

static void test_compositor_register_unregister(void) {
	compositor_test_reset();
	compositor_on_resize(10, 20);

	pane_t p1, p2;
	pane_init(&p1, 0, 0, 4, 4);
	pane_init(&p2, 5, 5, 4, 4);

	compositor_register(&p1);
	compositor_register(&p2);

	pane_t *hit = compositor_pane_at(1, 1);
	assert(hit == &p1);
	hit = compositor_pane_at(6, 6);
	assert(hit == &p2);

	compositor_unregister(&p1);
	hit = compositor_pane_at(1, 1);
	assert(hit == NULL);
	hit = compositor_pane_at(6, 6);
	assert(hit == &p2);

	/* Unregister of an already-removed pane is a no-op. */
	compositor_unregister(&p1);

	compositor_unregister(&p2);
	pane_destroy(&p1);
	pane_destroy(&p2);
}

static void test_compositor_render_single_pane(void) {
	compositor_test_reset();
	compositor_on_resize(5, 10);

	pane_t p;
	pane_init(&p, 2, 1, 3, 2);
	pane_puts(&p, 0, 0, "abc", 1);
	pane_puts(&p, 0, 1, "def", 1);
	compositor_register(&p);

	compositor_render();

	const cell_t *c = compositor_test_fb_at(2, 1);
	assert(c->ch == (uint32_t)'a');
	c = compositor_test_fb_at(3, 1);
	assert(c->ch == (uint32_t)'b');
	c = compositor_test_fb_at(4, 1);
	assert(c->ch == (uint32_t)'c');
	c = compositor_test_fb_at(2, 2);
	assert(c->ch == (uint32_t)'d');

	/* Cells outside the pane rect remain zeroed by composition. */
	c = compositor_test_fb_at(0, 0);
	assert(c->ch == 0);
	c = compositor_test_fb_at(5, 1);
	assert(c->ch == 0);

	compositor_unregister(&p);
	pane_destroy(&p);
}

static void test_compositor_render_overlapping(void) {
	compositor_test_reset();
	compositor_on_resize(5, 10);

	pane_t bottom, top;
	pane_init(&bottom, 0, 0, 5, 3);
	pane_init(&top, 2, 1, 3, 2);
	pane_puts(&bottom, 0, 0, "BBBBB", 1);
	pane_puts(&bottom, 0, 1, "BBBBB", 1);
	pane_puts(&bottom, 0, 2, "BBBBB", 1);
	pane_puts(&top, 0, 0, "TTT", 2);
	pane_puts(&top, 0, 1, "TTT", 2);

	compositor_register(&bottom);
	compositor_register(&top);
	compositor_render();

	/* In overlapping cells, top wins. */
	const cell_t *c = compositor_test_fb_at(2, 1);
	assert(c->ch == (uint32_t)'T');
	assert(c->attr == 2);

	/* Cells covered only by bottom remain bottom. */
	c = compositor_test_fb_at(0, 0);
	assert(c->ch == (uint32_t)'B');
	c = compositor_test_fb_at(0, 2);
	assert(c->ch == (uint32_t)'B');

	compositor_unregister(&top);
	compositor_unregister(&bottom);
	pane_destroy(&top);
	pane_destroy(&bottom);
}

static void test_pane_raise_reorders(void) {
	compositor_test_reset();
	compositor_on_resize(5, 10);

	pane_t a, b;
	pane_init(&a, 0, 0, 5, 3);
	pane_init(&b, 0, 0, 5, 3);
	pane_puts(&a, 0, 0, "AAAAA", 1);
	pane_puts(&b, 0, 0, "BBBBB", 2);

	compositor_register(&a);
	compositor_register(&b);
	/* b registered last → b is on top. */
	pane_t *hit = compositor_pane_at(0, 0);
	assert(hit == &b);

	pane_raise(&a);
	hit = compositor_pane_at(0, 0);
	assert(hit == &a);

	compositor_render();
	const cell_t *c = compositor_test_fb_at(0, 0);
	assert(c->ch == (uint32_t)'A');

	compositor_unregister(&a);
	compositor_unregister(&b);
	pane_destroy(&a);
	pane_destroy(&b);
}

static void test_compositor_pane_at_highest_z(void) {
	compositor_test_reset();
	compositor_on_resize(5, 10);

	pane_t a, b, c;
	pane_init(&a, 0, 0, 8, 3);
	pane_init(&b, 1, 0, 6, 3);
	pane_init(&c, 2, 0, 4, 3);
	compositor_register(&a);
	compositor_register(&b);
	compositor_register(&c);

	/* (0,0) only inside a. */
	assert(compositor_pane_at(0, 0) == &a);
	/* (1,0) inside a and b → b (registered later, on top). */
	assert(compositor_pane_at(1, 0) == &b);
	/* (3,0) inside a, b, c → c. */
	assert(compositor_pane_at(3, 0) == &c);
	/* Outside everything. */
	assert(compositor_pane_at(9, 9) == NULL);

	compositor_unregister(&a);
	compositor_unregister(&b);
	compositor_unregister(&c);
	pane_destroy(&a);
	pane_destroy(&b);
	pane_destroy(&c);
}

static void test_compositor_on_resize_reallocates(void) {
	compositor_test_reset();
	compositor_on_resize(10, 20);
	int rows = 0, cols = 0;
	compositor_test_fb_size(&rows, &cols);
	assert(rows == 10);
	assert(cols == 20);

	compositor_on_resize(30, 80);
	compositor_test_fb_size(&rows, &cols);
	assert(rows == 30);
	assert(cols == 80);

	/* After resize the framebuffer is freshly cleared. */
	const cell_t *cell = compositor_test_fb_at(0, 0);
	assert(cell->ch == 0);
	cell = compositor_test_fb_at(79, 29);
	assert(cell->ch == 0);
}

static void test_pane_move_and_resize(void) {
	compositor_test_reset();
	compositor_on_resize(10, 20);

	pane_t p;
	pane_init(&p, 0, 0, 4, 2);
	compositor_register(&p);
	pane_move(&p, 5, 5);
	assert(p.x == 5);
	assert(p.y == 5);

	pane_resize(&p, 6, 3);
	assert(p.w == 6);
	assert(p.h == 3);
	assert(p.buf != NULL);
	/* Resize zeros the new buffer. */
	for (int i = 0; i < 6 * 3; ++i) {
		assert(p.buf[i].ch == 0);
	}

	compositor_unregister(&p);
	pane_destroy(&p);
}

int main(void) {
	TR_INIT("pane_compositor_tests");
	TR_RUN(test_pane_init_creates_buffer);
	TR_RUN(test_pane_putc_and_puts);
	TR_RUN(test_pane_clear_zeros_buffer);
	TR_RUN(test_compositor_register_unregister);
	TR_RUN(test_compositor_render_single_pane);
	TR_RUN(test_compositor_render_overlapping);
	TR_RUN(test_pane_raise_reorders);
	TR_RUN(test_compositor_pane_at_highest_z);
	TR_RUN(test_compositor_on_resize_reallocates);
	TR_RUN(test_pane_move_and_resize);
	TR_SUMMARY();
	return TR_EXIT_CODE();
}
