/* This file handles all optional style functions. If your app doesn't use
any of the "set/get" style functions in Paige.h it can omit this library. */

#include "Paige.h"

#ifdef MAC_PLATFORM
#pragma segment pgstyles
#endif

#include "pgText.h"
#include "pgDefStl.h"
#include "machine.h"
#include "defprocs.h"
#include "pgUtils.h"
#include "pgEdit.h"
#include "pgSelect.h"
#include "pgPar.h"
#include "pgShapes.h"
#include "pgStyles.h"
#include "pgErrors.h"
#include "pgSubref.h"

static void get_style_info (change_info_ptr change, style_run_ptr style, pg_short_t record_ctr);
static pg_short_t change_style_info (change_info_ptr change, style_run_ptr style);
static pg_short_t change_class_info (change_info_ptr change, style_run_ptr style);
static pg_boolean valid_insertion (paige_rec_ptr pg, select_pair_ptr selection);


/* pgGetStyleInfo returns information about a style_info record belonging to
the current selection range. If either select_to or select_from is negative,
the current selection point(s) are used.  If set_any_match is TRUE, the mask
is set for all items that are found anywhere in info, otherwise info is filled
in and mask set to TRUE for conistent items.   The function result is the first
text offset examined.  */

PG_PASCAL (long) pgGetStyleInfo (pg_ref pg, const select_pair_ptr selection,
		short set_any_match, style_info_ptr info, style_info_ptr mask)
{
	paige_rec_ptr				pg_rec;
	memory_ref					select_list;
	register pg_short_t			num_selects;
	register select_pair_ptr	select_run;
	change_info					stuff_to_report;
	long						first_select;

	pg_rec = (paige_rec_ptr) UseMemory(pg);

	if (set_any_match)
		pgFillBlock(mask, sizeof(style_info), 0);
	else {
	
		pgFillBlock(info, sizeof(style_info), 0);
		pgFillBlock(mask, sizeof(style_info), -1);
	}

	if (select_list = pgSetupOffsetRun(pg_rec, selection, FALSE, TRUE)) {

		pgFillBlock(&stuff_to_report, sizeof(change_info), 0);
		
		stuff_to_report.any_match = set_any_match;
		
		stuff_to_report.pg = pg_rec;
		stuff_to_report.style_change = info;
		stuff_to_report.style_mask = mask;
		stuff_to_report.base = UseMemory(pg_rec->t_formats);
	
		num_selects = (pg_short_t)GetMemorySize(select_list);
		select_run = (select_pair_ptr) UseMemory(select_list);
		first_select = select_run->begin;

		while (num_selects) {

			pgReturnStyleInfo(pg_rec->t_style_run, &stuff_to_report, get_style_info,
					select_run->begin, select_run->end);

			++select_run;
			--num_selects;
		}
		
		UnuseMemory(pg_rec->t_formats);
		UnuseAndDispose(select_list);
	}
	else {  /* A single insertion point exists  */
		pg_short_t				style_index;
		style_run_ptr			run;

		if (!selection) {
		
			first_select = pgCurrentInsertion(pg_rec);
			style_index = pg_rec->insert_style;
		}
		else {
			
			first_select = selection->begin;
			run = pgFindRunFromRef(pg_rec->t_style_run, first_select, NULL);
			style_index = run->style_item;
			UnuseMemory(pg_rec->t_style_run);
		}
		
		if (set_any_match) {
			style_info			current_style;
			
			GetMemoryRecord(pg_rec->t_formats, style_index, &current_style);
			pgSetMaskFromFlds(info, &current_style, mask, style_compare, TRUE, FALSE);
		}
		else
			GetMemoryRecord(pg_rec->t_formats, style_index, info);
	}

	UnuseMemory(pg);
	
	return	first_select;
}


/* pgSetStyleInfo sets (changes) the style_info for a specified selection range. 
If either select_from or select_to are negative, the current selection point(s)
are used.  */

PG_PASCAL (void) pgSetStyleInfo (pg_ref pg, const select_pair_ptr selection,
		const style_info_ptr info, const style_info_ptr mask,  short draw_mode)
{
	paige_rec_ptr				pg_rec;
	pg_short_t					old_font_index;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	old_font_index = mask->font_index;
	mask->font_index = 0;
	
	pgChangeStyleInfo(pg_rec, selection, info, mask, draw_mode);
	mask->font_index = old_font_index;
	
	UnuseMemory(pg);
}


/* pgSetStyleClassInfo changes (only) the class_bits field in the specified
selection. The field changes by AND'ing with class_AND and OR'ing with class_OR. */

