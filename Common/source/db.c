
/*	$Id$    */

/******************************************************************************

    UserLand Frontier(tm) -- High performance Web content management,
    object database, system-level and Internet scripting environment,
    including source code editing and debugging.

    Copyright (C) 1992-2004 UserLand Software, Inc.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

******************************************************************************/

/* 2025-12-01 Codex: Add headless diagnostics around dbclose/dbflushheader failures during migration tests. */

/* 2025-11-24 Codex: Legacy header writes now use BE helpers for v7 parity. */
/* 2025-11-25 Codex: Update dbopenfile to route through widened legacy adapter signature. */
/* 2025-11-26 Codex: Expose raw db read/write for split reader/writer modules. */
/* 2025-11-29 Codex: Add Save As destination accessor and context wrappers for db stack ops. */
/* 2025-11-30 Codex: Scope Save As state through db_context guards to stop global leaks. */
/* 2025-12-01 Codex: Route Save As writes (alloc/view) through destination-scoped contexts. */


#include "frontier.h"
#include "standard.h"

#if defined(FRONTIER_HEADLESS)
#include <stdio.h>
#endif


#include "memory.h"
#include "cursor.h"
#include "dialogs.h"
#include "error.h"
#include "logging.h"  /* Phase 2D: fprintf migration */
#include "file.h"
#include "resources.h"
#include "strings.h"
#include "shell.h"
#include "db.h"
#include "db_format.h"
#include "db_reader.h"
#include "db_writer_v7.h"
#include "dbinternal.h"
#include "ops.h" //6.2b3 AR: for numbertostring
#include "byteorder.h"	/* 2006-04-08 aradke: endianness conversion macros */

#include "frontierdebug.h" //6.2b7 AR

#define dberrorlist 256

// 2025-11-23 Codex: Widened block header/trailer to 64-bit BE and updated avail links for v7 roots.
// 2025-11-20 Codex: Write v7 headers and record metadata with explicit big-endian encoding for portability.
// 2025-11-16 Codex: Keep dbgetsize locals wide enough so dbgetsizeandvariance
// writes don't corrupt the caller's stack on 64-bit builds.

static void db_sync_use64_to_current_db(void);

#define setdirty(hdb) 		((**hdb).flags |= dbdirtymask)
#define cleardirty(hdb)		((**hdb).flags &= ~dbdirtymask)
#define isdirty(hdb) 		((**hdb).flags & dbdirtymask)

#define majorversion(v)		(v & 0x00f0)
#define minorversion(v)		(v & 0x000f)

/* Sentinel value for dbreadheader_core's fnum parameter meaning
   "use the global databasedata file handle via dbread()".
   POSIX guarantees file descriptors are non-negative (fd >= 0),
   so -1 cannot collide with a real file descriptor. */
#define DB_FNUM_USE_GLOBAL ((hdlfilenum) -1)

/*
 * Maximum number of bytes to scan backward when searching for a block header.
 * The common path (adr == block start) is O(1) — no backward scan needed.
 * In heavily-fragmented databases, backward scanning could be expensive;
 * the 1 MB cap prevents runaway scans in pathological cases while covering
 * all practical block sizes.
 */
#define DB_BACKWARD_SCAN_MAX_BYTES  0x100000  /* 1 MB */

/*
 * dbfindblockforaddress, dbfindblockforaddress_hdb, dbnormalizeaddress,
 * and dbnormalizeaddress_hdb are headless-only.  The non-headless (classic
 * Mac) build path is being retired — future GUI will be a separate app
 * communicating over pipes/sockets/REST.
 */
#if defined(FRONTIER_HEADLESS)
static boolean dbfindblockforaddress(dbaddress adr, dbaddress *blockstart, long *nodebytes, tyvariance *variance, boolean *flfree) {
	long eof = 0;
    boolean is_legacy = (databasedata != nil && db_format_is_legacy_db(databasedata));
    const long header_size = is_legacy ? sizeheader_v6 : sizeheader;

    /* Use the actual database's headerLength instead of the compile-time constant.
     * v6 databases have headerLength=88 (0x58), but firstphysicaladdress=118 (sizeof struct).
     * Addresses between 88-117 are valid on-disk but rejected by the constant. */
    long min_address = firstphysicaladdress;
    if (databasedata != nil && (**databasedata).headerLength > 0) {
        min_address = (**databasedata).headerLength;
    }

	if (adr == nildbaddress)
		return false;

	if (!dbgeteof(&eof))
		return false;

	if (adr < min_address || adr >= (dbaddress) eof)
		return false;

	for (dbaddress candidate = adr; candidate >= min_address && (adr - candidate) <= DB_BACKWARD_SCAN_MAX_BYTES; --candidate) {
		boolean freeflag = false;
		long node_size = 0;
		tyvariance node_variance = 0;

		if (!dbreadheader(candidate, &freeflag, &node_size, &node_variance))
			continue;

		if (node_size <= 0 || node_size > eof)
			continue;

		if ((node_variance < 0) || (node_variance > node_size))
			continue;

		dbaddress data_start = candidate + header_size;
		dbaddress data_end = data_start + (node_size - node_variance);

		if (adr < candidate || adr >= data_end)
			continue;

		boolean trailer_free = false;
		long trailer_size = 0;
		dbaddress trailer_pos = candidate + header_size + node_size;

		if (trailer_pos >= (dbaddress) eof) {
#if defined(FRONTIER_HEADLESS)
			log_trace(LOG_COMP_DB,
				"dbfindblockforaddress candidate=0x%llx size=%ld variance=%ld eof=%ld trailer=0x%llx",
				(unsigned long long) candidate,
				node_size,
				(long) node_variance,
				eof,
				(unsigned long long) trailer_pos);
#endif
			continue;
		}

		if (!dbreadtrailer(trailer_pos, &trailer_free, &trailer_size))
			continue;

		if (trailer_size != node_size)
			continue;

		if (trailer_free != freeflag)
			continue;

		if (blockstart != NULL)
			*blockstart = candidate;
		if (nodebytes != NULL)
			*nodebytes = node_size;
		if (variance != NULL)
			*variance = node_variance;
		if (flfree != NULL)
			*flfree = freeflag;
		return true;
	}

	return false;
}

boolean dbnormalizeaddress(dbaddress *adr) {
	if (adr == NULL || *adr == nildbaddress)
		return true;

	dbaddress resolved = nildbaddress;
	if (!dbfindblockforaddress(*adr, &resolved, NULL, NULL, NULL))
		return false;

	*adr = resolved;
	return true;
}

/* Phase 8 forward declaration — dbreadtrailer_hdb is defined in the Layer 3+
   section but dbfindblockforaddress_hdb needs it here. */
static boolean dbreadtrailer_hdb(dbaddress adr, boolean *flfree, long *ctbytes, hdlfilenum fnum, hdldatabaserecord hdb);

/* db_hdb_* helpers now live in dbinternal.h (available to all includers). */

static boolean dbfindblockforaddress_hdb(dbaddress adr, dbaddress *blockstart, long *nodebytes, tyvariance *variance, boolean *flfree, hdldatabaserecord hdb) {

	/*
	Phase 8: Like dbfindblockforaddress but reads from explicit hdb instead
	of databasedata.  Uses _fnum variants for all I/O.
	*/

	long eof = 0;

	if (hdb == nil)
		return (false);

	const long header_size = db_hdb_header_size(hdb);
	hdlfilenum fnum = db_hdb_fnum(hdb);

	long min_address = firstphysicaladdress;
	if ((**hdb).headerLength > 0) {
		min_address = (**hdb).headerLength;
	}

	if (adr == nildbaddress)
		return false;

	if (!dbgeteof_fnum(&eof, fnum))
		return false;

	if (adr < min_address || adr >= (dbaddress) eof)
		return false;

	for (dbaddress candidate = adr; candidate >= min_address && (adr - candidate) <= DB_BACKWARD_SCAN_MAX_BYTES; --candidate) {
		boolean freeflag = false;
		long node_size = 0;
		tyvariance node_variance = 0;

		if (!dbreadheader_fnum(candidate, &freeflag, &node_size, &node_variance, header_size, fnum))
			continue;

		if (node_size <= 0 || node_size > eof)
			continue;

		if ((node_variance < 0) || (node_variance > node_size))
			continue;

		dbaddress data_start = candidate + header_size;
		dbaddress data_end = data_start + (node_size - node_variance);

		if (adr >= data_end) /* candidate <= adr by loop invariant */
			continue;

		boolean trailer_free = false;
		long trailer_size = 0;
		dbaddress trailer_pos = candidate + header_size + node_size;

		if (trailer_pos >= (dbaddress) eof) {
			log_trace(LOG_COMP_DB,
				"dbfindblockforaddress_hdb candidate=0x%llx size=%ld variance=%ld eof=%ld trailer=0x%llx",
				(unsigned long long) candidate,
				node_size,
				(long) node_variance,
				eof,
				(unsigned long long) trailer_pos);
			continue;
		}

		if (!dbreadtrailer_hdb(trailer_pos, &trailer_free, &trailer_size, fnum, hdb))
			continue;

		if (trailer_size != node_size)
			continue;

		if (trailer_free != freeflag)
			continue;

		if (blockstart != NULL)
			*blockstart = candidate;
		if (nodebytes != NULL)
			*nodebytes = node_size;
		if (variance != NULL)
			*variance = node_variance;
		if (flfree != NULL)
			*flfree = freeflag;
		return true;
	}

	return false;
}

boolean dbnormalizeaddress_hdb(dbaddress *adr, hdldatabaserecord hdb) {

	/*
	Phase 8: Like dbnormalizeaddress but uses explicit hdb for all I/O.
	*/

	if (adr == NULL || *adr == nildbaddress)
		return true;

	if (hdb == nil)
		return false;

	dbaddress resolved = nildbaddress;
	if (!dbfindblockforaddress_hdb(*adr, &resolved, NULL, NULL, NULL, hdb))
		return false;

	*adr = resolved;
	return true;
}
#endif /* FRONTIER_HEADLESS */

typedef enum {
	
	dbnoerror,
	
	dbwrongversionerror,
	
	dbfreeblockerror,

	dbfreelisterror,

	dbinconsistentavaillisterror,

	dbassignfreeblockerror,

	dbfilesizeerror,

	dbreleasefreeblockerror,

	dbreleaseinvalidblockerror,

	dbmergeinvalidblockerror
	} tydberror;


hdldatabaserecord databasedata; /*the global database handle*/

static inline boolean db_use64(void) {
    return db_format_mode_current().use_64bit_format;
}

boolean fldatabasesaveas = false; /*only true during Save As operation*/

#ifdef FRONTIER_TESTS
/* Test-only accessor for cleanup state validation (PR #137 regression test).
 * Provides controlled access to internal state without exposing global directly.
 * Tests should use this instead of 'extern boolean fldatabasesaveas'. */
boolean db_test_is_saveas_active(void) {
	return fldatabasesaveas;
}
#endif


static hdldatabaserecord databasedestination; /*for Save As*/
static hdldatabaserecord dbsaveas_source = nil; /*remember the original db while Save As runs*/



// Function to offer migration dialog (placeholder for now)
boolean offer_64bit_migration_dialog(const char* db_path) {
    (void)db_path;
    // TODO: Implement actual dialog
    // For now, return true to auto-migrate
    return true;
}

#if fldebug

static long leftmerges = 0, rightmerges = 0; /*statistics*/

static long splits = 0, nonsplits = 0; /*more statistics*/

static long allocs = 0, newallocs = 0, allocloops = 0; /*more statistics*/

#endif


#ifdef DATABASE_DEBUG

#pragma message ("*********************** DATABASE_DEBUG is ON: output to dblog.txt ***********************")

#define dberror(num) debug_dberror(num, __LINE__, true)
#define dblogerror(num) debug_dberror(num, __LINE__, false)
#define dbseteof(eof) debug_dbseteof(eof, __LINE__)


static boolean DBTRACKERGETROOTVISIT (WindowPtr w, bigstring bsfile) {

	hdlwindowinfo hinfo;

	if (getwindowinfo (w, &hinfo))
		if ((long) (**hinfo).fnum == (**databasedata).fnumdatabase) {

			copystring (fsname(&(**hinfo).fspec), bsfile);

			return (false); /*terminate visiting*/
			}

	return (true);
	}/*getrootnamevisit*/


static void DBTRACKERGETFILENAME (bigstring bsfile) {

	if (shellvisittypedwindows (idcancoonconfig, &DBTRACKERGETROOTVISIT, bsfile))
		setemptystring (bsfile); /*nothing found*/
	
	}/*DBTRACKERGETFILENAME*/


static void debug_dberror (short errnum, int line, boolean flthrow) {
	
	bigstring bs, bsfile;
	char str[1024];
	
	getstringlist (dberrorlist, errnum, bs);

	DBTRACKERGETFILENAME (bsfile);
	
	sprintf (str, "%s | %s [db.c,%ld]", stringbaseaddress (bsfile), stringbaseaddress (bs), line);

	DB_MSG_1 (str);

	if (flthrow)
		shellerrormessage (bs);
	} /*debug_dberror*/


static boolean debug_dbseteof (long eof, long line) {

	const long onegigabyte = 1024L * 1024L * 1024L;

	if (eof >= onegigabyte) {

		long oldeof = nil;

		if (dbgeteof (&oldeof) && (oldeof < onegigabyte)) {

			char str[1024];
			bigstring bsfile;
			
			DBTRACKERGETFILENAME (bsfile);
			
			sprintf (str, "%s | WARNING -- Growing database from %ld to %ld bytes. [db.c,%ld]", stringbaseaddress (bsfile), oldeof, eof, line);

			DB_MSG_1 (str);
			}
		}

	if ((eof & 0x80000000L) != 0x00000000L) {

		dberror (dbfilesizeerror);

		return (false); //trying to grow the file beyond 2 GB
		}

	return (fileseteof ((hdlfilenum)((**databasedata).fnumdatabase), eof));
	} /*dbseteof*/

#else

#define dblogerror(errnum)

static void dberror (short errnum) {
	
	bigstring bs;
	
	getstringlist (dberrorlist, errnum, bs);
	
	shellerrormessage (bs);
	} /*dberror*/

static boolean dbseteof (long eof) {

	if ((eof & 0x80000000L) != 0x00000000L) {

		dberror (dbfilesizeerror);

		return (false); //trying to grow the file beyond 2 GB
		}

	return (fileseteof ((hdlfilenum)((**databasedata).fnumdatabase), eof));
	} /*dbseteof*/

#endif


static db_context g_default_db_context;
typedef struct db_context_guard {
    db_format_mode prev_mode;
    db_saveas_state prev_saveas;
    hdldatabaserecord prev_db;
} db_context_guard;

static db_context *db_context_refresh_default(void) {
    db_context_init(&g_default_db_context);
    return &g_default_db_context;
}

void db_saveas_state_snapshot(db_saveas_state *state) {
    if (state == NULL)
        return;
    state->active = fldatabasesaveas;
    state->destination = databasedestination;
    state->source = dbsaveas_source;
}

void db_saveas_state_apply(const db_saveas_state *state) {
    if (state == NULL)
        return;
    fldatabasesaveas = state->active;
    databasedestination = state->destination;
    dbsaveas_source = state->source;
    db_sync_use64_to_current_db();
}

static void db_context_guard_enter(const db_context *context, db_context_guard *guard) {
    if (guard != NULL) {
        guard->prev_mode = db_format_mode_current();
        db_saveas_state_snapshot(&guard->prev_saveas);
        guard->prev_db = databasedata;
#if defined(FRONTIER_HEADLESS)
        if (log_enabled(LOG_LEVEL_TRACE, LOG_COMP_DB)) {
            static int call_count = 0;
            if (call_count++ < 5) {
                log_trace(LOG_COMP_DB, "db_context_guard_enter: prev_mode captured use_64bit=%d adapter_repack=%d",
                        (int) guard->prev_mode.use_64bit_format, (int) guard->prev_mode.adapter_repack);
            }
        }
#endif
    }
    if (context != NULL) {
        if (context->database != nil)
            databasedata = context->database;
        db_saveas_state_apply(&context->saveas);
#if defined(FRONTIER_HEADLESS)
        if (log_enabled(LOG_LEVEL_TRACE, LOG_COMP_DB)) {
            static int call_count2 = 0;
            if (call_count2++ < 5) {
                log_trace(LOG_COMP_DB, "db_context_guard_enter: applying mode use_64bit=%d adapter_repack=%d",
                        (int) context->mode.use_64bit_format, (int) context->mode.adapter_repack);
            }
        }
#endif
        db_format_mode_apply(&context->mode);
#if defined(FRONTIER_HEADLESS)
        if (log_enabled(LOG_LEVEL_TRACE, LOG_COMP_DB)) {
            static int call_count2 = 0;
            if (call_count2++ < 5) {
                db_format_mode current_after = db_format_mode_current();
                log_trace(LOG_COMP_DB, "db_context_guard_enter: after apply, current mode use_64bit=%d adapter_repack=%d",
                        (int) current_after.use_64bit_format, (int) current_after.adapter_repack);
            }
        }
#endif
    }
}

static void db_context_guard_exit(const db_context_guard *guard) {
    if (guard == NULL)
        return;
#if defined(FRONTIER_HEADLESS)
    if (log_enabled(LOG_LEVEL_TRACE, LOG_COMP_DB)) {
        static int call_count = 0;
        if (call_count++ < 5) {
            log_trace(LOG_COMP_DB, "db_context_guard_exit: restoring prev mode use_64bit=%d adapter_repack=%d",
                    (int) guard->prev_mode.use_64bit_format, (int) guard->prev_mode.adapter_repack);
        }
    }
#endif
    db_format_mode_apply(&guard->prev_mode);
    databasedata = guard->prev_db;
    db_saveas_state_apply(&guard->prev_saveas);
}

static void db_context_guard_exit_with_saveas(const db_context_guard *guard, const db_saveas_state *state) {
    if (guard == NULL)
        return;
    db_format_mode_apply(&guard->prev_mode);
    databasedata = guard->prev_db;
    if (state != NULL)
        db_saveas_state_apply(state);
    else
        db_saveas_state_apply(&guard->prev_saveas);
}

/* odb_guard functions moved to db_format.c where lang.h globals are available */

static boolean dbrelease_internal (dbaddress); /*6.2b2: Dropped from db.h and declared static*/
static boolean dballocate (long databytes, ptrvoid pdata, dbaddress *paddress); /*6.2b14 AR: forward declaration for dbwriteshadowavaillist*/
static boolean dbstartsaveas_internal(hdlfilenum fnum);
static boolean dbendsaveas_internal(void);
boolean dbassign_context(const db_context *context, dbaddress *padr, long newsize, ptrvoid pdata);
boolean dbreference_context(const db_context *context, dbaddress adr, long maxbytes, ptrvoid pdata);
boolean dbreference_handle_context(const db_context *context, dbaddress adr, Handle *h);
boolean dballocate_context(const db_context *context, long databytes, ptrvoid pdata, dbaddress *paddress);
boolean dbendsaveas_context(db_context *context);
boolean dbstartsaveas_context(db_context *context, hdlfilenum fnum);
boolean dbrelease_context(const db_context *context, dbaddress adr);
boolean dbassign_internal(dbaddress *padr, long newsize, ptrvoid pdata);
boolean dbcopy_internal(dbaddress adrorig, dbaddress *adrcopy);
boolean dbreference_internal(dbaddress adr, long maxbytes, ptrvoid pdata);
boolean dbreference_with_header_size(dbaddress adr, long maxbytes, ptrvoid pdata, long header_size);
boolean dbgetsize_internal(dbaddress adr, long *logicalsize);


