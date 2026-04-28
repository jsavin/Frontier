/*
    repl_variables.c - REPL persistent variable management

    Manages system.temp.FrontierREPL for persistent REPL state:
    - variables: Variables that persist across REPL evaluations
    - target: Current target address (for target.set()/clear())
    - focus: Mirrors /jump location, exposed to UserTalk
    - commands: Custom slash command scripts

    Implementation uses a callback hook in langpushlocalchain:
    When the `with` statement's local table is about to be disposed,
    we sync any new/modified variables to the persistent table.

    SPDX-License-Identifier: MIT

    Copyright (c) 1992-2004 UserLand Software, Inc.
    Copyright (c) 2025-present Frontier contributors

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

#include <stdio.h>
#include <string.h>

#include "repl_variables.h"
#include "../Common/headers/lang.h"
#include "../Common/headers/langinternal.h"
#include "../Common/headers/strings.h"
#include "../Common/headers/memory.h"
#include "../Common/headers/logging.h"
#include "../Common/headers/tablestructure.h"
#include "../Common/headers/langexternal.h"

/* Name constants for our REPL tables (Pascal strings: length byte + chars) */
static byte namerepltable[] = "\x0c" "FrontierREPL";	 /* "FrontierREPL" (12 chars) */
static byte namevariables[] = "\x09" "variables";		  /* "variables" (9 chars) */
static byte namecommands[] = "\x08" "commands";			  /* "commands" (8 chars) */
static byte nametemptable[] = "\x04" "temp";			  /* "temp" (4 chars) */
static byte nametarget[] = "\x06" "target";				  /* "target" (6 chars) */
static byte namefocus[] = "\x05" "focus";				  /* "focus" (5 chars) */

/* Cached handle to our tables for quick access */
static hdlhashtable g_repl_variables_table = nil;
static hdlhashtable g_repl_table = nil;

/* Flag to track when we're in a REPL eval - only sync during REPL evals */
static boolean g_repl_eval_active = false;

/* Forward declaration for table lookup */
extern boolean findnamedtable(hdlhashtable htable, bigstring bs, hdlhashtable *hnamedtable);

/*
 * Create a subtable if it doesn't exist.
 * Returns true if table exists or was created.
 */
static boolean ensure_subtable(hdlhashtable parent, byte *name, hdlhashtable *result) {
	if (findnamedtable(parent, name, result)) {
		return true;  /* Already exists */
	}

	/* Create it */
	if (!tablenewsubtable(parent, name, result)) {
		log_error(LOG_COMP_GENERAL, "Failed to create REPL subtable: %.*s",
				  (int)name[0], name + 1);
		return false;
	}

	log_debug(LOG_COMP_GENERAL, "Created REPL subtable: %.*s",
			  (int)name[0], name + 1);
	return true;
}

/*
 * Callback to sync variables from the with-block's local table
 * to the persistent variables table before disposal.
 *
 * FLOW DIAGRAM:
 *	 1. User enters code in REPL (e.g., "x = 42")
 *	 2. repl_eval_with_variables() wraps it: "with system.temp.FrontierREPL.variables { x = 42 }"
 *	 3. Sets g_repl_eval_active = true
 *	 4. langrunhandletraperror() evaluates the wrapped script
 *	 5. The `with` statement creates a local table with references to variables table
 *	 6. User's code executes, creating/modifying variables in the local table
 *	 7. When `with` block ends, langpoplocalchain() is called
 *	 8. langpoplocalchain() invokes this callback with the local table
 *	 9. We copy new/modified variables to system.temp.FrontierREPL.variables
 *	10. Local table is disposed (but variables now persisted)
 *	11. g_repl_eval_active = false
 *
 * This is called by langpoplocalchain just before disposing the local table.
 * We only sync when g_repl_eval_active is true (we're in a REPL eval).
 */
