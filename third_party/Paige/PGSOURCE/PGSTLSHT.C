/* This file handles "style sheets."  */

#include "Paige.h"

#ifdef MAC_PLATFORM
#pragma segment pgstlsht
#endif

#include "pgEdit.h"
#include "pgDefStl.h"
#include "pgStyles.h"
#include "pgPar.h"
#include "pgUtils.h"
#include "pgOSUtl.h"
#include "pgtxr.h"

static short count_style_sheets (paige_rec_ptr pg, short PG_FAR *highest_id);
static short count_par_style_sheets (paige_rec_ptr pg, short PG_FAR *highest_id);
static void affected_text_range (paige_rec_ptr pg, short compare_type,
		short stylesheet_id, select_pair_ptr range);
static pg_boolean compare_stylename (pg_c_string_ptr name1, pg_c_string_ptr name2);
static long new_named_style (pg_ref pg, pg_c_string_ptr stylename, const style_info_ptr style,
		const font_info_ptr font, par_info_ptr par);
static void rename_style (pg_ref pg, long named_style_index, pg_c_string_ptr style_name);
static void delete_named_style (pg_ref pg, long named_style_index);
static void merge_named_style (pg_ref pg, pg_c_string_ptr source_style,
		pg_c_string_ptr target_style, short draw_mode);
static void change_stylesheet_id (paige_rec_ptr pg_rec, short style_id, short new_style_id,
			long new_named_id);
static void change_par_stylesheet_id (paige_rec_ptr pg_rec, short style_id, short new_style_id,
			long new_named_id);


/* pgNewStyle creates a new style sheet record and returns a unique identifier */

PG_PASCAL (short) pgNewStyle (pg_ref pg, const style_info_ptr new_style, const font_info_ptr style_font)
{
	paige_rec_ptr				pg_rec;
	style_info					style_to_add;
	font_info					font_to_add;
	short						new_id;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	pgBlockMove(new_style, &style_to_add, sizeof(style_info));
	
	count_style_sheets(pg_rec, &new_id);
	++new_id;
	style_to_add.style_sheet_id = new_id;
	style_to_add.maintenance |= IS_STYLE_SHEET;

	if (style_font) {
		
		pgBlockMove(style_font, &font_to_add, sizeof(font_info));
		style_to_add.font_index = pgAddNewFont(pg_rec, &font_to_add);
	}

	pgAddStyleInfo(pg_rec, NULL, new_stylesheet_reason, &style_to_add);
	++pg_rec->change_ctr;
	
	UnuseMemory(pg);

	return		new_id;
}


/* pgRemoveStyle removes the style sheet style_id.  (Actually, the style won't
get physically removed until it is no longer used by any text).  After this,
the style_id will no longer be recognized.   */

PG_PASCAL (void) pgRemoveStyle (pg_ref pg, short style_id)
{
	paige_rec_ptr			pg_rec;
	style_info_ptr			the_style;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	
	if (the_style = pgLocateStyleSheet(pg_rec, style_id, NULL)) {
		
		--the_style->used_ctr;
		the_style->style_sheet_id = 0;
		
		UnuseMemory(pg_rec->t_formats);
	}
	
	++pg_rec->change_ctr;

	UnuseMemory(pg);
}


/* pgNumStyles returns the total number of style sheets available.  */

PG_PASCAL (short) pgNumStyles (pg_ref pg)
{
	paige_rec_ptr				pg_rec;
	short						num_styles;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	num_styles = count_style_sheets(pg_rec, NULL);
	UnuseMemory(pg);
	
	return		num_styles;
}


/* pgMaxStylesheetID returns the highest style id used.  */

PG_PASCAL (short) pgMaxStylesheetID (pg_ref pg)
{
	paige_rec_ptr				pg_rec;
	short						num_styles;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	count_style_sheets(pg_rec, &num_styles);
	UnuseMemory(pg);
	
	return		num_styles;
}


/* pgGetStyle returns the whole style record belonging to style sheet style_id.
However, if the style is not found the function returns FALSE and style is
not changed.  */

PG_PASCAL (pg_boolean) pgGetStyle (pg_ref pg, short style_id, style_info_ptr style)
{
	paige_rec_ptr			pg_rec;
	style_info_ptr			the_style;
	short					result;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	
	if (the_style = pgLocateStyleSheet(pg_rec, style_id, NULL)) {
		
		result = TRUE;
		pgBlockMove(the_style, style, sizeof(style_info));
		UnuseMemory(pg_rec->t_formats);
	}
	else
		result = FALSE;

	UnuseMemory(pg);
	
	return	result;
}

/* pgChangeStyle changes the whole style record for style_id to style and
style_font.  */

PG_PASCAL (void) pgChangeStyle (pg_ref pg, short style_id, const style_info_ptr style,
		const font_info_ptr style_font, short draw_mode)
{
	paige_rec_ptr			pg_rec;
	style_info_ptr			the_style, each_style, caps_style;
	static style_info		old_style, new_caps_style, mask;
	font_info				font_to_add;
	select_pair				range_affected;
	long					qty, change_index, old_caps_index, new_caps_index;
	short					negative_style_id, font_index, use_draw_mode;
	//QUALCOMM Begin == Kusuma
	pg_boolean				convertFont;
	//QUALCOMM End == Kusuma

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	
	if (the_style = pgLocateStyleSheet(pg_rec, style_id, NULL)) {
		
		pgBlockMove(the_style, &old_style, SIGNIFICANT_STYLE_SIZE);
		pgBlockMove(style, the_style, SIGNIFICANT_STYLE_SIZE);
		the_style->style_sheet_id = style_id;

		if (style_font) {
			
			pgBlockMove(style_font, &font_to_add, sizeof(font_info));
			the_style->font_index = pgAddNewFont(pg_rec, &font_to_add);
		}
		
		if ((font_index = the_style->font_index) == DEFAULT_FONT_INDEX)
			font_index = 0;

		the_style->procs.init(pg_rec, the_style, (font_info_ptr) UseMemoryRecord(pg_rec->fonts, font_index, 0, TRUE));
		UnuseMemory(pg_rec->fonts);

/* Now walk through all style_info's and locate any "offspring" from this
original stylesheet. If found, I need to change original fields. */

		each_style = (style_info_ptr) UseMemoryRecord(pg_rec->t_formats, 0, USE_ALL_RECS, FALSE);
		negative_style_id = -style_id;
		qty = GetMemorySize(pg_rec->t_formats);
		
		for (change_index = 0; change_index < qty; ++each_style, ++change_index) {
			
			if (each_style->style_sheet_id == negative_style_id && !each_style->embed_object) {
				
				if (each_style->styles[small_caps_var])
					old_caps_index = each_style->small_caps_index + 1;
				else
					old_caps_index = 0;

				pgFillBlock(&mask, SIGNIFICANT_STYLE_SIZE, COMPARE_TRUE);
				pgSetMaskFromFlds(&old_style, each_style, &mask, style_compare, FALSE, FALSE);
				//QUALCOMM Begin == Kusuma
				convertFont = TRUE;
				//If we had set the user_data bit, we don't want to change the font back
				if (each_style->user_data & 1)
				{
					convertFont = FALSE;
					each_style->user_data &= ~1;
				}
				if ( ((each_style->font_index != old_style.font_index) &&
					 (each_style->font_index != the_style->font_index)) || !convertFont)
				{
					mask.font_index = COMPARE_FALSE;
					mask.ascent = COMPARE_FALSE;
					mask.descent = COMPARE_FALSE;
					mask.leading = COMPARE_FALSE;
					mask.machine_var = COMPARE_FALSE;
					mask.machine_var2 = COMPARE_FALSE;
					mask.point = COMPARE_FALSE;
				}
				//Was the font already in the new font? Then set the user_data bit so we don't
				//lose it when changing back
				if (each_style->font_index == the_style->font_index)
				{
					//Do it only if it is not called by BlahBlahBlah
					if (each_style->styles[hidden_text_var] == the_style->styles[hidden_text_var])
					{
						each_style->user_data |= 1;
						mask.user_data = COMPARE_FALSE;
					}
				}
				//QUALCOMM End == Kusuma
				mask.style_sheet_id = 0;
				pgSetFldsFromMask(each_style, the_style, &mask, SIGNIFICANT_STYLE_SIZE);
				//QUALCOMM Begin == Kusuma
				if (each_style->font_index == the_style->font_index)
				//QUALCOMM End == Kusuma
				each_style->procs.init(pg_rec, each_style, (font_info_ptr) UseMemoryRecord(pg_rec->fonts, font_index, 0, TRUE));
				UnuseMemory(pg_rec->fonts);
				
				if (each_style->styles[small_caps_var])
					new_caps_index = each_style->small_caps_index + 1;
				else
					new_caps_index = 0;
				
				if ((new_caps_index != old_caps_index) && new_caps_index) {
					
					if (old_caps_index) {
						
						caps_style = (style_info_ptr) UseMemoryRecord(pg_rec->t_formats, old_caps_index - 1, 0, FALSE);
						caps_style->used_ctr -= 1;
					}
					
					new_caps_style = *each_style;
					UnuseMemory(pg_rec->t_formats);
					
					new_caps_index = pgAddSmallCapsStyle(pg_rec, NULL, internal_clone_reason, &new_caps_style);
					each_style = (style_info_ptr) UseMemoryRecord(pg_rec->t_formats, change_index, USE_ALL_RECS, TRUE);
					each_style->small_caps_index = new_caps_index;
					caps_style = (style_info_ptr) UseMemoryRecord(pg_rec->t_formats, new_caps_index, 0, FALSE);
					caps_style->used_ctr += 1;
				}
			}
		}

		UnuseMemory(pg_rec->t_formats);
		
		++pg_rec->change_ctr;

		affected_text_range(pg_rec, style_compare, style_id, &range_affected);
		
		if (range_affected.end > range_affected.begin) {

			pgInvalSelect(pg, range_affected.begin, range_affected.end);

			if (use_draw_mode = draw_mode) {
				
				if (draw_mode == best_way)
					use_draw_mode = bits_copy;

				pgUpdateText(pg_rec, NULL, 0, pg_rec->t_length, MEM_NULL, NULL, use_draw_mode, TRUE);
			}
		}
	}

	UnuseMemory(pg);
}



