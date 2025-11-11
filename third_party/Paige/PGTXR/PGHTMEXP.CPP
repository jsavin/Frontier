/* Import-Export module. Copyright 1997 by DataPak Software, Inc.  This software is a
part of the total PAIGE library.

This source file contains all the member functions for the HTML export class (derived
from the PaigeExportFilter class. */


#include "Paige.h"
#include "pgTraps.h"
#include "pgExceps.h"
#include "machine.h"
#include "pgUtils.h"
#include "pgTxrCPP.h"
#include "defprocs.h"
#include "pgErrors.h"
#include "pgDefStl.h"
#include "pgHTMDef.h"
#include "pgEmbed.h"
#include "pgdeftbl.h"
#include "pgHText.h"
#include "pgOSUtl.h"

#define LIST_TAB_DIVIDE		8

static long translate_color (color_value_ptr color, long PG_FAR *redcolor,
         long PG_FAR *greencolor, long PG_FAR *bluecolor);
static pg_boolean plain_styles (style_info_ptr style);
static void make_hex_char (pg_char_ptr output, long value);
static void decimal_value (long PG_FAR *value, long minus_value, pg_char_ptr output,
			short PG_FAR *index);
static void find_data_char (pg_char_ptr chars, pg_char_ptr table, pg_char the_char, pg_char_ptr output);
static pg_boolean special_char (pg_char the_char);
static pg_boolean equal_color (color_value_ptr color1, color_value_ptr color2);
static pg_boolean equal_string (pg_char_ptr str1, pg_char_ptr str2);
static pg_boolean has_colons (pg_char_ptr URL);
static short find_font_index (paige_rec_ptr pg, pg_char_ptr fontname);
static pg_boolean is_fixed_font (font_info_ptr font);


/* PaigeHTMLExportFilter constructor. This sets up the various members common to all filters.
No error checking or file verification occurs just yet. */


PaigeHTMLExportFilter::PaigeHTMLExportFilter ()
{
   file_type = pg_rtf_type;   // Default type for this class
   feature_bits = EXPORT_TEXT_FEATURE | EXPORT_TEXT_FORMATS_FEATURE
               | EXPORT_PAR_FORMATS_FEATURE | EXPORT_PAGE_INFO_FEATURE
               | EXPORT_EMBEDDED_OBJECTS_FEATURE
               | EXPORT_UNICODE_FEATURE
               | EXPORT_HYPERTEXT_FEATURE
               | EXPORT_SERIAL_SETUP
               | EXPORT_STYLE2CR;

   pgFillBlock((void PG_FAR *)file_kind, KIND_STR_SIZE * sizeof(pg_char), 0);
   pgBlockMove((void PG_FAR *)FILETYPE_HTML, (void PG_FAR *)file_kind, FILETYPE_STR_SIZE * sizeof(pg_char));
   pgFillBlock(file_extension, 8, 0);

   pretag_font = pretag_pointsize = 0;
   chars_since_cr = 0;

#ifdef MAC_PLATFORM
   pgBlockMove(".html", file_extension, 5);
#else
   pgBlockMove(".htm", file_extension, 4);
#endif

   point_levels[0] = 0x00090000;
   point_levels[1] = 0x000A0000;
   point_levels[2] = 0x000C0000;
   point_levels[3] = 0x0000E0000;
   point_levels[4] = 0x000120000;
   point_levels[5] = 0x000180000;
   point_levels[6] = 0x000200000;

   params_ref = converter = MEM_NULL;
   image_ctr = real_text_out = font_tag_enable = 0;
   listing_level = listing_code = total_chars_in = column_ctr = row_char_count = 0;
   new_par = TRUE;
   doing_table = pretag_enable = FALSE;
   list_indent_value = DEFLIST_INDENT_VALUE;
   cell_attribute = table_dcell_command;
	
   space_width = space_last_font = space_last_point = 0;
}


/* PaigeImportFilter destructor. This removes common items. */

PaigeHTMLExportFilter::~PaigeHTMLExportFilter ()
{
 	DisposeNonNilMemory(params_ref);
  	DisposeNonNilMemory(converter);
}


/* pgPrepareImport sets up the necessary extra buffers to begin importing. */

pg_error PaigeHTMLExportFilter::pgPrepareExport (void)
{
	pg_error		result = NO_ERROR;
	pg_char_ptr		title_ptr;
	long			title_size, body_index, pretag_index, code_index;

	params_ref = MemoryAlloc(export_pg_rec->globals->mem_globals, sizeof(out_tag_attribute), 0, 4);
	converter = MemoryAlloc(export_pg_rec->globals->mem_globals, sizeof(pg_char), 0, 128);

	if ((pretag_index = pgGetNamedStyleIndex(export_pg, pre_tag_style)) > 0) {

		pgGetNamedStyleInfo(export_pg, pretag_index, &current_style, NULL, NULL);
		pretag_font = current_style.font_index;
		current_style.point >>= 16;
		pretag_pointsize = (short)current_style.point;
	}
	else {
		
		pretag_pointsize = PRETAG_DEF_POINTSIZE;
		pretag_font = find_font_index(export_pg_rec, PRETAG_DEF_FONT);
	}

	if ((code_index = pgGetNamedStyleIndex(export_pg, code_style)) > 0) {

		pgGetNamedStyleInfo(export_pg, code_index, &current_style, NULL, NULL);
		codetag_font = current_style.font_index;
		current_style.point >>= 16;
		codetag_pointsize = (short)current_style.point;
	}
	else {
		
		codetag_pointsize = CODETAG_DEF_POINTSIZE;
		codetag_font = find_font_index(export_pg_rec, CODETAG_DEF_FONT);
	}


	if ((body_index = pgGetNamedStyleIndex(export_pg, body_style)) > 0)
		pgGetNamedStyleInfo(export_pg, body_index, &current_style, NULL, &current_par);
	else {
		
		GetMemoryRecord(export_pg_rec->t_formats, 0, &current_style);
		GetMemoryRecord(export_pg_rec->par_formats, 0, &current_par);
	}
	
	default_style = current_style;

	if (GetMemorySize(export_pg_rec->hyperlinks) > 1)
		GetMemoryRecord(export_pg_rec->hyperlinks, 0, &default_source);
	else
    	pgInitDefaultSource(export_pg, &default_source);

    pgInitDefaultTarget(export_pg, &default_target);

	WriteTag(control_commands, html_command, TRUE, TRUE);
	WriteTag(control_commands, heading_command, TRUE, TRUE);
	WriteTag(control_commands, title_command, TRUE, FALSE);
	
	if (!export_pg_rec->doc_info.title)
		export_pg_rec->doc_info.title = MemoryAllocClear(export_pg_rec->globals->mem_globals,
						sizeof(pg_char), 1, 32);
	title_ptr = (pg_char_ptr)UseMemory(export_pg_rec->doc_info.title);
	title_size = pgCStrLength(title_ptr);
	
	if (!title_size) {
		
		title_size = pgCStrLength(untitled_title);
		UnuseMemory(export_pg_rec->doc_info.title);
		SetMemorySize(export_pg_rec->doc_info.title, title_size + 1);
		title_ptr = (pg_char_ptr)UseMemory(export_pg_rec->doc_info.title);
		pgBlockMove(untitled_title, title_ptr, title_size + 1);
	}
	
	pgWriteString(title_ptr, 0, 0);
	UnuseMemory(export_pg_rec->doc_info.title);

	WriteTag(control_commands, -title_command, TRUE, TRUE);
	WriteTag(control_commands, -heading_command, TRUE, TRUE);

// Start body command:

	PreProcessStyleChange(&current_style, &current_style, -1);
	PreProcessParChange(&current_par, &current_par, -1);

	InsertColorParameter(bk_color_attribute, control_attributes, &export_pg_rec->bk_color, 0);

	WriteTag(control_commands, body_command, TRUE, TRUE);
	
	new_par = TRUE;

	return	result;
}


