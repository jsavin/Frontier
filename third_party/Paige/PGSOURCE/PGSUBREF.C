/* This file handles various features dealing with "mini" pg_refs, which are a subset of
full documents. A mini subset pg_ref is basically a clone of a mother pg_ref but with its own
text, style run and page rectangle. Usually the page rectangle is dynamic, i.e. defines the
drawing boundaries which are usually the boundaries of an embed_ref character, or table, etc.
The subset pg_ref is identical in every other respect. */

#include "Paige.h"

#include "defprocs.h"
#include "pgTraps.h"
#include "pgEmbed.h"
#include "pgSubref.h"
#include "pgExceps.h"
#include "pgUtils.h"
#include "pgText.h"
#include "pgEdit.h"
#include "pgSelect.h"
#include "pgTxtWid.h"
#include "pgDefStl.h"
#include "pgStyles.h"
#include "machine.h"
#include "pgTables.h"
#include "pgHText.h"

struct subref_matrix_info {
	embed_ref			ref;
	long				width;
};
typedef struct subref_matrix_info subref_matrix_info;
typedef subref_matrix_info PG_FAR *subref_matrix_ptr;

static pg_boolean is_valid_subref (text_block_ptr block, long index);
static long position_to_index (text_block_ptr block, long position);
static void pin_bounds_to_rect (paige_rec_ptr pg, pg_subref subref, rectangle_ptr bounds,
		long flags, draw_points_ptr draw_position);
static pg_boolean arrow_keys (pg_globals_ptr globals, pg_byte_ptr the_key);
static pg_subref get_adjacent_subref (text_block_ptr block, pg_subref current_ref, pg_boolean previous);
static void initialize_sub_record (pgm_globals_ptr mem_globals, paige_sub_ptr sub_ptr);
static memory_ref scan_to_nth_subref (paige_rec_ptr pg, memory_ref subref_stack,
		long PG_FAR *next_index, long wanted_index, pg_boolean follow_nest);
static pg_subref invalidate_hierarchy (paige_rec_ptr pg);
static void invalidate_subref (paige_rec_ptr pg, pg_subref ref);
static void decrement_subset_styles (block_ref t_blocks, pg_short_t bad_index,
			pg_short_t max_styles, pg_boolean do_par);
static long locate_subref_position (block_ref blocks, pg_subref subref);
static pg_short_t locate_stylesheet_option (paige_rec_ptr pg, short stylesheet_option);
static long get_align_extra (point_start_ptr the_starts, long total_width,
		short PG_FAR *num_starts, long alignment_flags);
static void pack_style_run (pack_walk_ptr walker, memory_ref run_ref);
static void pack_select_pairs (pack_walk_ptr walker, memory_ref selection_ref);
static void unpack_style_run (pack_walk_ptr walker, memory_ref run_ref);
static void unpack_select_pairs (pack_walk_ptr walker, memory_ref selection_ref);
static void transfer_style_indexes (paige_rec_ptr source_pg, paige_rec_ptr target_pg, memory_ref runref);
static void copy_width_table (paige_rec_ptr pg, paige_sub_ptr sub_ptr, pg_boolean copy_to_subref);
static void invalidate_selections (paige_sub_ptr sub_ptr);
static void clear_hilite (paige_rec_ptr pg);


/* pgCopySubfields copies the field members from pg to sub_ptr if move_to_subref is TRUE,
otherwise members from sub_ptr to pg are copied. */

PG_PASCAL (void) pgCopySubfields (paige_rec_ptr pg, paige_sub_ptr subrec_ptr, pg_boolean move_to_subref)
{
	register paige_sub_ptr			sub_ptr;
	
	sub_ptr = subrec_ptr;
	sub_ptr += sub_ptr->alt_index;

	if (move_to_subref) {
		
		sub_ptr->t_length = pg->t_length;
		sub_ptr->t_blocks = pg->t_blocks;
		sub_ptr->t_style_run = pg->t_style_run;
		sub_ptr->par_style_run = pg->par_style_run;
		sub_ptr->hyperlinks = pg->hyperlinks;
		sub_ptr->target_hyperlinks = pg->target_hyperlinks;
		sub_ptr->mem_id = pg->mem_id;
		sub_ptr->num_selects = pg->num_selects;
		sub_ptr->insert_style = pg->insert_style;
		sub_ptr->select = pg->select;
		sub_ptr->flags &= (~(NO_WRAP_BIT | NO_LF_BIT | NO_EDIT_BIT | CARET_BIT));
		sub_ptr->flags |= (pg->flags & (NO_WRAP_BIT | NO_LF_BIT | NO_EDIT_BIT | CARET_BIT));
	}
	else {

		pg->t_length = sub_ptr->t_length;
		pg->t_blocks = sub_ptr->t_blocks;
		pg->t_style_run = sub_ptr->t_style_run;
		pg->par_style_run = sub_ptr->par_style_run;
		pg->hyperlinks = sub_ptr->hyperlinks;
		pg->target_hyperlinks = sub_ptr->target_hyperlinks;
		pg->mem_id = sub_ptr->mem_id;
		pg->num_selects = sub_ptr->num_selects;
		pg->insert_style = sub_ptr->insert_style;
		pg->select = sub_ptr->select;
		pg->flags &= (~(NO_WRAP_BIT | NO_LF_BIT | NO_EDIT_BIT | CARET_BIT));
		pg->flags |= (sub_ptr->flags & (NO_WRAP_BIT | NO_LF_BIT | NO_EDIT_BIT | CARET_BIT));
	}
}


/* pgNewSubRef returns a new PAIGE subreference with no text. */

PG_PASCAL (pg_subref) pgNewSubRef (pgm_globals_ptr mem_globals)
{
	pg_subref			subref;
	paige_sub_ptr		sub_ptr;

	PG_TRY(mem_globals) {
		
		++mem_globals->next_mem_id;
		mem_globals->current_id = mem_globals->next_mem_id;
	
		subref = MemoryAllocClear(mem_globals, sizeof(paige_sub_rec), 1, 0);
		sub_ptr = (paige_sub_ptr) UseMemory(subref);
		sub_ptr->myself = subref;
		sub_ptr->mem_id = mem_globals->current_id;
		sub_ptr->flags = NO_WRAP_BIT;

		initialize_sub_record(mem_globals, sub_ptr);
		
		++mem_globals->next_mem_id;

		UnuseMemory(subref);
		SetMemoryPurge(subref, NO_PURGING_STATUS, FALSE);
	}
	
	PG_CATCH {
		
		DisposeAllFailedMemory(mem_globals, mem_globals->next_mem_id);

		subref = MEM_NULL;
	}
	
	PG_ENDTRY;
	
	mem_globals->current_id = mem_globals->next_mem_id;

	return subref;
}


/* pgInsertSubref inserts a new subref into pg. If set_focus is TRUE this becomes the
new focus, otherwise the focus is left alone. Text is re-drawn if draw_mode is non-zero. */

PG_PASCAL (void) pgInsertSubRef (pg_ref pg, pg_subref subref, long position,
		pg_boolean set_focus, short stylesheet_option, pg_byte_ptr initial_text,
		long initial_text_size, short draw_mode)
{
	paige_rec_ptr	pg_rec;
	pg_byte			subref_byte;
	pg_subref_ptr	subref_list;
	paige_sub_ptr	sub_ptr;
	style_run_ptr	run, par_run;
	text_block_ptr	block;
	style_info_ptr	def_style;
	size_t			real_position, sublist_position;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	real_position = pgFixOffset(pg_rec, position);
	sub_ptr = (paige_sub_ptr) UseMemory(subref);
	sub_ptr->home_position = real_position;
	sub_ptr->home_sub = pg_rec->active_subset;
	run = (style_run_ptr) UseMemory(sub_ptr->t_style_run);
	
	if ((run->style_item = locate_stylesheet_option(pg_rec, stylesheet_option)) == 0)
		run->style_item = pg_rec->insert_style;
	else {
	
		run->style_item -= 1;

#ifdef DOING_EQ
// Adjust the stylesheet option so we (a) Use same font and (b) use relative point size
// and (c) use nested subref style.
	{
		style_info			added_style;
		
		GetMemoryRecord(pg_rec->t_formats, (long)pg_rec->insert_style, &added_style);
		def_style = UseMemoryRecord(pg_rec->t_formats, (long) run->style_item, 0, TRUE);
		
		added_style.styles[relative_point_var] = def_style->styles[relative_point_var];

		added_style.styles[nested_subset_var] = def_style->styles[nested_subset_var];
		added_style.styles[italic_var] |= def_style->styles[italic_var];

		UnuseMemory(pg_rec->t_formats);
		
		run->style_item = pgAddStyleInfo(pg_rec, NULL, internal_clone_reason, &added_style);
	}
	
#endif
	}

	def_style = (style_info_ptr) UseMemoryRecord(pg_rec->t_formats, (long)run->style_item, 0, TRUE);
	def_style->used_ctr += 1;
	sub_ptr->insert_style = run->style_item;
	UnuseMemory(pg_rec->t_formats);

	if (initial_text && initial_text_size) {
		pg_hyperlink_ptr		first_link;

		block = (text_block_ptr) UseMemory(sub_ptr->t_blocks);
		block->end = initial_text_size;
		SetMemorySize(block->text, initial_text_size);
		pgBlockMove(initial_text, UseMemory(block->text), initial_text_size);
		UnuseMemory(block->text);
		UnuseMemory(sub_ptr->t_blocks);

		run[1].offset = initial_text_size + ZERO_TEXT_PAD;
		par_run = (style_run_ptr) UseMemory(sub_ptr->par_style_run);
		par_run[1].offset = initial_text_size + ZERO_TEXT_PAD;
		UnuseMemory(sub_ptr->par_style_run);
		first_link = (pg_hyperlink_ptr) UseMemory(sub_ptr->hyperlinks);
		first_link->applied_range.begin = first_link->applied_range.end = initial_text_size + ZERO_TEXT_PAD;
		UnuseMemory(sub_ptr->hyperlinks);
		first_link = (pg_hyperlink_ptr) UseMemory(sub_ptr->target_hyperlinks);
		first_link->applied_range.begin = first_link->applied_range.end = initial_text_size + ZERO_TEXT_PAD;
		UnuseMemory(sub_ptr->target_hyperlinks);

		sub_ptr->t_length = initial_text_size;
	}

	UnuseMemory(sub_ptr->t_style_run);
	UnuseMemory(subref);
	
	block = pgFindTextBlock(pg_rec, real_position, NULL, FALSE, TRUE);
	sublist_position = pgFindSubrefPosition(block, (pg_short_t)(real_position - block->begin));
	
	subref_list = (pg_subref_ptr) InsertMemory(block->subref_list, sublist_position, 1);
	*subref_list = subref;
	
	UnuseMemory(block->subref_list);
	UnuseMemory(pg_rec->t_blocks);
	
	subref_byte = SUBREF_CHAR;
	pgInsert(pg, &subref_byte, sizeof(pg_byte), real_position, data_insert_mode, 0, draw_none);

	if (set_focus) {
	
		pgUseSubRef(pg_rec, subref, 0, NULL, NULL);
		pg_rec->subset_focus = subref;
	}

	if (draw_mode)
		pgUpdateText(pg_rec, NULL, 0, pg_rec->t_length, MEM_NULL, NULL, draw_mode, TRUE);

	UnuseMemory(pg);
}


/* pgDrawSubRef locates the subref associated with local_offset of the current drawing
and draws its contents. */

PG_PASCAL (void) pgDrawSubRef (paige_rec_ptr pg, style_walk_ptr walker,
		draw_points_ptr draw_position, pg_short_t local_offset, co_ordinate_ptr vis_offset)
{

	rectangle				bounds, real_bounds;
	paige_sub_ptr			sub_ptr;
	pg_subref				subref;
	long					subref_index;

	subref_index = pgFindSubrefPosition(draw_position->block, local_offset);
	GetMemoryRecord(draw_position->block->subref_list, subref_index, (void PG_FAR *)&subref);
	sub_ptr = (paige_sub_ptr) UseMemory(subref);
	sub_ptr += sub_ptr->alt_index;
	sub_ptr->home_position = (long)draw_position->starts->offset;
	sub_ptr->home_position += draw_position->block->begin;

	bounds = real_bounds = draw_position->starts->bounds;
	bounds.top_left.h += sub_ptr->left_extra;
	bounds.bot_right.h -= sub_ptr->right_extra;
	pgUseSubRef(pg, subref, sub_ptr->subref_flags, &bounds, draw_position);

	sub_ptr->subref_flags |= SUBREF_DRAWN;
	pgDisplaySubset(pg, &bounds, vis_offset);
	sub_ptr->subset_bounds = real_bounds;
	bounds.top_left.h += sub_ptr->generated_extra;

	UnuseMemory(subref);
	
	pgOffsetRect(&bounds, vis_offset->h, vis_offset->v);
	pg->procs.subset_glitter(pg, draw_position, &bounds, vis_offset, walker);
	
	draw_position->to.h = draw_position->from.h + (real_bounds.bot_right.h - real_bounds.top_left.h);

// Re-install the font, was probably re-set during draw.

	walker->cur_style->procs.install(pg, walker->cur_style, walker->cur_font,
				&walker->superimpose, walker->style_overlay, TRUE);

	pgUnuseSubRef(pg);
}


