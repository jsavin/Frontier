/*
 * headless_thread_verbs.c - Thread processor verb implementations
 *
 * Implements cooperative threading for headless mode using the thread registry
 * and thread globals save/restore pattern from legacy Frontier (process.c).
 *
 * Cooperative model: Only one thread runs at a time. thread.evaluate() and
 * thread.callscript() run synchronously — they save main thread globals,
 * swap in new thread globals, execute the code, then restore main globals.
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
#include "tablestructure.h"
#include "tableverbs.h"
#include "process.h"
#include "shellthreads.h"
#include "logging.h"
#include "threadregistry.h"
#include "processinternal.h"
#include "script_portable.h"

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
 * headless_thread_evaluate - Cooperative thread.evaluate implementation
 *
 * Runs code in a new cooperative thread context:
 * 1. Compile code string
 * 2. Allocate thread record from registry
 * 3. Allocate new thread globals
 * 4. Register in system.compiler.threads
 * 5. Save main globals, swap in new globals
 * 6. Execute code synchronously
 * 7. Restore main globals
 * 8. Cleanup
 *
 * Returns thread ID to caller.
 */
static boolean headless_thread_evaluate(bigstring bscode, tyvaluerecord *vreturned) {
    hdltreenode hcode = nil;
    frontier_pthread_record *rec = nil;
    hdlthreadglobals new_hglobals = nil;
    hdlthreadglobals main_hglobals = hthreadglobals;
    tyvaluerecord result;
    boolean fl;
    long threadid;
    Handle htext = nil;
    long codelen;
    bigstring bsanon;

    /* Compile the code string into a tree */
    codelen = stringlength(bscode);

    if (!newfilledhandle(stringbaseaddress(bscode), codelen, &htext))
        return false;

    fl = langbuildtree(htext, true, &hcode); /* langbuildtree disposes htext */

    if (!fl || hcode == nil)
        return false;

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

    /* TODO: Register in system.compiler.threads — deferred due to hash table
     * context issues when called during cooperative thread setup. The table
     * registration needs proper push/pop context that doesn't interfere with
     * the globals swap. Will be addressed in a follow-up. */

    /* Save main thread globals */
    headless_save_threadglobals(main_hglobals);

    /* Copy the main thread's hashtablestack for the new thread */
    (**new_hglobals).htablestack = hashtablestack;

    /* Swap in new thread globals */
    headless_restore_threadglobals(new_hglobals);

    /* Execute code synchronously */
    initvalue(&result, novaluetype);

    fl = langruncode(hcode, nil, &result);

    /* Save new thread globals (captures thread's error state into its struct) */
    headless_save_threadglobals(new_hglobals);

    /* Restore main thread globals — this restores fllangerror, flreturn, etc.
     * from the main thread's saved state, ensuring spawned thread errors
     * don't contaminate the originating thread. Fire-and-forget semantics. */
    headless_restore_threadglobals(main_hglobals);

    /* Clear the global error buffer so spawned thread errors don't leak
     * through g_headless_error (which is process-global, not per-thread) */
    headless_clear_last_lang_error();

    /* Unregister from system.compiler.threads (deferred — debug) */
    /* headless_unregister_thread(threadid); */

    /* Cleanup */
    langdisposetree(hcode);
    disposevaluerecord(result, false);
    headless_dispose_threadglobals(new_hglobals);
    free_thread_record(rec);

    /* Return thread ID to caller */
    return setlongvalue(threadid, vreturned);
}

/*
 * headless_thread_callscript - Cooperative thread.callscript implementation
 *
 * Same cooperative pattern as evaluate, but uses langrunscript() to resolve
 * and execute a named script with parameters.
 */
static boolean headless_thread_callscript(bigstring bsscriptname, tyvaluerecord vparams,
                                          hdlhashtable hcontext, tyvaluerecord *vreturned) {
    frontier_pthread_record *rec = nil;
    hdlthreadglobals new_hglobals = nil;
    hdlthreadglobals main_hglobals = hthreadglobals;
    tyvaluerecord result;
    boolean fl;
    long threadid;

    /* Allocate thread record from registry */
    rec = allocate_thread_record();

    if (rec == NULL)
        return false;

    threadid = rec->user_thread_id;

    /* Allocate new thread globals */
    new_hglobals = headless_new_threadglobals();

    if (new_hglobals == nil) {
        free_thread_record(rec);
        return false;
    }

    /* Set thread ID in the new globals */
    (**new_hglobals).idthread = (hdlthread) threadid;

    /* Link globals to registry record */
    rec->hglobals = new_hglobals;

    /* TODO: Register in system.compiler.threads — deferred (see evaluate) */

    /* Save main thread globals */
    headless_save_threadglobals(main_hglobals);

    /* Copy the main thread's hashtablestack for the new thread */
    (**new_hglobals).htablestack = hashtablestack;

    /* Swap in new thread globals */
    headless_restore_threadglobals(new_hglobals);

    /* Execute script synchronously via langrunscript */
    initvalue(&result, novaluetype);

    fl = langrunscript(bsscriptname, &vparams, hcontext, &result);

    /* Save new thread globals */
    headless_save_threadglobals(new_hglobals);

    /* Restore main thread globals */
    headless_restore_threadglobals(main_hglobals);

    /* Clear global error buffer (fire-and-forget) */
    headless_clear_last_lang_error();

    /* Unregister from system.compiler.threads (deferred — debug) */
    /* headless_unregister_thread(threadid); */

    /* Cleanup */
    disposevaluerecord(result, false);
    headless_dispose_threadglobals(new_hglobals);
    free_thread_record(rec);

    /* Propagate script resolution/execution failures to the caller.
     * If langrunscript failed (bad script name, parameter binding, etc.),
     * return false so thread.callscript reports the error. */
    if (!fl)
        return false;

    /* Return thread ID to caller */
    return setlongvalue(threadid, vreturned);
}

