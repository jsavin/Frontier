/* This file contains all the machine-specific code that is not memory-related
(memory-related code can be found in "machinememory.h").  To convert to a
new platform, change the code in this file but do NOT change the API or data
structures unless there is specific comment that you may do so, or it is probably
OK to change all the static/local functions as you see fit.

MS WINDOWS NOTES 4/24/94:

--  The graf_device->machine_var will contain the HWND (main window
for default device, child window for pg_ref-specific). In globals->machine_const
is the original/main window from the app at startup. When PAIGE asks that a
graf_device is to be "used" a GetDC is performed and the GetDC result is placed
in graf_device->machine_ref. When not being used, machine_ref is NULL.

--  The font_info records contains information about the font text face. The
style_info record inself holds the actual font object (created with
CreateFontInd). When the style is "set" the font object is selected. It is
done this way so I create a font object only once for each unique style.

-- The font_info->machine_var data contains output precision, clip precision
and quality info (the element indexes defined in pgTraps.h).

Copyright 1993 by DataPak Software, Inc.  All rights reserved. It is illegal
to remove this notice   */

#include "Paige.h"
#include "pgRegion.h"

#ifdef WINDOWS_PLATFORM

#include <windows.h>
#include <windowsx.h>

#ifdef WIN16_COMPILE
#include "w16nls.h"
//include "olenls.h"

/*
 * fake these out
 */
static void GetCPInfo(short, CPINFO *);
static void GetStringTypeEx(LCID,DWORD,LPCSTR,int,LPWORD);
#endif

#ifdef UNICODE
// #define x_GetFontLanguageInfo GetFontLanguageInfo
#define FirstMultiCharByte(unused1, unused2) FALSE
#ifndef LANG_ARABIC
#define LANG_ARABIC                      0x01
#endif
#ifndef LANG_HEBREW
#define LANG_HEBREW                      0x0d
#endif
#else

#ifdef WIN_MULTILINGUAL
#ifdef WIN16_COMPILE 
DWORD WINAPI x_GetFontLanguageInfo( HDC );
#define FirstMultiCharByte(unused, the_char) IsDBCSLeadByte(the_char)
#else
// PDA:  These lines were added for an apparent bug in VC++ 4.2!!!
#ifndef LANG_ARABIC
#define LANG_ARABIC                      0x01
#endif
#ifndef LANG_HEBREW
#define LANG_HEBREW                      0x0d
#endif
// QUALCOMM hack.  Must dynamically decide for NT and 95 which function to call.  Declare it.
//#define x_GetFontLanguageInfo GetFontLanguageInfo
DWORD WINAPI x_GetFontLanguageInfo( HDC );
#define FirstMultiCharByte(code_page, the_char) IsDBCSLeadByteEx(code_page, the_char)
#endif
#else
#define FirstMultiCharByte(unused1, unused2) FALSE
#endif
#endif

#include "pgOSUtl.h"
#include "pgUtils.h"
#include "machine.h"
#include "defprocs.h"
#include "pgDefStl.h"
#include "pgText.h"
#include "pgTxtWid.h"
#include "pgSelect.h"
#include "pgShapes.h"
#include "pgFiles.h"
#include "pgErrors.h"
#include "pgEdit.h"

#include "pgScript.h"
#include "pgUtils.h"
#include "pgExceps.h"
#include "pgGrafx.h"
#include "pgLists.h"
#include "pgSubRef.h"

#include <string.h>

#define WIN_CARET_TIME  1
#define FONT_EXISTS 0x0010

#ifdef UNICODE
#define DISPLAY_DRIVER_NAME L"DISPLAY"
#else
#define DISPLAY_DRIVER_NAME "DISPLAY"
#endif

static void init_global_char (pg_char_ptr global_char, pg_short_t value);
static pg_short_t global_char_to_int (pg_char_ptr global_char);
static short global_char_size (pg_char_ptr global_char);
static void make_graf_device (pg_globals_ptr globals, generic_var port, generic_var machine_ref,
      graf_device_ptr device);
static void draw_cursor (paige_rec_ptr pg, t_select_ptr select, short verb);
static void install_machine_font (paige_rec_ptr pg, style_info_ptr style,
      font_info_ptr font, pg_boolean include_offscreen);
static void draw_special_char (paige_rec_ptr pg, pg_char_ptr the_char, co_ordinate_ptr point);
static pg_boolean create_caret (paige_rec_ptr pg, short h_loc, short v_loc, short caret_size);
static void init_def_font_name (paige_rec_ptr pg, font_info_ptr font);
static void set_alternate_pointsize(paige_rec_ptr pg, long new_resolution,
      style_walk_ptr walker);
static short set_invisible_symbol_style (paige_rec_ptr pg);
static void draw_box_style(paige_rec_ptr pg, style_walk_ptr walker,
      draw_points_ptr draw_position);
static void draw_box_blink (paige_rec_ptr pg, style_walk_ptr walker,
      draw_points_ptr draw_position);
static void scale_dc (HDC dc, graf_device_ptr the_device);
static HDC get_unscaled_dc (paige_rec_ptr pg, short PG_FAR *index);
static void restore_scaled_dc (paige_rec_ptr pg, short index);

/* Windows-specific functions: */
int CALLBACK enum_pgfont_proc (LOGFONT far *log, TEXTMETRIC far *tm, int font_type, LPARAM pg_font);
static void get_windows_font_object (paige_rec_ptr pg, style_info_ptr the_style, font_info_ptr font);
static pg_error MemError (void);
static pg_fixed windows_pointsize (pg_fixed ptsize, int resolution);
static pg_fixed windows_screensize (pg_fixed screensize, int resolution);
static HBRUSH build_background_brush (graf_device_ptr device);
static int get_space_extra (HDC hdc, pg_char_ptr text, short text_length,
      pg_char space_char);
static void set_printer_scaling (paige_rec_ptr pg, HDC printer_dc, short use_x, short use_y);
static void unset_printer_scaling (HDC printer_dc);
static short PG_FAR * x_drawing_positions (paige_rec_ptr pg, style_walk_ptr walker,
         pg_char_ptr text, short draw_length, long PG_FAR *end_pen);
static pg_boolean last_char_of_script (style_walk_ptr walker, long offset);

static long set_non_roman_bits (paige_rec_ptr pg, style_walk_ptr walker, pg_char_ptr data,
      long offset_begin, long offset_end, long char_offset, long mask_bits,
      WORD PG_FAR *ctype_3, pg_short_t PG_FAR *symbol_test, pg_boolean known_leadbyte);
static void init_font_record (paige_rec_ptr pg, font_info_ptr font);
// QUALCOM needs globals
static void init_font_language (pg_globals_ptr globals, font_info_ptr font, HDC hdc);
static long multibyte_character_flags (UINT code_page, pg_char_ptr string, long offset_begin,
         long offset_end, pg_boolean known_leadbyte);

static pg_boolean blink = FALSE;
static short      blink_mode = 0;

/* pgMachineInit must initialize all the "global" variables in pg_globals.
This is included here because certain items will be created that are specific
to the platform in question.
WINDOWS NOTE: For Windows, globals->machine_const is used to create a default DC
for whole screen which we are forced to use if app doesn't provide a window
at a critical time.  */

PG_PASCAL (void) pgMachineInit (pg_globals_ptr globals)
{
   register pg_globals_ptr  global_vars;
   long               def_res;
   pg_char               screen_driver[10];
   
   global_vars = globals;       /* Makes for less code */

   global_vars->max_offscreen = MAX_OFFSCREEN;
   global_vars->max_block_size = MAX_TEXTBLOCK;
   global_vars->def_tab_space = DEF_TAB_SPACE;
   global_vars->minimum_line_width = DEF_MIN_WIDTH;
   global_vars->line_wrap_char = CR_CHAR;
   global_vars->soft_line_char = SOFT_CR_CHAR;
   global_vars->tab_char = TAB_CHAR;
   global_vars->soft_hyphen_char = SOFT_HYPHEN;
   global_vars->bs_char = DELETE_CHAR;
   global_vars->ff_char = FF_CHAR;
   global_vars->container_brk_char = CONTAINER_BRK_CHAR;
   global_vars->left_arrow_char = LEFT_ARROW;
   global_vars->right_arrow_char = RIGHT_ARROW;
   global_vars->up_arrow_char = UP_ARROW;
   global_vars->down_arrow_char = DOWN_ARROW;
   global_vars->text_brk_char = TEXT_BRK_CHAR;
   global_vars->fwd_delete_char = FWD_DELETE_CHAR;

#ifdef WIN32_COMPILE
   {
	   OSVERSIONINFO ver;
	   ver.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	   GetVersionEx(&ver);
	   // QUALCOMM: RUNTIME_PLATFORM_WIN32S is true for Win95.
	   if((ver.dwPlatformId == VER_PLATFORM_WIN32s) ||
		   (ver.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS)	)
		   global_vars->machine_specific = RUNTIME_PLATFORM_WIN32S;
	   else
		   global_vars->machine_specific = RUNTIME_PLATFORM_WIN32;
   }
#else /* WIN32_COMPILE */
   global_vars->machine_specific = RUNTIME_PLATFORM_WIN16;
#endif /* WIN32_COMPILE */

   init_global_char(global_vars->hyphen_char, HYPHEN_CHAR);
   init_global_char(global_vars->decimal_char, DECIMAL_CHAR);
   init_global_char(global_vars->cr_invis_symbol, INVIS_CR);
   init_global_char(global_vars->lf_invis_symbol, INVIS_LF);
   init_global_char(global_vars->tab_invis_symbol, INVIS_TAB);
   init_global_char(global_vars->end_invis_symbol, INVIS_END);
   init_global_char(global_vars->cont_invis_symbol, INVIS_BREAK);
   init_global_char(global_vars->pbrk_invis_symbol, INVIS_PBREAK);
   init_global_char(global_vars->space_invis_symbol, INVIS_SPACE);
   init_global_char(global_vars->elipse_symbol, ELIPSE_SYMBOL);
   
   init_global_char(global_vars->flat_single_quote, APOSTROPHE);
   init_global_char(global_vars->flat_double_quote, FLAT_QUOTE_CHAR);
   init_global_char(global_vars->left_single_quote, LEFT_SINGLE_QUOTE);
   init_global_char(global_vars->right_single_quote, RIGHT_SINGLE_QUOTE);
   init_global_char(global_vars->left_double_quote, LEFT_DOUBLE_QUOTE);
   init_global_char(global_vars->right_double_quote, RIGHT_DOUBLE_QUOTE);
   init_global_char(global_vars->unknown_char, UNSUPPORTED_CHAR);
   init_global_char(global_vars->bullet_char, BULLET_CHAR);

   pgFillBlock(screen_driver, sizeof(pg_char) * 10, 0);
   pgBlockMove(DISPLAY_DRIVER_NAME, screen_driver, sizeof(pg_char) * 7);
   global_vars->machine_const = PG_LONGWORD(generic_var)CreateDC((PGSTR)screen_driver, NULL, NULL, NULL);

/* Init the default font, style & para records */

   pgInitDefaultFont(global_vars, &global_vars->def_font);
   pgInitDefaultStyle(global_vars, &global_vars->def_style, &global_vars->def_font);
   pgInitDefaultPar(global_vars, &global_vars->def_par);

   global_vars->def_bk_color.red = global_vars->def_bk_color.green
         = global_vars->def_bk_color.blue = 0xFFFF;
   global_vars->trans_color = global_vars->def_bk_color;
   
   def_res = (long)GetDeviceCaps((HDC)global_vars->machine_const, LOGPIXELSX);
   global_vars->bullet_size = MulDiv((int)5, (int)def_res, 72);

   PG_TRY(global_vars->mem_globals) {

   #ifdef WIN16_COMPILE
      global_vars->system_version = GetWinFlags();
   #else
      global_vars->system_version = (long)GetVersion();
   #endif

      global_vars->color_enable = TRUE;          
      global_vars->font_objects = MemoryAlloc(global_vars->mem_globals, sizeof(font_object_rec), 0, 4);
      global_vars->alpha_widths = MemoryAlloc(global_vars->mem_globals, sizeof(pg_text_int), 0, 128);

/* (no offscreen "buffer" required but initializing the graf_device is necessary) */

      make_graf_device(global_vars, MEM_NULL, MEM_NULL, &globals->offscreen_port);
#ifdef PG_OITC
      global_vars->pg_list = MemoryAlloc(global_vars->mem_globals, sizeof(pg_list_rec), 0, 5);
#endif
   }
   
   PG_CATCH {
      
      pgMachineShutdown(global_vars);
      pgFailure(global_vars->mem_globals, NO_MEMORY_ERR, 0);
   }
   
   PG_ENDTRY;

   global_vars->offscreen_enable = FALSE;
   global_vars->offscreen_exclusion = pgRectToShape(global_vars->mem_globals, NULL);
}


/* pgMachineShutdown must de-allocate anything it created during pgMachineInit */

PG_PASCAL (void) pgMachineShutdown (const pg_globals_ptr globals)
{
   pg_globals_ptr      global_vars;

   global_vars = globals;       /* Makes for less code */

   if (global_vars->machine_const)
      DeleteDC((HDC)global_vars->machine_const);

   if (global_vars->font_objects) {
      font_object_ptr       font_objects;
      size_t                num_fonts;
      
      font_objects = (font_object_ptr) UseMemory(global_vars->font_objects);
      num_fonts = GetMemorySize(global_vars->font_objects);
      
      while (num_fonts) {
         
         DeleteObject((HANDLE)font_objects->the_font);
         
         ++font_objects;
         --num_fonts;
      }
      
      UnuseAndDispose(global_vars->font_objects);
   }
   
   pgCloseDevice(global_vars, &global_vars->offscreen_port);
   DisposeNonNilMemory(global_vars->offscreen_exclusion);
   DisposeNonNilMemory(global_vars->alpha_widths);
   DisposeNonNilMemory(global_vars->pg_list);
}



/* pgInitDefaultDevice sets up a device to use as a default. In Mac, for instance,
this would be simply the current port */

PG_PASCAL (void) pgInitDefaultDevice (const pg_globals_ptr globals, graf_device_ptr device)
{
   HWND         focus_window;
   
   if (focus_window = GetFocus())
      pgInitDevice(globals, PG_LONGWORD(generic_var) focus_window, 0, device);
}


/* pgInitDevice sets up a Paige graf_device. Although the code in this file
is machine-dependent, we can hopefully use this same API for all platforms. The
idea is to pass <whatever is appropriate> in the_port and/or machine_var and
let this machine-specific function initialize a generic Paige graf_device.
FOR MACINTOSH: the_port is a GrafPtr. */

PG_PASCAL (void) pgInitDevice (const pg_globals_ptr globals, const generic_var the_port,
      size_t machine_ref, graf_device_ptr device)
{
   make_graf_device(globals, the_port, machine_ref, device);
}


/* pgCloseDevice disposes everything created for pgInitDevice.  */

PG_PASCAL (void) pgCloseDevice (const pg_globals_ptr globals, const graf_device_ptr device)
{
   if (device->graf_stack)
      DisposeMemory(device->graf_stack);

   if (device->clip_rgn)
      pgDisposeRgn(device->clip_rgn);

   if (device->scroll_rgn)
      pgDisposeRgn(device->scroll_rgn);
}



/* pgInsertQuery is a version 1.3 function, designed mostly for multilingual input. When PAIGE
is asked to insert a key with key_insert_mode, this hook is called. This hook must return
the insertion mode to use. For example, if another key is required and the current char should
be buffered then key_buffer_mode can be returned. */

PG_PASCAL (short) pgInsertQuery (paige_rec_ptr pg, pg_char_ptr the_char, short charsize)
{
   MSG            msg;
   HWND        win;
   
   win = (HWND)pg->port.machine_var;

   if (win) {
   
      if (!PeekMessage(&msg, win, WM_CHAR, WM_CHAR, PM_NOREMOVE | PM_NOYIELD))
         return   key_insert_mode;
      if (PeekMessage(&msg, win, WM_KEYDOWN, WM_KEYDOWN, PM_NOREMOVE | PM_NOYIELD))
         return   key_insert_mode;
      
      return   key_buffer_mode;
   }

   return   (short)key_insert_mode;
}


/* pgCharClassProc gets called when parsing chars to determine the subset type. Usually this
refers to multilingual double byte chars so highlighting will group all similar types. */

PG_PASCAL (pg_word) pgCharClassProc (paige_rec_ptr pg, pg_char_ptr the_char, short charsize,
      style_info_ptr style, font_info_ptr font)
{
#ifdef WIN_MULTILINGUAL
   WORD        types[4];
   
   pgFillBlock(types, sizeof(WORD) * 4, 0);
   GetStringTypeEx((LCID)font->language, CT_CTYPE3, (PGSTR)the_char, (int)charsize, types);
   types[0] &= (~(C3_HALFWIDTH | C3_FULLWIDTH));

   if (charsize == 1)
      types[0] |= C3_HALFWIDTH;
   else
      types[0] |= C3_FULLWIDTH;

   return   (WORD)(types[0] & (C3_KATAKANA | C3_HIRAGANA | C3_IDEOGRAPH | C3_HALFWIDTH | C3_FULLWIDTH));
#endif
   return   0;
}

/* DEFAULT HIGHLIGHT DRAW FUNCTION. This is the internal hook used to display
highlight. It is obviously machine-dependent. */

PG_PASCAL (void) pgDrawHiliteProc (paige_rec_ptr pg, shape_ref rgn)
{
   Rect                     piece_of_shape;
   rectangle                vis_bounds, scaled_r, container;
   co_ordinate              scroll_adjust, repeat_offset;
   register rectangle_ptr   r_ptr;
   register size_t          r_qty;
   long                     container_proc_refcon;
   pg_short_t               last_container, real_container;
   HDC                      hdc = pgGetPlatformDevice(&pg->port);
   
   SetMapMode(hdc, MM_TEXT);
   r_ptr = (rectangle_ptr) UseMemory(rgn);
    
    pg->scale_factor.scale = pg->port.scale.scale;
    
   scroll_adjust = pg->scroll_pos;
   pgNegatePt(&scroll_adjust);
   pgAddPt(&pg->port.origin, &scroll_adjust);

   pgShapeBounds(pg->vis_area, &vis_bounds);
   //pgOffsetRect(&vis_bounds, pg->port.origin.h, pg->port.origin.v);
   
   if (pg->flags & SCALE_VIS_BIT)
      pgScaleRect(&pg->port.scale, NULL, &vis_bounds);

// pgScaleLong(-pg->port.scale.scale, vis_bounds.top_left.h, &vis_bounds.bot_right.h); 
//    pgScaleLong(-pg->port.scale.scale, vis_bounds.top_left.v, &vis_bounds.bot_right.v); 
   
   pgInsetRect(&vis_bounds, -8, -8);

   container_proc_refcon = 0;
   
   real_container = pgGetSectWrapRect(pg, &r_ptr[1], &last_container, &repeat_offset);
   
   GetMemoryRecord(pg->wrap_area, real_container + 1, &container);
   pgOffsetRect(&container, repeat_offset.h, repeat_offset.v);

   pgCallContainerProc(pg, last_container, &scroll_adjust,
      clip_container_verb,  &pg->port.scale, &container_proc_refcon); 
   pgCallContainerProc(pg, last_container, &scroll_adjust,
      will_draw_verb,  &pg->port.scale, NULL);

   for (r_qty = GetMemorySize(rgn) - 1; r_qty; --r_qty) {
      
      ++r_ptr;
      
      if (!pgSectRect(r_ptr, &container, NULL)) {
         pg_short_t           new_container;
         
         real_container = pgGetSectWrapRect(pg, r_ptr, &new_container, &repeat_offset);
         
         if (new_container != last_container) {
            
            last_container = new_container;
            GetMemoryRecord(pg->wrap_area, real_container + 1, &container);
            pgOffsetRect(&container, repeat_offset.h, repeat_offset.v);
            
            pgCallContainerProc(pg, last_container, &scroll_adjust,
               clip_container_verb,  &pg->port.scale, &container_proc_refcon); 
            pgCallContainerProc(pg, last_container, &scroll_adjust,
               will_draw_verb,  &pg->port.scale, NULL);
         }
      }

      pgScaleRectToRect(&pg->scale_factor, r_ptr, &scaled_r, &scroll_adjust);

      if ((scaled_r.top_left.v < vis_bounds.bot_right.v)
            && (scaled_r.bot_right.v > vis_bounds.top_left.v)) {
         
         RectangleToRect(&scaled_r, NULL, &piece_of_shape);
         InvertRect(hdc, &piece_of_shape);
      }
   }
   
   pg->scale_factor.scale = 0;
   UnuseMemory(rgn);
   
   if (pg->port.scale.scale)
      scale_dc(hdc, &pg->port);

   pgCallContainerProc(pg, last_container, NULL, unclip_container_verb,  NULL, &container_proc_refcon);
   pgReleasePlatformDevice(&pg->port);
}



/* DEFAULT HIGHLIGHT DRAW THE CUSOR ("caret"). It is obviously machine-dependent.
NOTE: The correct port is set. */

PG_PASCAL (void) pgDrawCursorProc (paige_rec_ptr pg, t_select_ptr select,
      short verb)
{
   if ((pg->flags & NO_EDIT_BIT) || (pg->flags2 & NO_CARET_BIT) || !pg->port.machine_var)
      return;

   switch (verb) {

      case toggle_cursor:    
      case toggle_cursor_idle:
         if (!(pg->flags & CARET_CREATED_BIT))
            draw_cursor(pg, select, verb);
         break;

      case hide_cursor:
      case deactivate_cursor:
         if (pg->flags & CARET_BIT) {
         
            draw_cursor(pg, select, verb);
            pg->flags &= (~CARET_BIT);
         }
         break;

      case activate_cursor:
         draw_cursor(pg, select, verb);
         break;

      case show_cursor:
         draw_cursor(pg, select, verb);
         pg->flags |= CARET_BIT;
         break;

      case update_cursor:
         if ((pg->flags & INVALID_CURSOR_BIT) && (pg->flags & CARET_CREATED_BIT)) {
            
            DestroyCaret();
            pg->flags &= (long)(~(CARET_BIT | CARET_CREATED_BIT));
            draw_cursor(pg, select, update_cursor);
         }
         else
         if (pg->flags & CARET_BIT)
            draw_cursor(pg, select, verb);
         break;
      
      case restore_cursor:
         if ((pg->flags & INVALID_CURSOR_BIT) && (pg->flags & CARET_CREATED_BIT)) {

            DestroyCaret();
            pg->flags &= (long)(~(CARET_BIT | CARET_CREATED_BIT));
         }

         if (!(pg->flags & CARET_BIT))
            draw_cursor(pg, select, verb);
         
         break;

      case compute_cursor:
         draw_cursor(pg, select, verb);
         break;
   }       
}



