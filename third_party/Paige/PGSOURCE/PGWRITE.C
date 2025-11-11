/* This file contains the various functions for WRITING a Paige document.   */

/* Dec 22, 1994 added volitile support - TR Shaw, OITC */

/* Modified Jun 95 for various problems and enhancments by TR Shaw, OITC */

#include "Paige.h"

#ifdef MAC_PLATFORM
#pragma segment pgwrite
#endif

#include "pgExceps.h"
#include "machine.h"
#include "defprocs.h"
#include "pgBasics.h"
#include "pgText.h"
#include "pgUtils.h"
#include "pgSelect.h"
#include "pgDefStl.h"
#include "pgErrors.h"
#include "PackDefs.h"
#include "pgFiles.h"
#include "pgSubRef.h"
#include "pgtxr.h"
#include "pgHText.h"
#include "pgFrame.h"


#define CLR_LINE_SAVE_BIT (long) (~(long)NO_LINE_SAVE_BIT)

static void PG_FAR * use_array (memory_ref ref, long PG_FAR *num_recs);
static short do_write_handler (paige_rec_ptr pg, memory_ref handlers,
		pg_handler_ptr the_handler, memory_ref data, long element_info,
		file_io_proc write_proc, size_t PG_FAR *file_position, file_ref filemap,
		void PG_FAR *aux_data, size_t PG_FAR *original_size, long PG_FAR *new_element_info);
static void extend_buffer_size (pack_walk_ptr out_data, long extend_size);
static long send_short_hex (pg_bits8_ptr out_data, pg_short_t value,
		short PG_FAR *no_zero_suppress);
static long send_long_hex (pg_bits8_ptr out_data, long value,
		short PG_FAR *no_zero_suppress);
static pg_bits8 hex_char (pg_short_t value, short PG_FAR *no_zero_suppress);
static void pack_point_starts (pack_walk_ptr walker, point_start_ptr starts,
		pg_short_t num_starts);
static void optimize_packed_data (pack_walk_ptr walker);
static long like_bytes (pg_bits8_ptr data);
static long pack_tabs (pack_walk_ptr walker, tab_stop_ptr tabs, pg_short_t tab_qty);
static void pack_indents (pack_walk_ptr walker, pg_indents_ptr indents);
static long predict_byte_save (paige_rec_ptr pg, memory_ref handlers);
static void pack_style_info (paige_rec_ptr pg, pack_walk_ptr walker,
		style_info_ptr style_to_pack);
static void pack_par_info (pack_walk_ptr walker, par_info_ptr info);
static void pack_font_info (pack_walk_ptr walker, font_info_ptr info);
static void output_opt_character_ref(pack_walk_ptr walker, memory_ref ref);
static void load_overlapping_blocks (paige_rec_ptr pg, long offset_to);

#ifndef UNICODE
static void save_unicode_block (paige_rec_ptr pg, pack_walk_ptr walker, text_block_ptr block);
#endif


/* pgWriteHandlerProc is the standard handler for creating data items to
write. Its job is to process the data to make it writeable. The resulting
data is in key_data.   */

