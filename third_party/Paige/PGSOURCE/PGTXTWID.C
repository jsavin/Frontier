/* This file handles text_block records in a Paige struct. */

#include "Paige.h"

#ifdef MAC_PLATFORM
#pragma segment pgbasic2
#endif

#include "machine.h"
#include "defprocs.h"
#include "pgDefStl.h"
#include "pgUtils.h"
#include "pgText.h"
#include "pgTxtWid.h"
#include "pgSubref.h"
#include "pgShapes.h"
#include "pgScript.h"

static void measure_hidden_text (long PG_FAR *positions, short PG_FAR *chartypes,
		size_t measure_size);
static pg_short_t maximum_measure_size (paige_rec_ptr pg_rec, style_walk_ptr walker);
static long backup_to_valid_char (paige_rec_ptr pg, style_walk_ptr style_walker,
		pg_char_ptr data, long block_offset, long offset_begin, long offset_end,
		long PG_FAR *char_offset, long previous_c_info);
static size_t next_subref_offset (pg_char_ptr text, size_t remaining_length);
static long measure_subref_char (paige_rec_ptr pg, text_block_ptr block, short subref_index);


/* pgLineParse is the default line parser function, which merely returns the
current length-to-next-line-partition. When it is used for other fancy purposes
the idea is to return a lesser length than remaining_length if the line should
parse on something sooner. The text param is the text beginning at this portion
of the line.  */

PG_PASCAL (long) pgLineParse (paige_rec_ptr pg, pg_measure_ptr measure_info,
		pg_char_ptr text, point_start_ptr line_start, long global_offset, long remaining_length)
{
	return	remaining_length;
}


/* pgInitLineProc is the default init/uninit for line measuring. This is where
the measure_info is initialized (if init_verb = init_measure_verb), or starting
a new line (new_line_verb) or terminating a text block measure
(done_measure_verb).  */

PG_PASCAL (void) pgInitLineProc (paige_rec_ptr pg, pg_measure_ptr measure_info, short init_verb)
{
}



/* pgPageModify is the default modify-page function. This gets called for each repeating page.
This default function does not do anything. */

PG_PASCAL (void) pgPageModify  (paige_rec_ptr pg, size_t page_num, rectangle_ptr margins)
{
}


/* pgHyphenateProc is the default hyphenation function. Actually, "hyphenation"
might be slight of a misnomer since this function must determine where to break
a word at the end of a line. Upon entry, *offset is the relative offset of a
byte in text that is ONE TOO MANY characters that can fit in the available space.
This function must decide how far to back up (normally, it backs up to the beginning
of the current word minus any leading blanks). In the case of hyphenation you
can determine something else.  In any case, *offset should get updated to a
lower number and the line will break there.  If you are actually hyphenating,
return TRUE for the function result.   */

