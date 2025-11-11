/* This file handles paragraph formatting. Your app can omit this library if no
functions that set paragraph styles defined in Paige.h are called.    */

#include "Paige.h"

#ifdef MAC_PLATFORM
#pragma segment pgpars
#endif

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

static void get_par_info_proc (change_info_ptr change, style_run_ptr style, pg_short_t record_ctr);
static pg_short_t change_par_info (change_info_ptr change, style_run_ptr style);
static void set_par_info (pg_ref pg, const select_pair_ptr selection,
		const par_info_ptr info, const par_info_ptr mask, pg_boolean tables,
		pg_boolean non_tables, short draw_mode);
static long get_par_info (pg_ref pg, const select_pair_ptr selection,
		short set_any_match, par_info_ptr info, par_info_ptr mask,
		pg_boolean tables, pg_boolean non_tables);


/* pgGetParInfo is the same as pgGetStyleInfo but for paragraph formats.   */

PG_PASCAL (long) pgGetParInfo (pg_ref pg, const select_pair_ptr selection,
		short set_any_match, par_info_ptr info, par_info_ptr mask)
{
	return	get_par_info(pg, selection, set_any_match, info, mask, FALSE, FALSE);
}



/* pgSetParInfo sets the specified range(s) to a new paragraph format */

PG_PASCAL (void) pgSetParInfo (pg_ref pg, const select_pair_ptr selection,
		const par_info_ptr info, const par_info_ptr mask, short draw_mode)
{
	set_par_info(pg, selection, info, mask, FALSE, FALSE, draw_mode);
}

/* pgGetParInfoEx is the same as pgGetParInfo() except only tables or non-tables
are specified. */

PG_PASCAL (long) pgGetParInfoEx (pg_ref pg, const select_pair_ptr selection,
		pg_boolean set_any_match, par_info_ptr info, par_info_ptr mask, pg_boolean tables_only)
{
	pg_boolean		tables = FALSE;
	pg_boolean		non_tables = FALSE;

	if (tables_only)
		tables = TRUE;
	else
		non_tables = TRUE;

	return	get_par_info(pg, selection, set_any_match, info, mask, tables, non_tables);
}

/* pgSetParInfoEx is the same as pgSetParInfo() except only tables or non-tables
are specified. */

PG_PASCAL (void) pgSetParInfoEx (pg_ref pg, const select_pair_ptr selection,
		const par_info_ptr info, const par_info_ptr mask, pg_boolean tables_only, short draw_mode)
{
	pg_boolean		tables = FALSE;
	pg_boolean		non_tables = FALSE;

	if (tables_only)
		tables = TRUE;
	else
		non_tables = TRUE;

	set_par_info(pg, selection, info, mask, tables, non_tables, draw_mode);
}



/* pgGetParInfoRec returns the style_item'th paragraph style record  */

PG_PASCAL (void) pgGetParInfoRec (pg_ref pg, short style_item, par_info_ptr format)
{
	paige_rec_ptr			pg_rec;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	GetMemoryRecord(pg_rec->par_formats, style_item, format);

	UnuseMemory(pg);
}


/* pgParMaxWidth returns the maximum width of a paragraph at the specified position.  */

PG_PASCAL (long) pgParMaxWidth (pg_ref pg, long position)
{
	paige_rec_ptr		pg_rec;
	par_info_ptr		par_style;
	rectangle_ptr		wrap_bounds;
	long				result;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	
	par_style = pgFindParStyle(pg_rec, pgFixOffset(pg_rec, position));
	wrap_bounds = (rectangle_ptr) UseMemory(pg_rec->wrap_area);
	
	result = wrap_bounds->bot_right.h - wrap_bounds->top_left.h;
	result -= (par_style->indents.left_indent + par_style->indents.right_indent);
	if (par_style->indents.first_indent < 0)
		result -= par_style->indents.first_indent;

	UnuseMemory(pg_rec->wrap_area);
	UnuseMemory(pg_rec->par_formats);
	UnuseMemory(pg);
	
	return	result;
}


/* pgGetIndents returns just the indent values of the specified selection. The
mask is filled with zeros or 1's if inconsistency / consistency exists.  If
left_screen_offset is non-NULL it gets set to the amount of offset required to draw
the left & first indents on the screen as-is;  if right_screen_offset is non-NULL
it is set to the right side to base screen drawing for the right indent.  */