/* DEFAULT FUNCTION TO INITIALIZE A STYLE RECORD. It can be somewhat machine-
dependent. */

PG_PASCAL (void) pgStyleInitProc (paige_rec_ptr pg, style_info_ptr style,
         font_info_ptr font)
{
   register       style_info_ptr  the_style;
   HDC                  hdc;
   TEXTMETRIC          tm;
   short          underline_extra, save_index, super_or_sub_extra;
   
   the_style = style;
   underline_extra = 0;

   hdc = get_unscaled_dc(pg, &save_index);   
   get_windows_font_object(pg, the_style, font);
   the_style->procs.install(pg, the_style, font, NULL, 0, FALSE);
   GetTextMetrics(hdc, &tm);
 
   the_style->ascent       = (short)tm.tmAscent;
   the_style->descent      = (short)tm.tmDescent;
   
   if (!(pg->flags & NO_DEFAULT_LEADING))
      the_style->leading      = (short)tm.tmExternalLeading; //+ tm.tmExternalLeading;
   else
      the_style->leading = 0;

   the_style->right_overhang = (long) tm.tmOverhang;

   restore_scaled_dc(pg, save_index);

   if (super_or_sub_extra = the_style->styles[superscript_var]) {
      
      if (the_style->shift_verb == percent_of_style)
         super_or_sub_extra = ((the_style->ascent + the_style->descent) * super_or_sub_extra) / 100;

      the_style->ascent += super_or_sub_extra;
   }

   if (font->char_type & (RIGHT_FONT_FLAG | MULTIBYTE_FLAG))
      the_style->class_bits |= NO_SMART_DRAW_BIT;
   else
      the_style->class_bits &= (~NO_SMART_DRAW_BIT);

   if (font->char_type & RIGHT_FONT_FLAG)
      the_style->class_bits |= RIGHTLEFT_BIT;
   else
      the_style->class_bits &= (~RIGHTLEFT_BIT);
   
   if (super_or_sub_extra = the_style->styles[subscript_var]) {
      
      if (the_style->shift_verb == percent_of_style)
         super_or_sub_extra = ((the_style->ascent + the_style->descent) * super_or_sub_extra) / 100;
      
      the_style->descent += super_or_sub_extra;
   }

   if (pg->flags & LINE_EDITOR_BIT)
      underline_extra = 0;
   else
   if (the_style->styles[dbl_underline_var])
      underline_extra += 2;
   
   if (the_style->styles[overline_var])
      the_style->ascent += underline_extra;
   else
      the_style->leading += underline_extra;
}


/* DEFAULT FONT/STYLE INSTALL PROC. Obviously machine-dependent.
Added 9/6/95 "composite_style" which returns superimpose composite if non-NULL. */


PG_PASCAL (void) pgInstallFont (paige_rec_ptr pg, style_info_ptr the_style,
      font_info_ptr the_font, style_info_ptr composite_style, short style_overlay, short include_offscreen)
{
   pg_globals_ptr          globals;
   graf_device_ptr            old_device;
   style_info              imposed_style;
   font_info               imposed_font;
   register style_info_ptr    style;
   register font_info_ptr     font;

   globals = pg->globals;

   style = the_style;
   font = the_font;

   if (style_overlay) {
      
      pgStyleSuperImpose(pg, style, &imposed_style, &imposed_font, style_overlay);
      
      style = &imposed_style;
      font = &imposed_font;
      get_windows_font_object(pg, &imposed_style, &imposed_font);
   }
   else
   if (style->styles[relative_point_var] || style->styles[nested_subset_var]) {
      long        point_size;

      pgBlockMove(style, &imposed_style, sizeof(style_info));
      point_size = pgComputePointSize(pg, &imposed_style);
      point_size <<= 16;
      imposed_style.point = point_size;
      imposed_style.styles[relative_point_var] = imposed_style.styles[nested_subset_var] = 0;
      imposed_style.procs.init(pg, &imposed_style, the_font);
      style = &imposed_style;
   }

    old_device = globals->current_port;
    globals->current_port = (graf_device_ptr) &pg->port;
   install_machine_font(pg, style, font, include_offscreen);
    globals->current_port = old_device;

   if (include_offscreen) {
      if (globals->offscreen_port.machine_ref) {

         old_device = globals->current_port;
         globals->current_port = (graf_device_ptr) &globals->offscreen_port;
         install_machine_font(pg, style, font, FALSE);
         globals->current_port = old_device;
      }
   }
   
   if (composite_style)
      pgBlockMove(style, composite_style, sizeof(style_info));
}


/* DEFAULT DELETE STYLE PROC. For Mac and Windows, this does nothing. WARNING: pg can
be NULL if called from Undo functions! */

PG_PASCAL (void) pgDeleteStyleProc (paige_rec_ptr pg, pg_globals_ptr globals,
      short reason_verb, format_ref all_styles, style_info_ptr style)
{

}


/* DEFAULT DUPLICATE STYLE PROC. For Mac and Windows, this does nothing.
WARNING: pg can be NULL if called from Undo functions!     */

PG_PASCAL (void) pgDupStyleProc (paige_rec_ptr src_pg, paige_rec_ptr target_pg,
      short reason_verb, format_ref all_styles, style_info_ptr style)
{

}


/* pgSaveStyleProc is the default style that gets called just prior to saving
it to a file. For Macintosh this does nothing. For Windows the machine_var field
is zerod out (because it is a handle). */

PG_PASCAL (void) pgSaveStyleProc (paige_rec_ptr pg, style_info_ptr style_to_save)
{

}



/* DEFAULT FUNCTION TO MEASURE TEXT WIDTHS. It is obviously machine-dependent. 
Note: The current font, style, etc. is pre-set before this call. The types
parameter is a pointer to an array of shorts that should be set to character
type bits (as in char_info proc).  HOWEVER, only control char bits should be
set such as CR, TAB, etc.  NOTE: types can be NULL.
Changes 2/15/94 -- added measure_verb param which indicates the type of output
wanted (see enums in Paige.h). Also removed style pointer and font pointer,
replaced with a single style_walk_ptr param.
Added 4/14/94: if scale_widths is TRUE the result(s) are to be scaled.  */

PG_PASCAL (void) pgMeasureProc (paige_rec_ptr pg, style_walk_ptr walker,
      pg_char_ptr data, size_t length, pg_short_t slop, long PG_FAR *positions,
      short PG_FAR *types, short measure_verb, size_t current_offset, pg_boolean scale_widths,
      short call_order)
{
   register short       PG_FAR *each_type;
   register int         PG_FAR *int_positions;
   register pg_char_ptr    text;
   register long           PG_FAR *long_positions;
   register pg_short_t        the_byte;
   pg_char_ptr           measure_data, original_data;
   short PG_FAR          *the_types;
   long PG_FAR           *measure_positions;
   memory_ref            transliterate_ref;
   style_info_ptr       style, original_style;
   font_info_ptr           font;
   short                char_bytes, right_to_left, soft_hyphens_exist;
   short                system_just, done_types;
   long               measure_length, position_bias, extra_just, scale_slop, use_slop;
   long               total_length, position_ctr, num_spaces;
   pg_fixed          space_extra_width, space_accumulation;
   pg_boolean           small_caps_compensate;

   style = original_style = walker->cur_style;
   font = walker->cur_font;
   the_types = types;
   
   if (style->class_bits & NON_TEXT_BITS) {
      
      for (measure_length = 0, long_positions = positions;
            measure_length <= length; ++measure_length)
         long_positions[measure_length] = *long_positions;
      
      if (the_types)
         pgFillBlock(the_types, length * sizeof(short), 0);

      return;
   }
   
   if (transliterate_ref = pgConvertTextCaps(pg, style, data, length))
      measure_data = (pg_char_ptr) UseMemory(transliterate_ref);
   else
      measure_data = data;
   
   original_data = data;

   total_length = length;
   measure_positions = positions;
   
   use_slop = (long)slop;

// Determine if we need to compensate for full-justify, small caps style:

   small_caps_compensate = (pg_boolean)((original_style->styles[small_caps_var] != 0) && (slop != 0));
   
   if (small_caps_compensate) {
      
      if (!(num_spaces = get_space_extra((HDC)pg->port.machine_ref, data, (short)length, 0)))
         small_caps_compensate = FALSE;
      else {
      
         space_extra_width = (pg_fixed)pgFixedRatio((short)slop, (short)num_spaces);
         space_accumulation = 0;
      }
   }

   while (total_length) {
      
      measure_length = total_length;
      walker->cur_style = style = original_style;

      if (style->styles[small_caps_var]) {
         long         info_flags;
         
         measure_length = pgSameCaseLength(pg, walker, original_data,
               current_offset, total_length, &info_flags);
         
         if (!info_flags) {
            
            style = (style_info_ptr) UseMemoryRecord(pg->t_formats, style->small_caps_index,
                  0, FALSE);
            walker->cur_style = style;
         }
         
         style->procs.install(pg, style, walker->cur_font, &walker->superimpose, walker->style_overlay, FALSE);

         if (small_caps_compensate) {
            
            if ((num_spaces = get_space_extra((HDC)pg->port.machine_ref, measure_data, (short)measure_length, 0)) > 0) {

               pg_fixed       fractional_slop;
               
               fractional_slop = (pg_fixed)num_spaces;
               fractional_slop <<= 16;
               fractional_slop = pgMultiplyFixed(fractional_slop, space_extra_width);
               space_accumulation += fractional_slop;
               use_slop = HIWORD(space_accumulation);
               space_accumulation &= 0x0000FFFF;
            }
            else
               use_slop = 0;
         }
      }

      int_positions = (int PG_FAR *) measure_positions;
      position_bias = *measure_positions;
      scale_slop = use_slop;
      extra_just = 0;
      
      if (scale_slop)
         num_spaces = get_space_extra((HDC)pg->port.machine_ref, measure_data, (short)measure_length, 0);
      else
         num_spaces = 0;

      pgMeasureText(pg, measure_verb, measure_data, measure_length,
               scale_slop - extra_just, num_spaces, int_positions, walker);

      right_to_left = (font->char_type & RIGHT_FONT_FLAG);
      system_just = pgSystemDirection(pg->globals);

      long_positions = measure_positions;
      position_ctr = measure_length + 1;
      long_positions += position_ctr;
      int_positions += position_ctr;
   
      *(--long_positions) = *(--int_positions);
      *long_positions += position_bias;
      --position_ctr;

      if (each_type = the_types) {
      
         each_type += position_ctr;
         text = original_data + position_ctr;
         char_bytes = style->char_bytes;
      }
   
      done_types = 0;
      soft_hyphens_exist = FALSE;
   
      while (position_ctr) {
         *(--long_positions) = *(--int_positions);
         *long_positions += position_bias;
   
         if (each_type) {
   
            ++done_types;
   
            the_byte = *(--text);
            *(--each_type) = 0;
   
            if (!char_bytes) {
               
               if (the_byte == ' ')
                  *each_type = BLANK_BIT;
               else
               if (the_byte < ' ') {
                  pg_globals_ptr   globals;
                  
                  globals = pg->globals;
      
                  *each_type |= CTL_BIT;
         
                  if (the_byte == globals->line_wrap_char)
                     *each_type |= PAR_SEL_BIT;
                  else
                  if (the_byte == globals->soft_line_char)
                     *each_type |= LINE_SEL_BIT;
                  else
                  if (the_byte == globals->tab_char)
                     *each_type |= TAB_BIT;
                  else
                  if (the_byte == globals->container_brk_char)
                     *each_type |= CONTAINER_BRK_BIT;
                  else
                  if (the_byte == globals->ff_char)
                     *each_type |= PAGE_BRK_BIT;
                  else
                  if (the_byte == globals->soft_hyphen_char) {
      
                     *each_type |= SOFT_HYPHEN_BIT;
                     soft_hyphens_exist = TRUE;
                  }
               }
            }
         }
   
         --position_ctr;
      }
      
      if (soft_hyphens_exist) {
         long      hyphen_width, older_position;
         HDC           hdc = pgGetPlatformDevice(&pg->port);

         hyphen_width = MEASURE_SYMBOL(pg->globals->hyphen_char);
         pgReleasePlatformDevice(&pg->port);
         position_bias = 0;
   
         while (done_types) {
            
            older_position = *long_positions;
            *long_positions += position_bias;

            if (*each_type & SOFT_HYPHEN_BIT)                   
               position_bias += older_position - long_positions[1] + hyphen_width;
            
            ++long_positions;
            ++each_type;
            --done_types;
         }
      }
   
      if ( right_to_left && (system_just >= 0) && (measure_verb != measure_width_locs)) {
         
         long         trailing_pos;
         
         trailing_pos = *measure_positions;
         *measure_positions = measure_positions[measure_length];
         
         if (call_order == -1)
            measure_positions[measure_length] = trailing_pos;
      }
   
      if (scale_widths == COMPENSATE_SCALE) {
         pg_fixed    scaling;
         long         origin;
   
         long_positions = measure_positions;
         scaling = -pg->scale_factor.scale;      /* To upscale */
         origin = *long_positions;
   
         for (position_ctr = measure_length + 1; position_ctr; --position_ctr, ++long_positions)
            pgScaleLong(scaling, origin, long_positions);
      }
      
      measure_positions += measure_length;
      original_data += measure_length;
      
      if (the_types)
         the_types += measure_length;

      measure_data += measure_length;
      total_length -= measure_length;
   }

   if (transliterate_ref)
      UnuseMemory(transliterate_ref);

   walker->cur_style = original_style;
}



/* DEFAULT FUNCTION RETURN INFO ABOUT A CHARACTER. It can be somewhat machine-
dependent so we put it in "machinespecific.c". NOTE: Unlike pgDrawProc the
font, style, etc. is not necessarily pre-set.
Changes 1/20/94 -- added parameter "mask_bits" that informs the function which
bits you care about. This greatly increases the potential performance particularly
when only a few character attributes need to be checked. 
Also added, "global_offset" which is the absolute offset base for which local_offset
is relative to (global_offset is usually the text_block.begin value).
Also function result is now a long. */

#ifdef UNICODE
PG_PASCAL (long) pgCharInfoProc (paige_rec_ptr pg, style_walk_ptr style_walker,
      pg_char_ptr data, size_t block_offset, size_t offset_begin, size_t offset_end, size_t char_offset,
      long mask_bits)
{
   register    style_walk_ptr  walker;
   register    pg_char        the_char;
   register    long         result;
   register    pg_globals_ptr  globals;
   long                     real_break_bit, class_bits, proc_char_offset;
   pg_boolean                  font_non_roman;
   pg_short_t                  decimal_char, hyphen_char;
   pg_short_t                  symbol_check;
   short                    char_bytes;

   walker = style_walker;

   if ((pg->flags & NO_HIDDEN_TEXT_BIT) && walker->cur_style->styles[hidden_text_var])
      return  0;

   the_char = data[char_offset];

   font_non_roman = (pg_boolean)((walker->cur_font->char_type & NON_ROMAN_FLAG) != 0);
   proc_char_offset = char_offset;

   if (!(mask_bits & NON_MULTIBYTE_BITS))
      if (the_char < 256)
         return   0;

   class_bits = walker->cur_style->class_bits;
   globals = pg->globals;
   
   symbol_check = the_char;

   if (mask_bits & DECIMAL_CHAR_BIT)
      decimal_char = global_char_to_int(globals->decimal_char);
   else
      decimal_char = 0;

   if (mask_bits & WORD_BREAK_BIT)
      hyphen_char = global_char_to_int(globals->hyphen_char);
   else
      hyphen_char = 0;

   result = 0;

   real_break_bit = WORD_BREAK_BIT;
   if (class_bits & CANNOT_BREAK)
      if (walker->current_offset != walker->prev_style_run->offset)
         real_break_bit = 0;
   
   char_bytes = walker->cur_style->char_bytes + 1;

   if ((char_bytes > 1) || (class_bits & NON_TEXT_BITS)) {
      size_t    distance_in;
      short     byte_remainder;
      
      /* Multi-byte char or non-text chars */
      
      distance_in = walker->current_offset - walker->prev_style_run->offset;
      byte_remainder = (short)distance_in;
      if (!(byte_remainder %= char_bytes))
         result |= (FIRST_HALF_BIT | WORD_BREAK_BIT | WORD_SEL_BIT);
      else {
         
         if (byte_remainder == (char_bytes - 1))
            result |= (LAST_HALF_BIT | WORD_BREAK_BIT | WORD_SEL_BIT
                  | INCLUDE_SEL_BIT | INCLUDE_BREAK_BIT);
         else
            result |= MIDDLE_CHAR_BIT;
      }

      result |= NON_TEXT_BIT;
   }
   else
   if (the_char > 255) {
      long        next_char_offset;
      WORD        char_type, second_char_type;

      if (font_non_roman)
         result |= NON_ROMAN_BIT;

      result |= set_non_roman_bits (pg, walker, data, offset_begin, offset_end, char_offset,
            mask_bits, &char_type, &symbol_check, FALSE);

      if (char_type & C3_SYMBOL)
         result |= SYMBOL_BIT;
   
      if (result & BLANK_BIT)
         result |= (WORD_SEL_BIT);

      result |= WORD_BREAK_BIT;
      next_char_offset = char_offset + 1;

      if ((mask_bits & (WORD_BREAK_BIT | WORD_SEL_BIT)) && next_char_offset < offset_end) {
         long     result2;

         result2 = set_non_roman_bits (pg, walker, data, offset_begin, offset_end, next_char_offset,
            mask_bits, &second_char_type, NULL, FALSE);
         
         if (data[next_char_offset] < 256)
            result |= WORD_BREAK_BIT | INCLUDE_BREAK_BIT | WORD_SEL_BIT | INCLUDE_SEL_BIT;
         else {

            if (char_type != second_char_type)
               result |= WORD_SEL_BIT | INCLUDE_SEL_BIT;
         }
      }
   }
   else {
   
      if (mask_bits == BLANK_BIT) {
         
         if (the_char <= ' ')
            result |= BLANK_BIT;
      }
      else
      if (mask_bits & (~MULTICHAR_BITS)) {
   
         if (the_char <= ' ') {
            
            if (the_char < ' ') {
            
               result |= CTL_BIT;
               
               if (mask_bits & (PAR_SEL_BIT | LINE_SEL_BIT | TAB_BIT | CONTAINER_BRK_BIT | PAGE_BRK_BIT | INVIS_ACTION_BIT)) {
               
                  if (the_char == globals->line_wrap_char)
                     result |= (PAR_SEL_BIT | BLANK_BIT);
                  else
                  if (the_char == globals->soft_line_char)
                     result |= (LINE_SEL_BIT | BLANK_BIT);
                  else
                  if (the_char == globals->tab_char)
                     result |= (TAB_BIT | BLANK_BIT);
                  else
                  if (the_char == globals->container_brk_char)
                     result |= (CONTAINER_BRK_BIT | BLANK_BIT);
                  else
                  if (the_char == globals->ff_char)
                     result |= (PAGE_BRK_BIT | BLANK_BIT);
                  else
                     result |= INVIS_ACTION_BIT;
               }
               else
                  result |= BLANK_BIT;
            }
            else
               result |= BLANK_BIT;
   
            if (the_char == globals->soft_hyphen_char) {
               
               result &= (~(WORD_BREAK_BIT | WORD_SEL_BIT));
               result |= (BLANK_BIT | INCLUDE_BREAK_BIT | INCLUDE_SEL_BIT | SOFT_HYPHEN_BIT);
            }
            else
               result |=  (WORD_BREAK_BIT | WORD_SEL_BIT);
         }
         else {
            
            if (the_char == globals->fwd_delete_char)
               result |= INVIS_ACTION_BIT;
            else
            if (font_non_roman && the_char > 255 && (mask_bits & (WORD_SEL_BIT | WORD_BREAK_BIT)))
               result |= (WORD_BREAK_BIT | WORD_SEL_BIT | INCLUDE_BREAK_BIT | INCLUDE_SEL_BIT);
            else
            if (block_offset != NO_BLOCK_OFFSET && last_char_of_script(walker, char_offset + block_offset))
               result |= (WORD_BREAK_BIT | WORD_SEL_BIT | INCLUDE_BREAK_BIT | INCLUDE_SEL_BIT);
            else
            if ((the_char < '0') && (the_char != global_char_to_int(globals->flat_single_quote)) && (the_char != decimal_char)
                  && (the_char != hyphen_char))
               result |= (WORD_SEL_BIT | INCLUDE_BREAK_BIT);
            else
            if ((the_char == global_char_to_int(globals->left_double_quote))
               || (the_char == global_char_to_int(globals->right_double_quote)))
               result |= (WORD_SEL_BIT | INCLUDE_BREAK_BIT);
            else {
               
               if (mask_bits & (WORD_BREAK_BIT | INCLUDE_BREAK_BIT | INCLUDE_SEL_BIT | WORD_SEL_BIT)) {
                  
                  result |= INCLUDE_BREAK_BIT;
                  
                  if (the_char == hyphen_char)
                     result |= WORD_BREAK_BIT;
   
                  if (the_char == decimal_char)
                     result |= WORD_SEL_BIT;
                  else
                  if (the_char < 'A' && the_char > '9')
                     result |= WORD_SEL_BIT;
                  else
                  result |= INCLUDE_SEL_BIT;
               }
            }
         }
   
         if (mask_bits & LONG_FORM_INFO) {
            if ((the_char >= 'A') && (the_char <= 'Z'))
               result |= UPPER_CASE_BIT;
            if ((the_char >= '0') && (the_char <= '9'))
               result |= NUMBER_BIT;
            else
            if ((the_char > 0x20) && (the_char < 0x40))
               result |= PUNCT_NORMAL_BIT;
         }
      }
   }  // End Roman char check

   if (symbol_check == decimal_char)
      result |= DECIMAL_CHAR_BIT;
      
   if (mask_bits & QUOTE_BITS) {

      if (global_char_to_int(globals->flat_single_quote) == symbol_check)
         result |= (FLAT_QUOTE_BIT | SINGLE_QUOTE_BIT);

      if (global_char_to_int(globals->flat_double_quote) == symbol_check)
         result |= (FLAT_QUOTE_BIT);

      if (global_char_to_int(globals->left_single_quote) == symbol_check)
         result |= (LEFT_QUOTE_BIT | SINGLE_QUOTE_BIT);

      if (global_char_to_int(globals->right_single_quote) == symbol_check)
         result |= (RIGHT_QUOTE_BIT | SINGLE_QUOTE_BIT);

      if (global_char_to_int(globals->left_double_quote) == symbol_check)
         result |= (LEFT_QUOTE_BIT);

      if (global_char_to_int(globals->right_double_quote) == symbol_check)
         result |= (RIGHT_QUOTE_BIT);
   }

   if (mask_bits & LOWER_CASE_BIT)
      if (!(result & (QUOTE_BITS | UPPER_CASE_BIT | PUNCT_NORMAL_BIT | NUMBER_BIT | SYMBOL_BIT)))
         result |= LOWER_CASE_BIT;

   if (the_char == SUBREF_CHAR)
      result &= (~BLANK_BIT); 

   if (mask_bits & WORDBREAK_PROC_BITS)
      result = pg->procs.wordbreak_proc(pg, &data[char_offset], char_bytes, walker->cur_style,
            walker->cur_font, result);

   return  result & mask_bits;
}

