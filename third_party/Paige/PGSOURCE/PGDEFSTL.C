/* This file handles most actions for styles (including fonts and paragraph
formats) */

#include "Paige.h"

#ifdef MAC_PLATFORM
#pragma segment pgbasic3
#endif

#include "pgDefStl.h"
#include "pgOSUtl.h"
#include "pgText.h"
#include "pgEmbed.h"
#include "machine.h"
#include "defprocs.h"
#include "pgUtils.h"
#include "pgEdit.h"
#include "pgSelect.h"
#include "pgDefPar.h"
#include "pgPar.h"
#include "pgShapes.h"
#include "pgSubref.h"
#include "pgHText.h"


static void setup_walk_styles (style_walk_ptr walker);
static void convert_to_real_par_offsets (paige_rec_ptr pg, memory_ref selection);
static void change_used_ctr (change_info_ptr change, pg_short_t old_item, pg_short_t new_item);
static pg_short_t find_bad_style (paige_rec_ptr pg, style_ref ref,
		pg_short_t PG_FAR *bad_index);
static void compare_long_and_set (long PG_FAR *src_fld, long PG_FAR *target_fld,
		long PG_FAR *mask_fld, short num_flds, pg_boolean positive_match,
		pg_boolean bitwise_compare);
static void compare_short_and_set (short PG_FAR *src_fld, short PG_FAR *target_fld,
		short PG_FAR *mask_fld, short num_flds, pg_boolean positive_match);
static long trailing_text_blanks (paige_rec_ptr pg, size_t begin_select, size_t end_select);
static pg_short_t append_new_style_info (paige_rec_ptr pg, style_info_ptr the_style,
		paige_rec_ptr src_option, short reason_verb);
static void adjust_small_caps_ctr (paige_rec_ptr pg, style_info_ptr mother_style,
		long amt);
static void decrement_small_caps_indexes (paige_rec_ptr pg, pg_short_t bad_index);
static void set_non_zero_long (long PG_FAR *source, long PG_FAR *target);
static void fix_multiple_select_pairs (memory_ref pairs_ref);


/* pgAlterStyleProc gives app a chance to make last-minute changes as the
style is altered. DEFAULT DOES NOTHING. */

PG_PASCAL (void) pgAlterStyleProc (paige_rec_ptr pg, style_info_ptr old_style,
		style_info_ptr new_style, style_info_ptr style_mask)
{
#ifdef MAC_PLATFORM
#pragma unused (pg, old_style, new_style, style_mask)
#endif

}


/* pgActivateStyleProc gets called for activate/deactivate of highlight if
class_bits has ACTIVATE_ENABLE_BIT is set. */

PG_PASCAL (void) pgActivateStyleProc (paige_rec_ptr pg, style_info_ptr style,
		select_pair_ptr text_range, pg_char_ptr text, short front_back_state,
		short perm_state, pg_boolean show_hilite)
{
#ifdef MAC_PLATFORM
#pragma unused (pg, style, text_range, text, front_back_state, perm_state, show_hilite)
#endif
}


/* pgCopyTextProc is the default copy_text proc, but does nothing by default. */

PG_PASCAL (void) pgCopyTextProc (paige_rec_ptr src_pg, paige_rec_ptr target_pg,
		short reason_verb, style_info_ptr style, size_t style_position,
		size_t text_position, pg_char_ptr text, size_t length)
{
#ifdef MAC_PLATFORM
#pragma unused (src_pg, target_pg, reason_verb, style, style_position, text_position, text, length)
#endif
}

/* pgDeleteTextProc is the default delete_text proc, but does nothing by default. */

PG_PASCAL (void) pgDeleteTextProc (paige_rec_ptr pg, short reason_verb,
		style_info_ptr style, long style_position, long text_position,
		pg_char_ptr text, long length)
{
#ifdef MAC_PLATFORM
#pragma unused (pg, reason_verb, style, style_position, text_position, text, length)
#endif
}

/* pgSetupInsertProc is the default insert_proc. The default function checks for
text that would be invisible clears the invisibility. For version 2/11/94 this
occurs only if style is hidden text and pg flags have hide text set.  */

PG_PASCAL (pg_boolean) pgSetupInsertProc (paige_rec_ptr pg, style_info_ptr style,
		long position)
{
#ifdef MAC_PLATFORM
#pragma unused (position)
#endif

	if (pg->flags & NO_HIDDEN_TEXT_BIT)
		if (style->styles[hidden_text_var]) {
			
			style->styles[hidden_text_var] = 0;
			
			return	TRUE;
		}

	return	FALSE;
}


/* pgFindTextStyle returns the style_info record (pointer) based on a given
offset. Note you can get the same thing with two calls: pgFindStyleRun and
then pgIndexToStyle. */

PG_PASCAL (style_info_ptr) pgFindTextStyle (paige_rec_ptr pg_rec, size_t offset)
{
	style_run_ptr		run_info;
	pg_short_t			index;
	
	run_info = pgFindStyleRun(pg_rec, offset, NULL);
	index = run_info->style_item;
	UnuseMemory(pg_rec->t_style_run);
	return (style_info_ptr) UseMemoryRecord(pg_rec->t_formats, index, USE_ALL_RECS, TRUE);
}


/* pgLocateStyleSheet searches the list of styles and returns style_id record (or
NULL if not found). If style_index is non-null then it is filled with the element of the style. */

PG_PASCAL (style_info_ptr) pgLocateStyleSheet (paige_rec_ptr pg, short style_id,
		pg_short_t PG_FAR *style_index)
{
	register style_info_ptr		styles;
	register pg_short_t			style_ctr, style_qty;
	
	if (!style_id)
		return	NULL;

	styles = (style_info_ptr) UseMemory(pg->t_formats);
	style_qty = (pg_short_t)GetMemorySize(pg->t_formats);
	
	for (style_ctr = 0; style_ctr < style_qty; ++styles, ++style_ctr)
		if (styles->style_sheet_id == style_id) {
			
			if (style_index)
				*style_index = style_ctr;

			return	styles;
	}
	
	UnuseMemory(pg->t_formats);

	return	NULL;
}


/* pgFindStyleRun returns the style_run (pointer) based on a text offset. */

PG_PASCAL (style_run_ptr) pgFindStyleRun (paige_rec_ptr pg_rec, size_t offset,
		size_t PG_FAR *rec_num)
{
	return	pgFindRunFromRef(pg_rec->t_style_run, offset, rec_num);
}


/* pgFindParStyleRun returns the par style_run (pointer) based on a text offset. */

PG_PASCAL (style_run_ptr) pgFindParStyleRun (paige_rec_ptr pg_rec, size_t offset,
		size_t PG_FAR *rec_num)
{
	return	pgFindRunFromRef(pg_rec->par_style_run, offset, rec_num);
}



/* pgFindParStyle returns the par_info record (pointer) based on a given
offset. Note you can get the same thing with two calls: pgFindParStyleRun and
then pgIndexToParStyle. */

PG_PASCAL (par_info_ptr) pgFindParStyle (paige_rec_ptr pg_rec, size_t offset)
{
	style_run_ptr		run_info;
	pg_short_t			index;
	
	run_info = pgFindParStyleRun(pg_rec, offset, NULL);
	index = run_info->style_item;
	UnuseMemory(pg_rec->par_style_run);
	return (par_info_ptr) UseMemoryRecord(pg_rec->par_formats, index, USE_ALL_RECS, TRUE);
}



/* pgInitStyleMask fills a style_info record with a single char.  */

PG_PASCAL (void) pgInitStyleMask (style_info_ptr mask, short filler)
{
	pgFillBlock(mask, sizeof(style_info), (char)filler);
}


/* pgInitStyleMask fills a par_info record with a single char.  */

PG_PASCAL (void) pgInitParMask (par_info_ptr mask, short filler)
{
	pgFillBlock(mask, sizeof(par_info), (char)filler);
}


/* pgInitFontMask fills a font_info record with a single char.  */

PG_PASCAL (void) pgInitFontMask (font_info_ptr mask, short filler)
{
	pgFillBlock(mask, sizeof(font_info), (char)filler);
}



/* pgSetInsertionStyles inserts a style, and/or font for the next text insertion.
Note this must only get called for single insertions and not to an applied range.
The current insertion is used.  Either style or font can be NULL in which case
it is not set.  */

PG_PASCAL (void) pgSetInsertionStyles (pg_ref pg, const style_info_ptr style, const font_info_ptr font)
{
	paige_rec_ptr			pg_rec;
	style_info				copy_of_style;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	
	if (!style)
		GetMemoryRecord(pg_rec->t_formats, pg_rec->insert_style, &copy_of_style);
	else
		pgBlockMove(style, &copy_of_style, sizeof(style_info));
	
	if (font) {
		font_info			new_font;
		
		pgBlockMove(font, &new_font, sizeof(font_info));
		copy_of_style.font_index = pgAddNewFont(pg_rec, &new_font);
	}
	
	pg_rec->insert_style = pgAddStyleInfo(pg_rec, NULL, for_next_insert_reason, &copy_of_style);

	UnuseMemory(pg);
}