PG_PASCAL (void) pgGetIndents (pg_ref pg, const select_pair_ptr selection, pg_indents_ptr indents,
		pg_indents_ptr mask, long PG_FAR *left_screen_offset, long PG_FAR *right_screen_offset)
{
	paige_rec_ptr			pg_rec;
	par_info				info, indent_mask;
	long					first_select;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	
	pgGetParInfo(pg, selection, FALSE, &info, &indent_mask);

	if (!selection)
		first_select = pgCurrentInsertion(pg_rec);
	else
		first_select = selection->begin;

	pgBlockMove(&info.indents.left_indent, indents, sizeof(pg_indents));
	
	if (mask)
		pgBlockMove(&indent_mask.indents.left_indent, mask, sizeof(pg_indents));

	if (left_screen_offset || right_screen_offset) {
		rectangle			bounds;
		
		pgGetScreenOffsets(pg_rec, first_select, TAB_WRAP_RELATIVE, &bounds);
		
		if (left_screen_offset)
			*left_screen_offset = bounds.top_left.h;
		if (right_screen_offset)
			*right_screen_offset = bounds.bot_right.h;
	}
	
	UnuseMemory(pg);
}



/* pgSetIndents changes only the indents in the selection range. In every respect
it is the same as pgSetParInfo.  */

PG_PASCAL (void) pgSetIndents (pg_ref pg, const select_pair_ptr selection, const pg_indents_ptr indents,
		const pg_indents_ptr mask, short draw_mode)
{
	paige_rec_ptr			pg_rec;
	par_info				info, indent_mask;

	pg_rec = (paige_rec_ptr) UseMemory(pg);

	pgFillBlock(&info, sizeof(par_info), 0);
	pgFillBlock(&indent_mask, sizeof(par_info), 0);
	pgBlockMove(indents, &info.indents.left_indent, sizeof(pg_indents));
	pgBlockMove(mask, &indent_mask.indents.left_indent, sizeof(pg_indents));

	pgSetParInfoEx(pg, selection, &info, &indent_mask, FALSE, draw_mode);
	
	UnuseMemory(pg);
}

/* pgBasicallyEqualPars returns TRUE if the paragraph recs are essentially equal. */

PG_PASCAL (pg_boolean) pgBasicallyEqualPars (par_info_ptr src, par_info_ptr target)
{
	par_info			mask;

	pgFillBlock(&mask, SIGNIFICANT_PAR_STYLE_SIZE, 0);
	mask.justification = mask.outline_level = -1;
	mask.num_tabs = 0xFFFF;
	pgFillBlock(mask.tabs, TAB_ARRAY_SIZE * sizeof(tab_stop), -1);
	pgFillBlock(&mask.table, sizeof(pg_table), -1);
	pgFillBlock(&mask.indents, sizeof(pg_indents), -1);
	mask.spacing = mask.leading_extra = mask.leading_fixed = mask.top_extra
				 = mask.bot_extra = mask.left_extra = mask.right_extra = -1;
	mask.html_bullet = mask.html_style = -1;

	return	(pgEqualStructMasked(src, target, &mask, SIGNIFICANT_PAR_STYLE_SIZE));
}


/* pgGetScreenOffsets function returns the screen bounding left/right edges belonging to
the first selection. It can make a difference depending on base_to_use.  */

PG_PASCAL (void) pgGetScreenOffsets (paige_rec_ptr pg, long first_select,
		long base_to_use, rectangle_ptr bounds)
{
	register text_block_ptr		block;
	register point_start_ptr	starts;
	rectangle_ptr				wrap_base;
	co_ordinate					repeat_offset;
	rectangle					page_margins;
	long						local_offset, r_num;

	if (base_to_use == TAB_BOUNDS_RELATIVE)
		r_num = 0;
	else {
	
		block = pgFindTextBlock(pg, first_select, NULL, TRUE, TRUE);
		starts = (point_start_ptr) UseMemory(block->lines);
		local_offset = first_select - block->begin;
		
		while (local_offset > (long)starts->offset)
			++starts;
		
		if ((local_offset != (long)starts->offset) || (starts->flags == TERMINATOR_BITS))
			if (starts->offset)
				--starts;

		r_num = starts->r_num;
	
		UnuseMemory(block->lines);
		UnuseMemory(pg->t_blocks);
	}
	
	wrap_base = (rectangle_ptr) UseMemory(pg->wrap_area);
	++wrap_base;
	
	wrap_base += pgGetWrapRect(pg, r_num, &repeat_offset);
	pgBlockMove(wrap_base, bounds, sizeof(rectangle));
	pgOffsetRect(bounds, repeat_offset.h, repeat_offset.v);
	
	UnuseMemory(pg->wrap_area);

	if (pg->doc_info.attributes & USE_MARGINS_BIT)
		page_margins = pg->doc_info.margins;
	else
		pgFillBlock(&page_margins, sizeof(rectangle), 0);

	pg->procs.page_modify(pg, 0, &page_margins);
	
	bounds->top_left.h += page_margins.top_left.h;
	bounds->bot_right.h -= page_margins.bot_right.h;

	if (base_to_use >= 0)
		pgOffsetRect(bounds, base_to_use, 0);
}


