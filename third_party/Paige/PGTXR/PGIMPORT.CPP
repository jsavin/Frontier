/* Import-Export module. Copyright 1995-96 by DataPak Software, Inc.  This software is a
part of the total PAIGE library.

This source file contains all the default member and non-member functions for the import class. */


#include "Paige.h"
#include "machine.h"
#include "pgOSUtl.h"
#include "pgTraps.h"
#include "pgTxrCPP.h"
#include "pgExceps.h"
#include "defprocs.h"
#include "pgDefStl.h"
#include "pgSelect.h"
#include "pgErrors.h"
#include "pgUtils.h"
#include "pgdeftbl.h"
#include "pgHText.h"
#include "pgFrame.h"

#define TEST_UNICODE_FLAG     0x0001

static pg_error read_buffer (pg_char_ptr buffer, long PG_FAR *bytesize, long PG_FAR *position,
      long max_position, file_io_proc io_proc, file_ref filemap, pg_short_t PG_FAR *is_unicode);
static pg_boolean find_font_in_list (paige_rec_ptr pg, font_info_ptr font);


/* PaigeImportFilter constructor. This sets up the various members common to all filters.
No error checking or file verification occurs just yet. */

PaigeImportFilter::PaigeImportFilter ()
{
   pgFillBlock(&translator, sizeof(pg_translator), 0);
   bytes_imported = bytes_read = 0;
   last_physical_read = temp_bytes_read = 0;

   buffer_ref = MEM_NULL;        // Zero buffer so we don't try to dispose yet
   parallel_buffer = MEM_NULL;
   original_pg = MEM_NULL;
   error_log = MEM_NULL;
   
   file_type = pg_text_type;  // Default type for this class
   feature_bits = IMPORT_TEXT_FEATURE | IMPORT_CACHE_CAPABILITY;
   import_bits = IMPORT_TEXT_FLAG;
   file_os = CURRENT_OS;
   max_buffer = MAX_TEXT_BUFFER;
   max_output_buffer = TRANSLATOR_BUFFER_SIZE;
   
   font_cross_table = cross_font_table;
   character_table = cross_character_table;
   
   unicode_flag = 0;
   target_link_style = index_link_style = toc_link_style = 0;

   last_char_out = last_char2_out = last_char_in = this_char_in = 0;
   last_par_applied = 0;
   io_result = NO_ERROR;

   temp_buffer_ref = MEM_NULL;
   temp_buffer_ptr = NULL;
   temp_char_count = 0;
   last_buffer_char = 0;
}


/* PaigeImportFilter destructor. This removes common items. */

PaigeImportFilter::~PaigeImportFilter ()
{
   ::pgDisposeTranslatorRec(&translator);
   ::DisposeNonNilMemory(parallel_buffer);

   if (buffer_ref)
      ::UnuseAndDispose(buffer_ref);
   if (error_log)
      DisposeMemory(error_log);
}

/* pgInitImportFile prepares the physical file params to be imported. No data is transferred until the
first pgReadNextBlock function call. */

pg_error PaigeImportFilter:: pgInitImportFile (pg_globals_ptr globals, pg_file_unit fileref, memory_ref memory_unit,
               file_io_proc read_proc, long first_position, long last_position)
{
   pg_error       result = NO_ERROR;

   paige_globals = globals;
   memory_file = memory_unit;
   filemap = (file_ref)fileref;

   if ((io_proc = read_proc) == NULL) {

      if (memory_unit) {
      
         io_proc = pgScrapMemoryRead;
         filemap = (file_ref)memory_unit;
      }
      else
         io_proc = pgOSReadProc;
   }
   else
   if (!filemap)
      filemap = (file_ref)memory_unit;

   import_pg = MEM_NULL;
   text_position = 0;
   bytes_imported = 0;

   file_begin = filepos = first_position;
   
   if ((file_eof = last_position) == UNKNOWN_POSITION) {
   
      result = io_proc((void PG_FAR *)&file_eof, io_get_eof, &filepos, &filepos, filemap);
      filepos = file_begin;
   }
   
   ::pgInitTranslatorRec(globals, &translator);

   buffer_size = 0;
   buffer_ref = MEM_NULL;
   io_buffer = NULL;

   if (feature_bits & IMPORT_SERIAL_SETUP) {

      buffer_size = max_buffer;
      buffer_ref = ::MemoryAllocClear(globals->mem_globals, sizeof(pg_char), buffer_size, 0);
      io_buffer = (pg_char_ptr)::UseMemory(buffer_ref);
      logical_pos = 0;
      unicode_flag = TEST_UNICODE_FLAG;
      result = read_buffer(io_buffer, &buffer_size, &filepos, file_eof, io_proc, filemap, &unicode_flag);
      buffer_index = 0;
      half_char_pending = 0;
   }
   
   error_log = ::MemoryAlloc(globals->mem_globals, sizeof(pg_errlog_rec), 0, 2);

   return   result;
}


/* pgImportFile is the main entry point to import a file. The file that is imported is assumed
to be set up in this object. */

