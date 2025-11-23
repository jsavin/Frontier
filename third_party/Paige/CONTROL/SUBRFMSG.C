/* This source file handles all the PAIGE messages dealing with embed refs and graphics.
It handles all "PG_EMBEDMESSAGES" messages. */


#include "Paige.h"
#include "pgTraps.h"

#include "machine.h"
#include "pgUtils.h"
#include "defprocs.h"
#include "pgCtlUtl.h"
#include "pgCntl.h"
#include "pgDefStl.h"
#include "pgEdit.h"
#include "pgEmbed.h"
#include "pgSubRef.h"

#ifdef DOING_EQ
#include "EQ.h"
#endif


static memory_ref build_subref_array (paige_rec_ptr pg, paige_control_ptr pg_stuff,
		pg_subref within_ref, pg_boolean want_text);
static pg_boolean is_subref_noneditable (pg_subref ref);
static long copy_subref_data (paige_control_ptr pg_stuff, pg_subref subref, paige_sub_ptr alt_ptr,
		PAIGEOBJECTSTRUCT PG_FAR *obj_data, pg_boolean copy_to_subref, pg_boolean set_focus_flag);
static pg_subref locate_previous_ref (paige_rec_ptr pg, long position);
static pg_subref locate_next_ref (paige_rec_ptr pg, paige_sub_ptr sub_ptr, long position);
static paige_sub_ptr adjust_substack_level (paige_sub_ptr current_ptr, memory_ref stack_ref);
static void set_subref_text (paige_rec_ptr pg, paige_sub_ptr sub_ptr, memory_ref textref);
static void invalidate_subref (paige_control_ptr pg_stuff, pg_subref subref);
static pg_short_t get_char (memory_ref blocks, long position, long max_text_size,
		text_block_ptr subref_block);