/* pgSetParProcs is 100% identical to pgSetStyleProcs except paragraph style
records are changed.  */

PG_PASCAL (void) pgSetParProcs (pg_ref pg, const pg_par_hooks PG_FAR *procs,
		const par_info_ptr match_style, const par_info_ptr mask_style,
		const par_info_ptr AND_style, long user_data, long user_id,
		pg_boolean inval_text, short draw_mode)
{
	paige_rec_ptr		pg_rec;
	par_info_ptr		styles;
	pg_short_t			style_qty;
	short				some_changed;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	some_changed = FALSE;

	for (styles = (par_info_ptr) UseMemory(pg_rec->par_formats), style_qty = (pg_short_t)GetMemorySize(pg_rec->par_formats);
			style_qty; ++styles, --style_qty)
		if (pgStyleMatchesCriteria(styles, match_style, mask_style, AND_style,
				SIGNIFICANT_PAR_STYLE_SIZE)) {
			
			pgBlockMove(procs, &styles->procs, sizeof(pg_par_hooks));
			styles->user_data = user_data;
			styles->user_id = user_id;

			some_changed = TRUE;
		}
	
	UnuseMemory(pg_rec->par_formats);
	
	if (some_changed) {
		
		if (inval_text)
			pgInvalSelect(pg, 0, pg_rec->t_length);
		
		if (draw_mode)
			pgUpdateText(pg_rec, NULL, 0, pg_rec->t_length, MEM_NULL, NULL, draw_mode,
					TRUE);
	}

	UnuseMemory(pg);
}


/* pgSetSpecialTabFlags is a new featured addition to version 1.1. This allows
two possible special flags to be set:

		TAB_TEXT_HIDDEN_BIT -- hides text from tab n to tab n + 1
		TAB_TRUNCATES_BIT -- text cannot go beyond next tab, truncates if so.
							Draws with "elipse" char when truncated.

The tab_flags memory ref holds an array of tab stops. If the tab_stop has zero
for its "type" field the tab isn't set, otherwise the type field is OR-d with
each corresponding tab. If inval_text is TRUE the text is invalidated. The
draw_mode is as always. 			*/


PG_PASCAL (void) pgSetSpecialTabFlags (pg_ref pg, const select_pair_ptr selection,
		memory_ref tab_flags, pg_boolean inval_text, short draw_mode)
{
	paige_rec_ptr			pg_rec;
	register tab_stop_ptr	tabs_in, tabs_out;
	memory_ref				range_ref;
	style_walk				walker;
	select_pair_ptr			apply_range;
	long					num_in, num_out;
	pg_short_t				index;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	range_ref = pgSetupOffsetRun(pg_rec, selection, TRUE, FALSE);
	apply_range = (select_pair_ptr) UseMemory(range_ref);

	num_in = GetMemorySize(tab_flags);
	tabs_in = (tab_stop_ptr) UseMemory(tab_flags);

	pgPrepareStyleWalk(pg_rec, apply_range->begin, &walker, TRUE);
	
	for (;;) {
		
		if (num_out = walker.cur_par_style->num_tabs) {
			
			tabs_out = walker.cur_par_style->tabs;
			
			for (index = 0; (index < (pg_short_t)num_out) && (index < (pg_short_t)num_in); ++index) {
				
				if (tabs_in[index].tab_type)
					tabs_out[index].tab_type |= tabs_in[index].tab_type;
			}
		}
		
		if (walker.next_par_run->offset >= apply_range->end)
			break;
		
		pgSetWalkStyle(&walker, walker.next_par_run->offset);
	}

	UnuseMemory(tab_flags);
	pgPrepareStyleWalk(pg_rec, 0, NULL, TRUE);

	if (inval_text)
		pgInvalSelect(pg, apply_range->begin, apply_range->end);
	
	if (draw_mode)
		pgUpdateText(pg_rec, NULL, apply_range->begin, apply_range->end - apply_range->begin,
				MEM_NULL, NULL, draw_mode, TRUE);

	UnuseAndDispose(range_ref);
	UnuseMemory(pg);
}


