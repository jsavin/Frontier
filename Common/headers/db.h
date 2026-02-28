
/*	$Id$    */

/*****************************************************************************/
/* 2025-11-29 Codex: Add Save As destination getter for context-aware callers. */
/* 2025-11-30 Codex: Widen Save As context signatures for scoped state. */
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

 

#ifndef dbinclude /*so other includes can tell if we've been loaded*/
#define dbinclude /*so other includes can tell if we've been loaded*/


#ifdef HEADERTRACE
#pragma message( "**Compiling " __FILE__ )
#endif


typedef struct db_context db_context;

#ifndef memoryinclude
	#include "memory.h"
#endif

/* 2025-12-01 Codex: Expose tyvariance for public helpers. */
#ifndef TYVARIANCE_DEFINED
#define TYVARIANCE_DEFINED 1
typedef int32_t tyvariance;
#endif

#define nildbaddress 0L


#define ctviews 3 /*leave room for 3 different views in a database header*/
#define cancoonview 0

typedef long long dbaddress, *ptrdbaddress, **hdldbaddress;

#if defined(__clang__) || defined(__GNUC__)
#pragma pack(push, 2)
#else
#pragma pack(2)
#endif
typedef struct tydatabaserecord { /*stored at offset 0 in the db file*/
	

	unsigned char systemid;		/* 0 = MAC (compatiblity)  */


	unsigned char versionnumber; /*which version created this file?*/
	
	dbaddress availlist; /*avail list is singly-linked, nil terminated*/
	
	short oldfnumdatabase; /*only applies when database record is in memory*/

	short flags; /*any changes to header since it was last flushed?*/

	unsigned char _pad[2]; /* Explicit padding to align views to 8-byte boundary (offset 16) */

	dbaddress views [ctviews]; /*addresses of the root of each view*/
	
	Handle releasestack; /*holds addresses of nodes waiting to be released*/
	

						/* RAB 9/2/96 - fnumdatabase is delibritly declared

						as a long not a hdlfilenum. This is to allow for a consistent header

						size between Mac and Win platforms even though hdlfilenum is 

						system dependent.

						It has been moved to this position to allow view and fldirty

						to remain the same as version 3 files.

						*/

	long fnumdatabase; /* file handle while in memory - New to version 5*/

	long headerLength;		/*size of header - New to version 5*/

	short longversionMajor;	/*new extended version id - new to version 5*/

	short longversionMinor;	/*new extended version id - new to version 5*/
	
	union {
		char growthspace [50]; /*room for new fields without format change*/
		
		struct {
			dbaddress availlistblock; /*6.2a9 AR: on-disk structure mirroring availlist, a contiguous block*/
			
			handlestream availlistshadow; /*never saved to disk; in-memory structure mirroring availlist*/

			boolean flreadonly; /*6.2a9 AR: never saved to disk; if this is true, don't write to the file*/
			} extensions;
		} u;
	} tydatabaserecord, *ptrdatabaserecord, **hdldatabaserecord;

// 64-bit database record structure (Version 7)
typedef struct tydatabaserecord_64 { /*stored at offset 0 in the db file*/
	unsigned char systemid;		/* 0 = MAC (compatiblity)  */
	unsigned char versionnumber; /*which version created this file?*/
	dbaddress availlist; /*avail list is singly-linked, nil terminated*/
	short oldfnumdatabase; /*only applies when database record is in memory*/
	short flags; /*any changes to header since it was last flushed?*/
	unsigned char _pad[2]; /* Explicit padding to align views to 8-byte boundary (offset 16) */
	dbaddress views [ctviews]; /*addresses of the root of each view*/
	Handle releasestack; /*holds addresses of nodes waiting to be released*/
	long fnumdatabase; /* file handle while in memory - New to version 5*/
	long headerLength;		/*size of header - New to version 5*/
	short longversionMajor;	/*new extended version id - new to version 5*/
	short longversionMinor;	/*new extended version id - new to version 5*/
	union {
		char growthspace [22]; /*room for new fields without format change*/
		struct {
			dbaddress availlistblock; /*6.2a9 AR: on-disk structure mirroring availlist, a contiguous block*/
			dbaddress availlistshadow; /*in-memory handle pointer, persisted as 64-bit value for completeness*/
			boolean flreadonly; /*6.2a9 AR: never saved to disk; if this is true, don't write to the file*/
			unsigned char reserved[5]; /*pad to maintain 22-byte extension payload*/
			} extensions;
		} u;
	} tydatabaserecord_64, *ptrdatabaserecord_64, **hdldatabaserecord_64;
