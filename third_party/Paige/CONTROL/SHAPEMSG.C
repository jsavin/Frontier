/* This source file handles all the PAIGE messages dealing with shapes, containers and exclusions.
It handles all "PG_SHAPEMESSAGES" messages. */


#include "Paige.h"
#include "pgTraps.h"
#include "machine.h"
#include "pgUtils.h"
#include "defprocs.h"
#include "pgCtlUtl.h"
#include "pgCntl.h"
#include "pgShapes.h"

static void append_shape (paige_control_ptr pg_stuff, PAIGERECTSTRUCT PG_FAR *new_data,
		int merge, int for_exclusion);
static void establish_containers (paige_control_ptr pg_stuff, PAIGERECTSTRUCT PG_FAR *new_data);

static void get_shape_rect (paige_control_ptr pg_stuff, short r_num,
		PAIGERECTSTRUCT PG_FAR *data, int from_exclusion);
static void delete_shape_rect (paige_control_ptr pg_stuff, pg_short_t r_num, int from_exclusion);
static void modify_shape_rect (paige_control_ptr pg_stuff, pg_short_t r_num,
		PAIGERECTSTRUCT PG_FAR *container, int from_exclusion);

long pascal ShapeMessageProc (HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	paige_control_ptr		pg_stuff;			// One of these in each control
	memory_ref				pg_stuff_ref;		// A memory_ref that holds above
	paige_rec_ptr			pg_rec;
	rectangle				vis_bounds, page_bounds;
	long					response;
	short					draw_mode;

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

		case PG_SETPAGERECT:
			RectToRectangle((Rect PG_FAR *)lParam, &page_bounds);
			
			if (pg_stuff->ex_styles & PGS_VISPAGE)
				pg_stuff->vis_rect = page_bounds;
			else
				pg_stuff->page_rect = page_bounds;

			if (SetPageArea(pg_stuff))			
				if (wParam)
					RepaintControl(hWnd, pg_stuff, TRUE);
			
			break;
			
		case PG_GETPAGERECT:
			RectangleToRect(&pg_stuff->page_rect, NULL, (Rect PG_FAR *)lParam);
			break;

		case PG_SETVISRECT:
			RectToRectangle((Rect PG_FAR *)lParam, &vis_bounds);
			pgSetAreaBounds(pg_stuff->pg, NULL, &vis_bounds);
			
			if (pg_stuff->ex_styles & PGS_VISPAGE)
				pgSetAreaBounds(pg_stuff->pg, &vis_bounds, NULL);
			
			if (wParam)
				RepaintControl (hWnd, pg_stuff, TRUE);
			
			break;

		case PG_GETVISRECT:
			pgAreaBounds(pg_stuff->pg, NULL, &vis_bounds);
			RectangleToRect(&vis_bounds, NULL, (RECT PG_FAR *)lParam);
			break;
		
		case PG_SETCONTAINERMODE:
			establish_containers(pg_stuff, (PAIGERECTSTRUCT PG_FAR *)lParam);
			SendChangeCommand(pg_stuff);

			if (wParam)
				RepaintControl(hWnd, pg_stuff, TRUE);
			break;

		case PG_APPENDCONTAINER:
			pg_stuff->flags |= HAS_CONTAINERS;
			// No break
		case PG_APPENDEXCLUSION:
			append_shape(pg_stuff, (PAIGERECTSTRUCT PG_FAR *)lParam, wParam, (message == PG_APPENDEXCLUSION));
			RepaintControl(hWnd, pg_stuff, TRUE);
			SendChangeCommand(pg_stuff);

			break;

		case PG_GETINDCONTAINER:
		case PG_GETINDEXCLUSION:
			get_shape_rect(pg_stuff, (short)wParam, (PAIGERECTSTRUCT PG_FAR *)lParam, (int)(message == PG_GETINDEXCLUSION));
			break;

		case PG_DELETECONTAINER:
		case PG_DELETEEXCLUSION:
			delete_shape_rect(pg_stuff, (pg_short_t)wParam, (message == PG_DELETEEXCLUSION));
			RepaintControl(hWnd, pg_stuff, TRUE);
			SendChangeCommand(pg_stuff);
			break;

		case PG_MODIFYCONTAINER:
		case PG_MODIFYEXCLUSION:
			modify_shape_rect(pg_stuff, (pg_short_t)wParam, (PAIGERECTSTRUCT PG_FAR *)lParam,
					 (message == PG_MODIFYEXCLUSION));
			SendChangeCommand(pg_stuff);
			break;

		case PG_NUMCONTAINERS:
		case PG_NUMEXCLUSIONS:

			pg_rec = UseMemory(pg_stuff->pg);
			
			if (message == PG_NUMEXCLUSIONS) {
				
				if (!pgEmptyShape(pg_rec->exclude_area))
					response = GetMemorySize(pg_rec->exclude_area) - 1;
			}
			else {
				
				if (!(pg_stuff->flags & HAS_CONTAINERS))
					response = 0;
				else
					response = GetMemorySize(pg_rec->wrap_area) - 1;
			}

			UnuseMemory(pg_stuff->pg);

			break;

		case PG_CLEARCONTAINERS:
			pg_stuff->flags &= (~HAS_CONTAINERS);
			SetPageArea(pg_stuff);
            SendChangeCommand(pg_stuff);
            
			if (wParam)
				RepaintControl(hWnd, pg_stuff, TRUE);
			break;

		case PG_CLEAREXCLUSIONS:
			pg_rec = UseMemory(pg_stuff->pg);
			
			pgInvalSelect(pg_stuff->pg, 0, pgTextSize(pg_stuff->pg));
			pgSetShapeRect(pg_rec->exclude_area, NULL);
			SendChangeCommand(pg_stuff);
			InvalidateShape(pg_rec, TRUE);
			
			if (pg_rec->exclusions)
				SetMemorySize(pg_rec->exclusions, 0);

			if (wParam)
				RepaintControl(hWnd, pg_stuff, TRUE);

			break;

		case PG_POINTINCONTAINER:
		case PG_POINTINEXCLUSION:
			{
				co_ordinate			point;

				SplitIntoLongs(lParam, &point.v, &point.h);
				
				if (message == PG_POINTINCONTAINER)
					response = (long)pgPtInContainer(pg_stuff->pg, &point, NULL);
				else
					response = (long)pgPtInExclusion(pg_stuff->pg, &point, NULL);
			}
			
			break;

		case PG_SETEXCLUSIONDATA:
			pgSetExclusionRefCon(pg_stuff->pg, (pg_short_t)wParam, lParam);
			break;

		case PG_GETEXCLUSIONDATA:
			response = pgGetExclusionRefCon(pg_stuff->pg, (pg_short_t)wParam);
			break;

		case PG_SWAPEXCLUSIONS:
			{
				pg_short_t			box1, box2;
				
				box1 = (pg_short_t)HIWORD(lParam);
				box2 = (pg_short_t)LOWORD(lParam);
				pgSwapExclusions(pg_stuff->pg, box1, box2, draw_mode);
			}
			
			break;
		
		case PG_SETPAGEINDENT:
			pg_rec = UseMemory(pg_stuff->pg);
			pg_rec->doc_info.margins.top_left.h = pg_rec->doc_info.margins.top_left.v
					= pg_rec->doc_info.margins.bot_right.h
					= pg_rec->doc_info.margins.bot_right.v = lParam;
			
			if (lParam == 0)
				pg_rec->doc_info.attributes &= (~USE_MARGINS_BIT);
			else
				pg_rec->doc_info.attributes |= USE_MARGINS_BIT;

			pgInvalSelect(pg_stuff->pg, 0, pg_rec->t_length);

			UnuseMemory(pg_stuff->pg);
			break;

		default:
			response = DefWindowProc(hWnd, message, wParam, lParam);
			break;
	}

	UnuseMemory(pg_stuff_ref);

	return	response;
}