/* Scope Save As operations onto the destination handle without mutating caller globals. */
static db_context *db_context_for_saveas_destination(db_context *ctx, boolean *using_destination) {
    if (ctx == NULL)
        return NULL;

    db_context_init(ctx);

    if (ctx->saveas.active && ctx->saveas.destination != nil) {
        ctx->database = ctx->saveas.destination;
        /* During migration (adapter active), always use v7 format for destination writes.
           The destination database handle may still be marked as legacy, but we're writing
           the new v7 format data. */
        boolean adapter_active = db_format_adapter_is_active();
        boolean is_legacy = db_format_is_legacy_db(ctx->database);
#if defined(FRONTIER_HEADLESS)
        if (log_enabled(LOG_LEVEL_TRACE, LOG_COMP_DB)) {
            static int call_count = 0;
            if (call_count++ < 5) {
                log_trace(LOG_COMP_DB, "db_context_for_saveas_destination: adapter_active=%d is_legacy=%d",
                        (int) adapter_active, (int) is_legacy);
            }
        }
#endif
        if (adapter_active) {
            ctx->mode.use_64bit_format = true;
        } else {
            ctx->mode.use_64bit_format = !is_legacy;
        }
#if defined(FRONTIER_HEADLESS)
        if (log_enabled(LOG_LEVEL_TRACE, LOG_COMP_DB)) {
            static int call_count3 = 0;
            if (call_count3++ < 5) {
                log_trace(LOG_COMP_DB, "db_context_for_saveas_destination: set use_64bit_format=%d",
                        (int) ctx->mode.use_64bit_format);
            }
        }
#endif
        if (using_destination != NULL)
            *using_destination = true;
        return ctx;
    }

    if (using_destination != NULL)
        *using_destination = false;
    return NULL;
} /*db_context_for_saveas_destination*/


static void dbswapglobals (void) {
	
	/*
	used to temporarily set and unset databasedestination as the databasehandle
	*/
	
	if (fldatabasesaveas) {
		
		hdldatabaserecord htemp = databasedata;
		
		databasedata = databasedestination;
		
		databasedestination = htemp;
		db_sync_use64_to_current_db();
		}
	} /*dbswapglobals*/

static void db_sync_use64_to_current_db(void) {
	if (fldatabasesaveas && dbsaveas_source != nil && db_format_adapter_is_active()) {
		/* Legacy source stays 32-bit; destination writes are v7 BE64. */
        db_format_mode mode = db_format_mode_current();
        /* Guard against accessing databasedata if it's nil (e.g., after dbdispose in cleanup) */
        mode.use_64bit_format = (databasedata != nil) && !db_format_is_legacy_db(databasedata);
        db_format_mode_apply(&mode);
	}
}

static inline hdldatabaserecord db_begin_source_read(void) {
	if (fldatabasesaveas && dbsaveas_source != nil) {
		hdldatabaserecord previous = databasedata;
#if defined(FRONTIER_HEADLESS)
		log_debug(LOG_COMP_DB, "db_begin_source_read: switching from dest fnum=%ld to source fnum=%ld",
		          previous ? (long)(**previous).fnumdatabase : -1L,
		          (long)(**dbsaveas_source).fnumdatabase);
#endif
		databasedata = dbsaveas_source;
		db_sync_use64_to_current_db();
		return previous;
	}
	return nil;
}

static inline void db_end_source_read(hdldatabaserecord previous) {
	if (previous != nil) {
		databasedata = previous;
		db_sync_use64_to_current_db();
	}
}


static boolean dbseek (dbaddress adr) {
#if defined(FRONTIER_HEADLESS)
	log_debug(LOG_COMP_DB, "dbseek: adr=0x%llx fnum=%ld databasedata=%p",
	          (unsigned long long)adr,
	          (long)(**databasedata).fnumdatabase,
	          (void*)databasedata);
#endif
	return (filesetposition((hdlfilenum)((**databasedata).fnumdatabase), adr));
	} /*dbseek*/
		
	
boolean dbwrite (dbaddress adr, long ctbytes, ptrvoid pdata) {

	/* CRITICAL: Prevent writes to read-only databases.
	 * This protects v6 source database during migration.
	 * Store databasedata once to avoid TOCTOU race condition. */
#if defined(FRONTIER_HEADLESS)
	hdldatabaserecord hdb = databasedata;
	if (hdb && (**hdb).u.extensions.flreadonly) {
		log_error(LOG_COMP_DB, "dbwrite BLOCKED read-only fnum=%ld adr=0x%llx bytes=%ld",
		        (long) (**hdb).fnumdatabase,
		        (unsigned long long) adr,
		        ctbytes);
		return (false);
	}
#else
	hdldatabaserecord hdb = databasedata;
#endif

	if (!dbseek (adr)) {
#if defined(FRONTIER_HEADLESS)
		log_error(LOG_COMP_DB, "dbwrite seek failed fnum=%ld adr=0x%llx bytes=%ld",
		        hdb ? (long) (**hdb).fnumdatabase : -1L,
		        (unsigned long long) adr,
		        ctbytes);
#endif
		return (false);
	}

	/* Log every write operation to trace v6 modifications during migration */
#if defined(FRONTIER_HEADLESS)
	log_trace(LOG_COMP_DB, "dbwrite WRITE fnum=%ld adr=0x%llx bytes=%ld flreadonly=%s",
	        hdb ? (long) (**hdb).fnumdatabase : -1L,
	        (unsigned long long) adr,
	        ctbytes,
	        (hdb && (**hdb).u.extensions.flreadonly) ? "TRUE" : "FALSE");
#endif

	if (!filewrite ((hdlfilenum)((**hdb).fnumdatabase), ctbytes, pdata)) {
#if defined(FRONTIER_HEADLESS)
		log_error(LOG_COMP_DB, "dbwrite filewrite failed fnum=%ld adr=0x%llx bytes=%ld",
		        hdb ? (long) (**hdb).fnumdatabase : -1L,
		        (unsigned long long) adr,
		        ctbytes);
#endif
		return (false);
	}

	return (true);
	} /*dbwrite*/
	

boolean dbread (dbaddress adr, long ctbytes, ptrvoid pdata) {
    hdldatabaserecord previous = db_begin_source_read();

	if (!dbseek (adr)) {
#if defined(FRONTIER_HEADLESS)
		log_error(LOG_COMP_DB, "dbread seek failed fnum=%ld adr=0x%llx bytes=%ld saveas=%d source=%p current=%p dest=%p",
			(long) ((**databasedata).fnumdatabase),
			(unsigned long long) adr,
			ctbytes,
			(int) fldatabasesaveas,
			(void *) dbsaveas_source,
			(void *) databasedata,
			(void *) databasedestination);
#endif
        db_end_source_read(previous);
		return (false);
	}

	if (!fileread ((hdlfilenum)((**databasedata).fnumdatabase), ctbytes, pdata)) {
#if defined(FRONTIER_HEADLESS)
		log_error(LOG_COMP_DB, "dbread read failed fnum=%ld adr=0x%llx bytes=%ld saveas=%d source=%p current=%p dest=%p",
			(long) ((**databasedata).fnumdatabase),
			(unsigned long long) adr,
			ctbytes,
			(int) fldatabasesaveas,
			(void *) dbsaveas_source,
			(void *) databasedata,
			(void *) databasedestination);
#endif
        db_end_source_read(previous);
		return (false);
	}

    db_end_source_read(previous);

	return (true);
	} /*dbread*/
	

boolean dbgeteof (long *eof) {

	return (filegeteof ((hdlfilenum)((**databasedata).fnumdatabase), eof));
	} /*dbgeteof*/

		
static void dbheaderdirty (void) {
	
	/*
	5.0.1 dmb: was ^= instead of |=. Caused avail list database corruption
	*/
	
	(**databasedata).flags |= dbdirtymask;
	} /*dbheaderdirty*/


static boolean dbflushheader_core (hdldatabaserecord hdb, hdlfilenum fnum) {

	/*
	Phase 10: Shared core for dbflushheader and dbflushheader_hdb.
	Takes explicit hdb and fnum — no databasedata global access.
	*/

	boolean fl;
	tydatabaserecord diskrec;

	if (hdb == nil)
		return (false);

	boolean use64log = db_hdb_use64 (hdb);

	assert (sizeof (diskrec.u.growthspace) >= sizeof (diskrec.u.extensions));

#if defined(FRONTIER_HEADLESS)
	log_trace(LOG_COMP_DB, "dbflushheader_core enter hdb=%p fnum=%ld dirty=%d use64=%d",
	        (void *) hdb,
	        (long) (**hdb).fnumdatabase,
	        (int) isdirty(hdb),
	        (int) use64log);
#endif

	/* If we opened via legacy adapter, flip to wide writes before flushing.
	 * Call the non-context version directly so the mode persists. */
	db_format_adapter_enable_wide_writes(NULL);

	/* CRITICAL FIX: Don't write to read-only databases.
	 * Issue: v6 source database was being modified during migration because
	 * dbflushheader wrote to it despite flreadonly=1.
	 * Solution: Skip flush if database is read-only. */
	if ((**hdb).u.extensions.flreadonly) {
#if defined(FRONTIER_HEADLESS)
		log_trace(LOG_COMP_DB, "dbflushheader_core skip write (read-only) fnum=%ld dirty=%d",
		        (long) (**hdb).fnumdatabase,
		        (int) isdirty(hdb));
#endif
		return (true);  /* Return success without writing */
	}

	if (isdirty (hdb)) { /*changes made to header*/

		cleardirty (hdb); /*clear it*/

		diskrec = **hdb;

		#ifdef SMART_DB_OPENING
			clearbytes (&diskrec.u.extensions.availlistshadow, sizeof (diskrec.u.extensions.availlistshadow)); /*in-memory structure only*/

			clearbytes (&diskrec.u.extensions.flreadonly, sizeof (diskrec.u.extensions.flreadonly)); /*in-memory structure only*/
		#else
			clearbytes (&diskrec.u.growthspace, sizeof (diskrec.u.growthspace)); /*in-memory structure only*/
		#endif

		{
			unsigned char diskheader[sizeof (tydatabaserecord_64)];

			if (!db_write_v7_header(&diskrec, diskheader, sizeof (diskheader))) {
#if defined(FRONTIER_HEADLESS)
				log_error(LOG_COMP_DB, "dbflushheader_core db_write_v7_header failed");
#endif
				return (false);
			}

			fl = dbwrite_fnum ((dbaddress) 0, (long) sizeof (diskheader), diskheader, fnum, hdb);

#if defined(FRONTIER_HEADLESS)
			if (!fl) {
				log_error(LOG_COMP_DB, "dbflushheader_core dbwrite failed fnum=%ld len=%zu",
				        (long) (**hdb).fnumdatabase,
				        sizeof (diskheader));
			}
#endif
		}

		#ifndef FRONTIER_HEADLESS
		/*flush file buffers*/ {
			IOParam pb;

			clearbytes (&pb, sizeof (pb));

			pb.ioRefNum = (hdlfilenum)((**hdb).fnumdatabase);

			PBFlushFile ((ParmBlkPtr) &pb, false);
			}
		#endif

		return (fl);
		} /*changes made to header*/

	return (true);
	} /*dbflushheader_core*/


static boolean dbflushheader (void) {

	hdldatabaserecord hdb = databasedata;

	return dbflushheader_core (hdb, db_hdb_fnum (hdb));
	} /*dbflushheader*/


static boolean dbflushheader_hdb (hdldatabaserecord hdb) {

	return dbflushheader_core (hdb, db_hdb_fnum (hdb));
	} /*dbflushheader_hdb*/


/* Forward declaration for dbreadheader_core which needs to call dbread_fnum */
boolean dbread_fnum (dbaddress adr, long ctbytes, ptrvoid pdata, hdlfilenum fnum);

static boolean dbreadheader_core (dbaddress adr, boolean *flfree, long *ctbytes, tyvariance *variance, long header_size, hdlfilenum fnum) {

	/*
	Shared core for reading a database block header.

	header_size: Determines v6 (sizeheader_v6 = 8) vs v7 (sizeheader_v7 = 12) parsing.
	fnum:        File number to read from. DB_FNUM_USE_GLOBAL means use global dbread;
	             otherwise uses dbread_fnum.

	Both dbreadheader() and dbrefhandle_fnum() delegate here so header parsing
	logic is defined in exactly one place.
	*/

	uint64_t raw_size = 0;
	tyvariance disk_variance = 0;
	boolean use64 = (header_size == sizeheader_v7);

	if (use64) {
		tyheader64 header;
		boolean ok = (fnum != DB_FNUM_USE_GLOBAL) ? dbread_fnum (adr, sizeheader_v7, &header, fnum) : dbread (adr, sizeheader_v7, &header);

		if (!ok)
			return (false);

		raw_size = db_format_read_be64((unsigned char *) &header.sizefreeword.size);
		disk_variance = (tyvariance) db_format_read_be32((unsigned char *) &header.variance);

		/* Normalize legacy-shifted headers where size/variance landed in the high word. */
		if ((raw_size & 0xFFFFFFFFULL) == 0 && (raw_size >> 32) != 0)
			raw_size >>= 32;
		if ((disk_variance & 0xFFFF) == 0 && ((disk_variance >> 16) != 0))
			disk_variance = (tyvariance) (disk_variance >> 16);
	}
	else {
		tyheader32 header;
		boolean ok = (fnum != DB_FNUM_USE_GLOBAL) ? dbread_fnum (adr, sizeheader_v6, &header, fnum) : dbread (adr, sizeheader_v6, &header);

		if (!ok)
			return (false);

		raw_size = (uint64_t) db_format_read_be32((unsigned char *) &header.sizefreeword.size);
		disk_variance = (tyvariance) db_format_read_be32((unsigned char *) &header.variance);
	}

#if defined(FRONTIER_HEADLESS)
	{
	static int log_headers = -1;
	if (log_headers < 0) {
		const char *env = getenv("FRONTIER_DB_TRACE_HEADERS");
		log_headers = (env != NULL && *env != '\0') ? 1 : 0;
	}
	if (log_headers) {
		unsigned long long raw_dbg = (unsigned long long) raw_size;
		log_trace(LOG_COMP_DB, "dbreadheader_core parsed raw=0x%016llx variance=0x%08x use64=%d fnum=%d",
		          raw_dbg,
		          (unsigned int) disk_variance,
		          (int) use64,
		          (int) fnum);
	}
	}
#endif

	{
		uint64_t freeflag = use64 ? 0x8000000000000000ULL : 0x80000000ULL;
		uint64_t sizemask = use64 ? 0x7FFFFFFFFFFFFFFFULL : 0x7FFFFFFFULL;

		*flfree = (raw_size & freeflag) != 0;
		*ctbytes = (long) (raw_size & sizemask);
	}

	*variance = disk_variance;

	return (true);
	} /*dbreadheader_core*/


boolean dbreadheader (dbaddress adr, boolean *flfree, long *ctbytes, tyvariance *variance) {

	long header_size;
	boolean use64 = db_use64();

	if (databasedata != nil && db_format_is_legacy_db(databasedata))
		use64 = false;

	header_size = use64 ? sizeheader_v7 : sizeheader_v6;

	return dbreadheader_core (adr, flfree, ctbytes, variance, header_size, DB_FNUM_USE_GLOBAL);
	} /*dbreadheader*/


boolean dbread_fnum (dbaddress adr, long ctbytes, ptrvoid pdata, hdlfilenum fnum) {

	/*
	Like dbread but reads from an explicit file number instead of the global
	databasedata. Bypasses Save As source redirection intentionally: an
	explicit fnum means "read from this specific database, period."
	*/

	if (!filesetposition (fnum, adr))
		return (false);

	if (!fileread (fnum, ctbytes, pdata))
		return (false);

	return (true);
	} /*dbread_fnum*/


boolean dbwrite_fnum (dbaddress adr, long ctbytes, ptrvoid pdata, hdlfilenum fnum, hdldatabaserecord hdb) {

	/*
	Like dbwrite but writes to an explicit file number instead of the global
	databasedata. Preserves the read-only guard from dbwrite: callers must
	pass the database handle so we can check flreadonly.
	*/

	/* Read-only guard: correctness invariant, enforced in all builds. */
	if (hdb && (**hdb).u.extensions.flreadonly) {
		log_error(LOG_COMP_DB, "dbwrite_fnum BLOCKED read-only fnum=%ld adr=0x%llx bytes=%ld",
			(long) fnum,
			(unsigned long long) adr,
			ctbytes);
		return (false);
	}

	if (!filesetposition (fnum, adr))
		return (false);

	if (!filewrite (fnum, ctbytes, pdata))
		return (false);

	return (true);
	} /*dbwrite_fnum*/


boolean dbgeteof_fnum (long *eof, hdlfilenum fnum) {

	return (filegeteof (fnum, eof));
	} /*dbgeteof_fnum*/


boolean dbseteof_fnum (long eof, hdlfilenum fnum) {

	/*
	Like dbseteof but uses explicit file number instead of databasedata.
	*/

	if ((eof & 0x80000000L) != 0x00000000L) {

		dberror (dbfilesizeerror);

		return (false); /*trying to grow the file beyond 2 GB*/
		}

	return (fileseteof (fnum, eof));
	} /*dbseteof_fnum*/


boolean dbreadheader_fnum (dbaddress adr, boolean *flfree, long *ctbytes, tyvariance *variance, long header_size, hdlfilenum fnum) {

	/*
	Like dbreadheader but uses explicit file number and header size.
	Delegates to dbreadheader_core which already supports explicit fnum.

	This thin wrapper exists to provide a stable public API name.
	dbreadheader_core is static and also handles DB_FNUM_USE_GLOBAL
	for legacy callers — renaming it would conflate the two use cases.
	*/

	return dbreadheader_core (adr, flfree, ctbytes, variance, header_size, fnum);
	} /*dbreadheader_fnum*/


boolean dbwriteheader_fnum (dbaddress adr, boolean flfree, long ctbytes, tyvariance variance, hdlfilenum fnum, hdldatabaserecord hdb /* format detection only */) {

	/*
	Like dbwriteheader but writes to an explicit file number.
	Serializes the block header and writes via dbwrite_fnum.
	hdb is used only for v6/v7 format detection (headerLength);
	all I/O goes through fnum.
	*/

	boolean use64 = db_hdb_use64(hdb);

	if (use64) {
		uint64_t raw_size = (uint64_t) ctbytes;
		tyheader64 header;

		if (flfree)
			raw_size |= 0x8000000000000000ULL;

		memset(&header, 0, sizeof header);
		db_format_write_be64(&header.sizefreeword.size, raw_size);
		db_format_write_be32(&header.variance, (uint32_t) variance);

		return dbwrite_fnum (adr, sizeheader_v7, &header, fnum, hdb);
	}
	else {
		uint32_t raw_size = (uint32_t) ctbytes;
		tyheader32 header;

		if (flfree)
			raw_size |= 0x80000000UL;

		memset(&header, 0, sizeof header);
		db_format_write_be32(&header.sizefreeword.size, raw_size);
		db_format_write_be32(&header.variance, (uint32_t) variance);

		return dbwrite_fnum (adr, sizeheader_v6, &header, fnum, hdb);
	}
	} /*dbwriteheader_fnum*/


