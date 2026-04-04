/*
 * debug_handler.c - Protocol-based UserTalk debugger (Phase 1 MVP)
 *
 * Implements the debugger callback and protocol operations for headless
 * script debugging. Replaces the no-op debugger callback in langstartup.c
 * with a protocol-aware callback that can suspend execution and wait for
 * client commands via the NDJSON protocol.
 *
 * Threading model: debug/run spawns the script on a new thread. When the
 * debugger callback fires, the thread suspends via processsleep (yielding
 * the GIL). The protocol handler on the main thread processes debug/continue,
 * debug/kill, etc. and sets flags that the suspended thread checks.
 *
 * See planning/phase6/USERTALK_DEBUGGER_PLAN.md for full design.
 */

#include "debug_handler.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "../Common/headers/frontier.h"
#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "langinternal.h"
#include "logging.h"
#include "processinternal.h"
#include "threadregistry.h"
#include "headless_threading.h"
#include "../third_party/cJSON/cJSON.h"

/* Global: current hashtable and table stack (thread globals) */
extern hdlhashtable currenthashtable;
extern hdltablestack hashtablestack;

/*
 * Maximum number of concurrent debug threads.
 * Each slot holds a pointer to a debug state; NULL = unused.
 */
#define MAX_DEBUG_THREADS 16
static tydebugstate *g_debug_threads[MAX_DEBUG_THREADS] = {0};
static pthread_mutex_t g_debug_mutex = PTHREAD_MUTEX_INITIALIZER;
static atomic_bool g_debug_thread_was_killed = false; /* set when a debug thread is killed mid-execution */

/* ========================================================================
 * Reason string conversion
 * ======================================================================== */

const char *debug_reason_string(debug_suspend_reason_t reason) {
    switch (reason) {
        case DEBUG_REASON_ENTRY:       return "entry";
        case DEBUG_REASON_INTERRUPTED: return "interrupted";
        case DEBUG_REASON_BREAKPOINT:  return "breakpoint";
        case DEBUG_REASON_STEP:        return "step";
        case DEBUG_REASON_ERROR:       return "error";
        default:                       return "unknown";
    }
}

/* ========================================================================
 * Debug state management
 * ======================================================================== */

/*
 * Look up debug state by thread ID.
 *
 * Ownership invariant: the debug thread owns its tydebugstate and frees it
 * only after sending the debug/completed notification and unregistering.
 * Protocol handlers must not access the returned pointer after receiving
 * debug/completed for that thread. The atomic flags provide safe cross-thread
 * communication while the struct is alive.
 */
static tydebugstate *debug_get_state_for_thread(long threadid) {

    pthread_mutex_lock(&g_debug_mutex);

    for (int i = 0; i < MAX_DEBUG_THREADS; i++) {
        if (g_debug_threads[i] != NULL && g_debug_threads[i]->threadid == threadid) {
            tydebugstate *state = g_debug_threads[i];
            atomic_fetch_add(&state->refcount, 1); /* caller borrows a reference */
            pthread_mutex_unlock(&g_debug_mutex);
            return state;
        }
    }

    pthread_mutex_unlock(&g_debug_mutex);
    return NULL;
}

static tydebugstate *debug_register_thread(long threadid, transport_t *transport) {

    tydebugstate *state = (tydebugstate *)calloc(1, sizeof(tydebugstate));
    if (state == NULL)
        return NULL;

    state->fldebugmode = true;
    atomic_store(&state->flsuspended, false);
    atomic_store(&state->flinterrupt, false);
    atomic_store(&state->flkill, false);
    atomic_store(&state->refcount, 1); /* debug thread owns initial reference */
    state->transport = transport;
    state->threadid = threadid;

    pthread_mutex_lock(&g_debug_mutex);

    for (int i = 0; i < MAX_DEBUG_THREADS; i++) {
        if (g_debug_threads[i] == NULL) {
            g_debug_threads[i] = state;
            pthread_mutex_unlock(&g_debug_mutex);
            return state;
        }
    }

    pthread_mutex_unlock(&g_debug_mutex);
    free(state);
    return NULL; /* no slots available */
}

