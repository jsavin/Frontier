/*
 * headless_bootstrap_extensions.c
 *
 * Phase 3 Bootstrap Extensions - Temporary verb implementations
 *
 * PURPOSE: This file contains verb implementations that are NOT yet in the
 * generated kernel_verbs_init.c or in the system.verbs tables. These are
 * minimal implementations needed to get system.startup scripts executing.
 *
 * LIFECYCLE: This is TEMPORARY SCAFFOLDING. These verbs should eventually:
 * 1. Be added to the generated kernel_verbs_init.c (if they're core/universal)
 * 2. Be loaded dynamically from system.verbs tables in the database
 * 3. Be removed from this file entirely
 *
 * NORTH STAR: In the ideal architecture, there would be NO hardcoded verbs
 * except the bare minimum needed to open a database and read system.verbs.
 * Everything else would be loaded dynamically at runtime.
 *
 * TODO: Track which verbs are here and migrate them to dynamic loading.
 */

#include "frontier.h"
#include "standard.h"
#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "langinternal.h"
#include "tablestructure.h"
#include "file.h"
#include "file_portable.h"

/*
 * =============================================================================
 * FRONTIER VERBS - frontier.* processor extensions
 * =============================================================================
 */

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

/* Frontier verb tokens (minimal subset for bootstrap) */
enum {
	frv_getFilePath = 0  /* frontier.getFilePath() - returns database file path */
};

static boolean frontier_bootstrap_valueproc(short token, hdltreenode hparam1,
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

		default:
			copystring(BIGSTRING("\pfrontier bootstrap verb not implemented"), bserror);
			return false;
	}
}

static boolean init_frontier_bootstrap_verbs(void) {
	hdlhashtable htable = nil;
	bigstring bsname;

	copystring(BIGSTRING("\pfrontier_bootstrap"), bsname);

	if (!newfunctionprocessor(bsname, &frontier_bootstrap_valueproc, true, &htable))
		return false;

	pushhashtable(htable);

	/* Register minimal verbs */
	#define ADD_VERB(name, tok) do { \
		bigstring bs; \
		copystring(name, bs); \
		if (!langaddkeyword(bs, tok)) { \
			pophashtable(); \
			return false; \
		} \
	} while(0)

	ADD_VERB(BIGSTRING("\pgetfilepath"), frv_getFilePath);

	#undef ADD_VERB

	pophashtable();
	return true;
}

/*
 * =============================================================================
 * FILE VERBS - file.* processor extensions
 * =============================================================================
 */

/* File verb tokens (minimal subset for bootstrap) */
enum {
	fv_folderFromPath = 0,  /* file.folderFromPath(path) - extract folder from path */
	fv_fileFromPath = 1     /* file.fileFromPath(path) - extract filename from path */
};

static boolean file_bootstrap_valueproc(short token, hdltreenode hparam1,
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
			return false;
	}
}

static boolean init_file_bootstrap_verbs(void) {
	hdlhashtable htable = nil;
	bigstring bsname;

	copystring(BIGSTRING("\pfile_bootstrap"), bsname);

	if (!newfunctionprocessor(bsname, &file_bootstrap_valueproc, false, &htable))
		return false;

	pushhashtable(htable);

	#define ADD_VERB(name, tok) do { \
		bigstring bs; \
		copystring(name, bs); \
		if (!langaddkeyword(bs, tok)) { \
			pophashtable(); \
			return false; \
		} \
	} while(0)

	ADD_VERB(BIGSTRING("\pfolderfrompath"), fv_folderFromPath);
	ADD_VERB(BIGSTRING("\pfilefrompath"), fv_fileFromPath);

	#undef ADD_VERB

	pophashtable();
	return true;
}

/*
 * =============================================================================
 * MAIN BOOTSTRAP EXTENSION INITIALIZER
 * =============================================================================
 */

boolean headless_init_bootstrap_extensions(void) {
	/*
	 * Initialize all bootstrap extension verb processors.
	 *
	 * NOTE: These processors use DIFFERENT NAMES (frontier_bootstrap, file_bootstrap)
	 * to avoid conflicts with the standard processors registered by kernel_verbs_init.c.
	 *
	 * TODO: As verbs are added to the generated code or loaded dynamically from
	 * system.verbs, remove them from this file.
	 */

	if (!init_frontier_bootstrap_verbs())
		return false;

	if (!init_file_bootstrap_verbs())
		return false;

	return true;
}
