
/*	$Id$    */

/*
    SPDX-License-Identifier: MIT

    Copyright (c) 1992-2004 UserLand Software, Inc.
    Copyright (c) 2025-2026 Frontier contributors

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
*/

/*
	4.1b4 dmb: new verbs based on ODB Engine API
*/

#include "frontier.h"
#include "standard.h"

#ifndef FRONTIER_HEADLESS
#include <land.h>
#include "mac.h"
#endif

#include "ops.h"
#include "memory.h"
#include "error.h"
#include "file.h"
#include "resources.h"
#include "scrap.h"
#include "strings.h"
#include "launch.h"
#include "notify.h"
#include "shell.h"
#include "lang.h"
#include "langexternal.h"
#include "logging.h"
#include "langinternal.h"
#include "langipc.h"
#include "kernelverbs.h"
#include "kernelverbdefs.h"
#include "tablestructure.h"
#include "process.h"
#include "processinternal.h"
#include "odbinternal.h"
#include "db_format.h" /* migration helpers */
#include "db.h" /* odb_context_guard */

/* DB_PATH_MAX is defined in db_format.h */

/* Database file extension length */
#define ROOT_EXTENSION_LEN 5   /* ".root" */

/*
if we're generating cfm (powerpc), we're linking to an odb engine shared
library, which has it's own globals. on 68k machines, we're staically linked,
so the odb calls mess with out global data. so we need to protect it.

In headless mode, we use an explicit context guard pattern to protect globals
from ODB engine modifications. This is simpler and more reliable than thread
swapping, and doesn't require thread infrastructure initialization.
*/

/* ODB context guard - protects caller globals from ODB engine modifications */
/* odb_context_guard moved to db.h and implemented in db.c for shared use */

#ifdef usingsharedlibrary

	#define odbnewfile		odbNewFile
	#define odbopenfile		odbOpenFile
	#define odbsavefile		odbSaveFile
	#define odbclosefile	odbCloseFile
	#define odbdefined		odbDefined
	#define odbdelete		odbDelete
	#define odbgettype		odbGetType
	#define odbgetvalue		odbGetValue
	#define odbsetvalue		odbSetValue
	#define odbnewtable		odbNewTable
	#define odbcountitems	odbCountItems
	#define odbgetnthitem	odbGetNthItem
	#define odbgetmoddate	odbGetModDate
	#define odbdisposevalue	odbDisposeValue
	#define odbgeterror		odbGetError

#else

	/*Prototypes*/

	boolean odbnewfile (hdlfilenum fnum);
	
	boolean odbaccesswindow (WindowPtr w, odbref *odb);
	
	boolean odbopenfile (hdlfilenum fnum, odbref *odb, boolean flreadonly);
	
	boolean odbsavefile (odbref odb);
	
	boolean odbclosefile (odbref odb);
	
	boolean odbdefined (odbref odb, bigstring bspath);
	
	boolean odbdelete (odbref odb, bigstring bspath);
	
	boolean odbgettype (odbref odb, bigstring bspath, OSType *odbType);
	
	boolean odbgetvalue (odbref odb, bigstring bspath, odbValueRecord *value);
	
	boolean odbsetvalue (odbref odb, bigstring bspath, odbValueRecord *value);
	
	boolean odbnewtable (odbref odb, bigstring bspath);
	
	boolean odbcountitems (odbref odb, bigstring bspath, long *count);
	
	boolean odbgetnthitem (odbref odb, bigstring bspath, long n, bigstring bsname);
	
	boolean odbgetmoddate (odbref odb, bigstring bspath, int64_t *date);
	
	boolean odbdisposevalue (odbref odb, odbValueRecord *value);
	
	
	/*Functions*/
	
	boolean odbnewfile (hdlfilenum fnum) {
		odb_context_guard guard;
		boolean fl;

		odb_guard_enter(&guard);

		fl = odbNewFile (fnum);

		cancoonglobals = nil;

		odb_guard_exit(&guard);

		return (fl);
		}

	boolean odbaccesswindow (WindowPtr w, odbref *odb) {
		odb_context_guard guard;
		boolean fl;

		odb_guard_enter(&guard);

		fl = odbAccessWindow (w, odb);

		cancoonglobals = nil;

		odb_guard_exit(&guard);

		return (fl);
		}

	boolean odbopenfile (hdlfilenum fnum, odbref *odb, boolean flreadonly) {
		odb_context_guard guard;
		boolean fl;

		odb_guard_enter(&guard);

		fl = odbOpenFile (fnum, odb, flreadonly);

		cancoonglobals = nil;

		odb_guard_exit(&guard);

		return (fl);
		}

	boolean odbsavefile (odbref odb) {
		odb_context_guard guard;
		boolean fl;

		odb_guard_enter(&guard);

		fl = odbSaveFile (odb);

		cancoonglobals = nil;

		odb_guard_exit(&guard);

		return (fl);
		}

	boolean odbclosefile (odbref odb) {
		odb_context_guard guard;
		boolean fl;

		odb_guard_enter(&guard);

		fl = odbCloseFile (odb);

		cancoonglobals = nil;

		odb_guard_exit(&guard);

		return (fl);
		}

	boolean odbdefined (odbref odb, bigstring bspath) {
		odb_context_guard guard;
		boolean fl;

		odb_guard_enter(&guard);

		fl = odbDefined (odb, bspath);

		cancoonglobals = nil;

		odb_guard_exit(&guard);

		return (fl);
		}

	boolean odbdelete (odbref odb, bigstring bspath) {
		odb_context_guard guard;
		boolean fl;

		odb_guard_enter(&guard);

		fl = odbDelete (odb, bspath);

		cancoonglobals = nil;

		odb_guard_exit(&guard);

		return (fl);
		}

	boolean odbgettype (odbref odb, bigstring bspath, OSType *odbType) {
		odb_context_guard guard;
		boolean fl;

		odb_guard_enter(&guard);

		fl = odbGetType (odb, bspath, odbType);

		cancoonglobals = nil;

		odb_guard_exit(&guard);

		return (fl);
		}

	boolean odbgetvalue (odbref odb, bigstring bspath, odbValueRecord *value) {
		odb_context_guard guard;
		boolean fl;

		odb_guard_enter(&guard);

		fl = odbGetValue (odb, bspath, value);

		cancoonglobals = nil;

		odb_guard_exit(&guard);

		return (fl);
		}

	boolean odbsetvalue (odbref odb, bigstring bspath, odbValueRecord *value) {
		odb_context_guard guard;
		boolean fl;

		odb_guard_enter(&guard);

		fl = odbSetValue (odb, bspath, value);

		cancoonglobals = nil;

		odb_guard_exit(&guard);

		return (fl);
		}

	boolean odbnewtable (odbref odb, bigstring bspath) {
		odb_context_guard guard;
		boolean fl;

		odb_guard_enter(&guard);

		fl = odbNewTable (odb, bspath);

		cancoonglobals = nil;

		odb_guard_exit(&guard);

		return (fl);
		}

	boolean odbcountitems (odbref odb, bigstring bspath, long *count) {
		odb_context_guard guard;
		boolean fl;

		odb_guard_enter(&guard);

		fl = odbCountItems (odb, bspath, count);

		cancoonglobals = nil;

		odb_guard_exit(&guard);

		return (fl);
		}

	boolean odbgetnthitem (odbref odb, bigstring bspath, long n, bigstring bsname) {
		odb_context_guard guard;
		boolean fl;

		odb_guard_enter(&guard);

		fl = odbGetNthItem (odb, bspath, n, bsname);

		cancoonglobals = nil;

		odb_guard_exit(&guard);

		return (fl);
		}

	boolean odbgetmoddate (odbref odb, bigstring bspath, int64_t *date) {
		odb_context_guard guard;
		boolean fl;

		odb_guard_enter(&guard);

		fl = odbGetModDate (odb, bspath, date);

		cancoonglobals = nil;

		odb_guard_exit(&guard);

		return (fl);
		}

	boolean odbdisposevalue (odbref odb, odbValueRecord *value) {
		odb_context_guard guard;

		odb_guard_enter(&guard);

		odbDisposeValue (odb, value);

		cancoonglobals = nil;

		odb_guard_exit(&guard);

		return (true);
		}

	#define odbgeterror		odbGetError

