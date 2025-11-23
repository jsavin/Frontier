/* This file contains mandatory functions for default paragraph handling. This
library cannot be omitted from the basic Paige package. */

#include "Paige.h"

#ifdef MAC_PLATFORM
#pragma segment pgbasic3
#endif

#include "defProcs.h"
#include "machine.h"
#include "pgText.h"
#include "pgSelect.h"
#include "pgEdit.h"
#include "pgDefStl.h"
#include "pgUtils.h"
#include "pgShapes.h"
#include "pgScript.h"
#include "pgDefPar.h"
#include "pgSubRef.h"
#include "pgErrors.h"
#include "pgGrafx.h"
#include "pgTables.h"


static void find_word_boundary (paige_rec_ptr pg, size_t offset, long info_mask,
		size_t PG_FAR *begin, size_t PG_FAR *end, pg_boolean left_side);
static pg_short_t locate_closest_tab (style_walk_ptr walker, long left_base, long right_base,
			long cur_base, tab_stop_ptr tab_result);
static short encode_j_extra (long j_extra);
static long lines_on_different_page (text_block_ptr block, select_pair_ptr range);
static short top_page_this_block (paige_rec_ptr pg, text_block_ptr block,
		select_pair_ptr range);
static short bottom_page_this_block (paige_rec_ptr pg, text_block_ptr block,
		select_pair_ptr range);
static pg_boolean first_line_on_page (paige_rec_ptr pg, text_block_ptr block, point_start_ptr starts);
static long adjust_for_multibyte (paige_rec_ptr pg, style_walk_ptr walker, pg_char_ptr text,
		long block_begin, long offset_begin, long offset_end, long char_offset,
		short increment_direction);
static pg_boolean is_real_text (paige_rec_ptr pg, size_t offset_to_test);
static long compute_overflow_size (paige_rec_ptr pg, text_block_ptr blocks, long bottom_side);
static long get_cell_bottom (par_info_ptr par);


/* DEFAULT DUPLICATE PAR FORMAT PROC. By default, this duplicate a tab_ref if
one exists. WARNING: pg can be NULL if called from Undo functions!  */

PG_PASCAL (void) pgDupParProc (paige_rec_ptr src_pg, paige_rec_ptr target_pg,
		short reason_verb, par_ref all_pars, par_info_ptr par_style)
{
#ifdef MAC_PLATFORM
#pragma unused (src_pg, target_pg, reason_verb, all_pars, par_style)
#endif
}


#ifdef PG_DEBUG

/* pgCheckRunIntegrity is used only in debug mode. It checks the style and par runs for integrity
after one or more have been deleted or inserted. */

PG_PASCAL (void) pgCheckRunIntegrity (paige_rec_ptr pg)
{
	memory_ref				bad_ref = MEM_NULL;
	register style_info_ptr	style_base;
	register par_info_ptr	par_base;
	style_run_ptr			run;
	long					run_qty;
	pg_short_t				index, style_qty;
	pg_error				error = NO_ERROR;

	style_base = UseMemory(pg->t_formats);
	style_qty = (pg_short_t)GetMemorySize(pg->t_formats);
	run_qty = GetMemorySize(pg->t_style_run) - 1;
	run = UseMemory(pg->t_style_run);
	
	bad_ref = pg->t_style_run;

	while (run_qty) {
		
		index = run->style_item;
		
		if (index >= style_qty) {
			
			error = STRUCT_INTEGRITY_ERR;
			break;
		}
		
		if (style_base[index].used_ctr <= 0) {

			error = STRUCT_INTEGRITY_ERR;
			break;
		}
		
		++run;
		--run_qty;
	}

	UnuseMemory(pg->t_style_run);
	UnuseMemory(pg->t_formats);
	
	if (!error) {

		par_base = UseMemory(pg->par_formats);
		style_qty = (pg_short_t)GetMemorySize(pg->par_formats);
		run_qty = GetMemorySize(pg->par_style_run) - 1;
		run = UseMemory(pg->par_style_run);
		
		bad_ref = pg->par_style_run;
	
		while (run_qty) {
			
			index = run->style_item;
			
			if (index >= style_qty) {
				
				error = STRUCT_INTEGRITY_ERR;
				break;
			}
			
			if (par_base[index].used_ctr <= 0) {
	
				error = STRUCT_INTEGRITY_ERR;
				break;
			}
			
			++run;
			--run_qty;
		}
	
		UnuseMemory(pg->par_style_run);
		UnuseMemory(pg->par_formats);
	}
}


#endif



/* pgPostPaginateProc is the default "post(after) paginate" function. The
default checks if paragraphs should be moved around after they have changed.
If necessary the blocks are re-paginated to accommodate the new placements. */

PG_PASCAL (void) pgPostPaginateProc (paige_rec_ptr pg, text_block_ptr block,
		smart_update_ptr update_info, long lineshift_begin, long lineshift_end,
		short action_taken_verb)
{
	select_pair						tied_range;
	register text_block_ptr			back_blocks;
	short							line_count;

	if (pg->active_subset) {	// This is only a subset of the PAIGE doc
		
		pgUpdateSubRefBounds(pg, block);
			
		return;
	}

	if (!(pg->doc_info.attributes & V_REPEAT_BIT)) {
		
		if (pg->t_length && (pg->flags2 & CHECK_PAGE_OVERFLOW) && (pg->doc_info.attributes & BOTTOM_FIXED_BIT)) {
			rectangle		page_bounds;

			back_blocks = (text_block_ptr) UseMemoryRecord(pg->t_blocks, GetMemorySize(pg->t_blocks) - 1, USE_ALL_RECS, FALSE);
			pgShapeBounds(pg->wrap_area, &page_bounds);

			if (!(back_blocks->flags & (NEEDS_CALC | NEEDS_PAGINATE))) {

				if (back_blocks->bounds.bot_right.v > page_bounds.bot_right.v) {
					
					while ((back_blocks->begin > 0) && (back_blocks->bounds.top_left.v > page_bounds.bot_right.v))
						back_blocks -= 1;
					
					if ((pg->overflow_size = compute_overflow_size(pg, back_blocks,
								page_bounds.bot_right.v)) == 0)
						pg->overflow_size = -1;
				}
				else
					pg->overflow_size = 0;
			}
		}

		return;
	}

	if (pg->forced_break || lineshift_begin == lineshift_end)
		return;

	if (block->flags & ALL_TEXT_HIDDEN)
		return;

	tied_range.begin = block->begin;
	tied_range.end = block->end;
	
	while (pgTiedParagraphRange(pg, &tied_range)) {
		long			forced_break;

		if ((forced_break = lines_on_different_page(block, &tied_range)) >= 0) {
			
			pg->forced_break = forced_break;
			block->flags |= NEEDS_PAGINATE;
			back_blocks = block;
	
			while (back_blocks->begin > tied_range.begin) {
				
				--back_blocks;
				back_blocks->flags |= NEEDS_PAGINATE;
			}

			pgPaginateBlock(pg, block, update_info, TRUE);
			pg->forced_break = 0;
			
			if (update_info)
				update_info->suggest_begin = back_blocks->begin;
		}

		tied_range.begin = tied_range.end;
		
		if (tied_range.begin >= block->end)
			break;
	}
	
// Now check for widows and orphans:
	
	if (pg->doc_info.minimum_orphan) {
		
		tied_range.begin = block->begin;
		tied_range.end = block->end;

		for (;;) {
		
			line_count = top_page_this_block(pg, block, &tied_range);
			
			if (line_count == 0)
				break;
			
			if ((line_count < pg->doc_info.minimum_orphan) && tied_range.begin) {
				
				pg->forced_break = tied_range.begin;
				block->flags |= NEEDS_PAGINATE;
				pgPaginateBlock(pg, block, update_info, TRUE);
				pg->forced_break = 0;

				if (update_info)
					update_info->suggest_begin = block->begin;
			}
			
			tied_range.begin = tied_range.end;
			
			if (tied_range.begin >= block->end)
				break;
		}
	}

	if (pg->doc_info.minimum_widow) {
		
		tied_range.begin = block->begin;
		tied_range.end = block->end;

		for (;;) {
		
			line_count = bottom_page_this_block(pg, block, &tied_range);
			
			if (line_count == 0)
				break;
			
			if ((line_count < pg->doc_info.minimum_widow) && tied_range.begin) {
				
				pg->forced_break = tied_range.begin;
				block->flags |= NEEDS_PAGINATE;
				pgPaginateBlock(pg, block, update_info, TRUE);
				pg->forced_break = 0;

				if (update_info)
					update_info->suggest_begin = block->begin;
			}
			
			tied_range.begin = tied_range.end;
			
			if (tied_range.begin >= block->end)
				break;
		}
	}
}


