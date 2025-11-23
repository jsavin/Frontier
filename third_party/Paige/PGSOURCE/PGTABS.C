/* Paige file exclusively for TAB functions. The resulting library can be omitted
if your app uses no specific tab functions in Paige.h. If you include this file
you must also include paragraph style lib(s). */

#include "Paige.h"

#ifdef MAC_PLATFORM
#pragma segment pgtabs
#endif

#include "pgDefStl.h"
#include "pgPar.h"
#include "pgDefPar.h"
#include "pgUtils.h"


static void set_or_change_tabs (pg_ref pg, select_pair_ptr selection, tab_stop_ptr tab_ptr,
		long PG_FAR *mask, pg_short_t tab_qty, short draw_mode);
static pg_short_t change_tab_info (change_info_ptr change, style_run_ptr style);
static pg_boolean set_new_tab (paige_rec_ptr pg, par_info_ptr par, tab_stop_ptr tab,
		long PG_FAR *tab_mask);
static pg_short_t find_existing_tab (tab_stop_ptr tabs_list, pg_short_t tab_qty, tab_stop_ptr tab);
static void remove_tab (par_info_ptr par, pg_short_t element);


/* pgSetTabBase sets the relative tab base (default = 0)  */

PG_PASCAL (void) pgSetTabBase (pg_ref pg, long tab_base)
{
	paige_rec_ptr		pg_rec;
	
	pg_rec = (paige_rec_ptr) UseMemory(pg);
	pg_rec->tab_base = tab_base;
	
	UnuseMemory(pg);
}


/* pgGetTabBase returns the Paige tab base  */

PG_PASCAL (long) pgGetTabBase (pg_ref pg)
{
	long			result;
	
	paige_rec_ptr		pg_rec;
	pg_rec = (paige_rec_ptr) UseMemory(pg);
	result = pg_rec->tab_base;
	UnuseMemory(pg);
	
	return	result;
}



/* pgSetTab inserts a new tab record if it does not exist, or copies over an
existing tab if the positions match exactly.  The specified selection is changed
and re-drawn if draw_mode != 0.   */

PG_PASCAL (void) pgSetTab (pg_ref pg, const select_pair_ptr selection, const tab_stop_ptr tab_ptr,
		short draw_mode)
{
	set_or_change_tabs(pg, selection, tab_ptr, NULL, 1, draw_mode);
}



/* pgGetTabList fills in a tab_ref (which caller creates) of all unique tabs
in the specified selection. The tab_mask gets filled in with 1's (if non-NULL)
if the corresponding tab is consistent throughout the selection. Note that it is
possible to have tabs set to zero size (meaning no tabs exist at all).   */

