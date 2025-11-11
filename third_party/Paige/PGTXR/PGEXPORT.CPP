/* Import-Export module. Copyright 1995-96 by DataPak Software, Inc.  This software is a
part of the total PAIGE library.

This source file contains all the default member and non-member functions for the export class. */


#include "Paige.h"
#include "machine.h"
#include "pgExceps.h"
#include "pgTxrCPP.h"
#include "defprocs.h"
#include "pgDefStl.h"
#include "pgSelect.h"
#include "pgErrors.h"
#include "pgUtils.h"
#include "pgdeftbl.h"
#include "pgText.h"
#include "pgEmbed.h"
#include "pgOSUtl.h"


static pg_bits8_ptr unicode_to_bytes (pg_char_ptr chars, long PG_FAR *charsize, memory_ref buffer);


/* PaigeExportFilter constructor. This sets up the various members common to all filters.
No error checking or file verification occurs just yet. */

PaigeExportFilter::PaigeExportFilter ()
{
	pgFillBlock(&translator, sizeof(pg_translator), 0);
	bytes_exported = buffer_index = 0;
	buffer_ref = MEM_NULL;			// Zero buffer so we don't try to dispose yet
	unicode_ref = MEM_NULL;

	file_type = pg_text_type;	// Default type for this class
	file_version = 0x00010000;

	pgFillBlock((void PG_FAR *)file_kind, KIND_STR_SIZE * sizeof(pg_char), 0);
	pgBlockMove((void PG_FAR *)FILETYPE_TEXT, (void PG_FAR *)file_kind, FILETYPE_STR_SIZE * sizeof(pg_char));

	feature_bits = EXPORT_TEXT_FEATURE;
	export_bits = EXPORT_TEXT_FLAG;
	file_os = CURRENT_OS;
	max_buffer = MAX_TEXT_BUFFER;

	io_result = NO_ERROR;
	last_char_out = 0;
	last_buffer_char = 0;
	
	error_log_ref = cr_lf_ref = MEM_NULL;
}


/* PaigeExportFilter destructor. This removes common items. */

PaigeExportFilter::~PaigeExportFilter ()
{
	::pgDisposeTranslatorRec(&translator);
	
	if (buffer_ref)
		::DisposeFailedMemory(buffer_ref);
	if (unicode_ref)
		::DisposeFailedMemory(unicode_ref);
	if (error_log_ref)
		::DisposeFailedMemory(error_log_ref);
	if (cr_lf_ref)
		::DisposeFailedMemory(cr_lf_ref);
}


/* pgInitExportFile prepares the physical file to be exported. No data is transferred at this time. */

pg_error PaigeExportFilter:: pgInitExportFile (pg_globals_ptr globals, pg_file_unit fileref,
							memory_ref memory_unit, file_io_proc write_proc, long first_position)
{
	pg_error			result = NO_ERROR;

	paige_globals = globals;
	memory_file = memory_unit;
	filemap = (file_ref)fileref;
	
	if (!error_log_ref)
		error_log_ref = MemoryAlloc(paige_globals->mem_globals, sizeof(pg_errlog_rec), 0, 4);

	if (export_bits & INCLUDE_LF_WITH_CR)
		if (!cr_lf_ref)
			cr_lf_ref = MemoryAlloc(paige_globals->mem_globals, sizeof(pg_char), 0, 1024);

	if ((io_proc = write_proc) == NULL) {

		if (memory_unit) {
		
			io_proc = pgScrapMemoryWrite;
			filemap = (file_ref)memory_unit;
		}
		else
			io_proc = pgOSWriteProc;
	}

	file_begin = filepos = first_position;

	::pgInitTranslatorRec(globals, &translator);

	buffer_size = buffer_index = 0;
	total_out = 0;

#ifdef UNICODE
	if (!unicode_ref)
		unicode_ref = MemoryAlloc(paige_globals->mem_globals, 1, 0, 0);
#endif		

	if (feature_bits & EXPORT_SERIAL_SETUP) {
	
		buffer_size = max_buffer;
		
		if (!buffer_ref) {

			buffer_ref = ::MemoryAllocClear(globals->mem_globals, sizeof(pg_char), buffer_size, 0);
			io_buffer = (pg_char_ptr)::UseMemory(buffer_ref);
		}
		else
			pgFillBlock(io_buffer, buffer_size, 0);

		half_char_pending = 0;
	}

	return	result;
}


