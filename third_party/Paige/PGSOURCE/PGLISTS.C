/* 
	Paige List Management Support
	
	File:		pgLists.c

	by T. R. Shaw <tshaw@oitc.com>

	Copyright © 1994-96 OITC - All rights reserved.
	
	This software has been privately developed and falls within 
	DFARS 252.227-7013(c)(1) and associated regulations and it or its
	derivatives may not be used outside of the License agreement with 
	T. R. Shaw and OITC.  Copyright may not be removed.

	info@oitc.com
	http://www.oitc.com/

	Revision History
	07/03/95	1.0b4	TRS - Initial release
	01/20/96	1.0		TRS/CP - Final
	
*/

#ifndef PGLISTS_H
#include	"pgLists.h"
#endif

#include	"DefProcs.h"
#include	"pgShapes.h"
#include	"pgUtils.h"
#include	"pgText.h"
#include	"machine.h"


PG_PASCAL (long) pgPushToList(pg_list_ref list, void PG_FAR *data)
{
	void PG_FAR *	p;
	
	p = InsertMemory (list, 0, 1);
	pgBlockMove(data, p, GetMemoryRecSize(list));
	UnuseMemory(list);
	return 0;
}


PG_PASCAL (long) pgAppendToList(pg_list_ref list, void PG_FAR *data)
{
	void PG_FAR *	p;
	long			result;
	
	result = GetMemorySize(list);
	p = AppendMemory (list, 1, FALSE);
	pgBlockMove(data, p, GetMemoryRecSize(list));
	UnuseMemory(list);
	return result;
}


PG_PASCAL (long) pgInsertInList(pg_list_ref list, long index, void PG_FAR *data)
{
	void PG_FAR *	p;
	long			indexx = index;
	
	if (index == PG_FIRST_IN_LIST)
		indexx = 0;
	else if (index == PG_LAST_IN_LIST)
		indexx = GetMemorySize(list) - 1;
		
	if (indexx <= GetMemorySize(list) && index >= 0)
	{
		p = InsertMemory (list, indexx, 1);
		pgBlockMove(data, p, GetMemoryRecSize(list));
		UnuseMemory(list);
		return indexx;
	}
	return NOT_IN_LIST;
}


PG_PASCAL (long) pgFindInList(pg_list_ref list, void PG_FAR *data, pg_list_compare_proc compare_proc, void PG_FAR *compare_data)
{
	long	i;
	long	found = NOT_IN_LIST;

	for (i = 0; i < GetMemorySize(list); i++)
	{
		pg_byte_ptr				p;
		
		p = (pg_byte_ptr) UseMemoryRecord (list, i, 1 , TRUE);

		if (compare_proc((void PG_FAR *)p, compare_data))
		{
			found = i;
			if (data)
				pgBlockMove(p, data, GetMemoryRecSize(list));
			UnuseMemory(list);
			break;
		}
		UnuseMemory(list);
	}
	return found;
}


PG_PASCAL (long) pgChangeInList(pg_list_ref list, long index, void PG_FAR *data)
{
	long			indexx = index;
	
	if (index == PG_FIRST_IN_LIST)
		indexx = 0;
	else if (index == PG_LAST_IN_LIST)
		indexx = GetMemorySize(list) - 1;
		
	if (indexx < GetMemorySize(list) && index >= 0)
	{
		pg_byte_ptr				p;
		
		p = (pg_byte_ptr) UseMemoryRecord (list, indexx, 1 , TRUE);
		if (data)
			pgBlockMove(data, p, GetMemoryRecSize(list));
		UnuseMemory(list);
		
		return indexx;
	}
	return NOT_IN_LIST;
}


PG_PASCAL (long) pgGetFromList(pg_list_ref list, long index, void PG_FAR *data)
{
	long	indexx = index;
	
	if (index == PG_FIRST_IN_LIST)
		indexx = 0;
	else if (index == PG_LAST_IN_LIST)
		indexx = GetMemorySize(list) - 1;
		
	if (indexx < GetMemorySize(list) && indexx >= 0)
	{
		pg_byte_ptr				p;
		
		p = (pg_byte_ptr) UseMemoryRecord (list, indexx, 1 , TRUE);
		if (data)
			pgBlockMove(p, data, GetMemoryRecSize(list));
		UnuseMemory(list);
		
		return indexx;
	}
	return NOT_IN_LIST;
}


PG_PASCAL (long) pgRemoveFromList(pg_list_ref list, long index, pg_list_delete_proc delete_proc)
{
	long	found;
	
	found = pgGetFromList(list, index, NULL);

	if (found >= 0)
	{
		pg_byte_ptr				p;
		
		p = (pg_byte_ptr) UseMemoryRecord (list, index, 1 , TRUE);
		if (delete_proc)
			delete_proc(p);
		UnuseMemory(list);
		DeleteMemory (list, index, 1);
	}

	return found;
}


PG_PASCAL (long) pgTakeFromList(pg_list_ref list, long index, void PG_FAR *data)
{
	long	found;
	
	found = pgGetFromList(list, index, data);

	if (found >= 0)
		DeleteMemory (list, index, 1);

	return found;
}


PG_PASCAL (long) pgPopOffList(pg_list_ref list, void PG_FAR *data)
{
	return pgTakeFromList(list, 0, data);
}


PG_PASCAL (pg_boolean) pgListCompareLong(void PG_FAR *list_side, void PG_FAR *compare_data)
{
	return *((long PG_FAR *)list_side) == *((long PG_FAR *)compare_data);
}


PG_PASCAL (pg_boolean) pgListCompareShort(void PG_FAR *list_side, void PG_FAR *compare_data)
{
	return *((short PG_FAR *)list_side) == *((short PG_FAR *)compare_data);
}

