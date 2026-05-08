/*
 * scrollback_pane_tests.c - Unit tests for the REPL scrollback pane.
 *
 * Verifies the public API surface of scrollback_pane.{c,h} and the
 * thin glue in repl_output.c that routes async output through the
 * compositor while the palette modal is active (issue #593).
 *
 * Tests are behavioural — they read the framebuffer via the
 * compositor_test_fb_at() inspection hook and assert on which cells
 * contain which codepoints. No source-string scraping.
 *
 * Standalone test executable: links pane.c, palette.c, scrollback_pane.c,
 * and the small palette-active routing helpers from repl_output_async.c
 * (a new translation unit extracted to keep tests independent of
 * linenoise / stdio plumbing in the main repl_output.c).
 *
 * License
 * -------
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Frontier contributors.
 */

#include <assert.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pane.h"
#include "scrollback_pane.h"
#include "repl_output_async.h"
#include "test_report.h"

/* Test-only inspection hooks exposed by pane.c. */
extern const cell_t *compositor_test_fb_at(int x, int y);
extern void compositor_test_fb_size(int *rows, int *cols);
extern void compositor_test_reset(void);

/* ---------- Helpers ---------- */

/* Returns true iff every char of `s` appears consecutively starting at
 * some column on row `y` in the framebuffer. */
static bool fb_row_has_substring(int y, const char *s) {
	int rows = 0, cols = 0;
	compositor_test_fb_size(&rows, &cols);
	if (y < 0 || y >= rows) return false;
	int slen = (int)strlen(s);
	for (int x = 0; x + slen <= cols; ++x) {
		bool match = true;
		for (int k = 0; k < slen; ++k) {
			const cell_t *c = compositor_test_fb_at(x + k, y);
			if (!c || c->ch != (uint32_t)(unsigned char)s[k]) {
				match = false;
				break;
			}
		}
		if (match) return true;
	}
	return false;
}

/* Returns true iff `s` appears consecutively somewhere in the
 * framebuffer (any row, any starting column). */
static bool fb_any_has_substring(const char *s) {
	int rows = 0, cols = 0;
	compositor_test_fb_size(&rows, &cols);
	for (int y = 0; y < rows; ++y) {
		if (fb_row_has_substring(y, s)) return true;
	}
	return false;
}

/* Capture stdout to a tmpfile so we can verify async output ROUTES to
 * the scrollback (not stdout) when palette is active. */
static int g_saved_stdout_fd = -1;
static char g_capture_path[256];

static void capture_stdout_begin(void) {
	snprintf(g_capture_path, sizeof(g_capture_path),
	         "/tmp/scrollback_pane_test_%d_%ld.bin",
	         (int)getpid(), (long)random());
	fflush(stdout);
	g_saved_stdout_fd = dup(STDOUT_FILENO);
	assert(g_saved_stdout_fd >= 0);
	FILE *f = freopen(g_capture_path, "w+", stdout);
	assert(f != NULL);
}

static char *capture_stdout_end(size_t *out_len) {
	fflush(stdout);
	FILE *r = fopen(g_capture_path, "rb");
	assert(r != NULL);
	fseek(r, 0, SEEK_END);
	long sz = ftell(r);
	fseek(r, 0, SEEK_SET);
	char *buf = malloc((size_t)sz + 1);
	assert(buf != NULL);
	size_t n = fread(buf, 1, (size_t)sz, r);
	buf[n] = '\0';
	fclose(r);
	if (g_saved_stdout_fd >= 0) {
		dup2(g_saved_stdout_fd, STDOUT_FILENO);
		close(g_saved_stdout_fd);
		g_saved_stdout_fd = -1;
		clearerr(stdout);
	}
	unlink(g_capture_path);
	if (out_len) *out_len = n;
	return buf;
}

/* ---------- Tests for scrollback_pane lifecycle ---------- */

static void test_scrollback_pane_init_destroy(void) {
	scrollback_pane_t sb;
	scrollback_pane_init(&sb, 0, 1, 80, 23, 64);
	assert(sb.pane.w == 80);
	assert(sb.pane.h == 23);
	assert(sb.pane.x == 0);
	assert(sb.pane.y == 1);
	assert(sb.line_capacity == 64);
	scrollback_pane_destroy(&sb);
}