/* DEFAULT DELETE PAR FORMAT PROC.By default, this checks to see if a set of
tabs should be deleted. WARNING: pg can be NULL if called from Undo functions! */

PG_PASCAL (void) pgDeleteParProc (paige_rec_ptr pg, short reason_verb,
		par_ref all_pars, par_info_ptr par_style)
{
#ifdef MAC_PLATFORM
#pragma unused (pg, reason_verb, all_pars, par_style)
#endif

}

/* pgAlterParProc gives app a chance to make last-minute changes to the
paragraph record before it is altered. */

PG_PASCAL (void) pgAlterParProc (paige_rec_ptr pg, par_info_ptr old_par,
		par_info_ptr new_par)
{
#ifdef MAC_PLATFORM
#pragma unused(pg, old_par, new_par)
#endif
}


/* pgBoundaryProc gets called to locate the boundaries for a paragraph. The
offset of concern is in boundary->begin, on return the two offset pairs must
be set in boundary->begin and boundary->end.
CHANGED 9/14/95 by Gar, if boundary is NULL then return TRUE if current insertion
is the beginning of a new paragraph. Otherwise do usual boundary check and return
TRUE if beginning offset requested is the beginning of a new par. */

PG_PASCAL (pg_boolean) pgParBoundaryProc (paige_rec_ptr pg, select_pair_ptr boundary)
{
	register pg_char_ptr		text;
	register size_t				local_offset;
	text_block_ptr				block, starting_block;
	size_t						offset, starting_offset;
	pg_boolean					valid_cr, starts_on_par;
	pg_char						cr_char;

	cr_char = (pg_char)pg->globals->line_wrap_char;

	if (boundary) {
 
 		if (boundary->begin > pg->t_length)
			boundary->begin = pg->t_length;

		offset = boundary->begin;
		
		block = starting_block = pgFindTextBlock(pg, offset, NULL, FALSE, TRUE);
		local_offset = starting_offset = offset - block->begin;
		text = (pg_char_ptr) UseMemory(block->text);
		text += local_offset;
	
	/* Locate beginning of paragraph */
		
		valid_cr = FALSE;
	
		for (;;) {
			
			while (local_offset) {
				
				if (valid_cr = (pg_boolean)(*(--text) == cr_char))
					break;
	
				--local_offset;
			}
			
			valid_cr &= is_real_text(pg, block->begin + local_offset);

			if (valid_cr)
				break;
			
			if (!local_offset) {
				
				if (!block->begin)
					break;
				
				UnuseMemory(block->text);
	
				--block;
				pg->procs.load_proc(pg, block);

				local_offset = block->end - block->begin;
				text = (pg_char_ptr) UseMemory(block->text);
				text += local_offset;
			}
			else
				--local_offset;
		}
		
		starts_on_par = (pg_boolean)(local_offset == starting_offset);

		boundary->begin = local_offset + block->begin;
		UnuseMemory(block->text);

	/* Now locate ending par offset. */
	
		block = starting_block;
		pg->procs.load_proc(pg, block);

		text = (pg_char_ptr) UseMemory(block->text);
		text += starting_offset;
		
		valid_cr = FALSE;
	
		for (;;) {
		
			while (offset < block->end) {
				
				++offset;
				if (valid_cr = (*text++ == cr_char))
					break;
			}
	
			valid_cr &= is_real_text(pg, offset);
			
			if (valid_cr)
				break;

			if (offset == block->end) {
				
				if (block->end == pg->t_length)
					break;
				
				UnuseMemory(block->text);
				++block;
				pg->procs.load_proc(pg, block);
				text = (pg_char_ptr) UseMemory(block->text);
			}
		}
	
		boundary->end = offset;

		UnuseMemory(block->text);
		UnuseMemory(pg->t_blocks);
	
	}
	else {
		
		starts_on_par = TRUE;	// Default is starts on a par boundary
		
		if ((offset = pgCurrentInsertion(pg)) > 0) {
			
			offset -= 1;
			block = pgFindTextBlock(pg, offset, NULL, FALSE, TRUE);
			local_offset = offset - block->begin;
			text = (pg_char_ptr) UseMemory(block->text);
			
			if (text[local_offset] != cr_char)
				starts_on_par = FALSE;
			else
				starts_on_par &= is_real_text(pg, offset);
			
			UnuseMemory(block->text);
			UnuseMemory(pg->t_blocks);
		}
	}

	return	starts_on_par;
}


/* The default proc for measuring a TAB.  The result must be the new position
(which will be a value >= cur_pos).    */

PG_PASCAL (long) pgTabMeasureProc (paige_rec_ptr pg, style_walk_ptr walker, long cur_pos,
		long cur_text_pos, long line_left, long line_right, pg_char_ptr text, pg_short_t text_length,
		long PG_FAR *char_positions, pg_short_t PG_FAR *tab_rec_info)
{
	tab_stop				best_tab;
	long					left_base;
	register pg_short_t		remaining_text, non_left_tab_ctr;
	
	if (walker->cur_par_style->table.table_columns)
		return	cur_pos;

	if (pg->flags & TABS_ARE_WIDTHS_BIT)
		return	cur_pos + walker->cur_par_style->def_tab_space;

	left_base = pgGetLeftTabBase(pg, line_left);

	*tab_rec_info = locate_closest_tab(walker, left_base, line_right, cur_pos + 1, &best_tab);

	if ((best_tab.tab_type & TAB_TYPE_MASK) != left_tab) {
		style_walk			temp_walker;
		long				position_extra, will_be_position;
		long				c_info;
		
		pgBlockMove(walker, &temp_walker, sizeof(style_walk));
		pgSetWalkStyle(&temp_walker, cur_text_pos);

		for (remaining_text = text_length, non_left_tab_ctr = 0; remaining_text;
				++non_left_tab_ctr, --remaining_text) {			
			
			c_info = temp_walker.cur_style->procs.char_info(pg, &temp_walker, text,
				NO_BLOCK_OFFSET, 0, text_length, non_left_tab_ctr, CTL_BIT | TAB_BIT | DECIMAL_CHAR_BIT);

			if (c_info & (CTL_BIT | TAB_BIT))
				break;

			if (((best_tab.tab_type & TAB_TYPE_MASK) == decimal_tab) && (c_info & DECIMAL_CHAR_BIT))
				break;
			
			pgWalkStyle(&temp_walker, 1);
		}
		
		if (non_left_tab_ctr) {
			
			position_extra = char_positions[non_left_tab_ctr] - *char_positions;
			
			if ((best_tab.tab_type & TAB_TYPE_MASK) == center_tab)
				position_extra /= 2;
			
			will_be_position = best_tab.position + left_base - position_extra;
			
			if (will_be_position < cur_pos)
				return	cur_pos;
			
			return	will_be_position;
		}
	}

	return	best_tab.position + left_base;
}


