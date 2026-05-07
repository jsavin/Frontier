/*
 * repl_slash_resolver_tests.c - Unit tests for the REPL slash-command
 * menubar resolver (frontier-cli/repl_slash_resolver.{c,h}).
 *
 * The resolver looks up a slash-command token (e.g. "exit", "exi", "x")
 * against the installed REPL menubar at system.menus.data.repl.REPL,
 * matching by exact label, prefix label, or hotkey first-letter. The
 * intent of PR 7: a user typing "/exit" reaches the same UserTalk handler
 * as a user opening the palette and selecting "Exit".
 *
 * Coverage:
 *   - Exact label match (case-insensitive)
 *   - Prefix match (unique prefix)
 *   - Prefix match (ambiguous -> nil)
 *   - Hotkey match via first letter of label
 *   - Empty token rejected
 *   - Over-long token rejected (>= 64 chars)
 *   - Unknown token returns false
 *   - Resolver returns false when REPL menubar is absent
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
#include "../frontier-cli/repl_slash_resolver.h"

/* ---------- helpers (shared idiom from repl_palette_source_tests) ---------- */

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

static void clear_data_children(void) {
	hdlhashtable hdata = nil;
	if (!find_data(&hdata))
		return;
	emptyhashtable(hdata, true);
}

/*
 * Build the REPL menubar shape that PR 7 dispatches against:
 *   system.menus.data.repl.REPL.{Help, Clear, List, Jump, Key codes, Exit}
 *
 * The menu rung is named "REPL" (single uppercase pull-down) and the leaf
 * names match the integration-test labels. Each leaf carries label + script
 * fields (matching what installReplMenubar.ut writes via menu.addMenuCommand).
 */
static void build_repl_menubar(void) {
	hdlhashtable hdata = nil;

	assert(menudata_ensure_root());
	clear_data_children();
	assert(find_data(&hdata));

	hdlhashtable hbar = mk_subtable(hdata, "repl");
	hdlhashtable hmenu = mk_subtable(hbar, "REPL");

	struct {
		const char *key;
		const char *label;
		const char *script;
	} items[] = {
		{ "Help",      "Help",      "system.menus.handlers.repl.help ()" },
		{ "Clear",     "Clear",     "system.menus.handlers.repl.clear ()" },
		{ "List",      "List",      "system.menus.handlers.repl.list ()" },
		{ "Jump",      "Jump",      "system.menus.handlers.repl.jump ()" },
		{ "Key codes", "Key codes", "system.menus.handlers.repl.keycodes ()" },
		{ "Exit",      "Exit",      "system.menus.handlers.repl.exit ()" },
	};
	const int n = (int)(sizeof(items) / sizeof(items[0]));
	for (int i = 0; i < n; i++) {
		hdlhashtable hleaf = mk_subtable(hmenu, items[i].key);
		set_string(hleaf, "label", items[i].label);
		set_string(hleaf, "script", items[i].script);
	}
}

/* Read back the leaf's label string for assertion. */
static void read_label(hdlhashtable hleaf, char *out, size_t outsz) {
	bigstring bskey;
	hdlhashnode hnode;
	tyvaluerecord val;

	out[0] = '\0';
	cstr_to_bs("label", bskey);
	if (!hashtablelookup(hleaf, bskey, &val, &hnode))
		return;
	if (val.valuetype != stringvaluetype)
		return;
	bigstring bs;
	texthandletostring(val.data.stringvalue, bs);
	size_t len = (size_t)stringlength(bs);
	if (len >= outsz) len = outsz - 1;
	memcpy(out, stringbaseaddress(bs), len);
	out[len] = '\0';
}

/* ---------- resolver tests ---------- */

static void test_exact_label_match(void) {
	printf("[resolver] Test: exact label match (case-insensitive)... ");
	fflush(stdout);

	build_repl_menubar();

	hdlhashtable hleaf = nil;
	assert(repl_resolve_slash_command("exit", &hleaf, NULL, 0));
	assert(hleaf != nil);
	char label[64];
	read_label(hleaf, label, sizeof(label));
	assert(strcmp(label, "Exit") == 0);

	/* Mixed-case input still resolves to the same leaf. */
	hleaf = nil;
	assert(repl_resolve_slash_command("ExIt", &hleaf, NULL, 0));
	read_label(hleaf, label, sizeof(label));
	assert(strcmp(label, "Exit") == 0);

	clear_data_children();
	printf("PASS\n");
	fflush(stdout);
}

