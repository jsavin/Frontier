/*
 * split_demo.c -- boxen Phase A.6 interactive layout demo.
 *
 * Demonstrates:
 *   - boxen_layout_split_h: horizontal two-pane layout (left/right)
 *   - boxen_layout_split_v: vertical two-pane split of the right half
 *   - Chrome rendering: borders, title insets, scrollbars
 *   - Pinned keybind footer: borderless, BOXEN_PIN_BOTTOM
 *   - Tab to cycle focus; arrow keys to scroll the focused pane
 *   - 'q' or Escape to quit
 *
 * Layout (terminal full width x height):
 *
 *   +-- Left (50%) ------+-- Top-Right (50%, 60%) --+
 *   | scrollable list    |  file listing             |
 *   |                    |                           |
 *   |                    +-- Bot-Right (50%, 40%) ---+
 *   |                    |  log / status             |
 *   +--------------------+---------------------------+
 *   Tab:focus  Arrows:scroll  q:quit
 *
 * Build:
 *   make -C frontier-cli split_demo
 * Run:
 *   frontier-cli/split_demo
 */

#include "boxen.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* -------------------------------------------------------------------------
 * Demo content
 * ---------------------------------------------------------------------- */

static const char *left_lines[] = {
	"alpha.c",    "beta.c",     "gamma.c",    "delta.c",    "epsilon.c",
	"zeta.c",     "eta.c",      "theta.c",    "iota.c",     "kappa.c",
	"lambda.c",   "mu.c",       "nu.c",       "xi.c",       "omicron.c",
	"pi.c",       "rho.c",      "sigma.c",    "tau.c",      "upsilon.c",
	"phi.c",      "chi.c",      "psi.c",      "omega.c",    "alpha2.c",
	"beta2.c",    "gamma2.c",   "delta2.c",   "epsilon2.c", "zeta2.c",
};
#define LEFT_LINE_COUNT ((int)(sizeof(left_lines) / sizeof(left_lines[0])))

static const char *top_right_lines[] = {
	"README.md",       "LICENSE",         "Makefile",
	"boxen.c",         "boxen.h",         "boxen_internal.h",
	"boxen_log.c",     "boxen_log.h",     "boxen_wcwidth.c",
	"backend_tb2.c",   "backend_mock.c",  "backend_mock.h",
};
#define TOP_RIGHT_LINE_COUNT ((int)(sizeof(top_right_lines) / sizeof(top_right_lines[0])))

static const char *log_lines[] = {
	"[INFO]  boxen_init OK",
	"[INFO]  layout split_h ratio=0.50",
	"[INFO]  layout split_v ratio=0.60",
	"[INFO]  window 'Left'      opened",
	"[INFO]  window 'Top Right' opened",
	"[INFO]  window 'Bot Right' opened",
	"[INFO]  footer pinned BOTTOM, no borders",
	"[INFO]  phase A.6 demo running",
};
#define LOG_LINE_COUNT ((int)(sizeof(log_lines) / sizeof(log_lines[0])))

/* -------------------------------------------------------------------------
 * App state
 * ---------------------------------------------------------------------- */

typedef struct {
	boxen_window_t *left;
	boxen_window_t *top_right;
	boxen_window_t *bot_right;
	boxen_window_t *footer;
	int             focus_idx;   /* 0=left, 1=top_right, 2=bot_right */
} demo_state_t;

static demo_state_t g_state;

static boxen_window_t *focused_win(void) {
	switch (g_state.focus_idx) {
	case 1:  return g_state.top_right;
	case 2:  return g_state.bot_right;
	default: return g_state.left;
	}
}

/* -------------------------------------------------------------------------
 * Draw callbacks
 * ---------------------------------------------------------------------- */