PG_PASCAL (pg_boolean) pgHyphenateProc (paige_rec_ptr pg, text_block_ptr block,
		style_walk_ptr styles, pg_char_ptr block_text, long line_begin,
		long PG_FAR *line_end, long PG_FAR *positions, short PG_FAR *char_types,
		long PG_FAR *line_width_extra, pg_boolean zero_length_ok)
{
	register long			offset_begin, global_offset;
	register style_walk_ptr	walker;
	long					soft_hyphen_flag, original_end, multibyte_adjust;
	long					offset_end, max_end, c_info, class_bits, first_class_bits;
	pg_boolean				hyphenated;

#ifdef MAC_PLATFORM
#pragma unused (block, positions, char_types)
#endif

	if (pg->flags & NO_SOFT_HYPHEN_BIT)
		soft_hyphen_flag = 0;
	else
		soft_hyphen_flag = SOFT_HYPHEN_BIT;

	offset_begin = line_begin;
	offset_end = original_end = *line_end;
	walker = styles;
	global_offset = block->begin;
	max_end = block->end - block->begin;

	hyphenated = FALSE;
	
	if (offset_end > offset_begin) {

		pgWalkStyle(walker, -1);
		first_class_bits = walker->cur_style->class_bits;
		pgWalkStyle(walker, 1);
	}

	offset_end = *line_end;

	while (offset_end > offset_begin) {
		
		pgWalkStyle(walker, -1);
		--offset_end;

		class_bits = walker->cur_style->class_bits;
		
		if (class_bits & GROUP_CHARS_BIT) {

			if (!(class_bits & CANNOT_BREAK)) {

				if (walker->current_offset == walker->prev_style_run->offset)
					break;

				if (!(first_class_bits & GROUP_CHARS_BIT)) {

					++offset_end;
					pgWalkStyle(walker, 1);

					break;
				}
			}
		}
		else
		if (!(class_bits & CANNOT_BREAK)) {

			c_info = walker->cur_style->procs.char_info(pg, walker, block_text,
					block->begin, offset_begin, max_end, offset_end, 
					WORD_BREAK_BIT | INCLUDE_BREAK_BIT | LAST_HALF_BIT | MIDDLE_CHAR_BIT
					| NON_BREAKAFTER_BIT | NON_BREAKBEFORE_BIT | SOFT_HYPHEN_BIT | BLANK_BIT);
			
			if (c_info & (LAST_HALF_BIT | MIDDLE_CHAR_BIT))
				c_info = backup_to_valid_char(pg, walker, block_text, block->begin, offset_begin,
						max_end, &offset_end, c_info);

	   		if (!(c_info & (LAST_HALF_BIT | MIDDLE_CHAR_BIT))) {

				if ((c_info & WORD_BREAK_BIT) || (c_info & soft_hyphen_flag)) {
					
					/* Scan backwards until a non-blank exists */
					
					if (c_info & BLANK_BIT) {
					
						while (offset_end > offset_begin) {

							c_info = backup_to_valid_char(pg, walker, block_text, block->begin,
									offset_begin, max_end, &offset_end, c_info);

							if (!(c_info & BLANK_BIT)) {

								pgWalkStyle(walker, 1);
								++offset_end;

								while (offset_end < original_end) {
							
									c_info = walker->cur_style->procs.char_info(pg, walker,
										block_text, block->begin, offset_begin, max_end, offset_end,
										WORD_BREAK_BIT | INCLUDE_BREAK_BIT | SOFT_HYPHEN_BIT | BLANK_BIT
										| LAST_HALF_BIT | MIDDLE_CHAR_BIT | NON_BREAKAFTER_BIT | NON_BREAKBEFORE_BIT);
								
									if (!(c_info & (LAST_HALF_BIT | MIDDLE_CHAR_BIT)))
										break;

									++offset_end;
									pgWalkStyle(walker, 1);
								}
							
								break;
							}
						}
					}

					if ((c_info & INCLUDE_BREAK_BIT) || (c_info & soft_hyphen_flag))
						if (!(c_info & (NON_BREAKAFTER_BIT | NON_BREAKBEFORE_BIT)))
							++offset_end;

					if (offset_end >= original_end)
						multibyte_adjust = -1;
					else
						multibyte_adjust = 1;

					pgSetWalkStyle(walker, global_offset + offset_end);

					while (walker->cur_style->procs.char_info(pg, walker,
								block_text, block->begin, offset_begin, max_end, offset_end, LAST_HALF_BIT | MIDDLE_CHAR_BIT)) {
								
						offset_end += multibyte_adjust;
						pgWalkStyle(walker, multibyte_adjust);
					}

					hyphenated = ((c_info & soft_hyphen_flag) != 0);
					break;
				}
			}
		}
	}
	
	if (offset_end > offset_begin) {
		long		possible_new_end;

		pgSetWalkStyle(walker, global_offset + offset_end);

		while (offset_end > offset_begin) {
			
			possible_new_end = offset_end;
			c_info = backup_to_valid_char(pg, walker, block_text, block->begin, offset_begin,
							max_end, &possible_new_end, 0);

			if (c_info & NON_BREAKAFTER_BIT) {
			
				offset_end = possible_new_end;
				hyphenated = FALSE;
			}
			else
				pgSetWalkStyle(walker, global_offset + offset_end);
			
			if (walker->cur_style->procs.char_info(pg, walker,
					block_text, block->begin, offset_begin, max_end, offset_end, NON_BREAKBEFORE_BIT)) {
				
				hyphenated = FALSE;
				c_info = backup_to_valid_char(pg, walker, block_text, block->begin, offset_begin,
							max_end, &offset_end, 0);
			}
			else
				break;
		}
	}

	if ((!zero_length_ok) && (offset_end == offset_begin)) {

		hyphenated = FALSE;		

		offset_end = *line_end;
		pgSetWalkStyle(walker, global_offset + offset_end);
		
		c_info = 0;

		while (offset_end > offset_begin) {

			pgWalkStyle(walker, -1);
			--offset_end;
			
			if (!walker->cur_style->procs.char_info(pg, walker, block_text,
					global_offset, offset_begin, max_end, offset_end, LAST_HALF_BIT | MIDDLE_CHAR_BIT))
				break;
		}
	
		if (offset_end == offset_begin) {  /* Zero length line! */
			long			byte_advance, initial_begin;

			pgSetWalkStyle(walker, global_offset + offset_begin);
			offset_end = *line_end;
			initial_begin = offset_begin;

			while (offset_begin < offset_end) {
				
				byte_advance = walker->cur_style->char_bytes + 1;

				pgWalkStyle(walker, byte_advance);
				offset_begin += byte_advance;
				
				if (!walker->cur_style->procs.char_info(pg, walker, block_text,
						block->begin, initial_begin, max_end, offset_begin, LAST_HALF_BIT | MIDDLE_CHAR_BIT))
					break;
			}
			
			offset_end = offset_begin;
		}
	}

	*line_end = offset_end;

	if (hyphenated)
		if (offset_end)
			*line_width_extra = positions[offset_end] - positions[offset_end - 1];
	
	return	hyphenated;
}

/* pgShouldBreakPage returns TRUE if the current line should force a new page. */

PG_PASCAL (pg_boolean) pgShouldBreakPage (paige_rec_ptr pg, pg_measure_ptr line_info,
		pg_short_t previous_flags, pg_short_t  starts_offset)
{
	style_walk_ptr	walker;
	short			class_info;
	long			abs_offset;
	
	if (!(pg->doc_info.attributes & (V_REPEAT_BIT | BOTTOM_FIXED_BIT)))
		return	FALSE;

	if (previous_flags & (BREAK_CONTAINER_BIT | BREAK_PAGE_BIT))
		return	TRUE;
	
	if (!(previous_flags & PAR_BREAK_BIT))
		return	FALSE;

	abs_offset = (long)starts_offset;
	abs_offset += line_info->block->begin;

 	if (abs_offset == 0)
  		return FALSE;

	if ((walker = line_info->styles) != NULL) {
	
		if (walker->current_offset != abs_offset)
			pgSetWalkStyle(walker, abs_offset);
		
		class_info = walker->cur_par_style->class_info;
	}
	else {
		par_info_ptr			current_par;
		
		current_par = pgFindParStyle(pg, abs_offset);
		class_info = current_par->class_info;
		UnuseMemory(pg->par_formats);
	}

	return	(pg_boolean) ((class_info & PAR_ON_NEXT_PAGE) != 0);
}


/* This one is similar to pgLineValidate (above) except it is a known empty
line (due to end of text and previous CR. So it is handled slightly differently. */

