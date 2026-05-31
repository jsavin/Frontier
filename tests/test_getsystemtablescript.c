/*
 * test_getsystemtablescript.c - Behavioral tests for getsystemtablescript headless support
 *
 * SPDX-License-Identifier: MIT
 *
 * Phase B1 of the menu port plan: verify that getstringlist(idsystemtablescripts, ...)
 * (list ID 139) correctly resolves all idsystemtablescripts enum values to ODB verb
 * path strings in headless mode.
 *
 * Prior to this fix, getstringlist() had no case for list ID 139, so every call
 * to getsystemtablescript() silently returned false/empty in headless.
 *
 * getsystemtablescript() is a 1-line wrapper around getstringlist(139, ...):
 *
 *   boolean getsystemtablescript(short idscript, bigstring bsscript) {
 *       return getstringlist(idsystemtablescripts, idscript, bsscript);
 *   }
 *
 * Testing getstringlist(139, ...) directly covers the exact behavior being
 * changed, following the same pattern as test_stringerrorlist.c.
 *
 * These tests are BEHAVIORAL: they call the real function and assert on
 * returned paths. No source-inspection, no grep, no #define checks.
 */

#include "framework/test_framework.h"
#include "test_report.h"

/* Include frontier.h for bigstring, boolean, setemptystring, etc. */
#include "../Common/headers/frontier.h"
/* Include tablestructure.h for idsystemtablescripts and the enum constants */
#include "../Common/headers/tablestructure.h"

/* Provided by headless_lang_runtime_more_stubs.c */
extern boolean getstringlist(short listid, short index, bigstring bs);

/* Stub for tcp_process_callbacks referenced by processsleep in the stubs file */
int tcp_process_callbacks(void) { return 0; }

/* Helper: extract pascal string content into a null-terminated C string */
static void pstr_to_cstr(const bigstring bs, char *out, size_t outsz) {
	int len = (int)(unsigned char)bs[0];
	if ((size_t)len >= outsz)
		len = (int)(outsz - 1);
	if (len > 0)
		memcpy(out, bs + 1, (size_t)len);
	out[len] = '\0';
}

/* --- Test 1: list 139 is now accessible (previously returned false always) --- */
static bool test_list_139_accessible(void) {
	g_test_stats.current_test_name = "List 139 (idsystemtablescripts) is accessible";

	bigstring bs;
	setemptystring(bs);

	boolean ok = getstringlist(idsystemtablescripts, idopenwindowscript, bs);
	TEST_ASSERT(ok == true,
		"getstringlist(139, idopenwindowscript) must return true after Phase B1 fix");

	TEST_PASS("list 139 is now accessible");
}

/* --- Test 2: idopenwindowscript resolves to a non-empty dotted ODB path --- */
static bool test_idopenwindowscript_resolves(void) {
	g_test_stats.current_test_name = "idopenwindowscript resolves to non-empty dotted path";

	bigstring bs;
	setemptystring(bs);

	boolean ok = getstringlist(idsystemtablescripts, idopenwindowscript, bs);
	TEST_ASSERT(ok == true, "getstringlist(139, idopenwindowscript) must return true");
	TEST_ASSERT(bs[0] > 0, "returned path must be non-empty");

	char cstr[256];
	pstr_to_cstr(bs, cstr, sizeof(cstr));
	TEST_ASSERT(strchr(cstr, '.') != NULL,
		"idopenwindowscript path '%s' must be a dotted ODB path", cstr);

	TEST_PASS("idopenwindowscript resolves to non-empty dotted path");
}

/* --- Test 3: idclosewindowscript resolves to a non-empty dotted ODB path --- */
static bool test_idclosewindowscript_resolves(void) {
	g_test_stats.current_test_name = "idclosewindowscript resolves to non-empty dotted path";

	bigstring bs;
	setemptystring(bs);

	boolean ok = getstringlist(idsystemtablescripts, idclosewindowscript, bs);
	TEST_ASSERT(ok == true, "getstringlist(139, idclosewindowscript) must return true");
	TEST_ASSERT(bs[0] > 0, "returned path must be non-empty");

	char cstr[256];
	pstr_to_cstr(bs, cstr, sizeof(cstr));
	TEST_ASSERT(strchr(cstr, '.') != NULL,
		"idclosewindowscript path '%s' must be a dotted ODB path", cstr);

	TEST_PASS("idclosewindowscript resolves to non-empty dotted path");
}

/* --- Test 4: idfrontierstartup resolves --- */
static bool test_idfrontierstartup_resolves(void) {
	g_test_stats.current_test_name = "idfrontierstartup resolves to non-empty path";

	bigstring bs;
	setemptystring(bs);

	boolean ok = getstringlist(idsystemtablescripts, idfrontierstartup, bs);
	TEST_ASSERT(ok == true, "getstringlist(139, idfrontierstartup) must return true");
	TEST_ASSERT(bs[0] > 0, "returned path must be non-empty");

	TEST_PASS("idfrontierstartup resolves to non-empty path");
}

