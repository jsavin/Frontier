/* This section handles all selection and highlighting.  */

#include "Paige.h"

#ifdef MAC_PLATFORM
#pragma segment pgbasic1
#endif

#include "pgEdit.h"
#include "machine.h"
#include "pgUtils.h"
#include "defprocs.h"
#include "pgText.h"
#include "pgShapes.h"
#include "pgSelect.h"
#include "pgScript.h"
#include "pgDefStl.h"
#include "pgDefPar.h"
#include "pgTxtWid.h"
#include "pgSubref.h"
#include "pgLists.h"
#include "pgHText.h"
#include "pgTables.h"


static select_ref hilite_avoid_list (paige_rec_ptr pg, t_select_ptr cur_select,
		pg_short_t cur_sel_qty);
static void insert_select_rects (paige_rec_ptr pg, shape_ref rgn,
			t_select_ptr selection);
static void get_wrap_rect (paige_rec_ptr pg, rectangle_ptr wrap_base, 
		long r_num, t_select_ptr from_selection, rectangle_ptr wrap_rect);
static short pair_intersects (select_pair_ptr pair, t_select_ptr cur_select,
		pg_short_t cur_sel_qty);
static t_select_ptr current_selection_pair (paige_rec_ptr pg);
static void reverse_selection (t_select_ptr selection);
static void return_word_boundary (paige_rec_ptr pg, short modifiers, long offset,
		long PG_FAR *begin, long PG_FAR *end, pg_boolean left_side);
static long track_style_control (paige_rec_ptr pg, t_select_ptr new_select,
	long track_refcon, short modifiers, short verb);
static void update_hilite_rgn (paige_rec_ptr pg);
static pg_boolean is_selection_dirty (paige_rec_ptr pg);
static void extend_selection (paige_rec_ptr pg, t_select_ptr new_select,
		short modifiers, short drag_verb, short show_new_caret, short should_draw);
static void fix_restricted_hilite (paige_rec_ptr pg, co_ordinate_ptr point);
static void compute_vertical_corners (paige_rec_ptr pg, t_select_ptr selections);
static void add_to_text_rect (paige_rec_ptr pg, select_pair_ptr selection,
		rectangle_ptr rect, short PG_FAR *first_baseline);
static void point_to_start (paige_rec_ptr pg, co_ordinate_ptr point,
		pg_short_t PG_FAR *block_num, pg_short_t PG_FAR *start_num);
static pg_boolean point_to_exact_start (paige_rec_ptr pg, text_block_ptr block, co_ordinate_ptr point,
			long required_r_num, pg_short_t PG_FAR *start_num, pg_boolean must_find_one);
static pg_boolean whole_doc_hidden (paige_rec_ptr pg);
static long hyperlink_callback (paige_rec_ptr pg, short verb, short modifiers, memory_ref hyperlinks);


/* pgExamineClickProc is the standard "examine the last mouse-click" function.
This default checks for hypertext link */

PG_PASCAL (void) pgExamineClickProc (paige_rec_ptr pg, short click_verb,
		short modifiers, long refcon_return, t_select_ptr begin_select,
		t_select_ptr end_select)
{

}


/* DEFAULT "MAKE HIGHLIGHT REGION" FUNCTION. This is called to return a shape
which is an outline of selected text. Note it is not machine-dependent until
it draws. */

PG_PASCAL (void) pgHiliteProc (paige_rec_ptr pg, t_select_ptr selections,
		pg_short_t select_qty, shape_ref rgn)
{
	register t_select_ptr	select_pairs;
	register pg_short_t		qty;
	long					did_calc;

	pgSetShapeRect(rgn, NULL);
	
	qty = select_qty;
	select_pairs = selections;

	while (qty) {

		if (select_pairs->flags & VERTICAL_FLAG) {
		
			if (did_calc = (select_pairs->flags & SELECTION_DIRTY) )
				pgCalcSelect(pg, select_pairs);
			
			if (select_pairs[1].flags & SELECTION_DIRTY) {
			
				did_calc = TRUE;
				pgCalcSelect(pg, &select_pairs[1]);
			}
			
			if (did_calc)
				compute_vertical_corners(pg, select_pairs);
		}

		insert_select_rects(pg, rgn, select_pairs);
		select_pairs += 2;
		--qty;
	}

	ForceMinimumMemorySize(rgn);
}



/* DEFAULT FUNCTION TO RETURN A TEXT OFFSET MATCHING A POINT. */

PG_PASCAL (void) pgPt2OffsetProc (paige_rec_ptr pg, co_ordinate_ptr point,
		short conversion_info, t_select_ptr selection)
{
	text_block_ptr				block;
	register point_start_ptr	starts;
	register long PG_FAR		*char_locs;
	register short				control_style;
	style_info_ptr				begin_style;
	rectangle					bounds_check;
	co_ordinate					local_point;
	memory_ref					special_locs;
	long						j_extra, running_h, running_offset;
	long						closest_h, abs_h, abs_offset;
	pg_short_t					text_size, start_num, block_num, block_qty;
	short						system_direction, no_half_chars;

	system_direction = pgSystemDirection(pg->globals);
	no_half_chars = conversion_info & NO_HALFCHARS;

	block = (text_block_ptr) UseMemory(pg->t_blocks);
	block_qty = (pg_short_t)GetMemorySize(pg->t_blocks);

	for (block_num = 0; block_num < block_qty; ++block, ++block_num) {

		pgPaginateBlock(pg, block, NULL, FALSE);
		
		if (!(block->flags & ALL_TEXT_HIDDEN))
			if (block->bounds.top_left.v > point->v)
				break;
	}

	if (whole_doc_hidden(pg)) {
		
		pgFillBlock(selection, sizeof(t_select), 0);
		selection->original_pt = local_point = *point;
		UnuseMemory(pg->t_blocks);
		
		return;
	}
	
	pgShapeBounds(pg->wrap_area, &bounds_check);
	bounds_check.bot_right.v = pg->doc_bounds.bot_right.v;
	
	if (pg->subset_focus) {
		paige_sub_ptr		sub_ptr;
		
		sub_ptr = (paige_sub_ptr) UseMemory(pg->subset_focus);
		bounds_check = sub_ptr->subset_bounds;
		UnuseMemory(pg->subset_focus);
	}
	else
	if (GetMemorySize(pg->wrap_area) > SIMPLE_SHAPE_QTY) {
		rectangle				end_rect;
		co_ordinate				repeat_offset;
		pg_short_t				wrap_index;

		if (!(pg->doc_info.attributes & (V_REPEAT_BIT | H_REPEAT_BIT)))
			pgShapeBounds(pg->wrap_area, &bounds_check);
		else {
		
			block = (text_block_ptr) UseMemoryRecord(pg->t_blocks, block_qty - 1, 0, FALSE);
			
			while ((block->flags & (BELOW_CONTAINERS | ALL_TEXT_HIDDEN))
				&& block->begin)
				--block;
			
			if (!(block->flags & ANY_CALC)) {
				
				wrap_index = pgGetWrapRect(pg, block->end_start.r_num, &repeat_offset);
				GetMemoryRecord(pg->wrap_area, wrap_index + 1, &end_rect);
				
				pgOffsetRect(&end_rect, repeat_offset.h, repeat_offset.v);
				
				if (pgPtInRect(point, &end_rect)) {
					
					bounds_check = end_rect;
					bounds_check.bot_right.v = block->end_start.bounds.bot_right.v;
				}
			}
		}
	}

	if (point->v > bounds_check.bot_right.v) {
		
		point->v = bounds_check.bot_right.v - 1;

		if (system_direction == right_left_direction)
			point->h = bounds_check.top_left.h + 1;
		else
			point->h = bounds_check.bot_right.h - 1;
	}
	else
	if (point->v < bounds_check.top_left.v) {
		
		point->v = bounds_check.top_left.v + 1;

		if (system_direction == right_left_direction)
			point->h = bounds_check.bot_right.h - 1;
		else
			point->h = bounds_check.top_left.h + 1;
	}

	if (point->h > bounds_check.bot_right.h)
		point->h = bounds_check.bot_right.h;
	else
	if (point->h < bounds_check.top_left.h)
		point->h = bounds_check.top_left.h;

	UnuseMemory(pg->t_blocks);

	selection->flags = 0;
	selection->original_pt = local_point = *point;
	
	point_to_start(pg, point, &block_num, &start_num);

	block = (text_block_ptr) UseMemoryRecord(pg->t_blocks, block_num, USE_ALL_RECS, TRUE);
	
	pgPaginateBlock(pg, block, NULL, TRUE);
	starts = (point_start_ptr) UseMemoryRecord(block->lines, start_num, 0, TRUE);
	selection->line = start_num;

	if (starts->cell_num)
		selection->flags |= SELECT_IN_TABLE;

	special_locs = MEM_NULL;

	abs_offset = block->begin;
	abs_offset += (long) starts->offset;
	selection->control_offset = abs_offset;

	selection->flags = 0;
	begin_style = pgFindTextStyle(pg, abs_offset);
	if (begin_style->class_bits & STYLE_IS_CONTROL) {
	
		control_style = TRUE;
		selection->flags = SELECT_ON_CTL;
	}
	else
		control_style = FALSE;
	
	UnuseMemory(pg->t_formats);

	if (abs_offset == pg->t_length) {
		
		selection->offset = abs_offset;
		selection->primary_caret = 0;
	}
	else {
		
		text_size = starts[1].offset - starts->offset;
	
		running_offset = block->begin;
		running_offset += (long) starts->offset;
		
		j_extra = pgGetJExtra(starts);
			
		special_locs = pgGetSpecialLocs(pg, block, starts, text_size, j_extra, COMPENSATE_SCALE);
		char_locs = (long *) UseMemory(special_locs);

		selection->offset = running_offset;
		selection->primary_caret = 0;

		if ( control_style || no_half_chars )
			closest_h = starts->bounds.bot_right.h - starts->bounds.top_left.h;
		else
		if (starts->flags & RIGHT_DIRECTION_BIT) {
		
			closest_h = pgAbsoluteValue(starts->bounds.bot_right.h - local_point.h);
			selection->primary_caret = starts->bounds.bot_right.h - starts->bounds.top_left.h;
		}
		else
			closest_h = pgAbsoluteValue(starts->bounds.top_left.h - local_point.h);
		
		if ((starts->cell_num && (starts->flags & TAB_BREAK_BIT)) || (text_size && (starts->flags & (PAR_BREAK_BIT | SOFT_PAR_BIT
				| BREAK_CONTAINER_BIT | BREAK_PAGE_BIT)))) {

			text_size -= 1;
			
			if ( (pg->flags & NO_LF_BIT)
				&& (starts->flags & (PAR_BREAK_BIT | SOFT_PAR_BIT)) == (PAR_BREAK_BIT | SOFT_PAR_BIT) )
				if (text_size)
					text_size -= 1;
		}

		while (text_size) {
			
			++char_locs;
			++running_offset;

			if ((starts->flags & SOFT_BREAK_BIT) && (!(starts->flags & WORD_HYPHEN_BIT))
				&& (text_size == 1))
				running_h = starts->bounds.bot_right.h;
			else
			if ((text_size == 1) && (starts->flags & TAB_BREAK_BIT) && !starts->cell_num)				
				running_h = starts->bounds.bot_right.h;
			else
				running_h = *char_locs + starts->bounds.top_left.h;
			
			if (control_style || no_half_chars) {
				
				if ((abs_h = local_point.h - running_h) < 0)
					abs_h = closest_h + 1;
			}
			else
				abs_h = pgAbsoluteValue(running_h - local_point.h);

			if (abs_h <= closest_h) {

				selection->primary_caret = running_h - starts->bounds.top_left.h;
				selection->offset = running_offset;
				closest_h = abs_h;
				
				if (text_size == 1 && (starts->flags & LINE_BREAK_BIT))
					selection->flags |= END_LINE_FLAG;
				else
					selection->flags &= CLR_END_LINE_FLAG;
			}
			
			--text_size;
		}
	}
	
	selection->secondary_caret = selection->primary_caret;

	if ((selection->offset == block->end) && (selection->offset < pg->t_length)) {
		
		UnuseMemory(block->lines);
		++block;

		pgPaginateBlock(pg, block, NULL, TRUE);
		UseMemory(block->lines);
		selection->primary_caret = selection->line = 0;
		selection->flags &= CLR_END_LINE_FLAG;
	}

	pgSetSecondaryCaret(pg, block, selection);

	UnuseMemory(block->lines);
	UnuseMemory(pg->t_blocks);

	if (special_locs)
		UnuseAndDispose(special_locs);

	if (!(conversion_info & NO_BYTE_ALIGN))
		if (pgAlignSelection(pg, align_char_best, &selection->offset))
			pgCalcSelect(pg, selection);
}



/* DEFAULT "TRACK A CONTROL" PROC. All this does is returns 0.  */

PG_PASCAL (long) pgTrackCtlProc (paige_rec_ptr pg, short verb,
		t_select_ptr first_select, t_select_ptr last_select, style_walk_ptr styles,
		pg_char_ptr associated_text, point_start_ptr bounds_info, short modifiers,
		long track_refcon)
{
#ifdef MAC_PLATFORM
#pragma unused (pg, verb, first_select, last_select, styles, associated_text, bounds_info, modifiers, track_refcon)
#endif

	return	0;
}


