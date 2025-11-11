/* This file handles text_block records in a Paige struct. */

#include "Paige.h"

#ifdef MAC_PLATFORM
#pragma segment pgtblocks
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
#include "pgtxr.h"
#include "pgSubRef.h"
#include "pgTables.h"

static void split_subref_list (text_block_ptr block1, text_block_ptr block2);
static void inval_selections (paige_rec_ptr pg);
static long find_breaking_char (paige_rec_ptr pg, text_block_ptr block,
		style_walk_ptr styles, long begin_offset, long byte_count,
		long desired_flags);


/* pgInitTextblock initializes a new text_block. Offset_begin is the
absolute offset beginning while the_text is the text (or NULL if none). */

PG_PASCAL (void) pgInitTextblock (paige_rec_ptr pg_rec, long offset_begin,
			text_ref the_text, text_block_ptr block, pg_boolean cache_text)
{
	point_start_ptr			starts;
	
	pgPushMemoryID(pg_rec);

	pgFillBlock(block, sizeof(text_block), 0);
	
	block->begin = block->end = offset_begin;
	block->file_os = CURRENT_OS;

	if (the_text) {
		
		block->text = the_text;
		block->end += GetMemorySize(the_text);
	}
	else
	if (!cache_text)
		block->text = MemoryAlloc(pg_rec->globals->mem_globals, sizeof(pg_char), 0, 32);

	block->lines = MemoryAllocClear(pg_rec->globals->mem_globals,
			sizeof(point_start), 2, 4);
	
	if (block->text)
		SetMemoryPurge(block->text, TEXT_PURGE_STATUS, FALSE);

	SetMemoryPurge(block->lines, LINE_PURGE_STATUS, FALSE);

	block->subref_list = MemoryAlloc(pg_rec->globals->mem_globals, sizeof(memory_ref), 0, 8);

	starts = (point_start_ptr) UseMemory(block->lines);
	starts->flags = NEW_LINE_BIT | LINE_BREAK_BIT;
	++starts;
	starts->flags = TERMINATOR_BITS;

	UnuseMemory(block->lines);

	block->flags = (NEEDS_CALC | NEEDS_PARNUMS);
	
	pgPopMemoryID(pg_rec);
}



/* pgBuildFakeBlock gets called when the first line of a block is below the
very last "container" rectangle of a non-repeating shape. In such a case, there
is no reason at all to figure out the line positions since they will never
draw. This function, therefore, computes one large massive line for performance
increase. */

PG_PASCAL (void) pgBuildFakeBlock (paige_rec_ptr pg, pg_measure_ptr line_stuff)
{
	register pg_measure_ptr		line_info;
	register point_start_ptr	starts;
	text_block_ptr				block;
	rectangle					wrap_bounds;
	long						local_size;

	line_info = line_stuff;
	starts = line_info->starts;
	block = line_info->block;

	local_size = block->end - block->begin;
	
	pgFillBlock(starts, sizeof(point_start) * 2, 0);
	pgShapeBounds(pg->wrap_area, &wrap_bounds);
	
	starts->flags = NEW_LINE_BIT | LINE_BREAK_BIT | LINE_GOOD_BIT;
	starts->r_num = line_info->r_num_begin;
	starts->bounds = wrap_bounds;
	
	starts->bounds.bot_right.v += 2;
	starts->bounds.top_left.v = starts->bounds.bot_right.v - 1;
	block->bounds = starts->bounds;

	starts[1].bounds = starts->bounds;
	starts[1].offset = (pg_short_t)local_size;
	starts[1].flags = TERMINATOR_BITS;
	
	pgBlockMove(starts, &block->end_start, sizeof(point_start));
	
	block->flags |= BELOW_CONTAINERS;
}


/* DEFAULT FUNCTION TO DO "TEXT LOAD" FOR A BLOCK. This does not do anything
by default. Its purpose is future features of text paging from disk.  */