static void repl_sync_variables_callback(hdlhashtable hlocals) {
	/* Only sync during REPL evals, not for every script execution */
	if (!g_repl_eval_active) {
		return;
	}

	log_trace(LOG_COMP_GENERAL, "repl_sync_variables_callback called with hlocals=%p, eval_active=%d",
			  (void*)hlocals, g_repl_eval_active);

	if (hlocals == nil) {
		log_trace(LOG_COMP_GENERAL, "  hlocals is nil, returning");
		return;
	}

	if (g_repl_variables_table == nil) {
		log_trace(LOG_COMP_GENERAL, "  g_repl_variables_table is nil, returning");
		return;
	}

	/* Check if this is a local table (we only want to sync from local tables) */
	if (!(**hlocals).fllocaltable) {
		log_trace(LOG_COMP_GENERAL, "  not a local table, returning");
		return;
	}

	log_debug(LOG_COMP_GENERAL, "Syncing variables from hlocals=%p to persistent=%p",
			  (void*)hlocals, (void*)g_repl_variables_table);

	/* Walk all entries in the local table and copy to persistent table.
	 * Skip the "with" reference entries (address values pointing to tables). */
	hdlhashnode h;
	int count = 0;

	for (h = (**hlocals).hfirstsort; h != nil; h = (**h).sortedlink) {
		bigstring bsname;
		tyvaluerecord val;

		gethashkey(h, bsname);
		val = (**h).val;

		/* Skip the "with" reference entries - they're address values
		 * with names like "with1", "with2" etc. that point to tables */
		if (bsname[0] >= 4 &&
			bsname[1] == 'w' && bsname[2] == 'i' &&
			bsname[3] == 't' && bsname[4] == 'h') {
			log_trace(LOG_COMP_GENERAL, "Skipping with-reference: %.*s",
					  (int)bsname[0], bsname + 1);
			continue;
		}

		/* Skip code values (functions/scripts) - they require special handling
		 * to copy correctly and trigger assertion failures in langunpacktree.
		 * TODO: Support function persistence in a future update. */
		if (val.valuetype == codevaluetype) {
			log_trace(LOG_COMP_GENERAL, "Skipping code value: %.*s (function persistence not yet supported)",
					  (int)bsname[0], bsname + 1);
			continue;
		}

		/* Copy value to persistent table */
		tyvaluerecord valcopy;
		if (!copyvaluerecord(val, &valcopy)) {
			log_warn(LOG_COMP_GENERAL, "Failed to copy value for variable %.*s",
					 (int)bsname[0], bsname + 1);
			continue;
		}

		/* Assign to persistent table */
		pushhashtable(g_repl_variables_table);
		boolean fl = hashassign(bsname, valcopy);
		pophashtable();

		if (fl) {
			/* Remove from tmpstack since it's now owned by the hash table */
			exemptfromtmpstack(&valcopy);
			count++;
			log_debug(LOG_COMP_GENERAL, "Synced variable %.*s to persistent storage",
					  (int)bsname[0], bsname + 1);
		} else {
			/* Assignment failed - need to dispose the copy */
			disposevaluerecord(valcopy, false);
			log_warn(LOG_COMP_GENERAL, "Failed to assign variable %.*s",
					 (int)bsname[0], bsname + 1);
		}
	}

	if (count > 0) {
		log_info(LOG_COMP_GENERAL, "Synced %d variable(s) to persistent storage", count);
	}
}

boolean repl_variables_init(void) {
	hdlhashtable htemp = nil;
	hdlhashtable hrepl = nil;
	hdlhashtable hvars = nil;
	hdlhashtable hcommands = nil;

	log_debug(LOG_COMP_GENERAL, "Initializing REPL variables subsystem");

	/* Find system.temp - required for persistent variables */
	if (systemtable == nil) {
		log_warn(LOG_COMP_GENERAL, "systemtable is nil, REPL variables will not persist");
		return false;
	}

	if (!findnamedtable(systemtable, nametemptable, &htemp)) {
		log_warn(LOG_COMP_GENERAL, "system.temp not found, REPL variables will not persist");
		return false;
	}

	log_debug(LOG_COMP_GENERAL, "Found system.temp at %p", (void*)htemp);

	/* Create system.temp.FrontierREPL if it doesn't exist */
	if (!ensure_subtable(htemp, namerepltable, &hrepl)) {
		return false;
	}
	g_repl_table = hrepl;

	/* Create the subtables - only variables and commands are tables */
	if (!ensure_subtable(hrepl, namevariables, &hvars)) {
		return false;
	}
	g_repl_variables_table = hvars;

	if (!ensure_subtable(hrepl, namecommands, &hcommands)) {
		return false;
	}

	/* Initialize target to nil (no target set) */
	{
		tyvaluerecord nilval;
		initvalue(&nilval, novaluetype);
		pushhashtable(hrepl);
		if (!hashassign(nametarget, nilval)) {
			pophashtable();
			log_warn(LOG_COMP_GENERAL, "Failed to initialize REPL target");
		} else {
			pophashtable();
		}
	}

	/* Initialize focus to @root (start at root table) */
	{
		tyvaluerecord focusval;
		if (setaddressvalue(roottable, zerostring, &focusval)) {
			pushhashtable(hrepl);
			if (!hashassign(namefocus, focusval)) {
				pophashtable();
				disposevaluerecord(focusval, false);
				log_warn(LOG_COMP_GENERAL, "Failed to initialize REPL focus");
			} else {
				pophashtable();
				exemptfromtmpstack(&focusval);
			}
		}
	}

	log_info(LOG_COMP_GENERAL, "REPL variables initialized: variables=%p, commands=%p",
			 (void*)hvars, (void*)hcommands);

	/* Install the callback for syncing variables */
	langmagictabledisposecallback = repl_sync_variables_callback;

	log_debug(LOG_COMP_GENERAL, "Installed REPL callback: %p", (void*)langmagictabledisposecallback);

	return true;
}