/* pgDisposeSubRef disposes a subreference. */

PG_PASCAL (void) pgDisposeSubRef (pg_subref subref)
{
	pgm_globals_ptr		mem_globals;
	paige_sub_ptr		sub_ptr;
	long				memory_id;

	sub_ptr = (paige_sub_ptr) UseMemory(subref);
	pgDisposeAllSubRefs(sub_ptr->t_blocks);
	memory_id = sub_ptr->mem_id;
	UnuseMemory(subref);
	
	mem_globals = GetGlobalsFromRef(subref);
	DisposeAllMemory(mem_globals, memory_id);
}


/* pgDuplicateRef duplicates the whole pg_subref, returning the new one. */

PG_PASCAL (pg_subref) pgDuplicateRef (paige_rec_ptr source_pg, paige_rec_ptr target_pg,
			pg_subref subref, pg_subref home_ref)
{
	pg_subref			result;
	pgm_globals_ptr		mem_globals;
	paige_sub_ptr		sub_ptr;
	text_block_ptr		block;
	size_t				num_alternates;
	long				mem_id;

	mem_globals = GetGlobalsFromRef(subref);

	PG_TRY(mem_globals) {

		++mem_globals->next_mem_id;
		mem_id = mem_globals->next_mem_id;
		++mem_globals->next_mem_id;
		mem_globals->current_id = mem_globals->next_mem_id;

		result = MemoryDuplicateID(subref, mem_id);

		sub_ptr = (paige_sub_ptr) UseMemory(result);
		num_alternates = GetMemorySize(result);
		
		while (num_alternates) {

			sub_ptr->mem_id = mem_id;
			sub_ptr->home_sub = home_ref;
			sub_ptr->myself = result;
			sub_ptr->t_blocks = MemoryDuplicateID(sub_ptr->t_blocks, mem_id);
			sub_ptr->t_style_run = MemoryDuplicateID(sub_ptr->t_style_run, mem_id);
			sub_ptr->par_style_run = MemoryDuplicateID(sub_ptr->par_style_run, mem_id);
			sub_ptr->select = MemoryDuplicateID(sub_ptr->select, mem_id);
			sub_ptr->hyperlinks = MemoryDuplicateID(sub_ptr->hyperlinks, mem_id);
			sub_ptr->target_hyperlinks = MemoryDuplicateID(sub_ptr->hyperlinks, mem_id);

			block = (text_block_ptr) UseMemory(sub_ptr->t_blocks);
			block->text = MemoryDuplicateID(block->text, mem_id);
			block->lines = MemoryDuplicateID(block->lines, mem_id);
			block->subref_list = MemoryDuplicateID(block->subref_list, mem_id);
			
			UnuseMemory(sub_ptr->t_blocks);
			
			if (source_pg && target_pg)
				transfer_style_indexes(source_pg, target_pg, sub_ptr->t_style_run);

			pgDuplicateAllSubRefs(source_pg, target_pg, sub_ptr->t_blocks, result);

			++sub_ptr;
			--num_alternates;
		}

		UnuseMemory(result);
	}
	
	PG_CATCH {
		
		DisposeAllFailedMemory(mem_globals, mem_globals->next_mem_id);

		return	MEM_NULL;
	}
	
	PG_ENDTRY;
	
	return	result;
}


/* pgUseSubRef first "pushes" the critical contents of pg into the subref stack, and replaces
those critical fields with the members in the sub reference. Effectively, pg becomes a new document.
To restore the original pg_ref, use pgUnuseSubRef(). This function also establishes the absolute
bounding rectangle based on pin_to if it is non-NULL. */

PG_PASCAL (void) pgUseSubRef (paige_rec_ptr pg, pg_subref subref, long flags, rectangle_ptr pin_to,
		draw_points_ptr draw_position)
{
	paige_sub_ptr		sub_ptr, stack_ptr;
	
	if (pin_to)
		pin_bounds_to_rect(pg, subref, pin_to, flags, draw_position);

	stack_ptr = (paige_sub_ptr) AppendMemory(pg->subref_stack, 1, TRUE);
	sub_ptr = (paige_sub_ptr) UseMemory(subref);
	stack_ptr->alt_index = 0;
	pgCopySubfields(pg, stack_ptr, TRUE);
	copy_width_table(pg, stack_ptr, TRUE);
	stack_ptr->active_subset = pg->active_subset;
	pgCopySubfields(pg, sub_ptr, FALSE);
	copy_width_table(pg, sub_ptr, FALSE);

	pg->active_subset = subref;
	
	if (!(sub_ptr->subref_flags & SUBREF_NOT_EDITABLE))
		++pg->subref_level;

	UnuseMemory(pg->subref_stack);
	UnuseMemory(subref);
}



/* pgUnuseSubRef restores the critical fields in pg to their state just prior to pgUseSubRef().
This function must balance the calls to pgUseSubRef(). The function returns the previuous
subref selected. */

PG_PASCAL (pg_subref) pgUnuseSubRef (paige_rec_ptr pg)
{
	paige_sub_ptr		stack_ptr, sub_ptr;
	pg_subref			previous;
	size_t				stack_size;
	
	previous = pg->active_subset;

	if (stack_size = GetMemorySize(pg->subref_stack)) {
		
		sub_ptr = (paige_sub_ptr) UseMemory(pg->active_subset);

		if (!(sub_ptr->subref_flags & SUBREF_NOT_EDITABLE))
			--pg->subref_level;

		pgCopySubfields(pg, sub_ptr, TRUE);
		copy_width_table(pg, sub_ptr, TRUE);
		UnuseMemory(pg->active_subset);

		--stack_size;
		stack_ptr = (paige_sub_ptr) UseMemoryRecord(pg->subref_stack, stack_size, 0, TRUE);
		stack_ptr->alt_index = 0;
		pgCopySubfields(pg, stack_ptr, FALSE);
		copy_width_table(pg, stack_ptr, FALSE);
		pg->active_subset = stack_ptr->active_subset;
		UnuseMemory(pg->subref_stack);
		
		SetMemorySize(pg->subref_stack, stack_size);
	}
	
	return	previous;
}


/* pgGetSubrefState returns a list of all active and nested subrefs. This list can then be passed
to pgRestoreSubrefs to restore that exact state. If clear_refs is TRUE all subrefs are cleared.
Added 8-18-96, if clear_hilites is TRUE then all the selection(s) are cleared. */

PG_PASCAL (memory_ref) pgGetSubrefState (paige_rec_ptr pg, pg_boolean clear_hilites, pg_boolean clear_refs)
{
	pg_subref		PG_FAR		*restore_list;
	memory_ref					restore_ref;
	size_t						stack_size;

	if (clear_hilites)
		pgSetupGrafDevice(pg, &pg->port, MEM_NULL, clip_standard_verb);

	if (pg->active_subset) {
		
		restore_ref = MemoryAlloc(pg->globals->mem_globals, sizeof(pg_subref), 0, 2);

		stack_size = GetMemorySize(pg->subref_stack);
		
		if (stack_size > 1) {
			paige_sub_ptr			sub_ptr;
			
			sub_ptr = (paige_sub_ptr) UseMemory(pg->subref_stack);
			
			while (stack_size > 1) {
				
				++sub_ptr;
				--stack_size;
				restore_list = (pg_subref *) AppendMemory(restore_ref, 1, FALSE);
				*restore_list = sub_ptr->active_subset;
				UnuseMemory(restore_ref);
			}
			
			UnuseMemory(pg->subref_stack);
		}

		restore_list = (pg_subref *) AppendMemory(restore_ref, 1, FALSE);
		*restore_list = pg->active_subset;
		UnuseMemory(restore_ref);
		
		if (clear_refs) {
		
			while (pg->active_subset) {
				
				if (clear_hilites)
					clear_hilite(pg);

				pgUnuseSubRef(pg);
			}
		}
	}
	else
		restore_ref = MEM_NULL;

	if (clear_hilites) {
		
		clear_hilite(pg);
		pg->procs.set_device(pg, unset_pg_device, &pg->port, NULL);
	}

	return	restore_ref;
}


/* pgRestoreSubrefs restores whatever subref SHOULD be selected (the one at 
pg->active_subset). Typically this is used after pgGetSubrefState() call and you want
to put back whatever subref was there before. THE MEMORY_REF IS DISPOSED. */

PG_PASCAL (void) pgRestoreSubRefs (paige_rec_ptr pg, memory_ref old_subrefs)
{
	pg_subref		PG_FAR		*restore_list;
	size_t						list_size;
	
	if (old_subrefs) {
		
		if (pg->active_subset) {
			memory_ref			dummy_list;
			
			if (dummy_list = pgGetSubrefState(pg, FALSE, TRUE))
				DisposeMemory(dummy_list);
		}

		list_size = GetMemorySize(old_subrefs);
		
		for (restore_list = (pg_subref *) UseMemory(old_subrefs); list_size; ++restore_list, --list_size)
			pgUseSubRef(pg, *restore_list, 0, NULL, NULL);
		
		UnuseAndDispose(old_subrefs);
	}
}



/* pgClickSelectSubRef is the function that walks through all embed_refs and checks for the
mouse_point within a subset type (edit within edit). If found, all selection is removed from
the main body of text and the subset is established as the "main" pg_ref. */

PG_PASCAL (pg_boolean) pgClickSelectSubRef (pg_ref pg, co_ordinate_ptr mouse_point)
{
	paige_rec_ptr		pg_rec;
	co_ordinate			clicked_pt;
	paige_sub_ptr		sub_ptr;
	pg_subref			old_subset, clicked_ref;
	t_select_ptr		select;
	pg_boolean			subset_changed;
	size_t				current_offset, uneditable_ctr;

	clicked_pt = *mouse_point;
	pg_rec = (paige_rec_ptr) UseMemory(pg);
	old_subset = pg_rec->active_subset;

	pgSetupGrafDevice(pg_rec, &pg_rec->port, MEM_NULL, clip_standard_verb);

	while (pg_rec->active_subset) {

		pgTurnOffHighlight(pg_rec, FALSE);
		pgSetSelection(pg, 0, 0, 0, FALSE);
		pgUnuseSubRef(pg_rec);
	}

	pgTurnOffHighlight(pg_rec, FALSE);
	select = (t_select_ptr) UseMemory(pg_rec->select);
	current_offset = select->offset;
	UnuseMemory(pg_rec->select);
	pgSetSelection(pg, current_offset, current_offset, 0, FALSE);

	pg_rec->procs.set_device(pg_rec, unset_pg_device, &pg_rec->port, NULL);
	uneditable_ctr = 0;

	while (clicked_ref = pgPtInSubRef(pg_rec, mouse_point)) {

		pgUseSubRef(pg_rec, clicked_ref, 0, NULL, NULL);
		sub_ptr = (paige_sub_ptr) UseMemory(clicked_ref);
		sub_ptr += sub_ptr->alt_index;

		if (sub_ptr->subref_flags & SUBREF_NOT_EDITABLE)
			++uneditable_ctr;
		else
			uneditable_ctr = 0;

		UnuseMemory(clicked_ref);
	}

	while (pg_rec->active_subset && uneditable_ctr) {
	
		pgUnuseSubRef(pg_rec);
		--uneditable_ctr;
	}
	
	pg_rec->subset_focus = pg_rec->active_subset;
	
	subset_changed = (pg_boolean)(old_subset != pg_rec->active_subset);

	if (!(pg_rec->doc_info.attributes & NO_SUBSET_REDRAW_BIT))
		if (subset_changed) {
			long			old_subset_line;

			old_subset_line = pgRedrawSubsetLine(pg_rec, old_subset, -1);
			pgRedrawSubsetLine(pg_rec, pg_rec->active_subset, old_subset_line);
	}

	UnuseMemory(pg);
	
	return		(subset_changed);
}