PG_PASCAL (void) pgTextLoadProc (paige_rec_ptr pg, text_block_ptr block)
{
	if (pg->cache_read_proc && pg->cache_file && !block->text) {
		pg_globals_ptr		globals;
		pg_char_ptr			text;
		size_t				file_position, data_size, byte_size;

		globals = pg->globals;
		data_size = byte_size = (block->end - block->begin);
		file_position = block->cache_begin;

		byte_size = data_size;

#ifdef UNICODE

		if (block->cache_flags & CACHE_UNICODE_FLAG) {
			
			data_size += 1;		// Add one for BOM prefix
			byte_size = data_size * sizeof(pg_char);
		}

		block->text = MemoryAllocID(globals->mem_globals, sizeof(pg_char), data_size, 32, pg->mem_id);		
		text = (pg_char_ptr) UseMemory(block->text);
		pg->cache_read_proc((void PG_FAR *)text, io_data_direct, &file_position, &byte_size, pg->cache_file);
		
		UnuseMemory(block->text);
		
		pgBlockToUnicode(pg, block);
#else

		if (block->cache_flags & CACHE_UNICODE_FLAG) {

			data_size += 1;
			byte_size = data_size * sizeof(pg_char);
		}

		block->text = MemoryAllocID(globals->mem_globals, 1, byte_size, 32, pg->mem_id);		
		text = (pg_char_ptr) UseMemory(block->text);
		pg->cache_read_proc((void PG_FAR *)text, io_data_direct, &file_position, &byte_size, pg->cache_file);
		
		UnuseMemory(block->text);
		pgUnicodeToBlock(pg, block);
#endif
		
		if (block->file_os != CURRENT_OS)
			if (!(block->cache_flags & CACHE_UNICODE_FLAG))
				pgMapCharacters(pg, block);
	}
}


/* pgBlockToUnicode gets called after a text block is read and we have determined that its
text needs to be converted to Unicode. Note that the text might ALREADY by unicode,  in which
case we simply strip the Byte Order Mark. */

PG_PASCAL (void) pgBlockToUnicode (paige_rec_ptr pg, text_block_ptr block)
{
	style_walk			walker;
	memory_ref			output_ref;
	pg_short_t PG_FAR	*unicode_ptr;
	pg_bits8_ptr		bytes_ptr;
	long				text_index, byte_size, char_size, pre_size;

	byte_size = GetByteSize(block->text);
	
	if (GetMemoryRecSize(block->text) == 1)
		SetMemoryRecSize(block->text, sizeof(pg_short_t), 32);

	if (block->end == block->begin)
		return;

	bytes_ptr = (pg_bits8_ptr) UseMemory(block->text);
	
	if (pgBytesToUnicode(bytes_ptr, NULL, NULL, byte_size)) {  // Already Unicode
		
		unicode_ptr = (pg_short_t PG_FAR *)bytes_ptr;
		char_size = pgUnicodeToUnicode(unicode_ptr, byte_size / 2, FALSE);
		UnuseMemory(block->text);
		SetMemorySize(block->text, char_size);
	}
	else {
		
		output_ref = MemoryAlloc(pg->globals->mem_globals, sizeof(pg_short_t), 0, 128);
		
		text_index = 0;
		pgPrepareStyleWalk(pg, text_index, &walker, FALSE);

		while (text_index < block->end) {
			
			if (walker.next_style_run->offset < block->end)
				byte_size = walker.next_style_run->offset - text_index;
			else
				byte_size = block->end - text_index;
			
			pre_size = GetMemorySize(output_ref);
			unicode_ptr = (pg_short_t *) AppendMemory(output_ref, byte_size, FALSE);
			char_size = walker.cur_style->procs.bytes_to_unicode(bytes_ptr, unicode_ptr, walker.cur_font, byte_size);
			UnuseMemory(output_ref);
			
			if (char_size != byte_size)
				SetMemorySize(output_ref, pre_size + char_size);

			bytes_ptr += byte_size;
			text_index += byte_size;
			pgSetWalkStyle(&walker, text_index);
		}
		
		UnuseMemory(block->text);
		MemoryCopy(output_ref, block->text);
		DisposeMemory(output_ref);
		pgPrepareStyleWalk(pg, 0, NULL, FALSE);
	}
}


/* pgUnicodeToBlock gets called when the block is currently unicode and it must be translated
to bytes. */

