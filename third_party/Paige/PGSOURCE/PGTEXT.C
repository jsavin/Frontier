/* This file handles text_block records in a Paige struct. */

#include "Paige.h"

#ifdef MAC_PLATFORM
#pragma segment pgbasic2
#endif

#include "machine.h"
#include "pgOSUtl.h"
#include "pgText.h"
#include "pgEdit.h"
#include "pgSelect.h"
#include "pgDefStl.h"
#include "defprocs.h"
#include "pgUtils.h"
#include "pgShapes.h"
#include "pgScript.h"
#include "pgTxtWid.h"
#include "pgDefPar.h"
#include "pgSubRef.h"
#include "pgTables.h"


#define START_APPEND_SIZE	18			/* Amount I append to line_starts */
#define MINIMUM_GUESS_BEGIN	10000		/* Block begin must be at least this to make a guess */
#define FORCED_QUICK_BREAK -3

static void count_lines_and_pars (paige_rec_ptr pg, text_block_ptr the_block);
static void finish_line_update (paige_rec_ptr pg, pg_measure_ptr line_stuff);
static void do_partial_rebuild (paige_rec_ptr pg, pg_measure_ptr line_stuff,
		long PG_FAR *shift_begin, long PG_FAR *shift_end, smart_update_ptr update_info);
static void paginate_multi_rect_shape (paige_rec_ptr pg, text_block_ptr block,
		pg_short_t initial_offset, long PG_FAR *shift_begin, long PG_FAR *shift_end,
		pg_measure_ptr line_stuff);
static pg_short_t convert_wrap_rect (paige_rec_ptr pg, long r_num, pg_measure_ptr line_stuff,
		rectangle_ptr real_rect, co_ordinate_ptr rect_offset, pg_boolean top_of_page);
static void build_empty_block (paige_rec_ptr pg, pg_measure_ptr line_stuff);
static void update_vertical_line (paige_rec_ptr pg, pg_measure_ptr line_info,
		short PG_FAR *last_ascent, short PG_FAR *last_descent,
		short PG_FAR *last_leading, point_start_ptr starts, pg_short_t num_starts,
		rectangle_ptr must_fit, memory_ref subref);
static void set_par_left_and_right (paige_rec_ptr pg, pg_measure_ptr line_info, rectangle_ptr fit_rect,
		point_start_ptr first_start);
static point_start_ptr extend_starts (pg_measure_ptr line_info, point_start_ptr starts);
static void init_starting_line_rect (paige_rec_ptr pg, pg_measure_ptr line_stuff);
static void uninit_rects (paige_rec_ptr pg, pg_measure_ptr line_stuff);
static void advance_line_rect (paige_rec_ptr pg, pg_measure_ptr line_stuff);
static pg_boolean assure_minimum_width (paige_rec_ptr pg, pg_measure_ptr line_stuff);
static void setup_union_line_rect (paige_rec_ptr pg, pg_measure_ptr line_stuff,
		short compute_new_height);
static void assure_r_num_correct (paige_rec_ptr pg, pg_measure_ptr line_stuff);
static void set_fit_rect (paige_rec_ptr pg, pg_measure_ptr line_info, pg_boolean top_line_in_page);
static void adjust_line_starts (paige_rec_ptr pg, pg_measure_ptr line_stuff,
		rectangle_ptr adjust_rect, pg_short_t unadjusted_starts,
		par_info_ptr line_par_format);
static long set_last_good_start (pg_measure_ptr the_info);
static pg_boolean block_hidden (paige_rec_ptr pg, text_block_ptr block,
		style_walk_ptr walker);
static void set_extra_indents (paige_rec_ptr pg, long r_num, pg_measure_ptr line_stuff,
		pg_boolean first_line_in_page);
static void terminate_excess_starts (point_start_ptr end_start, pg_short_t remaining);
static pg_boolean starts_below_view (paige_rec_ptr pg, pg_measure_ptr line_stuff);
static tab_width_ptr insert_tab_record (memory_ref tab_recs, long current_offset,
		long PG_FAR *width_replace);
static void attach_par_exclusion (paige_rec_ptr pg, pg_measure_ptr line_info);


/* pgLineValidate is the default hook for validating a line. */

PG_PASCAL (pg_boolean) pgLineValidate (paige_rec_ptr pg, pg_measure_ptr measure_info)
{
	register pg_measure_ptr			line_info;
	register rectangle_ptr			wrap_r;
	register point_start_ptr		starts;
	register pg_short_t				num_starts;
	par_info_ptr					par_style;
	co_ordinate						last_topleft;
	long							line_height, wrap_width, last_wrap_width;
	long							wrap_height, r_bottom, abs_offset;
	long							united_rects;
	
	pgTableAdjustLine(pg, measure_info);

	line_info = measure_info;

	line_info->minimum_left = line_info->wrap_r_begin->top_left.h;
	line_info->maximum_right = line_info->wrap_r_begin->bot_right.h;
	
	united_rects = line_info->r_num_end - line_info->r_num_begin;

	if (united_rects && (line_info->r_num_begin < line_info->end_r)) {
	
		for (wrap_r = line_info->wrap_r_begin; united_rects; --united_rects) {
			
			++wrap_r;
			
			if (wrap_r->top_left.h > line_info->minimum_left)
				line_info->minimum_left = wrap_r->top_left.h;
			if (wrap_r->bot_right.h < line_info->maximum_right)
				line_info->maximum_right = wrap_r->bot_right.h;
		}
	}

	wrap_r = line_info->wrap_r_end;			/* I'll need to check the END rect */
	r_bottom = wrap_r->bot_right.v + line_info->repeat_offset.v;
	r_bottom -= line_info->extra_indents.bot_right.v;

	starts = line_info->starts;
	num_starts = line_info->num_starts;
	starts -= num_starts;
	
	if (pg->forced_break) {
		
		abs_offset = (long)starts->offset;
		abs_offset += line_info->block->begin;

		if (abs_offset == pg->forced_break) {
		
			r_bottom = line_info->actual_rect.bot_right.v - 1;
			pg->forced_break = -1;
		}
	}

	if (line_info->quick_paginate) {
		
		if (line_info->quick_paginate == FORCED_QUICK_BREAK)
			return	FALSE;
		
		return	(line_info->actual_rect.bot_right.v <= r_bottom); 
	}
	
	par_style = line_info->styles->cur_par_style;
	pgAdjustBiDirections(starts, num_starts, par_style);
	
	if (pgPrimaryTextDirection(pg->globals, par_style) == right_left_direction) {
	
		pgAdjustRightLeftStarts(pg, &line_info->wrap_bounds, right_left_direction, starts, num_starts);
		pgFlipRect(&line_info->wrap_bounds, &line_info->actual_rect, right_left_direction);
	}

	if (line_info->line_text_size) {
		
		if (!(pg->doc_info.attributes & BOTTOM_FIXED_BIT))
			return	TRUE;
		
		if (line_info->repeating)
			if (line_info->last_page != starts->r_num)
				return	TRUE;

		if ((line_info->actual_rect.bot_right.v <= r_bottom)
			&& (line_info->actual_rect.top_left.h >= line_info->minimum_left)
			&& (line_info->actual_rect.bot_right.h <= line_info->maximum_right))
			return	TRUE;
		
		if (line_info->actual_rect.bot_right.v <= r_bottom)
			if ((line_info->maximum_right - line_info->minimum_left) >= pg->globals->minimum_line_width)
			return	FALSE;

		if (pg->doc_info.attributes & NO_CONTAINER_JMP_BIT)	/* If no advance on containers */
			return	TRUE;
	}

/* Line will not fit in current rectangle. Hence, we'll decide on the least
   expensive path.  */

	last_wrap_width = line_info->fit_rect.bot_right.h - line_info->fit_rect.top_left.h;
	last_topleft = line_info->fit_rect.top_left;
	line_height = line_info->actual_rect.bot_right.v - line_info->actual_rect.top_left.v;

	if (line_info->wrap_dimension) {
		
		if (!line_info->line_text_size) {
			
			++line_info->r_num_begin;
			line_info->wrap_r_begin = line_info->wrap_r_base + pgGetWrapRect(pg,
					line_info->r_num_begin, &line_info->repeat_offset);

			line_info->wrap_r_end = line_info->wrap_r_begin;
			line_info->r_num_end = line_info->r_num_begin;
			set_fit_rect(pg, line_info, TRUE);
		}
		else
			line_info->fit_rect.bot_right.v = line_info->fit_rect.top_left.v + line_height;

		setup_union_line_rect(pg, line_info, (pg_boolean)(line_info->line_text_size == 0));
	}
	else {

		++line_info->r_num_end;
		wrap_r = line_info->wrap_r_base;
		wrap_r += pgGetWrapRect(pg, line_info->r_num_end, &line_info->repeat_offset);

		line_info->wrap_r_begin = line_info->wrap_r_end = wrap_r;
		line_info->r_num_begin = line_info->r_num_end;

		wrap_width = wrap_r->bot_right.h - wrap_r->top_left.h;
		wrap_height = wrap_r->bot_right.v - wrap_r->top_left.v;
		
		pgBlockMove(wrap_r, &line_info->fit_rect, sizeof(rectangle));
		pgOffsetRect(&line_info->fit_rect, line_info->repeat_offset.h, line_info->repeat_offset.v);
		set_extra_indents(pg, line_info->r_num_begin, line_info, TRUE);
	}

	line_info->minimum_left = line_info->wrap_r_begin->top_left.h;
	line_info->maximum_right = line_info->wrap_r_begin->bot_right.h;

	/* See if I can simply move the line  */
	
	if (((line_height > wrap_height) && line_info->repeating) || ((wrap_width == last_wrap_width)
		&& line_info->line_text_size && (!line_info->wrap_dimension))) {
		rectangle		new_line_bounds;
		long			adjust_h, adjust_v;
		
		adjust_h = line_info->fit_rect.top_left.h - last_topleft.h;
		adjust_v = line_info->fit_rect.top_left.v - last_topleft.v;
		pgOffsetRect(&line_info->actual_rect, adjust_h, adjust_v);
		new_line_bounds = starts->bounds;
		pgOffsetRect(&new_line_bounds, adjust_h, adjust_v);

		while (num_starts) {
			
			starts->r_num = line_info->r_num_begin;
			pgOffsetRect(&starts->bounds, adjust_h, adjust_v);
			pgLineUnionRect(starts, &new_line_bounds);

			++starts;
			--num_starts;
		}
		
		line_info->fit_rect.top_left.v += line_height;

		if ((new_line_bounds.bot_right.h - new_line_bounds.top_left.h) < pg->globals->minimum_line_width)
			new_line_bounds.bot_right.h = new_line_bounds.top_left.h + pg->globals->minimum_line_width;
			
		return	(!pgRectInShape(pg->exclude_area, &new_line_bounds, NULL, NULL));
	}

	return	FALSE;
}



/* DEFAULT LINE MEASURE FUNCTION. This is the internal hook used to calculate
a line. You can tap into this one or leave the default. All the information you
will ever need is in line_info (the starts field will hold the next
point_start and offset).  This function must update everything in line_info that
is appropriate (such as changing the pointers for the next line, etc.).  */