/* pgExportFile is the main entry point to export a file. The file that is exported is assumed
to be set up in this object. */

pg_error PaigeExportFilter::pgExportFile (pg_ref pg, select_pair_ptr range, long export_flags,
							pg_boolean selection_only)
{
	pg_error			result = NO_ERROR;
	pg_char_ptr			text, output_text;
	style_walk			walker;
	pg_boolean			export_styles, export_pars, export_embeds, export_links;
	embed_ref			embed;
	long				next_end;
	pg_short_t			last_style_item, last_par_item;
	pg_boolean			broke_on_embed, broke_on_link;
	short				last_font_index;

	export_pg = pg;
	export_pg_rec = (paige_rec_ptr)::UseMemory(pg);
	export_pg_rec->action_state = pg_state_export;

	export_bits = export_flags;
	total_out = 0;

	if (selection_only) {
		
		if (range)
			export_range = *range;
		else
			::pgGetSelection(pg, &export_range.begin, &export_range.end);
	}
	else {
		
		export_range.begin = 0;
		export_range.end = export_pg_rec->t_length;
	}
	
	translator.doc_info = export_pg_rec->doc_info;

	last_style_item = last_par_item = 0;
	last_font_index = translator.format.font_index;

	GetMemoryRecord(export_pg_rec->t_formats, 0, (void PG_FAR *)&translator.format);
	GetMemoryRecord(export_pg_rec->par_formats, 0, (void PG_FAR *)&translator.par_format);
	GetMemoryRecord(export_pg_rec->fonts, translator.format.font_index, (void PG_FAR *)&translator.font);

	if ((result = this->pgPrepareExport()) == NO_ERROR) {

		PG_TRY(export_pg_rec->globals->mem_globals) {

			::pgPrepareStyleWalk(export_pg_rec, 0, &walker, TRUE); // (Sets first style(s))
			previous_style = *(walker.cur_style);
			previous_par = *(walker.cur_par_style);
			
			export_styles = (pg_boolean)((feature_bits & EXPORT_TEXT_FORMATS_FLAG) != 0);
			export_pars = (pg_boolean)((feature_bits & EXPORT_PAR_FORMATS_FLAG) != 0);
			export_embeds = (pg_boolean)((feature_bits & EXPORT_EMBEDDED_OBJECTS_FLAG) != 0);
			export_links = (pg_boolean)((export_bits & EXPORT_HYPERTEXT_FLAG) != 0);

			block = ::pgFindTextBlock(export_pg_rec, export_range.begin, NULL, FALSE, TRUE);
			text = (pg_char_ptr)::UseMemory(block->text);
			
			for (;;) {

				::pgSetWalkStyle(&walker, export_range.begin);
				
				export_pg_rec->procs.wait_proc(export_pg_rec, save_wait, export_range.begin, export_range.end);
				
				translator.format_changed = (pg_boolean)(walker.prev_style_run->style_item != last_style_item);
				translator.par_format_changed = (pg_boolean)(walker.prev_par_run->style_item != last_par_item);
				translator.font_changed = (pg_boolean)(walker.cur_style->font_index != last_font_index);
				
				translator.hyperlink_target_changed = translator.hyperlink_changed = FALSE;
				
				if (export_styles) {
					
					if (translator.format_changed)
						::pgBlockMove(walker.cur_style, &translator.format, sizeof(style_info));
					if (translator.font_changed)
						::pgBlockMove(walker.cur_font, &translator.font, sizeof(font_info));
				}
				
				if (export_pars)
					if (translator.par_format_changed)
						::pgBlockMove(walker.cur_par_style, &translator.par_format, sizeof(par_info));

				broke_on_embed = broke_on_link = FALSE;
				
				next_end = export_range.end;
				
				if (block->end < next_end)
					next_end = block->end;
				
				if (export_embeds)
					embed = (embed_ref)walker.cur_style->embed_object;
				else
					embed = MEM_NULL;

				if (export_styles || embed)
					if (walker.next_style_run->offset < next_end)
						next_end = walker.next_style_run->offset;
				
				if (export_pars)
					if (walker.next_par_run->offset < next_end)
						next_end = walker.next_par_run->offset;
				
				if (export_links) {
					
					pgFillBlock(&translator.hyperlink, sizeof(pg_hyperlink), 0);
					pgFillBlock(&translator.hyperlink_target, sizeof(pg_hyperlink), 0);

					if (walker.hyperlink->applied_range.begin > export_range.begin) {

						if (walker.hyperlink->applied_range.begin < next_end) {
						
							next_end = walker.hyperlink->applied_range.begin;
							broke_on_link = TRUE;
						}
					}
					else
					if (walker.hyperlink->applied_range.begin == export_range.begin || walker.hyperlink->applied_range.end <= next_end) {

						translator.hyperlink_changed = TRUE;
						pgBlockMove(walker.hyperlink, &translator.hyperlink, sizeof(pg_hyperlink));
						
						if (walker.hyperlink->applied_range.end > export_range.begin) {
							
							if (walker.hyperlink->applied_range.end < next_end) {
							
								next_end = walker.hyperlink->applied_range.end;
								broke_on_link = TRUE;
							}
						}
					}
					
					if (walker.hyperlink_target->applied_range.begin > export_range.begin) {
						
						if (walker.hyperlink_target->applied_range.begin < next_end) {
						
							next_end = walker.hyperlink_target->applied_range.begin;
							broke_on_link = TRUE;
						}
					}
					else
					if (walker.hyperlink_target->applied_range.begin == export_range.begin || walker.hyperlink_target->applied_range.end <= next_end) {
						
						translator.hyperlink_target_changed = TRUE;
						pgBlockMove(walker.hyperlink_target, &translator.hyperlink_target, sizeof(pg_hyperlink));

						if (walker.hyperlink_target->applied_range.end > export_range.begin) {

							if (walker.hyperlink_target->applied_range.end < next_end) {

								next_end = walker.hyperlink_target->applied_range.end;
								broke_on_link = TRUE;
							}
						}
					}
				}

				translator.bytes_transferred = next_end - export_range.begin;
				::SetMemorySize(translator.data, translator.bytes_transferred);
				
				output_text = (pg_char_ptr)::UseMemory(translator.data);
				local_begin = export_range.begin - block->begin;
				pgBlockMove(&text[local_begin], output_text, translator.bytes_transferred);
				::UnuseMemory(translator.data);
				
			// Check possible embed_ref sitting in this position:
			
				if (embed) {

					broke_on_embed = TRUE;
					translator.data_type = export_embed_type;
					translator.data_param1 = (long)embed;
				}
				else
					translator.data_type = export_text_type;
				
				::pgSetWalkStyle(&walker, export_range.begin + translator.bytes_transferred);

				if ((feature_bits & EXPORT_STYLE2CR) && !translator.par_format.table.border_info
					&& !translator.par_format.table.table_columns
					&& !broke_on_embed && !broke_on_link) {	// Output consecutive CRs
					
					local_begin = next_end - block->begin;

					while (next_end < export_range.end) {
						
						if (walker.cur_par_style->table.border_info
							|| walker.cur_par_style->table.table_columns)
							break;

						if (next_end >= block->end) {
						
							::UnuseMemory(block->text);
							++block;
							export_pg_rec->procs.load_proc(export_pg_rec, block);
							text = (pg_char_ptr)::UseMemory(block->text);
							local_begin = 0;
						}
						
						if (text[local_begin] != 0x0D && text[local_begin] != 0x0A)
							break;
						
						output_text = (pg_char_ptr)::AppendMemory(translator.data, 1, FALSE);
						*output_text = 0x0D;
						UnuseMemory(translator.data);

						++local_begin;
						++next_end;

						pgWalkStyle(&walker, 1);
					}
				}

				next_style = walker.cur_style;
				next_par = walker.cur_par_style;
				next_font = walker.cur_font;

				if (!this->pgWriteNextBlock())
					break;

				::pgSetWalkStyle(&walker, export_range.begin);
				
				previous_style = (*walker.cur_style);
				previous_par = (*walker.cur_par_style);

				last_style_item = walker.prev_style_run->style_item;
				last_par_item = walker.prev_par_run->style_item;
				last_font_index = walker.cur_style->font_index;

				export_range.begin = next_end;
				
				if (export_range.begin >= export_range.end)
					break;
				
				if (block->end <= export_range.begin) {
					
					::UnuseMemory(block->text);
					
					++block;
					export_pg_rec->procs.load_proc(export_pg_rec, block);
					text = (pg_char_ptr)::UseMemory(block->text);
				}
			}
		}
		PG_CATCH {
			
		}
		
		PG_ENDTRY;
		
		export_pg_rec->procs.wait_proc(export_pg_rec, save_wait, export_range.end - 1, export_range.end);
		
		if (io_result == NO_ERROR)
			result = this->pgExportDone();
		
		if (!io_result)
			io_result = result;

		::UnuseMemory(block->text);
		::UnuseMemory(export_pg_rec->t_blocks);
		
		::pgPrepareStyleWalk(export_pg_rec, 0, NULL, TRUE);

		export_pg_rec->procs.wait_proc(export_pg_rec, save_wait, export_range.end, export_range.end);
	}
	
	export_pg_rec->action_state = pg_state_idle;

	::UnuseMemory(pg);

	return	io_result;
}


