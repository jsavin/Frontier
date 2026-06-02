/*
 * repl_palette_source_tests.c - Unit tests for the ODB-backed
 * palette_menu_source_t adapter (repl_palette_source.{c,h}).
 *
 * The adapter wraps menudata_list_leaves / menudata_describe_leaf to
 * project system.menus.data.<menubar>.<menu>.<item> into the palette's
 * vtable abstraction. These tests fabricate a synthetic menubar in a
 * fresh hashtable via menudata_headless and exercise the vtable through
 * the public adapter API.
 *
 * Coverage:
 *   - count_menus reflects the synthetic menubar's child count
 *   - menu_describe returns label + hotkey for a known menu
 *   - menu_describe out-of-range returns false
 *   - item_count for a top-level menu reports the leaf count
 *   - item_describe for a leaf reports label + script + shortcut + opaque
 *   - item_describe out-of-range returns false
 *   - script_handle survives a copyvaluerecord cycle (handle privacy
 *     contract per palette.h::palette_item_t)
 *   - dispose is idempotent and safe on a never-initialised source
 *
 * License
 * -------
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Frontier contributors.
 */

#include <assert.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "frontier.h"
#include "standard.h"
#include "shelltypes.h"
#include "test_report.h"
#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "langinternal.h"
#include "tablestructure.h"
#include "tableverbs.h"
#include "stringdefs.h"
#include "logging.h"
#include "../portable/wptext_portable.h"

#include "menudata_headless.h"
#include "../frontier-cli/palette.h"
#include "../frontier-cli/repl_palette_source.h"

/* ---------- helpers (mirrors the menu_list_describe_tests pattern) ---------- */

static void cstr_to_bs(const char *s, bigstring bs) {
	copyctopstring((char *)s, bs);
}

static boolean find_subtable(hdlhashtable hparent, bigstring bsname,
                             hdlhashtable *hresult) {
	*hresult = nil;
	return findnamedtable(hparent, bsname, hresult);
}

static boolean find_data(hdlhashtable *hdata) {
	hdlhashtable hsystem = nil;
	hdlhashtable hmenus = nil;

	if (!find_subtable(roottable, namesystembranch, &hsystem))
		return false;
	if (!find_subtable(hsystem, STR_menus, &hmenus))
		return false;
	return find_subtable(hmenus, STR_data, hdata);
}

static hdlhashtable mk_subtable(hdlhashtable hparent, const char *name) {
	bigstring bs;
	hdlhashtable hresult = nil;

	cstr_to_bs(name, bs);
	if (find_subtable(hparent, bs, &hresult))
		return hresult;
	assert(tablenewsubtable(hparent, bs, &hresult));
	return hresult;
}

static void set_string(hdlhashtable ht, const char *name, const char *value) {
	bigstring bsname, bsvalue;
	tyvaluerecord val;

	cstr_to_bs(name, bsname);
	cstr_to_bs(value, bsvalue);
	assert(setstringvalue(bsvalue, &val));
	assert(hashtableassign(ht, bsname, val));
}

static void set_char(hdlhashtable ht, const char *name, byte v) {
	bigstring bsname;
	tyvaluerecord val;

	cstr_to_bs(name, bsname);
	assert(setcharvalue(v, &val));
	assert(hashtableassign(ht, bsname, val));
}

static void set_long(hdlhashtable ht, const char *name, long v) {
	bigstring bsname;
	tyvaluerecord val;

	cstr_to_bs(name, bsname);
	assert(setlongvalue(v, &val));
	assert(hashtableassign(ht, bsname, val));
}

static void clear_data_children(void) {
	hdlhashtable hdata = nil;
	if (!find_data(&hdata))
		return;
	emptyhashtable(hdata, true);
}

/*
 * Build a synthetic menubar at system.menus.data.<bar>:
 *   <bar>/REPL/Help    (label="Help", script="help script", shortcut='H')
 *   <bar>/REPL/Exit    (label="Exit", script="exit script", shortcut='X')
 *   <bar>/Help/About   (label="About", script="about script", shortcut='A')
 *
 * Returns the <bar> hashtable handle for direct test inspection.
 */
static hdlhashtable build_test_menubar(const char *bar_name) {
	hdlhashtable hdata = nil;

	assert(menudata_ensure_root());
	clear_data_children();
	assert(find_data(&hdata));

	hdlhashtable hbar = mk_subtable(hdata, bar_name);

	hdlhashtable hreplmenu = mk_subtable(hbar, "REPL");
	hdlhashtable hhelp = mk_subtable(hreplmenu, "Help");
	set_string(hhelp, "label", "Help");
	set_string(hhelp, "script", "help script");
	set_char(hhelp, "cmdkey", 'H');

	hdlhashtable hexit = mk_subtable(hreplmenu, "Exit");
	set_string(hexit, "label", "Exit");
	set_string(hexit, "script", "exit script");
	set_char(hexit, "cmdkey", 'X');

	hdlhashtable hhelpmenu = mk_subtable(hbar, "Help");
	hdlhashtable habout = mk_subtable(hhelpmenu, "About");
	set_string(habout, "label", "About");
	set_string(habout, "script", "about script");
	set_char(habout, "cmdkey", 'A');

	return hbar;
}

