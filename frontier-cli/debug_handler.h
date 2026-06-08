/*
 * debug_handler.h - Protocol-based UserTalk debugger
 *
 * Provides debug/* protocol operations for headless script debugging:
 *	 debug/run			   — Run script in debug mode (non-blocking, spawns thread)
 *	 debug/continue		   — Resume suspended thread
 *	 debug/kill			   — Kill a debug thread
 *	 debug/pause		   — Interrupt a running thread
 *	 debug/setBreakpoint   — Set or clear a breakpoint (Phase 3)
 *	 debug/listBreakpoints — List all breakpoints (Phase 3)
 *	 debug/getLocals	   — Inspect local variables of suspended thread (Phase 4)
 *	 debug/getSource	   — View script source with line numbers (Phase 4)
 *	 debug/getStack		   — View call stack of suspended thread (Phase 4)
 *
 * The debugger replaces the headless no-op callback with a protocol-aware
 * callback that can suspend execution and wait for client commands.
 *
 * See planning/phase6/USERTALK_DEBUGGER_PLAN.md for full design.
 */

#ifndef DEBUG_HANDLER_H
#define DEBUG_HANDLER_H

#include <pthread.h>
#include <stdatomic.h>

#include "op_handler.h"
#include "../Common/headers/frontier.h"
#include "../Common/headers/lang.h"

/*
 * Suspension reasons — type-safe enum instead of raw strings.
 * Prevents JSON injection and ensures only known values are sent.
 */
typedef enum {
	DEBUG_REASON_ENTRY,		   /* suspended at entry before first statement */
	DEBUG_REASON_INTERRUPTED,  /* suspended via debug/pause */
	DEBUG_REASON_BREAKPOINT,   /* suspended at breakpoint (Phase 3) */
	DEBUG_REASON_STEP,		   /* suspended after step (Phase 2) */
	DEBUG_REASON_WATCHPOINT,   /* suspended on watchpoint value change (Phase 6) */
	DEBUG_REASON_ERROR		   /* suspended on error (future) */
} debug_suspend_reason_t;

/* Returns the JSON-safe string for a reason enum value */
const char *debug_reason_string(debug_suspend_reason_t reason);

/*
 * Step directions — matches legacy tydirection values from standard.h.
 */
typedef enum {
	DEBUG_STEP_NONE = 0,	/* not stepping */
	DEBUG_STEP_OVER = 2,	/* next line at same call depth (legacy: down) */
	DEBUG_STEP_OUT	= 3,	/* return to caller (legacy: left) */
	DEBUG_STEP_INTO = 4		/* next statement regardless of depth (legacy: right) */
} debug_step_direction_t;

/*
 * Per-thread debug state. Stored in tythreadglobals.debugstate.
 * Allocated when a thread enters debug mode, freed on thread exit.
 *
 * Cross-thread flags (flsuspended, flinterrupt, flkill) use _Atomic
 * because they are written by the protocol handler thread and read by
 * the debug thread. The GIL provides ordering at yield boundaries,
 * but _Atomic prevents register-caching between yields.
 *
 * Thread safety: refcount protects against use-after-free. Protocol
 * handlers increment refcount via debug_get_state_for_thread (under
 * g_debug_mutex), use the pointer, then call debug_release_state.
 * The debug thread frees the struct only when refcount drops to 0.
 */

/* Source tracking constants (used by tydebugstate and debug_breakpoint_t) */
#define DEBUG_SCRIPT_PATH_MAX 256
#define DEBUG_SCRIPT_STACK_MAX 32

