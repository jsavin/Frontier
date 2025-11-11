/* Paige utilities, miscellaneous pagination and container support functions.
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


static void validate_container_refcons (paige_rec_ptr pg);
static text_block_ptr find_first_block (paige_rec_ptr pg, pg_short_t position);
static long inval_container_change (paige_rec_ptr pg, pg_short_t position);
static void report_shape_change (paige_rec_ptr pg);

#ifdef PG_DEBUG
static void check_position_range (paige_rec_ptr pg, pg_short_t position);
#endif


/* pgNumContainers returns number of containers.  */

PG_PASCAL (pg_short_t) pgNumContainers (pg_ref pg)
{
	paige_rec_ptr		pg_rec;
	pg_short_t			result;
	
	pg_rec = (paige_rec_ptr) UseMemory(pg);
	result = (pg_short_t)GetMemorySize(pg_rec->wrap_area) - 1;
	UnuseMemory(pg);
	
	return	result;
}


/* pgGetContainer returns container rect position. It is scrolled scaled
based on respective booleans. */

PG_PASCAL (void) pgGetContainer (pg_ref pg, pg_short_t position,
		pg_boolean include_scroll, pg_boolean include_scale,
		rectangle_ptr container)
{
	paige_rec_ptr		pg_rec;
	
	pg_rec = (paige_rec_ptr) UseMemory(pg);

	GetMemoryRecord(pg_rec->wrap_area, position, container);
	
	if (include_scroll)
		pgOffsetRect(container, -pg_rec->scroll_pos.h,  -pg_rec->scroll_pos.v);
		
	if (include_scale) {
		co_ordinate		offset_extra;
		
		offset_extra = pg_rec->scroll_pos;
		pgNegatePt(&offset_extra);

		if (!include_scroll)
			offset_extra.h = offset_extra.v = 0;

		pgScaleRect(&pg_rec->scale_factor, &offset_extra, container);
	}

	UnuseMemory(pg);
}


/* pgPtInContainer returns the container, if any, containing point. Each
container is inset by insert_extra if non-NULL.  */

PG_PASCAL (pg_short_t) pgPtInContainer (pg_ref pg, const co_ordinate_ptr point,
		const co_ordinate_ptr inset_extra)
{
	paige_rec_ptr		pg_rec;
	co_ordinate			scaled_pt;
	pg_short_t			result;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	
	scaled_pt = *point;
	pgScaleLong(-pg_rec->scale_factor.scale, pg_rec->scale_factor.origin.h,
			&scaled_pt.h);
	pgScaleLong(-pg_rec->scale_factor.scale, pg_rec->scale_factor.origin.v,
			&scaled_pt.v);
	pgAddPt(&pg_rec->scroll_pos, &scaled_pt);

	result = pgPtInShape(pg_rec->wrap_area, &scaled_pt, NULL, inset_extra, NULL);

	UnuseMemory(pg);
	
	return	result;
}


/* pgInsertContainer adds a new container after position (if zero, adds to top). */

PG_PASCAL (void) pgInsertContainer (pg_ref pg, const rectangle_ptr container,
		pg_short_t position, long ref_con, short draw_mode)
{
	paige_rec_ptr		pg_rec;
	rectangle_ptr		wrap_r;
	long PG_FAR			*ref_longs;
	long				first_display;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	
	validate_container_refcons(pg_rec);

	wrap_r = (rectangle_ptr) InsertMemory(pg_rec->wrap_area, position + 1, 1);
	pgBlockMove(container, wrap_r, sizeof(rectangle));
	UnuseMemory(pg_rec->wrap_area);
	
	ref_longs = (long PG_FAR	*) InsertMemory(pg_rec->containers, position, 1);
	*ref_longs = ref_con;
	UnuseMemory(pg_rec->containers);
	
	report_shape_change(pg_rec);

	first_display = inval_container_change(pg_rec, (pg_short_t)(position + 1));

	if (draw_mode)
		pgUpdateText(pg_rec, NULL, first_display, pg_rec->t_length,
			MEM_NULL, NULL, draw_mode, TRUE);
	
	++pg_rec->change_ctr;

	UnuseMemory(pg);
}


/* pgRemoveContainer deletes the container position. */

PG_PASCAL (void) pgRemoveContainer (pg_ref pg, pg_short_t position,
		short draw_mode)
{
	paige_rec_ptr		pg_rec;
	long				first_display;

	pg_rec = (paige_rec_ptr) UseMemory(pg);

#ifdef PG_DEBUG
	check_position_range(pg_rec, position);
#endif

	validate_container_refcons(pg_rec);
	
	DeleteMemory(pg_rec->wrap_area, position, 1);
	DeleteMemory(pg_rec->containers, position - 1, 1);
	first_display = inval_container_change(pg_rec, position);

	report_shape_change(pg_rec);

	if (draw_mode)
		pgUpdateText(pg_rec, NULL, first_display, pg_rec->t_length,
			MEM_NULL, NULL, draw_mode, TRUE);

	++pg_rec->change_ctr;

	UnuseMemory(pg);
}


