
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
#include "file.h"
#include "resources.h"
#include "strings.h"
#include "shell.h"
#include "db.h"
#include "db_format.h"
#include "db_reader.h"
#include "db_writer_modern.h"
#include "dbinternal.h"
#include "ops.h" //6.2b3 AR: for numbertostring
#include "byteorder.h"	/* 2006-04-08 aradke: endianness conversion macros */

#include "frontierdebug.h" //6.2b7 AR

#define dberrorlist 256

// 2025-11-23 Codex: Widened block header/trailer to 64-bit BE and updated avail links for v7 roots.
// 2025-11-20 Codex: Write modern headers and record metadata with explicit big-endian encoding for portability.
// 2025-11-16 Codex: Keep dbgetsize locals wide enough so dbgetsizeandvariance
// writes don't corrupt the caller's stack on 64-bit builds.

static void db_sync_use64_to_current_db(void);

#define setdirty(hdb) 		((**hdb).flags |= dbdirtymask)
#define cleardirty(hdb)		((**hdb).flags &= ~dbdirtymask)
#define isdirty(hdb) 		((**hdb).flags & dbdirtymask)

#define majorversion(v)		(v & 0x00f0)
#define minorversion(v)		(v & 0x000f)

#if defined(FRONTIER_HEADLESS)
static boolean dbfindblockforaddress(dbaddress adr, dbaddress *blockstart, long *nodebytes, tyvariance *variance, boolean *flfree) {
	long eof = 0;
    const long header_size = (databasedata != nil && db_format_is_legacy_db(databasedata)) ? sizeheader_v6 : sizeheader;

	if (adr == nildbaddress)
		return false;

	if (!dbgeteof(&eof))
		return false;

	if (adr < firstphysicaladdress || adr >= (dbaddress) eof)
		return false;

	for (dbaddress candidate = adr; candidate >= firstphysicaladdress && (adr - candidate) <= 0x100000; --candidate) {
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
			fprintf(stderr,
				"[headless] dbfindblockforaddress candidate=0x%llx size=%ld variance=%ld eof=%ld trailer=0x%llx\n",
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


#define ctdatabasestack 10

short topdatabasestack = 0;

hdldatabaserecord databasestack [ctdatabasestack];
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
        static int call_count = 0;
        if (call_count++ < 5) {
            fprintf(stderr, "[headless] db_context_guard_enter: prev_mode captured use_64bit=%d adapter_repack=%d\n",
                    (int) guard->prev_mode.use_64bit_format, (int) guard->prev_mode.adapter_repack);
        }
#endif
    }
    if (context != NULL) {
        if (context->database != nil)
            databasedata = context->database;
        db_saveas_state_apply(&context->saveas);
#if defined(FRONTIER_HEADLESS)
        static int call_count2 = 0;
        if (call_count2++ < 5) {
            fprintf(stderr, "[headless] db_context_guard_enter: applying mode use_64bit=%d adapter_repack=%d\n",
                    (int) context->mode.use_64bit_format, (int) context->mode.adapter_repack);
        }
#endif
        db_format_mode_apply(&context->mode);
#if defined(FRONTIER_HEADLESS)
        if (call_count2 <= 5) {
            db_format_mode current_after = db_format_mode_current();
            fprintf(stderr, "[headless] db_context_guard_enter: after apply, current mode use_64bit=%d adapter_repack=%d\n",
                    (int) current_after.use_64bit_format, (int) current_after.adapter_repack);
        }
#endif
    }
}

static void db_context_guard_exit(const db_context_guard *guard) {
    if (guard == NULL)
        return;
#if defined(FRONTIER_HEADLESS)
    static int call_count = 0;
    if (call_count++ < 5) {
        fprintf(stderr, "[headless] db_context_guard_exit: restoring prev mode use_64bit=%d adapter_repack=%d\n",
                (int) guard->prev_mode.use_64bit_format, (int) guard->prev_mode.adapter_repack);
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
boolean dbpushdatabase_context(const db_context *context, hdldatabaserecord hdatabase);
boolean dbpopdatabase_context(const db_context *context);
boolean dbrelease_context(const db_context *context, dbaddress adr);
boolean dbassign_internal(dbaddress *padr, long newsize, ptrvoid pdata);
boolean dbcopy_internal(dbaddress adrorig, dbaddress *adrcopy);
boolean dbreference_internal(dbaddress adr, long maxbytes, ptrvoid pdata);
boolean dbgetsize_internal(dbaddress adr, long *logicalsize);

boolean dbpushdatabase (hdldatabaserecord hdatabase) {
	/*
	when you want to temporarily work with a different databaserecord, call this
	routine, do your stuff and then call dbpopdatabase.
	*/
	
	if (topdatabasestack >= ctdatabasestack) {
		
		DebugStr (STR_database_stack_overflow);
		
		return (false);
		}
	
	databasestack [topdatabasestack++] = databasedata;
	
	if (hdatabase != nil)
		databasedata = hdatabase;

	db_sync_use64_to_current_db();
	
	return (true);
	} /*dbpushdatabase*/
		

boolean dbpopdatabase (void) {
	
	if (topdatabasestack <= 0)
		return (false);
	
	databasedata = databasestack [--topdatabasestack];
	db_sync_use64_to_current_db();
	
	return (true);
	} /*dbpopdatabase*/

/* Context-aware push/pop to avoid leaking mode/handle changes. */
boolean dbpushdatabase_context(const db_context *context, hdldatabaserecord hdatabase) {
    db_context_guard guard;
    db_context_guard_enter(context, &guard);
    boolean ok = dbpushdatabase(hdatabase);
    db_context_guard_exit(&guard);
    return ok;
}

boolean dbpopdatabase_context(const db_context *context) {
    db_context_guard guard;
    db_context_guard_enter(context, &guard);
    boolean ok = dbpopdatabase();
    db_context_guard_exit(&guard);
    return ok;
}


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
        static int call_count = 0;
        if (call_count++ < 5) {
            fprintf(stderr, "[headless] db_context_for_saveas_destination: adapter_active=%d is_legacy=%d\n",
                    (int) adapter_active, (int) is_legacy);
        }
#endif
        if (adapter_active) {
            ctx->mode.use_64bit_format = true;
        } else {
            ctx->mode.use_64bit_format = !is_legacy;
        }
#if defined(FRONTIER_HEADLESS)
        if (call_count <= 5) {
            fprintf(stderr, "[headless] db_context_for_saveas_destination: set use_64bit_format=%d\n",
                    (int) ctx->mode.use_64bit_format);
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
		/* Legacy source stays 32-bit; destination writes are modern BE64. */
        db_format_mode mode = db_format_mode_current();
        mode.use_64bit_format = !db_format_is_legacy_db(databasedata);
        db_format_mode_apply(&mode);
	}
}

static inline hdldatabaserecord db_begin_source_read(void) {
	if (fldatabasesaveas && dbsaveas_source != nil) {
		hdldatabaserecord previous = databasedata;
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
	
	return (filesetposition((hdlfilenum)((**databasedata).fnumdatabase), adr));
	} /*dbseek*/
		
	
boolean dbwrite (dbaddress adr, long ctbytes, ptrvoid pdata) {
	
	if (!dbseek (adr)) {
#if defined(FRONTIER_HEADLESS)
		fprintf(stderr, "[headless] dbwrite seek failed fnum=%ld adr=0x%llx bytes=%ld\n",
		        databasedata ? (long) (**databasedata).fnumdatabase : -1L,
		        (unsigned long long) adr,
		        ctbytes);
#endif
		return (false);
	}
		
	if (!filewrite ((hdlfilenum)((**databasedata).fnumdatabase), ctbytes, pdata)) {
#if defined(FRONTIER_HEADLESS)
		fprintf(stderr, "[headless] dbwrite filewrite failed fnum=%ld adr=0x%llx bytes=%ld\n",
		        databasedata ? (long) (**databasedata).fnumdatabase : -1L,
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
		fprintf(stderr, "[headless] dbread seek failed fnum=%ld adr=0x%llx bytes=%ld saveas=%d source=%p current=%p dest=%p\n",
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
		fprintf(stderr, "[headless] dbread read failed fnum=%ld adr=0x%llx bytes=%ld saveas=%d source=%p current=%p dest=%p\n",
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


static boolean dbflushheader (void) {
	
	/*
	5.1.5 dmb: copy databasedata to local record for writing
	*/
	
	register hdldatabaserecord hdb = databasedata;
	boolean fl;
    tydatabaserecord diskrec;
	boolean use64log = db_use64();
	
	assert (sizeof (diskrec.u.growthspace) >= sizeof (diskrec.u.extensions));

#if defined(FRONTIER_HEADLESS)
	fprintf(stderr, "[headless] dbflushheader enter databasedata=%p fnum=%ld dirty=%d use64=%d\n",
	        (void *) hdb,
	        hdb ? (long) (**hdb).fnumdatabase : -1L,
	        hdb ? (int) isdirty(hdb) : 0,
	        (int) use64log);
#endif
	
	/* If we opened via legacy adapter, flip to wide writes before flushing. */
    {
        db_context ctx;
        db_context_init(&ctx);
        db_format_adapter_enable_wide_writes_context(&ctx, NULL);
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

			if (!db_write_modern_header(&diskrec, diskheader, sizeof (diskheader))) {
#if defined(FRONTIER_HEADLESS)
				fprintf(stderr, "[headless] dbflushheader db_write_modern_header failed\n");
#endif
				return (false);
			}

			fl = dbwrite ((dbaddress) 0, (long) sizeof (diskheader), diskheader);

#if defined(FRONTIER_HEADLESS)
			if (!fl) {
				fprintf(stderr, "[headless] dbflushheader dbwrite failed fnum=%ld len=%zu\n",
				        hdb ? (long) (**hdb).fnumdatabase : -1L,
				        sizeof (diskheader));
			}
#endif
		}
		
		#ifndef FRONTIER_HEADLESS
		/*flush file buffers*/ {
			IOParam pb;
			
			clearbytes (&pb, sizeof (pb));
			
			pb.ioRefNum = (hdlfilenum)((**databasedata).fnumdatabase);
			
			PBFlushFile ((ParmBlkPtr) &pb, false);
			}
		#endif

		return (fl);
		} /*changes made to header*/
		
	return (true);
	} /*dbflushheader*/
	

boolean dbreadheader (dbaddress adr, boolean *flfree, long *ctbytes, tyvariance *variance) {

	uint64_t raw_size = 0;
	tyvariance disk_variance = 0;
	boolean use64 = db_use64();

	if (databasedata != nil && db_format_is_legacy_db(databasedata))
		use64 = false;

	if (use64) {
		tyheader64 header;

		if (!dbread (adr, sizeheader_v7, &header))
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

		if (!dbread (adr, sizeheader_v6, &header))
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
		fprintf(stderr, "[headless] dbreadheader parsed raw=0x%016llx variance=0x%08x\n",
		        raw_dbg,
		        (unsigned int) disk_variance);
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
	} /*dbreadheader*/
	

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

	return db_write_modern_block_header(adr, flfree, ctbytes, variance);
	} /*dbwriteheader*/
	
	
static boolean dbwritetrailer (dbaddress adr, boolean flfree, long ctbytes) {

	return db_write_modern_block_trailer(adr, flfree, ctbytes);
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

boolean dbreference (dbaddress adr, long maxbytes, ptrvoid pdata) {
    db_context *ctx = db_context_refresh_default();
    return dbreference_context(ctx, adr, maxbytes, pdata);
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
    (void) dbnormalizeaddress(&a);
#endif

    if (!dbreadheader (a, &flfree, &ctbytes, &variance))
        return (false);

    ct = ctbytes - (long) variance;

#if defined(FRONTIER_HEADLESS)
    if (a == 0x76e) {
        fprintf(stderr, "[headless] dbrefhandle watch adr=0x%llx size=%ld variance=%ld flfree=%d\n",
            (unsigned long long)a, ctbytes, (long)variance, flfree ? 1 : 0);
    }
#endif

    if (flfree || (ct < 0)) { /*probably a bad address*/

        dberror (dbfreeblockerror);
#if defined(FRONTIER_HEADLESS)
        fprintf(stderr, "[headless] dbrefhandle found free block adr=0x%llx size=%ld variance=%ld\n",
                (unsigned long long)a, ctbytes, (long) variance);
#endif
        return (false);
        }
		
	if (!newclearhandle (ct, h))
		return (false);

#if defined(FRONTIER_HEADLESS)
    if (a == 0x76e) {
        fprintf(stderr, "[headless] dbrefhandle watch allocated handle size=%ld\n", ct);
    }
#endif
	
	hregister = *h;
	
	lockhandle (hregister);
	
	fl = dbread (a + sizeheader, ct, *hregister);

#if defined(FRONTIER_HEADLESS)
    if (a == 0x76e) {
        fprintf(stderr, "[headless] dbrefhandle watch dbread result=%d\n", fl ? 1 : 0);
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
    db_context_guard_enter(apply_ctx, &guard);

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

	db_context_guard_exit(&guard);
	
	return (true); /*the allocation was successful*/
	
	
failure:

#if defined(FRONTIER_HEADLESS)
    fprintf(stderr,
            "[headless] dballocate failure step=%s size=%ld saveas=%d current=%p dest=%p using_dest=%d\n",
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
            fprintf(stderr,
                    "[headless] dballocate failed size=%ld saveas=%d dest=%p\n",
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
    db_context *ctx = db_context_refresh_default();
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
		fprintf(stderr, "[headless-db] dbgetsize failed for adr=0x%llx\n", (unsigned long long) adrorig);
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
		fprintf(stderr, "[headless-db] dbreference failed for adr=0x%llx size=%ld\n",
		        (unsigned long long) adrorig, size);
#endif
	}
	db_end_source_read(source_db);

	if (flreturned)
		flreturned = dballocate (size, *h, adrcopy);
	else {
#if defined(FRONTIER_HEADLESS)
		fprintf(stderr, "[headless-db] dbcopy aborted before allocation adr=0x%llx size=%ld\n",
		        (unsigned long long) adrorig, size);
#endif
		flreturned = false;
	}
	
	unlockhandle (h);
	
	disposehandle (h);

	if (!flreturned) {
#if defined(FRONTIER_HEADLESS)
		fprintf(stderr, "[headless-db] dballocate failed during dbcopy size=%ld\n", size);
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
        fprintf(stderr,
                "[headless] dbassignhandle failed adr_in=0x%llx size=%ld saveas=%d dest=%p\n",
                (unsigned long long) original,
                hsize,
                (int) fldatabasesaveas,
                (void *) databasedestination);
    }
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
	
	ctbytes = gethandlesize (h);
	
	lockhandle (h);
	
	if (a == nildbaddress) 
		fl = dballocate (ctbytes, *h, &a);
	else
		fl = dbassign (&a, ctbytes, *h);
		
	unlockhandle (h);

	*adr = a; /*copy into returned value*/

 	if (!fl) {
#if defined(FRONTIER_HEADLESS)
		fprintf(stderr, "[headless] dbsavehandle failed: adr=%lld bytes=%ld\n", (long long)a, ctbytes);
#endif
	}

	return (fl);
	} /*dbsavehandle*/
	

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
	
	(**hdb).views [viewnumber] = adrtext;
	
	setdirty (hdb);
	
	dbflushheader ();
	
	db_context_guard_exit(&guard);
	} /*dbsetview*/


void dbgetview (short viewnumber, dbaddress *adrtext) {
	
	*adrtext = (**databasedata).views [viewnumber];
	} /*dbgetview*/


void dbcurrentdatabase (hdldatabaserecord hdb) {
	
	if (hdb != nil)
		databasedata = hdb; 
	} /*dbcurrentdatabase*/
	

void dbgetcurrentdatabase (hdldatabaserecord *hdb) {
	
	*hdb = databasedata;
	} /*dbgetcurrentdatabase*/
	
	
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

	/* Defensive: guard against stale or invalid handles during Save As teardown. */
	if ((uintptr_t) hstack < 0x1000) {
		(**databasedata).releasestack = nil;
		return;
	}

	disposehandle (hstack);
	
	(**databasedata).releasestack = nil;
	} /*dbzeroreleasestack_impl*/

static void dbzeroreleasestack (void) {
    db_context_guard guard;
    db_context_guard_enter(db_context_refresh_default(), &guard);
    dbzeroreleasestack_impl();
    db_context_guard_exit(&guard);
}

boolean dbzeroreleasestack_context(const db_context *context) {
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
	
	return (true);
	} /*dbdispose*/


boolean dbnew (hdlfilenum fnum) {
	
	/*
	2002-11-11 AR: Added assert to make sure the C compiler chose the
	proper byte alignment for the tydatabaserecord struct. If it did not,
	we would end up corrupting any database files we saved.
	*/
	
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


	(**hdb).versionnumber = dbversionnumber;

	(**hdb).headerLength = firstphysicaladdress;
	(**hdb).longversionMajor = dbversionnumber;
	(**hdb).longversionMinor = dbversionnumberminor;
	
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
	
    fail_step = "dbread";
	if (!dbread ((dbaddress) 0, sizeof (rawheader), &rawheader))
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
		if (!db_read_modern(rawheader, sizeof rawheader, &diskrec))
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
	fprintf(stderr,
		"[headless] dbopenfile version=%d parsedModern=%s views(parsed)=[0x%016llx,0x%016llx,0x%016llx]\n",
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
	fprintf(stderr,
		"[headless] dbopenfile post-detect use64=%s views(dec)=[0x%016llx,0x%016llx,0x%016llx]\n",
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
         * modern (v7) format. For legacy files (v<=6), defer version
         * changes until an explicit migration is performed (e.g., Save).
         */
        if (db_use64())
            (**hdb).versionnumber = dbversionnumber; /* we can only write what we know */
        
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
	fprintf(stderr, "[headless] dbopenfile fail at %s\n", fail_step);
#endif

	disposehandle ((Handle) hdb);
	
	databasedata = nil;
	
	return (false); /*error loading in header*/
	} /*dbopenfile*/


boolean dbclose (void) {
	
	dbzeroreleasestack (); /*don't release chunks accumulated in release stack*/
	
	setdirty (databasedata);

#if defined(FRONTIER_HEADLESS)
	fprintf(stderr, "[headless] dbclose enter databasedata=%p fnum=%ld dirty=%d\n",
	        (void *) databasedata,
	        databasedata ? (long) (**databasedata).fnumdatabase : -1L,
	        databasedata ? (int) isdirty(databasedata) : 0);
#endif
	
	return (dbflushheader ());
	} /*dbclose*/


static boolean dbstartsaveas_internal(hdlfilenum fnum) {
	
	register boolean fl;
		
	fldatabasesaveas = true; /*set global; enables databasehandle swapping*/
	dbsaveas_source = databasedata;

	/* 2025-11-25 Codex: Enable wide writes when legacy adapter is active. */
    {
        db_context ctx;
        db_context_init(&ctx);
        db_format_adapter_enable_wide_writes_context(&ctx, NULL);
    }
	
	dbswapglobals ();
	
	fl = dbnew (fnum);
	
	dbswapglobals ();
	
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
        fprintf(stderr,
                "[headless] saveas end enter data=%p master=%p dest=%p dest_master=%p valid(data)=%d valid(dest)=%d\n",
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
