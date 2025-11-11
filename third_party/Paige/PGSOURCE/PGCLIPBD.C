/* This file handles all "clipboard" functionality such as cut, copy, paste. It
also contains all the Undo support.

Copyright 1993 by DataPak Software, Inc.			*/

/* Modified Dec 22 1994 for the use of volatile in PG_TRY-PG_CATCH by TR Shaw, OITC */

#include <string.h>
#include "Paige.h"

#ifdef MAC_PLATFORM
#pragma segment pgclipbd
#endif

#include "pgExceps.h"
#include "pgText.h"
#include "pgDefStl.h"
#include "pgBasics.h"
#include "pgSelect.h"
#include "pgEdit.h"
#include "pgShapes.h"
#include "pgUtils.h"
#include "machine.h"
#include "defprocs.h"
#include "pgClipBd.h"
#include "pgTxtWid.h"
#include "pgDefPar.h"
#include "pgSubRef.h"
#include "pgHText.h"
#include "pgTables.h"
#include "pgOSUtl.h"

#define LARGE_COPY_SIZE			24000

static void execute_paste (pg_ref pg, pg_ref paste_ref, long position,
		short text_only, short draw_mode, pg_boolean doing_undo);
static pg_short_t get_style_index (paige_rec_ptr src_pg, paige_rec_ptr target_pg,
		short reason_verb, style_run_ptr src_run, style_run_ptr target_run,
		pg_short_t wanted_style_item, pg_short_t resolved_styles);
static void apply_text_styles (const paige_rec_ptr src_pg, paige_rec_ptr target_pg,
		style_info_ptr terminating_style, short reason_verb,
		select_pair_ptr src_range, select_pair_ptr target_range);
static void apply_paragraph_styles (paige_rec_ptr src_pg, paige_rec_ptr target_pg,
		select_pair_ptr src_range, select_pair_ptr target_range, short doing_undo);
static void prepare_style_undo (paige_rec_ptr pg, pg_undo_ptr undo_ptr);
static void do_style_undo (paige_rec_ptr pg, pg_undo_ptr undo_ptr);
static void make_duplicate_formats (paige_rec_ptr src_pg, paige_rec_ptr target_pg,
	short reason_verb, format_ref text_formats, par_ref par_formats);
static void perform_undo_delete (paige_rec_ptr pg, pg_undo_ptr undo_ptr);
static void perform_backspace_undo (paige_rec_ptr pg, pg_undo_ptr undo_ptr);
static undo_ref prepare_delete_or_paste_redo (paige_rec_ptr pg, pg_undo_ptr undo_ptr,
		short requires_redo);
static void do_pg_undo_delete_proc (pg_undo_ptr undo_stuff);
static void do_delete_from_ptr (style_info_ptr style, pg_char_ptr text,
		pg_short_t length);
static memory_ref copy_applied_range (paige_rec_ptr from_pg, size_t added_offset, pg_boolean force_select);
static long total_text_to_copy(select_pair_ptr selections, long num_selects);
static pg_boolean par_changed (paige_rec_ptr pg, par_info_ptr par);
static long insert_backspace_undo (paige_rec_ptr pg, pg_undo_ptr undo_ptr);
static pg_short_t add_keyboard_style (paige_rec_ptr pg, pg_undo_ptr undo_ptr, style_info_ptr style);
static pg_short_t add_keyboard_par (paige_rec_ptr pg, pg_undo_ptr undo_ptr, par_info_ptr par_style);
static void prepare_doc_info_undo (paige_rec_ptr pg, pg_undo_ptr undo_ptr);
static void prepare_shape_undo (shape_ref the_shape, memory_ref refcon_ref,
		pg_undo_ptr undo_ptr);
static void do_shape_undo (paige_rec_ptr pg_rec, shape_ref the_shape,
		memory_ref PG_FAR *refcon_ref, pg_undo_ptr undo_ptr, pg_boolean inval_text);
static void dispose_undo_structs (pg_undo_ptr undo_ptr);
static undo_ref prepare_keyboard_undo (paige_rec_ptr pg, short verb,
		undo_ref previous_undo);
static void select_for_undo (pg_ref pg, pg_undo_ptr undo_ptr,
		pg_boolean use_only_list);
static pg_short_t insert_target_style (paige_rec_ptr pg, paige_rec_ptr source_pg, short reason_verb,
			style_info_ptr style);
//QUALCOMM Begin
//Need the mem_id of the target pg_ref in order to allocate memory for long
//URLs but assign the ownership to the target_ref so that the memory ref stays 
//valid. Added the last parameter to achieve this.
//
//static void apply_hyperlinks (const memory_ref source_links, memory_ref target_links,
//		select_pair_ptr src_range, select_pair_ptr target_range);
static void apply_hyperlinks (const memory_ref source_links, memory_ref target_links,
		select_pair_ptr src_range, select_pair_ptr target_range, paige_rec_ptr target_pg_ptr);
//QUALCOMM End
static void restore_subref_state (paige_rec_ptr pg, pg_undo_ptr undo_ptr);
static void insert_undo_subref (paige_rec_ptr pg, pg_subref subref);

/* pgCut deletes the currently selected text and returns a copy of it  */

PG_PASCAL (pg_ref) pgCut (pg_ref pg, const select_pair_ptr selection, short draw_mode)
{
	pg_globals_ptr		globals;
	long				old_change_ctr;
	volatile pg_ref		copy_result = MEM_NULL;
	
	globals = pgGetGlobals(pg);
	
	PG_TRY(globals->mem_globals) {
	
		copy_result = pgCopy(pg, selection);
		pgFailNIL(globals->mem_globals, (void PG_FAR *) copy_result);

		old_change_ctr = pgGetChangeCtr(pg);
		pgDelete(pg, selection, draw_mode);
		++old_change_ctr;
		pgSetChangeCtr(pg, old_change_ctr);
	}
	
	PG_CATCH {

		if (copy_result)
			pgFailureDispose(copy_result);
		
		return	MEM_NULL;
	}
	
	PG_ENDTRY;

	return	copy_result;
}


/* pgCopy returns a pg_ref containing text and styles of the selection
specified.  If selection is NULL the current selection is used.  */

PG_PASCAL (pg_ref) pgCopy (pg_ref pg, const select_pair_ptr selection)
{
	paige_rec_ptr				pg_rec, pg_copy;
	pg_globals_ptr				globals;
	t_select_ptr				select_ptr;
	select_pair_ptr				selections;
	select_pair					target_apply;
	text_block_ptr				block;
	pg_char_ptr					text;
	size_t						begin_sel, end_sel, local_begin, copy_size;
	size_t						copy_base, progress, completion;
	pg_short_t					num_selects, old_insert;
	short						should_call_wait;
	volatile memory_ref			apply_ref = MEM_NULL;
	volatile pg_ref				copy_result = MEM_NULL;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	select_ptr = (t_select_ptr) UseMemory(pg_rec->select);
	UnuseMemory(pg_rec->select);

	globals = pg_rec->globals;

	PG_TRY(globals->mem_globals) {

		if (apply_ref = pgSetupOffsetRun(pg_rec, selection, FALSE, FALSE)) {
			
			num_selects = (pg_short_t)GetMemorySize(apply_ref);
			selections = (select_pair_ptr) UseMemory(apply_ref);
			completion = total_text_to_copy(selections, num_selects);
			should_call_wait = (completion > LARGE_COPY_SIZE);
	
			copy_base = selections->begin;
	
			copy_result = pgNewShell(globals);
			pg_copy = (paige_rec_ptr) UseMemory(copy_result);
			pg_copy->flags2 |= (pg_rec->flags2 & APPLY_ALL_PAR_INFOS);
			progress = 0;
			pg_copy->insert_style = NULL_RUN;
			old_insert = pg_copy->insert_style;

			pg_copy->flags2 |= NO_STYLE_CLEANUP;

			while (num_selects) {
				
				target_apply.begin = pg_copy->t_length;
				begin_sel = selections->begin;
				
				while (begin_sel < selections->end) {
	
					if (should_call_wait)
						pg_rec->procs.wait_proc(pg_rec, copy_wait, progress, completion);
	
					block = pgFindTextBlock(pg_rec, begin_sel, NULL, FALSE, TRUE);

					if (GetMemorySize(block->subref_list) > 0)
						pgInvalCharLocs(pg_rec);

					end_sel = selections->end;
					
					if (end_sel > block->end)
						end_sel = block->end;
					
					local_begin = begin_sel - block->begin;
					copy_size = end_sel - begin_sel;
					
					text = (pg_char_ptr) UseMemory(block->text);
					text += local_begin;
	
					pgInsert(copy_result, text, copy_size, CURRENT_POSITION,
							data_insert_mode, 0, draw_none);
					UnuseMemory(block->text);
					progress += copy_size;

					UnuseMemory(pg_rec->t_blocks);
	
					begin_sel = end_sel;
				}
				
				target_apply.end = pg_copy->t_length;

				#ifdef PG_DEBUG
					pgCheckRunIntegrity(pg_rec);
					pgCheckRunIntegrity(pg_copy);
				#endif

				apply_text_styles(pg_rec, pg_copy, NULL, copy_reason, selections, &target_apply);
				apply_paragraph_styles(pg_rec, pg_copy, selections, &target_apply, FALSE);
				//QUALCOMM Begin
				//Need the target's mem_id variable to allocate memory for large URLs
				//apply_hyperlinks(pg_rec->hyperlinks, pg_copy->hyperlinks, selections, &target_apply);
				//apply_hyperlinks(pg_rec->target_hyperlinks, pg_copy->target_hyperlinks, selections, &target_apply);
				apply_hyperlinks(pg_rec->hyperlinks, pg_copy->hyperlinks, selections, &target_apply, pg_copy);
				apply_hyperlinks(pg_rec->target_hyperlinks, pg_copy->target_hyperlinks, selections, &target_apply, pg_copy);
				//QUALCOMM End

				pgCleanupStyleRuns(pg_copy, TRUE, TRUE);

				#ifdef PG_DEBUG
					pgCheckRunIntegrity(pg_rec);
					pgCheckRunIntegrity(pg_copy);
				#endif

				selections->begin -= copy_base;
				selections->end -= copy_base;

				++selections;
				--num_selects;
			}
			
			pg_copy->insert_style = old_insert;

			pg_copy->flags2 &= (~NO_STYLE_CLEANUP);
			pgCleanupStyleRuns(pg_copy, TRUE, TRUE);

			if (should_call_wait)
				pg_rec->procs.wait_proc(pg_rec, copy_wait, completion, completion);
	
			pg_copy->applied_range = apply_ref;

			UnuseMemory(apply_ref);
			UnuseMemory(copy_result);
		}
	}
	
	PG_CATCH {
		
		if (apply_ref)
			UnuseFailedMemory(apply_ref);

		if (copy_result)
			pgFailureDispose(copy_result);

		UnuseAllFailedMemory(globals->mem_globals, pg_rec->mem_id);

		return	MEM_NULL;
	}
	
	PG_ENDTRY;

	UnuseMemory(pg);

	return	copy_result;
}


/* pgCopyText returns a memory_ref of all text of specified data type */

