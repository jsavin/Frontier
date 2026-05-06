/*
 * menudata_headless_tests.c - Unit tests for menudata_ensure_root()
 *
 * Validates the lazy creation of the system.menus.data table chain. This
 * function is called at the head of every menu verb so that the verbs work
 * on both fresh Virgin.root databases and v7-migrated roots that lack the
 * system.menus subtree (the gap that motivated 16 skipped integration tests
 * in tests/integration/test_cases/menu_data_verbs.yaml).
 *
 * Reference pattern: Common/source/tablestructure.c::linksystemtablestructure
 * — the gold standard for "find or create system sub-tables." Uses
 * findnamedtable + tablenewsubtable per missing rung.
 *
 * See planning/architectural_decision_records/ADR-016-headless-menu-system-projection.md
 * for the architectural model this implements.
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
#include "lang.h"
#include "tablestructure.h"
#include "tableverbs.h"
#include "stringdefs.h"
#include "logging.h"
#include "../portable/wptext_portable.h"

#include "menudata_headless.h"

/*
 * Helpers to inspect the chain.
 */

static boolean find_subtable(hdlhashtable hparent, bigstring bsname,
                             hdlhashtable *hresult) {
	*hresult = nil;
	return findnamedtable(hparent, bsname, hresult);
}

static boolean find_system(hdlhashtable *hsystem) {
	return find_subtable(roottable, namesystembranch, hsystem);
}

static boolean find_menus(hdlhashtable *hmenus) {
	hdlhashtable hsystem = nil;
	if (!find_system(&hsystem))
		return false;
	return find_subtable(hsystem, STR_menus, hmenus);
}

static boolean find_data(hdlhashtable *hdata) {
	hdlhashtable hmenus = nil;

	if (!find_menus(&hmenus))
		return false;

	return find_subtable(hmenus, STR_data, hdata);
}

/*
 * Reset to a known-clean state between tests.
 *
 * The harness initialises roottable once via inittablestructure() in main().
 * To exercise scenario 1 ("only system defined") we manually create system as
 * a sub-table of roottable, mirroring what linksystemtablestructure() does
 * during real boot. We DO NOT pre-create system.menus or system.menus.data —
 * those are precisely what menudata_ensure_root() must create.
 */
static void ensure_system_only(void) {
	hdlhashtable hsystem = nil;

	if (find_system(&hsystem))
		return; /* already exists from a prior test or init */

	assert(tablenewsubtable(roottable, namesystembranch, &hsystem));
}

/*
 * Test 1: Fresh root — only system exists, neither menus nor data.
 *
 * Expected: ensure returns true, system.menus and system.menus.data are
 * created.
 */
static void test_fresh_root(void) {
	printf("[menudata] Test 1: Fresh root (no menus subtree)... ");
	fflush(stdout);

	hdlhashtable hdata = nil;

	ensure_system_only();

	/* Pre-condition: data does not exist yet */
	assert(!find_data(&hdata));

	/* Action */
	assert(menudata_ensure_root());

	/* Post-condition: data exists and is reachable */
	hdata = nil;
	assert(find_data(&hdata));
	assert(hdata != nil);

	printf("PASS\n");
	fflush(stdout);
}

/*
 * Test 2: Idempotent — calling ensure twice yields the same handle.
 *
 * Expected: second call returns true, and resolves to the same hashtable
 * handle, i.e. no new table was created on the second call.
 */
static void test_idempotent(void) {
	printf("[menudata] Test 2: Idempotent (handle stable across calls)... ");
	fflush(stdout);

	ensure_system_only();

	assert(menudata_ensure_root());
	hdlhashtable hdata_first = nil;
	assert(find_data(&hdata_first));
	assert(hdata_first != nil);

	assert(menudata_ensure_root());
	hdlhashtable hdata_second = nil;
	assert(find_data(&hdata_second));
	assert(hdata_second != nil);

	assert(hdata_first == hdata_second);

	printf("PASS\n");
	fflush(stdout);
}

/*
 * Test 3: Partial chain — system.menus exists, data does not.
 *
 * Pre-create a sibling under system.menus to verify it survives the ensure
 * call (we should not blow away or rebuild existing tables).
 */
