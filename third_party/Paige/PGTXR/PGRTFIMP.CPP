/* Import-Export module. Copyright 1995-96 by DataPak Software, Inc.  This software is a
part of the total PAIGE library.

This source file contains all the member functions for the RTF import class (derived
from the PaigeImportFilter class. */


#include "Paige.h"
#include "pgTraps.h"
#include "pgUtils.h"
#include "pgTxrCPP.h"
#include "defprocs.h"
#include "pgErrors.h"
#include "pgDefStl.h"
#include "pgRTFDef.h"
#include "pgEmbed.h"
#include "pgdeftbl.h"
#include "machine.h"
#include "pgTables.h"
#include "pgHText.h"

static short rtf_command (pg_char_ptr table, pg_char_ptr argument, pg_short_t arg_size,
		short PG_FAR *param, pg_boolean PG_FAR *had_parameter);
static pg_boolean regular_letter (pg_char the_char);
static pg_boolean is_hex_char (pg_char the_char);
static long string_to_num (pg_char_ptr string, short string_size);
static pg_short_t translate_hex (pg_char hex_char);
static font_info_ptr locate_font_in_table (memory_ref table, short family_id);
static style_table_ptr locate_stylesheet (memory_ref table, short style_id);
static short convert_twips (paige_rec_ptr pg, short twipsvalue);


/* PaigeImportFilter constructor. This sets up the various members common to all filters.
No error checking or file verification occurs just yet. */


PaigeRTFImportFilter::PaigeRTFImportFilter ()
{
	file_type = pg_rtf_type;	// Default type for this class
	feature_bits = IMPORT_TEXT_FEATURE | IMPORT_TEXT_FORMATS_FEATURE
					| IMPORT_PAR_FORMATS_FEATURE | IMPORT_PAGE_INFO_FEATURE
					| IMPORT_EMBEDDED_OBJECTS_FEATURE
					| IMPORT_HYPERTEXT_FEATURE
					| IMPORT_SERIAL_SETUP;
}


/* PaigeImportFilter destructor. This removes common items. */

PaigeRTFImportFilter::~PaigeRTFImportFilter ()
{
 
}


/* pgPrepareImport sets up the necessary extra buffers to begin importing. */

pg_error PaigeRTFImportFilter::pgPrepareImport (void)
{
	rectangle		print_rect;
	long			topbottom_margin, leftright_margin;
	
	if (import_pg_rec->version < MINIMUM_RTF_VERSION)
		return		BAD_PAIGE_VERSION_ERR;

	style_stack = ::MemoryAlloc(paige_globals->mem_globals, sizeof(format_stack), 0, 2);
	font_table = ::MemoryAlloc(paige_globals->mem_globals, sizeof(font_info), 0, 4);
	color_table = ::MemoryAlloc(paige_globals->mem_globals, sizeof(color_value), 0, 4);
	index_text_ref = ::MemoryAlloc(paige_globals->mem_globals, sizeof(pg_char), 0, 32);

	current_style = translator.format;
	current_font = translator.font;
	current_par = par_table = translator.par_format;
	pgFillBlock(&stylesheet_par_mask, sizeof(par_info), 0);
	double_borders = FALSE;

// Establish the default font:

#ifdef MAC_PLATFORM
	default_font = current_font.family_id;
#else
	default_font = current_style.font_index;
#endif

	def_font_set = default_font;

	picture_ref = MEM_NULL;
	previous_embed = MEM_NULL;
	current_stylesheet = NULL;
	representing = had_actual_param = FALSE;

	pgGetDocInfo(import_pg, &translator.doc_info);
	pgAreaBounds(import_pg, &print_rect, NULL);
	print_rect.top_left.h -= translator.doc_info.margins.top_left.h;
	print_rect.top_left.v -= translator.doc_info.margins.top_left.v;
	print_rect.bot_right.h += translator.doc_info.margins.bot_right.h;
	print_rect.bot_right.v += translator.doc_info.margins.bot_right.v;
	pgOffsetRect(&print_rect, -print_rect.top_left.h, -print_rect.top_left.v);
	translator.doc_info.print_target = print_rect;

	topbottom_margin = pgConvertResolution(import_pg_rec, MSWORD_TOPBOTTOM_MARGIN);
	leftright_margin = pgConvertResolution(import_pg_rec, MSWORD_LEFTRIGHT_MARGIN);

	translator.doc_info_changed
			= (pg_boolean)(translator.doc_info.margins.top_left.v != topbottom_margin
			|| translator.doc_info.margins.bot_right.v != topbottom_margin
			|| translator.doc_info.margins.top_left.h != leftright_margin
			|| translator.doc_info.margins.bot_right.h != leftright_margin);
	
	translator.doc_info.margins.top_left.v = topbottom_margin;
	translator.doc_info.margins.bot_right.v = topbottom_margin;
	translator.doc_info.margins.top_left.h = leftright_margin;
	translator.doc_info.margins.bot_right.h = leftright_margin;
	
	index_level = toc_level = next_table_id = 0;
	table_entry = table_level = 0;
	last_border = 0;
	non_table_text = 1;

	return		NO_ERROR;
}


/* pgImportDone disposes all structs created for pgPrepareImport(). */

pg_error PaigeRTFImportFilter::pgImportDone (void)
{
	::DisposeNonNilMemory(style_stack);
	::DisposeNonNilMemory(font_table);
	::DisposeNonNilMemory(color_table);
	::DisposeNonNilMemory(index_text_ref);

	return		NO_ERROR;
}


/* pgVerifySignature returns NO_ERROR if this is an RTF file. */

pg_error PaigeRTFImportFilter::pgVerifySignature ()
{
	return	::pgVerifyRTF((pg_file_unit)filemap, io_proc, file_begin);
}


/* pgReadNextBlock is the major entry point that reads the next block of text, sets up paragraph
and text formats and returns something for the base class to insert into the pg_ref. */

pg_boolean PaigeRTFImportFilter::pgReadNextBlock (void)
{
	pg_char				next_char;
	pg_boolean			transfer_text = FALSE;
	pg_error			result = NO_ERROR;

	translator.bytes_transferred = 0;
	translator.flags = 0;

	if (current_style.embed_object) {	
	
// Zero-out embed_ref stuff from possible previous embedded insertion.

		current_style.procs = paige_globals->def_style.procs;
		current_style.embed_entry = current_style.embed_style_refcon
				= current_style.embed_refcon = current_style.embed_id = 0;
		current_style.embed_object = MEM_NULL;
		current_style.char_bytes = 0;
		current_style.class_bits = 0;
	}
	
	translator.format_changed = !pgEqualStruct(&translator.format, &current_style, SIGNIFICANT_STYLE_SIZE);
	translator.font_changed = !pgEqualStruct(&translator.font, &current_font, SIGNIFICANT_FONT_SIZE);

	translator.format = current_style;
	translator.font = current_font;

	if (picture_ref)
		if (ReadRTFPicture())
			return		TRUE;
	
	while ( (result = pgGetImportChar(&next_char)) == NO_ERROR ) {

		if (next_char == RTF_COMMAND_CHAR) {
			short			parameter, command, table_id;
			long			command_result;
			
			if ((command_result = ReadCommand(&parameter, &next_char)) == 0) {
				
				if (parameter)
					if (OutputCharacter((pg_char)parameter))
						break;
			}
			else
			if (command_result != DO_NOTHING_COMMAND) {
				
				table_id = pgHiWord(command_result);
				command = pgLoWord(command_result);
				
				switch (table_id) {

					case SPECIAL_CHAR_COMMAND:
						transfer_text = DoSpecialCharCommand(command, parameter);
						break;
						
					case GROUP_AVOID_COMMAND:
						SkipCurrentLevel();
						break;

					case DESTINATION_COMMAND:
						transfer_text = DoDestinationCommand(command, parameter);
						break;

					case DOCUMENT_COMMAND:
						transfer_text = DoDocumentCommand(command, parameter);
						break;

					case PARAGRAPH_COMMAND:
						transfer_text = DoParagraphCommand(command, parameter);
						break;

					case STYLE_COMMAND:
						transfer_text = DoStyleCommand(command, parameter);
						break;
					
					case TABLE_COMMAND:
						transfer_text = DoTableCommand(command, parameter);
						break;

					case INFO_COMMAND:
						ProcessInfoCommand(command, parameter);
						break;

				}
			}
		}
		else
		if (next_char == RTF_GROUPBEGIN_CHAR) {
		
				PushStyleStack();
				
				if (current_stylesheet)
					++stylesheet_level;
		}
		else
		if (next_char == RTF_GROUPEND_CHAR) {

			PopStyleStack();
			
			if (current_stylesheet) {
				
				if ((--stylesheet_level) == 0) {
					long			stylesheet_qty;
					
					stylesheet_qty = ::GetMemorySize(translator.stylesheet_table);
					
					if (!stylesheet.name[0] && stylesheet_qty)
						stylesheet_qty -= 1;

					::UnuseMemory(translator.stylesheet_table);
					::SetMemorySize(translator.stylesheet_table, stylesheet_qty);

					current_stylesheet = NULL;
				}
				else {
					
					current_style = import_pg_rec->globals->def_style;
					current_style.point = 0x000C0000;
				}
			}
			else
			if (!translator.bytes_transferred) {

				translator.format = current_style;
				translator.font = current_font;

			}
			else
			if (translator.font_changed || translator.format_changed || translator.hyperlink_target_changed)
				break;
		}
		else {
			// Default is regular text character.
			
			if (current_stylesheet) {
			
				if (next_char == TABLE_TERMINATOR)
					ProcessStylesheet();
				else
				if ((next_char >= ' ') && (stylesheet_index < (FONT_SIZE - 1))) {
					
					stylesheet.name[stylesheet_index] = next_char;
					++stylesheet_index;
				}
			}
			else
			if (next_char >= ' ') {
				
				if (index_level > 0 || toc_level > 0) {
					pg_char_ptr		index_append;
					
					index_append = (pg_char_ptr)AppendMemory(index_text_ref, 1, FALSE);
					*index_append = next_char;
					UnuseMemory(index_text_ref);

					if ((import_bits & IMPORT_HIDDENTEXT_FLAG) || current_style.styles[hidden_text_var] == 0)
						if (OutputCharacter(next_char))
							break;
				}
				else {
					
					if (!table_level && !table_entry)
						non_table_text += 1;
					
					if ((import_bits & IMPORT_HIDDENTEXT_FLAG) || current_style.styles[hidden_text_var] == 0)
						if (OutputCharacter(next_char))
							break;
				}
			}
		}

		if (transfer_text)
			break;
	}
	
	//if (table_level || table_entry)
	//	translator.par_format_changed = FALSE;
	//else
	if (!table_level && !table_entry)
		translator.par_format_changed |= !pgEqualStruct(&translator.par_format, &current_par, SIGNIFICANT_PAR_STYLE_SIZE);

	if (translator.par_format_changed) {
		
		if (current_par.table.table_columns) {
			
			if (current_par.indents.first_indent < current_par.indents.left_indent)
				current_par.indents.left_indent += current_par.indents.first_indent;
			
			current_par.indents.first_indent = 0;
			current_par.justification = 0;
		}

		translator.par_format = current_par;
	}
	
	bytes_read = (filepos - file_begin) - buffer_size + buffer_index;
	io_result = result;

	if (result == NO_ERROR || translator.bytes_transferred)
		return	TRUE;
	
	return	FALSE;
}


