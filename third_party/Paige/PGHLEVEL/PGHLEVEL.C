/* This source file handles high-level, Mac-specific functions to make life
easier for some of the more complex functions. Copyright 1994 DataPak Software,
Inc. Software created by "Gar," alias Gary Crandall.  */

/* Updated May 1995 for ease of use with opendoc by TR Shaw OITC */

#include "Paige.h"
#include "pgTraps.h"
#include "pgUtils.h"
#include "machine.h"
#include "pgHLevel.h"
#include "pgOSUtl.h"
#include "pgTables.h"
#include "pgEdit.h"

/* pgSetFontByName applies font_name font to the current selection range (or to
the text range presented in selection_range if that parameter is non-NULL).
If redraw is TRUE the changes are re-drawn.
ATTENTION WINDOWS USERS: THIS FUNCTION ONLY WORKS CORRECTLY FOR "STANDARD"
TRUETYPE FONTS AND FONTS THAT USE ANSI CHARACTER SET. To set other fonts,
see pgSetFontByLog. */

#ifdef WINDOWS_PLATFORM
  
  #include <string.h>
  #define STRICT
  #include <windowsx.h>
  #include <windows.h>
  
PG_PASCAL (void) pgSetFontByName (pg_ref pg, const pg_font_name_ptr font_name,
		const select_pair_ptr selection_range, pg_boolean redraw)
{
	font_info				font, mask;
	short					draw_mode;
	pg_short_t				font_name_size;

	pgFillBlock(&font, sizeof(font_info), 0);
	pgFillBlock(&mask, sizeof(font_info), SET_MASK_BITS);
	
	font_name_size = lstrlen(font_name);
	if (font_name_size  > (FONT_SIZE - 1))
		font_name_size = FONT_SIZE - 1;
	
	if (redraw)
		draw_mode = best_way;
	else
		draw_mode = draw_none;

	pgBlockMove(font_name, (LPSTR) font.name, (font_name_size + 1) * sizeof(pg_char));
	font.environs = NAME_IS_CSTR;
	pgSetFontInfo(pg, selection_range, &font, &mask, draw_mode);
}


/* pgSetFontByLog sets the font that is defined in log_font (which you
can get, for example, from a ChooseFont dialog). This is the one you must
use for all available fonts to work.

BOG[4/27/00] Changed this to use pascal string for the font name field. There
seems to be a problem, somewhere in the bowels of Paige, with the NAME_IS_CSTR
crap, causing Paige to think the font is "rial" instead of "Arial," etc. */

PG_PASCAL (void) pgSetFontByLog (pg_ref pg, const LOGFONT PG_FAR *log_font,
		const select_pair_ptr selection_range, pg_boolean redraw)
{
	font_info font, mask;
	short draw_mode;

	pgFillBlock(&font, sizeof(font_info), 0);
	pgFillBlock(&mask, FONT_SIZE, SET_MASK_BITS);
                
//    lstrcpy(font.name, log_font->lfFaceName);
//    font.environs |= NAME_IS_CSTR;

	font.name[0] = lstrlen(log_font->lfFaceName);
	lstrcpy(&font.name[1], log_font->lfFaceName);
    font.family_id = log_font->lfPitchAndFamily;
    font.machine_var[PG_OUT_PRECISION] = log_font->lfOutPrecision;
    font.machine_var[PG_CLIP_PRECISION] = log_font->lfClipPrecision;
    font.machine_var[PG_QUALITY] = log_font->lfQuality;
    font.machine_var[PG_CHARSET] = log_font->lfCharSet;
	
	if (redraw)
		draw_mode = best_way;
	else
		draw_mode = draw_none;

	pgSetFontInfo(pg, selection_range, &font, &mask, draw_mode);
}


/* pgGetFontByNum returns the font, by LOGFONT, applied to the current selection
range. If the same LOGFONT is consistent throughout, TRUE is returned. */

