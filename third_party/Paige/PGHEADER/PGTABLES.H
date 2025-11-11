/* Header file for pgTables.c.  Definitions for table feature. */

/* This header file goes with pgParagraph.h     */

#ifndef PGTABLES_H
#define PGTABLES_H

#include "Paige.h"

#define TABLE_CR_CHAR		0x0A;	/* "Linefeed" char within a cell */
#define TAB_POS_MASK		0x0000FFFF	/* Part of tab.position that holds cell width */

#ifdef __cplusplus
extern	"C" {
#endif

extern PG_PASCAL (void) pgInsertTable (pg_ref pg, long position, pg_table_ptr table,
		long row_qty, short draw_mode);

extern PG_PASCAL (pg_boolean) pgIsTable (pg_ref pg, long position);

extern PG_PASCAL (short) pgPtInTable (pg_ref pg, co_ordinate_ptr point, rectangle_ptr column_sides,
		select_pair_ptr offsets);

extern PG_PASCAL (memory_ref) pgTableColumnWidths (pg_ref pg, long position);

extern PG_PASCAL (void) pgCellOffsets (pg_ref pg, long position, select_pair_ptr offsets);

extern PG_PASCAL (void) pgTableOffsets (pg_ref pg, long position, select_pair_ptr offsets);

extern PG_PASCAL (void) pgSetColumnBorders (pg_ref pg, long position, short column_num,
		long border_info, short draw_mode);

extern PG_PASCAL (void) pgSetColumnShading (pg_ref pg, long position, short column_num,
		long shading, short draw_mode);

extern PG_PASCAL (void) pgSetColumnAlignment (pg_ref pg, long position, short column_num,
		short alignment, short draw_mode);

extern PG_PASCAL (void) pgSetColumnWidth (pg_ref pg, long position, short column_num,
		short width, short draw_mode);

extern PG_PASCAL (short) pgPositionToColumn (pg_ref pg, long position);

extern PG_PASCAL (long) pgPositionToRow (pg_ref pg, long position, select_pair_ptr offsets);

extern PG_PASCAL (pg_boolean) pgGetColumnInfo (pg_ref pg, long position, short column_num, tab_stop_ptr info);

extern PG_PASCAL (short) pgNumColumns (pg_ref pg, long position);

extern PG_PASCAL (long) pgNumRows (pg_ref pg, long position);

extern PG_PASCAL (void) pgInsertColumn (pg_ref pg, long position, short column_num,
		tab_stop_ptr info, short draw_mode);

extern PG_PASCAL (void) pgInsertRow (pg_ref pg, long position, long row_num, short draw_mode);

extern PG_PASCAL (void) pgDeleteColumn (pg_ref pg, long position, short column_num, short draw_mode);

extern PG_PASCAL (void) pgDeleteRow (pg_ref pg, long position, long row_num, short draw_mode);

extern PG_PASCAL (void) pgConvertTableText (pg_ref pg, select_pair_ptr selection,
		pg_boolean text_to_table, short draw_mode);


/********************* Low level utilities (used by PAIGE internally) **********************/

extern PG_PASCAL (void) pgRowOffsets (text_block_ptr block, size_t local_position, size_t PG_FAR *begin,
				size_t PG_FAR *end);

extern PG_PASCAL (pg_boolean) pgPositionInTable (paige_rec_ptr pg, long position, select_pair_ptr selection);

extern PG_PASCAL (pg_short_t) pgTableRowBounds (point_start_ptr line_starts, rectangle_ptr bounds);

extern PG_PASCAL (short) pgPtToTableOffset (paige_rec_ptr pg_rec, co_ordinate_ptr the_point,
			select_pair_ptr offsets, rectangle_ptr column_sides, t_select_ptr actual_selection);

extern PG_PASCAL (void) pgTabToCell (paige_rec_ptr pg_rec);

extern PG_PASCAL (memory_ref) pgCellSelections (paige_rec_ptr pg_rec, select_pair_ptr selection);

extern PG_PASCAL (void) pgFixCellBackspace (paige_rec_ptr pg, size_t PG_FAR *delete_from, size_t PG_FAR *delete_to);

extern PG_PASCAL (pg_boolean) pgPasteToCells (paige_rec_ptr pg_rec, size_t position,
		pg_char_ptr text, size_t length);
		
extern PG_PASCAL (void) pgGetTableBounds (paige_rec_ptr pg_rec, long position, rectangle_ptr bounds);

extern PG_PASCAL (void) pgSetTableRect (paige_rec_ptr pg_rec, short rect_verb, pg_measure_ptr line_info);

extern PG_PASCAL (long) pgSectColumnBounds (paige_rec_ptr pg, par_info_ptr par, long column_num,
		long wrap_num, rectangle_ptr bounds);

extern PG_PASCAL (long) pgTableJustifyOffset (par_info_ptr par, long max_width);

extern PG_PASCAL (void) pgSetCellSides (par_info_ptr par, pg_short_t cell_num, rectangle_ptr sides);

extern PG_PASCAL (void) pgMeasureMaxColumns (paige_rec_ptr pg, select_pair_ptr offsets);

extern PG_PASCAL (void) pgTableAdjustLine (paige_rec_ptr pg, pg_measure_ptr line_info);

extern PG_PASCAL (long) pgEffectiveColumns (par_info_ptr par);

#ifdef __cplusplus
	}
#endif

#endif