/* pgPrepareExport gets called just before first read. The default class does nothing.*/

pg_error PaigeExportFilter::pgPrepareExport (void)
{
	return	NO_ERROR;
}

/* InsertLFChars inserts LF chars after CR chars if that flag was set. */

long PaigeExportFilter::InsertLFChars (void)
{
	pg_char_ptr			input, output;
	long				input_size, output_size;

	SetMemorySize(cr_lf_ref, translator.bytes_transferred * 2);
	input = (pg_char_ptr)UseMemory(translator.data);
	output = (pg_char_ptr)UseMemory(cr_lf_ref);
	input_size = translator.bytes_transferred;
	output_size = 0;
	
	while (input_size) {
		
		if (last_buffer_char == 0x0D) {
			
			*output++ = 0x0A;
			++output_size;
		}
		
		last_buffer_char = *input++;
		*output++ = last_buffer_char;
		
		++output_size;
		--input_size;
	}
	
	UnuseMemory(translator.data);
	UnuseMemory(cr_lf_ref);
	
	return	output_size;
}

/* pgWriteNextBlock should write the data from the translator rec. If TRUE is returned, the
export process continues (this function gets called again if there is more data). Note that
some translators, such as PAIGE native exporter will simply call pgWriteDoc and return FALSE.

DEFAULT BEHAVIOUR:  This specific code just outputs raw text. */