PG_PASCAL (pg_boolean) pgWriteHandlerProc (paige_rec_ptr pg, pg_file_key key, memory_ref key_data,
		long PG_FAR *element_info, void PG_FAR *aux_data, long PG_FAR *unpacked_size)
{
	pack_walk					walker;
	register paige_rec_ptr		pgr;
	register text_block_ptr		block;
	register style_info_ptr		style_ptr;
	par_info_ptr				par_ptr;
	font_info_ptr				fonts;
	t_select_ptr				selections;
	select_pair_ptr				pair_ptr;
	memory_ref					general_ref;
	long						general_ctr;
	pg_boolean					no_line_save;
	void PG_FAR					*general_data;

	pgr = pg;
	pgFillBlock(&walker, sizeof(pack_walk), 0);

	walker.data_ref = key_data;
	walker.remaining_ctr = GetMemorySize(walker.data_ref);
	walker.data = (pg_bits8_ptr) UseMemory(walker.data_ref);

	switch (key) {
		
		case pg_eof_key:
		case pg_signature:
			*element_info = PAIGE_VERSION;
			pgPackNum(&walker, short_data, PAIGE_HEADER_SIG);
			break;

		case paige_key:
			if (!pgr->shared_flags) {
			
			 	pgPackNum(&walker, long_data, PAIGE_VERSION);	//е TRS/OITC Fixes file compatibility bug discovered in test
			 	pgPackNum(&walker, long_data, pgr->platform);
			 	pgPackNum(&walker, long_data, pgr->flags);
			 	pgPackNum(&walker, long_data, (pgr->flags2 | TEXT_FILE_FLAGS));
			 	pgPackNum(&walker, long_data, pgr->pg_type);		// (RTF enhancement) TRS/OITC
			 	pgPackNum(&walker, long_data, pgr->hdr_ftr_loc[0]);		// (RTF enhancement) TRS/OITC
			 	pgPackNum(&walker, long_data, pgr->hdr_ftr_loc[1]);		// (RTF enhancement) TRS/OITC
			 	pgPackNum(&walker, long_data, pgr->resolution);
			 	pgPackNum(&walker, long_data, pgr->tab_base);
			 	pgPackNum(&walker, long_data, pgr->doc_top);
			 	pgPackNum(&walker, long_data, pgr->doc_bottom);
			 	pgPackNum(&walker, short_data, pgr->unit_h);
			 	pgPackNum(&walker, short_data, pgr->unit_v);
			 	pgPackNum(&walker, short_data, pgr->append_h);
			 	pgPackNum(&walker, short_data, pgr->append_v);
			 	pgPackNum(&walker, long_data, pgr->scroll_align_h);
			 	pgPackNum(&walker, long_data, pgr->scroll_align_v);
			
				pgPackSelectPair(&walker, &pgr->first_word);
				pgPackSelectPair(&walker, &pgr->last_word);

				pgPackCoOrdinate(&walker, &pgr->logical_scroll_pos);
				pgPackCoOrdinate(&walker, &pgr->scale_factor.origin);
				pgPackNum(&walker, long_data, pgr->real_scaling);
	
				pgPackRect(&walker, &pgr->doc_bounds);
				pgPackColor(&walker, &pgr->bk_color);
				pgPackCoOrdinate(&walker, &pgr->port.origin);
				pgPackNum(&walker, long_data, pgr->text_direction);
				pgPackNum(&walker, long_data, pgr->author);
				pgPackNum(&walker, long_data, pgr->next_id);
				pgPackNum(&walker, long_data, pgr->hilite_flags);
				pgPackCoOrdinate(&walker, &pgr->base_vis_origin);
				pgPackNum(&walker, long_data, pgr->hilite_anchor);
				
				// pgPackNum(&walker, short_data, pgr->scaled_resolution);

				pgPackNum(&walker, short_data, pgr->def_named_index);

				*unpacked_size = sizeof(paige_rec);
			}
			
			break;

		case text_block_key:
			if (pgr->io_mask_bits & EXPORT_TEXT_FLAG)
				break;

			no_line_save = (pg_boolean)((pgr->flags & NO_LINE_SAVE_BIT) != 0);
			
			*element_info = GetMemorySize(pgr->t_blocks);
			for (block = (text_block_ptr) use_array(pgr->t_blocks, &general_ctr); general_ctr;
					++block, --general_ctr) {

				pgPackTextBlock(&walker, block, FALSE, no_line_save);
				*unpacked_size += sizeof(text_block);
			}
			
			UnuseMemory(pgr->t_blocks);

			break;

		case text_key:
		case line_key:
			if (pgr->io_mask_bits & EXPORT_TEXT_FLAG)
				break;

			block = pgFindTextBlock(pgr, *element_info, NULL, FALSE, FALSE);
			
			if (key == text_key) {
				
				pgr->procs.load_proc(pgr, block);
			
			#ifndef UNICODE
			
				if (pgr->flags2 & SAVE_AS_UNICODE)
					save_unicode_block(pgr, &walker, block);
				else
			#endif
					{
						general_ref = block->text;
				
						general_data = use_array(general_ref, &general_ctr);
						general_ctr *= sizeof(pg_char);
						*unpacked_size = general_ctr;
						
						pgPackUnicodeBytes(&walker, (pg_bits8_ptr) general_data, general_ctr, FALSE);

						UnuseMemory(general_ref);
					}
			}
			else {

				general_ref = block->lines;
				general_data = use_array(general_ref, &general_ctr);
				pack_point_starts(&walker, (point_start_ptr) general_data, (pg_short_t)general_ctr);
				
				*unpacked_size = GetByteSize(block->lines);
				UnuseMemory(general_ref);
			}
			
			UnuseMemory(pgr->t_blocks);

			break;

		case style_run_key:
		case par_run_key:
			if (key == style_run_key) {

				if (pgr->io_mask_bits & EXPORT_TEXT_FORMATS_FLAG)
					break;

				general_ref = pgr->t_style_run;
			}
			else {

				if (pgr->io_mask_bits & EXPORT_PAR_FORMATS_FLAG)
					break;

				general_ref = pgr->par_style_run;
			}
			
			*unpacked_size = 0;			
			*element_info = pgPackStyleRun(&walker, general_ref, unpacked_size);

			break;

		case style_info_key:
			if (pgr->io_mask_bits & EXPORT_TEXT_FORMATS_FLAG)
				break;

			if (!pgr->shared_flags) {
			
				pgPackNum(&walker, short_data, pgr->insert_style);
				*element_info = GetMemorySize(pgr->t_formats);
				*unpacked_size = GetByteSize(pgr->t_formats);
	
				for (style_ptr = (style_info_ptr) use_array(pgr->t_formats, &general_ctr); general_ctr;
					++style_ptr, --general_ctr)
					if (!(style_ptr->class_bits & NO_SAVEDOC_BIT))
						pack_style_info(pgr, &walker, style_ptr);
					else
						*element_info -= 1;
	
				UnuseMemory(pgr->t_formats);
			}
			
			break;

		case par_info_key:
			if (pgr->io_mask_bits & EXPORT_PAR_FORMATS_FLAG)
				break;

			if (!pgr->shared_flags) {

				*element_info = GetMemorySize(pgr->par_formats);
				*unpacked_size = GetByteSize(pgr->par_formats);
	
				for (par_ptr = (par_info_ptr) use_array(pgr->par_formats, &general_ctr); general_ctr;
						++par_ptr, --general_ctr)
					if (!(par_ptr->class_info & NO_SAVEDOC_PAR))
						pack_par_info(&walker, par_ptr);
					else
						*element_info -= 1;
	
				UnuseMemory(pgr->par_formats);
			}
			
			break;

		case font_info_key:
			if (pgr->io_mask_bits & EXPORT_TEXT_FORMATS_FLAG)
				break;

			if (!pgr->shared_flags) {

				*element_info = GetMemorySize(pgr->fonts);
				*unpacked_size = GetByteSize(pgr->fonts);
	
				for (fonts = (font_info_ptr) use_array(pgr->fonts, &general_ctr); general_ctr;
					++fonts, --general_ctr)
					if (!(fonts->environs & FONT_NOT_SAVED))
						pack_font_info(&walker, fonts);
					else
						*element_info -= 1;
	
				UnuseMemory(pgr->fonts);
			}
			
			break;

		case vis_shape_key:
			if (!(pgr->shared_flags & SHARED_VIS_AREA)) {
			
				*element_info = pgPackShape(&walker, pgr->vis_area);
				*unpacked_size = GetByteSize(pgr->vis_area);
			}
			
			break;

		case page_shape_key:
			if (pgr->io_mask_bits & EXPORT_CONTAINERS_FLAG)
				break;

			if (!(pgr->shared_flags & SHARED_PAGE_AREA)) {

				*element_info = pgPackShape(&walker, pgr->wrap_area);
				*unpacked_size = GetByteSize(pgr->wrap_area);
			}
			break;

		case exclude_shape_key:
			if (!(pgr->shared_flags & SHARED_EXCLUDE_AREA)) {

				*element_info = pgPackShape(&walker, pgr->exclude_area);
				*unpacked_size = GetByteSize(pgr->exclude_area);
			}
			
			break;

		case selections_key:
			if (!(general_ctr = pgr->num_selects))
				general_ctr = 1;
			else
				general_ctr *= 2;

			*element_info = general_ctr;

			for (selections = (t_select_ptr) UseMemory(pgr->select); general_ctr;
				++selections, --general_ctr) {
				
				*unpacked_size += sizeof(t_select);

				pgPackNum(&walker, long_data, selections->offset);
				pgPackSelectPair(&walker, &selections->word_offsets);
				pgPackCoOrdinate(&walker, &selections->original_pt);
				pgPackNum(&walker, short_data, selections->line);
				pgPackNum(&walker, long_data, selections->primary_caret);
				pgPackNum(&walker, long_data, selections->secondary_caret);
				pgPackNum(&walker, short_data, selections->flags);
			}
			
			UnuseMemory(pgr->select);
			
			break;

		case extra_struct_key:
			/* does nothing on its own */			
			break;
		
		case applied_range_key:
			if (pgr->applied_range)
				if (general_ctr = GetMemorySize(pgr->applied_range)) {
				
				*element_info = general_ctr;
				*unpacked_size = GetByteSize(pgr->applied_range);
				
				for (pair_ptr = (select_pair_ptr) UseMemory(pgr->applied_range); general_ctr;
						++pair_ptr, --general_ctr)
					pgPackSelectPair(&walker, pair_ptr);

				UnuseMemory(pgr->applied_range);
			}

			break;

		case doc_info_key:
			if (pgr->io_mask_bits & EXPORT_PAGE_INFO_FLAG)
				break;

			if (!pgr->shared_flags) {
			
				pgPackNum(&walker, long_data, pgr->doc_info.attributes);
				pgPackNum(&walker, short_data, pgr->doc_info.exclusion_inset);
				pgPackNum(&walker, long_data, pgr->doc_info.repeat_slop);
	
				pgPackCoOrdinate(&walker, &pgr->doc_info.repeat_offset);
				pgPackRect(&walker, &pgr->doc_info.print_target);
				pgPackRect(&walker, &pgr->doc_info.margins);
	
				pgPackNum(&walker, short_data, pgr->doc_info.num_pages);
				pgPackNum(&walker, long_data, pgr->doc_info.ref_con);
	
				pgPackNumbers(&walker, pgr->doc_info.future, PG_FUTURE, long_data);
	
				pgPackNum(&walker, long_data, pgr->doc_info.max_chars_per_line);
				pgPackNum(&walker, short_data, pgr->doc_info.minimum_widow);
				pgPackNum(&walker, short_data, pgr->doc_info.minimum_orphan);
	
				pgPackNum(&walker, long_data, pgr->doc_info.section_attributes);
				pgPackNum(&walker, short_data, pgr->doc_info.scroll_inset);
				pgPackNum(&walker, short_data, pgr->doc_info.caret_width_extra);
				pgPackRect(&walker, &pgr->doc_info.offsets);
				pgPackNum(&walker, short_data, pgr->doc_info.start_page_num);
				pgPackNum(&walker, short_data, pgr->doc_info.restart_page_num);
				pgPackNum(&walker, short_data, pgr->doc_info.page_num_prefix);
				pgPackNum(&walker, short_data, pgr->doc_info.page_num_sep);
				pgPackNum(&walker, short_data, pgr->doc_info.page_num_format);
				pgPackCoOrdinate(&walker, &pgr->doc_info.page_num_loc);
				pgPackNum(&walker, long_data, pgr->doc_info.hyph_hot_zone);
				pgPackNum(&walker, short_data, pgr->doc_info.consecutive_hyph);
				pgPackNum(&walker, short_data, pgr->doc_info.start_line_num);
				pgPackNum(&walker, short_data, pgr->doc_info.restart_line_num);
				pgPackNum(&walker, short_data, pgr->doc_info.line_increment);
				pgPackNum(&walker, long_data, pgr->doc_info.line_num_loc);
				pgPackNum(&walker, long_data, pgr->doc_info.line_num_width);
				pgPackNum(&walker, short_data, pgr->doc_info.rev_format);
				pgPackNum(&walker, short_data, pgr->doc_info.rev_bar);
				pgPackNum(&walker, short_data, pgr->doc_info.num_of_cols);
				pgPackNum(&walker, long_data, pgr->doc_info.gutter);
				pgPackNum(&walker, long_data, pgr->doc_info.dflt_gap);
				for (general_ctr = 0; general_ctr < MAX_COLS; general_ctr++) {
					pgPackNum(&walker, long_data, pgr->doc_info.col_info[general_ctr].width);
					pgPackNum(&walker, long_data, pgr->doc_info.col_info[general_ctr].gap);
				}
				pgPackNum(&walker, long_data, pgr->doc_info.version);
				pgPackNum(&walker, long_data, pgr->doc_info.vern);
				pgPackNum(&walker, long_data, pgr->doc_info.creationtime);
				pgPackNum(&walker, long_data, pgr->doc_info.revisiontime);
				pgPackNum(&walker, long_data, pgr->doc_info.printtime);
				pgPackNum(&walker, long_data, pgr->doc_info.backuptime);
				pgPackNum(&walker, long_data, pgr->doc_info.edittime);
				pgPackNum(&walker, long_data, pgr->doc_info.id);
	
				output_opt_character_ref(&walker, pgr->doc_info.title);
				output_opt_character_ref(&walker, pgr->doc_info.subject);
				output_opt_character_ref(&walker, pgr->doc_info.author);
				output_opt_character_ref(&walker, pgr->doc_info.wp_operator);
				output_opt_character_ref(&walker, pgr->doc_info.keywords);
				output_opt_character_ref(&walker, pgr->doc_info.comment);
				output_opt_character_ref(&walker, pgr->doc_info.doccomm);

				pgPackNum(&walker, long_data, pgr->doc_info.gutter_color);
				pgPackNum(&walker, long_data, pgr->doc_info.page_borders);

				*unpacked_size = sizeof(pg_doc_info);
			}
			
			break;
		
		case containers_key:
			if (pgr->io_mask_bits & EXPORT_CONTAINERS_FLAG)
				break;

		case exclusions_key:
			if (key == containers_key)
				general_ref = pgr->containers;
			else {
			
				if (pgr->flags2 & HAS_PG_FRAMES_BIT) {
					memory_ref PG_FAR	*frames;
					
					general_ctr = GetMemorySize(pgr->exclusions);
					*element_info = general_ctr;
					frames = (memory_ref *) UseMemory(pgr->exclusions);
					
					while (general_ctr) {
						
						pgPackFrame(pgr, &walker, *frames);
						++frames;
						--general_ctr;
					}
					
					UnuseMemory(pgr->exclusions);
					break;
				}
				
				general_ref = pgr->exclusions;
			}
			
			*unpacked_size = GetByteSize(general_ref);

			if (general_ctr = GetMemorySize(general_ref)) {

				pgPackNumbers(&walker, UseMemory(general_ref), (short)general_ctr, long_data);
				UnuseMemory(general_ref);

				*element_info = general_ctr;
			}

			break;
		
		case globals_key:
			if (!pgr->shared_flags) {
				register pg_globals_ptr		globals;
				
				globals = pgr->globals;
	
				pgPackNumbers(&walker, &globals->line_wrap_char, SHORT_GLOBAL_CHARS, short_data);
				pgPackBytes(&walker, (pg_bits8_ptr)globals->hyphen_char, LONG_GLOBAL_CHARS);
				pgPackColor(&walker, &globals->def_bk_color);
				pgPackColor(&walker, &globals->trans_color);
			}
			break;

		case exception_key:
			if (*element_info == DUP_KEY_HANDLER_ERROR)
				*element_info = NO_ERROR;

			break;
		
		case named_styles_key:
			if (pgr->io_mask_bits & EXPORT_STYLESHEETS_FLAG)
				break;

			if (!pgr->named_styles)
				break;

			if (!pgr->shared_flags) {
				named_stylesheet_ptr		named_styles;

				*element_info = GetMemorySize(pgr->named_styles);
				*unpacked_size = GetByteSize(pgr->named_styles);
	
				for (named_styles = (named_stylesheet_ptr) use_array(pgr->named_styles, &general_ctr); general_ctr;
					++named_styles, --general_ctr) {
					
					pgPackUnicodeBytes(&walker, (pg_bits8_ptr)named_styles->name, FONT_SIZE * sizeof(pg_char), FALSE);
					
					pgPackNum(&walker, short_data, named_styles->stylesheet_id);
					pgPackNum(&walker, short_data, named_styles->par_stylesheet_id);
				}
	
				UnuseMemory(pgr->named_styles);
			}

			break;
		
		case par_exclusions_key:
			if (GetMemorySize(pgr->par_exclusions)) {
			
				*unpacked_size = 0;			
				*element_info = pgPackStyleRun(&walker, pgr->par_exclusions, unpacked_size);
			}

			break;
		
		case hyperlink_key:
		case hyperlink_target_key:
			{
				memory_ref		hyperlinks;
				
				if (key == hyperlink_key)
					hyperlinks = pgr->hyperlinks;
				else
					hyperlinks = pgr->target_hyperlinks;
				
				*element_info = pgPackHyperlinks(&walker, hyperlinks, unpacked_size);
			}
			
			break;
	}
	
	if (walker.transfered)
		pgFinishPack(&walker);
	else
		UnuseMemory(walker.data_ref);

	SetMemorySize(walker.data_ref, walker.transfered);
	
	key_data = walker.data_ref;

	return	TRUE;
}


