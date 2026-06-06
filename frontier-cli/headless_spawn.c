/*
 * headless_spawn.c - Unified script-thread spawning primitive
 *
 * See headless_spawn.h for the public interface.
 *
 * Absorbs the common ~90% setup sequence shared by:
 *   - debug/run  (handle_debug_run in debug_handler.c)
 *   - thread.callScript (headless_thread_callscript in headless_thread_verbs.c)
 *
 * The two callers differ only in:
 *   - run_spec.is_callscript (selects langruncode vs langrunscriptcode)
 *   - debug_opts (NULL for callScript; non-NULL for debug/run)
 *
 * 2026-06-06 JES #691: Extracted from debug_handler.c and
 *   headless_thread_verbs.c as part of the PR-2 unification.
 *
 * See docs/THREAD_DEBUG_ATTACH_PLAN.md for the full design context.
 */

#include "headless_spawn.h"

#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "frontier.h"
#include "standard.h"
#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "langinternal.h"
#include "tablestructure.h"
#include "processinternal.h"
#include "logging.h"
#include "threadregistry.h"
#include "headless_threading.h"  /* GIL, save/restore, register/unregister,
                                  * headless_yield_mutex, headless_yield_cond */
#include "debug_handler.h"       /* tydebugstate, debug_register_thread,
                                  * debug_unregister_thread,
                                  * debug_send_suspended, debug_send_completed */

/* roottable: the clean base for independent spawned-thread execution */
extern hdlhashtable roottable;

/* ========================================================================
 * Internal params struct passed from spawner to thread entry
 * ======================================================================== */

/*
 * spawn_params - heap-allocated params block passed via pthread_create arg.
 *
 * Lifetime: allocated in headless_spawn_script_thread before pthread_create,
 * owned and freed by unified_thread_entry after cleanup.
 */
typedef struct {
	hdltreenode hcode;             /* compiled AST */
	hdlthreadglobals hglobals;     /* this thread's UserTalk globals */
	frontier_pthread_record *rec;  /* registry record (freed on exit) */
	thread_run_spec run;           /* copy of caller's run_spec */
	tydebugstate *debugstate;      /* non-NULL => debug mode; NULL => callScript */
	boolean start_suspended;       /* mirrors thread_debug_opts.start_suspended */
} spawn_params;

/* ========================================================================
 * Unified thread entry point
 * ======================================================================== */

/*
 * unified_thread_entry - single POSIX thread entry replacing both
 *   debug_thread_entry (debug_handler.c) and thread_entry_point
 *   (headless_thread_verbs.c).
 *
 * Execution sequence (mirrors debug_thread_entry:925-1009 and
 * thread_entry_point:237-298):
 *   1. Acquire GIL
 *   2. Restore thread globals
 *   3a. Debug path: wire debugstate <-> hglobals cross-pointers;
 *       register "debug" in system.compiler.threads
 *       (debug_register_thread was already called in the spawner)
 *   4. If start_suspended: send DEBUG_REASON_ENTRY; spin-wait for resume
 *      (mirrors debug_thread_entry:954-974)
 *   5. Dispatch: langrunscriptcode or langruncode
 *   6. Debug path: send debug/completed with the script's success flag
 *   7. Cleanup: save globals, clear error, unregister (user + debug), dispose
 *   8. Release GIL + broadcast
 *   9. callScript path: signal headless_yield_cond
 */