long pascal SubrefMessageProc (HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	paige_control_ptr			pg_stuff;			// One of these in each control
	paige_rec_ptr				pg_rec;
	memory_ref					pg_stuff_ref, subtext;
	pg_subref					subref;
	text_block_ptr				block;
	PAIGEOBJECTSTRUCT PG_FAR	*obj_data;  		// Struct for inserting an object
	paige_sub_ptr				sub_ptr;
	short						draw_mode, stylesheet_option;
	long						response, flags, stack_size;

	response = 0;
	pg_stuff = GetPGStuff(hWnd, &pg_stuff_ref);
	
	if (!pg_stuff)
		return	0;

    if (wParam)
    	draw_mode = pg_stuff->update_mode;
    else
       draw_mode = draw_none;

	switch (message) {
		
		case PG_INSERTSUBREF:
			subref = pgNewSubRef(&mem_globals);
			obj_data = (PAIGEOBJECTSTRUCT PG_FAR *)lParam;

			stylesheet_option = (short)obj_data->param1;
			flags = copy_subref_data(pg_stuff, subref, NULL, obj_data, TRUE, FALSE);
			pgInsertSubRef(pg_stuff->pg, subref, obj_data->position,
				(pg_boolean)((flags & PGOBJFLAG_SETFOCUS) != 0), stylesheet_option,
				(pg_char_ptr)obj_data->data, obj_data->dataModifier, draw_mode);
			obj_data->subref = (long)subref;
			SendChangeCommand(pg_stuff);
			break;

		case PG_GETSUBSETLINE:
			if ((subref = (pg_subref)lParam) == MEM_NULL)
				break;
						
			sub_ptr = UseMemory(subref);
			sub_ptr += sub_ptr->alt_index;
			
			block = UseMemory(sub_ptr->t_blocks);

			if (wParam == 0)
				response = block->end_start.bounds.bot_right.v - block->end_start.bounds.top_left.v;
			else {
			
			}
			
			UnuseMemory(sub_ptr->t_blocks);
			UnuseMemory(subref);
			break;

		case PG_SETALTERNATE:
			obj_data = (PAIGEOBJECTSTRUCT PG_FAR *)lParam;
			
			pgSetSubRefText(pg_stuff->pg, (pg_subref)obj_data->subref, obj_data->alternate,
					(pg_char_ptr)obj_data->embedPtr, (short)obj_data->param2,
					(short)obj_data->param1, draw_mode);
			break;

		case PG_GETALTERNATE:
			obj_data = (PAIGEOBJECTSTRUCT PG_FAR *)lParam;
				
			if (subtext = pgGetSubRefText(pg_stuff->pg, (pg_subref)obj_data->subref, obj_data->alternate)) {
					
				response = GetMemorySize(subtext);

				if (obj_data->embedPtr && response) {
					
					pgBlockMove(UseMemory(subtext), (void PG_FAR *)obj_data->embedPtr, response);
					UnuseMemory(subtext);
				}
			}

			break;

		case PG_INITOBJECTWALK:
			obj_data = (PAIGEOBJECTSTRUCT PG_FAR *)lParam;

			pg_rec = UseMemory(pg_stuff->pg);
			
			pgFillBlock(obj_data, sizeof(PAIGEOBJECTSTRUCT), 0);
			obj_data->walk_stack = (memory_ref)MemoryAllocClear(pg_rec->globals->mem_globals,
					sizeof(paige_sub_rec), 1, 4);
			obj_data->textRef = (memory_ref)MemoryAllocClear(pg_rec->globals->mem_globals, sizeof(pg_char), 0, 32);

			if (wParam)
				obj_data->walk_restore = (memory_ref) pgGetSubrefState(pg_rec, FALSE, TRUE);

			sub_ptr = UseMemory((memory_ref)obj_data->walk_stack);
			
			if (pg_rec->active_subset) {
				
				pgBlockMove(UseMemory(pg_rec->active_subset), sub_ptr, sizeof(paige_sub_rec));
				UnuseMemory(pg_rec->active_subset);
			}
			else
				pgCopySubfields(pg_rec, sub_ptr, TRUE);
			
			sub_ptr->home_position = 0;

			UnuseMemory((memory_ref)obj_data->walk_stack);
			UnuseMemory(pg_stuff->pg);
			
			// Falls through to PG_GETNEXTOBJECT. NO BREAK.
			
		case PG_GETNEXTOBJECT:
			obj_data = (PAIGEOBJECTSTRUCT PG_FAR *)lParam;
			pg_rec = UseMemory(pg_stuff->pg);
			stack_size = GetMemorySize((memory_ref)obj_data->walk_stack);
			sub_ptr = UseMemory((memory_ref)obj_data->walk_stack);
			sub_ptr += (stack_size - 1);
			sub_ptr = adjust_substack_level(sub_ptr, (memory_ref)obj_data->walk_stack);			

			if (sub_ptr->home_position < sub_ptr->t_length && GetMemorySize((memory_ref)obj_data->walk_stack) > 1) {
				long			old_home_position;

				old_home_position = sub_ptr->home_position;
				set_subref_text(pg_rec, sub_ptr, (memory_ref)obj_data->textRef);
				
				if (GetMemorySize((memory_ref)obj_data->textRef) > 0) {

					copy_subref_data(pg_stuff, MEM_NULL, sub_ptr, obj_data, FALSE, FALSE);
					response = TRUE;
					obj_data->nest_level = (short)GetMemorySize((memory_ref)obj_data->walk_stack);
					
					UnuseMemory((memory_ref)obj_data->walk_stack);
					sub_ptr = NULL;
				}
			}

			if (sub_ptr != NULL) {

				for (;;) {
					
					SetMemorySize((memory_ref)obj_data->textRef, 0);

					if ((subref = locate_next_ref(NULL, sub_ptr, sub_ptr->home_position)) != MEM_NULL) {
						
						copy_subref_data(pg_stuff, subref, NULL, obj_data, FALSE, FALSE);
						response = TRUE;
						obj_data->nest_level = (short)GetMemorySize((memory_ref)obj_data->walk_stack);

						sub_ptr->home_position = pgSubrefHomePosition(pg_rec, subref);
						sub_ptr->home_position += 1;
						sub_ptr = AppendMemory((memory_ref)obj_data->walk_stack, 1, TRUE);
						pgBlockMove(UseMemory(subref), sub_ptr, sizeof(paige_sub_rec));
						UnuseMemory(subref);
						
						sub_ptr->home_position = 0;
						set_subref_text(pg_rec, sub_ptr, (memory_ref)obj_data->textRef);
							
						break;
					}
					
					sub_ptr->home_position = sub_ptr->t_length;

					if (!(sub_ptr = adjust_substack_level(sub_ptr, (memory_ref)obj_data->walk_stack)))
						break;
				}

				if (sub_ptr != NULL) {
					
					sub_ptr->home_position += 1;
					UnuseMemory((memory_ref)obj_data->walk_stack);
				}
			}

			UnuseMemory(pg_stuff->pg);
			break;

		case PG_CLOSEOBJECTWALK:
			obj_data = (PAIGEOBJECTSTRUCT PG_FAR *)lParam;

			DisposeMemory(obj_data->walk_stack);
			DisposeMemory(obj_data->textRef);

			pg_rec = UseMemory(pg_stuff->pg);
			pgRestoreSubRefs(pg_rec, (memory_ref)obj_data->walk_restore);
			UnuseMemory(pg_stuff->pg);
			break;

		case PG_SETFOCUS:
		case PG_SETNESTEDFOCUS:
			pg_rec = UseMemory(pg_stuff->pg);
			obj_data = (PAIGEOBJECTSTRUCT PG_FAR *)lParam;
			
			if (message == PG_SETFOCUS)
				pgUseSubRef(pg_rec, (pg_subref)obj_data->subref, 0, NULL, NULL);
			else
				response = pgSetNestedFocus(pg_rec, (pg_subref)obj_data->subref, FALSE);

			UnuseMemory(pg_stuff->pg);
			break;

		case PG_UNSETFOCUS:
		case PG_UNSETNESTEDFOCUS:
			pg_rec = UseMemory(pg_stuff->pg);
			
			if (message == PG_UNSETFOCUS)
				pgUnuseSubRef(pg_rec);
			else
				pgRestoreSubRefs(pg_rec, lParam);

			UnuseMemory(pg_stuff->pg);
			break;
		
		case PG_GETSUBREFARRAY:
			pg_rec = UseMemory(pg_stuff->pg);
			response = (long)build_subref_array(pg_rec, pg_stuff, (pg_subref)lParam, (pg_boolean)wParam);
			UnuseMemory(pg_stuff->pg);
			break;
		
		case PG_GETSUBREFINFO:
			// wParam = nth subref in current level
			pg_rec = UseMemory(pg_stuff->pg);
			if ((subref = pgGetNthSubref(pg_rec, wParam)) != MEM_NULL)
				copy_subref_data(pg_stuff, subref, NULL, (PAIGEOBJECTSTRUCT PG_FAR *)lParam, FALSE,
						(pg_boolean)(pg_rec->subset_focus == subref));
			UnuseMemory(pg_stuff->pg);
			
			response = (long)subref;
			break;

		case PG_GETSELSUBREFINFO:
			// Returns info for nearest subref (or selected one)
			pg_rec = UseMemory(pg_stuff->pg);

			if (subref = pg_rec->active_subset)
				copy_subref_data(pg_stuff, subref, NULL, (PAIGEOBJECTSTRUCT PG_FAR *)lParam, FALSE,
						(pg_boolean)(pg_rec->subset_focus == subref));

			UnuseMemory(pg_stuff->pg);
			response = (long)subref;
			break;

		case PG_SETSELSUBREFINFO:
			obj_data = (PAIGEOBJECTSTRUCT PG_FAR *)lParam;
			
			if (obj_data->subref) {
			
				copy_subref_data(pg_stuff, (pg_subref)obj_data->subref, NULL, obj_data, TRUE, FALSE);
				invalidate_subref(pg_stuff, obj_data->subref);
			}
			
			if (draw_mode)
				RepaintControl(hWnd, pg_stuff, FALSE);

			break;

		default:
			response = DefWindowProc(hWnd, message, wParam, lParam);
			break;
	}

	UnuseMemory(pg_stuff_ref);

	return	response;
}