typedef struct tydebugstate {
	boolean fldebugmode;			 /* not atomic: set once at registration (under GIL) before
									  * thread starts, only read after. GIL provides ordering. */
	boolean fldetached;				 /* true for lazily-attached callScript threads, which are
									  * DETACHED (pthread_join is UB on them) and whose transport
									  * lifetime is managed by g_lazy_attached_count.
									  * Not atomic: written once at registration, read only during
									  * kill/join (under g_debug_mutex) and unregister (single
									  * writer, GIL-ordered). */
	atomic_bool flsuspended;		 /* is this thread paused? */
	atomic_bool flinterrupt;		 /* pause at next statement (debug/pause) */
	atomic_bool flkill;				 /* kill the script */
	atomic_int refcount;			 /* reference count (1 = debug thread only) */
	transport_t *transport;			 /* for sending notifications back to client */
	long threadid;					 /* this thread's ID */
	pthread_t pthread_id;			 /* POSIX thread ID for pthread_join */
	void *hglobals;					 /* hdlthreadglobals (processinternal.h) — void* to
									  * avoid pulling that header into this one. Cast to
									  * hdlthreadglobals in debug_handler.c. Safe to read
									  * when thread is suspended (in nanosleep). */

	/* Stepping state (Phase 2) — written by handle_debug_step on main thread,
	 * read by protocol_debugger_callback on debug thread. Access is GIL-ordered
	 * (step command runs while debug thread is suspended, thread resumes after).
	 * All fields use C11 atomic types for consistency with other cross-thread
	 * flags. atomic_short/atomic_ulong are standard C11 convenience typedefs
	 * (§7.17.6) supported by clang, GCC, and MSVC 2022+. */
	atomic_bool flstepping;			 /* stepping mode active */
	atomic_int stepdir;				 /* current step direction (debug_step_direction_t) */
	atomic_ulong lastlnum;			 /* line number at last suspension */
	boolean flskipaliasline;		 /* skip breakpoints at lastlnum until line changes;
									  * set on resume, cleared when lnum != lastlnum.
									  * Not atomic: written by protocol handler (under GIL)
									  * and read by debug thread callback (under GIL).
									  * Safe because only one thread holds the GIL at a time. */
	atomic_short steplevel;			 /* call depth when step was initiated */
	atomic_short calldepth;			 /* current call depth — 0 at top-level expression,
									  * incremented on function entry (push sourcecode),
									  * decremented on return (pop sourcecode). Used by
									  * step-over (same depth) and step-out (shallower). */

	/* Source tracking (Phase 3) — tracks which script is currently executing.
	 * Updated by the push/pop sourcecode callbacks installed in debug_init().
	 * The callback reads current_script to match breakpoints.
	 * Script path stack handles nested calls (A calls B): push saves path,
	 * pop restores caller's path so breakpoints in A still fire after B returns. */
	char current_script[DEBUG_SCRIPT_PATH_MAX]; /* current script dotted path */
	char script_stack[DEBUG_SCRIPT_STACK_MAX][DEBUG_SCRIPT_PATH_MAX]; /* saved caller paths */
	unsigned long script_stack_lines[DEBUG_SCRIPT_STACK_MAX]; /* saved caller line numbers */
	short script_stack_depth;					/* stack pointer (0 = empty) */
	int script_stack_overflow;					/* push/pop balance when stack overflows */
} tydebugstate, *ptrdebugstate;

/*
 * Initialize the debug subsystem. Call once at startup.
 * Installs the protocol-aware debugger callback.
 */
void debug_init(void);

/*
 * Handle debug/* protocol operations.
 * Called from op_dispatch() in op_handler.c.
 */
void handle_debug_run(int id, const char *json_line, transport_t *transport);
void handle_debug_step(int id, const char *json_line, transport_t *transport);
void handle_debug_continue(int id, const char *json_line, transport_t *transport);
void handle_debug_kill(int id, const char *json_line, transport_t *transport);
void handle_debug_pause(int id, const char *json_line, transport_t *transport);
void handle_debug_setbreakpoint(int id, const char *json_line, transport_t *transport);
void handle_debug_clearbreakpoints(int id, const char *json_line, transport_t *transport);
void handle_debug_listbreakpoints(int id, const char *json_line, transport_t *transport);
void handle_debug_getlocals(int id, const char *json_line, transport_t *transport);
void handle_debug_getsource(int id, const char *json_line, transport_t *transport);
void handle_debug_getstack(int id, const char *json_line, transport_t *transport);
void handle_debug_listthreads(int id, const char *json_line, transport_t *transport);
void handle_debug_setwatchpoint(int id, const char *json_line, transport_t *transport);
void handle_debug_listwatchpoints(int id, const char *json_line, transport_t *transport);
void handle_debug_clearwatchpoints(int id, const char *json_line, transport_t *transport);

/*
 * Release a reference to a debug state obtained from debug_get_state_for_thread.
 * Frees the struct if refcount drops to 0.
 */
void debug_release_state(tydebugstate *state);

/*
 * Kill all active debug threads. Called during shutdown.
 */
void debug_kill_all_threads(void);

/*
 * Join all debug threads. Called during shutdown after kill, with GIL released.
 * Blocks until all debug threads have exited.
 */
void debug_join_all_threads(void);

/*
 * Returns true if any debug threads are active (registered in g_debug_threads).
 */
boolean debug_has_active_threads(void);