/* pgReplaceContainer changes position container to container rect. */

PG_PASCAL (void) pgReplaceContainer (pg_ref pg, const rectangle_ptr container,
		pg_short_t position, short draw_mode)
{
	paige_rec_ptr		pg_rec;
	rectangle_ptr		wrap_r;
	long				first_display, second_display;

	pg_rec = (paige_rec_ptr) UseMemory(pg);

	first_display = inval_container_change(pg_rec, position);
	wrap_r = (rectangle_ptr) UseMemoryRecord(pg_rec->wrap_area, position, 0, TRUE);
	pgBlockMove(container, wrap_r, sizeof(rectangle));
	
	UnuseMemory(pg_rec->wrap_area);
	
	second_display = inval_container_change(pg_rec, position);
	if (second_display < first_display)
		first_display = second_display;

	report_shape_change(pg_rec);

	if (draw_mode)
		pgUpdateText(pg_rec, NULL, first_display, pg_rec->t_length,
			MEM_NULL, NULL, draw_mode, TRUE);

	++pg_rec->change_ctr;

	UnuseMemory(pg);
}


/* pgSwapContainers reverse container1 and container2 positions. */

PG_PASCAL (void) pgSwapContainers (pg_ref pg, pg_short_t container1,
		pg_short_t container2, short draw_mode)
{
	paige_rec_ptr		pg_rec;
	rectangle_ptr		wrap_r;
	rectangle			hold_r;
	long PG_FAR			*ref_longs;
	long				hold, first_display, second_display;

	pg_rec = (paige_rec_ptr) UseMemory(pg);

#ifdef PG_DEBUG
	check_position_range(pg_rec, container1);
	check_position_range(pg_rec, container2);
#endif

	validate_container_refcons(pg_rec);

	wrap_r = (rectangle_ptr)UseMemory(pg_rec->wrap_area);
	ref_longs = (long PG_FAR*) UseMemory(pg_rec->containers);
	
	pgBlockMove(&wrap_r[container1], &hold_r, sizeof(rectangle));
	pgBlockMove(&wrap_r[container2], &wrap_r[container1], sizeof(rectangle));
	pgBlockMove(&hold_r, &wrap_r[container2], sizeof(rectangle));
	
	hold = ref_longs[container1 - 1];
	ref_longs[container1 - 1] = ref_longs[container2 - 1];
	ref_longs[container2 - 1] = hold;
	
	UnuseMemory(pg_rec->wrap_area);
	UnuseMemory(pg_rec->containers);

	first_display = inval_container_change(pg_rec, container1);
	second_display = inval_container_change(pg_rec, container2);

	if (first_display < second_display)
		first_display = second_display;

	report_shape_change(pg_rec);

	if (draw_mode)
		pgUpdateText(pg_rec, NULL, first_display, pg_rec->t_length,
			MEM_NULL, NULL, draw_mode, TRUE);

	++pg_rec->change_ctr;

	UnuseMemory(pg);
}


/* pgGetContainerRefCon returns the app-specific refCon with container. */

PG_PASCAL (long) pgGetContainerRefCon (pg_ref pg, pg_short_t position)
{
	paige_rec_ptr		pg_rec;
	long				result;

	pg_rec = (paige_rec_ptr) UseMemory(pg);

#ifdef PG_DEBUG
	check_position_range(pg_rec, position);
#endif

	validate_container_refcons(pg_rec);
	GetMemoryRecord(pg_rec->containers, position - 1, &result);

	UnuseMemory(pg);
	
	return	result;
}


/* pgSetContainerRefCon sets the app-specific long associated with container. */

PG_PASCAL (void) pgSetContainerRefCon (pg_ref pg, pg_short_t position, long ref_con)
{
	paige_rec_ptr		pg_rec;
	long PG_FAR			*ref_longs;

	pg_rec = (paige_rec_ptr) UseMemory(pg);

#ifdef PG_DEBUG
	check_position_range(pg_rec, position);
#endif

	validate_container_refcons(pg_rec);
	
	ref_longs = (long PG_FAR*) UseMemoryRecord(pg_rec->containers, position - 1, 0, TRUE);
	*ref_longs = ref_con;
	UnuseMemory(pg_rec->containers);

	UnuseMemory(pg);
}


/* pgCharToContainer returns the container that contains text offset. Offset
can be CURRENT_POSITION. */

