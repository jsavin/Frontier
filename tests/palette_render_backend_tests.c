/*
 * palette_render_backend_tests.c - Behavioural tests for the
 * palette render-backend vtable abstraction (C.0.3a).
 *
 * Three tests validate the dispatch mechanism introduced by
 * palette_render_backend_t:
 *
 *   1. test_default_backend_is_pane
 *      Open via the legacy palette_open signature; assert that
 *      st.backend points at palette_render_pane_backend().
 *
 *   2. test_paint_dispatch_calls_backend_fn
 *      Install a mock backend with a paint counter; call
 *      palette_render_state(&st); assert counter > 0.
 *
 *   3. test_open_close_resize_lifecycle_calls_backend_fns
 *      Mock backend records call order; assert sequence
 *      open -> paint -> on_resize -> paint -> paint_teardown -> close.
 *
 * 2026-06-10 JES #691 Phase C.0.3a: render backend abstraction.
 *
 * License
 * -------
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Frontier contributors.
 */

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "pane.h"
#include "palette.h"
#include "test_report.h"

extern void compositor_test_reset(void);

/* ---------- Mock backend plumbing ---------- */

typedef struct {
	int open_count;
	int paint_count;
	int teardown_count;
	int resize_count;
	int close_count;
	/* Small recorded sequence for lifecycle test. Each char is the
	 * initial letter of the function that ran: O P T R C (open, paint,
	 * teardown, resize, close). */
	char call_log[256];
	int call_log_len;
} mock_backend_state_t;

static void mock_log(mock_backend_state_t *m, char c) {
	if (m->call_log_len < (int)(sizeof(m->call_log) - 1)) {
		m->call_log[m->call_log_len++] = c;
		m->call_log[m->call_log_len]   = '\0';
	}
}

static bool mock_open(palette_state_t *st, void *ctx) {
	(void)st;
	mock_backend_state_t *m = (mock_backend_state_t *)ctx;
	m->open_count++;
	mock_log(m, 'O');
	return true;
}

static void mock_paint(palette_state_t *st, void *ctx) {
	(void)st;
	mock_backend_state_t *m = (mock_backend_state_t *)ctx;
	m->paint_count++;
	mock_log(m, 'P');
}

static void mock_paint_teardown(palette_state_t *st, void *ctx) {
	(void)st;
	mock_backend_state_t *m = (mock_backend_state_t *)ctx;
	m->teardown_count++;
	mock_log(m, 'T');
}

static void mock_on_resize(palette_state_t *st, int rows, int cols, void *ctx) {
	(void)st; (void)rows; (void)cols;
	mock_backend_state_t *m = (mock_backend_state_t *)ctx;
	m->resize_count++;
	mock_log(m, 'R');
}

static void mock_close(palette_state_t *st, void *ctx) {
	(void)st;
	mock_backend_state_t *m = (mock_backend_state_t *)ctx;
	m->close_count++;
	mock_log(m, 'C');
}

/* ---------- Synthetic source (minimal, 2-menu fixture) ---------- */

static int src_count(void *ctx) { (void)ctx; return 2; }
static bool src_describe(void *ctx, int idx, char *out, size_t cap, char *hk) {
	(void)ctx;
	if (idx == 0) { snprintf(out, cap, "REPL"); *hk = 'R'; return true; }
	if (idx == 1) { snprintf(out, cap, "File"); *hk = 'F'; return true; }
	return false;
}
static int src_item_count(void *ctx, int mi, void *po) {
	(void)ctx; (void)mi; (void)po; return 0;
}
static bool src_item_describe(void *ctx, int mi, void *po, int ii,
                               palette_item_t *out) {
	(void)ctx; (void)mi; (void)po; (void)ii; (void)out; return false;
}

static palette_menu_source_t make_src(void) {
	palette_menu_source_t s;
	memset(&s, 0, sizeof(s));
	s.ctx = NULL;
	s.count_menus    = src_count;
	s.menu_describe  = src_describe;
	s.item_count     = src_item_count;
	s.item_describe  = src_item_describe;
	return s;
}

/* ---------- Tests ---------- */

/* 1. Legacy palette_open stores the pane backend pointer. */
static void test_default_backend_is_pane(void) {
	compositor_test_reset();
	compositor_on_resize(24, 80);
	palette_state_t st;
	palette_menu_source_t src = make_src();

	bool ok = palette_open(&st, 24, 80, 0, &src);
	assert(ok);

	/* st.backend must be the canonical pane backend, not NULL. */
	assert(st.backend != NULL);
	assert(st.backend == palette_render_pane_backend());

	palette_close(&st);
}

/* 2. palette_render_state dispatches to the backend's paint fn. */
static void test_paint_dispatch_calls_backend_fn(void) {
	compositor_test_reset();
	compositor_on_resize(24, 80);

	mock_backend_state_t m;
	memset(&m, 0, sizeof(m));

	palette_render_backend_t mock_be;
	memset(&mock_be, 0, sizeof(mock_be));
	mock_be.ctx            = &m;
	mock_be.open           = mock_open;
	mock_be.paint          = mock_paint;
	mock_be.paint_teardown = mock_paint_teardown;
	mock_be.on_resize      = mock_on_resize;
	mock_be.close          = mock_close;

	palette_state_t st;
	palette_menu_source_t src = make_src();

	bool ok = palette_open_ex(&st, 24, 80, 0, &src, &mock_be);
	assert(ok);
	assert(m.open_count == 1);

	int paint_before = m.paint_count;
	palette_render_state(&st);
	assert(m.paint_count > paint_before);

	palette_close(&st);
	assert(m.close_count == 1);
}

/* 3. Full lifecycle: open -> paint -> on_resize -> paint -> teardown -> close.
 * The call_log must record exactly "OPRPTC". */
static void test_open_close_resize_lifecycle_calls_backend_fns(void) {
	compositor_test_reset();
	compositor_on_resize(24, 80);

	mock_backend_state_t m;
	memset(&m, 0, sizeof(m));

	palette_render_backend_t mock_be;
	memset(&mock_be, 0, sizeof(mock_be));
	mock_be.ctx            = &m;
	mock_be.open           = mock_open;
	mock_be.paint          = mock_paint;
	mock_be.paint_teardown = mock_paint_teardown;
	mock_be.on_resize      = mock_on_resize;
	mock_be.close          = mock_close;

	palette_state_t st;
	palette_menu_source_t src = make_src();

	/* open: logs 'O' */
	bool ok = palette_open_ex(&st, 24, 80, 0, &src, &mock_be);
	assert(ok);

	/* paint: logs 'P' */
	palette_render_state(&st);

	/* on_resize: logs 'R' */
	palette_on_resize(&st, 30, 100);

	/* paint again: logs 'P' */
	palette_render_state(&st);

	/* paint_teardown: logs 'T' */
	palette_paint_teardown(&st);

	/* close: logs 'C' */
	palette_close(&st);

	/* Expected: O P R P T C */
	assert(strcmp(m.call_log, "OPRPTC") == 0);
}

/* ---------- Main ---------- */

int main(void) {
	TR_INIT("palette_render_backend_tests");
	TR_RUN(test_default_backend_is_pane);
	TR_RUN(test_paint_dispatch_calls_backend_fn);
	TR_RUN(test_open_close_resize_lifecycle_calls_backend_fns);
	TR_SUMMARY();
	return TR_EXIT_CODE();
}