#endif

/* Global ODB list - used by both GUI and headless dbinitverbs()
 * Uses sentinel pattern to prevent UAF when closing last database.
 * The sentinel is a permanent allocated handle that's never freed,
 * preventing hodblist from becoming a dangling pointer.
 * tyodbrecord/hdlodbrecord defined in odbinternal.h */
hdlodbrecord hodblist = nil;  /* Initialized to sentinel handle on first use */


typedef enum tydbtoken { /*verbs that are processed by db*/
	
	newfunc,
	
	openfunc,
	
	savefunc,
	
	closefunc,
	
	definedfunc,
	
	/*
	gettypefunc,
	*/
	
	getvaluefunc,
	
	setvaluefunc,
	
	deletefunc,
	
	newtablefunc,
	
	istablefunc,
	
	countitemsfunc,
	
	getnthitemfunc,
	
	getmoddatefunc,

	compactdatabasefunc, /* db.compactDatabase(srcPath, dstPath) — v7→v7 compaction */

	ctdbverbs
	} tydbtoken;


static boolean odberror (boolean flresult) {

	bigstring bserror;

	if (flresult)
		return (false);

	odbgeterror (bserror);

	log_error(LOG_COMP_DB, "odberror: ODB engine error: %.*s", (int)bserror[0], (const char *)(bserror + 1));

	langerrormessage (bserror);

	return (true);
	} /*odberror*/


/* Forward declarations */
static void odb_ensure_root_extension(tyfilespec *fs);
static boolean odb_detect_database_version(const tyfilespec *fs, unsigned char *version);


static boolean getodbparam (hdltreenode hparam1, short pnum, hdlodbrecord *hodbrecord) {

	//
	// 2006-06-23 creedon: for Mac, FSRef-zed
	//

	bigstring bs;
	hdlodbrecord hodb;
	tyfilespec fs;
	ptrfilespec ptrfs;

	ptrfs = &fs;

	if (!getfilespecvalue (hparam1, pnum, ptrfs))
		return (false);

	/* Try exact match first - skip sentinel (hodblist itself) */
	for (hodb = (**hodblist).hnext; hodb != nil; hodb = (**hodb).hnext) {
		if ( equalfilespecs ( &( **hodb ).fs, ptrfs ) ) {
			*hodbrecord = hodb;
			return (true);
			}
		}

	getfsfile ( ptrfs, bs );

	lang2paramerror (dbnotopenederror, bsfunctionname, bs );

	return (false);

	} // getodbparam


static boolean getodbvalue (hdltreenode hparam1, short pnum, tyodbrecord *odb, boolean flreadonly) {

	//
	// 2006-06-23 creedon: for Mac, FSRef-ized
	//

	hdlodbrecord hodb;

	if (!getodbparam (hparam1, pnum, &hodb))
		return (false);

	*odb = **hodb;
	
	if ((*odb).flreadonly && !flreadonly) {
	
		bigstring bs;
	
		getfsfile ( &( *odb ).fs, bs );
		
		lang2paramerror (dbopenedreadonlyerror, bsfunctionname, bs );
		
		return (false);
		}
	
	return (true);
	
	} // getodbvalue