/* pgSetStyleSheet changes the text range specieid to the style sheet. */

PG_PASCAL (void) pgSetStyleSheet (pg_ref pg, const select_pair_ptr selection,
		short style_id, short draw_mode)
{
	paige_rec_ptr			pg_rec;
	style_info_ptr			style_sheet;
	style_info				info, mask;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	
	if (style_sheet = pgLocateStyleSheet(pg_rec, style_id, NULL)) {

		pgBlockMove(style_sheet, &info, sizeof(style_info));
		UnuseMemory(pg_rec->t_formats);
		pgFillBlock(&mask, sizeof(style_info), -1);
		pg_rec->flags2 |= STYLESHEET_CHANGE;
		pgChangeStyleInfo(pg_rec, selection, &info, &mask, draw_mode);
		pg_rec->flags2 &= (~STYLESHEET_CHANGE);
	}

	UnuseMemory(pg);
}


/* pgGetStyleSheet returns the style sheet ID belonging to the text range
specified.  */

PG_PASCAL (pg_boolean) pgGetStyleSheet (pg_ref pg, const select_pair_ptr selection, short PG_FAR *style_id)
{
	style_info				info, mask;

	pgGetStyleInfo(pg, selection, FALSE, &info, &mask);
	
	if (info.style_sheet_id < 0)
		info.style_sheet_id = -info.style_sheet_id;

	if (style_id)
		*style_id = (short)info.style_sheet_id;
	
	return	(pg_boolean)(mask.style_sheet_id != 0);
}


/* pgFindStyleSheet looks for any stylesheet that matches style. If found the
stylesheet ID is returned (otherwise 0 is returned).  */

PG_PASCAL (short) pgFindStyleSheet (pg_ref pg, const style_info_ptr compare_style,
		const style_info_ptr mask)
{
	paige_rec_ptr				pg_rec;
	style_info					compare_mask;
	register style_info_ptr		target;
	register long				style_qty;
	short						result;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	result = 0;
	
	if (mask)
		pgBlockMove(mask, &compare_mask, SIGNIFICANT_STYLE_SIZE);
	else
		pgFillBlock(&compare_mask, SIGNIFICANT_STYLE_SIZE, COMPARE_TRUE);

	compare_mask.style_sheet_id = 0;
	compare_mask.ascent = compare_mask.descent = compare_mask.leading = 0;
	compare_mask.machine_var = compare_mask.machine_var2 = 0;
	compare_mask.future[0] = compare_mask.future[1] = compare_mask.future[2] = 0;

	target = (style_info_ptr) UseMemory(pg_rec->t_formats);
	
	for (style_qty = GetMemorySize(pg_rec->t_formats); style_qty; ++target, --style_qty)
		if (target->style_sheet_id > 0) {

			if (pgStyleMatchesCriteria(target, compare_style, &compare_mask,
					NULL, SIGNIFICANT_STYLE_SIZE))
				if ((result = (short)target->style_sheet_id) != 0)
					break;
		}

	UnuseMemory(pg_rec->t_formats);
	UnuseMemory(pg);

	return	result;
}


/* pgGetIndStyleSheet returns the nth stylesheet's ID. If stylesheet ptr is not
NULL, it is filled in with the style. */

PG_PASCAL (short) pgGetIndStyleSheet (pg_ref pg, short index, style_info_ptr stylesheet)
{
	paige_rec_ptr				pg_rec;
	register style_info_ptr		styles;
	register short				style_qty, ctr;
	short						result;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	result = 0;

	for (styles = (style_info_ptr) UseMemory(pg_rec->t_formats), style_qty = (pg_short_t)GetMemorySize(pg_rec->t_formats),
			ctr = 0; style_qty; ++styles, --style_qty)
		if (styles->style_sheet_id > 0) {
			
			if (ctr == index) {
				
				result = (short)styles->style_sheet_id;
				if (stylesheet)
					pgBlockMove(styles, stylesheet, sizeof(style_info));
				
				break;
			}

			++ctr;
		}

	UnuseMemory(pg_rec->t_formats);
	UnuseMemory(pg);
	
	return	result;
}



/* pgNewParStyle is the same as pgNewStyle except for paragraphs.  */

PG_PASCAL (short) pgNewParStyle (pg_ref pg, const par_info_ptr new_style)
{
	paige_rec_ptr				pg_rec;
	par_info					style_to_add;
	short						new_id;

	pg_rec = (paige_rec_ptr) UseMemory(pg);

	pgBlockMove(new_style, &style_to_add, sizeof(par_info));

	count_par_style_sheets(pg_rec, &new_id);
	++new_id;
	style_to_add.style_sheet_id = new_id;
	style_to_add.maintenance |= IS_STYLE_SHEET;

	pgAddParInfo(pg_rec, NULL, new_stylesheet_reason, &style_to_add);
	
	++pg_rec->change_ctr;

	UnuseMemory(pg);

	return		new_id;
}


/* pgRemoveParStyle is the same as pgRemoveStyle except for paragraphs.  */

PG_PASCAL (void) pgRemoveParStyle (pg_ref pg, short style_id)
{
	paige_rec_ptr			pg_rec;
	par_info_ptr			the_style;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	
	if (the_style = pgLocateParStyleSheet(pg_rec, style_id)) {
		
		--the_style->used_ctr;
		the_style->style_sheet_id = 0;
		
		UnuseMemory(pg_rec->par_formats);
	}
	
	++pg_rec->change_ctr;
	UnuseMemory(pg);
}


/* pgNumParStyles is the same as pgNumStyles except for paragraphs.  */

PG_PASCAL (short) pgNumParStyles (pg_ref pg)
{
	paige_rec_ptr				pg_rec;
	short						num_styles;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	num_styles = count_par_style_sheets(pg_rec, NULL);
	UnuseMemory(pg);
	
	return		num_styles;
}


/* pgMaxParStylesheetID is the same as pgMaxStylesheetID except for paragraphs.  */

PG_PASCAL (short) pgMaxParStylesheetID (pg_ref pg)
{
	paige_rec_ptr				pg_rec;
	short						num_styles;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	count_par_style_sheets(pg_rec, &num_styles);
	UnuseMemory(pg);
	
	return		num_styles;
}


/* pgGetParStyle is the same as pgGetStyle except for paragraphs.  */