/* pgSetNestedFocus is the same as pgSetSubRefFocus except the logical chain of nested subrefs
are inserted into the "subref stack" and any previous states are cleared. The function result
is the previous subref state. */
 
PG_PASCAL (memory_ref) pgSetNestedFocus (paige_rec_ptr pg, pg_subref ref, pg_boolean redraw)
{
	memory_ref			old_state, new_state;
	pg_subref			previous_ref, this_ref;
	pg_subref	PG_FAR	*stack_list;
	paige_sub_ptr		sub_ptr;
	size_t				stack_qty;

	old_state = pgGetSubrefState(pg, TRUE, TRUE);

	if (ref) {
	
		sub_ptr = (paige_sub_ptr) UseMemory(ref);
		previous_ref = sub_ptr->home_sub;
		UnuseMemory(ref);
		new_state = MemoryAlloc(pg->globals->mem_globals, sizeof(pg_subref), 0, 3);

		while (previous_ref) {
			
			this_ref = previous_ref;
			stack_list = (pg_subref *) AppendMemory(new_state, 1, FALSE);
			*stack_list = this_ref;
			UnuseMemory(new_state);

			sub_ptr = (paige_sub_ptr) UseMemory(this_ref);
			previous_ref = sub_ptr->home_sub;
			UnuseMemory(this_ref);
		}
		
		if ((stack_qty = GetMemorySize(new_state)) > 0) {
			
			stack_list = (pg_subref *) UseMemory(new_state);
			stack_list += stack_qty;
			
			while (stack_qty) {
				
				pgUseSubRef(pg, *(--stack_list), 0, NULL, NULL);
				--stack_qty;
			}
			
			UnuseMemory(new_state);
		}
		
		DisposeMemory(new_state);
		pgUseSubRef(pg, ref, 0, NULL, NULL);
	}

	pg->subset_focus = pg->active_subset;
	
	if (redraw)
		pgDisplay(pg->myself, NULL, MEM_NULL, MEM_NULL, NULL, bits_copy);

	return		old_state;
}


/* pgInsertIntoSubRef handles character insertion when the paige_rec has revealed there is
an active subset. NOTE, a "fake" insert can occur in which data is NULL. This is done to
force-invalidate the activate subset and the subsequent line. */

PG_PASCAL (pg_boolean) pgInsertIntoSubRef (paige_rec_ptr pg, pg_byte_ptr data, size_t length,
		size_t position, short insert_mode, short modifiers, short draw_mode)
{
	pg_subref			active_subset, focus_subset;
	paige_sub_ptr		sub_ptr;
	text_block_ptr		block;
	memory_ref			restore_ref;
	pg_boolean			result;
	size_t				fake_insert_pos;
	short				use_insert_mode, use_draw_mode;

	use_insert_mode = insert_mode;
	use_draw_mode = draw_mode;
	active_subset = pg->active_subset;
	focus_subset = pg->subset_focus;
	result = FALSE;

	if (pg->num_selects == 0 && draw_mode)
		pgTurnOffHighlight(pg, TRUE);

	if (data) {
		
		pg->subset_focus = pg->active_subset;
		pg->active_subset = MEM_NULL;
		
		if (use_insert_mode == key_buffer_mode)
			use_insert_mode = key_insert_mode;
		
		if (arrow_keys(pg->globals, data))
			use_draw_mode = best_way;
		else {
		
			use_draw_mode = draw_none;
			pg->flags2 |= NO_CARET_BIT;
		}
		
		result = pgInsert(pg->myself, data, length, position, use_insert_mode, modifiers, draw_none);
		pg->active_subset = active_subset;
		pg->flags2 &= (~NO_CARET_BIT);

		if (use_draw_mode)
			return	result;
	}

	pgInvalSelect(pg->myself, 0, pg->t_length);

	active_subset = invalidate_hierarchy(pg);

	sub_ptr = (paige_sub_ptr) UseMemory(active_subset);
	sub_ptr += sub_ptr->alt_index;
	fake_insert_pos = sub_ptr->home_position;  // = top level memory_ref insertion place
	UnuseMemory(active_subset);

	if (draw_mode) {

		restore_ref = pgGetSubrefState(pg, FALSE, TRUE);
		block = pgFindTextBlock(pg, fake_insert_pos, NULL, FALSE, TRUE);
		pgTextReMeasure(pg, block, fake_insert_pos - block->begin);
		pgInvalidateLine(pg, fake_insert_pos, draw_mode);
		UnuseMemory(pg->t_blocks);
		pgRestoreSubRefs(pg, restore_ref);

		pgSetupGrafDevice(pg, &pg->port, MEM_NULL, clip_standard_verb);
		pg->procs.cursor_proc(pg, (t_select_ptr) UseMemory(pg->select), restore_cursor);
		UnuseMemory(pg->select);
		pgDrawHighlight(pg, update_cursor);
		pg->procs.set_device(pg, unset_pg_device, &pg->port, NULL);
	}

	return	result;
}

/* pgClearSubrefCarets clears all caret flags everywhere in the stack. */

PG_PASCAL (void) pgClearSubrefCarets (paige_rec_ptr pg, pg_subref clear_this_also)
{
	paige_sub_ptr		stack_ptr;
	long				num_subs;
	
	num_subs = GetMemorySize(pg->subref_stack);
	stack_ptr = (paige_sub_ptr) UseMemory(pg->subref_stack);
	
	while (num_subs) {
		
		stack_ptr->flags &= (~CARET_BIT);
		invalidate_selections(stack_ptr);
		++stack_ptr;
		--num_subs;
	}
	
	UnuseMemory(pg->subref_stack);
	
	if (clear_this_also) {
		
		stack_ptr = (paige_sub_ptr) UseMemory(clear_this_also);
		stack_ptr->flags &= (~CARET_BIT);
		invalidate_selections(stack_ptr);
		UnuseMemory(clear_this_also);
	}
	
	if (pg->active_subset && pg->active_subset != clear_this_also) {

		stack_ptr = (paige_sub_ptr) UseMemory(pg->active_subset);
		stack_ptr->flags &= (~CARET_BIT);
		invalidate_selections(stack_ptr);
		UnuseMemory(pg->active_subset);
	}
}


/* pgUpdateSubRefBounds gets called after a block has paginated and is ONLY called if the
current doc is a subset (which means we are a composite "character"). In this case the
internal subref data for the character dimension needs to get updated. */


PG_PASCAL (void) pgUpdateSubRefBounds (paige_rec_ptr pg, text_block_ptr block)
{
	paige_sub_ptr			sub_ptr;
	point_start_ptr			starts, initial_starts;
	rectangle				union_bounds;
	long					width, height, align_extra;

	sub_ptr = (paige_sub_ptr) UseMemory(pg->active_subset);
	sub_ptr += sub_ptr->alt_index;
	starts = initial_starts = (point_start_ptr) UseMemory(block->lines);
	pgFillBlock(&union_bounds, sizeof(rectangle), 0);

	while (starts->flags != TERMINATOR_BITS) {
		
		pgTrueUnionRect(&starts->bounds, &union_bounds, &union_bounds);
		++starts;
	}
	
	if (sub_ptr->t_length == 0)
		width = sub_ptr->empty_width;
	else
		width = (union_bounds.bot_right.h - union_bounds.top_left.h);
	
	sub_ptr->actual_text_width = width;

	if (sub_ptr->t_length && (sub_ptr->subref_flags & (HORIZONTAL_CENTER_FLAG | HORIZONTAL_RIGHT_FLAG))) {
		short		starts_in_line;

		starts = initial_starts;

		while (starts->flags != TERMINATOR_BITS) {
			
			align_extra = get_align_extra(starts, width, &starts_in_line, sub_ptr->subref_flags);
			
			while (starts_in_line) {
			
				pgOffsetRect(&starts->bounds, align_extra, 0);
				++starts;
				--starts_in_line;
			}
		}
	}

	width += (sub_ptr->left_extra + sub_ptr->right_extra);

	if (width < sub_ptr->minimum_width)
		width = sub_ptr->minimum_width;

	height = (union_bounds.bot_right.v - union_bounds.top_left.v)
			+ sub_ptr->top_extra + sub_ptr->bot_extra;

	union_bounds.bot_right.h = union_bounds.top_left.h + width;
	union_bounds.bot_right.v = union_bounds.top_left.v + height;
	sub_ptr->subset_bounds = union_bounds;

	sub_ptr->subref_flags &= (~SUBREF_DRAWN);

// Compute the ascent and descent:

	sub_ptr->descent = block->end_start.baseline + sub_ptr->bot_extra + sub_ptr->nested_shift_v;
	sub_ptr->ascent = (short)(height - sub_ptr->descent + sub_ptr->nested_shift_v);

	if (sub_ptr->subref_flags & ANY_VERTICAL_FLAGS) {
		short				vertical_extra;
		
		vertical_extra = 0;
		
		if (sub_ptr->subref_flags & (VERTICAL_SHIFT_UP | VERTICAL_SHIFT_DOWN))
			vertical_extra = (short)(sub_ptr->subref_flags & ELEMENT_MASK);

		sub_ptr->descent = (short)(((height / 2) - (sub_ptr->descent / 2) + sub_ptr->bot_extra)
				+ vertical_extra);

		sub_ptr->ascent = (short)(height - sub_ptr->descent);
	}
	else
	if (sub_ptr->subref_flags & ANY_NTH_ALIGN) {
		long			element_ctr, element;
		short			computed_descent;

		element = (sub_ptr->subref_flags & ELEMENT_MASK) - 1;
		
		starts = initial_starts;
		
		for (element_ctr = 0; element_ctr < element; ++element_ctr) {
			
			for (;;) {
				
				starts += 1;
				
				if (starts->flags & NEW_LINE_BIT)
					break;
			}
		}
		
		computed_descent = starts->baseline;
		
		if (starts->flags != TERMINATOR_BITS) {
		
			for (;;) {
			
				for (;;) {

					starts += 1;
					if (starts->flags & NEW_LINE_BIT)
						break;
				}	
			
				if (starts->flags == TERMINATOR_BITS)
					break;
			
				computed_descent += (short)(starts->bounds.bot_right.v - starts->bounds.top_left.v);
		    }
		}
		
		computed_descent += (short)(sub_ptr->bot_extra + sub_ptr->nested_shift_v);
		sub_ptr->descent = computed_descent;
		sub_ptr->ascent = (short)(height - computed_descent);
	}

	UnuseMemory(block->lines);
	UnuseMemory(pg->active_subset);
}



/* pgApplyStylesToSubrefs changes the style(s) of all embedded subrefs in the specified
range. */

PG_PASCAL (void) pgApplyStylesToSubrefs (paige_rec_ptr pg, select_pair_ptr range,
			style_info_ptr style, style_info_ptr mask)
{
	register pg_byte_ptr		text;
	text_block_ptr				block;
	pg_subref					subref;
	select_pair					whole_range;
	long						offset_ctr;
	long						local_offset, local_end_offset, index;

	offset_ctr = range->begin;
	block = pgFindTextBlock(pg, offset_ctr, NULL, FALSE, TRUE);
	local_offset = offset_ctr - block->begin;
	
	while (offset_ctr < range->end) {
		
		if (range->end > block->end)
			local_end_offset = block->end - block->begin;
		else
			local_end_offset = range->end - block->begin;

		text = (pg_byte_ptr) UseMemory(block->text);
		
		while (local_offset < local_end_offset) {
		
			if (text[local_offset] == SUBREF_CHAR) {
				
				index = position_to_index(block, local_offset);
				
				if (is_valid_subref(block, index)) {
				
					GetMemoryRecord(block->subref_list, index, (void PG_FAR *)&subref);
					pgUseSubRef(pg, subref, 0, NULL, NULL);
					whole_range.begin = 0;
					whole_range.end = pg->t_length;
					pgChangeStyleInfo(pg, &whole_range, style, mask, draw_none);
					pgUnuseSubRef(pg);
				}
			}

			++local_offset;
			++offset_ctr;
		}
		
		UnuseMemory(block->text);

		if (offset_ctr >= block->end)
			if (offset_ctr < range->end) {
			
			++block;
			local_offset = 0;
		}
	}
	
	UnuseMemory(pg->t_blocks);
}