/* pgWriteKeyData sends a standard key, data size, element_info and data to the
target "file" at *file_position. The position is updated and an error, if any,
is returned.  If data size is zero, nothing is written.  */

extern PG_PASCAL (pg_error) pgWriteKeyData (pg_ref pg, pg_file_key key,
		void PG_FAR *data, long data_length, long element_info, file_io_proc io_proc,
		file_io_proc data_io_proc, size_t PG_FAR *file_position, file_ref filemap)
{
	paige_rec_ptr			pg_rec;
	pg_key_header			header;
	file_io_proc			write_proc, data_proc;
	pg_globals_ptr			globals;
	size_t					data_size, should_be_position, header_size;
	size_t					header_position;
	pg_boolean				cached_file;
	short					error, do_zeros;
	
	if (!(data_size = data_length))
		return	NO_ERROR;
	
	if (!(write_proc = io_proc))
		write_proc = pgStandardWriteProc;

	if (!(data_proc = data_io_proc))
		data_proc = pgStandardWriteProc;

	do_zeros = TRUE;			/* Must include all leading zeros */
	
	pg_rec = (paige_rec_ptr) UseMemory(pg);
	globals = pg_rec->globals;

	cached_file = (pg_boolean)((pg_rec->cache_target_file != MEM_NULL)
					&& (pg_rec->cache_file != pg_rec->cache_target_file)
					&& (key <= text_key));

	send_short_hex(header, key, &do_zeros);
	send_long_hex(&header[KEY_HEADER_SIZE], data_size, &do_zeros);
	send_long_hex(&header[KEY_HEADER_SIZE + DATA_HEADER_SIZE], element_info, &do_zeros);
	
	header_size = PG_HEADER_SIZE;
	
	should_be_position = PG_HEADER_SIZE + *file_position + data_size;
	header_position = *file_position ;

	if (cached_file) {

		load_overlapping_blocks(pg_rec, *file_position + header_size + data_size);
		
		if (key == text_key) {
			text_block_ptr		block;
			long				data_index;
			pg_char_ptr			data_ptr;
			
			block = pgFindTextBlock(pg_rec, element_info, NULL, FALSE, FALSE);
			block->cache_begin = *file_position + header_size;
			data_ptr = (pg_char_ptr)data;
			
			for (data_index = 0; data_index < data_size; ++data_index) {
				
				block->cache_begin += 1;
				
				if (data_ptr[data_index] == ',')
					break;
			}
			
			block->cache_flags &= (~CACHE_CHANGED_FLAG);
			block->cache_flags |= (CACHE_SAVED_FLAG);
			
			if (pg_rec->flags2 & UNICODE_SAVED)
				block->cache_flags |= CACHE_UNICODE_FLAG;

			UnuseMemory(pg_rec->t_blocks);
		}
	}

	if (!(error = write_proc(header, io_data_direct, file_position, &header_size, filemap)))
		if (data_size)
			if (!(error = data_proc(data, io_data_direct, file_position,
					&data_size, filemap)))
				if (*file_position != should_be_position) {  /* data size is different! */
					
					data_size += (*file_position - should_be_position);
					send_long_hex(&header[KEY_HEADER_SIZE], data_size, &do_zeros);
					error = write_proc(header, io_data_direct, &header_position, &header_size, filemap);
				}

	if (!error) {
		pg_handler_ptr			existing_handler;
		
		if (existing_handler = pgFindHandlerFromKey(globals->file_handlers, key, NULL)) {

			if (!(existing_handler->flags & HAS_STANDARD_WRITE_HANDLER))
				error = DUP_KEY_HANDLER_ERROR;

			UnuseMemory(globals->file_handlers);
		}
	}

	UnuseMemory(pg);

	return	error;
}


