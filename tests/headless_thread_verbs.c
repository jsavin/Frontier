/*
 * headless_thread_verbs.c - Thread processor verb implementations
 *
 * Implements cooperative threading for headless mode using a Global Interpreter
 * Lock (GIL) pattern. Threads are real POSIX threads, but only one runs at a
 * time — the GIL holder. This serializes access to C globals while allowing
 * threads to yield at sleep points and langbackgroundtask() callbacks.
 *
 * Execution model:
 * - thread.evaluate() / thread.callscript() spawn a real pthread that blocks
 *   on GIL acquisition, then return the thread ID immediately.
 * - langbackgroundtask() (called at loop boundaries) releases the GIL, yields,
 *   then reacquires — allowing spawned threads to run.
 * - thread.sleepTicks() releases the GIL, sleeps on a condvar (only blocking
 *   the OS thread), then reacquires the GIL.
 *
 * Thread IDs are allocated by the singleton thread registry (threadregistry.c).
 * The main thread is registered with ID 2 (idapplicationthread) at startup.
 * Spawned threads get IDs starting from 3.
 */

#include "frontier.h"
#include "standard.h"

#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "langinternal.h"
#include "langexternal.h"
#include "tablestructure.h"
#include "tableverbs.h"
#include "process.h"
#include "shellthreads.h"
#include "logging.h"
#include "file.h"  /* fifcloseallfiles */
#include "threadregistry.h"
#include "processinternal.h"
#include "script_portable.h"
#include <errno.h>
#include <sched.h>

/* Constant for tick-to-second conversion (classic Mac ticks = 60/sec) */
#define TICKS_PER_SECOND 60

/* Token enum for all verbs in the thread processor.
 * Naming convention: thrv_<verbname> for thread verbs */
enum {
    thrv_exists = 0,
    thrv_evaluate = 1,
    thrv_callscript = 2,
    thrv_getcurrentid = 3,
    thrv_getcount = 4,
    thrv_getnthid = 5,
    thrv_sleep = 6,
    thrv_sleepfor = 7,
    thrv_sleepticks = 8,
    thrv_issleeping = 9,
    thrv_wake = 10,
    thrv_kill = 11,
    thrv_gettimeslice = 12,
    thrv_settimeslice = 13,
    thrv_getdefaulttimeslice = 14,
    thrv_setdefaulttimeslice = 15,
    thrv_getstats = 16
};

/*
 * Forward declarations for cooperative threading functions
 * (defined in headless_threadglobals.c)
 */
extern hdlthreadglobals headless_new_threadglobals(void);
extern void headless_dispose_threadglobals(hdlthreadglobals hg);
extern void headless_save_threadglobals(hdlthreadglobals hg);
extern void headless_restore_threadglobals(hdlthreadglobals hg);

/*
 * Global Interpreter Lock (GIL)
 *
 * Only the thread holding frontier_gil may read/write C globals (fllangerror,
 * hashtablestack, hthreadglobals, etc.). Every yield point (langbackgroundtask,
 * thread.sleep) saves globals, releases the GIL, and reacquires before restoring.
 *
 * gil_available is broadcast whenever the GIL is released, so threads blocked
 * on acquisition can wake up and try to lock it.
 */
static pthread_mutex_t frontier_gil = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t gil_available = PTHREAD_COND_INITIALIZER;

/*
 * headless_threading_init - Main thread acquires the GIL at startup
 *
 * Must be called after register_main_thread() and before any scripts run.
 */
void headless_threading_init(void) {
    pthread_mutex_lock(&frontier_gil);
}

/*
 * headless_threading_shutdown - Kill spawned threads and release the GIL
 *
 * Sets flthreadkilled on all non-main threads so they exit at their next
 * langbackgroundtask() or thread.sleep() yield point. Releases the GIL to
 * let them run and detect the kill flag, then waits for them to finish.
 * Must be called before cleanup_thread_registry() and unload_system_root_database().
 */
