/*
 * headless_threading.h - Shared declarations for headless threading infrastructure
 *
 * Exports GIL, thread globals management, and thread registration functions
 * from headless_thread_verbs.c for use by other modules (debug_handler.c,
 * protocol_handler.c, main.c).
 *
 * These were originally static in headless_thread_verbs.c but were promoted
 * to support the protocol-based debugger, which needs to:
 *   - Acquire/release the GIL around I/O waits
 *   - Save/restore thread globals across GIL yields
 *   - Spawn and register debug threads
 *   - Unregister threads on completion
 *
 * Callers MUST hold the GIL before calling any function that accesses
 * C globals (headless_restore_threadglobals, headless_register_thread, etc.).
 * The GIL is a cooperative lock — yield via unlock/sleep/lock sequences
 * to let other threads run.
 *
 * Note: headless_thread_verbs.c lives in frontier-cli/ (moved from tests/
 * in #502) as it is production code compiled into the frontier-cli binary.
 */

#ifndef HEADLESS_THREADING_H
#define HEADLESS_THREADING_H

#include <pthread.h>
#include "../Common/headers/frontier.h"
#include "../Common/headers/processinternal.h"
#include "../Common/headers/strings.h"

/* Global Interpreter Lock — only the holder may access C globals */
extern pthread_mutex_t frontier_gil;
extern pthread_cond_t gil_available;

/*
 * Yield synchronization for headless_backgroundtask().
 *
 * When the main thread yields the GIL, it sleeps on headless_yield_cond for
 * a short timeout (1ms) so spawned threads get a chance to run before the
 * main thread reacquires. Spawned threads signal headless_yield_cond on exit
 * to wake the background task early.
 *
 * 2026-06-06 JES #691: Exported from headless_thread_verbs.c (was static)
 * so that unified_thread_entry in headless_spawn.c can signal it on exit.
 */
extern pthread_mutex_t headless_yield_mutex;
extern pthread_cond_t headless_yield_cond;

/* Thread globals management */
extern hdlthreadglobals headless_new_threadglobals(void);
extern void headless_dispose_threadglobals(hdlthreadglobals hg);
extern void headless_save_threadglobals(hdlthreadglobals hg);
extern void headless_restore_threadglobals(hdlthreadglobals hg);

/* Thread registration in system.compiler.threads */
extern boolean headless_register_thread(bigstring bsname, long idthread);
extern boolean headless_unregister_thread(long idthread);

/* Error state cleanup */
extern void headless_clear_last_lang_error(void);

#endif /* HEADLESS_THREADING_H */
