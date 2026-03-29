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
#include "../third_party/cJSON/cJSON.h"

/* External thread infrastructure from headless_thread_verbs.c */
extern hdlthreadglobals headless_new_threadglobals(void);
extern void headless_dispose_threadglobals(hdlthreadglobals hg);
extern void headless_save_threadglobals(hdlthreadglobals hg);
extern void headless_restore_threadglobals(hdlthreadglobals hg);
extern frontier_pthread_record *allocate_thread_record(void);
extern void free_thread_record(frontier_pthread_record *rec);
extern boolean headless_register_thread(bigstring bsname, long idthread);
extern boolean headless_unregister_thread(long idthread);
extern void headless_clear_last_lang_error(void);

/* GIL from headless_thread_verbs.c */
extern pthread_mutex_t frontier_gil;
extern pthread_cond_t gil_available;

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

/* ========================================================================
 * Debug state management
 * ======================================================================== */

static tydebugstate *debug_get_state_for_thread(long threadid) {

    pthread_mutex_lock(&g_debug_mutex);

    for (int i = 0; i < MAX_DEBUG_THREADS; i++) {
        if (g_debug_threads[i] != NULL && g_debug_threads[i]->threadid == threadid) {
            pthread_mutex_unlock(&g_debug_mutex);
            return g_debug_threads[i];
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
    state->flsuspended = false;
    state->flinterrupt = false;
    state->flkill = false;
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

    pthread_mutex_lock(&g_debug_mutex);

    for (int i = 0; i < MAX_DEBUG_THREADS; i++) {
        if (g_debug_threads[i] != NULL && g_debug_threads[i]->threadid == threadid) {
            free(g_debug_threads[i]);
            g_debug_threads[i] = NULL;
            break;
        }
    }

    pthread_mutex_unlock(&g_debug_mutex);
}

/* ========================================================================
 * Notifications
 * ======================================================================== */

void debug_send_suspended(transport_t *transport, long threadid, long line, const char *reason) {

    char json[512];
    snprintf(json, sizeof(json),
             "{\"id\":null,\"op\":\"debug/suspended\",\"params\":"
             "{\"threadId\":%ld,\"line\":%ld,\"reason\":\"%s\"}}",
             threadid, line, reason);

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

    (void)hnode;

    /* Get debug state from thread globals */
    if (hthreadglobals == nil)
        return true;

    tydebugstate *state = (tydebugstate *)((**hthreadglobals).param_reserved[0]);

    if (state == NULL || !state->fldebugmode)
        return true; /* not debugging this thread */

    /* Check kill flag */
    if (state->flkill) {
        log_debug(LOG_COMP_LANG, "debug: thread %ld killed", state->threadid);
        return false; /* signal interpreter to stop */
    }

    /* Check interrupt flag (debug/pause) */
    if (state->flinterrupt) {
        state->flinterrupt = false;
        state->flsuspended = true;

        /* Get line number from the current node */
        long line = 0;
        if (hnode != nil)
            line = (long)(**hnode).lnum;

        debug_send_suspended(state->transport, state->threadid, line, "interrupted");
        log_debug(LOG_COMP_LANG, "debug: thread %ld interrupted at line %ld", state->threadid, line);
    }

    /* Suspension loop — yields GIL so protocol handler can process commands */
    while (state->flsuspended) {

        if (state->flkill)
            return false;

        /* Save thread globals, release GIL, sleep, reacquire, restore */
        headless_save_threadglobals(hthreadglobals);
        pthread_mutex_unlock(&frontier_gil);

        /* Sleep 10ms — other threads (including protocol handler) can run */
        struct timespec ts = {0, 10000000}; /* 10ms */
        nanosleep(&ts, NULL);

        pthread_mutex_lock(&frontier_gil);
        headless_restore_threadglobals(hthreadglobals);
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

    /* Initial suspension — pause before first statement so client can set breakpoints */
    params->debugstate->flsuspended = true;
    debug_send_suspended(params->debugstate->transport, params->debugstate->threadid, 0, "entry");

    /* Suspension loop (same pattern as in the callback) */
    while (params->debugstate->flsuspended) {

        if (params->debugstate->flkill) {
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
    initvalue(&result, novaluetype);

    boolean fl = langruncode(params->hcode, nil, &result);

    /* Send completion notification */
    debug_send_completed(params->debugstate->transport, params->debugstate->threadid, fl);

    disposevaluerecord(result, false);

cleanup:
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
        char err[128];
        snprintf(err, sizeof(err), "{\"id\":%d,\"error\":{\"message\":\"Invalid JSON\"},\"success\":false}", id);
        transport->write_line(transport->ctx, err, strlen(err));
        return;
    }

    cJSON *params = cJSON_GetObjectItemCaseSensitive(root, "params");
    cJSON *expr_json = params ? cJSON_GetObjectItemCaseSensitive(params, "expression") : NULL;

    if (!cJSON_IsString(expr_json) || expr_json->valuestring == NULL) {
        char err[128];
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
        char err[128];
        snprintf(err, sizeof(err), "{\"id\":%d,\"error\":{\"message\":\"Memory allocation failed\"},\"success\":false}", id);
        transport->write_line(transport->ctx, err, strlen(err));
        cJSON_Delete(root);
        return;
    }

    /* langcompiletext always disposes htext (both success and failure) */
    if (!langcompiletext(htext, false, &hcode)) {
        char err[128];
        snprintf(err, sizeof(err), "{\"id\":%d,\"error\":{\"message\":\"Compilation failed\"},\"success\":false}", id);
        transport->write_line(transport->ctx, err, strlen(err));
        cJSON_Delete(root);
        return;
    }

    /* Allocate thread record */
    frontier_pthread_record *rec = allocate_thread_record();
    if (rec == NULL) {
        langdisposetree(hcode);
        char err[128];
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
        char err[128];
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
            char err[128];
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
        char err[128];
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
        char err[128];
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
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    if (pthread_create(&tid, &attr, debug_thread_entry, dparams) != 0) {
        pthread_attr_destroy(&attr);
        langdisposetree(hcode);
        headless_dispose_threadglobals(new_hglobals);
        free_thread_record(rec);
        debug_unregister_thread(threadid);
        free(dparams);
        char err[128];
        snprintf(err, sizeof(err), "{\"id\":%d,\"error\":{\"message\":\"Failed to spawn debug thread\"},\"success\":false}", id);
        transport->write_line(transport->ctx, err, strlen(err));
        cJSON_Delete(root);
        return;
    }

    pthread_attr_destroy(&attr);
    rec->pthread_id = tid;

    /* Return immediately with thread ID */
    char resp[128];
    snprintf(resp, sizeof(resp),
             "{\"id\":%d,\"result\":{\"threadId\":%ld,\"status\":\"running\"},\"success\":true}",
             id, threadid);
    transport->write_line(transport->ctx, resp, strlen(resp));

    cJSON_Delete(root);
}

void handle_debug_continue(int id, const char *json_line, transport_t *transport) {

    cJSON *root = cJSON_Parse(json_line);
    cJSON *params = root ? cJSON_GetObjectItemCaseSensitive(root, "params") : NULL;
    cJSON *tid_json = params ? cJSON_GetObjectItemCaseSensitive(params, "threadId") : NULL;

    if (!cJSON_IsNumber(tid_json)) {
        char err[128];
        snprintf(err, sizeof(err), "{\"id\":%d,\"error\":{\"message\":\"Missing 'threadId' in params\"},\"success\":false}", id);
        transport->write_line(transport->ctx, err, strlen(err));
        cJSON_Delete(root);
        return;
    }

    long threadid = (long)tid_json->valuedouble;
    tydebugstate *state = debug_get_state_for_thread(threadid);

    if (state == NULL) {
        char err[128];
        snprintf(err, sizeof(err), "{\"id\":%d,\"error\":{\"message\":\"No debug thread with that ID\"},\"success\":false}", id);
        transport->write_line(transport->ctx, err, strlen(err));
        cJSON_Delete(root);
        return;
    }

    state->flsuspended = false;

    char resp[128];
    snprintf(resp, sizeof(resp), "{\"id\":%d,\"result\":{\"threadId\":%ld,\"status\":\"running\"},\"success\":true}", id, threadid);
    transport->write_line(transport->ctx, resp, strlen(resp));

    cJSON_Delete(root);
}

void handle_debug_kill(int id, const char *json_line, transport_t *transport) {

    cJSON *root = cJSON_Parse(json_line);
    cJSON *params = root ? cJSON_GetObjectItemCaseSensitive(root, "params") : NULL;
    cJSON *tid_json = params ? cJSON_GetObjectItemCaseSensitive(params, "threadId") : NULL;

    if (!cJSON_IsNumber(tid_json)) {
        char err[128];
        snprintf(err, sizeof(err), "{\"id\":%d,\"error\":{\"message\":\"Missing 'threadId' in params\"},\"success\":false}", id);
        transport->write_line(transport->ctx, err, strlen(err));
        cJSON_Delete(root);
        return;
    }

    long threadid = (long)tid_json->valuedouble;
    tydebugstate *state = debug_get_state_for_thread(threadid);

    if (state == NULL) {
        char err[128];
        snprintf(err, sizeof(err), "{\"id\":%d,\"error\":{\"message\":\"No debug thread with that ID\"},\"success\":false}", id);
        transport->write_line(transport->ctx, err, strlen(err));
        cJSON_Delete(root);
        return;
    }

    state->flkill = true;
    state->flsuspended = false; /* wake it up so it can die */

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
        char err[128];
        snprintf(err, sizeof(err), "{\"id\":%d,\"error\":{\"message\":\"Missing 'threadId' in params\"},\"success\":false}", id);
        transport->write_line(transport->ctx, err, strlen(err));
        cJSON_Delete(root);
        return;
    }

    long threadid = (long)tid_json->valuedouble;
    tydebugstate *state = debug_get_state_for_thread(threadid);

    if (state == NULL) {
        char err[128];
        snprintf(err, sizeof(err), "{\"id\":%d,\"error\":{\"message\":\"No debug thread with that ID\"},\"success\":false}", id);
        transport->write_line(transport->ctx, err, strlen(err));
        cJSON_Delete(root);
        return;
    }

    /* Set interrupt flag — callback will suspend at next statement */
    state->flinterrupt = true;

    char resp[128];
    snprintf(resp, sizeof(resp), "{\"id\":%d,\"result\":{\"threadId\":%ld,\"status\":\"interrupting\"},\"success\":true}", id, threadid);
    transport->write_line(transport->ctx, resp, strlen(resp));

    cJSON_Delete(root);
}
