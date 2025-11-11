/* This file contains general utility functions for Paige. We realize some
of these functions are re-inventions of the wheel since they commonly exist
in some standard C libraries. However, for true portability we cannot assume
any of those libraries exist with this code.   */

/* Updated by TR Shaw, OITC, Inc. 20 Apr for C/C++/Pascal linkage and for inlines */

#include "Paige.h"

#ifdef MAC_PLATFORM
#pragma segment pgbasic3
#endif

#include "machine.h"
#include "pgUtils.h"
#include "pgDefStl.h"
#include "pgDefPar.h"
#include "pgText.h"
#include "pgShapes.h"
#include "pgEdit.h"


/* pgUniqueID returns an ID number unique to pg (won't be used anywhere else). */

PG_PASCAL (long) pgUniqueID (pg_ref pg)
{
	paige_rec_ptr		pg_rec;
	pg_ref				use_pg;
	long				result;
	
	use_pg = pg;
	pg_rec = pgUseSharedPg(&use_pg);
	
	++pg_rec->next_id;
	result = pg_rec->next_id;
	UnuseMemory(use_pg);

	return	result;
}


/* pgUseSharedPg is the same as UseMemory(pg) except the original pg_ref, if shared,
is used. */

PG_PASCAL (paige_rec_ptr) pgUseSharedPg (pg_ref PG_FAR *pg)
{
	paige_rec_ptr		pg_rec;
	pg_ref				shared_pg;

	pg_rec = (paige_rec_ptr) UseMemory(*pg);
	
	while (shared_pg = pg_rec->shared_pg) {
		
		UnuseMemory(*pg);
		*pg = shared_pg;
		pg_rec = (paige_rec_ptr) UseMemory(shared_pg);
	}
	
	return		pg_rec;
}


/* pgAreaBounds returns the bounding rectangle(s) for page area and vis area.
Either can be NULL.  */

PG_PASCAL (void) pgAreaBounds (pg_ref pg, rectangle_ptr page_bounds,
		rectangle_ptr vis_bounds)
{
	paige_rec_ptr		pg_rec;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	
	if (page_bounds)
		pgShapeBounds(pg_rec->wrap_area, page_bounds);
	if (vis_bounds)
		pgShapeBounds(pg_rec->vis_area, vis_bounds);

	UnuseMemory(pg);
}


/* pgSetAreaBounds sets page area and/or vis area to new rectangles. This is
only usable if shapes are a single rect. */

PG_PASCAL (void) pgSetAreaBounds (pg_ref pg, const rectangle_ptr page_bounds,
		const rectangle_ptr vis_bounds)
{
	paige_rec_ptr		pg_rec;
	pg_boolean			requires_pagination;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	requires_pagination = FALSE;

	if (page_bounds) {
		rectangle		current_page;
		
		pgShapeBounds(pg_rec->wrap_area, &current_page);

		requires_pagination = (pg_boolean)((current_page.top_left.h != page_bounds->top_left.h)
			|| (current_page.bot_right.h != page_bounds->bot_right.h)
			|| (current_page.top_left.v != page_bounds->top_left.v));
		if (!pgEqualStruct(&current_page, page_bounds, sizeof(rectangle)))
			pg_rec->port.clip_info.change_flags |= CLIP_PAGE_CHANGED;
			
		pgSetShapeRect(pg_rec->wrap_area, page_bounds);
	}

	if (vis_bounds) {
		rectangle		current_vis;
		
		pgShapeBounds(pg_rec->vis_area, &current_vis);

		if (!pgEqualStruct(&current_vis, vis_bounds, sizeof(rectangle)))
			pg_rec->port.clip_info.change_flags |= CLIP_VIS_CHANGED;

		pgSetShapeRect(pg_rec->vis_area, vis_bounds);
		pg_rec->base_vis_origin = vis_bounds->top_left;
	}
	
	pgResetDocBounds(pg_rec);

	if (!(pg_rec->flags & NO_EDIT_BIT))
    	pg_rec->flags |= INVALID_CURSOR_BIT;

	if (requires_pagination)
		pgInvalSelect(pg, 0, pg_rec->t_length);

	++pg_rec->change_ctr;

	UnuseMemory(pg);
}


/* pgWindowOriginChanged gets called by app when it changes the window origin, AND it wants
to keep the same effective vis area as before. If original_origin is NULL then the original
origin is assumed to be 0, 0.  If new_origin is NULL then it is assumed to be the current
scroll position of pg.  */

PG_PASCAL (void) pgWindowOriginChanged (pg_ref pg, const co_ordinate_ptr original_origin,
		co_ordinate_ptr new_origin)
{
	paige_rec_ptr			pg_rec;
	register rectangle_ptr	vis_ptr;
	co_ordinate				starting_origin, use_origin, new_position;
	long					offset_h, offset_v;
	pg_short_t				shape_qty;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	
	if (original_origin)
		starting_origin = *original_origin;
	else
		starting_origin.h = starting_origin.v = 0;
	
	if (new_origin)
		use_origin = *new_origin;
	else
		use_origin = pg_rec->logical_scroll_pos;

	new_position.h = pg_rec->base_vis_origin.h + (use_origin.h - starting_origin.h);
	new_position.v = pg_rec->base_vis_origin.v + (use_origin.v - starting_origin.v);
	vis_ptr = (rectangle_ptr) UseMemory(pg_rec->vis_area);
	offset_h = new_position.h - vis_ptr->top_left.h;
	offset_v = new_position.v - vis_ptr->top_left.v;

	if (offset_h || offset_v) {
		
		for (shape_qty = (pg_short_t)GetMemorySize(pg_rec->vis_area); shape_qty; ++vis_ptr, --shape_qty)
			pgOffsetRect(vis_ptr, offset_h, offset_v);
	}

	UnuseMemory(pg_rec->vis_area);
	UnuseMemory(pg);
}