/* pgDecrementSubsetStyles gets called by the style cleanup functions. The bad_index style record
is no longer valid. If do_par is TRUE we are doing paragraph formats, otherwise we are
doing style_info formats. Note that the regular "cleanupstyles" function ONLY fixes up the style
run(s) for the main pg_ref, not any subrefs. Hence, this function has to clean up all subref
style runs. */

PG_PASCAL (void) pgDecrementSubsetStyles (paige_rec_ptr pg, pg_short_t bad_index,
		pg_short_t max_styles, pg_boolean do_par)
{
	long			stack_qty;
	
	decrement_subset_styles(pg->tlevel_blocks, bad_index, max_styles, do_par);
	
	if ((stack_qty = GetMemorySize(pg->subref_stack)) > 0) {
		paige_sub_ptr		sub_ptr;
		
		sub_ptr = (paige_sub_ptr) UseMemory(pg->subref_stack);
		
		while (stack_qty) {
			
			if (sub_ptr->insert_style > bad_index)
				--sub_ptr->insert_style;
			
			++sub_ptr;
			--stack_qty;
		}

		UnuseMemory(pg->subref_stack);
	}
}


/* pgSetSubRefText sets an alternate text string. If the_text is NULL we just set the index
to be the current display item. */

PG_PASCAL (void) pgSetSubRefText (pg_ref pg, pg_subref ref, long index,
		pg_byte_ptr the_text, short text_size, short stylesheet_option, short draw_mode)
{
	paige_rec_ptr			pg_rec;
	paige_sub_ptr			sub_ptr;
	pg_hyperlink_ptr		hyperlink;
	text_block_ptr			block;
	style_run_ptr			run;
	style_info_ptr			first_style;
	long					index_size;
	
	pg_rec = (paige_rec_ptr) UseMemory(pg);
	index_size = GetMemorySize(ref);

	if (index >= index_size) {
		
		AppendMemory(ref, index - index_size + 1, TRUE);
		UnuseMemory(ref);
	}
	
	sub_ptr = (paige_sub_ptr) UseMemory(ref);
	
	if (sub_ptr[index].t_blocks == MEM_NULL)
		pgAllocateAlternate(sub_ptr, index);

	sub_ptr->alt_index = index;
	
	if (the_text && text_size) {

		sub_ptr += index;

		block = (text_block_ptr) UseMemory(sub_ptr->t_blocks);
		SetMemorySize(block->text, text_size);
		block->end = text_size;
		pgBlockMove(the_text, UseMemory(block->text), text_size);
		UnuseMemory(block->text);
		
		sub_ptr->t_length = text_size;
		
		SetMemorySize(sub_ptr->t_style_run, 2);
		SetMemorySize(sub_ptr->par_style_run, 2);
		run = (style_run_ptr) UseMemory(sub_ptr->t_style_run);
		run[1].offset = text_size + ZERO_TEXT_PAD;
		
		if (stylesheet_option)
			if (pgLocateStyleSheet(pg_rec, stylesheet_option, &run->style_item))
				UnuseMemory(pg_rec->t_formats);
		
		sub_ptr->insert_style = run->style_item;
		
		first_style = (style_info_ptr) UseMemoryRecord(pg_rec->t_formats, (long)run->style_item, 0, TRUE);
		first_style->used_ctr += 1;
		UnuseMemory(pg_rec->t_formats);
		UnuseMemory(sub_ptr->t_style_run);
		
		run = (style_run_ptr) UseMemory(sub_ptr->par_style_run);
		run[1].offset = text_size + ZERO_TEXT_PAD;
		UnuseMemory(sub_ptr->par_style_run);
		
		UnuseMemory(sub_ptr->t_blocks);
		
		SetMemorySize(sub_ptr->hyperlinks, 1);
		SetMemorySize(sub_ptr->target_hyperlinks, 1);
		hyperlink = (pg_hyperlink_ptr) UseMemory(sub_ptr->hyperlinks);
		hyperlink->applied_range.begin = hyperlink->applied_range.end = text_size + ZERO_TEXT_PAD;
		UnuseMemory(sub_ptr->hyperlinks);
		hyperlink = (pg_hyperlink_ptr) UseMemory(sub_ptr->target_hyperlinks);
		hyperlink->applied_range.begin = hyperlink->applied_range.end = text_size + ZERO_TEXT_PAD;
		UnuseMemory(sub_ptr->target_hyperlinks);

		pgInvalSubBlocks(pg_rec, sub_ptr);
		sub_ptr -= index;
	}
	
	if (pg_rec->active_subset) {
	
		if (pg_rec->active_subset == ref)
			pgCopySubfields(pg_rec, sub_ptr, FALSE);

		invalidate_hierarchy(pg_rec);
	}
	else
		invalidate_subref(pg_rec, ref);

	UnuseMemory(ref);
	
	if (draw_mode)
		pgUpdateText(pg_rec, NULL, 0, pg_rec->t_length, MEM_NULL, NULL, bits_copy, TRUE);

	UnuseMemory(pg);
}


/* pgAllocateAlternate allocates the necessaru fields for a new alternate subref. */

PG_PASCAL (void) pgAllocateAlternate (paige_sub_ptr sub_ptr, long index)
{
	pgm_globals_ptr	mem_globals;
	long			old_mem_id;
	
	pgBlockMove(sub_ptr, &sub_ptr[index], sizeof(paige_sub_rec));

	mem_globals = GetGlobalsFromRef(sub_ptr->t_blocks);
	old_mem_id = mem_globals->current_id;
	mem_globals->current_id = sub_ptr->mem_id;
	initialize_sub_record(mem_globals, &sub_ptr[index]);
	mem_globals->current_id = old_mem_id;

	sub_ptr[index].t_length = 0;
	sub_ptr[index].insert_style = 0;
	sub_ptr[index].num_selects = 0;
	sub_ptr[index].alt_index = 0;
}



/* pgGetSubRefText returns the alternate text memory_ref at index. If index is < 0 then
the current alternate text is used. */

PG_PASCAL (memory_ref) pgGetSubRefText (pg_ref pg, pg_subref ref, long index)
{
	paige_sub_ptr			sub_ptr;
	text_block_ptr			block;
	long					use_index;
	memory_ref				result;
	
	result = MEM_NULL;
	sub_ptr = (paige_sub_ptr) UseMemory(ref);

	if (index < 0)
		use_index = sub_ptr->alt_index;
	else
		use_index = index;

	if (use_index < GetMemorySize(ref)) {
	
		sub_ptr += use_index;
		
		if (sub_ptr->t_blocks) {
		
			block = (text_block_ptr) UseMemory(sub_ptr->t_blocks);
			result = block->text;
			UnuseMemory(sub_ptr->t_blocks);
		}
	}

	UnuseMemory(ref);
	return	result;
}


/* pgInvalSubBlocks invalidates all the text within sub_ptr */

PG_PASCAL (void) pgInvalSubBlocks (paige_rec_ptr pg, paige_sub_ptr sub_ptr)
{
	pg_globals_ptr		globals;
	text_block_ptr		block;
	pg_short_t			charloc_index;
	
	globals = pg->globals;

	block = (text_block_ptr) UseMemory(sub_ptr->t_blocks);
	block->flags |= NEEDS_CALC;
	UnuseMemory(sub_ptr->t_blocks);
	
	for (charloc_index = 0; charloc_index < CREATED_WIDTH_QTY; ++charloc_index)
		if (globals->width_tables[charloc_index].mem_id == sub_ptr->mem_id)
			pgFreeCharLocs(pg, charloc_index);
	
	sub_ptr->width_table.offset = -1;
	sub_ptr->width_table.used_ctr = 0;
}


/* pgMatrixWidth figures out the width of the current subref (which is the active subset)
assuming that subref contains rows and columns of other subrefs in a matrix or "table" format.
This is done by assuming a column is as wide as the widest cell. */

PG_PASCAL (long) pgMatrixWidth (paige_rec_ptr pg, paige_sub_ptr home_sub_ptr, text_block_ptr block)
{
	register short				row_index;
	point_start_ptr				starts;
	pg_subref_ptr				subref_list;
	paige_sub_ptr				sub_ptr;
	memory_ref					matrix_ref;
	pg_subref					subref;
	subref_matrix_ptr			matrix_ptr;
	long						widest_width, num_subs, result, generated_extra;
	short						num_columns, num_rows, row_ctr, column_ctr;

	starts = (point_start_ptr) UseMemory(block->lines);
	result = 0;
	generated_extra = 100000;

	num_columns = 0;

	while (starts->flags != TERMINATOR_BITS) {
		
		if (starts->extra & EXTRA_IS_SUBREF)
			num_columns += 1;

		++starts;
		
		if (starts->flags & NEW_PAR_BIT)
			break;
	}
	
	num_rows = 1;
	
	while (starts->flags != TERMINATOR_BITS) {
	
		if (starts->flags & NEW_PAR_BIT)
			++num_rows;
		
		++starts;
	}
	
	UnuseMemory(block->lines);
	
	if ( (num_columns > 0) && (num_rows > 1) ) {
		
		matrix_ref = MemoryAllocClear(pg->globals->mem_globals, (short)sizeof(subref_matrix_info),
				(long)0, (short)((num_columns * num_rows) + 1));

		subref_list = (pg_subref_ptr) UseMemory(block->subref_list);
		num_subs = GetMemorySize(block->subref_list);
		
		while (num_subs) {
			
			matrix_ptr = (subref_matrix_ptr) AppendMemory(matrix_ref, 1, FALSE);
			sub_ptr = (paige_sub_ptr) UseMemory(*subref_list);
			matrix_ptr->ref = *subref_list;
			matrix_ptr->width = sub_ptr->subset_bounds.bot_right.h - sub_ptr->subset_bounds.top_left.h;
			UnuseMemory(*subref_list);
			UnuseMemory(matrix_ref);
			
			++subref_list;
			--num_subs;
		}
		
		UnuseMemory(block->subref_list);
		
		matrix_ptr = (subref_matrix_ptr) UseMemory(matrix_ref);
		
		for (column_ctr = 0; column_ctr < num_columns; ++column_ctr) {
			
			widest_width = 0;

			for (row_index = row_ctr = 0; row_ctr < num_rows; ++row_ctr, row_index += num_columns) {
				
				if (matrix_ptr[row_index].width > widest_width)
					widest_width = matrix_ptr[row_index].width;
			}

			result += widest_width;

	// Here is where we actually change the subref widths:
	
			for (row_index = row_ctr = 0; row_ctr < num_rows; ++row_ctr, row_index += num_columns) {
				
				subref = matrix_ptr[row_index].ref;
				sub_ptr = (paige_sub_ptr) UseMemory(subref);
				sub_ptr->minimum_width = widest_width;
				
				if (sub_ptr->minimum_width < sub_ptr->empty_width)
					sub_ptr->minimum_width = sub_ptr->empty_width;

				if (row_ctr == 0)
					result += (sub_ptr->left_extra + sub_ptr->right_extra);

				if (column_ctr == 0) {
					long		generated;
					
					generated = (sub_ptr->minimum_width - sub_ptr->actual_text_width);
					
					if (generated < generated_extra)
						generated_extra = generated;
				}

				UnuseMemory(subref);
			}

			++matrix_ptr;
		}

		UnuseAndDispose(matrix_ref);
	}

	if (generated_extra < 100000 && generated_extra > 0)
		home_sub_ptr->generated_extra = generated_extra;
	else
		home_sub_ptr->generated_extra = 0;

	return	result;
}


/* pgClearMatrixWidths clears all the "minimum widths" for each element in the matrix. */

PG_PASCAL (void) pgInvalMatrix (paige_rec_ptr pg, text_block_ptr block, pg_boolean zero_widths)
{
	pg_subref_ptr		subref_list;
	
	long				num_subs;
	paige_sub_ptr		sub_ptr;

	subref_list = (pg_subref_ptr) UseMemory(block->subref_list);
	num_subs = GetMemorySize(block->subref_list);
	
	while (num_subs) {
		
		sub_ptr = (paige_sub_ptr) UseMemory(*subref_list);
		
		if (zero_widths)
			sub_ptr->minimum_width = sub_ptr->empty_width;
		else
			pgInvalSubBlocks(pg, sub_ptr);

		UnuseMemory(*subref_list);
		
		++subref_list;
		--num_subs;
	}
	
	UnuseMemory(block->subref_list);
}


/* pgFindSubrefPosition returns the relative position for a subref assuming it is embedded
into local text position local_position. */