PG_PASCAL (void) pgUnicodeToBlock (paige_rec_ptr pg, text_block_ptr block)
{
	style_walk			walker;
	pg_bits8_ptr		bytes_ptr;
	pg_short_t PG_FAR	*unicode_ptr;
	long				text_index, byte_size, char_size;

	byte_size = GetByteSize(block->text);
	bytes_ptr = (pg_bits8_ptr) UseMemory(block->text);
	
	if (pgBytesToUnicode(bytes_ptr, NULL, NULL, byte_size)) {

		text_index = 0;
		pgPrepareStyleWalk(pg, text_index, &walker, FALSE);
		unicode_ptr = (pg_short_t PG_FAR *)bytes_ptr;

		while (text_index < block->end) {
			
			if (walker.next_style_run->offset < block->end)
				byte_size = walker.next_style_run->offset - text_index;
			else
				byte_size = block->end - text_index;
			
			char_size = walker.cur_style->procs.unicode_to_bytes(unicode_ptr, bytes_ptr,
						walker.cur_font, byte_size / 2);
			
			bytes_ptr += char_size;
			unicode_ptr += byte_size;
			text_index += byte_size;

			pgSetWalkStyle(&walker, text_index);
		}
		
		pgPrepareStyleWalk(pg, 0, NULL, FALSE);
	}

	UnuseMemory(block->text);
}


/* pgTextBreakProc must find the best place to break a text block (it is too
large per pg_globals->max_block). The default looks for a CR first, then if not
found looks for the alternate break character, then if not found makes its
best guess as to where to break. */

PG_PASCAL (long) pgTextBreakProc (paige_rec_ptr pg, text_block_ptr block)
{
	style_walk				walker;
	long					text_size, quarter_size, eighth_size;
	long					result;

	pg->procs.load_proc(pg, block);
	pgPrepareStyleWalk(pg, block->begin, &walker, TRUE);

	text_size = block->end - block->begin;
	quarter_size = text_size / 4;
	eighth_size = quarter_size / 2;

	if ((result = find_breaking_char(pg, block, &walker, quarter_size + eighth_size,
			(quarter_size * 2 - eighth_size), PAR_SEL_BIT)) <= 0) {

		if ((result = find_breaking_char(pg, block, &walker, quarter_size,
				quarter_size * 2, PAR_SEL_BIT)) <= 0) {
			
			if (result == 0) {
			
				if (!(result = find_breaking_char(pg, block, &walker, quarter_size + eighth_size,
						(quarter_size * 2) - eighth_size, WORD_BREAK_BIT))) {
					pg_char_ptr			text;
					long				initial_result, max_offset;
					
					result = text_size / 2;
					pgSetWalkStyle(&walker, block->begin + result);
					text = (pg_char_ptr) UseMemory(block->text);
					initial_result = result;
					max_offset = block->end - block->begin;

					while (result < text_size) {
						
						if (!walker.cur_par_style->table.table_columns)
							if (!(walker.cur_style->procs.char_info(pg, &walker, text, block->begin,
									initial_result, max_offset, result, LAST_HALF_BIT | MIDDLE_CHAR_BIT)))
								break;

						pgWalkStyle(&walker, 1);
						result += 1;
					}
					
					UnuseMemory(block->text);
					
					if (result == text_size)
						result = 0;
				}			
			}
		}
	}

	pgPrepareStyleWalk(pg, 0, NULL, TRUE);
	
	if (result < 0)
		return	0;

	return	result;
}



/* pgAdjustDirection reverses direction of all point_start records
in this block. It is assumed that the text direction at pg->text_direction is
now the correct direction. */

PG_PASCAL (void) pgAdjustDirection (paige_rec_ptr pg, text_block_ptr block)
{
	register point_start_ptr	starts, first_start;
	register pg_short_t			num_starts;
	rectangle					wrap_bounds;
	style_walk					walker;
	long						style_advance;

	pgShapeBounds(pg->wrap_area, &wrap_bounds);

	if (!(block->flags & LINES_PURGED)) {

		pgPrepareStyleWalk(pg, block->begin, &walker, TRUE);
		block->bounds.top_left.h = block->bounds.bot_right.h;
		
		starts = (point_start_ptr) UseMemory(block->lines);
		
		while (starts->flags != TERMINATOR_BITS) {
			
			for (first_start = starts, num_starts = 1; !(starts->flags & LINE_BREAK_BIT);
					++num_starts, ++starts) ;
			
			if (walker.cur_par_style->direction == system_direction)
				pgAdjustRightLeftStarts(pg, &wrap_bounds, (short)pg->text_direction, first_start, num_starts);

			starts = first_start;

			while (num_starts) {
				
				pgLineUnionRect(starts, &block->bounds);
				
				++starts;
				--num_starts;
			}
			
			style_advance = starts->offset;
			pgSetWalkStyle(&walker, block->begin + style_advance);
		}
		
		UnuseMemory(block->lines);

		pgPrepareStyleWalk(pg, 0, NULL, TRUE);
	}
	else
		pgFlipRect(&wrap_bounds, &block->bounds, (short)pg->text_direction);

	block->flags &= (~SWITCHED_DIRECTIONS);
}


