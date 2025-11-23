/* This source file provides a wrapper around PAIGE low-level functions. The
purpose is to provide a custom control window. For ultra simple edit boxes, this
can replace the EDIT window class. Note, however, that breaking the 64K barrier
for text size or to apply rich formatting will require the app to call the
extended "PG_" messages.
Copyright 1995 DataPak Software, Inc., with help from Brett Oliver. PAIGE
code was create for DSI by "Gar."  */


#include "Paige.h"
#include "pgTraps.h"
#include "pgUtils.h"
#include "pgShapes.h" 
#include "pgEmbed.h"
#include "machine.h"
#include "defprocs.h"
#include "pgOSUtl.h"
#include "pgEdit.h"
#include "pgFiles.h"
#ifdef MAC_PLATFORM
#include "pgWinDef.h"
#endif
#include "pgCtlUtl.h"
#include "pgCntl.h"
#include "pgSubref.h"
#include "pgExceps.h"
#include "pgScrap.h"

#ifdef DOING_EQ
#include "EQ.h"
eq_global_rec eq_globals;
#endif

HINSTANCE   libInstance = (HINSTANCE)NULL;

pg_globals  paige_globals;          // PAIGE globals (required to run PAIGE)
pgm_globals mem_globals;            // Allocation globals (required for Alloc Mgr)
HCURSOR     arrow_cursor;
HCURSOR     ibeam_cursor;
pg_os_type  clipboard_format = 0;
int      dll_inited = FALSE;

#ifdef WINDOWS_PLATFORM

#ifndef WM_IME_REPORT
#define WM_IME_REPORT       0x0280
#define IR_STRING       0x140
#endif

#ifdef UNICODE
pg_char paige_class[] = L"paige";
pg_char paige_cp_format[] = L"PAIGE";
#else
pg_char paige_class[] = "paige";
pg_char paige_cp_format[] = "PAIGE";
#endif
#endif

static pg_ref textbox_test = MEM_NULL;

static pg_ref optional_pg = MEM_NULL;

