/* This file contains the various functions for READing a Paige document.   */

/* Modified Dec 22 1994 for the use of volatile in PG_TRY-PG_CATCH by TR Shaw, OITC */

/* Modified Jun 95 for various problems and enhancments by TR Shaw, OITC */

#include "Paige.h"

#ifdef MAC_PLATFORM
#pragma segment pgread
#endif

#include "pgExceps.h"
#include "machine.h"
#include "defprocs.h"
#include "pgText.h"
#include "pgUtils.h"
#include "pgSelect.h"
#include "pgDefStl.h"
#include "pgErrors.h"
#include "PackDefs.h"
#include "pgFiles.h"
#include "pgTxtWid.h"
#include "pgBasics.h"
#include "pgShapes.h"
#include "pgIO.h"
#include "pgtxr.h"
#include "pgHText.h"
#include "pgTables.h"
#include "pgSubref.h"
#include "pgFrame.h"

// Definition to fix Win16 bug:
#define REASONABLE_WIN16_SIZE	600

/* Verification constants: */

#define PG_VERIFY_SIZE			12
#define PG_VERIFY_BYTE_SIZE		6

#define VERIFY_BYTE1			0x01
#define VERIFY_BYTE2			'7'
#define VERIFY_BYTE3			'7'
#define VERIFY_BYTE4			'A'
#define VERIFY_BYTE5			'A'
#define VERIFY_BYTE6			0x03


static void PG_FAR * use_array (memory_ref ref, long PG_FAR *num_recs);
static void unpack_point_starts (pack_walk_ptr walker, point_start_ptr starts,
		pg_short_t num_starts);
static void unpack_tabs (paige_rec_ptr pg, pack_walk_ptr walker, tab_stop_ptr tabs, pg_short_t tab_qty);
static void fix_pg_from_done_handlers (paige_rec_ptr pg, memory_ref handlers);
static void unpack_indents (pack_walk_ptr walker, pg_indents_ptr indents);
static void unpack_style_info (paige_rec_ptr pg, pack_walk_ptr walker,
		style_info_ptr info);
static void unpack_par_info (paige_rec_ptr pg, pack_walk_ptr walker,
		par_info_ptr info);
static void unpack_font_info (paige_rec_ptr pg, pack_walk_ptr walker,
		font_info_ptr font_ptr);
static void fix_style_run (paige_rec_ptr pg, memory_ref run, memory_ref styles);
static void fix_hyperlink_run (paige_rec_ptr pg, memory_ref run);
static void unpack_refcon (paige_rec_ptr pg, pack_walk_ptr walker, long PG_FAR *refcon);
static pg_boolean equal_constants (pg_bits8_ptr str1, pg_bits8_ptr str2, short size);
static void input_opt_character_ref(pgm_globals_ptr mem_globals, pack_walk_ptr walker, memory_ref PG_FAR *ref);
static void unpack_fontsize_text (paige_rec_ptr pg, pack_walk_ptr walker, pg_char_ptr name);
static void convert_for_unicode (paige_rec_ptr pg);


/* pgReadHandlerProc is the standard handler for reading all data items. Its job
is to process key_data whose type is indicated by key.  */

/* NOTE: At the end of reading all Paige records have been converted to
	the current version's format.  If the document is changed and saved,
	pgWrite will update the version number so all the expanded fields 
	will be properly saved. The way Paige previously implemented this 
	was a bug in versions prior 0x0001000A. TRS/OITC */