PG_PASCAL (void) pgLineMeasureProc (paige_rec_ptr pg, pg_measure_ptr line_info)
{
	register long PG_FAR		*positions;
	register short PG_FAR		*char_types;
	register point_start_ptr	starts;
	par_info_ptr				line_par_format;
	rectangle_ptr				exclude_ptr;
	style_walk_ptr				walker;
	pg_char_ptr					text;
	tab_width_ptr				tabs, original_tabs;
	rectangle					adjust_rect;
	memory_ref					subref;
	long						text_length, width_base, tab_extra, global_offset;
	long						max_width, exclude_width, offset_begin, offset_end;
	long						start_size, line_text_size, left_edge, right_edge;
	long						next_style_size, current_width, old_tab_width;
	long						original_offset, original_end, last_tab_width;
	long						pg_flags, lowest_exclude, max_offset, c_info, last_tab_offset;
	long						overhang_extra, next_overhang_offset, extra_width;
	short						ascent, descent, leading;
	pg_short_t					unadjusted_starts, exclude_qty;
	short						hyphenated, possible_exclusions;

	exclude_ptr = NULL;
	subref = MEM_NULL;
	lowest_exclude = line_info->fit_rect.bot_right.v;
	global_offset = line_info->block->begin;

	possible_exclusions = TRUE;
	exclude_qty = 0;
	
	walker = line_info->styles;
	line_par_format = walker->cur_par_style;
	attach_par_exclusion(pg, line_info);

	while (possible_exclusions) {
		
		unadjusted_starts = 0;
		extra_width = tab_extra = last_tab_width = 0;
		
		starts = line_info->starts;
		positions = line_info->char_locs;
		char_types = line_info->char_types;
		
		offset_begin = starts->offset;
		last_tab_offset = offset_begin;
		original_offset = offset_begin;
		offset_end = offset_begin;
		
		text_length = line_info->max_text_size;
		max_offset = offset_begin + line_info->max_text_size;
		
		SetMemorySize(line_info->tab_info, 1);
		original_tabs = tabs = (tab_width_ptr) UseMemory(line_info->tab_info);
		tabs->offset = GOD_AWFUL_HUGE;

		text = (pg_char_ptr) UseMemory(line_info->block->text);
	
		width_base = *positions;			/* == base-zero for character widths */
	
		starts->flags = (LINE_GOOD_BIT | NEW_LINE_BIT);
		starts->r_num = line_info->r_num_begin;
		starts->cell_height = 0;
		starts->cell_num = line_info->cell_index;

		if (line_info->previous_flags & (PAR_BREAK_BIT | BREAK_CONTAINER_BIT))
			starts->flags |= NEW_PAR_BIT;

		if (exclude_ptr) {
			
			if (exclude_ptr[exclude_qty - 1].bot_right.h >= line_info->fit_rect.bot_right.h)
				line_info->fit_rect.bot_right.h = exclude_ptr[exclude_qty - 1].top_left.h;

			if (exclude_ptr->top_left.h <= line_info->fit_rect.top_left.h) {
				
				line_info->fit_rect.top_left.h = exclude_ptr->bot_right.h;
				++exclude_ptr;
			}
		}
		
		line_info->actual_rect = line_info->fit_rect;		
		set_par_left_and_right(pg, line_info, &line_info->actual_rect, starts);
		line_info->actual_rect.bot_right.v = line_info->actual_rect.top_left.v;
		line_info->num_starts = 0;
		ascent = descent = leading = 0;
		left_edge = line_info->actual_rect.top_left.h + line_info->cell_extra.top_left.h;
		
		hyphenated = FALSE;
	
		pg_flags = pg->flags;
		pgBlockMove(&line_info->actual_rect, &adjust_rect, sizeof(rectangle));

		if (!text_length) {
			
			starts->flags |= LINE_BREAK_BIT;
			line_text_size = 0;
			pgBlockMove(&line_info->actual_rect, &starts->bounds, sizeof(rectangle));
			right_edge = starts->bounds.top_left.h;
			starts->baseline = starts->extra = 0;
			update_vertical_line(pg, line_info, &ascent, &descent, &leading, starts,
					0, &line_info->actual_rect, MEM_NULL);
		}
		else {
			
			if (pg->flags & NO_WRAP_BIT)
				max_width = GOD_AWFUL_HUGE;
			else
				max_width = (line_info->actual_rect.bot_right.h
							- line_info->actual_rect.top_left.h)
							 - (line_info->cell_extra.top_left.h
							 + line_info->cell_extra.bot_right.h);
			
			if (exclude_ptr)
				exclude_width = exclude_ptr->top_left.h - line_info->actual_rect.top_left.h;
			else
				exclude_width = GOD_AWFUL_HUGE;

			current_width = tab_extra = overhang_extra = 0;

			pgSetWalkStyle(walker, offset_end + global_offset);
			
			overhang_extra = walker->cur_style->right_overhang;
			next_overhang_offset = walker->next_style_run->offset - global_offset;

		/* First step is to determine the size of the line regardless of any other info. */

			while (text_length) {
				
				++positions;
				++offset_end;
				--text_length;
				
				c_info = *char_types++;

				if (c_info & (~BLANK_BIT)) {
					
					if (c_info & (PAR_SEL_BIT | LINE_SEL_BIT | CONTAINER_BRK_BIT | PAGE_BRK_BIT)) {
						
						if (line_info->is_table || (!(pg_flags & NO_LF_BIT)) || (!(c_info & LINE_SEL_BIT))) {
							
							--positions;
							break;
						}
					}

					if (c_info & (TAB_BIT | SOFT_HYPHEN_BIT | LINE_SEL_BIT)) {
						
						tabs = insert_tab_record(line_info->tab_info, offset_end, &old_tab_width);
						
						tabs->offset = last_tab_offset = offset_end;
						tabs->extra = 0;
						tabs->type = (short)c_info;
						tabs->width = tabs->exclude_right = tabs->max_width = 0;

						if (line_info->is_table && (c_info & TAB_BIT)) {

							--positions;
							starts->flags |= TAB_BREAK_BIT;
							break;
						}

						if (c_info & TAB_BIT) {
							
							tabs->width = walker->cur_par_style->procs.tab_width(pg, walker,
								current_width + left_edge, offset_end + global_offset,
								line_info->fit_rect.top_left.h, line_info->actual_rect.bot_right.h,
								&text[offset_end], (pg_short_t)text_length, positions, &tabs->extra);

							tabs->width -= (current_width + left_edge - old_tab_width);
						}
	
						--positions;
						tabs->hard_width = positions[1] - *positions;
						++positions;
						
						tabs->text_width = 0;
						last_tab_width = tabs->width - tabs->hard_width;
						tab_extra += last_tab_width;
					}
				}

				if (offset_end >= next_overhang_offset) {
					
					pgWalkNextStyle(walker);
					overhang_extra = walker->cur_style->right_overhang;
					next_overhang_offset = walker->next_style_run->offset - global_offset;
				}

				current_width = (*positions - width_base) + tab_extra + overhang_extra;
				
				if ((current_width >= max_width) || (current_width >= exclude_width)) {  /* Line would overflow here */
					long			back_amount, full_length;
	
					pgSetWalkStyle(walker, offset_end + global_offset);
					original_end = offset_end;
					
					hyphenated = pg->procs.hyphenate(pg, line_info->block, walker,
							text, offset_begin, &offset_end, line_info->positions,
							line_info->types, &extra_width, (pg_boolean)(exclude_ptr != NULL));
					
					back_amount = original_end - offset_end;
					positions -= back_amount;
					char_types -= back_amount;
					text_length += back_amount;
					
					if (last_tab_offset >= offset_end)
						tab_extra -= last_tab_width;

			/* Now add all the trailing blanks to the line.  */

					pgSetWalkStyle(walker, offset_end + global_offset);
					full_length = text_length + offset_end;
					
					while (text_length) {
						
						c_info = walker->cur_style->procs.char_info(pg, walker, text, global_offset,
								offset_begin, full_length, offset_end, BLANK_BIT | TAB_BIT | CTL_BIT | SOFT_HYPHEN_BIT);
						
						if ((c_info & TAB_BIT) && offset_end > offset_begin)
							break;

						if ((!(c_info & BLANK_BIT) && (!(c_info & SOFT_HYPHEN_BIT))))
							break;

						++offset_end;
						--text_length;

						pgWalkStyle(walker, 1);
						
						if (c_info & CTL_BIT)
							break;
					}

			/* Possibly we terminated due to exclusion areas. If so, don't break
			out of the line calc loop just yet.  */

					if ((current_width < max_width) && (current_width >= exclude_width)) {
						long		real_right_edge;

						tabs = insert_tab_record(line_info->tab_info, offset_end, &old_tab_width);
						
						tab_extra -= old_tab_width;

						right_edge = left_edge + *positions - width_base + tab_extra;
						positions = line_info->positions + offset_end;
						real_right_edge = left_edge + *positions - width_base + tab_extra;
						
						char_types = line_info->types + offset_end;
						
						tabs->max_width = exclude_width;
						tabs->exclude_right = exclude_ptr->top_left.h;
						tabs->text_width = right_edge;
						tabs->width = exclude_ptr->bot_right.h - real_right_edge;
						tabs->offset = offset_end;
						tabs->extra = 0;
						tabs->type = 0;
						tabs->hard_width = 0;
						tab_extra += tabs->width;
						
						++exclude_ptr;
						exclude_width = exclude_ptr->top_left.h - line_info->actual_rect.top_left.h;
					}
					else
						break;
				}
			}

		/* Now that we have a text ending position, walk through the styles and char
		types to produce the point start records.  */

			right_edge = left_edge + *positions - width_base + tab_extra + extra_width;
			pgSetWalkStyle(walker, offset_begin + global_offset);
			
			right_edge += walker->cur_style->right_overhang;

			line_text_size = offset_end - offset_begin;

			positions = line_info->char_locs;
			char_types = line_info->char_types;
			
			tabs = (tab_width_ptr) UseMemoryRecord(line_info->tab_info, 0, USE_ALL_RECS, FALSE);
			
			while (offset_begin < offset_end) {
				pg_boolean		broke_on_exclusion;

				broke_on_exclusion = FALSE;

				starts->bounds.top_left.v = line_info->actual_rect.top_left.v;
				starts->bounds.top_left.h = starts->bounds.bot_right.h
					 = left_edge + *positions - width_base;
				starts->bounds.bot_right.v = line_info->actual_rect.bot_right.v;
				starts->baseline = starts->extra = 0;
				starts->r_num = line_info->r_num_begin;

				if (walker->cur_style->class_bits & STYLE_IS_CUSTOM)
					starts->flags |= CUSTOM_CHARS_BIT;
				if (walker->cur_style->class_bits & RIGHTLEFT_BIT)
					starts->flags |= RIGHT_DIRECTION_BIT;

				if ((pg_flags & NO_HIDDEN_TEXT_BIT) && walker->cur_style->styles[hidden_text_var])
					starts->flags |= LINE_HIDDEN_BIT;

				start_size = offset_end - offset_begin;

				if ((*char_types & PG_SUBSET_BIT) == PG_SUBSET_BIT) {
					
					subref = pgGetThisSubset(line_info, offset_begin);
					start_size = 1;
					starts->extra |= EXTRA_IS_SUBREF;
				}
				else {
					long			next_subset;

					subref = MEM_NULL;
					next_subset = pgFindNextSubset(char_types, start_size);
					
					if (next_subset < start_size)
						start_size = next_subset;
				}

				update_vertical_line(pg, line_info, &ascent, &descent, &leading, starts,
						line_info->num_starts, &line_info->actual_rect, subref);

				next_style_size = walker->next_style_run->offset - walker->current_offset;

				if (next_style_size < start_size)
					start_size = next_style_size;

				if (walker->current_offset >= walker->hyperlink->applied_range.begin)
					next_style_size = walker->hyperlink->applied_range.end - walker->current_offset;
				else
					next_style_size = walker->hyperlink->applied_range.begin - walker->current_offset;

				if (next_style_size < start_size)
					start_size = next_style_size;

				if (walker->current_offset >= walker->hyperlink_target->applied_range.begin)
					next_style_size = walker->hyperlink_target->applied_range.end - walker->current_offset;
				else
					next_style_size = walker->hyperlink_target->applied_range.begin - walker->current_offset;

				if (next_style_size < start_size)
					start_size = next_style_size;

				next_style_size = tabs->offset - offset_begin;
				if (next_style_size < start_size)
					start_size = next_style_size;
				
				next_style_size = pg->procs.parse_line(pg, line_info, text,
						starts, offset_begin, offset_end - offset_begin);

		// Add this for SteveG5:
		
				if (next_style_size < (offset_end - offset_begin)) {
					long		new_offset_end;
					
					new_offset_end = offset_begin + next_style_size;
					line_text_size -= (offset_end - new_offset_end);
					offset_end = new_offset_end;
				}

				if (next_style_size < start_size)
					start_size = next_style_size;

				positions += start_size;
				offset_begin += start_size;
				
				if (walker->cur_par_style->justification == justify_full) {
					long		blanks_check;

					for (blanks_check = 0; blanks_check < start_size; ++blanks_check)
						if (char_types[blanks_check] & BLANK_BIT) {
							
							while (blanks_check < start_size) {
								
								if (!(char_types[blanks_check] & BLANK_BIT)) {
									
									starts->flags |= HAS_WORDS_BIT;
									break;
								}
								
								++blanks_check;
							}
							
							break;
						}
				}

				char_types += start_size;
				
				if (offset_begin <= offset_end) {

					starts->bounds.bot_right.h = left_edge + *positions - width_base;

					if (offset_begin == tabs->offset) {
						
						if (tabs->type)
							starts->bounds.bot_right.h += (tabs->width - tabs->hard_width);
						else {  /* Line breaks due to exclusion! */
						
							broke_on_exclusion = TRUE;
							starts->bounds.bot_right.h = tabs->text_width;
							
							starts -= unadjusted_starts;
							adjust_rect.top_left.h = starts->bounds.top_left.h;
							adjust_rect.bot_right.h = tabs->exclude_right;

							starts += unadjusted_starts;
						}

						starts->extra = tabs->extra;
						
						if (tabs->type & TAB_BIT)
							starts->flags |= TAB_BREAK_BIT;
						else
							starts->flags |= SOFT_BREAK_BIT;
	
						left_edge += (tabs->width - tabs->hard_width);
	
						++tabs;
					}

					if (offset_begin < offset_end) {
					
						starts = extend_starts(line_info, starts);
						starts->offset = (pg_short_t)offset_begin;
						starts->flags = LINE_GOOD_BIT;
						++unadjusted_starts;

						pgWalkStyle(walker, start_size);
						
						if (broke_on_exclusion) {
							
							adjust_line_starts(pg, line_info, &adjust_rect,
								unadjusted_starts, line_par_format);
							
							unadjusted_starts = 0;
							adjust_rect.top_left.h = left_edge + *positions - width_base;
							adjust_rect.bot_right.h = line_info->actual_rect.bot_right.h;
						}
					}
				}
			}
			
			if (!(starts->flags & LINE_HIDDEN_BIT))
				starts->flags |= LINE_BREAK_BIT;

			if (hyphenated)
				starts->flags |= WORD_HYPHEN_BIT;

			c_info = *(--char_types);
	
			if (c_info & (PAR_SEL_BIT | LINE_SEL_BIT | CONTAINER_BRK_BIT | PAGE_BRK_BIT)) {

				if (c_info & PAR_SEL_BIT) {
				
					starts->flags |= PAR_BREAK_BIT;
					
					if ((pg_flags & NO_LF_BIT) && text_length)
						if (char_types[1] & LINE_SEL_BIT) {
							
							--text_length;
							++offset_end;
							++line_text_size;
							
							starts->flags |= SOFT_PAR_BIT;
						}
				}
				else
				if (c_info & LINE_SEL_BIT) {

					if (!(pg_flags & NO_LF_BIT))
						starts->flags |= SOFT_PAR_BIT;
				}
				else
				if (c_info & CONTAINER_BRK_BIT)
					starts->flags |= BREAK_CONTAINER_BIT;
				else
				if (c_info & PAGE_BRK_BIT)
					starts->flags |= BREAK_PAGE_BIT;
				
				--positions;
			}
		}

		starts->bounds.bot_right.h = right_edge;
		
		line_info->prv_prv_flags = line_info->previous_flags;
		line_info->previous_flags = starts->flags;

	/* Check situation where NO chars exist at all within the line.  */
	
		if ((offset_end <= original_offset) && (text_length))	
			if (line_info->r_num_begin == line_info->end_r && (!exclude_qty)) {		/* If no more rects */

			pgSetWalkStyle(walker, global_offset + original_offset);
			offset_end += (walker->cur_style->char_bytes + 1);
			line_text_size = walker->cur_style->char_bytes + 1;
		}

		starts = extend_starts(line_info, starts);
		line_info->old_offset = starts->offset;
		line_info->old_last_page = line_info->last_page;

		starts->offset = (pg_short_t)offset_end;			/* next line's offset */
		starts->cell_num = line_info->cell_index;
		starts->cell_height = 0;
		++unadjusted_starts;

		line_info->char_locs += line_text_size;
		line_info->char_types += line_text_size;
		line_info->line_text_size = line_text_size;

		UnuseMemory(line_info->tab_info);
		UnuseMemory(line_info->block->text);
		
/* Last phase:  check for possible exclusions (from pg->exclude_area) now that
  the line rectangle is completely figured out.  */
  
  		if (exclude_ptr) {  /* We've been here already */
  			
  			UnuseMemory(line_info->exclude_ref);
  
  			if (line_text_size)
	  			possible_exclusions = FALSE;
	  		else {
	  			
	  			exclude_ptr = NULL;
	  			pgResetStartingLine(line_info);
				line_info->fit_rect.top_left.v = lowest_exclude;
				assure_r_num_correct(pg, line_info);
				unadjusted_starts = 0;
	  		}
  		}
  		else
  		if (!line_text_size)
  			possible_exclusions = FALSE;
  		else
  		if (!(exclude_qty = pgExcludeRectInShape(pg, &line_info->actual_rect,
  				pg->globals->minimum_line_width, &lowest_exclude,
  				original_offset + line_info->block->begin, line_info->exclude_ref)))
  			possible_exclusions = FALSE;
  		else {
			long			actual_width;
			
			actual_width = line_info->actual_rect.bot_right.h - line_info->actual_rect.top_left.h;
  			exclude_ptr = (rectangle_ptr) UseMemory(line_info->exclude_ref);
  			
  			pgResetStartingLine(line_info);
  			line_info->block->flags |= JUMPED_4_EXCLUSIONS;
  			
  			if (actual_width < pg->globals->minimum_line_width) {
  
  				line_info->fit_rect.top_left.v = lowest_exclude;
				assure_r_num_correct(pg, line_info);
				UnuseMemory(line_info->exclude_ref);
				exclude_ptr = NULL;
			}

  			unadjusted_starts = 0;
  		}
	}

	if (unadjusted_starts) {
				
		adjust_rect.top_left.v = line_info->actual_rect.top_left.v;
		adjust_rect.bot_right.v = line_info->actual_rect.bot_right.v;		
		
		if ((tabs->offset == offset_end) && (!tabs->type)) {

			starts = line_info->starts;
			--starts;
			starts->bounds.bot_right.h = tabs->text_width;
			++starts;
			adjust_rect.bot_right.h = adjust_rect.top_left.h + tabs->max_width;
		}
		else
		if (pgExcludeRectInShape(pg, &adjust_rect, pg->globals->minimum_line_width,
  				&lowest_exclude, original_offset + line_info->block->begin, line_info->exclude_ref)) {
  
  			exclude_ptr = (rectangle_ptr) UseMemory(line_info->exclude_ref);
  			
  			if ((exclude_ptr->top_left.h < adjust_rect.bot_right.h)
  				&& (adjust_rect.top_left.h < exclude_ptr->top_left.h))
  					adjust_rect.bot_right.h = exclude_ptr->top_left.h;
  			
  			UnuseMemory(line_info->exclude_ref);
  		}
  		
  		if (unadjusted_starts) {
  			
  			starts = line_info->starts;
  			--starts;

			adjust_line_starts(pg, line_info, &adjust_rect, unadjusted_starts, line_par_format);
		}
	}
	
/* Final fix: It is possible (due to silly beta-tester attempts to crash the
app), that something is making the line height > repeating page height. If so,
I will fix with a sledgehammer by forcing the actual rect to be smaller. */

	if (line_info->repeating) {
		long			wrap_height, line_height;
		
		wrap_height = line_info->wrap_bounds.bot_right.v
				- line_info->wrap_bounds.top_left.v;
		line_height = line_info->actual_rect.bot_right.v
				- line_info->actual_rect.top_left.v;
				
		if (line_height > wrap_height)
			line_info->actual_rect.bot_right.v
				= line_info->actual_rect.top_left.v + wrap_height;
	}
}