static boolean dbclosefile (hdlodbrecord hodb) {
	
	if (!odbclosefile ((**hodb).odb))
		return (false);
	
	closefile ((**hodb).fref);
	
	listunlink ((hdllinkedlist) hodblist, (hdllinkedlist) hodb);
	
	disposehandle ((Handle) hodb);
	
	return (true);
	} /*dbclosefile*/


boolean dbcloseallfiles (long refcon) {
#pragma unused (refcon)

	return (true);
	} /*dbcloseallfiles*/



/*
 * odb_detect_database_version
 *
 * Detect database version by reading the header's version byte.
 * Returns true if successful, false if file doesn't exist or can't be read.
 * Sets *version to the database version number (6 or 7).
 */
static boolean odb_detect_database_version(const tyfilespec *fs, unsigned char *version) {
	hdlfilenum fnum = 0;
	unsigned char header[2];
	long ctread = 2;
	boolean ok = false;

	if (fs == NULL)
		return false;

	if (version != NULL)
		*version = 0;

	/* Open file for reading */
	if (!openfile((tyfilespec *)fs, &fnum, true)) {
		log_trace(LOG_COMP_DB, "odb_detect_database_version: openfile failed");
		return false;
	}

	/* Read first 2 bytes (version is at byte offset 1) */
	if (filereaddata(fnum, ctread, &ctread, header)) {
		if (ctread >= 2) {  /* Validate we read enough bytes before parsing */
			/* Validate magic byte (0x00 for v7, 0x01 for v6) */
			if (header[0] != 0x00 && header[0] != 0x01) {
				log_error(LOG_COMP_DB, "odb_detect_database_version: invalid magic byte 0x%02x (file corrupted)", header[0]);
				closefile(fnum);
				return false;
			}

			if (version != NULL) {
				*version = header[1];  /* Version is second byte */

				/* Validate version is in reasonable range */
				if (*version == 0 || *version > 10) {
					log_warn(LOG_COMP_DB, "odb_detect_database_version: suspicious version=%d (expected 1-10)", *version);
					/* Continue anyway - future versions may exceed 10 */
				}

				log_trace(LOG_COMP_DB, "odb_detect_database_version: detected version=%d", *version);
			}
			ok = true;
		} else {
			log_trace(LOG_COMP_DB, "odb_detect_database_version: read %ld bytes (expected 2)", ctread);
		}
	} else {
		log_trace(LOG_COMP_DB, "odb_detect_database_version: filereaddata failed");
	}

	closefile(fnum);
	return ok;
}

/*
 * odb_ensure_root_extension
 *
 * Ensures the filespec has a .root extension for new databases.
 * - If path ends with .root, leave it alone.
 * - If path ends with .root7 (legacy), replace with .root.
 * - Otherwise, append .root.
 */
static void odb_ensure_root_extension(tyfilespec *fs) {
	bigstring bspath;

	filespectopath(fs, bspath);

	long len = stringlength(bspath);

	/* Check if it ends with .root7 (6 characters) -- legacy, convert to .root */
	if (len >= 6) {
		bigstring bsext7;
		midstring(bspath, len - 5, 6, bsext7);  /* Extract last 6 chars */

		if (equalstrings(bsext7, BIGSTRING("\x06.root7"))) {
			/* Replace .root7 with .root */
			setstringlength(bspath, len - 6);  /* Remove .root7 */
			pushstring(BIGSTRING("\x05.root"), bspath);  /* Append .root */

			log_debug(LOG_COMP_DB, "odb_ensure_root_extension: converted .root7 to .root");
			pathtofilespec(bspath, fs);
			return;
		}
	}

	/* Check if it ends with .root (5 characters) */
	if (len >= 5) {
		bigstring bsext;
		midstring(bspath, len - 4, 5, bsext);  /* Extract last 5 chars */

		if (equalstrings(bsext, BIGSTRING("\x05.root"))) {
			/* Already has .root extension, nothing to do */
			log_debug(LOG_COMP_DB, "odb_ensure_root_extension: already has .root extension");
			return;
		}
	}

	/* Doesn't end with .root or .root7, append .root */
	pushstring(BIGSTRING("\x05.root"), bspath);
	log_debug(LOG_COMP_DB, "odb_ensure_root_extension: appended .root extension");

	/* Update filespec with modified path */
	pathtofilespec(bspath, fs);
}


static boolean dbnewverb (hdltreenode hparam1, tyvaluerecord *vreturned) {

	//
	// 2006-06-20 creedon: for Mac, extend filespec
	//
	// 4.1b5 dmb: new verb
	//
	// 2026-03-22 JES: Ensure .root extension for new v7 databases
	//

	tyodbrecord odbrec;
	boolean fl;

	flnextparamislast = true;

	if ( ! getfilespecvalue ( hparam1, 1, &odbrec.fs ) )
		return (false);

	/* Ensure .root extension for new databases */
	odb_ensure_root_extension(&odbrec.fs);

	shellpushdefaultglobals (); // so that config is correct

	fl = opennewfile ( &odbrec.fs, config.filecreator, config.filetype, &odbrec.fref );

	shellpopglobals ();

	if (!fl)
		return (false);

	{
		/*
		 * 2026-02-06: odb_context_guard protects databasedata, rootvariable,
		 * roottable, currenthashtable, and hashtablestack from being stomped
		 * by dbnew() / dbdispose() inside odbnewfile().
		 */
		odb_context_guard guard;
		odb_guard_enter (&guard);

		fl = odbnewfile (odbrec.fref);

		closefile (odbrec.fref);

		odb_guard_exit (&guard);
	}

	if (odberror (fl)) {

		deletefile ( &odbrec.fs );

		return (false);
		}

	return (setbooleanvalue (true, vreturned));
	} // dbnewverb