static void test_partial_chain(void) {
	printf("[menudata] Test 3: Partial chain (menus exists, data missing)... ");
	fflush(stdout);

	hdlhashtable hsystem = nil;
	hdlhashtable hmenus = nil;
	hdlhashtable hsibling = nil;
	bigstring bssibling;

	ensure_system_only();
	assert(find_system(&hsystem));

	/* Pre-create system.menus only (not data) */
	if (!find_subtable(hsystem, STR_menus, &hmenus)) {
		assert(tablenewsubtable(hsystem, STR_menus, &hmenus));
	}

	/* Pre-create a sibling beneath menus to verify non-disturbance */
	copyctopstring("sibling_witness", bssibling);
	if (!find_subtable(hmenus, bssibling, &hsibling)) {
		assert(tablenewsubtable(hmenus, bssibling, &hsibling));
	}
	hdlhashtable hsibling_before = hsibling;

	/* Action */
	assert(menudata_ensure_root());

	/* Post-condition: data exists */
	hdlhashtable hdata = nil;
	assert(find_data(&hdata));
	assert(hdata != nil);

	/* Post-condition: sibling untouched */
	hdlhashtable hsibling_after = nil;
	assert(find_subtable(hmenus, bssibling, &hsibling_after));
	assert(hsibling_after == hsibling_before);

	printf("PASS\n");
	fflush(stdout);
}

/*
 * Test 4: Fully-populated chain — system.menus.data already exists with a
 * pre-existing child entry. ensure must not disturb it.
 */
static void test_fully_populated(void) {
	printf("[menudata] Test 4: Fully-populated chain preserves children... ");
	fflush(stdout);

	hdlhashtable hsystem = nil;
	hdlhashtable hmenus = nil;
	hdlhashtable hdata = nil;
	hdlhashtable hchild = nil;
	bigstring bschild;

	ensure_system_only();
	assert(find_system(&hsystem));

	/* Build the full chain manually first */
	if (!find_subtable(hsystem, STR_menus, &hmenus)) {
		assert(tablenewsubtable(hsystem, STR_menus, &hmenus));
	}
	if (!find_subtable(hmenus, STR_data, &hdata)) {
		assert(tablenewsubtable(hmenus, STR_data, &hdata));
	}

	/* Add a child entry under system.menus.data */
	copyctopstring("testapp", bschild);
	if (!find_subtable(hdata, bschild, &hchild)) {
		assert(tablenewsubtable(hdata, bschild, &hchild));
	}
	hdlhashtable hdata_before = hdata;
	hdlhashtable hchild_before = hchild;

	/* Action */
	assert(menudata_ensure_root());

	/* Post-condition: data unchanged */
	hdlhashtable hdata_after = nil;
	assert(find_data(&hdata_after));
	assert(hdata_after == hdata_before);

	/* Post-condition: child entry survived */
	hdlhashtable hchild_after = nil;
	assert(find_subtable(hdata_after, bschild, &hchild_after));
	assert(hchild_after == hchild_before);

	printf("PASS\n");
	fflush(stdout);
}

/*
 * Test 5: Failure mode — roottable nil returns false without crashing.
 *
 * This guards against being called before linksystemtablestructure ran. We
 * temporarily save the global, set it nil, call, then restore. We do this
 * LAST so that if something goes wrong we don't poison subsequent tests.
 */
static void test_nil_roottable(void) {
	printf("[menudata] Test 5: Nil roottable returns false (graceful)... ");
	fflush(stdout);

	hdlhashtable saved = roottable;
	roottable = nil;

	boolean result = menudata_ensure_root();
	assert(result == false);

	roottable = saved;

	/* Sanity: make sure restoring works and the chain is still usable */
	assert(menudata_ensure_root());
	hdlhashtable hdata = nil;
	assert(find_data(&hdata));
	assert(hdata != nil);

	printf("PASS\n");
	fflush(stdout);
}

/*
 * Main test runner.
 */
int main(void) {
	TR_INIT("menudata_headless_tests");

	printf("\n=== Menudata Headless Tests ===\n");
	printf("[menudata] Initializing runtime...\n");
	fflush(stdout);

	log_init();

	assert(initmemory());
	initstrings();
	assert(initlang());
	assert(inittablestructure());
	assert(langinitresources_headless());
	assert(langinitverbs());
	assert(wp_portable_init());

	printf("[menudata] Testing menudata_ensure_root()\n");
	fflush(stdout);

	TR_RUN(test_fresh_root);
	TR_RUN(test_idempotent);
	TR_RUN(test_partial_chain);
	TR_RUN(test_fully_populated);
	TR_RUN(test_nil_roottable);

	printf("\n========================================\n");
	printf("[menudata] ALL TESTS PASSED\n");
	printf("========================================\n");
	fflush(stdout);

	wp_portable_shutdown();

	TR_SUMMARY();
	return TR_EXIT_CODE();
}