/* HandleSubrefArrows handles subref-specific arrow keys. If something was handled here
then TRUE is returned, otherwise we did not do anything special. */

int pascal HandleSubrefArrows (paige_control_ptr pg_stuff, pg_char the_char)
{
	pg_short_t			the_key, next_char;
	paige_rec_ptr		pg;
	memory_ref			blocks;
	text_block			subref_block;
	pg_subref			original_subref, current_subref, new_subref;
	paige_sub_ptr		sub_ptr;
	long				current_text_size, current_position, subref_index;
	int					subref_changed = FALSE;

	if (pgNumSelections(pg_stuff->pg) > 0 && pgTextSize(pg_stuff->pg) == 0)
		return	FALSE;
	
	the_key = (pg_short_t)the_char;
	
	if (the_key != paige_globals.left_arrow_char
		&& the_key != paige_globals.right_arrow_char
		&& the_key != paige_globals.up_arrow_char
		&& the_key != paige_globals.down_arrow_char)
		return	FALSE;

	new_subref = MEM_NULL;
	pg = UseMemory(pg_stuff->pg);
	original_subref = current_subref = pg->active_subset;
	current_position = pgCurrentInsertion(pg);

	for (;;) {

		if (current_subref) {
			
			sub_ptr = UseMemory(current_subref);
			sub_ptr += sub_ptr->alt_index;
			blocks = sub_ptr->t_blocks;
			current_text_size = sub_ptr->t_length;
			UnuseMemory(current_subref);
		}
		else {
		
			blocks = pg->t_blocks;
			current_text_size = pg->t_length;
		}

		if (the_key == paige_globals.left_arrow_char || the_key == paige_globals.up_arrow_char) {

			next_char = get_char(blocks, current_position - 1, current_text_size, &subref_block);
			
			if (next_char == 0) {
				
				the_key = paige_globals.left_arrow_char;  // Force this to avoid false loop

				if (!current_subref) // If not in a subref, we are at end of doc
					break;
				
				current_position = pgSubrefHomePosition(pg, current_subref);
				sub_ptr = UseMemory(current_subref);
				new_subref = sub_ptr->home_sub;
				UnuseMemory(current_subref);
				subref_changed = TRUE;
			}
			else
			if (next_char == SUBREF_CHAR) {
				
				the_key = paige_globals.left_arrow_char;  // Force this to avoid false loop

				subref_index = pgFindSubrefPosition(&subref_block, (pg_short_t)(current_position - subref_block.begin - 1));
				GetMemoryRecord(subref_block.subref_list, subref_index, &new_subref);
				sub_ptr = UseMemory(new_subref);
				sub_ptr += sub_ptr->alt_index;
				current_position = sub_ptr->t_length;
				UnuseMemory(new_subref);
				subref_changed = TRUE;
			}
			else {
			
				current_position -= 1;
				
				if (!current_subref)
					the_key = paige_globals.left_arrow_char;
			}
		}
		else
		if (the_key == paige_globals.right_arrow_char || the_key == paige_globals.down_arrow_char) {
			
			next_char = get_char(blocks, current_position, current_text_size, &subref_block);
			
			if (next_char == 0) {
				
				the_key = paige_globals.right_arrow_char;  // Force this to prevent false loop
				
				if (!current_subref) // If not in a subref, we are at end of doc
					break;
				
				current_position = pgSubrefHomePosition(pg, current_subref) + 1;
				sub_ptr = UseMemory(current_subref);
				new_subref = sub_ptr->home_sub;
				UnuseMemory(current_subref);
				subref_changed = TRUE;
			}
			else
			if (next_char == SUBREF_CHAR) {
				
				the_key = paige_globals.right_arrow_char;

				subref_index = pgFindSubrefPosition(&subref_block, (pg_short_t)(current_position - subref_block.begin));
				GetMemoryRecord(subref_block.subref_list, subref_index, &new_subref);
				current_position = 0;
				subref_changed = TRUE;
			}
			else {
			
				current_position += 1;

				if (!current_subref)
					the_key = paige_globals.right_arrow_char;
			}
		}
		
		if (the_key != paige_globals.up_arrow_char && the_key != paige_globals.down_arrow_char) {
		
			if (!new_subref)
				break;
			
			if (!is_subref_noneditable(new_subref))
				break;
			
			current_subref = new_subref;
		}
	}
	
	if (subref_changed) {
		memory_ref			state_ref;

		pg->procs.set_device(pg, set_pg_device, &pg->port, NULL);
		pgClipGrafDevice(pg, clip_standard_verb, MEM_NULL);
		pg->procs.cursor_proc(pg, UseMemory(pg->select), hide_cursor);
		UnuseMemory(pg->select);
		pg->procs.set_device(pg, unset_pg_device, &pg->port, NULL);	

		if (state_ref = pgGetSubrefState(pg, FALSE, TRUE))
			DisposeMemory(state_ref);
		
		if (state_ref = pgSetNestedFocus(pg, new_subref, TRUE))
			DisposeMemory(state_ref);
	
	#ifdef WINDOWS_PLATFORM
		pgSetSelection(pg->myself, current_position, current_position, 0, TRUE);
	#else
		pgSetSelection(pg->myself, current_position, current_position, 0, FALSE);
	#endif
	}

	UnuseMemory(pg_stuff->pg);

	return		subref_changed;
}