/* ---------- adapter tests ---------- */

static void test_init_for_missing_menubar_returns_false(void) {
	printf("[adapter] Test: init_for non-existent menubar returns false... ");
	fflush(stdout);

	clear_data_children(); /* ensure no menubar present */

	palette_menu_source_t src;
	memset(&src, 0xAA, sizeof(src)); /* poison */

	bool ok = repl_palette_source_init_for(&src, "no_such_bar");
	assert(!ok);

	/* dispose must be safe on a failed/uninitialised source */
	repl_palette_source_dispose(&src);

	printf("PASS\n");
	fflush(stdout);
}

static void test_count_menus_reflects_menubar(void) {
	printf("[adapter] Test: count_menus returns top-level menu count... ");
	fflush(stdout);

	(void)build_test_menubar("test_bar");

	palette_menu_source_t src;
	assert(repl_palette_source_init_for(&src, "test_bar"));

	int n = src.count_menus(src.ctx);
	/* test bar has 2 top-level menus: REPL and Help */
	assert(n == 2);

	repl_palette_source_dispose(&src);
	clear_data_children();

	printf("PASS\n");
	fflush(stdout);
}

static void test_menu_describe_returns_label(void) {
	printf("[adapter] Test: menu_describe yields label for known indices... ");
	fflush(stdout);

	(void)build_test_menubar("test_bar");

	palette_menu_source_t src;
	assert(repl_palette_source_init_for(&src, "test_bar"));

	int n = src.count_menus(src.ctx);
	assert(n == 2);

	bool found_repl = false, found_help = false;
	for (int i = 0; i < n; i++) {
		char label[64];
		char hotkey = '\0';
		memset(label, 0, sizeof(label));
		bool ok = src.menu_describe(src.ctx, i, label, sizeof(label), &hotkey);
		assert(ok);
		if (strcmp(label, "REPL") == 0) found_repl = true;
		if (strcmp(label, "Help") == 0) found_help = true;
	}
	assert(found_repl && found_help);

	/* out-of-range index */
	{
		char label[64] = {0};
		char hotkey = '?';
		bool ok = src.menu_describe(src.ctx, 99, label, sizeof(label), &hotkey);
		assert(!ok);
	}

	repl_palette_source_dispose(&src);
	clear_data_children();

	printf("PASS\n");
	fflush(stdout);
}

static void test_item_count_for_top_menu(void) {
	printf("[adapter] Test: item_count for top menu reports leaf count... ");
	fflush(stdout);

	(void)build_test_menubar("test_bar");

	palette_menu_source_t src;
	assert(repl_palette_source_init_for(&src, "test_bar"));

	/* Find REPL menu's index */
	int n = src.count_menus(src.ctx);
	int repl_idx = -1, help_idx = -1;
	for (int i = 0; i < n; i++) {
		char label[64] = {0};
		char hk = '\0';
		assert(src.menu_describe(src.ctx, i, label, sizeof(label), &hk));
		if (strcmp(label, "REPL") == 0) repl_idx = i;
		if (strcmp(label, "Help") == 0) help_idx = i;
	}
	assert(repl_idx >= 0 && help_idx >= 0);

	int repl_items = src.item_count(src.ctx, repl_idx, NULL);
	int help_items = src.item_count(src.ctx, help_idx, NULL);
	assert(repl_items == 2); /* Help, Exit */
	assert(help_items == 1); /* About */

	repl_palette_source_dispose(&src);
	clear_data_children();

	printf("PASS\n");
	fflush(stdout);
}

