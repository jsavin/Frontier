/* headless_frontier_verbs.c - Frontier processor verbs for headless mode */

#include "frontier.h"
#include "standard.h"
#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "langinternal.h"
#include "tablestructure.h"
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

/* Token definitions for frontier processor */
enum {
	frv_getProgramPath = 0,      /* token 0 */
	frv_getFilePath = 1,         /* token 1 - CRITICAL */
	frv_enableAgents = 2,        /* token 2 */
	frv_requestToFront = 3,      /* token 3 */
	frv_isRuntime = 4,           /* token 4 */
	frv_countThreads = 5,        /* token 5 */
	frv_isNative = 6,            /* token 6 (originally isPowerPC) */
	frv_reclaimMemory = 7,       /* token 7 */
	frv_version = 8,             /* token 8 */
	frv_hashStats = 9,           /* token 9 */
	frv_getHashLoopCount = 10,   /* token 10 */
	frv_hideApplication = 11,    /* token 11 */
	frv_isValidSerialNumber = 12,/* token 12 */
	frv_showApplication = 13     /* token 13 */
};

static boolean frontier_valueproc(short token, hdltreenode hparam1,
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
			/* Return filespec of frontier-cli executable */
			/* TODO: Implement by storing executable path at startup */
			langerrormessage(BIGSTRING("\pfrontier.getProgramPath not yet implemented"));
			return false;
		}

		case frv_version: {
			/* Return Frontier version string */
			bigstring bsversion;
			copystring(BIGSTRING("\p11.0.0-headless"), bsversion);
			return setstringvalue(bsversion, v);
		}

		case frv_isRuntime: {
			/* Headless is always runtime (not development environment) */
			return setbooleanvalue(true, v);
		}

		case frv_isNative: {
			/* Always true on modern systems (no emulation) */
			return setbooleanvalue(true, v);
		}

		default:
			/* All other verbs stubbed for now */
			copystring(BIGSTRING("\pfrontier verb not implemented in headless mode"), bserror);
			return false;
	}
}

boolean frontierinitverbs(void) {
	hdlhashtable htable = nil;
	bigstring bsname;

	copystring(BIGSTRING("\pfrontier"), bsname);

	if (!newfunctionprocessor(bsname, &frontier_valueproc, true, &htable))
		return false;

	pushhashtable(htable);

	/* Register all 14 verbs as defined in kernelverbs.rc */
	#define ADD_VERB(name, tok) do { \
		bigstring bs; \
		copystring(name, bs); \
		if (!langaddkeyword(bs, tok)) { \
			pophashtable(); \
			return false; \
		} \
	} while(0)

	ADD_VERB(BIGSTRING("\pgetprogrampath"), frv_getProgramPath);
	ADD_VERB(BIGSTRING("\pgetfilepath"), frv_getFilePath);
	ADD_VERB(BIGSTRING("\penableagents"), frv_enableAgents);
	ADD_VERB(BIGSTRING("\prequesttofront"), frv_requestToFront);
	ADD_VERB(BIGSTRING("\pisruntime"), frv_isRuntime);
	ADD_VERB(BIGSTRING("\pcountthreads"), frv_countThreads);
	ADD_VERB(BIGSTRING("\pisnative"), frv_isNative);  /* was ispowerpc */
	ADD_VERB(BIGSTRING("\preclaimmemory"), frv_reclaimMemory);
	ADD_VERB(BIGSTRING("\pversion"), frv_version);
	ADD_VERB(BIGSTRING("\phashstats"), frv_hashStats);
	ADD_VERB(BIGSTRING("\pgethashloopcount"), frv_getHashLoopCount);
	ADD_VERB(BIGSTRING("\phideapplication"), frv_hideApplication);
	ADD_VERB(BIGSTRING("\pisvalidserialnumber"), frv_isValidSerialNumber);
	ADD_VERB(BIGSTRING("\pshowapplication"), frv_showApplication);

	#undef ADD_VERB

	pophashtable();
	return true;
}
