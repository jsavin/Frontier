
/*	$Id$    */

/*
    SPDX-License-Identifier: MIT

    Copyright (c) 1992-2004 UserLand Software, Inc.
    Copyright (c) 2025-present Frontier contributors

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

// 2025-11-26 Codex: Expose raw db read/write helpers for split reader/writer modules.
// 2025-12-01 Codex: Advertise Save As destination header helper for migration tooling.

#ifndef FRONTIER_DBINTERNAL_H
#define FRONTIER_DBINTERNAL_H

#define dbinternalinclude

#include <stdint.h>

// 2025-11-23 Codex: Added 64-bit block header/trailer layout for v7 roots; legacy sizes remain for v6.

#define SMART_DB_OPENING	1
//#undef SMART_DB_OPENING


//RAB	#define dbversionnumber 3 /**VERSION used in 4.x**/

//AR	#define dbversionnumber 5 /**VERSION used up to 6.2a10**/

#ifdef SMART_DB_OPENING
	#define dbversionnumber 7 /* 64-bit format with legacy hash tables */
	#define dbversionnumber_legacy 6 /* Legacy 32-bit format */
	#define dbfirstversionwithcachedshadowavaillist 6 /* 6.2a11 AR */
#else
	#define dbversionnumber 5 /**VERSION used up to 6.2a10**/
#endif

#define dbversionnumberminor 0


#define dbsystemidMac	0

#define dbsystemidWin32	1


#define minblocksize 32L
#define firstphysicaladdress (long)sizeof(tydatabaserecord)


typedef enum {
	dbdirtymask		= 0x0001
	} tydbflagmask;


#ifndef TYVARIANCE_DEFINED
#define TYVARIANCE_DEFINED 1
typedef int32_t tyvariance;
#endif

#pragma pack(2)
typedef struct tysizefreeword32 {
	int32_t size;
	} tysizefreeword32;

typedef struct tysizefreeword64 {
	int64_t size;
	} tysizefreeword64;


typedef struct tyheader32 {

	tysizefreeword32 sizefreeword;
	
	tyvariance variance;
	} tyheader32, *ptrheader32, **hdlheader32;
	
typedef struct tyheader64 {

	tysizefreeword64 sizefreeword;
	
	tyvariance variance;
	} tyheader64, *ptrheader64, **hdlheader64;
	
typedef struct tytrailer32 {

	tysizefreeword32 sizefreeword;
	} tytrailer32, *ptrtrailer32, **hdltrailer32;

typedef struct tytrailer64 {

	tysizefreeword64 sizefreeword;
	} tytrailer64, *ptrtrailer64, **hdltrailer64;

#define sizeheader_v6 (long) sizeof (tyheader32)
#define sizeheader_v7 (long) sizeof (tyheader64)
#define sizetrailer_v6 (long) sizeof (tytrailer32)
#define sizetrailer_v7 (long) sizeof (tytrailer64)
#define sizeheader (db_format_mode_current().use_64bit_format ? sizeheader_v7 : sizeheader_v6)
#define sizetrailer (db_format_mode_current().use_64bit_format ? sizetrailer_v7 : sizetrailer_v6)

/* Explicit-handle format detection helpers — do NOT read global format mode.
   Used by _hdb/_fnum functions throughout db.c and callers like tableexternal. */

static inline boolean db_hdb_use64 (hdldatabaserecord hdb) {
	return (hdb != nil && (**hdb).headerLength == (long) sizeof (tydatabaserecord_64));
}

static inline long db_hdb_header_size (hdldatabaserecord hdb) {
	return db_hdb_use64(hdb) ? sizeheader_v7 : sizeheader_v6;
}

static inline long db_hdb_trailer_size (hdldatabaserecord hdb) {
	return db_hdb_use64(hdb) ? sizetrailer_v7 : sizetrailer_v6;
}

static inline hdlfilenum db_hdb_fnum (hdldatabaserecord hdb) {
	if (hdb == nil) return (hdlfilenum) -1;
	return (hdlfilenum)((**hdb).fnumdatabase);
}


#define dbshadow

typedef struct availnodeshadow {
	
	dbaddress adr;

	int64_t size;
	// next record in this array is the next free block
	} tyavailnodeshadow, ** hdlavaillistshadow;
#pragma options align=reset

/*prototypes*/

extern boolean dbgeteof (long *);
extern boolean dbwrite (dbaddress, long, ptrvoid);
extern boolean dbread (dbaddress, long, ptrvoid);

extern boolean dbreadtrailer (dbaddress, boolean *, long *);

extern boolean dbreadheader (dbaddress, boolean *, long *, tyvariance *);
extern boolean dbreadheader_destination (dbaddress, boolean *, long *, tyvariance *);

extern boolean dbreadavailnode (dbaddress, boolean *, long *, dbaddress *);

#endif /* FRONTIER_DBINTERNAL_H */
