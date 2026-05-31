/*
 * test_window_bridge.c - Behavioral tests for Phase C window-event callback bridge
 *
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 Frontier contributors
 *
 * Phase C of docs/MENU_PORT_PLAN.md: the C-side bridge that fires
 * idopenwindowscript / idclosewindowscript when the REPL "frontmost window"
 * changes.
 *
 * These tests are BEHAVIORAL: they invoke the real function and assert on
 * observable ODB side-effects. No source-inspection.
 *
 * Test plan (per task prompt):
 *   1. Bridge fires idopenwindowscript at the right time (openWindow marker set)
 *   2. Bridge fires idclosewindowscript at the right time (closeWindow marker set)
 *   3. nil + nil is a no-op (no markers set)
 *   4. Same window to same window: no double-fire
 *   5. Escaping: malicious path with embedded "); does not inject code
 *
 * Hook installation strategy
 * --------------------------
 * The bridge fires system.callbacks.openWindow/closeWindow via UserTalk.
 * The test installs lightweight stubs into system.callbacks.openWindow and
 * system.callbacks.closeWindow using the C-side langcompiletext + hashtableassign
 * API (same pattern as test_callback_infrastructure.c).  This avoids the
 * dependency on script.newScriptObject (which requires a full Frontier.root DB),
 * making the test run cleanly against the standalone headless runtime initialized
 * by db_format_prepare_runtime().
 *
 * Stub body design: the stub bodies are pure expressions
 * (no "on handler(...)" wrappers) that set a marker in system.temp:
 *
 *   system.temp.phasec_open_marker = true; return(true)
 *   system.temp.phasec_close_marker = true; return(true)
 *
 * When the bridge fires system.callbacks.openWindow("path"), the UserTalk
 * evaluator calls the CodeValue stored at that key and executes the stub body.
 * Parameters passed by the caller are accessible via "name" (first param) if
 * the body references them, but for marker-setting we only need side effects.
 */

#include "framework/test_framework.h"
#include "test_report.h"

#include <string.h>

#include "../Common/headers/frontier.h"
#include "../Common/headers/lang.h"
#include "../Common/headers/langinternal.h"
#include "../Common/headers/langsystem7.h"
#include "../Common/headers/tablestructure.h"
#include "../Common/headers/strings.h"
#include "../Common/headers/logging.h"
#include "../Common/headers/langexternal.h"
#include "../Common/headers/tableverbs.h"
#include "../Common/headers/db_format.h"

/*
 * The bridge functions we are testing.  These are declared in
 * frontier-cli/window_registry.h but we include the header directly from
 * the CLI tree.
 */
#include "../frontier-cli/window_registry.h"

/* ------------------------------------------------------------------ */
/*  Helper: run a UserTalk expression and return the bigstring result. */
/* ------------------------------------------------------------------ */

static boolean run_expr(const char *expr, bigstring bsresult) {
	bigstring bsprog;
	size_t len = strlen(expr);
	if (len > 255) len = 255;
	bsprog[0] = (unsigned char)len;
	memcpy(bsprog + 1, expr, len);
	return langrunstringnoerror(bsprog, bsresult);
}

/* ------------------------------------------------------------------ */
/*  Helper: check that a bigstring equals a C literal.                 */
/* ------------------------------------------------------------------ */

static boolean bs_equals(bigstring bs, const char *expected) {
	size_t elen = strlen(expected);
	size_t blen = (size_t)(unsigned char)bs[0];
	if (blen != elen) return false;
	return memcmp(bs + 1, expected, elen) == 0;
}

/* ------------------------------------------------------------------ */
/*  Helper: compile a script body and assign it to a name in a table. */
/*                                                                      */
/*  Mirrors the add_test_script() pattern in test_callback_infra.      */
/*  ht:   target hash table (e.g. the system.callbacks sub-table)      */
/*  name: key name as a Pascal bigstring                               */
/*  body: C string script body (no "on handler" wrapper needed)        */
/* ------------------------------------------------------------------ */