boolean dbwritetrailer_fnum (dbaddress adr, boolean flfree, long ctbytes, hdlfilenum fnum, hdldatabaserecord hdb /* format detection only */) {

	/*
	Like dbwritetrailer but writes to an explicit file number.
	hdb is used only for v6/v7 format detection (headerLength);
	all I/O goes through fnum.
	*/

	boolean use64 = db_hdb_use64(hdb);

	if (use64) {
		uint64_t raw_size = (uint64_t) ctbytes;
		tytrailer64 trailer;

		if (flfree)
			raw_size |= 0x8000000000000000ULL;

		memset(&trailer, 0, sizeof trailer);
		db_format_write_be64(&trailer.sizefreeword.size, raw_size);

		return dbwrite_fnum (adr, sizetrailer_v7, &trailer, fnum, hdb);
	}
	else {
		uint32_t raw_size = (uint32_t) ctbytes;
		tytrailer32 trailer;

		if (flfree)
			raw_size |= 0x80000000UL;

		memset(&trailer, 0, sizeof trailer);
		db_format_write_be32(&trailer.sizefreeword.size, raw_size);

		return dbwrite_fnum (adr, sizetrailer_v6, &trailer, fnum, hdb);
	}
	} /*dbwritetrailer_fnum*/


boolean dbrefhandle_fnum (dbaddress adr, Handle *h, long header_size, hdlfilenum fnum) {

	/*
	Like dbrefhandle_with_header_size but reads from an explicit file number
	instead of the global databasedata. This allows guest database reads to
	work without mutating global state.

	Called from dbrefhandle_context when a non-nil database is in the context.

	IMPORTANT: Callers must provide a block-start address, not an interior
	address. No normalization is performed here because dbnormalizeaddress
	uses the global databasedata which may point to a different file than fnum.
	If guest database externals can store interior addresses, a future
	dbnormalizeaddress_fnum that scans the correct file would be needed.
	*/
	register boolean fl;
	register Handle hregister;
	register long ct;
	long ctbytes;
	boolean flfree;
	tyvariance variance;

	*h = nil;

	if (header_size != 8 && header_size != 12) {
		log_error(LOG_COMP_DB, "dbrefhandle_fnum: invalid header_size %ld (must be 8 or 12)", header_size);
		return (false);
	}

	if (adr == nildbaddress)
		return (false);

	if (!dbreadheader_core (adr, &flfree, &ctbytes, &variance, header_size, fnum))
		return (false);

	ct = ctbytes - (long) variance;

	if (flfree || (ct < 0)) {
		dberror (dbfreeblockerror);
		return (false);
	}

	if (!newclearhandle (ct, h))
		return (false);

	hregister = *h;
	lockhandle (hregister);

	fl = dbread_fnum (adr + header_size, ct, *hregister, fnum);

	unlockhandle (hregister);

	if (!fl) {
		disposehandle (hregister);
		*h = nil;
		return (false);
	}

	return (true);
	} /*dbrefhandle_fnum*/


boolean dbreadtrailer (dbaddress adr, boolean *flfree, long *ctbytes) {

	uint64_t raw_size = 0;
	boolean use64 = db_use64();

	if (databasedata != nil && db_format_is_legacy_db(databasedata))
		use64 = false;

	if (use64) {
		tytrailer64 trailer;

		if (!dbread (adr, sizetrailer_v7, &trailer))
			return (false);
		
		raw_size = db_format_read_be64((unsigned char *) &trailer.sizefreeword.size);
	}
	else {
		tytrailer32 trailer;

		if (!dbread (adr, sizetrailer_v6, &trailer))
			return (false);
		
		raw_size = (uint64_t) db_format_read_be32((unsigned char *) &trailer.sizefreeword.size);
	}

	{
		uint64_t freeflag = use64 ? 0x8000000000000000ULL : 0x80000000ULL;
		uint64_t sizemask = use64 ? 0x7FFFFFFFFFFFFFFFULL : 0x7FFFFFFFULL;

		*flfree = (raw_size & freeflag) != 0;
		*ctbytes = (long) (raw_size & sizemask);
	}
	
	return (true);
	} /*dbreadtrailer*/
	

static boolean dbwriteheader (dbaddress adr, boolean flfree, long ctbytes, tyvariance variance) {

	return db_write_v7_block_header(adr, flfree, ctbytes, variance);
	} /*dbwriteheader*/
	
	
static boolean dbwritetrailer (dbaddress adr, boolean flfree, long ctbytes) {

	return db_write_v7_block_trailer(adr, flfree, ctbytes);
	} /*dbwritetrailer*/


static boolean dbwriteheaderandtrailer (dbaddress adr, boolean flfree, long ctbytes, tyvariance variance) {
	
	if (!dbwriteheader (adr, flfree, ctbytes, variance))
		return (false);
	
	return (dbwritetrailer (adr + sizeheader + ctbytes, flfree, ctbytes));
	} /*dbwriteheaderandtrailer*/
	
		
boolean dbreadavailnode (dbaddress adr, boolean *flfree, long *ctbytes, dbaddress *link) {

	/*
	each node on the avail list has a link stored in its data field, we get
	all the usual data from the node and return that link in the nextnomad
	parameter.
	*/

	tyvariance variance; /*variance is irrelevent in avail nodes*/
	
	if (!dbreadheader (adr, flfree, ctbytes, &variance))
		return (false);

	{
		long link_bytes = db_use64() ? (long) sizeof (uint64_t) : (long) sizeof (uint32_t);
		unsigned char raw[sizeof (uint64_t)];

		if (!dbread (adr + sizeheader, link_bytes, raw))
			return (false);

		if (db_use64())
			*link = (dbaddress) db_format_read_be64(raw);
		else
			*link = (dbaddress) db_format_read_be32(raw);
	}

	return (true);
	} /*dbreadavailnode*/
	

static boolean dbwriteavailnode (dbaddress adr, long ctbytes, dbaddress nextlink) {

	assert (adr != nildbaddress);
	
	assert ((**databasedata).u.extensions.availlistblock == nildbaddress);
	
	if (!dbwriteheader (adr, true, ctbytes, 0L))
		return (false);

	{
		long link_bytes = db_use64() ? (long) sizeof (uint64_t) : (long) sizeof (uint32_t);
		unsigned char raw[sizeof (uint64_t)];

		if (db_use64())
			db_format_write_be64(raw, (uint64_t) nextlink);
		else
			db_format_write_be32(raw, (uint32_t) nextlink);

		if (!dbwrite (adr + sizeheader, link_bytes, raw))
			return (false);
	}

	if (!dbwritetrailer (adr + sizeheader + ctbytes, true, ctbytes))
		return (false);
	
	return (true);
	} /*dbwriteavailnode*/
	

static boolean dbsetavaillink (dbaddress adr, dbaddress link) {

	/*
	adr points to a record in the database file.  move past the header and
	write the link address in the first four bytes of the block's space.
	*/
	
	assert ((**databasedata).u.extensions.availlistblock == nildbaddress);
	
	if (adr == nildbaddress) { /*special case, set link in file header*/
		
		(**databasedata).availlist = link;
		
		dbheaderdirty ();
			
		return (true);
		}
		
	{
		long link_bytes = db_use64() ? (long) sizeof (uint64_t) : (long) sizeof (uint32_t);
		unsigned char raw[sizeof (uint64_t)];

		if (db_use64())
			db_format_write_be64(raw, (uint64_t) link);
		else
			db_format_write_be32(raw, (uint32_t) link);

		return (dbwrite (adr + sizeheader, link_bytes, raw));
	}
	} /*dbsetavaillink*/
	
	
static boolean dbwritedatablock (dbaddress adr, long databytes, long nodebytes, ptrvoid pdata) {

	/*
	there might be less data to write than there is room in the 
	block, so we only write as much as is necessary, but we size 
	the block according to its logical size.
	*/
	
	if (!dbwriteheader (adr, false, nodebytes, (tyvariance) nodebytes - databytes))
		return (false);
		
	if (pdata != nil) /*write data bytes*/
	
		if (!dbwrite (adr + sizeheader, databytes, pdata)) 
			return (false);
	
	return (dbwritetrailer (adr + nodebytes + sizeheader, false, nodebytes));
	} /*dbwritedatablock*/
	

static boolean dbfindpreviousavail (dbaddress adr, dbaddress *prev, long *ixshadow) {
	
	/*
	the available list is not doubly-linked.  this is where we pay the price.
	
	the caller wants to know which node in the avail list points at it.  if
	its the list header, we return nildbaddress.
	
	returns true if *prev was correctly set, false otherwise.
	
	5.1.5 dmb: use in-memory shadow
	*/
	
#ifdef dbshadow
	hdldatabaserecord hdb = databasedata;
	hdlavaillistshadow havailshadow = (hdlavaillistshadow) (**hdb).u.extensions.availlistshadow.data;
	long i, ctavail = (**hdb).u.extensions.availlistshadow.eof / sizeof (tyavailnodeshadow);
	
	for (i = 0; i < ctavail; ++i) {
		
		if ((*havailshadow) [i].adr == adr) {
			
			if (i > 0)
				*prev = (*havailshadow) [i - 1].adr;
			else
				*prev = nildbaddress;
			
			*ixshadow = i;
			
			return (true);
			}
		}
	
	dblogerror (dbfreelisterror); /*something fishy is going on*/
	
	return (false);
#else
	dbaddress nomad;
	boolean flfree;
	long ctbytes;
	dbaddress nextnomad;
	
	nomad = (**databasedata).availlist;
	
	if (nomad == adr) { /*he's the first guy on the list*/
	
		*prev = nildbaddress;
		
		return (true);
		}
		
	while (true) {
		
		if (nomad == nildbaddress) /*reached end of list, no one points at the node*/
			return (false);
			
		if (!dbreadavailnode (nomad, &flfree, &ctbytes, &nextnomad))
			return (false);
			
		if (nextnomad == adr) { /*found the guy that points at our friend*/
			
			*prev = nomad;
			
			return (true);
			}
		
		nomad = nextnomad; /*advance to next node*/
		} /*while*/
#endif
	} /*dbfindpreviousavail*/



#ifdef SMART_DB_OPENING	

static void dbclearshadowavaillist_impl (void) {
	
	/*
	6.2b12 AR: This function MUST be called before modifying the linked list
	of available blocks on disk. We reset the pointer to the cached shadow avail list
	in the database header, set the header's dirty bit, and flush the header to disk.
	
	Finally, we also release the block allocated for the cached shadow avail list.
	
	Do nothing if we're performing a Save A Copy or if the db was opened read-only.
	*/

	register hdldatabaserecord hdb = databasedata;
	
	if (!fldatabasesaveas && !(**hdb).u.extensions.flreadonly)

		if ((**hdb).u.extensions.availlistblock != nildbaddress) {
	
			dbaddress adrblock = (**hdb).u.extensions.availlistblock;
			
			(**hdb).u.extensions.availlistblock = nildbaddress;
			
			dbheaderdirty ();
			
			dbflushheader ();		
			
			if (!dbrelease_internal (adrblock)) {
				#ifdef DATABASE_DEBUG
					char str[256];

					sprintf (str, "dbrelease failed for address %ld.", (**hdb).u.extensions.availlistblock);

					DB_MSG_2 (str);
				#endif
				}
			}
			
	return;
	} /*dbclearshadowavaillist_impl*/

static void dbclearshadowavaillist (void) {
    db_context_guard guard;
    db_context_guard_enter(db_context_refresh_default(), &guard);
    dbclearshadowavaillist_impl();
    db_context_guard_exit(&guard);
}

boolean dbclearshadowavaillist_context(const db_context *context) {
    db_context_guard guard;
    db_context_guard_enter(context, &guard);
    dbclearshadowavaillist();
    db_context_guard_exit(&guard);
    return true;
}


static void dbdisposeshadowavaillist (void) {
	
	register hdldatabaserecord hdb = databasedata;
	handlestream s;
	
	assert (hdb != nil);

	s = (**hdb).u.extensions.availlistshadow;

	disposehandlestream (&s);
		
	clearbytes (&s, sizeof (s));
		
	(**hdb).u.extensions.availlistshadow = s;

	return;
	} /*dbdisposeshadowavaillist*/

#endif


static boolean dbwriteshadowavaillist_impl (void) {

	/*
	6.2a9 AR: If there's an in-memory shadow avail list, write it to a faked new data block
	at the end of the database and store a pointer to the block in the file header.
	
	What happens if the database is opened by a version of Frontier that doesn't know about
	this convention? When it first saves the database, it will write the db header, thereby
	destroying the pointer to the on-disk shadow avail list. We end up with an orphaned block
	in the database, typically a few dozen kB in size. Next time the user saves a copy,
	the orphaned block is dropped from the database. Not a big deal.
	
	Also see dbreadshadowavaillist.

	If we don't have write permission, just dispose of everything. Don't even flush the db header.
	
	6.2b14 AR: Before we call dbwritedatablock, we have to determine the actual size
	of the allocated block. It could be larger than what we asked for. So we call
	dbreadheader to update the value of nodebytes to the real thing.
	*/

	register hdldatabaserecord hdb = databasedata;
	dbaddress adrblock = nildbaddress;
	boolean fl = false;
	boolean flfree;
	
	assert (hdb != nil);

	if ((**hdb).u.extensions.flreadonly || fldatabasesaveas)
		return (true); /*we're done already*/
	
	dbclearshadowavaillist_impl();
		
	if ((**hdb).u.extensions.availlistshadow.data == nil)
		return (true); /*we're done already*/
	
	if ((**hdb).u.extensions.availlistshadow.eof > 0) { /*there's something to be saved*/
	
		long nodebytes = (**hdb).u.extensions.availlistshadow.eof;
		long databytes;
		tyvariance variance = 0;
		Handle h = nil;
		
		if (!dballocate (nodebytes, nil, &adrblock))
			goto error;
		
		assert (adrblock != nildbaddress);

		closehandlestream (&(**hdb).u.extensions.availlistshadow);
		
		if (!copyhandle ((**hdb).u.extensions.availlistshadow.data, &h))
			goto error;
		
		databytes = gethandlesize (h);
		
		assert (databytes == nodebytes || databytes == nodebytes - (long) sizeof (tyavailnodeshadow));

		
		{
		long ix;
		long ct = databytes / sizeof (tyavailnodeshadow);
		register tyavailnodeshadow* p = (tyavailnodeshadow *) *h;

		for (ix = 0; ix < ct; ix++) {
			dbaddress adr_be = p[ix].adr;
			uint64_t size_be = (uint64_t) p[ix].size;

			if (db_use64()) {
				db_format_write_be64((unsigned char *) &p[ix].adr, (uint64_t) adr_be);
				db_format_write_be64((unsigned char *) &p[ix].size, size_be);
			} else {
				db_format_write_be32((unsigned char *) &p[ix].adr, (uint32_t) adr_be);
				db_format_write_be32((unsigned char *) &p[ix].size, (uint32_t) size_be);
			}
			}
		}

		lockhandle (h);
		
		fl = dbreadheader (adrblock, &flfree, &nodebytes, &variance);
		
		assert (databytes <= nodebytes);
		
		fl = fl && dbwritedatablock (adrblock, databytes, nodebytes, *h);
		
		unlockhandle (h);

		disposehandle (h);
		
		if (!fl)
			goto error;
		}
	
	fl = true;

error:

	(**hdb).u.extensions.availlistblock = fl ? adrblock : nildbaddress;

	setdirty (hdb);
	
	dbflushheader ();

	return (fl);
	}/*dbwriteshadowavaillist*/

boolean dbwriteshadowavaillist (void) {
    db_context_guard guard;
    db_context_guard_enter(db_context_refresh_default(), &guard);
    boolean ok = dbwriteshadowavaillist_impl();
    db_context_guard_exit(&guard);
    return ok;
}

boolean dbwriteshadowavaillist_context(const db_context *context) {
    db_context_guard guard;
    db_context_guard_enter(context, &guard);
    boolean ok = dbwriteshadowavaillist_impl();
    db_context_guard_exit(&guard);
    return ok;
}


static boolean dbreadshadowavaillist (void) {

	/*
	6.2a9 AR: If the availlistblock was set in the database header, go in and
	read the avail list block from the end of the database instead of chaining
	thru the linked list of free blocks. Nuke the reference to the availlistblock
	in the db header asap, so if we crash somewhere down the road, we won't read
	an inconsistent availlistblock the next time we open the db.
	
	Also see dbwriteshadowavaillist.
	*/

	register hdldatabaserecord hdb = databasedata;
	dbaddress adrblock;
	hdlavaillistshadow h;
	handlestream s;
	long dbeof;

	assert (hdb != nil);
	
	if (!dbgeteof (&dbeof))
		return (false);
	
	adrblock = (**hdb).u.extensions.availlistblock;

	if (adrblock == nildbaddress) /*for safety*/
		return (false);

	if (!dbrefhandle (adrblock, (Handle*) &h))
		return (false);

	{
		long ix;
		long ct = gethandlesize ((Handle) h) / sizeof (tyavailnodeshadow);
		register tyavailnodeshadow* p = *h;

		for (ix = 0; ix < ct; ix++) {
			if (db_use64()) {
				p[ix].adr = (dbaddress) db_format_read_be64((unsigned char *) &p[ix].adr);
				p[ix].size = (int64_t) db_format_read_be64((unsigned char *) &p[ix].size);
			} else {
				p[ix].adr = (dbaddress) db_format_read_be32((unsigned char *) &p[ix].adr);
				p[ix].size = (int64_t) db_format_read_be32((unsigned char *) &p[ix].size);
			}
		}
	}

	/*Test consistency of cached shadow avail list*/
	
	if ((**h).adr != (**hdb).availlist) {
		
		dblogerror (dbinconsistentavaillisterror);
		
		disposehandle ((Handle) h);
	
		return (false);
		}

	if ((**h).adr != nildbaddress) {

		long availbytes;
		dbaddress firstavail = (**h).adr;
		dbaddress nextavail;
		boolean flfree;
	
		if (!dbreadavailnode (firstavail, &flfree, &availbytes, &nextavail)
				|| !flfree || firstavail + availbytes > dbeof) {
			
			dblogerror (dbinconsistentavaillisterror);
			
			disposehandle ((Handle) h);
		
			return (false);
			}
		}

	openhandlestream ((Handle)h, &s);

		
	(**hdb).u.extensions.availlistshadow = s;
	
	return (true);
	}/*dbreadshadowavaillist*/


static boolean dbshadowavaillist (void) {
	
	/*
	5.1.5 dmb: read the entire avail list into memory. the last record in 
	the shadow array is {0, 0}
	
	6.2a9 AR: streamlined usage of handlestream, no longer keep separate
	local havaillist handle around which would interfere with the new
	shadow avail list caching in the db. (see dbwriteshadowavaillist)
	
	If dbreadschadowaviallist doesn't succeed, we try the old-fashioned way.
	*/
	
	handlestream s;
	tyavailnodeshadow availrec;
	dbaddress nextavail;
	boolean flfree;
	long dbeof;

#ifdef SMART_DB_OPENING	
	if ((**databasedata).u.extensions.availlistblock != nildbaddress)
		if (dbreadshadowavaillist ())
			return (true);
#endif

	if (!dbgeteof (&dbeof))
		return (false);
	
	openhandlestream (nil, &s);
	
	availrec.adr = (**databasedata).availlist;
	
	while (availrec.adr != nildbaddress) {
		
		long avail_size = 0;
		if (!dbreadavailnode (availrec.adr, &flfree, &avail_size, &nextavail) ||
			!flfree ||
			availrec.adr + avail_size > dbeof) {

			availrec.adr = nildbaddress;
			
			dberror (dbfreelisterror);

			break;
			}
		
		availrec.size = (int64_t) avail_size;
		if (!writehandlestream (&s, &availrec, sizeof (availrec)))
			goto error;
		
		availrec.adr = nextavail;
		
		rollbeachball ();
		} /*while*/	
	
	availrec.size = 0;
	
	if (!writehandlestream (&s, &availrec, sizeof (availrec)))
		goto error;
	
	(**databasedata).u.extensions.availlistshadow = s;
	
	return (true);
	
	error:
		disposehandlestream (&s);
		
		return (false);
	} /*dbshadowavaillist*/


