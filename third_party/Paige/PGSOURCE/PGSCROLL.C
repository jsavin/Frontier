/* This section handles scrolling and related items   */

#include "Paige.h"

#ifdef MAC_PLATFORM
#pragma segment pgscroll
#endif

#include "machine.h"
#include "pgRegion.h"
#include "defprocs.h"
#include "pgShapes.h"
#include "pgUtils.h"
#include "pgText.h"
#include "pgEdit.h"
#include "pgSelect.h"
#include "pgSubref.h"


#define MAX_LONG_VALUE	16367		/* Maximum "long" I want to scroll  */
#define DEF_H_UNIT	8				/* Divisor or visual scrolling area */
#define DEF_UNIT_SCROLL	16			/* Default line unit scroll */

static void scroll_pixels (paige_rec_ptr pg, long PG_FAR *pix_h, long PG_FAR *pix_v,
			pg_boolean scroll_max_h, pg_boolean scroll_max_v, pg_boolean final_call,
			short draw_mode);
static void do_physical_scroll (paige_rec_ptr pg, pg_boolean final_call, short draw_mode);
static long div_factor (long scroll_value);
static void maximum_pixel_scroll (paige_rec_ptr pg, long PG_FAR *h, long PG_FAR *v);
static long h_unit_to_pixels (paige_rec_ptr pg, short unit_verb);
static long v_unit_to_pixels (paige_rec_ptr pg, short unit_verb);
static long top_line_distance (paige_rec_ptr pg);
static long bottom_line_distance (paige_rec_ptr pg, pg_boolean best_guess);
static long closest_start_sect (paige_rec_ptr pg, rectangle_ptr vis, long side_to_use,
			short want_inside);
static long multiply_factor (short value, short factor);
static long get_scroll_align (long scroll_align);
static void align_movement (long PG_FAR *movement, long alignment);
static short fix_paginated_scroll (paige_rec_ptr pg, short wanted_scroll,
		short PG_FAR *max, long PG_FAR *v_factor, long PG_FAR *pixel_max);
static long re_compute_scroll (paige_rec_ptr pg, pg_fixed percent_of_max,
		short PG_FAR *current_scroll, short PG_FAR *max, short PG_FAR *old_max,
		long PG_FAR *v_factor, long PG_FAR *pixel_max);
static void get_max_scroll_bounds (paige_rec_ptr pg, rectangle_ptr bounds);
static void restore_scrolling_caret (paige_rec_ptr pg);
static void hide_scrolling_caret (paige_rec_ptr pg);


/* pgScrollAdjustProc is the standard scroll adjustment function, but does
nothing by default.  */


PG_PASCAL (void) pgScrollAdjustProc (paige_rec_ptr pg, long amount_h,
		long amount_v, short draw_mode)
{
#ifdef MAC_PLATFORM
#pragma unused (pg, amount_h, amount_v)
#endif

}


/* pgSetScrollAlign sets the pixel alignment mask */

PG_PASCAL (void) pgSetScrollAlign (pg_ref pg, short align_h, short align_v)
{
	paige_rec_ptr		pg_rec;
	short				align_value;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	
	if ((align_value = align_h - 1) > 0)
		pg_rec->scroll_align_h = -1 << align_value;
	if ((align_value = align_v - 1) > 0)
		pg_rec->scroll_align_v = -1 << align_value;

	UnuseMemory(pg);
}


/* pgGetScrollAlign returns the h/v scroll alignment masks. */

PG_PASCAL (void) pgGetScrollAlign (pg_ref pg, short PG_FAR *align_h,
		short PG_FAR *align_v)
{
	paige_rec_ptr		pg_rec;
	
	pg_rec = (paige_rec_ptr) UseMemory(pg);
	
	if (align_h)
		*align_h = (short)get_scroll_align(pg_rec->scroll_align_h);
	
	if (align_v)
		*align_v = (short)get_scroll_align(pg_rec->scroll_align_v);

	UnuseMemory(pg);
}


/* pgSetScrollParams sets all Paige's scroll parameters. SEE DOCS FOR DESCRIPTION
ON WHAT EACH PARAMETER MEANS.  */

PG_PASCAL (void) pgSetScrollParams (pg_ref pg, short unit_h, short unit_v,
		short append_h, short append_v)
{
	paige_rec_ptr			pg_rec;
	
	pg_rec = (paige_rec_ptr) UseMemory(pg);
	
	pg_rec->unit_h = unit_h;
	pg_rec->unit_v = unit_v;
	pg_rec->append_h = append_h;
	pg_rec->append_v = append_v;

	UnuseMemory(pg);
}


/* pgGetScrollParams returns Paige's scroll parameters. SEE DOCS FOR DESCRIPTION
ON WHAT EACH PARAMETER MEANS.  */

PG_PASCAL (void) pgGetScrollParams (pg_ref pg, short PG_FAR *unit_h, short PG_FAR *unit_v,
		short PG_FAR *append_h, short PG_FAR *append_v)
{
	paige_rec_ptr			pg_rec;
	
	pg_rec = (paige_rec_ptr) UseMemory(pg);
	
	if (unit_h)
		*unit_h = pg_rec->unit_h;
	if (unit_v)
		*unit_v = pg_rec->unit_v;
	if (append_h)
		*append_h = pg_rec->append_h;
	if (append_v)
		*append_v = pg_rec->append_v;

	UnuseMemory(pg);
}



/* pgGetScrollValues gets all the current scroll positions. Any parameter can
be NULL, in which case the value is not returned.  The function result is
TRUE if the scroll values have changed since the last pgGetScrollValues
had been called.   */

PG_PASCAL (pg_boolean) pgGetScrollValues (pg_ref pg, short PG_FAR *h, short PG_FAR *v,
		short PG_FAR *max_h, short PG_FAR *max_v)
{
	paige_rec_ptr			pg_rec;
	long					v_max, h_max;
	long					scroll_factor_h, scroll_factor_v;
	short					max_h_result, max_v_result;
	pg_boolean				result;

	pg_rec = (paige_rec_ptr) UseMemory(pg);

	pgScrollParamValues(pg_rec, &h_max, &v_max, &scroll_factor_h, &scroll_factor_v,
			&max_h_result, &max_v_result);

	result = FALSE;

	if (max_v) {

		*max_v = max_v_result;
		result |= (*max_v != pg_rec->last_max_v);
		pg_rec->last_max_v = *max_v;
	}

	if (max_h) {
	
		*max_h = max_h_result;
		result |= (*max_h != pg_rec->last_max_h);
		pg_rec->last_max_h = *max_h;
	}

	if (v) {
		
		if (pg_rec->logical_scroll_pos.v == v_max)
			*v = (short)max_v_result;
		else
			*v = (short)(pg_rec->logical_scroll_pos.v / scroll_factor_v);

		result |= (*v != pg_rec->last_v);
		pg_rec->last_v = *v;
	}
	
	if (h) {

		if (pg_rec->logical_scroll_pos.h == h_max)
			*h = max_h_result;
		else
			*h = (short)(pg_rec->logical_scroll_pos.h / scroll_factor_h);

		result |= (*h != pg_rec->last_h);
		pg_rec->last_h = *h;
	}

	UnuseMemory(pg);
	
	return	result;
}