void repl_variables_cleanup(void) {
	/* Remove the callback */
	langmagictabledisposecallback = nil;

	log_debug(LOG_COMP_GENERAL, "REPL variables cleanup");
	g_repl_variables_table = nil;
	g_repl_table = nil;
}

hdlhashtable repl_get_variables_table(void) {
	return g_repl_variables_table;
}

/*
 * Build a wrapped script that brings variables into scope.
 *
 * Transforms:
 *	 user code
 * Into:
 *	 with system.temp.FrontierREPL.variables { user code }
 *
 * NOTE: We previously included target.set(@system.temp.FrontierREPL.variables)
 * but calling target.set() multiple times crashes due to a bug in target verb
 * implementation. See issue for target.set() double-call crash.
 */
static boolean build_wrapped_script(const char *script, Handle *hresult) {
	static const char prefix[] =
		"with system.temp.FrontierREPL.variables {\n";
	static const char suffix[] = "\n}";

	size_t script_len = strlen(script);
	size_t prefix_len = sizeof(prefix) - 1;
	size_t suffix_len = sizeof(suffix) - 1;
	size_t total_len = prefix_len + script_len + suffix_len;

	Handle htext = nil;

	if (!newemptyhandle(&htext)) {
		return false;
	}

	if (!sethandlesize(htext, (long)total_len)) {
		disposehandle(htext);
		return false;
	}

	HLock(htext);
	char *p = (char *) *htext;
	memcpy(p, prefix, prefix_len);
	p += prefix_len;
	memcpy(p, script, script_len);
	p += script_len;
	memcpy(p, suffix, suffix_len);
	HUnlock(htext);

	*hresult = htext;
	return true;
}

boolean repl_eval_with_variables(
	const char *script,
	bigstring result,
	bigstring error_msg
) {
	Handle htext = nil;

	if (script == NULL || result == NULL || error_msg == NULL) {
		if (error_msg != NULL) {
			copyctopstring("Invalid parameters", error_msg);
		}
		return false;
	}

	/* Initialize result and error */
	setemptystring(result);
	setemptystring(error_msg);

	/* If variables table not initialized, fall back to regular eval */
	log_trace(LOG_COMP_GENERAL, "repl_eval_with_variables called, g_repl_variables_table=%p", (void*)g_repl_variables_table);
	if (g_repl_variables_table == nil) {
		log_warn(LOG_COMP_GENERAL, "REPL variables not initialized, using direct eval");

		/* Regular evaluation without wrapping */
		size_t script_len = strlen(script);

		if (!newemptyhandle(&htext)) {
			copyctopstring("Out of memory allocating script handle", error_msg);
			return false;
		}

		if (!sethandlesize(htext, (long)script_len)) {
			disposehandle(htext);
			copyctopstring("Out of memory resizing script handle", error_msg);
			return false;
		}

		HLock(htext);
		memcpy(*htext, script, script_len);
		HUnlock(htext);

		return langrunhandletraperror(htext, result, error_msg);
	}

	/* Build wrapped script: with system.temp.FrontierREPL.variables { ... } */
	if (!build_wrapped_script(script, &htext)) {
		copyctopstring("Failed to wrap script", error_msg);
		return false;
	}

	/* Log the wrapped script for debugging */
	HLock(htext);
	log_trace(LOG_COMP_GENERAL, "Wrapped script (%ld bytes): %.*s",
			  GetHandleSize(htext), (int)GetHandleSize(htext), *htext);
	HUnlock(htext);

	log_debug(LOG_COMP_GENERAL, "Evaluating script with persistent variables");

	/*
	 * Execute the wrapped script.
	 *
	 * Flow:
	 * 1. `with` statement evaluates, creating a local table
	 * 2. Code runs, creating variables in the local table
	 * 3. `with` block ends, langpoplocalchain is called
	 * 4. Our callback syncs variables from the local table to persistent
	 * 5. Local table is disposed
	 *
	 * IMPORTANT: langrunhandletraperror CONSUMES htext.
	 */
	g_repl_eval_active = true;	/* Enable callback syncing */

	boolean ok = langrunhandletraperror(htext, result, error_msg);

	g_repl_eval_active = false;	 /* Disable callback syncing */

	log_trace(LOG_COMP_GENERAL, "Script evaluation %s", ok ? "succeeded" : "failed");

	return ok;
}