static boolean assign_compiled_stub(hdlhashtable ht, bigstring name, const char *body) {
	Handle htext;
	hdltreenode hcode;
	tyvaluerecord val;
	bigstring bsbody;

	copyctopstring(body, bsbody);

	if (!newtexthandle(bsbody, &htext)) {
		printf("[test] assign_compiled_stub: newtexthandle failed\n");
		return false;
	}

	if (!langcompiletext(htext, false, &hcode)) {
		printf("[test] assign_compiled_stub: langcompiletext failed for: %s\n", body);
		return false;
	}

	val.valuetype = codevaluetype;
	val.data.codevalue = hcode;

	return hashtableassign(ht, name, val);
}

/* ------------------------------------------------------------------ */
/*  Helper: clear marker ODB values from prior test runs.             */
/*                                                                     */
/*  Uses the C-side hashdelete API because the bare UserTalk "delete" */
/*  verb is registered under "lang.delete" and bare-name lookup       */
/*  doesn't find it in the minimal headless runtime.                  */
/* ------------------------------------------------------------------ */

static boolean clear_markers(void) {
	bigstring bssystem, bstemp;
	bigstring bsopen, bsclose, bsinject;
	hdlhashtable hsystem, htemp;

	copyctopstring("system", bssystem);
	copyctopstring("temp", bstemp);
	copyctopstring("phasec_open_marker", bsopen);
	copyctopstring("phasec_close_marker", bsclose);
	copyctopstring("phasec_inject_marker", bsinject);

	if (!findnamedtable(roottable, bssystem, &hsystem))
		return false;
	if (!findnamedtable(hsystem, bstemp, &htemp))
		return false;

	/* Silently ignore errors -- marker may not exist on first run */
	pushhashtable(htemp);
	hashdelete(bsopen, false, false);
	hashdelete(bsclose, false, false);
	hashdelete(bsinject, false, false);
	pophashtable();

	return true;
}

/* ------------------------------------------------------------------ */
/*  Install test hooks into system.callbacks.                         */
/*                                                                     */
/*  Compiles simple stub bodies and assigns them directly into the     */
/*  system.callbacks table using the C hash table API.  This avoids   */
/*  the script.newScriptObject dependency.                             */
/* ------------------------------------------------------------------ */

static hdlhashtable g_hcallbacks = nil;

static boolean install_test_hooks(void) {
	bigstring bsopen, bsclose;

	if (g_hcallbacks == nil) {
		printf("[test] install_test_hooks: g_hcallbacks is nil -- setup failed\n");
		return false;
	}

	copyctopstring("openWindow", bsopen);
	copyctopstring("closeWindow", bsclose);

	/*
	 * The bridge calls system.callbacks.openWindow("path") and
	 * system.callbacks.closeWindow("path") passing the window path as
	 * a string argument.  The stubs must declare a parameter to accept it;
	 * otherwise the runtime raises "too many parameters".
	 */
	if (!assign_compiled_stub(g_hcallbacks, bsopen,
	        "on openWindow(name) {system.temp.phasec_open_marker = true; return(true)}"))
		return false;

	if (!assign_compiled_stub(g_hcallbacks, bsclose,
	        "on closeWindow(title) {system.temp.phasec_close_marker = true; return(true)}"))
		return false;

	return true;
}

/* ------------------------------------------------------------------ */
/*  Remove test hooks so subsequent tests see a clean callbacks table. */
/*  Uses C-side hashdelete for same reason as clear_markers.          */
/* ------------------------------------------------------------------ */

static boolean remove_test_hooks(void) {
	bigstring bsopen, bsclose;

	if (g_hcallbacks == nil)
		return true;

	copyctopstring("openWindow", bsopen);
	copyctopstring("closeWindow", bsclose);

	pushhashtable(g_hcallbacks);
	hashdelete(bsopen, false, false);
	hashdelete(bsclose, false, false);
	pophashtable();

	return true;
}