/* pgLineBounds returns the bounding rect for the line beginning at first_start. */

PG_PASCAL (void) pgLineBounds (point_start_ptr first_start, rectangle_ptr bounds)
{
	register point_start_ptr		starts;
	
	pgFillBlock(bounds, sizeof(rectangle), 0);
	starts = first_start;
	
	for (;;) {
		
		pgUnionRect(&starts->bounds, bounds, bounds);
		
		if (starts->flags & LINE_BREAK_BIT)
			break;
		
		++starts;
	}
}


/* pgResetDocBounds changes the bounding box in pg for document (which is
pg->doc_bounds).  Typically, pgResetDocBounds is called when the wrap area has changed.     */

PG_PASCAL (void) pgResetDocBounds (paige_rec_ptr pg)
{
	register rectangle_ptr			wrap_stuff;
	
	wrap_stuff = (rectangle_ptr) UseMemory(pg->wrap_area);
	pg->doc_bounds.top_left = wrap_stuff->top_left;
	pg->doc_bounds.bot_right.h = wrap_stuff->bot_right.h;

	UnuseMemory(pg->wrap_area);
}


/* pgFixOffset is called internally by Paige and returns the "real" offset
based on wanted_offset.  */

PG_PASCAL (size_t) pgFixOffset (paige_rec_ptr pg, size_t wanted_offset)
{
	if (wanted_offset == CURRENT_POSITION)
		return	pgCurrentInsertion(pg);
	
	if (wanted_offset < 0)
		return	0;
	if (wanted_offset > pg->t_length)
		return	pg->t_length;
	
	return	wanted_offset;
}


/* pgLongToShort returns a short value of a long by truncating its value if
the absolute value > 32K */

PG_PASCAL (short) pgLongToShort (long value)
{
	if (value < 0) {
		if (value < MAX_MINUS_INTEGER)
			return MAX_MINUS_INTEGER;
	}
	else
	if (value > MAX_INTEGER)
		return MAX_INTEGER;
	
	return ((short)value);
}


/* pgSwapLongs reverses lessor_val with greater_val if lessor_val > greater_val */

PG_PASCAL (void) pgSwapLongs (long PG_FAR *lessor_val, long PG_FAR *greater_val)
{
	if (*lessor_val > *greater_val) {
		long		swap;
		
		swap = *lessor_val;
		*lessor_val = *greater_val;
		*greater_val = swap;
	}
}


/* pgEmptyRect returns TRUE if the rectangle is empty (no dimension)  */

PG_PASCAL (pg_boolean) pgEmptyRect (rectangle_ptr rect)
{
	register long PG_FAR		*src;
	register long PG_FAR		*dest;
	
	src = dest = (long PG_FAR *) rect;
	dest += 2;
	if (*src++ == *dest++)
		return	TRUE;

	return (*src == *dest);
}


/* pgTrulyEmptyRect returns TRUE if the rectangle is empty (no dimension) on all four
sides (pgEmptyRect returns TRUE if either side is empty). */

PG_PASCAL (pg_boolean) pgTrulyEmptyRect (rectangle_ptr rect)
{
	register long PG_FAR		*src;
	register long PG_FAR		*dest;
	
	src = dest = (long PG_FAR *) rect;
	dest += 2;

	return ((*src++ == *dest++) && (*src == *dest));
}


/* pgOffsetRect moves a rectangle h and v amounts */

PG_PASCAL (void) pgOffsetRect (rectangle_ptr rect, long h, long v)
{
	register rectangle_ptr		rect_ptr;
	register long				shift_value;

	rect_ptr = rect;
	if (shift_value = h) {
		rect_ptr->top_left.h += shift_value;
		rect_ptr->bot_right.h += shift_value;
	}
	
	if (shift_value = v) {
		rect_ptr->top_left.v += shift_value;
		rect_ptr->bot_right.v += shift_value;
	}
}


/* pgInsetRect insets a rectangle h and v amounts. Negative amounts inset the
edges outwards.  */

PG_PASCAL (void) pgInsetRect (rectangle_ptr rect, long h, long v)
{
	register rectangle_ptr		rect_ptr;
	register long				inset_value;

	rect_ptr = rect;

	if (inset_value = h) {
		rect_ptr->top_left.h += inset_value;
		rect_ptr->bot_right.h -= inset_value;
	}
	
	if (inset_value = v) {
		rect_ptr->top_left.v += inset_value;
		rect_ptr->bot_right.v -= inset_value;
	}
}