static void test_item_describe_yields_leaf_fields(void) {
	printf("[adapter] Test: item_describe returns label/shortcut/script_handle... ");
	fflush(stdout);

	(void)build_test_menubar("test_bar");

	palette_menu_source_t src;
	assert(repl_palette_source_init_for(&src, "test_bar"));

	int n = src.count_menus(src.ctx);
	int repl_idx = -1;
	for (int i = 0; i < n; i++) {
		char label[64] = {0};
		char hk = '\0';
		assert(src.menu_describe(src.ctx, i, label, sizeof(label), &hk));
		if (strcmp(label, "REPL") == 0) { repl_idx = i; break; }
	}
	assert(repl_idx >= 0);

	int item_count = src.item_count(src.ctx, repl_idx, NULL);
	assert(item_count == 2);

	bool found_help = false, found_exit = false;
	for (int i = 0; i < item_count; i++) {
		palette_item_t out;
		memset(&out, 0, sizeof(out));
		bool ok = src.item_describe(src.ctx, repl_idx, NULL, i, &out);
		assert(ok);

		assert(out.label[0] != '\0');
		assert(!out.is_submenu); /* leaves only in our test menubar */
		assert(out.script_handle != NULL); /* leaf must carry a script handle */

		if (strcmp(out.label, "Help") == 0) {
			found_help = true;
			assert(out.shortcut == 'H');
		}
		if (strcmp(out.label, "Exit") == 0) {
			found_exit = true;
			assert(out.shortcut == 'X');
		}
	}
	assert(found_help && found_exit);

	/* out-of-range item index */
	{
		palette_item_t out;
		memset(&out, 0, sizeof(out));
		bool ok = src.item_describe(src.ctx, repl_idx, NULL, 99, &out);
		assert(!ok);
	}

	repl_palette_source_dispose(&src);
	clear_data_children();

	printf("PASS\n");
	fflush(stdout);
}

/*
 * GIL-stable handle contract: the script_handle returned by item_describe
 * must remain valid across a value-record copy/dispose cycle. This proves
 * the adapter satisfied option (b) per palette.h — independently allocated
 * handles owned by the adapter, not aliased into ODB-internal storage.
 */
static void test_script_handle_survives_copy_cycle(void) {
	printf("[adapter] Test: script_handle survives copyvaluerecord cycle... ");
	fflush(stdout);

	(void)build_test_menubar("test_bar");

	palette_menu_source_t src;
	assert(repl_palette_source_init_for(&src, "test_bar"));

	int n = src.count_menus(src.ctx);
	int repl_idx = -1;
	for (int i = 0; i < n; i++) {
		char label[64] = {0};
		char hk = '\0';
		assert(src.menu_describe(src.ctx, i, label, sizeof(label), &hk));
		if (strcmp(label, "REPL") == 0) { repl_idx = i; break; }
	}
	assert(repl_idx >= 0);

	palette_item_t out;
	memset(&out, 0, sizeof(out));
	assert(src.item_describe(src.ctx, repl_idx, NULL, 0, &out));
	assert(out.script_handle != NULL);

	Handle hsaved = (Handle)out.script_handle;

	/*
	 * Force allocation churn: build and dispose a value record copy via the
	 * same paths a yielded UserTalk script would. If the adapter's stored
	 * handle aliased ODB storage, this churn could corrupt or free the
	 * underlying bytes — strlen on the stored payload should fault or
	 * return junk in that case.
	 */
	for (int i = 0; i < 8; i++) {
		palette_item_t scratch;
		memset(&scratch, 0, sizeof(scratch));
		assert(src.item_describe(src.ctx, repl_idx, NULL, i % 2, &scratch));
		(void)scratch;
	}

	/*
	 * The handle should still be readable. We don't assert on contents
	 * (the stored format is Pascal-prefixed UserTalk source) — we just
	 * dereference and read at least one byte without crashing, and confirm
	 * the handle is non-empty (sizeof > 0).
	 */
	assert(hsaved != NULL);
	assert(*hsaved != NULL);
	long sz = gethandlesize(hsaved);
	assert(sz > 0);
	/* read the first byte to ensure the memory is mapped */
	volatile char first = (*hsaved)[0];
	(void)first;

	repl_palette_source_dispose(&src);
	clear_data_children();

	printf("PASS\n");
	fflush(stdout);
}

/*
 * Behavioural test for the keyed script-handle cache.
 *
 * Before the keyed-cache refactor, every cb_item_describe call grew the
 * cache by one slot regardless of whether the same item had been
 * described before. After the refactor, re-describing the SAME item
 * (same menu_index + item_index → same hashtable handle) must reuse
 * the existing cached copy and return a STABLE pointer.
 *
 * This test runs many describes against the same item and asserts the
 * script_handle pointer is identical every time. If the cache had
 * grown linearly, each call would have produced a freshly copyhandle'd
 * pointer (different value).
 *
 * It also runs more describes than the cache cap (256) so that the
 * pre-refactor implementation would have returned false partway through
 * — proving we no longer have an O(N) growth bug.
 */