PG_PASCAL (text_ref) pgCopyText (pg_ref pg, const select_pair_ptr selection, short data_type)
{
	paige_rec_ptr		pg_rec;
	pg_globals_ptr		globals;
	style_walk			walker;
	text_block_ptr		block;
	pg_char_ptr			text, append;
	select_pair_ptr		selections;
	size_t				begin_sel, end_sel, local_begin, local_size;
	pg_short_t			num_selects;
	volatile memory_ref	select_list = MEM_NULL;
	volatile text_ref	copy_result = MEM_NULL;

	pg_rec = (paige_rec_ptr) UseMemory(pg);

	globals = pg_rec->globals;

	PG_TRY(globals->mem_globals) {

		if (select_list = pgSetupOffsetRun(pg_rec, selection, FALSE, FALSE)) {
		
			copy_result = MemoryAlloc(pg_rec->globals->mem_globals, sizeof(pg_char), 0, 512);
			num_selects = (pg_short_t)GetMemorySize(select_list);
			selections = (select_pair_ptr) UseMemory(select_list);
			
			pgPrepareStyleWalk(pg_rec, 0, &walker, FALSE);

			while (num_selects) {
				
				begin_sel = selections->begin;
				
				pgSetWalkStyle(&walker, begin_sel);
				
				while (begin_sel < selections->end) {
				
					block = pgFindTextBlock(pg_rec, begin_sel, NULL, FALSE, TRUE);
					
					end_sel = selections->end;
	
					if (end_sel > block->end)
						end_sel = block->end;
					
					if (end_sel > walker.next_style_run->offset)
						end_sel = walker.next_style_run->offset;
	
					local_begin = begin_sel - block->begin;
					local_size = end_sel - begin_sel;
					
					text = (pg_char_ptr) UseMemory(block->text);
					
					if (pgBytesMatchCriteria(pg_rec, &walker, text, data_type,
							block->begin, local_begin, local_size)) {
						
						append = (pg_char_ptr) AppendMemory(copy_result, local_size, FALSE);
						pgBlockMove(&text[local_begin], append, local_size * sizeof(pg_char));
						UnuseMemory(copy_result);
					}
					
					UnuseMemory(block->text);
					UnuseMemory(pg_rec->t_blocks);
					
					begin_sel = end_sel;
					pgWalkStyle(&walker, local_size);
				}
				
				++selections;
				--num_selects;
			}
			
			pgPrepareStyleWalk(pg_rec, 0, NULL, FALSE);
			UnuseAndDispose(select_list);
		}
	}
	
	PG_CATCH {
		
		if (copy_result)
			DisposeFailedMemory(copy_result);
		if (select_list)
			DisposeFailedMemory(select_list);

		UnuseAllFailedMemory(globals->mem_globals, pg_rec->mem_id);
		
		return	MEM_NULL;
	}
	
	PG_ENDTRY;

	UnuseMemory(pg);
	return	copy_result;
}


/* pgPaste inserts text and styles from paste_ref. If text is selected it
gets deleted first.  */

PG_PASCAL (void) pgPaste (pg_ref pg, pg_ref paste_ref, long position,
		short text_only, short draw_mode)
{
	execute_paste(pg, paste_ref, position, text_only, draw_mode, FALSE);
}



/* pgPrepareUndo sets up for an Undo, based on the verb (which will be what is
about to occur).  It returns a special memory_ref that can be used to Undo.
The insert_ref is used only for certain undo's: if undo_insert, then insert_ref
is a *long of the insertion length. If undo_typing, insert_ref is the last
undo_ref passed for the same undo preparation (or NULL if none).  */

PG_PASCAL (undo_ref) pgPrepareUndo (pg_ref pg, short verb, void PG_FAR *insert_ref)
{
	paige_rec_ptr		pg_rec, eventual_paste;
	pg_globals_ptr		globals;
	select_pair_ptr		applied_insert;
	long PG_FAR			*long_ptr;
	long PG_FAR			*optional_position;
	long				fake_insert_size, flags_save;
	pg_undo_ptr			undo_ptr;
	volatile undo_ref	result = MEM_NULL;
	/*QUALCOMM , Begin Hack*/
	memory_ref resultMem;
	pg_short_t		num_selects;
	/*QUALCOMM End Hack*/

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	flags_save = (pg_rec->flags2 & APPLY_ALL_PAR_INFOS);
	pg_rec->flags2 |= APPLY_ALL_PAR_INFOS;

	globals = pg_rec->globals;
	long_ptr = NULL;
	optional_position = NULL;

	PG_TRY(globals->mem_globals) {
		
		if ((verb == undo_typing) || (verb == undo_backspace) || (verb == undo_fwd_delete)) {
		
			if (result = prepare_keyboard_undo(pg_rec, verb, (undo_ref) insert_ref))
				undo_ptr = (pg_undo_ptr) UseMemory(result);
		}
		else {
		
			result = MemoryAllocClear(globals->mem_globals, sizeof(pg_undo), 1, 0);
			undo_ptr = (pg_undo_ptr) UseMemory(result);
			undo_ptr->globals = globals;
			undo_ptr->verb = undo_ptr->real_verb = verb;
			undo_ptr->applied_range = pgSetupOffsetRun(pg_rec, NULL, FALSE, FALSE);
	
	/* Note for something that could be confusing:  The only way the verb parameter
	will be -undo_typing is when I prapare for a re-do typing the very first time.
	If so, the insert_ref parameter will contain the original undo_ref so I can
	determine how to set it up. Otherwise, the verb will be a "fake" verb, either
	undo_delete or undo_paste or a negative version of either.  */
	
			if (verb == -undo_typing) {
				pg_undo_ptr			original_undo;

				if (insert_ref) {
		
					if (undo_ptr->applied_range)
						DisposeMemory(undo_ptr->applied_range);

					undo_ptr->applied_range = MEM_NULL;
		
					original_undo = (pg_undo_ptr) UseMemory((memory_ref) insert_ref);
					
					if (original_undo->keyboard_ref) {  /* original is undo backspace */
						
						undo_ptr->real_verb = undo_paste;
		
						if (original_undo->keyboard_delete.end > original_undo->keyboard_delete.begin) {
							
							pgSetSelection(pg, original_undo->keyboard_delete.begin,
									original_undo->keyboard_delete.end, 0, FALSE);
		
							undo_ptr->data = pgCopy(pg, NULL);
						}
						
						undo_ptr->alt_range.begin = original_undo->alt_range.begin;
						undo_ptr->alt_range.end = undo_ptr->alt_range.begin
								+ GetMemorySize(original_undo->keyboard_ref);
						
						if (original_undo->data)
							undo_ptr->alt_range.end += pgTextSize(original_undo->data);
		
						/*QUALCOMM Begin Hack*/
						if ( (original_undo->verb == undo_fwd_delete) || (original_undo->verb == undo_backspace) )
						{
							if (undo_ptr->alt_range.end < pg_rec->t_length)
								undo_ptr->applied_range = pgSetupOffsetRun(pg_rec, &undo_ptr->alt_range, FALSE, FALSE);
							else
							{
								if (undo_ptr->alt_range.begin == undo_ptr->alt_range.end)
									undo_ptr->applied_range = MEM_NULL;
								else
								{
									resultMem = MemoryAlloc(pg_rec->globals->mem_globals, sizeof(select_pair), (long)1,
											(short)(pg_rec->num_selects * 2));

									pgBlockMove(&undo_ptr->alt_range, UseMemory(resultMem), sizeof(select_pair));
									
									UnuseMemory(resultMem);
									num_selects = (pg_short_t)GetMemorySize(resultMem);
									if (num_selects == 0) {
										
										DisposeMemory(resultMem);										
										undo_ptr->applied_range = MEM_NULL;
									}
									else 
										undo_ptr->applied_range = resultMem;
								}
							}
						}
						else	/*QUALCOMM End Hack*/
						undo_ptr->applied_range = pgSetupOffsetRun(pg_rec, &undo_ptr->alt_range, FALSE, FALSE);
					}
					else {	/* original is undo forward keys or undo backspace with highlight */
						
						undo_ptr->real_verb = -undo_paste;
						undo_ptr->alt_range.begin = original_undo->alt_range.begin;
						undo_ptr->alt_range.end = undo_ptr->alt_range.begin;

						pgSetSelection(pg, original_undo->alt_range.begin,
								original_undo->alt_range.end, 0, FALSE);

						undo_ptr->data = pgCopy(pg, NULL);
						
						if (original_undo->data) {
							paige_rec_ptr		original_copy;
							
							original_copy = (paige_rec_ptr) UseMemory(original_undo->data);
							undo_ptr->applied_range = copy_applied_range(original_copy, undo_ptr->alt_range.begin, FALSE);
							UnuseMemory(original_undo->data);
							
							if (undo_ptr->data == MEM_NULL)
								undo_ptr->data = pgDuplicate(original_undo->data);
						}
					}
		
					UnuseMemory((memory_ref) insert_ref);
				}
			}
			else {
	
				switch (undo_ptr->real_verb) {
					
					case -undo_doc_info:
						prepare_doc_info_undo(pg_rec, undo_ptr);
						break;
	
					case -undo_exclude_change:
						prepare_shape_undo(pg_rec->exclude_area, pg_rec->exclusions, undo_ptr);
						break;
						
					case -undo_vis_change:
						prepare_shape_undo(pg_rec->vis_area, MEM_NULL, undo_ptr);
						break;
						
					case -undo_page_change:
						prepare_shape_undo(pg_rec->wrap_area, pg_rec->containers, undo_ptr);
						break;
	
					case -undo_insert:
					case -undo_embed_insert:
					case -undo_app_insert:
						undo_ptr->data = pgCopy(pg, NULL);
						undo_ptr->alt_range.begin = pgCurrentInsertion(pg_rec);
						undo_ptr->alt_range.end = undo_ptr->alt_range.begin;
						break;
						
					case -undo_format:
						prepare_style_undo(pg_rec, undo_ptr);
						break;
					
					case -undo_paste:
						undo_ptr->data = pgCopy(pg, NULL);
						undo_ptr->alt_range.begin = pgCurrentInsertion(pg_rec);
						undo_ptr->alt_range.end = undo_ptr->alt_range.begin;
						break;
						
					case -undo_delete:
						break;
			
					case undo_delete:
						undo_ptr->data = pgCopy(pg, (select_pair_ptr)insert_ref);
						
						if (insert_ref) {
							
							if (undo_ptr->applied_range)
								DisposeMemory(undo_ptr->applied_range);

							undo_ptr->applied_range = pgSetupOffsetRun(pg_rec,
									(select_pair_ptr)insert_ref, FALSE, FALSE);
						}
						
						break;

					case undo_paste:
						if (pg_rec->num_selects)
							undo_ptr->data = pgCopy(pg, NULL);
		
						eventual_paste = (paige_rec_ptr) UseMemory((memory_ref) insert_ref);
						undo_ptr->alt_range.begin = pgCurrentInsertion(pg_rec);
						undo_ptr->alt_range.end = eventual_paste->t_length
								+ undo_ptr->alt_range.begin;

						if (undo_ptr->applied_range)
							DisposeMemory(undo_ptr->applied_range);
						
						pgSetSelection(eventual_paste->myself, 0, eventual_paste->t_length, 0, FALSE);
						undo_ptr->applied_range = copy_applied_range(eventual_paste,
								undo_ptr->alt_range.begin, TRUE);
	
						UnuseMemory((memory_ref) insert_ref);
						
						break;
						
					case undo_format:
						prepare_style_undo(pg_rec, undo_ptr);
						break;
					
					case undo_embed_insert:
					case undo_app_insert:
					case undo_insert:
						
						if (undo_ptr->real_verb == undo_embed_insert) {
						
							fake_insert_size = 2;
							long_ptr = &fake_insert_size;
							optional_position = (long PG_FAR *)insert_ref;
						}
						else
						if (undo_ptr->real_verb == undo_app_insert) {
							
							optional_position = (long PG_FAR *)insert_ref;
							long_ptr = &optional_position[1];
							undo_ptr->real_verb = undo_insert;
						}
						else
							long_ptr = (long *) insert_ref;
						
						if (pg_rec->num_selects && (verb != undo_app_insert))
							undo_ptr->data = pgCopy(pg, NULL);
						
						if (optional_position)
							undo_ptr->alt_range.begin = pgFixOffset(pg_rec, *optional_position);
						else
							undo_ptr->alt_range.begin = pgCurrentInsertion(pg_rec);

						undo_ptr->alt_range.end = *long_ptr + undo_ptr->alt_range.begin;
						
						if (undo_ptr->applied_range)
							DisposeMemory(undo_ptr->applied_range);
	
						undo_ptr->applied_range = MemoryAlloc(pg_rec->globals->mem_globals, sizeof(select_pair), 1, 0);
						applied_insert = (select_pair_ptr) UseMemory(undo_ptr->applied_range);
						
						*applied_insert = undo_ptr->alt_range;
						UnuseMemory(undo_ptr->applied_range);
						break;

					case undo_page_change:
						prepare_shape_undo(pg_rec->wrap_area, pg_rec->containers, undo_ptr);
						break;
	
					case undo_vis_change:
						prepare_shape_undo(pg_rec->vis_area, MEM_NULL, undo_ptr);
						break;
	
					case undo_exclude_change:
						prepare_shape_undo(pg_rec->exclude_area, pg_rec->exclusions, undo_ptr);
						break;
	
					case undo_doc_info:
						prepare_doc_info_undo(pg_rec, undo_ptr);
						break;
				}
			}
		
		}
		
		if (result)
			pg_rec->procs.undo_enhance(pg_rec, undo_ptr, insert_ref, enhance_prepared_undo);		
	}

	PG_CATCH {

		pgDisposeFailedUndo(result);
		UnuseAllFailedMemory(globals->mem_globals, pg_rec->mem_id);
		
		return	MEM_NULL;
	}
	
	PG_ENDTRY;
	
	if (result) {
		
		undo_ptr->rsrv = pg_rec->active_subset;

		UnuseMemory(result);
	}

	pg_rec->flags2 &= (~APPLY_ALL_PAR_INFOS);
	pg_rec->flags2 |= flags_save;

	UnuseMemory(pg);
	
	return	result;
}