PG_PASCAL (void) pgAcceptEmptyLine (paige_rec_ptr pg, point_start_ptr the_start,
		pg_measure_ptr line_stuff)
{
	register pg_measure_ptr			line_info;
	register rectangle_ptr			wrap_r;
	register point_start_ptr		starts;
	rectangle						minimum_bounds;
	co_ordinate						last_topleft;
	long							r_bottom, lowest_exclude, actual_width;
	long							global_offset, adjust_h, adjust_v, minimum_width;
	short							justify;
	pg_short_t						exclusions;

	line_info = line_stuff;
	wrap_r = line_info->wrap_r_end;
	r_bottom = wrap_r->bot_right.v + line_info->repeat_offset.v;
	
	starts = the_start;
	minimum_width = pg->globals->minimum_line_width;
	minimum_bounds = starts->bounds;
	global_offset = (long)starts->offset;
	global_offset += line_info->block->begin;

	justify = line_info->styles->cur_par_style->justification;

	if (justify == justify_right)
		minimum_bounds.top_left.h = minimum_bounds.bot_right.h - minimum_width;
	else
		minimum_bounds.bot_right.h = minimum_bounds.top_left.h + minimum_width;
	
	if (exclusions = pgExcludeRectInShape(pg, &minimum_bounds,
			minimum_width, &lowest_exclude, global_offset, line_info->exclude_ref)) {
		rectangle_ptr		exclude_ptr;

		adjust_h = 0;
		adjust_v = lowest_exclude - starts->bounds.top_left.v;
		minimum_bounds = starts->bounds;
		
		if (justify == justify_right)
			minimum_bounds.top_left.h = line_info->fit_rect.top_left.h;
		else
			minimum_bounds.bot_right.h = line_info->fit_rect.bot_right.h;

		exclusions = pgExcludeRectInShape(pg, &minimum_bounds,
			minimum_width, &lowest_exclude, global_offset, line_info->exclude_ref);

		exclude_ptr = (rectangle_ptr) UseMemory(line_info->exclude_ref);
		
		if (justify == justify_right) {
			
			exclude_ptr += exclusions;

			while (exclusions) {
				
				--exclude_ptr;
				--exclusions;

				if (exclusions) {
					
					--exclude_ptr;
					actual_width = exclude_ptr[1].top_left.h - exclude_ptr->bot_right.h;
					++exclude_ptr;				
				}
				else
					actual_width = exclude_ptr->top_left.h - line_info->fit_rect.top_left.h;
	
				if (actual_width >= minimum_width) {
					
					adjust_v = 0;
					adjust_h = exclude_ptr->top_left.h - starts->bounds.top_left.h;
					break;
				}
			}
		}
		else {
		
			while (exclusions) {
				
				if (exclusions > 1)
					actual_width = exclude_ptr[1].top_left.h - exclude_ptr->bot_right.h;
				else
					actual_width = line_info->fit_rect.bot_right.h - exclude_ptr->bot_right.h;
	
				if (actual_width >= minimum_width) {
					
					adjust_v = 0;
					adjust_h = exclude_ptr->bot_right.h - starts->bounds.top_left.h;
					break;
				}
				
				++exclude_ptr;
				--exclusions;
			}
		}

		UnuseMemory(line_info->exclude_ref);
		
		pgOffsetRect(&starts->bounds, adjust_h, adjust_v);
		pgOffsetRect(&line_info->actual_rect, adjust_h, adjust_v);
	}

	if (pg->t_length == 0)
		return;

	if (pgPrimaryTextDirection(pg->globals, line_info->styles->cur_par_style) == right_left_direction) {
	
		pgAdjustRightLeftStarts(pg, &line_info->wrap_bounds, right_left_direction, starts, 1);
		pgFlipRect(&line_info->wrap_bounds, &line_info->actual_rect, (short)right_left_direction);
	}

	if (line_info->actual_rect.bot_right.v <= r_bottom)
		return;

	if (pg->doc_info.attributes & NO_CONTAINER_JMP_BIT)	/* If no advance on containers */
		return;

	last_topleft = line_info->fit_rect.top_left;

	++line_info->r_num_end;
	wrap_r = line_info->wrap_r_base;
	wrap_r += pgGetWrapRect(pg, line_info->r_num_end, &line_info->repeat_offset);

	line_info->wrap_r_begin = line_info->wrap_r_end = wrap_r;
	line_info->r_num_begin = line_info->r_num_end;

	pgBlockMove(wrap_r, &line_info->fit_rect, sizeof(rectangle));
	pgOffsetRect(&line_info->fit_rect, line_info->repeat_offset.h, line_info->repeat_offset.v);

	adjust_h = line_info->fit_rect.top_left.h - last_topleft.h;
	adjust_v = line_info->fit_rect.top_left.v - last_topleft.v;
	pgOffsetRect(&line_info->actual_rect, adjust_h, adjust_v);

	starts->r_num = line_info->r_num_begin;
	pgOffsetRect(&starts->bounds, adjust_h, adjust_v);
}


/* Given a relative point_start in a block, pgLineBoundary returns the first
text offset of the line in *line_begin_offset and the ending text offset in
*line_end_offset (either parameter can also be NULL if you don't need the result).
The function returns the total number of point_starts for the line. Also,
from_start returns with the actual line's first start. */

PG_PASCAL (pg_short_t) pgLineBoundary (paige_rec_ptr pg, text_block_ptr block, pg_short_t PG_FAR *from_start,
			pg_short_t PG_FAR *line_begin_offset, pg_short_t *line_end_offset)
{
	register point_start_ptr		starts;
	register pg_short_t				ctr, back_ctr;
	long							abs_offset;
	pg_short_t						first_offset, last_offset;

	starts = (point_start_ptr) UseMemory(block->lines);
	back_ctr = *from_start;
	starts += back_ctr;
	abs_offset = block->begin;
	abs_offset += (long) starts->offset;
	
	if (abs_offset < pg->t_length) {
	
		while (starts->offset) {
			
			--starts;
	
			if (starts->flags & LINE_BREAK_BIT) {
				++starts;
				break;
			}
			--back_ctr;
		}
		
		for (ctr = 1, first_offset = starts->offset; !(starts->flags & LINE_BREAK_BIT);
					++ctr, ++starts) ;
		
		last_offset = starts[1].offset;
	}
	else {
		
		ctr = 1;
		first_offset = last_offset = starts->offset;
	}
	
	if (line_begin_offset)
		*line_begin_offset = first_offset;
	if (line_end_offset)
		*line_end_offset = last_offset;
	
	*from_start = back_ctr;

	UnuseMemory(block->lines);
	
	return	ctr;
}