/* pgSetScrollValues changes the scroll positions to an absolute position.
If align_line = TRUE, the new position tries to align to a line.  */

PG_PASCAL (void) pgSetScrollValues (pg_ref pg, short h, short v, short align_line,
			short draw_mode)
{
	paige_rec_ptr			pg_rec;
	long					pix_h, pix_v, h_max, v_max;
	long					scroll_factor_v, scroll_factor_h;
	short					max_h_result, max_v_result, updated_v, use_draw_mode;
	pg_boolean				do_max_h, do_max_v;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	pg_rec->previous_scroll = pg_rec->logical_scroll_pos;
	
	if ((use_draw_mode = draw_mode) == best_way)
		use_draw_mode = bits_emulate_or;

	pgScrollParamValues(pg_rec, &h_max, &v_max, &scroll_factor_h, &scroll_factor_v,
			&max_h_result, &max_v_result);
	
	updated_v = fix_paginated_scroll(pg_rec, v, &max_v_result, &scroll_factor_v, &v_max);
	
	do_max_h = do_max_v = FALSE;

	if (h == max_h_result) {
	
		pix_h = h_max;
		do_max_h = TRUE;
	}
	else
		pix_h = multiply_factor(h, (short)scroll_factor_h);

	if (updated_v == max_v_result) {
	
		pix_v = v_max;
		do_max_v = TRUE;
	}
	else
		pix_v = multiply_factor(updated_v, (short)scroll_factor_v);

	pix_h = pg_rec->logical_scroll_pos.h - pix_h;
	pix_v = pg_rec->logical_scroll_pos.v - pix_v;

	scroll_pixels(pg_rec, &pix_h, &pix_v, do_max_h, do_max_v, FALSE, draw_none);

	if (align_line && pix_v && (!do_max_v)) {
		long			extra_align;
		
		if (pix_v < 0)
			extra_align = -(bottom_line_distance(pg_rec, FALSE));
		else
			extra_align = top_line_distance(pg_rec);
		
		scroll_pixels(pg_rec, NULL, &extra_align, do_max_h, FALSE, FALSE, draw_none);
		pix_v += extra_align;
	}
	
	if (pix_h || pix_v)
		if (use_draw_mode)
			do_physical_scroll(pg_rec, TRUE, use_draw_mode);	
		
	UnuseMemory(pg);
}



/* pgAutoScrollProc is the standard pgDragSelect auto-scroll function. */

PG_PASCAL (void) pgAutoScrollProc (paige_rec_ptr pg, short h_verb, short v_verb,
		co_ordinate_ptr mouse_point, short draw_mode)
{
	pgScroll(pg->myself, h_verb, v_verb, draw_mode);
	
	if (pg->flags & EXTERNAL_SCROLL_BIT) {

		mouse_point->h += (pg->logical_scroll_pos.h - pg->previous_scroll.h);
		mouse_point->v += (pg->logical_scroll_pos.v - pg->previous_scroll.v);
	}
}


/* pgScrollUnitsToPixels returns the amount the document would scroll, in pixels, given the
two scroll verbs. The document neither scrolls nor adjusts for anything internally.
If add_to_position is TRUE, the pixel value(s) are added to the current scroll position
(affectively "scrolling" the document without visually changing anything.
If window_origin_changes is TRUE, PAIGE assumes that the target window origin will change
by the scrolled amount and will therefor adjust the vis area accordingly. Note,
window_origin_changes will affect the vis area even if add_to_position is FALSE. */

PG_PASCAL (void) pgScrollUnitsToPixels (pg_ref pg, short h_verb, short v_verb,
		pg_boolean add_to_position, pg_boolean window_origin_changes,
		long PG_FAR *h_pixels, long PG_FAR *v_pixels)
{
	paige_rec_ptr			pg_rec;
	co_ordinate				original_scroll, original_logical_scroll, original_previous;
	long					h, v, max_h, max_v, change_h, change_v;
	long					h_pos, v_pos, old_flags;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	old_flags = pg_rec->flags;
	original_logical_scroll = pg_rec->logical_scroll_pos;
	original_scroll = pg_rec->scroll_pos;
	original_previous = pg_rec->previous_scroll;
	
	pg_rec->previous_scroll = pg_rec->logical_scroll_pos;
	maximum_pixel_scroll(pg_rec, &max_h, &max_v);

	h = h_unit_to_pixels(pg_rec, h_verb);
	v = v_unit_to_pixels(pg_rec, v_verb);
	h_pos = pg_rec->logical_scroll_pos.h - h;
	v_pos = pg_rec->logical_scroll_pos.v - v;
	scroll_pixels(pg_rec, &h, &v, (pg_boolean)(h_pos >= max_h), (pg_boolean)(v_pos >= max_v), FALSE, draw_none);

	change_h = pg_rec->previous_scroll.h - pg_rec->logical_scroll_pos.h;
	change_v = pg_rec->previous_scroll.v - pg_rec->logical_scroll_pos.v;
	
	if (h_pixels)
		*h_pixels = change_h;
	if (v_pixels)
		*v_pixels = change_v;

	if (window_origin_changes)
		pgWindowOriginChanged(pg, NULL, NULL);

	if (!add_to_position) {
	
		pg_rec->flags = old_flags;
		pg_rec->logical_scroll_pos = original_logical_scroll;
		pg_rec->previous_scroll = original_previous;
	}

	pg_rec->scroll_pos = original_scroll;

	UnuseMemory(pg);
}


/* pgDrawScrolledArea displays the pg_ref into the area(s) that have just been scrolled.
This function is very useful for applications that have performed their own physical
scrolling. The original_origin and new_origin params are optional and work the same as
pgWindowOriginChanged() function. If these are non-NULL, the pg_ref's vis_area is first
adjusted to reflect the new origin before drawing the scrolled contents.  */

