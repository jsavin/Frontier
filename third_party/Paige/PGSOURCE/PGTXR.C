
/* Import/Export functions for PAIGE library.  This file contains miscellaneious utility functions
that can be called from both C and C++. */

#include "Paige.h"
#include "defprocs.h"
#include "pgText.h"
#include "pgDefStl.h"
#include "pgOSUtl.h"
#include "pgSelect.h"
#include "pgTxr.h"
#include "pgEdit.h"
#include "pgEmbed.h"
#include "pgTxtWid.h"
#include "machine.h"
#include "pgHText.h"
#include "pgTables.h"
#include "pgFrame.h"

static long make_style (pg_ref import_pg, pg_char_ptr name, long stylebits, short pointsize,
				pg_char_ptr fontname, par_info_ptr par);
static void copy_style_run (paige_rec_ptr pg, long position, memory_ref source_ref,
			memory_ref target_ref, long imported_length);
static void copy_hyperlink_run (paige_rec_ptr pg, long position, memory_ref source_ref,
			memory_ref target_ref, long imported_length);
static void merge_text_blocks (paige_rec_ptr pg, text_block_ptr import_blocks,
		long target_pos, long num_import_blocks);
static long find_breaking_char (pg_char_ptr text, long text_size, pg_char cr_char);
static long insert_default_item (memory_ref runref, pg_short_t style_item);
static long find_par_position (pg_import_ptr import_ptr, pg_char cr_char);
static void set_embed_info (paige_rec_ptr pg, pg_import_ptr import_ptr, style_info_ptr embed_style, pg_short_t style_item);
static pg_boolean equal_extensions (pg_bits8_ptr ext1, pg_bits8_ptr ext2, short size);
static pg_boolean compare_messages (pg_char_ptr msg1, pg_char_ptr msg2);


/* pgBeginImport sets up a pg_ref to receive very fast importing. Using this method, PAIGE
does not need to go through all the gyrations with style, paragraph and miscellaneious formatting. */

PG_PASCAL (void) pgBeginImport (pg_ref pg, long import_position)
{
	paige_rec_ptr		pg_rec;
	pgm_globals_ptr		mem_globals;
	pg_import_ptr		import_ptr;
	style_run_ptr		run;
	style_info_ptr		style;
	memory_ref			import_ref;
	text_block_ptr		block;
	long				memory_id;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	mem_globals = pg_rec->globals->mem_globals;
	memory_id = pg_rec->mem_id;
	
	import_ref = MemoryAllocClearID(mem_globals, sizeof(pg_import_rec), 1, 0, memory_id);
	import_ptr = (pg_import_ptr) UseMemory(import_ref);
	import_ptr->previous_import = pg_rec->import_control;
	pg_rec->import_control = import_ref;
	
	import_ptr->target_pos = import_position;
	import_ptr->t_blocks = MemoryAllocClearID(mem_globals, sizeof(text_block), 1, 4, memory_id);
	block = (text_block_ptr) UseMemory(import_ptr->t_blocks);
	pgInitTextblock(pg_rec, 0, MEM_NULL, block, FALSE);
	UnuseMemory(import_ptr->t_blocks);

	import_ptr->t_style_run = MemoryAllocID(mem_globals, sizeof(style_run), 0, 8, memory_id);
	import_ptr->par_style_run = MemoryAllocID(mem_globals, sizeof(style_run), 0, 8, memory_id);
	import_ptr->hyperlinks = MemoryAllocID(mem_globals, sizeof(pg_hyperlink), 0, 2, memory_id);
	import_ptr->hyperlinks_target = MemoryAllocID(mem_globals, sizeof(pg_hyperlink), 0, 8, memory_id);
	import_ptr->frames = MemoryAllocID(mem_globals, sizeof(pg_frame), 0, 4, memory_id);

	run = pgFindRunFromRef(pg_rec->t_style_run, import_position, NULL);
	import_ptr->last_style_item = run->style_item;
	UnuseMemory(pg_rec->t_style_run);

	run = pgFindRunFromRef(pg_rec->par_style_run, import_position, NULL);
	import_ptr->last_par_item = run->style_item;
	UnuseMemory(pg_rec->par_style_run);

	style = (style_info_ptr) UseMemoryRecord(pg_rec->t_formats, (long)import_ptr->last_style_item, 0, TRUE);
	import_ptr->last_font_index = style->font_index;
	UnuseMemory(pg_rec->t_formats);

	UnuseMemory(import_ref);
	UnuseMemory(pg);
}


/* pgEndImport terminates the "import mode" set up by calling pgBeginImport() above. */