/* pgLineUnionRect combines the bounds rect in starts with target. */

PG_PASCAL (void) pgLineUnionRect (point_start_ptr starts, rectangle_ptr target)
{
	if (!starts->cell_height)
		pgTrueUnionRect(&starts->bounds, target, target);
	else {
		rectangle		source;
		
		source = starts->bounds;
		source.bot_right.v += (long)starts->cell_height;
		pgTrueUnionRect(&source, target, target);
	}
}

/* pgRectFromSelection fills in a rectangle that encloses the line which
corresponds to a selection point. The function returns the absolute location
of the "caret" point.  The block must be the correct block that contains the
point. Note that line_rect can be NULL if you just want the caret point.	*/

PG_PASCAL (long) pgRectFromSelection (paige_rec_ptr pg, text_block_ptr block,
		t_select_ptr selection, rectangle_ptr line_rect, short PG_FAR *baseline)
{
	register point_start_ptr	first_start, alternate_start;
	long						caret_point;
	pg_short_t					begin_start, num_starts;

	alternate_start = first_start = (point_start_ptr) UseMemory(block->lines);
	alternate_start += selection->line;

	if (baseline)
		*baseline = alternate_start->baseline;

	caret_point = alternate_start->bounds.top_left.h + (long) selection->primary_caret;
	
	if (line_rect) {
	
		begin_start = selection->line;
		num_starts = pgLineBoundary(pg, block, &begin_start, NULL, NULL) - 1;

		first_start += begin_start;
		alternate_start = first_start + num_starts;
		
		line_rect->top_left = first_start->bounds.top_left;
		line_rect->bot_right = alternate_start->bounds.bot_right;
	}
	
	UnuseMemory(block->lines);
	
	return	caret_point;
}


/* pgNumTextblocks is intended for apps that want to examine blocks. This
one returns the total number of text blocks in pg.  */

PG_PASCAL (long) pgNumTextblocks (pg_ref pg)
{
	paige_rec_ptr			pg_rec;
	long					result;
	
	pg_rec = (paige_rec_ptr) UseMemory(pg);
	result = GetMemorySize(pg_rec->t_blocks);
	UnuseMemory(pg);
	
	return	result;
}


/* pgGetTextblock is intended for apps that want to examine blocks. This
one returns a text_block record that owns the specified offset.  The function
result is the record number.  If want_pagination is TRUE the block's lines are
re-calculated if necessary.  */

PG_PASCAL (long) pgGetTextblock (pg_ref pg, long offset, text_block_ptr block,
		pg_boolean want_pagination)
{
	paige_rec_ptr			pg_rec;
	text_block_ptr			the_block;
	long					wanted_offset;
	pg_short_t				rec_num_result;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	
	wanted_offset = pgFixOffset(pg_rec, offset);
	the_block = pgFindTextBlock(pg_rec, wanted_offset, &rec_num_result,
			want_pagination, want_pagination);
	pgBlockMove(the_block, block, sizeof(text_block));
	UnuseMemory(pg_rec->t_blocks);

	UnuseMemory(pg);
	
	return	rec_num_result;
}



/* pgFindTextBlock returns a text_block_ptr of the block that owns "offset."
if block_num is non-NULL, the block number is returned (0 to n).
If want_build is TRUE, the block is recalculated if necessary. The will_access_text
param is passed to  (if called).  */

PG_PASCAL (text_block_ptr) pgFindTextBlock (paige_rec_ptr pg_rec, long offset,
			pg_short_t PG_FAR *block_num, pg_boolean want_build,
			pg_boolean will_access_text)
{
	register text_block_ptr		block;
	register long				wanted_offset;
	pg_short_t					num_blocks, block_ctr;

	num_blocks = (pg_short_t)GetMemorySize(pg_rec->t_blocks) - 1;
	block = (text_block_ptr) UseMemory(pg_rec->t_blocks);
	
	wanted_offset = offset;
	block_ctr = 0;

	while (num_blocks) {
		
		if (block->end > wanted_offset)
			break;
		
		++block;
		++block_ctr;
		--num_blocks;
	}

	if (will_access_text)
		pg_rec->procs.load_proc(pg_rec, block);

	if (want_build)
		pgPaginateBlock(pg_rec, block, NULL, will_access_text);

	if (block_num)
		*block_num = block_ctr;

	return block;
}



