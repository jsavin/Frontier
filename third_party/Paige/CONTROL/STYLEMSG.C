/* This source file handles all the PAIGE messages dealing with text styles and fonts. It handles
all "PG_STYLEMESSAGES" messages. */


#include "Paige.h"
#include "pgTraps.h"
#include "machine.h"
#include "pgUtils.h"
#include "defprocs.h"
#include "pgdefstl.h"
#include "pgCtlUtl.h"
#include "pgCntl.h"


static void convert_log_font (pg_ref pg, LOGFONT PG_FAR *log_font,
		font_info_ptr font, style_info_ptr style, style_info_ptr stylemask);
static void convert_universal_font (pg_ref pg, PG_LOGFONT PG_FAR *log_font,
		font_info_ptr font, style_info_ptr style, style_info_ptr stylemask,
		pg_boolean convert_to_struct);
static void convert_par_info (par_info_ptr par, PAIGEPARSTRUCT PG_FAR *parstruct,
			pg_boolean convert_to_struct);

long pascal StyleMessageProc (HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
#ifdef WINDOWS_COMPILE
	LOGFONT PG_FAR			*log_font;
#endif
	paige_control_ptr		pg_stuff;			// One of these in each control
	memory_ref				pg_stuff_ref;		// A memory_ref that holds above
	PAIGENAMEDSTYLE PG_FAR	*named_style;
	named_stylesheet		paige_style;
	font_info				font, fontmask;
	style_info				style, stylemask, new_style, new_mask;
	par_info				par;
	pg_short_t				namesize;
	long					response, stylesheet_index;
	short					draw_mode;

	response = 0;
	
	if (!(pg_stuff = GetPGStuff(hWnd, &pg_stuff_ref)))
		return	0L;

/* The following sets the "draw_mode" for all functions in which wParam
is TRUE or FALSE for re-draw. If wParam is used for something else it
does not matter because we will not use "draw_mode" variable. */

    if (wParam)
    	draw_mode = pg_stuff->update_mode;
    else
       draw_mode = draw_none;

	switch (message) {

		case PG_SETFONT:
		case PG_SETFONTONLY:
			PrepareStyleUndo(pg_stuff);

			pgFillBlock(&fontmask, sizeof(font_info), -1);

#ifdef MAC_PLATFORM
			{
				pg_char_ptr		fontname;

				if (message == PG_SETFONTONLY) {
					pg_short_t		fontname_size;
					
					fontname = (pg_char_ptr)lParam;
					fontname_size = (pg_short_t)*fontname;
					pgFillBlock(&font, sizeof(font_info), 0);
					
					if (fontname_size > (FONT_SIZE - 2))
						fontname_size = FONT_SIZE - 2;
					
					pgBlockMove(fontname, font.name, (fontname_size + 1) * sizeof(pg_char));
					pgSetFontInfo(pg_stuff->pg, NULL, &font, &fontmask, draw_mode);
				}
				else {
					LOGFONT		*log_font;
					
					log_font = (LOGFONT *)lParam;
					convert_log_font(pg_stuff->pg, log_font, &font, &style, &stylemask);
					pgSetStyleAndFont(pg_stuff->pg, NULL, &style, &stylemask, &font, &fontmask, draw_mode);
				}					
			}
#endif
#ifdef WINDOWS_PLATFORM
			log_font = (LOGFONT PG_FAR *)lParam;
			convert_log_font(pg_stuff->pg, log_font, &font, &style, &stylemask);
			
			if (message == PG_SETFONT)
				pgSetStyleAndFont(pg_stuff->pg, NULL, &style, &stylemask,
						&font, &fontmask, draw_mode);
			else
				pgSetFontInfo(pg_stuff->pg, NULL, &font, &fontmask, draw_mode);

#endif

			if (draw_mode)
				NotifyOverflow(pg_stuff);

			break;

		case PG_GETFONT:
			#ifdef MAC_PLATFORM
				message = PG_GETFONTNAME;
			#endif
			
		case PG_GETFONTNAME:
			
			pgGetFontInfo(pg_stuff->pg, NULL, FALSE, &font, &fontmask);
			
			// Returns FALSE if font not consistent:
			
			response = (fontmask.name[0] != 0);

			if (message == PG_GETFONTNAME) {
				PGSTR		fontname;
				
				fontname = (PGSTR)lParam;
				namesize = (pg_short_t)font.name[0];
				
	#ifdef MAC_PLATFORM
				BlockMove(font.name, fontname, (namesize + 1) * sizeof(pg_char));
	#endif
	#ifdef WINDOWS_PLATFORM
				if (namesize)
					pgBlockMove(&font.name[1], fontname, namesize * sizeof(pg_char));

				fontname[namesize] = 0;
	#endif
				break;
			}
	
	#ifdef WINDOWS_PLATFORM
	
			log_font = (LOGFONT PG_FAR *)lParam;
			pgFillBlock(log_font, sizeof(LOGFONT), 0);

			if (namesize = (pg_short_t)font.name[0])
				pgBlockMove(&font.name[1], log_font->lfFaceName, namesize * sizeof(pg_char));
			
			log_font->lfFaceName[namesize] = 0;
			log_font->lfPitchAndFamily = (BYTE)font.family_id;
			log_font->lfOutPrecision = (BYTE)font.machine_var[PG_OUT_PRECISION];
			log_font->lfClipPrecision = (BYTE)font.machine_var[PG_CLIP_PRECISION];
			log_font->lfQuality = (BYTE)font.machine_var[PG_QUALITY];
			log_font->lfCharSet = (BYTE)font.machine_var[PG_CHARSET];
			
			pgGetStyleInfo(pg_stuff->pg, NULL, FALSE, &style, &stylemask);
			
			style.point = pgPointsizeToScreen(pg_stuff->pg, style.point);
			style.point >>= 16;
			log_font->lfHeight = (short)style.point;
			log_font->lfHeight = -log_font->lfHeight;
			
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
		#endif

			break;

		case PG_SETTEXTSTYLES:
			pgFillBlock(&stylemask, sizeof(style_info), 0);
			pgFillBlock(&style, sizeof(style_info), 0);
			
			PrepareStyleUndo(pg_stuff);

			if (lParam == 0) {
			
				pgFillBlock(stylemask.styles, MAX_STYLES * sizeof(short), -1);
				stylemask.styles[super_impose_var] = 0;
			}
			else {
				short				style_index;

				pgGetStyleInfo(pg_stuff->pg, NULL, FALSE, &new_style, &new_mask);
				QDStyleToPaige(lParam, &style);

				if (new_style.styles[small_caps_var])
					new_style.styles[small_caps_var] = -1;
				if (new_style.styles[superscript_var])
					new_style.styles[superscript_var] = -1;
				if (new_style.styles[subscript_var])
					new_style.styles[subscript_var] = -1;

				for (style_index = 0; style_index < MAX_STYLES; ++style_index)
					
					if (style.styles[style_index]) {
						
						stylemask.styles[style_index] = -1;

						if (new_mask.styles[style_index])
							style.styles[style_index] ^= new_style.styles[style_index];
					}

				stylemask.styles[super_impose_var] = stylemask.styles[relative_point_var] = 0;

				if (style.styles[small_caps_var])
					style.styles[small_caps_var] = 75;
				if (style.styles[superscript_var])
					style.styles[superscript_var] = 3;
				if (style.styles[subscript_var])
					style.styles[subscript_var] = 3;
            
            }

			pgSetStyleInfo(pg_stuff->pg, NULL, &style, &stylemask, draw_mode);

			if (draw_mode)
				NotifyOverflow(pg_stuff);
			break;

		case PG_GETTEXTSTYLES:
			pgGetStyleInfo(pg_stuff->pg, NULL, FALSE, &style, &stylemask);
	
			response = PaigeToQDStyle(&style);
			
			if (lParam) {
				long PG_FAR *consistent_bits;
				
				consistent_bits = (long PG_FAR *)lParam;
				*consistent_bits = PaigeToQDStyle(&stylemask);
			}

			break;


		case PG_SETFONTSIZE:
			PrepareStyleUndo(pg_stuff);
			pgFillBlock(&stylemask, sizeof(style_info), 0);
			stylemask.point = -1;
            
			style.point = lParam << 16;
			style.point = pgScreenToPointsize(pg_stuff->pg, style.point);
			pgSetStyleInfo(pg_stuff->pg, NULL, &style, &stylemask, draw_mode);

			if (draw_mode)
				NotifyOverflow(pg_stuff);

			break;

		case PG_GETFONTSIZE:
			pgGetStyleInfo(pg_stuff->pg, NULL, FALSE, &style, &stylemask);
			style.point = pgPointsizeToScreen(pg_stuff->pg, style.point);
			response = style.point >> 16;
			break;

		case PG_SETTEXTCOLOR:
			PrepareStyleUndo(pg_stuff);
			pgFillBlock(&stylemask, sizeof(style_info), 0);
			pgFillBlock(&stylemask.fg_color, sizeof(color_value), -1);
            
        #ifdef MAC_PLATFORM
			pgOSToPgColor((void PG_FAR *)lParam, &style.fg_color);
        #else
			pgOSToPgColor((void PG_FAR *)&lParam, &style.fg_color);
		#endif

			pgSetStyleInfo(pg_stuff->pg, NULL, &style, &stylemask, draw_mode);
			break;

		case PG_GETTEXTCOLOR:
			pgGetStyleInfo(pg_stuff->pg, NULL, FALSE, &style, &stylemask);
		#ifdef MAC_PLATFORM
			pgColorToOS(&style.fg_color, (void PG_FAR *)lParam);
		#else
			pgColorToOS(&style.fg_color, (void PG_FAR *)&response);
		#endif

			break;

		case PG_SETTEXTBKCOLOR:
			PrepareStyleUndo(pg_stuff);
			pgFillBlock(&stylemask, sizeof(style_info), 0);
			pgFillBlock(&stylemask.bk_color, sizeof(color_value), -1);

        #ifdef MAC_PLATFORM
			pgOSToPgColor((void PG_FAR *)lParam, &style.bk_color);
        #else
			pgOSToPgColor((void PG_FAR *)&lParam, &style.bk_color);
		#endif

			pgSetStyleInfo(pg_stuff->pg, NULL, &style, &stylemask, draw_mode);
			break;

		case PG_GETTEXTBKCOLOR:
			pgGetStyleInfo(pg_stuff->pg, NULL, FALSE, &style, &stylemask);
		#ifdef MAC_PLATFORM
			pgColorToOS(&style.bk_color, (void PG_FAR *)lParam);
		#else
			pgColorToOS(&style.bk_color, (void PG_FAR *)&response);
		#endif
			break;

		case PG_SETLEADING:
			PrepareStyleUndo(pg_stuff);
			pgFillBlock(&stylemask, sizeof(style_info), 0);
			stylemask.styles[superscript_var] = stylemask.styles[subscript_var] = -1;
			style.styles[superscript_var] = style.styles[subscript_var] = 0;
			
			if (lParam < 0)
				style.styles[superscript_var] = -(short)lParam;
			else
				style.styles[subscript_var] = (short)lParam;

			pgSetStyleInfo(pg_stuff->pg, NULL, &style, &stylemask, draw_mode);

			if (draw_mode)
				NotifyOverflow(pg_stuff);
			break;

		case PG_GETLEADING:
			pgGetStyleInfo(pg_stuff->pg, NULL, FALSE, &style, &stylemask);
			
			if (style.styles[superscript_var])
				response = (long)-style.styles[superscript_var];
			else
				response = (long)style.styles[subscript_var];
			break;

		case PG_SETCHARSPACING:
		case PG_SETEXTRASPACE:
			PrepareStyleUndo(pg_stuff);
			pgFillBlock(&stylemask, sizeof(style_info), 0);
			
			if (message == PG_SETCHARSPACING) {
			
				stylemask.char_extra = (pg_fixed)-1;
				style.char_extra = lParam;
			}
			else {
			
				stylemask.space_extra = (pg_fixed)-1;
				style.space_extra = lParam;
			}

			pgSetStyleInfo(pg_stuff->pg, NULL, &style, &stylemask, draw_mode);

			if (draw_mode)
				NotifyOverflow(pg_stuff);

			break;

		case PG_GETCHARSPACING:
		case PG_GETEXTRASPACE:
			pgGetStyleInfo(pg_stuff->pg, NULL, FALSE, &style, &stylemask);
			
			if (message == PG_GETCHARSPACING)
				response = style.char_extra;
			else
				response = style.space_extra;

			break;
			
		case PG_SETALTERNATEFONT:
			break;

		case PG_MAKESTYLESHEET:
		case PG_MAKEPORTABLESHEET:
			response = AddPGStylesheet(pg_stuff->pg, message,  (pg_boolean)wParam, (LOGFONT PG_FAR *)lParam);
			break;

		case PG_APPLYSTYLESHEET:
			pgSetStyleSheet(pg_stuff->pg, NULL, (short)lParam, draw_mode);

			if (draw_mode)
				NotifyOverflow(pg_stuff);
			break;

		case PG_MAKENAMEDSTYLE:
			named_style = (PAIGENAMEDSTYLE PG_FAR *)lParam;
			convert_universal_font(pg_stuff->pg, &named_style->style, &font, &style, &stylemask, FALSE);
			convert_par_info(&par, &named_style->par_style, FALSE);
			response = pgNewNamedStyle(pg_stuff->pg, (pg_char_ptr)named_style->name, &style, &font, &par);
			break;

		case PG_GETAPPLIEDNAMEDSTYLE:
		case PG_GETNAMEDSTYLE:
			
			if (message == PG_GETAPPLIEDNAMEDSTYLE) {
			
				pgGetStyleInfo(pg_stuff->pg, NULL, FALSE, &style, &stylemask);
				stylesheet_index = style.named_style_index;
			}
			else
				stylesheet_index = wParam;
			
			if (!pgGetNamedStyle(pg_stuff->pg, stylesheet_index, &paige_style))
				break;

			named_style = (PAIGENAMEDSTYLE PG_FAR *)lParam;

			// Returns the named style rec based on the index.
			
			pgBlockMove(paige_style.name, named_style->name, FONT_SIZE * sizeof(pg_char));
			response = stylesheet_index;
			pgGetNamedStyleInfo(pg_stuff->pg, stylesheet_index, &style, &font, &par);
			convert_universal_font(pg_stuff->pg, &named_style->style, &font, &style, &stylemask, TRUE);
			convert_par_info(&par, &named_style->par_style, TRUE);
			break;
			

		case PG_NUMNAMEDSTYLES:
			response = pgNumNamedStyles(pg_stuff->pg);
			break;

		case PG_APPLYNAMEDSTYLE:
			pgApplyNamedStyleIndex(pg_stuff->pg, NULL, lParam, draw_mode);
			break;

		case PG_FINDNAMEDSTYLE:
			response = pgGetNamedStyleIndex(pg_stuff->pg, (pg_char_ptr)lParam);
			break;

		case PG_RENAMESTYLE:
			pgRenameStyle(pg_stuff->pg, wParam, (pg_char_ptr)lParam);
			break;

		case PG_DELETENAMEDSTYLE:
			pgDeleteNamedStyle(pg_stuff->pg, wParam);
			break;
			
		default:
			response = DefWindowProc(hWnd, message, wParam, lParam);
			break;
	}

	UnuseMemory(pg_stuff_ref);

	return	response;
}


