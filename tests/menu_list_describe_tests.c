/*	$Id$    */

/*
    SPDX-License-Identifier: MIT

    Copyright (c) 2026 Frontier contributors

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
*/

/*
 * menu_list_describe_tests.c - Unit tests for menudata_list_leaves() and
 *                              menudata_describe_leaf()
 *
 * These functions back the headless menu.list and menu.describe verbs. Both
 * walk the system.menus.data.<app>.<menu>.<item> chain established by
 * menudata_ensure_root() (PR 1, #575). A "leaf" is a sub-table whose children
 * are all scalars (label, script, cmdkey, ...); intermediate sub-tables (the
 * app and menu rungs) are walked through but not themselves returned.
 *
 * See planning/architectural_decision_records/ADR-016-headless-menu-system-projection.md
 * for the projection model and planning/discussions/pr2-meuserselected-headless-plan.md
 * for sub-PR 2a scope.
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "frontier.h"
#include "standard.h"
#include "shelltypes.h"
#include "test_report.h"
#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "langinternal.h"
#include "langsystem7.h"
#include "tablestructure.h"
#include "tableverbs.h"
#include "stringdefs.h"
#include "logging.h"
#include "oplist.h"
#include "../portable/wptext_portable.h"

#include "menudata_headless.h"


/* ---------- helpers ---------- */

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

/* Make a Pascal bigstring from a C string. */
static void cstr_to_bs(const char *s, bigstring bs) {
	copyctopstring((char *)s, bs);
}

/* Create (or look up) a sub-table beneath hparent with the given C name. */
static hdlhashtable mk_subtable(hdlhashtable hparent, const char *name) {
	bigstring bs;
	hdlhashtable hresult = nil;

	cstr_to_bs(name, bs);
	if (find_subtable(hparent, bs, &hresult))
		return hresult;
	assert(tablenewsubtable(hparent, bs, &hresult));
	return hresult;
}

/* Assign a string scalar field. */
static void set_string(hdlhashtable ht, const char *name, const char *value) {
	bigstring bsname, bsvalue;
	tyvaluerecord val;

	cstr_to_bs(name, bsname);
	cstr_to_bs(value, bsvalue);
	assert(setstringvalue(bsvalue, &val));
	assert(hashtableassign(ht, bsname, val));
}

/* Assign a char scalar field. */
static void set_char(hdlhashtable ht, const char *name, byte v) {
	bigstring bsname;
	tyvaluerecord val;

	cstr_to_bs(name, bsname);
	assert(setcharvalue(v, &val));
	assert(hashtableassign(ht, bsname, val));
}

/* Wipe any existing children under system.menus.data so each test starts
   from a known state. */
static void clear_data_children(void) {
	hdlhashtable hdata = nil;
	if (!find_data(&hdata))
		return;
	emptyhashtable(hdata, true);
}

/* Read field value from a record-typed value, by C-string key. Asserts the
   key exists. */
static tyvaluerecord rec_field(tyvaluerecord rec, const char *key) {
	hdllistrecord hlist;
	long n, i;
	bigstring bskey, bsfound;
	tyvaluerecord val;

	assert(rec.valuetype == recordvaluetype);
	hlist = rec.data.recordvalue;

	cstr_to_bs(key, bskey);
	n = opcountlistitems(hlist);
	for (i = 1; i <= n; i++) {
		if (!getnthlistval(hlist, i, bsfound, &val))
			continue;
		if (equalstrings(bsfound, bskey))
			return val;
	}

	fprintf(stderr, "rec_field: key '%s' not found in record\n", key);
	abort();
}


/* ---------- menudata_list_leaves tests ---------- */

/*
 * Test 1: empty data tree returns an empty list (size 0).
 */
static void test_list_empty(void) {
	printf("[menu.list] Test 1: empty data tree -> empty list... ");
	fflush(stdout);

	tyvaluerecord v;

	assert(menudata_ensure_root());
	clear_data_children();

	assert(menudata_list_leaves(nil, &v));
	assert(v.valuetype == listvaluetype);
	assert(opcountlistitems(v.data.listvalue) == 0);

	disposevaluerecord(v, false);

	printf("PASS\n");
	fflush(stdout);
}