pg_boolean PaigeExportFilter::pgWriteNextBlock (void)
{
	pg_char_ptr			text;
	pg_error			result = NO_ERROR;

	if (translator.bytes_transferred) {
		
		if (cr_lf_ref) {
			
			translator.bytes_transferred = InsertLFChars();
			text = (pg_char_ptr)::UseMemory(cr_lf_ref);
		}
		else
			text = (pg_char_ptr)::UseMemory(translator.data);

	#ifdef UNICODE
		
		if (export_bits & EXPORT_UNICODE_FLAG) {
			
			if (filepos == file_begin) {
				long		bom_size = sizeof(pg_char);
				pg_char		bom;
				
				bom = PG_BOM;

				result = io_proc((void PG_FAR *)&bom, io_data_direct, &filepos, &bom_size, filemap);
			}

			translator.bytes_transferred *= sizeof(pg_char);
			result = io_proc((void PG_FAR *)text, io_data_direct, &filepos,
					&translator.bytes_transferred, filemap);
			translator.bytes_transferred /= sizeof(pg_char);
		}
		else {
			pg_bits8_ptr		bytes;
			
			bytes = unicode_to_bytes(text, &translator.bytes_transferred, unicode_ref);
			result = io_proc((void PG_FAR *)bytes, io_data_direct, &filepos, &translator.bytes_transferred, filemap);
			UnuseMemory(unicode_ref);
		}
	#else
		result = io_proc((void PG_FAR *)text, io_data_direct, &filepos, &translator.bytes_transferred, filemap);
	#endif
		
		if (cr_lf_ref)
			::UnuseMemory(cr_lf_ref);
		else
			::UnuseMemory(translator.data);
	}

	return	(pg_boolean)(result == NO_ERROR);
}


