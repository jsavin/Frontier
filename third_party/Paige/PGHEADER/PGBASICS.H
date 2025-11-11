/* Header file for pgBasics.c */

#ifndef PGBASICS_H
#define PGBASICS_H


#include "Paige.h"


#define EXTRA_STRUCT_RSRV	16
#define MERGE_PAIR_ID		-EXTRA_STRUCT_RSRV	/* I.D. for GetExtraStruct in merging */

extern PG_PASCAL (void) pgInitExtraStruct (paige_rec_ptr pg);
extern PG_PASCAL (long) pgInCacheList (pgm_globals_ptr mem_globals, pg_ref pg);
extern PG_PASCAL (void) pgDrawGeneralBorders (paige_rec_ptr pg, long border_info, long border_color,
				long shading, rectangle_ptr bounds, pg_boolean no_insets);


#endif