PG_PASCAL (pg_boolean) pgGetFontByLog (pg_ref pg, LOGFONT PG_FAR *log_font)
{
	font_info			font, mask;
	style_info			style, s_mask;
	paige_rec_ptr		pg_rec;
	pg_boolean			result;
	
	pgGetFontInfo(pg, NULL, FALSE, &font, &mask);
	pgGetStyleInfo(pg, NULL, FALSE, &style, &s_mask);
	
	result = (mask.name[0] != 0 &&
			style.styles[bold_var] != 0 && style.styles[italic_var] != 0 && 
			style.styles[underline_var] != 0 && style.styles[strikeout_var] != 0 &&
			style.char_width != 0 && style.point != 0 && 
			style.styles[relative_point_var] != 0);
	
	/*Begin Ben Hack
		Changed if (result) to if (TRUE)
	End Ben Hack*/
	//if (result)
	if (TRUE)
	{
	    lstrcpy(log_font->lfFaceName, &font.name[1]);
	    log_font->lfPitchAndFamily = (unsigned char)font.family_id;
	    log_font->lfOutPrecision = (unsigned char)font.machine_var[PG_OUT_PRECISION];
	    log_font->lfClipPrecision = (unsigned char)font.machine_var[PG_CLIP_PRECISION];
	    log_font->lfQuality = (unsigned char)font.machine_var[PG_QUALITY];
	    log_font->lfCharSet = (unsigned char)font.machine_var[PG_CHARSET];
	
		if (style.styles[bold_var])
			log_font->lfWeight = FW_BOLD;
		else
			log_font->lfWeight = FW_NORMAL;
	
	    if (style.styles[italic_var])
			log_font->lfItalic = 1;
		if (style.styles[underline_var])
		log_font->lfUnderline = 1;
	    if (style.styles[strikeout_var])
		log_font->lfStrikeOut = 1;
	
	 	log_font->lfWidth = pgHiWord(pgRoundFixed(style.char_width));
		
		pg_rec = UseMemory(pg);
		log_font->lfHeight = -MulDiv((int)pgComputePointSize(pg_rec, &style), (int) pgHiWord(pg_rec->resolution), 72);
		UnuseMemory(pg);
	}
	
	return result;
}




/* pgSetTextColor changes the color of text of the currently selected text or,
if selection_range is non-NULL it changes the text range specified. If redraw is
TRUE the changes are re-drawn. */

PG_PASCAL (void) pgSetTextColor (pg_ref pg, const pg_plat_color_value PG_FAR *color,
		const select_pair_ptr selection_range, pg_boolean redraw)
{
	style_info					style, mask;
	short						draw_mode;

	pgFillBlock(&mask, sizeof(style_info), 0);
	pgFillBlock(&mask.fg_color, sizeof(color_value), SET_MASK_BITS);
	pgOSToPgColor(color, &style.fg_color);

	if (redraw)
		draw_mode = best_way;
	else
		draw_mode = draw_none;

	pgSetStyleInfo(pg, selection_range, &style, &mask, draw_mode);
}


/* pgSetTextBKColor changes the background color of text of the currently selected text or,
if selection_range is non-NULL it changes the text range specified. If redraw is
TRUE the changes are re-drawn. */

PG_PASCAL (void) pgSetTextBKColor (pg_ref pg, const pg_plat_color_value PG_FAR *color,
		const select_pair_ptr selection_range, pg_boolean redraw)
{
	style_info					style, mask;
	short						draw_mode;

	pgFillBlock(&mask, sizeof(style_info), 0);
	pgFillBlock(&mask.bk_color, sizeof(color_value), SET_MASK_BITS);
	pgOSToPgColor(color, &style.bk_color);
	
	if (redraw)
		draw_mode = best_way;
	else
		draw_mode = draw_none;

	pgSetStyleInfo(pg, selection_range, &style, &mask, draw_mode);
}


/* pgGetFontByName returns the font, by name, applied to the current selection
range. If the same font is consistent throughout, TRUE is returned. */

PG_PASCAL (pg_boolean) pgGetFontByName (pg_ref pg, pg_font_name_ptr font_name)
{
	font_info			font, mask;
	pg_short_t			font_name_size;
	
	pgGetFontInfo(pg, NULL, FALSE, &font, &mask);
	font_name_size = font.name[0];
	pgBlockMove(&font.name[1], font_name, (font_name_size + 1) * sizeof(pg_char));
	
	return	(mask.name[0] != 0);
}

/* pgGetTextColor returns the text color applied to the current selection.
If the same text color is consistent throughout, TRUE is returned. */