/*
 * Returns true when it is safe to save the database on exit. Returns false
 * only if a debug thread was killed mid-execution — killing interrupts
 * hash table scope unwinding, leaving tables inconsistent for packing.
 * Debug threads that complete normally (via continue) are safe to save after.
 */
boolean debug_is_safe_to_save(void);

/*
 * Send an unsolicited debug/suspended notification to the client.
 * script_path is optional (may be NULL). When non-NULL, the notification
 * includes a "script" field so clients can identify which script hit the
 * breakpoint without a separate debug/getStack call.
 */
void debug_send_suspended(transport_t *transport, long threadid, long line,
                          debug_suspend_reason_t reason, const char *script_path);

/*
 * Set or clear the lazy-attach transport. Call with a non-NULL transport at
 * protocol session start and with NULL at session exit.
 *
 * Lifetime contract (2026-06-06 JES #691, heap-allocation fix):
 *   The transport_t pointed to by t MUST be heap-allocated and MUST remain
 *   valid until debug_set_attach_transport(NULL) is called AND
 *   debug_wait_lazy_threads_drained() returns. protocol_main fulfills this
 *   by allocating the transport on the heap, registering it here, waiting
 *   for g_lazy_attached_count to drop to zero via
 *   debug_wait_lazy_threads_drained(), and only then calling
 *   debug_set_attach_transport(NULL) and freeing the heap transport.
 *
 *   Stack-local transports (e.g., per-WS-message) do NOT satisfy the
 *   contract because the stack frame can unwind while a lazy-attached thread
 *   still holds a pointer. WS lazy attach is intentionally out of scope
 *   for this commit (UAF risk, deferred to a follow-up).
 *
 * 2026-06-06 JES #691
 */
void debug_set_attach_transport(transport_t *t);

/*
 * Wait until all lazily-attached threads have unregistered (i.e., completed
 * their final debug_unregister_thread call, which decrements
 * g_lazy_attached_count). Returns immediately if the count is already zero.
 * Polls with 10ms sleep; expected to return quickly in normal operation.
 * Must be called with the GIL held (releases and reacquires during each
 * sleep cycle so lazy threads can complete their cleanup).
 *
 * 2026-06-06 JES #691
 */
void debug_wait_lazy_threads_drained(void);

/*
 * Send a debug/completed notification to the client.
 * success is the return value of the script execution (langruncode/langrunscriptcode).
 *
 * 2026-06-06 JES #691: Promoted from static (was debug_handler.c-only) so
 * that unified_thread_entry in headless_spawn.c can call it.
 */
void debug_send_completed(transport_t *transport, long threadid, boolean success);

/*
 * Register a thread in the debug thread table.
 * Returns an allocated tydebugstate on success, NULL if the table is full.
 *
 * fldetached: pass true for lazily-attached callScript threads (POSIX DETACHED,
 *   pthread_join is UB on them). These are excluded from debug_kill_all_threads'
 *   kill-list capture and debug_join_all_threads' join loop. They also increment
 *   g_lazy_attached_count so protocol_main can wait for them to drain before
 *   freeing the heap transport (P1 #1 + P1 #2 fix, 2026-06-06 JES #691).
 *   Pass false for debug/run threads (joinable, pthread_id is valid).
 *
 * 2026-06-06 JES #691: Promoted from static so that unified_thread_entry in
 * headless_spawn.c can register the thread under its own ID (which is known
 * only after the thread starts and allocates its registry record).
 */
tydebugstate *debug_register_thread(long threadid, transport_t *transport,
                                    boolean fldetached);

/*
 * Unregister a thread from the debug thread table.
 * Called when a debug thread exits (inside unified_thread_entry).
 *
 * 2026-06-06 JES #691: Promoted from static (was debug_handler.c-only).
 */
void debug_unregister_thread(long threadid);

/*
 * 2026-06-08 JES Phase B.8 #691 P2-7: shared ODB source-fetch helper.
 *
 * Walk the ODB for the script at `path_no_at` (dotted path WITHOUT leading '@'),
 * extract the outline text via opgetlangtext, and return a heap Handle of the
 * source text on success.  The caller owns the returned Handle and must call
 * disposehandle() when done.
 *
 * Returns nil on any error (path not found, not a script, ODB load failure).
 *
 * GIL: must be called with GIL held (ODB operations are not thread-safe).
 *
 * Extracted from handle_debug_getsource to eliminate the parallel
 * implementation in tui_real_odb_fetch (debugger_tui.c).  Both call sites
 * now delegate to this helper.
 */
Handle debug_get_script_source(const char *path_no_at);

#endif /* DEBUG_HANDLER_H */
