/* This file handles paragraph formatting. Your app can omit this library if no
functions that set paragraph styles defined in Paige.h are called.    */

#include "Paige.h"

#include "defProcs.h"
#include "machine.h"
#include "pgText.h"
#include "pgSelect.h"
#include "pgEdit.h"
#include "pgDefStl.h"
#include "pgUtils.h"
#include "pgDefPar.h"
#include "pgPar.h"
#include "pgShapes.h"
#include "pgErrors.h"
#include "pgTables.h"
#include "pgTxtWid.h"

static par_info_ptr get_table_format (paige_rec_ptr pg, long position);
static memory_ref build_column_widths (paige_rec_ptr pg, par_info_ptr par);
static long table_leftside (paige_rec_ptr pg, long position, par_info_ptr par);
static long tab_to_cell (paige_rec_ptr pg_rec, long current_cell, pg_boolean advance_row);
static void column_insert_position (paige_rec_ptr pg_rec, short column_num, select_pair_ptr range,
				long PG_FAR *row_end);
static void row_insert_position (paige_rec_ptr pg_rec, long row_num, select_pair_ptr range);
static void clear_selections (paige_rec_ptr pg_rec, long new_position, short draw_mode);
static void adjust_overlapping_pages (paige_rec_ptr pg, pg_measure_ptr line_info);
static void set_info_rects (paige_rec_ptr pg, pg_measure_ptr line_info);
static void left_right_cell_extra (par_info_ptr par, long column_num, long PG_FAR *left_extra,
		long PG_FAR *right_extra);
static long column_width (par_info_ptr par, long column_num);
static pg_short_t change_to_text (change_info_ptr change, style_run_ptr style);
static pg_short_t change_to_table (change_info_ptr change, style_run_ptr style);
static pg_short_t count_tabs (paige_rec_ptr pg_rec, select_pair_ptr selection);


#define MINIMUM_CELL_WIDTH	16


/* pgInsertTable inserts a new table beginning at position.  If the position does not begin
a new paragraph,  a new paragraph is started. The column alignments are an optional array of
longs, hiword = alignment, loword = column width (0 = default). */

PG_PASCAL (void) pgInsertTable (pg_ref pg, long position, pg_table_ptr table,
		long row_qty, short draw_mode)
{
	paige_rec_ptr		pg_rec;
	memory_ref			row_data;
	par_info			def_par, original_par, mask;
	select_pair			table_range;
	pg_char_ptr			row, setup;
	pg_char				insertion[2];
	long				num_rows, row_ctr, num_columns, text_size, pensize, update_begin;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	table_range.begin = pgFixOffset(pg_rec, position);
	
	if (table_range.begin) {
		pg_short_t			charbytes;
		
		charbytes = pgCharByte(pg, table_range.begin - 1, insertion);
		
		if (insertion[0] != 0x0D || charbytes != 1) {
				
			insertion[0] = 0x0D;
			pgInsert(pg, insertion, 1, table_range.begin, data_insert_mode, 0, draw_none);
			table_range.begin += 1;
		}
	}

	update_begin = table_range.end = table_range.begin;
	pgGetParInfo(pg, &table_range, FALSE, &def_par, &mask);
	original_par = def_par;

	def_par.table = *table;
	pgFillBlock(def_par.tabs, sizeof(tab_stop) * TAB_ARRAY_SIZE, 0);
	pgFillBlock(&mask, sizeof(par_info), -1);

	def_par.table.unique_id = pgUniqueID(pg);
	if (!(num_rows = row_qty))
		num_rows = 1;

	if (!def_par.table.table_columns)
		def_par.table.table_columns = 1;
	else
	if (def_par.table.table_columns > TAB_ARRAY_SIZE)
		def_par.table.table_columns = TAB_ARRAY_SIZE;

	def_par.num_tabs = (short)def_par.table.table_columns;

	pensize = ((def_par.table.border_info & 0xC0000000) >> 30) + 1;
	
	if (pensize > def_par.table.border_spacing)
		def_par.table.border_spacing = pensize;

	if (def_par.table.table_column_width) {
		
		for (num_columns = 0; num_columns < def_par.table.table_columns; ++num_columns) {
			
			def_par.tabs[num_columns].position = def_par.table.table_column_width;
			def_par.tabs[num_columns].tab_type = left_tab;
			def_par.tabs[num_columns].leader = table->border_info;
			def_par.tabs[num_columns].ref_con = table->border_shading;

			if (num_columns == TAB_ARRAY_SIZE)
				break;
		}
	}

	def_par.table.border_shading = def_par.table.border_info = 0;

	pensize = ((def_par.table.cell_borders & 0xC0000000) >> 30) + 1;
	
	if (pensize < 2)
		pensize = 2;

	def_par.table.cell_h_extra = pensize;
	text_size = def_par.table.table_columns * num_rows;
	table_range.end += text_size;

	row_data = MemoryAlloc(pg_rec->globals->mem_globals, sizeof(pg_char), text_size, 0);

	row = setup = (pg_char_ptr) UseMemory(row_data);
	
	for (row_ctr = 0; row_ctr < num_rows; ++row_ctr) {

		for (num_columns = 0; num_columns < (def_par.table.table_columns - 1); ++num_columns)
			row[num_columns] = 0x09;
	
		row[num_columns] = 0x0D;

		row += def_par.table.table_columns;
	}

	pgInsert(pg, setup, text_size, table_range.begin, data_insert_mode, 0, draw_none);
	UnuseAndDispose(row_data);
	
	pgSetParInfo(pg, &table_range, &def_par, &mask, draw_none);
	
	pgFillBlock(&original_par.table, sizeof(pg_table), 0);
	original_par.procs = pg_rec->globals->def_par.procs;
	table_range.begin = table_range.end;
	pgSetParInfo(pg, &table_range, &original_par, &mask, draw_none);
	
	if (draw_mode)
		pgUpdateText(pg_rec, NULL, update_begin, pg_rec->t_length, MEM_NULL, NULL,
					bits_copy, TRUE);

	UnuseMemory(pg);
}


/* pgIsTable returns TRUE if the position is inside a table. */

PG_PASCAL (pg_boolean) pgIsTable (pg_ref pg, long position)
{
	paige_rec_ptr	pg_rec;
	pg_boolean		result = FALSE;
	
	pg_rec = (paige_rec_ptr) UseMemory(pg);
	result = pgPositionInTable(pg_rec, position, NULL);
	UnuseMemory(pg);
	
	return	result;
}


/* pgPtInTable returns a column num if the point is somewhere within a table. If positions is non-NULL
it gets set to the beginning and ending text positions for the cell containing the point. */

PG_PASCAL (short) pgPtInTable (pg_ref pg, co_ordinate_ptr point, rectangle_ptr column_sides,
		select_pair_ptr offsets)
{
	paige_rec_ptr			pg_rec;
	co_ordinate				the_point;
	short					result = 0;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	
	the_point = *point;	
	pg_rec->port.scale.scale = -pg_rec->port.scale.scale;
	pgScalePt(&pg_rec->port.scale, NULL, &the_point);
	pg_rec->port.scale.scale = -pg_rec->port.scale.scale;
	
	pgAddPt(&pg_rec->scroll_pos, &the_point);
	result = pgPtToTableOffset(pg_rec, &the_point, offsets, column_sides, NULL);
	UnuseMemory(pg);
	
	return	result;
}


/* pgTableColumnWidths returns a memory_ref containing an array of longs, each representing
the width of each column. */

PG_PASCAL (memory_ref) pgTableColumnWidths (pg_ref pg, long position)
{
	paige_rec_ptr		pg_rec;
	par_info_ptr		par;
	memory_ref			result = MEM_NULL;

	pg_rec = (paige_rec_ptr) UseMemory(pg);

	if ((par = get_table_format(pg_rec, position)) != NULL) {
		
		result = build_column_widths(pg_rec, par);
		UnuseMemory(pg_rec->par_formats);
	}

	UnuseMemory(pg);
	
	return	result;
}


/* pgCellOffsets returns the text positions(s) for the cell containing postion. */

PG_PASCAL (void) pgCellOffsets (pg_ref pg, long position, select_pair_ptr offsets)
{
	paige_rec_ptr			pg_rec;
	
	pg_rec = (paige_rec_ptr) UseMemory(pg);
	pgPositionInTable(pg_rec, pgFixOffset(pg_rec, position), offsets);
	UnuseMemory(pg);
}


/* pgSetColumnBorders sets the border info for a column. */

PG_PASCAL (void) pgSetColumnBorders (pg_ref pg, long position, short column_num,
		long border_info, short draw_mode)
{
	paige_rec_ptr			pg_rec;
	par_info				par, mask;
	select_pair				table_range;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	table_range.begin = table_range.end = pgFixOffset(pg_rec, position);
	pgGetParInfo(pg, &table_range, FALSE, &par, &mask);
	
	if (par.table.table_columns) {
		
		pgTableOffsets(pg, position, &table_range);
		
		pgFillBlock(&mask, sizeof(par_info), 0);
		mask.tabs[column_num].leader = -1;
		par.tabs[column_num].leader = border_info;
		pgSetParInfoEx(pg, &table_range, &par, &mask, TRUE, draw_mode);
	}

	UnuseMemory(pg);
}


PG_PASCAL (void) pgSetColumnShading (pg_ref pg, long position, short column_num,
		long shading, short draw_mode)
{
	paige_rec_ptr			pg_rec;
	par_info				par, mask;
	select_pair				table_range;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	table_range.begin = table_range.end = pgFixOffset(pg_rec, position);
	pgGetParInfo(pg, &table_range, FALSE, &par, &mask);
	
	if (par.table.table_columns) {
		
		pgTableOffsets(pg, position, &table_range);
		
		pgFillBlock(&mask, sizeof(par_info), 0);
		mask.tabs[column_num].ref_con = -1;
		par.tabs[column_num].ref_con = shading;
		pgSetParInfoEx(pg, &table_range, &par, &mask, TRUE, draw_mode);
	}

	UnuseMemory(pg);
}