PG_PASCAL (pg_boolean) pgGetParStyle (pg_ref pg, short style_id, par_info_ptr style)
{
	paige_rec_ptr			pg_rec;
	par_info_ptr			the_style;
	pg_boolean				result;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	
	if (the_style = pgLocateParStyleSheet(pg_rec, style_id)) {
		
		result = TRUE;
		pgBlockMove(the_style, style, sizeof(par_info));
		UnuseMemory(pg_rec->par_formats);
	}
	else
		result = FALSE;

	UnuseMemory(pg);
	
	return	result;
}


/* pgChangeParStyle is the same as pgChangeStyle except for paragraphs.  */

PG_PASCAL (void) pgChangeParStyle (pg_ref pg, short style_id, const par_info_ptr style,
		short draw_mode)
{
	paige_rec_ptr			pg_rec;
	par_info_ptr			the_style, each_style;
	par_info				old_style, mask;
	select_pair				range_affected;
	long					qty;
	short					negative_style_id, use_draw_mode;

	pg_rec = (paige_rec_ptr) UseMemory(pg);

	if (the_style = pgLocateParStyleSheet(pg_rec, style_id)) {
		
		pgBlockMove(the_style, &old_style, SIGNIFICANT_PAR_STYLE_SIZE);
		pgBlockMove(style, the_style, SIGNIFICANT_PAR_STYLE_SIZE);
		the_style->style_sheet_id = style_id;

/* Now walk through all par_info's and locate any "offspring" from this
original stylesheet. If found, I need to change original fields. */
		
		each_style = (par_info_ptr) UseMemoryRecord(pg_rec->par_formats, 0, USE_ALL_RECS, FALSE);
		negative_style_id = -style_id;

		for (qty = GetMemorySize(pg_rec->par_formats); qty; ++each_style, --qty) {
			
			if (each_style->style_sheet_id == negative_style_id) {
				
				pgFillBlock(&mask, SIGNIFICANT_PAR_STYLE_SIZE, COMPARE_TRUE);
				pgSetMaskFromFlds(&old_style, each_style, &mask, par_compare, FALSE, TRUE);
				mask.style_sheet_id = 0;
				pgSetFldsFromMask(each_style, the_style, &mask, SIGNIFICANT_PAR_STYLE_SIZE);
			}
		}

		UnuseMemory(pg_rec->par_formats);
		
		++pg_rec->change_ctr;

		affected_text_range(pg_rec, par_compare, style_id, &range_affected);
		
		if (range_affected.end > range_affected.begin) {

			pgInvalSelect(pg, range_affected.begin, range_affected.end);

			if (use_draw_mode = draw_mode) {
				
				if (draw_mode == best_way)
					use_draw_mode = bits_copy;
	
				pgUpdateText(pg_rec, NULL, 0, pg_rec->t_length, MEM_NULL, NULL, use_draw_mode, TRUE);
			}
		}
	}

	UnuseMemory(pg);
}


/* pgSetParStyleSheet is the same as pgSetStyleSheet except for paragraphs.  */

PG_PASCAL (void) pgSetParStyleSheet (pg_ref pg, const select_pair_ptr selection,
		short style_id, short draw_mode)
{
	paige_rec_ptr			pg_rec;
	par_info_ptr			style_sheet;
	par_info				info, mask;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	
	if (style_sheet = pgLocateParStyleSheet(pg_rec, style_id)) {

		pgBlockMove(style_sheet, &info, sizeof(par_info));
		UnuseMemory(pg_rec->par_formats);
		pgFillBlock(&mask, sizeof(par_info), -1);
		pg_rec->flags2 |= STYLESHEET_CHANGE;
		pgSetParInfoEx(pg, selection, &info, &mask, FALSE, draw_mode);
		pg_rec->flags2 &= (~STYLESHEET_CHANGE);
	}

	UnuseMemory(pg);
}


/* pgGetParStyleSheet is the same as pgGetStyleSheet except for paragraphs.  */

PG_PASCAL (pg_boolean) pgGetParStyleSheet (pg_ref pg, const select_pair_ptr selection, short PG_FAR *style_id)
{
	par_info				info, mask;

	pgGetParInfo(pg, selection, FALSE, &info, &mask);

	if (info.style_sheet_id < 0)
		info.style_sheet_id = -info.style_sheet_id;

	if (style_id)
		*style_id = (short)info.style_sheet_id;
	
	return	(pg_boolean)(mask.style_sheet_id != 0);
}


/* pgFindParStyleSheet looks for any stylesheet that matches style. If found the
stylesheet ID is returned (otherwise 0 is returned).  */

PG_PASCAL (short) pgFindParStyleSheet (pg_ref pg, const par_info_ptr compare_style,
		const par_info_ptr mask)
{
	paige_rec_ptr				pg_rec;
	par_info					compare_mask;
	register par_info_ptr		target;
	register long				style_qty;
	short						result;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	result = 0;
	
	if (mask)
		pgBlockMove(mask, &compare_mask, SIGNIFICANT_PAR_STYLE_SIZE);
	else
		pgFillBlock(&compare_mask, SIGNIFICANT_PAR_STYLE_SIZE, COMPARE_TRUE);
	
	compare_mask.style_sheet_id = 0;

	target = (par_info_ptr) UseMemory(pg_rec->par_formats);
	
	for (style_qty = GetMemorySize(pg_rec->par_formats); style_qty; ++target, --style_qty)
		if (target->style_sheet_id > 0) {

			if (pgStyleMatchesCriteria(target, compare_style, &compare_mask,
					NULL, SIGNIFICANT_PAR_STYLE_SIZE)) {
				
				result = (short)target->style_sheet_id;
				break;
			}
		}

	UnuseMemory(pg_rec->par_formats);
	UnuseMemory(pg);

	return	result;
}

/* pgGetIndParStyleSheet returns the nth stylesheet's ID. If stylesheet ptr is not
NULL, it is filled in with the style. */


PG_PASCAL (short) pgGetIndParStyleSheet (pg_ref pg, short index, par_info_ptr stylesheet)
{
	paige_rec_ptr				pg_rec;
	register par_info_ptr		styles;
	register short				style_qty, ctr;
	short						result;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	result = 0;

	for (styles = (par_info_ptr) UseMemory(pg_rec->par_formats), style_qty = (pg_short_t)GetMemorySize(pg_rec->par_formats),
			ctr = 0; style_qty; ++styles, --style_qty)
		if (styles->style_sheet_id > 0) {
			
			if (ctr == index) {
				
				result = (short)styles->style_sheet_id;
				if (stylesheet)
					pgBlockMove(styles, stylesheet, sizeof(par_info));
				
				break;
			}

			++ctr;
		}

	UnuseMemory(pg_rec->par_formats);
	UnuseMemory(pg);
	
	return	result;
}


/* This is identical to pgLocateStyleSheet except for paragraphs.  */

PG_PASCAL (par_info_ptr) pgLocateParStyleSheet (paige_rec_ptr pg, short style_id)
{
	register par_info_ptr		styles;
	register pg_short_t			style_qty;

	if (!style_id)
		return	NULL;

	styles = (par_info_ptr) UseMemory(pg->par_formats);

	for (style_qty = (pg_short_t)GetMemorySize(pg->par_formats); style_qty; ++styles, --style_qty)
		if (styles->style_sheet_id == style_id)
			return	styles;
	
	UnuseMemory(pg->par_formats);

	return	NULL;
}


/********** NAMED STYLES (2.0 implementation) ***********/

/* pgNewNamedStyle creates a new named stylesheet of stylename. If the name already exists, the
style(s) are changed to style, font and par.  The named style index is returned. */


PG_PASCAL (long) pgNewNamedStyle (pg_ref pg, pg_c_string_ptr stylename, const style_info_ptr style,
		const font_info_ptr font, par_info_ptr par)
{
	paige_rec_ptr		pg_rec;
	long				index;
	
	pg_rec = (paige_rec_ptr) UseMemory(pg);
	index = new_named_style(pg, stylename, style, font, par);
	
	if (pg_rec->global_styles)
		new_named_style(pg_rec->global_styles, stylename, style, font, par);
	
	UnuseMemory(pg);
	
	return	index;
}

/* pgNewNamedStyleMerge is the same as pgNewNamedStyle except a new name is not added if
the same style, font and par already exist as a named style. If one exists then that index ID
is returned AND stylename is set to that existing name. */