/* ReadCommand gets called after an RTF_COMMAND_CHAR byte has been read. The next byte to read
is known to be the first byte following RTF_COMMAND_CHAR. If this function returns -1, end-of-file
has been reached.  If this function returns zero, the command is not recognized in any table. Note,
however, that an unrecognized command could be a special character, in which case *param will
contain that char, otherwise *param will be set to zero.
If the command is recognized, the function result's highword is the table number and its lowword
is the enumeration for that table. The *terminator char will be the character the command terminated
with;  the next byte position in the file will be the first byte following the terminator UNLESS
the terminator is another RTF_COMMAND_CHAR, RTF_GROUPBEGIN or RTF_GROUPEND char. */

 
long PaigeRTFImportFilter::ReadCommand (short PG_FAR *param, pg_char_ptr terminator)
{
	pg_char			command_buffer[MAX_COMMAND_BYTES]; // RTF token keyword just read
	pg_char_ptr		command_buffer_ptr;
	pg_char			next_byte;
	pg_boolean		negative_param;
	long			result;
	short			byte_ctr, param_ctr, parameter;

	*terminator = 0;
	*param = 0;
	negative_param = FALSE;
	had_actual_param = FALSE;

	command_buffer_ptr = command_buffer;
	
	for (byte_ctr = 0; byte_ctr < (MAX_COMMAND_BYTES - 1); ++byte_ctr, ++command_buffer_ptr) {

		if (pgGetImportChar(command_buffer_ptr) != NO_ERROR)
			break;
		
	// Check for special characters:
	
		if (byte_ctr == 0) {
			
			if (*command_buffer_ptr == RTF_HEX_CHAR) {
				
				pgGetImportChar(&next_byte);
				*param = (pg_short_t)translate_hex(next_byte);
				*param <<= 4;
				pgGetImportChar(&next_byte);
				*param |= (pg_short_t)translate_hex(next_byte);
				
				return	0;
			}
			else
			if (*command_buffer_ptr == RTF_STAR_CHAR)
				return	((long)GROUP_AVOID_COMMAND << 16);
			else
			if (*command_buffer_ptr == RTF_COMMAND_CHAR
				|| *command_buffer_ptr == RTF_GROUPBEGIN_CHAR
				|| *command_buffer_ptr == RTF_GROUPEND_CHAR
				|| *command_buffer_ptr == RTF_COLON) {
				
				*param = *command_buffer_ptr;
				return	0;
			}
			else
			if (!regular_letter(*command_buffer_ptr))
				return	DO_NOTHING_COMMAND;
		}

		if ((*command_buffer_ptr < '0') && (*command_buffer_ptr != NEGATIVE_SIGN_CHAR))
			break;
	
	// Examine the NEXT byte to see if it is a special command character:
		
		if (buffer_index < buffer_size) {
		
			next_byte = io_buffer[buffer_index];
			
			if (next_byte == RTF_COMMAND_CHAR || next_byte == RTF_GROUPBEGIN_CHAR
					|| next_byte == RTF_GROUPEND_CHAR || next_byte == TABLE_TERMINATOR) {
				
				++command_buffer_ptr;
				*command_buffer_ptr = ' ';
				++byte_ctr;
				break;
			}
		}
	}

	*terminator = *command_buffer_ptr;

	if (!byte_ctr)
		return	0;

	if ((result = rtf_command(special_char_commands, command_buffer, byte_ctr, param, &had_actual_param)) != 0)
		return		(result | ((long)SPECIAL_CHAR_COMMAND << 16));

	if ((result = rtf_command(group_avoid_commands, command_buffer, byte_ctr, param, &had_actual_param)) != 0) {

		UnsupportedCommand(command_buffer, *param);
		return		(result | ((long)GROUP_AVOID_COMMAND << 16));
	}

	if ((result = rtf_command(destination_commands, command_buffer, byte_ctr, param, &had_actual_param)) != 0)
		return		(result | ((long)DESTINATION_COMMAND << 16));

	if ((result = rtf_command(document_commands, command_buffer, byte_ctr, param, &had_actual_param)) != 0)
		return		(result | ((long)DOCUMENT_COMMAND << 16));

	if ((result = rtf_command(paragraph_commands, command_buffer, byte_ctr, param, &had_actual_param)) != 0)
		return		(result | ((long)PARAGRAPH_COMMAND << 16));

	if ((result = rtf_command(style_commands, command_buffer, byte_ctr, param, &had_actual_param)) != 0)
		return		(result | ((long)STYLE_COMMAND << 16));
	
	if ((result = rtf_command(table_commands, command_buffer, byte_ctr, param, &had_actual_param)) != 0)
		return		(result | ((long)TABLE_COMMAND << 16));

	if ((result = rtf_command(info_commands, command_buffer, byte_ctr, param, &had_actual_param)) != 0)
		return		(result | ((long)INFO_COMMAND << 16));
	
	parameter = 0;

	for (param_ctr = 0; param_ctr < byte_ctr; ++param_ctr) {
		
		if (command_buffer[param_ctr] >= '0' && command_buffer[param_ctr] <= '9') {
			
			parameter = (short)string_to_num(&command_buffer[param_ctr], byte_ctr - param_ctr);
			had_actual_param = TRUE;
			break;
		}
	}
	
	command_buffer[param_ctr] = 0;

	UnsupportedCommand(command_buffer, parameter);

	*param = 0;
	return		0;
}


/* DoSpecialCharCommand handles special-character commands. If TRUE is returned, the
current block of text should be transferred. */