/* pgUndo does the actual Undo. If requires_redo is TRUE, the function returns
a new undo_ref with an inverted verb (e.g., -undo_paste is "redo paste"). */

PG_PASCAL (undo_ref) pgUndo (pg_ref pg, undo_ref ref, pg_boolean requires_redo,
		short draw_mode)
{
	paige_rec_ptr			pg_rec;
	pg_globals_ptr			globals;
	long					old_change_ctr, old_flags2;
	short					use_draw_mode;
#ifndef PG_VOLATILE
	volatile undo_ref		result = MEM_NULL;
	volatile pg_undo_ptr	undo_ptr = NULL;
#else
	undo_ref 				result;
	pg_undo_ptr 			undo_ptr;
	
	PGVolatile(result);
	PGVolatile(undo_ptr);

	result = MEM_NULL;
	undo_ptr = NULL;
#endif
	
	pg_rec = (paige_rec_ptr) UseMemory(pg);
	globals = pg_rec->globals;
	
	if ((use_draw_mode = draw_mode) == best_way)
		use_draw_mode = bits_copy;

	PG_TRY(globals->mem_globals) {

		undo_ptr = (pg_undo_ptr) UseMemory(ref);
		restore_subref_state(pg_rec, undo_ptr);
		if (use_draw_mode && (!(pg_rec->flags & DEACT_BITS)))
			pgTurnOffHighlight(pg_rec, TRUE);

		old_change_ctr = pg_rec->change_ctr;

		if (undo_ptr->real_verb < 0)
			++old_change_ctr;
		else
			--old_change_ctr;

		switch (undo_ptr->real_verb) {

			case -undo_doc_info:
				if (requires_redo)
					result = pgPrepareUndo(pg, undo_doc_info, NULL);
				pgSetDocInfo(pg_rec->myself, (const pg_doc_ptr) UseMemory(undo_ptr->doc_data), TRUE, draw_none);
				UnuseMemory(undo_ptr->doc_data);
				
				do_shape_undo(pg_rec, pg_rec->wrap_area, &pg_rec->containers,
					undo_ptr, TRUE);
				
				break;
				
			case -undo_exclude_change:
				if (requires_redo)
					result = pgPrepareUndo(pg, undo_exclude_change, NULL);
				do_shape_undo(pg_rec, pg_rec->exclude_area, &pg_rec->exclusions,
						undo_ptr, TRUE);
				break;

			case -undo_vis_change:
				if (requires_redo)
					result = pgPrepareUndo(pg, undo_vis_change, NULL);
				do_shape_undo(pg_rec, pg_rec->vis_area, NULL, undo_ptr, FALSE);

				break;

			case -undo_page_change:
				if (requires_redo)
					result = pgPrepareUndo(pg, undo_page_change, NULL);
				do_shape_undo(pg_rec, pg_rec->wrap_area, &pg_rec->containers,
					undo_ptr, TRUE);

				break;

			case -undo_insert:
			case -undo_embed_insert:
			case -undo_app_insert:
				select_for_undo(pg, undo_ptr, FALSE);
	
				if (requires_redo) {
					long PG_FAR		*optional_param;
					long			ins_length;
					
					if (undo_ptr->real_verb == -undo_insert) {
					
						ins_length = pgTextSize(undo_ptr->data);
						optional_param = &ins_length;
					}
					else
						optional_param = NULL;

					result = pgPrepareUndo(pg, (short)-undo_ptr->real_verb, optional_param);
				}
				
				execute_paste(pg, undo_ptr->data, CURRENT_POSITION, FALSE, draw_none, TRUE);
				break;

			case -undo_format:
				select_for_undo(pg, undo_ptr, FALSE);
				
				if (requires_redo)
					result = pgPrepareUndo(pg, undo_format, NULL);
				do_style_undo(pg_rec, undo_ptr);
				
				break;
			
			case -undo_paste:
				select_for_undo(pg, undo_ptr, FALSE);
					
				result = prepare_delete_or_paste_redo(pg_rec, undo_ptr, requires_redo);
				
				execute_paste(pg, undo_ptr->data, CURRENT_POSITION, FALSE, draw_none, TRUE);
				
				break;
			
			case -undo_delete:
				select_for_undo(pg, undo_ptr, TRUE);
				old_flags2 = pg_rec->flags2 & APPLY_ALL_PAR_INFOS;
				pg_rec->flags2 |= APPLY_ALL_PAR_INFOS;
				result = prepare_delete_or_paste_redo(pg_rec, undo_ptr, requires_redo);
				pg_rec->flags2 &= (~APPLY_ALL_PAR_INFOS);
				pg_rec->flags2 |= old_flags2;

				pgDelete(pg, NULL, draw_none);
	
				break;
	
			case undo_typing:
			case undo_backspace:
			case undo_fwd_delete:
			
				if (requires_redo) {
					pg_undo_ptr			redo_ptr;
					
					result = pgPrepareUndo(pg, -undo_typing, (void PG_FAR *) ref);
					redo_ptr = (pg_undo_ptr) UseMemory(result);
					redo_ptr->verb = -undo_ptr->real_verb;
					UnuseMemory(result);
				}

				if (undo_ptr->keyboard_ref)
					perform_backspace_undo(pg_rec, undo_ptr);
				else {
	
					pgSetSelection(pg, undo_ptr->alt_range.begin, undo_ptr->alt_range.end,
							0, FALSE);
					
					if (undo_ptr->alt_range.begin != undo_ptr->alt_range.end)
						pgDelete(pg, NULL, draw_none);
					
					if (undo_ptr->data)
						perform_undo_delete(pg_rec, undo_ptr);
				}
	
				break;

			case undo_delete:
				perform_undo_delete(pg_rec, undo_ptr);
				result = prepare_delete_or_paste_redo(pg_rec, undo_ptr, requires_redo);
					
				break;
	
			case undo_paste:
				select_for_undo(pg, undo_ptr, TRUE);
	
				result = prepare_delete_or_paste_redo(pg_rec, undo_ptr, requires_redo);
				
				pgDelete(pg, NULL, draw_none);
				
				if (undo_ptr->data) {
					paige_rec_ptr		src_data;
					long				current_insert;
					
					current_insert = pgCurrentInsertion(pg_rec);
					execute_paste(pg, undo_ptr->data, CURRENT_POSITION, FALSE, draw_none, TRUE);
					src_data = (paige_rec_ptr) UseMemory(undo_ptr->data);
					
					pgSetSelectionList(pg, src_data->applied_range, current_insert, FALSE);

					UnuseMemory(undo_ptr->data);
				}
				
				if (result) {
					pg_undo_ptr		redo_ptr;
					
					redo_ptr = (pg_undo_ptr) UseMemory(result);
					if (redo_ptr->applied_range)
						DisposeMemory(redo_ptr->applied_range);
					redo_ptr->applied_range = pgSetupOffsetRun(pg_rec, NULL, FALSE, FALSE);
	
					UnuseMemory(result);
				}
				
				break;
				
			case undo_format:
				select_for_undo(pg, undo_ptr, FALSE);

				if (requires_redo)
					result = pgPrepareUndo(pg, -undo_format, NULL);
				do_style_undo(pg_rec, undo_ptr);
				break;

			case undo_insert:
			case undo_embed_insert:
			case undo_app_insert:
				select_for_undo(pg, undo_ptr, TRUE);
				
				if (requires_redo)
					result = pgPrepareUndo(pg, (short)-undo_ptr->real_verb, NULL);
				
				pgDelete(pg, NULL, draw_none);
				
				if (undo_ptr->data) {
					paige_rec_ptr		src_data;
					long				current_insert;
					
					current_insert = pgCurrentInsertion(pg_rec);
					execute_paste(pg, undo_ptr->data, CURRENT_POSITION, FALSE, draw_none, TRUE);
					src_data = (paige_rec_ptr) UseMemory(undo_ptr->data);
					pgSetSelectionList(pg, src_data->applied_range, current_insert, FALSE);
					UnuseMemory(undo_ptr->data);
				}
				
				if (result) {
					pg_undo_ptr		redo_ptr;
					
					redo_ptr = (pg_undo_ptr) UseMemory(result);
					if (redo_ptr->applied_range)
						DisposeMemory(redo_ptr->applied_range);
					redo_ptr->applied_range = pgSetupOffsetRun(pg_rec, NULL, FALSE, FALSE);
	
					UnuseMemory(result);
				}

				break;

			case undo_page_change:
				if (requires_redo)
					result = pgPrepareUndo(pg, -undo_page_change, NULL);
				do_shape_undo(pg_rec, pg_rec->wrap_area, &pg_rec->containers,
						undo_ptr, TRUE);

				break;

			case undo_vis_change:
				if (requires_redo)
					result = pgPrepareUndo(pg, -undo_vis_change, NULL);
				do_shape_undo(pg_rec, pg_rec->vis_area, NULL, undo_ptr, FALSE);

				break;

			case undo_exclude_change:
				if (requires_redo)
					result = pgPrepareUndo(pg, -undo_exclude_change, NULL);
				do_shape_undo(pg_rec, pg_rec->exclude_area, &pg_rec->exclusions,
					undo_ptr, TRUE);

				break;

			case undo_doc_info:
				if (requires_redo)
					result = pgPrepareUndo(pg, -undo_doc_info, NULL);
				pgSetDocInfo(pg_rec->myself, (const pg_doc_ptr) UseMemory(undo_ptr->doc_data), TRUE, draw_none);
				UnuseMemory(undo_ptr->doc_data);

				do_shape_undo(pg_rec, pg_rec->wrap_area, &pg_rec->containers,
					undo_ptr, TRUE);
				break;
		}

		pg_rec->procs.undo_enhance(pg_rec, undo_ptr, NULL, enhance_performed_undo);

		if (use_draw_mode)
			pgUpdateText(pg_rec, &pg_rec->port, 0, pg_rec->t_length, MEM_NULL, NULL,
					use_draw_mode, TRUE);
	}
	
	PG_CATCH {
		
		if (undo_ptr)
			UnuseMemory(ref);

		pgDisposeFailedUndo(result);
		
		UnuseAllFailedMemory(globals->mem_globals, pg_rec->mem_id);
		
		return	MEM_NULL;
	}

	PG_ENDTRY;

	UnuseMemory(ref);
	
	pg_rec->change_ctr = old_change_ctr;
	UnuseMemory(pg);
	
	return	result;
}