/* This does the same thing as rebuild_block except previous blocks are
rebuilt to create an effective "pagination," if necessary. The function returns
the first offset it calculated. If will_access_text is TRUE then the caller
expects to look at point_starts and/or text.  */

PG_PASCAL (void) pgPaginateBlock (paige_rec_ptr pg, text_block_ptr block,
			smart_update_ptr update_info, pg_boolean will_access_text)
{
	register text_block_ptr	remaining_block_ptr;
	rectangle				original_bounds;
	long					original_begin, bounds_diff, editor_lineheight;
	long					blocks_to_compute, progress_ctr, block_size;
	smart_update_ptr		update;
	pg_boolean				cant_just_paginate, should_call_wait, doc_dirty;
	pg_boolean				complex_shape, is_line_editor;

	if (!(block->flags & (ANY_CALC | LINES_PURGED | SWITCHED_DIRECTIONS)))
		return;
	
	is_line_editor = ((pg->flags & LINE_EDITOR_BIT) != 0);

	if (!will_access_text)
		if (!(block->flags & ANY_CALC)) {
			
			if (block->flags & SWITCHED_DIRECTIONS)
				pgAdjustDirection(pg, block);
				
			return;
	}

	if (!(block->flags & (ANY_CALC | LINES_PURGED))) {

		if (block->flags & SWITCHED_DIRECTIONS)
			pgAdjustDirection(pg, block);

		return;
	}

	if (is_line_editor) {
		style_info_ptr		first_style;
		
		first_style = (style_info_ptr) UseMemory(pg->t_formats);
		editor_lineheight = first_style->ascent + first_style->descent + first_style->leading;
		UnuseMemory(pg->t_formats);
	}

	doc_dirty = ((pg->flags & DOC_BOUNDS_DIRTY) != 0);
	pg->flags &= (~DOC_BOUNDS_DIRTY);
	
	cant_just_paginate = complex_shape = (pgShapeDimension(pg->wrap_area)
			& (MUTIPLE_WIDTH_DIMENSION | UNION_DIMENSION));
	cant_just_paginate |= (!pgEmptyShape(pg->exclude_area));
	cant_just_paginate |= (pg_boolean)((block->flags & BLOCK_HAS_TABLE) != 0);

	original_begin = block->begin;
	original_bounds = block->end_start.bounds;
	
	blocks_to_compute = 1;
	
	if (!is_line_editor || !will_access_text) {
	
		while (block->begin) {
	
			--block;
	
			if (!(block->flags & ANY_CALC)) {
		
				++block;
				break;
			}
	
			++blocks_to_compute;
		}
	}

	if (block->begin != original_begin) {
		
		if (update_info) {
			
			pgFillBlock(update_info, sizeof(smart_update), 0);
			update_info->suggest_begin = block->begin;
			update_info->suggest_end = pg->t_length;
			update_info->num_display_lines = update_info->num_display_starts
					= UNKNOWN_LINE_QTY;
		}
		
		update = NULL;
	}
	else
		update = update_info;
	
	progress_ctr = 0;
	block_size = block->end - block->begin;
	should_call_wait = (blocks_to_compute > 1);
	should_call_wait |= (!(block->flags & SOME_LINES_GOOD) && block_size > 500);

	for (;;) {
		
		if (should_call_wait)
			pg->procs.wait_proc(pg, paginate_wait, progress_ctr, blocks_to_compute);
		
		++progress_ctr;

		block->flags &= (~JUMPED_4_EXCLUSIONS);

		if (is_line_editor && (block->begin != original_begin || !will_access_text)) {
			
			if (block->flags & NEEDS_PARNUMS) {
			
				count_lines_and_pars(pg, block);
				pgShapeBounds(pg->wrap_area, &block->bounds);

				if (block->begin > 0) {
					
					--block;
					block[1].bounds.top_left.v = block->bounds.bot_right.v;
					++block;
				}
				
				block->bounds.bot_right.v = block->bounds.top_left.v + (block->num_pars * editor_lineheight);			
				
				pgFillBlock(&block->end_start, sizeof(point_start), 0);
				block->end_start.flags = PAR_BREAK_BIT;
				block->end_start.bounds = block->bounds;
				block->end_start.bounds.top_left.v = block->end_start.bounds.bot_right.v - editor_lineheight;
			}
		}
		else
		if ((!(block->flags & NEEDS_CALC)) && (!cant_just_paginate) && !(block->flags & LINES_PURGED))
			pgPaginateStarts(pg, block, block->begin, update_info);
		else {
		
			RebuildTextblock(pg, block, update);

			if (pgActiveMatrix(pg)) {
				paige_sub_ptr			sub_ptr;
				
				sub_ptr = (paige_sub_ptr) UseMemory(pg->active_subset);
				
				if ((sub_ptr->subref_flags & MATRIX_PHASE_FLAG) != 0) {
					
					block->flags |= NEEDS_CALC;
					RebuildTextblock(pg, block, update);
					sub_ptr->subref_flags &= (~MATRIX_PHASE_FLAG);
				}
				
				UnuseMemory(pg->active_subset);
			}
		}

		if (block->begin == original_begin)
			break;

		++block;
	}
	
	if (original_bounds.bot_right.v != block->end_start.bounds.bot_right.v
		|| original_bounds.top_left.h != block->end_start.bounds.top_left.h) {

		if (bounds_diff = block->end_start.bounds.bot_right.v - original_bounds.bot_right.v)
			if (update_info) {
			
				update_info->suggest_end = pg->t_length;
				update_info->num_display_lines = update_info->num_display_starts
						= UNKNOWN_LINE_QTY;
			}

		remaining_block_ptr = block;
		
		while (remaining_block_ptr->end < pg->t_length) {
			
			++remaining_block_ptr;
			
			remaining_block_ptr->bounds.top_left.v += bounds_diff;
			remaining_block_ptr->bounds.bot_right.v += bounds_diff;
			remaining_block_ptr->flags |= NEEDS_PAGINATE;
		}
	}

	if ((block->end_start.flags & NO_LINEFEED_BIT) && (block->end < pg->t_length))
		if (GetMemorySize(block[1].lines)) {
		point_start_ptr		next_starts;

		next_starts = (point_start_ptr) UseMemory(block[1].lines);
		next_starts->flags &= (~LINE_GOOD_BIT);
		UnuseMemory(block[1].lines);
		block[1].flags |= NEEDS_CALC | NEEDS_PARNUMS;
	}

	if (!pg->active_subset)
		pgComputeDocHeight(pg, doc_dirty);

	if (should_call_wait)
		pg->procs.wait_proc(pg, 0, blocks_to_compute, blocks_to_compute);
}