PG_PASCAL (long) pgFindSubrefPosition (text_block_ptr block, pg_short_t local_position)
{
	pg_byte_ptr				text;
	register pg_short_t		subref_pos, text_pos;
	short					text_size, num_subs;
	
	text_size = (short)(block->end - block->begin);
	num_subs = (short)GetMemorySize(block->subref_list);
	
	if (text_size == 0 || local_position == 0 || num_subs == 0)
		return	0;
	
	text = (pg_byte_ptr) UseMemory(block->text);
	subref_pos = text_pos = 0;
	
	while (text_pos < local_position) {
		
		if (subref_pos >= (pg_short_t)num_subs)
			break;

		if (text[text_pos] == SUBREF_CHAR)
			subref_pos += 1;

		text_pos += 1;
	}

	UnuseMemory(block->text);

	return	subref_pos;
}



/* pgSubrefHomePosition returns the home position of subref (the text position, one level up,
the contains this subref). */

PG_PASCAL (long) pgSubrefHomePosition (paige_rec_ptr pg, pg_subref subref)
{
	paige_sub_ptr			home_ptr, sub_ptr;
	block_ref				blockref;

	sub_ptr = (paige_sub_ptr) UseMemory(subref);
	
	if (sub_ptr->home_sub) {
		
		home_ptr = (paige_sub_ptr) UseMemory(sub_ptr->home_sub);
		blockref = home_ptr->t_blocks;
		UnuseMemory(sub_ptr->home_sub);
	}
	else
		blockref = pg->tlevel_blocks;

	UnuseMemory(subref);
	
	return		locate_subref_position(blockref, subref);
}


/* pgSubsetGlitter handles border(s) if the subref is a table. */

PG_PASCAL (void) pgSubsetGlitter (paige_rec_ptr pg, draw_points_ptr draw_position,
		rectangle_ptr draw_bounds, co_ordinate_ptr offset_extra, style_walk_ptr walker)
{
}


/* pgGetNthSubref returns the nth subref given in index. */

PG_PASCAL (pg_subref) pgGetNthSubref (paige_rec_ptr pg, long index)
{
	text_block_ptr				block;
	pg_subref					result;
	pg_subref_ptr				subref_list;
	long						num_blocks, num_subs, counter;
	
	if (index == 0)
		return	MEM_NULL;

	block = (text_block_ptr) UseMemory(pg->t_blocks);
	num_blocks = GetMemorySize(pg->t_blocks);
	result = MEM_NULL;
	counter = index - 1;

	while (num_blocks) {
		
		num_subs = GetMemorySize(block->subref_list);
		
		if (num_subs > counter) {
			
			subref_list = (pg_subref_ptr) UseMemory(block->subref_list);
			result = subref_list[counter];
			UnuseMemory(block->subref_list);
			
			break;
		}
		
		counter -= num_subs;

		++block;
		--num_blocks;
	}
	
	UnuseMemory(pg->t_blocks);
	
	return		result;
}

/* pgGetSubrefInSelect returns the subref that is selected (or has insertion point to left). */

PG_PASCAL (pg_subref) pgGetSubrefInSelect (paige_rec_ptr pg)
{
	text_block_ptr			block;
	pg_subref				result;
	long					selection, num_subs;
	
	selection = pgCurrentInsertion(pg);
	block = pgFindTextBlock(pg, selection, NULL, FALSE, TRUE);
	result = MEM_NULL;

	num_subs = GetMemorySize(block->subref_list);
	
	if (num_subs > 0) {
		
		selection = pgFindSubrefPosition(block, (pg_short_t)(selection - block->begin));
		
		if (selection < num_subs)
			GetMemoryRecord(block->subref_list, selection, (void PG_FAR *)&result);
	}
	
	UnuseMemory(pg->t_blocks);
	
	return	result;
}


/* pgDeleteSubRefs removes all subrefs in block, beginning at local_offset up to (but not
including) local_offset + length. */

PG_PASCAL (void) pgDeleteSubRefs (text_block_ptr block, long local_offset, long length)
{
	register pg_byte_ptr		text;
	pg_subref					subref;
	long						offset_ctr, end_offset, subref_ctr, num_subs;
	
	if ((num_subs = GetMemorySize(block->subref_list)) == 0)
		return;
	
	text = (pg_byte_ptr) UseMemory(block->text);
	
	for (subref_ctr = offset_ctr = 0; offset_ctr < local_offset; ++offset_ctr)
		if (text[offset_ctr] == SUBREF_CHAR)
			subref_ctr += 1;
	
	end_offset = local_offset + length;

	while ( (subref_ctr < num_subs) && offset_ctr < end_offset) {
		
		if (text[offset_ctr] == SUBREF_CHAR) {
			
			if (subref_ctr < GetMemorySize(block->subref_list)) {
			
				GetMemoryRecord(block->subref_list, subref_ctr, (void PG_FAR *)&subref);
				pgDisposeSubRef(subref);
				DeleteMemory(block->subref_list, subref_ctr, 1);
			}
			
			num_subs -= 1;
		}
		
		++offset_ctr;
	}

	UnuseMemory(block->text);
}


/* pgDisposeAllSubRefs disposes every subref within blocks, recursing other refs if necessary. */

PG_PASCAL (void) pgDisposeAllSubRefs (block_ref blocks)
{
	text_block_ptr				block;
	pg_subref_ptr				subref_list;
	long						num_blocks, num_subs;
	
	num_blocks = GetMemorySize(blocks);
	block = (text_block_ptr) UseMemory(blocks);
	
	while (num_blocks) {
		
		if ((num_subs = GetMemorySize(block->subref_list)) > 0) {
			
			subref_list = (pg_subref_ptr) UseMemory(block->subref_list);
			
			while (num_subs) {
				
				pgDisposeSubRef(*subref_list);

				++subref_list;
				--num_subs;
			}

			UnuseMemory(block->subref_list);
		}

		++block;
		--num_blocks;
	}
	
	UnuseMemory(blocks);
}


/* pgDuplicateAllSubRefs forces every subref within blocks to be a duplicate. This function
may be recursive. */

PG_PASCAL (void) pgDuplicateAllSubRefs (paige_rec_ptr src_pg, paige_rec_ptr target_pg,
			block_ref blocks, pg_subref home_ref)
{
	text_block_ptr				block;
	pg_subref_ptr				subref_list;
	long						num_blocks, num_subs;
	
	num_blocks = GetMemorySize(blocks);
	block = (text_block_ptr) UseMemory(blocks);
	
	while (num_blocks) {
		
		if ((num_subs = GetMemorySize(block->subref_list)) > 0) {
			
			subref_list = (pg_subref_ptr) UseMemory(block->subref_list);
			
			while (num_subs) {
				
				*subref_list = pgDuplicateRef(src_pg, target_pg, *subref_list, home_ref);

				++subref_list;
				--num_subs;
			}

			UnuseMemory(block->subref_list);
		}

		++block;
		--num_blocks;
	}
	
	UnuseMemory(blocks);
}


/* pgCopySubRefs copies all subrefs from the text in source_pg to target_pg. */

PG_PASCAL (void) pgCopySubRefs (paige_rec_ptr src_pg, paige_rec_ptr target_pg,
		select_pair_ptr source_range, long target_begin)
{
	text_block_ptr			block;
	pg_byte_ptr				text;
	pg_subref_ptr			subref_list;
	pg_subref				subref;
	long					offset_ctr, subref_index;
	long					begin_text, end_text, end_block;

	block = pgFindTextBlock(target_pg, target_begin, NULL, FALSE, TRUE);
	
	offset_ctr = 0;
	begin_text = target_begin;
	end_text = begin_text + (source_range->end - source_range->begin);
	
	if (end_text > target_pg->t_length)
		end_text = target_pg->t_length;

	subref_index = position_to_index(block, target_begin - block->begin);

	while (begin_text < end_text) {
		
		text = (pg_byte_ptr) UseMemory(block->text);
		text += (begin_text - block->begin);
		
		if ((end_block = block->end) > end_text)
			end_block = end_text;
		
		while (begin_text < end_block) {
			
			if (*text == SUBREF_CHAR) {
				
				if ((subref = pgGetSourceSubRef(src_pg, source_range->begin + offset_ctr)) != MEM_NULL) {
				
					subref_list = (pg_subref_ptr) InsertMemory(block->subref_list, subref_index, 1);
					*subref_list = pgDuplicateRef(src_pg, target_pg, subref, target_pg->active_subset);
					UnuseMemory(block->subref_list);

					subref_index += 1;
				}
			}

			++begin_text;
			++text;
			++offset_ctr;
		}

		UnuseMemory(block->text);

		begin_text = block->end;
		subref_index = 0;
		block += 1;
	}

	UnuseMemory(target_pg->t_blocks);
}


/* pgGetSourceSubRef returns the subref associated with position. */

PG_PASCAL (pg_subref) pgGetSourceSubRef (paige_rec_ptr pg, long position)
{
	text_block_ptr				block;
	pg_subref					subref = MEM_NULL;
	long						subref_pos;
	
	block = pgFindTextBlock(pg, position, NULL, FALSE, TRUE);
	subref_pos = position_to_index(block, position - block->begin);
	
	if (is_valid_subref(block, subref_pos))
		GetMemoryRecord(block->subref_list, subref_pos, (void PG_FAR *)&subref);
	
	UnuseMemory(pg->t_blocks);
	
	return		subref;
}


/* pgPackSubRefs packs (for file saving) all the subrefs in the text block. This makes
recursive calls, if necessary. */

PG_PASCAL (void) pgPackSubRefs (pack_walk_ptr walker, text_block_ptr block)
{
	pg_subref_ptr	subref_list;
	paige_sub_ptr	sub_ptr;
	long			num_subs, alternate_qty;
	
	num_subs = GetMemorySize(block->subref_list);
	pgPackNum(walker, long_data, num_subs);
	
	if (num_subs) {
		
		subref_list = (pg_subref_ptr) UseMemory(block->subref_list);
		
		while (num_subs) {
			
			sub_ptr = (paige_sub_ptr) UseMemory(*subref_list);
			alternate_qty = GetMemorySize(*subref_list);
			pgPackNum(walker, long_data, alternate_qty);
			
			while (alternate_qty) {
				
				pgPackNum(walker, long_data, sub_ptr->t_length);
				pgPackNum(walker, short_data, (long)sub_ptr->num_selects);
				pgPackNum(walker, short_data, (long)sub_ptr->insert_style);
				pgPackNum(walker, long_data, sub_ptr->flags);
				pgPackRect(walker, &sub_ptr->subset_bounds);
				pgPackNum(walker, long_data, sub_ptr->empty_width);
				pgPackNum(walker, long_data, sub_ptr->minimum_width);
				pgPackNum(walker, long_data, sub_ptr->home_position);
				pgPackNum(walker, long_data, sub_ptr->subref_flags);
				pgPackNum(walker, short_data, sub_ptr->descent);
				pgPackNum(walker, short_data, sub_ptr->ascent);
				pgPackNum(walker, short_data, sub_ptr->left_extra);
				pgPackNum(walker, short_data, sub_ptr->right_extra);
				pgPackNum(walker, short_data, sub_ptr->top_extra);
				pgPackNum(walker, short_data, sub_ptr->bot_extra);
				pgPackNum(walker, long_data, sub_ptr->alignment_flags);
				pgPackNum(walker, long_data, sub_ptr->modifier);
				pgPackNum(walker, long_data, sub_ptr->callback_refcon);
				pgPackNum(walker, short_data, sub_ptr->reserved1);
				pgPackNum(walker, long_data, sub_ptr->reserved2);
				pgPackBytes(walker, (pg_bits8_ptr)&sub_ptr->reserved3, sizeof(double));
				pgPackNum(walker, short_data, sub_ptr->reserved4);
				pgPackNum(walker, short_data, sub_ptr->reserved5);
				pgPackNum(walker, long_data, sub_ptr->reserved6);
				pgPackNum(walker, long_data, sub_ptr->alt_index);

				pgPackNum(walker, short_data, sub_ptr->tighten_ascent);
				pgPackNum(walker, short_data, sub_ptr->tighten_descent);

				pack_style_run(walker, sub_ptr->t_style_run);
				pack_style_run(walker, sub_ptr->par_style_run);
				pack_select_pairs(walker, sub_ptr->select);

				pgPackNum(walker, long_data, GetMemorySize(sub_ptr->hyperlinks));
				pgPackHyperlinks(walker, sub_ptr->hyperlinks, NULL);
				pgPackNum(walker, long_data, GetMemorySize(sub_ptr->target_hyperlinks));
				pgPackHyperlinks(walker, sub_ptr->target_hyperlinks, NULL);

				pgPackTextBlock(walker, (text_block_ptr) UseMemory(sub_ptr->t_blocks), TRUE, TRUE);
				UnuseMemory(sub_ptr->t_blocks);

				++sub_ptr;
				--alternate_qty;
			}

			UnuseMemory(*subref_list);

			++subref_list;
			--num_subs;
		}
		
		UnuseMemory(block->subref_list);
	}
}