/* pgFixBadBlocks walks through all the text blocks and deletes blocks that are
completely empty, or splits blocks that are too large.  */

PG_PASCAL (short) pgFixBadBlocks (paige_rec_ptr pg)
{
	register text_block_ptr	block;
	register pg_short_t		num_blocks, block_ctr;
	pg_short_t				charloc_index;
	long					block_size, max_block_size;
	short					some_fixed;

	max_block_size = pg->globals->max_block_size;
	block = (text_block_ptr) UseMemory(pg->t_blocks);
	num_blocks = (pg_short_t)GetMemorySize(pg->t_blocks);
	block_ctr = 0;
	some_fixed = FALSE;

	while (num_blocks) {
		
		block_size = block->end - block->begin;

		if (some_fixed)
			block->flags |= NEEDS_PAGINATE;

		if (((block->flags & BROKE_BLOCK) && (num_blocks > 1)) || (!(block_size))) {

			inval_selections(pg);
			
			if (block_size) {	/* Block has no terminator, merge with next */
				text_ref		src_text_ref;
				pg_char_ptr		inserted_text;
				rectangle		old_bounds;
				long			old_block_begin, old_text_size;
				
				some_fixed = TRUE;
				pg->procs.load_proc(pg, block);
				block->cache_flags |= CACHE_CHANGED_FLAG;
				src_text_ref = block->text;
				old_block_begin = block->begin;
				old_text_size = block->end - block->begin;

				pgBlockMove(&block->bounds, &old_bounds, sizeof(rectangle));

				++block;
				pg->procs.load_proc(pg, block);
				block->cache_flags |= CACHE_CHANGED_FLAG;
				block->begin = old_block_begin;
				pgTrueUnionRect(&old_bounds, &block->bounds, &block->bounds);
				
				inserted_text = (pg_char_ptr) InsertMemory(block->text, 0, old_text_size);
				pgBlockMove(UseMemory(src_text_ref), inserted_text, old_text_size * sizeof(pg_char));
				UnuseMemory(block->text);
				UnuseMemory(src_text_ref);
				
			{
				memory_ref			src_subrefs;
				pg_subref_ptr		subref_list;
				long				num_subs;
				
				--block;
				src_subrefs = block->subref_list;
				++block;
				
				if ((num_subs = GetMemorySize(src_subrefs)) > 0) {
					
					subref_list = (pg_subref_ptr) InsertMemory(block->subref_list, 0, num_subs);
					pgBlockMove(UseMemory(src_subrefs), subref_list, num_subs * sizeof(pg_subref));
					UnuseMemory(src_subrefs);
					SetMemorySize(src_subrefs, 0);
				}
			}
				block->flags = (NEEDS_CALC | NEEDS_PARNUMS);
				
				if (pgFindCharLocs(pg, block, &charloc_index, NULL, TRUE, FALSE))
					pgFreeCharLocs(pg, charloc_index);
				
				--block;
			}
			
			if (block->begin || (num_blocks > 1)) {

				some_fixed = TRUE;

				if (pgFindCharLocs(pg, block, &charloc_index, NULL, TRUE, FALSE))
					pgFreeCharLocs(pg, charloc_index);

				DisposeMemory(block->lines);
				DisposeNonNilMemory(block->text);

				if (block->subref_list)
					DisposeMemory(block->subref_list);

				UnuseMemory(pg->t_blocks);
				DeleteMemory(pg->t_blocks, block_ctr, 1);

				block = (text_block_ptr) UseMemory(pg->t_blocks);
				block += block_ctr;
				
				if (num_blocks == 1)
					break;
				
				block->flags = (NEEDS_CALC | NEEDS_PARNUMS);
			}
		}
		else {

			if (block_size > max_block_size) {
				text_ref		new_text_ref;
				pg_char_ptr		src_text;
				long			old_bounds_bottom;
				long			split_offset, new_text_size, old_text_size;
				
				pg->procs.load_proc(pg, block);
				block->cache_flags |= CACHE_CHANGED_FLAG;

				if (split_offset = pg->procs.break_proc(pg, block)) {

					pgPushMemoryID(pg);
	
					inval_selections(pg);
					some_fixed = TRUE;
					
					split_offset += block->begin;
					new_text_size = block->end - split_offset;
					block->end = split_offset;
					old_text_size = (block->end - block->begin);
					
					new_text_ref = MemoryAlloc(pg->globals->mem_globals, sizeof(pg_char), new_text_size, 32);
					
					pg->procs.load_proc(pg, block);
	
					src_text = (pg_char_ptr) UseMemoryRecord(block->text, old_text_size, new_text_size - 1, TRUE);
					pgBlockMove(src_text, UseMemory(new_text_ref), new_text_size * sizeof(pg_char));
					UnuseMemory(block->text);
					UnuseMemory(new_text_ref);
					SetMemorySize(block->text, old_text_size);
					
					block->flags = (NEEDS_CALC | NEEDS_PARNUMS);
					old_bounds_bottom = block->bounds.bot_right.v;
					++block_ctr;
					
					block = (text_block_ptr) InsertMemory(pg->t_blocks, block_ctr, 1);
					
					pgInitTextblock(pg, split_offset, new_text_ref, block, FALSE);
					block->bounds.top_left.v = block->bounds.bot_right.v = old_bounds_bottom;

					--block;
					split_subref_list(block, &block[1]);
					++block;

					pgPopMemoryID(pg);
				}
			}
			
			++block;
			++block_ctr;
		}
		
		--num_blocks;
	}

	UnuseMemory(pg->t_blocks);
	
	pg->last_key_check = 0;
	
	if (some_fixed)
		pg->t_blocks = ForceMinimumMemorySize(pg->t_blocks);
	
	return	some_fixed;
}