PG_PASCAL (void) pgEndImport (pg_ref pg, pg_boolean keep_selection, short draw_mode)
{
	paige_rec_ptr		pg_rec;
	pg_import_ptr		import_ptr;
	memory_ref			import_ref;
	text_block_ptr		block, import_block, last_import_block;
	pg_frame_ptr		frame;
	par_info_ptr		end_par;
	pg_table			end_table;
	t_select_ptr		selections;
	pg_boolean			result = FALSE;
	long				num_blocks, num_links, target_pos, num_frames;

	pg_rec = (paige_rec_ptr) UseMemory(pg);

	if ((import_ref = pg_rec->import_control) != MEM_NULL) {
		
		import_ptr = (pg_import_ptr) UseMemory(import_ref);
		pg_rec->import_control = import_ptr->previous_import;
		
		num_blocks = GetMemorySize(import_ptr->t_blocks);
		target_pos = import_ptr->target_pos;

		if (result = (pg_boolean)(import_ptr->t_length) != 0) {
			
			import_block = (text_block_ptr) UseMemory(import_ptr->t_blocks);
			
			if (num_blocks > 1) {
				
				last_import_block = &import_block[num_blocks - 1];
				
				if (last_import_block->begin == last_import_block->end) {
					
					DisposeNonNilMemory(last_import_block->text);
					DisposeMemory(last_import_block->lines);
					UnuseMemory(import_ptr->t_blocks);
					
					--num_blocks;
					SetMemorySize(import_ptr->t_blocks, num_blocks);
					import_block = (text_block_ptr) UseMemory(import_ptr->t_blocks);
				}
			}

			if (pg_rec->t_length == 0) {
				// We can just insert everything, empty doc.
				
				block = (text_block_ptr) UseMemory(pg_rec->t_blocks);
				
				if (import_block->text && block->text)
					MemoryCopy(import_block->text, block->text);
				
				block->end = import_block->end - import_block->begin;
				block->flags = NEEDS_CALC;
				block->file_os = import_block->file_os;

				UnuseMemory(pg_rec->t_blocks);
				
				if (num_blocks > 1) {
					long			append_qty;
					
					append_qty = num_blocks - 1;

					block = (text_block_ptr) AppendMemory(pg_rec->t_blocks, append_qty, FALSE);
					pgBlockMove(&import_block[1], block, sizeof(text_block) * append_qty);
					UnuseMemory(pg_rec->t_blocks);
				}
			}
			else
				merge_text_blocks(pg_rec, import_block, target_pos, num_blocks);

			UnuseMemory(import_ptr->t_blocks);
			
			// Adjust all the formats and the total text length:
			
			pg_rec->t_length += import_ptr->t_length;
			copy_style_run(pg_rec, target_pos, import_ptr->t_style_run, pg_rec->t_style_run, import_ptr->t_length);
			copy_style_run(pg_rec, target_pos, import_ptr->par_style_run, pg_rec->par_style_run, 0);
			copy_hyperlink_run(pg_rec, target_pos, import_ptr->hyperlinks, pg_rec->hyperlinks, import_ptr->t_length);
			copy_hyperlink_run(pg_rec, target_pos, import_ptr->hyperlinks_target, pg_rec->target_hyperlinks, import_ptr->t_length);

			SetMemorySize(pg_rec->select, 2);
			selections = (t_select_ptr) UseMemory(pg_rec->select);
			
			if (keep_selection)
				selections->offset = import_ptr->target_pos;
			else
				selections->offset = target_pos + import_ptr->t_length;

			selections[1].offset = selections->offset;

			selections->flags = selections[1].flags = SELECTION_DIRTY;
			UnuseMemory(pg_rec->select);
			
			pg_rec->change_ctr += 1;
		}

		// Insert possible frames:
		
		frame = (pg_frame_ptr) UseMemory(import_ptr->frames);
		num_frames = GetMemorySize(import_ptr->frames);
		
		while (num_frames) {
			
			pgInsertFrame(pg, frame, (embed_callback)pg_rec->globals->embed_callback_proc, draw_none);
			
			++frame;
			--num_frames;
		}

		UnuseMemory(import_ptr->frames);

	// First text block is never inserted directly (only the other blocks get inserted).
	
		import_block = (text_block_ptr) UseMemory(import_ptr->t_blocks);

		DisposeMemory(import_block->lines);
		DisposeNonNilMemory(import_block->text);
		UnuseAndDispose(import_ptr->t_blocks);

		DisposeMemory(import_ptr->t_style_run);
		DisposeMemory(import_ptr->par_style_run);
		DisposeMemory(import_ptr->hyperlinks);
		DisposeMemory(import_ptr->hyperlinks_target);
		DisposeMemory(import_ptr->frames);

		UnuseAndDispose(import_ref);
		
		pgSetTerminatorFlag(pg_rec);

	// Handle situation in which border(s) or table(s) are at very end of doc:
	
		end_par = pgFindParStyle(pg_rec, pg_rec->t_length);
		end_table = end_par->table;
		UnuseMemory(pg_rec->par_formats);
		
		if (end_table.border_info || end_table.table_columns) {
			par_info			par, mask;
			select_pair			end_point;

			pgFillBlock(&mask, sizeof(par_info), -1);
			par = pg_rec->globals->def_par;
			end_point.end = end_point.begin = pg_rec->t_length;
			pgSetParInfo(pg_rec->myself, &end_point, &par, &mask, draw_none);
		}
		
	// Handle situation in which target hyperlinks have no text range:
	
		if ((num_links = GetMemorySize(pg_rec->target_hyperlinks) - 1) > 0) {
			pg_hyperlink_ptr		links;
			long					link_index;
			
			links = (pg_hyperlink_ptr) UseMemory(pg_rec->target_hyperlinks);
			
			for (link_index = 0; link_index < num_links; ++link_index, ++links) {
				
				if (links->applied_range.end <= links->applied_range.begin)
					pg_rec->procs.boundary_proc(pg_rec, &links->applied_range);
			}
			
			UnuseMemory(pg_rec->target_hyperlinks);
		}
		
		pgInvalCharLocs(pg_rec);
		
		if (draw_mode)
			pgUpdateText(pg_rec, NULL, 0, pg_rec->t_length, MEM_NULL, NULL, draw_mode, TRUE);
	}

	UnuseMemory(pg);
}


/* pgInsertText is a convenience utility used for importing. It allows a block of text
to be inserted with a specified format and paragraph format applied. */