pg_boolean PaigeRTFImportFilter::DoSpecialCharCommand (short command, short parameter)
{
	pg_boolean			result = FALSE;
	pg_boolean			cross_platform;

#ifdef MAC_PLATFORM
#pragma unused (parameter)
#endif

	cross_platform = (pg_boolean)(file_os != CURRENT_OS);

	switch (command) {
	
		case bullet_command:
			if (cross_platform)
				result = OutputCharacter((pg_char)BULLET_CROSS_CHAR);
			else
	#ifdef GLOBALS_ARE_PSTRINGS
				result = OutputString(&paige_globals->bullet_char[1]);
	#else
				result = OutputString(paige_globals->bullet_char);
	#endif
			break;
		
		case left_doublequote_command:
			if (cross_platform)
				result = OutputCharacter((pg_char)LEFTDBLQUOTE_CROSS_CHAR);
			else
	#ifdef GLOBALS_ARE_PSTRINGS
				result = OutputString(&paige_globals->left_double_quote[1]);
	#else
				result = OutputString(paige_globals->left_double_quote);
	#endif
			break;

		case line_feed_command:
			OutputCharacter((pg_char)0x0A);

			break;
		
		case left_singlequote_command:
			if (cross_platform)
				result = OutputCharacter((pg_char)LEFTQUOTE_CROSS_CHAR);
			else
				result = OutputString(paige_globals->left_single_quote);
			break;
			
		case page_break_command:
			result = OutputCharacter((pg_char)paige_globals->ff_char);
			break;

		case cr_command:
			if (table_level || table_entry) {
				
				if (par_table.table.table_columns > 1)
					OutputCharacter((pg_char)0x0A);
			}
			else {
			
				OutputCharacter((pg_char)paige_globals->line_wrap_char);
				result = TRUE;
			}

			break;

		case right_doublequote_command:
			if (cross_platform)
				result = OutputCharacter((pg_char)RIGHTDBLQUOTE_CROSS_CHAR);
			else
	#ifdef GLOBALS_ARE_PSTRINGS
				result = OutputString(&paige_globals->right_double_quote[1]);
	#else
				result = OutputString(paige_globals->right_double_quote);
	#endif
			break;
			
		case right_singlequote_command:
			if (cross_platform)
				result = OutputCharacter((pg_char)RIGHTQUOTE_CROSS_CHAR);
			else
	#ifdef GLOBALS_ARE_PSTRINGS
				result = OutputString(&paige_globals->right_single_quote[1]);
	#else
				result = OutputString(paige_globals->right_single_quote);
	#endif
			break;

		case end_section_command:
			break;
			
		case tab_char_command:
			if (!table_level && !table_entry)
				result = OutputCharacter((pg_char)paige_globals->tab_char);

			break;
	}
	
	if (!table_level && !table_entry)
		non_table_text += 1;

	return		result;
}


/* DoDestinationCommand handles all the commands in the "destination type" table. */

pg_boolean PaigeRTFImportFilter::DoDestinationCommand (short command, short parameter)
{
	font_info			font;
	color_value			color;
	long				command_result, long_dimension;
	pg_char_ptr			font_name;
	pg_char				next_char;
	pg_boolean			result = FALSE;
	short				param, bracket_level, table_id, command_id;
	short				fontname_level, avoid_level;

	switch (command) {
	
		case non_mac_os_command:
			file_os = WINDOWS_OS;
			break;
		
		case binary_data_command:
			embed_binary = (long)parameter;
			break;
		
		case def_font_command:
			default_font = parameter;
			break;

		case color_table_command:
		case font_table_command:
			bracket_level = 1;
			fontname_level = 0;
			avoid_level = 0;

			pgFillBlock(&font, sizeof(font_info), 0);
			font_name = &font.name[1];
			pgFillBlock(&color, sizeof(color_value), 0);

			while (pgGetImportChar(&next_char) == NO_ERROR) {
				
				if (next_char == RTF_COMMAND_CHAR) {
					
					if ((command_result = ReadCommand(&param, &next_char)) != 0) {

						table_id = pgHiWord(command_result);
						
						if (table_id == GROUP_AVOID_COMMAND)
							avoid_level = bracket_level;
						else
						if (table_id == STYLE_COMMAND) {
						
							command_id = pgLoWord(command_result);
							
							if (command == font_table_command) {

								if (command_id == font_command)
									font.family_id = (short)param;
								else
								if (command_id == charset_command || command_id == font_charset_command)
									font.machine_var[PG_CHARSET] = param;
							}
							else
							if (command == color_table_command) {
								pg_short_t			colorfield;
								
								colorfield = (pg_short_t)param;
								colorfield <<= 8;

								switch (command_id) {
									
									case redcolor_command:
										color.red = colorfield;
										break;
										
									case greencolor_command:
										color.green = colorfield;
										break;

									case bluecolor_command:
										color.blue = colorfield;
										break;
								}
							}
						}
					}
				}
				else
				if (next_char == RTF_GROUPBEGIN_CHAR)
					++bracket_level;
				else
				if (next_char == RTF_GROUPEND_CHAR) {
					
					--bracket_level;
					
					if (bracket_level == 0)
						break;
					
					if (bracket_level < avoid_level)
						avoid_level = 0;
				}
				else
				if (next_char == TABLE_TERMINATOR) {
					font_info_ptr		font_to_add;
					color_value_ptr		color_to_add;

					if (command == font_table_command) {

						font_to_add = (font_info_ptr)AppendMemory(font_table, 1, FALSE);
						pgBlockMove(&font, font_to_add, sizeof(font_info));
						UnuseMemory(font_table);
						pgFillBlock(&font, sizeof(font_info), 0);
						font_name = &font.name[1];
					}
					else
					if (command == color_table_command) {
						
						color_to_add = (color_value_ptr)AppendMemory(color_table, 1, FALSE);
						pgBlockMove(&color, color_to_add, sizeof(color_value));
						UnuseMemory(color_table);
						pgFillBlock(&color, sizeof(color_value), 0);
					}
				}
				else
				if (command == font_table_command && !avoid_level) {
					
					if (fontname_level == 0)
						fontname_level = bracket_level;

					if (fontname_level == bracket_level && (font_name[0] < (FONT_SIZE - 2))) {
	
						*font_name++ = next_char;
						++font.name[0];
					}
				}
			}

			if (command == font_table_command && def_font_set != default_font) {
				font_info_ptr		first_font;

				DoStyleCommand(font_command, default_font);
				first_font = (font_info_ptr)UseMemory(import_pg_rec->fonts);
				pgBlockMove(&current_font, first_font, sizeof(font_info));
				this->pgMapFont(first_font, file_os, CURRENT_OS);
				import_pg_rec->procs.font_proc(import_pg_rec, first_font);

				UnuseMemory(import_pg_rec->fonts);
				def_font_set = default_font;
			}

			break;
		
		case style_basedon_command:
			if (current_stylesheet) {
				style_table_ptr		style_basedon;

				if ((style_basedon = locate_stylesheet(translator.stylesheet_table, parameter)) != NULL) {
					par_info		par;

					pgGetNamedStyleInfo(import_pg, style_basedon->index, NULL, NULL, &par);
					pgSetFldsFromMask(&current_par, &par, &stylesheet_par_mask, SIGNIFICANT_PAR_STYLE_SIZE);

					UnuseMemory(translator.stylesheet_table);
				}
			}
			break;

		case stylesheet_command:
		// Set up the variables so we know stylesheet(s) are being processed:

			current_stylesheet = (style_table_ptr) ::AppendMemory(translator.stylesheet_table, 1, TRUE);
			stylesheet_level = 1;
			stylesheet_index = 0;
			pgFillBlock(&stylesheet, sizeof(style_table_entry), 0);
			pgFillBlock(&stylesheet_par_mask, sizeof(par_info), -1);			
			stylesheet.par = current_par = paige_globals->def_par;
			stylesheet.style = current_style = paige_globals->def_style;
			stylesheet.style.point = 0x000C0000;		// Must force 12 point default
			GetMemoryRecord(import_pg_rec->fonts, 0, &current_font);
			stylesheet.has_styles = stylesheet.has_par_styles = stylesheet.has_font = TRUE;
			break;

		case mac_platform_command:
			file_os = MACINTOSH_OS;
			break;

		case mac_pict_command:
			embed_type = embed_mac_pict;
			break;

		case object_command:
		case pict_command:
			result = PrepareRTFPicture(command);
			break;

		case picframe_height_command:
			if (embed_type == embed_meta_file)
				twips_gheight = parameter;

			embed_height = convert_twips(import_pg_rec, parameter);
			break;

		case pict_height_command:
		case object_height_command:
			if (embed_type == embed_meta_file || embed_type == embed_unsupported_object) {
				
				metafile.y_ext = convert_twips(import_pg_rec, parameter);
				
				if (!embed_height)
					embed_height = metafile.y_ext;

				twips_height = parameter;
			}
			else
				embed_height = (short)parameter;
			
			break;

		case picframe_width_command:
			if (embed_type == embed_meta_file)
				twips_gwidth = parameter;
			
			embed_width = convert_twips(import_pg_rec, parameter);
			break;

		case pict_width_command:
		case object_width_command:
			if (embed_type == embed_meta_file || embed_type == embed_unsupported_object) {
				
				metafile.x_ext = convert_twips(import_pg_rec, parameter);
				twips_width = parameter;
				
				if (!embed_width)
					embed_width = metafile.x_ext;
			}
			else
				embed_width = (short)parameter;
			break;

		case pict_vscale_command:
			if (parameter) {
			    
			    metascale_y = parameter;
			    metascale_y <<= 16;
			    metascale_y |= 100;
				long_dimension = embed_height;
				pgScaleLong(metascale_y, 0, &long_dimension);
				embed_height = (short)long_dimension;
			}
			break;

		case pict_hscale_command:
			if (parameter) {
  
  			    metascale_x = parameter;
			    metascale_x <<= 16;
			    metascale_x |= 100;

				long_dimension = embed_width;
				pgScaleLong(metascale_x, 0, &long_dimension);
				embed_width = (short)long_dimension;
			}
			break;

		case object_result_command:
			if (previous_embed)
				representing = TRUE;
			break;

		case rtf_signature_command:
			break;

		case table_defaults:

			table_level = 1;
			table_entry = 0;

			pgGetParInfoRec(import_pg, 0, &par_table);
			par_table.indents = current_par.indents;
			par_table.table.grid_borders = PG_BORDER_ALLSIDES | 0x08080808;

			if (non_table_text)
				next_table_id = pgUniqueID(import_pg);
			
			par_table.table.unique_id = next_table_id;
			par_table.table.cell_h_extra = 2;
			non_table_text = 0;

			last_column_tab = 0;
			result = (pg_boolean)(translator.bytes_transferred > 0);
			break;

		case windows_metafile_command:
			embed_type = embed_meta_file;
			metafile.mapping_mode = parameter;

			break;
	}
	
	return		result;
}