PG_PASCAL (void) pgSetColumnAlignment (pg_ref pg, long position, short column_num,
		short alignment, short draw_mode)
{
	paige_rec_ptr			pg_rec;
	par_info				par, mask;
	select_pair				table_range;
	long					align_value = 0;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	table_range.begin = table_range.end = pgFixOffset(pg_rec, position);
	pgGetParInfo(pg, &table_range, FALSE, &par, &mask);
	
	if (par.table.table_columns) {
		
		switch (alignment) {
			
			case justify_left:
				align_value = left_tab;
				break;
				
			case justify_right:
				align_value = right_tab;
				break;
				
			case justify_center:
				align_value = center_tab;
				break;
		}

		pgTableOffsets(pg, position, &table_range);
		
		pgFillBlock(&mask, sizeof(par_info), 0);
		mask.tabs[column_num].tab_type = -1;
		par.tabs[column_num].tab_type = align_value;
		pgSetParInfoEx(pg, &table_range, &par, &mask, TRUE, draw_mode);
	}

	UnuseMemory(pg);
}


/* pgSetColumnWidth sets the individual column width. */

PG_PASCAL (void) pgSetColumnWidth (pg_ref pg, long position, short column_num,
		short width, short draw_mode)
{
	paige_rec_ptr			pg_rec;
	par_info				par, mask;
	select_pair				table_range;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	table_range.begin = table_range.end = pgFixOffset(pg_rec, position);
	pgGetParInfo(pg, &table_range, FALSE, &par, &mask);
	
	if (par.table.table_columns) {
		
		pgTableOffsets(pg, position, &table_range);
		
		pgFillBlock(&mask, sizeof(par_info), 0);
		mask.tabs[column_num].position = -1;
		par.tabs[column_num].position &= 0xFFFF0000;
		par.tabs[column_num].position |= width;
		pgSetParInfoEx(pg, &table_range, &par, &mask, TRUE, draw_mode);
	}

	UnuseMemory(pg);
}



/* pgTableOffsets returns the beginning and ending positions for the beginning and ending of
the whole table. */

PG_PASCAL (void) pgTableOffsets (pg_ref pg, long position, select_pair_ptr offsets)
{
	paige_rec_ptr		pg_rec;
	style_run_ptr		par_run;
	par_info_ptr		par_base;
	pg_short_t			style_item;
	long				table_id;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	
	par_run = pgFindRunFromRef(pg_rec->par_style_run, pgFixOffset(pg_rec, position), NULL);
	par_base = (par_info_ptr) UseMemory(pg_rec->par_formats);
	offsets->begin = offsets->end = 0;
	
	style_item = par_run->style_item;
	
	if ((table_id = par_base[style_item].table.unique_id) != 0) {

		for (;;) {
			
			style_item = par_run->style_item;
			
			if (par_base[style_item].table.unique_id != table_id) {
				
				++par_run;
				offsets->begin = par_run->offset;
				break;
			}
			
			if (par_run->offset == 0)
				break;

			--par_run;
		}

		for (;;) {

			style_item = par_run->style_item;
			
			if (par_base[style_item].table.unique_id != table_id)
				break;
			
			if (par_run->offset >= pg_rec->t_length)
				break;
			
			++par_run;
		}
		
		if ((offsets->end = par_run->offset) > pg_rec->t_length)
			offsets->end = pg_rec->t_length;
	}

	UnuseMemory(pg_rec->par_formats);
	UnuseMemory(pg_rec->par_style_run);
	UnuseMemory(pg);
}


/* pgPositionToColumn returns the column number containing position. If not part of a table
the result is -1. */

PG_PASCAL (short) pgPositionToColumn (pg_ref pg, long position)
{
	paige_rec_ptr			pg_rec;
	short					column = -1;
	
	pg_rec = (paige_rec_ptr) UseMemory(pg);
	
	if (get_table_format(pg_rec, position)) {
		text_block_ptr			block;
		pg_char_ptr				text;
		long					global_offset, local_offset, cell_offset;
		
		UnuseMemory(pg_rec->par_formats);
		
		global_offset = pgFixOffset(pg_rec, position);
		block = pgFindTextBlock(pg_rec, global_offset, NULL, FALSE, TRUE);
		local_offset = cell_offset = global_offset - block->begin;
		text = (pg_char_ptr) UseMemoryRecord(block->text, local_offset, 0, TRUE);
		
		while (local_offset) {
			
			--text;
			--local_offset;
			
			if (*text == 0x0D) {
				
				++text;
				++local_offset;
				break;
			}
		}
		
		column = 0;

		while (local_offset < cell_offset) {
			
			++local_offset;
			
			if (*text++ == 9)
				column += 1;
		}

		UnuseMemory(block->text);
		UnuseMemory(pg_rec->t_blocks);
	}

	UnuseMemory(pg);

	return		column;
}


/* pgPositionToRow returns the row number containing position. If not part of a table
-1 is returned.  If offsets is non-NULL it is set to the positions for the whole row. */

PG_PASCAL (long) pgPositionToRow (pg_ref pg, long position, select_pair_ptr offsets)
{
	paige_rec_ptr			pg_rec;
	long					row = -1;
	
	pg_rec = (paige_rec_ptr) UseMemory(pg);
	
	if (get_table_format(pg_rec, position)) {
		text_block_ptr			block;
		pg_char_ptr				text;
		select_pair				table_range;
		long					global_offset, wanted_offset, local_offset, end_offset;
		
		UnuseMemory(pg_rec->par_formats);
		wanted_offset = pgFixOffset(pg_rec, position);
		pgTableOffsets(pg, wanted_offset, &table_range);
		global_offset = table_range.begin;
		block = pgFindTextBlock(pg_rec, global_offset, NULL, FALSE, TRUE);
		local_offset = global_offset - block->begin;
		end_offset = block->end - block->begin;
		text = (pg_char_ptr) UseMemoryRecord(block->text, local_offset, 0, TRUE);

		row = 0;
		
		while (global_offset < table_range.end) {
			
			if (offsets)
				offsets->begin = global_offset;

			for (;;) {
				
				++global_offset;
				++local_offset;
				
				if (*text++ == 0x0D)
					break;
			}
			
			if (offsets)
				offsets->end = global_offset;
			
			if (global_offset > wanted_offset)
				break;
			
			++row;
			
			if (local_offset >= end_offset && global_offset < table_range.end) {
				
				UnuseMemory(block->text);
				++block;
				pg_rec->procs.load_proc(pg_rec, block);
				text = (pg_char_ptr) UseMemory(block->text);
				end_offset = block->end - block->begin;
				local_offset = 0;
			}
		}

		UnuseMemory(block->text);
		UnuseMemory(pg_rec->t_blocks);
	}

	UnuseMemory(pg);

	return		row;
}

/* pgGetColumnInfo returns the column information for column_num. If not a table then
FALSE is returned. */

PG_PASCAL (pg_boolean) pgGetColumnInfo (pg_ref pg, long position, short column_num, tab_stop_ptr info)
{
	paige_rec_ptr			pg_rec;
	par_info_ptr			par;
	pg_boolean				is_table = FALSE;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	
	if ((par = get_table_format(pg_rec, position)) != NULL) {
		
		is_table = TRUE;
		
		if (par->num_tabs) {
		
			*info = par->tabs[column_num];
			
			if (!(info->tab_type & TAB_TYPE_MASK))
				info->tab_type = par->justification + 1;
			if (!(info->position & TAB_POS_MASK))
				info->position = par->column_var;
			if (!info->leader)
				info->leader = par->table.cell_borders;
			if (!info->ref_con)
				info->ref_con = par->table.border_shading;
		}
		else {
			
			info->position = par->column_var;
			info->leader = par->table.cell_borders;
			info->ref_con = par->table.border_shading;
			info->tab_type = par->justification;
		}
		
		UnuseMemory(pg_rec->par_formats);
	}
	
	UnuseMemory(pg);

	return	is_table;
}

/* pgNumColumns returns the number of columns in the table containing position.  If not a table
zero is returned. */

PG_PASCAL (short) pgNumColumns (pg_ref pg, long position)
{
	paige_rec_ptr		pg_rec;
	par_info_ptr		par;
	short				result = 0;
	
	pg_rec = (paige_rec_ptr) UseMemory(pg);
	
	if ((par = get_table_format(pg_rec, position)) != NULL) {
		
		result = (short)par->table.table_columns;
		UnuseMemory(pg_rec->par_formats);
	}
	
	UnuseMemory(pg);
	
	return		result;
}

/* pgNumColumns returns the number of rows in the table containing position.  If not a table
zero is returned. */

PG_PASCAL (long) pgNumRows (pg_ref pg, long position)
{
	paige_rec_ptr		pg_rec;
	par_info_ptr		par;
	long				result = 0;
	
	pg_rec = (paige_rec_ptr) UseMemory(pg);
	
	if ((par = get_table_format(pg_rec, position)) != NULL) {
		select_pair			table_range;
		text_block_ptr		block;
		pg_char_ptr			text;
		long				local_offset, end_offset;
		
		UnuseMemory(pg_rec->par_formats);
		pgTableOffsets(pg, position, &table_range);
		block = pgFindTextBlock(pg_rec, table_range.begin, NULL, FALSE, TRUE);
		local_offset = table_range.begin - block->begin;
		text = (pg_char_ptr) UseMemoryRecord(block->text, local_offset, 0, TRUE);
		end_offset = block->end - block->begin;
		
		while (table_range.begin < table_range.end) {
			
			for (;;) {
				
				++table_range.begin;
				++local_offset;
				
				if (*text++ == 0x0D)
					break;
			}
			
			++result;
			
			if (local_offset >= end_offset && table_range.begin < table_range.end) {
				
				UnuseMemory(block->text);
				++block;
				pg_rec->procs.load_proc(pg_rec, block);
				text = (pg_char_ptr) UseMemory(block->text);
				local_offset = 0;
				end_offset = block->end - block->begin;
			}
		}
		
		UnuseMemory(block->text);
		UnuseMemory(pg_rec->t_blocks);
	}
	
	UnuseMemory(pg);
	
	return		result;
}

/* pgInsertColumn inserts a new column(s) before or after column_num. The column attributes
are established by info. */

