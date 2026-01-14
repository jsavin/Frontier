/*
 * repl_eval.c - REPL evaluation engine implementation
 *
 * Part of Frontier REPL interactive mode (Phase 1).
 * Manages workspace table and executes UserTalk scripts.
 *
 * Reference: planning/phase4/REPL_INTERACTIVE_MODE_DESIGN.md
 *
 * Created: 2026-01-13
 */

#include "repl_eval.h"
#include "../Common/headers/lang.h"
#include "../Common/headers/strings.h"
#include "../Common/headers/memory.h"
#include "../Common/headers/tablestructure.h"
#include "../Common/headers/tableverbs.h"
#include "../Common/headers/logging.h"

/* External globals */
extern hdlhashtable systemtable;
extern hdlhashtable currenthashtable;
extern hdlhashtable roottable;

/* Forward declarations for internal functions */
extern short emptyhashtable(hdlhashtable htable, boolean fldisk);

/*
 * repl_workspace_init - Initialize workspace table
 *
 * Creates root.workspace table for REPL variables.
 * When system.paths is loaded, workspace.x resolves to root.workspace.x
 * The workspace persists across all REPL evaluations.
 */
boolean repl_workspace_init(repl_workspace *ws) {
    if (ws == NULL) {
        return false;
    }

    /* Initialize struct */
    ws->workspace_table = nil;
    ws->initialized = false;

    /* Check if system root is loaded */
    if (roottable == nil) {
        log_error(LOG_COMP_GENERAL, "Root table not loaded - cannot create workspace");
        return false;
    }

    /* Find or create root.workspace table
     *
     * IMPORTANT: root.workspace usually already exists in the database with user data.
     * We use findnamedtable() first to find the existing table - only create if missing.
     * This preserves any existing data in root.workspace (like notepad, pt, etc.)
     *
     * Normal name resolution will find workspace.x as root.workspace.x
     */
    hdlhashtable workspace = nil;
    bigstring bs_workspace;
    copyctopstring("workspace", bs_workspace);

    if (!findnamedtable(roottable, bs_workspace, &workspace)) {
        /* Table doesn't exist - safe to create new one */
        if (!tablenewsubtable(roottable, bs_workspace, &workspace)) {
            log_error(LOG_COMP_GENERAL, "Failed to create root.workspace table");
            return false;
        }
    }

    ws->workspace_table = workspace;
    ws->initialized = true;

    log_debug(LOG_COMP_GENERAL, "REPL workspace initialized as root.workspace");
    return true;
}

/*
 * repl_workspace_cleanup - Clean up workspace
 *
 * Does NOT dispose the workspace table (it's part of system root).
 * Just marks as uninitialized.
 */
void repl_workspace_cleanup(repl_workspace *ws) {
    if (ws == NULL) {
        return;
    }

    ws->workspace_table = nil;
    ws->initialized = false;
}

/*
 * repl_workspace_clear - Clear all workspace variables
 *
 * Removes all entries from workspace table using emptyhashtable().
 */
boolean repl_workspace_clear(repl_workspace *ws) {
    if (ws == NULL || ws->workspace_table == nil) {
        return false;
    }

    /* Clear all entries from workspace table */
    emptyhashtable(ws->workspace_table, false);

    log_debug(LOG_COMP_GENERAL, "REPL workspace cleared");
    return true;
}

/*
 * repl_eval_script - Evaluate UserTalk script in workspace context
 *
 * Compiles and executes the script with workspace as current table context.
 * Variables declared in the script are stored in workspace.
 */
boolean repl_eval_script(
    repl_workspace *ws,
    const char *script,
    bigstring result,
    bigstring error_msg
) {
    if (ws == NULL || script == NULL || result == NULL || error_msg == NULL) {
        if (error_msg != NULL) {
            copyctopstring("Invalid parameters", error_msg);
        }
        return false;
    }

    /* Check workspace is initialized */
    if (!ws->initialized || ws->workspace_table == nil) {
        copyctopstring("Workspace not initialized", error_msg);
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
    memcpy(*htext, script, script_len);
    HUnlock(htext);

    /* Execute script using langrunhandle
     * Note: Name resolution will find workspace.x as root.workspace.x
     * Don't set currenthashtable - let normal lookup work
     * Note: langrunhandle returns the result OR error message in the result parameter.
     * On failure, the result parameter contains the error message.
     */
    boolean ok = langrunhandle(htext, result);

    if (!ok) {
        /* Execution failed - error message is already in result parameter */
        copystring(result, error_msg);
        setemptystring(result);
        if (stringlength(error_msg) == 0) {
            copyctopstring("Script execution failed", error_msg);
        }
        return false;
    }

    /* Success - result is already set by langrunhandle */
    return true;
}