/* pgClearSpecialTabFlags clears all special flags (set from above function)
for the range specified. */

PG_PASCAL (void) pgClearSpecialTabFlags (pg_ref pg, const select_pair_ptr selection,
		pg_boolean inval_text, short draw_mode)
{
	paige_rec_ptr			pg_rec;
	register tab_stop_ptr	tabs_out;
	memory_ref				range_ref;
	style_walk				walker;
	select_pair_ptr			apply_range;
	long					num_out;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	range_ref = pgSetupOffsetRun(pg_rec, selection, TRUE, FALSE);
	apply_range = (select_pair_ptr) UseMemory(range_ref);

	pgPrepareStyleWalk(pg_rec, apply_range->begin, &walker, TRUE);
	
	for (;;) {
		
		if (num_out = walker.cur_par_style->num_tabs) {
			
			tabs_out = walker.cur_par_style->tabs;
			
			while (num_out) {
				
				tabs_out->tab_type &= TAB_TYPE_MASK;
				++tabs_out;
				--num_out;
			}
		}
		
		if (walker.next_par_run->offset >= apply_range->end)
			break;
		
		pgSetWalkStyle(&walker, walker.next_par_run->offset);
	}

	pgPrepareStyleWalk(pg_rec, 0, NULL, TRUE);

	if (inval_text)
		pgInvalSelect(pg, apply_range->begin, apply_range->end);
	
	if (draw_mode)
		pgUpdateText(pg_rec, NULL, apply_range->begin, apply_range->end - apply_range->begin,
				MEM_NULL, NULL, draw_mode, TRUE);

	UnuseAndDispose(range_ref);
	UnuseMemory(pg);
}


/************************************ Local Functions **************************/

/* This is the "get" proc that is called for obtaining info on a paragraph record. */

static void get_par_info_proc (change_info_ptr change, style_run_ptr style, pg_short_t record_ctr)
{
	register par_info_ptr			base_style;
	
	base_style = (par_info_ptr) change->base;
	base_style += style->style_item;
	
	if (change->any_match)
		pgSetMaskFromFlds(base_style, change->par_change, change->par_mask,
			par_compare, TRUE, FALSE);
	else {
	
		if (!record_ctr) {

			pgBlockMove(base_style, change->par_change, sizeof(par_info));
			
			if (change->non_tables && base_style->table.table_columns) {
				
				pgFillBlock(change->par_change->tabs, TAB_ARRAY_SIZE * sizeof(tab_stop), 0);
				pgFillBlock(&change->par_change->table, sizeof(pg_table), 0);
				change->par_change->num_tabs = 0;
			}
		}
		else
			pgSetMaskFromFlds(base_style, change->par_change, change->par_mask,
				par_compare, FALSE, change->non_tables);
	}
}


/* This changes paragraph information (similar to change_style_info)  */

static pg_short_t change_par_info (change_info_ptr change, style_run_ptr style)
{
	paige_rec_ptr				pg;
	par_info_ptr				change_mask;
	par_info					new_style, old_style, table_mask;

	pg = change->pg;
	
	GetMemoryRecord(pg->par_formats, style->style_item, &new_style);
	old_style = new_style;
	change_mask = change->par_mask;
	
	if (change->tables && !new_style.table.table_columns)
		return	style->style_item;
	
	if (change->non_tables && new_style.table.table_columns) {
		
		table_mask = *change_mask;
		pgFillBlock(table_mask.tabs, TAB_ARRAY_SIZE * sizeof(tab_stop), 0);
		pgFillBlock(&table_mask.table, sizeof(pg_table), 0);
		table_mask.num_tabs = 0;
		change_mask = &table_mask;
	}

	if (pgSetFldsFromMask(&new_style, change->par_change, change_mask, SIGNIFICANT_PAR_STYLE_SIZE)) {
		
		++change->changed;
	
	// Handle "bits" for border info(s):
	
		new_style.table.border_info = old_style.table.border_info & (~change_mask->table.border_info);
		new_style.table.border_info |= (change->par_change->table.border_info & change_mask->table.border_info);

		new_style.table.cell_borders = old_style.table.cell_borders & (~change_mask->table.cell_borders);
		new_style.table.cell_borders |= (change->par_change->table.cell_borders & change_mask->table.cell_borders);

		new_style.table.grid_borders = old_style.table.grid_borders & (~change_mask->table.grid_borders);
		new_style.table.grid_borders |= (change->par_change->table.grid_borders & change_mask->table.grid_borders);
		
		if (new_style.table.table_columns) {
			pg_short_t		tab_index, num_tabs;
			
			num_tabs = new_style.num_tabs;
			
			for (tab_index = 0; tab_index < num_tabs; ++tab_index) {
				
				if (change_mask->tabs[tab_index].leader) {
				
					new_style.tabs[tab_index].leader = old_style.tabs[tab_index].leader & (~change_mask->tabs[tab_index].leader);
					new_style.tabs[tab_index].leader |= (change->par_change->tabs[tab_index].leader & change_mask->tabs[tab_index].leader);
				}
			}
		}

		old_style.procs.alter_par(pg, &old_style, &new_style);

		return	pgAddParInfo(pg, NULL, internal_clone_reason, &new_style);
	}

	return	style->style_item;	/* no change if it gets here */
}