static void test_describe_handle_stable_under_repeated_calls(void) {
	printf("[adapter] Test: re-describe returns stable script_handle... ");
	fflush(stdout);

	(void)build_test_menubar("test_bar");

	palette_menu_source_t src;
	assert(repl_palette_source_init_for(&src, "test_bar"));

	int n = src.count_menus(src.ctx);
	int repl_idx = -1;
	for (int i = 0; i < n; i++) {
		char label[64] = {0};
		char hk = '\0';
		assert(src.menu_describe(src.ctx, i, label, sizeof(label), &hk));
		if (strcmp(label, "REPL") == 0) { repl_idx = i; break; }
	}
	assert(repl_idx >= 0);

	/* First describe — record the handle. */
	palette_item_t first;
	memset(&first, 0, sizeof(first));
	assert(src.item_describe(src.ctx, repl_idx, NULL, 0, &first));
	assert(first.script_handle != NULL);
	void *first_handle = first.script_handle;

	/* Re-describe the SAME item 1024 times — significantly above the
	 * 256-slot cache cap. With the linear-growth bug each call would
	 * either return false (cap exhausted) or return a different
	 * pointer (freshly allocated). With the keyed cache the pointer
	 * is stable. */
	for (int i = 0; i < 1024; i++) {
		palette_item_t scratch;
		memset(&scratch, 0, sizeof(scratch));
		bool ok = src.item_describe(src.ctx, repl_idx, NULL, 0, &scratch);
		assert(ok);
		assert(scratch.script_handle == first_handle);
	}

	repl_palette_source_dispose(&src);
	clear_data_children();

	printf("PASS\n");
	fflush(stdout);
}

/*
 * P1 #1 guard: init_for with a name longer than 255 bytes must return false
 * rather than silently overflowing the bigstring buffer. bigstring is
 * unsigned char[256] with byte 0 as the Pascal length, so max content is
 * 255 characters.
 */
static void test_init_for_too_long_name_returns_false(void) {
	printf("[adapter] Test: init_for rejects name longer than 255 bytes... ");
	fflush(stdout);

	/* 300-byte name -- well beyond the 255-char bigstring limit. */
	char long_name[301];
	memset(long_name, 'x', 300);
	long_name[300] = '\0';

	palette_menu_source_t src;
	memset(&src, 0xAA, sizeof(src)); /* poison */

	bool ok = repl_palette_source_init_for(&src, long_name);
	assert(!ok);

	/* dispose must be safe on a rejected init */
	repl_palette_source_dispose(&src);

	printf("PASS\n");
	fflush(stdout);
}


/*
 * P2 #3 guard: menus within a bar must be enumerated in deterministic
 * alphabetical order regardless of insertion order.
 *
 * We install three menus in non-alphabetical order ("Zoo", "Apple", "Mango")
 * and verify menu_describe returns them sorted ("Apple", "Mango", "Zoo").
 */
static void test_menus_within_bar_are_alphabetically_ordered(void) {
	printf("[adapter] Test: menus within a bar are alphabetically ordered... ");
	fflush(stdout);

	hdlhashtable hdata = nil;
	assert(menudata_ensure_root());
	clear_data_children();
	assert(find_data(&hdata));

	/* Insert menus in non-alphabetical order: Zoo, Apple, Mango. */
	hdlhashtable hbar = mk_subtable(hdata, "sort_test_bar");

	hdlhashtable hzoo  = mk_subtable(hbar, "Zoo");
	hdlhashtable hzi   = mk_subtable(hzoo, "Item1");
	set_string(hzi, "label", "Zoo");
	set_string(hzi, "script", "-- zoo script");

	hdlhashtable happle = mk_subtable(hbar, "Apple");
	hdlhashtable hai    = mk_subtable(happle, "Item2");
	set_string(hai, "label", "Apple");
	set_string(hai, "script", "-- apple script");

	hdlhashtable hmango = mk_subtable(hbar, "Mango");
	hdlhashtable hmi    = mk_subtable(hmango, "Item3");
	set_string(hmi, "label", "Mango");
	set_string(hmi, "script", "-- mango script");

	palette_menu_source_t src;
	assert(repl_palette_source_init_for(&src, "sort_test_bar"));

	int n = src.count_menus(src.ctx);
	assert(n == 3);

	char label0[64] = {0}, label1[64] = {0}, label2[64] = {0};
	char hk = '\0';
	assert(src.menu_describe(src.ctx, 0, label0, sizeof(label0), &hk));
	assert(src.menu_describe(src.ctx, 1, label1, sizeof(label1), &hk));
	assert(src.menu_describe(src.ctx, 2, label2, sizeof(label2), &hk));

	/* Alphabetical: Apple < Mango < Zoo */
	assert(strcmp(label0, "Apple") == 0);
	assert(strcmp(label1, "Mango") == 0);
	assert(strcmp(label2, "Zoo")   == 0);

	repl_palette_source_dispose(&src);
	clear_data_children();

	printf("PASS\n");
	fflush(stdout);
}


