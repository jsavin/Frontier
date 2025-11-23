/* This source file handles all the PAIGE messages dealing with file I/O and printing.
It handles all "PG_IOMESSAGES" messages. */


#include "Paige.h"
#include "pgTraps.h"
#include "pgUtils.h"
#include "defprocs.h"
#include "pgCtlUtl.h"
#include "pgCntl.h"
#include "pgFiles.h"
#include "pgEmbed.h"
#include "pgText.h"

#ifdef MAC_PLATFORM
#ifndef __PRINTTRAPS__
#include <Printing.h>
#endif
#endif

#ifdef WINDOWS_PLATFORM
PG_PASCAL(void ) rtf_set_hinst(HINSTANCE hi);
#endif

#ifdef DO_RTF

#ifdef RTF_PURCHASED
#include "pgRTF.h"
#define DEMO_COMMENT_1	"Created by Paige/RTF DLL"
#define DEMO_COMMENT_2	"Contact DSI (76424.3027@compuserve.com or OITC (oitc@iu.net)"
#define DEMO_COMMENT_3	"Paige, RTF, Word Processing, DSI, OITC"
#endif

#include "pgrtf.h"
#include "tdchrset.h"

pg_error rtf_idle(pg_rtf_trans_state_ptr state);
void rtf_apply_doc (pg_rtf_trans_state_ptr state);
static long do_rtf_import(paige_control_ptr pg_stuff, short f_ref);

static HINSTANCE hInst = 0;

#endif


#define DOCUMENT_INFO_KEY	CUSTOM_HANDLER_KEY
#define CONTROL_HANDLER_KEY	(CUSTOM_HANDLER_KEY + 1)

static long save_document (paige_control_ptr pg_stuff, pg_file_unit f_ref, long position,
		int save_text_only, int cache_write, memory_ref scrap_memory);
static long read_document (paige_control_ptr pg_stuff, pg_file_unit f_ref, long position,
		int read_text_only, int verify_only, int cache_read, memory_ref scrap_memory);
static void print_next_page (paige_control_ptr pg_stuff, PAIGEPRINTSTRUCT PG_FAR *print_info);
static void set_print_defaults (paige_control_ptr pg_stuff, PAIGEPRINTSTRUCT PG_FAR *print_info);


STATIC_PASCAL (pg_boolean) control_read_handler (paige_rec_ptr pg, pg_file_key key, memory_ref key_data,
		long PG_FAR *element_info, void PG_FAR *aux_data, long PG_FAR *unpacked_size);
STATIC_PASCAL (pg_boolean) doc_read_handler (paige_rec_ptr pg, pg_file_key key, memory_ref key_data,
		long PG_FAR *element_info, void PG_FAR *aux_data, long PG_FAR *unpacked_size);
STATIC_PASCAL (pg_boolean) doc_write_handler (paige_rec_ptr pg, pg_file_key key, memory_ref key_data,
		long PG_FAR *element_info, void PG_FAR *aux_data, long PG_FAR *unpacked_size);