PG_PASCAL (pg_boolean) pgReadHandlerProc (paige_rec_ptr pg, pg_file_key key, memory_ref key_data,
		long PG_FAR *element_info, void PG_FAR *aux_data, long PG_FAR *unpacked_size)
{
	pack_walk					walker;
	register paige_rec_ptr		pgr;
	register text_block_ptr		block;
	register style_info_ptr		style_ptr;
	pg_char_ptr					target_text;
	par_info_ptr				par_ptr;
	font_info_ptr				fonts;
	t_select_ptr				selections;
	select_pair_ptr				pair_ptr;
	memory_ref					general_ref;
	long						input_byte_size, general_ctr, run_ctr;
	long						device_bit, flags2_save, unicode_input_size;
	void PG_FAR					*general_data;
	
#ifdef MAC_PLATFORM
#pragma unused (unpacked_size)
#endif

	if (key_data == MEM_NULL)
		return	TRUE;

	pgSetupPacker(&walker, key_data, 0);
	pgr = pg;
	
	switch (key) {
		
		case pg_signature:
			if (pgUnpackNum(&walker) != PAIGE_HEADER_SIG) {
				
				UnuseMemory(walker.data_ref);
				
				return	FALSE;
			}
			
			if (pgr->shared_flags)
				pgr->version = *element_info;

			break;

		case paige_key:			
			device_bit = pgr->flags & NO_DEVICE_BIT;
			device_bit |= INIT_IO_BIT;
		
		 	pgr->version = pgUnpackNum(&walker);
		 	pgr->platform = pgUnpackNum(&walker);
		 	pgr->flags = pgUnpackNum(&walker) & CLR_NEW_FLAGS;
		 	pgr->flags |= device_bit;
		 	if (pgr->version >= KEY_REVISION6) {
		 		
		 		flags2_save = pgr->flags2;
		 		pgr->flags2 = pgUnpackNum(&walker);		//ее TRS/OITC
		 		pgr->flags2 &= (~NO_READVIS_BIT);
		 		pgr->flags2 |= (flags2_save & NO_READVIS_BIT);
		 	}

			if (pgr->version >= KEY_REVISION9) {		// (RTF enhancement) TRS/OITC
		 		pgr->pg_type = pgUnpackNum(&walker);	// (RTF enhancement) TRS/OITC
		 		if (pgr->pg_type == MAIN_DOCUMENT)		// (RTF enhancement) TRS/OITC
		 			pgr->doc_pg = pg->myself;			// (RTF enhancement) TRS/OITC
		 		pgr->hdr_ftr_loc[0] = pgUnpackNum(&walker);	// (RTF enhancement) TRS/OITC
		 		pgr->hdr_ftr_loc[1] = pgUnpackNum(&walker);	// (RTF enhancement) TRS/OITC
		 	}
		 	pgr->resolution = pgUnpackNum(&walker);
		 	pgr->tab_base = pgUnpackNum(&walker);

		 	pgr->doc_top = pgUnpackNum(&walker);
		 	pgr->doc_bottom = pgUnpackNum(&walker);
		 	pgr->unit_h = (short)pgUnpackNum(&walker);
		 	pgr->unit_v = (short)pgUnpackNum(&walker);
		 	pgr->append_h = (short)pgUnpackNum(&walker);
		 	pgr->append_v = (short)pgUnpackNum(&walker);
		 	pgr->scroll_align_h = pgUnpackNum(&walker);
		 	pgr->scroll_align_v = pgUnpackNum(&walker);
			
			pgUnpackSelectPair(&walker, &pgr->first_word);
			pgUnpackSelectPair(&walker, &pgr->last_word);
			
			pgUnpackCoOrdinate(&walker, &pgr->logical_scroll_pos);
			
			if (!(pgr->flags & EXTERNAL_SCROLL_BIT))
				pgr->scroll_pos = pgr->logical_scroll_pos;

			pgUnpackCoOrdinate(&walker, &pgr->scale_factor.origin);
			pgr->scale_factor.scale = pgUnpackNum(&walker);
			pgr->real_scaling = pgr->scale_factor.scale;

			pgUnpackRect(&walker, &pgr->doc_bounds);
			
			if (walker.remaining_ctr > 1) {

				pgUnpackColor(&walker, &pgr->bk_color);
				pgUnpackCoOrdinate(&walker, &pgr->port.origin);
			}
			
			pgr->text_direction = pgUnpackNum(&walker);
			pgr->author = pgUnpackNum(&walker);
			pgr->next_id = pgUnpackNum(&walker);
			pgr->hilite_flags = pgUnpackNum(&walker);
			pgUnpackCoOrdinate(&walker, &pgr->base_vis_origin);
			pgr->hilite_anchor = pgUnpackNum(&walker);
			
			if (pgr->version < KEY_REVISION15)
				pgUnpackNum(&walker);		// Skip extra value

			pgr->def_named_index = pgUnpackNum(&walker);

			break;

		case text_block_key:
			if (pgr->io_mask_bits & IMPORT_TEXT_FLAG)
				break;
			
			run_ctr = *element_info;	/* = original # of text blocks */

			if ((run_ctr -= GetMemorySize(pgr->t_blocks)) > 0) {
				
				AppendMemory(pgr->t_blocks, run_ctr, TRUE);
				UnuseMemory(pgr->t_blocks);
			}

			pgr->t_length = 0;  // MM fix

/* MSVC++ 1.52 optimization bug forces us to do the following trick for large
text_block arrays: */

#ifdef WIN16_COMPILE
			
			if (*element_info > REASONABLE_WIN16_SIZE) {
				text_block		second_block;
				long			real_num_recs, rec_ctr;
				
				real_num_recs = 0;
				block = UseMemory(pgr->t_blocks);
				general_ctr = GetMemorySize(pgr->t_blocks);
				
				for (rec_ctr = 0; rec_ctr < general_ctr; ++rec_ctr) {
					
					pgUnpackTextBlock(pg, &walker, block, FALSE, MEM_NULL);
					pgr->t_length += (block->end - block->begin);
					
					if ((rec_ctr + 1) < general_ctr) {
						
						pgFillBlock(&second_block, sizeof(text_block), 0);
						pgUnpackTextBlock(pg, &walker, &second_block, FALSE, MEM_NULL);
						pgr->t_length += (second_block.end - second_block.begin);
						block->end = second_block.end;
						block->end_start = second_block.end_start;
						pgTrueUnionRect(&block->bounds, &second_block.bounds, &block->bounds);
						block->flags |= second_block.flags;
						
						DisposeMemory(second_block.text);
						DisposeMemory(second_block.lines);
	
						++rec_ctr;
					}

					++real_num_recs;
					++block;
				}

				UnuseMemory(pgr->t_blocks);
				
				SetMemorySize(pgr->t_blocks, real_num_recs);
			}
			else
#endif
		{
			for (block = (text_block_ptr) use_array(pgr->t_blocks, &general_ctr); general_ctr;
					++block, --general_ctr) {

				pgUnpackTextBlock(pg, &walker, block, FALSE, MEM_NULL);
				pgr->t_length += (block->end - block->begin);
			}

			UnuseMemory(pgr->t_blocks);
		}
			break;

		case text_key:

			if (pgr->cache_file) {
				select_pair_ptr			cache_range;

				cache_range = (select_pair_ptr)aux_data;
				
				block = pgFindTextBlock(pgr, *element_info, NULL, FALSE, FALSE);
				
				if (*element_info > block->end) {
				
					block = (text_block_ptr) AppendMemory(pgr->t_blocks, 1, FALSE);
					pgInitTextblock(pgr, *element_info, MEM_NULL, block, TRUE);
					unicode_input_size = cache_range->end - cache_range->begin;

					if (pgr->flags2 & (UNICODE_SAVED | SAVE_AS_UNICODE))
						unicode_input_size /= 2;

					block->end = block->begin + unicode_input_size;
				}
				else
				if (block->text) {
					
					DisposeMemory(block->text);
					block->text = MEM_NULL;
				}

				block->cache_begin = cache_range->begin;

				if (pgr->flags2 & (UNICODE_SAVED | SAVE_AS_UNICODE))
					block->cache_flags = CACHE_UNICODE_FLAG;
				else
					block->cache_flags = 0;

				UnuseMemory(pgr->t_blocks);
				break;
			}

			if (pgr->io_mask_bits & IMPORT_TEXT_FLAG)
				break;

			block = pgFindTextBlock(pgr, *element_info, NULL, FALSE, FALSE);
			input_byte_size = unicode_input_size = pgGetUnpackedSize(&walker);
		
		#ifdef UNICODE
			unicode_input_size /= sizeof(pg_char);
		#endif

	/* Possibility: It is conceivable that a user saved a file with no
	"text_block_key" but it has text. If so, I'll need to append to text blocks. */
			
			if (*element_info > block->end) {
				
				block = (text_block_ptr) AppendMemory(pgr->t_blocks, 1, FALSE);
				pgInitTextblock(pgr, *element_info, MEM_NULL, block, (pg_boolean)(pgr->cache_file != MEM_NULL));			
				block->end = block->begin + unicode_input_size;
			}

			target_text = (pg_char_ptr) AppendMemory(block->text, unicode_input_size, FALSE);
			pgUnpackPtrBytes(&walker, (pg_bits8_ptr)target_text);
			UnuseMemory(block->text);
			UnuseMemory(pgr->t_blocks);

			break;

		case line_key:
			if (pgr->io_mask_bits & IMPORT_TEXT_FLAG)
				break;

			block = pgFindTextBlock(pgr, *element_info, NULL, FALSE, FALSE);
			input_byte_size = pgGetUnpackedSize(&walker);

	/* Possibility: It is conceivable that a user saved a file with no
	"text_block_key" but it has text. If so, I'll need to append to text blocks. */
			
			if (*element_info > block->end) {
				
				block = (text_block_ptr) AppendMemory(pgr->t_blocks, 1, FALSE);
				pgInitTextblock(pgr, *element_info, MEM_NULL, block, (pg_boolean)(pgr->cache_file != MEM_NULL));
			}
			
			SetMemorySize(block->lines, input_byte_size / sizeof(point_start));
			general_data = use_array(block->lines, &general_ctr);
			unpack_point_starts(&walker, (point_start_ptr) general_data, (pg_short_t)general_ctr);
			UnuseMemory(block->lines);

			UnuseMemory(pgr->t_blocks);

			break;

		case style_run_key:
		case par_run_key:

			if (key == style_run_key) {

				if (pgr->io_mask_bits & IMPORT_TEXT_FORMATS_FLAG)
					break;

				general_ref = pgr->t_style_run;
			}
			else {

				if (pgr->io_mask_bits & IMPORT_PAR_FORMATS_FLAG)
					break;

				general_ref = pgr->par_style_run;
			}

			pgUnpackStyleRun(&walker, general_ref, *element_info);
	
			break;

		case style_info_key:
			if (pgr->io_mask_bits & IMPORT_TEXT_FORMATS_FLAG)
				break;

			if (!(pgr->shared_flags & SHARED_FORMATS)) {
				
				pgr->insert_style = (pg_short_t)pgUnpackNum(&walker);
				
				if ((general_ctr = *element_info) != 0) {
				
				// Fix for earlier incompatibility:
					walker.max_bytes = pgGetUnpackedSize(&walker) / general_ctr;
					walker.max_bytes_ctr = 0;

					if (pgr->version == KEY_REVISION8A)
						if (walker.max_bytes == REV8_STYLE_SIZE)
							pgr->version -= 1;

					if (!(pgr->flags2 & KEEP_READ_STYLES))
						SetMemorySize(pgr->t_formats, 0);
	
					style_ptr = (style_info_ptr) AppendMemory(pgr->t_formats, general_ctr, TRUE);
	
					while (general_ctr) {
					
						unpack_style_info(pgr, &walker, style_ptr);
						walker.max_bytes_ctr = 0;

						++style_ptr;
						--general_ctr;
					}
					
					if (pgr->version < KEY_REVISION10) {
					
						style_ptr = (style_info_ptr) UseMemoryRecord(pgr->t_formats, 0, 0, FALSE);
						style_ptr->used_ctr += 1;
					}
					
					UnuseMemory(pgr->t_formats);
					
					walker.max_bytes = 0;
				}
			}
			
			break;

		case par_info_key:
			if (pgr->io_mask_bits & IMPORT_PAR_FORMATS_FLAG)
				break;

			if (!(pgr->shared_flags & SHARED_FORMATS)) {

				if ((run_ctr = *element_info) != 0) {
				
					if (!(pgr->flags2 & KEEP_READ_PARS))
						SetMemorySize(pgr->par_formats, 0);
					
					par_ptr = (par_info_ptr) AppendMemory(pgr->par_formats, run_ctr, TRUE);

					while (run_ctr) {
				
						unpack_par_info(pg, &walker, par_ptr);
						++par_ptr;
						--run_ctr;
					}
	
					if (pgr->version < KEY_REVISION10) {
					
						par_ptr = (par_info_ptr) UseMemoryRecord(pgr->par_formats, 0, 0, FALSE);
						par_ptr->used_ctr += 1;
					}
	
					UnuseMemory(pg->par_formats);
				}
			}
			
			break;

		case font_info_key:
			if (pgr->io_mask_bits & IMPORT_TEXT_FORMATS_FLAG)
				break;

			if (!(pgr->shared_flags & SHARED_FORMATS)) {

				if ((run_ctr = *element_info) != 0) {
				
					if (!(pgr->flags2 & KEEP_READ_FONTS))
						SetMemorySize(pgr->fonts, 0);
					
					fonts = (font_info_ptr) AppendMemory(pgr->fonts, run_ctr, TRUE);
				
					while (run_ctr) {
				
						unpack_font_info(pg, &walker, fonts);
						++fonts;
						--run_ctr;
					}
					
					UnuseMemory(pg->fonts);
				}
			}
			
			break;

		case vis_shape_key:
		
			if (pgr->flags2 & NO_READVIS_BIT)
				break;

			if (!(pgr->shared_flags & SHARED_VIS_AREA)) {

				SetMemorySize(pgr->vis_area, *element_info);
				pgUnpackShape(&walker, pgr->vis_area);
				
				if (pgr->version < KEY_REVISION7) {
				 	rectangle_ptr			vis_ptr;
	
				 	vis_ptr = (rectangle_ptr) UseMemory(pgr->vis_area);
				 	pgr->base_vis_origin = vis_ptr->top_left;
				 	UnuseMemory(pgr->vis_area);
				}
			}

			break;

		case page_shape_key:
			if (pgr->io_mask_bits & (IMPORT_CONTAINERS_FLAG | IMPORT_PAGE_INFO_FLAG))
				break;

			if (!(pgr->shared_flags & SHARED_PAGE_AREA)) {

				SetMemorySize(pgr->wrap_area, *element_info);
				pgUnpackShape(&walker, pg->wrap_area);
			}

			break;

		case exclude_shape_key:
			if (!(pgr->shared_flags & SHARED_EXCLUDE_AREA)) {

				SetMemorySize(pgr->exclude_area, *element_info);
				pgUnpackShape(&walker, pg->exclude_area);
			}
			break;
		
		case selections_key:
			SetMemorySize(pgr->select, *element_info + MINIMUM_SELECT_MEMSIZE);
			pgr->num_selects = (pg_short_t)(*element_info / 2);

			for (selections = (t_select_ptr) use_array(pgr->select, &general_ctr); general_ctr;
				++selections, --general_ctr) {
				
				selections->offset = pgUnpackNum(&walker);
				pgUnpackSelectPair(&walker, &selections->word_offsets);
				pgUnpackCoOrdinate(&walker, &selections->original_pt);
				selections->line = (pg_short_t)pgUnpackNum(&walker);
				selections->primary_caret = selections->secondary_caret = pgUnpackNum(&walker);
				
				if (pgr->version >= KEY_REVISION2)
					selections->secondary_caret = pgUnpackNum(&walker);

				selections->flags = (pg_short_t)pgUnpackNum(&walker);
			}
			
			UnuseMemory(pgr->select);
			
			break;

		case extra_struct_key:
			
			break;

		case applied_range_key:
			if (*element_info == 0) {
				
				if (pgr->applied_range)
					DisposeMemory(pgr->applied_range);
				pgr->applied_range = MEM_NULL;
				
				break;
			}
			else
			if (!pgr->applied_range)
				pgr->applied_range = MemoryAlloc(pgr->globals->mem_globals,
						sizeof(select_pair), *element_info, 4);
			else
				SetMemorySize(pgr->applied_range, *element_info);

			run_ctr = *element_info;
			for (pair_ptr = (select_pair_ptr) UseMemory(pg->applied_range); run_ctr;
					++pair_ptr, --run_ctr)
				pgUnpackSelectPair(&walker, pair_ptr);

			UnuseMemory(pgr->applied_range);

			break;
		
		case doc_info_key:
			if (pgr->io_mask_bits & IMPORT_PAGE_INFO_FLAG)
				break;

			pgr->doc_info.attributes = pgUnpackNum(&walker);
			pgr->doc_info.exclusion_inset = (short)pgUnpackNum(&walker);
			pgr->doc_info.repeat_slop = pgUnpackNum(&walker);
			
			pgUnpackCoOrdinate(&walker, &pgr->doc_info.repeat_offset);
			pgUnpackRect(&walker, &pgr->doc_info.print_target);
			pgUnpackRect(&walker, &pgr->doc_info.margins);
			
			pgr->doc_info.num_pages = (pg_short_t)pgUnpackNum(&walker);
			pgr->doc_info.ref_con = pgUnpackNum(&walker);

			pgUnpackNumbers(&walker, pgr->doc_info.future, PG_FUTURE, long_data);
			
			pgr->doc_info.max_chars_per_line = pgUnpackNum(&walker);
			pgr->doc_info.minimum_widow = (short)pgUnpackNum(&walker);
			pgr->doc_info.minimum_orphan = (short)pgUnpackNum(&walker);

			if (pgr->version >= KEY_REVISION9) {		// (RTF enhancement) TRS/OITC
				short	i;
				
				pgr->doc_info.section_attributes = pgUnpackNum(&walker);
				pgr->doc_info.scroll_inset = (short)pgUnpackNum(&walker);
				pgr->doc_info.caret_width_extra = (short)pgUnpackNum(&walker);
				pgUnpackRect(&walker, &pgr->doc_info.offsets);
				pgr->doc_info.start_page_num = (short)pgUnpackNum(&walker);
				pgr->doc_info.restart_page_num = (short)pgUnpackNum(&walker);
				pgr->doc_info.page_num_prefix = (short)pgUnpackNum(&walker);
				pgr->doc_info.page_num_sep = (pg_char)pgUnpackNum(&walker);
				pgr->doc_info.page_num_format = (pg_char)pgUnpackNum(&walker);
				pgUnpackCoOrdinate(&walker, &pgr->doc_info.page_num_loc);
				pgr->doc_info.hyph_hot_zone = pgUnpackNum(&walker);
				pgr->doc_info.consecutive_hyph = (short)pgUnpackNum(&walker);
				pgr->doc_info.start_line_num = (short)pgUnpackNum(&walker);
				pgr->doc_info.restart_line_num = (short)pgUnpackNum(&walker);
				pgr->doc_info.line_increment = (short)pgUnpackNum(&walker);
				pgr->doc_info.line_num_loc = pgUnpackNum(&walker);
				pgr->doc_info.line_num_width = pgUnpackNum(&walker);
				pgr->doc_info.rev_format = (pg_char)pgUnpackNum(&walker);
				pgr->doc_info.rev_bar = (pg_char)pgUnpackNum(&walker);
				pgr->doc_info.num_of_cols = (short)pgUnpackNum(&walker);
				pgr->doc_info.gutter = pgUnpackNum(&walker);
				pgr->doc_info.dflt_gap = pgUnpackNum(&walker);
				for (i = 0; i < MAX_COLS; i++) {
					pgr->doc_info.col_info[i].width = pgUnpackNum(&walker);
					pgr->doc_info.col_info[i].gap = pgUnpackNum(&walker);
				}

			if (pgr->version >= KEY_REVISION10) {		// (RTF enhancement) TRS/OITC

				pgr->doc_info.version = pgUnpackNum(&walker);
				pgr->doc_info.vern = pgUnpackNum(&walker);
				pgr->doc_info.creationtime = pgUnpackNum(&walker);
				pgr->doc_info.revisiontime = pgUnpackNum(&walker);
				pgr->doc_info.printtime = pgUnpackNum(&walker);
				pgr->doc_info.backuptime = pgUnpackNum(&walker);
				pgr->doc_info.edittime = pgUnpackNum(&walker);
				pgr->doc_info.id = pgUnpackNum(&walker);
	
				input_opt_character_ref(pg->globals->mem_globals, &walker, &pgr->doc_info.title);
				input_opt_character_ref(pg->globals->mem_globals, &walker, &pgr->doc_info.subject);
				input_opt_character_ref(pg->globals->mem_globals, &walker, &pgr->doc_info.author);
				input_opt_character_ref(pg->globals->mem_globals, &walker, &pgr->doc_info.wp_operator);
				input_opt_character_ref(pg->globals->mem_globals, &walker, &pgr->doc_info.keywords);
				input_opt_character_ref(pg->globals->mem_globals, &walker, &pgr->doc_info.comment);
				input_opt_character_ref(pg->globals->mem_globals, &walker, &pgr->doc_info.doccomm);
				}
			}
			
			pgr->doc_info.gutter_color = pgUnpackNum(&walker);
			pgr->doc_info.page_borders = pgUnpackNum(&walker);

			break;

		case containers_key:
			if (pgr->io_mask_bits & IMPORT_CONTAINERS_FLAG)
				break;
			
		case exclusions_key:
			if (key == containers_key)
				general_ref = pgr->containers;
			else
				general_ref = pgr->exclusions;

			SetMemorySize(general_ref, *element_info);
			
			if (key == exclusions_key && (pgr->flags2 & HAS_PG_FRAMES_BIT)) {
				memory_ref PG_FAR 	*frames;
				long				num_frames;
				
				frames = (memory_ref *) UseMemory(general_ref);
				num_frames = *element_info;
				
				while (num_frames) {
					
					*frames = pgUnpackFrame(pgr, &walker);
					++frames;
					--num_frames;
				}
				
				UnuseMemory(general_ref);
				break;
			}
			
			pgUnpackNumbers(&walker, UseMemory(general_ref), (short)*element_info, (short)long_data);
			UnuseMemory(general_ref);
			break;
		
		case globals_key:
			if (aux_data) {
				register pg_globals_ptr		globals;


				globals = (pg_globals_ptr) aux_data;

				pgUnpackNumbers(&walker, &globals->line_wrap_char, SHORT_GLOBAL_CHARS, short_data);
				pgUnpackPtrBytes(&walker, (pg_bits8_ptr)globals->hyphen_char);
				pgUnpackColor(&walker, &globals->def_bk_color);
				pgUnpackColor(&walker, &globals->trans_color);
			}

			break;

		case exception_key:
			if ( (*element_info == NO_HANDLER_ERR) || (*element_info == GLOBALS_MISMATCH_ERROR))
				*element_info = NO_ERROR;

			break;
		
		case named_styles_key:
			if (pgr->io_mask_bits & IMPORT_STYLESHEETS_FLAG)
				break;

			if (!(pgr->shared_flags & SHARED_FORMATS)) {
				named_stylesheet_ptr		named_styles;

				if ((run_ctr = *element_info) != 0) {

					if (!(pgr->flags2 & KEEP_READ_STYLES))
						SetMemorySize(pgr->named_styles, 0);

					named_styles = (named_stylesheet_ptr) AppendMemory(pgr->named_styles, run_ctr, TRUE);
				
					while (run_ctr) {
						
						unpack_fontsize_text(pgr, &walker, named_styles->name);
						named_styles->stylesheet_id = (short)pgUnpackNum(&walker);
						named_styles->par_stylesheet_id = (short)pgUnpackNum(&walker);

						++named_styles;
						--run_ctr;
					}

					UnuseMemory(pg->named_styles);
				}
			}
			
			break;

		case par_exclusions_key:
			if (pgr->version >= KEY_REVISION15)
				pgUnpackStyleRun(&walker, pgr->par_exclusions, *element_info);

			break;
		
		case hyperlink_key:
		case hyperlink_target_key:
			{
				memory_ref		hyperlinks;
				ht_callback		callback;

				if (key == hyperlink_key) {
				
					hyperlinks = pgr->hyperlinks;
					callback = pgStandardSourceCallback;
				}
				else {
				
					hyperlinks = pgr->target_hyperlinks;
					callback = pgStandardTargetCallback;
				}

				pgUnpackHyperlinks(pgr, &walker, callback, *element_info, hyperlinks);
			}
			
			break;
	}

	UnuseMemory(walker.data_ref);

	return	TRUE;
}