/* SubsetPostDraw is where we call the app for drawing ornaments in a subref. */

PG_PASCAL (void) SubsetPostDraw (paige_rec_ptr pg, draw_points_ptr draw_position,
		rectangle_ptr draw_bounds, co_ordinate_ptr offset_extra, style_walk_ptr walker)
{
	PAIGEOBJECTSTRUCT			obj, PG_FAR *obj_ptr;
	style_info_ptr				style;
	paige_control_ptr			pg_stuff;
#ifdef WINDOWS_PLATFORM
	HPEN						old_color, pen_color;
	HBRUSH						old_brush, brush_color;
	long						colorref;
#endif
	
	style = walker->cur_style;
	pg_stuff = UseMemory(pg->custom_control);
	pgFillBlock(&obj, sizeof(PAIGEOBJECTSTRUCT), 0);
	copy_subref_data(pg_stuff, pg->active_subset, NULL, &obj, FALSE, (pg_boolean)(pg->subset_focus == pg->active_subset));
	obj.param1 = (long)draw_bounds;
	obj.hDC = (HDC)pg->globals->current_port->machine_ref;
	obj_ptr = &obj;

// Set the current pen color:

#ifdef MAC_PLATFORM
	RGBForeColor((RGBColor *)&style->fg_color);
#endif

#ifdef WINDOWS_PLATFORM
	pgColorToOS(&style->fg_color, &colorref);
	pen_color = CreatePen(PS_SOLID, 1, colorref);
	old_color = SelectObject(obj.hDC, pen_color);
	brush_color = CreateSolidBrush(colorref);
	old_brush = SelectObject(obj.hDC, brush_color);
#endif

#ifdef DOING_EQ
	eqDrawTemplate(obj_ptr, (PAIGERECT PG_FAR *)draw_bounds);
#else
	SendNotification(pg_stuff, PGN_SUBSETDRAW, 0, obj_ptr);
#endif
	
#ifdef WINDOWS_PLATFORM
	SelectObject(obj.hDC, old_color);
	DeleteObject(pen_color);
	SelectObject(obj.hDC, old_brush);
	DeleteObject(brush_color);
#endif
	UnuseMemory(pg->custom_control);
}