PG_PASCAL (void) pgDrawScrolledArea (pg_ref pg, long pixels_h, long pixels_v,
		const co_ordinate_ptr original_origin, const co_ordinate_ptr new_origin, short draw_mode)
{
	paige_rec_ptr			pg_rec;
	shape_ref				scroll_area;
	rectangle				vis_bounds, affected_area;
	short					use_draw_mode;

	pg_rec = (paige_rec_ptr) UseMemory(pg);

	if (new_origin)
		pgWindowOriginChanged(pg, original_origin, new_origin);

	scroll_area = pgRectToShape(pg_rec->globals->mem_globals, NULL);

	pgShapeBounds(pg_rec->vis_area, &vis_bounds);

	if (pixels_h) {
		
		affected_area = vis_bounds;
		
		if (pixels_h < 0)
			affected_area.top_left.h = affected_area.bot_right.h + pixels_h;
		else
			affected_area.bot_right.h = affected_area.top_left.h + pixels_h;
		
		pgAddRectToShape(scroll_area, &affected_area); 
	}

	if (pixels_v) {
		
		affected_area = vis_bounds;
		
		if (pixels_v < 0)
			affected_area.top_left.v = affected_area.bot_right.v + pixels_v;
		else
			affected_area.bot_right.v = affected_area.top_left.v + pixels_v;
		
		pgAddRectToShape(scroll_area, &affected_area); 
	}

	pg_rec->procs.set_device(pg_rec, set_pg_device, &pg_rec->port, NULL);
	pgClipGrafDevice(pg_rec, clip_standard_verb, MEM_NULL);

	if ((use_draw_mode = draw_mode) == best_way)
		use_draw_mode = bits_emulate_or;
	
/* Needs to erase the page area in case background color is not white */

	if (!pgTransColor(pg_rec->globals, &pg_rec->bk_color))
		pgErasePageArea(pg_rec->myself, MEM_NULL);

	pgClipGrafDevice(pg_rec, clip_standard_verb, MEM_NULL);
	pg_rec->procs.draw_scroll(pg_rec, scroll_area, &pg_rec->logical_scroll_pos, FALSE);
	
	pgUpdateText(pg_rec, &pg_rec->port, 0, pg_rec->t_length, scroll_area, NULL,
			use_draw_mode, (short)(pg_rec->num_selects != 0));	
	pgDrawPages(pg_rec, NULL, NULL, scroll_area, use_draw_mode);

	pgClipGrafDevice(pg_rec, clip_standard_verb, MEM_NULL);
	pg_rec->procs.draw_scroll(pg_rec, scroll_area, &pg_rec->logical_scroll_pos, TRUE);

	pgDisposeShape(scroll_area);
	
	if (!pg_rec->num_selects)
		restore_scrolling_caret(pg_rec);

 	pg_rec->procs.set_device(pg_rec, unset_pg_device, &pg_rec->port, NULL);
 
	UnuseMemory(pg);
}



/* pgScroll scrolls a specified h and v distance.  */

PG_PASCAL (void) pgScroll (pg_ref pg, short h_verb, short v_verb, short draw_mode)
{
	paige_rec_ptr			pg_rec;
	long					h, v, max_h, max_v;
	long					h_pos, v_pos;
	short					use_draw_mode;
	
	pg_rec = (paige_rec_ptr) UseMemory(pg);

	pg_rec->previous_scroll = pg_rec->logical_scroll_pos;
	maximum_pixel_scroll(pg_rec, &max_h, &max_v);
	
	if ((use_draw_mode = draw_mode) == best_way)
		use_draw_mode = bits_emulate_or;

	h = h_unit_to_pixels(pg_rec, h_verb);
	v = v_unit_to_pixels(pg_rec, v_verb);
	h_pos = pg_rec->logical_scroll_pos.h - h;
	v_pos = pg_rec->logical_scroll_pos.v - v;

	scroll_pixels(pg_rec, &h, &v, (pg_boolean)(h_pos >= max_h), (pg_boolean)(v_pos >= max_v), TRUE, use_draw_mode);	
	UnuseMemory(pg);
}


/* pgAdjustScrollMax fixes the following situation: (A) You had a big doc, scrolled
near or to the bottom, (B) Text is deleted, reducing the max scroll, hence (C) The
doc now sits too low.  This function scrolls the document so it sits on the
maximum boundary.  The function returns "TRUE" if it had to adjust. */

PG_PASCAL (pg_boolean) pgAdjustScrollMax (pg_ref pg, short draw_mode)
{
	paige_rec_ptr			pg_rec;
	long					the_max, h_max, diff;
	short					use_draw_mode;
	pg_boolean				result;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	result = FALSE;
	pg_rec->previous_scroll = pg_rec->logical_scroll_pos;
	
	if ((use_draw_mode = draw_mode) == best_way)
		use_draw_mode = bits_emulate_or;

	pgComputeDocHeight(pg_rec, FALSE);
	maximum_pixel_scroll(pg_rec, &h_max, &the_max);

	if (pg_rec->logical_scroll_pos.v > the_max) {
		
		diff = pg_rec->logical_scroll_pos.v - the_max;
		pg_rec->logical_scroll_pos.v = the_max;

		if (!(pg_rec->flags & EXTERNAL_SCROLL_BIT))
			pg_rec->scroll_pos.v = the_max;

		if (use_draw_mode)
			do_physical_scroll(pg_rec, TRUE, use_draw_mode);
		
		result = TRUE;
	}
	
	if (pg_rec->logical_scroll_pos.h > h_max) {
		
		diff = pg_rec->logical_scroll_pos.h - h_max;
		pg_rec->logical_scroll_pos.h = h_max;

		if (!(pg_rec->flags & EXTERNAL_SCROLL_BIT))
			pg_rec->scroll_pos.h = h_max;

		if (use_draw_mode)
			do_physical_scroll(pg_rec, TRUE, use_draw_mode);
		
		result = TRUE;
	}

	UnuseMemory(pg);
	
	return	result;
}


/* pgLastScrollAmount returns the distance the document was scrolled, in pixels, during the
last scrolling action. */

PG_PASCAL (void) pgLastScrollAmount (pg_ref pg, long *h_pixels, long *v_pixels)
{
	paige_rec_ptr			pg_rec;
	
	pg_rec = (paige_rec_ptr) UseMemory(pg);
	
	if (h_pixels)
		*h_pixels = pg_rec->previous_scroll.h - pg_rec->logical_scroll_pos.h;
	if (v_pixels)
		*v_pixels = pg_rec->previous_scroll.v - pg_rec->logical_scroll_pos.v;

	UnuseMemory(pg);
}


/* pgScrollToView causes the specified offset to be scrolled to view. The offset
can be CURRENT_POSITION (-1), or some other number.  The position of text_offset
is scrolled to vis area's top or bottom + v_extra minimum and left/right 
v_extra minimum.  If align_line is TRUE the new position tries to align with
a line.  The function returns TRUE if any scrolling occurred.   */