/* pgGlobalIdle gets called constantly to call all pgIdle's. */

PG_PASCAL (pg_boolean) pgGlobalIdle (pg_globals_ptr globals)
{
#ifdef PG_OITC
	pg_list_ptr		p;
	long			i;
	pg_boolean		result = FALSE;
	
	p = (pg_list_ptr)UseMemory(globals->pg_list);
	
	for (i = GetMemorySize(globals->pg_list); i != 0; i--, p++)
	{
		result |= pgIdle (p->pg);
	}
	
	UnuseMemory(globals->pg_list);
	
	return result;
#else
	return	FALSE;
#endif
}


/* pgIdle gets called constantly to handle idle processing, if desired. */

PG_PASCAL (pg_boolean) pgIdle (pg_ref pg)
{
	paige_rec_ptr			pg_rec;
	short					starting_buffer_mode, ending_buffer_mode, verb;
	pg_boolean				caret_time;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	starting_buffer_mode = pg_rec->key_buffer_mode;

	if (caret_time = pgIsCaretTime(pg_rec))
		verb = toggle_cursor;
	else verb = toggle_cursor_idle;

	pg_rec->procs.idle_proc(pg_rec, verb);

	if (!(pg_rec->flags & (DEACT_BITS | NO_EDIT_BIT))) {
		
		if (pg_rec->key_buffer_mode)
			pgInsertPendingKeys(pg);
		
		if (!pg_rec->num_selects) {

			pg_rec->procs.set_device(pg_rec, set_pg_device, &pg_rec->port, &pg_rec->bk_color);
			pgClipGrafDevice(pg_rec, clip_standard_verb, MEM_NULL);
			pg_rec->procs.cursor_proc(pg_rec, (t_select_ptr) UseMemory(pg_rec->select), verb);
			UnuseMemory(pg_rec->select);
	
			pg_rec->procs.set_device(pg_rec, unset_pg_device, &pg_rec->port, NULL);
		}
	}

	ending_buffer_mode = pg_rec->key_buffer_mode;
	UnuseMemory(pg);
	
	return	(ending_buffer_mode != starting_buffer_mode);
}



/* pgDragSelect is called to set or extend the selection. Typically, it would get
called in response to a mouse click. */

PG_PASCAL (long) pgDragSelect (pg_ref pg, const co_ordinate_ptr location, short verb,
		short modifiers, long track_refcon, pg_boolean auto_scroll)
{
	paige_rec_ptr				pg_rec;
	style_info_ptr				selected_style;
	long						ref_con_result;
	t_select_ptr				selection;
	t_select					new_select;
	co_ordinate					point;
	pg_short_t					sel_rec;
	pg_boolean					originally_in_ctl;
	short						no_half_chars, use_modifiers;
	
	pgInsertPendingKeys(pg);
	pg_rec = (paige_rec_ptr) UseMemory(pg);
	use_modifiers = modifiers;

	if(verb == mouse_down) {
	
		pg_rec->flags2 |= MOUSE_DRAG_STATE;
		
		if (!pg_rec->num_selects)
			pg_rec->hilite_anchor = pgCurrentInsertion(pg_rec);
	}

	if (verb == mouse_down && !(use_modifiers & (DIS_MOD_BIT | EXTEND_MOD_BIT)))
		pgClickSelectSubRef(pg, location);

	pg_rec->procs.set_device(pg_rec, set_pg_device, &pg_rec->port, &pg_rec->bk_color);
	pgClipGrafDevice(pg_rec, clip_standard_verb, MEM_NULL);

	if (!pg_rec->num_selects)
		sel_rec = 0;
	else
		sel_rec = (pg_rec->num_selects * 2) - 1;

	GetMemoryRecord(pg_rec->select, sel_rec, &new_select);
	
	if (modifiers & ARROW_ACTIVE_BIT)
		originally_in_ctl = FALSE;
	else
	if (verb != mouse_down)
		originally_in_ctl = ((new_select.flags & SELECT_ON_CTL) != 0);
	else
		originally_in_ctl = FALSE;

	ref_con_result = 0;

	if (verb == mouse_up) {
		pg_short_t			delete_rec, qty;
		
		if (originally_in_ctl)
		    ref_con_result = track_style_control(pg_rec, &new_select, track_refcon, use_modifiers, verb);

		while (qty = pgFindEmptyHilite(pg_rec, &delete_rec))
			DeleteMemory(pg_rec->select, delete_rec, qty);
		
		if (pg_rec->num_selects) {
		
			selection = (t_select_ptr) UseMemory(pg_rec->select);
			
			for (qty = pg_rec->num_selects; qty; selection += 2, --qty)
				if (selection->offset > selection[1].offset)
					reverse_selection(selection);

			UnuseMemory(pg_rec->select);
		}
		else {
		
			pgSetNextInsertIndex(pg_rec);
			pg_rec->stable_caret.h = pg_rec->stable_caret.v = 0;
		}
		
		pg_rec->flags2 &= (~MOUSE_DRAG_STATE);
	}
	else {

		if ((verb == mouse_moved) && auto_scroll && !originally_in_ctl) {
			rectangle			vis_rect;
			co_ordinate			use_location;
			short				h_verb, v_verb;

			GetMemoryRecord(pg_rec->vis_area, 0, &vis_rect);
			
			pgOffsetRect(&vis_rect, pg_rec->port.origin.h, pg_rec->port.origin.v);

			if (pg_rec->flags & SCALE_VIS_BIT)
				pgScaleRect(&pg_rec->port.scale, NULL, &vis_rect);

			use_location = *location;
			
			if (pg_rec->flags2 & NO_HAUTOSCROLL)
				use_location.h = vis_rect.top_left.h + 1;
			if (pg_rec->flags2 & NO_VAUTOSCROLL)
				use_location.v = vis_rect.top_left.v + 1;

			if (!pgPtInRect(&use_location, &vis_rect)) {
				
				h_verb = v_verb = scroll_none;
				
				if (use_location.h < vis_rect.top_left.h)
					h_verb = scroll_unit;
				else
				if (use_location.h > vis_rect.bot_right.h)
					h_verb = -scroll_unit;

				if (use_location.v < vis_rect.top_left.v)
					v_verb = scroll_unit;
				else
				if (use_location.v > vis_rect.bot_right.v)
					v_verb = -scroll_unit;
				
				pg_rec->procs.auto_scroll(pg_rec, h_verb, v_verb, &use_location, pg_rec->autoscroll_mode);
 				pg_rec->procs.set_device(pg_rec, unset_pg_device, &pg_rec->port, NULL);
				pg_rec->procs.set_device(pg_rec, set_pg_device, &pg_rec->port, &pg_rec->bk_color);

				pgClipGrafDevice(pg_rec, clip_standard_verb, MEM_NULL);
			}
		}
		
		pgConvertInputPoint(pg_rec, location, NULL, &point);

		if (verb == mouse_moved)
			fix_restricted_hilite(pg_rec, &point);
		
		no_half_chars = ((use_modifiers & (WORD_CTL_MOD_BIT | NO_HALF_CHARS_BIT)) != 0);

		if (!pgPtToTableOffset(pg_rec, &point, NULL, NULL, &new_select))
	 		pg_rec->procs.offset_proc(pg_rec, &point, no_half_chars, &new_select);

		pg_rec->stable_caret.h = pg_rec->stable_caret.v = 0;

		selected_style = pgFindTextStyle(pg_rec, new_select.offset);

		if (selected_style->class_bits & GROUP_CHARS_BIT)
			if (new_select.offset < pg_rec->t_length)
				use_modifiers |= STYLE_MOD_BIT;

		UnuseMemory(pg_rec->t_formats);

		if (verb == mouse_down) {
			
			if (pgPtInHyperlinkSource(pg, &point) >= 0)
				originally_in_ctl = FALSE;
			else
			if (pgPtInHyperlinkTarget(pg, &point) >= 0)
				originally_in_ctl = FALSE;
			else
				originally_in_ctl = ((new_select.flags & SELECT_ON_CTL) != 0);

			pg_rec->flags |= INVALID_CURSOR_BIT;
		}
		
		if (originally_in_ctl)
			ref_con_result = track_style_control(pg_rec, &new_select, track_refcon, use_modifiers, verb);

	    if (!ref_con_result) {
			
			new_select.flags &= (~SELECT_ON_CTL);			
			
			if ((verb == mouse_down) && (!(use_modifiers & EXTEND_MOD_BIT)))
				extend_selection (pg_rec, &new_select, use_modifiers, mouse_down, TRUE, TRUE);
			else
				extend_selection(pg_rec, &new_select, use_modifiers, mouse_moved, TRUE, TRUE);
		}
	}

	selection = (t_select_ptr) UseMemory(pg_rec->select);

	if (sel_rec = pg_rec->num_selects) {
		
		sel_rec = (sel_rec * 2) - 2;
		pg_rec->procs.click_proc(pg_rec, verb, use_modifiers, ref_con_result,
				&selection[sel_rec], &selection[sel_rec + 1]);
	}
	else
		pg_rec->procs.click_proc(pg_rec, verb, use_modifiers, ref_con_result, selection, selection);

	UnuseMemory(pg_rec->select);
 	pg_rec->procs.set_device(pg_rec, unset_pg_device, &pg_rec->port, NULL);
 	
	if (!ref_con_result && (verb == mouse_down || verb == mouse_up)) {
		
		ref_con_result = hyperlink_callback(pg_rec, verb, modifiers, pg_rec->hyperlinks);
		
		if (!ref_con_result)
			ref_con_result = hyperlink_callback(pg_rec, verb, modifiers, pg_rec->target_hyperlinks);
	}

 	UnuseMemory(pg);

 	return	ref_con_result;
}


/* pgSetCursorState changes the caret state per cursor_state.  */

PG_PASCAL (void) pgSetCursorState (pg_ref pg, short cursor_state)
{
	paige_rec_ptr		pg_rec;
	
	pg_rec = (paige_rec_ptr) UseMemory(pg);

	if (!(pg_rec->flags & DEACT_BITS) && !pg_rec->num_selects) {

		pg_rec->procs.set_device(pg_rec, set_pg_device, &pg_rec->port, &pg_rec->bk_color);
		pgClipGrafDevice(pg_rec, clip_standard_verb, MEM_NULL);

		pg_rec->procs.cursor_proc(pg_rec, (t_select_ptr) UseMemory(pg_rec->select), cursor_state);

		UnuseMemory(pg_rec->select);

		pg_rec->procs.set_device(pg_rec, unset_pg_device, &pg_rec->port, NULL);
	}
	
	UnuseMemory(pg);
}


/* This returns TRUE or FALSE for caret being on/off  */

PG_PASCAL (short) pgGetCursorState (pg_ref pg)
{
	paige_rec_ptr		pg_rec;
	short				result;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	result = ((pg_rec->flags & CARET_BIT) != 0);

	UnuseMemory(pg);
	
	return	result;
}


/* pgRemoveAllHilites gets called by several functions that will eventually
display text but will change the selection shape(s). Hence, carets or highlighting
needs to get turned off.  */

PG_PASCAL (void) pgRemoveAllHilites (paige_rec_ptr pg, short will_draw_mode)
{
	if (will_draw_mode && pgHighlightShowing(pg))
		pgTurnOffHighlight(pg, TRUE);
}


/* pgConvertInputPoint takes a point (typically a mouse location) and upscales it so it
correctly matches the scaled document, if scaled at all. */

PG_PASCAL (void) pgConvertInputPoint (paige_rec_ptr pg, co_ordinate_ptr in_point,
		co_ordinate_ptr offset_extra, co_ordinate_ptr out_point)
{
	*out_point = *in_point;
	pg->port.scale.scale = -pg->port.scale.scale;
	pgScalePt(&pg->port.scale, NULL, out_point);
	pgAddPt(&pg->scroll_pos, out_point);
	
	if (offset_extra)
		pgAddPt(offset_extra, out_point);

	pgSubPt(&pg->port.origin, out_point);
	pg->port.scale.scale = -pg->port.scale.scale;
}



/* pgCaretPosition returns a rectangle outlining the exact caret position. If there
is a selection range instead, the rectangle is set to the top-left of the selection
and FALSE is returned.  If offset is not CURRENT_POSITION (-1) that value is used
instead.   */

PG_PASCAL (pg_boolean) pgCaretPosition (pg_ref pg, long offset, rectangle_ptr caret_rect)
{
	paige_rec_ptr				pg_rec;
	register rectangle_ptr		answer_rect;
	register point_start_ptr	starts;
	t_select					use_select;
	text_block_ptr				block;
	short						result;
	
	pg_rec = (paige_rec_ptr) UseMemory(pg);
	
	if (offset != CURRENT_POSITION) {
		
		use_select.offset = offset;
		use_select.flags = SELECTION_DIRTY;
	}
	else
		GetMemoryRecord(pg_rec->select, 0, &use_select);
		
	if (use_select.flags & SELECTION_DIRTY)
		pgCalcSelect(pg_rec, &use_select);

	block = pgFindTextBlock(pg_rec, use_select.offset, NULL, TRUE, TRUE);
	
	answer_rect = caret_rect;

	starts = (point_start_ptr) UseMemory(block->lines);
	starts += use_select.line;
	answer_rect->top_left.h = answer_rect->bot_right.h = starts->bounds.top_left.h + use_select.primary_caret;
	answer_rect->top_left.v = starts->bounds.top_left.v;
	answer_rect->bot_right.v = starts->bounds.bot_right.v;
	++answer_rect->bot_right.h;
	
	pgOffsetRect(answer_rect, -pg_rec->scroll_pos.h, -pg_rec->scroll_pos.v);
	
	UnuseMemory(block->lines);
	UnuseMemory(pg_rec->t_blocks);

	result = (pg_rec->num_selects == 0);

	UnuseMemory(pg);
	
	return	result;
}