/* pgImportDone disposes all structs created for pgPrepareImport(). */

pg_error PaigeHTMLExportFilter::pgExportDone (void)
{
    SetMemorySize(params_ref, 0);
    EndFontTag();
    
    while (listing_level) {
    	
    	TerminateList((short)listing_code);
    	--listing_level;
    }
 
    WriteTag(control_commands, -body_command, TRUE, TRUE);
    WriteTag(control_commands, -html_command, TRUE, TRUE);

    pgFlushOutputBuffer();

   return   io_proc(NULL, io_set_eof, &filepos, &filepos, filemap);
}


/* pgReadNextBlock is the major entry point that reads the next block of text, sets up paragraph
and text formats and returns something for the base class to insert into the pg_ref. */

pg_boolean PaigeHTMLExportFilter::pgWriteNextBlock (void)
{
	pg_char_ptr			text;
	pg_char_ptr			URL, converted_URL;
	long				num_chars, char_skip;

	text = (pg_char_ptr)UseMemory(translator.data);
	num_chars = GetMemorySize(translator.data);
	styles_output = FALSE;

	OutputStyleEndTags();

	if (new_par) {
		
		chars_since_cr = 0;

		if (doing_table) {
			
			if (translator.par_format.table.table_columns == 0) {
				
				WriteTag(table_data_commands, -table_begin_command, TRUE, TRUE);
				doing_table = FALSE;
			}
			else {
				
				column_ctr = 0;
				WriteTag(table_data_commands, table_row_command, TRUE, FALSE);
				OutputTableCell();
				new_par = FALSE;
				OutputStyleTags(text, num_chars);
			}
		}

		if (!doing_table) {
			
			OutputStyleTags(text, num_chars);
			OutputParChanges();
			
			if (listing_level && *text != 0x0D)
				WriteListingTag();
		}
	}

	if (translator.hyperlink_target_changed && translator.hyperlink_target.type == HYPERLINK_NORMAL) {

		if (translator.hyperlink_target.alt_URL)
			URL = (pg_char_ptr)UseMemory(translator.hyperlink_target.alt_URL);
		else
			URL = translator.hyperlink_target.URL;
		
		if (URL[0])
			InsertParameter(link_name_attribute, data_attributes, URL, (short)pgCStrLength(URL));

		if (translator.hyperlink_target.alt_URL)
			UnuseMemory(translator.hyperlink_target.alt_URL);
		
		WriteTag(data_commands, hyperlink_command, TRUE, FALSE);
		WriteTag(data_commands, -hyperlink_command, TRUE, FALSE);
	}
	
	if (translator.hyperlink_changed) {
		
		if (translator.hyperlink.alt_URL)
			URL = (pg_char_ptr)UseMemory(translator.hyperlink.alt_URL);
		else
			URL = translator.hyperlink.URL;
		
		if (URL[0]) {
			long		url_size;
			
			url_size = pgCStrLength(URL);
			SetMemorySize(converter, url_size + 6);
			converted_URL = (pg_char_ptr)UseMemory(converter);
			ConvertURL(URL, converted_URL);
		}
		else {
			
			SetMemorySize(converter, 3);
			converted_URL = (pg_char_ptr)UseMemory(converter);
			converted_URL[0] = converted_URL[1] = '?';
			converted_URL[2] = 0;
			InsertErrorLog(EMPTY_LINK_ERROR, total_chars_in, total_chars_in + 1, NULL, FALSE);
		}

		InsertParameter(link_url_attribute, data_attributes, converted_URL, (short)pgCStrLength(converted_URL));
		UnuseMemory(converter);

		if (translator.hyperlink.alt_URL)
			UnuseMemory(translator.hyperlink.alt_URL);
		
		WriteTag(data_commands, hyperlink_command, TRUE, FALSE);
	}

	if (translator.format.embed_object) {
	
		OutputImage((memory_ref)translator.format.embed_object, (embed_callback)translator.format.embed_entry,
				text, num_chars);
		
		total_chars_in += 2;
	}
	else {
		
		while (num_chars) {
			
			if (!styles_output)
				OutputStyleTags(text, num_chars);

			if (num_chars > 1)
				if (*text == '@' && text[1] == '@')
					char_skip = 0;
					
			if (new_par) {
				
				chars_since_cr = 0;

				if (doing_table) {

					column_ctr = 0;
					WriteTag(table_data_commands, table_row_command, TRUE, FALSE);
					OutputTableCell();
				}
				else
				if (listing_level && *text != 0x0D)
					WriteListingTag();
			}

			new_par = FALSE;

			if (*text < ' ') {
				long		cr_tag_flag;
				
				if (*text == 0x09) {
				
					if (doing_table) {
										
						if (column_ctr)
							WriteTag(table_data_commands, -cell_attribute, TRUE, FALSE);

						++column_ctr;
						OutputTableCell();
						++row_char_count;
					}
					else
					if (is_fixed_font(&translator.font))
						EmulateTab();
					else
						pgWriteChar(' ');
				}
				else
				if (*text == 0x0D || *text == 0x0A) {
					
					if (*text == 0x0D)
						new_par = TRUE;

					if (doing_table) {
						
						if (*text == 0x0D) {
							
							WriteTag(table_data_commands, -cell_attribute, TRUE, FALSE);
							WriteTag(table_data_commands, -table_row_command, TRUE, TRUE);
							
							row_char_count = 0;
							column_ctr = 0;
						}
						else
							WriteBreak(FALSE);
					}
					else {
						
						if (current_style.html_style == pre_tag_stylesheet)
							OutputCR();
						else
						if ((cr_tag_flag = ValidCR(text, num_chars)) > 0) {
						
							WriteBreak((pg_boolean)((cr_tag_flag > 1) || (listing_level != 0)));
							--cr_tag_flag;
							
							if (cr_tag_flag > (num_chars - 1))
								cr_tag_flag = num_chars - 1;

							text += cr_tag_flag;
							num_chars -= (long)cr_tag_flag;
							total_chars_in += cr_tag_flag;
						}
					}

					previous_par = current_par;
				}
			}
			else {
				
				if (special_char(*text))
					OutputSpecialChar(*text);
				else
					pgWriteByte(*text);
				
				++real_text_out;
				++row_char_count;
				++chars_since_cr;
			}

			++text;
			++total_chars_in;
			--num_chars;
		}
	}

	UnuseMemory(translator.data);
	
	if (translator.hyperlink_changed && real_text_out)
		WriteTag(data_commands, -hyperlink_command, TRUE, FALSE);

	return	TRUE;
}

/* ValidCR returns TRUE if the next char (which is a CR) is a valid "break" or
"paragraph" and if so returns 1 for <br> and 2 for <p>. */