static void debug_unregister_thread(long threadid) {

    tydebugstate *state = NULL;

    pthread_mutex_lock(&g_debug_mutex);

    for (int i = 0; i < MAX_DEBUG_THREADS; i++) {
        if (g_debug_threads[i] != NULL && g_debug_threads[i]->threadid == threadid) {
            state = g_debug_threads[i];
            g_debug_threads[i] = NULL; /* remove from registry */
            break;
        }
    }

    pthread_mutex_unlock(&g_debug_mutex);

    /* Release the debug thread's reference. If a protocol handler also holds
     * a reference (from debug_get_state_for_thread), the struct stays alive
     * until they call debug_release_state. */
    if (state != NULL)
        debug_release_state(state);
}

void debug_release_state(tydebugstate *state) {

    if (state == NULL)
        return;

    int old = atomic_fetch_sub(&state->refcount, 1);
    assert(old > 0); /* refcount underflow */
    if (old == 1) {
        /* Last reference — safe to free */
        free(state);
    }
}

/* Thread IDs captured during kill, joined during shutdown.
 * Needed because debug_unregister_thread NULLs the g_debug_threads
 * entry before debug_join_all_threads can read it.
 * Synchronization: written under g_debug_mutex in debug_kill_all_threads,
 * read without lock in debug_join_all_threads. Safe because these are
 * called sequentially during shutdown (kill first, then join). */
static pthread_t g_killed_threads[MAX_DEBUG_THREADS];
static int g_killed_thread_count = 0;

boolean debug_is_safe_to_save(void) {
    return !atomic_load(&g_debug_thread_was_killed);
}

boolean debug_has_active_threads(void) {

    boolean active = false;

    pthread_mutex_lock(&g_debug_mutex);

    for (int i = 0; i < MAX_DEBUG_THREADS; i++) {
        if (g_debug_threads[i] != NULL) {
            active = true;
            break;
        }
    }

    pthread_mutex_unlock(&g_debug_mutex);
    return active;
}

void debug_kill_all_threads(void) {

    pthread_mutex_lock(&g_debug_mutex);

    g_killed_thread_count = 0;

    for (int i = 0; i < MAX_DEBUG_THREADS; i++) {
        if (g_debug_threads[i] != NULL) {
            /* Killing a thread mid-execution corrupts hash table state.
             * Mark as unsafe so save-on-exit is skipped. */
            atomic_store(&g_debug_thread_was_killed, true);

            /* Capture pthread_t before the thread can unregister and free state */
            g_killed_threads[g_killed_thread_count++] = g_debug_threads[i]->pthread_id;
            atomic_store_explicit(&g_debug_threads[i]->flkill, true, memory_order_seq_cst);
            atomic_store_explicit(&g_debug_threads[i]->flsuspended, false, memory_order_seq_cst); /* wake suspended threads */
        }
    }

    pthread_mutex_unlock(&g_debug_mutex);
}

void debug_join_all_threads(void) {

    /* Join threads captured by debug_kill_all_threads. Must be called
     * with GIL released so threads can acquire it to finish cleanup. */
    for (int i = 0; i < g_killed_thread_count; i++) {
        pthread_join(g_killed_threads[i], NULL);
    }

    g_killed_thread_count = 0;
}

/* ========================================================================
 * Notifications
 * ======================================================================== */

/* Send a debug/suspended notification with a type-safe reason enum.
 * The enum is converted to a JSON-safe string via debug_reason_string(). */
void debug_send_suspended(transport_t *transport, long threadid, long line, debug_suspend_reason_t reason) {

    char json[512];
    snprintf(json, sizeof(json),
             "{\"id\":null,\"op\":\"debug/suspended\",\"params\":"
             "{\"threadId\":%ld,\"line\":%ld,\"reason\":\"%s\"}}",
             threadid, line, debug_reason_string(reason));

    transport->write_line(transport->ctx, json, strlen(json));
}

static void debug_send_completed(transport_t *transport, long threadid, boolean success) {

    char json[256];
    snprintf(json, sizeof(json),
             "{\"id\":null,\"op\":\"debug/completed\",\"params\":"
             "{\"threadId\":%ld,\"success\":%s}}",
             threadid, success ? "true" : "false");

    transport->write_line(transport->ctx, json, strlen(json));
}