/* pgTurnOffHighlight removes the highlight (or cursor) */

PG_PASCAL (void) pgTurnOffHighlight (paige_rec_ptr pg, pg_boolean set_port)
{
	if (set_port)
		pgSetupGrafDevice(pg, &pg->port, MEM_NULL, clip_standard_verb);

	if (pg->num_selects)
		pgDrawHighlight(pg, hide_cursor);
	else {
		pg->procs.cursor_proc(pg, (t_select_ptr) UseMemory(pg->select), hide_cursor);
		UnuseMemory(pg->select);
	}

	if (set_port)
		pg->procs.set_device(pg, unset_pg_device, &pg->port, NULL);
}



/* pgHighlightShowing returns "TRUE" if the caret or highlight is currently on. */

PG_PASCAL (pg_boolean) pgHighlightShowing (paige_rec_ptr pg)
{
	if (pg->flags & DEACT_BITS)
		return	FALSE;

	if (pg->num_selects)
		return	TRUE;
	
	return ((long) (pg->flags & CARET_BIT) != 0);
}


/* pgSetHiliteState sets the two activate / deactivate modes.  */

PG_PASCAL (void) pgSetHiliteStates (pg_ref pg, short front_back_state,
		short perm_state, pg_boolean should_draw)
{
	paige_rec_ptr		pg_rec;
	short				old_fb_state, old_perm_state;
	short				old_was_active, new_is_active;
	short				new_cursor_verb;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	
	old_fb_state = (!(pg_rec->flags & DEACT_BIT));
	old_perm_state = (!(pg_rec->flags & PERM_DEACT_BIT));

	if (front_back_state) {
		
		if (((front_back_state == activate_verb) && (!old_fb_state))
			|| ((front_back_state == deactivate_verb) && old_fb_state))
			pg_rec->flags ^= DEACT_BIT;
	}

	if (perm_state) {
		
		if (((perm_state == activate_verb) && (!old_perm_state))
			|| ((perm_state == deactivate_verb) && old_perm_state))
			pg_rec->flags ^= PERM_DEACT_BIT;
	}

	old_was_active = (old_fb_state && old_perm_state);
	new_is_active = ((!(pg_rec->flags & DEACT_BIT)) && (!(pg_rec->flags & PERM_DEACT_BIT)));

	if (old_was_active != new_is_active) {

		if (should_draw) {
			
			if (new_is_active)
				new_cursor_verb = activate_cursor;
			else
				new_cursor_verb = deactivate_cursor;

			pgSetupGrafDevice(pg_rec, &pg_rec->port, MEM_NULL, clip_standard_verb);
			
			if (new_is_active)
				pgCallTextHook(pg_rec, NULL, 0, 0, pg_rec->t_length, call_for_activate,
					front_back_state, perm_state, should_draw);

			pgDrawHighlight(pg_rec, new_cursor_verb);
			
			if (!new_is_active)
				pgCallTextHook(pg_rec, NULL, 0, 0, pg_rec->t_length, call_for_activate,
					front_back_state, perm_state, should_draw);

			pg_rec->procs.set_device(pg_rec, unset_pg_device, &pg_rec->port, NULL);
		}
		else
			pgCallTextHook(pg_rec, NULL, 0, 0, pg_rec->t_length, call_for_activate,
					front_back_state, perm_state, should_draw);
	}
	else
	if (new_is_active && (pg_rec->num_selects == 0) && should_draw) {
		t_select_ptr				select;
		
		select = (t_select_ptr) UseMemory(pg_rec->select);
		pgSetupGrafDevice(pg_rec, &pg_rec->port, MEM_NULL, clip_standard_verb);
		pg_rec->procs.cursor_proc(pg_rec, select, restore_cursor);
		UnuseMemory(pg_rec->select);
		pg_rec->procs.set_device(pg_rec, unset_pg_device, &pg_rec->port, NULL);
	}

	UnuseMemory(pg);
}


/* pgGetHiliteStates returns the two hilite states.  */

PG_PASCAL (void) pgGetHiliteStates (pg_ref pg, short PG_FAR *front_back_state,
		short PG_FAR *perm_state)
{
	paige_rec_ptr		pg_rec;
	
	pg_rec = (paige_rec_ptr) UseMemory(pg);
	
	if (front_back_state) {
		
		if (pg_rec->flags & DEACT_BIT)
			*front_back_state = deactivate_verb;
		else
			*front_back_state = activate_verb;
	}

	if (perm_state) {

		if (pg_rec->flags & PERM_DEACT_BIT)
			*perm_state = deactivate_verb;
		else
			*perm_state = activate_verb;
	}

	UnuseMemory(pg);
}


/* This function sets a selection range according to the modifiers (which can
be the same as modifiers in pgDragSelect).    */

PG_PASCAL (void) pgSetSelection (pg_ref pg, size_t begin_sel, size_t end_sel,
		short modifiers, short show_hilite)
{
	paige_rec_ptr			pg_rec;
	t_select_ptr			selections;
	size_t					beginning_select, ending_select;
	size_t					unused_offset, first_select_flags, second_select_flags;
	short					will_draw, left_can_extend, right_can_extend;
	pg_short_t				select_qty;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	first_select_flags = second_select_flags = SELECTION_DIRTY;
	pg_rec->stable_caret.h = pg_rec->stable_caret.v = 0;

	if (will_draw = show_hilite && (!(pg_rec->flags & DEACT_BITS))) {
	
		pgSetupGrafDevice(pg_rec, &pg_rec->port, MEM_NULL, clip_standard_verb);
		pgTurnOffHighlight(pg_rec, FALSE);
	}

	ending_select = pgFixOffset(pg_rec, end_sel);
	beginning_select = pgFixOffset(pg_rec, begin_sel);
	
	pgAlignSelection (pg_rec, align_char_best, &beginning_select);
	pgAlignSelection (pg_rec, align_char_right, &ending_select);

	left_can_extend = ( !(modifiers & EXTEND_MOD_BIT) || (begin_sel != CURRENT_POSITION) );
	right_can_extend = ( !(modifiers & EXTEND_MOD_BIT) || (end_sel != CURRENT_POSITION) );

	if (modifiers & WORD_MOD_BIT) {
		
		if (left_can_extend)
			pgFindWord(pg, beginning_select, &beginning_select, &unused_offset, TRUE, FALSE);
		
		if (right_can_extend)
			pgFindWord(pg, ending_select, &unused_offset, &ending_select,
				(pg_boolean)FALSE, (pg_boolean)((pg_rec->flags & NO_SMART_CUT_BIT) == 0));
		
		if (beginning_select != ending_select) {
			
			if (left_can_extend)
				first_select_flags |= WORD_FLAG;
			if (right_can_extend)
				second_select_flags |= WORD_FLAG;
		}
	}
	else
	if (modifiers & PAR_MOD_BIT) {

		if (left_can_extend)
			pgFindPar(pg, beginning_select, &beginning_select, &unused_offset);

		if (right_can_extend)
			pgFindPar(pg, ending_select, &unused_offset, &ending_select);
	}
	else
	if (modifiers & LINE_MOD_BIT) {

		if (left_can_extend)
			pgFindLine(pg, beginning_select, &beginning_select, &unused_offset);

		if (right_can_extend)
			pgFindLine(pg, ending_select, &unused_offset, &ending_select);
	}
	else
	if (modifiers & WORD_CTL_MOD_BIT) {

		if (left_can_extend)
			pgFindCtlWord(pg, beginning_select, &beginning_select, &unused_offset, TRUE);

		if (right_can_extend)
			pgFindCtlWord(pg, ending_select, &unused_offset, &ending_select, FALSE);
	}
	else
	if (modifiers & STYLE_MOD_BIT) {
		select_pair			style_mod_range;

		if (left_can_extend) {
		
			pgGetStyleClassInfo(pg, beginning_select, &style_mod_range);
			beginning_select = style_mod_range.begin;
		}
		
		if (right_can_extend) {
		
			pgGetStyleClassInfo(pg, ending_select, &style_mod_range);
			ending_select = style_mod_range.end;
		}
	}
	
	select_qty = pg_rec->num_selects * 2;

	if (modifiers & DIS_MOD_BIT) {
		
		SetMemorySize(pg_rec->select, select_qty + 2);
		++pg_rec->num_selects;
	}
	else {

		SetMemorySize(pg_rec->select, 2);
		
		if (beginning_select != ending_select)
			pg_rec->num_selects = 1;
		else
			pg_rec->num_selects = 0;
		
		select_qty = 0;
	}

	selections = (t_select_ptr) UseMemory(pg_rec->select);
	selections += select_qty;
	selections->offset = beginning_select;
	selections[1].offset = ending_select;
	selections->flags = (pg_short_t)first_select_flags;
	selections[1].flags = (pg_short_t)second_select_flags;

	if (!pg_rec->num_selects)
		pg_rec->hilite_anchor = selections->offset;

	UnuseMemory(pg_rec->select);

	if (pg_rec->active_subset) {
		paige_sub_ptr		sub_ptr;
		
		sub_ptr = (paige_sub_ptr) UseMemory(pg_rec->active_subset);
		sub_ptr->num_selects = pg_rec->num_selects;
		UnuseMemory(pg_rec->active_subset);
	}

	if (will_draw) {
		
		update_hilite_rgn(pg_rec);
		pgDrawHighlight (pg_rec, restore_cursor);
		
		if (pg_rec->num_selects == 0)
			pg_rec->stable_caret.h = pg_rec->stable_caret.v = 0;
		
		pg_rec->procs.set_device(pg_rec, unset_pg_device, &pg_rec->port, NULL);
	}

	if (!pg_rec->num_selects)
		pgSetNextInsertIndex(pg_rec);

	UnuseMemory(pg);
}


/* pgGetSelection returns a simple selection (a simple selection is 1 or less
selection pairs exist).  Either parameter can be null.
Modified 10/20/94 -- if multiple selections, the outside boundaries are
computed for "begin" end "end" selection points.  */

PG_PASCAL (void) pgGetSelection (pg_ref pg, size_t PG_FAR *begin_sel, size_t PG_FAR *end_sel)
{
	paige_rec_ptr	pg_rec;
	register t_select_ptr	selections;
	register pg_short_t		num_selections;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	selections = (t_select_ptr) UseMemory(pg_rec->select);
	
	if (begin_sel)
		*begin_sel = selections->offset;
	if (end_sel)
		*end_sel = selections->offset;

	for (num_selections = pg_rec->num_selects * 2; num_selections; ++selections, --num_selections) {
		
		if (begin_sel)
			if (selections->offset < *begin_sel)
				*begin_sel = selections->offset;
		if (end_sel)
			if (selections->offset > *end_sel)
				*end_sel = selections->offset;
	}
	
	UnuseMemory(pg_rec->select);
	UnuseMemory(pg);
}


/* pgNumSelections returns the number of selection pairs in pg.  */

PG_PASCAL (pg_short_t) pgNumSelections (pg_ref pg)
{
	paige_rec_ptr	pg_rec;
	pg_short_t		num_sel;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	num_sel = pg_rec->num_selects;
	UnuseMemory(pg);
	
	return	num_sel;
}



/* pgExtendSelection extends the current selection by amount_ext. Negative
numbers extend to the left, positive number to the right.  Modifiers can be
the same as pgDragSelect values.  */