/* DEFAULT LINE ADJUST FUNCTION. This is the internal hook used to adjust
a line after calculation. You can tap into this one or leave the default. The
default figures out the following: (1) Justification, (2) Top/bottom offsets,
if any, for paragraph before/after stuff, (3) Determines if point_starts need
to move around due to right-left direction(s).     */

PG_PASCAL (void) pgLineAdjustProc (paige_rec_ptr pg, pg_measure_ptr measure_info,
		point_start_ptr line_starts, pg_short_t num_starts,
		rectangle_ptr line_fit, par_info_ptr par_format)
{
	register point_start_ptr	starts, last_start;
	register par_info_ptr		par_stuff;
	register long				add_extra;
	register pg_short_t			start_ctr;
	text_block_ptr				block;
	pg_short_t					first_start_flags;
	long 						top_extra, bot_extra, abs_end, abs_begin;
	long						top_frame_extra, bot_frame_extra;
	short						justification;

	par_stuff = par_format;
	justification = par_stuff->justification;
	
	if (line_starts->cell_num)
		justification = (short)measure_info->cell_alignment;

	starts = last_start = line_starts;
	block = measure_info->block;

	while (!(starts->flags & NEW_LINE_BIT)) {
		
		if (!starts->offset)
			break;
		
		--starts;
	}
	
	first_start_flags = starts->flags;
	abs_begin = (long)starts->offset;
	abs_begin += block->begin;
	
	starts = line_starts;

	last_start += (num_starts - 1);
	abs_end = last_start[1].offset;
	abs_end += block->begin;

	top_extra = bot_extra = top_frame_extra = bot_frame_extra = 0;
	measure_info->adjusted_descent = measure_info->adjusted_height = 0;

	if (measure_info->cell_qty <= 0) {

		if (par_stuff->spacing) {
			long		line_height, height_fract, space_integer;
			
			line_height = starts->bounds.bot_right.v - starts->bounds.top_left.v;
			height_fract = line_height << 16;
			space_integer = pgHiWord(pgMultiplyFixed(height_fract, par_stuff->spacing));
			space_integer -= line_height;
			top_extra += space_integer;
		}

		if (par_stuff->table.border_info || par_stuff->table.table_columns) {
			rectangle		border_sizes;
			long			top_pen, bottom_pen, original_top, original_bottom;

			if (pgGetBorderInfo(pg, block, line_starts, NULL, &border_sizes, NULL, NULL, &top_frame_extra, &bot_frame_extra)) {
				
				top_pen = original_top = border_sizes.top_left.v & PG_BORDER_PENSIZE;

				if (border_sizes.top_left.v & PG_BORDER_DOUBLE)
					top_pen += (original_top + 1);

				bottom_pen = original_bottom = border_sizes.bot_right.v & PG_BORDER_PENSIZE;

				if (border_sizes.bot_right.v & PG_BORDER_DOUBLE)
					bottom_pen += (original_bottom + 1);

				if (border_sizes.bot_right.v & PG_BORDER_SHADOW)
					bottom_pen += original_bottom;
				
				top_frame_extra += top_pen;
				bot_frame_extra += bottom_pen;
			}
		}

		if (first_start_flags & NEW_PAR_BIT)
			if (par_stuff->top_extra)
				top_extra += par_stuff->top_extra;

		if (last_start->flags & PAR_BREAK_BIT)
			bot_extra += par_stuff->bot_extra;
	}
	
	if (top_extra || bot_extra || top_frame_extra || bot_frame_extra) {
		
		add_extra = top_extra + bot_extra + top_frame_extra + bot_frame_extra;
		measure_info->adjusted_height += add_extra;
		measure_info->adjusted_descent += (bot_extra + bot_frame_extra);

		for (start_ctr = num_starts; start_ctr; ++starts, --start_ctr) {
			
			starts->bounds.bot_right.v += add_extra;
			starts->baseline += (short)(bot_extra + bot_frame_extra);
		}
		
		line_fit->bot_right.v += (add_extra);

		starts = line_starts;
	}
	
	if (/*measure_info->cell_qty >= 0 && */justification != justify_left) {
		long		amount_to_right;

		if ((amount_to_right = line_fit->bot_right.h - last_start->bounds.bot_right.h) < 0)
			if (par_stuff->justification != justify_full)
				amount_to_right = 0;

		switch (justification) {
			
			case justify_center:
				add_extra = amount_to_right / 2;
				break;
				
			case justify_right:
				add_extra = amount_to_right;
				break;

			case justify_full:
				if ((abs_end < pg->t_length)  && (!(last_start->flags
					& (PAR_BREAK_BIT | SOFT_PAR_BIT | BREAK_CONTAINER_BIT | BREAK_PAGE_BIT)))
					&& amount_to_right) {
				
					if (num_starts == 1) {
					
						starts->extra = encode_j_extra(amount_to_right);
						starts->bounds.bot_right.h += amount_to_right;
					}
					else {
						long		line_width, start_width, extra_pix;
						long		percent_of_width, percent_extra;
						long		slop_fixed, pixel_errors, real_extra;
						short		first_non_tab;
						pg_short_t	had_soft_hifen, has_soft_hifen;
						
						slop_fixed = amount_to_right << 16;
						pixel_errors = 0;
						extra_pix = 0;
						
						had_soft_hifen = FALSE;
						first_non_tab = 0;

						for (start_ctr = 0; start_ctr < num_starts; ++start_ctr)
							if (starts[start_ctr].flags & TAB_BREAK_BIT)
								first_non_tab = start_ctr + 1;

						if (first_non_tab < (short)num_starts) {
							
							starts += first_non_tab;
							num_starts -= first_non_tab;
							line_width = last_start->bounds.bot_right.h - starts->bounds.top_left.h;

							for (start_ctr = num_starts; start_ctr; ++starts, --start_ctr) {
	
								starts->bounds.top_left.h += (long) extra_pix;
								starts->bounds.bot_right.h += (long) extra_pix;
								has_soft_hifen = starts->flags & SOFT_BREAK_BIT;
								
								if (start_width = starts->bounds.bot_right.h - starts->bounds.top_left.h) {
									
									percent_of_width = pgFixedRatio((short)start_width, (short)line_width);
									percent_extra = pgMultiplyFixed(percent_of_width, slop_fixed);
									
									real_extra = (long) pgHiWord(percent_extra);
									
									if (had_soft_hifen && has_soft_hifen) {
									
										starts->extra = 0;
										real_extra = 0;
									}
									else {
									
										starts->extra = encode_j_extra(real_extra);
	
										if (percent_extra > 0)
											percent_extra &= 0x0000FFFF;
									}
									
									pixel_errors += percent_extra;
									
									if (!has_soft_hifen) {
									
										real_extra += (long)(pgHiWord(pixel_errors));
	
										if (pixel_errors > 0)
											pixel_errors &= 0x0000FFFF;
									}
									
									extra_pix += real_extra;
									had_soft_hifen = starts->flags & SOFT_BREAK_BIT;
									
									if (starts->flags & LINE_BREAK_BIT)
										starts->bounds.bot_right.h += real_extra;
								}
							}
						}
					}
				}

				add_extra = 0;
				break;

			case force_left:
			case force_right:
				add_extra = 0;
				break;
		}
		
		starts = line_starts;

		for (start_ctr = num_starts; start_ctr; ++starts, --start_ctr) {
			
			starts->bounds.top_left.h += add_extra;
			starts->bounds.bot_right.h += add_extra;
		}
	}
}