static long pascal edit_win_proc (HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

static long cstring_length (PGSTR cstr);
static int non_insertable_char (pg_char the_char);
static void draw_rectangle (HDC hdc, rectangle_ptr rect, co_ordinate_ptr offset,
      pg_scale_ptr scale_factor, short inset, short pen_size);
static void pack_pen (pack_walk_ptr walker, HPEN pen);
static HPEN unpack_pen (pack_walk_ptr walker, short PG_FAR *pen_size);
static void setup_drag_info (paige_control_ptr pg_stuff, co_ordinate_ptr pg_mouse,
      short wParam, PAIGEMOUSESTRUCT PG_FAR *mouse_info);
static void handle_button_up (paige_control_ptr pg_stuff, long lParam);

// Local hooks:


STATIC_PASCAL (void) document_draw_proc (paige_rec_ptr pg, shape_ptr page_shape,
      pg_short_t r_qty, pg_short_t page_num, co_ordinate_ptr vis_offset,
      short draw_mode_used, short call_order);
STATIC_PASCAL (void) bits_modify_proc (paige_rec_ptr pg, graf_device_ptr bits_port,
      pg_boolean post_call, rectangle_ptr bits_rect, co_ordinate_ptr screen_offset,
      long text_offset);
STATIC_PASCAL (void) wait_idle_proc (paige_rec_ptr pg, short wait_verb, long progress_ctr,
      long completion_ctr);
STATIC_PASCAL (void) line_draw_proc (paige_rec_ptr pg, style_walk_ptr walker,
      long line_number, long par_number, text_block_ptr block, point_start_ptr first_line,
      point_start_ptr last_line, point_start_ptr previous_first, point_start_ptr previous_last,
      co_ordinate_ptr offset_extra, rectangle_ptr vis_rect, short call_verb);
STATIC_PASCAL (void) set_dc_proc (paige_rec_ptr pg, short verb, graf_device_ptr device,
      color_value_ptr bk_color);
STATIC_PASCAL (void) auto_scroll (paige_rec_ptr pg, short h_verb, short v_verb,
      co_ordinate_ptr mouse_point, short draw_mode);
STATIC_PASCAL (void) modify_pages (paige_rec_ptr pg, long page_num, rectangle_ptr margins);


STATIC_PASCAL (pg_boolean) control_read_handler (paige_rec_ptr pg, pg_file_key key, memory_ref key_data,
      long PG_FAR *element_info, void PG_FAR *aux_data, long PG_FAR *unpacked_size);
STATIC_PASCAL (pg_boolean) control_write_handler (paige_rec_ptr pg, pg_file_key key, memory_ref key_data,
      long PG_FAR *element_info, void PG_FAR *aux_data, long PG_FAR *unpacked_size);

#ifdef MAC_PLATFORM


/* InitPaigeLibs is the only required initialization call to activate the
PAIGE custom control. Here is where we register the class for PAIGE. We also
do all the pgInit stuff. */

PG_PASCAL (void) InitPaigeLibs (Str255 defFont, RGBColor *background)
{    
    if (!dll_inited) {
 
      pgMemStartup(&mem_globals, 0);
	  mem_globals.debug_flags |= ALL_MEMORY_CHECK;
      pgInit(&paige_globals, &mem_globals);
      paige_globals.def_hooks.subset_glitter = SubsetPostDraw;
      paige_globals.embed_callback_proc = (long)EmbedCallbackProc;
    }

   if (defFont) {    // caller wants a default font
      pg_short_t     name_size;
      
      name_size = (pg_short_t)defFont[0];
      pgFillBlock(&paige_globals.def_font, sizeof(font_info), 0);
      BlockMove(defFont, paige_globals.def_font.name, (name_size + 1) * sizeof(pg_char));
   }

 // Set up default hooks we use for user features:
 
   if (!dll_inited) {
   
      paige_globals.def_hooks.page_proc = document_draw_proc;
      paige_globals.def_hooks.wait_proc = wait_idle_proc;
      paige_globals.def_par.procs.line_glitter = line_draw_proc;
      paige_globals.def_hooks.bitmap_proc = bits_modify_proc;
      paige_globals.def_hooks.set_device = set_dc_proc;
      paige_globals.def_hooks.auto_scroll = auto_scroll;
      paige_globals.def_hooks.page_modify = modify_pages;
    
      if (background)
         BlockMove(background, &paige_globals.def_bk_color, sizeof(RGBColor));
    
      pgSetHandler(&paige_globals, source_refcon_key, control_read_handler,
            control_write_handler, (file_io_proc)NULL, (file_io_proc)NULL);
    
   #ifdef DOING_EQ
      pgFillBlock(&eq_globals, sizeof(eq_global_rec), 0);
      InitEQLibs("eqfonts.dat", &eq_globals, &mem_globals, &paige_globals);
   #endif
   
      if (!clipboard_format)
         clipboard_format = 'paig';
   }
   
   dll_inited += 1;
}


PG_PASCAL (void) ClosePaigeLibs (void)
{
   if ((dll_inited -= 1) == 0) {
      
#ifdef DOING_EQ
      CloseEQLibs(&eq_globals);
#endif
      if (libInstance)
         DisposeMemory((memory_ref)libInstance);
      
      libInstance = 0;

      pgShutdown(&paige_globals);
      pgMemShutdown(&mem_globals);
   }
}


#else

/* InitPaigeLibs is the only required initialization call to activate the
PAIGE custom control. Here is where we register the class for PAIGE. We also
do all the pgInit stuff. */

PG_PASCAL (void) InitPaigeLibs (HANDLE hInstance, PGSTR defFont, HBRUSH background)
{
   WNDCLASS       wc;
    
    if (!dll_inited) {
        
      pgMemStartup(&mem_globals, 0);
      pgInit(&paige_globals, &mem_globals);
      paige_globals.def_hooks.subset_glitter = SubsetPostDraw;
      paige_globals.embed_callback_proc = (long)EmbedCallbackProc;

#ifdef DOING_EQ
      pgFillBlock(&eq_globals, sizeof(eq_global_rec), 0);
      InitEQLibs("eqfonts.win", &eq_globals, &mem_globals, &paige_globals);
#endif
    }
    
   if (defFont) {    // caller wants a default font
      short    name_size;

      pgFillBlock(&paige_globals.def_font, sizeof(font_info), 0);
      name_size = (short)pgCStrLength((pg_c_string_ptr)defFont);
      pgBlockMove(defFont, &paige_globals.def_font.name[1], name_size * sizeof(pg_char));
      paige_globals.def_font.name[0] = (pg_char)name_size;
   }

   if (!dll_inited) {
   
 // Set up default hooks we use for user features:
 
      paige_globals.def_hooks.page_proc = document_draw_proc;
      paige_globals.def_hooks.wait_proc = wait_idle_proc;
      paige_globals.def_par.procs.line_glitter = line_draw_proc;
      paige_globals.def_hooks.bitmap_proc = bits_modify_proc;
      paige_globals.def_hooks.set_device = set_dc_proc;
      paige_globals.def_hooks.auto_scroll = auto_scroll;
      paige_globals.def_hooks.page_modify = modify_pages;
   
      pgSetHandler(&paige_globals, source_refcon_key, control_read_handler,
            control_write_handler, (file_io_proc)NULL, (file_io_proc)NULL);
    
      wc.style = CS_GLOBALCLASS | CS_PARENTDC | CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
      wc.lpfnWndProc = ControlWinProc;
      wc.cbClsExtra = 0;
      wc.cbWndExtra = sizeof(memory_ref);
      
      if (libInstance)
         wc.hInstance = libInstance;
      else
         wc.hInstance = hInstance;
   
      wc.hIcon = (HICON) NULL;
      wc.hCursor = (HCURSOR)NULL;
      
      if (!(wc.hbrBackground = background))
         wc.hbrBackground = (HBRUSH) GetStockObject(WHITE_BRUSH);
   
      wc.lpszMenuName  = NULL;
      wc.lpszClassName = paige_class;
      RegisterClass(&wc);
   
      arrow_cursor = LoadCursor((HINSTANCE)NULL, IDC_ARROW);
      ibeam_cursor = LoadCursor((HINSTANCE)NULL, IDC_IBEAM);

// Register clipboard format:
      
      if (!clipboard_format)
         clipboard_format = RegisterClipboardFormat((PGSTR)paige_cp_format);
   }

   dll_inited += 1;
}


/* ClosePaigeLibs gets called before app terminates. */

PG_PASCAL (void) ClosePaigeLibs (void)
{
   if ((dll_inited -= 1) == 0) {

#ifdef DOING_EQ
      CloseEQLibs(&eq_globals);
#endif
      pgShutdown(&paige_globals);
      pgMemShutdown(&mem_globals);
   }
}

#endif
// endif for Mac/Windows platform.


PG_PASCAL (void) GetPaigeGlobals (pg_globals_ptr globals)
{
   pgBlockMove(&paige_globals, globals, sizeof(pg_globals));
}


PG_PASCAL (void) SetPaigeGlobals (pg_globals_ptr globals)
{
   pgBlockMove(globals, &paige_globals, sizeof(pg_globals));
}


PG_PASCAL (pg_globals_ptr) GetPaigeGlobalsPtr (void)
{
   return   (pg_globals_ptr)&paige_globals;
}

/* CreateMemoryRef creates a new memory_ref and returns it to the app. It can then call
all the MemMgr functions that don't need a globals pointer. */

PG_PASCAL (memory_ref) CreateMemoryRef (long bytesize, pg_boolean zero_fill)
{
   if (zero_fill)
      return      MemoryAllocClear(&mem_globals, 1, bytesize, 0);
   else
      return      MemoryAlloc(&mem_globals, 1, bytesize, 0);
}


/* GetPGStuff returns a pointer to the record struct I keep attached to
each window control. If all goes well, the pointer is returned as the function
result and *ref is initialized to the actual memory_ref. */

paige_control_ptr pascal GetPGStuff (HWND hWnd, memory_ref PG_FAR *ref)
{
   long     control_ref = 0;
   
   if (!hWnd) {
   
      if (optional_pg) {
         paige_rec_ptr     pg;
         
         pg = UseMemory(optional_pg);
         control_ref = (long)pg->custom_control;
         UnuseMemory(optional_pg);
      }
   }
   else
      control_ref = GetWindowLong(hWnd, 0);
   
   if (control_ref) {
      
      *ref = (memory_ref)control_ref;
      return   ((paige_control_ptr)UseMemory((memory_ref)control_ref));
   }

   *ref = MEM_NULL;
   return   NULL;
}


/* RepaintControl re-draws the PAIGE control wherever it lives. This gets called
when something changes and the user (or implied functionality) wants the
object re-drawn. However, if draw_everything is TRUE then we merely invalidate
a paint region, otherwise we do an immediate update. */

void pascal RepaintControl (HWND hWnd, paige_control_ptr pg_stuff, int draw_everything)
{
   paige_rec_ptr     pg_rec;
   RECT           vis_r;
#ifdef WINDOWS_PLATFORM
   rectangle         vis_bounds;
#endif

   UpdateScrollbars(hWnd, pg_stuff, FALSE);

   pg_rec = UseMemory(pg_stuff->pg);
   
   if (draw_everything) {
   
      if (hWnd) {

#ifdef MAC_PLATFORM
         pg_window_ptr        window;
         
         window = (pg_window_ptr)hWnd;
         vis_r = window->vis_bounds;
#endif
#ifdef WINDOWS_PLATFORM
         pgShapeBounds(pg_rec->vis_area, &vis_bounds);
         pgScaleRect(&pg_rec->scale_factor, NULL, &vis_bounds);
         RectangleToRect(&vis_bounds, NULL, &vis_r);
#endif
         InvalidateRect(hWnd, &vis_r, TRUE);
      }
   }
   else {
   
      pgUpdateText(pg_rec, NULL, 0, pg_rec->t_length,
            MEM_NULL, NULL, pg_stuff->update_mode, TRUE);
      
      NotifyOverflow(pg_stuff);
   }

   UnuseMemory(pg_stuff->pg);
}


/* CToPString takes a Windows (c)string and copies it to a pascal string. */

void pascal CToPString (PGSTR src_str, pg_char_ptr target_str)
{
   pg_short_t           str_size;
   pg_char              the_byte;
   
   str_size = 0;
   
   while (str_size < (FONT_SIZE - 1)) {
      
      if (!(the_byte = (pg_char)src_str[str_size]))
         break;
      
      ++str_size;
      target_str[str_size] = the_byte;
   }
   
   target_str[0] = (pg_char)str_size;
}


/* This sets up a PrepareUndo for styles. While we are at it, a "change"
notifications is also sent. */

void pascal PrepareStyleUndo(paige_control_ptr pg_stuff)
{
   if (!(pg_stuff->ex_styles & PGS_NOAUTOUNDO)) {
      
      if (pg_stuff->undo)
         pgDisposeUndo(pg_stuff->undo);
      
      pg_stuff->undo = pgPrepareUndo(pg_stuff->pg, undo_format, NULL);
   }
   
   SendChangeCommand(pg_stuff);
}


/* UpdateScrollbars gets called after any text-altering function that might
have affected the scrollbar positions. Additionally, if scroll_to_cursor is TRUE
then we force-scroll to the insertion position. */

void pascal UpdateScrollbars (HWND hWnd, paige_control_ptr pg_stuff, pg_boolean scroll_to_cursor)
{
   short       max_h, max_v, h_value, v_value;
   pg_boolean     scroll_to_insertion, has_h, has_v;
    
    has_h = ((pg_stuff->win_flags & WS_HSCROLL) != 0);
    has_v = ((pg_stuff->win_flags & WS_VSCROLL) != 0);
    
    if (!hWnd)
      return;

    if (!has_h && !has_v)
      return;
 
   if (pgNumSelections(pg_stuff->pg))
      scroll_to_insertion = FALSE;
   else
      scroll_to_insertion = scroll_to_cursor;

   for (;;) {

      if (pgGetScrollValues(pg_stuff->pg, &h_value, &v_value, &max_h, &max_v)) {
      
      #ifdef WINDOWS_PLATFORM
         if (max_v < 1)
            max_v = 1;
         if (max_h < 1)
            max_h = 1;
      #endif

         if (has_v) {
         
            SetScrollRange(hWnd, SB_VERT, 0, max_v, FALSE);
            SetScrollPos(hWnd, SB_VERT, v_value, TRUE);
         }
         
         if (has_h) {
         
            SetScrollRange(hWnd, SB_HORZ, 0, max_h, FALSE);       
            SetScrollPos(hWnd, SB_HORZ, h_value, TRUE);
         }
      }

      if (!scroll_to_insertion)
         break;
      
      if (!pgScrollToView(pg_stuff->pg, CURRENT_POSITION, pg_stuff->extra_scroll_h,
            pg_stuff->extra_scroll_v, TRUE, pg_stuff->scroll_mode))
         break;

      scroll_to_insertion = FALSE;
   }
   
   pg_stuff->flags &= (~SCROLLBARS_DIRTY_FLAG);
}


#ifdef WINDOWS_PLATFORM

/* SendChangeCommand sends a WM_COMMAND + EN_CHANGE to notify common Windows things. */

long pascal SendChangeCommand (paige_control_ptr pg_stuff)
{
   long     hiParam, lParam;
   
   if (!pg_stuff->parent)
      return   0;
   
   if (pg_stuff->notification & PGN_CHANGEFLAG) {

      SendNotification(pg_stuff, PGN_CHANGE, 0, NULL);
      return      0;
   }
   else {

   #ifdef WIN32_COMPILE
      lParam = (long)pg_stuff->myself;
      hiParam = EN_CHANGE;
      hiParam <<= 16;
   
      return   (SendMessage ((HWND)pg_stuff->parent, WM_COMMAND,
               pg_stuff->control_id | hiParam, lParam));
   #else
      lParam = PG_LONGWORD(long)pg_stuff->myself;
      lParam &= 0x0000FFFF;
      hiParam = EN_CHANGE;
      hiParam <<= 16;
   
      return   (SendMessage ((HWND)pg_stuff->parent, WM_COMMAND,
               pg_stuff->control_id, hiParam | lParam));
   #endif

   }
}

#endif

/* SendNotification sends message to the parent window of the
control. If lParam2 is not NULL it is used as the lParam part of the
message, otherwise lParam1 is used. */

long pascal SendNotification (paige_control_ptr pg_stuff, short message,
      long lParam1, void PG_FAR *lParam2)
{
   long        second_param;
   
   if (!pg_stuff->parent)
      return   0;
   
   if (message == PGN_WRITECLIPBOARD)
      if (!(pg_stuff->notification & PGN_WRITECLIPBOARDFLAG))
         return   0;

   if (message == PGN_GETCLIPBOARD)
      if (!(pg_stuff->notification & PGN_GETCLIPBOARDFLAG))
         return   0;

   if (lParam2)
      second_param = (long)lParam2;
   else
      second_param = lParam1;

#ifdef MAC_PLATFORM
   {
      pg_window_ptr     window;
      
      window = (pg_window_ptr)pg_stuff->myself;
      return   window->callback((HWND)window, message, pg_stuff->control_id, second_param);
   }
#endif
#ifdef WINDOWS_PLATFORM
   return   (SendMessage((HWND)pg_stuff->parent, message, pg_stuff->control_id, second_param));
#endif
}


/* NotifyOverflow sends a text overflow notification if it is enabled. */

void pascal NotifyOverflow (paige_control_ptr pg_stuff)
{
   paige_rec_ptr        pg_rec;
   long              overflow_size;

   if (!(pg_stuff->notification & PGN_OVERFLOWFLAG))
      return;
   
   pg_rec = UseMemory(pg_stuff->pg);
   
   if (pg_rec->overflow_size) {
      
      overflow_size = pg_rec->overflow_size;
      pg_rec->overflow_size = 0;
      SendNotification(pg_stuff, PGN_OVERFLOW, overflow_size, NULL);
   }
   
   UnuseMemory(pg_stuff->pg);
}


/* SplitIntoLongs returns two longs (with correct sign) from lo and high
words in input value. */

void pascal SplitIntoLongs (long input, long PG_FAR *hiword, long PG_FAR *loword)
{
   short    lo, hi;
   
   lo = LOWORD(input);
   hi = HIWORD(input);
   
   if (hiword)
      *hiword = hi;
   if (loword)
      *loword = lo;
}


/* SetPageArea sets the wrap area shape based on info in pg_stuff
such as page rect, columns, etc. If anything changed the function
returns TRUE. Upon entry, all rects and flags in pg_stuff should be in tact. 
Note, the eventual page_area rect in pg is computed from pg_stuff->page_rect. Hence
pg_stuff->page_rect must be the original page rect, i.e. margins NOT subracted. When
the function returns, pg_stuff->page_rect is only altered if PGS_VISPAGE was set
(in which case it is forced to pg_stuff->vis_rect). */

int pascal SetPageArea (paige_control_ptr pg_stuff)
{
   rectangle         current_page, current_vis, page_bounds, vis_bounds;
   pg_doc_info       doc_info;
   paige_rec_ptr     pg_rec;
   pg_boolean        changed;
   long           page_pen_inset;
    
    changed = FALSE;
   
   if (!pg_stuff->frame_pen)
      pg_stuff->frame_pen_width = 0;
   if (!pg_stuff->page_pen)
      pg_stuff->page_pen_width = 0;

   page_pen_inset = pg_stuff->page_pen_width;
    pgGetDocInfo(pg_stuff->pg, &doc_info);

   vis_bounds = pg_stuff->vis_rect;
   pgInsetRect(&vis_bounds, pg_stuff->frame_pen_width, pg_stuff->frame_pen_width);

   if (pg_stuff->ex_styles & PGS_VISPAGE)
      pg_stuff->page_rect = vis_bounds;

   page_bounds = pg_stuff->page_rect;
   pgInsetRect(&page_bounds, page_pen_inset, page_pen_inset);

   if (pg_stuff->ex_styles & PGS_HIDEMARGINS)
      doc_info.repeat_offset.v = 1;
   else {
      
      pg_rec = UseMemory(pg_stuff->pg);

      page_bounds.top_left.v += pg_stuff->top_margin;
      page_bounds.top_left.h += pg_stuff->left_margin;
      page_bounds.bot_right.v -= pg_stuff->bottom_margin;
      page_bounds.bot_right.h -= pg_stuff->right_margin;

       if (pg_stuff->right_margin > pg_rec->append_h)
         pg_rec->append_h = pg_stuff->right_margin;
      
      UnuseMemory(pg_stuff->pg);

        doc_info.repeat_offset.v = pg_stuff->top_margin + pg_stuff->bottom_margin;
        doc_info.repeat_offset.v += pg_stuff->break_pen_width;
   }

   doc_info.repeat_slop = pg_stuff->page_slop;
   pgAreaBounds(pg_stuff->pg, &current_page, &current_vis);

   changed |= (!pgEqualStruct((void PG_FAR *)&page_bounds, (void PG_FAR *)&current_page, sizeof(rectangle)));
   changed |= (!pgEqualStruct((void PG_FAR *)&vis_bounds, (void PG_FAR *)&current_vis, sizeof(rectangle)));
   
   if (pg_stuff->ex_styles & PGS_CARETVIEW)
      doc_info.attributes |= WINDOW_CURSOR_BIT;
   else
      doc_info.attributes &= (~WINDOW_CURSOR_BIT);

   pgSetDocInfo(pg_stuff->pg, &doc_info, changed, draw_none);
    
    if (!(pg_stuff->flags & HAS_CONTAINERS))
      pgSetAreaBounds(pg_stuff->pg, &page_bounds, &vis_bounds);
    else
      pgSetAreaBounds(pg_stuff->pg, NULL, &vis_bounds);
    
    if (pg_stuff->columns > 1 && !(pg_stuff->flags & HAS_CONTAINERS))
      SetContainerColumns(pg_stuff);
    
   return   changed;
}


/* SetContainerColumns changes the page shape to become "columns." The "gap"
param indicates the amount of space between columns. */

void pascal SetContainerColumns (paige_control_ptr pg_stuff)
{
   paige_rec_ptr        pg_rec;
   rectangle            bounds;
   long              qty, column_width, total_width;

   pgAreaBounds(pg_stuff->pg, &bounds, NULL);
   pg_rec = UseMemory(pg_stuff->pg);
   
   if (!(qty = pg_stuff->columns))
      qty = 1;

   if (qty > 1) {
      
      total_width = (bounds.bot_right.h - bounds.top_left.h)
            - ((qty) * pg_stuff->column_gap);
      column_width = total_width / qty;
   
   // make sure document has repeating shape:
   
      pg_rec->doc_info.attributes |= (V_REPEAT_BIT | BOTTOM_FIXED_BIT);
   }
   else
      column_width = bounds.bot_right.h - bounds.top_left.h;

   bounds.bot_right.h = bounds.top_left.h + column_width;
   pgSetShapeRect(pg_rec->wrap_area, &bounds);
   
   while (qty > 1) {
      
      pgOffsetRect(&bounds, column_width + (long)pg_stuff->column_gap, 0);
      pgAddRectToShape(pg_rec->wrap_area, &bounds);
      --qty;
   }

   InvalidateShape(pg_rec, FALSE);

   UnuseMemory(pg_stuff->pg);
}


/* InvalidateShape invalidates the PAIGE object so it re-paginates and clips
correctly. */

void pascal InvalidateShape (paige_rec_ptr pg_rec, int exclude_changed)
{
   pgInvalSelect(pg_rec->myself, 0, pg_rec->t_length);
   pg_rec->port.clip_info.change_flags |= CLIP_PAGE_CHANGED;
   
   if (exclude_changed)
      pg_rec->port.clip_info.change_flags |= CLIP_EXCLUDE_CHANGED;
}


/* SetupDragInfo sets up misc info for mouse up and drag notifications */

void pascal SetupDragInfo (paige_control_ptr pg_stuff, co_ordinate_ptr pg_mouse,
      PAIGEMOUSESTRUCT PG_FAR *mouse_info)
{
   mouse_info->yMouse = pg_mouse->v;
   mouse_info->xMouse = pg_mouse->h;
   mouse_info->wParam = pg_stuff->wParam;
   mouse_info->area = pg_stuff->mouse_loc;
   mouse_info->dblClick = ((pg_stuff->flags & DOUBLECLICK_FLAG) != 0);
   mouse_info->trplClick = ((pg_stuff->modifiers & PAR_MOD_BIT) != 0);
   pgGetSelection(pg_stuff->pg, &mouse_info->firstPos, &mouse_info->endPos);
}


#ifdef DOING_EQ


/* InitializeEQRef sets up all the hooks, etc. for EQ without a Control window. */

PG_PASCAL (void) InitDirectEQRef (memory_ref pg)
{
   memory_ref        pg_stuff_ref;
   paige_control_ptr pg_stuff;
   paige_rec_ptr     pg_rec;

   pg_stuff_ref = MemoryAllocClear(&mem_globals, 1, sizeof(paige_control), 0);            
   pg_stuff = UseMemory(pg_stuff_ref);
   pg_rec = UseMemory(pg);
    pg_rec->custom_control = pg_stuff_ref;
    pg_rec->flags |= NO_DEFAULT_LEADING;
   pg_stuff->pg = pg;

   pg_stuff->paint_mode = pg_stuff->scroll_mode = bits_emulate_or;
   pg_stuff->update_mode = pg_stuff->print_mode = best_way;
   pg_stuff->insert_mode = key_insert_mode;
   pg_rec->flags2 |= NO_VAUTOSCROLL;
   pg_rec->flags2 |= NO_HAUTOSCROLL;

   eqInitInstance(&eq_globals, pg, 0);

   UnuseMemory(pg);
   UnuseMemory(pg_stuff_ref);
}


/* UnInitDirectEQRef de-initializes EQ attachments. */

PG_PASCAL (void) UnInitDirectEQRef (memory_ref pg)
{
   paige_rec_ptr        pg_rec;
   
   pg_rec = UseMemory(pg);
   
   if (pg_rec->custom_control)
      DisposeMemory((memory_ref)pg_rec->custom_control);
   
   
   pg_rec->custom_control = 0;
   
   UnuseMemory(pg);
}


/* SendDirectMessage is used by the EQ module to send a direct message to the control. */

PG_PASCAL (long) SendDirectMessage (memory_ref pg, short message, WORD wParam, long lParam)
{
   memory_ref        old_pg;
   paige_rec_ptr     pg_rec;
   long           response = 0;
   
   pg_rec = UseMemory(pg);
   
   if (pg_rec->custom_control) {
   
      old_pg = optional_pg;
      optional_pg = pg;
      response = ControlWinProc((HWND)NULL, message, wParam, lParam);
      optional_pg = old_pg;
   }

   UnuseMemory(pg);

   return      response;
}

#endif


/* The Window function called for all messages. */

long pascal REGISTER_PREFIX ControlWinProc (HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
   paige_control_ptr pg_stuff;         // One of these in each control
   PAIGEMOUSESTRUCT  mouse_info;       // Used for page or exclusion click notify
   memory_ref        pg_stuff_ref;     // A memory_ref that holds above
   style_info        info, mask;       // Used to retrieve text formats
   co_ordinate       pg_mouse;         // Used for mouse tracking
   pg_ref            the_scrap;
   paige_rec_ptr     pg_rec;
   rectangle         page_rect, vis_rect;
   long           new_h, new_v, response;
   pg_short_t        caret_verb;
   short          draw_mode;
   short          scroll_position;
   short          scroll_code;

   response = 0;
   pg_stuff = NULL;

   if (message >= PG_SHAPEMESSAGES)
      return   (ShapeMessageProc(hWnd, message, wParam, lParam));

   if (message >= PG_DOCMESSAGES)
      return   (DocMessageProc(hWnd, message, wParam, lParam));

   if (message >= PG_SUBMESSAGES)
      return   (SubrefMessageProc(hWnd, message, wParam, lParam));

   if (message >= PG_EMBEDMESSAGES)
      return   (EmbedMessageProc(hWnd, message, wParam, lParam));

   if (message >= PG_IOMESSAGES)
      return   (IOMessageProc(hWnd, message, wParam, lParam));

   if (message >= PG_EDITMESSAGES)
      return   (EditMessageProc(hWnd, message, wParam, lParam));

   if (message >= PG_PARMESSAGES)
      return   (ParMessageProc(hWnd, message, wParam, lParam));

   if (message >= PG_STYLEMESSAGES)
      return   (StyleMessageProc(hWnd, message, wParam, lParam));

   if (message >= PG_USER)
      return   (UserMessageProc(hWnd, message, wParam, lParam));

 #ifdef WIN16_COMPILE
   if (message >= WM_USER)
      return   (edit_win_proc(hWnd, message, wParam, lParam));
 #endif

   pg_stuff_ref = MEM_NULL;

// If we get here then "normal" windows messages are to be processed:

   switch (message) {
   
      case WM_CHAR: {
            pg_char        the_char;
            long        starting_sel, ending_sel;
            long        had_selections;
            short       verb_for_undo;

            pg_stuff = GetPGStuff(hWnd, &pg_stuff_ref);

            if (pg_stuff->notification & PGN_CHARFLAG)
               if (SendNotification(pg_stuff, PGN_CHAR, (long)wParam, NULL))
                  break;

            the_char = (pg_char)wParam;

            if (HandleSubrefArrows(pg_stuff, the_char))
               break;

            if (pg_stuff->max_input)
               if (the_char != paige_globals.bs_char && the_char != paige_globals.fwd_delete_char)
                  if (the_char >= ' '
                     || the_char == paige_globals.line_wrap_char
                     || the_char == paige_globals.soft_line_char
                     || the_char == paige_globals.tab_char)
                        if (pgTextSize(pg_stuff->pg) >= pg_stuff->max_input)
                           break;

            if (pg_stuff->win_flags & ES_LOWERCASE) {
               
               if (the_char >= 'A' && the_char <= 'Z')
                  the_char += 0x20;
            }
            else
            if (pg_stuff->win_flags & ES_UPPERCASE) {

               if (the_char >= 'a' && the_char <= 'z')
                  the_char -= 0x20;
            }
                
                if (!(pg_stuff->ex_styles & PGS_NOAUTOUNDO) && !non_insertable_char(the_char)) {
                
               if (the_char == paige_globals.bs_char)
                  verb_for_undo = undo_backspace;
               else
               if (the_char == paige_globals.fwd_delete_char)
                  verb_for_undo = undo_fwd_delete;
               else
                  verb_for_undo = undo_typing;
            
               pg_stuff->undo = pgPrepareUndo(pg_stuff->pg, verb_for_undo, (void PG_FAR *)pg_stuff->undo);
            }
                
                had_selections = pgNumSelections(pg_stuff->pg);
                
                if (pg_stuff->modifiers & EXTEND_MOD_BIT)
                  pgGetSelection(pg_stuff->pg, &starting_sel, &ending_sel);

           #ifdef WINDOWS_PLATFORM
           
            if (pgInsert(pg_stuff->pg, (pg_char_ptr)&the_char, 1,
                  CURRENT_POSITION, pg_stuff->insert_mode, pg_stuff->modifiers, pg_stuff->update_mode)) {
         #else

            if (pgInsert(pg_stuff->pg, (pg_char_ptr)&the_char, 1,
                  CURRENT_POSITION, key_buffer_mode, pg_stuff->modifiers, pg_stuff->update_mode)) {
         #endif
               if (had_selections)
                  pgAdjustScrollMax(pg_stuff->pg, pg_stuff->update_mode);
                  
                  if (pg_stuff->modifiers & EXTEND_MOD_BIT) {
                     long     begin, end;
                     
                     if (the_char == paige_globals.up_arrow_char
                        || the_char == paige_globals.left_arrow_char
                        || the_char == paige_globals.down_arrow_char
                        || the_char == paige_globals.right_arrow_char) {
                     
                        pgGetSelection(pg_stuff->pg, &begin, &end);
                        
                        if (end != ending_sel)
                           pgScrollToView(pg_stuff->pg, end, pg_stuff->extra_scroll_h,
                           pg_stuff->extra_scroll_v, TRUE, pg_stuff->scroll_mode);
                     else
                     if (begin != starting_sel)
                        pgScrollToView(pg_stuff->pg, begin, pg_stuff->extra_scroll_h,
                           pg_stuff->extra_scroll_v, TRUE, pg_stuff->scroll_mode);
                     }
                  }
            
               UpdateScrollbars(hWnd, pg_stuff, TRUE);
               SendChangeCommand(pg_stuff);
               NotifyOverflow(pg_stuff);
            }
         }
         break;

      case WM_CLEAR:
         pg_stuff = GetPGStuff(hWnd, &pg_stuff_ref);

         if (!(pg_stuff->ex_styles & PGS_NOAUTOUNDO)) {
         
            if (pg_stuff->undo)
               pgDisposeUndo(pg_stuff->undo);

            pg_stuff->undo = pgPrepareUndo(pg_stuff->pg, undo_delete, NULL);
         }

         pgDelete(pg_stuff->pg, NULL, pg_stuff->update_mode);
         pgAdjustScrollMax(pg_stuff->pg, pg_stuff->update_mode);
            UpdateScrollbars(hWnd, pg_stuff, TRUE);
            NotifyOverflow(pg_stuff);
            
         break;

#ifdef WIN_MULTILINGUAL
      
      case WM_IME_REPORT:

         pg_stuff = GetPGStuff(hWnd, &pg_stuff_ref);
         
         if (wParam == IR_STRING) {
            pg_char_ptr    ir_string;
            long        size;
            
            ir_string = GlobalLock((HANDLE)lParam);
            size = pgCStrLength(ir_string);

            if (!(pg_stuff->ex_styles & PGS_NOAUTOUNDO)) {

               if (pg_stuff->undo)
                  pgDisposeUndo(pg_stuff->undo);

               pg_stuff->undo = pgPrepareUndo(pg_stuff->pg, undo_insert, (void PG_FAR *)&size);
            }

            pgInsert(pg_stuff->pg, ir_string, size, CURRENT_POSITION, data_insert_mode, 0, bits_copy);
            GlobalUnlock((HANDLE)lParam);

            pgAdjustScrollMax(pg_stuff->pg, pg_stuff->update_mode);
               UpdateScrollbars(hWnd, pg_stuff, TRUE);
               NotifyOverflow(pg_stuff);

            response = TRUE;
         }

         break;
#endif

      case WM_COPY:
         the_scrap = MEM_NULL;

         PG_TRY(&mem_globals) {
         
            pg_stuff = GetPGStuff(hWnd, &pg_stuff_ref);
            the_scrap = pgCopy(pg_stuff->pg, NULL);
            pgFailZero(&mem_globals, the_scrap);

         #ifdef WINDOWS_PLATFORM
            OpenClipboard(hWnd);
         #endif
            
            if (!SendNotification(pg_stuff, PGN_WRITECLIPBOARD, 0, (void PG_FAR *)the_scrap))
               pgPutScrap(the_scrap, clipboard_format, pg_void_scrap);

            pgDispose(the_scrap);
            the_scrap = MEM_NULL;

         #ifdef WINDOWS_PLATFORM
            CloseClipboard();
         #endif
         }

         PG_CATCH {
            
            if (the_scrap)
               pgDispose(the_scrap);

            if (pg_stuff_ref) {
            
               SendNotification(UseMemory(pg_stuff_ref), PGN_FAILURE, mem_globals.last_error, NULL);
               UnuseMemory(pg_stuff_ref);
            }
         }
         
         PG_ENDTRY
         
         break;

      case WM_CREATE: {
      
            CREATESTRUCT PG_FAR  *create_info;
            shape_ref         page_area, vis_area;
            rectangle         client_rect;
            long           pgnew_flags, win_flags;
#ifdef WINDOWS_PLATFORM
            RECT           client_r;
#endif
            create_info = (CREATESTRUCT PG_FAR *)lParam;
            pg_stuff_ref = MemoryAllocClear(&mem_globals, 1, sizeof(paige_control), 0);            
            SetWindowLong(hWnd, 0, (long)pg_stuff_ref);
            pg_stuff = UseMemory(pg_stuff_ref);
            
            pg_stuff->extra_scroll_h = 16;
            pg_stuff->extra_scroll_v = 0;
            pg_stuff->paint_mode = pg_stuff->scroll_mode = bits_emulate_or;
            pg_stuff->update_mode = pg_stuff->print_mode = best_way;
            pg_stuff->insert_mode = key_insert_mode;
            pg_stuff->win_flags = win_flags = create_info->style;

#ifdef MAC_PLATFORM
            pg_stuff->ex_styles = create_info->ext_winflags;
            pg_stuff->parent = hWnd;
                pg_stuff->control_id = (WORD)create_info->refCon;
                pg_stuff->myself = hWnd;
            pg_stuff->notification = PGN_CHANGEFLAG;
            
            if (create_info->style & WS_NONSIZEABLE)
                  pg_stuff->ex_styles |= PGS_NONSIZEABLE;

            RectToRectangle(&create_info->bounds, &client_rect);
            RectToRectangle(&create_info->page, &page_rect);
            RectToRectangle(&create_info->w_ptr->portRect, &vis_rect);
            pgSectRect(&vis_rect, &client_rect, &vis_rect);
            page_area = pgRectToShape(&mem_globals, &page_rect);
            vis_area = pgRectToShape(&mem_globals, &vis_rect);
            pg_stuff->page_rect = page_rect;
            pg_stuff->vis_rect = vis_rect;
            pg_stuff->columns = 1;

            pgnew_flags = NO_LF_BIT | COUNT_LINES_BIT;

#ifdef DOING_EQ
            pgnew_flags |= NO_DEFAULT_LEADING;
#endif

      // Set up the "flags" according to the requested edit style(s):
            
            if (win_flags & ES_CENTER)
               paige_globals.def_par.justification = justify_center;
            else
            if (win_flags & ES_RIGHT)
               paige_globals.def_par.justification = justify_right;
            else
               paige_globals.def_par.justification = justify_left;
            
            if (pg_stuff->ex_styles & PGS_LINEDITMODE)
               pgnew_flags |= NO_WRAP_BIT;
            
            if (win_flags & ES_READONLY)
               pgnew_flags |= NO_EDIT_BIT;

            pg_stuff->flags |= PARENT_IS_MDI;
            pg_stuff->notification |= PGN_CHANGEFLAG;

            pg_stuff->pg = pgNew(&paige_globals, (generic_var)create_info->w_ptr, vis_area, page_area, MEM_NULL, pgnew_flags);
            pgSetTabBase(pg_stuff->pg, TAB_BOUNDS_RELATIVE);
            SetPageArea(pg_stuff);
#endif
#ifdef WINDOWS_PLATFORM
// Windows code
            
            pg_stuff->ex_styles = PGS_VISPAGE | PGS_SMARTQUOTES | PGS_NOLEADING | PGS_SMARTCUT | PGS_SCALEVISRGN;

            pg_stuff->parent = create_info->hwndParent;
                pg_stuff->control_id = (WORD)create_info->hMenu;
                pg_stuff->myself = hWnd;

            GetClientRect(create_info->hwndParent, &client_r);
            RectToRectangle(&client_r, &client_rect);

            page_rect = client_rect;
            
            if (create_info->y > client_r.top)
               page_rect.top_left.v = (long)create_info->y;
            if (create_info->x > client_r.left)
               page_rect.top_left.h = (long)create_info->x;
   
            if (create_info->cy)
               page_rect.bot_right.v = page_rect.top_left.v + (long)create_info->cy;
            if (create_info->cx)
               page_rect.bot_right.h = page_rect.top_left.h + (long)create_info->cx;
                
          // Make page rect local since we are a window in ourselves
          
               pgOffsetRect(&page_rect, client_rect.top_left.h - page_rect.top_left.h,
                     client_rect.top_left.v - page_rect.top_left.v);
  
            pgSectRect(&page_rect, &client_rect, &vis_rect);
            page_area = pgRectToShape(&mem_globals, &page_rect);
            vis_area = pgRectToShape(&mem_globals, &vis_rect);
            pg_stuff->page_rect = page_rect;
            pg_stuff->vis_rect = vis_rect;
            pg_stuff->columns = 1;

            pgnew_flags = NO_DEFAULT_LEADING | NO_LF_BIT | COUNT_LINES_BIT | SMART_QUOTES_BIT;

      // Set up the "flags" according to the requested edit style(s):
            
            if (win_flags & ES_CENTER)
               paige_globals.def_par.justification = justify_center;
            else
            if (win_flags & ES_RIGHT)
               paige_globals.def_par.justification = justify_right;
            else
               paige_globals.def_par.justification = justify_left;
            
            if (!(win_flags & ES_MULTILINE))
               pgnew_flags |= NO_WRAP_BIT;
            
            if (win_flags & ES_READONLY)
               pgnew_flags |= NO_EDIT_BIT;
                
                pgnew_flags |= DEACT_BIT;
            pg_stuff->pg = pgNew(&paige_globals, PG_LONGWORD(generic_var)hWnd, vis_area, page_area, MEM_NULL, pgnew_flags);
#endif
            pgDisposeShape(page_area);
            pgDisposeShape(vis_area);
            
            pg_rec = UseMemory(pg_stuff->pg);

            pg_rec->doc_info.repeat_slop = DEF_REPEAT_SLOP;
            pg_rec->doc_info.repeat_offset.v = DEF_REPEAT_OFFSET;
                pg_rec->doc_info.attributes |= COLOR_VIS_BIT;
                pg_rec->append_v = DEF_SCROLL_APPEND_V;
                pg_rec->append_h = DEF_SCROLL_APPEND_H;
                
                pg_rec->notification = pg_stuff->notification;
                pg_rec->custom_control = pg_stuff_ref;

            if (!(pg_stuff->win_flags & ES_AUTOVSCROLL))
               pg_rec->flags2 |= NO_VAUTOSCROLL;
            if (!(pg_stuff->win_flags & ES_AUTOHSCROLL))
               pg_rec->flags2 |= NO_HAUTOSCROLL;

            UnuseMemory(pg_stuff->pg);

#ifdef DOING_EQ
            eqInitInstance(&eq_globals, pg_stuff->pg, 0);
#endif

      #ifdef MAC_PLATFORM

            UpdateScrollbars(hWnd, pg_stuff, FALSE);
      #endif
         }

         break;

      case WM_CUT:
         the_scrap = MEM_NULL;

         PG_TRY(&mem_globals) {
            
            pg_stuff = GetPGStuff(hWnd, &pg_stuff_ref);
   
               if (!(pg_stuff->ex_styles & PGS_NOAUTOUNDO)) {
               
               if (pg_stuff->undo)
                  pgDisposeUndo(pg_stuff->undo);
   
               pg_stuff->undo = pgPrepareUndo(pg_stuff->pg, undo_delete, NULL);
            }
            
            the_scrap = pgCut(pg_stuff->pg, NULL, pg_stuff->update_mode);
            pgFailZero(&mem_globals, the_scrap);
            
      #ifdef WINDOWS_PLATFORM
            OpenClipboard(hWnd);
      #endif
            if (!SendNotification(pg_stuff, PGN_WRITECLIPBOARD, 0, (void PG_FAR *)the_scrap))
               pgPutScrap(the_scrap, clipboard_format, pg_void_scrap);

      #ifdef WINDOWS_PLATFORM
            CloseClipboard();
      #endif
            pgDispose(the_scrap);
            the_scrap = MEM_NULL;
               pgAdjustScrollMax(pg_stuff->pg, pg_stuff->update_mode);
               UpdateScrollbars(hWnd, pg_stuff, TRUE);
               NotifyOverflow(pg_stuff);
         }
         
         PG_CATCH {
            
            if (the_scrap)
               pgDispose(the_scrap);
            
            if (pg_stuff_ref) {
            
               SendNotification(UseMemory(pg_stuff_ref), PGN_FAILURE, mem_globals.last_error, NULL);
               UnuseMemory(pg_stuff_ref);
            }
         }

         PG_ENDTRY
         
         break;
      
      case WM_DESTROY:
         response = 0;

         if ((pg_stuff = GetPGStuff(hWnd, &pg_stuff_ref)) != NULL) {

#ifdef WINDOWS_PLATFORM
            {
                   HDC        hDC;
                   
                   if (hDC = (HDC)pgGetPrintDevice(pg_stuff->pg))
                     DeleteDC(hDC);
     
                   if (pg_stuff->page_pen)
                     DeleteObject((HANDLE)pg_stuff->page_pen);
                   if (pg_stuff->frame_pen)
                     DeleteObject((HANDLE)pg_stuff->frame_pen);
                   if (pg_stuff->page_break_pen)
                     DeleteObject((HANDLE)pg_stuff->page_break_pen);
               }
#endif
            if (pg_stuff->undo)
               pgDisposeUndo(pg_stuff->undo);

            pgDispose(pg_stuff->pg);
         
            UnuseAndDispose(pg_stuff_ref);
            pg_stuff_ref = MEM_NULL;
            SetWindowLong(hWnd, 0, 0);
            }
 
         break;
      
      case WM_GETDLGCODE:
         #ifdef WINDOWS_PLATFORM
         {
            long     dlog_flags = DLGC_HASSETSEL | DLGC_WANTCHARS | DLGC_WANTARROWS;
            
            if (pg_stuff = GetPGStuff(hWnd, &pg_stuff_ref)) {
               
               if (pg_stuff->win_flags & ES_WANTRETURN)
                  dlog_flags |= DLGC_WANTALLKEYS;
               
               UnuseMemory(pg_stuff_ref);
            }
            
            return      dlog_flags;
         }
         #endif
         break;

      case WM_GETFONT:
         pg_stuff = GetPGStuff(hWnd, &pg_stuff_ref);
         pgGetStyleInfo(pg_stuff->pg, NULL, FALSE, &info, &mask);
         UnuseMemory(pg_stuff_ref);

         return   (long)info.machine_var;
               
      case WM_GETTEXT:
         if (wParam > 0) {
            text_block_ptr       block;
            pg_char_ptr          user_ptr;
            long              max_size, actual_size, block_size;
            
            pg_stuff = GetPGStuff(hWnd, &pg_stuff_ref);
            pg_rec = UseMemory(pg_stuff->pg);
            block = UseMemory(pg_rec->t_blocks);
            user_ptr = (pg_char_ptr)lParam;
            max_size = (long)wParam;
            max_size -= sizeof(char);

            for (actual_size = 0; actual_size < max_size; ++block) {
               
               block_size = block->end - block->begin;
               
               if ((actual_size + block_size) > max_size)
                  block_size = max_size - actual_size;
               
               pgBlockMove(UseMemory(block->text), user_ptr, block_size * sizeof(pg_char));
               UnuseMemory(block->text);
               
               actual_size += block_size;
               user_ptr += block_size;

               if (block->end == pg_rec->t_length)
                  break;
            }
            
            *user_ptr = 0;

            UnuseMemory(pg_rec->t_blocks);
            UnuseMemory(pg_stuff->pg);
         }

         break;

      case WM_HSCROLL:
         pg_stuff = GetPGStuff(hWnd, &pg_stuff_ref);
         draw_mode = pg_stuff->scroll_mode;

#ifdef MAC_PLATFORM
         scroll_position = LOWORD(lParam);
         scroll_code = wParam;
#endif

#ifdef WIN16_COMPILE
         scroll_position = LOWORD(lParam);
         scroll_code = wParam;
#elif defined(WIN32_COMPILE)
         scroll_position = HIWORD(wParam);
         scroll_code = LOWORD(wParam);
#endif
         switch (scroll_code) {
            
            case SB_PAGEDOWN:
               pgScroll(pg_stuff->pg, -scroll_page, scroll_none, draw_mode);
               break;

            case SB_LINEDOWN:
               pgScroll(pg_stuff->pg, -scroll_unit, scroll_none, draw_mode);
               break;

            case SB_PAGEUP:
               pgScroll(pg_stuff->pg, scroll_page, scroll_none, draw_mode);
               break;

            case SB_LINEUP:
               pgScroll(pg_stuff->pg, scroll_unit, scroll_none, draw_mode);
               break;

            case SB_THUMBPOSITION:
            case SB_THUMBTRACK:    {
                  short    cur_h, cur_v, max_h, max_v;
                  
                  pgGetScrollValues(pg_stuff->pg, &cur_h, &cur_v, &max_h, &max_v);
                  pgSetScrollValues(pg_stuff->pg, scroll_position, cur_v,
                        (pg_boolean)(scroll_code == SB_THUMBPOSITION), draw_mode);
               }

               break;

            case SB_TOP:
               pgScroll(pg_stuff->pg, scroll_home, scroll_none, draw_mode);
               break;

            case SB_BOTTOM:
               pgScroll(pg_stuff->pg, scroll_end, scroll_none, draw_mode);
               break;
         }
 
            UpdateScrollbars(hWnd, pg_stuff, FALSE);
         break;
      
      case WM_KEYDOWN:

#ifdef MAC_PLATFORM

         if ((pg_stuff = GetPGStuff(hWnd, &pg_stuff_ref)) != NULL) {
            
            if (lParam & shiftKey)
               pg_stuff->modifiers |= EXTEND_MOD_BIT;
            else
               pg_stuff->modifiers &= (~EXTEND_MOD_BIT);
            
            if (lParam & controlKey)
               pg_stuff->modifiers |= CONTROL_MOD_BIT;
            else
               pg_stuff->modifiers &= (~CONTROL_MOD_BIT);
            
            UnuseMemory(pg_stuff_ref);
            pg_stuff_ref = MEM_NULL;
         }
#endif
         switch (wParam) {
         
   #ifdef WINDOWS_PLATFORM
            case VK_SHIFT:
               if (pg_stuff = GetPGStuff(hWnd, &pg_stuff_ref))
                  pg_stuff->modifiers |= EXTEND_MOD_BIT;

               break;
            
            case VK_CONTROL:
               if (pg_stuff = GetPGStuff(hWnd, &pg_stuff_ref))
                  pg_stuff->modifiers |= CONTROL_MOD_BIT;
               break;
   #endif
            case VK_LEFT:
               SendMessage(hWnd, WM_CHAR, paige_globals.left_arrow_char, 0L);
               break;

            case VK_UP:
               SendMessage(hWnd, WM_CHAR, paige_globals.up_arrow_char, 0L);
               break;

            case VK_RIGHT:
               SendMessage(hWnd, WM_CHAR, paige_globals.right_arrow_char, 0L);
               break;

            case VK_DOWN:
               SendMessage(hWnd, WM_CHAR, paige_globals.down_arrow_char, 0L);
               break;

            case VK_HOME:
               if ((pg_stuff = GetPGStuff(hWnd, &pg_stuff_ref)) != NULL) {

                  if (pg_stuff->modifiers & EXTEND_MOD_BIT)
                     caret_verb = EXTEND_CARET_FLAG;
                  else
                     caret_verb = 0;
                  
                  if (pg_stuff->modifiers & CONTROL_MOD_BIT)
                     caret_verb |= home_caret;
                  else
                     caret_verb |= begin_line_caret;
                     
                  pgSetCaretPosition(pg_stuff->pg, caret_verb, TRUE);
                  
                  if (!pgNumSelections(pg_stuff->pg))
                     UpdateScrollbars(hWnd, pg_stuff, TRUE);
               }

               break;
               
            case VK_END:
               if ((pg_stuff = GetPGStuff(hWnd, &pg_stuff_ref)) != NULL) {

                  if (pg_stuff->modifiers & EXTEND_MOD_BIT)
                     caret_verb = EXTEND_CARET_FLAG;
                  else
                     caret_verb = 0;
                  
                  if (pg_stuff->modifiers & CONTROL_MOD_BIT)
                     caret_verb |= doc_bottom_caret;
                  else
                     caret_verb |= end_line_caret;
                     
                  pgSetCaretPosition(pg_stuff->pg, caret_verb, TRUE);
                  
                  if (!pgNumSelections(pg_stuff->pg))
                     UpdateScrollbars(hWnd, pg_stuff, TRUE);
               }

               break;
               
            case VK_PRIOR:
               SendMessage(hWnd, WM_VSCROLL, SB_PAGEUP, 0L);
               break;

            case VK_NEXT:
               SendMessage(hWnd, WM_VSCROLL, SB_PAGEDOWN, 0L);
               break;
            
            case VK_DELETE:
               SendMessage(hWnd, WM_CHAR, paige_globals.fwd_delete_char, 0L);
               break;
               
         }
         
         break;

      case WM_KEYUP:
#ifdef WINDOWS_PLATFORM          
         if (pg_stuff = GetPGStuff(hWnd, &pg_stuff_ref)) {

            switch (wParam) {

               case VK_SHIFT:
                  pg_stuff->modifiers &= (~EXTEND_MOD_BIT);
                  break;
            
               case VK_CONTROL:
                  pg_stuff->modifiers &= (~CONTROL_MOD_BIT);
                  break;
            }
         }
#endif
         break;

      case WM_KILLFOCUS:
         if ((pg_stuff = GetPGStuff(hWnd, &pg_stuff_ref)) != NULL) {
            
            if (pg_stuff->win_flags & ES_NOHIDESEL)
               if (pgNumSelections(pg_stuff->pg))
                  break;

            pgSetHiliteStates(pg_stuff->pg, deactivate_verb, no_change_verb, TRUE);

#ifdef MAC_PLATFORM
            ActivatePGScrollbars(hWnd, WM_KILLFOCUS);
#endif
         }

         break;
         
      case WM_LBUTTONDBLCLK:
      case WM_LBUTTONDOWN:
         pg_stuff = GetPGStuff(hWnd, &pg_stuff_ref);
         
         response = TRUE;
         SplitIntoLongs(lParam, &pg_mouse.v, &pg_mouse.h);

         if ((pg_stuff->mouse_loc = pgPtInView(pg_stuff->pg, &pg_mouse, NULL)) != 0) {
            
            if (pg_stuff->mouse_loc & WITHIN_VIS_AREA) {
      #ifdef WINDOWS_PLATFORM
               long     focus_state;
      
               focus_state = pgGetAttributes(pg_stuff->pg);
               
               if ((focus_state & (DEACT_BIT | PERM_DEACT_BIT)) == DEACT_BIT) {                 
                   
                   SetFocus(hWnd);
                   pg_stuff->mouse_loc = 0;
                  break;
               }
      #endif
            }
            else {
                
                pg_stuff->mouse_loc = 0;
               break;
            }
            
            mouse_info.yMouse = pg_mouse.v;
            mouse_info.xMouse = pg_mouse.h;
            mouse_info.wParam = pg_stuff->wParam = wParam;
            mouse_info.firstPos = mouse_info.endPos = 0;
            mouse_info.dblClick = mouse_info.trplClick = FALSE;

            if (message == WM_LBUTTONDBLCLK) {

               mouse_info.dblClick = TRUE;

               if (pg_stuff->flags & DOUBLECLICK_FLAG)
                  mouse_info.trplClick = TRUE;
               
               pg_stuff->flags |= DOUBLECLICK_FLAG;
            }
            else
               pg_stuff->flags &= (~DOUBLECLICK_FLAG);
               
            if (pg_stuff->mouse_loc & WITHIN_WRAP_AREA)
               if (pg_stuff->notification & PGN_PAGECLICKFLAG) {
                  
                  mouse_info.area = (short)pgPtInContainer(pg_stuff->pg, &pg_mouse, NULL);
                  
                  if ((response = SendNotification(pg_stuff, PGN_PAGECLICK, 0, &mouse_info)) != 0) {
                     
                     handle_button_up(pg_stuff, lParam);
                     break;
                  }
            }

            if (pg_stuff->mouse_loc & WITHIN_EXCLUDE_AREA)
               if (pg_stuff->notification & PGN_EXCLICKFLAG) {

                  mouse_info.area = (short)pgPtInExclusion(pg_stuff->pg, &pg_mouse, NULL);
                  
                  if ((response = SendNotification(pg_stuff, PGN_EXCLICK, 0, &mouse_info)) != 0) {
                     
                     handle_button_up(pg_stuff, lParam);
                     break;
                  }
            }

            if (message == WM_LBUTTONDBLCLK) {
               
               if (mouse_info.trplClick)
                  pg_stuff->modifiers = PAR_MOD_BIT;
               else
                  pg_stuff->modifiers = WORD_MOD_BIT;
            }
            else
               pg_stuff->modifiers = 0;
            
            if (wParam & MK_SHIFT)
               pg_stuff->modifiers |= EXTEND_MOD_BIT;
            if (wParam & MK_CONTROL)
               pg_stuff->modifiers |= DIS_MOD_BIT;
   
      #ifdef WINDOWS_PLATFORM
            SetCapture(hWnd);
      #endif
            pgDragSelect(pg_stuff->pg, &pg_mouse, mouse_down, pg_stuff->modifiers, 0, TRUE);

            mouse_info.area = pg_stuff->mouse_loc;

            if (message == WM_LBUTTONDOWN && (pg_stuff->notification & PGN_CLICKFLAG)) {

               pgGetSelection(pg_stuff->pg, &mouse_info.firstPos, &mouse_info.endPos);

               if ((response = SendNotification(pg_stuff, PGN_CLICK, 0, &mouse_info)) != 0) {
                  
                  handle_button_up(pg_stuff, lParam);
                  break;
               }
            }
            else
            if (message == WM_LBUTTONDBLCLK && (pg_stuff->notification & PGN_DBLCLICKFLAG)) {

               pgGetSelection(pg_stuff->pg, &mouse_info.firstPos, &mouse_info.endPos);

               if ((response = SendNotification(pg_stuff, PGN_DBLCLICK, 0, &mouse_info)) != 0) {
                  
                  handle_button_up(pg_stuff, lParam);
                  break;
               }
            }
         }
         else
            pg_stuff->flags &= (~DOUBLECLICK_FLAG);

         break;

      case WM_LBUTTONUP:
         pg_stuff = GetPGStuff(hWnd, &pg_stuff_ref);
         
         if (pg_stuff->mouse_loc == 0)
            break;

         if (pg_stuff->notification & PGN_CLICKMOUSEUPFLAG) {
            
            SetupDragInfo(pg_stuff, &pg_mouse, &mouse_info);
            SendNotification(pg_stuff, PGN_CLICKMOUSEUP, 0, &mouse_info);
         }

         handle_button_up(pg_stuff, lParam);
         break;

      case WM_MOVE:
      #ifdef MAC_PLATFORM
         pg_stuff = GetPGStuff(hWnd, &pg_stuff_ref);
         
         if (pg_stuff->flags & PARENT_IS_MDI) {
         
            SplitIntoLongs(lParam, &new_v, &new_h);

            SizePGWindow(hWnd, message, &new_h, &new_v);       

            pgAreaBounds(pg_stuff->pg, NULL, &vis_rect);
            new_h -= vis_rect.top_left.h;
            new_v -= vis_rect.top_left.v;       
            pgOffsetRect(&pg_stuff->page_rect, new_h, new_v);
            pgOffsetRect(&vis_rect, new_h, new_v);
            pgSetAreaBounds(pg_stuff->pg, NULL, &vis_rect);
            SetPageArea(pg_stuff);
         }
        #endif 
         break;

      case WM_MOUSEMOVE:
         pg_stuff = GetPGStuff(hWnd, &pg_stuff_ref);

         SplitIntoLongs(lParam, &pg_mouse.v, &pg_mouse.h);
         
         if (pg_stuff->mouse_loc) {

            pgDragSelect(pg_stuff->pg, &pg_mouse, mouse_moved, pg_stuff->modifiers, 0,
                  (pg_boolean)((pg_stuff->win_flags & (long)(ES_AUTOVSCROLL | ES_AUTOHSCROLL)) != 0));
             
             UpdateScrollbars(hWnd, pg_stuff, FALSE);
             
             if (pg_stuff->win_flags & (long)(ES_AUTOVSCROLL | ES_AUTOHSCROLL)) {
                    
                 pgAreaBounds(pg_stuff->pg, NULL, &vis_rect);
                 
       #ifdef WINDOWS_PLATFORM
            {
                    MSG          msg;

                 if (!PeekMessage(&msg, hWnd, WM_MOUSEFIRST, WM_MOUSELAST, PM_NOREMOVE))   
                  if (!pgPtInRect(&pg_mouse, &vis_rect))
                     PostMessage(hWnd, WM_MOUSEMOVE, wParam, lParam);
             }
       #endif

             }

            if (pg_stuff->notification & PGN_MOUSEMOVEFLAG) {
               
               SetupDragInfo(pg_stuff, &pg_mouse, &mouse_info);
               SendNotification(pg_stuff, PGN_MOUSEMOVE, 0, &mouse_info);
            }
         }
         else {
            short          mouse_area = 0;
            
            if ((pg_stuff->embed_click = pgPtInEmbed(pg_stuff->pg, &pg_mouse, NULL, NULL, TRUE)) != 0)
               mouse_area = PGPT_INEMBED;

            mouse_area |= pgPtInView(pg_stuff->pg, &pg_mouse, NULL);

            if (pg_stuff->notification & PGN_CURSORFLAG) {
               
               pgFillBlock(&mouse_info, sizeof(PAIGEMOUSESTRUCT), 0);
               mouse_info.area = mouse_area;
               mouse_info.yMouse = pg_mouse.v;
               mouse_info.xMouse = pg_mouse.h;
               mouse_info.wParam = wParam;

               if ((response = SendNotification(pg_stuff, PGN_CURSOR, 0, &mouse_info)) != 0)
                  break;
            }

            mouse_area &= (WITHIN_TEXT | WITHIN_VIS_AREA | PGPT_INEMBED);
      
      #ifdef MAC_PLATFORM
            
            if (mouse_area == (WITHIN_TEXT | WITHIN_VIS_AREA))
               SetPGCNTLCursor(iBeamCursor);
            else
               SetPGCNTLCursor(0);
      #endif
      #ifdef WINDOWS_PLATFORM
            if (mouse_area == (WITHIN_TEXT | WITHIN_VIS_AREA))
               SetCursor(ibeam_cursor);
            else
               SetCursor(arrow_cursor);
      #endif

         }

         break;

      case WM_PAINT: {
      
            PAINTSTRUCT    ps;
            
            pg_stuff = GetPGStuff(hWnd, &pg_stuff_ref);
            BeginPaint(hWnd, &ps);
            pgErasePageArea(pg_stuff->pg, MEM_NULL);
            pgDisplay(pg_stuff->pg, NULL, MEM_NULL, MEM_NULL, NULL, pg_stuff->paint_mode);

              if (pg_stuff->frame_pen && pg_stuff->frame_pen_width) {
               rectangle         vis_rect;
               HPEN           old_pen;
               HDC               hdc;

               vis_rect = pg_stuff->vis_rect;
               pg_rec = UseMemory(pg_stuff->pg);
               hdc = (HDC)pgGetPlatformDevice(&pg_rec->port);

#ifdef MAC_PLATFORM
               
               if (pg_stuff->win_flags & WS_VSCROLL)
                  vis_rect.bot_right.h += 16;
               if (pg_stuff->win_flags & WS_HSCROLL)
                  vis_rect.bot_right.v += 16;
#endif
               old_pen = SelectObject(hdc, (HPEN)pg_stuff->frame_pen);
               draw_rectangle(hdc, &vis_rect, NULL, &pg_rec->scale_factor,
                     (short)-pg_stuff->frame_pen_width, (short)pg_stuff->frame_pen_width);
                SelectObject(hdc, old_pen);
                
                pgReleasePlatformDevice(&pg_rec->port);
                UnuseMemory(pg_stuff->pg);
            }

            EndPaint(hWnd, &ps);
            NotifyOverflow(pg_stuff);
         }
         break;

      case WM_PASTE:
         the_scrap = MEM_NULL;

         #ifdef WINDOWS_PLATFORM
         OpenClipboard(hWnd);
         #endif
         
         pg_stuff = GetPGStuff(hWnd, &pg_stuff_ref);
         
         if ((the_scrap = (pg_ref)SendNotification(pg_stuff, PGN_GETCLIPBOARD, TRUE, 0)) == MEM_NULL)
            the_scrap = pgGetScrap(&paige_globals, clipboard_format, EmbedCallbackProc);
         
         if (the_scrap) {
         
            PG_TRY(&mem_globals) {
               
               if (!(pg_stuff->ex_styles & PGS_NOAUTOUNDO)) {
               
                  if (pg_stuff->undo)
                     pgDisposeUndo(pg_stuff->undo);
                  
                  pg_stuff->undo = MEM_NULL;
                  pg_stuff->undo = pgPrepareUndo(pg_stuff->pg, undo_paste, (void *)the_scrap);
               }
   
               pgPaste(pg_stuff->pg, the_scrap, CURRENT_POSITION, FALSE, pg_stuff->update_mode);
                pgDispose(the_scrap);
                the_scrap = MEM_NULL;
                pgAdjustScrollMax(pg_stuff->pg, pg_stuff->update_mode);
                UpdateScrollbars(hWnd, pg_stuff, TRUE);
                SendChangeCommand(pg_stuff);
                NotifyOverflow(pg_stuff);
            }
            PG_CATCH
               
               if (the_scrap)
                  pgDispose(the_scrap);

               if (pg_stuff_ref) {
                  
                  pg_stuff = UseMemory(pg_stuff_ref);

                  if (pg_stuff->undo)
                     pgDisposeUndo(pg_stuff->undo);
                  
                  pg_stuff->undo = MEM_NULL;

                  SendNotification(pg_stuff, PGN_FAILURE, mem_globals.last_error, NULL);
                  UnuseMemory(pg_stuff_ref);
               }
               
            PG_ENDTRY
         }

         #ifdef WINDOWS_PLATFORM
         CloseClipboard();
         #endif

         break;
         
      case WM_SETFOCUS:
         if ((pg_stuff = GetPGStuff(hWnd, &pg_stuff_ref)) != NULL)
            pgSetHiliteStates(pg_stuff->pg, activate_verb, no_change_verb, TRUE);

#ifdef MAC_PLATFORM
         ActivatePGScrollbars(hWnd, WM_SETFOCUS);
#endif
         break;

#ifdef WINDOWS_PLATFORM
      case WM_SETFONT: {
            LOGFONT        lf;
            
            pg_stuff = GetPGStuff(hWnd, &pg_stuff_ref);
            PrepareStyleUndo(pg_stuff);
            
            GetObject((HANDLE)wParam, sizeof(LOGFONT), (PGSTR)&lf);
            UnuseMemory(pg_stuff_ref);

            return   (StyleMessageProc(hWnd, PG_SETFONT, (WORD)lParam, (LPARAM)&lf));
         }

         break;
#endif

      case WM_SIZE:
         pg_stuff = GetPGStuff(hWnd, &pg_stuff_ref);
         
         if (!(pg_stuff->ex_styles & PGS_NONSIZEABLE)) {
      
            memory_ref     subref_state;
            
            pg_rec = UseMemory(pg_stuff->pg);
            subref_state = pgGetSubrefState(pg_rec, FALSE, TRUE);
            SplitIntoLongs(lParam, &new_v, &new_h);

      #ifdef MAC_PLATFORM
            SizePGWindow(hWnd, message, &new_h, &new_v);
      #endif
            pg_stuff->vis_rect.bot_right.h = pg_stuff->vis_rect.top_left.h + new_h;
            pg_stuff->vis_rect.bot_right.v = pg_stuff->vis_rect.top_left.v + new_v;

            if (pg_stuff->ex_styles & PGS_VISPAGE)
               SetPageArea(pg_stuff);
            else
               pgSetAreaBounds(pg_stuff->pg, NULL, &pg_stuff->vis_rect);

            pgRestoreSubRefs(pg_rec, subref_state);
            UnuseMemory(pg_stuff->pg);          
          }

         break;

      case WM_SETTEXT: {
            long        textsize;
            
            PG_TRY(&mem_globals) {
            
               pg_stuff = GetPGStuff(hWnd, &pg_stuff_ref);
               
               if (lParam != 0)
                  textsize = cstring_length((PGSTR)lParam);
               else
                  textsize = 0;

               if (pg_stuff->undo)
                  pgDisposeUndo(pg_stuff->undo);
   
               pg_stuff->undo = MEM_NULL;
               
               if (textsize == 0) {
                  select_pair       selection;
                  
                  selection.begin = 0;
                  selection.end = pgTextSize(pg_stuff->pg);
                  pgDelete(pg_stuff->pg, &selection, pg_stuff->update_mode);
                  pgAdjustScrollMax(pg_stuff->pg, pg_stuff->update_mode);
                     UpdateScrollbars(hWnd, pg_stuff, TRUE);
               }
               else {
               
                  pgSetSelection(pg_stuff->pg, 0, pgTextSize(pg_stuff->pg), 0, FALSE);
                  pgInsert(pg_stuff->pg, (pg_char_ptr)lParam, textsize, 0,
                     data_insert_mode, 0, pg_stuff->update_mode);
   
                  NotifyOverflow(pg_stuff);
               }
            }

            PG_CATCH
               if (pg_stuff_ref) {
               
                  SendNotification(UseMemory(pg_stuff_ref), PGN_FAILURE, mem_globals.last_error, NULL);
                  UnuseMemory(pg_stuff_ref);
               }
            
            PG_ENDTRY
         }

         break;

      case WM_UNDO:
         EditMessageProc(hWnd, PG_UNDO, TRUE, lParam);
         response = TRUE;
         
         break;
      
      case WM_VSCROLL:
         pg_stuff = GetPGStuff(hWnd, &pg_stuff_ref);
         draw_mode = pg_stuff->scroll_mode;

#ifdef MAC_PLATFORM
         scroll_position = LOWORD(lParam);
         scroll_code = wParam;
#endif

#ifdef WIN16_COMPILE
         scroll_position = LOWORD(lParam);
         scroll_code = wParam;
#elif defined(WIN32_COMPILE)
         scroll_position = HIWORD(wParam);
         scroll_code = LOWORD(wParam);
#endif
         switch (scroll_code) {
            
            case SB_PAGEDOWN:
               pgScroll(pg_stuff->pg, scroll_none, -scroll_page, draw_mode);
               break;

            case SB_LINEDOWN:
               pgScroll(pg_stuff->pg, scroll_none, -scroll_unit, draw_mode);
               break;

            case SB_PAGEUP:
               pgScroll(pg_stuff->pg, scroll_none, scroll_page, draw_mode);
               break;

            case SB_LINEUP:
               pgScroll(pg_stuff->pg, scroll_none, scroll_unit, draw_mode);
               break;

            case SB_THUMBPOSITION:
            case SB_THUMBTRACK: {
                  short    cur_h, cur_v, max_h, max_v;
                  
                  pgGetScrollValues(pg_stuff->pg, &cur_h, &cur_v, &max_h, &max_v);
                  pgSetScrollValues(pg_stuff->pg, cur_h, scroll_position,
                        (pg_boolean)(scroll_code == SB_THUMBPOSITION), draw_mode);
               }
                break;
                
            case SB_TOP:
               pgScroll(pg_stuff->pg, scroll_none, scroll_home, draw_mode);
               break;

            case SB_BOTTOM:
               pgScroll(pg_stuff->pg, scroll_none, scroll_end, draw_mode);
               break;
         }
            
            UpdateScrollbars(hWnd, pg_stuff, FALSE);
         break;

   #ifdef WIN32_COMPILE
      case EM_SETSEL:
      case EM_GETRECT:
      case EM_SETRECT:
      case EM_SETRECTNP:
      case EM_LINESCROLL:
      case EM_GETMODIFY:
      case EM_SETMODIFY:
      case EM_GETLINECOUNT:
      case EM_LINEINDEX:
      case EM_LINELENGTH:
      case EM_REPLACESEL:
      case EM_GETLINE:        
      case EM_LIMITTEXT:
      case EM_CANUNDO:        
      case EM_LINEFROMCHAR:
      case EM_SETTABSTOPS:
      case EM_EMPTYUNDOBUFFER:
      case EM_UNDO:
         response = edit_win_proc(hWnd, message, wParam, lParam);
         break;
   #endif

      default:
         response = DefWindowProc(hWnd, message, wParam, lParam);
         break;
   }

   if (pg_stuff_ref)
      UnuseMemory(pg_stuff_ref);

   return   response;
}


/******************************* Local Functions ******************************/



/* edit_win_proc handles all the "EDIT" messages. */

static long pascal edit_win_proc (HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
   paige_control_ptr pg_stuff;         // One of these in each control
   memory_ref        pg_stuff_ref;     // A memory_ref that holds above
   rectangle         vis_rect, page_rect;
   long           response, line_begin, line_end, textsize;

   response = 0;
   pg_stuff = GetPGStuff(hWnd, &pg_stuff_ref);

   switch (message) {
   
      case EM_SETSEL:
         pgSetSelection(pg_stuff->pg, (long)LOWORD(lParam), (long)HIWORD(lParam), 0, TRUE);
         break;

      case EM_GETRECT:
         pgAreaBounds(pg_stuff->pg, &page_rect, &vis_rect);
         
         if (pg_stuff->ex_styles & PGS_VISPAGE)
            RectangleToRect(&vis_rect, NULL, (Rect PG_FAR *)lParam);
         else
            RectangleToRect(&page_rect, NULL, (Rect PG_FAR *)lParam);

         break;

      case EM_SETRECT:
      case EM_SETRECTNP:
         pgAreaBounds(pg_stuff->pg, &page_rect, &vis_rect);
         RectToRectangle((Rect PG_FAR *)lParam, &page_rect);
         
         if (pg_stuff->ex_styles & PGS_VISPAGE)
            vis_rect = page_rect;
         
         pgSetAreaBounds(pg_stuff->pg, &page_rect, &vis_rect);
         
         if (message == EM_SETRECT)
            RepaintControl(hWnd, pg_stuff, FALSE);
         else
            pg_stuff->flags |= SCROLLBARS_DIRTY_FLAG;

         break;

      case EM_LINESCROLL: {
            rectangle      char_rect;
            long        h_scroll, v_scroll, amount_h, amount_v;

            pgCharacterRect(pg_stuff->pg, 0, FALSE, FALSE, &char_rect);
            SplitIntoLongs(lParam, &amount_v, &amount_h);
            v_scroll = char_rect.bot_right.v - char_rect.top_left.v;
            h_scroll = 7;
            v_scroll *= amount_v;
            h_scroll *= amount_h;
            
            pgScrollPixels(pg_stuff->pg, h_scroll, v_scroll, pg_stuff->scroll_mode);
         }

         break;

      case EM_GETMODIFY:
         response = ((pgGetChangeCtr(pg_stuff->pg) != 0) || (pg_stuff->changes != 0));
         break;
         
      case EM_SETMODIFY:
         pgSetChangeCtr(pg_stuff->pg, wParam);
         pg_stuff->changes = wParam;
         break;

      case EM_GETLINECOUNT:
         response = pgNumLines(pg_stuff->pg);
         break;
         
      case EM_LINEINDEX:
         pgLineNumToOffset(pg_stuff->pg, (long)wParam, &response, &line_end);
         break;

      case EM_LINELENGTH:
         pgFindLine(pg_stuff->pg, (long)wParam, &line_begin, &line_end);
         response = line_end - line_begin;
         break;

      case EM_SETHANDLE:
      case EM_GETHANDLE:
         // not applicable to PAIGE
         break;

      case EM_REPLACESEL:
         if (!lParam)
            textsize = 0;
         else
            textsize = cstring_length((PGSTR)lParam);
            
            if (textsize == 0) {
               
               if (pgNumSelections(pg_stuff->pg) > 0) {

                  if (!(pg_stuff->ex_styles & PGS_NOAUTOUNDO)) {
                  
                  if (pg_stuff->undo)
                     pgDisposeUndo(pg_stuff->undo);
      
                  pg_stuff->undo = pgPrepareUndo(pg_stuff->pg, undo_delete, NULL);
                  }
                  
                  pgDelete(pg_stuff->pg, NULL, best_way);
               UpdateScrollbars(hWnd, pg_stuff, TRUE);
               }
            }
            else {
            
               if (!(pg_stuff->ex_styles & PGS_NOAUTOUNDO)) {
               
               if (pg_stuff->undo)
                  pgDisposeUndo(pg_stuff->undo);
   
               pg_stuff->undo = pgPrepareUndo(pg_stuff->pg, undo_insert, (void *)&textsize);
               }
               
            if (pgInsert(pg_stuff->pg, (pg_char_ptr)lParam, textsize, CURRENT_POSITION,
                  data_insert_mode, 0, pg_stuff->update_mode))
               UpdateScrollbars(hWnd, pg_stuff, TRUE);
         }

         break;

      case EM_GETLINE: {
            memory_ref     ref;
            select_pair    line_bounds;
            short PG_FAR   *maxsize;

            pgLineNumToOffset(pg_stuff->pg, (long)wParam, &line_bounds.begin, &line_bounds.end);
            
            if ((ref = pgCopyText(pg_stuff->pg, &line_bounds, all_data)) != MEM_NULL) {
               
               maxsize = (short PG_FAR *)lParam;
               response = GetMemorySize(ref);
               
               if (response > (long)*maxsize)
                  response = (long)*maxsize;
               
               if (response) {
                  
                  pgBlockMove(UseMemory(ref), maxsize, response * sizeof(pg_char));
                  UnuseMemory(ref);
               }
               
               DisposeMemory(ref);
            }
         }
         
         break;
         
      case EM_LIMITTEXT:
         pg_stuff->max_input = (wParam);
         break;

      case EM_CANUNDO:
         response = (pg_stuff->undo != MEM_NULL);
         break;

      case EM_FMTLINES:
         break;
         
      case EM_LINEFROMCHAR:
         line_begin = (long)wParam;
         
         if (line_begin < 0)
            pgGetSelection(pg_stuff->pg, &line_begin, NULL);

         response = pgOffsetToLineNum(pg_stuff->pg, line_begin, FALSE);
         break;

      case EM_SETTABSTOPS:
         break;

      case EM_SETPASSWORDCHAR:
         break;
         
      case EM_EMPTYUNDOBUFFER:
         if (pg_stuff->undo)
            pgDisposeUndo(pg_stuff->undo);
         
         pg_stuff->undo = MEM_NULL;
         break;

      case EM_GETFIRSTVISIBLELINE:
         break;
         
      case EM_SETREADONLY:
         break;
         
      case EM_SETWORDBREAKPROC:
      case EM_GETWORDBREAKPROC:
         break;
      
      case EM_UNDO:
         EditMessageProc(hWnd, PG_UNDO, TRUE, lParam);
         response = TRUE;
         break;
   
      default:
         response = DefWindowProc(hWnd, message, wParam, lParam);
         break;
   }

   UnuseMemory(pg_stuff_ref);

   return   response;
}



/* cstring_length returns the size of a null-terminated string. [We do not
use lib functions because we do not want to include them in the link]. */

static long cstring_length (PGSTR cstr)
{
   register PGSTR       ptr;
   register long        ctr;
   
   ptr = cstr;
   ctr = 0;
   
   for (;;) {
      
      if (*ptr++ == 0)
         return   ctr;
      
      ++ctr;
   }
}



/* document_draw_proc is the hook that gets called by PAIGE to draw
ornaments around the page area (like gray lines, etc.). */

STATIC_PASCAL (void) document_draw_proc (paige_rec_ptr pg, shape_ptr page_shape,
      pg_short_t r_qty, pg_short_t page_num, co_ordinate_ptr vis_offset,
      short draw_mode_used, short call_order)
{
   paige_control_ptr    pg_stuff;
   PAIGERECTSTRUCT         page_info;
   rectangle            vis_bounds, draw_bounds, union_r;
   long              scaled_inset;
   HPEN              old_pen;
   HDC                  hdc;
    pg_short_t          index;
    int                 print_mode;
   
   pgDrawPageProc(pg, page_shape, r_qty, page_num, vis_offset, draw_mode_used, call_order);
   
   if (!pg->custom_control || !draw_mode_used)
      return;

   print_mode = ((pg->flags & PRINT_MODE_BIT) != 0);

   pg_stuff = UseMemory(pg->custom_control);
   hdc = (HDC)pg->globals->current_port->machine_ref;
   pgShapeBounds(pg->vis_area, &vis_bounds);
   draw_bounds.top_left.h = vis_bounds.top_left.h;
   draw_bounds.top_left.v = vis_bounds.top_left.v;
   draw_bounds.bot_right.h = vis_bounds.bot_right.h;
   draw_bounds.bot_right.v = vis_bounds.bot_right.v;
   // draw_bounds = vis_bounds;

   if (!print_mode) {

      if (pg_stuff->page_pen && pg_stuff->page_pen_width && !(pg_stuff->ex_styles & PGS_HIDEMARGINS)
         && (pg->doc_info.attributes & (BOTTOM_FIXED_BIT | V_REPEAT_BIT))) {
   
         old_pen = SelectObject(hdc, (HPEN)pg_stuff->page_pen);
         scaled_inset = pg_stuff->page_pen_width;
         pgScaleLong(pg->port.scale.scale, 0, &scaled_inset);
         
         if (scaled_inset == 0)
            scaled_inset = 1;

         scaled_inset = -scaled_inset;
         
         for (index = 0; index < r_qty; ++index)
            draw_rectangle(hdc, &page_shape[index], vis_offset,
                  &pg->scale_factor, (short)scaled_inset,
                  pg_stuff->page_pen_width);
           
           if (pg_stuff->page_break_pen && pg->doc_info.repeat_offset.v) {
            short       break_bottom, break_right;
            
            SelectObject(hdc, (HPEN)pg_stuff->page_break_pen);
            pgShapeBounds(pg->wrap_area, &union_r);
               
            if (vis_offset) {
               
               pgOffsetRect(&draw_bounds, 0, vis_offset->v);
               pgOffsetRect(&union_r, vis_offset->h, vis_offset->v); 
               }
               
               break_bottom = (short)(union_r.bot_right.v + pg_stuff->bottom_margin);
               break_right = (short)draw_bounds.bot_right.h;
               
               if (union_r.bot_right.h > draw_bounds.bot_right.h)
                  break_right = (short)union_r.bot_right.h;
                      
            MovePenTo(hdc, (short)draw_bounds.top_left.h, break_bottom);
            PenLineTo(hdc, break_right, break_bottom);
           }
           
         SelectObject(hdc, old_pen);
      }
   }
    
// Check on app notification possibilities:

   if (pg->notification & PGN_PAGEDRAWFLAG) {

      page_info.screen_y = vis_offset->v;
      page_info.screen_x = vis_offset->h;
      pgShapeBounds(pg->wrap_area, (rectangle_ptr)&page_info.top);
      pgOffsetRect((rectangle_ptr)&page_info.top, vis_offset->h, vis_offset->v);
      page_info.query = page_num + 1;
      page_info.hDC = hdc;
      SendNotification(pg_stuff, PGN_PAGEDRAW, 0, &page_info);
   }

   if (call_order < 0) {
      
      if (pg->notification & PGN_EXDRAWFLAG) {
         pg_short_t        num_excludes;
         
         if ((num_excludes = pgNumExclusions(pg_stuff->pg)) != 0) {
            
            pgShapeBounds(pg->vis_area, &vis_bounds);
            pg->port.scale.scale = -pg->port.scale.scale;
            pgScaleRect(&pg->port.scale, NULL, &vis_bounds);
            pg->port.scale.scale = -pg->port.scale.scale;

            page_info.hDC = hdc;
            page_info.screen_y = 0;
            page_info.screen_x = 0;

            for (index = 1; index <= num_excludes; ++index) {
               
               pgGetExclusion(pg_stuff->pg, index, TRUE, FALSE, (rectangle_ptr)&page_info.top);
               page_info.query = pgGetExclusionRefCon(pg_stuff->pg, index);
               
               if (pgSectRect((rectangle_ptr)&page_info.top, &vis_bounds, NULL))
                  SendNotification(pg_stuff, PGN_EXDRAW, 0, &page_info);
            }
         }
      }
   }

   UnuseMemory(pg->custom_control);
}

/* modify_pages is the "page modify" hook. */

STATIC_PASCAL (void) modify_pages (paige_rec_ptr pg, long page_num, rectangle_ptr margins)
{
   paige_control_ptr pg_stuff;
   long           page_number;
   
   pgPageModify(pg, page_num, margins);

   if (pg->notification & PGN_PAGEMODIFYFLAG) {
      long        changes;
      
      pg_stuff = UseMemory(pg->custom_control);
      page_number = page_num + 1;

      if ((changes = SendNotification(pg_stuff, PGN_XPAGEMODIFY, 0, (void PG_FAR *)page_number)) != 0) {
         
         margins->bot_right.h = (LOWORD(changes));
         margins->top_left.h = (HIWORD(changes));
      }

      if ((changes = SendNotification(pg_stuff, PGN_YPAGEMODIFY, 0, (void PG_FAR *)page_number)) != 0) {

         margins->bot_right.v = (LOWORD(changes));
         margins->top_left.v = (HIWORD(changes));
      }
      
      UnuseMemory(pg->custom_control);
   }
}


/* draw_rectangle frames a PAIGE rectangle struct, offset by "offset"
(if non-NULL) and scaled by scale_factor, inset by "inset." */

static void draw_rectangle (HDC hdc, rectangle_ptr rect, co_ordinate_ptr offset,
      pg_scale_ptr scale_factor, short inset, short pen_size)
{
   rectangle         target;
   RECT           r;

#ifdef MAC_PLATFORM
#pragma unused (hdc)
#endif

   target = *rect;

   if (offset)
      pgOffsetRect(&target, offset->h, offset->v);
   if (scale_factor)
      pgScaleRect(scale_factor, offset, &target);
   
   pgInsetRect(&target, inset, inset);
   RectangleToRect(&target, NULL, &r);
   r.right -= pen_size;
   r.bottom -= pen_size;
   
   MovePenTo(hdc, r.left, r.top);
   PenLineTo(hdc, r.right, r.top);
   PenLineTo(hdc, r.right, r.bottom);
   PenLineTo(hdc, r.left, r.bottom);
   PenLineTo(hdc, r.left, r.top);
}


/* bits_modify_proc allows background drawing. */

STATIC_PASCAL (void) bits_modify_proc (paige_rec_ptr pg, graf_device_ptr bits_port,
      pg_boolean post_call, rectangle_ptr bits_rect, co_ordinate_ptr screen_offset,
      long text_offset)
{
   PAIGERECTSTRUCT            target;
   paige_control_ptr       pg_stuff;

#ifdef MAC_PLATFORM
#pragma unused (text_offset)
#endif

   if (post_call)
      return;

   if ((pg->notification & PGN_BACKDRAWFLAG) && pg->custom_control) {
       
       pg_stuff = UseMemory(pg->custom_control);
      target.hDC = (HDC)bits_port->machine_ref;
      pgBlockMove(bits_rect, &target.top, sizeof(rectangle));
      target.screen_x = screen_offset->h;
      target.screen_y = screen_offset->v;
      pgSubPt(&pg->scroll_pos, (co_ordinate_ptr)&target.screen_y);
      
      target.query = 0;
      SendNotification(pg_stuff, PGN_BACKDRAW, 0, &target);
      UnuseMemory(pg->custom_control);
   }
}


/* wait_idle_proc gets called by PAIGE when doing something that takes
a while. */

STATIC_PASCAL (void) wait_idle_proc (paige_rec_ptr pg, short wait_verb, long progress_ctr,
      long completion_ctr)
{
   paige_control_ptr       pg_stuff;
   PAIGEWAITSTRUCT            wait_info;
   
   if ((pg->notification & PGN_WAITFLAG) && pg->custom_control) {
       
       pg_stuff = UseMemory(pg->custom_control);
       wait_info.editWindow = (HWND)pg_stuff->myself;
       wait_info.waitVerb = wait_verb;
       wait_info.progress = progress_ctr;
       wait_info.completion = completion_ctr;
      SendNotification(pg_stuff, PGN_WAIT, 0, &wait_info);
      UnuseMemory(pg->custom_control);
   }
}

/* line_draw_proc gets called by PAIGE, allowing us to make "line draw" notification. */

STATIC_PASCAL (void) line_draw_proc (paige_rec_ptr pg, style_walk_ptr walker,
      long line_number, long par_number, text_block_ptr block, point_start_ptr first_line,
      point_start_ptr last_line, point_start_ptr previous_first, point_start_ptr previous_last,
      co_ordinate_ptr offset_extra, rectangle_ptr vis_rect, short call_verb)
{
   paige_control_ptr       pg_stuff;
   rectangle               union_r;
   PAIGELINESTRUCT            line_info;

#ifdef MAC_PLATFORM
#pragma unused (walker, vis_rect)
#endif
   
   pgLineGlitterProc(pg, walker, line_number, par_number, block, first_line,
            last_line, previous_first, previous_last, offset_extra,
            vis_rect, call_verb);

   if (call_verb == glitter_post_bitmap_draw || call_verb == glitter_pre_draw)
      return;

   if ((pg->notification & PGN_LINEDRAWFLAG) && pg->custom_control) {

      pgFillBlock(&line_info, sizeof(PAIGELINESTRUCT), 0);
      
      line_info.hDC = (HDC)pg->globals->current_port->machine_ref;
      
      pgUnionRect(&first_line->bounds, &last_line->bounds, &union_r);
      RectangleToRect(&union_r, offset_extra, &line_info.currentBox);
      
      if (previous_first) {
      
         pgUnionRect(&previous_first->bounds, &previous_last->bounds, &union_r);
         RectangleToRect(&union_r, offset_extra, &line_info.previousBox);
      }

      line_info.lineNum = line_number;
      line_info.parNum = par_number;
      line_info.currentPos = (long)first_line->offset;
      line_info.endingPos = (long)last_line[1].offset;
      line_info.currentPos += block->begin;
      line_info.endingPos += block->begin;

      line_info.flags = first_line->flags & NEW_PAR_BIT;
      line_info.flags |= (last_line->flags & (PAR_BREAK_BIT | BREAK_PAGE_BIT | SOFT_BREAK_BIT));
      
      if (!previous_first)
         line_info.flags |= PGLINE_NOPREVIOUS;

      if (line_info.endingPos == pg->t_length)
         line_info.flags |= PGLINE_ENDDOC;
      
       pg_stuff = UseMemory(pg->custom_control);

      SendNotification(pg_stuff, PGN_LINEDRAW, 0, &line_info);
      UnuseMemory(pg->custom_control);
   }
}


// set_dc_proc is the standard "set device" hook to notify the app.

STATIC_PASCAL (void) set_dc_proc (paige_rec_ptr pg, short verb, graf_device_ptr device,
      color_value_ptr bk_color)
{
   paige_control_ptr       pg_stuff;
   HDC                     hdc, PG_FAR *hdc_ptr;

   if ((pg->notification & PGN_SETDCFLAG) && pg->custom_control) {

       pg_stuff = UseMemory(pg->custom_control);

      if (verb == set_pg_device) {

         pgSetGrafDevice(pg, verb, device, bk_color);
         hdc = (HDC)device->machine_ref;
         hdc_ptr = &hdc;
         SendNotification(pg_stuff, PGN_SETDC, 0, (void PG_FAR *)hdc_ptr);
         device->machine_ref = PG_LONGWORD(long)hdc;
      }
      else {

         hdc = (HDC)device->machine_ref;
         hdc_ptr = &hdc;
         SendNotification(pg_stuff, PGN_RELEASEDC, 0, (void PG_FAR *)hdc_ptr);
         device->machine_ref = PG_LONGWORD(long)hdc;
         pgSetGrafDevice(pg, verb, device, bk_color);
      }

      UnuseMemory(pg->custom_control);
   }
   else
      pgSetGrafDevice(pg, verb, device, bk_color);
}


/* auto_scroll gets called to automatically scroll as mouse is dragged. */

STATIC_PASCAL (void) auto_scroll (paige_rec_ptr pg, short h_verb, short v_verb,
      co_ordinate_ptr mouse_point, short draw_mode)
{
   paige_control_ptr       pg_stuff;
   PAIGESCROLLSTRUCT       scroll_info;
   
   if ((pg->notification & PGN_SCROLLFLAG) && pg->custom_control) {

       pg_stuff = UseMemory(pg->custom_control);

      scroll_info.hDC = (HDC)pg->port.machine_ref;
      scroll_info.point_y = mouse_point->v;
      scroll_info.point_x = mouse_point->h;
      scroll_info.verb_y = v_verb;
      scroll_info.verb_x = h_verb;
      SendNotification(pg_stuff, PGN_SCROLL, 0, &scroll_info);
      
      mouse_point->h = scroll_info.point_x;
      mouse_point->v = scroll_info.point_y;

      UnuseMemory(pg->custom_control);
   }
   else
      pgAutoScrollProc(pg, h_verb, v_verb, mouse_point, draw_mode);
}


/* control_read_handler initializes the extra data saved about the
control document being read. */

STATIC_PASCAL (pg_boolean) control_read_handler (paige_rec_ptr pg, pg_file_key key, memory_ref key_data,
      long PG_FAR *element_info, void PG_FAR *aux_data, long PG_FAR *unpacked_size)
{
   pack_walk         walker;
   paige_control_ptr pg_stuff;

#ifdef MAC_PLATFORM
#pragma unused (key, element_info, aux_data, unpacked_size)
#endif

   if (!pg->custom_control || !key_data)
      return   TRUE;
    
    pgSetupPacker(&walker, key_data, 0);
   pg_stuff = UseMemory(pg->custom_control);
   
   pgUnpackRect(&walker, &pg_stuff->page_rect);
   pgUnpackNumbers(&walker, &pg_stuff->left_margin, 4, short_data);
   
   pg_stuff->columns = (short)pgUnpackNum(&walker);
   pg_stuff->column_gap = (short)pgUnpackNum(&walker);
   pg_stuff->ex_styles = pgUnpackNum(&walker);
   pg_stuff->max_input = pgUnpackNum(&walker);
   pg_stuff->extra_scroll_h = (short)pgUnpackNum(&walker);
   pg_stuff->extra_scroll_v = (short)pgUnpackNum(&walker);
   
#ifdef WINDOWS_PLATFORM
   if (pg_stuff->frame_pen)
      DeleteObject((HANDLE)pg_stuff->frame_pen);
   if (pg_stuff->page_pen)
      DeleteObject((HANDLE)pg_stuff->page_pen);
   if (pg_stuff->page_break_pen)
      DeleteObject((HANDLE)pg_stuff->page_break_pen);
#endif
   pg_stuff->frame_pen = unpack_pen(&walker, &pg_stuff->frame_pen_width);
   pg_stuff->page_pen = unpack_pen(&walker, &pg_stuff->page_pen_width);
   pg_stuff->page_break_pen = unpack_pen(&walker, &pg_stuff->break_pen_width);

    UnuseMemory(pg->custom_control);
    UnuseMemory(key_data);
    
    return  TRUE;
}


/* control_write_handler writes the extra document info when a file is
saved. */

STATIC_PASCAL (pg_boolean) control_write_handler (paige_rec_ptr pg, pg_file_key key, memory_ref key_data,
      long PG_FAR *element_info, void PG_FAR *aux_data, long PG_FAR *unpacked_size)
{
   pack_walk         walker;
   paige_control_ptr pg_stuff;

#ifdef MAC_PLATFORM

#pragma unused (key, element_info, aux_data, unpacked_size)

#endif

   if (!pg->custom_control) {
      
      SetMemorySize(key_data, 0);
      return   TRUE;
   }

   pgFillBlock(&walker, sizeof(pack_walk), 0);
   walker.data_ref = key_data;
   walker.remaining_ctr = GetMemorySize(key_data);
   walker.data = UseMemory(key_data);
   pg_stuff = UseMemory(pg->custom_control);
   
   pgPackRect(&walker, &pg_stuff->page_rect);
   pgPackNumbers(&walker, &pg_stuff->left_margin, 4, short_data);
   pgPackNum(&walker, short_data, pg_stuff->columns);
   pgPackNum(&walker, short_data, pg_stuff->column_gap);
   pgPackNum(&walker, long_data, pg_stuff->ex_styles);
   pgPackNum(&walker, long_data, pg_stuff->max_input);
   pgPackNum(&walker, long_data, pg_stuff->extra_scroll_h);
   pgPackNum(&walker, long_data, pg_stuff->extra_scroll_v);
   
   pack_pen(&walker, (HPEN)pg_stuff->frame_pen);
   pack_pen(&walker, (HPEN)pg_stuff->page_pen);
    pack_pen(&walker, (HPEN)pg_stuff->page_break_pen);

    UnuseMemory(pg->custom_control);
    
    pgFinishPack(&walker);
    
    SetMemorySize(key_data, walker.transfered);
    
    return  TRUE;
}


/* pack_pen saves all the pen info, if any, to the file. If pen is NULL
then at least one zero is sent (indicating pen size). */

static void pack_pen (pack_walk_ptr walker, HPEN pen)
{
#ifdef MAC_PLATFORM
   pgPackNum(walker, long_data, pen);
#endif
#ifdef WINDOWS_PLATFORM
   LOGPEN         pen_info;
   
   if (pen) {
      
      GetObject(pen, sizeof(LOGPEN), &pen_info);
      pgPackNum(walker, short_data, pen_info.lopnWidth.x);
      
      if (pen_info.lopnWidth.x) {
         
         pgPackNum(walker, short_data, pen_info.lopnStyle);
         pgPackNum(walker, short_data, pen_info.lopnColor);
      }
   }
   else
      pgPackNum(walker, short_data, 0);
#endif
}


/* unpack_pen recovers the pen, if any, that was saved to the file.
If no pen, the function returns NULL. The pen_size gets set to zero
if no pen, or the pen size. */

static HPEN unpack_pen (pack_walk_ptr walker, short PG_FAR *pen_size)
{
#ifdef MAC_PLATFORM
#pragma unused (pen_size)

   return   pgUnpackNum(walker);

#endif
#ifdef WINDOWS_PLATFORM
   LOGPEN         pen;
   HPEN        result;
   
   if (pen.lopnWidth.x = (short)pgUnpackNum(walker)) {
      
      pen.lopnStyle = (short)pgUnpackNum(walker);
      pen.lopnColor = pgUnpackNum(walker);
      result = CreatePenIndirect(&pen);
   }
   else
      result = (HPEN)NULL;
   
   *pen_size = (short)pen.lopnWidth.x;
   
   return   result;
#endif
}


/* handle_button_up gets called to inform PAIGE that the button is now up. */

static void handle_button_up (paige_control_ptr pg_stuff, long lParam)
{
   co_ordinate       pg_mouse;
   
   if (pg_stuff->mouse_loc) {
   
      SplitIntoLongs(lParam, &pg_mouse.v, &pg_mouse.h);
      pgDragSelect(pg_stuff->pg, &pg_mouse, mouse_up, pg_stuff->modifiers, 0, FALSE);
      pg_stuff->modifiers = 0;
      pg_stuff->mouse_loc = 0;
      pg_stuff->embed_click = MEM_NULL;

#ifdef WINDOWS_PLATFORM
      ReleaseCapture();
#endif
   }
}

// non_insertable_char returns TRUE if the_char will not be inserted

static int non_insertable_char (pg_char the_char)
{
   if (the_char < ' ')
      return   (the_char == paige_globals.left_arrow_char
               || the_char == paige_globals.right_arrow_char
               || the_char == paige_globals.up_arrow_char
               || the_char == paige_globals.down_arrow_char);
   
   return   FALSE;
}

