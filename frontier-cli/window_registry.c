/*
 * window_registry.c - Static REPL window sentinel and window-event bridge
 *
 * Phase C of docs/MENU_PORT_PLAN.md.
 *
 * See window_registry.h for full design documentation.
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Frontier contributors
 */

#include <string.h>

#include "window_registry.h"

#include "../Common/headers/frontier.h"
#include "../Common/headers/lang.h"
#include "../Common/headers/langinternal.h"
#include "../Common/headers/strings.h"
#include "../Common/headers/tablestructure.h"
#include "../Common/headers/tableverbs.h"
#include "../Common/headers/langexternal.h"
#include "../Common/headers/logging.h"

/*
 * getsystemtablescript is a 1-line wrapper in tablestructure.c:
 *   return getstringlist(idsystemtablescripts, idscript, bsscript);
 * It fills bsscript with the Pascal-string template for the given hook ID.
 */
extern boolean getsystemtablescript(short idscript, bigstring bsscript);

/*
 * parsedialogstring substitutes ^0..^3 parameters into a template.
 * Signature (from strings.c):
 *   void parsedialogstring(const bigstring bssource,
 *                          ptrstring bs0, ptrstring bs1,
 *                          ptrstring bs2, ptrstring bs3,
 *                          bigstring bsresult);
 */
extern void parsedialogstring(const bigstring bssource,
                              ptrstring bs0, ptrstring bs1,
                              ptrstring bs2, ptrstring bs3,
                              bigstring bsresult);

/*
 * langdeparsestring escapes a bigstring for safe embedding as a string
 * literal inside a UserTalk expression.  The second parameter is the
 * quote character (chclosecurlyquote = '"' in the UserTalk lexer).
 * Returns true if any escaping was done.
 */
extern boolean langdeparsestring(bigstring bs, byte ch);

/*
 * chclosecurlyquote is the UserTalk string-literal quote character.
 * Defined in lang.c; also visible via lang.h or langinternal.h.
 */
#ifndef chclosecurlyquote
#define chclosecurlyquote '"'
#endif

/*
 * langrunstringnoerror: compile and run a UserTalk bigstring expression.
 * Suppresses error output (like shellrunwindowconfirmationscript uses).
 * Returns true if the script compiled and ran without error.
 */
extern boolean langrunstringnoerror(const bigstring bsprogram, bigstring bsresult);

/* ------------------------------------------------------------------ */
/*  Helpers                                                            */
/* ------------------------------------------------------------------ */

/*
 * Copy a C string into a Pascal bigstring.  Truncates silently at 255 bytes.
 */
static void cstr_to_bigstring(const char *cstr, bigstring bs) {
	size_t len = strlen(cstr);
	if (len > 255) len = 255;
	bs[0] = (unsigned char)len;
	memcpy(bs + 1, cstr, len);
}

/*
 * fire_window_script -- internal workhorse.
 *
 * Looks up the script template for idscript, substitutes the escaped window
 * path as ^0, and runs it synchronously via langrunstringnoerror.
 *
 * Logs a warning and returns (does NOT abort) on any failure, per the
 * "boot-failure-safe" contract in docs/MENU_PORT_PLAN.md and ADR-016.
 *
 * ESCAPING CONTRACT (issue #682)
 * The window_path string is passed through langdeparsestring before
 * parsedialogstring substitution.  langdeparsestring(bs, '"') inserts a
 * backslash before any embedded ASCII '"' (0x22) so it cannot break out
 * of the surrounding UserTalk string literal and inject additional statements.
 * We pass '"' (not chclosecurlyquote/0xD3) because the script templates use
 * straight ASCII double-quotes as string delimiters.
 *
 * In Phase C, window_path is always WINDOW_BRIDGE_REPL_PATH, which is a
 * compile-time constant with no user-controlled content.  The escaping is
 * present regardless to enforce the contract for Phase E callers that will
 * supply user-derived window paths (editor window ODB addresses).
 */
