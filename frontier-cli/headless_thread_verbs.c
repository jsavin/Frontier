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
 *	 on GIL acquisition, then return the thread ID immediately.
 * - langbackgroundtask() (called at loop boundaries) releases the GIL, yields,
 *	 then reacquires — allowing spawned threads to run.
 * - thread.sleepTicks() releases the GIL, sleeps on a condvar (only blocking
 *	 the OS thread), then reacquires the GIL.
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
#include "tcpverbs.h"  /* tcp_process_callbacks */
#include "headless_spawn.h"  /* headless_spawn_script_thread, thread_run_spec */
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
pthread_mutex_t frontier_gil = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t gil_available = PTHREAD_COND_INITIALIZER;

/*
 * Yield synchronization for headless_backgroundtask().
 *
 * When the main thread yields the GIL via backgroundtask, it needs to
 * actually block long enough for waiting threads (e.g., TCP callback
 * threads) to acquire the GIL and run. A bare sched_yield() is not
 * sufficient — it's a hint to the OS scheduler and often returns before
 * the waiting thread gets scheduled, causing the main thread to immediately
 * reacquire the GIL (starvation).
 *
 * Solution: the yielding thread sleeps on headless_yield_cond for a short timeout
 * (1ms) WITHOUT holding the GIL. This guarantees the waiting thread gets
 * a chance to run. The callback thread signals headless_yield_cond when it finishes,
 * waking the yielder early if no more work is pending.
 */
/* 2026-06-06 JES #691: Exported (was static) so headless_spawn.c can signal
 * headless_yield_cond from unified_thread_entry. Declaration in headless_threading.h. */
pthread_mutex_t headless_yield_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t headless_yield_cond = PTHREAD_COND_INITIALIZER;

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

/* Forward declaration used by callback_thread_entry_point */
boolean headless_unregister_thread(long idthread);