static boolean dbopenverb (hdltreenode hparam1, tyvaluerecord *vreturned) {

	//
	// 2006-06-20 creedon: for Mac, extend filespec
	//
	// 4.1b5 dmb: added ability to access already-open root in Frontier
	//
	// 2026-03-22 JES: Simplified auto-migration, removed .root7 fallback
	//

	tyodbrecord odbrec;
	hdlodbrecord hodb;
	WindowPtr w;
	bigstring bspath;

	log_trace(LOG_COMP_DB, "dbopenverb: enter");

	odbrec.fref = 0;

	if ( ! getfilespecvalue ( hparam1, 1, &odbrec.fs ) ) {
		log_error(LOG_COMP_DB, "dbopenverb: getfilespecvalue failed for parameter 1");
		return (false);
	}

	filespectopath(&odbrec.fs, bspath);
	log_debug(LOG_COMP_DB, "dbopenverb: path=%.*s", (int)bspath[0], (const char *)(bspath + 1));

	flnextparamislast = true;

	if (!getbooleanvalue (hparam1, 2, &odbrec.flreadonly)) {
		log_error(LOG_COMP_DB, "dbopenverb: getbooleanvalue failed for parameter 2 (readonly flag)");
		return (false);
	}

	/* Issue #127: --lock-opened-roots / FRONTIER_LOCK_OPENED_ROOTS=1 forces
	 * every loaded-from-disk guest DB read-only, regardless of the user's
	 * db.open(path, readonly) flag. Newly created roots (db.new /
	 * file.save / file.saveAs / db.compactDatabase) are unaffected --
	 * those paths don't go through dbopenverb.
	 *
	 * env_truthy() (Common/SystemHeaders/standard.h) shares the truthiness
	 * contract with cli_parser.c so the CLI flag and the dbopenverb env
	 * check agree on what counts as enabled. */
	if (!odbrec.flreadonly && env_truthy("FRONTIER_LOCK_OPENED_ROOTS")) {
		log_debug(LOG_COMP_DB, "dbopenverb: FRONTIER_LOCK_OPENED_ROOTS in effect, forcing readonly=true");
		odbrec.flreadonly = true;
	}

	log_debug(LOG_COMP_DB, "dbopenverb: readonly=%d", odbrec.flreadonly);

	/* Auto-migration: If opening a v6 database in read-write mode, migrate to v7.
	 * ensure_database_v7 handles all cases: already v7, previous migration exists,
	 * or fresh v6 needing migration. After migration, the v7 database is at the
	 * original .root path and the v6 original is backed up to .v6.root. */
	if (!odbrec.flreadonly) {
		char cpath[DB_PATH_MAX];
		char output_path[DB_PATH_MAX];
		boolean migrated = false;

		log_debug(LOG_COMP_DB, "dbopenverb: AUTO-MIGRATION CHECK START (read-write mode)");

		filespectopath(&odbrec.fs, bspath);
		copyptocstring(bspath, cpath);

		if (ensure_database_v7(cpath, &migrated, output_path, sizeof(output_path))) {
			if (migrated) {
				/* Fresh migration occurred */
				log_info(LOG_COMP_DB, "dbopenverb: migrated v6 database to v7: %s", output_path);

				fputs("Migrated v6 database to v7 format.\n", stdout);
				fputs("  v7 database: ", stdout);
				fputs(output_path, stdout);
				fputs("\n", stdout);
				fputs("  v6 backup preserved alongside.\n", stdout);
			}

			if (output_path[0] != '\0' && strcmp(cpath, output_path) != 0) {
				/* Output path differs from input -- update filespec */
				bigstring bsoutput;
				copyctopstring(output_path, bsoutput);
				if (!pathtofilespec(bsoutput, &odbrec.fs)) {
					log_error(LOG_COMP_DB, "dbopenverb: pathtofilespec failed for %s", output_path);
					return (false);
				}
				log_debug(LOG_COMP_DB, "dbopenverb: using migrated path %s", output_path);
			}
		}
		else {
			log_error(LOG_COMP_DB, "dbopenverb: migration failed for %s", cpath);

			bigstring bserr;
			char errmsg[512];
			snprintf(errmsg, sizeof(errmsg), "Can't open the database because v7 format verification failed for: %s (check file permissions and disk space)", cpath);
			copyctopstring(errmsg, bserr);
			langerrormessage(bserr);
			return (false);
		}
	}

	/* #270: Check if database is already in the open database list.
	 * Opening the same file twice causes concurrent write corruption. */
	{
		hdlodbrecord h;

		for (h = (**hodblist).hnext; h != nil; h = (**h).hnext) {
			if (equalfilespecs (&(**h).fs, &odbrec.fs)) {
				bigstring bs;

				getfsfile (&odbrec.fs, bs);
				log_error(LOG_COMP_DB, "dbopenverb: database already open: %.*s", (int)bs[0], (const char *)(bs + 1));
				lang2paramerror (dbalreadyopenederror, bsfunctionname, bs);
				return (false);
			}
		}
	}

	w = shellfindfilewindow ( &odbrec.fs );

	if (w != nil) {
		log_debug(LOG_COMP_DB, "dbopenverb: file already open in window, accessing existing window");

		if (odberror (odbaccesswindow (w, &odbrec.odb))) {
			log_error(LOG_COMP_DB, "dbopenverb: odbaccesswindow failed");
			return (false);
		}

		// fref remains zero, so unwanted closefiles aren't a problem
		}
	else {
		log_debug(LOG_COMP_DB, "dbopenverb: opening new file");

		if ( ! openfile ( &odbrec.fs, &odbrec.fref, odbrec.flreadonly)) {
			log_error(LOG_COMP_DB, "dbopenverb: openfile failed, path=%s readonly=%d",
				stringbaseaddress(bspath), odbrec.flreadonly);
			return (false);
		}

		log_debug(LOG_COMP_DB, "dbopenverb: openfile succeeded, fref=%d", odbrec.fref);

		if (odberror (odbopenfile (odbrec.fref, &odbrec.odb, odbrec.flreadonly))) {
			log_error(LOG_COMP_DB, "dbopenverb: odbopenfile failed, fref=%d readonly=%d",
				odbrec.fref, odbrec.flreadonly);
			closefile (odbrec.fref);
			return (false);
			}

		log_debug(LOG_COMP_DB, "dbopenverb: odbopenfile succeeded, odb=%p", odbrec.odb);
		}

	if (!newfilledhandle (&odbrec, sizeof (odbrec), (Handle *) &hodb)) {
		log_error(LOG_COMP_DB, "dbopenverb: newfilledhandle failed (out of memory?)");
		odbclosefile (odbrec.odb);
		closefile (odbrec.fref);
		return (false);
		}

	/* Add to open database list after sentinel (hodblist itself is the sentinel) */
	if ((**hodblist).hnext == nil) {
		/* First real database after sentinel */
		(**hodblist).hnext = hodb;
		(**hodb).hnext = nil;
	}
	else {
		/* Add to existing list (after sentinel) */
		listlink ((hdllinkedlist) (**hodblist).hnext, (hdllinkedlist) hodb);
	}

	return (setbooleanvalue (true, vreturned));

	} // dbopenverb


