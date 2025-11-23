/* This source file handles all the PAIGE messages dealing with general document changing
messages. It handles all "PG_DOCMESSAGES" messages. */


#include "Paige.h"
#include "pgTraps.h"
#include "machine.h"
#include "pgUtils.h"
#include "pgText.h"
#include "pgSelect.h"
#include "defprocs.h"
#include "pgCtlUtl.h"
#include "pgShapes.h"
#include "pgCntl.h"


static void create_user_pen (HWND hWnd, paige_control_ptr pg_stuff,
		int message, int wParam, long lParam);


long pascal DocMessageProc (HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	paige_control_ptr		pg_stuff;			// One of these in each control
	paige_rec_ptr			pg_rec;
	co_ordinate_ptr			pixel;
	co_ordinate				scroll_pos;
	memory_ref				pg_stuff_ref;		// A memory_ref that holds above
	pg_doc_info				doc_info;
	RECT PG_FAR 			*margins_ptr;
	color_value				bk_color;
	pg_scale_factor			scaling;
	long					response, flags;
	short					draw_mode;
#ifdef WINDOWS_PLATFORM
	long					os_color;
#endif

	response = 0;
	pg_stuff = GetPGStuff(hWnd, &pg_stuff_ref);

/* The following sets the "draw_mode" for all functions in which wParam
is TRUE or FALSE for re-draw. If wParam is used for something else it
does not matter because we will not use "draw_mode" variable. */

    if (wParam)
    	draw_mode = pg_stuff->update_mode;
    else
       draw_mode = draw_none;

	switch (message) {

		case PG_SETBKCOLOR:
#ifdef MAC_PLATFORM
			pgOSToPgColor((void PG_FAR *)lParam, &bk_color);
#endif

#ifdef WINDOWS_PLATFORM
			os_color = lParam;
			pgOSToPgColor((void PG_FAR *)&os_color, &bk_color);
#endif
			pgSetPageColor(pg_stuff->pg, &bk_color);
			SendChangeCommand(pg_stuff);
			
			if (wParam)
				RepaintControl(hWnd, pg_stuff, TRUE);
			
			break;

		case PG_GETBKCOLOR:
			pgGetPageColor(pg_stuff->pg, &bk_color);
#ifdef MAC_PLATFORM
			if (lParam) {
				
				pgColorToOS(&bk_color, (void PG_FAR *)lParam);
			}
#endif
#ifdef WINDOWS_PLATFORM
			pgColorToOS(&bk_color, (void PG_FAR *)&response);
#endif
			break;

		case PG_SETFRAMEPEN:
		case PG_SETPAGEPEN:
		case PG_SETBREAKPEN:
			create_user_pen(hWnd, pg_stuff, message, wParam, lParam);
			break;
			
		case PG_GETFRAMEPEN:
			response = PG_LONGWORD(long)pg_stuff->frame_pen;
			break;
			
		case PG_GETPAGEPEN:
			response = PG_LONGWORD(long)pg_stuff->page_pen;
            break;
        
        case PG_GETBREAKPEN:
        	response = PG_LONGWORD(long)pg_stuff->page_break_pen;
        	break;

		case PG_SETCOLUMNS:
			pg_stuff->flags &= (~HAS_CONTAINERS);
			pg_stuff->columns = LOWORD(lParam);
			if (!(pg_stuff->column_gap = HIWORD(lParam)))
				pg_stuff->column_gap = 1;

			SetPageArea(pg_stuff);

			if (wParam)
				RepaintControl(hWnd, pg_stuff, TRUE);
			break;

		case PG_GETCOLUMNS:
			response = pg_stuff->column_gap;
			response <<= 16;
			response |= (long)pg_stuff->columns;
			break;

		case PG_SETSCALING:
			pgGetScaling(pg_stuff->pg, &scaling);
			scaling.scale = lParam;
			scaling.origin.h = scaling.origin.v = 0;
			pgSetScaling(pg_stuff->pg, &scaling, draw_none);

            if (wParam)
				RepaintControl(hWnd, pg_stuff, TRUE);
				
			break;
			
		case PG_GETSCALING:
			pgGetScaling(pg_stuff->pg, &scaling);
			response = scaling.scale;

			break;

		case PG_SETMARGINS:
	    	margins_ptr = (RECT PG_FAR *)lParam;
	    	pg_stuff->left_margin = (short)margins_ptr->left;
	    	pg_stuff->right_margin = (short)margins_ptr->right;
	    	pg_stuff->top_margin = (short)margins_ptr->top;
	    	pg_stuff->bottom_margin = (short)margins_ptr->bottom;
	    	
	    	SetPageArea(pg_stuff);
	    	
	    	if (wParam)
	    		RepaintControl(hWnd, pg_stuff, TRUE);

			break;
			
		case PG_GETMARGINS:		    	
	    	margins_ptr = (RECT PG_FAR *)lParam;
	    	margins_ptr->left = pg_stuff->left_margin;
	    	margins_ptr->right = pg_stuff->right_margin;
	    	margins_ptr->top = pg_stuff->top_margin;
	    	margins_ptr->bottom = pg_stuff->bottom_margin;

			break;

		case PG_ENABLEHIDDENTEXT:
			flags = pgGetAttributes(pg_stuff->pg);
			
			if (wParam)
				flags |= NO_HIDDEN_TEXT_BIT;
			else
				flags &= (~NO_HIDDEN_TEXT_BIT);
			
			pgSetAttributes(pg_stuff->pg, flags);
			RepaintControl(hWnd, pg_stuff, TRUE);
			break;

		case PG_HIDDENTEXTSTATE:
			flags = pgGetAttributes(pg_stuff->pg);
			response = ((flags & NO_HIDDEN_TEXT_BIT) != 0);
			break;

		case PG_ENABLEINVISIBLES:
			flags = pgGetAttributes(pg_stuff->pg);
			
			if (wParam)
				flags |= SHOW_INVIS_CHAR_BIT;
			else
				flags &= (~SHOW_INVIS_CHAR_BIT);

			pgSetAttributes(pg_stuff->pg, flags);
			RepaintControl(hWnd, pg_stuff, TRUE);
			break;

		case PG_INVISIBLESSTATE:
			flags = pgGetAttributes(pg_stuff->pg);
			response = ((flags & SHOW_INVIS_CHAR_BIT) != 0);
			break;
        
        case PG_INSERTBREAK: {
        		long		insert_size;
            	pg_char		break_char;
            	
            	if (lParam == PGBREAK_PAGE)
            		break_char = (pg_char)paige_globals.ff_char;
            	else
            	if (lParam == PGBREAK_CONTAINER)
            		break_char = (pg_char)paige_globals.container_brk_char;
            	else
            		break;
            	
            	insert_size = 1;
            	
            	if (pg_stuff->undo)
            		pgDisposeUndo(pg_stuff->undo);
            	
            	if (!(pg_stuff->ex_styles & PGS_NOAUTOUNDO))
            		pg_stuff->undo = pgPrepareUndo(pg_stuff->pg, undo_insert, (void PG_FAR *)&insert_size);

            	pgInsert(pg_stuff->pg, &break_char, insert_size,
            			CURRENT_POSITION, data_insert_mode, 0, draw_mode);
            	
        	}
        	break;

		case PG_OFFSETTOPAGE:
			pg_rec = UseMemory(pg_stuff->pg);
			
			if (pg_rec->doc_info.attributes & (V_REPEAT_BIT | H_REPEAT_BIT)) {
				text_block_ptr				block;
				point_start_ptr				starts;
				long						global_offset, r_num;
				pg_short_t					local_offset;
				
				global_offset = lParam;
				block = pgFindTextBlock(pg_rec, global_offset, NULL, TRUE, TRUE);
				
				global_offset -= block->begin;
				local_offset = (pg_short_t)global_offset;
				starts = UseMemory(block->lines);
				
				while (starts->offset < local_offset) {
					
					++starts;
					
					if (starts->flags == TERMINATOR_BITS)
						break;
				}
				
				if (starts->offset > local_offset)
					--starts;

				r_num = starts->r_num;
				UnuseMemory(block->lines);
				UnuseMemory(pg_rec->t_blocks);
				
				response = (r_num / (GetMemorySize(pg_rec->wrap_area) - 1)) + 1;
			}

			UnuseMemory(pg_stuff->pg);
			break;

		case PG_POINTTOPAGE:
			pixel = (co_ordinate_ptr)lParam;
			pgScrollPosition(pg_stuff->pg, &scroll_pos);
			pgAddPt(pixel, &scroll_pos);
			response = (long)pgPixelToPage(pg_stuff->pg, &scroll_pos, NULL, NULL, NULL, NULL, TRUE);
			response += 1;
			break;

		case PG_GETPAGEINFO:
			{
				PAIGERECTSTRUCT	PG_FAR	*page_info;
				rectangle		page;
				co_ordinate		extra;
				
				page_info = (PAIGERECTSTRUCT PG_FAR	*)lParam;
				pg_rec = UseMemory(pg_stuff->pg);
				pgGetWrapRect(pg_rec, page_info->query - 1, &extra);
				pgShapeBounds(pg_rec->wrap_area, &page);
				pgOffsetRect(&page, extra.h, extra.v);
				pgBlockMove(&page, &page_info->top, sizeof(rectangle));

				page_info->screen_x = pg_rec->scroll_pos.h;
				page_info->screen_y = pg_rec->scroll_pos.v;
				
				UnuseMemory(pg_stuff->pg);
			}

			break;

		case PG_USEPALETTE:
			pgSetDevicePalette(pg_stuff->pg, (generic_var)lParam);
			break;
			
		case PG_GETPALETTE:
			response = (long)pgGetDevicePalette(pg_stuff->pg);
			break;
			
		case PG_SETWIDOWCONTROL:
			pgGetDocInfo(pg_stuff->pg, &doc_info);
			doc_info.minimum_widow = HIWORD(lParam);
			doc_info.minimum_orphan = LOWORD(lParam);
			pgSetDocInfo(pg_stuff->pg, &doc_info, TRUE, draw_mode);
			
			if (wParam)
				NotifyOverflow(pg_stuff);

			break;

		case PG_GETWIDOWCONTROL:
			pgGetDocInfo(pg_stuff->pg, &doc_info);
			response = doc_info.minimum_widow;
			response <<= 16;
			response |= doc_info.minimum_orphan;
			break;

		default:
			response = DefWindowProc(hWnd, message, wParam, lParam);
			break;
	}

	UnuseMemory(pg_stuff_ref);

	return	response;
}