void headless_threading_shutdown(void) {
    long main_id = (long)(**hthreadglobals).idthread;
    hdlthreadglobals main_globals = hthreadglobals;

    /* Save main thread globals before releasing the GIL */
    headless_save_threadglobals(main_globals);

    /* Kill all non-main threads by setting their kill flags */
    {
        int count = get_thread_count();

        for (int i = 1; i <= count; i++) {
            long tid = get_nth_thread_id((long)i);

            if (tid != 0 && tid != main_id) {
                frontier_pthread_record *rec = get_thread_by_id(tid);

                if (rec != NULL) {
                    pthread_mutex_lock(&rec->state_mutex);
                    rec->is_killed = true;
                    pthread_cond_signal(&rec->wake_cond);
                    pthread_mutex_unlock(&rec->state_mutex);

                    if (rec->hglobals != nil)
                        (**rec->hglobals).flthreadkilled = true;

                    release_thread_record(rec);
                }
            }
        }
    }

    /* Release the GIL so killed threads can wake up and exit */
    pthread_mutex_unlock(&frontier_gil);
    pthread_cond_broadcast(&gil_available);

    /* Wait for all spawned threads to finish (thread count == 1 means only main).
     * We must block here until all workers exit — proceeding to
     * cleanup_thread_registry() or unload_system_root_database() with active
     * threads would crash (stale pointers to freed ODB state). */
    {
        int attempts = 0;
        const int max_attempts = 300; /* 300 * 10ms = 3 seconds initial wait */

        while (get_thread_count() > 1 && attempts < max_attempts) {
            struct timespec ts = {0, 10000000}; /* 10ms */
            nanosleep(&ts, NULL);
            attempts++;
        }

        if (get_thread_count() > 1) {
            /* Threads are still active after initial timeout. This is a serious
             * problem — we cannot safely proceed with cleanup. Log and extend
             * the wait with a hard upper bound to avoid hanging forever. */
            int remaining = get_thread_count() - 1;

            log_warn(LOG_COMP_THREAD,
                "Shutdown: %d thread(s) still active after 3s, extending wait...",
                remaining);

            /* Extended wait: another 7 seconds (total 10s hard limit) */
            int ext_attempts = 0;
            const int ext_max = 700; /* 700 * 10ms = 7 seconds */

            while (get_thread_count() > 1 && ext_attempts < ext_max) {
                struct timespec ts = {0, 10000000}; /* 10ms */
                nanosleep(&ts, NULL);
                ext_attempts++;
            }

            if (get_thread_count() > 1)
                log_error(LOG_COMP_THREAD,
                    "Shutdown: %d thread(s) still active after 10s hard limit — "
                    "proceeding with cleanup (may crash)",
                    get_thread_count() - 1);
        }
    }

    /* Re-acquire the GIL before restoring globals. We released it above to let
     * spawned threads run; now we need it back for the cleanup path. */
    pthread_mutex_lock(&frontier_gil);

    /* Restore main thread globals — spawned threads may have left C globals
     * pointing to their (now-freed) state. */
    headless_restore_threadglobals(main_globals);
}

/* Forward declarations for static functions used by thread_entry_point */
static boolean headless_unregister_thread(long idthread);

/*
 * Thread launch parameters - passed from spawning thread to new POSIX thread
 */
typedef struct {
    hdltreenode hcode;
    hdlthreadglobals hglobals;
    frontier_pthread_record *rec;
    /* For callscript: */
    hdlhashtable htable;
    bigstring bsverb;
    tyvaluerecord vparams;
    hdlhashtable hcontext;
    boolean is_callscript;
} thread_launch_params;

/*
 * thread_entry_point - POSIX thread entry for spawned threads
 *
 * Acquires the GIL, restores this thread's globals, executes the code,
 * then cleans up. The thread blocks on GIL acquisition until the spawning
 * thread yields via langbackgroundtask() or thread.sleep().
 */
static void *thread_entry_point(void *arg) {
    thread_launch_params *params = (thread_launch_params *)arg;
    tyvaluerecord result;

    if (params == NULL) {
        log_error(LOG_COMP_THREAD, "thread_entry_point: NULL params — aborting thread");
        return NULL;
    }

    /* Block until we can acquire the GIL */
    pthread_mutex_lock(&frontier_gil);

    /* Restore this thread's globals (makes C globals point to our state) */
    headless_restore_threadglobals(params->hglobals);

    /* Execute the code */
    initvalue(&result, novaluetype);

    if (params->is_callscript) {
        langrunscriptcode(params->htable, params->bsverb, params->hcode,
                          &params->vparams, params->hcontext, &result);
    }
    else {
        langruncode(params->hcode, nil, &result);
    }

    /* Save our globals before cleanup (while we still hold the GIL) */
    headless_save_threadglobals(params->hglobals);

    /* Clear global error buffer (fire-and-forget) */
    headless_clear_last_lang_error();

    /* Unregister from system.compiler.threads */
    headless_unregister_thread(params->rec->user_thread_id);

    /* Cleanup */
    if (!params->is_callscript) {
        /* For evaluate: we compiled hcode, so we own it */
        langdisposetree(params->hcode);
    }
    else {
        /* For callscript: hcode belongs to the hash table, do NOT dispose.
         * Dispose the deep-copied vparams (we own it from copyvaluerecord). */
        disposevaluerecord(params->vparams, false);
    }

    disposevaluerecord(result, false);
    headless_dispose_threadglobals(params->hglobals);
    free_thread_record(params->rec);
    free(params);

    /* Release the GIL and signal other threads */
    pthread_mutex_unlock(&frontier_gil);
    pthread_cond_broadcast(&gil_available);

    return NULL;
}

/*
 * callback_thread_entry_point - POSIX thread entry for TCP callback threads
 *
 * Similar to thread_entry_point but tailored for fire-and-forget callbacks:
 * - Performs error cleanup (fifcloseallfiles, langreleasesemaphores) on failure,
 *   matching the one-shot process cleanup in process.c:2565-2576
 * - Does not register in system.compiler.threads (callbacks are transient)
 * - Always disposes hcode (callback AST is owned by this thread)
 */
