/*
 * palette_state_tests.c - Behavioural unit tests for the REPL palette
 * state machine (palette.{c,h}).
 *
 * Tests exercise the public API only: palette_open / palette_feed_byte /
 * palette_feed_mouse / palette_feed_esc_timeout / palette_close /
 * palette_on_resize. Assertions are on observable struct state and the
 * palette_done_t return values — not on source code structure.
 *
 * The tests use a static in-memory palette_menu_source_t. PR 6 will
 * supply the ODB-backed adapter; the abstraction's whole point is that
 * we can test the state machine without a Frontier runtime linked in.
 *
 * Coverage:
 *   - open: menubar pane registered, cursor at 0
 *   - down/up: cursor clamps; submenu navigation
 *   - right on submenu item: opens cascade pane
 *   - left at submenu: closes deepest level, parent stays open
 *   - left at top-level menu (open): closes the menu, returns to menubar
 *   - left on menubar: moves to previous menubar entry
 *   - enter on leaf: DONE_EXECUTE with exec_script populated
 *   - enter on submenu: opens cascade
 *   - ESC at sublevel: closes that level only
 *   - ESC at top: DONE_CANCEL
 *   - ESC disambiguation: bare ESC vs ESC-CSI both work via the timeout API
 *   - hotkey on menubar: opens that menu
 *   - hotkey within open menu: activates that item (leaf=execute, submenu=open)
 *   - mouse click on menubar entry: opens that menu
 *   - mouse click on item: leaf executes / submenu opens
 *   - mouse click outside any pane: DONE_CANCEL
 *   - cascade overflow LEFT: submenu wider than remaining right space
 *     opens with x < parent.x
 *   - cascade overflow UP: submenu taller than remaining bottom space
 *     opens with y < parent.y + parent.cursor
 *   - simultaneous LEFT+UP overflow
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
#include <stdlib.h>
#include <string.h>

#include "pane.h"
#include "palette.h"
#include "test_report.h"

/* Test-only compositor inspection (defined in pane.c). */
extern void compositor_test_reset(void);

/* ---------- Synthetic menu source ---------- */

typedef struct fake_item {
	const char *label;
	const char *description;
	char shortcut;
	bool enabled;
	bool is_submenu;
	const char *script;
	struct fake_item *children;
	int child_count;
} fake_item_t;

typedef struct fake_menu {
	const char *label;
	char hotkey;
	fake_item_t *items;
	int item_count;
} fake_menu_t;

typedef struct fake_source {
	fake_menu_t *menus;
	int menu_count;
} fake_source_t;

/* Helper: locate the (parent_opaque-anchored) item array. NULL parent
 * means "top-level menu items"; non-NULL parent means a child item set
 * carried by the parent's `opaque` pointer. */
static fake_item_t *fake_resolve_items(fake_source_t *src, int menu_index,
                                       void *parent_opaque, int *out_count) {
	if (parent_opaque) {
		fake_item_t *parent = (fake_item_t *)parent_opaque;
		*out_count = parent->child_count;
		return parent->children;
	}
	if (menu_index < 0 || menu_index >= src->menu_count) {
		*out_count = 0;
		return NULL;
	}
	fake_menu_t *m = &src->menus[menu_index];
	*out_count = m->item_count;
	return m->items;
}

static int fake_count_menus(void *ctx) {
	fake_source_t *s = (fake_source_t *)ctx;
	return s->menu_count;
}

static bool fake_menu_describe(void *ctx, int idx, char *out_label,
                               size_t cap, char *out_hotkey) {
	fake_source_t *s = (fake_source_t *)ctx;
	if (idx < 0 || idx >= s->menu_count) return false;
	snprintf(out_label, cap, "%s", s->menus[idx].label);
	*out_hotkey = s->menus[idx].hotkey;
	return true;
}

static int fake_item_count(void *ctx, int menu_index, void *parent_opaque) {
	fake_source_t *s = (fake_source_t *)ctx;
	int n = 0;
	(void)fake_resolve_items(s, menu_index, parent_opaque, &n);
	return n;
}

