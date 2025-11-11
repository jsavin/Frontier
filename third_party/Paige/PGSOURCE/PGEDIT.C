/* This file handles all display, insertions and deletions (basic editing) */

#include "Paige.h"

#ifdef MAC_PLATFORM
#pragma segment pgbasic1
#endif

#include "machine.h"
#include "pgUtils.h"
#include "defprocs.h"
#include "pgText.h"
#include "pgPar.h"
#include "pgSelect.h"
#include "pgDefStl.h"
#include "pgDefPar.h"
#include "pgShapes.h"
#include "pgEdit.h"
#include "pgStyles.h"
#include "pgScript.h"
#include "pgErrors.h"
#include "pgSubRef.h"
#include "pgTxtWid.h"
#include "pgHText.h"
#include "pgTables.h"
#include "pgGrafx.h"


struct glitter_info {
	style_walk		walker;					/* Copy of walker for line or par begin */
	point_start_ptr	par_begin;				/* First start for paragraph */
	long			par_number;				/* Current paragraph number */
	long			line_number;			/* Current line number */
	long			last_printed;			/* Last offset "printed" */
};
typedef struct glitter_info glitter_info;
typedef glitter_info PG_FAR *glitter_info_ptr;

struct graphic_line_info {
	rectangle		bounds;					/* area that the line covered */
	long			r_num;					/* line rectangle number (could make a difference) */
	long			cell_num;				/* The cell number, if any */
};
typedef struct graphic_line_info graphic_line_info;
typedef graphic_line_info PG_FAR *graphic_line_ptr;

static void do_partial_display (paige_rec_ptr pg, long draw_from,
		pg_short_t num_bytes, short draw_mode);
static void advance_text_runs (paige_rec_ptr pg, text_block_ptr block,
		pg_short_t block_num, long offset, long length, pg_boolean advance_equal_styles,
		pg_boolean advance_equal_pars, pg_boolean deleted_block);
static void fix_deleted_pars (paige_rec_ptr pg, long deleted_from, long deleted_to);
static void advance_style_run (style_ref the_run, long offset, long length,
		pg_boolean include_equal_offsets);
static void insert_key_data (paige_rec_ptr pg, pg_char_ptr data, size_t length,
		short modifiers, short draw_mode);
static void dump_key_data (paige_rec_ptr pg, style_info_ptr style,
		pg_char_ptr data, long offset, long length, short draw_mode);
static void handle_action_key (paige_rec_ptr pg, pg_char_ptr data, long offset,
			short modifiers, short draw_mode);
static long align_selection (paige_rec_ptr pg, long position, long byte_advance);
static long increment_selection (paige_rec_ptr pg, long num_chars, short modifiers, short set_it);
static void increment_line_select(paige_rec_ptr pg, short modifiers, short direction);
static void advance_line_run (paige_rec_ptr pg, text_block_ptr block, pg_short_t offset, long amount);
static void clear_line_end_flag (paige_rec_ptr pg);
static short block_should_delete (paige_rec_ptr pg, text_block_ptr block,
		pg_short_t block_num, pg_short_t local_offset, pg_short_t local_length);
static short line_has_custom_styles (point_start_ptr line_starts);
static void erase_top_or_bottom (paige_rec_ptr pg, short dont_draw, pg_boolean do_bottom);
static void erase_top_margin (paige_rec_ptr pg);
static void erase_from_last_line (paige_rec_ptr pg, co_ordinate_ptr vis_offset,
		graphic_line_ptr last_line, graphic_line_ptr cur_line);
static void erase_end_display (paige_rec_ptr pg, text_block_ptr block, co_ordinate_ptr vis_offset,
		graphic_line_ptr last_line, long last_drawn_offset);
static pg_short_t get_num_pages (paige_rec_ptr pg);
static void extend_visual_pages (paige_rec_ptr pg, short draw_mode);
static pg_boolean has_any_controls (paige_rec_ptr pg, pg_char_ptr text, size_t length);
static void scale_drawing (paige_rec_ptr pg, co_ordinate_ptr amount_offset,
		draw_points_ptr draw_position);
static pg_boolean can_quit_display (paige_rec_ptr pg, point_start_ptr line_start,
		rectangle_ptr vis_rect, co_ordinate_ptr offset_extra, rectangle_ptr wrap_base,
		pg_short_t wrap_qty);
static void set_fastest_drawing_path (paige_rec_ptr pg, text_block_ptr block,
		long offset, long length, smart_update_ptr update_info);
static void fill_pages (paige_rec_ptr pg, shape_ptr page_shape,
		pg_short_t r_qty, pg_short_t r_num, co_ordinate_ptr offset);
static void fix_non_linefeed_update (paige_rec_ptr pg, text_block_ptr block,
		smart_update_ptr update_info);
static pg_boolean special_action_key (paige_rec_ptr pg, long will_insert_here, pg_char_ptr the_key);
static long draw_line (paige_rec_ptr pg, text_block_ptr block, pg_char_ptr text,
		point_start_ptr first_start, style_walk_ptr walker, co_ordinate_ptr offset_extra,
		rectangle_ptr vis_rect, rectangle_ptr wrap_base, glitter_info_ptr glitter,
		long PG_FAR *actual_printed, graphic_line_ptr drawn_line_info, short draw_mode);
static long advance_to_next_line (point_start_ptr first_start);
static void compute_top_and_bottom (text_block_ptr block, pg_short_t starts_offset,
		long PG_FAR *top, long PG_FAR *bottom);
static void set_caret (paige_rec_ptr pg, short verb);
static void delete_style_records (paige_rec_ptr pg, long first_rec, long num_recs);
static void delete_par_records (paige_rec_ptr pg, long first_rec, long num_recs);
static void delete_par_exclusions (paige_rec_ptr pg, long position, long length);
static long update_text (paige_rec_ptr pg, graf_device_ptr draw_port,
		long beginning_offset, long ending_offset, shape_ref vis_rgn,
		co_ordinate_ptr offset_extra, short display_verb, short hilite_mode);
static pg_boolean is_subref_char (text_block_ptr block, pg_char_ptr text, pg_short_t offset,
					pg_short_t text_length);


/* pgTextIncrementProc gets called when text has been inserted or deleted.
The begininning offset is base_offset and amount is increment_amount. If
increment_amount is negative, text has been deleted from base_offset to
-increment_amount.  */

PG_PASCAL (void) pgTextIncrementProc (paige_rec_ptr pg, long base_offset,
		long increment_amount)
{

}


/* pgSmartQuotesProc handles "smart quote" characters by checking if there is
another complimentary quote (based on info_bits) in the text stream in which
*char_to_change is about to be inserted. If so, *char_to_insert can be altered,
if appropriate, for a smart quote. NOTE, this function does not get called
unless *char_to_insert is known to be a quote character of some type.
To accommodate for input and/or output of multi-byte chars, *insert_length
will initially be set to the number of bytes of the input character, and
char_to_insert will be <at least> large enough to accommodate a 4-byte char.
If the data is changed, *insert_length should be changed to the number of
bytes of the new character. */

PG_PASCAL (void) pgSmartQuotesProc (paige_rec_ptr pg, long insert_offset,
		long info_bits, pg_char_ptr char_to_insert, short PG_FAR *insert_length)
{
	register pg_globals_ptr		globals;
	register text_block_ptr		block;
	register long				global_offset;
	pg_char_ptr					text, new_char;
	style_walk					walker;
	long						left_info, end_offset;
	
	if (!(info_bits & FLAT_QUOTE_BIT))
		return;

	globals = pg->globals;
	block = pgFindTextBlock(pg, insert_offset, NULL, FALSE, TRUE);
	pgPrepareStyleWalk(pg, insert_offset, &walker, FALSE);

	text = (pg_char_ptr) UseMemory(block->text);

	global_offset = insert_offset;

	if (global_offset == block->begin)
		left_info = BLANK_BIT;
	else {
		
		end_offset = block->end - block->begin;

		while (global_offset > block->begin) {
			
			pgWalkStyle(&walker, -1);
			global_offset -= 1;

			left_info = walker.cur_style->procs.char_info(pg, &walker, text,
					block->begin, 0, end_offset, global_offset - block->begin,
						BLANK_BIT | LAST_HALF_BIT | MIDDLE_CHAR_BIT | TAB_BIT | PAR_SEL_BIT);	// PAJ

			if (!(left_info & (LAST_HALF_BIT | MIDDLE_CHAR_BIT)))
				break;
		}
	}
	
	if (!(left_info & BLANK_BIT)) {  /* char on left non breaking */
		
		if (info_bits & SINGLE_QUOTE_BIT)
			new_char = &globals->right_single_quote[1];
		else
			new_char = &globals->right_double_quote[1];
	}
	else {	/* char on left is breaking char */
		
		if (info_bits & SINGLE_QUOTE_BIT)
			new_char = &globals->left_single_quote[1];
		else
			new_char = &globals->left_double_quote[1];
	}

#ifdef UNICODE
	*insert_length = 1;
	*char_to_insert = *new_char;
#else
	*insert_length = pgGlobalStrCopy(new_char, char_to_insert, 3);
#endif

	UnuseMemory(block->text);
	UnuseMemory(pg->t_blocks);

	pgPrepareStyleWalk(pg, 0, NULL, FALSE);
}


/* pgInsert is the preferred method of inserting data, including keys. The data
can be anything passed in data, and it is inserted at position. If position = -1,
the insertion occurs at the current insertion point.
Change for version 1.1, now returns a pg_boolean, TRUE if display or caret
changed in any way. */

PG_PASCAL (pg_boolean) pgInsert (pg_ref pg, const pg_char_ptr data, size_t length, size_t position,
		short insert_mode, short modifiers, short draw_mode)
{
	paige_rec_ptr				pg_rec;
	short						use_insert_mode;
	pg_boolean					insertion_displayed;
	size_t						insert_position, remaining_length;
	size_t						max_blocksize, insert_length;

	if (!length)
		return	FALSE;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	insertion_displayed = FALSE;
	
	if ((use_insert_mode = insert_mode) == recursive_insert_mode)
		use_insert_mode = key_insert_mode;

#ifdef PG_DEBUG
	if (pg_rec->lock_id)
		pgFailure(pg_rec->globals->mem_globals, LOCKED_PG_ERROR, 1);
#endif

	if (pg_rec->active_subset)	// We are inserting to a subset of self
		insertion_displayed = pgInsertIntoSubRef(pg_rec, data, length, position, insert_mode, modifiers, draw_mode);
	else
	if ((use_insert_mode == key_insert_mode) || (use_insert_mode == key_buffer_mode)) {
		
		if ( (pg_rec->flags & (DEACT_BIT | PERM_DEACT_BIT | NO_EDIT_BIT))
			|| (pg_rec->flags2 & MOUSE_DRAG_STATE) ) {
			
			UnuseMemory(pg);
			return	FALSE;
		}

		if (insert_mode != recursive_insert_mode) {
			
			if (use_insert_mode == key_insert_mode)
					use_insert_mode = pg_rec->procs.insert_query(pg_rec, data, (short)length);
				else
					use_insert_mode = key_buffer_mode;

			if (use_insert_mode == key_buffer_mode)
				if (has_any_controls(pg_rec, data, length))
					use_insert_mode = key_insert_mode;
		}

		if ((use_insert_mode == key_buffer_mode) && draw_mode && (!modifiers)) {
			
			pgBlockMove(data, AppendMemory(pg_rec->key_buffer, length, FALSE), length * sizeof(pg_char));
			UnuseMemory(pg_rec->key_buffer);
			
			if (!pg_rec->key_buffer_mode)
				pg_rec->key_buffer_mode = draw_mode;
			else
			if (insert_mode == key_insert_mode)
				if (pg_rec->procs.insert_query(pg_rec, data, (short)length) == key_insert_mode)
					insertion_displayed = pgInsertPendingKeys(pg);

			UnuseMemory(pg);
			return	insertion_displayed;
		}
		
		insertion_displayed |= pgInsertPendingKeys(pg);

		if (!(modifiers & EXTEND_MOD_BIT))
			pgRemoveAllHilites(pg_rec, draw_mode);

		if (position != CURRENT_POSITION)
			pgSetInsertSelect(pg_rec, position);

		insert_key_data(pg_rec, data, length, modifiers, draw_mode);
		insertion_displayed |= (pg_boolean)(draw_mode != draw_none);
	}
	else {
		long			display_from, display_to;
		short			drawing_verb;

		insertion_displayed |= pgInsertPendingKeys(pg);
		insertion_displayed |= (pg_boolean)(draw_mode != draw_none);
		if (draw_mode && (!(pg_rec->flags & DEACT_BITS)))
			pgTurnOffHighlight(pg_rec, TRUE);

		clear_line_end_flag(pg_rec);
		
		++pg_rec->change_ctr;

		if ((insert_position = position) == CURRENT_POSITION) {
			
			if (pg_rec->num_selects)
				pgDelete(pg, NULL, draw_none);

			insert_position = pgCurrentInsertion(pg_rec);
		}
		else
			insert_position = pgFixOffset(pg_rec, insert_position);

		drawing_verb = draw_mode;
		max_blocksize = pg_rec->globals->max_block_size;
		
		if ((drawing_verb == best_way) && (length > max_blocksize))
			drawing_verb = bits_copy;

		if (drawing_verb == best_way) {
			smart_update		update_info;
			long				old_text_size;
			
			old_text_size = pg_rec->t_length;

			pgInsertRawData(pg_rec, data, length, insert_position, &update_info, insert_mode);
			display_to = update_info.suggest_end;
			display_from = update_info.suggest_begin;
			
			if (update_info.flags & CAN_DRAW_PARTIAL_LINE)
				drawing_verb = direct_or;
			else
				drawing_verb = bits_copy;
		}
		else {
			pg_char_ptr			insert_data;
			
			if ((remaining_length = length) > max_blocksize)
				pgInvalCharLocs(pg_rec);

			insert_data = data;

			while (remaining_length) {
				
				if ((insert_length = remaining_length) > max_blocksize)
					insert_length = max_blocksize;

				pgInsertRawData(pg_rec, insert_data, insert_length, insert_position, NULL, insert_mode);
				
				insert_data += insert_length;
				insert_position += insert_length;
				
				if ((remaining_length -= insert_length) > 0)
					pgFixBadBlocks(pg_rec);
			}

			display_from = 0;
			display_to = DRAW_TO_END;
			pg_rec->last_key_check = MAX_KEY_CHECK + 1;
		}
		
		if (pg_rec->last_key_check > MAX_KEY_CHECK)
			if (!(pg_rec->flags2 & NO_STYLE_CLEANUP))
				if (pgFixBadBlocks(pg_rec))
					display_from = 0;

			if (draw_mode)
				pgUpdateText(pg_rec, &pg_rec->port, display_from, display_to, MEM_NULL, NULL,
							drawing_verb, TRUE);
	}
	
	if (draw_mode)
		extend_visual_pages(pg_rec, draw_mode);

	UnuseMemory(pg);
	
	return	insertion_displayed;
}


/* pgDelete deletes the specified range of text.  */