/* DoDocumentCommand handles commands that set document sizes, etc. */

pg_boolean PaigeRTFImportFilter::DoDocumentCommand (short command, short parameter)
{
	long			PG_FAR 	*value_ptr;
	long					real_parameter;
	
	real_parameter = (long)convert_twips(import_pg_rec, parameter);
	value_ptr = NULL;

	switch (command) {
	
		case bottom_margin_command:
			value_ptr = &translator.doc_info.margins.bot_right.v;
			break;

		case left_margin_command:
			value_ptr = &translator.doc_info.margins.top_left.h;
			break;
			
		case right_margin_command:
			value_ptr = &translator.doc_info.margins.bot_right.h;
			break;

		case top_margin_command:
			value_ptr = &translator.doc_info.margins.top_left.v;
			break;

		case paper_height_command:
			value_ptr = &translator.doc_info.print_target.bot_right.v;
			break;

		case paper_width_command:
			value_ptr = &translator.doc_info.print_target.bot_right.h;
			break;
	}
	
	if (value_ptr) {
		
		translator.doc_info_changed |= (pg_boolean)(*value_ptr != real_parameter);
		*value_ptr = real_parameter;
	}

	return	FALSE;
}


/* DoParagraphCommand handles paragraph formatting commands. */

pg_boolean PaigeRTFImportFilter::DoParagraphCommand (short command, short parameter)
{
	pg_boolean		result = FALSE;
	pg_short_t		tab_index;
	long			real_parameter, AND_mask, table_index, shading_color;
	
	if (index_level > 0 || toc_level > 0)
		return	FALSE;

	real_parameter = (long)convert_twips(import_pg_rec, parameter);

	tab_index = current_par.num_tabs;
	table_index = par_table.table.table_columns;

	switch (command) {

		case box_border:
			current_par.table.border_info |= PG_BORDER_ALLSIDES;
			current_par.table.border_spacing = 2;
			last_border = -1;
			stylesheet_par_mask.table.border_info = stylesheet_par_mask.table.border_spacing = 0;
			break;

		case bottom_border:
			if (double_borders)
				current_par.table.border_info |= (PG_BORDER_BOTTOMLINE << 1);
			else
				current_par.table.border_info |= PG_BORDER_BOTTOMLINE;

			current_par.table.border_spacing = 2;
			last_border = 24;
			stylesheet_par_mask.table.border_info = stylesheet_par_mask.table.border_spacing = 0;
			break;
		
		case border_colorfill:
			if (GetMemorySize(color_table) > (long)parameter) {
				color_value			color;
				long				red, green, blue, translated_color;

				GetMemoryRecord(color_table, parameter, &color);

				red = (long)(color.red & 0xFF00);
				green = (long)(color.green & 0xFF00);
				blue = (long)(color.blue & 0xFF00);
				translated_color = ((blue << 8) | green | (red >> 8));
				
				switch (last_border) {
					
					case -1:
						current_par.table.left_border_color = current_par.table.top_border_color
								= current_par.table.bottom_border_color = current_par.table.right_border_color = translated_color;
						break;
					
					case 0:
						current_par.table.left_border_color = translated_color;
						break;
					
					case 8:
						current_par.table.right_border_color = translated_color;
						break;
					
					case 16:
						current_par.table.top_border_color = translated_color;
						break;

					case 24:
						current_par.table.bottom_border_color = translated_color;
						break;
				}
			}
			break;

		case double_border:
			if (last_border < 0)
				real_parameter = PG_BORDER_ALLDOUBLE;
			else {
			
				real_parameter = PG_BORDER_DOUBLE;
				
				if (last_border > 0)
					real_parameter <<= last_border;
			}
			
			current_par.table.border_info |= real_parameter;
			stylesheet_par_mask.table.border_info = 0;
			break;
		
		case dot_border:
			if (last_border < 0)
				real_parameter = PG_BORDER_ALLDOTTED;
			else {
			
				real_parameter = PG_BORDER_DOTTED;
				
				if (last_border > 0)
					real_parameter <<= last_border;
			}
			
			if (table_level || table_entry) {
				
				par_table.tabs[par_table.table.table_columns].leader |= real_parameter;
			}
			else
				current_par.table.border_info |= real_parameter;
			
			stylesheet_par_mask.table.border_info = 0;

			break;
			
		case left_border:
			if (double_borders)
				current_par.table.border_info |= (PG_BORDER_LEFTLINE << 1);
			else
				current_par.table.border_info |= PG_BORDER_LEFTLINE;

			current_par.table.border_spacing = 2;
			last_border = 0;
			stylesheet_par_mask.table.border_info = stylesheet_par_mask.table.border_spacing = 0;
			break;

		case right_border:
			if (double_borders)
				current_par.table.border_info |= (PG_BORDER_RIGHTLINE << 1);
			else
				current_par.table.border_info |= PG_BORDER_RIGHTLINE;
				
			current_par.table.border_spacing = 2;
			last_border = 8;
			stylesheet_par_mask.table.border_info = stylesheet_par_mask.table.border_spacing = 0;
			break;

		case shadow_border:
			real_parameter = PG_BORDER_SHADOW;
			
			if (last_border < 0)
				real_parameter = PG_BORDER_SHADOWBOX;
			else
			if (last_border > 0)
				real_parameter <<= last_border;
			
			current_par.table.border_info |= real_parameter;
			stylesheet_par_mask.table.border_info = 0;
			break;

		case top_border:
			if (double_borders)
				current_par.table.border_info |= (PG_BORDER_TOPLINE << 1);
			else
				current_par.table.border_info |= PG_BORDER_TOPLINE;
				
			current_par.table.border_spacing = 2;
			last_border = 16;
			stylesheet_par_mask.table.border_info = stylesheet_par_mask.table.border_spacing = 0;
			break;

		case thick_border:
			double_borders = TRUE;
			break;

		case border_width:
			
			if (double_borders) {
				pg_fixed		twips_mod;
				
				(twips_mod = pgFixedRatio(parameter, 20)) & 0x00008000;
				
				if (twips_mod)
					parameter += 20;
				
				real_parameter = (long)convert_twips(import_pg_rec, parameter * 2);
			}
			
			if (real_parameter > 7)
				real_parameter = 7;

			if (last_border < 0) {
			
				real_parameter = (real_parameter | (real_parameter << 8) | (real_parameter << 16) | (real_parameter << 24));
				AND_mask = 0x07070707;
			}
			else
			if (last_border > 0) {
			
				real_parameter <<= last_border;
				AND_mask = 0x07 << last_border;
			}
			else
				AND_mask = 0x07;

			if (table_level || table_entry) {
				
				par_table.tabs[par_table.table.table_columns].leader &= (~AND_mask);
				par_table.tabs[par_table.table.table_columns].leader |= real_parameter;
			}
			else {
				
				current_par.table.border_info &= (~AND_mask);
				current_par.table.border_info |= real_parameter;
			}
			
			stylesheet_par_mask.table.border_info = 0;
			
			break;

		case border_spacing:
			current_par.table.border_spacing = real_parameter;
			stylesheet_par_mask.table.border_spacing = 0;
			break;

		case first_indent_command:
			current_par.indents.first_indent = real_parameter;
			stylesheet_par_mask.indents.first_indent = 0;
			break;

		case keep_together_command:
			break;

		case left_indent_command:
			current_par.indents.left_indent = real_parameter;
			stylesheet_par_mask.indents.left_indent = 0;
			break;

		case page_break_before_command:
			break;
			
		case par_default_command:
			pgGetParInfoRec(import_pg, 0, &current_par);
			table_entry = 0;
			double_borders = FALSE;
			pgFillBlock(&stylesheet_par_mask, sizeof(par_info), 0);
			break;

		case center_justify_command:
			current_par.justification = justify_center;
			stylesheet_par_mask.justification = 0;
			break;

		case full_justify_command:
			current_par.justification = justify_full;
			stylesheet_par_mask.justification = 0;
			break;

		case left_justify_command:
			current_par.justification = justify_left;
			stylesheet_par_mask.justification = 0;
			break;

		case right_justify_command:
			current_par.justification = justify_right;
			stylesheet_par_mask.justification = 0;
			break;

		case right_indent_command:
			current_par.indents.right_indent = real_parameter;
			stylesheet_par_mask.indents.right_indent = 0;
			break;

		case space_after_command:
			current_par.bot_extra = pgAbsoluteValue(real_parameter);
			stylesheet_par_mask.bot_extra = 0;
			break;

		case space_before_command:
			current_par.top_extra = pgAbsoluteValue(real_parameter);
			stylesheet_par_mask.top_extra = 0;
			break;
		
		case shading_command:
			shading_color = (parameter * 255) / 10000;
			
			if (shading_color) {
			
				if ((shading_color = 255 - shading_color) == 0)
					shading_color = 1;

				current_par.table.border_shading = (shading_color << 16) | (shading_color << 8) | shading_color;
			}
			break;
			
		case line_spacing_command:
			current_par.leading_fixed = pgAbsoluteValue(real_parameter);
			stylesheet_par_mask.leading_fixed = 0;
			break;

		case dot_leader_command:
			if (tab_index < TAB_ARRAY_SIZE)
				current_par.tabs[tab_index].leader = (pg_short_t)'.';
			stylesheet_par_mask.num_tabs = 0;
			pgFillBlock(stylesheet_par_mask.tabs, sizeof(tab_stop) * TAB_ARRAY_SIZE, 0);
			break;
			
		case dash_leader_command:
			if (tab_index < TAB_ARRAY_SIZE)
				current_par.tabs[tab_index].leader = (pg_short_t)'-';
			stylesheet_par_mask.num_tabs = 0;
			pgFillBlock(stylesheet_par_mask.tabs, sizeof(tab_stop) * TAB_ARRAY_SIZE, 0);
			break;

		case center_tab_command:
			if (tab_index < TAB_ARRAY_SIZE)
				current_par.tabs[tab_index].tab_type = center_tab;
			stylesheet_par_mask.num_tabs = 0;
			pgFillBlock(stylesheet_par_mask.tabs, sizeof(tab_stop) * TAB_ARRAY_SIZE, 0);
			break;

		case decimal_tab_command:
			if (tab_index < TAB_ARRAY_SIZE)
				current_par.tabs[tab_index].tab_type = decimal_tab;
			stylesheet_par_mask.num_tabs = 0;
			pgFillBlock(stylesheet_par_mask.tabs, sizeof(tab_stop) * TAB_ARRAY_SIZE, 0);
			break;

		case right_tab_command:
			if (tab_index < TAB_ARRAY_SIZE)
				current_par.tabs[tab_index].tab_type = right_tab;
			stylesheet_par_mask.num_tabs = 0;
			pgFillBlock(stylesheet_par_mask.tabs, sizeof(tab_stop) * TAB_ARRAY_SIZE, 0);
			break;

		case set_tab_command:
			if (tab_index < TAB_ARRAY_SIZE) {
					rectangle		wrap_bounds, vis_bounds;

				if (current_par.tabs[tab_index].tab_type == no_tab)
					current_par.tabs[tab_index].tab_type = left_tab;
				
				pgAreaBounds(import_pg, &wrap_bounds, &vis_bounds);
				real_parameter += import_pg_rec->tab_compensate;

				if (import_pg_rec->tab_base != TAB_BOUNDS_RELATIVE)
					real_parameter -= wrap_bounds.top_left.h;
				
				if (real_parameter < 0)
					real_parameter = 0;

				current_par.tabs[tab_index].position = real_parameter;
				current_par.num_tabs += 1;

				stylesheet_par_mask.num_tabs = 0;
				pgFillBlock(stylesheet_par_mask.tabs, sizeof(tab_stop) * TAB_ARRAY_SIZE, 0);
			}

			break;
	}

	if (current_stylesheet)
		stylesheet.has_par_styles = TRUE;
	else
	if (command != par_default_command)
		if (current_par.style_sheet_id > 0)
			current_par.style_sheet_id = -current_par.style_sheet_id;

	return	FALSE;
}