static void draw_left(boxen_window_t *win, void *ud) {
	(void)ud;
	int w = boxen_window_content_width(win);
	if (w <= 0) return;
	for (int i = 0; i < LEFT_LINE_COUNT; i++) {
		char buf[128];
		snprintf(buf, sizeof(buf), "%-*.*s", w, w, left_lines[i]);
		uint16_t fg = BOXEN_COLOR_DEFAULT;
		boxen_draw_text(win, 0, i, buf, fg, BOXEN_COLOR_DEFAULT, BOXEN_ATTR_NONE);
	}
}

static void draw_top_right(boxen_window_t *win, void *ud) {
	(void)ud;
	int w = boxen_window_content_width(win);
	if (w <= 0) return;
	for (int i = 0; i < TOP_RIGHT_LINE_COUNT; i++) {
		char buf[128];
		snprintf(buf, sizeof(buf), "%-*.*s", w, w, top_right_lines[i]);
		boxen_draw_text(win, 0, i, buf,
		                BOXEN_COLOR_GREEN, BOXEN_COLOR_DEFAULT, BOXEN_ATTR_NONE);
	}
}

static void draw_bot_right(boxen_window_t *win, void *ud) {
	(void)ud;
	int w = boxen_window_content_width(win);
	if (w <= 0) return;
	for (int i = 0; i < LOG_LINE_COUNT; i++) {
		char buf[128];
		snprintf(buf, sizeof(buf), "%-*.*s", w, w, log_lines[i]);
		uint16_t fg = BOXEN_COLOR_YELLOW;
		boxen_draw_text(win, 0, i, buf, fg, BOXEN_COLOR_DEFAULT, BOXEN_ATTR_NONE);
	}
}

static void draw_footer(boxen_window_t *win, void *ud) {
	(void)ud;
	int w = boxen_window_content_width(win);
	if (w <= 0) return;
	const char *text = "Tab:focus  Arrows:scroll  q:quit";
	char buf[256];
	snprintf(buf, sizeof(buf), "%-*.*s", w, w, text);
	boxen_draw_text(win, 0, 0, buf,
	                BOXEN_COLOR_BLACK, BOXEN_COLOR_WHITE, BOXEN_ATTR_BOLD);
}

/* -------------------------------------------------------------------------
 * Input callback (shared by all content windows)
 * ---------------------------------------------------------------------- */

static void on_input(boxen_window_t *win, const boxen_event_t *ev, void *ud) {
	(void)win;
	(void)ud;
	if (ev->type != BOXEN_EV_KEY) return;

	switch (ev->key.key) {
	case BOXEN_KEY_ESCAPE:
	case BOXEN_KEY_CTRL_C:
		boxen_quit();
		return;

	case BOXEN_KEY_TAB:
		/* Cycle focus: 0 -> 1 -> 2 -> 0 */
		g_state.focus_idx = (g_state.focus_idx + 1) % 3;
		boxen_window_focus(focused_win());
		return;

	case BOXEN_KEY_UP:    boxen_window_scroll_by(focused_win(),  0, -1); return;
	case BOXEN_KEY_DOWN:  boxen_window_scroll_by(focused_win(),  0,  1); return;
	case BOXEN_KEY_LEFT:  boxen_window_scroll_by(focused_win(), -1,  0); return;
	case BOXEN_KEY_RIGHT: boxen_window_scroll_by(focused_win(),  1,  0); return;
	case BOXEN_KEY_PGUP:  boxen_window_scroll_by(focused_win(),  0, -10); return;
	case BOXEN_KEY_PGDN:  boxen_window_scroll_by(focused_win(),  0,  10); return;

	default: break;
	}

	/* Printable key check */
	if (ev->key.ch == 'q' || ev->key.ch == 'Q') {
		boxen_quit();
	}
}

/* -------------------------------------------------------------------------
 * Layout construction
 * ---------------------------------------------------------------------- */

/*
 * Build the demo layout for the given terminal dimensions.
 *
 * We use the layout helpers for the content area and open the footer manually
 * so we can configure it (no borders, pinned bottom).
 *
 * split_h opens Left + a temporary Right placeholder.  We close the placeholder
 * and re-split that rect vertically.  split_v opens Top-Right + Bot-Right.
 */