/* pgTiedParagraphRange scans the par styles within range and returns the first range
of paragraphs that should be tied together on the same page. If any are found the
function returns TRUE and that range is placed in range->begin and range->end. */

PG_PASCAL (pg_boolean) pgTiedParagraphRange (paige_rec_ptr pg, select_pair_ptr range)
{
	register par_info_ptr			par_base;
	register style_run_ptr			run;
	size_t							end_range;
	pg_short_t						index;
	pg_boolean						some_tied;

	run = pgFindParStyleRun(pg, range->begin, NULL);
	par_base = (par_info_ptr) UseMemory(pg->par_formats);
	end_range = range->end;
	some_tied = FALSE;
	
	for (;;) {

		index = run->style_item;
		
		if (!(par_base[index].class_info & KEEP_PARS_TOGETHER))
			break;

		range->begin = run->offset;
		
		if (run->offset == 0)
			break;
		
		--run;
	}

	while (run->offset < end_range) {
		
		index = run->style_item;
		
		if (some_tied) {

			if (!(par_base[index].class_info & KEEP_PARS_TOGETHER)) {
				
				range->end = run->offset;
				break;
			}
		}
		else
		if (par_base[index].class_info & KEEP_PARS_TOGETHER) {
			
			some_tied = TRUE;
			
			if (run->offset > range->begin)
				range->begin = run->offset;
		}
		
		++run;
	}
	
	UnuseMemory(pg->par_formats);
	UnuseMemory(pg->par_style_run);
	
	if (!some_tied)
		range->begin = range->end;

	return	some_tied;
}

/* pgGetBorderInfo returns the exact information required to draw four-sided borders.
The line_start indicates the first start of a line, upon return border_box is set to the
four sides (NON-scaled NON-scrolled). The values set in border_top_extra and border_bottom_extra
are the amounts for top and bottom above and beyond the actual (original) text line height,
but these values are not actually added or subracted from border_box.
If border info is for a table cell, *cell_info is filled with "border" information defining
the cell's position as follows: if PG_BORDER_LEFT is set then the cell is the left-most cell,
same for PG_BORDER_RIGHT.  If PG_BORDER_TOP is set then the cell is at the top of the table,
same for PG_BORDER_BOTTOM. */