long pascal IOMessageProc (HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	PAIGEPRINTSTRUCT PG_FAR *print_info;
	PAIGEIOSTRUCT PG_FAR	*io_data;
	paige_control_ptr		pg_stuff;			// One of these in each control
	paige_rec_ptr			pg_rec;
	style_info_ptr			styles;
	memory_ref				pg_stuff_ref;		// A memory_ref that holds above
	embed_callback			embed_proc;
	long					response, filepos, num_styles;
	short					draw_mode;
	int						text_verb;

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

		case PG_SAVEDOC:
		case PG_CACHESAVEDOC:
			response = (long)save_document(pg_stuff, (pg_file_unit)wParam, (long)lParam, FALSE,
					(int)(message == PG_CACHESAVEDOC), MEM_NULL);
			
			if (response >= 0) {
				
				pgSetChangeCtr(pg_stuff->pg, 0);
				pg_stuff->changes = 0;
			}
			
			break;

		case PG_SAVETEXT:
		case PG_SAVEUNICODETEXT:
			if (message == PG_SAVETEXT)
				text_verb = SAVE_FILE_AS_TEXT;
			else
				text_verb = SAVE_FILE_AS_UNICODE;

			response = (long)save_document(pg_stuff, (pg_file_unit)wParam, (long)lParam, text_verb, FALSE, MEM_NULL);
			break;

		case PG_READDOC:
		case PG_CACHEREADDOC:
			response = (long)read_document(pg_stuff, (pg_file_unit)wParam, (long)lParam, FALSE, FALSE,
					(int)(message == PG_CACHEREADDOC), MEM_NULL);

			if (response >= 0) {
				
				pgSetChangeCtr(pg_stuff->pg, 0);
				pg_stuff->changes = 0;
			}
			
			break;

		case PG_READTEXT:
			response = (long)read_document(pg_stuff, (pg_file_unit)wParam, (long)lParam, TRUE, FALSE, FALSE, MEM_NULL);
			break;

		case PG_SETREADHANDLER:
			pgSetHandler(&paige_globals, (pg_file_key)(wParam + CONTROL_HANDLER_KEY), control_read_handler,
					NULL, NULL, NULL);
			break;
		
		case PG_VERIFYFILE:
			response = (long)read_document(pg_stuff, (pg_file_unit)wParam, (long)lParam, FALSE, TRUE, FALSE, MEM_NULL);
			break;
			
		case PG_WRITECUSTOMDATA: {
				memory_ref		ref;
				short PG_FAR	*the_file;
				
				ref = MemoryAlloc(&mem_globals, sizeof(pg_file_unit), 1, 0);
				the_file = UseMemory(ref);
				*the_file = (short)wParam;
				UnuseMemory(ref);
	
				io_data = (PAIGEIOSTRUCT PG_FAR *)lParam;
				filepos = io_data->position;
				response = pgWriteKeyData(pg_stuff->pg, CONTROL_HANDLER_KEY,
					(void PG_FAR *)io_data->data, io_data->dataSize, io_data->refCon,
					NULL, NULL, &filepos, ref);
				
				DisposeMemory(ref);

				if (!response)
					response = filepos - io_data->position;
			}
			
			break;

		case PG_PRINTPAGE:
			print_info = (PAIGEPRINTSTRUCT PG_FAR *)lParam;
			print_next_page(pg_stuff, print_info);
			response = (print_info->from <= print_info->to);
			break;
		
		case PG_PRINTDEFAULTS:
			print_info = (PAIGEPRINTSTRUCT PG_FAR *)lParam;
			set_print_defaults(pg_stuff, print_info);

			break;
        
        case PG_MATCHPRINTER: {
            	PAIGEPRINTSTRUCT	print_data;
            
            #ifdef MAC_PLATFORM
            	print_data.machineRef = (long)lParam;
			#endif
			#ifdef WINDOWS_PLATFORM
            	print_data.dc = (HDC)lParam;
            #endif
            	print_info = &print_data;
        		set_print_defaults(pg_stuff, print_info);
        		RectToRectangle(&print_info->page, &pg_stuff->page_rect);
        	    
        		if (SetPageArea(pg_stuff))
        			if (wParam)
        				RepaintControl(hWnd, pg_stuff, TRUE);
        	}
        	
        	break;
        
        case PG_SETPRINTERDC:
        	pgSetPrintDevice(pg_stuff->pg, (generic_var)lParam);
        		if (wParam)
        			RepaintControl(hWnd, pg_stuff, TRUE);
			break;

        case PG_GETPRINTERDC:
        	response = (long)pgGetPrintDevice(pg_stuff->pg);
        	break;

		case PG_SAVETOMEMORY:
			response = (long)save_document(pg_stuff, 0, (long)wParam, FALSE, FALSE, lParam);
			
			if (response >= 0) {
				
				pgSetChangeCtr(pg_stuff->pg, 0);
				pg_stuff->changes = 0;
			}
			break;
		
		case PG_VERIFYMEMORYFILE:
		case PG_READFROMMEMORY:
			response = (long)read_document(pg_stuff, (pg_file_unit)0, (long)wParam, FALSE,
					(int)(message == PG_VERIFYMEMORYFILE), FALSE, lParam);

			if ((message == PG_READFROMMEMORY) && response >= 0) {
				
				pgSetChangeCtr(pg_stuff->pg, 0);
				pg_stuff->changes = 0;
			}

			break;
        
        case PG_REALIZEIMPORT:
        	pgAreaBounds(pg_stuff->pg, &pg_stuff->page_rect, &pg_stuff->vis_rect);
        	
        	if (!(pg_stuff->ex_styles & PGS_HIDEMARGINS)) {
        	
        		pg_stuff->page_rect.top_left.h -= pg_stuff->left_margin;
        		pg_stuff->page_rect.top_left.v -= pg_stuff->top_margin;
        		pg_stuff->page_rect.bot_right.h += pg_stuff->right_margin;
        		pg_stuff->page_rect.bot_right.v += pg_stuff->bottom_margin;   
        	}
        	
        	pgInsetRect(&pg_stuff->page_rect, -pg_stuff->page_pen_width, -pg_stuff->page_pen_width);
        	pgOffsetRect(&pg_stuff->page_rect, pg_stuff->vis_rect.top_left.h - pg_stuff->page_rect.top_left.h,
        								pg_stuff->vis_rect.top_left.v - pg_stuff->page_rect.top_left.v);
 
        	pgInsetRect(&pg_stuff->vis_rect, -pg_stuff->frame_pen_width, -pg_stuff->frame_pen_width);
        	
         	SetPageArea(pg_stuff);
        	
        	pg_rec = UseMemory(pg_stuff->pg);
        	styles = UseMemory(pg_rec->t_formats);
        	embed_proc = EmbedCallbackProc;

        	for (num_styles = GetMemorySize(pg_rec->t_formats); num_styles; ++styles, --num_styles) {
        		
        		if (styles->embed_object)
        			styles->embed_entry = (long)embed_proc;
        	}
        	
        	UnuseMemory(pg_rec->t_formats);
        	
        	if (wParam)
				RepaintControl(hWnd, pg_stuff, TRUE);

         	UpdateScrollbars((HWND)pg_stuff->myself, pg_stuff, FALSE);
        	break;

		default:
			response = DefWindowProc(hWnd, message, wParam, lParam);
			break;
	}

	UnuseMemory(pg_stuff_ref);

	return	response;
}


