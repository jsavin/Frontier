/* This source file handles all the PAIGE messages dealing with general maintanence. It handles
all "PG_USER" messages. */


#include "Paige.h"
#include "pgTraps.h"
#include "pgUtils.h"
#include "defprocs.h"
#include "pgCtlUtl.h"
#include "pgEmbed.h"
#include "pgEdit.h"
#include "pgCntl.h"
#include "pgSubref.h"

long pascal UserMessageProc (HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	PAIGEINFOSTRUCT PG_FAR	*user_info;
	paige_control_ptr		pg_stuff;			// One of these in each control
	memory_ref				pg_stuff_ref;		// A memory_ref that holds above
	pg_doc_info				doc_info;
	co_ordinate				position_pt;
	long					response;
	int						repaint;
	paige_rec_ptr			pg_rec;

	response = 0;
	pg_stuff = GetPGStuff(hWnd, &pg_stuff_ref);

	switch (message) {

		case PG_GETGLOBALS:
			pgBlockMove(&paige_globals, (void PG_FAR *)lParam, sizeof(pg_globals));
			break;
		
		case PG_SETGLOBALS:
			pgBlockMove((void PG_FAR *)lParam, &paige_globals, sizeof(pg_globals));
			break;

		case PG_GETEXSTYLES:
			response = pg_stuff->ex_styles;
			break;

		case PG_SETEXSTYLES:
			pg_stuff->flags |= SCROLLBARS_DIRTY_FLAG;

			if (SetExtendedStyleBits(pg_stuff, lParam)) {
				
				SetPageArea(pg_stuff);

				if (wParam)
					RepaintControl(hWnd, pg_stuff, FALSE);
			}
			
			break;

		case PG_GETNOTIFICATIONS:
			response = pg_stuff->notification;
			break;

		case PG_SETNOTIFICATIONS: {
		        paige_rec_ptr	pg_rec;
		        
				pg_stuff->notification = lParam;
				pg_rec = UseMemory(pg_stuff->pg);
				pg_rec->notification = lParam;
				
				if (lParam & PGN_OVERFLOWFLAG)
					pg_rec->flags2 |= CHECK_PAGE_OVERFLOW;
				else
					pg_rec->flags2 &= (~CHECK_PAGE_OVERFLOW);

				UnuseMemory(pg_stuff->pg);
			}
			break;

		case PG_SETINFO:
			user_info = (PAIGEINFOSTRUCT PG_FAR *)lParam;

			pg_stuff->notification = user_info->notification;
			pg_stuff->paint_mode = user_info->paint_mode;
			pg_stuff->update_mode = user_info->update_mode;
			pg_stuff->scroll_mode = user_info->scroll_mode;
            pg_stuff->print_mode = user_info->print_mode;
            
            repaint = (pg_stuff->page_pen != user_info->page_pen);
			repaint |= (pg_stuff->frame_pen != user_info->frame_pen);
			repaint |= (pg_stuff->columns != user_info->columns);
			repaint |= (pg_stuff->column_gap != user_info->column_gap);
			
			pg_stuff->page_pen = user_info->page_pen;
			pg_stuff->frame_pen = user_info->frame_pen;
			pg_stuff->page_break_pen = user_info->page_break_pen;
			pg_stuff->extra_scroll_h = user_info->extra_scroll_h;
			pg_stuff->extra_scroll_v = user_info->extra_scroll_v;
            pg_stuff->left_margin = (short)user_info->print_margins.left;
            pg_stuff->right_margin = (short)user_info->print_margins.right;
            pg_stuff->top_margin = (short)user_info->print_margins.top;
            pg_stuff->bottom_margin = (short)user_info->print_margins.bottom;
            pg_stuff->columns = user_info->columns;
            pg_stuff->column_gap = user_info->column_gap;
            pg_stuff->break_pen_width = user_info->page_gap;
			pg_stuff->page_slop = user_info->page_slop;

            RectToRectangle(&user_info->page_rect, &pg_stuff->page_rect);
            RectToRectangle(&user_info->vis_rect, &pg_stuff->vis_rect);

			repaint = SetExtendedStyleBits(pg_stuff, user_info->ex_styles);
			repaint |= SetPageArea(pg_stuff);

			if (wParam && repaint)
				RepaintControl(hWnd, pg_stuff, TRUE);

			break;

		case PG_GETINFO:
			user_info = (PAIGEINFOSTRUCT PG_FAR *)lParam;

			user_info->ex_styles = pg_stuff->ex_styles;
			user_info->notification = pg_stuff->notification;
			user_info->paint_mode = pg_stuff->paint_mode;
			user_info->update_mode = pg_stuff->update_mode;
			user_info->scroll_mode = pg_stuff->scroll_mode;
			user_info->print_mode = pg_stuff->print_mode;
			user_info->page_pen = (HPEN)pg_stuff->page_pen;
			user_info->frame_pen = (HPEN)pg_stuff->frame_pen;
			user_info->page_break_pen = (HPEN)pg_stuff->page_break_pen;
			user_info->extra_scroll_h = pg_stuff->extra_scroll_h;
			user_info->extra_scroll_v = pg_stuff->extra_scroll_v;
            user_info->columns = pg_stuff->columns;
            user_info->column_gap = pg_stuff->column_gap;
            user_info->page_gap = pg_stuff->break_pen_width;
            user_info->print_margins.left = pg_stuff->left_margin;
            user_info->print_margins.right = pg_stuff->right_margin;
            user_info->print_margins.top = pg_stuff->top_margin;
            user_info->print_margins.bottom = pg_stuff->bottom_margin;
            
			pgGetDocInfo(pg_stuff->pg, &doc_info);
			
			user_info->page_slop = (short)doc_info.repeat_slop;
			
			RectangleToRect(&pg_stuff->vis_rect, NULL, &user_info->vis_rect);
			RectangleToRect(&pg_stuff->page_rect, NULL, &user_info->page_rect);

			break;

		case PG_GETPGREF:
			response = (long)pg_stuff->pg;
			break;

		case PG_VMENABLE:
			if (wParam)
				InitVirtualMemory(&mem_globals, NULL, (long)wParam);
			break;

		case PG_MOUSEINDOC: {
				co_ordinate			pt;
				
				SplitIntoLongs(lParam, &pt.v, &pt.h);
				response = (long)pgPtInView(pg_stuff->pg, &pt, NULL);
				
				if (pgPtInEmbed(pg_stuff->pg, &pt, NULL, NULL, FALSE))
					response |= PGPT_INEMBED;
			}
			
			break;

		case PG_TEXTHEIGHT:
			response = pgTotalTextHeight(pg_stuff->pg, TRUE);
			break;

		case PG_SETEXTRASTRUCT:
			if (pg_stuff->user_data == MEM_NULL)
				pg_stuff->user_data = MemoryAllocID(&mem_globals,
						1, 0, 0, GetMemoryRefID(pg_stuff->pg));

			SetMemorySize(pg_stuff->user_data, (long)wParam);
			
			if (wParam) {
				
				pgBlockMove((void PG_FAR *)lParam, UseMemory(pg_stuff->user_data), wParam);
				UnuseMemory(pg_stuff->user_data);
			}
			
			break;

		case PG_GETEXTRASTRUCT:
			if (pg_stuff->user_data) {
				long			datasize;
				
				if (!(datasize = wParam))
					datasize = GetMemorySize(pg_stuff->user_data);

				if (datasize) {
					
					pgBlockMove(UseMemory(pg_stuff->user_data), (void PG_FAR *)lParam, datasize);
					UnuseMemory(pg_stuff->user_data);
				}
				
				response = datasize;
			}
			
			break;

		case PG_TEXTSIZE:
			response = pgTextSize(pg_stuff->pg);
			break;

		case PG_SCROLLPOSITION:
			pgScrollPosition(pg_stuff->pg, (co_ordinate_ptr)lParam);
			break;

		case PG_PAGINATENOW:
			pgPaginateNow(pg_stuff->pg, lParam, FALSE);
			break;
		
		case PG_POINTPOSITION:
			position_pt.h = HIWORD(lParam);
			position_pt.v = LOWORD(lParam);
			response = pgPtToChar(pg_stuff->pg, &position_pt, NULL);
			break;

		case PG_REPAINT:
			pg_rec = UseMemory(pg_stuff->pg);

#ifdef MAC_PLATFORM
			pg_rec->flags &= (~CARET_BIT);

			if (pg_rec->active_subset)
				pgClearSubrefCarets(pg_rec, MEM_NULL);
#endif
			pgUpdateText(pg_rec, NULL, 0, pg_rec->t_length, MEM_NULL, NULL, (short)wParam, TRUE);
			UnuseMemory(pg_stuff->pg);
			NotifyOverflow(pg_stuff);
			break;
		
		case PG_GETCHANGES:
			response = pgGetChangeCtr(pg_stuff->pg);
			break;

		case PG_SETCHANGES:
			pgSetChangeCtr(pg_stuff->pg, lParam);
			break;

		case PG_SAVEFOCUS:
			pg_rec = UseMemory(pg_stuff->pg);
			response = (long)pgGetSubrefState(pg_rec, FALSE, FALSE);
			UnuseMemory(pg_stuff->pg);
			break;

		case PG_RESTOREFOCUS:
			pg_rec = UseMemory(pg_stuff->pg);
			pgRestoreSubRefs(pg_rec, (memory_ref)lParam);
			UnuseMemory(pg_stuff->pg);
			break;
		
		case PG_SHAREREFS:
			if (lParam)
				pgShareRefs(pg_stuff->pg, (pg_ref)lParam, (long)wParam);
			break;
		
		case PG_UPDATESCROLLBARS:
	    	UpdateScrollbars(hWnd, pg_stuff, (pg_boolean)wParam);
			NotifyOverflow(pg_stuff);
			break;
		
		case PG_SETNOTIFYWINDOW:
			pg_stuff->parent = (HWND)lParam;
			break;

		default:
			response = DefWindowProc(hWnd, message, wParam, lParam);
			break;
	}

	UnuseMemory(pg_stuff_ref);

	return	response;
}


