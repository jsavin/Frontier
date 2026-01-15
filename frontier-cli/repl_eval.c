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
extern boolean disposehashtable(hdlhashtable htable, boolean fldisk);
extern boolean newhashtable(hdlhashtable *htable);
extern void disposevaluerecord(tyvaluerecord val, boolean fldisk);

/*
 * REPL Workspace Architecture
 *
 * The REPL workspace is a thread-local, ephemeral hash table for REPL variables.
 *
 * Variables assigned in the REPL (e.g., "x = 42") are stored in this ephemeral
 * table and are:
 * - NOT persisted to database
 * - Thread-local (isolated per REPL session)
 * - Clearable with /clear command
 *
 * Name Resolution:
 * - Simple names (x, y, z) → REPL workspace (ephemeral)
 * - Dotted paths (root.workspace.x, workspace.x) → Database tables (persistent)
 *
 * The /clear command empties ONLY the ephemeral REPL workspace, never touching
 * root.workspace or any other persisted data.
 *
 * See docs/REPL_WORKSPACE_ARCHITECTURE.md for complete architecture details.
 */

/*
 * repl_workspace_init - Initialize ephemeral thread-local workspace
 *
 * Creates thread-local hash table for REPL variables.
 * This table is NOT persisted and is isolated from root.workspace.
 */
boolean repl_workspace_init(repl_workspace *ws) {
    if (ws == NULL) {
        return false;
    }

    /* Initialize struct */
    ws->workspace_table = nil;
    ws->initialized = false;

    /* Create ephemeral thread-local hash table for REPL variables.
     * This table is NOT persisted and is isolated from root.workspace.
     */
    hdlhashtable hnew = nil;

    /* Create new hash table */
    if (!newhashtable(&hnew)) {
        log_error(LOG_COMP_GENERAL, "Failed to create ephemeral REPL workspace table");
        return false;
    }

    /* Configure as local scope (ephemeral, not persisted) */
    (**hnew).fllocaltable = true;      /* Marks as ephemeral */
    (**hnew).prevhashtable = currenthashtable;  /* Chain to previous table for name resolution */
    (**hnew).hashtablerefcon = 0;      /* No database association */

    /* Push onto hash table stack for proper script execution context.
     * pushhashtable() will set currenthashtable = hnew for us.
     */
    if (!pushhashtable(hnew)) {
        disposehashtable(hnew, false);
        log_error(LOG_COMP_GENERAL, "Failed to push REPL workspace onto hash table stack");
        return false;
    }

    /* Save reference in REPL context */
    ws->workspace_table = hnew;
    ws->initialized = true;

    log_debug(LOG_COMP_GENERAL, "REPL workspace initialized (thread-local, ephemeral)");
    return true;
}

/*
 * repl_workspace_cleanup - Clean up ephemeral workspace
 *
 * Disposes the ephemeral workspace table and clears the reference.
 */
void repl_workspace_cleanup(repl_workspace *ws) {
    if (ws == NULL) {
        return;
    }

    /* Pop workspace from hash table stack and dispose */
    if (ws->workspace_table != nil) {
        pophashtable();  /* Remove from stack */
        disposehashtable(ws->workspace_table, false);
    }

    ws->workspace_table = nil;
    ws->initialized = false;
}

/*
 * repl_workspace_clear - Clear all ephemeral workspace variables
 *
 * Removes all entries from the ephemeral workspace table.
 * Refuses to clear non-local tables (safety check to prevent data loss).
 */