static boolean dbsaveverb (hdltreenode hparam1, tyvaluerecord *vreturned) {

	tyodbrecord odbrec;
	
	flnextparamislast = true;
	
	if (!getodbvalue (hparam1, 1, &odbrec, false))
		return (false);
	
	return (setbooleanvalue (odbsavefile (odbrec.odb), vreturned));
	} /*dbsaveverb*/


static boolean dbcloseverb (hdltreenode hparam1, tyvaluerecord *vreturned) {

	hdlodbrecord hodb;
	
	flnextparamislast = true;
	
	if (!getodbparam (hparam1, 1, &hodb))
		return (false);
	
	return (setbooleanvalue (dbclosefile (hodb), vreturned));
	} /*dbcloseverb*/


static boolean dbdefinedverb (hdltreenode hparam1, tyvaluerecord *vreturned) {

	/*
	4.1b5 dmb: new verb
	*/

	tyodbrecord odbrec;
	bigstring bsaddress;
	boolean fl;

	if (!getodbvalue (hparam1, 1, &odbrec, true))
		return (false);

	flnextparamislast = true;

	if (!getstringvalue (hparam1, 2, bsaddress))
		return (false);
	
	fl = odbdefined (odbrec.odb, bsaddress);
	
	return (setbooleanvalue (fl, vreturned));
	} /*dbdefinedverb*/


static boolean dbgetvalueverb (hdltreenode hparam1, tyvaluerecord *vreturned) {

	/*
	4.1.1b1 dmb: fixed memory leak; push value on temp stack
	
	5.0b17 dmb: use pushtmpstackvalue to put external types returned from 
	the other odb onto our temp stack

	5.0.1 dmb: but only pushtmpstack if heapallocated
	*/
	
	tyodbrecord odbrec;
	bigstring bsaddress;
	odbValueRecord value;
	tyvaluetype type;
	
	if (!getodbvalue (hparam1, 1, &odbrec, true))
		return (false);
	
	flnextparamislast = true;
	
	if (!getstringvalue (hparam1, 2, bsaddress))
		return (false);
	
	if (odberror (odbgetvalue (odbrec.odb, bsaddress, &value)))
		return (false);
	
	type = langexternalgetvaluetype (value.valuetype);
	
	if (type == (tyvaluetype) -1) {
	
		return (setbinaryvalue (value.data.binaryvalue, value.valuetype, vreturned));
		}
	else {
		
		initvalue (vreturned, type);
		
		(*vreturned).data.binaryvalue = value.data.binaryvalue;
		
		if (langheapallocated (vreturned, nil))
			pushtmpstackvalue (vreturned); //5.0b17
		
		return (true);
		
		/*
		initvalue (&val, type);
		
		val.data.binaryvalue = value.data.binaryvalue;
		
		return (copyvaluerecord (val, vreturned));
		*/
		}
	} /*dbgetvalueverb*/


static boolean dbsetvalueverb (hdltreenode hparam1, tyvaluerecord *vreturned) {
	
	tyodbrecord odbrec;
	bigstring bsaddress;
	odbValueRecord value;
	tyvaluerecord val;
	boolean flerror;
	
	if (!getodbvalue (hparam1, 1, &odbrec, false))
		return (false);
	
	if (!getstringvalue (hparam1, 2, bsaddress))
		return (false);
	
	flnextparamislast = true;
	
	if (!getparamvalue (hparam1, 3, &val))
		return (false);
	
	if (!copyvaluedata (&val))
		return (false);
	
	value.valuetype = (odbValueType) langexternalgettypeid (val);
	
	/*
	if (val.valuetype == binaryvaluetype)
		pullfromhandle (val.data.binaryvalue, 0L, sizeof (value.valuetype), &value.valuetype);
	*/
	
	value.data.binaryvalue = val.data.binaryvalue; /*largest field covers everything*/
	
	flerror = odberror (odbsetvalue (odbrec.odb, bsaddress, &value));
	
	disposevaluerecord (val, false);
	
	if (flerror)
		return (false);

	return (setbooleanvalue (true, vreturned));
	} /*dbsetvalueverb*/


