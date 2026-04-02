/*
 * debug_handler.h - Protocol-based UserTalk debugger
 *
 * Provides debug/* protocol operations for headless script debugging:
 *   debug/run        — Run script in debug mode (non-blocking, spawns thread)
 *   debug/continue   — Resume suspended thread
 *   debug/kill       — Kill a debug thread
 *   debug/pause      — Interrupt a running thread
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
    DEBUG_REASON_ENTRY,        /* suspended at entry before first statement */
    DEBUG_REASON_INTERRUPTED,  /* suspended via debug/pause */
    DEBUG_REASON_BREAKPOINT,   /* suspended at breakpoint (Phase 3) */
    DEBUG_REASON_STEP,         /* suspended after step (Phase 2) */
    DEBUG_REASON_ERROR         /* suspended on error (future) */
} debug_suspend_reason_t;

/* Returns the JSON-safe string for a reason enum value */
const char *debug_reason_string(debug_suspend_reason_t reason);

/*
 * Per-thread debug state. Stored in tythreadglobals.param_reserved[0].
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
typedef struct tydebugstate {
    boolean fldebugmode;             /* not atomic: set once at registration (under GIL) before
                                      * thread starts, only read after. GIL provides ordering. */
    atomic_bool flsuspended;         /* is this thread paused? */
    atomic_bool flinterrupt;         /* pause at next statement (debug/pause) */
    atomic_bool flkill;              /* kill the script */
    atomic_int refcount;             /* reference count (1 = debug thread only) */
    transport_t *transport;          /* for sending notifications back to client */
    long threadid;                   /* this thread's ID */
    pthread_t pthread_id;            /* POSIX thread ID for pthread_join */
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
void handle_debug_continue(int id, const char *json_line, transport_t *transport);
void handle_debug_kill(int id, const char *json_line, transport_t *transport);
void handle_debug_pause(int id, const char *json_line, transport_t *transport);

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
 * Send an unsolicited debug/suspended notification to the client.
 */
void debug_send_suspended(transport_t *transport, long threadid, long line, debug_suspend_reason_t reason);

#endif /* DEBUG_HANDLER_H */