/* pgPrepareStyleWalk sets up a style_walk record so you can start walking
through styles and formats one at a time. The typical use is to advance through
text and to know when there is a style change automatically. Subsequent calls
to "pgWalkStyle" are made with only the walker parameter and the amount you want
to walk the style (plus or minus). The walk function adjusts the items
appropriately. WHEN YOU ARE THROUGH using the walker, call pgPrepareStyleWalk
once more passing NULL as the walker parameter.
Change 3/2594, "include_pars," if TRUE, includes par styles otherwise only
text styles + font. */

PG_PASCAL (void) pgPrepareStyleWalk (paige_rec_ptr pg, size_t offset,
		style_walk_ptr walker, pg_boolean include_pars)
{
	register style_walk_ptr		walk;

	if ((walk = walker) != NULL) {
		
		walk->flags2 = pg->flags2;

		walk->next_style_run = walk->prev_style_run = pgFindStyleRun(pg, offset, NULL);
		++walk->next_style_run;
		
		walk->style_base = (style_info_ptr) UseMemory(pg->t_formats);
		walk->font_base = (font_info_ptr) UseMemory(pg->fonts);

		walk->current_offset = offset;
		walk->last_font = -3;
		walk->t_length = pg->t_length;
		walk->hyperlink = pgFindHypertextRun(pg->hyperlinks, offset, NULL);
		walk->hyperlink_base = (pg_hyperlink_ptr) UseMemoryRecord(pg->hyperlinks, 0, 0, FALSE);
		walk->hyperlink_target = pgFindHypertextRun(pg->target_hyperlinks, offset, NULL);
		walk->hyperlink_target_base = (pg_hyperlink_ptr) UseMemoryRecord(pg->target_hyperlinks, 0, 0, FALSE);

		if (include_pars) {

			walk->next_par_run = walk->prev_par_run = pgFindParStyleRun(pg, offset, NULL);
			++walk->next_par_run;
			walk->par_base = (par_info_ptr) UseMemory(pg->par_formats);
		}
		else
			walk->par_base = NULL;

#ifdef PG_DEBUG
		walk->globals = pg->globals;
		walk->max_styles = GetMemorySize(pg->t_formats);
		walk->max_pars = GetMemorySize(pg->par_formats);
		walk->stylerun = pg->t_style_run;
		walk->parrun = pg->par_style_run;
#endif

		setup_walk_styles(walk);
	}
	else {
		UnuseMemory(pg->t_formats);
		UnuseMemory(pg->fonts);
		UnuseMemory(pg->t_style_run);
		UnuseMemory(pg->hyperlinks);
		UnuseMemory(pg->target_hyperlinks);

		if (include_pars) {
		
			UnuseMemory(pg->par_formats);
			UnuseMemory(pg->par_style_run);
		}
	}
}


/* This function can be called repeatedly after pgPrepareStyleWalk. Pass the amount
you want to advance in amount (plus or minus) and the next/previous styles are
set up.  TRUE is returned if the style(s) changed.    */

PG_PASCAL (pg_boolean) pgWalkStyle (style_walk_ptr walker, size_t amount)
{
	register style_walk_ptr		walk;
	register size_t				new_position;
	pg_boolean					result;

	walk = walker;
	result = FALSE;

	if ((walk->current_offset += amount) < 0)
		walk->current_offset = 0;

	new_position = walk->current_offset;

	while (new_position >= walk->next_style_run->offset) {
		
		walk->prev_style_run = walk->next_style_run;
		++walk->next_style_run;

		result = TRUE;
	}

	while (new_position < walk->prev_style_run->offset) {
		
		walk->next_style_run = walk->prev_style_run;
		--walk->prev_style_run;

		result = TRUE;
	}
	
	if (new_position >= walk->hyperlink->applied_range.end
		|| new_position < walk->hyperlink->applied_range.begin) {
		
		walk->hyperlink = walk->hyperlink_base;
		
		while (walk->hyperlink->applied_range.end <= new_position)
			++walk->hyperlink;
	}

	if (new_position >= walk->hyperlink_target->applied_range.end
		|| new_position < walk->hyperlink_target->applied_range.begin) {
		
		walk->hyperlink_target = walk->hyperlink_target_base;
		
		while (walk->hyperlink_target->applied_range.end <= new_position)
			++walk->hyperlink_target;
	}

	result |= (pg_boolean)(new_position <= walk->hyperlink->applied_range.begin
				&& new_position < walk->hyperlink->applied_range.end);
	result |= (pg_boolean)(new_position <= walk->hyperlink_target->applied_range.begin
				&& new_position < walk->hyperlink_target->applied_range.end);

	if (walk->par_base) {
	
		while (new_position >= walk->next_par_run->offset) {
			
			walk->prev_par_run = walk->next_par_run;
			++walk->next_par_run;
	
			result = TRUE;
		}
	
		while (new_position < walk->prev_par_run->offset) {
			
			walk->next_par_run = walk->prev_par_run;
			--walk->prev_par_run;
	
			result = TRUE;
		}
	}

	if (result)
		setup_walk_styles(walk);
	
	return	result;
}


/* pgWalkNextStyle advances to the next style and returns TRUE or, if there are
no additional styles, FALSE is returned and nothing advances.  */

PG_PASCAL (pg_boolean) pgWalkNextStyle (style_walk_ptr walker)
{
	register long		next_run_offset;

	next_run_offset = walker->next_style_run->offset;

	if (next_run_offset > walker->t_length)
		return	FALSE;
	
	pgWalkStyle(walker, next_run_offset - walker->current_offset);
	
	return	TRUE;
}


/* pgWalkPreviousStyle sets up the previous style if there is one. */

PG_PASCAL (pg_boolean) pgWalkPreviousStyle (style_walk_ptr walker)
{
	long			back_offset;
	
	if (back_offset = walker->prev_style_run->offset)
		return	pgSetWalkStyle(walker, back_offset - 1);
	
	return	FALSE;
}


/* pgSetWalkStyle is the same as pgWalkStyle except an absolute position is
set (instead of increment or decrement from the current position). It returns
TRUE if the new style is different than the previous.  */

PG_PASCAL (pg_boolean) pgSetWalkStyle (style_walk_ptr walker, size_t position)
{
	if (walker->current_offset == position)
		return	FALSE;

	walker->current_offset = position;
	return pgWalkStyle(walker, 0);
}


/* pgSetNextInsertIndex places the style index item at the insert_style
positions.  This does nothing, however, if the selection is not a single insertion. */

PG_PASCAL (void) pgSetNextInsertIndex (paige_rec_ptr pg_rec)
{
	if (!pg_rec->num_selects) {

		pg_rec->insert_style = pgGetInsertionStyle(pg_rec);
		pgSetupInsertStyle(pg_rec);
	}
}


/* pgSetupInsertStyle gets called internally by Paige to invoke the insert_proc
hook in the current keyboard insertion style.  */

PG_PASCAL (void) pgSetupInsertStyle(paige_rec_ptr pg_rec)
{
	style_info			copy_of_style;
	font_info_ptr		insert_font;
	short				font_index;

	GetMemoryRecord(pg_rec->t_formats, pg_rec->insert_style, &copy_of_style);

	if (copy_of_style.procs.insert_proc(pg_rec, &copy_of_style, pgCurrentInsertion(pg_rec))) {

		if ((font_index = copy_of_style.font_index) == DEFAULT_FONT_INDEX)
			font_index = 0;

		insert_font = (font_info_ptr) UseMemoryRecord(pg_rec->fonts, font_index, 0, TRUE);
		copy_of_style.procs.init(pg_rec, &copy_of_style, insert_font);
		UnuseMemory(pg_rec->fonts);
		
		pg_rec->insert_style = pgAddStyleInfo(pg_rec, NULL, for_next_insert_reason, &copy_of_style);
	}
}



/* pgGetInsertionStyle returns the style item at the selection's beginning. */

PG_PASCAL (pg_short_t) pgGetInsertionStyle (paige_rec_ptr pg_rec)
{
	t_select		selection;
	style_run_ptr	style;
	pg_short_t		item;

	GetMemoryRecord(pg_rec->select, 0, &selection);
	
	if (selection.offset)
		if (!pg_rec->procs.boundary_proc(pg_rec, NULL))
			--selection.offset;

	style = pgFindStyleRun(pg_rec, selection.offset, NULL);
	item = style->style_item;
	UnuseMemory(pg_rec->t_style_run);
	
	return	item;
}



/* pgCleanupStyleRuns cleans up the text and paragraph styles (typically called
after a deletion or massive insertion). Style run is cleaned up if style_changed
is TRUE, and same for paragraph runs and info runs if par_changed = TRUE. */

PG_PASCAL (void) pgCleanupStyleRuns (paige_rec_ptr pg, pg_boolean style_changed,
		pg_boolean par_changed)
{
	change_info			cleanup_info;
	
	pgFillBlock(&cleanup_info, sizeof(change_info), -1);

	cleanup_info.pg = pg;
	
	if (!style_changed)
		cleanup_info.style_change = NULL;
	if (!par_changed) {
	
		cleanup_info.par_change = NULL;
		cleanup_info.tab_change = NULL;
	}
	
	pgFixAllStyleRuns(pg, &cleanup_info);
}


/* pgSetupOffsetRun sets up a run of selections based on selection if non-NULL or
the current selection range. It returns a memory_ref of longs (offsets). Or, if
the selection is a single insertion AND that point is the current insertion
of pg AND for_paragraph = FALSE, the function returns NULL.
Added 3/25/94 "smart_select" to remove trailing blanks if TRUE and selection
was word selection. */