PG_PASCAL (void) pgDelete (pg_ref pg, const select_pair_ptr delete_range, short draw_mode)
{
	paige_rec_ptr				pg_rec;
	memory_ref					select_list;
	register select_pair_ptr	selections;
	register pg_short_t			num_selects;
	register long				total_to_delete, deleted, multi_del_base;
	t_select_ptr				new_selection;
	size_t						delete_from, delete_to;
	short						use_draw_mode;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	pgInsertPendingKeys(pg);

#ifdef PG_DEBUG
	if (pg_rec->lock_id)
		pgFailure(pg_rec->globals->mem_globals, LOCKED_PG_ERROR, 1);
#endif

	++pg_rec->change_ctr;
	
	if ((use_draw_mode = draw_mode) == best_way)
		use_draw_mode = bits_copy;

	if (select_list = pgCellSelections(pg_rec, delete_range)) {
		
		pgRemoveAllHilites(pg_rec, use_draw_mode);
		SetMemorySize(pg_rec->select, MINIMUM_SELECT_MEMSIZE);
		new_selection = (t_select_ptr) UseMemory(pg_rec->select);
		new_selection->flags |= SELECTION_DIRTY;
		new_selection[1] = new_selection[0];
		pg_rec->stable_caret.h = pg_rec->stable_caret.v = 0;
		pg_rec->num_selects = 0;
		multi_del_base = 0;

		for (selections = (select_pair_ptr) UseMemory(select_list), num_selects = (pg_short_t)GetMemorySize(select_list);
				num_selects;  ++selections, --num_selects) {

			delete_from = selections->begin - multi_del_base;
			delete_to = selections->end - multi_del_base;
			
			if (delete_from < new_selection->offset)
				new_selection->offset = delete_from;

			total_to_delete = delete_to - delete_from;
			multi_del_base += total_to_delete;

			while (total_to_delete) {
				
				pgDeleteRawData(pg_rec, delete_from, &delete_to, NULL);
				deleted = delete_to - delete_from;
				
				selections->end -= deleted;
				total_to_delete -= deleted;
				delete_to = selections->end;
			}
		}
		
		if (!pg_rec->num_selects)
			pg_rec->hilite_anchor = pgCurrentInsertion(pg_rec);

		UnuseMemory(pg_rec->select);
		UnuseAndDispose(select_list);

		if (use_draw_mode)
			pgUpdateText(pg_rec, &pg_rec->port, 0, pg_rec->t_length, MEM_NULL,
					NULL, use_draw_mode, TRUE);
	}

	UnuseMemory(pg);
}


/* This function dumps any pending chars in key_buffer (saved from inserts) */

PG_PASCAL (pg_boolean) pgInsertPendingKeys (pg_ref pg)
{
	paige_rec_ptr	pg_rec;
	short			use_draw_mode;
	pg_boolean		result = FALSE;
	long			insert_size;
	
	pg_rec = (paige_rec_ptr) UseMemory(pg);

	if ((use_draw_mode = pg_rec->key_buffer_mode) != 0) {
		
		pg_rec->key_buffer_mode = draw_none;	/* Avoids infinite recursions */
		insert_size = GetMemorySize(pg_rec->key_buffer);
		
		result = pgInsert(pg, (const pg_char_ptr) UseMemory(pg_rec->key_buffer), insert_size, CURRENT_POSITION,
				recursive_insert_mode, 0, use_draw_mode);

		UnuseMemory(pg_rec->key_buffer);
		SetMemorySize(pg_rec->key_buffer, 0);
	}

	UnuseMemory(pg);
	
	return	result;
}


/* This is the main entry point to display a pg_ref. */

PG_PASCAL (void) pgDisplay (pg_ref pg, const graf_device_ptr target_device, shape_ref vis_target,
		shape_ref wrap_target, const co_ordinate_ptr offset_extra, short draw_mode)
{
	select_pair		range;
	
	range.begin = 0;
	range.end = DRAW_TO_END;
	
	pgDoDisplay(pg, &range, target_device, vis_target, wrap_target, offset_extra, draw_mode);
}


/* pgDisplayLineRange is identical to pgDisplay except only the text between the two
line(s) is drawn. */

PG_PASCAL (void) pgDisplayLineRange (pg_ref pg, long line_from, long line_to,
		const graf_device_ptr target_device, shape_ref vis_target,
		shape_ref wrap_target, const co_ordinate_ptr offset_extra, short draw_mode)
{
	select_pair		range;

	pgLineNumToOffset(pg, line_from, &range.begin, NULL);
	pgLineNumToOffset(pg, line_to, NULL, &range.end);
	pgDoDisplay(pg, &range, target_device, vis_target, wrap_target, offset_extra, draw_mode);
}


/* pgDoDisplay is a lower-level display function. */

PG_PASCAL (void) pgDoDisplay (pg_ref pg, select_pair_ptr range, const graf_device_ptr target_device,
		shape_ref vis_target, shape_ref wrap_target, const co_ordinate_ptr offset_extra,
		short draw_mode)
{
	paige_rec_ptr			pg_rec;
	graf_device				old_device;
	shape_ref				old_wrap, old_vis;
	memory_ref				copy_of_subs;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	copy_of_subs = pgGetSubrefState(pg_rec, FALSE, TRUE);

	if (wrap_target || vis_target || target_device) {
	
		if (wrap_target) {
			old_wrap = pg_rec->wrap_area;
			pg_rec->wrap_area = wrap_target;

			pgInvalSelect(pg_rec->myself, 0, pg_rec->t_length);
			pg_rec->port.clip_info.change_flags |= CLIP_DEVICE_CHANGED;
		}

		if (vis_target) {
			
			old_vis = pg_rec->vis_area;
			pg_rec->vis_area = vis_target;
		}
		
		if (target_device) {
			
			pgBlockMove(&pg_rec->port, &old_device, sizeof(graf_device));
			pgBlockMove(target_device, &pg_rec->port, sizeof(graf_device));
			pg_rec->port.clip_info.change_flags |= CLIP_DEVICE_CHANGED;
		}
	}
	
	update_text(pg_rec, NULL, range->begin, range->end, MEM_NULL, offset_extra,
				draw_mode, (short)(copy_of_subs == MEM_NULL));

	pgDrawPages(pg_rec, NULL, offset_extra, MEM_NULL, draw_mode);

	if (wrap_target || vis_target || target_device) {
	
		if (wrap_target) {
			pg_rec->wrap_area = old_wrap;
			pgInvalSelect(pg_rec->myself, 0, pg_rec->t_length);
			pg_rec->port.clip_info.change_flags |= CLIP_PAGE_CHANGED;
		}
		
		if (vis_target) {
		
			pg_rec->vis_area = old_vis;
			pg_rec->port.clip_info.change_flags |= CLIP_VIS_CHANGED;
		}

		if (target_device) {
			
			pg_rec->port.clip_info.change_flags |= CLIP_DEVICE_CHANGED;
			pgBlockMove(&old_device, &pg_rec->port, sizeof(graf_device));
			pg_rec->port.clip_info.change_flags |= CLIP_DEVICE_CHANGED;
		}
	}

	pgRestoreSubRefs(pg_rec, copy_of_subs);
	
	if (copy_of_subs) {

		if ((!(pg_rec->flags & DEACT_BITS)) && (!(pg_rec->flags & PRINT_MODE_BIT))) {

			pgSetupGrafDevice(pg_rec, &pg_rec->port, MEM_NULL, clip_standard_verb);
		    set_caret(pg_rec, restore_cursor);
			pgDrawHighlight(pg_rec, update_cursor);
			pg_rec->procs.set_device(pg_rec, unset_pg_device, &pg_rec->port, NULL);
	    }
	}

	UnuseMemory(pg);
}

/* pgUpdateText is called internally from various places in Paige. Its purpose
is to re-draw text from a specified offset -- but only when and if it falls in
the vis_rgn shape parameters. The draw_port can be NULL, in which case the default
port is used. Offset_extra can also be NULL, in which case no extra offset other
than the current scroll position is used. Finally, vis_rgn can be NULL, in which
case the current vis_area of pg is used.
Text updates at starting offset through ending offset (or if ending offset = -1,
text is updated to end of pg).  Note, that if the beginning offset does not
land on a point_start boundary, it is assumed to be a partial line display (such
as a single char), but only if draw_mode == best_way.

The function returns the first text offset it could not draw. This is mainly
used for printing (so print function gets the first offset that won't appear
on the page).  */

PG_PASCAL (long) pgUpdateText (paige_rec_ptr pg, graf_device_ptr draw_port,
		size_t beginning_offset, size_t ending_offset, shape_ref vis_rgn,
		co_ordinate_ptr offset_extra, short display_verb, short hilite_mode)
{
	long		result;
	
	result = update_text(pg, draw_port, beginning_offset, ending_offset, vis_rgn, offset_extra, display_verb, hilite_mode);
	
	if (pg->flags2 & PAR_EXCLUSION_CHANGE)
		pgDrawPages(pg, draw_port, offset_extra, vis_rgn, display_verb);
	
	return	result;
}


/* pgDisplaySubset cranks through a paige_rec that is known to be a "subset" of itself,
typically fairly small in size. We use this for embedded subrefs. */

PG_PASCAL (void) pgDisplaySubset (paige_rec_ptr pg, rectangle_ptr draw_bounds,
			co_ordinate_ptr vis_offset)
{
	register point_start_ptr		starts;
	point_start_ptr					initial_starts;
	text_block_ptr					block;
	rectangle_ptr					wrap_base;
	pg_char_ptr						text;
	style_walk						walker;
	rectangle						vis_bounds, union_of_starts;
	glitter_info					glitter;
	long							num_blocks, abs_begin_offset;

	if (!pg->active_subset)
		return;

	num_blocks = GetMemorySize(pg->t_blocks);
	wrap_base = (rectangle_ptr) UseMemory(pg->wrap_area);
	++wrap_base;

	if (!pg->num_selects)
		if (!(pg->flags & PRINT_MODE_BIT))
			pgTurnOffHighlight(pg, FALSE);

	block = pgFindTextBlock(pg, 0, NULL, TRUE, FALSE);
	pgPaginateBlock(pg, block, NULL, TRUE);
	
	vis_bounds = *draw_bounds;
	pgOffsetRect(&vis_bounds, vis_offset->h, vis_offset->v);

	while (num_blocks) {

		if (block->flags & BELOW_CONTAINERS)
			break;
		
		if (!(block->flags & ALL_TEXT_HIDDEN)) {

			starts = initial_starts = (point_start_ptr) UseMemory(block->lines);
			pgFillBlock(&union_of_starts, sizeof(rectangle), 0);
			
			while (starts->flags != TERMINATOR_BITS) {
				
				pgUnionRect(&starts->bounds, &union_of_starts, &union_of_starts);
				++starts;
			}
			
			starts = initial_starts;

			pgFillBlock(&glitter, sizeof(glitter_info), 0);
			
			glitter.par_number = block->first_par_num;
			glitter.line_number = block->first_line_num;
			
			abs_begin_offset = (long)starts->offset;
			abs_begin_offset += block->begin;
			pgPrepareStyleWalk(pg, abs_begin_offset, &walker, TRUE);
			glitter.walker = walker;
			
			pg->procs.load_proc(pg, block);
			text = (pg_char_ptr) UseMemory(block->text);
			
			while (starts->flags != TERMINATOR_BITS)
				starts += draw_line(pg, block, text, starts, &walker,
						vis_offset, &vis_bounds, wrap_base, &glitter,
						NULL, NULL, direct_or);

			pgPrepareStyleWalk(pg, 0, NULL, TRUE);
			UnuseMemory(block->lines);
			UnuseMemory(block->text);
		}
		
		++block;
		if ((num_blocks -= 1) != 0)
			pgPaginateBlock(pg, block, NULL, TRUE);
	}

	UnuseMemory(pg->t_blocks);
	UnuseMemory(pg->wrap_area);

    pg->flags &= (~INVALID_CURSOR_BIT);
	draw_bounds->top_left.v = union_of_starts.top_left.v;
	draw_bounds->bot_right.v = union_of_starts.bot_right.v;
}


/* pgInvalidateLine invalidates the current line assuming length byte have been inserted.
This is commonly used to draw a whole line after a subset ref has changed. */

PG_PASCAL (void) pgInvalidateLine (paige_rec_ptr pg, long offset, short draw_mode)
{
	pg_char			dummy_char[2];
	smart_update	update_info;

	dummy_char[0] = dummy_char[1] = (pg_char)pg->globals->null_char;
	
	if (draw_mode == draw_none)
		pgInsertRawData(pg, dummy_char, 2, offset, NULL, key_insert_mode);
	else {

		pgInsertRawData(pg, dummy_char, 2, offset, &update_info, key_insert_mode);
		pgUpdateText(pg, &pg->port, update_info.suggest_begin, update_info.suggest_end, MEM_NULL, NULL,
			bits_copy, FALSE);
	}
}



/* pgSetupGrafDevice sets a wanted graphic device and clips it with an optional region.
If device is NULL, it is substituted with the default. The function
returns the drawing port used. The clip_verb is passed to pgClipGrafDevice. */

PG_PASCAL (graf_device_ptr) pgSetupGrafDevice (paige_rec_ptr pg, graf_device_ptr device,
		shape_ref intersect_shape, short clip_verb)
{
	graf_device_ptr			drawing_port;
	
	if (!(drawing_port = device))
		drawing_port = &pg->port;

	pg->procs.set_device(pg, set_pg_device, drawing_port, &pg->bk_color);
	pgClipGrafDevice(pg, clip_verb, intersect_shape);

	return	drawing_port;
}


/* pgCurrentInsertion returns the current insertion point of pg */

PG_PASCAL (long) pgCurrentInsertion (paige_rec_ptr pg)
{
	t_select			first_loc;
	
	GetMemoryRecord(pg->select, 0, &first_loc);
	
	return	first_loc.offset;
}


/* This function is called from various places. Raw data is inserted at the
specified offset. The first offset that is effected in the display is returned
(which you could safely draw only from that spot).
The update_info parameter indicates whether or not to figure out the shortest
(smallest) screen update path. The reason we even have such a parameter is extra
code is required to produce a minimum setting, hence if large insertions occur
it might be best to just invalidate whole text blocks, whereas smaller insertions
such as keys should determine the minimum update area for user-view speed. If
update_info is NULL, the code figures out an approximate starting spot, otherwise
it figures it out more accurately.

The function will perform the following:

(1) Locate the appropriate text_block and insert the data into the text stream.
(2) Flag the block as requiring "recalc."
(3) Splits the block if required.
(4) Advances all style and paragraph runs.
(5) Increments total text size in pg.
(6) Advances insertion point if appropriate.

The function does NOT do the following:

(1) Calculate lines or alter the point_starts (unless update_info != NULL).
(2) Insert styles or alter style_info/par_info/font_info records in any way.
(3) Display.
(4) Check for "control codes" or the like -- raw data is blindly inserted.
(5) Insert more than 64K.

Added 7/14/95, if key_insert_mode and a single null char is offered, the current line
is invalidated, recomputed but nothing is inserted. */

PG_PASCAL (long) pgInsertRawData (paige_rec_ptr pg, pg_char_ptr data, size_t length,
			size_t offset, smart_update_ptr update_info, short insert_mode)
{
	register text_block_ptr		block;
	style_info					info, mask;
	style_run_ptr				run;
	select_pair					affected_select;
	pg_short_t					block_num, style_item;
	pg_boolean					advance_equal_pars, style_changed, use_insert_style;
	pg_char_ptr					target_ptr;
	long						first_display, insert_position, use_length;
	long						original_insertion, affected_position, end_position;

	style_changed = FALSE;
	use_length = 0;
	
	use_insert_style = (pg_boolean)(offset == pgCurrentInsertion(pg));

	original_insertion = pgCurrentInsertion(pg);
	end_position = offset + length;

	pg->last_key_check += length;

	block = pgFindTextBlock(pg, offset, &block_num, FALSE, TRUE);
	block->cache_flags |= CACHE_CHANGED_FLAG;
	insert_position = (offset - block->begin);

	if ((insert_mode == data_insert_mode) || (*data != pg->globals->null_char)) {
		use_length = length;

		if (offset == pg->t_length)
			advance_equal_pars = ((pg->flags & DOC_TERMINATED_BIT) == 0);
		else
			advance_equal_pars = FALSE;

		target_ptr = (pg_char_ptr) InsertMemory(block->text, insert_position, use_length);
		
		if (use_length == 1)
			*target_ptr = *data;
		else
			pgBlockMove(data, target_ptr, use_length * sizeof(pg_char));
	
		UnuseMemory(block->text);
	
		pg->t_length += use_length;

		advance_text_runs(pg, block, block_num, offset, length,
				(pg_boolean)(offset < pg->t_length), advance_equal_pars, FALSE);
		if (affected_position = offset)
			--affected_position;
	
		if (use_insert_style && pg->insert_style != NULL_RUN) {
		
			run = pgFindStyleRun(pg, affected_position, NULL);
			style_item = run->style_item;
			UnuseMemory(pg->t_style_run);

			if (pg->insert_style != style_item) {
				
				affected_select.begin = offset;
				affected_select.end = end_position;
				pgFillBlock(&mask, sizeof(style_info), -1);
				GetMemoryRecord(pg->t_formats, pg->insert_style, &info);
	
				pgChangeStyleInfo(pg, &affected_select, &info, &mask, draw_none);
				style_changed = TRUE;
			}
		}

		pgPartialTextMeasure(pg, block, (pg_short_t)insert_position, (pg_short_t)use_length);
		advance_line_run(pg, block, (pg_short_t)insert_position, use_length);
	}

	first_display = pgInvalTextBlock(pg, block, offset, length);

	if (update_info) {
		
		pgPaginateBlock(pg, block, update_info, TRUE);
		fix_non_linefeed_update(pg, block, update_info);
		first_display = update_info->suggest_begin;
		set_fastest_drawing_path(pg, block, offset, length, update_info);
	}

	UnuseMemory(pg->t_blocks);
	
	if (end_position == pg->t_length)
		pgSetTerminatorFlag(pg);
	
	if (style_changed)
		pgSetNextInsertIndex(pg);

	return	first_display;
}


