/* Paige utilities, miscellaneous "exclusion" and layout support functions.
Copyright 1993 / 1994 DataPak Software, Inc. All rights reserved.  */

#include "Paige.h"

#ifdef MAC_PLATFORM
#pragma segment pgboxes
#endif

#include "machine.h"
#include "pgUtils.h"
#include "pgText.h"
#include "pgShapes.h"
#include "pgEdit.h"
#include "pgSelect.h"
#include "pgErrors.h"
#include "pgFrame.h"

#define INVAL_SLOP_VALUE -16

static void validate_exclusion_refcons (paige_rec_ptr pg);
static text_block_ptr find_first_block (paige_rec_ptr pg, rectangle_ptr exclusion);
static long inval_exclusion_change (paige_rec_ptr pg, rectangle_ptr exclusion);
static void report_shape_change (paige_rec_ptr pg);

#ifdef PG_DEBUG
static void check_position_range (paige_rec_ptr pg, pg_short_t position);
#endif


/* pgNumExclusions returns number of exclusions.  */

PG_PASCAL (pg_short_t) pgNumExclusions (pg_ref pg)
{
	paige_rec_ptr		pg_rec;
	pg_short_t			result;
	
	pg_rec = (paige_rec_ptr) UseMemory(pg);
	
	if (pgEmptyShape(pg_rec->exclude_area))
		result = 0;
	else
		result = (pg_short_t)GetMemorySize(pg_rec->exclude_area) - 1;

	UnuseMemory(pg);
	
	return	result;
}


/* pgGetExclusion returns exclusion rect position. It is scrolled scaled
based on respective booleans. */

PG_PASCAL (void) pgGetExclusion (pg_ref pg, pg_short_t position,
		pg_boolean include_scroll, pg_boolean include_scale,
		rectangle_ptr exclusion)
{
	paige_rec_ptr		pg_rec;
	
	pg_rec = (paige_rec_ptr) UseMemory(pg);

	GetMemoryRecord(pg_rec->exclude_area, position, exclusion);
	
	if (include_scroll)
		pgOffsetRect(exclusion, -pg_rec->scroll_pos.h,  -pg_rec->scroll_pos.v);
	if (include_scale) {
		co_ordinate		offset_extra;
		
		offset_extra = pg_rec->scroll_pos;
		pgNegatePt(&offset_extra);

		if (!include_scroll)
			offset_extra.h = offset_extra.v = 0;

		pgScaleRect(&pg_rec->scale_factor, &offset_extra, exclusion);
	}

	UnuseMemory(pg);
}


/* pgPtInExclusion returns the exclusion, if any, containing point. Each
exclusion is inset by insert_extra if non-NULL.  */

PG_PASCAL (pg_short_t) pgPtInExclusion (pg_ref pg, const co_ordinate_ptr point,
		const co_ordinate_ptr inset_extra)
{
	paige_rec_ptr		pg_rec;
	co_ordinate			scaled_pt;
	pg_short_t			result;

	if (!pgNumExclusions(pg))
		return	0;

	pg_rec = (paige_rec_ptr) UseMemory(pg);

	scaled_pt = *point;
	pgScaleLong(-pg_rec->scale_factor.scale, pg_rec->scale_factor.origin.h,
			&scaled_pt.h);
	pgScaleLong(-pg_rec->scale_factor.scale, pg_rec->scale_factor.origin.v,
			&scaled_pt.v);
	pgAddPt(&pg_rec->scroll_pos, &scaled_pt);

	result = pgPtInShape(pg_rec->exclude_area, &scaled_pt, NULL, inset_extra, NULL);

	UnuseMemory(pg);
	
	return	result;
}


/* pgInsertExclusion adds a new exclusion after position (if zero, adds to top). */