#if defined(__clang__) || defined(__GNUC__)
#pragma pack(pop)
#else
#pragma options align=reset
#endif
	
extern hdldatabaserecord databasedata; /*can be set by external user*/

extern boolean fldatabasesaveas;

#ifdef FRONTIER_TESTS
/* Test-only accessor for cleanup state validation (see db.c) */
extern boolean db_test_is_saveas_active(void);
#endif

#if defined(FRONTIER_HEADLESS)
extern boolean dbnormalizeaddress(dbaddress *adr);
extern boolean dbnormalizeaddress_hdb(dbaddress *adr, hdldatabaserecord hdb);
#endif

/* Phase 8: Push to release stack of explicit database handle. */
extern boolean dbpushreleasestack_hdb(dbaddress adr, long valtype, hdldatabaserecord hdb);


#ifdef DATABASE_DEBUG

typedef struct {
	dbaddress	adr;
	long		id;
	long		line;
	Handle		file;
	} tydbreleasestackframe;

#define dbpushreleasestack(adr, valtype) \
	debug_dbpushreleasestack(adr, valtype, __LINE__, __FILE__);

extern boolean debug_dbpushreleasestack (dbaddress adr, long valtype, long line, char *sourcefile);

#else

extern boolean dbpushreleasestack (dbaddress, long);

#endif

/*prototypes*/

extern boolean dbreference (dbaddress, long, ptrvoid);

extern boolean dbrefhandle (dbaddress, Handle *);

/* Explicit header size versions for migration (v6=8 bytes, v7=12 bytes).
 * These bypass the global format mode lock during v6→v7 migration. */
extern boolean dbreference_with_header_size(dbaddress adr, long maxbytes, ptrvoid pdata, long header_size);

extern boolean dbrefhandle_with_header_size(dbaddress adr, Handle *h, long header_size);

extern boolean dbread_fnum(dbaddress adr, long ctbytes, ptrvoid pdata, hdlfilenum fnum);

/* hdb: owning database handle, used for the read-only guard.  Pass nil
   to skip the guard (only when no database handle is available). */
extern boolean dbwrite_fnum(dbaddress adr, long ctbytes, ptrvoid pdata, hdlfilenum fnum, hdldatabaserecord hdb);

extern boolean dbgeteof_fnum(long *eof, hdlfilenum fnum);

extern boolean dbreference_fnum(dbaddress adr, long maxbytes, ptrvoid pdata, long header_size, hdlfilenum fnum);

extern boolean dbrefhandle_fnum(dbaddress adr, Handle *h, long header_size, hdlfilenum fnum);

/* Phase 7, Layer 2: Header/trailer I/O and seteof with explicit file number.
   These bypass databasedata entirely. */
extern boolean dbseteof_fnum(long eof, hdlfilenum fnum);
extern boolean dbreadheader_fnum(dbaddress adr, boolean *flfree, long *ctbytes, tyvariance *variance, long header_size, hdlfilenum fnum);
extern boolean dbwriteheader_fnum(dbaddress adr, boolean flfree, long ctbytes, tyvariance variance, hdlfilenum fnum, hdldatabaserecord hdb);
extern boolean dbwritetrailer_fnum(dbaddress adr, boolean flfree, long ctbytes, hdlfilenum fnum, hdldatabaserecord hdb);

/* Phase 7, Layer 3: Allocator/release with explicit database handle.
   These do NOT use databasedata or db_context_guard — the caller passes
   the resolved database handle directly. */