/* pgSectRect checks for an intersection between r1 and r2. If r3 is set to the
intersection if non-NULL;  the function returns TRUE if an intersection exists,
otherwise FALSE is returned and r3 is unchanged. Added 3/26/94, r2 can be
NULL for convenience (in which case TRUE is returned, i.e. it is considered
to intersect infinity. */

PG_PASCAL (pg_boolean) pgSectRect (rectangle_ptr r1, rectangle_ptr r2, rectangle_ptr r3)
{
	register rectangle_ptr		src;
	register rectangle_ptr		target;
	register rectangle_ptr		output;

	if (!r2) {
		
		if (r3 && r1)
			*r3 = *r1;

		return	TRUE;
	}

	if (!r1) {

		if (r3 && r2)
			*r3 = *r2;

		return	TRUE;
	}

	src = r1;
	target = r2;
	
	if (src->top_left.v >= target->bot_right.v)
		return	FALSE;
	if (target->top_left.v >= src->bot_right.v)
		return	FALSE;
	if (src->top_left.h >= target->bot_right.h)
		return	FALSE;
	if (target->top_left.h >= src->bot_right.h)
		return	FALSE;
		
	if (output = r3) {
		
		output->top_left.h = pgMax(src->top_left.h, target->top_left.h);
		output->top_left.v = pgMax(src->top_left.v, target->top_left.v);
		output->bot_right.h = pgMin(src->bot_right.h, target->bot_right.h);
		output->bot_right.v = pgMin(src->bot_right.v, target->bot_right.v);
	}
	
	return	TRUE;
}


/* pgUnionRect returns the union in r3 of r1 and r2. */

PG_PASCAL (void) pgUnionRect (rectangle_ptr r1, rectangle_ptr r2, rectangle_ptr r3)
{
	register rectangle_ptr		src;
	register rectangle_ptr		target;
	rectangle					result;
	
	target = r2;
	src = r1;
	result = *target;

	if (pgTrulyEmptyRect(src))
		*r3 = *target;
	else
	if (pgEmptyRect(target))
		*r3 = *src;
	else {
		if (src->top_left.h < target->top_left.h)
			result.top_left.h = src->top_left.h;
	
		if (src->top_left.v < target->top_left.v)
			result.top_left.v = src->top_left.v;
		
		if (src->bot_right.h > target->bot_right.h)
			result.bot_right.h = src->bot_right.h;
		
		if (src->bot_right.v > target->bot_right.v)
			result.bot_right.v = src->bot_right.v;
		
		*r3 = result;
	}
}


/* pgTrueUnionRect is identical to pgUnionRect except the sides are united
unconditionally, even if two opposite sides are "empty" yet the whole rect is
not empty. */

PG_PASCAL (void) pgTrueUnionRect (rectangle_ptr r1, rectangle_ptr r2, rectangle_ptr r3)
{
	register rectangle_ptr		src;
	register rectangle_ptr		target;
	rectangle					result;
	
	target = r2;
	src = r1;
	result = *target;

	if (pgTrulyEmptyRect(src))
		*r3 = *target;
	else
	if (pgTrulyEmptyRect(target))
		*r3 = *src;
	else {
		if (src->top_left.h < target->top_left.h)
			result.top_left.h = src->top_left.h;
	
		if (src->top_left.v < target->top_left.v)
			result.top_left.v = src->top_left.v;
		
		if (src->bot_right.h > target->bot_right.h)
			result.bot_right.h = src->bot_right.h;
		
		if (src->bot_right.v > target->bot_right.v)
			result.bot_right.v = src->bot_right.v;
		
		*r3 = result;
	}
}


/* pgSectOrTouchRect is identical to pgSectRect except TRUE is returned if
opposing left and right sides are touching as well. "Opposing sides" would be
r1-left against r2-right or visa versa. Note: r2 can't be NULL in this case. */

PG_PASCAL (pg_boolean) pgSectOrTouchRect (rectangle_ptr r1, rectangle_ptr r2, rectangle_ptr r3)
{
	register rectangle_ptr		src;
	register rectangle_ptr		target;
	register rectangle_ptr		output;

	src = r1;
	target = r2;
	
	if (src->top_left.v >= target->bot_right.v)
		return	FALSE;
	if (target->top_left.v >= src->bot_right.v)
		return	FALSE;
	if (src->top_left.h > target->bot_right.h)
		return	FALSE;
	if (target->top_left.h > src->bot_right.h)
		return	FALSE;
		
	if (output = r3) {

		output->top_left.v = pgMax(src->top_left.v, target->top_left.v);
		output->bot_right.v = pgMin(src->bot_right.v, target->bot_right.v);

		if (pgSectRect(src, target, NULL)) {
		
			output->top_left.h = pgMax(src->top_left.h, target->top_left.h);
			output->bot_right.h = pgMin(src->bot_right.h, target->bot_right.h);
		}
		else {
			
			if (src->top_left.h == target->top_left.h)
				output->top_left.h = src->top_left.h;
			else
				output->top_left.h = src->bot_right.h;
			
			output->bot_right.h = output->top_left.h;
		}
	}
	
	return	TRUE;
}


/* pgPtInRect returns "TRUE" if the given co_ordinate falls somewhere within
the specified rectangle */