PG_PASCAL (void) pgInsertExclusion (pg_ref pg, const rectangle_ptr exclusion,
		pg_short_t position, long ref_con, short draw_mode)
{
	paige_rec_ptr		pg_rec;
	rectangle_ptr		wrap_r;
	long PG_FAR			*ref_longs;
	long				first_display, num_par_exclusions;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	
	validate_exclusion_refcons(pg_rec);

	if (pgEmptyShape(pg_rec->exclude_area))
		pgAddRectToShape(pg_rec->exclude_area, exclusion);
	else {
		wrap_r = (rectangle_ptr) InsertMemory(pg_rec->exclude_area, position + 1, 1);
		pgBlockMove(exclusion, wrap_r, sizeof(rectangle));
		UnuseMemory(pg_rec->exclude_area);
		
		if ((num_par_exclusions = GetMemorySize(pg_rec->par_exclusions)) > 0) {
			style_run_ptr			run;
			
			run = (style_run_ptr) UseMemory(pg_rec->par_exclusions);
			
			while (num_par_exclusions) {
				
				if (run->style_item > position)
					run->style_item += 1;
				
				++run;
				--num_par_exclusions;
			}

			UnuseMemory(pg_rec->par_exclusions);
		}
	}

	ref_longs = (long *) InsertMemory(pg_rec->exclusions, position, 1);
	*ref_longs = ref_con;
	UnuseMemory(pg_rec->exclusions);
	
	report_shape_change(pg_rec);

	first_display = inval_exclusion_change(pg_rec, exclusion);

	if (draw_mode) {
		short		use_draw_mode;
		
		if ((use_draw_mode = draw_mode) == best_way)
			use_draw_mode = bits_copy;
			
		pgUpdateText(pg_rec, NULL, first_display, pg_rec->t_length,
			MEM_NULL, NULL, use_draw_mode, TRUE);
		pgDrawPages(pg_rec, NULL, NULL, MEM_NULL, use_draw_mode);
	}

	++pg_rec->change_ctr;

	UnuseMemory(pg);
}


/* pgRemoveExclusion deletes the exclusion position. */

PG_PASCAL (void) pgRemoveExclusion (pg_ref pg, pg_short_t position,
		short draw_mode)
{
	paige_rec_ptr		pg_rec;
	rectangle			deleted_r;
	long				first_display, num_par_exclusions;

	pg_rec = (paige_rec_ptr) UseMemory(pg);

#ifdef PG_DEBUG
	check_position_range(pg_rec, position);
#endif

	if (GetMemorySize(pg_rec->exclude_area) <= SIMPLE_SHAPE_QTY) {
		
		pgSetShapeRect(pg_rec->exclude_area, NULL);
		SetMemorySize(pg_rec->exclusions, 0);
		pgInvalSelect(pg, 0, pg_rec->t_length);
		first_display = 0;
	}
	else {
		
		GetMemoryRecord(pg_rec->exclude_area, position, &deleted_r);

		if (pg_rec->flags2 & HAS_PG_FRAMES_BIT) {
			memory_ref		frame_ref;
			co_ordinate		scroll;
			frame_callback	callback;
			pg_frame_ptr	frame;
			
			GetMemoryRecord(pg_rec->exclusions, position - 1, &frame_ref);
			frame = (pg_frame_ptr) UseMemory(frame_ref);
			scroll = pg_rec->scroll_pos;
			pgNegatePt(&scroll);
			callback = (frame_callback)frame->callback;
			callback(pg_rec, frame, &deleted_r, &scroll, FRAME_DESTROY_VERB);
			UnuseAndDispose(frame_ref);
		}

		validate_exclusion_refcons(pg_rec);

		DeleteMemory(pg_rec->exclude_area, position, 1);
		DeleteMemory(pg_rec->exclusions, position - 1, 1);
		first_display = inval_exclusion_change(pg_rec, &deleted_r);
		
		if ((num_par_exclusions = GetMemorySize(pg_rec->par_exclusions)) > 0) {
			style_run_ptr			run;
			long					run_index;
			pg_short_t				deleted_rec;

			run = (style_run_ptr) UseMemory(pg_rec->par_exclusions);
			deleted_rec = position - 1;
			
			for (run_index = 0; run_index < num_par_exclusions; ++run_index, ++run) {
				
				if (run->style_item > deleted_rec)
					run->style_item -= 1;
				else
				if (run->style_item == deleted_rec) {
					
					UnuseMemory(pg_rec->par_exclusions);
					DeleteMemory(pg_rec->par_exclusions, run_index, 1);
					--num_par_exclusions;
					run = (style_run_ptr) UseMemory(pg_rec->par_exclusions);
					run += run_index;
				}
			}

			UnuseMemory(pg_rec->par_exclusions);
		}
	}

	report_shape_change(pg_rec);

	if (draw_mode)
		pgUpdateText(pg_rec, NULL, first_display, pg_rec->t_length,
			MEM_NULL, NULL, draw_mode, TRUE);

	++pg_rec->change_ctr;

	UnuseMemory(pg);
}


/* pgReplaceExclusion changes position exclusion to exclusion rect. */

