/*
 * palette_render_snapshot_tests.c - Visual-state snapshots of the
 * horizontal-cascade palette renderer.
 *
 * After the host-anchored horizontal cascade rework, each open cascade
 * level renders as a full-width, single-row strip stacked beneath the
 * menubar.  Items reserve 4 cells around their label (bracket + space +
 * label + space + bracket) so neighbours do not jitter as the cursor
 * moves; the brackets render as spaces on non-selected items.
 *
 * Coverage:
 *   - menubar background fill (cyan-on-blue across the entire row)
 *   - selected cascade item shows '[' and ']' brackets
 *   - non-selected cascade items occupy the same cells (no jitter)
 *   - hotkey letter is BOLD only when the item is selected
 *   - hotkey letter is BOLD + bright-yellow when not selected
 *
 * License
 * -------
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Frontier contributors.
 */

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pane.h"
#include "palette.h"
#include "test_report.h"

extern const cell_t *compositor_test_fb_at(int x, int y);
extern void compositor_test_fb_size(int *rows, int *cols);
extern void compositor_test_reset(void);

/* ---------- Synthetic source ---------- */

typedef struct snap_item {
	const char *label;
	char shortcut;
	bool enabled;
	bool is_submenu;
	const char *script;
	struct snap_item *children;
	int child_count;
} snap_item_t;

typedef struct snap_menu {
	const char *label;
	char hotkey;
	snap_item_t *items;
	int item_count;
} snap_menu_t;

typedef struct snap_source {
	snap_menu_t *menus;
	int menu_count;
} snap_source_t;

static snap_item_t *resolve_items(snap_source_t *src, int menu_index,
                                  void *parent_opaque, int *out_count) {
	if (parent_opaque) {
		snap_item_t *parent = (snap_item_t *)parent_opaque;
		*out_count = parent->child_count;
		return parent->children;
	}
	if (menu_index < 0 || menu_index >= src->menu_count) {
		*out_count = 0;
		return NULL;
	}
	*out_count = src->menus[menu_index].item_count;
	return src->menus[menu_index].items;
}

static int s_count_menus(void *ctx) {
	return ((snap_source_t *)ctx)->menu_count;
}
static bool s_menu_describe(void *ctx, int idx, char *out_label, size_t cap,
                            char *out_hotkey) {
	snap_source_t *s = (snap_source_t *)ctx;
	if (idx < 0 || idx >= s->menu_count) return false;
	snprintf(out_label, cap, "%s", s->menus[idx].label);
	*out_hotkey = s->menus[idx].hotkey;
	return true;
}
static int s_item_count(void *ctx, int menu_index, void *parent_opaque) {
	int n = 0;
	(void)resolve_items((snap_source_t *)ctx, menu_index, parent_opaque, &n);
	return n;
}
static bool s_item_describe(void *ctx, int menu_index, void *parent_opaque,
                            int item_index, palette_item_t *out) {
	int n = 0;
	snap_item_t *items = resolve_items((snap_source_t *)ctx, menu_index,
	                                   parent_opaque, &n);
	if (item_index < 0 || item_index >= n) return false;
	snap_item_t *it = &items[item_index];
	memset(out, 0, sizeof(*out));
	snprintf(out->label, sizeof(out->label), "%s", it->label);
	out->shortcut = it->shortcut;
	out->enabled = it->enabled;
	out->is_submenu = it->is_submenu;
	out->opaque = it;
	out->script_handle = it->script ? (void *)it->script : NULL;
	return true;
}

static palette_menu_source_t snap_source(snap_source_t *src) {
	palette_menu_source_t s;
	memset(&s, 0, sizeof(s));
	s.ctx = src;
	s.count_menus = s_count_menus;
	s.menu_describe = s_menu_describe;
	s.item_count = s_item_count;
	s.item_describe = s_item_describe;
	return s;
}

static void render_all(palette_state_t *st) {
	palette_render_state(st);
	compositor_render();
}

/* Capture all cells of a single row into a caller-provided array, up to
 * the smaller of the framebuffer's column count and `cap`.  Returns the
 * number of cells captured. */