/* --- Test 5: idsuspendscript resolves --- */
static bool test_idsuspendscript_resolves(void) {
	g_test_stats.current_test_name = "idsuspendscript resolves to non-empty path";

	bigstring bs;
	setemptystring(bs);

	boolean ok = getstringlist(idsystemtablescripts, idsuspendscript, bs);
	TEST_ASSERT(ok == true, "getstringlist(139, idsuspendscript) must return true");
	TEST_ASSERT(bs[0] > 0, "returned path must be non-empty");

	TEST_PASS("idsuspendscript resolves to non-empty path");
}

/* --- Test 6: idresumescript resolves --- */
static bool test_idresumescript_resolves(void) {
	g_test_stats.current_test_name = "idresumescript resolves to non-empty path";

	bigstring bs;
	setemptystring(bs);

	boolean ok = getstringlist(idsystemtablescripts, idresumescript, bs);
	TEST_ASSERT(ok == true, "getstringlist(139, idresumescript) must return true");
	TEST_ASSERT(bs[0] > 0, "returned path must be non-empty");

	TEST_PASS("idresumescript resolves to non-empty path");
}

/* --- Test 7: idmenubarscript (ID=1, first entry) resolves --- */
static bool test_idmenubarscript_resolves(void) {
	g_test_stats.current_test_name = "idmenubarscript (ID=1) resolves to non-empty path";

	bigstring bs;
	setemptystring(bs);

	boolean ok = getstringlist(idsystemtablescripts, idmenubarscript, bs);
	TEST_ASSERT(ok == true, "getstringlist(139, idmenubarscript) must return true");
	TEST_ASSERT(bs[0] > 0, "returned path must be non-empty");

	TEST_PASS("idmenubarscript resolves to non-empty path");
}

/* --- Test 8: out-of-range ID returns false and empty string gracefully --- */
static bool test_out_of_range_id_returns_false(void) {
	g_test_stats.current_test_name = "Out-of-range script ID returns false";

	bigstring bs;
	setemptystring(bs);

	/* 99 is well beyond any valid idsystemtablescripts entry */
	boolean ok = getstringlist(idsystemtablescripts, 99, bs);
	TEST_ASSERT(ok == false,
		"getstringlist(139, 99) must return false for unknown index");

	/* The returned string must be empty on failure */
	TEST_ASSERT(bs[0] == 0,
		"returned bigstring must be empty on failure (length byte must be 0)");

	TEST_PASS("out-of-range ID correctly returns false with empty string");
}

/* --- Test 9: idrunfilemenuscript (pike-era hook) resolves --- */
static bool test_idrunfilemenuscript_resolves(void) {
	g_test_stats.current_test_name = "idrunfilemenuscript resolves to non-empty path";

	bigstring bs;
	setemptystring(bs);

	boolean ok = getstringlist(idsystemtablescripts, idrunfilemenuscript, bs);
	TEST_ASSERT(ok == true, "getstringlist(139, idrunfilemenuscript) must return true");
	TEST_ASSERT(bs[0] > 0, "returned path must be non-empty");

	TEST_PASS("idrunfilemenuscript resolves to non-empty path");
}

/* --- Test 10: idruneditmenuscript (pike-era hook) resolves --- */
static bool test_idruneditmenuscript_resolves(void) {
	g_test_stats.current_test_name = "idruneditmenuscript resolves to non-empty path";

	bigstring bs;
	setemptystring(bs);

	boolean ok = getstringlist(idsystemtablescripts, idruneditmenuscript, bs);
	TEST_ASSERT(ok == true, "getstringlist(139, idruneditmenuscript) must return true");
	TEST_ASSERT(bs[0] > 0, "returned path must be non-empty");

	TEST_PASS("idruneditmenuscript resolves to non-empty path");
}

/* --- Test 11: idrunopenrecentmenuscript (ID=46, last entry) resolves --- */
static bool test_idrunopenrecentmenuscript_resolves(void) {
	g_test_stats.current_test_name = "idrunopenrecentmenuscript (ID=46) resolves to non-empty path";

	bigstring bs;
	setemptystring(bs);

	boolean ok = getstringlist(idsystemtablescripts, idrunopenrecentmenuscript, bs);
	TEST_ASSERT(ok == true, "getstringlist(139, idrunopenrecentmenuscript) must return true");
	TEST_ASSERT(bs[0] > 0, "returned path must be non-empty");

	TEST_PASS("idrunopenrecentmenuscript resolves to non-empty path");
}

/*
 * Test 12: All 46 enum values defined in tablestructure.h resolve to
 * non-empty paths. This is the comprehensive smoke test: if any entry
 * is missing from the YAML table, this test catches it.
 */