/* pgDrawPages gets called from a couple of places. This function first sets up
a real "page" (which is the wrap_area shape offset by the repeater value
appropriate for the first visual page) then calls page_draw_proc n times until
the repeating page is no longer visible. Visual area is determined by
vis_rgn parameter. The draw_mode_used param indicates what the actual drawing
mode was.
Modified 5/9/94, draw_mode_used can be negative which implies internal PAIGE
actions. For example, we use -1 for "erase pages" action, which doesn't really
call the app's hooks. */

PG_PASCAL (void) pgDrawPages (paige_rec_ptr pg, graf_device_ptr device,
		co_ordinate_ptr offset_extra, shape_ref vis_rgn, short draw_mode_used)
{
	graf_device		old_device;
	shape_ref		vis_area, old_vis;
	rectangle_ptr	page_ptr;
	rectangle		vis_bounds, page_bounds;
	co_ordinate		vis_offset, visible_top, old_scroll;
	long			repeating, page_height, page_width;
	long			max_bottom;
	short			call_order, used_mode;
	pg_short_t		r_qty, page_num;

	if (!draw_mode_used)
		return;

	old_vis = pg->vis_area;
	old_scroll = pg->scroll_pos;
	
	if (device) {
		
		pgBlockMove(&pg->port, &old_device, sizeof(graf_device));
		pgBlockMove(device, &pg->port, sizeof(graf_device));
	}

	if (!(vis_area = vis_rgn))
		vis_area = pg->vis_area;

	vis_offset = pg->scroll_pos;
	pgNegatePt(&vis_offset);
	pgAddPt(&pg->port.origin, &vis_offset);

	if (offset_extra)
		pgAddPt(offset_extra, &vis_offset);

	pg->scroll_pos = vis_offset;
	pgNegatePt(&pg->scroll_pos);
	pg->vis_area = vis_area;		/* <-- needs for hook */

	page_ptr = (rectangle_ptr) UseMemory(pg->wrap_area);
	r_qty = (pg_short_t)(GetMemorySize(pg->wrap_area) - 1);
	pgShapeBounds(vis_area, &vis_bounds);
	pgBlockMove(page_ptr, &page_bounds, sizeof(rectangle));

	++page_ptr;

	page_height = page_width = 0;
	page_num = call_order = 0;

	pgSetupGrafDevice(pg, NULL, vis_area, clip_with_none_verb);

	pg->doc_info.num_pages = get_num_pages(pg);

	if ((used_mode = draw_mode_used) == best_way)
		used_mode = bits_copy;

	if (repeating = pg->doc_info.attributes & (V_REPEAT_BIT | H_REPEAT_BIT)) {
		co_ordinate		repeat_offset;

		pgScaleLong(-pg->port.scale.scale, pg->port.scale.origin.v,
				&vis_bounds.bot_right.v);
		pgScaleLong(-pg->port.scale.scale, pg->port.scale.origin.h,
				&vis_bounds.bot_right.h);
 		pgScaleLong(-pg->port.scale.scale, pg->port.scale.origin.v,
				&vis_bounds.top_left.v);
		pgScaleLong(-pg->port.scale.scale, pg->port.scale.origin.h,
				&vis_bounds.top_left.h);

		visible_top = vis_bounds.top_left;
		pgSubPt(&vis_offset, &visible_top);
		
		page_num = pgPixelToPage(pg->myself, &visible_top, &repeat_offset,
				&page_width, &page_height, &max_bottom, FALSE);
		pgAddPt(&repeat_offset, &vis_offset);
		max_bottom += vis_offset.v;

		if (max_bottom > vis_bounds.bot_right.v)
			max_bottom = vis_bounds.bot_right.v;

		pgOffsetRect(&page_bounds, vis_offset.h, vis_offset.v);

		for (;;) {

			page_bounds.top_left.v += page_height;
			page_bounds.top_left.h += page_width;

			if ((page_bounds.top_left.v > max_bottom) || (page_bounds.top_left.h > vis_bounds.bot_right.h))
				call_order = -1;
			
			if (used_mode == sp_erase_all_pages)
				fill_pages(pg, page_ptr, r_qty, page_num, &vis_offset);
			else
				pg->procs.page_proc(pg, page_ptr, r_qty, page_num, &vis_offset, used_mode, call_order);

			if (call_order == -1)
				break;
			
			++call_order;
			++page_num;
			vis_offset.h += page_width;
			vis_offset.v += page_height;
		}
	}
	else  {
	    
		pgOffsetRect(&page_bounds, vis_offset.h, vis_offset.v);

	    if (used_mode == sp_erase_all_pages)
			fill_pages(pg, page_ptr, r_qty, page_num, &vis_offset);
		else
			pg->procs.page_proc(pg, page_ptr, r_qty, page_num, &vis_offset, used_mode, -1);
    }

	pg->procs.set_device(pg, unset_pg_device, &pg->port, NULL);

	pg->vis_area = old_vis;
	pg->scroll_pos = old_scroll;
	
	if (device)
		pgBlockMove(&old_device, &pg->port, sizeof(graf_device));

	UnuseMemory(pg->wrap_area);
	
	pg->flags2 &= (~PAR_EXCLUSION_CHANGE);
}


/* This function returns the text length of text + offset if all blank chars
are stripped from the trailing end. Note that the style won't change during
the whole text range because it is getting called during the draw function,
and all text pieces are known to be one style.
Modified 2/24/94 to omit stripping of spaces unless the portion of the line
is the last point_start.
Modified 7/13/94 to force stripping of spaces if j_extra != 0 and mid-line. */

PG_PASCAL (pg_short_t) pgStripTrailingBlanks (paige_rec_ptr pg, point_start_ptr related_start,
		style_walk_ptr styles, pg_char_ptr text, pg_short_t text_offset,
		pg_short_t text_length, long j_extra)
{
	register style_walk_ptr		walker;
	register pg_short_t			result, offset;
	long						offset_begin, offset_end;
	pg_boolean					strip_midline;
	
	if (result = text_length) {
		
		offset_end = (long)text_length;

		walker = styles;
		
		if (walker->cur_style->char_bytes)
			return	result;
		
		strip_midline = ((j_extra != 0) && (related_start->flags & SOFT_BREAK_BIT));
							
		offset = text_offset + text_length - 1;
		offset_begin = (long)related_start->offset;
				
		if ((related_start->flags & LINE_BREAK_BIT) || strip_midline) {
			long			c_info;
			
			for (;;) {
			
				c_info = walker->cur_style->procs.char_info(pg, walker, text, NO_BLOCK_OFFSET,
						offset_begin, offset_end, offset, BLANK_BIT | LAST_HALF_BIT);
				
				if (!(c_info & BLANK_BIT))
					break;
				
				if (c_info & LAST_HALF_BIT) {
					
					result -= 2;
					offset -= 2;
				}
				else {

					result -= 1;
					offset -= 1;
				}
				
				if (result < 1)
					break;
			}
		}
		else
			if (walker->cur_style->procs.char_info(pg, walker, text, NO_BLOCK_OFFSET,
					offset_begin, offset_end, offset, CTL_BIT))
				result -= 1;
	}

	return	result;
}


/* This function sets DOC_TERMINATED_BIT (or clears it) depending on the ending
character. */

PG_PASCAL (void) pgSetTerminatorFlag (paige_rec_ptr pg)
{
	register text_block_ptr		block;
	style_walk					walker;
	long						wanted_char;

	pg->flags &= CLR_DOC_TERMINATED;
	
	if (pg->t_length) {
		
		wanted_char = pg->t_length - 1;

		block = (text_block_ptr) UseMemoryRecord(pg->t_blocks, GetMemorySize(pg->t_blocks) - 1, 0, TRUE);
		pgPrepareStyleWalk(pg, wanted_char, &walker, FALSE);
		pg->procs.load_proc(pg, block);

		if (walker.cur_style->procs.char_info(pg, &walker, (pg_char_ptr) UseMemory(block->text),
				block->begin, 0, block->end - block->begin, wanted_char - block->begin, PAR_SEL_BIT))
			pg->flags |= DOC_TERMINATED_BIT;
		
		UnuseMemory(block->text);
		UnuseMemory(pg->t_blocks);
		pgPrepareStyleWalk(pg, 0, NULL, FALSE);
	}
}


/* This deletes data bytes from delete_from to *delete_to. It does not update the
text nor does it rebuild anything. However, it returns the suggested offset as
to where to update (draw) the new text if desired.

The delete_to param is a pointer because the function sets the offset to whatever
amount it deleted. This MIGHT be different than the suggested amount because
pgDeleteRawData won't remove more than one text_block. Hence, a partial deletion
will result in *delete_to != what was passed.

The update_info parameter indicates whether or not to figure out the shortest
(smallest) screen update path. The reason we even have such a parameter is extra
code is required to produce a minimum setting, hence if large insertions occur
it might be best to just invalidate whole text blocks, whereas smaller insertions
such as keys should determine the minimum update area for user-view speed. If
update_info is NULL, the code figures out an approximate starting spot, otherwise
it figures it out more accurately.			*/

PG_PASCAL (long) pgDeleteRawData (paige_rec_ptr pg, size_t delete_from, size_t PG_FAR *delete_to,
		smart_update_ptr update_info)
{
	register text_block_ptr			block;
	style_run_ptr					nuked_run;
	pg_short_t						block_num;
	pg_short_t						style_item, par_item;
	size_t							nuked_rec_begin, local_offset, local_length;
	size_t							nuked_styles, offset_to, first_display;
	pg_boolean						whole_par_special_case;				// PAJ
	size_t							par_begin, par_end, par_dummy;		// PAJ

	block = pgFindTextBlock(pg, delete_from, &block_num, FALSE, TRUE);
	block->cache_flags |= CACHE_CHANGED_FLAG;

	if ((offset_to = *delete_to) > block->end)
		offset_to = *delete_to = block->end;

	// PAJ fix
	// find paragraph boundaries around the begin and end of the deletion range
	pgFindPar(pg->myself, delete_from, &par_begin, &par_dummy);
	par_end = offset_to;
	if (offset_to)
		if (!pgCharType(pg->myself, offset_to-1, PAR_SEL_BIT))
			pgFindPar(pg->myself, offset_to, &par_dummy, &par_end);
	whole_par_special_case = ((delete_from == par_begin) && (offset_to == par_end));
	// end PAJ fix

	local_length = offset_to - delete_from;
	first_display = pgInvalTextBlock(pg, block, offset_to, -local_length);

	pgCallTextHook(pg, NULL, delete_text_reason, delete_from, offset_to - delete_from, call_for_delete, 0, 0, 0);
	pgGetPadStyles(pg, offset_to, &style_item, &par_item, TRUE);
	pgGetPadStyles(pg, delete_from, &pg->insert_style, NULL, FALSE);
	pg->flags |= INVALID_CURSOR_BIT;

	if (GetMemorySize(pg->t_style_run) > 2) {

		nuked_run = pgFindStyleRun(pg, delete_from, &nuked_rec_begin);
		
		if ( (nuked_run->offset < delete_from) || (nuked_run->offset == 0) ) {
			
			++nuked_rec_begin;
			++nuked_run;
		}
		
		for (nuked_styles = 0; offset_to >= nuked_run->offset; ++nuked_run)
			if ((delete_from <= nuked_run->offset) && (offset_to > nuked_run->offset))
				++nuked_styles;
		
		UnuseMemory(pg->t_style_run);
		
		if (nuked_styles)
			delete_style_records(pg, nuked_rec_begin, nuked_styles);
	}

	if (GetMemorySize(pg->par_style_run) > 2) {

		nuked_run = pgFindParStyleRun(pg, delete_from, &nuked_rec_begin);

		if ( (nuked_run->offset < delete_from) ) {
			
			++nuked_rec_begin;
			++nuked_run;
		}

		// PAJ fix
		if (whole_par_special_case)
		{
			for (nuked_styles = 0; offset_to >= nuked_run->offset; ++nuked_run)
				if ((delete_from <= nuked_run->offset) && (offset_to >= nuked_run[1].offset))
					++nuked_styles;
		}
		// end PAJ fix
		else
		{
			for (nuked_styles = 0; offset_to >= nuked_run->offset; ++nuked_run)
				if ((delete_from < nuked_run->offset) && (offset_to >= nuked_run->offset))
					++nuked_styles;
		}
		
		UnuseMemory(pg->par_style_run);
		
		if (nuked_styles)
			delete_par_records(pg, nuked_rec_begin, nuked_styles);
	}

	delete_par_exclusions(pg, delete_from, local_length);
	
#ifdef PG_DEBUG
	pgCheckRunIntegrity(pg);
#endif

	local_offset = delete_from - block->begin;

	pgDeleteSubRefs(block, local_offset, local_length);	
	DeleteMemory(block->text, local_offset, local_length);
	pg->t_length -= local_length;

	if (block_should_delete(pg, block, (pg_short_t)block_num, (pg_short_t)local_offset, (pg_short_t)local_length)) {

		advance_text_runs(pg, block, block_num, delete_from, -local_length, FALSE, FALSE, TRUE);
		pgSetPadStyles(pg, style_item, par_item, delete_from);
		pgCleanupStyleRuns(pg, TRUE, FALSE);
		
		fix_deleted_pars(pg, delete_from, offset_to);
		
		if (delete_from == pg->t_length)
			pgSetTerminatorFlag(pg);
		
		pgCleanupStyleRuns(pg, FALSE, TRUE);

		block->flags &= (~SOME_LINES_GOOD);
		block->flags |= (NEEDS_CALC | NEEDS_PARNUMS);
		first_display = block->begin;

		UnuseMemory(pg->t_blocks);
		pgFixBadBlocks(pg);

		if (update_info) {

			pgFillBlock(update_info, sizeof(smart_update), 0);
			update_info->suggest_end = pg->t_length;
			update_info->num_display_lines = UNKNOWN_LINE_QTY;
		}

#ifdef PG_DEBUG
		pgCheckRunIntegrity(pg);
#endif

	}
	else {
		pgDeleteTextMeasure(pg, block, (pg_short_t)local_offset, (pg_short_t)local_length);
		advance_text_runs(pg, block, block_num, delete_from, -local_length, FALSE, FALSE, FALSE);
		pgSetPadStyles(pg, style_item, par_item, delete_from);
		pgCleanupStyleRuns(pg, TRUE, FALSE);
		
		fix_deleted_pars(pg, delete_from, offset_to);

		if (delete_from == pg->t_length)
			pgSetTerminatorFlag(pg);

		pgCleanupStyleRuns(pg, FALSE, TRUE);

		advance_line_run(pg, block, (pg_short_t)local_offset, -local_length);

		if (update_info) {

			pgPaginateBlock(pg, block, update_info, TRUE);
			fix_non_linefeed_update(pg, block, update_info);
			first_display = update_info->suggest_begin;
		}

		UnuseMemory(pg->t_blocks);
	}

	pgSetupInsertStyle(pg);

	return	first_display;
}


/********************************** Local Functions ***********************/


/* update_text is a subset of pgUpdateText().  See that function for comments. */

