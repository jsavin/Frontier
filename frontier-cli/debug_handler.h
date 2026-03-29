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

#include "op_handler.h"
#include "../Common/headers/frontier.h"
#include "../Common/headers/lang.h"

/*
 * Per-thread debug state. Stored in tythreadglobals.param_reserved[0].
 * Allocated when a thread enters debug mode, freed on thread exit.
 */
typedef struct tydebugstate {
    boolean fldebugmode;         /* is this thread in debug mode? */
    boolean flsuspended;         /* is this thread paused? */
    boolean flinterrupt;         /* pause at next statement (debug/pause) */
    boolean flkill;              /* kill the script */
    transport_t *transport;      /* for sending notifications back to client */
    long threadid;               /* this thread's ID */
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
 * Send an unsolicited debug/suspended notification to the client.
 */
void debug_send_suspended(transport_t *transport, long threadid, long line, const char *reason);

#endif /* DEBUG_HANDLER_H */
