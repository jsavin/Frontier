/* This file contains miscellaneous utilities for text, styles and chars. If you
don't call anything here you can omit this file from the app.  */

#include "Paige.h"

#ifdef MAC_PLATFORM
#pragma segment pgtxtutls
#endif

#include "defprocs.h"
#include "machine.h"
#include "pgDefStl.h"
#include "pgText.h"
#include "pgSelect.h"
#include "pgUtils.h"
#include "pgEdit.h"
#include "pgTxtWid.h"
#include "pgSubref.h"
#include "pgExceps.h"


static pg_boolean compare_mask (style_info_ptr compare_from, long compare_to, long mask, pg_boolean exclude);
static void append_range (memory_ref ref, select_pair_ptr range);


/* pgPtToChar returns a text offset that corresponds to point + offset_extra.
(The offset_extra pointer can be NULL).  */

PG_PASCAL (size_t) pgPtToChar (pg_ref pg, const co_ordinate_ptr point, const co_ordinate_ptr offset_extra)
{
	paige_rec_ptr			pg_rec;
	co_ordinate				the_point;
	t_select				selection;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	
	the_point = *point;
	if (offset_extra)
		pgAddPt(offset_extra, &the_point);
	
	pg_rec->port.scale.scale = -pg_rec->port.scale.scale;
	pgScalePt(&pg_rec->port.scale, offset_extra, &the_point);
	pg_rec->port.scale.scale = -pg_rec->port.scale.scale;
	
	pgAddPt(&pg_rec->scroll_pos, &the_point);
	pgFillBlock(&selection, sizeof(t_select), 0);
	pg_rec->procs.offset_proc(pg_rec, &the_point, FALSE, &selection);

	UnuseMemory(pg);
	
	return	selection.offset;
}


/* pgPtToStyleInfo returns the style_info record for the character that contains
point.  The info is returned in style.  If range is non-NULL it gets set to the
range of text for which this style applies.  The function result is the text
position for the character found containing point.   The function will always
return a style and position even if the point is way beyond text (in which case
the style for the last character is returned) or before text (which the first
style is returned). Note: Either style or range can be NULL if you don't need
those values.  The conversion info param can be 0 (style will be as if clicked),
or NO_HALFCHARS and/or NO_CHAR_ALIGN  */

PG_PASCAL (size_t) pgPtToStyleInfo (pg_ref pg, const co_ordinate_ptr point,
		short conversion_info, style_info_ptr style, select_pair_ptr range)
{
	paige_rec_ptr		pg_rec;
	co_ordinate			use_point;
	style_run_ptr		run;
	t_select			selection;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	
	pg_rec->procs.set_device(pg_rec, set_pg_device, &pg_rec->port, NULL);
	
	use_point = *point;

	if (pgPtInPageArea(pg_rec, &use_point, NULL, NULL)) {

		pgConvertInputPoint(pg_rec, &use_point, NULL, &use_point);
		pg_rec->procs.offset_proc(pg_rec, &use_point, conversion_info, &selection);
	}
	else
	if (use_point.v < pg_rec->doc_bounds.top_left.v)
		selection.offset = 0;
	else
		selection.offset = pg_rec->t_length;

	if (style || range) {
	
		run = pgFindStyleRun(pg_rec, selection.offset, NULL);

		if (style)
			GetMemoryRecord(pg_rec->t_formats, run->style_item, style);
		if (range) {
			
			range->begin = run->offset;

			if ((range->end = run[1].offset) > pg_rec->t_length)
				range->end = pg_rec->t_length;
		}
		
		UnuseMemory(pg_rec->t_style_run);
	}

 	pg_rec->procs.set_device(pg_rec, unset_pg_device, &pg_rec->port, NULL);

	UnuseMemory(pg);
	
	return	selection.offset;
}


/* pgTextboxDisplay draws pg to a specified target rectangle. The inside_pg param is used
to obtain the target device, etc (if MEM_NULL it is not used). If wrap_rect is non-NULL the text wraps
inside of it, otherwise the page area dimensions are used within pg. */