pg_error PaigeImportFilter::pgImportFile (pg_ref pg, long pg_position, long import_flags,
         pg_boolean keep_selection, short draw_mode)
{
   paige_rec_ptr     		pg_rec;
   volatile pgm_globals_ptr	 mem_globals;
   pg_boolean        		data_transferred;
   short          	 		use_draw_mode = draw_mode;
   pg_error       	 		result = NO_ERROR;

   import_pg = pg;
   pg_rec = (paige_rec_ptr)UseMemory(pg);
   mem_globals = pg_rec->globals->mem_globals;
   
   if (pgTextSize(pg) != 0 && pgNumSelections(pg) == 0) {
      
      original_pg = pg;
      import_pg = ::pgNewShell(paige_globals);
      use_draw_mode = draw_none;
   }
   
   import_pg_rec = (paige_rec_ptr)::UseMemory(import_pg);
   
   if (original_pg)
   		original_pg_rec = (paige_rec_ptr)::UseMemory(original_pg);
   else
   		original_pg_rec = import_pg_rec;

   import_bits = import_flags;
   import_pg_rec->action_state = pg_state_import;
   last_par_applied = GetMemorySize(import_pg_rec->par_formats);

   is_cached = (pg_boolean)((import_bits & IMPORT_CACHE_FLAG) != 0 && (feature_bits & IMPORT_CACHE_CAPABILITY) != 0);
   
   if (is_cached) {
      
      import_pg_rec->cache_file = filemap;
      import_pg_rec->cache_read_proc = io_proc;
   }

   import_pg_rec->cross_table[0] = character_table;
   import_pg_rec->cross_table[1] = cross_symbolfont_table;

   text_position = ::pgFixOffset(import_pg_rec, pg_position);

   ::pgTurnOffHighlight(pg_rec, TRUE);

   if (!target_link_style)
      target_link_style = pgNewHyperlinkStyle(pg, 0xFE00, 0x9D00, 0xFE00, 0, TRUE);
   if (!index_link_style)
      index_link_style = pgNewHyperlinkStyle(pg, 0xFE00, 0x9D00, 0, 0, TRUE);
   if (!toc_link_style)
      toc_link_style = pgNewHyperlinkStyle(pg, 0x9D00, 0xFE00, 0xFE00, 0, TRUE);

   if (!translator.hyperlink.state1_style)
      translator.hyperlink.state1_style = pgNewHyperlinkStyle(pg, 0, 0, STYLE_COLOR_INTENSITY, X_UNDERLINE_BIT, FALSE);
   if (!translator.hyperlink.state2_style)
      translator.hyperlink.state2_style = pgNewHyperlinkStyle(pg, STYLE_COLOR_INTENSITY, 0, 0, X_UNDERLINE_BIT, FALSE);
   if (!translator.hyperlink.state3_style)
      translator.hyperlink.state3_style = pgNewHyperlinkStyle(pg, STYLE_COLOR_INTENSITY, 0, STYLE_COLOR_INTENSITY, X_UNDERLINE_BIT, FALSE);

   PG_TRY(mem_globals) {
   
	   if ((result = this->pgPrepareImport()) == NO_ERROR) {
	      
	      if (pg_rec->num_selects)
	         ::pgDelete(import_pg, NULL, draw_none);
	      
	      ::pgSetSelection(import_pg, text_position, text_position, 0, FALSE);
	      
	      pgBeginImport(import_pg, text_position);

	      output_ptr = (pg_char_ptr)UseMemory(translator.data);
	      
	      do {
	         
	         data_transferred = this->pgReadNextBlock();
	         ProcessImportData();

	      } while (data_transferred);

	// Change document info, if appropriate:

	      if (translator.doc_info_changed && (import_bits & IMPORT_PAGE_INFO_FLAG)) {
	         pg_doc_info    doc_info;
	         rectangle      page_bounds;
	         
	         pgGetDocInfo(import_pg, &doc_info);
	         doc_info.margins = translator.doc_info.margins;
	         doc_info.print_target = translator.doc_info.print_target;
	         pgSetDocInfo(import_pg, &doc_info, FALSE, draw_none);
	         
	         if (import_bits & APPLY_PAGE_DIMENSIONS) {
	            
	            if (!pgEmptyRect(&doc_info.print_target)) {
	               
	               page_bounds = doc_info.print_target;
	               page_bounds.top_left.h += doc_info.margins.top_left.h;
	               page_bounds.top_left.v += doc_info.margins.top_left.v;
	               page_bounds.bot_right.h -= doc_info.margins.bot_right.h;
	               page_bounds.bot_right.v -= doc_info.margins.bot_right.v;

	               pgSetAreaBounds(import_pg, &page_bounds, NULL);
	            }
	         }
	      }
	
	      if (is_cached) {
	         text_block_ptr       block;
	         
	         block = (text_block_ptr)UseMemory(import_pg_rec->t_blocks);
	         DisposeNonNilMemory(block->text);
	         block->text = MEM_NULL;
	         UnuseMemory(import_pg_rec->t_blocks);
	      }

	      this->pgImportDone();
	      result = io_result;
	      pg_rec->procs.wait_proc(pg_rec, open_wait, file_eof - 1, file_eof);
	      
	      UnuseMemory(translator.data);

	      ::pgEndImport(import_pg, keep_selection, use_draw_mode);
	      this->pgMapChars(import_pg);
	      this->pgImportCleanup();
	   }

	   pg_rec->procs.wait_proc(pg_rec, open_wait, file_eof - 1, file_eof);
	   
	   if (import_bits & IMPORT_MERGED_STYLES_FLAG)
	   	    pgMatchParStyles(pg_rec, file_eof - 1, file_eof);

	   pg_rec->procs.wait_proc(pg_rec, open_wait, file_eof - 1, file_eof);

	   import_pg_rec->action_state = pg_state_idle;

	   ::UnuseMemory(import_pg);
	   ::UnuseMemory(pg);

	   if (original_pg) {
		  
		  UnuseMemory(original_pg);
	      pgPaste(original_pg, import_pg, CURRENT_POSITION, FALSE, draw_mode);
	      pgDispose(import_pg);
	      import_pg = original_pg;
	      original_pg = MEM_NULL;
	   }
	}
	PG_CATCH {

		UnuseFailedMemory(import_pg);
		
		if (original_pg)
			pgFailureDispose(original_pg);
		
		pgFailure(mem_globals, NO_MEMORY_ERR, 0);
	}
	PG_ENDTRY;

   if (result == EOF_ERR)
      return   NO_ERROR;

   return      result;
}