/* pgExportDone gets called when transfer is complete. */

pg_error PaigeExportFilter::pgExportDone (void)
{
	return	io_proc(NULL, io_set_eof, &filepos, &filepos, filemap);
}


/* InsertErrorLog inserts a new error to the log. */

void PaigeExportFilter::InsertErrorLog (short error_code, long begin, long end, pg_char_ptr msg, pg_boolean no_dups)
{
	::pgInsertErrorLog(export_pg, error_log_ref, error_code, begin, end, error_refcon, msg, no_dups);
}


/* MapExportFont returns an alternate font name per font. Result is the output length. */

long PaigeExportFilter::MapExportFont (font_info_ptr font, pg_char_ptr output)
{
   register pg_char_ptr     table_ptr;
   short                	alternate_index, out_index;

   table_ptr = export_font_table;
   pgFillBlock(output, FONT_SIZE, 0);

   while (*table_ptr <= font->name[1]) {
      
      if ((alternate_index = pgCompareFontTable(&font->name[1], table_ptr)) != 0) {
         
         table_ptr += alternate_index;
          
         for (out_index = 0; out_index < (FONT_SIZE - 1); ++out_index) {

            if (table_ptr[out_index] == ']' || table_ptr[out_index] == 0)
               break;

            output[out_index] = table_ptr[out_index];
         }
         
         break;
      }
      
      for (;;) {
         
         if (*table_ptr++ == 0)
            break;
      }
   }
   
   return	pgCStrLength(output);
}


/* pgWriteByte places the_byte into the holding buffer and, if it is filled,
flushes the buffer to the file. */

void PaigeExportFilter::pgWriteByte (pg_bits8 the_byte)
{
#ifndef UNICODE
	io_buffer[buffer_index] = the_byte;

	if (the_byte >= ' ' && the_byte <= 'z')
		last_char_out = ascii_char_out;
	else
		last_char_out = delimiter_char_out;
#else
	pg_char			the_char;

	if (!half_char_pending) {
		
		half_char_pending = (pg_short_t)the_byte;
		half_char_pending <<= 8;
		half_char_pending |= 1;

		return;
	}
	
	the_char = half_char_pending & 0xFF00;
	the_char |= (pg_short_t)the_byte;

	io_buffer[buffer_index] = the_char;

	if (the_char >= ' ' && the_char <= 'z')
		last_char_out = ascii_char_out;
	else
		last_char_out = delimiter_char_out;

	half_char_pending = 0;
	
//falls through after endif --

#endif
	++buffer_index;
	++total_out;

	if (buffer_index == buffer_size)
		this->pgFlushOutputBuffer();
}

/* pgWriteChar sends a pg_char to the input buffer. Note, this is identical to pgWriteByte
if NOT Unicode. */

void PaigeExportFilter::pgWriteChar (pg_char the_char)
{
#ifndef UNICODE
	pgWriteByte(the_char);
#else
	pg_char			half_char;

	half_char = the_char >> 8;
	pgWriteByte((pg_bits8)half_char);
	pgWriteByte((pg_bits8)(the_char & 0x00FF));
#endif
}

/* pgWriteInt saves a 16 bit int. */

void PaigeExportFilter::pgWriteInt (pg_short_t the_int)
{
	pg_short_t			int_holder;
	
	int_holder = the_int >> 8;
	pgWriteByte((pg_bits8)int_holder);
	pgWriteByte((pg_bits8)the_int);
}

/* pgWriteInt saves a 16 bit int. */

void PaigeExportFilter::pgWrite3ByteInt (long the_int)
{
	pg_short_t			int_holder;
	
	int_holder = (pg_short_t)(the_int >> 16);
	pgWriteByte((pg_bits8)int_holder);
	int_holder = (pg_short_t)(the_int >> 8);
	pgWriteByte((pg_bits8)int_holder);
	int_holder = (pg_short_t)(the_int);
	pgWriteByte((pg_bits8)the_int);
}

/* pgWriteLong writes a portable long word. */

