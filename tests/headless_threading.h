/*
 * headless_threading.h - Shared declarations for headless threading infrastructure
 *
 * Exports GIL, thread globals management, and thread registration functions
 * from headless_thread_verbs.c for use by other modules (debug_handler.c, etc.).
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