/* pgUndoType returns the undo type in ref (which can be NULL)  */

PG_PASCAL (short) pgUndoType (undo_ref ref)
{
	pg_undo_ptr	undo_ptr;
	short		the_verb;
	
	if (!ref)
		the_verb = undo_none;
	else {
		undo_ptr = (pg_undo_ptr) UseMemory(ref);
		the_verb = undo_ptr->verb;
		UnuseMemory(ref);
	}

	return	the_verb;
}


/* pgSetUndoRefCon allows the app to set a long into an undo_ref */

PG_PASCAL (void) pgSetUndoRefCon (undo_ref ref, long refCon)
{
	pg_undo_ptr	undo_ptr;
	
	undo_ptr = (pg_undo_ptr) UseMemory(ref);
	undo_ptr->ref_con = refCon;
	UnuseMemory(ref);
}


/* pgGetUndoRefCon returns whatever is in the ref_con in ref  */

PG_PASCAL (long) pgGetUndoRefCon (undo_ref ref)
{
	pg_undo_ptr	undo_ptr;
	long		ref_con;
	
	if (!ref)
		return	0;

	undo_ptr = (pg_undo_ptr) UseMemory(ref);
	ref_con = undo_ptr->ref_con;
	UnuseMemory(ref);
	
	return	ref_con;
}


/* pgDisposeUndo disposes everything in an undo_ref. For convenience, you can
pass a NULL ref in which case this does nothing.  */

PG_PASCAL (void) pgDisposeUndo (undo_ref ref)
{
	pg_undo_ptr	undo_ptr;
	
	if (ref) {

		undo_ptr = (pg_undo_ptr) UseMemory(ref);
		dispose_undo_structs(undo_ptr);
		UnuseAndDispose(ref);
	}
}


#ifdef PG_DEBUG

/* pgDisposeFailedUndo disposes everything in an undo_ref but ignores the access
counter. This is used when an Undo fails.   */

PG_PASCAL (void) pgDisposeFailedUndo (undo_ref ref)
{
	pg_undo_ptr	undo_ptr;
	
	if (ref) {
	
		undo_ptr = UseMemory(ref);
		
		if (undo_ptr->data)
			pgFailureDispose(undo_ptr->data);
		if (undo_ptr->applied_range)
			DisposeFailedMemory(undo_ptr->applied_range);
		if (undo_ptr->keyboard_ref) {
			
			if (GetMemorySize(undo_ptr->keyboard_ref)) {
			
				do_pg_undo_delete_proc(undo_ptr);
				pgWillDeleteFormats(NULL, undo_ptr->globals, disposeundo_reason, undo_ptr->keyboard_styles, undo_ptr->keyboard_pars);
			}

			DisposeFailedMemory(undo_ptr->keyboard_ref);
			DisposeFailedMemory(undo_ptr->keyboard_styles);
			DisposeFailedMemory(undo_ptr->keyboard_pars);
		}

		DisposeFailedMemory(ref);
	}
}

#endif


/* pgBytesMatchCriteria checks the given text and reports TRUE if it falls within
the specified data type. Added 1/21/94, "global_offset" is the absolute offset
for the block in which text begins.  */

PG_PASCAL (pg_boolean) pgBytesMatchCriteria (paige_rec_ptr pg, style_walk_ptr walker,
		pg_char_ptr text, short data_type, size_t global_offset, size_t offset, size_t length)
{
	register style_info_ptr		style;
	register size_t				offset_ctr;
	register pg_boolean			not_vis_flag;
	
	style = walker->cur_style;
	not_vis_flag = ( (pg->flags & NO_HIDDEN_TEXT_BIT) && style->styles[hidden_text_var]);
	
	switch (data_type) {
	
		case all_visible_data:
			return	!not_vis_flag;
			
		case all_data:
			return	TRUE;
		
		case all_visible_text_chars:
			if (not_vis_flag)
				return	FALSE;

		case all_text_chars:
			return	(style->char_bytes == 0);
			break;
		
		case all_visible_roman:
			if (not_vis_flag)
				return	FALSE;

		case all_roman:
			for (offset_ctr = offset; offset_ctr < length; ++offset_ctr)
				if (style->procs.char_info(pg, walker, text, global_offset, offset, length, offset_ctr,
						NON_ROMAN_BIT | NON_TEXT_BIT))
					return	FALSE;

			break;
	}

	return	TRUE;
}


/************************************** Local Functions *****************************************/

/* execute_paste gets called by both pgPaste and handling undo. This is a separate function
because we need to know if this is Undo versus a regular paste due to the potential difference
of pasting a different par format into the middle of another paragraph. */

static void execute_paste (pg_ref pg, pg_ref paste_ref, long position,
		short text_only, short draw_mode, pg_boolean doing_undo)
{
	volatile paige_rec_ptr		pg_rec;
	volatile paige_rec_ptr		pg_paste;
	volatile pg_globals_ptr		globals;
	select_pair_ptr				applied_base;
	select_pair					target_range, src_range;
	text_block_ptr				block;
	pg_char_ptr					text;
	style_run_ptr				target_run;
	style_info_ptr				termination;
	style_info					terminating_style;
	size_t						old_change_ctr, insert_base, insert_position;
	size_t						src_begin, src_end, src_size, local_begin;
	size_t						apply_to;
	size_t						completion, progress;
	pg_short_t					old_insertion;
	short						should_call_wait, use_draw_mode;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	old_change_ctr = pg_rec->change_ctr;
	globals = pg_rec->globals;
	
	if ((use_draw_mode = draw_mode) == best_way)
		use_draw_mode = bits_copy;

	PG_TRY(globals->mem_globals) {

		if (draw_mode && (!(pg_rec->flags & DEACT_BITS)))
			pgTurnOffHighlight(pg_rec, TRUE);

		if (pg_rec->num_selects)
			pgDelete(pg, NULL, draw_none);
		
		insert_base = pgFixOffset(pg_rec, position);
		pgSetNextInsertIndex(pg_rec);
		
		pg_paste = (paige_rec_ptr) UseMemory(paste_ref);
		completion = pg_paste->t_length;
		should_call_wait = (completion > LARGE_COPY_SIZE);
		progress = 0;
	
		applied_base = NULL;
		insert_position = insert_base;
	
		if (pg_paste->applied_range)
			if (GetMemorySize(pg_paste->applied_range)) {
			
				applied_base = (select_pair_ptr) UseMemory(pg_paste->applied_range);
				
				if (applied_base->end > pg_paste->t_length)
					applied_base->end = pg_paste->t_length;
		}

		src_begin = 0;
		pg_rec->flags2 |= NO_STYLE_CLEANUP;
		
		while (src_begin < pg_paste->t_length) {
			
			src_range.begin = src_begin;
			
			if (applied_base) {
				
				insert_position = insert_base + applied_base->begin;
				apply_to = src_begin + applied_base->end - applied_base->begin;
			}
			else {
				
				insert_position = insert_base + src_begin;
				apply_to = pg_paste->t_length;
			}
			
			if (insert_position > pg_rec->t_length)
				insert_position = pg_rec->t_length;

			target_range.begin = insert_position;
			target_range.end = target_range.begin + apply_to - src_begin;
			
			if (should_call_wait)
				pg_rec->procs.wait_proc(pg_rec, insert_wait, 0, completion);

			if (!text_only) {

				target_run = pgFindStyleRun(pg_rec, insert_position, NULL);
				termination = (style_info_ptr) UseMemoryRecord(pg_rec->t_formats, (long)target_run->style_item, 0, TRUE);
				termination->used_ctr += 1;
				terminating_style = *termination;
				UnuseMemory(pg_rec->t_formats);
				UnuseMemory(pg_rec->t_style_run);
			}
			
			old_insertion = pg_rec->insert_style;
			pg_rec->insert_style = NULL_RUN;

			while (src_begin < apply_to) {
	
				block = pgFindTextBlock(pg_paste, src_begin, NULL, FALSE, TRUE);

				if (GetMemorySize(block->subref_list) > 0)
					pgInvalCharLocs(pg_rec);

				src_end = block->end;

				if (src_end > apply_to)
					src_end = apply_to;
	
				src_size = src_end - src_begin;
				local_begin = src_begin - block->begin;
				
				text = (pg_char_ptr) UseMemory(block->text);
				text += local_begin;

				if (!pgPasteToCells(pg_rec, insert_position, text, src_size))
					pgInsert(pg, text, src_size, insert_position, data_insert_mode, 0, draw_none);
				
				progress += src_size;
				
				if (should_call_wait)
					pg_rec->procs.wait_proc(pg_rec, insert_wait, progress, completion);
	
				UnuseMemory(block->text);
				UnuseMemory(pg_paste->t_blocks);
	
				insert_position += src_size;
	
				src_begin = src_end;
			}

			pg_rec->insert_style = old_insertion;

			src_begin = apply_to;
			src_range.end = src_begin;
	
			if (!text_only) {
				pg_short_t			terminator_match;
				
			if (terminator_match = pgFindMatchingStyle((memory_ref) pg_rec->t_formats, &terminating_style,
					0, SIGNIFICANT_STYLE_SIZE)) {
					
					termination = (style_info_ptr) UseMemoryRecord(pg_rec->t_formats, (long)(terminator_match - 1), 0, TRUE);
					termination->used_ctr -= 1;
					UnuseMemory(pg_rec->t_formats);
				}

				apply_text_styles(pg_paste, pg_rec, &terminating_style, paste_reason, &src_range, &target_range);
				apply_paragraph_styles(pg_paste, pg_rec, &src_range, &target_range, doing_undo);

				//QUALCOMM Begin
				//Need the target's mem_id variable to allocate memory for large URLs
				//apply_hyperlinks(pg_paste->hyperlinks, pg_rec->hyperlinks, &src_range, &target_range);
				//apply_hyperlinks(pg_paste->target_hyperlinks, pg_rec->target_hyperlinks, &src_range, &target_range);
				apply_hyperlinks(pg_paste->hyperlinks, pg_rec->hyperlinks, &src_range, &target_range, pg_rec);
				apply_hyperlinks(pg_paste->target_hyperlinks, pg_rec->target_hyperlinks, &src_range, &target_range, pg_rec);
				//QUALCOMM End

				pgCleanupStyleRuns(pg_rec, TRUE, TRUE);

				#ifdef PG_DEBUG
					pgCheckRunIntegrity(pg_rec);
				#endif
			}

			pgInvalSelect(pg_rec->myself, target_range.begin, target_range.end);
			
			if (applied_base)
				++applied_base;
		}

		pg_rec->flags2 &= (~NO_STYLE_CLEANUP);
		pgCleanupStyleRuns(pg_rec, TRUE, TRUE);

		if (applied_base)
			UnuseMemory(pg_paste->applied_range);
	
		UnuseMemory(paste_ref);
	
		pg_rec->change_ctr = old_change_ctr;
		
		if (should_call_wait)
			pg_rec->procs.wait_proc(pg_rec, insert_wait, completion, completion);
	
		++pg_rec->change_ctr;
	
		if (draw_mode)
			pgUpdateText(pg_rec, &pg_rec->port, 0, pg_rec->t_length, MEM_NULL, NULL,
					use_draw_mode, TRUE);
	}
	
	PG_CATCH {
		pg_error			last_error;
		
		last_error = globals->mem_globals->last_error;
		UnuseAllFailedMemory(globals->mem_globals, pg_rec->mem_id);
		UnuseAllFailedMemory(globals->mem_globals, pgGetPaigeRefID(paste_ref));
		pgFailure(globals->mem_globals, last_error, 0);
	}
	
	PG_ENDTRY;

	UnuseMemory(pg);
}