static long update_text (paige_rec_ptr pg, graf_device_ptr draw_port,
		long beginning_offset, long ending_offset, shape_ref vis_rgn,
		co_ordinate_ptr offset_extra, short display_verb, short hilite_mode)
{
	shape_ref					vis_area;
	text_block_ptr				block;
	graf_device					original_device;
	pg_short_t					num_blocks, cur_block;
	pg_short_t					short_offset, wrap_qty;
	register point_start_ptr	starts;
	point_start_ptr				line_starts;
	pg_char_ptr					text;
	style_walk					walker;
	rectangle_ptr				wrap_base;
	rectangle					vis_bounds, vis_rect, check_bounds;
	co_ordinate					vis_offset, bits_emulate_offset;
	glitter_info				glitter;
	graphic_line_info			last_drawn_line, cur_drawn_line;
	long						last_offset, local_end_offset, last_container;
	long						starting_offset, last_printed, lines_printed;
	long						container_proc_refcon, abs_begin_offset, old_caret_flags;
	short						draw_mode, display_mode_verb, dont_erase, skip_block;
	short						emulating_bits;
	pg_boolean					drew_something = FALSE;

	if (pg->active_subset) {
		
		if (display_verb)
			return	(pgInsertIntoSubRef (pg, NULL, 0, beginning_offset, key_insert_mode, 0, display_verb));

		return	FALSE;		
	}

	original_device = pg->port;

	draw_mode = display_verb;
	pg->flags &= (~TEXT_DIRTY_BIT);

	if (draw_port)
		pg->port = *draw_port;

	if (pg->flags2 & CHECK_PAGE_OVERFLOW)
		pgPaginateNow(pg->myself, pg->t_length, FALSE);

	emulating_bits = (draw_mode >= bits_emulate_copy && draw_mode <= bits_emulate_xor);

	if (emulating_bits || draw_mode == bits_copy || draw_mode == bits_or || draw_mode == bits_xor)
		pg->flags |= BITMAP_ERASE_BIT;

	pgCheckDirectionChange(pg);

	if (!(vis_area = vis_rgn))
		vis_area = pg->vis_area;

	vis_offset = pg->scroll_pos;
	pgNegatePt(&vis_offset);
	pgAddPt(&pg->port.origin, &vis_offset);

	bits_emulate_offset.h = bits_emulate_offset.v = 0;

	if (offset_extra) {
		
		pgAddPt(offset_extra, &vis_offset);
		bits_emulate_offset = *offset_extra;
		pgSubPt(offset_extra, &pg->scroll_pos);
	}
	
	last_printed = pg->t_length;
	lines_printed = 0;
	last_container = -1;
	container_proc_refcon = 0;

	pgSetupGrafDevice(pg, NULL, vis_rgn, clip_standard_verb);
	GetMemoryRecord(vis_area, 0, &vis_rect);
	
	if (pg->flags & SCALE_VIS_BIT)
		pgScaleRectToRect(&pg->port.scale, &vis_rect, &vis_bounds, &pg->port.origin);
	else
		vis_bounds = vis_rect;

	dont_erase = ((draw_mode == direct_or) || (draw_mode == direct_xor)
		|| (draw_mode == bits_or) || (draw_mode == bits_xor)
		|| ((pg->flags & PRINT_MODE_BIT) != 0));

	if (emulating_bits) {

		switch (draw_mode) {
	
			case bits_emulate_copy:
				draw_mode = direct_copy;
				break;
				
			case bits_emulate_or:
				draw_mode = direct_or;
				break;
				
			case bits_emulate_xor:
				draw_mode = direct_xor;
				break;
		}
	}

	last_drawn_line.r_num = -1;
	last_drawn_line.cell_num = 0;

	wrap_base = (rectangle_ptr) UseMemory(pg->wrap_area);
	++wrap_base;
	wrap_qty = (pg_short_t)(GetMemorySize(pg->wrap_area) - 1);

	num_blocks = (pg_short_t)GetMemorySize(pg->t_blocks);
	if ((last_offset = ending_offset) == DRAW_TO_END)
		last_offset = pg->t_length;
	
	starting_offset = beginning_offset;
	old_caret_flags = 0;

	if ((draw_mode == direct_copy) || (draw_mode == best_way) || draw_mode == bits_copy)
		if (!pg->num_selects)
			if (!(pg->flags & PRINT_MODE_BIT)) {

				old_caret_flags = (pg->flags & CARET_BIT);
				pgTurnOffHighlight(pg, FALSE);
	}

	block = pgFindTextBlock(pg, starting_offset, &cur_block, TRUE, FALSE);

	if (pg->t_length) {
	
		erase_top_or_bottom(pg, dont_erase, FALSE);
		
		if (!dont_erase)
			if (pg->doc_info.attributes & USE_MARGINS_BIT)
				erase_top_margin(pg);
	}
	else {

		if (!dont_erase)
			if ((pg->flags & BITMAP_ERASE_BIT) || emulating_bits)
				pgEraseContainerArea(pg, &pg->scale_factor, &vis_offset, NULL, NULL);
	}

	if (emulating_bits)
		pg->procs.bitmap_proc(pg, &pg->port, FALSE, &vis_bounds, &bits_emulate_offset, beginning_offset);

	while (cur_block < num_blocks) {
		
		pgPaginateBlock(pg, block, NULL, FALSE);

		if (block->flags & BELOW_CONTAINERS)
			break;
		
		skip_block = FALSE;

		if (block->flags & ALL_TEXT_HIDDEN)
			skip_block = TRUE;
		else {
						
			pgScaleRectToRect(&pg->port.scale, &block->bounds, &check_bounds, &vis_offset);
#ifdef CHANDHOK
			// chandhok - fixes for 1.273 version of Paige
			pgOffsetRect(&check_bounds, -vis_offset.h, -vis_offset.v);
#endif
			if (check_bounds.bot_right.h <= check_bounds.top_left.h)
				check_bounds.bot_right.h = check_bounds.top_left.h + 1;

			if (!pgSectRect(&check_bounds, &vis_bounds, NULL)) {
				
				skip_block = TRUE;
				
				if ((check_bounds.bot_right.v > vis_bounds.bot_right.v)
					|| (check_bounds.top_left.h > vis_bounds.bot_right.h))
					if (can_quit_display(pg, &block->end_start, &vis_bounds, &vis_offset,
						wrap_base, wrap_qty)) {

					if ((block->begin < last_printed) && lines_printed)
						last_printed = block->begin;

					break;
				}
			}
		}

		if (!skip_block) {

			pgPaginateBlock(pg, block, NULL, TRUE);
			
			if (pg->port.clip_info.change_flags & CLIP_EXCLUDE_CHANGED)
				pgClipGrafDevice(pg, clip_standard_verb, MEM_NULL);

			starts = line_starts = (point_start_ptr) UseMemory(block->lines);
			
			pgFillBlock(&glitter, sizeof(glitter_info), 0);
			
			glitter.par_number = block->first_par_num;
			glitter.line_number = block->first_line_num;

			drew_something = TRUE;
			
			if (starting_offset > block->begin) {  /* Need to find first line */
				
				short_offset = (pg_short_t)(starting_offset - block->begin);

				while ((starts[1].offset <= short_offset)
					&& (starts[1].flags != TERMINATOR_BITS)) {
				
					++starts;

					if (starts->flags & NEW_LINE_BIT)
						++glitter.line_number;
					
					if (starts->flags & NEW_PAR_BIT)
						++glitter.par_number;
				}
			}

			abs_begin_offset = starts->offset;
			abs_begin_offset += block->begin;
			pgPrepareStyleWalk(pg, abs_begin_offset, &walker, TRUE);
			
			pgBlockMove(&walker, &glitter.walker, sizeof(style_walk));
			
			local_end_offset = last_offset - block->begin;
			pg->procs.load_proc(pg, block);
			text = (pg_char_ptr) UseMemory(block->text);
			
			if ((display_mode_verb = draw_mode) == best_way)
				display_mode_verb = direct_copy;

			while (starts->flags != TERMINATOR_BITS) {

				if ((display_mode_verb == direct_copy)
					&& (!(pg->flags & PRINT_MODE_BIT))
					&& line_has_custom_styles(starts))
					display_mode_verb = bits_copy;

				if ((long) starts->offset >= local_end_offset)
					if (starts[1].offset - starts->offset)
						break;
					
					if (last_container != starts->r_num) {
						
						last_container = starts->r_num;

						pgCallContainerProc(pg, (pg_short_t)last_container, &vis_offset,
							clip_container_verb,  &pg->port.scale, &container_proc_refcon); 

						pgCallContainerProc(pg, (pg_short_t)last_container, &vis_offset,
							will_draw_verb,  &pg->port.scale, NULL); 
				}

				glitter.last_printed = last_printed;
				starts += draw_line(pg, block, text, starts, &walker,
						&vis_offset, &vis_bounds, wrap_base, &glitter,
						&lines_printed, &cur_drawn_line, display_mode_verb);
				last_printed = glitter.last_printed;
				if (!dont_erase) {
					
					if (walker.cur_par_style->table.table_columns)
						last_drawn_line = cur_drawn_line;
					else
						erase_from_last_line(pg, &vis_offset, &last_drawn_line, &cur_drawn_line);
				}	
			}

			pgPrepareStyleWalk(pg, 0, NULL, TRUE);

			UnuseMemory(block->lines);
			UnuseMemory(block->text);
		}
		
		starting_offset = block->end;
		
		if (block->end == pg->t_length || block->end > last_offset)
			break;

		++block;
		++cur_block;
	}
	
	if (cur_block < num_blocks)
		pgPaginateBlock(pg, block, NULL, TRUE);

	if (!dont_erase && drew_something)
		erase_end_display(pg, block, &vis_offset, &last_drawn_line, glitter.last_printed);

	if (last_container != -1)
		pgCallContainerProc(pg, (pg_short_t)last_container, NULL, unclip_container_verb,
				NULL, &container_proc_refcon); 

	if (pg->t_length || !(pg->doc_info.attributes & BOTTOM_FIXED_BIT))
		erase_top_or_bottom(pg, dont_erase, TRUE);
	else
	if (!pg->t_length) {

// Bug fix by Gar 12/10/95, update "last_drawn_bottom". 
		
		block = (text_block_ptr) UseMemoryRecord(pg->t_blocks, 0, 0, FALSE);
		pg->last_drawn_bottom.v_position = block->bounds.bot_right.v;
		pg->last_drawn_bottom.v_position += pg->port.origin.v - pg->scroll_pos.v;
	}
	
	UnuseMemory(pg->t_blocks);
	UnuseMemory(pg->wrap_area);

	if (hilite_mode && (!(pg->flags & DEACT_BITS)) && (!(pg->flags & PRINT_MODE_BIT))) {
		
	    set_caret(pg, restore_cursor);
		pg->flags |= old_caret_flags;
		pgDrawHighlight(pg, update_cursor);
    }
    
	if (display_verb >= bits_emulate_copy && display_verb <= bits_emulate_xor)
		pg->procs.bitmap_proc(pg, &pg->port, TRUE, &vis_bounds, &bits_emulate_offset, beginning_offset);

	pg->procs.set_device(pg, unset_pg_device, &pg->port, NULL);

	pg->flags &= (~BITMAP_ERASE_BIT);

	if (draw_port)
		pgBlockMove(&original_device, &pg->port, sizeof(graf_device));
	
	if (offset_extra)
		pgAddPt(offset_extra, &pg->scroll_pos);

	return	last_printed;
}



/* This function is called when only a few chars are drawn, as in key insertion.
This will NOT be called if (A) The new text is more than one style, (B) The
text is not at the end of a visible line, (C) if the line is not left
justified, or (D) If class_bits has "NO_SMART_DRAW_BIT" */

static void do_partial_display (paige_rec_ptr pg, long draw_from,
		pg_short_t num_bytes, short draw_mode)
{
	style_walk			walker;
	draw_points			draw_position;
	long PG_FAR			*char_locs;
	memory_ref			locs_ref, types_ref;
	co_ordinate			offset_extra;
	text_block_ptr		block;
	point_start_ptr		starts;
	long				offset_begin, offset_end;
	pg_char_ptr			text;
	pg_short_t			local_offset, length;
	
	pgSetupGrafDevice(pg, &pg->port, MEM_NULL, clip_standard_verb);
	length = num_bytes;
	offset_begin = draw_from;

	block = pgFindTextBlock(pg, draw_from, NULL, TRUE, TRUE);
	offset_begin -= block->begin;
	offset_end = block->end - block->begin;
	local_offset = (pg_short_t)offset_begin;
	starts = (point_start_ptr) UseMemory(block->lines);
	
	while (starts->offset < local_offset)
		++starts;
	
	if (starts->offset != local_offset)
		--starts;
	
	draw_position.starts = starts;
	draw_position.block = block;

	char_locs = pgGetCharLocs(pg, block, &locs_ref, &types_ref);
	draw_position.from.h = char_locs[local_offset] - char_locs[starts->offset];
	draw_position.from.h += starts->bounds.top_left.h;
	draw_position.from.v = starts->bounds.bot_right.v - starts->baseline;

	draw_position.to = draw_position.from;
	draw_position.to.h += (starts->bounds.bot_right.h - starts->bounds.top_left.h);
	draw_position.real_offset = draw_from;

	draw_position.descent = starts->baseline;
	draw_position.ascent = draw_position.from.v - starts->bounds.top_left.v;

	UnuseMemory(locs_ref);
	pgReleaseCharLocs(pg, locs_ref);

	text = (pg_char_ptr) UseMemory(block->text);
	pgPrepareStyleWalk(pg, draw_from, &walker, TRUE);
	
	if (local_offset)
		if (walker.cur_style->procs.char_info(pg, &walker, text, block->begin, offset_begin, offset_end, local_offset, LAST_HALF_BIT)) {

			--local_offset;
			++length;
		}

	if (walker.cur_style->procs.char_info(pg, &walker, text, block->begin,
			offset_begin, offset_end, local_offset + length - 1, FIRST_HALF_BIT))
		--length;

	if (length > 0) {
	
		offset_extra = pg->scroll_pos;
		pgNegatePt(&offset_extra);
		pgAddPt(&pg->port.origin, &offset_extra);
		pgAddPt(&offset_extra, &draw_position.from);
		pgAddPt(&offset_extra, &draw_position.to);
	
		scale_drawing(pg, &offset_extra, &draw_position);
		walker.cur_style->procs.install(pg, walker.cur_style, walker.cur_font,
							&walker.superimpose, walker.style_overlay, FALSE);

		if (text[local_offset] == SUBREF_CHAR && pgIsValidSubref(block, (long)local_offset))
			pgDrawSubRef(pg, NULL, &draw_position, local_offset, &offset_extra);
		else
			walker.cur_style->procs.draw(pg, &walker, text, local_offset, length, &draw_position, 0, draw_mode);
	}

	pgPrepareStyleWalk(pg, 0, NULL, TRUE);
	
	UnuseMemory(block->text);
	UnuseMemory(block->lines);
	UnuseMemory(pg->t_blocks);
	
	set_caret(pg, restore_cursor);
	pg->procs.set_device(pg, unset_pg_device, &pg->port, NULL);
}



/* This function advances style runs, total text length and block offsets. On
entry, block is the starting block and block_num its relative element position.
Offset is the starting (absolute) offset and length is the amount of adjustment.
Advance_equal_styles is passed to advance_style_run and if TRUE causes offsets
equal to the changed offset to get advanced; advance_equal_pars does same for
paragraph formats. If deleted_block is TRUE then block will (or has been) deleted. */