static void *callback_thread_entry_point(void *arg) {
    thread_launch_params *params = (thread_launch_params *)arg;
    tyvaluerecord result;
    boolean fl;

    if (params == NULL) {
        log_error(LOG_COMP_THREAD, "callback_thread_entry_point: NULL params — aborting thread");
        return NULL;
    }

    /* Block until we can acquire the GIL */
    pthread_mutex_lock(&frontier_gil);

    /* Restore this thread's globals (makes C globals point to our state) */
    headless_restore_threadglobals(params->hglobals);

    /* Execute the callback */
    initvalue(&result, novaluetype);

    fl = langruncode(params->hcode, nil, &result);

    if (!fl) {
        /* Error cleanup matching process.c one-shot behavior (lines 2565-2576).
         *
         * fifcloseallfiles(0L): In process.c, the refcon is (long)hp (the process
         * handle). In headless mode, process.c is not compiled — there are no
         * process handles. File verbs in headless mode open files with refcon 0,
         * so 0L is the correct value here.
         *
         * langreleasesemaphores(nil): The hp parameter is #pragma unused in the
         * implementation — it uses getcurrentthreadglobals() internally. nil is
         * functionally equivalent to passing a process handle. */
        fifcloseallfiles(0L);
        langreleasesemaphores(nil);
        log_warn(LOG_COMP_LANG, "callback_thread_entry_point: langruncode failed for stream_id=%ld",
                 params->rec->user_thread_id);
    }

    /* Save our globals before cleanup (while we still hold the GIL) */
    headless_save_threadglobals(params->hglobals);

    /* Clear global error buffer (fire-and-forget) */
    headless_clear_last_lang_error();

    /* Cleanup */
    langdisposetree(params->hcode);
    disposevaluerecord(result, false);
    headless_dispose_threadglobals(params->hglobals);
    free_thread_record(params->rec);
    free(params);

    /* Release the GIL and signal other threads */
    pthread_mutex_unlock(&frontier_gil);
    pthread_cond_broadcast(&gil_available);

    return NULL;
}

/*
 * headless_spawn_callback_thread - Spawn a GIL-aware thread for a TCP callback
 *
 * Takes ownership of hcode. The caller must hold the GIL.
 * The spawned thread blocks on GIL acquisition until the main thread yields
 * at the next langbackgroundtask() call.
 *
 * stream_id is used only for logging — the callback AST already encodes
 * the stream and refcon as parameters in the function call tree.
 */
boolean headless_spawn_callback_thread(hdltreenode hcode, long stream_id) {
    frontier_pthread_record *rec = nil;
    hdlthreadglobals new_hglobals = nil;
    thread_launch_params *params = nil;
    pthread_t tid;
    pthread_attr_t attr;

    /* Allocate thread record from registry */
    rec = allocate_thread_record();

    if (rec == NULL) {
        langdisposetree(hcode);
        return false;
    }

    /* Allocate new thread globals */
    new_hglobals = headless_new_threadglobals();

    if (new_hglobals == nil) {
        langdisposetree(hcode);
        free_thread_record(rec);
        return false;
    }

    /* Set thread ID in the new globals */
    (**new_hglobals).idthread = (hdlthread) rec->user_thread_id;

    /* Link globals to registry record */
    rec->hglobals = new_hglobals;

    /* Deep-copy the calling thread's hashtablestack so the callback gets its
     * own stack pointer/index state. Shared underlying hash table pointers are
     * safe under GIL serialization. */
    {
        Handle hcopy;

        if (!newfilledhandle((char *)(*hashtablestack), sizeof(tytablestack), &hcopy)) {
            langdisposetree(hcode);
            headless_dispose_threadglobals(new_hglobals);
            free_thread_record(rec);
            return false;
        }

        (**new_hglobals).htablestack = (hdltablestack)hcopy;
    }
    (**new_hglobals).hcurrenthashtable = currenthashtable;

    /* Package launch parameters */
    params = (thread_launch_params *)malloc(sizeof(thread_launch_params));

    if (params == NULL) {
        langdisposetree(hcode);
        headless_dispose_threadglobals(new_hglobals);
        free_thread_record(rec);
        return false;
    }

    params->hcode = hcode;
    params->hglobals = new_hglobals;
    params->rec = rec;
    params->is_callscript = false;

    /* Spawn detached POSIX thread — it will block on GIL until main thread yields */
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    if (pthread_create(&tid, &attr, callback_thread_entry_point, params) != 0) {
        log_error(LOG_COMP_THREAD, "pthread_create failed for TCP callback (stream_id=%ld)", stream_id);
        pthread_attr_destroy(&attr);
        langdisposetree(hcode);
        headless_dispose_threadglobals(new_hglobals);
        free_thread_record(rec);
        free(params);
        return false;
    }

    pthread_attr_destroy(&attr);
    rec->pthread_id = tid;

    log_debug(LOG_COMP_THREAD, "headless_spawn_callback_thread: spawned thread %ld for stream_id=%ld",
              rec->user_thread_id, stream_id);

    return true;
}

/*
 * Thread table registration helpers for system.compiler.threads
 *
 * These implement the add/remove pattern from legacy Frontier's
 * initprocessthread/exitprocessthread (process.c:2117, 2193).
 */

/*
 * headless_register_thread - Register thread in system.compiler.threads table
 *
 * Adds an entry mapping the thread name to its ID. Handles name collisions
 * by appending -1, -2, etc. (matching process.c:2152-2173).
 */