static void test_scrollback_append_renders_on_pane(void) {
	compositor_test_reset();
	compositor_on_resize(24, 80);

	scrollback_pane_t sb;
	scrollback_pane_init(&sb, 0, 1, 80, 23, 64);
	compositor_register(&sb.pane);

	scrollback_pane_append(&sb, "hello world\n", 12);
	scrollback_pane_render_to_pane(&sb);
	compositor_render();

	/* The line "hello world" must appear somewhere in the framebuffer
	 * under row 0 (which the menubar would occupy in a real session). */
	bool found = false;
	for (int y = 1; y < 24; ++y) {
		if (fb_row_has_substring(y, "hello world")) { found = true; break; }
	}
	assert(found);

	compositor_unregister(&sb.pane);
	scrollback_pane_destroy(&sb);
}

static void test_scrollback_multiple_lines_appear_bottom_up(void) {
	compositor_test_reset();
	compositor_on_resize(24, 80);

	scrollback_pane_t sb;
	scrollback_pane_init(&sb, 0, 1, 80, 23, 64);
	compositor_register(&sb.pane);

	scrollback_pane_append(&sb, "older\n", 6);
	scrollback_pane_append(&sb, "newer\n", 6);
	scrollback_pane_render_to_pane(&sb);
	compositor_render();

	/* Both lines should be visible. The newer line should be on a
	 * later (lower) row than the older line — scrollback grows
	 * downward, oldest at top. */
	int rows = 0, cols = 0;
	compositor_test_fb_size(&rows, &cols);
	int row_older = -1, row_newer = -1;
	for (int y = 0; y < rows; ++y) {
		if (row_older < 0 && fb_row_has_substring(y, "older")) row_older = y;
		if (row_newer < 0 && fb_row_has_substring(y, "newer")) row_newer = y;
	}
	assert(row_older >= 0);
	assert(row_newer >= 0);
	assert(row_newer > row_older);

	compositor_unregister(&sb.pane);
	scrollback_pane_destroy(&sb);
}

static void test_scrollback_ring_buffer_drops_oldest(void) {
	compositor_test_reset();
	compositor_on_resize(24, 80);

	/* Tiny capacity so we can verify wrap-around quickly. */
	scrollback_pane_t sb;
	scrollback_pane_init(&sb, 0, 1, 80, 23, 4);
	compositor_register(&sb.pane);

	scrollback_pane_append(&sb, "L1\n", 3);
	scrollback_pane_append(&sb, "L2\n", 3);
	scrollback_pane_append(&sb, "L3\n", 3);
	scrollback_pane_append(&sb, "L4\n", 3);
	scrollback_pane_append(&sb, "L5\n", 3); /* should evict L1 */
	scrollback_pane_append(&sb, "L6\n", 3); /* should evict L2 */

	scrollback_pane_render_to_pane(&sb);
	compositor_render();

	/* L1 and L2 must NOT appear; L3..L6 must appear. */
	assert(!fb_any_has_substring("L1"));
	assert(!fb_any_has_substring("L2"));
	assert(fb_any_has_substring("L3"));
	assert(fb_any_has_substring("L4"));
	assert(fb_any_has_substring("L5"));
	assert(fb_any_has_substring("L6"));

	compositor_unregister(&sb.pane);
	scrollback_pane_destroy(&sb);
}