/* pgSaveDoc writes one or more data portions to the target file specified in
filemap (which contains machine-specific items).  The actual I/O is performed
by the write_proc (if non-NULL) or the standard file_io_proc for writing (whose
definition is in defaultprocs.h and code in machinespecific.h). If keys is non-
NULL, num_keys pg_file_key items in the pointer are output;  if keys is NULL
all available handlers are called. The function result is zero if successful,
or the appropriate error code. */

PG_PASCAL (pg_error) pgSaveDoc (pg_ref pg, size_t PG_FAR *file_position, const pg_file_key_ptr keys,
		pg_short_t num_keys, file_io_proc write_proc, file_ref filemap,
		long doc_element_info)
{
	volatile paige_rec_ptr		pg_rec;
	volatile pgm_globals_ptr	mem_globals;
	volatile wait_process_proc	wait_proc;
	register pg_handler_ptr		handlers;
	register text_block_ptr		block;
	pg_handler					header_handler;
	size_t						element_info, total_to_save, progress, actual_size;
	pg_error					result;
	pg_short_t					ctr;
	pg_short_t					key_qty;
	volatile memory_ref			handlers_to_use = MEM_NULL;
	volatile memory_ref			key_data = MEM_NULL;
	volatile memory_ref			subref_list = MEM_NULL;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	subref_list = pgGetSubrefState(pg_rec, FALSE, TRUE);

	wait_proc = pg_rec->procs.wait_proc;
	pg_rec->version = PAIGE_VERSION;

	mem_globals = pg_rec->globals->mem_globals;
	total_to_save = 0;
	
	PG_TRY(mem_globals) {

		if (!pg_rec->globals->file_handlers)
			pgInitStandardHandlers(pg_rec->globals);
	
		pgPushMemoryID(pg_rec);

		handlers_to_use = pgBuildHandlerList(pg_rec->globals, keys, num_keys);
		
		if (pgFindHandlerFromKey(handlers_to_use, line_key, NULL))
			UnuseMemory(handlers_to_use);
		else
			pg_rec->flags |= NO_LINE_SAVE_BIT;

		total_to_save = predict_byte_save(pg_rec, handlers_to_use);
		progress = 0;

		result = NO_HANDLER_ERR;

		key_data = MemoryAlloc(mem_globals, 1, 0, 0);
		
		wait_proc(pg_rec, save_wait, 0, total_to_save);

		handlers = (pg_handler_ptr) UseMemory(handlers_to_use);
		
		pgInitOneHandler(&header_handler, pg_signature);
		result = do_write_handler(pg_rec, MEM_NULL, &header_handler, key_data,
				0, write_proc, file_position, filemap, NULL, &actual_size, NULL);
		pgFailError(mem_globals, result);
		
		for (key_qty = (pg_short_t)GetMemorySize(handlers_to_use); key_qty; ++handlers, --key_qty) {
			
			result = NO_ERROR;

			if ((handlers->key == text_key) || (handlers->key == line_key)) {
				
				for (block = (text_block_ptr) UseMemory(pg_rec->t_blocks), ctr = (pg_short_t)GetMemorySize(pg_rec->t_blocks);
						ctr;  ++block, --ctr) {
						
					result = do_write_handler(pg_rec, handlers_to_use, handlers, key_data,
							block->begin, write_proc, file_position,
							filemap, NULL, &actual_size, NULL);

					pgFailError(mem_globals, result);
					
					progress += actual_size;
					if (progress >= total_to_save)
						progress = total_to_save - 1;
					
					wait_proc(pg_rec, save_wait, progress, total_to_save);
				}

				UnuseMemory(pg_rec->t_blocks);
			}
			else
			if (handlers->key == extra_struct_key) {
				
				if (pg_rec->extra_stuff) {
					long PG_FAR		*extra_ptr;
					
					extra_ptr = (long *) UseMemory(pg_rec->extra_stuff);
					ctr = (pg_short_t)GetMemorySize(pg_rec->extra_stuff);
					
					
					for (element_info = -EXTRA_STRUCT_RSRV; ctr;
								++element_info, ++extra_ptr, --ctr) {

						if (*extra_ptr)
							result = do_write_handler(pg_rec, handlers_to_use, handlers, key_data,
									element_info, write_proc, file_position,
									filemap, extra_ptr, &actual_size, NULL);

						pgFailError(mem_globals, result);
					
						progress += actual_size;
					}

					UnuseMemory(pg_rec->extra_stuff);
				}
			}
			else {

				if (handlers->key == paige_key)
					element_info = doc_element_info;
				else
					element_info = DEFAULT_ELEMENT_INFO;

				result = do_write_handler(pg_rec, handlers_to_use, handlers, key_data, element_info,
					write_proc, file_position, filemap, NULL, &actual_size, NULL);

				pgFailError(mem_globals, result);
				
				progress += actual_size;
			}
			
			if (progress >= total_to_save)
				progress = total_to_save - 1;

			wait_proc(pg_rec, save_wait, progress, total_to_save);
		}

		DisposeMemory(key_data);
		UnuseAndDispose(handlers_to_use);

		pg_rec->flags &= CLR_LINE_SAVE_BIT;
		
		pgPopMemoryID(pg_rec);

		wait_proc(pg_rec, save_wait, total_to_save, total_to_save);
		
		UnuseMemory(pg);
	}
	
	PG_CATCH {
		
		wait_proc(pg_rec, save_wait, total_to_save, total_to_save);
		
		if (handlers_to_use)
			DisposeFailedMemory(handlers_to_use);
		
		if (key_data)
			DisposeFailedMemory(key_data);

		pg_rec->flags &= CLR_LINE_SAVE_BIT;

		pgPopMemoryID(pg_rec);
		pgRestoreSubRefs(pg_rec, subref_list);
		UnuseAllFailedMemory(mem_globals, pg_rec->mem_id);

		return	mem_globals->last_error;
	}
	
	PG_ENDTRY;

	pgRestoreSubRefs(pg_rec, subref_list);

	return		NO_ERROR;
}


/* pgCacheSaveDoc is identical to pgSaveDoc() except the target file will become the new
"cache" file (as if opened with pgCacheReadDoc() ). It is OK if filemap contains a file
reference already in use as the cache file.  The new file must remain open. */

PG_PASCAL (pg_error) pgCacheSaveDoc (pg_ref pg, size_t PG_FAR *file_position, const pg_file_key_ptr keys,
		pg_short_t num_keys, file_io_proc write_proc, file_ref filemap,
		long doc_element_info)
{
	paige_rec_ptr			pg_rec;
	pg_error				result = NO_ERROR;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	
	if (pg_rec->cache_file == filemap)
		pg_rec->cache_target_file = filemap;
	else
		pg_rec->cache_target_file = MEM_NULL;

	result = pgSaveDoc(pg, file_position, keys, num_keys, write_proc, filemap, doc_element_info);
	
	if (result == NO_ERROR) {
		
		if (pg_rec->cache_target_file) {
			text_block_ptr			block;
			long					ctr;

			block = (text_block_ptr) UseMemory(pg_rec->t_blocks);
			
			for (ctr = GetMemorySize(pg_rec->t_blocks); ctr; ++block, --ctr)
				block->cache_flags &= (~(CACHE_SAVED_FLAG | CACHE_CHANGED_FLAG));
			
			UnuseMemory(pg_rec->t_blocks);
		}

		pg_rec->cache_file = filemap;
		pg_rec->cache_target_file = MEM_NULL;
	}

	UnuseMemory(pg);
	
	return		result;
}