PG_PASCAL (void) pgTextboxDisplay (pg_ref pg, paige_rec_ptr target_pg, const rectangle_ptr target_box,
		const rectangle_ptr wrap_rect, short draw_mode)
{
	paige_rec_ptr			pg_rec;
	generic_var				old_device, old_var;
	shape_ref				vis_shape, wrap_shape, old_vis, old_page;
	rectangle				wrap_bounds, clip_box;
	co_ordinate				offset_extra;
	pg_scale_factor			old_scale, old_scale_factor;
	pg_region				old_scroll_rgn;
	long					old_flags;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	old_device = (generic_var)pg_rec->port.machine_ref;
	old_var = pg_rec->port.machine_var;
	old_flags = pg_rec->flags;
	old_vis = pg_rec->vis_area;
	old_page = pg_rec->wrap_area;
	old_scale = pg_rec->port.scale;
	old_scale_factor = pg_rec->scale_factor;
	old_scroll_rgn = pg_rec->port.scroll_rgn;

	pg_rec->flags &= (~CARET_BIT);
	pg_rec->flags |= (DEACT_BIT | NO_EDIT_BIT);
	clip_box = *target_box;

	if (target_pg) {

		pg_rec->port.access_ctr += 1;
		pg_rec->port.machine_ref = PG_LONGWORD(size_t)pgGetPlatformDevice(&target_pg->port);
		pg_rec->port.machine_var = target_pg->port.machine_var;
		pg_rec->port.scale = target_pg->port.scale;
		pg_rec->scale_factor = target_pg->scale_factor;
		pg_rec->port.scroll_rgn = target_pg->port.scroll_rgn;
		pg_rec->flags |= (target_pg->flags & PRINT_MODE_BIT);
		pgShapeBounds(target_pg->vis_area, &wrap_bounds);
		pgSectRect(target_box, &wrap_bounds, &clip_box);
	}

	vis_shape = pgRectToShape(pg_rec->globals->mem_globals, &clip_box);
	pg_rec->vis_area = vis_shape;
	pg_rec->port.clip_info.change_flags |= CLIP_PAGE_CHANGED;

	if (wrap_rect) {

		wrap_shape = pgRectToShape(pg_rec->globals->mem_globals, wrap_rect);
		pg_rec->wrap_area = wrap_shape;
		pgInvalSelect(pg, 0, pg_rec->t_length);
	}
	else
		wrap_shape = MEM_NULL;

	pgShapeBounds(pg_rec->wrap_area, &wrap_bounds);
	offset_extra.h = target_box->top_left.h - wrap_bounds.top_left.h;
	offset_extra.v = target_box->top_left.v - wrap_bounds.top_left.v;
	pg_rec->scroll_pos.h = pg_rec->scroll_pos.v = 0;
	pg_rec->flags |= SCALE_VIS_BIT;
	pgUpdateText(pg_rec, NULL, 0, pg_rec->t_length, MEM_NULL, &offset_extra, draw_mode, FALSE);

	pg_rec->flags = old_flags;
	pg_rec->port.machine_ref = (long)old_device;
	pg_rec->port.machine_var = old_var;
	pg_rec->vis_area = old_vis;
	pg_rec->wrap_area = old_page;
	pg_rec->port.scale = old_scale;
	pg_rec->scale_factor = old_scale_factor;
	pg_rec->port.scroll_rgn = old_scroll_rgn;

	if (target_pg) {
	
		--pg_rec->port.access_ctr;
		pgReleasePlatformDevice(&target_pg->port);
	}

	pgDisposeShape(vis_shape);
	
	if (wrap_shape)
		pgDisposeShape(wrap_shape);

	UnuseMemory(pg);
}



/* pgCharacterRect returns a rectangle that encloses the character at position
exactly.  If want_scrolled is TRUE the rect is scrolled to the appropriate
position (i.e., its coordinates are set to what they would be on the screen
if pg was drawn), otherwise the rect is not scrolled and instead relative to
the top-left of pg's wrap area. If want_scaled is TRUE the rectangle is scaled
according to the scale factor in pg, otherwise it is returned non scaled.
The function returns the baseline of the text. */

PG_PASCAL (short) pgCharacterRect (pg_ref pg, long position, pg_boolean want_scrolled,
		pg_boolean want_scaled, rectangle_ptr rect)
{
	paige_rec_ptr		pg_rec;
	text_block_ptr		block;
	style_info_ptr		offset_style;
	t_select			selection;
	memory_ref			locs_ref;
	long PG_FAR			*positions;
	size_t				char_bytes, local_begin, local_end;
	long				caret_pt;
	short				baseline;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	pg_rec->procs.set_device(pg_rec, set_pg_device, &pg_rec->port, NULL);

	selection.offset = pgFixOffset(pg_rec, position);
	
	selection.flags = 0;
	pgCalcSelect(pg_rec, &selection);
	block = pgFindTextBlock(pg_rec, selection.offset, NULL, TRUE, TRUE);
	
	caret_pt = pgRectFromSelection(pg_rec, block, &selection, rect, &baseline);
	rect->top_left.h = rect->bot_right.h = caret_pt;
	
	if (selection.offset < pg_rec->t_length) {
	
		offset_style = pgFindTextStyle(pg_rec, selection.offset);
		char_bytes = offset_style->char_bytes + 1;
		UnuseMemory(pg_rec->t_formats);
	
		local_begin = selection.offset - block->begin;
		local_end = local_begin + char_bytes;
	
		positions = pgGetCharLocs(pg_rec, block, &locs_ref, NULL);
		rect->bot_right.h = rect->top_left.h + (positions[local_end] - positions[local_begin]);

		UnuseMemory(locs_ref);
		pgReleaseCharLocs(pg_rec, locs_ref);
	}

	UnuseMemory(pg_rec->t_blocks);

	if (want_scrolled)
		pgOffsetRect(rect, -pg_rec->scroll_pos.h, -pg_rec->scroll_pos.v);
	if (want_scaled) {
		co_ordinate		offset_extra;
		
		offset_extra = pg_rec->scroll_pos;
		pgNegatePt(&offset_extra);
		
		if (!want_scrolled)
			offset_extra.h = offset_extra.v = 0;

		pgScaleRect(&pg_rec->port.scale, &offset_extra, rect);
	}

 	pg_rec->procs.set_device(pg_rec, unset_pg_device, &pg_rec->port, NULL);

	UnuseMemory(pg);
	
	return	baseline;
}