/************************************* Local Functions ****************************************/

/* get_char returns the char at "position" within the text blocks.
If no such char the function returns zero. If subref_block is non-null it is filled with
the text_block that contains the char. */

static pg_short_t get_char (memory_ref blocks, long position, long max_text_size,
		text_block_ptr subref_block)
{
	text_block_ptr		block;
	pg_char_ptr			text;
	long				num_blocks;
	pg_short_t			result;

	if (position == -1 || position >= max_text_size)
		return	0;
	
	num_blocks = GetMemorySize(blocks);
	block = UseMemory(blocks);
	
	while (num_blocks > 2) {
		
		if (position < block->end)
			break;
		
		++block;
		--num_blocks;
	}

	text = UseMemory(block->text);
	result = (pg_short_t)text[position - block->begin];
	UnuseMemory(block->text);
	
	if (subref_block)
		*subref_block = *block;

	UnuseMemory(blocks);

	return		result;
}


/* adjust_substack_level checks the current text position for the "get-next-object" walker,
and if that position is sitting at the end of text then we "pop" the nested level to
return to the location we were at, then test again. */

static paige_sub_ptr adjust_substack_level (paige_sub_ptr current_ptr, memory_ref stack_ref)
{
	paige_sub_ptr	sub_ptr;
	long			stack_size;

	stack_size = GetMemorySize(stack_ref);
	sub_ptr = current_ptr;

	while (sub_ptr->home_position >= sub_ptr->t_length) {
		
		// "Pop" current nested level, if any.
		
		UnuseMemory(stack_ref);
		
		if ((stack_size -= 1) == 0)
			return	NULL;

		SetMemorySize(stack_ref, stack_size);
		sub_ptr = UseMemory(stack_ref);
		sub_ptr += (stack_size - 1);
	}

	return		sub_ptr;
}