PG_PASCAL (memory_ref) pgSetupOffsetRun (paige_rec_ptr pg, select_pair_ptr selection,
			pg_boolean for_paragraph, pg_boolean smart_select)
{
	memory_ref		result;
	select_pair_ptr	run;
	t_select_ptr	selections;
	pg_short_t		num_selects;
	
	if (selection) {
	
		if (selection->begin > pg->t_length)
			selection->begin = pg->t_length;
		if (selection->end > pg->t_length)
			selection->end = pg->t_length;
    }
    
	if ((!pg->num_selects) && (!for_paragraph)) {
		
		if (!selection)
			return	MEM_NULL;
		
		if (selection->begin == selection->end)
			return	MEM_NULL;
	}

	result = MemoryAlloc(pg->globals->mem_globals, sizeof(select_pair), (long)1,
			(short)(pg->num_selects * 2));

	if (selection)
		pgBlockMove(selection, UseMemory(result), sizeof(select_pair));
	else {
		
		if (!(num_selects = pg->num_selects)) {
			
			run = (select_pair_ptr) UseMemory(result);
			selections = (t_select_ptr) UseMemory(pg->select);
			run->begin = run->end = selections->offset;
			UnuseMemory(pg->select);
		}
		else {
			
			selections = (t_select_ptr) UseMemory(pg->select);

			if (selections->flags & VERTICAL_FLAG) {
				shape_ref		rgn;
				
				rgn = pgRectToShape(pg->globals->mem_globals, NULL);
				pg->procs.hilite_rgn(pg, selections, num_selects, rgn);
				pgShapeToSelections(pg->myself, rgn, result);
				pgDisposeShape(rgn);
				UseMemory(result);		/* To satisfy the balance at the end */
			}
			else {

				SetMemorySize(result, num_selects);
				
				for (run = (select_pair_ptr) UseMemory(result); num_selects; ++run, --num_selects) {
					
					run->begin = selections[0].offset;
					run->end = selections[1].offset;
					
					if (run->end < run->begin) {
					
						run->end = selections[0].offset;
						run->begin = selections[1].offset;
					}
						
					selections += 2;
					
					if (smart_select && (selections->flags & WORD_FLAG))
						if (!(pg->flags & NO_SMART_CUT_BIT))
							run->end -= trailing_text_blanks(pg, run->begin, run->end);
				}
			}

			UnuseMemory(pg->select);
		}
	}
	
	UnuseMemory(result);
	
	num_selects = (pg_short_t)GetMemorySize(result);

	if (for_paragraph) {
		
		if (num_selects == 0) {
			
			SetMemorySize(result, 1);
			run = (select_pair_ptr) UseMemory(result);
			selections = (t_select_ptr) UseMemory(pg->select);
			run->begin = run->end = selections->offset;
			
			UnuseMemory(result);
			UnuseMemory(pg->select);
		}
		
		convert_to_real_par_offsets(pg, result);
	}
	else
	if (num_selects == 0) {
		
		DisposeMemory(result);
		
		return	MEM_NULL;
	}

	fix_multiple_select_pairs(result);

	return	result;
}



/* pgFindMatchingStyle is a generic function that can be used to find a matching style_info,
par_info or font, etc.  Pass the memory_ref of the array in ref_to_use, the
record struct to match against in rec_to_match.  The match_size is the size to
compare.  The result is the matching record number + 1 (or zero if none).
The start_rec parameter indicates the first record to look for, first rec is 0. */

PG_PASCAL (pg_short_t) pgFindMatchingStyle (memory_ref ref_to_use, void PG_FAR *rec_to_match,
		pg_short_t start_rec, pg_short_t match_size)
{
	register pg_short_t				rec_size, ctr;
	register size_t					num_recs;
	register pg_bits8_ptr			rec_ptr;
	long							first_rec;

	first_rec = start_rec;
	rec_size = GetMemoryRecSize(ref_to_use);
	num_recs = GetMemorySize(ref_to_use) - first_rec;
	
	if (num_recs <= 0)
		return	0;
	
	if (match_size == SIGNIFICANT_STYLE_SIZE) {
		style_info_ptr			style_ptr;
		
		style_ptr = (style_info_ptr)rec_to_match;
		
		if (!(style_ptr->maintenance & IS_STYLE_SHEET))
			if (style_ptr->style_sheet_id > 0)
				style_ptr->style_sheet_id = -style_ptr->style_sheet_id;
	}
	else {
		par_info_ptr			par_ptr;
		
		par_ptr = (par_info_ptr)rec_to_match;
		
		if (!(par_ptr->maintenance & IS_STYLE_SHEET))
			if (par_ptr->style_sheet_id > 0)
				par_ptr->style_sheet_id = -par_ptr->style_sheet_id;
	}

	rec_ptr = (pg_bits8_ptr) UseMemoryRecord(ref_to_use, first_rec, USE_ALL_RECS, TRUE);

	for (ctr = start_rec + 1; num_recs; ++ctr, rec_ptr += rec_size, --num_recs)
		if (pgEqualStruct(rec_ptr, rec_to_match, (long)match_size)) {

			UnuseMemory(ref_to_use);
			return	ctr;
		}
	
	UnuseMemory(ref_to_use);

	return	0;
}

/* pgGetPadStyles returns the text and paragraph style items for pad_offset all
for the ending format that should exist following a deletion. When this is
called, the text and par format's used_ctr is incremented (so they do not delete)
if increment_used is TRUE.
The purpose of this is to save off what should be the ending format following
a text deletion;  once a deletion occurs, pgSetPadStyles is called. 
The two must be balanced if increment_used was TRUE.  */

PG_PASCAL (void) pgGetPadStyles (paige_rec_ptr pg, long pad_offset,
		pg_short_t PG_FAR *style_item, pg_short_t PG_FAR *par_item,
		pg_boolean increment_used)
{
	register style_run_ptr		run;
	style_info_ptr				style;
	par_info_ptr				par_style;

	run = pgFindStyleRun(pg, pad_offset, NULL);
	*style_item = run->style_item;
	
	if (increment_used) {
	
		style = (style_info_ptr) UseMemoryRecord(pg->t_formats, *style_item, 0, TRUE);
		++style->used_ctr;
		adjust_small_caps_ctr(pg, style, 1);

		UnuseMemory(pg->t_formats);
	}

	UnuseMemory(pg->t_style_run);
	
	if (par_item) {

		//pgFindPar(pg->myself, pad_offset, &unwanted_offset, &par_offset);
		//run = pgFindParStyleRun(pg, par_offset, NULL);

		run = pgFindParStyleRun(pg, pad_offset, NULL);

		*par_item = run->style_item;
		
		if (increment_used) {
		
			par_style = (par_info_ptr) UseMemoryRecord(pg->par_formats, *par_item, 0, TRUE);
			++par_style->used_ctr;
			UnuseMemory(pg->par_formats);
		}
		
		UnuseMemory(pg->par_style_run);
	}
}


/* pgSetPadStyles is the counterpart to pgGetPadStyles. This inserts the
appopriate styles to the pad_style_offset and par_style_offset values. It
gets called after a text deletion and must have been balanced with
pgGetPadStyles above.   */

PG_PASCAL (void) pgSetPadStyles (paige_rec_ptr pg, pg_short_t style_item,
		pg_short_t par_item, long pad_style_offset)
{
	register style_run_ptr		run;
	style_info_ptr				style;
	par_info_ptr				par_style;
	size_t						used_ctr_amt, par_offset;
	size_t						unwanted_offset, run_num;

	run = pgFindStyleRun(pg, pad_style_offset, &run_num);
	used_ctr_amt = 0;
	
	if ((pad_style_offset < pg->t_length) && (run->offset != pad_style_offset)) {
		
		if (pad_style_offset >= run->offset)
			++run_num;

		run = (style_run_ptr) InsertMemory(pg->t_style_run, run_num, 1);
		run->style_item = style_item;
		run->offset = pad_style_offset;
	}
	else
	if (pad_style_offset < pg->t_length)
		run->style_item = style_item;
	else
		used_ctr_amt = -1;
	
	UnuseMemory(pg->t_style_run);

	style = (style_info_ptr) UseMemoryRecord(pg->t_formats, style_item, 0, TRUE);
	style->used_ctr += used_ctr_amt;
	adjust_small_caps_ctr(pg, style, used_ctr_amt);

	UnuseMemory(pg->t_formats);

	pgFindPar(pg->myself, pad_style_offset, &unwanted_offset, &par_offset);

	run = pgFindParStyleRun(pg, par_offset, &run_num);
	used_ctr_amt = 0;
	
	if (run->offset != par_offset) {
//	if ((par_offset < pg->t_length) && (run->offset != par_offset)) {
		
		run = (style_run_ptr) InsertMemory(pg->par_style_run, run_num + 1, 1);
		run->style_item = par_item;
		run->offset = par_offset;
	}
	else
	if (unwanted_offset == pad_style_offset) {

		run = (style_run_ptr) InsertMemory(pg->par_style_run, run_num, 1);
		run->style_item = par_item;
		run->offset = pad_style_offset;
	}
	else
		used_ctr_amt = -1;

	UnuseMemory(pg->par_style_run);

	par_style = (par_info_ptr) UseMemoryRecord(pg->par_formats, par_item, 0, TRUE);
	par_style->used_ctr += used_ctr_amt;
	UnuseMemory(pg->par_formats);
}