static int snapshot_row(int y, cell_t *out, int cap) {
	int rows = 0, cols = 0;
	compositor_test_fb_size(&rows, &cols);
	if (y < 0 || y >= rows) return 0;
	int n = cols < cap ? cols : cap;
	for (int x = 0; x < n; ++x) {
		const cell_t *c = compositor_test_fb_at(x, y);
		out[x] = *c;
	}
	return n;
}

/* ---------- Fixture ---------- */

static snap_item_t g_repl_items[] = {
	{ "Help",  'H', true, false, "repl.help()",  NULL, 0 },
	{ "Clear", 'C', true, false, "repl.clear()", NULL, 0 },
	{ "Exit",  'X', true, false, "repl.exit()",  NULL, 0 },
};

static snap_menu_t g_menus[] = {
	{ "REPL", 'R', g_repl_items, 3 },
	{ "File", 'F', NULL, 0 },
};

static snap_source_t g_src = { g_menus, 2 };

/* ---------- Tests ---------- */

static void test_menubar_strip_full_width(void) {
	/* The menubar strip background paints across the entire term_cols
	 * width (not just behind the entries) so the strip reads as one
	 * continuous bar.  Cells in the SELECTED menubar entry use the
	 * inverse-style white bg; every other cell uses the menubar's
	 * blue bg -- including the empty trailing region past the last
	 * entry. */
	compositor_test_reset();
	compositor_on_resize(24, 80);
	palette_state_t st;
	palette_menu_source_t src = snap_source(&g_src);
	palette_open(&st, 24, 80, 0, &src);
	render_all(&st);

	int y = st.menubar.y;
	int rows = 0, cols = 0;
	compositor_test_fb_size(&rows, &cols);
	/* The selected entry (REPL) occupies cells menu_x[0]..menu_x[0]+w-1.
	 * Cells past menu_x[1]+menu_w[1] (trailing fill) must be blue. */
	int trailing_start = st.menu_x[1] + st.menu_w[1];
	int n_trailing_blue = 0;
	for (int x = trailing_start; x < cols; ++x) {
		const cell_t *c = compositor_test_fb_at(x, y);
		assert(c != NULL);
		assert(c->bg == PALETTE_COLOR_BLUE);
		n_trailing_blue++;
	}
	assert(n_trailing_blue > 0);
	/* The non-selected menubar entry "File" must also be blue. */
	for (int x = st.menu_x[1]; x < st.menu_x[1] + st.menu_w[1]; ++x) {
		const cell_t *c = compositor_test_fb_at(x, y);
		assert(c->bg == PALETTE_COLOR_BLUE);
	}

	palette_close(&st);
}

static void test_horizontal_strip_full_width(void) {
	/* The cascade strip is also full-width with bg fill.  The selected
	 * item's 4 reserved cells (bracket + space + label + space +
	 * bracket) carry the inverse white bg; everything else on the
	 * strip carries the menubar blue bg, including the trailing fill
	 * past the last item. */
	compositor_test_reset();
	compositor_on_resize(24, 80);
	palette_state_t st;
	palette_menu_source_t src = snap_source(&g_src);
	palette_open(&st, 24, 80, 0, &src);
	palette_feed_byte(&st, '\r');           /* open REPL menu */
	render_all(&st);

	int y = st.levels[0].pane.y;
	int rows = 0, cols = 0;
	compositor_test_fb_size(&rows, &cols);
	int n_blue = 0;
	int n_white = 0;
	for (int x = 0; x < cols; ++x) {
		const cell_t *c = compositor_test_fb_at(x, y);
		assert(c != NULL);
		if (c->bg == PALETTE_COLOR_WHITE) n_white++;
		else if (c->bg == PALETTE_COLOR_BLUE) n_blue++;
		else {
			/* Neither selected nor default -- fail. */
			assert(0 && "unexpected bg on cascade strip");
		}
	}
	/* "Help" is selected -- 4 reserved cells around the 4-char label
	 * means 8 white cells (label + 2 spaces + 2 brackets). */
	assert(n_white == 4 + 2 + 2);
	/* The rest of the row is blue. */
	assert(n_blue == cols - n_white);

	palette_close(&st);
}