/*
 * headless_thread_sleep - Registry-based sleep with condvar
 *
 * Sleeps for the specified number of ticks (60 ticks/sec).
 * Can be interrupted by thread.wake() or thread.kill().
 */
static boolean headless_thread_sleep(long ticks) {
    long idthread = (long)(**hthreadglobals).idthread;
    frontier_pthread_record *rec = get_thread_by_id(idthread);

    if (rec == NULL)
        return false;

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

    /* Wait until timeout or wake signal */
    pthread_cond_timedwait(&rec->wake_cond, &rec->state_mutex, &ts);

    rec->is_sleeping = false;
    boolean was_killed = rec->is_killed;
    pthread_mutex_unlock(&rec->state_mutex);

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
            /* Verb #1: thread.evaluate - Evaluate code in cooperative thread */
            bigstring bscode;

            if (!langcheckparamcount(hparam1, 1))
                return false;

            flnextparamislast = true;
            if (!getstringvalue(hparam1, 1, bscode))
                return false;

            return headless_thread_evaluate(bscode, vreturned);
        }
        case thrv_callscript: {
            /* Verb #2: thread.callscript - Call script in cooperative thread */
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
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
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
            /* Verb #11: thread.kill - Set is_killed in registry + flthreadkilled */
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
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case thrv_settimeslice:
            /* Verb #13: thread.settimeslice - not yet implemented */
            log_warn(LOG_COMP_LANG, "thread.settimeslice not yet implemented");
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case thrv_getdefaulttimeslice:
            /* Verb #14: thread.getdefaulttimeslice - not yet implemented */
            log_warn(LOG_COMP_LANG, "thread.getdefaulttimeslice not yet implemented");
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case thrv_setdefaulttimeslice:
            /* Verb #15: thread.setdefaulttimeslice - not yet implemented */
            log_warn(LOG_COMP_LANG, "thread.setdefaulttimeslice not yet implemented");
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        case thrv_getstats:
            /* Verb #16: thread.getstats - not yet implemented */
            log_warn(LOG_COMP_LANG, "thread.getstats not yet implemented");
            if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
            return false;
        default:
            return false;
    }
}

boolean threadinitverbs(void) {
    hdlhashtable htable = nil;
    bigstring bsname;

    copystring(BIGSTRING("\pthread"), bsname);

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

    ADD_VERB(BIGSTRING("\pexists"), thrv_exists);
    ADD_VERB(BIGSTRING("\pevaluate"), thrv_evaluate);
    ADD_VERB(BIGSTRING("\pcallscript"), thrv_callscript);
    ADD_VERB(BIGSTRING("\pgetcurrentid"), thrv_getcurrentid);
    ADD_VERB(BIGSTRING("\pgetcount"), thrv_getcount);
    ADD_VERB(BIGSTRING("\pgetnthid"), thrv_getnthid);
    ADD_VERB(BIGSTRING("\psleep"), thrv_sleep);
    ADD_VERB(BIGSTRING("\psleepfor"), thrv_sleepfor);
    ADD_VERB(BIGSTRING("\psleepticks"), thrv_sleepticks);
    ADD_VERB(BIGSTRING("\pissleeping"), thrv_issleeping);
    ADD_VERB(BIGSTRING("\pwake"), thrv_wake);
    ADD_VERB(BIGSTRING("\pkill"), thrv_kill);
    ADD_VERB(BIGSTRING("\pgettimeslice"), thrv_gettimeslice);
    ADD_VERB(BIGSTRING("\psettimeslice"), thrv_settimeslice);
    ADD_VERB(BIGSTRING("\pgetdefaulttimeslice"), thrv_getdefaulttimeslice);
    ADD_VERB(BIGSTRING("\psetdefaulttimeslice"), thrv_setdefaulttimeslice);
    ADD_VERB(BIGSTRING("\pgetstats"), thrv_getstats);

    #undef ADD_VERB

    pophashtable();
    return true;
}