/* ProcessImportData is the code that processes all the data obtained from pgReadNextBlock().
This function is separate to allow external import filters to call this to "dump" pending
text without returning from pgReadNextBlock(). */

void PaigeImportFilter::ProcessImportData (void)
{

   style_info_ptr    style_ptr;
   par_info_ptr      par_ptr;
   font_info_ptr     font_ptr;
   font_info         inserted_font;
   pg_import_ptr     import_ptr;
   pg_hyperlink_ptr  hyperlink;
   long           	 bytes_to_insert;

   if (translator.hyperlink_changed) {
	  
	  import_ptr = (pg_import_ptr)UseMemory(import_pg_rec->import_control);

      translator.hyperlink.unique_id = pgAssignLinkID(import_ptr->hyperlinks);
      hyperlink = (pg_hyperlink_ptr)AppendMemory(import_ptr->hyperlinks, 1, FALSE);
      *hyperlink = translator.hyperlink;
      
      if (hyperlink->applied_range.end <= hyperlink->applied_range.begin) {
      
         hyperlink->applied_range.begin = hyperlink->applied_range.end = text_position;
         hyperlink->applied_range.end += translator.bytes_transferred;
      }

      UnuseMemory(import_ptr->hyperlinks);
      UnuseMemory(import_pg_rec->import_control);

      translator.hyperlink_changed = FALSE;
   }

   import_ptr = (pg_import_ptr)UseMemory(import_pg_rec->import_control);

   if (translator.hyperlink_target_changed) {
      
      translator.hyperlink_target.unique_id = pgAssignLinkID(import_ptr->hyperlinks_target);
      hyperlink = (pg_hyperlink_ptr)AppendMemory(import_ptr->hyperlinks_target, 1, FALSE);
      *hyperlink = translator.hyperlink_target;
      
      if (hyperlink->applied_range.end <= hyperlink->applied_range.begin) {

         hyperlink->applied_range.begin = hyperlink->applied_range.end = text_position;
         hyperlink->applied_range.end += translator.bytes_transferred;
      }
      
      UnuseMemory(import_ptr->hyperlinks_target);
      translator.hyperlink_target_changed = FALSE;
   }

   import_ptr->par_format_mark = translator.par_format_mark;
   import_ptr->par_format_verb = translator.par_format_verb;

   UnuseMemory(import_pg_rec->import_control);

   output_ptr = (pg_char_ptr)UseMemoryRecord(translator.data, 0, 0, FALSE);

   if (translator.bytes_transferred) {
      
      if (!bytes_imported)
         translator.format_changed = translator.font_changed = TRUE;

      if (translator.format_changed && (import_bits & IMPORT_TEXT_FORMATS_FLAG))
         style_ptr = &translator.format;
      else
         style_ptr = NULL;

      if (!(feature_bits & IMPORT_PARS_EXTERNAL) && translator.par_format_changed && (import_bits & IMPORT_PAR_FORMATS_FLAG))
         par_ptr = &translator.par_format;
      else
         par_ptr = NULL;
      
      translator.par_format_changed = FALSE;
      
      if (translator.font_changed && (import_bits & IMPORT_TEXT_FORMATS_FLAG)) {
         
         inserted_font = translator.font;
         font_ptr = &inserted_font;
         
         if (!FindFontInList(font_ptr))
            this->pgMapFont(font_ptr, file_os, CURRENT_OS);
      }
      else
         font_ptr = NULL;

      if (import_bits & IMPORT_TEXT_FLAG)
         bytes_to_insert = translator.bytes_transferred;
      else
         bytes_to_insert = 0;
      
      import_ptr = (pg_import_ptr)UseMemory(import_pg_rec->import_control);
      import_ptr->file_os = file_os;

      if (is_cached)
         import_ptr->cache_pos = translator.cache_begin;
         
      UnuseMemory(import_pg_rec->import_control);
      
      pgInsertText(import_pg, output_ptr, bytes_to_insert, font_ptr, style_ptr, par_ptr, draw_none);
      
      translator.hyperlink_changed = translator.hyperlink_target_changed = FALSE;

      text_position += translator.bytes_transferred;
      bytes_imported += translator.bytes_transferred;
      translator.bytes_transferred = 0;
   }
   
   CallWait();
}

/* InsertFrame inserts a new frame into the (eventual) pg_ref. */

void PaigeImportFilter::InsertFrame (pg_frame_ptr frame)
{
	pg_import_ptr     import_ptr;
	pg_frame_ptr	  frame_ptr;
	
	import_ptr = (pg_import_ptr)UseMemory(import_pg_rec->import_control);
	frame_ptr = (pg_frame_ptr)AppendMemory(import_ptr->frames, 1, FALSE);
	*frame_ptr = *frame;
	UnuseMemory(import_ptr->frames);
	UnuseMemory(import_pg_rec->import_control);
}