static boolean headless_register_thread(bigstring bsname, long idthread) {
    tyvaluerecord val;
    bigstring bs;

    if (threadtable == nil)
        return true;  /* table not initialized yet — not an error */

    setlongvalue(idthread, &val);
    copystring(bsname, bs);

    pushhashtable(threadtable);

    /* Handle name collisions with -1, -2 suffixes (matches process.c:2152-2173) */
    if (hashsymbolexists(bs)) {
        int len = stringlength(bs) + 1;
        int x = 0;

        pushchar('-', bs);

        do {
            setstringlength(bs, len);
            pushlong(++x, bs);
        } while (hashsymbolexists(bs));
    }

    hashinsert(bs, val);
    pophashtable();
    return true;
}

/*
 * Inverse search visitor - finds a hash node whose long value matches the target
 */
static boolean findthreadbyidvisit(bigstring bsname, hdlhashnode hnode, tyvaluerecord val, ptrvoid refcon) {
#pragma unused(bsname, hnode)
    long target = (long)(intptr_t)refcon;

    if (val.valuetype == longvaluetype && val.data.longvalue == target)
        return true;  /* Found it - stop searching */

    return false;  /* Keep searching */
}

/*
 * headless_unregister_thread - Remove thread from system.compiler.threads table
 */
static boolean headless_unregister_thread(long idthread) {
    bigstring bsname;

    if (threadtable == nil)
        return true;

    if (hashinversesearch(threadtable, &findthreadbyidvisit, (ptrvoid)(intptr_t)idthread, bsname)) {
        pushhashtable(threadtable);
        hashdelete(bsname, true, true);
        pophashtable();
    }

    return true;
}

/*
 * headless_thread_evaluate - Spawn a real POSIX thread for thread.evaluate
 *
 * Compiles the code string, allocates thread resources, then spawns a detached
 * POSIX thread that blocks on the GIL. Returns the thread ID immediately.
 * The spawned thread will run when the calling thread yields (langbackgroundtask
 * or thread.sleep).
 *
 * Return semantics:
 * - Compilation failure (bad syntax) → returns false (error propagates to caller)
 * - Resource allocation failure → returns false
 * - Successful spawn → returns true with thread ID. Runtime errors in the
 *   spawned thread are fire-and-forget.
 */
static boolean headless_thread_evaluate(bigstring bscode, tyvaluerecord *vreturned) {
    hdltreenode hcode = nil;
    frontier_pthread_record *rec = nil;
    hdlthreadglobals new_hglobals = nil;
    boolean fl;
    long threadid;
    Handle htext = nil;
    long codelen;
    bigstring bsanon;
    thread_launch_params *params = nil;
    pthread_t tid;
    pthread_attr_t attr;

    /* Compile the code string into a tree */
    codelen = stringlength(bscode);

    if (!newfilledhandle(stringbaseaddress(bscode), codelen, &htext))
        return false;

    fl = langbuildtree(htext, true, &hcode); /* langbuildtree disposes htext */

    if (!fl || hcode == nil) {
        if (hcode != nil)
            langdisposetree(hcode);
        return false;
    }

    /* Allocate thread record from registry */
    rec = allocate_thread_record();

    if (rec == NULL) {
        langdisposetree(hcode);
        return false;
    }

    threadid = rec->user_thread_id;

    /* Allocate new thread globals */
    new_hglobals = headless_new_threadglobals();

    if (new_hglobals == nil) {
        langdisposetree(hcode);
        free_thread_record(rec);
        return false;
    }

    /* Set thread ID in the new globals */
    (**new_hglobals).idthread = (hdlthread) threadid;

    /* Link globals to registry record */
    rec->hglobals = new_hglobals;

    /* Deep-copy the calling thread's hashtablestack structure so the new
     * thread gets its own stack pointer/index state (push/pop won't alias
     * the parent's stack).
     *
     * NOTE: This is a shallow copy of the tytablestack structure — both
     * threads share pointers to the same underlying hash tables. This is
     * safe under GIL serialization (only one thread accesses at a time),
     * but would be a data race if the GIL is ever removed.
     * TODO(Phase4): When global state elimination removes the GIL, each
     * thread will need deep-copied or thread-local hash table chains. */
    {
        Handle hcopy;

        if (!newfilledhandle((char *)(*hashtablestack), sizeof(tytablestack), &hcopy)) {
            langdisposetree(hcode);
            headless_dispose_threadglobals(new_hglobals);
            free_thread_record(rec);
            return false;
        }

        (**new_hglobals).htablestack = (hdltablestack)hcopy;
    }
    (**new_hglobals).hcurrenthashtable = currenthashtable;

    /* Register in system.compiler.threads (calling thread context) */
    copystring(BIGSTRING("\011anonymous"), bsanon);
    headless_register_thread(bsanon, threadid);

    /* Package launch parameters */
    params = (thread_launch_params *)malloc(sizeof(thread_launch_params));

    if (params == NULL) {
        langdisposetree(hcode);
        headless_unregister_thread(threadid);
        headless_dispose_threadglobals(new_hglobals);
        free_thread_record(rec);
        return false;
    }

    params->hcode = hcode;
    params->hglobals = new_hglobals;
    params->rec = rec;
    params->is_callscript = false;

    /* Spawn detached POSIX thread — it will block on GIL until we yield */
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    if (pthread_create(&tid, &attr, thread_entry_point, params) != 0) {
        log_error(LOG_COMP_THREAD, "pthread_create failed for thread.evaluate");
        pthread_attr_destroy(&attr);
        /* Full cleanup: unregister from system.compiler.threads, dispose
         * thread globals (frees deep-copied hashtablestack and error stack),
         * release the registry record (decrements refcount to zero, removing
         * it from the registry). params->hcode is not freed here because
         * langdisposetree handles it directly. */
        langdisposetree(hcode);
        headless_unregister_thread(threadid);
        headless_dispose_threadglobals(new_hglobals);
        free_thread_record(rec);
        free(params);
        return false;
    }

    pthread_attr_destroy(&attr);
    rec->pthread_id = tid;  /* Set AFTER successful create — not read until thread runs */

    /* Return thread ID to caller — thread is spawned but blocked on GIL */
    return setlongvalue(threadid, vreturned);
}

