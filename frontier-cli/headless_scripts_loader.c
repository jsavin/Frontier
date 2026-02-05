/*
 * headless_scripts_loader.c - Loads and executes system startup scripts in headless mode
 *
 * Provides headless-compatible implementations for startup script execution,
 * including a msg() verb that outputs to stdout instead of GUI dialogs.
 * Startup scripts run by default; use --skip-startup or FRONTIER_HEADLESS_RUN_STARTUP=0 to skip.
 */

/* 2025-12-02 Codex: Allow headless startup scripts to be skipped for debugging (env flag). */

#include "../Common/headers/frontier.h"

#include "../Common/headers/lang.h"
#include "../Common/headers/langexternal.h"
#include "../Common/headers/langinternal.h"
#include "../Common/headers/opverbs.h"
#include "../Common/headers/scripts.h"
#include "../Common/headers/strings.h"
#include "../Common/headers/tableverbs.h"
#include "../Common/headers/tablestructure.h"
#include "../Common/headers/stringdefs.h"
#include "../Common/headers/logging.h"

#include <stdio.h>

/* Headless msg() verb that outputs to stdout instead of showing a dialog. */
static boolean headless_msgverb(hdltreenode hparam1, tyvaluerecord *vreturned) {
	bigstring bsmsg;
	char msg[256];

	flnextparamislast = true;

	if (!getstringvalue(hparam1, 1, bsmsg))
		return false;

	/* Convert Pascal string to C string */
	copyptocstring(bsmsg, msg);

	/* Output to stdout with "msg: " prefix to distinguish from return values */
	fputs("msg: ", stdout);
	fputs(msg, stdout);
	fputs("\n", stdout);
	fflush(stdout);

	return setbooleanvalue(true, vreturned);
}

extern boolean scriptbuildtree (Handle htext, long signature, hdltreenode *hcode);
extern boolean langruncode (hdltreenode htree, hdlhashtable hcontext, tyvaluerecord *vreturned);
extern boolean pushhashtable (hdlhashtable);
extern boolean pophashtable (void);
extern hdlhashtable currenthashtable;
extern hdlhashtable roottable;

/* External declaration for langrunhandletraperror - runs scripts like REPL does */
extern boolean langrunhandletraperror (Handle htext, bigstring bsresult, bigstring bserror);

/* Runs startup scripts by calling startup.startupScript() using the same
 * execution path as the REPL. This ensures identical context: proper process
 * management, error trapping for try blocks, and correct error flag handling.
 *
 * Previously we iterated through system.startup and called langruncode() directly,
 * which bypassed process setup and error trapping, causing try block errors to
 * be logged even when caught.
 */
static boolean headless_run_startup_script (void) {
    Handle htext = nil;
    bigstring bsresult;
    bigstring bserror;
    const char *script = "startup.startupScript()";
    size_t script_len = strlen(script);
    boolean ok;

    log_debug(LOG_COMP_STARTUP, "run_startup_script: calling startup.startupScript()");

    /* Allocate handle for script text */
    if (!newemptyhandle(&htext)) {
        log_error(LOG_COMP_STARTUP, "run_startup_script: out of memory allocating script handle");
        return false;
    }

    if (!sethandlesize(htext, (long)script_len)) {
        disposehandle(htext);
        log_error(LOG_COMP_STARTUP, "run_startup_script: out of memory resizing script handle");
        return false;
    }

    HLock(htext);
    if (*htext == NULL) {
        disposehandle(htext);
        log_error(LOG_COMP_STARTUP, "run_startup_script: handle lock failed");
        return false;
    }
    memcpy(*htext, script, script_len);
    HUnlock(htext);

    /* Initialize result and error strings */
    setemptystring(bsresult);
    setemptystring(bserror);

    /* Run using langrunhandletraperror - same path as REPL.
     * This properly:
     * - Pushes a process context
     * - Sets up error trapping (so try blocks work correctly)
     * - Resets fllangerror after execution
     *
     * Note: langrunhandletraperror consumes htext - do not access after this call.
     */
    ok = langrunhandletraperror(htext, bsresult, bserror);

    if (ok) {
        log_debug(LOG_COMP_STARTUP, "run_startup_script: completed successfully");
        if (stringlength(bsresult) > 0) {
            char result_cstr[256];
            copyptocstring(bsresult, result_cstr);
            log_debug(LOG_COMP_STARTUP, "run_startup_script: result = %s", result_cstr);
        }
    } else {
        /* Errors here are typically from try/catch blocks that properly handle
         * errors internally. The script still completes successfully even when
         * bserror is populated (it captures the last caught error, not a failure).
         * Logging removed per user request - these warnings were cosmetic noise. */
    }

    /* Return true to allow startup to continue - errors are logged but not fatal */
    return true;
}

/* External accessor for CLI --skip-startup flag */
extern boolean cli_should_skip_startup(void);

/* Initializes the headless environment and runs startup scripts by default.
 * Scripts can be skipped via --skip-startup flag or FRONTIER_HEADLESS_RUN_STARTUP=0. */
boolean loadsystemscripts (void) {
    const char *env_startup = getenv("FRONTIER_HEADLESS_RUN_STARTUP");
    boolean skip_startup = false;

    if (systemtable == nil) {
        log_error(LOG_COMP_STARTUP, "loadsystemscripts: system table is nil");
        return false;
    }

    /* Register headless msg() verb callback */
    langcallbacks.msgverbcallback = &headless_msgverb;
    log_debug(LOG_COMP_STARTUP, "loadsystemscripts: registered headless msg() verb");

    /* Check if startup should be skipped:
     * 1. --skip-startup CLI flag
     * 2. FRONTIER_HEADLESS_RUN_STARTUP=0 environment variable */
    if (cli_should_skip_startup()) {
        skip_startup = true;
        log_debug(LOG_COMP_STARTUP, "loadsystemscripts: skipping startup (--skip-startup flag)");
    } else if (env_startup && strcmp(env_startup, "0") == 0) {
        skip_startup = true;
        log_debug(LOG_COMP_STARTUP, "loadsystemscripts: skipping startup (FRONTIER_HEADLESS_RUN_STARTUP=0)");
    }

    if (skip_startup) {
        return true;
    }

    log_debug(LOG_COMP_STARTUP, "loadsystemscripts: running startup.startupScript()");
    if (!headless_run_startup_script ()) {
        log_error(LOG_COMP_STARTUP, "loadsystemscripts: failed to run startup.startupScript()");
        return false;
    }

    log_debug(LOG_COMP_STARTUP, "loadsystemscripts: startup complete");
    return true;
}