/************************************* Local Functions **********************************/


/* save_document saves the contents of PAIGE object to f_ref file, first byte
is written at position offset. If save_text_only is TRUE then raw text is
saved (otherwise all PAIGE items are saved in its native format). The function
result is how many bytes were saved. If unsuccessful we return -1.  */

static long save_document (paige_control_ptr pg_stuff, pg_file_unit f_ref, long position,
		int save_text_only, int cache_write, memory_ref scrap_memory)
{
	memory_ref			ref;
	file_io_proc		write_proc;
	long				file_position, data_size;
	short				result;
	pg_file_unit PG_FAR	*the_file;
	
	if (scrap_memory) {
	
		ref = scrap_memory;
		write_proc = pgScrapMemoryWrite;
	}
	else {
		
		write_proc = NULL;
		ref = MemoryAlloc(&mem_globals, sizeof(pg_file_unit), 1, 0);
		the_file = UseMemory(ref);
		*the_file = f_ref;
		UnuseMemory(ref);
	}

	file_position = position;

	if (save_text_only) {
		paige_rec_ptr			pg_rec;
		text_block_ptr			block;
		long					num_blocks;
	#ifdef UNICODE
		memory_ref				holding_ref;
		pg_char_ptr				holding_ptr;

		if (save_text_only == SAVE_FILE_AS_TEXT)
			holding_ref = MemoryAlloc(&mem_globals, sizeof(pg_bits8), 0, 0);
		else
			holding_ref = MEM_NULL;
	#endif

		pg_rec = UseMemory(pg_stuff->pg);
		num_blocks = GetMemorySize(pg_rec->t_blocks);
		block = UseMemory(pg_rec->t_blocks);
	
		while (num_blocks) {

			pg_rec->procs.load_proc(pg_rec, block);
			data_size = GetByteSize(block->text);
			
	#ifdef UNICODE

			if (save_text_only == SAVE_FILE_AS_UNICODE)
				result = pgStandardWriteProc((void PG_FAR *)block->text,
						io_data_indirect, &file_position, &data_size, ref);
			else {
				memory_ref		saved_ref;
				
				saved_ref = block->text;
				SetMemorySize(holding_ref, data_size);
				holding_ptr = UseMemory(holding_ref);
				pgBlockMove(UseMemory(block->text), holding_ptr, data_size);
				UnuseMemory(block->text);
				UnuseMemory(holding_ref);
				block->text = holding_ref;
				pgUnicodeToBlock(pg_rec, block);
				block->text = saved_ref;

				result = pgStandardWriteProc((void PG_FAR *)holding_ref,
						io_data_indirect, &file_position, &data_size, ref);
			}
	#else
			result = pgStandardWriteProc((void PG_FAR *)block->text,
					io_data_indirect, &file_position, &data_size, ref);
	#endif

			if (result != 0)
				break;
			
			++block;
			--num_blocks;
		}

	#ifdef UNICODE
		if (holding_ref)
			DisposeMemory(holding_ref);
	#endif
	
		UnuseMemory(pg_stuff->pg);
	}
	else {

		pgSetHandler(&paige_globals, DOCUMENT_INFO_KEY, doc_read_handler, doc_write_handler, NULL, NULL);
		
		if (cache_write)
			result = pgCacheSaveDoc(pg_stuff->pg, &file_position, NULL, 0, write_proc, ref, 0);
		else
			result = pgSaveDoc(pg_stuff->pg, &file_position, NULL, 0, write_proc, ref, 0);

		if (result == 0) {
		    paige_rec_ptr		pg_rec;
		    
		    pg_rec = UseMemory(pg_stuff->pg);
		    
		    if (!pg_rec->shared_flags)
		   	 	result = pgSaveAllEmbedRefs(pg_stuff->pg, write_proc, NULL, &file_position, ref);
		    
		    UnuseMemory(pg_stuff->pg);

		    if (result == 0)
				pgTerminateFile(pg_stuff->pg, &file_position, write_proc, ref);
		}
		
		pgRemoveHandler(&paige_globals, DOCUMENT_INFO_KEY);
	}

	if (!scrap_memory)
		DisposeMemory(ref);

	if (result != 0)
		return	(long)-1;
	
	return	file_position;
}