PG_PASCAL (pg_boolean) pgGetTextColor (pg_ref pg, pg_plat_color_value PG_FAR *color)
{
	style_info			style, mask;
	
	pgGetStyleInfo(pg, NULL, FALSE, &style, &mask);
	pgColorToOS( &style.fg_color, (void PG_FAR *)color);
	
	return	(mask.fg_color.red != 0
			&& mask.fg_color.green != 0
			&& mask.fg_color.blue != 0);
}


/* pgGetTextBKColor returns the background color applied to the current selection.
If the same background color is consistent throughout, TRUE is returned. */

PG_PASCAL (pg_boolean) pgGetTextBKColor (pg_ref pg,  pg_plat_color_value PG_FAR *color)
{
	style_info			style, mask;
	
	pgGetStyleInfo(pg, NULL, FALSE, &style, &mask);
	pgColorToOS( &style.bk_color, (void PG_FAR *)color);
	
	return	(mask.bk_color.red != 0
			&& mask.bk_color.green != 0
			&& mask.bk_color.blue != 0);
}

 #else
 
 // MAC-SPECIFIC FUNCTIONS BEGIN HERE

#ifdef MAC_PLATFORM
#include <Fonts.h>
#endif

PG_PASCAL (void) pgSetFontByName (pg_ref pg, const pg_font_name_ptr font_name,
		const select_pair_ptr selection_range, pg_boolean redraw)
{
	font_info				font, mask;
	short					draw_mode;
	pg_short_t				font_name_size;

	pgFillBlock(&font, sizeof(font_info), 0);
	pgFillBlock(&mask, sizeof(font_info), SET_MASK_BITS);
	
	if ((font_name_size = font_name[0]) > (FONT_SIZE - 1))
		font_name_size = FONT_SIZE - 1;
	
	if (redraw)
		draw_mode = best_way;
	else
		draw_mode = draw_none;

	pgBlockMove(font_name, font.name, (font_name_size + 1) * sizeof(pg_char));
	pgSetFontInfo(pg, selection_range, &font, &mask, draw_mode);
}


/* pgSetFontByNum applies font_num font to the current selection range (or to
the text range presented in selection_range if that parameter is non-NULL).
If redraw is TRUE the changes are re-drawn. */

PG_PASCAL (void) pgSetFontByNum (pg_ref pg, short font_num,
		const select_pair_ptr selection_range, pg_boolean redraw)
{
	Str255			font_name;

	GetFontName(font_num, font_name);
	pgSetFontByName(pg, font_name, selection_range, redraw);
}


/* pgSetTextColor changes the color of text of the currently selected text or,
if selection_range is non-NULL it changes the text range specified. If redraw is
TRUE the changes are re-drawn. */

PG_PASCAL (void) pgSetTextColor (pg_ref pg, const pg_plat_color_value *color,
		const select_pair_ptr selection_range, pg_boolean redraw)
{
	style_info					style, mask;
	short						draw_mode;

	pgFillBlock(&mask, sizeof(style_info), 0);
	pgFillBlock(&mask.fg_color, sizeof(color_value), SET_MASK_BITS);
	pgBlockMove(color, &style.fg_color, sizeof(RGBColor));
	
	if (redraw)
		draw_mode = best_way;
	else
		draw_mode = draw_none;

	pgSetStyleInfo(pg, selection_range, &style, &mask, draw_mode);
}


/* pgSetTextBKColor changes the background color of text of the currently selected text or,
if selection_range is non-NULL it changes the text range specified. If redraw is
TRUE the changes are re-drawn. */

PG_PASCAL (void) pgSetTextBKColor (pg_ref pg, const pg_plat_color_value *color,
		const select_pair_ptr selection_range, pg_boolean redraw)
{
	style_info					style, mask;
	short						draw_mode;

	pgFillBlock(&mask, sizeof(style_info), 0);
	pgFillBlock(&mask.bk_color, sizeof(color_value), SET_MASK_BITS);
	pgBlockMove(color, &style.bk_color, sizeof(RGBColor));
	
	if (redraw)
		draw_mode = best_way;
	else
		draw_mode = draw_none;

	pgSetStyleInfo(pg, selection_range, &style, &mask, draw_mode);
}


/* pgGetFontByName returns the font, by name, applied to the current selection
range. If the same font is consistent throughout, TRUE is returned. */