long PaigeHTMLExportFilter::ValidCR (pg_char_ptr text, long remaining_chars)
{
	long		old_code, new_code, old_indent, new_indent;
	long		num_crs = 1;
	short		bottom_extra = 0;
	
	if (remaining_chars > 1) {
	
		if (text[1] == 0x0D)
			++num_crs;
	}

	if (translator.par_format.bot_extra >= 6) {
		
		bottom_extra = (short)(translator.par_format.bot_extra / 16);
		
		if (bottom_extra == 0)
			bottom_extra = 1;
		
		num_crs += bottom_extra;
	}

	if (num_crs == remaining_chars) {
		
		if (styles_output)
			if (translator.format.html_style >= heading1_stylesheet && translator.format.html_style <= heading6_stylesheet)
				if (next_style->html_style < heading1_stylesheet || next_style->html_style > heading6_stylesheet)
					num_crs -= 2;

		if (translator.par_format.justification != next_par->justification)
			if (next_par->justification == justify_center || next_par->justification == justify_right)
				num_crs -= 2;
		
		if (styles_output)
			if (next_style->html_style != pre_tag_stylesheet)
				if (translator.format.html_style == pre_tag_stylesheet || previous_style.html_style == pre_tag_stylesheet)
					num_crs -= 1;

		if (translator.par_format.html_style != next_par->html_style) {
		
			num_crs -= 1;
			
			if (!next_par->html_style)
				num_crs -= 1;
		}
		
		if (ComputeListingCodes(&current_par, next_par, &old_indent, &new_indent,
				&old_code, &new_code))
			num_crs -= 1;

		if ((translator.par_format.table.border_info & PG_BORDER_BOTTOM) == PG_BORDER_LINERULE)
			num_crs -= 2;
	}

// Check previous paragraph to see if the current CR is valid or not.

	if (listing_level || ComputeListingCodes(&previous_par, &translator.par_format, &old_indent, &new_indent,
			&old_code, &new_code))
		num_crs -= 1;
	
	//if (previous_style.html_style >= heading1_stylesheet && previous_style.html_style <= heading6_stylesheet)
	//	if (translator.format.html_style < heading1_stylesheet || translator.format.html_style > heading6_stylesheet)
	//		num_crs -= 2;
	
	if (num_crs < 0)
		num_crs = 0;
	
	return	num_crs;
}

/* IsPretagStyle returns TRUE if the style is pretag. */

pg_boolean PaigeHTMLExportFilter::IsPretagStyle (style_info_ptr style)
{
	long		pointsize;
	
	pointsize = style->point >> 16;
	
	if (style->html_style == pre_tag_stylesheet)
		return	TRUE;

	return	(pg_boolean)(style->font_index == pretag_font && pointsize == style->point);
}


/* EmulateTab sends out space char(s) to emulate a tab condition. (Typically used in "pre" tags).
The chars_since_cr indicate the number of characters since the last hard break. */

void PaigeHTMLExportFilter::EmulateTab (void)
{
	long			char_pixels, new_pixels, num_tabs, tab_space;
	long			this_pointsize;

	this_pointsize = translator.format.point >> 16;

	if (space_width == 0 || space_last_font != translator.format.font_index || space_last_point != (short)this_pointsize)
		space_width = pgGetCharWidth(export_pg_rec, &translator.format, (pg_char)' ');
	
	space_last_font = translator.format.font_index;
	space_last_point = (short)this_pointsize;

	if ((tab_space = translator.par_format.def_tab_space) == 0)
		tab_space = 28;

	char_pixels = chars_since_cr * space_width;
	char_pixels += translator.par_format.indents.left_indent;
	num_tabs = char_pixels / tab_space;			// = number of default "tabs" so far
	num_tabs += 1;
	
	new_pixels = num_tabs * tab_space;

	while (char_pixels < new_pixels) {
		
		pgWriteChar(' ');
		char_pixels += space_width;
		chars_since_cr += 1;
	}
}


/* PreProcessStyleChange is a virtual function that would allow custom output when the style
changes in any way. If doc_position is -1 then this is the first initialization call
before any text is output. */

void PaigeHTMLExportFilter::PreProcessStyleChange (style_info_ptr old_style, style_info_ptr new_style,
				long doc_position)
{

}

/* PreProcessParChange is a virtual function that would allow custom output when the par format
changes in any way. If doc_position is -1 then this is the first initialization call
before any text is output. */

void PaigeHTMLExportFilter::PreProcessParChange (par_info_ptr old_style, par_info_ptr new_style,
				long doc_position)
{

}


/* ConvertURL appends the appropriate extension to the URL. This function can be
overridden or the "extension" can be nulled in this->file_extension. */

void PaigeHTMLExportFilter::ConvertURL (pg_char_ptr input, pg_char_ptr output)
{
	long		input_size, output_size, extension_size;
	
	extension_size = pgCStrLength(file_extension);
	input_size = pgCStrLength(input);
	
	if (!extension_size || !input_size || has_colons(input))
		pgBlockMove(input, output, input_size + 1);
	else {
		pg_char_ptr			in_ptr, out_ptr;
		
		in_ptr = input;
		out_ptr = output;
		
		while (*in_ptr) {
			
			if (*in_ptr == '#')
				break;
			
			*out_ptr++ = *in_ptr++;
		}
		
		*out_ptr = 0;
		
		if (*output)
			pgSetFileExtension(output, file_extension, (short)extension_size, NULL);

		if (*in_ptr == '#') {
			
			out_ptr = output;
			output_size = pgCStrLength(out_ptr);
			out_ptr += output_size;
			
			while (*in_ptr)
				*out_ptr++ = *in_ptr++;
			
			*out_ptr = 0;
		}
	}
}


/* OutputSpecialChar sends a special char to the output stream. */

void PaigeHTMLExportFilter::OutputSpecialChar (pg_char the_char)
{
	pg_char		char_str[32];
	
	char_str[0] = '&';
	char_str[1] = 0;
	
	if (the_char == 0x22) {
	
		pgBlockMove("quot", &char_str[1], 4);
		char_str[5] = ';';
		char_str[6] = 0;
	}
	else
		find_data_char(data_chars, data_char_values, the_char, &char_str[1]);
	
	if (!char_str[1])
		find_data_char(data_chars2, data_char_values2, the_char, &char_str[1]);
		
	if (!char_str[1]) {
		pg_globals_ptr		globals;
		long				char_value;
		short				decimal_index;

		globals = export_pg_rec->globals;
		
		if (the_char == globals->left_single_quote[1] || (the_char == globals->right_single_quote[1]))
			char_str[1] = 0x27;
		else
		if (the_char == globals->left_double_quote[1] || (the_char == globals->right_double_quote[1]))
			char_str[1] = 0x22;
		
		if (char_str[1]) {
			
			char_str[0] = char_str[1];
			char_str[1] = 0;
		}
		else {
		
			char_str[1] = '#';
			decimal_index = 0;
			char_value = (long)the_char;
			char_value &= 0x00FF;
			
			decimal_value(&char_value, 100, &char_str[2], &decimal_index);
			decimal_value(&char_value, 10, &char_str[2], &decimal_index);
			char_str[decimal_index + 2] = (pg_char) (char_value | 0x30);
			++decimal_index;
			char_str[decimal_index + 2] = ';';
			++decimal_index;
			char_str[decimal_index + 2] = 0;
			
			if (the_char > 0x7F)
				InsertCharacterError(the_char);
		}
	}

	pgWriteString(char_str, 0, 0);
}