#else
// Non-Unicode:

PG_PASCAL (long) pgCharInfoProc (paige_rec_ptr pg, style_walk_ptr style_walker,
      pg_char_ptr data, long block_offset, long offset_begin, long offset_end, long char_offset,
      long mask_bits)
{
   register    style_walk_ptr  walker;
   register    pg_char        the_char;
   register    long         result;
   register    pg_globals_ptr  globals;
   long                     real_break_bit, class_bits, proc_char_offset;
   pg_boolean                  non_roman, font_non_roman;
   pg_short_t                  decimal_char, hyphen_char;
   pg_short_t                  symbol_check;
   short                    char_bytes;

   walker = style_walker;

   if ((pg->flags & NO_HIDDEN_TEXT_BIT) && walker->cur_style->styles[hidden_text_var])
      return  0;

   the_char = data[char_offset];

   non_roman = (((walker->cur_font->char_type & NON_ROMAN_FLAG) != 0) && (mask_bits & NON_CTL_CHAR_BITS));

   font_non_roman = non_roman;
   proc_char_offset = char_offset;

   if (!(mask_bits & NON_MULTIBYTE_BITS))
      if (!non_roman)
         return   0;

   class_bits = walker->cur_style->class_bits;
   globals = pg->globals;
   
   symbol_check = the_char;

   if (mask_bits & DECIMAL_CHAR_BIT)
      decimal_char = global_char_to_int(globals->decimal_char);
   else
      decimal_char = 0;

   if (mask_bits & WORD_BREAK_BIT)
      hyphen_char = global_char_to_int(globals->hyphen_char);
   else
      hyphen_char = 0;

   result = 0;

   real_break_bit = WORD_BREAK_BIT;
   if (class_bits & CANNOT_BREAK)
      if (walker->current_offset != walker->prev_style_run->offset)
         real_break_bit = 0;
   
   char_bytes = walker->cur_style->char_bytes + 1;

   if ((char_bytes > 1) || (class_bits & NON_TEXT_BITS)) {
      long      distance_in;
      short    byte_remainder;
      
      /* Multi-byte char or non-text chars */
      
      distance_in = walker->current_offset - walker->prev_style_run->offset;
      byte_remainder = (short)distance_in;
      if (!(byte_remainder %= char_bytes))
         result |= (FIRST_HALF_BIT | WORD_BREAK_BIT | WORD_SEL_BIT);
      else {
         
         if (byte_remainder == (char_bytes - 1))
            result |= (LAST_HALF_BIT | WORD_BREAK_BIT | WORD_SEL_BIT
                  | INCLUDE_SEL_BIT | INCLUDE_BREAK_BIT);
         else
            result |= MIDDLE_CHAR_BIT;
      }

      result |= NON_TEXT_BIT;
   }
   else
   
   if (non_roman) {
      WORD        first_char_type;

      result |= NON_ROMAN_BIT;

      result |= set_non_roman_bits (pg, walker, data, offset_begin, offset_end, char_offset,
            mask_bits, &first_char_type, &symbol_check, FALSE);

      if (result & (FIRST_HALF_BIT | LAST_HALF_BIT)) {  // If multibyte char...
         long        next_char_offset;
         WORD        second_char_type;
         
         if ((char_offset + 1) < offset_end)
            char_bytes = 2;

         if (first_char_type & C3_SYMBOL)
            result |= SYMBOL_BIT;
      
         if (result & BLANK_BIT) {

            result |= WORD_SEL_BIT;

            if (result & LAST_HALF_BIT)
               result |= INCLUDE_SEL_BIT;
         }
            
         result |= WORD_BREAK_BIT;
         next_char_offset = char_offset + 2;

         if ((result & FIRST_HALF_BIT) && (mask_bits & (WORD_BREAK_BIT | WORD_SEL_BIT)) && next_char_offset < offset_end) {
            long     result2;

            result2 = set_non_roman_bits (pg, walker, data, offset_begin, offset_end, next_char_offset,
               FIRST_HALF_BIT | LAST_HALF_BIT, &second_char_type, NULL, TRUE);
            
            if ((result2 & (FIRST_HALF_BIT | LAST_HALF_BIT)) == 0)
               result |= WORD_BREAK_BIT | INCLUDE_BREAK_BIT | WORD_SEL_BIT | INCLUDE_SEL_BIT;
            else {

               if (first_char_type != second_char_type)
                  result |= WORD_SEL_BIT | INCLUDE_SEL_BIT;
            }
         }
      }
      else  {

         non_roman = FALSE;   // Forces this to fall through to ASCII test.
         result = 0;
      }
   }

   if (!non_roman) {
   
      if (mask_bits == BLANK_BIT) {
         
         if (the_char <= ' ')
            result |= BLANK_BIT;
      }
      else
      if (mask_bits & (~MULTICHAR_BITS)) {
   
         if (the_char <= ' ') {
            
            if (the_char < ' ') {
            
               result |= CTL_BIT;
               
               if (mask_bits & (PAR_SEL_BIT | LINE_SEL_BIT | TAB_BIT | CONTAINER_BRK_BIT | PAGE_BRK_BIT | INVIS_ACTION_BIT)) {
               
                  if (the_char == globals->line_wrap_char)
                     result |= (PAR_SEL_BIT | BLANK_BIT);
                  else
                  if (the_char == globals->soft_line_char)
                     result |= (LINE_SEL_BIT | BLANK_BIT);
                  else
                  if (the_char == globals->tab_char)
                     result |= (TAB_BIT | BLANK_BIT);
                  else
                  if (the_char == globals->container_brk_char)
                     result |= (CONTAINER_BRK_BIT | BLANK_BIT);
                  else
                  if (the_char == globals->ff_char)
                     result |= (PAGE_BRK_BIT | BLANK_BIT);
                  else
                     result |= INVIS_ACTION_BIT;
               }
               else
                  result |= BLANK_BIT;
            }
            else
               result |= BLANK_BIT;
   
            if (the_char == globals->soft_hyphen_char) {
               
               result &= (~(WORD_BREAK_BIT | WORD_SEL_BIT));
               result |= (BLANK_BIT | INCLUDE_BREAK_BIT | INCLUDE_SEL_BIT | SOFT_HYPHEN_BIT);
            }
            else
               result |=  (WORD_BREAK_BIT | WORD_SEL_BIT);
         }
         else {
            
            if (the_char == globals->fwd_delete_char)
               result |= INVIS_ACTION_BIT;
            else
            if (font_non_roman && (mask_bits & (WORD_SEL_BIT | WORD_BREAK_BIT))
               && (set_non_roman_bits(pg, walker, data, offset_begin,
               offset_end, char_offset + 1, FIRST_HALF_BIT | LAST_HALF_BIT,
               NULL, NULL, TRUE) & (FIRST_HALF_BIT | LAST_HALF_BIT)) )
               result |= (WORD_BREAK_BIT | WORD_SEL_BIT | INCLUDE_BREAK_BIT | INCLUDE_SEL_BIT);
            else
            if (block_offset != NO_BLOCK_OFFSET && last_char_of_script(walker, char_offset + block_offset))
               result |= (WORD_BREAK_BIT | WORD_SEL_BIT | INCLUDE_BREAK_BIT | INCLUDE_SEL_BIT);
            else
            if ((the_char < '0') && (the_char != global_char_to_int(globals->flat_single_quote)) && (the_char != decimal_char)
                  && (the_char != hyphen_char))
               result |= (WORD_SEL_BIT | INCLUDE_BREAK_BIT);
            else
            if ((the_char == global_char_to_int(globals->left_double_quote))
               || (the_char == global_char_to_int(globals->right_double_quote)))
               result |= (WORD_SEL_BIT | INCLUDE_BREAK_BIT);
            else {
               
               if (mask_bits & (WORD_BREAK_BIT | INCLUDE_BREAK_BIT | INCLUDE_SEL_BIT | WORD_SEL_BIT)) {
                  
                  result |= INCLUDE_BREAK_BIT;
                  
                  if (the_char == hyphen_char)
                     result |= WORD_BREAK_BIT;
   
                  if (the_char == decimal_char)
                     result |= WORD_SEL_BIT;
                  else
                  if (the_char < 'A' && the_char > '9')
                     result |= WORD_SEL_BIT;
                  else
                  result |= INCLUDE_SEL_BIT;
               }
            }
         }
   
         if (mask_bits & LONG_FORM_INFO) {
            if ((the_char >= 'A') && (the_char <= 'Z'))
               result |= UPPER_CASE_BIT;
            if ((the_char >= '0') && (the_char <= '9'))
               result |= NUMBER_BIT;
            else
            if ((the_char > 0x20) && (the_char < 0x40))
               result |= PUNCT_NORMAL_BIT;
         }
      }
   }  // End Roman char check

   if (symbol_check == decimal_char)
      result |= DECIMAL_CHAR_BIT;
      
   if (mask_bits & QUOTE_BITS) {

      if (global_char_to_int(globals->flat_single_quote) == symbol_check)
         result |= (FLAT_QUOTE_BIT | SINGLE_QUOTE_BIT);

      if (global_char_to_int(globals->flat_double_quote) == symbol_check)
         result |= (FLAT_QUOTE_BIT);

      if (global_char_to_int(globals->left_single_quote) == symbol_check)
         result |= (LEFT_QUOTE_BIT | SINGLE_QUOTE_BIT);

      if (global_char_to_int(globals->right_single_quote) == symbol_check)
         result |= (RIGHT_QUOTE_BIT | SINGLE_QUOTE_BIT);

      if (global_char_to_int(globals->left_double_quote) == symbol_check)
         result |= (LEFT_QUOTE_BIT);

      if (global_char_to_int(globals->right_double_quote) == symbol_check)
         result |= (RIGHT_QUOTE_BIT);
   }

   if (mask_bits & LOWER_CASE_BIT)
      if (!(result & (QUOTE_BITS | UPPER_CASE_BIT | PUNCT_NORMAL_BIT | NUMBER_BIT | SYMBOL_BIT)))
         result |= LOWER_CASE_BIT;

   if (the_char == SUBREF_CHAR)
      result &= (~BLANK_BIT); 

   if (mask_bits & WORDBREAK_PROC_BITS)
      if (!(result & (NON_TEXT_BIT | LAST_HALF_BIT | MIDDLE_CHAR_BIT)))
         result = pg->procs.wordbreak_proc(pg, &data[char_offset], char_bytes, walker->cur_style,
               walker->cur_font, result);

   return  result & mask_bits;
}
#endif


/* DEFAULT DRAW TEXT FUNCTION. Obviously machine-dependent. Note that the
font, style, etc. has been pre-set in the current port (or, if your platform
does not do such a thing as pre-set the font and style, use the style info
provided in this function).   Draw_position->to.h & v must get set to
the ending text position, otherwise tab leaders will fail.

NOTE ON TEXT SCALING:  Upon entry, all the begin/ending points for the screen
have ALREADY BEEN SCALED, but the point size of the text is still set to the
non-scaled size.  If scaling is enabled (pg->scale_factor != 0) this function
must take appropriate action to draw in the scaled size.  */

PG_PASCAL (void) pgDrawProc (paige_rec_ptr pg, style_walk_ptr walker, pg_char_ptr data,
      pg_short_t offset, pg_short_t length, draw_points_ptr draw_position,
      long extra, short draw_mode)
{
   register pg_char_ptr    text;
   style_info_ptr          original_style;
   memory_ref              transliterate_ref, x_widths_ref, special_locs;
   Point                   start_pt, end_pen;
   long                    total_length, draw_length, draw_offset, extra_just, use_extra;
   short                   use_mode, underline_bits;
   short                   small_caps_extra;
   pg_boolean              non_transparent_text;
   HDC                     hdc;
   COLORREF                bk_color;
   TEXTMETRIC              tm;
   int   PG_FAR            *x_widths;
   int                     num_spaces;
   long                    style_ascent;
   long PG_FAR             *small_caps_locs;
   pg_boolean              printing;

   if (walker->cur_style->class_bits & NON_TEXT_BITS)
      return;
      
   printing = ((pg->flags & PRINT_MODE_BIT) != 0);
   
   original_style = walker->cur_style;
   draw_offset = (long) offset;
   total_length = length;

   use_mode = draw_mode;
   use_extra = extra;
   
   if (pg->globals->offscreen_enable == OFFSCREEN_SUCCESSFUL)
      use_mode = direct_or;

   start_pt.h = pgLongToShort(draw_position->from.h);
   start_pt.v = pgLongToShort(draw_position->from.v);
   
   end_pen = start_pt;
   start_pt.h -= (short)(walker->cur_style->right_overhang / 2);

   hdc = pgGetPlatformDevice(pg->globals->current_port);
   walker->cur_style->procs.install(pg, walker->cur_style, walker->cur_font, &walker->superimpose, walker->style_overlay, TRUE);
   non_transparent_text = !(pgTransColor(pg->globals, &walker->superimpose.bk_color));

   //style_ascent = walker->superimpose.ascent;

   GetTextMetrics(hdc, &tm);
   style_ascent = tm.tmAscent;
   start_pt.v -= (short)(style_ascent + 1);

   if (non_transparent_text) {
   
      pgColorToOS(&walker->superimpose.bk_color, &bk_color);
      bk_color |= pg->port.palette_select;
         
      SetBkColor(hdc, bk_color);
      use_mode = direct_copy;
   }

   underline_bits = SpecialUnderline(&walker->superimpose);

   if (original_style->styles[small_caps_var] && extra) {

      special_locs = pgGetSpecialLocs(pg, draw_position->block, draw_position->starts,
            length, extra, PLAIN_SCALE);

      small_caps_locs = (long *) UseMemory(special_locs);
   }
   else
      small_caps_locs = NULL;

   while (total_length) {
      
      draw_length = total_length;
      small_caps_extra = 0;
      
      if (original_style->styles[small_caps_var]) {
         long      info_flags;
         
         draw_length = pgSameCaseLength(pg, walker, &data[draw_offset],
            draw_position->real_offset, total_length, &info_flags);

         if (!info_flags)
            walker->cur_style = (style_info_ptr) UseMemoryRecord(pg->t_formats,
                  original_style->small_caps_index, 0, FALSE);

         small_caps_extra = (short)(original_style->ascent - walker->cur_style->ascent);
         
         walker->cur_style->procs.install(pg, walker->cur_style, walker->cur_font, &walker->superimpose, walker->style_overlay, TRUE);
      }

      text = data + draw_offset;
      if (transliterate_ref = pgConvertTextCaps(pg, original_style, text, draw_length))
         text = (pg_char_ptr) UseMemory(transliterate_ref);

      if (small_caps_locs) {
         long        compensated_width, real_width;
         
         compensated_width = small_caps_locs[draw_length] - *small_caps_locs;

         pgSetMeasureDevice(pg);    
         walker->cur_style->procs.install(pg, walker->cur_style, walker->cur_font,
               &walker->superimpose, walker->style_overlay, FALSE);
         real_width = GetTextWidth((HDC)pg->port.machine_ref, (pg_char_ptr)text, (int)draw_length);
         pgUnsetMeasureDevice(pg);

         use_extra = compensated_width - real_width;
         small_caps_locs += draw_length;
      }

      extra_just = use_extra;

      if (use_mode == direct_copy)
         SetBkMode(hdc, OPAQUE);
      else
          SetBkMode(hdc, TRANSPARENT);
        
        SetTextAlign(hdc, TA_LEFT);      // MM found this one

       if (extra_just && draw_length)
         num_spaces = get_space_extra(hdc, text, (short)draw_length, 0);
      else
         num_spaces = 0;
      
       if ((walker->cur_style->char_extra || (walker->cur_style->maintenance & HAS_FORCED_WIDTHS)
            || (extra_just && num_spaces == 0) || printing || pg->port.print_port || pg->port.scale.scale)
            && (draw_length > 1)) {
         rectangle      bounds;
         RECT        bk_box;
         UINT        flags;
              
         if (use_mode == direct_copy)
            flags = ETO_OPAQUE;
         else
            flags = 0;
         
         bounds = draw_position->starts->bounds;
         bk_box.left = start_pt.h;
         bk_box.top = (short)(draw_position->from.v - draw_position->ascent);
         bk_box.bottom = bk_box.top + (short)(bounds.bot_right.v - bounds.top_left.v);
         bk_box.right = bk_box.left + (short)(bounds.bot_right.h - bounds.top_left.h);
            
         x_widths_ref = pg->globals->alpha_widths;
         
         SetMemorySize(x_widths_ref, draw_length + 1);
         x_widths = (int *) UseMemory(x_widths_ref);
         
         pgSetMeasureDevice(pg);
      
         walker->cur_style->procs.install(pg, walker->cur_style, walker->cur_font,
               &walker->superimpose, walker->style_overlay, FALSE);
         end_pen.h += (short)(pgMeasureText(pg, measure_textout, text,
            draw_length, use_extra, (long)num_spaces, x_widths, walker));
  
         pgUnsetMeasureDevice(pg);

		{
			int mode;
			SIZE viewport, window, dummy;

			mode = GetMapMode(hdc);
			GetWindowExtEx(hdc, &window);
			GetViewportExtEx(hdc, &viewport);
			SetMapMode(hdc, MM_TEXT);
			SetMapMode(hdc, mode);
			SetWindowExtEx(hdc, window.cx, window.cy, &dummy);
			SetViewportExtEx(hdc, viewport.cx, viewport.cy, &dummy);

			ExtTextOut(hdc, start_pt.h, start_pt.v + small_caps_extra, flags, &bk_box, (PGSTR)text,
    				(UINT)draw_length, x_widths);
		}

         UnuseMemory(x_widths_ref);
       }
       else {

         if (extra_just && draw_length)
            if (num_spaces)
               SetTextJustification(hdc, (short)use_extra, num_spaces);

         TextOut(hdc, start_pt.h, start_pt.v + small_caps_extra, (PGSTR) text, (int)draw_length);
         end_pen.h += (short)GetTextWidth(hdc, (pg_char_ptr)text, (int)draw_length);
       }

      if (transliterate_ref)
         UnuseMemory(transliterate_ref);
   
      if (draw_position->starts->flags & WORD_HYPHEN_BIT) {
          short      hyphen_size;
          PGSTR      p;
          
          hyphen_size = global_char_size(pg->globals->hyphen_char);
#ifndef GLOBALS_ARE_PSTRINGS
         p = (PGSTR)&pg->globals->hyphen_char[0];
         TextOut(hdc, end_pen.h, start_pt.v + small_caps_extra, p, hyphen_size);   
         end_pen.h += (short)GetTextWidth(hdc, p, hyphen_size);
#else
         p = (PGSTR)&pg->globals->hyphen_char[1];
         TextOut(hdc, end_pen.h, start_pt.v + small_caps_extra, p, hyphen_size);   
         end_pen.h += (short)GetTextWidth(hdc, p, hyphen_size);
#endif
       }

      draw_position->to.h = end_pen.h;
      draw_position->to.v = end_pen.v;
      
      total_length -= draw_length;
      draw_offset += draw_length;
      start_pt.h = end_pen.h;
      walker->cur_style = original_style;
   }

   if (original_style->styles[small_caps_var])
      original_style->procs.install(pg, original_style, walker->cur_font, &walker->superimpose,
         walker->style_overlay, FALSE);
      
   if (walker->superimpose.styles[boxed_var])
      draw_box_style(pg, walker, draw_position);

   if (small_caps_locs)
      UnuseAndDispose(special_locs);

   start_pt.h = pgLongToShort(draw_position->from.h);
   start_pt.v = pgLongToShort(draw_position->from.v);

   if (underline_bits) {
      
      walker->current_offset = draw_position->real_offset;

      if (underline_bits & WORD_UNDERLINE)
         pgDrawWordUnderline(pg, walker, underline_bits, data, offset, length, extra, draw_position);
      else
         pgDrawSpecialUnderline(pg, start_pt, (short)(end_pen.h - start_pt.h),
               walker->cur_style, underline_bits);

      pgSetWalkStyle(walker, draw_position->real_offset);
   }

   if (walker->cur_style->styles[hidden_text_var]) {
      HPEN      hidden_pen, old_pen;
      TEXTMETRIC   tm;
      short     vertical;
      
      GetTextMetrics(hdc, &tm);
      
      hidden_pen = CreatePen(PS_SOLID, 1, (COLORREF)0x00808080);
      old_pen = (HPEN) SelectObject(hdc, hidden_pen);
      vertical = (short)(start_pt.v - (tm.tmAscent / 3));
      MoveToEx(hdc, start_pt.h, vertical, NULL);
      LineTo(hdc, end_pen.h, vertical);
      SelectObject(hdc, old_pen);
      DeleteObject(hidden_pen);
   }

   if (non_transparent_text) {
   
      pgColorToOS(&pg->bk_color, &bk_color);
      bk_color |= pg->port.palette_select;;
      SetBkColor(hdc, bk_color);
   }

   pgReleasePlatformDevice(pg->globals->current_port);
} 


/* DEFAULT TAB DRAW PROC (for leaders, etc.). Obviously machine-dependent. */