void PaigeImportFilter::CallWait(void)
{
   if (temp_buffer_ref)
      import_pg_rec->procs.wait_proc(import_pg_rec, open_wait, last_physical_read + temp_bytes_read, file_eof);
   else
   if (parallel_buffer)
      import_pg_rec->procs.wait_proc(import_pg_rec, open_wait, last_physical_read, file_eof);
   else
      import_pg_rec->procs.wait_proc(import_pg_rec, open_wait, file_begin + bytes_read, file_eof);
}


/* ApplyInputParagraph applies the paragraph format to the current insertion point in the
import record. If the last insert is not a carriage return then one is inserted (so the
par format applies to the exact, current position).  If unconditional_cr is TRUE then
a CR is inserted no matter what. THIS FUNCTION SHOULD ONLY BE CALLED WHEN IMPORTING
THROUGH THE IMPORT MECHANISM.
If applied_offset >= 0 then NO CR IS INSERTED and the paragraph is applied to applied_offset. */

void PaigeImportFilter::ApplyInputParagraph (par_info_ptr par, long applied_offset, pg_boolean unconditional_cr)
{
   pg_import_ptr     import_ptr;
   par_info_ptr      par_access;
   pg_char_ptr       text;
   text_block_ptr    block;
   style_run         run_rec;
   style_run_ptr     run;
   pg_short_t        style_index;
   long           	 num_blocks, last_block_element, num_runs, use_offset;
   pg_char           ending_char;

   import_ptr = (pg_import_ptr)UseMemory(import_pg_rec->import_control);
   
   if ((use_offset = applied_offset) < 0) {

	   num_blocks = GetMemorySize(import_ptr->t_blocks);
	   last_block_element = num_blocks - 1;
	   block = (text_block_ptr)UseMemoryRecord(import_ptr->t_blocks, last_block_element, USE_ALL_RECS, TRUE);
	   
	   if (block->begin)
	      if (block->begin == block->end) {
	      
	         --block;
	         --last_block_element;
	   }
	   
	   if (unconditional_cr)
	      ending_char = 0;
	   else
	   if (!import_ptr->t_length)
	      ending_char = 0x0D;
	   else
	      GetMemoryRecord(block->text, (block->end - block->begin) - 1, &ending_char);
	   
	   if (ending_char != 0x0D) {
	      
	      text = (pg_char_ptr)AppendMemory(block->text, 1, FALSE);
	      *text = 0x0D;
	      UnuseMemory(block->text);
	      
	      ++block->end;
	      ++import_ptr->t_length;
	      ++text_position;
	      ++bytes_imported;
	      
	      last_char2_out = last_char_out;
	      last_char_out = 0x0D;
	      
	      while (last_block_element < (num_blocks - 1)) {
	           
	           ++block;
	           block->begin += 1;
	           block->end += 1;
	           ++last_block_element;
	      }
	   }

	   UnuseMemory(import_ptr->t_blocks);
	   
	   use_offset = import_ptr->t_length;
   }
   
   num_runs = GetMemorySize(import_ptr->par_style_run);
   
   if (!num_runs) {
      
      AppendMemory(import_ptr->par_style_run, 1, TRUE);
      UnuseMemory(import_ptr->par_style_run);
      num_runs = 1;
   }
   
   GetMemorySize(import_pg_rec->par_formats);
   style_index = pgAddParInfo(import_pg_rec, NULL, for_next_insert_reason, par);
   last_par_applied = (long)style_index;

   GetMemoryRecord(import_ptr->par_style_run, num_runs - 1, &run_rec);
   
   if (style_index != import_ptr->last_par_item) {
      
      if (run_rec.offset == use_offset)
         run = (style_run_ptr)UseMemoryRecord(import_ptr->par_style_run, num_runs - 1, 0, TRUE);
      else
         run =(style_run_ptr)AppendMemory(import_ptr->par_style_run, 1, FALSE);

      run->offset = use_offset;
      run->style_item = style_index;
      UnuseMemory(import_ptr->par_style_run);

      par_access = (par_info_ptr)UseMemory(import_pg_rec->par_formats);
      par_access[style_index].used_ctr += 1;
      UnuseMemory(import_pg_rec->par_formats);
   }

   import_ptr->last_par_item = style_index;

   UnuseMemory(import_pg_rec->import_control);
   
   par->class_info &= (~BULLETED_LINE);
}

/* InsertErrorLog inserts a new error to the log. */

void PaigeImportFilter::InsertErrorLog (short error_code, long begin, long end, pg_char_ptr msg, pg_boolean no_dups)
{
	pg_ref			pg;

	if (!(pg = original_pg))
		pg = import_pg;
	
	::pgInsertErrorLog(pg, error_log, error_code, begin, end, 0, msg, no_dups);
}


/* pgGetImportByte returns the next byte in the read buffer. This is used for "serial" formats
such as ASCII text and RTF. */