static void test_scrollback_z_order_under_palette_pane(void) {
	/* Snapshot test: register scrollback pane (z=0, head=bottom), then
	 * register a small "palette" pane (top, tail) that overlaps. The
	 * palette pane's filled region must NOT be overwritten by
	 * scrollback content; scrollback content remains visible only in
	 * cells the palette pane does not cover. */
	compositor_test_reset();
	compositor_on_resize(24, 80);

	scrollback_pane_t sb;
	scrollback_pane_init(&sb, 0, 1, 80, 23, 64);
	compositor_register(&sb.pane);

	/* Fill a row with a long string so we can test occlusion. */
	scrollback_pane_append(&sb, "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\n", 76);
	scrollback_pane_render_to_pane(&sb);

	/* Now layer a palette pane filled with 'X' at (10,5)-(40,9). */
	pane_t pal;
	pane_init(&pal, 10, 5, 30, 5);
	for (int y = 0; y < 5; ++y) {
		for (int x = 0; x < 30; ++x) {
			pane_putc(&pal, x, y, 'X', 0);
		}
	}
	compositor_register(&pal);

	compositor_render();

	/* Inside the palette region we see X. */
	const cell_t *c = compositor_test_fb_at(15, 7);
	assert(c != NULL);
	assert(c->ch == (uint32_t)'X');

	/* Outside the palette region but in the scrollback row we still
	 * see A (or 0 if no row had been written there). Pick a column
	 * to the right of the palette region but on a row that has
	 * scrollback content. */
	bool any_a = false;
	for (int y = 1; y < 24; ++y) {
		const cell_t *cc = compositor_test_fb_at(50, y);
		if (cc && cc->ch == (uint32_t)'A') { any_a = true; break; }
	}
	assert(any_a);

	compositor_unregister(&pal);
	pane_destroy(&pal);
	compositor_unregister(&sb.pane);
	scrollback_pane_destroy(&sb);
}

static void test_scrollback_long_line_wraps_or_truncates(void) {
	compositor_test_reset();
	compositor_on_resize(24, 80);

	scrollback_pane_t sb;
	scrollback_pane_init(&sb, 0, 1, 40, 23, 64); /* 40-col pane */
	compositor_register(&sb.pane);

	/* 50-char line — exceeds pane width. Implementation may truncate
	 * or wrap; we only require that no crash occurs and that a
	 * recognizable prefix appears. */
	const char *s = "0123456789012345678901234567890123456789xxxxxxxxxx\n";
	scrollback_pane_append(&sb, s, strlen(s));
	scrollback_pane_render_to_pane(&sb);
	compositor_render();

	/* The first 10 chars of the prefix should appear somewhere. */
	assert(fb_any_has_substring("0123456789"));

	compositor_unregister(&sb.pane);
	scrollback_pane_destroy(&sb);
}

/* ---------- Tests for repl_output_async routing ---------- */

static void test_async_output_default_writes_stdout(void) {
	/* When palette is NOT active, repl_async_output_emit() writes
	 * directly to stdout. */
	repl_async_output_set_palette_active(false, NULL);

	capture_stdout_begin();
	repl_async_output_emit("plain stdout line", 17);
	size_t n = 0;
	char *out = capture_stdout_end(&n);

	/* The captured bytes contain our message. */
	bool found = false;
	for (size_t i = 0; i + 16 < n; i++) {
		if (memcmp(out + i, "plain stdout line", 17) == 0) {
			found = true; break;
		}
	}
	assert(found);
	free(out);
}

static void test_async_output_routes_to_scrollback_when_palette_active(void) {
	compositor_test_reset();
	compositor_on_resize(24, 80);

	scrollback_pane_t sb;
	scrollback_pane_init(&sb, 0, 1, 80, 23, 64);
	compositor_register(&sb.pane);
	repl_async_output_set_palette_active(true, &sb);

	capture_stdout_begin();
	repl_async_output_emit("routed-line\n", 12);
	size_t n = 0;
	char *out = capture_stdout_end(&n);

	/* Stdout should NOT contain the routed line. */
	bool found_in_stdout = false;
	for (size_t i = 0; i + 10 < n; i++) {
		if (memcmp(out + i, "routed-line", 11) == 0) {
			found_in_stdout = true; break;
		}
	}
	assert(!found_in_stdout);
	free(out);

	/* Scrollback pane buffer should contain the routed line. */
	scrollback_pane_render_to_pane(&sb);
	compositor_render();
	assert(fb_any_has_substring("routed-line"));

	repl_async_output_set_palette_active(false, NULL);
	compositor_unregister(&sb.pane);
	scrollback_pane_destroy(&sb);
}