PG_PASCAL (void) pgGetTabList (pg_ref pg, const select_pair_ptr selection, tab_ref tabs,
		memory_ref tab_mask, long PG_FAR *screen_offset)
{
	paige_rec_ptr			pg_rec;
	par_info_ptr			par_base, par;
	tab_stop_ptr			source_tab_list, target_tab_list;
	long PG_FAR				*target_mask;
	style_run_ptr			run;
	memory_ref				selections;
	select_pair_ptr			select_ptr;
	long					select_qty, added_pars;
	pg_short_t				source_qty, target_qty, run_index, element;

	SetMemoryRecSize(tabs, sizeof(tab_stop), 0);
	
	if (tab_mask)
		SetMemoryRecSize(tab_mask, sizeof(long), 0);

	target_qty = 0;

	pg_rec = (paige_rec_ptr) UseMemory(pg);

	selections = pgSetupOffsetRun(pg_rec, selection, TRUE, FALSE);
	select_qty = GetMemorySize(selections);
	select_ptr = (select_pair_ptr) UseMemory(selections);

	if (screen_offset) {
		rectangle			bounds;
		
		pgGetScreenOffsets(pg_rec, select_ptr->begin, pg_rec->tab_base, &bounds);
		*screen_offset = bounds.top_left.h;
	}

	par_base = (par_info_ptr) UseMemory(pg_rec->par_formats);

	target_tab_list = (tab_stop_ptr) UseMemory(tabs);
	
	if (tab_mask)
		target_mask = (long *) UseMemory(tab_mask);

	added_pars = 0;

	while (select_qty) {
		
		run = pgFindParStyleRun(pg_rec, select_ptr->begin, NULL);
		
		for (;;) {
			
			run_index = run->style_item;
			par = par_base;
			par += run_index;

			source_tab_list = par->tabs;
			
			if (par->table.table_columns)
				source_qty = 0;
			else
				source_qty = par->num_tabs;
			
			if (source_qty)
				++added_pars;

			while (source_qty) {

				element = find_existing_tab(target_tab_list, target_qty, source_tab_list);
				
				if (element == target_qty) {
					
					++target_qty;
					
					UnuseMemory(tabs);
					SetMemorySize(tabs, target_qty);
					target_tab_list = (tab_stop_ptr) UseMemory(tabs);
					target_tab_list[element] = *source_tab_list;

					if (tab_mask) {

						UnuseMemory(tab_mask);
						SetMemorySize(tab_mask, target_qty);
						target_mask = (long *) UseMemory(tab_mask);
						target_mask[element] = 1;
					}
				}
				else
				if (tab_mask) {
					
					*target_mask += 1;

					if (source_tab_list->tab_type != target_tab_list[element].tab_type)
						*target_mask = 0;
					if (source_tab_list->leader != target_tab_list[element].leader)
						*target_mask = 0;
					if (source_tab_list->ref_con != target_tab_list[element].ref_con)
						*target_mask = 0;
				}
				
				++source_tab_list;
				--source_qty;
			}

			++run;
			
			if (run->offset >= select_ptr->end)
				break;
		}

		UnuseMemory(pg_rec->par_style_run);

		++select_ptr;
		--select_qty;
	}

	UnuseMemory(tabs);

	if (tab_mask) {
		// Set the mask->position fields to reflect the consistency:
		
		while (target_qty) {
			
			if (*target_mask < added_pars)
				*target_mask = 0;
			else
				*target_mask = -1;
			
			++target_mask;
			--target_qty;
		}

		UnuseMemory(tab_mask);
	}

	UnuseMemory(pg_rec->par_formats);
	UnuseAndDispose(selections);
	
	UnuseMemory(pg);
}



/* pgSetTabList applies all tabs in the tab_ref to the specified selection. For
each tab, if the same position already exists for a tab it is replaced, otherwise
the new tab is inserted.  Also, if a tab type is "no_tab," corresponding tabs
with that position are deleted.  */

PG_PASCAL (void) pgSetTabList (pg_ref pg, const select_pair_ptr selection, tab_ref tabs,
		memory_ref tab_mask, short draw_mode)
{
	tab_stop_ptr			tab_list;
	long PG_FAR				*mask_list;
	pg_short_t				qty;
	
	tab_list = (tab_stop_ptr) UseMemory(tabs);
	
	if (tab_mask)
		mask_list = (long *) UseMemory(tab_mask);
	else
		mask_list = NULL;

	qty = (pg_short_t)(GetByteSize(tabs) / sizeof(tab_stop));
	
	if (qty > TAB_ARRAY_SIZE)
		qty = TAB_ARRAY_SIZE;

	set_or_change_tabs(pg, selection, tab_list, mask_list, qty, draw_mode);

	UnuseMemory(tabs);
	
	if (tab_mask)
		UnuseMemory(tab_mask);
}


/****************************  Local Functions  ***************************/


/* set_or_change_tabs sets one or more tabs in the specified selection. This gets called by
both pgSetTab and pgSetTabList. */