/* read_document loads all the data from f_ref beginning at position. If
read_text_only is TRUE then the file is assumed to be raw text. The function
result is the number of bytes from the file that were read (or -1 if error). */

static long read_document (paige_control_ptr pg_stuff, pg_file_unit f_ref, long position,
		int read_text_only, int verify_only, int cache_read, memory_ref scrap_memory)
{
	memory_ref			ref;
	rectangle			vis_rect;
	paige_rec_ptr		pg_rec;
	file_io_proc		read_proc;
	long				file_position, data_size, old_flags;
	short				result;
	pg_file_unit PG_FAR	*the_file;

	if (scrap_memory) {
		
		ref = scrap_memory;
		read_proc = pgScrapMemoryRead;
	}
	else {
		
		read_proc = NULL;
		ref = MemoryAlloc(&mem_globals, sizeof(pg_file_unit), 1, 0);
		the_file = UseMemory(ref);
		*the_file = (pg_file_unit)f_ref;
		UnuseMemory(ref);
		
	#ifdef DO_RTF
		if (((file_position = do_rtf_import(pg_stuff,f_ref))) != 0)
			return(file_position);
	#endif
	}

	file_position = position;
	
	if (verify_only) {
		
		result = pgVerifyFile(ref, read_proc, position);
		
		if (!scrap_memory)
			DisposeMemory(ref);

		if (result)
			result = -1;
		
		return	(long)result;
	}

	if (read_text_only) {
		memory_ref			buf_ref;
		pg_char_ptr			buffer;
		long				eof;

		buf_ref = MemoryAlloc(&mem_globals, sizeof(pg_char), MAX_READ_SIZE, 0);
		buffer = UseMemory(buf_ref);
		data_size = sizeof(long);
		pgStandardReadProc(&eof, io_get_eof, &file_position, &data_size, ref);
		
		while (file_position < eof) {
			
			data_size = MAX_READ_SIZE;

			if ((file_position + data_size) > eof)
				data_size = eof - file_position;
			
			result = pgStandardReadProc(buffer, io_data_direct, &file_position, &data_size, ref);
			
			if (result != 0)
				break;
			
			if (data_size) {
		
		#ifdef UNICODE
				
				data_size = pgBytesToUnicode((pg_bits8_ptr)buffer, buffer, NULL, data_size);				
		#endif
				pgInsert(pg_stuff->pg, buffer, data_size, CURRENT_POSITION, data_insert_mode, 0, draw_none);
			}
		}

		UnuseAndDispose(buf_ref);
	}
	else {

		pgSetHandler(&paige_globals, DOCUMENT_INFO_KEY, doc_read_handler, doc_write_handler, NULL, NULL);

		pgAreaBounds(pg_stuff->pg, NULL, &vis_rect);
		pgInitEmbedProcs(&paige_globals, EmbedCallbackProc, NULL);
		pg_rec = UseMemory(pg_stuff->pg);
		old_flags = pg_rec->flags;
		
		if (cache_read)
			result = pgCacheReadDoc(pg_stuff->pg, &file_position, NULL, 0, read_proc, ref);
		else
			result = pgReadDoc(pg_stuff->pg, &file_position, NULL, 0, read_proc, ref);

		pg_rec->flags |= (old_flags & NO_EDIT_BIT);
		UnuseMemory(pg_stuff->pg);
		
		pgRemoveHandler(&paige_globals, DOCUMENT_INFO_KEY);

		if (result == 0) {
			
			SetExtendedStyleBits(pg_stuff, pg_stuff->ex_styles);
			SetPageArea(pg_stuff);
		}
	}
	
	if (!scrap_memory)
		DisposeMemory(ref);

	if (result != 0) {
		
		if (result < 0)
			return	(long)result;

		return	(long)(-result);
	}
	
	if (read_text_only)
		pgPaginateNow(pg_stuff->pg, pgTextSize(pg_stuff->pg), FALSE);

	UpdateScrollbars((HWND)pg_stuff->myself, pg_stuff, FALSE);

	return	file_position;
}