/* pgMaxTextBounds returns the smallest rectangle that will fit inside the
whole pg_ref. If paginate is TRUE the document is fully paginated if it needs to.
Note that pagination can slow it down if text is changed a lot, then again if
not fully paginated you won't always get an exact answer.  */

PG_PASCAL (void) pgMaxTextBounds (pg_ref pg, rectangle_ptr bounds, pg_boolean paginate)
{
	paige_rec_ptr				pg_rec;
	register point_start_ptr	starts;
	text_block_ptr				block;
	size_t						num_blocks;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	block = (text_block_ptr) UseMemory(pg_rec->t_blocks);
	
	if (!pg_rec->t_length)
		pgBlockMove(&block->bounds, bounds, sizeof(rectangle));
	else {
	
		pgFillBlock(bounds, sizeof(rectangle), 0);
	
		for (num_blocks = GetMemorySize(pg_rec->t_blocks); num_blocks; ++block, --num_blocks) {
	
			if ((block->flags & (NEEDS_CALC | NEEDS_PAGINATE)) && paginate)
				pgPaginateBlock(pg_rec, block, NULL, TRUE);

			if (!(block->flags & (NEEDS_CALC | ALL_TEXT_HIDDEN))) {
				
				for (starts = (point_start_ptr) UseMemory(block->lines); starts->flags != TERMINATOR_BITS; ++starts)
					pgUnionRect(&starts->bounds, bounds, bounds);
				
				UnuseMemory(block->lines);
			}
		}
	}
	
	UnuseMemory(pg_rec->t_blocks);
	UnuseMemory(pg);
}


/* pgCharType returns information about the character at offset. The information
will be one of the character type bits set (see Paige.h). The offset can be
CURRENT_POSITION.  Only mask_bits are considered. */

PG_PASCAL (long) pgCharType (pg_ref pg, long offset, long mask_bits)
{
	paige_rec_ptr		pg_rec;
	style_walk			walker;
	text_block_ptr		block;
	size_t				use_offset;
	long				result;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	
	use_offset = pgFixOffset(pg_rec, offset);
	block = pgFindTextBlock(pg_rec, use_offset, NULL, FALSE, TRUE);

	pgPrepareStyleWalk(pg_rec, use_offset, &walker, FALSE);
	
	result = walker.cur_style->procs.char_info(pg_rec, &walker, (pg_char_ptr) UseMemory(block->text),
			block->begin, 0, block->end - block->begin, use_offset - block->begin, mask_bits);
	
	UnuseMemory(block->text);
	UnuseMemory(pg_rec->t_blocks);
	
	pgPrepareStyleWalk(pg_rec, 0, NULL, FALSE);

	UnuseMemory(pg);
	
	return	result;
}


/* pgCharByte returns the byte(s) that constitute the character at offset. Since
offset might point to the middle of a multi-byte character, this function
adjusts accordingly and returns the full character in char_bytes. The function
result is the number of bytes placed in the pointer. If char_bytes is NULL the
function simply returns how many bytes the character is.   */

PG_PASCAL (pg_short_t) pgCharByte (pg_ref pg, size_t offset, pg_char_ptr char_bytes)
{
	paige_rec_ptr		pg_rec;
	style_walk			walker;
	text_block_ptr		block;
	pg_char_ptr			text;
	size_t				use_offset, c_info, byte_position, begin_offset, end_offset;
	pg_short_t			result;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	
	use_offset = pgFixOffset(pg_rec, offset);
	result = 0;

	if (use_offset < pg_rec->t_length) {

		block = pgFindTextBlock(pg_rec, use_offset, NULL, FALSE, TRUE);
		
		pgPrepareStyleWalk(pg_rec, use_offset, &walker, FALSE);
		text = (pg_char_ptr) UseMemory(block->text);
		end_offset = block->end - block->begin;

		while (walker.cur_style->procs.char_info(pg_rec, &walker, text,
				block->begin, 0, end_offset, use_offset - block->begin, LAST_HALF_BIT | MIDDLE_CHAR_BIT))
			pgWalkStyle(&walker, -1);

		use_offset = walker.current_offset;
		byte_position = use_offset - block->begin;
		begin_offset = byte_position;
		
		for (;;) {
	
			if (char_bytes)
				char_bytes[result] = text[byte_position];
			
			c_info = walker.cur_style->procs.char_info(pg_rec, &walker, text,
				block->begin, begin_offset, end_offset, byte_position, FIRST_HALF_BIT | LAST_HALF_BIT | MIDDLE_CHAR_BIT);
			
			++result;
			++byte_position;
			++use_offset;
	
			if ((use_offset == block->end) || (c_info & LAST_HALF_BIT)
				|| !(c_info & (FIRST_HALF_BIT | LAST_HALF_BIT | MIDDLE_CHAR_BIT)))
			break;

			pgWalkStyle(&walker, 1);
		}
		
		UnuseMemory(block->text);
		UnuseMemory(pg_rec->t_blocks);
		
		pgPrepareStyleWalk(pg_rec, 0, NULL, FALSE);
	}

	UnuseMemory(pg);
	
	return	result;
}