/* pgTerminateFile writes a logical end-of-file key to file_position. This
gets called by apps that want to terminate the file (because more data that
is non-PAIGE data can be written). Note that if the file's physical end of file
is the same as the PAIGE doc end of file, this function does not need to be
called (although it does not hurt). */

PG_PASCAL (pg_error) pgTerminateFile (pg_ref pg, size_t PG_FAR *file_position,
		file_io_proc write_proc, file_ref filemap)
{
	paige_rec_ptr				pg_rec;
	pg_handler					eof_handler;
	memory_ref					key_data;
	size_t						actual_size;
	pg_error					result;

	pg_rec = (paige_rec_ptr) UseMemory(pg);

	key_data = MemoryAlloc(pg_rec->globals->mem_globals, 1, 0, 0);
	pgInitOneHandler(&eof_handler, pg_eof_key);
	actual_size = 0;
	result = do_write_handler(pg_rec, MEM_NULL, &eof_handler, key_data,
			0, write_proc, file_position, filemap, NULL, &actual_size, NULL);
	
	DisposeMemory(key_data);
	UnuseMemory(pg);

	return		result;
}



/* pgPackData packs src_data into target_data. The data type is coerced to be
data_type (which is byte_data, short_data or long_data).  This works correctly
even if the target_data memory_ref is not of the same type (e.g., target might
have a record size > long's but you can pack longs). The target_data ref can
be any size and gets set to whatever size is required; additionally, a special
terminator character is placed at the end of packing so different packed items
can be partitioned.  */

PG_PASCAL (void) pgPackData (memory_ref src_data, memory_ref target_data,
		pg_short_t data_type)
{
	pack_walk			walker;
	void PG_FAR			*src_ptr;
	pg_short_t			increment_size;
	long				src_byte_size;

	pgSetupPacker(&walker, target_data, GetMemorySize(target_data));
	
	src_byte_size = GetByteSize(src_data);
	src_ptr = UseMemory(src_data);
	
	if (data_type == byte_data)
		pgPackBytes(&walker, (pg_bits8_ptr)src_ptr, src_byte_size);
	else {

		if (data_type == short_data)
			increment_size = sizeof(short);
		else
			increment_size = sizeof(long);
		
		pgPackNumbers(&walker, src_ptr, (short)(src_byte_size / increment_size), data_type);
	}

	UnuseMemory(src_data);

	pgFinishPack(&walker);
}


/* pgPackNum gets called to send a short or long value to the data stream.
The number is always sent in "hex" to avoid platform dependencies.  */

PG_PASCAL (void) pgPackNum (pack_walk_ptr out_data, short code, long value)
{
	register pack_walk_ptr		data;
	register pg_bits8_ptr		data_ptr;
	register long				out_ctr, comp_val;

	data = out_data;
	if (data->remaining_ctr < MAX_HEX_SIZE)
		extend_buffer_size(data, HEX_EXTEND_SIZE);
	
	data_ptr = data->data;
	out_ctr = 0;

	if (data->last_code && (data->last_value == value)) {
		
		*data_ptr++ = (code | REPEAT_LAST_VALUE);
		++out_ctr;
	}
	else {
		
		*data_ptr++ = (pg_bits8)code;
		++out_ctr;

		if ((comp_val = value) < 0) {
			
			comp_val = -comp_val;
			*data_ptr++ = MINUS_SIGN;
			++out_ctr;
		}

		if (!comp_val) {
			
			*data_ptr++ = '0';
			++out_ctr;
		}
		else {
			short		no_zero_suppress;
			long		amt_sent;
			
			no_zero_suppress = FALSE;
			
			amt_sent = send_long_hex(data_ptr, comp_val, &no_zero_suppress);

			data_ptr += amt_sent;
			out_ctr += amt_sent;
		}
	}
	
	data->data = data_ptr;
	data->transfered += out_ctr;
	data->remaining_ctr -= out_ctr;
	data->last_value = value;
	data->last_code = code;
}


/* pgPackTextBlock packs (saves) a text_block. */

extern PG_PASCAL (void) pgPackTextBlock (pack_walk_ptr walker, text_block_ptr block,
			pg_boolean include_text, pg_boolean no_line_save)
{
	short		flags_save;
	
	flags_save = block->flags;
	if (no_line_save)
		block->flags |= LINES_PURGED;

	pgPackNum(walker, long_data, block->begin);
	pgPackNum(walker, long_data, block->end);
	pgPackNum(walker, short_data, block->flags);
	pgPackNum(walker, long_data, block->first_line_num);
	pgPackNum(walker, short_data, block->num_lines);
	pgPackNum(walker, long_data, block->first_par_num);
	pgPackNum(walker, short_data, block->num_pars);
	pgPackNum(walker, long_data, block->user_var.refcon);

	pgPackRect(walker, &block->bounds);
	pack_point_starts(walker, &block->end_start, 1);
	
	block->flags = flags_save;
	
	if (include_text) {
	
		pgPackUnicodeBytes(walker, (pg_bits8_ptr) UseMemory(block->text), (block->end - block->begin) * sizeof(pg_char), FALSE);
		UnuseMemory(block->text);
	}

	pgPackSubRefs(walker, block);
}


/* pgPackNumbers packs qty numbers from ptr into out_data. The data_code is
either short_data or long_data.  */

PG_PASCAL (void) pgPackNumbers (pack_walk_ptr out_data, void PG_FAR *ptr, short qty, 
		short data_code)
{
	register short			ctr;
	register short PG_FAR	*short_ptr;
	register long  PG_FAR	*long_ptr;

	if (data_code == short_data) {
		for (ctr = 0, short_ptr = (short*) ptr; ctr < qty; ++ctr)
			pgPackNum(out_data, short_data, short_ptr[ctr]);
	}
	else {
		for (ctr = 0, long_ptr = (long *) ptr; ctr < qty; ++ctr)
			pgPackNum(out_data, long_data, long_ptr[ctr]);
	}
}

PG_PASCAL (void) pgPackMemoryRef (pack_walk_ptr walker, memory_ref ref)
{
	output_opt_character_ref(walker,ref);
}


/* pgPackBytes gets called to send a raw stream of data. The byte code is
always followed by a hex quantity and a comma.  */

PG_PASCAL (void) pgPackBytes (pack_walk_ptr out_data, pg_bits8_ptr the_bytes, long length)
{
	pg_bits8_ptr	data;
	long			amt_out, out_ctr;
	short			no_zero_suppress;

	if (!length)
		return;

	amt_out = length + MAX_HEX_SIZE + 1;

	if (out_data->remaining_ctr < amt_out)
		extend_buffer_size(out_data, amt_out + HEX_EXTEND_SIZE);

	data = out_data->data;
	out_ctr = 0;
	no_zero_suppress = FALSE;

	*data++ = byte_data;
	++out_ctr;
	
	amt_out = send_long_hex(data, length, &no_zero_suppress);
	data += amt_out;
	out_ctr += amt_out;
	
	*data++ = ',';
	++out_ctr;

	pgBlockMove(the_bytes, data, length);
	out_ctr += length;
	data += length;

	out_data->remaining_ctr -= out_ctr;
	out_data->transfered += out_ctr;
	out_data->data = data;
}

/* pgFinishPack gets called by functions that perform a pack. It terminates the
packed data.  */

PG_PASCAL (memory_ref) pgFinishPack (pack_walk_ptr walker)
{
	if (!walker->remaining_ctr)
		extend_buffer_size(walker, 1);
	
	walker->data[0] = terminator_data;
	++walker->transfered;

	UnuseMemory(walker->data_ref);
	optimize_packed_data(walker);
	
	return	walker->data_ref;
}


/* pgPackRect packs a rectangle*/

PG_PASCAL (void) pgPackRect (pack_walk_ptr walker, rectangle_ptr r)
{
	pgPackCoOrdinate(walker, &r->top_left);
	pgPackCoOrdinate(walker, &r->bot_right);
}



/* pgPackCoOrdinate packs a co_ordinate */

PG_PASCAL (void) pgPackCoOrdinate (pack_walk_ptr walker, co_ordinate_ptr point)
{
	pgPackNum(walker, long_data, point->v);
	pgPackNum(walker, long_data, point->h);
}