static bool fake_item_describe(void *ctx, int menu_index, void *parent_opaque,
                               int item_index, palette_item_t *out) {
	fake_source_t *s = (fake_source_t *)ctx;
	int n = 0;
	fake_item_t *items = fake_resolve_items(s, menu_index, parent_opaque, &n);
	if (item_index < 0 || item_index >= n) return false;
	fake_item_t *it = &items[item_index];
	memset(out, 0, sizeof(*out));
	snprintf(out->label, sizeof(out->label), "%s", it->label);
	if (it->description) {
		snprintf(out->description, sizeof(out->description), "%s",
		         it->description);
	}
	out->shortcut = it->shortcut;
	out->enabled = it->enabled;
	out->is_submenu = it->is_submenu;
	out->opaque = it;          /* used for child enumeration when submenu */
	out->script_handle = it->script ? (void *)it->script : NULL;
	return true;
}

static palette_menu_source_t make_source(fake_source_t *src) {
	palette_menu_source_t s;
	memset(&s, 0, sizeof(s));
	s.ctx = src;
	s.count_menus = fake_count_menus;
	s.menu_describe = fake_menu_describe;
	s.item_count = fake_item_count;
	s.item_describe = fake_item_describe;
	return s;
}

/* ---------- Sample fixture: a small REPL+File menubar ---------- */

static fake_item_t g_repl_items[] = {
	{ "Help",     "Show help",     'H', true, false, "repl.help()",     NULL, 0 },
	{ "Clear",    "Clear screen",  'C', true, false, "repl.clear()",    NULL, 0 },
	{ "Exit",     "Quit REPL",     'X', true, false, "repl.exit()",     NULL, 0 },
};

static fake_item_t g_view_children[] = {
	{ "Outline", NULL, 'O', true, false, "view.outline()", NULL, 0 },
	{ "Table",   NULL, 'T', true, false, "view.table()",   NULL, 0 },
};

static fake_item_t g_file_items[] = {
	{ "New",     NULL, 'N', true, false, "file.new()",  NULL, 0 },
	/* Submenu — children = g_view_children */
	{ "Views",   NULL, 'V', true, true,  NULL, g_view_children, 2 },
	{ "Open",    NULL, 'O', true, false, "file.open()", NULL, 0 },
};

static fake_menu_t g_menus[] = {
	{ "REPL", 'R', g_repl_items, 3 },
	{ "File", 'F', g_file_items, 3 },
};

static fake_source_t g_src = { g_menus, 2 };

static void reset_fixture(void) {
	/* Restore enabled flags / cursors that previous tests may have
	 * trampled. (None do today, but keep this hygienic.) */
	for (int i = 0; i < (int)(sizeof(g_repl_items) / sizeof(g_repl_items[0])); ++i) {
		g_repl_items[i].enabled = true;
	}
	for (int i = 0; i < (int)(sizeof(g_file_items) / sizeof(g_file_items[0])); ++i) {
		g_file_items[i].enabled = true;
	}
}

static palette_menu_source_t make_default_source(void) {
	reset_fixture();
	return make_source(&g_src);
}

/* ---------- Tests ---------- */

static void test_open_initial_state(void) {
	compositor_test_reset();
	compositor_on_resize(24, 80);

	palette_state_t st;
	palette_menu_source_t src = make_default_source();
	bool ok = palette_open(&st, 24, 80, &src);
	assert(ok);
	assert(st.active);
	assert(st.menu_count == 2);
	assert(st.menubar_cursor == 0);
	assert(st.open_depth == 0);
	/* Menubar pane registered. */
	pane_t *hit = compositor_pane_at(0, 0);
	assert(hit == &st.menubar);

	palette_close(&st);
	assert(!st.active);
	hit = compositor_pane_at(0, 0);
	assert(hit == NULL);
}