PG_PASCAL (void) pgTabDrawProc (paige_rec_ptr pg, style_walk_ptr walker, tab_stop_ptr tab,
      draw_points_ptr draw_position)
{
   memory_ref           filler;
   pg_char_ptr       filler_text;
   pg_char           check_filler[6];
   pg_short_t        leader;
   short             single_width, needed_width, filler_size, filler_width;
   short             underline_bits, i;
    
   if ((leader = (pg_short_t)(tab->leader & 0x0000FFFF)) != 0) {

      for (i = 0; i < 5; ++i)
         check_filler[i] = (pg_char)leader;
   
      single_width = GetTextWidth((HDC)pg->port.machine_ref, (PGSTR)check_filler, 4) / 4;

      needed_width = (short)(draw_position->to.h - draw_position->from.h);
   
      filler_size = needed_width / single_width + 2;
      filler = MemoryAlloc(pg->globals->mem_globals, sizeof(pg_char),
            filler_size, 0);
      filler_text = (pg_char_ptr) UseMemory(filler);
      pgFillBlock(filler_text, filler_size * sizeof(pg_char), (char)leader);
   
      while (filler_size) {
         
         filler_width = GetTextWidth((HDC)pg->port.machine_ref, (PGSTR)filler_text, filler_size);
         if (filler_width <= needed_width)
            break;
         
         --filler_size;
      }
      
      if (filler_size) {
         HDC           hdc;

         hdc = pgGetPlatformDevice(pg->globals->current_port);
         TextOut(hdc, (short) draw_position->to.h - filler_width,
            (short) (draw_position->to.v - walker->superimpose.ascent),
               (PGSTR) filler_text, filler_size);
         pgReleasePlatformDevice(pg->globals->current_port);
      }
   
      UnuseMemory(filler);
      DisposeMemory(filler);
   }

   underline_bits = SpecialUnderline(walker->cur_style);

   if (walker->cur_style->styles[underline_var])
      underline_bits |= FAKE_UNDERLINE;
   
   if (!walker->cur_style->styles[word_underline_var] && underline_bits) {
      Point    start_pt;
      
      start_pt.h = pgLongToShort(draw_position->from.h);
      start_pt.v = pgLongToShort(draw_position->from.v);

      pgDrawSpecialUnderline(pg, start_pt, (short) (draw_position->to.h - draw_position->from.h + 1),
            walker->cur_style, underline_bits);
   }
}


/* pgSpecialCharProc is the default "draw special character" function. This gets
called by Paige when a control character is encountered in drawing mode. The
purpose of this function is to show "invisibles."  It won't get called unless
"show invisibles" is active within the paige item.  */

PG_PASCAL (void) pgSpecialCharProc (paige_rec_ptr pg, style_walk_ptr walker, pg_char_ptr data,
      pg_short_t offset, pg_short_t length, draw_points_ptr draw_position,
      long extra, short draw_mode)
{
   register pg_globals_ptr       globals;
   register pg_short_t            flags;
   point_start_ptr               starts;
   co_ordinate                 draw_point, tab_point;
   long                    show_invisibles;

   show_invisibles = ((pg->flags & (SHOW_INVIS_CHAR_BIT | PRINT_MODE_BIT))
                           == SHOW_INVIS_CHAR_BIT);

   globals = pg->globals;
   starts = draw_position->starts;
   flags = starts->flags;

   if (show_invisibles) {

      draw_point = draw_position->from;
      tab_point = draw_position->to;
      draw_point.h += (starts->bounds.bot_right.h - starts->bounds.top_left.h);
      
      draw_point.v -= set_invisible_symbol_style(pg);
       tab_point.v = draw_point.v;
       
      if (flags & PAR_BREAK_BIT)
         draw_special_char(pg, globals->cr_invis_symbol, &draw_point);
      if (flags & SOFT_PAR_BIT)
         draw_special_char(pg, globals->lf_invis_symbol, &draw_point);
      if (flags & TAB_BREAK_BIT)
         draw_special_char(pg, globals->tab_invis_symbol, &tab_point);
      if (flags & BREAK_CONTAINER_BIT)
         draw_special_char(pg, globals->cont_invis_symbol, &draw_point);
      if ((draw_position->real_offset + length) == pg->t_length)
         if (!(flags & (PAR_BREAK_BIT | SOFT_PAR_BIT)))     
            draw_special_char(pg, globals->end_invis_symbol, &draw_point);

      if (global_char_size(globals->space_invis_symbol) && length) {
         register long PG_FAR *locs;
         register pg_short_t  local_offset;
         memory_ref          special_locs;
         
         special_locs = pgGetSpecialLocs(pg, draw_position->block, draw_position->starts,
               length, extra, PLAIN_SCALE);

         locs = (long *) UseMemory(special_locs);

         local_offset = 0;
         draw_point.h = draw_position->from.h;
         
         while (local_offset < length) {

            if (walker->cur_style->procs.char_info(pg, walker, data, draw_position->block->begin,
               offset, length, local_offset + offset, BLANK_BIT | CTL_BIT) == BLANK_BIT) {

               draw_point.h = (short) (draw_position->from.h + *locs);
               draw_special_char(pg, globals->space_invis_symbol, &draw_point);
            }

            ++local_offset;
            ++locs;
         }
         
         UnuseAndDispose(special_locs);
      }
      
      walker->cur_style->procs.install(pg, walker->cur_style, walker->cur_font,
            &walker->superimpose, walker->style_overlay, (short)(globals->offscreen_port.machine_ref != 0));
   }
   
   if (flags & NEW_PAR_BIT) {
      par_info_ptr par;
      
      par = walker->cur_par_style;
      
      if (par->class_info & BULLETED_LINE) {
         RECT        bullet;
         HDC            hdc;
         HBRUSH         brush, old_brush;
         LOGPEN         logpen;
         HPEN        pen, old_pen;
         long        bullet_width;
         COLORREF fg_color;
         long bullet_type;

         bullet_type = par->html_bullet;
         hdc = (HDC)pg->globals->current_port->machine_ref;
         // Only draw hollow circle dots (at level 2) not hollow rectangles (at level 4),
         // because no other browsers on Windows seem to be doing hollow rectangles any more. GCW 8/20/02
         if (bullet_type == 2) {
            
            if (pgTransColor(pg->globals, &walker->cur_style->bk_color))
            	pgColorToOS(&pg->bk_color, &fg_color);
            else
            	pgColorToOS(&walker->cur_style->bk_color, &fg_color);
         }
         else
            pgColorToOS(&walker->cur_style->fg_color, &fg_color);
         brush = CreateSolidBrush((COLORREF)(fg_color & 0x00FFFFFF));

         pgColorToOS(&walker->cur_style->fg_color, &fg_color);
         logpen.lopnStyle = PS_SOLID;
         logpen.lopnWidth.x = logpen.lopnWidth.y = 1;
         // Give a light shadow when going onscreen so that the bullet looks better,
         // but not when printing since printing devices are generally much higher
         // resolution and can draw decent circles.  JDB 2/9/01
         // But only for solid circle dots. GCW 8/20/02
         logpen.lopnColor = fg_color;
         if ( (bullet_type == 1) && !(pg->flags & PRINT_MODE_BIT) )
             logpen.lopnColor |= 0x808080;
         pen = CreatePenIndirect(&logpen);
         old_pen = (HPEN) SelectObject(hdc, pen);
         old_brush = (HBRUSH) SelectObject(hdc, brush);

         draw_point = draw_position->from;
         bullet.left = bullet.right = (short)(draw_point.h - 5);
         
         bullet_width = pg->globals->bullet_size;
         // Certain bullet sizes look better than others.  JDB 2/9/01
         if (bullet_width == 6 || bullet_width == 7)
             bullet_width = 5;
         pgScaleLong(pg->scale_factor.scale, 0, &bullet_width);

         // Center bullet.  JDB 2/9/01
         // Use walker instead of draw_position because draw_position is affected by
         // top_extra, which is used in the new bullet offsetting code. GCW 8/21/02
         bullet.bottom = bullet.top = (short)draw_point.v + walker->cur_style->descent / 2 -
             ((walker->cur_style->ascent - bullet_width) / 2);

         bullet.left -= (int)bullet_width;
         bullet.top -= (int)bullet_width;

         // Draw a rectangle for levels 3 and above (rather than just levels 3 & 4). GCW 8/20/02
         if (bullet_type >= 3)
            Rectangle(hdc, bullet.left, bullet.top, bullet.right, bullet.bottom);
         else
            Ellipse(hdc, bullet.left, bullet.top, bullet.right, bullet.bottom);

         SelectObject(hdc, old_pen);
         SelectObject(hdc, old_brush);
         DeleteObject(pen);
         DeleteObject(brush);
      }
   }
}



/* DEFAULT FONT INIT PROC. This is machine-dependent. */

PG_PASCAL (void) pgInitFont (paige_rec_ptr pg, font_info_ptr info)
{
   HDC            hdc;
   short       name_length;

   if (!info->name[0])
      return;

   if (!info->platform)
      info->platform = PAIGE_GRAPHICS;

   info->environs &= (~FONT_USES_ALTERNATE);

   pgFixFontName(info);
   //¥ TRS/OITC
   if ((name_length = FONT_NAME_SIZE(info->name) + 1) > (FONT_SIZE)) {
   
      name_length = FONT_SIZE - 1;
      info->name[0] = (pg_char)name_length;
   }
   
   //¥ TRS/OITC
   pgFillBlock(&info->name[name_length], (FONT_SIZE - name_length) * sizeof(pg_char), 0);

   info->environs &= (~(FONT_NOT_AVAIL | FONT_BEST_GUESS));

/* Validate the font record per platform: */
      
   if ( (info->environs & FONT_GOOD) || (pgIsRealFont(pg->globals, info, FALSE)))
      info->environs |= FONT_GOOD;
   else {
      
      if (pgIsRealFont(pg->globals, info, TRUE))
         info->environs |= FONT_GOOD | FONT_USES_ALTERNATE;
      else
         info->environs |= (FONT_NOT_AVAIL | FONT_BEST_GUESS);
   }

    if (!info->machine_var[PG_OUT_PRECISION])
      info->machine_var[PG_OUT_PRECISION] = OUT_DEFAULT_PRECIS;
    if (!info->machine_var[PG_CLIP_PRECISION])
      info->machine_var[PG_CLIP_PRECISION] = CLIP_DEFAULT_PRECIS;
    if (!info->machine_var[PG_QUALITY])
      info->machine_var[PG_QUALITY] = DRAFT_QUALITY;
   
   if (name_length == 1)  { /* null font name = default for this window */
      
      hdc = pgGetPlatformDevice(&pg->port);
      GetTextFace(hdc, FONT_SIZE, &info->name[1]);
      info->name[0] = (pg_char)pgCStrLength((pg_c_string_ptr)&info->name[1]);

      pgReleasePlatformDevice(&pg->port);
   }

   init_font_record (pg, info);
}




/* pgInitDefaultFont must initialize a font record to the default settings.
NOTE: Upon entry, the entire record has been filled with zeros. Note that
for Macintosh, only the font name is initialized. This is because all other
fields get set when the general hook, "pgInitFont" gets called. */

PG_PASCAL (void) pgInitDefaultFont (const pg_globals_ptr globals, font_info_ptr font)
{
   HDC            hdc;
   short       char_type;

   font->environs = FONT_GOOD;
   font->platform = PAIGE_GRAPHICS;
    pgIsRealFont(globals, font, FALSE);
 
    if (!font->machine_var[PG_OUT_PRECISION])
      font->machine_var[PG_OUT_PRECISION] = OUT_DEFAULT_PRECIS;
    if (!font->machine_var[PG_CLIP_PRECISION])
      font->machine_var[PG_CLIP_PRECISION] = CLIP_DEFAULT_PRECIS;
    if (!font->machine_var[PG_QUALITY])
      font->machine_var[PG_QUALITY] = DRAFT_QUALITY;
   if (!font->machine_var[PG_CHARSET])
#ifndef WIN_CHARSET
      font->machine_var[PG_CHARSET] = DEFAULT_CHARSET;
#else
      font->machine_var[PG_CHARSET] = WIN_CHARSET;
#endif
   if (font->name[0] == 0)  { /* null font name = default for this window */

      hdc = (HDC)globals->machine_const;
      GetTextFace(hdc, FONT_SIZE - 1, (PGSTR)&font->name[1]);
      font->name[0] = lstrlen(&font->name[1]);
   }
#ifdef WIN_MULTILINGUAL 

#ifdef WIN16_COMPILE
      font->code_page = GetKBCodePage();
      font->char_type = (short)font->machine_var[PG_CHARSET];
#else
      font->code_page = GetConsoleOutputCP();
      font->char_type = GetTextCharset((HDC)globals->machine_const) & SCRIPT_CODE_MASK;
#endif
      font->environs |= FONT_BEST_GUESS;
      init_font_language(globals, font, (HDC)globals->machine_const);
#else
   font->char_type = WIN_CHARSET;      //Win16 character set
   font->code_page = WIN_CODEPAGE;     //Win16 code page
   font->language = WIN_LANGUAGE;      //Win16 language
#endif
   char_type = font->char_type & SCRIPT_CODE_MASK;
   
   if (font->machine_var[PG_CHARSET] > SYMBOL_CHARSET)
      font->char_type |= NON_ROMAN_FLAG;

   if (char_type == HEBREW_CHARSET || char_type == ARABIC_CHARSET || char_type == THAI_CHARSET)
      font->char_type |= RIGHT_FONT_FLAG;
}



/* pgSetGrafDevice should "install" the graphics device to become the target
for subsequent display (if verb == set_pg_device). The function should also preserve the state of the
device (such as save off the current device and current settings) as necessary.
If bk_color is not NULL the background color gets set to that color.
Note: This function will always get balanced with a subsequent call with unset_pg_device. */

PG_PASCAL (void) pgSetGrafDevice (paige_rec_ptr pg, short verb, graf_device_ptr device,
         color_value_ptr bk_color)
{
   register port_preserve     PG_FAR *preserve_ptr;
   pg_globals_ptr             globals;
   HDC                     	  hdc;
   COLORREF             	  color;
   size_t                     stack_size;

   globals = pg->globals;

   if (verb == unset_pg_device) {
   
      stack_size = GetMemorySize(device->graf_stack) - 1;
      preserve_ptr = (port_preserve *) UseMemory(device->graf_stack);
      preserve_ptr += stack_size;

      globals->current_port = preserve_ptr->last_device;
   
      if (preserve_ptr->saved_dc_index)
         RestoreDC((HDC)device->machine_ref, preserve_ptr->saved_dc_index);
      
      pgReleasePlatformDevice(device);
      UnuseMemory(device->graf_stack);
      SetMemorySize(device->graf_stack, stack_size);
   }
   else {
   
      preserve_ptr = (port_preserve *) AppendMemory(device->graf_stack, 1, FALSE);
      preserve_ptr->last_device = globals->current_port;
      globals->current_port = device;
   
      hdc = pgGetPlatformDevice(device);
      GetClipBox(hdc, (LPRECT)&preserve_ptr->rgn_box);
      preserve_ptr->saved_dc_index = SaveDC(hdc);

      if (pg->doc_info.attributes & COLOR_VIS_BIT)
         device->bk_color = pg->bk_color;
      else
      if (bk_color) {
      
         if ((!pgEqualColor(bk_color, &device->bk_color)))
             device->bk_color = *bk_color;
      }
   
      pgColorToOS(&device->bk_color, &color);
       color |= device->palette_select;;
      SetBkColor(hdc, color);
      UnuseMemory(device->graf_stack);
   }
}



/* pgPrepareOffscreen should set up an offscreen drawing port to be later
transfered to the screen. The (ultimate) target area is target_area. If the
offscreen device is set up, pass the h,v amounts to adjust drawing in offset_adjust.
The actual offscreen transfer occurs in pgFinishOffscreen. For Macintosh, the
offscreen bitMap port is set as the current port when this function exits.

IF YOU CANNOT DO THIS ON YOUR PLATFORM, you should do nothing but erase the
target_area rectangle and pass 0,0 for offset_adjust.

Change 3/29/94 -- real_bits_target specifies the actual area that needs bitmap
transfer. If different than target_area then smaller bitmaps can be created
and the remainder "erased" for increased performance and less memory use. For
example, a portion of the line might be graphics that is drawn separately.

Change 10/16/94 -- we no longer do direct drawing to screen if bitmap too
large. Instead, we make smaller rect and real_bits_target gets set to the
actual target area we have used. So if bits too large for one pass, the caller
must make multiple passes until the bitmap drawing is complete. */

PG_PASCAL (void) pgPrepareOffscreen (paige_rec_ptr pg, rectangle_ptr target_area,
         rectangle_ptr real_bits_target, co_ordinate_ptr offset_adjust,
         long text_offset, point_start_ptr line_start, short draw_mode)
{
   register pg_globals_ptr globals;
    rectangle            bits_rect;
   Rect               target;
   HDC                  bitmap_dc;
   HBITMAP              bitmap;
   COLORREF          bk_color;
   long              data_size, container_num;
   long              bitmapWidth, bitmapHeight;
   short             pix_size, row_bytes;

   globals = pg->globals;

   *real_bits_target = *target_area;
    container_num = 0;
 
   if (line_start)
      container_num = line_start->r_num - 1;
   else
      container_num = 0;

   offset_adjust->h = -real_bits_target->top_left.h;
   offset_adjust->v = -real_bits_target->top_left.v;       
    globals->offscreen_port.access_ctr += 1;
    
   if (globals->offscreen_port.access_ctr == 1) {

       globals->original_target = globals->offscreen_target = *target_area;
      RectangleToRect(target_area, offset_adjust, &target);
   
      bitmap_dc = CreateCompatibleDC((HDC)pg->port.machine_ref);
      bitmapWidth = target.right; // CLM
      bitmapHeight = target.bottom; // CLM
// CLM begin
      {
      long        resolution, scaled_resolution;
            
      resolution = GetDeviceCaps((HDC)bitmap_dc, LOGPIXELSX);
      scaled_resolution = resolution;
      pgScaleLong(pg->port.scale.scale, 0, &resolution);
      pgScaleLong(pg->port.scale.scale, 0, &bitmapWidth);
      pgScaleLong(pg->port.scale.scale, 0, &bitmapHeight);
      bitmapHeight++; // To extend through fractional part (avoids clipping at < 100%)
      SetMapMode((HDC)bitmap_dc, MM_ISOTROPIC);
      SetWindowExtEx((HDC)bitmap_dc, (short)scaled_resolution, (short)scaled_resolution, NULL);
      SetViewportExtEx((HDC)bitmap_dc, (short)resolution, (short)resolution, NULL); 
      }
// CLM end
      if (pg->port.palette) {
         
         SelectPalette(bitmap_dc, (HPALETTE)pg->port.palette, FALSE);
         RealizePalette(bitmap_dc);
         globals->offscreen_port.palette_select = 0x02000000L;
      }
       else
         globals->offscreen_port.palette_select = 0;

      pix_size = GetDeviceCaps(bitmap_dc, BITSPIXEL);
      row_bytes = (short) (((target.right / 8) + 1) * pix_size);
      if (row_bytes & 1)
         ++row_bytes;
      
      data_size = (long)(row_bytes * target.bottom);
      
	  // The bitmap we create has not been limited by this size for quite a while
	  // (note that the call to CreateCompatibleBitmap below uses bitmapHeight
	  // not target.bottom). It makes no sense to do the drawing in chunks when
	  // the bitmap is already big enough.
	  /*
      if (data_size > globals->max_offscreen) {
         
         target.bottom = (short) (globals->max_offscreen / row_bytes);
   
         globals->offscreen_target.bot_right.v
                  = globals->offscreen_target.top_left.v + (long)target.bottom;
         *real_bits_target = globals->offscreen_target;
      }
	  */
   
      //bitmap = CreateCompatibleBitmap((HDC)pg->port.machine_ref, target.right, target.bottom);
      bitmap = CreateCompatibleBitmap((HDC)pg->port.machine_ref, (int)bitmapWidth, (int)bitmapHeight);
      globals->offscreen_buf = PG_LONGWORD(memory_ref) bitmap;
      globals->offscreen_port.previous_items[PREVIOUS_BITMAP] = PG_LONGWORD(size_t)SelectObject(bitmap_dc, bitmap);
      globals->offscreen_port.machine_ref = PG_LONGWORD(generic_var) bitmap_dc;
        globals->current_port = (graf_device_ptr) &globals->offscreen_port;
      pgColorToOS(&pg->bk_color, &bk_color);
      bk_color |= pg->port.palette_select;
      SetBkColor(bitmap_dc, bk_color);
          
      if (!pgEqualColor(&pg->bk_color, &globals->offscreen_port.bk_color))
          globals->offscreen_port.bk_color = pg->bk_color;
         
       globals->offscreen_enable = OFFSCREEN_SUCCESSFUL;
     
      pgSetShapeRect(globals->offscreen_exclusion, NULL);
      RectToRectangle(&target, &bits_rect);
      pgEraseRect(globals, &bits_rect, NULL, NULL);
   }
   
   if (globals->offscreen_enable == OFFSCREEN_SUCCESSFUL) {

      globals->current_port = (graf_device_ptr) &globals->offscreen_port;

      pg->procs.bitmap_proc(pg, &globals->offscreen_port, FALSE,
         &globals->offscreen_target, offset_adjust, text_offset);
   }
}


/* pgFinishOffscreen gets called after pgPrepareOffscreen set up an offscreen
port and text was drawin to it. The purpose of pgFinishOffscreen is to transfer
the "bits" from the offscreen port to the graphics device that was set before
establishing the offscreen device. IF YOU CANNOT DO THIS ON YOUR PLATFORM, this
function will do nothing.

Added 10-17-94, the function returns a Boolean value if the transfer did not
fill the entire area, and if so, the bitmap is kept "alive," the target frame
is offset and new_offset is adjusted to the new drawing offset. In other words,
another pass must be made drawing the same thing but at offset_adjust. ). */