pg_error PaigeImportFilter::pgGetImportByte (pg_bits8_ptr the_byte)
{
   pg_error       result = NO_ERROR;

   if (buffer_index == buffer_size) {
      
      *the_byte = 0;
      result = EOF_ERR;
   }
   else {

#ifndef UNICODE
      *the_byte = io_buffer[buffer_index];
      buffer_index += 1;
      logical_pos += 1;

      if (parallel_buffer) {
         pg_char_ptr    next_ptr;
         
         next_ptr = (pg_char_ptr)AppendMemory(parallel_buffer, 1, FALSE);
         *next_ptr = *the_byte;
         UnuseMemory(parallel_buffer);
      }

#else
      if ((half_char_pending & 1) == 0)
         *the_byte = (pg_bits8)(io_buffer[buffer_index] >> 8);
      else {

         *the_byte = (pg_bits8)(io_buffer[buffer_index] & 0x00FF);

         if (parallel_buffer) {
            pg_char_ptr    next_ptr;
            
            next_ptr = (pg_char_ptr)AppendMemory(parallel_buffer, 1, FALSE);
            *next_ptr = io_buffer[buffer_index];
            UnuseMemory(parallel_buffer);
         }

         buffer_index += 1;
         logical_pos += 1;
      }

      ++half_char_pending;
#endif
      if (buffer_index == buffer_size)
         if (filepos < file_eof) {

         buffer_size = max_buffer;
         result = read_buffer(io_buffer, &buffer_size, &filepos, file_eof, io_proc, filemap, &unicode_flag);
         buffer_index = 0;
         
         if (result == EOF_ERR)
            result = NO_ERROR;
      }
   }
   
   last_char_in = this_char_in;
   this_char_in = (pg_short_t)*the_byte;
   return   result;
}


/* pgGetImportChar gets the next logical character (Unicode and non-Unicode). */

pg_error PaigeImportFilter::pgGetImportChar (pg_char_ptr the_char)
{
   pg_error       result = NO_ERROR;
   
   if (temp_buffer_ptr) {
      
      *the_char = *temp_buffer_ptr++;
      ++temp_bytes_read;
      --temp_char_count;
      
      if (!temp_char_count) {
         
         UnuseAndDispose(temp_buffer_ref);
         temp_buffer_ref = MEM_NULL;
         temp_buffer_ptr = NULL;
      }
      
      last_char_in = this_char_in;
      this_char_in = (pg_short_t)*the_char;

      return   result;
   }

   if (buffer_index == buffer_size) {
      
      *the_char = 0;
      result = EOF_ERR;
   }
   else {

      *the_char = io_buffer[buffer_index];
      buffer_index += 1;
      logical_pos += sizeof(pg_char);

      if (parallel_buffer) {
         pg_char_ptr    next_ptr;
         
         next_ptr = (pg_char_ptr)AppendMemory(parallel_buffer, 1, FALSE);
         *next_ptr = *the_char;
         UnuseMemory(parallel_buffer);
      }

      if (buffer_index == buffer_size)
         if (filepos < file_eof) {

         buffer_size = max_buffer;
         result = read_buffer(io_buffer, &buffer_size, &filepos, file_eof, io_proc, filemap, &unicode_flag);
         buffer_index = 0;
         
         if (result == EOF_ERR)
            result = NO_ERROR;
      }
   }
   
   last_char_in = this_char_in;
   this_char_in = (pg_short_t)*the_char;

   return   result;
}

/* pgNextImportChar returns the next character without removing it from the buffer. */

pg_char PaigeImportFilter::pgNextImportChar (void)
{
	pg_char		result;

   if (temp_buffer_ptr)
   	   result = *temp_buffer_ptr;
   else
   if (buffer_index == buffer_size)
      result = 0;
   else
      result = io_buffer[buffer_index];

   return	result;
}


/* pgGetImportString returns a cstring from the input string. Caller must realize the max
potential of size because we do not check this. */

pg_error PaigeImportFilter::pgGetImportString (pg_char_ptr the_str)
{
   pg_char_ptr       str_ptr;
   pg_error       result;

   str_ptr = the_str;

   for (;;) {
      
      result = pgGetImportChar(str_ptr);
      
      if (*str_ptr == 0)
         break;
      
      ++str_ptr;
   }
   
   return      result;
}


/* pgGetImportInt returns a 16 bit short based on the next two bytes. */

pg_error PaigeImportFilter::pgGetImportInt (pg_short_t PG_FAR *the_int)
{
   pg_error       result = NO_ERROR;
   pg_bits8       hold_byte;
   pg_short_t        hold_int = 0;
   
   result = pgGetImportByte(&hold_byte);
   
   if (!result) {
      
      hold_int = (pg_short_t) hold_byte;
      hold_int <<= 8;
      result = pgGetImportByte(&hold_byte);
      hold_int |= (pg_short_t)hold_byte;
   }
   
   *the_int = hold_int;

   return   result;
}

/* pgGetImport3ByteInt gets a 3-byte long value. */

pg_error PaigeImportFilter::pgGetImport3ByteInt (long PG_FAR *the_int)
{
   pg_bits8       data;
   long           long_holder;
   pg_error       error;
   pg_short_t        int_holder;

   if ((error = pgGetImportByte(&data)) != 0)
      return   error;

   int_holder = data;
   long_holder = int_holder;
   *the_int = long_holder << 16;

   if ((error = pgGetImportByte(&data)) != 0)
      return   error;

   int_holder = data;
   long_holder = int_holder;
   *the_int |= long_holder << 8;

   if ((error = pgGetImportByte(&data)) != 0)
      return   error;

   int_holder = data;
   long_holder = int_holder;
   *the_int |= long_holder;
   
   return   NO_ERROR;
}