PG_PASCAL (pg_boolean) pgGetFontByName (pg_ref pg, Str255 font_name)
{
	font_info			font, mask;
	pg_short_t			font_name_size;
	
	pgGetFontInfo(pg, NULL, FALSE, &font, &mask);
	font_name_size = font.name[0];

	pgBlockMove(font.name, font_name, (font_name_size + 1) * sizeof(pg_char));
	
	return	(mask.name[0] != 0);
}


/* pgGetFontByNum returns the font, by number, applied to the current selection
range. If the same font is consistent throughout, TRUE is returned. */


PG_PASCAL (pg_boolean) pgGetFontByNum (pg_ref pg, short *font_num)
{
	font_info			font, mask;
	
	pgGetFontInfo(pg, NULL, FALSE, &font, &mask);
	GetFNum((StringPtr)font.name, font_num);
	
	return	(mask.name[0] != 0);
}


/* pgGetTextColor returns the text color applied to the current selection.
If the same text color is consistent throughout, TRUE is returned. */

PG_PASCAL (pg_boolean) pgGetTextColor (pg_ref pg, pg_plat_color_value *color)
{
	style_info			style, mask;
	
	pgGetStyleInfo(pg, NULL, FALSE, &style, &mask);
	pgBlockMove(&style.fg_color, color, sizeof(RGBColor));
	
	return	(mask.fg_color.red != 0
			&& mask.fg_color.green != 0
			&& mask.fg_color.blue != 0);
}


/* pgGetTextBKColor returns the background color applied to the current selection.
If the same background color is consistent throughout, TRUE is returned. */

PG_PASCAL (pg_boolean) pgGetTextBKColor (pg_ref pg, pg_plat_color_value *color)
{
	style_info			style, mask;
	
	pgGetStyleInfo(pg, NULL, FALSE, &style, &mask);
	pgBlockMove(&style.bk_color, color, sizeof(RGBColor));
	
	return	(mask.bk_color.red != 0
			&& mask.bk_color.green != 0
			&& mask.bk_color.blue != 0);
}

 #endif
 

/* pgSetStyleBits applies style bits to the current selection range (or to
the text range presented in selection_range if that parameter is non-NULL).
The style_bits are taken as a long so you can include extended PAIGE styles
in the bits (see definitions for PAIGE extended styles in header). Note that
all style settings ("true" bit values) cause an exclusive-or with any styles
that are consistent throughout the selection, otherwise the style is forced
to "on" state. If redraw is TRUE the changes are re-drawn. */

PG_PASCAL (void) pgSetStyleBits (pg_ref pg, long style_bits, long set_which_bits,
		const select_pair_ptr selection_range, pg_boolean redraw)
{
	style_info				new_style, current_style, mask;
	long					mask_bits;
	short					style_index, draw_mode;

	pgFillBlock(new_style.styles, MAX_STYLES * sizeof(short), 0);
	
	if (style_bits != 0 || set_which_bits != -1) {

		pgGetStyleInfo(pg, selection_range, FALSE, &current_style, &mask);
		QDStyleToPaige(style_bits, &new_style);

	  // SNG - ALL CAPS style and all lower case should be mutualy exclusive
		if (new_style.styles[all_caps_var] == -1 &&
			current_style.styles[all_lower_var] == -1) {
			current_style.styles[all_lower_var] = 0;
			set_which_bits |= X_ALL_LOWER_BIT;
	   	}
	
		if (new_style.styles[all_lower_var] == -1 &&
			current_style.styles[all_caps_var] == -1) {
	   		current_style.styles[all_caps_var] = 0;
	   		set_which_bits |= X_ALL_CAPS_BIT;
		}
	    
		for (style_index = 0; style_index < MAX_STYLES; ++style_index)
			if (mask.styles[style_index])
				if (new_style.styles[style_index])
					new_style.styles[style_index] ^= current_style.styles[style_index];
	}

	pgFillBlock(&mask, sizeof(style_info), 0);

	mask_bits = set_which_bits;

	for (style_index = 0; style_index < MAX_STYLES; ++style_index, mask_bits >>= 1)
		if (mask_bits & 1)
			mask.styles[style_index] = SET_MASK_BITS;
		else
			mask.styles[style_index] = 0;
	
	if (redraw)
		draw_mode = best_way;
	else
		draw_mode = draw_none;

// Cancel out any chance of altering unreal styles (that are relative, etc.)

	mask.styles[super_impose_var] = mask.styles[relative_point_var] = 0;

// Set good numbers for superscript, subscript and small caps:

	if (new_style.styles[small_caps_var])
		new_style.styles[small_caps_var] = 75;
	if (new_style.styles[superscript_var])
		new_style.styles[superscript_var] = 3;
	if (new_style.styles[subscript_var])
		new_style.styles[subscript_var] = 3;

	pgSetStyleInfo(pg, selection_range, &new_style, &mask, draw_mode);
}

