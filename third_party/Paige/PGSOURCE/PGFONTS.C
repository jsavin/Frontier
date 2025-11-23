/* This file handler high-level font "get/set" functions. If you don't change fonts
you can omit this file from your app.  */

#include "Paige.h"

#ifdef MAC_PLATFORM
#pragma segment pgfonts
#endif

#include "machine.h"
#include "defProcs.h"
#include "pgText.h"
#include "pgSelect.h"
#include "pgEdit.h"
#include "pgDefStl.h"
#include "pgUtils.h"
#include "pgStyles.h"
#include "pgErrors.h"


static void get_font_info (change_info_ptr change, style_run_ptr style, pg_short_t record_ctr);
static void copy_possible_alternate (font_info_ptr font);

/* pgGetFontInfo returns info about fonts for the specified selection. */

PG_PASCAL (long) pgGetFontInfo (pg_ref pg, const select_pair_ptr selection,
		pg_boolean set_any_match, font_info_ptr info, font_info_ptr mask)
{
	paige_rec_ptr				pg_rec;
	memory_ref					select_list;
	register pg_short_t			num_selects;
	register select_pair_ptr	select_run;
	change_info					stuff_to_report;
	style_info					text_styles;
	long						first_select;

	pg_rec = (paige_rec_ptr) UseMemory(pg);

	if (set_any_match)
		pgFillBlock(mask, sizeof(font_info), 0);
	else {
	
		pgFillBlock(info, sizeof(font_info), 0);
		pgFillBlock(mask, sizeof(font_info), -1);
	}

	if (select_list = pgSetupOffsetRun(pg_rec, selection, FALSE, TRUE)) {

		pgFillBlock(&stuff_to_report, sizeof(change_info), 0);
		
		stuff_to_report.any_match = set_any_match;
		
		stuff_to_report.pg = pg_rec;
		stuff_to_report.font_change = info;
		stuff_to_report.font_mask = mask;
		stuff_to_report.style_change = &text_styles;
		stuff_to_report.base = UseMemory(pg_rec->t_formats);
	
		num_selects = (pg_short_t)GetMemorySize(select_list);
		select_run = (select_pair_ptr) UseMemory(select_list);
		first_select = select_run->begin;

		while (num_selects) {

			pgReturnStyleInfo(pg_rec->t_style_run, &stuff_to_report, get_font_info,
					select_run->begin, select_run->end);
	
			++select_run;
			--num_selects;
		}
		
		UnuseMemory(pg_rec->t_formats);
		UnuseAndDispose(select_list);
		
		copy_possible_alternate(info);
	}
	else {	/* A single insertion point exists */
		style_info			current_style;
		font_info			current_font;
		short				font_index;

		first_select = pgCurrentInsertion(pg_rec);
		GetMemoryRecord(pg_rec->t_formats, pg_rec->insert_style, &current_style);
		
		if ((font_index = current_style.font_index) == DEFAULT_FONT_INDEX)
			font_index = 0;

		GetMemoryRecord(pg_rec->fonts, font_index, &current_font);
		copy_possible_alternate(&current_font);

		if (set_any_match)			
			pgSetMaskFromFlds(info, &current_style, mask, font_compare, TRUE, FALSE);
		else
			pgBlockMove(&current_font, info, SIGNIFICANT_FONT_SIZE);
	}

	UnuseMemory(pg);
	
	return	first_select;
}


/* pgSetFontInfo changes the font in the specified selection.  */

PG_PASCAL (void) pgSetFontInfo (pg_ref pg, const select_pair_ptr selection,
		const font_info_ptr info, const font_info_ptr mask, short draw_mode)
{
	paige_rec_ptr		pg_rec;
	font_info			new_font;
	style_info			new_style, new_mask;

	pg_rec = (paige_rec_ptr) UseMemory(pg);

#ifdef PG_DEBUG
	if (GetAccessCtr(pg_rec->fonts))
		pgFailure(pg_rec->globals->mem_globals, ILLEGAL_RE_ENTER_ERROR, 1);
	if (pg_rec->lock_id)
		pgFailure(pg_rec->globals->mem_globals, LOCKED_PG_ERROR, 1);
#endif

	GetMemoryRecord(pg_rec->fonts, 0, &new_font);

	if (mask->name[0]) {

		pgFillBlock(&mask->name[0], FONT_SIZE * sizeof(pg_char), SET_MASK_BITS);
	}
	else
		pgFillBlock(new_font.name, FONT_SIZE * sizeof(pg_char), 0);

	if (mask->alternate_name[0])
		pgFillBlock(&mask->alternate_name[0], FONT_SIZE * sizeof(pg_char), SET_MASK_BITS);
	else {
		pgFillBlock(mask->alternate_name, FONT_SIZE * sizeof(pg_char), SET_MASK_BITS);
		pgFillBlock(new_font.alternate_name, FONT_SIZE * sizeof(pg_char), 0);
	}

	pgSetFldsFromMask(&new_font, info, mask, SIGNIFICANT_FONT_SIZE);

	pgFillBlock(&new_style, sizeof(style_info), 0);
	pgFillBlock(&new_mask, sizeof(style_info), 0);

	new_style.font_index = pgAddNewFont(pg_rec, &new_font);
	new_mask.font_index = -1;
	pgChangeStyleInfo(pg_rec, selection, &new_style, &new_mask, draw_mode);

	UnuseMemory(pg);
}


/* pgSetStyleAndFont sets both font and style at the same time. Parameters
are otherwise the same as pgSetFontInfo and/or pgSetStyleInfo. */