void PaigeExportFilter::pgWriteLong (long the_long)
{
	pg_short_t			int_holder;
	
	int_holder = (pg_short_t)pgHiWord(the_long);
	pgWriteInt(int_holder);
	int_holder = (pg_short_t)pgLoWord(the_long);
	pgWriteInt(int_holder);
}

/* pgWriteMemoryRef writes all the bytes from ref to the output. NOTE, THIS CAN FAIL IF THE
RECORD SIZE IS > 1. */

void PaigeExportFilter::pgWriteMemoryRef (memory_ref ref)
{
	pg_bits8_ptr		byte_ptr;
	long				size = GetByteSize(ref);
	
	byte_ptr = (pg_char_ptr)UseMemory(ref);
	
	while (size) {
		
		pgWriteByte(*byte_ptr++);
		--size;
	}

	UnuseMemory(ref);
}

/* pgWriteNBytes sends num_bytes chars to the holding buffer. */

void PaigeExportFilter::pgWriteNBytes (pg_char_ptr bytes, long num_bytes)
{
	register pg_char_ptr		output_ptr;
	long						output_ctr;
	
	output_ptr = bytes;
	
	for (output_ctr = num_bytes; output_ctr; --output_ctr)
		this->pgWriteChar(*output_ptr++);
}

/* pgWriteCString writes out the cstring specified. */

void PaigeExportFilter::pgWriteCString (pg_char_ptr the_str)
{
	pgWriteString(the_str, 0, 0);
	pgWriteChar(0);
}


/* pgWriteString writes the_str, preceded by prefix (if non zero) and terminated with
suffix (if non-zero). */

void PaigeExportFilter::pgWriteString (pg_char_ptr the_str, pg_char prefix, pg_char suffix)
{
#ifndef UNICODE
	pgWriteBytes(the_str, prefix, suffix);
#else
	short			index;
	
	if (prefix)
		this->pgWriteChar(prefix);

	index = 0;
	
	while (the_str[index]) {
		
		this->pgWriteChar(the_str[index]);
		++index;
	}

	if (suffix)
		this->pgWriteChar(suffix);
#endif
}

/* pgWriteBytes writes the_str, preceded by prefix (if non zero) and terminated with
suffix (if non-zero). */

void PaigeExportFilter::pgWriteBytes (pg_bits8_ptr the_str, pg_bits8 prefix, pg_bits8 suffix)
{
	short			index;
	
	if (prefix)
		this->pgWriteByte(prefix);

	index = 0;
	
	while (the_str[index]) {
		
		this->pgWriteByte(the_str[index]);
		++index;
	}

	if (suffix)
		this->pgWriteByte(suffix);
}


/* pgWriteDecimal sends decimal_value as ASCII decimal. */

void PaigeExportFilter::pgWriteDecimal (short decimal_value)
{
	pg_boolean			zero_suppress = TRUE;
	short				value;
	
	if ((value = decimal_value) == 0)
		pgWriteChar((pg_char)'0');
	else {
	
		if (value < 0) {
			
			pgWriteChar((pg_char)'-');
			value = -value;
		}
	
		pgOutputBDC(&value, 10000, &zero_suppress);
		pgOutputBDC(&value, 1000, &zero_suppress);
		pgOutputBDC(&value, 100, &zero_suppress);
		pgOutputBDC(&value, 10, &zero_suppress);
		pgWriteChar((pg_char)(value | 0x30));
	}
}

/* pgOutputBDC outputs a decimal (ASCII) char if leading zeros is not suppressed. */

void PaigeExportFilter::pgOutputBDC (short PG_FAR *value, short subtractor, pg_boolean PG_FAR *zero_suppress)
{
	pg_short_t		digit = (pg_short_t)'0';

	while (subtractor <= *value) {
		
		++digit;
		*value -= subtractor;
	}
	
	if (digit != (pg_short_t)'0')
		*zero_suppress = FALSE;
	
	if (*zero_suppress == FALSE)
		pgWriteChar((pg_char)(digit));
}


/* pgWriteHexByte writes the_byte as two-char hex. */

