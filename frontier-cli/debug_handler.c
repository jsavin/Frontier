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
#include <time.h>     /* clock_gettime, CLOCK_MONOTONIC */

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
#include "db_format.h"
#include "../third_party/cJSON/cJSON.h"
#include "headless_spawn.h"  /* headless_spawn_script_thread, thread_run_spec, thread_debug_opts */

/* Global: currenthashtable is a macro in processinternal.h; roottable is the
 * clean base for independent spawned-thread execution. */
extern hdlhashtable roottable;

/* Forward declarations — these functions exist in Common/source but have no
 * header declaration. Used by debug/getSource for script path resolution
 * and outline-to-text conversion. */
extern boolean opgetlangtext(hdloutlinerecord, boolean, Handle *);	/* oplangtext.c */
extern boolean opverbinmemory(const struct db_context *, hdlexternalvariable);	/* opverbs.c */
extern void db_context_init(struct db_context *);  /* db_format.c */
extern void db_context_init_legacy_read(struct db_context *, hdldatabaserecord);  /* db_format.c */
extern boolean db_format_is_legacy_db(hdldatabaserecord);  /* db_format.c */
extern boolean langfastaddresstotable(hdlhashtable, bigstring, hdlhashtable *);	 /* langops.c */

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
#define DEBUG_VALUE_MAX 256	 /* shared: breakpoint conditions + watchpoint values */

typedef struct {
	char script[DEBUG_SCRIPT_PATH_MAX]; /* dotted script path, e.g. "mainResponder.respond" */
	unsigned long line;					/* 1-based line number */
	boolean active;						/* is this slot in use? */
	char condition[DEBUG_VALUE_MAX];	/* optional condition expression (Phase 7).
										 * Empty string = unconditional breakpoint.
										 * Simple format: "varname op value" where op is
										 * ==, !=, >, <, >=, <=. Evaluated against locals
										 * when breakpoint line is reached. */
} debug_breakpoint_t;

static debug_breakpoint_t g_breakpoints[MAX_BREAKPOINTS] = {0};
static atomic_bool g_has_breakpoints = false; /* fast-path: skip mutex when no breakpoints set */

/* ========================================================================
 * Lazy-attach infrastructure (#691)
 *
 * Allows callScript-spawned threads (which have state == NULL) to be
 * lazily registered with the debug subsystem when they hit a breakpoint.
 *
 * TLS tracks the current script path on every thread -- not just debug
 * threads -- so the breakpoint callback can match scripts even when
 * the thread has no registered debug state.
 *
 * g_debug_attach_transport holds the stdio transport for the active
 * protocol session. Set by protocol_main via debug_set_attach_transport;
 * cleared at session exit. The pointer-to-transport is valid for the
 * lifetime of the protocol_main stack frame, which outlives any spawned
 * thread that could hit a breakpoint during the session.
 *
 * Lock ordering: g_debug_attach_transport is load/acquire by the
 * breakpoint callback (running under GIL), set/cleared by protocol_main
 * (also under GIL). The GIL serializes all writes, so a simple
 * memory_order_acquire load is sufficient.
 * ======================================================================== */

/* 2026-06-06 JES #691: per-thread record of the current script path,
   used by the breakpoint callback to match breakpoints on threads
   that don't yet have a registered debug state. */
static __thread char tls_current_script[DEBUG_SCRIPT_PATH_MAX];
static __thread short tls_script_depth;
static __thread char tls_script_stack[DEBUG_SCRIPT_STACK_MAX][DEBUG_SCRIPT_PATH_MAX];

/* 2026-06-06 JES #691: when a protocol debug session is attached,
   this is the transport the lazy-attach path uses to register
   spawned threads. NULL when no debug client is attached.
   Lifetime contract: the pointed-to transport_t must be heap-allocated
   and must remain valid until debug_wait_lazy_threads_drained() returns
   after debug_set_attach_transport(NULL) is called. See debug_handler.h
   for the full contract. Stack-local transports do NOT satisfy it. */
static _Atomic(transport_t *) g_debug_attach_transport = NULL;

/*
 * 2026-06-06 JES #691 (P1 #1 fix): count of lazily-attached callScript
 * threads that are currently registered in g_debug_threads. Incremented
 * under g_debug_mutex when a lazy thread registers (fldetached=true);
 * decremented under g_debug_mutex in debug_unregister_thread when that
 * thread exits. protocol_main waits for this to drop to zero before
 * freeing the heap-allocated transport (debug_wait_lazy_threads_drained).
 *
 * Atomic so protocol_main can read it without holding g_debug_mutex
 * in the polling loop of debug_wait_lazy_threads_drained.
 */
static atomic_int g_lazy_attached_count = 0;

/* Set or clear the lazy-attach transport. Pass NULL to clear at session
   teardown. See debug_handler.h for the heap-allocation lifetime contract. */
void debug_set_attach_transport(transport_t *t) {
	atomic_store_explicit(&g_debug_attach_transport, t, memory_order_release);
}

/*
 * 2026-06-06 JES #691 (P1 #1 fix, updated for concurrency P1): block until
 * all lazily-attached threads have unregistered, with a bounded timeout.
 * Called by protocol_main BEFORE freeing the heap transport and BEFORE calling
 * debug_set_attach_transport(NULL).
 *
 * Releases and reacquires the GIL on each 10ms sleep cycle so lazy threads can
 * acquire the GIL to finish their cleanup.
 *
 * Timeout design (5-second drain + 500ms grace):
 *   If a lazily-attached callScript thread is suspended at a breakpoint when
 *   the protocol client disconnects (without sending debug/continue), the thread
 *   will never self-resume -- it is waiting for a continue that will never arrive.
 *   Without a timeout the drain would block the process forever.
 *
 *   On timeout expiry:
 *     1. Walk g_debug_threads[] under g_debug_mutex; for each detached entry
 *        with a non-NULL debugstate, set flkill=true and clear flsuspended.
 *        This causes the thread to exit on its next callback cycle (it checks
 *        flkill after each statement). flkill is the same signal used by
 *        debug_kill_all_threads.
 *     2. Re-poll for up to 500ms of grace to let those threads exit.
 *     3. If counter still non-zero after grace: log_warn and proceed anyway.
 *        The worst case is a thread writes through a pointer to the
 *        already-freeing transport -- a bounded race on process exit, which is
 *        better than hanging the process indefinitely.
 *
 * Trade-off: wedged threads that are NOT at a breakpoint (e.g., blocked in a
 * system call outside the interpreter loop) will not respond to flkill within
 * the grace period, so the 500ms grace may expire without the counter dropping.
 * The log_warn + proceed path exists for this case. For the common case
 * (suspended at a breakpoint), flkill causes exit within one callback cycle.
 */