/* pgPaginateStarts is similar to RebuildTextblock except it doesn't actually
determine any line widths, rather, the lines are simply moved vertically. Note
the only time we can get away with this is for wrap shapes that have a consistent
width throughout.  */

PG_PASCAL (void) pgPaginateStarts (paige_rec_ptr pg, text_block_ptr block, long initial_offset,
		smart_update_ptr update_info)
{
	register point_start_ptr		starts;
	register long					paginate_amt;
	pg_measure						line_info;
	long							bounds_ht, shift_begin, shift_end;

	pgFillBlock(&line_info, sizeof(pg_measure), 0);
	line_info.block = block;
	line_info.starts = starts = (point_start_ptr) UseMemory(block->lines);
	starts->offset = 0;
	shift_begin = shift_end = block->begin;
	
	init_starting_line_rect(pg, &line_info);
	
	if (line_info.repeating || line_info.end_r)
		paginate_multi_rect_shape(pg, block, (pg_short_t)(initial_offset - block->begin),
				&shift_begin, &shift_end, &line_info);
	else
	if (paginate_amt = (line_info.fit_rect.top_left.v - starts->bounds.top_left.v)) {
		
		shift_end = block->end;

		bounds_ht = block->bounds.bot_right.v - block->bounds.top_left.v;
		block->bounds.top_left.v = line_info.fit_rect.top_left.v;
		block->bounds.bot_right.v = line_info.fit_rect.top_left.v + bounds_ht;

		while (starts->flags != TERMINATOR_BITS) {
			
			starts->bounds.top_left.v += paginate_amt;
			starts->bounds.bot_right.v += paginate_amt;
			++starts;
		}
		
		block->end_start.bounds.top_left.v += paginate_amt;
		block->end_start.bounds.bot_right.v += paginate_amt;
	}
	
	UnuseMemory(block->lines);
	uninit_rects(pg, &line_info);
	
	if (block->flags & SWITCHED_DIRECTIONS)
		pgAdjustDirection(pg, block);

	block->flags &= CLEAR_CALC;
	block->flags |= SOME_LINES_GOOD;

	pg->procs.paginate_proc(pg, block, update_info, shift_begin, shift_end, paginated_line_shift);
	pgCalcTableSpace(pg, block);
}


/* pgResetStartingLine gets called to reset the line information to where it was before
anything got calculated.  */

PG_PASCAL (void) pgResetStartingLine (pg_measure_ptr line_stuff)
{
	register pg_measure_ptr	line_info;
	pg_short_t				starting_offset, num_starts;

	line_info = line_stuff;
	
	line_info->starts->offset = line_info->old_offset;
	line_info->last_page = line_info->old_last_page;
	num_starts = line_info->num_starts;
	line_info->starts -= num_starts;
	line_info->starts_ctr += num_starts;
	starting_offset = line_info->starts->offset;

	line_info->char_locs = line_info->positions + starting_offset;
	line_info->char_types = line_info->types + starting_offset;
	line_info->previous_flags = line_info->prv_prv_flags;
	line_info->fit_rect.top_left.h = line_info->wrap_r_begin->top_left.h
		+ line_info->extra_indents.top_left.h;
	line_info->fit_rect.bot_right.h = line_info->wrap_r_begin->bot_right.h
		- line_info->extra_indents.bot_right.h;

	if (line_info->is_table) {
		
		line_info->fit_rect.top_left.h = line_info->cell_rect.top_left.h;
		line_info->fit_rect.bot_right.h = line_info->cell_rect.bot_right.h;
	}

	if (line_info->fit_rect.top_left.h < line_info->minimum_left)
		line_info->fit_rect.top_left.h = line_info->minimum_left;
	if (line_info->fit_rect.bot_right.h > line_info->maximum_right)
		line_info->fit_rect.bot_right.h = line_info->maximum_right;
}



/* RebuildTextblock is fairly major since it rebuilds all the point starts
that require re-adjusting. If the flags field of the block is "NEEDS_PAGINATE"
then current lines are adjusted, otherwise they are rebuilt from scratch.
NOTE: An assumption is made that no prior blocks require recalculation.
The update_info param, if non-NULL, is initialized with data for minimum
screen refresh of the updated text.	   */


PG_PASCAL (void) RebuildTextblock (paige_rec_ptr pg, text_block_ptr block,
		smart_update_ptr update_info)
{
	pg_measure					line_info;
	register point_start_ptr	starts;
	style_walk					walker;
	memory_ref					charloc_ref, char_types;
	long						text_length, abs_position, shift_begin, shift_end;
	short						post_paginate_verb;

	post_paginate_verb = paginated_full_block;	/* Default post-paginate verb */
	shift_begin = shift_end = block->begin;

	if (block->flags & LINES_PURGED) {

		block->flags |= NEEDS_CALC;
		block->flags &= (~SOME_LINES_GOOD);
	}
	
	pgFillBlock(&line_info, sizeof(pg_measure), 0);
	
	if (update_info)
		pgFillBlock(update_info, sizeof(smart_update), 0);
	
	block->cache_flags |= CACHE_LOCK_FLAG;
	pg->procs.load_proc(pg, block);

	pgPrepareStyleWalk(pg, block->begin, &walker, TRUE);

	line_info.styles = &walker;
	line_info.block = block;
	line_info.starts = starts = (point_start_ptr) UseMemory(block->lines);
	line_info.starts->offset = 0;
	line_info.starts_ctr = (pg_short_t)GetMemorySize(block->lines);

	line_info.char_locs = line_info.positions = pgGetSubrefCharLocs(pg, block, &charloc_ref, &char_types);
	line_info.char_types = line_info.types = (short *) UseMemory(char_types);
	line_info.starts_ref = block->lines;
	line_info.tab_info = MemoryAlloc(pg->globals->mem_globals, sizeof(tab_width_info), 4, 16);
	pgShapeBounds(pg->wrap_area, &line_info.wrap_bounds);

	SetMemoryPurge(line_info.tab_info, NO_PURGING_STATUS, FALSE);
	text_length = block->end - block->begin;
	
	if (!text_length || block_hidden(pg, block, &walker)) {
		
		init_starting_line_rect(pg, &line_info);
		build_empty_block(pg, &line_info);
		post_paginate_verb = paginated_empty_block;

		if (text_length) {
			
			block->bounds.bot_right = block->bounds.top_left;
			block->end_start.bounds.bot_right = block->end_start.bounds.top_left;
			block->flags |= ALL_TEXT_HIDDEN;
			// QUALCOMM: build_empty_block sets up two blocks - this block with LINE_BREAK_BIT
			// set and the next block with flags set to TERMINATOR_BITS.
			// This code was overwriting the flags value for the first block to include
			// LINE_HIDDEN_BIT, but was (I believe) incorrectly getting rid of LINE_BREAK_BIT.
			// Other Paige code relies on the block before the TERMINATOR_BITS having
			// LINE_BREAK_BIT set. If it's not set that code can continue traversing past the
			// end of the blocks into other memory. Luck sometimes won out and other times crashes
			// resulted.
			//
			// Changed the code here to also set LINE_BREAK_BIT to avoid the above problems.
			//line_info.starts->flags = NEW_LINE_BIT | NEW_PAR_BIT | LINE_HIDDEN_BIT | LINE_GOOD_BIT;
			line_info.starts->flags = NEW_LINE_BIT | NEW_PAR_BIT | LINE_HIDDEN_BIT | LINE_GOOD_BIT | LINE_BREAK_BIT;
			
			if (update_info)
				update_info->suggest_end = pg->t_length;
		}
	}
	else {

		if (!pg->active_subset && (block->flags & SOME_LINES_GOOD) && !(block->flags & NEEDS_PAGINATE)) {
		
			do_partial_rebuild(pg, &line_info, &shift_begin, &shift_end, update_info);
			post_paginate_verb = paginated_partial_block;
		}
		else {  /* full rebuild */
		
			if (update_info) {
				
				update_info->suggest_begin = block->begin;
				update_info->suggest_end = pg->t_length;
			}
			
			shift_end = block->end;
			pgFillBlock(&block->bounds, sizeof(rectangle), 0);		
			init_starting_line_rect(pg, &line_info);
			
			block->flags &= (~(BELOW_CONTAINERS | BLOCK_HAS_TABLE));

			if (starts_below_view(pg, &line_info)) {
			
				pgBuildFakeBlock(pg, &line_info);
				post_paginate_verb = paginated_fake_block;
			}
			else {

				if (pgShouldBreakPage(pg, &line_info, line_info.previous_flags, starts->offset)) {
				
					advance_line_rect(pg, &line_info);

					if (block->begin)
						if (update_info) {
						
						block -= 1;
						update_info->suggest_begin = block->begin;
						block += 1;
					}
				}
				
				while (line_info.starts->offset < (pg_short_t)text_length) {

					if (!pg->doc_info.max_chars_per_line)
						line_info.max_text_size = text_length - line_info.starts->offset;
					else
						line_info.max_text_size = pg->doc_info.max_chars_per_line;

					abs_position = block->begin;
					abs_position += line_info.starts->offset;
					pgSetWalkStyle(&walker, abs_position);
					
					pg->procs.line_init(pg, &line_info, new_line_verb);

					if (walker.cur_par_style->table.table_columns) {
						
						if (!line_info.is_table) {

							set_par_left_and_right(pg, &line_info, &line_info.fit_rect, line_info.starts);
							pgSetTableRect(pg, 0, &line_info);
						}
						else
							pgSetTableRect(pg, 1, &line_info);
					}
					else
					if (line_info.is_table)
						pgSetTableRect(pg, -1, &line_info);

					for (;;) {

						abs_position = block->begin;
						abs_position += line_info.starts->offset;
						pgSetWalkStyle(&walker, abs_position);
						
						walker.cur_par_style->procs.line_proc(pg, &line_info);

						if (pg->procs.validate_line(pg, &line_info))
							break;

			/* Line record is not acceptable, i.e., it overflows a container of sorts.
			  Hence I cannot break out of this loop yet.  The function validate_line
			  will have made a better guess at a more appropriate beginning rectangle
			  so I will try once again (above code).   */

						pgResetStartingLine(&line_info);
					}
					
					pgTrueUnionRect(&line_info.actual_rect, &block->bounds, &block->bounds);
					
					if (line_info.is_table) {
						
						if (pgEmptyRect(&line_info.table_union))
							line_info.table_union = line_info.actual_rect;
						else
							pgTrueUnionRect(&line_info.actual_rect, &line_info.table_union, &line_info.table_union);
					}
					
					advance_line_rect(pg, &line_info);		/* Advance to next rect */
				}
				
				finish_line_update(pg, &line_info);
				terminate_excess_starts (line_info.starts, line_info.starts_ctr);
			}
		}
	}

	uninit_rects(pg, &line_info);

	pgPrepareStyleWalk(pg, 0, NULL, TRUE);
	UnuseMemory(block->lines);
	UnuseMemory(charloc_ref);
	UnuseMemory(char_types);
	DisposeMemory(line_info.tab_info);
	pgReleaseCharLocs(pg, charloc_ref);
	
	block->flags &= CLEAR_CALC;
	block->flags |= SOME_LINES_GOOD;

	pg->procs.paginate_proc(pg, block, update_info, shift_begin, shift_end, post_paginate_verb);
	pgCalcTableSpace(pg, block);

	if (pg->flags & (COUNT_LINES_BIT | LINE_EDITOR_BIT))
		count_lines_and_pars(pg, block);

	block->lines = ForceMinimumMemorySize(block->lines);
	block->cache_flags &= (~CACHE_LOCK_FLAG);
}


/**********************************  Local Functions  **********************/


/* This function initializes the line count/par count in the_block. It only
gets called when COUNT_LINES_BIT is set in pg flags.  */