/* DoStyleCommand handles text formatting commands. */

pg_boolean PaigeRTFImportFilter::DoStyleCommand (short command, short parameter)
{
	pg_boolean			result = FALSE;
	style_table_ptr		stylesheet_ptr;
	font_info_ptr		font_ptr;
	long				kerning_register;
	short				supersub_index;

	switch (command) {
	
		case bold_command:
			if (had_actual_param && parameter == 0)
				current_style.styles[bold_var] = 0;
			else
				current_style.styles[bold_var] = -1;
			break;

		case all_caps_command:
			current_style.styles[all_caps_var] = -1;
			break;
		
		case fgcolor_command:
			if (GetMemorySize(color_table) > (long)parameter)
				GetMemoryRecord(color_table, parameter, &current_style.fg_color);
			break;

		case subscript_command:
			current_style.styles[subscript_var] = (short)(parameter / 2);
			break;
		
		case expand_condense_command:
			kerning_register = parameter;
			
			if (kerning_register >= 57 && kerning_register <= 63)
				kerning_register -= 64;
	
			current_style.char_extra = pgFixedRatio((short)kerning_register, 4);
			current_style.char_extra = current_style.char_extra;

			break;

		case charset_command:
		case font_charset_command:
			current_font.machine_var[PG_CHARSET] = parameter;
			
			if (!translator.bytes_transferred)
				translator.font.machine_var[PG_CHARSET] = parameter;

			break;

		case font_command:
			if ((font_ptr = locate_font_in_table(font_table, parameter)) != NULL) {

				pgBlockMove(font_ptr->name, current_font.name, FONT_SIZE * sizeof(pg_char));
				current_font.machine_var[PG_CHARSET] = font_ptr->machine_var[PG_CHARSET];
				UnuseMemory(font_table);

				translator.font_changed |= !pgEqualStruct(&translator.font, &current_font, SIGNIFICANT_FONT_SIZE);

				if (!translator.bytes_transferred)
					translator.font = current_font;
				else 
					result = translator.font_changed;
				
				if (current_stylesheet)
					stylesheet.has_font = TRUE;
			}
			break;

		case pointsize_command:
			current_style.point = (pg_fixed)(parameter / 2);
			current_style.point <<= 16;
			break;

		case italic_command:
			if (had_actual_param && parameter == 0)
				current_style.styles[italic_var] = 0;
			else
				current_style.styles[italic_var] = -1;
			break;

		case outline_command:
			if (had_actual_param && parameter == 0)
				current_style.styles[outline_var] = 0;
			else
				current_style.styles[outline_var] = -1;
			break;

		case plain_command:
			current_style = import_pg_rec->globals->def_style;
			current_style.point = 0x000C0000;
			break;

		case style_command:
			if (current_stylesheet)
				stylesheet.style_id = current_stylesheet->style_id = parameter;
			else
			if ((stylesheet_ptr = locate_stylesheet(translator.stylesheet_table, parameter)) != NULL) {
				
				pgGetNamedStyleInfo(import_pg, stylesheet_ptr->index, &current_style, &current_font, &current_par);
				::UnuseMemory(translator.stylesheet_table);
			}
			
			translator.font_changed |= !pgEqualStruct(&translator.font, &current_font, SIGNIFICANT_FONT_SIZE);

			if (!translator.bytes_transferred)
				translator.font = current_font;
			else 
				result = translator.font_changed;

			break;

		case small_caps_command:
			current_style.styles[small_caps_var] = -1;
			break;

		case shadow_command:
			current_style.styles[shadow_var] = -1;
			break;

		case strikeout_command:
			current_style.styles[strikeout_var] = -1;
			break;

		case sub_command:
		case super_command:
			current_style.point = (current_style.point >> 1) + (current_style.point >> 2);
			kerning_register = current_style.point >> 16;
			
			if (command == super_command)
				supersub_index = superscript_var;
			else
				supersub_index = subscript_var;
				
			current_style.styles[supersub_index] = (short)(kerning_register / 2);
			current_style.shift_verb = 0;
			break;

		case toc_command:
			if (!(import_bits & IMPORT_HYPERTEXT_FLAG))
				break;

			toc_level = GetMemorySize(style_stack);
			SetMemorySize(index_text_ref, 0);
			index_begin = bytes_imported + translator.bytes_transferred; 
			break;

		case toc_level_command:
			break;

		case underline_command:
			if (had_actual_param && parameter == 0)
				current_style.styles[italic_var] = 0;
			else
				current_style.styles[underline_var] = -1;
			break;

		case dotted_underline_command:
			current_style.styles[dotted_underline_var] = -1;
			break;

		case double_underline_command:
			current_style.styles[dbl_underline_var] = -1;
			break;

		case word_underline_command:
			current_style.styles[word_underline_var] = -1;
			break;

		case superscript_command:
			current_style.styles[superscript_var] = (short)(parameter / 2);
			break;
			
		case invis_text_command:
			current_style.styles[hidden_text_var] = -1;
			break;
		
		case index_style_command:
			if (!(import_bits & IMPORT_HYPERTEXT_FLAG))
				break;

			index_level = GetMemorySize(style_stack);
			SetMemorySize(index_text_ref, 0);
			index_begin = bytes_imported + translator.bytes_transferred;
			break;
	}

	if (current_stylesheet) {
		
		if (command != style_command)
			stylesheet.has_styles = TRUE;
	}
	else {
		
		translator.format_changed |= !pgEqualStruct(&translator.format, &current_style, SIGNIFICANT_STYLE_SIZE);
		
		if (translator.format_changed)
			if (current_style.style_sheet_id > 0)
				current_style.style_sheet_id = -current_style.style_sheet_id;

		if (!translator.bytes_transferred)
			translator.format = current_style;
		else
			result |= translator.format_changed;
	}

	return		result;
}