extern PG_PASCAL (pg_boolean) pgScrollToView (pg_ref pg, long text_offset, short h_extra,
		short v_extra, pg_boolean align_line, short draw_mode)
{
	paige_rec_ptr			pg_rec;
	rectangle				target_rect, vis_rect;
	long					vis_height, vis_width, pix_h, pix_v, max_h, max_v;
	long					h_pos, v_pos, use_offset, target_height, old_attributes;
	short					use_v_extra, use_h_extra, use_draw_mode;
	pg_boolean				use_align_line, result;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	old_attributes = (pg_rec->doc_info.attributes & WINDOW_CURSOR_BIT);
	pg_rec->doc_info.attributes &= (~WINDOW_CURSOR_BIT);
	
	if ((use_draw_mode = draw_mode) == best_way)
		use_draw_mode = bits_emulate_or;

	use_offset = pgFixOffset(pg_rec, text_offset);

	result = FALSE;
	pix_h = pix_v = 0;
	use_v_extra = v_extra;
	use_h_extra = h_extra;
	use_align_line = align_line;

	pg_rec->previous_scroll = pg_rec->logical_scroll_pos;
	
	pgCaretPosition(pg, text_offset, &target_rect);
	pgInsetRect(&target_rect, -1, 0);
	target_height = target_rect.bot_right.v - target_rect.top_left.v;

	GetMemoryRecord(pg_rec->vis_area, 0, &vis_rect);
	
	if (!(pg_rec->flags & SCALE_VIS_BIT)) {
	
		pg_rec->port.scale.scale = -pg_rec->port.scale.scale;
		pgScaleRect(&pg_rec->port.scale, NULL, &vis_rect);
		pg_rec->port.scale.scale = -pg_rec->port.scale.scale;
	}

	vis_height = vis_rect.bot_right.v - vis_rect.top_left.v;
	vis_width = vis_rect.bot_right.h - vis_rect.top_left.h;
	
	maximum_pixel_scroll(pg_rec, &max_h, &max_v);

	if (target_height > vis_height) {
		
		target_rect.top_left.v += (target_height / 2);
		target_rect.bot_right.v = target_rect.top_left.v;
		use_v_extra = 0;
		use_align_line = FALSE;
	}
	else
	if (target_height > (vis_height / 4)) {
	
		use_v_extra = 0;
		use_align_line = FALSE;
	}

	if (use_v_extra > (vis_height / 3))
		use_v_extra = 0;

	if (use_h_extra > (vis_width / 2))
		use_h_extra = (short)vis_width / 2;
	
	if (!(pg_rec->flags2 & NO_VAUTOSCROLL)) {

		if (target_rect.bot_right.v > vis_rect.bot_right.v)
			pix_v = vis_rect.bot_right.v - target_rect.bot_right.v - use_v_extra;
		else
		if (target_rect.top_left.v < vis_rect.top_left.v)
			pix_v = vis_rect.top_left.v - target_rect.top_left.v + use_v_extra;
	}

	if (!(pg_rec->flags2 & NO_HAUTOSCROLL)) {

		if (target_rect.bot_right.h > vis_rect.bot_right.h)
			pix_h = vis_rect.bot_right.h - target_rect.bot_right.h - use_h_extra;
		else
		if (target_rect.top_left.h < vis_rect.top_left.h)
			pix_h = vis_rect.top_left.h - target_rect.top_left.h + use_h_extra;
	}

	if (pix_h || pix_v) {
		long			align_extra;
		
		h_pos = pg_rec->logical_scroll_pos.h - pix_h;
		v_pos = pg_rec->logical_scroll_pos.v - pix_v;

		scroll_pixels(pg_rec, &pix_h, &pix_v, (pg_boolean)(h_pos >= max_h),
				(pg_boolean)(v_pos >= max_v), TRUE, use_draw_mode);

		pg_rec->stable_caret.h = pg_rec->stable_caret.v = 0;
		pg_rec->previous_scroll = pg_rec->logical_scroll_pos;

		result = ((pix_h != 0) || (pix_v != 0));
		
		if (pix_v && use_align_line) {
			
			if (pix_v < 0)
				align_extra = -(bottom_line_distance(pg_rec, FALSE));
			else
				align_extra = top_line_distance(pg_rec);

			v_pos = pg_rec->logical_scroll_pos.v - align_extra;

			scroll_pixels(pg_rec, NULL, &align_extra, FALSE,
				(pg_boolean)(v_pos >= max_v), TRUE, use_draw_mode);

			pix_v += align_extra;
		}
	}

	pg_rec->doc_info.attributes |= old_attributes;

	UnuseMemory(pg);
	
	return	result;
}


/* pgScrollViewRect is a platform-indpendent method of physically scrolling the visual area
of the pg_ref. Note that only a visual pixel scroll is performed -- no scrolling adjustments are
made to the document. If update_area is not MEM_NULL, the shape gets set to the area that
requires re-drawing. */

PG_PASCAL (pg_region) pgScrollViewRect (pg_ref pg, long h_pixels, long v_pixels)
{
	paige_rec_ptr			pg_rec;
	rectangle				vis_bounds;
	pg_region				result;
	
	pg_rec = (paige_rec_ptr) UseMemory(pg);
	
	pgSetupGrafDevice(pg_rec, &pg_rec->port, MEM_NULL, clip_with_none_verb);
	pgShapeBounds(pg_rec->vis_area, &vis_bounds);
    pgInsetRect(&vis_bounds, pg_rec->doc_info.scroll_inset, pg_rec->doc_info.scroll_inset);
	
	result = REGION_NULL;

	if (v_pixels)
		result = pgScrollRect(pg_rec, &vis_bounds, 0, v_pixels, NULL, draw_none);
	
	pg_rec->port.scroll_rgn = result;

	if (h_pixels)
		result = pgScrollRect(pg_rec, &vis_bounds, h_pixels, 0, NULL, draw_none);
	
	pg_rec->port.scroll_rgn = REGION_NULL;
	pg_rec->procs.set_device(pg_rec, unset_pg_device, &pg_rec->port, NULL);

	UnuseMemory(pg);
	
	return		result;
}


/* pgScrollPosition returns the current physical scroll position (which is
always a positive number) */

PG_PASCAL (void) pgScrollPosition (pg_ref pg, co_ordinate_ptr scroll_pos)
{
	paige_rec_ptr	pg_rec;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	*scroll_pos = pg_rec->logical_scroll_pos;
	UnuseMemory(pg);
}


/* pgScrollPixels allows a hard pixel scroll. If draw_mode is FALSE nothing
is changed on screen.  */

PG_PASCAL (void) pgScrollPixels (pg_ref pg, long h, long v, short draw_mode)
{
	paige_rec_ptr	pg_rec;
	long			scroll_h, scroll_v, max_h, max_v;
	long			pix_h, pix_v;
	short			use_draw_mode;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	pg_rec->previous_scroll = pg_rec->logical_scroll_pos;
	
	if ((use_draw_mode = draw_mode) == best_way)
		use_draw_mode = bits_emulate_or;

	scroll_h = h;
	scroll_v = v;
	maximum_pixel_scroll(pg_rec, &max_h, &max_v);
	pix_h = pg_rec->logical_scroll_pos.h - scroll_h;
	pix_v = pg_rec->logical_scroll_pos.v - scroll_v;

	scroll_pixels(pg_rec, &scroll_h, &scroll_v, (pg_boolean)(pix_h >= max_h), (pg_boolean)(pix_v >= max_v), TRUE, use_draw_mode);

	UnuseMemory(pg);
}