static void count_lines_and_pars (paige_rec_ptr pg, text_block_ptr the_block)
{
	register text_block_ptr		block;
	register point_start_ptr	starts;

	block = the_block;
	
	block->num_lines = block->num_pars = 0;
	block->first_line_num = block->first_par_num = 0;
	
	if (block->begin) {
		
		--block;
		block[1].first_line_num = block->first_line_num + (long) block->num_lines;
		block[1].first_par_num = block->first_par_num + (long) block->num_pars;
		++block;
	}

	if ((block->end == block->begin) || (block->flags & ALL_TEXT_HIDDEN))
		return;

	block->flags &= (~NEEDS_PARNUMS);

	if (block->flags & (SOME_LINES_BAD | NEEDS_CALC)) {

		block->num_pars = pgCountCtlChars(block, pg->globals->line_wrap_char);
		block->num_lines = block->num_pars;

		return;
	}

	starts = (point_start_ptr) UseMemory(block->lines);
	
	while (starts->flags != TERMINATOR_BITS) {
		
		if (starts->flags & (NEW_LINE_BIT | NEW_PAR_BIT))
			if (!(starts->flags & LINE_HIDDEN_BIT))
				if (starts->offset < starts[1].offset) {
				
			++block->num_lines;
			
			if (starts->flags & NEW_PAR_BIT)
				++block->num_pars;
		}
		
		++starts;
	}

	UnuseMemory(block->lines);
}


/* starts_below_view returns TRUE if the starting line number begins below the
last "container." This test is important so a block can be built quickly if it
isn't even invisible. */

static pg_boolean starts_below_view (paige_rec_ptr pg, pg_measure_ptr line_stuff)
{
	pg_measure_ptr		line_info;
	
	if (!(pg->doc_info.attributes & BOTTOM_FIXED_BIT))
		return	FALSE;

	line_info = line_stuff;

	return ( (!line_info->repeating) && (line_info->r_num_begin == line_info->end_r)
		&& (line_info->fit_rect.top_left.v > line_info->wrap_r_save)
		&& (!(pg->doc_info.attributes & NO_CLIP_PAGE_AREA)) );
}


/* finish_line_update gets called by both RebuildTextblock and do_partial_rebuild
to terminate the block's ending. It only gets called if the block paginated all
the way to the end. */

static void finish_line_update (paige_rec_ptr pg, pg_measure_ptr line_stuff)
{
	register pg_measure_ptr		line_info;
	register point_start_ptr	starts;
	text_block_ptr				block;
	long						abs_position;

	line_info = line_stuff;
	block = line_info->block;
	starts = line_info->starts;
	--starts;
	abs_position = (long)starts->offset;
	abs_position += block->begin;

	if ((block->end < pg->t_length) && (!(line_info->previous_flags & HARD_BREAK_BITS))) {

		starts = line_info->starts;
		--starts;
		starts->flags |= NO_LINEFEED_BIT;
		block->flags |= NO_CR_BREAK;
	}
	else
	if ((line_info->previous_flags & HARD_BREAK_BITS) && block->end == pg->t_length) {

		pgSetWalkStyle(line_info->styles, pg->t_length);
		line_info->max_text_size = 0;
		line_info->styles->cur_par_style->procs.line_proc(pg, line_info);

		starts = line_info->starts;
		--starts;

		abs_position = block->begin;
		abs_position += starts->offset;
		pgSetWalkStyle(line_info->styles, abs_position);
		
		pgAcceptEmptyLine(pg, starts, line_info);

		pgTrueUnionRect(&line_info->actual_rect, &block->bounds, &block->bounds);
	}

	starts = line_info->starts;
	starts->flags = TERMINATOR_BITS;

	--starts;

	starts->flags |= LINE_BREAK_BIT;

	starts[1].bounds.top_left.h = starts[1].bounds.bot_right.h
			= starts->bounds.bot_right.h;
	starts[1].bounds.top_left.v = starts[1].bounds.bot_right.v
			= starts->bounds.bot_right.v;
	
	while ((starts->flags & LINE_HIDDEN_BIT) && starts->offset)
		--starts;

	pgBlockMove(starts, &block->end_start, sizeof(point_start));
	block->end_start.flags |= LINE_BREAK_BIT;
	if ((block->end != pg->t_length) && (!(block->flags & NO_CR_BREAK)))
		block->end_start.flags |= PAR_BREAK_BIT;
}


/* This function figures out the first rectangle to be looking at in the wrap
shape of pg and initializes the appropriate fields in line_info. THIS INCLUDES
a UseMemory on pg->wrap_area. NOTE: The line_stuff->starts MUST point to the
first start you intend to calculate.  */

static void init_starting_line_rect (paige_rec_ptr pg, pg_measure_ptr line_stuff)
{
	register pg_measure_ptr		line_info;
	long						r_qty;
	long						start_v;
	
	line_info = line_stuff;
	line_info->wrap_dimension = pgShapeDimension(pg->wrap_area) & UNION_DIMENSION;
	line_info->repeating = ((pg->doc_info.attributes & (V_REPEAT_BIT | H_REPEAT_BIT)) != 0);
	line_info->wrap_r_begin = (rectangle_ptr) UseMemory(pg->wrap_area);
	line_info->end_r = line_info->subpage_qty = r_qty = GetMemorySize(pg->wrap_area);
	line_info->subpage_qty -= 1;
	line_info->end_r -= 2;
	line_info->wrap_r_save = line_info->wrap_r_begin[r_qty - 1].bot_right.v;
	
	if (!line_info->repeating) {
		long		infinite_bottom;
		
		line_info->subpage_qty = 0;
		infinite_bottom = GOD_AWFUL_HUGE - (line_info->wrap_r_begin[r_qty - 1].bot_right.v
					- line_info->wrap_r_begin->top_left.v);
		line_info->wrap_r_begin[r_qty - 1].bot_right.v = infinite_bottom;
	}

	++line_info->wrap_r_begin;			/* Begins after bounds rect */
	line_info->wrap_r_base = line_info->wrap_r_begin;

	start_v = set_last_good_start(line_info);

	line_info->wrap_r_begin += pgGetWrapRect(pg, line_info->r_num_begin,
				&line_info->repeat_offset);
	
	line_info->last_page = -1;

	if (!assure_minimum_width(pg, line_info))
		start_v = line_info->wrap_r_begin->top_left.v;

	line_info->r_num_end = line_info->r_num_begin;
	line_info->wrap_r_end = line_info->wrap_r_begin;

	set_fit_rect(pg, line_info, FALSE);
	
	if (line_info->starts->offset == 0 && line_info->block->begin == 0)
		start_v += line_info->extra_indents.top_left.v;

	line_info->fit_rect.top_left.v = start_v;
	if ((line_info->previous_flags & NO_LINEFEED_BIT) || (!(line_info->previous_flags & LINE_BREAK_BIT)))
		line_info->fit_rect.top_left.h = line_info->prev_bounds.bot_right.h;

	line_info->exclude_ref = MemoryAlloc(pg->globals->mem_globals, sizeof(rectangle), 1, 16);

	if (line_info->wrap_dimension)
		setup_union_line_rect(pg, line_info, TRUE);
	
	line_info->minimum_left = line_info->wrap_r_begin->top_left.h;
	line_info->maximum_right = line_info->wrap_r_begin->bot_right.h;

	start_v = line_info->block->begin;
	start_v += line_info->starts->offset;

	pg->procs.line_init(pg, line_info, init_measure_verb);
}


/* Call this once you are through using a pg_measure. This restores the original
wrap bottom and does an UnuseMemory on pg->wrap_area.  */

static void uninit_rects (paige_rec_ptr pg, pg_measure_ptr line_stuff)
{
	rectangle_ptr			end_of_wrap;
	
	end_of_wrap = (rectangle_ptr) UseMemoryRecord(pg->wrap_area, line_stuff->end_r + 1, 0, FALSE);
	end_of_wrap->bot_right.v = line_stuff->wrap_r_save;
	
	UnuseMemory(pg->wrap_area);
	DisposeMemory(line_stuff->exclude_ref);

	pg->procs.line_init(pg, line_stuff, done_measure_verb);
}


/* This function builds an empty (no text) block. Note this only happens when
the entire pg_ref has no text and we want to "build" the first block. */

static void build_empty_block (paige_rec_ptr pg, pg_measure_ptr line_stuff)
{
	register style_info_ptr		styles;
	register pg_measure_ptr		line_info;
	style_info					imposed_style;
	style_walk_ptr				walker;
	point_start_ptr				starts;
	long						text_height;
	
	line_info = line_stuff;
	walker = line_info->styles;
	styles = walker->cur_style;
	starts = line_info->starts;

	if (styles->styles[super_impose_var]) {
		
		pgStyleSuperImpose(pg, styles, &imposed_style, NULL, 0);
		styles = &imposed_style;
	}

	starts->flags = NEW_LINE_BIT | NEW_PAR_BIT | LINE_GOOD_BIT | LINE_BREAK_BIT;
	starts->offset = 0;
	starts->r_num = 0;

	set_par_left_and_right(pg, line_stuff, &line_info->fit_rect, starts);

	starts->bounds.top_left = starts->bounds.bot_right = line_info->fit_rect.top_left;
	starts->bounds.bot_right.h = starts->bounds.top_left.h;

	text_height = styles->ascent + styles->descent + styles->leading
					+ styles->top_extra + styles->bot_extra;
	starts->bounds.bot_right.v += text_height;
	starts->baseline = (short)(styles->descent + styles->leading + styles->bot_extra);
	
	//pgOffsetRect(&starts->bounds, line_info->extra_indents.top_left.h,
	//		 line_info->extra_indents.top_left.v),

	line_info->block->bounds = starts->bounds;
	
	starts[1].offset = 0;
	starts[1].r_num = 0;
	starts[1].flags = TERMINATOR_BITS;
	
	pg->procs.line_init(pg, line_info, new_line_verb);
	pg->procs.adjust_proc(pg, line_info, starts, 1, &line_info->fit_rect,
			walker->cur_par_style);
	
	if (pgPrimaryTextDirection(pg->globals, walker->cur_par_style) == right_left_direction) {
		rectangle		wrap_bounds;
		
		pgShapeBounds(pg->wrap_area, &wrap_bounds);
		pgAdjustRightLeftStarts(pg, &wrap_bounds, right_left_direction, starts, 1);
	}
	
	pgAcceptEmptyLine(pg, starts, line_info);
	pgBlockMove(starts, &line_info->block->end_start, sizeof(point_start));
}



/* This function updates the line's vertical position(s) according to the
latest font info. If necessary, the previous point starts are adjusted so the
line "fits" correctly on a straight co-ordinate row.
The subref param is reserved for DSI, used for special subref computations. */