PG_PASCAL (pg_boolean) pgInsertText (pg_ref pg, pg_char_ptr data, long length,
		font_info_ptr font, style_info_ptr style, par_info_ptr paragraph, short draw_mode)
{
	paige_rec_ptr			pg_rec;
	pg_short_t				style_index;
	pg_boolean				inserted_something = FALSE;
	
	if (!length)
		return	FALSE;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	
	if (pg_rec->import_control) {
		pg_import_ptr		import_ptr;
		pg_char_ptr			text, source_text;
		style_info_ptr		style_access;
		par_info_ptr		par_access;
		text_block_ptr		block;
		style_info			added_style;
		style_run_ptr		run;
		style_run			last_run;
		pg_boolean			cached_file;
		pg_short_t			last_run_index;
		long				num_blocks, cr_break, threefourths_of_max, par_position;
		long				initial_block_size, source_size, style_position;
		long				inserted_default, previous_cache_pos;
		
		import_ptr = (pg_import_ptr) UseMemory(pg_rec->import_control);
		num_blocks = GetMemorySize(import_ptr->t_blocks);
		block = (text_block_ptr) UseMemoryRecord(import_ptr->t_blocks, num_blocks - 1, USE_ALL_RECS, TRUE);
		block->file_os = import_ptr->file_os;

		threefourths_of_max = pg_rec->globals->max_block_size - (pg_rec->globals->max_block_size / 4);
		initial_block_size = block->end - block->begin;
		source_text = data;
		source_size = length;
		cached_file = (pg_boolean)(pg_rec->cache_file != MEM_NULL);
		
		if (cached_file && initial_block_size == 0)
			block->cache_begin = import_ptr->cache_pos;

		if (initial_block_size >= threefourths_of_max) {
		
			if (cr_break = find_breaking_char(source_text, source_size, (pg_char)pg_rec->globals->line_wrap_char)) {
				long				end_pos;
				
				if (!cached_file) {
				
					text = (pg_char_ptr) AppendMemory(block->text, cr_break, FALSE);
					pgBlockMove(source_text, text, cr_break * sizeof(pg_char));
					UnuseMemory(block->text);
				}

				block->end += cr_break;
				end_pos = block->end;
				previous_cache_pos = block->cache_begin;
				block = (text_block_ptr) AppendMemory(import_ptr->t_blocks, 1, TRUE);
				pgInitTextblock(pg_rec, end_pos, MEM_NULL, block, cached_file);	
				block->file_os = import_ptr->file_os;			
				source_size -= cr_break;
				source_text += cr_break;
				
				if (cached_file)
					block->cache_begin = previous_cache_pos + initial_block_size + (cr_break * sizeof(pg_char));
			}
		}

		if (source_size) {
			
			if (!cached_file) {
			
				text = (pg_char_ptr) AppendMemory(block->text, source_size, FALSE);
				pgBlockMove(source_text, text, source_size * sizeof(pg_char));
				UnuseMemory(block->text);
			}

			block->end += source_size;
		}

		import_ptr->t_length += length;
		
		UnuseMemory(import_ptr->t_blocks);
	
	// Insert the new formats:
		
		style_index = import_ptr->last_style_item;
		style_position = import_ptr->t_length - length;
		
		if (import_ptr->last_was_embed)
			if ((style_position - import_ptr->last_style_position) > 2)
				style_position = import_ptr->last_style_position + 2;
		
		import_ptr->last_style_position = style_position;

		if (font) {
			
			if (style)
				added_style = *style;
			else
				GetMemoryRecord(pg_rec->t_formats, (long)import_ptr->last_style_item, &added_style);
			
			added_style.font_index = pgAddNewFont(pg_rec, font);
			import_ptr->last_font_index = added_style.font_index;
			style_index = pgAddStyleInfo(pg_rec, NULL, for_next_insert_reason, &added_style);
			set_embed_info(pg_rec, import_ptr, &added_style, style_index);
		}
		else
		if (style) {
			
			added_style = *style;
			added_style.font_index = import_ptr->last_font_index;
			style_index = pgAddStyleInfo(pg_rec, NULL, for_next_insert_reason, &added_style);
			set_embed_info(pg_rec, import_ptr, &added_style, style_index);
		}
		
		if (style_index != import_ptr->last_style_item) {
			
			if (style_position > 0)
				inserted_default = insert_default_item(import_ptr->t_style_run, import_ptr->last_style_item);
			else
				inserted_default = 0;

			if ((last_run_index = (unsigned short)GetMemorySize(import_ptr->t_style_run)) > 0)
				GetMemoryRecord(import_ptr->t_style_run, last_run_index - 1, &last_run);
			else
				last_run.offset = -1;

			style_access = (style_info_ptr) UseMemory(pg_rec->t_formats);

			if (last_run.offset == style_position) {
				pg_short_t		previous_index;
				
				previous_index = last_run.style_item;
				run = (style_run_ptr) UseMemoryRecord(import_ptr->t_style_run, last_run_index - 1, 0, TRUE);
				style_access[previous_index].used_ctr -= 1;
			}
			else
				run = (style_run_ptr) AppendMemory(import_ptr->t_style_run, 1, FALSE);

			run->offset = style_position;
			run->style_item = style_index;
			style_access[style_index].used_ctr += 1;
			UnuseMemory(import_ptr->t_style_run);
			
			if (inserted_default)
				style_access[inserted_default - 1].used_ctr += 1;

			UnuseMemory(pg_rec->t_formats);
		}

		import_ptr->last_style_item = style_index;

		if (paragraph) {
				
			style_index = pgAddParInfo(pg_rec, NULL, for_next_insert_reason, paragraph);
			
			if (style_index != import_ptr->last_par_item) {
				
				if ((par_position = find_par_position(import_ptr, (pg_char)pg_rec->globals->line_wrap_char)) > 0)
					inserted_default = insert_default_item(import_ptr->par_style_run, import_ptr->last_par_item);
				else
					inserted_default = 0;
				
				if ((last_run_index = (unsigned short)GetMemorySize(import_ptr->par_style_run)) > 0)
					GetMemoryRecord(import_ptr->par_style_run, last_run_index - 1, &last_run);
				else
					last_run.offset = -1;

				par_access = (par_info_ptr) UseMemory(pg_rec->par_formats);

				if (last_run.offset == par_position) {
					pg_short_t		previous_index;
					
					previous_index = last_run.style_item;
					run = (style_run_ptr) UseMemoryRecord(import_ptr->par_style_run, last_run_index - 1, 0, TRUE);
					par_access[previous_index].used_ctr -= 1;
				}
				else
					run = (style_run_ptr) AppendMemory(import_ptr->par_style_run, 1, FALSE);

				run->offset = par_position;
				run->style_item = style_index;
				par_access[style_index].used_ctr += 1;

				UnuseMemory(import_ptr->par_style_run);

				if (inserted_default)
					par_access[inserted_default - 1].used_ctr += 1;

				UnuseMemory(pg_rec->par_formats);
			}

			import_ptr->last_par_item = style_index;
		}

		UnuseMemory(pg_rec->import_control);
	}
	else {
		style_info			style_mask;
		par_info			par_mask;
		font_info			font_mask;
		select_pair			par_range;
		short				use_draw_mode;

		if (pg_rec->num_selects)
			pgDelete(pg, NULL, draw_none);
		
		pgFillBlock(&style_mask, sizeof(style_info), -1);
		pgFillBlock(&par_mask, sizeof(par_info), -1);
		pgFillBlock(&font_mask, sizeof(font_info), -1);

		if (font) {
			
			if (style)
				pgSetStyleAndFont(pg, NULL, style, &style_mask, font, &font_mask, draw_none);
			else
				pgSetFontInfo(pg, NULL, font, &font_mask, draw_none);
		}
		else
		if (style)
			pgSetStyleInfo(pg, NULL, style, &style_mask, draw_none);
		
		if (paragraph)
			use_draw_mode = draw_none;
		else
			use_draw_mode = draw_mode;
		
		par_range.begin = par_range.end = pgCurrentInsertion(pg_rec);
		
		inserted_something = pgInsert(pg, data, length, CURRENT_POSITION, data_insert_mode, 0, draw_mode);
		
		if (paragraph) {
			
			par_range.end += length;
			pgSetParInfo(pg, &par_range, paragraph, &par_mask, draw_none);
			
			if (draw_mode)
				pgUpdateText(pg_rec, NULL, 0, pg_rec->t_length, MEM_NULL, NULL, draw_mode, TRUE);
		}
	}

	UnuseMemory(pg);
	
	return		inserted_something;
}