static void test_unique_prefix_match(void) {
	printf("[resolver] Test: unique prefix match... ");
	fflush(stdout);

	build_repl_menubar();

	/* "exi" is a unique prefix of "Exit" (no other label starts with E... */
	/* well actually "Exit" is the only E-leaf, so "exi" -> "Exit"). */
	hdlhashtable hleaf = nil;
	assert(repl_resolve_slash_command("exi", &hleaf, NULL, 0));
	char label[64];
	read_label(hleaf, label, sizeof(label));
	assert(strcmp(label, "Exit") == 0);

	clear_data_children();
	printf("PASS\n");
	fflush(stdout);
}

static void test_ambiguous_prefix_fails(void) {
	printf("[resolver] Test: ambiguous prefix returns false... ");
	fflush(stdout);

	/*
	 * Build a menubar where TWO canonical leaves carry labels that share
	 * a "Cl" prefix so the resolver has to refuse rather than guess. Use
	 * canonical slot keys "Clear" + "Help" but mutate Help's label to
	 * "Closeup" — both pass the canonical-slot-key allowlist, and their
	 * labels collide on prefix "cl" / "clo".
	 *
	 * This shape isn't realistic in production (the install script writes
	 * label = slot key for both) but it exercises the ambiguity path
	 * without depending on non-canonical slot keys, which the allowlist
	 * now rejects outright.
	 */
	hdlhashtable hdata = nil;
	assert(menudata_ensure_root());
	clear_data_children();
	assert(find_data(&hdata));

	hdlhashtable hbar = mk_subtable(hdata, "repl");
	hdlhashtable hmenu = mk_subtable(hbar, "REPL");
	hdlhashtable hclear = mk_subtable(hmenu, "Clear");
	set_string(hclear, "label", "Clear");
	hdlhashtable hhelp = mk_subtable(hmenu, "Help");
	set_string(hhelp, "label", "Closeup");

	hdlhashtable hleaf = nil;
	assert(!repl_resolve_slash_command("cl", &hleaf, NULL, 0));
	assert(hleaf == nil);

	/* "cle" is unique to Clear -> resolves. */
	assert(repl_resolve_slash_command("cle", &hleaf, NULL, 0));
	char label[64];
	read_label(hleaf, label, sizeof(label));
	assert(strcmp(label, "Clear") == 0);

	clear_data_children();
	printf("PASS\n");
	fflush(stdout);
}

static void test_hotkey_first_letter_match(void) {
	printf("[resolver] Test: single-letter token matches by first letter... ");
	fflush(stdout);

	build_repl_menubar();

	/* "x" matches "Exit" only via first-letter (no label starts with X). */
	/* But our menubar uses "Exit" first letter = 'E'. There's no leaf */
	/* starting with X. So "x" should NOT match in the standard menubar. */
	hdlhashtable hleaf = nil;
	assert(!repl_resolve_slash_command("x", &hleaf, NULL, 0));

	/* "h" is a unique first-letter for "Help" -> resolves. */
	hleaf = nil;
	assert(repl_resolve_slash_command("h", &hleaf, NULL, 0));
	char label[64];
	read_label(hleaf, label, sizeof(label));
	assert(strcmp(label, "Help") == 0);

	/* "e" is unique to "Exit" -> resolves. */
	hleaf = nil;
	assert(repl_resolve_slash_command("e", &hleaf, NULL, 0));
	read_label(hleaf, label, sizeof(label));
	assert(strcmp(label, "Exit") == 0);

	clear_data_children();
	printf("PASS\n");
	fflush(stdout);
}

static void test_ambiguous_first_letter_fails(void) {
	printf("[resolver] Test: ambiguous single-letter token returns false... ");
	fflush(stdout);

	/*
	 * Two canonical leaves with labels both starting with C ("Clear" and
	 * "Help" with label "Closeup"). Token "c" must not resolve to either
	 * — caller has to disambiguate. Slot keys are canonical so the
	 * allowlist does not pre-filter.
	 */
	hdlhashtable hdata = nil;
	assert(menudata_ensure_root());
	clear_data_children();
	assert(find_data(&hdata));

	hdlhashtable hbar = mk_subtable(hdata, "repl");
	hdlhashtable hmenu = mk_subtable(hbar, "REPL");
	hdlhashtable hclear = mk_subtable(hmenu, "Clear");
	set_string(hclear, "label", "Clear");
	hdlhashtable hhelp = mk_subtable(hmenu, "Help");
	set_string(hhelp, "label", "Closeup");

	hdlhashtable hleaf = nil;
	assert(!repl_resolve_slash_command("c", &hleaf, NULL, 0));
	assert(hleaf == nil);

	clear_data_children();
	printf("PASS\n");
	fflush(stdout);
}