static void set_or_change_tabs (pg_ref pg, select_pair_ptr selection, tab_stop_ptr tab_ptr,
		long PG_FAR *mask, pg_short_t tab_qty, short draw_mode)
{
	paige_rec_ptr				pg_rec;
	memory_ref					select_list;
	register pg_short_t			num_selects;
	register select_pair_ptr	select_run;
	par_info					temp_par;
	change_info					stuff_to_change;

	if (!tab_qty)
		return;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	pgFillBlock(&temp_par, sizeof(par_info), 0);
	pgBlockMove(tab_ptr, temp_par.tabs, sizeof(tab_stop) * tab_qty);
	temp_par.num_tabs = tab_qty;

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
	stuff_to_change.tab_change = temp_par.tabs;
	stuff_to_change.tab_mask = mask;
	stuff_to_change.num_tabs = tab_qty;

	num_selects = (pg_short_t)GetMemorySize(select_list);
	select_run = (select_pair_ptr) UseMemory(select_list);
	
	stuff_to_change.change_range = *select_run;

	while (num_selects) {

		pgChangeStyleRun(pg_rec->par_style_run, &stuff_to_change, change_tab_info,
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



/* This changes paragraph TAB information (similar to change_style_info and change_par_info only
in this case we are only altering tabs)  */

static pg_short_t change_tab_info (change_info_ptr change, style_run_ptr style)
{
	paige_rec_ptr				pg;
	par_info					par_style, old_style;
	long PG_FAR					*tab_mask;
	pg_short_t					tab_ctr;
	pg_boolean					changes_made;

	pg = change->pg;
	
	GetMemoryRecord(pg->par_formats, style->style_item, &par_style);
	
	if (par_style.table.table_columns)
		return	style->style_item;

	old_style = par_style;
	tab_mask = change->tab_mask;

	for (changes_made = FALSE, tab_ctr = 0; tab_ctr < (pg_short_t)change->num_tabs; ++tab_ctr) {
	
		if (set_new_tab(pg, &par_style, &change->tab_change[tab_ctr], tab_mask)) {
	
			++change->changed;
			changes_made = TRUE;
		}
		
		if (tab_mask)
			++tab_mask;
	}

	if (changes_made) {

		old_style.procs.alter_par(pg, &old_style, &par_style);
		return	pgAddParInfo(pg, NULL, internal_clone_reason, &par_style);
	}

	return	style->style_item;	/* no change if it gets here */
}


/* set_new_tab places a new tab stop into "par" and returns TRUE if it is now unique.
Otherwise, if the exact same tab already exists, FALSE is returned and par_info has
not changed. */

static pg_boolean set_new_tab (paige_rec_ptr pg, par_info_ptr par, tab_stop_ptr tab,
		long PG_FAR *tab_mask)
{
	tab_stop			old_tab;
	pg_short_t			element, old_qty;
	
	if ((old_qty = par->num_tabs) == TAB_ARRAY_SIZE)
		return	FALSE;
	
	if (tab_mask)
		if (*tab_mask == 0)
			return	FALSE;

	element = find_existing_tab(par->tabs, old_qty, tab);

	if (element == par->num_tabs) {
		
		if (tab->tab_type == no_tab)
			return	FALSE;

		par->num_tabs = element + 1;
		par->tabs[element] = *tab;

		return	TRUE;
	}

	old_tab = par->tabs[element];
	par->tabs[element] = *tab;

	if (par->tabs[element].tab_type == no_tab) {
		
		remove_tab(par, element);
		
		return	TRUE;
	}

	return	(!pgEqualStruct(&old_tab, &par->tabs[element], sizeof(tab_stop)));
}



/* find_existing_tab returns the tab element in par (zero-based) that matches tab->position
exactly. If no such tab exists, then par->num_tabs is returned. Note that par->num_tabs
element will point to a "no tab" record content. */

static pg_short_t find_existing_tab (tab_stop_ptr tabs_list, pg_short_t tab_qty, tab_stop_ptr tab)
{
	register tab_stop_ptr		tabs;
	register pg_short_t			tab_ctr;
	register long				position;

	if (tab_qty == 0)
		return	0;

	tabs = tabs_list;
	position = tab->position;

	for (tab_ctr = 0; tab_ctr < tab_qty; ++tab_ctr)
		if (tabs[tab_ctr].position == position)
			return	tab_ctr;
	
	return	tab_qty;
}


/* remove_tab deletes the tab at element in the tab array, and ->num_tabs is decremented. */

static void remove_tab (par_info_ptr par, pg_short_t element)
{
	pg_short_t		ending_element, ending_qty;
	
	ending_element = par->num_tabs - 1;

	if (ending_qty = (ending_element - element))
		pgBlockMove(&par->tabs[element + 1], &par->tabs[element], ending_qty * sizeof(tab_stop));
	
	pgFillBlock(&par->tabs[ending_element], sizeof(tab_stop), 0);
	
	par->num_tabs = ending_element;	
}