/* pgRemoveLastInsert removes the last character inserted. If match_char is zero then the
last char is removed no matter what,  else it is removed only if it matches.  THIS ONLY WORKS
CORRECTLY FOR SERIAL IMPORTS. */

PG_PASCAL (void) pgRemoveLastInsert (paige_rec_ptr pg_rec, pg_char match_char)
{
	if (pg_rec->import_control) {
		pg_import_ptr		import_ptr;
		pg_char_ptr			text;
		text_block_ptr		block;
		long				num_blocks, text_pos;

		import_ptr = (pg_import_ptr) UseMemory(pg_rec->import_control);
		
		if (import_ptr->t_length) {
		
			num_blocks = GetMemorySize(import_ptr->t_blocks);
			block = (text_block_ptr) UseMemoryRecord(import_ptr->t_blocks, num_blocks - 1, USE_ALL_RECS, TRUE);
			
			if ((text_pos = block->end - block->begin) > 0) {
				pg_char			end_char;

				text = (pg_char_ptr) UseMemory(block->text);
				end_char = text[text_pos - 1];
				UnuseMemory(block->text);
				
				if (match_char == 0 || match_char == end_char) {
					
					SetMemorySize(block->text, text_pos - 1);
					block->end -= 1;
					import_ptr->t_length -= 1;
				}
				
				UnuseMemory(import_ptr->t_blocks);
			}
		}
		
		UnuseMemory(pg_rec->import_control);
	}
}


// pgInsertEmbed is here for historical reasons but our import-export does not use it.

PG_PASCAL (pg_boolean) pgInsertEmbed (pg_ref pg, embed_ref ref,
		short stylesheet_option, embed_callback callback, long callback_refcon,
		font_info_ptr font, style_info_ptr style, short draw_mode)
{
	paige_rec_ptr			pg_rec;
	style_info_ptr			use_style;
	style_info				stylemask, embed_style;
	font_info				font_mask;
	pg_char					embed_chars[2];
	pg_boolean				result;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	
	if (!pg_rec->import_control) {

		style_info	s_mask;
		font_info	f_mask;
		
		pgInitFontMask (&f_mask, SET_MASK_BITS);
		pgInitStyleMask (&s_mask, SET_MASK_BITS);
			
		pgSetStyleAndFont (pg, NULL, style, &s_mask, font, &f_mask, draw_mode);
	
		result = pgInsertEmbedRef (pg, ref, CURRENT_POSITION, stylesheet_option, callback, callback_refcon, draw_mode);
	}
	else {
	
		embed_chars[0] = DUMMY_LEFT_EMBED;
		embed_chars[1] = DUMMY_RIGHT_EMBED;
	    
	    if (style)
	    	use_style = style;
	    else {
	        
	        embed_style = pg_rec->globals->def_style;
	    	use_style = &embed_style;
	    }
	
		pgInitEmbedStyleInfo(pg_rec, 0, ref, stylesheet_option, callback,
					callback_refcon, use_style, &stylemask, font, &font_mask, FALSE);
		 
		result = pgInsertText(pg, embed_chars, 2, font, use_style, NULL, draw_none);
	}

	UnuseMemory(pg);
	
	return	result;
}


/* pgConvertResolution returns the value to use for this machine assuming pointvalue point size. */

PG_PASCAL (short) pgConvertResolution (paige_rec_ptr pg, short pointvalue)
{
	pg_fixed		point_size;
	
	point_size = pointvalue;
	point_size <<= 16;
	point_size = pgPointsizeToScreen(pg->myself, point_size);
	point_size >>= 16;
	
	return	(short)point_size;
}

/* pgResolutionConvert does the reverse of pgConvertResolution. */

PG_PASCAL (short) pgResolutionConvert (paige_rec_ptr pg, short value)
{
	pg_fixed		point_size;
	
	point_size = value;
	point_size <<= 16;
	point_size = pgScreenToPointsize(pg->myself, point_size);
	point_size >>= 16;
	
	return	(short)point_size;
}


/* pgInitTranslatorRec initializes a PAIGE translator record to its defaults. Note, the
translator record is assumed to be initialized to all zeros upon entry. */

PG_PASCAL (void) pgInitTranslatorRec (pg_globals_ptr globals, pg_translator_ptr translator_ptr)
{
	translator_ptr->format = globals->def_style;
	translator_ptr->par_format = globals->def_par;
	translator_ptr->font = globals->def_font;
	translator_ptr->format.point = 0x000C0000;

	if (!translator_ptr->data)
		translator_ptr->data = MemoryAllocClear(globals->mem_globals, sizeof(pg_char), TRANSLATOR_BUFFER_SIZE, 0);
	
	if (!translator_ptr->stylesheet_table)
		translator_ptr->stylesheet_table = MemoryAllocClear(globals->mem_globals, sizeof(style_table), 0, 1);
	else
		SetMemorySize(translator_ptr->stylesheet_table, 0);
}


/* pgDisposeTranslatorRec disposes any structures it made when creating the translator */

PG_PASCAL (void) pgDisposeTranslatorRec (pg_translator_ptr translator_ptr)
{
	if (translator_ptr->data)
		DisposeFailedMemory(translator_ptr->data);
	if (translator_ptr->stylesheet_table)
		DisposeFailedMemory(translator_ptr->stylesheet_table);
}


/* pgDetermineFileType returns the file type based on the file's contents.  This function
only determines the predefined, known types.  Upon entry, starting_position is the first
byte in the file. */