/*
 * headless_thread_callscript - Spawn a real POSIX thread for thread.callscript
 *
 * Phase 1 (resolve script) runs in the calling thread context so resolution
 * failures propagate to the caller. Phase 2 (execute) is handed off to a
 * new POSIX thread.
 *
 * Return semantics:
 * - Resolution failure → returns false (error propagates to caller)
 * - Successful spawn → returns true with thread ID. Runtime errors are
 *   fire-and-forget.
 */
static boolean headless_thread_callscript(bigstring bsscriptname, tyvaluerecord vparams,
                                          hdlhashtable hcontext, tyvaluerecord *vreturned) {
    frontier_pthread_record *rec = nil;
    hdlthreadglobals new_hglobals = nil;
    boolean fl;
    long threadid;
    thread_launch_params *params = nil;
    pthread_t tid;
    pthread_attr_t attr;

    /* --- Phase 1: Resolve script in calling thread context --- */
    /* Failures here (bad name, not a script, compile error) propagate to caller */

    bigstring bsverb;
    hdltreenode hcode;
    hdlhashtable htable;
    tyvaluerecord vhandler;
    hdlhashnode handlernode;

    /* Resolve script path to table + verb name (matches langrunscript lines 1794-1807) */
    pushhashtable(roottable);

    fl = langexpandtodotparams(bsscriptname, &htable, bsverb);

    if (fl) {
        if (htable == nil)
            langsearchpathlookup(bsverb, &htable);
    }

    pophashtable();

    if (!fl)
        return false;

    /* Look up the handler node (matches langrunscript lines 1809-1813) */
    if (!hashtablelookupnode(htable, bsverb, &handlernode)) {
        langparamerror(unknownfunctionerror, bsverb);
        return false;
    }

    vhandler = (**handlernode).val;

    /* Get or compile the code tree (matches langrunscript lines 1822-1846) */
    hcode = nil;

    if (vhandler.valuetype == codevaluetype) {
        hcode = vhandler.data.codevalue;
    }
    else if ((**htable).valueroutine == nil) { /* not a kernel table */
        if (!langexternalvaltocode(vhandler, &hcode)) {
            langparamerror(notfunctionerror, bsverb);
            return false;
        }

        if (hcode == nil) { /* needs compilation */
            if (!langcompilescript(handlernode, &hcode))
                return false;
        }
    }

    /* --- Phase 2: Spawn POSIX thread for execution --- */
    /* Runtime errors from here on are fire-and-forget */

    rec = allocate_thread_record();

    if (rec == NULL)
        return false;

    threadid = rec->user_thread_id;

    new_hglobals = headless_new_threadglobals();

    if (new_hglobals == nil) {
        free_thread_record(rec);
        return false;
    }

    (**new_hglobals).idthread = (hdlthread) threadid;
    rec->hglobals = new_hglobals;

    /* Deep-copy the calling thread's hashtablestack structure. See comment
     * in headless_thread_evaluate for shallow-copy semantics and Phase 4 TODO. */
    {
        Handle hcopy;

        if (!newfilledhandle((char *)(*hashtablestack), sizeof(tytablestack), &hcopy)) {
            headless_dispose_threadglobals(new_hglobals);
            free_thread_record(rec);
            return false;
        }

        (**new_hglobals).htablestack = (hdltablestack)hcopy;
    }
    (**new_hglobals).hcurrenthashtable = currenthashtable;

    /* Register in system.compiler.threads (calling thread context) */
    headless_register_thread(bsverb, threadid);

    /* Package launch parameters */
    params = (thread_launch_params *)malloc(sizeof(thread_launch_params));

    if (params == NULL) {
        headless_unregister_thread(threadid);
        headless_dispose_threadglobals(new_hglobals);
        free_thread_record(rec);
        return false;
    }

    params->hcode = hcode;
    params->hglobals = new_hglobals;
    params->rec = rec;

    /* Store resolved script references. These are pointers into the ODB —
     * htable and hcode are owned by the hash table, not by us. This is safe
     * under GIL serialization: the caller cannot mutate or delete the script
     * while we hold the GIL (which we do until pthread_create returns and
     * the caller yields). The spawned thread acquires the GIL before accessing
     * these pointers, so no stale-pointer race is possible.
     * TODO(Phase4): If GIL is removed, callscript must retain/copy these
     * references or re-resolve the script inside the spawned thread. */
    params->htable = htable;
    copystring(bsverb, params->bsverb);
    params->hcontext = hcontext;
    params->is_callscript = true;

    /* Deep-copy vparams so the spawned thread owns its own list Handle.
     * A struct copy would alias the calling thread's Handle data, which
     * becomes invalid after the calling thread's stack frame is unwound
     * or the tmp stack reclaims it. */
    if (!copyvaluerecord(vparams, &params->vparams)) {
        headless_unregister_thread(threadid);
        headless_dispose_threadglobals(new_hglobals);
        free_thread_record(rec);
        free(params);
        return false;
    }

    /* Exempt the deep-copied vparams from the calling thread's tmp stack.
     * Without this, the calling thread's evaluator will dispose the Handle
     * when it cleans up its tmp stack, invalidating the spawned thread's copy. */
    exemptfromtmpstack(&params->vparams);

    /* Spawn detached POSIX thread */
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    if (pthread_create(&tid, &attr, thread_entry_point, params) != 0) {
        log_error(LOG_COMP_THREAD, "pthread_create failed for thread.callscript");
        pthread_attr_destroy(&attr);
        /* Full cleanup: unregister, dispose globals (frees deep-copied
         * hashtablestack and error stack), dispose deep-copied vparams,
         * release registry record (refcount → 0, removed from registry). */
        disposevaluerecord(params->vparams, false);
        headless_unregister_thread(threadid);
        headless_dispose_threadglobals(new_hglobals);
        free_thread_record(rec);
        free(params);
        return false;
    }

    pthread_attr_destroy(&attr);
    rec->pthread_id = tid;  /* Set AFTER successful create */

    /* Return thread ID — thread is spawned but blocked on GIL */
    return setlongvalue(threadid, vreturned);
}

