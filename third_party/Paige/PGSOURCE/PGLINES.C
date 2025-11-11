/* This source file handles "line and paragraph numbering" utility functions
for PAIGE-based documents. Note, these generally work only if the "count lines"
attribute is set in the pg_ref. */

#include "Paige.h"
#include "pgText.h"
#include "pgEdit.h"
#include "pgUtils.h"


/* pgNumLines returns the number of lines in pg. This always returns zero
if COUNT_LINES_BIT is not set in paige flags. */

PG_PASCAL (long) pgNumLines (pg_ref pg)
{
	paige_rec_ptr			pg_rec;
	text_block_ptr			block;
	long					result;
	
	pg_rec = (paige_rec_ptr) UseMemory(pg);
	
	if (!(pg_rec->flags & COUNT_LINES_BIT))
		result = 0;
	else {
	
		block = (text_block_ptr) UseMemory(pg_rec->t_blocks);
		block += (GetMemorySize(pg_rec->t_blocks) - 1);
		
		if (block->flags & NEEDS_CALC)
			pgPaginateBlock(pg_rec, block, NULL, FALSE);
		
		result = block->first_line_num;
		result += (long)block->num_lines;
		
		UnuseMemory(pg_rec->t_blocks);
	}

	UnuseMemory(pg);
	
	return	result;
}


/* pgOffsetToLineNum returns the line number belonging to the offset text position.
This only works if COUNT_LINES_BIT is set. */

PG_PASCAL (long) pgOffsetToLineNum (pg_ref pg, long offset,
		pg_boolean line_end_has_precedence)
{
	paige_rec_ptr				pg_rec;
	register point_start_ptr	starts;
	text_block_ptr				block;
	pg_short_t					local_offset;
	long						result, use_offset;

	pg_rec = (paige_rec_ptr) UseMemory(pg);

	if (!(pg_rec->flags & COUNT_LINES_BIT))
		result = 0;
	else {
		
		use_offset = pgFixOffset(pg_rec, offset);
		block = pgFindTextBlock(pg_rec, use_offset, NULL, TRUE, TRUE);
		local_offset = (pg_short_t) ((long)(use_offset - block->begin));
		starts = (point_start_ptr) UseMemory(block->lines);
		
		result = block->first_line_num;
		
		for (;;) {
			
			for (;;) {
				
				++starts;
				
				if (starts->flags & NEW_LINE_BIT)
					break;
			}
			
			++result;
			
			if (starts->offset >= local_offset)
				break;
		}
		
		if (starts->offset == local_offset && starts->flags != TERMINATOR_BITS
			&& !line_end_has_precedence)
			++result;

		UnuseMemory(block->lines);
		UnuseMemory(pg_rec->t_blocks);
	}

	UnuseMemory(pg);
	
	return	result;
}



/* pgLineNumToOffset detertimes the positions at start and end of line_num
line number. */

PG_PASCAL (void) pgLineNumToOffset (pg_ref pg, size_t line_num,
		size_t PG_FAR *begin_offset, size_t PG_FAR *end_offset)
{
	paige_rec_ptr				pg_rec;
	text_block_ptr				block;
	register point_start_ptr	starts;
	long						local_begin, local_end;
	long						line_gauge, wanted_num;
	pg_boolean					terminated = FALSE;

	pg_rec = (paige_rec_ptr) UseMemory(pg);

	if (!(pg_rec->flags & COUNT_LINES_BIT))
		local_begin = local_end = 0;
	else {
	
		block = (text_block_ptr) UseMemory(pg_rec->t_blocks);
		wanted_num = line_num - 1;
	
		for (;;) {
			
			if (block->flags & NEEDS_CALC)
				pgPaginateBlock(pg_rec, block, NULL, TRUE);
			
			line_gauge = block->first_line_num + (long)block->num_lines;
			
			if (line_gauge > wanted_num)
				break;
			
			if (block->end >= pg_rec->t_length)
				break;
	
			++block;
		}
		
		starts = (point_start_ptr) UseMemory(block->lines);
		line_gauge = wanted_num - block->first_line_num + 1;
	
		while (line_gauge) {
			
			local_begin = (long)starts->offset;
			
			for (;;) {
				
				if (starts[1].flags == TERMINATOR_BITS) {
					
					terminated = TRUE;
					break;
				}
				
				++starts;
				
				if (starts->flags & NEW_LINE_BIT)
					break;
			}
			
			local_end = (long)starts->offset;
			
			if (local_end > (local_begin + 1))
				--local_end;

			--line_gauge;
		}

		UnuseMemory(block->lines);
		UnuseMemory(pg_rec->t_blocks);
	}

	if (begin_offset)
		*begin_offset = block->begin + local_begin;
	if (end_offset) {
		
		if (terminated)
			*end_offset = block->end;
		else
			*end_offset = block->begin + local_end;
	}
	
	UnuseMemory(pg);
}



/* pgNumPars returns the number of CR-terminated lines in pg. This always returns zero
if COUNT_LINES_BIT is not set in paige flags. */