/* ========================================================================
 * Debugger callback — replaces cb_noop_treenode
 * ======================================================================== */

/*
 * Protocol-aware debugger callback. Called at every statement during
 * script execution via langdebuggercall(hnode).
 *
 * For non-debug threads: returns true immediately (no overhead).
 * For debug threads: checks suspension flags, sends notifications,
 * and yields the GIL while suspended.
 *
 * Returns false to kill the script, true to continue.
 */
static boolean protocol_debugger_callback(hdltreenode hnode) {

    /* Get debug state from thread globals. param_reserved[0] is set to a
     * tydebugstate* by debug_thread_entry. For non-debug threads it's NULL
     * (calloc-initialized). The cast is safe as long as only debug_handler.c
     * writes to param_reserved[0]. */
    if (hthreadglobals == nil)
        return true;

    tydebugstate *state = (tydebugstate *)((**hthreadglobals).param_reserved[0]);

    if (state == NULL || !state->fldebugmode)
        return true; /* not debugging this thread */

    /* Check kill flag */
    if (atomic_load(&state->flkill)) {
        log_debug(LOG_COMP_LANG, "debug: thread %ld killed", state->threadid);
        return false;
    }

    /* Get current line number */
    unsigned long lnum = (hnode != nil) ? (**hnode).lnum : 0;

    /* Determine if this is a "steppable" node. Infrastructure nodes (module,
     * noop, bundle, local, assignlocal) should execute normally but not
     * trigger stepping suspensions — they're not meaningful "lines" to
     * stop on. The callback still returns true (continue executing). */
    boolean flsteppable = true;
    if (hnode != nil) {
        short op = (**hnode).nodetype;
        if (op == moduleop || op == noop || op == bundleop || op == localop || op == assignlocalop)
            flsteppable = false;
    }

    /* Check interrupt flag (debug/pause) — only on steppable nodes */
    if (flsteppable && atomic_load(&state->flinterrupt)) {
        atomic_store_explicit(&state->flinterrupt, false, memory_order_seq_cst);
        atomic_store_explicit(&state->flsuspended, true, memory_order_seq_cst);

        atomic_store(&state->lastlnum, lnum);

        debug_send_suspended(state->transport, state->threadid, (long)lnum, DEBUG_REASON_INTERRUPTED);
        log_debug(LOG_COMP_LANG, "debug: thread %ld interrupted at line %ld", state->threadid, (long)lnum);
    }

    /* Stepping logic — check if we should suspend based on step direction.
     * Uses simplified call depth model: calldepth tracks nesting relative
     * to the depth when stepping was initiated (steplevel).
     *
     * Step-into: suspend at the very next statement
     * Step-over: suspend when line changes at same or shallower call depth
     * Step-out:  suspend when call depth decreases below step level */
    if (atomic_load(&state->flstepping) && flsteppable && !atomic_load(&state->flsuspended)) {

        short diff = state->calldepth - atomic_load(&state->steplevel);
        boolean flstop = false;

        switch (atomic_load(&state->stepdir)) {

            case DEBUG_STEP_INTO:
                /* Stop at the very next statement */
                flstop = true;
                break;

            case DEBUG_STEP_OVER:
                if (diff == 0) {
                    /* Same call depth: stop when line changes */
                    flstop = (lnum != atomic_load(&state->lastlnum));
                } else if (diff < 0) {
                    /* Returned to shallower depth: stop */
                    flstop = true;
                }
                /* diff > 0: inside a function call, keep going */
                break;

            case DEBUG_STEP_OUT:
                /* Stop only when we return to a shallower depth */
                flstop = (diff < 0);
                break;

            default:
                break;
        }

        if (flstop) {
            atomic_store(&state->flstepping, false);
            atomic_store(&state->lastlnum, lnum);

            /* Send notification BEFORE suspending so it arrives immediately */
            debug_send_suspended(state->transport, state->threadid, (long)lnum, DEBUG_REASON_STEP);
            log_debug(LOG_COMP_LANG, "debug: thread %ld step completed at line %ld", state->threadid, (long)lnum);

            atomic_store_explicit(&state->flsuspended, true, memory_order_seq_cst);
        }
    }

    /* Suspension loop — yields GIL so protocol handler can process commands */
    while (atomic_load(&state->flsuspended)) {

        if (atomic_load(&state->flkill)) {
            if (hthreadglobals != nil)
                (**hthreadglobals).flthreadkilled = true;
            return false;
        }

        /* Save thread globals, release GIL, sleep, reacquire, restore */
        headless_save_threadglobals(hthreadglobals);
        pthread_mutex_unlock(&frontier_gil);

        /* Sleep 10ms — other threads (including protocol handler) can run */
        struct timespec ts = {0, 10000000}; /* 10ms */
        nanosleep(&ts, NULL);

        pthread_mutex_lock(&frontier_gil);
        headless_restore_threadglobals(hthreadglobals);
    }

    /* Check kill flag after loop exit — handle_debug_kill sets flkill=true
     * and flsuspended=false simultaneously, so we may exit the loop without
     * seeing the kill flag inside it. Set flthreadkilled so the interpreter
     * (evaluatelist) knows this is a kill, not a bug.
     *
     * Notification flow for kill-after-continue: this callback returns false,
     * langruncode returns false, debug_thread_entry sends debug/completed
     * with success=false, then cleans up. The callback does NOT send
     * debug/completed — that's always the thread entry's responsibility. */
    if (atomic_load(&state->flkill)) {
        if (hthreadglobals != nil)
            (**hthreadglobals).flthreadkilled = true;
        return false;
    }

    return true;
}