PG_PASCAL (pg_filetype) pgDetermineFileType (pg_file_unit fileref, file_io_proc io_proc,
		long starting_position)
{
	file_io_proc			use_io_proc;
	pg_filetype				result = pg_unknown_type;
	
	if ((use_io_proc = io_proc) == NULL)
		use_io_proc = pgOSReadProc;
	
	if (pgVerifyFile((file_ref)fileref, use_io_proc, starting_position) == NO_ERROR)
		result = pg_paige_type;
	else
	if (pgVerifyRTF((pg_file_unit)fileref, use_io_proc, starting_position) == NO_ERROR)
		result = pg_rtf_type;
	else
	if (pgVerifyHTML((pg_file_unit)fileref, use_io_proc, starting_position) == NO_ERROR)
		result = pg_html_type;

	return		result;
}


/* pgMapCharacters maps the characters from one OS to another based on the character_tables. */

PG_PASCAL (void) pgMapCharacters (paige_rec_ptr pg, text_block_ptr block)
{
	style_walk					walker;
	pg_globals_ptr				globals;
	register pg_char_ptr		table_ptr;
	register long				byte_qty;
	long						num_chars;
	pg_char_ptr					cross_table;

	if (!pg->cross_table[0] && !pg->cross_table[1])
		return;
	if (!block->text || block->file_os == CURRENT_OS)
		return;
	
	globals = pg->globals;
	pgPrepareStyleWalk(pg, block->begin, &walker, FALSE);
	
	table_ptr = (pg_char_ptr) UseMemory(block->text);
	num_chars = GetMemorySize(block->text);

	for (byte_qty = 0; byte_qty < num_chars; ++byte_qty, ++table_ptr) {
	
	#ifdef UNICODE
		
		if (*table_ptr < 0x100)
	#endif
		if (*table_ptr >= HIGH_CHARS_BASE) {
			
			if (walker.cur_font->machine_var[PG_CHARSET] == 2)
				cross_table = pg->cross_table[1];
			else
				cross_table = pg->cross_table[0];
			
			if (cross_table)
				if ((*table_ptr = cross_table[(*table_ptr - HIGH_CHARS_BASE)]) == N_A)
					*table_ptr = globals->unknown_char[0];
		}
		
		pgWalkStyle(&walker, 1);
	}
	
	pgPrepareStyleWalk(pg, block->begin, NULL, FALSE);
	UnuseMemory(block->text);
	
	block->file_os = CURRENT_OS;
}


/* pgGetCacheFileRef returns the cached file reference, if any. */

PG_PASCAL (file_ref) pgGetCacheFileRef (pg_ref pg)
{
	paige_rec_ptr		pg_rec;
	file_ref			result = MEM_NULL;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	result = pg_rec->cache_file;
	UnuseMemory(pg);

	return		result;
}


/* pgCompareFontTable compares the two font names. The table_font contains a font name equiv. following
the "[" char. The initial table name can have a "*" wild card at the end. If a match is made,
the byte count to the first char after "[" is returned. */

PG_PASCAL (short) pgCompareFontTable (pg_char_ptr source_font, pg_char_ptr table_font)
{
	register pg_char_ptr		source_ptr, table_ptr;
	register short				byte_ctr;

	source_ptr = source_font;
	table_ptr = table_font;
	byte_ctr = 0;
	
	for (;;) {
		
		if (*source_ptr++ != *table_ptr++)
			return	0;
		
		++byte_ctr;
		
		if (*table_ptr == '*' || (*table_ptr == '[' && *source_ptr == 0))
			break;
	}
	
	for (;;) {
		
		++byte_ctr;
		
		if (*table_ptr++ == '[')
			break;
	}
	
	return		byte_ctr;
}

/* pgMatchParStyles walks through all the styles and, if the paragraphs corresponding to character
styles do not contain the same NAMED style index values then they are fixed to match. */
 
PG_PASCAL (void) pgMatchParStyles (paige_rec_ptr pg, long wait_progress, long wait_max)
{
	style_walk			walker;
	memory_ref			changes_ref;
	named_stylesheet	the_style;
	text_block_ptr		block;
	pg_char_ptr			text;
	long				global_offset, source_id;
	long				num_changes;
	long				PG_FAR *append;
	short				def_par_id = 0;

	changes_ref = MemoryAlloc(pg->globals->mem_globals, sizeof(long) * 3, 0, 16);

	pgPrepareStyleWalk(pg, 0, &walker, TRUE);
	global_offset = 0;
	block = (text_block_ptr) UseMemory(pg->t_blocks);
	text = (pg_char_ptr) UseMemory(block->text);

	while (global_offset < pg->t_length) {
		
		pgSetWalkStyle(&walker, global_offset);
		source_id = walker.cur_style->named_style_index;
		
		pg->procs.wait_proc(pg, open_wait, wait_progress, wait_max);
		
		if (source_id) {
			
			if (pgGetNamedStyle(pg->myself, source_id, &the_style)) {
				long			par_id;
				
				if ((par_id = walker.cur_par_style->style_sheet_id) < 0)
					par_id = -par_id;
				
				if (walker.cur_par_style->named_style_index != source_id
					|| par_id != the_style.par_stylesheet_id) {
					
					if (!the_style.par_stylesheet_id) {
						named_stylesheet_ptr	target_style;
						
						if (!def_par_id)
							def_par_id = pgNewParStyle(pg->myself, &pg->globals->def_par);

						the_style.par_stylesheet_id = def_par_id;
						target_style = (named_stylesheet_ptr) UseMemoryRecord(pg->named_styles, source_id - 1, 0, TRUE);
						target_style->par_stylesheet_id = def_par_id;
						UnuseMemory(pg->named_styles);
					}
					
					append = (long *) AppendMemory(changes_ref, 1, FALSE);
					append[0] = global_offset;
					append[1] = source_id;
					append[2] = the_style.par_stylesheet_id;
					
					UnuseMemory(changes_ref);
				}
			}
		}
		
		for (;;) {
			
			if (global_offset >= block->end) {
				
				if (global_offset >= pg->t_length)
					break;

				UnuseMemory(block->text);
				++block;
				text = (pg_char_ptr) UseMemory(block->text);
			}
			
			++global_offset;
			
			if (*text++ == 0x0D)
				break;
		}
	}
	
	UnuseMemory(block->text);
	UnuseMemory(pg->t_blocks);
	pgPrepareStyleWalk(pg, 0, NULL, TRUE);
	
	if ((num_changes = GetMemorySize(changes_ref)) > 0) {
		select_pair		range;
		par_info		par, mask;

		pgFillBlock(&mask, sizeof(par_info), 0);
		mask.style_sheet_id = -1;
		mask.named_style_index = -1;

		append = (long *) UseMemory(changes_ref);
		
		while (num_changes) {
			
			range.begin = range.end = append[0];
			par.named_style_index = append[1];
			par.style_sheet_id = -append[2];
			pgSetParInfoEx(pg->myself, &range, &par, &mask, FALSE, draw_none);

			append += 3;
			--num_changes;
		}
		
		UnuseMemory(changes_ref);
	}
	
	DisposeMemory(changes_ref);
}