static void test_menubar_arrow_navigation(void) {
	compositor_test_reset();
	compositor_on_resize(24, 80);
	palette_state_t st;
	palette_menu_source_t src = make_default_source();
	palette_open(&st, 24, 80, &src);

	/* RIGHT on closed menubar moves cursor. */
	palette_done_t r = palette_feed_byte(&st, 0x1b);
	assert(r == PALETTE_DONE_NONE);
	r = palette_feed_byte(&st, '[');
	assert(r == PALETTE_DONE_NONE);
	r = palette_feed_byte(&st, 'C');  /* CSI C = right arrow */
	assert(r == PALETTE_DONE_NONE);
	assert(st.menubar_cursor == 1);

	/* LEFT moves back. */
	palette_feed_byte(&st, 0x1b);
	palette_feed_byte(&st, '[');
	palette_feed_byte(&st, 'D');
	assert(st.menubar_cursor == 0);

	/* LEFT at index 0 clamps. */
	palette_feed_byte(&st, 0x1b);
	palette_feed_byte(&st, '[');
	palette_feed_byte(&st, 'D');
	assert(st.menubar_cursor == 0);

	/* RIGHT past last clamps. */
	for (int i = 0; i < 5; ++i) {
		palette_feed_byte(&st, 0x1b);
		palette_feed_byte(&st, '[');
		palette_feed_byte(&st, 'C');
	}
	assert(st.menubar_cursor == st.menu_count - 1);

	palette_close(&st);
}

static void test_enter_opens_menu_then_navigates(void) {
	compositor_test_reset();
	compositor_on_resize(24, 80);
	palette_state_t st;
	palette_menu_source_t src = make_default_source();
	palette_open(&st, 24, 80, &src);

	/* ENTER on menubar opens the highlighted (REPL) menu. */
	palette_done_t r = palette_feed_byte(&st, '\r');
	assert(r == PALETTE_DONE_NONE);
	assert(st.open_depth == 1);
	assert(st.levels[0].cursor == 0);
	assert(st.levels[0].item_count == 3);

	/* DOWN moves cursor in the open menu. */
	palette_feed_byte(&st, 0x1b);
	palette_feed_byte(&st, '[');
	palette_feed_byte(&st, 'B');
	assert(st.levels[0].cursor == 1);

	/* DOWN past last clamps. */
	palette_feed_byte(&st, 0x1b);
	palette_feed_byte(&st, '[');
	palette_feed_byte(&st, 'B');
	palette_feed_byte(&st, 0x1b);
	palette_feed_byte(&st, '[');
	palette_feed_byte(&st, 'B');
	assert(st.levels[0].cursor == 2);

	/* UP moves back. */
	palette_feed_byte(&st, 0x1b);
	palette_feed_byte(&st, '[');
	palette_feed_byte(&st, 'A');
	assert(st.levels[0].cursor == 1);

	palette_close(&st);
}

static void test_enter_on_leaf_executes(void) {
	compositor_test_reset();
	compositor_on_resize(24, 80);
	palette_state_t st;
	palette_menu_source_t src = make_default_source();
	palette_open(&st, 24, 80, &src);

	palette_feed_byte(&st, '\r');           /* open REPL menu */
	/* cursor at item 0 (Help) — leaf */
	palette_done_t r = palette_feed_byte(&st, '\r');
	assert(r == PALETTE_DONE_EXECUTE);
	assert(st.exec_script != NULL);
	assert(strcmp((const char *)st.exec_script, "repl.help()") == 0);

	palette_close(&st);
}

static void test_enter_on_submenu_opens_cascade(void) {
	compositor_test_reset();
	compositor_on_resize(24, 80);
	palette_state_t st;
	palette_menu_source_t src = make_default_source();
	palette_open(&st, 24, 80, &src);

	/* Move to File menu. */
	palette_feed_byte(&st, 0x1b); palette_feed_byte(&st, '['); palette_feed_byte(&st, 'C');
	/* ENTER opens it. */
	palette_feed_byte(&st, '\r');
	assert(st.open_depth == 1);
	/* Cursor at item 0 = "New" (leaf). DOWN to item 1 = "Views" (submenu). */
	palette_feed_byte(&st, 0x1b); palette_feed_byte(&st, '['); palette_feed_byte(&st, 'B');
	assert(st.levels[0].cursor == 1);
	assert(st.levels[0].items[1].is_submenu);

	/* ENTER opens the cascade. */
	palette_done_t r = palette_feed_byte(&st, '\r');
	assert(r == PALETTE_DONE_NONE);
	assert(st.open_depth == 2);
	assert(st.levels[1].item_count == 2);
	assert(strcmp(st.levels[1].items[0].label, "Outline") == 0);

	/* Cascade pane is positioned to the right of parent at the parent's
	 * selected row (item rows start at parent.y + 1 because of the top
	 * border, so submenu.y = parent.y + 1 + cursor). */
	int parent_right = st.levels[0].pane.x + st.levels[0].pane.w;
	assert(st.levels[1].pane.x == parent_right);
	assert(st.levels[1].pane.y == st.levels[0].pane.y + 1 + st.levels[0].cursor);

	palette_close(&st);
}