/* pgAddNewFont adds a new font (or finds the same font) and returns the index.
Note that the font record must be perfectly in tact. */

PG_PASCAL (short) pgAddNewFont (paige_rec_ptr pg, const font_info_ptr font)
{
	font_info_ptr			fonts;
	pg_short_t				num_fonts, index;
	
	pg->procs.font_proc(pg, font);

	if (!font->name[0] && !font->alternate_name[0])
		return	DEFAULT_FONT_INDEX;

	for (num_fonts = (pg_short_t)GetMemorySize(pg->fonts), fonts = (font_info_ptr) UseMemory(pg->fonts),
			index = 0; num_fonts; ++fonts, ++index, --num_fonts)
		if (pgEqualFontNames(font, fonts, FALSE) && pgEqualFontNames(font, fonts, TRUE))
			break;

	if (!num_fonts)
		pgBlockMove(font, AppendMemory(pg->fonts, 1, FALSE), sizeof(font_info));
	
	UnuseMemory(pg->fonts);
	
	return	index;
}


/* pgIsPlatformFont returns TRUE if the platform fits within runtime graphics
platform. */

PG_PASCAL (pg_boolean) pgIsPlatformFont (long platform)
{
	return	pgFromThisPlatform(platform);
}


/* pgAddStyleInfo adds a new style_info record if none exist or, if one exists,
that record is returned.  In either case, the index (rec num) is returned.  */

PG_PASCAL (pg_short_t) pgAddStyleInfo (paige_rec_ptr pg, paige_rec_ptr src_option,
		short reason_verb, style_info_ptr style)
{
	pg_short_t			found_rec;
	font_info_ptr		the_font;
	short				font_index;
	
	if (!style->procs.init)
		style->procs = pg->globals->def_style.procs;

	if ((font_index = style->font_index) == DEFAULT_FONT_INDEX)
		font_index = 0;
	
	the_font = (font_info_ptr) UseMemoryRecord(pg->fonts, font_index, 0, TRUE);
	style->procs.init(pg, style, the_font);
	UnuseMemory(pg->fonts);

	if (found_rec = pgFindMatchingStyle((memory_ref) pg->t_formats, style,
			0, SIGNIFICANT_STYLE_SIZE))
		return	found_rec - 1;

	if (style->styles[small_caps_var])
		style->small_caps_index = pgAddSmallCapsStyle(pg, src_option, reason_verb, style);

	found_rec = append_new_style_info(pg, style, src_option, reason_verb);

	return	found_rec;
}


/* pgAddParInfo adds a new par_info record if none exist or, if one exists,
that record is returned.  In either case, the index (rec num) is returned.
Fix 4/25/94, bad indents are corrected. */

PG_PASCAL (pg_short_t) pgAddParInfo (paige_rec_ptr pg, paige_rec_ptr src_option,
		short reason_verb, par_info_ptr style)
{
	pg_short_t			found_rec;
	par_info_ptr		appended_ptr;

	if (found_rec = pgFindMatchingStyle((memory_ref) pg->par_formats, style,
			0, SIGNIFICANT_PAR_STYLE_SIZE))
		return	found_rec - 1;

	found_rec = (pg_short_t)GetMemorySize(pg->par_formats);
	appended_ptr = (par_info_ptr) AppendMemory(pg->par_formats, 1, FALSE);
	pgBlockMove(style, appended_ptr, sizeof(par_info));
	appended_ptr->used_ctr = 0;

	if (appended_ptr->indents.left_indent < 0)
		appended_ptr->indents.left_indent = 0;
	if (appended_ptr->indents.right_indent < 0)
		appended_ptr->indents.right_indent = 0;
	if ((appended_ptr->indents.left_indent + appended_ptr->indents.first_indent) < 0)
		appended_ptr->indents.first_indent = -appended_ptr->indents.left_indent;

		appended_ptr->procs.duplicate(src_option, pg, reason_verb, pg->par_formats, appended_ptr);
	
	if (!(appended_ptr->maintenance & IS_STYLE_SHEET)) {
	
		if (appended_ptr->style_sheet_id > 0)
			appended_ptr->style_sheet_id = -appended_ptr->style_sheet_id;	
	}
	else
		++appended_ptr->used_ctr;

	appended_ptr->maintenance &= MAINTENANCE_MASK;

	UnuseMemory(pg->par_formats);
	
	return	found_rec;
}


/* pgSetFldsFromMask sets new_rec with <<short>> fields from fill if the corresponding
<<short>> field in mask is non-zero. The new_rec structure should contain all the
default items, i.e., should be OK if not altered.  The function returns TRUE if
any field has changed from the original value(s) in new_rec.   */

PG_PASCAL (pg_boolean) pgSetFldsFromMask (void PG_FAR *new_rec, void PG_FAR *fill, void PG_FAR *mask,
		pg_short_t rec_size)
{
	register pg_short_t PG_FAR		*target, *src, *mask_items;
	register pg_short_t				size_ctr;
	pg_boolean						changed;
	
	changed = FALSE;
	size_ctr = rec_size / sizeof(short);
	
	for (target = (pg_short_t *) new_rec, src = (pg_short_t *) fill, mask_items = (pg_short_t *) mask;  size_ctr; --size_ctr,
			++target, ++src) {
		
		if (*mask_items++) {
			
			changed |= (*target != *src);
			*target = *src;
		}
	}
	
	return	changed;
}


/* pgSetMaskFromFlds compares each field in rec1 to the corresponding field in
rec2 and sets that field in mask (or not) depending on the comparison, as follows:
if positive_match is FALSE, the mask field is cleared to zero if rec1.field and rec2.field
do not match (otherwise mask is left alone). If positive_match is TRUE, the mask field
is set to 1's if rec1.field matches rec2.field, otherwise the mask is left alone.
The compare_type param will be either style_compare, par_compare or font_compare
if it is positive. If negative it is a general info compare, in this case rec_type
is a negative value of the record size.
ADDED FOR 3.1 -- If exclude_tables is true then table info is not compared. */

PG_PASCAL (void) pgSetMaskFromFlds (void PG_FAR *rec1, void PG_FAR *rec2, void PG_FAR *mask,
		short rec_type, pg_boolean positive_match, pg_boolean exclude_tables)
{
	if (rec_type < 0) {	/* General Info compare */
		short			num_fields;

		num_fields = -rec_type;
		num_fields /= sizeof(long);

		compare_long_and_set((pg_info_ptr) rec1, (pg_info_ptr) rec2,
			(pg_info_ptr) mask, num_fields, positive_match, FALSE);
	}
	else
	if (rec_type == style_compare) {
		register style_info_ptr		src, target, the_mask;
		
		src = (style_info_ptr) rec1;
		target = (style_info_ptr) rec2;
		the_mask = (style_info_ptr) mask;

/* NOTE!!! THE FOLLOWING CODE ASSUMES A CERTAIN SEQUENCE OF FIELDS IN STYLE_INFO.
THIS CODE NEEDS TO BE CHANGED IF FIELDS ARE CHANGED. */

		compare_short_and_set(&src->font_index, &target->font_index,
				&the_mask->font_index, STYLEINFO_SHORTS, positive_match);

		compare_long_and_set(&src->class_bits, &target->class_bits,
				&the_mask->class_bits, STYLEINFO_LONGS, positive_match, FALSE);
		
		/* Compare style function pointers:  */

		compare_long_and_set((long PG_FAR *)&src->procs, (long PG_FAR *)&target->procs, 
				(long PG_FAR *)&the_mask->procs, sizeof(pg_style_hooks) / sizeof(long),
					positive_match, FALSE);
	}
	else
	if (rec_type == par_compare) {
		register par_info_ptr		src, target, the_mask;

		src = (par_info_ptr) rec1;
		target = (par_info_ptr) rec2;
		the_mask = (par_info_ptr) mask;
		
		compare_short_and_set(&src->justification, &target->justification,
				&the_mask->justification, PARINFO_SHORTS, positive_match);
		
		if (!exclude_tables || src->table.table_columns == 0) {
		
			compare_short_and_set(&src->justification, (short PG_FAR *)&target->num_tabs,
					(short PG_FAR *)&the_mask->num_tabs, 1, positive_match);

			compare_long_and_set((long PG_FAR *)src->tabs, (long PG_FAR *)target->tabs,
					(long PG_FAR *)the_mask->tabs, PARINFO_TABLE_LONGS, positive_match, FALSE);
		}
		
		compare_long_and_set(&src->style_sheet_id, &target->style_sheet_id,
				&the_mask->style_sheet_id, PARINFO_NONTABLE_LONGS, positive_match, FALSE);

		compare_long_and_set((long PG_FAR *)&src->procs, (long PG_FAR *)&target->procs, 
				(long PG_FAR *)&the_mask->procs, sizeof(pg_par_hooks) / sizeof(long),
					positive_match, FALSE);

		compare_long_and_set(&src->table.border_info, &target->table.border_info,
				&the_mask->table.border_info, 1, positive_match, TRUE);
		compare_long_and_set(&src->table.cell_borders, &target->table.cell_borders,
				&the_mask->table.cell_borders, 1, positive_match, TRUE);
		compare_long_and_set(&src->table.grid_borders, &target->table.grid_borders,
				&the_mask->table.grid_borders, 1, positive_match, TRUE);
	}
	else {
		register font_info_ptr		src, target, the_mask;

		src = (font_info_ptr) rec1;
		target = (font_info_ptr) rec2;
		the_mask = (font_info_ptr) mask;

		/* Compare font name */
		
		if (positive_match) {

			if (pgEqualFontNames(src, target, FALSE))
				pgFillBlock(the_mask->name, FONT_SIZE * sizeof(pg_char), COMPARE_TRUE);
			if (pgEqualFontNames(src, target, TRUE))
				pgFillBlock(the_mask->alternate_name, FONT_SIZE * sizeof(pg_char), COMPARE_TRUE);
		}
		else {
			if (!pgEqualFontNames(src, target, FALSE))
				pgFillBlock(the_mask->name, FONT_SIZE * sizeof(pg_char), COMPARE_FALSE);
			if (!pgEqualFontNames(src, target, TRUE))
				pgFillBlock(the_mask->alternate_name, FONT_SIZE * sizeof(pg_char), COMPARE_FALSE);
		}

		/* Compare "shorts": */

		compare_short_and_set(&src->environs, &target->environs,
				&the_mask->environs, FONTINFO_SHORTS, positive_match);

		/* Compare "longs": */

		compare_long_and_set(&src->platform, &target->platform,
				&the_mask->platform, FONTINFO_LONGS, positive_match, FALSE);
	}
}