PG_PASCAL (void) pgInsertColumn (pg_ref pg, long position, short column_num,
		tab_stop_ptr info, short draw_mode)
{
	paige_rec_ptr			pg_rec;
	select_pair				table_range, insert_range;
	par_info_ptr			par;
	par_info				new_par, mask;
	pg_char					tab_char = (pg_char)9;
	short					column_insert;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	
	if ((par = get_table_format(pg_rec, position)) != NULL) {

		new_par = *par;
		UnuseMemory(pg_rec->par_formats);
		
		if (new_par.table.table_columns == TAB_ARRAY_SIZE) {
			
			UnuseMemory(pg);
			return;
		}

		pgTableOffsets(pg, position, &table_range);
		insert_range = table_range;
		column_insert = column_num;

		while (insert_range.begin < table_range.end) {
			long			row_end;

			column_insert_position(pg_rec, column_insert, &insert_range, &row_end);
			pgInsertRawData(pg_rec, &tab_char, 1, insert_range.begin, NULL, data_insert_mode);
			insert_range.begin = row_end + 1;
			++table_range.end;
		}

		pgFillBlock(&mask, sizeof(par_info), 0);
		mask.table.table_columns = -1;

		new_par.table.table_columns += 1;
		
		if (new_par.num_tabs) {
			short		target_element, source_element;
			long		move_size;
			
			pgFillBlock(mask.tabs, sizeof(tab_stop) * TAB_ARRAY_SIZE, -1);
			mask.num_tabs = 0xFFFF;
			target_element = source_element = (short)new_par.num_tabs;
			++target_element;
			move_size = (long)(new_par.num_tabs - column_insert);
			
			if (move_size) {
				
				move_size *= sizeof(tab_stop);
				pgBlockMove(&new_par.tabs[source_element], &new_par.tabs[target_element], -move_size);
			}
			
			new_par.tabs[column_insert] = *info;
			new_par.num_tabs += 1;
		}
		
		pgSetParInfoEx(pg, &table_range, &new_par, &mask, TRUE, draw_mode);
	}
	
	UnuseMemory(pg);
}


/* pgInsertRow inserts a new row before the row at position. Or, if position is the absolute
end of the table a new row is appended to the end. */

PG_PASCAL (void) pgInsertRow (pg_ref pg, long position, long row_num, short draw_mode)
{
	paige_rec_ptr			pg_rec;
	memory_ref				row_ref;
	pg_char_ptr				row_data;
	par_info_ptr			par;
	long					row_size;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	
	if ((par = get_table_format(pg_rec, position)) != NULL) {
		select_pair		row_range;

		++pg_rec->change_ctr;
		
		row_range.begin = pgFixOffset(pg_rec, position);
		row_insert_position(pg_rec, row_num, &row_range);

		row_size = par->table.table_columns;
		UnuseMemory(pg_rec->par_formats);
		row_ref = MemoryAlloc(pg_rec->globals->mem_globals, sizeof(pg_char), row_size, 0);
		row_data = (pg_char_ptr) UseMemory(row_ref);
		pgFillBlock(row_data, row_size, 9);
		
		if (row_range.begin < row_range.end)
			row_data[row_size - 1] = 0x0D;
		else {
			
			--row_range.begin;
			*row_data = 0x0D;
		}

		pgInsert(pg, row_data, row_size, row_range.begin, data_insert_mode, 0, draw_mode);
		
		UnuseAndDispose(row_ref);
	}

	UnuseMemory(pg);
}


/* pgDeleteColumn removes a column. */

PG_PASCAL (void) pgDeleteColumn (pg_ref pg, long position, short column_num, short draw_mode)
{
	paige_rec_ptr			pg_rec;
	par_info_ptr			par;
	par_info				new_par, mask;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	
	if ((par = get_table_format(pg_rec, position)) != NULL) {
		select_pair				table_range, delete_range;
		long					next_row;

		new_par = *par;
		UnuseMemory(pg_rec->par_formats);
		
		if (new_par.table.table_columns > 1) {
		
			pgTableOffsets(pg, position, &table_range);
			clear_selections(pg_rec, table_range.begin, draw_mode);
			delete_range = table_range;

			while (delete_range.begin < table_range.end) {
				long			delete_size;

				column_insert_position(pg_rec, column_num, &delete_range, &next_row);
				delete_size = (delete_range.end - delete_range.begin) + 1;
				
				if (delete_range.end == (next_row - 1))
					--delete_range.begin;
				
				delete_range.end = delete_range.begin + delete_size;
				pgDeleteRawData(pg_rec, delete_range.begin, &delete_range.end, NULL);
				
				delete_range.begin = next_row - delete_size;
				table_range.end -= delete_size;
			}
 			
 			clear_selections(pg_rec, delete_range.end, draw_none);
 
			pgFillBlock(&mask, sizeof(par_info), 0);
			mask.table.table_columns = -1;
			mask.num_tabs = (unsigned short)-1;
			new_par.table.table_columns -= 1;
			
			if (new_par.num_tabs) {

				if (column_num < (short)(new_par.num_tabs < 2)) {
					long		insert_size;
					
					insert_size = (long) (((short)new_par.num_tabs - column_num - 1) * sizeof(tab_stop));
					
					if (insert_size > 0)
						pgBlockMove(&new_par.tabs[column_num + 1], &new_par.tabs[column_num], insert_size);
				}

				new_par.num_tabs -= 1;
				pgFillBlock(&new_par.tabs[new_par.num_tabs], sizeof(tab_stop), 0);

				pgFillBlock(mask.tabs, sizeof(tab_stop) * TAB_ARRAY_SIZE, -1);
			}

			pgSetParInfoEx(pg, &table_range, &new_par, &mask, TRUE, draw_mode);
		}
	}

	UnuseMemory(pg);
}

/* pgDeleteRow deletes a row. */

PG_PASCAL (void) pgDeleteRow (pg_ref pg, long position, long row_num, short draw_mode)
{
	paige_rec_ptr			pg_rec;
	par_info_ptr			par;
	short					use_draw_mode;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	
	if ((par = get_table_format(pg_rec, position)) != NULL) {
		select_pair				table_range, delete_range;

		UnuseMemory(pg_rec->par_formats);
		
		if (pgNumRows(pg, position) > 1) {

			++pg_rec->change_ctr;
			
			if ((use_draw_mode = draw_mode) == best_way)
				use_draw_mode = bits_copy;

			pgTableOffsets(pg, position, &table_range);
			delete_range.begin = pgFixOffset(pg_rec, position);
			row_insert_position(pg_rec, row_num, &delete_range);
			clear_selections(pg_rec, delete_range.end, draw_mode);
			pgDeleteRawData(pg_rec, delete_range.begin, &delete_range.end, NULL);

			if (draw_mode)
				pgUpdateText(pg_rec, NULL, table_range.begin, pg_rec->t_length, MEM_NULL, NULL, use_draw_mode, TRUE);
		}
	}

	UnuseMemory(pg);
}


/* pgConvertTableText converts a table to regular text. */

PG_PASCAL (void) pgConvertTableText (pg_ref pg, select_pair_ptr selection,
		pg_boolean text_to_table, short draw_mode)
{
	paige_rec_ptr				pg_rec;
	memory_ref					select_list;
	register pg_short_t			num_selects;
	register select_pair_ptr	select_run;
	change_info					stuff_to_change;
	
	pg_rec = (paige_rec_ptr) UseMemory(pg);

#ifdef PG_DEBUG
	if (GetAccessCtr(pg_rec->par_style_run))
		pgFailure(pg_rec->globals->mem_globals, ILLEGAL_RE_ENTER_ERROR, 1);
	if (pg_rec->lock_id)
		pgFailure(pg_rec->globals->mem_globals, LOCKED_PG_ERROR, 1);
#endif

    if (draw_mode)
    	pgSetCursorState(pg_rec->myself, hide_cursor);

	select_list = pgSetupOffsetRun(pg_rec, selection, TRUE, FALSE);

	pgFillBlock(&stuff_to_change, sizeof(change_info), 0);
	stuff_to_change.pg = pg_rec;
	stuff_to_change.par_change = &pg_rec->globals->def_par;
	num_selects = (pg_short_t)GetMemorySize(select_list);

	if (text_to_table) {
		pg_short_t		index;
		
		stuff_to_change.class_OR = pgUniqueID(pg);
		select_run = (select_pair_ptr) UseMemory(select_list);
		
		for (index = 0; index < num_selects; ++index, ++select_run) {
			select_pair		range;
			long			text_line;
			pg_short_t		hard_tab_count;
			par_info		info, mask;
			
			pgFillBlock(&mask, sizeof(par_info), 0);
			mask.num_tabs = 0xFFFF;
			text_line = select_run->begin;
			
			while (text_line < select_run->end) {
				
				range = *select_run;
				range.begin = text_line;
				hard_tab_count = count_tabs(pg_rec, &range) + 1;
				pgGetParInfo(pg, &range, FALSE, &info, &mask);
				
				if (hard_tab_count > info.num_tabs)
					info.num_tabs = hard_tab_count;

				text_line = range.end;
				pgSetParInfoEx(pg, &range, &info, &mask, FALSE, draw_none);
			}
		}

		UnuseMemory(select_list);
	}
	
	select_run = (select_pair_ptr) UseMemory(select_list);
	
	stuff_to_change.change_range = *select_run;
	
	while (num_selects) {
		
		if (text_to_table)
			pgChangeStyleRun(pg_rec->par_style_run, &stuff_to_change, change_to_table,
					select_run->begin, select_run->end);
		else
			pgChangeStyleRun(pg_rec->par_style_run, &stuff_to_change, change_to_text,
					select_run->begin, select_run->end);

		if (select_run->begin < stuff_to_change.change_range.begin)
			stuff_to_change.change_range.begin = select_run->begin;
		if (select_run->end > stuff_to_change.change_range.end)
			stuff_to_change.change_range.end = select_run->end;
		
		if (text_to_table)
			pgMeasureMaxColumns(pg_rec, select_run);

		++select_run;
		--num_selects;
	}

	pgFinishStyleUpdate(pg_rec, &stuff_to_change, select_list, TRUE, draw_mode);
	
	UnuseMemory(pg);
}