PG_PASCAL (long) pgNewNamedStyleMerge (pg_ref pg, pg_c_string_ptr stylename, const style_info_ptr style,
		const font_info_ptr font, par_info_ptr par)
{
	long			existing_index;
	
	if ((existing_index = pgFindSameNamedStyle(pg, style, font, par, 0)) > 0) {
		named_stylesheet		stylesheet;
		long					str_size;
		
		pgGetNamedStyle(pg, existing_index, &stylesheet);
		str_size = pgCStrLength(stylesheet.name);
		
		if (str_size)
			pgBlockMove(stylesheet.name, stylename, str_size);
		
		stylename[str_size] = 0;
		
		return	existing_index;
	}

	return	pgNewNamedStyle(pg, stylename, style, font, par);
}

/* pgMergeNamedStyles merges source_index style into target_style. That is, all occurrences
of source_style in the pg_ref (AND global ref, if any) are changed to target_index
and source_style is removed from the list. */

extern PG_PASCAL (void) pgMergeNamedStyles (pg_ref pg, pg_c_string_ptr source_style,
		pg_c_string_ptr target_style, short draw_mode)
{
	paige_rec_ptr		pg_rec;
	
	pg_rec = (paige_rec_ptr) UseMemory(pg);
	
	if (pg_rec->global_styles)
		merge_named_style(pg_rec->global_styles, source_style, target_style, draw_none);
	
	merge_named_style(pg, source_style, target_style, draw_mode);
	
	UnuseMemory(pg);
}

/* pgAddNamedStyle associates an existing stylesheet to a new named stylesheet of stylename. 
	If the name already exists, the style(s) are changed to style and par.  The named 
	style index is returned. DO NOT CALL THIS FOR GLOBAL STYLESHEET IMPLEMENTATIONS. */

PG_PASCAL (long) pgAddNamedStyle (pg_ref pg, pg_c_string_ptr stylename, const short style_id, const short par_id)
{
	paige_rec_ptr			pg_rec;
	named_stylesheet_ptr	named_style;
	style_info_ptr			style_ptr;
	par_info_ptr			par_ptr;
	long					style_index;
	short					name_index;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	
	if ((style_index = pgGetNamedStyleIndex(pg, stylename)) > 0) {
		style_info_ptr		old_style;
		par_info_ptr		old_par;

		named_style = (named_stylesheet_ptr) UseMemoryRecord(pg_rec->named_styles, style_index - 1, 0, TRUE);

		if (named_style->stylesheet_id)
			if (old_style = pgLocateStyleSheet(pg_rec, named_style->stylesheet_id, NULL)) {

				old_style->used_ctr -= 1;
				UnuseMemory(pg_rec->t_formats);
		}
		
		if (named_style->par_stylesheet_id)
			if (old_par = pgLocateParStyleSheet(pg_rec, named_style->par_stylesheet_id)) {

				old_par->used_ctr -= 1;
				UnuseMemory(pg_rec->par_formats);
		}
	}
	else {
		
		named_style = (named_stylesheet_ptr) AppendMemory(pg_rec->named_styles, 1, TRUE);
		style_index = GetMemorySize(pg_rec->named_styles);
		
		for (name_index = 0; name_index < (FONT_SIZE - 1); ++name_index)
			if ((named_style->name[name_index] = stylename[name_index]) == 0)
				break;
	}
	
	if (style_id) {
	
		named_style->stylesheet_id = style_id;
		style_ptr = pgLocateStyleSheet(pg_rec, style_id, NULL);
		style_ptr->named_style_index = style_index;
		UnuseMemory(pg_rec->t_formats);
	}

	if (par_id) {
	
		named_style->par_stylesheet_id = par_id;
		par_ptr = pgLocateParStyleSheet(pg_rec, par_id);
		par_ptr->named_style_index = style_index;
		UnuseMemory(pg_rec->par_formats);
 	}
 
 	UnuseMemory(pg_rec->named_styles);
	UnuseMemory(pg);
	
	return		style_index;
}


/* pgApplyNamedStyle applies the named stylesheet to the specified selection. */

PG_PASCAL (void) pgApplyNamedStyle (pg_ref pg, select_pair_ptr selection, pg_char_ptr stylename, short draw_mode)
{
	long					style_index;
	
	if ((style_index = pgGetNamedStyleIndex(pg, stylename)))
		pgApplyNamedStyleIndex(pg, selection, style_index, draw_mode);
}


/* pgApplyNamedStyleIndex applies style index "index" to the specified selection. The whole
selection changes to the style and-or par format. */

PG_PASCAL (void) pgApplyNamedStyleIndex (pg_ref pg, select_pair_ptr selection, long index, short draw_mode)
{
	named_stylesheet		named_style;
	short					style_draw_mode;

	if (pgGetNamedStyle(pg, index, &named_style)) {
		
		if (!named_style.par_stylesheet_id)
			style_draw_mode = draw_mode;
		else
			style_draw_mode = draw_none;
		
		if (named_style.stylesheet_id)
			pgSetStyleSheet(pg, selection, named_style.stylesheet_id, style_draw_mode);
		if (named_style.par_stylesheet_id)
			pgSetParStyleSheet(pg, selection, named_style.par_stylesheet_id, draw_mode);
	}
}


/* pgNumNamedStyles returns the number of named stylesheets */

PG_PASCAL (long) pgNumNamedStyles (pg_ref pg)
{
	paige_rec_ptr			pg_rec;
	long					num_styles;
	
	pg_rec = (paige_rec_ptr) UseMemory(pg);
	num_styles = GetMemorySize(pg_rec->named_styles);
	UnuseMemory(pg);
	
	return		num_styles;
}


/* pgGetNamedStyleIndex returns a named style index that matches stylename,
or zero if not found.  Or, if stylename is NULL the index, if any, is returned for
the current selection. */

PG_PASCAL (long) pgGetNamedStyleIndex (pg_ref pg, pg_c_string_ptr stylename)
{
	paige_rec_ptr					pg_rec;
	register named_stylesheet_ptr	named_style;
	long							index_result, num_styles, style_index;
	
	if (!stylename) {
		style_info			info, mask;
		par_info			parinfo, parmask;

		pgGetStyleInfo(pg, NULL, FALSE, &info, &mask);
		
		if (info.named_style_index == 0 || mask.named_style_index == 0) {
			
			info.named_style_index = 0;
			pgGetParInfo(pg, NULL, FALSE, &parinfo, &parmask);
			
			if (parmask.named_style_index != 0)
				info.named_style_index = parinfo.named_style_index;
		}
		
		return	info.named_style_index;
	}

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	num_styles = GetMemorySize(pg_rec->named_styles);
	named_style = (named_stylesheet_ptr) UseMemory(pg_rec->named_styles);
	index_result = 0;

	for (style_index = 1; style_index <= num_styles; ++style_index, ++named_style) 
		if (compare_stylename(named_style->name, stylename)) {
		
		index_result = style_index;
		break;
	}

	UnuseMemory(pg_rec->named_styles);
	UnuseMemory(pg);

	return		index_result;
}


/* pgGetNamedStyleInfo returns the style, par and font_info records belonging to the
named style. */

PG_PASCAL (void) pgGetNamedStyleInfo (pg_ref pg, long index, style_info_ptr style,
		font_info_ptr font, par_info_ptr par)
{
	paige_rec_ptr			pg_rec;
	named_stylesheet_ptr	the_style;
	style_info_ptr			style_ptr;
	par_info_ptr			par_ptr;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	
	if (index > 0 && index <= GetMemorySize(pg_rec->named_styles)) {
		
		the_style = (named_stylesheet_ptr) UseMemoryRecord(pg_rec->named_styles, index - 1, 0, TRUE);
		
		if (style) {
			
			if ((style_ptr = pgLocateStyleSheet(pg_rec, the_style->stylesheet_id, NULL)) != NULL) {
				
				*style = *style_ptr;
				
				if (font)
					GetMemoryRecord(pg_rec->fonts, (long)style->font_index, font);

				UnuseMemory(pg_rec->t_formats);
			}
			else {
			
				*style = pg_rec->globals->def_style;
				*font = pg_rec->globals->def_font;
			}
		}

		if (par) {
			
			if ((par_ptr = pgLocateParStyleSheet(pg_rec, the_style->par_stylesheet_id)) != NULL) {
				
				*par = *par_ptr;
				UnuseMemory(pg_rec->par_formats);
			}
			else
				*par = pg_rec->globals->def_par;
		}

		UnuseMemory(pg_rec->named_styles);
	}
	else {
		
		if (style)
			*style = pg_rec->globals->def_style;
		if (par)
			*par = pg_rec->globals->def_par;
		if (font)
			GetMemoryRecord(pg_rec->fonts, 0, font);
	}

	UnuseMemory(pg);
}