/*
 * Test 2: a single 3-deep leaf is enumerated as one address entry.
 */
static void test_list_single_leaf(void) {
	printf("[menu.list] Test 2: single leaf -> list of length 1... ");
	fflush(stdout);

	hdlhashtable hdata = nil;
	tyvaluerecord v;

	assert(menudata_ensure_root());
	clear_data_children();
	assert(find_data(&hdata));

	hdlhashtable happ  = mk_subtable(hdata, "AppA");
	hdlhashtable hmenu = mk_subtable(happ, "MenuA");
	hdlhashtable hitem = mk_subtable(hmenu, "ItemA");
	set_string(hitem, "label", "Item A");
	set_string(hitem, "script", "doStuff()");

	assert(menudata_list_leaves(nil, &v));
	assert(v.valuetype == listvaluetype);
	assert(opcountlistitems(v.data.listvalue) == 1);

	disposevaluerecord(v, false);
	clear_data_children();

	printf("PASS\n");
	fflush(stdout);
}

/*
 * Test 3: scoped enumeration returns only the requested subtree.
 */
static void test_list_scoped(void) {
	printf("[menu.list] Test 3: scoped enumeration filters subtree... ");
	fflush(stdout);

	hdlhashtable hdata = nil;
	tyvaluerecord v;

	assert(menudata_ensure_root());
	clear_data_children();
	assert(find_data(&hdata));

	/* App A: one leaf. */
	hdlhashtable happA  = mk_subtable(hdata, "AppA");
	hdlhashtable hmenuA = mk_subtable(happA, "MenuA");
	hdlhashtable hitemA = mk_subtable(hmenuA, "ItemA");
	set_string(hitemA, "label", "A");
	set_string(hitemA, "script", "scriptA()");

	/* App B: one leaf. */
	hdlhashtable happB  = mk_subtable(hdata, "AppB");
	hdlhashtable hmenuB = mk_subtable(happB, "MenuB");
	hdlhashtable hitemB = mk_subtable(hmenuB, "ItemB");
	set_string(hitemB, "label", "B");
	set_string(hitemB, "script", "scriptB()");

	assert(menudata_list_leaves(happA, &v));
	assert(v.valuetype == listvaluetype);
	assert(opcountlistitems(v.data.listvalue) == 1);

	disposevaluerecord(v, false);
	clear_data_children();

	printf("PASS\n");
	fflush(stdout);
}


/* ---------- menudata_describe_leaf tests ---------- */

/*
 * Test 4: stored script field is reflected in the record.
 */
static void test_describe_script(void) {
	printf("[menu.describe] Test 4: stored script is reported... ");
	fflush(stdout);

	hdlhashtable hdata = nil;
	tyvaluerecord rec;
	tyvaluerecord script;
	bigstring bsexpected;

	assert(menudata_ensure_root());
	clear_data_children();
	assert(find_data(&hdata));

	hdlhashtable happ  = mk_subtable(hdata, "AppD");
	hdlhashtable hmenu = mk_subtable(happ, "Menu");
	hdlhashtable hitem = mk_subtable(hmenu, "Item");
	set_string(hitem, "label", "L");
	set_string(hitem, "script", "the script");

	assert(menudata_describe_leaf(hitem, &rec));
	assert(rec.valuetype == recordvaluetype);

	script = rec_field(rec, "script");
	assert(script.valuetype == stringvaluetype);

	cstr_to_bs("the script", bsexpected);
	bigstring bsactual;
	pullstringvalue(&script, bsactual);
	assert(equalstrings(bsactual, bsexpected));

	disposevaluerecord(rec, false);
	clear_data_children();

	printf("PASS\n");
	fflush(stdout);
}

/*
 * Test 5: missing optional fields surface their documented defaults.
 *   enabled = true, hidden = false, shortcut = "", accepts_args = false
 */