/* pgBorderRowBounds returns the total bounds of all starts. This is used for potentially
non-regular line records within table cells. */

PG_PASCAL (pg_short_t) pgTableRowBounds (point_start_ptr line_starts, rectangle_ptr bounds)
{
	register point_start_ptr	 starts;
	pg_short_t					 num_starts;

	starts = line_starts;
	*bounds = starts->bounds;
	num_starts = 0;

	while (starts->flags != TERMINATOR_BITS) {
		
		pgTrueUnionRect(&starts->bounds, bounds, bounds);
		++num_starts;

		if (starts->flags & PAR_BREAK_BIT)
			break;

		++starts;
	}

#ifdef PG_DEBUG
	if (starts->flags == TERMINATOR_BITS)
		pgDebugProc(STRUCT_INTEGRITY_ERR, MEM_NULL);
#endif

	return	num_starts;
}


/* pgPtToTableOffset is the lower-level function that actually finds the cell containing the point.
If offsets is non-null then the cell text offsets are returned. If column_sides is non-null
then the column rect is returned (unscrolled).  The function result is zero if not in a table
otherwise it is the column number (1-based).  */

PG_PASCAL (short) pgPtToTableOffset (paige_rec_ptr pg_rec, co_ordinate_ptr the_point,
			select_pair_ptr offsets, rectangle_ptr column_sides, t_select_ptr actual_selection)
{
	par_info_ptr			par;
	t_select				selection;
	short					result = 0;

	pgFillBlock(&selection, sizeof(t_select), 0);
	pg_rec->procs.offset_proc(pg_rec, the_point, FALSE, &selection);
	
	if ((par = get_table_format(pg_rec, selection.offset)) != NULL) {
		text_block_ptr			block;
		point_start_ptr			starts;
		rectangle				page_bounds;
		long					cell_index;
		pg_short_t				r_num;

		if (actual_selection)
			*actual_selection = selection;

		block = pgFindTextBlock(pg_rec, selection.offset, NULL, TRUE, TRUE);
		starts = (point_start_ptr) UseMemoryRecord(block->lines, (long)selection.line, 0, TRUE);
		result = (short)(starts->cell_num & CELL_NUM_MASK);

		if (offsets || column_sides) {
			long			max_table_width;
			
			r_num = pgGetWrapRect(pg_rec, starts->r_num, NULL);
			GetMemoryRecord(pg_rec->wrap_area, (long)(r_num + 1), &page_bounds);
			cell_index = (long)starts->cell_num & CELL_NUM_MASK;
			cell_index -= 1;
			max_table_width = pgSectColumnBounds(pg_rec, par, cell_index, starts->r_num, &page_bounds);
			pgOffsetRect(&page_bounds, pgTableJustifyOffset(par, max_table_width), 0);

			if (column_sides)
				*column_sides = page_bounds;

			if (offsets) {

				while (!(starts->cell_num & CELL_TOP_BIT) && starts->offset)
					--starts;
				while (!(starts->flags & NEW_LINE_BIT) && starts->offset)
					--starts;

				offsets->begin = (long)starts->offset;
				offsets->begin += block->begin;
				
				while (!(starts->cell_num & CELL_TOP_BIT) && starts[1].flags != TERMINATOR_BITS)
					++starts;
				while (!(starts->flags & LINE_BREAK_BIT) && starts[1].flags != TERMINATOR_BITS)
					++starts;
				
				offsets->end = (long)(starts[1].offset - 1);
				offsets->end += block->begin;
			}
			
			if (column_sides) {
				select_pair		table_offsets;
				pg_short_t		local_offset;

				UnuseMemory(block->lines);
				UnuseMemory(pg_rec->t_blocks);
			
				pgTableOffsets(pg_rec->myself, selection.offset, &table_offsets);
				block = pgFindTextBlock(pg_rec, table_offsets.begin, NULL, TRUE, TRUE);
				starts = (point_start_ptr) UseMemory(block->lines);
				local_offset = (pg_short_t)(table_offsets.begin - block->begin);
				
				while (starts[1].offset <= local_offset)
					++starts;
				
				column_sides->top_left.v = starts->bounds.top_left.v;
				UnuseMemory(block->lines);
				UnuseMemory(pg_rec->t_blocks);

				block = pgFindTextBlock(pg_rec, table_offsets.end, NULL, TRUE, TRUE);
				starts = (point_start_ptr) UseMemory(block->lines);
				local_offset = (pg_short_t)(table_offsets.end - block->begin);
				
				if (local_offset > 0)
					local_offset -= 1;

				while (starts[1].offset <= local_offset)
					++starts;

				column_sides->bot_right.v = starts->bounds.bot_right.v + (long)starts->cell_height;
			}
		}

		UnuseMemory(block->lines);
		UnuseMemory(pg_rec->t_blocks);
		UnuseMemory(pg_rec->par_formats);
	}

	return	result;
}


/* pgTabToCell advances from the current cell to the next.  It is assumed that the current
insertion is in a table,  also that the caret and highlighting are not showing. */

PG_PASCAL (void) pgTabToCell (paige_rec_ptr pg_rec)
{
	text_block_ptr			block;
	pg_char_ptr				text;
	pg_char					next_char;
	long					current_insertion, local_offset, local_end;
	
	current_insertion = pgCurrentInsertion(pg_rec);
	
	if (current_insertion == pg_rec->t_length)
		return;

	block = pgFindTextBlock(pg_rec, current_insertion, NULL, FALSE, TRUE);
	local_offset = current_insertion - block->begin;
	local_end = block->end - block->begin;

	text = (pg_char_ptr) UseMemoryRecord(block->text, local_offset, 0, TRUE);
	
	for (;;) {
		
		if (local_offset == local_end) {
			
			UnuseMemory(block->text);

			++block;
			pg_rec->procs.load_proc(pg_rec, block);
			text = (pg_char_ptr) UseMemory(block->text);
			local_offset = 0;
			local_end = block->end - block->begin;
		}
		
		++local_offset;
		next_char = *text++;
		
		if (next_char == 0x09 || next_char == 0x0D)
			break;
	}
	
	current_insertion = local_offset + block->begin;

	UnuseMemory(block->text);
	UnuseMemory(pg_rec->t_blocks);
	
	pgSetSelection(pg_rec->myself, current_insertion, current_insertion, 0, FALSE);
}

/* pgPositionInTable returns TRUE if the position is within a table. If selection is non-null
it is filled with the range of text for the corresponding cell (excluding tab, cr) */

PG_PASCAL (pg_boolean) pgPositionInTable (paige_rec_ptr pg, long position, select_pair_ptr selection)
{
	text_block_ptr		block;
	pg_char_ptr			text;
	long				local_offset, offset;
	pg_boolean			result = FALSE;

	if (get_table_format(pg, position)) {
		
		result = TRUE;
		
		if (selection) {
			
			selection->begin = selection->end = position;

			block = pgFindTextBlock(pg, position, NULL, FALSE, TRUE);
			local_offset = offset = (position - block->begin);
			text = (pg_char_ptr) UseMemory(block->text);
			
			while (local_offset) {
				
				--local_offset;
				
				if (text[local_offset] == 0x09 || text[local_offset] == 0x0D) {
					
					++local_offset;
					break;
				}
			}
			
			selection->begin = local_offset + block->begin;
			
			local_offset = offset;
			offset = block->end - block->begin;
			
			while (local_offset < offset) {
				
				if (text[local_offset] == 0x09 || text[local_offset] == 0x0D)
					break;
				
				++local_offset;
			}
			
			selection->end = local_offset + block->begin;

			UnuseMemory(block->text);
			UnuseMemory(pg->t_blocks);
		}

		UnuseMemory(pg->par_formats);
	}
	
	return	result;
}


/* pgCellSelections returns selection pair(s) based on selection. If no part of the selection
overlaps a table,  pgSetupOffsetRun() is called, otherwise text-only (without tabs or CRS)
is returned. EXCEPTION:  If the whole table is enclosed by the selection it is considered
part of all the text (tabs, cr included). */

PG_PASCAL (memory_ref) pgCellSelections (paige_rec_ptr pg_rec, select_pair_ptr selection)
{
	memory_ref			result = MEM_NULL;
	select_pair			wanted_select, begin_cell_select, end_cell_select, table_select;
	select_pair_ptr		cell_select_ptr;
	pg_boolean			begin_in_table, end_in_table;
	pg_boolean			whole_table_selected = FALSE;

	if (selection)
		wanted_select = *selection;
	else
		pgGetSelection(pg_rec->myself, &wanted_select.begin, &wanted_select.end);
	
	begin_in_table = pgPositionInTable(pg_rec, wanted_select.begin, &begin_cell_select);
	end_in_table = pgPositionInTable(pg_rec, wanted_select.end, &end_cell_select);
	
	if (begin_in_table && !end_in_table) {
		
		pgTableOffsets(pg_rec->myself, wanted_select.begin, &table_select);
		whole_table_selected = (pg_boolean)(table_select.begin == wanted_select.begin);
	}
	
	if (!whole_table_selected && (begin_in_table || end_in_table)) {

		if (begin_in_table)
			pgTableOffsets(pg_rec->myself, wanted_select.begin, &table_select);
		else
			pgTableOffsets(pg_rec->myself, wanted_select.end, &table_select);

		if (begin_in_table)
			table_select.begin = begin_cell_select.begin;
		if (end_in_table)
			table_select.end = end_cell_select.end;

		result = MemoryAlloc(pg_rec->globals->mem_globals, sizeof(select_pair), 0, 8);
		
		if (wanted_select.begin < (table_select.begin - 1)) {
			
			cell_select_ptr = (select_pair_ptr) AppendMemory(result, 1, FALSE);
			cell_select_ptr->begin = wanted_select.begin;
			cell_select_ptr->end = table_select.begin - 1;
			UnuseMemory(result);
			
			wanted_select.begin = table_select.begin;
		}

		while (wanted_select.begin < wanted_select.end) {

			if (!pgPositionInTable(pg_rec, wanted_select.begin, &begin_cell_select))
				break;
			
			if (begin_cell_select.end > wanted_select.end)
				begin_cell_select.end = wanted_select.end;
			if (begin_cell_select.begin < wanted_select.begin)
				begin_cell_select.begin = wanted_select.begin;

			if (begin_cell_select.begin < begin_cell_select.end) {
			
				cell_select_ptr = (select_pair_ptr) AppendMemory(result, 1, FALSE);
				*cell_select_ptr = begin_cell_select;
				UnuseMemory(result);
			}

			wanted_select.begin = begin_cell_select.end + 1;
		}
		
		if (wanted_select.begin < wanted_select.end) {

			cell_select_ptr = (select_pair_ptr) AppendMemory(result, 1, FALSE);
			*cell_select_ptr = wanted_select;
			UnuseMemory(result);
		}
		
		if (GetMemorySize(result) == 0) {
			
			DisposeMemory(result);
			result = MEM_NULL;
		}
	}
	else
	
		result = pgSetupOffsetRun(pg_rec, selection, FALSE, FALSE);

	return	result;
}