static void test_empty_token_rejected(void) {
	printf("[resolver] Test: empty token rejected... ");
	fflush(stdout);

	build_repl_menubar();

	hdlhashtable hleaf = nil;
	assert(!repl_resolve_slash_command("", &hleaf, NULL, 0));
	assert(hleaf == nil);

	assert(!repl_resolve_slash_command(NULL, &hleaf, NULL, 0));
	assert(hleaf == nil);

	clear_data_children();
	printf("PASS\n");
	fflush(stdout);
}

static void test_overlong_token_rejected(void) {
	printf("[resolver] Test: token >= 64 chars rejected... ");
	fflush(stdout);

	build_repl_menubar();

	char big[128];
	memset(big, 'x', sizeof(big));
	big[64] = '\0'; /* exactly 64 chars + NUL: rejected (>= 64) */

	hdlhashtable hleaf = nil;
	assert(!repl_resolve_slash_command(big, &hleaf, NULL, 0));
	assert(hleaf == nil);

	/* 63 chars + NUL: still rejected (no match), but not because of length */
	big[63] = '\0';
	hleaf = nil;
	assert(!repl_resolve_slash_command(big, &hleaf, NULL, 0));
	/* legitimate "no match" — different code path, still false */

	clear_data_children();
	printf("PASS\n");
	fflush(stdout);
}

static void test_unknown_token_returns_false(void) {
	printf("[resolver] Test: unknown token returns false... ");
	fflush(stdout);

	build_repl_menubar();

	hdlhashtable hleaf = nil;
	assert(!repl_resolve_slash_command("nonsense", &hleaf, NULL, 0));
	assert(hleaf == nil);

	clear_data_children();
	printf("PASS\n");
	fflush(stdout);
}

static void test_no_menubar_returns_false(void) {
	printf("[resolver] Test: missing menubar returns false (no crash)... ");
	fflush(stdout);

	clear_data_children(); /* no system.menus.data.repl */

	hdlhashtable hleaf = nil;
	assert(!repl_resolve_slash_command("exit", &hleaf, NULL, 0));
	assert(hleaf == nil);

	printf("PASS\n");
	fflush(stdout);
}

static void test_two_word_label(void) {
	printf("[resolver] Test: two-word labels match collapsed token... ");
	fflush(stdout);

	build_repl_menubar();

	/* "Key codes" should be reachable as "keycodes" (spaces collapsed). */
	hdlhashtable hleaf = nil;
	assert(repl_resolve_slash_command("keycodes", &hleaf, NULL, 0));
	char label[64];
	read_label(hleaf, label, sizeof(label));
	assert(strcmp(label, "Key codes") == 0);

	clear_data_children();
	printf("PASS\n");
	fflush(stdout);
}

static void test_out_name_returns_slot_key(void) {
	printf("[resolver] Test: out_name buffer returns leaf slot key... ");
	fflush(stdout);

	build_repl_menubar();

	/* Resolve "list" — should match leaf slot key "List". */
	hdlhashtable hleaf = nil;
	char name[64];
	memset(name, 0xAA, sizeof(name));
	assert(repl_resolve_slash_command("list", &hleaf, name, sizeof(name)));
	assert(strcmp(name, "List") == 0);

	/* Resolve via prefix — out_name still gets the slot key. */
	hleaf = nil;
	memset(name, 0xAA, sizeof(name));
	assert(repl_resolve_slash_command("ju", &hleaf, name, sizeof(name)));
	assert(strcmp(name, "Jump") == 0);

	/* Resolve via single-letter — out_name still gets the slot key. */
	hleaf = nil;
	memset(name, 0xAA, sizeof(name));
	assert(repl_resolve_slash_command("e", &hleaf, name, sizeof(name)));
	assert(strcmp(name, "Exit") == 0);

	/* Two-word slot key "Key codes" preserved verbatim. */
	hleaf = nil;
	memset(name, 0xAA, sizeof(name));
	assert(repl_resolve_slash_command("keycodes", &hleaf, name, sizeof(name)));
	assert(strcmp(name, "Key codes") == 0);

	/* On failure, out_name is cleared so callers can pass it through. */
	hleaf = nil;
	memset(name, 0xAA, sizeof(name));
	assert(!repl_resolve_slash_command("nonsense", &hleaf, name, sizeof(name)));
	assert(name[0] == '\0');

	clear_data_children();
	printf("PASS\n");
	fflush(stdout);
}