PG_PASCAL (pg_short_t) pgCharToContainer (pg_ref pg, long offset)
{
	paige_rec_ptr		pg_rec;
	text_block_ptr		block;
	point_start_ptr		starts;
	t_select			selection;
	pg_short_t			result;
	

	pg_rec = (paige_rec_ptr) UseMemory(pg);

	if (offset == CURRENT_POSITION)
		GetMemoryRecord(pg_rec->select, 0, &selection);
	else {
		
		pgFillBlock(&selection, sizeof(t_select), 0);
		selection.flags = SELECTION_DIRTY;
		selection.offset = offset;
	}
	
	if (selection.flags & SELECTION_DIRTY)
		pgCalcSelect(pg_rec, &selection);
	
	block = pgFindTextBlock(pg_rec, selection.offset, NULL, FALSE, TRUE);
	starts = (point_start_ptr) UseMemoryRecord(block->lines, selection.line, 0, TRUE);
	result = (pg_short_t)(starts->r_num + 1);
	UnuseMemory(block->lines);
	UnuseMemory(pg_rec->t_blocks);

	UnuseMemory(pg);
	
	return	result;
}


/* pgContainerToChar returns the first character in container position.
If -1 returns, container has no chars.  */

PG_PASCAL (size_t) pgContainerToChar (pg_ref pg, pg_short_t position)
{
	paige_rec_ptr				pg_rec;
	register text_block_ptr		block;
	register point_start_ptr	starts;
	size_t						result, qty, r_num;
	
	pg_rec = (paige_rec_ptr) UseMemory(pg);
	
	block = (text_block_ptr) UseMemory(pg_rec->t_blocks);
	r_num = (long)position - 1;
	result = -1;
	
	for (qty = GetMemorySize(pg_rec->t_blocks); qty; ++block, --qty) {
		
		if (block->flags & (NEEDS_CALC | LINES_PURGED))
			pgPaginateBlock(pg_rec, block, NULL, TRUE);
		
		if (block->end_start.r_num >= r_num)
			break;
	}
	
	if (qty) {
		
		starts = (point_start_ptr) UseMemory(block->lines);
		
		while (starts->flags != TERMINATOR_BITS) {

			if (starts->r_num == r_num) {
				
				result = block->begin;
				result += starts->offset;
				break;
			}
			
			++starts;
		}

		UnuseMemory(block->lines);
	}

	UnuseMemory(pg_rec->t_blocks);
	UnuseMemory(pg);
	
	return	result;
}


/****************************** Local Functions ***************************/


/* This function makes sure container refcon quantity matches with the number
of containers and, if not, fixes it. (This could happen by app starting with
"containers" but no refcons. */

static void validate_container_refcons (paige_rec_ptr pg)
{
	size_t		refcon_size, container_size;
	
	refcon_size = GetMemorySize(pg->containers);
	container_size = GetMemorySize(pg->wrap_area) - 1;
	
	if (refcon_size != container_size) {
		
		if (container_size > refcon_size) {
			
			AppendMemory(pg->containers, container_size - refcon_size, TRUE);
			UnuseMemory(pg->containers);
		}
		else
			SetMemorySize(pg->containers, container_size);
	}
}


/* This function locates the first text_block containing rect number
position. Or, if none are found (and all text is calculated), NULL is
returned. It also stops (and returns) first block that needs re-calc. */

static text_block_ptr find_first_block (paige_rec_ptr pg, pg_short_t position)
{
	register text_block_ptr		block;
	register size_t				num_blocks;
	short						r_num;
	
	num_blocks = GetMemorySize(pg->t_blocks);
	block = (text_block_ptr) UseMemory(pg->t_blocks);
	r_num = position - 1;
	
	while (num_blocks) {
		
		if ((block->flags & NEEDS_CALC) || (block->end_start.r_num >= r_num))
			return	block;
		
		++block;
		--num_blocks;
	}
	
	UnuseMemory(pg->t_blocks);
	
	return	NULL;
}


/* This invalidates all text starting at container position. It also returns
the best place to start displaying.  */

static long inval_container_change (paige_rec_ptr pg, pg_short_t position)
{
	text_block_ptr			block;
	
	pg->port.clip_info.change_flags |= CLIP_PAGE_CHANGED;

	if (position == 1) {
	
		pgInvalSelect(pg->myself, 0, pg->t_length);
		
		return	0;
	}
	else
	if (block = find_first_block(pg, (pg_short_t)(position - 1))) {
		long		result;
		
		result = block->begin;

		pgInvalSelect(pg->myself, result, pg->t_length);
		UnuseMemory(pg->t_blocks);
		
		return	result;
	}
	
	return	pg->t_length;
}


/* This function "reports" that the page_area has changed, first by re-building
the bounds rect in the page shape, then by telling machinespecific code that
clip regions might be different. */

static void report_shape_change (paige_rec_ptr pg)
{
	pgResetBounds(pg->wrap_area);
}


#ifdef PG_DEBUG

/* Checks for valid exclusion number if debug mode. */

static void check_position_range (paige_rec_ptr pg, pg_short_t position)
{
	short			check_sign;
	
	check_sign = position;
	
	if ( (check_sign < 1) || (position > (pg_short_t)GetMemorySize(pg->wrap_area)) )
		pgFailure (pg->globals->mem_globals, BAD_REFCON_ID_ERROR, check_sign);
}

#endif