/* pgFixCellBackspace handles the situation where a tab or CR can be deleted. This gets called
only if we are deleting a single char. */

PG_PASCAL (void) pgFixCellBackspace (paige_rec_ptr pg, size_t PG_FAR *delete_from, size_t PG_FAR *delete_to)
{
	select_pair			cell_bounds;

	if (pgPositionInTable(pg, *delete_from, &cell_bounds)) {
		
		if (*delete_to > cell_bounds.end) {
			
			*delete_to = cell_bounds.end;
			
			if (*delete_from > cell_bounds.begin)
				*delete_from -= 1;
		}
	}
}


/* pgPasteToCells inserts text into pg_rec if the target position is a table. If this occurs
then TRUE is returned, otherwise nothing occurs and FALSE is returned. */

PG_PASCAL (pg_boolean) pgPasteToCells (paige_rec_ptr pg_rec, size_t position,
		pg_char_ptr text, size_t length)
{
	pg_boolean			result = FALSE;
	pg_char				next_char;
	long				next_position, text_index, end_text_index, text_size;
	
	next_position = position;
	text_index = 0;

	while (get_table_format(pg_rec, next_position)) {
		
		result = TRUE;

		UnuseMemory(pg_rec->par_formats);
		
		text_size = 0;
		end_text_index = text_index;

		while (end_text_index < length) {
			
			next_char = text[end_text_index];
			++end_text_index;
			
			if (next_char == 0x09 || next_char == 0x0D)
				break;
			
			++text_size;
		}
		
		if (text_size)
			pgInsert(pg_rec->myself, &text[text_index], text_size, next_position, data_insert_mode, 0, draw_none);
		
		text_index = end_text_index;
		
		if (text_index >= length)
			break;
		
		next_position = tab_to_cell(pg_rec, next_position, (pg_boolean)(next_char == 0x0D));
	}
	
	return	result;
}

/* pgMeasureMaxColumns is a fairly extensive function. This is used mostly with HTML.
It determines the maximum width in all columns of the table at offsets. If there is
already a pre-determined width in the cell then that column is left alone. */
 
PG_PASCAL (void) pgMeasureMaxColumns (paige_rec_ptr pg, select_pair_ptr offsets)
{
	par_info_ptr		par;
	style_walk			walker;
	pg_short_t			num_tabs;
	long				column_mask[TAB_ARRAY_SIZE], width_array[TAB_ARRAY_SIZE];
	
	pgPrepareStyleWalk(pg, offsets->begin, &walker, TRUE);
	par = walker.cur_par_style;
	num_tabs = par->num_tabs;
	pgFillBlock(width_array, TAB_ARRAY_SIZE * sizeof(long), 0);
	pgFillBlock(column_mask, TAB_ARRAY_SIZE * sizeof(long), 0);

	if (par->table.table_columns) {
		text_block_ptr				block;
		rectangle					pensizes, page_bounds;
		register pg_char_ptr		text;
		memory_ref					locs_ref;
		long PG_FAR					*locs;
		long						global_offset, width_base, local_offset;
		long						width, max_width, max_column_width;
		pg_short_t					column_ctr, output_ctr;
		pg_boolean					needs_measure;
		pg_char						the_char;

		GetMemoryRecord(pg->wrap_area, 1, &page_bounds);
		max_width = page_bounds.bot_right.h - page_bounds.top_left.h;
		
		if (pg->doc_info.attributes & USE_MARGINS_BIT)
			max_width -= (pg->doc_info.margins.top_left.h + pg->doc_info.margins.bot_right.h);
		
		max_column_width = (max_width / par->table.table_columns) + 8;

		needs_measure = FALSE;
		
		for (;;) {

			for (column_ctr = 0; column_ctr < num_tabs; ++column_ctr) {
				
				if (par->tabs[column_ctr].position > width_array[column_ctr])
					width_array[column_ctr] = par->tabs[column_ctr].position;
			}

			if (walker.next_par_run->offset >= offsets->end)
				break;
			
			pgSetWalkStyle(&walker, walker.next_par_run->offset);
			par = walker.cur_par_style;
		}
		
		for (column_ctr = 0; column_ctr < num_tabs; ++column_ctr) {
			
			if (width_array[column_ctr] == 0) {
				
				width_array[column_ctr] = 16;
				column_mask[column_ctr] = TRUE;
				needs_measure = TRUE;
			}
		}

		if (needs_measure) {
			
			pgSetWalkStyle(&walker, offsets->begin);
			par = walker.cur_par_style;
			block = pgFindTextBlock(pg, offsets->begin, NULL, FALSE, TRUE);
			locs = pgGetCharLocs(pg, block, &locs_ref, NULL);
			text = (pg_char_ptr) UseMemory(block->text);
			global_offset = offsets->begin;
			local_offset = global_offset - block->begin;
			text += local_offset;
			column_ctr = 0;
			
			while (global_offset < offsets->end) {
				
				pgSetWalkStyle(&walker, global_offset);
				par = walker.cur_par_style;

				width_base = locs[local_offset];
				
				while (global_offset < offsets->end) {
					
					the_char = *text++;
					++global_offset;
					++local_offset;
					
					if (the_char < ' ')
						break;
				}
				
				if (column_mask[column_ctr]) {
					long			column_span;
					
					width = locs[local_offset - 1] - width_base;
					width += (par->table.cell_h_extra * 2);
					pgSetCellSides(par, column_ctr, &pensizes);
					width += (pensizes.top_left.h + pensizes.bot_right.h + 2);
					
					if (width > max_column_width)
						width = max_column_width;

					if ((column_span = par->tabs[column_ctr].tab_type >> 24) > 1)
						width /= (column_span);

					if (width > width_array[column_ctr])
						width_array[column_ctr] = width;
				}
				
				if (the_char == 0x09)
					++column_ctr;
				else
				if (the_char == 0x0D)
					column_ctr = 0;

				if (global_offset < offsets->end)
					if (global_offset >= block->end) {
					
					UnuseMemory(block->text);
					UnuseMemory(locs_ref);
					pgReleaseCharLocs(pg, locs_ref);
					
					++block;
					locs = pgGetCharLocs(pg, block, &locs_ref, NULL);
					text = (pg_char_ptr) UseMemory(block->text);
					local_offset = 0;
				}
			}
			
			UnuseMemory(block->text);
			UnuseMemory(locs_ref);
			pgReleaseCharLocs(pg, locs_ref);
			UnuseMemory(pg->t_blocks);
		}
		
		pgSetWalkStyle(&walker, offsets->begin);
		
		for (;;) {
			long		real_tab_index, span_index;

			par = walker.cur_par_style;
			num_tabs = par->num_tabs;

			for (column_ctr = 0, output_ctr = 0; column_ctr < num_tabs; ++column_ctr) {
				
				if ((span_index = (par->tabs[column_ctr].tab_type) >> 24) == 0)
					span_index = 1;
				
				par->tabs[column_ctr].position = 0;

				for (real_tab_index = 0; real_tab_index < span_index; ++real_tab_index) {
					
					par->tabs[column_ctr].position += width_array[output_ctr];
					++output_ctr;
				}
				
				num_tabs -= (pg_short_t)(span_index - 1);
			}

			if (walker.next_par_run->offset >= offsets->end)
				break;
			
			pgSetWalkStyle(&walker, walker.next_par_run->offset);
		}
	}
	
	pgPrepareStyleWalk(pg, 0, NULL, TRUE);
}

/* pgGetTableBounds returns the bounding rectangle for the table. This function must never
be called unless you know the position is a table. */

PG_PASCAL (void) pgGetTableBounds (paige_rec_ptr pg_rec, long position, rectangle_ptr bounds)
{
	text_block_ptr			block;
	point_start_ptr			starts;
	pg_short_t				local_offset;

	block = pgFindTextBlock(pg_rec, position, NULL, TRUE, FALSE);
	starts = (point_start_ptr) UseMemory(block->lines);
	local_offset = (pg_short_t) (position - block->begin);
	
	while (starts->offset < local_offset)
		++starts;
	
	while (!(starts->flags & NEW_LINE_BIT))
		--starts;
	
	pgTableRowBounds(starts, bounds);

	UnuseMemory(block->lines);
	UnuseMemory(pg_rec->t_blocks);
}


/* pgSetCellSides sets the pensize sides of sides based cell number cell_num. Sides are
set depending on the four pen sizes based on the border(s) that will be drawn. The value in
"sides" is the amount the rectangle should be inset to cover all the pen + extra width(s)
around the edge. In other words if we drew all borders around the inside edges of cell_num
the resulting non-pen rectangular area would be the cell size exactly. */

