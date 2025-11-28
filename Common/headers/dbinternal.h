
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

// 2025-11-26 Codex: Expose raw db read/write helpers for split reader/writer modules.

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


typedef int32_t tyvariance;

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

extern boolean dbreadavailnode (dbaddress, boolean *, long *, dbaddress *);

#endif /* FRONTIER_DBINTERNAL_H */