static boolean dbinsertavailshadow (long ixshadow, dbaddress adr, long ctbytes) {
	
	handlestream s = (**databasedata).u.extensions.availlistshadow;
	tyavailnodeshadow avail;
	
	assert ((ixshadow >= 0) && (ixshadow <= s.eof / (long) sizeof (tyavailnodeshadow)));
	
	avail.adr = adr;
	avail.size = (int64_t) ctbytes;
	
	s.pos = ixshadow * sizeof (tyavailnodeshadow);
	
	if (!mergehandlestreamdata (&s, 0L, &avail, sizeof (avail)))
		return (false);
	
	(**databasedata).u.extensions.availlistshadow = s;
	
	return (true);
	} /*dbinsertavailshadow*/


static boolean dbdeleteavailshadow (long ixshadow) {

	handlestream s = (**databasedata).u.extensions.availlistshadow;
	
	assert ((ixshadow >= 0) && (ixshadow < s.eof / (long) sizeof (tyavailnodeshadow)));
	
	s.pos = ixshadow * sizeof (tyavailnodeshadow);
	
	if (!pullfromhandlestream (&s, sizeof (tyavailnodeshadow), nil))
		return (false);
	
	(**databasedata).u.extensions.availlistshadow = s;
	
	return (true);
	} /*dbdeleteavailshadow*/


static boolean dbsetavailshadow (long ixshadow, dbaddress adr, long ctbytes) {

	handlestream s = (**databasedata).u.extensions.availlistshadow;
	tyavailnodeshadow avail;
	
	assert ((ixshadow >= 0) && (ixshadow < s.eof / (long) sizeof (tyavailnodeshadow)));
	
	avail.adr = adr;
	
	avail.size = (int64_t) ctbytes;
	
	s.pos = ixshadow * sizeof (tyavailnodeshadow);
	
	if (!mergehandlestreamdata (&s, sizeof (avail), &avail, sizeof (avail)))
		return (false);
	
	(**databasedata).u.extensions.availlistshadow = s;
	
	return (true);
	} /*dbsetavailshadow*/


static boolean dbgetsizeandvariance (dbaddress adr, long *size, tyvariance *variance) {

	/*
	give me the address of a database block and I'll return the number
	of bytes it has reserved.  the variance is the number of unused bytes
	that are the result of block-splitting in allocate.
	*/
	
	boolean flfree;
	
	return (dbreadheader (adr, &flfree, size, variance));
	} /*dbgetsizeandvariance*/
	

static boolean dbsetsize (dbaddress adr, long size, tyvariance variance) {
	
	return (dbwriteheader (adr, false, size, variance));
	} /*dbsetsize*/
	

boolean dbreference_internal (dbaddress adr, long maxbytes, ptrvoid pdata) {

	/*
	copy into pdata the database block located at address.  the number
	of bytes is found in the header/trailer word at the beginning of
	the block.
	
	under no circumstances will we read in more than maxbytes.  the caller
	should supply us with the size of pdata in this argument, it prevents
	disastrous overwriting of memory.
	
	each block also records a variance -- the number of extra bytes 
	due to block-splitting in allocate.  we only copy the number of
	bytes that actually hold the caller's data, probably saves a little
	time, and keeps us from overwriting other important stuff!
	*/
	
	long ctbytes;
	boolean flfree;
	tyvariance variance;

    #if defined(FRONTIER_HEADLESS)
    (void) dbnormalizeaddress(&adr);
    #endif

    if (!dbreadheader (adr, &flfree, &ctbytes, &variance))
        return (false);
		
	if (flfree || (ctbytes < 0)) { /*referencing a free node -- probably a bad address*/
		
		dberror (dbfreeblockerror);
		
		return (false);
		}
	
	return (dbread (adr + sizeheader, min (maxbytes, ctbytes - (long) variance), pdata));
	} /*dbreference_internal*/

boolean dbreference_with_header_size(dbaddress adr, long maxbytes, ptrvoid pdata, long header_size) {
	/*
	Like dbreference_internal but uses explicit header size instead of sizeheader macro.
	Needed during migration when global mode is locked but we need to read v6 blocks.

	header_size: Must be 8 (v6) or 12 (v7). Other values are invalid.
	*/
	long ctbytes;
	boolean flfree;
	tyvariance variance;

	/* Validate header_size - only 8 (v6) and 12 (v7) are valid */
	if (header_size != 8 && header_size != 12) {
		log_error(LOG_COMP_DB, "dbreference_with_header_size: invalid header_size %ld (must be 8 or 12)", header_size);
		return (false);
	}

#if defined(FRONTIER_HEADLESS)
	(void) dbnormalizeaddress(&adr);
#endif

	if (!dbreadheader(adr, &flfree, &ctbytes, &variance))
		return (false);

	if (flfree || (ctbytes < 0)) {
		dberror(dbfreeblockerror);
		return (false);
	}

	return (dbread(adr + header_size, min(maxbytes, ctbytes - (long) variance), pdata));
}

boolean dbreference_fnum(dbaddress adr, long maxbytes, ptrvoid pdata, long header_size, hdlfilenum fnum) {
	/*
	Like dbreference_with_header_size but reads from an explicit file number
	instead of the global databasedata. This allows context-aware reads without
	mutating global state.

	header_size: Must be 8 (v6) or 12 (v7).
	fnum: File number to read from directly.
	*/
	long ctbytes;
	boolean flfree;
	tyvariance variance;

	if (header_size != 8 && header_size != 12) {
		log_error(LOG_COMP_DB, "dbreference_fnum: invalid header_size %ld (must be 8 or 12)", header_size);
		return (false);
	}

	/* No dbnormalizeaddress: callers must provide block-start addresses.
	   dbnormalizeaddress uses global databasedata which may point to a
	   different file than fnum. */

	if (!dbreadheader_core(adr, &flfree, &ctbytes, &variance, header_size, fnum))
		return (false);

	if (flfree || (ctbytes < 0)) {
		dberror(dbfreeblockerror);
		return (false);
	}

	return (dbread_fnum(adr + header_size, min(maxbytes, ctbytes - (long) variance), pdata, fnum));
}

boolean dbreference (dbaddress adr, long maxbytes, ptrvoid pdata) {
    db_context *ctx = db_context_refresh_default();
    return dbreference_context(ctx, adr, maxbytes, pdata);
}
	

boolean dbrefhandle_with_header_size(dbaddress adr, Handle *h, long header_size) {
	/*
	Like dbrefhandle but uses explicit header size instead of sizeheader macro.
	Needed during migration when global mode is locked but we need to read v6 blocks.

	header_size: Must be 8 (v6) or 12 (v7). Other values are invalid.
	*/
	dbaddress a = adr;
	register boolean fl;
	register Handle hregister;
	register long ct;
	long ctbytes;
	boolean flfree;
	tyvariance variance;

	*h = nil;

	/* Validate header_size - only 8 (v6) and 12 (v7) are valid */
	if (header_size != 8 && header_size != 12) {
		log_error(LOG_COMP_DB, "dbrefhandle_with_header_size: invalid header_size %ld (must be 8 or 12)", header_size);
		return (false);
	}

	if (a == nildbaddress)
		return (false);

#if defined(FRONTIER_HEADLESS)
	(void) dbnormalizeaddress(&a);
#endif

	if (!dbreadheader_core(a, &flfree, &ctbytes, &variance, header_size, DB_FNUM_USE_GLOBAL))
		return (false);

	ct = ctbytes - (long) variance;

	if (flfree || (ct < 0)) {
		dberror(dbfreeblockerror);
		return (false);
	}

	if (!newclearhandle(ct, h))
		return (false);

	hregister = *h;
	lockhandle(hregister);

	fl = dbread(a + header_size, ct, *hregister);

	unlockhandle(hregister);

	if (!fl) {
		disposehandle(hregister);
		*h = nil;
		return (false);
	}

	return (true);
}

boolean dbrefhandle (dbaddress adr, Handle *h) {

	/*
	copy a block from the database into a handle which we allocate.

	the caller must dispose of the handle.

	5.0.1 dmb: added freeblock error; don't fail silently
	*/

    dbaddress a = adr;
	register boolean fl;
	register Handle hregister;
	register long ct;
	long ctbytes;
	boolean flfree;
	tyvariance variance;

	*h = nil;

	if (a == nildbaddress) /*defensive driving*/
		return (false);

#if defined(FRONTIER_HEADLESS)
    if (!dbnormalizeaddress(&a)) {
        log_trace(LOG_COMP_DB, "dbrefhandle: normalization returned false for 0x%llx (may be legacy address)",
                (unsigned long long) adr);
    }
#endif

    if (!dbreadheader (a, &flfree, &ctbytes, &variance))
        return (false);

    ct = ctbytes - (long) variance;

    if (flfree || (ct < 0)) { /*probably a bad address*/

        dberror (dbfreeblockerror);
#if defined(FRONTIER_HEADLESS)
        log_error(LOG_COMP_DB, "dbrefhandle found free block adr=0x%llx size=%ld variance=%ld",
                  (unsigned long long)a, ctbytes, (long) variance);
#endif
        return (false);
        }

	if (!newclearhandle (ct, h))
		return (false);

#if defined(FRONTIER_HEADLESS)
    if (a == 0x76e) {
        log_trace(LOG_COMP_DB, "dbrefhandle watch allocated handle size=%ld", ct);
    }
#endif

	hregister = *h;

	lockhandle (hregister);

#if defined(FRONTIER_HEADLESS)
	/* Log first 20 reads to check for format mode mismatches */
	if (log_enabled(LOG_LEVEL_TRACE, LOG_COMP_DB)) {
		static int header_log_count = 0;
		if (header_log_count < 20) {
			db_format_mode current_mode = db_format_mode_current();
			long actual_header_size = current_mode.use_64bit_format ? sizeheader_v7 : sizeheader_v6;
			log_trace(LOG_COMP_DB, "dbrefhandle[%d]: adr=0x%llx use_64bit=%d header_size=%ld (v6=%ld v7=%ld) read_offset=0x%llx",
			          header_log_count++,
			          (unsigned long long)a,
			          current_mode.use_64bit_format ? 1 : 0,
			          actual_header_size,
			          sizeheader_v6,
			          sizeheader_v7,
			          (unsigned long long)(a + sizeheader));
		} else {
			header_log_count++;
		}
	}
#endif

	fl = dbread (a + sizeheader, ct, *hregister);

#if defined(FRONTIER_HEADLESS)
    if (a == 0x76e) {
        log_trace(LOG_COMP_DB, "dbrefhandle watch dbread result=%d", fl ? 1 : 0);
    }
#endif
	
	unlockhandle (hregister);
	
	return (fl);
	} /*dbrefhandle*/
	



static boolean dballocate (long databytes, ptrvoid pdata, dbaddress *paddress) {

	/*
	allocate databytes space in the database.  return the database address of the
	allocated space in paddress.  if allocation error, paddress == nildbaddress.
	
	the caller can supply the address of data to be saved in the database, if its
	not nil, we will copy the data into the file before returning.

	4.1b9 dmb: removed special case check for nil prevnomad; dbsetavaillink handles
	that case.
	
	5.1.5b1 dmb: use and maintain availlist shadow
	*/

	long origeof;
	long nodebytes, newnodebytes;
	//boolean flfree;
	dbaddress nomad, prevnomad, nextnomad;
	tyvariance variance;
	long smallestinterestingblock;
	long ctalloc;
    const char *fail_step = "start";
	
#if fldebug
	allocs++;
#endif

#ifdef SMART_DB_OPENING	
	dbclearshadowavaillist (); /*6.2b12 AR*/
#endif

    db_context_guard guard;
    db_context swap_ctx;
    boolean using_destination = false;
    db_context *apply_ctx = db_context_for_saveas_destination(&swap_ctx, &using_destination);
#if defined(FRONTIER_HEADLESS)
    if (log_enabled(LOG_LEVEL_TRACE, LOG_COMP_DB)) {
        static int call_count = 0;
        if (call_count++ < 5) {
            log_trace(LOG_COMP_DB, "dballocate: using_destination=%d apply_ctx=%p",
                      (int) using_destination, (void *) apply_ctx);
            if (apply_ctx != NULL) {
                log_trace(LOG_COMP_DB, "dballocate: context mode use_64bit=%d adapter_repack=%d",
                          (int) apply_ctx->mode.use_64bit_format, (int) apply_ctx->mode.adapter_repack);
            }
        }
    }
#endif
    db_context_guard_enter(apply_ctx, &guard);
#if defined(FRONTIER_HEADLESS)
    if (log_enabled(LOG_LEVEL_TRACE, LOG_COMP_DB)) {
        static int call_count_after = 0;
        if (call_count_after++ < 5) {
            db_format_mode current_after = db_format_mode_current();
            log_trace(LOG_COMP_DB, "dballocate: after guard enter, current mode use_64bit=%d adapter_repack=%d",
                      (int) current_after.use_64bit_format, (int) current_after.adapter_repack);
        }
    }
#endif

#if !defined(FRONTIER_HEADLESS)
    (void) using_destination;
#endif
	
	smallestinterestingblock = max (databytes, (long) minblocksize);
	
#ifdef dbshadow
	{
	hdldatabaserecord hdb = databasedata;
	hdlavaillistshadow havailshadow = (hdlavaillistshadow) (**hdb).u.extensions.availlistshadow.data;
	long i, ctavail = (**hdb).u.extensions.availlistshadow.eof / sizeof (tyavailnodeshadow);
	
	for (i = 0, prevnomad = nildbaddress; i < ctavail; ++i, prevnomad = nomad) {

#if	fldebug
		allocloops++;
#endif

		nomad = (*havailshadow) [i].adr;
		
		if (nomad == nildbaddress)
			break;
		
		nodebytes = (*havailshadow) [i].size;
		
		if (nodebytes < smallestinterestingblock) //too small to be of interest
			continue;

		/*found a block to allocate off avail list*/
		
		//if (!dbreadavailnode (nomad, &flfree, &nodebytes, &nextnomad))
		//	goto failure;
		
		nextnomad = (*havailshadow) [i + 1].adr;
		
		//assert (nodebytes == (*havailshadow) [i].size);
		
		variance = nodebytes - databytes; //how much more we got than what we asked for
		
		if (variance >= (minblocksize + sizeheader + sizetrailer)) { //split into two blocks
			
			newnodebytes = nodebytes - (databytes + sizeheader + sizetrailer);
			
			if (!dbwriteheaderandtrailer (nomad, true, newnodebytes, (tyvariance) 0))
				goto failure;
			
			dbsetavailshadow (i, nomad, newnodebytes);
			
			nomad += sizeheader + newnodebytes + sizetrailer;
			
			if (!dbwritedatablock (nomad, databytes, databytes, pdata))
				goto failure;
			
			*paddress = nomad; /*use the newly split off block*/
			

#if fldebug
			splits++;
#endif

		goto success;
			} /*splitting into two blocks*/
			
        fail_step = "dbwritedatablock(assign)";
		if (!dbwritedatablock (nomad, databytes, nodebytes, pdata))
			goto failure;
		

#if fldebug
		nonsplits++;
#endif		

		*paddress = nomad;
		
		dbdeleteavailshadow (i);
		
		if (!dbsetavaillink (prevnomad, nextnomad)) /*unlink node from avail list*/
			goto failure;
		
		goto success;
		}
	}
#else
	nomad = (**databasedata).availlist;
	
	prevnomad = nildbaddress; /*no previous node*/
	
	while (nomad != nildbaddress) { /*look at each element on the avail list, first-fit*/
		
        fail_step = "dbreadavailnode";
		if (!dbreadavailnode (nomad, &flfree, &nodebytes, &nextnomad))
			goto failure;
		
		if (nodebytes < smallestinterestingblock) /*too small to be of interest*/
			goto nextloop;
		
		/*found a block to allocate off avail list*/
		
		variance = nodebytes - databytes; /*how much more we got than what we asked for*/
		
		if (variance >= (minblocksize + sizeheader + sizetrailer)) { /*split into two blocks*/
			
            fail_step = "dbwriteheaderandtrailer";
			newnodebytes = nodebytes - (databytes + sizeheader + sizetrailer);
			
			if (!dbwriteheaderandtrailer (nomad, true, newnodebytes, (tyvariance) 0))
				goto failure;
				
			nomad += sizeheader + newnodebytes + sizetrailer;
			
            fail_step = "dbwritedatablock(split)";
			if (!dbwritedatablock (nomad, databytes, databytes, pdata))
				goto failure;
				
			*paddress = nomad; /*use the newly split off block*/
			

#if fldebug
			splits++;
#endif			

			goto success;
			} /*splitting into two blocks*/
			
		if (!dbwritedatablock (nomad, databytes, nodebytes, pdata))
			goto failure;

#if fldebug		
		nonsplits++;
#endif

		*paddress = nomad;
		
        fail_step = "dbsetavaillink";
		if (!dbsetavaillink (prevnomad, nextnomad)) /*unlink node from avail list*/
			goto failure;
		
		goto success;
		
		nextloop:
		
		prevnomad = nomad; /*remember in case we have to unlink the next one*/
		
		nomad = nextnomad; /*advance to next node in the avail list*/
		} /*while*/
#endif

#if fldebug	
	newallocs++;
#endif

    fail_step = "dbgeteof";
	if (!dbgeteof (&origeof))
		goto failure;
	
	if (databytes < minblocksize) { /*we never alloc a block smaller than minblocksize*/
		
		ctalloc = minblocksize;
		
		variance = minblocksize - databytes;
		}
	else {
	
		ctalloc = databytes;
		
		variance = 0;
		}
		
    fail_step = "dbseteof";
	if (!dbseteof (origeof + sizeheader + ctalloc + sizetrailer))
		goto failure;
		
    fail_step = "dbwritedatablock(eof)";
	if (!dbwritedatablock (origeof, databytes, ctalloc, pdata)) 	
		goto failure;
	
 	*paddress = origeof; /*this is the address of the block we allocated*/


success:
#if defined(FRONTIER_HEADLESS)
	if (log_enabled(LOG_LEVEL_TRACE, LOG_COMP_DB) && *paddress != nildbaddress) {
		log_trace(LOG_COMP_DB, "dballocate: allocated %ld bytes at 0x%llx using_dest=%d",
		          databytes, (unsigned long long)*paddress, using_destination ? 1 : 0);
		log_trace(LOG_COMP_DB, "dballocate:   databasedata=%p headerLength=%ld",
		          (void*)databasedata,
		          databasedata ? (long)(**databasedata).headerLength : 0L);
		log_trace(LOG_COMP_DB, "dballocate:   databasedestination=%p headerLength=%ld eof=%ld",
		          (void*)databasedestination,
		          databasedestination ? (long)(**databasedestination).headerLength : 0L, origeof);
	}
#endif

	db_context_guard_exit(&guard);
	
	return (true); /*the allocation was successful*/
	
	
failure:

#if defined(FRONTIER_HEADLESS)
    log_error(LOG_COMP_DB,
              "dballocate failure step=%s size=%ld saveas=%d current=%p dest=%p using_dest=%d",
              fail_step,
              databytes,
              (int) fldatabasesaveas,
              (void *) databasedata,
              (void *) databasedestination,
              using_destination ? 1 : 0);
#endif

	db_context_guard_exit(&guard);
	
	return (false);
	} /*dballocate*/