/* pgReadDoc reads one or more data portions from the target file specified in
filemap (which contains machine-specific items).  The actual I/O is performed
by the read_proc (if non-NULL) or the standard file_io_proc for reading (whose
definition is in defaultprocs.h and code in machinespecific.h). If keys is non-
NULL, num_keys pg_file_key items in the pointer are input;  if keys is NULL
all available handlers are called.  The function result is zero if successful,
or the appropriate error code.  */

/* pgReadDoc reads one or more portions of a "file" into pg.  */

PG_PASCAL (pg_error) pgReadDoc (pg_ref pg, size_t PG_FAR *file_position, const pg_file_key_ptr keys,
		pg_short_t num_keys, file_io_proc read_proc, file_ref filemap)
{
	volatile paige_rec_ptr			pg_rec;
	volatile paige_rec_ptr			original_pg;
	volatile pgm_globals_ptr		mem_globals;
	pg_handler_ptr		handler;
	pg_handler			header_handler;
	file_io_proc		proc_to_read, data_proc;
	void PG_FAR			*aux_data_ptr;
	size_t				data_size, file_size, num_keys_read;
	long				element_info, aux_refcon;
	select_pair			cache_range;
	pg_file_key			data_key;
	pg_boolean			skip_all_keys, wait_terminated;
	pg_error			result;
	volatile memory_ref	handlers_to_use = MEM_NULL;
	volatile memory_ref	data_ref = MEM_NULL;
	volatile pg_ref		aux_pg = MEM_NULL;
	volatile memory_ref	doc_globals = MEM_NULL;
	
	pg_rec = original_pg = (paige_rec_ptr) UseMemory(pg);

	mem_globals = pg_rec->globals->mem_globals;
	file_size = 0;
	aux_refcon = 0;
	wait_terminated = FALSE;

	PG_TRY(mem_globals) {
		
		if (!pg_rec->globals->file_handlers)
			pgInitStandardHandlers(pg_rec->globals);
		
		handlers_to_use = pgBuildHandlerList(pg_rec->globals, keys, num_keys);

		pgPushMemoryID(pg_rec);
		
		pgInvalCharLocs(pg_rec);
		
		if (!(proc_to_read = read_proc))
			proc_to_read = pgStandardReadProc;

		data_ref = MemoryAlloc(mem_globals, 1, 0, 0);

		data_size = sizeof(long);
		if (result = proc_to_read(&file_size, io_get_eof, file_position, &data_size, filemap))
			result = pgDoExceptionKey(pg_rec, handlers_to_use, result, FALSE, data_ref);

		if ((!result) && (*file_position >= file_size))
			result = EOF_ERR;
		
		pgFailError(mem_globals, result);

		if (file_size) {

			result = NO_HANDLER_ERR;
			skip_all_keys = FALSE;
			num_keys_read = 0;

			pg_rec->procs.wait_proc(pg_rec, open_wait, 0, file_size);
			
			while (*file_position < file_size) {
	
				if (result = pgReadKey(proc_to_read, file_position, filemap,
					&data_key, &data_size, &element_info))
					result = pgDoExceptionKey(pg_rec, handlers_to_use, result, FALSE, data_ref);
				
				pgFailError(mem_globals, result);
				
				if (data_key == pg_eof_key) {

					*file_position += data_size;
					break;
				}

				if (!num_keys_read)
					if (data_key > paige_key)
						pgFailError(mem_globals, BAD_TYPE_ERR);

				++num_keys_read;
				
				if ((data_key == paige_key) && element_info) { /* auxillary pg_ref */
					
					skip_all_keys = FALSE;

					if (aux_pg) {
						
						fix_pg_from_done_handlers(pg_rec, handlers_to_use);
						UnuseMemory(aux_pg);
						pg_rec = original_pg;
						aux_pg = MEM_NULL;
					}
					
					if (handler = pgFindHandlerFromKey(handlers_to_use, aux_pg_key, NULL)) {
#ifdef THINK_C
						//Fixed for compiler problems
						pg_ref	aux_pg2;
						
						handler->read_handler(pg_rec, aux_pg_key, MEM_NULL, &element_info,
							&aux_pg2, &data_size);
						aux_pg = aux_pg2;
#else
						handler->read_handler(pg_rec, aux_pg_key, MEM_NULL, &element_info,
							(void PG_FAR *)&aux_pg, &data_size);
#endif						
						
						UnuseMemory(handlers_to_use);
					}
					
					if (aux_pg) {
					
						pg_rec = (volatile paige_rec_ptr) UseMemory(aux_pg);
						pgPushMemoryID(pg_rec);
					}
					else
						skip_all_keys = TRUE;
				}
				
				if (skip_all_keys)
					handler = NULL;
				else
				if (data_key == pg_signature) {
					
					pgInitOneHandler(&header_handler, data_key);
					handler = (pg_handler_ptr)&header_handler;
					UseMemory(handlers_to_use);
				}
				else
					handler = pgFindHandlerFromKey(handlers_to_use, data_key, NULL);

				if (handler) {

					if (!(data_proc = read_proc))
						data_proc = handler->read_data_proc;

					if ((data_key == text_key) && pg_rec->cache_file) {
						size_t		hex_header_size, hex_position;
						pg_bits8	cache_buffer[6];
						short		hex_index;

						// Version 2.0, cache read setup
						
						hex_header_size = 6;
						hex_position = *file_position;
						cache_range.begin = *file_position;
						*file_position += data_size;

						data_proc((void PG_FAR *)cache_buffer, io_data_direct, &hex_position, &hex_header_size, filemap);
						
						for (hex_index = 0; hex_index < 6; ++hex_index) {
							
							cache_range.begin += 1;
							data_size -= 1;

							if (cache_buffer[hex_index] == ',')
								break;
						}

						result = NO_ERROR;
						cache_range.end = cache_range.begin + data_size;
						aux_data_ptr = (void PG_FAR *)&cache_range;
					}
					else {
						
						aux_data_ptr = (void PG_FAR *)&aux_refcon;
						result = data_proc((void PG_FAR *) data_ref, io_data_indirect, file_position, &data_size, filemap);
					}

					if (result)
						result = pgDoExceptionKey(pg_rec, handlers_to_use, result, FALSE, data_ref);
					
					pgFailError(mem_globals, result);
					
					for (;;) {
						pg_boolean			done;
						
						if (data_key == globals_key) {
							pg_globals_ptr	temp_globals;

							doc_globals = MemoryAlloc(mem_globals, sizeof(pg_globals), 1, 0);
							temp_globals = (pg_globals_ptr) UseMemory(doc_globals);
							pgBlockMove(pg_rec->globals, temp_globals, sizeof(pg_globals));
							
							done = handler->read_handler(pg_rec, globals_key, data_ref,
									&element_info, temp_globals, &data_size);
							
							if (!pgEqualStruct(pg_rec->globals, temp_globals, sizeof(pg_globals))) {
								
								mem_globals->last_message = (long) doc_globals;
								result = pgDoExceptionKey(pg_rec, handlers_to_use, GLOBALS_MISMATCH_ERROR, FALSE, data_ref);
								
								pgFailError(mem_globals, result);
								
								mem_globals->last_message = 0;
							}
							
							UnuseAndDispose(doc_globals);
							doc_globals = MEM_NULL;
						}
						else
							done = handler->read_handler(pg_rec, data_key, data_ref,
									&element_info, aux_data_ptr, &data_size);

						if (done)
							break;
						
						if (data_key == pg_signature)
							pgFailError(mem_globals, BAD_TYPE_ERR);
					}
					
					UnuseMemory(handlers_to_use);
				}
				else {

					result = pgDoExceptionKey(pg_rec, handlers_to_use, NO_HANDLER_ERR, FALSE, data_ref);
					pgFailError(mem_globals, result);

				/* If not wanted, skip it.  */
				
					*file_position += data_size;
				}
				
				pg_rec->procs.wait_proc(pg_rec, open_wait, *file_position, file_size);
				wait_terminated = (pg_boolean)(*file_position == file_size);
			}
			
			if (aux_pg) {
				
				fix_pg_from_done_handlers(pg_rec, handlers_to_use);
				
				UnuseMemory(aux_pg);
				pg_rec = original_pg;
				pgPushMemoryID(pg_rec);
				
				aux_pg = MEM_NULL;
			}
			
			fix_pg_from_done_handlers(pg_rec, handlers_to_use);

			DisposeMemory(handlers_to_use);
			DisposeMemory(data_ref);
		}

		if (!wait_terminated)
			pg_rec->procs.wait_proc(pg_rec, open_wait, file_size, file_size);

		pg_rec->flags &= (~INIT_IO_BIT);
		pgPopMemoryID(pg_rec);

		pg_rec->port.clip_info.change_flags |= CLIP_DEVICE_CHANGED;
		UnuseMemory(pg);
	}
	
	PG_CATCH {
		
		if (aux_pg) {
			
			UnuseMemory(aux_pg);
			aux_pg = MEM_NULL;
			pg_rec = original_pg;
		}
		
		if (mem_globals->last_error == SOFT_EOF_ERR)
			mem_globals->last_error = 0;
		else {
			pg_error		original_error;
			
			original_error = mem_globals->last_error;

			if (handlers_to_use) {
				
				fix_pg_from_done_handlers(pg_rec, handlers_to_use);
				DisposeFailedMemory(handlers_to_use);
			}
			
			if (data_ref)
				DisposeFailedMemory(data_ref);
			
			if (doc_globals)
				DisposeFailedMemory(doc_globals);

			pgPopMemoryID(pg_rec);
			
			pg_rec->procs.wait_proc(pg_rec, open_wait, file_size, file_size);
			wait_terminated = TRUE;
			pg_rec->flags &= (~INIT_IO_BIT);

			UnuseAllFailedMemory(mem_globals, pg_rec->mem_id);
			mem_globals->last_error = original_error;
			
			if (original_error == NO_MEMORY_ERR)
				pgFailure(mem_globals, NO_MEMORY_ERR, 0);

			return	mem_globals->last_error;
		}
	}

	PG_ENDTRY;

	return	NO_ERROR;
}