/* pgFindCharType locates the first character whose info type & char_info is
non-zero beginning at (and including) *offset. If a match is found, *offset
is updated to the byte offset and the actual info flags are returned. A zero
result implies no match.  If the_byte is non-NULL the byte found is returned. */

PG_PASCAL (long) pgFindCharType (pg_ref pg, long char_info, size_t PG_FAR *offset,
		pg_char_ptr the_byte)
{
	paige_rec_ptr		pg_rec;
	style_walk			walker;
	text_block_ptr		block;
	pg_char_ptr			text;
	size_t				use_offset, byte_position, begin_offset, end_offset;
	long				c_info;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	
	use_offset = pgFixOffset(pg_rec, *offset);
	c_info = 0;

	if (use_offset < pg_rec->t_length) {

		block = pgFindTextBlock(pg_rec, use_offset, NULL, FALSE, TRUE);

		pgPrepareStyleWalk(pg_rec, use_offset, &walker, FALSE);
		text = (pg_char_ptr) UseMemory(block->text);
		byte_position = use_offset - block->begin;
		begin_offset = byte_position;
		end_offset = block->end - block->begin;

		for (;;) {
			
			c_info = walker.cur_style->procs.char_info(pg_rec, &walker, text,
				block->begin, begin_offset, end_offset, byte_position, char_info);
			
			if (c_info & char_info) {
				
				if (the_byte)
					*the_byte = text[byte_position];

				break;
			}

			c_info = 0;

			++use_offset;
			++byte_position;

			if (use_offset == pg_rec->t_length)
				break;
				
			pgWalkStyle(&walker, 1);
			
			if (use_offset == block->end) {
				
				UnuseMemory(block->text);
				++block;
				pg_rec->procs.load_proc(pg_rec, block);

				text = (pg_char_ptr) UseMemory(block->text);
				byte_position = 0;
			}
		}

		UnuseMemory(block->text);
		UnuseMemory(pg_rec->t_blocks);
		pgPrepareStyleWalk(pg_rec, 0, NULL, FALSE);
	}

	*offset = use_offset;

	UnuseMemory(pg);
	
	return	c_info;
}

/* pgMaskedSelection is a new 2.2 function that returns a selection list which masks certain
styles (or not). All the non-zero fields in mask_styles are compare and, if they match,
are included in the selection pairs (if exclude is false) or include (if exclude is true).
If there is no selection, a "delete" is assumed, hence it finds the first matching character
prior to the selection. */

PG_PASCAL (memory_ref) pgMaskedSelection (pg_ref pg, const select_pair_ptr range,
		long compare_styles, long mask_styles, pg_boolean exclude)
{
	paige_rec_ptr			pg_rec;
	style_walk				walker;
	size_t					begin_select, end_select, next_position;
	select_pair				current_range;
	memory_ref				result = MEM_NULL;
	
	pg_rec = (paige_rec_ptr) UseMemory(pg);
	
	if (range == NULL)
		pgGetSelection(pg, &begin_select, &end_select);
	else {
		
		begin_select = range->begin;
		end_select = range->end;
	}
	
	pgPrepareStyleWalk(pg_rec, begin_select, &walker, FALSE);
	result = MemoryAlloc(pg_rec->globals->mem_globals, sizeof(select_pair), 0, 4);
	current_range.begin = current_range.end = begin_select;

	if (begin_select == end_select) {
		
		while (current_range.begin > 0) {

			next_position = current_range.begin - 1;
			pgSetWalkStyle(&walker, next_position);
			
			if (compare_mask(walker.cur_style, compare_styles, mask_styles, exclude)) {
				
				current_range.begin -= (walker.cur_style->char_bytes + 1);
				break;
			}
			
			current_range.begin = current_range.end = walker.prev_style_run->offset;
		}
	}
	else {
		
		while (current_range.end < end_select) {
			
			next_position = walker.next_style_run->offset;

			if (next_position > end_select)
				next_position = end_select;
			
			if (compare_mask(walker.cur_style, compare_styles, mask_styles, exclude))
				current_range.end = next_position;
			else {
				
				append_range(result, &current_range);
				current_range.begin = current_range.end = next_position;
			}

			pgSetWalkStyle(&walker, next_position);
		}
	}
	
	append_range(result, &current_range);
	pgPrepareStyleWalk(pg_rec, begin_select, NULL, FALSE);

	UnuseMemory(pg);
	
	if (GetMemorySize(result) == 0) {
		
		DisposeMemory(result);
		return	MEM_NULL;
	}
	
	return		result;
}