/* This is a "convenience" function to return values I use frequently. H_max and
v_max are the maximum allowed pixel scrollings, scroll_factor_h & v are the amount
to divide for < 32K scroll value(s), max_h_result and max_v_result are the short
values for maximum scroll (< 32K).    */

PG_PASCAL (void) pgScrollParamValues (paige_rec_ptr pg, long PG_FAR *h_max, long PG_FAR *v_max,
		long PG_FAR *scroll_factor_h, long PG_FAR *scroll_factor_v,
		short PG_FAR *max_h_result, short PG_FAR *max_v_result)
{
	rectangle		page_bounds;

	get_max_scroll_bounds(pg, &page_bounds);
	
	*scroll_factor_v = div_factor(page_bounds.bot_right.v);
	*scroll_factor_h = div_factor(page_bounds.bot_right.h);
	maximum_pixel_scroll(pg, h_max, v_max);

	*max_h_result = (short)(*h_max / *scroll_factor_h);
	*max_v_result = (short)(*v_max / *scroll_factor_v);
}



/*************************** Local Functions **************************/


/* get_max_scroll_bounds returns the bounding, scrollable maximum. */

static void get_max_scroll_bounds (paige_rec_ptr pg, rectangle_ptr bounds)
{
	co_ordinate		page_bottom;
	long			page_height;
	pg_short_t		bottom_page;

	if ((pg->doc_info.attributes & MAX_SCROLL_ON_SHAPE) && (pg->doc_info.attributes & BOTTOM_FIXED_BIT)) {

		pgShapeBounds(pg->wrap_area, bounds);

		if (pg->doc_info.attributes & V_REPEAT_BIT) {
			
			page_bottom = pg->doc_bounds.bot_right;
			page_bottom.v += pg->append_v;
			bottom_page = pgPixelToPage(pg->myself, &page_bottom,
					NULL, NULL, &page_height, NULL, FALSE) + 1;
			bounds->bot_right.v = bounds->top_left.v + (page_height * bottom_page);
			bounds->bot_right.v -= pg->doc_info.repeat_offset.v;
		}
	}
	else
		pgBlockMove(&pg->doc_bounds, bounds, sizeof(rectangle));
}


/* This function does the physical scrolling of a Paige item, in pixels. The
values in *pix_h and *pix_v are also checked for legal maximums / minimums and are
adjusted accordingly. On return, *pix_h and *pix_v are set to the actual (real)
scrolled amount.
Added 4/5/94: If scroll_max_h and/or scroll_max_v are TRUE then the caller
wants maximum scroll without any alignment. This fixes a bug in aligning bits
when app wants to scroll to bottom.
If final_call is FALSE then adjust_scroll proc is not called. */

static void scroll_pixels (paige_rec_ptr pg, long PG_FAR *pix_h, long PG_FAR *pix_v,
			pg_boolean scroll_max_h, pg_boolean scroll_max_v, pg_boolean final_call,
			short draw_mode)
{
	co_ordinate			adjust_pos;
	long				max_h, max_v, move_h, move_v;

	move_h = move_v = 0;

	if (pix_h)
		move_h = *pix_h;
	if (pix_v)
		move_v = *pix_v;

	maximum_pixel_scroll(pg, &max_h, &max_v);
	
	if (!scroll_max_h) {
	
		max_h &= pg->scroll_align_h;
		align_movement(&move_h, pg->scroll_align_h);
	}

	if (!scroll_max_v) {
	
		max_v &= pg->scroll_align_v;
		align_movement(&move_v, pg->scroll_align_v);
	}

	adjust_pos = pg->logical_scroll_pos;
	adjust_pos.h -= move_h;
	adjust_pos.v -= move_v;

	if (adjust_pos.h < 0) {
		
		adjust_pos.h = 0;
		move_h = pg->logical_scroll_pos.h;
	}
	else
	if (adjust_pos.h > max_h) {
		
		adjust_pos.h = max_h;
		move_h = pg->logical_scroll_pos.h - max_h;
	}

	if (adjust_pos.v < 0) {
		
		adjust_pos.v = 0;
		move_v = pg->logical_scroll_pos.v;
	}
	else
	if (adjust_pos.v > max_v) {
		
		adjust_pos.v = max_v;
		move_v = pg->logical_scroll_pos.v - max_v;
	}

	if (pix_h)
		*pix_h = move_h;
	if (pix_v)
		*pix_v = move_v;

	if (move_h || move_v) {
		
		hide_scrolling_caret(pg);
        pg->flags |= INVALID_CURSOR_BIT;
		pg->logical_scroll_pos = adjust_pos;
		
		if (!(pg->flags & EXTERNAL_SCROLL_BIT))
			pg->scroll_pos = pg->logical_scroll_pos;

		do_physical_scroll(pg, final_call, draw_mode);
	} 
}


/* This aligns movement value by the alignment */

static void align_movement (long PG_FAR *movement, long alignment)
{
	long		inverted_align, value;
	
	if (alignment != -1) {
		
		if ((value = *movement) < 0)
			value = -value;

		inverted_align = ~alignment;
		
		if (*movement & inverted_align)
			value += inverted_align;
		
		value &= alignment;
		
		if (*movement < 0)
			value = -value;
		
		*movement = value;			
	}
}


/* This is the function that performs the visual scroll and screen re-draw. */