PG_PASCAL (void) pgReplaceExclusion (pg_ref pg, const rectangle_ptr exclusion,
		pg_short_t position, short draw_mode)
{
	paige_rec_ptr		pg_rec;
	rectangle_ptr		wrap_r;
	long				first_display, second_display, exclusion_display;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	wrap_r = (rectangle_ptr) UseMemoryRecord(pg_rec->exclude_area, position, 0, TRUE);
	
	exclusion_display = pgGetAttachedPar(pg, position);
	first_display = inval_exclusion_change(pg_rec, wrap_r);
	pgBlockMove(exclusion, wrap_r, sizeof(rectangle));
	
	UnuseMemory(pg_rec->exclude_area);
	
	second_display = inval_exclusion_change(pg_rec, exclusion);
	if (second_display < first_display)
		first_display = second_display;
	
	if (exclusion_display >= 0) {
		
		pgInvalSelect(pg, exclusion_display, pg_rec->t_length);
		
		if (exclusion_display < first_display)
			first_display = exclusion_display;
	}

	report_shape_change(pg_rec);

	if (draw_mode) {
		short		use_draw_mode;
		
		if ((use_draw_mode = draw_mode) == best_way)
			use_draw_mode = bits_copy;

		pgUpdateText(pg_rec, NULL, first_display, pg_rec->t_length,
			MEM_NULL, NULL, use_draw_mode, TRUE);
		pgDrawPages(pg_rec, NULL, NULL, MEM_NULL, use_draw_mode);
	}
	
	++pg_rec->change_ctr;

	UnuseMemory(pg);
}


/* pgSwapExclusions reverse exclusion1 and exclusion2 positions. */

PG_PASCAL (void) pgSwapExclusions (pg_ref pg, pg_short_t exclusion1,
		pg_short_t exclusion2, short draw_mode)
{
	paige_rec_ptr		pg_rec;
	rectangle_ptr		wrap_r;
	rectangle			hold_r;
	long PG_FAR			*ref_longs;
	long				hold, first_display, second_display;

	pg_rec = (paige_rec_ptr) UseMemory(pg);

#ifdef PG_DEBUG
	check_position_range(pg_rec, exclusion1);
	check_position_range(pg_rec, exclusion2);
#endif

	validate_exclusion_refcons(pg_rec);

	wrap_r = (rectangle_ptr) UseMemory(pg_rec->exclude_area);
	ref_longs = (long *) UseMemory(pg_rec->exclusions);
	
	pgBlockMove(&wrap_r[exclusion1], &hold_r, sizeof(rectangle));
	pgBlockMove(&wrap_r[exclusion2], &wrap_r[exclusion1], sizeof(rectangle));
	pgBlockMove(&hold_r, &wrap_r[exclusion2], sizeof(rectangle));

	hold = ref_longs[exclusion1 - 1];
	ref_longs[exclusion1 - 1] = ref_longs[exclusion2 - 1];
	ref_longs[exclusion2 - 1] = hold;

	first_display = inval_exclusion_change(pg_rec, &wrap_r[exclusion1]);
	second_display = inval_exclusion_change(pg_rec, &wrap_r[exclusion2]);

	UnuseMemory(pg_rec->exclude_area);
	UnuseMemory(pg_rec->exclusions);

	if (first_display < second_display)
		first_display = second_display;

	report_shape_change(pg_rec);

	if (draw_mode)
		pgUpdateText(pg_rec, NULL, first_display, pg_rec->t_length,
			MEM_NULL, NULL, draw_mode, TRUE);

	++pg_rec->change_ctr;

	UnuseMemory(pg);
}


/* pgGetExclusionRefCon returns the app-specific refCon with exclusion. */

PG_PASCAL (long) pgGetExclusionRefCon (pg_ref pg, pg_short_t position)
{
	paige_rec_ptr		pg_rec;
	long				result;

	pg_rec = (paige_rec_ptr) UseMemory(pg);


#ifdef PG_DEBUG
	check_position_range(pg_rec, position);
#endif

	validate_exclusion_refcons(pg_rec);
	GetMemoryRecord(pg_rec->exclusions, position - 1, &result);

	UnuseMemory(pg);
	
	return	result;
}


/* pgSetExclusionRefCon sets the app-specific long associated with exclusion. */

PG_PASCAL (void) pgSetExclusionRefCon (pg_ref pg, pg_short_t position, long ref_con)
{
	paige_rec_ptr		pg_rec;
	long PG_FAR			*ref_longs;

	pg_rec = (paige_rec_ptr) UseMemory(pg);

#ifdef PG_DEBUG
	check_position_range(pg_rec, position);
#endif

	validate_exclusion_refcons(pg_rec);
	
	ref_longs = (long *) UseMemoryRecord(pg_rec->exclusions, position - 1, 0, TRUE);
	*ref_longs = ref_con;
	UnuseMemory(pg_rec->exclusions);

	UnuseMemory(pg);
}


