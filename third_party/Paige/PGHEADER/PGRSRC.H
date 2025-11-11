/* 
	Paige Transportable Resource API file
	
	File:		pgRsrc.h

	by T. R. Shaw

	Copyright © 1995 OITC, Inc.
	All rights reserved.
	
	pgRsrc.h - Routines for cross platform resource management.

	This software has been privately developed and falls within 
	DFARS 252.227-7013(c)(1) and associated regulations and it or its
	derivatives may not be used outside of the License agreement with 
	T. R. Shaw and OITC.  Copyright may not be removed.
	
	oitc@iu.net

	Revision History
	03/27/95	1.0b1	TRS - Initial release for the mac
	06/20/95	1.0b2	TRS - Corrected incompatibilities between Mac and Intel for resources
	08/30/95	1.0b5	TRS - Cross platform resource compatible
*/


#ifndef PGRSRC_H
#define PGRSRC_H

#include "Paige.h"
#include "pgIO.h"

#ifdef MAC_PLATFORM

#include <resources.h>

typedef short pg_resource_ref;				/* Mac resource file reference */
typedef Handle pg_rsrc_ref;					/* Mac resource reference */
typedef Handle pg_rsrc_data_ref;			/* Mac resource memory reference */
typedef ResType pg_rsrc_type;				/* Mac resource type */
typedef short pg_rsrc_id;					/* Mac resource id */

#endif

#ifdef WINDOWS_PLATFORM

#ifdef WIN32_COMPILE
#include <winbase.h>
#endif
#ifdef WIN16_COMPILE
#include <olenls.h>
#endif

typedef HINSTANCE pg_resource_ref;			/* Windows resource hModule reference */
typedef HRSRC pg_rsrc_ref;					/* Windows resource reference */
typedef HGLOBAL pg_rsrc_data_ref;			/* Windows resource memory reference */
typedef LPCSTR pg_rsrc_type;				/* Windows resource name */
typedef long pg_rsrc_id;					/* Windows resource type */

#endif

#define PG_RSRC_TYPE	'rsrc'
#define PG_RSRC_CREATOR	'RSED'

#ifdef __cplusplus
extern	"C" {
#endif

		
#define PG_FAIL_RES_ERROR()										\
		pgFailResError( __pg_globals_p)
		
#define PG_FAIL_NIL_RES(p)										\
		pgFailNILRes( __pg_globals_p, (void PG_FAR *)(p))

/**************************  Main Functions ***************************/

extern PG_C (pg_resource_ref) pgCurResRef(void);
extern PG_C (void) pgSetRsrcRef(pg_resource_ref which);
extern PG_C (pg_error) pgResError(void);
extern PG_C (pg_rsrc_ref) pgGetThisResource(pg_resource_ref which, pg_rsrc_type theType, pg_rsrc_id theID);
extern PG_C (pg_rsrc_ref) pgGetResource(pg_resource_ref which, pg_rsrc_type theType, pg_rsrc_id theID);
extern PG_C (void) pgReleaseResource(pg_rsrc_ref ref);
extern PG_C (generic_var) pgLockResource(pg_rsrc_ref ref);
extern PG_C (void) pgUnlockResource(pg_rsrc_ref ref);
extern PG_C (void) pgLoadResource(pg_rsrc_ref ref);
extern PG_C (long) pgResourceSize(pg_rsrc_ref ref);
extern PG_C (memory_ref) pgRsrcRef2Memory(pgm_globals_ptr mem_globals, pg_rsrc_ref ref, short struct_size, long rsrc_size);

extern PG_PASCAL (void) pgFailResError( pgm_globals_ptr pgm_globals_p );
extern PG_PASCAL (void) pgFailNILRes( pgm_globals_ptr pgm_globals_p, void PG_FAR *p);

#ifdef __cplusplus
	}
#endif

#endif