/* This function applies the style(s) from src_pg to target_pg from src_range
to target_range, respectively. It is used for "copy" and "paste." I have also
built into this the copy_text proc in case the class_bits indicate such a
requirement.  */

static void apply_text_styles (const paige_rec_ptr src_pg, paige_rec_ptr target_pg,
		style_info_ptr terminating_style, short reason_verb,
		select_pair_ptr src_range, select_pair_ptr target_range)
{
	register style_info_ptr	new_styles_base;
	register style_run_ptr  start_target = 0;
	register pg_short_t		new_style_index;
	style_run_ptr			src_run, target_run, start_src;
	style_run				ending_run;
	size_t					base_run_offset, run_rec_num;
	pg_short_t				num_runs, num_resolved;

	pgCopySubRefs(src_pg, target_pg, src_range, target_range->begin);
	pgFixBadBlocks(target_pg);

	src_run = start_src = pgFindStyleRun(src_pg, src_range->begin, NULL);
	
	for (num_runs = 1; src_run[num_runs].offset < src_range->end; ++num_runs) ;
	
	target_run = pgFindStyleRun(target_pg, target_range->begin, &run_rec_num);
	ending_run = *target_run;

	if (terminating_style)
		ending_run.style_item = insert_target_style(target_pg, src_pg, reason_verb, terminating_style);
		
	if (target_run->offset == target_range->begin) {
		
		UnuseMemory(target_pg->t_style_run);
		DeleteMemory(target_pg->t_style_run, run_rec_num, 1);
	}
	else
		++run_rec_num;
	
	target_run = (style_run_ptr) InsertMemory(target_pg->t_style_run, run_rec_num, num_runs + 1);
	base_run_offset = src_run->offset;
	num_resolved = 0;

	while (num_runs) {
		
		target_run->offset = src_run->offset - base_run_offset + target_range->begin;
		target_run->style_item = get_style_index(src_pg, target_pg, reason_verb,
				start_src, start_target, src_run->style_item, num_resolved);
		
		base_run_offset = src_range->begin;
		
		++src_run;
		++target_run;
		++num_resolved;
		--num_runs;
	}
	
	target_run->offset = target_range->end;
	target_run->style_item = ending_run.style_item;
	++num_resolved;
	
	new_styles_base = (style_info_ptr) UseMemory(target_pg->t_formats);
	
	while (num_resolved) {
		
		new_style_index = start_target->style_item;

		++new_styles_base[new_style_index].used_ctr;
		
		if (new_styles_base[new_style_index].styles[small_caps_var]) {
			long		small_caps_index;
			
			small_caps_index = new_styles_base[new_style_index].small_caps_index;
			new_styles_base[small_caps_index].used_ctr += 1;
		}

		if (new_styles_base[new_style_index].class_bits & REQUIRES_COPY_BIT) {
			style_info_ptr		style_for_copy;
			text_block_ptr		block;
			pg_char_ptr			text;
			long				copy_length;
			
			style_for_copy = new_styles_base + new_style_index;
			block = pgFindTextBlock(target_pg, start_target->offset, NULL, FALSE, TRUE);
			text = (pg_char_ptr) UseMemory(block->text);
			text += (start_target->offset - block->begin);
			
			copy_length = start_target[1].offset;
			if (copy_length > block->end)
				copy_length = block->end;
			
			copy_length -= start_target->offset;

			style_for_copy->procs.copy_text(src_pg, target_pg, reason_verb,
					style_for_copy, start_target->offset, start_target->offset,
					text, copy_length);
			
			UnuseMemory(block->text);
			UnuseMemory(target_pg->t_blocks);
		}

		++start_target;
		--num_resolved;
	}

	UnuseMemory(target_pg->t_formats);
	UnuseMemory(target_pg->t_style_run);
	UnuseMemory(src_pg->t_style_run);	
}


/* apply_hyperlinks applies source_links to target_links. */
//QUALCOMM Begin
//Need the last parameter to allocate memory for long URLs in the
//target ref
//static void apply_hyperlinks (const memory_ref source_links, memory_ref target_links,
//		select_pair_ptr src_range, select_pair_ptr target_range)
static void apply_hyperlinks (const memory_ref source_links, memory_ref target_links,
		select_pair_ptr src_range, select_pair_ptr target_range, paige_rec_ptr target_pg_ptr)
//QUALCOMM End
{
	pg_hyperlink_ptr		first_source, source, target;
	long					index, source_qty;
	pg_char_ptr				string, URL;
	size_t					string_length;

	
	source = pgFindHypertextRun(source_links, src_range->begin, NULL);
	
	if (source->applied_range.begin < src_range->begin)
		++source;
	
	source_qty = 0;
	first_source = source;

	while (source->applied_range.end <= src_range->end) {
		
		++source;
		++source_qty;
	}
	
	if (source_qty > 0) {
		
		pgFindHypertextRun(target_links, target_range->begin, &index);
		target = (pg_hyperlink_ptr) InsertMemory(target_links, index, source_qty);
		
		while (source_qty) {
			
			*target = *first_source;
			//QUALCOMM Begin
			//The alt_URL variable is not duplicated and when the pg_ref related to the 
			//target links [copy_ref] is deleted, it renders the source link invalid
				if (first_source->alt_URL)
				{
					string = (pg_char_ptr) UseMemory(first_source->alt_URL);
					string_length = pgCStrLength((pg_c_string_ptr)string);
	
				if (string_length) 
				{
					target->alt_URL = MemoryAllocClearID(GetGlobalsFromRef(source_links),
												sizeof(pg_char), string_length + 1, 0, target_pg_ptr->mem_id);
					URL = (pg_char_ptr) UseMemory(target->alt_URL);
					pgBlockMove(string, URL, string_length);
					URL[string_length] = 0;
					UnuseMemory(target->alt_URL);
				}

				UnuseMemory(first_source->alt_URL);
			}
			//QUALCOMM End
			++first_source;
			target->applied_range.begin += (target_range->begin - src_range->begin);
			target->applied_range.end += (target_range->begin - src_range->begin);
			target->unique_id = pgAssignLinkID(target_links);		// Added CPB 3/11/97 to give each a unique id (this might be slow for large pastes)	
			++target;
			--source_qty;
		}
		
		UnuseMemory(target_links);
	}
	
	UnuseMemory(source_links);
}


/* This function adds a new style_info record to target_pg if the style doesn't
already exist. It gets called from above (apply_text_styles) to obtain the style_item
value. For performance purposes, the source and target style runs that have been
resolved thus far are given as parameters, then if the style_item value has
already been looked at then the target run can be used as a "cross table" as
opposed to checking for/adding a whole style_info. The resolved_styles parameter
is the number of style_run records that have been resolved, beginning with
src_run.  The style item in question is wanted_style_item */

static pg_short_t get_style_index (paige_rec_ptr src_pg, paige_rec_ptr target_pg,
		short reason_verb, style_run_ptr src_run, style_run_ptr target_run,
		pg_short_t wanted_style_item, pg_short_t resolved_styles)
{
	pg_short_t				cross_ctr;
	style_info				new_style;
	font_info				new_font;
		
	for (cross_ctr = 0; cross_ctr < resolved_styles; ++cross_ctr) {
		
		if (wanted_style_item == src_run[cross_ctr].style_item)
			return	target_run[cross_ctr].style_item;
	}
	
	GetMemoryRecord(src_pg->t_formats, wanted_style_item, &new_style);
	
	if (new_style.font_index != DEFAULT_FONT_INDEX) {
	
		GetMemoryRecord(src_pg->fonts, new_style.font_index, &new_font);
		new_style.font_index = pgAddNewFont(target_pg, &new_font);
	}

	return	insert_target_style(target_pg, src_pg, reason_verb, &new_style);
}


/* insert_target_style inserts a new style_info into pg if it does not exist. Also,
if super_impose_var is non-zero it makes sure that the stylesheet is also added. */

static pg_short_t insert_target_style (paige_rec_ptr pg, paige_rec_ptr source_pg, short reason_verb,
			style_info_ptr style)
{
	style_info			new_style, new_stylesheet;
	style_info_ptr		home_style;
	font_info			new_font;
	short				stylesheet;
	
	new_style = *style;

	if ((stylesheet = new_style.styles[super_impose_var]) != 0) {
		
		if (home_style = pgLocateStyleSheet(source_pg, stylesheet, NULL)) {
			
			new_stylesheet = *home_style;
			GetMemoryRecord(source_pg->fonts, (long)home_style->font_index, &new_font);

			UnuseMemory(source_pg->t_formats);

			new_stylesheet.style_sheet_id = 0;
			new_stylesheet.font_index = pgAddNewFont(pg, &new_font);

			if ((stylesheet = pgFindStyleSheet(pg->myself, &new_stylesheet, NULL)) == 0)
				stylesheet = pgNewStyle(pg->myself, &new_stylesheet, &new_font);
			
			new_style.styles[super_impose_var] = stylesheet;
		}
	}
	
	return		pgAddStyleInfo(pg, source_pg, reason_verb, &new_style);
}