PG_PASCAL (pg_boolean) pgGetBorderInfo (paige_rec_ptr pg, text_block_ptr block,
			point_start_ptr line_start, rectangle_ptr border_box, rectangle_ptr border_mask,
			long PG_FAR *shading, long PG_FAR *border_bits, long PG_FAR *border_top_extra,
			long PG_FAR *border_bottom_extra)
{
	register point_start_ptr	starts;
	par_info_ptr				par_base;
	par_info_ptr				par, previous_par, next_par;
	style_run_ptr				run, temp_run;
	pg_short_t					run_item;
	long						abs_offset, abs_offset_begin, abs_offset_end, border_info;
	long						border_shading, previous_border_info, next_border_info, full_border_info;
	pg_short_t					cell_index;
	pg_boolean					result = FALSE;
	pg_boolean					cr_top = FALSE;
	pg_boolean					cr_bottom = FALSE;

	par_base = (par_info_ptr) UseMemory(pg->par_formats);
	
	abs_offset = (long)line_start->offset;
	abs_offset += block->begin;
	run = pgFindParStyleRun(pg, abs_offset, NULL);
	run_item = run->style_item;
	par = &par_base[run_item];

	if (par->table.border_info != 0 || par->table.table_columns != 0 || par->table.border_shading) {
		
		border_info = border_shading = 0;
		
		if ((cell_index = (line_start->cell_num & CELL_NUM_MASK)) > 0) {
			
			border_info = par->tabs[cell_index - 1].leader;
			border_shading = par->tabs[cell_index - 1].ref_con;
		}

		if (!border_info)
			border_info = par->table.border_info;
		if (!border_shading)
			border_shading = par->table.border_shading;
		
		previous_border_info = next_border_info = 0;
		full_border_info = border_info;
		
		previous_par = next_par = par;

		starts = line_start;
		
		while (!(starts->flags & NEW_PAR_BIT) && starts->offset)
			--starts;
		
		if ((abs_offset_begin = (long)starts->offset) > 0)
			abs_offset_begin -= 1;

		temp_run = pgFindParStyleRun(pg, block->begin + abs_offset_begin, NULL);
		run_item = temp_run->style_item;
		previous_par = &par_base[run_item];
		UnuseMemory(pg->par_style_run);

		starts = line_start;

		while (!(starts->flags & LINE_BREAK_BIT))
			++starts;
		
		abs_offset_end = (long)starts[1].offset;
		abs_offset_end += block->begin;
		
		if (abs_offset_end == pg->t_length)
			cr_bottom = TRUE;
		else
			cr_bottom = (pg_boolean)((starts->flags & PAR_BREAK_BIT) != 0);

		temp_run = pgFindParStyleRun(pg, abs_offset_end, NULL);
		run_item = temp_run->style_item;
		next_par = &par_base[run_item];
		UnuseMemory(pg->par_style_run);

		// Determine if the previous line cancels this line's top border:

		if (cell_index) {  // A table
			long			previous_row_borders;
			
			if (previous_par->table.unique_id == par->table.unique_id) {
			
				previous_row_borders = previous_par->tabs[cell_index - 1].leader;
				
				if (!previous_row_borders)
					previous_row_borders = previous_par->table.border_info;
			}
			else
				previous_row_borders = 0;

			if (!(pg->flags & PRINT_MODE_BIT))
				border_info |= (par->table.cell_borders & (~border_info));

			if (cell_index < (pg_short_t)pgEffectiveColumns(par)) {
				long			test_left;
				
				if (!(test_left = par->tabs[cell_index].leader))
					test_left = par->table.border_info;
				
				if (test_left & PG_BORDER_LEFT)
					border_info &= (~PG_BORDER_RIGHT);
			}

			if (!(line_start->cell_num & CELL_TOP_BIT))
				border_info &= (~PG_BORDER_TOP);
			if (!(line_start->cell_num & CELL_BOT_BIT))
				border_info &= (~PG_BORDER_BOTTOM);

			if (!(line_start->cell_num & CELL_FIRST_BIT)) {
				
				if (previous_row_borders & PG_BORDER_BOTTOM)
					border_info &= (~PG_BORDER_TOP);
			}
		}
		else {
			
			cr_top = (pg_boolean)((line_start->flags & NEW_PAR_BIT) != 0);
			
			if (!cr_top || run->offset < abs_offset)
				border_info &= (~PG_BORDER_TOP);
			else {

				if (!previous_par->table.table_columns && abs_offset > 0) {
				
					if (previous_par->table.border_info & PG_BORDER_BOTTOM)
						border_info &= (~PG_BORDER_TOP);
					else
					if ((previous_par->table.border_info & PG_BORDER_TOP) == (border_info & PG_BORDER_TOP))
						border_info &= (~PG_BORDER_TOP);
				}
			}
			
			if (!cr_bottom)
				border_info &= (~PG_BORDER_BOTTOM);
			else
			if (abs_offset_end < pg->t_length) {
				
				if (run[1].offset > abs_offset_end)
					border_info &= (~PG_BORDER_BOTTOM);
				else {
	
					if (!previous_par->table.table_columns) {
					
						if (next_par->table.border_info & PG_BORDER_TOP)
							border_info &= (~PG_BORDER_BOTTOM);
						else
						if ((next_par->table.border_info & PG_BORDER_BOTTOM) == (border_info & PG_BORDER_BOTTOM))
							border_info &= (~PG_BORDER_BOTTOM);
					}
				}
			}
		}

		if (border_info || border_shading) {
			rectangle		border_values;
			long			border_right, border_bottom;
			
			result = TRUE;

			if (shading)
				*shading = border_shading;
			if (border_bits)
				*border_bits = border_info;

			border_right = ((border_info & PG_BORDER_RIGHT) >> 8);
			border_bottom = (border_info & PG_BORDER_BOTTOM) >> 24;

			border_values.top_left.h = (border_info & PG_BORDER_LEFT);
			border_values.top_left.v = (border_info & PG_BORDER_TOP) >> 16;
			border_values.bot_right.h = border_right;
			border_values.bot_right.v = border_bottom;

			if (border_mask)
				*border_mask = border_values;
			
			if (border_top_extra) {
				
				if (!(border_info & PG_BORDER_TOP))
					*border_top_extra = 0;
				else
					*border_top_extra = par->table.border_spacing;
			}

			if (border_bottom_extra) {
				
				if (!(border_info & PG_BORDER_BOTTOM))
					*border_bottom_extra = 0;
				else
					*border_bottom_extra = par->table.border_spacing;
			}

			if (border_box) {
				pg_short_t		r_num;

				r_num = pgGetWrapRect(pg, line_start->r_num, NULL);
				GetMemoryRecord(pg->wrap_area, (long)(r_num + 1), border_box);

				border_box->top_left.v = line_start->bounds.top_left.v;
				border_box->bot_right.v = line_start->bounds.bot_right.v + (long)line_start->cell_height;

				if (cell_index) {
					long		max_table_width;
					
					max_table_width = pgSectColumnBounds(pg, par, (long)(cell_index - 1), line_start->r_num, border_box);
					pgOffsetRect(border_box, pgTableJustifyOffset(par, max_table_width), 0);
				}
				else {
					rectangle		extra_indents;
					size_t			page_num;
					
					border_box->top_left.h += par->indents.left_indent;
					border_box->bot_right.h -= par->indents.right_indent;
					
					if (par->indents.first_indent < 0)
						border_box->top_left.h += par->indents.first_indent;

					if (pg->doc_info.attributes & USE_MARGINS_BIT)
						extra_indents = pg->doc_info.margins;
					else
						pgFillBlock(&extra_indents, sizeof(rectangle), 0);
					
					page_num = (line_start->r_num / (GetMemorySize(pg->wrap_area) - 1)) + 1;
					pg->procs.page_modify(pg, page_num, &extra_indents);
					border_box->top_left.h += extra_indents.top_left.h;
					border_box->bot_right.h -= extra_indents.bot_right.h;

					if (border_info & PG_BORDER_TOP)
						border_box->top_left.v += par->top_extra;
						
					if (border_info & PG_BORDER_BOTTOM)
						border_box->bot_right.v -= par->bot_extra;
				}
				
				if (border_shading && !border_info) {
					
					if (cr_top)
						border_box->top_left.v += par->top_extra;
					if (cr_bottom)
						border_box->bot_right.v -= par->bot_extra;
				}

				if (!(full_border_info & PG_BORDER_ALL3D)) {
				
					border_box->bot_right.v -= (border_bottom & PG_BORDER_PENSIZE);
					border_box->bot_right.h -= (border_right & PG_BORDER_PENSIZE);
				}
			}
		}
	}
	
	UnuseMemory(pg->par_style_run);
	UnuseMemory(pg->par_formats);

	return	result;
}


/* pgHalfPenSize returns 1/2 of a border pensize, rounding off to odd pixel values. */

PG_PASCAL (void) pgHalfPenSize (long PG_FAR *pensize)
{
	long		result;
	
	result = *pensize >> 1;
	
	if (*pensize & 1)
		++result;
	
	*pensize = result;
}


/* pgFindWord returns the byte offset of a word that encloses the offset given.
The word is what would invert if it were selected (highlighted). If left_side is
TRUE the word to the left is chosen if the offset is in between "real" words.
If smart_select is TRUE the blank(s) are selected to the right side of the word. */

PG_PASCAL (void) pgFindWord (pg_ref pg, size_t offset, size_t PG_FAR *first_byte,
		size_t PG_FAR *last_byte, pg_boolean left_side, pg_boolean smart_select)
{
	paige_rec_ptr				pg_rec;
	long						info_mask;
	
	pg_rec = (paige_rec_ptr) UseMemory(pg);
	
	info_mask = WORD_SEL_BIT;
	if (smart_select)
		info_mask |= BLANK_BIT;

	find_word_boundary(pg_rec, pgFixOffset(pg_rec, offset), info_mask, first_byte, last_byte, left_side);
	
	UnuseMemory(pg);
}


/* pgFindCtlWord locates a "word" seperated only by control chars. The "word"
offsets returned will exclude the left control char but will include the right.
If left_side is TRUE the word to the left is chosen if the offset is in between
"real" words.  */

PG_PASCAL (void) pgFindCtlWord (pg_ref pg, size_t offset, size_t PG_FAR *first_byte,
		size_t PG_FAR *last_byte, short left_side)
{
	paige_rec_ptr				pg_rec;
	
	pg_rec = (paige_rec_ptr) UseMemory(pg);

	find_word_boundary(pg_rec, pgFixOffset(pg_rec, offset), CTL_BIT, first_byte, last_byte, left_side);
	
	UnuseMemory(pg);	
}


/* pgFindPar returns the byte offset of a paragraph that encloses the offset given.
The paragraph is what would invert if it were selected (highlighted).  */