/* 4 threads × 4 lines = 16 lines total — fits in the 23-row pane so
 * every thread's first message is still in the bottom-justified
 * projection. Increasing this past ~22 starts dropping the oldest
 * messages off the top of the projection (not a bug — it's the design
 * of the pane render). */
#define NUM_THREADS 4
#define LINES_PER_THREAD 4

typedef struct worker_arg {
	int tid;
} worker_arg_t;

static void *real_worker(void *arg) {
	worker_arg_t *w = (worker_arg_t *)arg;
	for (int i = 0; i < LINES_PER_THREAD; ++i) {
		char buf[64];
		int n = snprintf(buf, sizeof(buf), "T%d-m%02d\n", w->tid, i);
		repl_async_output_emit(buf, (size_t)n);
	}
	return NULL;
}

static void test_async_output_thread_safety_real(void) {
	compositor_test_reset();
	compositor_on_resize(24, 80);

	scrollback_pane_t sb;
	scrollback_pane_init(&sb, 0, 1, 80, 23, 256);
	compositor_register(&sb.pane);
	repl_async_output_set_palette_active(true, &sb);

	capture_stdout_begin();

	pthread_t threads[NUM_THREADS];
	worker_arg_t args[NUM_THREADS];
	for (int i = 0; i < NUM_THREADS; ++i) {
		args[i].tid = i;
		int rc = pthread_create(&threads[i], NULL, real_worker, &args[i]);
		assert(rc == 0);
	}
	for (int i = 0; i < NUM_THREADS; ++i) {
		pthread_join(threads[i], NULL);
	}

	(void)capture_stdout_end(NULL);

	scrollback_pane_render_to_pane(&sb);
	compositor_render();

	/* Sample a handful of message labels — at least one from each
	 * thread should be visible. */
	for (int t = 0; t < NUM_THREADS; ++t) {
		char needle[16];
		snprintf(needle, sizeof(needle), "T%d-m00", t);
		assert(fb_any_has_substring(needle));
	}

	repl_async_output_set_palette_active(false, NULL);
	compositor_unregister(&sb.pane);
	scrollback_pane_destroy(&sb);
}

static void test_async_output_flush_drains_to_stdout(void) {
	/* On palette close, scrollback content should be drainable to
	 * stdout so the user sees what happened during the modal. */
	compositor_test_reset();
	compositor_on_resize(24, 80);

	scrollback_pane_t sb;
	scrollback_pane_init(&sb, 0, 1, 80, 23, 64);
	compositor_register(&sb.pane);
	repl_async_output_set_palette_active(true, &sb);

	repl_async_output_emit("flushed-msg\n", 12);

	/* Switch off palette mode and flush. */
	repl_async_output_set_palette_active(false, NULL);

	capture_stdout_begin();
	scrollback_pane_flush_to_stdout(&sb);
	size_t n = 0;
	char *out = capture_stdout_end(&n);

	bool found = false;
	for (size_t i = 0; i + 10 < n; i++) {
		if (memcmp(out + i, "flushed-msg", 11) == 0) {
			found = true; break;
		}
	}
	assert(found);
	free(out);

	compositor_unregister(&sb.pane);
	scrollback_pane_destroy(&sb);
}

int main(void) {
	TR_INIT("scrollback_pane_tests");
	TR_RUN(test_scrollback_pane_init_destroy);
	TR_RUN(test_scrollback_append_renders_on_pane);
	TR_RUN(test_scrollback_multiple_lines_appear_bottom_up);
	TR_RUN(test_scrollback_ring_buffer_drops_oldest);
	TR_RUN(test_scrollback_z_order_under_palette_pane);
	TR_RUN(test_scrollback_long_line_wraps_or_truncates);
	TR_RUN(test_async_output_default_writes_stdout);
	TR_RUN(test_async_output_routes_to_scrollback_when_palette_active);
	TR_RUN(test_async_output_thread_safety_real);
	TR_RUN(test_async_output_flush_drains_to_stdout);
	TR_SUMMARY();
	return TR_EXIT_CODE();
}