/*
 * headless_backgroundtask - GIL yield point for langbackgroundtask callback
 *
 * Called at loop boundaries and I/O points by the interpreter. Saves the
 * current thread's globals, releases the GIL (allowing other threads to run),
 * yields the CPU, then reacquires the GIL and restores globals.
 *
 * Returns false if the thread has been killed (terminates the script).
 */
boolean headless_backgroundtask(boolean flresting) {
#pragma unused(flresting)
    hdlthreadglobals my_globals_handle = hthreadglobals;

    headless_save_threadglobals(my_globals_handle);

    /* Release the GIL and let other threads run */
    pthread_mutex_unlock(&frontier_gil);
    pthread_cond_broadcast(&gil_available);
    sched_yield();

    /* Reacquire the GIL */
    pthread_mutex_lock(&frontier_gil);

    /* Restore our globals (another thread may have changed C globals) */
    headless_restore_threadglobals(my_globals_handle);

    /* Check if we've been killed while yielded */
    if ((**my_globals_handle).flthreadkilled)
        return false;

    return true;
}

/*
 * headless_thread_sleep - GIL-aware sleep with condvar
 *
 * Sleeps for the specified number of ticks (60 ticks/sec).
 * Releases the GIL during sleep so other threads can run.
 * Can be interrupted by thread.wake() or thread.kill().
 */
static boolean headless_thread_sleep(long ticks) {
    long idthread = (long)(**hthreadglobals).idthread;
    frontier_pthread_record *rec = get_thread_by_id(idthread);
    hdlthreadglobals my_globals_handle = hthreadglobals;

    if (rec == NULL)
        return false;

    /* Save globals before releasing the GIL */
    headless_save_threadglobals(my_globals_handle);

    /* Mark as sleeping and compute wake time */
    pthread_mutex_lock(&rec->state_mutex);
    rec->is_sleeping = true;

    /* Convert ticks to timespec (60 ticks/sec) */
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    long seconds = ticks / TICKS_PER_SECOND;
    long remaining_ticks = ticks % TICKS_PER_SECOND;
    ts.tv_sec += seconds;
    ts.tv_nsec += (remaining_ticks * 1000000000L) / TICKS_PER_SECOND;
    if (ts.tv_nsec >= 1000000000L) {
        ts.tv_sec += 1;
        ts.tv_nsec -= 1000000000L;
    }

    /* Release the GIL so other threads can run while we sleep */
    pthread_mutex_unlock(&frontier_gil);
    pthread_cond_broadcast(&gil_available);

    /* Wait until timeout or wake signal.
     * This blocks only this OS thread — others can acquire the GIL.
     * Returns 0 (signaled), ETIMEDOUT (normal expiry), or error. */
    {
        int wait_rc = pthread_cond_timedwait(&rec->wake_cond, &rec->state_mutex, &ts);

        if (wait_rc != 0 && wait_rc != ETIMEDOUT)
            log_error(LOG_COMP_THREAD, "pthread_cond_timedwait failed: %d", wait_rc);
    }

    rec->is_sleeping = false;
    boolean was_killed = rec->is_killed;
    pthread_mutex_unlock(&rec->state_mutex);

    /* Reacquire the GIL before touching C globals */
    pthread_mutex_lock(&frontier_gil);

    /* Restore our globals */
    headless_restore_threadglobals(my_globals_handle);

    release_thread_record(rec);

    /* If killed, set the thread-killed flag so langruncode checks it */
    if (was_killed)
        (**hthreadglobals).flthreadkilled = true;

    return true;
}