PG_PASCAL (void) pgFindPar (pg_ref pg, size_t offset, size_t PG_FAR *first_byte,
		size_t PG_FAR *last_byte)
{
	paige_rec_ptr				pg_rec;
	select_pair					boundary;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	boundary.begin = offset;
	pg_rec->procs.boundary_proc(pg_rec, &boundary);
	
	if (first_byte)
		*first_byte = boundary.begin;
	if (last_byte)
		*last_byte = boundary.end;

	UnuseMemory(pg);
}


/* pgFindLine returns the byte offset of a line that encloses the offset given.
The line is what would invert if it were selected (highlighted).  */

PG_PASCAL (void) pgFindLine (pg_ref pg, size_t offset, size_t PG_FAR *first_byte,
		size_t PG_FAR *last_byte)
{
	paige_rec_ptr				pg_rec;
	text_block_ptr				block;
	point_start_ptr				starts;
	size_t						use_offset;
	pg_short_t					local_begin, local_end, first_start;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
 	pg_rec->procs.set_device(pg_rec, set_pg_device, &pg_rec->port, &pg_rec->bk_color);

	use_offset = pgFixOffset(pg_rec, offset);

	block = pgFindTextBlock(pg_rec, use_offset, NULL, TRUE, TRUE);
	local_begin = (pg_short_t)(use_offset - block->begin);
	
	for (starts = (point_start_ptr) UseMemory(block->lines), first_start = 0; starts->offset < local_begin;
			++starts, ++first_start) ;
	
	if (starts->offset > local_begin)
		--first_start;

	UnuseMemory(block->lines);

	pgLineBoundary(pg_rec, block, &first_start, &local_begin, &local_end);
	
	*first_byte = block->begin + local_begin;
	*last_byte = block->begin + local_end;

	UnuseMemory(pg_rec->t_blocks);

 	pg_rec->procs.set_device(pg_rec, unset_pg_device, &pg_rec->port, NULL);

	UnuseMemory(pg);
}



/* pgGetLeftTabBase returns the left tab base to be used for getting an actual
tab position.  */

PG_PASCAL (long) pgGetLeftTabBase (paige_rec_ptr pg, long line_left)
{
	rectangle		page_margins;
	long			left_base;

	if ((left_base = pg->tab_base) < 0) {
		rectangle		wrap_bounds;

		if (left_base == TAB_BOUNDS_RELATIVE) {
			GetMemoryRecord(pg->wrap_area, 0, &wrap_bounds);
			left_base = wrap_bounds.top_left.h;
		}
		else
			left_base = line_left;
	}

	if (pg->doc_info.attributes & USE_MARGINS_BIT)
		page_margins = pg->doc_info.margins;
	else
		pgFillBlock(&page_margins, sizeof(rectangle), 0);

	pg->procs.page_modify(pg, 0, &page_margins);
	
	return	left_base + page_margins.top_left.h;
}


/* pgGetJExtra returns the amount of extra pixel slop to use for line_start. */

PG_PASCAL (long) pgGetJExtra (point_start_ptr line_start)
{
	long		extra;
	
	if ((pg_short_t)(line_start->extra & EXTRA_NOT_EXTRA))
		return	0;

	if (extra = (long) line_start->extra & J_EXTRA_MASK)
		if (line_start->extra & J_EXTRA_NEGATIVE)
			extra = -extra;

	return	extra;
}


/* pgScaleParInfo scales a par_info record based on numerator over denominator. */

PG_PASCAL (void) pgScaleParInfo (paige_rec_ptr pg, par_info_ptr par, short numerator, short denominator)
{
	pg_fixed			scale;
	tab_stop_ptr		tabs;
	long				origin, tab_qty;
	
	scale = numerator;
	scale <<= 16;
	scale |= denominator;
	origin = pg->port.origin.h;

	pgScaleLong(scale, origin, &par->indents.left_indent);
	pgScaleLong(scale, origin, &par->indents.right_indent);
	pgScaleLong(scale, origin, &par->indents.first_indent);
	pgScaleLong(scale, 0, &par->left_extra);
	pgScaleLong(scale, 0, &par->right_extra);
	pgScaleLong(scale, 0, &par->def_tab_space);
	pgScaleLong(scale, 0, &par->top_extra);
	pgScaleLong(scale, 0, &par->bot_extra);
	pgScaleLong(scale, 0, &par->leading_extra);
	pgScaleLong(scale, 0, &par->leading_fixed);
	pgScaleLong(scale, 0, &par->leading_variable);

	if (par->num_tabs) {
		
		tabs = par->tabs;

		for (tab_qty = par->num_tabs; tab_qty; ++tabs, --tab_qty)
			pgScaleLong(scale, origin, &tabs->position);
	}
}


/*********************************** Local Functions ***************************/



/* This function returns the absolute byte offset of a word or paragraph (which
is indicated in info_mask).   */

static void find_word_boundary (paige_rec_ptr pg, size_t offset, long info_mask,
		size_t PG_FAR *begin, size_t PG_FAR *end, pg_boolean left_side)
{
	text_block_ptr				block;
	pg_char_ptr					text;
	style_walk					walker;
	size_t						begin_offset, end_offset, local_offset, info, boundary_info;
	pg_short_t					end_length;
	
	if (!pg->t_length) {
	
		*begin = *end = 0;
		
		return;
	}
	
	pgPrepareStyleWalk(pg, offset, &walker, FALSE);
	boundary_info = info_mask & (~BLANK_BIT);
	
/* Find beginning of word or control word */
	
	if ((info_mask & WORD_SEL_BIT) && walker.cur_style->char_bytes
			&& offset < pg->t_length) {
		long		start_of_style, distance_in;
		pg_short_t	logical_chars_in, char_size;
		
		start_of_style = walker.prev_style_run->offset;
		char_size = walker.cur_style->char_bytes + 1;
		distance_in = walker.current_offset - start_of_style;
		logical_chars_in = (pg_short_t)(distance_in / char_size);
		
		distance_in = start_of_style;
		distance_in += (long) logical_chars_in * char_size;
		*begin = *end = distance_in;
		*end += (long) char_size;
	}
	else {

		block = pgFindTextBlock(pg, offset, NULL, FALSE, TRUE);
		text = (pg_char_ptr) UseMemory(block->text);
		local_offset = offset - block->begin;
		end_offset = block->end - block->begin;
		
		while (local_offset) {
			
			pgWalkStyle(&walker, -1);
			--local_offset;
			
			info = walker.cur_style->procs.char_info(pg, &walker, text,
							block->begin, 0, end_offset, local_offset, info_mask | LAST_HALF_BIT | MIDDLE_CHAR_BIT);
			
			if (!(info & (LAST_HALF_BIT | MIDDLE_CHAR_BIT)))			
				if ((info & boundary_info) || ((info_mask & WORD_SEL_BIT) && walker.cur_style->char_bytes)) {
				
					++local_offset;
					pgWalkStyle(&walker, 1);
	
					break;
				}
		}

		*begin = block->begin + local_offset;
	
		pgSetWalkStyle(&walker, offset);
		local_offset = offset - block->begin;
		begin_offset = local_offset;
		end_offset = block->end - block->begin;
		end_length = (pg_short_t)(block->end - offset);
		
		while (end_length) {
	
			info = walker.cur_style->procs.char_info(pg, &walker, text,
							block->begin, begin_offset, end_offset, local_offset,
							info_mask | INCLUDE_SEL_BIT | LAST_HALF_BIT | MIDDLE_CHAR_BIT);
			
			if ((info_mask & WORD_SEL_BIT) && walker.cur_style->char_bytes)
				break;
			
			if (!(info & (LAST_HALF_BIT | MIDDLE_CHAR_BIT)))
				if (info & boundary_info) {
					if (((info_mask & WORD_SEL_BIT) && (info & INCLUDE_SEL_BIT))
						|| (info_mask & PAR_SEL_BIT) || (info_mask & CTL_BIT)) {
					
					++local_offset;
					--end_length;
					
					}
					
				break;
			}
			
			++local_offset;
			--end_length;
	
			pgWalkStyle(&walker, 1);
		}

		if (info_mask & BLANK_BIT) {
			
			pgSetWalkStyle(&walker, block->begin + local_offset);
			
			while (end_length) {
	
				info = walker.cur_style->procs.char_info(pg, &walker, text,
					block->begin, begin_offset, end_offset, local_offset, BLANK_BIT | CTL_BIT | LAST_HALF_BIT | MIDDLE_CHAR_BIT);
				
				if (!(info & BLANK_BIT) || (info & CTL_BIT) )
					if (!(info & (LAST_HALF_BIT | MIDDLE_CHAR_BIT)))
						break;

				++local_offset;
				--end_length;
				
				pgWalkStyle(&walker, 1);
			}
		}

		*end = block->begin + local_offset;
		
		if (*end == *begin) {
			
			if (left_side) {
				if (*begin) {
					pgSetWalkStyle(&walker, *begin - 1);
					*begin -= (walker.cur_style->char_bytes + 1);
				}
			}
			else
			if (*end < pg->t_length)
				*end += (walker.cur_style->char_bytes + 1);
		}

		*begin = adjust_for_multibyte(pg, &walker, text, block->begin, 0, end_offset, *begin, 1);
		*end = adjust_for_multibyte(pg, &walker, text, block->begin, 0, end_offset, *end, 1);
		
		UnuseMemory(block->text);
		UnuseMemory(pg->t_blocks);
	}

	pgPrepareStyleWalk(pg, 0, NULL, FALSE);
}