/* AddPGStylesheet adds a direct stylesheet to the pg_ref. */

long AddPGStylesheet (pg_ref pg, long message, pg_boolean change_existing, LOGFONT PG_FAR *log_font)
{
	font_info			font;
	style_info			style, stylemask;
	long				response;

	if (message == PG_MAKESTYLESHEET)
		convert_log_font(pg, log_font, &font, &style, &stylemask);
	else
		convert_universal_font(pg, (PG_LOGFONT PG_FAR *)log_font, &font, &style, &stylemask, FALSE);

	if (change_existing)
		pgChangeStyle(pg, change_existing, &style, &font, draw_none);
	else {
		paige_rec_ptr		pg_rec;
		
		pg_rec = UseMemory(pg);
		style.font_index = pgAddNewFont(pg_rec, &font);
		UnuseMemory(pg);

		if (!(response = pgFindStyleSheet(pg, &style, NULL)))
			response = pgNewStyle(pg, &style, &font);
	}
	
	return		response;
}


/************************************* Local Functions ************************************/

/* convert_log_font converts a LOGFONT struct to the font and style for PAIGE. */

static void convert_log_font (pg_ref pg, LOGFONT PG_FAR *log_font,
		font_info_ptr font, style_info_ptr style, style_info_ptr stylemask)
{
	pgFillBlock(font, sizeof(font_info), 0);
	pgFillBlock(style, sizeof(style_info), 0);
	pgFillBlock(stylemask, sizeof(style_info), 0);
	pgFillBlock(stylemask->styles, MAX_STYLES * sizeof(short), -1);
	stylemask->point = (pg_fixed)-1;
	style->procs = paige_globals.def_style.procs;

#ifdef MAC_PLATFORM

#pragma unused (pg)

	pgFillBlock(&stylemask->fg_color, sizeof(color_value), -1);
	BlockMove(log_font->fontName, font->name, (log_font->fontName[0] + 1) * sizeof(pg_char));
	style->point = log_font->pointsize << 16;
	QDStyleToPaige(log_font->styles, style);
	style->styles[relative_point_var] = log_font->relPointsize;
	BlockMove(&log_font->textColor, &style->fg_color, sizeof(RGBColor));
#endif
#ifdef WINDOWS_PLATFORM
	CToPString(log_font->lfFaceName, font->name);
	font->family_id = log_font->lfPitchAndFamily;
	font->machine_var[PG_OUT_PRECISION] = log_font->lfOutPrecision;
	font->machine_var[PG_CLIP_PRECISION] = log_font->lfClipPrecision;
	font->machine_var[PG_QUALITY] = log_font->lfQuality;
	font->machine_var[PG_CHARSET] = log_font->lfCharSet;
	
	if ((style->point = (pg_fixed)log_font->lfHeight) < 0)
		style->point = -style->point;
	
	style->point <<= 16;
	style->point = pgScreenToPointsize(pg, style->point);

	if (log_font->lfWeight == FW_BOLD)
		style->styles[bold_var] = -1;
	if (log_font->lfItalic)
		style->styles[italic_var] = -1;
	if (log_font->lfUnderline)
		style->styles[underline_var] = -1;
	if (log_font->lfStrikeOut)
		style->styles[strikeout_var] = -1;
#endif

	style->bk_color = paige_globals.def_style.bk_color;
}