/* ========================================================================
 * debug_init — Install the protocol debugger callback
 * ======================================================================== */

void debug_init(void) {

    langcallbacks.debuggercallback = &protocol_debugger_callback;
    log_info(LOG_COMP_GENERAL, "Protocol debugger initialized");
}

/* ========================================================================
 * Debug thread entry point
 * ======================================================================== */

typedef struct {
    hdltreenode hcode;
    hdlthreadglobals hglobals;
    frontier_pthread_record *rec;
    tydebugstate *debugstate;
} debug_thread_params;

static void *debug_thread_entry(void *arg) {

    debug_thread_params *params = (debug_thread_params *)arg;
    tyvaluerecord result;

    if (params == NULL)
        return NULL;

    /* Acquire GIL */
    pthread_mutex_lock(&frontier_gil);

    /* Restore this thread's globals */
    headless_restore_threadglobals(params->hglobals);

    /* Register in system.compiler.threads */
    {
        bigstring bsname;
        copyctopstring("debug", bsname);
        headless_register_thread(bsname, params->debugstate->threadid);
    }

    /* Store debug state in thread globals for the callback to find */
    (**params->hglobals).param_reserved[0] = (void *)params->debugstate;

    boolean fl_ran = false;

    /* Initial suspension — pause before first statement so client can set breakpoints */
    atomic_store(&params->debugstate->flsuspended, true);
    debug_send_suspended(params->debugstate->transport, params->debugstate->threadid, 0, DEBUG_REASON_ENTRY);

    /* Suspension loop (same pattern as in the callback) */
    while (atomic_load(&params->debugstate->flsuspended)) {

        if (atomic_load(&params->debugstate->flkill)) {
            debug_send_completed(params->debugstate->transport, params->debugstate->threadid, false);
            goto cleanup;
        }

        headless_save_threadglobals(params->hglobals);
        pthread_mutex_unlock(&frontier_gil);

        struct timespec ts = {0, 10000000};
        nanosleep(&ts, NULL);

        pthread_mutex_lock(&frontier_gil);
        headless_restore_threadglobals(params->hglobals);
    }

    /* Execute the script */
    fl_ran = true;
    initvalue(&result, novaluetype);

    boolean fl = langruncode(params->hcode, nil, &result);

    /* Send completion notification */
    debug_send_completed(params->debugstate->transport, params->debugstate->threadid, fl);

cleanup:
    if (fl_ran)
        disposevaluerecord(result, false);

    /* Save globals while we still hold GIL */
    headless_save_threadglobals(params->hglobals);

    /* Clear error state */
    headless_clear_last_lang_error();

    /* Unregister from system.compiler.threads and debug registry */
    headless_unregister_thread(params->rec->user_thread_id);
    debug_unregister_thread(params->debugstate->threadid);

    /* Cleanup */
    langdisposetree(params->hcode);
    headless_dispose_threadglobals(params->hglobals);
    free_thread_record(params->rec);
    free(params);

    /* Release GIL */
    pthread_mutex_unlock(&frontier_gil);
    pthread_cond_broadcast(&gil_available);

    return NULL;
}