/* This packs a select_pair */

PG_PASCAL (void) pgPackSelectPair (pack_walk_ptr walker, select_pair_ptr pair)
{
	pgPackNum(walker, long_data, pair->begin);
	pgPackNum(walker, long_data, pair->end);
}



/* pgPackColor packs a "color_value" record */

PG_PASCAL (void) pgPackColor (pack_walk_ptr walker, color_value PG_FAR *color)
{
	pgPackNum(walker, short_data, color->red);
	pgPackNum(walker, short_data, color->green);
	pgPackNum(walker, short_data, color->blue);
	pgPackNum(walker, short_data, color->alpha);
}


/* This packs a shape_ref. The function result is the number of rectangles
packed.  */

PG_PASCAL (long) pgPackShape (pack_walk_ptr walker, shape_ref the_shape)
{
	rectangle_ptr	rects;
	long			shape_size, result;
	
	shape_size = result = GetMemorySize(the_shape);
	for (rects = (rectangle_ptr) UseMemory(the_shape); shape_size; ++rects, --shape_size)
		pgPackRect(walker, rects);

	UnuseMemory(the_shape);
	
	return	result;
}


/* This function packs any style_run, given in general_ref. The function result
is the number of elements in the run that were written. */

PG_PASCAL (long) pgPackStyleRun (pack_walk_ptr walker, memory_ref general_ref,
		long PG_FAR *unpacked_size)
{
	register style_run_ptr		run_ptr;
	long						run_ctr, result;
	
	*unpacked_size += GetByteSize(general_ref);
	run_ptr = (style_run_ptr) use_array(general_ref, &run_ctr);
	result = run_ctr;

	while (run_ctr) {
			
		pgPackNum(walker, long_data, run_ptr->offset);
		pgPackNum(walker, short_data, run_ptr->style_item);
		
		++run_ptr;
		--run_ctr;
	}

	UnuseMemory(general_ref);
	
	return	result;
}


/* pgPackUnicodeBytes is identical to pgPackBytes except a Unicode BOM is placed in front
of the text if Unicode is enabled for this library. Otherwise the bytes are just given
to pgPackBytes(). If unconditional == TRUE we do this no matter what,  otherwise if this
is not a Unicode lib then it gets mapped to pgPackBytes(). */

PG_PASCAL (void) pgPackUnicodeBytes (pack_walk_ptr out_data, pg_bits8_ptr the_bytes,
		long length, pg_boolean unconditional)
{
	pg_bits8_ptr	data;
	long			amt_out, out_ctr;
	pg_short_t		byte_order_mark;
	short			no_zero_suppress;

	if (!length)
		return;

#ifndef UNICODE
	if (!unconditional) {
		
		pgPackBytes(out_data, the_bytes, length);
		return;
	}
#endif

	amt_out = length + MAX_HEX_SIZE + sizeof(pg_char) + 1;

	if (out_data->remaining_ctr < amt_out)
		extend_buffer_size(out_data, amt_out + HEX_EXTEND_SIZE);

	data = out_data->data;
	out_ctr = 0;
	no_zero_suppress = FALSE;

	*data++ = byte_data;
	++out_ctr;
	
	amt_out = send_long_hex(data, length + sizeof(pg_byte), &no_zero_suppress);
	data += amt_out;
	out_ctr += amt_out;
	
	*data++ = ',';
	++out_ctr;
	
	byte_order_mark = PG_BOM;
	pgBlockMove((void PG_FAR *)&byte_order_mark, data, sizeof(pg_short_t));
	out_ctr += sizeof(pg_char);
	data += sizeof(pg_char);

	pgBlockMove(the_bytes, data, length);
	out_ctr += length;
	data += length;

	out_data->remaining_ctr -= out_ctr;
	out_data->transfered += out_ctr;
	out_data->data = data;
}


/****************************** Local Functions **************************/


/* To reduce code size a wee bit, this function gets called to do a "Use" on
a paige structure and return its size.  */

static void PG_FAR * use_array (memory_ref ref, long PG_FAR *num_recs)
{
	*num_recs = GetMemorySize(ref);
	return	UseMemory(ref);
}



/* This is called by pgSaveDoc and does (A) Calls the write handler, (B) Writes
the data returned by handlers, and (C) Handlers any exceptions (file errors)
and returns that result. Note: NO DATA IS WRITTEN IF ZERO LENGTH DATA. */

static short do_write_handler (paige_rec_ptr pg, memory_ref handlers,
		pg_handler_ptr the_handler, memory_ref data, long element_info,
		file_io_proc write_proc, size_t PG_FAR *file_position, file_ref filemap,
		void PG_FAR *aux_data, size_t PG_FAR *original_size, long PG_FAR *new_element_info)
{
	pg_boolean		handler_done;
	memory_ref		global_handlers;
	void PG_FAR		*alternate_ptr;
	file_io_proc	data_proc;
	short			result;
	size_t			aux_default_data, data_size;
	long			element_data;
	
	element_data = element_info;
	result = NO_ERROR;
	global_handlers = pg->globals->file_handlers;
	
	if (!(alternate_ptr = aux_data)) {
		
		aux_default_data = 0;
		alternate_ptr = (void PG_FAR *)&aux_default_data;
	}
	
	if (handlers)
		pg->globals->file_handlers = handlers;

	*original_size = 0;

	for (;;) {

		handler_done = the_handler->write_handler(pg, the_handler->key, data,
				&element_data, alternate_ptr, original_size);
		
		if (data_size = GetMemorySize(data)) {
			
			if (write_proc)
				data_proc = write_proc;
			else
				data_proc = the_handler->write_data_proc;

			result = pgWriteKeyData(pg->myself, the_handler->key, UseMemory(data), data_size,
				element_data, write_proc, data_proc, file_position, filemap);

			UnuseMemory(data);
			
			if (result)
				result = pgDoExceptionKey(pg, pg->globals->file_handlers, result, TRUE, data);
		}

		if (handler_done || result)
			break;
	}
	
	pg->globals->file_handlers = global_handlers;

	if (new_element_info)
		*new_element_info = element_data;

	return	result;
}



/* This function sends a short as a hex stream to the output, suppressing leading zeros. 
The number of bytes output is returned.  */

static long send_short_hex (pg_bits8_ptr out_data, pg_short_t value,
		short PG_FAR *no_zero_suppress)
{
	register pg_bits8_ptr		data;
	register long				amt_output;
	register pg_short_t			part_byte, ctr;
	register pg_bits8			hex_byte;

	amt_output = 0;
	data = out_data;

	for (ctr = 2, part_byte = value >> 8; ctr; --ctr) {
	
		if (hex_byte = hex_char((pg_short_t)(part_byte >> 4), no_zero_suppress)) {
			
			*data++ = hex_byte;
			++amt_output;
		}

		if (hex_byte = hex_char((pg_short_t)(part_byte & 0x0F), no_zero_suppress)) {
			
			*data++ = hex_byte;
			++amt_output;
		}
		
		part_byte = value & 0xFF;
	}
	
	return	amt_output;
}


/* This is the same as send_short_hex but for a long.  */

static long send_long_hex (pg_bits8_ptr out_data, long value,
		short PG_FAR *no_zero_suppress)
{
	long		amt_sent;
	
	amt_sent = send_short_hex(out_data, pgHiWord(value), no_zero_suppress);
	out_data += amt_sent;
	amt_sent += send_short_hex(out_data, (pg_short_t)value, no_zero_suppress);
	
	return	amt_sent;
}



/* This function returns a single ASCII hex character based on value. Zero
suppress is set if the character is non-zero. If the resulting char will simply
be another leading zero, zero is returned (indicating no value).  */

static pg_bits8 hex_char (pg_short_t value, short PG_FAR *no_zero_suppress)
{
	pg_bits8		result;
	
	result = (pg_bits8)(value | '0');
	if (value > 9)
		result += 7;
	
	if (result == '0') {
		
		if (!*no_zero_suppress)
			return	0;
	}
	else
		*no_zero_suppress = TRUE;

	return	result;
}



/* This function gets called to extend the packed output data size. It extends
by extend_size.  */