boolean repl_workspace_clear(repl_workspace *ws) {
    if (ws == NULL || ws->workspace_table == nil) {
        return false;
    }

    /* SAFETY CHECK: Refuse to clear non-local tables (prevents data loss) */
    if (!(**ws->workspace_table).fllocaltable) {
        log_error(LOG_COMP_GENERAL, "Refusing to clear non-local table (safety check)");
        return false;
    }

    /* DEBUG: Check state before clear */
    log_debug(LOG_COMP_GENERAL, "Before clear: ws->workspace_table=%p, currenthashtable=%p, match=%d, prevhashtable=%p",
              ws->workspace_table, currenthashtable, ws->workspace_table == currenthashtable,
              (**ws->workspace_table).prevhashtable);

    /* CRITICAL FIX: Manually clear workspace table entries WITHOUT callbacks
     * emptyhashtable() triggers callbacks (langsymboldeleted) that corrupt state.
     * We can't dispose/recreate because process stack has saved pointers to this table.
     * Instead, manually clear the hash buckets and sorted list.
     */

    /* Clear sorted list */
    (**ws->workspace_table).hfirstsort = nil;

    /* Clear all hash buckets (ctbuckets is defined in lang.h) */
    #define ctbuckets 11
    for (int i = 0; i < ctbuckets; i++) {
        /* For each node in bucket, dispose it WITHOUT callbacks */
        hdlhashnode nomad = (**ws->workspace_table).hashbucket[i];
        while (nomad != nil) {
            hdlhashnode nextnomad = (**nomad).hashlink;

            /* Dispose node's value data */
            disposevaluerecord((**nomad).val, false);  /* false = local table */

            /* Dispose node itself */
            disposehandle((Handle)nomad);

            nomad = nextnomad;
        }

        /* Clear bucket pointer */
        (**ws->workspace_table).hashbucket[i] = nil;
    }

    /* Re-set currenthashtable to ensure it's correct */
    currenthashtable = ws->workspace_table;

    /* DEBUG: Check state after clear */
    log_debug(LOG_COMP_GENERAL, "After clear: ws->workspace_table=%p, currenthashtable=%p, match=%d, hfirstsort=%p, fllocaltable=%d, prevhashtable=%p",
              ws->workspace_table, currenthashtable, ws->workspace_table == currenthashtable,
              (**ws->workspace_table).hfirstsort, (**ws->workspace_table).fllocaltable,
              (**ws->workspace_table).prevhashtable);

    log_debug(LOG_COMP_GENERAL, "REPL workspace cleared (ephemeral variables only)");
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
    if (*htext == NULL) {
        disposehandle(htext);
        copyctopstring("Handle lock failed", error_msg);
        return false;
    }
    memcpy(*htext, script, script_len);
    HUnlock(htext);

    /* Execute script using langrunhandletraperror
     *
     * IMPORTANT: langrunhandletraperror() CONSUMES the text handle.
     * It disposes htext before returning (both success and error paths).
     * Do NOT access htext after this call.
     *
     * Name resolution:
     * - Simple names (x, y) → currenthashtable (ephemeral REPL workspace)
     * - Dotted paths (root.workspace.x) → Database lookup (persistent)
     *
     * currenthashtable was set in repl_workspace_init() to point to the
     * ephemeral workspace, so variable assignments automatically go there.
     *
     * Returns: result in one param, error in another (separated cleanly)
     */

    /* DEBUG: Check currenthashtable BEFORE script execution */
    log_debug(LOG_COMP_GENERAL, "BEFORE langrun: workspace_table=%p, currenthashtable=%p, match=%d, fllocal=%d",
              ws->workspace_table, currenthashtable, ws->workspace_table == currenthashtable,
              (**currenthashtable).fllocaltable);

    /* CRITICAL WORKAROUND: Force workspace onto hash table stack
     * langrunhandletraperror() does pushprocess/popprocess which restores old hashtablestack.
     * This can leave workspace table OFF the stack, causing assignments to go elsewhere.
     * Force workspace back onto stack before evaluation.
     */
    pophashtable();  /* Remove whatever's on top */
    pushhashtable(ws->workspace_table);  /* Put workspace on top */
    currenthashtable = ws->workspace_table;  /* Ensure currenthashtable is correct */
    log_debug(LOG_COMP_GENERAL, "FORCED workspace onto stack: %p", currenthashtable);

    boolean ok = langrunhandletraperror(htext, result, error_msg);

    /* DEBUG: Check currenthashtable AFTER script execution */
    log_debug(LOG_COMP_GENERAL, "AFTER langrun: workspace_table=%p, currenthashtable=%p, match=%d",
              ws->workspace_table, currenthashtable, ws->workspace_table == currenthashtable);

    /* CRITICAL: Restore correct hash table state after langrun
     * langrunhandletraperror may have left hash table stack in inconsistent state.
     * Force workspace back as current table.
     */
    currenthashtable = ws->workspace_table;

    /* DEBUG: Check workspace state after script execution */
    log_debug(LOG_COMP_GENERAL, "After script eval: workspace_table=%p, currenthashtable=%p, hfirstsort=%p, fllocaltable=%d",
              ws->workspace_table, currenthashtable, (**ws->workspace_table).hfirstsort, (**ws->workspace_table).fllocaltable);

    if (!ok) {
        /* Execution failed - error message is in error_msg parameter */
        if (stringlength(error_msg) == 0) {
            copyctopstring("Script execution failed", error_msg);
        }
        return false;
    }

    /* Success - result is already set by langrunhandletraperror */
    return true;
}