PG_PASCAL (void) pgSetCellSides (par_info_ptr par, pg_short_t cell_num, rectangle_ptr sides)
{
	rectangle		pen_info;
	long			border_info, original_bottom, original_right;
	
	if (!(border_info = par->tabs[cell_num].leader))
		border_info = par->table.border_info;
	
	border_info |= (par->table.cell_borders & (~border_info));
	
	sides->top_left.h = pen_info.top_left.h = border_info & PG_BORDER_LEFT;
	sides->top_left.v = pen_info.top_left.v = (border_info & PG_BORDER_TOP) >> 16;
	sides->bot_right.h = pen_info.bot_right.h = (border_info & PG_BORDER_RIGHT) >> 8;
	sides->bot_right.v = pen_info.bot_right.v = (border_info & PG_BORDER_BOTTOM) >> 24;
	
	sides->top_left.h &= PG_BORDER_PENSIZE;
	sides->top_left.v &= PG_BORDER_PENSIZE;
	sides->bot_right.h &= PG_BORDER_PENSIZE;
	sides->bot_right.v &= PG_BORDER_PENSIZE;
	original_bottom = sides->bot_right.v;
	original_right = sides->bot_right.h;

	if (pen_info.top_left.h & PG_BORDER_DOUBLE)
		sides->top_left.h += (sides->top_left.h + 1);
	if (pen_info.top_left.v & PG_BORDER_DOUBLE)
		sides->top_left.v += (sides->top_left.v + 1);
	if (pen_info.bot_right.h & PG_BORDER_DOUBLE)
		sides->bot_right.h += (sides->bot_right.h + 1);
	if (pen_info.bot_right.v & PG_BORDER_DOUBLE)
		sides->bot_right.v += (sides->bot_right.v + 1);

	if (pen_info.bot_right.h & PG_BORDER_SHADOW)
		sides->bot_right.h += original_right;
	if (pen_info.bot_right.v & PG_BORDER_SHADOW)
		sides->bot_right.v += original_bottom;
}


/* pgSetTableRect is called by the pagination code. This is where we set up a series of
dummy "containers" that get fed to the line-calc code. This allows any number of lines
to be paginated within a cell, including weird situations like crossing page or container
boundaries. The rect_verb is one of the following:  0 = initialize to first, 1 = set next
sequential, -1 = end process. */

PG_PASCAL (void) pgSetTableRect (paige_rec_ptr pg_rec, short rect_verb, pg_measure_ptr line_info)
{
	par_info_ptr			par;
	point_start_ptr			starts, end_start;
	long					cell_index, cell_width, use_cell_width;
	long					left_extra, right_extra;

	par = line_info->styles->cur_par_style;
	left_extra = right_extra = 0;
	
	if (rect_verb == 0) {		// Initialize first cell rectangle
		text_block_ptr	block;
		rectangle		starting_bounds;
		long			abs_position;
		
		if ((line_info->is_table = par->table.unique_id) == 0)
			line_info->is_table = -1;
		
		line_info->cell_index = 0;
			line_info->cell_index = 1;

		line_info->table_fit_rect = line_info->fit_rect;
		line_info->table_fit_rect.top_left.h += pgTableJustifyOffset(par, line_info->fit_rect.bot_right.h - line_info->fit_rect.top_left.h);
		line_info->cell_rect = line_info->table_fit_rect;

		line_info->top_table_r = line_info->bot_table_r = line_info->r_num_begin;
		line_info->cell_qty = (pg_short_t)par->table.table_columns;
		
		pgFillBlock(&line_info->table_union, sizeof(rectangle), 0);
			
		starting_bounds = line_info->cell_rect;
		line_info->cell_rect.top_left.h = line_info->table_fit_rect.top_left.h = starting_bounds.top_left.h;
		
	// Compute top and ending row offsets:
		
		block = line_info->block;
		abs_position = (long)line_info->starts->offset;
		abs_position += block->begin;
		pgTableOffsets(pg_rec->myself, abs_position, &line_info->table_top);
		line_info->table_bottom = line_info->table_top;

		block = pgFindTextBlock(pg_rec, line_info->table_top.begin, NULL, FALSE, TRUE);
		pgRowOffsets(block, line_info->table_top.begin - block->begin, NULL, &line_info->table_top.end);
		UnuseMemory(pg_rec->t_blocks);

		block = pgFindTextBlock(pg_rec, line_info->table_bottom.end - 1, NULL, FALSE, TRUE);
		pgRowOffsets(block, line_info->table_bottom.end - block->begin - 1, &line_info->table_bottom.begin, NULL);
		UnuseMemory(pg_rec->t_blocks);
	}
	else
	if (rect_verb == -1)  {		// Done with table
		
		line_info->is_table = FALSE;
		line_info->cell_index = line_info->cell_qty = 0;
		line_info->cell_alignment = 0;
		pgFillBlock(&line_info->cell_extra, sizeof(rectangle), 0);
		line_info->r_num_begin = line_info->bot_table_r;
		line_info->fit_rect.top_left.v = line_info->fit_rect.bot_right.v = line_info->table_union.bot_right.v;
		set_info_rects(pg_rec, line_info);
	}
	else {	// Advance to next rectangle
		
		starts = end_start = line_info->starts;
		starts -= line_info->num_starts;
		--end_start;

		if (starts->r_num > line_info->bot_table_r)
			line_info->bot_table_r = starts->r_num;

		if (end_start->flags & PAR_BREAK_BIT) {
			
			line_info->cell_index = 1;
			line_info->cell_qty = (pg_short_t)par->table.table_columns;

			line_info->top_table_r = line_info->bot_table_r;
			line_info->r_num_begin = line_info->top_table_r;

			line_info->cell_rect = line_info->table_fit_rect;
			line_info->cell_rect.top_left.v = line_info->table_union.bot_right.v;
			line_info->cell_rect.bot_right.v = line_info->cell_rect.top_left.v + 2;
				// Note, "+ 2" is arbitrary, we just dont want an empty rect.
				
			line_info->table_fit_rect = line_info->cell_rect;
			set_info_rects(pg_rec, line_info);
		}
		else
		if (end_start->flags & TAB_BREAK_BIT) {
			
			line_info->r_num_begin = line_info->top_table_r;
			
			line_info->cell_rect.top_left.v = line_info->table_fit_rect.top_left.v;
			line_info->cell_index += 1;
			line_info->cell_rect.top_left.h = line_info->cell_rect.bot_right.h;

			set_info_rects(pg_rec, line_info);
		}
		else {
			
			line_info->cell_rect.top_left.v = line_info->cell_rect.bot_right.v = line_info->fit_rect.top_left.v;
			line_info->cell_rect.bot_right.v += 2;
		}
	}
	
	if (line_info->is_table) {
		rectangle	previous_sides, next_sides, these_sides;
		
		cell_index = (long)(line_info->cell_index - 1);
		pgFillBlock(&line_info->cell_extra, sizeof(rectangle), 0);

		line_info->cell_extra.top_left.h = line_info->cell_extra.bot_right.h = par->table.cell_h_extra;
		line_info->fit_rect = line_info->cell_rect;
		line_info->cell_alignment = justify_left;
		
		switch (par->tabs[cell_index].tab_type & TAB_TYPE_MASK) {
			
			case left_tab:
				line_info->cell_alignment = justify_left;
				break;

			case center_tab:
				line_info->cell_alignment = justify_center;
				break;

			case right_tab:
				line_info->cell_alignment = justify_right;
				break;

			case decimal_tab:
				line_info->cell_alignment = justify_full;
				break;
		}

		cell_width = par->column_var = par->table.table_column_width;
		
		if (cell_width <= 0) {
			long		percent;
			
			percent = -cell_width;
			cell_width = line_info->table_fit_rect.bot_right.h - line_info->table_fit_rect.top_left.h;		
			
			if (percent) {
				
				cell_width *= percent;
				cell_width /= 100;
			}
			
			cell_width /= par->table.table_columns;

			if (cell_width < MINIMUM_CELL_WIDTH)
				cell_width = MINIMUM_CELL_WIDTH;
		}
		
		use_cell_width = column_width(par, cell_index);

		if (use_cell_width == 0)
			use_cell_width = cell_width;
		
		par->column_var = use_cell_width;
		
		pgSetCellSides(par, (pg_short_t)cell_index, &these_sides);
		
		if (cell_index > 0) {
			
			pgSetCellSides(par, (pg_short_t)(cell_index - 1L), &previous_sides);
			
			if (previous_sides.bot_right.h) {
				
				left_extra = these_sides.top_left.h;
				pgHalfPenSize(&left_extra);
			}
		}
		else
			left_extra = these_sides.top_left.h;
		
		if (cell_index < (pgEffectiveColumns(par) - 1)) {

			pgSetCellSides(par, (pg_short_t)(cell_index + 1L), &next_sides);
			
			if (next_sides.top_left.h) {
				
				right_extra = these_sides.bot_right.h;
				pgHalfPenSize(&right_extra);
			}
		}
		else
			right_extra = these_sides.bot_right.h;

		line_info->fit_rect.bot_right.h = line_info->fit_rect.top_left.h + use_cell_width;
		line_info->cell_rect = line_info->fit_rect;
		
		if (line_info->cell_rect.bot_right.h > line_info->maximum_right)
			line_info->maximum_right = line_info->cell_rect.bot_right.h;
		
		line_info->fit_rect.top_left.h += left_extra;
		line_info->fit_rect.bot_right.h -= right_extra;
	}
}

/* pgEffectiveColumns returns the number of logical columns. This is not necessarily
par->table.table_columns since there can be column span values */

PG_PASCAL (long) pgEffectiveColumns (par_info_ptr par)
{
	long				result, index, span;
	
	result = par->table.table_columns;
	
	for (index = 0; index < par->table.table_columns; ++index) {
		
		if ((span = par->tabs[index].tab_type >> 24) > 0)
			result -= (span - 1);
	}
	
	if (result < 1)
		result = 1;

	return	result;
}

/* pgSectColumnBounds intersects bounds with the specified table column. The bounds rect
is typically the page or container rect. The function returns the maximum width
for the "paragraph" format. */