#define DRAIN_TIMEOUT_NS  (5LL * 1000000000LL)  /* 5 seconds */
#define DRAIN_GRACE_NS    (500LL * 1000000LL)    /* 500ms additional grace */

static long long _mono_ns(void) {
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (long long)ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

void debug_wait_lazy_threads_drained(void) {

	/* Fast path: no lazy threads were ever registered this session */
	if (atomic_load_explicit(&g_lazy_attached_count, memory_order_acquire) == 0)
		return;

	log_info(LOG_COMP_LANG, "debug: waiting for %d lazy-attached thread(s) to drain",
			 atomic_load(&g_lazy_attached_count));

	long long deadline = _mono_ns() + DRAIN_TIMEOUT_NS;

	while (atomic_load_explicit(&g_lazy_attached_count, memory_order_acquire) > 0) {
		if (_mono_ns() >= deadline)
			break;
		/* Release GIL so lazy threads can complete cleanup and decrement counter */
		pthread_mutex_unlock(&frontier_gil);
		struct timespec ts = {0, 10000000}; /* 10ms */
		nanosleep(&ts, NULL);
		pthread_mutex_lock(&frontier_gil);
	}

	/* If timed out, forcibly kill suspended lazy threads so they can exit */
	if (atomic_load_explicit(&g_lazy_attached_count, memory_order_acquire) > 0) {
		log_warn(LOG_COMP_LANG,
			"debug: drain timeout -- forcibly killing %d suspended lazy thread(s)",
			atomic_load(&g_lazy_attached_count));

		/* Walk the table under g_debug_mutex and signal all detached entries */
		pthread_mutex_lock(&g_debug_mutex);
		for (int i = 0; i < MAX_DEBUG_THREADS; i++) {
			tydebugstate *s = g_debug_threads[i];
			if (s != NULL && s->fldetached) {
				atomic_store_explicit(&s->flkill, true, memory_order_seq_cst);
				atomic_store_explicit(&s->flsuspended, false, memory_order_seq_cst);
			}
		}
		pthread_mutex_unlock(&g_debug_mutex);

		/* Grace period: re-poll while threads respond to flkill */
		long long grace_deadline = _mono_ns() + DRAIN_GRACE_NS;
		while (atomic_load_explicit(&g_lazy_attached_count, memory_order_acquire) > 0) {
			if (_mono_ns() >= grace_deadline)
				break;
			pthread_mutex_unlock(&frontier_gil);
			struct timespec ts = {0, 10000000}; /* 10ms */
			nanosleep(&ts, NULL);
			pthread_mutex_lock(&frontier_gil);
		}

		if (atomic_load_explicit(&g_lazy_attached_count, memory_order_acquire) > 0) {
			/* Wedged threads did not exit within the grace period. Proceed anyway:
			 * the process is exiting, and hanging indefinitely is worse than the
			 * bounded race of threads writing through a nearly-freed transport.
			 * This path is only reachable for threads blocked outside the
			 * interpreter callback loop (not at a breakpoint). */
			log_warn(LOG_COMP_LANG,
				"debug: %d lazy thread(s) did not drain after grace -- proceeding",
				atomic_load(&g_lazy_attached_count));
		}
	}

	log_info(LOG_COMP_LANG, "debug: lazy-attached thread drain complete");
}

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

typedef struct {
	char varname[DEBUG_VARNAME_MAX];	/* variable name to watch */
	char last_value[DEBUG_VALUE_MAX];	/* last known value (string repr) */
	boolean has_snapshot;				/* have we taken an initial snapshot? */
	boolean active;						/* is this slot in use? */
} debug_watchpoint_t;

static debug_watchpoint_t g_watchpoints[MAX_WATCHPOINTS] = {0};
static atomic_bool g_has_watchpoints = false; /* fast-path */

/* ========================================================================
 * Reason string conversion
 * ======================================================================== */

const char *debug_reason_string(debug_suspend_reason_t reason) {
	switch (reason) {
		case DEBUG_REASON_ENTRY:	   return "entry";
		case DEBUG_REASON_INTERRUPTED: return "interrupted";
		case DEBUG_REASON_BREAKPOINT:  return "breakpoint";
		case DEBUG_REASON_STEP:		   return "step";
		case DEBUG_REASON_WATCHPOINT:  return "watchpoint";
		case DEBUG_REASON_ERROR:	   return "error";
		default:					   return "unknown";
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

/* 2026-06-06 JES #691: Promoted from static; declared in debug_handler.h.
 * fldetached distinguishes lazy-attached (POSIX DETACHED, pthread_join is UB)
 * from normal debug/run threads (joinable). See debug_handler.h for full
 * contract. */
tydebugstate *debug_register_thread(long threadid, transport_t *transport,
                                    boolean fldetached) {

	tydebugstate *state = (tydebugstate *)calloc(1, sizeof(tydebugstate));
	if (state == NULL)
		return NULL;

	state->fldebugmode = true;
	state->fldetached = fldetached;
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
			/*
			 * 2026-06-06 JES #691 (P1 #1 + P1 #2 fix): track lazy-attached threads
			 * separately so protocol_main can wait for them before freeing the
			 * heap transport, and so kill/join can skip them (pthread_join is UB
			 * on DETACHED threads).
			 */
			if (fldetached)
				atomic_fetch_add(&g_lazy_attached_count, 1);
			pthread_mutex_unlock(&g_debug_mutex);
			return state;
		}
	}

	pthread_mutex_unlock(&g_debug_mutex);
	free(state);
	return NULL; /* no slots available */
}

/* 2026-06-06 JES #691: Promoted from static; declared in debug_handler.h */
void debug_unregister_thread(long threadid) {

	tydebugstate *state = NULL;
	boolean was_detached = false;

	pthread_mutex_lock(&g_debug_mutex);

	for (int i = 0; i < MAX_DEBUG_THREADS; i++) {
		if (g_debug_threads[i] != NULL && g_debug_threads[i]->threadid == threadid) {
			state = g_debug_threads[i];
			was_detached = state->fldetached;
			g_debug_threads[i] = NULL; /* remove from registry */
			break;
		}
	}

	/*
	 * 2026-06-06 JES #691 (P1 #1 fix): decrement lazy-attach counter under
	 * g_debug_mutex so protocol_main's drain loop sees a consistent count.
	 * This is the signal that the heap transport is no longer referenced by
	 * this thread -- safe to free once the counter reaches zero.
	 */
	if (was_detached)
		atomic_fetch_sub(&g_lazy_attached_count, 1);

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

			/*
			 * 2026-06-06 JES #691 (P1 #2 fix): lazily-attached callScript threads
			 * are POSIX DETACHED -- pthread_join on a detached thread is UB.
			 * They also never wrote pthread_id (it stays zero-initialized from
			 * calloc), so capturing it into g_killed_threads would store 0 and
			 * cause pthread_join(0, NULL) -- also UB.
			 *
			 * Skip detached entries from the kill-list capture. The flkill signal
			 * is still sent so the thread can exit on its next callback; draining
			 * is handled by g_lazy_attached_count in debug_wait_lazy_threads_drained.
			 */
			if (!g_debug_threads[i]->fldetached) {
				/* Capture pthread_t before the thread can unregister and free state */
				g_killed_threads[g_killed_thread_count++] = g_debug_threads[i]->pthread_id;
			}
			atomic_store_explicit(&g_debug_threads[i]->flkill, true, memory_order_seq_cst);
			atomic_store_explicit(&g_debug_threads[i]->flsuspended, false, memory_order_seq_cst); /* wake suspended threads */
		}
	}

	pthread_mutex_unlock(&g_debug_mutex);
}