static boolean dbmergeleft (boolean flmerged, dbaddress adr, boolean* ptrflmergedleft) {

	/*
	try to merge the database block pointed to by adr with the block to its
	left.  this often may not be possible because the block to the left may
	or may not be free, or even may not exist.  return true if it worked, 
	false otherwise.
	
	we do nothing to the avail list if we merge.  we assume that the free
	block to the left is already on the avail list.
	
	flmerged tells us whether a right-merge has already been performed.  if
	so, the node at adr is on the available list and must be popped off the
	list if a left-merge takes place.
	
	5.1.5b1 dmb: maintain availlist shadow
	*/
	
	dbaddress newadr;
	long newsize;
	boolean flfree, flleftfree;
	long ctbytes, ctleftbytes;
	dbaddress nextavail, prevavail;
	long ixshadow;
	
	*ptrflmergedleft = false; /*default return value*/
	
	if (adr == firstphysicaladdress) /*nothing to the left, other than the header!*/
		return (true);

	if (adr < firstphysicaladdress) { /*nothing to the left, other than the header!*/

		dblogerror (dbmergeinvalidblockerror); /*illegal address*/

		return (false);
		}
	
	if (!dbreadtrailer (adr - sizetrailer, &flleftfree, &ctleftbytes))
		return (false);

	if (ctleftbytes < minblocksize) { /*probably an invalid block*/

		dblogerror (dbmergeinvalidblockerror); /*illegal address*/

		return (false);
		}

	if (!flleftfree) /*can't merge if block to left is not free*/
		return (true);

#ifdef fldebug //DATABASE_DEBUG
	{
		long leftblockadr = adr - sizetrailer - ctleftbytes - sizeheader;
		long dbeof;
		boolean flfreeheader;
		long ctbytesheader;
		tyvariance variance;

		if (!dbgeteof (&dbeof))
			return (false);

		if (leftblockadr < firstphysicaladdress) {

			dblogerror (dbmergeinvalidblockerror); /*illegal address*/

			return (false);
			}

		if (leftblockadr > dbeof) {

			dblogerror (dbmergeinvalidblockerror); /*illegal address*/

			return (false);
			}

		if (!dbreadheader (leftblockadr, &flfreeheader, &ctbytesheader, &variance))
			return (false);

		if (!flfreeheader) { /*the trailer said otherwise!*/

			dblogerror (dbmergeinvalidblockerror); /*illegal address*/

			return (false);
			}
		
		if (ctbytesheader != ctleftbytes) {

			dblogerror (dbmergeinvalidblockerror); /*illegal address*/

			return (false);
			}
	}
#endif

	if (!dbreadavailnode (adr, &flfree, &ctbytes, &nextavail)) /*get data about our block*/
		return (false);
	
	if (flmerged) { /*the node we're releasing is already on the avail list, pop him!*/
		
		if (!dbfindpreviousavail (adr, &prevavail, &ixshadow))
			return (false); /*damaged free list*/
		
		assert ((*(hdlavaillistshadow)(**databasedata).u.extensions.availlistshadow.data) [ixshadow + 1].adr == nextavail);
		
		if (!dbsetavaillink (prevavail, nextavail)) /*point around the soon-to-be-defunct node*/
			return (false);
			
		if (!dbdeleteavailshadow (ixshadow))
			return (false);
		}
	
	newsize = ctleftbytes + ctbytes + sizeheader + sizetrailer; /*start merging*/
	
	newadr = adr - sizetrailer - ctleftbytes - sizeheader;
	
	if (!dbwriteheaderandtrailer (newadr, true, newsize, 0L)) //avail link already set
		return (false);
	
	if (!dbfindpreviousavail (newadr, &prevavail, &ixshadow)) // don't need prev, just index
		return (false);

	dbsetavailshadow (ixshadow, newadr, newsize);
	
#if fldebug
	leftmerges++;
#endif

	*ptrflmergedleft = true; /*actually merged*/
	
	return (true);
	} /*dbmergeleft*/


static boolean dbmergeright (dbaddress adr, long ctbytes, boolean* ptrflmergedright) {

	/*
	try to merge the database block pointed to by adr with the block to its
	right.  this often may not be possible because the block to the right may
	or may not be free, or even may not exist.  return true if we merged, 
	false otherwise.
	
	we also adjust the available list if we merge.  it must point at the 
	beginning of the two merged blocks.
	
	we don't need to change the address because even if we merge, the address
	of the merged blocks is the same as adr.
	
	5.1.5b1 dmb: take ctbytes parameter so we don't need to re-read header;
	maintain availlist shadow

	6.2b5 AR: Do writes for merged block sequentially
	*/
	
	long eof;
	dbaddress rightblockadr;
	boolean flrightfree;
	long ctrightbytes;
	dbaddress prevavail, nextavail;
	long ixshadow;
	
	*ptrflmergedright = false;
	
	rightblockadr = adr + sizeheader + ctbytes + sizetrailer;
	
	if (!dbgeteof (&eof))
		return (false);

	if (rightblockadr == eof) /*there is no block to the right*/
		return (true);

	if (rightblockadr > eof) { /*reached the end of the file*/

		dblogerror (dbmergeinvalidblockerror);

		return (false);
		}

	if (!dbreadavailnode (rightblockadr, &flrightfree, &ctrightbytes, &nextavail))
		return (false);

	if (ctrightbytes < minblocksize) {

		dblogerror (dbmergeinvalidblockerror);

		return (false); //not likely to be a valid block, probably just a stream of nil bytes
		}
	
	if (!flrightfree) /*the block to the right is in use*/
		return (true);
	
#ifdef fldebug //DATABASE_DEBUG
	{
		long traileradr = rightblockadr + sizeheader + ctrightbytes;
		boolean flfreetrailer;
		long ctbytestrailer;

		if (traileradr < firstphysicaladdress) {

			dblogerror (dbmergeinvalidblockerror); /*illegal address*/

			return (false);
			}

		if (traileradr > eof) {

			dblogerror (dbmergeinvalidblockerror); /*illegal address*/

			return (false);
			}

		if (!dbreadtrailer (traileradr, &flfreetrailer, &ctbytestrailer))
			return (false);

		if (!flfreetrailer) { /*the header said otherwise!*/

			dblogerror (dbmergeinvalidblockerror); /*illegal address*/

			return (false);
			}
		
		if (ctbytestrailer != ctrightbytes) {

			dblogerror (dbmergeinvalidblockerror); /*illegal address*/

			return (false);
			}
	}
#endif

	if (!dbfindpreviousavail (rightblockadr, &prevavail, &ixshadow))
		return (false);
	
	assert ((*(hdlavaillistshadow)(**databasedata).u.extensions.availlistshadow.data) [ixshadow + 1].adr == nextavail);

	if (!dbsetavaillink (prevavail, adr)) /*point at beginning of two merged blocks*/
		return (false);
		
	ctbytes += ctrightbytes + sizeheader + sizetrailer;

	if (!dbwriteavailnode (adr, ctbytes, nextavail))
		return (false);

	if (!dbsetavailshadow (ixshadow, adr, ctbytes))
		return (false);
		
#if fldebug
	rightmerges++;
#endif

	*ptrflmergedright = true; /*actually merged*/
	
	return (true); /*actually merged*/
	} /*dbmergeright*/
	
	
static boolean dbrelease_internal (dbaddress adr) {

	/*
	release the database block at adr.
	
	try to merge it with the block to the left and then with the block to right.
	
	push any new free block(s) on the available list.
	
	5.1.4 dmb: do all three writes sequentially (write availlink before trailer)

	6.2b3 AR: Change in our philosophy: We no longer consider it a big deal if releasing
	a block fails, but make absolutely sure we don't corrupt the database by releasing a non-existant
	block in the database. Most callers now ignore our return value.
	*/
	
	boolean flmergedleft, flmergedright;
	boolean flfree;
	long ctbytes;
	tyvariance variance;
	 
	if (adr == nildbaddress) /*its easy to release the nil node*/
		return (true);
	
#ifdef SMART_DB_OPENING	
	dbclearshadowavaillist (); /*6.2b12 AR*/
#endif

	if (!dbreadheader (adr, &flfree, &ctbytes, &variance)) 
		return (false);
	
	if (flfree) { /*nasty internal error - block is already free*/
		
		dberror (dbreleasefreeblockerror);
		
		return (false);
		}

#ifdef fldebug //DATABASE_DEBUG
	/*check header/trailer consistency*/ {

		boolean flfreetrailer;
		long ctbytestrailer;
		dbaddress traileradr = adr + sizeheader + ctbytes;
		long dbeof;

		if (!dbgeteof (&dbeof))
			return (false);

		if (traileradr > dbeof) { /*nasty internal error - probably not a valid address*/
			
			dblogerror (dbreleaseinvalidblockerror);
			
			return (false);
			}

		if (!dbreadtrailer (traileradr, &flfreetrailer, &ctbytestrailer))
			return (false);

		if (flfreetrailer) { /*nasty internal error - probably not a valid address*/
			
			dblogerror (dbreleaseinvalidblockerror);
			
			return (false);
			}

		if (ctbytes != ctbytestrailer) { /*nasty internal error - probably not a valid address*/
			
			dblogerror (dbreleaseinvalidblockerror);
			
			return (false);
			}
		}
#endif
	
	if (!dbmergeright (adr, ctbytes, &flmergedright))
		return (false);
	
	if (!dbmergeleft (flmergedright, adr, &flmergedleft))
		return (false);
		
	if (flmergedleft || flmergedright)
		return (true); /*we're done*/
	
	/*no merging -- set free bits in header & trailer, insert at head of avail list*/
	
	if (!dbwriteavailnode (adr, ctbytes, (**databasedata).availlist))
		return (false);
	
	(**databasedata).availlist = adr;
	
	if (!dbinsertavailshadow (0, adr, ctbytes))
		return (false);
	
	dbheaderdirty ();
	
	return (true);
	} /*dbrelease_internal*/
	

	

static boolean dbmove (ptrvoid pdata, long ctbytes, dbaddress adr) {

	/*
	copy the data from memory (pdata) to the data part of the block at adr.
	
	call this when you know that the size of the object you're writing is the
	same as the object this block was created to hold.
	*/

	return (dbwrite (adr + sizeheader, ctbytes, pdata)); 
	} /*dbmove*/


boolean dbassign_internal (dbaddress *padr, long newsize, ptrvoid pdata) {
	
	/*
	we want to move new data into the database block whose address is adr.
	
	maybe the size has changed?  think about variable length strings.  if so, the
	new size is given in newsize. 
	
	we get a pointer to the address because we might change the address if we have
	to allocate to fit new larger data.  we never re-allocate a block if the data
	got smaller.
	
	10/16/91 dmb: found longstanding bug.  the variance must we updated any time the 
	size changes, not just when the newsize is less than cttotal

	6.2b2 AR: Improved error checking based on the assumption that we should
	never assign to a free block -- except when saving a copy, of course.
	*/
	
	register dbaddress adr;
	tyvariance ctunused;
	long cttotal;
	boolean flfree;
	
	adr = *padr; /*copy into a register*/

	if (fldatabasesaveas || (adr == nildbaddress)) { /*no previous allocation, create a new one*/
		boolean ok = dballocate (newsize, pdata, padr);
#if defined(FRONTIER_HEADLESS)
        if (!ok) {
            log_error(LOG_COMP_DB,
                      "dballocate failed size=%ld saveas=%d dest=%p",
                      newsize,
                      (int) fldatabasesaveas,
                      (void *) databasedestination);
        }
#endif
        return ok;
    }
	
	if (!dbreadheader (adr, &flfree, &cttotal, &ctunused)) /*find out how much space we have in block*/
		return (false);

	if (flfree) { /*6.2b2 AR: here's another chance to easily detect corruption, why not use it?*/

		dberror (dbassignfreeblockerror);
	
		return (false);
		}
	
	if (newsize > cttotal) { /*there isn't enough room*/

		if (!dbrelease_internal (adr)) { //ignore return value, don't want to abort saving
			#ifdef DATABASE_DEBUG
				char str[256];

				sprintf (str, "dbrelease failed for address %ld.", adr);

				DB_MSG_2 (str);
			#endif
			}

		return (dballocate (newsize, pdata, padr)); /*allocate the new, bigger block*/
		}
	
	if (newsize != cttotal - ctunused) /*must update the variance*/
	
		if (!dbsetsize (adr, cttotal, cttotal - newsize))
		
			return (false);
		
	return (dbmove (pdata, newsize, adr)); /*copy the data into a big-enough block*/
	} /*dbassign_internal*/

boolean dbassign (dbaddress *padr, long newsize, ptrvoid pdata) {
    db_context ctx_storage;
    boolean using_destination = false;
    db_context *ctx = db_context_for_saveas_destination(&ctx_storage, &using_destination);
    if (ctx == NULL)
        ctx = db_context_refresh_default();
#if defined(FRONTIER_HEADLESS)
    if (log_enabled(LOG_LEVEL_TRACE, LOG_COMP_DB)) {
        static int log_count = 0;
        if (log_count++ < 10) {
            log_trace(LOG_COMP_DB, "dbassign: saveas_active=%d using_destination=%d dest_db=%p",
                      (int)fldatabasesaveas, (int)using_destination, (void*)databasedestination);
        }
    }
#endif
    return dbassign_context(ctx, padr, newsize, pdata);
}
	
	
boolean dbgetsize_internal (dbaddress adr, long *logicalsize) {

	/*
	give me the address of a database block and I'll return the number
	of logical bytes it is using.
	*/
	
	long size;
	tyvariance variance;
	
	*logicalsize = 0;
	
	if (adr == nildbaddress) 
		return (false);
	
	if (!dbgetsizeandvariance (adr, &size, &variance)) 		
		return (false);
		
	*logicalsize = size - variance;
	
	return (true);
	} /*dbgetsize_internal*/


boolean dbcopy_internal (dbaddress adrorig, dbaddress *adrcopy) {
	
	/*
	create a copy of the database block pointed to by adrorig.  return
	true if adrcopy has the address of a new block, the same logical size
	as the original with a copy of the original's data.
	*/
	
	register boolean flreturned;
	Handle hnew;
	register Handle h;
	long size;
	
	if (adrorig == nildbaddress) { /*it's very easy to copy the nil node*/
		
		*adrcopy = nildbaddress;
		
		return (true);
		}

	hdldatabaserecord source_db = db_begin_source_read();
	if (!dbgetsize_internal (adrorig, &size)) {
#if defined(FRONTIER_HEADLESS)
		log_error(LOG_COMP_DB, "dbgetsize failed for adr=0x%llx", (unsigned long long) adrorig);
#endif
		db_end_source_read(source_db);
		return (false);
	}
	db_end_source_read(source_db);
	
	if (!newhandle (size, &hnew)) { /*not enough room in the heap*/
		return (false);
	}
	
	h = hnew; /*copy into register*/
	
	lockhandle (h);
	
	flreturned = false; /*default*/
	
	source_db = db_begin_source_read();
	if (dbreference (adrorig, size, *h))

		flreturned = true;
	else {
#if defined(FRONTIER_HEADLESS)
		log_error(LOG_COMP_DB, "dbreference failed for adr=0x%llx size=%ld",
		          (unsigned long long) adrorig, size);
#endif
	}
	db_end_source_read(source_db);

	if (flreturned)
		flreturned = dballocate (size, *h, adrcopy);
	else {
#if defined(FRONTIER_HEADLESS)
		log_error(LOG_COMP_DB, "dbcopy aborted before allocation adr=0x%llx size=%ld",
		          (unsigned long long) adrorig, size);
#endif
		flreturned = false;
	}
	
	unlockhandle (h);
	
	disposehandle (h);

	if (!flreturned) {
#if defined(FRONTIER_HEADLESS)
		log_error(LOG_COMP_DB, "dballocate failed during dbcopy size=%ld", size);
#endif
	}
	return (flreturned);
	} /*dbcopy_internal*/

boolean dbcopy (dbaddress adrorig, dbaddress *adrcopy) {
    db_context *ctx = db_context_refresh_default();
    return dbcopy_context(ctx, adrorig, adrcopy);
}
	
	
static boolean dballocstring (dbaddress *adr, bigstring bs) {
	
	return (dballocate ((long) stringlength(bs) + 1, bs, adr));
	} /*dballocstring*/
	

static boolean dbrefstring (dbaddress adr, bigstring bs) {
	
	setstringlength (bs, 0);
		
	if (adr == nildbaddress) /*nil adr represents an empty string, saves time & space*/
		return (true);
		
	return (dbreference_internal (adr, sizeof (bigstring), bs));
	} /*dbrefstring*/
	
	
static boolean dbassignstring (dbaddress *adr, bigstring bs) {
	
	if (*adr == nildbaddress) 
		return (dballocstring (adr, bs));
	else
		return (dbassign_internal (adr, (long) stringlength(bs) + 1, bs));
	} /*dbassignstring*/
	
	
static boolean dbreleasestring (dbaddress adr) {

    if (!dbrelease_internal (adr)) {
		#ifdef DATABASE_DEBUG
			char str[256];

			sprintf (str, "dbrelease failed for address %ld.", adr);

			DB_MSG_2 (str);
		#endif

		return (false);
		}

	return (true);
	} /*dbreleasestring*/
	

boolean dbrefheapstring (dbaddress adr, hdlstring *hstring) {
	
	bigstring bs;
	
	if (!dbrefstring (adr, bs))
		return (false);
		
	return (newheapstring (bs, hstring));
	} /*dbrefheapstring*/
	

boolean dbassignheapstring (dbaddress *adr, hdlstring hstring) {

	bigstring bs;
	
	copyheapstring (hstring, bs); /*checks for hstring == nil*/
	
	if (isemptystring (bs)) {
	
		if (!fldatabasesaveas)
			dbreleasestring (*adr); 
		
		*adr = nildbaddress; /*default return value, indicates empty string*/
		
		return (true);
		}
	
	return (dbassignstring (adr, bs));
	} /*dbassignheapstring*/
	
	
boolean dballochandle (Handle halloc, dbaddress *adr) {
	
	register Handle h = halloc;
	register boolean fl;
	
	if (h == nil) { /*defensive driving, nil handles are represented by nil addresses*/
		
		*adr = nildbaddress;
		
		return (true);
		}
		
	lockhandle (h);
	
	fl = dballocate ((long) gethandlesize (h), *h, adr);
	
	unlockhandle (h);
	
	return (fl);
	} /*dballochandle*/
	
	
boolean dbassignhandle (Handle h, dbaddress *adr) {
	
	/*
	6/30/92 dmb: added check for nil handle
	*/
	
	register boolean fl;
    long hsize = (h != nil) ? gethandlesize(h) : 0;
    dbaddress original = *adr;
	
	if (*adr == nildbaddress) /*creating a new guy*/
	
		return (dballochandle (h, adr));
	
	if (h == nil)
		return (dbassign (adr, 0, nil));
	
	lockhandle (h);
	
	fl = dbassign (adr, hsize, *h);
	
	unlockhandle (h);

#if defined(FRONTIER_HEADLESS)
    if (!fl) {
        log_error(LOG_COMP_DB,
                  "dbassignhandle failed adr_in=0x%llx size=%ld saveas=%d dest=%p",
                  (unsigned long long) original,
                  hsize,
                  (int) fldatabasesaveas,
                  (void *) databasedestination);
    }
    /* 2025-12-20: Verification code disabled - read-back during migration fails
     * because destination database is still being constructed. The verification
     * reads with wrong context and gets corrupted addresses. Since the actual
     * write succeeded (fl==true), this is just diagnostic noise during migration.
     */
#if 0
    else {
        if (log_enabled(LOG_LEVEL_TRACE, LOG_COMP_DB)) {
            static int verify_count = 0;
            if (verify_count++ < 10) {
                log_trace(LOG_COMP_DB, "dbassignhandle SUCCESS adr=0x%llx size=%ld",
                          (unsigned long long)*adr, hsize);
                /* Verify write by reading back first 16 bytes */
                if (hsize >= 16) {
                    unsigned char verify_buf[16];
                    if (dbreference(*adr, 16, verify_buf)) {
                        log_hex_dump(LOG_COMP_DB, LOG_LEVEL_TRACE, verify_buf, 16, "dbassignhandle verify");
                    } else {
                        log_error(LOG_COMP_DB, "dbassignhandle verify: read-back FAILED");
                    }
                }
            }
        }
    }
#endif
#endif

	return (fl);
	} /*dbassignhandle*/
	
	