/* pgReturnStyleInfo gets called to walk through a range of styles so "Get"
functions (e.g., pgGetStyleInfo) can report to the app about a run of styles.
The get_proc is called for actual interrogation.  */

PG_PASCAL (void) pgReturnStyleInfo (style_ref ref, change_info_ptr report, style_get_proc get_proc,
		long from_offset, long to_offset)
{
	pg_short_t					rec_ctr;
	register style_run_ptr		run;
	
	run = pgFindRunFromRef (ref, from_offset, NULL);
	rec_ctr = 0;
	
	for (;;) {

		get_proc(report, run, rec_ctr);
		++rec_ctr;
		++run;
		
		if (run->offset >= to_offset)
			break;
	}

	UnuseMemory(ref);
}


/* pgAddSmallCapsStyle builds the required special style for small_caps_var.  This returns
the style index number. */

PG_PASCAL (long) pgAddSmallCapsStyle (paige_rec_ptr pg, paige_rec_ptr src_option,
		short reason_verb, style_info_ptr style)
{
	style_info			small_caps;
	long				mother_point;
	short				point_percent, font_index;
	long				result;

	pgBlockMove(style, &small_caps, sizeof(style_info));		
	small_caps.styles[small_caps_var] = small_caps.styles[all_lower_var]
			= small_caps.styles[all_caps_var] = 0;

	mother_point = small_caps.point >> 16;

	if ((point_percent = style->styles[small_caps_var]) == -1)
		point_percent = 75;
	
	mother_point *= point_percent;
	mother_point /= 100;
	small_caps.point = mother_point << 16;
	small_caps.maintenance = 0;
	small_caps.used_ctr = small_caps.style_sheet_id = 0;
	font_index = style->font_index;
	small_caps.procs.init(pg, &small_caps, (font_info_ptr) UseMemoryRecord(pg->fonts, font_index, 0, TRUE));
	UnuseMemory(pg->fonts);

	if ((result = (long)pgFindMatchingStyle((memory_ref) pg->t_formats, &small_caps,
				0, SIGNIFICANT_STYLE_SIZE)) > 0)
		result -= 1;
	else
		result = (long)append_new_style_info(pg, &small_caps, src_option, reason_verb);
	
	if (style->maintenance & IS_STYLE_SHEET) {
		style_info_ptr		added_style;
		
		added_style = (style_info_ptr) UseMemoryRecord(pg->t_formats, result, 0, TRUE);
		added_style->used_ctr += 1;
		UnuseMemory(pg->t_formats);
	}

	return	result;
}


/* pgChangeStyleRun is called to change a style, info or paragraph run. The ref
param is either the style or info or paragraph run reference;  the change param is a pointer
to a style_change record and the change_proc gets called for each record that will
get changed.  Everything in the run is handled between from_offset and to_offset. */

PG_PASCAL (void) pgChangeStyleRun (style_ref ref, change_info_ptr change, style_change_proc change_proc,
		long from_offset, long to_offset)
{
	register style_run_ptr	run;
	style_run				ending_run, change_run;
	pg_short_t				old_style_item, new_style_item;
	size_t					doc_size, rec_num;

	run = pgFindRunFromRef (ref, to_offset, NULL);
	ending_run = *run;
	UnuseMemory(ref);
	
	run = pgFindRunFromRef (ref, from_offset, &rec_num);
	change_run = *run;

	new_style_item = change_proc(change, &change_run);
	doc_size = change->pg->t_length;

	if (!doc_size || (from_offset == to_offset && run->offset == from_offset
			&& from_offset == doc_size)) {
		/* Does this of doc is completely empty or if changing empty par at end of doc */

		change_used_ctr(change, run->style_item, new_style_item);
		run->style_item = new_style_item;
	}
	else {

		if (new_style_item != change_run.style_item) {
		
			if (from_offset != run->offset) {
				
				++rec_num;
				run = (style_run_ptr) InsertMemory(ref, rec_num, 1);
				old_style_item = NULL_RUN;
			}
			else
				old_style_item = change_run.style_item;
	
			change_used_ctr(change, old_style_item, new_style_item);
			
			run->offset = from_offset;
			run->style_item = new_style_item;
		}
		
		for (;;) {
			
			++rec_num;
			++run;
			if (run->offset >= to_offset)
				break;
			
			new_style_item = change_proc(change, run);
			change_used_ctr(change, run->style_item, new_style_item);
			run->style_item = new_style_item;
		}
		
		if (run->offset != to_offset) {
			old_style_item = NULL_RUN;
			run = (style_run_ptr) InsertMemory(ref, rec_num, 1);
		}
		else
			old_style_item = run->style_item;
		
		change_used_ctr(change, old_style_item, ending_run.style_item);
	
		ending_run.offset = to_offset;
		*run = ending_run;
	}

	UnuseMemory(ref);
}



/* pgFinishStyleUpdate gets called after a style/par/font change has been made. Its
purpose, really, is to save code space since all mentioned actions have this
ending part in common. The change_stuff is the change_info record just used,
ref_to_dispose is typically the selection list used and draw_mode is usually
given to us by the app.  */

PG_PASCAL (void) pgFinishStyleUpdate (paige_rec_ptr pg, change_info_ptr change_stuff,
		memory_ref ref_to_dispose, pg_boolean inval_text, short draw_mode)
{
	if (ref_to_dispose)
		UnuseAndDispose(ref_to_dispose);
	
	pgFixAllStyleRuns(pg, change_stuff);

	if (change_stuff->changed || (pg->flags & TEXT_DIRTY_BIT)) {

		if (inval_text)
			pgInvalSelect(pg->myself, change_stuff->change_range.begin,
						change_stuff->change_range.end);

		if (change_stuff->changed) {
			
			pg->flags |= TEXT_DIRTY_BIT;
			++pg->change_ctr;
		}

		if (draw_mode) {

			if (draw_mode == best_way)
				draw_mode = bits_copy;

			pgUpdateText(pg, &pg->port, 0, pg->t_length, MEM_NULL, NULL, draw_mode, TRUE);
		}
	}
}




/* pgStyleMatchesCriteria returns TRUE if target_style is the same as match_style based
on the following criteria:  only non-zero fields in mask are compared;  before
compared, the corresponding field in target_style is AND'd with AND_mask. Any
of these pointers except target_style might be NULL.  If mask is NULL, all
fields are compared;  if match_style is NULL the match is considered TRUE if
target field (optionally AND'd with AND_mask) is "true" (non-zero); 
if AND_mask is NULL, nothing is AND'd and the whole field is compared. */

PG_PASCAL (pg_boolean) pgStyleMatchesCriteria (void PG_FAR *target_style, void PG_FAR *match_style,
		void PG_FAR *mask, void PG_FAR *AND_mask, short rec_length)
{
	register short PG_FAR			*compare_to, *compare_from;
	short PG_FAR					*mask_values, *and_values;
	register short					match_value, match_to, compare_qty;

	compare_to = (short *) target_style;
	compare_from = (short *) match_style;
	mask_values = (short *) mask;
	and_values = (short *) AND_mask;
	
	for (compare_qty = rec_length / sizeof(short); compare_qty; --compare_qty) {
	
		match_to = *compare_to++;

		if (and_values) {
			
			match_to &= *and_values;
			++and_values;
		}
		
		if (compare_from) {
		
			match_value = *compare_from++;
		

			if (!mask_values) {
	
				if (match_value != match_to)
					return	FALSE;
			}
			else {
				
				if (*mask_values++)
					if (match_value != match_to)
						return	FALSE;
			}
		}
		else {

			if (!mask_values) {
	
				if (match_to)
					return	TRUE;
			}
			else {
				
				if (*mask_values++)
					if (match_to)
						return	TRUE;
			}
		}
	}
	
	return	(compare_from != NULL);
}



/* pgFindRunFromRef returns a <<used>> pointer for a style run that matches offset. If rec_num
is non-NULL, the record number is returned.  */

