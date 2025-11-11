/* This source file handles all the PAIGE messages dealing with additional Edit Menu items.
It handles all "PG_EDITMESSAGES" messages. */


#include "Paige.h"
#include "pgTraps.h"
#include "pgUtils.h"
#include "defprocs.h"
#include "pgCtlUtl.h"
#include "pgText.h"
#include "pgEdit.h"
#include "pgSelect.h"
#include "pgDefStl.h"
#include "pgCntl.h"
#include "pgExceps.h"
#include "pgSubref.h"
#include "pgScrap.h"


long pascal EditMessageProc (HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	paige_control_ptr		pg_stuff;			// One of these in each control
	memory_ref				pg_stuff_ref;		// A memory_ref that holds above
	PAIGEBUFFERSTRUCT PG_FAR *buffer_ptr;
	paige_rec_ptr			pg_rec;
	text_block_ptr			block;
	pg_char_ptr				text_copy, text;
	pg_ref					the_scrap;
	long PG_FAR 			*selection;
	void PG_FAR				*undo_param;
	long					response, offset, max_size, insert_undo_param;
	short					draw_mode;

	response = 0;
	pg_stuff = GetPGStuff(hWnd, &pg_stuff_ref);

	if (pg_stuff == NULL && message != PG_CANPASTE)
		return	0L;

/* The following sets the "draw_mode" for all functions in which wParam
is TRUE or FALSE for re-draw. If wParam is used for something else it
does not matter because we will not use "draw_mode" variable. */

    if (wParam)
    	draw_mode = pg_stuff->update_mode;
    else
       draw_mode = draw_none;

	switch (message) {

		case PG_GETSELECTION:
			selection = (long PG_FAR *)lParam;
			pgGetSelection(pg_stuff->pg, &selection[0], &selection[1]);
			break;

		case PG_SETSELECTION:
			selection = (long PG_FAR *)lParam;
			pgSetSelection(pg_stuff->pg, selection[0], selection[1], 0, (pg_boolean)wParam);
			break;


		case PG_INSERTTEXT: {
				long		textsize;
				
				PG_TRY(&mem_globals) {

					selection = (long PG_FAR *)lParam;
					textsize = *selection++;
	
					pgInsert(pg_stuff->pg, (pg_char_ptr)selection, textsize,
							CURRENT_POSITION, data_insert_mode, 0, draw_mode);
	
					NotifyOverflow(pg_stuff);
				}
				
				PG_CATCH {
					
					if (pg_stuff_ref) {
					
						SendNotification(UseMemory(pg_stuff_ref), PGN_FAILURE, mem_globals.last_error, NULL);
						UnuseMemory(pg_stuff_ref);
					}
				}
				
				PG_ENDTRY;
			}
			
			break;

		case PG_GETTEXT:
		// PG_GETTEXT must have lParam as a far ptr to a buffer, with first long as the
		// text offset (or -1 if current location), second long as maximum size.
			pg_rec = UseMemory(pg_stuff->pg);
			buffer_ptr = (PAIGEBUFFERSTRUCT PG_FAR *)lParam;
			offset = pgFixOffset(pg_rec, buffer_ptr->beginPos);
			block = pgFindTextBlock(pg_rec, offset, NULL, FALSE, TRUE);
			max_size = buffer_ptr->maxSize;
			text_copy = (pg_char_ptr)buffer_ptr->buffer;
			
			if ((offset + max_size) > pg_rec->t_length)
				max_size = pg_rec->t_length - offset;
			
			response = 0;
			
			while (response < max_size) {
				long			remaining, ending_offset, transfer_qty;
				
				text = UseMemory(block->text);
				text += (offset - block->begin);
				
				remaining = max_size - response;
				ending_offset = offset + remaining;
				
				if (ending_offset > block->end)
					ending_offset = block->end;
				
				transfer_qty = ending_offset - offset;
				pgBlockMove(text, text_copy, transfer_qty * sizeof(pg_char));
				UnuseMemory(block->text);

				response += transfer_qty;
				text_copy += transfer_qty;
				
				offset = block->end;
				++block;
			}
			
			UnuseMemory(pg_rec->t_blocks);
			UnuseMemory(pg_stuff->pg);
			break;
			
		
		case PG_COPYTEXT:
			{
				memory_ref		copied_text = MEM_NULL;

				PG_TRY(&mem_globals) {

					if ((copied_text = pgCopyText(pg_stuff->pg, (select_pair_ptr)lParam, (short)wParam)) != MEM_NULL) {
						
						AppendMemory(copied_text, 1, TRUE);
						UnuseMemory(copied_text);
						response = PG_LONGWORD(long)MemoryToHandle(copied_text);
					}
				}
				
				PG_CATCH {
				
					if (copied_text)
						DisposeMemory(copied_text);
				
					if (pg_stuff_ref) {
					
						SendNotification(UseMemory(pg_stuff_ref), PGN_FAILURE, mem_globals.last_error, NULL);
						UnuseMemory(pg_stuff_ref);
					}
				}
				
				PG_ENDTRY
			}

			break;

		case PG_PREPAREUNDO:
			
			PG_TRY(&mem_globals) {
			
				undo_param = NULL;
				
				if (wParam == PGUNDO_PASTE) {

			#ifdef WINDOWS_PLATFORM
					OpenClipboard(hWnd);
			#endif

					if ((the_scrap = (pg_ref)SendNotification(pg_stuff, PGN_GETCLIPBOARD, TRUE, 0)) == MEM_NULL)
						the_scrap = pgGetScrap(&paige_globals, clipboard_format, EmbedCallbackProc);

			#ifdef WINDOWS_PLATFORM
					CloseClipboard();
			#endif
					
					if (the_scrap == MEM_NULL)
						break;

					undo_param = (void PG_FAR *)the_scrap;
				}
				else
				if (wParam == PGUNDO_INSERT) {
					
					insert_undo_param = lParam;
					undo_param = (void PG_FAR *)&insert_undo_param;
				}
				
				if (pg_stuff->undo) {
					undo_ref		current_undo;
					
					current_undo = pg_stuff->undo;
					pg_stuff->undo = MEM_NULL;
					pgDisposeUndo(pg_stuff->undo);
				}
				
				pg_stuff->undo = pgPrepareUndo(pg_stuff->pg, (short)wParam, undo_param);
				response = (long)pg_stuff->undo;
				
				if (wParam == PGUNDO_PASTE)
					pgDispose(the_scrap);
			}
			
			PG_CATCH {
				
				if (pg_stuff->undo) {
				
					pgDisposeUndo(pg_stuff->undo);
					pg_stuff->undo = MEM_NULL;

					if (pg_stuff_ref) {
					
						SendNotification(UseMemory(pg_stuff_ref), PGN_FAILURE, mem_globals.last_error, NULL);
						UnuseMemory(pg_stuff_ref);
					}
				}
			}
			
			PG_ENDTRY
			break;

        case PG_UNDO:
 			if (pg_stuff->undo) {
				undo_ref		redo_ref;
				
				PG_TRY(&mem_globals) {
				
					redo_ref = pgUndo(pg_stuff->pg, pg_stuff->undo, TRUE, pg_stuff->update_mode);
					pgDisposeUndo(pg_stuff->undo);
					pg_stuff->undo = redo_ref;
					NotifyOverflow(pg_stuff);
				}
				
				PG_CATCH {
					
					if (pg_stuff_ref) {
					
						SendNotification(UseMemory(pg_stuff_ref), PGN_FAILURE, mem_globals.last_error, NULL);
						UnuseMemory(pg_stuff_ref);
					}
				}
				
				PG_ENDTRY
			}
            
            break;
		
		case PG_UNDOTYPE:
			response = pgUndoType(pg_stuff->undo);
			break;

		case PG_SELECTALL:
		{
			t_select_ptr		select;
			pg_subref			original_subref;
			long				current_offset;
			
			pg_rec = UseMemory(pg_stuff->pg);
			pgSetupGrafDevice(pg_rec, &pg_rec->port, MEM_NULL, TRUE);
			original_subref = pg_rec->active_subset;

			while (pg_rec->active_subset) {
		
				pgTurnOffHighlight(pg_rec, FALSE);
				pgSetSelection(pg_rec->myself, 0, 0, 0, FALSE);
				pgUnuseSubRef(pg_rec);
			}
			
			pg_rec->subset_focus = 0;
			pg_rec->subref_level = 0;

			pgTurnOffHighlight(pg_rec, FALSE);
			select = UseMemory(pg_rec->select);
			current_offset = select->offset;
			UnuseMemory(pg_rec->select);
			pgSetSelection(pg_rec->myself, current_offset, current_offset, 0, FALSE);
			pg_rec->procs.set_device(pg_rec, unset_pg_device, &pg_rec->port, NULL);
			
			if (original_subref)
				pgRedrawSubsetLine(pg_rec, original_subref, -1);

			UnuseMemory(pg_stuff->pg);
		}

			pgSetSelection(pg_stuff->pg, 0, pgTextSize(pg_stuff->pg), 0, (pg_boolean)wParam);
			break;

		case PG_CANCUT:
			response = (pgNumSelections(pg_stuff->pg) != 0);
			break;

		case PG_CANPASTE:
			#ifdef WINDOWS_PLATFORM
			OpenClipboard(hWnd);
			#endif
			
			if ((response = SendNotification(pg_stuff, PGN_GETCLIPBOARD, FALSE, 0)) == 0)
				response = (long)pgScrapAvail(clipboard_format);

			#ifdef WINDOWS_PLATFORM
			CloseClipboard();
			#endif

			break;

        case PG_ENABLEUNDO:
        	if (wParam)
        		pg_stuff->ex_styles &= (~PGS_NOAUTOUNDO);
        	else
        		pg_stuff->ex_styles |= PGS_NOAUTOUNDO;
        	
        	break;
 
		case PG_SETCARET:
			if (pgNumSelections(pg_stuff->pg)) {
				
				pgGetSelection(pg_stuff->pg, &offset, NULL);
				pgSetSelection(pg_stuff->pg, offset, offset, 0, TRUE);
			}

			switch (wParam) {
			
				case PGCARET_ABSOLUTE:
					pgSetSelection(pg_stuff->pg, lParam, lParam, 0, TRUE);
					break;
		
				case PGCARET_RELATIVE:
					pg_rec = UseMemory(pg_stuff->pg);
					
					if ((offset = lParam) == CURRENT_POSITION)
						offset = 0;
		
					offset += pgCurrentInsertion(pg_rec);
					UnuseMemory(pg_stuff->pg);
					pgSetSelection(pg_stuff->pg, offset, offset, 0, TRUE);
					break;
		
				case PGCARET_HOME:
					pgSetCaretPosition(pg_stuff->pg, home_caret, TRUE);
					break;
		
				case PGCARET_BOTTOM:
					pgSetCaretPosition(pg_stuff->pg, doc_bottom_caret, TRUE);
					break;

				case PGCARET_BEGINLINE:
					pgSetCaretPosition(pg_stuff->pg, begin_line_caret, TRUE);
					break;
					
				case PGCARET_ENDLINE:
					pgSetCaretPosition(pg_stuff->pg, end_line_caret, TRUE);
					break;

				case PGCARET_TOPVIS:
				case PGCARET_BOTVIS:
					{
						rectangle			vis;
						co_ordinate			use_point;
						
						pgAreaBounds(pg_stuff->pg, NULL, &vis);
						
						if (wParam == PGCARET_TOPVIS)
							use_point.v = vis.top_left.v + 2;
						else
							use_point.v = vis.bot_right.v - 2;
						
						use_point.h = vis.top_left.h;
						offset = pgPtToChar(pg_stuff->pg, &use_point, NULL);
						pgSetSelection(pg_stuff->pg, offset, offset, 0, TRUE);
					}

					break;
			}
			
			break;

		case PG_GETHILITERGN:
			{
				shape_ref			hilite_shape;
				
				hilite_shape = pgRectToShape(&mem_globals, NULL);
				
				if (pgGetHiliteRgn(pg_stuff->pg, (select_pair_ptr)lParam, MEM_NULL, hilite_shape)) {
					
					pg_rec = UseMemory(pg_stuff->pg);
			#ifdef MAC_PLATFORM
					response = (long)NewRgn();
			#else
					response = PG_LONGWORD(long)CreateRectRgn(0, 0, 0, 0);
			#endif
					ShapeToRgn(hilite_shape, 0, 0,
							&pg_rec->scale_factor, 0, NULL, (RgnHandle)response);
					UnuseMemory(pg_stuff->pg);
				}

				pgDisposeShape(hilite_shape);
			}

			break;
			
		case PG_NUMLINES:
			response = pgNumLines(pg_stuff->pg);
			break;

		case PG_OFFSETTOLINENUM:
			response = pgOffsetToLineNum(pg_stuff->pg, (long)lParam, (pg_boolean)wParam);
			break;

		case PG_LINENUMTOOFFSET:
			pgLineNumToOffset(pg_stuff->pg, lParam, &response, NULL);
			break;

		case PG_NUMPARS:
			response = pgNumPars(pg_stuff->pg);
			break;

		case PG_OFFSETTOPARNUM:
			response = pgOffsetToParNum(pg_stuff->pg, lParam);
			break;

		case PG_PARNUMTOOFFSET:
			pgParNumToOffset(pg_stuff->pg, lParam, &response, NULL);
			break;

		case PG_CHARACTERRECT:
			{
				PAIGERECTSTRUCT	PG_FAR	*char_info;
				
				char_info = (PAIGERECTSTRUCT PG_FAR	*)lParam;
				pgCharacterRect(pg_stuff->pg, char_info->query, FALSE, TRUE, (rectangle_ptr)&char_info->top);
				
				pg_rec = UseMemory(pg_stuff->pg);
				char_info->screen_x = -pg_rec->scroll_pos.h;
				char_info->screen_y = -pg_rec->scroll_pos.v;
				UnuseMemory(pg_stuff->pg);
			}
			break;

		case PG_OFFSETTOWORD:
			// lParam points to a pair of offsets, the first one is used to determine word.
			selection = (long PG_FAR *)lParam;
			pgFindWord(pg_stuff->pg, *selection, selection, &selection[1], (pg_boolean)wParam, FALSE);
			break;

		case PG_OFFSETTOPAR:
			// lParam points to a pair of offsets, the first one is used to determine word.
			selection = (long PG_FAR *)lParam;
			pgFindPar(pg_stuff->pg, *selection, selection, &selection[1]);
			break;

		case PG_STYLECOPY:
			PG_TRY(&mem_globals) {
			
				response = (long)pgCopy(pg_stuff->pg, (select_pair_ptr)lParam);
			}
			
			PG_CATCH
				if (pg_stuff_ref) {
				
					SendNotification(UseMemory(pg_stuff_ref), PGN_FAILURE, mem_globals.last_error, NULL);
					UnuseMemory(pg_stuff_ref);
				}
			PG_ENDTRY

			break;

		case PG_STYLEPASTE:
		case PG_STYLEPASTETEXTONLY:
		case PG_STYLEPASTEDEFAULT:
			if (lParam) {
				style_info			def_source, def_target;

				PG_TRY(&mem_globals) {
				
					if (message == PG_STYLEPASTEDEFAULT) {
						paige_rec_ptr		source_pg;
						
						source_pg = UseMemory((pg_ref)lParam);
						pg_rec = UseMemory(pg_stuff->pg);
						
						pgGetDefaultStyle((pg_ref)lParam, &def_source);
						pgGetDefaultStyle(pg_stuff->pg, &def_target);
						def_target.font_index = pgAddCrossFont(pg_rec, source_pg, &def_target);
						UnuseMemory((pg_ref)lParam);
						UnuseMemory(pg_stuff->pg);
						def_target.used_ctr = def_source.used_ctr;
						pgSetDefaultStyle((pg_ref)lParam, &def_target, draw_none);
					}
	
					pgPaste(pg_stuff->pg, (pg_ref)lParam, CURRENT_POSITION,
						(pg_boolean)(message == PG_STYLEPASTETEXTONLY), draw_mode);
					
					if (draw_mode) {
				   		pgAdjustScrollMax(pg_stuff->pg, pg_stuff->update_mode);
				    	UpdateScrollbars(hWnd, pg_stuff, TRUE);
						NotifyOverflow(pg_stuff);
				    }
	
					if (message == PG_STYLEPASTEDEFAULT)
						pgSetDefaultStyle((pg_ref)lParam, &def_source, draw_none);
	
				    SendChangeCommand(pg_stuff);
				}
				
				PG_CATCH
				
				if (pg_stuff_ref) {
				
					SendNotification(UseMemory(pg_stuff_ref), PGN_FAILURE, mem_globals.last_error, NULL);
					UnuseMemory(pg_stuff_ref);
				}

				PG_ENDTRY
			}

			break;

		case PG_STYLEDISPOSE:
			if (lParam)
				pgDispose((pg_ref)lParam);
			break;

		case PG_INCREMENTSELECT:
		case PG_DECREMENTSELECT:
			pgGetSelection(pg_stuff->pg, NULL, &offset);
			
			if (message == PG_INCREMENTSELECT)
				offset += 1;
			else
				offset -= 1;
			pgSetSelection(pg_stuff->pg, offset, offset, 0, (pg_boolean)(wParam != 0));
			break;

		default:
			response = DefWindowProc(hWnd, message, wParam, lParam);
			break;
	}

	if (pg_stuff_ref)
		UnuseMemory(pg_stuff_ref);

	return	response;
}