/* ================================================================== */
/*  Test 1: firing idopenwindowscript sets the open marker            */
/* ================================================================== */

static bool test_openwindow_fires_script(void) {
	g_test_stats.current_test_name = "on_frontmost_changed(nil, window) fires idopenwindowscript";

	clear_markers();
	install_test_hooks();

	/* Fire the bridge: nil -> test window (simulates REPL boot) */
	on_frontmost_changed(nil, WINDOW_BRIDGE_REPL_PATH);

	/* Check that the open marker was set by the stub */
	bigstring bsresult;
	run_expr(
		"if defined(system.temp.phasec_open_marker) {\"set\"} else {\"(not set)\"}",
		bsresult);
	TEST_ASSERT(!bs_equals(bsresult, "(not set)"),
		"system.temp.phasec_open_marker must have been set by idopenwindowscript");

	remove_test_hooks();
	clear_markers();
	TEST_PASS("on_frontmost_changed(nil, window) fires idopenwindowscript");
}

/* ================================================================== */
/*  Test 2: firing idclosewindowscript sets the close marker          */
/* ================================================================== */

static bool test_closewindow_fires_script(void) {
	g_test_stats.current_test_name = "on_frontmost_changed(window, nil) fires idclosewindowscript";

	clear_markers();
	install_test_hooks();

	/* Fire the bridge: test window -> nil (window closing) */
	on_frontmost_changed(WINDOW_BRIDGE_REPL_PATH, nil);

	bigstring bsresult;
	run_expr(
		"if defined(system.temp.phasec_close_marker) {\"set\"} else {\"(not set)\"}",
		bsresult);
	TEST_ASSERT(!bs_equals(bsresult, "(not set)"),
		"system.temp.phasec_close_marker must have been set by idclosewindowscript");

	remove_test_hooks();
	clear_markers();
	TEST_PASS("on_frontmost_changed(window, nil) fires idclosewindowscript");
}

/* ================================================================== */
/*  Test 3: nil -> nil is a no-op (no markers set)                   */
/* ================================================================== */

static bool test_nil_to_nil_noop(void) {
	g_test_stats.current_test_name = "on_frontmost_changed(nil, nil) is a no-op";

	clear_markers();
	install_test_hooks();

	on_frontmost_changed(nil, nil);

	bigstring bsresult;
	run_expr(
		"if defined(system.temp.phasec_open_marker) {\"set\"} else {\"(not set)\"}",
		bsresult);
	TEST_ASSERT(bs_equals(bsresult, "(not set)"),
		"nil->nil must NOT set open marker");

	run_expr(
		"if defined(system.temp.phasec_close_marker) {\"set\"} else {\"(not set)\"}",
		bsresult);
	TEST_ASSERT(bs_equals(bsresult, "(not set)"),
		"nil->nil must NOT set close marker");

	remove_test_hooks();
	TEST_PASS("on_frontmost_changed(nil, nil) is a no-op");
}

/* ================================================================== */
/*  Test 4: same window -> same window does not double-fire           */
/* ================================================================== */

static bool test_same_window_no_double_fire(void) {
	g_test_stats.current_test_name = "on_frontmost_changed(same, same) does not double-fire";

	clear_markers();
	install_test_hooks();

	/* fire same -> same */
	on_frontmost_changed(WINDOW_BRIDGE_REPL_PATH, WINDOW_BRIDGE_REPL_PATH);

	bigstring bsresult;
	run_expr(
		"if defined(system.temp.phasec_open_marker) {\"set\"} else {\"(not set)\"}",
		bsresult);
	TEST_ASSERT(bs_equals(bsresult, "(not set)"),
		"same->same must fire openWindow ZERO times (open marker must be not set)");

	remove_test_hooks();
	clear_markers();
	TEST_PASS("on_frontmost_changed(same, same) does not double-fire");
}

