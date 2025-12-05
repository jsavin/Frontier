/*
 * headless_file_verbs_impl.c - File verb implementations for headless mode
 *
 * This provides implementations for file.* verbs that work in headless mode.
 * These are called from the generated kernel_verbs_init.c via processor callbacks.
 *
 * Token IDs are sequential (0, 1, 2...) as defined in kernelverbs.rc and match
 * the order in generated/kernel_verbs_init.c init_efp_1007 function.
 */

#include "frontier.h"
#include "standard.h"
#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "file.h"
#include "file_portable.h"

/*
 * File verb token IDs (from kernelverbs.rc file processor, EFP 1007)
 * These MUST match the order in kernel_verbs_init.c
 */
enum {
	/* Tokens 0-22: other file verbs not yet implemented */
	fv_fileFromPath = 23,      /* file.fileFromPath */
	fv_folderFromPath = 24     /* file.folderFromPath */
};

boolean headless_file_verbs_callback(short token, hdltreenode hparam1,
                                      tyvaluerecord *vreturned, bigstring bserror) {
	tyvaluerecord *v = vreturned;

	#pragma unused(bserror)

	switch (token) {
		case fv_folderFromPath: {
			bigstring bspath, bsfolder;

			flnextparamislast = true;
			if (!getstringvalue(hparam1, 1, bspath))
				return false;

			if (!portable_folderfrompath(bspath, bsfolder))
				return false;

			return setstringvalue(bsfolder, v);
		}

		case fv_fileFromPath: {
			bigstring bspath, bsfile;

			flnextparamislast = true;
			if (!getstringvalue(hparam1, 1, bspath))
				return false;

			if (!portable_filefrompath(bspath, bsfile))
				return false;

			return setstringvalue(bsfile, v);
		}

		default:
			/* Other file verbs not yet implemented in headless */
			return false;
	}
}