void debug_join_all_threads(void) {

	/* Join threads captured by debug_kill_all_threads. Must be called
	 * with GIL released so threads can acquire it to finish cleanup.
	 *
	 * 2026-06-06 JES #691 (P1 #2 fix): g_killed_threads only contains
	 * pthread_t values from non-detached (joinable) threads -- detached
	 * entries were excluded in debug_kill_all_threads. pthread_join on a
	 * detached thread is UB; those threads drain via g_lazy_attached_count
	 * in debug_wait_lazy_threads_drained, not here. */
	for (int i = 0; i < g_killed_thread_count; i++) {
		pthread_join(g_killed_threads[i], NULL);
	}

	g_killed_thread_count = 0;
}

/* ========================================================================
 * Notifications
 * ======================================================================== */

/* Send a debug/suspended notification with a type-safe reason enum.
 * The enum is converted to a JSON-safe string via debug_reason_string().
 * script_path is optional (may be NULL). When non-NULL, a "script" field
 * is included so clients can identify the suspended script without a
 * separate debug/getStack call. */
void debug_send_suspended(transport_t *transport, long threadid, long line,
                          debug_suspend_reason_t reason, const char *script_path) {

	char json[640];
	if (script_path != NULL && script_path[0] != '\0') {
		snprintf(json, sizeof(json),
				 "{\"id\":null,\"op\":\"debug/suspended\",\"params\":"
				 "{\"threadId\":%ld,\"line\":%ld,\"reason\":\"%s\",\"script\":\"%s\"}}",
				 threadid, line, debug_reason_string(reason), script_path);
	} else {
		snprintf(json, sizeof(json),
				 "{\"id\":null,\"op\":\"debug/suspended\",\"params\":"
				 "{\"threadId\":%ld,\"line\":%ld,\"reason\":\"%s\"}}",
				 threadid, line, debug_reason_string(reason));
	}

	transport->write_line(transport->ctx, json, strlen(json));
}