/* pgGetCharLocs returns the character positions pointer for a text_block. Note
the positions array might already exist (in the pg_rec) and, if so, they are
returned as a pointer in use.  If they do not exist, they are created, placed
in the pg_rec and returned as a pointer in use. In either case, the memory ref
is returned in *memory_ref and you must do UnuseMemory with it when finished.

If types_ref is non-NULL it gets set to the character-types list memory_ref.  */

PG_PASCAL (long PG_FAR*) pgGetCharLocs (paige_rec_ptr pg_rec, text_block_ptr block,
		memory_ref PG_FAR *used_ref, memory_ref PG_FAR *types_ref)
{
	register long PG_FAR		*positions;
	register short PG_FAR		*char_types;
	long PG_FAR					*positions_result;
	memory_ref					long_pos_ref, types;
	pg_char_ptr					text;
	style_walk					walker;
	size_t						text_length, max_chars, partial_length, next_style, next_link;
	short						call_order;
	size_t						next_subref_pos;
	short						subref_ctr = 0;
	short						subref_qty = 0;
	
	if ((long_pos_ref = pgFindCharLocs(pg_rec, block, NULL, &types, TRUE, FALSE)) != MEM_NULL) {
		
		*used_ref = long_pos_ref;

		if (types_ref)
			*types_ref = types;

		return	(long PG_FAR*) UseMemory(long_pos_ref);
	}
	
	*used_ref = long_pos_ref = pgFindCharLocs(pg_rec, block, NULL, &types, FALSE, TRUE);

	if (types_ref)
		*types_ref = types;
	
	pg_rec->procs.load_proc(pg_rec, block);
	
	if (block->subref_list)
		subref_qty = (short)GetMemorySize(block->subref_list);

	text_length = block->end - block->begin;
	SetMemorySize(long_pos_ref, text_length + 1);
	SetMemorySize(types, text_length + 1);

	positions_result = positions = (long *) UseMemory(long_pos_ref);
	*positions = 0;
	char_types = (short *) UseMemory(types);

	pgPrepareStyleWalk(pg_rec, block->begin, &walker, FALSE);
	text = (pg_char_ptr) UseMemory(block->text);

	pgSetMeasureDevice(pg_rec);
	call_order = 0;
	
	while (text_length) {
		
		walker.cur_style->procs.install(pg_rec, walker.cur_style, walker.cur_font,
						&walker.superimpose, walker.style_overlay, FALSE);
		
		max_chars = (long)maximum_measure_size(pg_rec, &walker);
		partial_length = text_length;
		next_style = walker.next_style_run->offset - walker.current_offset;

		if (next_style < partial_length)
			partial_length = next_style;

		if (walker.current_offset >= walker.hyperlink->applied_range.begin)
			next_link = walker.hyperlink->applied_range.end - walker.current_offset;
		else
			next_link = walker.hyperlink->applied_range.begin - walker.current_offset;

		if (next_link < partial_length)
			partial_length = next_link;

		if (walker.current_offset >= walker.hyperlink_target->applied_range.begin)
			next_link = walker.hyperlink_target->applied_range.end - walker.current_offset;
		else
			next_link = walker.hyperlink_target->applied_range.begin - walker.current_offset;

		if (next_link < partial_length)
			partial_length = next_link;

		if (partial_length > max_chars)
			partial_length = max_chars;
		
		if (subref_ctr < subref_qty) {
		
			next_subref_pos = next_subref_offset(text, partial_length);

			if (partial_length > next_subref_pos)
				partial_length = next_subref_pos;
		}

		if (partial_length == text_length)
			call_order = -1;

		if ((pg_rec->flags & NO_HIDDEN_TEXT_BIT) && walker.cur_style->styles[hidden_text_var])
			measure_hidden_text (positions, char_types, partial_length);
		else {
			
			if (*text == SUBREF_CHAR && subref_ctr < subref_qty) {

				positions[1] = *positions + measure_subref_char(pg_rec, block, subref_ctr);
				partial_length = 1;
				*char_types = CTL_BIT | INVIS_ACTION_BIT;
				subref_ctr += 1;
			}
			else
				walker.cur_style->procs.measure(pg_rec, &walker, text, partial_length,
					0, positions, char_types, measure_width_locs, walker.current_offset,
					FALSE, call_order);
		}

		positions += partial_length;
		char_types += partial_length;
		++call_order;

		if ((text_length -= partial_length) > 0) {

			pgWalkStyle(&walker, partial_length);			
			text += partial_length;
		}
	}
	
	UnuseMemory(block->text);
	UnuseMemory(types);

	pgPrepareStyleWalk(pg_rec, 0, NULL, FALSE);
	pgUnsetMeasureDevice(pg_rec);

	return	positions_result;
}


/* pgGetSpecialLocs returns a (new) memory_ref of a smaller block of text measured
with a full-justify slop. It gets called by mouse-selection routines when the
actual screen positions (including space-extra settings, etc.) are required. The
block of text to be measured is guaranteed to be a single style run.
If scaled_widths is non-zero it gets passed to the measure proc. */

