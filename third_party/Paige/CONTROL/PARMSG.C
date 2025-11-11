/* This source file handles all the PAIGE messages dealing with paragraph formats and tabs.
It handles all "PG_PARMESSAGES" messages. */


#include "Paige.h"
#include "pgTraps.h"
#include "pgUtils.h"
#include "defprocs.h"
#include "pgCtlUtl.h"
#include "pgCntl.h"


long pascal ParMessageProc (HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	paige_control_ptr		pg_stuff;			// One of these in each control
	memory_ref				pg_stuff_ref;		// A memory_ref that holds above
	memory_ref				the_tabs, tabmask;
	par_info				par, parmask;
	long					response;
	short					draw_mode;

	response = 0;
	pg_stuff = GetPGStuff(hWnd, &pg_stuff_ref);
	
	if (!pg_stuff)
		return	response;

/* The following sets the "draw_mode" for all functions in which wParam
is TRUE or FALSE for re-draw. If wParam is used for something else it
does not matter because we will not use "draw_mode" variable. */

    if (wParam)
    	draw_mode = pg_stuff->update_mode;
    else
       draw_mode = draw_none;

	switch (message) {

		case PG_SETINDENT:
			pgFillBlock(&parmask, sizeof(par_info), 0);
			PrepareStyleUndo(pg_stuff);
			
			switch (LOWORD(lParam)) {
				
				case PGI_LEFT:
					par.indents.left_indent = HIWORD(lParam);
					parmask.indents.left_indent = -1;
					break;
					
				case PGI_RIGHT:
					par.indents.right_indent = HIWORD(lParam);
					parmask.indents.right_indent = -1;
					break;

				case PGI_FIRSTLINE:
					par.indents.first_indent = (signed short)HIWORD(lParam);
					parmask.indents.first_indent = -1;
					break;

				default:
					par.indents.left_indent = HIWORD(lParam);
					parmask.indents.left_indent = -1;
					break;
			}
			
			pgSetParInfo(pg_stuff->pg, NULL, &par, &parmask, draw_mode);
			
			if (draw_mode)
				NotifyOverflow(pg_stuff);

			break;

		case PG_GETINDENT:
			pgGetParInfo(pg_stuff->pg, NULL, FALSE, &par, &parmask);
			
			switch (LOWORD(lParam)) {
				
				case PGI_LEFT:
					response = par.indents.left_indent;
					break;
					
				case PGI_RIGHT:
					response = par.indents.right_indent;
					break;

				case PGI_FIRSTLINE:
					response = par.indents.first_indent;
					break;

				default:
					response = par.indents.left_indent;
					break;
			}
			
			break;

		case PG_SETJUSTIFICATION:
			pgFillBlock(&parmask, sizeof(par_info), 0);
			parmask.justification = -1;
			par.justification = (short)lParam;
			PrepareStyleUndo(pg_stuff);
			pgSetParInfo(pg_stuff->pg, NULL, &par, &parmask, draw_mode);

			if (draw_mode)
				NotifyOverflow(pg_stuff);
			break;
			
		case PG_GETJUSTIFICATION:
			pgGetParInfo(pg_stuff->pg, NULL, FALSE, &par, &parmask);
			response = par.justification;
			break;

		case PG_SETLINESPACING:
			pgFillBlock(&parmask, sizeof(par_info), 0);
			parmask.spacing = -1;
			par.spacing = lParam;
			PrepareStyleUndo(pg_stuff);			
			pgSetParInfo(pg_stuff->pg, NULL, &par, &parmask, draw_mode);

			if (draw_mode)
				NotifyOverflow(pg_stuff);
			break;

		case PG_GETLINESPACING:
			pgGetParInfo(pg_stuff->pg, NULL, FALSE, &par, &parmask);
			response = par.spacing;
			break;

		case PG_GETPARTOPEXTRA:
			pgGetParInfo(pg_stuff->pg, NULL, FALSE, &par, &parmask);
			response = par.top_extra;
			break;

		case PG_SETPARTOPEXTRA:
			pgFillBlock(&parmask, sizeof(par_info), 0);
			parmask.top_extra = -1;
			par.top_extra = lParam;
			pgSetParInfo(pg_stuff->pg, NULL, &par, &parmask, draw_mode);

			if (draw_mode)
				NotifyOverflow(pg_stuff);
			break;

		case PG_GETPARBOTEXTRA:
			pgGetParInfo(pg_stuff->pg, NULL, FALSE, &par, &parmask);
			response = par.bot_extra;
			break;
			
		case PG_SETPARBOTEXTRA:
			pgFillBlock(&parmask, sizeof(par_info), 0);
			parmask.bot_extra = -1;
			par.bot_extra = lParam;
			pgSetParInfo(pg_stuff->pg, NULL, &par, &parmask, draw_mode);

			if (draw_mode)
				NotifyOverflow(pg_stuff);
			break;

		case PG_SETTAB:
			PrepareStyleUndo(pg_stuff);
			pgSetTab(pg_stuff->pg, NULL, (tab_stop_ptr)lParam, draw_mode);

			if (draw_mode)
				NotifyOverflow(pg_stuff);

			break;

		case PG_NUMTABS:
		case PG_GETTABLIST:
		case PG_GETTABLISTMASK:
			the_tabs = MemoryAlloc(&mem_globals, sizeof(tab_stop), 0, 2);
			
			if (message == PG_GETTABLISTMASK)
				tabmask = MemoryAlloc(&mem_globals, sizeof(long), 0, 2);
			else
				tabmask = MEM_NULL;

			pgGetTabList(pg_stuff->pg, NULL, the_tabs, tabmask, NULL);

			if ((response = GetMemorySize(the_tabs)) != 0) {

				if (message == PG_GETTABLIST) {
					
					pgBlockMove(UseMemory(the_tabs), (void PG_FAR *)lParam, response * sizeof(tab_stop));
					UnuseMemory(the_tabs);
				}
				else
				if (message == PG_GETTABLISTMASK) {

					pgBlockMove(UseMemory(tabmask), (void PG_FAR *)lParam, response * sizeof(long));
					UnuseMemory(tabmask);
				}
			}
			
			DisposeMemory(the_tabs);
			
			if (tabmask)
				DisposeMemory(tabmask);

			break;

		case PG_SETPARSTOGETHER:
			pgFillBlock(&parmask, sizeof(par_info), 0);
			parmask.class_info = -1;

			if (lParam)
				par.class_info |= KEEP_PARS_TOGETHER;
			else
				par.class_info &= (~KEEP_PARS_TOGETHER);
			
			pgSetParInfo(pg_stuff->pg, NULL, &par, &parmask, draw_mode);

			if (draw_mode)
				NotifyOverflow(pg_stuff);

			break;
			
		case PG_GETPARSTOGETHER:
			pgGetParInfo(pg_stuff->pg, NULL, FALSE, &par, &parmask);
			response = ((par.class_info & KEEP_PARS_TOGETHER) != 0);
			break;

		default:
			response = DefWindowProc(hWnd, message, wParam, lParam);
			break;
	}

	UnuseMemory(pg_stuff_ref);

	return	response;
}