static boolean thread_valueproc(short token, hdltreenode hparam1,
                                     tyvaluerecord *vreturned,
                                     bigstring bserror) {
    switch(token) {
        case thrv_exists: {
            /* Verb #0: thread.exists - Check if thread exists via registry */
            long id;

            if (!langcheckparamcount(hparam1, 1))
                return false;

            flnextparamislast = true;
            if (!getlongvalue(hparam1, 1, &id))
                return false;

            {
                frontier_pthread_record *rec = get_thread_by_id(id);
                boolean exists = (rec != NULL);

                if (rec != NULL)
                    release_thread_record(rec);

                return setbooleanvalue(exists, vreturned);
            }
        }
        case thrv_evaluate: {
            /* Verb #1: thread.evaluate - Spawn thread via GIL model */
            bigstring bscode;

            if (!langcheckparamcount(hparam1, 1))
                return false;

            flnextparamislast = true;
            if (!getstringvalue(hparam1, 1, bscode))
                return false;

            return headless_thread_evaluate(bscode, vreturned);
        }
        case thrv_callscript: {
            /* Verb #2: thread.callscript - Spawn thread via GIL model */
            bigstring bsscriptname;
            tyvaluerecord vparams;
            hdlhashtable hcontext = nil;

            /* Get script name (required) */
            if (!getstringvalue(hparam1, 1, bsscriptname))
                return false;

            /* Get parameters (required) */
            if (!getparamvalue(hparam1, 2, &vparams))
                return false;

            /* Get optional context table (3rd param) */
            if (langgetparamcount(hparam1) > 2) {
                flnextparamislast = true;

                if (!gettablevalue(hparam1, 3, &hcontext))
                    return false;
            } else {
                flnextparamislast = true;
            }

            return headless_thread_callscript(bsscriptname, vparams, hcontext, vreturned);
        }
        case thrv_getcurrentid: {
            /* Verb #3: thread.getcurrentid - Get current thread ID from globals */

            if (!langcheckparamcount(hparam1, 0))
                return false;

            return setlongvalue((long)(**hthreadglobals).idthread, vreturned);
        }
        case thrv_getcount: {
            /* Verb #4: thread.getcount - Get count from registry */
            if (!langcheckparamcount(hparam1, 0))
                return false;

            return setlongvalue(get_thread_count(), vreturned);
        }
        case thrv_getnthid: {
            /* Verb #5: thread.getnthid - Get Nth thread ID from registry */
            short n;

            if (!langcheckparamcount(hparam1, 1))
                return false;

            flnextparamislast = true;
            if (!getintvalue(hparam1, 1, &n))
                return false;

            return setlongvalue(get_nth_thread_id((long)n), vreturned);
        }
        case thrv_sleep:
            /* Verb #6: thread.sleep - not yet implemented */
            log_warn(LOG_COMP_LANG, "thread.sleep not yet implemented");
            if (bserror) copystring(BIGSTRING("\017not implemented"), bserror);
            return false;
        case thrv_sleepfor: {
            /* Verb #7: thread.sleepfor - Sleep for N seconds via registry */
            long seconds;

            if (!langcheckparamcount(hparam1, 1))
                return false;

            flnextparamislast = true;
            if (!getlongvalue(hparam1, 1, &seconds))
                return false;

            return setbooleanvalue(headless_thread_sleep(seconds * TICKS_PER_SECOND), vreturned);
        }
        case thrv_sleepticks: {
            /* Verb #8: thread.sleepticks - Sleep for N ticks via registry */
            long ticks;

            if (!langcheckparamcount(hparam1, 1))
                return false;

            flnextparamislast = true;
            if (!getlongvalue(hparam1, 1, &ticks))
                return false;

            return setbooleanvalue(headless_thread_sleep(ticks), vreturned);
        }
        case thrv_issleeping: {
            /* Verb #9: thread.issleeping - Check via registry state */
            long id;
            frontier_pthread_record *rec;

            if (!langcheckparamcount(hparam1, 1))
                return false;

            flnextparamislast = true;
            if (!getlongvalue(hparam1, 1, &id))
                return false;

            rec = get_thread_by_id(id);
            if (rec == NULL)
                return false;

            {
                boolean sleeping;

                pthread_mutex_lock(&rec->state_mutex);
                sleeping = rec->is_sleeping;
                pthread_mutex_unlock(&rec->state_mutex);

                release_thread_record(rec);
                return setbooleanvalue(sleeping, vreturned);
            }
        }
        case thrv_wake: {
            /* Verb #10: thread.wake - Signal wake condvar in registry */
            long id;
            frontier_pthread_record *rec;

            if (!langcheckparamcount(hparam1, 1))
                return false;

            flnextparamislast = true;
            if (!getlongvalue(hparam1, 1, &id))
                return false;

            rec = get_thread_by_id(id);
            if (rec == NULL)
                return false;

            pthread_mutex_lock(&rec->state_mutex);
            pthread_cond_signal(&rec->wake_cond);
            pthread_mutex_unlock(&rec->state_mutex);

            release_thread_record(rec);
            return setbooleanvalue(true, vreturned);
        }
        case thrv_kill: {
            /* Verb #11: thread.kill - Set is_killed in registry + flthreadkilled
             *
             * With the GIL model, kill works on running threads too — the target
             * checks flthreadkilled at every langbackgroundtask() yield point.
             * If the target is sleeping, cond_signal wakes it immediately.
             * If the target is blocked on GIL acquisition, it checks the flag
             * after acquiring. If the target holds the GIL (running), the kill
             * request is deferred until the target's next yield point. */
            long id;
            frontier_pthread_record *rec;

            if (!langcheckparamcount(hparam1, 1))
                return false;

            flnextparamislast = true;
            if (!getlongvalue(hparam1, 1, &id))
                return false;

            rec = get_thread_by_id(id);
            if (rec == NULL)
                return false;

            pthread_mutex_lock(&rec->state_mutex);
            rec->is_killed = true;
            pthread_cond_signal(&rec->wake_cond);  /* Wake if sleeping */
            pthread_mutex_unlock(&rec->state_mutex);

            /* Also set flthreadkilled in the thread's globals so langruncode stops */
            if (rec->hglobals != nil)
                (**rec->hglobals).flthreadkilled = true;

            release_thread_record(rec);
            return setbooleanvalue(true, vreturned);
        }
        case thrv_gettimeslice:
            /* Verb #12: thread.gettimeslice - not yet implemented */
            log_warn(LOG_COMP_LANG, "thread.gettimeslice not yet implemented");
            if (bserror) copystring(BIGSTRING("\017not implemented"), bserror);
            return false;
        case thrv_settimeslice:
            /* Verb #13: thread.settimeslice - not yet implemented */
            log_warn(LOG_COMP_LANG, "thread.settimeslice not yet implemented");
            if (bserror) copystring(BIGSTRING("\017not implemented"), bserror);
            return false;
        case thrv_getdefaulttimeslice:
            /* Verb #14: thread.getdefaulttimeslice - not yet implemented */
            log_warn(LOG_COMP_LANG, "thread.getdefaulttimeslice not yet implemented");
            if (bserror) copystring(BIGSTRING("\017not implemented"), bserror);
            return false;
        case thrv_setdefaulttimeslice:
            /* Verb #15: thread.setdefaulttimeslice - not yet implemented */
            log_warn(LOG_COMP_LANG, "thread.setdefaulttimeslice not yet implemented");
            if (bserror) copystring(BIGSTRING("\017not implemented"), bserror);
            return false;
        case thrv_getstats:
            /* Verb #16: thread.getstats - not yet implemented */
            log_warn(LOG_COMP_LANG, "thread.getstats not yet implemented");
            if (bserror) copystring(BIGSTRING("\017not implemented"), bserror);
            return false;
        default:
            return false;
    }
}