/* pgExamineText returns the actual pointer of text from specified offset.
The length will never cross text block boundaries The intended purpose of
pgExamineText is to look at a few bytes in the text stream or do searches, etc.
Effectively, the length result will always be the distance from offset to
end of the home text block, in bytes.

The offset can be CURRENT_POSITION (-1) as well as a positive number.

Offset can be a huge number, greater than all text, and this function will
adjust it.  In any case, non-existent text (offsets >= total text size) will
return NULL.   */

PG_PASCAL (pg_char_ptr) pgExamineText (pg_ref pg, long offset, text_ref *text,
		size_t PG_FAR *length)
{
	paige_rec_ptr					pg_rec;
	register pg_char_ptr			ptr_result;
	register long					wanted_offset;

	pg_rec = (paige_rec_ptr) UseMemory(pg);

	wanted_offset = pgFixOffset(pg_rec, offset);

	ptr_result = pgTextFromOffset(pg_rec, wanted_offset, text, length);

	UnuseMemory(pg);

	return	ptr_result;
}



/* pgPaginateNow forces a "pagination" to occur in pg up to paginate_to. If
use_best_guess is TRUE the line height(s) are only guessed at for maximum speed,
otherwise complete calculation is performed.  */

PG_PASCAL (void) pgPaginateNow (pg_ref pg, long paginate_to, pg_boolean use_best_guess)
{
	paige_rec_ptr			pg_rec;
	text_block_ptr			block;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	
	block = pgFindTextBlock(pg_rec, pgFixOffset(pg_rec, paginate_to), NULL, FALSE, TRUE);
	
	while (!(block->flags & (NEEDS_CALC | NEEDS_PAGINATE))) {
		
		if (block->begin == 0)
			break;
		
		block -= 1;
	}

	pgPaginateBlock(pg_rec, block, (smart_update_ptr)NULL, (pg_boolean)!use_best_guess);
	UnuseMemory(pg_rec->t_blocks);

	UnuseMemory(pg);
}

/* pgSetPageColor sets the page area background color. */

PG_PASCAL (void) pgSetPageColor (pg_ref pg, const color_value_ptr color)
{
	paige_rec_ptr		pg_rec;
	
	pg_rec = (paige_rec_ptr) UseMemory(pg);
	pg_rec->bk_color = *color;
	++pg_rec->change_ctr;

	UnuseMemory(pg);
}


/* pgSetPageColor returns the page area background color. */

PG_PASCAL (void) pgGetPageColor (pg_ref pg, color_value_ptr color)
{
	paige_rec_ptr		pg_rec;
	
	pg_rec = (paige_rec_ptr) UseMemory(pg);
	*color = pg_rec->bk_color;
	UnuseMemory(pg);
}


/* pgGetSelectionList returns a new memory_ref containing all the offset pairs
representing the selection. This will be a memory_ref holding an array of
select_pair records.  However, if pg has a single insertion, this function
will return NULL.  If for_paragraph is TRUE, the resulting offset pairs will
enclose paragraphs (which means even a single insertion will return an offset
pair enclosing the current paragraph).  */

PG_PASCAL (memory_ref) pgGetSelectionList (pg_ref pg, pg_boolean for_paragraph)
{
	paige_rec_ptr				pg_rec;
	memory_ref					result_ref;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	result_ref = pgSetupOffsetRun(pg_rec, NULL, for_paragraph, FALSE);
	UnuseMemory(pg);

	return	result_ref;
}


/* pgSetSelectionList sets the selection range of pg to match the offset pairs
contained in select_list.  If show_hilite is TRUE the highlight is drawn.  */

PG_PASCAL (void) pgSetSelectionList (pg_ref pg, memory_ref select_list,
		long extra_offset, pg_boolean show_hilite)
{
	paige_rec_ptr					pg_rec;
	register select_pair_ptr		offset_pairs;
	register t_select_ptr			selections;
	register pg_short_t				select_qty;
	pg_boolean						will_draw;
	
	if (select_list == MEM_NULL)
		return;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	will_draw = show_hilite && ((pg_rec->flags & DEACT_BITS) == 0); 
	
	pgRemoveAllHilites(pg_rec, will_draw);

	select_qty = pg_rec->num_selects = (pg_short_t)GetMemorySize(select_list);
	SetMemorySize(pg_rec->select, (select_qty * 2) + MINIMUM_SELECT_MEMSIZE);
	
	selections = (t_select_ptr) UseMemory(pg_rec->select);
	offset_pairs = (select_pair_ptr) UseMemory(select_list);
	
	while (select_qty) {
		
		selections->offset = pgFixOffset(pg_rec, offset_pairs->begin + extra_offset);
		selections->flags = SELECTION_DIRTY;
		++selections;
		selections->offset = pgFixOffset(pg_rec, offset_pairs->end + extra_offset);
		selections->flags = SELECTION_DIRTY;

		++selections;
		++offset_pairs;
		--select_qty;
	}

	UnuseMemory(pg_rec->select);
	UnuseMemory(select_list);
	
	if (will_draw) {

		pgSetupGrafDevice(pg_rec, &pg_rec->port, MEM_NULL, clip_standard_verb);
		pgDrawHighlight(pg_rec, hide_cursor);
		pg_rec->procs.set_device(pg_rec, unset_pg_device, &pg_rec->port, NULL);
	}

	UnuseMemory(pg);
}