static void test_non_canonical_slot_key_rejected(void) {
	printf("[resolver] Test: non-canonical slot key rejected (allowlist)... ");
	fflush(stdout);

	/*
	 * Plant an "EvilExit" leaf alongside the canonical "Exit". Both labels
	 * start with E, so without the allowlist the resolver might either
	 * (a) resolve "/e" ambiguously to false, or (b) resolve "/evilexit"
	 * exactly to the EvilExit leaf and dispatch the attacker's script
	 * via the Exit slot-key special case.
	 *
	 * With the allowlist, EvilExit is filtered out before any tier check.
	 * "/e" and "/exit" both resolve to canonical Exit (slot key "Exit"),
	 * and "/evilexit" matches no canonical leaf (no canonical label
	 * collapses to "evilexit") so it returns false.
	 */
	hdlhashtable hdata = nil;
	assert(menudata_ensure_root());
	clear_data_children();
	assert(find_data(&hdata));

	hdlhashtable hbar = mk_subtable(hdata, "repl");
	hdlhashtable hmenu = mk_subtable(hbar, "REPL");

	hdlhashtable hexit = mk_subtable(hmenu, "Exit");
	set_string(hexit, "label", "Exit");
	set_string(hexit, "script", "system.menus.handlers.repl.exit ()");

	/* Hostile: non-canonical slot key whose label collides with canonical. */
	hdlhashtable hevil = mk_subtable(hmenu, "Evil");
	set_string(hevil, "label", "EvilExit");
	set_string(hevil, "script", "shell.run (\"rm -rf /\")");

	/* "/e" resolves to canonical Exit (Evil is filtered, so no first-letter
	 * ambiguity). */
	hdlhashtable hleaf = nil;
	char name[64];
	memset(name, 0xAA, sizeof(name));
	assert(repl_resolve_slash_command("e", &hleaf, name, sizeof(name)));
	assert(strcmp(name, "Exit") == 0);

	/* "/exit" resolves to canonical Exit by exact match. */
	hleaf = nil;
	memset(name, 0xAA, sizeof(name));
	assert(repl_resolve_slash_command("exit", &hleaf, name, sizeof(name)));
	assert(strcmp(name, "Exit") == 0);

	/* "/evilexit" must not resolve — the only label that would match
	 * sits behind a non-canonical slot key. */
	hleaf = nil;
	memset(name, 0xAA, sizeof(name));
	assert(!repl_resolve_slash_command("evilexit", &hleaf, name, sizeof(name)));
	assert(hleaf == nil);
	assert(name[0] == '\0');

	/* Direct slot-key "/evil" must not resolve either. */
	hleaf = nil;
	memset(name, 0xAA, sizeof(name));
	assert(!repl_resolve_slash_command("evil", &hleaf, name, sizeof(name)));
	assert(hleaf == nil);
	assert(name[0] == '\0');

	clear_data_children();
	printf("PASS\n");
	fflush(stdout);
}

/* ---------- main ---------- */

int main(void) {
	TR_INIT("repl_slash_resolver_tests");

	printf("\n=== REPL Slash-Command Resolver Tests ===\n");
	fflush(stdout);

	log_init();

	assert(initmemory());
	initstrings();
	assert(initlang());
	assert(inittablestructure());
	assert(langinitresources_headless());
	assert(langinitverbs());
	assert(wp_portable_init());

	TR_RUN(test_exact_label_match);
	TR_RUN(test_unique_prefix_match);
	TR_RUN(test_ambiguous_prefix_fails);
	TR_RUN(test_hotkey_first_letter_match);
	TR_RUN(test_ambiguous_first_letter_fails);
	TR_RUN(test_empty_token_rejected);
	TR_RUN(test_overlong_token_rejected);
	TR_RUN(test_unknown_token_returns_false);
	TR_RUN(test_no_menubar_returns_false);
	TR_RUN(test_two_word_label);
	TR_RUN(test_out_name_returns_slot_key);
	TR_RUN(test_non_canonical_slot_key_rejected);

	printf("\n=========================================\n");
	printf("[resolver] ALL TESTS PASSED\n");
	printf("=========================================\n");
	fflush(stdout);

	wp_portable_shutdown();

	TR_SUMMARY();
	return TR_EXIT_CODE();
}