void PaigeExportFilter::pgWriteHexByte (pg_char the_byte)
{
	pg_char				nibble;

	nibble = the_byte & 0xF0;
	nibble >>= 4;
	
	if (nibble > 9)
		nibble += 0x27;
	
	pgWriteChar((pg_char)(nibble + 0x30));
	
	if ((nibble = the_byte & 0x0F) > 9)
		nibble += 0x27;

	pgWriteChar((pg_char)(nibble + 0x30));
}

/* pgSetSerialPos sets up the byte output stream to begin in one particular area. */

void PaigeExportFilter::pgSetSerialPos (long the_position)
{
	filepos = the_position;
	buffer_index = 0;
}


/* pgFlushOutputBuffer writes any remaining byte(s) to the file that are in the
holding buffer. */

void PaigeExportFilter::pgFlushOutputBuffer (void)
{
	if (buffer_index) {

#ifdef UNICODE
		long			real_bytecount;
		
		if (half_char_pending)
			pgWriteByte(0);

		if (export_bits & EXPORT_UNICODE_FLAG) {
			
			real_bytecount = buffer_index * sizeof(pg_char);
			io_result = io_proc((void PG_FAR *)io_buffer, io_data_direct, &filepos, &real_bytecount, filemap);
		}
		else {
			pg_bits8_ptr		bytes;
			
			real_bytecount = buffer_index;
			bytes = unicode_to_bytes(io_buffer, &real_bytecount, unicode_ref);
			io_result = io_proc((void PG_FAR *)bytes, io_data_direct, &filepos, &real_bytecount, filemap);
			UnuseMemory(unicode_ref);
		}
#else
		io_result = io_proc((void PG_FAR *)io_buffer, io_data_direct, &filepos, &buffer_index, filemap);
#endif
		buffer_index = 0;
		pgFailError(paige_globals->mem_globals, io_result);
	}
}


/* pgPrepareEmbedData is called to return the byte stream and byte count of an embed_ref.
If we do not know how to handle it then NULL is returned. */

pg_bits8_ptr PaigeExportFilter::pgPrepareEmbedData (embed_ref ref, long PG_FAR *byte_count,
			long PG_FAR *local_storage)
{
	pg_embed_ptr		embed_ptr;
	pg_bits8_ptr		result_ptr = (pg_bits8_ptr)NULL;

#ifdef MAC_PLATFORM
#pragma unused (local_storage)
#endif

	embed_ptr = (pg_embed_ptr)::UseMemory(ref);
	*byte_count = 0;

	if (embed_ptr->data) {
		
		switch (embed_ptr->type & EMBED_TYPE_MASK) {
			
			case embed_ole:
				*byte_count = GetMemorySize((embed_ref)embed_ptr->data);
				break;

			case embed_mac_pict:
			{
#ifdef MAC_PLATFORM
				Handle			the_pict;
				
				the_pict = (Handle)embed_ptr->data;
				HLock(the_pict);
				result_ptr = (pg_bits8_ptr)*the_pict;
				*byte_count = GetHandleSize(the_pict);
#else
				*byte_count = GetMemorySize((embed_ref)embed_ptr->data);
#endif
            }
				break;
				
			case embed_meta_file:
            {
 #ifdef WINDOWS_PLATFORM
				HANDLE			metabits;
				long			meta, metasize;
				void PG_FAR		*metaptr;

				meta = (long)embed_ptr->data;				

#ifndef WIN16_COMPILE
				
				if (metasize = GetMetaFileBitsEx((HMETAFILE)meta, 0, NULL)) {
					
					*byte_count = metasize;
					metabits = GlobalAlloc(GMEM_MOVEABLE, metasize);
					metaptr = GlobalLock(metabits);
					result_ptr = (pg_bits8_ptr)metaptr;
					*local_storage = (long)metabits;
					GetMetaFileBitsEx((HMETAFILE)meta, metasize, (LPVOID)metaptr);
				}
#else		
				metabits = (HANDLE)GetMetaFileBits((HMETAFILE)meta);
				metasize = GlobalSize(metabits);
				metaptr = GlobalLock(metabits);
				
				*local_storage = PG_LONGWORD(long)metabits;
				*byte_count = metasize;
				result_ptr = (pg_bits8_ptr)metaptr;
#endif
#else
				*byte_count = GetMemorySize((embed_ref)embed_ptr->data);
#endif
				break;
		   }
		   
		   case embed_unsupported_object:
		   		*byte_count = GetMemorySize((embed_ref)embed_ptr->data);
		   		break;
		}

		if (!result_ptr && *byte_count)
			result_ptr = (pg_bits8_ptr)UseMemory((memory_ref)embed_ptr->data);
	}

	UnuseMemory(ref);
	                    
	return		result_ptr;   
}

