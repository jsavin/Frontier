
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

/* 2025-11-24 Codex: Normalize BE writes/coverage for v7 portability. */

#define ODBENGINE_PROVIDES_CANCOONGLOBALS
#include "frontier.h"
#include "standard.h"

#include "memory.h"
#include "dialogs.h"
#include "file.h"
#include "font.h"
#include "resources.h"
#include "ops.h"
#include "quickdraw.h"
#include "strings.h"
#include "langexternal.h"
#include "langinternal.h"
#include "langipc.h"
#include "tablestructure.h"
#include "tableinternal.h"
#include "shell.rsrc.h"
#include "odbinternal.h"
#include "shellprivate.h"
#include "timedate.h"
#include "byteorder.h"	/* 2006-04-08 aradke: endianness conversion macros */
#include "db_format.h" /* format mode */
#include "logging.h"

#pragma pack(2)
typedef struct tycancoonrecord { /*one of these for every cancoon file that's open*/

	hdldatabaserecord hdatabase; /*db.c's record*/
	
	hdlhashtable hroottable; /*the root symbol table for this file*/
	
	Handle hrootvariable; /*the variable record for the root symbol table*/
	
	hdltablestack htablestack;
	
	boolean accesssing;
	
	
	WindowPtr shellwindow;
	
	} tycancoonrecord, *ptrcancoonrecord, **hdlcancoonrecord;
#pragma options align=reset

#define cancoonversionnumber 0x03




#define ctwindowinfo 6 /*number of windowinfo records saved in each cancoon record*/
#pragma pack(2)
typedef struct tycancoonwindowinfo { /*lives both in memory and on disk*/	
	
	Rect windowrect;
	
	diskfontstring fontname; /*only maintained on disk*/
	
	short fontnum; /*only valid when it's in memory*/
	
	short fontsize, fontstyle;
	
	WindowPtr w; /*only valid when it's in memory*/
	
	char waste [10];
	} tycancoonwindowinfo;
#pragma options align=reset

#pragma pack(2)
typedef struct tyversion2cancoonrecord {
	
	short versionnumber;
	
	dbaddress adrroottable;
	
	tycancoonwindowinfo windowinfo [ctwindowinfo];
	
	dbaddress adrscriptstring; /*the string that appears in the quickscript window*/
	
	unsigned short flags;
	
	short ixprimaryagent;
	
	short waste [28]; /*room to grow*/
	} tyversion2cancoonrecord;
#pragma options align=reset

	#define flflagdisabled_mask 0x8000 /*hide the flag?*/
	#define flpopupdisabled_mask 0x4000 /*hide the agents popup menu?*/
	#define flbigwindow_mask 0x2000 /*is the flag toggled to the big window state?*/


static bigstring bserror = "\0";

static hdlcancoonrecord cancoonglobals = nil;


static boolean disposecancoonrecord (hdlcancoonrecord hcancoon) {
	
	/*
	5.0.1 dmb: fixed leak - use tabledisposetable, not disposehashtable
	*/
	
	hdlcancoonrecord hc = hcancoon;
	
	if (hc == nil)
		return (false);
	
	if (!(**hc).accesssing) {
		
		cleartablestructureglobals ();	/*do this now to avoid debug check in disposehashtable*/
		
		tabledisposetable ((**hc).hroottable, false); /*yup, checks for nil*/
		
		disposehandle ((Handle) (**hc).hrootvariable); /*1.0b2 dmb: site of memory leak*/
		}
	
	disposehandle ((Handle) (**hc).htablestack);
	
	disposehandle ((Handle) hc);
	
	return (true);
	} /*disposecancoonrecord*/


static boolean newcancoonrecord (hdlcancoonrecord *hcancoon) {
	
	hdlcancoonrecord hc;
	hdltablestack htablestack;
	
	if (!newclearhandle (sizeof (tycancoonrecord), (Handle *) hcancoon))
		return (false);
	
	hc = *hcancoon;
	
	if (!newclearhandle (sizeof (tytablestack), (Handle *) &htablestack)) {
		
		disposehandle ((Handle) hc);
		
		return (false);
		}
	
	(**hc).htablestack = htablestack;

	return (true);
	} /*newcancoonrecord*/


static boolean ccloadsystemtable (hdlcancoonrecord hcancoon, dbaddress adr) {
	
	/*
	1.0b2 dmb: we don't rely on the system table, so don't force 
	its creation by calling settablestructureglobals.
	*/
	
	hdlcancoonrecord hc = hcancoon;
	boolean fl;
	Handle hvariable;
	hdlhashtable htable;
	
	hashtablestack = (**hc).htablestack;
	
	fl = tableloadsystemtable (adr, &hvariable, &htable, false);
	
	if (fl) {
		
		cleartablestructureglobals ();
		
		(**hc).hrootvariable = rootvariable = hvariable;
		
		(**hc).hroottable = roottable = htable;
		
		assert (tablevalidate (htable, true));
		
		/*
		fl = settablestructureglobals (hvariable, true);
		*/
		
		currenthashtable = roottable;
		}
	
	return (fl);
	} /*ccloadsystemtable*/