PG_PASCAL (void) pgExtendSelection (pg_ref pg, long amount_ext, short modifiers,
			short show_hilite)
{
	paige_rec_ptr			pg_rec;
	t_select_ptr			select;
	t_select				use_select;
	pg_short_t				use_sel_rec;
	rectangle				sel_rect;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	use_sel_rec = 0;

	if (!pg_rec->num_selects)
		pg_rec->hilite_anchor = pgCurrentInsertion(pg_rec);

	if ((modifiers & PIVOT_EXTEND_BIT) && pg_rec->num_selects) {
		
		select = (t_select_ptr) UseMemory(pg_rec->select);
		if (select->offset >= pg_rec->hilite_anchor)
			use_sel_rec = 1;
		
		UnuseMemory(pg_rec->select);
	}
	else 
		if (pg_rec->num_selects && amount_ext > 0)
			use_sel_rec = pg_rec->num_selects * 2 - 1;

	GetMemoryRecord(pg_rec->select, use_sel_rec, &use_select);
	
	use_select.offset = pgFixOffset(pg_rec, use_select.offset + amount_ext);

	if (amount_ext < 0 && use_select.offset > 0)
		if (pgCharType(pg, use_select.offset, LAST_HALF_BIT))
			use_select.offset -= 1;

	if (!show_hilite) {
		t_select_ptr			selections;
		long					sel_from, sel_to, temp_sel;

		selections = (t_select_ptr) UseMemory(pg_rec->select);
		
		if (pg_rec->num_selects && (amount_ext < 0)) {
					
			sel_from = use_select.offset;
			sel_to = selections[pg_rec->num_selects * 2 - 1].offset;
			
		}
		else {
			sel_from = use_select.offset;
			sel_to = selections->offset;
		}
		
		UnuseMemory(pg_rec->select);
		
		if (sel_from > sel_to) {
			
			temp_sel = sel_from;
			sel_from = sel_to;
			sel_to = temp_sel;
		}
		
		pgSetSelection(pg, sel_from, sel_to, modifiers, FALSE);
	}
	else {
		co_ordinate		scroll_pos, fake_pt;
		short			use_verb;

		pgCaretPosition(pg, use_select.offset, &sel_rect);
		scroll_pos = pg_rec->scroll_pos;
		pgNegatePt(&scroll_pos);


		++sel_rect.top_left.v;
		
		if ((modifiers & (WORD_MOD_BIT | PAR_MOD_BIT | LINE_MOD_BIT | STYLE_MOD_BIT))
				&& pg_rec->num_selects)
			use_verb = mouse_moved;
		else
			use_verb = mouse_down;
		
		pgScaleRect(&pg_rec->port.scale, &scroll_pos, &sel_rect);

		fake_pt = sel_rect.top_left;               // chandhok/within 2/21/96
        pgAddPt(&pg_rec->port.origin, &fake_pt);   // chandhok/within 2/21/96

		pgDragSelect(pg, &fake_pt, use_verb, (short)(modifiers | EXTEND_MOD_BIT), (long)0, (pg_boolean)FALSE);
		pgDragSelect(pg, &fake_pt, mouse_up, modifiers, (long)0, (pg_boolean)FALSE);
	}

	UnuseMemory(pg);
}



/* pgSetInsertSelect gets called by insertion functions (within Paige, usually)
that want to set a single selection, no drawing and no disturbance of the current
insertion style (the insertion style stays the same -- normal pgSetSelect might
change it).   */

PG_PASCAL (void) pgSetInsertSelect (paige_rec_ptr pg, long offset)
{
	t_select_ptr		selections;
	long				use_offset;

	use_offset = pgFixOffset(pg, offset);

	selections = (t_select_ptr) UseMemory(pg->select);
	
	if (selections->offset != use_offset) {
	
		selections->offset = use_offset;
		selections->flags = SELECTION_DIRTY;
		pg->stable_caret.h = pg->stable_caret.v = 0;
	}
	
	UnuseMemory(pg->select);

	pg->num_selects = 0;
}


/* pgCalcSelect figures out all the fields in selection. Upon entry, selection->offset
must be the absolute text position and selection->flags should be appropriate. */

PG_PASCAL (void) pgCalcSelect (paige_rec_ptr pg, t_select_ptr selection)
{
	text_block_ptr					block;
	register point_start_ptr		starts;
	long PG_FAR						*char_locs;
	memory_ref						special_locs;
	long							j_extra;
	pg_short_t						local_offset, line_ctr, old_flags;
	pg_short_t						text_size, text_size_index;

	block = pgFindTextBlock(pg, selection->offset, NULL, TRUE, TRUE);
	starts = (point_start_ptr) UseMemory(block->lines);
	old_flags = selection->flags;
	line_ctr = 0;
	selection->primary_caret = 0;

	selection->flags &= CLR_INIT_SELECT;
	
	if (local_offset = (pg_short_t)(selection->offset - block->begin)) {
	
		while (local_offset >= starts->offset) {
			
			++starts;
			++line_ctr;
			if (starts->flags == TERMINATOR_BITS)
				break;
		}
		
		--line_ctr;
		--starts;
		
		text_size_index = 1;

		if ((old_flags & END_LINE_FLAG) && line_ctr && (local_offset == starts->offset)) {
			
			--line_ctr;
			--starts;
			selection->flags |= END_LINE_FLAG;
			text_size_index = 2;
		}

		j_extra = pgGetJExtra(starts);
		text_size = starts[text_size_index].offset - starts->offset;

		if (pg->t_length == 0 || ((starts->flags & TAB_BREAK_BIT) && selection->offset == pg->t_length))
			selection->primary_caret = starts->bounds.bot_right.h - starts->bounds.top_left.h;
		else {

			special_locs = pgGetSpecialLocs(pg, block, starts, text_size, j_extra, COMPENSATE_SCALE);
			char_locs = (long *) UseMemory(special_locs);
			selection->primary_caret = char_locs[local_offset - starts->offset];
			UnuseAndDispose(special_locs);
		}
	}
	else
	if (starts->flags & RIGHT_DIRECTION_BIT)
		selection->primary_caret = starts->bounds.bot_right.h - starts->bounds.top_left.h;

	selection->line = line_ctr;
	selection->secondary_caret = selection->primary_caret;
	pgSetSecondaryCaret(pg, block, selection);

	UnuseMemory(block->lines);
	UnuseMemory(pg->t_blocks);
}


/* pgDrawHighlight draws either the caret (if turned on) or the highlight */

PG_PASCAL (void) pgDrawHighlight (paige_rec_ptr pg, short cursor_verb)
{
	t_select_ptr		selections;

	selections = (t_select_ptr) UseMemory(pg->select);
	
	if (pg->num_selects) {
		
		update_hilite_rgn(pg);
		pg->procs.hilite_draw(pg, pg->hilite_rgn);
		
		if (cursor_verb == deactivate_cursor)
			pg->procs.cursor_proc(pg, selections, cursor_verb);
	}
	else {
		
		if (cursor_verb == update_cursor)
			pg->procs.cursor_proc(pg, selections, restore_cursor);

		pg->procs.cursor_proc(pg, selections, cursor_verb);
	}

	UnuseMemory(pg->select);
}

/* pgInvalidateHilite invalidates the current caret or highlight. If remove_caret
is TRUE then the caret, if visible, is turned off first. */

PG_PASCAL (void) pgInvalidateHilite (paige_rec_ptr pg_rec, pg_boolean remove_caret)
{
	t_select_ptr			select;
	pg_short_t				num_selects;

	select = (t_select_ptr) UseMemory(pg_rec->select);

	if (!(num_selects = pg_rec->num_selects)) {
		
		if (remove_caret)
			if (pg_rec->flags & CARET_BIT)
				if (!(select->flags & SELECTION_DIRTY))
					pgTurnOffHighlight(pg_rec, TRUE);
	}
	
	for (num_selects += 1; num_selects; --num_selects, ++select) {
		
		select->flags |= SELECTION_DIRTY;
		select[1].flags |= SELECTION_DIRTY;
	}
	
	pg_rec->flags |= INVALID_CURSOR_BIT;

	UnuseMemory(pg_rec->select);
}


/* pgInvalSelect sets the range specified to require recalculation  */

PG_PASCAL (void) pgInvalSelect (pg_ref pg, size_t select_from, size_t select_to)
{
	paige_rec_ptr					pg_rec;
	register text_block_ptr			block;
	t_select_ptr					selections;
	pg_short_t						num_starts;
	pg_short_t						remaining_recs, block_num;
	long							lowest_select;

	pg_rec = (paige_rec_ptr) UseMemory(pg);

	pg_rec->flags |= (INVALID_CURSOR_BIT);
	pgInvalTextMeasure(pg_rec, select_from, select_to);

	block = pgFindTextBlock(pg_rec, select_from, &block_num, FALSE, FALSE);
	remaining_recs = (pg_short_t)GetMemorySize(pg_rec->t_blocks) - block_num;
	lowest_select = block->begin;
	
	while (remaining_recs) {
		
		block->flags &= (BELOW_CONTAINERS | LINES_PURGED);
		block->flags |= (NEEDS_CALC | NEEDS_PARNUMS);

		if (block->end >= select_to)
			break;

		++block;
		--remaining_recs;
	}

	UnuseMemory(pg_rec->t_blocks);

	selections = (t_select_ptr) UseMemory(pg_rec->select);
	if (!(num_starts = pg_rec->num_selects)) {
	
		if (selections->offset >= lowest_select) {
		
			selections->flags |= SELECTION_DIRTY;
			pg_rec->stable_caret.h = pg_rec->stable_caret.v = 0;
		}
	}
	else {
		num_starts *= 2;
		while (num_starts) {
			
			if (selections->offset >= lowest_select)
				selections->flags |= SELECTION_DIRTY;
			
			++selections;
			--num_starts;
		}
	}
	
	UnuseMemory(pg_rec->select);
	UnuseMemory(pg);
}


/* pgInvalTextMeasure invalidates the existing measurements for text for
expected text range select_from to select_to. */

PG_PASCAL (void) pgInvalTextMeasure (paige_rec_ptr pg_rec, long select_from, long select_to)
{
	register short		charloc_index;
	pg_globals_ptr		globals;
	
	globals = pg_rec->globals;

	for (charloc_index = 0; charloc_index < WIDTH_QTY; ++charloc_index)
		if (globals->width_tables[charloc_index].mem_id == pg_rec->mem_id) {
		
			if ((select_from < globals->width_tables[charloc_index].end)
				&& (select_to > globals->width_tables[charloc_index].offset))
					pgFreeCharLocs(pg_rec, (pg_short_t)charloc_index);
		}
}


/* pgPixelToPage returns the "page number" that contains coordinate pixel.
It also returns the amount to add to the wrap_area to obtain the physical
page that contains the point. THIS FUNCTION ASSUMES A "REPEATER" MODE FOR
THE PAGE AREA. p_width, if non-NULL gets set to the page height if VERTICAL
repeat mode, or p_height if non-NULL gets set to page height if HORIZONTAL
repeat mode. Note: page_offset can be NULL. If include_scroll = TRUE the
"real" position is computed (scrolled position). */

PG_PASCAL (pg_short_t) pgPixelToPage (pg_ref pg, co_ordinate_ptr pixel,
			co_ordinate_ptr page_offset, long PG_FAR *p_width, long PG_FAR *p_height,
			long PG_FAR *max_bottom, pg_boolean include_scroll)
{
	paige_rec_ptr	pg_rec;
	rectangle		page_bounds;
	long			first_vis, page_height, page_width, max_v;
	long			text_height;
	pg_short_t		page_num;

	pg_rec = (paige_rec_ptr) UseMemory(pg);

	pgShapeBounds(pg_rec->wrap_area, &page_bounds);
	
	if (include_scroll)
		pgOffsetRect(&page_bounds, -pg_rec->scroll_pos.h, -pg_rec->scroll_pos.v);

	text_height = pg_rec->doc_bounds.bot_right.v - pg_rec->doc_bounds.top_left.v;
	max_v = page_bounds.top_left.v + text_height
			+ pg_rec->doc_info.repeat_offset.v - 1 + pg_rec->doc_info.repeat_slop;

	page_width = page_height = 0;

	if (page_offset)
		page_offset->v = page_offset->h = 0;

	if (pg_rec->doc_info.attributes & V_REPEAT_BIT) {
		long		use_pixel;

		page_height = page_bounds.bot_right.v - page_bounds.top_left.v
				+ pg_rec->doc_info.repeat_offset.v;
		
		if (page_height == 0)
			++page_height;

		if (pixel->v > max_v)
			use_pixel = max_v;
		else
			use_pixel = pixel->v;

		first_vis = use_pixel - page_bounds.top_left.v;
		page_num = (pg_short_t)(first_vis / page_height);
		
		if (page_offset)
			page_offset->v = (page_num * page_height);
	}
	else {
		page_width = page_bounds.bot_right.h - page_bounds.top_left.h
			+ pg_rec->doc_info.repeat_offset.h;

		if (page_width == 0)
			++page_width;

		first_vis = pixel->h - page_bounds.top_left.h;
		page_num = (pg_short_t)(first_vis / page_width);
		
		if (page_offset)
			page_offset->h = (page_num * page_width);
	}
	
	if (p_width)
		*p_width = page_width;
	if (p_height)
		*p_height = page_height;
	if (max_bottom)
		*max_bottom = max_v;

	UnuseMemory(pg);
	
	return	page_num;
}



/* pgPtInPageArea returns TRUE if the given point is within the wrap_area shape.
Note that this function ALSO CHANGES point to the scaled point offset by
extra and pg's scroll position (extra can be NULL). If view_flags is non NULL
then extra flag for "inside repeat area" is set appropriately. */

extern PG_PASCAL (pg_boolean) pgPtInPageArea (paige_rec_ptr pg, co_ordinate_ptr point,
		co_ordinate_ptr offset_extra, short PG_FAR *view_flags )
{
	co_ordinate			scaled_pt, repeat_offset;

	pgConvertInputPoint(pg, point, offset_extra, &scaled_pt);

	if (pg->doc_info.attributes & (V_REPEAT_BIT | H_REPEAT_BIT)) {
	
		pgPixelToPage(pg->myself, &scaled_pt, &repeat_offset, NULL,
				NULL, NULL, FALSE);
		
		if (view_flags && ( (scaled_pt.v >= pg->doc_bounds.top_left.v)
				&& (scaled_pt.v < pg->doc_bounds.bot_right.v)) ) {
			rectangle		page_bounds;

			pgShapeBounds(pg->wrap_area, &page_bounds);
			pgOffsetRect(&page_bounds, repeat_offset.h, repeat_offset.v);
			
			if (pg->doc_info.attributes & V_REPEAT_BIT) {
				
				if ((scaled_pt.v < page_bounds.top_left.v) || (scaled_pt.v > page_bounds.bot_right.v))
					*view_flags |= WITHIN_REPEAT_AREA;
			}
			else
			if (pg->doc_info.attributes & H_REPEAT_BIT) {

				if ((scaled_pt.h < page_bounds.top_left.h) || (scaled_pt.v > page_bounds.bot_right.h))
					*view_flags |= WITHIN_REPEAT_AREA;
			}
		}
	}
	else
		repeat_offset.h = repeat_offset.v = 0;

	return	pgPtInShape(pg->wrap_area, &scaled_pt, &repeat_offset, NULL, NULL);
}