/* set_subref_text fills in textref with the text beginning at sub_ptr->home_position up to the next
embed_ref or end of text, whichever comes first. The home_position is updated accordingly. */

static void set_subref_text (paige_rec_ptr pg, paige_sub_ptr sub_ptr, memory_ref textref)
{
	pg_subref					subref;
	long						end_offset, text_size;

	end_offset = sub_ptr->t_length;

	if ((subref = locate_next_ref(NULL, sub_ptr, sub_ptr->home_position)) != MEM_NULL) {
		long			home_offset;
		
		home_offset = pgSubrefHomePosition(pg, subref);
		
		if (home_offset < end_offset)
			end_offset = home_offset;
	}

	text_size = end_offset - sub_ptr->home_position;
	SetMemorySize(textref, text_size);
	
	if (text_size) {
		text_block_ptr		block;
		pg_char_ptr			source_text, target_text;
		
		target_text = UseMemory(textref);
		block = UseMemory(sub_ptr->t_blocks);
		source_text = UseMemoryRecord(block->text, sub_ptr->home_position, USE_ALL_RECS, TRUE);
		pgBlockMove(source_text, target_text, text_size);
		UnuseMemory(block->text);
		UnuseMemory(sub_ptr->t_blocks);
		UnuseMemory(textref);
	}
	
	sub_ptr->home_position = end_offset;
}


/* is_subref_noneditable returns TRUE if the ref is a non-editable situation (which will always be
false if MEM_NULL. */

static pg_boolean is_subref_noneditable (pg_subref ref)
{
	paige_sub_ptr			sub_ptr;
	long					flags;
	
	if (!ref)
		return	FALSE;

	sub_ptr = UseMemory(ref);
	flags = sub_ptr->subref_flags;
	UnuseMemory(ref);
	
	return	(pg_boolean)((flags & SUBREF_NOT_EDITABLE) != 0);
}


/* build_subref_array builds a memory_ref whose contents are an array of editable subrefs inside
within_ref. If within_ref is MEM_NULL then we check the top level. */

static memory_ref build_subref_array (paige_rec_ptr pg, paige_control_ptr pg_stuff,
		pg_subref within_ref, pg_boolean want_text)
{
	memory_ref					result;
	paige_sub_ptr				sub_ptr;
	PAIGEOBJECTSTRUCT PG_FAR	*obj;
	text_block_ptr				block;
	pg_subref_ptr				subref_list;
	memory_ref					block_ref;
	long						num_subs;

	if (!within_ref)
		block_ref = pg->t_blocks;
	else {
	
		sub_ptr = UseMemory(within_ref);
		block_ref = sub_ptr->t_blocks;
		UnuseMemory(within_ref);
	}

	block = UseMemory(block_ref);
	num_subs = GetMemorySize(block->subref_list);
	
	result = MemoryAllocClear(&mem_globals, sizeof(PAIGEOBJECTSTRUCT), num_subs, 2);
	obj = UseMemory(result);
	subref_list = UseMemory(block->subref_list);

	while (num_subs) {
		
		copy_subref_data(pg_stuff, *subref_list, NULL, obj, FALSE, (pg_boolean)(pg->subset_focus == *subref_list));
		
		++subref_list;
		++obj;
		--num_subs;
	}

	UnuseMemory(block->subref_list);
	UnuseMemory(result);
	UnuseMemory(block_ref);

	return	result;
}



/* copy_subref_data copies the data from a subref pointer to user struct, or vice versa.
If copy_to_subref, data copied from user struct to subref. The subref_flags member is returned.
The subref pointer is used if alt_ptr is non-NULL. */