/* print_next_page prints the current page in print_info. If end of document
(or end of specified range), the print_info is updated to reflect. So, if
page-from returns > page-to then we are through printing. */

static void print_next_page (paige_control_ptr pg_stuff, PAIGEPRINTSTRUCT PG_FAR *print_info)
{
	paige_rec_ptr			pg_rec;
	graf_device				print_port;
	rectangle				target_page;

	if (!print_info->to)                
		print_info->to = 9999;

	if (print_info->from > print_info->to)
		return;

	pg_rec = UseMemory(pg_stuff->pg);
	pgBlockMove(&pg_rec->port, &print_port, sizeof(graf_device));

#ifdef MAC_PLATFORM
	print_port.machine_var = (generic_var)print_info->dc;
	print_port.machine_ref = (long)print_info->dc;

    if (!(print_port.resolution = print_info->resolution)) {
		if (print_info->machineRef)
		{
			THPrint print_handle = (THPrint)print_info->machineRef;
			
			print_port.resolution = ((long)(**print_handle).prInfo.iHRes << 16) | ((**print_handle).prInfo.iVRes);
		}
		else print_port.resolution = 0x00480048;
	}
#endif
#ifdef WINDOWS_PLATFORM
	print_port.machine_ref = PG_LONGWORD(long)print_info->dc;
	print_port.access_ctr = 1;
    
    if (!(print_port.resolution = print_info->resolution)) {
    	int			res_x, res_y;
    	
    	res_x = GetDeviceCaps(print_info->dc, LOGPIXELSX);
    	res_y = GetDeviceCaps(print_info->dc, LOGPIXELSY);
    	print_port.resolution = MAKELONG(res_y, res_x);
    }
#endif

	RectToRectangle(&print_info->page, &target_page);
    target_page.top_left.h += print_info->margins.left;
    target_page.top_left.v += print_info->margins.top;
    target_page.bot_right.h -= print_info->margins.right;
    target_page.bot_right.v -= print_info->margins.bottom;
    
	if (!print_info->from)
		print_info->from = 1;
    
	print_info->position = pgFindPage(pg_rec->myself, print_info->from, &target_page);
	print_info->position = pgPrintToPage(pg_stuff->pg, &print_port, print_info->position,
			&target_page, pg_stuff->print_mode);

	if (!print_info->position)
		print_info->to = print_info->from;
	
	++print_info->from;
	
	UnuseMemory(pg_stuff->pg);
}