/* ProcessInfoCommand gets called for "\info" type commands. The default member
function here does nothing but you can override. */

void PaigeRTFImportFilter::ProcessInfoCommand (short command, short parameter)
{
#ifdef MAC_PLATFORM
#pragma unused (command, parameter)
#endif

	SkipCurrentLevel();
}


/* ProcessIndexCommand gets called when an index style is hit.  The entire text for
the index is provided. */

void PaigeRTFImportFilter::ProcessIndexCommand (pg_char_ptr index_text, long length)
{
	long			use_length;
	
	if ((use_length = length) > 0)
		if (index_text[use_length - 1] == ';')
			--use_length;
	
	if (toc_level) {
		
		if (!(import_bits & IMPORT_TOC_FLAG))
			return;
	}
	else
	if (!(import_bits & IMPORT_INDEX_FLAG))
		return;

	if (use_length) {
		
		pgFillBlock(translator.hyperlink_target.URL, FONT_SIZE + BOM_HEADER, 0);
		translator.hyperlink_target.alt_URL = MEM_NULL;
		translator.hyperlink_target.callback = pgStandardTargetCallback;
		translator.hyperlink_target.unique_id = translator.hyperlink_target.target_id
					= translator.hyperlink_target.type = translator.hyperlink_target.refcon = 0;
		translator.hyperlink_target.applied_range.begin = translator.hyperlink_target.applied_range.end = 0;
		translator.hyperlink_target.active_style = translator.hyperlink_target.state1_style;
		
		pgInsertURL(import_pg_rec, &translator.hyperlink_target, index_text);
		
		if (toc_level) {
		
			translator.hyperlink_target.type = HYPERLINK_TOC;
			translator.hyperlink_target.active_style = translator.hyperlink_target.state1_style;
		}
		else
			translator.hyperlink_target.type = HYPERLINK_INDEX;

		translator.hyperlink_target.applied_range.begin = translator.hyperlink_target.applied_range.end = index_begin;
		translator.hyperlink_target.applied_range.end = bytes_imported + translator.bytes_transferred;
		
		if (translator.hyperlink_target.applied_range.end == translator.hyperlink_target.applied_range.begin)
			++translator.hyperlink_target.applied_range.end;

		translator.hyperlink_target_changed = TRUE;
	}
}


/* UnsupportedCommand gets called when an RTF command is not recognized. The
token is in command + any parameters */

void PaigeRTFImportFilter::UnsupportedCommand (pg_char_ptr command, short parameter)
{
#ifdef MAC_PLATFORM
#pragma unused (command, parameter)
#endif
}


/* ProcessTableCommand gets called when a table has been started (but not completed).
Essentially every "table" token is passed to this function.  Return TRUE if you want the
importer to insert the current setting(s) and format(s).  */

pg_boolean PaigeRTFImportFilter::DoTableCommand (short command, short parameter)
{
	pg_boolean			result = FALSE;
	long				column_value, real_parameter, table_index, shading_color;
	short				column_index, column_tab;

	real_parameter = (long)convert_twips(import_pg_rec, parameter);
	table_index = par_table.table.table_columns;

	switch (command) {
	
		case table_cell:
			OutputCharacter((pg_char)paige_globals->tab_char);
			break;

		case cell_setright:			
			if (table_index == TAB_ARRAY_SIZE)
				break;
			
			column_tab = (short)last_column_tab;
			last_column_tab = real_parameter;
			real_parameter -= column_tab;

			if (par_table.table.table_column_width == 0)
				par_table.table.table_column_width = real_parameter;
			
			par_table.tabs[table_index].position = real_parameter;
			++table_index;
			par_table.table.table_columns = table_index;
			par_table.num_tabs = (short)table_index;
			break;

		case cell_border_bottom:
			par_table.tabs[table_index].leader |= PG_BORDER_BOTTOMLINE;
			last_border = 24;
			break;

		case cell_border_left:
			par_table.tabs[table_index].leader |= PG_BORDER_LEFTLINE;
			last_border = 0;
			break;

		case cell_border_right:
			par_table.tabs[table_index].leader |= PG_BORDER_RIGHTLINE;
			last_border = 8;
			break;

		case cell_border_top:
			par_table.tabs[table_index].leader |= PG_BORDER_TOPLINE;
			last_border = 16;
			break;

		case cell_first_merge:
		case cell_merge:
			break;
		
		case cell_shading:
			shading_color = (parameter * 255) / 10000;
			
			if (shading_color) {
			
				if ((shading_color = 255 - shading_color) == 0)
					shading_color = 1;

				par_table.tabs[table_index].ref_con = (shading_color << 16) | (shading_color << 8) | shading_color;
			}
			break;
						
		case enter_table:
			current_par = par_table;
			++table_entry;
			break;

		case table_row_end:
			if (translator.bytes_transferred) {
				
				--output_ptr;
				
				if (*output_ptr == (pg_char)paige_globals->tab_char)
					--translator.bytes_transferred;
				else
					++output_ptr;
			}
			else
				pgRemoveLastInsert(import_pg_rec, (pg_char)paige_globals->tab_char);

			OutputCharacter((pg_char)paige_globals->line_wrap_char);

			if ((table_level -= 1) <= 0)
				table_level = table_entry = 0;
			
			translator.par_format_changed |= !pgEqualStruct(&translator.par_format, &current_par, SIGNIFICANT_PAR_STYLE_SIZE);
			result = TRUE;
			break;

		case table_border_bottom:
			par_table.table.border_info |= PG_BORDER_BOTTOMLINE;
			break;


		case table_border_left:
			par_table.table.border_info |= PG_BORDER_LEFTLINE;
			break;

		case table_border_right:
			par_table.table.border_info |= PG_BORDER_RIGHTLINE;
			break;

		case table_border_vertical:
		case table_border_horizontal:
			if (command == table_border_vertical)
				column_value = PG_BORDER_RIGHTLINE;
			else
				column_value = PG_BORDER_BOTTOMLINE;

			for (column_index = 0; column_index < table_index; ++column_index)
				par_table.tabs[column_index].leader |= column_value;

			break;

		case table_border_top:
			par_table.table.border_info |= PG_BORDER_TOPLINE;
			break;

		case table_spacing:
		case table_header:
		case table_keep_together:
			break;

		case table_position_left:
			break;

		case table_center:
//			par_table.justification = justify_center;
			break;

		case table_left:
			par_table.justification = justify_left;
			break;

		case table_right:
//			par_table.justification = justify_right;
			break;

		case table_height:
			break;
	}

	return		result;
}


/* PrepareRTFPicture prepares for reading a picture. It is necessary to read the picture
after the current text, if any, is transferred. If result == TRUE then current text should
be sent to the host. */

pg_boolean PaigeRTFImportFilter::PrepareRTFPicture (short pict_command_type)
{
	long				command_result;
	short				parameter, table_id, command;
	pg_boolean			reading_object;
	pg_char				next_char;

	pgFillBlock(&metafile, sizeof(metafile_struct), 0);
	twips_width = twips_gwidth = twips_height = twips_gheight = 0;
	metascale_x = metascale_y = 0;

	unsupported_ref = MEM_NULL;

	embed_width = embed_height = 0;
	reading_object = (pg_boolean)(pict_command_type == object_command);

	if (reading_object) {
	
		embed_type = embed_unsupported_object;
		parallel_buffer = MemoryAlloc(paige_globals->mem_globals, sizeof(pg_char), 0, 32);
	}
	else
		embed_type = 0;

	embed_binary = 0;

	while ((!is_hex_char(io_buffer[buffer_index]) && !embed_binary) || reading_object) {
		
		if (pgGetImportChar(&next_char) != NO_ERROR)
			return	FALSE;

		if (next_char == RTF_COMMAND_CHAR) {

			if ((command_result = ReadCommand(&parameter, &next_char)) != 0) {
				
				table_id = pgHiWord(command_result);
				command = pgLoWord(command_result);
				
				if (table_id == DESTINATION_COMMAND) {
					
					if (command == object_data_command)
						reading_object = FALSE;
					else
						DoDestinationCommand(command, parameter);
				}
			}
		}
		else
		if (next_char == RTF_GROUPBEGIN_CHAR)
				PushStyleStack();
		else
		if (next_char == RTF_GROUPEND_CHAR)
			PopStyleStack();
	
	}
	
	if (parallel_buffer) {
		
		unsupported_ref = parallel_buffer;
		parallel_buffer = MEM_NULL;
	}

	picture_ref = MemoryAlloc(paige_globals->mem_globals, 1, 0, PICT_APPEND_SIZE);
	
	if (translator.bytes_transferred)
		return	TRUE;

	return	ReadRTFPicture();
}