/* This function returns various bits to indicate where the point is within pg,
if anywhere.   */

PG_PASCAL (short) pgPtInView (pg_ref pg, const co_ordinate_ptr point,
		const co_ordinate_ptr offset_extra)
{
	paige_rec_ptr			pg_rec;
	pg_scale_ptr			scaler;
	co_ordinate				extra, scaled_pt;
	short					view;

	pg_rec = (paige_rec_ptr) UseMemory(pg);

	view = 0;
	
	extra = pg_rec->port.origin;

	if (offset_extra)
		pgAddPt(offset_extra, &extra);
	
	scaled_pt = *point;
	pgAddPt(&extra, &scaled_pt);
	
	if (pg_rec->flags & SCALE_VIS_BIT)
		scaler = &pg_rec->port.scale;
	else
		scaler = (pg_scale_ptr)NULL;

	if (pgPtInShape(pg_rec->vis_area, &scaled_pt, &extra, NULL, scaler))
		view |= WITHIN_VIS_AREA;

	scaled_pt = *point;

	pgScaleLong(-pg_rec->port.scale.scale, pg_rec->port.scale.origin.h,
			&scaled_pt.h);
	pgScaleLong(-pg_rec->port.scale.scale, pg_rec->port.scale.origin.v,
			&scaled_pt.v);

	pgAddPt(&pg_rec->scroll_pos, &scaled_pt);
	pgSubPt(&pg_rec->port.origin, &scaled_pt);

	if (pgPtInPageArea(pg_rec, point, offset_extra, &view)) {
		
		view |= WITHIN_WRAP_AREA;
		if (scaled_pt.v < pg_rec->doc_bounds.bot_right.v) {

			view |= WITHIN_TEXT;
			
			if (pgPtInHyperlinkSource(pg, point) >= 0)
				view |= WITHIN_LINK_SOURCE;
			if (pgPtInHyperlinkTarget(pg, point) >= 0)
				view |= WITHIN_LINK_TARGET;
			if (pgPtInTable(pg, point, NULL, NULL))
				view |= WITHIN_TABLE;
		}
	}

	if (scaled_pt.v < pg_rec->doc_bounds.top_left.v)
		view |= WITHIN_TOP_AREA;
	if (scaled_pt.v > pg_rec->doc_bounds.bot_right.v)
		view |= WITHIN_BOTTOM_AREA;
	if (scaled_pt.h < pg_rec->doc_bounds.top_left.h)
		view |= WITHIN_LEFT_AREA;
	if (scaled_pt.h > pg_rec->doc_bounds.bot_right.h)
		view |= WITHIN_RIGHT_AREA;

	if (pgPtInShape(pg_rec->exclude_area, &scaled_pt, NULL, NULL, NULL))
		view |= WITHIN_EXCLUDE_AREA;

	UnuseMemory(pg);
	
	return	view;
}


/* pgTextRect returns the rectangle that encloses the range of text exactly.
The resulting rectangle is moved to the current "scrolled" position if want_scroll
is TRUE;  the rectangle is scaled to the current scaled position if want_scaled
is TRUE.
Feature enhancement 2/25/95, function now returns the baseline value for the
first char in the rectangle. */

PG_PASCAL (short) pgTextRect (pg_ref pg, const select_pair_ptr range, pg_boolean want_scroll,
		pg_boolean want_scaled, rectangle_ptr rect)
{
	paige_rec_ptr				pg_rec;
	register pg_short_t			num_selects;
	memory_ref					select_list;
	select_pair_ptr				selections;
	short						first_baseline;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	pg_rec->procs.set_device(pg_rec, set_pg_device, &pg_rec->port, &pg_rec->bk_color);
	first_baseline = 0;

	pgFillBlock(rect, sizeof(rectangle), 0);
	
	if (select_list = pgSetupOffsetRun(pg_rec, range, FALSE, FALSE)) {
		
		for (selections = (select_pair_ptr) UseMemory(select_list), num_selects = (pg_short_t)GetMemorySize(select_list);
				num_selects; ++selections, --num_selects)
			add_to_text_rect(pg_rec, selections, rect, &first_baseline);
		
		UnuseAndDispose(select_list);
	}

	if (want_scroll)
		pgOffsetRect(rect, -pg_rec->scroll_pos.h, -pg_rec->scroll_pos.v);
	if (want_scaled) {
		co_ordinate		offset_extra;
		
		offset_extra = pg_rec->scroll_pos;
		pgNegatePt(&offset_extra);
		
		if (!want_scroll)
			offset_extra.h = offset_extra.v = 0;

		pgScaleRect(&pg_rec->port.scale, &offset_extra, rect);
	}

 	pg_rec->procs.set_device(pg_rec, unset_pg_device, &pg_rec->port, NULL);

	UnuseMemory(pg);
	
	return	first_baseline;
}


/* pgBuildHiliteRgn builds the required highlight shape by calling the appropriate
low-level function. If necessary, non-hiliting styles are subtracted from the
shape.  */

PG_PASCAL (void) pgBuildHiliteRgn (paige_rec_ptr pg, t_select_ptr selections,
		pg_short_t select_qty, shape_ref rgn)
{
	select_ref			no_hilite_ref;
	pg_short_t			no_hilite_qty;
	shape_ref			avoid_shape, copy_of_original;
	
	if (!select_qty)
		pgSetShapeRect(rgn, NULL);
	else {
	
		pg->procs.hilite_rgn(pg, selections, select_qty, rgn);
		
		if (no_hilite_ref = hilite_avoid_list(pg, selections, select_qty)) {
			
			copy_of_original = MemoryDuplicate(rgn);
			avoid_shape = pgRectToShape(pg->globals->mem_globals, NULL);
			
			no_hilite_qty = (pg_short_t)GetMemorySize(no_hilite_ref) / 2;
			pg->procs.hilite_rgn(pg, (t_select_ptr)UseMemory(no_hilite_ref), no_hilite_qty,
					avoid_shape);
			
			UnuseMemory(no_hilite_ref);
			DisposeMemory(no_hilite_ref);
			pgDiffShape(avoid_shape, copy_of_original, rgn);
			DisposeMemory(copy_of_original);
			DisposeMemory(avoid_shape);
		}
	}
}


/* pgAlignSelection checks if the given offset (which is absolute relative for
all text) is on a legal byte position. If not, it is adjusted and TRUE returned.
An example of illegal byte offsets would be between multibyte chars; another
example would be in the middle of hidden text.
If direction is positive the alignment is adjusted forward, if negative the
alignment is adjusted backwards.  */

PG_PASCAL (pg_boolean) pgAlignSelection (paige_rec_ptr pg, short align_verb, size_t PG_FAR *select_offset)
{
	text_block_ptr			block;
	pg_char_ptr				text;
	style_walk				walker;
	register size_t PG_FAR	*offset;
	register size_t			byte_advance, local_offset;
	size_t					distance_in, max_size, offset_begin, offset_end;
	pg_short_t				byte_modulo, char_size;
	pg_boolean				did_adjust;

	offset = select_offset;
	
	if (*offset > pg->t_length)
		*offset = pg->t_length;

	block = pgFindTextBlock(pg, *offset, NULL, FALSE, TRUE);
	text = (pg_char_ptr) UseMemory(block->text);
	pgPrepareStyleWalk(pg, *offset, &walker, FALSE);
	
	local_offset = *offset - block->begin;
	max_size = (block->end - block->begin);
	offset_end = block->end - block->begin;
	
	char_size = walker.cur_style->char_bytes + 1;
	distance_in = *offset - walker.prev_style_run->offset;
	byte_modulo = (pg_short_t)distance_in;
	byte_modulo %= char_size;

	switch (align_verb) {

		case align_char_best:
			if (byte_modulo < (char_size / 2))
				byte_advance = -1;
			else
				byte_advance = 1;
			break;
			
		case align_char_right:
			byte_advance = 1;
			break;
		
		case align_char_left:
			byte_advance = -1;
			break;

	}	
	
	did_adjust = FALSE;
	
	if (!(block->flags & ANY_CALC)) {
		point_start_ptr		starts;
		pg_short_t			start_offset;
		
		starts = (point_start_ptr) UseMemory(block->lines);
		start_offset = (pg_short_t)local_offset;
		
		while (starts[1].flags != TERMINATOR_BITS) {
			
			if (starts[1].offset > start_offset)
				break;
			
			++starts;
		}
		
		offset_begin = (long)starts->offset;
		UnuseMemory(block->lines);
	}
	else
		offset_begin = 0;
	
	if (pg->flags & NO_HIDDEN_TEXT_BIT) {
	
		while (walker.cur_style->styles[hidden_text_var]) {
			
			if (!pgWalkNextStyle(&walker)) {
				
				*offset = pg->t_length;
				break;
			}
			
			*offset = walker.current_offset;
			did_adjust = TRUE;
		}
	}

	if (local_offset < max_size)
		while (walker.cur_style->procs.char_info(pg, &walker, text, block->begin,
			offset_begin, offset_end, local_offset, LAST_HALF_BIT | MIDDLE_CHAR_BIT)) {

		*offset += byte_advance;
		
		walker.current_offset += byte_advance;
		local_offset += byte_advance;
		did_adjust = TRUE;

		if (local_offset >= max_size)
			break;
	}

	UnuseMemory(block->text);
	UnuseMemory(pg->t_blocks);
	pgPrepareStyleWalk(pg, 0, NULL, FALSE);
	
	return	did_adjust;
}


/* pgFindEmptyHilite locates any empty highlight pairs (where both offsets are the
same) and returns the number of t_selects to delete and where.  */

PG_PASCAL (pg_short_t) pgFindEmptyHilite (paige_rec_ptr pg, pg_short_t PG_FAR *rec)
{
	pg_short_t			num_delete, num_selects;
	t_select_ptr		select1, select2;
	
	if (!pg->num_selects)
		return	0;

	select1 = select2 = (t_select_ptr) UseMemory(pg->select);
	++select2;

	num_delete = *rec = 0;
	num_selects = pg->num_selects * 2 - 1;

	while (*rec < num_selects) {
		
		if (select1->offset == select2->offset) {
			
			if (pg->num_selects == 1)
				num_delete = 1;
			else
				num_delete = 2;
			
			--pg->num_selects;

			break;
		}
		
		++select1;
		++select2;
		++*rec;
	}
	
	UnuseMemory(pg->select);
	
	return	num_delete;
}



/********************************* Local Functions *****************************/


/* hilite_avoid_list returns a memory_ref of selection pairs for every place
intersecting cur_select that should not be highlighted. Or, if none found, this
function returns NULL.  */

static select_ref hilite_avoid_list (paige_rec_ptr pg, t_select_ptr cur_select,
		pg_short_t cur_sel_qty)
{
	register style_info_ptr	style_base;
	register style_run_ptr	run;
	register long			t_length;
	t_select_ptr			new_list;
	select_pair				test_range;
	select_ref				result;
	
	result = MEM_NULL;
	
	style_base = (style_info_ptr) UseMemory(pg->t_formats);
	run = (style_run_ptr) UseMemory(pg->t_style_run);
	t_length = pg->t_length;

	while (run->offset < t_length) {
		
		if (style_base[run->style_item].class_bits & CANNOT_HILITE_BIT) {
			
			test_range.begin = run->offset;
			if ((test_range.end = run[1].offset) > t_length)
				test_range.end = t_length;
			
			if (pair_intersects(&test_range, cur_select, cur_sel_qty)) {
				
				if (!result)
					result = MemoryAlloc(pg->globals->mem_globals,
							sizeof(t_select), 0, 4);
					
				new_list = (t_select_ptr) AppendMemory(result, 2, FALSE);
				new_list->offset = test_range.begin;
				new_list[1].offset = test_range.end;
				new_list->flags = SELECTION_DIRTY;
				new_list[1].flags = SELECTION_DIRTY;
				
				UnuseMemory(result);
			}
		}
		
		++run;
	}

	UnuseMemory(pg->t_formats);
	UnuseMemory(pg->t_style_run);

	return	result;
}


/* This function returns TRUE if the pair overlaps any selection pair in
cur_select;  cur_sel_qty is the number of pairs in the tested selection. */

static short pair_intersects (select_pair_ptr pair, t_select_ptr cur_select,
		pg_short_t cur_sel_qty)
{
	register t_select_ptr	pair_check;
	register pg_short_t		num_pairs;
	register long			begin, end;
	
	begin = pair->begin;
	end = pair->end;
	
	for (pair_check = cur_select, num_pairs = cur_sel_qty; num_pairs;
			pair_check += 2, --num_pairs)
		if ((!(end < pair_check->offset)) && (!(begin > pair_check[1].offset)))
			return	TRUE;

	return	FALSE;
}