/* set_print_defaults sets up the recommended print page for the app. If
the DC in the print info is NULL then we set the defaults based on the
current information in pg_stuff, otherwise we get the paper rect from
the printer DC. */

#ifdef MAC_PLATFORM
static void set_print_defaults (paige_control_ptr pg_stuff, PAIGEPRINTSTRUCT PG_FAR *print_info)
{
	THPrint			print_handle;
	Rect			page;

#pragma unused (pg_stuff)

	if (!(print_handle = (THPrint)print_info->machineRef)) {
	
		PrOpen();
		print_handle = (THPrint)NewHandle(sizeof(TPrint));
		PrintDefault(print_handle);
		PrClose();
	}
		
	page = (**print_handle).rPaper;
	OffsetRect(&page, -page.left, -page.top);

	print_info->margins = (**print_handle).rPaper;
	print_info->page = page;
	print_info->margins.left = -print_info->margins.left;
	print_info->margins.top = -print_info->margins.top;
	print_info->margins.right = print_info->margins.right - page.right;
	print_info->margins.bottom = print_info->margins.bottom - page.bottom;
	print_info->resolution = ((long)(**print_handle).prInfo.iHRes << 16) | ((**print_handle).prInfo.iVRes);
	print_info->from = print_info->to = 0;
	print_info->position = 0;
	
	if (!print_info->machineRef)
		DisposeHandle((Handle)print_handle);
}

#endif
#ifdef WINDOWS_PLATFORM

static void set_print_defaults (paige_control_ptr pg_stuff, PAIGEPRINTSTRUCT PG_FAR *print_info)
{
	paige_rec_ptr	pg_rec;
	long			print_width, print_height;
	HDC				printer_dc;
	long			pg_x, pg_y, print_x, print_y;

	pg_rec = UseMemory(pg_stuff->pg);

	if ((printer_dc = print_info->dc) != NULL) {
	
	#ifdef WIN16_COMPILE
	{
		long			org_pts;

		org_pts = GetViewportOrg(printer_dc);
		print_info->page.left = print_info->page.right = HIWORD(org_pts);
    	print_info->page.top = print_info->page.bottom = LOWORD(org_pts);
	}
	#else
	{
		POINT			origin;
		
		GetViewportOrgEx(printer_dc, &origin);
		print_info->page.left = print_info->page.right = origin.x;
    	print_info->page.top = print_info->page.bottom = origin.y;
    }

	#endif
		print_height = GetDeviceCaps (printer_dc, VERTRES);
		print_width = GetDeviceCaps (printer_dc, HORZRES);
		SplitIntoLongs(pg_rec->resolution, &pg_x, &pg_y);

    	print_x = GetDeviceCaps(printer_dc, LOGPIXELSX);
		print_y = GetDeviceCaps(printer_dc, LOGPIXELSY);
        print_info->resolution = MAKELONG((int)print_y, (int)print_x);
        
		pgScaleLong((pg_x << 16) | print_x, 0, &print_width);
		pgScaleLong((pg_y << 16) | print_y, 0, &print_height);
	}
	else {
	    
	    RectangleToRect(&pg_stuff->page_rect, NULL, &print_info->page);
	    print_info->resolution = pg_rec->resolution;
	}
	
	print_info->machineRef = 0;
	print_info->page.right += (int)print_width;
	print_info->page.bottom += (int)print_height;
	print_info->margins.left = pg_stuff->left_margin;
	print_info->margins.right = pg_stuff->right_margin;
	print_info->margins.top = pg_stuff->top_margin;
	print_info->margins.bottom = pg_stuff->bottom_margin;
	
	print_info->from = 1;
	print_info->to = 0;
	print_info->position = 0;

	UnuseMemory(pg_stuff->pg);
}