boolean threadinitverbs(void) {
    hdlhashtable htable = nil;
    bigstring bsname;

    copystring(BIGSTRING("\006thread"), bsname);

    if (!newfunctionprocessor(bsname, &thread_valueproc, false, &htable))
        return false;

    pushhashtable(htable);

    #define ADD_VERB(name, tok) do { \
        bigstring bs; \
        copystring(name, bs); \
        if (!langaddkeyword(bs, tok)) { \
            pophashtable(); \
            return false; \
        } \
    } while(0)

    ADD_VERB(BIGSTRING("\006exists"), thrv_exists);
    ADD_VERB(BIGSTRING("\010evaluate"), thrv_evaluate);
    ADD_VERB(BIGSTRING("\012callscript"), thrv_callscript);
    ADD_VERB(BIGSTRING("\014getcurrentid"), thrv_getcurrentid);
    ADD_VERB(BIGSTRING("\010getcount"), thrv_getcount);
    ADD_VERB(BIGSTRING("\010getnthid"), thrv_getnthid);
    ADD_VERB(BIGSTRING("\005sleep"), thrv_sleep);
    ADD_VERB(BIGSTRING("\010sleepfor"), thrv_sleepfor);
    ADD_VERB(BIGSTRING("\012sleepticks"), thrv_sleepticks);
    ADD_VERB(BIGSTRING("\012issleeping"), thrv_issleeping);
    ADD_VERB(BIGSTRING("\004wake"), thrv_wake);
    ADD_VERB(BIGSTRING("\004kill"), thrv_kill);
    ADD_VERB(BIGSTRING("\014gettimeslice"), thrv_gettimeslice);
    ADD_VERB(BIGSTRING("\014settimeslice"), thrv_settimeslice);
    ADD_VERB(BIGSTRING("\023getdefaulttimeslice"), thrv_getdefaulttimeslice);
    ADD_VERB(BIGSTRING("\023setdefaulttimeslice"), thrv_setdefaulttimeslice);
    ADD_VERB(BIGSTRING("\010getstats"), thrv_getstats);

    #undef ADD_VERB

    pophashtable();
    return true;
}