static void do_physical_scroll (paige_rec_ptr pg, pg_boolean final_call, short draw_mode)
{
	co_ordinate			old_pos, new_pos;
	rectangle			scroll_rect, affect_h, affect_v;
	shape_ref			affected_shape;
	long				move_h, move_v, actual_move;
	short				use_draw_mode;
	
	pg->port.clip_info.change_flags |= CLIP_SCROLL_CHANGED;
	pg->procs.set_device(pg, set_pg_device, &pg->port, NULL);
	pgClipGrafDevice(pg, clip_with_none_verb, MEM_NULL);
	pgFillBlock(&affect_h, sizeof(rectangle), 0);
	pgFillBlock(&affect_v, sizeof(rectangle), 0);

	old_pos = pg->previous_scroll;
	new_pos = pg->logical_scroll_pos;
	pgScaleLong(pg->port.scale.scale, 0, &old_pos.h);
	pgScaleLong(pg->port.scale.scale, 0, &old_pos.v);
	pgScaleLong(pg->port.scale.scale, 0, &new_pos.h);
	pgScaleLong(pg->port.scale.scale, 0, &new_pos.v);

	move_h = old_pos.h - new_pos.h;
	move_v = old_pos.v - new_pos.v;

	if (move_h || move_v) {
	
		GetMemoryRecord(pg->vis_area, 0, &scroll_rect);
		
		if (pg->flags & SCALE_VIS_BIT)
			pgScaleRect(&pg->port.scale, NULL, &scroll_rect);

		pgInsetRect(&scroll_rect, pg->doc_info.scroll_inset, pg->doc_info.scroll_inset);

		if (actual_move = move_v) {
	
			//pgScaleLong(pg->port.scale.scale, 0, &actual_move);
			
			if (final_call)
				pg->procs.adjust_scroll(pg, 0, actual_move, draw_mode);
			
			if (draw_mode)
				pg->port.scroll_rgn = pgScrollRect(pg, &scroll_rect, 0, actual_move, &affect_v, draw_mode);

			if (final_call)
				pg->procs.adjust_scroll(pg, 0, 0, draw_mode);
		}
	
		if (actual_move = move_h) {
	
			//pgScaleLong(pg->port.scale.scale, 0, &actual_move);

			if (final_call)
				pg->procs.adjust_scroll(pg, actual_move, 0, draw_mode);
			
			if (draw_mode)
				pg->port.scroll_rgn = pgScrollRect(pg, &scroll_rect, actual_move, 0, &affect_h, draw_mode);

			if (final_call)
				pg->procs.adjust_scroll(pg, 0, 0, draw_mode);
		}

		if ((pg->flags & SCALE_VIS_BIT) && pg->port.scale.scale) {
			
			pg->port.scale.scale = -pg->port.scale.scale;
			pgScaleRect(&pg->port.scale, NULL, &affect_v);
			pg->port.scale.scale = -pg->port.scale.scale;
		}

		pgUnionRect(&affect_h, &affect_v, &affect_v);
		affected_shape = pgRectToShape(pg->globals->mem_globals, &affect_v);

		if (draw_mode) {
			memory_ref		copy_of_subs;
			
			copy_of_subs = pgGetSubrefState(pg, FALSE, TRUE);

			if ((use_draw_mode = draw_mode) == best_way)
				use_draw_mode = bits_emulate_or;
			
			if (use_draw_mode != bits_copy)
				pgErasePageArea(pg->myself, MEM_NULL);

			pgClipGrafDevice(pg, clip_with_none_verb, MEM_NULL);
			pg->procs.draw_scroll(pg, affected_shape, &pg->logical_scroll_pos, FALSE);
			
			if (draw_mode == bits_copy)
				pg->flags2 |= BITMAP_SCROLL_CHANGE;

			pgUpdateText(pg, &pg->port, 0, pg->t_length, affected_shape, NULL, use_draw_mode,
					(short)(pg->num_selects != 0));
			pgDrawPages(pg, NULL, NULL, affected_shape, use_draw_mode);
			
			pg->flags2 &= (~BITMAP_SCROLL_CHANGE);
			pgRestoreSubRefs(pg, copy_of_subs);
		}

		pgClipGrafDevice(pg, clip_with_none_verb, MEM_NULL);
		pg->procs.draw_scroll(pg, affected_shape, &pg->logical_scroll_pos, TRUE);

		pgDisposeShape(affected_shape);
	}

	pg->port.clip_info.change_flags |= CLIP_SCROLL_CHANGED;

	if (!pg->num_selects && draw_mode)
		restore_scrolling_caret(pg);
	
	pg->procs.set_device(pg, unset_pg_device, &pg->port, NULL);

	if (pg->port.scroll_rgn) {
		
		pgDisposeRgn(pg->port.scroll_rgn);
		pg->port.scroll_rgn = REGION_NULL;
	}
}

/* This function returns a short(er) version of value if value is too large.  */

static long div_factor (long scroll_value)
{
	register long		shift_value;
	register long		new_value;

	new_value = scroll_value;
	shift_value = 1;
	
	while (new_value > MAX_LONG_VALUE) {
		shift_value <<= 1;
		new_value >>= 1;
	}
	
	return	shift_value;
}


/* maximum_pixel_scroll returns the maximum distance, in pixels, that pg can
scroll h and v. (Parameters can be NULL).  */

static void maximum_pixel_scroll (paige_rec_ptr pg, long PG_FAR *h, long PG_FAR *v)
{
	rectangle			vis_bounds, doc_bounds;
	long				pix_remain;

	pgShapeBounds(pg->vis_area, &vis_bounds);
	get_max_scroll_bounds(pg, &doc_bounds);

	if (!(pg->flags & SCALE_VIS_BIT)) {
	
		pgScaleLong(-pg->port.scale.scale, 0, &vis_bounds.bot_right.v);
		pgScaleLong(-pg->port.scale.scale, 0, &vis_bounds.bot_right.h);
	}

	if (v) {
	
		if ((pix_remain = doc_bounds.bot_right.v - vis_bounds.bot_right.v
				+ pg->append_v) < 0)
			pix_remain = 0;
		
		*v = pix_remain;
		*v &= pg->scroll_align_v;
	}
	
	if (h) {
	
		if ((pix_remain = doc_bounds.bot_right.h - vis_bounds.bot_right.h
				+ pg->append_h) < 0)
			pix_remain = 0;
		
		*h = pix_remain;
		*h &= pg->scroll_align_h;
	}
}


/* This function returns the number of pixels to scroll based on the unit_verb
for horizontal motion.  */

static long h_unit_to_pixels (paige_rec_ptr pg, short unit_verb)
{
	long			abs_unit, result;
	rectangle		vis_rect;

	abs_unit = pgAbsoluteValue(unit_verb);
	GetMemoryRecord(pg->vis_area, 0, &vis_rect);

	switch (abs_unit) {

		case scroll_none:
			return	0;

		case scroll_unit:
			if (!(result = pg->unit_h))
				if (!(result = ((vis_rect.bot_right.h - vis_rect.top_left.h) / DEF_H_UNIT)))
					result = 1;
			break;

		case scroll_page:
			result = vis_rect.bot_right.h - vis_rect.top_left.h;
			pgScaleLong(-pg->port.scale.scale, 0, &result);

			break;

		case scroll_home:
			result = pg->logical_scroll_pos.h;
			break;

		case scroll_end:
			maximum_pixel_scroll(pg, &result, NULL);
			break;
	}
	
	if (unit_verb < 0)
		result = -result;

	return	result;
}


/* This function returns the number of pixels to scroll based on the unit_verb
for vertical motion.  */