/* This function locates the closest tab to cur_base, where tab stops begin relative
to left_base + par_info->tab_base.  If no tab stops in the styles, the default
spacing is used and "left tab" is set.  The function result is zero if the tab
is a default tab (none exist in the paragraph style), or it is the tab record
number (in par style) | EXTRA_IS_TAB.   */

static pg_short_t locate_closest_tab (style_walk_ptr walker, long left_base, long right_base,
			long cur_base, tab_stop_ptr tab_result)
{
	register par_info_ptr			par_stuff;
	register tab_stop_ptr			tab, real_tabs;
	register pg_short_t				num_tabs, tab_ctr;
	long							def_tab_space, fake_indent_tab, first_indent;
	pg_short_t						real_tab_rec, tab_short_work;

	par_stuff = walker->cur_par_style;
	tab = tab_result;
	pgFillBlock(tab, sizeof(tab_stop), 0);

	fake_indent_tab = walker->cur_par_style->indents.left_indent + left_base;
	if ((first_indent = walker->cur_par_style->indents.first_indent) > 0)
		fake_indent_tab += first_indent;

	if (num_tabs = par_stuff->num_tabs) {
		long			closest_h, match_h;

		closest_h = -1;
		tab_ctr = real_tab_rec = 0;
		real_tabs = par_stuff->tabs;

		while (num_tabs) {

			match_h = real_tabs->position + left_base;
			
			if (cur_base < match_h && match_h < right_base) {
				
				match_h -= cur_base;

				if ((closest_h == -1) || (match_h < closest_h)) {
					
					*tab = *real_tabs;
					closest_h = match_h;
					real_tab_rec = tab_ctr;
				}
			}
			
			++real_tabs;
			++tab_ctr;
			--num_tabs;
		}
		
		if (closest_h >= 0) {
			
			if (fake_indent_tab > 0)
				if (fake_indent_tab > cur_base)
					if ((tab->position + left_base) > fake_indent_tab) {
						
						tab->tab_type = left_tab;
						tab->position = fake_indent_tab - left_base;
						
						return	0;
					}
			
			return	(real_tab_rec | EXTRA_IS_TAB);
		}
	}
	
/* Default falls through to here */
	
	if ((fake_indent_tab > 0) && (fake_indent_tab > cur_base)) {

		tab->tab_type = left_tab;
		tab->position = fake_indent_tab - left_base;
	}
	else {
	
		tab->tab_type = left_tab;
		tab->position = cur_base - left_base;
		
		if (!(def_tab_space = par_stuff->def_tab_space))
			def_tab_space = MINIMUM_TAB_SPACE;
		tab_short_work = (pg_short_t)(tab->position / def_tab_space);
		++tab_short_work;
		tab->position = tab_short_work * def_tab_space;
	}

	return		0;
}



/* encode_j_extra returns the value that should be placed in the "extra" field
of a point_start record. I used special flags for negative vs. positive numbers. */
static short encode_j_extra (long j_extra)
{
	short			result;
	
	if (j_extra > 0)
		result = (short)j_extra;
	else
		result = (short)((-j_extra) | J_EXTRA_NEGATIVE);
	
	return	result;
}



/* lines_on_different_page returns >= 0 if any of the lines within the specified range
are on different pages. If so, the function returns the text offset of the first line
in the range. Otherwise, we return -1. */

static long lines_on_different_page (text_block_ptr block, select_pair_ptr range)
{
	register point_start_ptr	starts;
	long						r_num, abs_offset, abs_begin;
	pg_short_t					local_begin, previous_flags;

	starts = (point_start_ptr) UseMemory(block->lines);
	local_begin = (pg_short_t)(range->begin - block->begin);
	
	while (starts->offset < local_begin)
		++starts;

	r_num = starts->r_num;
	abs_begin = (long)starts->offset;
	abs_begin += block->begin;
	previous_flags = 0;

	for (;;) {
		
		abs_offset = (long)starts->offset;
		abs_offset += block->begin;
		
		if (abs_offset >= range->end)
			break;
		
		if ((previous_flags & (BREAK_CONTAINER_BIT | BREAK_PAGE_BIT)) != 0)
			r_num = starts->r_num;

		if (starts->r_num != r_num) {
			
			UnuseMemory(block->lines);
			
			return	abs_begin;
		}
		
		previous_flags = starts->flags;

		++starts;
		
		if (starts->flags == TERMINATOR_BITS) {
			
			if (range->end <= block->end)
				break;
			
			UnuseMemory(block->lines);
			++block;
			starts = (point_start_ptr) UseMemory(block->lines);
		}
	}

	UnuseMemory(block->lines);
	
	return	-1;
}


/* top_page_this_block returns the number of lines sitting at the top of the page that
belong to a paragraph that begins on the previous page. This is used to check widows.
The test is done on all lines beginning at range->begin. If any are found, range->begin is set to
the top offset of the paragraph and range->end is set to the end of the lines checked. */