/* pgGetNamedStyle returns the named_style_index.  If not a valid index, FALSE is returned. 
If named_style is non-NULL it is initialized with the named style, if found,  otherwise
the function is used to determine if the index is valid. */

PG_PASCAL (pg_boolean) pgGetNamedStyle (pg_ref pg, long named_style_index, named_stylesheet_ptr named_style)
{
	paige_rec_ptr			pg_rec;
	pg_boolean				valid_style;
	
	pg_rec = (paige_rec_ptr) UseMemory(pg);
	valid_style = FALSE;
	
	if ( (named_style_index > 0) && (named_style_index <= GetMemorySize(pg_rec->named_styles)) ) {
		
		valid_style = TRUE;
		
		if (named_style)
			GetMemoryRecord(pg_rec->named_styles, named_style_index - 1, (void PG_FAR *)named_style);
	}

	UnuseMemory(pg);
	
	return		valid_style;
}


/* pgRenameStyle changes the name of the style index style to style_name. */

PG_PASCAL (void) pgRenameStyle (pg_ref pg, long named_style_index, pg_c_string_ptr style_name)
{
	paige_rec_ptr			pg_rec;
	named_stylesheet		named_style;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	
	if (pg_rec->global_styles) {
		
		if (pgGetNamedStyle(pg, named_style_index, &named_style)) {
			long		parent_index;
			
			parent_index = pgGetNamedStyleIndex(pg_rec->global_styles, named_style.name);
			
			if (parent_index > 0)
				rename_style(pg_rec->global_styles, parent_index, style_name);
		}
	}

	rename_style(pg, named_style_index, style_name);

	UnuseMemory(pg);
}


/* pgDeleteNamedStyle removes a named style from the list. */

PG_PASCAL (void) pgDeleteNamedStyle (pg_ref pg, long named_style_index)
{
	paige_rec_ptr			pg_rec;
	named_stylesheet		named_style;

	pg_rec = (paige_rec_ptr) UseMemory(pg);

	if (pg_rec->global_styles) {
		
		if (pgGetNamedStyle(pg, named_style_index, &named_style)) {
			long		parent_index;
			
			parent_index = pgGetNamedStyleIndex(pg_rec->global_styles, named_style.name);
			
			if (parent_index > 0)
				delete_named_style(pg_rec->global_styles, parent_index);
		}
	}

	delete_named_style(pg, named_style_index);

	UnuseMemory(pg);
}


/* pgSetDefaultNamedStyle sets the default named style (used mostly in RTF) */

PG_PASCAL (void) pgSetDefaultNamedStyle (pg_ref pg, long def_index)
{
	paige_rec_ptr			pg_rec;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	
	if ((pg_rec->def_named_index = def_index - 1) < 0)
		pg_rec->def_named_index = 0;

	UnuseMemory(pg);
}

/* pgGetDefaultNamedStyle returns the default named style */

PG_PASCAL (long) pgGetDefaultNamedStyle (pg_ref pg)
{
	long		result;
	paige_rec_ptr			pg_rec;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	result = pg_rec->def_named_index;
	UnuseMemory(pg);

	return		result;
}


/* pgGetAppliedNamedStyle returns the named stylesheet applied to the specified selection.
If none found, FALSE is returned and named_style is set to an empty string. */

PG_PASCAL (pg_boolean) pgGetAppliedNamedStyle (pg_ref pg, select_pair_ptr selection, pg_c_string_ptr stylename)
{
	paige_rec_ptr			pg_rec;
	named_stylesheet_ptr	named_style;
	long					num_styles;
	pg_boolean				found;
	short					style_id, par_id, name_index;

	found = FALSE;

	if (!pgGetStyleSheet(pg, selection, &style_id))
		style_id = 0;
	if (!pgGetParStyleSheet(pg, selection, &par_id))
		par_id = 0;
	
	if (style_id > 0 || par_id > 0) {
	
		pg_rec = (paige_rec_ptr) UseMemory(pg);
		named_style = (named_stylesheet_ptr) UseMemory(pg_rec->named_styles);
		num_styles = GetMemorySize(pg_rec->named_styles);
		*stylename = 0;
	
		while (num_styles) {
			
			if (style_id && named_style->stylesheet_id == style_id)
				found = TRUE;
			else
			if (par_id && named_style->par_stylesheet_id == par_id)
				found = TRUE;

			if (found) {
				
				for (name_index = 0; name_index < (FONT_SIZE - 1); ++name_index)
					if ((stylename[name_index] = named_style->name[name_index]) == 0)
						break;
				
				stylename[name_index] = 0;
				break;
			}
			
			++named_style;
			--num_styles;
		}
		
		UnuseMemory(pg_rec->named_styles);
		UnuseMemory(pg);
	}
	
	return		found;
}

/* pgSetGlobalNamedStyles sets up a "parent" pg_ref to maintain a parallel set of
named styles. This is intended to implement "global" stylesheets. When this function
returns the named styles in pg will also exist in global_pg.  During the conversion
if there are identical style names yet the format is different, the global_pg wins
precedence and an error is generated (pg_errlog_rec). Such errors are returned as a
memory_ref -- MEM_NULL returned means no errors occurred. After this function is
called, additions, changes, etc. to pg's named styles will automatically update the
parent's list. (You only need pgUpdateNamedStyleChild, below, if the parent's list
changes). If remove_from_child is TRUE then styles in the child that do not exist
in the parent are removed.
If merge_duplicates is true then all source styles that are identical from the source
addopt the master's style name. */