/* pgVerifyRTF walks through the given file and verifies if it is an RTF file. */

PG_PASCAL (pg_boolean) pgVerifyRTF (pg_file_unit fileref, file_io_proc io_proc,
		long starting_position)
{
	file_io_proc			use_io_proc;
	size_t					position, datasize;
	pg_bits8				verify_buffer[5];
	pg_boolean				result = BAD_TYPE_ERR;

	datasize = 6;
	pgFillBlock(verify_buffer, datasize, 0);
	
	if ((use_io_proc = io_proc) == NULL)
		use_io_proc = pgOSReadProc;
	
	position = starting_position;
	
	if ((use_io_proc((void PG_FAR *)verify_buffer, io_data_direct, &position, &datasize, (file_ref)fileref)) == NO_ERROR) {
		
		if (verify_buffer[0] == RTF_GROUPBEGIN_CHAR)
			if (verify_buffer[1] == RTF_COMMAND_CHAR)
				if (verify_buffer[2] == 'r' && verify_buffer[3] == 't' && verify_buffer[4] == 'f')
					result = NO_ERROR;
	}

	position = starting_position;
	use_io_proc((void PG_FAR *)NULL, io_set_fpos, &position, NULL, (file_ref)fileref);
	
	return		result;
}


/* pgVerifyHTML walks through the given file and verifies if it is an HMTL file. */

PG_PASCAL (pg_boolean) pgVerifyHTML (pg_file_unit fileref, file_io_proc io_proc,
		long starting_position)
{
	file_io_proc			use_io_proc;
	size_t					position, datasize, index, file_eof;
	pg_bits8				verify_buffer[256];
	pg_boolean				result = BAD_TYPE_ERR;

	datasize = 256;
	pgFillBlock(verify_buffer, datasize, 0);
	
	if ((use_io_proc = io_proc) == NULL)
		use_io_proc = pgOSReadProc;
	
	position = starting_position;

	if ((use_io_proc((void PG_FAR *)&file_eof, io_get_eof, &position, &position, (file_ref)fileref)) == 0)
		if (file_eof < 256)
			datasize = file_eof;

	if ((use_io_proc((void PG_FAR *)verify_buffer, io_data_direct, &position, &datasize, (file_ref)fileref)) == NO_ERROR) {
		
		index = 0;
		
		while (index < datasize) {

			if (verify_buffer[index] > ' ')
				break;

			++index;
		}

		if (verify_buffer[index] == '<') {
			long		element;

		// Check on first tag being a "comment" and if so assume HTML. */

			if (index < (datasize - 3))
				if (verify_buffer[index + 1] == '!' && verify_buffer[index + 2] == '-' && verify_buffer[index + 3] == '-')
					result = NO_ERROR;
			
			if (result != NO_ERROR) {
			
				for (element = index; element < datasize; ++element)
					if (verify_buffer[element] == '>')
						break;
				
				if (element > 1 && element < datasize)
					result = NO_ERROR;
			}
		}
	}

	position = starting_position;
	use_io_proc((void PG_FAR *)NULL, io_set_fpos, &position, NULL, (file_ref)fileref);
	
	return		result;
}


/* pgTruncateFileName truncates the give string to max_size considering the file_sep char.
The function result is the new length. */

PG_PASCAL (pg_short_t) pgTruncateFileName (pg_char_ptr string, pg_short_t string_size,
			pg_short_t max_size, pg_char file_sep)
{
	pg_short_t					index, file_size, diff_size;
	pg_short_t					max_plus_terminator;

	max_plus_terminator = max_size + 1;

	if (string_size < max_plus_terminator)
		return	string_size;

	index = string_size;
	
	while (index) {
		
		--index;
		
		if (string[index] == file_sep) {
			
			++index;
			break;
		}
	}
	
	file_size = string_size - index;
	
	if (file_size < max_plus_terminator)
		return	string_size;
	
	diff_size = file_size - max_size;
	index = string_size - diff_size;
	
	while (index < string_size) {
		
		string[index] = 0;
		++index;
	}
	
	return	string_size - diff_size;
}


/* pgInsertErrorLog inserts a new error to the log. */

PG_PASCAL (void) pgInsertErrorLog (pg_ref pg, memory_ref error_log, short error_code, long begin,
			long end, long refcon, pg_char_ptr msg, pg_boolean no_dups)
{
	pg_errlog_ptr		error_ptr;
	long				length, qty;

	length = 0;

	if (msg) {
		
		length = pgCStrLength(msg);

		if (length > 63)
			length = 63;
	}
	
	if (no_dups) {
		
		qty = GetMemorySize(error_log);
		error_ptr = (pg_errlog_ptr)UseMemory(error_log);
		
		while (qty) {
			
			if (error_ptr->code == error_code)
				if (compare_messages(error_ptr->message, msg))
					break;
			
			++error_ptr;
			--qty;
		}
		
		UnuseMemory(error_log);
		
		if (qty != 0)
			return;
	}

	error_ptr = (pg_errlog_ptr)AppendMemory(error_log, 1, TRUE);
	error_ptr->code = error_code;
	error_ptr->offsets.begin = begin;
	error_ptr->offsets.end = end;
	error_ptr->pg = pg;
	error_ptr->refcon = refcon;

	if (msg && length)
		pgBlockMove(msg, error_ptr->message, length);
	
	UnuseMemory(error_log);
}

/* pgSetFileExtension appends the file extension to filename if it is not there already.
The new size of the string is returned. If non_extend_size is not null then it is set to
the size of the name without an extension. */