static long v_unit_to_pixels (paige_rec_ptr pg, short unit_verb)
{
	long			abs_unit, use_for_paging, result;
	rectangle		vis_rect;

	abs_unit = pgAbsoluteValue(unit_verb);
	GetMemoryRecord(pg->vis_area, 0, &vis_rect);

	switch (abs_unit) {

		case scroll_none:
			return	0;

		case scroll_unit:			
			if (!(result = pg->unit_v)) {

				if (unit_verb < 0)
					result = bottom_line_distance(pg, TRUE);
				else
					result = top_line_distance(pg);
			}

			break;

		case scroll_page:
			result = vis_rect.bot_right.v - vis_rect.top_left.v;
			pgScaleLong(-pg->port.scale.scale, 0, &result);

			if (unit_verb < 0)
				use_for_paging = vis_rect.bot_right.v;
			else
				use_for_paging = vis_rect.top_left.v;
				
			result -= closest_start_sect(pg, &vis_rect, use_for_paging, TRUE);

			break;

		case scroll_home:
			result = pg->logical_scroll_pos.v;
			break;

		case scroll_end:
			maximum_pixel_scroll(pg, NULL, &result);
			result = -result;
			break;
	}
	
	if (unit_verb < 0)
		result = -result;

	return	result;
}


/* This function figures out the distance from the top edge of vis area to the
previous line's top. If none, some best guess is made.  */

static long top_line_distance (paige_rec_ptr pg)
{
	rectangle						vis_rect;
	long							result;

	GetMemoryRecord(pg->vis_area, 0, &vis_rect);

	if (result = closest_start_sect(pg, &vis_rect, vis_rect.top_left.v, FALSE))
		return	result;
	
	return	DEF_UNIT_SCROLL;
}



/* This function figures out the distance from the bottom edge of vis area to the
next line's bottom. If none, some best guess is made if best_guess is TRUE.  */

static long bottom_line_distance (paige_rec_ptr pg, pg_boolean best_guess)
{
	rectangle						vis_rect;
	long							result;

	GetMemoryRecord(pg->vis_area, 0, &vis_rect);
	
	if (result = closest_start_sect(pg, &vis_rect, vis_rect.bot_right.v, FALSE))
		return	result;
	
	if (best_guess)
		return	DEF_UNIT_SCROLL;
	
	return	0;
}


/* This function works mainly with bottom_line_distance and top_line_distance.
Its purpose is to return the distance (as a positive number) between either the
top or bottom of vis to the previous or next closest line position. The basic
idea is to figure out how far to scroll to make a previous or next line align
on the top/bottom of vis.  The vis rect is the visual area wanted. If want_inside
is TRUE, the closest line is the one INSIDE the vis rectangle.  */

static long closest_start_sect (paige_rec_ptr pg, rectangle_ptr vis, long side_to_use,
			short want_inside)
{
	register text_block_ptr		block;
	register point_start_ptr	starts;
	register long				edge_compare, edge_check;
	rectangle					scrolled_r;
	pg_short_t					num_blocks;
	long						best_result, less_page_amt, minimum, maximum;
	long						one_eighth;
	short						use_bottom;

	pgBlockMove(vis, &scrolled_r, sizeof(rectangle));
	scrolled_r.top_left.v += pg->scroll_pos.v;
	scrolled_r.bot_right.v += pg->scroll_pos.v;

	num_blocks = (pg_short_t)GetMemorySize(pg->t_blocks);
	block = (text_block_ptr) UseMemory(pg->t_blocks);
	one_eighth = (vis->bot_right.v - vis->top_left.v) / 8;
	
	if (use_bottom = (vis->bot_right.v == side_to_use)) {
		
		edge_compare = scrolled_r.bot_right.v;
		scrolled_r.top_left.v = scrolled_r.bot_right.v - 1;
		minimum = scrolled_r.bot_right.v - one_eighth;
		maximum = 0x7FFFFFF0;
	}
	else {
		edge_compare = scrolled_r.top_left.v;
		scrolled_r.bot_right.v = scrolled_r.top_left.v + 1;
		minimum = block->bounds.top_left.v - 2;
		maximum = scrolled_r.top_left.v + one_eighth;
	}

	scrolled_r.top_left.v -= 1;
	scrolled_r.bot_right.v += 1;

	best_result = 0;

	while (num_blocks) {
		
		if (pgSectRect(&block->bounds, &scrolled_r, NULL)) {
			
			pgPaginateBlock(pg, block, NULL, TRUE);
			starts = (point_start_ptr) UseMemory(block->lines);
			
			while (starts->flags != TERMINATOR_BITS) {
				
				if (want_inside)
					less_page_amt = (starts->bounds.bot_right.v - starts->bounds.top_left.v) / 2;

				if (use_bottom) {
					
					if (want_inside) {

						if ((edge_check = edge_compare - starts->bounds.top_left.v) > less_page_amt)
							if ((starts->bounds.top_left.v >= minimum) && (starts->bounds.bot_right.v <= maximum))
								if ((!best_result) || (edge_check < best_result))
									best_result = edge_check;
					}
					else
					if ((edge_check = starts->bounds.bot_right.v - edge_compare) > 0)
						if ((starts->bounds.top_left.v >= minimum) && (starts->bounds.bot_right.v <= maximum))
							if ((!best_result) || (edge_check < best_result))
								best_result = edge_check;
				}
				else {

					if (want_inside) {
						
						if ((edge_check = starts->bounds.bot_right.v - edge_compare) > less_page_amt)
							if ((starts->bounds.top_left.v >= minimum) && (starts->bounds.bot_right.v <= maximum))
								if ((!best_result) || (edge_check < best_result))
									best_result = edge_check;
					}
					else
					if ((edge_check = edge_compare - starts->bounds.top_left.v) > 0)
						if ((starts->bounds.top_left.v >= minimum) && (starts->bounds.bot_right.v <= maximum))
							if ((!best_result) || (edge_check < best_result))
								best_result = edge_check;
				}

				++starts;
			}
			
			UnuseMemory(block->lines);
		}
		
		++block;
		--num_blocks;
	}
	
	UnuseMemory(pg->t_blocks);
	return	best_result;
}


/* multiply_factor returns an UNSIGNED product of value X factor (and factor is
known to be a power of 2).  */

static long multiply_factor (short value, short factor)
{
	unsigned long		long_value;

	if (factor < 2)
		return	value;
	
	long_value = value;
	long_value *= factor;
	
	return	(long)	long_value;
}


/* This function returns the original scroll align based on a negative "mask" align */
static long get_scroll_align (long scroll_align)
{
	long		ctr, result;
	
	for (result = 1, ctr = scroll_align; !(ctr & 1); ++result, ctr >>= 1);
	
	return	result;
}


/* fix_paginated_scroll returns what a new vertical scroll position SHOULD be
when and if it paginates to the new location. If necessary, *max, *v_factor
and *pixel_max get updated if these values change. */

