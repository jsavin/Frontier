/*
 * backend_mock.c -- in-memory mock backend for boxen unit tests.
 *
 * Implements the boxen_backend_t vtable using a flat cell grid and a
 * simple FIFO event queue. Double-click synthesis uses an injectable
 * clock function so tests can control timing deterministically.
 *
 * This file is compiled into frontier-cli (alongside backend_tb2.c) at
 * Phase A.1 to ensure regressions are caught during normal builds.
 * Unused statics get __attribute__((unused)) where needed.
 */

#include "backend_mock.h"
#include "boxen_internal.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* -------------------------------------------------------------------------
 * Grid storage
 * ---------------------------------------------------------------------- */

#define MOCK_MAX_WIDTH  512
#define MOCK_MAX_HEIGHT 256
#define MOCK_MAX_EVENTS 256

static boxen_mock_cell_t g_grid[MOCK_MAX_HEIGHT][MOCK_MAX_WIDTH];
static int g_width  = 80;
static int g_height = 24;

/* Cursor state for A.7 test accessors. boxen_mock_reset() restores these
 * to the "never called" defaults (-1, -1, false). */
static int  g_cursor_x       = -1;
static int  g_cursor_y       = -1;
static bool g_cursor_visible = false;

/* 2026-08-09 JES C M7: mouse-mode recorder for the set_mouse vtable slot.
 * The call counter lets tests assert that startup performs ZERO backend
 * mouse calls (plan section 5.1), not merely that the final state is off. */
static bool g_mock_mouse_enabled   = false;
static int  g_mock_set_mouse_calls = 0;

/* -------------------------------------------------------------------------
 * Event queue (FIFO ring buffer)
 * ---------------------------------------------------------------------- */

static boxen_event_t g_queue[MOCK_MAX_EVENTS];
static int g_queue_head = 0;  /* index to dequeue from */
static int g_queue_tail = 0;  /* index to enqueue at */
static int g_queue_size = 0;

static void queue_push(const boxen_event_t *ev) {
	if (g_queue_size >= MOCK_MAX_EVENTS) {
		/* Drop oldest event to make room */
		g_queue_head = (g_queue_head + 1) % MOCK_MAX_EVENTS;
		g_queue_size--;
	}
	g_queue[g_queue_tail] = *ev;
	g_queue_tail = (g_queue_tail + 1) % MOCK_MAX_EVENTS;
	g_queue_size++;
}

static int queue_pop(boxen_event_t *out) {
	if (g_queue_size == 0) {
		return BOXEN_ERR_TIMEOUT;
	}
	*out = g_queue[g_queue_head];
	g_queue_head = (g_queue_head + 1) % MOCK_MAX_EVENTS;
	g_queue_size--;
	return BOXEN_OK;
}

/* -------------------------------------------------------------------------
 * Clock injection for double-click synthesis
 * ---------------------------------------------------------------------- */

/* Default clock: monotonically-incrementing counter (starts at 0). */
static uint64_t g_default_clock_ms = 0;

static uint64_t default_now_ms(void) {
	return g_default_clock_ms++;
}

static uint64_t (*g_now_ms_fn)(void) = default_now_ms;

void boxen_mock_set_now_ms_fn(uint64_t (*fn)(void)) {
	if (fn) {
		g_now_ms_fn = fn;
	} else {
		g_now_ms_fn = default_now_ms;
	}
}

/* -------------------------------------------------------------------------
 * Double-click state
 * ---------------------------------------------------------------------- */

static struct {
	uint64_t time_ms;
	int      x;
	int      y;
	uint8_t  button;
	bool     valid;
} g_last_press;

/*
 * Called for each press event. If it matches the previous press in time
 * (within BOXEN_DOUBLE_CLICK_MS) and position (within BOXEN_DOUBLE_CLICK_RADIUS
 * cells, Chebyshev distance) and the same button, sets BOXEN_MOUSE_DOUBLE_CLICK
 * on *ev and clears the last-press state. Otherwise records the current
 * press as the new last-press.
 */