static void test_describe_defaults(void) {
	printf("[menu.describe] Test 5: missing fields use documented defaults... ");
	fflush(stdout);

	hdlhashtable hdata = nil;
	tyvaluerecord rec;

	assert(menudata_ensure_root());
	clear_data_children();
	assert(find_data(&hdata));

	hdlhashtable happ  = mk_subtable(hdata, "AppD2");
	hdlhashtable hmenu = mk_subtable(happ, "M");
	hdlhashtable hitem = mk_subtable(hmenu, "I");
	set_string(hitem, "label", "Bare");
	set_string(hitem, "script", "x()");

	assert(menudata_describe_leaf(hitem, &rec));
	assert(rec.valuetype == recordvaluetype);

	tyvaluerecord enabled = rec_field(rec, "enabled");
	assert(enabled.valuetype == booleanvaluetype);
	assert(enabled.data.flvalue == true);

	tyvaluerecord hidden = rec_field(rec, "hidden");
	assert(hidden.valuetype == booleanvaluetype);
	assert(hidden.data.flvalue == false);

	tyvaluerecord shortcut = rec_field(rec, "shortcut");
	assert(shortcut.valuetype == stringvaluetype);
	bigstring bsempty, bsshort;
	cstr_to_bs("", bsempty);
	pullstringvalue(&shortcut, bsshort);
	assert(equalstrings(bsshort, bsempty));

	tyvaluerecord acceptsargs = rec_field(rec, "accepts_args");
	assert(acceptsargs.valuetype == booleanvaluetype);
	assert(acceptsargs.data.flvalue == false);

	disposevaluerecord(rec, false);
	clear_data_children();

	printf("PASS\n");
	fflush(stdout);
}

/*
 * Test 6: explicitly-set cmdkey is reported.
 */
static void test_describe_cmdkey(void) {
	printf("[menu.describe] Test 6: cmdkey field is reported... ");
	fflush(stdout);

	hdlhashtable hdata = nil;
	tyvaluerecord rec;
	tyvaluerecord cmdkey;

	assert(menudata_ensure_root());
	clear_data_children();
	assert(find_data(&hdata));

	hdlhashtable happ  = mk_subtable(hdata, "AppD3");
	hdlhashtable hmenu = mk_subtable(happ, "M");
	hdlhashtable hitem = mk_subtable(hmenu, "I");
	set_string(hitem, "label", "Save");
	set_string(hitem, "script", "saveDoc()");
	set_char(hitem, "cmdkey", 'S');

	assert(menudata_describe_leaf(hitem, &rec));
	assert(rec.valuetype == recordvaluetype);

	cmdkey = rec_field(rec, "cmdkey");
	assert(cmdkey.valuetype == charvaluetype);
	assert(cmdkey.data.chvalue == 'S');

	disposevaluerecord(rec, false);
	clear_data_children();

	printf("PASS\n");
	fflush(stdout);
}


/* ---------- main ---------- */

int main(void) {
	TR_INIT("menu_list_describe_tests");

	printf("\n=== Menu list/describe Headless Tests ===\n");
	printf("[menu] Initializing runtime...\n");
	fflush(stdout);

	log_init();

	assert(initmemory());
	initstrings();
	assert(initlang());
	assert(inittablestructure());
	assert(langinitresources_headless());
	assert(langinitverbs());
	assert(wp_portable_init());

	printf("[menu] Testing menudata_list_leaves / menudata_describe_leaf\n");
	fflush(stdout);

	TR_RUN(test_list_empty);
	TR_RUN(test_list_single_leaf);
	TR_RUN(test_list_scoped);
	TR_RUN(test_describe_script);
	TR_RUN(test_describe_defaults);
	TR_RUN(test_describe_cmdkey);

	printf("\n========================================\n");
	printf("[menu] ALL TESTS PASSED\n");
	printf("========================================\n");
	fflush(stdout);

	wp_portable_shutdown();

	TR_SUMMARY();
	return TR_EXIT_CODE();
}