/* This function must insert the appropriate rectangles into the
given shape for the select_pair (which is an array of two t_select records).
The reason this is in "machine" file is the Macintosh might need to alter a
normal selection box in favor of WorldScript discontiguous highlights.  */

static void insert_select_rects (paige_rec_ptr pg, shape_ref rgn,
			t_select_ptr selection)
{
	text_block_ptr			first_block, last_block;
	register t_select_ptr	first_select, last_select;
	rectangle_ptr			wrap_base;
	point_start_ptr			starts;
	rectangle				wrap_rect, insert_rect, begin_rect, ending_rect;
	rectangle				vis_bounds;
	pg_short_t				first_line_begin, last_line_begin;
	pg_short_t				begin_select, begin_r, end_begin_r, end_r;

	first_select = last_select = selection;
	++last_select;
	
	if (first_select->offset > last_select->offset) {
		t_select_ptr		swap;
				
		swap = last_select;
		last_select = first_select;
		first_select = swap; 
	}

	pgShapeBounds(pg->vis_area, &vis_bounds);

	first_block = pgFindTextBlock(pg, first_select->offset, NULL, TRUE, TRUE);
	last_block = pgFindTextBlock(pg, last_select->offset, NULL, FALSE, TRUE);
	UnuseMemory(pg->t_blocks);	/* Is now used only once by me */

	wrap_base = (rectangle_ptr) UseMemory(pg->wrap_area);
	++wrap_base;			/* = first rectangle in wrap area */

	if (first_select->flags & SELECTION_DIRTY)
		pgCalcSelect(pg, first_select);

	if (last_select->flags & SELECTION_DIRTY) {
		
		if ( (first_block->end >= last_block->begin)
			|| (!(last_block->flags & (NEEDS_CALC | SWITCHED_DIRECTIONS)))
			|| (pgShapeDimension(pg->wrap_area) & MULTIPLE_RECT_DIMENSION) )
			pgCalcSelect(pg, last_select);
		else {
			
			/* Second selection is not in tact, but I don't want to force a
			potential huge pagination unless absolutely necessary. At this time
			I know that (A) second selection is not in same block or even the
			next block, (B) wrap shape is not multiple "container" or column
			rects, and (C) block for second selection requires calculating. What
			I shall do is compute as much pagination as necessary that
			intersects the vis region. */
			
			pgOffsetRect(&vis_bounds, pg->scroll_pos.h, pg->scroll_pos.v);

			starts = (point_start_ptr) UseMemoryRecord(first_block->lines, first_select->line, USE_ALL_RECS, TRUE);
			get_wrap_rect(pg, wrap_base, starts->r_num, first_select, &wrap_rect);
			pgInsertHiliteRect(pg, first_block, rgn, first_select, NULL,
					starts->bounds.top_left.v, &wrap_rect, first_select->offset,
					last_select->offset);
			
			insert_rect = wrap_rect;
			insert_rect.top_left.v = starts->bounds.bot_right.v;
			insert_rect.bot_right.v = first_block->bounds.bot_right.v;
			pgAddRectToShape(rgn, &insert_rect);
			
			UnuseMemory(first_block->lines);
			
			while (first_block->end < pg->t_length) {
				
				if (first_block->bounds.bot_right.v > vis_bounds.bot_right.v)
					break;

				++first_block;
				pgPaginateBlock(pg, first_block, NULL, TRUE);
				insert_rect.top_left.v = first_block->bounds.top_left.v;
				insert_rect.bot_right.v = first_block->bounds.bot_right.v;

				if (first_block->begin == last_block->begin) {
					
					pgCalcSelect(pg, last_select);
					starts = (point_start_ptr) UseMemoryRecord(first_block->lines, last_select->line, USE_ALL_RECS, TRUE);
					insert_rect.bot_right.v = starts->bounds.top_left.v;
					pgAddRectToShape(rgn, &insert_rect);
					
					get_wrap_rect(pg, wrap_base, starts->r_num, last_select, &wrap_rect);
					pgInsertHiliteRect(pg, first_block, rgn, NULL, last_select,
							starts->bounds.top_left.v, &wrap_rect, first_select->offset,
							last_select->offset);
					
					UnuseMemory(first_block->lines);
					
					break;
				}
				else
					pgAddRectToShape(rgn, &insert_rect);
			}
			
			UnuseMemory(pg->wrap_area);
			UnuseMemory(pg->t_blocks);
			
			return;
		}
	}
	
	starts = (point_start_ptr) UseMemoryRecord(first_block->lines, first_select->line, USE_ALL_RECS, TRUE);
	begin_r = end_begin_r = (pg_short_t)starts->r_num;
	
	if ((first_block->begin != last_block->begin)
			|| (first_select->line != last_select->line)) {
		
		if (starts[1].flags == TERMINATOR_BITS) {

			pgPaginateBlock(pg, &first_block[1], NULL, TRUE);
			starts = (point_start_ptr) UseMemory(first_block[1].lines);
			end_begin_r = (pg_short_t)starts->r_num;
			UnuseMemory(first_block[1].lines);
		}
		else
			end_begin_r = (pg_short_t)starts[1].r_num;
	}

	UnuseMemory(first_block->lines);
	
	starts = (point_start_ptr) UseMemoryRecord(last_block->lines, last_select->line, USE_ALL_RECS, TRUE);
	end_r = (pg_short_t)starts->r_num;
	UnuseMemory(last_block->lines);

	begin_select = first_select->line;
	pgLineBoundary(pg, first_block, &begin_select, &first_line_begin, NULL);
	begin_select = last_select->line;
	pgLineBoundary(pg, last_block, &begin_select, &last_line_begin, NULL);

	insert_rect.top_left.h = pgRectFromSelection(pg, first_block, first_select, &insert_rect, NULL);
	insert_rect.bot_right.h = pgRectFromSelection(pg, last_block, last_select, &ending_rect, NULL);

	if (selection->flags & VERTICAL_FLAG) {
		
		insert_rect.bot_right.v = ending_rect.bot_right.v;
		
		insert_rect.top_left.h = first_select->original_pt.h;
		insert_rect.bot_right.h = last_select->original_pt.h;
		
		pgSwapLongs(&insert_rect.top_left.h, &insert_rect.bot_right.h);
		pgSwapLongs(&insert_rect.top_left.v, &insert_rect.bot_right.v);
		
		pgAddRectToShape(rgn, &insert_rect);
	}
	else {
		
		get_wrap_rect(pg, wrap_base, begin_r, selection, &wrap_rect);
		
		if ((first_block->begin == last_block->begin) && (first_line_begin == last_line_begin))
			pgInsertHiliteRect(pg, first_block, rgn, first_select, last_select,
					insert_rect.top_left.v, &wrap_rect, first_select->offset,
					last_select->offset);
		else {

			insert_rect.bot_right.h = wrap_rect.bot_right.h;
			
			pgInsertHiliteRect(pg, first_block, rgn, first_select, NULL,
					insert_rect.top_left.v, &wrap_rect, first_select->offset,
					last_select->offset);
					
			pgBlockMove(&insert_rect, &begin_rect, sizeof(rectangle));

			ending_rect.bot_right.h = pgRectFromSelection(pg, last_block, last_select, &ending_rect, NULL);
			get_wrap_rect(pg, wrap_base, end_r, last_select, &wrap_rect);
			ending_rect.top_left.h = wrap_rect.top_left.h;
	
			if ((ending_rect.top_left.v != insert_rect.bot_right.v)
				|| (end_begin_r < end_r)) {
				
				if (pg->doc_info.attributes & PG_LIST_MODE_BIT)
					wrap_rect = vis_bounds;
				else
					get_wrap_rect(pg, wrap_base, end_begin_r, last_select, &wrap_rect);

				insert_rect.top_left.v = insert_rect.bot_right.v;

				for (;;) {

					insert_rect.top_left.h = wrap_rect.top_left.h;
					insert_rect.bot_right.h = wrap_rect.bot_right.h;
					
					if (end_begin_r < end_r)
						insert_rect.bot_right.v = wrap_rect.bot_right.v;
					else
						insert_rect.bot_right.v = ending_rect.top_left.v;
					
					pgAddRectToShape(rgn, &insert_rect);

					++end_begin_r;
					
					if (!(pg->doc_info.attributes & PG_LIST_MODE_BIT))
					get_wrap_rect(pg, wrap_base, end_begin_r, last_select, &wrap_rect);
					
					if (end_begin_r <= end_r)
						insert_rect.top_left.v = wrap_rect.top_left.v;
					else
						break;
				}
			}

			get_wrap_rect(pg, wrap_base, end_r, last_select, &wrap_rect);

			pgInsertHiliteRect(pg, last_block, rgn, NULL, last_select,
					ending_rect.top_left.v, &wrap_rect, first_select->offset,
					last_select->offset);
		}
	}

	UnuseMemory(pg->wrap_area);
	UnuseMemory(pg->t_blocks);
}


/* This function must set wrap_rect to the "real" rectangle based on r_num
in case pg is set for "repeat shape" mode. That means r_num can be > shape size
implying a modulo rect and page offset. */

static void get_wrap_rect (paige_rec_ptr pg, rectangle_ptr wrap_base, 
		long r_num, t_select_ptr from_selection, rectangle_ptr wrap_rect)
{
	co_ordinate		repeat_offset;
	pg_short_t		rect_index;
	
	if (from_selection->flags & SELECT_IN_TABLE)
		pgGetTableBounds(pg, from_selection->offset, wrap_rect);
	else {
	
		rect_index = pgGetWrapRect(pg, r_num, &repeat_offset);
		pgBlockMove(&wrap_base[rect_index], wrap_rect, sizeof(rectangle));
		pgOffsetRect(wrap_rect, repeat_offset.h, repeat_offset.v);
	}
}



/* This function returns the beginning and ending offsets for a word, paragraph
or line.  */

static void return_word_boundary (paige_rec_ptr pg, short modifiers, long offset,
		long PG_FAR *begin, long PG_FAR *end, pg_boolean left_side)
{
	size_t		begin_sel, end_sel;
	select_pair	style_mod_range;

	if (modifiers & (WORD_MOD_BIT | PAR_MOD_BIT | LINE_MOD_BIT | WORD_CTL_MOD_BIT))
		if (modifiers & STYLE_MOD_BIT)
			modifiers ^= STYLE_MOD_BIT;		/* Must not have both */
	
	switch (modifiers & (WORD_MOD_BIT | PAR_MOD_BIT | LINE_MOD_BIT
			| STYLE_MOD_BIT | WORD_CTL_MOD_BIT)) {
		
		case WORD_MOD_BIT:
			pgFindWord(pg->myself, offset, &begin_sel, &end_sel, left_side,
					(pg_boolean)((pg->flags & NO_SMART_CUT_BIT) == 0));
			break;
			
		case PAR_MOD_BIT:
			pgFindPar(pg->myself, offset, &begin_sel, &end_sel);
			break;

		case LINE_MOD_BIT:
			pgFindLine(pg->myself, offset, &begin_sel, &end_sel);
			break;
		
		case STYLE_MOD_BIT:
			pgGetStyleClassInfo(pg->myself, offset, &style_mod_range);
			begin_sel = style_mod_range.begin;
			end_sel = style_mod_range.end;
			break;
		
		case WORD_CTL_MOD_BIT:
			pgFindCtlWord(pg->myself, offset, &begin_sel, &end_sel, TRUE);
			break;
		
		default:
			begin_sel = end_sel = offset;
			break;
	}
	
	if (begin)
		*begin = begin_sel;
	
	if (end)
		*end = end_sel;
}


/* This function flips two selections (selection is a pointer to a select pair ) */

static void reverse_selection (t_select_ptr selection)
{
	t_select			hold_select;
	
	pgBlockMove(selection, &hold_select, sizeof(t_select));
	pgBlockMove(&selection[1], selection, sizeof(t_select));
	pgBlockMove(&hold_select, &selection[1], sizeof(t_select));
}


/* This function returns the <<used>> pointer to the current selection pair. */
static t_select_ptr current_selection_pair (paige_rec_ptr pg)
{
	if (!pg->num_selects)
		return (t_select_ptr)UseMemory(pg->select);

	return (t_select_ptr)UseMemoryRecord(pg->select, pg->num_selects * 2 - 2, USE_ALL_RECS, TRUE);
}


/* This function sets up whatever is needed to call the track_control_proc for
the style. It is already known that the dragging selection began on a control
style. The result is the same result returned from track_ctl function.  */