static long copy_subref_data (paige_control_ptr pg_stuff, pg_subref subref, paige_sub_ptr alt_ptr,
		PAIGEOBJECTSTRUCT PG_FAR *obj_data, pg_boolean copy_to_subref, pg_boolean set_focus_flag)
{
	paige_sub_ptr			sub_ptr;
	long					flags;

	if (alt_ptr)
		sub_ptr = alt_ptr;
	else
		sub_ptr = UseMemory(subref);
	
	sub_ptr += (sub_ptr->alt_index);

	if (copy_to_subref) {
		
		sub_ptr->empty_width = obj_data->emptyWidth;
		sub_ptr->minimum_width = obj_data->minWidth;
		sub_ptr->modifier = obj_data->dataModifier;
		sub_ptr->subref_flags = flags = obj_data->flags;
		sub_ptr->home_position = obj_data->position;
		sub_ptr->left_extra = obj_data->leftExtra;
		sub_ptr->top_extra = obj_data->topExtra;
		sub_ptr->right_extra = obj_data->rightExtra;
		sub_ptr->bot_extra = obj_data->botExtra;
		sub_ptr->refcon = obj_data->refCon;
		sub_ptr->callback_refcon = obj_data->callbackRefCon;
		sub_ptr->alignment_flags = obj_data->param2;
		sub_ptr->tighten_ascent = obj_data->tightenAscent;
		sub_ptr->tighten_descent = obj_data->tightenDescent;

		sub_ptr->reserved1 = obj_data->reserved1;
		sub_ptr->reserved2 = obj_data->reserved2;
		sub_ptr->reserved3 = obj_data->reserved3;
		sub_ptr->reserved4 = obj_data->reserved4;
		sub_ptr->reserved5 = obj_data->reserved5;
		sub_ptr->reserved6 = obj_data->reserved6;
	}
	else {
		
		obj_data->param2 = sub_ptr->alignment_flags;
		obj_data->width = (short)(sub_ptr->subset_bounds.bot_right.h - sub_ptr->subset_bounds.top_left.h);
		obj_data->height = (short)(sub_ptr->subset_bounds.bot_right.v - sub_ptr->subset_bounds.top_left.v);
		obj_data->alternate = sub_ptr->alt_index;
		obj_data->subref = (long)sub_ptr->myself;
		obj_data->objType = PGEMBED_PGSUBSET;
		obj_data->emptyWidth = sub_ptr->empty_width;
		obj_data->minWidth = (short)sub_ptr->minimum_width;
		obj_data->matrixExtra = (short)sub_ptr->generated_extra;
		obj_data->dataModifier = sub_ptr->t_length;
		obj_data->flags = flags = sub_ptr->subref_flags;

		if (set_focus_flag)
			obj_data->flags |= PGOBJFLAG_SETFOCUS;
		else
			obj_data->flags &= (~PGOBJFLAG_SETFOCUS);

		obj_data->editWindow = (HWND)pg_stuff->myself;
		obj_data->pg = pg_stuff->pg;
		obj_data->position = sub_ptr->home_position;
		obj_data->leftExtra = sub_ptr->left_extra;
		obj_data->topExtra = sub_ptr->top_extra;
		obj_data->rightExtra = sub_ptr->right_extra;
		obj_data->botExtra = sub_ptr->bot_extra;
		obj_data->refCon = sub_ptr->refcon;
		obj_data->callbackRefCon = sub_ptr->callback_refcon;
		obj_data->tightenAscent = sub_ptr->tighten_ascent;
		obj_data->tightenDescent = sub_ptr->tighten_descent;
		obj_data->descent = sub_ptr->descent;

		obj_data->reserved1 = sub_ptr->reserved1;
		obj_data->reserved2 = sub_ptr->reserved2;
		obj_data->reserved3 = sub_ptr->reserved3;
		obj_data->reserved4 = sub_ptr->reserved4;
		obj_data->reserved5 = sub_ptr->reserved5;
		obj_data->reserved6 = sub_ptr->reserved6;
	}
	
	if (!alt_ptr)
		UnuseMemory(subref);

	return	flags;
}