/* pgReadKey reads the next key by calling the io_proc to read the appropriate
key header, beginning at *position. The key returned in *key; data_size will
be set to the size of the data while *element_size will be the element_info. The
next file positions should be the real data.  The function result is a file
error (or NO_ERROR). Either data_size or element_size can be NULL (which just
returns the key and error result).  */

PG_PASCAL (pg_error) pgReadKey (file_io_proc io_proc, size_t PG_FAR *position,
	file_ref filemap, pg_file_key_ptr key, size_t PG_FAR *data_size,
	long PG_FAR *element_info)
{
	pg_key_header			header;
	size_t					read_size;
	register short			index;
	register pg_file_key	the_key;
	register long			value_result;
	pg_error				error;

	read_size = PG_HEADER_SIZE;
	
	if (error = io_proc(header, io_data_direct, position, &read_size, filemap))
		return	error;

	the_key = 0;

	for (index = 0; index < KEY_HEADER_SIZE; ++index) {
		
		the_key <<= 4;
		the_key |= pgHexToByte(header[index]);
	}
	
	*key = the_key;

	if (data_size) {
		
		for (index = 0, value_result = 0; index < DATA_HEADER_SIZE; ++index) {
		
			value_result <<= 4;
			value_result |= pgHexToByte(header[index + KEY_HEADER_SIZE]);
		}
		
		*data_size = value_result;
	}

	if (element_info) {
		for (index = 0, value_result = 0; index < ELEMENT_HEADER_SIZE; ++index) {
		
			value_result <<= 4;
			value_result |= pgHexToByte(header[index + KEY_HEADER_SIZE + DATA_HEADER_SIZE]);
		}
		
		*element_info = value_result;
	}

	return	NO_ERROR;
}


/* pgCacheReadDoc is the same as pgReadDoc() except the text is cached (not read) until its
text portions are visible. This allows huge files to open and display quickly. All params
are the same, the only difference is that filemap MUST remain open until the pg_ref is disposed
or the file is saved as a new file reference. */