/* ========================================================================
 * Protocol operation handlers
 * ======================================================================== */

void handle_debug_run(int id, const char *json_line, transport_t *transport) {

    /* Parse the expression from params */
    cJSON *root = cJSON_Parse(json_line);
    if (root == NULL) {
        char err[512];
        snprintf(err, sizeof(err), "{\"id\":%d,\"error\":{\"message\":\"Invalid JSON\"},\"success\":false}", id);
        transport->write_line(transport->ctx, err, strlen(err));
        return;
    }

    cJSON *params = cJSON_GetObjectItemCaseSensitive(root, "params");
    cJSON *expr_json = params ? cJSON_GetObjectItemCaseSensitive(params, "expression") : NULL;

    if (!cJSON_IsString(expr_json) || expr_json->valuestring == NULL) {
        char err[512];
        snprintf(err, sizeof(err), "{\"id\":%d,\"error\":{\"message\":\"Missing 'expression' in params\"},\"success\":false}", id);
        transport->write_line(transport->ctx, err, strlen(err));
        cJSON_Delete(root);
        return;
    }

    const char *expression = expr_json->valuestring;

    /* Compile the expression into a code tree */
    Handle htext;
    hdltreenode hcode;

    if (!newfilledhandle((void *)expression, strlen(expression), &htext)) {
        char err[512];
        snprintf(err, sizeof(err), "{\"id\":%d,\"error\":{\"message\":\"Memory allocation failed\"},\"success\":false}", id);
        transport->write_line(transport->ctx, err, strlen(err));
        cJSON_Delete(root);
        return;
    }

    /* langcompiletext always disposes htext (both success and failure) */
    if (!langcompiletext(htext, false, &hcode)) {
        extern const unsigned char *headless_get_last_lang_error(void);
        const unsigned char *errmsg = headless_get_last_lang_error();

        cJSON *resp = cJSON_CreateObject();
        cJSON_AddNumberToObject(resp, "id", id);
        cJSON *errobj = cJSON_CreateObject();
        if (errmsg != NULL && errmsg[0] > 0) {
            int msglen = (int)errmsg[0];
            char msgbuf[256];
            if (msglen > 255) msglen = 255;
            memcpy(msgbuf, errmsg + 1, (size_t)msglen);
            msgbuf[msglen] = '\0';
            char full_msg[512];
            snprintf(full_msg, sizeof(full_msg), "Compilation failed: %s", msgbuf);
            cJSON_AddStringToObject(errobj, "message", full_msg);
        } else {
            cJSON_AddStringToObject(errobj, "message", "Compilation failed");
        }
        cJSON_AddItemToObject(resp, "error", errobj);
        cJSON_AddBoolToObject(resp, "success", 0);
        char *json_str = cJSON_PrintUnformatted(resp);
        if (json_str) {
            transport->write_line(transport->ctx, json_str, strlen(json_str));
            free(json_str);
        }
        cJSON_Delete(resp);
        cJSON_Delete(root);
        return;
    }

    /* Allocate thread record */
    frontier_pthread_record *rec = allocate_thread_record();
    if (rec == NULL) {
        langdisposetree(hcode);
        char err[512];
        snprintf(err, sizeof(err), "{\"id\":%d,\"error\":{\"message\":\"Failed to allocate thread\"},\"success\":false}", id);
        transport->write_line(transport->ctx, err, strlen(err));
        cJSON_Delete(root);
        return;
    }

    /* Allocate thread globals */
    hdlthreadglobals new_hglobals = headless_new_threadglobals();
    if (new_hglobals == nil) {
        langdisposetree(hcode);
        free_thread_record(rec);
        char err[512];
        snprintf(err, sizeof(err), "{\"id\":%d,\"error\":{\"message\":\"Failed to allocate thread globals\"},\"success\":false}", id);
        transport->write_line(transport->ctx, err, strlen(err));
        cJSON_Delete(root);
        return;
    }

    long threadid = (long)rec->user_thread_id;
    (**new_hglobals).idthread = (hdlthread)threadid;
    rec->hglobals = new_hglobals;

    /* Copy hashtable stack from current thread */
    {
        Handle hcopy;
        if (!newfilledhandle((char *)(*hashtablestack), sizeof(tytablestack), &hcopy)) {
            langdisposetree(hcode);
            headless_dispose_threadglobals(new_hglobals);
            free_thread_record(rec);
            char err[512];
            snprintf(err, sizeof(err), "{\"id\":%d,\"error\":{\"message\":\"Failed to copy table stack\"},\"success\":false}", id);
            transport->write_line(transport->ctx, err, strlen(err));
            cJSON_Delete(root);
            return;
        }
        (**new_hglobals).htablestack = (hdltablestack)hcopy;
    }
    (**new_hglobals).hcurrenthashtable = currenthashtable;

    /* Register debug state */
    tydebugstate *debugstate = debug_register_thread(threadid, transport);
    if (debugstate == NULL) {
        langdisposetree(hcode);
        headless_dispose_threadglobals(new_hglobals);
        free_thread_record(rec);
        char err[512];
        snprintf(err, sizeof(err), "{\"id\":%d,\"error\":{\"message\":\"Too many debug threads\"},\"success\":false}", id);
        transport->write_line(transport->ctx, err, strlen(err));
        cJSON_Delete(root);
        return;
    }

    /* Package launch parameters */
    debug_thread_params *dparams = (debug_thread_params *)malloc(sizeof(debug_thread_params));
    if (dparams == NULL) {
        langdisposetree(hcode);
        headless_dispose_threadglobals(new_hglobals);
        free_thread_record(rec);
        debug_unregister_thread(threadid);
        char err[512];
        snprintf(err, sizeof(err), "{\"id\":%d,\"error\":{\"message\":\"Memory allocation failed\"},\"success\":false}", id);
        transport->write_line(transport->ctx, err, strlen(err));
        cJSON_Delete(root);
        return;
    }

    dparams->hcode = hcode;
    dparams->hglobals = new_hglobals;
    dparams->rec = rec;
    dparams->debugstate = debugstate;

    /* Spawn debug thread */
    pthread_t tid;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    if (pthread_create(&tid, &attr, debug_thread_entry, dparams) != 0) {
        pthread_attr_destroy(&attr);
        langdisposetree(hcode);
        headless_dispose_threadglobals(new_hglobals);
        free_thread_record(rec);
        debug_unregister_thread(threadid);
        free(dparams);
        char err[512];
        snprintf(err, sizeof(err), "{\"id\":%d,\"error\":{\"message\":\"Failed to spawn debug thread\"},\"success\":false}", id);
        transport->write_line(transport->ctx, err, strlen(err));
        cJSON_Delete(root);
        return;
    }

    pthread_attr_destroy(&attr);
    rec->pthread_id = tid;

    /* Set pthread_id immediately after create — debug_kill_all_threads reads
     * this field under g_debug_mutex, so set it before any code that could
     * trigger shutdown (the response write below). */
    pthread_mutex_lock(&g_debug_mutex);
    debugstate->pthread_id = tid;
    pthread_mutex_unlock(&g_debug_mutex);

    /* Return immediately with thread ID */
    char resp[128];
    snprintf(resp, sizeof(resp),
             "{\"id\":%d,\"result\":{\"threadId\":%ld,\"status\":\"started\"},\"success\":true}",
             id, threadid);
    transport->write_line(transport->ctx, resp, strlen(resp));

    cJSON_Delete(root);
}

