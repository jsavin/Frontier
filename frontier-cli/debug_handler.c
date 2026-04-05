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
#include <strings.h>  /* strcasecmp */
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
#include "langexternal.h"
#include "op.h"
#include "../third_party/cJSON/cJSON.h"

/* Global: current hashtable and table stack (thread globals) */
extern hdlhashtable currenthashtable;
extern hdltablestack hashtablestack;
extern hdlhashtable roottable;

/* Forward declarations — these functions exist in Common/source but have no
 * header declaration. Used by debug/getSource for script path resolution
 * and outline-to-text conversion. */
extern boolean opgetlangtext(hdloutlinerecord, boolean, Handle *);  /* oplangtext.c */
extern boolean langfastaddresstotable(hdlhashtable, bigstring, hdlhashtable *);  /* langops.c */

/*
 * Maximum number of concurrent debug threads.
 * Each slot holds a pointer to a debug state; NULL = unused.
 *
 * Lock ordering invariant: GIL → g_debug_mutex. All protocol handlers
 * hold the GIL (acquired by protocol_handler.c before dispatch) and may
 * then acquire g_debug_mutex. Never acquire the GIL while holding
 * g_debug_mutex — this would deadlock. The debugger callback runs with
 * the GIL held and acquires g_debug_mutex for breakpoint checks.
 */
#define MAX_DEBUG_THREADS 16
static tydebugstate *g_debug_threads[MAX_DEBUG_THREADS] = {0};
static pthread_mutex_t g_debug_mutex = PTHREAD_MUTEX_INITIALIZER;
static atomic_bool g_debug_thread_was_killed = false; /* set when a debug thread is killed mid-execution */

/* ========================================================================
 * Breakpoints (Phase 3)
 *
 * Stored as (script_path, line) pairs in a global array. Protected by
 * g_debug_mutex. The debugger callback checks this list at each statement
 * to determine if execution should suspend.
 *
 * Breakpoints persist for the lifetime of the process. They survive across
 * multiple debug/run invocations and are only cleared by toggling them off
 * via debug/setBreakpoint or when the process exits.
 *
 * Script paths use dotted notation without leading "@", e.g.
 * "mainResponder.respond". The path is matched against the current
 * script tracked via the push/pop sourcecode callbacks.
 * ======================================================================== */

#define MAX_BREAKPOINTS 256

typedef struct {
    char script[DEBUG_SCRIPT_PATH_MAX]; /* dotted script path, e.g. "mainResponder.respond" */
    unsigned long line;                 /* 1-based line number */
    boolean active;                     /* is this slot in use? */
} debug_breakpoint_t;

static debug_breakpoint_t g_breakpoints[MAX_BREAKPOINTS] = {0};
static atomic_bool g_has_breakpoints = false; /* fast-path: skip mutex when no breakpoints set */

/* ========================================================================
 * Watchpoints (Phase 6)
 *
 * Watchpoints monitor a named variable and suspend when its value changes.
 * The callback snapshots the variable's string representation on first
 * encounter and compares on each subsequent callback. If the value differs,
 * the thread suspends with reason "watchpoint" and reports old/new values.
 *
 * Like breakpoints, watchpoints persist for the lifetime of the process
 * and are protected by g_debug_mutex.
 * ======================================================================== */

#define MAX_WATCHPOINTS 64
#define DEBUG_VARNAME_MAX 64
#define DEBUG_VALUE_MAX 256

typedef struct {
    char varname[DEBUG_VARNAME_MAX];    /* variable name to watch */
    char last_value[DEBUG_VALUE_MAX];   /* last known value (string repr) */
    boolean has_snapshot;               /* have we taken an initial snapshot? */
    boolean active;                     /* is this slot in use? */
} debug_watchpoint_t;

static debug_watchpoint_t g_watchpoints[MAX_WATCHPOINTS] = {0};
static atomic_bool g_has_watchpoints = false; /* fast-path */

/* ========================================================================
 * Reason string conversion
 * ======================================================================== */