#endif

#ifdef DO_RTF
#ifdef WINDOWS_PLATFORM
PG_PASCAL(void ) rtf_set_hinst(HINSTANCE hi)
{
        hInst = hi;
}

static
long
do_rtf_import(paige_control_ptr pg_stuff, short f_ref)
{
        pg_error error;
        long rv = 0;

        if (pgIsRTFFile(f_ref))
        {
                pg_rtf_trans_state      state;
                pg_rtf_trans_state_ptr  state_ptr;
                memory_ref              state_ref;
                pg_ref pg;
                
                pg = pg_stuff->pg;
                
                PG_TRY(&mem_globals) 
                {
                        state_ref = MemoryAlloc(&mem_globals, sizeof(pg_rtf_trans_state), 1, 0);
                        state_ptr = UseMemory(state_ref);
                        pgStartupRTF(pgGetGlobals(pg), &state, hInst);

                        state.procs.rtf_def_apply_doc_attr_proc = rtf_apply_doc;
                        state.procs.rtf_idle_proc = rtf_idle; 
                        state.file.ref_num = f_ref;

                        error = pgRTFImportFile (pg, 0l, f_ref, 0, &state);

                        UnuseMemory(state_ref);
                        DisposeMemory(state_ref);
                        
                        pgFailError(&mem_globals, error);
                }
                PG_CATCH 
                {
                        error = PG_GET_ERROR();
                        if (error == USER_CANCELLED_ERR)
                                error = NO_ERROR;
                }
                PG_ENDTRY
                
                pgCloseFile(f_ref);
                f_ref = 0;

                pgReleaseRTFDocInfo(&state);
                pgShutdownRTF(&state);
                
                pgFailError(&mem_globals, error);
                
                pgSetAttributes (pg, pgGetAttributes (pg) | NO_EDIT_BIT);
                rv++;
        }
        return (0l);
}

// Idle handler
pg_error rtf_idle(pg_rtf_trans_state_ptr state)
{       
        //Place any idle processing here
	//	return USER_CANCELLED_ERR;	//To stop
        return NO_ERROR;
}


//Applies documant formatting to Paige
void rtf_apply_doc (pg_rtf_trans_state_ptr state)
{
        //An app can relayout the pg_ref as it wants to
        return;
}
#else
#ifdef WINDOWS_PLATFORM
PG_PASCAL(void ) rtf_set_hinst(HINSTANCE hi)
{
// Does nothing, but defined for .DEF file
}
#endif
#endif
#endif

/* control_read_handler is the default read handler for custom keys. */

STATIC_PASCAL (pg_boolean) control_read_handler (paige_rec_ptr pg, pg_file_key key, memory_ref key_data,
		long PG_FAR *element_info, void PG_FAR *aux_data, long PG_FAR *unpacked_size)
{
	paige_control_ptr			pg_stuff;
	PAIGEIOSTRUCT				io_data;

#ifdef MAC_PLATFORM
#pragma unused (key, aux_data)
#endif

	if (!pg->custom_control)
		return	TRUE;

	pg_stuff = UseMemory(pg->custom_control);
	pgFillBlock(&io_data, sizeof(PAIGEIOSTRUCT), 0);

	io_data.refCon = *element_info;
	io_data.data = UseMemory(key_data);
	io_data.dataSize = GetMemorySize(key_data);
	
	if (unpacked_size)
		*unpacked_size = io_data.dataSize;

	SendNotification(pg_stuff, PGN_READFILE, 0, &io_data);
	UnuseMemory(key_data);
	UnuseMemory(pg->custom_control);

	return	TRUE;
}