PG_PASCAL (memory_ref) pgGetSpecialLocs (paige_rec_ptr pg_rec, text_block_ptr block,
		point_start_ptr related_start, pg_short_t text_size, long slop_extra,
		short scale_widths)
{
	register long PG_FAR			*positions;
	register pg_char_ptr			text;
	memory_ref						result_ref;
	style_walk						walker;
	long							char_size, starting_offset, ending_offset;
	short							scale_verb, measure_verb;
	pg_short_t						non_blank_size;
	pg_short_t						text_offset, unresolved_chars;

	result_ref = MemoryAlloc(pg_rec->globals->mem_globals, sizeof(long),
			text_size + 1, 0);
	positions = (long *) UseMemory(result_ref);
	*positions = 0;

	starting_offset = block->begin + (long) related_start->offset;
	ending_offset = block->end - block->begin;

	pg_rec->procs.load_proc(pg_rec, block);
	text = (pg_char_ptr) UseMemory(block->text);
	text_offset = related_start->offset;

	if (text_offset < (pg_short_t)(block->end - block->begin))
		if (text[text_offset] == SUBREF_CHAR) 
			if (pgIsValidSubref(block, text_offset)) {
			long			subref_index;
			
			subref_index = pgFindSubrefPosition(block, text_offset);
			positions[1] = measure_subref_char(pg_rec, block, (short)subref_index);
			UnuseMemory(block->text);
			UnuseMemory(result_ref);
			
			return	result_ref;
	}

	text += related_start->offset;

	pgPrepareStyleWalk(pg_rec, starting_offset, &walker, TRUE);

	pgSetMeasureDevice(pg_rec);

	walker.cur_style->procs.install(pg_rec, walker.cur_style, walker.cur_font,
					&walker.superimpose, walker.style_overlay, FALSE);

	non_blank_size = text_size;
	
/* Check to see if we omit blanks from the measurement's end: */

/* (Fix BaseView reported: ) */

	if (!(related_start->flags & PAR_BREAK_BIT) && (related_start[1].flags != TERMINATOR_BITS)
		&& (related_start->flags & LINE_BREAK_BIT) && ( (related_start->flags & RIGHT_DIRECTION_BIT)
			|| (walker.cur_par_style->justification != justify_left)) )  {

		while (non_blank_size) {
	
			char_size = walker.cur_style->char_bytes + 1;
			if (!walker.cur_style->procs.char_info(pg_rec, &walker, text,
					NO_BLOCK_OFFSET, 0, ending_offset, non_blank_size - char_size, BLANK_BIT))
				break;
	
			non_blank_size -= (pg_short_t)char_size;
			pgWalkStyle(&walker, -char_size);
		}
	}

	if (pg_rec->scale_factor.scale)
		scale_verb = scale_widths;
	else
		scale_verb = FALSE;

	if (non_blank_size) {

		pgSetWalkStyle(&walker, starting_offset);

		if ((pg_rec->flags & NO_HIDDEN_TEXT_BIT) && walker.cur_style->styles[hidden_text_var])
			measure_hidden_text (positions, NULL, non_blank_size);
		else {

			measure_verb = (short)measure_draw_locs;
			
			if (!(related_start->flags & HAS_WORDS_BIT))
				measure_verb = -measure_verb;

			walker.cur_style->procs.measure(pg_rec, &walker, text, (long)non_blank_size,
				(pg_short_t)slop_extra, positions, (short PG_FAR *)NULL, measure_verb,
				walker.current_offset, (pg_boolean)scale_verb, (short)-1);
		}
	}

	if (unresolved_chars = text_size - non_blank_size) {
		
		for (positions += non_blank_size; unresolved_chars; ++positions, --unresolved_chars)
			positions[1] = *positions;

	}

	UnuseMemory(block->text);
	UnuseMemory(result_ref);
	
	pgPrepareStyleWalk(pg_rec, 0, NULL, TRUE);
	pgUnsetMeasureDevice(pg_rec);

	return	result_ref;
}


/* pgPartialTextMeasure inserts new character positions in the current positions
record if it exists and if it matches up with the given block. This does nothing
if current positions (in globals->width_tables) do not exist. This gets called by
functions that insert new text. The insertion took place at measure_from
(local offset to block) for new_measure_length bytes. */

PG_PASCAL (void) pgPartialTextMeasure (paige_rec_ptr pg_rec, text_block_ptr block,
			pg_short_t measure_from, pg_short_t new_measure_length)
{
	long PG_FAR				*original_positions;
	register long PG_FAR	*positions;
	register short PG_FAR	*char_types;
	register pg_char_ptr	text;
	memory_ref				long_pos_ref, types;
	style_walk				walker;
	size_t					end_base, next_style, next_link, global_offset;
	pg_short_t				partial_length, text_length, this_index, text_position, max_chars;
	short					call_order;

	if (!(long_pos_ref = pgFindCharLocs(pg_rec, block, &this_index, &types, TRUE, TRUE)))
		return;

	pg_rec->procs.load_proc(pg_rec, block);

	pgPrepareStyleWalk(pg_rec, block->begin + measure_from, &walker, FALSE);
	text = (pg_char_ptr) UseMemory(block->text);
	global_offset = block->begin;

	if (walker.cur_style->procs.char_info(pg_rec, &walker, text, global_offset, 0, block->end - block->begin,
			measure_from, LAST_HALF_BIT | MIDDLE_CHAR_BIT)) {
		
		UnuseMemory(block->text);
		pgPrepareStyleWalk(pg_rec, 0, NULL, FALSE);
		pgFreeCharLocs(pg_rec, this_index);
		
		return;
	}

	positions = (long *) InsertMemory(long_pos_ref, measure_from, new_measure_length);
	pg_rec->globals->width_tables[this_index].end += new_measure_length;
	char_types = (short *) InsertMemory(types, measure_from, new_measure_length);

	original_positions = positions;
	end_base = positions[new_measure_length];

	text_length = new_measure_length;
	text += measure_from;
	pgSetMeasureDevice(pg_rec);
	call_order = 0;
	text_position = measure_from;

	while (text_length) {
		
		walker.cur_style->procs.install(pg_rec, walker.cur_style, walker.cur_font,
						&walker.superimpose, walker.style_overlay, FALSE);
		
		partial_length = text_length;
		next_style = walker.next_style_run->offset - walker.current_offset;
		
		max_chars = maximum_measure_size(pg_rec, &walker);

		if (next_style < (long)partial_length)
			partial_length = (pg_short_t)next_style;

		if (walker.current_offset >= walker.hyperlink->applied_range.begin)
			next_link = walker.hyperlink->applied_range.end - walker.current_offset;
		else
			next_link = walker.hyperlink->applied_range.begin - walker.current_offset;

		if (next_link < (long)partial_length)
			partial_length = (unsigned short)next_link;

		if (walker.current_offset >= walker.hyperlink_target->applied_range.begin)
			next_link = walker.hyperlink_target->applied_range.end - walker.current_offset;
		else
			next_link = walker.hyperlink_target->applied_range.begin - walker.current_offset;

		if (next_link < (long)partial_length)
			partial_length = (unsigned short)next_link;

		if (partial_length > max_chars)
			partial_length = max_chars;
		if (partial_length == text_length)
			call_order = -1;

		if ((pg_rec->flags & NO_HIDDEN_TEXT_BIT) && walker.cur_style->styles[hidden_text_var])
			measure_hidden_text(positions, char_types, partial_length);
		else
		if (*text == SUBREF_CHAR && pgIsValidSubref(block, text_position)) {
			long			subref_index;
			
			subref_index = pgFindSubrefPosition(block, text_position);
			positions[1] = *positions + measure_subref_char(pg_rec, block, (short)subref_index);
			*char_types = CTL_BIT | INVIS_ACTION_BIT;
			partial_length = 1;
		}
		else
			walker.cur_style->procs.measure(pg_rec, &walker, text,
				partial_length, 0, positions, char_types, measure_width_locs,
				walker.current_offset, FALSE, call_order);

		positions += partial_length;
		char_types += partial_length;
		++call_order;
		
		text_length -= partial_length;
		text_position += partial_length;

		pgWalkStyle(&walker, partial_length);
		text += partial_length;
	}

	pgPrepareStyleWalk(pg_rec, 0, NULL, FALSE);
	UnuseMemory(block->text);
	UnuseMemory(types);

	text_length = (pg_short_t)(block->end - block->begin);
	partial_length = measure_from + new_measure_length;
	
	end_base = *positions - end_base;

	while (partial_length < text_length) {
		
		++positions;
		*positions += end_base;
		++partial_length;
	}

	UnuseMemory(long_pos_ref);
	
	pgReleaseCharLocs(pg_rec, long_pos_ref);
	pgUnsetMeasureDevice(pg_rec);
}