/* OutputImage sends an image anchor. */

void PaigeHTMLExportFilter::OutputImage (memory_ref embed, embed_callback callback,
			pg_char_ptr alt_text, long alt_text_size)
{
	pg_embed_ptr		embed_ptr;
	pg_url_image		image;
	
	embed_ptr = (pg_embed_ptr)UseMemory(embed);
	
	if ((embed_ptr->type & EMBED_TYPE_MASK) == embed_url_image)
		GetMemoryRecord((memory_ref)embed_ptr->data, 0, &image);
	else {
		
		pgFillBlock(&image, sizeof(pg_url_image), 0);
		image.image_data = (generic_var)embed_ptr->data;
		image.source_width = (short)embed_ptr->width;
		image.source_height = (short)embed_ptr->height;
		image.type_and_flags = embed_ptr->type;
	}
	
	if (!image.alt_string[0]) {
		
		if (alt_text_size > 63)
			alt_text_size = 63;
		
		if (alt_text_size)
			pgBlockMove(alt_text, image.alt_string, alt_text_size);
	}
	
	callback(export_pg_rec, embed_ptr, embed_url_image, EMBED_SAVE_IMAGE, embed_ptr->user_refcon,
			(long)&image, image_ctr);
	
	if (image.URL[0]) {
		
		InsertParameter(image_url_attribute, data_attributes, image.URL, (short)pgCStrLength(image.URL));
		InsertDecimalParameter(image_width_attribute, data_attributes, embed_ptr->width);
		InsertDecimalParameter(image_height_attribute, data_attributes, embed_ptr->height);
		
		if (image.alt_string[0])
			InsertParameter(alt_image_attribute, data_attributes, image.alt_string, (short)pgCStrLength(image.alt_string));

		WriteTag(data_commands, image_command, TRUE, FALSE);
		
		++image_ctr;
	}
	
	UnuseMemory(embed);
}


/* OutputStyleTags sends appropriate tags for a difference (now) in styles. */

void PaigeHTMLExportFilter::OutputStyleTags (pg_char_ptr text, long remaining_chars)
{
	style_info		changes, previous_style;
	pg_boolean		font_changed, point_changed, color_changed;
	
	if (translator.format.embed_object)
		return;
	
	if (*text == 0x0D || *text == 0x0A) {
		
		if (current_style.html_style != translator.format.html_style)
			if (translator.format.html_style == pre_tag_stylesheet) {
				
				if (!pretag_enable)
					WriteTag(html_style_commands, asis_command, TRUE, FALSE);
				
				pretag_enable = TRUE;
				current_style.html_style = pre_tag_stylesheet;
		}
		
		return;
	}

	if (!pgEqualStruct(&current_style, &translator.format, SIGNIFICANT_STYLE_SIZE))
		PreProcessStyleChange(&current_style, &translator.format, export_range.begin);

	if (current_style.html_style != translator.format.html_style) {
		
		EndFontTag();

		switch (translator.format.html_style) {

			case pre_tag_stylesheet:
				if (!pretag_enable)
					WriteTag(html_style_commands, asis_command, TRUE, FALSE);
				pretag_enable = TRUE;
				break;

			case teletype_stylesheet:
				WriteTag(html_style_commands, teletype_style_command, TRUE, FALSE);
				break;

			case blockquote_stylesheet:
				WriteTag(html_style_commands, blockquote_style_command, TRUE, FALSE);
				break;

			case address_stylesheet:
				WriteTag(html_style_commands, address_style_command, TRUE, FALSE);
				break;

			case cite_stylesheet:
				WriteTag(html_style_commands, cite_style_command, TRUE, FALSE);
				break;

			case code_stylesheet:
				WriteTag(html_style_commands, code_style_command, TRUE, FALSE);
				break;

			case code_def_stylesheet:
				WriteTag(html_style_commands, code_def_style_command, TRUE, FALSE);
				break;

			case code_var_stylesheet:
				WriteTag(html_style_commands, code_var_style_command, TRUE, FALSE);
				break;

			case heading1_stylesheet:
				WriteTag(html_style_commands, heading1_command, TRUE, FALSE);
				break;

			case heading2_stylesheet:
				WriteTag(html_style_commands, heading2_command, TRUE, FALSE);
				break;

			case heading3_stylesheet:
				WriteTag(html_style_commands, heading3_command, TRUE, FALSE);
				break;

			case heading4_stylesheet:
				WriteTag(html_style_commands, heading4_command, TRUE, FALSE);
				break;

			case heading5_stylesheet:
				WriteTag(html_style_commands, heading5_command, TRUE, FALSE);
				break;

			case heading6_stylesheet:
				WriteTag(html_style_commands, heading6_command, TRUE, FALSE);
				break;
		}
	}

	StyleExpected(&changes, &previous_style, FALSE);
	
	color_changed = (pg_boolean)(changes.fg_color.red == 0 || changes.fg_color.green == 0 || changes.fg_color.blue == 0);
	point_changed = (pg_boolean)(changes.point == 0);
	font_changed = (pg_boolean)(changes.font_index == 0);
	
	if (translator.format.font_index == default_style.font_index)
		font_changed = FALSE;
	
	if (font_changed || point_changed || color_changed) {
		
		if (font_changed) {
			long			fontsize, alt_size;
			pg_char			font_and_alt[130];
			
			fontsize = pgCStrLength(&translator.font.name[1]);
			pgBlockMove(&translator.font.name[1], font_and_alt, fontsize);
			
			if ((alt_size = MapExportFont(&translator.font, &font_and_alt[fontsize + 2])) > 0) {
				
				font_and_alt[fontsize] = ',';
				font_and_alt[fontsize + 1] = ' ';
				fontsize += (alt_size + 2);
			}
			
			InsertParameter(fontface_attribute, style_attributes, font_and_alt, (short)fontsize);
			InsertFontError(&translator.font.name[1], remaining_chars);
		}
		
		if (point_changed) {
			long		translated_point, converted_point;
			
			translated_point = GetPointLevel(&translator.format, &converted_point);
			InsertDecimalParameter(fontsize_attribute, style_attributes, translated_point);
			
			if (converted_point != translator.format.point)
				InsertPointsizeError(translator.format.point >> 16, remaining_chars);
		}

		if (color_changed)
			InsertColorParameter(fontcolor_attribute, style_attributes, &translator.format.fg_color, 0);

		WriteTag(html_style_commands, font_command, TRUE, FALSE);
		font_tag_enable = TRUE;
	}

	if (!changes.styles[bold_var])
		if (previous_style.styles[bold_var])		
			WriteTag(html_style_commands, bold_command, TRUE, FALSE);

	if (!changes.styles[italic_var])
		if (previous_style.styles[italic_var])
			WriteTag(html_style_commands, italic_command, TRUE, FALSE);

	if (!changes.styles[underline_var])
		if (previous_style.styles[underline_var])
			WriteTag(html_style_commands, underline_command, TRUE, FALSE);

	if (!changes.styles[subscript_var])
		if (previous_style.styles[subscript_var])
			WriteTag(html_style_commands, subscript_command, TRUE, FALSE);

	if (!changes.styles[superscript_var])
		if (previous_style.styles[superscript_var])
			WriteTag(html_style_commands, superscript_command, TRUE, FALSE);

	if (!changes.styles[strikeout_var])
		if (previous_style.styles[strikeout_var])
			WriteTag(html_style_commands, strikeout_command, TRUE, FALSE);

	if (!changes.styles[blink_var])
		if (previous_style.styles[blink_var])
			WriteTag(html_style_commands, blink_command, TRUE, FALSE);

	current_style = translator.format;
	styles_output = TRUE;
}