PG_PASCAL (void) pgSetStyleClassInfo (pg_ref pg, const select_pair_ptr range, long class_AND,
		long class_OR, short draw_mode)
{
	paige_rec_ptr				pg_rec;
	memory_ref					select_list;
	register pg_short_t			num_selects;
	register select_pair_ptr	select_run;
	change_info					stuff_to_change;

	pg_rec = (paige_rec_ptr) UseMemory(pg);

	if (select_list = pgSetupOffsetRun(pg_rec, range, FALSE, TRUE)) {

		pgFillBlock(&stuff_to_change, sizeof(change_info), 0);
		stuff_to_change.pg = pg_rec;
		stuff_to_change.class_AND = class_AND;
		stuff_to_change.class_OR = class_OR;
		stuff_to_change.style_change = (style_info_ptr) - 1;

		num_selects = (pg_short_t)GetMemorySize(select_list);
		select_run = (select_pair_ptr) UseMemory(select_list);
		stuff_to_change.change_range = *select_run;

		while (num_selects) {
			
			pgChangeStyleRun(pg_rec->t_style_run, &stuff_to_change, change_class_info,
					select_run->begin, select_run->end);
			
			if (select_run->begin < stuff_to_change.change_range.begin)
				stuff_to_change.change_range.begin = select_run->begin;
			if (select_run->end > stuff_to_change.change_range.end)
				stuff_to_change.change_range.end = select_run->end;
	
			++select_run;
			--num_selects;
		}
		
		pgFinishStyleUpdate(pg_rec, &stuff_to_change, select_list, TRUE, draw_mode);
	}
	else {   /* A single insertion exists here  */
		style_info			new_style, temp_mask;
		
		pgGetStyleInfo(pg, NULL, FALSE, &new_style, &temp_mask);
		
		new_style.class_bits &= class_AND;
		new_style.class_bits |= class_OR;
		pg_rec->insert_style = pgAddStyleInfo(pg_rec, NULL, internal_clone_reason, &new_style);
	}
	
	UnuseMemory(pg);
}



/* pgGetStyleInfoRec returns the style_item'th style record  */

PG_PASCAL (void) pgGetStyleInfoRec (pg_ref pg, short style_item, style_info_ptr format)
{
	paige_rec_ptr			pg_rec;
	
	pg_rec = (paige_rec_ptr) UseMemory(pg);
	GetMemoryRecord(pg_rec->t_formats, style_item, format);
	UnuseMemory(pg);
}




/* pgFindStyleInfo returns the first style matching the criteria, beginning at
(and including) *begin_position. If a matching style is found, *begin_position
and *end_position return the style boundaries and the function returns TRUE. 
End_position can be NULL if you don't need its value;  any of the style_info
pointers can be NULL in which case the default comparisons are used.   */

PG_PASCAL (pg_boolean) pgFindStyleInfo (pg_ref pg, size_t PG_FAR *begin_position,
		size_t PG_FAR *end_position, style_info_ptr match_style, style_info_ptr mask,
		style_info_ptr AND_mask)
{
	paige_rec_ptr		pg_rec;
	style_walk			walker;
	long				start_offset;
	pg_boolean			result;
	
	pg_rec = (paige_rec_ptr) UseMemory(pg);
	result = FALSE;
	
	if ((start_offset = *begin_position) < pg_rec->t_length) {
		
		pgPrepareStyleWalk(pg_rec, start_offset, &walker, FALSE);
		
		for (;;) {
			
			if (result = pgStyleMatchesCriteria(walker.cur_style, match_style,
					mask, AND_mask, SIGNIFICANT_STYLE_SIZE))
				break;
			
			if (!pgWalkNextStyle(&walker))
				break;
		}
		
		if (result) {
			
			*begin_position = walker.prev_style_run->offset;
			
			for (;;) {
				
				if (end_position)
					*end_position = walker.next_style_run->offset;
				
				if (!pgWalkNextStyle(&walker))
					break;
				
				if (!pgStyleMatchesCriteria(walker.cur_style, match_style,
						mask, AND_mask, SIGNIFICANT_STYLE_SIZE))
					break;
			}
		}

		pgPrepareStyleWalk(pg_rec, 0, NULL, FALSE);
	}

	UnuseMemory(pg);

	return	result;
}


/* pgGetStyleClass returns the class_bits value from the style applied to character
at position.  The position value can also be CURRENT_POSITION.  The style_range
parameter is an optional pointer to a selection pair;  if non-NULL it gets set
to the boundaries of the style run that contains the same style class bits (if those
class bits are non-zero).  */