PG_PASCAL (pg_boolean) pgPtInRect (co_ordinate_ptr point, rectangle_ptr rect)
{
	register co_ordinate_ptr		the_pt;
	register rectangle_ptr			the_rect;
	
	the_pt = point;
	the_rect = rect;
	
	if (the_pt->h < the_rect->top_left.h)
		return FALSE;
	if (the_pt->h >= the_rect->bot_right.h)
		return FALSE;
	if (the_pt->v < the_rect->top_left.v)
		return FALSE;
	if (the_pt->v >= the_rect->bot_right.v)
		return FALSE;
	
	return	TRUE;
}


/* pgPtInRectInset is same as pgPtInRect except inset_extra is applied to the
rectangle before checking the point. */

PG_PASCAL (pg_boolean) pgPtInRectInset (co_ordinate_ptr point, rectangle_ptr rect,
			co_ordinate_ptr inset_extra, pg_scale_ptr scaling)
{
	rectangle		hold_r;
	
	if (!inset_extra && !scaling)
		return	pgPtInRect(point, rect);
	
	hold_r = *rect;

	if (scaling)
		pgScaleRect(scaling, NULL, &hold_r);
	
	if (inset_extra)
		pgInsetRect(&hold_r, inset_extra->h, inset_extra->v);

	return	pgPtInRect(point, &hold_r);
}



/* pgDistanceToRect returns the shortest <absolute value> distance, both h and v,
from any side of the specified rectangle. This is used to see how close a rectangle
is to a point. */

PG_PASCAL (void) pgDistanceToRect (co_ordinate_ptr point, rectangle_ptr rect,
			co_ordinate_ptr distance)
{
	register co_ordinate_ptr		the_pt;
	register rectangle_ptr			the_rect;
	register long					h, v;
	
	the_pt = point;
	the_rect = rect;
	
	h = pgMin(pgAbsoluteValue(the_rect->top_left.h - the_pt->h),
					pgAbsoluteValue(the_rect->bot_right.h - the_pt->h));
	v = pgMin(pgAbsoluteValue(the_rect->top_left.v - the_pt->v),
					pgAbsoluteValue(the_rect->bot_right.v - the_pt->v));
	
	distance->h = h;
	distance->v = v;
}


/* pgCoversRect returns TRUE if r1 completely covers up r2 (overlaps all 4 sides). */

PG_PASCAL (pg_boolean) pgCoversRect (rectangle_ptr r1, rectangle_ptr r2)
{
	register rectangle_ptr		src, target;
	
	src = r1;
	target = r2;
	
	if (src->top_left.h > target->top_left.h)
		return	FALSE;
	if (src->top_left.v > target->top_left.v)
		return	FALSE;
	if (src->bot_right.h < target->bot_right.h)
		return	FALSE;
	if (src->bot_right.v < target->bot_right.v)
		return	FALSE;
	
	return	TRUE;
}


/* pgEqualStruct compares one struct to another, returns TRUE if they match
exactly.  Note that structs must be an even byte count for this to work! */

PG_PASCAL (pg_boolean) pgEqualStruct (void PG_FAR *rec1, void PG_FAR *rec2, size_t length)
{
	register short PG_FAR		*ptr1;
	register short PG_FAR		*ptr2;
	register size_t				ctr;
	
	for (ptr1 = (short *) rec1, ptr2 = (short *) rec2, ctr = length / sizeof(short); ctr; --ctr)
		if (*ptr1++ != *ptr2++)
			return	FALSE;
		
	return	TRUE;
}

/* pgEqualStructMasked is the same as pgEqualStruct except only non-zero values in mask
are compared. */

PG_PASCAL (pg_boolean) pgEqualStructMasked (void PG_FAR *rec1, void PG_FAR *rec2,
			void PG_FAR *mask, long length)
{
	register short PG_FAR		*ptr1;
	register short PG_FAR		*ptr2;
	register short PG_FAR		*ptr3;
	register long				ctr;
	
	ptr3 = (short *) mask;

	for (ptr1 = (short *) rec1, ptr2 = (short *) rec2, ctr = length / sizeof(short); ctr; --ctr) {
		
		if (*ptr3++ != 0) {
		
			if (*ptr1++ != *ptr2++)
				return	FALSE;
		}
		else {
			
			ptr1 += 1;
			ptr2 += 1;
		}
	}

	return	TRUE;
}


/* pgZeroStruct returns TRUE if *rec has nothing but zeros. */

PG_PASCAL (pg_boolean) pgZeroStruct (void PG_FAR *rec, pg_short_t length)
{
	register short PG_FAR		*ptr;
	register short				ctr;
	
	for (ptr = (short *) rec, ctr = length / sizeof(short); ctr; --ctr)
		if (*ptr++ != 0)
			return	FALSE;

	return	TRUE;
}


/* pgEraseContainerArea must fill the page_area of pg with the color of the current
window or port or whatever. If offset_extra is non-NULL, the shape if offset by
offset_extra. If top_start is non-NULL the erasing begins with that section of
the shape otherwise erasing occurs at the top;  if bottom_end is non_NULL
erasing terminates at that section, otherwise erasing goes to the bottom.
The shape can also be scaled before erasing: if scale_factor is non-NULL the
target rect is scaled.  */