/* OutputStyleEndTags sends appropriate end tags for styles that have now cleared. */

void PaigeHTMLExportFilter::OutputStyleEndTags (void)
{
	style_info		changes, previous_style;
	pg_boolean		font_changed, point_changed, color_changed;

	StyleExpected(&changes, &previous_style, TRUE);

	color_changed = (pg_boolean)(changes.fg_color.red == 0 || changes.fg_color.green == 0 || changes.fg_color.blue == 0);
	point_changed = (pg_boolean)(changes.point == 0);
	font_changed = (pg_boolean)(changes.font_index == 0);

	if (font_changed || point_changed || color_changed)
		EndFontTag();

	if (!changes.styles[bold_var])
		if (!previous_style.styles[bold_var])
			WriteTag(html_style_commands, -bold_command, TRUE, FALSE);

	if (!changes.styles[italic_var])
		if (!previous_style.styles[italic_var])
			WriteTag(html_style_commands, -italic_command, TRUE, FALSE);

	if (!changes.styles[underline_var])
		if (!previous_style.styles[underline_var])
			WriteTag(html_style_commands, -underline_command, TRUE, FALSE);

	if (!changes.styles[subscript_var])
		if (!previous_style.styles[subscript_var])
			WriteTag(html_style_commands, -subscript_command, TRUE, FALSE);

	if (!changes.styles[superscript_var])
		if (!previous_style.styles[superscript_var])		
			WriteTag(html_style_commands, -superscript_command, TRUE, FALSE);

	if (!changes.styles[strikeout_var])
		if (!previous_style.styles[strikeout_var])
			WriteTag(html_style_commands, -strikeout_command, TRUE, FALSE);

	if (!changes.styles[blink_var])
		if (!previous_style.styles[blink_var])
			WriteTag(html_style_commands, -blink_command, TRUE, FALSE);

	if (current_style.html_style != previous_style.html_style) {
		
		if (real_text_out) {	// Turn off the previous tag
			
			EndFontTag();

			switch (current_style.html_style) {

				case body_stylesheet:
					break;
					
				case pre_tag_stylesheet:
					if (pretag_enable)
						WriteTag(html_style_commands, -asis_command, TRUE, FALSE);
					pretag_enable = FALSE;
					break;

				case teletype_stylesheet:
					WriteTag(html_style_commands, -teletype_style_command, TRUE, FALSE);
					break;

				case blockquote_stylesheet:
					WriteTag(html_style_commands, -blockquote_style_command, TRUE, FALSE);
					break;

				case address_stylesheet:
					WriteTag(html_style_commands, -address_style_command, TRUE, FALSE);
					break;

				case cite_stylesheet:
					WriteTag(html_style_commands, -cite_style_command, TRUE, FALSE);
					break;

				case code_stylesheet:
					WriteTag(html_style_commands, -code_style_command, TRUE, FALSE);
					break;

				case code_def_stylesheet:
					WriteTag(html_style_commands, -code_def_style_command, TRUE, FALSE);
					break;

				case code_var_stylesheet:
					WriteTag(html_style_commands, -code_var_style_command, TRUE, FALSE);
					break;

				case heading1_stylesheet:
					WriteTag(html_style_commands, -heading1_command, TRUE, FALSE);
					break;

				case heading2_stylesheet:
					WriteTag(html_style_commands, -heading2_command, TRUE, FALSE);
					break;

				case heading3_stylesheet:
					WriteTag(html_style_commands, -heading3_command, TRUE, FALSE);
					break;

				case heading4_stylesheet:
					WriteTag(html_style_commands, -heading4_command, TRUE, FALSE);
					break;

				case heading5_stylesheet:
					WriteTag(html_style_commands, -heading5_command, TRUE, FALSE);
					break;

				case heading6_stylesheet:
					WriteTag(html_style_commands, -heading6_command, TRUE, FALSE);
					break;
			}
		}
	}
}


/* OutputParChanges sends appropriate paragraph tags. This only gets called after a CR
has been hit and we are not in <pre> tag. */

void PaigeHTMLExportFilter::OutputParChanges (void)
{
	long			old_indent, new_indent, old_code, new_code;

	if (translator.par_format_changed)
		PreProcessParChange(&current_par, &translator.par_format, export_range.begin);
	
	if (!doing_table && translator.par_format.table.table_columns)
		OutputTableTag();
	else {

		if ((translator.par_format.table.border_info & PG_BORDER_BOTTOM) == PG_BORDER_LINERULE)
			WriteTag(control_commands, linerule_command, TRUE, TRUE);

		if (ComputeListingCodes(&current_par, &translator.par_format, &old_indent, &new_indent, &old_code, &new_code)) {
			
			indent_level = new_indent;
			listing_code = new_code;
			
			if (new_code == html_definition_list)
				new_indent = (new_indent + 1) / 2;
			
			if (old_code == html_definition_list)
				old_indent = (old_indent + 1) / 2;
			
			listing_level = new_indent;
			
			while (new_indent < old_indent) {
				
				TerminateList((short)old_code);
				--old_indent;
			}

			while (new_indent > old_indent) {
			
				OutputCR();

				switch (new_code) {

					case html_ordered_list:
						WriteTag(control_commands, ordered_list_command, TRUE, FALSE);
						break;

					case html_unordered_list:
						WriteTag(control_commands, unordered_list_command, TRUE, FALSE);
						break;

					case html_definition_list:
						WriteTag(control_commands, deflist_command, TRUE, FALSE);
						break;

					case html_directory:
						WriteTag(control_commands, directory_command, TRUE, FALSE);
						break;

					case html_menu:
						WriteTag(control_commands, menu_command, TRUE, FALSE);
						break;
				}
				
				++old_indent;
			}
		}
		
		if (current_par.justification != translator.par_format.justification) {
			
			if (real_text_out) {
				
				if (current_par.justification == justify_center || current_par.justification ==justify_right)
					WriteTag(control_commands, -parbreak_command, TRUE, TRUE);
			}

			if (translator.par_format.justification == justify_center || translator.par_format.justification ==justify_right) {
				
				InsertAlignmentParam((short)translator.par_format.justification);
				WriteTag(control_commands, parbreak_command, TRUE, TRUE);
			}
		}
	}

	current_par = translator.par_format;
	new_par = FALSE;
}

/* OutputTableTag sends the table tag + its params. */

