/* 
	Paige List Management Support
	
	File:		pgLists.h

	by T. R. Shaw

	Copyright © 1994-95 OITC - All rights reserved.
	
	This software has been privately developed and falls within 
	DFARS 252.227-7013(c)(1) and associated regulations and it or its
	derivatives may not be used outside of the License agreement with 
	T. R. Shaw and OITC.  Copyright may not be removed.

	oitc@iu.net

	Revision History
	07/03/95	1.0b4	TRS - Initial release
	
*/

#ifndef PGLISTS_H
#define PGLISTS_H

#include	"Paige.h"

struct pg_list_rec {
	pg_ref		pg;				/* Paige ref */
	long		flags;			/* horizontal position */
};
typedef struct pg_list_rec pg_list_rec;
typedef pg_list_rec PG_FAR *pg_list_ptr;

//typedef co_ordinate PG_FAR *co_ordinate_ptr; /* "FAR" pointer for coordinate */


#define PG_FIRST_IN_LIST			0
#define NOT_IN_LIST					-1
#define PG_LAST_IN_LIST				-2

typedef PG_FN_PASCAL(pg_boolean, pg_list_compare_proc) (void PG_FAR *list_side, void PG_FAR *compare_data);
typedef PG_FN_PASCAL(pg_boolean, pg_list_delete_proc) (void PG_FAR *list_side);


#ifdef __cplusplus
extern	"C" {
#endif


extern PG_PASCAL (long) pgPushToList(memory_ref list, void PG_FAR *data);
extern PG_PASCAL (long) pgAppendToList(memory_ref list, void PG_FAR *data);
extern PG_PASCAL (long) pgInsertInList(memory_ref list, long index, void PG_FAR *data);
extern PG_PASCAL (long) pgFindInList(memory_ref list, void PG_FAR *data, pg_list_compare_proc compare_proc, void PG_FAR *compare_data);
extern PG_PASCAL (long) pgGetFromList(memory_ref list, long index, void PG_FAR *data);
extern PG_PASCAL (long) pgChangeInList(memory_ref list, long index, void PG_FAR *data);
extern PG_PASCAL (long) pgRemoveFromList(memory_ref list, long index, pg_list_delete_proc delete_proc);
extern PG_PASCAL (long) pgTakeFromList(memory_ref list, long index, void PG_FAR *data);
extern PG_PASCAL (long) pgPopOffList(memory_ref list, void PG_FAR *data);

extern PG_PASCAL (pg_boolean) pgListCompareLong(void PG_FAR *list_side, void PG_FAR *compare_data);
extern PG_PASCAL (pg_boolean) pgListCompareShort(void PG_FAR *list_side, void PG_FAR *compare_data);

#ifdef __cplusplus
	}
#endif

#endif