static bool test_all_defined_ids_resolve(void) {
	g_test_stats.current_test_name = "All 46 idsystemtablescripts enum values resolve";

	/*
	 * Enum values from tablestructure.h, in declaration order.
	 * idopenrecentmenutable=44, idreplacedialogexpertmode=45, idrunopenrecentmenuscript=46
	 * are explicit numeric assignments; all others follow sequential assignment from 1.
	 */
	static const short all_ids[] = {
		idmenubarscript,                    /*  1 */
		idobjectdbscript,                   /*  2 */
		idquickscriptscript,                /*  3 */
		idtechsupportscript,                /*  4 */
		idfinder2clickscript,               /*  5 */
		idfinder2frontscript,               /*  6 */
		idfrontierclickers,                 /*  7 */
		idcontrol2clickscript,              /*  8 */
		idcommand2clickscript,              /*  9 */
		idoption2clickscript,               /* 10 */
		idopenwindowscript,                 /* 11 */
		idsavewindowscript,                 /* 12 */
		idclosewindowscript,                /* 13 */
		idcompilewindowscript,              /* 14 */
		idisfirsttimescript,                /* 15 */
		idopenurlscript,                    /* 16 */
		iduseriso8859map,                   /* 17 */
		iduserfontprefscript,               /* 18 */
		idinexpertmodescript,               /* 19 */
		idtoggleexpertmodescript,           /* 20 */
		idrequiredeclarationsscript,        /* 21 */
		idsuspendscript,                    /* 22 */
		idresumescript,                     /* 23 */
		idsearchparamstable,                /* 24 */
		idagentsenabledscript,              /* 25 */
		idautosave,                         /* 26 */
		idfrontierstartup,                  /* 27 */
		idflwaitduringstartup,              /* 28 */
		idwebserverstats,                   /* 29 */
		idinetdshutdown,                    /* 30 */
		idpikeisfilemenuitemenabledscript,   /* 31 */
		idpikegetmenuitemstring,             /* 32 */
		idrunfilemenuscript,                /* 33 */
		idopstruct2clickscript,             /* 34 */
		idopreturnkeyscript,                /* 35 */
		idopexpandscript,                   /* 36 */
		idopcollapsescript,                 /* 37 */
		idopcursormovedscript,              /* 38 */
		idoprightclickscript,               /* 39 */
		idruneditmenuscript,                /* 40 */
		idpikeisfilemenuitemcheckedscript,   /* 41 */
		idopinsertscript,                   /* 42 */
		idopenrecentmenutable,              /* 44 - explicit */
		idreplacedialogexpertmode,          /* 45 - explicit */
		idrunopenrecentmenuscript,          /* 46 - explicit */
	};

	size_t n = sizeof(all_ids) / sizeof(all_ids[0]);

	for (size_t i = 0; i < n; i++) {
		bigstring bs;
		setemptystring(bs);
		boolean ok = getstringlist(idsystemtablescripts, all_ids[i], bs);
		TEST_ASSERT(ok == true,
			"getstringlist(139, %d) must return true", (int)all_ids[i]);
		TEST_ASSERT(bs[0] > 0,
			"getstringlist(139, %d) must return non-empty path", (int)all_ids[i]);
	}

	TEST_PASS("all 45 idsystemtablescripts enum values resolve to non-empty paths");
}

static test_case_t test_cases[] = {
	{"list 139 accessible",                  test_list_139_accessible},
	{"idopenwindowscript resolves",          test_idopenwindowscript_resolves},
	{"idclosewindowscript resolves",         test_idclosewindowscript_resolves},
	{"idfrontierstartup resolves",           test_idfrontierstartup_resolves},
	{"idsuspendscript resolves",             test_idsuspendscript_resolves},
	{"idresumescript resolves",              test_idresumescript_resolves},
	{"idmenubarscript resolves",             test_idmenubarscript_resolves},
	{"out-of-range ID returns false",        test_out_of_range_id_returns_false},
	{"idrunfilemenuscript resolves",         test_idrunfilemenuscript_resolves},
	{"idruneditmenuscript resolves",         test_idruneditmenuscript_resolves},
	{"idrunopenrecentmenuscript resolves",   test_idrunopenrecentmenuscript_resolves},
	{"all 45 IDs resolve",                  test_all_defined_ids_resolve},
};

int main(void) {
	TR_INIT("test_getsystemtablescript");
	printf("=== getsystemtablescript / list 139 (idsystemtablescripts) Tests ===\n");
	log_init();
	test_init();
	bool all_passed = run_test_suite(test_cases, sizeof(test_cases) / sizeof(test_cases[0]));
	test_summary();
	if (tr_count < TR_MAX_TESTS) {
		tr_results[tr_count].name = "run_test_suite";
		tr_results[tr_count].passed = (all_passed ? 1 : 0);
		tr_count++;
		if (all_passed) tr_pass_count++; else tr_fail_count++;
	}
	TR_SUMMARY();
	return TR_EXIT_CODE();
}