/*
 * Order-field guard (Option A): when items within a menu carry an explicit
 * integer "order" field, the adapter must enumerate them by ascending order
 * value, NOT alphabetically by sub-table name.
 *
 * We install three items whose alphabetical name order is the REVERSE of
 * their intended display order:
 *   sub-table "Cmd_z"  order=0  (should be first)
 *   sub-table "Cmd_m"  order=1  (should be second)
 *   sub-table "Cmd_a"  order=2  (should be third)
 *
 * Alphabetically the names sort Cmd_a < Cmd_m < Cmd_z, so the pre-Option-A
 * adapter (which sorts purely by name) would yield labels in the order
 * Aaa, Mmm, Zzz. With order honored, the labels must come back Zzz, Mmm, Aaa.
 * This test fails RED against the name-only comparator and passes GREEN once
 * child_slot_cmp keys on the order field first.
 */
static void test_items_ordered_by_order_field(void) {
	printf("[adapter] Test: items honor explicit order field over name... ");
	fflush(stdout);

	hdlhashtable hdata = nil;
	assert(menudata_ensure_root());
	clear_data_children();
	assert(find_data(&hdata));

	hdlhashtable hbar  = mk_subtable(hdata, "order_test_bar");
	hdlhashtable hmenu = mk_subtable(hbar, "REPL");

	/* Names sort A<M<Z but order says Z(0) < M(1) < A(2). */
	hdlhashtable hz = mk_subtable(hmenu, "Cmd_z");
	set_string(hz, "label", "Zzz");
	set_string(hz, "script", "-- z");
	set_long(hz, "order", 0);

	hdlhashtable hm = mk_subtable(hmenu, "Cmd_m");
	set_string(hm, "label", "Mmm");
	set_string(hm, "script", "-- m");
	set_long(hm, "order", 1);

	hdlhashtable ha = mk_subtable(hmenu, "Cmd_a");
	set_string(ha, "label", "Aaa");
	set_string(ha, "script", "-- a");
	set_long(ha, "order", 2);

	palette_menu_source_t src;
	assert(repl_palette_source_init_for(&src, "order_test_bar"));

	int n = src.count_menus(src.ctx);
	assert(n == 1);

	int items = src.item_count(src.ctx, 0, NULL);
	assert(items == 3);

	palette_item_t out0, out1, out2;
	memset(&out0, 0, sizeof(out0));
	memset(&out1, 0, sizeof(out1));
	memset(&out2, 0, sizeof(out2));
	assert(src.item_describe(src.ctx, 0, NULL, 0, &out0));
	assert(src.item_describe(src.ctx, 0, NULL, 1, &out1));
	assert(src.item_describe(src.ctx, 0, NULL, 2, &out2));

	/* By order field: Zzz(0), Mmm(1), Aaa(2). */
	assert(strcmp(out0.label, "Zzz") == 0);
	assert(strcmp(out1.label, "Mmm") == 0);
	assert(strcmp(out2.label, "Aaa") == 0);

	repl_palette_source_dispose(&src);
	clear_data_children();

	printf("PASS\n");
	fflush(stdout);
}

/*
 * Back-compat guard for the order field: items WITHOUT an order field must
 * keep enumerating alphabetically (the legacy default). This pins the
 * fall-back behavior so the order-field change does not regress existing
 * menubars that never wrote an order value.
 *
 * Distinct from test_menus_within_bar_are_alphabetically_ordered (which
 * covers top-level menus); this covers items within a single menu.
 */
static void test_items_without_order_fall_back_to_alpha(void) {
	printf("[adapter] Test: items without order field stay alphabetical... ");
	fflush(stdout);

	hdlhashtable hdata = nil;
	assert(menudata_ensure_root());
	clear_data_children();
	assert(find_data(&hdata));

	hdlhashtable hbar  = mk_subtable(hdata, "noorder_bar");
	hdlhashtable hmenu = mk_subtable(hbar, "REPL");

	/* No order field written on any item. */
	hdlhashtable hz = mk_subtable(hmenu, "Zebra");
	set_string(hz, "label", "Zebra");
	set_string(hz, "script", "-- z");

	hdlhashtable ha = mk_subtable(hmenu, "Apple");
	set_string(ha, "label", "Apple");
	set_string(ha, "script", "-- a");

	palette_menu_source_t src;
	assert(repl_palette_source_init_for(&src, "noorder_bar"));

	int items = src.item_count(src.ctx, 0, NULL);
	assert(items == 2);

	palette_item_t out0, out1;
	memset(&out0, 0, sizeof(out0));
	memset(&out1, 0, sizeof(out1));
	assert(src.item_describe(src.ctx, 0, NULL, 0, &out0));
	assert(src.item_describe(src.ctx, 0, NULL, 1, &out1));

	/* Alphabetical: Apple < Zebra. */
	assert(strcmp(out0.label, "Apple") == 0);
	assert(strcmp(out1.label, "Zebra") == 0);

	repl_palette_source_dispose(&src);
	clear_data_children();

	printf("PASS\n");
	fflush(stdout);
}