void handle_debug_continue(int id, const char *json_line, transport_t *transport) {

    cJSON *root = cJSON_Parse(json_line);
    cJSON *params = root ? cJSON_GetObjectItemCaseSensitive(root, "params") : NULL;
    cJSON *tid_json = params ? cJSON_GetObjectItemCaseSensitive(params, "threadId") : NULL;

    if (!cJSON_IsNumber(tid_json)) {
        char err[512];
        snprintf(err, sizeof(err), "{\"id\":%d,\"error\":{\"message\":\"Missing 'threadId' in params\"},\"success\":false}", id);
        transport->write_line(transport->ctx, err, strlen(err));
        cJSON_Delete(root);
        return;
    }

    long threadid = (long)tid_json->valuedouble;
    tydebugstate *state = debug_get_state_for_thread(threadid);

    if (state == NULL) {
        char err[512];
        snprintf(err, sizeof(err), "{\"id\":%d,\"error\":{\"message\":\"No debug thread with that ID\"},\"success\":false}", id);
        transport->write_line(transport->ctx, err, strlen(err));
        cJSON_Delete(root);
        return;
    }

    /* Clear any stepping state — continue means run freely */
    atomic_store(&state->flstepping, false);
    atomic_store(&state->stepdir, DEBUG_STEP_NONE);

    atomic_store(&state->flsuspended, false);
    debug_release_state(state);

    char resp[128];
    snprintf(resp, sizeof(resp), "{\"id\":%d,\"result\":{\"threadId\":%ld,\"status\":\"running\"},\"success\":true}", id, threadid);
    transport->write_line(transport->ctx, resp, strlen(resp));

    cJSON_Delete(root);
}