static void build_layout(int tw, int th) {
	/* Reserve one row at the bottom for the keybind footer */
	int content_h = (th > 2) ? (th - 1) : 1;

	boxen_rect_t full = { 0, 0, tw, content_h };

	/* Step 1: horizontal split -- Left (50%) | Right (50%) */
	boxen_window_t *right_placeholder = NULL;
	boxen_layout_split_h(full, 0.50f,
	                     "Left",  &g_state.left,
	                     "Right", &right_placeholder);

	/* Step 2: close the placeholder Right and split its rect vertically */
	int left_w  = 0;
	if (right_placeholder) {
		boxen_rect_t r = boxen_window_get_rect(right_placeholder);
		left_w = r.x;   /* right pane starts at x=left_w */
		boxen_window_close(right_placeholder);
		right_placeholder = NULL;

		/* Vertical split of the right half */
		boxen_rect_t right_rect = { r.x, r.y, r.w, r.h };
		boxen_layout_split_v(right_rect, 0.60f,
		                     "Top Right", &g_state.top_right,
		                     "Bot Right", &g_state.bot_right);
	}
	(void)left_w;

	/* Step 3: keybind footer -- full width, 1 row, borderless, pinned bottom */
	boxen_rect_t footer_rect = { 0, th - 1, tw, 1 };
	g_state.footer = boxen_window_open("footer", footer_rect, NULL);
	if (g_state.footer) {
		boxen_window_set_borders(g_state.footer, false);
		boxen_window_set_pinned(g_state.footer, BOXEN_PIN_BOTTOM);
		boxen_window_set_draw(g_state.footer, draw_footer);
	}
}

/* -------------------------------------------------------------------------
 * main
 * ---------------------------------------------------------------------- */

int main(void) {
	boxen_result_t rc = boxen_init(boxen_tb2_backend(), NULL, NULL);
	if (rc != BOXEN_OK) {
		fprintf(stderr, "boxen_init failed: %s\n", boxen_last_error_str());
		return 1;
	}

	/* Query terminal dimensions from the tb2 backend */
	int tw = 80, th = 24;
	{
		const boxen_backend_t *be = boxen_tb2_backend();
		int w = be->width  ? be->width()  : 0;
		int h = be->height ? be->height() : 0;
		if (w > 0) tw = w;
		if (h > 0) th = h;
	}

	/* Build layout */
	memset(&g_state, 0, sizeof(g_state));
	build_layout(tw, th);

	/* Wire draw + input callbacks and set content sizes */
	if (g_state.left) {
		boxen_window_set_draw(g_state.left, draw_left);
		boxen_window_set_input(g_state.left, on_input);
		boxen_window_set_content_size(g_state.left,
		                              boxen_window_content_width(g_state.left),
		                              LEFT_LINE_COUNT);
	}
	if (g_state.top_right) {
		boxen_window_set_draw(g_state.top_right, draw_top_right);
		boxen_window_set_input(g_state.top_right, on_input);
		boxen_window_set_content_size(g_state.top_right,
		                              boxen_window_content_width(g_state.top_right),
		                              TOP_RIGHT_LINE_COUNT);
	}
	if (g_state.bot_right) {
		boxen_window_set_draw(g_state.bot_right, draw_bot_right);
		boxen_window_set_input(g_state.bot_right, on_input);
		boxen_window_set_content_size(g_state.bot_right,
		                              boxen_window_content_width(g_state.bot_right),
		                              LOG_LINE_COUNT);
	}

	/* Initial focus on the left pane */
	g_state.focus_idx = 0;
	if (g_state.left) {
		boxen_window_focus(g_state.left);
	}

	/* Run the event loop */
	boxen_run();

	boxen_shutdown();
	return 0;
}