static void update_vertical_line (paige_rec_ptr pg, pg_measure_ptr line_info,
		short PG_FAR *last_ascent, short PG_FAR *last_descent,
		short PG_FAR *last_leading, point_start_ptr starts, pg_short_t num_starts,
		rectangle_ptr must_fit, memory_ref subref)
{ 
	register style_info_ptr	style;
	register par_info_ptr	par_style;
	point_start_ptr			old_starts;
	style_info				imposed_style;
	style_walk_ptr			walker;
	short					old_line_height, new_line_height;
	short					ascent, descent, leading, v_change;
	short					super_extra, sub_extra;
	short					cell_top_extra, cell_bot_extra;

	walker = line_info->styles;
	style = walker->cur_style;
	
	if ((pg->flags & NO_HIDDEN_TEXT_BIT) && style->styles[hidden_text_var])
		return;

	if (style->styles[super_impose_var]) {
		
		pgStyleSuperImpose(pg, style, &imposed_style, NULL, 0);
		style = &imposed_style;
	}
	else
	if (style->styles[relative_point_var] || style->styles[nested_subset_var]) {
		long			point_size;

		pgBlockMove(style, &imposed_style, sizeof(style_info));
		point_size = pgComputePointSize(pg, &imposed_style);
		point_size <<= 16;
		imposed_style.point = point_size;
		imposed_style.styles[relative_point_var] = imposed_style.styles[nested_subset_var] = 0;
		imposed_style.procs.init(pg, &imposed_style, walker->cur_font);
		style = &imposed_style;
	}

	par_style = walker->cur_par_style;
	ascent = (short)(style->ascent + style->top_extra);
	descent = style->descent;
	
	cell_top_extra = (short) line_info->cell_extra.top_left.v;
	cell_bot_extra = (short) line_info->cell_extra.bot_right.v;

	if (cell_top_extra || cell_bot_extra)
		if (num_starts) {
		
		old_starts = starts;
		--old_starts;
		
		if (!(old_starts->flags & TAB_BREAK_BIT | PAR_BREAK_BIT))
			cell_bot_extra = 0;
		
		while (!(old_starts->flags & NEW_LINE_BIT) && old_starts->offset)
			--old_starts;
		
		if (old_starts->offset) {
			
			--old_starts;
			
			if (!(old_starts->flags & NEW_LINE_BIT | PAR_BREAK_BIT))
				cell_top_extra = 0;
		}
	}
	
	ascent += (short)cell_top_extra;
	descent += (short)cell_bot_extra;

#ifdef PG_BASEVIEW
	if ((leading = (short)(style->leading + style->bot_extra + pgHiWord(par_style->leading_extra))) < 0)
		leading = 0;
#else
	if ((leading = (short)(style->leading + style->bot_extra + par_style->leading_extra)) < 0)
		leading = 0;
#endif

	if (style->bot_extra < 0)
		if ((descent += (short)style->bot_extra) < 0)
			descent = 0;
	
	if (super_extra = style->styles[superscript_var]) {
		
		if (style->shift_verb == percent_of_style)
			super_extra = ((ascent + descent) * super_extra) / 100;
	}

	if (sub_extra = style->styles[subscript_var]) {
		
		if (style->shift_verb == percent_of_style)
			sub_extra = ((ascent + descent) * sub_extra) / 100;
	}

	old_line_height = *last_ascent + *last_descent + *last_leading;
	
	v_change = 0;

	if (subref) {
		paige_sub_ptr			sub_ptr;
		
		sub_ptr = (paige_sub_ptr) UseMemory(subref);
		sub_extra += sub_ptr->nested_shift_v;

		if (sub_ptr->alignment_flags & SUBREF_SUBSCRIPT_ALIGN) {
		
			sub_extra += sub_ptr->descent;
			
			if (sub_ptr->home_sub) {
				paige_sub_ptr		home_ptr;
				
				home_ptr = (paige_sub_ptr) UseMemory(sub_ptr->home_sub);
				home_ptr->nested_shift_v = sub_extra;
				UnuseMemory(sub_ptr->home_sub);
			}
		}

		if (sub_ptr->ascent > ascent)
			ascent = sub_ptr->ascent;
		if ((sub_ptr->descent + sub_extra) > descent)
			descent = sub_ptr->descent + sub_extra;
		
		UnuseMemory(subref);
	}

	if (ascent > *last_ascent)
		*last_ascent = ascent;
	else
		ascent = *last_ascent;

	if (descent > *last_descent) {
		
		v_change += (descent - *last_descent);
		*last_descent = descent;
	}
	else
		descent = *last_descent;

	if (leading > *last_leading) {
		
		v_change += (leading - *last_leading);
		*last_leading = leading;
	}
	else
		leading = *last_leading;

	new_line_height = ascent + descent + leading;
	
	//ее TRS/OITC Variable line spacing
	if (par_style->leading_variable > new_line_height) {
		new_line_height = (short)par_style->leading_variable;
		leading = 0;
	}
	else
	if (par_style->leading_fixed) {

#ifdef PG_BASEVIEW
		short			new_height;
		
		new_height = pgHiWord(par_style->leading_fixed);
		
		if (new_height > new_line_height)
			new_line_height = new_height;
#else
		if ((short)par_style->leading_fixed > new_line_height)
			new_line_height = (short)par_style->leading_fixed;
#endif

	}

	if (new_line_height < old_line_height)
		if (!par_style->leading_fixed)
			new_line_height = old_line_height;

	must_fit->bot_right.v = must_fit->top_left.v + new_line_height;

	if ((v_change || (new_line_height != old_line_height)) && num_starts) {
		
		old_starts = starts;
		
		while (num_starts) {
			
			--old_starts;
			old_starts->bounds.bot_right.v = must_fit->bot_right.v;
			old_starts->baseline += v_change;
			--num_starts;
		}
	}
	
	starts->baseline = leading + descent + super_extra - sub_extra;
	starts->bounds.bot_right.v = must_fit->bot_right.v;
}

		
/* This function sets up the left and right edges for the line (left and right
indents, etc.  */

static void set_par_left_and_right (paige_rec_ptr pg, pg_measure_ptr line_info, rectangle_ptr fit_rect,
		point_start_ptr first_start)
{
	register rectangle_ptr		wrap;
	register par_info_ptr		par_style;
	style_walk_ptr				walker;
	long						first_left_extra = 0;
	long						left_extra = 0;
	long						right_extra = 0;
	long						html_style;
	pg_short_t					first_line;
	
	walker = line_info->styles;
	par_style = walker->cur_par_style;
	wrap = fit_rect;
	first_line = first_start->flags & NEW_PAR_BIT;
	
	if (!line_info->is_table) {
	
		left_extra = par_style->left_extra + par_style->indents.left_indent;
		right_extra = par_style->indents.right_indent + par_style->right_extra;
		
		if (!par_style->table.table_columns) {
		
			first_left_extra += par_style->indents.first_indent;
			pgAdjustBorders (par_style, &left_extra, &right_extra);
		}
		else
		if (par_style->indents.first_indent < 0)
			left_extra += par_style->indents.first_indent;
	}
	
	html_style = par_style->html_style & 0x0000FFFF;

	if (html_style)
		if (html_style == html_unordered_list || html_style == html_directory || html_style == html_menu)
			left_extra += (pg->globals->bullet_size + 5);

	if (left_extra < 0)
		left_extra = 0;
	if (right_extra < 0)
		right_extra = 0;
	
	wrap->top_left.h += left_extra;

	if ((first_start->offset == 0) || (first_line))
		wrap->top_left.h += first_left_extra;

	wrap->bot_right.h -= right_extra;
}

/* This function increments the point_start pointer in line_info, decrementing
the starts_ctr value and, if it goes to zero, appends additional start records.
On entry, the starts parameter should be the current starts and the function will
return the correct updated pointer.  */

static point_start_ptr extend_starts (pg_measure_ptr line_info, point_start_ptr starts)
{
	line_info->starts = starts;
	
	if (line_info->num_starts)
		if (starts->bounds.top_left.h > starts->bounds.bot_right.h) {
		long		swap_h;
		
		swap_h = starts->bounds.bot_right.h;
		starts->bounds.bot_right.h = starts->bounds.top_left.h;
		starts->bounds.top_left.h = swap_h;
	}
	
	++line_info->starts;
	++line_info->num_starts;

	if (!(--line_info->starts_ctr)) {
		
		line_info->starts = (point_start_ptr) AppendMemory(line_info->starts_ref, START_APPEND_SIZE, TRUE);
		line_info->starts_ctr = START_APPEND_SIZE;
	}
	
	line_info->starts->cell_num = line_info->cell_index;
	line_info->starts->cell_height = 0;

	return	line_info->starts;
}


/* This gets called when the top of line_stuff->fit_rect has been changed due
to inability to fit outside of current exclusion(s). In this case it is possible
that the new line top is in a different rectangle. This handles that situation. */

static void assure_r_num_correct (paige_rec_ptr pg, pg_measure_ptr line_stuff)
{
	register pg_measure_ptr				line_info;
	register rectangle_ptr				wrap_r;
	long								r_bottom;

	line_info = line_stuff;
	line_info->actual_rect.top_left.v = line_info->actual_rect.bot_right.v
			= line_info->fit_rect.top_left.v;

	for (;;) {
	
		wrap_r = line_info->wrap_r_end;
		r_bottom = wrap_r->bot_right.v + line_info->repeat_offset.v;
		r_bottom -= line_info->extra_indents.bot_right.v;
		
		if (r_bottom > line_info->fit_rect.top_left.v)
			break;
		
		if (line_info->r_num_begin == line_info->end_r)
			break;

		advance_line_rect(pg, line_info);
	}
}


/* This function sets up the probable "next rectangle" for line calculations,
advancing the information in line_stuff as necessary.  */

static void advance_line_rect (paige_rec_ptr pg, pg_measure_ptr line_stuff)
{
	register pg_measure_ptr			line_info;
	long							actual_limit_v, vertical_line_position;
	long							abs_offset;
	pg_short_t						new_rect_index;

	line_info = line_stuff;

	line_info->r_num_begin = line_info->r_num_end;
	line_info->wrap_r_begin = line_info->wrap_r_end;
	actual_limit_v = line_info->wrap_r_begin->bot_right.v + line_info->repeat_offset.v
			- line_info->extra_indents.bot_right.v;
	vertical_line_position = line_info->actual_rect.bot_right.v;
	abs_offset = line_info->starts->offset;
	abs_offset += line_info->block->begin;

	if (abs_offset == pg->t_length)
		vertical_line_position += (line_info->actual_rect.bot_right.v - line_info->actual_rect.top_left.v);
	
	if ( (pgShouldBreakPage(pg, line_info, line_info->previous_flags, line_info->starts->offset))
		&& ( line_info->repeating || (line_info->r_num_begin < line_info->end_r) ) ) {

		for (;;) {
		
			++line_info->r_num_begin;
			new_rect_index = pgGetWrapRect(pg, line_info->r_num_begin, &line_info->repeat_offset);
			
			if ((line_info->previous_flags & BREAK_CONTAINER_BIT)
				|| (!line_info->repeating) || (!new_rect_index))
				break;
		}

		line_info->wrap_r_begin = line_info->wrap_r_base;
		line_info->wrap_r_begin += new_rect_index;
		
		assure_minimum_width(pg, line_info);
		set_fit_rect(pg, line_info, TRUE);
	}
	else
	if ((actual_limit_v > vertical_line_position)
		|| (pg->doc_info.attributes & NO_CONTAINER_JMP_BIT)) {

		line_info->fit_rect.top_left.v = line_info->actual_rect.bot_right.v;
		line_info->fit_rect.bot_right.v = actual_limit_v;
	}
	else {	/* Next line would flow past the current rectangle.  */
		
		++line_info->r_num_begin;
		line_info->wrap_r_begin = line_info->wrap_r_base;
		line_info->wrap_r_begin += pgGetWrapRect(pg, line_info->r_num_begin,
				&line_info->repeat_offset);
		assure_minimum_width(pg, line_info);
		set_fit_rect(pg, line_info, TRUE);
	}

	line_info->r_num_end = line_info->r_num_begin;
	line_info->wrap_r_end = line_info->wrap_r_begin;

	line_info->fit_rect.top_left.h = line_info->wrap_r_begin->top_left.h
		+ line_info->extra_indents.top_left.h;
	line_info->fit_rect.bot_right.h = line_info->wrap_r_begin->bot_right.h
		- line_info->extra_indents.bot_right.h;

	pgOffsetRect(&line_info->fit_rect, line_info->repeat_offset.h, 0);

	if (line_info->wrap_dimension)
		setup_union_line_rect(pg, line_info, TRUE);

	line_info->minimum_left = line_info->wrap_r_begin->top_left.h;
	line_info->maximum_right = line_info->wrap_r_begin->bot_right.h;
}


/* This function makes sure the current rect is equal or greater than the
designated line width minimum (which is defined in pg globals). If the
rectangles had to be advanced (due to too small a width), the function
returns FALSE.   */

static pg_boolean assure_minimum_width (paige_rec_ptr pg, pg_measure_ptr line_stuff)
{
	register pg_measure_ptr		line_info;
	long						minimum;
	pg_short_t					begin_r;

	line_info = line_stuff;
	minimum = pg->globals->minimum_line_width;
	begin_r = (pg_short_t)line_info->r_num_begin;

	while (line_info->r_num_begin < line_info->end_r) {

		if ((line_info->wrap_r_begin->bot_right.h
					- line_info->wrap_r_begin->top_left.h) < minimum) {

			++line_info->r_num_begin;
			line_info->wrap_r_begin = line_info->wrap_r_base + pgGetWrapRect(pg,
					line_info->r_num_begin, &line_info->repeat_offset);
		}
		else
			break;
	}
	
	return (pg_boolean)(begin_r == (pg_short_t)line_info->r_num_begin);
}


/* This function gets called to set up the most probable series of rectangles
for a complex shape (in which rects are touching). By "most probable" is meant
the likely line height, assuming the same point size and leading will run
throughout. This gets called when the first line is set up and when the next
line is advanced. If compute_new_height is FALSE the function uses the current
line height (line_stuff->fit_rect's height) instead of computing a likely value. */