pg_error PaigeImportFilter::pgGetImportLong (long PG_FAR *the_long)
{
   pg_error       result = NO_ERROR;
   long           hold_long;
   pg_short_t        hold_int;
   
   result = pgGetImportInt(&hold_int);

   if (!result) {
      
      hold_long = (long) hold_int;
      hold_long <<= 16;
      result = pgGetImportInt(&hold_int);
      hold_long |= (long)hold_int;
   }
   
   *the_long = hold_long;

   return   result;
}

/* pgGetImportMemoryRef imports a memory_ref of bytes_size long. The memory_ref
MUST be byte-oriented. */

pg_error PaigeImportFilter::pgGetImportMemoryRef (memory_ref ref, long byte_size)
{
   register pg_bits8_ptr      byte_ptr;
   long                 byte_ctr;
   pg_error             result = NO_ERROR;
   
   SetMemorySize(ref, byte_size);
   byte_ptr = (pg_bits8_ptr)UseMemory(ref);

   for (byte_ctr = 0; byte_ctr < byte_size; ++byte_ctr, ++byte_ptr)
      if ((result = pgGetImportByte(byte_ptr)) != NO_ERROR)
         break;

   UnuseMemory(ref);

   return   result;
}


/* pgGetImportNBytes is the same as pgGetImportByte except multiple bytes are read. */

pg_error PaigeImportFilter::pgGetImportNBytes (long PG_FAR *num_bytes, pg_bits8_ptr bytes)
{
   register pg_char_ptr    output;
   register long           bytecount;
   pg_error             result = NO_ERROR;
   
   output = bytes;
   
   for (bytecount = 0; bytecount < *num_bytes; ++bytecount) {
   
      if ( (result = pgGetImportByte(output)) != NO_ERROR )
         break;

      ++output;
   }

   *num_bytes = bytecount;

   return      result;
}

/* pgVerifySignature checks the existing file to verify that it is the appropriate type.
If not, BAD_TYPE is returned as an error. */

pg_error PaigeImportFilter::pgVerifySignature (void)
{
   return   0;
}

pg_error PaigeImportFilter::pgPrepareImport (void)
{
   return   NO_ERROR;
}


/* pgReadNextBlock should load the next textblock into the buffer. If no bytes are read (end
of file reached last call), FALSE is returned, otherwise TRUE is return even if only one byte
is read. Note, however, for some translators the read process may process the data, in which case
the buffer will contain ZERO bytes even though the function returns TRUE.

DEFAULT BEHAVIOUR:  This specific code assumes raw text. */

pg_boolean PaigeImportFilter::pgReadNextBlock (void)
{  
   if (filepos == file_eof)
      translator.bytes_transferred = 0;
   else {

      if (file_begin == filepos)
         unicode_flag = TEST_UNICODE_FLAG;

      translator.cache_begin = filepos;
      translator.bytes_transferred = max_buffer;
      io_result = read_buffer(output_ptr, &translator.bytes_transferred, &filepos, file_eof, io_proc, filemap, &unicode_flag);
   	  
   	  if (!is_cached)
   	  	  TranslateCrLf();
   }

   bytes_read = filepos - file_begin;
   
   if (translator.bytes_transferred == 0)
      return   FALSE;

   return   TRUE;
}

/* pgImportDone gets called after everything has been imported. */

pg_error PaigeImportFilter::pgImportDone (void)
{
   if (import_pg_rec->cache_file) {
      text_block_ptr       block;
      long              num_blocks;
      
      num_blocks = GetMemorySize(import_pg_rec->t_blocks);
      
      for (block = (text_block_ptr)UseMemory(import_pg_rec->t_blocks); num_blocks; ++block, --num_blocks) {
         
         block->cache_flags = 0;
         block->cache_begin = file_begin + block->begin;
      }
      
      UnuseMemory(import_pg_rec->t_blocks);
   }

   return   NO_ERROR;
}

/* ImportCleanup gets called after the import is resolved and prior to pagination. */

void PaigeImportFilter::pgImportCleanup ()
{

}

/* TranslateCrLf removes LFs from stream since Paige doesn't want them. */

void PaigeImportFilter::TranslateCrLf (void)
{
	register pg_char_ptr	input;
	register pg_char_ptr	output;
	long					input_size, output_size;
	pg_char					next_char;

	input_size = translator.bytes_transferred;
	input = output = output_ptr;

	for (output_size = 0; input_size; --input_size) {
		
		next_char = *input++;
		
		if (next_char != 0x0A || last_buffer_char != 0x0D) {
			
			if (next_char == 0x0A)
				next_char = 0x0D;

			*output++ = next_char;
			++output_size;
		}
		
		last_buffer_char = next_char;
	}
	
	translator.bytes_transferred = output_size;
}


/* PushOutputState preserves the current state of the output chars. This is not a "stack"
and therefore this call overwrites the previous, non-nested. */

void PaigeImportFilter::PushOutputState (void)
{
   pushed_transferred = translator.bytes_transferred;
   pushed_text_read = translator.total_text_read;
   pushed_char_out = last_char_out;
   pushed_char2_out = last_char2_out;
}

/* PopOutputState restores the output chars to what they were on the last PushPutputState(). */