/************************************* Local Functions ***********************************/


/* append_shape appends a new rect to the end of the page area or exclusion
area (meant for exclusion if for_exclusion = TRUE). If merge is TRUE then we
attempt to merge the new rectangle with the shape (as opposed to merely
appending it). */

static void append_shape (paige_control_ptr pg_stuff, PAIGERECTSTRUCT PG_FAR *new_data,
		int merge, int for_exclusion)
{
	paige_rec_ptr			pg_rec;
	rectangle				rect;
	
	pg_rec = UseMemory(pg_stuff->pg);
	pgBlockMove(&new_data->top, &rect, sizeof(rectangle));

	if (for_exclusion)
		pgInsertExclusion(pg_stuff->pg, &rect, pgNumExclusions(pg_stuff->pg), 0, draw_none);
	else {
	
		if (merge)
			pgMergeRectToShape(pg_rec->wrap_area, &rect);
		else
			pgAddRectToShape(pg_rec->wrap_area, &rect);

		InvalidateShape(pg_rec, FALSE);
	}

	UnuseMemory(pg_stuff->pg);
}


/* establish_containers set sup "containers mode." */

static void establish_containers (paige_control_ptr pg_stuff, PAIGERECTSTRUCT PG_FAR *new_data)
{
	paige_rec_ptr			pg_rec;
	rectangle				rect;
	
	pg_rec = UseMemory(pg_stuff->pg);
	pgBlockMove(&new_data->top, &rect, sizeof(rectangle));
	pgSetShapeRect(pg_rec->wrap_area, &rect);
	pgInvalSelect(pg_rec->myself, 0, pg_rec->t_length);
	pg_rec->port.clip_info.change_flags |= CLIP_PAGE_CHANGED;

	UnuseMemory(pg_stuff->pg);
	
	pg_stuff->flags |= HAS_CONTAINERS;
	SetExtendedStyleBits(pg_stuff, pg_stuff->ex_styles);
}