static void advance_text_runs (paige_rec_ptr pg, text_block_ptr block,
		pg_short_t block_num, long offset, long length, pg_boolean advance_equal_styles,
		pg_boolean advance_equal_pars, pg_boolean deleted_block)
{
	t_select_ptr				selections;
	pg_globals_ptr				globals;
	register pg_short_t			num_selects, index_ctr;
	select_pair_ptr				applied_pairs;
	style_run_ptr				par_exclusions;
	long						num_par_exclusions;
	pg_short_t					remaining_blocks;

	block->end += length;
	++block;
	
	remaining_blocks = (pg_short_t)GetMemorySize(pg->t_blocks) - block_num - 1;
	globals = pg->globals;

	while (remaining_blocks) {
		
		for (index_ctr = 0; index_ctr < WIDTH_QTY; ++index_ctr)
			if ((globals->width_tables[index_ctr].mem_id) == pg->mem_id && globals->width_tables[index_ctr].offset == block->begin) {
			
				globals->width_tables[index_ctr].offset += length;
				globals->width_tables[index_ctr].end += length;
			}
		
		block->begin += length;
		block->end += length;
		++block;
		--remaining_blocks;
	}

	advance_style_run(pg->t_style_run, offset, length, advance_equal_styles);
	advance_style_run(pg->par_style_run, offset, length, advance_equal_pars);
	
	if ((par_exclusions = pgFindParExclusionRun(pg, offset, &num_par_exclusions)) != NULL) {
		
		while (num_par_exclusions) {
		
			par_exclusions->offset += length;
			
			++par_exclusions;
			--num_par_exclusions;
		}
		
		UnuseMemory(pg->par_exclusions);
	}

	selections = (t_select_ptr) UseMemory(pg->select);
	num_selects = pg->num_selects + 1;
	
	while (num_selects) {

		selections->flags |= SELECTION_DIRTY;

		if (length < 0) {

			if (selections->offset > offset)
				selections->offset += length;
		}
		else
		if (selections->offset >= offset)
			selections->offset += length;
		
		++selections;
		--num_selects;
	}

	if (!pg->num_selects) {
		
		selections = (t_select_ptr) UseMemoryRecord(pg->select, 0, 0, FALSE);
		selections[1] = selections[0];
		pg->hilite_anchor = selections->offset;
	}

	UnuseMemory(pg->select);
	
	if (pg->applied_range) {
		
		num_selects = (pg_short_t)GetMemorySize(pg->applied_range);
		applied_pairs = (select_pair_ptr) UseMemory(pg->applied_range);
		
		while (num_selects) {

			if (length < 0) {
	
				if (applied_pairs->begin > offset)
					applied_pairs->begin += length;
				if (applied_pairs->end > offset)
					applied_pairs->end += length;
			}
			else
			if (applied_pairs->end >= offset)
				applied_pairs->end += length;
			
			++applied_pairs;
			--num_selects;
		}
		
		UnuseMemory(pg->applied_range);
	}
	
	pgAdvanceHyperlinks(pg, pg->hyperlinks, offset, length);
	pgAdvanceHyperlinks(pg, pg->target_hyperlinks, offset, length);

	pg->stable_caret.h = pg->stable_caret.v = 0;
	pg->procs.text_increment(pg, offset, length);
}


/* fix_deleted_pars walks through the par_info run and removes par_infos that
no longer define a true paragraph boundary (i.e., the CR has been removed). */

static void fix_deleted_pars (paige_rec_ptr pg, long deleted_from, long deleted_to)
{
	register style_run_ptr			par_run;
	register long					ending_offset;
	select_pair						true_par;
	size_t							beginning_offset, run_rec_num;

	ending_offset = deleted_to + 1;

	if (ending_offset > pg->t_length)
		ending_offset = pg->t_length;
	
	if ((beginning_offset = deleted_from) > 0)
		beginning_offset -= 1;

	par_run = pgFindParStyleRun(pg, beginning_offset, &run_rec_num);
	
	while (par_run->offset < ending_offset) {
		
		true_par.begin = true_par.end = par_run->offset;
		pg->procs.boundary_proc(pg, &true_par);
		
		if (true_par.begin != par_run->offset) {
			
			UnuseMemory(pg->par_style_run);
			DeleteMemory(pg->par_style_run, run_rec_num, 1);
			par_run = (style_run_ptr) UseMemoryRecord(pg->par_style_run, run_rec_num, USE_ALL_RECS, TRUE);
		}
		else {
			
			++par_run;
			++run_rec_num;
		}
	}

	UnuseMemory(pg->par_style_run);
}



/* This function returns TRUE if the block will be deleted. This can happen
if it is now empty (and not the only block) or if the terminator char was
been removed.  The flag "BROKE_BLOCK" is also set if appropriate. Note that this
function gets called BEFORE the block info has been altered.  */

static short block_should_delete (paige_rec_ptr pg, text_block_ptr block,
		pg_short_t block_num, pg_short_t local_offset, pg_short_t local_length)
{
	pg_short_t			total_blocks, text_size;

	total_blocks = (pg_short_t)GetMemorySize(pg->t_blocks);
	
	if (total_blocks == 1)
		return	FALSE;
	
	text_size = (pg_short_t)(block->end - block->begin);
	
	if (local_length == text_size)
		return	TRUE;
	
	--total_blocks;

	if (block_num < total_blocks) {
		
		if ((local_offset + local_length) == text_size) {
			
			block->flags |= BROKE_BLOCK;
			return	TRUE;
		}
	}
	
	return	FALSE;
}


/* This increments all the style runs affected by offset. The length can also
be negative (which would be the case for a deletion).  If include_equal_offsets
is TRUE, the advance begins at an offset equal to the offset parameters.  */

static void advance_style_run (style_ref the_run, long offset, long length,
		pg_boolean include_equal_offsets)
{
	register long			adjust_amount;
	register long 			minimum_offset;
	register style_run_ptr	run;
	pg_short_t				num_runs;
	
	adjust_amount = length;
	minimum_offset = offset;

	if (include_equal_offsets)
		--minimum_offset;

	num_runs = (pg_short_t)GetMemorySize(the_run);

	for (run = (style_run_ptr) UseMemory(the_run); minimum_offset >= run->offset; ++run, --num_runs) ;
	
	if (num_runs > 1)
		if ((run->offset == run[1].offset) || (!run->offset)) {

		++run;
		--num_runs;
	}
	
	while (num_runs) {
		run->offset += adjust_amount;
		
		if (run->offset < 0)
			run->offset = 0;

		++run;
		--num_runs;
	}
	
	UnuseMemory(the_run);
}


/* This function handles the insertion of "key" characters. This is different
than a regular insertion except the character get be mixed (or exclusively)
with arrows, backspace, etc.   The modifiers is looked at only for action keys, such
as arrows. NOTE: The char_bytes field is temporarily forced to 0. The purpose
of this is to ensure special control code for the incoming stream are
recognized.     */

static void insert_key_data (paige_rec_ptr pg, pg_char_ptr data, size_t length,
		short modifiers, short draw_mode)
{
	style_info			insert_style;
	font_info			insert_font;
	style_walk			fake_walker;
	style_run			fake_run;
	style_run_ptr		run_ptr;
	long				offset_scanner, length_scanner, info;
	long				last_insertion, current_length, current_position;
	pg_boolean			uses_smart_quotes;
	short				font_index;
	
	if ((!pg->num_selects) && (pg->flags & CARET_BIT) && draw_mode)
		pgTurnOffHighlight(pg, TRUE);
	
	current_position = pgCurrentInsertion(pg);
	
	if (uses_smart_quotes = ((pg->flags & SMART_QUOTES_BIT) != 0)) {

		pgFillBlock(&fake_walker, sizeof(style_walk), 0);
	
		GetMemoryRecord(pg->t_formats, pg->insert_style, &insert_style);

		if ((font_index = insert_style.font_index) == DEFAULT_FONT_INDEX)
			font_index = 0;

		GetMemoryRecord(pg->fonts, font_index, &insert_font);
		fake_walker.cur_style = &insert_style;
		fake_walker.cur_font = &insert_font;
		run_ptr = pgFindStyleRun(pg, current_position, NULL);
		fake_run = *run_ptr;
		UnuseMemory(pg->t_style_run);
		
		if (fake_run.style_item != (pg_short_t)font_index)
			fake_run.offset = 0;
		
		fake_walker.prev_style_run = &fake_run;
	}

	fake_walker.current_offset = current_position;

	for (length_scanner = length, offset_scanner = last_insertion = current_length = 0;
				length_scanner; ++offset_scanner, --length_scanner) {
		
		if (special_action_key(pg, fake_walker.current_offset, &data[offset_scanner])) {
			
			dump_key_data(pg, &insert_style, data, last_insertion, current_length, draw_mode);
			handle_action_key(pg, data, offset_scanner, modifiers, draw_mode);

			current_length = 0;
			last_insertion = offset_scanner + 1;
			fake_walker.current_offset = pgCurrentInsertion(pg);
		}
		else {
			
			if (uses_smart_quotes)
				info = insert_style.procs.char_info(pg, &fake_walker, data, NO_BLOCK_OFFSET,
						0, length, offset_scanner, QUOTE_BITS);
			else
				info = 0;

			if (info) {
				pg_char			holding_buf[4];
				short			holding_size;
				
				dump_key_data(pg, &insert_style, data, last_insertion, current_length, draw_mode);
				last_insertion = offset_scanner;
				current_length = 0;
				fake_walker.current_offset = pgCurrentInsertion(pg);
				
				holding_size = 1;

				if (info & FIRST_HALF_BIT)
					if (length_scanner > 1) {
					
						holding_size += 1;
						offset_scanner += 1;
						length_scanner -= 1;
						last_insertion = offset_scanner;
				}
				
				pgBlockMove(&data[last_insertion], holding_buf, holding_size * sizeof(pg_char));
				last_insertion += 1;		// PAJ

				pg->procs.smart_quotes(pg, fake_walker.current_offset,
						info, holding_buf, &holding_size);
				dump_key_data(pg, &insert_style, holding_buf, 0, holding_size, draw_mode);
			}
			else
				current_length += 1;
		}
	}

	if (current_length)
		dump_key_data(pg, &insert_style, data, last_insertion, current_length, draw_mode);
}


/* The following function works mainly with insert_key_data, above. It inserts
actual key chars that are pending in the data buffer (or does nothing if length
is zero). Drawing occurs if draw_mode != 0.  */

static void dump_key_data (paige_rec_ptr pg, style_info_ptr style,
		pg_char_ptr data, long offset, long length, short draw_mode)
{
	smart_update	update_info;
	long			insertion_spot, old_text_size;
	pg_short_t		must_draw_all;

	if (!length)
		return;

	++pg->change_ctr;

	if (must_draw_all = pg->num_selects)
		pgDelete(pg->myself, NULL, draw_none);

	insertion_spot = pgCurrentInsertion(pg);
	old_text_size = pg->t_length;

	data += offset;		
	
	if (draw_mode)
		pgInsertRawData(pg, data, length, insertion_spot, &update_info, key_insert_mode);
	else
		pgInsertRawData(pg, data, length, insertion_spot, NULL, key_insert_mode);

	clear_line_end_flag(pg);

	if (pg->last_key_check > MAX_KEY_CHECK)
		if (pgFixBadBlocks(pg))
			must_draw_all = TRUE;

	if (must_draw_all && draw_mode) {

		update_info.suggest_begin = 0;
		update_info.suggest_end = pg->t_length;
		update_info.flags = 0;
	}
	
	if (draw_mode) {
		shape_section		last_bottom;
		
		last_bottom = pg->last_drawn_bottom;
		
		if (draw_mode == best_way) {			
			
			if (!(style->class_bits & NO_SMART_DRAW_BIT)
				&& (update_info.flags & CAN_DRAW_PARTIAL_LINE)
				&& !(pg->flags & SHOW_INVIS_CHAR_BIT)
				&& !(pg->port.scale.scale)
				&& !(pg->port.print_port))
				do_partial_display(pg, insertion_spot, (pg_short_t)length, direct_or);
			else
				pgUpdateText(pg, &pg->port, update_info.suggest_begin, update_info.suggest_end, MEM_NULL, NULL,
					bits_copy, TRUE);
		}
		else {

			pgUpdateText(pg, &pg->port, update_info.suggest_begin, update_info.suggest_end, MEM_NULL, NULL,
				draw_mode, TRUE);
		}
		
		if (last_bottom.v_position != pg->last_drawn_bottom.v_position)
			pgDrawPages(pg, NULL, NULL, MEM_NULL, draw_mode);
	}
}


/* This small function clears the "line end" cursor flag in the single selection. */

static void clear_line_end_flag (paige_rec_ptr pg)
{
	t_select_ptr		selection;

	selection = (t_select_ptr) UseMemory(pg->select);
	selection->flags &= CLR_END_LINE_FLAG;
	UnuseMemory(pg->select);
}


/* This function handles backspace and "arrow" direction keys. */

static void handle_action_key (paige_rec_ptr pg, pg_char_ptr data, long offset,
			short modifiers, short draw_mode)
{
	register pg_globals_ptr		globals;
	register pg_char			the_key;
	smart_update				update_info;
	size_t						current_selection, num_to_delete;
	size_t						delete_to, deleted;

	globals = pg->globals;
	the_key = data[offset];
	current_selection = pgCurrentInsertion(pg);
	clear_line_end_flag(pg);

	if (the_key == globals->bs_char || the_key == globals->fwd_delete_char) {
		size_t			delete_from, display_from, after_delete_insertion;
		short			use_draw_mode;
		
		if (pg->num_selects) {
			
			if ((use_draw_mode = draw_mode) == best_way)
				use_draw_mode = bits_copy;

			pgDelete(pg->myself, NULL, draw_none);
			if (use_draw_mode)
				pgUpdateText(pg, &pg->port, 0, pg->t_length, MEM_NULL, NULL,
					use_draw_mode, TRUE);
		}
		else {
			
			if (the_key == globals->bs_char) {
			
				if (!current_selection) {

					set_caret(pg, restore_cursor);
					return;
				}
				
				//after_delete_insertion = current_selection + -1;
				after_delete_insertion = increment_selection(pg, -1, 0, FALSE);
			}
			else {
				
				if (current_selection == pg->t_length) {
					
					set_caret(pg, restore_cursor);
					return;
				}
					
				after_delete_insertion = current_selection;
			}
			
			if (pg->flags & NO_HIDDEN_TEXT_BIT) {
				style_info_ptr		bs_style;
				short				hidden;
				
				bs_style = pgFindTextStyle(pg, after_delete_insertion);
				hidden = bs_style->styles[hidden_text_var];
				UnuseMemory(pg->t_formats);
				
				if (hidden) {
					
					set_caret(pg, restore_cursor);
					return;
				}
			}
			
			++pg->change_ctr;

			if (the_key == globals->bs_char) {

				delete_from = increment_selection(pg, -1, 0, FALSE);
				delete_to = current_selection;
			}
			else {
				
				delete_from = current_selection;
				delete_to = increment_selection(pg, 1, 0, FALSE);
			}
			
			pgFixCellBackspace(pg, &delete_from, &delete_to);

			num_to_delete = delete_to - delete_from;
			
			while (num_to_delete) {
				
				current_selection = delete_to;
				display_from = pgDeleteRawData(pg, delete_from, &current_selection, &update_info);

				deleted = current_selection - delete_from;
				
				num_to_delete -= deleted;
				delete_to -= deleted;
			}
			
			if (draw_mode)
				pgUpdateText(pg, &pg->port, display_from, update_info.suggest_end, MEM_NULL, NULL,
							bits_copy, TRUE);
		}
	}
	else
	if (the_key == globals->left_arrow_char) {
		
		if (!current_selection) {
			
			if (pg->num_selects && (modifiers & EXTEND_MOD_BIT)) {
				
				if (pg->hilite_anchor)
					return;
			}
			else {
/* Note, the selection has been turned off here if EXTEND_MOD_BIT is ON. */

				if (pg->num_selects & !(modifiers & EXTEND_MOD_BIT))
					pgSetSelection(pg->myself, 0, 0, 0, FALSE);
				else
					set_caret(pg, restore_cursor);
	
				return;
			}
		}

		increment_selection(pg, -1, (short)(modifiers | PIVOT_EXTEND_BIT), TRUE);
	}
	else
	if (the_key == globals->right_arrow_char) {

		if (current_selection == pg->t_length) {

			if (pg->num_selects && (modifiers & EXTEND_MOD_BIT)) {
				
				if (pg->hilite_anchor < current_selection)
					return;
			}
			else {

/* Note, the selection has been turned off here if EXTEND_MOD_BIT is ON. */

				if (pg->num_selects & !(modifiers & EXTEND_MOD_BIT))
					pgSetSelection(pg->myself, pg->t_length, pg->t_length, 0, FALSE);
				else
					set_caret(pg, restore_cursor);
	
				return;
			}
		}

		increment_selection(pg, 1, (short)(modifiers | PIVOT_EXTEND_BIT), TRUE);
	}
	else
	if (the_key == globals->up_arrow_char)
		increment_line_select(pg, (short)(modifiers | PIVOT_EXTEND_BIT | ARROW_ACTIVE_BIT), -1);
	else
	if (the_key == globals->down_arrow_char)
		increment_line_select(pg, (short)(modifiers | PIVOT_EXTEND_BIT | ARROW_ACTIVE_BIT), 1);
	else
	if (the_key == globals->container_brk_char)
		pgTabToCell(pg);

	pg->stable_caret.h = pg->stable_caret.v = 0;

	set_caret(pg, restore_cursor);
}