void PaigeHTMLExportFilter::OutputTableTag (void)
{
	long		borders = translator.par_format.table.border_info;
	pg_short_t			index;

	for (index = 0; index < translator.par_format.num_tabs; ++index)
		borders |= translator.par_format.tabs[index].leader;

	if (!borders)
		InsertDecimalParameter(border_attribute, data_attributes, 0);
	else {
		long		border_info;

		if (!(border_info = (borders & 0x00070000) >> 16))
			if (!(border_info = (borders & 0x07000000) >> 24))
				if (!(border_info = borders & 0x00000007))
					border_info = (borders & 0x00000700) >> 8;
		
		if (border_info > 2)
			border_info -= 2;

		InsertDecimalParameter(border_attribute, data_attributes, border_info);
	}
	
	if (translator.par_format.justification)
		InsertAlignmentParam((short)translator.par_format.justification);

	WriteTag(table_data_commands, table_begin_command, TRUE, TRUE);

	doing_table = TRUE;
	column_ctr = row_char_count = 0;
	
	WriteTag(table_data_commands, table_row_command, TRUE, FALSE);
	OutputTableCell();
}


/* OutputTableCell sends the appropriate tags for the current table. */

void PaigeHTMLExportFilter::OutputTableCell (void)
{
	long		alignment = translator.par_format.tabs[column_ctr].tab_type & 0x00FF;
	long		column_span = translator.par_format.tabs[column_ctr].tab_type >> 24;
	long		shading = translator.par_format.tabs[column_ctr].ref_con;
	long		cell_width = translator.par_format.tabs[column_ctr].position & 0x0000FFFF;

	if (alignment == center_tab || alignment == right_tab)
		InsertAlignmentParam((short)(alignment - 1));
	if (column_span > 1)
		InsertDecimalParameter(column_span_attribute, data_attributes, column_span);
	if (shading)
		InsertColorParameter(bk_table_attribute, data_attributes, NULL, shading);
	if (cell_width)
		InsertDecimalParameter(image_width_attribute, data_attributes, cell_width);
	
	if (translator.format.styles[bold_var])
		cell_attribute = table_hcell_command;
	else
		cell_attribute = table_dcell_command;

	WriteTag(table_data_commands, cell_attribute, TRUE, FALSE);
}


/* ComputeListingCodes returns the params for listing info tag(s) .*/

pg_boolean PaigeHTMLExportFilter::ComputeListingCodes (par_info_ptr old_format,
				par_info_ptr new_format,
				long PG_FAR *old_indent, long PG_FAR *new_indent,
				long PG_FAR *old_code, long PG_FAR *new_code)
{
	*old_indent = old_format->indents.left_indent / list_indent_value;
	*new_indent = new_format->indents.left_indent / list_indent_value;
	*old_code = old_format->html_style & 0x0000FFFF;
	*new_code = new_format->html_style & 0x0000FFFF;

	if (*new_code == 0 && *new_indent != 0)
		*new_code = html_definition_list;
	if (*old_code == 0 && *old_indent != 0)
		*old_code = html_definition_list;
	
	if (*old_code == html_definition_list)
		*old_indent += 1;
	if (*new_code == html_definition_list)
		*new_indent += 1;

	return	(pg_boolean)(*new_indent != *old_indent);
}

/* WriteListingTag outputs the appropriate tag for a line with listing. */

void PaigeHTMLExportFilter::WriteListingTag (void)
{
	OutputCR();

	if (listing_code == html_definition_list) {
		
		if (indent_level < 2)
			WriteTag(html_style_commands, deflist_item_command, TRUE, FALSE);
		else
			WriteTag(html_style_commands, deflist_indented_command, TRUE, FALSE);
	}
	else
	if (listing_level && (translator.par_format.class_info & BULLETED_LINE))
		WriteTag(control_commands, listing_command, TRUE, FALSE);
}


/* GetPointLevel returns the proper point level. */

long PaigeHTMLExportFilter::GetPointLevel (style_info_ptr style, long PG_FAR *converted_point)
{
	long			index, best_index, best_point, point_test;
	
	best_point = 0x00FF0000;
	best_index = 0;
	
	for (index = 0; index < 7; ++index) {
		
		if ((point_test = point_levels[index] - style->point) < 0)
			point_test = -point_test;
		
		if (point_test < best_point) {
			
			best_point = point_test;
			best_index = index;
		}
	}

	if (converted_point)
		*converted_point = point_levels[best_index];

	return	best_index + 1;
}


/* EndFontTag ends the font tag if it has been turned on. */

void PaigeHTMLExportFilter::EndFontTag (void)
{
	if (font_tag_enable) {
	
		WriteTag(html_style_commands, -font_command, TRUE, FALSE);
		font_tag_enable = FALSE;
	}
}


/* TerminateList sends an end-tag for listing(s). */

void PaigeHTMLExportFilter::TerminateList (short code)
{
	OutputCR();

	switch (code) {
			
		case html_ordered_list:
			WriteTag(control_commands, -ordered_list_command, TRUE, FALSE);
			break;

		case html_unordered_list:
			WriteTag(control_commands, -unordered_list_command, TRUE, FALSE);
			break;

		case html_directory:
			WriteTag(control_commands, -directory_command, TRUE, FALSE);
			break;
		
		case html_definition_list:
			WriteTag(control_commands, -deflist_command, TRUE, FALSE);
			break;

		case html_menu:
			WriteTag(control_commands, -menu_command, TRUE, FALSE);
			break;
	}
}


/* InsertAlignmentParam sets up an alignment parameter. */

void PaigeHTMLExportFilter::InsertAlignmentParam (short alignment)
{
	if (alignment == justify_left)
		InsertParameter(align_attribute, control_attributes, (pg_char_ptr)"left", 4);
	else
	if (alignment == justify_center)
		InsertParameter(align_attribute, control_attributes, (pg_char_ptr)"center", 6);
	else
	if (alignment == justify_right)
		InsertParameter(align_attribute, control_attributes, (pg_char_ptr)"right", 5);
}

/* WriteCommand sends an RTF keyword to the file. The keyword token is the table_entry-th
element of rtf_table.  If parameter is non-NULL the numeric parameter is appended.
If delimeter TRUE, a space char is output, otherwise no delimeter is given. */

void PaigeHTMLExportFilter::WriteTag (pg_char_ptr tag_table,
			short table_entry, pg_boolean terminate_tag,
			pg_boolean send_CR)
{
	out_tag_ptr		params;
	long			num_params;
	short			table_item;

	pgWriteByte((pg_char)'<');
	
	if ((table_item = table_entry) < 0) {
		
		pgWriteByte((pg_char)0x2F);
		table_item = -table_item;
	}
	
	OutputParam(tag_table, table_item, FALSE);
	
	num_params = GetMemorySize(params_ref);
	params = (out_tag_ptr)UseMemory(params_ref);

	while (num_params) {

		OutputParam(params->table, params->code & 0x3FFF, TRUE);
		
		if (params->param_size) {
			short		param_index;
			
			pgWriteChar('=');
			
			if (!(params->code & HTML_PARAM_NUMERIC))
				pgWriteChar(0x22);
			
			for (param_index = 0; param_index < params->param_size; ++param_index)
				pgWriteChar(params->param[param_index]);
			
			if (!(params->code & HTML_PARAM_NUMERIC))
				pgWriteChar(0x22);
		}
		
		++params;
		--num_params;
	}
	
	UnuseMemory(params_ref);
	SetMemorySize(params_ref, 0);

	if (terminate_tag)
		pgWriteByte('>');
	
	if (send_CR)
		OutputCR();
}