/* ReadRTFPicture imports an RTF graphic. This only gets called after a picture has been
prepped for import. */

pg_boolean PaigeRTFImportFilter::ReadRTFPicture (void)
{
	register pg_bits8_ptr		pict_output;
	short						output_ctr, byte_toggle;
	long						real_output;
	pg_char						next_char, hex_char;

	if (!(import_bits & IMPORT_EMBEDDED_OBJECTS_FLAG)) {
		
		// Pass through binary data, if any

		while (embed_binary) {
			
			if (pgGetImportChar(&next_char) != NO_ERROR)
				break;
			
			--embed_binary;

		}
		
		while (pgGetImportChar(&next_char) == NO_ERROR) {

			if (next_char == RTF_GROUPEND_CHAR) {
				
				PopStyleStack();
				break;
			}
		}

		return	FALSE;
	}

	::SetMemorySize(picture_ref, PICT_APPEND_SIZE);
	pict_output = (pg_bits8_ptr)UseMemory(picture_ref);
	output_ctr = PICT_APPEND_SIZE;
	real_output = 0;
	byte_toggle = 0;

	if (embed_binary) {

		while (embed_binary) {
			
			if (pgGetImportChar(&next_char) != NO_ERROR)
				break;
					
			*pict_output++ |= (pg_bits8)next_char;
			++real_output;
			
			if ((output_ctr -= 1) == 0) {
				
				pict_output = (pg_bits8_ptr)AppendMemory(picture_ref, PICT_APPEND_SIZE, FALSE);
				output_ctr = PICT_APPEND_SIZE;
			}
			
			--embed_binary;
		}

		while (pgGetImportChar(&next_char) == NO_ERROR) {
			
			if (next_char == RTF_GROUPEND_CHAR) {
				
				PopStyleStack();
				break;
			}
		}
	}
	else {
	
		while (pgGetImportChar(&next_char) == NO_ERROR) {
			
			if (next_char == RTF_GROUPEND_CHAR) {
				
				PopStyleStack();
				break;
			}
			else
			if (is_hex_char(next_char)) {
			
				hex_char = (pg_char)translate_hex(next_char);
				++byte_toggle;
		
				if (byte_toggle & 1)
					*pict_output = (hex_char << 4);
				else {
					
					*pict_output++ |= hex_char;
					++real_output;
					
					if ((output_ctr -= 1) == 0) {

						pict_output = (pg_bits8_ptr)AppendMemory(picture_ref, PICT_APPEND_SIZE, FALSE);
						output_ctr = PICT_APPEND_SIZE;
					}
				}
			}
		}
	}

	UnuseMemory(picture_ref);
	SetMemorySize(picture_ref, real_output);
	
	if (!real_output || !embed_type)
		DisposeMemory(picture_ref);
	else {
		embed_ref			embed;
		pg_embed_ptr		embed_ptr;
		void PG_FAR			*embed_data;
		
		embed_modifier = 0;
		
		if (embed_width < 0)
			embed_width = -embed_width;
		if (embed_height < 0)
			embed_height = -embed_height;

		if ((embed_data = pgProcessEmbedData(picture_ref, embed_type)) == NULL)
			embed_data = (void PG_FAR *) picture_ref;
		
		if (embed_type == embed_unsupported_object)
			embed_modifier = GetMemorySize(picture_ref);
		else
		if (embed_type == embed_meta_file) {
			
			metafile.metafile = (long)embed_data;
			
			if ((metafile.bounds.bot_right.h = embed_width) == 0)
				metafile.bounds.bot_right.h = metafile.x_ext;
			if ((metafile.bounds.bot_right.v = embed_height) == 0)
				metafile.bounds.bot_right.v = metafile.y_ext;
			
			embed_data = (void PG_FAR *)&metafile;
			
	// - or -
	#ifdef MAC_PLATFORM
		{
			Rect				frame;
			pg_bits8_ptr		bits;
			
			bits = (pg_bits8_ptr)UseMemory(picture_ref);
			RectangleToRect(&metafile.bounds, NULL, &frame);			
			embed_data = (void PG_FAR *)TranslateWMFToPicture(&bits, &frame);
			embed_type = embed_mac_pict;
			UnuseAndDispose(picture_ref);
			picture_ref = (memory_ref)embed_data;
		}
		
	#endif

		}

		embed = pgNewEmbedRef(paige_globals->mem_globals, embed_type,
				embed_data, embed_modifier, 0, 0, 0, FALSE);

		embed_ptr = (pg_embed_ptr)UseMemory(embed);
		
		if (embed_ptr->width == 0 || embed_type == embed_mac_pict)
			embed_ptr->width = embed_width;
		if (embed_ptr->height == 0 || embed_type == embed_mac_pict)
			embed_ptr->height = embed_height;
		
		embed_ptr->rtf_text_data = unsupported_ref;

		if (embed_type == embed_meta_file) {

			embed_ptr->uu.pict_data.twips_width = twips_width;
			embed_ptr->uu.pict_data.twips_height = twips_height;
			embed_ptr->uu.pict_data.twips_gwidth = twips_gwidth;
			embed_ptr->uu.pict_data.twips_gheight = twips_gheight;
			embed_ptr->uu.pict_data.horiz_scale = metascale_x;
			embed_ptr->uu.pict_data.vert_scale = metascale_y;
		}
		
		if (representing)
			embed_ptr->type |= REPRESENTS_OTHER_FLAG;

		UnuseMemory(embed);

		if (representing && previous_embed) {
			pg_embed_ptr		previous_ptr;
			font_info_ptr		mother_font;
			style_info_ptr		mother_style;

			embed_ptr = (pg_embed_ptr)UseMemory(embed);
			previous_ptr = (pg_embed_ptr)UseMemory(previous_embed);
			previous_ptr->embed_represent = (memory_ref)embed;
			previous_ptr->height = embed_ptr->height;
			previous_ptr->width = embed_ptr->width;
			previous_ptr->uu = embed_ptr->uu;
			previous_ptr->descent = embed_ptr->descent;
			
			mother_style = (style_info_ptr)UseMemoryRecord(import_pg_rec->t_formats, previous_ptr->lowlevel_index, 0, TRUE);
			mother_font = (font_info_ptr)UseMemoryRecord(import_pg_rec->fonts, (long)mother_style->font_index, 0, TRUE);
			mother_style->class_bits &= (~EMBED_INITED_BIT);
			pgEmbedStyleInit(import_pg_rec, mother_style, mother_font);
			
			UnuseMemory(import_pg_rec->t_formats);
			UnuseMemory(import_pg_rec->fonts);
		
			UnuseMemory(previous_embed);
			UnuseMemory(embed);

			real_output = 0;
			previous_embed = MEM_NULL;
			representing = FALSE;
		}
		else {

			previous_embed = embed;
			representing = FALSE;
			pgInitEmbedStyleInfo(import_pg_rec, 0, embed, 0, NULL, 0, &current_style, NULL, NULL, NULL, FALSE);

			OutputCharacter(DUMMY_LEFT_EMBED);
			OutputCharacter(DUMMY_RIGHT_EMBED);
			
			translator.format = current_style;
			translator.format_changed = TRUE;
		}
	}

	picture_ref = MEM_NULL;
	representing = FALSE;

	return	(pg_boolean)(real_output > 0);
}


/* PushStyleStack pushes the current style and font into a "stack" for {levels} */

void PaigeRTFImportFilter::PushStyleStack (void)
{
	format_stack_ptr	style_stack_ptr;

	style_stack_ptr = (format_stack_ptr)AppendMemory(style_stack, 1, FALSE);
	pgBlockMove(&current_style, &style_stack_ptr->style, sizeof(style_info));
	pgBlockMove(&current_font, &style_stack_ptr->font, sizeof(font_info));
	UnuseMemory(style_stack);
}


/* PopStyleStack pops the current style and font into a "stack" for {levels} */