PG_PASCAL (void) pgEraseContainerArea (paige_rec_ptr pg, pg_scale_ptr scale_factor,
		co_ordinate_ptr offset_extra, shape_section_ptr top_start,
		shape_section_ptr bottom_end)
{
	pgm_globals_ptr				mem_globals;
	rectangle					target, vis_bounds;
	co_ordinate					repeat_offset;
	register shape_ptr			rects;
	size_t						r_ctr, repeat_ctr, start_rect, end_rect;
	size_t						physical_rects, max_rects, container_proc_refcon;
	pg_boolean					called_clip, use_vis_rect;

	mem_globals = pg->globals->mem_globals;

	physical_rects = GetMemorySize(pg->wrap_area) - 1;
	rects = (shape_ptr) UseMemory(pg->wrap_area);
	++rects;

	container_proc_refcon = 0;
	called_clip = FALSE;
	r_ctr = repeat_ctr = 0;
	
	use_vis_rect = FALSE;
	
	if (top_start) {
	
		start_rect = r_ctr = repeat_ctr = top_start->r_num;

		if (start_rect < physical_rects)
			rects += start_rect;
		else {
			
			repeat_ctr = (start_rect % physical_rects);
			rects += repeat_ctr;
		}
	}
	else
		start_rect = -1;
	
	if (pg->doc_info.attributes & V_REPEAT_BIT)
		max_rects = start_rect + physical_rects + 1;
	else
		max_rects = physical_rects;

	if (bottom_end)
		end_rect = bottom_end->r_num;
	else
		end_rect = max_rects + 1;
	
	pgShapeBounds(pg->vis_area, &vis_bounds);

	if (pg->doc_info.attributes & COLOR_VIS_BIT)
		use_vis_rect = TRUE;

	while (r_ctr < max_rects) {
		
		if (r_ctr >= start_rect) {
			
			if (use_vis_rect)
				target = vis_bounds;
			else {
				
				target = *rects;
				
				if (!(pg->doc_info.attributes & BOTTOM_FIXED_BIT)) {
					
					pgComputeDocHeight(pg, FALSE);
					target.bot_right.v = pg->doc_bounds.bot_right.v;

					if (target.bot_right.v < (vis_bounds.bot_right.v + 8))
						target.bot_right.v = vis_bounds.bot_right.v + 8;
				}
				else {

					pgGetWrapRect(pg, r_ctr, &repeat_offset);
					pgOffsetRect(&target, repeat_offset.h, repeat_offset.v);
				}
			}

			if (r_ctr == start_rect)
				target.top_left.v = top_start->v_position;	

			if (r_ctr == end_rect)
				target.bot_right.v = bottom_end->v_position;
			
			if (target.bot_right.v > target.top_left.v) {
			
				pg->procs.container_proc(pg, (short)(r_ctr + 1), &target, scale_factor,
						offset_extra, clip_container_verb, &container_proc_refcon);
				pg->procs.container_proc(pg, (short)(r_ctr + 1), &target, scale_factor,
						offset_extra, erase_rect_verb, NULL);
				
				called_clip = TRUE;
			}
		}

		if (r_ctr == end_rect)
			break;

		++r_ctr;
		++repeat_ctr;
		++rects;
		
		if (repeat_ctr >= physical_rects) {
		
			rects = (shape_ptr) UseMemoryRecord(pg->wrap_area, 1, 0, FALSE);
			repeat_ctr = 0;
		}
	}

	if (called_clip)
		pg->procs.container_proc(pg, (pg_short_t)r_ctr, &target, NULL,
				offset_extra, unclip_container_verb, &container_proc_refcon);

	UnuseMemory(pg->wrap_area);
}


/* Gets a pg_ref's specific id for its subserviant memory references */

PG_PASCAL (long) pgGetPaigeRefID(pg_ref pg)
{
	long mem_id;
 
    mem_id = ((paige_rec_ptr)UseMemory(pg))->mem_id;
    UnuseMemory(pg);

    return (mem_id);
}


/* pgScaleLong reduces or enlarges value by scale_factor amount. The
scale factor is hiword/loword fraction.  If scale_factor is negative, the value
is "upscaled" (opposite of scaled).  */


PG_PASCAL (void) pgScaleLong (long scale_factor, long origin, long PG_FAR *value)
{
	long		origin_base;

	if (!scale_factor)
		return;
	
	if (*value < 0)
		origin_base = -origin;
	else
		origin_base = origin;
	
	*value -= origin_base;

	if (scale_factor < 0) {
		long		scale;
		
		scale = -scale_factor;
		
		*value = pgDivideFixed(*value, pgFixedRatio(pgHiWord(scale), pgLoWord(scale)));
	}
	else {
		pg_fixed	param1;
		long		original_value;

		original_value = *value;
		param1 = pgFixedRatio(pgHiWord(scale_factor), pgLoWord(scale_factor));
		*value = pgMultiplyFixed(param1, *value);
    }
	
	*value += origin_base;
}