static void extend_buffer_size (pack_walk_ptr out_data, long extend_size)
{
	out_data->data = (pg_bits8_ptr) AppendMemory(out_data->data_ref, extend_size, FALSE);
	out_data->data -= out_data->remaining_ctr;
	out_data->remaining_ctr += extend_size;
}



/* This function packs 1 or more point starts. */

static void pack_point_starts (pack_walk_ptr walker, point_start_ptr starts,
		pg_short_t num_starts)
{
	register point_start_ptr	line_starts;
	register pg_short_t			qty;
	
	for (qty = num_starts, line_starts = starts; qty; ++line_starts, --qty) {
		
		pgPackNum(walker, short_data, line_starts->offset);
		pgPackNum(walker, short_data, line_starts->extra);
		pgPackNum(walker, short_data, line_starts->baseline);
		pgPackNum(walker, short_data, line_starts->flags);
		pgPackNum(walker, short_data, line_starts->r_num);
		pgPackRect(walker, &line_starts->bounds);
	}
}


/* This function makes one last pass through the packed data and optimizes it
to compress lots of repeat bytes.  */

static void optimize_packed_data (pack_walk_ptr walker)
{
	register pg_bits8_ptr		in_data, out_data;
	register long				data_size, new_size;
	register long				ctr;
	register pg_bits8			the_byte;
	long						text_size;

	new_size = walker->first_offset;

	in_data = (pg_bits8_ptr) UseMemory(walker->data_ref);
	in_data += new_size;
	out_data = in_data;

	data_size = walker->transfered - new_size;

	while (data_size) {
		
		the_byte = *in_data;
		
		if (the_byte & REPEAT_LAST_VALUE) {
			
			if ((ctr = like_bytes(in_data)) >= MINIMUM_MULTI_PACK) {
				
				*out_data++ = (the_byte | REPEAT_LAST_N_TIMES);
				*out_data++ = (pg_bits8)ctr;
				new_size += 2;
				
				in_data += ctr;
				data_size -= ctr;
			}
			else {
				
				*out_data++ = *in_data++;
				++new_size;
				--data_size;
			}
		}
		else {

			*out_data++ = *in_data++;
			++new_size;
			--data_size;

			switch (the_byte) {
				
				case byte_data:
					ctr = pgUnpackHex(in_data, &text_size);
					ctr += text_size + 1;
					break;					
					
				case short_data:
				case long_data:
					ctr = pgUnpackHex(in_data, NULL);
					break;
				
				case terminator_data:
					ctr = 0;
					break;
			}
			
			if (!ctr)
				break;

			pgBlockMove(in_data, out_data, ctr);
			
			new_size += ctr;
			in_data += ctr;
			out_data += ctr;
			data_size -= ctr;
		}
	}
	
	UnuseMemory(walker->data_ref);
	
	walker->transfered = new_size;
	SetMemorySize(walker->data_ref, new_size);
}

/* This gets called from optimize_packed_data (above). The data pointer will be
sitting on a data-type code (0x81 or 0x82). Note, it does not matter what the remaining
size of the data pointer is because it will be terminated with 0x03 (which will
be an impossible value otherwise).  The function always returns 1 (the first
byte is a "sequence" of 1).  */

static long like_bytes (pg_bits8_ptr data)
{
	register pg_bits8_ptr		look_ptr;
	register pg_bits8			byte_check;
	register long				result;
	
	look_ptr = data;
	byte_check = *look_ptr++;
	
	for (result = 1; *look_ptr == byte_check; ++result, ++look_ptr)
		if (result == MAX_REPEAT_SIZE)
			break;
	
	return	result;
}


/* This packs a list of tabs in a tab_ref. Returns number of tabs in list. */

static long pack_tabs (pack_walk_ptr walker, tab_stop_ptr tabs, pg_short_t tab_qty)
{
	long					num_tabs;
	register tab_stop_ptr	tabs_ptr;

	tabs_ptr = tabs;

	for (num_tabs = tab_qty; num_tabs; ++tabs_ptr, --num_tabs) {
		
		pgPackNum(walker, long_data, tabs_ptr->tab_type);
		pgPackNum(walker, long_data, tabs_ptr->position);
		pgPackNum(walker, long_data, tabs_ptr->leader);
		pgPackNum(walker, long_data, tabs_ptr->ref_con);
	}
	
	return	(long)tab_qty;
}



/* This packs pg_indents record */

static void pack_indents (pack_walk_ptr walker, pg_indents_ptr indents)
{
	pgPackNum(walker, long_data, indents->left_indent);
	pgPackNum(walker, long_data, indents->right_indent);
	pgPackNum(walker, long_data, indents->first_indent);
}


/* This function makes a buest-guess as to how many byte will be saved. This
is used to report the progress for possible "wait proc."  */

static long predict_byte_save (paige_rec_ptr pg, memory_ref handlers)
{
	long						result;
	register paige_rec_ptr		pg_rec;
	register pg_handler_ptr		handler;
	register text_block_ptr		block;
	memory_ref					byte_test;
	pg_short_t					qty, num_blocks;

	pg_rec = pg;
	handler = (pg_handler_ptr) UseMemory(handlers);
	
	for (qty = (pg_short_t)GetMemorySize(handlers), result = 0; qty; ++handler, --qty) {
		
		byte_test = MEM_NULL;
		
		switch (handler->key) {
		
			case paige_key:
				result += sizeof(paige_rec);
				break;

			case text_block_key:
				byte_test = pg_rec->t_blocks;
				break;

			case text_key:
				result += pg_rec->t_length;
				break;

			case line_key:
				for (block = (text_block_ptr) UseMemory(pg_rec->t_blocks), num_blocks = (pg_short_t)GetMemorySize(pg_rec->t_blocks);
						num_blocks; ++block, --num_blocks)
					result += GetByteSize(block->lines);
				
				UnuseMemory(pg_rec->t_blocks);
				break;

			case style_run_key:
				byte_test = pg_rec->t_style_run;
				break;

			case par_run_key:
				byte_test = pg_rec->par_style_run;
				break;

			case style_info_key:
				byte_test = pg_rec->t_formats;
				break;

			case par_info_key:
				byte_test = pg_rec->par_formats;
				break;
				
			case font_info_key:
				byte_test = pg_rec->fonts;
				break;

			case vis_shape_key:
				byte_test = pg_rec->vis_area;
				break;

			case page_shape_key:
				byte_test = pg_rec->wrap_area;
				break;

			case exclude_shape_key:
				byte_test = pg_rec->exclude_area;
				break;

			case selections_key:
				if (pg_rec->num_selects)
					result += ((pg_rec->num_selects * 2) * sizeof(t_select));
				else
					result += sizeof(t_select);

				break;
				
			case extra_struct_key:
				byte_test = pg_rec->extra_stuff;
				break;

			case applied_range_key:
				byte_test = pg_rec->applied_range;
				break;
				
			case doc_info_key:
				result += sizeof(pg_doc_info);
				break;
				
			case containers_key:
				byte_test = pg_rec->containers;
				break;

			case exclusions_key:
				byte_test = pg_rec->exclusions;
				break;
		}

		if (byte_test)
			result += GetByteSize(byte_test);
	}

	UnuseMemory(handlers);

	return	result;
}


/* This packs a style_info record */