/* pgTextReMeasure re-measures the embedded character located at measure_from. This is called
only by subref code (reserved for DSI). */


PG_PASCAL (void) pgTextReMeasure (paige_rec_ptr pg_rec, text_block_ptr block, size_t measure_from)
{
	register long PG_FAR	*positions;
	register pg_char_ptr	text;
	memory_ref				long_pos_ref, types;
	long					end_base, subref_index, remaining;
	pg_char					measure_byte;
	pg_short_t				this_index;

	if (!(long_pos_ref = pgFindCharLocs(pg_rec, block, &this_index, &types, TRUE, FALSE)))
		return;
	
	pg_rec->procs.load_proc(pg_rec, block);
	text = (pg_char_ptr) UseMemory(block->text);
	measure_byte = text[measure_from];
	UnuseMemory(block->text);
	
	if (measure_byte != SUBREF_CHAR) {
		
		pgReleaseCharLocs(pg_rec, long_pos_ref);
		return;
	}
	
	if (!pgIsValidSubref(block, measure_from)) {
		
		pgReleaseCharLocs(pg_rec, long_pos_ref);
		return;
	}
		
	subref_index = pgFindSubrefPosition(block, (pg_short_t)measure_from);
	positions = (long *) UseMemory(long_pos_ref);
	positions += measure_from;
	end_base = positions[1];
	positions[1] = *positions + measure_subref_char(pg_rec, block, (short)subref_index);

	positions += 1;
	end_base = *positions - end_base;
	
	remaining = (block->end - block->begin - measure_from) - 1;
	
	while (remaining) {
	
		++positions;
		*positions += end_base;
		--remaining;
	}
	
	UnuseMemory(long_pos_ref);
	pgReleaseCharLocs(pg_rec, long_pos_ref);
}


/* pgDeleteTextMeasure deletes character positions from the current positions
record if it exists and if it matches up with the given block. This does nothing
if current positions (in globals->width_tables) do not exist. This gets called by
functions that delete text. The deletion took place at delete_from
(local offset to block) for delete_length bytes. */

PG_PASCAL (void) pgDeleteTextMeasure (paige_rec_ptr pg_rec, text_block_ptr block,
			pg_short_t delete_from, pg_short_t delete_length)
{
	long PG_FAR					*positions;
	memory_ref					long_pos_ref, types;
	long						base_position;
	pg_short_t					remaining_length, this_index;

	if (!(long_pos_ref = pgFindCharLocs(pg_rec, block, &this_index, &types, (pg_boolean)TRUE, TRUE)))
		return;

	remaining_length = (pg_short_t)GetMemorySize(long_pos_ref);

	GetMemoryRecord(long_pos_ref, delete_from, &base_position);
	DeleteMemory(long_pos_ref, delete_from, delete_length);
	DeleteMemory(types, delete_from, delete_length);

	pg_rec->globals->width_tables[this_index].end -= delete_length;

	remaining_length -= (delete_from + delete_length);

	positions = (long *) UseMemoryRecord(long_pos_ref, delete_from, USE_ALL_RECS, TRUE);
	base_position = *positions - base_position;
		
	while (remaining_length) {
		
		*positions -= base_position;
		++positions;
		--remaining_length;
	}
	
	UnuseMemory(long_pos_ref);
	
	pgReleaseCharLocs(pg_rec, long_pos_ref);
}



/* pgFindCharLocs takes a look at three possible char loc records in pg to
see if any of them match text_offset.  If so, the appopriate memory ref is
returned. However, if none match up and must_be_exact is TRUE, the function
return NULL;  if none match and must_be_exact is FALSE, the function initializes
its best choice in the three char loc entries in pg and returns than memory_ref. */