static void maybe_set_double_click(boxen_event_t *ev) {
	if (!ev->mouse.pressed) {
		return;
	}

	uint64_t now = g_now_ms_fn();

	if (g_last_press.valid) {
		uint64_t elapsed = now - g_last_press.time_ms;
		int dx = ev->mouse.x - g_last_press.x;
		int dy = ev->mouse.y - g_last_press.y;
		if (dx < 0) dx = -dx;
		if (dy < 0) dy = -dy;
		int dist = dx > dy ? dx : dy;  /* Chebyshev distance */

		if (elapsed <= BOXEN_DOUBLE_CLICK_MS
		    && dist  <= BOXEN_DOUBLE_CLICK_RADIUS
		    && ev->mouse.button == g_last_press.button) {
			ev->mouse.flags |= BOXEN_MOUSE_DOUBLE_CLICK;
			g_last_press.valid = false;
			return;
		}
	}

	/* Record this press as the new candidate. */
	g_last_press.time_ms = now;
	g_last_press.x       = ev->mouse.x;
	g_last_press.y       = ev->mouse.y;
	g_last_press.button  = ev->mouse.button;
	g_last_press.valid   = true;
}

/* -------------------------------------------------------------------------
 * Public reset and control API
 * ---------------------------------------------------------------------- */

void boxen_mock_reset(int width, int height) {
	if (width  > MOCK_MAX_WIDTH)  width  = MOCK_MAX_WIDTH;
	if (height > MOCK_MAX_HEIGHT) height = MOCK_MAX_HEIGHT;
	if (width  < 1) width  = 1;
	if (height < 1) height = 1;

	g_width  = width;
	g_height = height;

	memset(g_grid, 0, sizeof(g_grid));

	g_queue_head = 0;
	g_queue_tail = 0;
	g_queue_size = 0;

	g_last_press.valid = false;
	g_default_clock_ms = 0;

	g_cursor_x       = -1;
	g_cursor_y       = -1;
	g_cursor_visible = false;

	g_mock_mouse_enabled   = false;
	g_mock_set_mouse_calls = 0;
}

bool boxen_mock_mouse_enabled(void) {
	return g_mock_mouse_enabled;
}

int boxen_mock_set_mouse_calls(void) {
	return g_mock_set_mouse_calls;
}

void boxen_mock_get_cursor(int *x, int *y) {
	if (x) *x = g_cursor_x;
	if (y) *y = g_cursor_y;
}

bool boxen_mock_cursor_visible(void) {
	return g_cursor_visible;
}

void boxen_mock_width_set(int w) {
	if (w >= 1 && w <= MOCK_MAX_WIDTH) g_width = w;
}

void boxen_mock_height_set(int h) {
	if (h >= 1 && h <= MOCK_MAX_HEIGHT) g_height = h;
}

const boxen_mock_cell_t *boxen_mock_cell_at(int x, int y) {
	if (x < 0 || x >= g_width || y < 0 || y >= g_height) {
		return NULL;
	}
	return &g_grid[y][x];
}

bool boxen_mock_has_text(const char *s) {
	if (!s || !*s) return false;
	int slen = 0;
	const char *p = s;
	while (*p++) slen++;

	for (int y = 0; y < g_height; y++) {
		for (int x = 0; x + slen <= g_width; x++) {
			bool match = true;
			for (int k = 0; k < slen; k++) {
				if (g_grid[y][x + k].ch != (uint32_t)(unsigned char)s[k]) {
					match = false;
					break;
				}
			}
			if (match) return true;
		}
	}
	return false;
}

/* 2026-06-10 JES #691 C.1.x: codepoint-based grid scan.
 * Use for non-ASCII glyphs that boxen_mock_has_text can't match. */
bool boxen_mock_has_codepoint(uint32_t cp) {
	for (int y = 0; y < g_height; y++) {
		for (int x = 0; x < g_width; x++) {
			if (g_grid[y][x].ch == cp) return true;
		}
	}
	return false;
}

/* -------------------------------------------------------------------------
 * Event injection
 * ---------------------------------------------------------------------- */

void boxen_mock_push_event(const boxen_event_t *ev) {
	queue_push(ev);
}

void boxen_mock_push_key(boxen_key_t key, uint32_t ch, uint16_t mod) {
	boxen_event_t ev;
	memset(&ev, 0, sizeof(ev));
	ev.type    = BOXEN_EV_KEY;
	ev.key.key = key;
	ev.key.ch  = ch;
	ev.key.mod = mod;
	queue_push(&ev);
}

