/* This file handles "mail merge" features. If you don't use this stuff it
can be omitted from your app.  */

#include "Paige.h"

#ifdef MAC_PLATFORM
#pragma segment pgmerge
#endif

#include "defprocs.h"
#include "machine.h"
#include "pgBasics.h"
#include "pgDefStl.h"
#include "pgText.h"
#include "pgSelect.h"
#include "pgUtils.h"
#include "pgEdit.h"


#define CLR_PG_MERGE		(~MERGE_MODE_BIT)			/* Clears merge mode bit in pg */
#define CLR_STYLE_MERGE		(~STYLE_MERGED_BIT)		    /* Clears merge mode bit in style */


static pg_boolean walk_merge_styles (paige_rec_ptr pg, style_info_ptr matching_style,
		style_info_ptr mask, style_info_ptr AND_mask, text_ref merged_text,
		memory_ref merged_pairs, long ref_con);
static void restore_merge (paige_rec_ptr pg, short draw_mode);



/* pgMergeText performs the following:

(A) Walks through the whole style run and locates any styles that match with
matching_style/mask/AND_mask (see below).

(B) If a style matches up, the merge_proc is called to swap in different text.

(C) If text is swapped in, previous text is deleted. If "merge_mode" has not
been previously set, the old text is preserved (saved) in a form of a "push"
if push_text = TRUE. But, text can only be pushed once.

Hence, pgRestoreMerge can be called to restore the original text. Note that
multiple calls can be made to swap out text, saving the original text (which is
typically placeholders for merge fields), to finally restore the original with
pgUnsetMergeMode.

(D) If draw_mode is non-zero the text is re-drawn.

(E) Every style that has been merged is flagged with STYLE_MERGED_BIT.

(F) The pg_ref is set to "merge_mode" (which is mostly for the app's reference;
Paige doesn't actually do anything different with merge_mode). The merge_mode is
NOT set, however, if nothing merged.

(G) The function returns TRUE if anything merged. 

Matches are checked as follows:  For each style that is examined, only the
non-zero fields in mask are compared, in which case the corresponding field in
matching_style is compared to the target style.  However, the target style field
is first AND'd with the corresponding value in AND_mask before compared.

If matching_style is NULL, all styles are used.  If AND_mask is null, nothing
is AND'd. If matching_style is non-NULL but mask is NULL, the whole style is
compared, in which case AND_mask is AND'd for every field if it exists as well.

The ref_con value gets passed to the merge_proc for the app's reference. This
same ref_con is given to the merge_proc.  */


PG_PASCAL (pg_boolean) pgMergeText (pg_ref pg, const style_info_ptr matching_style,
		const style_info_ptr mask, const style_info_ptr AND_mask, long ref_con, short draw_mode)
{
	paige_rec_ptr				pg_rec;
	text_ref					merged_text;
	memory_ref					merged_pairs;
	long						old_change_ctr;
	pg_boolean					merge_result;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	merge_result = FALSE;
	old_change_ctr = pg_rec->change_ctr;

	if (pg_rec->merge_save) {
		
		merged_pairs = (memory_ref) pgGetExtraStruct(pg_rec->merge_save, MERGE_PAIR_ID);
		restore_merge(pg_rec, draw_none);
	}
	else {
		
		merged_pairs = MemoryAlloc(pg_rec->globals->mem_globals,
				sizeof(select_pair), 0, 16);
		
		if (merge_result = walk_merge_styles(pg_rec, matching_style, mask, AND_mask,
				MEM_NULL, merged_pairs, ref_con)) {

			pgSetSelectionList(pg, merged_pairs, 0, FALSE);
			pg_rec->merge_save = pgCopy(pg, NULL);
			pgSetExtraStruct(pg_rec->merge_save, (void PG_FAR *) merged_pairs, MERGE_PAIR_ID);
		}
		else
			DisposeMemory(merged_pairs);
	}

	if (pg_rec->merge_save) {
		
		pgSetSelection(pg, 0, 0, 0, FALSE);
		merged_text = MemoryAlloc(pg_rec->globals->mem_globals, sizeof(pg_char), 0, 64);
		merge_result = walk_merge_styles(pg_rec, matching_style, mask, AND_mask,
				merged_text, merged_pairs, ref_con);

		DisposeMemory(merged_text);
	}

	if (merge_result) {
		
		++old_change_ctr;

		if (draw_mode) {
			short		use_draw_mode;
			
			if ((use_draw_mode = draw_mode) == best_way) {
				
				if (pg_rec->flags & PRINT_MODE_BIT)
					use_draw_mode = direct_or;
				else
					use_draw_mode = bits_copy;
			}
			
			pgUpdateText(pg_rec, NULL, 0, pg_rec->t_length, MEM_NULL, NULL, use_draw_mode, TRUE);
		}
	}

	pg_rec->change_ctr = old_change_ctr;

	UnuseMemory(pg);
	
	return	merge_result;
}



/* pgRestoreMerge reverses the effect of pgSetMergeMode -- if any data was
merged, an effective "undo" is performed if rever_original is TRUE.  In either
case, the "merge undo" data is disposed and MERGE_MODE_BIT is cleared.   */