static void *unified_thread_entry(void *arg) {

	spawn_params *params = (spawn_params *)arg;
	tyvaluerecord result;
	boolean fl_ran = false;
	boolean fl_success = false;
	/*
	 * 2026-06-06 JES #691 (P1 #4 fix): hoisted to function top to avoid
	 * "declared after goto" UB. Previously declared after the goto cleanup
	 * at line ~149; the C standard says the initializer is skipped on the
	 * goto path, leaving the variable indeterminate at the else-if check
	 * below. Currently unreachable on the goto path (params->debugstate is
	 * non-NULL when goto fires), but fragile. Hoist removes the latent UB.
	 */
	tydebugstate *lazy_state = NULL;

	if (params == NULL)
		return NULL;

	/* --- Step 1: Acquire GIL --- */
	pthread_mutex_lock(&frontier_gil);

	/* --- Step 2: Restore thread globals --- */
	headless_restore_threadglobals(params->hglobals);

	/* --- Step 3a: Debug wiring (debug mode only) --- */
	if (params->debugstate != NULL) {

		/*
		 * Wire debugstate into thread globals so the debugger callback can find
		 * it, and wire hglobals into debugstate so protocol handlers can access
		 * locals of a suspended thread.
		 * Mirrors debug_thread_entry:946-950.
		 */
		(**params->hglobals).debugstate = (void *)params->debugstate;
		params->debugstate->hglobals = (void *)params->hglobals;

		/*
		 * Register "debug" in system.compiler.threads.
		 * Mirrors debug_thread_entry:939-943.
		 * debug_register_thread was called in the spawner (before pthread_create)
		 * so the debug slot is already populated when we arrive here.
		 */
		bigstring bsname;
		copyctopstring("debug", bsname);
		headless_register_thread(bsname, params->rec->user_thread_id);
	}

	/* --- Step 4: Initial suspension (debug mode only) --- */
	if (params->debugstate != NULL && params->start_suspended) {

		/*
		 * Suspend before the first statement so the client can set breakpoints.
		 * Mirrors debug_thread_entry:954-974.
		 */
		atomic_store(&params->debugstate->flsuspended, true);
		debug_send_suspended(
			params->debugstate->transport,
			params->debugstate->threadid,
			0,
			DEBUG_REASON_ENTRY,
			NULL); /* no current script at entry suspension */

		while (atomic_load(&params->debugstate->flsuspended)) {

			if (atomic_load(&params->debugstate->flkill)) {
				debug_send_completed(
					params->debugstate->transport,
					params->debugstate->threadid,
					false);
				goto cleanup;
			}

			headless_save_threadglobals(params->hglobals);
			pthread_mutex_unlock(&frontier_gil);

			struct timespec ts = {0, 10000000}; /* 10 ms */
			nanosleep(&ts, NULL);

			pthread_mutex_lock(&frontier_gil);
			headless_restore_threadglobals(params->hglobals);
		}
	}

	/* --- Step 5: Dispatch --- */
	fl_ran = true;
	initvalue(&result, novaluetype);

	if (params->run.is_callscript) {
		fl_success = langrunscriptcode(
			params->run.htable,
			params->run.bsverb,
			params->hcode,
			&params->run.vparams,
			params->run.hcontext,
			&result);
	}
	else {
		fl_success = langruncode(params->hcode, nil, &result);
	}

	/* 2026-06-06 JES #691: Check for lazy debug attach.
	 * A callScript thread (params->debugstate == NULL) may have been lazily
	 * registered by the breakpoint callback if a protocol session was active
	 * and a breakpoint matched. In that case, hthreadglobals->debugstate was
	 * set by the callback. We must send debug/completed and unregister here,
	 * since unified_thread_entry is the only place that can do so reliably
	 * (the callback can return at any point, the GIL owner is this thread).
	 * lazy_state was declared at function top (P1 #4 fix). */
	if (params->debugstate == NULL && params->hglobals != nil) {
		lazy_state = (tydebugstate *)((**params->hglobals).debugstate);
	}

	/* --- Step 6: Debug completion notification --- */
	if (params->debugstate != NULL) {
		/*
		 * Send the actual success/failure status of the script execution.
		 * Mirrors debug_thread_entry:980-983:
		 *   boolean fl = langruncode(...);
		 *   debug_send_completed(..., fl);
		 */
		debug_send_completed(
			params->debugstate->transport,
			params->debugstate->threadid,
			fl_success);
	} else if (lazy_state != NULL) {
		/* Lazy-attached thread: send completion on the lazily-acquired transport */
		debug_send_completed(lazy_state->transport, lazy_state->threadid, fl_success);
	}

cleanup:
	/* --- Step 7: Cleanup --- */

	if (fl_ran)
		disposevaluerecord(result, false);

	/* Save globals while we still hold the GIL */
	headless_save_threadglobals(params->hglobals);

	/* Clear error state */
	headless_clear_last_lang_error();

	/* Unregister from system.compiler.threads */
	headless_unregister_thread(params->rec->user_thread_id);

	/* Unregister from debug registry (if explicitly or lazily registered).
	 * Mirrors debug_thread_entry:997. */
	if (params->debugstate != NULL) {
		debug_unregister_thread(params->debugstate->threadid);
	} else if (lazy_state != NULL) {
		/* 2026-06-06 JES #691: Unregister lazily-attached state */
		debug_unregister_thread(lazy_state->threadid);
	}

	/*
	 * Dispose resources -- ownership rules:
	 *   langruncode path (is_callscript == false):
	 *     hcode was compiled by the caller; the thread owns and disposes it.
	 *   langrunscriptcode path (is_callscript == true):
	 *     hcode belongs to the ODB hash table; the thread must NOT dispose it.
	 *     vparams is a deep-copy owned by the thread; dispose it.
	 * Mirrors thread_entry_point:273-281.
	 */
	if (!params->run.is_callscript) {
		langdisposetree(params->hcode);
	}
	else {
		disposevaluerecord(params->run.vparams, false);
	}

	/* Save the debug flag before free(params) -- used in Step 9 below.
	 * A lazily-attached thread is treated as a debug thread for the
	 * purposes of the yield_cond signal: it was joinable? No -- callScript
	 * threads are DETACHED regardless of lazy attach. The yield_cond
	 * signal must still be sent for detached threads. Lazy attach does not
	 * change the thread's detach state, so is_debug_thread stays false
	 * for callScript threads to ensure the yield_cond signal fires. */
	boolean is_debug_thread = (params->debugstate != NULL);

	headless_dispose_threadglobals(params->hglobals);
	free_thread_record(params->rec);
	free(params);
	params = NULL;

	/* --- Step 8: Release GIL --- */
	pthread_mutex_unlock(&frontier_gil);
	pthread_cond_broadcast(&gil_available);

	/* --- Step 9: Signal headless_yield_cond (callScript / detached path only) ---
	 * headless_backgroundtask() yields the GIL and sleeps on headless_yield_cond
	 * waiting for spawned threads to do work. When a detached thread exits,
	 * signaling headless_yield_cond wakes the background task early rather than
	 * waiting for the full 1ms timeout.
	 * Mirrors thread_entry_point:293-295.
	 * Debug threads are joinable; their caller polls/joins separately --
	 * no yield_cond signal needed for the debug path. */
	if (!is_debug_thread) {
		pthread_mutex_lock(&headless_yield_mutex);
		pthread_cond_signal(&headless_yield_cond);
		pthread_mutex_unlock(&headless_yield_mutex);
	}

	return NULL;
}