PG_PASCAL (style_run_ptr) pgFindRunFromRef (style_ref ref, size_t offset, size_t PG_FAR *rec_num)
{
	register style_run_ptr		run;
	register size_t				abs_offset, record;
	size_t						num_records;
	
	num_records = GetMemorySize(ref);
	run = (style_run_ptr) UseMemory(ref);
	record = 0;

	if (abs_offset = offset) {

		while (num_records > BINARY_TABLE_MINIMUM) {
			
			if (run[BINARY_TABLE_MINIMUM].offset >= offset)
				break;
			
			run += BINARY_TABLE_MINIMUM;
			num_records -= BINARY_TABLE_MINIMUM;
			record += BINARY_TABLE_MINIMUM;
		}

		while (run->offset <= abs_offset) {
		
			++record;
			++run;
		}
		
		--run;
		--record;
	}
	
	if (rec_num)
		*rec_num = record;

	return run;
}


/* pgFixAllStyleRuns walks through the style run(s) and removes anything useless
or redundant (such as two of the same styles back to back and style_info records
whose used_ctr is zero).  */

PG_PASCAL (void) pgFixAllStyleRuns (paige_rec_ptr pg, change_info_ptr change)
{
	register style_info_ptr		style_base;
	register par_info_ptr		par_base;
	style_run_ptr				end_ptr;
	style_run					end_of_run;
	memory_ref					main_run;
	long						num_runs;
	pg_short_t					bad_style, bad_index;
	pg_short_t					num_styles, max_styles;
	paige_sub_ptr				sub_ptr;

	change->pg = pg;

	if (change->style_change) {
		
		style_base = (style_info_ptr) UseMemory(pg->t_formats);
		
		while (bad_style = find_bad_style(pg, pg->t_style_run, &bad_index)) {

			DeleteMemory(pg->t_style_run, bad_style, 1);
			style_base[bad_index].used_ctr -= 1;
			adjust_small_caps_ctr(pg, &style_base[bad_index], -1);
		}

/* Remove any "styles" that are equal to or greater than the current text length */

		for (;;) {
			
			num_styles = (pg_short_t)GetMemorySize(pg->t_style_run) - 2;
			
			if (num_styles < 1)
				break;

			GetMemoryRecord(pg->t_style_run, num_styles, &end_of_run);
			
			if ((!end_of_run.offset) || (end_of_run.offset < pg->t_length))
				break;
			
			DeleteMemory(pg->t_style_run, num_styles, 1);
			style_base[end_of_run.style_item].used_ctr -= 1;
			adjust_small_caps_ctr(pg, &style_base[end_of_run.style_item], -1);
		}
		
		end_ptr = (style_run_ptr) UseMemoryRecord(pg->t_style_run, GetMemorySize(pg->t_style_run) - 2,
				USE_ALL_RECS, TRUE);
		end_ptr[1].style_item = end_ptr->style_item;
		
		end_ptr = (style_run_ptr) UseMemoryRecord(pg->t_style_run, 0, 0, FALSE);
		end_ptr->offset = 0;

		UnuseMemory(pg->t_style_run);
		
		if (!(pg->flags2 & (NO_STYLE_CLEANUP | IS_MASTER_BIT))) {
		
			num_styles = max_styles = (pg_short_t)GetMemorySize(pg->t_formats);
			bad_index = 0;
	
			main_run = pg->t_style_run;
				
			if (GetMemorySize(pg->subref_stack)) {
				
				sub_ptr = (paige_sub_ptr) UseMemory(pg->subref_stack);
				main_run = sub_ptr->t_style_run;
				UnuseMemory(pg->subref_stack);
			}

			while (num_styles) {
	
				if ((!style_base->used_ctr) && (bad_index != pg->insert_style)) {
	
					style_base->procs.delete_style(pg, pg->globals, not_used_reason, pg->t_formats, style_base);
	 
					UnuseMemory(pg->t_formats);
					DeleteMemory(pg->t_formats, bad_index, 1);
					--max_styles;
					pgDescrementRunItems(main_run, bad_index, max_styles);
					decrement_small_caps_indexes(pg, bad_index);
					pgDecrementSubsetStyles(pg, bad_index, max_styles, FALSE);

					if (pg->insert_style > bad_index)
						--pg->insert_style;
					if (pg->def_style_index > bad_index)
						--pg->def_style_index;

					if (!(--num_styles))
						bad_index = 0;
					
					style_base = (style_info_ptr) UseMemoryRecord(pg->t_formats, bad_index, USE_ALL_RECS, TRUE);
				}
				else {
					++style_base;
					++bad_index;
					--num_styles;
				}
			}
		}

		UnuseMemory(pg->t_formats);
		pg->t_formats = ForceMinimumMemorySize(pg->t_formats);
	}

	if (change->par_change || change->tab_change) {

		par_base = (par_info_ptr) UseMemory(pg->par_formats);
		
		while (bad_style = find_bad_style(pg, pg->par_style_run, &bad_index)) {
			
			DeleteMemory(pg->par_style_run, bad_style, 1);
			par_base[bad_index].used_ctr -= 1;
		}
		
	/* Check ending record to see if "valid" or not */

		end_ptr = (style_run_ptr) UseMemory(pg->par_style_run);
		end_ptr->offset = 0;
		UnuseMemory(pg->par_style_run);

		num_runs = GetMemorySize(pg->par_style_run) - 2;
		
		if ((num_runs > 0) && pg->t_length && !(pg->flags & DOC_TERMINATED_BIT)) {
			
			GetMemoryRecord(pg->par_style_run, num_runs, &end_of_run);
			
			if (end_of_run.offset == pg->t_length) {

				DeleteMemory(pg->par_style_run, num_runs, 1);
				par_base[end_of_run.style_item].used_ctr -= 1;
			}
		}

		if (!(pg->flags2 & (NO_STYLE_CLEANUP | IS_MASTER_BIT))) {
		
			num_styles = max_styles = (pg_short_t)GetMemorySize(pg->par_formats);
			par_base = (par_info_ptr) UseMemoryRecord(pg->par_formats, 0, USE_ALL_RECS, FALSE);
	
			bad_index = 0;
			
			main_run = pg->par_style_run;
				
			if (GetMemorySize(pg->subref_stack)) {
				
				sub_ptr = (paige_sub_ptr) UseMemory(pg->subref_stack);
				main_run = sub_ptr->par_style_run;
				UnuseMemory(pg->subref_stack);
			}
	
			while (num_styles) {

				if (!par_base->used_ctr) {
	
					par_base->procs.delete_par(pg, not_used_reason, pg->par_formats, par_base);
	
					UnuseMemory(pg->par_formats);
					DeleteMemory(pg->par_formats, bad_index, 1);
					--max_styles;
					pgDescrementRunItems(main_run, bad_index, max_styles);
					pgDecrementSubsetStyles(pg, bad_index, max_styles, TRUE);
	
					if (!(--num_styles))
						bad_index = 0;
	
					par_base = (par_info_ptr) UseMemoryRecord(pg->par_formats, bad_index, USE_ALL_RECS, TRUE);
				}
				else {
					++par_base;
					++bad_index;
					--num_styles;
				}
			}
		}
		
		UnuseMemory(pg->par_formats);
		pg->par_formats = ForceMinimumMemorySize(pg->par_formats);
	}

#ifdef PG_DEBUG
	pgCheckRunIntegrity(pg);
#endif
}


/* pgStyleSuperImpose gets called by the font installer code (from machine-specific
functions) which sets up a new style and font depending on the stylesheet ID
found in style->styles[super_impose_var].  The new_style and new_font params return
as the style/font that should actually be used. Note, not every field is
super-imposed, only display-affecting elements (but not function pointers).
The style array is exclusive-ORd with the target. */

PG_PASCAL (void) pgStyleSuperImpose (paige_rec_ptr pg, style_info_ptr style,
		style_info_ptr new_style, font_info_ptr new_font, short use_stylesheet)
{
	register style_info_ptr		stylesheet;
	register style_info_ptr		target_style;
	font_info					imposed_font;
	short						stylesheet_id, style_index_ctr;

	target_style = new_style;
	pgBlockMove(style, target_style, sizeof(style_info));
	
	if (use_stylesheet != 0)
		stylesheet_id = use_stylesheet;
	else
		stylesheet_id = target_style->styles[super_impose_var];

	if (stylesheet = pgLocateStyleSheet(pg, stylesheet_id, NULL)) {
		
		if (stylesheet->font_index != 0)
			target_style->font_index = stylesheet->font_index;

		for (style_index_ctr = 0; style_index_ctr < MAX_STYLES; ++style_index_ctr)
			if (style_index_ctr != super_impose_var) {
			
				if (stylesheet->styles[style_index_ctr] == XOR_SUPERIMPOSE)
					target_style->styles[style_index_ctr] ^= stylesheet->styles[style_index_ctr];
				else
				if (stylesheet->styles[style_index_ctr])
					target_style->styles[style_index_ctr] = stylesheet->styles[style_index_ctr];
		}

		set_non_zero_long(&stylesheet->point, &target_style->point);
		set_non_zero_long(&stylesheet->char_width, &target_style->char_width);		
		set_non_zero_long(&stylesheet->top_extra, &target_style->top_extra);
		set_non_zero_long(&stylesheet->bot_extra, &target_style->bot_extra);
		set_non_zero_long(&stylesheet->space_extra, &target_style->space_extra);
		set_non_zero_long(&stylesheet->char_extra, &target_style->char_extra);

		if (!pgZeroStruct(&stylesheet->fg_color, sizeof(color_value)))
			target_style->fg_color = stylesheet->fg_color;
		if (!pgZeroStruct(&stylesheet->bk_color, sizeof(color_value)))	//¥ TRS/OITC
			target_style->bk_color = stylesheet->bk_color;
		
		UnuseMemory(pg->t_formats);
	}

	target_style->styles[super_impose_var] = 0;
	target_style->small_caps_index = 0;
	
	if (target_style->font_index == DEFAULT_FONT_INDEX)
		target_style->font_index = 0;
	
	GetMemoryRecord(pg->fonts, target_style->font_index, &imposed_font);
	target_style->procs.init(pg, target_style, &imposed_font);

	if (new_font)
		pgBlockMove(&imposed_font, new_font, sizeof(font_info));
}