/* This function applies a set of paragraph formats to the text that just got
inserted. It is called by pgPaste or pgCopy to insert the target formats from
one paige_rec to another.  The pasted range is target_range and the source (source
pg_ref that was copied) is in src_range. */

static void apply_paragraph_styles (paige_rec_ptr src_pg, paige_rec_ptr target_pg,
		select_pair_ptr src_range, select_pair_ptr target_range, short doing_undo)
{
	style_walk		walker;
	par_info		new_par, mask;
	select_pair		target_apply;
	size_t			target_begin, target_end, first_par_end, unwanted_offset;
	size_t			applied_size, source_end_par_begin;

	pgFillBlock(&mask, sizeof(par_info), -1);

	pgFindPar(target_pg->myself, target_range->begin, NULL, &first_par_end);
	pgPrepareStyleWalk(src_pg, src_range->begin, &walker, TRUE);

	target_begin = target_range->begin;
	target_end = target_range->end;

	pgFindPar(src_pg->myself, src_range->end, &source_end_par_begin, NULL);

	while (walker.current_offset < src_range->end) {
		
		if ((applied_size = src_range->end) > walker.next_par_run->offset)
			applied_size = walker.next_par_run->offset;
		
		applied_size -= walker.current_offset;
		
		pgFindPar(target_pg->myself, target_begin, &target_apply.begin, &first_par_end);
		pgFindPar(target_pg->myself, target_begin + applied_size - 1, &unwanted_offset,
						&target_apply.end);
		
		if ((walker.current_offset > src_range->begin) || !doing_undo)
			if (target_apply.begin < target_begin)
				target_apply.begin = first_par_end;

		if ((src_pg->flags2 & APPLY_ALL_PAR_INFOS) || ((target_apply.end >= target_apply.begin) && (target_apply.end <= target_end))) {
			
			pgBlockMove(walker.cur_par_style, &new_par, sizeof(par_info));
			pgSetParInfoEx(target_pg->myself, &target_apply, &new_par, &mask, FALSE, draw_none);
		}

		target_begin += applied_size;

		if (walker.current_offset == src_pg->t_length)
			break;
			
		pgWalkStyle(&walker, applied_size);
	}
	
// Fix for "Undo". See if there is one more paragraph:
	if (src_pg->flags2 & APPLY_ALL_PAR_INFOS)
		if (source_end_par_begin == src_range->end) {

		pgBlockMove(walker.cur_par_style, &new_par, sizeof(par_info));
		target_apply.begin = target_apply.end = target_begin;

		pgSetParInfoEx(target_pg->myself, &target_apply, &new_par, &mask, FALSE, draw_none);
	}

	pgPrepareStyleWalk(src_pg, 0, NULL, TRUE);
}


/* prepare_style_undo simply copies all the styles in pg, preparing for any
manner of style/format/font changes */

static void prepare_style_undo (paige_rec_ptr pg, pg_undo_ptr undo_ptr)
{
	paige_rec_ptr		copy_stuff;

	undo_ptr->data = pgNewShell(pg->globals);
	copy_stuff = (paige_rec_ptr) UseMemory(undo_ptr->data);
	
	MemoryCopy(pg->t_style_run, copy_stuff->t_style_run);
	MemoryCopy(pg->par_style_run, copy_stuff->par_style_run);
	MemoryCopy(pg->t_formats, copy_stuff->t_formats);
	MemoryCopy(pg->par_formats, copy_stuff->par_formats);
	MemoryCopy(pg->fonts, copy_stuff->fonts);

	make_duplicate_formats(pg, NULL, prepare_undo_style_reason, copy_stuff->t_formats, copy_stuff->par_formats);

	UnuseMemory(undo_ptr->data);
	
	pgGetSelection(pg->myself, &undo_ptr->alt_range.begin, &undo_ptr->alt_range.end);
}


/* This function performs the actual style_undo */

static void do_style_undo (paige_rec_ptr pg, pg_undo_ptr undo_ptr)
{
	paige_rec_ptr		copy_stuff;

	copy_stuff = (paige_rec_ptr) UseMemory(undo_ptr->data);

	pgWillDeleteFormats(pg, undo_ptr->globals, undo_style_reason, pg->t_formats, pg->par_formats);

	MemoryCopy(copy_stuff->t_formats, pg->t_formats);
	MemoryCopy(copy_stuff->par_formats, pg->par_formats);
	MemoryCopy(copy_stuff->t_style_run, pg->t_style_run);
	MemoryCopy(copy_stuff->par_style_run, pg->par_style_run);

	make_duplicate_formats(NULL, pg, prepare_undo_style_reason, pg->t_formats, pg->par_formats);

	UnuseMemory(undo_ptr->data);

	pgInvalSelect(pg->myself, undo_ptr->alt_range.begin, undo_ptr->alt_range.end);
	pgInvalCharLocs(pg);
	
	pgSetNextInsertIndex(pg);
}



/* This function walks through a list of style and paragraph formats and calls
the "duplicate" function */

static void make_duplicate_formats (paige_rec_ptr src_pg, paige_rec_ptr target_pg,
	short reason_verb, format_ref text_formats, par_ref par_formats)
{
	register style_info_ptr		styles;
	register par_info_ptr		pars;
	register pg_short_t			qty;

	for (qty = (pg_short_t)GetMemorySize(text_formats), styles = (style_info_ptr) UseMemory(text_formats);
			qty; ++styles, --qty)
		styles->procs.duplicate(src_pg, target_pg, reason_verb, text_formats, styles);
	UnuseMemory(text_formats);

	for (qty = (pg_short_t)GetMemorySize(par_formats), pars = (par_info_ptr) UseMemory(par_formats);
			qty; ++pars, --qty)
		pars->procs.duplicate(src_pg, target_pg, reason_verb, par_formats, pars);

	UnuseMemory(par_formats);
}



/* This inserts the old deletion. Called by various Undo functions */

static void perform_undo_delete (paige_rec_ptr pg, pg_undo_ptr undo_ptr)
{
	paige_rec_ptr	src_pg;
	select_pair		first_select;
	long			old_flags2;
	
	GetMemoryRecord(undo_ptr->applied_range, 0, &first_select);
	pgSetInsertSelect(pg, first_select.begin);
	src_pg = (paige_rec_ptr) UseMemory(undo_ptr->data);
	old_flags2 = src_pg->flags2 & APPLY_ALL_PAR_INFOS;
	src_pg->flags2 |= APPLY_ALL_PAR_INFOS;

	execute_paste(pg->myself, undo_ptr->data, CURRENT_POSITION, FALSE, draw_none, TRUE);
	src_pg->flags2 &= (~APPLY_ALL_PAR_INFOS);
	src_pg->flags2 |= old_flags2;
	UnuseMemory(undo_ptr->data);

	select_for_undo(pg->myself, undo_ptr, TRUE);
}



/* This function does the "undo" for backspaced characters.  */

static void perform_backspace_undo (paige_rec_ptr pg, pg_undo_ptr undo_ptr)
{
	register pg_kb_ptr		kb_ptr;
	register short			key_qty;
	register short			byte_index;
	memory_ref				temp_ref;
	pg_char_ptr				bytes_to_insert;
	style_info_ptr			style_to_insert;
	par_info_ptr			par_to_insert;
	pg_boolean				not_fwd_delete;
	pg_subref PG_FAR		*sublist;
	size_t					insert_position;
	short					action_verb, char_size, char_qty, increment;
	
	if (undo_ptr->keyboard_delete.end > undo_ptr->keyboard_delete.begin)
		pgDelete(pg->myself, &undo_ptr->keyboard_delete, draw_none);

	pgSetInsertSelect(pg, undo_ptr->alt_range.begin);
	temp_ref = MemoryAlloc(pg->globals->mem_globals, sizeof(pg_char), 0, 4);

	kb_ptr = (pg_kb_ptr) UseMemory(undo_ptr->keyboard_ref);
	key_qty = (short)GetMemorySize(undo_ptr->keyboard_ref);
	
	action_verb = undo_ptr->verb;
	
	if (not_fwd_delete = (pg_boolean)(action_verb != undo_fwd_delete))
		kb_ptr += key_qty;

	undo_ptr->alt_range.end = undo_ptr->alt_range.begin;

	if (undo_ptr->subset_data)
		sublist = (pg_subref *) UseMemory(undo_ptr->subset_data);
	else
		sublist = NULL;

	while (key_qty) {
		
		if (action_verb != undo_fwd_delete)
			--kb_ptr;

		style_to_insert = (style_info_ptr) UseMemoryRecord(undo_ptr->keyboard_styles, kb_ptr->style_item, 0, TRUE);
		pgSetInsertionStyles(pg->myself, style_to_insert, NULL);
		char_size = style_to_insert->char_bytes + 1;
		UnuseMemory(undo_ptr->keyboard_styles);
		
		if (char_size > key_qty)
			char_size = key_qty;

		SetMemorySize(temp_ref, char_size);
		bytes_to_insert = (pg_char_ptr) UseMemory(temp_ref);
		
		if (not_fwd_delete) {
		
			byte_index = char_size - 1;
			increment = -1;
		}
		else {
		
			byte_index = 0;
			increment = 1;
		}

		for (char_qty = char_size; char_qty; --char_qty) {
			
			bytes_to_insert[byte_index] = kb_ptr->text_byte[0];
			
			byte_index += increment;
			
			if (char_qty > 1)
				kb_ptr += increment;
		}
		
		pgGetSelection(pg->myself, &insert_position, NULL);

		if (sublist)
			if (*bytes_to_insert == SUBREF_CHAR) {
			
				insert_undo_subref(pg, *sublist);
				++sublist;
			}

		pgInsert(pg->myself, bytes_to_insert, (long)char_size,
				CURRENT_POSITION, data_insert_mode, 0, draw_none);

		UnuseMemory(temp_ref);

		undo_ptr->alt_range.end += char_size;
		key_qty-= char_size;
		
		par_to_insert = (par_info_ptr) UseMemoryRecord(undo_ptr->keyboard_pars, kb_ptr->par_item, 0, TRUE);

		if (par_changed(pg, par_to_insert)) {
			par_info			mask;

			pgFillBlock(&mask, sizeof(par_info), -1);
			pgSetParInfoEx(pg->myself, NULL, par_to_insert, &mask, FALSE, draw_none);
		}

		UnuseMemory(undo_ptr->keyboard_pars);
		
		if (action_verb == undo_fwd_delete)
			++kb_ptr;
	}
	
	UnuseMemory(undo_ptr->keyboard_ref);
	
	if (sublist)
		UnuseMemory(undo_ptr->subset_data);

	if (undo_ptr->alt_range.begin < undo_ptr->alt_range.end)
		pgCallTextHook(pg, NULL, undo_delete_reason, undo_ptr->alt_range.begin, undo_ptr->alt_range.end
				- undo_ptr->alt_range.begin, call_for_copy, 0, 0, 0);
	
	if (undo_ptr->data) {
		long			first_paste;
		
		first_paste = pgCurrentInsertion(pg);
		execute_paste(pg->myself, undo_ptr->data, CURRENT_POSITION, FALSE, draw_none, TRUE);
		pgSetSelection(pg->myself, first_paste, pgCurrentInsertion(pg), 0, FALSE);
	}
	
	DisposeMemory(temp_ref);
}