static boolean odberrorroutine (bigstring bs, ptrvoid refcon) {
#pragma unused (refcon)

	copystring (bs, bserror);
	
	return (false); /*consume the error*/
	} /*odberrorroutine*/


static void setcancoonglobals (hdlcancoonrecord hcancoon) {

	hdlcancoonrecord hc = hcancoon;

		{
		databasedata = (**hc).hdatabase;

		hashtablestack = (**hc).htablestack;

		/*
		 * CRITICAL P0 FIX (Issue #266): Don't call settablestructureglobals for guest databases.
		 *
		 * settablestructureglobals tries to create system.builtins and other system tables,
		 * which is inappropriate for guest databases. Just set the globals directly like
		 * ccloadsystemtable does.
		 */
		cleartablestructureglobals();

		rootvariable = (Handle) (**hc).hrootvariable;
		roottable = (**hc).hroottable;

		currenthashtable = roottable;
		
		cancoonglobals = hc; /*this global is independent of shellpush/popglobals*/
		
		langcallbacks.errormessagecallback = &odberrorroutine;
		}
	} /*setcancoonglobals*/


static void clearcancoonglobals (void) {
	
	databasedata = nil; /*db.c*/
	
	cleartablestructureglobals (); /*tablestructure.c: roottable, handlertable, etc.*/
	
	currenthashtable = nil;
	
	cancoonglobals = nil; /*our very own superglobal*/
	
	hashtablestack = nil;
	} /*clearcancoonglobals*/


static boolean loadversion2cancoonfile (dbaddress adr, hdlcancoonrecord hcancoon) {
	
	tyversion2cancoonrecord info;
	
	if (!dbreference (adr, sizeof (info), &info))
		return (false);
	
	/* only variable used here! */
	disktomemlong (info.adrroottable);

	if (!ccloadsystemtable (hcancoon, info.adrroottable))
		return (false);
	
	assert ((**cancoonglobals).hroottable == roottable); /*should already be set up*/
	
	return (true);
	} /*loadversion2cancoonfile*/



	static boolean odbexpandtodotparams (bigstring bs, hdlhashtable *htable, bigstring bsname) {
		
		/*
		the odbengine version of langexpandtodotparams gaurantees that htable 
		will be non-nil. the frontier version doesn't. if we have just a name, 
		we interpret it as a root-level item
		*/
		
		boolean fl;
		
		disablelangerror ();
		
		fl = langexpandtodotparams (bs, htable, bsname);
		
		enablelangerror ();
		
		if (!fl) {
			
			langparamerror (addresscoerceerror, bs);
			
			return (false);
			}
		
		if (*htable == nil)
			langsearchpathlookup (bsname, htable); /*always sets htable*/
		
		return (true);
		} /*odbexpandtodotparams*/



static boolean odbvaltotable (tyvaluerecord val, hdlhashtable *htable, hdlhashnode hnode) {
	
	if (!langexternalvaltotable (val, htable, hnode)) {
		
		bigstring bs;
		
		getstringlist (tableerrorlist, namenottableerror, bs);
		
		langerrormessage (bs);
		
		return (false);
		}
	
	return (true);		
	} /*odbvaltotable*/

pascal boolean odbUpdateOdbref (WindowPtr w, odbref odb) {
	hdlcancoonrecord hc;
	
	setemptystring (bserror);
	
	hc = (hdlcancoonrecord) odb;
	
	if (w != NULL) {
		shellpushglobals (w);
	
		(*shellglobals.setsuperglobalsroutine) ();
		}
	
	(**hc).hdatabase = databasedata; /*current database*/
	
	(**hc).hroottable = roottable; /*current root table*/
	
	(**hc).hrootvariable = rootvariable; /*current root variable*/
	
	(**hc).shellwindow = shellwindow;
	
	(**hc).accesssing = true;
		
	if (w != NULL) {
		shellpopglobals ();
	
		(*shellglobals.setsuperglobalsroutine) ();	
		}
	
	return (true);
	} /*odbUpdateOdbref*/