/* get_par_info is the lower level function for changing pars. */

static long get_par_info (pg_ref pg, const select_pair_ptr selection,
		short set_any_match, par_info_ptr info, par_info_ptr mask,
		pg_boolean tables, pg_boolean non_tables)
{
	paige_rec_ptr				pg_rec;
	memory_ref					select_list;
	register pg_short_t			num_selects;
	register select_pair_ptr	select_run;
	change_info					stuff_to_report;
	long						first_select;

	pg_rec = (paige_rec_ptr) UseMemory(pg);

	if (set_any_match)
		pgFillBlock(mask, sizeof(par_info), 0);
	else {
	
		pgFillBlock(info, sizeof(par_info), 0);
		pgFillBlock(mask, sizeof(par_info), -1);
	}
	
	select_list = pgSetupOffsetRun(pg_rec, selection, TRUE, FALSE);

	pgFillBlock(&stuff_to_report, sizeof(change_info), 0);
	
	stuff_to_report.any_match = set_any_match;

	stuff_to_report.pg = pg_rec;
	stuff_to_report.par_change = info;
	stuff_to_report.par_mask = mask;
	stuff_to_report.base = UseMemory(pg_rec->par_formats);
	stuff_to_report.tables = tables;
	stuff_to_report.non_tables = non_tables;

	num_selects = (pg_short_t)GetMemorySize(select_list);
	select_run = (select_pair_ptr) UseMemory(select_list);
	first_select = select_run->begin;

	while (num_selects) {

		pgReturnStyleInfo(pg_rec->par_style_run, &stuff_to_report, get_par_info_proc,
				select_run->begin, select_run->end);

		++select_run;
		--num_selects;
	}
	
	UnuseMemory(pg_rec->par_formats);
	UnuseAndDispose(select_list);

	UnuseMemory(pg);
	
	return	first_select;
}

/* set_par_info is the lower-level function to change par styles tables, non-tables or both. */

static void set_par_info (pg_ref pg, const select_pair_ptr selection,
		const par_info_ptr info, const par_info_ptr mask, pg_boolean tables,
		pg_boolean non_tables, short draw_mode)
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
	stuff_to_change.par_change = info;
	stuff_to_change.par_mask = mask;
	stuff_to_change.tables = tables;
	stuff_to_change.non_tables = non_tables;
	
	num_selects = (pg_short_t)GetMemorySize(select_list);
	select_run = (select_pair_ptr) UseMemory(select_list);
	
	stuff_to_change.change_range = *select_run;
	
	while (num_selects) {
		
		if (non_tables) {
			select_pair		offsets;
			
			if (pgIsTable(pg, select_run->begin)) {

				pgTableOffsets(pg, select_run->begin, &offsets);
				
				if (offsets.begin < select_run->begin)
					select_run->begin = offsets.begin;
			}
			
			if (pgIsTable(pg, select_run->end)) {

				pgTableOffsets(pg, select_run->end, &offsets);
				
				if (offsets.end > select_run->end)
					select_run->end = offsets.end;
			}
		}

		pgChangeStyleRun(pg_rec->par_style_run, &stuff_to_change, change_par_info,
				select_run->begin, select_run->end);
		
		if (select_run->begin < stuff_to_change.change_range.begin)
			stuff_to_change.change_range.begin = select_run->begin;
		if (select_run->end > stuff_to_change.change_range.end)
			stuff_to_change.change_range.end = select_run->end;

		++select_run;
		--num_selects;
	}

	pgFinishStyleUpdate(pg_rec, &stuff_to_change, select_list, TRUE, draw_mode);
	
	UnuseMemory(pg);
}