PG_PASCAL (pg_error) pgCacheReadDoc (pg_ref pg, size_t PG_FAR *file_position, const pg_file_key_ptr keys,
		pg_short_t num_keys, file_io_proc read_proc, file_ref filemap)
{
	paige_rec_ptr		pg_rec;
	file_io_proc		use_read_proc;
	pg_error			result;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	
	if ((use_read_proc = read_proc) == NULL)
		use_read_proc = pgStandardReadProc;
	
	pg_rec->cache_file = filemap;
	pg_rec->cache_read_proc = use_read_proc;

	result = pgReadDoc(pg, file_position, keys, num_keys, read_proc, filemap);
	
	if (result == NO_ERROR) {
		pgm_globals_ptr		mem_globals;
		pg_ref PG_FAR		*refs;
		long				index;
		
		mem_globals = pg_rec->globals->mem_globals;
		
		if ((index = pgInCacheList(mem_globals, pg)) == 0) {
			
			if (mem_globals->freemem_info == MEM_NULL) {
			
				mem_globals->freemem_info = MemoryAlloc(mem_globals, sizeof(pg_ref), 0, 0);
				SetMemoryPurge(mem_globals->freemem_info, NO_PURGING_STATUS, FALSE);
			}

			refs = (pg_ref *) AppendMemory(mem_globals->freemem_info, 1, FALSE);
			*refs = pg;
			UnuseMemory(mem_globals->freemem_info);
		}
	}

	UnuseMemory(pg);

	return	result;
}


/* pgVerifyFile verifies that filemap file is a real PAIGE file. If so, NO_ERROR is returned. */

PG_PASCAL (pg_error) pgVerifyFile (file_ref filemap, file_io_proc io_proc, size_t position)
{
	pg_bits8			header[PG_HEADER_SIZE];
	file_io_proc		read_proc;
	size_t				file_position, data_size;
	pg_error			error = NO_ERROR;
	
	if (!(read_proc = io_proc))
		read_proc = pgStandardReadProc;
	
	file_position = position;
	data_size = PG_HEADER_SIZE;

	if (!(error = read_proc((void PG_FAR *)header, io_data_direct, &file_position, &data_size, filemap))) {
	
		if (!equal_constants((pg_bits8_ptr)"FFFF00000006", header, PG_VERIFY_SIZE))
			error = BAD_TYPE_ERR;
		else {
	
			// So far so good...

			data_size = PG_VERIFY_BYTE_SIZE;
		
			if (!(error = read_proc((void PG_FAR *)header, io_data_direct, &file_position, &data_size, filemap))) {
			
				if (header[0] != VERIFY_BYTE1
					|| header[1] != VERIFY_BYTE2
					|| header[2] != VERIFY_BYTE3
					|| header[3] != VERIFY_BYTE4
					|| header[4] != VERIFY_BYTE5
					|| header[5] != VERIFY_BYTE6)
					error = BAD_TYPE_ERR;
			}
		}
	}
	
	read_proc((void PG_FAR *)NULL, io_set_fpos, &position, NULL, filemap);
	
	return	error;
}
	

/* pgUnpackTextBlock recovers a single textblock. */

PG_PASCAL (void) pgUnpackTextBlock (paige_rec_ptr pg, pack_walk_ptr walker,
		text_block_ptr block, pg_boolean include_text, memory_ref home_subref)
{
	long			version = pg->version;

	block->begin = pgUnpackNum(walker);
	block->end = pgUnpackNum(walker);
	block->flags = (pg_short_t)pgUnpackNum(walker);
	block->first_line_num = pgUnpackNum(walker);
	block->num_lines = (pg_short_t)pgUnpackNum(walker);
	block->first_par_num = pgUnpackNum(walker);
	block->num_pars = (pg_short_t)pgUnpackNum(walker);
	unpack_refcon(pg, walker, &block->user_var.refcon);

	pgUnpackRect(walker, &block->bounds);
	unpack_point_starts(walker, &block->end_start, 1);
	
	if (!block->text && (!pg->cache_file || include_text)) {
	
		block->text = MemoryAlloc(pg->globals->mem_globals, sizeof(pg_char), 0, 64);
		SetMemoryPurge(block->text, TEXT_PURGE_STATUS, FALSE);
	}

	if (!block->lines)
		block->lines = MemoryAllocClear(pg->globals->mem_globals, sizeof(point_start), 2, 8);

	SetMemoryPurge(block->lines, LINE_PURGE_STATUS, FALSE);

	if (!block->subref_list)
		block->subref_list = MemoryAlloc(pg->globals->mem_globals,
				sizeof(pg_subref), 0, 8);

	if (include_text) {
		long		input_byte_size;
		
		input_byte_size = block->end - block->begin;
		SetMemorySize(block->text, input_byte_size);
		pgUnpackBytes(walker, block->text);
	}

#ifdef DOING_EQ
	version = KEY_REVISION19;
#endif

	if (version >= KEY_REVISION19) {
	
		if (pgUnpackSubRefs(pg, walker, block, home_subref)) {
			
			block->flags |= NEEDS_CALC;
			pgInvalCharLocs(pg);
		}
	}
	else
		pgUnpackNum(walker);

/* Revision for 2.1, include source platform for cache-read conversion. */

	block->file_os = pgGetOSConstant(pg->platform);
}


/* pgUnpackData performs the reverse of pgPackData, except a starting offset
is specified;  on return, target_data is set to the appropriate size and
*starting_offset is updated to ending position (which could be a partition if
if you did multiple pgPackData calls).  */

PG_PASCAL (void) pgUnpackData (memory_ref src_data, memory_ref target_data,
		long PG_FAR *starting_offset, pg_short_t data_type)
{
	pack_walk			walker;
	pg_short_t			target_rec_size, increment_size;
	long				target_byte_size;

	pgSetupPacker(&walker, src_data, *starting_offset);
	
	if (!walker.remaining_ctr) {
	
		SetMemorySize(target_data, 0);
		--walker.transfered;
	}
	else
	if ((walker.data[0] & CODE_MASK) == terminator_data)
		SetMemorySize(target_data, 0);
	else {
	
		if (data_type == byte_data)
			pgUnpackBytes(&walker, target_data);
		else {
	
			target_byte_size = pgGetUnpackedSize(&walker);
			target_rec_size = GetMemoryRecSize(target_data);
			
			if (data_type == short_data)
				increment_size = sizeof(short);
			else
				increment_size = sizeof(long);
			
			SetMemorySize(target_data, target_byte_size / target_rec_size);
			pgUnpackNumbers(&walker, UseMemory(target_data), (short)(target_byte_size / increment_size),
					(short)data_type);
			
			UnuseMemory(target_data);
		}
	}

	UnuseMemory(src_data);
	
	*starting_offset = walker.transfered + 1;
}


/* pgUnpackNum is the function that returns the next packed numerical value in the
packed stream. The caller must know it is a number (if it's not, this function
returns zero and does not advance the pointers).  */

PG_PASCAL (long) pgUnpackNum (pack_walk_ptr in_data)
{
	register pg_bits8_ptr		data;
	register long				input_ctr, hex_ctr;
	long						result;
	pg_bits8					data_code, real_code;
	
	if (in_data->repeat_ctr) {
		
		--in_data->repeat_ctr;
		
		if (in_data->max_bytes) {
		
			if (in_data->last_code == short_data)
				in_data->max_bytes_ctr += sizeof(short);
			else
				in_data->max_bytes_ctr += sizeof(long);
		}

		return	in_data->last_value;
	}
	
	if (!in_data->remaining_ctr)
		return	0;

	if (in_data->max_bytes)
		if (in_data->max_bytes_ctr >= in_data->max_bytes)
			return	0;

	data = in_data->data;
	input_ctr = result = 0;
	
	real_code = data_code = *data;
	data_code &= CODE_MASK;

	if ((data_code == short_data) || (data_code == long_data)) {
		
		++data;
		++input_ctr;

		if (in_data->max_bytes) {

			if (data_code == short_data)
				in_data->max_bytes_ctr += sizeof(short);
			else
				in_data->max_bytes_ctr += sizeof(long);
		}

		if (real_code & REPEAT_LAST_VALUE) {

			result = in_data->last_value;
			
			if (real_code & REPEAT_LAST_N_TIMES) {
				
				in_data->repeat_ctr = (*data - 1);
				++input_ctr;
				++data;
			}
		}
		else {
			
			hex_ctr = pgUnpackHex(data, &result);
			data += hex_ctr;
			input_ctr += hex_ctr;
		}
	}

	in_data->last_code = data_code;
	in_data->last_value = result;
	in_data->data = data;
	in_data->transfered += input_ctr;
	in_data->remaining_ctr -= input_ctr;
	
	return	result; 
}


/* pgUnpackNumbers calls pgUnpackNum for qty times. */

PG_PASCAL (void) pgUnpackNumbers (pack_walk_ptr out_data, void PG_FAR *ptr, short qty, short data_code)
{
	register short			ctr;
	register short PG_FAR	*short_ptr;
	register long  PG_FAR	*long_ptr;

	if (data_code == short_data) {
		for (ctr = 0, short_ptr = (short *) ptr; ctr < qty; ++ctr)
			short_ptr[ctr] = (short)pgUnpackNum(out_data);
	}
	else {
		for (ctr = 0, long_ptr = (long *) ptr; ctr < qty; ++ctr)
			long_ptr[ctr] = pgUnpackNum(out_data);
	}
}


PG_PASCAL (void) pgUnpackMemoryRef (pg_globals_ptr pg_globals, pack_walk_ptr walker, memory_ref PG_FAR *ref)
{
	input_opt_character_ref(pg_globals->mem_globals,walker,ref);
}


/* pgUnpackBytes is the function that returns a byte stream (data code = byte_data). The
resulting data is filled in out_data. If the code is not byte_data the resulting
output is zero length.  The output length is returned as the function result.
NOTE: To just get the byte size -- to force a skip over of input, packed data --
pass NULL for out_data.  */