/* pgSetPointsize changes the point size of the currently selected text or,
if selection_range is non-NULL it changes the text range specified. If redraw is
TRUE the changes are re-drawn. */

PG_PASCAL (void) pgSetPointSize (pg_ref pg, short point_size,
		const select_pair_ptr selection_range, pg_boolean redraw)
{
	style_info					style, mask;
	short						draw_mode;

	pgFillBlock(&mask, sizeof(style_info), 0);
	mask.point = SET_MASK_BITS;
	style.point = point_size;
	style.point <<= 16;

	if (redraw)
		draw_mode = best_way;
	else
		draw_mode = draw_none;

	pgSetStyleInfo(pg, selection_range, &style, &mask, draw_mode);
}

/* pgGetStyleBits returns the style(s) applied to the current selection range.
The style(s) that are true throughout will be in consistent_bits. Note that
true "plain" text will return 0 for *style_bits and -1 for *consistent_bits. */

PG_PASCAL (void) pgGetStyleBits (pg_ref pg, long PG_FAR *style_bits,
		long PG_FAR *consistent_bits)
{
	style_info			style, mask;
	
	pgGetStyleInfo(pg, NULL, FALSE, &style, &mask);
	
	*style_bits = PaigeToQDStyle(&style);
	*consistent_bits = PaigeToQDStyle(&mask);
}

/* pgGetPointsize returns the point size applied to the current selection.
If the same point size is consistent throughout, TRUE is returned. */

PG_PASCAL (pg_boolean) pgGetPointsize (pg_ref pg, short PG_FAR *point_size)
{
	style_info			style, mask;
	
	pgGetStyleInfo(pg, NULL, FALSE, &style, &mask);
	*point_size = pgHiWord(pgRoundFixed(style.point));
	
	return	(mask.point != 0);
}


PG_PASCAL (void) pgApplyToSelectedCells(memory_ref pg, select_pair_ptr selection,
											ChangeTableAttributesCallbackPtr callback,
											long user_value)
{
	par_info		info;
	par_info		mask;
	short			row;
	short			first_column,last_column;
	select_pair	 	row_offsets;
	select_pair		limits;
	select_pair		working_selection = *selection;
	
	pgTableOffsets (pg, selection->begin,&limits);

	if (working_selection.begin < limits.begin)
		working_selection.begin = limits.begin;

	if (working_selection.end > limits.end)
		working_selection.end = limits.end;
		
	while (working_selection.begin <= working_selection.end)
	{
		// Find the row we need to work inÉ
		row = (short)pgPositionToRow (pg,working_selection.begin,&row_offsets);
		if (row != -1)
		{
			// get the par_info for the row
			pgGetParInfo(pg,&row_offsets,FALSE,&info,&mask);
			pgFillBlock(&mask,sizeof(par_info),0);

			if (row_offsets.begin < selection->begin)
				row_offsets.begin = selection->begin;
				
			// Find the first column on which to work
			first_column = pgPositionToColumn(pg,row_offsets.begin);
			if (working_selection.end >= row_offsets.end)
				last_column = (short)info.table.table_columns;
			else
				last_column = pgPositionToColumn(pg,working_selection.end);
			
			while (first_column <= last_column)
			{
				callback(row,first_column,&info.tabs[first_column],&mask.tabs[first_column],user_value);
				++first_column;
			}

			pgSetParInfo (pg,&row_offsets,&info,&mask, draw_none);
		}

		working_selection.begin = row_offsets.end + 1;
	}
	
	{
		paige_rec_ptr pg_ptr = UseMemory(pg);
		pgUpdateText(pg_ptr, NULL, 0, pg_ptr->t_length, MEM_NULL, NULL, bits_copy, TRUE);
		UnuseMemory(pg);
	}
}