/* 2026-06-06 JES #691: Promoted from static; declared in debug_handler.h */
void debug_send_completed(transport_t *transport, long threadid, boolean success) {

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

	/* 2026-06-06 JES #691: Update TLS script tracking BEFORE checking
	 * debug state. This allows the breakpoint callback to match scripts
	 * on threads with state == NULL (callScript-spawned threads). */

	/* Build full dotted path from table + name */
	bigstring bspath;
	hdlwindowinfo hroot = NULL;
	char new_script[DEBUG_SCRIPT_PATH_MAX];
	new_script[0] = '\0';

	if (langexternalgetfullpath(htable, bsname, bspath, &hroot)) {
		(void)hroot;
		int len = bspath[0];
		if (len >= DEBUG_SCRIPT_PATH_MAX)
			len = DEBUG_SCRIPT_PATH_MAX - 1;
		memcpy(new_script, bspath + 1, (size_t)len);
		new_script[len] = '\0';
	}

	/* Push TLS stack unconditionally (independent of debug state) */
	if (tls_script_depth < DEBUG_SCRIPT_STACK_MAX) {
		memcpy(tls_script_stack[tls_script_depth], tls_current_script, DEBUG_SCRIPT_PATH_MAX);
		tls_script_depth++;
	}
	/* If TLS stack overflows, silently drop the old value (rare deep call nesting);
	 * the worst outcome is a missed breakpoint match in the overflowed caller frame. */
	memcpy(tls_current_script, new_script, DEBUG_SCRIPT_PATH_MAX);

	/* --- Debug-state path (registered debug threads only) --- */
	tydebugstate *state = (tydebugstate *)((**hthreadglobals).debugstate);

	if (state == NULL || !state->fldebugmode)
		return true;

	/* Save current script path on the debug state stack before overwriting */
	if (state->script_stack_depth < DEBUG_SCRIPT_STACK_MAX) {
		memcpy(state->script_stack[state->script_stack_depth],
			   state->current_script, DEBUG_SCRIPT_PATH_MAX);
		state->script_stack_lines[state->script_stack_depth] = atomic_load(&state->lastlnum);
		state->script_stack_depth++;
	} else {
		/* Stack overflow — track the imbalance so pop skips the corresponding restore.
		 * Clear current_script to avoid false breakpoint matches: we can't save the
		 * caller's path, so it's safer to match nothing than to leave a stale path
		 * that persists into the caller after this frame returns. */
		state->script_stack_overflow++;
		state->current_script[0] = '\0';
	}

	/* Store resolved path in debug state */
	memcpy(state->current_script, new_script, DEBUG_SCRIPT_PATH_MAX);
	if (new_script[0] != '\0') {
		log_debug(LOG_COMP_LANG, "debug: push source '%s' for thread %ld", state->current_script, state->threadid);
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

	/* 2026-06-06 JES #691: Pop TLS stack BEFORE checking debug state,
	 * mirroring the push ordering for callScript-spawned threads. */
	if (tls_script_depth > 0) {
		tls_script_depth--;
		memcpy(tls_current_script, tls_script_stack[tls_script_depth], DEBUG_SCRIPT_PATH_MAX);
	} else {
		tls_current_script[0] = '\0';
	}

	/* --- Debug-state path (registered debug threads only) --- */
	tydebugstate *state = (tydebugstate *)((**hthreadglobals).debugstate);

	if (state == NULL || !state->fldebugmode)
		return true;

	/* Restore caller's script path from the debug state stack.
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

	/* Get debug state from thread globals. debugstate is wired by
	 * unified_thread_entry (headless_spawn.c) for debug threads; NULL for
	 * non-debug threads. The cast is safe as long as only debug_handler.c
	 * and headless_spawn.c write to tythreadglobals.debugstate. */
	if (hthreadglobals == nil)
		return true;

	tydebugstate *state = (tydebugstate *)((**hthreadglobals).debugstate);

	if (state == NULL || !state->fldebugmode) {
		/* 2026-06-06 JES #691: Lazy-attach path for callScript-spawned threads.
		 *
		 * callScript threads have state == NULL (intentional -- menu clicks must
		 * not pay per-click debug registration cost). When a protocol debug
		 * session is attached AND a breakpoint is set AND tls_current_script
		 * matches, lazily register this thread with the debug subsystem.
		 *
		 * Cheap path: three gated conditions before any expensive work.
		 *   1. g_debug_attach_transport: one acquire atomic load (NULL in all
		 *      non-debug-session execution -- effectively free).
		 *   2. g_has_breakpoints: one relaxed atomic load (fast-path guard).
		 *   3. tls_current_script: one null-check on TLS memory.
		 * Only when all three pass do we take g_debug_mutex for the match. */
		transport_t *attach_t = atomic_load_explicit(
				&g_debug_attach_transport, memory_order_acquire);

		if (attach_t == NULL
			|| !atomic_load_explicit(&g_has_breakpoints, memory_order_relaxed)
			|| tls_current_script[0] == '\0')
			return true; /* unchanged cheap path */

		/* Get the current line number for the match */
		unsigned long lazy_lnum = (hnode != nil) ? (**hnode).lnum : 0;
		if (lazy_lnum == 0)
			return true;

		/* Check if any breakpoint matches tls_current_script + lazy_lnum */
		boolean lazy_match = false;
		pthread_mutex_lock(&g_debug_mutex);
		for (int i = 0; i < MAX_BREAKPOINTS; i++) {
			if (g_breakpoints[i].active &&
				g_breakpoints[i].line == lazy_lnum &&
				strcasecmp(g_breakpoints[i].script, tls_current_script) == 0) {
				lazy_match = true;
				break;
			}
		}
		pthread_mutex_unlock(&g_debug_mutex);

		if (!lazy_match)
			return true;

		/* Lazy register: allocate a tydebugstate and hook it to the attach
		 * transport. debug_register_thread stores state in g_debug_threads.
		 *
		 * TLS reliability (P1 #3 resolved, 2026-06-06 JES #691): the lazy-attach
		 * check at the top of this function already filtered out tls_current_script
		 * being empty. The path that populates TLS (debug_push_sourcecode in
		 * langvalue.c) fires BEFORE langdebuggercall reaches this point for all
		 * user-visible scripts: langhandlercall calls langpushsourcecode at line 8382
		 * BEFORE evaluatelist runs the function body's first statement. Test 21
		 * (which passes) is the live proof -- a breakpoint on line 1 of a
		 * callScript-dispatched script fires correctly. No code change is needed.
		 *
		 * fldetached=true: callScript threads are POSIX DETACHED (see headless_spawn.c
		 * PTHREAD_CREATE_DETACHED); pthread_join on them is UB. The fldetached flag
		 * prevents debug_kill_all_threads from capturing their pthread_t (which is
		 * also 0 -- never written for lazy threads) and prevents
		 * debug_join_all_threads from calling pthread_join on them. */
		long idthread = (long)((**hthreadglobals).idthread);
		state = debug_register_thread(idthread, attach_t, true /* fldetached */);
		if (state == NULL) {
			log_warn(LOG_COMP_LANG,
					 "lazy debug attach: debug_register_thread failed "
					 "(thread=%ld); continuing without attach",
					 idthread);
			return true;
		}
		(**hthreadglobals).debugstate = (void *)state;
		/* Wire the TLS current_script into the debug state so the existing
		 * suspension-and-wait path (below) sees the right script name. */
		memcpy(state->current_script, tls_current_script, DEBUG_SCRIPT_PATH_MAX);
		log_debug(LOG_COMP_LANG,
				  "lazy debug attach: registered thread %ld at %s line %ld",
				  idthread, tls_current_script, lazy_lnum);
		/* Fall through to the existing suspension-and-wait path */
	}

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

		debug_send_suspended(state->transport, state->threadid, (long)lnum,
							 DEBUG_REASON_INTERRUPTED, state->current_script);
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
	/* Skip breakpoint re-trigger on the same line we just resumed from.
	 * After any suspension, flskipaliasline is set. While true, breakpoints
	 * at lastlnum are skipped (multiple AST nodes per source line). Once the
	 * line number changes (next source line), the flag is cleared and
	 * breakpoints fire normally — including if a loop returns to lastlnum. */
	boolean flskipbreakpoint = false;
	if (state->flskipaliasline && lnum > 0) {
		if (lnum == atomic_load(&state->lastlnum)) {
			flskipbreakpoint = true;
		} else {
			state->flskipaliasline = false; /* line changed — re-enable breakpoints */
		}
	}

	if (!flskipbreakpoint && atomic_load_explicit(&g_has_breakpoints, memory_order_relaxed) &&
		lnum > 0 && !atomic_load(&state->flsuspended) && state->current_script[0] != '\0') {

		boolean flbreakpoint = false;

		pthread_mutex_lock(&g_debug_mutex);

		/* Find matching breakpoint and copy its condition (if any) */
		char bp_condition[DEBUG_VALUE_MAX] = {0};

		for (int i = 0; i < MAX_BREAKPOINTS; i++) {
			if (g_breakpoints[i].active &&
				g_breakpoints[i].line == lnum &&
				strcasecmp(g_breakpoints[i].script, state->current_script) == 0) {
				flbreakpoint = true;
				memcpy(bp_condition, g_breakpoints[i].condition, DEBUG_VALUE_MAX);
				break;
			}
		}

		pthread_mutex_unlock(&g_debug_mutex);

		/* Evaluate condition if present (Phase 7).
		 * Simple format: "varname op value" where op is ==, !=, >, <, >=, <=.
		 * Compares string representation of the variable against expected value.
		 * Numeric comparison used when both sides parse as numbers. */
		if (flbreakpoint && bp_condition[0] != '\0') {
			boolean cond_met = false;
			boolean cond_evaluated = false; /* did we actually evaluate the condition? */
			char bp_condition_orig[DEBUG_VALUE_MAX]; /* preserve for logging before parse mutates */
			memcpy(bp_condition_orig, bp_condition, DEBUG_VALUE_MAX);

			/* Parse: find operator (check two-char ops before one-char) */
			char *op_pos = NULL;
			int op_len = 0;
			enum { OP_EQ, OP_NE, OP_GE, OP_LE, OP_GT, OP_LT } op_type = OP_EQ;

			if ((op_pos = strstr(bp_condition, "==")) != NULL) { op_len = 2; op_type = OP_EQ; }
			else if ((op_pos = strstr(bp_condition, "!=")) != NULL) { op_len = 2; op_type = OP_NE; }
			else if ((op_pos = strstr(bp_condition, ">=")) != NULL) { op_len = 2; op_type = OP_GE; }
			else if ((op_pos = strstr(bp_condition, "<=")) != NULL) { op_len = 2; op_type = OP_LE; }
			else if ((op_pos = strstr(bp_condition, ">")) != NULL) { op_len = 1; op_type = OP_GT; }
			else if ((op_pos = strstr(bp_condition, "<")) != NULL) { op_len = 1; op_type = OP_LT; }

			if (op_pos != NULL) {
				*op_pos = '\0';
				char *varname = bp_condition;
				char *expected = op_pos + op_len;

				/* Trim whitespace (guard against op at position 0) */
				while (*varname == ' ') varname++;
				if (op_pos > bp_condition) {
					char *vend = op_pos - 1;
					while (vend > varname && *vend == ' ') *vend-- = '\0';
				}
				while (*expected == ' ') expected++;
				size_t explen = strlen(expected);
				if (explen > 0) {
					char *eend = expected + explen - 1;
					while (eend > expected && *eend == ' ') *eend-- = '\0';
				}

				/* Strip quotes from expected value */
				size_t elen = strlen(expected);
				if (elen >= 2 && expected[0] == '"' && expected[elen-1] == '"') {
					expected[elen-1] = '\0';
					expected++;
				}

				/* Look up variable — walk the full hash table chain (locals,
				 * enclosing scopes, globals) not just innermost local. */
				bigstring bsname;
				int nlen = (int)strlen(varname);
				if (nlen > 255) nlen = 255;
				bsname[0] = (unsigned char)nlen;
				memcpy(bsname + 1, varname, (size_t)nlen);

				tyvaluerecord cond_val;
				hdlhashnode hn_cond = nil;
				boolean found_var = false;

				hdlhashtable hwalk_cond = currenthashtable;
				while (hwalk_cond != nil) {
					if (hashtablelookup(hwalk_cond, bsname, &cond_val, &hn_cond)) {
						found_var = true;
						break;
					}
					hwalk_cond = (**hwalk_cond).prevhashtable;
				}

				if (!found_var) {
					log_warn(LOG_COMP_LANG, "debug: condition variable '%s' not found at line %ld",
							 varname, (long)lnum);
				} else {
					bigstring bsval;
					if (hashgetvaluestring(cond_val, bsval)) {
						char actual[DEBUG_VALUE_MAX];
						int avlen = bsval[0];
						if (avlen >= DEBUG_VALUE_MAX) avlen = DEBUG_VALUE_MAX - 1;
						memcpy(actual, bsval + 1, (size_t)avlen);
						actual[avlen] = '\0';

						cond_evaluated = true;

						/* Try numeric comparison first */
						char *endp1, *endp2;
						double da = strtod(actual, &endp1);
						double de = strtod(expected, &endp2);
						if (*endp1 == '\0' && *endp2 == '\0') {
							/* Exact floating-point comparison — appropriate for
							 * integer values stored as doubles (the common case).
							 * Use string comparison for epsilon-sensitive floats. */
							switch (op_type) {
								case OP_EQ: cond_met = (da == de); break;
								case OP_NE: cond_met = (da != de); break;
								case OP_GE: cond_met = (da >= de); break;
								case OP_LE: cond_met = (da <= de); break;
								case OP_GT: cond_met = (da > de); break;
								case OP_LT: cond_met = (da < de); break;
							}
						} else {
							int cmp = strcmp(actual, expected);
							switch (op_type) {
								case OP_EQ: cond_met = (cmp == 0); break;
								case OP_NE: cond_met = (cmp != 0); break;
								case OP_GE: cond_met = (cmp >= 0); break;
								case OP_LE: cond_met = (cmp <= 0); break;
								case OP_GT: cond_met = (cmp > 0); break;
								case OP_LT: cond_met = (cmp < 0); break;
							}
						}
					}
				}
			} else {
				log_warn(LOG_COMP_LANG, "debug: unparseable condition '%s' at line %ld (expected: varname op value)",
						 bp_condition_orig, (long)lnum);
			}

			if (!cond_met) {
				if (cond_evaluated)
					log_debug(LOG_COMP_LANG, "debug: conditional breakpoint at line %ld skipped (condition '%s' evaluated to false)",
							  (long)lnum, bp_condition_orig);
				flbreakpoint = false;
			}
		}

		if (flbreakpoint) {
			atomic_store(&state->lastlnum, lnum);

			/* Clear stepping state if we were stepping — breakpoint takes priority */
			atomic_store(&state->flstepping, false);
			atomic_store(&state->stepdir, DEBUG_STEP_NONE);

			atomic_store_explicit(&state->flsuspended, true, memory_order_seq_cst);
			debug_send_suspended(state->transport, state->threadid, (long)lnum,
								 DEBUG_REASON_BREAKPOINT, state->current_script);
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

		/* Use currenthashtable (correct under GIL) as starting point.
		 * Walk the full chain (locals, enclosing scopes, globals) for
		 * each watched variable — not just the innermost local table. */
		if (currenthashtable != nil) {
			pthread_mutex_lock(&g_debug_mutex);

			for (int w = 0; w < MAX_WATCHPOINTS; w++) {
				if (!g_watchpoints[w].active)
					continue;

				/* Look up the variable by name — walk full scope chain */
				bigstring bsname;
				int nlen = (int)strlen(g_watchpoints[w].varname);
				if (nlen > 255) nlen = 255;
				bsname[0] = (unsigned char)nlen;
				memcpy(bsname + 1, g_watchpoints[w].varname, (size_t)nlen);

				tyvaluerecord val;
				hdlhashnode hn = nil;
				boolean found_wp_var = false;
				hdlhashtable hwalk = currenthashtable;
				while (hwalk != nil) {
					if (hashtablelookup(hwalk, bsname, &val, &hn)) {
						found_wp_var = true;
						break;
					}
					hwalk = (**hwalk).prevhashtable;
				}
				if (!found_wp_var)
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
				if (bsval[0] >= 255) {
					/* Value was likely truncated by bigstring limit */
					if (vlen >= 4) {
						cval[vlen-3] = '.'; cval[vlen-2] = '.'; cval[vlen-1] = '.';
					}
				}

				if (!g_watchpoints[w].has_snapshot) {
					/* First encounter — save snapshot, don't trigger */
					memcpy(g_watchpoints[w].last_value, cval, (size_t)(vlen + 1));
					g_watchpoints[w].has_snapshot = true;
					continue;
				}

				/* Compare with last known value */
				if (strcmp(g_watchpoints[w].last_value, cval) != 0) {
					/* Value changed! Copy all needed data before releasing mutex */
					char old_value[DEBUG_VALUE_MAX];
					char fired_varname[DEBUG_VARNAME_MAX];
					memcpy(old_value, g_watchpoints[w].last_value, DEBUG_VALUE_MAX);
					memcpy(fired_varname, g_watchpoints[w].varname, DEBUG_VARNAME_MAX);
					memcpy(g_watchpoints[w].last_value, cval, (size_t)(vlen + 1));

					pthread_mutex_unlock(&g_debug_mutex);

					/* Clear stepping state — watchpoint takes priority */
					atomic_store(&state->flstepping, false);
					atomic_store(&state->stepdir, DEBUG_STEP_NONE);
					atomic_store(&state->lastlnum, lnum);

					/* Send watchpoint notification with old/new values */
					cJSON *notif = cJSON_CreateObject();
					if (notif == NULL) {
						atomic_store_explicit(&state->flsuspended, true, memory_order_seq_cst);
						log_warn(LOG_COMP_LANG, "debug: cJSON_CreateObject failed (OOM) for watchpoint notification");
						goto after_stepping;
					}
					cJSON_AddNullToObject(notif, "id");
					cJSON_AddStringToObject(notif, "op", "debug/suspended");
					cJSON *wp_params = cJSON_CreateObject();
					cJSON_AddNumberToObject(wp_params, "threadId", (double)state->threadid);
					cJSON_AddNumberToObject(wp_params, "line", (double)lnum);
					cJSON_AddStringToObject(wp_params, "reason", "watchpoint");
					cJSON_AddStringToObject(wp_params, "variable", fired_varname);
					cJSON_AddStringToObject(wp_params, "oldValue", old_value);
					cJSON_AddStringToObject(wp_params, "newValue", cval);
					cJSON_AddItemToObject(notif, "params", wp_params);

					/* Always suspend — even if notification serialization fails */
					atomic_store_explicit(&state->flsuspended, true, memory_order_seq_cst);

					char *json_str = cJSON_PrintUnformatted(notif);
					if (json_str) {
						state->transport->write_line(state->transport->ctx, json_str, strlen(json_str));
						free(json_str);
					} else {
						log_warn(LOG_COMP_LANG, "debug: cJSON_PrintUnformatted failed (OOM) for watchpoint notification");
					}
					cJSON_Delete(notif);

					log_debug(LOG_COMP_LANG, "debug: thread %ld watchpoint '%s' changed: '%s' -> '%s' at line %ld",
							  state->threadid, fired_varname, old_value, cval, (long)lnum);

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
			debug_send_suspended(state->transport, state->threadid, (long)lnum,
								 DEBUG_REASON_STEP, state->current_script);
			log_debug(LOG_COMP_LANG, "debug: thread %ld step completed at line %ld", state->threadid, (long)lnum);
		}
	}

after_stepping: ; /* label for watchpoint goto — skips stepping when watchpoint fires */

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
	 * langruncode returns false, unified_thread_entry (headless_spawn.c) sends
	 * debug/completed with success=false, then cleans up. The callback does NOT
	 * send debug/completed -- that's always the thread entry's responsibility. */
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
 * Protocol operation handlers
 *
 * 2026-06-06 JES #691: debug_thread_params struct and debug_thread_entry
 * function removed; replaced by unified_thread_entry in headless_spawn.c.
 * handle_debug_run now calls headless_spawn_script_thread.
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
		} else {
			log_warn(LOG_COMP_LANG, "debug: cJSON_PrintUnformatted failed (OOM)");
		}
		cJSON_Delete(resp);
		cJSON_Delete(root);
		return;
	}

	/* 2026-06-06 JES #691: Spawn boilerplate replaced by headless_spawn_script_thread.
	 * The common alloc-globals / wire-idthread / alloc-tablestack / pthread_create
	 * sequence is now in headless_spawn.c. debug_register_thread is called inside
	 * headless_spawn_script_thread (before pthread_create) using the allocated
	 * thread's own ID, so the debug slot is populated before the thread can run.
	 * headless_spawn_script_thread returns the tydebugstate* via out_debugstate. */

	thread_run_spec run_spec;
	memset(&run_spec, 0, sizeof(run_spec));
	run_spec.is_callscript = false;

	tydebugstate *debugstate = NULL;
	thread_debug_opts debug_opts;
	debug_opts.transport = transport;
	debug_opts.out_debugstate = (void **)&debugstate;
	debug_opts.start_suspended = true;

	long user_threadid = 0;

	if (!headless_spawn_script_thread(hcode, &run_spec, &debug_opts, &user_threadid)) {
		/* spawn failed -- all resources freed inside the primitive,
		 * including debug registration (debugstate is NULL). */
		char err[512];
		snprintf(err, sizeof(err), "{\"id\":%d,\"error\":{\"message\":\"Failed to spawn debug thread\"},\"success\":false}", id);
		transport->write_line(transport->ctx, err, strlen(err));
		cJSON_Delete(root);
		return;
	}

	/* pthread_id was set in headless_spawn_script_thread (under GIL before
	 * any context switch) and is ready for debug_kill_all_threads. No
	 * additional setup needed here. */

	/* Return immediately with thread ID */
	char resp[128];
	snprintf(resp, sizeof(resp),
			 "{\"id\":%d,\"result\":{\"threadId\":%ld,\"status\":\"started\"},\"success\":true}",
			 id, user_threadid);
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
	state->flskipaliasline = true; /* skip re-trigger at same line on resume */

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
	state->flskipaliasline = true; /* skip re-trigger at same line on resume */

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
 *	 script: dotted path (e.g. "mainResponder.respond") — no leading "@"
 *	 line:	 1-based line number
 *
 * Returns:
 *	 action: "set" or "cleared"
 *	 script, line: echo back the breakpoint location
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
	cJSON *cond_json = params ? cJSON_GetObjectItemCaseSensitive(params, "condition") : NULL;

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
				g_breakpoints[i].condition[0] = '\0'; /* default: unconditional */
				if (cJSON_IsString(cond_json) && cond_json->valuestring != NULL) {
					size_t clen = strlen(cond_json->valuestring);
					if (clen >= DEBUG_VALUE_MAX) clen = DEBUG_VALUE_MAX - 1;
					memcpy(g_breakpoints[i].condition, cond_json->valuestring, clen);
					g_breakpoints[i].condition[clen] = '\0';
				}
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
	if (!cleared && cJSON_IsString(cond_json) && cond_json->valuestring != NULL)
		cJSON_AddStringToObject(result, "condition", cond_json->valuestring);
	cJSON_AddItemToObject(resp, "result", result);
	cJSON_AddBoolToObject(resp, "success", 1);

	char *json_str = cJSON_PrintUnformatted(resp);
	if (json_str) {
		transport->write_line(transport->ctx, json_str, strlen(json_str));
		free(json_str);
	} else {
		log_warn(LOG_COMP_LANG, "debug: cJSON_PrintUnformatted failed (OOM)");
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
	if (resp == NULL) {
		char err[128];
		snprintf(err, sizeof(err), "{\"id\":%d,\"error\":{\"message\":\"Memory allocation failed\"},\"success\":false}", id);
		transport->write_line(transport->ctx, err, strlen(err));
		return;
	}
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
			if (g_breakpoints[i].condition[0] != '\0')
				cJSON_AddStringToObject(bp, "condition", g_breakpoints[i].condition);
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
	} else {
		log_warn(LOG_COMP_LANG, "debug: cJSON_PrintUnformatted failed (OOM)");
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

	cJSON *resp_bp = cJSON_CreateObject();
	if (resp_bp == NULL) {
		char err[128];
		snprintf(err, sizeof(err), "{\"id\":%d,\"error\":{\"message\":\"Memory allocation failed\"},\"success\":false}", id);
		transport->write_line(transport->ctx, err, strlen(err));
		return;
	}
	cJSON_AddNumberToObject(resp_bp, "id", id);
	cJSON *result_bp = cJSON_CreateObject();
	cJSON_AddNumberToObject(result_bp, "cleared", cleared);
	cJSON_AddItemToObject(resp_bp, "result", result_bp);
	cJSON_AddBoolToObject(resp_bp, "success", 1);
	char *json_str = cJSON_PrintUnformatted(resp_bp);
	if (json_str) {
		transport->write_line(transport->ctx, json_str, strlen(json_str));
		free(json_str);
	} else {
		log_warn(LOG_COMP_LANG, "debug: cJSON_PrintUnformatted failed (OOM)");
	}
	cJSON_Delete(resp_bp);

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
	if (resp == NULL) {
		char err[128];
		snprintf(err, sizeof(err), "{\"id\":%d,\"error\":{\"message\":\"Memory allocation failed\"},\"success\":false}", id);
		transport->write_line(transport->ctx, err, strlen(err));
		debug_release_state(state);
		cJSON_Delete(root);
		return;
	}
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
					if (bsval[0] >= 255) {
						/* Value was likely truncated by bigstring limit */
						if (vlen >= 4) {
							cval[vlen-3] = '.'; cval[vlen-2] = '.'; cval[vlen-1] = '.';
						}
					}
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
	} else {
		log_warn(LOG_COMP_LANG, "debug: cJSON_PrintUnformatted failed (OOM)");
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
	if (resp == NULL) {
		char err[128];
		snprintf(err, sizeof(err), "{\"id\":%d,\"error\":{\"message\":\"Memory allocation failed\"},\"success\":false}", id);
		transport->write_line(transport->ctx, err, strlen(err));
		debug_release_state(state);
		cJSON_Delete(root);
		return;
	}
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
			if (state->script_stack_lines[i] > 0)
				cJSON_AddNumberToObject(frame, "line", (double)state->script_stack_lines[i]);
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
	} else {
		log_warn(LOG_COMP_LANG, "debug: cJSON_PrintUnformatted failed (OOM)");
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

		/* Load from database if not yet in memory.
		 * Use format-aware context to handle both v6 and v7 databases. */
		if (!(**hv).flinmemory) {
			db_context ctx;
			if ((**hv).hdatabase != nil && db_format_is_legacy_db((**hv).hdatabase)) {
				db_context_init_legacy_read(&ctx, (**hv).hdatabase);
			} else {
				db_context_init(&ctx);
				if ((**hv).hdatabase != nil)
					ctx.database = (**hv).hdatabase;
			}
			if (!opverbinmemory(&ctx, hv)) {
				char err[512];
				snprintf(err, sizeof(err),
						 "{\"id\":%d,\"error\":{\"message\":\"Failed to load script from database\"},\"success\":false}", id);
				transport->write_line(transport->ctx, err, strlen(err));
				cJSON_Delete(root);
				return;
			}
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
	if (resp == NULL) {
		char err[128];
		snprintf(err, sizeof(err), "{\"id\":%d,\"error\":{\"message\":\"Memory allocation failed\"},\"success\":false}", id);
		transport->write_line(transport->ctx, err, strlen(err));
		disposehandle(htext);
		cJSON_Delete(root);
		return;
	}
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
	} else {
		log_warn(LOG_COMP_LANG, "debug: cJSON_PrintUnformatted failed (OOM)");
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
	if (resp == NULL) {
		char err[128];
		snprintf(err, sizeof(err), "{\"id\":%d,\"error\":{\"message\":\"Memory allocation failed\"},\"success\":false}", id);
		transport->write_line(transport->ctx, err, strlen(err));
		return;
	}
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
	} else {
		log_warn(LOG_COMP_LANG, "debug: cJSON_PrintUnformatted failed (OOM)");
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
 *	 variable: name of the variable to watch (e.g. "x", "msg")
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
	} else {
		log_warn(LOG_COMP_LANG, "debug: cJSON_PrintUnformatted failed (OOM)");
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
	if (resp == NULL) {
		char err[128];
		snprintf(err, sizeof(err), "{\"id\":%d,\"error\":{\"message\":\"Memory allocation failed\"},\"success\":false}", id);
		transport->write_line(transport->ctx, err, strlen(err));
		return;
	}
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
	} else {
		log_warn(LOG_COMP_LANG, "debug: cJSON_PrintUnformatted failed (OOM)");
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

	cJSON *resp_wp = cJSON_CreateObject();
	if (resp_wp == NULL) {
		char err[128];
		snprintf(err, sizeof(err), "{\"id\":%d,\"error\":{\"message\":\"Memory allocation failed\"},\"success\":false}", id);
		transport->write_line(transport->ctx, err, strlen(err));
		return;
	}
	cJSON_AddNumberToObject(resp_wp, "id", id);
	cJSON *result_wp = cJSON_CreateObject();
	cJSON_AddNumberToObject(result_wp, "cleared", cleared_count);
	cJSON_AddItemToObject(resp_wp, "result", result_wp);
	cJSON_AddBoolToObject(resp_wp, "success", 1);
	char *json_str = cJSON_PrintUnformatted(resp_wp);
	if (json_str) {
		transport->write_line(transport->ctx, json_str, strlen(json_str));
		free(json_str);
	} else {
		log_warn(LOG_COMP_LANG, "debug: cJSON_PrintUnformatted failed (OOM)");
	}
	cJSON_Delete(resp_wp);

	log_info(LOG_COMP_LANG, "debug: cleared %d watchpoints", cleared_count);
}