/* This function aligns a byte offset to the proper character position. This is
necessary since Paige text can have multibyte chars. On entry, position is the
wanted byte position and byte_advance should be +1 if you are going forward (from
a right arrow key response, for instance) or -1 if you are going backwards (from
the delete key, for instance). 		*/

static long align_selection (paige_rec_ptr pg, long position, long byte_advance)
{
	style_walk				walker;
	text_block_ptr			block;
	pg_char_ptr				text;
	long					offset_end, new_position, char_flags, hide_text, no_lfs;

	new_position = position;
	pgPrepareStyleWalk(pg, new_position, &walker, FALSE);
	
	block = pgFindTextBlock(pg, new_position, NULL, FALSE, TRUE);
	text = (pg_char_ptr) UseMemory(block->text);
	offset_end = block->end - block->begin;

	if ((byte_advance > 0) && (new_position < pg->t_length)) {

		pgWalkStyle(&walker, -1);
		
		if (walker.cur_style->procs.char_info(pg, &walker, text,
					block->begin, 0, offset_end, new_position - block->begin, SOFT_HYPHEN_BIT))
			new_position += byte_advance;

		pgSetWalkStyle(&walker, new_position);
	}
	
	hide_text = pg->flags & NO_HIDDEN_TEXT_BIT;
	no_lfs = pg->flags & NO_LF_BIT;

	if (new_position < pg->t_length) {
		
		for (;;) {
		
			char_flags = walker.cur_style->procs.char_info(pg, &walker, text, block->begin,
				0, offset_end, new_position - block->begin, LAST_HALF_BIT | MIDDLE_CHAR_BIT | LINE_SEL_BIT);
			
			if ( !(char_flags & (LAST_HALF_BIT | MIDDLE_CHAR_BIT))
				&& !(hide_text && walker.cur_style->styles[hidden_text_var])
				&& !((char_flags & LINE_SEL_BIT) && no_lfs) )
				break;
	
			new_position += byte_advance;
			pgWalkStyle(&walker, byte_advance);
			
			if (!new_position || new_position == pg->t_length)
				break;
		}
	}

	if ((byte_advance < 0) && new_position)
		if (walker.cur_style->procs.char_info(pg, &walker, text, block->begin,
				0, offset_end, new_position - block->begin, SOFT_HYPHEN_BIT))
			new_position += byte_advance;

	pgPrepareStyleWalk(pg, 0, NULL, FALSE);
	UnuseMemory(block->text);
	UnuseMemory(pg->t_blocks);
	
	return	new_position;
}


/* This function returns the offset num_chars away from the current insertion. It is
CHARACTER BASED, not byte based. The (new) byte offset is returned. If set_it is
TRUE, the new selection point gets set according to set_it's draw mode (which
should be one of the cursor modes). */

static long increment_selection (paige_rec_ptr pg, long num_chars, short modifiers, short set_it)
{
	long					new_position, byte_advance;
	t_select				first_selection;
	pg_short_t				use_select;

	if (modifiers & CONTROL_MOD_BIT) {

		if (num_chars < 0)
			use_select = previous_word_caret;
		else
			use_select = next_word_caret;

		if (modifiers & EXTEND_MOD_BIT)
			use_select |= EXTEND_CARET_FLAG;

		pgSetCaretPosition(pg->myself, use_select, (pg_boolean)set_it);
		new_position = pgCurrentInsertion(pg);
	}
	else {
	
		if (modifiers & EXTEND_MOD_BIT) {
			pg_short_t			select_rec;
	
			pgExtendSelection(pg->myself, num_chars, modifiers, set_it);
	
			if ((!pg->num_selects) || (num_chars >= 0))
				select_rec = 0;
			else
				select_rec = pg->num_selects;
	
			GetMemoryRecord(pg->select, select_rec, &first_selection);
			
			return	first_selection.offset;
		}
	
		if (set_it) {
	
			if (pg->num_selects) {
				
				if (num_chars > 0)
					use_select = pg->num_selects * 2 - 1;
				else
					use_select = 0;
	
				GetMemoryRecord(pg->select, use_select, &first_selection);
				pgSetSelection(pg->myself, first_selection.offset, first_selection.offset, 0, FALSE);
			}
		}
	
		byte_advance = 1;
		if (num_chars < 0)
			byte_advance = -byte_advance;
		
		new_position = align_selection(pg, pgCurrentInsertion(pg) + num_chars, byte_advance);
		
		if (set_it)
			pgSetSelection(pg->myself, new_position, new_position, 0, set_it);
		else
			pgSetNextInsertIndex(pg);
	}

	return	new_position;
}


/* This function increments or decrements the selection to the next or previous
line. It does so by faking a pgDragSelect. Negative numbers are UP. */

static void increment_line_select(paige_rec_ptr pg, short modifiers, short direction)
{
	text_block_ptr				block;
	t_select_ptr				selection;
	register point_start_ptr	starts;
	long						previous_container, scaled_extra;
	co_ordinate					fake_pt;
	
	selection = (t_select_ptr) UseMemory(pg->select);
	
	if (modifiers & EXTEND_MOD_BIT) {

		if (modifiers & PIVOT_EXTEND_BIT) {
			
			if (pg->num_selects)
				if (selection->offset >= pg->hilite_anchor)
					selection += (pg->num_selects * 2 - 1);
		}
		else {
		
			if (pg->num_selects)
				if (direction > 0)
					selection += (pg->num_selects * 2 - 1);
		}
	}
	else {
		
		if (pg->num_selects)
			if (direction >= 0)
				*selection = selection[1];

		pg->num_selects = 0;			
	}
	
	block = pgFindTextBlock(pg, selection->offset, NULL, FALSE, TRUE);
	starts = (point_start_ptr) UseMemory(block->lines);
	starts += selection->line;

	fake_pt.h = starts->bounds.top_left.h + selection->primary_caret;
	previous_container = starts->r_num;

	if (direction < 0) {
		
		for (;;) {
		
			while (!(starts->flags & NEW_LINE_BIT)) {
			
				if (!starts->offset)
					break;
					
				--starts;
			}
			
			if (starts->offset) {
			
				--starts;
				
				break;
			}
			else
			if (block->begin == 0)
				break;
				
			UnuseMemory(block->lines);
			
			--block;
			pgPaginateBlock(pg, block, NULL, FALSE);
			starts = (point_start_ptr) UseMemory(block->lines);
			
			while (starts[1].flags != TERMINATOR_BITS)
				++starts;
		}
	}
	else {
		
		for (;;) {
		
			while (!(starts->flags & LINE_BREAK_BIT))
				++starts;
			
			if (starts[1].flags != TERMINATOR_BITS) {
			
				++starts;
				break;
			}
			else
			if (block->end >= pg->t_length)
				break;
				
			UnuseMemory(block->lines);
			++block;
			pgPaginateBlock(pg, block, NULL, FALSE);
			starts = (point_start_ptr) UseMemory(block->lines);
		}
	}

	scaled_extra = 1;
	pgScaleLong(-pg->port.scale.scale, 0, &scaled_extra);
	
	if (scaled_extra == 0)
		scaled_extra = 1;

	fake_pt.v = starts->bounds.top_left.v + scaled_extra;
	
	if (starts->r_num != previous_container) {
		rectangle			wrap_rect;
		pg_short_t			r_num;
		
		r_num = pgGetWrapRect(pg, starts->r_num, NULL);
		GetMemoryRecord(pg->wrap_area, r_num + 1, &wrap_rect);
		
		if (fake_pt.h < wrap_rect.top_left.h)
			fake_pt.h = wrap_rect.top_left.h + scaled_extra;
		else
		if (fake_pt.h > wrap_rect.bot_right.h)
			fake_pt.h = wrap_rect.bot_right.h - scaled_extra;
	}

	pgSubPt(&pg->scroll_pos, &fake_pt);
	pgAddPt(&pg->port.origin, &fake_pt);

	UnuseMemory(block->lines);
	UnuseMemory(pg->t_blocks);
	UnuseMemory(pg->select);
	
	pgScalePt(&pg->port.scale, NULL, &fake_pt);
	pgDragSelect(pg->myself, &fake_pt, mouse_down, modifiers, 0, FALSE);
	pgDragSelect(pg->myself, &fake_pt, mouse_up, modifiers, 0, FALSE);
}


/* This function increments (or decrements) a point start run beginning at offset
for amount bytes. The purpose is to try and second-guess where new insertions or
deletions will leave the point_start run for minimum screen updates.  */

static void advance_line_run (paige_rec_ptr pg, text_block_ptr block, pg_short_t offset, long amount)
{
	register point_start_ptr		starts;
	register short					previous_offset, new_offset;
	point_start_ptr					first_starts;

	if (block->begin == block->end)
		return;
	if (!(block->flags & SOME_LINES_GOOD) || (block->flags & LINES_PURGED))
		return;

	starts = (point_start_ptr) UseMemory(block->lines);
	first_starts = starts;

	for (previous_offset = 0; starts->flags != TERMINATOR_BITS; ++starts) {
		
		if (starts->offset)
			if (starts->offset == offset)
				if (!(starts->flags & NEW_PAR_BIT)) {
			
			starts->flags &= (~LINE_GOOD_BIT);
			--starts;
			starts->flags &= (~LINE_GOOD_BIT);
			++starts;
		}

		if (starts->offset > offset) {
			
			starts->offset += (pg_short_t)amount;

			if (amount < 0) {
				
				new_offset = starts->offset;

				if (new_offset <= previous_offset) {
					
					starts->flags &= (~LINE_GOOD_BIT);
					starts->offset = previous_offset;
				}
			}
		}

		previous_offset = starts->offset;
	}
	
	starts->offset += (pg_short_t)amount;
	

	if (amount < 0) {
		par_info_ptr					par_base;
		style_run_ptr					par_run;
		long							par_offset;
		pg_short_t						par_index, local_offset;
		
		local_offset = offset;
		par_offset = block->begin;
		par_offset += (long)local_offset;
		par_base = (par_info_ptr) UseMemory(pg->par_formats);
		par_run = pgFindParStyleRun(pg, par_offset, NULL);
		par_index = par_run->style_item;
		
		if (par_base[par_index].class_info & SPECIAL_PAR_BITS) {
			
			starts = first_starts;
			
			while (starts->offset <= local_offset) {
				
				++starts;
				
				if (starts->flags == TERMINATOR_BITS)
					break;
			}
			
			if (par_run->offset < block->begin)
				local_offset = 0;
			else
				local_offset = (pg_short_t)(par_run->offset - block->begin);
			
			for (;;) {
				
				--starts;
				starts->flags &= (~LINE_GOOD_BIT);
				
				if (starts->offset == 0)
					break;

				if (starts->flags <= local_offset) {
					
					--par_run;
					par_index = par_run->style_item;
					
					if (!(par_base[par_index].class_info & SPECIAL_PAR_BITS))
						break;

					if (par_run->offset < block->begin)
						local_offset = 0;
					else
						local_offset = (pg_short_t)(par_run->offset - block->begin);
				}
			}
		}
		
		UnuseMemory(pg->par_style_run);
		UnuseMemory(pg->par_formats);
	}

	UnuseMemory(block->lines);
}


/* This function returns TRUE if line contains any custom styles.  */

static short line_has_custom_styles (point_start_ptr line_starts)
{
	register point_start_ptr	starts;
	
	for (starts = line_starts; starts->flags != TERMINATOR_BITS;
			++starts) {
		
		if (starts->flags & CUSTOM_CHARS_BIT)
			return	TRUE;
		
		if (starts->flags & LINE_BREAK_BIT)
			return	FALSE;
	}
	
	return	FALSE;
}



/* This function erases the top of wrap area to the first line (if do_bottom
= FALSE) but only if last_drawn_top is different than the real top. If
do_bottom = TRUE, it is the opposite.  In both cases, the info in pg is
updated to avoid erasing when not necessary. */

static void erase_top_or_bottom (paige_rec_ptr pg, short dont_draw, pg_boolean do_bottom)
{
	text_block_ptr			block;
	point_start_ptr			starts;
	shape_section_ptr		end_position;
	shape_section			section;
	co_ordinate				scroll_offset;
	
	scroll_offset.h = -pg->scroll_pos.h;
	scroll_offset.v = -pg->scroll_pos.v;
	pgAddPt(&pg->port.origin, &scroll_offset);
	
	block = (text_block_ptr) UseMemory(pg->t_blocks);

	if (do_bottom) {
		
		block += GetMemorySize(pg->t_blocks) - 1;
		
		if (!(block->flags & (NEEDS_CALC | NEEDS_PAGINATE | SOME_LINES_BAD | LINES_PURGED))) {

			section.r_num = block->end_start.r_num;
			section.v_position = block->end_start.bounds.bot_right.v + (long)block->end_start.cell_height;

			if ((section.v_position < pg->last_drawn_bottom.v_position)
				|| (section.r_num < pg->last_drawn_bottom.r_num)
				|| (pg->flags2 & BITMAP_SCROLL_CHANGE))
					if (!dont_draw) {
						
						if (pg->flags2 & BITMAP_SCROLL_CHANGE)
							end_position = NULL;
						else
							end_position = &pg->last_drawn_bottom;

						if (pg->flags2 & PAR_EXCLUSION_CHANGE) {
							rectangle		vis_rect;
							
							pgShapeBounds(pg->vis_area, &vis_rect);
							
							if (vis_rect.bot_right.v > pg->last_drawn_bottom.v_position)
								pg->last_drawn_bottom.v_position = vis_rect.bot_right.v;
						}

						pgEraseContainerArea(pg, &pg->scale_factor, &scroll_offset, &section, end_position);
					}
					
			pg->last_drawn_bottom = section;
		}
	}
	else {
		
		starts = (point_start_ptr) UseMemory(block->lines);
		
		section.r_num = starts->r_num;
		section.v_position = starts->bounds.top_left.v;

		UnuseMemory(block->lines);

		if ((section.v_position > pg->last_drawn_top.v_position)
				|| (section.r_num > pg->last_drawn_top.r_num))
			if (!dont_draw)
				pgEraseContainerArea(pg, &pg->scale_factor, &scroll_offset, NULL, &section);
		
		pg->last_drawn_top = section;
	}

	UnuseMemory(pg->t_blocks);
}


/* erase_top_margin erases the top area if we are using forced, internal margins. This
only gets called if USE_MARGINS_BIT is set. */

static void erase_top_margin (paige_rec_ptr pg)
{
	rectangle			wrap_bounds, vis_bounds;
	
	pgShapeBounds(pg->wrap_area, &wrap_bounds);
	pgShapeBounds(pg->vis_area, &vis_bounds);
	wrap_bounds.bot_right.v = wrap_bounds.top_left.v + pg->doc_info.margins.top_left.v;
	pgOffsetRect(&wrap_bounds, -pg->scroll_pos.h, -pg->scroll_pos.v);
	
	if (pgSectRect(&wrap_bounds, &vis_bounds, &wrap_bounds))
		pg->procs.container_proc(pg, (pg_short_t)0, &wrap_bounds, &pg->scale_factor,
		NULL, erase_rect_verb, (void PG_FAR *)NULL);
}


/* compute_top_and_bottom gets called to return the lowest top and/or bottom
when line records are not always horizontal. Either top or bottom can be
NULL parameters. */