/*
 * Thread launch parameters - passed from spawning thread to new POSIX thread.
 * Used by callback_thread_entry_point.
 *
 * 2026-06-06 JES #691: thread_entry_point (which also used this struct for the
 * callScript and evaluate paths) has been removed. Those paths now use
 * headless_spawn_script_thread (headless_spawn.c) with thread_run_spec.
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
 * callback_thread_entry_point - POSIX thread entry for TCP callback threads
 *
 * Similar to thread_entry_point but tailored for fire-and-forget callbacks:
 * - Performs error cleanup (fifcloseallfiles, langreleasesemaphores) on failure,
 *	 matching the one-shot process cleanup in process.c:2565-2576
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
	log_debug(LOG_COMP_THREAD, "callback_thread_entry_point: waiting for GIL");
	pthread_mutex_lock(&frontier_gil);
	log_debug(LOG_COMP_THREAD, "callback_thread_entry_point: acquired GIL, executing callback");

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

	/* Wake the yielding thread (headless_backgroundtask) so it can
	 * reacquire the GIL promptly instead of waiting for the full
	 * yield timeout to expire. */
	pthread_mutex_lock(&headless_yield_mutex);
	pthread_cond_signal(&headless_yield_cond);
	pthread_mutex_unlock(&headless_yield_mutex);

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

	/* Allocate a fresh, empty hashtablestack for the callback thread.
	 * Same fix as headless_thread_evaluate/callscript: copying the caller's
	 * toptables depth causes stack overflow in the callback script. TCP
	 * callbacks are spawned from headless_backgroundtask(), which can be
	 * called from arbitrary interpreter depth. Starting with toptables = 0
	 * and roottable as the base gives the callback a clean execution context. */
	{
		Handle hcopy;

		/* newclearhandle zeroes all memory: toptables = 0 and stack[] = nil. */
		if (!newclearhandle(sizeof(tytablestack), &hcopy)) {
			langdisposetree(hcode);
			headless_dispose_threadglobals(new_hglobals);
			free_thread_record(rec);
			return false;
		}

		(**new_hglobals).htablestack = (hdltablestack)hcopy;
	}
	(**new_hglobals).hcurrenthashtable = roottable;

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
boolean headless_register_thread(bigstring bsname, long idthread) {
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
boolean headless_unregister_thread(long idthread) {
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
 *	 spawned thread are fire-and-forget.
 */
static boolean headless_thread_evaluate(bigstring bscode, tyvaluerecord *vreturned) {
	hdltreenode hcode = nil;
	boolean fl;
	Handle htext = nil;
	long codelen;
	bigstring bsanon;

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

	/* 2026-06-06 JES #691: Replaced open-coded alloc/wire/pthread_create with
	 * headless_spawn_script_thread. The evaluate path uses is_callscript=false
	 * (langruncode, thread owns hcode). Registration name is "anonymous". */

	/* Build run spec: langruncode path (evaluate compiles hcode, thread owns it) */
	thread_run_spec eval_run_spec;
	memset(&eval_run_spec, 0, sizeof(eval_run_spec));
	eval_run_spec.is_callscript = false;

	/* Register with name "anonymous" before spawning, matching original behavior.
	 * headless_spawn_script_thread registers for is_callscript=true paths, but
	 * skips registration for is_callscript=false (debug path registers as "debug"
	 * in the thread entry). For evaluate, we want "anonymous" registered here
	 * before pthread_create, so we do it manually and then let the spawn skip it.
	 *
	 * NOTE: headless_spawn_script_thread calls headless_register_thread only for
	 * is_callscript=true. For is_callscript=false (this path), the thread entry
	 * (unified_thread_entry) registers as "debug" -- but only when debug_opts is
	 * non-NULL. Since we pass NULL for debug_opts here, NO registration happens
	 * inside the primitive. We handle it here to preserve the original behavior.
	 */
	copystring(PSTRING("\011", "anonymous"), bsanon);

	long eval_threadid = 0;

	if (!headless_spawn_script_thread(hcode, &eval_run_spec, NULL, &eval_threadid)) {
		log_error(LOG_COMP_THREAD, "headless_spawn_script_thread failed for thread.evaluate");
		/* hcode freed inside the primitive (is_callscript=false, thread owns it) */
		return false;
	}

	/* Register in system.compiler.threads AFTER spawn so we know the real threadid.
	 * Safe because we still hold the GIL across this call -- the spawned thread
	 * cannot have begun executing (it blocks on pthread_mutex_lock(&frontier_gil)
	 * as its first action). Registration order vs the spawned thread is unchanged
	 * from the pre-refactor behavior: the original code registered before
	 * pthread_create, which is equally safe since the new thread was blocked on
	 * GIL acquisition. */
	headless_register_thread(bsanon, eval_threadid);

	/* Return thread ID to caller -- thread is spawned but blocked on GIL */
	return setlongvalue(eval_threadid, vreturned);
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
 *	 fire-and-forget.
 */
static boolean headless_thread_callscript(bigstring bsscriptname, tyvaluerecord vparams,
										  hdlhashtable hcontext, tyvaluerecord *vreturned) {
	boolean fl;

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

	/* --- Phase 2: Spawn POSIX thread for execution ---
	 * 2026-06-06 JES #691: Replaced open-coded alloc/wire/pthread_create
	 * with headless_spawn_script_thread.  Runtime errors are fire-and-forget. */

	/* Deep-copy vparams so the spawned thread owns its own list Handle.
	 * A struct copy would alias the calling thread's Handle data, which
	 * becomes invalid after the calling thread's stack frame is unwound
	 * or the tmp stack reclaims it.
	 * TODO(Phase4): If GIL is removed, callscript must retain/copy these
	 * references or re-resolve the script inside the spawned thread. */
	tyvaluerecord vparams_copy;
	if (!copyvaluerecord(vparams, &vparams_copy))
		return false;

	/* Exempt the deep-copied vparams from the calling thread's tmp stack.
	 * Without this, the calling thread's evaluator will dispose the Handle
	 * when it cleans up its tmp stack, invalidating the spawned thread's copy. */
	exemptfromtmpstack(&vparams_copy);

	thread_run_spec run_spec;
	memset(&run_spec, 0, sizeof(run_spec));
	run_spec.is_callscript = true;
	copystring(bsverb, run_spec.bsverb);
	run_spec.htable = htable;
	run_spec.hcontext = hcontext;
	run_spec.vparams = vparams_copy;
	/* hcode: ODB-owned pointer, safe under GIL (see original comment above) */

	long user_threadid = 0;

	if (!headless_spawn_script_thread(hcode, &run_spec, NULL, &user_threadid)) {
		log_error(LOG_COMP_THREAD, "headless_spawn_script_thread failed for thread.callscript");
		/* On spawn failure for is_callscript, vparams_copy is NOT disposed inside
		 * the primitive (hcode and vparams are caller-owned for this path).
		 * Dispose vparams_copy here to avoid leaking the deep-copied Handle. */
		disposevaluerecord(vparams_copy, false);
		return false;
	}

	/* Return thread ID -- thread is spawned but blocked on GIL */
	return setlongvalue(user_threadid, vreturned);
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

	/* Process any pending TCP callbacks BEFORE yielding the GIL.
	 * The TCP accept thread enqueues callbacks into a queue, but they
	 * must be dequeued and spawned as GIL-aware threads by the main
	 * thread (or any thread holding the GIL). Without this, callbacks
	 * enqueued during -e mode (no REPL idle loop) would never execute. */
	tcp_process_callbacks();

	headless_save_threadglobals(my_globals_handle);

	/* Release the GIL and let other threads run */
	pthread_mutex_unlock(&frontier_gil);
	pthread_cond_broadcast(&gil_available);

	/* Block briefly WITHOUT holding the GIL so waiting threads (e.g., TCP
	 * callback threads) can acquire it and execute. A bare sched_yield()
	 * was insufficient — it often returned before the waiting thread got
	 * scheduled, starving callback threads indefinitely.
	 *
	 * The 1ms timeout is a ceiling; callback threads signal headless_yield_cond
	 * when they finish, waking us early. */
	{
		struct timespec ts;
		clock_gettime(CLOCK_REALTIME, &ts);
		ts.tv_nsec += 1000000; /* 1ms */
		if (ts.tv_nsec >= 1000000000L) {
			ts.tv_sec++;
			ts.tv_nsec -= 1000000000L;
		}
		pthread_mutex_lock(&headless_yield_mutex);
		pthread_cond_timedwait(&headless_yield_cond, &headless_yield_mutex, &ts);
		pthread_mutex_unlock(&headless_yield_mutex);
	}

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
 *
 * Uses a polling loop (50ms intervals) to periodically reacquire the
 * GIL and process TCP callbacks. Without this, HTTP requests arriving
 * during a long sleep would be accepted but never dispatched — the
 * callback would sit in the queue until the sleep expires.
 */
static boolean headless_thread_sleep(long ticks) {
	long idthread = (long)(**hthreadglobals).idthread;
	frontier_pthread_record *rec = get_thread_by_id(idthread);
	hdlthreadglobals my_globals_handle = hthreadglobals;

	if (rec == NULL)
		return false;

	/* Process any pending TCP callbacks before sleeping */
	tcp_process_callbacks();

	/* Save globals before releasing the GIL */
	headless_save_threadglobals(my_globals_handle);

	/* Mark as sleeping and compute absolute wake time */
	pthread_mutex_lock(&rec->state_mutex);
	rec->is_sleeping = true;
	rec->is_woken = false;	/* clear predicate before entering sleep loop */

	struct timespec wake_time;
	clock_gettime(CLOCK_REALTIME, &wake_time);
	long seconds = ticks / TICKS_PER_SECOND;
	long remaining_ticks = ticks % TICKS_PER_SECOND;
	wake_time.tv_sec += seconds;
	wake_time.tv_nsec += (remaining_ticks * 1000000000L) / TICKS_PER_SECOND;
	if (wake_time.tv_nsec >= 1000000000L) {
		wake_time.tv_sec += 1;
		wake_time.tv_nsec -= 1000000000L;
	}

	/* Polling loop: sleep in 50ms intervals, periodically reacquiring
	 * the GIL to process TCP callbacks. This ensures HTTP connections
	 * accepted during a long sleep get dispatched promptly.
	 *
	 * Lock ordering: frontier_gil → state_mutex (same as thread.wake/kill).
	 * After condvar wait re-acquires state_mutex, we must release it BEFORE
	 * acquiring frontier_gil to avoid ABBA deadlock. */
	boolean was_killed = false;
	boolean was_woken = false;

	while (!was_killed && !was_woken) {
		/* Compute next poll time: min(wake_time, now + 50ms) */
		struct timespec poll_time;
		clock_gettime(CLOCK_REALTIME, &poll_time);
		poll_time.tv_nsec += 50000000L; /* 50ms */
		if (poll_time.tv_nsec >= 1000000000L) {
			poll_time.tv_sec++;
			poll_time.tv_nsec -= 1000000000L;
		}

		/* Use wake_time if it comes before the next poll */
		boolean is_final = false;
		if (poll_time.tv_sec > wake_time.tv_sec ||
			(poll_time.tv_sec == wake_time.tv_sec && poll_time.tv_nsec >= wake_time.tv_nsec)) {
			poll_time = wake_time;
			is_final = true;
		}

		/* Release the GIL so other threads can run while we sleep */
		pthread_mutex_unlock(&frontier_gil);
		pthread_cond_broadcast(&gil_available);

		/* Wait until poll_time or wake/kill signal.
		 * condvar atomically releases state_mutex while waiting and
		 * re-acquires it on return. */
		{
			int wait_rc = pthread_cond_timedwait(&rec->wake_cond, &rec->state_mutex, &poll_time);

			if (wait_rc != 0 && wait_rc != ETIMEDOUT)
				log_error(LOG_COMP_THREAD, "headless_thread_sleep: pthread_cond_timedwait failed: %d", wait_rc);
		}

		/* Read state under state_mutex, then release it BEFORE acquiring
		 * the GIL. This maintains the lock ordering (GIL → state_mutex)
		 * and prevents ABBA deadlock with thread.wake/kill which acquire
		 * state_mutex while holding the GIL. */
		was_killed = rec->is_killed;
		was_woken = rec->is_woken;	/* predicate flag, not condvar return */
		rec->is_sleeping = (!(is_final || was_woken || was_killed));
		pthread_mutex_unlock(&rec->state_mutex);

		/* Reacquire the GIL (no other mutex held — safe) */
		pthread_mutex_lock(&frontier_gil);

		/* Process any TCP callbacks that arrived during the sleep interval.
		 * Invariant: tcp_process_callbacks only acquires CALLBACK_QUEUE_LOCK,
		 * never state_mutex, so no deadlock risk here. */
		headless_restore_threadglobals(my_globals_handle);
		tcp_process_callbacks();
		headless_save_threadglobals(my_globals_handle);

		if (is_final || was_woken || was_killed)
			break;

		/* Re-lock state_mutex for the next condvar wait iteration */
		pthread_mutex_lock(&rec->state_mutex);
	}

	/* Globals are in saved state from the last save_threadglobals in the loop.
	 * Restore them now that we hold the GIL and are done sleeping. */
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
			if (bserror) copystring(PSTRING("\017", "not implemented"), bserror);
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
			rec->is_woken = true;  /* predicate flag for spurious wakeup detection */
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
			if (bserror) copystring(PSTRING("\017", "not implemented"), bserror);
			return false;
		case thrv_settimeslice:
			/* Verb #13: thread.settimeslice - not yet implemented */
			log_warn(LOG_COMP_LANG, "thread.settimeslice not yet implemented");
			if (bserror) copystring(PSTRING("\017", "not implemented"), bserror);
			return false;
		case thrv_getdefaulttimeslice:
			/* Verb #14: thread.getdefaulttimeslice - not yet implemented */
			log_warn(LOG_COMP_LANG, "thread.getdefaulttimeslice not yet implemented");
			if (bserror) copystring(PSTRING("\017", "not implemented"), bserror);
			return false;
		case thrv_setdefaulttimeslice:
			/* Verb #15: thread.setdefaulttimeslice - not yet implemented */
			log_warn(LOG_COMP_LANG, "thread.setdefaulttimeslice not yet implemented");
			if (bserror) copystring(PSTRING("\017", "not implemented"), bserror);
			return false;
		case thrv_getstats:
			/* Verb #16: thread.getstats - not yet implemented */
			log_warn(LOG_COMP_LANG, "thread.getstats not yet implemented");
			if (bserror) copystring(PSTRING("\017", "not implemented"), bserror);
			return false;
		default:
			return false;
	}
}

boolean threadinitverbs(void) {
	hdlhashtable htable = nil;
	bigstring bsname;

	copystring(PSTRING("\006", "thread"), bsname);

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

	ADD_VERB(PSTRING("\006", "exists"), thrv_exists);
	ADD_VERB(PSTRING("\010", "evaluate"), thrv_evaluate);
	ADD_VERB(PSTRING("\012", "callscript"), thrv_callscript);
	ADD_VERB(PSTRING("\014", "getcurrentid"), thrv_getcurrentid);
	ADD_VERB(PSTRING("\010", "getcount"), thrv_getcount);
	ADD_VERB(PSTRING("\010", "getnthid"), thrv_getnthid);
	ADD_VERB(PSTRING("\005", "sleep"), thrv_sleep);
	ADD_VERB(PSTRING("\010", "sleepfor"), thrv_sleepfor);
	ADD_VERB(PSTRING("\012", "sleepticks"), thrv_sleepticks);
	ADD_VERB(PSTRING("\012", "issleeping"), thrv_issleeping);
	ADD_VERB(PSTRING("\004", "wake"), thrv_wake);
	ADD_VERB(PSTRING("\004", "kill"), thrv_kill);
	ADD_VERB(PSTRING("\014", "gettimeslice"), thrv_gettimeslice);
	ADD_VERB(PSTRING("\014", "settimeslice"), thrv_settimeslice);
	ADD_VERB(PSTRING("\023", "getdefaulttimeslice"), thrv_getdefaulttimeslice);
	ADD_VERB(PSTRING("\023", "setdefaulttimeslice"), thrv_setdefaulttimeslice);
	ADD_VERB(PSTRING("\010", "getstats"), thrv_getstats);

	#undef ADD_VERB

	pophashtable();
	return true;
}