PG_PASCAL (void) pgScaleShort (long scale_factor, long origin, short PG_FAR *value)
{
	long		real_value;
	
	real_value = *value;
	pgScaleLong(scale_factor, origin, &real_value);

	*value = (short)real_value;
}


/* pgScaleVirtualRect scales a rect by a numerator and denominator. */

PG_PASCAL (void) pgScaleVirtualRect (paige_rec_ptr pg, rectangle_ptr rect, short old_res, short new_res)
{
	pg_scale_factor		scale;
	
	scale.scale = old_res;
	scale.scale <<= 16;
	scale.scale |= new_res;
	scale.origin = pg->port.origin;
	
	pgScaleRect(&scale, NULL, rect);
}


/* pgScalePt reduces or enlarges pt by scale_factor amount. The
scale factor is hiword/loword fraction.  If scale_factor is negative, the value
is "upscaled" (opposite of scaled).  */


PG_PASCAL (void) pgScalePt (const pg_scale_ptr scale_factor, const co_ordinate_ptr amount_offset, co_ordinate_ptr pt)
{
	if (!scale_factor->scale)
		return;
	
	if (amount_offset)
		pgSubPt(amount_offset, pt);
	
	pgScaleLong(scale_factor->scale, scale_factor->origin.h, &pt->h);
	pgScaleLong(scale_factor->scale, scale_factor->origin.v, &pt->v);
	
	if (amount_offset) {
		co_ordinate		compensate;
		
		compensate = *amount_offset;
		pgScaleLong(scale_factor->scale, 0, &compensate.h);
		pgScaleLong(scale_factor->scale, 0, &compensate.v);
		pgAddPt(&compensate, pt);
	}
}


/* pgScaleRect reduces or enlarges rect by scale_factor amount. The
scale factor is hiword/loword fraction.  If scale_factor is negative, the value
is "upscaled" (opposite of scaled).  */

PG_PASCAL (void) pgScaleRect (const pg_scale_ptr scale_factor, const co_ordinate_ptr amount_offset,
		rectangle_ptr rect)
{
	if (!scale_factor->scale)
		return;

	pgScalePt(scale_factor, amount_offset, &rect->top_left);
	pgScalePt(scale_factor, amount_offset, &rect->bot_right);
}


/* pgScaleRectToRect offsets src_rect by offset_extra amount (if non-NULL),
then scales it scale_factor amount and places scaled result in target_rect.
Change 1/14/94, scale_factor can be NULL in which case rect transfers to
target_rect and is offset.  */

PG_PASCAL (void) pgScaleRectToRect (const pg_scale_ptr scale_factor, const rectangle_ptr src_rect,
			rectangle_ptr target_rect, const co_ordinate_ptr offset_extra)
{
	pgBlockMove(src_rect, target_rect, sizeof(rectangle));
	
	if (offset_extra)
		pgOffsetRect(target_rect, offset_extra->h, offset_extra->v);
	
	if (scale_factor)
		pgScaleRect(scale_factor, offset_extra, target_rect);
}



/* pgCallTextHook gets called to invoke the delete_text style hook for the
specified range, or the copy_text hook or the activate_proc.
Text will be deleted soon after this call if deleting; if copying,
the text has already been copied. If activate, the text has not yet been
inverted for activate, or has already been un-inverted for deactivate.
The front_back_state through show_hilite params are used only for calling
activate_proc. */

PG_PASCAL (void) pgCallTextHook (paige_rec_ptr pg, paige_rec_ptr src_option,
		short reason_verb, long starting_offset, long length, short verb,
		short front_back_state, short perm_state, pg_boolean show_hilite)
{
	register style_run_ptr			run;
	register style_info_ptr			style_base;
	register size_t					text_size, caller_size, beginning_offset;
	select_pair						active_range;
	style_info_ptr					style_used;
	pg_char_ptr						text;
	text_block_ptr					block;
	size_t							ending_offset, block_end_size;
	size_t							offset_to_find;
	long							class_bits_check;

	run = pgFindStyleRun(pg, starting_offset, NULL);
	style_base = (style_info_ptr) UseMemory(pg->t_formats);
	beginning_offset = starting_offset;
	ending_offset = beginning_offset + length;
	
	if (verb == call_for_activate)
		class_bits_check = ACTIVATE_ENABLE_BIT;
	else
		class_bits_check = REQUIRES_COPY_BIT;

	while (run->offset < ending_offset) {
	
		style_used = style_base + run->style_item;
		if (style_used->class_bits & class_bits_check) {

			if ((text_size = run[1].offset) > pg->t_length)
				text_size = pg->t_length;
			text_size -= beginning_offset;
			
			if (verb == call_for_activate)
				offset_to_find = run->offset;
			else
				offset_to_find = beginning_offset;
				
			block = pgFindTextBlock(pg, offset_to_find, NULL, FALSE, TRUE);
			block_end_size = block->end - offset_to_find;
	
			if ((caller_size = text_size) > block_end_size)
				caller_size = block_end_size;
	
			text = (pg_char_ptr) UseMemory(block->text);
			text += (offset_to_find - block->begin);
			
			switch (verb) {
				
				case call_for_copy:
					style_used->procs.copy_text(src_option, pg, reason_verb,
							style_used, run->offset, beginning_offset, text,
							caller_size);
					
					break;
					
				case call_for_delete:
					style_used->procs.delete_text(pg, reason_verb, style_used,
						run->offset, beginning_offset, text, caller_size);
					
					break;
					
				case call_for_activate:

					active_range.begin = offset_to_find;
					active_range.end = run[1].offset;
					if (active_range.end > ending_offset)
						active_range.end = ending_offset;

					style_used->procs.activate_proc(pg, style_used, &active_range,
						text, front_back_state, perm_state, show_hilite);
					
					break;
			}
			
			UnuseMemory(block->text);
			UnuseMemory(pg->t_blocks);
		}

		++run;
		beginning_offset = run->offset;
	}
	
	UnuseMemory(pg->t_style_run);
	UnuseMemory(pg->t_formats);
}