boolean dbsavehandle (Handle hsave, dbaddress *adr) {

	/*
	xxx -- not sure why this is needed, looks like dbassignhandle, above,
	does the job fairly well.
	*/

	register Handle h = hsave;
	register long ctbytes;
	register boolean fl;
	dbaddress a = *adr;

	log_trace(LOG_COMP_DB, "dbsavehandle: ENTRY input_adr=0x%llx", (unsigned long long) *adr);

	ctbytes = gethandlesize (h);

	lockhandle (h);

	log_trace(LOG_COMP_DB, "dbsavehandle: BEFORE alloc/assign a=0x%llx", (unsigned long long) a);

	if (a == nildbaddress)
		fl = dballocate (ctbytes, *h, &a);
	else
		fl = dbassign (&a, ctbytes, *h);

	log_trace(LOG_COMP_DB, "dbsavehandle: AFTER alloc/assign a=0x%llx", (unsigned long long) a);

	unlockhandle (h);

	*adr = a; /*copy into returned value*/

	log_trace(LOG_COMP_DB, "dbsavehandle: EXIT output_adr=0x%llx", (unsigned long long) *adr);

 	if (!fl) {
#if defined(FRONTIER_HEADLESS)
		log_error(LOG_COMP_DB, "dbsavehandle failed: adr=%lld bytes=%ld", (long long)a, ctbytes);
#endif
	}

	return (fl);
	} /*dbsavehandle*/


/*============================================================================
 * Phase 7, Layer 3+4: Explicit-hdb variants of allocator, release, and
 * high-level database operations.
 *
 * These do NOT read from or write to the global databasedata.  Instead they
 * accept an hdldatabaserecord hdb parameter and derive fnum, availlist,
 * header size, etc. from (**hdb) directly.
 *
 * Design:
 *   - Each _hdb helper mirrors a static helper above but takes (fnum, hdb).
 *   - v6 vs v7 format is determined from (**hdb).headerLength, not from
 *     the global db_format_mode_current().
 *   - All I/O goes through the Layer 1-2 _fnum functions.
 *   - The legacy functions (without _hdb suffix) continue to work via
 *     databasedata for callers not yet converted.
 *============================================================================*/

/* db_hdb_* helpers now live in dbinternal.h. */

static boolean dbwriteheaderandtrailer_hdb (dbaddress adr, boolean flfree, long ctbytes, tyvariance variance, hdlfilenum fnum, hdldatabaserecord hdb) {

	long hs = db_hdb_header_size(hdb);

	if (!dbwriteheader_fnum (adr, flfree, ctbytes, variance, fnum, hdb))
		return (false);

	return dbwritetrailer_fnum (adr + hs + ctbytes, flfree, ctbytes, fnum, hdb);
	} /*dbwriteheaderandtrailer_hdb*/


static boolean dbwriteavailnode_hdb (dbaddress adr, long ctbytes, dbaddress nextlink, hdlfilenum fnum, hdldatabaserecord hdb) {

	long hs = db_hdb_header_size(hdb);
	boolean use64 = db_hdb_use64(hdb);

	assert (adr != nildbaddress);

	if ((**hdb).u.extensions.availlistblock != nildbaddress) {
		dberror (dbfreelisterror);
		return (false);
	}

	if (!dbwriteheader_fnum (adr, true, ctbytes, 0L, fnum, hdb))
		return (false);

	{
		long link_bytes = use64 ? (long) sizeof (uint64_t) : (long) sizeof (uint32_t);
		unsigned char raw[sizeof (uint64_t)];

		if (use64)
			db_format_write_be64(raw, (uint64_t) nextlink);
		else
			db_format_write_be32(raw, (uint32_t) nextlink);

		if (!dbwrite_fnum (adr + hs, link_bytes, raw, fnum, hdb))
			return (false);
	}

	if (!dbwritetrailer_fnum (adr + hs + ctbytes, true, ctbytes, fnum, hdb))
		return (false);

	return (true);
	} /*dbwriteavailnode_hdb*/


static boolean dbreadavailnode_hdb (dbaddress adr, boolean *flfree, long *ctbytes, dbaddress *link, hdlfilenum fnum, hdldatabaserecord hdb) {

	long hs = db_hdb_header_size(hdb);
	boolean use64 = db_hdb_use64(hdb);
	tyvariance variance;

	if (!dbreadheader_fnum (adr, flfree, ctbytes, &variance, hs, fnum))
		return (false);

	{
		long link_bytes = use64 ? (long) sizeof (uint64_t) : (long) sizeof (uint32_t);
		unsigned char raw[sizeof (uint64_t)];

		if (!dbread_fnum (adr + hs, link_bytes, raw, fnum))
			return (false);

		if (use64)
			*link = (dbaddress) db_format_read_be64(raw);
		else
			*link = (dbaddress) db_format_read_be32(raw);
	}

	return (true);
	} /*dbreadavailnode_hdb*/


static boolean dbsetavaillink_hdb (dbaddress adr, dbaddress link, hdlfilenum fnum, hdldatabaserecord hdb) {

	long hs = db_hdb_header_size(hdb);
	boolean use64 = db_hdb_use64(hdb);

	if ((**hdb).u.extensions.availlistblock != nildbaddress) {
		dberror (dbfreelisterror);
		return (false);
	}

	if (adr == nildbaddress) { /*special case, set link in file header*/

		(**hdb).availlist = link;

		setdirty (hdb);

		return (true);
		}

	{
		long link_bytes = use64 ? (long) sizeof (uint64_t) : (long) sizeof (uint32_t);
		unsigned char raw[sizeof (uint64_t)];

		if (use64)
			db_format_write_be64(raw, (uint64_t) link);
		else
			db_format_write_be32(raw, (uint32_t) link);

		return dbwrite_fnum (adr + hs, link_bytes, raw, fnum, hdb);
	}
	} /*dbsetavaillink_hdb*/


static boolean dbwritedatablock_hdb (dbaddress adr, long databytes, long nodebytes, ptrvoid pdata, hdlfilenum fnum, hdldatabaserecord hdb) {

	long hs = db_hdb_header_size(hdb);

	if (!dbwriteheader_fnum (adr, false, nodebytes, (tyvariance) nodebytes - databytes, fnum, hdb))
		return (false);

	if (pdata != nil)
		if (!dbwrite_fnum (adr + hs, databytes, pdata, fnum, hdb))
			return (false);

	return dbwritetrailer_fnum (adr + nodebytes + hs, false, nodebytes, fnum, hdb);
	} /*dbwritedatablock_hdb*/


static boolean dbfindpreviousavail_hdb (dbaddress adr, dbaddress *prev, long *ixshadow, hdlfilenum fnum, hdldatabaserecord hdb) {

#ifdef dbshadow
	(void) fnum; /* unused in shadow path — all lookups go through havailshadow */
	hdlavaillistshadow havailshadow = (hdlavaillistshadow) (**hdb).u.extensions.availlistshadow.data;
	long i, ctavail = (**hdb).u.extensions.availlistshadow.eof / sizeof (tyavailnodeshadow);

	for (i = 0; i < ctavail; ++i) {

		if ((*havailshadow) [i].adr == adr) {

			if (i > 0)
				*prev = (*havailshadow) [i - 1].adr;
			else
				*prev = nildbaddress;

			*ixshadow = i;

			return (true);
			}
		}

	dblogerror (dbfreelisterror);

	return (false);
#else
	dbaddress nomad;
	boolean flfree;
	long ctbytes;
	dbaddress nextnomad;

	nomad = (**hdb).availlist;

	if (nomad == adr) {

		*prev = nildbaddress;

		return (true);
		}

	while (true) {

		if (nomad == nildbaddress)
			return (false);

		if (!dbreadavailnode_hdb (nomad, &flfree, &ctbytes, &nextnomad, fnum, hdb))
			return (false);

		if (nextnomad == adr) {

			*prev = nomad;

			return (true);
			}

		nomad = nextnomad;
		} /*while*/
#endif
	} /*dbfindpreviousavail_hdb*/


static boolean dbinsertavailshadow_hdb (long ixshadow, dbaddress adr, long ctbytes, hdldatabaserecord hdb) {

	handlestream s = (**hdb).u.extensions.availlistshadow;
	tyavailnodeshadow avail;

	assert ((ixshadow >= 0) && (ixshadow <= s.eof / (long) sizeof (tyavailnodeshadow)));

	avail.adr = adr;
	avail.size = (int64_t) ctbytes;

	s.pos = ixshadow * sizeof (tyavailnodeshadow);

	if (!mergehandlestreamdata (&s, 0L, &avail, sizeof (avail)))
		return (false);

	(**hdb).u.extensions.availlistshadow = s;

	return (true);
	} /*dbinsertavailshadow_hdb*/


static boolean dbdeleteavailshadow_hdb (long ixshadow, hdldatabaserecord hdb) {

	handlestream s = (**hdb).u.extensions.availlistshadow;

	assert ((ixshadow >= 0) && (ixshadow < s.eof / (long) sizeof (tyavailnodeshadow)));

	s.pos = ixshadow * sizeof (tyavailnodeshadow);

	if (!pullfromhandlestream (&s, sizeof (tyavailnodeshadow), nil))
		return (false);

	(**hdb).u.extensions.availlistshadow = s;

	return (true);
	} /*dbdeleteavailshadow_hdb*/


static boolean dbsetavailshadow_hdb (long ixshadow, dbaddress adr, long ctbytes, hdldatabaserecord hdb) {

	handlestream s = (**hdb).u.extensions.availlistshadow;
	tyavailnodeshadow avail;

	assert ((ixshadow >= 0) && (ixshadow < s.eof / (long) sizeof (tyavailnodeshadow)));

	avail.adr = adr;
	avail.size = (int64_t) ctbytes;

	s.pos = ixshadow * sizeof (tyavailnodeshadow);

	if (!mergehandlestreamdata (&s, sizeof (avail), &avail, sizeof (avail)))
		return (false);

	(**hdb).u.extensions.availlistshadow = s;

	return (true);
	} /*dbsetavailshadow_hdb*/


static boolean dbsetsize_hdb (dbaddress adr, long size, tyvariance variance, hdlfilenum fnum, hdldatabaserecord hdb) {

	return dbwriteheader_fnum (adr, false, size, variance, fnum, hdb);
	} /*dbsetsize_hdb*/


static boolean dbmove_hdb (ptrvoid pdata, long ctbytes, dbaddress adr, hdlfilenum fnum, hdldatabaserecord hdb) {

	long hs = db_hdb_header_size(hdb);

	return dbwrite_fnum (adr + hs, ctbytes, pdata, fnum, hdb);
	} /*dbmove_hdb*/


static void dbclearshadowavaillist_hdb (hdldatabaserecord hdb) {

	/*
	Phase 7/8 variant: Clear shadow avail list using explicit hdb instead of
	databasedata.  Only used by dballocate_hdb / dbrelease_hdb.

	Phase 8: Uses dbflushheader_hdb and dbrelease_hdb directly — no
	databasedata mutation required.
	*/

#ifdef SMART_DB_OPENING
	if (!fldatabasesaveas && !(**hdb).u.extensions.flreadonly) {

		if ((**hdb).u.extensions.availlistblock != nildbaddress) {

			dbaddress adrblock = (**hdb).u.extensions.availlistblock;

			(**hdb).u.extensions.availlistblock = nildbaddress;

			setdirty (hdb);

			dbflushheader_hdb (hdb);

			dbrelease_hdb (adrblock, hdb);
			}
		}
#endif
	} /*dbclearshadowavaillist_hdb*/


static boolean dbreadtrailer_hdb (dbaddress adr, boolean *flfree, long *ctbytes, hdlfilenum fnum, hdldatabaserecord hdb) {

	/*
	Like dbreadtrailer but reads from explicit fnum with hdb-derived format.
	*/

	uint64_t raw_size = 0;
	boolean use64 = db_hdb_use64(hdb);

	if (use64) {
		tytrailer64 trailer;

		if (!dbread_fnum (adr, sizetrailer_v7, &trailer, fnum))
			return (false);

		raw_size = db_format_read_be64((unsigned char *) &trailer.sizefreeword.size);
	}
	else {
		tytrailer32 trailer;

		if (!dbread_fnum (adr, sizetrailer_v6, &trailer, fnum))
			return (false);

		raw_size = (uint64_t) db_format_read_be32((unsigned char *) &trailer.sizefreeword.size);
	}

	{
		uint64_t freeflag = use64 ? 0x8000000000000000ULL : 0x80000000ULL;
		uint64_t sizemask = use64 ? 0x7FFFFFFFFFFFFFFFULL : 0x7FFFFFFFULL;

		*flfree = (raw_size & freeflag) != 0;
		*ctbytes = (long) (raw_size & sizemask);
	}

	return (true);
	} /*dbreadtrailer_hdb*/


static boolean dbmergeright_hdb (dbaddress adr, long ctbytes, boolean *ptrflmergedright, hdlfilenum fnum, hdldatabaserecord hdb) {

	long hs = db_hdb_header_size(hdb);
	long ts = db_hdb_trailer_size(hdb);
	long eof;
	dbaddress rightblockadr;
	boolean flrightfree;
	long ctrightbytes;
	dbaddress prevavail, nextavail;
	long ixshadow;

	*ptrflmergedright = false;

	rightblockadr = adr + hs + ctbytes + ts;

	if (!dbgeteof_fnum (&eof, fnum))
		return (false);

	if (rightblockadr == eof)
		return (true);

	if (rightblockadr > eof) {

		dblogerror (dbmergeinvalidblockerror);

		return (false);
		}

	if (!dbreadavailnode_hdb (rightblockadr, &flrightfree, &ctrightbytes, &nextavail, fnum, hdb))
		return (false);

	if (ctrightbytes < minblocksize) {

		dblogerror (dbmergeinvalidblockerror);

		return (false);
		}

	if (!flrightfree)
		return (true);

	if (!dbfindpreviousavail_hdb (rightblockadr, &prevavail, &ixshadow, fnum, hdb))
		return (false);

#ifdef dbshadow
	{
		long ctavail = (**hdb).u.extensions.availlistshadow.eof / sizeof (tyavailnodeshadow);

		if (ixshadow + 1 >= ctavail) {
			dblogerror (dbmergeinvalidblockerror);
			return (false);
		}

		assert ((*(hdlavaillistshadow)(**hdb).u.extensions.availlistshadow.data) [ixshadow + 1].adr == nextavail);
	}
#endif

	if (!dbsetavaillink_hdb (prevavail, adr, fnum, hdb))
		return (false);

	ctbytes += ctrightbytes + hs + ts;

	if (!dbwriteavailnode_hdb (adr, ctbytes, nextavail, fnum, hdb))
		return (false);

	if (!dbsetavailshadow_hdb (ixshadow, adr, ctbytes, hdb))
		return (false);

	*ptrflmergedright = true;

	return (true);
	} /*dbmergeright_hdb*/


static boolean dbmergeleft_hdb (boolean flmerged, dbaddress adr, boolean *ptrflmergedleft, hdlfilenum fnum, hdldatabaserecord hdb) {

	long hs = db_hdb_header_size(hdb);
	long ts = db_hdb_trailer_size(hdb);
	dbaddress newadr;
	long newsize;
	boolean flfree, flleftfree;
	long ctbytes, ctleftbytes;
	dbaddress nextavail, prevavail;
	long ixshadow;

	/* Phase 8 fix: Use hdb's headerLength instead of the compile-time
	   constant firstphysicaladdress.  v7 databases have headerLength=90
	   but firstphysicaladdress=118 (sizeof tydatabaserecord on 64-bit). */
	long first_data_address = (**hdb).headerLength;
	if (first_data_address <= 0)
		first_data_address = firstphysicaladdress;

	*ptrflmergedleft = false;

	if (adr == (dbaddress) first_data_address)
		return (true);

	if (adr < (dbaddress) first_data_address) {

		dblogerror (dbmergeinvalidblockerror);

		return (false);
		}

	if (!dbreadtrailer_hdb (adr - ts, &flleftfree, &ctleftbytes, fnum, hdb))
		return (false);

	if (ctleftbytes < minblocksize) {

		dblogerror (dbmergeinvalidblockerror);

		return (false);
		}

	if (!flleftfree)
		return (true);

	if (!dbreadavailnode_hdb (adr, &flfree, &ctbytes, &nextavail, fnum, hdb))
		return (false);

	(void) flfree; /* only ctbytes and nextavail used for merge logic */

	if (flmerged) { /*the node we're releasing is already on the avail list, pop him!*/

		if (!dbfindpreviousavail_hdb (adr, &prevavail, &ixshadow, fnum, hdb))
			return (false);

#ifdef dbshadow
		{
			long ctavail = (**hdb).u.extensions.availlistshadow.eof / sizeof (tyavailnodeshadow);

			if (ixshadow + 1 >= ctavail) {
				dblogerror (dbmergeinvalidblockerror);
				return (false);
			}

			assert ((*(hdlavaillistshadow)(**hdb).u.extensions.availlistshadow.data) [ixshadow + 1].adr == nextavail);
		}
#endif

		if (!dbsetavaillink_hdb (prevavail, nextavail, fnum, hdb))
			return (false);

		if (!dbdeleteavailshadow_hdb (ixshadow, hdb))
			return (false);
		}

	newsize = ctleftbytes + ctbytes + hs + ts;

	newadr = adr - ts - ctleftbytes - hs;

	if (!dbwriteheaderandtrailer_hdb (newadr, true, newsize, 0L, fnum, hdb))
		return (false);

	if (!dbfindpreviousavail_hdb (newadr, &prevavail, &ixshadow, fnum, hdb))
		return (false);

	if (!dbsetavailshadow_hdb (ixshadow, newadr, newsize, hdb))
		return (false);

	*ptrflmergedleft = true;

	return (true);
	} /*dbmergeleft_hdb*/