/* ========================================================================
 * Public API
 * ======================================================================== */

boolean headless_spawn_script_thread(hdltreenode hcode,
                                     const thread_run_spec *run_spec,
                                     const thread_debug_opts *debug_opts,
                                     long *out_user_threadid) {

	frontier_pthread_record *rec = NULL;
	hdlthreadglobals new_hglobals = nil;
	spawn_params *sparams = NULL;
	tydebugstate *debugstate = NULL;
	pthread_t tid;
	pthread_attr_t attr;

	if (run_spec == NULL || out_user_threadid == NULL) {
		log_error(LOG_COMP_THREAD, "headless_spawn_script_thread: NULL required arg");
		return false;
	}

	/* Clear out_debugstate early so callers can always safely check it */
	if (debug_opts != NULL && debug_opts->out_debugstate != NULL)
		*debug_opts->out_debugstate = NULL;

	/* --- Allocate thread record --- */
	rec = allocate_thread_record();
	if (rec == NULL) {
		log_error(LOG_COMP_THREAD, "headless_spawn_script_thread: allocate_thread_record failed");
		return false;
	}

	/* --- Allocate thread globals --- */
	new_hglobals = headless_new_threadglobals();
	if (new_hglobals == nil) {
		log_error(LOG_COMP_THREAD, "headless_spawn_script_thread: headless_new_threadglobals failed");
		free_thread_record(rec);
		return false;
	}

	/* Wire idthread into globals and hglobals into rec */
	(**new_hglobals).idthread = (hdlthread)(long)rec->user_thread_id;
	rec->hglobals = new_hglobals;

	/* --- Allocate fresh, empty hashtablestack ---
	 * Copying the caller's toptables depth would cause stack overflow in the
	 * spawned script, producing a CPU-bound GIL-starvation hang. A zeroed
	 * tytablestack (toptables == 0, stack[] == nil) is the correct base.
	 * First applied as a fix in headless_thread_callscript (#706). */
	{
		Handle hcopy;

		if (!newclearhandle(sizeof(tytablestack), &hcopy)) {
			log_error(LOG_COMP_THREAD, "headless_spawn_script_thread: newclearhandle failed");
			headless_dispose_threadglobals(new_hglobals);
			free_thread_record(rec);
			return false;
		}

		(**new_hglobals).htablestack = (hdltablestack)hcopy;
	}

	/* Root table is the correct base for independent script execution */
	(**new_hglobals).hcurrenthashtable = roottable;

	/* --- Register debug state BEFORE pthread_create (debug path) ---
	 * Ensures the debug slot is populated before the thread can run,
	 * preserving parity with the original handle_debug_run ordering.
	 * Mirrors handle_debug_run:1143-1153.
	 *
	 * callScript path: skip -- no debug registration needed. */
	if (debug_opts != NULL) {
		debugstate = debug_register_thread((long)rec->user_thread_id, debug_opts->transport,
		                                   false /* fldetached: joinable debug/run thread */);

		if (debugstate == NULL) {
			log_error(LOG_COMP_THREAD,
				"headless_spawn_script_thread: debug_register_thread failed (table full)");
			headless_dispose_threadglobals(new_hglobals);
			free_thread_record(rec);
			return false;
		}
	}

	/* --- Register in system.compiler.threads (callScript path only) ---
	 * - callScript path: register here in the spawner, matching
	 *   headless_thread_callscript:812 (registration before pthread_create).
	 *   The verb name bsverb is available in the calling context.
	 * - debug path: registration happens inside unified_thread_entry
	 *   (name "debug", matching debug_thread_entry:939-943). */
	if (run_spec->is_callscript) {
		/* headless_register_thread takes a non-const bigstring; copy to drop
		 * the const from run_spec->bsverb (a read-only fixed-size array). */
		bigstring bsverb_copy;
		copystring(run_spec->bsverb, bsverb_copy);
		headless_register_thread(bsverb_copy, (long)rec->user_thread_id);
	}

	/* --- Package params --- */
	sparams = (spawn_params *)calloc(1, sizeof(spawn_params));
	if (sparams == NULL) {
		log_error(LOG_COMP_THREAD, "headless_spawn_script_thread: calloc sparams failed");
		if (debugstate != NULL)
			debug_unregister_thread((long)rec->user_thread_id);
		if (run_spec->is_callscript)
			headless_unregister_thread((long)rec->user_thread_id);
		headless_dispose_threadglobals(new_hglobals);
		free_thread_record(rec);
		return false;
	}

	sparams->hcode = hcode;
	sparams->hglobals = new_hglobals;
	sparams->rec = rec;
	sparams->run = *run_spec;
	sparams->debugstate = debugstate;

	if (debug_opts != NULL)
		sparams->start_suspended = debug_opts->start_suspended;

	/* --- pthread_attr: joinable for debug, detached for callScript --- */
	if (pthread_attr_init(&attr) != 0) {
		log_error(LOG_COMP_THREAD, "headless_spawn_script_thread: pthread_attr_init failed");
		if (debugstate != NULL)
			debug_unregister_thread((long)rec->user_thread_id);
		if (run_spec->is_callscript)
			headless_unregister_thread((long)rec->user_thread_id);
		headless_dispose_threadglobals(new_hglobals);
		free_thread_record(rec);
		free(sparams);
		return false;
	}

	{
		int detach_state = (debug_opts != NULL)
			? PTHREAD_CREATE_JOINABLE
			: PTHREAD_CREATE_DETACHED;

		if (pthread_attr_setdetachstate(&attr, detach_state) != 0) {
			log_error(LOG_COMP_THREAD, "headless_spawn_script_thread: pthread_attr_setdetachstate failed");
			pthread_attr_destroy(&attr);
			if (debugstate != NULL)
				debug_unregister_thread((long)rec->user_thread_id);
			if (run_spec->is_callscript)
				headless_unregister_thread((long)rec->user_thread_id);
			headless_dispose_threadglobals(new_hglobals);
			free_thread_record(rec);
			free(sparams);
			return false;
		}
	}

	/* --- pthread_create --- */
	if (pthread_create(&tid, &attr, unified_thread_entry, sparams) != 0) {
		log_error(LOG_COMP_THREAD, "headless_spawn_script_thread: pthread_create failed");
		pthread_attr_destroy(&attr);
		if (debugstate != NULL)
			debug_unregister_thread((long)rec->user_thread_id);
		if (run_spec->is_callscript)
			headless_unregister_thread((long)rec->user_thread_id);
		headless_dispose_threadglobals(new_hglobals);
		free_thread_record(rec);
		free(sparams);
		return false;
	}

	pthread_attr_destroy(&attr);

	/* Wire the pthread handle back into the registry record */
	rec->pthread_id = tid;

	/* Store pthread_id in debug state for debug_kill_all_threads.
	 * Mirrors handle_debug_run:1222-1228 (pre-refactor).
	 * We write without g_debug_mutex because: the spawned thread is blocked
	 * on GIL acquisition (it cannot read debugstate yet), and no other thread
	 * can interleave under GIL serialization. The GIL provides the necessary
	 * ordering here, consistent with the ADR-014 GIL model. */
	if (debugstate != NULL)
		debugstate->pthread_id = tid;

	/* Expose debug state pointer to caller for any post-spawn wiring
	 * (e.g., setting debugstate->pthread_id under g_debug_mutex). */
	if (debug_opts != NULL && debug_opts->out_debugstate != NULL)
		*debug_opts->out_debugstate = (void *)debugstate;

	*out_user_threadid = (long)rec->user_thread_id;
	return true;
}