static boolean dbdeleteverb (hdltreenode hparam1, tyvaluerecord *vreturned) {

	tyodbrecord odbrec;
	bigstring bsaddress;
	
	if (!getodbvalue (hparam1, 1, &odbrec, false))
		return (false);
	
	flnextparamislast = true;
	
	if (!getstringvalue (hparam1, 2, bsaddress))
		return (false);
	
	if (odberror (odbdelete (odbrec.odb, bsaddress)))
		return (false);
	
	return (setbooleanvalue (true, vreturned));
	} /*dbdeleteverb*/


static boolean dbnewtableverb (hdltreenode hparam1, tyvaluerecord *vreturned) {

	tyodbrecord odbrec;
	bigstring bsaddress;
	
	if (!getodbvalue (hparam1, 1, &odbrec, false))
		return (false);
	
	flnextparamislast = true;
	
	if (!getstringvalue (hparam1, 2, bsaddress))
		return (false);
	
	if (odberror (odbnewtable (odbrec.odb, bsaddress)))
		return (false);

	return (setbooleanvalue (true, vreturned));
	} /*dbnewtableverb*/


static boolean dbistableverb (hdltreenode hparam1, tyvaluerecord *vreturned) {

	tyodbrecord odbrec;
	bigstring bsaddress;
	OSType odbtype;
	
	if (!getodbvalue (hparam1, 1, &odbrec, true))
		return (false);
	
	flnextparamislast = true;
	
	if (!getstringvalue (hparam1, 2, bsaddress))
		return (false);
	
	if (odberror (odbgettype (odbrec.odb, bsaddress, &odbtype)))
		return (false);
	
	return (setbooleanvalue (odbtype == tableT, vreturned));
	} /*dbistableverb*/


static boolean dbcountitemsverb (hdltreenode hparam1, tyvaluerecord *vreturned) {

	tyodbrecord odbrec;
	bigstring bsaddress;
	long ctitems;
	
	if (!getodbvalue (hparam1, 1, &odbrec, true))
		return (false);
	
	flnextparamislast = true;
	
	if (!getstringvalue (hparam1, 2, bsaddress))
		return (false);
	
	if (odberror (odbcountitems (odbrec.odb, bsaddress, &ctitems)))
		return (false);
	
	return (setlongvalue (ctitems, vreturned));
	} /*dbcountitemsverb*/


static boolean dbgetnthitemverb (hdltreenode hparam1, tyvaluerecord *vreturned) {

	tyodbrecord odbrec;
	bigstring bsaddress;
	bigstring bsname;
	long n;
	
	if (!getodbvalue (hparam1, 1, &odbrec, true))
		return (false);
	
	if (!getstringvalue (hparam1, 2, bsaddress))
		return (false);
	
	flnextparamislast = true;
	
	if (!getlongvalue (hparam1, 3, &n))
		return (false);
	
	if (odberror (odbgetnthitem (odbrec.odb, bsaddress, n, bsname)))
		return (false);
	
	return (setstringvalue (bsname, vreturned));
	} /*dbgetnthitemverb*/


static boolean dbgetmoddateverb (hdltreenode hparam1, tyvaluerecord *vreturned) {

	tyodbrecord odbrec;
	bigstring bsaddress;
	int64_t moddate;

	if (!getodbvalue (hparam1, 1, &odbrec, true))
		return (false);

	flnextparamislast = true;

	if (!getstringvalue (hparam1, 2, bsaddress))
		return (false);

	if (odberror (odbgetmoddate (odbrec.odb, bsaddress, &moddate)))
		return (false);

	return (setdatevalue (moddate, vreturned));
	} /*dbgetmoddateverb*/


/* Lowercase wrapper following the existing pattern (e.g. odbsavefile).
 * Wraps odbCompactDatabase with the odb_context_guard so the caller's
 * databasedata / rootvariable / mode stack are preserved across the call.
 * The wrapping verb (dbcompactdatabaseverb) is responsible for closing
 * the source database after this returns, on both success and failure. */
static boolean odbcompactdatabase_local (odbref odb, const char *dst_path) {
	odb_context_guard guard;
	boolean fl;

	odb_guard_enter (&guard);

	fl = odbCompactDatabase (odb, dst_path);

	cancoonglobals = nil;

	odb_guard_exit (&guard);

	return (fl);
}