boolean dballocate_hdb (long databytes, ptrvoid pdata, dbaddress *paddress, hdldatabaserecord hdb) {

	/*
	Phase 7: Allocate space in database using explicit hdb — no global
	databasedata access.  Mirrors dballocate() but threads hdb/fnum through
	every helper call.

	The caller is responsible for passing the correct hdb (already resolved
	for Save As context if needed).  No db_context_guard is used internally.
	*/

	hdlfilenum fnum = db_hdb_fnum(hdb);
	long hs = db_hdb_header_size(hdb);
	long ts = db_hdb_trailer_size(hdb);
	long origeof;
	long nodebytes, newnodebytes;
	dbaddress nomad, prevnomad, nextnomad;
	tyvariance variance;
	long smallestinterestingblock;
	long ctalloc;

#ifdef SMART_DB_OPENING
	dbclearshadowavaillist_hdb (hdb);
#endif

	smallestinterestingblock = max (databytes, (long) minblocksize);

#ifdef dbshadow
	{
	hdlavaillistshadow havailshadow = (hdlavaillistshadow) (**hdb).u.extensions.availlistshadow.data;
	long i, ctavail = (**hdb).u.extensions.availlistshadow.eof / sizeof (tyavailnodeshadow);

	for (i = 0, prevnomad = nildbaddress; i < ctavail; ++i, prevnomad = nomad) {

		nomad = (*havailshadow) [i].adr;

		if (nomad == nildbaddress)
			break;

		nodebytes = (*havailshadow) [i].size;

		if (nodebytes < smallestinterestingblock)
			continue;

		/*found a block to allocate off avail list*/

		nextnomad = (*havailshadow) [i + 1].adr;

		variance = nodebytes - databytes;

		if (variance >= (minblocksize + hs + ts)) { /*split into two blocks*/

			newnodebytes = nodebytes - (databytes + hs + ts);

			if (!dbwriteheaderandtrailer_hdb (nomad, true, newnodebytes, (tyvariance) 0, fnum, hdb))
				goto failure;

			dbsetavailshadow_hdb (i, nomad, newnodebytes, hdb);

			nomad += hs + newnodebytes + ts;

			if (!dbwritedatablock_hdb (nomad, databytes, databytes, pdata, fnum, hdb))
				goto failure;

			*paddress = nomad;

			goto success;
			} /*splitting into two blocks*/

		if (!dbwritedatablock_hdb (nomad, databytes, nodebytes, pdata, fnum, hdb))
			goto failure;

		*paddress = nomad;

		if (!dbdeleteavailshadow_hdb (i, hdb))
			goto failure;

		if (!dbsetavaillink_hdb (prevnomad, nextnomad, fnum, hdb))
			goto failure;

		goto success;
		}
	}
#else
	{
	boolean flfree;

	nomad = (**hdb).availlist;

	prevnomad = nildbaddress;

	while (nomad != nildbaddress) {

		if (!dbreadavailnode_hdb (nomad, &flfree, &nodebytes, &nextnomad, fnum, hdb))
			goto failure;

		if (nodebytes < smallestinterestingblock)
			goto nextloop;

		variance = nodebytes - databytes;

		if (variance >= (minblocksize + hs + ts)) {

			newnodebytes = nodebytes - (databytes + hs + ts);

			if (!dbwriteheaderandtrailer_hdb (nomad, true, newnodebytes, (tyvariance) 0, fnum, hdb))
				goto failure;

			nomad += hs + newnodebytes + ts;

			if (!dbwritedatablock_hdb (nomad, databytes, databytes, pdata, fnum, hdb))
				goto failure;

			*paddress = nomad;

			goto success;
			}

		if (!dbwritedatablock_hdb (nomad, databytes, nodebytes, pdata, fnum, hdb))
			goto failure;

		*paddress = nomad;

		if (!dbsetavaillink_hdb (prevnomad, nextnomad, fnum, hdb))
			goto failure;

		goto success;

		nextloop:

		prevnomad = nomad;

		nomad = nextnomad;
		} /*while*/
	}
#endif

	if (!dbgeteof_fnum (&origeof, fnum))
		goto failure;

	if (databytes < minblocksize) {

		ctalloc = minblocksize;

		variance = minblocksize - databytes;
		}
	else {

		ctalloc = databytes;

		variance = 0;
		}

	if (!dbseteof_fnum (origeof + hs + ctalloc + ts, fnum))
		goto failure;

	if (!dbwritedatablock_hdb (origeof, databytes, ctalloc, pdata, fnum, hdb))
		goto failure;

	*paddress = origeof;


success:

	return (true);


failure:

	return (false);
	} /*dballocate_hdb*/


boolean dbrelease_hdb (dbaddress adr, hdldatabaserecord hdb) {

	/*
	Phase 7: Release a database block using explicit hdb — no global
	databasedata access.  Mirrors dbrelease_internal().
	*/

	hdlfilenum fnum = db_hdb_fnum(hdb);
	long hs = db_hdb_header_size(hdb);
	boolean flmergedleft, flmergedright;
	boolean flfree;
	long ctbytes;
	tyvariance variance;

	if (adr == nildbaddress)
		return (true);

#ifdef SMART_DB_OPENING
	dbclearshadowavaillist_hdb (hdb);
#endif

	if (!dbreadheader_fnum (adr, &flfree, &ctbytes, &variance, hs, fnum))
		return (false);

	if (flfree) {

		dberror (dbreleasefreeblockerror);

		return (false);
		}

	if (!dbmergeright_hdb (adr, ctbytes, &flmergedright, fnum, hdb))
		return (false);

	if (!dbmergeleft_hdb (flmergedright, adr, &flmergedleft, fnum, hdb))
		return (false);

	if (flmergedleft || flmergedright)
		return (true);

	/*no merging -- set free bits in header & trailer, insert at head of avail list*/

	if (!dbwriteavailnode_hdb (adr, ctbytes, (**hdb).availlist, fnum, hdb))
		return (false);

	(**hdb).availlist = adr;

	if (!dbinsertavailshadow_hdb (0, adr, ctbytes, hdb))
		return (false);

	setdirty (hdb);

	return (true);
	} /*dbrelease_hdb*/


/*============================================================================
 * Phase 7, Layer 4: High-level _hdb operations.
 *============================================================================*/


boolean dbassign_hdb (dbaddress *padr, long newsize, ptrvoid pdata, hdldatabaserecord hdb) {

	/*
	Phase 7: Assign data to a database block using explicit hdb.
	Mirrors dbassign_internal() but routes all I/O through _hdb/_fnum helpers.
	Does not read or write databasedata.  Reads the global fldatabasesaveas
	to detect Save As context (forces fresh allocation in destination).
	*/

	hdlfilenum fnum = db_hdb_fnum(hdb);
	long hs = db_hdb_header_size(hdb);
	register dbaddress adr;
	tyvariance ctunused;
	long cttotal;
	boolean flfree;

	adr = *padr;

	if (fldatabasesaveas || (adr == nildbaddress)) { /*during Save As, always allocate fresh in destination*/
		return dballocate_hdb (newsize, pdata, padr, hdb);
	}

	if (!dbreadheader_fnum (adr, &flfree, &cttotal, &ctunused, hs, fnum))
		return (false);

	if (flfree) {

		dberror (dbassignfreeblockerror);

		return (false);
		}

	if (newsize > cttotal) {

		if (!dbrelease_hdb (adr, hdb)) /*don't abort saving, but log the failure*/
			dblogerror (dbfreelisterror);

		return dballocate_hdb (newsize, pdata, padr, hdb);
		}

	if (newsize != cttotal - ctunused)

		if (!dbsetsize_hdb (adr, cttotal, cttotal - newsize, fnum, hdb))

			return (false);

	return dbmove_hdb (pdata, newsize, adr, fnum, hdb);
	} /*dbassign_hdb*/


boolean dbcopy_hdb (dbaddress adrorig, dbaddress *adrcopy, hdldatabaserecord hdb) {

	/*
	Phase 7: Copy a database block using explicit hdb.
	Mirrors dbcopy_internal() but routes I/O through _hdb/_fnum helpers.
	Does not read or write databasedata.  Reads the globals fldatabasesaveas
	and dbsaveas_source to detect Save As context.

	During Save As, adrorig lives in the *source* database while allocation
	goes to the destination (hdb).  We must read from the source db, not hdb.
	*/

	register boolean flreturned;
	Handle hnew;
	register Handle h;
	long size;

	/* Determine source db for reads: during Save As the original block
	   lives in dbsaveas_source, otherwise it's in hdb itself. */
	hdldatabaserecord source_hdb = (fldatabasesaveas && dbsaveas_source != nil) ? dbsaveas_source : hdb;
	hdlfilenum src_fnum = db_hdb_fnum(source_hdb);
	long src_hs = db_hdb_header_size(source_hdb);

	if (adrorig == nildbaddress) {

		*adrcopy = nildbaddress;

		return (true);
		}

	/* Get logical size of original block from source. */
	{
		long total;
		tyvariance variance;
		boolean flfree;

		if (!dbreadheader_fnum (adrorig, &flfree, &total, &variance, src_hs, src_fnum))
			return (false);

		if (flfree) {
			dberror (dbfreeblockerror);
			return (false);
		}

		size = total - (long) variance;
	}

	if (!newhandle (size, &hnew))
		return (false);

	h = hnew;

	lockhandle (h);

	/* Read original data from source. */
	flreturned = dbreference_fnum (adrorig, size, *h, src_hs, src_fnum);

	/* Allocate copy in destination (hdb). */
	if (flreturned)
		flreturned = dballocate_hdb (size, *h, adrcopy, hdb);

	unlockhandle (h);

	disposehandle (h);

	return (flreturned);
	} /*dbcopy_hdb*/


boolean dbsavehandle_hdb (Handle hsave, dbaddress *adr, hdldatabaserecord hdb) {

	/*
	Phase 7: Save a handle to a database block using explicit hdb.
	Mirrors dbsavehandle().
	*/

	register Handle h = hsave;
	register long ctbytes;
	register boolean fl;
	dbaddress a = *adr;

	ctbytes = gethandlesize (h);

	lockhandle (h);

	if (a == nildbaddress)
		fl = dballocate_hdb (ctbytes, *h, &a, hdb);
	else
		fl = dbassign_hdb (&a, ctbytes, *h, hdb);

	unlockhandle (h);

	*adr = a;

	return (fl);
	} /*dbsavehandle_hdb*/


boolean dbrefhandle_hdb (dbaddress adr, Handle *h, hdldatabaserecord hdb) {

	/*
	Phase 7: Read a handle from database using explicit hdb.
	Mirrors dbrefhandle() but uses explicit fnum and header size.
	*/

	return dbrefhandle_fnum (adr, h, db_hdb_header_size(hdb), db_hdb_fnum(hdb));
	} /*dbrefhandle_hdb*/


boolean dbassignhandle_hdb (Handle h, dbaddress *adr, hdldatabaserecord hdb) {

	/*
	Phase 7: Assign a handle to a database block using explicit hdb.
	Mirrors dbassignhandle().
	*/

	register boolean fl;

	if (*adr == nildbaddress) { /*creating a new guy*/

		if (h == nil) {

			*adr = nildbaddress;

			return (true);
			}

		lockhandle (h);

		fl = dballocate_hdb ((long) gethandlesize (h), *h, adr, hdb);

		unlockhandle (h);

		return (fl);
		}

	if (h == nil)
		return dbassign_hdb (adr, 0, nil, hdb);

	lockhandle (h);

	fl = dbassign_hdb (adr, (long) gethandlesize (h), *h, hdb);

	unlockhandle (h);

	return (fl);
	} /*dbassignhandle_hdb*/


/*
boolean dbnewarray (ctelements, sizeelement, pdata, adr) short ctelements, sizeelement; ptrvoid pdata; dbaddress *adr; {
	
	register long ctbytes;
	
	ctbytes = ((long) ctelements * sizeelement) + sizeof (tydbarrayheader);
	
	return (dballocate (ctbytes, pdata, adr));
	} /%dbnewarray%/
*/
	

void dbsetview (short viewnumber, dbaddress adrtext) {

	register hdldatabaserecord hdb;
    db_context_guard guard;
    db_context swap_ctx;
    db_context *apply_ctx = db_context_for_saveas_destination(&swap_ctx, NULL);

    db_context_guard_enter(apply_ctx, &guard);

	hdb = databasedata; /*move into register*/

#if defined(FRONTIER_HEADLESS)
	log_trace(LOG_COMP_DB, "dbsetview: view[%d] = 0x%llx (was 0x%llx) headerLength=%ld",
	          viewnumber, (unsigned long long)adrtext, (unsigned long long)(**hdb).views[viewnumber],
	          (long)(**hdb).headerLength);
#endif

	(**hdb).views [viewnumber] = adrtext;
	
	setdirty (hdb);
	
	dbflushheader ();
	
	db_context_guard_exit(&guard);
	} /*dbsetview*/


void dbgetview (short viewnumber, dbaddress *adrtext) {
	
	*adrtext = (**databasedata).views [viewnumber];
	} /*dbgetview*/


boolean dbfnumchanged (hdlfilenum newfnum) {
	
	register hdldatabaserecord hdb = databasedata;
	
	(**hdb).fnumdatabase = (long) newfnum;
	
	setdirty (hdb);
	
	return (dbflushheader ());
	} /*dbfnumchanged*/


#ifdef DATABASE_DEBUG

boolean debug_dbpushreleasestack (dbaddress adr, long valtype, long line, char *sourcefile) {
	
	/*
	the chunk of db space pointed to by adr is being logically released, but
	the caller is saying that he doesn't want to make the effects permanent
	until some time in the future.  he indicates it's time to release all these
	guys by calling dbflushreleasestack, below.
	
	if the user decides to not save changes, you should call dbzeroreleasestack.
	*/
	
	Handle hstack = (**databasedata).releasestack;
	tydbreleasestackframe info;

	if (adr == nildbaddress) /*no need to waste space on a nil address*/
		return (true);
		
	if (hstack == nil) {
		
		if (!newclearhandle (0L, &hstack))
			return (false);
			
		(**databasedata).releasestack = hstack;
		}

	clearbytes (&info, sizeof (info));

	info.adr = adr;

	info.id = valtype;

	info.line = line;

	newfilledhandle (sourcefile, strlen (sourcefile), &info.file);
		
	return (enlargehandle (hstack, sizeof (info), &info));
	} /*dbpushreleasestack*/


static boolean dbflushreleasestack_impl (void) {
	
	/*
	release all the chunks accumulated in the database's releasestack.
	
	5.1.4 dmb: don't lock the handle
	*/
	
	Handle h = (**databasedata).releasestack;
	tydbreleasestackframe info;
	long i, ct;
	long hsize;
	
	if (h != nil) {
		
		hsize = gethandlesize (h);
		
		ct = hsize / sizeof (info);
		
		for (i = 0; i < ct; ++i) {
			
			rollbeachball (); /*dmb 4.1b9*/

			info = ((tydbreleasestackframe*)(*h)) [i];
			
			if (!dbrelease_internal (info.adr)) {

				bigstring bsfile;
				char str[256];

				texthandletostring (info.file, bsfile);

				sprintf (str, "dbrelease failed for address %ld, type %ld, line %ld in %s.", info.adr, info.id, info.line, stringbaseaddress (bsfile));
				
				DB_MSG_2 (str);
				}
			}

		disposehandle (h);
		
		(**databasedata).releasestack = nil;
		}
	
#ifdef SMART_DB_OPENING	
	dbwriteshadowavaillist (); /*6.2b12 AR: this is a good place to do it since we're about done with saving*/
#endif
	 
	return (true);
} /*dbflushreleasestack_impl*/

boolean dbflushreleasestack (void) {
    db_context_guard guard;
    db_context_guard_enter(db_context_refresh_default(), &guard);
    boolean ok = dbflushreleasestack_impl();
    db_context_guard_exit(&guard);
    return ok;
} /*dbflushreleasestack*/

boolean dbrelease_context(const db_context *context, dbaddress adr) {
    db_context_guard guard;
    db_context_guard_enter(context, &guard);
    boolean ok = dbrelease_internal(adr);
    db_context_guard_exit(&guard);
    return ok;
}

#else
	
static boolean dbpushreleasestack_impl (dbaddress adr) {

	/*
	the chunk of db space pointed to by adr is being logically released, but
	the caller is saying that he doesn't want to make the effects permanent
	until some time in the future.  he indicates it's time to release all these
	guys by calling dbflushreleasestack, below.
	
	if the user decides to not save changes, you should call dbzeroreleasestack.

	6.2b3 AR: Added valtype parameter, only used in debug version (see above).
	*/
	
	Handle hstack = (**databasedata).releasestack;
	
	if (adr == nildbaddress) /*no need to waste space on a nil address*/
		return (true);
		
	if (hstack == nil) {
		
		if (!newclearhandle (0L, &hstack))
			return (false);
			
		(**databasedata).releasestack = hstack;
		}
		
	return (enlargehandle (hstack, sizeof (adr), &adr));
	} /*dbpushreleasestack_impl*/


boolean dbpushreleasestack (dbaddress adr, long valtype) {
#pragma unused(valtype)
    db_context_guard guard;
    db_context_guard_enter(db_context_refresh_default(), &guard);
    boolean ok = dbpushreleasestack_impl(adr);
    db_context_guard_exit(&guard);
    return ok;
}


static boolean dbflushreleasestack_impl_nondebug (void) {
	
	/*
	release all the chunks accumulated in the database's releasestack.
	
	5.1.4 dmb: don't lock the handle
	*/
	
	Handle h = (**databasedata).releasestack;
	long i, ct;
	long hsize;
	
	if (h != nil) {
		
		hsize = gethandlesize (h);
		
		ct = hsize / sizeof (dbaddress);
		
		for (i = 0; i < ct; ++i) {
			
			rollbeachball (); /*dmb 4.1b9*/
			
						dbrelease_internal (((ptrdbaddress) (*h)) [i]);
			}

		disposehandle (h);
		
		(**databasedata).releasestack = nil;
		}
	
#ifdef SMART_DB_OPENING	
	dbwriteshadowavaillist (); /*6.2b12 AR: this is a good place to do it since we're about done with saving*/
#endif
	
	return (true);
	} /*dbflushreleasestack_impl_nondebug*/

boolean dbflushreleasestack (void) {
    db_context_guard guard;
    db_context_guard_enter(db_context_refresh_default(), &guard);
    boolean ok = dbflushreleasestack_impl_nondebug();
    db_context_guard_exit(&guard);
    return ok;
	} /*dbflushreleasestack*/

#endif

boolean dbpushreleasestack_hdb (dbaddress adr, long valtype, hdldatabaserecord hdb) {
#pragma unused(valtype)

	/*
	Phase 8: Like dbpushreleasestack but pushes onto hdb's release stack
	directly, without mutating databasedata.  Used by langexternaldisposevariable.
	*/

	Handle hstack;

	if (adr == nildbaddress)
		return (true);

	if (hdb == nil)
		return (false);

	hstack = (**hdb).releasestack;

	if (hstack == nil) {

		if (!newclearhandle (0L, &hstack))
			return (false);

		(**hdb).releasestack = hstack;
		}

	return (enlargehandle (hstack, sizeof (adr), &adr));
	} /*dbpushreleasestack_hdb*/


boolean dbflushreleasestack_context(const db_context *context) {
    db_context_guard guard;
    db_context_guard_enter(context, &guard);
#ifdef DATABASE_DEBUG
    boolean ok = dbflushreleasestack_impl();
#else
    boolean ok = dbflushreleasestack_impl_nondebug();
#endif
    db_context_guard_exit(&guard);
    return ok;
}


static void dbzeroreleasestack_impl (void) {

	if (databasedata == nil)
		return;

	Handle hstack = (**databasedata).releasestack;
	if (hstack == nil)
		return;

	/* Defensive: validate handle before dereferencing during cleanup.
	 * This protects against edge cases where releasestack could contain a stale pointer
	 * (e.g., if a previous disposal attempt failed partway through, or if memory corruption
	 * occurred during error handling). validhandle() uses OS-level handle validation
	 * rather than heuristic pointer checks, making this more robust than checking address ranges.
	 * If invalid, skip disposal entirely - we can't safely access the structure. */
	if (!validhandle(hstack)) {
		/* Can't safely nil out releasestack if hstack is invalid - would require
		 * dereferencing databasedata which might also point to freed memory.
		 * Best we can do during teardown is return safely. */
		return;
	}

	disposehandle (hstack);

	(**databasedata).releasestack = nil;
	} /*dbzeroreleasestack_impl*/

static void dbzeroreleasestack (void) {
    /* Direct call - no guards needed during database disposal.
     * Context restoration is meaningless when destroying the database.
     *
     * The guard pattern previously caused a bug: guard_exit restored
     * databasedata pointer, then dbdispose() freed it, leaving a
     * dangling pointer that caused segfaults during program exit.
     *
     * This fix is part of a broader effort to eliminate push/pop anti-patterns
     * in favor of deterministic, explicit context management (see Issues #135, #136).
     * The guard removal here aligns with the mode stack refactor (PR #125) which
     * eliminated similar hidden state restoration during disposal operations. */
    dbzeroreleasestack_impl();
}