PG_PASCAL (memory_ref) pgFindCharLocs (paige_rec_ptr pg, text_block_ptr block,
		pg_short_t PG_FAR *found_index, memory_ref PG_FAR *types, pg_boolean must_be_exact,
		pg_boolean must_be_unlocked)
{
	register pg_globals_ptr		globals;
	register long				offset;
	register short				index;
	long						memory_id;
	
	globals = pg->globals;
	memory_id = pg->mem_id;
	
	if (pg->active_subset && globals->width_tables[WIDTH_QTY].mem_id == memory_id) {
		
		if (must_be_exact && globals->width_tables[WIDTH_QTY].offset == -1)
			return		MEM_NULL;
		
		if (found_index)
			*found_index = WIDTH_QTY;
		if (types)
			*types = globals->width_tables[WIDTH_QTY].types;

		globals->width_tables[WIDTH_QTY].used_ctr += 1;

		return	globals->width_tables[WIDTH_QTY].positions;
	}

	for (offset = block->begin, index = 0; index < WIDTH_QTY; ++index)
		if ((globals->width_tables[index].mem_id == memory_id) && (globals->width_tables[index].offset == offset)) {
			
			if (!must_be_unlocked || (globals->width_tables[index].used_ctr == 0)) {
			
				if (found_index)
					*found_index = index;
	
				if (types)
					*types = globals->width_tables[index].types;
				
				globals->width_tables[index].used_ctr += 1;

				return	globals->width_tables[index].positions;
			}
		}

	if (must_be_exact)
		return	MEM_NULL;

	for (index = 0; index < WIDTH_QTY; ++index) {
		
		if (globals->width_tables[index].offset == -1)
			break;
	}

	if (index == WIDTH_QTY) {  // Still have not located anything
		
		for (index = 0; index < WIDTH_QTY; ++index)
			if (globals->width_tables[index].used_ctr == 0)
				break;
	}

#ifdef PG_DEBUG
	if (index == WIDTH_QTY)
		globals->mem_globals->debug_proc(ACCESS_ERR, globals->width_tables[0].positions);
#endif

	globals->width_tables[index].offset = offset;
	globals->width_tables[index].end = block->end;
	globals->width_tables[index].mem_id = memory_id;
	globals->width_tables[index].used_ctr += 1;

	if (found_index)
		*found_index = (pg_short_t)index;
	if (types)
		*types = globals->width_tables[index].types;

	return	globals->width_tables[index].positions;
}


/* pgFreeCharLocs forces the width_table charloc_index to be rendered invalid. */

PG_PASCAL (void) pgFreeCharLocs(paige_rec_ptr pg, pg_short_t charloc_index)
{
	pg_globals_ptr		globals = pg->globals;

	globals->width_tables[charloc_index].offset = -1;
	
	if (charloc_index < WIDTH_QTY)
		globals->width_tables[charloc_index].mem_id = 0;

	globals->width_tables[charloc_index].used_ctr = 0;
}


/* pgReleaseCharLocs locates locs_ref and decrements its usage counter. */

PG_PASCAL (void) pgReleaseCharLocs (paige_rec_ptr pg, memory_ref locs_ref)
{
	pg_globals_ptr		globals;
	short				index;
	
	globals = pg->globals;

	for (index = 0; index < CREATED_WIDTH_QTY; ++index)
		if (globals->width_tables[index].positions == locs_ref) {
			
			globals->width_tables[index].used_ctr -= 1;
			break;
		}
}


/* pgInvalTextBlock tries to tag individual line records as "dirty" so I won't
necessarily need to paginate the whole block. Upon entry, block is the textblock
in question, insert_position is the (absolute) insertion position and ins_amount
the amount that has been inserted (if postitive) or deleted (if negative).
The function result is the lowest offset that got tagged. */

PG_PASCAL (long) pgInvalTextBlock (paige_rec_ptr pg, text_block_ptr block,
			long insert_position, long ins_amount)
{
	register point_start_ptr		starts;
	point_start_ptr					first_start;
	long							begin_offset, ending_offset;
	pg_short_t						wanted_offset, lowest_offset;

#ifdef MAC_PLATFORM
#pragma unused (pg)
#endif

	if (!(block->flags & SOME_LINES_GOOD) || (block->flags & LINES_PURGED))
		return	block->begin;

	begin_offset = insert_position - block->begin;
	ending_offset = begin_offset + ins_amount;
	
	if (ins_amount < 0) {
		long		swap;
		
		swap = begin_offset;
		begin_offset = ending_offset;
		ending_offset = swap;
	}

	wanted_offset = (pg_short_t)begin_offset;

	starts = first_start = (point_start_ptr) UseMemory(block->lines);
	block->flags |= (NEEDS_CALC | NEEDS_PARNUMS);
	
	while ((starts[1].offset < wanted_offset) && (starts[1].flags != TERMINATOR_BITS))
		++starts;
	
	if (starts[1].offset == wanted_offset)
		if (starts[1].flags != TERMINATOR_BITS)
			++starts;

	while (!(starts->flags & NEW_LINE_BIT) && starts->offset)
		--starts;

	wanted_offset = (pg_short_t)ending_offset;
	
	while ((starts[1].offset < (pg_short_t)ending_offset) && (starts[1].flags != TERMINATOR_BITS)) {
		
		starts->flags &= (~LINE_GOOD_BIT);
		++starts;
	}

	if ((ins_amount < 0) && (starts[1].offset == (pg_short_t)ending_offset)
		&& (starts->flags & (PAR_BREAK_BIT | SOFT_PAR_BIT))) {
		
		while (starts[1].flags != TERMINATOR_BITS) {
			
			starts->flags &= (~LINE_GOOD_BIT);   /* No lines good through end */
			++starts;
		}
	}
	else {
	
		for (;;) {
			
			starts->flags &= (~LINE_GOOD_BIT);
			
			if ((starts->flags == TERMINATOR_BITS) || (starts->flags & LINE_BREAK_BIT))
				break;
			
			++starts;
		}
	}

	ending_offset = starts->offset;
	ending_offset += block->begin;

	for (starts = first_start; (starts->flags & LINE_GOOD_BIT); ++starts)
		if (starts->flags == TERMINATOR_BITS) {
			
			--starts;
			break;
		}
	
	lowest_offset = starts->offset;

	if (!lowest_offset && ending_offset >= block->end)
		block->flags &= (~SOME_LINES_GOOD);

	UnuseMemory(block->lines);

	return	block->begin + lowest_offset;
}


/* pgInvalCharLocs forces all current char widths to be invalid. */

PG_PASCAL (void) pgInvalCharLocs (paige_rec_ptr pg)
{
	pg_globals_ptr		globals;
	pg_short_t			index;

	globals = pg->globals;

	for (index = 0; index < CREATED_WIDTH_QTY; ++index)
		if (globals->width_tables[index].mem_id == pg->mem_id)
			pgFreeCharLocs(pg, index);
}