static boolean dbcompactdatabaseverb (hdltreenode hparam1, tyvaluerecord *vreturned) {

	/*
	 * db.compactDatabase(srcPath, dstPath) — write a freshly-compacted copy
	 * of an open guest database to a new file.
	 *
	 * Walks the live in-memory tree of the source and writes it to dstPath.
	 * The result has no avail-list dead space, so the destination file is
	 * the minimum size required to represent the live data. Used to reclaim
	 * space after large delete operations.
	 *
	 * Contrast with fileMenu.saveCopy: that does a byte-for-byte file copy
	 * which preserves the avail-list (and the dead space inside it).
	 *
	 * Constraints:
	 *   - srcPath must be currently open as a guest database
	 *   - dstPath must NOT already exist (refuses to overwrite, atomic
	 *     O_EXCL+O_NOFOLLOW create at the file layer)
	 *   - dstPath gets a .root extension auto-appended if missing (matches
	 *     db.new / db.open semantics)
	 *   - srcPath cannot be the running system root cancoon (defensive —
	 *     the running root must be saved via fileMenu.save / shutdown,
	 *     not compacted while live)
	 *   - dstPath length is bounded by UserTalk bigstring (255 chars)
	 *
	 * Side effects:
	 *   - On SUCCESS: the source database is auto-closed (its in-memory
	 *     tree is unsafe to reuse — oldaddress fields point at dst's
	 *     address space).  Callers do NOT need to call db.close.
	 *   - On FAILURE: the source database is force-closed (its in-memory
	 *     tree may be partially mutated).  Callers do NOT need to call
	 *     db.close.  The on-disk source file is not modified, so callers
	 *     can re-open from disk if needed.
	 *
	 * Returns true on success, false on error (with langerror set).
	 */

	bigstring bssrc, bsdst;
	tyfilespec srcfs, dstfs;
	hdlodbrecord hodb_source = nil;
	char dstpath_c[DB_PATH_MAX];
	boolean ok = false;
	boolean source_closed = false;

	setbooleanvalue (false, vreturned);

	/* Validate exactly 2 params */
	if (langgetparamcount (hparam1) != 2) {
		langerrormessage (PSTRING ("\x43",
			"db.compactDatabase requires exactly 2 parameters (srcPath, dstPath)"));
		return (false);
	}

	/* Get srcPath as filespec to look up the open db */
	if (!getfilespecvalue (hparam1, 1, &srcfs))
		return (false);

	flnextparamislast = true;

	if (!getstringvalue (hparam1, 2, bsdst))
		return (false);

	/* Length check: bigstring is 255 chars max but the verb's user-facing
	 * contract limits dstPath to that even if bigstring grew.  Detect the
	 * "passed a too-long literal" case explicitly (P2-12 — earlier the
	 * >=1024 check below was dead code because bigstring already truncated
	 * to 255). */
	if (stringlength (bsdst) > 255) {
		langerrormessage (PSTRING ("\x39",
			"db.compactDatabase: dstPath length exceeds 255 characters"));
		return (false);
	}

	/* Convert bigstring path to filespec, auto-coerce to .root extension
	 * (matches db.new / db.open semantics — P2-14).  We modify the local
	 * filespec only; the caller's bsdst is unchanged. */
	if (!pathtofilespec (bsdst, &dstfs)) {
		langerrormessage (PSTRING ("\x3c",
			"db.compactDatabase: destination path is not a valid filespec"));
		return (false);
	}
	odb_ensure_root_extension (&dstfs);

	/* Convert dst filespec back to a C string for db_format_compact_to_path */
	{
		bigstring bsdst_coerced;
		long ctbytes;
		filespectopath (&dstfs, bsdst_coerced);
		ctbytes = stringlength (bsdst_coerced);
		if (ctbytes >= (long) sizeof dstpath_c) {
			langerrormessage (PSTRING ("\x2d", "db.compactDatabase: destination path too long"));
			return (false);
		}
		copyptocstring (bsdst_coerced, dstpath_c);
	}

	/* Find the source database in hodblist (must be open as guest) */
	{
		hdlodbrecord hodb;
		filespectopath (&srcfs, bssrc);

		for (hodb = (**hodblist).hnext; hodb != nil; hodb = (**hodb).hnext) {
			if (equalfilespecs (&(**hodb).fs, &srcfs)) {
				hodb_source = hodb;
				break;
			}
		}

		if (hodb_source == nil) {
			lang2paramerror (dbnotopenederror, bsfunctionname, bssrc);
			return (false);
		}

		if ((**hodb_source).flreadonly) {
			lang2paramerror (dbopenedreadonlyerror, bsfunctionname, bssrc);
			return (false);
		}
	}

	/* Reject the system root (P1-3).  The system root's databasedata is
	 * the global currently in effect — cli main.c sets it at boot and
	 * leaves it there for the lifetime of the process.  Each guest db
	 * opened via db.open creates its OWN cancoon record and database
	 * handle, distinct from the system root's.  So if (**hodb_source).odb
	 * (which points at a cancoon record) has hdatabase == databasedata
	 * (the running system root's database), we are about to compact the
	 * system root itself — refuse.
	 *
	 * Note: this check must come AFTER we've verified hodb_source is in
	 * hodblist.  Calling db.compactDatabase BEFORE db.open on the system
	 * root path returns the "not open" error from the loop above; only
	 * if a path gets registered as both system root and guest can this
	 * fire.  Currently the call chain (db.open creates a new cancoon)
	 * makes that impossible from UserTalk — this is defense-in-depth
	 * against a future change that exposes the system cancoon as a
	 * guest. */
	{
		hdldatabaserecord src_db = odb_get_database ((**hodb_source).odb);
		if (src_db != nil && src_db == databasedata) {
			langerrormessage (PSTRING ("\x3a",
				"db.compactDatabase: cannot compact the running system root"));
			return (false);
		}
	}

	/* Atomic O_EXCL+O_NOFOLLOW create happens inside db_format_compact_to_path
	 * via opennewfile_exclusive (P1-1) — no separate fopen("rb") pre-check. */

	/* First, ensure the source is fully saved to disk. compaction reads from
	 * the in-memory tree, so the on-disk state must be flushed for any
	 * caller that later reopens the source from disk to see the same data. */
	if (!odbsavefile ((**hodb_source).odb)) {
		log_error (LOG_COMP_DB, "db.compactDatabase: pre-compaction save failed");
		langerrormessage (PSTRING ("\x26", "db.compactDatabase: source save failed"));
		return (false);
	}

	/* Run the compaction. */
	ok = odbcompactdatabase_local ((**hodb_source).odb, dstpath_c);

	/* Auto-close the source on BOTH success and failure (P1-2 + P0-3).
	 * - Success: source's in-memory tree has dst-space oldaddress fields
	 *   and is unsafe to reuse — close before any subsequent verb can
	 *   touch it.
	 * - Failure: source's in-memory tree may be partially-mutated and
	 *   is unsafe to reuse — force-close to put the state in a known
	 *   place.  The on-disk source is unchanged, so re-opening from
	 *   disk gives a clean state.
	 * dbclosefile() unlinks from hodblist, calls odbCloseFile (which
	 * disposes the cancoon record + in-memory tree), and frees the
	 * hodbrecord handle. */
	if (!dbclosefile (hodb_source)) {
		/* Disposal failed — log but don't propagate (the compact result
		 * is what callers care about; the source is in an unknown state
		 * either way). */
		log_warn (LOG_COMP_DB, "db.compactDatabase: dbclosefile of source failed (continuing)");
	} else {
		source_closed = true;
	}

	if (!ok) {
		langerrormessage (PSTRING ("\x2f", "db.compactDatabase: compaction operation failed"));
		return (false);
	}

	(void) source_closed;
	return (setbooleanvalue (true, vreturned));
	} /*dbcompactdatabaseverb*/