/* WriteBreak sends a <BR> tag */

void PaigeHTMLExportFilter::WriteBreak (pg_boolean paragraph)
{
	short		code;
	
	if (current_style.html_style == pre_tag_stylesheet && !doing_table)
		OutputCR();
	else {
	
		if (paragraph)
			code = parbreak_command;
		else
			code = linebreak_command;
		
		WriteTag(control_commands, code, TRUE, TRUE);
	}
}

/* OutputCR sends a CR (+ LF for Windows) */

void PaigeHTMLExportFilter::OutputCR (void)
{
	pgWriteChar((pg_char)0x0D);
#ifdef WINDOWS_PLATFORM
	pgWriteChar((pg_char)0x0A);
#endif
}


/* InsertParameter adds a parameter to the param list. */

void PaigeHTMLExportFilter::InsertParameter(short code, pg_char_ptr table_used,
			pg_char_ptr param, short param_size)
{
	out_tag_ptr		params;
	short			end_index;
	
	end_index = param_size;
	
	while (end_index) {
		
		--end_index;
		
		if (param[end_index] > ' ')
			break;
		
		--param_size;
	}
	
	params = (out_tag_ptr)AppendMemory(params_ref, 1, TRUE);
	params->code = code;
	params->table = table_used;
	
	if ((params->param_size = param_size) > 0) {
		
		if (params->param_size > 256)
			params->param_size = 256;
		
		pgBlockMove(param, params->param, params->param_size);
	}
	
	UnuseMemory(params_ref);
}


/* InsertColorParameter is identical to InsertParameter() except it outputs a hex color. */

void PaigeHTMLExportFilter::InsertColorParameter(short code, pg_char_ptr table_used,
			color_value_ptr color, long long_color)
{
	long		the_color;
	pg_char		color_str[8];
	
	if (color)
		the_color = translate_color(color, NULL, NULL, NULL);
	else
		the_color = long_color;

	color_str[0] = '#';
	make_hex_char(&color_str[1], the_color >> 16);
	make_hex_char(&color_str[3], the_color >> 8);
	make_hex_char(&color_str[5], the_color);
	
	InsertParameter(code, table_used, color_str, 7);
}

/* InsertDecimalParameter inserts a decimal param with no quotes. */

void PaigeHTMLExportFilter::InsertDecimalParameter(short code, pg_char_ptr table_used, long value)
{
	pg_char		num_str[16];
	short		index;
	
	index = 0;
	
	decimal_value(&value, 1000000000, num_str, &index);
	decimal_value(&value, 100000000, num_str, &index);
	decimal_value(&value, 10000000, num_str, &index);
	decimal_value(&value, 1000000, num_str, &index);
	decimal_value(&value, 100000, num_str, &index);
	decimal_value(&value, 10000, num_str, &index);
	decimal_value(&value, 1000, num_str, &index);
	decimal_value(&value, 100, num_str, &index);
	decimal_value(&value, 10, num_str, &index);
	num_str[index] = (pg_char)(value | 0x30);
	++index;

	InsertParameter(code | HTML_PARAM_NUMERIC, table_used, num_str, index);
}


/* OutputParam sends a table entry to the output. */

void PaigeHTMLExportFilter::OutputParam (pg_char_ptr table_ptr, short table_entry, pg_boolean leading_space)
{
	long		index;
	short		entry_index;
	pg_char		the_char;
	
	index = 0;
	entry_index = 1;
	
	while (entry_index < table_entry) {
		
		for (;;) {
			
			the_char = table_ptr[index];
			++index;
			
			if (the_char == ' ')
				break;
		}
		
		++entry_index;
	}
	
	if (leading_space)
		pgWriteChar(' ');

	while (table_ptr[index] != ' ') {
		
		pgWriteChar(table_ptr[index]);
		++index;
	}
}

/* StyleExpected sets all the differences between current_style and translator.format.
However, if translator.format (the new style for the current block) has an HTML stylesheet
then we clear the change flags for items that will be set inherently. For example if
new style is "Heading 1" then we clear pointsize change if it matches Heading 1.
The previous_style param returns with the effective previous style (the one in translator
rec -- which might be modified to perform specific logic).  */

void PaigeHTMLExportFilter::StyleExpected (style_info_ptr changes, style_info_ptr previous_style,
			pg_boolean for_endtags)
{
	style_info		style;
	pg_boolean		is_pretag;
	short			index;

	pgFillBlock(changes, SIGNIFICANT_STYLE_SIZE, -1);
	*previous_style = translator.format;

	if (for_endtags) {
		
		if (current_style.html_style != previous_style->html_style)
			if (real_text_out)
				if (current_style.html_style != body_stylesheet)
					if (current_style.html_style)
						pgFillBlock(previous_style->styles, MAX_STYLES, 0);
	}
	else {

		if (current_style.html_style != previous_style->html_style)
			if (previous_style->html_style != body_stylesheet)
				if (previous_style->html_style)
					pgFillBlock(&current_style.styles, MAX_STYLES, 0);
	}

	pgSetMaskFromFlds(&current_style, previous_style, changes, style_compare, FALSE, FALSE);
	
	if (!for_endtags) {

		is_pretag = IsPretagStyle(previous_style);
		
		if (is_pretag || previous_style->named_style_index && previous_style->html_style) {

			if (is_pretag && !previous_style->named_style_index) {
				
				style = default_style;
				style.font_index = pretag_font;
				style.point = pretag_pointsize;
				style.point <<= 16;
			}
			else
				pgGetNamedStyleInfo(export_pg, previous_style->named_style_index, &style, NULL, NULL);
			
			if (previous_style->point == style.point)
				changes->point = -1;
			if (previous_style->font_index == style.font_index)
				changes->font_index = -1;
			if (equal_color(&previous_style->fg_color, &style.fg_color))
				changes->fg_color.red = changes->fg_color.green = changes->fg_color.blue = 0xFFFF;
			
			for (index = 0; index < MAX_STYLES; ++index)
				if (current_style.styles[index] == style.styles[index])
					changes->styles[index] = -1;
		}
		else {
			long		this_html, previous_html;
			
			if (is_pretag)
				this_html = pre_tag_stylesheet;
			else
				this_html = previous_style->html_style;
			
			if (IsPretagStyle(&current_style))
				previous_html = pre_tag_stylesheet;
			else
				previous_html = current_style.html_style;
			
			if (this_html == 0 && previous_html != 0) {
				
				if (previous_style->point != 0x000C0000)
					changes->point = 0;

				for (index = 0; index < MAX_STYLES; ++index)
					if (previous_style->styles[index])
						changes->styles[index] = 0;
			}
		}
	}
}


/* InsertFontError inserts a font warning (if we have not warned about this font yet). */

void PaigeHTMLExportFilter::InsertFontError (pg_char_ptr fontface, long size_of_run)
{
	pg_errlog_ptr			errors;
	long					num_errors;
	
	num_errors = GetMemorySize(error_log_ref);
	errors = (pg_errlog_ptr)UseMemory(error_log_ref);
	
	while (num_errors) {
		
		if (errors->code == UNSUPPORTED_FONT)
			if (equal_string(errors->message, fontface))
				break;

		++errors;
		--num_errors;
	}
	
	UnuseMemory(error_log_ref);
	
	if (!num_errors)
		InsertErrorLog(UNSUPPORTED_FONT, total_chars_in, total_chars_in + size_of_run, fontface, FALSE);
}