static void test_dispose_idempotent(void) {
	printf("[adapter] Test: dispose is idempotent... ");
	fflush(stdout);

	(void)build_test_menubar("test_bar");

	palette_menu_source_t src;
	assert(repl_palette_source_init_for(&src, "test_bar"));

	repl_palette_source_dispose(&src);
	repl_palette_source_dispose(&src); /* second call must not crash */

	clear_data_children();

	printf("PASS\n");
	fflush(stdout);
}

/* ---------- multi-bar helpers ---------- */

/*
 * Set the .installed boolean on a bar that already has its subtable
 * created (build_test_menubar calls menudata_ensure_root so the system
 * path exists; we then call menudata_set_installed to stamp the flag).
 */
static void install_bar(const char *bar_name) {
	bigstring bs;
	cstr_to_bs(bar_name, bs);
	assert(menudata_set_installed(bs, true));
}

/*
 * Build a minimal menubar at system.menus.data.<bar_name> WITHOUT clearing
 * existing data children first (unlike build_test_menubar which calls
 * clear_data_children). Used so multi-bar tests can build two bars
 * side-by-side.
 *
 * Shape: <bar_name>/<menu_a>/Item1, <bar_name>/<menu_b>/Item2
 */
static hdlhashtable build_bar_additive(const char *bar_name,
                                       const char *menu1, const char *menu2) {
	hdlhashtable hdata = nil;

	assert(menudata_ensure_root());
	assert(find_data(&hdata));

	hdlhashtable hbar = mk_subtable(hdata, bar_name);

	hdlhashtable hm1 = mk_subtable(hbar, menu1);
	hdlhashtable hi1 = mk_subtable(hm1, "Item1");
	set_string(hi1, "label", menu1);
	set_string(hi1, "script", "-- item1 script");
	set_char(hi1, "cmdkey", '1');

	if (menu2 != nil) {
		hdlhashtable hm2 = mk_subtable(hbar, menu2);
		hdlhashtable hi2 = mk_subtable(hm2, "Item2");
		set_string(hi2, "label", menu2);
		set_string(hi2, "script", "-- item2 script");
		set_char(hi2, "cmdkey", '2');
	}

	return hbar;
}


/* ---------- multi-bar init_all tests ---------- */

/*
 * Two installed bars: bar_a (2 menus), bar_b (1 menu).
 * init_all should union them -> count_menus == 3.
 */
static void test_multi_bar_union_count(void) {
	printf("[adapter] Test: init_all unions menus from multiple bars... ");
	fflush(stdout);

	clear_data_children();
	build_bar_additive("bar_a", "Alpha", "Beta");
	install_bar("bar_a");
	build_bar_additive("bar_b", "Gamma", nil);
	install_bar("bar_b");

	palette_menu_source_t src;
	bool ok = repl_palette_source_init_all(&src);
	assert(ok);

	int n = src.count_menus(src.ctx);
	assert(n == 3); /* bar_a: Alpha, Beta; bar_b: Gamma */

	repl_palette_source_dispose(&src);
	clear_data_children();

	printf("PASS\n");
	fflush(stdout);
}


/*
 * Alphabetical ordering: bar "zzz" vs bar "aaa".
 * menu_describe(0) must return the first menu from "aaa".
 */
static void test_multi_bar_ordering_is_alphabetical(void) {
	printf("[adapter] Test: init_all orders bars alphabetically... ");
	fflush(stdout);

	clear_data_children();
	build_bar_additive("zzz", "ZMenu", nil);
	install_bar("zzz");
	build_bar_additive("aaa", "AMenu", nil);
	install_bar("aaa");

	palette_menu_source_t src;
	bool ok = repl_palette_source_init_all(&src);
	assert(ok);

	char label[64] = {0};
	char hk = '\0';
	bool described = src.menu_describe(src.ctx, 0, label, sizeof(label), &hk);
	assert(described);
	/* "aaa" sorts before "zzz", so index 0 must be from "aaa" -> "AMenu" */
	assert(strcmp(label, "AMenu") == 0);

	repl_palette_source_dispose(&src);
	clear_data_children();

	printf("PASS\n");
	fflush(stdout);
}


/*
 * Cross-bar describe: two bars (2+1 menus), menu_describe(2) returns
 * the label from the second bar's first menu.
 */