static void fire_window_script(short idscript, const char *window_path) {
	bigstring bs_template;
	bigstring bs_path;
	bigstring bs_substituted;
	bigstring bs_result;

	/* Step 1: get the script template from the system table script registry */
	if (!getsystemtablescript(idscript, bs_template)) {
		log_warn(LOG_COMP_GENERAL,
		         "window_registry: getsystemtablescript(%d) returned false -- "
		         "script hook not configured (normal if windowTypes not loaded)",
		         (int)idscript);
		return;
	}

	/* Step 2: copy the window path into a Pascal bigstring */
	cstr_to_bigstring(window_path, bs_path);

	/*
	 * Step 3: escape the path for safe embedding as a string literal.
	 *
	 * The idsystemtablescripts templates use straight ASCII double-quotes
	 * to delimit the ^0 parameter (e.g. openWindow("^0")).  We must escape
	 * any embedded '"' (0x22) in the path so it cannot break out of the
	 * surrounding string literal.
	 *
	 * langdeparsestring(bs, chendquote) inserts a backslash before every
	 * occurrence of chendquote in bs.  Passing '"' (0x22) ensures that an
	 * embedded double-quote becomes '\"', which the UserTalk scanner reads
	 * as an escaped quote inside the string literal rather than as the
	 * string terminator.
	 *
	 * We do NOT pass chclosecurlyquote (0xD3) here because the templates
	 * use straight ASCII quotes, not curly quotes.  Using 0xD3 would leave
	 * embedded ASCII '"' unescaped and allow injection.
	 *
	 * Example: if window_path were 'a");evil("b', after escaping it becomes
	 * 'a\");evil(\"b', which is the string content a");evil("b to UserTalk,
	 * NOT two statements.
	 */
	langdeparsestring(bs_path, '"');

	/* Step 4: substitute the escaped path as ^0 in the template */
	parsedialogstring(bs_template, bs_path, nil, nil, nil, bs_substituted);

	/* Step 5: run the substituted script synchronously */
	if (!langrunstringnoerror(bs_substituted, bs_result)) {
		/*
		 * Log but do not fail: the windowTypes framework may not be loaded
		 * (e.g. a minimal headless build without Frontier.root), or the
		 * script may reference a path that doesn't exist yet.  In all cases
		 * the REPL should continue.
		 */
		log_warn(LOG_COMP_GENERAL,
		         "window_registry: script for hook %d failed or returned error "
		         "(window path: %s) -- REPL continues",
		         (int)idscript, window_path);
	}
}

/* ------------------------------------------------------------------ */
/*  Public API                                                         */
/* ------------------------------------------------------------------ */

boolean window_registry_init(void) {
	/*
	 * Create system.temp.windowTypes.windows.repl as a table with a "type"
	 * attribute of "ReplWindow".  This lets the windowTypes framework resolve
	 * the window's type entry in Frontier.tools.data.windowTypes.ReplWindow.
	 *
	 * We do this via a short inline UserTalk script rather than raw ODB API
	 * calls because:
	 *   1. The path is multi-level; ODB API calls would need multiple
	 *      hashtablelookupnode + hashtablesetnodevalue calls.
	 *   2. The UserTalk path matches what the framework expects, so if the
	 *      path convention changes we only change it in one place.
	 *   3. The script is idempotent ("if not defined" guards).
	 *
	 * If any part of this fails (e.g. system.temp doesn't exist yet), we log
	 * a warning and return false.  The caller (repl.c) continues -- the REPL
	 * runs without the window sentinel, but the bridge will be a no-op.
	 */
	static const char *init_script =
		"if not defined (system.temp.windowTypes) {"
		"  new (tableType, @system.temp.windowTypes)};"
		"if not defined (system.temp.windowTypes.windows) {"
		"  new (tableType, @system.temp.windowTypes.windows)};"
		"if not defined (system.temp.windowTypes.windows.repl) {"
		"  new (tableType, @system.temp.windowTypes.windows.repl)};"
		/* Store "type" directly as a string value in the window table.
		 * window.attributes.getOne reads from the /atts sibling of the
		 * window node; we also store directly for simpler Phase C testing. */
		"system.temp.windowTypes.windows.repl.type = \"ReplWindow\";"
		"system.temp.windowTypes.windows.repl.title = \"REPL\"";

	bigstring bsprog;
	bigstring bsresult;
	cstr_to_bigstring(init_script, bsprog);

	if (!langrunstringnoerror(bsprog, bsresult)) {
		log_warn(LOG_COMP_GENERAL,
		         "window_registry_init: init script failed -- "
		         "REPL window sentinel not created");
		return false;
	}

	log_debug(LOG_COMP_GENERAL,
	          "window_registry_init: REPL window sentinel created at "
	          WINDOW_BRIDGE_REPL_PATH);
	return true;
}

void on_frontmost_changed(const char *old_path, const char *new_path) {
	/*
	 * Edge case 1: both nil -- no window transitions, nothing to fire.
	 */
	if (old_path == nil && new_path == nil)
		return;

	/*
	 * Edge case 2: same window -- the conceptual "frontmost" did not change
	 * (e.g. a spurious call from the boot sequence when the REPL is already
	 * front and a re-init is triggered).  Do not re-fire open/close.
	 */
	if (old_path != nil && new_path != nil &&
	    strcmp(old_path, new_path) == 0)
		return;

	/*
	 * Close the old window first (matches legacy Frontier ordering in
	 * shellwindow.c: close fires before open on a window-switch event).
	 */
	if (old_path != nil)
		fire_window_script(idclosewindowscript, old_path);

	/*
	 * Open the new window.
	 */
	if (new_path != nil)
		fire_window_script(idopenwindowscript, new_path);
}