PG_PASCAL (long) pgGetStyleClassInfo (pg_ref pg, long position, select_pair_ptr style_range)
{
	paige_rec_ptr				pg_rec;
	style_run_ptr				run;
	register style_info_ptr		style;
	long						use_position;
	long						result;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	
	use_position = pgFixOffset(pg_rec, position);

	run = pgFindStyleRun(pg_rec, use_position, NULL);
	style = (style_info_ptr) UseMemory(pg_rec->t_formats);
	result = style[run->style_item].class_bits;
	
	if (style_range) {
		
		style_range->begin = run->offset;
		style_range->end = run[1].offset;

		if (style_range->end > pg_rec->t_length)
			style_range->end = pg_rec->t_length;
	}

	UnuseMemory(pg_rec->t_formats);
	UnuseMemory(pg_rec->t_style_run);
	UnuseMemory(pg);

	return	result;
}



/* pgSetStyleProcs is used to set specific style_info records with new
function pointers.  This is for the app's convenience (Paige never call this
function, but rather it initializes the default functions). Only the style records
that match the criteria (per parameters given) are changed.

Matches are checked as follows:  For each style that is examined, only the
non-zero fields in mask_style are compared, in which case the corresponding field in
match_style is compared to the target style.  However, the target style field
is first AND'd with the corresponding field in AND_style before compared.

If match_style is NULL, all styles are changed.  If AND_style is NULL, nothing
is AND'd. If match_style is non-NULL but mask_style is NULL, the whole style is
compared, in which case AND_style is AND'd for every field if it exists as well.

User_data and user_id are placed in their respective fields for all
styles that have been changed.

If inval_text is TRUE, the text is invalidated (so it rebuilds).  The draw_mode
works like all other functions with a draw_mode.   */

PG_PASCAL (void) pgSetStyleProcs (pg_ref pg, const pg_style_hooks PG_FAR *procs,
		const style_info_ptr match_style, const style_info_ptr mask_style,
		const style_info_ptr AND_style, long user_data, long user_id,
		pg_boolean inval_text, short draw_mode)
{
	paige_rec_ptr		pg_rec;
	style_info_ptr		styles;
	pg_short_t			style_qty;
	short				some_changed;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	some_changed = FALSE;

	for (styles = (style_info_ptr) UseMemory(pg_rec->t_formats), style_qty = (pg_short_t)GetMemorySize(pg_rec->t_formats);
			style_qty; ++styles, --style_qty)
		if (pgStyleMatchesCriteria(styles, match_style, mask_style, AND_style,
				SIGNIFICANT_STYLE_SIZE)) {

			pgBlockMove(procs, &styles->procs, sizeof(pg_style_hooks));
			styles->user_data = user_data;
			styles->user_id = user_id;
			
			some_changed = TRUE;
		}

	UnuseMemory(pg_rec->t_formats);
	
	if (some_changed) {
		
		if (inval_text)
			pgInvalSelect(pg, 0, pg_rec->t_length);
		
		if (draw_mode)
			pgUpdateText(pg_rec, NULL, 0, pg_rec->t_length, MEM_NULL, NULL, draw_mode,
					TRUE);
	}

	UnuseMemory(pg);
}


/* pgChangeStyleInfo gets called by either pgSetStyleInfo or pgSetFontInfo.
All mask items are considered valid territory (as opposed to pgSetStyleInfo
that never changes font_index). */