PG_PASCAL (long) pgSectColumnBounds (paige_rec_ptr pg, par_info_ptr par, long column_num,
		long wrap_num, rectangle_ptr bounds)
{
	rectangle		extra_indents;
	long			page_num, index, left_side, right_side, left_extra;
	long			right_extra, max_width;

	if (pg->doc_info.attributes & USE_MARGINS_BIT)
		extra_indents = pg->doc_info.margins;
	else
		pgFillBlock(&extra_indents, sizeof(rectangle), 0);
	
	page_num = (wrap_num / (GetMemorySize(pg->wrap_area) - 1)) + 1;
	pg->procs.page_modify(pg, page_num, &extra_indents);
	
	left_side = bounds->top_left.h + par->indents.left_indent + extra_indents.top_left.h;
	
	if (par->indents.first_indent < 0)
		left_side += par->indents.first_indent;

	right_side = bounds->bot_right.h - par->indents.right_indent - extra_indents.bot_right.h;
	max_width = right_side - left_side;

	if (column_num > 0) {

		index = 0;
		
		while (index < column_num) {
			
			left_side += column_width(par, index);
			index += 1;
		}
	}
	
	bounds->top_left.h = left_side;
	bounds->bot_right.h = left_side + column_width(par, column_num);

	left_right_cell_extra(par, column_num, &left_extra, &right_extra);
	bounds->bot_right.h -= right_extra;
	bounds->top_left.h -= left_extra;
	
	return	max_width;
}

/* pgTableJustifyOffset returns the amount the table should be offset per par
justification. */

PG_PASCAL (long) pgTableJustifyOffset (par_info_ptr par, long max_width)
{
	long		index, num_columns, span;
	long		diff = 0;

	if (par->justification == justify_center || par->justification == justify_right) {
		long			total_width;
		
		total_width = 0;
		num_columns = par->table.table_columns;

		for (index = 0; index < num_columns; ++index) {
		
			if (par->tabs[index].position)
				total_width += par->tabs[index].position;
			else
				total_width += par->column_var;
			
			span = par->tabs[index].tab_type >> 24;
			
			if (span)
				num_columns -= (span - 1);
		}

		diff = max_width - total_width;
		
		if (par->justification == justify_center)
			diff /= 2;
	}
	
	return	diff;
}


/* pgRowOffsets returns the bounding text offsets for the row that contains position.
The local_position param is the LOCAL position of the text relative to block->begin. */

PG_PASCAL (void) pgRowOffsets (text_block_ptr block, size_t local_position, size_t PG_FAR *begin,
				size_t PG_FAR *end)
{
	register pg_char_ptr		text;
	register size_t				position;
	size_t						text_size;
	
	text = (pg_char_ptr) UseMemory(block->text);
	
	if (begin) {
		
		position = local_position;

		while (position) {
			
			--position;
			
			if (text[position] == 0x0D) {
				
				++position;
				break;
			}
		}
		
		*begin = position + block->begin;
	}
	
	if (end) {
	
		text_size = block->end - block->begin;
		position = local_position;
		text += position;
		
		while (position < text_size) {
			
			++position;
			
			if (*text++ == 0x0D)
				break;
		}
		
		*end = position + block->begin;
	}

	UnuseMemory(block->text);
}

/* pgTableAdjustLine gets called after a line is calculated. If the line belongs to no
table this function does nothing.  */

PG_PASCAL (void) pgTableAdjustLine (paige_rec_ptr pg, pg_measure_ptr line_info)
{
	register point_start_ptr	starts;
	rectangle					pensizes;
	point_start_ptr				last_start;
	style_walk_ptr				walker;
	long						abs_position, v_extra, baseline_extra;
	pg_short_t					num_starts, cell_index, previous_flags;
	
	if (!line_info->is_table)
		return;
	
	line_info->block->flags |= BLOCK_HAS_TABLE;

	starts = last_start = line_info->starts;
	num_starts = line_info->num_starts;
	starts -= num_starts;
	--last_start;
	
	cell_index = (starts->cell_num & CELL_NUM_MASK) - 1;
	
	walker = line_info->styles;
	
	if (starts->offset == 0 || (starts->flags & NEW_PAR_BIT))
		previous_flags = PAR_BREAK_BIT;
	else {
		
		--starts;
		previous_flags = starts->flags;
		++starts;
	}
	
	if (cell_index == 0)
		starts->cell_num |= CELL_LEFT_BIT;
	if (last_start->flags & PAR_BREAK_BIT)
		starts->cell_num |= CELL_RIGHT_BIT;

	v_extra = baseline_extra = 0;

	pgSetCellSides(walker->cur_par_style, cell_index, &pensizes);

	abs_position = (long)starts->offset;
	abs_position += line_info->block->begin;

	if (previous_flags & (PAR_BREAK_BIT | TAB_BREAK_BIT)) {
		
		starts->cell_num |= CELL_TOP_BIT;
		
		if (abs_position >= line_info->table_top.begin && abs_position < line_info->table_top.end) {
			
			v_extra = pensizes.top_left.v;
			starts->cell_num |= CELL_FIRST_BIT;
		}
	}

	abs_position = (long)last_start->offset;
	abs_position += line_info->block->begin;

	if (last_start->flags & (PAR_BREAK_BIT | TAB_BREAK_BIT)) {
		
		starts->cell_num |= CELL_BOT_BIT;
		v_extra += pensizes.bot_right.v;
		baseline_extra = pensizes.bot_right.v;

		if (abs_position >= line_info->table_bottom.begin)
			starts->cell_num |= CELL_LAST_BIT;
	}

	if ((starts->cell_num & (CELL_TOP_BIT | CELL_BOT_BIT)) == (CELL_TOP_BIT | CELL_BOT_BIT)) {
		long		cell_height;
		
		cell_height = starts->bounds.bot_right.v - starts->bounds.top_left.v;
		cell_height += (long)starts->cell_height;

		if (cell_height < walker->cur_par_style->table.table_cell_height) {
			
			v_extra += (walker->cur_par_style->table.table_cell_height - cell_height);
			baseline_extra += (v_extra / 2);
		}
	}
	
	while (num_starts) {
		
		starts->bounds.bot_right.v += v_extra;
		starts->baseline += (short)baseline_extra;
		
		++starts;
		--num_starts;
	}

	line_info->actual_rect.bot_right.v += v_extra;
}


/********************************* Local Functions ***************************/


static void set_info_rects (paige_rec_ptr pg, pg_measure_ptr line_info)
{
	line_info->r_num_end = line_info->r_num_begin;
	line_info->wrap_r_begin = line_info->wrap_r_end = line_info->wrap_r_base + pgGetWrapRect(pg,
			line_info->r_num_begin, &line_info->repeat_offset);
}


/* get_table_format returns the par_info for a table, or NULL if position is not a table. NOTE,
THIS RETURNS A USED MEMORY REF (pg->par_formats). */

static par_info_ptr get_table_format (paige_rec_ptr pg, long position)
{
	par_info_ptr		format;

	format = pgFindParStyle(pg, pgFixOffset(pg, position));
	
	if (format->table.table_columns != 0)
		return	format;

	UnuseMemory(pg->par_formats);
	
	return	NULL;
}

/* build_column_positions builds the positions of each column. These are unscaled, unscrolled. */

static memory_ref build_column_widths (paige_rec_ptr pg, par_info_ptr par)
{
	memory_ref		result;
	long PG_FAR		*positions;
	long			index;

	result = MemoryAlloc(pg->globals->mem_globals, sizeof(long), par->table.table_columns, 0);
	positions = (long *) UseMemory(result);
	
	if (par->table.table_column_width > 0) {
		long			max_columns;
		
		if ((max_columns = par->table.table_columns) > TAB_ARRAY_SIZE)
			max_columns = TAB_ARRAY_SIZE;

		for (index = 0; index < par->table.table_columns; ++index)
			*positions++ = (par->tabs[index].position & TAB_POS_MASK);
	}
	else {

		for (index = 0; index < par->table.table_columns; ++index)
			*positions++ = par->column_var;
	}

	UnuseMemory(result);
	
	return		result;
}

/* tab_to_cell returns the offset to the next cell assuming the current_cell position.
If advance_row is TRUE then we are looking for next row only. */

static long tab_to_cell (paige_rec_ptr pg_rec, long current_cell, pg_boolean advance_row)
{
	text_block_ptr			block;
	pg_char_ptr				text;
	pg_char					next_char;
	long					current_insertion, local_offset, local_end;
	
	current_insertion = current_cell;
	
	if (current_insertion < pg_rec->t_length) {

		block = pgFindTextBlock(pg_rec, current_insertion, NULL, FALSE, TRUE);
		local_offset = current_insertion - block->begin;
		local_end = block->end - block->begin;

		text = (pg_char_ptr) UseMemoryRecord(block->text, local_offset, 0, TRUE);
		
		for (;;) {
			
			if (local_offset == local_end) {
				
				UnuseMemory(block->text);

				++block;
				pg_rec->procs.load_proc(pg_rec, block);
				text = (pg_char_ptr) UseMemory(block->text);
				local_offset = 0;
				local_end = block->end - block->begin;
			}
			
			++local_offset;
			next_char = *text++;
			
			if (advance_row) {

				if (next_char == 0x0D)
					break;
			}
			else {
			
				if (next_char == 0x09 || next_char == 0x0D)
					break;
			}
		}
		
		current_insertion = local_offset + block->begin;

		UnuseMemory(block->text);
		UnuseMemory(pg_rec->t_blocks);
	}
	
	return		current_insertion;
}

/* column_insert_position returns the text position required to insert a new column (tab char).
The beginning position is entered as range->begin,  upon return range->begin is set to the
place to insert and range->end is set to the end of that column (tab or cr not included).
If row_end is non-NULL it is set to the end of the row. */

static void column_insert_position (paige_rec_ptr pg_rec, short column_num, select_pair_ptr range,
				long PG_FAR *row_end)
{
	text_block_ptr			block;
	pg_char_ptr				text;
	long					local_position, insert_position;
	pg_char					last_char;
	short					index;
	
	block = pgFindTextBlock(pg_rec, range->begin, NULL, FALSE, TRUE);
	insert_position = range->begin;
	local_position = range->begin - block->begin;
	text = (pg_char_ptr) UseMemoryRecord(block->text, local_position, 0, TRUE);
	last_char = 0;

	for (index = 0; index < column_num; ++index) {
		
		for (;;) {
			
			++insert_position;
			last_char = *text++;
			
			if (last_char == 9 || last_char == 0x0D)
				break;
		}
		
		if (last_char == 0x0D)
			break;
	}
	
	if (last_char == 0x0D)
		range->end = range->begin = insert_position - 1;
	else {
		
		range->begin = insert_position;
		
		for (;;) {
			
			++insert_position;
			last_char = *text++;
			
			if (last_char == 9 || last_char == 0x0D)
				break;
		}
		
		range->end = insert_position - 1;

		if (last_char != 0x0D) {
			
			for (;;) {
				
				++insert_position;
				
				if (*text++ == 0x0D)
					break;
			}
		}
	}
	
	if (row_end)
		*row_end = insert_position;

	UnuseMemory(block->text);
	UnuseMemory(pg_rec->t_blocks);
}