/**************************************** Local Functions ************************************/


/* create_user_pen sets the pen object for the frame or page. The message
param is the message sent to the PAIGE control. If wParam is non-zero,
a new HPEN is created with that style, otherwise the pen is assumed to
be an object lParam. If both are zero the pen is removed (so no drawing
occurs). MACINTOSH VERSION: Only lParam is used and is a pen definition
with bits per pgWinDef.h. */

static void create_user_pen (HWND hWnd, paige_control_ptr pg_stuff,
		int message, int wParam, long lParam)
{
#ifdef MAC_PLATFORM
	short			pen_width;

#pragma unused (wParam)

	pen_width = HiWord(lParam) & 0x00FF;

	if (message == PG_SETBREAKPEN) {
		
		pg_stuff->page_break_pen = (long)lParam;
		pg_stuff->break_pen_width = pen_width;
	}
	else
	if (message == PG_SETFRAMEPEN) {

		pg_stuff->frame_pen = (long)lParam;
		pg_stuff->frame_pen_width = pen_width;
	}
	else  {
	
		pg_stuff->page_pen = (long)lParam;
		pg_stuff->page_pen_width = pen_width;
	}
	
#endif
#ifdef WINDOWS_PLATFORM

	LOGPEN			log_pen;
	HPEN			new_pen, old_pen;
    paige_rec_ptr	pg_rec;
    
	if (lParam) {
		
		new_pen = (HPEN)lParam;
		GetObject(new_pen, sizeof(LOGPEN), &log_pen);
	}
	else
	if (wParam >= 0) {
		
		log_pen.lopnStyle = wParam;
		
		if (message == PG_SETBREAKPEN) {
		    
		    pg_rec = UseMemory(pg_stuff->pg);
		    
		    log_pen.lopnWidth.x = DEF_BREAK_WIDTH;
		    
		    if (pg_rec->doc_info.repeat_offset.v < DEF_BREAK_WIDTH)
		    	pg_rec->doc_info.repeat_offset.v = DEF_BREAK_WIDTH;

			UnuseMemory(pg_stuff->pg);
			log_pen.lopnColor = 0x00808080;
		}
		else {
		
			log_pen.lopnWidth.x = 1;
			log_pen.lopnColor = 0;
		}
		
		new_pen = CreatePenIndirect(&log_pen);
	}
	else {
	
		new_pen = (HPEN)NULL;
		log_pen.lopnWidth.x = 0;
	}
	
	if (message == PG_SETBREAKPEN) {
		
		old_pen = (HPEN)pg_stuff->page_break_pen;
		pg_stuff->page_break_pen = new_pen;
		pg_stuff->break_pen_width = (short)log_pen.lopnWidth.x;
	}
	else
	if (message == PG_SETFRAMEPEN) {

		old_pen = (HPEN)pg_stuff->frame_pen;
		pg_stuff->frame_pen = new_pen;
		pg_stuff->frame_pen_width = (short)log_pen.lopnWidth.x;
	}
	else  {
	
		old_pen = (HPEN)pg_stuff->page_pen;
		pg_stuff->page_pen = new_pen;
		pg_stuff->page_pen_width = (short)log_pen.lopnWidth.x;
	}
	
	if (old_pen != new_pen)
		if (old_pen)
			DeleteObject(old_pen);
#endif

    SetPageArea(pg_stuff);
	RepaintControl(hWnd, pg_stuff, TRUE);
}