/* pgGetHiliteRgn sets rgn to the highlight region for range (if non-NULL) or
select_list (if non-NULL). If both range and select_list are NULL then the
current highlight range is used.  */

PG_PASCAL (pg_boolean) pgGetHiliteRgn (pg_ref pg, const select_pair_ptr range,
		memory_ref select_list, shape_ref rgn)
{
	paige_rec_ptr				pg_rec;
	register pg_short_t			qty, ctr;
	register t_select_ptr		selection_ptr;
	register select_pair_ptr	pairs_ptr;
	memory_ref					selections;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	
	if (!range && !select_list) {
		
		pgBuildHiliteRgn(pg_rec, (t_select_ptr)UseMemory(pg_rec->select), pg_rec->num_selects,
					pg_rec->hilite_rgn);
		MemoryCopy(pg_rec->hilite_rgn, rgn);

		UnuseMemory(pg_rec->select);
	}
	else {
		
		if (range) {
			
			qty = 2;
			selections = MemoryAlloc(pg_rec->globals->mem_globals, sizeof(t_select), 2, 0);
			selection_ptr = (t_select_ptr) UseMemory(selections);
			selection_ptr->offset = range->begin;
			selection_ptr[1].offset = range->end;
		}
		else {
			
			qty = (pg_short_t)(GetMemorySize(select_list) * 2);
			selections = MemoryAlloc(pg_rec->globals->mem_globals, sizeof(t_select), qty, 0);
			selection_ptr = (t_select_ptr) UseMemory(selections);
			pairs_ptr = (select_pair_ptr) UseMemory(select_list);
			
			for (ctr = 0; ctr < qty; ++pairs_ptr) {
				
				selection_ptr[ctr].offset = pairs_ptr->begin;
				++ctr;
				selection_ptr[ctr].offset = pairs_ptr->end;
				++ctr;
			}
			
			UnuseMemory(select_list);
		}
		
		for (ctr = 0; ctr < qty; ++ctr)
			selection_ptr[ctr].flags = SELECTION_DIRTY;
		
		pgBuildHiliteRgn(pg_rec, selection_ptr, (pg_short_t)(qty / 2), rgn);
		
		UnuseAndDispose(selections);
	}

	pgOffsetShape(rgn, -pg_rec->scroll_pos.h, -pg_rec->scroll_pos.v);

	UnuseMemory(pg);

	return	!pgEmptyShape(rgn);
}


/* pgInvalShapes tells PAIGE that the page area and/or exclusion area has
changed. This is a lower level support function for the application. */

PG_PASCAL (void) pgInvalShapes (pg_ref pg, pg_boolean inval_page, pg_boolean inval_exclusion)
{
	paige_rec_ptr			pg_rec;
	
	pg_rec = (paige_rec_ptr) UseMemory(pg);
	
	if (inval_page || inval_exclusion) {

		++pg_rec->change_ctr;

		pgInvalSelect(pg, 0, pg_rec->t_length);
		pg_rec->flags |= (DOC_BOUNDS_DIRTY);
		
		if (inval_page)
			pgResetDocBounds(pg_rec);
	}

	UnuseMemory(pg);
}


/* pgGetEndingPage returns the ending page of the document. If page_rect is non-NULL,
it is filled with the physical rect that encloses that page. */

PG_PASCAL (long) pgGetEndingPage (pg_ref pg, rectangle_ptr page_rect)
{
	paige_rec_ptr			pg_rec;
	text_block_ptr			block;
	co_ordinate				page_point, repeat_offset;
	long					page_num;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	block = (text_block_ptr) UseMemoryRecord(pg_rec->t_blocks, GetMemorySize(pg_rec->t_blocks) - 1, USE_ALL_RECS, TRUE);
	pgPaginateBlock(pg_rec, block, (smart_update_ptr)NULL, FALSE);
	page_point.h = block->bounds.top_left.h;
	page_point.v = block->bounds.bot_right.v - 1;
	UnuseMemory(pg_rec->t_blocks);

	page_num = (long)pgPixelToPage(pg, &page_point, &repeat_offset,
			NULL, NULL, NULL, FALSE);
	page_num += 1;
	
	if (page_rect) {
		
		pgShapeBounds(pg_rec->wrap_area, page_rect);
		pgOffsetRect(page_rect, repeat_offset.h, repeat_offset.v);
	}

	UnuseMemory(pg);
	
	return		page_num;
}


/* pgSetCaretPosition is a way to set the cursor to various common places. */