PG_PASCAL (void) pgChangeStyleInfo (paige_rec_ptr pg_rec, select_pair_ptr selection,
		style_info_ptr info, style_info_ptr mask,  short draw_mode)
{
	memory_ref					select_list;
	register pg_short_t			num_selects;
	register select_pair_ptr	select_run;
	change_info					stuff_to_change;

#ifdef PG_DEBUG
	if (GetAccessCtr(pg_rec->t_style_run))
		pgFailure(pg_rec->globals->mem_globals, ILLEGAL_RE_ENTER_ERROR, 1);
	if (pg_rec->lock_id)
		pgFailure(pg_rec->globals->mem_globals, LOCKED_PG_ERROR, 1);
#endif

	if (select_list = pgSetupOffsetRun(pg_rec, selection, FALSE, TRUE)) {

		pgFillBlock(&stuff_to_change, sizeof(change_info), 0);
		stuff_to_change.pg = pg_rec;
		stuff_to_change.style_change = info;
		stuff_to_change.style_mask = mask;
	
		num_selects = (pg_short_t)GetMemorySize(select_list);
		select_run = (select_pair_ptr) UseMemory(select_list);
		stuff_to_change.change_range = *select_run;

		while (num_selects) {
			
			pgChangeStyleRun(pg_rec->t_style_run, &stuff_to_change, change_style_info,
					select_run->begin, select_run->end);
			
			if (select_run->begin < stuff_to_change.change_range.begin)
				stuff_to_change.change_range.begin = select_run->begin;
			if (select_run->end > stuff_to_change.change_range.end)
				stuff_to_change.change_range.end = select_run->end;
	
			pgApplyStylesToSubrefs(pg_rec, select_run, stuff_to_change.style_change,
						stuff_to_change.style_mask);

			++select_run;
			--num_selects;
		}
		
		pgFinishStyleUpdate(pg_rec, &stuff_to_change, select_list, TRUE, draw_mode);
	}
	else {   /* A single insertion exists here  */
		style_info			new_style, temp_mask;
		
		if (valid_insertion(pg_rec, selection)) {
			
			pgGetStyleInfo(pg_rec->myself, NULL, FALSE, &new_style, &temp_mask);
			pgSetFldsFromMask(&new_style, info, mask, SIGNIFICANT_STYLE_SIZE);
			new_style.procs.insert_proc(pg_rec, &new_style, pgCurrentInsertion(pg_rec));
			pg_rec->insert_style = pgAddStyleInfo(pg_rec, NULL, internal_clone_reason, &new_style);
			
		// Check to see if this is an empty pg_ref and if so, make this the default.
		
			if (pg_rec->t_length == 0 && !new_style.embed_object)
				if (new_style.embed_object == MEM_NULL && new_style.char_bytes == 0) {
				style_info_ptr		def_style;				
				style_run_ptr		def_run;
				t_select_ptr		select;
				text_block_ptr		block;
				pg_short_t			old_def_index, new_def_index;
				
				if (draw_mode && !pg_rec->num_selects) {
					
					select = (t_select_ptr) UseMemory(pg_rec->select);
					pgSetupGrafDevice(pg_rec, &pg_rec->port, MEM_NULL, clip_standard_verb);
					pg_rec->procs.cursor_proc(pg_rec, select, hide_cursor);
				}
				else
					select = NULL;

				def_style = (style_info_ptr)UseMemory(pg_rec->t_formats);
				def_run = (style_run_ptr)UseMemory(pg_rec->t_style_run);
				old_def_index = def_run->style_item;
				new_def_index = pg_rec->insert_style;
				
				def_style[new_def_index].used_ctr = def_style[old_def_index].used_ctr + 1;
				def_style[old_def_index].used_ctr -= 1;
				def_run->style_item = new_def_index;
				
				UnuseMemory(pg_rec->t_style_run);
				UnuseMemory(pg_rec->t_formats);
				
				block = (text_block_ptr) UseMemory(pg_rec->t_blocks);
				block->flags |= NEEDS_CALC;
				pgPaginateBlock(pg_rec, block, NULL, TRUE);
				UnuseMemory(pg_rec->t_blocks);
				
				if (select) {
					
					pg_rec->procs.cursor_proc(pg_rec, select, show_cursor);
					pg_rec->procs.set_device(pg_rec, unset_pg_device, &pg_rec->port, NULL);
					UnuseMemory(pg_rec->select);
				}
			}
		}
	}
}

/* pgBasicallyEqualStyles compares src to target returning true if they are basically equal. */

PG_PASCAL (pg_boolean) pgBasicallyEqualStyles (style_info_ptr src, style_info_ptr target)
{
	style_info			mask;

	pgFillBlock(&mask, SIGNIFICANT_STYLE_SIZE, 0);
	pgFillBlock(mask.styles, MAX_STYLES * sizeof(short), -1);
	pgFillBlock(&mask.fg_color, sizeof(color_value), -1);
	pgFillBlock(&mask.bk_color, sizeof(color_value), -1);
	mask.point = -1;

	mask.space_extra = mask.char_extra = -1;
	
	return	pgEqualStructMasked(src, target, &mask, SIGNIFICANT_STYLE_SIZE);
}

/* pgBasicallyEqualFonts returns true if the fonts are equal. */


PG_PASCAL (pg_boolean) pgBasicallyEqualFonts (font_info_ptr src, font_info_ptr target)
{
	return	pgEqualStruct(src->name, target->name, FONT_SIZE);
}