static void compute_top_and_bottom (text_block_ptr block, pg_short_t starts_offset,
		long PG_FAR *top, long PG_FAR *bottom)
{
	register point_start_ptr		starts;
	long							lowest, highest;

	starts = (point_start_ptr) UseMemory(block->lines);
	
	while (starts->offset < starts_offset) {

		if (starts[1].flags == TERMINATOR_BITS)
			break;

		++starts;
	}
	
	while (!(starts->flags & NEW_LINE_BIT)) {
		
		if (!starts->offset)
			break;
		--starts;
	}
	
	lowest = starts->bounds.top_left.v;
	highest = starts->bounds.bot_right.v;
	
	for (;;) {
		
		if (starts->bounds.top_left.v < lowest)
			lowest = starts->bounds.top_left.v;
		if (starts->bounds.bot_right.v > highest)
			highest = starts->bounds.bot_right.v;
		
		if (starts->flags & LINE_BREAK_BIT)
			break;
		
		++starts;
		
		if (starts->flags == TERMINATOR_BITS)
			break;
	}
	
	UnuseMemory(block->lines);

	if (top)
		*top = lowest;
	if (bottom)
		*bottom = highest;
}


/* erase_end_display checks for the situation where the next block's starting line is below
the last drawn line but also below the vis rect. This can cause redraw problems, so this
function will erase the gap between them, if necessary. */

static void erase_end_display (paige_rec_ptr pg, text_block_ptr block, co_ordinate_ptr vis_offset,
		graphic_line_ptr last_line, long last_drawn_offset)
{
	graphic_line_info	current_line;
	point_start_ptr		starts;

	if (!(block->flags & ALL_TEXT_HIDDEN) && block->begin && block->begin == last_drawn_offset) {

		pgPaginateBlock(pg, block, NULL, TRUE);				
		starts = (point_start_ptr) UseMemory(block->lines);
		current_line.bounds = starts->bounds;
		current_line.bounds.bot_right.v += (long)starts->cell_height;
		current_line.r_num = starts->r_num;
		current_line.cell_num = (long)(starts->cell_num & CELL_NUM_MASK);
		UnuseMemory(block->lines);

		erase_from_last_line(pg, vis_offset, last_line, &current_line);
	}
}


/* This function gets called by pgUpdateText and handles the situation where
text jumps to a new rectangle and/or irregular vertical position which will leave
an unerased area between bottom of last_line to top of cur_line. Initially
(before any lines are drawn by caller), last_line->r_num == -1 to indicated
no erasing should occur. This vis_offset is the total amount that is added to
the lines to be drawn.  */

static void erase_from_last_line (paige_rec_ptr pg, co_ordinate_ptr vis_offset,
		graphic_line_ptr last_line, graphic_line_ptr cur_line)
{
	register rectangle_ptr		wrap_r;
	register long				begin_r, end_r;
	rectangle					target;
	co_ordinate					offset_extra;
	long						container_proc_refcon;
	pg_short_t					rect_index;
	pg_boolean					called_clip;

	
	if (last_line->cell_num != 0 && cur_line->cell_num != 0)
		return;

	if ((last_line->r_num != -1) && ((cur_line->bounds.top_left.v > last_line->bounds.bot_right.v)
				|| (cur_line->r_num != last_line->r_num)))  {
		
		begin_r = last_line->r_num;
		end_r = cur_line->r_num;

		wrap_r = (rectangle_ptr) UseMemory(pg->wrap_area);
		
		++wrap_r;
		
		called_clip = FALSE;
		container_proc_refcon = 0;

		while (begin_r <= end_r) {
			
			rect_index = pgGetWrapRect(pg, begin_r, &offset_extra);
			target = wrap_r[rect_index];

			if (begin_r == last_line->r_num)
				target.top_left.v = last_line->bounds.bot_right.v - offset_extra.v;
			
			if (begin_r == end_r)
				target.bot_right.v = cur_line->bounds.top_left.v - offset_extra.v;

			pgOffsetRect(&target, offset_extra.h, offset_extra.v);
			
			if (pgSectRect(&last_line->bounds, &target, NULL))
				target.top_left.v = last_line->bounds.bot_right.v;
			if (pgSectRect(&cur_line->bounds, &target, NULL))
				target.bot_right.v = cur_line->bounds.top_left.v;

			if (target.bot_right.v > target.top_left.v) {
			
				pg->procs.container_proc(pg, (pg_short_t)(begin_r + 1), &target, &pg->port.scale,
						vis_offset, clip_container_verb, (void PG_FAR *)&container_proc_refcon);
			
				pg->procs.container_proc(pg, (pg_short_t)(begin_r + 1), &target, &pg->scale_factor,
						vis_offset, erase_rect_verb, (void PG_FAR *)NULL);

				called_clip = TRUE;
			}

			++begin_r;
		}

		UnuseMemory(pg->wrap_area);

		if (called_clip)
			pg->procs.container_proc(pg, (pg_short_t)begin_r, &target, &pg->port.scale,
						&offset_extra, unclip_container_verb, (void PG_FAR *)&container_proc_refcon);
	}

	*last_line = *cur_line;
}


/* This functions returns the current number of pages in pg's doc_info */

static pg_short_t get_num_pages (paige_rec_ptr pg)
{
	if (pg->doc_info.attributes & (V_REPEAT_BIT | H_REPEAT_BIT)) {
		co_ordinate		page_pixel;
		
		page_pixel.v = pg->doc_bounds.bot_right.v + pg->doc_info.repeat_slop - 1;
		page_pixel.h = pg->doc_bounds.bot_right.h - 1;

		return 	pgPixelToPage(pg->myself, &page_pixel,
				NULL, NULL, NULL, NULL, FALSE) + 1;
	}

	return	1;
}


/* This function checks to see if number of pages has changed since last
insertion and, if so, forces a re-draw (update) of pages on screen. Note this
function doesn't get called unless "draw_mode" is/has been active. */

static void extend_visual_pages (paige_rec_ptr pg, short draw_mode)
{
	if (pg->doc_info.attributes & (V_REPEAT_BIT || H_REPEAT_BIT)) {
		long			num_pages;
		
		num_pages = get_num_pages(pg);
		
		if (pg->doc_info.num_pages != (pg_short_t)num_pages) {
			
			pg->port.clip_info.change_flags |= CLIP_PAGE_CHANGED;
			
			if (num_pages < (long)pg->doc_info.num_pages) {
				rectangle		missing_page, vis_bounds;
				co_ordinate		repeat_offset;
				pg_short_t		page_index;
				long			old_page;
				
				old_page = (pg->doc_info.num_pages - 2) * (GetMemorySize(pg->wrap_area) - 1);
				
				pgShapeBounds(pg->vis_area, &vis_bounds);
				page_index = pgGetWrapRect(pg, old_page, &repeat_offset);
				GetMemoryRecord(pg->wrap_area, (long)page_index + 1, &missing_page);
				pgSubPt(&pg->scroll_pos, &repeat_offset);
				pgAddPt(&pg->port.origin, &repeat_offset);
				vis_bounds.top_left.v = missing_page.bot_right.v + repeat_offset.v;

				if (vis_bounds.top_left.v < vis_bounds.bot_right.v) {
					
					pg->procs.set_device(pg, set_pg_device, &pg->port, &pg->bk_color);
					pgEraseRect(pg->globals, &vis_bounds, &pg->scale_factor, NULL);
					pg->procs.set_device(pg, unset_pg_device, &pg->port, NULL);
				}
			}
			
			pgDrawPages(pg, NULL, NULL, MEM_NULL, draw_mode);
		}
	}
}


/* This returns TRUE if any char in text is a control type char. The text is
assumed to all be the current insertion style. */

static pg_boolean has_any_controls (paige_rec_ptr pg, pg_char_ptr text, size_t length)
{
	register long			byte_ctr;

	for (byte_ctr = 0; byte_ctr < length; ++byte_ctr)
		if (special_action_key(pg, -1, &text[byte_ctr]))
			return	TRUE;
	
	return	FALSE;
}


/* This function scales the elements in draw_position per scale factor of pg. */

static void scale_drawing (paige_rec_ptr pg, co_ordinate_ptr amount_offset,
		draw_points_ptr draw_position)
{
	if (pg->scale_factor.scale) {
		
		pgScalePt(&pg->scale_factor, amount_offset, &draw_position->from);
		pgScalePt(&pg->scale_factor, amount_offset, &draw_position->to);
		
		pgScaleLong(pg->scale_factor.scale, 0, &draw_position->descent);
		pgScaleLong(pg->scale_factor.scale, 0, &draw_position->ascent);
	}
}


/* This function returns TRUE if the line beginning at line_start plus all
subsequent lines (to end of doc) are outside of vis_rect. The purpose is to
prevent excessive pagination and/or display when it is no longer necessary. */

static pg_boolean can_quit_display (paige_rec_ptr pg, point_start_ptr line_start,
		rectangle_ptr vis_rect, co_ordinate_ptr offset_extra, rectangle_ptr wrap_base,
		pg_short_t wrap_qty)
{
	rectangle		check_rect;
	long			r_num, w_qty;
	
	r_num = line_start->r_num;

	if (pg->doc_info.attributes & (V_REPEAT_BIT | H_REPEAT_BIT)) {
		rectangle			page_bounds;
		co_ordinate			repeat_offset;
	
		pgShapeBounds(pg->wrap_area, &page_bounds);
		
		for (;;) {
			
			pgGetWrapRect(pg, r_num, &repeat_offset);
			pgAddPt(offset_extra, &repeat_offset);

			pgScaleRectToRect(&pg->port.scale, &page_bounds, &check_rect, &repeat_offset);
			
			if (pgSectRect(&check_rect, vis_rect, NULL))
				return	FALSE;
			
			if (check_rect.top_left.v >= vis_rect->bot_right.v)
				break;

			++r_num;
		}
	}
	else
	if (pg->doc_info.attributes & BOTTOM_FIXED_BIT) {
		
		w_qty = (long)wrap_qty;
		
		while (r_num < w_qty) {
			
			pgScaleRectToRect(&pg->port.scale, &wrap_base[r_num],
					&check_rect, offset_extra);
			
			if (pgSectRect(&check_rect, vis_rect, NULL))
				return	FALSE;

			++r_num;
		}
	}
	else {

		pgScaleRectToRect(&pg->port.scale, &line_start->bounds,
				&check_rect, offset_extra);

		if (pgSectRect(&check_rect, vis_rect, NULL))
			return	FALSE;
	}

	return	TRUE;
}


/* fix_non_linefeed_update fixes the situation where the last point_start of
the block about to be drawn has no CR terminator, which means the next block's
first line starts in the same vertical position!  */

static void fix_non_linefeed_update (paige_rec_ptr pg, text_block_ptr block,
		smart_update_ptr update_info)
{
	smart_update		aux_info;
	
	if ((block->flags & NO_CR_BREAK) && (update_info->suggest_end == block->end))
		if (block->end < pg->t_length)
			if (block[1].flags & NEEDS_CALC) {
		
		pgPaginateBlock(pg, &block[1], &aux_info, TRUE);
		update_info->suggest_end = aux_info.suggest_end;
		update_info->num_display_lines += aux_info.num_display_lines;
		update_info->num_display_starts += aux_info.num_display_starts;
		update_info->flags = 0;
	}
}


/* This function gets called when text has been inserted and will be displayed.
Its purpose is to set flag bits in update_info that will indicate the best
path to take for drawing, for example I might get away with drawing a single line
or even a single character according to the update info given. The block param
contains the text block just used for inserting, offset is the (absolute) offset,
lengh is the insertion length and update_info's data has been set by
pgInvalTextBlock.  */

static void set_fastest_drawing_path (paige_rec_ptr pg, text_block_ptr block,
		long offset, long length, smart_update_ptr update_info)
{
	register style_info_ptr	insert_style;
	register long			bytes_to_end, local_offset;
	pg_char_ptr				text;
	style_walk				walker;
	long					ending_offset, beginning_offset, ending_display_offset;
	long					end_of_block, paginated_flags;
	short					style_or, style_ctr;

	paginated_flags = update_info->flags;
	update_info->flags = 0;

	if (update_info->num_display_lines == 1) {
		
		update_info->flags = CAN_DRAW_1_LINE;
		
		if (paginated_flags & TAB_BREAK_BIT)
			return;

		pgPrepareStyleWalk(pg, offset, &walker, TRUE);
		insert_style = walker.cur_style;

		if (insert_style->char_extra || (insert_style->class_bits & NO_SMART_DRAW_BIT)
			|| (pg->flags & SHOW_INVIS_CHAR_BIT))
			style_or = TRUE;
		else
			for (style_ctr = style_or = 0; style_ctr < MAX_STYLES; ++style_ctr)
				if (style_or = insert_style->styles[style_ctr])
					break;
		
		if (!style_or) {
		
			ending_offset = update_info->suggest_end;
			ending_display_offset = offset + length;
			bytes_to_end = ending_offset - ending_display_offset;
			pgSetWalkStyle(&walker, ending_display_offset);
			
			if ((walker.cur_par_style->justification == justify_left)
				&& (pgPrimaryTextDirection(pg->globals, walker.cur_par_style) != right_left_direction)) {
				
				update_info->flags |= CAN_DRAW_PARTIAL_LINE;  /* Assume this is true */
				
				if (bytes_to_end > 0) {
	
					local_offset = ending_display_offset - block->begin;
					beginning_offset = local_offset;
					end_of_block = block->end - block->begin;
					
					text = (pg_char_ptr) UseMemory(block->text);
					
					while (bytes_to_end) {
						
						if (!walker.cur_style->procs.char_info(pg, &walker, text,
							block->begin, beginning_offset, end_of_block, local_offset, BLANK_BIT)) {
							
							update_info->flags = CAN_DRAW_1_LINE;
							break;
						}
						
						pgWalkStyle(&walker, 1);
						++local_offset;
						--bytes_to_end;
					}
					
					UnuseMemory(block->text);
				}
			}
		}

		pgPrepareStyleWalk(pg, 0, NULL, TRUE);
	}
}



/* This function gets called by pgDrawPages if draw_mode is "sp_erase_all_pages."
This effectively fills all pages with background color. */

static void fill_pages (paige_rec_ptr pg, shape_ptr page_shape,
		pg_short_t r_qty, pg_short_t r_num, co_ordinate_ptr offset)
{
	register pg_short_t		qty;
	co_ordinate				use_offset;
	shape_section			begin_r, end_r;
	shape_section_ptr		begin_r_ptr, end_r_ptr;

	if (offset)
		use_offset = *offset;
	else
		use_offset.h = use_offset.v = 0;

	if ((pg->doc_info.attributes & (COLOR_VIS_BIT | V_REPEAT_BIT)) == COLOR_VIS_BIT) {
	
		begin_r_ptr = end_r_ptr = NULL;
		
		if (offset)
			pgAddPt(&pg->scroll_pos, &use_offset);
	}
	else {

		begin_r.r_num = 0;
		begin_r.v_position = page_shape->top_left.v;
	
		qty = r_qty - 1;
		end_r.r_num = qty;
		end_r.v_position = page_shape[qty].bot_right.v;
		
		begin_r_ptr = &begin_r;
		end_r_ptr = &end_r;
		
		if (!(pg->doc_info.attributes & BOTTOM_FIXED_BIT))
			end_r_ptr = NULL;
	}

	pgEraseContainerArea(pg, &pg->scale_factor, &use_offset, begin_r_ptr, end_r_ptr);
}


/* special_action_key compares the byte in the_key against all the control codes
for arrow keys, backspace and forward delete and returns TRUE if any of those. */

static pg_boolean special_action_key (paige_rec_ptr pg, long will_insert_here, pg_char_ptr the_key)
{
	register pg_globals_ptr			globals_ptr;
	register pg_short_t				compare_key;
	
	globals_ptr = pg->globals;
	compare_key = (pg_short_t)*the_key;
	
	if (will_insert_here >= 0)
		if (compare_key == globals_ptr->tab_char || compare_key == globals_ptr->line_wrap_char)
			if (pgPositionInTable(pg, will_insert_here, NULL)) {
		
			if (compare_key == globals_ptr->tab_char) {
			
				*the_key = (pg_char)globals_ptr->container_brk_char;
				return	TRUE;
			}

			*the_key = (pg_char)globals_ptr->soft_line_char;
			return	FALSE;
	}

	return ( (compare_key == globals_ptr->bs_char)
			|| (compare_key == globals_ptr->left_arrow_char)
			|| (compare_key == globals_ptr->right_arrow_char)
			|| (compare_key == globals_ptr->up_arrow_char)
			|| (compare_key == globals_ptr->down_arrow_char)
			|| (compare_key == globals_ptr->fwd_delete_char));
}