pascal boolean odbAccessWindow (WindowPtr w, odbref *odb) {
	
	/*
	4.1b5 dmb: new entrypoint
	
	for the db verbs withing frontier, we need to be able to "open" a 
	database that is already open. We keep our own hashtablestack; that's
	our context. But the root table and variable and the database are just 
	borrowed from the given root window.
	*/
		
	setemptystring (bserror);
	
	if (!newcancoonrecord (&cancoonglobals))
		return (false);
	
	*odb = (odbref) cancoonglobals;
	
	odbUpdateOdbref (w, *odb);
	return (true);
	} /*odbAccess*/


/*
 * odb_detect_cancoon_record
 *
 * Determines if the data at the given address is a v6 Cancoon record or a v7 root table.
 *
 * v6 Cancoon records have versionnumber = 2 or 3 in the first 2 bytes.
 * v7 databases point views[0] directly at the root table (no Cancoon record).
 *
 * Returns: true if Cancoon record detected, false if v7 root table
 */
__attribute__((unused))
static boolean odb_detect_cancoon_record(dbaddress adr) {
	short versionnumber;

	/* Read first 2 bytes to check version */
	if (!dbreference(adr, sizeof(versionnumber), &versionnumber)) {
		log_error(LOG_COMP_DB, "odb_detect_cancoon_record: dbreference failed at adr=0x%08llx",
		          (unsigned long long)adr);
		return false;
	}

	disktomemshort(versionnumber);

	log_debug(LOG_COMP_DB, "odb_detect_cancoon_record: adr=0x%08llx versionnumber=%d",
	          (unsigned long long)adr, versionnumber);

	/* Cancoon records have version 2 or 3 */
	if (versionnumber == 2 || versionnumber == cancoonversionnumber) {
		log_debug(LOG_COMP_DB, "odb_detect_cancoon_record: detected v6 Cancoon record (version=%d)",
		          versionnumber);
		return true;
	}

	/* Anything else is assumed to be a v7 root table header */
	log_debug(LOG_COMP_DB, "odb_detect_cancoon_record: detected v7 root table (non-Cancoon data)");
	return false;
}


pascal boolean odbNewFile (hdlfilenum fnum) {

	/*
	4.1b5 dmb: new routine. minimal db creation. does not leave it open

	2026-01-07 Codex: Updated for v7 format - creates minimal database instead of Cancoon record.
	Creates .root files with no Cancoon record, empty database.
	*/

	boolean fl;

	log_trace(LOG_COMP_DB, "odbNewFile: enter, fnum=%d", fnum);

	setemptystring (bserror);

	if (!dbnew (fnum, true)) {  /* Create v7 format for new databases */
		log_error(LOG_COMP_DB, "odbNewFile: dbnew failed");
		return (false);
	}

	log_debug(LOG_COMP_DB, "odbNewFile: dbnew succeeded - v7 database created");

	/*
	 * v7 format: Minimal database with no Cancoon record.
	 * views[0] will be set to nildbaddress for now (empty database).
	 * TODO: Create actual root table in future milestone.
	 */

	/* Set views[0] to nildbaddress (v7 pattern, no Cancoon, no root table yet) */
	dbsetview(cancoonview, nildbaddress);

	log_debug(LOG_COMP_DB, "odbNewFile: dbsetview(cancoonview=%d, adr=nildbaddress) for v7 minimal database",
	          cancoonview);

	/* Close the database */
	fl = dbclose();

	if (!fl) {
		log_error(LOG_COMP_DB, "odbNewFile: dbclose failed!");
	} else {
		log_debug(LOG_COMP_DB, "odbNewFile: dbclose succeeded");
	}

	cancoonglobals = nil;

	dbdispose();

	log_trace(LOG_COMP_DB, "odbNewFile: exit, success=%d", fl);

	return (fl);
	} /*odbNewFile*/