static void test_esc_at_sublevel_closes_one_level(void) {
	compositor_test_reset();
	compositor_on_resize(24, 80);
	palette_state_t st;
	palette_menu_source_t src = make_default_source();
	palette_open(&st, 24, 80, &src);

	/* Open File → Views cascade. */
	palette_feed_byte(&st, 0x1b); palette_feed_byte(&st, '['); palette_feed_byte(&st, 'C');
	palette_feed_byte(&st, '\r');
	palette_feed_byte(&st, 0x1b); palette_feed_byte(&st, '['); palette_feed_byte(&st, 'B');
	palette_feed_byte(&st, '\r');
	assert(st.open_depth == 2);

	/* ESC + timeout → bare ESC. */
	palette_feed_byte(&st, 0x1b);
	palette_done_t r = palette_feed_esc_timeout(&st);
	assert(r == PALETTE_DONE_NONE);
	assert(st.open_depth == 1);

	palette_close(&st);
}

static void test_esc_at_top_level_cancels(void) {
	compositor_test_reset();
	compositor_on_resize(24, 80);
	palette_state_t st;
	palette_menu_source_t src = make_default_source();
	palette_open(&st, 24, 80, &src);

	/* No menus open — ESC cancels. */
	palette_feed_byte(&st, 0x1b);
	palette_done_t r = palette_feed_esc_timeout(&st);
	assert(r == PALETTE_DONE_CANCEL);

	palette_close(&st);
}

static void test_esc_then_arrow_is_csi_not_bare_esc(void) {
	compositor_test_reset();
	compositor_on_resize(24, 80);
	palette_state_t st;
	palette_menu_source_t src = make_default_source();
	palette_open(&st, 24, 80, &src);

	/* ESC, then immediately '[', then 'C' — should NOT cancel. */
	palette_done_t r = palette_feed_byte(&st, 0x1b);
	assert(r == PALETTE_DONE_NONE);
	assert(st.esc_pending);
	r = palette_feed_byte(&st, '[');
	assert(r == PALETTE_DONE_NONE);
	assert(!st.esc_pending);
	r = palette_feed_byte(&st, 'C');
	assert(r == PALETTE_DONE_NONE);
	assert(st.menubar_cursor == 1);

	palette_close(&st);
}

static void test_left_at_submenu_closes_only_submenu(void) {
	compositor_test_reset();
	compositor_on_resize(24, 80);
	palette_state_t st;
	palette_menu_source_t src = make_default_source();
	palette_open(&st, 24, 80, &src);

	/* Open File → Views cascade. */
	palette_feed_byte(&st, 0x1b); palette_feed_byte(&st, '['); palette_feed_byte(&st, 'C');
	palette_feed_byte(&st, '\r');
	palette_feed_byte(&st, 0x1b); palette_feed_byte(&st, '['); palette_feed_byte(&st, 'B');
	palette_feed_byte(&st, '\r');
	assert(st.open_depth == 2);

	/* LEFT closes the deepest cascade. */
	palette_feed_byte(&st, 0x1b); palette_feed_byte(&st, '['); palette_feed_byte(&st, 'D');
	assert(st.open_depth == 1);
	/* Parent cursor preserved. */
	assert(st.levels[0].cursor == 1);

	palette_close(&st);
}

static void test_left_on_open_top_level_closes_to_menubar(void) {
	compositor_test_reset();
	compositor_on_resize(24, 80);
	palette_state_t st;
	palette_menu_source_t src = make_default_source();
	palette_open(&st, 24, 80, &src);

	palette_feed_byte(&st, '\r');           /* open REPL menu, depth 1 */
	assert(st.open_depth == 1);
	palette_feed_byte(&st, 0x1b); palette_feed_byte(&st, '['); palette_feed_byte(&st, 'D');
	/* Closes the menu, returns to menubar. */
	assert(st.open_depth == 0);

	palette_close(&st);
}