PG_PASCAL (void) pgSetCaretPosition (pg_ref pg, pg_short_t position_verb, pg_boolean show_caret)
{
	paige_rec_ptr		pg_rec;
	text_block_ptr		block;
	point_start_ptr		starts;
	t_select			temp_select;
	t_select_ptr		select;
	pg_short_t			use_position_verb, extend_flag;
	size_t				left_word, right_word, position;
	long				original_position;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	pgSetupGrafDevice(pg_rec, &pg_rec->port, MEM_NULL, clip_standard_verb);

	use_position_verb = position_verb & CARET_VERB_MASK;
	extend_flag = position_verb & EXTEND_CARET_FLAG;

	original_position = position = pgCurrentInsertion(pg_rec);

	if (show_caret && pg_rec->num_selects && !(position_verb & EXTEND_CARET_FLAG))
		pgRemoveAllHilites(pg_rec, TRUE);
	else
	if ((pg_rec->flags & (CARET_BIT | CARET_CREATED_BIT)) && !pg_rec->num_selects)
		pgSetCursorState(pg, hide_cursor);

	switch (use_position_verb) {

		case home_caret:
			if (extend_flag)
				pgExtendSelection(pg, -position, EXTEND_MOD_BIT, show_caret);
			else {
			
				pgSetSelection(pg, 0, 0, 0, FALSE);

				if (show_caret)
					pgSetCursorState(pg, show_cursor);
			}
			break;

		case doc_bottom_caret:
			if (extend_flag && position < pg_rec->t_length)
				pgExtendSelection(pg, pg_rec->t_length - position, EXTEND_MOD_BIT, show_caret);
			else {
			
				pgSetSelection(pg, pg_rec->t_length, pg_rec->t_length, 0, FALSE);

				if (show_caret)
					pgSetCursorState(pg, show_cursor);
			}
			break;

		case begin_line_caret:
		case end_line_caret:
			select = (t_select_ptr) UseMemory(pg_rec->select);
			
			if (use_position_verb == end_line_caret && extend_flag) {
			
				select += 1;
				position = select->offset;
			}
			
			if (select->flags & SELECTION_DIRTY)
				pgCalcSelect(pg_rec, select);
			
			if (extend_flag) {
				
				temp_select = *select;
				select = &temp_select;
			}

			block = pgFindTextBlock(pg_rec, select->offset, NULL, FALSE, TRUE);
			starts = (point_start_ptr) UseMemory(block->lines);
			starts += select->line;
			
			if (use_position_verb == begin_line_caret) {
				
				original_position = select->offset;
				
				for (;;) {
				
					while (!(starts->flags & NEW_LINE_BIT)) {
						
						--starts;
						--select->line;
					}
					
					select->secondary_caret = select->primary_caret = 0;
					select->offset = starts->offset;
					select->offset += block->begin;
					select->flags &= (~END_LINE_FLAG);

					if (!extend_flag || select->offset < original_position || !select->offset)
						break;
					
					select->offset -= 1;
					UnuseMemory(block->lines);
					UnuseMemory(pg_rec->t_blocks);
					pgCalcSelect(pg_rec, select);

					block = pgFindTextBlock(pg_rec, select->offset, NULL, FALSE, TRUE);
					starts = (point_start_ptr) UseMemory(block->lines);
					starts += select->line;
				}
			}
			else {
				
				original_position = select->offset;

				for (;;) {
				
					while (!(starts->flags & LINE_BREAK_BIT)) {
						
						if (starts[1].flags == TERMINATOR_BITS)
							break;
							
						++starts;
						++select->line;
					}
	
					select->secondary_caret = select->primary_caret = (starts->bounds.bot_right.h - starts->bounds.top_left.h);
					select->offset = starts[1].offset;
					
					if ((starts->flags & HARD_BREAK_BITS) && select->offset) {
					
						select->offset -= 1;
						select->flags &= (~END_LINE_FLAG);
					}
					else
						select->flags |= END_LINE_FLAG;
	
					select->offset += block->begin;
					
					if (!extend_flag || select->offset > original_position || select->offset == pg_rec->t_length)
						break;
			
		// New selection did not advance, in this increment by 1.
		// Probably it is sitting on CR.

					UnuseMemory(block->lines);
					UnuseMemory(pg_rec->t_blocks);
					select->offset += 1;
					pgCalcSelect(pg_rec, select);
					block = pgFindTextBlock(pg_rec, select->offset, NULL, FALSE, TRUE);
					starts = (point_start_ptr) UseMemory(block->lines);
					starts += select->line;
				}
				
				pgCalcSelect(pg_rec, select);
			}
			
			if (!extend_flag) {
			
				select[1] = select[0];
				pg_rec->num_selects = 0;
			}
			
			UnuseMemory(block->lines);
			UnuseMemory(pg_rec->select);
			UnuseMemory(pg_rec->t_blocks);

			if (extend_flag)
				pgExtendSelection(pg, temp_select.offset - position, EXTEND_MOD_BIT, show_caret);
			else 
			if (show_caret)
				pgSetCursorState(pg, show_cursor);			
			break;

		case next_word_caret:
			if (extend_flag && pg_rec->num_selects) {
				
				select = (t_select_ptr) UseMemory(pg_rec->select);
				position = original_position = select[1].offset;
				UnuseMemory(pg_rec->select);
			}

			position += 1;
			
			if (position < pg_rec->t_length)
				if (pgCharType(pg, position, LAST_HALF_BIT))
					position += 1;

			pgFindCtlWord(pg, position, &left_word, &right_word, FALSE);
			
			if (left_word != position) {
			
				pgFindWord(pg, position, &left_word, &right_word, FALSE, TRUE);
				position = right_word;
				pgFindWord(pg, position, &left_word, &right_word, FALSE, FALSE);
				
				if (position >= right_word)
					left_word = right_word;
			}

			if (extend_flag)
				pgExtendSelection(pg, left_word - original_position, EXTEND_MOD_BIT, show_caret);
			else
				pgSetSelection(pg, left_word, left_word, 0, show_caret);
			break;

		case previous_word_caret:
			pgFindCtlWord(pg, position, &left_word, &right_word, FALSE);
			position -= 1;

			if (position > 0)
				if (pgCharType(pg, position, LAST_HALF_BIT))
					position -= 1;

			if (position != left_word)
				pgFindWord(pg, position, &left_word, &right_word, TRUE, FALSE);
			
			if (extend_flag)
				pgExtendSelection(pg, left_word - original_position, EXTEND_MOD_BIT, show_caret);
			else
				pgSetSelection(pg, left_word, left_word, 0, show_caret);

			break;
	}
	
	pgSetNextInsertIndex(pg_rec);
	pg_rec->procs.set_device(pg_rec, unset_pg_device, &pg_rec->port, NULL);

	UnuseMemory(pg);
}