pascal boolean odbOpenFile (hdlfilenum fnum, odbref *odb, boolean flreadonly) {

	hdlcancoonrecord hc = nil;
	dbaddress adr;
	short versionnumber;
	unsigned char sourceFileVersion = 7;  /* Default to v7, will be updated if v6 detected */

	setemptystring (bserror);

#if defined(FRONTIER_HEADLESS)
	/* In headless tests, proactively migrate legacy headers to v7 before
	   invoking dbopenfile, to avoid mixed 32/64-bit on-disk layouts.
	   We detect legacy by peeking at byte 1 (versionnumber). */
	{
		extern const char* headless_fnum_path(hdlfilenum fnum);
		extern boolean headless_reopen_fnum(hdlfilenum fnum, const char *path, boolean flreadonly);
		const char *path = headless_fnum_path(fnum);
		if (path != NULL) {
			FILE *fp = fopen(path, "rb");
			if (fp) {
				unsigned char hdr[2];
				if (fread(hdr, 1, 2, fp) == 2) {
					sourceFileVersion = hdr[1];  /* Capture source version before migration */
#if defined(FRONTIER_HEADLESS)
					log_debug(LOG_COMP_DB, "odbOpenFile pre-open header: path=%s ver=%u", path, (unsigned)sourceFileVersion);
#endif
					if (sourceFileVersion <= 6) {
						fclose(fp);
					fp = NULL;  /* Prevent double-close later */
						if (!migrate_32bit_to_64bit(path))
							return (false);
						char migrated_path[1024];
						if (!db_format_last_migration_output_path(migrated_path, sizeof migrated_path))
							return (false);
						if (!headless_reopen_fnum(fnum, migrated_path, flreadonly))
							return (false);
						path = migrated_path;
					
					/* Re-read header to get migrated v7 version */
					fp = fopen(path, "rb");
					if (fp) {
						unsigned char migrated_hdr[2];
						if (fread(migrated_hdr, 1, 2, fp) == 2) {
							sourceFileVersion = migrated_hdr[1];
							log_debug(LOG_COMP_DB, "odbOpenFile post-migration: updated sourceFileVersion=%u", (unsigned)sourceFileVersion);
						}
						fclose(fp);
					}
					}
				}
				if (fp) fclose(fp);
			}
		}
	}
#endif

	
	if (!dbopenfile (fnum, flreadonly))
		return (false);

	/*
	 * Determine if this is a v6 database by checking the on-disk header.
	 * We can't rely on db_format_is_legacy_db() because the global legacy marker
	 * can be cleared by other database operations (e.g., system root re-opening).
	 *
	 * The sourceFileVersion captured from the file header tells us the source format.
	 */
	hdldatabaserecord hdb = databasedata;
	boolean isLegacy = (sourceFileVersion <= 6);

	log_debug(LOG_COMP_DB, "odbOpenFile: after dbopenfile, hdb=%p sourceVer=%u isLegacy=%d inMemVer=%d views[0]=0x%08llx",
	          (void*)hdb,
	          (unsigned)sourceFileVersion,
	          isLegacy,
	          (**hdb).versionnumber,
	          (unsigned long long)(**hdb).views[0]);

	dbgetview (cancoonview, &adr);

	log_debug(LOG_COMP_DB, "odbOpenFile: cancoonview=%d, adr=0x%08llx", cancoonview, (unsigned long long)adr);

	/*
	 * Determine format based on source database version (before legacy adaptation):
	 * - v6 source databases: views[0] points to Cancoon record (load legacy path)
	 * - v7 databases: views[0] is either nildbaddress (empty) or points to root table
	 *
	 * We use sourceFileVersion (captured from file header) to determine the source format,
	 * not the in-memory version which has been adapted to v7.
	 */
	if (isLegacy) {
		/* v6 database - load Cancoon record using legacy path */
		log_debug(LOG_COMP_DB, "odbOpenFile: v6 database detected, loading Cancoon record");

		/*
		 * Read v6 Cancoon data using legacy adapter's read path.
		 * The address has been adapted to v7 (0x4112), but the data on disk is still v6 format.
		 *
		 * The legacy adapter is active, which adapts v6 addresses to v7 space.
		 * We need to read through the adapter to get the v6 data correctly.
		 *
		 * CRITICAL: Don't try to force v6 mode here - the adapter handles the format translation.
		 * The adapted address (adr) already points to the right location through the adapter layer.
		 */
		log_debug(LOG_COMP_DB, "odbOpenFile: reading Cancoon record through legacy adapter at adr=0x%08llx",
		          (unsigned long long)adr);

		/* Read Cancoon version number - the adapter will handle the v6 format translation */
		if (!dbreference (adr, sizeof(versionnumber), &versionnumber))
			goto error;

		disktomemshort (versionnumber);

		log_debug(LOG_COMP_DB, "odbOpenFile: Cancoon versionnumber=%d (expecting 2 or 3)", versionnumber);

		if (!newcancoonrecord (&cancoonglobals))
			goto error;

		hc = cancoonglobals;
		(**hc).hdatabase = databasedata;

		switch (versionnumber) {
			case 2:
			case cancoonversionnumber:
				if (!loadversion2cancoonfile (adr, hc))
					goto error;

				*odb = (odbref) hc;

				log_debug(LOG_COMP_DB, "odbOpenFile: v6 Cancoon database opened successfully");
				return (true);

			default:
				log_error(LOG_COMP_DB, "odbOpenFile: unrecognized Cancoon version: %d", versionnumber);
				alertdialog ((ptrstring) "\x59" "The version number of this database file is not recognized by this version of Frontier.");
				goto error;
		}
	}
	else if (adr == nildbaddress) {
		/* v7 database with no root table yet - create empty root table */
		log_debug(LOG_COMP_DB, "odbOpenFile: v7 minimal database detected (nildbaddress)");

		if (!newcancoonrecord(&cancoonglobals))
			goto error;

		hc = cancoonglobals;
		(**hc).hdatabase = databasedata;

		/*
		 * CRITICAL P0 FIX (Issue #266): Create empty root table for new database.
		 *
		 * When system root is loaded and we open an empty guest database, we must
		 * create a root table for the guest database. Otherwise currenthashtable
		 * becomes nil and hash operations crash.
		 */
		tyvaluerecord val;
		hdlhashtable htable;
		if (!langexternalnewvalue(idtableprocessor, nil, &val))
			goto error;
		if (!langexternalvaltotable(val, &htable, nil)) {
			disposevaluerecord(val, false);
			goto error;
		}

		rootvariable = (Handle) val.data.externalvalue;
		(**hc).hrootvariable = rootvariable;
		(**hc).hroottable = roottable = htable;
		/* htablestack is already allocated by newcancoonrecord() */

		*odb = (odbref) hc;

		log_debug(LOG_COMP_DB, "odbOpenFile: v7 database opened with new root table");
		return (true);
	}
	else {
		/* v7 database with root table - Milestone 2 implementation */
		log_debug(LOG_COMP_DB, "odbOpenFile: v7 database with root table at adr=0x%llx", (unsigned long long)adr);

		Handle hvariable = nil;
		hdlhashtable htable = nil;

		/* Load the root table directly from views[0] */
		if (!tableloadsystemtable(adr, &hvariable, &htable, false)) {
			log_error(LOG_COMP_DB, "odbOpenFile: failed to load v7 root table");
			goto error;
		}

		/* Set up Cancoon record to hold the root table */
		if (!newcancoonrecord(&cancoonglobals))
			goto error;

		hc = cancoonglobals;
		(**hc).hdatabase = databasedata;
		(**hc).hrootvariable = rootvariable = hvariable;
		(**hc).hroottable = roottable = htable;
		/* htablestack is already allocated by newcancoonrecord() */

		cleartablestructureglobals();
		currenthashtable = roottable;

		*odb = (odbref) hc;

		log_debug(LOG_COMP_DB, "odbOpenFile: v7 database opened successfully");
		return (true);
	}
	
	error:
	
	dbdispose ();
	
	disposecancoonrecord (hc); /*checks for nil*/
	
	clearcancoonglobals ();
	
	return (false);
	} /*odbOpenFile*/


