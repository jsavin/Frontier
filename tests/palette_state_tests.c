/*
 * palette_state_tests.c - Behavioural unit tests for the REPL palette
 * state machine (palette.{c,h}) after the host-anchored horizontal
 * cascade rework.
 *
 * The renderer is now a stack of full-width single-row strips
 * (menubar + one strip per open cascade level), anchored to
 * prompt_row+1 (i.e. directly below the REPL prompt) instead of
 * row 0.  Type-to-select-and-activate replaces the legacy filter:
 * any hotkey letter at any level activates the matched item.
 *
 * Coverage (post-rework):
 *   - open / close, prompt_row anchoring, scroll-up edge case
 *   - menubar arrow navigation, ENTER opens a menu
 *   - ENTER on a leaf executes; ENTER on a submenu opens cascade
 *   - LEFT/RIGHT in cascade move the cursor along the strip
 *   - DOWN in cascade drills into submenu; UP collapses
 *   - ESC at top cancels, at submenu collapses one level
 *   - depth cap refuses extra open
 *   - mouse click on menubar / on cascade item
 *   - mouse wheel moves the cursor
 *   - hotkey activation (menubar AND cascade)
 *   - disabled items do not execute
 *   - resize repositions / clamps cursors
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
#include <unistd.h>

#include "pane.h"
#include "palette.h"
#include "test_report.h"

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
	out->opaque = it;
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

/* ---------- Sample fixture ---------- */

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
	{ "Views",   NULL, 'V', true, true,  NULL, g_view_children, 2 },
	{ "Open",    NULL, 'O', true, false, "file.open()", NULL, 0 },
};

static fake_menu_t g_menus[] = {
	{ "REPL", 'R', g_repl_items, 3 },
	{ "File", 'F', g_file_items, 3 },
};

static fake_source_t g_src = { g_menus, 2 };