/* pgTextFromOffset returns a <<used>> pointer to the text at the specified offset.
The memory_ref for the text is returned in the_ref;  the maximum size of text --
which is the offset to end of text block -- is returned in max_length. The
max_length param can be NULL if you just want the pointer. */

PG_PASCAL (pg_char_ptr) pgTextFromOffset (paige_rec_ptr pg, long offset,
		text_ref PG_FAR *the_ref, long PG_FAR *max_length)
{
	register text_block_ptr		block;
	register pg_char_ptr		text_result;
	register long				local_offset;

	if (offset >= pg->t_length)
		return	NULL;

	block = pgFindTextBlock(pg, offset, NULL, FALSE, TRUE);
	*the_ref = block->text;
	
	if (max_length)
		*max_length = block->end - offset;

	local_offset = offset - block->begin;

	UnuseMemory(pg->t_blocks);
	
	text_result = (pg_char_ptr) UseMemory(*the_ref);
	text_result += local_offset;
	
	return	text_result;
}

/* pgCalcTableSpace determines what vertical space, if any, exists between potential table
cells in each table column.  If there are no tables in this block then this function does
nothing. */

PG_PASCAL (void) pgCalcTableSpace (paige_rec_ptr pg, text_block_ptr block)
{
	register point_start_ptr		starts, look_ahead_starts;
	style_walk						walker;
	rectangle						bounds;
	long							abs_offset, num_starts;
	long							real_bottom;
	pg_short_t						flags;

	pgPrepareStyleWalk(pg, block->begin, &walker, TRUE);
	starts = (point_start_ptr) UseMemory(block->lines);

	while (starts->flags != TERMINATOR_BITS) {

		abs_offset = (long)starts->offset;
		abs_offset += block->begin;
		pgSetWalkStyle(&walker, abs_offset);
		
		if (walker.cur_par_style->table.table_columns) { // We hit a table
			
			pgTableRowBounds(starts, &bounds);

			for (;;) {

				look_ahead_starts = starts;
				num_starts = 0;
				
				for (;;) {
					
					flags = look_ahead_starts->flags;

					if (flags == TERMINATOR_BITS)
						break;

					++look_ahead_starts;
					++num_starts;
					
					if (flags & (LINE_BREAK_BIT | TAB_BREAK_BIT))
						break;
				}
				
				if (flags & (TAB_BREAK_BIT | PAR_BREAK_BIT))
					real_bottom = bounds.bot_right.v;
				else
					real_bottom = look_ahead_starts->bounds.top_left.v;

				while (num_starts) {
					
					starts->cell_height = (pg_short_t)(real_bottom - starts->bounds.bot_right.v);
					
					if (block->end_start.offset == starts->offset)
						block->end_start.cell_height = starts->cell_height;

					++starts;
					--num_starts;
				}

				if (flags & PAR_BREAK_BIT)
					break;
			}
		}
		else {
			
			for (;;) {
				
				flags = starts->flags;
				++starts;
				
				if (flags & LINE_BREAK_BIT)
					break;
			}
		}
	}

	UnuseMemory(block->lines);
	pgPrepareStyleWalk(pg, 0, NULL, TRUE);
}


