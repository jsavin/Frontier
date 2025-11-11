/* Header file for bi-directional text utilities */

#ifndef PGSCRIPT_H
#define PGSCRIPT_H

#include "Paige.h"


#ifdef __cplusplus
extern	"C" {
#endif

extern PG_PASCAL (pg_short_t) pgDirectionRun (point_start_ptr first_start, long num_starts,
		pg_boolean back_direction);
extern PG_PASCAL (pg_boolean) pgHasRightLeftRun (point_start_ptr first_start,
		long num_starts);
extern PG_PASCAL (void) pgInsertHiliteRect (paige_rec_ptr pg, text_block_ptr block,
		shape_ref rgn, t_select_ptr first_select, t_select_ptr last_select,
		long top, rectangle_ptr page_wrap_rect, long actual_begin_select,
		long actual_end_select);
extern PG_PASCAL (void) pgAdjustBiDirections (point_start_ptr first_start,
		pg_short_t num_starts, par_info_ptr par_format);
extern PG_PASCAL (void) pgAdjustRightLeftStarts (paige_rec_ptr pg, rectangle_ptr wrap_bounds,
	short wanted_direction, point_start_ptr first_start, pg_short_t num_starts);
extern PG_PASCAL (short) pgPrimaryTextDirection (pg_globals_ptr globals, par_info_ptr par);
extern PG_PASCAL (void) pgSetSecondaryCaret (paige_rec_ptr pg, text_block_ptr block,
		t_select_ptr selection);
extern PG_PASCAL (pg_boolean) pgCheckDirectionChange(paige_rec_ptr pg);
extern PG_PASCAL (long) pgFlipLong (rectangle_ptr base, long value, short wanted_direction);
extern PG_PASCAL (void) pgFlipRect (rectangle_ptr base, rectangle_ptr rect, short wanted_direction);
extern PG_PASCAL (long) pgRightLeftOffset (rectangle_ptr base, point_start_ptr the_start,
		short wanted_direction);

#ifdef __cplusplus
	}
#endif

#endif