static void reset_fixture(void) {
	for (size_t i = 0; i < sizeof(g_repl_items) / sizeof(g_repl_items[0]); ++i) {
		g_repl_items[i].enabled = true;
	}
	for (size_t i = 0; i < sizeof(g_file_items) / sizeof(g_file_items[0]); ++i) {
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
	bool ok = palette_open(&st, 24, 80, 0, &src);
	assert(ok);
	assert(st.active);
	assert(st.menu_count == 2);
	assert(st.menubar_cursor == 0);
	assert(st.open_depth == 0);
	/* Menubar is anchored at prompt_row + 1 = 1 with prompt_row=0. */
	assert(st.menubar.y == 1);
	assert(st.menubar.h == 1);
	assert(st.menubar.x == 0);
	assert(st.menubar.w == 80);

	palette_close(&st);
	assert(!st.active);
}

static void test_palette_open_anchors_below_prompt_row(void) {
	compositor_test_reset();
	compositor_on_resize(24, 80);
	palette_state_t st;
	palette_menu_source_t src = make_default_source();
	bool ok = palette_open(&st, 24, 80, 10, &src);
	assert(ok);
	assert(st.menubar.y == 11);

	palette_close(&st);
}

static void test_palette_open_scrolls_when_prompt_at_bottom(void) {
	/* prompt at the bottom-most row (23 in 24-row term) leaves no room
	 * for the menubar below it.  palette_open scrolls the terminal up
	 * by 1 and anchors the menubar to the freed row. */
	compositor_test_reset();
	compositor_on_resize(24, 80);
	palette_state_t st;
	palette_menu_source_t src = make_default_source();
	/* Suppress the \x1b[1S scroll-up that palette_open writes to
	 * stderr -- it would otherwise leak into the test transcript. */
	fflush(stderr);
	int saved_err = dup(fileno(stderr));
	FILE *devnull = freopen("/dev/null", "w", stderr);
	(void)devnull;
	bool ok = palette_open(&st, 24, 80, 23, &src);
	fflush(stderr);
	if (saved_err >= 0) {
		dup2(saved_err, fileno(stderr));
		close(saved_err);
		clearerr(stderr);
	}
	assert(ok);
	/* prompt_row decremented to 22, menubar at 23. */
	assert(st.prompt_row == 22);
	assert(st.menubar.y == 23);

	palette_close(&st);
}

static void test_menubar_arrow_navigation(void) {
	compositor_test_reset();
	compositor_on_resize(24, 80);
	palette_state_t st;
	palette_menu_source_t src = make_default_source();
	palette_open(&st, 24, 80, 0, &src);

	/* RIGHT moves menubar cursor. */
	palette_feed_byte(&st, 0x1b);
	palette_feed_byte(&st, '[');
	palette_feed_byte(&st, 'C');
	assert(st.menubar_cursor == 1);

	/* LEFT moves back. */
	palette_feed_byte(&st, 0x1b);
	palette_feed_byte(&st, '[');
	palette_feed_byte(&st, 'D');
	assert(st.menubar_cursor == 0);

	/* LEFT at 0 clamps. */
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
	palette_open(&st, 24, 80, 0, &src);

	palette_done_t r = palette_feed_byte(&st, '\r');
	assert(r == PALETTE_DONE_NONE);
	assert(st.open_depth == 1);
	assert(st.levels[0].cursor == 0);
	assert(st.levels[0].item_count == 3);
	/* The cascade strip is one row below the menubar. */
	assert(st.levels[0].pane.y == st.menubar.y + 1);
	assert(st.levels[0].pane.h == 1);
	assert(st.levels[0].pane.w == 80);

	/* RIGHT moves the cascade cursor (horizontal model). */
	palette_feed_byte(&st, 0x1b);
	palette_feed_byte(&st, '[');
	palette_feed_byte(&st, 'C');
	assert(st.levels[0].cursor == 1);

	/* RIGHT past last clamps. */
	for (int i = 0; i < 5; ++i) {
		palette_feed_byte(&st, 0x1b);
		palette_feed_byte(&st, '[');
		palette_feed_byte(&st, 'C');
	}
	assert(st.levels[0].cursor == 2);

	/* LEFT moves back. */
	palette_feed_byte(&st, 0x1b);
	palette_feed_byte(&st, '[');
	palette_feed_byte(&st, 'D');
	assert(st.levels[0].cursor == 1);

	palette_close(&st);
}

static void test_enter_on_leaf_executes(void) {
	compositor_test_reset();
	compositor_on_resize(24, 80);
	palette_state_t st;
	palette_menu_source_t src = make_default_source();
	palette_open(&st, 24, 80, 0, &src);

	palette_feed_byte(&st, '\r');
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
	palette_open(&st, 24, 80, 0, &src);

	/* Move to File menu. */
	palette_feed_byte(&st, 0x1b); palette_feed_byte(&st, '['); palette_feed_byte(&st, 'C');
	palette_feed_byte(&st, '\r');
	assert(st.open_depth == 1);
	/* RIGHT to "Views" (item index 1). */
	palette_feed_byte(&st, 0x1b); palette_feed_byte(&st, '['); palette_feed_byte(&st, 'C');
	assert(st.levels[0].cursor == 1);
	assert(st.levels[0].items[1].is_submenu);

	/* ENTER opens the cascade strip. */
	palette_done_t r = palette_feed_byte(&st, '\r');
	assert(r == PALETTE_DONE_NONE);
	assert(st.open_depth == 2);
	assert(st.levels[1].item_count == 2);
	assert(strcmp(st.levels[1].items[0].label, "Outline") == 0);

	/* The submenu strip is the next row down. */
	assert(st.levels[1].pane.y == st.levels[0].pane.y + 1);
	assert(st.levels[1].pane.w == 80);
	assert(st.levels[1].pane.h == 1);

	palette_close(&st);
}

static void test_down_on_submenu_drills(void) {
	compositor_test_reset();
	compositor_on_resize(24, 80);
	palette_state_t st;
	palette_menu_source_t src = make_default_source();
	palette_open(&st, 24, 80, 0, &src);

	/* File menu, Views item via two RIGHTs and an ENTER. */
	palette_feed_byte(&st, 0x1b); palette_feed_byte(&st, '['); palette_feed_byte(&st, 'C');
	palette_feed_byte(&st, '\r');
	palette_feed_byte(&st, 0x1b); palette_feed_byte(&st, '['); palette_feed_byte(&st, 'C');
	assert(st.levels[0].cursor == 1);

	/* DOWN on submenu item drills. */
	palette_feed_byte(&st, 0x1b);
	palette_feed_byte(&st, '[');
	palette_feed_byte(&st, 'B');
	assert(st.open_depth == 2);

	palette_close(&st);
}

static void test_up_collapses_deepest_level(void) {
	compositor_test_reset();
	compositor_on_resize(24, 80);
	palette_state_t st;
	palette_menu_source_t src = make_default_source();
	palette_open(&st, 24, 80, 0, &src);

	/* Open File -> Views cascade. */
	palette_feed_byte(&st, 0x1b); palette_feed_byte(&st, '['); palette_feed_byte(&st, 'C');
	palette_feed_byte(&st, '\r');
	palette_feed_byte(&st, 0x1b); palette_feed_byte(&st, '['); palette_feed_byte(&st, 'C');
	palette_feed_byte(&st, '\r');
	assert(st.open_depth == 2);

	/* UP collapses. */
	palette_feed_byte(&st, 0x1b);
	palette_feed_byte(&st, '[');
	palette_feed_byte(&st, 'A');
	assert(st.open_depth == 1);

	palette_close(&st);
}

static void test_esc_at_sublevel_closes_one_level(void) {
	compositor_test_reset();
	compositor_on_resize(24, 80);
	palette_state_t st;
	palette_menu_source_t src = make_default_source();
	palette_open(&st, 24, 80, 0, &src);

	palette_feed_byte(&st, 0x1b); palette_feed_byte(&st, '['); palette_feed_byte(&st, 'C');
	palette_feed_byte(&st, '\r');
	palette_feed_byte(&st, 0x1b); palette_feed_byte(&st, '['); palette_feed_byte(&st, 'C');
	palette_feed_byte(&st, '\r');
	assert(st.open_depth == 2);

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
	palette_open(&st, 24, 80, 0, &src);

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
	palette_open(&st, 24, 80, 0, &src);

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

/* ---------- Hotkey activation (Phase D) ---------- */

static void test_menubar_hotkey_activates_immediately(void) {
	/* Pressing 'F' on the menubar (no menu open) must both select File
	 * AND open it in one byte -- the type-to-activate behaviour. */
	compositor_test_reset();
	compositor_on_resize(24, 80);
	palette_state_t st;
	palette_menu_source_t src = make_default_source();
	palette_open(&st, 24, 80, 0, &src);

	palette_done_t r = palette_feed_byte(&st, 'F');
	assert(r == PALETTE_DONE_NONE);
	assert(st.menubar_cursor == 1);
	assert(st.open_depth == 1);
	assert(st.levels[0].menu_index == 1);

	palette_close(&st);
}

static void test_cascade_hotkey_activates_leaf(void) {
	/* Inside an open cascade, pressing a hotkey letter that matches a
	 * leaf item dispatches it in one byte (no separate ENTER needed). */
	compositor_test_reset();
	compositor_on_resize(24, 80);
	palette_state_t st;
	palette_menu_source_t src = make_default_source();
	palette_open(&st, 24, 80, 0, &src);

	palette_feed_byte(&st, '\r');           /* open REPL menu */
	assert(st.open_depth == 1);

	/* 'X' is the Exit hotkey -- should both select and execute. */
	palette_done_t r = palette_feed_byte(&st, 'X');
	assert(r == PALETTE_DONE_EXECUTE);
	assert(st.exec_script != NULL);
	assert(strcmp((const char *)st.exec_script, "repl.exit()") == 0);

	palette_close(&st);
}

static void test_cascade_hotkey_opens_submenu(void) {
	/* In an open cascade, pressing a hotkey that matches a submenu
	 * item opens that submenu (drills one level deeper). */
	compositor_test_reset();
	compositor_on_resize(24, 80);
	palette_state_t st;
	palette_menu_source_t src = make_default_source();
	palette_open(&st, 24, 80, 0, &src);

	/* Move to File and open it via hotkey. */
	palette_done_t r = palette_feed_byte(&st, 'F');
	assert(r == PALETTE_DONE_NONE);
	assert(st.open_depth == 1);

	/* 'V' is the Views submenu hotkey. */
	r = palette_feed_byte(&st, 'V');
	assert(r == PALETTE_DONE_NONE);
	assert(st.open_depth == 2);
	assert(st.levels[1].item_count == 2);

	palette_close(&st);
}

/* ---------- Hotkey auto-derivation (regression for production bug) ---------- */

/*
 * The ODB-backed source (repl_palette_source.c) returns shortcut='\0'
 * for any item without an explicit cmdkey field — which is almost every
 * item in the headless REPL menubar. The previous Phase D tests above
 * passed because their fixtures set `shortcut` explicitly, masking the
 * fact that runtime type-to-activate was broken end-to-end.
 *
 * These tests pin the auto-derivation contract: when a sibling list has
 * no explicit shortcuts, the palette MUST derive them from the labels
 * (first available uppercase alpha letter, claiming-as-it-goes) so
 * find_hotkey_in_level returns hits at runtime.
 */
static fake_item_t g_noshortcut_items[] = {
	{ "Help",     NULL, '\0', true, false, "h",  NULL, 0 },
	{ "Clear",    NULL, '\0', true, false, "c",  NULL, 0 },
	{ "List",     NULL, '\0', true, false, "l",  NULL, 0 },
	{ "Jump",     NULL, '\0', true, false, "j",  NULL, 0 },
	{ "Key codes", NULL, '\0', true, false, "k", NULL, 0 },
	{ "Exit",     NULL, '\0', true, false, "x",  NULL, 0 },
};

static fake_menu_t g_noshortcut_menus[] = {
	{ "REPL", '\0', g_noshortcut_items, 6 },
};

static fake_source_t g_noshortcut_src = { g_noshortcut_menus, 1 };

static void test_menubar_hotkey_autoderived_when_source_silent(void) {
	/* The source provides no menu hotkey ('\0'). The palette MUST derive
	 * 'R' from "REPL" so typing 'R' on the menubar opens that menu. */
	compositor_test_reset();
	compositor_on_resize(24, 80);
	palette_state_t st;
	palette_menu_source_t src = make_source(&g_noshortcut_src);
	bool ok = palette_open(&st, 24, 80, 0, &src);
	assert(ok);
	assert(st.menu_hotkeys[0] == 'R');

	palette_done_t r = palette_feed_byte(&st, 'R');
	assert(r == PALETTE_DONE_NONE);
	assert(st.menubar_cursor == 0);
	assert(st.open_depth == 1);

	palette_close(&st);
}

static void test_cascade_items_hotkey_autoderived_when_source_silent(void) {
	/* The items have no explicit shortcuts. After opening REPL, every
	 * item.shortcut must hold a derived uppercase letter that matches
	 * the per-layer auto-derivation rule: first unclaimed letter of the
	 * label, walking siblings left to right. For "Help, Clear, List,
	 * Jump, Key codes, Exit" the expected derivation is H,C,L,J,K,E
	 * (each label's first letter is free at the time of derivation). */
	compositor_test_reset();
	compositor_on_resize(24, 80);
	palette_state_t st;
	palette_menu_source_t src = make_source(&g_noshortcut_src);
	palette_open(&st, 24, 80, 0, &src);

	palette_feed_byte(&st, '\r');           /* open REPL menu */
	assert(st.open_depth == 1);
	const palette_level_t *lvl = &st.levels[0];
	assert(lvl->item_count == 6);
	assert(lvl->items[0].shortcut == 'H');
	assert(lvl->items[1].shortcut == 'C');
	assert(lvl->items[2].shortcut == 'L');
	assert(lvl->items[3].shortcut == 'J');
	assert(lvl->items[4].shortcut == 'K');
	assert(lvl->items[5].shortcut == 'E');

	/* End-to-end: 'H' must dispatch the Help leaf. */
	palette_done_t r = palette_feed_byte(&st, 'H');
	assert(r == PALETTE_DONE_EXECUTE);
	assert(st.exec_script != NULL);
	assert(strcmp((const char *)st.exec_script, "h") == 0);

	palette_close(&st);
}

static fake_item_t g_collide_items[] = {
	/* Both labels start with 'F' — second one must skip to 'I'. */
	{ "File",  NULL, '\0', true, false, "f",  NULL, 0 },
	{ "Find",  NULL, '\0', true, false, "i",  NULL, 0 },
};

static fake_menu_t g_collide_menus[] = {
	{ "Top", '\0', g_collide_items, 2 },
};

static fake_source_t g_collide_src = { g_collide_menus, 1 };

static void test_hotkey_autoderive_handles_collision(void) {
	/* "File" claims 'F'; "Find" must skip to 'I' (next unclaimed alpha
	 * in "Find"). */
	compositor_test_reset();
	compositor_on_resize(24, 80);
	palette_state_t st;
	palette_menu_source_t src = make_source(&g_collide_src);
	palette_open(&st, 24, 80, 0, &src);
	palette_feed_byte(&st, '\r');           /* open Top menu */
	assert(st.open_depth == 1);
	assert(st.levels[0].items[0].shortcut == 'F');
	assert(st.levels[0].items[1].shortcut == 'I');
	palette_close(&st);
}

/* ---------- Mouse ---------- */

static void test_mouse_click_on_menubar_opens_menu(void) {
	compositor_test_reset();
	compositor_on_resize(24, 80);
	palette_state_t st;
	palette_menu_source_t src = make_default_source();
	palette_open(&st, 24, 80, 0, &src);

	int target_x = st.menu_x[1] + 1;     /* 1-based */
	int target_y = st.menubar.y + 1;     /* 1-based */
	mouse_event_t ev = { MOUSE_LEFT, target_x, target_y, true };
	palette_done_t r = palette_feed_mouse(&st, &ev);
	assert(r == PALETTE_DONE_NONE);
	assert(st.menubar_cursor == 1);
	assert(st.open_depth == 1);

	palette_close(&st);
}

static void test_mouse_wheel_moves_menubar_cursor(void) {
	compositor_test_reset();
	compositor_on_resize(24, 80);
	palette_state_t st;
	palette_menu_source_t src = make_default_source();
	palette_open(&st, 24, 80, 0, &src);

	mouse_event_t wd = { MOUSE_WHEEL_DOWN, 1, 1, true };
	palette_feed_mouse(&st, &wd);
	assert(st.menubar_cursor == 1);

	mouse_event_t wu = { MOUSE_WHEEL_UP, 1, 1, true };
	palette_feed_mouse(&st, &wu);
	assert(st.menubar_cursor == 0);

	/* Wheel-up at 0 clamps. */
	palette_feed_mouse(&st, &wu);
	assert(st.menubar_cursor == 0);

	palette_close(&st);
}

static void test_mouse_invalid_low_coords_rejected(void) {
	compositor_test_reset();
	compositor_on_resize(24, 80);
	palette_state_t st;
	palette_menu_source_t src = make_default_source();
	palette_open(&st, 24, 80, 0, &src);

	mouse_event_t ev0 = { MOUSE_LEFT, 0, 1, true };
	palette_done_t r = palette_feed_mouse(&st, &ev0);
	assert(r == PALETTE_DONE_NONE);
	assert(st.open_depth == 0);

	mouse_event_t ev1 = { MOUSE_LEFT, 1, 0, true };
	r = palette_feed_mouse(&st, &ev1);
	assert(r == PALETTE_DONE_NONE);
	assert(st.open_depth == 0);

	palette_close(&st);
}

/* ---------- Disabled items, depth cap, resize, ESC mid-CSI ---------- */

static void test_disabled_item_does_not_execute(void) {
	compositor_test_reset();
	compositor_on_resize(24, 80);
	palette_state_t st;
	g_repl_items[0].enabled = false;
	palette_menu_source_t src = make_source(&g_src);
	palette_open(&st, 24, 80, 0, &src);

	palette_feed_byte(&st, '\r');
	palette_done_t r = palette_feed_byte(&st, '\r');
	assert(r == PALETTE_DONE_NONE);

	g_repl_items[0].enabled = true;
	palette_close(&st);
}

static void test_esc_mid_csi_aborts_sequence(void) {
	compositor_test_reset();
	compositor_on_resize(24, 80);
	palette_state_t st;
	palette_menu_source_t src = make_default_source();
	palette_open(&st, 24, 80, 0, &src);

	palette_feed_byte(&st, 0x1b);
	palette_feed_byte(&st, '[');
	palette_feed_byte(&st, '5');
	assert(st.csi_len > 0);

	palette_done_t r = palette_feed_byte(&st, 0x1b);
	assert(r == PALETTE_DONE_NONE);
	assert(st.csi_len == 0);
	assert(st.esc_pending);

	r = palette_feed_esc_timeout(&st);
	assert(r == PALETTE_DONE_CANCEL);

	palette_close(&st);
}

static void test_resize_clamps_menubar_cursor(void) {
	compositor_test_reset();
	compositor_on_resize(24, 80);
	palette_state_t st;
	palette_menu_source_t src = make_default_source();
	palette_open(&st, 24, 80, 0, &src);

	palette_feed_byte(&st, 0x1b); palette_feed_byte(&st, '['); palette_feed_byte(&st, 'C');
	assert(st.menubar_cursor == 1);

	compositor_on_resize(24, 40);
	palette_on_resize(&st, 24, 40);
	assert(st.term_rows == 24);
	assert(st.term_cols == 40);
	assert(st.menubar_cursor < st.menu_count);

	palette_close(&st);
}

/* ---------- Recursive synthetic source for depth-cap test ---------- */

#define MAX_GEN_DEPTH 12

static int recur_count_menus(void *ctx) { (void)ctx; return 1; }

static bool recur_menu_describe(void *ctx, int idx, char *out_label,
                                size_t cap, char *out_hotkey) {
	(void)ctx;
	if (idx != 0) return false;
	snprintf(out_label, cap, "Deep");
	*out_hotkey = 'D';
	return true;
}

static intptr_t recur_depth_from_opaque(void *p) { return ((intptr_t)p) - 1; }
static void *recur_opaque_for_depth(intptr_t d) { return (void *)(intptr_t)(d + 1); }

static int recur_item_count(void *ctx, int menu_index, void *parent_opaque) {
	(void)ctx; (void)menu_index;
	intptr_t d = parent_opaque ? recur_depth_from_opaque(parent_opaque) : 0;
	if (d > MAX_GEN_DEPTH) return 0;
	return 2;
}

static char g_recur_labels[MAX_GEN_DEPTH + 1][4][16];
static const char *g_recur_scripts[MAX_GEN_DEPTH + 1][4] = {{0}};

static bool recur_item_describe(void *ctx, int menu_index, void *parent_opaque,
                                int item_index, palette_item_t *out) {
	(void)ctx; (void)menu_index;
	intptr_t d = parent_opaque ? recur_depth_from_opaque(parent_opaque) : 0;
	if (d > MAX_GEN_DEPTH) return false;
	if (item_index < 0 || item_index >= 2) return false;
	memset(out, 0, sizeof(*out));
	int slot_d = (int)(d <= MAX_GEN_DEPTH ? d : MAX_GEN_DEPTH);
	int slot_i = item_index < 4 ? item_index : 0;
	snprintf(g_recur_labels[slot_d][slot_i],
	         sizeof(g_recur_labels[slot_d][slot_i]),
	         "L%lldI%d", (long long)d, item_index);
	snprintf(out->label, sizeof(out->label), "%s",
	         g_recur_labels[slot_d][slot_i]);
	out->shortcut = '\0';
	out->enabled = true;
	out->hidden = false;
	bool deeper_available = (d + 1) <= MAX_GEN_DEPTH;
	if (item_index == 0 && deeper_available) {
		out->is_submenu = true;
		out->opaque = recur_opaque_for_depth(d + 1);
		out->script_handle = NULL;
	} else {
		out->is_submenu = false;
		out->opaque = NULL;
		g_recur_scripts[slot_d][slot_i] = "leaf.run()";
		out->script_handle = (void *)g_recur_scripts[slot_d][slot_i];
	}
	return true;
}

static palette_menu_source_t make_recursive_source(void) {
	palette_menu_source_t s;
	memset(&s, 0, sizeof(s));
	s.count_menus = recur_count_menus;
	s.menu_describe = recur_menu_describe;
	s.item_count = recur_item_count;
	s.item_describe = recur_item_describe;
	return s;
}

static void test_depth_cap_refuses_extra_open(void) {
	compositor_test_reset();
	compositor_on_resize(80, 200);
	palette_state_t st;
	palette_menu_source_t src = make_recursive_source();
	bool ok = palette_open(&st, 80, 200, 0, &src);
	assert(ok);

	palette_done_t r = palette_feed_byte(&st, '\r');
	assert(r == PALETTE_DONE_NONE);
	assert(st.open_depth == 1);
	for (int i = 1; i < PALETTE_MAX_DEPTH; ++i) {
		r = palette_feed_byte(&st, '\r');
		assert(r == PALETTE_DONE_NONE);
		assert(st.open_depth == i + 1);
	}
	assert(st.open_depth == PALETTE_MAX_DEPTH);

	int depth_before = st.open_depth;
	r = palette_feed_byte(&st, '\r');
	assert(r == PALETTE_DONE_NONE);
	assert(st.open_depth == depth_before);

	palette_close(&st);
}

static void test_fast_timers_env_var_sets_short_esc_timeout(void) {
	const char *saved = getenv("FRONTIER_PALETTE_FAST_TIMERS");
	char saved_copy[64];
	bool had = false;
	if (saved) {
		had = true;
		strncpy(saved_copy, saved, sizeof(saved_copy) - 1);
		saved_copy[sizeof(saved_copy) - 1] = '\0';
	}

	unsetenv("FRONTIER_PALETTE_FAST_TIMERS");
	assert(palette_esc_timeout_ms() == 10);

	setenv("FRONTIER_PALETTE_FAST_TIMERS", "1", 1);
	assert(palette_esc_timeout_ms() == 1);

	setenv("FRONTIER_PALETTE_FAST_TIMERS", "", 1);
	assert(palette_esc_timeout_ms() == 10);

	setenv("FRONTIER_PALETTE_FAST_TIMERS", "yes", 1);
	assert(palette_esc_timeout_ms() == 1);

	if (had) setenv("FRONTIER_PALETTE_FAST_TIMERS", saved_copy, 1);
	else unsetenv("FRONTIER_PALETTE_FAST_TIMERS");
}

int main(void) {
	TR_INIT("palette_state_tests");
	TR_RUN(test_open_initial_state);
	TR_RUN(test_palette_open_anchors_below_prompt_row);
	TR_RUN(test_palette_open_scrolls_when_prompt_at_bottom);
	TR_RUN(test_menubar_arrow_navigation);
	TR_RUN(test_enter_opens_menu_then_navigates);
	TR_RUN(test_enter_on_leaf_executes);
	TR_RUN(test_enter_on_submenu_opens_cascade);
	TR_RUN(test_down_on_submenu_drills);
	TR_RUN(test_up_collapses_deepest_level);
	TR_RUN(test_esc_at_sublevel_closes_one_level);
	TR_RUN(test_esc_at_top_level_cancels);
	TR_RUN(test_esc_then_arrow_is_csi_not_bare_esc);
	TR_RUN(test_menubar_hotkey_activates_immediately);
	TR_RUN(test_cascade_hotkey_activates_leaf);
	TR_RUN(test_cascade_hotkey_opens_submenu);
	TR_RUN(test_menubar_hotkey_autoderived_when_source_silent);
	TR_RUN(test_cascade_items_hotkey_autoderived_when_source_silent);
	TR_RUN(test_hotkey_autoderive_handles_collision);
	TR_RUN(test_mouse_click_on_menubar_opens_menu);
	TR_RUN(test_mouse_wheel_moves_menubar_cursor);
	TR_RUN(test_mouse_invalid_low_coords_rejected);
	TR_RUN(test_disabled_item_does_not_execute);
	TR_RUN(test_esc_mid_csi_aborts_sequence);
	TR_RUN(test_resize_clamps_menubar_cursor);
	TR_RUN(test_depth_cap_refuses_extra_open);
	TR_RUN(test_fast_timers_env_var_sets_short_esc_timeout);
	TR_SUMMARY();
	return TR_EXIT_CODE();
}