/*********************************** Local Functions **********************************/


/* This function flags all selection points to require recalculating  */

static void inval_selections (paige_rec_ptr pg)
{
	register t_select_ptr		selections;
	register pg_short_t			num_selects;
	
	selections = (t_select_ptr) UseMemory(pg->select);
	
	if (!(num_selects = pg->num_selects))
		selections->flags |= SELECTION_DIRTY;
	else {
		
		num_selects *= 2;

		while (num_selects) {
			
			selections->flags |= SELECTION_DIRTY;
			++selections;
			--num_selects;
		}
	}
	
	UnuseMemory(pg->select);
}


/* find_breaking_char scans the text in block beginning at begin_offset
for byte_count bytes. The scan occurs forward (1).
The desired_flags indicate the kind of break being searched, e.g. CR or
LF or word break, etc.  A non-zero response means the offset has been located. */

static long find_breaking_char (paige_rec_ptr pg, text_block_ptr block,
		style_walk_ptr styles, long begin_offset, long byte_count,
		long desired_flags)
{
	register style_walk_ptr		walker;
	pg_char_ptr					text;
	long						offset_ctr, remaining_bytes, max_offset, c_info;

	walker = styles;
	pgSetWalkStyle(walker, block->begin + begin_offset);
	text = (pg_char_ptr) UseMemory(block->text);
	max_offset = block->end - block->begin;

	remaining_bytes = byte_count;
	offset_ctr = begin_offset;

	while (remaining_bytes) {

		if (c_info = walker->cur_style->procs.char_info(pg, walker, text, block->begin,
				begin_offset, max_offset, offset_ctr, desired_flags))
			break;
		
		pgWalkStyle(walker, 1);
		remaining_bytes -= 1;
		offset_ctr += 1;
	}
	
	while (remaining_bytes) {
		
		pgWalkStyle(walker, 1);
		remaining_bytes -= 1;
		offset_ctr += 1;
		
		if (!(walker->cur_style->procs.char_info(pg, walker, text, block->begin,
				begin_offset, max_offset, offset_ctr, LAST_HALF_BIT | MIDDLE_CHAR_BIT)))
			break;
	}

	UnuseMemory(block->text);
	
	if (remaining_bytes) {
		
		if (walker->cur_par_style->table.table_columns) {
			
			if (desired_flags != PAR_SEL_BIT)
				return	-1;
		}
		
		return	offset_ctr;
	}
	
	return	0;
}


static void split_subref_list (text_block_ptr block1, text_block_ptr block2)
{
	register pg_char_ptr		text;
	pg_subref_ptr				subref_list;
	long						num_subs, split_refs, text_size, list1_size;
	
	if (num_subs = GetMemorySize(block1->subref_list)) {
		
		text_size = GetMemorySize(block2->text);
		text = (pg_char_ptr) UseMemory(block2->text);
		split_refs = 0;

		while (text_size) {
			
			if (*text++ == SUBREF_CHAR)
				split_refs += 1;
			
			--text_size;
		}
		
		UnuseMemory(block2->text);
		
		if (split_refs) {
			
			SetMemorySize(block2->subref_list, split_refs);
			subref_list = (pg_subref_ptr) UseMemory(block1->subref_list);
			list1_size = num_subs - split_refs;
			pgBlockMove(&subref_list[list1_size], UseMemory(block2->subref_list),
						split_refs * sizeof(pg_subref));
			UnuseMemory(block2->subref_list);
			UnuseMemory(block1->subref_list);
			SetMemorySize(block1->subref_list, list1_size);
		}
	}
}