static long track_style_control (paige_rec_ptr pg, t_select_ptr new_select,
	long track_refcon, short modifiers, short verb)
{
	style_walk			walker;
	t_select			first_select;
	t_select_ptr		selections;
	text_block_ptr		block;
	point_start_ptr		starts;
	pg_char_ptr			examine_text;
	long				text_offset, local_offset, refcon_result;
	pg_short_t			first_sel_rec;

	if (verb == mouse_down) {
		
		if (!(modifiers & EXTEND_MOD_BIT)) {

			pgRemoveAllHilites(pg, TRUE);
			pg->num_selects = 0;
			SetMemorySize(pg->select, 2);
			
			selections = (t_select_ptr) UseMemory(pg->select);
			*selections = selections[1] = *new_select;
			
			UnuseMemory(pg->select);
		}
	}

	if (pg->num_selects)
		first_sel_rec = (pg->num_selects * 2) - 2;
	else
		first_sel_rec = 0;
	
	GetMemoryRecord(pg->select, first_sel_rec, &first_select);

	pgPrepareStyleWalk(pg, first_select.control_offset, &walker, TRUE);
	text_offset = walker.prev_style_run->offset;
	pgSetWalkStyle(&walker, text_offset);

	block = pgFindTextBlock(pg, text_offset, NULL, TRUE, TRUE);
	local_offset = text_offset - block->begin;
	examine_text = (pg_char_ptr) UseMemory(block->text);
	starts = (point_start_ptr) UseMemory(block->lines);

	refcon_result = walker.cur_style->procs.track_ctl(pg, verb, &first_select,
			new_select, &walker, &examine_text[local_offset], &starts[first_select.line],
			modifiers, track_refcon);

	UnuseMemory(block->lines);
	UnuseMemory(block->text);
	UnuseMemory(pg->t_blocks);
	pgPrepareStyleWalk(pg, 0, NULL, TRUE);

	return	refcon_result;
}


/* This functions checks to see if the current highlight region requires
a re-build and, if so, builds it.  */

static void update_hilite_rgn (paige_rec_ptr pg)
{
	t_select_ptr	selections;
	pg_boolean		selection_dirty;

	if (pg->num_selects) {
		
		selection_dirty = (is_selection_dirty(pg) || pgEmptyShape(pg->hilite_rgn));

		if (selection_dirty) {
			
			selections = (t_select_ptr) UseMemory(pg->select);
			pgBuildHiliteRgn(pg, selections, pg->num_selects, pg->hilite_rgn);
			UnuseMemory(pg->select);
		}
	}
	else
		pgSetShapeRect(pg->hilite_rgn, NULL);
}


/* This function returns TRUE if any part of the selection is dirty (needs calc).
This works correctly even for a "caret" selection. */

static pg_boolean is_selection_dirty (paige_rec_ptr pg)
{
	register t_select_ptr		selections;
	register pg_short_t			num_selections;
	pg_boolean					result;
	
	result = FALSE;
	selections = (t_select_ptr) UseMemory(pg->select);
	num_selections = pg->num_selects;
	
	if (!num_selections)
		result = ( (selections->flags & SELECTION_DIRTY) != 0 );
	else
		for (num_selections *= 2; num_selections; ++selections, --num_selections)
			if (result = ((selections->flags & SELECTION_DIRTY) != 0 ))
				break;
	
	UnuseMemory(pg->select);
	
	return	result;
}


/* The following function is called from several places to change the current
selection range and possibly update the highlighting (visually). Both
pgSetSelection and pgDragSelect use this function to name a few.  The new_select
parameter is the new point to extend to; modifiers define the type of selection
extension; drag_verb is either "mouse_moved" or "mouse_down," implying either
an extension to the current selection or a completely new one. Note that the
drag_verb is not necessarily the one passed by the app, but rather an indication
whether or not to extend the selection (mouse moved) or start a new one
(mouse_down). Nothing is actually drawn unless should_draw is true.  */

static void extend_selection (paige_rec_ptr pg, t_select_ptr new_select,
		short modifiers, short drag_verb, short show_new_caret, short should_draw)
{
	long				begin_sel, end_sel;
	short				will_draw, word_verb;
	t_select_ptr		current_select;

	if (word_verb = modifiers & (WORD_MOD_BIT | PAR_MOD_BIT | LINE_MOD_BIT
				| STYLE_MOD_BIT | WORD_CTL_MOD_BIT))
		new_select->flags |= WORD_FLAG;
	
	new_select->word_offsets.begin = new_select->word_offsets.end = new_select->offset;

	if (will_draw = (should_draw && (!(pg->flags & DEACT_BITS)))) {
		
		if (!pg->num_selects)
			if ((pgCurrentInsertion(pg) != new_select->offset) || word_verb)
					pgTurnOffHighlight(pg, FALSE);

		update_hilite_rgn(pg);
		MemoryCopy(pg->hilite_rgn, pg->temp_rgn);
	}

	if (drag_verb == mouse_down) {
		
		if (modifiers & DIS_MOD_BIT)
			++pg->num_selects;
		else 
			pg->num_selects = 0;

		SetMemorySize(pg->select, (pg->num_selects * 2) + MINIMUM_SELECT_MEMSIZE);
		
		current_select = current_selection_pair(pg);

		pgBlockMove(new_select, current_select, sizeof(t_select));
		pgBlockMove(new_select, &current_select[1], sizeof(t_select));

		if (word_verb) {
			
			return_word_boundary(pg, modifiers, new_select->offset,
					&begin_sel, &end_sel, (pg_boolean)FALSE);
			
			current_select->word_offsets.begin
				= current_select[1].word_offsets.begin = begin_sel;
			current_select->word_offsets.end
				= current_select[1].word_offsets.end = end_sel;

			current_select->flags &= CLR_END_LINE_FLAG;
			current_select[1].flags &= CLR_END_LINE_FLAG;

			if (modifiers & WORD_CTL_MOD_BIT)
				current_select[1].flags |= END_LINE_FLAG;

			current_select->offset = begin_sel;
			current_select[1].offset = end_sel;
			current_select->flags |= SELECTION_DIRTY;
			current_select[1].flags |= SELECTION_DIRTY;
			
			pg->first_word.begin = pg->last_word.begin = begin_sel;
			pg->first_word.end = pg->last_word.end = end_sel;
		}
	}
	else {	/* Extended selection */

		current_select = current_selection_pair(pg);
		
		if (modifiers & PIVOT_EXTEND_BIT) {
		
			if ((modifiers & EXTEND_MOD_BIT) && pg->num_selects)
				if (!(modifiers & VERTICAL_MOD_BIT))
					if (pg->hilite_anchor > current_select->offset)
							reverse_selection(current_select);
		}
		else {

			if ((modifiers & EXTEND_MOD_BIT) && pg->num_selects)
				if (!(modifiers & VERTICAL_MOD_BIT))
					if (pgAbsoluteValue(new_select->offset - current_select[1].offset)
						> pgAbsoluteValue(new_select->offset - current_select->offset))
							reverse_selection(current_select);
		}

		if (word_verb) {
			pg_boolean	left_side;
		
			left_side = (current_select[1].offset < current_select->offset);

			return_word_boundary(pg, modifiers, new_select->offset, &begin_sel, &end_sel,
					(pg_boolean)(left_side && (modifiers & (WORD_MOD_BIT | STYLE_MOD_BIT))));
			
			current_select[1].word_offsets.begin = begin_sel;
			current_select[1].word_offsets.end = end_sel;

			if ((begin_sel != pg->last_word.begin) || (end_sel != pg->last_word.end)) {
				
				pg->last_word.begin = begin_sel;
				pg->last_word.end = end_sel;

				if (left_side) {
					
					if (end_sel > pg->first_word.begin)
							current_select[1].offset = end_sel;
					else
						current_select[1].offset = begin_sel;
				}
				else {
					
					if (begin_sel < pg->first_word.end)
							current_select[1].offset = begin_sel;
					else
						current_select[1].offset = end_sel;
				}
				
				current_select[1].flags |= SELECTION_DIRTY;

				current_select->flags &= CLR_END_LINE_FLAG;
				current_select[1].flags &= CLR_END_LINE_FLAG;

				if (current_select[1].offset >= current_select->offset) {
					
					if (modifiers & WORD_CTL_MOD_BIT)
						current_select[1].flags |= END_LINE_FLAG;
						
					if (current_select->offset > pg->first_word.begin) {
						
						current_select->offset = pg->first_word.begin;
						current_select->flags |= SELECTION_DIRTY;
					}
					
					if (current_select[1].offset < pg->first_word.end)
						current_select[1].offset = pg->first_word.end;
				}
				else {

					if (modifiers & WORD_CTL_MOD_BIT)
						current_select->flags |= END_LINE_FLAG;

					if (current_select->offset < pg->first_word.end) {

						current_select->offset = pg->first_word.end;
						current_select->flags |= SELECTION_DIRTY;
					}
					
					if (current_select[1].offset > pg->first_word.begin)
						current_select[1].offset = pg->first_word.begin;
				}
			}
		}
		else
			pgBlockMove(new_select, &current_select[1], sizeof(t_select));
	}
	
	if (modifiers & VERTICAL_MOD_BIT) {
		short			extra_select_flags;

		extra_select_flags = VERTICAL_FLAG;
		if (modifiers & WORD_CTL_MOD_BIT)
			extra_select_flags |= VERTICAL_CTL_FLAG;
		
		extra_select_flags |= SELECTION_DIRTY;
		current_select->flags |= extra_select_flags;
		current_select[1].flags |= extra_select_flags;
	}

	if (current_select->offset != current_select[1].offset) {
		
		if (!pg->num_selects)
			++pg->num_selects;
	}
	else {
		
		if (pg->num_selects == 1)
			pg->num_selects = 0;
	}

	if (will_draw) {
		shape_ref			diff_shape;
		
		pgBuildHiliteRgn(pg, (t_select_ptr)UseMemory(pg->select), pg->num_selects, pg->hilite_rgn);
		UnuseMemory(pg->select);

		if (pgEmptyShape(pg->temp_rgn)) {
			
			if (!pgEmptyShape(pg->hilite_rgn))
				pg->procs.hilite_draw(pg, pg->hilite_rgn);
		}
		else
		if (pgEmptyShape(pg->hilite_rgn))
			pg->procs.hilite_draw(pg, pg->temp_rgn);
		else {
			
			if (!pgEqualShapes(pg->hilite_rgn, pg->temp_rgn)) {
				
				diff_shape = pgRectToShape(pg->globals->mem_globals, NULL);
				
				pgDiffShape(pg->temp_rgn, pg->hilite_rgn, diff_shape);
				if (!pgEmptyShape(diff_shape))
					pg->procs.hilite_draw(pg, diff_shape);
				
				pgDiffShape(pg->hilite_rgn, pg->temp_rgn, diff_shape);
				if (!pgEmptyShape(diff_shape))
					pg->procs.hilite_draw(pg, diff_shape);
					
				pgDisposeShape(diff_shape);
			}
		}

		if (!pg->num_selects)
			if (show_new_caret)
				pg->procs.cursor_proc(pg, current_select, show_cursor);
	}

	UnuseMemory(pg->select);
}



/* fix_restricted_hilite checks to see if point goes beyond the range
of restricted highlighting.  This function only gets called by pgDragSelect
and only if the entry verb is mouse_moved.  */

static void fix_restricted_hilite (paige_rec_ptr pg, co_ordinate_ptr point)
{
	t_select_ptr		original_select;
	style_info			wanted_info, mask, AND_mask;
	select_pair			style_range;
	rectangle			text_box;
	pg_boolean			exclude_any = FALSE;

	original_select = current_selection_pair(pg);
	style_range.begin = style_range.end = 0;
	
	if (pgGetStyleClassInfo(pg->myself, original_select->offset, &style_range)
			& HILITE_RESTRICT_BIT) {
		
		exclude_any = TRUE;

		pgInitStyleMask(&mask, 0);
		wanted_info.class_bits = AND_mask.class_bits = HILITE_RESTRICT_BIT;
		mask.class_bits = -1;
		
		pgFindStyleInfo(pg->myself, &style_range.begin, &style_range.end,
				&wanted_info, &mask, &AND_mask);
	}
	
	if (exclude_any && (style_range.begin < style_range.end)) {
	
		pgTextRect(pg->myself, &style_range, FALSE, TRUE, &text_box);
		
		if (point->h <= text_box.top_left.h)
			point->h = text_box.top_left.h + 1;
		if (point->h >= text_box.bot_right.h)
			point->h = text_box.bot_right.h - 1;
		if (point->v <= text_box.top_left.v)
			point->v = text_box.top_left.v + 1;
		if (point->v >= text_box.bot_right.v)
			point->v = text_box.bot_right.v - 1;
	}
	
	UnuseMemory(pg->select);
}


/* This function is called to compute the corners of a vertical/word selection. */

static void compute_vertical_corners (paige_rec_ptr pg, t_select_ptr selections)
{
	t_select_ptr	select1, select2;
	rectangle		word_rect1, word_rect2, union_rect;
	
	if (selections[1].offset < selections->offset) {

		select1 = &selections[1];
		select2 = selections;
	}
	else {

		select1 = selections;
		select2 = &selections[1];
	}

	if (select1->word_offsets.end == select1->word_offsets.begin) {

		pgCharacterRect(pg->myself, select1->offset, FALSE, FALSE, &word_rect1);
		pgCharacterRect(pg->myself, select2->offset, FALSE, FALSE, &word_rect2);
		
		if (word_rect1.top_left.h < word_rect2.top_left.h) {
		
			selections->original_pt.h = word_rect1.top_left.h;
			selections[1].original_pt.h = word_rect2.top_left.h;
		}
		else {

			selections->original_pt.h = word_rect2.top_left.h;
			selections[1].original_pt.h = word_rect1.top_left.h;
		}
		
		if (selections->original_pt.h == selections[1].original_pt.h) {
			
			++selections[1].original_pt.h;
			--selections->original_pt.h;
		}
	}
	else {

		pgTextRect(pg->myself, &select1->word_offsets, FALSE, FALSE, &word_rect1);
		pgTextRect(pg->myself, &select2->word_offsets, FALSE, FALSE, &word_rect2);
		pgUnionRect(&word_rect1, &word_rect2, &union_rect);
		
		selections->original_pt.h = union_rect.top_left.h;
		selections[1].original_pt.h = union_rect.bot_right.h;
	}
}