/* pgInsertExclusionShape inserts an entire shape into exclude area after
position.  */

PG_PASCAL (void) pgInsertExclusionShape (pg_ref pg, pg_short_t position,
		shape_ref exclude_shape, short draw_mode)
{
	paige_rec_ptr		pg_rec;
	rectangle_ptr		wrap_r, new_rects;
	rectangle			new_bounds;
	long				num_par_exclusions;
	long PG_FAR			*ref_longs;
	pg_short_t			num_new_rects, first_display;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	
	validate_exclusion_refcons(pg_rec);
	num_new_rects = (pg_short_t)GetMemorySize(exclude_shape) - 1;

	if (pgEmptyShape(pg_rec->exclude_area))
		pgSetAreas(pg, MEM_NULL, MEM_NULL, exclude_shape);
	else {
		wrap_r = (rectangle_ptr) InsertMemory(pg_rec->exclude_area, position + 1, num_new_rects);
		new_rects = (rectangle_ptr) UseMemoryRecord(exclude_shape, 1, USE_ALL_RECS, TRUE);
		pgBlockMove(new_rects, wrap_r, sizeof(rectangle) * num_new_rects);
		UnuseMemory(exclude_shape);
		UnuseMemory(pg_rec->exclude_area);

		if ((num_par_exclusions = GetMemorySize(pg_rec->par_exclusions)) > 0) {
			style_run_ptr			run;
			
			run = (style_run_ptr) UseMemory(pg_rec->par_exclusions);
			
			while (num_par_exclusions) {
				
				if (run->style_item > position)
					run->style_item += num_new_rects;
				
				++run;
				--num_par_exclusions;
			}

			UnuseMemory(pg_rec->par_exclusions);
		}
	}

	ref_longs = (long *) InsertMemory(pg_rec->exclusions, position, num_new_rects);
	pgFillBlock(ref_longs, sizeof(long) * num_new_rects, 0);
	UnuseMemory(pg_rec->exclusions);

	report_shape_change(pg_rec);
	
	pgShapeBounds(exclude_shape, &new_bounds);
	first_display = (pg_short_t)inval_exclusion_change(pg_rec, &new_bounds);

	if (draw_mode)
		pgUpdateText(pg_rec, NULL, first_display, pg_rec->t_length,
			MEM_NULL, NULL, draw_mode, TRUE);

	++pg_rec->change_ctr;

	UnuseMemory(pg);
}


/* pgAttachParExclusion attaches an existing exclusion area to the paragraph at the
specified position (which can be CURRENT_POSITION).  The exclusion rect is index
which is 1 through num exclusions. YOU CAN REMOVE an existing paragraph exclusion by
passing zero for index.  */

PG_PASCAL (long) pgAttachParExclusion (pg_ref pg, long position, pg_short_t index, short draw_mode)
{
	paige_rec_ptr			pg_rec;
	style_run_ptr			run;
	size_t					par_position, num_runs, run_index;
	short					use_draw_mode;
	
	pg_rec = (paige_rec_ptr) UseMemory(pg);
	
	if ((use_draw_mode = draw_mode) == best_way)
		use_draw_mode = bits_copy;
	
	pgFindPar(pg, pgFixOffset(pg_rec, position), &par_position, NULL);
	
	num_runs = GetMemorySize(pg_rec->par_exclusions);
	run = (style_run_ptr) UseMemory(pg_rec->par_exclusions);
	
	for (run_index = 0; run_index < num_runs; ++run_index, ++run)
		if (run->offset >= par_position) {
			
			if (run->offset == par_position) {
				
				UnuseMemory(pg_rec->par_exclusions);
				DeleteMemory(pg_rec->par_exclusions, run_index, 1);
				UseMemory(pg_rec->par_exclusions);
			}
			
			break;
		}

	UnuseMemory(pg_rec->par_exclusions);

	if (index) {
	
		run = (style_run_ptr) InsertMemory(pg_rec->par_exclusions, run_index, 1);
		run->offset = par_position;
		run->style_item = index;
		UnuseMemory(pg_rec->par_exclusions);
	}

	pgInvalSelect(pg, par_position, pg_rec->t_length);
	
	if (use_draw_mode)
		pgUpdateText(pg_rec, NULL, par_position, pg_rec->t_length, MEM_NULL, NULL, use_draw_mode, TRUE);

	UnuseMemory(pg);
	
	return	par_position;
}