extern boolean dballocate_hdb(long databytes, ptrvoid pdata, dbaddress *paddress, hdldatabaserecord hdb);
extern boolean dbrelease_hdb(dbaddress adr, hdldatabaserecord hdb);

/* Phase 7, Layer 4: High-level operations with explicit database handle. */
extern boolean dbassign_hdb(dbaddress *padr, long newsize, ptrvoid pdata, hdldatabaserecord hdb);
extern boolean dbcopy_hdb(dbaddress adrorig, dbaddress *adrcopy, hdldatabaserecord hdb);
extern boolean dbsavehandle_hdb(Handle hsave, dbaddress *adr, hdldatabaserecord hdb);
extern boolean dbrefhandle_hdb(dbaddress adr, Handle *h, hdldatabaserecord hdb);
extern boolean dbassignhandle_hdb(Handle h, dbaddress *adr, hdldatabaserecord hdb);

extern boolean dbassign (dbaddress *, long, ptrvoid);

extern boolean dbcopy (dbaddress, dbaddress *);

extern boolean dbsetsize_public (dbaddress, long, tyvariance);

extern boolean dballochandle (Handle, dbaddress *);

extern boolean dbassignhandle (Handle, dbaddress *);

extern boolean dbsavehandle (Handle, dbaddress *);

extern boolean dbrefheapstring (dbaddress, hdlstring *);

extern boolean dbassignheapstring (dbaddress *, hdlstring);

extern void dbsetview (short, dbaddress);

extern void dbgetview (short, dbaddress *);

extern boolean dbflushreleasestack (void);

extern boolean dbfnumchanged (hdlfilenum);

extern boolean dbdispose (void);

extern boolean dbnew (hdlfilenum, boolean); /* second param: use_v7_format */

extern boolean dbopenfile (hdlfilenum, boolean);

extern boolean dbclose (void);

extern boolean dbstartsaveas (hdlfilenum);
extern boolean dbstartsaveas_context(db_context *context, hdlfilenum fnum);

extern boolean dbgetdestinationdatabase (hdldatabaserecord *);

/* NOTE: dbendsaveas() and dbendsaveas_context() call dbdispose() internally on the destination database.
 * Caller MUST set databasedata = nil after these calls to prevent double-free. */
extern boolean dbendsaveas (void);
extern boolean dbendsaveas_context(db_context *context);

extern boolean statsblockinuse (dbaddress, bigstring); /*dbstats.c*/

extern boolean dbstatsmessage (hdldatabaserecord, boolean); /*6.2a8 AR*/

extern boolean statsstart (void);

/* Database global state guard for nested database operations (migration, etc.)
 * Uses opaque pointers to avoid circular header dependencies with lang.h.
 *
 * Must save/restore ALL globals that cleartablestructureglobals() clears,
 * otherwise opening a guest database via odbOpenFile will destroy the system
 * root's table structure globals (pathstable, systemtable, etc.). */
typedef struct odb_context_guard {
	void *saved_currenthashtable;  /* hdlhashtable */
	hdldatabaserecord saved_databasedata;
	void *saved_hashtablestack;    /* hdltablestack */
	void *saved_rootvariable;      /* Handle */
	void *saved_roottable;         /* hdlhashtable */
	void *saved_cancoonglobals;    /* hdlcancoonrecord */
	/* Table structure globals cleared by cleartablestructureglobals() */
	void *saved_systemtable;       /* hdlhashtable */
	void *saved_builtinstable;     /* hdlhashtable */
	void *saved_pathstable;        /* hdlhashtable */
	void *saved_verbstable;        /* hdlhashtable */
	void *saved_iacgluetable;      /* hdlhashtable */
	void *saved_iachandlertable;   /* hdlhashtable */
	void *saved_resourcestable;    /* hdlhashtable */
	void *saved_agentstable;       /* hdlhashtable */
	void *saved_menubartable;      /* hdlhashtable */
	void *saved_objectmodeltable;  /* hdlhashtable */
} odb_context_guard;

extern void odb_guard_enter(odb_context_guard *guard);
extern void odb_guard_exit(odb_context_guard *guard);

#endif