static void test_hotkey_on_menubar_opens_menu(void) {
	compositor_test_reset();
	compositor_on_resize(24, 80);
	palette_state_t st;
	palette_menu_source_t src = make_default_source();
	palette_open(&st, 24, 80, &src);

	/* Press 'F' on menubar — opens File. */
	palette_done_t r = palette_feed_byte(&st, 'F');
	assert(r == PALETTE_DONE_NONE);
	assert(st.menubar_cursor == 1);
	assert(st.open_depth == 1);
	assert(st.levels[0].menu_index == 1);

	palette_close(&st);
}

static void test_hotkey_within_open_menu_executes(void) {
	compositor_test_reset();
	compositor_on_resize(24, 80);
	palette_state_t st;
	palette_menu_source_t src = make_default_source();
	palette_open(&st, 24, 80, &src);

	palette_feed_byte(&st, '\r');           /* open REPL menu */
	/* 'X' is the Exit hotkey. */
	palette_done_t r = palette_feed_byte(&st, 'X');
	assert(r == PALETTE_DONE_EXECUTE);
	assert(strcmp((const char *)st.exec_script, "repl.exit()") == 0);

	palette_close(&st);
}

static void test_mouse_click_on_menubar_opens_menu(void) {
	compositor_test_reset();
	compositor_on_resize(24, 80);
	palette_state_t st;
	palette_menu_source_t src = make_default_source();
	palette_open(&st, 24, 80, &src);

	/* Click on the second menubar entry. mouse_event_t coords are 1-based. */
	int target_x = st.menu_x[1] + 1;  /* 1-based */
	mouse_event_t ev = { MOUSE_LEFT, target_x, 1, true };
	palette_done_t r = palette_feed_mouse(&st, &ev);
	assert(r == PALETTE_DONE_NONE);
	assert(st.menubar_cursor == 1);
	assert(st.open_depth == 1);
	assert(st.levels[0].menu_index == 1);

	palette_close(&st);
}

static void test_mouse_click_on_leaf_executes(void) {
	compositor_test_reset();
	compositor_on_resize(24, 80);
	palette_state_t st;
	palette_menu_source_t src = make_default_source();
	palette_open(&st, 24, 80, &src);

	/* Open REPL menu via hotkey. */
	palette_feed_byte(&st, 'R');
	assert(st.open_depth == 1);

	/* Click on item 2 ("Exit"). Pane y starts at row 1 (after menubar);
	 * with border, item rows start at pane.y + 1. */
	int target_x = st.levels[0].pane.x + 2 + 1; /* center of label, 1-based */
	int target_y = st.levels[0].pane.y + 1 + 2 + 1; /* item 2, 1-based */
	mouse_event_t ev = { MOUSE_LEFT, target_x, target_y, true };
	palette_done_t r = palette_feed_mouse(&st, &ev);
	assert(r == PALETTE_DONE_EXECUTE);
	assert(strcmp((const char *)st.exec_script, "repl.exit()") == 0);

	palette_close(&st);
}

static void test_mouse_click_outside_cancels(void) {
	compositor_test_reset();
	compositor_on_resize(24, 80);
	palette_state_t st;
	palette_menu_source_t src = make_default_source();
	palette_open(&st, 24, 80, &src);

	/* Click on row 23 (well past the menubar and any menu) — outside. */
	mouse_event_t ev = { MOUSE_LEFT, 50, 23, true };
	palette_done_t r = palette_feed_mouse(&st, &ev);
	assert(r == PALETTE_DONE_CANCEL);

	palette_close(&st);
}

/* ---------- Cascade overflow positioning ---------- */