static void pack_style_info (paige_rec_ptr pg, pack_walk_ptr walker,
		style_info_ptr style_to_pack)
{
	style_info			style;	
	
	style = *style_to_pack;
	style.procs.save_style(pg, &style);

	pgPackNum(walker, short_data, style.font_index);
	pgPackNum(walker, short_data, style.char_bytes);
	pgPackNum(walker, short_data, style.max_chars);
	pgPackNum(walker, short_data, style.ascent);
	pgPackNum(walker, short_data, style.descent);
	pgPackNum(walker, short_data, style.leading);
	pgPackNum(walker, short_data, style.shift_verb);
	pgPackNum(walker, long_data, style.class_bits);	//е TRS/OITC
	pgPackNum(walker, long_data, style.style_sheet_id);	//е TRS/OITC

	pgPackColor(walker, &style.fg_color);
	pgPackColor(walker, &style.bk_color);

	pgPackNum(walker, long_data, style.char_width);
	pgPackNum(walker, long_data, style.point);
	pgPackNum(walker, long_data, style.left_overhang);
	pgPackNum(walker, long_data, style.right_overhang);
	pgPackNum(walker, long_data, style.top_extra);
	pgPackNum(walker, long_data, style.bot_extra);
	pgPackNum(walker, long_data, style.space_extra);
	pgPackNum(walker, long_data, style.char_extra);
	pgPackNum(walker, long_data, style.user_id);
	pgPackNum(walker, long_data, style.user_data);
	pgPackNum(walker, long_data, style.user_data2);

	pgPackNum(walker, long_data, style.time_stamp);	// (RTF enhancement) TRS/OITC

	pgPackNum(walker, long_data, style.user_var.refcon);
	pgPackNum(walker, long_data, style.used_ctr);

	pgPackNumbers(walker, style.future, PG_FUTURE, long_data);	// Note this include embeddeds
	pgPackNumbers(walker, style.styles, MAX_STYLES, short_data);
	pgPackNum(walker, long_data, style.small_caps_index);

	pgPackNum(walker, long_data, style.key_equiv);	//ее TRS/OITC
	pgPackNum(walker, short_data, style.style_num);
	pgPackNum(walker, short_data, style.style_basedon);
	pgPackNum(walker, short_data, style.rtf_reserved);
	pgPackNum(walker, long_data, style.named_style_index);
}


/* This packs a par_info record */

static void pack_par_info (pack_walk_ptr walker, par_info_ptr info)
{
	register par_info_ptr	par_ptr;
	
	par_ptr = info;

	pgPackNum(walker, short_data, par_ptr->justification);
	pgPackNum(walker, short_data, par_ptr->direction);
	pgPackNum(walker, short_data, par_ptr->style_sheet_id);

	pack_indents(walker, &par_ptr->indents);

	pgPackNum(walker, long_data, par_ptr->spacing);
	pgPackNum(walker, long_data, par_ptr->leading_extra);
	pgPackNum(walker, long_data, par_ptr->leading_fixed);
	pgPackNum(walker, long_data, par_ptr->leading_variable);
	pgPackNum(walker, long_data, par_ptr->top_extra);
	pgPackNum(walker, long_data, par_ptr->bot_extra);
	pgPackNum(walker, long_data, par_ptr->left_extra);
	pgPackNum(walker, long_data, par_ptr->right_extra);
	pgPackNum(walker, long_data, par_ptr->user_id);
	pgPackNum(walker, long_data, par_ptr->user_data);
	pgPackNum(walker, long_data, par_ptr->user_data2);
	pgPackNum(walker, long_data, par_ptr->partial_just);
	pgPackNum(walker, short_data, par_ptr->outline_level);
	pgPackNum(walker, long_data, par_ptr->user_var.refcon);
	pgPackNum(walker, long_data, par_ptr->used_ctr);

	pgPackNumbers(walker, par_ptr->future, PG_FUTURE, long_data);
	pgPackNum(walker, short_data, (long)par_ptr->num_tabs);
	
	if (par_ptr->num_tabs)
		pack_tabs(walker, par_ptr->tabs, par_ptr->num_tabs);

	pgPackNum(walker, long_data, par_ptr->def_tab_space);

#ifdef PG_BASEVIEW
	// PAJ added 6/9/94

	pgPackUnicodeBytes(walker, (pg_bits8_ptr)par_ptr->user_var.styleSheetName, kStyleSheetNameMaxSize * sizeof(pg_char), FALSE);
	pgPackNum(walker, short_data, par_ptr->user_var.styleSheetResID);
	pgPackNum(walker, short_data, par_ptr->user_var.unused);

	// PAJ end added

#endif

	pgPackNum(walker, short_data, par_ptr->class_info);

	pgPackNum(walker, long_data, par_ptr->key_equiv);	//ее TRS/OITC
	pgPackNum(walker, short_data, par_ptr->style_num);
	pgPackNum(walker, short_data, par_ptr->style_basedon);
	pgPackNum(walker, short_data, par_ptr->next_style);
	pgPackNum(walker, long_data, par_ptr->named_style_index);

	pgPackNum(walker, long_data, par_ptr->table.table_columns);
	pgPackNum(walker, long_data, par_ptr->table.table_column_width);
	pgPackNum(walker, long_data, par_ptr->table.table_cell_height);
	pgPackNum(walker, long_data, par_ptr->table.border_info);
	pgPackNum(walker, long_data, par_ptr->table.border_spacing);
	pgPackNum(walker, long_data, par_ptr->table.border_shading);
	pgPackNum(walker, long_data, par_ptr->table.cell_borders);
	pgPackNum(walker, long_data, par_ptr->table.grid_borders);
	pgPackNum(walker, long_data, par_ptr->table.cell_h_extra);
	
	pgPackNum(walker, long_data, par_ptr->html_style);

	pgPackNum(walker, long_data, par_ptr->table.top_border_color);
	pgPackNum(walker, long_data, par_ptr->table.left_border_color);
	pgPackNum(walker, long_data, par_ptr->table.bottom_border_color);
	pgPackNum(walker, long_data, par_ptr->table.right_border_color);
}


/* This packs a font_info record */

static void pack_font_info (pack_walk_ptr walker, font_info_ptr info)
{
	register font_info_ptr		fonts;
	
	fonts = info;

	pgPackUnicodeBytes(walker, (pg_bits8_ptr)fonts->name, FONT_SIZE * sizeof(pg_char), FALSE);
	pgPackUnicodeBytes(walker, (pg_bits8_ptr)fonts->alternate_name, FONT_SIZE * sizeof(pg_char), FALSE);

	pgPackNum(walker, short_data, fonts->environs);
	pgPackNum(walker, short_data, fonts->typeface);
	pgPackNum(walker, short_data, fonts->family_id);
	pgPackNum(walker, short_data, fonts->char_type);
	pgPackNum(walker, long_data, fonts->code_page);
	if ((fonts->environs & FONTS_TEXT_UNKNOWN) && fonts->machine_var[PG_PREV_LANG])
		pgPackNum(walker, long_data, fonts->machine_var[PG_PREV_LANG]);
	else pgPackNum(walker, long_data, fonts->language);
	pgPackNum(walker, long_data, fonts->user_var.refcon);
	
	pgPackNumbers(walker, fonts->machine_var, PG_FUTURE, long_data);
	
	if (fonts->environs & FONTS_TEXT_UNKNOWN)
		pgPackNum(walker, long_data, fonts->platform);
	else pgPackNum(walker, long_data, PAIGE_GRAPHICS);

	pgPackNum(walker, short_data, fonts->alternate_id);
}


/* Outputs a pg_bits8 block if memory_ref is not MEM_NULL */

static void output_opt_character_ref(pack_walk_ptr walker, memory_ref ref)
{
	if (ref) {
		pgPackBytes(walker, (pg_bits8_ptr)UseMemory(ref), GetByteSize(ref));
		UnuseMemory(ref);
	}
	else pgPackBytes(walker, (pg_bits8_ptr)walker, 0);
}


/* load_overlapping_blocks walks through the text_blocks in pg to load any text that is
not yet loaded into memory but would be overwritten by saving to the file. */

static void load_overlapping_blocks (paige_rec_ptr pg, long offset_to)
{
	text_block_ptr			block;
	long					num_blocks;
	
	num_blocks = GetMemorySize(pg->t_blocks);
	block = (text_block_ptr) UseMemory(pg->t_blocks);
	
	while (num_blocks) {

		if (block->cache_flags & (CACHE_CHANGED_FLAG | CACHE_SAVED_FLAG))
			if (block->cache_begin < offset_to) {
			
			pg->procs.load_proc(pg, block);
			block->cache_flags |= CACHE_CHANGED_FLAG;
		}

		++block;
		--num_blocks;
	}
	
	UnuseMemory(pg->t_blocks);
}

#ifndef UNICODE

static void save_unicode_block (paige_rec_ptr pg, pack_walk_ptr walker, text_block_ptr block)
{
	memory_ref		original_text;

	original_text = block->text;
	block->text = MemoryDuplicate(block->text);
	pgBlockToUnicode(pg, block);
	pgPackUnicodeBytes(walker, (pg_bits8_ptr)UseMemory(block->text), GetMemorySize(block->text), TRUE);
	UnuseAndDispose(block->text);
	block->text = original_text;
}

#endif