boolean repl_eval_with_variables_value(
	const char *script,
	tyvaluerecord *vreturned,
	bigstring error_msg
) {
	Handle htext = nil;

	if (script == NULL || vreturned == NULL || error_msg == NULL) {
		if (error_msg != NULL) {
			copyctopstring("Invalid parameters", error_msg);
		}
		return false;
	}

	/* Initialize result and error */
	initvalue(vreturned, novaluetype);
	setemptystring(error_msg);

	/* If variables table not initialized, fall back to regular eval */
	if (g_repl_variables_table == nil) {
		log_warn(LOG_COMP_GENERAL, "REPL variables not initialized, using direct eval");

		/* Regular evaluation without wrapping */
		size_t script_len = strlen(script);

		if (!newemptyhandle(&htext)) {
			copyctopstring("Out of memory allocating script handle", error_msg);
			return false;
		}

		if (!sethandlesize(htext, (long)script_len)) {
			disposehandle(htext);
			copyctopstring("Out of memory resizing script handle", error_msg);
			return false;
		}

		HLock(htext);
		memcpy(*htext, script, script_len);
		HUnlock(htext);

		return langrunhandle_value(htext, vreturned);
	}

	/* Build wrapped script: with system.temp.FrontierREPL.variables { ... } */
	if (!build_wrapped_script(script, &htext)) {
		copyctopstring("Failed to wrap script", error_msg);
		return false;
	}

	/* Use langruntraperror which returns value and traps errors */
	g_repl_eval_active = true;

	boolean ok = langruntraperror(htext, vreturned, error_msg);

	g_repl_eval_active = false;

	return ok;
}

void repl_set_focus(hdlhashtable htable) {
	/*
	 * Update system.temp.FrontierREPL.focus to point to the given table.
	 *
	 * We create an address value pointing to the table itself (with empty name)
	 * rather than trying to reconstruct the path. This avoids calling
	 * langexpandtodotparams which can corrupt interpreter state.
	 *
	 * We use setexemptaddressvalue instead of setaddressvalue to avoid
	 * interacting with the tmpstack, which can cause issues when called
	 * from REPL command handlers.
	 *
	 * CONTEXT GUARD NOTE (per docs/ARCHITECTURAL_ANTIPATTERNS.md):
	 * We do NOT need context guards here because:
	 * 1. We're not navigating through addresses (no langgetdotparams/langsymbolreference)
	 * 2. We're directly assigning a table handle we already have
	 * 3. setexemptaddressvalue() creates a simple address value without evaluation
	 * 4. hashassign() just stores the value - no interpreter state changes
	 * This function is called from REPL command handlers (/jump) outside of
	 * script evaluation context, so there's no mode stack to corrupt.
	 */
	log_debug(LOG_COMP_GENERAL, "repl_set_focus: htable=%p", (void*)htable);

	if (g_repl_table == nil) {
		log_warn(LOG_COMP_GENERAL, "repl_set_focus: REPL table not initialized");
		return;
	}

	/* Use roottable if htable is nil */
	if (htable == nil) {
		htable = roottable;
	}

	/* Create address value pointing to this table.
	 * Use setexemptaddressvalue to avoid tmpstack interaction.
	 * This creates a direct reference without evaluating paths. */
	tyvaluerecord focusval;
	if (!setexemptaddressvalue(htable, zerostring, &focusval)) {
		log_warn(LOG_COMP_GENERAL, "repl_set_focus: failed to create address");
		return;
	}

	/* Store in system.temp.FrontierREPL.focus */
	pushhashtable(g_repl_table);
	boolean ok = hashassign(namefocus, focusval);
	pophashtable();

	if (!ok) {
		disposevaluerecord(focusval, false);
		log_warn(LOG_COMP_GENERAL, "repl_set_focus: failed to assign focus");
	} else {
		log_debug(LOG_COMP_GENERAL, "repl_set_focus: focus set to table %p", (void*)htable);
	}
}