pascal Handle odbGetRootVariable (odbref odb) {

	if (odb == nil)
		return (nil);

	hdlcancoonrecord hc = (hdlcancoonrecord) odb;

	return ((Handle) (**hc).hrootvariable);
	} /*odbGetRootVariable*/


pascal boolean odbSaveFile (odbref odb) {

	hdlcancoonrecord hc = (hdlcancoonrecord) odb;
	tyversion2cancoonrecord info;
	dbaddress adr;

    setemptystring (bserror);

    /* Save implies migration to modern (v7) format. */
    if (!db_format_mode_current().use_64bit_format) {
        extern boolean db_migrate_reopen_if_legacy(odbref *podb);
        if (!db_migrate_reopen_if_legacy(&odb))
            return (false);
        /* After migration/reopen, refresh hc to point to new database */
        hc = (hdlcancoonrecord) odb;
    }

	setcancoonglobals (hc);

	if ((**hc).accesssing) {

		return (shellsave ((**hc).shellwindow));
		}


	/*
	 * v7 Database Save Path: No Cancoon Record
	 * =========================================
	 * v7 databases store the root table directly in views[0], with NO Cancoon record.
	 * v6 databases wrap the root table in a Cancoon record stored in views[0].
	 *
	 * For v7: Just save the root table and update views[0] directly.
	 * For v6: Read old Cancoon record, update root table pointer, write Cancoon back.
	 *
	 * CRITICAL: Use global rootvariable, NOT (**hc).hrootvariable!
	 * When db.setvalue() is called on an empty database, langexpandtodotparams creates
	 * the root table and sets the global rootvariable, but (**hc).hrootvariable remains nil.
	 */
	if (db_format_mode_current().use_64bit_format) {
		/* v7 database: Save root table directly to views[0] */
		dbaddress root_adr;

		log_debug(LOG_COMP_DB, "odbSaveFile: v7 database - saving root table directly");

		/*
		 * Handle empty database case: If rootvariable is nil, the database is empty
		 * (no data has been added yet). Set views[0] to nildbaddress and return success.
		 */
		if (rootvariable == nil) {
			log_debug(LOG_COMP_DB, "odbSaveFile: v7 database is empty, setting views[0] = nildbaddress");
			dbsetview(cancoonview, nildbaddress);
			return (true);
		}

		/* Update cancoon record to reflect current global state */
		(**hc).hrootvariable = rootvariable;
		(**hc).hroottable = roottable;

		/* Save the root table (use global rootvariable, which is always current) */
		{
			boolean repack_scope = false;
			db_format_mode mode = {true, true};  /* 64-bit, adapter_repack */
			db_format_mode_push(&mode);
			repack_scope = true;
			if (!tablesavesystemtable(rootvariable, &root_adr)) {
				if (repack_scope) {
					db_format_mode_pop();
					repack_scope = false;
				}
				log_error(LOG_COMP_DB, "odbSaveFile: failed to save v7 root table");
				return (false);
			}
			if (repack_scope) {
				db_format_mode_pop();
				repack_scope = false;
			}
		}

		/* Flush release stack */
		{
			db_context ctx;
			db_context_init(&ctx);
			dbflushreleasestack_context(&ctx);
		}

		/* Update views[0] to point directly at root table (v7 pattern) */
		dbsetview(cancoonview, root_adr);

		log_debug(LOG_COMP_DB, "odbSaveFile: v7 database saved, views[0] = 0x%llx", (unsigned long long)root_adr);
		return (true);
	}

	/*
	 * v6 Database Save Path: Cancoon Record
	 * ======================================
	 * v6 databases require reading the Cancoon record, updating the root table pointer,
	 * and writing the Cancoon record back.
	 */
	log_debug(LOG_COMP_DB, "odbSaveFile: v6 database - using Cancoon record save path");

	dbgetview (cancoonview, &adr);

	if (adr == nildbaddress)
		return (false);

	if (!dbreference (adr, sizeof (info), &info))
		return (false);

	info.versionnumber = conditionalshortswap (cancoonversionnumber);

    {
        boolean repack_scope = false;
        db_format_mode mode = {true, true};  /* 64-bit, adapter_repack */
        db_format_mode_push(&mode);
        repack_scope = true;
        if (!tablesavesystemtable((**hc).hrootvariable, &info.adrroottable)) {
            if (repack_scope) {
                db_format_mode_pop();
                repack_scope = false;
            }
            return (false);
        }
        if (repack_scope) {
            db_format_mode_pop();
            repack_scope = false;
        }
    }

	db_format_write_be32(&info.adrroottable, (uint32_t) info.adrroottable);

	clearbytes (&info.waste, sizeof (info.waste));

    {
        boolean repack_scope = false;
        db_format_mode mode = {true, true};  /* 64-bit, adapter_repack */
        db_format_mode_push(&mode);
        repack_scope = true;
        if (!dbassign(&adr, sizeof (info), &info)) {
            if (repack_scope) {
                db_format_mode_pop();
                repack_scope = false;
            }
            return (false);
        }
        if (repack_scope) {
            db_format_mode_pop();
            repack_scope = false;
        }
    }

	{
		db_context ctx;
		db_context_init(&ctx);
		dbflushreleasestack_context(&ctx); /*release all the db objects that were saved up*/
	}

	dbsetview (cancoonview, adr);

	return (true);
	} /*odbSaveFile*/