PG_PASCAL (void) pgRestoreMerge (pg_ref pg, pg_boolean revert_original, short draw_mode)
{
	paige_rec_ptr				pg_rec;
	long						old_change_ctr;

	pg_rec = (paige_rec_ptr) UseMemory(pg);
	old_change_ctr = pg_rec->change_ctr;

	if (pg_rec->merge_save) {
		
		--old_change_ctr;

		if (revert_original)
			restore_merge(pg_rec, draw_mode);

		DisposeMemory((memory_ref) pgGetExtraStruct(pg_rec->merge_save, MERGE_PAIR_ID));
		pgDispose(pg_rec->merge_save);
		pg_rec->merge_save = MEM_NULL;
		pg_rec->flags &= CLR_PG_MERGE;
	}

	pg_rec->change_ctr = old_change_ctr;

	UnuseMemory(pg);
}




/******************************* Local Functions ********************************/


/* This gets called twice by PgMergeText, once to build the offset pair(s) that
should get replaced and once to actually replace the text. It knows the difference
between these by merged_text and merged_pairs parameters: if merged_text is NULL
then merged_pairs will be filled in with all merged offsets. If merged_tex is
non-NULL, the text will literally be merged and merged_pairs will get filled in
with all offsets of the new merge (so it can be deleted later).  */

static pg_boolean walk_merge_styles (paige_rec_ptr pg, style_info_ptr matching_style,
		style_info_ptr mask, style_info_ptr AND_mask, text_ref merged_text,
		memory_ref merged_pairs, long ref_con)
{
	register pg_char_ptr		text_ptr;
	register select_pair_ptr	appended_pairs;
	style_walk					walker;
	select_pair					merge_range;
	text_ref					used_text_ref;
	size_t						merge_length, max_length;
	short						style_deleted;
	pg_boolean					merge_result;

	SetMemorySize(merged_pairs, 0);

	pgPrepareStyleWalk(pg, 0, &walker, FALSE);
	merge_result = FALSE;

	for (;;) {
		
		style_deleted = FALSE;

		if (pgStyleMatchesCriteria(walker.cur_style, matching_style,
				mask, AND_mask, SIGNIFICANT_STYLE_SIZE)) {

			merge_length = walker.next_style_run->offset;
			if (merge_length > pg->t_length)
				merge_length = pg->t_length;
			
			merge_length -= walker.current_offset;

			merge_range.begin = walker.current_offset;
			merge_range.end = merge_range.begin + merge_length;

			if (!(text_ptr = pgTextFromOffset(pg, walker.current_offset,
					&used_text_ref, &max_length)))
				break;
			
			if (max_length < merge_length)
				merge_length = max_length;
			
			if (walker.cur_style->procs.merge(pg, walker.cur_style,
					text_ptr, (pg_short_t)merge_length, merged_text, ref_con)) {
				
				merge_result = TRUE;
				
				if (merged_text) {  /* Meant for merging, not select pairs */
				
					walker.cur_style->class_bits |= STYLE_MERGED_BIT;
					pg->flags |= MERGE_MODE_BIT;
	
					UnuseMemory(used_text_ref);
					pgPrepareStyleWalk(pg, 0, NULL, FALSE);
					pgDelete(pg->myself, &merge_range, draw_none);
					
					if (merge_length = GetMemorySize(merged_text)) {
						
						pgInsert(pg->myself, (const pg_char_ptr) UseMemory(merged_text), merge_length,
								merge_range.begin, data_insert_mode, 0, draw_none);
	
						UnuseMemory(merged_text);
					}
					else
						style_deleted = TRUE;

					merge_range.end = merge_range.begin + merge_length;

					pgPrepareStyleWalk(pg, merge_range.begin, &walker, FALSE);
				}
				else
					UnuseMemory(used_text_ref);

				appended_pairs = (select_pair_ptr) AppendMemory(merged_pairs, 1, FALSE);
				*appended_pairs = merge_range;
				UnuseMemory(merged_pairs);
			}
			else
				UnuseMemory(used_text_ref);
		}
		
		if (!style_deleted)
			if (!pgWalkNextStyle(&walker))
				break;
	}
	
	pgPrepareStyleWalk(pg, 0, NULL, FALSE);

	return	merge_result;
}


/* This function is called to replace all text with the original merge items. */
static void restore_merge (paige_rec_ptr pg, short draw_mode)
{
	register style_info_ptr		styles_to_clear;
	register pg_short_t			style_qty;
	memory_ref					select_ref;
	select_pair					paste_position;

	select_ref = (memory_ref) pgGetExtraStruct(pg->merge_save, MERGE_PAIR_ID);
	pgSetSelectionList(pg->myself, select_ref, 0, FALSE);
	pgDelete(pg->myself, NULL, draw_none);
	
	GetMemoryRecord(select_ref, 0, &paste_position);

	for (styles_to_clear = (style_info_ptr) UseMemory(pg->t_formats), 
			style_qty = (pg_short_t)GetMemorySize(pg->t_formats); style_qty;
			++styles_to_clear, --style_qty)
		styles_to_clear->class_bits &= CLR_STYLE_MERGE;
	
	UnuseMemory(pg->t_formats);

	pgPaste(pg->myself, pg->merge_save, paste_position.begin, FALSE, draw_mode);
}