PG_PASCAL (pg_boolean) pgFinishOffscreen (paige_rec_ptr pg, long text_offset,
      point_start_ptr line_start, co_ordinate_ptr new_offset,
      rectangle_ptr new_target, short draw_mode)
{
   register pg_globals_ptr    globals;
   rectangle                 bits_rect, next_target;
   co_ordinate             offset_adjust;
   Rect                 target, source;

   globals = pg->globals;

   if (globals->offscreen_enable != OFFSCREEN_SUCCESSFUL) {
      
      globals->offscreen_enable = FALSE;
      
      return   TRUE;
   }

   offset_adjust.h = -globals->offscreen_target.top_left.h;
   offset_adjust.v = -globals->offscreen_target.top_left.v;
   bits_rect = globals->offscreen_target;
   pgOffsetRect(&bits_rect, offset_adjust.h, offset_adjust.v);
   pg->procs.bitmap_proc(pg, &globals->offscreen_port, TRUE,
      &globals->offscreen_target, &offset_adjust, text_offset);
   
   if ((--globals->offscreen_port.access_ctr) == 0) {

      RectangleToRect(&globals->offscreen_target, NULL, &target);
      source = target;     
      source.right -= source.left;
      source.bottom -= source.top;
      source.top = source.left = 0;

      BitBlt((HDC)pg->port.machine_ref, target.left, target.top,
            source.right, source.bottom, (HDC)globals->offscreen_port.machine_ref,
            0, 0, SRCCOPY);
      
      SelectObject((HDC)globals->offscreen_port.machine_ref, (HANDLE)globals->offscreen_port.previous_items[PREVIOUS_BITMAP]);
      DeleteDC((HDC)globals->offscreen_port.machine_ref);
      DeleteObject((HBITMAP)globals->offscreen_buf);
      globals->offscreen_port.machine_ref = MEM_NULL;
      
      globals->current_port = (graf_device_ptr)&pg->port;
   
      globals->offscreen_enable = FALSE;
   
      if (globals->offscreen_target.bot_right.v < globals->original_target.bot_right.v) {
         
         next_target = globals->original_target;
         next_target.top_left.v = globals->offscreen_target.bot_right.v;
   
         pgPrepareOffscreen(pg, &next_target, new_target, new_offset, text_offset,
               line_start, draw_mode);
   
         return   FALSE;
      }
   }

   return   TRUE;
}


/* pgScrollRect must scroll a rectangle's contents h or v pixels.
NOTE: Both distance_h and distance_v will never be non-zero, only one or the
other will contain a non-zero value. Positive values must move the contents
down or right, negative values up or left.
ADDITION 1/14/94: "target_device" had been added due to background color potential
problem. Mainly, if background color temporarily set to something else the
scrolling function, at least for Mac, needs to restore the color to the original
background so it "fills in" correctly.  Note, however, that all the correct
statuses are currently "pushed" and target_device is the current port.
The affected area gets added to affected_area shape.

Change for 2.0 -- this function returns a region (pg_region) in addition to the shape.
If there is already a scroll_rgn in pg->port, the otherwise function result is
combined with it and that region is returned. */

PG_PASCAL (pg_region) pgScrollRect (paige_rec_ptr pg, rectangle_ptr rect,
      long distance_h, long distance_v, rectangle_ptr affected_area, short draw_mode)
{
   pg_region         scroll_rgn;
   Rect            	 scroll_rect, update_rect;
   HDC               hdc;
   HBRUSH            bk_brush;
   short             amount_h, amount_v;
    
   RectangleToRect(rect, NULL, &scroll_rect);
   
   scroll_rgn = pgCreateRgn();

   amount_h = pgLongToShort(distance_h);
   amount_v = pgLongToShort(distance_v);
      
   hdc = pgGetPlatformDevice(&pg->port);
   
   if (pg->port.scale.scale)
      SetMapMode(hdc, MM_TEXT);

   ScrollDC(hdc, amount_h, amount_v,
         (RECT far *)&scroll_rect, (RECT far *) &scroll_rect, scroll_rgn, (RECT far *)&update_rect);
   
   if (pg->port.machine_var)
      ValidateRect((HWND)pg->port.machine_var, &scroll_rect);

   if (affected_area) {
       
       GetRgnBox(scroll_rgn, &update_rect);  
       RectToRectangle((RECT PG_FAR *)&update_rect, affected_area);
   }
   
   if (pg->doc_info.attributes & COLOR_VIS_BIT) {
   
	   bk_brush = build_background_brush(&pg->port);
	   FillRgn(hdc, scroll_rgn, bk_brush);
	   DeleteObject(bk_brush);
	}
	else
	if ((pg->doc_info.attributes & NO_ERASE_SCROLL_BIT) == 0) {
		
		if (pg->doc_info.gutter_color)
			bk_brush = CreateSolidBrush(pg->doc_info.gutter_color);
		else
			bk_brush = CreateSolidBrush(GetSysColor(COLOR_WINDOW));
		
		FillRgn(hdc, scroll_rgn, bk_brush);
		DeleteObject(bk_brush);
	}

    if (pg->port.scale.scale)
      scale_dc(hdc, &pg->port);

   pgReleasePlatformDevice(&pg->port);

   if (pg->port.scroll_rgn) {
      
      pgUnionRgn(scroll_rgn, pg->port.scroll_rgn, pg->port.scroll_rgn);
      pgDisposeRgn(scroll_rgn);
      scroll_rgn = pg->port.scroll_rgn;
   }

   return      scroll_rgn;
}


/* pgScaleGrafDevice should prepare whatever it has to for subsequent device scaling. For
Windows we zero-out the main scale factor in pg and place it in the device scaling field. */

PG_PASCAL (void) pgScaleGrafDevice (paige_rec_ptr pg)
{
   pg->port.scale = pg->scale_factor;
   pg->port.scale.scale = pg->real_scaling;
   
   pg->scale_factor.scale = 0;
}



/* REVISION 2.0 --
pgClipGrafDevice sets the clipping region for pg->port. The clip_verb controls how the
window is clipped, as follows:  clip_standard_verb clips to the vis_area minus the exclusion
area (and intersected with page_area if appropriate pg attributes are set);
clip_with_current_verb is the same as clip_standard_verb except it is also intersected with
the current clipping region; clip_with_none_verb sets vis_area only (no exclusions are
subtracted and page_area not intersected).
For all clip_verb values, if alternate_vis is non-NULL then it is used instead of the pg's vis
area for computing the regions. */

PG_PASCAL (void) pgClipGrafDevice (paige_rec_ptr pg, short clip_verb, shape_ref alternate_vis)
{
   HDC               hdc;
   pg_scale_factor      scaler;
   rectangle         vis_bounds;
   pg_region         intersect_region, clip_region;
   shape_ref         use_vis;
   RECT           vis_r;
   pg_boolean        scale_vis_area;

   if ((pg->flags & PRINT_MODE_BIT) || (pg->doc_info.attributes & NO_CLIP_REGIONS))
      return;
   
   clip_region = pg->port.clip_rgn;
   hdc = pgGetPlatformDevice(&pg->port);

   if (!(use_vis = alternate_vis))
      use_vis = pg->vis_area;

   #ifdef PG_DEBUG
   if (!hdc)
      pg->globals->mem_globals->debug_proc(PROGRAM_BUG_ASSERT, pg->port.graf_stack);
   #endif

   scale_vis_area = (pg_boolean)((pg->flags & SCALE_VIS_BIT) != 0);
   pgShapeBounds(use_vis, &vis_bounds);

   scaler = pg->port.scale;

   if (scale_vis_area) 
      pgScaleRect(&scaler, NULL, &vis_bounds);

   RectangleToRect(&vis_bounds, NULL, &vis_r);

   switch (clip_verb) {

      case clip_standard_verb:
         
         if (pgVisRegionChanged(pg, TRUE)) {
            
            pgRectToRgn(clip_region, &vis_r);

            if (!(pg->doc_info.attributes & NO_CLIP_PAGE_AREA)
               && (pg->doc_info.attributes & BOTTOM_FIXED_BIT)) {
               // Compute the page_area region
               
               intersect_region = pgCreateRgn();
               pgBuildPageRegion(pg, &scaler, intersect_region);
               pgSectRgn(clip_region, intersect_region, clip_region);
               pgDisposeRgn(intersect_region);
            }
            
            if (!pgEmptyShape(pg->exclude_area)) {
               
               intersect_region = pgCreateRgn();
   
               if (pgBuildExclusionRegion(pg, &scaler, intersect_region))
                  pgSubRgn(clip_region, intersect_region, clip_region);
   
               pgDisposeRgn(intersect_region);
            }
         }

         break;
         
      case clip_with_none_verb:
         clip_region = pgCreateRgn();
         pgRectToRgn(clip_region, &vis_r);
         break;
   }

   if (pg->port.scroll_rgn) {
      
      intersect_region = pgCreateRgn();
      pgSectRgn(clip_region, pg->port.scroll_rgn, intersect_region);
      SelectClipRgn(hdc, intersect_region);
      pgDisposeRgn(intersect_region);
   }
   else
      SelectClipRgn(hdc, clip_region);

   if (clip_verb == clip_with_none_verb)
      pgDisposeRgn(clip_region);
   
   pgReleasePlatformDevice(&pg->port);
}


/* Added 4/12/94, pgSetMeasureDevice is called just before measuring character
widths. Its purpose is to QUICKLY set up some kind of screen device, if
necessary, so the main window isn't affected by subsequent font/style settings.
This may or may not apply to the machine, but for Macintosh we (a) save off
the current port, and (b) set the offscreen port. THIS IS INTENDED TO BE FAST.
(We used to call pgSetGrafDevice which is too slow).  */

PG_PASCAL (void) pgSetMeasureDevice (paige_rec_ptr pg)
{
   pg_globals_ptr       globals;
   
   globals = pg->globals;
   
   if (pg->port.print_port) {
      HDC            printer_dc;
      
      pg->port.machine_ref4 = pg->port.machine_ref;
      pg->port.machine_ref = pg->port.print_port;
      ++pg->port.access_ctr;
      printer_dc = (HDC)pg->port.print_port;
      set_printer_scaling(pg, printer_dc, 0, 0);
   }

   pg->procs.set_device(pg, set_pg_device, &pg->port, NULL);
}


/* Added 4/12/94, pgUnsetMeasureDevice unsets whatever was set up in
pgSetMeasureDevice. For Macintosh we restore the previous port. */

PG_PASCAL (void) pgUnsetMeasureDevice (paige_rec_ptr pg)
{
   pg->procs.set_device(pg, unset_pg_device, &pg->port, NULL);

   if (pg->port.print_port) {
      
      pg->port.machine_ref = pg->port.machine_ref4;
      pg->port.machine_ref4 = 0;
      --pg->port.access_ctr;
      
      unset_printer_scaling((HDC)pg->port.print_port);
   }
}



/* pgEraseRect must fill the rectangle "rect" with the current background
color (or whatever is standard with machine). The globals are given
in case the platform needs it to ascertain the "current port." If scaling
is non-NULL the rect should be scaled by that much. If offset_extra is non-NULL
the rect should be offset by h & v amounts. */

PG_PASCAL (void) pgEraseRect (pg_globals_ptr globals, rectangle_ptr rect,
      pg_scale_ptr scaling, co_ordinate_ptr offset_extra)
{
   rectangle            converted_r;
   HBRUSH            bk_brush;
   HDC               hdc;
   Rect            r;
   
    if (pgEmptyRect(rect))
      return;
    
   pgScaleRectToRect(scaling, rect, &converted_r, offset_extra);
   RectangleToRect(&converted_r, NULL, &r);
   hdc = pgGetPlatformDevice(globals->current_port);
   bk_brush = build_background_brush(globals->current_port);
   FillRect(hdc, (LPRECT)&r, bk_brush);
   DeleteObject(bk_brush);
   pgReleasePlatformDevice(globals->current_port);
}


/* pgScalePointSize must scale the text to the appropriate point size based on
the scaling factor in pg, if any. The point_size param holds the current point size.
Text and length hold the text that will be drawn. The function result is
the amount of extra pixels that need to be compensated (plus or minus) to make
up for any scaling errors.
Modified June 26 94, point size scaled is CURRENT font and walker param
is machine-specific (ignored for Mac but used for Windows). */

PG_PASCAL (short) pgScalePointSize (paige_rec_ptr pg, style_walk_ptr walker,
      pg_char_ptr text, long length, pg_boolean PG_FAR *did_scale)
{
   HDC               hdc;
   long           pixeldiff;
   long           scaled_resolution, actual_extent, should_be_extent;
    
    *did_scale = FALSE;
    pixeldiff = 0;

   if (pg->scale_factor.scale && length) {
      
      hdc = pgGetPlatformDevice(&pg->port);
      
      *did_scale = TRUE;
      scaled_resolution = pg->resolution >> 16;    
      set_alternate_pointsize(pg, scaled_resolution, walker);
        should_be_extent = GetTextWidth(hdc, (PGSTR)text, (int)length);
      should_be_extent &= 0x0000FFFF;
      pgScaleLong(pg->scale_factor.scale, 0, &should_be_extent);
      
      pgScaleLong(pg->scale_factor.scale, 0, &scaled_resolution);
      set_alternate_pointsize(pg, scaled_resolution, walker);

        actual_extent = (long)GetTextWidth(hdc, (PGSTR)text, (int)length);
        actual_extent &= 0x0000FFFF;

      pgReleasePlatformDevice(&pg->port);
      
      pixeldiff = should_be_extent - actual_extent;
   }

   return  (short)pixeldiff;
}



/* pgIsRealFont returns TRUE if the font name exists in the system. If substitute
is non-NULL it is initialized to a name that does exist (used only for Windows).
*/

PG_PASCAL (pg_boolean) pgIsRealFont(pg_globals_ptr globals, font_info_ptr font, pg_boolean use_alternate)
{
   pg_char_ptr       font_name;
   pg_boolean        result;

   //return TRUE;

   if (use_alternate)
      font_name = font->alternate_name;
   else
      font_name = font->name;
   
   result = (pg_boolean)EnumFontFamilies((HDC)globals->machine_const, (PGSTR)&font_name[1],
                     (FONTENUMPROC)enum_pgfont_proc, (LPARAM)font);
    
    return  (pg_boolean)(result == FONT_EXISTS);
}

/* Given an arbitrary point size, pgPointsizeToScreen returns what size to set
a font to achieve pointsize visual size. For example, if screen resolution is
96 dpi then a 12-point font needs to be slightly larger to look like 12 point.
Hence in this case, if pointsize = 72 this function returns 96. */

PG_PASCAL (pg_fixed) pgPointsizeToScreen (pg_ref pg, pg_fixed pointsize)
{
   paige_rec_ptr           pg_rec;
   pg_fixed          result;
   long              doc_resolution;
   int                  resolution;

   pg_rec = (paige_rec_ptr) UseMemory(pg);
   
   doc_resolution = pg_rec->port.resolution >> 16;
   resolution = (int)doc_resolution;
   result = (pg_fixed) windows_pointsize(pointsize, resolution);
   UnuseMemory(pg);
   
   return  (pgRoundFixed(result));
}


/* Given an arbitrary point size on screen, pgScreenToPointsize returns what size to set
a point size in a font really is. For example, if screen resolution is
96 dpi then a 12-point font needs to be slightly larger to look like 12 point.
Hence in this case, if screensize = 96 this function returns 72. */

PG_PASCAL (pg_fixed) pgScreenToPointsize (pg_ref pg, pg_fixed screensize)
{
   paige_rec_ptr        pg_rec;
   pg_fixed       result;
   long           doc_resolution;
   int                resolution;

   pg_rec = (paige_rec_ptr) UseMemory(pg);
   
   doc_resolution = pg_rec->port.resolution >> 16;
   resolution = (int)doc_resolution;
      
   result = (pg_fixed) windows_screensize((long)screensize, resolution);
   UnuseMemory(pg);
   
   return  (pgRoundFixed(result));
}


/* pgOSToPgColor converts an OS-specific color variable to a PAIGE color_value. For
Macintosh, *os_color is an RGBColor. For Windows, *os_color is a COLORREF. */

PG_PASCAL (void) pgOSToPgColor (const pg_plat_color_value PG_FAR *os_color, color_value_ptr pg_color)
{
    unsigned short       red, green, blue;
    
   red = GetRValue(*os_color);
   green = GetGValue(*os_color);
   blue = GetBValue(*os_color);
    
    pg_color->red = pg_color->green = pg_color->blue = pg_color->alpha = 0;

    if (red)
      pg_color->red = (red << 8) | 0x00FF;
    if (green)
      pg_color->green = (green << 8) | 0x00FF;
    if (blue)
      pg_color->blue = (blue << 8) | 0x00FF;
}


/* pgColorToOS converts a PAIGE color_value to an OS-specific color variable.
For Macintosh this creates an RGBColor. For Windows this creates a COLORREF. */

PG_PASCAL (void) pgColorToOS (const color_value_ptr pg_color, pg_plat_color_value PG_FAR *os_color)
{
    *os_color = RGB(pg_color->red >> 8, pg_color->green >> 8, pg_color->blue >> 8);
}



/* pgDrawSpecialUnderline draws all underlining that is otherwise unavailable through the
OS standard styles.  The known text points are start_pt for the
beginning (with start_pt.v as the baseline) and the distance parameter as the
horizontal distance.  Note that "normal" underline might be indicated, which
IS supported by QuickDraw but is required for word underlining.

The underline type(s) to draw are indicated in draw_bits which can be any of
the bits #define at the top of this file.

The ascent is provided in case this is overline.  */

PG_PASCAL (void) pgDrawSpecialUnderline (paige_rec_ptr pg, Point from_pt,
      short distance, style_info_ptr style, short draw_bits)
{
   Point          start_pt;
   HDC                hdc;
   HPEN           underline_pen, old_pen;
   COLORREF       pen_color;
   long           pen_size;
   short          ascent;
   int               pen_type;
   
   ascent = (short)style->ascent;
   start_pt = from_pt;

   if (draw_bits & OVERLINE_LINE)
      start_pt.v -= (ascent - 1);
 
   hdc = pgGetPlatformDevice(pg->globals->current_port);
   
   if (draw_bits & GRAY_UNDERLINE)
      pen_type = PS_DOT;
   else
      pen_type = PS_SOLID;
   
   pgColorToOS(&style->fg_color, &pen_color);
    pen_color |= pg->port.palette_select;
   pen_size = 1;
   pgScaleLong(pg->scale_factor.scale, 0, &pen_size);
   
   if (!pen_size)
      pen_size = 1;

   underline_pen = CreatePen(pen_type, (short)pen_size, pen_color);
   old_pen = (HPEN) SelectObject(hdc, underline_pen);

   MoveToEx(hdc, start_pt.h, start_pt.v, NULL);
   LineTo(hdc, start_pt.h + distance, start_pt.v);

   if (draw_bits & DOUBLE_UNDERLINE) {
      
      start_pt.v += (short)(pen_size * 2);
      MoveToEx(hdc, start_pt.h, start_pt.v, NULL);
      LineTo(hdc, start_pt.h + distance, start_pt.v);
   }
    
    SelectObject(hdc, old_pen);
    DeleteObject(underline_pen);

   pgReleasePlatformDevice(pg->globals->current_port);
}



/* trans_literate converts upper to lower or lower to upper of all chars in
text depending on do_upperase verb. */

PG_PASCAL (void) pgTransLiterate (pg_char_ptr text, long length, pg_char_ptr target,
      pg_boolean do_uppercase)
{
   register pg_char_ptr    src_ptr, target_ptr;
   register long           ctr;
   
   src_ptr = text;
   target_ptr = target;
   
   if (do_uppercase) {
   
      for (ctr = 0; ctr < length; ++ctr) {
         
         target_ptr[ctr] = src_ptr[ctr];
         if (target_ptr[ctr] >= 'a' && target_ptr[ctr] <= 'z')
            target_ptr[ctr] -= ' ';
      }
   }
   else {

      for (ctr = 0; ctr < length; ++ctr) {
         
         target_ptr[ctr] = src_ptr[ctr];
         if (target_ptr[ctr] >= 'A' && target_ptr[ctr] <= 'Z')
            target_ptr[ctr] += ' ';
      }
   }
}



/* pgEmptyRgn returns TRUE if the region is empty. */

PG_PASCAL (pg_boolean) pgEmptyRgn(pg_region rgn)
{
   RECT     r;
   
   return   (pg_boolean)(GetRgnBox(rgn, &r) == NULLREGION);
}

PG_PASCAL (void)pgRectToRgn(pg_region rgn, RECT PG_FAR *r)
{
   SetRectRgn(rgn, r->left, r->top, r->right, r->bottom);
}
 
/* Emulates a Mac Inset region */

//¥ TRS/OITC

PG_PASCAL (void) pgInsetRgn(pg_region rgn, short dh, short dv)
{
   RgnHandle   temp;
   
   temp = pgCreateRgn();
   pgCopyRgn(rgn, temp);

   if (dh < 0)
   {
      pgOffsetRgn(temp, -dh, 0);
      pgUnionRgn(rgn, temp, rgn);
      pgOffsetRgn(temp, dh << 1, 0);
      pgUnionRgn(rgn, temp, rgn);
      pgOffsetRgn(temp, -dh, 0);
   }
   else if (dh > 0)
   {
      pgOffsetRgn(temp, -dh, 0);
      pgSectRgn(rgn, temp, rgn);
      pgOffsetRgn(temp, dh << 1, 0);
      pgSectRgn(rgn, temp, rgn);
      pgOffsetRgn(temp, -dh, 0);
   }

   if (dv < 0)
   {
      pgOffsetRgn(temp, 0, -dv);
      pgUnionRgn(rgn, temp, rgn);
      pgOffsetRgn(temp, 0, dv << 1);
      pgUnionRgn(rgn, temp, rgn);
      pgOffsetRgn(temp, 0, -dv);
   }
   else if (dv > 0)
   {
      pgOffsetRgn(temp, 0, -dv);
      pgSectRgn(rgn, temp, rgn);
      pgOffsetRgn(temp, 0, dv << 1);
      pgSectRgn(rgn, temp, rgn);
   }
   
   pgDisposeRgn(temp);
}

/* This function does GetDC (if access_ctr is zero) or just increments the
access counter and returns the HDC for this device. */

PG_PASCAL (HDC) pgGetPlatformDevice (graf_device_ptr the_device)
{
   HDC         dc;

   if (!the_device->access_ctr) {
      
      if (the_device->machine_var)
         the_device->machine_ref = PG_LONGWORD(size_t) GetDC((HWND)the_device->machine_var);
      else
         the_device->machine_ref = the_device->machine_ref3;
      
      dc = (HDC)the_device->machine_ref;

      if (the_device->palette) {
         
         the_device->previous_items[PREVIOUS_PALETTE] = PG_LONGWORD(size_t)SelectPalette(dc, (HPALETTE)the_device->palette, FALSE);
         RealizePalette(dc);
         the_device->palette_select = 0x02000000L;
      }
      else
         the_device->palette_select = 0;

      if (the_device->scale.scale)
         scale_dc(dc, the_device);
   }
   else
      dc = (HDC)the_device->machine_ref;

   ++the_device->access_ctr;

   return      dc;
}


/* This function does a ReleaseDC if access_ctr decrements to zero. */