void PaigeRTFImportFilter::PopStyleStack (void)
{
	format_stack_ptr	style_stack_ptr;
	long				style_stack_size;

	if ((style_stack_size = GetMemorySize(style_stack)) > 0) {

		--style_stack_size;
		
		style_stack_ptr = (format_stack_ptr)UseMemory(style_stack);
		style_stack_ptr += style_stack_size;
		pgBlockMove(&style_stack_ptr->style, &current_style, sizeof(style_info));
		pgBlockMove(&style_stack_ptr->font, &current_font, sizeof(font_info));
		UnuseMemory(style_stack);

		SetMemorySize(style_stack, style_stack_size);
		
		if (style_stack_size < index_level || style_stack_size < toc_level) {
			// Process the index text.
			long			index_length;
			
			if ((index_length = GetMemorySize(index_text_ref)) > 0) {
				
				AppendMemory(index_text_ref, 1, TRUE);
				UnuseMemory(index_text_ref);
				this->ProcessIndexCommand((pg_char_ptr)UseMemory(index_text_ref), index_length);
				UnuseMemory(index_text_ref);
			}
			
			index_level = toc_level = 0;
		}

		translator.format_changed |= !pgEqualStruct(&translator.format, &current_style, SIGNIFICANT_STYLE_SIZE);
		translator.font_changed |= !pgEqualStruct(&translator.font, &current_font, SIGNIFICANT_FONT_SIZE);
	}
}


/* SkipCurrentLevel just scans the byte stream until the current level is reduced by one. */

void PaigeRTFImportFilter::SkipCurrentLevel (void)
{
	pg_char			next_char;
	long			stack_level, current_level;
	
	if ((stack_level = GetMemorySize(style_stack)) != 0) {
		
		current_level = stack_level;

		while (pgGetImportChar(&next_char) == NO_ERROR) {
			
			if (next_char == RTF_GROUPBEGIN_CHAR) {

				if (current_stylesheet)
					++stylesheet_level;

				++current_level;
			}
			else
			if (next_char == RTF_GROUPEND_CHAR) {
				
				--current_level;

				if (current_stylesheet)
					--stylesheet_level;

				if (current_level < stack_level) {
					
					PopStyleStack();
					break;
				}
			}
		}
	}
}


/* ProcessStylesheet gets called after a name terminator in the text stream. Here we
complete the stylesheet record and begin a new one. */

void PaigeRTFImportFilter::ProcessStylesheet (void)
{
	named_stylesheet	named_style;
	style_info_ptr		style_ptr = NULL;
	par_info_ptr		par_ptr = NULL;
	font_info_ptr		font_ptr = NULL;

	if (stylesheet.name[0]) {
	
		stylesheet.style = current_style;
		stylesheet.font = current_font;
		stylesheet.par = current_par;
		
		if (import_bits & IMPORT_STYLESHEETS_FLAG) {

			style_ptr = &stylesheet.style;
			par_ptr = &stylesheet.par;
			
			if (!FindFontInList(&stylesheet.font))
				pgMapFont(&stylesheet.font, file_os, CURRENT_OS);

			font_ptr = &stylesheet.font;
		}
		
		current_stylesheet->index = 0;
		current_stylesheet->index = pgNewNamedStyle(import_pg, stylesheet.name, style_ptr, font_ptr, par_ptr);
		
		pgGetNamedStyle(import_pg, current_stylesheet->index, &named_style);
		
		if (stylesheet.style_id == 0) {
		
			par_ptr = (par_info_ptr)UseMemory(import_pg_rec->par_formats);
			*par_ptr = stylesheet.par;
			par_ptr->used_ctr += 1;
			UnuseMemory(import_pg_rec->par_formats);
			
			import_pg_rec->def_named_index = current_stylesheet->index - 1;
		}

		current_stylesheet = (style_table_ptr)::AppendMemory(translator.stylesheet_table, 1, TRUE);
	}

	pgFillBlock(&stylesheet, sizeof(style_table_entry), 0);
	stylesheet_index = 0;
	
	current_style = paige_globals->def_style;
	current_style.point = 0x000C0000;
	current_par = paige_globals->def_par;
	stylesheet.par = current_par;
	GetMemoryRecord(import_pg_rec->fonts, 0, &current_font);
}


/********************************* Local Functions ********************************/


/* locate_font_in_table walks through the stored font table looking for family_id */

static font_info_ptr locate_font_in_table (memory_ref table, short family_id)
{
	font_info_ptr			fonts;
	long					num_fonts;
	
	if ((num_fonts = GetMemorySize(table)) > 0) {
		
		fonts = (font_info_ptr)UseMemory(table);
		
		while (num_fonts) {
			
			if (fonts->family_id == family_id)
				return		fonts;
			
			++fonts;
			--num_fonts;
		}
	}
	
	UnuseMemory(table);
	
	return	(font_info_ptr)NULL;
}



/* This functions scans a table of terms and returns the argument number if found,
or zero if not found. The arg_size is the length of the argument, however the argument might
be space-terminated or control-terminated.  If there is a numeric command or parameter in
the argument, it is returned in *param (if param is non-null) and *had_parameter is set to
TRUE. */

static short rtf_command (pg_char_ptr table, pg_char_ptr argument, pg_short_t arg_size,
		short PG_FAR *param, pg_boolean PG_FAR *had_parameter)
{
	register pg_char_ptr		table_ptr;
	register pg_char_ptr		arg_ptr;
	register pg_short_t			arg_ctr;
	short						table_item;
	
	*had_parameter = FALSE;

	if (!arg_size)
		return	FALSE;

	table_ptr = table;
	table_item = 1;

	while (*table_ptr) {
		
		arg_ptr = argument;

		if (*table_ptr > *arg_ptr)
			return	FALSE;
		
		if (*table_ptr == *argument) {
		
			arg_ctr = arg_size;
	
			for (arg_ctr = arg_size; arg_ctr; --arg_ctr) {

				if (*table_ptr++ != *arg_ptr++)
					break;

				if (*table_ptr == ' ') {
					
					if (arg_ctr == 1)
						return	table_item;
					
					if (!regular_letter(*arg_ptr)) {
						
						if (param) {
							
							if ( (*arg_ptr >= '0' && *arg_ptr <= '9') || (*arg_ptr == NEGATIVE_SIGN_CHAR)) {
							
								*param = (short)string_to_num(arg_ptr, arg_ctr - 1);
								*had_parameter = TRUE;
							}
							else
								*param = (short)(*arg_ptr & 0x00FF);
								
							*arg_ptr = 0;
						}

						return	table_item;
					}
					
					break;
				}
			}
		}

		for (;;)
			if (*table_ptr++ == ' ')
				break;

		table_item += 1;
	}

	return		0;
}


/* regular_letter returns TRUE if the_char is a - z or A-Z. */

static pg_boolean regular_letter (pg_char the_char)
{
	if (the_char < 'A' || the_char > 'z')
		return	FALSE;
	
	if (the_char > 'Z' && the_char < 'a')
		return	FALSE;

	return 	TRUE;
}


/* is_hex_char returns TRUE if the_char is hexidecimal ascii. */

static pg_boolean is_hex_char (pg_char the_char)
{
	if (the_char >= '0' && the_char <= '9')
		return	TRUE;
	if (the_char >= 'a' && the_char <= 'f')
		return	TRUE;
	if (the_char >= 'A' && the_char <= 'F')
		return	TRUE;

	return		FALSE;
}


/* string_to_num returns the numeric value of a string. */

static long string_to_num (pg_char_ptr string, short string_size)
{
	register pg_char_ptr	string_ptr;
	register short			string_ctr;
	pg_boolean				negative_sign;
	long					result = 0;
	
	string_ptr = string;
	string_ctr = string_size;
	
	if (*string_ptr == NEGATIVE_SIGN_CHAR) {
		
		negative_sign = TRUE;
		++string_ptr;
		--string_ctr;
	}
	else
		negative_sign = FALSE;

	while (string_ctr) {
		
		if (*string_ptr < '0' || *string_ptr > '9')
			break;
		
		result *= 10;
		result += (long)((*string_ptr++) & 0x0F);
		
		--string_ctr;
	}
	
	if (negative_sign)
		result = -result;

	return		result;
}

/* translate_hex translates a hax character to its value. */

static pg_short_t translate_hex (pg_char hex_char)
{
	pg_short_t			result;
	
	result = (pg_short_t)hex_char;
	
	if (result > 'F')
		result -= 0x20;
	if (result > '9')
		result -= 7;
	
	return		(result - 0x30);
}


/* locate_stylesheet returns a pointer to the stylesheet in table.  If found the pointer is returned. */

static style_table_ptr locate_stylesheet (memory_ref table, short style_id)
{
	style_table_ptr		table_ptr;
	long				num_entries, index_ctr;
	
	num_entries = GetMemorySize(table);
	table_ptr = (style_table_ptr)UseMemory(table);
	
	for (index_ctr = 1; index_ctr <= num_entries; ++index_ctr, ++table_ptr) {
		
		if (table_ptr->style_id == style_id)
			return		table_ptr;
	}
	
	UnuseMemory(table);
	
	return	(style_table_ptr)NULL;
}


/* convert_twips converts twipsvalue to the appropriate pixel size. */

static short convert_twips (paige_rec_ptr pg, short twipsvalue)
{
	long		result;		
	
	result = pgConvertResolution(pg, twipsvalue);
	result /= 20;
	
	return	(short)result;
}