/* pgIsPaigeUnicode returns TRUE if the lib is unicode-enabled.
(Source hacker note: As tempting as it is, do NOT make this function an inline
macro, otherwise there will be no way to find out of a PAIGE runtime library is
Unicode-enabled or not). */

PG_PASCAL (pg_boolean) pgIsPaigeUnicode (void)
{
	return	(pg_boolean)(sizeof(pg_char) == 2);
}


#ifdef UNICODE

/* pgInsertBytes is the same as pgInsert() except the data is known to be 8-bit ASCII. This
function converts the data to 16 bit ASCII and then inserts it. */

PG_PASCAL (pg_boolean) pgInsertBytes (pg_ref pg, const pg_bits8_ptr data, long length, long position,
		short insert_mode, short modifiers, short draw_mode)
{
	paige_rec_ptr			pg_rec;
	pgm_globals_ptr			mem_globals;
	pg_short_t*				chars;
	style_info_ptr			insert_style;
	font_info_ptr			insert_font;
	long					character_count;
	volatile memory_ref		temp_ref;
	pg_boolean				result = FALSE;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	mem_globals = pg_rec->globals->mem_globals;
	temp_ref = MEM_NULL;

	PG_TRY(mem_globals) {
		
		temp_ref = MemoryAlloc(mem_globals, sizeof(pg_char), length, 0);
		chars = (pg_short_t*) UseMemory(temp_ref);

		insert_style = (style_info_ptr) UseMemoryRecord(pg_rec->t_formats, (long)pg_rec->insert_style, 0, TRUE);
		insert_font = (font_info_ptr) UseMemoryRecord(pg_rec->fonts, (long)insert_style->font_index, 0, TRUE);
		
		character_count = insert_style->procs.bytes_to_unicode(data, chars, insert_font, length);
		
		result = pgInsert(pg, (const pg_char_ptr) chars, character_count, position, insert_mode, modifiers, draw_mode);
		UnuseMemory(temp_ref);
	};
	
	PG_CATCH {
		
		if (temp_ref)
			DisposeFailedMemory(temp_ref);
		
		temp_ref = MEM_NULL;
	};
	
	PG_ENDTRY;
	
	DisposeNonNilMemory(temp_ref);
	UnuseMemory(pg);
	
	return		result;
}

#endif


/******************************* Local Functions  ***************************/

/* compare_mask compares two style_info records based on the mask (which fields to test).
If the match, true is returned. */

static pg_boolean compare_mask (style_info_ptr compare_from, long compare_to, long mask, pg_boolean exclude)
{
	long		style_compare;

	style_compare = PaigeToQDStyle(compare_from);
	style_compare &= mask;
	
	if (style_compare != compare_to)
		return	exclude;
	
	return		!exclude;
}

/* append_range adds range if range->end > range->begin. */

static void append_range (memory_ref ref, select_pair_ptr range)
{
	if (range->end > range->begin) {
		select_pair_ptr		range_ptr;
		
		range_ptr = (select_pair_ptr) AppendMemory(ref, 1, FALSE);
		*range_ptr = *range;
		UnuseMemory(ref);
	}
}


