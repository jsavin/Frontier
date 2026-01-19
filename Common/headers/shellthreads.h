/*
 * shellthreads.h - Thread verb API declarations
 *
 * Exposes thread management functions for use by headless verb stubs.
 * These are public wrappers around the internal thread management code.
 */

#ifndef __SHELLTHREADS__
#define __SHELLTHREADS__

#ifndef __STANDARD__
	#include "standard.h"
#endif

#ifndef __LANG__
	#include "lang.h"
#endif


/*
 * thread_callscript_kernel - Public wrapper for thread.callScript kernel verb
 *
 * Creates a new thread and executes the specified script with parameters.
 *
 * Parameters:
 *   bsscriptname - Name of the script/function to execute
 *   vparams      - Parameters to pass to the script (can be list, table, or other value)
 *   hcontext     - Optional context table (nil for no context)
 *   vreturned    - Return value (boolean indicating success)
 *
 * Returns: true if thread was created successfully, false otherwise
 */
boolean thread_callscript_kernel(bigstring bsscriptname,
                                  tyvaluerecord vparams,
                                  hdlhashtable hcontext,
                                  tyvaluerecord *vreturned);

/*
 * thread_evaluate_kernel - Public wrapper for thread.evaluate kernel verb
 *
 * Evaluates an expression string in a new thread.
 *
 * Parameters:
 *   bscode      - UserTalk code to evaluate as a string
 *   vreturned   - Return value (thread ID or error)
 *
 * Returns: true if thread was created successfully, false otherwise
 */
boolean thread_evaluate_kernel(bigstring bscode, tyvaluerecord *vreturned);

#endif /* __SHELLTHREADS__ */