/* convert_universal_font converts the logfont struct universal to both platforms.
If convert_to_struct is TRUE then we convert FROM a style_info, etc. to the logfont. */

static void convert_universal_font (pg_ref pg, PG_LOGFONT PG_FAR *log_font,
		font_info_ptr font, style_info_ptr style, style_info_ptr stylemask,
		pg_boolean convert_to_struct)
{
#ifdef MAC_PLATFORM
#pragma unused (pg)
#endif

	if (convert_to_struct) {
		
		pgFillBlock(log_font, sizeof(PG_LOGFONT), 0);
	#ifdef WINDOWS_COMPILE
		pgBlockMove(&font->name[1], log_font->fontName, (FONT_SIZE - 1) * sizeof(pg_char));
	#else
		BlockMove(font->name, log_font->fontName, FONT_SIZE * sizeof(pg_char));
	#endif
		log_font->pointsize = pgHiWord(style->point);
		log_font->styles = PaigeToQDStyle(style);
		log_font->relPointsize = style->styles[relative_point_var];
		BlockMove(&style->fg_color, &log_font->textColor, sizeof(RGBColor));
	}
	else {
	
		pgFillBlock(font, sizeof(font_info), 0);
		*style = paige_globals.def_style;
		pgFillBlock(stylemask, sizeof(style_info), 0);
		pgFillBlock(stylemask->styles, MAX_STYLES * sizeof(short), -1);
		stylemask->point = (pg_fixed)-1;
		pgFillBlock(&stylemask->fg_color, sizeof(color_value), -1);
	#ifdef WINDOWS_COMPILE
		CToPString(log_font->fontName, font->name);
	#else
		BlockMove(log_font->fontName, font->name, (log_font->fontName[0] + 1) * sizeof(pg_char));
	#endif
		style->point = log_font->pointsize << 16;
		QDStyleToPaige(log_font->styles, style);
		style->styles[relative_point_var] = (short)log_font->relPointsize;
		BlockMove(&log_font->textColor, &style->fg_color, sizeof(RGBColor));
	}
}