/******************************* Local Functions *******************************/


/* This is the "get" proc that is called for obtaining style info on a style record. */

static void get_style_info (change_info_ptr change, style_run_ptr style, pg_short_t record_ctr)
{
	register style_info_ptr			base_style;
	
	base_style = (style_info_ptr) change->base;
	base_style += style->style_item;
	
	if (change->any_match)
		pgSetMaskFromFlds(base_style, change->style_change, change->style_mask,
			style_compare, TRUE, FALSE);
	else {
	
		if (!record_ctr)
			pgBlockMove(base_style, change->style_change, sizeof(style_info));
		else
			pgSetMaskFromFlds(base_style, change->style_change, change->style_mask,
				style_compare, FALSE, FALSE);
	}
}


/* This is the change_proc for pgSetStyleInfo. (See style_change_proc for info
on these parameters)  */

static pg_short_t change_style_info (change_info_ptr change, style_run_ptr style)
{
	paige_rec_ptr				pg;
	style_info_ptr				change_mask;
	style_info					special_mask;
	style_info					new_style, old_style;
	
	pg = change->pg;
	
	GetMemoryRecord(pg->t_formats, style->style_item, &new_style);
	pgBlockMove(&new_style, &old_style, sizeof(style_info));

// GC Aug 12 95, need to prevent embed_refs from getting wiped out:
// GC Sept 14, need to prevent old stylesheets from getting 100% nuked

	change_mask = change->style_mask;	// Default mask, may change if embeds or old stylesheet
	
	if (pg->flags2 & STYLESHEET_CHANGE) {
		style_info_ptr			real_mask;
		
		if (old_style.embed_object) {
		
			real_mask = change->style_mask;
			pgFillBlock(&special_mask, sizeof(style_info), 0);
			pgBlockMove(real_mask->styles, special_mask.styles, MAX_STYLES * sizeof(short));
			special_mask.font_index = real_mask->font_index;
			special_mask.style_sheet_id = real_mask->style_sheet_id;
			special_mask.small_caps_index = real_mask->small_caps_index;
			special_mask.fg_color = real_mask->fg_color;
			special_mask.bk_color = real_mask->bk_color;
			special_mask.top_extra = real_mask->top_extra;
			special_mask.bot_extra = real_mask->bot_extra;
			special_mask.space_extra = real_mask->space_extra;
			special_mask.char_extra = real_mask->char_extra;
	
			change_mask = &special_mask;
		}
		else
		if ((pg->flags2 & STYLESHEET_CHANGE) && (old_style.style_sheet_id < 0)) {
			style_info_ptr		old_stylesheet;
			
			if (old_stylesheet = pgLocateStyleSheet(pg, (short)(-old_style.style_sheet_id), NULL)) {

				pgFillBlock(&special_mask, sizeof(style_info), 0);
				pgSetMaskFromFlds(old_stylesheet, &old_style, &special_mask, style_compare, TRUE, FALSE);
				special_mask.style_sheet_id = COMPARE_TRUE;
				change_mask = &special_mask;
				UnuseMemory(pg->t_formats);
			}
		}
	}

	if (pgSetFldsFromMask(&new_style, change->style_change, change_mask, SIGNIFICANT_STYLE_SIZE)) {
		
		++change->changed;
		old_style.procs.alter_style(pg, &old_style, &new_style, change_mask);

		return	pgAddStyleInfo(pg, NULL, internal_clone_reason, &new_style);
	}

	return	style->style_item;	/* no change if it gets here */
}


/* This one is used when the class_info is being changed */

static pg_short_t change_class_info (change_info_ptr change, style_run_ptr style)
{
	paige_rec_ptr				pg;
	style_info					new_style;
	long						old_class_bits;
	
	pg = change->pg;
	
	GetMemoryRecord(pg->t_formats, style->style_item, &new_style);
	
	old_class_bits = new_style.class_bits;
	new_style.class_bits &= change->class_AND;
	new_style.class_bits |= change->class_OR;
	
	if (new_style.class_bits != old_class_bits) {
		
		++change->changed;

		return	pgAddStyleInfo(pg, NULL, internal_clone_reason, &new_style);
	}

	return	style->style_item;	/* no change if it gets here */
}



/* This returns TRUE if the selection is a valid insertion point, i.e. is the
same as the current insertion of pg. */

static pg_boolean valid_insertion (paige_rec_ptr pg, select_pair_ptr selection)
{
	if (!selection)
		return	TRUE;
	
	return (pgCurrentInsertion(pg) == selection->begin);
}