PG_PASCAL (pg_short_t) pgSetFileExtension (pg_char_ptr filename, pg_char_ptr extension,
				short extension_size, short PG_FAR *non_extend_size)
{
	long		str_size, index;
	
	str_size = pgCStrLength(filename);
	
	if (non_extend_size)
		*non_extend_size = (short)str_size;

	index = str_size;
	
	while (index > 0) {
		
		--index;
		
		if (filename[index] == '.')
			break;
	}

	if (filename[index] == '.') {
		
		if (non_extend_size)
			*non_extend_size = (short)index;

		if (equal_extensions((pg_bits8_ptr)extension, (pg_bits8_ptr) &filename[index], extension_size))
			return	(pg_short_t)str_size;
	}
	
	if (filename[index] == '.')
		str_size = index;

	pgBlockMove(extension, &filename[str_size], extension_size);
	str_size += extension_size;

	filename[str_size] = 0;
	
	return	(pg_short_t)str_size;
}


/* equal_extensions returns true if the two extensions match. */

static pg_boolean equal_extensions (pg_bits8_ptr ext1, pg_bits8_ptr ext2, short size)
{
	short		element;
	pg_bits8	source_char, target_char;

	for (element = 0; element < size; ++element) {
		
		source_char = ext1[element];
		target_char = ext2[element];
		
		if (source_char >= 'A' && source_char <= 'Z')
			source_char += 0x20;
		if (target_char >= 'A' && target_char <= 'Z')
			target_char += 0x20;
		
		if (source_char != target_char)
			return	FALSE;
	}
	
	return	(pg_boolean)(ext2[element] == 0);
}



/* copy_style_run inserts a style_run into source_ref and adjusts all the offsets. */

static void copy_style_run (paige_rec_ptr pg, long position, memory_ref source_ref,
			memory_ref target_ref, long imported_length)
{
	style_run_ptr			run, source_run;
	style_run				first_target_run;
	size_t					rec_num, rec_ctr, num_target_runs, num_source_runs;

	run = pgFindRunFromRef(target_ref, position, &rec_num);
	first_target_run = *run;
	
	if (imported_length) {
	
		num_target_runs = GetMemorySize(target_ref);
		
		for (rec_ctr = rec_num + 1; rec_ctr < num_target_runs; ++rec_ctr) {
			
			run += 1;
			run->offset += imported_length;
		}
	}

	UnuseMemory(target_ref);

	if ((imported_length > 0) && (position < (pg->t_length - imported_length))) {
		
		run = (style_run_ptr) InsertMemory(target_ref, rec_num + 1, 1);
		run->style_item = first_target_run.style_item;
		run->offset = position + imported_length;
		UnuseMemory(target_ref);
	}

	if ((num_source_runs = GetMemorySize(source_ref)) > 0) {
		
		source_run = (style_run_ptr) UseMemory(source_ref);

		if (first_target_run.offset == position)
			DeleteMemory(target_ref, rec_num, 1);
		else 
			rec_num += 1;
		
		run = (style_run_ptr) InsertMemory(target_ref, rec_num, num_source_runs);
		
		while (num_source_runs) {
			
			*run = *source_run;
			run->offset += position;
			
			++run;
			++source_run;
			--num_source_runs;
		}

		UnuseMemory(target_ref);
		UnuseMemory(source_ref);
	}

	run = (style_run_ptr) UseMemoryRecord(target_ref, GetMemorySize(target_ref) - 1, 0, TRUE);
	run->offset = pg->t_length + ZERO_TEXT_PAD;
	UnuseMemory(target_ref);
}

/* copy_hyperlink_run copies any hypertext link data. */

static void copy_hyperlink_run (paige_rec_ptr pg, long position, memory_ref source_ref,
			memory_ref target_ref, long imported_length)
{
	pg_hyperlink_ptr		run, target_run;
	long					index, num_source_items;

	pgAdvanceHyperlinks (pg, target_ref, position, imported_length);
	num_source_items = GetMemorySize(source_ref);

	if (num_source_items > 0) {
	
		target_run = pgFindHypertextRun(target_ref, position, &index);
		target_run = (pg_hyperlink_ptr) InsertMemory(target_ref, index, num_source_items);
		
		run = (pg_hyperlink_ptr) UseMemory(source_ref);
		
		while (num_source_items) {
			
			*target_run = *run;
			target_run->applied_range.begin += position;
			target_run->applied_range.end += position;
			
			++target_run;
			++run;
			--num_source_items;
		}
		
		UnuseMemory(source_ref);
		UnuseMemory(target_ref);
	}

	run = (pg_hyperlink_ptr) UseMemoryRecord(target_ref, GetMemorySize(target_ref) - 1, 0, TRUE);
	run->applied_range.begin = run->applied_range.end = pg->t_length + ZERO_TEXT_PAD;
	UnuseMemory(target_ref);
}

/* merge_text_blocks merges import_blocks into the text blocks of pg. */