/* SetExtendedStyleBits sets (or changes) the ex-style bits for the control.
If something changes that would change the document appearance, TRUE is returned. */

int pascal SetExtendedStyleBits (paige_control_ptr pg_stuff, long new_flags)
{
	paige_rec_ptr	pg_rec;
	pg_doc_info		doc_info;
	long			old_bits, flags;
	int				result;

	old_bits = pg_stuff->ex_styles & (PGS_USELF | PGS_MULTIPAGE | PGS_FIXEDPAGE);
	result = (old_bits != (new_flags & (PGS_USELF | PGS_MULTIPAGE | PGS_FIXEDPAGE)));

	pg_stuff->ex_styles = new_flags;
	pgGetDocInfo(pg_stuff->pg, &doc_info);

	if (new_flags & PGS_MULTIPAGE) {

		doc_info.attributes = V_REPEAT_BIT | BOTTOM_FIXED_BIT;
		doc_info.attributes |= MAX_SCROLL_ON_SHAPE;
	}
	else {
	
		doc_info.attributes &= (~(V_REPEAT_BIT | BOTTOM_FIXED_BIT));
		
		if (new_flags & PGS_FIXEDPAGE)
			doc_info.attributes |= BOTTOM_FIXED_BIT;
	}
    
    if (new_flags & PGS_CARETVIEW)
    	doc_info.attributes |= WINDOW_CURSOR_BIT;
	else
		doc_info.attributes &= (~WINDOW_CURSOR_BIT);

	if (!(doc_info.attributes & BOTTOM_FIXED_BIT) || (new_flags & PGS_COLORVISRGN))
		doc_info.attributes |= COLOR_VIS_BIT;
	else
		doc_info.attributes &= (~COLOR_VIS_BIT);

	flags = pgGetAttributes(pg_stuff->pg);
	flags &= (~(NO_LF_BIT | PGS_NOLEADING | SMART_QUOTES_BIT | NO_SMART_CUT_BIT | LINE_EDITOR_BIT | SCALE_VIS_BIT));

	if (!(new_flags & PGS_USELF))
		flags |= NO_LF_BIT;
	if (new_flags & PGS_SMARTQUOTES)
		flags |= SMART_QUOTES_BIT;
	if (!(new_flags & PGS_SMARTCUT))
		flags |= NO_SMART_CUT_BIT;
	if (new_flags & PGS_SCALEVISRGN)
		flags |= SCALE_VIS_BIT;
	if (new_flags & PGS_NOLEADING)
		flags |= PGS_NOLEADING;

	if (new_flags & PGS_LINEDITMODE) {

		flags |= LINE_EDITOR_BIT;
		doc_info.attributes &= (~(V_REPEAT_BIT | BOTTOM_FIXED_BIT | BOTTOM_FIXED_BIT));
	}

	pgSetAttributes(pg_stuff->pg, flags);
	pgSetDocInfo(pg_stuff->pg, &doc_info, (pg_boolean)(result != FALSE), draw_none);
	
// Make sure appropriate flags2 bits are set:

	pg_rec = UseMemory(pg_stuff->pg);
	
	if (pg_rec->notification & PGN_OVERFLOWFLAG)
		pg_rec->flags2 |= CHECK_PAGE_OVERFLOW;
	else
		pg_rec->flags2 &= (~CHECK_PAGE_OVERFLOW);

	UnuseMemory(pg_stuff->pg);

	return	result;
}



/************************************* Local Functions ******************************************/