PG_PASCAL (long) pgUnpackBytes (pack_walk_ptr in_data, memory_ref out_data)
{
	pg_bits8_ptr	data;
	long			output_size, in_ctr, hex_size;

	data = in_data->data;
	output_size = in_ctr = 0;
	
	if (*data == byte_data) {
	
		++data;
		++in_ctr;
		
		hex_size = pgUnpackHex(data, &output_size);
		++hex_size;		/* (skips comma) */
		data += hex_size;
		in_ctr += hex_size;
		
		if (out_data) {
			long			size_of_ref;
			short			rec_size;
			
			size_of_ref = output_size;

			if ((rec_size = GetMemoryRecSize(out_data)) > 1) {
			
				size_of_ref = output_size / rec_size;
				
				if ((output_size % rec_size) != 0)
					size_of_ref += 1;
			}

			SetMemorySize(out_data, size_of_ref);
			pgBlockMove(data, UseMemory(out_data), output_size);
			UnuseMemory(out_data);
			
			if (in_data->max_bytes)
				in_data->max_bytes_ctr += output_size;
		}

		in_ctr += output_size;
		data += output_size;
	}
	
	in_data->data = data;
	in_data->transfered += in_ctr;
	in_data->remaining_ctr -= in_ctr;
	
	return	output_size;
}


/* This is same as pgUnpackBytes except data is unpacked directly to out_data ptr. */

PG_PASCAL (void) pgUnpackPtrBytes (pack_walk_ptr in_data, pg_bits8_ptr out_ptr)
{
	pg_bits8_ptr	data;
	long			output_size, in_ctr, hex_size;

	data = in_data->data;
	output_size = in_ctr = 0;
	
	if (*data == byte_data) {
		
		++data;
		++in_ctr;
		
		hex_size = pgUnpackHex(data, &output_size);
		++hex_size;		/* (skips comma) */
		data += hex_size;
		in_ctr += hex_size;
		
		pgBlockMove(data, out_ptr, output_size);

		in_ctr += output_size;
		data += output_size;
	}
	
	in_data->data = data;
	in_data->transfered += in_ctr;
	in_data->remaining_ctr -= in_ctr;
}


/* pgGetUnpackedPtr returns the raw byte pointer to the next data (which is assumed to
be a binary data byte stream). */

PG_PASCAL (pg_bits8_ptr) pgGetUnpackedPtr (pack_walk_ptr walker, long PG_FAR *bytesize)
{
	pg_bits8_ptr	data;
	long			output_size, in_ctr, hex_size;

	data = walker->data;
	output_size = in_ctr = 0;
	
	if (walker->remaining_ctr)
		if (*data == byte_data) {

		++data;
		++in_ctr;
		
		hex_size = pgUnpackHex(data, &output_size);
		++hex_size;		/* (skips comma) */
		data += hex_size;
		in_ctr += hex_size;
		
	}
	
	*bytesize = output_size;
	
	return	data;
}


/* pgGetUnpackedSize returns the **unpacked** size of a packed input contained in
walker. It walks through the stream, beginning at the current settings, and
figures out what the output result will be.  */

PG_PASCAL (long) pgGetUnpackedSize (pack_walk_ptr walker)
{
	pack_walk			temp_walker;
	register pg_bits8	next_code;
	long				result;

	pgBlockMove(walker, &temp_walker, sizeof(pack_walk));
	result = 0;

	for (;;) {
		
		if (temp_walker.repeat_ctr)
			next_code = (pg_bits8)temp_walker.last_code;
		else
			next_code = temp_walker.data[0] & CODE_MASK;

		if (next_code == terminator_data)
			break;

		switch (next_code) {
			
			case byte_data:
				result += pgUnpackBytes(&temp_walker, MEM_NULL);
				break;

			case short_data:
				pgUnpackNum(&temp_walker);
				result += sizeof(short);
				break;

			case long_data:
				pgUnpackNum(&temp_walker);
				result += sizeof(long);
				break;
			
			default:
				temp_walker.data[0] = terminator_data;
				break;
		}
	}

	return	result;
}



/* pgUnpackCoOrdinate a co_ordinate */

PG_PASCAL (void) pgUnpackCoOrdinate (pack_walk_ptr walker, co_ordinate_ptr point)
{
	point->v = pgUnpackNum(walker);
	point->h = pgUnpackNum(walker);
}


/* pgUnpackSelectPair a select_pair */

PG_PASCAL (void) pgUnpackSelectPair (pack_walk_ptr walker, select_pair_ptr pair)
{
	pair->begin = pgUnpackNum(walker);
	pair->end = pgUnpackNum(walker);
}


/* pgUnpackRect a rectangle */

PG_PASCAL (void) pgUnpackRect (pack_walk_ptr walker, rectangle_ptr r)
{
	pgUnpackCoOrdinate(walker, &r->top_left);
	pgUnpackCoOrdinate(walker, &r->bot_right);
}


/* pgUnpackColor unpacks a "color_value" record */

PG_PASCAL (void) pgUnpackColor (pack_walk_ptr walker, color_value PG_FAR *color)
{
	color->red = (pg_short_t)pgUnpackNum(walker);
	color->green = (pg_short_t)pgUnpackNum(walker);
	color->blue = (pg_short_t)pgUnpackNum(walker);
	color->alpha = (pg_short_t)pgUnpackNum(walker);
}



/* pgUnpackShape a shape_ref */

PG_PASCAL (void) pgUnpackShape (pack_walk_ptr walker, shape_ref the_shape)
{
	rectangle_ptr	rects;
	long			shape_size;
	
	shape_size = GetMemorySize(the_shape);
	for (rects = (rectangle_ptr) UseMemory(the_shape); shape_size; ++rects, --shape_size)
		pgUnpackRect(walker, rects);

	UnuseMemory(the_shape);
}


/* pgUnpackStyleRun unpacks the given style run in ref for elements number of
records. */

PG_PASCAL (void) pgUnpackStyleRun (pack_walk_ptr walker, memory_ref ref, long elements)
{
	register style_run_ptr			run_ptr;
	long							run_ctr;
	
	SetMemorySize(ref, elements);		/* = original # of recs */

	run_ptr = (style_run_ptr) use_array(ref, &run_ctr);

	while (run_ctr) {

		run_ptr->offset = pgUnpackNum(walker);
		run_ptr->style_item = (pg_short_t)pgUnpackNum(walker);
		
		++run_ptr;
		--run_ctr;
	}

	UnuseMemory(ref);
}



/****************************** Local Functions **************************/


/* To reduce code size a wee bit, this function gets called to do a "Use" on
a paige structure and return its size.  */

static void PG_FAR * use_array (memory_ref ref, long PG_FAR *num_recs)
{
	*num_recs = GetMemorySize(ref);
	return	UseMemory(ref);
}




/* This function unpacks 1 or more point starts. */

static void unpack_point_starts (pack_walk_ptr walker, point_start_ptr starts,
		pg_short_t num_starts)
{
	register point_start_ptr	line_starts;
	register pg_short_t			qty;
	
	for (qty = num_starts, line_starts = starts; qty; ++line_starts, --qty) {
		
		line_starts->offset = (pg_short_t)pgUnpackNum(walker);
		line_starts->extra = (short)pgUnpackNum(walker);
		line_starts->baseline = (short)pgUnpackNum(walker);
		line_starts->flags = (pg_short_t)pgUnpackNum(walker);
		line_starts->r_num = pgUnpackNum(walker);		
		pgUnpackRect(walker, &line_starts->bounds);
	}
}


/* This unpacks a list of tabs to an array of tab_stops */

static void unpack_tabs (paige_rec_ptr pg, pack_walk_ptr walker, tab_stop_ptr tabs, pg_short_t tab_qty)
{
	pg_short_t				num_tabs;
	register tab_stop_ptr	tabs_ptr;
	
	tabs_ptr = tabs;
	
	for (num_tabs = tab_qty; num_tabs; ++tabs_ptr, --num_tabs) {

		tabs_ptr->tab_type = pgUnpackNum(walker);
		tabs_ptr->position = pgUnpackNum(walker);
		tabs_ptr->leader = pgUnpackNum(walker);
		unpack_refcon(pg, walker, &tabs_ptr->ref_con);
	}
}


/* This function is called by pgReadDoc after calling all handlers. Its purpose
is to fix up things in case of partial handlers (e.g., style runs read but no
new text, text read but no line info, etc.).  */