PG_PASCAL (void) pgSetStyleAndFont (pg_ref pg, const select_pair_ptr selection,
		const style_info_ptr the_style, const style_info_ptr style_mask,
		const font_info_ptr font, const font_info_ptr font_mask, short draw_mode)
{
	paige_rec_ptr		pg_rec;
	font_info			new_font;
	style_info			new_style, new_mask;

	pg_rec = (paige_rec_ptr) UseMemory(pg);

#ifdef PG_DEBUG
	if (GetAccessCtr(pg_rec->fonts))
		pgFailure(pg_rec->globals->mem_globals, ILLEGAL_RE_ENTER_ERROR, 1);
	if (pg_rec->lock_id)
		pgFailure(pg_rec->globals->mem_globals, LOCKED_PG_ERROR, 1);
#endif

	GetMemoryRecord(pg_rec->fonts, 0, &new_font);

	if (font_mask->name[0]) {
		
		new_font.environs |= FONT_GOOD;		/* Default to name is good */
		pgFillBlock(&font_mask->name[0], FONT_SIZE * sizeof(pg_char), -1);
	}

	pgSetFldsFromMask(&new_font, font, font_mask, SIGNIFICANT_FONT_SIZE);
	pgBlockMove(the_style, &new_style, sizeof(style_info));
	pgBlockMove(style_mask, &new_mask, sizeof(style_info));
	
	new_style.font_index = pgAddNewFont(pg_rec, &new_font);
	new_mask.font_index = -1;
	pgChangeStyleInfo(pg_rec, selection, &new_style, &new_mask, draw_mode);

	UnuseMemory(pg);
}


/* pgGetFontTable returns the memory_ref to pg's font table */

PG_PASCAL (memory_ref) pgGetFontTable (pg_ref pg)
{
	paige_rec_ptr	pg_rec;
	memory_ref		font_table;
	
	pg_rec = (paige_rec_ptr) UseMemory(pg);
	font_table = pg_rec->fonts;
	UnuseMemory(pg);
	
	return	font_table;
}


/* pgGetFontInfoRec returns the font_index'th font record  */

PG_PASCAL (void) pgGetFontInfoRec (pg_ref pg, short font_index, font_info_ptr info)
{
	paige_rec_ptr			pg_rec;
	short					use_font_index;
	
	if ((use_font_index = font_index) == DEFAULT_FONT_INDEX)
		use_font_index = 0;
	
	pg_rec = (paige_rec_ptr) UseMemory(pg);
	GetMemoryRecord(pg_rec->fonts, use_font_index, info);
	UnuseMemory(pg);
}


/* pgCrossFont returns what font index in new_pg would be used to obtain the
same exact font as src_index of src_pg. If no match, -1 is returned. */

PG_PASCAL (short) pgCrossFont (pg_ref src_pg, pg_ref new_pg, short src_index)
{
	memory_ref			src_ref, target_ref;
	font_info			src_font;
	font_info_ptr		target_fonts;
	short				target_qty, result, ctr;
	
	src_ref = pgGetFontTable(src_pg);
	target_ref = pgGetFontTable(new_pg);
	target_qty = (short)GetMemorySize(target_ref);
	
	result = -1;
	
	GetMemoryRecord(src_ref, src_index, &src_font);
	
	for (target_fonts = (font_info_ptr) UseMemory(target_ref), ctr = 0; target_qty;
			++target_fonts, ++ctr, --target_qty)
		if (pgEqualStruct(target_fonts->name, src_font.name, FONT_SIZE)) {
			
			result = ctr;
			break;
		}
	
	UnuseMemory(target_ref);
	
	return	result;
}


/* pgAddCrossFont changes style->font_index to the proper font index after the style has
been copied from source_pg to target_pg. The purpose of this function is to make sure
the font actually exists in target_pg and adding it if necessary. The function returns
the new font index. */

PG_PASCAL (short) pgAddCrossFont (paige_rec_ptr source_pg, paige_rec_ptr target_pg,
		style_info_ptr style)
{
	font_info			cross_font;
	
	GetMemoryRecord(source_pg->fonts, style->font_index, &cross_font);
	
	return	pgAddNewFont(target_pg, &cross_font);
}


/******************************** Local Functions ****************************/



/* This is the function the compares font_info for the "get" process */

static void get_font_info (change_info_ptr change, style_run_ptr style, pg_short_t record_ctr)
{
	register style_info_ptr			base_style;
	font_info						style_font;
	short							font_index;

	base_style = (style_info_ptr) change->base;
	base_style += style->style_item;
	
	if ((font_index = base_style->font_index) == DEFAULT_FONT_INDEX)
		font_index = 0;

	GetMemoryRecord(change->pg->fonts, font_index, &style_font);

	if (change->any_match) {
		
		if (pgEqualStruct(&style_font, change->font_change, FONT_SIZE))
			pgFillBlock(change->font_mask, FONT_SIZE * sizeof(pg_char), -1);

		pgSetMaskFromFlds(&style_font, change->font_change, change->font_mask,
				font_compare, TRUE, FALSE);
	}
	else {
	
		if (!record_ctr)
			pgBlockMove(&style_font, change->font_change, sizeof(font_info));
		else {

			pgSetMaskFromFlds(&style_font, change->font_change, change->font_mask,
				font_compare, FALSE, FALSE);
			if (!pgEqualStruct(&style_font, change->font_change, FONT_SIZE))
				pgFillBlock(change->font_mask, FONT_SIZE * sizeof(pg_char), 0);
		}
	}
}


/* copy_possible_alternate checks to see if we should use the alternate name and if so,
copies to the main name. */

static void copy_possible_alternate (font_info_ptr font)
{
	if (font->environs & FONT_USES_ALTERNATE)
		pgBlockMove(font->alternate_name, font->name, FONT_SIZE * sizeof(pg_char));
}