void PaigeImportFilter::PopOutputState (void)
{
   translator.bytes_transferred = pushed_transferred;
   translator.total_text_read = pushed_text_read;
   output_ptr = (pg_char_ptr)UseMemoryRecord(translator.data, translator.bytes_transferred, 0, FALSE);
   last_char_out = pushed_char_out;
   last_char2_out = pushed_char2_out;
}


/* ImportTemporaryBuffer causes all subsequent bytes to import from ref until the
bytes are exhausted, at which time the ref is destroyed and normal input resumes. */

void PaigeImportFilter::ImportTemporaryBuffer (memory_ref ref)
{
   if (temp_buffer_ref)
      UnuseAndDispose(temp_buffer_ref);

   temp_buffer_ptr = NULL;
   temp_buffer_ref = MEM_NULL;
   temp_bytes_read = 0;

   if (ref)
      temp_buffer_ref = ref;
   else {
   
      if (!(temp_buffer_ref = parallel_buffer))
         return;
      
      temp_buffer_ref = parallel_buffer;
      parallel_buffer = MEM_NULL;
   }
   
   if (!(temp_char_count = GetMemorySize(temp_buffer_ref))) {
      
      DisposeMemory(temp_buffer_ref);
      temp_buffer_ref = MEM_NULL;
   }
   else
      temp_buffer_ptr = (pg_char_ptr)UseMemory(temp_buffer_ref);
}


/* OutputCharacter appends another text byte to the output.  If we hit the max then TRUE
is returned. */

pg_boolean PaigeImportFilter::OutputCharacter (pg_char the_byte)
{
   if (feature_bits & IMPORT_NO_LEADING_SPACES)
      if (the_byte == ' ')
         if (last_char_out <= 0x20)
            return   FALSE;

   *output_ptr++ = the_byte;
   last_char2_out = last_char_out;
   last_char_out = (pg_short_t)the_byte;

   translator.bytes_transferred += 1;
   translator.total_text_read += 1;

   if (translator.bytes_transferred == max_output_buffer) {

      if (feature_bits & IMPORT_EXTEND_OUTPUT) {
         
         output_ptr = (pg_char_ptr)AppendMemory(translator.data, 1024, TRUE);
         max_output_buffer += 1024;
         CallWait();
      }
      else
         return   TRUE;
   }

   return   FALSE;
}

/* OutputString outputs characters in cstring the_str. */

pg_boolean PaigeImportFilter::OutputString (pg_char_ptr the_str)
{
   pg_boolean  result = FALSE;
   short    index;
   
   for (index = 0; the_str[index]; ++index)
      if ((result = OutputCharacter(the_str[index])) != FALSE)
         break;
   
   return   result;
}


/* pgProcessEmbedData accepts data from from a translator then processes that data to
the appropriate type. */

void PG_FAR * PaigeImportFilter::pgProcessEmbedData (memory_ref ref, long embed_type)
{
   void PG_FAR    *result_ptr = NULL;
   long        data_size;
   
   data_size = GetMemorySize(ref);

   switch (embed_type) {
      
      case embed_mac_pict:
      
#ifdef MAC_PLATFORM

         result_ptr = (void PG_FAR *)MemoryToHandle(ref);
#endif
         break;

      case embed_meta_file:
         {
#ifdef WINDOWS_PLATFORM

#ifdef WIN32_COMPILE
            void PG_FAR    *metaptr;
            
            
            metaptr = UseMemory(ref);
            result_ptr = (void PG_FAR *)SetMetaFileBitsEx((UINT)data_size, (BYTE PG_FAR *)metaptr);
            UnuseMemory(ref);
            DisposeMemory(ref);
#else
            HANDLE         metabits;
   
            metabits = MemoryToHandle(ref);
            result_ptr = (void PG_FAR *)SetMetaFileBits(metabits);
#endif
#endif
         }
         
         break;
   }
   
   if (!result_ptr)
      result_ptr = (void PG_FAR *)ref;

   return   result_ptr;
}


/* pgMapFont sets the alternate font name. This function gets called regardless of
source and current OS. Upon entry, file_os is the OS from which the file came,
while current_os is the current platform. */

void PaigeImportFilter::pgMapFont (font_info_ptr font, long importing_os, long current_os)
{
   register pg_char_ptr    table_ptr;
   pg_char                 alternate_buffer[FONT_SIZE];
   short                alternate_index, out_index;

#ifdef MAC_PLATFORM
#pragma unused (importing_os, current_os)
#endif

   if ((table_ptr = font_cross_table) == (pg_char_ptr)NULL)
      return;
   
   if (pgIsRealFont(paige_globals, font, FALSE))
      return;

   while (*table_ptr <= font->name[1]) {
      
      if ((alternate_index = pgCompareFontTable(&font->name[1], table_ptr)) != 0) {
         
         table_ptr += alternate_index;
         pgBlockMove(font->name, font->alternate_name, FONT_SIZE * sizeof(pg_char));
         pgFillBlock(font->name, FONT_SIZE * sizeof(pg_char), 0);
         
         for (out_index = 1; out_index < FONT_SIZE; ++out_index) {
            
            font->name[out_index] = *table_ptr++;
            font->name[0] += 1;
            
            if (*table_ptr == ']' || *table_ptr == 0)
               break;
         }
         
         break;
      }
      
      for (;;) {
         
         if (*table_ptr++ == 0)
            break;
      }
   }

// Check to see if the font is valid on this machine:

   if (!pgIsRealFont(paige_globals, font, FALSE)) {

      pgBlockMove(font->name, alternate_buffer, FONT_SIZE * sizeof(pg_char));

      if (pgIsRealFont(paige_globals, font, TRUE)) {
         
         pgBlockMove(font->name, font->alternate_name, FONT_SIZE * sizeof(pg_char));
         pgBlockMove(alternate_buffer, font->name, FONT_SIZE * sizeof(pg_char));
      }
      else {
      
         *font = paige_globals->def_font;
         pgBlockMove(alternate_buffer, font->alternate_name, FONT_SIZE * sizeof(pg_char));
      }
   }
}


