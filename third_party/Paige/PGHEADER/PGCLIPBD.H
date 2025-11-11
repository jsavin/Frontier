/* Header file for pgClipBd.c. Copyright 1993 DataPak Software, Inc.  */

#ifndef PGCLIPBD_H
#define PGCLIPBD_H

#include "Paige.h"

#ifdef __cplusplus
extern	"C" {
#endif


#ifdef PG_DEBUG

extern PG_PASCAL (void) pgDisposeFailedUndo (undo_ref ref);

#else

#define pgDisposeFailedUndo pgDisposeUndo

#endif

extern PG_PASCAL (pg_boolean) pgBytesMatchCriteria (paige_rec_ptr pg, style_walk_ptr walker,
		pg_char_ptr text, short data_type, size_t global_offset, size_t offset, size_t length);

#ifdef __cplusplus
	}
#endif

#endif
