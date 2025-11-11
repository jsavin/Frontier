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
#include "pgEmbed.h"


static void set_embed_info (pg_ref pg, embed_ref ref, PAIGEOBJECTSTRUCT PG_FAR *info, long offset);

long pascal EmbedMessageProc (HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	paige_control_ptr			pg_stuff;			// One of these in each control
	pg_embed_ptr				embed_ptr;			// Embed record I can zap
	memory_ref					pg_stuff_ref;		// A memory_ref that holds above
	PAIGEOBJECTSTRUCT PG_FAR	*obj_data;  		// Struct for inserting an object
	PAIGERECTSTRUCT PG_FAR		*bounds_data;
	paige_rec_ptr				pg_rec;
	rectangle					userbox;
	void PG_FAR					*the_data;
	metafile_struct				metafile;
	style_info					style, mask;
	select_pair					full_range;
	embed_ref					ref;
	pg_fixed					vertical_pos;
	long						response, the_type, offset;
	short						draw_mode, baseline;

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

		case PG_INSERTOBJECT:
		case PG_APPLYOBJECT:
		    obj_data = (PAIGEOBJECTSTRUCT PG_FAR *)lParam;
		    the_data = (void PG_FAR *)obj_data->data;
		    
		    vertical_pos = obj_data->descent;
		    
		    if (obj_data->descent < 0)
		    	vertical_pos = -vertical_pos;
		    vertical_pos <<= 16;
		    
		    if (obj_data->descent < 0)
		    	vertical_pos = -vertical_pos;

		    switch (obj_data->objType) {

				case PGEMBED_MACPICT:
					the_type = embed_mac_pict;
					break;

				case PGEMBED_METAFILE:
					pgFillBlock(&metafile, sizeof(metafile_struct), 0);
		    		metafile.metafile = obj_data->data;
		    		metafile.bounds.top_left.h = metafile.bounds.top_left.v = 0;
		    		metafile.bounds.bot_right.h = obj_data->width;
		    		metafile.bounds.bot_right.v = obj_data->height;
		    		metafile.mapping_mode = obj_data->mapping_mode;
		    		metafile.x_ext = obj_data->xExt;
		    		metafile.y_ext = obj_data->yExt;

					the_data = (void PG_FAR *)&metafile;
					the_type = embed_meta_file;
					break;

				case PGEMBED_ALTCHAR:
					the_type = embed_alternate_char;
					break;
					
				case PGEMBED_ALTSTRING:
					the_type = embed_dynamic_string;
					break;
					
				case PGEMBED_USERDATA:
					the_type = embed_user_data;
					break;
					
				case PGEMBED_OLE:
					the_type = embed_ole;
					break;
					
				case PGEMBED_USERBOX:
					the_type = embed_user_box;
					userbox.top_left.h = userbox.top_left.v = 0;
					userbox.bot_right.h = obj_data->width;
					userbox.bot_right.v = obj_data->height;
					the_data = (void PG_FAR *)&userbox;
					
					break;
			}

		    ref = pgNewEmbedRef(&mem_globals, the_type, the_data, obj_data->dataModifier,
		    		obj_data->flags, vertical_pos, obj_data->refCon, FALSE);
            embed_ptr = UseMemory(ref);
            embed_ptr->empty_width = obj_data->emptyWidth;
			embed_ptr->top_extra = obj_data->topExtra;
			embed_ptr->left_extra = obj_data->leftExtra;
			embed_ptr->right_extra = obj_data->rightExtra;
			embed_ptr->bot_extra = obj_data->botExtra;
			
			obj_data->ref_var = (long)ref;

            if (obj_data->objType == PGEMBED_USERDATA ||
                obj_data->objType == PGEMBED_OLE) { // PDA:
            	
            	embed_ptr->width = obj_data->width;
            	embed_ptr->height = obj_data->height;
            }

            UnuseMemory(ref);
 
			if (pg_stuff->undo)
				pgDisposeUndo(pg_stuff->undo);
            
            pg_stuff->undo = MEM_NULL;
 
			pg_stuff->notification |= PGN_EMBEDPROCFLAG;
			pg_rec = UseMemory(pg_stuff->pg);
			pg_rec->notification |= PGN_EMBEDPROCFLAG;
			UnuseMemory(pg_stuff->pg);

		    if (message == PG_INSERTOBJECT) {
				
				if (!(pg_stuff->ex_styles & PGS_NOAUTOUNDO))
					pg_stuff->undo = pgPrepareUndo(pg_stuff->pg, undo_embed_insert, NULL);

			    pgInsertEmbedRef(pg_stuff->pg, ref, obj_data->position, (short)obj_data->param1, EmbedCallbackProc,
			    		obj_data->callbackRefCon, draw_mode);
			}
			else {
				
				if (!(pg_stuff->ex_styles & PGS_NOAUTOUNDO))
					pg_stuff->undo = pgPrepareUndo(pg_stuff->pg, undo_format, NULL);

				pgSetEmbedRef(pg_stuff->pg, ref, NULL, (short)obj_data->param1, EmbedCallbackProc, obj_data->callbackRefCon, draw_mode);
			}

		    SendChangeCommand(pg_stuff);

			if (draw_mode)
				NotifyOverflow(pg_stuff);

			break;

		case PG_NUMOBJECTS:
			response = pgNumEmbeds(pg_stuff->pg, (select_pair_ptr)lParam);
			break;

		case PG_GETOBJECTINFO:
		case PG_SETOBJECTINFO:
			obj_data = (PAIGEOBJECTSTRUCT PG_FAR *)lParam;
			full_range.begin = 0;
			full_range.end = pgTextSize(pg_stuff->pg);
			
			if ((ref = pgGetIndEmbed(pg_stuff->pg, &full_range, wParam, &offset, &style)) != MEM_NULL) {
				
				if (message == PG_GETOBJECTINFO)
					FillEmbedInfo(ref, obj_data, offset, &style);
				else {
				
					set_embed_info(pg_stuff->pg, ref, obj_data, offset);
					NotifyOverflow(pg_stuff);
				}
				
				response = TRUE;
			}
			
			break;

		case PG_GETOBJECTBOUNDS:
			bounds_data = (PAIGERECTSTRUCT PG_FAR *)lParam;
			full_range.begin = 0;
			full_range.end = pgTextSize(pg_stuff->pg);
			
			bounds_data->query = pgGetEmbedBounds(pg_stuff->pg, wParam, &full_range, (rectangle_ptr)&bounds_data->top,
						&baseline, (co_ordinate_ptr)&bounds_data->screen_x);
			bounds_data->baseline = (long)baseline;

			break;

		case PG_SETOBJECTBOUNDS:
			bounds_data = (PAIGERECTSTRUCT PG_FAR *)lParam;
			full_range.begin = 0;
			full_range.end = pgTextSize(pg_stuff->pg);
			baseline = (short)bounds_data->baseline;

			pgSetEmbedBounds(pg_stuff->pg, wParam, &full_range, (rectangle_ptr)&bounds_data->top,
						&baseline, pg_stuff->update_mode);
          	break;
 
		case PG_PTINOBJECT:
			{
				co_ordinate			point;
				
				SplitIntoLongs(lParam, &point.v, &point.h);
				
				if ((ref = pgPtInEmbed(pg_stuff->pg, &point, NULL, &style, FALSE)) != MEM_NULL)
					response = pgEmbedStyleToIndex(pg_stuff->pg, &style);
			}

			break;

		case PG_FINDOBJECT:
			/* For this message, a PAIGEOBJECTSTRUCT pointer is given in lParam. The last three
			params in the struct must be initialized for this search. */

			obj_data = (PAIGEOBJECTSTRUCT PG_FAR *)lParam;
			
			if ((ref = pgFindNextEmbed(pg_stuff->pg, &obj_data->position, obj_data->callbackRefCon, obj_data->param1)) != MEM_NULL) {
				
				pg_rec = UseMemory(pg_stuff->pg);
				embed_ptr = UseMemory(ref);
				GetMemoryRecord(pg_rec->t_formats, embed_ptr->lowlevel_index, &style);
				UnuseMemory(ref);
				UnuseMemory(pg_stuff->pg);
				response = pgEmbedStyleToIndex(pg_stuff->pg, &style);
			}

			break;

		case PG_REMOVEOBJECT:
			full_range.begin = 0;
			full_range.end = pgTextSize(pg_stuff->pg);

			if ((ref = pgGetIndEmbed(pg_stuff->pg, &full_range, wParam, &offset, &style)) != MEM_NULL) {

				full_range.begin = offset;
				full_range.end = full_range.begin + 2;
				
				if (lParam)
			    	draw_mode = pg_stuff->update_mode;
			    else
			       draw_mode = draw_none;

			    pgDelete(pg_stuff->pg, &full_range, best_way);
			}

			break;

		case PG_GETSELOBJECTINFO:
			obj_data = (PAIGEOBJECTSTRUCT PG_FAR *)lParam;
			pgFillBlock(obj_data, sizeof(PAIGEOBJECTSTRUCT), 0);

			if (wParam) {
			
				full_range.begin = obj_data->param1;
				full_range.end = obj_data->param2;
			}
			else
				pgGetSelection(pg_stuff->pg, &full_range.begin, &full_range.end);

			pgGetStyleInfo(pg_stuff->pg, &full_range, FALSE, &style, &mask);
			
			if (mask.embed_object && style.embed_object) {
				
				response = TRUE;
				FillEmbedInfo((memory_ref)style.embed_object, obj_data, full_range.begin, &style);
			}
			else
				pgFillBlock(obj_data, sizeof(PAIGEOBJECTSTRUCT), 0);
			
			break;
		
		case PG_SETSELOBJECTINFO:
			obj_data = (PAIGEOBJECTSTRUCT PG_FAR *)lParam;
			set_embed_info(pg_stuff->pg, (embed_ref)obj_data->ref_var, obj_data, obj_data->position);
			
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



/* EmbedCallbackProc is the single entry point for all embed_ref callbacks. */

extern PG_PASCAL (long) EmbedCallbackProc (paige_rec_ptr pg, pg_embed_ptr embed_ptr,
		long embed_type, short command, long user_refcon,
		long param1, long param2)
{
	PAIGEOBJECTSTRUCT		data;
	PAIGEMOUSESTRUCT		mouse_data;
	PAIGEMOUSESTRUCT PG_FAR	*mouse_ptr;
	paige_control_ptr		pg_stuff;
	rectangle_ptr			bounds;
	pg_embed_click_ptr		click_ptr;
	long					return_value, callback_response, notify_flags;

   // PDA:  Deleted some code here!

    return_value = 0;
    
    if (command == EMBED_INIT || !pg)
    	return	0L;
 	
 	notify_flags = pg->notification;
  
	if (!(notify_flags & PGN_EMBEDPROCFLAG))
		return	pgDefaultEmbedCallback(pg, embed_ptr, embed_type, command, user_refcon,
				param1, param2);

	pg_stuff = UseMemory(pg->custom_control);

	data.objType = LOWORD(embed_ptr->type);
	data.flags = embed_ptr->type & 0xFFFF0000;
	data.width = (short)embed_ptr->width;
	data.height = (short)embed_ptr->height;
	data.leftExtra = (short)embed_ptr->left_extra;
	data.topExtra = (short)embed_ptr->top_extra;
	data.rightExtra = (short)embed_ptr->right_extra;
	data.botExtra = (short)embed_ptr->bot_extra;
	data.embedPtr = (long)embed_ptr;
	data.editWindow = (HWND)pg_stuff->myself;
	data.minWidth = (short)embed_ptr->minimum_width;
	data.emptyWidth = embed_ptr->empty_width;
	data.ref_var = 0;
	
	if (pg->flags & PRINT_MODE_BIT)
		data.flags |= PGOBJFLAG_PRINTING;

	data.command = command;
	data.descent = embed_ptr->descent;
	data.data = (long)embed_ptr->data;
	pgBlockMove(embed_ptr->uu.alt_data, data.altData, sizeof(pg_char) * ALT_SIZE);
	data.dataModifier = embed_ptr->modifier;
	data.refCon = embed_ptr->user_refcon;
	data.position = data.param1 = data.param2 = 0;

	if (embed_ptr->style) {
		
		data.descent += embed_ptr->style->bot_extra;
		data.callbackRefCon = embed_ptr->style->embed_style_refcon;
	}
	
	data.hDC = (HDC)NULL;

	if (pg)
		if (pg->globals->current_port)
			data.hDC = (HDC) pg->globals->current_port->machine_ref;

	data.param1 = param1;
	data.param2 = param2;
   	
   	if (data.objType == embed_user_data && embed_ptr->data)
   		data.data = (long)UseMemory((memory_ref)embed_ptr->data);

    switch (command) {
    	
    	case	EMBED_MOUSEDOWN:
    	case	EMBED_MOUSEMOVE:
    	case	EMBED_MOUSEUP:
    	case	EMBED_DOUBLECLICK:

			click_ptr = (pg_embed_click_ptr)param1;
			bounds = &click_ptr->bounds;
			mouse_ptr = &mouse_data;

			data.param2 = (long)bounds;
			data.param1 = (long)mouse_ptr;

			mouse_data.yMouse = click_ptr->point.v;
			mouse_data.xMouse = click_ptr->point.h;
			mouse_data.wParam = pg_stuff->wParam;
			mouse_data.area = pg_stuff->mouse_loc | PGPT_INEMBED;
			mouse_data.dblClick = mouse_data.trplClick = 0;
		
			if (command == EMBED_DOUBLECLICK)
				mouse_data.dblClick = TRUE;
		
			mouse_data.firstPos = click_ptr->first_select->offset;
			mouse_data.endPos = click_ptr->last_select->offset;
		
			break;
		
       case EMBED_READ_DATA:
       		 // Param1 is given to me as a pointer and param2 as its size.
       		 // Note, I do nothing because these params are already set!
  
       		break;
       		
       case EMBED_WRITE_DATA:
       		//Param1 is given to me as a memory_ref to fill in.
       		//Param2 means nothing.

       		break;       		
    }
    
	if ((callback_response = SendNotification(pg_stuff, PGN_EMBEDPROC, 0, &data)) != 0) {
 
		if (command == EMBED_SWAP)
			return_value = callback_response;
		else
		if (command == EMBED_WRITE_DATA) {
        	long		data_size;


      #ifdef MAC_PLATFORM
        	if ((data_size = GetHandleSize((Handle)callback_response)) != 0) {
        		Handle				data;
        		
        		data = (Handle)callback_response;
        		SetMemorySize((memory_ref)param1, data_size);
        		HLock(data);
        		pgBlockMove(data, UseMemory((memory_ref)param1), data_size);
        		HUnlock(data);
        		DisposeHandle(data);
        		UnuseMemory((memory_ref)param1);
        	}
      #else
        	if (data_size = GlobalSize((HANDLE)callback_response)) {
        	
        		SetMemorySize((memory_ref)param1, data_size);
        		pgBlockMove(GlobalLock((HANDLE)callback_response), UseMemory((memory_ref)param1), data_size);
        		GlobalUnlock((HANDLE)callback_response);
        		GlobalFree((HANDLE)callback_response);
        		UnuseMemory((memory_ref)param1);
        	}
       #endif

		}
		else
		if (command == EMBED_MEASURE)
			embed_ptr->width = data.width;
	}
	else
		return_value = pgDefaultEmbedCallback(pg, embed_ptr, embed_type, command, user_refcon, param1, param2);
  	
  	if (command != EMBED_DESTROY)
 		if (data.objType == embed_user_data && embed_ptr->data)
 			UnuseMemory((memory_ref)embed_ptr->data);
 
	UnuseMemory(pg->custom_control);

	return	return_value;
}


/* FillEmbedInfo initializes the users data with the embed_ref info. */

void pascal FillEmbedInfo (embed_ref ref, PAIGEOBJECTSTRUCT PG_FAR *info,
		long offset, style_info_ptr style)
{
	pg_embed_ptr			embed_ptr;
	short					the_type;

	embed_ptr = UseMemory(ref);
	the_type = (short)(embed_ptr->type & EMBED_TYPE_MASK);

	info->objType = the_type;
	info->data = (long)embed_ptr->data;
	info->width = (short)embed_ptr->width;
	info->height = (short)embed_ptr->height;
	info->dataModifier = embed_ptr->modifier;
	info->emptyWidth = embed_ptr->empty_width;
	info->refCon = embed_ptr->user_refcon;
	info->descent = embed_ptr->descent + style->bot_extra;
	info->leftExtra = embed_ptr->left_extra;
	info->rightExtra = embed_ptr->right_extra;
	info->minWidth = (short)embed_ptr->minimum_width;

	info->callbackRefCon = style->embed_style_refcon;
	info->position = offset;
	info->ref_var = (long)ref;
	pgBlockMove(embed_ptr->uu.alt_data, info->altData, sizeof(pg_char) * ALT_SIZE);

	info->alternate = 0;
	UnuseMemory(ref);
}


/************************************* Local Functions ****************************************/


/* set_embed_info changes information in the ref (except volatile members) */

static void set_embed_info (pg_ref pg, embed_ref ref, PAIGEOBJECTSTRUCT PG_FAR *info, long offset)
{
	paige_rec_ptr			pg_rec;
	style_info_ptr			style;
	font_info_ptr			font;
	pg_embed_ptr			embed_ptr;
	long					old_descent;
	int						changed;

	pg_rec = UseMemory(pg);
	embed_ptr = UseMemory(ref);
	
	if (offset >= 0)
		style = pgFindTextStyle(pg_rec, offset);
	else {
		long				num_styles;
		
		style = UseMemory(pg_rec->t_formats);
		num_styles = GetMemorySize(pg_rec->t_formats);
		
		while (num_styles) {
			
			if (style->embed_object == (memory_ref)ref)
				break;
			
			++style;
			--num_styles;
		}
		
		if (num_styles == 0) {
			
			UnuseMemory(pg_rec->t_formats);
			return;
		}
	}

	style->embed_style_refcon = info->callbackRefCon;

	old_descent = embed_ptr->descent + style->bot_extra;

	if ((changed = (info->descent != old_descent)) != FALSE) {
		
		style->bot_extra += (info->descent - old_descent);
		
		if ((style->bot_extra + style->descent) < 0)
			style->bot_extra = -style->descent;
	}

	changed |= (int)(embed_ptr->height != info->height);
	changed |= (int)(embed_ptr->user_refcon != info->refCon);
	changed |= (int)(style->embed_style_refcon != info->callbackRefCon);
	changed |= (int)(embed_ptr->left_extra != info->leftExtra);
	changed |= (int)(embed_ptr->right_extra != info->rightExtra);
	changed |= (int)(embed_ptr->empty_width != info->emptyWidth);

	embed_ptr->height = info->height;
	embed_ptr->user_refcon = info->refCon;
	embed_ptr->empty_width = info->emptyWidth;
	style->embed_style_refcon = info->callbackRefCon;
	embed_ptr->left_extra = info->leftExtra;
	embed_ptr->right_extra = info->rightExtra;

	if (changed) {
		
		if (offset >= 0) {
			
			font = UseMemoryRecord(pg_rec->fonts, style->font_index, 0, TRUE);
			pgEmbedStyleInit(pg_rec, style, font);
			UnuseMemory(pg_rec->fonts);
			
			pgInvalSelect(pg, offset, offset + style->char_bytes);
		}
	}

	UnuseMemory(ref);
	UnuseMemory(pg_rec->t_formats);

	UnuseMemory(pg);
}