const char *debug_reason_string(debug_suspend_reason_t reason) {
    switch (reason) {
        case DEBUG_REASON_ENTRY:       return "entry";
        case DEBUG_REASON_INTERRUPTED: return "interrupted";
        case DEBUG_REASON_BREAKPOINT:  return "breakpoint";
        case DEBUG_REASON_STEP:        return "step";
        case DEBUG_REASON_WATCHPOINT:  return "watchpoint";
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
 * Source tracking callbacks (Phase 3)
 *
 * Installed by debug_init() to replace the no-op sourcecode callbacks.
 * These track the current script path in each debug thread's state,
 * enabling breakpoint matching in the debugger callback.
 * ======================================================================== */

static boolean debug_push_sourcecode(hdlhashtable htable, hdlhashnode hnode, bigstring bsname) {

    (void)hnode;

    if (hthreadglobals == nil)
        return true;

    tydebugstate *state = (tydebugstate *)((**hthreadglobals).param_reserved[0]);

    if (state == NULL || !state->fldebugmode)
        return true;

    /* Save current script path on the stack before overwriting */
    if (state->script_stack_depth < DEBUG_SCRIPT_STACK_MAX) {
        memcpy(state->script_stack[state->script_stack_depth],
               state->current_script, DEBUG_SCRIPT_PATH_MAX);
        state->script_stack_depth++;
    } else {
        /* Stack overflow — track the imbalance so pop skips the corresponding restore.
         * Clear current_script to avoid false breakpoint matches: we can't save the
         * caller's path, so it's safer to match nothing than to leave a stale path
         * that persists into the caller after this frame returns. */
        state->script_stack_overflow++;
        state->current_script[0] = '\0';
    }

    /* Build full dotted path from table + name */
    bigstring bspath;
    hdlwindowinfo hroot = NULL;

    if (langexternalgetfullpath(htable, bsname, bspath, &hroot)) {
        (void)hroot; /* used only by langexternalgetfullpath, not needed here */
        /* Convert Pascal string to C string, store in debug state.
         * Path is like "mainResponder.respond" (no leading @). */
        int len = bspath[0];
        if (len >= DEBUG_SCRIPT_PATH_MAX)
            len = DEBUG_SCRIPT_PATH_MAX - 1;
        memcpy(state->current_script, bspath + 1, (size_t)len);
        state->current_script[len] = '\0';

        log_debug(LOG_COMP_LANG, "debug: push source '%s' for thread %ld", state->current_script, state->threadid);
    } else {
        /* Path resolution failed — clear to avoid false breakpoint matches */
        state->current_script[0] = '\0';
    }

    /* Track call depth for step-over/step-out.
     * Incremented on every function call entry, decremented on return.
     * Used by the stepping logic: step-over stops when depth returns to
     * the same level, step-out stops when depth decreases. */
    atomic_fetch_add(&state->calldepth, 1);

    return true;
}

static boolean debug_pop_sourcecode(void) {

    if (hthreadglobals == nil)
        return true;

    tydebugstate *state = (tydebugstate *)((**hthreadglobals).param_reserved[0]);

    if (state == NULL || !state->fldebugmode)
        return true;

    /* Restore caller's script path from the stack.
     * If we overflowed on push, consume the overflow counter instead
     * of restoring — the saved path was never recorded. */
    if (state->script_stack_overflow > 0) {
        state->script_stack_overflow--;
    } else if (state->script_stack_depth > 0) {
        state->script_stack_depth--;
        memcpy(state->current_script,
               state->script_stack[state->script_stack_depth], DEBUG_SCRIPT_PATH_MAX);
    } else {
        state->current_script[0] = '\0';
    }

    /* Decrement call depth (balanced with increment in push).
     * Guard against underflow from unbalanced interpreter error paths. */
    if (atomic_load(&state->calldepth) > 0)
        atomic_fetch_sub(&state->calldepth, 1);

    return true;
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

    /* Breakpoint check (Phase 3) — if not already suspended, check if there's
     * a breakpoint matching the current script and line number. Unlike stepping
     * (which skips infrastructure nodes), breakpoints fire on any line including
     * local declarations.
     *
     * current_script is thread-local to the debug thread (written only by push/pop
     * callbacks on this same thread) — no lock needed. g_debug_mutex protects only
     * the shared g_breakpoints array.
     *
     * Fast-path: g_has_breakpoints is checked with relaxed ordering to skip
     * the mutex entirely when no breakpoints are set (common case). */
    /* Skip breakpoint check when stepping from the same line at the same depth —
     * the step should advance past the current breakpoint, not re-trigger it.
     * A recursive call at the same lnum but greater calldepth is NOT skipped,
     * since the breakpoint should fire on re-entry at a different call level.
     *
     * The multiple atomic_load calls form a consistent snapshot because the
     * callback runs with the GIL held — no other thread can modify these fields. */
    /* Skip breakpoint re-trigger on the same line we just suspended at.
     * After any suspension (breakpoint, step, watchpoint), lastlnum records
     * the suspension line. The callback may fire again for the same lnum
     * (multiple AST nodes per source line) before advancing. Without this
     * guard, the thread would immediately re-hit the same breakpoint.
     * Once lnum changes (next source line), breakpoints fire normally again. */
    boolean flskipbreakpoint = (lnum > 0 && lnum == atomic_load(&state->lastlnum));

    if (!flskipbreakpoint && atomic_load_explicit(&g_has_breakpoints, memory_order_relaxed) &&
        lnum > 0 && !atomic_load(&state->flsuspended) && state->current_script[0] != '\0') {

        boolean flbreakpoint = false;

        pthread_mutex_lock(&g_debug_mutex);

        for (int i = 0; i < MAX_BREAKPOINTS; i++) {
            if (g_breakpoints[i].active &&
                g_breakpoints[i].line == lnum &&
                strcasecmp(g_breakpoints[i].script, state->current_script) == 0) {
                flbreakpoint = true;
                break;
            }
        }

        pthread_mutex_unlock(&g_debug_mutex);

        if (flbreakpoint) {
            atomic_store(&state->lastlnum, lnum);

            /* Clear stepping state if we were stepping — breakpoint takes priority */
            atomic_store(&state->flstepping, false);
            atomic_store(&state->stepdir, DEBUG_STEP_NONE);

            atomic_store_explicit(&state->flsuspended, true, memory_order_seq_cst);
            debug_send_suspended(state->transport, state->threadid, (long)lnum, DEBUG_REASON_BREAKPOINT);
            log_debug(LOG_COMP_LANG, "debug: thread %ld hit breakpoint at %s line %ld",
                      state->threadid, state->current_script, (long)lnum);
        }
    }

    /* Watchpoint check (Phase 6) — if not already suspended and watchpoints
     * exist, check if any watched variable has changed value since last check.
     * Uses string representation comparison (hashgetvaluestring) to avoid
     * the dispose-both-inputs issue with EQvalue.
     *
     * Watchpoints fire on steppable nodes only (meaningful lines where values
     * could have changed). */
    if (atomic_load_explicit(&g_has_watchpoints, memory_order_relaxed) &&
        flsteppable && !atomic_load(&state->flsuspended)) {

        /* Get current local hash table — use the global currenthashtable
         * (which is correct since we hold the GIL and own this thread's
         * restored context) rather than reading from hglobals. */
        hdlhashtable htable = currenthashtable;

        /* Find the innermost local table */
        hdlhashtable hlocals = nil;
        hdlhashtable hwalk = htable;
        while (hwalk != nil) {
            if ((**hwalk).fllocaltable) {
                hlocals = hwalk;
                break;
            }
            hwalk = (**hwalk).prevhashtable;
        }

        if (hlocals != nil) {
            pthread_mutex_lock(&g_debug_mutex);

            for (int w = 0; w < MAX_WATCHPOINTS; w++) {
                if (!g_watchpoints[w].active)
                    continue;

                /* Look up the variable by name */
                bigstring bsname;
                int nlen = (int)strlen(g_watchpoints[w].varname);
                if (nlen > 255) nlen = 255;
                bsname[0] = (unsigned char)nlen;
                memcpy(bsname + 1, g_watchpoints[w].varname, (size_t)nlen);

                tyvaluerecord val;
                hdlhashnode hnode;
                if (!hashtablelookup(hlocals, bsname, &val, &hnode))
                    continue;

                /* Get current value as string */
                bigstring bsval;
                if (!hashgetvaluestring(val, bsval))
                    continue;

                char cval[DEBUG_VALUE_MAX];
                int vlen = bsval[0];
                if (vlen >= DEBUG_VALUE_MAX) vlen = DEBUG_VALUE_MAX - 1;
                memcpy(cval, bsval + 1, (size_t)vlen);
                cval[vlen] = '\0';

                if (!g_watchpoints[w].has_snapshot) {
                    /* First encounter — save snapshot, don't trigger */
                    memcpy(g_watchpoints[w].last_value, cval, (size_t)(vlen + 1));
                    g_watchpoints[w].has_snapshot = true;
                    continue;
                }

                /* Compare with last known value */
                if (strcmp(g_watchpoints[w].last_value, cval) != 0) {
                    /* Value changed! */
                    char old_value[DEBUG_VALUE_MAX];
                    memcpy(old_value, g_watchpoints[w].last_value, DEBUG_VALUE_MAX);
                    memcpy(g_watchpoints[w].last_value, cval, (size_t)(vlen + 1));

                    pthread_mutex_unlock(&g_debug_mutex);

                    /* Clear stepping state — watchpoint takes priority */
                    atomic_store(&state->flstepping, false);
                    atomic_store(&state->stepdir, DEBUG_STEP_NONE);
                    atomic_store(&state->lastlnum, lnum);

                    /* Send watchpoint notification with old/new values */
                    cJSON *notif = cJSON_CreateObject();
                    cJSON_AddNullToObject(notif, "id");
                    cJSON_AddStringToObject(notif, "op", "debug/suspended");
                    cJSON *params = cJSON_CreateObject();
                    cJSON_AddNumberToObject(params, "threadId", (double)state->threadid);
                    cJSON_AddNumberToObject(params, "line", (double)lnum);
                    cJSON_AddStringToObject(params, "reason", "watchpoint");
                    cJSON_AddStringToObject(params, "variable", g_watchpoints[w].varname);
                    cJSON_AddStringToObject(params, "oldValue", old_value);
                    cJSON_AddStringToObject(params, "newValue", cval);
                    cJSON_AddItemToObject(notif, "params", params);

                    char *json_str = cJSON_PrintUnformatted(notif);
                    if (json_str) {
                        atomic_store_explicit(&state->flsuspended, true, memory_order_seq_cst);
                        state->transport->write_line(state->transport->ctx, json_str, strlen(json_str));
                        free(json_str);
                    }
                    cJSON_Delete(notif);

                    log_debug(LOG_COMP_LANG, "debug: thread %ld watchpoint '%s' changed: '%s' -> '%s' at line %ld",
                              state->threadid, g_watchpoints[w].varname, old_value, cval, (long)lnum);

                    goto after_stepping; /* skip stepping logic, already suspended */
                }
            }

            pthread_mutex_unlock(&g_debug_mutex);
        }
    }

    /* Stepping logic — check if we should suspend based on step direction.
     * Uses simplified call depth model: calldepth tracks nesting relative
     * to the depth when stepping was initiated (steplevel).
     *
     * Step-into: suspend at the very next statement
     * Step-over: suspend when line changes at same or shallower call depth
     * Step-out:  suspend when call depth decreases below step level */
    if (atomic_load(&state->flstepping) && flsteppable && !atomic_load(&state->flsuspended)) {

        short diff = atomic_load(&state->calldepth) - atomic_load(&state->steplevel);
        boolean flstop = false;

        switch (atomic_load(&state->stepdir)) {

            case DEBUG_STEP_INTO:
                /* Stop at the very next statement */
                flstop = true;
                break;

            case DEBUG_STEP_OVER:
                if (diff == 0) {
                    /* Same call depth: stop when line changes.
                     * lastlnum is safe to read here — it was set while
                     * the debug thread was suspended, and the GIL
                     * happens-before guarantees visibility. */
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
            atomic_store(&state->stepdir, DEBUG_STEP_NONE);
            atomic_store(&state->lastlnum, lnum);

            /* Set suspended BEFORE notifying — ensures the thread is in the
             * suspended state before a fast client can react to the notification
             * and send a continue/step command. */
            atomic_store_explicit(&state->flsuspended, true, memory_order_seq_cst);
            debug_send_suspended(state->transport, state->threadid, (long)lnum, DEBUG_REASON_STEP);
            log_debug(LOG_COMP_LANG, "debug: thread %ld step completed at line %ld", state->threadid, (long)lnum);
        }
    }

after_stepping: /* label for watchpoint goto — skips stepping when watchpoint fires */

    /* Capture the thread globals handle in a local variable BEFORE releasing
     * the GIL. The global `hthreadglobals` is shared — other threads overwrite
     * it when they restore their own context. Using the global after reacquiring
     * the GIL would restore the WRONG thread's state (e.g., the main thread's
     * currenthashtable instead of this debug thread's), causing the
     * hlocals != currenthashtable assertion in evaluatelist. (#505) */
    hdlthreadglobals my_hglobals = hthreadglobals;

    /* Suspension loop — yields GIL so protocol handler can process commands */
    while (atomic_load(&state->flsuspended)) {

        if (atomic_load(&state->flkill)) {
            if (my_hglobals != nil)
                (**my_hglobals).flthreadkilled = true;
            return false;
        }

        /* Save thread globals, release GIL, sleep, reacquire, restore */
        headless_save_threadglobals(my_hglobals);
        pthread_mutex_unlock(&frontier_gil);

        /* Sleep 10ms — other threads (including protocol handler) can run */
        struct timespec ts = {0, 10000000}; /* 10ms */
        nanosleep(&ts, NULL);

        pthread_mutex_lock(&frontier_gil);
        headless_restore_threadglobals(my_hglobals);
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
        if (my_hglobals != nil)
            (**my_hglobals).flthreadkilled = true;
        return false;
    }

    return true;
}

/* ========================================================================
 * debug_init — Install the protocol debugger callback
 * ======================================================================== */

void debug_init(void) {

    langcallbacks.debuggercallback = &protocol_debugger_callback;
    langcallbacks.pushsourcecodecallback = &debug_push_sourcecode;
    langcallbacks.popsourcecodecallback = &debug_pop_sourcecode;
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

    /* Store debug state in thread globals for the callback to find,
     * and store thread globals in debug state for protocol handlers to
     * access the suspended thread's hash tables (debug/getLocals). */
    (**params->hglobals).param_reserved[0] = (void *)params->debugstate;
    params->debugstate->hglobals = (void *)params->hglobals;

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

    if (root == NULL) {
        char err[512];
        snprintf(err, sizeof(err), "{\"id\":%d,\"error\":{\"message\":\"Invalid JSON\"},\"success\":false}", id);
        transport->write_line(transport->ctx, err, strlen(err));
        return;
    }

    cJSON *params = cJSON_GetObjectItemCaseSensitive(root, "params");
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

    /* Note: flsuspended check is not under g_debug_mutex. In the current
     * single-client model this is safe (only one protocol handler thread).
     * Phase 5 (multi-session) will need to hold the lock across the
     * check-and-modify sequence to prevent concurrent continue/kill races. */
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
    atomic_store(&state->stepdir, (int)dir);
    atomic_store(&state->steplevel, atomic_load(&state->calldepth));
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

/* ========================================================================
 * Breakpoint protocol handlers (Phase 3)
 * ======================================================================== */

/*
 * debug/setBreakpoint — Set or clear a session breakpoint.
 *
 * Toggle behavior: if a breakpoint already exists at the given script+line,
 * it is cleared. Otherwise, a new breakpoint is set.
 *
 * Params:
 *   script: dotted path (e.g. "mainResponder.respond") — no leading "@"
 *   line:   1-based line number
 *
 * Returns:
 *   action: "set" or "cleared"
 *   script, line: echo back the breakpoint location
 */
void handle_debug_setbreakpoint(int id, const char *json_line, transport_t *transport) {

    cJSON *root = cJSON_Parse(json_line);

    if (root == NULL) {
        char err[512];
        snprintf(err, sizeof(err), "{\"id\":%d,\"error\":{\"message\":\"Invalid JSON\"},\"success\":false}", id);
        transport->write_line(transport->ctx, err, strlen(err));
        return;
    }

    cJSON *params = cJSON_GetObjectItemCaseSensitive(root, "params");
    cJSON *script_json = params ? cJSON_GetObjectItemCaseSensitive(params, "script") : NULL;
    cJSON *line_json = params ? cJSON_GetObjectItemCaseSensitive(params, "line") : NULL;

    if (!cJSON_IsString(script_json) || script_json->valuestring == NULL) {
        char err[512];
        snprintf(err, sizeof(err), "{\"id\":%d,\"error\":{\"message\":\"Missing 'script' in params\"},\"success\":false}", id);
        transport->write_line(transport->ctx, err, strlen(err));
        cJSON_Delete(root);
        return;
    }

    if (!cJSON_IsNumber(line_json)) {
        char err[512];
        snprintf(err, sizeof(err), "{\"id\":%d,\"error\":{\"message\":\"Missing 'line' in params\"},\"success\":false}", id);
        transport->write_line(transport->ctx, err, strlen(err));
        cJSON_Delete(root);
        return;
    }

    const char *script = script_json->valuestring;
    double line_raw = line_json->valuedouble;

    if (line_raw < 1.0 || line_raw > 1000000.0 || line_raw != (double)(unsigned long)line_raw) {
        char err[512];
        snprintf(err, sizeof(err), "{\"id\":%d,\"error\":{\"message\":\"Line must be a positive integer\"},\"success\":false}", id);
        transport->write_line(transport->ctx, err, strlen(err));
        cJSON_Delete(root);
        return;
    }

    unsigned long line = (unsigned long)line_raw;

    /* Strip leading "@" if present — normalize to dotted path */
    if (script[0] == '@')
        script++;

    if (strlen(script) >= DEBUG_SCRIPT_PATH_MAX) {
        char err[512];
        snprintf(err, sizeof(err), "{\"id\":%d,\"error\":{\"message\":\"Script path too long\"},\"success\":false}", id);
        transport->write_line(transport->ctx, err, strlen(err));
        cJSON_Delete(root);
        return;
    }

    /* Toggle: check if breakpoint already exists */
    boolean cleared = false;
    boolean set = false;

    pthread_mutex_lock(&g_debug_mutex);

    /* First pass: check for existing breakpoint to toggle off */
    for (int i = 0; i < MAX_BREAKPOINTS; i++) {
        if (g_breakpoints[i].active &&
            g_breakpoints[i].line == line &&
            strcasecmp(g_breakpoints[i].script, script) == 0) {
            g_breakpoints[i].active = false;
            cleared = true;
            break;
        }
    }

    /* Second pass: if not clearing, find an empty slot to set */
    if (!cleared) {
        for (int i = 0; i < MAX_BREAKPOINTS; i++) {
            if (!g_breakpoints[i].active) {
                /* strlen(script) < DEBUG_SCRIPT_PATH_MAX is guaranteed by the guard above */
                memcpy(g_breakpoints[i].script, script, strlen(script) + 1);
                g_breakpoints[i].line = line;
                g_breakpoints[i].active = true;
                set = true;
                break;
            }
        }
    }

    /* Update fast-path flag: check if any breakpoints remain active */
    boolean any_active = false;
    for (int i = 0; i < MAX_BREAKPOINTS; i++) {
        if (g_breakpoints[i].active) {
            any_active = true;
            break;
        }
    }
    atomic_store(&g_has_breakpoints, any_active);

    pthread_mutex_unlock(&g_debug_mutex);

    if (!cleared && !set) {
        char err[512];
        snprintf(err, sizeof(err), "{\"id\":%d,\"error\":{\"message\":\"Too many breakpoints (max %d)\"},\"success\":false}", id, MAX_BREAKPOINTS);
        transport->write_line(transport->ctx, err, strlen(err));
        cJSON_Delete(root);
        return;
    }

    /* Build response using cJSON to safely escape the script path */
    cJSON *resp = cJSON_CreateObject();
    cJSON_AddNumberToObject(resp, "id", id);
    cJSON *result = cJSON_CreateObject();
    cJSON_AddStringToObject(result, "action", cleared ? "cleared" : "set");
    cJSON_AddStringToObject(result, "script", script);
    cJSON_AddNumberToObject(result, "line", (double)line);
    cJSON_AddItemToObject(resp, "result", result);
    cJSON_AddBoolToObject(resp, "success", 1);

    char *json_str = cJSON_PrintUnformatted(resp);
    if (json_str) {
        transport->write_line(transport->ctx, json_str, strlen(json_str));
        free(json_str);
    }
    cJSON_Delete(resp);

    log_info(LOG_COMP_LANG, "debug: breakpoint %s at %s line %ld",
             cleared ? "cleared" : "set", script, line);

    cJSON_Delete(root);
}

/*
 * debug/listBreakpoints — List all session breakpoints.
 *
 * Returns an array of {script, line} objects.
 */
void handle_debug_listbreakpoints(int id, const char *json_line, transport_t *transport) {

    (void)json_line; /* no params needed */

    cJSON *resp = cJSON_CreateObject();
    cJSON_AddNumberToObject(resp, "id", id);

    cJSON *result = cJSON_CreateObject();
    cJSON *bparray = cJSON_CreateArray();

    pthread_mutex_lock(&g_debug_mutex);

    for (int i = 0; i < MAX_BREAKPOINTS; i++) {
        if (g_breakpoints[i].active) {
            cJSON *bp = cJSON_CreateObject();
            cJSON_AddStringToObject(bp, "script", g_breakpoints[i].script);
            cJSON_AddNumberToObject(bp, "line", (double)g_breakpoints[i].line);
            cJSON_AddStringToObject(bp, "type", "session");
            cJSON_AddItemToArray(bparray, bp);
        }
    }

    pthread_mutex_unlock(&g_debug_mutex);

    cJSON_AddItemToObject(result, "breakpoints", bparray);
    cJSON_AddItemToObject(resp, "result", result);
    cJSON_AddBoolToObject(resp, "success", 1);

    char *json_str = cJSON_PrintUnformatted(resp);
    if (json_str) {
        transport->write_line(transport->ctx, json_str, strlen(json_str));
        free(json_str);
    }
    cJSON_Delete(resp);
}

/*
 * debug/clearBreakpoints — Clear all session breakpoints.
 */
void handle_debug_clearbreakpoints(int id, const char *json_line, transport_t *transport) {

    (void)json_line; /* no params to validate */

    int cleared = 0;

    pthread_mutex_lock(&g_debug_mutex);

    for (int i = 0; i < MAX_BREAKPOINTS; i++) {
        if (g_breakpoints[i].active) {
            g_breakpoints[i].active = false;
            cleared++;
        }
    }

    atomic_store(&g_has_breakpoints, false);

    pthread_mutex_unlock(&g_debug_mutex);

    char resp[128];
    snprintf(resp, sizeof(resp),
             "{\"id\":%d,\"result\":{\"cleared\":%d},\"success\":true}", id, cleared);
    transport->write_line(transport->ctx, resp, strlen(resp));

    log_info(LOG_COMP_LANG, "debug: cleared %d breakpoints", cleared);
}

/* ========================================================================
 * Inspection protocol handlers (Phase 4)
 * ======================================================================== */

/*
 * Helper: parse threadId from JSON params and look up the debug state.
 * Returns the state (with refcount incremented) or NULL on error.
 * Sends an error response and cleans up root on failure.
 */
static tydebugstate *parse_thread_param(int id, const char *json_line,
                                         transport_t *transport, cJSON **out_root) {

    cJSON *root = cJSON_Parse(json_line);

    if (root == NULL) {
        char err[512];
        snprintf(err, sizeof(err), "{\"id\":%d,\"error\":{\"message\":\"Invalid JSON\"},\"success\":false}", id);
        transport->write_line(transport->ctx, err, strlen(err));
        *out_root = NULL;
        return NULL;
    }

    *out_root = root;

    cJSON *params = cJSON_GetObjectItemCaseSensitive(root, "params");
    cJSON *tid_json = params ? cJSON_GetObjectItemCaseSensitive(params, "threadId") : NULL;

    if (!cJSON_IsNumber(tid_json)) {
        char err[512];
        snprintf(err, sizeof(err), "{\"id\":%d,\"error\":{\"message\":\"Missing 'threadId' in params\"},\"success\":false}", id);
        transport->write_line(transport->ctx, err, strlen(err));
        cJSON_Delete(root);
        *out_root = NULL;
        return NULL;
    }

    long threadid = (long)tid_json->valuedouble;
    tydebugstate *state = debug_get_state_for_thread(threadid);

    if (state == NULL) {
        char err[512];
        snprintf(err, sizeof(err), "{\"id\":%d,\"error\":{\"message\":\"No debug thread with that ID\"},\"success\":false}", id);
        transport->write_line(transport->ctx, err, strlen(err));
        cJSON_Delete(root);
        *out_root = NULL;
        return NULL;
    }

    if (!atomic_load(&state->flsuspended)) {
        debug_release_state(state);
        char err[512];
        snprintf(err, sizeof(err), "{\"id\":%d,\"error\":{\"message\":\"Thread %ld is not suspended\"},\"success\":false}", id, threadid);
        transport->write_line(transport->ctx, err, strlen(err));
        cJSON_Delete(root);
        *out_root = NULL;
        return NULL;
    }

    return state;
}

/*
 * debug/getLocals — Inspect local variables of a suspended debug thread.
 *
 * When a debug thread is suspended, its thread globals contain the current
 * hash table context. The local variables are in the hash table chain —
 * specifically, tables with fllocaltable set.
 *
 * Returns an array of {name, value, type} objects.
 */
void handle_debug_getlocals(int id, const char *json_line, transport_t *transport) {

    cJSON *root = NULL;
    tydebugstate *state = parse_thread_param(id, json_line, transport, &root);

    if (state == NULL)
        return;

    /* Access the suspended thread's hash table context.
     * Safe because the debug thread is in nanosleep and not touching globals. */
    hdlthreadglobals hg = (hdlthreadglobals)state->hglobals;
    hdlhashtable htable = (hg != nil) ? (**hg).hcurrenthashtable : nil;

    cJSON *resp = cJSON_CreateObject();
    cJSON_AddNumberToObject(resp, "id", id);
    cJSON *result = cJSON_CreateObject();
    cJSON *locals = cJSON_CreateArray();

    /* Walk the hash table chain looking for local tables */
    while (htable != nil) {
        if ((**htable).fllocaltable) {
            /* Enumerate entries in this local table via sorted list */
            hdlhashnode hnode = (**htable).hfirstsort;

            while (hnode != nil) {
                cJSON *entry = cJSON_CreateObject();

                /* Name: Pascal string in hashkey */
                bigstring bsname;
                copystring((**hnode).hashkey, bsname);
                char cname[256];
                int nlen = bsname[0];
                if (nlen >= (int)sizeof(cname)) nlen = (int)sizeof(cname) - 1;
                memcpy(cname, bsname + 1, (size_t)nlen);
                cname[nlen] = '\0';
                cJSON_AddStringToObject(entry, "name", cname);

                /* Value: convert to display string */
                bigstring bsval;
                if (hashgetvaluestring((**hnode).val, bsval)) {
                    char cval[256];
                    int vlen = bsval[0];
                    if (vlen >= (int)sizeof(cval)) vlen = (int)sizeof(cval) - 1;
                    memcpy(cval, bsval + 1, (size_t)vlen);
                    cval[vlen] = '\0';
                    cJSON_AddStringToObject(entry, "value", cval);
                } else {
                    cJSON_AddStringToObject(entry, "value", "(unknown)");
                }

                /* Type */
                bigstring bstype;
                if (langgettypestring((**hnode).val.valuetype, bstype)) {
                    char ctype[64];
                    int tlen = bstype[0];
                    if (tlen >= (int)sizeof(ctype)) tlen = (int)sizeof(ctype) - 1;
                    memcpy(ctype, bstype + 1, (size_t)tlen);
                    ctype[tlen] = '\0';
                    cJSON_AddStringToObject(entry, "type", ctype);
                }

                cJSON_AddItemToArray(locals, entry);
                hnode = (**hnode).sortedlink;
            }
            break; /* only enumerate the innermost local table */
        }
        htable = (**htable).prevhashtable;
    }

    cJSON_AddItemToObject(result, "locals", locals);
    if (state->current_script[0] != '\0')
        cJSON_AddStringToObject(result, "script", state->current_script);
    cJSON_AddNumberToObject(result, "line", (double)atomic_load(&state->lastlnum));
    cJSON_AddItemToObject(resp, "result", result);
    cJSON_AddBoolToObject(resp, "success", 1);

    char *json_str = cJSON_PrintUnformatted(resp);
    if (json_str) {
        transport->write_line(transport->ctx, json_str, strlen(json_str));
        free(json_str);
    }
    cJSON_Delete(resp);

    debug_release_state(state);
    cJSON_Delete(root);
}

/*
 * debug/getStack — View call stack of a suspended debug thread.
 *
 * Uses the script_stack from the push/pop sourcecode callbacks to
 * reconstruct the call chain.
 *
 * Returns an array of frames from outermost to innermost.
 */
void handle_debug_getstack(int id, const char *json_line, transport_t *transport) {

    cJSON *root = NULL;
    tydebugstate *state = parse_thread_param(id, json_line, transport, &root);

    if (state == NULL)
        return;

    cJSON *resp = cJSON_CreateObject();
    cJSON_AddNumberToObject(resp, "id", id);
    cJSON *result = cJSON_CreateObject();
    cJSON *frames = cJSON_CreateArray();

    /* Build stack from the script_stack (outermost to innermost).
     * script_stack[0] is the outermost caller, current_script is the
     * innermost (currently executing) script. */
    for (short i = 0; i < state->script_stack_depth; i++) {
        if (state->script_stack[i][0] != '\0') {
            cJSON *frame = cJSON_CreateObject();
            cJSON_AddNumberToObject(frame, "level", i + 1);
            cJSON_AddStringToObject(frame, "script", state->script_stack[i]);
            cJSON_AddItemToArray(frames, frame);
        }
    }

    /* Add current frame (innermost) */
    if (state->current_script[0] != '\0') {
        cJSON *frame = cJSON_CreateObject();
        cJSON_AddNumberToObject(frame, "level", state->script_stack_depth + 1);
        cJSON_AddStringToObject(frame, "script", state->current_script);
        cJSON_AddNumberToObject(frame, "line", (double)atomic_load(&state->lastlnum));
        cJSON_AddItemToArray(frames, frame);
    }

    cJSON_AddItemToObject(result, "frames", frames);
    cJSON_AddItemToObject(resp, "result", result);
    cJSON_AddBoolToObject(resp, "success", 1);

    char *json_str = cJSON_PrintUnformatted(resp);
    if (json_str) {
        transport->write_line(transport->ctx, json_str, strlen(json_str));
        free(json_str);
    }
    cJSON_Delete(resp);

    debug_release_state(state);
    cJSON_Delete(root);
}

/*
 * debug/getSource — View script source with line numbers.
 *
 * Resolves a script path in the ODB, extracts the outline text,
 * and returns line-by-line source with breakpoint and current-line markers.
 *
 * Does not require a suspended thread — can be called anytime.
 * If threadId is provided and the thread is suspended in this script,
 * the current execution line is marked.
 */
void handle_debug_getsource(int id, const char *json_line, transport_t *transport) {

    cJSON *root = cJSON_Parse(json_line);

    if (root == NULL) {
        char err[512];
        snprintf(err, sizeof(err), "{\"id\":%d,\"error\":{\"message\":\"Invalid JSON\"},\"success\":false}", id);
        transport->write_line(transport->ctx, err, strlen(err));
        return;
    }

    cJSON *params = cJSON_GetObjectItemCaseSensitive(root, "params");
    cJSON *script_json = params ? cJSON_GetObjectItemCaseSensitive(params, "script") : NULL;
    cJSON *tid_json = params ? cJSON_GetObjectItemCaseSensitive(params, "threadId") : NULL;

    if (!cJSON_IsString(script_json) || script_json->valuestring == NULL) {
        char err[512];
        snprintf(err, sizeof(err), "{\"id\":%d,\"error\":{\"message\":\"Missing 'script' in params\"},\"success\":false}", id);
        transport->write_line(transport->ctx, err, strlen(err));
        cJSON_Delete(root);
        return;
    }

    const char *script_path = script_json->valuestring;
    if (script_path[0] == '@')
        script_path++;

    /* Resolve the script path.
     * Use script/eval to evaluate string(scriptAddress) — simpler than
     * navigating the ODB directly and handles all edge cases. */

    /* Parse the dotted path to find the containing table and leaf name */
    bigstring bsfullpath;
    int pathlen = (int)strlen(script_path);
    if (pathlen > 255) pathlen = 255;
    bsfullpath[0] = (unsigned char)pathlen;
    memcpy(bsfullpath + 1, script_path, (size_t)pathlen);

    /* Find the last dot to split into table path + name */
    int lastdot = -1;
    for (int i = pathlen; i > 0; i--) {
        if (bsfullpath[i] == '.') {
            lastdot = i;
            break;
        }
    }

    if (lastdot < 0) {
        char err[512];
        snprintf(err, sizeof(err), "{\"id\":%d,\"error\":{\"message\":\"Script path must be fully qualified (e.g. system.temp.myFunc)\"},\"success\":false}", id);
        transport->write_line(transport->ctx, err, strlen(err));
        cJSON_Delete(root);
        return;
    }

    /* Split: table path is bsfullpath[1..lastdot-1], name is bsfullpath[lastdot+1..] */
    bigstring bstablepath, bsname;
    bstablepath[0] = (unsigned char)(lastdot - 1);
    memcpy(bstablepath + 1, bsfullpath + 1, (size_t)(lastdot - 1));

    int namelen = pathlen - lastdot;
    bsname[0] = (unsigned char)namelen;
    memcpy(bsname + 1, bsfullpath + lastdot + 1, (size_t)namelen);

    /* Navigate to the table */
    hdlhashtable htable;
    if (!langfastaddresstotable(roottable, bstablepath, &htable)) {
        char err[512];
        snprintf(err, sizeof(err), "{\"id\":%d,\"error\":{\"message\":\"Table not found in path\"},\"success\":false}", id);
        transport->write_line(transport->ctx, err, strlen(err));
        cJSON_Delete(root);
        return;
    }

    /* Look up the script */
    tyvaluerecord val;
    hdlhashnode hnode;
    if (!hashtablelookup(htable, bsname, &val, &hnode)) {
        char err[512];
        snprintf(err, sizeof(err), "{\"id\":%d,\"error\":{\"message\":\"Script not found\"},\"success\":false}", id);
        transport->write_line(transport->ctx, err, strlen(err));
        cJSON_Delete(root);
        return;
    }

    /* Get the script text via opgetlangtext. The protocol handler holds the GIL
     * (acquired before dispatch in protocol_handler.c), so ODB operations are safe. */
    Handle htext = nil;

    if (val.valuetype == externalvaluetype) {
        hdlexternalvariable hv = (hdlexternalvariable)val.data.externalvalue;

        /* Scripts must be in memory to extract source. Compiled/run scripts are
         * always loaded. Scripts never accessed in this session may still be on
         * disk — loading requires db context work deferred to a follow-up. */
        if (!(**hv).flinmemory) {
            char err[512];
            snprintf(err, sizeof(err),
                     "{\"id\":%d,\"error\":{\"message\":\"Script not loaded in memory (try running it first)\"},\"success\":false}", id);
            transport->write_line(transport->ctx, err, strlen(err));
            cJSON_Delete(root);
            return;
        }

        hdloutlinerecord houtline = (hdloutlinerecord)(**hv).variabledata;

        if (houtline != nil) {
            oppushoutline(houtline);
            opgetlangtext(houtline, false, &htext);
            oppopoutline();
        }
    }

    if (htext == nil) {
        char err[512];
        snprintf(err, sizeof(err), "{\"id\":%d,\"error\":{\"message\":\"Could not get script source\"},\"success\":false}", id);
        transport->write_line(transport->ctx, err, strlen(err));
        cJSON_Delete(root);
        return;
    }

    /* Determine current line if threadId is provided */
    long current_line = -1;
    if (cJSON_IsNumber(tid_json)) {
        long threadid = (long)tid_json->valuedouble;
        tydebugstate *dbgstate = debug_get_state_for_thread(threadid);
        if (dbgstate != NULL) {
            if (atomic_load(&dbgstate->flsuspended) &&
                strcasecmp(dbgstate->current_script, script_path) == 0) {
                current_line = (long)atomic_load(&dbgstate->lastlnum);
            }
            debug_release_state(dbgstate);
        }
    }

    /* Build line-by-line response.
     * opgetlangtext uses CR (\r) as line separator. */
    long textlen = GetHandleSize(htext);
    char *text = *htext;

    cJSON *resp = cJSON_CreateObject();
    cJSON_AddNumberToObject(resp, "id", id);
    cJSON *result_obj = cJSON_CreateObject();
    cJSON_AddStringToObject(result_obj, "script", script_path);
    if (current_line > 0)
        cJSON_AddNumberToObject(result_obj, "currentLine", (double)current_line);

    /* Pre-scan breakpoints for this script to avoid O(lines * MAX_BREAKPOINTS) */
    unsigned long bp_lines[MAX_BREAKPOINTS];
    int bp_count = 0;
    pthread_mutex_lock(&g_debug_mutex);
    for (int b = 0; b < MAX_BREAKPOINTS; b++) {
        if (g_breakpoints[b].active &&
            strcasecmp(g_breakpoints[b].script, script_path) == 0) {
            bp_lines[bp_count++] = g_breakpoints[b].line;
        }
    }
    pthread_mutex_unlock(&g_debug_mutex);

    cJSON *lines = cJSON_CreateArray();
    long linenum = 1;
    long linestart = 0;

    for (long i = 0; i <= textlen; i++) {
        if (i == textlen || text[i] == '\r' || text[i] == '\n') {
            /* Extract this line */
            long linelen = i - linestart;
            char linebuf[4096];
            if (linelen >= (long)sizeof(linebuf))
                linelen = (long)sizeof(linebuf) - 1;
            memcpy(linebuf, text + linestart, (size_t)linelen);
            linebuf[linelen] = '\0';

            cJSON *lineobj = cJSON_CreateObject();
            cJSON_AddNumberToObject(lineobj, "num", (double)linenum);
            cJSON_AddStringToObject(lineobj, "text", linebuf);

            /* Check if this line has a breakpoint */
            boolean hasbp = false;
            for (int b = 0; b < bp_count; b++) {
                if (bp_lines[b] == (unsigned long)linenum) {
                    hasbp = true;
                    break;
                }
            }
            cJSON_AddBoolToObject(lineobj, "breakpoint", hasbp);

            if (linenum == current_line)
                cJSON_AddBoolToObject(lineobj, "current", 1);

            cJSON_AddItemToArray(lines, lineobj);
            linenum++;
            linestart = i + 1;
        }
    }

    disposehandle(htext);

    cJSON_AddItemToObject(result_obj, "lines", lines);
    cJSON_AddItemToObject(resp, "result", result_obj);
    cJSON_AddBoolToObject(resp, "success", 1);

    char *json_str = cJSON_PrintUnformatted(resp);
    if (json_str) {
        transport->write_line(transport->ctx, json_str, strlen(json_str));
        free(json_str);
    }
    cJSON_Delete(resp);
    cJSON_Delete(root);
}

/*
 * debug/listThreads — List all active debug threads (Phase 5).
 *
 * Returns an array of {threadId, suspended, script, line} objects
 * for all currently registered debug threads.
 */
void handle_debug_listthreads(int id, const char *json_line, transport_t *transport) {

    (void)json_line; /* no params to validate */

    cJSON *resp = cJSON_CreateObject();
    cJSON_AddNumberToObject(resp, "id", id);

    cJSON *result = cJSON_CreateObject();
    cJSON *threads = cJSON_CreateArray();

    /* We hold g_debug_mutex for the entire loop, so no debug thread can
     * unregister (debug_unregister_thread NULLs the slot under this mutex)
     * or be freed (refcount drop to 0 requires unregistration first).
     * This makes direct pointer access safe without incrementing refcount. */
    pthread_mutex_lock(&g_debug_mutex);

    for (int i = 0; i < MAX_DEBUG_THREADS; i++) {
        if (g_debug_threads[i] != NULL) {
            tydebugstate *state = g_debug_threads[i];
            cJSON *thread = cJSON_CreateObject();
            cJSON_AddNumberToObject(thread, "threadId", (double)state->threadid);
            boolean suspended = atomic_load(&state->flsuspended);
            cJSON_AddBoolToObject(thread, "suspended", suspended);
            /* Only read current_script and lastlnum when suspended — a running
             * thread may be writing these fields concurrently via the push/pop
             * sourcecode callbacks under the GIL (not g_debug_mutex). */
            if (suspended) {
                if (state->current_script[0] != '\0')
                    cJSON_AddStringToObject(thread, "script", state->current_script);
                cJSON_AddNumberToObject(thread, "line", (double)atomic_load(&state->lastlnum));
            }
            cJSON_AddItemToArray(threads, thread);
        }
    }

    pthread_mutex_unlock(&g_debug_mutex);

    cJSON_AddItemToObject(result, "threads", threads);
    cJSON_AddItemToObject(resp, "result", result);
    cJSON_AddBoolToObject(resp, "success", 1);

    char *json_str = cJSON_PrintUnformatted(resp);
    if (json_str) {
        transport->write_line(transport->ctx, json_str, strlen(json_str));
        free(json_str);
    }
    cJSON_Delete(resp);
}

/* ========================================================================
 * Watchpoint protocol handlers (Phase 6)
 * ======================================================================== */

/*
 * debug/setWatchpoint — Set or clear a watchpoint on a variable.
 *
 * Toggle behavior: if a watchpoint already exists for the variable,
 * it is cleared. Otherwise, a new watchpoint is set.
 *
 * Params:
 *   variable: name of the variable to watch (e.g. "x", "msg")
 */
void handle_debug_setwatchpoint(int id, const char *json_line, transport_t *transport) {

    cJSON *root = cJSON_Parse(json_line);

    if (root == NULL) {
        char err[512];
        snprintf(err, sizeof(err), "{\"id\":%d,\"error\":{\"message\":\"Invalid JSON\"},\"success\":false}", id);
        transport->write_line(transport->ctx, err, strlen(err));
        return;
    }

    cJSON *params_json = cJSON_GetObjectItemCaseSensitive(root, "params");
    cJSON *var_json = params_json ? cJSON_GetObjectItemCaseSensitive(params_json, "variable") : NULL;

    if (!cJSON_IsString(var_json) || var_json->valuestring == NULL) {
        char err[512];
        snprintf(err, sizeof(err), "{\"id\":%d,\"error\":{\"message\":\"Missing 'variable' in params\"},\"success\":false}", id);
        transport->write_line(transport->ctx, err, strlen(err));
        cJSON_Delete(root);
        return;
    }

    const char *varname = var_json->valuestring;

    if (strlen(varname) >= DEBUG_VARNAME_MAX) {
        char err[512];
        snprintf(err, sizeof(err), "{\"id\":%d,\"error\":{\"message\":\"Variable name too long\"},\"success\":false}", id);
        transport->write_line(transport->ctx, err, strlen(err));
        cJSON_Delete(root);
        return;
    }

    boolean cleared = false;
    boolean set = false;

    pthread_mutex_lock(&g_debug_mutex);

    /* First pass: check for existing watchpoint to toggle off */
    for (int i = 0; i < MAX_WATCHPOINTS; i++) {
        if (g_watchpoints[i].active &&
            strcmp(g_watchpoints[i].varname, varname) == 0) {
            g_watchpoints[i].active = false;
            cleared = true;
            break;
        }
    }

    /* Second pass: if not clearing, find an empty slot */
    if (!cleared) {
        for (int i = 0; i < MAX_WATCHPOINTS; i++) {
            if (!g_watchpoints[i].active) {
                memcpy(g_watchpoints[i].varname, varname, strlen(varname) + 1);
                g_watchpoints[i].has_snapshot = false;
                g_watchpoints[i].last_value[0] = '\0';
                g_watchpoints[i].active = true;
                set = true;
                break;
            }
        }
    }

    /* Update fast-path flag */
    boolean any_active = false;
    for (int i = 0; i < MAX_WATCHPOINTS; i++) {
        if (g_watchpoints[i].active) {
            any_active = true;
            break;
        }
    }
    atomic_store(&g_has_watchpoints, any_active);

    pthread_mutex_unlock(&g_debug_mutex);

    if (!cleared && !set) {
        char err[512];
        snprintf(err, sizeof(err), "{\"id\":%d,\"error\":{\"message\":\"Too many watchpoints (max %d)\"},\"success\":false}", id, MAX_WATCHPOINTS);
        transport->write_line(transport->ctx, err, strlen(err));
        cJSON_Delete(root);
        return;
    }

    cJSON *resp = cJSON_CreateObject();
    cJSON_AddNumberToObject(resp, "id", id);
    cJSON *result = cJSON_CreateObject();
    cJSON_AddStringToObject(result, "action", cleared ? "cleared" : "set");
    cJSON_AddStringToObject(result, "variable", varname);
    cJSON_AddItemToObject(resp, "result", result);
    cJSON_AddBoolToObject(resp, "success", 1);

    char *json_str = cJSON_PrintUnformatted(resp);
    if (json_str) {
        transport->write_line(transport->ctx, json_str, strlen(json_str));
        free(json_str);
    }
    cJSON_Delete(resp);

    log_info(LOG_COMP_LANG, "debug: watchpoint %s on '%s'",
             cleared ? "cleared" : "set", varname);

    cJSON_Delete(root);
}

/*
 * debug/listWatchpoints — List all active watchpoints.
 */
void handle_debug_listwatchpoints(int id, const char *json_line, transport_t *transport) {

    (void)json_line; /* no params to validate */

    cJSON *resp = cJSON_CreateObject();
    cJSON_AddNumberToObject(resp, "id", id);

    cJSON *result = cJSON_CreateObject();
    cJSON *wparray = cJSON_CreateArray();

    pthread_mutex_lock(&g_debug_mutex);

    for (int i = 0; i < MAX_WATCHPOINTS; i++) {
        if (g_watchpoints[i].active) {
            cJSON *wp = cJSON_CreateObject();
            cJSON_AddStringToObject(wp, "variable", g_watchpoints[i].varname);
            if (g_watchpoints[i].has_snapshot)
                cJSON_AddStringToObject(wp, "lastValue", g_watchpoints[i].last_value);
            cJSON_AddItemToArray(wparray, wp);
        }
    }

    pthread_mutex_unlock(&g_debug_mutex);

    cJSON_AddItemToObject(result, "watchpoints", wparray);
    cJSON_AddItemToObject(resp, "result", result);
    cJSON_AddBoolToObject(resp, "success", 1);

    char *json_str = cJSON_PrintUnformatted(resp);
    if (json_str) {
        transport->write_line(transport->ctx, json_str, strlen(json_str));
        free(json_str);
    }
    cJSON_Delete(resp);
}

/*
 * debug/clearWatchpoints — Clear all watchpoints.
 */
void handle_debug_clearwatchpoints(int id, const char *json_line, transport_t *transport) {

    (void)json_line; /* no params to validate */

    int cleared_count = 0;

    pthread_mutex_lock(&g_debug_mutex);

    for (int i = 0; i < MAX_WATCHPOINTS; i++) {
        if (g_watchpoints[i].active) {
            g_watchpoints[i].active = false;
            cleared_count++;
        }
    }

    atomic_store(&g_has_watchpoints, false);

    pthread_mutex_unlock(&g_debug_mutex);

    char resp[128];
    snprintf(resp, sizeof(resp),
             "{\"id\":%d,\"result\":{\"cleared\":%d},\"success\":true}", id, cleared_count);
    transport->write_line(transport->ctx, resp, strlen(resp));
}