static void test_item_bracket_reservation(void) {
	/* The cells occupied by the cascade items must be identical
	 * (positionally) regardless of which item is selected.  Snapshot
	 * the row with cursor on item 0, move RIGHT, snapshot again, and
	 * compare the cells of NON-selected items between the two
	 * snapshots.  They must be unchanged. */
	compositor_test_reset();
	compositor_on_resize(24, 80);
	palette_state_t st;
	palette_menu_source_t src = snap_source(&g_src);
	palette_open(&st, 24, 80, 0, &src);

	palette_feed_byte(&st, '\r');           /* open REPL */
	render_all(&st);

	cell_t row_a[80];
	int n_a = snapshot_row(st.levels[0].pane.y, row_a, 80);
	assert(n_a > 0);

	/* Move cursor RIGHT to item 1, snapshot again. */
	palette_feed_byte(&st, 0x1b);
	palette_feed_byte(&st, '[');
	palette_feed_byte(&st, 'C');
	assert(st.levels[0].cursor == 1);
	render_all(&st);

	cell_t row_b[80];
	int n_b = snapshot_row(st.levels[0].pane.y, row_b, 80);
	assert(n_b == n_a);

	/* For every cell that contains a label character of NON-selected
	 * items in both snapshots (i.e. the characters of Clear and Exit
	 * when cursor is on Help, vs. Help and Exit when cursor on Clear),
	 * the character itself must be in the same column.  Strategy: find
	 * the 'C' of "Clear" and the 'E' of "Exit" in both snapshots and
	 * assert the columns are identical. */
	int col_c_a = -1, col_e_a = -1;
	int col_c_b = -1, col_e_b = -1;
	for (int x = 0; x < n_a; ++x) {
		if (col_c_a < 0 && row_a[x].ch == 'C') col_c_a = x;
		if (col_e_a < 0 && row_a[x].ch == 'E') col_e_a = x;
	}
	for (int x = 0; x < n_b; ++x) {
		if (col_c_b < 0 && row_b[x].ch == 'C') col_c_b = x;
		if (col_e_b < 0 && row_b[x].ch == 'E') col_e_b = x;
	}
	assert(col_c_a >= 0 && col_e_a >= 0);
	assert(col_c_b >= 0 && col_e_b >= 0);
	assert(col_c_a == col_c_b);
	assert(col_e_a == col_e_b);

	palette_close(&st);
}

static void test_selected_item_has_brackets(void) {
	/* The selected item's first and last (of its 4 reserved cells)
	 * contain '[' and ']'.  Non-selected items have spaces in those
	 * positions. */
	compositor_test_reset();
	compositor_on_resize(24, 80);
	palette_state_t st;
	palette_menu_source_t src = snap_source(&g_src);
	palette_open(&st, 24, 80, 0, &src);

	palette_feed_byte(&st, '\r');           /* open REPL, cursor on Help */
	render_all(&st);

	int y = st.levels[0].pane.y;
	cell_t row[80];
	int n = snapshot_row(y, row, 80);
	assert(n > 0);

	/* Find the 'H' of "Help" and inspect the two cells before and one
	 * cell after the label. */
	int col_h = -1;
	for (int x = 0; x < n; ++x) {
		if (row[x].ch == 'H') { col_h = x; break; }
	}
	assert(col_h >= 2);
	int label_len = 4;     /* "Help" */
	/* Bracket cells: col_h - 2 (open) and col_h + label_len + 1 (close). */
	const cell_t *open_b = &row[col_h - 2];
	const cell_t *close_b = &row[col_h + label_len + 1];
	assert(open_b->ch == '[');
	assert(close_b->ch == ']');

	/* Non-selected Clear/Exit: their open-bracket positions should be
	 * spaces.  Find 'C' of Clear. */
	int col_c = -1;
	for (int x = 0; x < n; ++x) {
		if (row[x].ch == 'C') { col_c = x; break; }
	}
	assert(col_c >= 2);
	assert(row[col_c - 2].ch == ' ');
	assert(row[col_c + 5 + 1].ch == ' ');  /* "Clear" len=5 */

	palette_close(&st);
}