boolean dbfunctionvalue (short token, hdltreenode hparam1, tyvaluerecord *vreturned, bigstring bserror) {
#pragma unused (bserror)

	/*
	4.1b4 dmb: new verb set based on odbEngine

	5.0b17 dmb: use swapinthreadglobals (nil) for all our odb calls to protect ours
	*/

	register hdltreenode hp1 = hparam1;
	register tyvaluerecord *v = vreturned;

	setbooleanvalue (false, v); /*assume the worst*/

	switch (token) {
		
		case newfunc:
			return (dbnewverb (hp1, v));
		
		case openfunc:
			return (dbopenverb (hp1, v));
		
		case savefunc:
			return (dbsaveverb (hp1, v));
		
		case closefunc:
			return (dbcloseverb (hp1, v));
		
		case definedfunc:
			return (dbdefinedverb (hp1, v));
		
		case getvaluefunc:
			return (dbgetvalueverb (hp1, v));
		
		case setvaluefunc:
			return (dbsetvalueverb (hp1, v));
		
		case deletefunc:
			return (dbdeleteverb (hp1, v));
		
		case newtablefunc:
			return (dbnewtableverb (hp1, v));
		
		case istablefunc:
			return (dbistableverb (hp1, v));
		
		case countitemsfunc:
			return (dbcountitemsverb (hp1, v));
		
		case getnthitemfunc:
			return (dbgetnthitemverb (hp1, v));
		
		case getmoddatefunc:
			return (dbgetmoddateverb (hp1, v));

		case compactdatabasefunc:
			return (dbcompactdatabaseverb (hp1, v));

		default:
			return (false);
		}
	} /*dbfunctionvalue*/


#ifndef FRONTIER_HEADLESS
/* Windowed mode: register with dbfunctionvalue callback */
boolean dbinitverbs (void) {

	if (!loadfunctionprocessor (iddbverbs, &dbfunctionvalue))
		return (false);

	/* Initialize sentinel handle to prevent UAF when closing last database
	 * This handle is never freed, so hodblist never becomes a dangling pointer */
	if (!newclearhandle (sizeof (tyodbrecord), (Handle *) &hodblist))
		return (false);

	return (true);
	} /*dbinitverbs*/
#endif /* !FRONTIER_HEADLESS */
/* Note: Headless mode provides its own dbinitverbs() in tests/headless_db_verbs.c
 * which registers with headless_db_verbs_callback instead. */


/* Exposed helpers for Save-path migration */
boolean db_get_path_for_odb(odbref odb, bigstring out) {
    hdlodbrecord hodb;
    /* Skip sentinel (hodblist itself) when searching */
    for (hodb = (**hodblist).hnext; hodb != nil; hodb = (**hodb).hnext) {
        if ((**hodb).odb == odb) {
            return filegetpath(&(**hodb).fs, out);
        }
    }
    return false;
}

boolean db_migrate_reopen_if_legacy(odbref *podb) {
    if (podb == NULL || *podb == NULL)
        return false;
    if (db_format_mode_current().use_64bit_format)
        return true; /* already v7 */

    hdlodbrecord hodb;
    /* Skip sentinel (hodblist itself) when searching */
    for (hodb = (**hodblist).hnext; hodb != nil; hodb = (**hodb).hnext) {
        if ((**hodb).odb == *podb) {
            bigstring bspath;
            char cpath[1024];
            if (!filegetpath(&(**hodb).fs, bspath))
                return false;
            copyptocstring(bspath, cpath);
            /* Close current file handle if open */
            if ((**hodb).fref)
                closefile((**hodb).fref);
            /* Perform header-only migration with backup */
            if (!migrate_32bit_to_64bit(cpath))
                return false;
            char migrated_path[1024];
            if (!db_format_last_migration_output_path(migrated_path, sizeof migrated_path))
                return false;
            bigstring bsmigrated;
            copyctopstring(migrated_path, bsmigrated);
            if (!pathtofilespec(bsmigrated, &(**hodb).fs))
                return false;
            /* Reopen the migrated file */
            if (!openfile(&(**hodb).fs, &(**hodb).fref, (**hodb).flreadonly))
                return false;
            odbref newodb;
            if (odberror(odbopenfile((**hodb).fref, &newodb, (**hodb).flreadonly))) {
                closefile((**hodb).fref);
                return false;
            }
            (**hodb).odb = newodb;
            *podb = newodb;
            return true;
        }
    }
    return false;
}