PG_PASCAL (memory_ref) pgSetGlobalNamedStyles (pg_ref pg, pg_ref global_pg,
			pg_boolean remove_from_child, pg_boolean merge_duplicates)
{
	paige_rec_ptr			pg_rec;
	paige_rec_ptr			global_pg_rec;
	named_stylesheet_ptr	named_styles;
	named_stylesheet		parent_style;
	memory_ref				errors = MEM_NULL;
	style_info				style;
	par_info				par;
	font_info				font;
	style_info_ptr			source_style;
	par_info_ptr			source_par;
	font_info_ptr			source_font;
	long					num_styles, style_index;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	global_pg_rec = (paige_rec_ptr) UseMemory(global_pg);
	pg_rec->global_styles = MEM_NULL;			// to make sure we dont add them globally yet.

// First, merge all styles that are the same if merge_duplicates is TRUE.

	if (merge_duplicates) {
		long			duplicate;
		
		num_styles = GetMemorySize(pg_rec->named_styles);
		
		style_index = 1;

		while (style_index <= num_styles) {
			
			pgGetNamedStyleInfo(pg, style_index, &style, &font, &par);
			
			if ((duplicate = pgFindSameNamedStyle(pg, &style, &font, &par, style_index)) > 0) {
				long		num_style_infos;
				
				num_style_infos = GetMemorySize(pg_rec->t_formats);
				source_style = (style_info_ptr) UseMemory(pg_rec->t_formats);
				
				while (num_style_infos) {
					
					if (source_style->named_style_index == style_index)
						source_style->named_style_index = duplicate;

					++source_style;
					--num_style_infos;
				}
				
				UnuseMemory(pg_rec->t_formats);
				
				pgDeleteNamedStyle(pg, style_index);
				
				num_styles -= 1;
			}
			else
				style_index += 1;
		}
	}

	num_styles = GetMemorySize(pg_rec->named_styles);
	named_styles = (named_stylesheet_ptr) UseMemory(pg_rec->named_styles);

// Second, add styles from child to parent (or remove from child if remove_from_child is TRUE):

	while (num_styles) {

		source_style = pgLocateStyleSheet(pg_rec, named_styles->stylesheet_id, NULL);
		source_par = pgLocateParStyleSheet(pg_rec, named_styles->par_stylesheet_id);

		if (source_style)
			source_font = (font_info_ptr) UseMemoryRecord(pg_rec->fonts, (long)source_style->font_index, 0, TRUE);
		else
			source_font = NULL;
		
		style_index = 0;

		if (merge_duplicates)
			style_index = pgFindSameNamedStyle(global_pg, source_style, source_font, source_par, 0);
		
		if (style_index == 0)
			style_index = pgGetNamedStyleIndex(global_pg, named_styles->name);

		if (style_index > 0) {
			pg_boolean				equal_styles, equal_pars;
			long					name_index, par_name_index;
			
			pgGetNamedStyle(global_pg, style_index, &parent_style);
			pgGetNamedStyleInfo(global_pg, style_index, &style, &font, &par);
			
		// The following tests possible unlike names, which ONLY HAPPENS
		// if merge_duplicates is TRUE and we found matching style contents.
		// In this case we do not want to add to parent, but catch it later
		// when we add parent styles to child.

			if (pgEqualStruct(parent_style.name, named_styles->name, sizeof(pg_char) * FONT_SIZE)) {

				style.procs = pg_rec->globals->def_style.procs;
				par.procs = pg_rec->globals->def_par.procs;

				name_index = par_name_index = 0;

				if (source_style) {
					
					name_index = source_style->named_style_index;
					if ((equal_styles = pgBasicallyEqualStyles(source_style, &style)) != 0)
						equal_styles = pgBasicallyEqualFonts(source_font, &font);
				}
				else
					equal_styles = (pg_boolean)(parent_style.stylesheet_id == 0);

				if (source_par) {

					par_name_index = source_par->named_style_index;
					equal_pars = pgBasicallyEqualPars(source_par, &par);
				}
				else
					equal_pars = (pg_boolean)(parent_style.par_stylesheet_id == 0);
					
				if (source_style) {
					
					UnuseMemory(pg_rec->fonts);
					UnuseMemory(pg_rec->t_formats);
				}

				if (source_par)
					UnuseMemory(pg_rec->par_formats);

				source_style = NULL;
				source_font = NULL;
				source_par = NULL;

				if (!equal_styles) {
					
					style.named_style_index = name_index;
					
					if (parent_style.stylesheet_id) {
						
						if (named_styles->stylesheet_id)
							pgChangeStyle(pg, named_styles->stylesheet_id, &style, &font, draw_none);
						else
							named_styles->stylesheet_id = pgNewStyle(pg, &style, &font);
					}
					else
						named_styles->stylesheet_id = 0;
				}

				if (!equal_pars) {
					
					par.named_style_index = par_name_index;

					if (parent_style.par_stylesheet_id) {
						
						if (named_styles->par_stylesheet_id)
							pgChangeParStyle(pg, named_styles->par_stylesheet_id, &par, draw_none);
						else
							named_styles->par_stylesheet_id = pgNewParStyle(pg, &par);
					}
					else
						named_styles->par_stylesheet_id = 0;
				}
			}
		}
		else {
			
			if (remove_from_child) {
				long			child_index;
				
				child_index = pgGetNamedStyleIndex(pg, named_styles->name);
				UnuseMemory(pg_rec->named_styles);

				delete_named_style(pg, child_index);
				named_styles = (named_stylesheet_ptr) UseMemory(pg_rec->named_styles);
				named_styles += (child_index - 1);
				
				if (num_styles > 1)
					num_styles -= 1;
			}
			else
				new_named_style(global_pg, named_styles->name, source_style, source_font, source_par);
		}

		if (source_style) {
			
			UnuseMemory(pg_rec->fonts);
			UnuseMemory(pg_rec->t_formats);
		}

		if (source_par)
			UnuseMemory(pg_rec->par_formats);
		
		++named_styles;
		--num_styles;
	}
	
	UnuseMemory(pg_rec->named_styles);
	
// Now add all the parent styles to the child:
	
	named_styles = (named_stylesheet_ptr) UseMemory(global_pg_rec->named_styles);
	num_styles = GetMemorySize(global_pg_rec->named_styles);
	
	while (num_styles) {
		long			duplicate_index;

		if (pgGetNamedStyleIndex(pg, named_styles->name) == 0) {
			
			source_style = pgLocateStyleSheet(global_pg_rec, named_styles->stylesheet_id, NULL);
			source_par = pgLocateParStyleSheet(global_pg_rec, named_styles->par_stylesheet_id);
		
			if (source_style)
				source_font = (font_info_ptr) UseMemoryRecord(global_pg_rec->fonts, (long)source_style->font_index, 0, TRUE);
			else
				source_font = NULL;
			
			duplicate_index = 0;

			if (merge_duplicates)
				duplicate_index = pgFindSameNamedStyle(pg, source_style, source_font, source_par, 0);

			if (duplicate_index) {
				named_stylesheet_ptr	renamed_style;
				
				renamed_style = (named_stylesheet_ptr) UseMemoryRecord(pg_rec->named_styles, duplicate_index - 1, 0, TRUE);
				pgBlockMove(named_styles->name, renamed_style->name, sizeof(pg_char) * FONT_SIZE);
				UnuseMemory(pg_rec->named_styles);
			}
			else {
			
				if (source_style) {
					
					style = *source_style;
					style.procs = pg_rec->globals->def_style.procs;
					source_style = &style;
					style.procs = pg_rec->globals->def_style.procs;
				}
				
				if (source_par) {
					
					par = *source_par;
					par.procs = pg_rec->globals->def_par.procs;
					source_par = &par;
					par.procs = pg_rec->globals->def_par.procs;
				}

				new_named_style(pg, named_styles->name, source_style, source_font, source_par);
			}

			if (source_style) {
				
				UnuseMemory(global_pg_rec->fonts);
				UnuseMemory(global_pg_rec->t_formats);
			}

			if (source_par)
				UnuseMemory(global_pg_rec->par_formats);
		}

		++named_styles;
		--num_styles;
	}

	UnuseMemory(global_pg_rec->named_styles);
	pg_rec->global_styles = global_pg;

	UnuseMemory(pg);
	UnuseMemory(global_pg);

	return	errors;
}


/* pgUnsetGlobalNamedStyles clears the "parent" pg_ref, if any, for global styles.
Normally this is called if the parent is disposed. */

PG_PASCAL (void) pgUnsetGlobalNamedStyles (pg_ref pg)
{
	paige_rec_ptr			pg_rec;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	pg_rec->global_styles = MEM_NULL;
	UnuseMemory(pg);
}


/* pgUpdateNamedStyleChild can be called after changing a global stylesheet, which can
effect a child pg_ref. This function makes sure that any changes are copied into pg.
If a change occurs that can affect formatting or display TRUE is returned. If deletes is
TRUE then the stylename was deleted in the parent. */

PG_PASCAL (pg_boolean) pgUpdateNamedStyleChild (pg_ref pg, pg_char_ptr stylename, pg_boolean deleted)
{
	paige_rec_ptr			pg_rec;
	paige_rec_ptr			global_pg_rec;
	long					parent_index, child_index;
	pg_boolean				changed = FALSE;
	
	pg_rec = (paige_rec_ptr) UseMemory(pg);
	
	if (pg_rec->global_styles) {
		
		if ((child_index = pgGetNamedStyleIndex(pg, stylename)) > 0) {
			
			if (deleted)
				delete_named_style(pg, child_index);
			else
			if ((parent_index = pgGetNamedStyleIndex(pg_rec->global_styles, stylename)) > 0) {
				named_stylesheet	child_named_style, parent_named_style;
				pg_boolean			style_changed, par_changed;
				style_info			style, child_style;
				par_info			par, child_par;
				font_info			font, child_font;
				
				global_pg_rec = (paige_rec_ptr) UseMemory(pg_rec->global_styles);

				pgGetNamedStyle(pg, child_index, &child_named_style);
				pgGetNamedStyle(pg_rec->global_styles, parent_index, &parent_named_style);

				pgGetNamedStyleInfo(pg_rec->global_styles, parent_index, &style, &font, &par);
				pgGetNamedStyleInfo(pg, child_index, &child_style, &child_font, &child_par);
				style.procs = pg_rec->globals->def_style.procs;
				par.procs = pg_rec->globals->def_par.procs;
				
				if (parent_named_style.stylesheet_id) {
				
					if (!(style_changed = !pgBasicallyEqualStyles(&child_style, &style)))
						style_changed = !pgBasicallyEqualFonts(&child_font, &font);
				}
				else
					style_changed = FALSE;
				
				if (parent_named_style.par_stylesheet_id)
					par_changed = !pgBasicallyEqualPars(&child_par, &par);
				else
					par_changed = FALSE;
				
				if (style_changed || par_changed) {
					
					if (child_named_style.stylesheet_id)
						pgChangeStyle(pg, child_named_style.stylesheet_id, &style, &font, draw_none);
					if (child_named_style.par_stylesheet_id)
						pgChangeParStyle(pg, child_named_style.par_stylesheet_id, &par, draw_none);
				}
				
				UnuseMemory(pg_rec->global_styles);
			}
			
		}
	}
	
	UnuseMemory(pg);

	return		changed;
}