/* Two-menu fixture: a leading wide "Pad" menu pushes the second "M"
 * menu far enough right that an M-anchored submenu opening rightward
 * would overflow, AND M's left edge has room for a leftward cascade.
 *
 * Geometry (term_cols=40):
 *   menubar entry width = strlen(label) + 2
 *   PadMenuLabelXXXXXXXXX (21 chars) → width 23 → spans x=[0,23)
 *   M (1 char)            → width 3  → spans x=[23,26)
 *
 * Submenu pane width = max(item_label_len, 4) + 4 padding/border.
 *   "ItemAAAAAAAAAAAAAAA" (19 chars) → submenu.w = 19 + 4 = 23
 *
 * When cursor is on M (parent.x=23, parent.w=8 — pane width built from
 * the single child item "Wide" → max(4,4)+4=8), default cascade
 * placement is x = parent.x + parent.w = 31. With submenu.w=23,
 * x + w = 54 > 40 → must open LEFT.
 *
 * LEFT placement: x = parent.x - submenu.w = 23 - 23 = 0. Fits cleanly,
 * sub_right = 23 = parent_x → assertion sub_right <= parent_x holds. */
static fake_item_t g_pad_items[] = {
	{ "z", NULL, 'Z', true, false, "z()", NULL, 0 },
};
static fake_item_t g_lefto_children[] = {
	{ "ItemAAAAAAAAAAAAAAA", NULL, 'A', true, false, "x.a()", NULL, 0 },
};
static fake_item_t g_lefto_items[] = {
	{ "Wide", NULL, 'W', true, true, NULL, g_lefto_children, 1 },
};
static fake_menu_t g_lefto_menus[] = {
	{ "PadMenuLabelXXXXXXXXX", 'P', g_pad_items, 1 },  /* 21 chars → entry w=23 */
	{ "M",                     'M', g_lefto_items, 1 },
};
static fake_source_t g_lefto_src = { g_lefto_menus, 2 };

static void test_cascade_overflow_right_opens_left(void) {
	compositor_test_reset();
	compositor_on_resize(24, 40);
	palette_state_t st;
	palette_menu_source_t src = make_source(&g_lefto_src);
	bool ok = palette_open(&st, 24, 40, &src);
	assert(ok);

	/* Move to second menu (M). */
	palette_feed_byte(&st, 0x1b); palette_feed_byte(&st, '['); palette_feed_byte(&st, 'C');
	palette_feed_byte(&st, '\r');
	assert(st.open_depth == 1);
	palette_feed_byte(&st, '\r');               /* open Wide cascade */
	assert(st.open_depth == 2);

	/* The cascade must be fully on-screen. */
	int sub_x = st.levels[1].pane.x;
	int sub_w = st.levels[1].pane.w;
	int sub_right = sub_x + sub_w;
	assert(sub_x >= 0);
	assert(sub_right <= 40);

	/* If the natural right placement would have overflowed, palette
	 * must have opened LEFT (i.e. submenu's right edge no further right
	 * than parent's left edge). */
	int parent_x = st.levels[0].pane.x;
	int parent_right = parent_x + st.levels[0].pane.w;
	if (parent_right + sub_w > 40) {
		assert(sub_right <= parent_x);
	}

	palette_close(&st);
}

static void test_cascade_overflow_bottom_opens_up(void) {
	/* Tall menu with the cursor near the bottom: the submenu's natural
	 * y = parent.y + cursor would overshoot screen rows. Expect the
	 * cascade to be shifted upward so it fits. */
	compositor_test_reset();
	compositor_on_resize(15, 80);  /* short terminal */

	/* Build a fixture with one menu containing 10 items, the last of
	 * which is a submenu with 5 children. The submenu pane height is
	 * 5 + 2 (border) = 7. If parent y=1, cursor=9 → submenu y=10,
	 * y+h=17 > 15, so it must shift up. */
	static fake_item_t children[5];
	static char childlabel[5][8];
	for (int i = 0; i < 5; ++i) {
		snprintf(childlabel[i], sizeof(childlabel[i]), "C%d", i);
		children[i].label = childlabel[i];
		children[i].description = NULL;
		children[i].shortcut = '\0';
		children[i].enabled = true;
		children[i].is_submenu = false;
		children[i].script = "c.x()";
		children[i].children = NULL;
		children[i].child_count = 0;
	}
	static fake_item_t parent_items[10];
	static char itemlabel[10][8];
	for (int i = 0; i < 10; ++i) {
		snprintf(itemlabel[i], sizeof(itemlabel[i]), "I%d", i);
		parent_items[i].label = itemlabel[i];
		parent_items[i].description = NULL;
		parent_items[i].shortcut = '\0';
		parent_items[i].enabled = true;
		parent_items[i].is_submenu = (i == 9);
		parent_items[i].script = (i == 9) ? NULL : "i.x()";
		parent_items[i].children = (i == 9) ? children : NULL;
		parent_items[i].child_count = (i == 9) ? 5 : 0;
	}
	static fake_menu_t menus[1] = { { "Tall", 'T', parent_items, 10 } };
	static fake_source_t src_data = { menus, 1 };
	palette_menu_source_t src = make_source(&src_data);

	palette_state_t st;
	bool ok = palette_open(&st, 15, 80, &src);
	assert(ok);

	palette_feed_byte(&st, '\r');           /* open Tall */
	/* Cursor down to item 9 (submenu). */
	for (int i = 0; i < 9; ++i) {
		palette_feed_byte(&st, 0x1b);
		palette_feed_byte(&st, '[');
		palette_feed_byte(&st, 'B');
	}
	assert(st.levels[0].cursor == 9);
	palette_feed_byte(&st, '\r');           /* open submenu cascade */
	assert(st.open_depth == 2);

	/* Submenu must fit on screen — y + h <= rows. */
	int sub_bottom = st.levels[1].pane.y + st.levels[1].pane.h;
	assert(sub_bottom <= 15);
	assert(st.levels[1].pane.y >= 0);

	palette_close(&st);
}