/* pgUnpackSubRefs unpacks (for file saving) all the subrefs in the text block. This makes
recursive calls, if necessary. */

PG_PASCAL (long) pgUnpackSubRefs (paige_rec_ptr pg, pack_walk_ptr walker, text_block_ptr block,
			 pg_subref home_ref)
{
	pg_subref_ptr		subref_list;
	paige_sub_ptr		sub_ptr;
	text_block_ptr		sub_block;
	pg_hyperlink_ptr	def_link;
	pg_subref			subref;
	long				num_subs, result, alternate_qty, alt_index;
	long				old_mem_id;

	old_mem_id = pg->globals->mem_globals->current_id;

	num_subs = result = pgUnpackNum(walker);

	while (num_subs) {
		
		subref = pgNewSubRef(GetGlobalsFromRef(block->text));
		subref_list = (pg_subref_ptr) AppendMemory(block->subref_list, 1, FALSE);
		*subref_list = subref;
		UnuseMemory(block->subref_list);

		alternate_qty = pgUnpackNum(walker);
		sub_ptr = (paige_sub_ptr) UseMemory(subref);
		alt_index = 0;

		while (alternate_qty) {

			sub_ptr->home_sub = home_ref;
			sub_ptr->t_length = pgUnpackNum(walker);
			sub_ptr->num_selects = (pg_short_t)pgUnpackNum(walker);
			sub_ptr->insert_style = (pg_short_t)pgUnpackNum(walker);
			sub_ptr->flags = pgUnpackNum(walker);

			pgUnpackRect(walker, &sub_ptr->subset_bounds);

			sub_ptr->empty_width = pgUnpackNum(walker);
			sub_ptr->minimum_width = pgUnpackNum(walker);
			sub_ptr->home_position = pgUnpackNum(walker);
			sub_ptr->subref_flags = pgUnpackNum(walker);
			sub_ptr->subref_flags |= MATRIX_PHASE_FLAG;

			sub_ptr->descent = (short)pgUnpackNum(walker);
			sub_ptr->ascent = (short)pgUnpackNum(walker);
			sub_ptr->left_extra = (short)pgUnpackNum(walker);
			sub_ptr->right_extra = (short)pgUnpackNum(walker);
			sub_ptr->top_extra = (short)pgUnpackNum(walker);
			sub_ptr->bot_extra = (short)pgUnpackNum(walker);
			
			sub_ptr->alignment_flags = pgUnpackNum(walker);
			sub_ptr->modifier = pgUnpackNum(walker);
			sub_ptr->callback_refcon = pgUnpackNum(walker);
			sub_ptr->reserved1 = (short)pgUnpackNum(walker);
			sub_ptr->reserved2 = pgUnpackNum(walker);
			pgUnpackPtrBytes(walker, (pg_bits8_ptr)&sub_ptr->reserved3);

			sub_ptr->reserved4 = (short)pgUnpackNum(walker);
			sub_ptr->reserved5 = (short)pgUnpackNum(walker);
			sub_ptr->reserved6 = pgUnpackNum(walker);
			sub_ptr->alt_index = pgUnpackNum(walker);
			
			if (pg->version >= KEY_REVISION16) {
				
				sub_ptr->tighten_ascent = (short)pgUnpackNum(walker);
				sub_ptr->tighten_descent = (short)pgUnpackNum(walker);
			}

			unpack_style_run(walker, sub_ptr->t_style_run);
			unpack_style_run(walker, sub_ptr->par_style_run);
			unpack_select_pairs(walker, sub_ptr->select);
			
			if (pg->version >= KEY_REVISION19) {
				ht_callback			callback;
				long				links_qty;
				
				def_link = (pg_hyperlink_ptr) UseMemory(pg->hyperlinks);
				callback = def_link->callback;
				UnuseMemory(pg->hyperlinks);
				links_qty = pgUnpackNum(walker);
				pgUnpackHyperlinks(pg, walker, callback, links_qty, sub_ptr->hyperlinks);

				def_link = (pg_hyperlink_ptr) UseMemory(pg->target_hyperlinks);
				callback = def_link->callback;
				UnuseMemory(pg->target_hyperlinks);
				links_qty = pgUnpackNum(walker);
				pgUnpackHyperlinks(pg, walker, callback, links_qty, sub_ptr->target_hyperlinks);
			}
			else {

				def_link = (pg_hyperlink_ptr) UseMemory(sub_ptr->hyperlinks);
				def_link->applied_range.begin = def_link->applied_range.end = sub_ptr->t_length + ZERO_TEXT_PAD;
				UnuseMemory(sub_ptr->hyperlinks);

				def_link = (pg_hyperlink_ptr) UseMemory(sub_ptr->target_hyperlinks);
				def_link->applied_range.begin = def_link->applied_range.end = sub_ptr->t_length + ZERO_TEXT_PAD;
				UnuseMemory(sub_ptr->target_hyperlinks);
			}

			sub_block = (text_block_ptr) UseMemory(sub_ptr->t_blocks);
			pgUnpackTextBlock(pg, walker, sub_block, TRUE, subref);
			sub_block->flags |= NEEDS_CALC;
			UnuseMemory(sub_ptr->t_blocks);

			--alternate_qty;
			++alt_index;

			if (alternate_qty > 0) {
				
				AppendMemory(subref, 1, TRUE);
				sub_ptr = (paige_sub_ptr) UseMemoryRecord(subref, 0, USE_ALL_RECS, FALSE);
				pgAllocateAlternate(sub_ptr, alt_index);
				sub_ptr += alt_index;
			}
		}
		
		UnuseMemory(subref);

		--num_subs;
	}
	
	pg->globals->mem_globals->current_id = old_mem_id;
	
	return		result;
}


/* pgRedrawSubsetLine draws a single line containing subref. If the line containing
this subref begins at previous_redraw, we do nothing (the assumption is the line just re-drew). */

PG_PASCAL (long) pgRedrawSubsetLine (paige_rec_ptr pg_rec, pg_subref subref, long previous_redraw)
{
	paige_sub_ptr			sub_ptr;
	memory_ref				subref_state;
	pg_subref				current_ref, next_ref;
	text_block_ptr			block;
	point_start_ptr			starts;
	pg_short_t				local_position;
	long					home_home_position, end_position;

	if (!subref)
		return	previous_redraw;
	
	current_ref = subref;
	sub_ptr = (paige_sub_ptr) UseMemory(current_ref);
	
	while (sub_ptr->home_sub) {
		
		next_ref = sub_ptr->home_sub;
		UnuseMemory(current_ref);
		current_ref = next_ref;
		sub_ptr = (paige_sub_ptr) UseMemory(current_ref);
	}
	
	UnuseMemory(current_ref);
	home_home_position = pgSubrefHomePosition(pg_rec, current_ref);
	
	subref_state = pgGetSubrefState(pg_rec, FALSE, TRUE);
	
	block = pgFindTextBlock(pg_rec, home_home_position, NULL, TRUE, TRUE);
	local_position = (pg_short_t)(home_home_position - block->begin);
	starts = (point_start_ptr) UseMemory(block->lines);
	
	while (starts[1].offset <= local_position) {
		
		if (starts[1].offset == TERMINATOR_BITS)
			break;

		++starts;
	}
	
	while (starts->offset) {
		
		if (starts->flags & NEW_LINE_BIT)
			break;
		
		--starts;
	}
	
	home_home_position = (long)starts->offset;
	home_home_position += block->begin;
	
	while (!(starts->flags & LINE_BREAK_BIT)) {
		
		if (starts[1].flags == TERMINATOR_BITS)
			break;
		
		++starts;
	}

	end_position = starts[1].offset;
	end_position += block->begin;

	UnuseMemory(block->lines);
	UnuseMemory(pg_rec->t_blocks);
	
	if (home_home_position != previous_redraw)
		pgUpdateText(pg_rec, NULL, home_home_position, end_position, MEM_NULL, NULL, bits_copy, TRUE);

	pgRestoreSubRefs(pg_rec, subref_state);
	
	return		home_home_position;
}

/************************* Support for pagination **********************/

PG_PASCAL (pg_boolean) pgActiveMatrix (paige_rec_ptr pg)
{
	paige_sub_ptr		sub_ptr;
	pg_boolean			result = FALSE;

	if (pg->active_subset) {
		
		sub_ptr = (paige_sub_ptr) UseMemory(pg->active_subset);
		
		result = (pg_boolean)((sub_ptr->subref_flags & SUBREF_MATRIXALIGN) != 0);
		
		UnuseMemory(pg->active_subset);
	}
	
	return		result;
}


PG_PASCAL (long PG_FAR*) pgGetSubrefCharLocs (paige_rec_ptr pg, text_block_ptr block,
		memory_ref PG_FAR *used_ref, memory_ref PG_FAR *types_ref)
{
	long PG_FAR 		*result;
	paige_sub_ptr		sub_ptr;

	if (pgActiveMatrix(pg)) {
		
		sub_ptr = (paige_sub_ptr) UseMemory(pg->active_subset);
		pgInvalSubBlocks(pg, sub_ptr);
		pgInvalMatrix(pg, block, TRUE);
		pgInvalMatrix(pg, block, FALSE);
		pgGetCharLocs(pg, block, used_ref, types_ref); // forces a re-measure
		UnuseMemory(*used_ref);
		pgReleaseCharLocs(pg, *used_ref);
		pgMatrixWidth(pg, sub_ptr, block);
		pgInvalMatrix(pg, block, FALSE);
		pgInvalSubBlocks(pg, sub_ptr);
		result = pgGetCharLocs(pg, block, used_ref, types_ref);

		UnuseMemory(pg->active_subset);
	}
	else
		result = pgGetCharLocs(pg, block, used_ref, types_ref);

	return	result;
}

/* pgFindNextSubset returns the amount of bytes to next subset, if any. */

PG_PASCAL (long) pgFindNextSubset (short PG_FAR *char_types, long remaining)
{
	register long 		ctr;
	
	for (ctr = 0; ctr < remaining; ++ctr)
		if ((char_types[ctr] & PG_SUBSET_BIT) == PG_SUBSET_BIT)
			break;
	
	return	ctr;
}


/* pgGetThisSubset returns the subref associated with the current point_start. */

PG_PASCAL (memory_ref) pgGetThisSubset (pg_measure_ptr line_info, long current_offset)
{
	register long		index;
	pg_subref_ptr		subref_list;
	short PG_FAR		*types;
	memory_ref			result;
	long				subref_index;
	
	types = line_info->types;
	subref_index = 0;
	
	for (index = 0; index < current_offset; ++index)
		if ((types[index] & PG_SUBSET_BIT) == PG_SUBSET_BIT)
			subref_index += 1;
	
	subref_list = (pg_subref_ptr) UseMemory(line_info->block->subref_list);
	result = subref_list[subref_index];
	UnuseMemory(line_info->block->subref_list);
	
	return	result;
}


/* pgPtInSubRef returns a subref, if any, that contains point. */

PG_PASCAL (pg_subref) pgPtInSubRef (paige_rec_ptr pg, co_ordinate_ptr mouse_point)
{
	text_block_ptr			block;
	paige_sub_ptr			sub_ptr;
	pg_subref_ptr			subref_list;
	pg_subref				result;
	pg_boolean				pt_in_rect;
	co_ordinate				point;
	long					num_blocks, num_subs;
	
	result = MEM_NULL;
	point = *mouse_point;
	pgAddPt(&pg->scroll_pos, &point);

	num_blocks = GetMemorySize(pg->t_blocks);
	block = (text_block_ptr) UseMemory(pg->t_blocks);
	
	while (num_blocks && (result == MEM_NULL)) {
		
		num_subs = GetMemorySize(block->subref_list);
		subref_list = (pg_subref_ptr) UseMemory(block->subref_list);
		
		while (num_subs) {
			
			sub_ptr = (paige_sub_ptr) UseMemory(*subref_list);
			sub_ptr += sub_ptr->alt_index;

			if (sub_ptr->subref_flags & SUBREF_DRAWN)
				pt_in_rect = pgPtInRect(&point, &sub_ptr->subset_bounds);
			else
				pt_in_rect = FALSE;

			UnuseMemory(*subref_list);
			
			if (pt_in_rect) {
				
				result = *subref_list;
				break;
			}
			
			++subref_list;
			--num_subs;
		}
		
		UnuseMemory(block->subref_list);
		
		++block;
		--num_blocks;
	}
	
	UnuseMemory(pg->t_blocks);
	
	return		result;
}