/* pgMapChars converts chars in all the imported text blocks. */

void PaigeImportFilter::pgMapChars (pg_ref pg)
{
   paige_rec_ptr     pg_rec;
   text_block_ptr    block;
   long           num_blocks;
   
   pg_rec = (paige_rec_ptr)UseMemory(pg);
   num_blocks = GetMemorySize(pg_rec->t_blocks);
   block = (text_block_ptr)UseMemory(pg_rec->t_blocks);
   
   while (num_blocks) {
      
      pgMapCharacters(pg_rec, block);
      ++block;
      --num_blocks;
   }
   
   UnuseMemory(pg_rec->t_blocks);
}


/* FindFontInList returns TRUE if the given font name is already in the list. */

pg_boolean PaigeImportFilter::FindFontInList (font_info_ptr font)
{
   font_info_ptr     fonts;
   pg_boolean        result = FALSE;
   long           num_fonts;
   
   num_fonts = GetMemorySize(import_pg_rec->fonts);
   
   for (fonts = (font_info_ptr)UseMemory(import_pg_rec->fonts); num_fonts; ++fonts, --num_fonts)
      if (pgEqualFontNames(fonts, font, FALSE))
         break;

   UnuseMemory(import_pg_rec->fonts);
   
   return      result;
}


/************************* Utility functions (C interface) ****************************/


PG_PASCAL (pg_error) pgImportFileFromC (pg_ref pg, pg_filetype filetype, long feature_flags,
      long file_begin, pg_file_unit f_ref)
{
   PaigeImportObject       filter;
   pg_globals_ptr          globals;
   long                 flags;
   pg_error             result = NO_ERROR;
   
   if (!(flags = feature_flags))
      flags = IMPORT_EVERYTHING_FLAG;

   globals = pgGetGlobals(pg);
   
   switch (filetype) {
      
      case pg_text_type:
         filter = new PaigeImportFilter();
         break;

      case pg_rtf_type:
         filter = (PaigeImportObject) new PaigeRTFImportFilter();
         break;

      case pg_paige_type:
         filter = (PaigeImportObject) new PaigeNativeImportFilter();
         break;
      
      case pg_html_type:
         filter = (PaigeImportObject) new PaigeHTMLImportFilter();
         break;

      default:
         return   (pg_error)BAD_TYPE_ERR;
   }

   if ((result = filter->pgInitImportFile(globals, f_ref, NULL, NULL, file_begin, UNKNOWN_POSITION)) == NO_ERROR)
      result = filter->pgImportFile(pg, CURRENT_POSITION, flags, TRUE, draw_none);

   delete filter;

   return      result;
}


/***************************************** Local Functions *****************************************/

/* read_buffer loads in large buffer so we can input sequential bytes thereafter. If *is_unicode
is a BOM (Byte Order Mark) then we have read unicode characters.  Or,  if *is_unicode is
TEST_UNICODE_FLAG we test for Unicode and set the flag accordingly.  Or, if *is_inicode is FALSE
then we just assume ASCII 8-bit bytes. */

static pg_error read_buffer (pg_char_ptr buffer, long PG_FAR *bytesize, long PG_FAR *position,
      long max_position, file_io_proc io_proc, file_ref filemap, pg_short_t PG_FAR *is_unicode)
{
   long        distance_to_end;
   pg_bits8_ptr   trans_buffer;
   pg_error    result = NO_ERROR;

   if ((distance_to_end = max_position - *position) < *bytesize)
      *bytesize = distance_to_end;
   
   trans_buffer = (pg_bits8_ptr)buffer;
   result = io_proc((void PG_FAR *)buffer, io_data_direct, position, bytesize, filemap);

   if (result == NO_ERROR) {
      
      if (*is_unicode == TEST_UNICODE_FLAG) {
         
         pgBlockMove(buffer, is_unicode, 2);

         if (*is_unicode == PG_BOM || *is_unicode == PG_REVERSE_BOM) {
            
            *bytesize -= 2;
            pgBlockMove(&trans_buffer[2], trans_buffer, *bytesize);
         }
         else
            *is_unicode = FALSE;
      }

#ifdef UNICODE

      if (*is_unicode)
         *bytesize = pgUnicodeToUnicode(buffer, *bytesize,
                  (pg_boolean)(*is_unicode == PG_REVERSE_BOM));
      else
         *bytesize = pgBytesToUnicode((pg_bits8_ptr)buffer, buffer, NULL, *bytesize);
#else
      if (*is_unicode) {
         long        charsize;
         
         charsize = *bytesize / 2;

         charsize = pgUnicodeToUnicode((pg_short_t PG_FAR *)buffer, charsize / sizeof(pg_char),
               (pg_boolean)(*is_unicode == PG_REVERSE_BOM));
         *bytesize = pgUnicodeToBytes((pg_short_t PG_FAR *)buffer, buffer, NULL, charsize);
      }
#endif

   }

   return      result;
}