/* ================================================================== */
/*  Test 5: injection-attempt path does not execute injected code     */
/* ================================================================== */

static bool test_injection_attempt_is_safe(void) {
	g_test_stats.current_test_name = "Injection-attempt window path is safely escaped";

	clear_markers();
	install_test_hooks();

	/*
	 * A window path designed to break out of the string parameter and inject
	 * a second UserTalk statement.
	 *
	 * Without langdeparsestring escaping, this path would produce the script:
	 *   system.callbacks.openWindow("evil");system.temp.phasec_inject_marker=true;(")
	 *
	 * With proper escaping (langdeparsestring inserts a backslash before each
	 * `"` and `\`), the embedded quote is backslash-escaped to `\"` so the
	 * injected code is never parsed as a separate statement.
	 */
	const char *evil_path =
		"evil\");system.temp.phasec_inject_marker=true;(\"";

	on_frontmost_changed(nil, evil_path);

	bigstring bsresult;
	run_expr(
		"if defined(system.temp.phasec_inject_marker) {\"INJECTED\"} else {\"safe\"}",
		bsresult);
	TEST_ASSERT(bs_equals(bsresult, "safe"),
		"Injected code must NOT execute; langdeparsestring must double the embedded quote");

	remove_test_hooks();
	clear_markers();
	TEST_PASS("Injection-attempt window path is safely escaped");
}

/* ================================================================== */
/*  Suite registration                                                 */
/* ================================================================== */

static test_case_t test_cases[] = {
	{"openwindow fires script",       test_openwindow_fires_script},
	{"closewindow fires script",      test_closewindow_fires_script},
	{"nil to nil is noop",            test_nil_to_nil_noop},
	{"same window no double fire",    test_same_window_no_double_fire},
	{"injection attempt is safe",     test_injection_attempt_is_safe},
};

int main(void) {
	TR_INIT("test_window_bridge");
	printf("=== Phase C Window-Event Callback Bridge Tests ===\n");
	log_init();

	/*
	 * Full runtime init: db_format_prepare_runtime() handles the complete
	 * sequence (initmemory, initstrings, initlang, inittablestructure,
	 * langinitresources_headless, langinitverbs, linksystemtablestructure).
	 * After this call, roottable has system.temp available and all verb
	 * handlers are registered.
	 */
	if (!db_format_prepare_runtime()) {
		printf("FATAL: db_format_prepare_runtime() failed -- cannot run bridge tests\n");
		return 1;
	}

	/*
	 * Create system.callbacks table using the C-side API.
	 *
	 * system.temp is already created by linksystemtablestructure inside
	 * db_format_prepare_runtime (it calls checktable on the system branch).
	 * system.callbacks is NOT created by that path, so we create it here
	 * using tablenewsystemtable (which marks it as not-saved, matching the
	 * ephemeral semantics of a headless-only callbacks table).
	 *
	 * Path: roottable -> "system" -> create "callbacks"
	 */
	{
		bigstring bssystem, bscallbacks;
		hdlhashtable hsystem;

		copyctopstring("system", bssystem);
		copyctopstring("callbacks", bscallbacks);

		/* Navigate to system sub-table (created by inittablestructure path) */
		if (!findnamedtable(roottable, bssystem, &hsystem)) {
			printf("FATAL: system table not found in roottable -- cannot run bridge tests\n");
			return 1;
		}

		/* Create system.callbacks if it doesn't already exist */
		if (!findnamedtable(hsystem, bscallbacks, &g_hcallbacks)) {
			if (!tablenewsystemtable(hsystem, bscallbacks, &g_hcallbacks)) {
				printf("FATAL: tablenewsystemtable(system.callbacks) failed -- cannot run bridge tests\n");
				return 1;
			}
		}
	}

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