/* get_shape_rect returns the r_num element rect within the page shape
or exclusion area (if from_exclusion = TRUE). Note that r_num is 1-based. */

static void get_shape_rect (paige_control_ptr pg_stuff, short r_num,
		PAIGERECTSTRUCT PG_FAR *data, int from_exclusion)
{
	paige_rec_ptr			pg_rec;
	shape_ref				ref;
	
	pg_rec = UseMemory(pg_stuff->pg);
	
	if (from_exclusion)
		ref = pg_rec->exclude_area;
	else
		ref = pg_rec->wrap_area;
	
	pgFillBlock(data, sizeof(PAIGERECTSTRUCT), 0);

	if ((long)r_num < GetMemorySize(ref)) {

		GetMemoryRecord(ref, r_num, (rectangle_ptr)&data->top);
		data->screen_y = -pg_rec->scroll_pos.v;
		data->screen_x = -pg_rec->scroll_pos.h;
	}

	UnuseMemory(pg_stuff->pg);
}



/* delete_shape_rect removes the r_num element rectangle from the shape
specified. */

static void delete_shape_rect (paige_control_ptr pg_stuff, pg_short_t r_num, int from_exclusion)
{
	paige_rec_ptr			pg_rec;
	pg_short_t				shape_size;
	
	pg_rec = UseMemory(pg_stuff->pg);
	
	if (from_exclusion)
		pgRemoveExclusion(pg_stuff->pg, r_num, draw_none);
	else {
	
		shape_size = (pg_short_t)GetMemorySize(pg_rec->wrap_area);
	
		if (r_num < shape_size) {
		
			if (shape_size > 2) {
			
				DeleteMemory(pg_rec->wrap_area, r_num, 1);
				pgResetBounds(pg_rec->wrap_area);
				InvalidateShape(pg_rec, from_exclusion);
			}
		}
    }
    
	UnuseMemory(pg_stuff->pg);
}


/* modify_shape_rect changes the r_num element rectangle from the shape
specified. */

static void modify_shape_rect (paige_control_ptr pg_stuff, pg_short_t r_num,
		PAIGERECTSTRUCT PG_FAR *container, int from_exclusion)
{	
	if (from_exclusion)
		pgReplaceExclusion(pg_stuff->pg, (rectangle_ptr)&container->top, r_num, draw_none);
	else
		pgReplaceContainer(pg_stuff->pg, (rectangle_ptr)&container->top, r_num, draw_none);
}