/* pgReleaseEmbedData gets called to balance pgPrepareEmbedData. */

void PaigeExportFilter::pgReleaseEmbedData (embed_ref ref, long local_storage)
{
	pg_embed_ptr			embed_ptr;

#ifdef MAC_PLATFORM
#pragma unused (local_storage)
#endif

	embed_ptr = (pg_embed_ptr)UseMemory(ref);
	
	if (embed_ptr->data) {
	
		switch (embed_ptr->type & EMBED_TYPE_MASK) {

			case embed_ole:
				UnuseMemory((memory_ref)embed_ptr->data);
				break;

			case embed_mac_pict:
	
	#ifdef MAC_PLATFORM
				HUnlock((Handle)embed_ptr->data);
	#else
				UnuseMemory((memory_ref)embed_ptr->data);
	#endif
				break;
				
			case embed_meta_file:
	#ifdef WINDOWS_PLATFORM
	
	#ifndef WIN16_COMPILE
				GlobalUnlock((HGLOBAL)local_storage);
				GlobalFree((HGLOBAL)local_storage);
	#else

				GlobalUnlock((HMETAFILE)local_storage);
				embed_ptr->data = (void PG_FAR *)SetMetaFileBits((HMETAFILE)local_storage);
	#endif
	#else
				UnuseMemory((memory_ref)embed_ptr->data);
	#endif
				break;
		}
	}
	
	UnuseMemory(ref);
}


/*********************************** Non-member Functions ************************************/


/* pgExportFileFromC is a way to create an export object and send the file from straight C. */

PG_PASCAL (pg_error) pgExportFileFromC (pg_ref pg, pg_filetype filetype, long feature_flags,
		long file_begin, select_pair_ptr output_range, pg_boolean use_selection, pg_file_unit f_ref)
{
	PaigeExportObject			filter;
	pg_globals_ptr				globals;
	long						flags;
	pg_error					result = NO_ERROR;
	
	if (!(flags = feature_flags))
		flags = EXPORT_EVERYTHING_FLAG;

#ifdef WINDOWS_PLATFORM
	flags |= INCLUDE_LF_WITH_CR;
#endif

	globals = pgGetGlobals(pg);
	
	switch (filetype) {
		
		case pg_text_type:
			filter = new PaigeExportFilter();
			break;

		case pg_rtf_type:
			filter = (PaigeExportObject) new PaigeRTFExportFilter();
			break;

		case pg_html_type:
			filter = (PaigeExportObject) new PaigeHTMLExportFilter();
			break;

		case pg_paige_type:
			filter = (PaigeExportObject) new PaigeNativeExportFilter();
			break;

		default:
			return	(pg_error)BAD_TYPE_ERR;
	}

	if ((result = filter->pgInitExportFile(globals, f_ref, MEM_NULL, NULL, file_begin)) == NO_ERROR)
		result = filter->pgExportFile(pg, output_range, flags, use_selection);

	delete filter;

	return		result;
}


/******************************  Local Functions ***************************/

/* unicode_to_bytes converts some known 16-bit chars to 8-bit bytes. The bytesize is returned
in *charsize.  The memory_ref is USED upon exit. */

static pg_bits8_ptr unicode_to_bytes (pg_char_ptr chars, long PG_FAR *charsize, memory_ref buffer)
{
	pg_bits8_ptr		bytes;
	
	SetMemorySize(buffer, *charsize * sizeof(pg_char));
	bytes = (pg_bits8_ptr)UseMemory(buffer);
	pgBlockMove(chars, bytes, *charsize * sizeof(pg_char));
	*charsize = pgUnicodeToBytes((pg_short_t PG_FAR *)bytes, bytes, NULL, *charsize);

	return		bytes;
}