/* InsertFontError inserts a font warning (if we have not warned about this font yet). */

void PaigeHTMLExportFilter::InsertPointsizeError (long point, long size_of_run)
{
	pg_char					num_str[6];
	pg_errlog_ptr			errors;
	long					num_errors, value;
	short					index;

	num_errors = GetMemorySize(error_log_ref);
	errors = (pg_errlog_ptr)UseMemory(error_log_ref);
	
	index = 0;
	value = point;
	decimal_value(&value, 10000, num_str, &index);
	decimal_value(&value, 1000, num_str, &index);
	decimal_value(&value, 100, num_str, &index);
	decimal_value(&value, 10, num_str, &index);
	num_str[index] = (pg_char)(value | 0x30);
	++index;
	num_str[index] = 0;

	while (num_errors) {
		
		if (errors->code == POINT_MISMATCH_ERROR)
			if (equal_string(errors->message, num_str))
				break;

		++errors;
		--num_errors;
	}
	
	UnuseMemory(error_log_ref);
	
	if (!num_errors)
		InsertErrorLog(POINT_MISMATCH_ERROR, total_chars_in, total_chars_in + size_of_run, num_str, FALSE);
}


/* InsertCharacterError inserts an error regarding hard-coded chars > 7F. */

void PaigeHTMLExportFilter::InsertCharacterError (pg_char the_char)
{
	pg_errlog_ptr			errors;
	long					num_errors;
	
	num_errors = GetMemorySize(error_log_ref);
	errors = (pg_errlog_ptr)UseMemory(error_log_ref);
	
	while (num_errors) {
		
		if (errors->code == HARD_CODED_CHAR_WARNING)
			if (errors->message[0] == the_char)
				break;
		
		++errors;
		--num_errors;
	}
	
	UnuseMemory(error_log_ref);
	
	if (!num_errors) {
		pg_char		str[2];
		
		str[0] = the_char;
		str[1] = 0;
		InsertErrorLog(HARD_CODED_CHAR_WARNING, total_chars_in, total_chars_in + 1, str, FALSE);
	}
}


/**************************************** Local Functions **************************************/


/* find_font_index returns the font index, if any, for fontname. If none exist then -1 is returned. */

static short find_font_index (paige_rec_ptr pg, pg_char_ptr fontname)
{
	font_info_ptr			fonts;
	short					num_fonts, index;
	
	num_fonts = (short)GetMemorySize(pg->fonts);
	fonts = (font_info_ptr)UseMemory(pg->fonts);
	
	for (index = 0; index < num_fonts; ++index, ++fonts)
		if (equal_string(fonts->name, fontname))
			break;
	
	UnuseMemory(pg->fonts);
	
	if (index == num_fonts)
		return	-1;
	
	return	index;
}

/* is_fixed_font returns TRUE if the font is fixed spacing. */

static pg_boolean is_fixed_font (font_info_ptr font)
{
	if (equal_string(&font->name[1], (pg_char_ptr)"Courier\0"))
		return	TRUE;
	if (equal_string(&font->name[1], PRETAG_DEF_FONT))
		return	TRUE;
	if (equal_string(&font->name[1], CODETAG_DEF_FONT))
		return	TRUE;
	
	return	FALSE;
}

/* equal_string returns TRUE if the two strings match. */

static pg_boolean equal_string (pg_char_ptr str1, pg_char_ptr str2)
{
	register pg_char_ptr		input;
	register pg_char_ptr		output;
	
	input = str1;
	output = str2;
	
	for (;;) {
		
		if (*input != *output)
			break;
		
		if (*input++ == 0)
			return	TRUE;

		++output;
	}
	
	return	FALSE;	
}


/* translate_color converts a color value RGB to 0-255 shorts and returns a hex value
to send out. */

static long translate_color (color_value_ptr color, long PG_FAR *redcolor,
         long PG_FAR *greencolor, long PG_FAR *bluecolor)
{
   long     red, green, blue;

   red = color->red >> 8;
   green = color->green >> 8;
   blue = color->blue >> 8;
   
   if (redcolor)
  	 *redcolor = red;
   if (greencolor)
  	 *greencolor = green;
   if (bluecolor)
   	 *bluecolor = blue;
   
   return	(((red & 0xFF) << 16) | ((green & 0xFF) << 8) | (blue & 0xFF));
}



/* plain_styles returns TRUE if all styles are turned off. */

static pg_boolean plain_styles (style_info_ptr style)
{
   short    index;
   
   for (index = 0; index < MAX_STYLES; ++index)
      if (style->styles[index])
         return   FALSE;
   
   return   TRUE;
}

/* make_hex_char outputs a single hex char. */

static void make_hex_char (pg_char_ptr output, long value)
{
	pg_char		nibble;

	nibble = (pg_char)(value >> 4);
	nibble &= 0x0F;

	if (nibble > 9)
		nibble += 7;
	nibble += 0x30;
	output[0] = nibble;

	nibble = (pg_char)(value & 0x0F);
	
	if (nibble > 9)
		nibble += 7;
	nibble += 0x30;
	output[1] = nibble;
	
}

/* decimal_value outputs a digit, suppressing leading zeros. */

static void decimal_value (long PG_FAR *value, long minus_value, pg_char_ptr output,
			short PG_FAR *index)
{
	pg_char		digit;
	
	digit = 0;
	
	while (minus_value <= *value) {
		
		*value -= minus_value;
		++digit;
	}
	
	if (digit > 0 || *index > 0) {
		
		output[*index] = digit | 0x30;
		*index += 1;
	}
}

/* special_char returns true if the character is a special char. */

static pg_boolean special_char (pg_char the_char)
{
	return	(pg_boolean) (the_char == '<'
			|| the_char == '>'
			|| the_char == 0x2F
			|| the_char == '&'
			|| the_char == '#'
			|| the_char > 0x7F
			|| the_char == 0x22
			|| the_char == ':');
}

/* find_data_char scans a table looking for a character and, if found, fills in the name. */

static void find_data_char (pg_char_ptr chars, pg_char_ptr table, pg_char the_char, pg_char_ptr output)
{
	short			index;

	index = 0;
	
	while (chars[index]) {
		
		if (chars[index] == the_char)
			break;
		
		++index;
	}
	
	if (chars[index]) {
		
		while (index) {
			
			while (*table != ' ')
				++table;
			
			++table;
			--index;
		}
		
		index = 0;
		
		while (*table != ' ') {
			
			output[index] = *table++;
			++index;
		}

		output[index] = ';';
		++index;
		output[index] = 0;
	}
}

/* equal_color returns TRUE if the two colors match. */

static pg_boolean equal_color (color_value_ptr color1, color_value_ptr color2)
{
	return	(pg_boolean)(color1->red == color2->red && color1->green == color2->green && color1->blue == color2->blue);
}

/* has_colons returns TRUE if there are colons in the URL. */

static pg_boolean has_colons (pg_char_ptr URL)
{
	short		index;
	
	index = 0;
	
	while (URL[index]) {
		
		if (URL[index] == ':')
			return	TRUE;
		
		++index;
	}
	
	return	FALSE;
}