/* pgCallContainerProc is a generic function used for calling container_proc
when the exact container rect is desired. The container_num parameter must be
zero-based (nut is passed to container_proc as 1-based). The offset_extra, verb,
scale_factor and extra_info parameters are simply passed to container_proc. */

PG_PASCAL (void) pgCallContainerProc (paige_rec_ptr pg, pg_short_t container_num,
		co_ordinate_ptr offset_extra, short verb, pg_scale_ptr scale_factor,
		void PG_FAR *extra_info)
{
	co_ordinate			repeat_offset;
	pg_short_t			real_container;
	
	real_container = pgGetWrapRect(pg, container_num, &repeat_offset);
	
	if (offset_extra)
		pgAddPt(offset_extra, &repeat_offset);

	pg->procs.container_proc(pg, (pg_short_t)(container_num + 1),
			(rectangle_ptr) UseMemoryRecord(pg->wrap_area, real_container + 1, 0, TRUE), scale_factor,
				&repeat_offset, verb, extra_info);

	UnuseMemory(pg->wrap_area);
}


/* pgWillDeleteFormats walks through a list of text, paragraph and info run formats and calls
the DELETE function for each  */

PG_PASCAL (void) pgWillDeleteFormats (paige_rec_ptr pg, pg_globals_ptr globals,
		short reason_verb, format_ref text_formats, par_ref par_formats)
{
	register style_info_ptr		styles;
	register par_info_ptr		pars;
	register pg_short_t			qty;

	if (qty = (pg_short_t)GetMemorySize(text_formats)) {
	
		for (styles = (style_info_ptr) UseMemory(text_formats); qty; ++styles, --qty)
			styles->procs.delete_style(pg, globals, reason_verb, text_formats, styles);
		UnuseMemory(text_formats);
	}

	if (qty = (pg_short_t)GetMemorySize(par_formats)) {
	
		for (pars = (par_info_ptr) UseMemory(par_formats); qty; ++pars, --qty)
			pars->procs.delete_par(pg, reason_verb, par_formats, pars);
		
		UnuseMemory(par_formats);
	}
}


/* pgComputeDocHeight computes the document height (highest point in doc) and
also the physical bottom (not necessarily the same if irregular shapes are in
place). If paginate is TRUE, all blocks are paginated if necessary.  */

PG_PASCAL (void) pgComputeDocHeight (paige_rec_ptr pg, pg_boolean paginate)
{
	register text_block_ptr				block;
	pg_short_t							num_blocks;
	pg_short_t							dimension;
	long								highest;

	block = (text_block_ptr) UseMemory(pg->t_blocks);
	num_blocks = (pg_short_t)GetMemorySize(pg->t_blocks);
	
	if (paginate)
		pgPaginateBlock(pg, &block[num_blocks - 1], NULL, FALSE);

	pg->doc_top = block->bounds.top_left.v;
	
	dimension = pgShapeDimension(pg->wrap_area);

	if (!(dimension & COMPLEX_DIMENSION)) {
		
		block += (num_blocks - 1);
		pg->doc_bounds.bot_right.v = pg->doc_bottom = block->bounds.bot_right.v;
	}
	else {
		
		highest = block->bounds.bot_right.v;
		while (num_blocks) {
			
			if (block->bounds.bot_right.v > highest)
				highest = block->bounds.bot_right.v;
			
			pg->doc_bottom = block->bounds.bot_right.v;

			++block;
			--num_blocks;
		}
		
		pg->doc_bounds.bot_right.v = highest;
	}

	UnuseMemory(pg->t_blocks);
	
	if (!(pg->doc_info.attributes & BOTTOM_FIXED_BIT)) {
		rectangle_ptr		wrap_bounds;
		long				previous_value;

		wrap_bounds = (rectangle_ptr) UseMemoryRecord(pg->wrap_area, GetMemorySize(pg->wrap_area) - 1, 0, TRUE);
		previous_value = wrap_bounds->bot_right.v;

		if ((wrap_bounds->bot_right.v = pg->doc_bottom) <=  wrap_bounds->top_left.v)
			wrap_bounds->bot_right.v = wrap_bounds->top_left.v + 1;

		if (previous_value != wrap_bounds->bot_right.v)
			pgResetBounds(pg->wrap_area);

		UnuseMemory(pg->wrap_area);
	}

	if (pg->flags & EX_DIMENSION_BIT) {
		rectangle		x_bounds;
		
		pgShapeBounds(pg->exclude_area, &x_bounds);
		pgUnionRect(&x_bounds, &pg->doc_bounds, &pg->doc_bounds);
	}
}