/* pgIsValidSubref returns TRUE if the subref is valid per the (local) text position. */

PG_PASCAL (pg_boolean) pgIsValidSubref (text_block_ptr block, long local_text_pos)
{
	memory_ref		subref_list;
	long			sub_index;
	
	if ((subref_list = block->subref_list) == MEM_NULL)
		return	FALSE;

	if (GetMemorySize(subref_list) == 0)
		return	FALSE;
	
	sub_index = position_to_index(block, local_text_pos);
	return	(pg_boolean)(sub_index < GetMemorySize(block->subref_list));
}

/********************************** Local Functions *****************************************/


/* is_valid_subref returns TRUE if the subref is valid per the subref index. */

static pg_boolean is_valid_subref (text_block_ptr block, long index)
{
	if (!block->subref_list)
		return	FALSE;
	
	return	(pg_boolean)(index < GetMemorySize(block->subref_list));
}

static long position_to_index (text_block_ptr block, long position)
{
	register pg_byte_ptr		text;
	register long				index;
	long						result;
	
	text = (pg_byte_ptr) UseMemory(block->text);

	for (index = 0, result = 0; index < position; index += 1)
		if (text[index] == SUBREF_CHAR)
			result += 1;
	
	UnuseMemory(block->text);

	return		result;
}


/* pin_bounds_to_rect makes sure that the bounding rect of pg_subref is "pinned" to bounds,
moving all the internal point_starts if necessary. The final bounding rect is recorded within
the subref "subset_bounds" member.
TECHNICAL NOTE:  The subref is known to be nested somewhere in the text stream of pg, and it
is NOT yet "in use". This might make it easier to locate the ref to left or right. */

static void pin_bounds_to_rect (paige_rec_ptr pg, pg_subref subref, rectangle_ptr bounds,
		long flags, draw_points_ptr draw_position)
{
	paige_sub_ptr		sub_ptr;
	text_block_ptr		block;
	point_start_ptr		starts;
	rectangle			initial_bounds, resulting_bounds;
	long				num_blocks, offset_h, offset_v, baseline_diff;
	long				alignment_flags, width;

	sub_ptr = (paige_sub_ptr) UseMemory(subref);
	sub_ptr += sub_ptr->alt_index;

	num_blocks = GetMemorySize(sub_ptr->t_blocks);
	block = (text_block_ptr) UseMemory(sub_ptr->t_blocks);

	offset_h = bounds->top_left.h - block->bounds.top_left.h;
	offset_v = bounds->top_left.v - block->bounds.top_left.v;
	
	if (sub_ptr->subref_flags & SUBREF_IS_TABLE) {
		style_run_ptr		run;
		par_info_ptr		par;
		
		run = (style_run_ptr) UseMemory(sub_ptr->par_style_run);
		par = (par_info_ptr) UseMemoryRecord(pg->par_formats, (long)run->style_item, 0, TRUE);
		offset_h += par->table.cell_h_extra;
		UnuseMemory(pg->par_formats);
		UnuseMemory(sub_ptr->par_style_run);
	}

	pgFillBlock(&initial_bounds, sizeof(rectangle), 0);

	starts = (point_start_ptr) UseMemory(block->lines);

	while (starts->flags != TERMINATOR_BITS) {
		
		pgTrueUnionRect(&starts->bounds, &initial_bounds, &initial_bounds);
		++starts;
	}
	
	UnuseMemory(block->lines);

	if (flags & (ANY_VERTICAL_FLAGS | ANY_NTH_ALIGN)) {
		
		if ((baseline_diff = draw_position->ascent - sub_ptr->ascent) > 0)
			offset_v += baseline_diff;
	}
	else {

		baseline_diff = draw_position->descent - block->end_start.baseline;
		offset_v = bounds->bot_right.v - initial_bounds.bot_right.v - baseline_diff;
	}

	if (sub_ptr->t_length == 0)
		offset_h += (sub_ptr->empty_width / 2);

	if (alignment_flags = sub_ptr->alignment_flags) {
		pg_subref					adjacent_sub;
		paige_sub_ptr				adjacent_ptr;
		
		if (alignment_flags & (CELL_ALIGN_RIGHT | CELL_ALIGN_DECIMAL)) {
			long					width_diff;
			
			width = sub_ptr->actual_text_width;
			width_diff = sub_ptr->minimum_width - width;
			
			if (width_diff > 0) {

				offset_h += width_diff;
				pgOffsetRect(bounds, width_diff, 0);
			}
		}

		if (alignment_flags & (SUBREF_ALIGNTOP | SUBREF_ALIGNTOPRIGHT | SUBREF_ALIGNMIDDLE)) {
			long		adjacent_height, this_height, amount_offset;

			if (adjacent_sub = get_adjacent_subref(draw_position->block, subref,
						(pg_boolean)((alignment_flags & (SUBREF_ALIGNTOP | SUBREF_ALIGNMIDDLE)) != 0))) {
				
				adjacent_ptr = (paige_sub_ptr) UseMemory(adjacent_sub);
				resulting_bounds = initial_bounds;
				pgOffsetRect(&resulting_bounds, offset_h, offset_v);

				this_height = resulting_bounds.bot_right.v - resulting_bounds.top_left.v;
				adjacent_height = adjacent_ptr->subset_bounds.bot_right.v - adjacent_ptr->subset_bounds.top_left.v;
				
				amount_offset = adjacent_height - this_height;
				
				if (alignment_flags & SUBREF_ALIGNMIDDLE)
					amount_offset /= 2;
					
				offset_v -= amount_offset;
	
				UnuseMemory(adjacent_sub);
			}
		}
	}
	
	offset_v -= sub_ptr->tighten_ascent;
	offset_v += sub_ptr->tighten_descent;

	if (offset_h || offset_v || pgEmptyRect(&sub_ptr->subset_bounds)) {
		
		pgOffsetRect(&block->bounds, offset_h, offset_v);
		pgOffsetRect(&block->end_start.bounds, offset_h, offset_v);

		starts = (point_start_ptr) UseMemory(block->lines);

		while (starts->flags != TERMINATOR_BITS) {
			
			pgOffsetRect(&starts->bounds, offset_h, offset_v);
			++starts;
		}
		
		UnuseMemory(block->lines);
	}
	
	sub_ptr->subset_bounds.bot_right.h = sub_ptr->subset_bounds.top_left.h;
	sub_ptr->subset_bounds.top_left.v = bounds->top_left.v;
	UnuseMemory(sub_ptr->t_blocks);
	UnuseMemory(subref);
}


/* get_adjacent_subref returns the previous or next subref to current, if any. Note that we only
test the currently nested level. */

static pg_subref get_adjacent_subref (text_block_ptr block, pg_subref current_ref, pg_boolean previous)
{
	register short		num_subs, index;
	pg_subref_ptr		subref_list;
	pg_subref			result;
	
	result = MEM_NULL;
	num_subs = (short)GetMemorySize(block->subref_list);
	subref_list = (pg_subref_ptr) UseMemory(block->subref_list);
	
	for (index = 0; index < num_subs; ++index)
		if (subref_list[index] == current_ref)
			break;
		
	if (index < num_subs) {
		
		if (previous) {
			
			if (index > 0)
				result = subref_list[index - 1];
		}
		else {
			
			if ((index + 1) < num_subs)
				result = subref_list[index + 1];
		}
	}

	UnuseMemory(block->subref_list);

	return		result;
}


/* arrow_keys returns TRUE if the_key is an arrow key. */

static pg_boolean arrow_keys (pg_globals_ptr globals, pg_byte_ptr the_key)
{
	register pg_globals_ptr			globals_ptr;
	register pg_short_t				compare_key;
	
	globals_ptr = globals;
	compare_key = (pg_short_t)*the_key;

	return ( (compare_key == globals_ptr->left_arrow_char)
			|| (compare_key == globals_ptr->right_arrow_char)
			|| (compare_key == globals_ptr->up_arrow_char)
			|| (compare_key == globals_ptr->down_arrow_char));
}



/* initialize_sub_record allocates all the structs for a subref */

static void initialize_sub_record (pgm_globals_ptr mem_globals, paige_sub_ptr sub_ptr)
{
	text_block_ptr			block;
	point_start_ptr			starts;
	style_run_ptr			first_run;
	pg_hyperlink_ptr		first_link;

	sub_ptr->width_table.positions = MemoryAlloc(mem_globals, sizeof(long), 0, 4);
	sub_ptr->width_table.types = MemoryAlloc(mem_globals, sizeof(short), 0, 4);
	sub_ptr->width_table.offset = -1;
	sub_ptr->width_table.mem_id = mem_globals->current_id;
	sub_ptr->width_table.used_ctr = 0;

	sub_ptr->t_blocks = MemoryAllocClear(mem_globals, sizeof(text_block), 1, 0);
	block = (text_block_ptr) UseMemory(sub_ptr->t_blocks);

	block->text = MemoryAlloc(mem_globals, sizeof(pg_byte), 0, 16);
	block->lines = MemoryAllocClear(mem_globals, sizeof(point_start), 2, 1);
	SetMemoryPurge(block->text, TEXT_PURGE_STATUS, FALSE);
	SetMemoryPurge(block->text, LINE_PURGE_STATUS, FALSE);
	block->subref_list = MemoryAlloc(mem_globals, sizeof(memory_ref), 0, 8);

	starts = (point_start_ptr) UseMemory(block->lines);
	starts->flags = NEW_LINE_BIT | LINE_BREAK_BIT;
	++starts;
	starts->flags = TERMINATOR_BITS;

	UnuseMemory(block->lines);

	block->flags = (NEEDS_CALC | NEEDS_PARNUMS);

	UnuseMemory(sub_ptr->t_blocks);

	sub_ptr->t_style_run = MemoryAllocClear(mem_globals, sizeof(style_run), 2, 2);
	sub_ptr->par_style_run = MemoryAllocClear(mem_globals, sizeof(style_run), 2, 2);
	sub_ptr->hyperlinks = MemoryAllocClear(mem_globals, sizeof(pg_hyperlink), 1, 0);
	sub_ptr->target_hyperlinks = MemoryAllocClear(mem_globals, sizeof(pg_hyperlink), 1, 0);

	first_run = (style_run_ptr) UseMemory(sub_ptr->t_style_run);
	first_run[1].offset = ZERO_TEXT_PAD;
	UnuseMemory(sub_ptr->t_style_run);

	first_run = (style_run_ptr) UseMemory(sub_ptr->par_style_run);
	first_run[1].offset = ZERO_TEXT_PAD;
	UnuseMemory(sub_ptr->par_style_run);

	first_link = (pg_hyperlink_ptr) UseMemory(sub_ptr->hyperlinks);
	first_link->applied_range.begin = first_link->applied_range.end = ZERO_TEXT_PAD;
	UnuseMemory(sub_ptr->hyperlinks);

	first_link = (pg_hyperlink_ptr) UseMemory(sub_ptr->target_hyperlinks);
	first_link->applied_range.begin = first_link->applied_range.end = ZERO_TEXT_PAD;
	UnuseMemory(sub_ptr->target_hyperlinks);

	sub_ptr->select = MemoryAllocClear(mem_globals, sizeof(t_select), 2, 0);

/* Create three possible character loc records:  */

	SetMemoryPurge(sub_ptr->t_blocks, NO_PURGING_STATUS, FALSE);
	SetMemoryPurge(sub_ptr->t_style_run, 0x00A0, FALSE);
	SetMemoryPurge(sub_ptr->par_style_run, 0x00A0, FALSE);
}


/* invalidate_hierarchy invalidates all levels above the current one. The result is the "top"
level. */

