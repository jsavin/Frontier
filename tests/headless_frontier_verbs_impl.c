/*
 * headless_frontier_verbs_impl.c - Frontier verb implementations for headless mode
 *
 * This provides implementations for frontier.* verbs that work in headless mode.
 * These are called from the generated kernel_verbs_init.c via processor callbacks.
 *
 * Token IDs are sequential (0, 1, 2...) as defined in kernelverbs.rc and match
 * the order in generated/kernel_verbs_init.c init_efp_1023 function.
 */

#include "frontier.h"
#include "standard.h"
#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "file.h"

/* Global to track the currently open database file */
static tyfilespec headless_frontier_filespec;
static boolean headless_frontier_filespec_valid = false;

/* Called by main() after opening database */
boolean headless_set_frontier_file(tyfilespec *fs) {
	if (fs == NULL)
		return false;

	headless_frontier_filespec = *fs;
	headless_frontier_filespec_valid = true;
	return true;
}

/*
 * Frontier verb token IDs (from kernelverbs.rc frontier processor, EFP 1023)
 * These MUST match the order in kernel_verbs_init.c
 */
enum {
	frv_getProgramPath = 0,      /* frontier.getProgramPath */
	frv_getFilePath = 1          /* frontier.getFilePath */
	/* Tokens 2-13: other frontier verbs not yet implemented */
};

boolean headless_frontier_verbs_callback(short token, hdltreenode hparam1,
                                          tyvaluerecord *vreturned, bigstring bserror) {
	tyvaluerecord *v = vreturned;

	#pragma unused(hparam1)

	switch (token) {
		case frv_getFilePath: {
			/* Return filespec of currently open database */
			if (!headless_frontier_filespec_valid) {
				copystring(BIGSTRING("\pNo database file is open"), bserror);
				return false;
			}
			return setfilespecvalue(&headless_frontier_filespec, v);
		}

		case frv_getProgramPath: {
			/* Not yet implemented */
			copystring(BIGSTRING("\pfrontier.getProgramPath not yet implemented"), bserror);
			return false;
		}

		default:
			/* Other frontier verbs not yet implemented in headless */
			copystring(BIGSTRING("\pfrontier verb not implemented in headless mode"), bserror);
			return false;
	}
}