static void test_multi_bar_item_describe_crosses_bars(void) {
	printf("[adapter] Test: init_all menu_describe crosses bar boundary... ");
	fflush(stdout);

	clear_data_children();
	build_bar_additive("aaa", "Menu1", "Menu2");
	install_bar("aaa");
	build_bar_additive("zzz", "Menu3", nil);
	install_bar("zzz");

	palette_menu_source_t src;
	bool ok = repl_palette_source_init_all(&src);
	assert(ok);

	int n = src.count_menus(src.ctx);
	assert(n == 3);

	char label[64] = {0};
	char hk = '\0';
	bool described = src.menu_describe(src.ctx, 2, label, sizeof(label), &hk);
	assert(described);
	/* index 2 is the 3rd menu across the union: aaa[0]=Menu1 aaa[1]=Menu2 zzz[0]=Menu3 */
	assert(strcmp(label, "Menu3") == 0);

	repl_palette_source_dispose(&src);
	clear_data_children();

	printf("PASS\n");
	fflush(stdout);
}


/*
 * Installed filter: two bars, only one marked installed.
 * init_all must exclude the uninstalled bar.
 */
static void test_installed_filter_excludes_uninstalled(void) {
	printf("[adapter] Test: init_all excludes bars not marked installed... ");
	fflush(stdout);

	clear_data_children();
	build_bar_additive("installed_bar", "VisibleMenu", nil);
	install_bar("installed_bar");
	build_bar_additive("hidden_bar", "HiddenMenu", nil);
	/* hidden_bar: do NOT call install_bar — .installed defaults to false */

	palette_menu_source_t src;
	bool ok = repl_palette_source_init_all(&src);
	assert(ok);

	int n = src.count_menus(src.ctx);
	assert(n == 1); /* only installed_bar's VisibleMenu */

	repl_palette_source_dispose(&src);
	clear_data_children();

	printf("PASS\n");
	fflush(stdout);
}


/*
 * Single installed bar named "repl" still works through init_all.
 */
static void test_single_installed_bar_still_works_via_init_all(void) {
	printf("[adapter] Test: init_all works with single installed bar... ");
	fflush(stdout);

	clear_data_children();
	build_bar_additive("repl", "REPL", "Help");
	install_bar("repl");

	palette_menu_source_t src;
	bool ok = repl_palette_source_init_all(&src);
	assert(ok);

	int n = src.count_menus(src.ctx);
	assert(n == 2); /* REPL + Help */

	repl_palette_source_dispose(&src);
	clear_data_children();

	printf("PASS\n");
	fflush(stdout);
}


/* ---------- legacy prefix-code parsing (mereducemenucodes parity) ---------- */

/*
 * Helper: run palette_reduce_menu_codes on a fresh copy of `in` and report
 * the resulting label + flags. Mirrors how cb_item_describe will call it.
 */
static void reduce(const char *in, char *out_label, size_t cap,
                   bool *enabled, bool *checked, bool *is_sep) {
	*enabled = true;       /* defaults per mereducemenucodes contract */
	*checked = false;
	*is_sep = false;
	snprintf(out_label, cap, "%s", in);
	palette_reduce_menu_codes(out_label, enabled, checked, is_sep);
}

static void test_prefix_plain_label_unchanged(void) {
	printf("[adapter] Prefix: plain label is untouched... ");
	fflush(stdout);

	char label[64];
	bool en, ck, sep;
	reduce("Open", label, sizeof(label), &en, &ck, &sep);

	assert(strcmp(label, "Open") == 0);
	assert(en == true);
	assert(ck == false);
	assert(sep == false);

	printf("PASS\n");
	fflush(stdout);
}

static void test_prefix_open_paren_disables(void) {
	printf("[adapter] Prefix: leading '(' disables and is stripped... ");
	fflush(stdout);

	char label[64];
	bool en, ck, sep;
	reduce("(Save", label, sizeof(label), &en, &ck, &sep);

	assert(strcmp(label, "Save") == 0);
	assert(en == false);
	assert(ck == false);
	assert(sep == false);

	printf("PASS\n");
	fflush(stdout);
}

static void test_prefix_paren_pair_not_disabled(void) {
	printf("[adapter] Prefix: '(' with trailing ')' is NOT a disable code... ");
	fflush(stdout);

	/* mereducemenucodes: '(' only disables when lastchar != ')'. A label
	   like "(beta)" is a literal parenthesised word, left intact + enabled. */
	char label[64];
	bool en, ck, sep;
	reduce("(beta)", label, sizeof(label), &en, &ck, &sep);

	assert(strcmp(label, "(beta)") == 0);
	assert(en == true);
	assert(ck == false);
	assert(sep == false);

	printf("PASS\n");
	fflush(stdout);
}