static pg_subref invalidate_hierarchy (paige_rec_ptr pg)
{
	paige_sub_ptr	sub_ptr, stack_ptr;
	pg_subref		active_subset;
	long			stack_size;

	stack_size = GetMemorySize(pg->subref_stack);
	active_subset = pg->active_subset;

	while (stack_size > 1) {
		
		--stack_size;
		stack_ptr = (paige_sub_ptr) UseMemoryRecord(pg->subref_stack, stack_size, 0, TRUE);
		active_subset = stack_ptr->active_subset;
		sub_ptr = (paige_sub_ptr) UseMemory(active_subset);
		sub_ptr += sub_ptr->alt_index;
		pgInvalSubBlocks(pg, sub_ptr);
		pgInvalSubBlocks(pg, stack_ptr);
		UnuseMemory(active_subset);
		UnuseMemory(pg->subref_stack);
	}
	
	return	active_subset;
}


/* decrement_subset_styles walks through the list of blocks and handles all subrefs that it
finds regarding the fixing up of deleted styles. */

static void decrement_subset_styles (block_ref t_blocks, pg_short_t bad_index,
			pg_short_t max_styles, pg_boolean do_par)
{
	text_block_ptr					block;
	pg_subref_ptr					subref_list;
	paige_sub_ptr					sub_ptr;
	long							num_blocks, num_subs, alt_qty;
	
	num_blocks = GetMemorySize(t_blocks);
	block = (text_block_ptr) UseMemory(t_blocks);
	
	while (num_blocks) {
		
		if ((num_subs = GetMemorySize(block->subref_list)) != 0) {
			
			subref_list = (pg_subref_ptr) UseMemory(block->subref_list);
			
			while (num_subs) {
				
				alt_qty = GetMemorySize(*subref_list);
				sub_ptr = (paige_sub_ptr) UseMemory(*subref_list);
				
				while (alt_qty) {
					
					if (sub_ptr->t_blocks) {
					
						if (do_par)
							pgDescrementRunItems(sub_ptr->par_style_run, bad_index, max_styles);
						else {

							if (sub_ptr->insert_style > bad_index)
								sub_ptr->insert_style -= 1;

							pgDescrementRunItems(sub_ptr->t_style_run, bad_index, max_styles);
						}

						decrement_subset_styles(sub_ptr->t_blocks, bad_index, max_styles, do_par);
					}

					++sub_ptr;
					--alt_qty;
				}

				UnuseMemory(*subref_list);
				
				++subref_list;
				--num_subs;
			}

			UnuseMemory(block->subref_list);
		}
		
		++block;
		--num_blocks;
	}
	
	UnuseMemory(t_blocks);
}



/* locate_subref_position locates the text position for subref somewhere within blocks. */

static long locate_subref_position (block_ref blocks, pg_subref subref)
{
	register short			text_position, text_size;
	text_block_ptr			block;
	pg_byte_ptr				text;
	pg_subref_ptr			subref_list;
	pg_subref				matching_ref;
	long					result, num_blocks, num_subs, sub_ctr;
	
	block = (text_block_ptr) UseMemory(blocks);
	num_blocks = GetMemorySize(blocks);
	matching_ref = MEM_NULL;

	while (num_blocks) {
	
		if ((num_subs = GetMemorySize(block->subref_list)) > 0) {
			
			subref_list = (pg_subref_ptr) UseMemory(block->subref_list);
			
			for (sub_ctr = 1; sub_ctr <= num_subs; ++subref_list, ++sub_ctr)
				if (*subref_list == subref) {
					
					matching_ref = *subref_list;
					break;
				}

			UnuseMemory(block->subref_list);
		}
		
		if (matching_ref != MEM_NULL)
			break;

		++block;
		--num_blocks;
	}

	result = 0;

	if (matching_ref) {
		
		text = (pg_byte_ptr) UseMemory(block->text);
		text_size = (short)(block->end - block->begin);
		result = block->begin;
		
		for (text_position = 0; text_position < text_size; ++text_position, ++result)
			if (text[text_position] == SUBREF_CHAR) {
				
				if ((sub_ctr -= 1) == 0)
					break;
			}

		UnuseMemory(block->text);
	}

	UnuseMemory(blocks);
	
	return		result;
}


/* locate_stylesheet_option gets called for pg_subref types. Its purpose is to return the
style index for the subref run based on stylesheet_option. If one is found, the original
stylesheet's used counter is incremented and the index returned. If none found, zero returns. */

static pg_short_t locate_stylesheet_option (paige_rec_ptr pg, short stylesheet_option)
{
	style_info_ptr			styles;
	long					style_qty, style_ctr;
 	pg_short_t				result;

	if (!stylesheet_option)
		return	0;

	result = 0;

	styles = (style_info_ptr) UseMemory(pg->t_formats);
	style_qty = GetMemorySize(pg->t_formats);
	
	for (style_ctr = 1; style_ctr <= style_qty; ++styles, ++style_ctr)
		if (styles->style_sheet_id == stylesheet_option) {
			
			styles->used_ctr += 1;
			result = (pg_short_t)style_ctr;
			break;
	}
	
	UnuseMemory(pg->t_formats);

	return	result;
}


/* get_align_extra returns the amount to add to each start to align the text line centered
inside total_width. Number of starts in line returned in *num_starts .*/

static long get_align_extra (point_start_ptr the_starts, long total_width,
		short PG_FAR *num_starts, long alignment_flags)
{
	register point_start_ptr		starts;
	rectangle						union_r;
	long							union_width;
	
	pgFillBlock(&union_r, sizeof(rectangle), 0);
	starts = the_starts;
	*num_starts = 0;

	while (starts->flags != TERMINATOR_BITS) {
		
		pgUnionRect(&starts->bounds, &union_r, &union_r);
		*num_starts += 1;
		
		if (starts->flags & LINE_BREAK_BIT)
			break;
		
		++starts;
	}
	
	union_width = union_r.bot_right.h - union_r.top_left.h;
	
	if (alignment_flags & HORIZONTAL_CENTER_FLAG)
		return	((total_width - union_width) / 2);
	
	return	(total_width - union_width);
}

/*pack_style_run packs a style run from a subref. */

static void pack_style_run (pack_walk_ptr walker, memory_ref run_ref)
{
	long				qty;
	
	qty = GetMemorySize(run_ref);
	pgPackNum(walker, long_data, qty);

	pgPackStyleRun (walker, run_ref, &qty);
}

/* pack_select_pairs packs the select_pair list from a subref. */

static void pack_select_pairs (pack_walk_ptr walker, memory_ref selection_ref)
{
	select_pair_ptr		selections;
	long				qty;
	
	qty = GetMemorySize(selection_ref);
	pgPackNum(walker, long_data, qty);
	
	for (selections = (select_pair_ptr) UseMemory(selection_ref); qty; ++selections, --qty)
		pgPackSelectPair(walker, selections);
	
	UnuseMemory(selection_ref);
}


/* unpack_style_run unpacks a style_run for a subref. */

static void unpack_style_run (pack_walk_ptr walker, memory_ref run_ref)
{
	long			qty;
	
	qty = pgUnpackNum(walker);

	pgUnpackStyleRun(walker, run_ref, qty);
}


/* unpack_select_pairs unpacks all select pairs for a subref. */

static void unpack_select_pairs (pack_walk_ptr walker, memory_ref selection_ref)
{
	select_pair_ptr		selections;
	long				qty;

	qty = pgUnpackNum(walker);
	SetMemorySize(selection_ref, qty);
	
	for (selections = (select_pair_ptr) UseMemory(selection_ref); qty; ++selections, --qty)
		pgUnpackSelectPair(walker, selections);
	
	UnuseMemory(selection_ref);
}


/* invalidate_subref invalidates the whole chain of refs begin with ref, on upwards. */

static void invalidate_subref (paige_rec_ptr pg, pg_subref ref)
{
	paige_sub_ptr			sub_ptr;
	text_block_ptr			block;
	pg_subref				last_ref, this_ref;
	block_ref				blockref;
	long					home_pos, num_blocks;

	last_ref = ref;

	while (last_ref) {
		
		this_ref = last_ref;
		sub_ptr = (paige_sub_ptr) UseMemory(this_ref);
		
		if (!(last_ref = sub_ptr->home_sub))
			home_pos = pgSubrefHomePosition(pg, this_ref);

		sub_ptr += sub_ptr->alt_index;
		pgInvalSubBlocks(pg, sub_ptr);
		UnuseMemory(this_ref);
	}

	if (pg->active_subset) {
		
		sub_ptr = (paige_sub_ptr) UseMemory(pg->subref_stack);
		blockref = sub_ptr->t_blocks;
		pgInvalSubBlocks(pg, sub_ptr);
		UnuseMemory(pg->subref_stack);
	}
	else {

		pgInvalCharLocs(pg);
		blockref = pg->t_blocks;
	}

	num_blocks = GetMemorySize(blockref);
	block = (text_block_ptr) UseMemory(blockref);
	
	while (num_blocks) {
		
		if (block->end > home_pos)
			break;
		
		if ((--num_blocks) == 0)
			break;
		
		++block;
	}
	
	block->flags |= NEEDS_CALC;

	UnuseMemory(blockref);
}


/* transfer_style_indexes assures the target (new) run matches the target paige record for
style_infos. */

static void transfer_style_indexes (paige_rec_ptr source_pg, paige_rec_ptr target_pg, memory_ref runref)
{
	register style_run_ptr		run;
	style_info_ptr				added_style, home_style;
	style_info					new_style, new_stylesheet;
	font_info					new_font;
	long						num_runs;
	short						stylesheet;

	num_runs = GetMemorySize(runref) - 1;
	run = (style_run_ptr) UseMemory(runref);
	
	while (num_runs) {
		
		GetMemoryRecord(source_pg->t_formats, (long)run->style_item, &new_style);
		GetMemoryRecord(source_pg->fonts, (long)new_style.font_index, &new_font);
		
		new_style.font_index = pgAddNewFont(target_pg, &new_font);

		if ((stylesheet = new_style.styles[super_impose_var]) != 0) {
			
			if (home_style = pgLocateStyleSheet(source_pg, stylesheet, NULL)) {
				
				new_stylesheet = *home_style;
				GetMemoryRecord(source_pg->fonts, (long)home_style->font_index, &new_font);
	
				UnuseMemory(source_pg->t_formats);
	
				new_stylesheet.style_sheet_id = 0;
				new_stylesheet.font_index = pgAddNewFont(target_pg, &new_font);
	
				if ((stylesheet = pgFindStyleSheet(target_pg->myself, &new_stylesheet, NULL)) == 0)
					stylesheet = pgNewStyle(target_pg->myself, &new_stylesheet, &new_font);
				
				new_style.styles[super_impose_var] = stylesheet;
			}
		}

		run->style_item = pgAddStyleInfo(target_pg, source_pg, internal_clone_reason, &new_style);
		
		added_style = (style_info_ptr) UseMemoryRecord(target_pg->t_formats, (long)run->style_item, 0, TRUE);
		added_style->used_ctr += 1;
		UnuseMemory(target_pg->t_formats);
		
		++run;
		--num_runs;
	}
	
	UnuseMemory(runref);
}

/* copy_width_table copies the width table from or to globals and the subref. If copy_to_subref
is TRUE then we go from globals->width_tables[WIDTH_QTY] to the subref. */

static void copy_width_table (paige_rec_ptr pg, paige_sub_ptr sub_ptr, pg_boolean copy_to_subref)
{
	if (copy_to_subref)
		pgBlockMove(&pg->globals->width_tables[WIDTH_QTY], &sub_ptr->width_table, sizeof(char_widths));
	else
		pgBlockMove(&sub_ptr->width_table, &pg->globals->width_tables[WIDTH_QTY], sizeof(char_widths));
}

static void invalidate_selections (paige_sub_ptr sub_ptr)
{
	t_select_ptr		selections;
	long				num_selects;
	
	selections = (t_select_ptr) UseMemory(sub_ptr->select);
	num_selects = GetMemorySize(sub_ptr->select);
	
	while (num_selects) {
		
		selections->flags |= SELECTION_DIRTY;
		++selections;
		--num_selects;
	}
	
	UnuseMemory(sub_ptr->select);
}

/* clear_hilite gets rid of any highlighting. */

static void clear_hilite (paige_rec_ptr pg)
{
	t_select_ptr		select;
	long				current_select;

	pgTurnOffHighlight(pg, FALSE);
	select = (t_select_ptr) UseMemory(pg->select);
	current_select = select->offset;
	UnuseMemory(pg->select);
	pgSetSelection(pg->myself, current_select, current_select, 0, FALSE);
}