void handle_debug_step(int id, const char *json_line, transport_t *transport) {

    cJSON *root = cJSON_Parse(json_line);
    cJSON *params = root ? cJSON_GetObjectItemCaseSensitive(root, "params") : NULL;
    cJSON *tid_json = params ? cJSON_GetObjectItemCaseSensitive(params, "threadId") : NULL;
    cJSON *dir_json = params ? cJSON_GetObjectItemCaseSensitive(params, "direction") : NULL;

    if (!cJSON_IsNumber(tid_json)) {
        char err[512];
        snprintf(err, sizeof(err), "{\"id\":%d,\"error\":{\"message\":\"Missing 'threadId' in params\"},\"success\":false}", id);
        transport->write_line(transport->ctx, err, strlen(err));
        cJSON_Delete(root);
        return;
    }

    long threadid = (long)tid_json->valuedouble;
    tydebugstate *state = debug_get_state_for_thread(threadid);

    if (state == NULL) {
        char err[512];
        snprintf(err, sizeof(err), "{\"id\":%d,\"error\":{\"message\":\"No debug thread with that ID\"},\"success\":false}", id);
        transport->write_line(transport->ctx, err, strlen(err));
        cJSON_Delete(root);
        return;
    }

    if (!atomic_load(&state->flsuspended)) {
        debug_release_state(state);
        char err[512];
        snprintf(err, sizeof(err), "{\"id\":%d,\"error\":{\"message\":\"Thread %ld is not suspended\"},\"success\":false}", id, threadid);
        transport->write_line(transport->ctx, err, strlen(err));
        cJSON_Delete(root);
        return;
    }

    /* Parse direction: "over", "into", "out" */
    debug_step_direction_t dir = DEBUG_STEP_OVER; /* default */
    if (cJSON_IsString(dir_json)) {
        const char *d = dir_json->valuestring;
        if (strcmp(d, "into") == 0)
            dir = DEBUG_STEP_INTO;
        else if (strcmp(d, "out") == 0)
            dir = DEBUG_STEP_OUT;
        else if (strcmp(d, "over") == 0)
            dir = DEBUG_STEP_OVER;
        else {
            debug_release_state(state);
            char err[512];
            snprintf(err, sizeof(err), "{\"id\":%d,\"error\":{\"message\":\"Unknown step direction (use 'over', 'into', or 'out')\"},\"success\":false}", id);
            transport->write_line(transport->ctx, err, strlen(err));
            cJSON_Delete(root);
            return;
        }
    }

    /* Set stepping state. These writes are safe because the debug thread is
     * suspended (flsuspended=true) and won't read stepping fields until we
     * clear flsuspended below. GIL ordering guarantees the writes are visible. */
    atomic_store(&state->flstepping, true);
    atomic_store(&state->stepdir, dir);
    atomic_store(&state->steplevel, state->calldepth);
    /* lastlnum already set from the last suspension point */

    /* Resume the thread — it will execute until the stepping condition is met */
    atomic_store_explicit(&state->flsuspended, false, memory_order_seq_cst);
    debug_release_state(state);

    char resp[128];
    snprintf(resp, sizeof(resp), "{\"id\":%d,\"result\":{\"threadId\":%ld,\"status\":\"stepping\"},\"success\":true}", id, threadid);
    transport->write_line(transport->ctx, resp, strlen(resp));

    cJSON_Delete(root);
}