/* row_insert_position returns the text position required to insert a new row.
The beginning position is entered as range->begin,  upon return range->begin is set to the
place to insert and range->end is set to the END of the row. */

static void row_insert_position (paige_rec_ptr pg_rec, long row_num, select_pair_ptr range)
{
	text_block_ptr			block;
	pg_char_ptr				text;
	select_pair				table_range;
	long					index, local_position, end_position;

	pgTableOffsets(pg_rec->myself, range->begin, &table_range);

	block = pgFindTextBlock(pg_rec, table_range.begin, NULL, FALSE, TRUE);
	local_position = table_range.begin - block->begin;
	end_position = block->end - block->begin;
	text = (pg_char_ptr) UseMemoryRecord(block->text, local_position, 0, TRUE);
	index = 0;
	
	while (table_range.begin < table_range.end) {
		
		if (index == row_num) {
			
			table_range.end = table_range.begin;
			
			for (;;) {
				
				++table_range.end;
				if (*text++ == 0x0D)
					break;
			}
			
			break;
		}
		
		for (;;) {
			
			++table_range.begin;
			++local_position;
			
			if (*text++ == 0x0D)
				break;
		}

		++index;
		
		if (local_position >= end_position && table_range.begin < table_range.end) {
			
			UnuseMemory(block->text);
			++block;
			pg_rec->procs.load_proc(pg_rec, block);
			text = (pg_char_ptr) UseMemory(block->text);
			local_position = 0;
			end_position = block->end - block->begin;
		}
	}
	
	*range = table_range;
	
	UnuseMemory(block->text);
	UnuseMemory(pg_rec->t_blocks);
}

/* clear_selections nulls out all selections (makes minimum position). */

static void clear_selections (paige_rec_ptr pg_rec, long new_position, short draw_mode)
{
	t_select_ptr		new_selection;
	
	if (draw_mode)
		pgRemoveAllHilites(pg_rec, draw_mode);

	SetMemorySize(pg_rec->select, MINIMUM_SELECT_MEMSIZE);
	new_selection = (t_select_ptr) UseMemory(pg_rec->select);
	new_selection->flags |= SELECTION_DIRTY;
	new_selection->offset = new_position;
	new_selection[1] = new_selection[0];
	pg_rec->stable_caret.h = pg_rec->stable_caret.v = 0;
	pg_rec->num_selects = 0;
	UnuseMemory(pg_rec->select);
}

/* adjust_overlapping_pages checks for the situation that a table's cell is extended beyond
the boundaries of a "page" or container bottom. If so we make an adjustment. */

static void adjust_overlapping_pages (paige_rec_ptr pg, pg_measure_ptr line_info)
{
	rectangle_ptr			wrap_r;
	point_start_ptr			starts;
	pg_short_t				num_starts;
	long					r_bottom, r_top, adjustment;

	if (!(pg->doc_info.attributes & BOTTOM_FIXED_BIT))
		return;
	

	wrap_r = line_info->wrap_r_base;
	wrap_r += pgGetWrapRect(pg, line_info->r_num_begin, &line_info->repeat_offset);

	r_bottom = wrap_r->bot_right.v + line_info->repeat_offset.v;
	r_bottom -= line_info->extra_indents.bot_right.v;
	
	if (line_info->actual_rect.bot_right.v <= r_bottom)
		return;
	
	++line_info->r_num_begin;
	wrap_r = line_info->wrap_r_base;
	wrap_r += pgGetWrapRect(pg, line_info->r_num_begin, &line_info->repeat_offset);

	r_top = wrap_r->top_left.v + line_info->repeat_offset.v;
	r_top += line_info->extra_indents.top_left.v;

	adjustment = r_top - line_info->actual_rect.top_left.v;
	starts = line_info->starts;
	num_starts = line_info->num_starts;
	starts -= num_starts;
	
	while (num_starts) {
		
		starts->r_num = line_info->r_num_begin;
		pgOffsetRect(&starts->bounds, 0, adjustment);
		
		++starts;
		--num_starts;
	}
	
	pgOffsetRect(&line_info->actual_rect, 0, adjustment);
}

/* left_right_cell_extra compensates for large pen sizes on the left and right of a cell. */

static void left_right_cell_extra (par_info_ptr par, long column_num, long PG_FAR *left_extra,
		long PG_FAR *right_extra)
{
	long		border_info, test_side;
	
	*left_extra = *right_extra = 0;

	if (!(border_info = par->tabs[column_num].leader))
		border_info = par->table.border_info;
	
	if (border_info & PG_BORDER_RIGHT)
		if (column_num < (pgEffectiveColumns(par) - 1)) {
			
			if (!(test_side = par->tabs[column_num + 1].leader))
				test_side = par->table.border_info;
			
			if (test_side & PG_BORDER_LEFT)
				*right_extra = (((test_side & PG_BORDER_LEFT) & PG_BORDER_PENSIZE) / 2);
		}
	
	if (column_num > 0)
		if (border_info & PG_BORDER_LEFT) {

			if (!(test_side = par->tabs[column_num - 1].leader))
				test_side = par->table.border_info;
			
			if (test_side & PG_BORDER_RIGHT)
				*left_extra = (((border_info & PG_BORDER_LEFT) & PG_BORDER_PENSIZE) / 2);
	}
}

/* column_width returns the effective column width of column_num. */

static long column_width (par_info_ptr par, long column_num)
{
	long		width;
	
	if ((width = par->tabs[column_num].position) == 0)
		width = par->column_var;
	
	return	width;
}

/* change_to_table is the low level function that converts text to a table. */

static pg_short_t change_to_table (change_info_ptr change, style_run_ptr style)
{
	paige_rec_ptr				pg;
	par_info					new_style;
	pg_short_t					tab_index;

	pg = change->pg;
	
	GetMemoryRecord(pg->par_formats, style->style_item, &new_style);
	
	if (new_style.table.table_columns)
		return	style->style_item;
	
	new_style.table.table_columns = (long)new_style.num_tabs;
	new_style.table.unique_id = change->class_OR;

	if (new_style.num_tabs) {
		rectangle		bounds;
		pg_short_t		num_tabs;
		long			left_side = 0;
		long			next_left_side;

		num_tabs = new_style.num_tabs;
		
		pgShapeBounds(pg->wrap_area, &bounds);

		if (pg->doc_info.attributes & USE_MARGINS_BIT) {
			
			bounds.top_left.h += pg->doc_info.margins.top_left.h;
			bounds.bot_right.h -= pg->doc_info.margins.bot_right.h;
		}
		
		if (pg->tab_base == 0)
			left_side = bounds.top_left.h;
		
		for (tab_index = 0; tab_index < new_style.num_tabs; ++tab_index) {
			
			if (new_style.tabs[tab_index].position > 0) {

				next_left_side = new_style.tabs[tab_index].position;
				new_style.tabs[tab_index].position -= left_side;
				left_side = next_left_side;
			}
			
			new_style.tabs[tab_index].leader = new_style.table.border_info;
			new_style.tabs[tab_index].ref_con = new_style.table.border_shading;
		}
	}
	
	change->changed = TRUE;

	return	pgAddParInfo(pg, NULL, internal_clone_reason, &new_style);
}


/* change_to_table is the low level function that converts a table to regular text. */

static pg_short_t change_to_text (change_info_ptr change, style_run_ptr style)
{
	paige_rec_ptr				pg;
	par_info					new_style;
	pg_short_t					tab_index;

	pg = change->pg;
	
	GetMemoryRecord(pg->par_formats, style->style_item, &new_style);
	
	if (!new_style.table.table_columns)
		return	style->style_item;
	
	pgFillBlock(&new_style.table, sizeof(pg_table), 0);
	if (new_style.num_tabs) {
		long			left_side = 0;

		new_style.table.border_info = new_style.tabs[0].leader;
		new_style.table.border_shading = new_style.tabs[0].ref_con;

		if (pg->tab_base == 0) {
			rectangle		bounds;
			
			pgShapeBounds(pg->wrap_area, &bounds);
			left_side = bounds.top_left.h;
		}

		for (tab_index = 0; tab_index < new_style.num_tabs; ++tab_index) {
			
			left_side += new_style.tabs[tab_index].position;
			new_style.tabs[tab_index].position = left_side;
			new_style.tabs[tab_index].tab_type &= 0x00000FFF;
			
			if (new_style.tabs[tab_index].tab_type == 0)
				new_style.tabs[tab_index].tab_type = left_tab;
			
			new_style.tabs[tab_index].leader = new_style.tabs[tab_index].ref_con = 0;
		}
	}
	
	change->changed = TRUE;

	return	pgAddParInfo(pg, NULL, internal_clone_reason, &new_style);
}

/* count_tabs returns the number of tabs in the selection, and when this returns
selection->end is set o the actual par end. */

static pg_short_t count_tabs (paige_rec_ptr pg_rec, select_pair_ptr selection)
{
	text_block_ptr			block;
	pg_char_ptr				text;
	pg_char					next_char;
	long					local_offset, local_end;
	pg_short_t				count;
	
	block = pgFindTextBlock(pg_rec, selection->begin, NULL, FALSE, TRUE);
	local_offset = selection->begin - block->begin;
	local_end = block->end - block->begin;

	text = (pg_char_ptr) UseMemoryRecord(block->text, local_offset, 0, TRUE);
	count = 0;

	while (local_offset < local_end) {
		
		++local_offset;
		next_char = *text++;
		
		if (next_char == 0x0D)
			break;
		if (next_char == 0x09)
			count += 1;
	}
	
	UnuseMemory(block->text);
	
	selection->end = local_offset + block->begin;
	UnuseMemory(pg_rec->t_blocks);
	
	return	count;
}