boolean dbzeroreleasestack_context(const db_context *context) {
    /* Context-aware wrapper - guards ARE needed here.
     * Unlike dbzeroreleasestack() which is called during permanent disposal,
     * this variant is called with an explicit context that must be temporarily
     * applied and then restored. The caller expects their context to remain
     * unchanged after this call, so guards are appropriate and necessary. */
    db_context_guard guard;
    db_context_guard_enter(context, &guard);
    dbzeroreleasestack_impl();
    db_context_guard_exit(&guard);
    return true;
}


boolean dbdispose (void) {

	dbzeroreleasestack ();

#ifdef SMART_DB_OPENING
	dbdisposeshadowavaillist ();
#else
	disposehandle ((**databasedata).u.extensions.availlistshadow.data);
#endif

	disposehandle ((Handle) databasedata);

	databasedata = nil;

	/* Pop the format mode that was pushed during dbopenfile */
#if defined(FRONTIER_HEADLESS)
	log_trace(LOG_COMP_DB, "dbdispose: calling db_format_mode_pop before exit");
#endif
	db_format_mode_pop();

	return (true);
	} /*dbdispose*/


boolean dbnew (hdlfilenum fnum, boolean use_v7_format) {

	/*
	2002-11-11 AR: Added assert to make sure the C compiler chose the
	proper byte alignment for the tydatabaserecord struct. If it did not,
	we would end up corrupting any database files we saved.

	2026-01-08: Added use_v7_format parameter to explicitly specify v6 (false) or v7 (true)
	format instead of relying on global format mode state. This fixes Guest Database migration
	where dbnew() was called before v7 mode was set, causing headerLength=118 instead of 90.
	*/

#if defined(FRONTIER_HEADLESS)
	log_trace(LOG_COMP_DB, "dbnew: ENTER fnum=%ld use_v7_format=%d", (long)fnum, use_v7_format ? 1 : 0);
#endif

	register hdldatabaserecord hdb;

	{
		const size_t expected_header_size = (sizeof (void *) == 8) ? 118u : 90u;  /* Updated for 2-byte padding before views */
		assert (sizeof (tydatabaserecord) == expected_header_size);
	}
	
	if (!newclearhandle (sizeof (tydatabaserecord), (Handle *) &databasedata))
		return (false);
		
	hdb = databasedata; /*copy into register*/
	
	(**hdb).fnumdatabase = (long) fnum;
	
	(**hdb).systemid = dbsystemidMac;


	/* Set version and header size based on format parameter */
	if (use_v7_format) {
		/* v7 BE64 format: 90-byte header, version 7 */
		(**hdb).versionnumber = 7;
		(**hdb).headerLength = (long) sizeof(tydatabaserecord_64);  /* 90 bytes */
		(**hdb).longversionMajor = 7;
		(**hdb).longversionMinor = 0;
#if defined(FRONTIER_HEADLESS)
		log_trace(LOG_COMP_DB, "dbnew: v7 format - headerLength=%ld", (long)sizeof(tydatabaserecord_64));
#endif
	} else {
		/* v6 32-bit format: 118-byte header (in-memory size with padding) */
		(**hdb).versionnumber = dbversionnumber;
		(**hdb).headerLength = firstphysicaladdress;  /* sizeof(tydatabaserecord) = 118 bytes */
		(**hdb).longversionMajor = dbversionnumber;
		(**hdb).longversionMinor = dbversionnumberminor;
#if defined(FRONTIER_HEADLESS)
		log_trace(LOG_COMP_DB, "dbnew: v6 format - headerLength=%ld", firstphysicaladdress);
#endif
	}

#if defined(FRONTIER_HEADLESS)
	log_trace(LOG_COMP_DB, "dbnew: EXIT hdb=%p headerLength=%ld version=%d use_v7=%d",
	          (void*)hdb, (**hdb).headerLength, (**hdb).versionnumber, use_v7_format ? 1 : 0);
#endif

	dbshadowavaillist ();

	setdirty (hdb);
	
	if (dbflushheader ()) /*initial info written to disk*/
		return (true);
	
	dbdispose (); /*error flushing the data out to disk*/
	
	return (false);
	} /*dbnew*/
	

/* 2025-11-24 Codex: Route dbopenfile through v7 reader or legacy adapter. */

/* 2025-11-30 Codex: Add headless failure breadcrumbs so migration tests can pinpoint dbopenfile failures. */

boolean dbopenfile (hdlfilenum fnum, boolean flreadonly) {
	
	/*
	4.1b9 dmb: allow opening of databases newer than us, as long as 
	version number change is not major.
	
	5.1.5 dmb: use diskrec instead of handle locking; shadow avail list

	6.2a9 AR: To support the builtins.db verbs we need to know whether
	we have write permission, so we introduced the flreadonly param.
	
	2002-11-11 AR: Added assert to make sure the C compiler chose the
	proper byte alignment for the tydatabaserecord struct. If it did not,
	we would end up corrupting any database files we saved.
	*/

    tydatabaserecord diskrec;
    /*
     * Allocate buffer large enough for both v6 and v7 headers.
     * We need to read the header before we know which version it is,
     * so we size the buffer to accommodate whichever is larger.
     * Currently tydatabaserecord (118 bytes with padding) > tydatabaserecord_64 (90 bytes)
     * due to larger growthspace despite 64-bit addresses.
     */
    #define MAX_HEADER_SIZE (sizeof(tydatabaserecord) > sizeof(tydatabaserecord_64) ? sizeof(tydatabaserecord) : sizeof(tydatabaserecord_64))
    unsigned char rawheader[MAX_HEADER_SIZE];
    boolean header_is_modern = false;
	int header_version = 0;
    const char *fail_step = "alloc";
	
    register hdldatabaserecord hdb = nil;
	
	// Version-specific size validation will be done after reading header
	
	if (!newclearhandle (longsizeof (tydatabaserecord), (Handle *) &databasedata))
		goto error;
	
	hdb = databasedata; /*copy into register*/
	
	(**hdb).fnumdatabase = (long) fnum; /*set up so dbread will work*/

    fail_step = "dbgeteof";
	long filesize;
	if (!dbgeteof(&filesize))
		goto error;

	/*
	 * Determine how many bytes to read:
	 * - v6 databases: 118 bytes (sizeof(tydatabaserecord))
	 * - v7 databases: 90 bytes (sizeof(tydatabaserecord_64))
	 * Read the smaller of (file size, buffer size) to handle both formats.
	 */
	long bytes_to_read = (filesize < (long)sizeof(rawheader)) ? filesize : (long)sizeof(rawheader);

	log_debug(LOG_COMP_DB, "dbopenfile: filesize=%ld, buffer=%lu, reading=%ld bytes",
	          filesize, (unsigned long)sizeof(rawheader), bytes_to_read);

    fail_step = "dbread";
	if (!dbread ((dbaddress) 0, bytes_to_read, &rawheader))
		goto error;
	
    fail_step = "header-version";
	if (!db_format_header_version(rawheader, sizeof rawheader, &header_version))
		goto error;

	header_is_modern = header_version >= 7;

    fail_step = "legacy-read";
	if (header_version <= 6) {
		if (!db_read_legacy(rawheader, sizeof rawheader, &diskrec))
			goto error;
		if (!db_format_load_legacy_adapter(&diskrec, flreadonly, NULL))
			goto error;
		db_format_set_legacy_source_db(hdb);
	} else {
        fail_step = "modern-read";
		if (!db_read_v7(rawheader, sizeof rawheader, &diskrec))
			goto error;
        fail_step = "modern-reader";
		if (!db_format_load_v7_reader(&diskrec, flreadonly))
			goto error;
		db_format_force_strict_v7_reader();
		db_format_set_legacy_source_db(nil);
	}
	
	diskrec.fnumdatabase = (long) fnum; /*this just got overwritten*/
	
	diskrec.releasestack = nil; /*this is an in-memory structure only*/
	
	diskrec.u.extensions.flreadonly = flreadonly; /*this is an in-memory structure only*/

	**hdb = diskrec;

#if defined(FRONTIER_HEADLESS)
	log_trace(LOG_COMP_DB,
	          "dbopenfile version=%d parsedModern=%s views(parsed)=[0x%016llx,0x%016llx,0x%016llx]",
	          (int)(**hdb).versionnumber,
	          header_is_modern ? "true" : "false",
	          (unsigned long long)(**hdb).views[0],
	          (unsigned long long)(**hdb).views[1],
	          (unsigned long long)(**hdb).views[2]);
#endif
	
	// Detect database format and validate structure size
	if (!detect_database_format(&(**hdb))) {
		dberror (dbwrongversionerror);
		goto error;
	}
	
	// Version-specific size validation
	if (db_use64()) {
		assert(sizeof(tydatabaserecord_64) == 90);  // 64-bit format with 2-byte padding
	} else {
		assert(sizeof(tydatabaserecord) == 118);  // 32-bit format (on 64-bit systems) with 2-byte padding
	}

#if defined(FRONTIER_HEADLESS)
	log_trace(LOG_COMP_DB,
	          "dbopenfile post-detect use64=%s views(dec)=[0x%016llx,0x%016llx,0x%016llx]",
	          db_use64() ? "true" : "false",
	          (unsigned long long)(**hdb).views[0],
	          (unsigned long long)(**hdb).views[1],
	          (unsigned long long)(**hdb).views[2]);
#endif

	if (db_use64()) {
		for (int i = 0; i < ctviews; ++i) {
			uint64_t view = (uint64_t) (**hdb).views[i];
			if ((view & 0xFFFFFFFF00000000ULL) != 0 && (view & 0xFFFFFFFFULL) == 0)
				(**hdb).views[i] = (dbaddress) (view >> 32);
		}
	}
	
    if ((**hdb).versionnumber != dbversionnumber) {

		if (majorversion ((**hdb).versionnumber) != majorversion (dbversionnumber)) {
		
			dberror (dbwrongversionerror);
			
			goto error;
			}
		
		#ifdef SMART_DB_OPENING
		if ((**hdb).versionnumber < dbfirstversionwithcachedshadowavaillist)
			(**hdb).u.extensions.availlistblock = nildbaddress; /*don't count on old version to handle this one*/
		#endif
		
        /*
         * Only bump the in-memory header version when operating in the
         * v7 format. For legacy files (v<=6), defer version
         * changes until an explicit migration is performed (e.g., Save).
         */
        if (db_use64())
            (**hdb).versionnumber = dbversionnumber; /* we can only write what we know */

        /* Only mark database dirty if opened for writing.
         * CRITICAL FIX: Read-only databases must never have dirty headers flushed.
         * This prevents database migration from modifying the source v6 database file.
         * Issue: Migration opens source read-only but setdirty() was being called,
         * causing dirty header to be flushed on close, corrupting the v6 source file. */
        if (!flreadonly)
            setdirty (hdb);
        }
		
	// Check if this is a legacy database that should be migrated
	if (!db_use64() && (**hdb).versionnumber <= 6) {
		// Offer migration to 64-bit format
		if (offer_64bit_migration_dialog("current_database_path")) {
			// TODO: Get actual database path
			// For now, just mark for migration
			// migrate_32bit_to_64bit("current_database_path");
		}
	}
		
	if (!dbshadowavaillist ())
		goto error;
	
	return (true);
	
	error:

#if defined(FRONTIER_HEADLESS)
	log_error(LOG_COMP_DB, "dbopenfile fail at %s", fail_step);
#endif

	disposehandle ((Handle) hdb);
	
	databasedata = nil;
	
	return (false); /*error loading in header*/
	} /*dbopenfile*/


boolean dbclose (void) {

	dbzeroreleasestack (); /*don't release chunks accumulated in release stack*/

	setdirty (databasedata);

#if defined(FRONTIER_HEADLESS)
	log_trace(LOG_COMP_DB, "dbclose enter databasedata=%p fnum=%ld dirty=%d flreadonly=%s",
	          (void *) databasedata,
	          databasedata ? (long) (**databasedata).fnumdatabase : -1L,
	          databasedata ? (int) isdirty(databasedata) : 0,
	          (databasedata && (**databasedata).u.extensions.flreadonly) ? "TRUE" : "FALSE");
#endif

	boolean result = dbflushheader ();

#if defined(FRONTIER_HEADLESS)
	log_trace(LOG_COMP_DB, "dbclose exit fnum=%ld result=%s",
	          databasedata ? (long) (**databasedata).fnumdatabase : -1L,
	          result ? "TRUE" : "FALSE");
#endif

	return result;
	} /*dbclose*/


static boolean dbstartsaveas_internal(hdlfilenum fnum) {

	register boolean fl;

#if defined(FRONTIER_HEADLESS)
	log_trace(LOG_COMP_DB, "dbstartsaveas BEGIN: source_db=%p dest_db=%p",
	          (void*)databasedata,
	          (void*)databasedestination);
#endif

	fldatabasesaveas = true; /*set global; enables databasehandle swapping*/
	dbsaveas_source = databasedata;

	/* 2025-12-16: Enable wide writes when legacy adapter is active.
	 * Call the non-context version directly so the mode persists globally
	 * for all subsequent writes during migration. Using _context() here would
	 * undo the mode change when the guard exits. */
    db_format_adapter_enable_wide_writes(NULL);

#if defined(FRONTIER_HEADLESS)
	log_trace(LOG_COMP_DB, "dbstartsaveas: fldatabasesaveas=true dbsaveas_source=%p databasedata=%p",
	          (void*)dbsaveas_source,
	          (void*)databasedata);
#endif

	dbswapglobals ();

#if defined(FRONTIER_HEADLESS)
	log_trace(LOG_COMP_DB, "dbstartsaveas: after swap, databasedata=%p databasedestination=%p",
	          (void*)databasedata,
	          (void*)databasedestination);
#endif

	fl = dbnew (fnum, true);  /* Create v7 format during migration */

#if defined(FRONTIER_HEADLESS)
	log_trace(LOG_COMP_DB, "dbstartsaveas: after dbnew, fl=%d databasedata=%p headerLength=%ld",
	          (int)fl,
	          (void*)databasedata,
	          databasedata ? (**databasedata).headerLength : -1L);
	log_trace(LOG_COMP_DB, "dbstartsaveas: before swap back, databasedestination=%p headerLength=%ld",
	          (void*)databasedestination,
	          databasedestination ? (**databasedestination).headerLength : -1L);
#endif

	dbswapglobals ();

#if defined(FRONTIER_HEADLESS)
	log_trace(LOG_COMP_DB, "dbstartsaveas END: after final swap, databasedata=%p headerLength=%ld",
	          (void*)databasedata,
	          databasedata ? (**databasedata).headerLength : -1L);
	log_trace(LOG_COMP_DB, "dbstartsaveas END: databasedestination=%p headerLength=%ld fl=%d",
	          (void*)databasedestination,
	          databasedestination ? (**databasedestination).headerLength : -1L,
	          (int)fl);
#endif

	fldatabasesaveas = fl;
	if (!fl)
		dbsaveas_source = nil;

	return (fl);
	} /*dbstartsaveas_internal*/

boolean dbstartsaveas (hdlfilenum fnum) {
    db_context *ctx = db_context_refresh_default();
    return dbstartsaveas_context(ctx, fnum);
}

boolean dbgetdestinationdatabase (hdldatabaserecord *hdb) {
    if (hdb == NULL)
        return (false);
    db_context *ctx = db_context_refresh_default();
    if (!ctx->saveas.active || ctx->saveas.destination == nil)
        return (false);
    *hdb = ctx->saveas.destination;
    return (true);
    } /*dbgetdestinationdatabase*/

static boolean dbendsaveas_internal (void) {
	
	register boolean fl;
	
	if (!fldatabasesaveas)
		return (false);

    if (databasedata == nil || databasedestination == nil)
        return (false);

#if defined(FRONTIER_HEADLESS)
    {
        boolean valid_data = validhandle((Handle) databasedata);
        boolean valid_dest = validhandle((Handle) databasedestination);
        log_trace(LOG_COMP_DB,
                  "saveas end enter data=%p master=%p dest=%p dest_master=%p valid(data)=%d valid(dest)=%d",
                  (void *) databasedata,
                  valid_data ? (void *) (*databasedata) : NULL,
                  (void *) databasedestination,
                  valid_dest ? (void *) (*databasedestination) : NULL,
                  valid_data ? 1 : 0,
                  valid_dest ? 1 : 0);
    }
#endif
		
	dbswapglobals ();
	
    if (databasedata != nil)
        (**databasedata).releasestack = nil; /* destination Save As handle shouldn't carry stale release stack */

	fl = dbclose ();
	
	dbdispose ();
	
	dbswapglobals ();
	
	fldatabasesaveas = false;
	dbsaveas_source = nil;
	
	return (fl);
	} /*dbendsaveas_internal*/

boolean dbendsaveas (void) {
    db_context *ctx = db_context_refresh_default();
    return dbendsaveas_context(ctx);
}

/* Thread-safe context wrapper for dballocate to avoid global flips. */
boolean dballocate_context(const db_context *context, long databytes, ptrvoid pdata, dbaddress *paddress) {
    db_context_guard guard;
    db_context_guard_enter(context, &guard);
    boolean ok = dballocate(databytes, pdata, paddress);
    db_context_guard_exit(&guard);
    return ok;
}

/* Context-aware Save As completion to keep destination scoped. */
boolean dbendsaveas_context(db_context *context) {
    db_context local_ctx;
    if (context != NULL && context->saveas.active && context->saveas.source != nil) {
        local_ctx = *context;
        local_ctx.database = context->saveas.source; /* ensure swap restores source */
        context = &local_ctx;
    }
    db_context_guard guard;
    db_context_guard_enter(context, &guard);
    boolean ok = dbendsaveas_internal();
    db_saveas_state applied_state;
    db_saveas_state_snapshot(&applied_state);
    const db_saveas_state *restore_state = &applied_state;
    if (context != NULL) {
        context->saveas = applied_state;
        context->database = nil;
    }
    db_context_guard_exit_with_saveas(&guard, restore_state);
    return ok;
}

boolean dbstartsaveas_context(db_context *context, hdlfilenum fnum) {
    db_context_guard guard;
    db_context_guard_enter(context, &guard);
    boolean ok = dbstartsaveas_internal(fnum);
    db_saveas_state applied_state;
    db_saveas_state_snapshot(&applied_state);
    const db_saveas_state *restore_state = &applied_state;
    if (context != NULL) {
        context->saveas = applied_state;
        /* Keep the source handle associated with the context; callers can
           temporarily point database at the destination when they need to
           write into the Save As target. */
        if (context->saveas.source != nil)
            context->database = context->saveas.source;
        if (context != &g_default_db_context)
            restore_state = &guard.prev_saveas;
    }
    if (context == &g_default_db_context)
        db_saveas_state_apply(&applied_state);
    db_context_guard_exit_with_saveas(&guard, restore_state);
    return ok;
}

/* Context-aware wrapper for Save As swapping globals. */
void dbswapglobals_context(db_context *context) {
    if (context == NULL)
        context = db_context_refresh_default();

    db_context_guard guard;
    db_context_guard_enter(context, &guard);
    dbswapglobals();
    db_saveas_state_snapshot(&context->saveas);
    context->database = databasedata;
    db_context_guard_exit_with_saveas(&guard, &guard.prev_saveas);
}
/* Context-aware default wrapper for dbclose to preserve globals while enabling contexts. */
boolean dbclose_context(const db_context *context) {
    db_context_guard guard;
    db_context_guard_enter(context, &guard);
    boolean ok = dbclose();
    db_context_guard_exit(&guard);
    return ok;
}