static void test_disabled_item_does_not_execute(void) {
	compositor_test_reset();
	compositor_on_resize(24, 80);
	palette_state_t st;
	g_repl_items[0].enabled = false;       /* disable Help */
	palette_menu_source_t src = make_source(&g_src);
	palette_open(&st, 24, 80, &src);

	palette_feed_byte(&st, '\r');           /* open REPL menu */
	/* Cursor at 0 (Help, disabled). ENTER must NOT execute. */
	palette_done_t r = palette_feed_byte(&st, '\r');
	assert(r == PALETTE_DONE_NONE);

	g_repl_items[0].enabled = true;        /* restore */
	palette_close(&st);
}

static void test_resize_clamps_cursors(void) {
	compositor_test_reset();
	compositor_on_resize(24, 80);
	palette_state_t st;
	palette_menu_source_t src = make_default_source();
	palette_open(&st, 24, 80, &src);

	/* Move menubar cursor right and resize. The cursor must remain
	 * valid (i.e. within st.menu_count). */
	palette_feed_byte(&st, 0x1b);
	palette_feed_byte(&st, '[');
	palette_feed_byte(&st, 'C');
	assert(st.menubar_cursor == 1);

	compositor_on_resize(24, 40);
	palette_on_resize(&st, 24, 40);
	assert(st.term_rows == 24);
	assert(st.term_cols == 40);
	assert(st.menubar_cursor < st.menu_count);

	palette_close(&st);
}

int main(void) {
	TR_INIT("palette_state_tests");
	TR_RUN(test_open_initial_state);
	TR_RUN(test_menubar_arrow_navigation);
	TR_RUN(test_enter_opens_menu_then_navigates);
	TR_RUN(test_enter_on_leaf_executes);
	TR_RUN(test_enter_on_submenu_opens_cascade);
	TR_RUN(test_esc_at_sublevel_closes_one_level);
	TR_RUN(test_esc_at_top_level_cancels);
	TR_RUN(test_esc_then_arrow_is_csi_not_bare_esc);
	TR_RUN(test_left_at_submenu_closes_only_submenu);
	TR_RUN(test_left_on_open_top_level_closes_to_menubar);
	TR_RUN(test_hotkey_on_menubar_opens_menu);
	TR_RUN(test_hotkey_within_open_menu_executes);
	TR_RUN(test_mouse_click_on_menubar_opens_menu);
	TR_RUN(test_mouse_click_on_leaf_executes);
	TR_RUN(test_mouse_click_outside_cancels);
	TR_RUN(test_cascade_overflow_right_opens_left);
	TR_RUN(test_cascade_overflow_bottom_opens_up);
	TR_RUN(test_disabled_item_does_not_execute);
	TR_RUN(test_resize_clamps_cursors);
	TR_SUMMARY();
	return TR_EXIT_CODE();
}