/* This function is used to return TRUE if the par_info given is different than
the paragraph format of the current insertion point.  */

static pg_boolean par_changed (paige_rec_ptr pg, par_info_ptr par)
{
	par_info_ptr		cur_par;
	short				result;

	cur_par = pgFindParStyle(pg, pgCurrentInsertion(pg));
	result = !pgEqualStruct(cur_par, par, SIGNIFICANT_PAR_STYLE_SIZE);

	UnuseMemory(pg->par_formats);
	
	return	result;
}


/* This function gets called for setting up a re-do for delete or paste. The
purpose of this separate function is to check the possibility of an original
"undo typing" so the verb/real_verb fields can be set appropriately. However,
if requires_redo is falls, this function does nothing but return NULL.  */

static undo_ref prepare_delete_or_paste_redo (paige_rec_ptr pg, pg_undo_ptr undo_ptr,
		short requires_redo)
{
	undo_ref		result;

	if (!requires_redo)
		return	MEM_NULL;

	result = pgPrepareUndo(pg->myself, (short)-undo_ptr->real_verb, (void PG_FAR *) undo_ptr->data);
	
	if (undo_ptr->verb != undo_ptr->real_verb) {
		pg_undo_ptr		new_undo_ptr;
		
		new_undo_ptr = (pg_undo_ptr) UseMemory(result);
		new_undo_ptr->verb = -undo_ptr->verb;
		UnuseMemory(result);
	}

	return	result;
}



/* This function gets called to invoke the delete_text function for all
backspace-undo chars within undo_stuff. This function won't get called unless
there is a non-NULL value in undo_stuff->keyboard_ref.  */

static void do_pg_undo_delete_proc (pg_undo_ptr undo_stuff)
{
	register pg_char_ptr		backspace_chars;
	register pg_kb_ptr			kb_ptr;
	register pg_short_t			kb_qty, char_index, style_index, last_style;
	style_info_ptr				style_base;
	text_ref					temp_ref;
	
	kb_qty = (pg_short_t)GetMemorySize(undo_stuff->keyboard_ref);
	kb_ptr = (pg_kb_ptr) UseMemory(undo_stuff->keyboard_ref);
	kb_ptr += kb_qty;

	temp_ref = MemoryAlloc(GetGlobalsFromRef(undo_stuff->keyboard_ref), sizeof(pg_char), kb_qty, 0);
	backspace_chars = (pg_char_ptr) UseMemory(temp_ref);
	style_base = (style_info_ptr) UseMemory(undo_stuff->keyboard_styles);
	
	char_index = last_style = 0;

	while (kb_qty) {
		
		--kb_ptr;
		
		style_index = kb_ptr->style_item;
		
		if ((last_style != style_index) && char_index) {
			
			do_delete_from_ptr(&style_base[style_index], backspace_chars, char_index);
			char_index = 0;
		}
		
		last_style = style_index;
		backspace_chars[char_index] = kb_ptr->text_byte[0];

		++char_index;
		--kb_qty;
	}
	
	do_delete_from_ptr(&style_base[style_index], backspace_chars, char_index);

	UnuseMemory(undo_stuff->keyboard_styles);
	UnuseMemory(undo_stuff->keyboard_ref);
	UnuseAndDispose(temp_ref);
}


/* This gets called from within do_pg_undo_delete_proc. The delete_text hook
will get called to do something with the backspace-undo chars about to be
disposed.  */

static void do_delete_from_ptr (style_info_ptr style, pg_char_ptr text,
		pg_short_t length)
{
	if (!length)
		return;
	
	style->procs.delete_text(NULL, disposeundo_reason, style, 0, 0, text, length);
}


/* This function returns a duplicate of a select_pair list (src_range) with
each entry offset by added_offset.  */

static memory_ref copy_applied_range (paige_rec_ptr from_pg, size_t added_offset, pg_boolean force_select)
{
	memory_ref			src_range;
	memory_ref			result = MEM_NULL;
	select_pair_ptr		selections;
	pg_short_t			num_selects;
	
	src_range = from_pg->applied_range;

	if (!src_range) {
		
		if (force_select)
			result = pgSetupOffsetRun(from_pg, NULL, FALSE, FALSE);
		
		if (!result)
			result = MemoryAllocClear(from_pg->globals->mem_globals, sizeof(select_pair), 1, 0);
	}
	else
		result = MemoryDuplicate(src_range);

	num_selects = (pg_short_t)GetMemorySize(result);
	selections = (select_pair_ptr) UseMemory(result);
	
	while (num_selects) {
		
		selections->begin += added_offset;
		selections->end += added_offset;
		
		++selections;
		--num_selects;
	}
	
	UnuseMemory(result);
	
	return	result;
}


/* Given a selection range, this function returns the total amount of text
affected.  */

static long total_text_to_copy(select_pair_ptr selections, long num_selects)
{
	register long				total, qty;
	register select_pair_ptr	selects;

	for (total = 0, qty = num_selects, selects = selections; qty; ++selects, --qty)
		total += (selects->end - selects->begin);
	
	return	total;
}


/* This functions inserts the byte(s) to be backspaced into the undo record
(but only if the undo_ptr has been set up for backspace undo). In any event,
the function returns the number of bytes to backspace (the previous style could
be multiple bytes).  */

static long insert_backspace_undo (paige_rec_ptr pg, pg_undo_ptr undo_ptr)
{
	style_walk				walker;
	text_block_ptr			block;
	register pg_kb_ptr		kb_ptr;
	register pg_char_ptr	text;
	register long			insertion, bytes_inserted;
	par_info				affected_par;
	long					local_offset, begin_offset, end_offset, result;

	insertion = undo_ptr->alt_range.end;
	pgPrepareStyleWalk(pg, insertion, &walker, TRUE);
	bytes_inserted = 0;

	if (undo_ptr->verb == undo_fwd_delete) {

		block = pgFindTextBlock(pg, insertion, NULL, FALSE, TRUE);
		local_offset = insertion - block->begin;
		begin_offset = local_offset;
		end_offset = block->end - block->begin;
		text = (pg_char_ptr) UseMemory(block->text);
		
		for (;;) {
			
			++bytes_inserted;
			++insertion;
			++local_offset;
			++undo_ptr->alt_range.end;
			++undo_ptr->keyboard_delete.end;
			
			pgWalkStyle(&walker, 1);

			if (insertion >= block->end)
				break;

			if (!walker.cur_style->procs.char_info(pg, &walker, text, block->begin,
					begin_offset, end_offset, local_offset, LAST_HALF_BIT | MIDDLE_CHAR_BIT))
				break;
		}
		
		UnuseMemory(block->text);
		UnuseMemory(pg->t_blocks);
	}
	
	bytes_inserted = 1;

	if (insertion == 0 || bytes_inserted == 0) {
		
		pgPrepareStyleWalk(pg, 0, NULL, TRUE);
		return	0;
	}
	
	pgBlockMove(walker.cur_par_style, &affected_par, sizeof(par_info));

	--insertion;
	pgWalkStyle(&walker, -1);

	block = pgFindTextBlock(pg, insertion, NULL, FALSE, TRUE);

	local_offset = insertion - block->begin;
	end_offset = block->end - block->begin;
	text = (pg_char_ptr) UseMemory(block->text);

	while (walker.cur_style->procs.char_info(pg, &walker, text, block->begin,
			0, end_offset, local_offset, LAST_HALF_BIT | MIDDLE_CHAR_BIT)) {
		
		++bytes_inserted;
		--insertion;
		if (!(--local_offset))
			break;
		
		pgWalkStyle(&walker, -1);
	}

	if (text[local_offset] == SUBREF_CHAR) {
		pg_subref			subref, PG_FAR *sublist;

		if ((subref = pgGetSourceSubRef(pg, insertion)) != MEM_NULL) {
		
			if (!undo_ptr->subset_data)
				undo_ptr->subset_data = MemoryAlloc(pg->globals->mem_globals, sizeof(pg_subref), 0, 2);
			
			sublist = (pg_subref *) InsertMemory(undo_ptr->subset_data, 0, 1);
			*sublist = pgDuplicateRef(pg, NULL, subref, pg->active_subset);
			UnuseMemory(undo_ptr->subset_data);
		}
	}

	result = bytes_inserted;
	
	if (undo_ptr->keyboard_ref) {
		text_ref			copy_of_text;
		short				char_bytes, copy_ctr;

		copy_of_text = MemoryAlloc(pg->globals->mem_globals, sizeof(pg_char), bytes_inserted, 0);
		pgBlockMove(&text[local_offset], UseMemory(copy_of_text), bytes_inserted * sizeof(pg_char));
		text = (pg_char_ptr) UseMemoryRecord(copy_of_text, 0, USE_ALL_RECS, FALSE);
		
		walker.cur_style->procs.copy_text(pg, NULL, prepare_undo_typing_reason,
				walker.cur_style, 0, 0, text, bytes_inserted);
		
		text += bytes_inserted;

		while (bytes_inserted > 0) {
			
			char_bytes = walker.cur_style->char_bytes + 1;
			
			text -= char_bytes;

			kb_ptr = (pg_kb_ptr) AppendMemory(undo_ptr->keyboard_ref, char_bytes, FALSE);
			
			for (copy_ctr = 0; copy_ctr < char_bytes; ++copy_ctr) {
			
				kb_ptr[copy_ctr].text_byte[0] = text[copy_ctr];
				kb_ptr[copy_ctr].style_item = add_keyboard_style(pg, undo_ptr, walker.cur_style);
				kb_ptr[copy_ctr].par_item = add_keyboard_par(pg, undo_ptr, &affected_par);
			}
			
			UnuseMemory(undo_ptr->keyboard_ref);
			bytes_inserted -= char_bytes;
		}
		
		UnuseAndDispose(copy_of_text);
	}

	UnuseMemory(block->text);
	UnuseMemory(pg->t_blocks);

	pgPrepareStyleWalk(pg, 0, NULL, TRUE);
	
	return	result;
}



/* This gets called by insert_backspace_undo to insert the style into kb_ptr
if it does not already exist, and returns the <local> style index.  */

static pg_short_t add_keyboard_style (paige_rec_ptr pg, pg_undo_ptr undo_ptr, style_info_ptr style)
{
	register style_info_ptr		src_styles;
	register pg_short_t			src_qty, rec_ctr;
	
	src_styles = (style_info_ptr) UseMemory(undo_ptr->keyboard_styles);
	src_qty = (pg_short_t)GetMemorySize(undo_ptr->keyboard_styles);
	rec_ctr = 0;
	
	while (src_qty) {
		
		if (pgEqualStruct(src_styles, style, SIGNIFICANT_STYLE_SIZE)) {
			
			UnuseMemory(undo_ptr->keyboard_styles);
			
			return	rec_ctr;
		}
		
		++rec_ctr;
		++src_styles;
		--src_qty;
	}
	
	src_styles = (style_info_ptr) AppendMemory(undo_ptr->keyboard_styles, 1, FALSE);
	pgBlockMove(style, src_styles, sizeof(style_info));
	src_styles->procs.duplicate(pg, NULL, prepare_undo_typing_reason, undo_ptr->keyboard_styles, src_styles);

	UnuseMemory(undo_ptr->keyboard_styles);
	
	return	rec_ctr;
}