PG_PASCAL (void) pgReleasePlatformDevice (graf_device_ptr the_device)
{
   if (!(--the_device->access_ctr)) {
      
      if (the_device->machine_var) {
         
         if (the_device->previous_items[PREVIOUS_FONT])
            SelectObject((HDC)the_device->machine_ref, (HANDLE)the_device->previous_items[PREVIOUS_FONT]);
         if (the_device->previous_items[PREVIOUS_PALETTE])
            SelectPalette((HDC)the_device->machine_ref, (HPALETTE)the_device->previous_items[PREVIOUS_PALETTE], FALSE);

         ReleaseDC((HWND)the_device->machine_var, (HDC)the_device->machine_ref);
         
         the_device->previous_items[PREVIOUS_FONT] = the_device->previous_items[PREVIOUS_PALETTE] = 0;
      }
   }

#ifdef PG_DEBUG
   
   if (the_device->access_ctr < 0)
      pgDebugProc(RANGE_ERR, 0);
#endif
}

/* SetFontCharWidths is a Windows-only utility that forces the character widths for a specific
style + font. Upon entry, style is the composite style (whose font is also in font_index).
The charwidths must be 256 int values. */

#ifndef UNICODE

PG_PASCAL (void) SetFontCharWidths (pg_ref pg, style_info_ptr style, int PG_FAR *charwidths)
{
   paige_rec_ptr     pg_rec;
   font_object_ptr      fonttable;
   font_info_ptr     font;
   pg_short_t        style_index;

   pg_rec = (paige_rec_ptr) UseMemory(pg);

   if ((style_index = pgFindMatchingStyle((memory_ref) pg_rec->t_formats, style, 0,
            SIGNIFICANT_STYLE_SIZE)) > 0) {
      style_info_ptr    found_style;
      
      found_style = (style_info_ptr) UseMemoryRecord(pg_rec->t_formats, (long)(style_index - 1), 0, TRUE);
      found_style->maintenance |= HAS_FORCED_WIDTHS;
      UnuseMemory(pg_rec->t_formats);
   }

   font = (font_info_ptr) UseMemoryRecord(pg_rec->fonts, (long)style->font_index, 0, TRUE);
   get_windows_font_object(pg_rec, style, font);
   
   fonttable = (font_object_ptr) UseMemoryRecord(pg_rec->globals->font_objects, style->machine_var2, 0, TRUE);
   pgBlockMove(charwidths, fonttable->widths, sizeof(int) * 256);

   fonttable->valid_widths |= CHARWIDTHS_COMPUTED;
   
   UnuseMemory(pg_rec->globals->font_objects);
   UnuseMemory(pg_rec->fonts);
   UnuseMemory(pg);
}
#endif


/* pgOpenPrinter sets up the target device (which will receive pgPrint drawings)
to map images correctly. For Windows we do several things. For Mac we don't
really do anything. */

PG_PASCAL (void) pgOpenPrinter (paige_rec_ptr pg_rec, graf_device_ptr print_dev,
      long first_position, rectangle_ptr page_rect)
{
   HDC                  screen_dc, printer_dc;
   text_block_ptr       block;
   long              height_to_paginate, height_paginated;
    short               print_x, print_y;

   screen_dc = pgGetPlatformDevice(&pg_rec->port);
   print_dev->machine_ref4 = PG_LONGWORD(size_t)screen_dc;

/* Paginate the text blocks that will print to assure all font objects
are created and the line(s) are in place: */

   block = pgFindTextBlock(pg_rec, first_position, NULL, TRUE, TRUE);
   height_to_paginate = page_rect->bot_right.v - page_rect->top_left.v;
    height_paginated = 0;
    
   while (block->end < pg_rec->t_length) {

      ++block;
      pgPaginateBlock(pg_rec, block, NULL, FALSE);
      height_paginated += (block->bounds.bot_right.v - block->bounds.top_left.v);
 
      if (height_paginated >= height_to_paginate)
         break;
   }

    UnuseMemory(pg_rec->t_blocks);

   printer_dc = (HDC)print_dev->machine_ref;
   
   if (!print_dev->resolution) {
      
      print_y = GetDeviceCaps(printer_dc, LOGPIXELSY);
      print_x = GetDeviceCaps(printer_dc, LOGPIXELSX);
      print_dev->resolution = print_x;
      print_dev->resolution <<= 16;
      print_dev->resolution |= print_y;
   }

/* Resolve the difference in printer resolution and force the document
to change scaling by that much: */
    
    print_x = HIWORD(print_dev->resolution);
   print_y = LOWORD(print_dev->resolution);
   set_printer_scaling(pg_rec, printer_dc, print_x, print_y);
}


/* pgPrintDeviceChanged gets called when pgSetPrintDevice() is called from the
application. For Windows we walk through and invalidate all the char widths.
For Mac we do nothing. */

PG_PASCAL (void) pgPrintDeviceChanged (paige_rec_ptr pg)
{
#ifndef UNICODE
   font_object_ptr         fonts;
   long              font_qty;

   fonts = (font_object_ptr) UseMemory(pg->globals->font_objects);
   
   for (font_qty = GetMemorySize(pg->globals->font_objects); font_qty; ++fonts, --font_qty)
      fonts->valid_widths |= USE_TEMP_WIDTHS;

   UnuseMemory(pg->globals->font_objects);
#endif
}


/* pgClosePrinter undoes whatever it did in pgOpenPrinter. For Mac we do nothing.
For Windows we re-invalidate the text and restore the old mapping mode. */

PG_PASCAL (void) pgClosePrinter (paige_rec_ptr pg_rec, graf_device_ptr print_dev)
{
   HDC                  printer_dc;
   
   printer_dc = (HDC)print_dev->machine_ref;
   pgReleasePlatformDevice(&pg_rec->port);

#ifndef UNICODE
   {
      font_object_ptr         fonts;
      long              font_qty;
   
      fonts = (font_object_ptr) UseMemory(pg_rec->globals->font_objects);
      
      for (font_qty = GetMemorySize(pg_rec->globals->font_objects); font_qty; ++fonts, --font_qty)
         fonts->valid_widths &= (~USE_TEMP_WIDTHS);
   
      UnuseMemory(pg_rec->globals->font_objects);
   }
#endif
}



/* HandleToMemory -- converts a Handle to a memory ref of rec_size record size. */

PG_PASCAL (memory_ref) HandleToMemory (pgm_globals_ptr mem_globals, HANDLE h,
      pg_short_t rec_size)
{
   mem_rec_ptr              mem_ptr;
   register pg_bits8_ptr    src, dest;
   size_t                   h_size, new_size, block_size;
   HANDLE                   hNew;

   h_size = GlobalSize(h);
   new_size = (h_size / rec_size);
   block_size = (new_size * rec_size) + NON_APP_SIZE;
   
   hNew = GlobalReAlloc (h, block_size, 0);
   if (hNew)
   {
      // ReAlloc good, proceed with new handle
      h = hNew;
   }
   else
   {
      // ReAlloc failed, discard handle
      // Discarding is arguable, but the memory_ref would have assumed
      // ownership of the handle so this makes some sense.
      GlobalDiscard(h);
      return MEM_NULL;
   }

    src = (pg_bits8_ptr) GlobalLock(h);
    mem_ptr = (mem_rec_ptr)src;
 
    dest = src;
    dest += sizeof(mem_rec);
    
    src += h_size;
    dest += h_size;
    pgBlockMove(src, dest, -(ptrdiff_t)h_size);
   
   mem_globals->total_unpurged += block_size;
   
   mem_ptr->rec_size = rec_size;
   mem_ptr->num_recs = mem_ptr->real_num_recs = new_size;
   mem_ptr->extend_size = 0;
   mem_ptr->access = 0;
   mem_ptr->purge = 0x08;
   mem_ptr->globals = mem_globals;
   mem_ptr->mem_id = mem_globals->current_id;

#ifdef PG_DEBUG
   {
      pg_bits8_ptr            ptr;

      ptr = (pg_bits8_ptr)mem_ptr;
      ptr += (h_size + sizeof(mem_rec));

#ifdef PG_BIG_CHECKSUMS
       {
      long               checksum_ctr;

         for (checksum_ctr = CHECKSUM_SIZE; checksum_ctr; --checksum_ctr)
            *ptr++ = BIG_CHECKSUM_BYTE;
      }
#else
      *ptr++ = CHECKSUM_BYTE1;
      *ptr++ = CHECKSUM_BYTE2;
      *ptr++ = CHECKSUM_BYTE3;
      *ptr++ = CHECKSUM_BYTE4;
#endif
   }
   
#endif
   
   GlobalUnlock(h);

   return   InitMemoryRef(mem_globals, h);
}


/* MemoryToHandle -- converts a memory_ref to a HANDLE.  */

PG_PASCAL (HANDLE) MemoryToHandle (memory_ref ref)
{
   HANDLE            result;
   pg_bits8_ptr      src, target;
   size_t            bytesize;
   
   bytesize = GetByteSize(ref);
   UseMemory(ref);         // forces a re-load
   result = (HANDLE)DetachMemory(ref);
   
   if (bytesize) {
      
      target = src = (pg_bits8_ptr) GlobalLock(result);
      src += sizeof(mem_rec);
      pgBlockMove(src, target, bytesize);
      GlobalUnlock(result);
   }

   result = GlobalReAlloc(result, bytesize, 0);
   
   return   result;
}



/* Compute if it is caret time. For PC not used for carets but is used for blinking */

PG_PASCAL (pg_boolean) pgIsCaretTime(paige_rec_ptr pg)
{
   time_t      timer, elapsed_time;
   pg_boolean  result = FALSE;

   timer = time(NULL);
   elapsed_time = timer - (time_t)pg->timer_info;
   
   if (elapsed_time >= WIN_CARET_TIME) {
      
      result = TRUE;
      blink_mode = (++blink_mode) & 1;
      pg->timer_info = (unsigned long)timer;
   }
   
   if (pg->flags & PRINT_MODE_BIT)
      blink = FALSE;
   else blink = result;
   
   return result;
}


/* Default Idle procedure - we just handle blinking character styles */

PG_PASCAL (void) pgIdleProc (paige_rec_ptr pg, short verb)
{
   style_info_ptr   info, info_1;
   size_t           i, count;
   pg_boolean       got_blinks = FALSE;
   
   if (verb == toggle_cursor)
   {
      if (!(pg->flags & DEACT_BITS) || ((pg->flags & DEACT_BITS) && (pg->flags2 & BLINK_ON_DEACTIVE)))
      {
         info = info_1 = (style_info_ptr) UseMemory (pg->t_formats);
         count =  GetMemorySize(pg->t_formats);
         
         for (i = 0; i < count; i++, info++)
         {
            if (info->styles[blink_var])
            {
               got_blinks = TRUE;
            }
         }
         
         if (got_blinks)
         {
//          pg->procs.set_device(pg, set_pg_device, &pg->port, &pg->bk_color);
//          pgClipGrafDevice(pg, MEM_NULL, TRUE, FALSE);
         
            for (i = 0; i < count; i++, info_1++)
            {
               if (info_1->styles[blink_var])
               {
                  style_run_ptr  run;
                  size_t        j, count2;
                  
                  run = (style_run_ptr) UseMemory (pg->t_style_run);
                  count2 =  GetMemorySize(pg->t_style_run);
                  
                  for (j = 0; j < count2; j++, run++)
                  {
                     if (run->style_item == (pg_short_t)i)
                     {
                        size_t beginning_offset, ending_offset;
                        
                        beginning_offset = run->offset;
                        
                        j++;
                        run++;
                        ending_offset = run->offset;
   
                        pgUpdateText (pg, NULL, beginning_offset, ending_offset, MEM_NULL, NULL, best_way, TRUE);
                     }
                  }
                  
                  UnuseMemory (pg->t_style_run);
               }
            }
            
//          pg->procs.set_device(pg, unset_pg_device, &pg->port, NULL);
         }
         
         UnuseMemory (pg->t_formats);
      }
   }
}


PG_PASCAL (pg_short_t) pgMeasureText (paige_rec_ptr pg, short measure_verb, pg_char_ptr data,
      long length, long slop, long num_spaces, pg_text_int PG_FAR *positions,
      style_walk_ptr walker)
{
	if(pg->globals->machine_specific == RUNTIME_PLATFORM_WIN32S ||
	   pg->globals->machine_specific == RUNTIME_PLATFORM_WIN16)
		return pgMeasureText16(pg, measure_verb, data, length, slop,
		                       num_spaces, positions, walker);
	else
		return pgMeasureText32(pg, measure_verb, data, length, slop,
		                       num_spaces, positions, walker);
}

/* pgMeasureText sets *positions to an array of character positions for data,
with the ending position (positions[length]) as the ending position.
The function returns the total width of the text. */

PG_PASCAL (pg_short_t) pgMeasureText32 (paige_rec_ptr pg, short measure_verb, pg_char_ptr data,
      long length, long slop, long num_spaces, pg_text_int PG_FAR *positions,
      style_walk_ptr walker)
{
   HDC                  hdc;
   register int PG_FAR     *int_positions;
   register long        ctr;
   pg_char_ptr          text;
   pg_globals_ptr       globals;
   TEXTMETRIC           tm;
   pg_fixed          text_extra, space_extra;
   pg_fixed          running_space_extra, running_text_extra;
   SIZE              extent;
   pg_byte              space_char;
   short             running_width, non_roman;
   BOOL              is_double_byte;
   UINT              code_page;
   int                  result, chars_fit, old_map_mode;

   globals = pg->globals;

   hdc = pgGetPlatformDevice(globals->current_port);
   old_map_mode = SetMapMode(hdc, MM_TEXT);
   non_roman = walker->cur_font->char_type & NON_ROMAN_FLAG;
   code_page = walker->cur_font->code_page;
   SetTextJustification(hdc, 0, 0);
    GetTextMetrics(hdc, (TEXTMETRIC far*)&tm);
    space_char = (pg_char)tm.tmBreakChar;

   positions[0] = 0;

#ifndef UNICODE
   if (walker->cur_style->maintenance & HAS_FORCED_WIDTHS) {
      font_object_ptr      fonts;
      pg_short_t        index;
      int PG_FAR        *n_widths;

      int_positions = positions;
      text = data;
      fonts = (font_object_ptr) UseMemoryRecord(globals->font_objects, walker->superimpose.machine_var2, 0, TRUE);
      n_widths = fonts->widths;

      for (running_width = 0, ctr = 0; ctr < length; ++int_positions, ++ctr) {
      
         index = *text++;
         *int_positions = running_width;
         running_width += n_widths[index];
      }
      
      UnuseMemory(globals->font_objects);
   }
   else
#endif
      GetTextExtentExPoint(hdc, (PGSTR)data, (int)length, 32767, &chars_fit, &positions[1], &extent);

   text_extra = walker->superimpose.char_extra;
   
   if (pg->flags2 & WHOLE_KERNING_BIT)
      text_extra = pgRoundFixed(text_extra) & 0xFFFF0000;

    space_extra = (pg_fixed) 0;
   
    if (slop) {
      
      if (num_spaces)
         space_extra = pgFixedRatio((short)slop, (short)num_spaces);
      else
         space_extra = pgFixedRatio((short)slop, (short)length);
    }

   int_positions = positions;
   text = data;
    running_space_extra = running_text_extra = 0;
    result = 0;
    
   if (measure_verb == measure_textout) {

      for (ctr = 0; ctr < length; ++int_positions, ++text, ++ctr) {
         
         if (non_roman && (ctr < (length - 1)))
            is_double_byte = FirstMultiCharByte(code_page, *text);
         else
            is_double_byte = FALSE;

         if (is_double_byte)
            *int_positions = int_positions[2] - int_positions[0];
         else  
            *int_positions = int_positions[1] - int_positions[0];

         if (text_extra) {

             running_text_extra += text_extra;
             running_width = (short)(HIWORD(running_text_extra));
             running_text_extra &= 0x0000FFFF;
             *int_positions += running_width;
         }

         if ((!num_spaces || (*text == space_char)) && slop) {
             
             running_space_extra += space_extra;
            if (is_double_byte)
               running_space_extra += space_extra; 
             running_width = (short)(HIWORD(running_space_extra));
             running_space_extra &= 0x0000FFFF;
             *int_positions += running_width;
         }
      
         result += *int_positions;

         if (is_double_byte) {
            int_positions[1] = int_positions[0];
            *int_positions = 0;
            int_positions += 1;
            text += 1;
            ctr += 1;
         }
      }
   }
   else {

      ctr = 0;
      
      if (text_extra || slop) {
      
         for (running_width = 0; ctr < length; ++int_positions, ++text, ++ctr) {
   
            if (non_roman && (ctr < (length - 1)))
               is_double_byte = FirstMultiCharByte(code_page, *text);
            else
               is_double_byte = FALSE;

            *int_positions += running_width;
            
            if (is_double_byte)
               int_positions[1] += running_width;

            if (text_extra) {

                running_text_extra += text_extra;
                running_width += (short)(HIWORD(running_text_extra));
                running_text_extra &= 0x0000FFFF;
            }
            
            if ((!num_spaces || (*text == (pg_char)space_char)) && slop) {
                
                running_space_extra += space_extra;
               if (is_double_byte)
                  running_space_extra += space_extra;
                running_width += (short)(HIWORD(running_space_extra));
                running_space_extra &= 0x0000FFFF;
            }

            if (is_double_byte) {

               int_positions += 1;
               ctr += 1;
               text += 1;
            }
         }
      
         *int_positions += running_width;
         result = *int_positions;
      }
   }

   SetMapMode(hdc, old_map_mode);
   pgReleasePlatformDevice(globals->current_port);
   
   return   result;
}

PG_PASCAL (pg_short_t) pgMeasureText16 (paige_rec_ptr pg, short measure_verb, pg_char_ptr data,
      long length, long slop, long num_spaces, pg_text_int PG_FAR *positions,
      style_walk_ptr walker)
{
   int                        result = 0;

#ifndef UNICODE

   register int     PG_FAR       *int_positions;
   register pg_char_ptr       text;
   register int               running_width;
   register long            ctr;
   register pg_fixed        text_extra, running_text_extra;
   pg_fixed					running_space_extra, space_extra, overhang;
   font_object_ptr          fonts;
   pg_globals_ptr           globals;
   TEXTMETRIC                    tm;
   HDC                        hdc;
   BOOL                    is_double_byte;
   pg_short_t                    index;
   UINT                    code_page;
   static int                 temp_widths[256];
   int PG_FAR                 *n_widths;
   long                    full_width;
   short                      non_roman, space_char;

   globals = pg->globals;
   
   hdc = pgGetPlatformDevice(globals->current_port);
   
   non_roman = walker->cur_font->char_type & MULTIBYTE_FLAG;
   code_page = walker->cur_font->code_page;

    GetTextMetrics(hdc, (TEXTMETRIC far*)&tm);
    overhang = (pg_fixed)tm.tmOverhang;
    overhang <<= 16;

   fonts = (font_object_ptr) UseMemoryRecord(globals->font_objects, walker->superimpose.machine_var2, 0, TRUE);
   
   n_widths = fonts->widths;
   
   if (!(fonts->valid_widths & CHARWIDTHS_COMPUTED)) {
       
      GetCharWidth(hdc, 0, 255, n_widths);
      n_widths[globals->soft_line_char] = 0;
      n_widths[globals->line_wrap_char] = 0;    
      fonts->valid_widths |= CHARWIDTHS_COMPUTED;
   }
   else
   if (fonts->valid_widths & USE_TEMP_WIDTHS) {
      
      n_widths = temp_widths;
      GetCharWidth(hdc, 0, 255, n_widths);
      n_widths[globals->soft_line_char] = 0;
      n_widths[globals->line_wrap_char] = 0;    
   }

   text_extra = walker->superimpose.char_extra;
   
   if (pg->flags2 & WHOLE_KERNING_BIT)
      text_extra = pgRoundFixed(text_extra) & 0xFFFF0000;

    space_extra = (pg_fixed) 0;
    space_char = (short)tm.tmBreakChar;
   
    if (slop) {
      
      if (num_spaces)
         space_extra = pgFixedRatio((short)slop, (short)num_spaces);
      else
         space_extra = pgFixedRatio((short)slop, (short)length);
    }

   int_positions = positions;
   text = data;
    running_space_extra = running_text_extra = 0;
    result = 0;
    
   if (measure_verb == measure_textout) {

      for (ctr = 0; ctr < length; ++int_positions, ++ctr) {
         
         if (non_roman && (ctr < (length - 1)))
            is_double_byte = FirstMultiCharByte(code_page, *text);
         else
            is_double_byte = FALSE;
         
         if (is_double_byte) {
            
            full_width = GetTextWidth(hdc, (PGSTR)text, 2);
            
            if (text_extra) {
            
               running_text_extra += text_extra;
                running_width = (short)(HIWORD(running_text_extra));
                running_text_extra &= 0x0000FFFF;
                full_width += running_width;
            }
            
            if (!num_spaces && slop) {
                
                running_space_extra += (space_extra + space_extra);
                running_width = (short)(HIWORD(running_space_extra));
                running_space_extra &= 0x0000FFFF;
                full_width += running_width;
            }
            
            *int_positions = 0;              // sdr our anti-aliasing engine expects double byte chars to have first byte 0
            int_positions[1] = (int)full_width; // sdr and the second byte contains the width of the double byte character
            ++int_positions;

            ++ctr;
            text += 2;
            result += (int)full_width;
         }
         else {

            index = *text++;
            *int_positions = n_widths[index];
            
            if (text_extra) {

                running_text_extra += text_extra;
                running_width = (short)(HIWORD(running_text_extra));
                running_text_extra &= 0x0000FFFF;
                *int_positions += running_width;
            }

            if ((!num_spaces || (index == (pg_short_t)space_char)) && slop) {
                
                running_space_extra += space_extra;
                running_width = (short)(HIWORD(running_space_extra));
                running_space_extra &= 0x0000FFFF;
                *int_positions += running_width;
            }
         
            result += *int_positions;
         }
      }
   }
   else {

      ctr = 0;
      text_extra -= overhang;

      for (running_width = 0; ctr < length; ++int_positions, ++ctr) {

         if (non_roman && (ctr < (length - 1)))
            is_double_byte = FirstMultiCharByte(code_page, *text);
         else
            is_double_byte = FALSE;
         
         if (is_double_byte) {
            int            full_width;

            *int_positions++ = running_width;
            full_width = GetTextWidth(hdc, (LPSTR)text, 2);
            
            //text_extra += overhang;

            if (text_extra) {

                running_text_extra += (text_extra);
                full_width += (short)(HIWORD(running_text_extra));
                running_text_extra &= 0x0000FFFF;
            }
            
            //text_extra -= overhang;

            if (!num_spaces && slop) {
                
                running_space_extra += (space_extra + space_extra);
                full_width += (short)(HIWORD(running_space_extra));
                running_space_extra &= 0x0000FFFF;
            }

            running_width += full_width;
            *int_positions = running_width - (full_width / 2);

            ++ctr;
            text += 2;
         }
         else {
         
            index = *text++;
            *int_positions = running_width;
            running_width += n_widths[index];
            
            if (text_extra) {

                running_text_extra += text_extra;
                running_width += (short)(HIWORD(running_text_extra));
                running_text_extra &= 0x0000FFFF;
            }
            
            if ((!num_spaces || (index == (pg_short_t)space_char)) && slop) {
                
                running_space_extra += space_extra;
                running_width += (short)(HIWORD(running_space_extra));
                running_space_extra &= 0x0000FFFF;
            }
         }
      }
   
      *int_positions = result = running_width;
   }

   pgReleasePlatformDevice(globals->current_port);

   UnuseMemory(globals->font_objects);
#endif
   return   result;
}