/* pgGetLineRealDescent returns the actual line descent of the line containing arbitrary_start.
This can be different than the point_start baseline if, for instance, extra leading or line
spacing has been applied. */

PG_PASCAL (long) pgGetLineRealDescent (text_block_ptr block,
		point_start_ptr arbitrary_start, style_walk_ptr walker)
{
	register point_start_ptr	starts;
	long						global_offset, original_walker;
	long						result, total_descent;
	
	starts = arbitrary_start;
	
	while (starts->offset) {
		
		if (starts->flags & NEW_LINE_BIT)
			break;
		
		--starts;
	}
	
	global_offset = starts->offset;
	global_offset += block->begin;
	original_walker = walker->current_offset;
	pgSetWalkStyle(walker, global_offset);
	
	result = 0;
	
	while (starts->flags != TERMINATOR_BITS) {
		
		total_descent = walker->cur_style->descent + walker->cur_style->leading;

		if (total_descent > result)
			result = total_descent;

		if (starts->flags & LINE_BREAK_BIT)
			break;
		
		pgWalkStyle(walker, (long)(starts[1].offset - starts->offset));
		++starts;
	}
	
	pgSetWalkStyle(walker, original_walker);

	return	result;
}



/****************************** Local Functions **************************/


/* measure_hidden_text is called when "invisible" char widths are to be measured.
This function fills *positions with all the same value, and if chartypes is
non-null it fills in with zeros. */

static void measure_hidden_text (long PG_FAR *positions, short PG_FAR *chartypes,
		size_t measure_size)
{
	register long PG_FAR		*measure_ptr;
	register long				measure_val;
	register pg_short_t			measure_qty;
	
	measure_ptr = positions;
	measure_val = *measure_ptr;

	for (measure_qty = (pg_short_t)(measure_size + 1); measure_qty; --measure_qty)
		*measure_ptr++ = measure_val;
	
	if (chartypes)
		pgFillBlock(chartypes, measure_size * sizeof(short), 0);
}


/* maximum_measure_size returns the maximum safe quantity of chars to measure at once. */

static pg_short_t maximum_measure_size (paige_rec_ptr pg_rec, style_walk_ptr walker)
{
#ifdef UNICODE
	pg_char			test_char[] = {L"W"};
#else
	pg_char			test_char[] = {"W"};
#endif
	long			charwidth, suggested_max;
	long			positions[2] = {0,0};

	walker->cur_style->procs.measure(pg_rec, walker, (pg_char_ptr)test_char, 1,
		0, positions, NULL, measure_width_locs, walker->current_offset,
		FALSE, -1);

	if ((charwidth = positions[1] - positions[0]) < 0)
		charwidth = -charwidth;

	if (charwidth < 6)
		charwidth = 6;

	suggested_max = MAX_ACCUMULATED_WIDTHS;
	suggested_max /= charwidth;
	
	if (suggested_max > TEXT_PARTITION_SIZE)
		return	(pg_short_t)TEXT_PARTITION_SIZE;
	
	return	(pg_short_t)suggested_max;
}

/* backup_to_valid_char walks back char by char until it hits the first of a valid char. */

static long backup_to_valid_char (paige_rec_ptr pg, style_walk_ptr style_walker,
		pg_char_ptr data, long block_offset, long offset_begin, long offset_end,
		long PG_FAR *char_offset, long previous_c_info)
{
	long		c_info = previous_c_info;
	
	while (*char_offset > offset_begin) {
		
		*char_offset -= 1;
		pgWalkStyle(style_walker, -1);
		c_info = style_walker->cur_style->procs.char_info(pg, style_walker, data,
					block_offset, offset_begin, offset_end, *char_offset, 
					WORD_BREAK_BIT | INCLUDE_BREAK_BIT | LAST_HALF_BIT | MIDDLE_CHAR_BIT
					| NON_BREAKAFTER_BIT | NON_BREAKBEFORE_BIT | SOFT_HYPHEN_BIT | BLANK_BIT);
		
		if (!(c_info & (LAST_HALF_BIT | MIDDLE_CHAR_BIT)))
			break;
	}
	
	return	c_info;
}


/* next_subref_offset returns the next subref embedded char, if any. */

static size_t next_subref_offset (pg_char_ptr text, size_t remaining_length)
{
	register pg_char_ptr		text_ptr;
	register size_t				ctr;
	
	ctr = 0;
	text_ptr = text;
	
	if (*text_ptr == SUBREF_CHAR)
		return	remaining_length;

	while (ctr < remaining_length) {
		
		if (*text_ptr++ == SUBREF_CHAR)
			break;
		
		++ctr;
	}
	
	return	ctr;
}


/* measure_subref_char obtains the width measurement for the subref located at subref_index
in the block. */

static long measure_subref_char (paige_rec_ptr pg, text_block_ptr block, short subref_index)
{
	pg_subref_ptr				subref_list;
	paige_sub_ptr				sub_ptr;
	pg_globals_ptr				globals;
	text_block_ptr				subref_block;
	long						width;
	
	subref_list = (pg_subref_ptr) UseMemoryRecord(block->subref_list, subref_index, 0, TRUE);
	sub_ptr = (paige_sub_ptr) UseMemory(*subref_list);
	sub_ptr += sub_ptr->alt_index;
	subref_block = (text_block_ptr) UseMemory(sub_ptr->t_blocks);
	
	if (subref_block->flags & NEEDS_CALC) {
		
		globals = pg->globals;

		pgUseSubRef(pg, *subref_list, 0, NULL, NULL);
		pgPaginateBlock(pg, (text_block_ptr) UseMemory(pg->t_blocks), NULL, FALSE);
		UnuseMemory(pg->t_blocks);

		pgUnuseSubRef(pg);
	}
	
	width = sub_ptr->subset_bounds.bot_right.h - sub_ptr->subset_bounds.top_left.h;
	
	if (width < sub_ptr->minimum_width)
		width = sub_ptr->minimum_width;

	UnuseMemory(sub_ptr->t_blocks);
	UnuseMemory(*subref_list);
	UnuseMemory(block->subref_list);
	
	return		width;
}

