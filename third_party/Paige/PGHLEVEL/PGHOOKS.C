
#include "PGHLEVEL.H"
#include "defprocs.h"
#include "pgText.h"
#include "pgEdit.h"
#include "pgShapes.h"
#include "pgUtils.h"


/* TO "SET" HOOK YOU NEED TO MODIFY THE DEFAULT BOUNDARY_PROC IN PAIGE GLOBALS.
If you are using the custom control you (probably) should modify the control code directly
(pgcntl.c) and re-compile.  The following is an example of doing this:

		pgInit(&paige_globals, &mem_globals); 
   	 	paige_globals.def_hooks.boundary_proc = pgContainerParProc;

The source for pgContainerParProc() is below, the prototype is in pghlevel.h.  */


/* This is an alternate hook function which reports a container break char as a paragraph
boundary.  Use this for "forms" when you want paragraph formats to stay inside containers. */

PG_PASCAL (pg_boolean) pgContainerParProc (paige_rec_ptr pg, select_pair_ptr boundary)
{
	register pg_char_ptr		text;
	register long				local_offset;
	text_block_ptr				block, starting_block;
	long						offset, starting_offset;
	pg_boolean					valid_cr, starts_on_par;
	pg_char						cr_char;

	cr_char = (pg_char)pg->globals->container_brk_char;

	if (boundary) {

		offset = boundary->begin;
		
		block = starting_block = pgFindTextBlock(pg, offset, NULL, FALSE, TRUE);
		local_offset = starting_offset = offset - block->begin;
		text = UseMemory(block->text);
		text += local_offset;
	
	/* Locate beginning of paragraph */
		
		valid_cr = FALSE;
	
		for (;;) {
			
			while (local_offset) {
				
				if (valid_cr = (pg_boolean)(*(--text) == cr_char))
					break;
	
				--local_offset;
			}
			
			if (valid_cr)
				break;
			
			if (!local_offset) {
				
				if (!block->begin)
					break;
				
				UnuseMemory(block->text);
	
				--block;
				pg->procs.load_proc(pg, block);

				local_offset = block->end - block->begin;
				text = UseMemory(block->text);
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

		text = UseMemory(block->text);
		text += starting_offset;
		
		valid_cr = FALSE;
	
		for (;;) {
		
			while (offset < block->end) {
				
				++offset;
				if (valid_cr = (*text++ == cr_char))
					break;
			}
	
			if (valid_cr)
				break;

			if (offset == block->end) {
				
				if (block->end == pg->t_length)
					break;
				
				UnuseMemory(block->text);
				++block;
				pg->procs.load_proc(pg, block);
				text = UseMemory(block->text);
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
			text = UseMemory(block->text);
			
			if (text[local_offset] != cr_char)
				starts_on_par = FALSE;
			
			UnuseMemory(block->text);
			UnuseMemory(pg->t_blocks);
		}
	}

	return	starts_on_par;
}


/* This hook replaces "line_glitter_proc" and computes paragraph numbers relative to the top of
the current page.  For example, the first paragraph of every page will be "1," subsequent pars
are then 2, 3, etc.  You can also do this for line numbers,  if so replace the code per comments
that begin with "FOR LINE(S) -- ."  THIS CODE ONLY WORKS FOR REPEATING PAGE DOCUMENTS.  It will
fail for long continuous docs.
Please note that this hook, by itself, draws nothing (i.e. does not draw "line numbers." That will
be your job. This code computes the correct line or paragraph number only. Add drawing code to
the end. */

PG_PASCAL (void) PageLines (paige_rec_ptr pg, style_walk_ptr walker,
		long line_number, long par_number, text_block_ptr block, point_start_ptr first_line,
		point_start_ptr last_line, point_start_ptr previous_first, point_start_ptr previous_last,
		co_ordinate_ptr offset_extra, rectangle_ptr vis_rect, short call_verb)
{
	point_start_ptr		starts;
	text_block_ptr		textblock;
	long				shape_qty, current_page;
	long				top_par_offset, top_page_line;

// First call the default (used in v2.12 for paragraph borders and tables)

	pgLineGlitterProc(pg, walker, line_number, par_number, block, first_line, last_line,
						previous_first, previous_last, offset_extra, vis_rect, call_verb);

// Check if the current line begins a new paragraph.
// (FOR LINE(S) -- Omit this "if" statement,  i.e. do ALL lines.)

	if (first_line->flags & NEW_PAR_BIT) {

		shape_qty = GetMemorySize(pg->wrap_area) - 1;

		current_page = first_line->r_num / shape_qty;
		textblock = UseMemory(pg->t_blocks);
	
	// Locate text block that contains current page

		while ((textblock->end_start.r_num / shape_qty) < current_page)
			textblock += 1;
		
		starts = (point_start_ptr)UseMemory(textblock->lines);
		
		while ((starts->r_num / shape_qty) != current_page)
			starts += 1;
		
		// We located the offset for the top of this page.
		// Check to see if top of paragraph and, if not, scan to next par
		// (FOR LINE(S) -- OMIT THIS IF STATEMENT AND ITS CODE

		if (!(starts->flags & NEW_PAR_BIT)) {
			
			while (!(starts->flags & PAR_BREAK_BIT))
				starts += 1;
			
			starts += 1;
		}
		
		top_par_offset = (long)starts->offset;
		top_par_offset += textblock->begin;
		UnuseMemory(textblock->lines);
		UnuseMemory(pg->t_blocks);

	// Compute first paragraph number of page.
	// (FOR LINE(S), use pgOffsetToLineNum() ).

		top_page_line = pgOffsetToParNum(pg->myself, top_par_offset);

	// Final comment -- For drawing line or paragraph numbers relative to the top
	// of every page, compute as (par_number - top_page_line) + 1.
	// FOR LINE(S) -- compute as (line_number - top_page_line) + 1.

	}
}


/* This hook is tgo replace line_adjust_proc.  Its purpose is to force extra line leading
or spacing to reverse to the line's descent instead of the line's ascent.  For example,
if you set fixed leading to, say, 24 point the extra whitespace is added to the top of the
line.  This hook causes a reversal of that so the extra whitespace is at the bottom of the
line. This code works for ALL cases of extra leading, spacing, etc. */

PG_PASCAL (void) pgAdjustToTop (paige_rec_ptr pg, pg_measure_ptr measure_info,
		point_start_ptr line_starts, pg_short_t num_starts,
		rectangle_ptr line_fit, par_info_ptr par_format)
{
	style_walk_ptr					walker;
	style_info_ptr					style;
	register point_start_ptr		starts;
	long							line_offset, global_begin;
	short							ascent;
	pg_short_t						line_index;
	
	// Call the default:

	pgLineAdjustProc(pg, measure_info, line_starts, num_starts, line_fit, par_format);
	
	if (!par_format->spacing)
		return;

	// Set up style run so we can compute the "real" line's ascent (not the fake one
	// due to line spacing or extra leading):

	walker = measure_info->styles;
	ascent = measure_info->adjusted_height - measure_info->adjusted_descent;
	global_begin = measure_info->block->begin;
	starts = line_starts;

	for (line_index = 0; line_index < num_starts; ++line_index, ++starts) {

		line_offset = (long)starts->offset;
		line_offset += global_begin;
		pgSetWalkStyle(walker, line_offset);
		style = walker->cur_style;
		
		if (ascent < style->ascent)
			ascent = style->ascent;
	}

	// Re-adjust the line record(s) so extra spacing is on the bottom.
	
	for (starts = line_starts, line_index = 0; line_index < num_starts; ++line_index, ++starts) {
		long			ascent_diff, current_ascent;
		
		current_ascent = (starts->bounds.bot_right.v - starts->bounds.top_left.v) - (long)starts->baseline;
		ascent_diff = current_ascent - (long)ascent;
	
	// Adjust for anything that we DO want to keep on the top, for example "top_extra" for
	// paragraphs. YOU MAY NEED TO MODIFY THIS CODE depending upon what you want.

		if (par_format->top_extra)
			if (line_starts->flags & NEW_PAR_BIT)
				ascent_diff -= par_format->top_extra;

		starts->baseline += (short)(ascent_diff);
	}
}