void handle_debug_kill(int id, const char *json_line, transport_t *transport) {

    cJSON *root = cJSON_Parse(json_line);
    cJSON *params = root ? cJSON_GetObjectItemCaseSensitive(root, "params") : NULL;
    cJSON *tid_json = params ? cJSON_GetObjectItemCaseSensitive(params, "threadId") : NULL;

    if (!cJSON_IsNumber(tid_json)) {
        char err[512];
        snprintf(err, sizeof(err), "{\"id\":%d,\"error\":{\"message\":\"Missing 'threadId' in params\"},\"success\":false}", id);
        transport->write_line(transport->ctx, err, strlen(err));
        cJSON_Delete(root);
        return;
    }

    long threadid = (long)tid_json->valuedouble;
    tydebugstate *state = debug_get_state_for_thread(threadid);

    if (state == NULL) {
        char err[512];
        snprintf(err, sizeof(err), "{\"id\":%d,\"error\":{\"message\":\"No debug thread with that ID\"},\"success\":false}", id);
        transport->write_line(transport->ctx, err, strlen(err));
        cJSON_Delete(root);
        return;
    }

    /* Mark as killed — hash tables will be inconsistent after this */
    atomic_store(&g_debug_thread_was_killed, true);

    atomic_store_explicit(&state->flkill, true, memory_order_seq_cst);
    atomic_store_explicit(&state->flsuspended, false, memory_order_seq_cst); /* wake it up so it can die */
    debug_release_state(state);

    char resp[128];
    snprintf(resp, sizeof(resp), "{\"id\":%d,\"result\":{\"threadId\":%ld,\"status\":\"killed\"},\"success\":true}", id, threadid);
    transport->write_line(transport->ctx, resp, strlen(resp));

    cJSON_Delete(root);
}

void handle_debug_pause(int id, const char *json_line, transport_t *transport) {

    cJSON *root = cJSON_Parse(json_line);
    cJSON *params = root ? cJSON_GetObjectItemCaseSensitive(root, "params") : NULL;
    cJSON *tid_json = params ? cJSON_GetObjectItemCaseSensitive(params, "threadId") : NULL;

    if (!cJSON_IsNumber(tid_json)) {
        char err[512];
        snprintf(err, sizeof(err), "{\"id\":%d,\"error\":{\"message\":\"Missing 'threadId' in params\"},\"success\":false}", id);
        transport->write_line(transport->ctx, err, strlen(err));
        cJSON_Delete(root);
        return;
    }

    long threadid = (long)tid_json->valuedouble;
    tydebugstate *state = debug_get_state_for_thread(threadid);

    if (state == NULL) {
        char err[512];
        snprintf(err, sizeof(err), "{\"id\":%d,\"error\":{\"message\":\"No debug thread with that ID\"},\"success\":false}", id);
        transport->write_line(transport->ctx, err, strlen(err));
        cJSON_Delete(root);
        return;
    }

    /* Already suspended — return error instead of setting interrupt flag */
    if (atomic_load(&state->flsuspended)) {
        debug_release_state(state);
        char err[256];
        snprintf(err, sizeof(err), "{\"id\":%d,\"error\":{\"message\":\"Thread %ld is already suspended\"},\"success\":false}", id, threadid);
        transport->write_line(transport->ctx, err, strlen(err));
        cJSON_Delete(root);
        return;
    }

    /* Set interrupt flag — callback will suspend at next statement */
    atomic_store(&state->flinterrupt, true);
    debug_release_state(state);

    char resp[128];
    snprintf(resp, sizeof(resp), "{\"id\":%d,\"result\":{\"threadId\":%ld,\"status\":\"interrupting\"},\"success\":true}", id, threadid);
    transport->write_line(transport->ctx, resp, strlen(resp));

    cJSON_Delete(root);
}
