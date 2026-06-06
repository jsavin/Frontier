/*
 * headless_spawn.h - Unified script-thread spawning primitive
 *
 * Provides headless_spawn_script_thread(), a single entry point that
 * absorbs the common setup shared by debug/run (debug_handler.c) and
 * thread.callScript (headless_thread_verbs.c).
 *
 * Common sequence handled here:
 *   - Alloc thread record via allocate_thread_record()
 *   - Alloc thread globals via headless_new_threadglobals()
 *   - Wire idthread + thread record cross-pointers
 *   - Alloc fresh tytablestack via newclearhandle()
 *   - Set hcurrenthashtable = roottable
 *   - Call headless_register_thread() (user-side registration)
 *   - If debug: call debug_register_thread(own_id, transport) BEFORE pthread_create
 *   - Package params, pthread_attr_init, pthread_create, error-cleanup ladder
 *
 * Debug-specific behavior (initial suspend, debug_register_thread, joinable
 * thread) is gated on the nullable thread_debug_opts argument.
 *
 * 2026-06-06 JES #691: Extracted from debug_handler.c and
 *   headless_thread_verbs.c as part of the PR-2 unification.
 *
 * See docs/THREAD_DEBUG_ATTACH_PLAN.md for the full design.
 */

#ifndef HEADLESS_SPAWN_H
#define HEADLESS_SPAWN_H

#include "../Common/headers/frontier.h"
#include "../Common/headers/strings.h"
#include "../Common/headers/processinternal.h"
#include "op_handler.h"   /* transport_t */

/*
 * thread_run_spec - selects the execution path inside the spawned thread.
 *
 * is_callscript == false: calls langruncode(hcode, nil, &result)
 *   -- hcode is owned by the caller (debug/run compiled it); the thread
 *      disposes it on exit.
 *
 * is_callscript == true: calls langrunscriptcode(htable, bsverb, hcode,
 *   &vparams, hcontext, &result)
 *   -- hcode is owned by the hash table; the thread does NOT dispose it.
 *   -- vparams is a deep-copy owned by the thread; thread disposes it.
 */
typedef struct {
	bigstring bsverb;      /* handler name;  empty  for langruncode path */
	hdlhashtable htable;   /* owning table;  NULL   for langruncode path */
	hdlhashtable hcontext; /* context table; NULL   for langruncode path */
	tyvaluerecord vparams; /* call params;   unused for langruncode path */
	boolean is_callscript; /* true => langrunscriptcode; false => langruncode */
} thread_run_spec;

/*
 * thread_debug_opts - optional debug configuration for the spawned thread.
 *
 * Pass NULL to get a detached thread with no debug wiring and no initial
 * suspension (the callScript / evaluate path).
 *
 * Pass a non-NULL pointer for debug/run semantics:
 *   - Thread is created JOINABLE (so debug_join_all_threads can pthread_join it)
 *   - debug_register_thread() is called BEFORE pthread_create using the
 *     allocated thread's own ID, preserving the original parity that the
 *     debug slot is populated before the thread can run
 *   - The resulting tydebugstate* is stored at *out_debugstate (void** to
 *     avoid pulling tydebugstate into this header); the caller uses it to
 *     set debugstate->pthread_id under g_debug_mutex after the spawn returns
 *   - If start_suspended is true, the thread entry sends DEBUG_REASON_ENTRY
 *     and blocks until the client sends debug/continue
 *
 * On spawn failure, headless_spawn_script_thread calls debug_unregister_thread
 * and frees the debug state. *out_debugstate is set to NULL on failure.
 *
 * transport must remain valid for the lifetime of the spawned thread.
 */
typedef struct {
	transport_t *transport;     /* for sending debug event notifications */
	void **out_debugstate;      /* *out_debugstate = tydebugstate* on success, NULL on fail */
	boolean start_suspended;    /* if true: suspend before first statement */
} thread_debug_opts;

/*
 * headless_spawn_script_thread - allocate, wire, and launch a GIL-aware thread
 *
 * Parameters:
 *   hcode             -- compiled AST to execute (ownership per run_spec)
 *   run_spec          -- selects langruncode vs langrunscriptcode; non-NULL required
 *   debug_opts        -- NULL for a detached/undebugable thread; see above
 *   out_user_threadid -- receives the UserTalk-visible thread ID on success
 *
 * Returns true on successful pthread_create, false on any setup failure.
 * On failure, all allocated resources have been freed (including hcode when
 * run_spec->is_callscript == false). Callers of the langrunscriptcode path
 * retain ownership of hcode on failure.
 *
 * Must be called while holding the GIL.
 */
boolean headless_spawn_script_thread(hdltreenode hcode,
                                     const thread_run_spec *run_spec,
                                     const thread_debug_opts *debug_opts,
                                     long *out_user_threadid);

#endif /* HEADLESS_SPAWN_H */