static void test_hotkey_bold_yellow_when_not_selected(void) {
	/* On a non-selected item, the hotkey letter cell carries BOLD
	 * attribute and the bright-yellow foreground. */
	compositor_test_reset();
	compositor_on_resize(24, 80);
	palette_state_t st;
	palette_menu_source_t src = snap_source(&g_src);
	palette_open(&st, 24, 80, 0, &src);

	palette_feed_byte(&st, '\r');           /* cursor on Help (item 0) */
	render_all(&st);

	int y = st.levels[0].pane.y;
	cell_t row[80];
	(void)snapshot_row(y, row, 80);

	/* "Clear" is not selected -- its 'C' hotkey is bright-yellow + BOLD. */
	int col_c = -1;
	for (int x = 0; x < 80; ++x) {
		if (row[x].ch == 'C') { col_c = x; break; }
	}
	assert(col_c >= 0);
	assert(row[col_c].attr & PALETTE_ATTR_BOLD);
	assert(row[col_c].fg == PALETTE_COLOR_BRIGHT_YELLOW);

	palette_close(&st);
}

static void test_hotkey_bold_only_when_selected(void) {
	/* On the selected item, the hotkey letter is BOLD but NOT
	 * overridden to yellow -- the inverse-style selection cell handles
	 * the highlighting on its own. */
	compositor_test_reset();
	compositor_on_resize(24, 80);
	palette_state_t st;
	palette_menu_source_t src = snap_source(&g_src);
	palette_open(&st, 24, 80, 0, &src);

	palette_feed_byte(&st, '\r');           /* cursor on Help (selected) */
	render_all(&st);

	int y = st.levels[0].pane.y;
	cell_t row[80];
	(void)snapshot_row(y, row, 80);

	int col_h = -1;
	for (int x = 0; x < 80; ++x) {
		if (row[x].ch == 'H') { col_h = x; break; }
	}
	assert(col_h >= 0);
	/* Selected hotkey: BOLD set, but fg is the selected scheme (black),
	 * not bright-yellow. */
	assert(row[col_h].attr & PALETTE_ATTR_BOLD);
	assert(row[col_h].fg != PALETTE_COLOR_BRIGHT_YELLOW);
	assert(row[col_h].fg == PALETTE_COLOR_BLACK);
	assert(row[col_h].bg == PALETTE_COLOR_WHITE);

	palette_close(&st);
}

static void test_menubar_hotkey_styling(void) {
	/* On the menubar itself, the hotkey letter of a non-selected entry
	 * is bright yellow on blue. */
	compositor_test_reset();
	compositor_on_resize(24, 80);
	palette_state_t st;
	palette_menu_source_t src = snap_source(&g_src);
	palette_open(&st, 24, 80, 0, &src);
	render_all(&st);

	int y = st.menubar.y;
	cell_t row[80];
	(void)snapshot_row(y, row, 80);

	/* "File" is not selected (REPL is the initial selection).  Find F. */
	int col_f = -1;
	for (int x = 0; x < 80; ++x) {
		if (row[x].ch == 'F') { col_f = x; break; }
	}
	assert(col_f >= 0);
	assert(row[col_f].attr & PALETTE_ATTR_BOLD);
	assert(row[col_f].fg == PALETTE_COLOR_BRIGHT_YELLOW);
	assert(row[col_f].bg == PALETTE_COLOR_BLUE);

	palette_close(&st);
}

int main(void) {
	TR_INIT("palette_render_snapshot_tests");
	TR_RUN(test_menubar_strip_full_width);
	TR_RUN(test_horizontal_strip_full_width);
	TR_RUN(test_item_bracket_reservation);
	TR_RUN(test_selected_item_has_brackets);
	TR_RUN(test_hotkey_bold_yellow_when_not_selected);
	TR_RUN(test_hotkey_bold_only_when_selected);
	TR_RUN(test_menubar_hotkey_styling);
	TR_SUMMARY();
	return TR_EXIT_CODE();
}