#ifdef PG_DEBUG
PG_PASCAL(void)pgDebugOut(pg_char_ptr str)
{
	OutputDebugString((LPCTSTR)str);
}
#endif

/********************************** LOCAL FUNCTIONS ***************************/

/* enum_pgfont_proc is the function we use to verify the existence of a font AND to initialize
the miscellaneious required info. */

int CALLBACK enum_pgfont_proc (LOGFONT far *log, TEXTMETRIC far *tm, int font_type, LPARAM pg_font)
{
   font_info_ptr        user_font;

   user_font = (font_info_ptr)pg_font;

    if (!user_font->machine_var[PG_OUT_PRECISION])
      user_font->machine_var[PG_OUT_PRECISION] = log->lfOutPrecision;
    if (!user_font->machine_var[PG_CLIP_PRECISION])
      user_font->machine_var[PG_CLIP_PRECISION] = log->lfClipPrecision;
    if (!user_font->machine_var[PG_QUALITY])
      user_font->machine_var[PG_QUALITY] = log->lfQuality;
    if (!user_font->machine_var[PG_CHARSET])
      user_font->machine_var[PG_CHARSET] = DEFAULT_CHARSET;
//      user_font->machine_var[PG_CHARSET] = log->lfCharSet;

   return   FONT_EXISTS;
}



/* This initializes the four-byte global "char" (note that the 4 bytes are
already initialized to zeros). This function is used for DEFAULTS, which assumes
single byte values! */

static void init_global_char(pg_char_ptr global_char, pg_short_t value)
{
#ifdef GLOBALS_ARE_PSTRINGS
   global_char[0] = 1;
   global_char[1] = (pg_char)value;
#else                               
   global_char[0] = (pg_char)value;
#endif
}


/* This returns a 16-bit value from a global char string */

static pg_short_t global_char_to_int (pg_char_ptr global_char)
{
   register pg_short_t       result_16;

#ifdef GLOBALS_ARE_PSTRINGS

   result_16 = global_char[1];
   if (global_char[0] > 1) {
      
      result_16 <<= 8;
      result_16 |= global_char[2];
   }

#else
   
   result_16 = global_char[0];
   if (global_char[1]) {
      result_16 <<= 8;
      result_16 |= global_char[1];
   }

#endif

   return  result_16;
}


/* This returns the size of the global char (either 0, 1 or 2). */

static short global_char_size (pg_char_ptr global_char)
{
#ifdef GLOBALS_ARE_PSTRINGS
   return  global_char[0];
#else
   if (!global_char[0])
      return  0;
   if (!global_char[1])
      return  1;
   return  2;
#endif
}


/* This functions must build a graf_device record based on the single device
pointer in *port */

static void make_graf_device (pg_globals_ptr globals, generic_var port, generic_var machine_ref,
      graf_device_ptr device)
{
   long           y_axis, x_axis;
   HDC               hdc;

   pgFillBlock(device, sizeof(graf_device), 0);
   device->machine_ref3 = globals->machine_const;  // = default DC

   device->machine_var = port;
   
   if ((device->machine_ref = machine_ref) != 0)
      ++device->access_ctr;   /* This assumes HDC already valid. */
   
   device->graf_stack = MemoryAlloc(globals->mem_globals,
         sizeof(port_preserve), 0, 4);
   SetMemoryPurge(device->graf_stack, NO_PURGING_STATUS, FALSE);
   device->bk_color = globals->def_bk_color;
   
   if (device->machine_var || device->machine_ref) {

      hdc = pgGetPlatformDevice(device);
      y_axis = (long)GetDeviceCaps(hdc, LOGPIXELSY);
      x_axis = (long)GetDeviceCaps(hdc, LOGPIXELSX);
      pgReleasePlatformDevice(device);
      
      device->resolution = (y_axis << 16) | x_axis;
   }
   else
   {
      hdc = CreateDC(DISPLAY_DRIVER_NAME, NULL, NULL, NULL);

      y_axis = (long)GetDeviceCaps(hdc, LOGPIXELSY);
      x_axis = (long)GetDeviceCaps(hdc, LOGPIXELSX);
      DeleteDC(hdc);

      device->resolution = (y_axis << 16) | x_axis;
   }

   device->clip_rgn = pgCreateRgn();
   device->clip_info.change_flags = -1;
}


/* This function draws the caret in XOR mode */

static void draw_cursor (paige_rec_ptr pg, t_select_ptr select, short verb)
{
   point_start_ptr      starts;
   style_info_ptr       caret_style;
   text_block_ptr       block;
   rectangle            wrap_rect;
   co_ordinate          caret_top, caret_bot, scroll_adjust, repeat_offset;
   Point                top_caret, bottom_caret;
   size_t               style_offset, r_num;
   long                 container_proc_refcon, caret_height;
   short                top_height, bottom_height;

   if (select->flags & SELECTION_DIRTY)
      pgCalcSelect(pg, select);

   block = pgFindTextBlock(pg, select->offset, NULL, TRUE, TRUE);
   starts = (point_start_ptr) UseMemoryRecord(block->lines, select->line, USE_ALL_RECS, TRUE);
   
   container_proc_refcon = 0;
   scroll_adjust = pg->scroll_pos;
   pgNegatePt(&scroll_adjust);
   pgAddPt(&pg->port.origin, &scroll_adjust);
   r_num = starts->r_num;

   pgCallContainerProc(pg, (pg_short_t)r_num, &scroll_adjust, clip_container_verb,
          &pg->port.scale, &container_proc_refcon); 

   if (style_offset = select->offset)
      --style_offset;

   caret_style = pgFindTextStyle(pg, style_offset);

   caret_top.h = starts->bounds.top_left.h + select->primary_caret;
   caret_height = caret_style->ascent + caret_style->descent;
   caret_top.v = starts->bounds.bot_right.v - starts->baseline - caret_style->ascent;
   caret_bot.h = starts->bounds.top_left.h + select->secondary_caret;
   caret_bot.v = caret_top.v;
   
   caret_top.h -= 1;
   caret_bot.h -= 1;

   GetMemoryRecord(pg->wrap_area, pgGetWrapRect(pg, starts->r_num, &repeat_offset) + 1, &wrap_rect);
   pgOffsetRect(&wrap_rect, repeat_offset.h, 0);
   
   if (caret_top.h >= wrap_rect.bot_right.h)
      caret_top.h = wrap_rect.bot_right.h - 1;
   if (caret_bot.h >= wrap_rect.bot_right.h)
      caret_bot.h = wrap_rect.bot_right.h - 1;

   if (caret_top.h < wrap_rect.top_left.h)
      caret_top.h = wrap_rect.top_left.h;
   if (caret_bot.h < wrap_rect.top_left.h)
      caret_bot.h = wrap_rect.top_left.h;

   UnuseMemory(pg->t_formats);

   pgSubPt(&pg->scroll_pos, &caret_top);
   pgAddPt(&pg->port.origin, &caret_top);
   pgSubPt(&pg->scroll_pos, &caret_bot);
   pgAddPt(&pg->port.origin, &caret_bot);

   pgScalePt(&pg->port.scale, NULL, &caret_top);
   pgScalePt(&pg->port.scale, NULL, &caret_bot);

   pgScaleLong(pg->port.scale.scale, 0, &caret_height);

   top_height = (short)caret_height;
   top_height /= 2;
   bottom_height = (short)(caret_height - top_height - 1);

   top_caret.h = pgLongToShort(caret_top.h);
   top_caret.v = pgLongToShort(caret_top.v);

   bottom_caret.h = pgLongToShort(caret_bot.h);
   bottom_caret.v = pgLongToShort(caret_bot.v);
   bottom_caret.v += (top_height + 1);

   UnuseMemory(block->lines);
   UnuseMemory(pg->t_blocks);

    top_caret.v = (short)((bottom_caret.v + bottom_height) - caret_height);
   
   switch (verb) 
   {   
      case toggle_cursor:       
      case toggle_cursor_idle:
      case restore_cursor:
      case show_cursor:
      case activate_cursor:
         if (!(pg->flags & CARET_BIT)) {
         
            if (create_caret(pg, (short)top_caret.h, (short)top_caret.v, (short)caret_height)) {
            
               SetCaretPos(top_caret.h - (int)pg->window_origin.h, top_caret.v - (int)pg->window_origin.v);     
               ShowCaret((HWND)pg->port.machine_var);
               pg->flags |= CARET_BIT;
            }  
         }

         break;
   
      case deactivate_cursor:
      case hide_cursor:
         if (pg->flags & CARET_CREATED_BIT)
            DestroyCaret();
         
         pg->flags &= (long)(~(CARET_BIT | CARET_CREATED_BIT));

         break;
      
      case update_cursor:
         // do nothing
         break;
      
      case compute_cursor:
         if (!(pg->stable_caret.h = top_caret.h))
            ++pg->stable_caret.h;

         pg->stable_caret.v = top_caret.v + 1;
         
         break;
   }

   pgCallContainerProc(pg, (pg_short_t)r_num, (co_ordinate_ptr)NULL, unclip_container_verb, (pg_scale_ptr)NULL, &container_proc_refcon); 
}



/* This is the machine-dependent function to set all text fonts & styles. The
window defined in globals->current_port is affected.
Windows note: The font object is in style->machine_var, or it is created if
style->machine_var is zero.  */

static void install_machine_font (paige_rec_ptr pg, style_info_ptr style,
      font_info_ptr font, pg_boolean include_offscreen)
{
   register style_info_ptr the_style = style;
   pg_globals_ptr       globals;
   HDC                  hdc;
   COLORREF          text_color;
   HANDLE               previous_font;

   globals = pg->globals;
   hdc = pgGetPlatformDevice(globals->current_port);

// Fix MM reported:

   previous_font = SelectObject(hdc, (HANDLE) the_style->machine_var);
   
   if (globals->current_port->previous_items[PREVIOUS_FONT] == 0)
      globals->current_port->previous_items[PREVIOUS_FONT] = PG_LONGWORD(size_t)previous_font;

   pgColorToOS(&the_style->fg_color, &text_color);
   text_color |= globals->current_port->palette_select;
   SetTextColor(hdc, text_color);
   SetTextJustification(hdc, 0, 0);
 
   pgReleasePlatformDevice(globals->current_port);
}


/* scale_dc scales the device context to match the PAIGE scaling factor. */

static void scale_dc (HDC dc, graf_device_ptr the_device)
{
// long        resolution, scaled_resolution;
         
// resolution = GetDeviceCaps(dc, LOGPIXELSX) * 100; // CLM

// scaled_resolution = resolution;
// pgScaleLong(the_device->scale.scale, 0, &resolution); // CLM

// SetMapMode(dc, MM_ISOTROPIC);
   
// SetWindowExtEx(dc, (short)scaled_resolution, (short)scaled_resolution, NULL); // CLM
// SetViewportExtEx(dc, (short)resolution, (short)resolution, NULL); // CLM

//    resolution = GetDeviceCaps(dc, LOGPIXELSX);

   SetMapMode(dc, MM_ISOTROPIC);
   SetWindowExtEx(dc, LOWORD(the_device->scale.scale), LOWORD(the_device->scale.scale), NULL);
   SetViewportExtEx(dc, HIWORD(the_device->scale.scale), HIWORD(the_device->scale.scale), NULL);
}


/* get_unscaled_dc returns the DC forced to unscaled mode. */

static HDC get_unscaled_dc (paige_rec_ptr pg, short PG_FAR *index)
{
   HDC         result;
   
   if (pg->port.access_ctr)
      *index = SaveDC((HDC)pg->port.machine_ref);
   else
      *index = 0;
   
   result = pgGetPlatformDevice(&pg->port);
   unset_printer_scaling(result);
   
   return      result;
}


/* restore_scaled_dc returns the DC to its original state, after
calling get_unscaled_dc. */

static void restore_scaled_dc (paige_rec_ptr pg, short index)
{
   pgReleasePlatformDevice(&pg->port);
   
   if (pg->port.access_ctr)
      RestoreDC((HDC)pg->port.machine_ref, index);
}

         
/* This draws special symbol the_char unless the_char is zero.  */

static void draw_special_char (paige_rec_ptr pg, pg_char_ptr the_char, co_ordinate_ptr point)
{
   if (the_char) {

#ifndef GLOBALS_ARE_PSTRINGS
      TextOut((HDC)pg->globals->current_port->machine_ref, (short) point->h, (short) point->v,
            (PGSTR) the_char, global_char_size(the_char));
#else
        TextOut((HDC)pg->globals->current_port->machine_ref, (short) point->h, (short) point->v,
            (PGSTR) &the_char[1], global_char_size(the_char));
#endif

   }
}



/* This sets the invisible char style, also scales point size if necessary.
The function returns the ascent of the new font. */

static short set_invisible_symbol_style (paige_rec_ptr pg)
{
   HDC                  hdc;
   TEXTMETRIC           tm;
   style_info              the_style;
   font_info               the_font;
   pg_boolean              offscreen_included;
   long              invis_pointsize;
   
   invis_pointsize = SPECIAL_SYMBOL_TXSIZE;
   pgScaleLong(pg->scale_factor.scale, 0, &invis_pointsize);

   offscreen_included = (pg->globals->offscreen_port.machine_ref != 0);

   pgBlockMove(&pg->globals->def_style, &the_style, sizeof(style_info));
   pgBlockMove(&pg->globals->def_font, &the_font, sizeof(font_info));
   the_style.point = (invis_pointsize << 16);
         
   if (!pg->globals->invis_font) {
   
       the_style.procs.init(pg, &the_style, &the_font);
        get_windows_font_object(pg, &the_style, &the_font);
        pg->globals->invis_font = the_style.machine_var;
    }

   the_style.machine_var = pg->globals->invis_font;
   the_style.procs.install(pg, &the_style, &the_font, NULL, 0, offscreen_included);
    
    hdc = pgGetPlatformDevice(&pg->port);
    GetTextMetrics(hdc, &tm);
    pgReleasePlatformDevice(&pg->port);
    
    return  (short)tm.tmAscent;
}



/* draw_box_style draws a frame around the current line. It will not draw the
left edge if the previous style is also boxed (and we are not at beginning of
a line), nor will it draw the right side if next style is also boxed (and we
are not at end of line). */


static void draw_box_style (paige_rec_ptr pg, style_walk_ptr walker,
      draw_points_ptr draw_position)
{
   register style_info_ptr  style_base;
   rectangle                box_frame;
   Rect                     frame;
   point_start_ptr          line_start;
   pg_short_t               style_index;
   pg_boolean               draw_edge;
   long                     line_width, descent, next_offset;
   size_t                   original_walker;
   HDC                      hdc;
   
   hdc = (HDC)pg->globals->current_port->machine_ref;
   SelectObject(hdc, GetStockBrush(BLACK_BRUSH));

   line_start = draw_position->starts;
   style_base = walker->style_base;
   original_walker = walker->current_offset;
   next_offset = line_start->offset;
   next_offset += draw_position->block->begin;
   pgSetWalkStyle(walker, next_offset);

   descent = pgGetLineRealDescent(draw_position->block, line_start, walker);
   pgScaleLong(pg->scale_factor.scale, 0, &descent);

   box_frame.top_left = box_frame.bot_right = draw_position->from;
   box_frame.top_left.v -= draw_position->ascent;
   box_frame.bot_right.v += descent;
   line_width = line_start->bounds.bot_right.h - line_start->bounds.top_left.h;
   pgScaleLong(pg->scale_factor.scale, 0, &line_width);
   box_frame.bot_right.h = box_frame.top_left.h + line_width;
   
   RectangleToRect(&box_frame, NULL, &frame);

   MoveToEx(hdc, frame.left, frame.top, NULL);
   LineTo(hdc, frame.right, frame.top);
   MoveToEx(hdc, frame.left, frame.bottom - 1, NULL);
   LineTo(hdc, frame.right, frame.bottom - 1);

   draw_edge = TRUE;

   if (!(line_start->flags & NEW_LINE_BIT))
      if (walker->current_offset == walker->prev_style_run->offset) {
      
      if (walker->prev_style_run->offset) {
         
         --walker->prev_style_run;
         style_index = walker->prev_style_run->style_item;
         ++walker->prev_style_run;
         
         draw_edge = (style_base[style_index].styles[boxed_var] == 0);
      }
   }

   if (draw_edge) {

      MoveToEx(hdc, frame.left, frame.top, NULL);
      LineTo(hdc, frame.left, frame.bottom - 1);
   }
   
   draw_edge = TRUE;

   next_offset = line_start[1].offset;
   next_offset += draw_position->block->begin;

   if (!(line_start->flags & LINE_BREAK_BIT))
      if (walker->next_style_run->offset == next_offset) {


   if (!(line_start->flags & LINE_BREAK_BIT))
      if (walker->next_style_run->offset == next_offset) {
         
         style_index = walker->next_style_run->style_item;
         draw_edge = (style_base[style_index].styles[boxed_var] == 0);
      }
   }

   if (draw_edge) {

      MoveToEx(hdc, frame.right, frame.top, NULL);
      LineTo(hdc, frame.right, frame.bottom - 1);
   }

   pgSetWalkStyle(walker, original_walker);
}



/* This function sets a default font name if the name is null(s)  */

static void init_def_font_name (paige_rec_ptr pg, font_info_ptr font)
{
   HDC           hdc;
   
   hdc = pgGetPlatformDevice(&pg->port);
   GetTextFace(hdc, FONT_SIZE - 2, &font->name[1]);
   font->name[0] = (pg_char)pgCStrLength((pg_c_string_ptr)&font->name[1]);

   pgReleasePlatformDevice(&pg->port);
}


/* set_alternate_pointsize changes the current font based on a different
resolution (in new_resolution). */

static void set_alternate_pointsize (paige_rec_ptr pg, long new_resolution,
      style_walk_ptr walker)
{
   long        old_resolution;
    
    old_resolution = pg->resolution;
    pg->resolution = new_resolution << 16;
    pg->resolution |= new_resolution;
   get_windows_font_object(pg, walker->cur_style, walker->cur_font);
   walker->cur_style->procs.install(pg, walker->cur_style, walker->cur_font,
         &walker->superimpose, walker->style_overlay, TRUE);
   pg->resolution = old_resolution;
}


/* pgMultiplyFixed multiplies two fixed decimal numbers (a fixed decimal is a

long whose high-order word is the integer and low-order word the fraction. Hence,
0x00018000 = 1.5).   */

PG_PASCAL (pg_fixed) pgMultiplyFixed (pg_fixed fix1, pg_fixed fix2)
{
   pg_fixed arg1, arg2;
   pg_boolean  negate;
   long     lo1, hi1, lo2, hi2, product;
   
   if (!fix1 || !fix2)
      return   (pg_fixed)0;

   negate = FALSE;

   if ((arg1 = (long)fix1) < 0) {
   
      arg1 = (long)-fix1;
      negate ^= 1;
   }
   
   if ((arg2 = (long)fix2) < 0) {
   
      arg2 = (long)-fix2;
      negate ^= 1;
   }
      
   lo1 = (long)arg1 & 0xFFFF;
   hi1 = (long)arg1 >> 16;
   lo2 = (long)arg2 & 0xFFFF;
   hi2 = (long)arg2 >> 16;
   
   product = ((unsigned long) (((lo1 * lo2) + 0x8000)) >> 16) + (lo1 * hi2) + (hi1 * lo2) + ((hi1 * hi2) << 16);
   
   if (negate)
      product = -product;

   return (pg_fixed)product;
}


/* pgDivideFixed divides fixed number fix1 into fix2 (a fixed decimal is a

long whose high-order word is the integer and low-order word the fraction. Hence,
0x00018000 = 1.5).   */

PG_PASCAL (pg_fixed) pgDivideFixed (pg_fixed fix1, pg_fixed fix2)
{
   long    fix_long;
   
   if (!fix2)
      return   0;

   // CLM begin

   if ((fix1 > 0 && (fix1 & 0xFFFF0000) != 0) || (fix1 < 0 && (fix1 & 0xFFFF8000) != 0))
      fix_long = (long)(((float) fix1 * 65536.) / fix2);
   else
   if (fix1 > 0 && fix2 > 0)
      fix_long = (long) (((unsigned long) (fix1 << 16)) / (unsigned long) fix2);
   else
      fix_long = (fix1 << 16) / fix2;
   // CLM end

   return fix_long;
}


/* pgFixedRatio returns a fixed number which is the ratio of n / d (a fixed decimal is a

long whose high-order word is the integer and low-order word the fraction. Hence,
0x00018000 = 1.5).   */

PG_PASCAL (pg_fixed) pgFixedRatio (short n, short d)
{
   long      result;

   result = (long) n;
   
   result = ((pg_fixed) (result << 16) / d);      
   return  result;
}


/* get_windows_font_object first checks to see if a matching font object
exists in globals and, if so, it is returned. If it does not exist, one is created.
Note, this only gets called at style initialization time. */