/* This functions does pgUnionRect(s) of all text between the two selection
points given.  If *first_baseline is 0, the baseline of the first point_start
is returned in *first_baseline. */

static void add_to_text_rect (paige_rec_ptr pg, select_pair_ptr selection,
		rectangle_ptr rect, short PG_FAR *first_baseline)
{
	text_block_ptr						first_block, last_block;
	register point_start_ptr			starts;
	register pg_short_t					start_ctr;
	rectangle							last_rect;
	t_select							first_pt, last_pt;
	long								global_end;
	short								baseline;

	first_pt.offset = selection->begin;
	last_pt.offset = selection->end;
	first_pt.flags = last_pt.flags = 0;
	
	pgCalcSelect(pg, &first_pt);
	pgCalcSelect(pg, &last_pt);

	first_block = pgFindTextBlock(pg, first_pt.offset, NULL, TRUE, TRUE);

// Determine if the second point is on the same line (fixes bug with end-char select):

	starts = (point_start_ptr) UseMemory(first_block->lines);
	starts += first_pt.line;
	
	while (!(starts->flags & LINE_BREAK_BIT)) {
		
		if (starts[1].flags == TERMINATOR_BITS)
			break;
		
		starts += 1;
	}
	
	global_end = (long)starts[1].offset;
	global_end += first_block->begin;
	UnuseMemory(first_block->lines);

	if (global_end == selection->end && global_end < pg->t_length) {
		
		last_pt.flags = END_LINE_FLAG;
		pgCalcSelect(pg, &last_pt);
	}

	last_block = pgFindTextBlock(pg, last_pt.offset, NULL, TRUE, TRUE);
	UnuseMemory(pg->t_blocks);

	rect->top_left.h = pgRectFromSelection(pg, first_block, &first_pt, rect, &baseline);
	last_rect.bot_right.h = pgRectFromSelection(pg, last_block, &last_pt, &last_rect, NULL);
	
	if (*first_baseline == 0)
		*first_baseline = baseline;

	if (last_rect.top_left.v == rect->top_left.v)
		rect->bot_right.h = last_rect.bot_right.h;
	else {
		
		starts = (point_start_ptr) UseMemory(first_block->lines);
		start_ctr = first_pt.line;
		starts += start_ctr;

		while (last_block->begin != first_block->begin) {
			
			if (!starts->offset)
				pgTrueUnionRect(&first_block->bounds, rect, rect);
			else
				while (starts->flags != TERMINATOR_BITS) {
					
					pgTrueUnionRect(&starts->bounds, rect, rect);
					++starts;
				}
			
			UnuseMemory(first_block->lines);
			
			++first_block;
			starts = (point_start_ptr) UseMemory(first_block->lines);
			start_ctr = 0;
		}
		
		while (start_ctr < last_pt.line) {
		
			pgTrueUnionRect(&starts->bounds, rect, rect);
			++starts;
			++start_ctr;
		}
		
		last_rect = starts->bounds;
		last_rect.bot_right.h = last_rect.top_left.h + last_pt.primary_caret;
		
		pgTrueUnionRect(&last_rect, rect, rect);
		
		UnuseMemory(first_block->lines);
	}

	UnuseMemory(pg->t_blocks);
}


/* This function returns the best text_block and point_start number in *block_num
and *start_num respectively that contains point. If no exact match is found then
a best-guess method is used. */

static void point_to_start (paige_rec_ptr pg, co_ordinate_ptr point,
		pg_short_t PG_FAR *block_num, pg_short_t PG_FAR *start_num)
{
	text_block_ptr	block;
	rectangle		bounds_test;
	long			required_r_num = 0;
	long			r_qty;
	pg_short_t		num_blocks, block_index;
	
	if (pg->doc_info.attributes & BOTTOM_FIXED_BIT) {
		co_ordinate			repeat_offset, inset;
		rectangle_ptr		page_ptr;
		long				r_index, page_num;
		
		r_qty = GetMemorySize(pg->wrap_area) - 1;
		repeat_offset.h = repeat_offset.v = page_num = 0;
		
		if (pg->doc_info.attributes & V_REPEAT_BIT)
			page_num = (long)pgPixelToPage(pg->myself, point, &repeat_offset, NULL, NULL, NULL, FALSE);
		
		inset.h = inset.v = 0;
		required_r_num = -1;
		page_ptr = (rectangle_ptr) UseMemory(pg->wrap_area);
		++page_ptr;

		while (required_r_num < 0) {
			
			for (r_index = 0; r_index < r_qty; ++r_index) {
				
				bounds_test = page_ptr[r_index];
				pgOffsetRect(&bounds_test, repeat_offset.h, repeat_offset.v);

				if (pgPtInRectInset(point, &bounds_test, &inset, NULL)) {
					
					required_r_num = (page_num * r_qty) + r_index;
					break;
				}
			}
			
			inset.h -= 16;
			inset.v -= 16;
		}
		
		UnuseMemory(pg->wrap_area);
	}

	block = (text_block_ptr) UseMemory(pg->t_blocks);
	num_blocks = (pg_short_t)GetMemorySize(pg->t_blocks);

	for (block_index = 0; block_index < num_blocks; ++block_index, ++block) {
		
		if (!(block->flags & (NEEDS_PAGINATE | NEEDS_CALC))) {
			
			if (block_index == (num_blocks - 1)) {
			
				if (point_to_exact_start(pg, block, point, required_r_num, start_num, FALSE))
					break;
			}
			else
			if (point->v < block->bounds.bot_right.v)
				if (point_to_exact_start(pg, block, point, required_r_num, start_num, FALSE))
					break;
		}
	}
	
	if (block_index == num_blocks) {
		
		block = (text_block_ptr) UseMemoryRecord(pg->t_blocks, 0, 0, FALSE);

		for (block_index = 0; block_index < num_blocks; ++block_index, ++block) {
			
			if (!(block->flags & (NEEDS_PAGINATE | NEEDS_CALC))) {
				
				if (block_index == (num_blocks - 1)) {
				
					point_to_exact_start(pg, block, point, required_r_num, start_num, TRUE);
					break;
				}
				else
				if (point->v < block->bounds.bot_right.v) {
				
					point_to_exact_start(pg, block, point, required_r_num, start_num, TRUE);
					break;
				}
			}
		}
	}
	
	if (block_index == num_blocks) {
	
		*start_num = 0;
		block_index = 0;
	}
	
	*block_num = block_index;
	
	UnuseMemory(pg->t_blocks);
}


/* point_to_exact_start returns the point start number that logical contains point. The point
is not necessarily within the start for a "match" to be found, for example if clicking to
the right of a column or right side of a table results in a true, logical match of the
rightmost cell.  Also if must_find_one is TRUE then a point_start MUST be located, otherwise
FALSE is returned if nothing really matches. */

static pg_boolean point_to_exact_start (paige_rec_ptr pg, text_block_ptr block, co_ordinate_ptr point,
			long required_r_num, pg_short_t PG_FAR *start_num, pg_boolean must_find_one)
{
	register point_start_ptr	starts, end_starts;
	pg_boolean					result = FALSE;
	pg_short_t					num_starts, starts_index, ending_starts_index;

	if (block->flags & LINES_PURGED)
		pgPaginateBlock(pg, block, NULL, TRUE);
	
	starts = (point_start_ptr) UseMemory(block->lines);
	starts_index = ending_starts_index = 0;

	while (starts->flags != TERMINATOR_BITS) {
		
		end_starts = starts;
		num_starts = 1;

		while (!(end_starts->flags & LINE_BREAK_BIT)) {
			
			++end_starts;
			++num_starts;
		}
		
		if (starts->r_num == required_r_num) {
			
			if (end_starts[1].flags == TERMINATOR_BITS || end_starts[1].r_num != required_r_num
						|| (starts->bounds.bot_right.v + (long)starts->cell_height) > point->v) {
				
				result = TRUE;
				break;
			}
		}
		
		starts += num_starts;
		ending_starts_index = starts_index;
		starts_index += num_starts;
	}
	
	if (!result && must_find_one) {
		
		starts = (point_start_ptr) UseMemoryRecord(block->lines, (long)ending_starts_index, 0, FALSE);
		starts_index = ending_starts_index;
		result = TRUE;
	}

	if (result) {
		
		if (starts->cell_num) {
			/* The point is within a table, hence we need to scan for the exact cell
			of the resulting line may be incorrect. */
			select_pair			table_offsets;
			rectangle			page_bounds;
			par_info_ptr		par;
			long				abs_position, r_qty, max_table_width;
			pg_short_t			local_position, required_column;

			abs_position = (long)starts->offset;
			abs_position += block->begin;
			par = pgFindParStyle(pg, abs_position);
			
			pgTableOffsets(pg->myself, abs_position, &table_offsets);
			
			if (table_offsets.begin < block->begin)
				table_offsets.begin = block->begin;
			
			local_position = (pg_short_t)(table_offsets.begin - block->begin);
			starts = (point_start_ptr) UseMemoryRecord(block->lines, 0, 0, FALSE);
			starts_index = ending_starts_index = 0;

			while (starts[1].offset <= local_position) {
				
				++starts_index;
				++starts;
			}
			
			ending_starts_index = starts_index;

			r_qty = GetMemorySize(pg->wrap_area) - 1;
			GetMemoryRecord(pg->wrap_area, (starts->r_num % r_qty) + 1, &page_bounds);
			max_table_width = pgSectColumnBounds(pg, par, 0, starts->r_num, &page_bounds);
			pgOffsetRect(&page_bounds, pgTableJustifyOffset(par, max_table_width), 0);

			for (required_column = 1; required_column < (pg_short_t)par->table.table_columns; ++required_column) {
				long		column_width;
				
				if (page_bounds.bot_right.h > point->h)
					break;
				
				if ((column_width = par->tabs[required_column].position) == 0)
					column_width = par->column_var;

				page_bounds.bot_right.h += column_width;
			}

			UnuseMemory(pg->par_formats);
			
			for (;;) {
			
				end_starts = starts;
				num_starts = 1;

				while (!(end_starts->flags & LINE_BREAK_BIT)) {
					
					++end_starts;
					++num_starts;
				}
				
				if (end_starts[1].flags == TERMINATOR_BITS)
					break;

				if ((starts->cell_num & CELL_NUM_MASK) == required_column) {
					
					if (!end_starts[1].cell_num || (starts->bounds.bot_right.v + starts->cell_height) > point->v)
						break;
				}
				
				starts += num_starts;
				ending_starts_index = starts_index;
				starts_index += num_starts;
			}
		}

		while (!(starts->flags & LINE_BREAK_BIT)) {
			
			if (starts->bounds.bot_right.h > point->h)
				break;
			
			++starts;
			++starts_index;
		}
		
		*start_num = starts_index;
	}

	UnuseMemory(block->lines);
	
	return	result;
}


/* If all text is hidden and "Hide Hidden Text" is turned on. */

static pg_boolean whole_doc_hidden (paige_rec_ptr pg)
{
	register text_block_ptr	blocks;
	register long			num_blocks;
	
	blocks = (text_block_ptr) UseMemory(pg->t_blocks);
	
	for (num_blocks = GetMemorySize(pg->t_blocks); num_blocks; ++blocks, --num_blocks)
		if (!(blocks->flags & ALL_TEXT_HIDDEN)) {
			
			UnuseMemory(pg->t_blocks);
			return	FALSE;
		}

	UnuseMemory(pg->t_blocks);
	
	return	TRUE;
}

/* hyperlink_callback calls the hyperlink function if the point is in one. The offset of the
hyperlink + 1 is returned if one was clicked. */

static long hyperlink_callback (paige_rec_ptr pg, short verb, short modifiers, memory_ref hyperlinks)
{
	pg_hyperlink_ptr		link;
	pg_char_ptr				URL;
	memory_ref				URL_ref;
	pg_hyperlink			callback_link;
	size_t					position;
	long					result = 0;
	short					command;
	
	if (modifiers & ARROW_ACTIVE_BIT)
		return	0;

	if (verb == mouse_down) {
		
		if (modifiers & WORD_MOD_BIT)
			command = hyperlink_doubleclick_verb;
		else
			command = hyperlink_mousedown_verb;
	}
	else
		command = hyperlink_mouseup_verb;
	
	pgGetSelection(pg->myself, &position, NULL);

	link = pgFindHypertextRun(hyperlinks, position, NULL);

	if (position >= link->applied_range.begin && position < link->applied_range.end) {
		
		callback_link = *link;
		UnuseMemory(hyperlinks);

		position = callback_link.applied_range.begin;
		
		if (callback_link.alt_URL != MEM_NULL) {
			
			URL_ref = MemoryDuplicate(callback_link.alt_URL);
			URL = (pg_char_ptr) UseMemory(URL_ref);
		}
		else {
		
			URL = callback_link.URL;
			URL_ref = MEM_NULL;
		}
		
		result = callback_link.callback(pg, &callback_link, command, modifiers, position, URL);

		if (URL_ref)
			UnuseAndDispose(URL_ref);

		return		result;
	}
	
	UnuseMemory(hyperlinks);
	
	return		result;
}