pascal boolean odbCompactDatabase (odbref odb, const char *dst_path) {

	/*
	 * v7→v7 compaction. Writes a freshly-compacted copy of the source database
	 * to dst_path. The result has no avail-list dead space, so the destination
	 * file is the minimum size required to represent the live data.
	 *
	 * Preconditions:
	 *   - odb is non-nil and points at an open v7 database
	 *   - The in-memory root variable for odb is loaded
	 *   - dst_path is writable and does not currently exist (caller checks)
	 *
	 * Side effects:
	 *   - Mutates the in-memory tree's oldaddress fields to point at dst's
	 *     address space. After this returns, the source's IN-MEMORY state is
	 *     indeterminate — caller must close + reopen the source to keep using
	 *     it. The source's ON-DISK bytes are not modified.
	 *   - Sets cancoonglobals to nil on exit (caller will restore via guard
	 *     in dbverbs.c::odbcompactdatabase).
	 *
	 * Returns true on success.
	 */

	hdlcancoonrecord hc = (hdlcancoonrecord) odb;
	Handle source_root = nil;
	Handle source_script = nil;
	boolean ok = false;

	setemptystring (bserror);

	if (hc == nil) {
		log_error (LOG_COMP_DB, "odbCompactDatabase: odb is nil");
		return (false);
	}

	/* Make this odb the active one (sets databasedata, rootvariable, roottable,
	 * and odbengine's private cancoonglobals to hc). */
	setcancoonglobals (hc);

	source_root = (Handle) (**hc).hrootvariable;
	/* odbengine's tycancoonrecord (in odbengine.c) has no hscriptstring field;
	 * the quickscript handle lives only on the GUI cancoon record. Guest dbs
	 * opened via db.open never have an associated script string, so we always
	 * pass nil for source_script. */
	source_script = nil;

	if (source_root == nil) {
		log_error (LOG_COMP_DB, "odbCompactDatabase: source has no in-memory root variable");
		clearcancoonglobals ();
		return (false);
	}

	/* Run the compaction. The helper writes to dst_path, mutates the in-memory
	 * tree's oldaddress fields, and disposes the destination's database handle.
	 * It also disposes the source's in-memory root variable handle. */
	ok = db_format_compact_to_path ((**hc).hdatabase, source_root, source_script, dst_path);

	/* The compaction disposed the source's in-memory root. Clear the cancoon
	 * record's stale references so anyone holding hc doesn't follow them. */
	(**hc).hrootvariable = nil;
	(**hc).hroottable = nil;

	/* Clear our cancoonglobals so callers that didn't use a guard see a clean
	 * state. Caller in dbverbs.c uses odb_context_guard to restore proper
	 * globals. */
	clearcancoonglobals ();

	return (ok);
	} /*odbCompactDatabase*/