PG_PASCAL (long) pgNumPars (pg_ref pg)
{
	paige_rec_ptr			pg_rec;
	text_block_ptr			block;
	long					result;
	
	pg_rec = (paige_rec_ptr) UseMemory(pg);
	
	if (!(pg_rec->flags & COUNT_LINES_BIT))
		result = 0;
	else {
	
		block = (text_block_ptr) UseMemory(pg_rec->t_blocks);
		block += (GetMemorySize(pg_rec->t_blocks) - 1);
		
		if (block->flags & NEEDS_CALC)
			pgPaginateBlock(pg_rec, block, NULL, FALSE);
		
		result = block->first_par_num;
		result += (long)block->num_pars;
		
		UnuseMemory(pg_rec->t_blocks);
	}

	UnuseMemory(pg);
	
	return	result;
}


/* pgOffsetToParNum returns the para number belonging to the offset text position.
This only works if COUNT_LINES_BIT is set. */

PG_PASCAL (long) pgOffsetToParNum (pg_ref pg, long offset)
{
	paige_rec_ptr				pg_rec;
	register point_start_ptr	starts;
	text_block_ptr				block;
	pg_short_t					local_offset;
	long						result;

	pg_rec = (paige_rec_ptr) UseMemory(pg);

	if (!(pg_rec->flags & COUNT_LINES_BIT))
		result = 0;
	else {
	
		block = pgFindTextBlock(pg_rec, offset, NULL, TRUE, TRUE);
		local_offset = (pg_short_t) ((long)(offset - block->begin));
		starts = (point_start_ptr) UseMemory(block->lines);
		
		result = block->first_par_num + 1;
		
		for (;;) {
		
			for (;;) {
				
				if (starts[1].flags == TERMINATOR_BITS)
					break;
				
				++starts;
				
				if (starts->flags & NEW_PAR_BIT)
					break;
			}
			
			if (starts->offset > local_offset)
				break;
			
			++result;
			
			if (starts[1].flags == TERMINATOR_BITS)
				break;
		}

		UnuseMemory(block->lines);
		UnuseMemory(pg_rec->t_blocks);
	}

	UnuseMemory(pg);
	
	return	result;
}


/* pgParNumToOffset detertimes the positions at start and end of line_num
para number. */

PG_PASCAL (void) pgParNumToOffset (pg_ref pg, size_t par_num,
		size_t PG_FAR *begin_offset, size_t PG_FAR *end_offset)
{
	paige_rec_ptr				pg_rec;
	text_block_ptr				block;
	register point_start_ptr	starts;
	size_t						local_begin, local_end;
	size_t						line_gauge, wanted_num;
	pg_boolean					terminated = FALSE;

	pg_rec = (paige_rec_ptr) UseMemory(pg);

	if (!(pg_rec->flags & COUNT_LINES_BIT))
		local_begin = local_end = 0;
	else {
	
		block = (text_block_ptr) UseMemory(pg_rec->t_blocks);
		wanted_num = par_num - 1;
	
		for (;;) {
			
			if (block->flags & NEEDS_CALC)
				pgPaginateBlock(pg_rec, block, NULL, TRUE);
			
			line_gauge = block->first_par_num + (long)block->num_pars;
			
			if (line_gauge > wanted_num)
				break;
			
			if (block->end >= pg_rec->t_length)
				break;
	
			++block;
		}
		
		starts = (point_start_ptr) UseMemory(block->lines);
		line_gauge = wanted_num - block->first_par_num + 1;
	
		while (line_gauge) {
			
			local_begin = (long)starts->offset;
			
			for (;;) {
				
				if (starts[1].flags == TERMINATOR_BITS) {
					
					terminated = TRUE;
					break;
				}
				
				++starts;
				
				if (starts->flags & NEW_PAR_BIT)
					break;
			}
			
			local_end = (long)starts->offset;
			
			if (local_end > (local_begin + 1))
				--local_end;

			--line_gauge;
		}

		UnuseMemory(block->lines);
		UnuseMemory(pg_rec->t_blocks);
	}

	if (begin_offset)
		*begin_offset = block->begin + local_begin;
	if (end_offset) {
		
		if (terminated)
			*end_offset = block->end;
		else
			*end_offset = block->begin + local_end;
	}
	
	UnuseMemory(pg);
}


/* pgLineNumToBounds returns the bounding rect that encloses line number line_num. */

PG_PASCAL (void) pgLineNumToBounds (pg_ref pg, long line_num,
		pg_boolean want_scrolled, pg_boolean want_scaled,
		rectangle_ptr bounds)
{
	select_pair				line_range;
	
	pgLineNumToOffset(pg, line_num, &line_range.begin, &line_range.end);
	pgTextRect(pg, &line_range, want_scrolled, want_scaled, bounds);
}


/* pgParNumToBounds returns the bounding rect that encloses para number par_num. */

PG_PASCAL (void) pgParNumToBounds (pg_ref pg, long par_num,
		pg_boolean want_scrolled, pg_boolean want_scaled, rectangle_ptr bounds)
{
	select_pair				par_range;
	
	pgParNumToOffset(pg, par_num, &par_range.begin, &par_range.end);

	if (par_range.end > par_range.begin)
		--par_range.end;

	pgTextRect(pg, &par_range, want_scrolled, want_scaled, bounds);
}