static void fix_pg_from_done_handlers (paige_rec_ptr pg, memory_ref handlers)
{
	register text_block_ptr			block;
	style_info_ptr					styles;
	font_info_ptr					fonts;
	par_info_ptr					pars;
	pg_handler_ptr					init_handler, line_handler, text_handler;
	register long					text_size;
	size_t							data_size;
	long							element;
	short							font_index, font_qty, saved_resolution, actual_resolution;
	pg_short_t						qty;

	pgScaleGrafDevice(pg);

	pg->stable_caret.h = pg->stable_caret.v = 0;

	text_size = pg->t_length;

	fix_style_run(pg, pg->t_style_run, pg->t_formats);
	fix_style_run(pg, pg->par_style_run, pg->par_formats);
	fix_hyperlink_run(pg, pg->hyperlinks);
	fix_hyperlink_run(pg, pg->target_hyperlinks);

/* Handle possible document resolution mismatch in "resolution" field */
	
	if (!pg->resolution)
		pg->resolution = pg->port.resolution;
	
	saved_resolution = pgHiWord(pg->resolution);
	actual_resolution = pgHiWord(pg->port.resolution);

	if (actual_resolution != saved_resolution)
		pgScaleDocument(pg, actual_resolution, saved_resolution, (pg_boolean)(!(pg->io_mask_bits & IMPORT_PAGE_INFO_FLAG)));

	pg->resolution = pg->port.resolution;

	if (line_handler = pgFindHandlerFromKey(handlers, line_key, NULL))
		UnuseMemory(handlers);

	if (text_handler = pgFindHandlerFromKey(handlers, text_key, NULL))
		UnuseMemory(handlers);

	if ((!line_handler) || (!text_handler)) {
		pg_short_t			actual_block_qty;
		t_select_ptr		selections;
		
		if (!line_handler) {

			qty = (pg_short_t)GetMemorySize(pg->select);
			
			for (selections = (t_select_ptr) UseMemory(pg->select); qty; ++selections, --qty)
				selections->flags |= SELECTION_DIRTY;
				
				UnuseMemory(pg->select);
		}
		
		qty = (pg_short_t)GetMemorySize(pg->t_blocks);
		actual_block_qty = 0;

		for (block = (text_block_ptr) UseMemory(pg->t_blocks); qty; ++block, --qty) {
			
			if (!line_handler)
				block->flags |= LINES_PURGED;
			
			if (block->end <= text_size)
				++actual_block_qty;
			else {
				
				if (block->lines)
					DisposeMemory(block->lines);
				if (block->text)
					DisposeMemory(block->text);
			}
		}

		UnuseMemory(pg->t_blocks);
		SetMemorySize(pg->t_blocks, actual_block_qty);
	}

	init_handler = pgFindHandlerFromKey(handlers, format_init_key, NULL);

	if (init_handler)
		init_handler->read_handler(pg, format_init_key, (memory_ref) init_start_verb,
					NULL, NULL, NULL);

// Version 1.4, only initialize fonts if not "importing":

	element = 0;
	data_size = sizeof(font_info);
    qty = (pg_short_t)GetMemorySize(pg->fonts);
    font_qty = (short)qty;

	for (fonts = (font_info_ptr) UseMemory(pg->fonts); qty; ++fonts, ++element, --qty) {
		
		fonts->environs &= (~(FONT_GOOD | FONT_USES_ALTERNATE));	/* Default to font not good (yet) */
		
		if ((fonts->platform & GENERAL_PLATFORM_MASK) != GENERAL_PLATFORM)
		{
			fonts->machine_var[PG_PREV_LANG] = fonts->language;			/* Save previous language on import as it will get cratered by font_proc */
			fonts->environs |= FONTS_TEXT_UNKNOWN;						/* It maybe unknown so remember so on save we can restore */
		}
		
		if (!pg->import_control) {
		
			pg->procs.font_proc(pg, fonts);
	
			if (init_handler)
				init_handler->read_handler(pg, format_init_key, (memory_ref) init_font_verb,
						&element, fonts, &data_size);
		}
	}

	UnuseMemory(pg->fonts);

	element = 0;
	data_size = sizeof(style_info);
    
    fonts = (font_info_ptr) UseMemory(pg->fonts);
 
	for (styles = (style_info_ptr) UseMemory(pg->t_formats), qty = (pg_short_t)GetMemorySize(pg->t_formats);
		qty; ++styles, ++element, --qty) {
		
		styles->class_bits &= (~EMBED_READ_BIT);
		
		if (init_handler)
			init_handler->read_handler(pg, format_init_key, (memory_ref) init_style_verb,
					&element, styles, &data_size);

		styles->machine_var = styles->machine_var2 = 0;
		
		if ((font_index = styles->font_index) == DEFAULT_FONT_INDEX)
			font_index = 0;
        else
        if (font_index >= font_qty) {
            
            font_index = font_qty - 1;
        	styles->font_index = font_index;       	
        }

		if (!pg->import_control)
			styles->procs.init(pg, styles, &fonts[font_index]);
	}

	UnuseMemory(pg->t_formats);
	UnuseMemory(pg->fonts);

	element = 0;
	data_size = sizeof(par_info);

	if (init_handler) {
		
		for (pars = (par_info_ptr) UseMemory(pg->par_formats), qty = (pg_short_t)GetMemorySize(pg->par_formats);
			qty; ++pars, ++element, --qty)
			init_handler->read_handler(pg, format_init_key, (memory_ref) init_par_verb,
				&element, pars, &data_size);
	
		UnuseMemory(pg->par_formats);
		UnuseMemory(handlers);
	}

	if (pg->platform != PAIGE_PLATFORM)
		pgInvalSelect(pg->myself, 0, pg->t_length);

	if (init_handler) 
		init_handler->read_handler(pg, format_init_key, (memory_ref) init_end_verb,
				NULL, NULL, NULL);
	
	if (!pg->cache_file)
		convert_for_unicode(pg);

	pg->version = PAIGE_VERSION;	/* Upgrade version now that its read */

#ifndef UNICODE
	pg->flags2 &= (~(UNICODE_TEXT | UNICODE_SAVED | SAVE_AS_UNICODE));
#else
	pg->flags2 |= UNICODE_TEXT;
#endif

	if (!pg->import_control)
   	 	pg->platform = PAIGE_PLATFORM;
   	 
   	 if (pg->url_list_ref) {
   	 	
   	 	DisposeMemory(pg->url_list_ref);
   	 	pg->url_list_ref = MEM_NULL;
   	 }
}


/* fix_style_run walks through a style_run to detect any problems and fix it up. For
example, "run" might be pg->t_style_run and "styles" will be pg->t_formats. A legitimate
reason for a "bad" run would be the application's ommision of the style run key. */

static void fix_style_run (paige_rec_ptr pg, memory_ref run, memory_ref styles)
{
	style_run_ptr			run_ptr;
	pg_short_t				style_qty;
	long					run_qty;
	
	run_qty = GetMemorySize(run);
	style_qty = (pg_short_t)GetMemorySize(styles);
	run_ptr = (style_run_ptr) UseMemory(run);
	
	while (run_qty) {
		
		if (run_ptr->style_item >= style_qty)
			run_ptr->style_item = 0;			// Use default if "bad" style element!
		
		++run_ptr;
		--run_qty;
	}

	--run_ptr;
	run_ptr->offset = pg->t_length + ZERO_TEXT_PAD;
	
	UnuseMemory(run);
}


/* fix_hyperlink_run adjusts the ending point of the hypertext link(s). */

static void fix_hyperlink_run (paige_rec_ptr pg, memory_ref run)
{
	pg_hyperlink_ptr		hyperlinks;
	long					qty;
	
	qty = GetMemorySize(run);
	hyperlinks = (pg_hyperlink_ptr) UseMemoryRecord(run, qty - 1, 0, TRUE);
	hyperlinks->applied_range.end = hyperlinks->applied_range.begin = pg->t_length + ZERO_TEXT_PAD;
	UnuseMemory(run);
}

/* This unpacks pg_indents record */

static void unpack_indents (pack_walk_ptr walker, pg_indents_ptr indents)
{
	indents->left_indent = pgUnpackNum(walker);
	indents->right_indent = pgUnpackNum(walker);
	indents->first_indent = pgUnpackNum(walker);
}


/* This unpacks a style_info record */

static void unpack_style_info (paige_rec_ptr pg, pack_walk_ptr walker,
		style_info_ptr info)
{
	register style_info_ptr		style_ptr;
	short						num_styles;
	
	style_ptr = info;
	
	pgFillBlock(style_ptr, sizeof(style_info), 0);

	style_ptr->font_index = (short)pgUnpackNum(walker);
	style_ptr->char_bytes = (short)pgUnpackNum(walker);
	style_ptr->max_chars = (short)pgUnpackNum(walker);
	style_ptr->ascent = (short)pgUnpackNum(walker);
	style_ptr->descent = (short)pgUnpackNum(walker);
	style_ptr->leading = (short)pgUnpackNum(walker);
	style_ptr->shift_verb = (short)pgUnpackNum(walker);
	style_ptr->class_bits = pgUnpackNum(walker);
	style_ptr->style_sheet_id = pgUnpackNum(walker);

	pgUnpackColor(walker, &style_ptr->fg_color);
	pgUnpackColor(walker, &style_ptr->bk_color);

	if (pg->version < KEY_REVISION6)				//е TRS/OITC
		style_ptr->machine_var = pgUnpackNum(walker);
	style_ptr->char_width = pgUnpackNum(walker);
	style_ptr->point = pgUnpackNum(walker);
	style_ptr->left_overhang = pgUnpackNum(walker);
	style_ptr->right_overhang = pgUnpackNum(walker);
	style_ptr->top_extra = pgUnpackNum(walker);
	style_ptr->bot_extra = pgUnpackNum(walker);
	style_ptr->space_extra = pgUnpackNum(walker);
	style_ptr->char_extra = pgUnpackNum(walker);
	style_ptr->user_id = pgUnpackNum(walker);
	style_ptr->user_data = pgUnpackNum(walker);
	style_ptr->user_data2 = pgUnpackNum(walker);
	if (pg->version < KEY_REVISION6)				//е TRS/OITC
		style_ptr->class_bits = pgUnpackNum(walker);
	if (pg->version >= KEY_REVISION8A) {				// (RTF enhancement) TRS/OITC
		style_ptr->time_stamp = pgUnpackNum(walker);	// Time stamp is in Paige Format
	}

	unpack_refcon(pg, walker, &style_ptr->user_var.refcon);

	style_ptr->used_ctr = pgUnpackNum(walker);

	pgUnpackNumbers(walker, style_ptr->future, PG_FUTURE, long_data);
	
	if (pg->version < KEY_REVISION4)
		num_styles = OLD_MAX_STYLES;
	else
		num_styles = MAX_STYLES;

	pgUnpackNumbers(walker, style_ptr->styles, num_styles, short_data);

	if (pg->version >= KEY_REVISION5)
		style_ptr->small_caps_index = pgUnpackNum(walker);

	if (pg->version >= KEY_REVISION9) {				//ее TRS/OITC

		style_ptr->key_equiv = pgUnpackNum(walker);
		style_ptr->style_num = (short)pgUnpackNum(walker);
		style_ptr->style_basedon = (short)pgUnpackNum(walker);
		style_ptr->rtf_reserved = (short)pgUnpackNum(walker);
	}

	if (pg->version >= KEY_REVISION12)
		style_ptr->named_style_index = pgUnpackNum(walker);

	style_ptr->procs = pg->globals->def_style.procs;
}


/* This unpacks a par_info record */