pascal boolean odbCloseFile (odbref odb) {

	/*
	1/22/91 dmb: added scan of new ccglobalsstack

	2/26/93 dmb: support shutdown scripts
	*/

	hdlcancoonrecord hc = (hdlcancoonrecord) odb;

	setemptystring (bserror);

	if (hc == nil) /*nothing to do*/
		return (true);

	setcancoonglobals (hc);

	if (!(**hc).accesssing)
		dbdispose (); /*do before clearing globals -- depends on databasedata*/

	disposecancoonrecord (hc);

	clearcancoonglobals ();

	return (true);
	} /*odbCloseFile*/


pascal boolean odbDefined (odbref odb, bigstring bspath) {

	/*
	4.1b5 dmb: new routine
	*/
	
	hdlhashtable htable;
	bigstring bsname;
	boolean fl;
	
	setemptystring (bserror);
	
	setcancoonglobals ((hdlcancoonrecord) odb);
	
	disablelangerror ();
	
	fl = odbexpandtodotparams (bspath, &htable, bsname);
	
	enablelangerror ();
	
	if (fl) {
		
		pushhashtable (htable);
		
		fl = hashsymbolexists (bsname);
		
		pophashtable ();
		}
	
	return (fl);
	} /*odbDefined*/


pascal boolean odbDelete (odbref odb, bigstring bspath) {

	hdlhashtable htable;
	bigstring bsname;
	
	setemptystring (bserror);
	
	setcancoonglobals ((hdlcancoonrecord) odb);
	
	if (!odbexpandtodotparams (bspath, &htable, bsname))
		return (false);
	
	return (hashtabledelete (htable, bsname));
	} /*odbDelete*/


pascal boolean odbGetType (odbref odb, bigstring bspath, OSType *odbType) {
	
	hdlhashtable htable;
	bigstring bsname;
	tyvaluerecord val;
	hdlhashnode hnode;
	
	setemptystring (bserror);
	
	setcancoonglobals ((hdlcancoonrecord) odb);
	
	if (!odbexpandtodotparams (bspath, &htable, bsname))
		return (false);
	
	if (!langsymbolreference (htable, bsname, &val, &hnode))
		return (false);
	
	if (val.valuetype == binaryvaluetype)
		*odbType = getbinarytypeid (val.data.binaryvalue);
	else
		*odbType = langexternalgettypeid (val);
	
	return (true);
	} /*odbGetType*/


pascal boolean odbGetValue (odbref odb, bigstring bspath, odbValueRecord *value) {

	hdlhashtable htable;
	bigstring bsname;
	tyvaluerecord val;
	hdlhashnode hnode;
	
	setemptystring (bserror);
	
	setcancoonglobals ((hdlcancoonrecord) odb);
	
	if (!odbexpandtodotparams (bspath, &htable, bsname))
		return (false);
	
	if (!langsymbolreference (htable, bsname, &val, &hnode))
		return (false);
	
	
	if (!copyvaluerecord (val, &val))
		return (false);
	
	if (!copyvaluedata (&val))
		return (false);
	
	exemptfromtmpstack (&val);
	
	(*value).valuetype = (odbValueType) langexternalgettypeid (val);
	
	/*
	if (val.valuetype == binaryvaluetype)
		pullfromhandle (val.data.binaryvalue, 0L, sizeof (typeid), &(*value).valuetype);
	*/
	
	(*value).data.binaryvalue = val.data.binaryvalue; /*largest field covers everything*/
	
	return (true);
	} /*odbGetValue*/