static void invalidate_subref (paige_control_ptr pg_stuff, pg_subref subref)
{
	paige_rec_ptr			pg;
	pg_subref				current_ref, next_subref;
	paige_sub_ptr			sub_ptr;
	memory_ref				state;
	long					home_position;

	pg = UseMemory(pg_stuff->pg);
	sub_ptr = UseMemory(subref);
	sub_ptr += (sub_ptr->alt_index);
	current_ref = subref;
	
	for (;;) {
		
		pgInvalSubBlocks(pg, sub_ptr);
		home_position = sub_ptr->home_position;

		if ((next_subref = sub_ptr->home_sub) == MEM_NULL)
			break;
		
		UnuseMemory(current_ref);
		sub_ptr = UseMemory(next_subref);
		current_ref = next_subref;
	}
	
	UnuseMemory(current_ref);
	
	state = pgGetSubrefState(pg, FALSE, TRUE);
	pgInvalSelect(pg->myself, home_position, home_position);
	pgRestoreSubRefs(pg, state);
	
	UnuseMemory(pg_stuff->pg);
}


/* locate_previous_ref returns the next subref in the block->text[position] */

static pg_subref locate_previous_ref (paige_rec_ptr pg, long position)
{
	register pg_char_ptr	text;
	register pg_short_t		local_ctr;
	text_block_ptr			block;
	pg_subref				result;

	if (position == 0)
		return	MEM_NULL;

	block = pgFindTextBlock(pg, position, NULL, FALSE, TRUE);

	text = UseMemory(block->text);
	result = MEM_NULL;
	local_ctr = (pg_short_t)(position - block->begin);
		
	while (local_ctr) {
		
		--local_ctr;
		
		if (text[local_ctr] == SUBREF_CHAR)
			break;
		
		if (local_ctr == 0)
			if (block->begin > 0) {
			
			UnuseMemory(block->text);
			--block;
			text = UseMemory(block->text);
			local_ctr = (pg_short_t)(block->end - block->begin);
		}
	}

	if (text[local_ctr] == SUBREF_CHAR) {
		long			subref_pos;
		
		subref_pos = pgFindSubrefPosition(block, local_ctr);
		GetMemoryRecord(block->subref_list, subref_pos, (void PG_FAR *)&result);
	}

	UnuseMemory(block->text);
	UnuseMemory(pg->t_blocks);

	return		result;
}


/* locate_next_ref locates the next subref, beginning at position. If sub_ptr is NULL,
the top level (pg) is used. */

static pg_subref locate_next_ref (paige_rec_ptr pg, paige_sub_ptr sub_ptr, long position)
{
	register pg_char_ptr	text;
	register pg_short_t		local_ctr;
	text_block_ptr			block;
	pg_short_t				text_size;
	pg_subref				result;
	memory_ref				blockref;
	long					t_length;

	if (sub_ptr)
		t_length = sub_ptr->t_length;
	else
		t_length = pg->t_length;

	if (position == t_length)
		return	MEM_NULL;

	if (sub_ptr) {
		
		blockref = sub_ptr->t_blocks;
		block = UseMemory(blockref);
	}
	else {
		
		blockref = pg->t_blocks;
		block = pgFindTextBlock(pg, position, NULL, FALSE, TRUE);
	}

	text = UseMemory(block->text);
	result = MEM_NULL;
	local_ctr = (pg_short_t)(position - block->begin);
	text_size = (pg_short_t)(block->end - block->begin);

	while (local_ctr < text_size) {
		
		if (text[local_ctr] == SUBREF_CHAR)
			break;
		
		++local_ctr;
		
		if (local_ctr >= text_size)
			if (pg)
				if (block->end < t_length) {
			
			UnuseMemory(block->text);
			++block;
			text = UseMemory(block->text);
			local_ctr = 0;
			text_size = (pg_short_t)(block->end - block->begin);
		}
	}
	
	if (local_ctr < text_size)
		if (text[local_ctr] == SUBREF_CHAR) {
		long			subref_pos;
		
		subref_pos = pgFindSubrefPosition(block, local_ctr);
		GetMemoryRecord(block->subref_list, subref_pos, (void PG_FAR *)&result);
	}

	UnuseMemory(block->text);
	UnuseMemory(blockref);

	return		result;
}