/* Given an index from a style_run just deleted, this function decrements
all index values in the style_run that are greater than the deleted one.  */

PG_PASCAL (void) pgDescrementRunItems (style_ref ref, pg_short_t index, pg_short_t max_styles)
{
	pg_short_t				num_styles;
	register style_run_ptr	styles;
	
	num_styles = (pg_short_t)GetMemorySize(ref);
	
	for (styles = (style_run_ptr) UseMemory(ref); num_styles; ++styles, --num_styles)
		if (styles->style_item != NULL_RUN) {
		
			if (styles->style_item > index)
				--styles->style_item;
			else
			if (styles->style_item >= max_styles)
				styles->style_item = max_styles - 1;
		}

	UnuseMemory(ref);
}


/* pgScaleParInfo scales a style_info record based on numerator over denominator. */

PG_PASCAL (void) pgScaleStyleInfo (paige_rec_ptr pg, style_info_ptr style, short numerator, short denominator)
{
	pg_fixed			scale;
	
	scale = numerator;
	scale <<= 16;
	scale |= denominator;

	pgScaleLong(scale, 0, &style->top_extra);
	pgScaleLong(scale, 0, &style->bot_extra);
	pgScaleLong(scale, 0, &style->left_overhang);
	pgScaleLong(scale, 0, &style->right_overhang);
	
	if (style->embed_object) {
		pg_embed_ptr		embed_ptr;
		
		embed_ptr = (pg_embed_ptr) UseMemory((memory_ref)style->embed_object);
		
		pgScaleLong(scale, 0, &embed_ptr->width);
		pgScaleLong(scale, 0, &embed_ptr->minimum_width);
		pgScaleLong(scale, 0, &embed_ptr->descent);
		pgScaleShort(scale, 0, &embed_ptr->top_extra);
		pgScaleShort(scale, 0, &embed_ptr->bot_extra);
		pgScaleShort(scale, 0, &embed_ptr->left_extra);
		pgScaleShort(scale, 0, &embed_ptr->right_extra);
		
		UnuseMemory((memory_ref)style->embed_object);
	}
}


/* pgEqualFontNames compares the font names between these two fonts. Changed
	to support alt name C types different from primary name */

PG_PASCAL (pg_boolean) pgEqualFontNames (font_info_ptr font1, font_info_ptr font2, pg_boolean use_alternate)
{
	register pg_char_ptr		name1, name2;
	pg_short_t					length1, length2;
	short						c_test1, c_test2;
	
	if (use_alternate) {

		name1 = font1->alternate_name;
		name2 = font2->alternate_name;
		c_test1 = font1->environs & NAME_ALT_IS_CSTR;
		c_test2 = font2->environs & NAME_ALT_IS_CSTR;
	}
	else {
	
		name1 = font1->name;
		name2 = font2->name;
		c_test1 = font1->environs & NAME_IS_CSTR;
		c_test2 = font2->environs & NAME_IS_CSTR;
	}
	if (c_test1)

		length1 = (pg_short_t)pgCStrLength((pg_c_string_ptr)name1);
	else
		length1 = *name1++;

	if (c_test2)
		length2 = (pg_short_t)pgCStrLength((pg_c_string_ptr)name2);
	else
		length2 = *name2++;
	
	if (length1 != length2)
		return	FALSE;
	
	while (length1) {
		
		if (*name1++ != *name2++)
			return	FALSE;
		
		--length1;
	}

	return	TRUE;
}


/* pgFindParExclusionRun locates a paragraph exclusion item, if any, beginning at offset.
An exact match is not expected.  If remaining_run is non-NULL it is set to the number of
records remaining (including the current one!). Note that the tail end of the run might
be returned -- which means it points to NOTHING.  Check this by examining *remaining_run --
it will be zero in this case. */

PG_PASCAL (style_run_ptr) pgFindParExclusionRun (paige_rec_ptr pg, long offset,
		long PG_FAR *remaining_run)
{
	style_run_ptr			result = NULL;
	long					num_runs;
	
	if ((num_runs = GetMemorySize(pg->par_exclusions)) > 0) {
		
		result = (style_run_ptr) UseMemory(pg->par_exclusions);
		
		while (num_runs) {
			
			if (result->offset >= offset)
				break;

			++result;
			--num_runs;
		}
	}

	if (remaining_run)
		*remaining_run = num_runs;

	return		result;
}


/******************************** Local Functions ****************************/


/* This function sets up the current styles and font info */

static void setup_walk_styles (style_walk_ptr walker)
{
	register style_walk_ptr		walk;

	walk = walker;

#ifdef PG_DEBUG
	if ((long)walk->prev_style_run->style_item >= walk->max_styles)
		walker->globals->mem_globals->debug_proc(RANGE_ERR, walk->stylerun);
#endif

	walk->cur_style = (walk->style_base + walk->prev_style_run->style_item);
	
	if (walk->par_base) {
	
#ifdef PG_DEBUG
		if ((long)walk->prev_par_run->style_item >= walk->max_pars)
			walker->globals->mem_globals->debug_proc(RANGE_ERR, walk->parrun);
#endif
		walk->cur_par_style = (walk->par_base + walk->prev_par_run->style_item);
	}
	
	if (walk->last_font != walk->cur_style->font_index) {

		if ((walk->last_font = walk->cur_style->font_index) == DEFAULT_FONT_INDEX)
			walk->last_font = 0;

		walk->cur_font = (walk->font_base + walk->last_font);
	}

	walk->style_overlay = walk->cur_style->styles[super_impose_var];

	if (walk->flags2 & SHOW_HT_TARGETS)
		if (walk->current_offset >= walk->hyperlink_target->applied_range.begin
			&& walk->current_offset < walk->hyperlink_target->applied_range.end)
				walk->style_overlay = walk->hyperlink_target->active_style;

	if (walk->current_offset >= walk->hyperlink->applied_range.begin
		&& walk->current_offset < walk->hyperlink->applied_range.end)
			walk->style_overlay = walk->hyperlink->active_style;
}


/* Given a memory_ref of offset pairs, this function converts all pairs to
real paragraph boundaries.  */

static void convert_to_real_par_offsets (paige_rec_ptr pg, memory_ref selection)
{
	register select_pair_ptr	selections;
	register short				num_refs;
	size_t						unwanted_offset, right_offset;

	for (selections = (select_pair_ptr) UseMemory(selection), num_refs = (short)GetMemorySize(selection);
			num_refs;  ++selections, --num_refs) {

		pgFindPar(pg->myself, selections->begin, &selections->begin, &unwanted_offset);
		if ((right_offset = selections->end) != selections->begin)
			--right_offset;
		pgFindPar(pg->myself, right_offset, &unwanted_offset, &selections->end);

	}
	
	UnuseMemory(selection);
}


/* This changes the used_ctr field for a style, info or paragraph record if old_item
does not equal new_item.  These items are the style_item values in a style_run.
If old_item is NULL_RUN, it is not affected.  */

static void change_used_ctr (change_info_ptr change, pg_short_t old_item, pg_short_t new_item)
{
	register style_info_ptr		style_base;
	register par_info_ptr		par_base;

	if (old_item == new_item)
		return;
	
	if (change->par_change || change->tab_change) {

		par_base = (par_info_ptr) UseMemory(change->pg->par_formats);
		
		if (old_item != NULL_RUN)
			par_base[old_item].used_ctr -= 1;
		par_base[new_item].used_ctr += 1;

		UnuseMemory(change->pg->par_formats);
	}
	else {

		style_base = (style_info_ptr) UseMemory(change->pg->t_formats);
		
		if (old_item != NULL_RUN) {
		
			style_base[old_item].used_ctr -= 1;
			adjust_small_caps_ctr(change->pg, &style_base[old_item], -1);
		}

		style_base[new_item].used_ctr += 1;
		adjust_small_caps_ctr(change->pg, &style_base[new_item], 1);
		
		UnuseMemory(change->pg->t_formats);
	}
}


/* This function returns a record offset of a style_run that should be deleted.
It will never return the very first offset, so a zero result indicates nothing
to be deleted.  The bad_index param is a pointer to the style index that is in
the style_run to be deleted.   */