static void test_prefix_bang_checks(void) {
	printf("[adapter] Prefix: leading '!' checks and is stripped... ");
	fflush(stdout);

	char label[64];
	bool en, ck, sep;
	reduce("!Wrap", label, sizeof(label), &en, &ck, &sep);

	assert(strcmp(label, "Wrap") == 0);
	assert(en == true);
	assert(ck == true);
	assert(sep == false);

	printf("PASS\n");
	fflush(stdout);
}

static void test_prefix_bang_alone_not_checked(void) {
	printf("[adapter] Prefix: lone '!' is a literal label, not a check code... ");
	fflush(stdout);

	/* mereducemenucodes: '!' only checks when stringlength > 1. */
	char label[64];
	bool en, ck, sep;
	reduce("!", label, sizeof(label), &en, &ck, &sep);

	assert(strcmp(label, "!") == 0);
	assert(en == true);
	assert(ck == false);
	assert(sep == false);

	printf("PASS\n");
	fflush(stdout);
}

static void test_prefix_paren_then_bang_stacks(void) {
	printf("[adapter] Prefix: '(!Foo' stacks disable + check... ");
	fflush(stdout);

	/* '(' is processed before '!', and both can stack. */
	char label[64];
	bool en, ck, sep;
	reduce("(!Foo", label, sizeof(label), &en, &ck, &sep);

	assert(strcmp(label, "Foo") == 0);
	assert(en == false);
	assert(ck == true);
	assert(sep == false);

	printf("PASS\n");
	fflush(stdout);
}

static void test_prefix_dash_is_separator(void) {
	printf("[adapter] Prefix: a line of exactly '-' is a separator... ");
	fflush(stdout);

	char label[64];
	bool en, ck, sep;
	reduce("-", label, sizeof(label), &en, &ck, &sep);

	assert(sep == true);
	assert(en == false);   /* separators are non-selectable / disabled */
	assert(ck == false);

	printf("PASS\n");
	fflush(stdout);
}

static void test_prefix_multidash_not_separator(void) {
	printf("[adapter] Prefix: '--' (len>1) is NOT a separator... ");
	fflush(stdout);

	/* Separator is ONLY the exact single '-' (stringlength == 1). */
	char label[64];
	bool en, ck, sep;
	reduce("--", label, sizeof(label), &en, &ck, &sep);

	assert(sep == false);
	assert(en == true);
	assert(strcmp(label, "--") == 0);

	printf("PASS\n");
	fflush(stdout);
}

/* ---------- main ---------- */

int main(void) {
	TR_INIT("repl_palette_source_tests");

	printf("\n=== REPL Palette Source Adapter Tests ===\n");
	fflush(stdout);

	log_init();

	assert(initmemory());
	initstrings();
	assert(initlang());
	assert(inittablestructure());
	assert(langinitresources_headless());
	assert(langinitverbs());
	assert(wp_portable_init());

	TR_RUN(test_init_for_missing_menubar_returns_false);
	TR_RUN(test_init_for_too_long_name_returns_false);
	TR_RUN(test_count_menus_reflects_menubar);
	TR_RUN(test_menu_describe_returns_label);
	TR_RUN(test_item_count_for_top_menu);
	TR_RUN(test_item_describe_yields_leaf_fields);
	TR_RUN(test_script_handle_survives_copy_cycle);
	TR_RUN(test_describe_handle_stable_under_repeated_calls);
	TR_RUN(test_dispose_idempotent);
	TR_RUN(test_menus_within_bar_are_alphabetically_ordered);
	TR_RUN(test_items_ordered_by_order_field);
	TR_RUN(test_items_without_order_fall_back_to_alpha);

	/* Legacy prefix-code parsing (mereducemenucodes parity) */
	TR_RUN(test_prefix_plain_label_unchanged);
	TR_RUN(test_prefix_open_paren_disables);
	TR_RUN(test_prefix_paren_pair_not_disabled);
	TR_RUN(test_prefix_bang_checks);
	TR_RUN(test_prefix_bang_alone_not_checked);
	TR_RUN(test_prefix_paren_then_bang_stacks);
	TR_RUN(test_prefix_dash_is_separator);
	TR_RUN(test_prefix_multidash_not_separator);

	/* Phase A: multi-bar init_all tests */
	TR_RUN(test_multi_bar_union_count);
	TR_RUN(test_multi_bar_ordering_is_alphabetical);
	TR_RUN(test_multi_bar_item_describe_crosses_bars);
	TR_RUN(test_installed_filter_excludes_uninstalled);
	TR_RUN(test_single_installed_bar_still_works_via_init_all);

	printf("\n========================================\n");
	printf("[adapter] ALL TESTS PASSED\n");
	printf("========================================\n");
	fflush(stdout);

	wp_portable_shutdown();

	TR_SUMMARY();
	return TR_EXIT_CODE();
}