void boxen_mock_push_mouse(int x, int y, uint8_t button, bool pressed, uint16_t mod) {
	boxen_event_t ev;
	memset(&ev, 0, sizeof(ev));
	ev.type          = BOXEN_EV_MOUSE;
	ev.mouse.x       = x;
	ev.mouse.y       = y;
	ev.mouse.button  = button;
	ev.mouse.pressed = pressed;
	ev.mouse.mod     = mod;
	ev.mouse.flags   = 0;

	if (pressed) {
		maybe_set_double_click(&ev);
	}

	queue_push(&ev);
}

void boxen_mock_push_double_click(int x, int y, uint16_t mod) {
	/* Push first press (records last-press state) */
	boxen_mock_push_mouse(x, y, 1, true, mod);
	/* Push second press (within double-click timing) - timing handled by
	 * the caller having set up g_now_ms_fn to return a value within the window.
	 * For the convenience version here we call push_mouse again; the clock
	 * state should already be configured by the test. */
	boxen_mock_push_mouse(x, y, 1, true, mod);
}

/* -------------------------------------------------------------------------
 * Backend vtable implementation
 * ---------------------------------------------------------------------- */

static int mock_init(void *config) {
	(void)config;
	return BOXEN_OK;
}

static void mock_shutdown(void) {
	/* Clear double-click state so a re-init starts fresh. Symmetric with
	 * the tb2 backend. boxen_mock_reset() is the broader test-reset entry
	 * point; this handles the narrower init/shutdown lifecycle case. */
	memset(&g_last_press, 0, sizeof(g_last_press));
}

static int mock_width(void) {
	return g_width;
}

static int mock_height(void) {
	return g_height;
}

static int mock_color_depth(void) {
	return 256;  /* test default: 256-color */
}

static void mock_set_cell(int x, int y, uint32_t ch,
                          uint16_t fg, uint16_t bg, uint16_t attr) {
	if (x < 0 || x >= g_width || y < 0 || y >= g_height) {
		return;
	}
	g_grid[y][x].ch   = ch;
	g_grid[y][x].fg   = fg;
	g_grid[y][x].bg   = bg;
	g_grid[y][x].attr = attr;
}

static void mock_set_cursor(int x, int y) {
	g_cursor_x = x;
	g_cursor_y = y;
}

static void mock_set_cursor_visible(bool visible) {
	g_cursor_visible = visible;
}

static void mock_present(void) {
	/* In the mock, present is a no-op: set_cell writes directly to the
	 * inspection grid, so there is no diff/flush step. */
}

static int mock_poll_event(boxen_event_t *out, int timeout_ms) {
	(void)timeout_ms;
	return queue_pop(out);
}

static void mock_clear(void) {
	/* Clear only the active region (g_width x g_height), not the full static
	 * MOCK_MAX_HEIGHT x MOCK_MAX_WIDTH grid -- consistent with set_cell which
	 * also bounds at active dimensions. */
	for (int y = 0; y < g_height; y++) {
		memset(g_grid[y], 0, (size_t)g_width * sizeof(g_grid[y][0]));
	}
}

/* 2026-08-09 JES C M7: record mouse-mode requests for test inspection. */
static void mock_set_mouse(bool enable) {
	g_mock_mouse_enabled = enable;
	g_mock_set_mouse_calls++;
}

/* -------------------------------------------------------------------------
 * Vtable and accessor
 * ---------------------------------------------------------------------- */

static const boxen_backend_t g_mock_backend = {
	.init               = mock_init,
	.shutdown           = mock_shutdown,
	.width              = mock_width,
	.height             = mock_height,
	.color_depth        = mock_color_depth,
	.set_cell           = mock_set_cell,
	.set_cursor         = mock_set_cursor,
	.set_cursor_visible = mock_set_cursor_visible,
	.present            = mock_present,
	.poll_event         = mock_poll_event,
	.clear              = mock_clear,
	.set_mouse          = mock_set_mouse,
};

const boxen_backend_t *boxen_mock_backend(void) {
	return &g_mock_backend;
}