static void setup_union_line_rect (paige_rec_ptr pg, pg_measure_ptr line_stuff,
		short compute_new_height)
{
	register pg_measure_ptr	line_info;
	point_start_ptr			starts;
	long					line_height;
	short					ascent, descent, leading;

	line_info = line_stuff;
	
	if (line_info->r_num_begin == line_info->end_r)
		return;
	
	if (compute_new_height) {
	
		starts = line_info->starts;
		ascent = descent = leading = 0;
	
		starts->baseline = 0;
		line_info->fit_rect.bot_right.v = line_info->fit_rect.top_left.v;
		pgBlockMove(&line_info->fit_rect, &starts->bounds, sizeof(rectangle));
		
		update_vertical_line(pg, line_info, &ascent, &descent, &leading,
				starts, 0, &line_info->fit_rect, MEM_NULL);
	}

	line_info->wrap_r_end = line_info->wrap_r_begin;
	line_height = line_info->fit_rect.bot_right.v - line_info->fit_rect.top_left.v;

	for (;;) {
		
		line_info->r_num_end = line_info->r_num_begin;
		if (line_info->r_num_begin == line_info->end_r)
			break;
		
		if (pg->doc_info.attributes & NO_CONTAINER_JMP_BIT)
			break;

		if (pgMaxRectInShape(pg->wrap_area, &line_info->fit_rect,
				pg->globals->minimum_line_width, &line_info->r_num_end,
				&line_info->repeat_offset))
			break;
		
		++line_info->r_num_begin;
		line_info->wrap_r_begin = line_info->wrap_r_base + pgGetWrapRect(pg,
				line_info->r_num_begin, &line_info->repeat_offset);
		set_fit_rect(pg, line_info, TRUE);
		line_info->fit_rect.bot_right.v = line_info->fit_rect.top_left.v + line_height;
	}

	line_info->wrap_r_end = line_info->wrap_r_base + pgGetWrapRect(pg,
				line_info->r_num_end, &line_info->repeat_offset);
}


/* This short function moves line_info->wrap_r_begin to line_info->fit_rect
then offsets it by line_info->repeat_offset amounts. */

static void set_fit_rect (paige_rec_ptr pg, pg_measure_ptr line_info, pg_boolean top_line_in_page)
{
	line_info->fit_rect = *(line_info->wrap_r_begin);
	pgOffsetRect(&line_info->fit_rect, line_info->repeat_offset.h, line_info->repeat_offset.v);
	
	set_extra_indents(pg, line_info->r_num_begin, line_info, top_line_in_page);
}


/* The following function calls the line_adjust proc for unadjusted_starts
prior to the starts in line_info. This used to exist within the line measure
proc but has been taken out for clarity of code. It also makes adjust_rect's
bottom match up with line_info->actual_rect's bottom. */

static void adjust_line_starts (paige_rec_ptr pg, pg_measure_ptr line_stuff,
		rectangle_ptr adjust_rect, pg_short_t unadjusted_starts,
		par_info_ptr line_par_format)
{
	register pg_measure_ptr	line_info;
	point_start_ptr			starts;

	line_info = line_stuff;
	starts = line_info->starts;
	starts -= unadjusted_starts;
	
	adjust_rect->bot_right.v = line_info->actual_rect.bot_right.v;
	
	pg->procs.adjust_proc(pg, line_stuff, starts, unadjusted_starts,
			adjust_rect, line_par_format);
	
	line_info->actual_rect.bot_right.v = adjust_rect->bot_right.v;
}



/* This function looks for the previous point_start info and sets the_info
accordingly. This can be as simple as getting the last start, but as complex as
backing up many blocks due to massive hidden text. The function returns the
vertical position to start the line. */

static long set_last_good_start (pg_measure_ptr the_info)
{
	register pg_measure_ptr		line_info;
	register point_start_ptr	starts;
	register text_block_ptr		block;

	line_info = the_info;
	starts = line_info->starts;
	block = line_info->block;

/* Step 1: See if previous start exists and, if so, back up to first visible start */

	if (starts->offset) {
		
		while (starts->offset) {
			
			--starts;
			if (!(starts->flags & LINE_HIDDEN_BIT)) {
				
				++starts;
				break;
			}
		}
	}

/* Step 2: See if last start is in previous text block and if so, grab the
"end_start" field. */

	if (!starts->offset) {
		
		if (!block->begin) {

			line_info->r_num_begin = 0;
			line_info->previous_flags = PAR_BREAK_BIT | LINE_BREAK_BIT;
			pgBlockMove(line_info->wrap_r_begin, &line_info->prev_bounds, sizeof(rectangle));
			line_info->prev_bounds.bot_right.v = line_info->prev_bounds.top_left.v;
		}
		else {
			
			while (block->begin) {
				
				--block;
				if (!(block->flags & ALL_TEXT_HIDDEN))
					break;
			}
	
	if (block->end_start.cell_num)
		line_info->r_num_begin = 0;
		
			line_info->r_num_begin = block->end_start.r_num;
			line_info->previous_flags = block->end_start.flags;
			line_info->prev_bounds = block->end_start.bounds;
			line_info->prev_bounds.bot_right.v += (long)block->end_start.cell_height;
		}
	}
	else {
		
		--starts;
		line_info->previous_flags = starts->flags;
		line_info->r_num_begin = starts->r_num;

		if (starts->cell_num && (starts->flags & TAB_BREAK_BIT)) {
			
			while (starts->offset) {
				
				--starts;
				
				if (starts->flags & PAR_BREAK_BIT)
					break;
			}
			
			line_info->prev_bounds = starts->bounds;
			
			if (!starts->offset && !(starts->flags & PAR_BREAK_BIT))
				line_info->prev_bounds.bot_right.v = line_info->prev_bounds.top_left.v;
		}
		else {
		
			line_info->prev_bounds = starts->bounds;
			line_info->prev_bounds.bot_right.v += starts->cell_height;
		}
	}

	line_info->r_num_end = line_info->r_num_begin;

	if (line_info->previous_flags & NO_LINEFEED_BIT)
		return	line_info->prev_bounds.top_left.v;

	return	 line_info->prev_bounds.bot_right.v;
}


/* This function returns TRUE if whole block is hidden from hidden text. */

static pg_boolean block_hidden (paige_rec_ptr pg, text_block_ptr block,
		style_walk_ptr walker)
{
	style_walk			copy_of_walker;

	if (!(pg->flags & NO_HIDDEN_TEXT_BIT))
		return	FALSE;
	
	if (!walker->cur_style->styles[hidden_text_var])
		return	FALSE;
	
	pgBlockMove(walker, &copy_of_walker, sizeof(style_walk));
	
	while (copy_of_walker.current_offset < block->end) {
		
		if (!pgWalkNextStyle(&copy_of_walker))
			break;
		
		if (copy_of_walker.current_offset >= block->end)
			break;
		
		if (!copy_of_walker.cur_style->styles[hidden_text_var])
			return	FALSE;
	}
	
	return	TRUE;
}


/* paginate_multi_rect_shape is called whan I can just "paginate" (move lines
around) but the wrap area is multi-rectangles. However, it is known at this time
that all rects are of the same width and none overlap. */

static void paginate_multi_rect_shape (paige_rec_ptr pg, text_block_ptr block,
		pg_short_t initial_offset, long PG_FAR *shift_begin, long PG_FAR *shift_end,
		pg_measure_ptr line_stuff)
{
	register pg_measure_ptr		line_info;
	register point_start_ptr	starts;
	register long				adjust_v, adjust_h;
	register pg_short_t			num_starts;
	pg_short_t					ending_flags;
	rectangle					old_bounds, new_bounds;
	pg_boolean					next_quick_paginate;
	long						cur_r, cur_start_r;
	
	line_info = line_stuff;
	line_info->quick_paginate = next_quick_paginate = TRUE;

	starts = line_info->starts;

	pgFillBlock(&block->bounds, sizeof(rectangle), 0);

	if (initial_offset) {
	
		while (starts->offset < initial_offset) {
			
			pgLineUnionRect(starts, &block->bounds);
	
			--line_info->starts_ctr;
			++starts;
			
			if (starts->flags == TERMINATOR_BITS)
				break;
		}
		
		line_info->starts = starts;
		line_info->r_num_begin = starts->r_num;
	}
	
	cur_r = line_info->r_num_begin;
	cur_start_r = starts->r_num;

	convert_wrap_rect(pg, cur_r, line_info, &new_bounds, NULL, FALSE);
	convert_wrap_rect(pg, cur_start_r, line_info, &old_bounds, NULL, (pg_boolean)(cur_start_r != cur_r));
	
	if (initial_offset) {
	
		line_info->fit_rect = new_bounds;
		*shift_begin = (long)line_info->starts->offset;
		*shift_begin += block->begin;
		*shift_end = *shift_begin;
	}
	
	adjust_h = new_bounds.top_left.h - old_bounds.top_left.h;
	
	if (starts->offset == 0) {
		
		ending_flags = PAR_BREAK_BIT;
		
		if (pgShouldBreakPage(pg, line_info, ending_flags, starts->offset))
			next_quick_paginate = FORCED_QUICK_BREAK;
	}

	while (starts->flags != TERMINATOR_BITS) {
		
		line_info->starts = starts;
		
		for (num_starts = 1; !(starts->flags & LINE_BREAK_BIT); ++starts, ++num_starts) ;
		
		if ((ending_flags = starts->flags) == TERMINATOR_BITS)
			ending_flags = 0;

		line_info->num_starts = num_starts;
		starts = line_info->starts;
		
		if (starts->r_num != cur_start_r) {
			
			cur_start_r = starts->r_num;
			convert_wrap_rect(pg, cur_start_r, line_info, &old_bounds, NULL, TRUE);
			adjust_h = new_bounds.top_left.h - old_bounds.top_left.h;
		}
		
		adjust_v = line_info->fit_rect.top_left.v - starts->bounds.top_left.v;
		
		if (adjust_v)
			if (*shift_begin == *shift_end) {
				
				*shift_begin = (long)starts->offset;
				*shift_begin += block->begin;
				*shift_end = block->end;
		}

		line_info->actual_rect = line_info->fit_rect;
		line_info->actual_rect.top_left.h = starts->bounds.top_left.h + adjust_h;
		line_info->actual_rect.bot_right.h = starts[num_starts - 1].bounds.bot_right.h + adjust_h;
		line_info->actual_rect.bot_right.v = line_info->actual_rect.top_left.v
				+ (starts->bounds.bot_right.v - starts->bounds.top_left.v);
		
		while (num_starts) {
			
			pgOffsetRect(&starts->bounds, adjust_h, adjust_v);
			starts->r_num = cur_r;

			++starts;
			--num_starts;
		}
		
		line_info->starts = starts;
		line_info->quick_paginate = next_quick_paginate;

		if (pgShouldBreakPage(pg, line_info, ending_flags, starts->offset))
			next_quick_paginate = FORCED_QUICK_BREAK;
		else
			next_quick_paginate = TRUE;

		while (!pg->procs.validate_line(pg, line_info)) {
			
			line_info->old_offset = line_info->starts->offset;
			starts = line_info->starts;
			pgResetStartingLine(line_info);
			line_info->starts = starts;
			num_starts = line_info->num_starts;
			starts -= num_starts;

			adjust_h = new_bounds.top_left.h;

			++cur_r;
			
			set_extra_indents(pg, cur_r, line_info, TRUE);
			line_info->r_num_begin = line_info->r_num_end = cur_r;
			line_info->wrap_r_end = line_info->wrap_r_base;
			line_info->wrap_r_end += convert_wrap_rect(pg, cur_r, line_info,
					&new_bounds, &line_info->repeat_offset, TRUE);
			
			adjust_h = new_bounds.top_left.h - adjust_h;
			adjust_v = new_bounds.top_left.v - starts->bounds.top_left.v;
			pgOffsetRect(&line_info->actual_rect, adjust_h, adjust_v);

			while (num_starts) {
				
				pgOffsetRect(&starts->bounds, adjust_h, adjust_v);
				starts->r_num = cur_r;
				
				++starts;
				--num_starts;
			}
			
			adjust_h = new_bounds.top_left.h - old_bounds.top_left.h;
			
			if (line_info->quick_paginate == FORCED_QUICK_BREAK) {
				
				if (line_info->subpage_qty == 1 || (ending_flags & BREAK_CONTAINER_BIT)
					|| ((cur_r % line_info->subpage_qty) == 0))
					line_info->quick_paginate = TRUE;
			}
		}

		line_info->quick_paginate = TRUE;

		line_info->fit_rect.top_left.v = line_info->fit_rect.bot_right.v
				= line_info->actual_rect.bot_right.v;
		
		starts = line_info->starts;
		num_starts = line_info->num_starts;
		starts -= num_starts;
		
		while (num_starts) {
			
			pgLineUnionRect(starts, &block->bounds);
			++starts, --num_starts;
		}
	}
	
	if (starts->offset)
		--starts;
	
	pgBlockMove(starts, &block->end_start, sizeof(point_start));
}



/* convert_wrap_rect takes a point_start rectangle number and returns the "real"
rectangle offset to the correct place. If rect_offset is non-NULL it gets set
to the repeat_offset value within the function. If needed, this function returns
the ACTUAL rect num used in wrap_base. */