static void get_windows_font_object (paige_rec_ptr pg, style_info_ptr the_style, font_info_ptr font)
{
   pg_globals_ptr   globals;
   LOGFONT          log_font;
   font_object_ptr  available_fonts;
   long             pointsize, resolution, widthsize, font_ctr;
   size_t           num_fonts;
    
    globals = pg->globals;
   pgFillBlock(&log_font, sizeof(LOGFONT), 0);
   
   pointsize = pgComputePointSize(pg, the_style);
   widthsize = the_style->char_width >> 16;
   resolution = pg->resolution >> 16;  // Document base resolution

   log_font.lfHeight =  -MulDiv((int)pointsize, (int)resolution, 72);

   log_font.lfWidth = (short)widthsize;
   
   if (the_style->styles[bold_var])
      log_font.lfWeight = FW_BOLD;
   else
      log_font.lfWeight = FW_NORMAL;

    if (the_style->styles[italic_var])
      log_font.lfItalic = 1;
   if (the_style->styles[underline_var])
   log_font.lfUnderline = 1;
    if (the_style->styles[strikeout_var])
   log_font.lfStrikeOut = 1;

    log_font.lfOutPrecision = (BYTE)font->machine_var[PG_OUT_PRECISION];
    log_font.lfClipPrecision = (BYTE)font->machine_var[PG_CLIP_PRECISION];
    log_font.lfQuality = (BYTE)font->machine_var[PG_QUALITY];
    if (!(log_font.lfPitchAndFamily = (BYTE)font->family_id))
      log_font.lfPitchAndFamily = FF_DONTCARE | DEFAULT_PITCH;
   log_font.lfCharSet = (BYTE)font->machine_var[PG_CHARSET];
   
   if (font->environs & FONT_USES_ALTERNATE)
      lstrcpy(log_font.lfFaceName, &font->alternate_name[1]);
   else
      lstrcpy(log_font.lfFaceName, &font->name[1]);

 // See if the font exists:
 
   num_fonts = GetMemorySize(globals->font_objects);
   available_fonts = (font_object_ptr) UseMemory(globals->font_objects);
   
   for (font_ctr = 0; font_ctr < num_fonts; ++font_ctr) {
      
      if (pgEqualStruct(&log_font, &available_fonts->log, sizeof(LOGFONT))) {
         
         the_style->machine_var = available_fonts->the_font;
         the_style->machine_var2 = font_ctr;
         UnuseMemory(globals->font_objects);
         
         return;
      }
 
      ++available_fonts;
   }
   
   UnuseMemory(globals->font_objects);
 
   the_style->machine_var = PG_LONGWORD(size_t) CreateFontIndirect(&log_font);
   the_style->machine_var2 = GetMemorySize(globals->font_objects);
   
   available_fonts = (font_object_ptr) AppendMemory(globals->font_objects, 1, TRUE);
   available_fonts->the_font = the_style->machine_var;
   available_fonts->style_ptsize = pointsize;
   pgBlockMove(&log_font, &available_fonts->log, sizeof(LOGFONT));
   
   UnuseMemory(globals->font_objects);
}


/* windows_pointsize returns what the pointsize should be based on resolution */

static pg_fixed windows_pointsize (pg_fixed ptsize, int resolution)
{
   pg_fixed point_ratio;

   if (point_ratio = pgFixedRatio((short)resolution, (short)72))
      return  pgMultiplyFixed(point_ratio, ptsize);
   
   return  ptsize;
}


/* windows_screensize returns the opposite of windows_pointsize */

static pg_fixed windows_screensize (pg_fixed screensize, int resolution)
{
   pg_fixed point_ratio;

   if (!resolution)
      return  screensize;

   if (point_ratio = pgFixedRatio((short)72, (short)resolution))
      return  pgMultiplyFixed(point_ratio, screensize);
   
   return  screensize;
}


/* pgSystemTextDirection must return the default writing justification, which
should be one of the enums for direction (except system_direction). */

PG_PASCAL (short) pgSystemDirection (pg_globals_ptr globals)
{
#ifdef WIN_MULTILINGUAL
   WORD  sys_lang;

   if ((sys_lang = GetSystemDefaultLangID()) == LANG_ARABIC || sys_lang == LANG_HEBREW)
      return  right_left_direction;
#endif
   return  left_right_direction;
}


/* create_caret creates a new caret if not created already, then
returns TRUE if a caret was indeed created. Initially, the caret is
not shown.  */

static pg_boolean create_caret (paige_rec_ptr pg, short h_loc, short v_loc, short caret_size)
{
   rectangle_ptr     vis_ptr;
   long           top_loc, bottom_loc, left_loc, right_loc;

   if (pg->port.caret_info != (long) caret_size) {
      
      if (pg->flags & CARET_CREATED_BIT)
         DestroyCaret();
      
      pg->flags &= (long) (~(CARET_BIT | CARET_CREATED_BIT));
   }
   
    vis_ptr = (rectangle_ptr) UseMemory(pg->vis_area);
    left_loc = right_loc = h_loc - pg->port.origin.h;
    ++right_loc;
    top_loc = bottom_loc = v_loc - pg->port.origin.v;
    bottom_loc += caret_size;
   
   if (!(pg->flags & (long)CARET_CREATED_BIT)) {
   
      CreateCaret((HWND)pg->port.machine_var, (HBITMAP)MEM_NULL,
            pg->doc_info.caret_width_extra + 1, (int)caret_size);
      pg->flags |= CARET_CREATED_BIT;
      pg->flags &= (~INVALID_CURSOR_BIT);
   }

   pg->port.caret_info = caret_size;
   
   UnuseMemory(pg->vis_area);

   return   TRUE;
}


/* build_background_brush creates a BRUSH object for erasing the background. */

static HBRUSH build_background_brush (graf_device_ptr device)
{
   LOGBRUSH    brush;
   
   brush.lbStyle = BS_SOLID;
   pgColorToOS(&device->bk_color, &brush.lbColor);
   brush.lbColor |= device->palette_select;
   brush.lbHatch = 0;

   return   CreateBrushIndirect(&brush);
}


/* set_space_extra sets the "justify" value of the text. If space_char is a null
char then the break character is determined. */

static int get_space_extra (HDC hdc, pg_char_ptr text, short text_length, pg_char space_char)
{
   register pg_char_ptr   data;
   register short      t_length, num_brks;
   register pg_char       brk_char;
   TEXTMETRIC             metrics;
   
   if (!(brk_char = space_char)) {
      GetTextMetrics(hdc, &metrics);
      brk_char = (pg_char) metrics.tmBreakChar;
    }
    
   data = text;
   
   for (t_length = num_brks = 0; t_length < text_length; ++t_length)
       if (data[t_length] == brk_char)
      ++num_brks;
    
    return      (int) num_brks;
}


/* set_printer_scaling sets up the correct mode in the printer DC. If use_x
and use_y are non-zero we use those values for the printer DC resolution. */

static void set_printer_scaling (paige_rec_ptr pg, HDC printer_dc, short use_x, short use_y)
{
   short          print_resolution_x, screen_resolution_x;
   short          print_resolution_y, screen_resolution_y;

   if (!(print_resolution_x = use_x))
      print_resolution_x = GetDeviceCaps(printer_dc, LOGPIXELSX);
   if (!(print_resolution_y = use_y))
      print_resolution_y = GetDeviceCaps(printer_dc, LOGPIXELSY);

   screen_resolution_x = HIWORD(pg->resolution);
   screen_resolution_y = LOWORD(pg->resolution);

   SetMapMode(printer_dc, MM_ISOTROPIC);
   SetWindowExtEx(printer_dc, screen_resolution_x, screen_resolution_y, NULL);
   SetViewportExtEx(printer_dc, print_resolution_x, print_resolution_y, NULL);
}


/* unset_printer_scaling restores normal mode in the printer_dc. */

static void unset_printer_scaling (HDC printer_dc)
{
   SetMapMode(printer_dc, MM_TEXT);
   SetWindowExtEx(printer_dc, 1, 1, NULL);
   SetViewportExtEx(printer_dc, 1, 1, NULL);
}


// Win32 & NT specific

#ifdef WIN32_COMPILE
short GetTextWidth (HDC hdc, PGSTR str, int length)
{
   SIZE        lpSize;

   GetTextExtentPoint32(hdc, str, (int)length, &lpSize);
   return   (short)lpSize.cx;
}
#endif

/* pgGetCharWidth returns the width of a character. */

PG_PASCAL (short) pgGetCharWidth (paige_rec_ptr pg_rec, style_info_ptr style, pg_char the_char)
{
	HDC				hdc;
	HANDLE			previous_font;
	short			width;

	hdc = (HDC)pg_rec->globals->machine_const;
	previous_font = SelectObject(hdc, (HANDLE) style->machine_var);
	width = GetTextWidth(hdc, (PGSTR)" ", 1);
	SelectObject(hdc, previous_font);

	return		width;
}



/* This function returns TRUE if the character at the given offset is the last
of a particular World script run. The offset param is a global (absolute) offset. */

static pg_boolean last_char_of_script (style_walk_ptr walker, long offset)
{
   register style_run_ptr    run;
   register style_info_ptr  info;
   short              font_index;

   if (!offset)
      return  FALSE;

   run = walker->next_style_run;
   if (!run)
      return FALSE;

   if ((offset + (long)1) == run->offset) {

      info = walker->style_base;
      info += run->style_item;
      if ((font_index = info->font_index) == DEFAULT_FONT_INDEX)
         font_index = 0;

      return  (pg_boolean) ((walker->cur_font->code_page != walker->font_base[font_index].code_page)
            || (walker->cur_font->machine_var[PG_CHARSET] != walker->font_base[font_index].machine_var[PG_CHARSET])
            || (walker->cur_font->language != walker->font_base[font_index].language) );
   }

   return  FALSE;
}


/* set_non_roman_bits sets all the appropriate bits for the character in question. For performance,
if known_leadbyte is TRUE then we do not need to go backwards to compute the bytecount per char.
The ctype_3 param gets set to the result of CTYPE_3 query. */

static long set_non_roman_bits (paige_rec_ptr pg, style_walk_ptr walker, pg_char_ptr data,
      long offset_begin, long offset_end, long char_offset, long mask_bits,
      WORD PG_FAR *ctype_3, pg_short_t PG_FAR *symbol_test, pg_boolean known_leadbyte)
{
   LCID        lcid;
   WORD        type3, types[4];
   pg_short_t     symbol_check = 0;
   long        result = 0;
   
#ifdef UNICODE
   pgFillBlock(types, sizeof(WORD) * 4, 0);

   symbol_check = data[char_offset];

   lcid = (LCID)walker->cur_font->language;

   if (mask_bits & (LONG_FORM_INFO | BLANK_BIT)) {
      
      GetStringTypeEx(lcid, CT_CTYPE1, (PGSTR)&data[char_offset], 1, types);

      if (types[0] & C1_BLANK)
         result |= BLANK_BIT;

      if (mask_bits & LONG_FORM_INFO) {

         if (types[0] & C1_PUNCT)
            result |= PUNCT_NORMAL_BIT;
         if (types[0] & C1_UPPER)
            result |= UPPER_CASE_BIT;
         if (types[0] & C1_LOWER)
            result |= LOWER_CASE_BIT;
         if (types[0] & C1_DIGIT)
            result |= NUMBER_BIT;
      }
   }
   
   if (ctype_3)
      type3 = pg->procs.charclass_proc(pg, &data[char_offset], 1,
            walker->cur_style, walker->cur_font);

   if (symbol_test)
      *symbol_test = symbol_check;
   if (ctype_3)
      *ctype_3 = type3;

   return      result;
#else
   UINT        code_page;

   pgFillBlock(types, sizeof(WORD) * 4, 0);

      code_page = walker->cur_font->code_page;     /* get font codepage */
   result |= multibyte_character_flags(code_page, data, offset_begin, char_offset, known_leadbyte);
   
   if (symbol_test)
      symbol_check = *symbol_test;
   
   type3 = 0;

   if ((result & (FIRST_HALF_BIT | LAST_HALF_BIT)) != 0) {
      long           first_byte;
      int               byte_count;
      pg_short_t        half_char;
      
      lcid = (LCID)walker->cur_font->language;
      first_byte = char_offset;
      byte_count = 1;

      if (result & LAST_HALF_BIT) {
         
         if (offset_begin < char_offset) {
            
            first_byte = char_offset - 1;
            byte_count += 1;
            half_char = (pg_short_t)data[first_byte];
            half_char <<= 8;
            symbol_check |= half_char;
         }
         else
            symbol_check = 0;
      }
      else {
         
         if (offset_end > (char_offset + 1)) {
         
            byte_count += 1;
            symbol_check <<= 8;
            symbol_check |= (pg_short_t)data[char_offset];
         }
      }
      
      if (mask_bits & (LONG_FORM_INFO | BLANK_BIT)) {
         
         GetStringTypeEx(lcid, CT_CTYPE1, (LPCSTR)&data[first_byte], byte_count, types);

         if (types[0] & C1_BLANK)
            result |= BLANK_BIT;

         if (mask_bits & LONG_FORM_INFO) {

            if (types[0] & C1_PUNCT)
               result |= PUNCT_NORMAL_BIT;
            if (types[0] & C1_UPPER)
               result |= UPPER_CASE_BIT;
            if (types[0] & C1_LOWER)
               result |= LOWER_CASE_BIT;
            if (types[0] & C1_DIGIT)
               result |= NUMBER_BIT;
         }
      }
      
      if (ctype_3)
         type3 = pg->procs.charclass_proc(pg, &data[first_byte], (short)byte_count,
               walker->cur_style, walker->cur_font);
   }

   if (symbol_test)
      *symbol_test = symbol_check;
   if (ctype_3)
      *ctype_3 = type3;

   return   result;
#endif
}



/* Initializes Paige font info in a WIndows environment */

static void init_font_record (paige_rec_ptr pg, font_info_ptr font)
{

   HDC                  hdc;
   pg_globals_ptr       globals;
   style_info           the_style;
   HANDLE               previous_font;
   short             char_type;

   globals = pg->globals;
   
   the_style = globals->def_style;
   the_style.procs.init(pg, &the_style, font);
   
   get_windows_font_object(pg, &the_style, font);

   hdc = pgGetPlatformDevice((graf_device_ptr)&pg->port);
   previous_font = SelectObject(hdc, (HANDLE) the_style.machine_var);
   
#ifdef WIN_MULTILINGUAL 
#ifdef WIN16_COMPILE
   font->code_page = GetKBCodePage();
#else
   font->code_page = GetACP();
#endif
   {
      font_object_ptr               fonts;
   
      fonts = (font_object_ptr) UseMemoryRecord(globals->font_objects, the_style.machine_var2, 0, TRUE);
      font->char_type = (short)fonts->log.lfCharSet & SCRIPT_CODE_MASK;
      UnuseMemory(globals->font_objects); 
   }

   init_font_language(globals, font, hdc);
#else
   font->char_type = WIN_CHARSET;      //Win16 character set
   font->code_page = WIN_CODEPAGE;     //Win16 code page
   font->language = WIN_LANGUAGE;      //Win16 language
#endif

   if (font->machine_var[PG_CHARSET] > SYMBOL_CHARSET)
      font->char_type |= NON_ROMAN_FLAG | MULTIBYTE_FLAG;

// if (IntlIsDoubleByteCharSet(font->machine_var[PG_CHARSET]))
//    font->char_type |= (MULTIBYTE_FLAG);

   char_type = (short)font->machine_var[PG_CHARSET];

   if (char_type == HEBREW_CHARSET || char_type == ARABIC_CHARSET || char_type == THAI_CHARSET)
      font->char_type |= RIGHT_FONT_FLAG;
   
   SelectObject(hdc, previous_font);
   pgReleasePlatformDevice((graf_device_ptr)&pg->port);
}


/* Converts Windows language id to numeric - Can't find any other way to do this.
Fixed up by Gar 1/11/96 to create a valid LCID and make machine_var[PG_LANGINFO]
contain actual language and script info. */

static void init_font_language (pg_globals_ptr globals, font_info_ptr font, HDC hdc)
{

#ifndef WIN_MULTILINGUAL
   font->machine_var[PG_LANGINFO] = font->language = 0;
#else

   font->language = (long)(MAKELCID((MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT)), SORT_DEFAULT));
   
   if (((font->platform & GENERAL_PLATFORM_MASK) != GENERAL_WINDOWS) &&
      (font->environs & FONT_NOT_AVAIL))
      font->machine_var[PG_LANGINFO] = 0;
   else
   {
	   // QUALCOMM: Win95 call the 16 bit version.
	   if (globals->machine_specific == RUNTIME_PLATFORM_WIN32S )
			font->machine_var[PG_LANGINFO] = (long)(GetFontLanguageInfo(hdc));
	   else
	   		font->machine_var[PG_LANGINFO] = (long)(GetFontLanguageInfo(hdc));
   }
#endif
}

/* multibyte_character_flags returns the PAIGE character bits for first half, last half
attributes given the code page, string and offset in question. If known_leadbyte is TRUE then
we are calling this knowing that the first byte in question is the first byte of the char. */

static long multibyte_character_flags (UINT code_page, pg_char_ptr string, long offset_begin,
         long offset_end, pg_boolean known_leadbyte)
{
#ifdef UNICODE
   return      0;
#else 

#ifndef WIN_MULTILINGUAL
   return      0;
#else
   long        result = 0;
   register long  index;
   BOOL        previous_leadflag = FALSE;
   
   index = offset_end;
   
   if (!known_leadbyte && (index > offset_begin) ) {

         // Compute the true lead byte flag of previous byte

      while (index > offset_begin) {
      
         --index;

         if (!FirstMultiCharByte(code_page, string[index])) {
            
            ++index;
            break;
         }
      }
      
      while (index < offset_end) {
         
         previous_leadflag = FALSE;

         if (FirstMultiCharByte(code_page, string[index])) {

            index += 2;

            if (index > offset_end)
               previous_leadflag = TRUE;
         }
         else
            index += 1;
      }
      
      index = offset_end;
   }

   if (FirstMultiCharByte(code_page, string[index])) {
      
      if (!previous_leadflag)
         result |= FIRST_HALF_BIT;
      else
         result |= LAST_HALF_BIT;
   }
   else {
      
      if (previous_leadflag)
         result |= LAST_HALF_BIT;
   }
   return   result;
#endif
#endif
}

/* Blink the style */

static void draw_box_blink (paige_rec_ptr pg, style_walk_ptr walker,
      draw_points_ptr draw_position)
{
   rectangle                  box_frame;
   Rect                    frame;
   point_start_ptr               line_start;
   long                    line_width, descent, next_offset;
   HDC                         hdc;
   
   if (blink_mode == 1)
   {
      line_start = draw_position->starts;
      next_offset = line_start->offset;
      next_offset += draw_position->block->begin;
      pgSetWalkStyle(walker, next_offset);
   
      descent = pgGetLineRealDescent(draw_position->block, line_start, walker);
      pgScaleLong(pg->scale_factor.scale, 0, &descent);
   
      box_frame.top_left = box_frame.bot_right = draw_position->from;
      box_frame.top_left.v -= draw_position->ascent;
      box_frame.bot_right.v += descent;
      line_width = line_start->bounds.bot_right.h - line_start->bounds.top_left.h;
      pgScaleLong(pg->scale_factor.scale, 0, &line_width);
      box_frame.bot_right.h = box_frame.top_left.h + line_width;
      
      RectangleToRect(&box_frame, NULL, &frame);
         
      hdc = pgGetPlatformDevice(&pg->port);
      InvertRect(hdc, &frame);
      pgReleasePlatformDevice(&pg->port);
   }
}
// 
// QUALCOMM: We're using this on Win95.  It really speeds things up.
//

DWORD WINAPI 
x_GetFontLanguageInfo( HDC dc )
{
   TEXTMETRIC tm;
   LANGID lang,sublang,sort;
   LCID result;

   GetTextMetrics(dc,&tm);
   sublang = SUBLANG_NEUTRAL;
   sort = SORT_DEFAULT;
   switch (tm.tmCharSet)
   {
      case CHINESEBIG5_CHARSET:
         lang = LANG_CHINESE;
         sort = SORT_CHINESE_BIG5;
         sublang = SUBLANG_CHINESE_TRADITIONAL;
         break;
   /* case GB2312_CHARSET:
         lang = LANG_CHINESE;
         sublang = SUBLANG_CHINESE_SIMPLIFIED;
         break;    */
         
      case HANGEUL_CHARSET:
         lang = LANG_KOREAN;
         sort = SORT_KOREAN_KSC;
         sublang = SUBLANG_KOREAN;
         break;
      case SHIFTJIS_CHARSET:
         lang = LANG_JAPANESE;
         sort = SORT_JAPANESE_XJIS;
         break;
      default:
         result = GetUserDefaultLCID();
         if (!result)
            result = GetSystemDefaultLCID();
         if (result)
            return result;
         lang = LANG_ENGLISH;
         sublang = SUBLANG_ENGLISH_US;
         break;
   }
   result = MAKELCID(MAKELANGID(lang,sublang),sort);
   return (result);
}


#ifdef WIN16_COMPILE
// okay. All we want to know is if the font has DBCS.
// In the MSDN October CD, there was an article that stated
// how incredibly difficult this was. Let's just try to fake it
// out with the code page numbers.

static short fe_code_pages[] =
{
   932, 936, 949, 950, 874, 864, 862, 708, 1256, 1255, 0
};

static
void
GetCPInfo(short code_page, CPINFO *cp_info)
{
   int i = 0;
   while (fe_code_pages[i])
   {
      if (fe_code_pages[i] == code_page)
         break;
      i++;
   }
   if (fe_code_pages[i])
      cp_info->MaxCharSize = 2;
}

static
void
GetStringTypeEx(LCID loc,DWORD itype,LPCSTR str,int csx,LPWORD rtype)
{
#ifdef WIN_MULTILINGUAL
   GetStringTypeA(loc,itype,str,csx,rtype);
#endif
}

#endif

#ifdef WIN16_COMPILE
#ifdef WIN_MULTILINGUAL
DWORD WINAPI 
x_GetFontLanguageInfo( HDC dc )
{
   TEXTMETRIC tm;
   LANGID lang,sublang,sort;
   LCID result;

   GetTextMetrics(dc,&tm);
   sublang = SUBLANG_NEUTRAL;
   sort = SORT_DEFAULT;
   switch (tm.tmCharSet)
   {
      case CHINESEBIG5_CHARSET:
         lang = LANG_CHINESE;
         sort = SORT_CHINESE_BIG5;
         sublang = SUBLANG_CHINESE_TRADITIONAL;
         break;
   /* case GB2312_CHARSET:
         lang = LANG_CHINESE;
         sublang = SUBLANG_CHINESE_SIMPLIFIED;
         break;    */
         
      case HANGEUL_CHARSET:
         lang = LANG_KOREAN;
         sort = SORT_KOREAN_KSC;
         sublang = SUBLANG_KOREAN;
         break;
      case SHIFTJIS_CHARSET:
         lang = LANG_JAPANESE;
         sort = SORT_JAPANESE_XJIS;
         break;
      default:
         result = GetUserDefaultLCID();
         if (!result)
            result = GetSystemDefaultLCID();
         if (result)
            return result;
         lang = LANG_ENGLISH;
         sublang = SUBLANG_ENGLISH_US;
         break;
   }
   result = MAKELCID(MAKELANGID(lang,sublang),sort);
   return (result);
}
#endif
#endif
#endif