static void merge_text_blocks (paige_rec_ptr pg, text_block_ptr import_blocks,
		long target_pos, long num_import_blocks)
{
	text_block_ptr			block, last_block;
	pg_char_ptr				text, source_text;
	pg_short_t				block_num, last_block_num, end_char;
	long					num_blocks, local_position, first_block_size;
	long					end_block_size, end_pos;

	block = pgFindTextBlock(pg, target_pos, &block_num, FALSE, TRUE);
	local_position = target_pos - block->begin;
	first_block_size = import_blocks->end - import_blocks->begin;

	if (num_import_blocks > 1) {
		// Adjust beginning and ending text so we can just append
		
		source_text = (pg_char_ptr) UseMemory(block->text);

		if (local_position > 0) {
			
			text = (pg_char_ptr) InsertMemory(import_blocks->text, 0, local_position);
			pgBlockMove(source_text, text, local_position * sizeof(pg_char));
			UnuseMemory(import_blocks->text);
			
			import_blocks->end = import_blocks->begin + GetMemorySize(import_blocks->text);
		}
		
		if ((end_block_size = block->end - target_pos) > 0) {
			
			last_block = &import_blocks[num_import_blocks - 1];
			text = (pg_char_ptr) AppendMemory(last_block->text, end_block_size, FALSE);
			pgBlockMove(&source_text[local_position], text, end_block_size * sizeof(pg_char));
			UnuseMemory(last_block->text);
			
			last_block->end = last_block->begin + GetMemorySize(last_block->text);
		}

		UnuseMemory(block->text);
		
		first_block_size = import_blocks->end - import_blocks->begin;
		import_blocks->begin = block->begin;
		import_blocks->end = block->begin + first_block_size;

		DisposeNonNilMemory(block->text);
		DisposeMemory(block->lines);
		UnuseMemory(pg->t_blocks);
		DeleteMemory(pg->t_blocks, 0, 1);
		
		block = (text_block_ptr) InsertMemory(pg->t_blocks, 0, num_import_blocks);
		pgBlockMove(import_blocks, block, num_import_blocks * sizeof(text_block));
		
	// See if ending block terminates on a CR or not:
		
		num_blocks = GetMemorySize(pg->t_blocks);
		last_block_num = (pg_short_t)((long)block_num + num_import_blocks);
		
		if (last_block_num < (pg_short_t)num_blocks) {
		
			last_block = (text_block_ptr) UseMemoryRecord(pg->t_blocks, last_block_num - 1, 0, FALSE);
			source_text = (pg_char_ptr) UseMemory(last_block->text);
			end_char = (pg_short_t)source_text[last_block->end - last_block->begin - 1];
			
			if (end_char != pg->globals->line_wrap_char) {
				long			text_size;
				
				text_size = GetMemorySize(last_block->text);
				text = (pg_char_ptr) InsertMemory(last_block[1].text, 0, text_size);
				pgBlockMove(source_text, text, text_size * sizeof(pg_char));
				last_block[1].end += text_size;
				last_block[1].flags |= NEEDS_CALC;
				UnuseMemory(last_block[1].text);
				
				DisposeNonNilMemory(last_block->text);
				DisposeMemory(last_block->lines);
				
				UnuseMemory(pg->t_blocks);
				DeleteMemory(pg->t_blocks, last_block_num - 1, 1);
				
				block = (text_block_ptr) UseMemory(pg->t_blocks);
			}
		}
	}
	else {
		
		text = (pg_char_ptr) InsertMemory(block->text, local_position, first_block_size);
		pgBlockMove(UseMemory(import_blocks->text), text, first_block_size * sizeof(pg_char));
		UnuseMemory(import_blocks->text);
		UnuseMemory(block->text);

		block->flags = NEEDS_CALC;
		block->end += first_block_size;
	}
	
	block = (text_block_ptr) UseMemoryRecord(pg->t_blocks, (long)block_num, 0, FALSE);
	
	num_blocks = GetMemorySize(pg->t_blocks) - (long)block_num;
	end_pos = block->begin;

	while (num_blocks) {
		
		block->end = (block->end - block->begin) + end_pos;
		block->begin = end_pos;
		end_pos = block->end;
		block->flags |= NEEDS_PAGINATE;

		++block;
		--num_blocks;
	}

	UnuseMemory(pg->t_blocks);
}


/* find_breaking_char returns the first text_block breaking char that it finds. If one is found
then its position + 1 is returned. */

static long find_breaking_char (pg_char_ptr text, long text_size, pg_char cr_char)
{
	register pg_char_ptr		text_ptr;
	register long				ctr;
	
	text_ptr = text;
	
	for (ctr = 1; ctr <= text_size; ++ctr)
		if (*text_ptr++ == cr_char)
			return	ctr;
	
	return		0;
}


/* find_par_position locates the text position for the current paragraph. This only gets
called after a new insertion so the ending byte is the current end-of-doc. */

static long find_par_position (pg_import_ptr import_ptr, pg_char cr_char)
{
	text_block_ptr			block;
	register pg_char_ptr	text;
	long					text_size, position, num_blocks;
	
	if (import_ptr->par_format_verb == par_mark)
		return	import_ptr->par_format_mark;
	
	num_blocks = GetMemorySize(import_ptr->t_blocks) - 1;
	block = (text_block_ptr) UseMemoryRecord(import_ptr->t_blocks, num_blocks, 0, TRUE);

	if (import_ptr->par_format_verb == par_forward)
		position = block->begin + GetMemorySize(block->text);
	else {	

		if ((text_size = GetMemorySize(block->text)) == 0)
			if (num_blocks)
				--block;

		if ((text_size = GetMemorySize(block->text)) > 0) {
		
			text = (pg_char_ptr) UseMemory(block->text);
			text_size -= 1;
			text += text_size;

			while (text_size) {
				
				--text;
				
				if (*text == cr_char)
					break;
				
				--text_size;
			}

			UnuseMemory(block->text);
		}

		position = block->begin + text_size;
	}

	UnuseMemory(import_ptr->t_blocks);
	
	return		position;
}

/* insert_default_item inserts the default beginning record if there are no records so far. 
If something is done,  the style_item + 1 is returned. */

static long insert_default_item (memory_ref runref, pg_short_t style_item)
{
	style_run_ptr			run;
	long					result = 0;

	if (GetMemorySize(runref) == 0) {
		
		run = (style_run_ptr) AppendMemory(runref, 1, TRUE);
		run->style_item = style_item;
		result = (long)style_item;
		++result;
		UnuseMemory(runref);
	}
	
	return	result;
}

/* set_embed_info sets the low level style index inside an embed -if- this style_info is
intended for an embed_ref. */

static void set_embed_info (paige_rec_ptr pg, pg_import_ptr import_ptr, style_info_ptr embed_style, pg_short_t style_item)
{
	memory_ref			embed;
	pg_embed_ptr		embed_ptr;

	if ((embed = embed_style->embed_object) != MEM_NULL) {
		
		embed_ptr = (pg_embed_ptr) UseMemory(embed);
		embed_ptr->lowlevel_index = (long)style_item;
		UnuseMemory(embed);
		
		import_ptr->last_was_embed = TRUE;
	}
	else
		import_ptr->last_was_embed = FALSE;
}

/* compare_messages returns true if both are the same. Only msg2 can be NULL*/

static pg_boolean compare_messages (pg_char_ptr msg1, pg_char_ptr msg2)
{
	short		index;
	
	if (!msg2)
		return	(pg_boolean)(*msg1 == 0);
	
	index = 0;
	
	for (;;) {
		
		if (msg1[index] != msg2[index])
			return	FALSE;
		if (msg1[index] == 0)
			break;

		++index;
	}
	
	return	TRUE;
}