/* This gets called by insert_backspace_undo to insert the par style into kb_ptr
if it does not already exist, and returns the <local> par style index.  */

static pg_short_t add_keyboard_par (paige_rec_ptr pg, pg_undo_ptr undo_ptr, par_info_ptr par_style)
{
	register par_info_ptr		src_styles;
	register pg_short_t			src_qty, rec_ctr;
	
	src_styles = (par_info_ptr) UseMemory(undo_ptr->keyboard_pars);
	src_qty = (pg_short_t)GetMemorySize(undo_ptr->keyboard_pars);
	rec_ctr = 0;
	
	while (src_qty) {
		
		if (pgEqualStruct(src_styles, par_style, SIGNIFICANT_PAR_STYLE_SIZE)) {
			
			UnuseMemory(undo_ptr->keyboard_pars);
			
			return	rec_ctr;
		}
		
		++rec_ctr;
		++src_styles;
		--src_qty;
	}
	
	src_styles = (par_info_ptr) AppendMemory(undo_ptr->keyboard_pars, 1, FALSE);
	pgBlockMove(par_style, src_styles, sizeof(par_info));
	src_styles->procs.duplicate(pg, NULL, prepare_undo_typing_reason, undo_ptr->keyboard_pars, src_styles);

	UnuseMemory(undo_ptr->keyboard_pars);
	
	return	rec_ctr;
}



/* This prepares for an undo of doc_info. */

static void prepare_doc_info_undo (paige_rec_ptr pg, pg_undo_ptr undo_ptr)
{
	undo_ptr->doc_data = MemoryAlloc(pg->globals->mem_globals, sizeof(pg_doc_info), 1, 0);
	pgBlockMove(&pg->doc_info, UseMemory(undo_ptr->doc_data), sizeof(pg_doc_info));
	UnuseMemory(undo_ptr->doc_data);
	
	prepare_shape_undo(pg->wrap_area, pg->containers, undo_ptr);

	undo_ptr->verb = undo_ptr->real_verb;
}

/* This prepares for undoing a shape. If refcon_ref is non-NULL it gets saved
as well in "applied_range."  */

static void prepare_shape_undo (shape_ref the_shape, memory_ref refcon_ref,
		pg_undo_ptr undo_ptr)
{
	undo_ptr->shape_data = (shape_ref) MemoryDuplicate(the_shape);
	
	if (refcon_ref)
		undo_ptr->refcon_data = MemoryDuplicate(refcon_ref);

	undo_ptr->verb = undo_ptr->real_verb;
}


/* This performs an Undo for shape changes. If inval_text is TRUE the doc is
flagged to force calc.  */

static void do_shape_undo (paige_rec_ptr pg_rec, shape_ref the_shape,
		memory_ref PG_FAR *refcon_ref, pg_undo_ptr undo_ptr, pg_boolean inval_text)
{
	if (refcon_ref) {
		
		if (!undo_ptr->refcon_data)
			*refcon_ref = MEM_NULL;
		else
		if (*refcon_ref)
			MemoryCopy(undo_ptr->refcon_data, *refcon_ref);
		else
			*refcon_ref = MemoryDuplicate(undo_ptr->refcon_data);
	}

	MemoryCopy(undo_ptr->shape_data, the_shape);

	if (inval_text)
		pgInvalSelect(pg_rec->myself, 0, pg_rec->t_length);
}



/* This function disposes all the fields in a pg_undo_ptr. */

static void dispose_undo_structs (pg_undo_ptr undo_ptr)
{
	if (undo_ptr->data)
		pgDispose(undo_ptr->data);

	DisposeNonNilMemory(undo_ptr->applied_range);
	DisposeNonNilMemory(undo_ptr->shape_data);
	DisposeNonNilMemory(undo_ptr->doc_data);
	DisposeNonNilMemory(undo_ptr->refcon_data);
	
	if (undo_ptr->subset_data) {
		long			num_subs;
		pg_subref		PG_FAR *sublist;
		
		num_subs = GetMemorySize(undo_ptr->subset_data);
		sublist = (pg_subref *) UseMemory(undo_ptr->subset_data);
		
		while (num_subs) {
			
			pgDisposeSubRef(*sublist++);
			--num_subs;
		}
		
		UnuseAndDispose(undo_ptr->subset_data);
	}

	if (undo_ptr->keyboard_ref) {

		if (GetMemorySize(undo_ptr->keyboard_ref)) {
		
			do_pg_undo_delete_proc(undo_ptr);
			pgWillDeleteFormats(NULL, undo_ptr->globals, disposeundo_reason, undo_ptr->keyboard_styles, undo_ptr->keyboard_pars);
		}

		DisposeMemory(undo_ptr->keyboard_ref);
		DisposeMemory(undo_ptr->keyboard_styles);
		DisposeMemory(undo_ptr->keyboard_pars);
	}
}


/* This is special-case keyboard undo. Depending on the verb it handles either
regular single pg_char insertions or backspace. */


static undo_ref prepare_keyboard_undo (paige_rec_ptr pg, short verb,
		undo_ref previous_undo)
{
	register pg_undo_ptr	undo_ptr;
	long					insertion, starting_insertion;
	undo_ref				result_undo;
	
	insertion = starting_insertion = pgCurrentInsertion(pg);
	
	if (verb == undo_backspace && insertion == 0 && pg->num_selects == 0)
		return	previous_undo;

	if (verb == undo_fwd_delete && insertion >= pg->t_length && pg->num_selects== 0)
	    return  previous_undo;

	if (pg->key_buffer_mode) {
		long		buffer_qty;
		
		buffer_qty = GetMemorySize(pg->key_buffer);
		insertion += buffer_qty;
	}

	if (result_undo = previous_undo) {
		
		undo_ptr = (pg_undo_ptr) UseMemory(result_undo);
		if ((undo_ptr->real_verb != verb) || (undo_ptr->alt_range.end != insertion)
				|| (pg->num_selects)) {
			
			dispose_undo_structs(undo_ptr);
			pgFillBlock(undo_ptr, sizeof(pg_undo), 0);
			undo_ptr->globals = pg->globals;
		}
	}
	else {
		
		result_undo = MemoryAllocClear(pg->globals->mem_globals, sizeof(pg_undo), 1, 0);
		undo_ptr = (pg_undo_ptr) UseMemory(result_undo);
		undo_ptr->globals = pg->globals;
	}
	
	if (pg->num_selects) {
		
		if (undo_ptr->applied_range)
			DisposeMemory(undo_ptr->applied_range);

		undo_ptr->applied_range = pgSetupOffsetRun(pg, NULL, FALSE, FALSE);
		undo_ptr->data = pgCopy(pg->myself, NULL);

		if ((verb == undo_backspace) || (verb == undo_fwd_delete)) {
			
			undo_ptr->real_verb = undo_backspace;
			undo_ptr->verb = verb;
			undo_ptr->alt_range.begin = undo_ptr->alt_range.end = insertion;
			
			if (undo_ptr->applied_range)
				GetMemoryRecord(undo_ptr->applied_range, 0, &undo_ptr->keyboard_delete);

			undo_ptr->keyboard_delete.end = undo_ptr->keyboard_delete.begin;

			UnuseMemory(result_undo);
			
			return	result_undo;
		}
	}

	if (verb == undo_typing) {
		
		if (!undo_ptr->verb) {
			
			undo_ptr->verb = undo_ptr->real_verb = verb;
			undo_ptr->alt_range.begin = starting_insertion;
			undo_ptr->alt_range.end = insertion;
		}
		
		undo_ptr->alt_range.end += 1;
	}
	else { /* backspace or forward delete undo */
		style_info_ptr	bs_style;
		long			backspace_amt, insert_result;
		short			text_hidden;

		if ((pg->flags & NO_HIDDEN_TEXT_BIT) && insertion) {
			
			insert_result = insertion;
			
			if (verb == undo_backspace)
				insert_result -= 1;

			bs_style = pgFindTextStyle(pg, insert_result);
			
			text_hidden = bs_style->styles[hidden_text_var];
			UnuseMemory(pg->t_formats);
		}
		else
			text_hidden = FALSE;

		if (!undo_ptr->verb || !undo_ptr->keyboard_ref) {
			
			undo_ptr->verb = undo_ptr->real_verb = verb;

			undo_ptr->keyboard_ref = MemoryAlloc(pg->globals->mem_globals,
					sizeof(pg_kb_data), 0, 16);
			undo_ptr->keyboard_styles = MemoryAlloc(pg->globals->mem_globals,
					sizeof(style_info), 0, 2);
			undo_ptr->keyboard_pars = MemoryAlloc(pg->globals->mem_globals,
					sizeof(par_info), 0, 1);
			
			undo_ptr->alt_range.begin = undo_ptr->alt_range.end = insertion;
		}
		
		if (!text_hidden) {
		
			backspace_amt = insert_backspace_undo(pg, undo_ptr);
			undo_ptr->alt_range.end -= backspace_amt;
	
			if (undo_ptr->alt_range.end < undo_ptr->alt_range.begin)
				undo_ptr->alt_range.begin = undo_ptr->alt_range.end;
			
			if (undo_ptr->keyboard_delete.end > undo_ptr->keyboard_delete.begin)
				undo_ptr->keyboard_delete.end -= backspace_amt;
		}
	}

	UnuseMemory(result_undo);
	
	return	result_undo;
}





/* select_for_undo gets called by several undo operations to set a selection
range. If use_only_list is TRUE then only undo_ptr->applied_range is
considered. */

static void select_for_undo (pg_ref pg, pg_undo_ptr undo_ptr,
	pg_boolean use_only_list)
{
	if (undo_ptr->applied_range)
		pgSetSelectionList(pg, undo_ptr->applied_range, 0, FALSE);
	else
	if (!use_only_list)
		pgSetSelection(pg, undo_ptr->alt_range.begin, undo_ptr->alt_range.end,
			0, FALSE);
}


static void restore_subref_state (paige_rec_ptr pg, pg_undo_ptr undo_ptr)
{
	memory_ref			old_state;
	
	if (undo_ptr->rsrv != pg->active_subset) {

		old_state = pgSetNestedFocus(pg, undo_ptr->rsrv, TRUE);
		
		if (old_state)
			DisposeMemory(old_state);
	}
}

/* insert_undo_subref inserts the new subref into the target (current text position). */

static void insert_undo_subref (paige_rec_ptr pg, pg_subref subref)
{
	text_block_ptr		block;
	pg_subref PG_FAR	*target_list;
	size_t				current, subref_index;
	
	pgGetSelection(pg->myself, &current, NULL);

	block = pgFindTextBlock(pg, current, NULL, FALSE, FALSE);
	subref_index = pgFindSubrefPosition(block, (pg_short_t)(current - block->begin));

	target_list = (pg_subref *) InsertMemory(block->subref_list, subref_index, 1);
	*target_list = pgDuplicateRef(pg, NULL, subref, pg->active_subset);

	UnuseMemory(block->subref_list);
	UnuseMemory(pg->t_blocks);
}