/* pgGetAttachedPar returns the offset of a paragraph, if any, belonging to exclusion. If no
attachment then -1 is returned. */

PG_PASCAL (long) pgGetAttachedPar (pg_ref pg, pg_short_t exclusion)
{
	paige_rec_ptr		pg_rec;
	style_run_ptr		run;
	long				result, num_runs;
	
	pg_rec = (paige_rec_ptr) UseMemory(pg);
	result = -1;
	
	if ((num_runs = GetMemorySize(pg_rec->par_exclusions)) > 0) {
		
		for (run = (style_run_ptr) UseMemory(pg_rec->par_exclusions); num_runs; ++run, --num_runs)
			if (run->style_item == exclusion) {
				
				result = run->offset;
				break;
			}
		
		UnuseMemory(pg_rec->par_exclusions);
	}

	UnuseMemory(pg);
	
	return		result;
}



/****************************** Local Functions ***************************/


/* This function makes sure exclusion refcon quantity matches with the number
of exclusions and, if not, fixes it. (This could happen by app starting with
"exclusions" but no refcons. */

static void validate_exclusion_refcons (paige_rec_ptr pg)
{
	long		refcon_size, exclusion_size;

	refcon_size = GetMemorySize(pg->exclusions);
	
	if (pgEmptyShape(pg->exclude_area)) {
		
		if (refcon_size)
			SetMemorySize(pg->exclusions, 0);
		
		return;
	}

	exclusion_size = GetMemorySize(pg->exclude_area) - 1;
	
	if (refcon_size != exclusion_size) {
		
		if (exclusion_size > refcon_size) {
			
			AppendMemory(pg->exclusions, exclusion_size - refcon_size, TRUE);
			UnuseMemory(pg->exclusions);
		}
		else
			SetMemorySize(pg->exclusions, exclusion_size);
	}
}


/* This function locates the first text_block that potentially intersects
the exclusion rect.  */

static text_block_ptr find_first_block (paige_rec_ptr pg, rectangle_ptr exclusion)
{
	register text_block_ptr		block;
	register long				num_blocks;
	rectangle					combined_r, inset_exclude;
	
	num_blocks = GetMemorySize(pg->t_blocks);
	block = (text_block_ptr) UseMemory(pg->t_blocks);
	pgBlockMove(&block->bounds, &combined_r, sizeof(rectangle));
	pgBlockMove(exclusion, &inset_exclude, sizeof(rectangle));
	
	pgInsetRect(&inset_exclude, INVAL_SLOP_VALUE, INVAL_SLOP_VALUE);

	while (num_blocks) {
		
		pgUnionRect(&block->bounds, &combined_r, &combined_r);

		if ((block->flags & NEEDS_CALC) || pgSectRect(&combined_r, &inset_exclude, NULL)) {
			
			if (block->begin)
				--block;

			return	block;
		}

		++block;
		--num_blocks;
	}
	
	UnuseMemory(pg->t_blocks);
	
	return	NULL;
}


/* This invalidates all text starting at exclusion rect. It also returns
the best place to start displaying.  */

static long inval_exclusion_change (paige_rec_ptr pg, rectangle_ptr exclusion)
{
	text_block_ptr			block;

	pg->port.clip_info.change_flags |= CLIP_EXCLUDE_CHANGED;

	if (block = find_first_block(pg, exclusion)) {
		long		result;
		
		result = block->begin;

		pgInvalSelect(pg->myself, result, pg->t_length);
		UnuseMemory(pg->t_blocks);
		
		return	result;
	}

	pgComputeDocHeight(pg, FALSE);

	return	pg->t_length;
}


/* This function "reports" that the page_area has changed, first by re-building
the bounds rect in the page shape, then by telling machinespecific code that
clip regions might be different. */

static void report_shape_change (paige_rec_ptr pg)
{
	pg->port.clip_info.change_flags |= CLIP_EXCLUDE_CHANGED;
	pgResetBounds(pg->exclude_area);
}


#ifdef PG_DEBUG

/* Checks for valid exclusion number if debug mode. */

static void check_position_range (paige_rec_ptr pg, pg_short_t position)
{
	short			check_sign;
	
	check_sign = position;
	
	if ( (check_sign < 1) || (position > (pg_short_t)GetMemorySize(pg->exclude_area)) )
		pgFailure (pg->globals->mem_globals, BAD_REFCON_ID_ERROR, check_sign);
}

#endif

