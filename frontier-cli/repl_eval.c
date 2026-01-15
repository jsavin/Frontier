/*
 * repl_eval.c - REPL evaluation engine implementation
 *
 * Part of Frontier REPL interactive mode (Phase 1).
 * Implements QuickScript model: each evaluation runs independently with thread cleanup.
 *
 * Reference: planning/phase4/REPL_INTERACTIVE_MODE_DESIGN.md
 *
 * Created: 2026-01-13
 * Updated: 2026-01-14 - Refactored to QuickScript model (no workspace, no persistence)
 */

#include "repl_eval.h"
#include "../Common/headers/lang.h"
#include "../Common/headers/strings.h"
#include "../Common/headers/memory.h"
#include "../Common/headers/logging.h"

/*
 * REPL QuickScript Architecture
 *
 * The REPL follows the QuickScript model from legacy Frontier:
 * - Each Enter press runs code in its own thread
 * - Local variables are thread-scoped and cleaned up after evaluation
 * - No implicit persistence between evaluations
 * - Users who want persistence use explicit database paths
 *
 * Variable Persistence Scopes:
 * - Local variables (x = 5): Evaluation-scoped, cleaned up immediately
 * - system.temp.* : Session-scoped, persists across evaluations, cleared on exit
 * - workspace.* or other root tables: Disk-scoped, saved with database
 *
 * This is how legacy Frontier's QuickScript window worked. Let Frontier be Frontier.
 *
 * See docs/CLI_USAGE_GUIDE.md for user-facing persistence documentation.
 */

/*
 * repl_eval_script - Evaluate UserTalk script (QuickScript model)
 *
 * Compiles and executes the script. Each evaluation runs independently.
 * Local variables don't persist - they're cleaned up after evaluation returns.
 *
 * For persistent data, users should use explicit database paths:
 * - system.temp.x (session-scoped)
 * - workspace.x (disk-scoped)
 *
 * Parameters:
 *   script     - UserTalk script to execute (null-terminated C string)
 *   result     - OUT: Result as string (bigstring)
 *   error_msg  - OUT: Error message if execution failed (bigstring)
 *
 * Returns: true if evaluation succeeded, false on error
 *
 * On success: result contains string representation of return value (may be empty)
 * On error: error_msg contains error description
 */
boolean repl_eval_script(
    const char *script,
    bigstring result,
    bigstring error_msg
) {
    if (script == NULL || result == NULL || error_msg == NULL) {
        if (error_msg != NULL) {
            copyctopstring("Invalid parameters", error_msg);
        }
        return false;
    }

    /* Initialize result and error */
    setemptystring(result);
    setemptystring(error_msg);

    /* Convert script to Handle */
    size_t script_len = strlen(script);
    Handle htext = nil;

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
    if (*htext == NULL) {
        disposehandle(htext);
        copyctopstring("Handle lock failed", error_msg);
        return false;
    }
    memcpy(*htext, script, script_len);
    HUnlock(htext);

    /* Execute script using langrunhandletraperror
     *
     * QuickScript Model:
     * - Each evaluation runs in its own thread context
     * - pushprocess(nil)/popprocess() handle thread lifecycle
     * - Local variables are thread-scoped and cleaned up automatically
     * - No workspace mechanism needed - let Frontier be Frontier
     *
     * IMPORTANT: langrunhandletraperror() CONSUMES the text handle.
     * It disposes htext before returning (both success and error paths).
     * Do NOT access htext after this call.
     *
     * Name resolution:
     * - Simple names (x, y) → Thread-local, cleaned up after evaluation
     * - Dotted paths (system.temp.x, workspace.x) → Database tables (persistent)
     *
     * Returns: result in one param, error in another (separated cleanly)
     */

    log_debug(LOG_COMP_GENERAL, "Evaluating script (QuickScript model - thread-local execution)");

    boolean ok = langrunhandletraperror(htext, result, error_msg);

    log_debug(LOG_COMP_GENERAL, "Script evaluation %s", ok ? "succeeded" : "failed");

    return ok;
}