static pg_short_t convert_wrap_rect (paige_rec_ptr pg, long r_num, pg_measure_ptr line_stuff,
		rectangle_ptr real_rect, co_ordinate_ptr rect_offset, pg_boolean top_of_page)
{
	pg_measure_ptr				line_info;
	co_ordinate					repeat_offset;
	pg_short_t					real_r;
	long						offset_h, offset_v, sub_page;
	
	line_info = line_stuff;

	real_r = pgGetWrapRect(pg, r_num, &repeat_offset);
	*real_rect = line_info->wrap_r_base[real_r];
	
	offset_h = repeat_offset.h;
	offset_v = repeat_offset.v;
	
	if (line_info->subpage_qty) {
		
		sub_page = r_num % line_info->subpage_qty;
		
		if (sub_page == 0)
			offset_h += line_info->extra_indents.top_left.h;
			
		if ((r_num == 0) || top_of_page)
			offset_v += line_info->extra_indents.top_left.v;
	}

	pgOffsetRect(real_rect, offset_h, offset_v);
	
	if (rect_offset)
		*rect_offset = repeat_offset;
		
	return	real_r;
}


/* set_extra_indents calls the modify_page_proc to get extra indents for r_num,
then adjusts line_stuff->fit_rect's top. If first_line_in_page is TRUE then the
vertical top is adjusted.  */

static void set_extra_indents (paige_rec_ptr pg, long r_num, pg_measure_ptr line_stuff,
		pg_boolean first_line_in_page)
{
	register pg_measure_ptr			line_info;
	long							sub_page, qty;
	pg_boolean						page_change;

	line_info = line_stuff;

	if ((qty = line_info->subpage_qty) == 0) {

		if (!(pg->doc_info.attributes & BOTTOM_FIXED_BIT) && (pg->doc_info.attributes & USE_MARGINS_BIT)) {
		
			line_info->extra_indents = pg->doc_info.margins;
			line_info->fit_rect.top_left.h += line_info->extra_indents.top_left.h;
			line_info->fit_rect.bot_right.h -= line_info->extra_indents.bot_right.h;

			if (first_line_in_page)
				line_info->fit_rect.top_left.v += line_info->extra_indents.top_left.v;
		}
		
		return;
	}
	
	page_change = (pg_boolean)(r_num != line_info->last_page);

	if (page_change) {

		if (pg->doc_info.attributes & USE_MARGINS_BIT)
			line_info->extra_indents = pg->doc_info.margins;

		pg->procs.page_modify(pg, r_num + 1, &line_info->extra_indents);
		line_info->last_page = r_num;

		if (first_line_in_page)
			line_info->fit_rect.top_left.v += line_info->extra_indents.top_left.v;
	}

	sub_page = r_num % qty;
	
	if (sub_page == 0) {

		line_info->fit_rect.top_left.h += line_info->extra_indents.top_left.h;
	}

	if (sub_page == (qty - 1))
		line_info->fit_rect.bot_right.h -= line_info->extra_indents.bot_right.h;
}


/* do_partial_rebuild is a "faster" pagination in that the line records are
basically in tact except one or more is tagged as "dirty." Note that these
dirty tags will always begin on a line boundary (never in the middle of a line). */

static void do_partial_rebuild (paige_rec_ptr pg, pg_measure_ptr line_stuff,
		long PG_FAR *shift_begin, long PG_FAR *shift_end, smart_update_ptr update_info)
{
	register pg_measure_ptr			line_info;
	register point_start_ptr		starts;
	register pg_short_t				size_compare;
	text_block_ptr					block;
	style_walk_ptr					walker;
	long							local_offset, abs_position;
	long							suggest_begin, suggest_end;
	long							previous_r_num, previous_top;
	pg_short_t						num_display_lines, num_display_starts, previous_cell_num;
	pg_short_t						original_starting_offset, back_flags, old_flags;
	pg_boolean						shift_computed;

	line_info = line_stuff;
	starts = line_info->starts;
	block = line_info->block;
	num_display_lines = num_display_starts = 0;

	block->flags &= (~BELOW_CONTAINERS);
	pgFillBlock(&block->bounds, sizeof(rectangle), 0);

	while (starts->flags & LINE_GOOD_BIT) {
		
		pgLineUnionRect(starts, &block->bounds);

		--line_info->starts_ctr;
		++starts;
		
		if (starts->flags == TERMINATOR_BITS)
			break;
	}

/* For good measure, back up one line in case newly inserted text has a breaking
char and forces previous line to re-wrap. */

	if (original_starting_offset = starts->offset) {
		
		while (starts->offset) {
		
			--starts;
			++line_info->starts_ctr;

			while (starts->offset) {
				
				if (starts->flags & NEW_LINE_BIT) {
					
					if (!starts->cell_num)
						break;
					
					original_starting_offset = starts->offset;
					if ((starts->cell_num & CELL_TOP_BIT) && (starts->cell_num & CELL_LEFT_BIT))
						break;
				}

				--starts;
				++line_info->starts_ctr;
			}
			
			if (starts->offset) {
				
				--starts;
				back_flags = starts->flags;
				++starts;
				
				if (!pgShouldBreakPage(pg, line_info, back_flags, starts->offset))
					break;
			}
		}
	}

	line_info->starts = starts;
	local_offset = starts->offset;
	line_info->cell_index = (starts->cell_num & CELL_NUM_MASK);

	suggest_begin = block->begin + local_offset;
	*shift_begin = *shift_end = suggest_begin;
	shift_computed = FALSE;

	line_info->char_locs += local_offset;
	line_info->char_types += local_offset;

	init_starting_line_rect(pg, line_info);

	if (pgShouldBreakPage(pg, line_info, line_info->previous_flags, starts->offset))
		advance_line_rect(pg, line_info);

	abs_position = block->end - block->begin;
	size_compare = (pg_short_t)abs_position;
	
	walker = line_info->styles;

	while (starts->offset < size_compare) {

		if (!pg->doc_info.max_chars_per_line)
			line_info->max_text_size = size_compare - starts->offset;
		else
			line_info->max_text_size = pg->doc_info.max_chars_per_line;

		abs_position = block->begin;
		abs_position += starts->offset;
		pgSetWalkStyle(walker, abs_position);

		if (walker->cur_par_style->table.table_columns) {
			
			if (!line_info->is_table) {

				set_par_left_and_right(pg, line_info, &line_info->fit_rect, line_info->starts);
				pgSetTableRect(pg, 0, line_info);
			}
			else
				pgSetTableRect(pg, 1, line_info);
		}
		else
		if (line_info->is_table)
			pgSetTableRect(pg, -1, line_info);

		pg->procs.line_init(pg, line_info, new_line_verb);
		
		if (!shift_computed) {
		
			previous_top = starts->bounds.top_left.v;
			old_flags = starts->flags;
		}

		for (;;) {
			
			abs_position = block->begin;
			abs_position += starts->offset;
			pgSetWalkStyle(walker, abs_position);
			previous_r_num = line_info->r_num_begin;
			previous_cell_num = starts->cell_num & CELL_NUM_MASK;
			walker->cur_par_style->procs.line_proc(pg, line_info);
			
			if (pg->procs.validate_line(pg, line_info))
				break;

/* Line record is not acceptable, i.e., it overflows a container of sorts.
  Hence I cannot break out of this loop yet.  The function validate_line
  will have made a better guess at a more appropriate beginning rectangle
  so I will try once again (above code).   */

			pgResetStartingLine(line_info);
		}
		
		if (!shift_computed)
			if (!(old_flags & LINE_GOOD_BIT) || (previous_top != line_info->actual_rect.top_left.v)) {
				
				*shift_begin = *shift_end = abs_position;
				shift_computed = TRUE;
		}

		pgTrueUnionRect(&line_info->actual_rect, &block->bounds, &block->bounds);

		if (line_info->is_table) {
			
			if (pgEmptyRect(&line_info->table_union))
				line_info->table_union = line_info->actual_rect;
			else
				pgTrueUnionRect(&line_info->actual_rect, &line_info->table_union, &line_info->table_union);
		}

		starts = line_info->starts;
		abs_position = starts->offset;

		if (starts->offset > original_starting_offset) {
		
			++num_display_lines;
			num_display_starts += line_info->num_starts;
		}
		else
			suggest_begin = abs_position + block->begin;
		
		suggest_end = abs_position + block->begin;
		
/* Check to see if we can quit rebuilding */
		
		if (!line_info->is_table)
			if (starts->flags != TERMINATOR_BITS)
				if ( ((starts->flags & (LINE_GOOD_BIT | NEW_LINE_BIT)) == (LINE_GOOD_BIT | NEW_LINE_BIT))
					&& (starts->offset == line_info->old_offset)
					&& (starts->r_num == previous_r_num)
					&& (starts->cell_num == previous_cell_num)
					&& (starts->bounds.top_left.v == line_info->actual_rect.bot_right.v)) {

				advance_line_rect(pg, line_info);
				break;
			}
		
		if (shift_computed) {
			
			*shift_end = (long)starts->offset;
			*shift_end += block->begin;
		}
		
		advance_line_rect(pg, line_info);		/* Advance to next rect */
	}
	
	starts = line_info->starts;
	
	if (update_info && (num_display_lines == 1)) {
	
		for (;;) {
			
			--starts;
			update_info->flags |= (starts->flags & TAB_BREAK_BIT);
			if ((starts->flags & NEW_LINE_BIT) || !starts->offset)
				break;
		}
		
		starts = line_info->starts;
	}

	if (starts->offset == size_compare)
		finish_line_update(pg, line_info);
	else {

		while (starts->flags != TERMINATOR_BITS) {
			
			pgLineUnionRect(starts, &block->bounds);
			--line_info->starts_ctr;
			++starts;
		}
		
		line_info->starts = starts;
	}

	terminate_excess_starts(line_info->starts, line_info->starts_ctr);
	
	if (update_info) {
		
		update_info->suggest_begin = pgMin(suggest_begin,*shift_begin);
		update_info->suggest_end = suggest_end;
		update_info->num_display_lines = num_display_lines;
		update_info->num_display_starts = num_display_starts;
	}
}


/* terminate_excess_starts assures that excess starts following the terminator
are tagged as "terminated". */

static void terminate_excess_starts (point_start_ptr end_start, pg_short_t remaining)
{
	register point_start_ptr		starts;
	register long					num_starts;
	
	starts = end_start;
	num_starts = remaining;

	while (num_starts) {
		
		starts->flags = TERMINATOR_BITS;
		++starts;
		--num_starts;
	}
}


/* insert_tab_record examines the current list of tab records (while we build the text line),
and returns the appropriate place to insert a new one. Note that exclusion area breaks (hop-overs)
are also recorded in the tab array.*/

static tab_width_ptr insert_tab_record (memory_ref tab_recs, long current_offset,
		long PG_FAR *width_replace)
{
	tab_width_ptr		tabs;
	long				index;

	tabs = (tab_width_ptr) UseMemoryRecord(tab_recs, 0, USE_ALL_RECS, FALSE);
	index = 0;

	while (tabs->offset < current_offset) {
		
		++tabs;
		++index;
	}

	if (tabs->offset == current_offset) {
		
		*width_replace = (tabs->width - tabs->hard_width);

		return	tabs;
	}

	*width_replace = 0;

	return	(tab_width_ptr)InsertMemory(tab_recs, index, 1);
}


/* attach_par_exclusion checks for a possible paragraph exclusion and, if found,
moves that exclusion to be exactly on top of the line (vertically). */

static void attach_par_exclusion (paige_rec_ptr pg, pg_measure_ptr line_info)
{
	style_run_ptr			run;
	rectangle_ptr			exclusion;
	long					global_offset, remaining_qty;
	
	global_offset = (long)line_info->starts->offset;
	global_offset += line_info->block->begin;

	if ((run = pgFindParExclusionRun(pg, global_offset, &remaining_qty)) != NULL) {
		
		if (remaining_qty) {
			
			if (run->offset == global_offset) {
				// found one!
				
				exclusion = (rectangle_ptr) UseMemoryRecord(pg->exclude_area, (long)run->style_item, 0, TRUE);
				
				if (line_info->fit_rect.top_left.v != exclusion->top_left.v) {
				
					pgOffsetRect(exclusion, 0, line_info->fit_rect.top_left.v - exclusion->top_left.v);
					pgResetBounds(pg->exclude_area);
					pg->port.clip_info.change_flags |= CLIP_EXCLUDE_CHANGED;
					pg->flags2 |= PAR_EXCLUSION_CHANGE;
				}

				UnuseMemory(pg->exclude_area);
			}
		}

		UnuseMemory(pg->par_exclusions);
	}
}