/* pgFindSameNamedStyle returns the named style index, if any, that matches style, font
and par exactly. Any of these can be null (which means we don't care). This function
omits exclude_index style. */

PG_PASCAL (long) pgFindSameNamedStyle (pg_ref pg, style_info_ptr style, font_info_ptr font,
			par_info_ptr par, long exclude_index)
{
	paige_rec_ptr			pg_rec;
	style_info_ptr			source_style;
	par_info_ptr			source_par;
	font_info_ptr			source_font;
	named_stylesheet_ptr	named_styles;
	long					num_styles, index;
	long					result = 0;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	
	num_styles = GetMemorySize(pg_rec->named_styles);
	named_styles = (named_stylesheet_ptr) UseMemory(pg_rec->named_styles);
	
	for (index = 1; index <= num_styles; ++index, ++named_styles) {
		
		if (index == exclude_index)
			result = 0;
		else {
		
			result = index;				// Assume a match
			source_font = NULL;
			source_style = NULL;

			if (style) {
				
				source_style = pgLocateStyleSheet(pg_rec, named_styles->stylesheet_id, NULL);
				
				if (!source_style)
					result = 0;
				else {
					
					if (pgBasicallyEqualStyles(source_style, style)) {
						
						if (font) {
							
							source_font = (font_info_ptr) UseMemoryRecord(pg_rec->fonts, (long)source_style->font_index, 0, TRUE);
							if (!pgBasicallyEqualFonts(source_font, font))
								result = 0;
						}
					}
				}
				
				if (source_style)
					UnuseMemory(pg_rec->t_formats);
				if (source_font)
					UnuseMemory(pg_rec->fonts);
			}
			
			if (result && par) {
				
				source_par = pgLocateParStyleSheet(pg_rec, named_styles->par_stylesheet_id);
				
				if (source_par) {
					
					if (!pgBasicallyEqualPars(source_par, par))
						result = 0;

					UnuseMemory(pg_rec->par_formats);
				}
			}
		}

		if (result)
			break;
	}
	
	UnuseMemory(pg_rec->named_styles);
	UnuseMemory(pg);
	
	return	result;
}


/*********************************** Local Functions ***************************/


/* This function returns the total number of style sheets. If highest_id is
non-NULL, *highest_id returns with the highest ID found.  */

static short count_style_sheets (paige_rec_ptr pg, short PG_FAR *highest_id)
{
	register style_info_ptr		styles;
	register pg_short_t			style_qty;
	short						num_styles, highest;

	styles = (style_info_ptr) UseMemory(pg->t_formats);
	style_qty = (pg_short_t)GetMemorySize(pg->t_formats);

	for (num_styles = highest = 0; style_qty; ++styles, --style_qty)
		if (styles->style_sheet_id > 0) {
		
		++num_styles;
		
		if (styles->style_sheet_id > highest)
			highest = (short)styles->style_sheet_id;
	}

	UnuseMemory(pg->t_formats);
	
	if (highest_id)
		*highest_id = highest;

	return		num_styles;
}


/* This is the same as count_style_sheets except for paragraphs.  */

static short count_par_style_sheets (paige_rec_ptr pg, short PG_FAR *highest_id)
{
	register par_info_ptr		styles;
	register pg_short_t			style_qty;
	short						num_styles, highest;

	styles = (par_info_ptr) UseMemory(pg->par_formats);
	style_qty = (pg_short_t)GetMemorySize(pg->par_formats);

	for (num_styles = highest = 0; style_qty; ++styles, --style_qty)
		if (styles->style_sheet_id > 0) {
		
		++num_styles;
		
		if (styles->style_sheet_id > highest)
			highest = (short)styles->style_sheet_id;
	}

	UnuseMemory(pg->par_formats);
	
	if (highest_id)
		*highest_id = highest;

	return		num_styles;
}


/* Given a stylesheet change, this function returns the maximum text range
affected based on stylesheet_id. The compare_type is either style_compare or
par_compare. The range is returned in range->begin and range->end. */

static void affected_text_range (paige_rec_ptr pg, short compare_type,
		short stylesheet_id, select_pair_ptr range)
{
	register style_run_ptr			run;
	register select_pair_ptr		result;
	register long					qty;
	register short					index, style_id;
	short							negative_id;
	
	negative_id = -stylesheet_id;
	result = range;
	
	result->begin = pg->t_length;
	result->end = 0;

	if (compare_type == style_compare) {
		register style_info_ptr		base;

		run = (style_run_ptr) UseMemory(pg->t_style_run);
		base = (style_info_ptr) UseMemory(pg->t_formats);
		
		for (qty = GetMemorySize(pg->t_style_run) - 1; qty; ++run, --qty) {
			
			index = run->style_item;

			if (style_id = (short)base[index].style_sheet_id)
				if ((style_id == stylesheet_id) || (style_id == negative_id)) {
				
				if (run->offset < result->begin)
					result->begin = run->offset;
				if (run[1].offset > result->end)
					result->end = run[1].offset;
			}
		}

		UnuseMemory(pg->t_style_run);
		UnuseMemory(pg->t_formats);
	}
	else {
		register par_info_ptr		base;

		run = (style_run_ptr) UseMemory(pg->par_style_run);
		base = (par_info_ptr) UseMemory(pg->par_formats);
		
		for (qty = GetMemorySize(pg->par_style_run); qty; ++run, --qty) {
			
			index = run->style_item;

			if (style_id = (short)base[index].style_sheet_id)
				if ((style_id == stylesheet_id) || (style_id == negative_id)) {
				
				if (run->offset < result->begin)
					result->begin = run->offset;
				if (run[1].offset > result->end)
					result->end = run[1].offset;
			}
		}

		UnuseMemory(pg->par_style_run);
		UnuseMemory(pg->par_formats);
	}
}

/* compare_stylename returns TRUE if name1 matches name2. Both are cstrings. */

static pg_boolean compare_stylename (pg_c_string_ptr name1, pg_c_string_ptr name2)
{
	register pg_c_string_ptr		source_name;
	register pg_c_string_ptr		target_name;
	
	source_name = name1;
	target_name = name2;
	
	for (;;) {
		
		if (*source_name != *target_name)
			return	FALSE;
		if (*source_name++ == 0)
			break;
		
		++target_name;
	}
	
	return	TRUE;
}

/* new_named_style is the lower-level call for pgNewNamedStyle(). */

static long new_named_style (pg_ref pg, pg_c_string_ptr stylename, const style_info_ptr style,
		const font_info_ptr font, par_info_ptr par)
{
	short					style_id, par_id;

	if (style) {
		style_id = pgNewStyle(pg, style, font);
	}
	else
		style_id = 0;
	
	if (par)
		par_id = pgNewParStyle(pg, par);
	else
		par_id = 0;
	
	return pgAddNamedStyle(pg, stylename, style_id, par_id);
}

/* rename_style is the lower-level function for pgRenameStyle. */

static void rename_style (pg_ref pg, long named_style_index, pg_c_string_ptr style_name)
{
	paige_rec_ptr			pg_rec;
	named_stylesheet_ptr	the_style;
	short					ctr;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	
	if ( (named_style_index > 0) && (named_style_index <= GetMemorySize(pg_rec->named_styles)) ) {
		
		the_style = (named_stylesheet_ptr) UseMemoryRecord(pg_rec->named_styles, named_style_index - 1, 0, TRUE);
		pgFillBlock(the_style->name, FONT_SIZE * sizeof(pg_char), 0);
		
		for (ctr = 0; ctr < (FONT_SIZE) - 1; ++ctr)
			if ((the_style->name[ctr] = style_name[ctr]) == 0)
				break;
		
		the_style->name[ctr] = 0;
		UnuseMemory(pg_rec->named_styles);
	}

	UnuseMemory(pg);
}

/* delete_named_style is the lower-level call from pgDeleteNamedStyle. */