static void unpack_par_info (paige_rec_ptr pg, pack_walk_ptr walker,
		par_info_ptr info)
{
	register par_info_ptr		par_ptr;
	long						tab_qty;

	par_ptr = info;

	par_ptr->justification = (short)pgUnpackNum(walker);
	par_ptr->direction = (short)pgUnpackNum(walker);
	par_ptr->style_sheet_id = pgUnpackNum(walker);

	unpack_indents(walker, &par_ptr->indents);
	
	par_ptr->spacing = pgUnpackNum(walker);
	par_ptr->leading_extra = pgUnpackNum(walker);
	par_ptr->leading_fixed = pgUnpackNum(walker);
	if (pg->version >= KEY_REVISION6)						//ее TRS/OITC
		par_ptr->leading_variable = pgUnpackNum(walker);	//ее TRS/OITC
	par_ptr->top_extra = pgUnpackNum(walker);
	par_ptr->bot_extra = pgUnpackNum(walker);
	par_ptr->left_extra = pgUnpackNum(walker);
	par_ptr->right_extra = pgUnpackNum(walker);
	par_ptr->user_id = pgUnpackNum(walker);
	par_ptr->user_data = pgUnpackNum(walker);
	par_ptr->user_data2 = pgUnpackNum(walker);
	par_ptr->partial_just = pgUnpackNum(walker);
	if (pg->version >= KEY_REVISION9)						//ее TRS/OITC
		par_ptr->outline_level = (short)pgUnpackNum(walker);//ее TRS/OITC
	
	unpack_refcon(pg, walker, &par_ptr->user_var.refcon);

	par_ptr->used_ctr = pgUnpackNum(walker);

	pgUnpackNumbers(walker, par_ptr->future, PG_FUTURE, long_data);
	
	if (tab_qty = pgUnpackNum(walker)) {
		
		par_ptr->num_tabs = (pg_short_t)tab_qty;
	
		if (par_ptr->num_tabs > TAB_ARRAY_SIZE)
			par_ptr->num_tabs = TAB_ARRAY_SIZE;

		unpack_tabs(pg, walker, par_ptr->tabs, par_ptr->num_tabs);
		
		while (tab_qty > TAB_ARRAY_SIZE) {
			// Read in possible excessive tabs from earlier versions
			pgUnpackNum(walker);
			--tab_qty;
		}
	}

	if (pg->version >= KEY_REVISION1)
		par_ptr->def_tab_space = pgUnpackNum(walker);

	// PAJ added 6/9/94
#ifdef PG_BASEVIEW
	pgUnpackPtrBytes(walker, (pg_bits8_ptr)par_ptr->user_var.styleSheetName);
	par_ptr->user_var.styleSheetResID = pgUnpackNum(walker);
	par_ptr->user_var.unused = pgUnpackNum(walker);
	// PAJ end added
#endif

	if (pg->version >= KEY_REVISION7)
		par_ptr->class_info = (short)pgUnpackNum(walker);

	if (pg->version >= KEY_REVISION9) {				//ее TRS/OITC
		par_ptr->key_equiv = pgUnpackNum(walker);
		par_ptr->style_num = (short)pgUnpackNum(walker);
		par_ptr->style_basedon = (short)pgUnpackNum(walker);
		par_ptr->next_style = (short)pgUnpackNum(walker);
	}

	if (pg->version >= KEY_REVISION12)
		par_ptr->named_style_index = pgUnpackNum(walker);
	
	if (pg->version >= KEY_REVISION18) {
		
		par_ptr->table.table_columns = pgUnpackNum(walker);
		par_ptr->table.table_column_width = pgUnpackNum(walker);
		par_ptr->table.table_cell_height = pgUnpackNum(walker);
		par_ptr->table.border_info = pgUnpackNum(walker);
		par_ptr->table.border_spacing = pgUnpackNum(walker);
		par_ptr->table.border_shading = pgUnpackNum(walker);
		par_ptr->table.cell_borders = pgUnpackNum(walker);
		par_ptr->table.grid_borders = pgUnpackNum(walker);
		par_ptr->table.cell_h_extra = pgUnpackNum(walker);

		par_ptr->table.unique_id = pgUniqueID(pg->myself);
	}

	if (pg->version >= KEY_REVISION22)
		par_ptr->html_style = pgUnpackNum(walker);
	
	if (pg->version >= KEY_REVISION23) {
		
		par_ptr->table.top_border_color = pgUnpackNum(walker);
		par_ptr->table.left_border_color = pgUnpackNum(walker);
		par_ptr->table.bottom_border_color = pgUnpackNum(walker);
		par_ptr->table.right_border_color = pgUnpackNum(walker);
	}

	par_ptr->procs = pg->globals->def_par.procs;
}


/* This unpacks a font_info record */

static void unpack_font_info (paige_rec_ptr pg, pack_walk_ptr walker,
		font_info_ptr font_ptr)
{
	register font_info_ptr		fonts;
	
	fonts = font_ptr;
	pgFillBlock(fonts, sizeof(font_info), 0);

	unpack_fontsize_text(pg, walker, fonts->name);

	if (pg->version >= KEY_REVISION8)
		unpack_fontsize_text(pg, walker, fonts->alternate_name);

	fonts->environs = (short)pgUnpackNum(walker);
	fonts->typeface = (short)pgUnpackNum(walker);
	fonts->family_id = (short)pgUnpackNum(walker);
	fonts->char_type = (short)pgUnpackNum(walker);
	if (pg->version >= KEY_REVISION9)
		fonts->code_page = (short)pgUnpackNum(walker);
	fonts->language = pgUnpackNum(walker);
	unpack_refcon(pg, walker, &fonts->user_var.refcon);

	pgUnpackNumbers(walker, fonts->machine_var, PG_FUTURE, long_data);
	
	if (pg->version >= KEY_REVISION3) {
		
		fonts->platform = pgUnpackNum(walker);
		fonts->alternate_id = (short)pgUnpackNum(walker);
	}
	else {
		
		fonts->platform = PAIGE_PLATFORM;
		fonts->alternate_id = 0;
		fonts->environs &= (~FONT_NOT_AVAIL);
	}

	//Correct language code
	if (pg->version <= KEY_REVISION10)
		if ((fonts->platform & GENERAL_PLATFORM_MASK) == GENERAL_MACINTOSH)
			fonts->language &= 0x000000FF;
}


/* unpack_fontsize_text reads a FONT_SIZE string and makes necessary Unicode conversions. */

static void unpack_fontsize_text (paige_rec_ptr pg, pack_walk_ptr walker, pg_char_ptr name)
{
#ifdef UNICODE
	pgUnpackPtrBytes(walker, (pg_bits8_ptr)name);
	
	if (pg->flags2 & UNICODE_SAVED)
		pgUnicodeToUnicode((pg_short_t PG_FAR *)name, FONT_SIZE + 1, FALSE);
	else
		pgBytesToUnicode((pg_bits8_ptr)name, (pg_short_t PG_FAR *)name, NULL, FONT_SIZE);
#else
	if (pg->flags2 & UNICODE_SAVED) {

		pg_char		temp_name[(FONT_SIZE * 2) + 2];
		
		pgFillBlock(temp_name, (FONT_SIZE * 2) + 2, 0);
		pgUnpackPtrBytes(walker, (pg_bits8_ptr)temp_name);
		
		pgUnicodeToUnicode((pg_short_t PG_FAR *)temp_name, FONT_SIZE + 1, FALSE);
		pgUnicodeToBytes((pg_short_t PG_FAR *)temp_name, temp_name, NULL, FONT_SIZE);
		pgBlockMove(temp_name, name, FONT_SIZE);
	}
	else 
		pgUnpackPtrBytes(walker, (pg_bits8_ptr)name);
#endif
}


/* unpack_refcon handles the pedaml pe-pead, `eaauqe mle ilteppim tepqiml did a  paai `yteq 
dmp thiq `ut ue hate ahaleed it back to a long. */

static void unpack_refcon (paige_rec_ptr pg, pack_walk_ptr walker, long PG_FAR *refcon)
{
	if (pg->version == KEY_REVISION6)
		pgUnpackPtrBytes(walker, (pg_bits8_ptr)refcon);
	else
		*refcon = pgUnpackNum(walker);
}


/* equal_constants compares two byte arrays */

static pg_boolean equal_constants (pg_bits8_ptr str1, pg_bits8_ptr str2, short size)
{
	register pg_bits8_ptr			compare1, compare2;
	register short					ctr;
	
	for (compare1 = str1, compare2 = str2, ctr = size; ctr; --ctr)
		if (*compare1++ != *compare2++)
			return	FALSE;
	
	return	TRUE;
}


/* Input optional pg_bits8 memory reference */

static void input_opt_character_ref(pgm_globals_ptr mem_globals, pack_walk_ptr walker, memory_ref PG_FAR *ref)
{
	long	input_byte_size = pgGetUnpackedSize(walker);
	
	if (input_byte_size) {
		*ref = MemoryAlloc(mem_globals, 1, input_byte_size, 0);
		pgUnpackBytes(walker, *ref);
	}
	else *ref = MEM_NULL;
}

/* convert_for_unicode makes the necessary conversions to and from Unicode systems. */

	/* Version 2.0 Unicode notes. The following scenarios are possible:
		(a) The file is not unicode yet this library is. Result: The incoming text is
			8-bit ASCII and the begin-end members of text_block are correct character counts.
			The text memory_ref is the correct size for 8-bits but needs to be converted
			to 16 bit chars.
		(b) The file is unicode yet this library is not. Result: The incoming text is
			16-bit ASCII and the begin-end members of text_block are correct character counts.
			The text memory_ref will be twice as large as we need.
		(c) The file is Unicode and so is this lib. Result: Nothing needs to change.
		(d) The file is not Unicode and neither is this lib. Nothing needs to change.
	*/

static void convert_for_unicode (paige_rec_ptr pg)
{
	text_block_ptr			block;
	long					num_blocks;
	
	num_blocks = GetMemorySize(pg->t_blocks);
	
	for (block = (text_block_ptr) UseMemory(pg->t_blocks); num_blocks; ++block, --num_blocks) {

	#ifdef UNICODE
		pgBlockToUnicode(pg, block);
	#else
		pgUnicodeToBlock(pg, block);
	#endif
	}

	UnuseMemory(pg->t_blocks);
}
		