static short fix_paginated_scroll (paige_rec_ptr pg, short wanted_scroll,
		short PG_FAR *max, long PG_FAR *v_factor, long PG_FAR *pixel_max)
{
	text_block_ptr			block;
	rectangle				vis_bounds, block_check;
	pg_fixed				percent_of_max;
	long					will_scroll_to;
	pg_short_t				num_blocks, max_blocks, block_ctr;
	short					current_scroll, old_max, needed_calc;
	pg_boolean				called_wait;

	if (!wanted_scroll)
		return	0;

	if (*max == 0)
		percent_of_max = 0;
	else
		percent_of_max = pgFixedRatio(wanted_scroll, *max);

	old_max = *max;

	pgShapeBounds(pg->vis_area, &vis_bounds);

	current_scroll = wanted_scroll;

	block = (text_block_ptr) UseMemory(pg->t_blocks);
	num_blocks = max_blocks = (pg_short_t)GetMemorySize(pg->t_blocks);
	block_ctr = 1;
	called_wait = FALSE;

	while (num_blocks) {
		
		needed_calc = (block->flags & ANY_CALC);
		
		pgPaginateBlock(pg, block, NULL, FALSE);

		if (called_wait || (needed_calc && (num_blocks > 1))) {
		
			pg->procs.wait_proc(pg, paginate_wait, block_ctr, max_blocks);
			called_wait = TRUE;
		}

		will_scroll_to = re_compute_scroll(pg, percent_of_max, &current_scroll,
				max, &old_max, v_factor, pixel_max);

		block_check = block->bounds;
		pgOffsetRect(&block_check, 0, -will_scroll_to);

		if (pgSectRect(&block_check, &vis_bounds, NULL))
			break;

		++block;
		++block_ctr;
		--num_blocks;
	}

	if ( (num_blocks > 1) && (block_check.bot_right.v < vis_bounds.bot_right.v) ) {
		
		++block;
		pgPaginateBlock(pg, block, NULL, FALSE);

		if (called_wait)
			pg->procs.wait_proc(pg, paginate_wait, block_ctr + 1, max_blocks);

	}

	UnuseMemory(pg->t_blocks);
	
	re_compute_scroll(pg, percent_of_max, &current_scroll,
				max, &old_max, v_factor, pixel_max);

	if (called_wait)
		pg->procs.wait_proc(pg, paginate_wait, max_blocks, max_blocks);

	
	return	current_scroll;
}


/* re_compute_scroll re-works the vertical scroll position based on max X
percent_of_max. While we are at it, the *max, *pixel_max and *v_factor params are
updated. The *old_max param is needed (and updated) to know if max scroll
has changed. The *current_scroll param is the last known vertical scroll value.
and it gets updated as well.  The function result is the new pixel position for
the scrolling. */

static long re_compute_scroll (paige_rec_ptr pg, pg_fixed percent_of_max,
		short PG_FAR *current_scroll, short PG_FAR *max, short PG_FAR *old_max,
		long PG_FAR *v_factor, long PG_FAR *pixel_max)
{
	long		h_factor, h_max, work;
	short		max_h_result;

	pgScrollParamValues(pg, &h_max, pixel_max, &h_factor, v_factor,
			&max_h_result, max);

	if (*max != *old_max) {
		
		*old_max = *max;

		work = *max;
		work <<= 16;
		work = pgMultiplyFixed(work, percent_of_max);
		
		if (work & 0x00008000)
			work += 0x00010000;
		
		work >>= 16;
		*current_scroll = (short)work;
	}
	
	return	multiply_factor(*current_scroll, (short)*v_factor);
}


/* hide_scrolling_caret hides the caret just before scroll. Additionally, if WINDOW_CURSOR_BIT
is set in doc_info then we make sure the stable caret is computed and the relative cursor
is returned. */

static void hide_scrolling_caret (paige_rec_ptr pg)
{
	t_select_ptr				select;
	rectangle_ptr				vis_rect;

	if (pg->num_selects || (pg->flags & (DEACT_BIT | PERM_DEACT_BIT)))
		return;

	pgSetupGrafDevice(pg, &pg->port, MEM_NULL, clip_standard_verb);

	select = (t_select_ptr) UseMemory(pg->select);
	
	if (pg->doc_info.attributes & WINDOW_CURSOR_BIT) {
		
		if (!pg->stable_caret.h)
			pg->procs.cursor_proc(pg, select, compute_cursor);
		
		vis_rect = (rectangle_ptr) UseMemory(pg->vis_area);
		pg->relative_caret.h = pg->stable_caret.h - vis_rect->top_left.h;
		pg->relative_caret.v = pg->stable_caret.v - vis_rect->top_left.v;

		UnuseMemory(pg->vis_area);
	}
	
	if ((pg->flags & CARET_BIT))
		pg->procs.cursor_proc(pg, select, hide_cursor);
	
	pg->procs.set_device(pg, unset_pg_device, &pg->port, NULL);

	UnuseMemory(pg->select);
}


/* restore_scrolling_caret changes the current selection in pg (if just a caret) to
match the position relative_cursor. This is to accommodate all the Windows people with
keeping the caret in view. The releative_cursor spot is RELATIVE TO TOP-LEFT OF VIS AREA,
not an absolute co-ordinates. */

static void restore_scrolling_caret (paige_rec_ptr pg)
{
	t_select_ptr				select;
	rectangle_ptr				vis_rect;
	co_ordinate					fake_point;
	rectangle					caretbox;
	short						infinite_loop_protect;

	if (pg->num_selects || (pg->flags & (DEACT_BIT | PERM_DEACT_BIT)))
		return;
	
	select = (t_select_ptr) UseMemory(pg->select);

	if (pg->doc_info.attributes & WINDOW_CURSOR_BIT) {

		vis_rect = (rectangle_ptr) UseMemory(pg->vis_area);

		for (infinite_loop_protect = 3; infinite_loop_protect; infinite_loop_protect -= 1) {

			fake_point = vis_rect->top_left;
			pgAddPt(&pg->relative_caret, &fake_point);
			pgAddPt(&pg->scroll_pos, &fake_point);
		 	pg->procs.offset_proc(pg, &fake_point, 0, select);
		 	pgBlockMove(select, &select[1], sizeof(t_select));
		 	pg->stable_caret = pg->relative_caret;
		 	pgAddPt(&vis_rect->top_left, &pg->stable_caret);
		 	
			pgCaretPosition(pg->myself, CURRENT_POSITION, &caretbox);
			caretbox.top_left.v -= 2;
			caretbox.bot_right.v += 2;
			
			if (caretbox.top_left.v < vis_rect->top_left.v)
				pg->relative_caret.v += (vis_rect->top_left.v - caretbox.top_left.v + 1);
			else
			if (caretbox.bot_right.v > vis_rect->bot_right.v)
				pg->relative_caret.v -= (caretbox.bot_right.v - vis_rect->bot_right.v - 1);
			else
				break;
		}

		UnuseMemory(pg->vis_area);
	}

	pg->procs.cursor_proc(pg, select, restore_cursor);
	UnuseMemory(pg->select);
}