/* The doc_read and doc_write handlers transfer misc. data from the control. */

STATIC_PASCAL (pg_boolean) doc_read_handler (paige_rec_ptr pg, pg_file_key key, memory_ref key_data,
		long PG_FAR *element_info, void PG_FAR *aux_data, long PG_FAR *unpacked_size)
{
	paige_control_ptr			pg_stuff;
	pack_walk					walker;

#ifdef MAC_PLATFORM
#pragma unused (key, element_info, aux_data, unpacked_size)
#endif

	pg_stuff = UseMemory(pg->custom_control);
	pgSetupPacker(&walker, key_data, 0);

	pg_stuff->flags = (short)pgUnpackNum(&walker);
	pg_stuff->flags &= HAS_CONTAINERS;

	pg_stuff->left_margin = (short)pgUnpackNum(&walker);
	pg_stuff->right_margin = (short)pgUnpackNum(&walker);
	pg_stuff->top_margin = (short)pgUnpackNum(&walker);
	pg_stuff->bottom_margin = (short)pgUnpackNum(&walker);
	pg_stuff->columns = (short)pgUnpackNum(&walker);
	pg_stuff->column_gap = (short)pgUnpackNum(&walker);
	pg_stuff->frame_pen_width = (short)pgUnpackNum(&walker);
	pg_stuff->page_pen_width = (short)pgUnpackNum(&walker);
	pg_stuff->break_pen_width = (short)pgUnpackNum(&walker);
	pg_stuff->ex_styles = pgUnpackNum(&walker);
	pg_stuff->notification = pgUnpackNum(&walker);

	pg->notification = pg_stuff->notification;

	UnuseMemory(walker.data_ref);
	UnuseMemory(pg->custom_control);
	
	return	TRUE;
}


STATIC_PASCAL (pg_boolean) doc_write_handler (paige_rec_ptr pg, pg_file_key key, memory_ref key_data,
		long PG_FAR *element_info, void PG_FAR *aux_data, long PG_FAR *unpacked_size)
{
	paige_control_ptr			pg_stuff;
	pack_walk					walker;

#ifdef MAC_PLATFORM
#pragma unused (key, element_info, aux_data, unpacked_size)
#endif

	pg_stuff = UseMemory(pg->custom_control);
    
    pgFillBlock(&walker, sizeof(pack_walk), 0);
	walker.data_ref = key_data;
	walker.remaining_ctr = GetMemorySize(walker.data_ref);
	walker.data = UseMemory(walker.data_ref);

	pgPackNum(&walker, short_data, (short)pg_stuff->flags);
	pgPackNum(&walker, short_data, (short)pg_stuff->left_margin);
	pgPackNum(&walker, short_data, (short)pg_stuff->right_margin);
	pgPackNum(&walker, short_data, (short)pg_stuff->top_margin);
	pgPackNum(&walker, short_data, (short)pg_stuff->bottom_margin);
	pgPackNum(&walker, short_data, (short)pg_stuff->columns);
	pgPackNum(&walker, short_data, (short)pg_stuff->column_gap);
	pgPackNum(&walker, short_data, (short)pg_stuff->frame_pen_width);
	pgPackNum(&walker, short_data, (short)pg_stuff->page_pen_width);
	pgPackNum(&walker, short_data, (short)pg_stuff->break_pen_width);
	pgPackNum(&walker, long_data, pg_stuff->ex_styles);
	pgPackNum(&walker, long_data, pg_stuff->notification);

	pgFinishPack(&walker);

	UnuseMemory(pg->custom_control);
	
	return		TRUE;
}