static void delete_named_style (pg_ref pg, long named_style_index)
{
	paige_rec_ptr			pg_rec;
	
	pg_rec = (paige_rec_ptr) UseMemory(pg);
	
	if ( (named_style_index > 0) && (named_style_index <= GetMemorySize(pg_rec->named_styles)) ) {
		style_info_ptr		styles;
		par_info_ptr		pars;
		long				style_index, num_items;
		
		style_index = named_style_index - 1;

		DeleteMemory(pg_rec->named_styles, style_index, 1);
		
		if (pg_rec->def_named_index > style_index)
			--pg_rec->def_named_index;
		
	// Walk through and adjust all the named style refs in the format records:
	
		styles = (style_info_ptr) UseMemory(pg_rec->t_formats);
		num_items = GetMemorySize(pg_rec->t_formats);
		
		while (num_items) {
			
			if (styles->named_style_index > style_index)
				styles->named_style_index -= 1;
			else
			if (styles->named_style_index == named_style_index)
				styles->named_style_index = 0;

			++styles;
			--num_items;
		}
		
		UnuseMemory(pg_rec->t_formats);

		pars = (par_info_ptr) UseMemory(pg_rec->par_formats);
		num_items = GetMemorySize(pg_rec->par_formats);
		
		while (num_items) {
			
			if (pars->named_style_index > style_index)
				pars->named_style_index -= 1;
			else
			if (pars->named_style_index == named_style_index)
				pars->named_style_index = 0;

			++pars;
			--num_items;
		}
		
		UnuseMemory(pg_rec->par_formats);
	}

	UnuseMemory(pg);
}

/* merge_named_style is the low level action for pgMergeNamedStyles(). */

static void merge_named_style (pg_ref pg, pg_c_string_ptr source_style,
		pg_c_string_ptr target_style, short draw_mode)
{
	paige_rec_ptr		pg_rec;
	pg_ref				master_save;
	named_stylesheet	source_rec, target_rec;
	long				source_index, target_index;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	master_save = pg_rec->global_styles;
	pg_rec->global_styles = MEM_NULL;

	source_index = pgGetNamedStyleIndex(pg, source_style);
	target_index = pgGetNamedStyleIndex(pg, target_style);
	
	if (source_index && target_index) {
		
		pgGetNamedStyle(pg, source_index, &source_rec);
		pgGetNamedStyle(pg, target_index, &target_rec);
		
		if (source_rec.par_stylesheet_id)
			change_par_stylesheet_id(pg_rec, source_rec.par_stylesheet_id,
						target_rec.par_stylesheet_id, target_index);
		if (source_rec.stylesheet_id)
			change_stylesheet_id(pg_rec, source_rec.stylesheet_id,
						target_rec.stylesheet_id, target_index);
	}
	
	if (source_index)
		pgDeleteNamedStyle(pg, source_index);
	
	pg_rec->global_styles = master_save;
	UnuseMemory(pg);
}


/* change_stylesheet_id changes all styles whose stylesheet ID matches style_id, the
style and ID is changed to new_style_id. */

static void change_stylesheet_id (paige_rec_ptr pg_rec, short style_id, short new_style_id,
			long new_named_id)
{
	style_info_ptr			the_style, each_style, caps_style;
	style_info				old_style, new_caps_style, mask;
	select_pair				range_affected;
	long					qty, change_index, old_caps_index, new_caps_index;
	short					negative_style_id, font_index;
	
	if (the_style = pgLocateStyleSheet(pg_rec, style_id, NULL)) {
	
		pgBlockMove(the_style, &old_style, SIGNIFICANT_STYLE_SIZE);
		UnuseMemory(pg_rec->t_formats);
	}
	
	if (the_style = pgLocateStyleSheet(pg_rec, new_style_id, NULL)) {
			
/* Now walk through all style_info's and locate any "offspring" from this
original stylesheet. If found, I need to change original fields. */

		each_style = (style_info_ptr) UseMemoryRecord(pg_rec->t_formats, 0, USE_ALL_RECS, FALSE);
		negative_style_id = -style_id;
		qty = GetMemorySize(pg_rec->t_formats);
		
		for (change_index = 0; change_index < qty; ++each_style, ++change_index) {
			
			if (each_style->style_sheet_id == negative_style_id && !each_style->embed_object) {
				
				if (each_style->styles[small_caps_var])
					old_caps_index = each_style->small_caps_index + 1;
				else
					old_caps_index = 0;

				pgFillBlock(&mask, SIGNIFICANT_STYLE_SIZE, COMPARE_TRUE);
				pgSetMaskFromFlds(&old_style, each_style, &mask, style_compare, FALSE, FALSE);
				mask.style_sheet_id = 0;
				pgSetFldsFromMask(each_style, the_style, &mask, SIGNIFICANT_STYLE_SIZE);
				each_style->style_sheet_id = -new_style_id;
				
				if (new_named_id)
					each_style->named_style_index = new_named_id;
				
				font_index = each_style->font_index;
				each_style->procs.init(pg_rec, each_style, (font_info_ptr)UseMemoryRecord(pg_rec->fonts, font_index, 0, TRUE));
				UnuseMemory(pg_rec->fonts);
				
				if (each_style->styles[small_caps_var])
					new_caps_index = each_style->small_caps_index + 1;
				else
					new_caps_index = 0;
				
				if ((new_caps_index != old_caps_index) && new_caps_index) {
					
					if (old_caps_index) {
						
						caps_style = (style_info_ptr) UseMemoryRecord(pg_rec->t_formats, old_caps_index - 1, 0, FALSE);
						caps_style->used_ctr -= 1;
					}
					
					new_caps_style = *each_style;
					UnuseMemory(pg_rec->t_formats);
					
					new_caps_index = pgAddSmallCapsStyle(pg_rec, NULL, internal_clone_reason, &new_caps_style);
					each_style = (style_info_ptr) UseMemoryRecord(pg_rec->t_formats, change_index, USE_ALL_RECS, TRUE);
					each_style->small_caps_index = new_caps_index;
					caps_style = (style_info_ptr) UseMemoryRecord(pg_rec->t_formats, new_caps_index, 0, FALSE);
					caps_style->used_ctr += 1;
				}
			}
		}

		UnuseMemory(pg_rec->t_formats);
		
		++pg_rec->change_ctr;

		affected_text_range(pg_rec, style_compare, new_style_id, &range_affected);
		
		if (range_affected.end > range_affected.begin)
			pgInvalSelect(pg_rec->myself, range_affected.begin, range_affected.end);
	}
}



/* change_par_stylesheet_id changes all styles whose stylesheet ID matches style_id, the
style and ID is changed to new_style_id. */

static void change_par_stylesheet_id (paige_rec_ptr pg_rec, short style_id, short new_style_id,
			long new_named_id)
{
	par_info_ptr			the_style, each_style;
	par_info				old_style, mask;
	select_pair				range_affected;
	long					qty, change_index;
	short					negative_style_id;
	
	if (the_style = pgLocateParStyleSheet(pg_rec, style_id)) {
	
		pgBlockMove(the_style, &old_style, SIGNIFICANT_PAR_STYLE_SIZE);
		UnuseMemory(pg_rec->par_formats);
	}
	
	if (the_style = pgLocateParStyleSheet(pg_rec, new_style_id)) {
			
/* Now walk through all style_info's and locate any "offspring" from this
original stylesheet. If found, I need to change original fields. */

		each_style = (par_info_ptr) UseMemoryRecord(pg_rec->par_formats, 0, USE_ALL_RECS, FALSE);
		negative_style_id = -style_id;
		qty = GetMemorySize(pg_rec->par_formats);
		
		for (change_index = 0; change_index < qty; ++each_style, ++change_index) {
			
			if (each_style->style_sheet_id == negative_style_id) {

				pgFillBlock(&mask, SIGNIFICANT_PAR_STYLE_SIZE, COMPARE_TRUE);
				pgSetMaskFromFlds(&old_style, each_style, &mask, par_compare, FALSE, FALSE);
				mask.style_sheet_id = 0;
				pgSetFldsFromMask(each_style, the_style, &mask, SIGNIFICANT_PAR_STYLE_SIZE);
				each_style->style_sheet_id = -new_style_id;
				
				if (new_named_id)
					each_style->named_style_index = new_named_id;
			}
		}

		UnuseMemory(pg_rec->par_formats);
		
		++pg_rec->change_ctr;

		affected_text_range(pg_rec, par_compare, new_style_id, &range_affected);
		
		if (range_affected.end > range_affected.begin)
			pgInvalSelect(pg_rec->myself, range_affected.begin, range_affected.end);
	}
}