static short top_page_this_block (paige_rec_ptr pg, text_block_ptr block,
		select_pair_ptr range)
{
	point_start_ptr			starts;
	pg_short_t				local_offset, last_local_par;
	long					found_offset, r_num;
	short					answer;

	answer = 0;

	if ((found_offset = lines_on_different_page(block, range)) >= 0) {
		
		if (found_offset < block->begin)
			found_offset = block->begin;

		starts = (point_start_ptr) UseMemory(block->lines);
		local_offset = (pg_short_t)(found_offset - block->begin);
		while (local_offset > starts->offset)
			++starts;
		
		r_num = starts->r_num;

	// Locate "top" of the page.
		
		last_local_par = starts->offset;

		while (starts->r_num == r_num) {
			
			if (starts->flags & NEW_PAR_BIT)
				last_local_par = starts->offset;

			++starts;
		}
		
		range->begin = (long)last_local_par;
		range->begin += block->begin;

		while (!(starts->flags & NEW_PAR_BIT)) {

			if (starts->flags & NEW_LINE_BIT)
				++answer;
			
			++starts;
			range->end = (long)starts->offset;
			range->end += block->begin;

			if (starts->flags == TERMINATOR_BITS)
				break;
		}
		
		UnuseMemory(block->lines);
	}
	
	return	answer;
}


/* bottom_page_this_block works just like its counterpart above except the number of
paragraph lines on the bottom are returned. */

static short bottom_page_this_block (paige_rec_ptr pg, text_block_ptr block,
		select_pair_ptr range)
{
	register point_start_ptr	starts;
	pg_short_t					local_offset;
	long						found_offset, r_num;
	short						answer;

	answer = 0;

	if ((found_offset = lines_on_different_page(block, range)) >= 0) {
		
		if (found_offset < block->begin)
			found_offset = block->begin;

		starts = (point_start_ptr) UseMemory(block->lines);
		local_offset = (pg_short_t)(found_offset - block->begin);
		while (local_offset > starts->offset)
			++starts;
		
		r_num = starts->r_num;

	// Locate "bottom" of the page.
		
		while (starts[1].r_num == r_num) {
			
			if (starts[1].flags == TERMINATOR_BITS)
				break;

			++starts;
		}
		
	// We now sit on the very last point_start of the page.

		range->end = (long)starts[1].offset;
		range->end += block->begin;

		if (!(starts[1].flags & NEW_PAR_BIT)) {
			
			for (;;) {
				
				if (starts->flags & NEW_LINE_BIT)
					++answer;
				if (starts->flags & NEW_PAR_BIT)
					break;
				if (starts->offset == 0)
					break;
				
				--starts;
			}
		}
		
		range->begin = (long)starts->offset;
		range->begin += block->begin;

		UnuseMemory(block->lines);
	}
	
	return	answer;
}


/* first_line_on_page returns TRUE if the line (starts) is the first line on a page. */

static pg_boolean first_line_on_page (paige_rec_ptr pg, text_block_ptr block, point_start_ptr starts)
{
	long			r_num;

	if (starts->offset == 0 && block->begin == 0)
		return	TRUE;
	if (!(pg->doc_info.attributes & V_REPEAT_BIT))
		return	FALSE;
	
	r_num = starts->r_num;

	if (starts->offset) {
		
		--starts;
		
		return	(starts->r_num != r_num);
	}
	else {
		
		--block;
		
		return	(block->end_start.r_num != r_num);
	}
}


/* adjust_for_multibyte takes a text offset (char_offset) and goes forward or backward until
it aligns to a single byte char or the first half of a multibyte char. The new offset is
returned as the function result. NOTE, char_offset is ABSOLUTE GLOBAL to pg and so is the
function result. */

static long adjust_for_multibyte (paige_rec_ptr pg, style_walk_ptr walker, pg_char_ptr text,
		long block_begin, long offset_begin, long offset_end, long char_offset,
		short increment_direction)
{
	register long		local_offset;
	
	local_offset = char_offset - block_begin;
	pgSetWalkStyle(walker, char_offset);

	if (increment_direction < 0) {
		
		while (local_offset > offset_begin) {
			
			if (!(walker->cur_style->procs.char_info(pg, walker, text, block_begin,
					offset_begin, offset_end, local_offset, LAST_HALF_BIT | MIDDLE_CHAR_BIT)))
				break;
			
			pgWalkStyle(walker, -1);
			--local_offset;
		}
	}
	else {	// Forward advance

		while (local_offset < offset_end) {
			
			if (!(walker->cur_style->procs.char_info(pg, walker, text, block_begin,
					offset_begin, offset_end, local_offset, LAST_HALF_BIT | MIDDLE_CHAR_BIT)))
				break;
			
			pgWalkStyle(walker, 1);
			++local_offset;
		}
	}

	return	(block_begin + local_offset);
}


/* is_real_text returns TRUE if the offset in question is not part of a non-text style. */

static pg_boolean is_real_text (paige_rec_ptr pg, size_t offset_to_test)
{
	style_info_ptr	the_style;
	long			style_check;
	pg_boolean		valid_text;

	if ((style_check = offset_to_test) != 0)
		--style_check;

	the_style = pgFindTextStyle(pg, style_check);
	valid_text = (pg_boolean)((the_style->class_bits & IS_NOT_TEXT_BIT) == 0)
				&& (the_style->styles[hidden_text_var] == 0);
	UnuseMemory(pg->t_formats);
	
	return	valid_text;
}


/* compute_overflow_size returns the number of bytes in the text that are below bounds. */

static long compute_overflow_size (paige_rec_ptr pg, text_block_ptr blocks, long bottom_side)
{
	text_block_ptr				block;
	register point_start_ptr	starts;
	long						offset_begin;
	
	block = blocks;

	for (;;) {
		
		offset_begin = block->end;
		
		if (!(block->flags & (NEEDS_CALC | NEEDS_PAGINATE))) {
		
			starts = (point_start_ptr) UseMemory(block->lines);
			
			while (starts->flags &= TERMINATOR_BITS) {
				
				if (starts->bounds.bot_right.v > bottom_side) {
					
					offset_begin = (long)starts->offset;
					offset_begin += block->begin;
					UnuseMemory(block->lines);
					
					return	(pg->t_length - offset_begin);
				}
				
				++starts;
			}
			
			UnuseMemory(block->lines);
		}

		if (block->end == pg->t_length)
			break;
		
		++block;
	}
	
	return		0;
}

/* get_cell_bottom returns the bottom (largest) pen size. This does not get called unless
the paragraph format is a table. */

static long get_cell_bottom (par_info_ptr par)
{
	pg_short_t			tab_ctr;
	long				pensize, cell_pensize, border_info;

	pensize = (par->table.cell_borders >> 24) & PG_BORDER_PENSIZE;
	
	if ((tab_ctr = par->num_tabs) > 0) {
		
		for (tab_ctr = 0; tab_ctr < par->num_tabs; ++tab_ctr) {
			
			border_info = par->tabs[tab_ctr].leader >> 24;

			cell_pensize = border_info & PG_BORDER_PENSIZE;
			
			if (border_info & PG_BORDER_SHADOW)
				cell_pensize += cell_pensize;
			else
			if (border_info & PG_BORDER_DOUBLE)
				cell_pensize += (cell_pensize + 1);
			
			if (cell_pensize > pensize)
				pensize = cell_pensize;
		}
	}
	
	return	pensize;
}