/* convert_par_info converts a par_info rec to a PAIGEPARSTRUCT or vise versa. */

static void convert_par_info (par_info_ptr par, PAIGEPARSTRUCT PG_FAR *parstruct,
			pg_boolean convert_to_struct)
{
	if (convert_to_struct) {
		
		pgFillBlock(parstruct, sizeof(PAIGEPARSTRUCT), 0);
		
		parstruct->left_indent = par->indents.left_indent;
		parstruct->right_indent = par->indents.right_indent;
		parstruct->first_indent = par->indents.first_indent;
		parstruct->justification = (short)par->justification;
		parstruct->num_tabs = par->num_tabs;
		pgBlockMove(par->tabs, parstruct->tabs, TAB_ARRAY_SIZE * sizeof(PAIGETABSTRUCT));
		parstruct->spacing = (short)par->leading_fixed;
		parstruct->space_after = (short)par->bot_extra;
		parstruct->space_before = (short)par->top_extra;
		parstruct->flags = par->class_info;
	}
	else {
		
		*par = paige_globals.def_par;

		par->indents.left_indent = parstruct->left_indent;
		par->indents.right_indent = parstruct->right_indent;
		par->indents.first_indent = parstruct->first_indent;
		par->justification = parstruct->justification;
		par->num_tabs = parstruct->num_tabs;
		pgBlockMove(parstruct->tabs, par->tabs, TAB_ARRAY_SIZE * sizeof(PAIGETABSTRUCT));
		par->leading_fixed = parstruct->spacing;
		par->bot_extra = parstruct->space_after;
		par->top_extra = parstruct->space_before;
		par->class_info = parstruct->flags;
	}
}