pascal boolean odbSetValue (odbref odb, bigstring bspath, odbValueRecord *value) {

	hdlhashtable htable;
	bigstring bsname;
	tyvaluerecord val;
	tyvaluetype type = langexternalgetvaluetype (value->valuetype);
	hdlcancoonrecord hc = (hdlcancoonrecord) odb;

	setemptystring (bserror);


	setcancoonglobals (hc);

	if (!odbexpandtodotparams (bspath, &htable, bsname))
		return (false);
	
	if (type == (tyvaluetype) -1) {
	
		if (!setbinaryvalue (value->data.binaryvalue, value->valuetype, &val))
			return (false);
		}
	else {
	
		initvalue (&val, type);
		
		val.data.binaryvalue = value->data.binaryvalue;
		}
	
	if (!copyvaluerecord (val, &val))
		return (false);
	
	if (!hashtableassign (htable, bsname, val)) {
	
		disposevaluerecord (val, true);
		
		return (false);
		}
	
	exemptfromtmpstack (&val);
	
	return (true);	
	} /*odbSetValue*/


pascal boolean odbNewTable (odbref odb, bigstring bspath) {

	hdlhashtable htable;
	bigstring bsname;
	tyvaluerecord val;
	
	setemptystring (bserror);
	
	setcancoonglobals ((hdlcancoonrecord) odb);
	
	if (!odbexpandtodotparams (bspath, &htable, bsname))
		return (false);
	
	if (!langexternalnewvalue (idtableprocessor, nil, &val))
		return (false);
	
	if (!hashtableassign (htable, bsname, val)) {
	
		disposevaluerecord (val, true);
		
		return (false);
		}
	
	exemptfromtmpstack (&val);
	
	return (true);	
	} /*odbNewTable*/


pascal boolean odbCountItems (odbref odb, bigstring bspath, long *count) {

	hdlhashtable htable;
	bigstring bsname;
	tyvaluerecord val;
	long ctitems;
	hdlhashnode hnode;
	
	setemptystring (bserror);
	
	setcancoonglobals ((hdlcancoonrecord) odb);
	
	if (!odbexpandtodotparams (bspath, &htable, bsname))
		return (false);
	
	if (!langsymbolreference (htable, bsname, &val, &hnode))
		return (false);
	
	if (!odbvaltotable (val, &htable, hnode))
		return (false);
	
	if (!hashcountitems (htable, &ctitems))
		return (false);
	
	*count = ctitems;
	
	return (true);
	} /*odbCountItems*/


pascal boolean odbGetNthItem (odbref odb, bigstring bspath, long n, bigstring bsname) {

	hdlhashtable htable;
	tyvaluerecord val;
	hdlhashnode hnode;
	
	setemptystring (bserror);
	
	setcancoonglobals ((hdlcancoonrecord) odb);
	
	if (!odbexpandtodotparams (bspath, &htable, bsname))
		return (false);
	
	if (!langsymbolreference (htable, bsname, &val, &hnode))
		return (false);
	
	if (!odbvaltotable (val, &htable, hnode))
		return (false);
	
	if (!hashgetiteminfo (htable, (short) (n - 1), bsname, nil))
		return (false);
	
	return (true);
	} /*odbGetNthItem*/



pascal boolean odbGetModDate (odbref odb, bigstring bspath, int64_t *date) {

	hdlhashtable htable, htableitem;
	bigstring bsname;
	tyvaluerecord val;
	hdlhashnode hnode;

	setemptystring (bserror);

	setcancoonglobals ((hdlcancoonrecord) odb);

	if (!odbexpandtodotparams (bspath, &htable, bsname))
		return (false);

	if (!langsymbolreference (htable, bsname, &val, &hnode))
		return (false);

	if (hnode == NULL)  /* defensive check */
		return (false);

	/* If the item is a table, return its timelastsave.
	   Otherwise, return the parent table's timelastsave. */
	if (langexternalvaltotable (val, &htableitem, hnode)) {
		*date = (**htableitem).timelastsave;
	}
	else {
		*date = (**htable).timelastsave;
	}

	return (true);
	} /*odbGetModDate*/


pascal void odbDisposeValue (odbref odb, odbValueRecord *value) {
	
	tyvaluetype type;
	tyvaluerecord val;
	
	setemptystring (bserror);
	
	setcancoonglobals ((hdlcancoonrecord) odb);
	
	type = langexternalgetvaluetype ((OSType) (*value).valuetype);
	
	if (type == -1)	/*no match; must have been a binary value*/
		type = binaryvaluetype;
	
	initvalue (&val, type);
	
	val.data.binaryvalue = (*value).data.binaryvalue;
	
	disposevaluerecord (val, false);
	} /*odbDisposeValue*/


pascal void odbGetError (bigstring bs) {

	copystring (bserror, bs);
	} /*odbGetError*/