/* This function draws a full text line beginning at first_start and continuing
to the ending start of the line. The function returns the number of point_start
records drawn (which will always be at least one). If the draw_mode is any of
the bitmap modes, multiple passes are made to the offscreen drawing, if
necessary, to accommodate bitmaps that would otherwise be too large.
The *actual_printed value gets incremented by 1 if the text line actually
drew. */


static long draw_line (paige_rec_ptr pg, text_block_ptr block, pg_char_ptr text,
		point_start_ptr first_start, style_walk_ptr walker, co_ordinate_ptr offset_extra,
		rectangle_ptr vis_rect, rectangle_ptr wrap_base, glitter_info_ptr glitter,
		long PG_FAR *actual_printed, graphic_line_ptr drawn_line_info, short draw_mode)
{
	register point_start_ptr			starts;
	register glitter_info_ptr			line_glitter;
	memory_ref							previous_line_ref;
	point_start_ptr						last_start, previous_start, previous_end;
	pg_globals_ptr						globals;
	rectangle_ptr						wrap_rect;
	par_info_ptr						par;
	rectangle							bounds, non_table_bounds, check_bounds, actual_target;
	draw_points							draw_position;
	co_ordinate							adjust_extra, full_extra;
	tab_stop							the_tab;
	long								block_offset, abs_offset, starts_ctr;
	long								ending_offset, skipped_starts, beginning_offset;
	long								extra_top, extra_bot, tab_ending, is_table;
	short								display_mode_verb, post_glitter_verb;
	pg_short_t							use_j_extra, text_offset, text_length;
	pg_short_t							draw_length, previous_block_flags, text_end;
	pg_boolean							bitmap_enable, bitmap_done;

	starts = first_start;

	line_glitter = glitter;
	globals = pg->globals;
	extra_top = extra_bot = DISPLAY_SLOP_EXTRA;

	if (block_offset = block->begin) {
		
		--block;
		previous_block_flags = block->flags;
		++block;
	}
	else
		previous_block_flags = 0;

	abs_offset = (long)starts->offset;
	abs_offset += block_offset;
	beginning_offset = abs_offset;
	starts_ctr = 0;
	bitmap_done = FALSE;
	text_end = (pg_short_t)(block->end - block->begin);
	
	pgSetWalkStyle(walker, abs_offset);
	par = walker->cur_par_style;

	if (!pg->active_subset)
		bitmap_enable = (draw_mode == bits_copy || draw_mode == bits_or || draw_mode == bits_xor);
	else
		bitmap_enable = FALSE;

	wrap_rect = wrap_base;
	wrap_rect += pgGetWrapRect(pg, starts->r_num, NULL);
	adjust_extra.h = adjust_extra.v = 0;

	pgFillBlock(&draw_position, sizeof(draw_points), 0);
	draw_position.block = block;
	display_mode_verb = draw_mode;
	
	bounds = starts->bounds;
	
	if (drawn_line_info) {
	
		drawn_line_info->bounds = bounds;
		drawn_line_info->bounds.bot_right.v += (long)starts->cell_height;
		drawn_line_info->r_num = starts->r_num;
		drawn_line_info->cell_num = (starts->cell_num & CELL_NUM_MASK);
	}

	if (starts->offset || !(previous_block_flags & NO_CR_BREAK))
		bounds.top_left.h = wrap_rect->top_left.h;

	bounds.bot_right.h = wrap_rect->bot_right.h;
	pgOffsetRect(&bounds, offset_extra->h, offset_extra->v);

	non_table_bounds = bounds;
	
	if ((is_table = par->table.table_columns) != 0) {
		long			max_table_width;
		
		max_table_width = pgSectColumnBounds(pg, par, (long)((starts->cell_num & CELL_NUM_MASK) - 1), starts->r_num, &bounds);
		pgOffsetRect(&bounds, pgTableJustifyOffset(walker->cur_par_style, max_table_width), 0);
		bounds.bot_right.v += starts->cell_height;
	}
	
	check_bounds = bounds;
	pgScaleRect(&pg->scale_factor, offset_extra, &bounds);
#ifdef CHANDHOK
	// chandhok - fixes for 1.273 version of Paige
	pgOffsetRect(&check_bounds, -offset_extra->h, -offset_extra->v);
#else
	pgScaleRect(&pg->port.scale, offset_extra, &check_bounds);
	pgSectRect(&bounds, vis_rect, &bounds);
#endif
	
	++line_glitter->line_number;
	
	if (starts->flags & NEW_PAR_BIT) {
		
		pgSetWalkStyle(&line_glitter->walker, abs_offset);

		++line_glitter->par_number;
		line_glitter->par_begin = starts;
		extra_top += line_glitter->walker.cur_par_style->top_extra;
	}
	
	if (pgEmptyRect(&bounds))
		return	advance_to_next_line(starts);

	if (!pg->active_subset) {
	
		if (pg->flags & PRINT_MODE_BIT)
			if (check_bounds.top_left.v < vis_rect->top_left.v || check_bounds.bot_right.v > vis_rect->bot_right.v)
			return advance_to_next_line(starts);
		
		if (((check_bounds.top_left.v - extra_top) > vis_rect->bot_right.v) || ((check_bounds.bot_right.v + extra_bot) < vis_rect->top_left.v))
			return advance_to_next_line(starts);
	}

	if (!draw_mode) {

		skipped_starts = advance_to_next_line(starts);
		ending_offset = starts[skipped_starts].offset;
		ending_offset += block_offset;
		line_glitter->last_printed = ending_offset;

		return	skipped_starts;
	}

// Determine the previous line, if any:

	previous_line_ref = MEM_NULL;

	if (first_start->offset) {
		
		previous_start = first_start;
		--previous_start;
	}
	else {
		
		if (block->begin == 0)
			previous_start = NULL;
		else {
			
			--block;
			pgPaginateBlock(pg, block, NULL, TRUE);
			previous_line_ref = block->lines;
			++block;

			previous_start = (point_start_ptr) UseMemory(previous_line_ref);
			
			while (previous_start->flags != TERMINATOR_BITS)
				++previous_start;
			
			--previous_start;
		}
	}
	
	previous_end = previous_start;

	if (previous_start) {
		
		while (!(previous_start->flags & NEW_LINE_BIT))
			--previous_start;
	}

	if (bitmap_enable) {

		display_mode_verb = direct_or;
		
		if (is_table) {
			long			cell_num, column_span;
			
			cell_num = starts->cell_num & CELL_NUM_MASK;
			
			column_span = par->tabs[cell_num - 1].tab_type >> 24;
			
			if (column_span)
				column_span -= 1;
			
			if (cell_num == 1)
				if (non_table_bounds.top_left.h < bounds.top_left.h)
					bounds.top_left.h = non_table_bounds.top_left.h;

			if ((cell_num + column_span) == is_table)
				if (non_table_bounds.bot_right.h > bounds.bot_right.h)
					bounds.bot_right.h = non_table_bounds.bot_right.h;
		}
		
		pgPrepareOffscreen(pg, &bounds, &actual_target, &adjust_extra,
			beginning_offset, starts, draw_mode);
		
		draw_position.bitmap_exclude = pg->globals->offscreen_exclusion;
		draw_position.bitmap_offset = pg->globals->offscreen_target.top_left;
	}

	post_glitter_verb = glitter_normal_draw;

	while (!bitmap_done) {

		starts_ctr = 0;
		pgSetWalkStyle(walker, beginning_offset);
		walker->cur_style->procs.install(pg, walker->cur_style, walker->cur_font,
					&walker->superimpose, walker->style_overlay, TRUE);
		
		full_extra = *offset_extra;
		pgAddPt(&adjust_extra, &full_extra);
		last_start = first_start;
		
		while (!(last_start->flags & LINE_BREAK_BIT))
			++last_start;
		
		if (!(starts->flags & LINE_HIDDEN_BIT))
			line_glitter->walker.cur_par_style->procs.line_glitter(pg, walker,
					line_glitter->line_number, line_glitter->par_number, block,
					first_start, last_start, previous_start, previous_end,
					&full_extra, vis_rect, glitter_pre_draw);

		for (;;) {
	
			if (!(starts->flags & LINE_HIDDEN_BIT)) {
				
				if (actual_printed)
					*actual_printed += 1;
	
				abs_offset = (long)starts->offset;
				abs_offset += block_offset;
				draw_position.real_offset = abs_offset;
				
				if (pgSetWalkStyle(walker, abs_offset) || pg->scale_factor.scale)
					walker->cur_style->procs.install(pg, walker->cur_style, walker->cur_font,
									&walker->superimpose, walker->style_overlay, TRUE);
				
				text_offset = starts->offset;
				text_length = starts[1].offset - text_offset;				
				
				use_j_extra = (pg_short_t)pgGetJExtra(starts);
		
				draw_position.starts = starts;
				draw_position.from.h = starts->bounds.top_left.h;
				draw_position.from.v = starts->bounds.bot_right.v - starts->baseline;
				draw_position.to.h = starts->bounds.bot_right.h;
				draw_position.to.v = draw_position.from.v;
				draw_position.descent = starts->baseline;
				draw_position.ascent = draw_position.from.v - starts->bounds.top_left.v;
				draw_position.vis_offset = adjust_extra;

				pgAddPt(offset_extra, &draw_position.from);
				pgAddPt(offset_extra, &draw_position.to);
				pgAddPt(offset_extra, &draw_position.vis_offset);

				scale_drawing(pg, offset_extra, &draw_position);

				pgAddPt(&adjust_extra, &draw_position.from);
				pgAddPt(&adjust_extra, &draw_position.to);
				
				tab_ending = draw_position.to.h;

				if (is_subref_char(block, text, text_offset, text_end))
					pgDrawSubRef(pg, walker, &draw_position, text_offset, &draw_position.vis_offset);
				else
				if (draw_length = pgStripTrailingBlanks(pg, starts, walker, text,
						text_offset, text_length, use_j_extra))
					walker->cur_style->procs.draw(pg, walker, text, text_offset,
							draw_length, &draw_position, use_j_extra, display_mode_verb);
				else
					draw_position.to.h = draw_position.from.h;
				
				pg->procs.special_proc(pg, walker, text, text_offset,
					draw_length, &draw_position, use_j_extra, display_mode_verb);

				if (starts->flags & TAB_BREAK_BIT) {
		
					draw_position.from = draw_position.to;
					draw_position.to.h = tab_ending;

					if (starts->extra & EXTRA_IS_TAB)
						the_tab = par->tabs[starts->extra & J_EXTRA_MASK];
					else {
					
						pgFillBlock(&the_tab, sizeof(tab_stop), 0);
						the_tab.position = draw_position.to.h;
					}
					
					par->procs.tab_draw(pg, walker, &the_tab, &draw_position);
				}
			}
	
			++starts_ctr;
			
			if (starts->flags & LINE_BREAK_BIT)
				break;

			if (starts[1].flags == TERMINATOR_BITS)
				break;

			++starts;
		}

		ending_offset = starts[1].offset;
		ending_offset += block_offset;
		line_glitter->last_printed = ending_offset;
		
		last_start = starts;

		starts = first_start;

		if (bitmap_enable) {
			
			post_glitter_verb = glitter_post_bitmap_draw;
			
			line_glitter->walker.cur_par_style->procs.line_glitter(pg, walker,
					line_glitter->line_number, line_glitter->par_number, block,
					first_start, last_start, previous_start, previous_end,
					&full_extra, vis_rect, glitter_bitmap_draw);

			bitmap_done = pgFinishOffscreen(pg, beginning_offset, starts,
					&adjust_extra, &actual_target, draw_mode);
		}
		else
			bitmap_done = TRUE;
	}

	pgSubPt(&adjust_extra, &full_extra);

	if (!pg->active_subset)  // If this is not a subset of another line
		line_glitter->walker.cur_par_style->procs.line_glitter(pg, walker,
				line_glitter->line_number, line_glitter->par_number, block, first_start,
				last_start, previous_start, previous_end, &full_extra, vis_rect,
				post_glitter_verb);

	if (previous_line_ref)
		UnuseMemory(previous_line_ref);

	return	starts_ctr;
}


/* advance_to_next_line returns the number of point_starts to add to first_start
in order to advance to the next logical line. */

static long advance_to_next_line (point_start_ptr first_start)
{
	register point_start_ptr	starts;
	long						line_ctr;
	
	starts = first_start;

	if (starts->flags == TERMINATOR_BITS)
		return	0;

	for (line_ctr = 1; !(starts->flags & LINE_BREAK_BIT); ++line_ctr, ++starts)
		if (starts[1].flags == TERMINATOR_BITS)
			break;

	return	line_ctr;
}

/* set_caret sends verb to the cursor hook, but only if there is not a
selection range. */

static void set_caret (paige_rec_ptr pg, short verb)
{
	if (!pg->num_selects) {
	
		pg->procs.cursor_proc(pg, (t_select_ptr) UseMemory(pg->select), verb);
		UnuseMemory(pg->select);
	}
}


/* delete_style_records removes a first_rec style run record for num_recs, decrementing
the "usage counter" for the associated style info. */

static void delete_style_records (paige_rec_ptr pg, long first_rec, long num_recs)
{
	style_info_ptr			style_base;
	style_run_ptr			run;
	pg_short_t				index;
	long					run_qty;
	
	style_base = (style_info_ptr) UseMemory(pg->t_formats);
	run = (style_run_ptr) UseMemoryRecord(pg->t_style_run, first_rec, USE_ALL_RECS, TRUE);
	
	for (run_qty = 0; run_qty < num_recs; ++run, ++run_qty) {
		
		index = run->style_item;
		style_base[index].used_ctr -= 1;
	}
	
	UnuseMemory(pg->t_formats);
	UnuseMemory(pg->t_style_run);
	
	DeleteMemory(pg->t_style_run, first_rec, num_recs);
}


/* delete_style_records removes a first_rec style run record for num_recs, decrementing
the "usage counter" for the associated par info. */

static void delete_par_records (paige_rec_ptr pg, long first_rec, long num_recs)
{
	par_info_ptr			style_base;
	style_run_ptr			run;
	pg_short_t				index;
	long					run_qty;
	
	style_base = (par_info_ptr) UseMemory(pg->par_formats);
	run = (style_run_ptr) UseMemoryRecord(pg->par_style_run, first_rec, USE_ALL_RECS, TRUE);
	
	for (run_qty = 0; run_qty < num_recs; ++run, ++run_qty) {
		
		index = run->style_item;
		style_base[index].used_ctr -= 1;
	}
	
	UnuseMemory(pg->par_formats);
	UnuseMemory(pg->par_style_run);
	
	DeleteMemory(pg->par_style_run, first_rec, num_recs);
}


/* delete_par_exclusions removes any paragraph exclusions in the run which fall into
the specified range. */

static void delete_par_exclusions (paige_rec_ptr pg, long position, long length)
{
	style_run_ptr			run;
	long					num_par_exclusions, remaining, par_index;
	long					end_delete;

	if ((run = pgFindParExclusionRun(pg, position, &remaining)) != NULL) {
		
		num_par_exclusions = GetMemorySize(pg->par_exclusions);
		par_index = num_par_exclusions - remaining;
		end_delete = position + length - 1;

		while (par_index < num_par_exclusions) {
			
			if (run->offset > end_delete)
				break;
			
			UnuseMemory(pg->par_exclusions);
			DeleteMemory(pg->par_exclusions, par_index, 1);
			run = (style_run_ptr) UseMemory(pg->par_exclusions);
			run += par_index;
			num_par_exclusions -= 1;
		}

		UnuseMemory(pg->par_exclusions);
	}
}

/* is_subref_char returns TRUE if a valid subref char is in text[offset]. */

static pg_boolean is_subref_char (text_block_ptr block, pg_char_ptr text, pg_short_t offset,
					pg_short_t text_length)
{
	if (offset >= text_length)
		return	FALSE;
	
	if (text[offset] != SUBREF_CHAR)
		return	FALSE;

	return pgIsValidSubref(block, (long)offset);
}