static pg_short_t find_bad_style (paige_rec_ptr pg, style_ref ref,
		pg_short_t PG_FAR *bad_index)
{
	register style_run_ptr		styles;
	long						t_length;
	pg_short_t					num_runs;
	pg_short_t					rec_num;

	if (!(t_length = pg->t_length)) {
		
		if (GetMemorySize(ref) > 2) {
			
			styles = (style_run_ptr) UseMemory(ref);
			*bad_index = styles[1].style_item;
			UnuseMemory(ref);

			return	1;
		}

		return	0;
	}
	
	t_length += ZERO_TEXT_PAD;

	if (num_runs = (pg_short_t)GetMemorySize(ref) - 2) {
		
		for (rec_num = 1, styles = (style_run_ptr) UseMemory(ref); num_runs; ++rec_num, ++styles, --num_runs) {
			
			if ((styles[1].offset <= styles->offset)
					|| (styles[1].style_item == styles->style_item)
					|| (styles[1].offset >= t_length)) {
				
				*bad_index = styles[1].style_item;

				UnuseMemory(ref);

				return	rec_num;
			}
		}
		
		UnuseMemory(ref);
	}
	
	return	0;
}


/* decrement_small_caps_indexes decrements any small_caps_index values in the
array of style_info since bad_index just got deleted. */

static void decrement_small_caps_indexes (paige_rec_ptr pg, pg_short_t bad_index)
{
	register style_info_ptr		styles;
	long						num_styles, index_just_deleted;
	
	index_just_deleted = (long)bad_index;
	
	for (styles = (style_info_ptr) UseMemory(pg->t_formats), num_styles = GetMemorySize(pg->t_formats);
			num_styles; ++styles, --num_styles)
		if (styles->styles[small_caps_var] && (styles->small_caps_index > index_just_deleted))
			--styles->small_caps_index;
	
	UnuseMemory(pg->t_formats);
}


/* This function is called heavily by pgSetMaskFromFlds for comparing two *longs
and setting the mask accordingly. If positive_match is FALSE, *mask is cleared only if the
two longs don't match;  if positive_match is TRUE, *mask is set to 1's only if the two
longs match. Multiple consecutive longs can be compare if num_flds > 1.
If bitwise_compare is TRUE then the individual bits get set in the field(s). */

static void compare_long_and_set (long PG_FAR *src_fld, long PG_FAR *target_fld,
		long PG_FAR *mask_fld, short num_flds, pg_boolean positive_match, pg_boolean bitwise_compare)
{
	register long PG_FAR	*src, *target, *mask;
	register short			ctr;
	long					bitfield;

	src = src_fld;
	target = target_fld;
	mask = mask_fld;
	ctr = num_flds;

	if (positive_match) {
		
		if (bitwise_compare) {

			while (ctr) {
				
				bitfield = *target++;
				bitfield ^= *src++;
				*mask++ = ~bitfield;
				--ctr;
			}
		}
		else {
			
			while (ctr) {
				
				if (*target++ == *src++)
					*mask = COMPARE_TRUE;
				
				++mask;
				--ctr;
			}
		}
	}
	else {
		
		while (ctr) {

			if (*target++ != *src++)
				*mask = COMPARE_FALSE;

			++mask;
			--ctr;
		}
	}
}


/* This function is called heavily by pgSetMaskFromFlds for comparing two *shorts
and setting the mask accordingly. If positive_match is FALSE, *mask is cleared only if the
two shorts don't match;  if positive_match is TRUE, *mask is set to 1's only if the two
shorts match. Multiple consecutive shorts can be compare if num_flds > 1. */
 
static void compare_short_and_set (short PG_FAR *src_fld, short PG_FAR *target_fld,
		short PG_FAR *mask_fld, short num_flds, pg_boolean positive_match)
{
	register short PG_FAR	*src, *target, *mask;
	register short			ctr;

	src = src_fld;
	target = target_fld;
	mask = mask_fld;
	ctr = num_flds;

	if (positive_match) {
		
		while (ctr) {

			if (*target++ == *src++)
				*mask = COMPARE_TRUE;
			
			++mask;
			--ctr;
		}
	}
	else {

		while (ctr) {

			if (*target++ != *src++)
				*mask = COMPARE_FALSE;

			++mask;
			--ctr;
		}
	}
}


/* This function returns the number of trailing blanks at end of specified
selection range. Could be zero, but at not more than end - begin - 1. */

static long trailing_text_blanks (paige_rec_ptr pg, size_t begin_select, size_t end_select)
{
	text_block_ptr		block;
	pg_char_ptr			text;
	style_walk			walker;
	long				local_offset, global_offset, start_offset, info;
	long				local_start, end_offset;

	if (global_offset = end_select)
		global_offset -= 1;
	
	if (global_offset == begin_select)
		return	0;

	block = pgFindTextBlock(pg, global_offset, NULL, FALSE, TRUE);
	text = (pg_char_ptr) UseMemory(block->text);
	pgPrepareStyleWalk(pg, global_offset, &walker, FALSE);
	
	local_offset = global_offset - block->begin;

	if ((start_offset = begin_select) < block->begin)
		start_offset = block->begin;

	local_start = start_offset - block->begin;
	end_offset = block->end - block->begin;

	while (global_offset > start_offset) {
		
		info = walker.cur_style->procs.char_info(pg, &walker, text, block->begin,
				local_start, end_offset, local_offset, BLANK_BIT | CTL_BIT | LAST_HALF_BIT | MIDDLE_CHAR_BIT);
		
		if (!(info & BLANK_BIT) || (info & CTL_BIT))
			break;
		
		pgWalkStyle(&walker, -1);
		global_offset -= 1;
		local_offset -= 1;
	}
	
	pgPrepareStyleWalk(pg, 0, NULL, FALSE);
	UnuseMemory(block->text);
	UnuseMemory(pg->t_blocks);
	
	return	end_select - global_offset - 1;
}


/* append_new_style_info adds a new style to the array of style_info records. */

static pg_short_t append_new_style_info (paige_rec_ptr pg, style_info_ptr the_style,
		paige_rec_ptr src_option, short reason_verb)
{
	style_info_ptr		appended_ptr = NULL;
	long				num_styles, index;
	pg_short_t			result;
	
	num_styles = index = GetMemorySize(pg->t_formats);
	
	if ((pg->flags2 & IS_MASTER_BIT) && !(pg->flags2 & NO_STYLE_CLEANUP)) {
		
		appended_ptr = (style_info_ptr) UseMemory(pg->t_formats);
		
		for (index = 0; index < num_styles; ++index, ++appended_ptr)
			if (appended_ptr->used_ctr == 0)
				break;
	}

	if (index == num_styles)
		appended_ptr = (style_info_ptr) AppendMemory(pg->t_formats, 1, FALSE);
	
	result = (pg_short_t)index;

	*appended_ptr = *the_style;
	appended_ptr->used_ctr = 0;
	appended_ptr->procs.duplicate(src_option, pg, reason_verb, pg->t_formats, appended_ptr);

	if (!(appended_ptr->maintenance & IS_STYLE_SHEET)) {
		
		if (appended_ptr->style_sheet_id > 0)
			appended_ptr->style_sheet_id = -appended_ptr->style_sheet_id;
	}
	else
		++appended_ptr->used_ctr;

	appended_ptr->maintenance &= MAINTENANCE_MASK;
	
	UnuseMemory(pg->t_formats);
	
	return	result;
}


/* adjust_small_caps_ctr adds or subtracts to used_ctr field for any subset
small_caps_index style. Caller of this function already has used pointer from
pg->t_formats. */

static void adjust_small_caps_ctr (paige_rec_ptr pg, style_info_ptr mother_style,
		long amt)
{
	style_info_ptr			alt_style;

	if (mother_style->styles[small_caps_var]) {
		
		alt_style = (style_info_ptr) UseMemoryRecord(pg->t_formats, mother_style->small_caps_index,
				0, FALSE);
		alt_style->used_ctr += amt;
	}
}

/* set_non_zero_long sets *target with *source if *source is non-zero. */

static void set_non_zero_long (long PG_FAR *source, long PG_FAR *target)
{
	if (*source)
		*target = *source;
}


/* fix_multiple_select_pairs walks through the selection pairs in pairs_ref
and fixes "backwards sequence" pairs. */

static void fix_multiple_select_pairs (memory_ref pairs_ref)
{
	register select_pair_ptr	selections;
	register short				index;
	select_pair					swap_pair;
	short						num_selects;
	
	num_selects = (short)GetMemorySize(pairs_ref);
	selections = (select_pair_ptr) UseMemory(pairs_ref);
	
	if (selections->end < selections->begin) {
		
		swap_pair = *selections;
		selections->begin = swap_pair.end;
		selections->end = swap_pair.begin;
	}
	
	--num_selects;
	
	for (index = 0; index < num_selects; ++index) {

		if (selections[index].end < selections[index].begin) {
			
			swap_pair = selections[index];
			selections[index].begin = swap_pair.end;
			selections[index].end = swap_pair.begin;
		}
	
		if (selections[index + 1].begin < selections[index].begin) {
			
			swap_pair = selections[index];
			selections[index] = selections[index + 1];
			selections[index + 1] = swap_pair;
			index = -1;
		}
	}
	

/* Fix the offsets so none of them overlap: */

	for (index = 0; index < num_selects; ++index) {
		
		if (selections[index + 1].begin < selections[index].end)
			selections[index + 1].begin = selections[index].end;
	}
	
	++num_selects;
	
	for (index = 1; index < num_selects; ++index)
		if (selections[index].begin > selections[index].end)
			selections[index].begin = selections[index].end;
	
	UnuseMemory(pairs_ref);
}