/* pgScaleDocument changes everything in pg so it is rendered in a new resolution. */

PG_PASCAL (void) pgScaleDocument (paige_rec_ptr pg, short old_resolution, short new_resolution,
			pg_boolean scale_page_rect)
{
	style_info_ptr	styles;
	par_info_ptr	pars;
	text_block_ptr	blocks;
	long			qty;

	if (old_resolution == new_resolution
		|| !old_resolution || !new_resolution)
		return;

	if (scale_page_rect)
		pgScaleShape(pg, pg->wrap_area, old_resolution, new_resolution);

	pgScaleShape(pg, pg->exclude_area, old_resolution, new_resolution);

	styles = (style_info_ptr) UseMemory(pg->t_formats);
	
	for (qty = GetMemorySize(pg->t_formats); qty; ++styles, --qty)
		pgScaleStyleInfo(pg, styles, old_resolution, new_resolution);
	
	UnuseMemory(pg->t_formats);

	pars = (par_info_ptr) UseMemory(pg->par_formats);
	
	for (qty = GetMemorySize(pg->par_formats); qty; ++pars, --qty)
		pgScaleParInfo(pg, pars, old_resolution, new_resolution);
	
	UnuseMemory(pg->par_formats);

	blocks = (text_block_ptr) UseMemory(pg->t_blocks);
	
	for (qty = GetMemorySize(pg->t_blocks); qty; ++blocks, --qty)
		blocks->flags |= NEEDS_CALC;

	UnuseMemory(pg->t_blocks);
	
	pgResetDocBounds(pg);
}


/* pgGetOSConstant returns the OS constant based on the platform constant. */

PG_PASCAL (long) pgGetOSConstant (long platform)
{
	long		os;

	switch (platform & GENERAL_PLATFORM_MASK) {
		
		case GENERAL_MACINTOSH:
			os = MACINTOSH_OS;
			break;
		
		case GENERAL_WINDOWS:
			os = WINDOWS_OS;
			break;
		
		case GENERAL_UNIX:
			os = UNIX_OS;
			break;
		
		default:
			os = CURRENT_OS;
			break;
	}
	
	return	os;
}


#ifdef NO_C_INLINE

PG_PASCAL (short) pgLoWord(long value)
{
	return (short)value;
}

PG_PASCAL (short) pgHiWord(long value)
{
	return (short)((long) value >> 16);
}

/* pgPushMemoryID sets the current memory ID to be that of the paige object (so
all new memory ref's created assume the same ID).  */

PG_PASCAL (void) pgPushMemoryID (paige_rec_ptr pg)
{	
	pg->globals->mem_globals->current_id = pg->mem_id;
}


/* pgPopMemoryID restores the current memory ID to be the next unique memory ID.
This gets called sometime after pgPushMemoryID was done.  */

PG_PASCAL (void) pgPopMemoryID (paige_rec_ptr pg)
{
	pg->globals->mem_globals->current_id = pg->globals->mem_globals->next_mem_id;
}

PG_PASCAL (void) pgAddPt (co_ordinate_ptr pt1, co_ordinate_ptr pt2)
{
	pt2->h += pt1->h;
	pt2->v += pt1->v;
}


/* pgSubPt subtracts pt1 from pt2, result in pt2 */

PG_PASCAL (void) pgSubPt (co_ordinate_ptr pt1, co_ordinate_ptr pt2)
{
	pt2->h -= pt1->h;
	pt2->v -= pt1->v;
}


/* pgNegatePt reverses polarity of point.  */

PG_PASCAL (void) pgNegatePt (co_ordinate_ptr point)
{
	point->h = -point->h;
	point->v = -point->v;
}

#endif

#ifndef C_LIBRARY

/* pgAbsoluteValue returns an absolute value */

PG_PASCAL (long) pgAbsoluteValue (long value)
{
	if (value >= 0)
		return	value;
	
	return	-value;
}

#endif


/* pgGetType returns the current type of document the paige object is */

PG_PASCAL (long) pgGetType (pg_ref pg)
{
	paige_rec_ptr		pg_rec;
	long				result;
	
	pg_rec = (paige_rec_ptr) UseMemory(pg);
	result = pg_rec->pg_type;
	UnuseMemory(pg);
	
	return	result;
}


/* pgSetType sets the type of document the paige object is */

PG_PASCAL (void) pgSetType (pg_ref pg, long new_type)
{
	paige_rec_ptr		pg_rec;
	
	pg_rec = (paige_rec_ptr) UseMemory(pg);
	pg_rec->pg_type = new_type;
	UnuseMemory(pg);
}


#ifdef NO_C_INLINE

/* This returns the lessor of two values */

PG_PASCAL (long) pgMin (long value1, long value2)
{
	if (value1 < value2)
		return	value1;
	
	return	value2;
}


/* This returns the greater of two values */

PG_PASCAL (long) pgMax (long value1, long value2)
{
	if (value1 > value2)
		return	value1;

	return	value2;
}


#endif

/***************************  Local Functions  ***********************/




