/* This file contains all the machine-specific code that is not memory-related
(memory-related code can be found in "machinememory.h").  To convert to a
new platform, change the code in this file but do NOT change the API or data
structures unless there is specific comment that you may do so, or it is probably
OK to change all the static/local functions as you see fit.

SPECIAL NOTE TO THINK C USERS:  The original pgMac.c on Mac has
been compiled with "MacHeaders" that might not match your own. To determine
what precompiled header you need, examine the list of #includes below (after
#ifndef THINK_C).

Copyright 1993 by DataPak Software, Inc.  All rights reserved. It is illegal
to remove this notice   */

/* Add Mac screen resolution processing and Universal headers, 28 Dec 94 OITC (oitc@iu.net) */

#include "Paige.h"

#ifdef MAC_PLATFORM

#include <lowmem.h>

#pragma segment pgbasic4

#include "pgTraps.h"
#include "pgRegion.h"
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
#include "pgSubref.h"

static void init_global_char (pg_char_ptr global_char, pg_short_t value);
static pg_short_t global_char_to_int (pg_char_ptr global_char);
static short global_char_size (pg_char_ptr global_char);
static void make_graf_device (const pg_globals_ptr globals, const generic_var port, graf_device_ptr device);
static void draw_cursor (paige_rec_ptr pg, t_select_ptr select, short verb);
static long find_next_word(paige_rec_ptr pg, style_walk_ptr walker,
		pg_char_ptr text, long global_offset, long local_offset, long end_offset,
		pg_short_t PG_FAR *begin, pg_short_t PG_FAR *end);
static void install_machine_font (paige_rec_ptr pg, style_info_ptr style,
		font_info_ptr font, pg_boolean include_offscreen);
static void draw_special_char (paige_rec_ptr pg, pg_char_ptr the_char, co_ordinate_ptr point);
static pg_boolean last_char_of_script (style_walk_ptr walker, long offset);
static long current_font_size (paige_rec_ptr pg, style_info_ptr style);
static void set_invisible_symbol_style (paige_rec_ptr pg);
static long same_case_length (paige_rec_ptr pg, style_walk_ptr walker,
		pg_char_ptr data, long global_offset, long length,
		long PG_FAR *info_flags);
static void draw_box_style(paige_rec_ptr pg, style_walk_ptr walker,
		draw_points_ptr draw_position);
static void draw_box_blink (paige_rec_ptr pg, style_walk_ptr walker,
		draw_points_ptr draw_position);
static void restore_graf_items (port_preserve PG_FAR *graf_stuff, GrafPtr port);
static long extra_char_info (long short_info, short sm_value);
static void make_mac_pattern (pg_char_ptr pat_bytes, unsigned short pat);
static void create_color_port (CGrafPtr port, Rect *global_area);
static pg_fixed return_pointsize (pg_fixed ptsize, short resolution);
static pg_fixed return_screensize (pg_fixed screensize, short resolution);
static void set_mac_script_info(paige_rec_ptr pg, StringPtr name, font_info_ptr info, short *font_num);
static void set_mac_lang_info(font_info_ptr info);
static short count_spaces (pg_char_ptr text, short length);
static void smooth_scroll_rect (Rect *rect, short distance_h, short distance_v, RgnHandle scroll_rgn);

static pg_boolean	blink = FALSE;
static short		blink_mode = 0;


/* pgMachineInit must initialize all the "global" variables in pg_globals.
This is included here because certain items will be created that are specific
to the platform in question.
WINDOWS NOTE: For Windows, globals->machine_const should already contain the
main window client area.	*/

PG_PASCAL (void) pgMachineInit (pg_globals_ptr globals)
{
	register pg_globals_ptr	 global_vars;
	GrafPtr					 bits_port, cur_port;
	SysEnvRec				 the_world;
	Handle					 rsrc;

	global_vars = globals;		  /* Makes for less code */

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
	global_vars->bullet_size = 6;

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

/* Init the default font, style & para records */

	pgInitDefaultFont(global_vars, &global_vars->def_font);
	pgInitDefaultStyle(global_vars, &global_vars->def_style, &global_vars->def_font);
	pgInitDefaultPar(global_vars, &global_vars->def_par);

	global_vars->def_bk_color.red = global_vars->def_bk_color.green
			= global_vars->def_bk_color.blue = 0xFFFF;
	global_vars->trans_color = global_vars->def_bk_color;

/* Check on existence of resource */

	if ((rsrc = GetResource(DEF_GLOBAL_RESTYPE, DEF_GLOBAL_ID)) != NULL) {
		long	    res_size;
		
		res_size = GetHandleSize(rsrc);
		BlockMove(*rsrc, &global_vars->max_offscreen, res_size);
		ReleaseResource(rsrc);
	}

	SysEnvirons(curSysEnvVers, &the_world);	//¥ TRS/OITC
	global_vars->color_enable = the_world.hasColorQD;	       
	global_vars->system_version = the_world.systemVersion;

/* Create an offscreen BitMap or PixMap: */

	PG_TRY(global_vars->mem_globals) {
	
		GetPort(&cur_port);
	
		bits_port = (GrafPtr) NewPtr(sizeof(GrafPort));
		global_vars->machine_const = (long) NewPtr(sizeof(GrafPort));

		if (!bits_port || !global_vars->machine_const) {
			
			if (bits_port)
				DisposePtr((Ptr)bits_port);
			if (global_vars->machine_const)
				DisposePtr((Ptr)global_vars->machine_const);
			
			global_vars->machine_const = 0;

			pgFailure(global_vars->mem_globals, NO_MEMORY_ERR, 0);
		}
		
		if (global_vars->color_enable) {
			Rect			    huge_area;
	
			SetRect(&huge_area, -32000, -32000, 32000, 32000);
			create_color_port((CGrafPtr)bits_port, &huge_area);
			create_color_port((CGrafPtr)globals->machine_const, &huge_area);
		}
		else {
	
			OpenPort(bits_port);
			OpenPort((GrafPtr) globals->machine_const);
		}
		
		SetPort(cur_port);
	
		global_vars->offscreen_buf = MemoryAlloc(global_vars->mem_globals,
				sizeof(pg_bits8), MAX_OFFSCREEN / 2, MAX_OFFSCREEN / 8);
		SetMemoryPurge(global_vars->offscreen_buf, 0x00A0, TRUE);
		
		make_graf_device(global_vars, bits_port, &globals->offscreen_port);
		
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

/* pgGetCharWidth returns the width of a character. */

PG_PASCAL (short) pgGetCharWidth (paige_rec_ptr pg_rec, style_info_ptr style, pg_char the_char)
{
	GrafPtr			old_port;
	font_info_ptr	font;
	long			font_index;
	short			width;

	GetPort(&old_port);
	SetPort((GrafPtr)pg_rec->globals->machine_const);
	font_index = (long)style->font_index;
	
	if (font_index < 0)
		font_index = 0;

	font = UseMemoryRecord(pg_rec->fonts, font_index, 0, TRUE);
	install_machine_font(pg_rec, style, font, FALSE);
	width = CharWidth((char)the_char);

	SetPort(old_port);

	return		width;
}

/* pgMachineShutdown must de-allocate anything it created during pgMachineInit */

PG_PASCAL (void) pgMachineShutdown (const pg_globals_ptr globals)
{
	pg_globals_ptr		  	global_vars;
	CGrafPtr				color_port;

	global_vars = globals;		  /* Makes for less code */

	if (global_vars->color_enable) {
		
		if ((color_port = global_vars->offscreen_port.machine_var) != NULL)
			CloseCPort(color_port);

		if (global_vars->machine_const)
			CloseCPort((CGrafPtr) global_vars->machine_const);
	}
	else {
		GrafPtr		 bw_port;
		
		if ((bw_port = global_vars->offscreen_port.machine_var) != NULL)
			ClosePort(bw_port);
		if (global_vars->machine_const)
			ClosePort((GrafPtr) global_vars->machine_const);
	}

	if (global_vars->offscreen_port.machine_var)
		DisposePtr(global_vars->offscreen_port.machine_var);
	if (global_vars->machine_const)
		DisposePtr((Ptr) global_vars->machine_const);

	DisposeNonNilMemory(global_vars->offscreen_buf);

	pgCloseDevice(global_vars, &global_vars->offscreen_port);
	DisposeNonNilMemory(global_vars->offscreen_exclusion);
	DisposeNonNilMemory(global_vars->pg_list);
}



/* pgInitDefaultDevice sets up a device to use as a default. In Mac, for instance,
this would be simply the current port */

PG_PASCAL (void) pgInitDefaultDevice (const pg_globals_ptr globals, graf_device_ptr device)
{
	GrafPtr		 cur_port;

	GetPort(&cur_port);
	make_graf_device(globals, (generic_var) cur_port, device);
}


/* pgInitDevice sets up a Paige graf_device. Although the code in this file
is machine-dependent, we can hopefully use this same API for all platforms. The
idea is to pass <whatever is appropriate> in the_port and/or machine_var and
let this machine-specific function initialize a generic Paige graf_device.
FOR MACINTOSH: the_port is a GrafPtr. */

PG_PASCAL (void) pgInitDevice (const pg_globals_ptr globals, const generic_var the_port,
		long machine_ref, graf_device_ptr device)
{
	make_graf_device(globals, the_port, device);
}


/* pgCloseDevice disposes everything created for pgInitDevice.  */

PG_PASCAL (void) pgCloseDevice (const pg_globals_ptr globals, const graf_device_ptr device)
{
#pragma unused (globals)

		port_preserve	   *stack_stuff;
		long			    stack_size;

	if (device->graf_stack) {

		if ((stack_size = GetMemorySize(device->graf_stack)) != 0) {
		
			stack_stuff = UseMemory(device->graf_stack);
			
			while (stack_size) {
				
				if (stack_stuff->clip_rgn)
					pgDisposeRgn(stack_stuff->clip_rgn);
	
				++stack_stuff;
				--stack_size;
			}

			UnuseMemory(device->graf_stack);
		}

		DisposeMemory(device->graf_stack);
	}
	
	if (device->clip_rgn)
		pgDisposeRgn(device->clip_rgn);
	if (device->scroll_rgn)
		pgDisposeRgn(device->scroll_rgn);
	if (device->previous_items[ORIGINAL_CLIP])
		pgDisposeRgn((RgnHandle)device->previous_items[ORIGINAL_CLIP]);

}


/* pgInsertQuery is a version 1.3 function, designed mostly for multilingual input. When PAIGE
is asked to insert a key with key_insert_mode, this hook is called. This hook must return
the insertion mode to use. For example, if another key is required and the current char should
be buffered then key_buffer_mode can be returned. */

PG_PASCAL (short) pgInsertQuery (paige_rec_ptr pg, pg_char_ptr the_char, short charsize)
{
#pragma unused (pg, the_char, charsize)
	return	(short)key_insert_mode;
}


/* pgCharClassProc gets called when parsing chars to determine the subset type. Usually this
refers to multilingual double byte chars so highlighting will group all similar types. */

PG_PASCAL (pg_word) pgCharClassProc (paige_rec_ptr pg, pg_char_ptr the_char, short charsize,
		style_info_ptr style, font_info_ptr font)
{
#pragma unused (pg, the_char, charsize, style, font)
	return	0;
}


/* DEFAULT HIGHLIGHT DRAW FUNCTION. This is the internal hook used to display
highlight. It is obviously machine-dependent. */

PG_PASCAL (void) pgDrawHiliteProc (paige_rec_ptr pg, shape_ref rgn)
{
	Rect					    piece_of_shape;
	rectangle				    vis_bounds, scaled_r, container;
	co_ordinate				    scroll_adjust, repeat_offset;
	register rectangle_ptr	  	r_ptr;
	register long			    r_qty;
	long					    container_proc_refcon;
	pg_short_t				    last_container, real_container;

	if (!pgTransColor(pg->globals, &pg->bk_color))
		RGBBackColor((RGBColor *) &pg->bk_color);

	r_ptr = UseMemory(rgn);

	scroll_adjust = pg->scroll_pos;
	pgNegatePt(&scroll_adjust);
	pgAddPt(&pg->port.origin, &scroll_adjust);

	pgShapeBounds(pg->vis_area, &vis_bounds);
	//pgOffsetRect(&vis_bounds, pg->port.origin.h, pg->port.origin.v);
	
	if (pg->flags & SCALE_VIS_BIT)
		pgScaleRect(&pg->port.scale, NULL, &vis_bounds); 

	container_proc_refcon = 0;
	
	real_container = pgGetSectWrapRect(pg, &r_ptr[1], &last_container, &repeat_offset);
	
	GetMemoryRecord(pg->wrap_area, real_container + 1, &container);
	pgOffsetRect(&container, repeat_offset.h, repeat_offset.v);

	pgCallContainerProc(pg, last_container, &scroll_adjust,
		clip_container_verb,  &pg->scale_factor, &container_proc_refcon); 
	pgCallContainerProc(pg, last_container, &scroll_adjust,
		will_draw_verb,  &pg->scale_factor, NULL);

	for (r_qty = GetMemorySize(rgn) - 1; r_qty; --r_qty) {
		
		++r_ptr;
		
		if (!pgSectRect(r_ptr, &container, NULL)) {
			pg_short_t		      new_container;
			
			real_container = pgGetSectWrapRect(pg, r_ptr, &new_container, &repeat_offset);
			
			if (new_container != last_container) {
				
				last_container = new_container;
				GetMemoryRecord(pg->wrap_area, real_container + 1, &container);
				pgOffsetRect(&container, repeat_offset.h, repeat_offset.v);
				
				pgCallContainerProc(pg, last_container, &scroll_adjust,
					clip_container_verb,  &pg->scale_factor, &container_proc_refcon); 
				pgCallContainerProc(pg, last_container, &scroll_adjust,
					will_draw_verb,  &pg->scale_factor, NULL);
			}
		}

		pgScaleRectToRect(&pg->scale_factor, r_ptr, &scaled_r, &scroll_adjust);

		if ((scaled_r.top_left.v < vis_bounds.bot_right.v)
				&& (scaled_r.bot_right.v > vis_bounds.top_left.v)) {
			
			unsigned char old_hilite;

			RectangleToRect(&scaled_r, NULL, &piece_of_shape);

			//¥ TRS/OITC
			old_hilite = LMGetHiliteMode();
			LMSetHiliteMode(50);
			InvertRect(&piece_of_shape);
			LMSetHiliteMode(old_hilite);
		}
	}
	
	UnuseMemory(rgn);
	pgCallContainerProc(pg, last_container, NULL, unclip_container_verb,  NULL, &container_proc_refcon);
}




/* DEFAULT HIGHLIGHT DRAW THE CUSOR ("caret"). It is obviously machine-dependent.
NOTE: The correct port is set. */

PG_PASCAL (void) pgDrawCursorProc (paige_rec_ptr pg, t_select_ptr select,
		short verb)
{
	if (pg->flags & NO_EDIT_BIT)
		return;

	switch (verb) {

		case toggle_cursor_idle:
			break;
			
		case toggle_cursor:    
			draw_cursor(pg, select, verb);
			pg->flags ^= CARET_BIT;
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

			if (!(pg->flags & CARET_BIT))
				pg->timer_info = TickCount();
			break;

		case show_cursor:
			if (!(pg->flags & CARET_BIT)) {
				draw_cursor(pg, select, verb);
				pg->flags |= CARET_BIT;
				pg->timer_info = TickCount();
			}
			break;

		case update_cursor:
			if (pg->flags & CARET_BIT)
				draw_cursor(pg, select, verb);
			break;
		
		case compute_cursor:
			draw_cursor(pg, select, verb);
			break;
	}

#ifdef PG_DEBUG
	draw_cursor(pg, select, debug_cursor);
#endif
}


/* DEFAULT FUNCTION TO INITIALIZE A STYLE RECORD. It can be somewhat machine-
dependent. */

PG_PASCAL (void) pgStyleInitProc (paige_rec_ptr pg, style_info_ptr style,
			font_info_ptr font)
{
	register	style_info_ptr  the_style;
	GrafPtr		cur_port, my_port;
	FontInfo	f_info;
	short	    underline_extra, super_or_sub_extra;
	
	the_style = style;
	underline_extra = 0;

	GetPort(&cur_port);
	my_port = (GrafPtr) pg->globals->machine_const;
	SetPort(my_port);

	the_style->machine_var = PaigeToQDStyle(the_style);
	if (SpecialUnderline(the_style))
		the_style->machine_var &= ((long)~underline);

	style->procs.install(pg, style, font, NULL, 0, FALSE);
	GetFontInfo(&f_info);

	if (!(pg->flags & NO_DEFAULT_LEADING))
		the_style->leading = f_info.leading;
	else
		the_style->leading = 0;

	the_style->ascent = f_info.ascent;
	the_style->descent = f_info.descent;

	if (font->char_type) {
	
		the_style->class_bits |= NO_SMART_DRAW_BIT;
		
		if ((font->char_type & SCRIPT_CODE_MASK) <= smKorean) {
		
			the_style->class_bits |= (CANT_UNDERLINE_BIT);
			
			if (the_style->styles[underline_var]
				|| the_style->styles[word_underline_var]
				|| the_style->styles[dotted_underline_var])
				underline_extra = 1;
		}
	}
	else
		the_style->class_bits &= (~NO_SMART_DRAW_BIT);

	if (font->char_type & RIGHT_FONT_FLAG)
		the_style->class_bits |= RIGHTLEFT_BIT;
	else
		the_style->class_bits &= (~RIGHTLEFT_BIT);
	
/* Compute this font's "overhang" values: */

	if (the_style->machine_var & MAC_STYLE_MASK) {
		FMInput				 	fm_input;
		FMOutPtr				fm_output;
		long				    font_height, pixel_extra;
					
		fm_input.family = font->family_id;
		fm_input.size = pgComputePointSize(pg, the_style);
		fm_input.face = the_style->machine_var & MAC_STYLE_MASK;
		fm_input.needBits = FALSE;
		fm_input.device = 0;
		fm_input.numer.h = fm_input.numer.v = 1;
		fm_input.denom = fm_input.numer;
		
		fm_output = FMSwapFont(&fm_input);
		
		pixel_extra = (long) fm_output->italicPixels + fm_output->shadowPixels;
		pixel_extra = (Fixed)FixRatio(pixel_extra, 32);
		
		font_height = f_info.ascent + f_info.descent - 1;
		pixel_extra = FixMul((Fixed)(font_height << 16), (Fixed)pixel_extra);
		
		the_style->right_overhang = pixel_extra >> 16;

		if (pixel_extra & 0x00008000)
			++the_style->right_overhang;
	}
	else
		the_style->right_overhang = 0;

	SetPort(cur_port);

	if ((super_or_sub_extra = the_style->styles[superscript_var]) != 0) {
		
		if (the_style->shift_verb == percent_of_style)
			super_or_sub_extra = ((the_style->ascent + the_style->descent) * super_or_sub_extra) / 100;

		the_style->ascent += super_or_sub_extra;
	}

	if ((super_or_sub_extra = the_style->styles[subscript_var]) != 0) {
		
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
		font_info_ptr the_font, style_info_ptr composite_style, short style_overlay, pg_boolean include_offscreen)
{
	pg_globals_ptr	  			globals;
	graf_device_ptr	  			old_device;
	style_info					imposed_style;
	font_info					imposed_font;
	register style_info_ptr		style;
	register font_info_ptr		font;

	globals = pg->globals;

	style = the_style;
	font = the_font;

	if (style_overlay) {
		
		pgStyleSuperImpose(pg, style, &imposed_style, &imposed_font, style_overlay);

		style = &imposed_style;
		font = &imposed_font;
	}

    old_device = globals->current_port;
    globals->current_port = (graf_device_ptr) &pg->port;
	install_machine_font(pg, style, font, include_offscreen);
    globals->current_port = old_device;

	if (include_offscreen) {

		GrafPtr	 current_port, other_port;

		GetPort(&current_port);
		other_port = globals->offscreen_port.machine_var;

		if (current_port == other_port)
			other_port = globals->current_port->machine_var;
		
		SetPort(other_port);
		install_machine_font(pg, style, font, FALSE);
		SetPort(current_port);
	}

	if (composite_style)
		pgBlockMove(style, composite_style, sizeof(style_info));
}


/* DEFAULT DELETE STYLE PROC. For Macintosh, this does nothing. WARNING: pg can
be NULL if called from Undo functions!
For Windows, the font object (in style->machine_var) is deleted. */

PG_PASCAL (void) pgDeleteStyleProc (paige_rec_ptr pg, pg_globals_ptr globals,
		short reason_verb, format_ref all_styles, style_info_ptr style)
{
#pragma unused (pg, reason_verb, all_styles, style)

}


/* DEFAULT DUPLICATE STYLE PROC. For Macintosh, this does nothing.
WARNING: pg can be NULL if called from Undo functions!     */

PG_PASCAL (void) pgDupStyleProc (paige_rec_ptr src_pg, paige_rec_ptr target_pg,
		short reason_verb, format_ref all_styles, style_info_ptr style)
{
#pragma unused (src_pg, target_pg, reason_verb, all_styles, style)
}


/* pgSaveStyleProc is the default style that gets called just prior to saving
it to a file. For Macintosh this does nothing. For Windows the machine_var field
is zerod out (because it is a handle). */

PG_PASCAL (void) pgSaveStyleProc (paige_rec_ptr pg, style_info_ptr style_to_save)
{
#pragma unused (pg, style_to_save)
}



/* DEFAULT FUNCTION TO MEASURE TEXT WIDTHS. It is obviously machine-dependent. 
Note: The current font, style, etc. is pre-set before this call. The types
parameter is a pointer to an array of shorts that should be set to character
type bits (as in char_info proc).  HOWEVER, only control char bits should be
set such as CR, TAB, etc.  NOTE: types can be NULL.
Changes 2/15/94 -- added measure_verb param which indicates the type of output
wanted (see enums in Paige.h). Also removed style pointer and font pointer,
replaced with a single style_walk_ptr param.
Added 4/14/94: if scale_widths is "TRUE" the result(s) are to be scaled as
follows: if scaled_widths == PLAIN_SCALE then widths are scaled according
to scaling factor; if scaled_widths == COMPENSATE_SCALE then the widths
are "normal" but errors in scaling are compensated. */

PG_PASCAL (void) pgMeasureProc (paige_rec_ptr pg, style_walk_ptr walker,
		pg_char_ptr data, long length, pg_short_t slop, long PG_FAR *positions,
		short PG_FAR *types, short measure_verb, long current_offset, pg_boolean scale_widths,
		short call_order)
{
	register short		  	PG_FAR *each_type;
	register pg_short_t	    PG_FAR *short_positions;
	register pg_char_ptr    text;
	register long		   	PG_FAR *long_positions;
	register pg_short_t	   	the_byte;
	pg_char_ptr			    measure_data, original_data;
	short PG_FAR		    *the_types;
	long PG_FAR			    *measure_positions;
	memory_ref			    transliterate_ref;
	style_info				measure_style;
	style_info_ptr		  	style, original_style;
	font_info_ptr		   	font;
	short				   	char_bytes, right_to_left, soft_hyphens_exist;
	short				   	system_just, done_types, kern_short, num_spaces, use_slop;
	long				    measure_length, position_bias, extra_just, scale_slop;
	long				    total_length, position_ctr, kerning_extra, kerning_ctr;
	pg_fixed				space_extra_width, space_accumulation;
	pg_boolean				scaled_it, small_caps_compensate;

#pragma unused (current_offset)

	style = original_style = walker->cur_style;
	font = walker->cur_font;
	the_types = types;
	scaled_it = FALSE;
	
	if (style->class_bits & NON_TEXT_BITS) {
		
		for (measure_length = 0, long_positions = positions;
				measure_length <= length; ++measure_length)
			long_positions[measure_length] = *long_positions;
		
		if (the_types)
			pgFillBlock(the_types, length * sizeof(short), 0);

		return;
	}
	
	if ((transliterate_ref = pgConvertTextCaps(pg, style, data, length)) != MEM_NULL)
		measure_data = UseMemory(transliterate_ref);
	else
		measure_data = data;
	
	original_data = data;

	total_length = length;
	measure_positions = positions;

// Determine if we need to compensate for full-justify, small caps style:

	small_caps_compensate = (pg_boolean)((original_style->styles[small_caps_var] != 0) && (slop != 0));
	
	if (small_caps_compensate) {
		
		if (!(num_spaces = count_spaces(measure_data, length)))
			small_caps_compensate = FALSE;
		else {
		
			space_extra_width = (pg_fixed)FixRatio(slop, num_spaces);
			space_accumulation = 0;
		}
	}
	
	use_slop = slop;

	while (total_length) {
		
		measure_length = total_length;
		walker->cur_style = style = original_style;
		
		if (style->styles[small_caps_var]) {
			long		    info_flags;

			measure_length = pgSameCaseLength(pg, walker, original_data,
					current_offset, total_length, &info_flags);
			
			if (!info_flags) {
				
				style = UseMemoryRecord(pg->t_formats, style->small_caps_index, 0, FALSE);
				walker->cur_style = style;
			}
			
			style->procs.install(pg, style, walker->cur_font, &walker->superimpose, walker->style_overlay, FALSE);
			
			if (small_caps_compensate) {
				
				if ((num_spaces = count_spaces(measure_data, measure_length)) > 0) {
					pg_fixed			fractional_slop;
					
					fractional_slop = (pg_fixed)num_spaces;
					fractional_slop <<= 16;
					fractional_slop = pgMultiplyFixed(fractional_slop, space_extra_width);
					space_accumulation += fractional_slop;
					use_slop = HiWord(space_accumulation);
					space_accumulation &= 0x0000FFFF;
				}
				else
					use_slop = 0;
			}
		}

		short_positions = (pg_short_t *) measure_positions;
		position_bias = *measure_positions;
		scale_slop = use_slop;
		extra_just = 0;
	
		if (scale_widths) {

			extra_just = pgScalePointSize(pg, walker, measure_data,
					measure_length, &scaled_it); /* Scale text if appropriate */
			pgScaleLong(pg->scale_factor.scale, 0, &scale_slop);
		}

		MeasureJust((Ptr)measure_data, measure_length, scale_slop + extra_just, (Ptr)short_positions);
		
		if (font->char_type && (font->char_type & SCRIPT_CODE_MASK) <= smKorean)
			kerning_extra = original_style->char_extra;
		else {
			
			if ((scale_slop + extra_just) && measure_verb < 0)
				kerning_extra = FixRatio(scale_slop + extra_just, measure_length);
			else
				kerning_extra = 0;
		}
		
		if (pg->flags2 & WHOLE_KERNING_BIT)
			kerning_extra = pgRoundFixed(kerning_extra) & 0xFFFF0000;

		if (kerning_extra) {
		
			kerning_ctr = 0;

			// MeasureJust is not accounting for CharExtra with Japanese fonts.
			// Fill in this array with the absolute position of each character. The first element is 0,
			// the second is the width of the first character, the third is the width of the first and second characters.
			// The last element at length + 1 is the width of the entire string. 
			// Double byte characters are represented with two consecutive bytes of the same width.	
			
			// GC note 5-7-96 -- We also want to add extra spacing for situations that have
			// no spaces.  Works for Windows but not here.

			for (position_ctr = 0; position_ctr <= measure_length; ++position_ctr) {
				kern_short = HiWord(kerning_ctr);
				short_positions[position_ctr] += kern_short;
				
				if (CharByte((Ptr)measure_data, position_ctr) < 0
					|| (CharByte((Ptr)measure_data, position_ctr) == 0 && measure_data[position_ctr] != ' '))
					kerning_ctr += kerning_extra;
			}
		}
		
		right_to_left = (font->char_type & RIGHT_FONT_FLAG);
		system_just = pgSystemDirection(pg->globals);

		if (right_to_left) {
			short			   swap_val, char_width, end_index;
	
			if (measure_verb == measure_width_locs) {
				
				if (measure_length > 1) {
				
					if (system_just >= 0) {
					
						swap_val = *short_positions;
						*short_positions = short_positions[measure_length];
						
						for (end_index = measure_length; end_index; --end_index)
							if (short_positions[end_index] == *short_positions)
								short_positions[end_index] = swap_val;
							else
								break;
					}
	
					swap_val = *short_positions;
					char_width = 0;
	
					for (position_ctr = measure_length + 1; position_ctr; ++short_positions, --position_ctr) {
						
						char_width+= (swap_val - *short_positions);
						swap_val = *short_positions;
						*short_positions = char_width;
					}
	
					short_positions = (pg_short_t *) measure_positions;
				}
				else
				if (system_just < 0) {
					
					swap_val = *short_positions;
					*short_positions = short_positions[1];
					short_positions[1] = swap_val;
				}
			}
		}
	
		long_positions = measure_positions;
		position_ctr = measure_length + 1;
		long_positions += position_ctr;
		short_positions += position_ctr;
	
		*(--long_positions) = *(--short_positions);
		*long_positions += position_bias;
		--position_ctr;
	
		if ((each_type = the_types) != NULL) {
		
			each_type += position_ctr;
			text = original_data + position_ctr;
			char_bytes = style->char_bytes;
		}
	
		done_types = 0;
		soft_hyphens_exist = FALSE;
	
		while (position_ctr) {
			*(--long_positions) = *(--short_positions);
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
						pg_globals_ptr	  globals;
						
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
			long	    hyphen_width, older_position;

			hyphen_width = MEASURE_SYMBOL(pg->globals->hyphen_char);
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
			long		    trailing_pos;
			
			trailing_pos = *measure_positions;
			*measure_positions = measure_positions[measure_length];
			
			if (call_order == -1)
				measure_positions[measure_length] = trailing_pos;
		}

		if (scale_widths == COMPENSATE_SCALE) {
			pg_fixed		scaling;
			long		    origin;
	
			long_positions = measure_positions;
			scaling = -pg->scale_factor.scale;
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

PG_PASCAL (long) pgCharInfoProc (paige_rec_ptr pg, style_walk_ptr style_walker,
		pg_char_ptr data, long block_offset, long offset_begin, long offset_end, long char_offset,
		long mask_bits)
{
	register		style_walk_ptr  walker;
	register		pg_char		 	the_char;
	register		long		    result;
	register		pg_globals_ptr  globals;
	long						    real_break_bit;
	pg_boolean					    non_roman;
	pg_short_t					    decimal_char, hyphen_char;
	pg_short_t					    symbol_check;
	long							class_bits;
	short						    char_bytes;
	register		short		    sm_value;
	GrafPtr						 	cur_port, scratch_port;
	short						    sm_type, sm_class, sm_2byte, char_order;
	pg_boolean						blank_char_status;
	short							char_after_type;
	long							next_char_offset;

	walker = style_walker;

	if ((pg->flags & NO_HIDDEN_TEXT_BIT) && walker->cur_style->styles[hidden_text_var])
		return  0;
	
	non_roman = (walker->cur_font->char_type && (mask_bits & NON_CTL_CHAR_BITS));

	if (!(mask_bits & NON_MULTIBYTE_BITS))
		if (!non_roman)
			return	0;

	class_bits = walker->cur_style->class_bits;
	
	globals = pg->globals;

	the_char = data[char_offset];
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
		long	    distance_in;
		short	   byte_remainder;
		
		/* Multi-byte char or non-text chars */
		
		distance_in = walker->current_offset - walker->prev_style_run->offset;		
		byte_remainder = (short)distance_in;
		
		if (byte_remainder == 0 || !(byte_remainder %= char_bytes))
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
	if (non_roman && (the_char > 0x1F)) {
		
		GetPort(&cur_port);
		scratch_port = (GrafPtr) globals->machine_const;
		SetPort(scratch_port);

		if (scratch_port->txFont != walker->cur_font->family_id)
			walker->cur_style->procs.install(pg, walker->cur_style, walker->cur_font, NULL, walker->style_overlay, FALSE);

		sm_value = CharType((Ptr) data, char_offset);
		sm_type = sm_value & smcTypeMask;
		sm_class = sm_value & smcClassMask;
		sm_2byte = sm_value & smChar2byte;
		char_order = CharByte((Ptr) data, char_offset);

// ¥WAP / MSI
		if ((blank_char_status = ((sm_class == smPunctBlank) && (sm_type == smCharPunct))) != 0)
			result |= BLANK_BIT;
		else if (mask_bits & (WORD_BREAK_BIT | WORD_SEL_BIT | INCLUDE_SEL_BIT)){
			char_after_type = sm_value;
			
			// test on the first byte of multibyte chars or single byte chars only
			
			if ((!sm_2byte) || (char_order < 0)){
				next_char_offset = 1;
				if(sm_2byte)
					next_char_offset += 1;
				if((char_offset + next_char_offset) <= offset_end)
					char_after_type = CharType((Ptr) data, char_offset + next_char_offset);
				char_after_type &= smcTypeMask;
				if(char_after_type == 0x0F)
					char_after_type = smCharKatakana;
			}
			
			switch (sm_type){
				case smCharPunct:
					if(sm_class == smPunctNormal)
						result |= (WORD_SEL_BIT | INCLUDE_BREAK_BIT | INCLUDE_SEL_BIT);
					break;
					
				case 0x0F:				// system bug ?
					result |= (INCLUDE_BREAK_BIT);
					sm_type = smCharKatakana;
					break;
					
				case smCharKatakana:	// Japanese
				case smCharHiragana:
				case smCharIdeographic:
				case smCharHangul:		// Korean
				case smCharJamo:
				case smCharBopomofo:	// Chinese
					result |= (WORD_BREAK_BIT);
					if(char_after_type & (smCharAscii || smCharExtAscii))
						result |= (INCLUDE_BREAK_BIT);
					break;
					
				default:
				case smCharAscii:		// roman
				case smCharExtAscii:
					result |= (INCLUDE_BREAK_BIT | INCLUDE_SEL_BIT);
					break;
			}
			
			if ((char_after_type != sm_type) && (mask_bits & WORD_SEL_BIT))
				result |= (WORD_SEL_BIT | INCLUDE_BREAK_BIT | INCLUDE_SEL_BIT);

		}
		
		if (sm_2byte) {
			
			if ((char_offset + 1) < offset_end)
				char_bytes = 2;

			if (char_order < 0) {
			
				result |= FIRST_HALF_BIT;

				if (mask_bits & (QUOTE_BITS | DECIMAL_CHAR_BIT)) {
					symbol_check <<= 8;
					symbol_check |= data[char_offset + 1];
				}
			}
			else {
				
				symbol_check = 0xFFFF;
				result |= LAST_HALF_BIT;
			}
		}


		if (mask_bits & (WORD_SEL_BIT | WORD_BREAK_BIT)) {
			
			if (block_offset != NO_BLOCK_OFFSET && last_char_of_script(walker, block_offset + char_offset))
				result |= (WORD_BREAK_BIT | WORD_SEL_BIT | INCLUDE_SEL_BIT | INCLUDE_BREAK_BIT);
			else{
				if (blank_char_status){
					result |= (real_break_bit | INCLUDE_SEL_BIT | WORD_SEL_BIT | INCLUDE_BREAK_BIT | WORD_BREAK_BIT);
				}
			}
		}

		if (mask_bits & (EUROPEAN_BIT | SYMBOL_BIT | NUMBER_BIT | UPPER_CASE_BIT
				| LOWER_CASE_BIT | PUNCT_NORMAL_BIT | OTHER_PUNCT_BIT))
			result = extra_char_info(result, sm_value);

		result |= NON_ROMAN_BIT;
		
		SetPort(cur_port);
	}
	else
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

			GetPort(&cur_port);
			scratch_port = (GrafPtr) globals->machine_const;
			SetPort(scratch_port);
	
			if (scratch_port->txFont != walker->cur_font->family_id)
				walker->cur_style->procs.install(pg, walker->cur_style, walker->cur_font, NULL, walker->style_overlay, FALSE);

			sm_value = CharType((Ptr) data, char_offset);
			
			result = extra_char_info(result, sm_value);
			
			SetPort(cur_port);
		}
	}

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
	register pg_char_ptr		text;
	style_info_ptr		  		original_style;
	memory_ref			      	transliterate_ref, special_locs;
	Point				   		start_pt, end_pen;
	GrafPtr				 		cur_port;
	unsigned short		  		port_in_color;
	long				    	total_length, draw_length, draw_offset, extra_just, use_extra;
	long PG_FAR					*small_caps_locs;
	short				   		text_mode, use_mode, underline_bits, scale_slop;
	pg_boolean			      	non_transparent_text, scaled_it;

	if (walker->cur_style->class_bits & NON_TEXT_BITS)
		return;
	
	original_style = walker->cur_style;
	draw_offset = (long) offset;
	total_length = length;

	use_mode = draw_mode;
	use_extra = extra;
	non_transparent_text = !(pgTransColor(pg->globals, &walker->superimpose.bk_color));
	
	if (pg->globals->offscreen_enable == OFFSCREEN_SUCCESSFUL)
		use_mode = direct_or;

	GetPort(&cur_port);
	port_in_color = cur_port->portBits.rowBytes & 0xC000;

	if (non_transparent_text) {
		
		RGBBackColor((RGBColor *)&walker->superimpose.bk_color);
		use_mode = direct_copy;
	}
	else
	if (port_in_color)
		if (!pgTransColor(pg->globals, (color_value_ptr) &((CGrafPtr) cur_port)->rgbBkColor))
			use_mode = direct_or;

	if (use_mode == direct_copy)
		text_mode = srcCopy;
	else
	if (use_mode == direct_xor)
		text_mode = srcXor;
	else
		text_mode = srcOr;

/* However, an exception is "grayish_or" */

	if (draw_mode == grayish_or || draw_mode == grayish_bits_copy)
		text_mode = grayishTextOr;

	cur_port->txMode = text_mode;

	start_pt.h = pgLongToShort(draw_position->from.h);
	start_pt.v = pgLongToShort(draw_position->from.v);

	start_pt.h -= (short)(walker->cur_style->right_overhang / 2);

	underline_bits = SpecialUnderline(&walker->superimpose);

	if (original_style->styles[small_caps_var] && extra) {

		special_locs = pgGetSpecialLocs(pg, draw_position->block, draw_position->starts,
				length, extra, PLAIN_SCALE);

		small_caps_locs = UseMemory(special_locs);
	}
	else
		small_caps_locs = NULL;

	while (total_length) {
		
		draw_length = total_length;
		
		if (original_style->styles[small_caps_var]) {
			long	    info_flags;
			
			draw_length = pgSameCaseLength(pg, walker, &data[draw_offset],
				draw_position->real_offset, total_length, &info_flags);

			if (!info_flags)
				walker->cur_style = UseMemoryRecord(pg->t_formats,
						original_style->small_caps_index, 0, FALSE);
			
			walker->cur_style->procs.install(pg, walker->cur_style, walker->cur_font,
							&walker->superimpose, walker->style_overlay, TRUE);
		}

		text = data + draw_offset;
		if ((transliterate_ref = pgConvertTextCaps(pg, original_style, text, draw_length)) != MEM_NULL)
			text = UseMemory(transliterate_ref);

		if (small_caps_locs) {
			long			compensated_width, real_width;
			
			compensated_width = small_caps_locs[draw_length] - *small_caps_locs;
			real_width = TextWidth((Ptr)text, 0, draw_length);
			use_extra = compensated_width - real_width;
			small_caps_locs += draw_length;
		}

	    scaled_it = FALSE;
		scale_slop = pgScalePointSize(pg, walker, text, draw_length, &scaled_it); /* Scale text if appropriate */
		extra_just = use_extra;
		pgScaleLong(pg->scale_factor.scale, 0, &extra_just);
	
		MoveTo(start_pt.h, start_pt.v);
		
		if ((extra_just + scale_slop) && !(draw_position->starts->flags & HAS_WORDS_BIT)) {
			pg_fixed			char_extra;
			
			char_extra = FixRatio(extra_just + scale_slop, draw_length);
			CharExtra(char_extra);
			DrawJust((Ptr) text, draw_length, 0);
			CharExtra(original_style->char_extra);
		}
		else
			DrawJust((Ptr) text, draw_length, extra_just + scale_slop);

		if (transliterate_ref)
			UnuseMemory(transliterate_ref);
	
		if (draw_position->starts->flags & WORD_HYPHEN_BIT)
			DRAW_SYMBOL(pg->globals->hyphen_char);
	
		GetPen(&end_pen);
	
		draw_position->to.h = end_pen.h;
		draw_position->to.v = end_pen.v;
		
		total_length -= draw_length;
		draw_offset += draw_length;
		start_pt.h = end_pen.h;
		walker->cur_style = original_style;
	}
	
	if (small_caps_locs)
		UnuseAndDispose(special_locs);

	if (original_style->styles[small_caps_var])
		original_style->procs.install(pg, original_style, walker->cur_font,
						&walker->superimpose, walker->style_overlay, FALSE);

	if (walker->superimpose.styles[boxed_var])
		draw_box_style(pg, walker, draw_position);

	start_pt.h = pgLongToShort(draw_position->from.h);
	start_pt.v = pgLongToShort(draw_position->from.v);

	if (underline_bits) {
		
		walker->current_offset = draw_position->real_offset;

		if (underline_bits & WORD_UNDERLINE)
			pgDrawWordUnderline(pg, walker, underline_bits, data, offset, length, extra, draw_position);
		else
			pgDrawSpecialUnderline(pg, start_pt, end_pen.h - start_pt.h,
					walker->cur_style, underline_bits);

		pgSetWalkStyle(walker, draw_position->real_offset);
	}
 
	if (walker->cur_style->styles[strikeout_var]
			|| (walker->cur_style->styles[hidden_text_var] && !walker->style_overlay)) {
		Pattern				hidden_pat;
		FontInfo			font_metric;
		
		PenNormal();
		GetFontInfo(&font_metric);

		if (walker->cur_style->styles[hidden_text_var]) {

			//¥ TRS/OITC
			make_mac_pattern((pg_char_ptr)hidden_pat.pat, HIDDEN_TEXT_PAT);
			PenPat(&hidden_pat);
		}

		// MoveTo(start_pt.h, start_pt.v - (draw_position->ascent / 3));
		
		MoveTo(start_pt.h, start_pt.v - (font_metric.ascent / 3));
		Line(end_pen.h - start_pt.h - 1, 0);
	}

	if (non_transparent_text) {
		RGBBackColor((RGBColor *)&pg->bk_color);
	}

	if (walker->superimpose.styles[blink_var])
		draw_box_blink(pg, walker, draw_position);
} 


/* DEFAULT TAB DRAW PROC (for leaders, etc.). Obviously machine-dependent. */

PG_PASCAL (void) pgTabDrawProc (paige_rec_ptr pg, style_walk_ptr walker, tab_stop_ptr tab,
		draw_points_ptr draw_position)
{
	memory_ref	      	filler;
	pg_short_t			leader;
	pg_char_ptr	     	filler_text;
	pg_char		    	check_filler[6];
	short		   		single_width, needed_width, filler_size, filler_width;
	short		   		underline_bits, i;
    pg_boolean			scaled_it;
    
	if ((leader = (pg_short_t)(tab->leader & 0x0000FFFF)) != 0) {

		for (i = 0; i < 5; ++i)
			check_filler[i] = (pg_char)leader;
	
		if (!(single_width = TextWidth((Ptr)check_filler, 0, 4) / 4))
			single_width = 1;
		
		if ((needed_width = draw_position->to.h - draw_position->from.h) < 1)
			needed_width = 1;

		filler_size = needed_width / single_width + 2;
		filler = MemoryAlloc(pg->globals->mem_globals, sizeof(pg_char), filler_size, 0);
		filler_text = UseMemory(filler);
      	
      	for (i = 0; i < filler_size; i++)
         	filler_text[i] = (pg_char) leader;

		pgScalePointSize(pg, walker, filler_text, filler_size, &scaled_it);
	
		while (filler_size) {
			
			filler_width = TextWidth((Ptr)filler_text, 0, filler_size);
			if (filler_width <= needed_width)
				break;
			
			--filler_size;
		}
		
		if (filler_size) {
	
			MoveTo(draw_position->to.h - filler_width, draw_position->to.v);
			DrawText(filler_text, 0, filler_size);
		}

		UnuseMemory(filler);
		DisposeMemory(filler);
	}
	
	underline_bits = SpecialUnderline(walker->cur_style);

	if (walker->cur_style->styles[underline_var])
		underline_bits |= FAKE_UNDERLINE;
	
	if (!walker->cur_style->styles[word_underline_var] && underline_bits) {
		Point	   start_pt;
		
		start_pt.h = pgLongToShort(draw_position->from.h);
		start_pt.v = pgLongToShort(draw_position->from.v);

		pgDrawSpecialUnderline(pg, start_pt, ((short) draw_position->to.h - draw_position->from.h),
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
	register pg_globals_ptr		 	globals;
	register pg_short_t			    flags;
	par_info_ptr					par;
	point_start_ptr					starts;
	co_ordinate					    draw_point, tab_point;
	long							scaled_width, show_invisibles;
	
	show_invisibles = ((pg->flags & (SHOW_INVIS_CHAR_BIT | PRINT_MODE_BIT))
									== SHOW_INVIS_CHAR_BIT);

	globals = pg->globals;
	starts = draw_position->starts;
	flags = starts->flags;

	if (show_invisibles) {

		draw_point = draw_position->from;
		tab_point = draw_position->to;
		scaled_width = starts->bounds.bot_right.h - starts->bounds.top_left.h;
		pgScaleLong(pg->scale_factor.scale, 0, &scaled_width);
		draw_point.h += scaled_width;
		
		set_invisible_symbol_style(pg);

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
			memory_ref		     special_locs;
			
			special_locs = pgGetSpecialLocs(pg, draw_position->block, draw_position->starts,
					length, extra, PLAIN_SCALE);

			locs = UseMemory(special_locs);

			local_offset = 0;
			draw_point.h = draw_position->from.h;
			
			while (local_offset < length) {

				if (walker->cur_style->procs.char_info(pg, walker, data, draw_position->block->begin,
					offset, (long)length, local_offset + offset, BLANK_BIT | CTL_BIT) == BLANK_BIT) {

					draw_point.h = (short) (draw_position->from.h + *locs);
					draw_special_char(pg, globals->space_invis_symbol, &draw_point);
				}

				++local_offset;
				++locs;
			}
			
			UnuseAndDispose(special_locs);
		}
		
		walker->cur_style->procs.install(pg, walker->cur_style, walker->cur_font,
				&walker->superimpose, walker->style_overlay, (globals->offscreen_port.machine_ref != 0));
	}
	
	if (length && (flags & NEW_PAR_BIT)) {
		long		html_style;
		
		par = walker->cur_par_style;

		if (par->class_info & BULLETED_LINE) {
			Rect			bullet;
			long			bullet_width;
			
			draw_point = draw_position->from;
			bullet.left = bullet.right = (short)(draw_point.h - 5);
			bullet.bottom = bullet.top = (short)draw_point.v;
			
			bullet_width = pg->globals->bullet_size;
			pgScaleLong(pg->scale_factor.scale, 0, &bullet_width);
			bullet.left -= bullet_width;
			bullet.top -= bullet_width;
			
			PenNormal();
			PenSize(2, 2);
			RGBForeColor((RGBColor *)&walker->cur_style->fg_color);
			
			switch (par->html_bullet) {
				
				case 2:
					FrameOval(&bullet);
					break;

				case 3:
					PaintRect(&bullet);
					break;
					
				case 4:
					FrameRect(&bullet);
					break;
				
				default:
					PaintOval(&bullet);
					break;
			}
		}
	}
}


/* DEFAULT FONT INIT PROC. This is machine-dependent. */

PG_PASCAL (void) pgInitFont (paige_rec_ptr pg, font_info_ptr info)
{
	short		   name_length;
	
	if (!info->name[0])
		return;
	
	info->environs &= (~FONT_USES_ALTERNATE);

	if (!info->platform)
		info->platform = PAIGE_GRAPHICS;

	pgFixFontName(info);
	//¥ TRS/OITC
	if ((name_length = FONT_NAME_SIZE(info->name) + 1) > (FONT_SIZE)) {
	
		name_length = FONT_SIZE - 1;
		info->name[0] = name_length;
	}
	
	//¥ TRS/OITC
	pgFillBlock(&info->name[name_length], (FONT_SIZE - name_length) * sizeof(pg_char), 0);

	info->environs &= (~(FONT_NOT_AVAIL | FONT_BEST_GUESS));

/* Validate the font record per platform: */
		
	if ( (info->environs & FONT_GOOD) || (pgIsRealFont(pg->globals, info, FALSE))) {

		set_mac_script_info(pg, (StringPtr) info->name, info, &info->family_id);
		info->environs |= FONT_GOOD;
	}
	else {
		
		if (pgIsRealFont(pg->globals, info, TRUE)) {
		
			set_mac_script_info(pg, (StringPtr) info->alternate_name, info, &info->alternate_id);
			info->environs |= FONT_USES_ALTERNATE;
		}
		else
		if (pgIsPlatformFont(info->platform) && (info->family_id || info->alternate_id)) {
			/* Allow Mac's font and script manager to do the right thing if possible */
			
			if (info->family_id)
			info->alternate_id = info->family_id;
			info->environs |= FONT_BEST_GUESS;
			//¥ TRS/OITC
			info->char_type = FontToScript(info->alternate_id);
			set_mac_lang_info(info);
		}
		else if (!info->alternate_id) {

			//¥ TRS/OITC
			info->alternate_id = pg->globals->def_font.family_id;
			info->char_type = pg->globals->def_font.char_type;
			info->environs |= FONT_NOT_AVAIL;
			set_mac_lang_info(info);
		}
	}

	if (GetScriptVariable(info->char_type, smScriptRight) & 0x000000FF)
		info->char_type |= RIGHT_FONT_FLAG;
}



/* pgInitDefaultFont must initialize a font record to the default settings.
NOTE: Upon entry, the entire record has been filled with zeros. Note that
for Macintosh, only the font name is initialized. This is because all other
fields get set when the general hook, "pgInitFont" gets called. */

PG_PASCAL (void) pgInitDefaultFont (const pg_globals_ptr globals, font_info_ptr font)
{
	short	   app_script;

	app_script = GetScriptManagerVariable(smSysScript);
	font->family_id = GetScriptVariable(app_script, smScriptAppFond);
	GetFontName(font->family_id, (StringPtr) font->name);
	font->environs = FONT_GOOD;
	font->platform = PAIGE_GRAPHICS;
	set_mac_lang_info(font);

	if (GetScriptVariable(font->char_type, smScriptRight) & 0x000000FF)
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
	register port_preserve	  PG_FAR *preserve_ptr;
	register GrafPtr		  port;
	pg_globals_ptr			  globals;
	long					  stack_size;
	RgnHandle				  clip_rgn;
	
	globals = pg->globals;

	if (verb == unset_pg_device) {
	
		stack_size = GetMemorySize(device->graf_stack) - 1;
		preserve_ptr = UseMemory(device->graf_stack);
		preserve_ptr += stack_size;
	
		globals->current_port = preserve_ptr->last_device;
		port = device->machine_var;
		restore_graf_items(preserve_ptr, port);
		
		if (preserve_ptr->prev_port)
			SetPort(preserve_ptr->prev_port);

		UnuseMemory(device->graf_stack);
		SetMemorySize(device->graf_stack, stack_size);
	}
	else {

		preserve_ptr = AppendMemory(device->graf_stack, 1, FALSE);
		preserve_ptr->last_device = globals->current_port;
		globals->current_port = device;
	
		GetPort(&preserve_ptr->prev_port);
		port = device->machine_var;
		SetPort(port);
		
		clip_rgn = port->clipRgn;
		
		if (!(pg->flags & NO_WINDOW_VIS_BIT))
			if (GetMemorySize(device->graf_stack) == 1)
				GetClip((RgnHandle)device->previous_items[ORIGINAL_CLIP]);

		if ((**clip_rgn).rgnSize == (sizeof(Rect) + sizeof(short))) {
			
			preserve_ptr->simple_clip = (**clip_rgn).rgnBBox;
			preserve_ptr->clip_rgn = NULL;
		}
		else {
		
			preserve_ptr->clip_rgn = (generic_var) pgCreateRgn();
			GetClip(preserve_ptr->clip_rgn);
		}
		
		preserve_ptr->origin.h = port->portRect.left;
		preserve_ptr->origin.v = port->portRect.top;
		
		GetPenState(&preserve_ptr->pen_state);
	
		preserve_ptr->text_font = port->txFont;
		preserve_ptr->text_face = port->txFace;
		preserve_ptr->text_size = port->txSize;
		preserve_ptr->text_mode = port->txMode;
	
		if (port->portBits.rowBytes & 0xC000) {
	
			preserve_ptr->text_fg = ((CGrafPtr) port)->rgbFgColor;
			preserve_ptr->text_bk = ((CGrafPtr) port)->rgbBkColor;
			
			if (pg->doc_info.attributes & COLOR_VIS_BIT) {
				color_value			use_color;
				
				if (bk_color)
					use_color = *bk_color;
				else
					use_color = pg->bk_color;
				
				RGBBackColor((RGBColor *) &use_color);
			}
			else
			if (bk_color)
				if (!pgTransColor(globals, bk_color))
					RGBBackColor((RGBColor *) bk_color);
		}
		else {
			
			preserve_ptr->old_fg = port->fgColor;
			preserve_ptr->old_bk = port->bkColor;
		}
	
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
    rectangle			    bits_rect;
	Rect				    target;
	GrafPtr					current_port, bits_port;
	memory_ref				bits_ref;
	short					rows, port_in_color;
	long					num_bits, target_bottom, container_num;
	pg_error				error;

	globals = pg->globals;
	if (line_start)
		container_num = line_start->r_num - 1;
	else
		container_num = 0;

	*real_bits_target = *target_area;
    globals->original_target = globals->offscreen_target = *target_area;

	offset_adjust->h = -real_bits_target->top_left.h;
	offset_adjust->v = -real_bits_target->top_left.v;       
	RectangleToRect(target_area, offset_adjust, &target);

    error = NO_ERROR;

	GetPort(&current_port);
	bits_port = globals->offscreen_port.machine_var;
	SetPort(bits_port);

	port_in_color = current_port->portBits.rowBytes & 0xC000;
	bits_ref = globals->offscreen_buf;

	rows = target.right / 8;
	rows += 1;
	if (rows & 1)
		++rows;
	target.right = rows * 8;
	
	target_bottom = (long)target.bottom;
	num_bits = target_bottom * rows;

	PG_TRY(globals->mem_globals) {
	
		if (globals->color_enable)  {
			PixMapHandle	    pix;
			GDHandle			max_device;
			Rect			    global_area;
			short			    pix_size, long_pix_size;

			pix = ((CGrafPtr)bits_port)->portPixMap;

			global_area = current_port->portRect;
			LocalToGlobal((Point*) &global_area.top);
			LocalToGlobal((Point*) &global_area.bottom);
			
			if ((max_device = GetMaxDevice(&global_area)) != NULL)
				CopyPixMap((**max_device).gdPMap, pix);
	
			pix_size = (long)(**pix).pixelSize;
			
			num_bits *= pix_size;

			if (num_bits > globals->max_offscreen) {
				
				num_bits = rows * pix_size;
				target.bottom = globals->max_offscreen / num_bits;
				target_bottom = (long)target.bottom;
				num_bits = (long)(target_bottom * rows);
				long_pix_size = (long)pix_size;
				num_bits *= long_pix_size;
				
				globals->offscreen_target.bot_right.v
					= globals->offscreen_target.top_left.v + (long)target.bottom;
				*real_bits_target = globals->offscreen_target;
			}

			SetMemorySize(bits_ref, num_bits);

			(**pix).baseAddr = UseMemory(bits_ref);
			(**pix).rowBytes = (rows * pix_size) | 0x8000;
			(**pix).bounds = target;

			if (port_in_color)
				//RGBBackColor(&((CGrafPtr) current_port)->rgbBkColor);
				RGBBackColor((RGBColor *)&pg->bk_color);
		}
		else {
			BitMap		  map;
	
			if (num_bits > globals->max_offscreen) {
				
				target_bottom = globals->max_offscreen / rows;
				target.bottom = (short)target_bottom;
				num_bits = target_bottom * rows;

				globals->offscreen_target.bot_right.v
					= globals->offscreen_target.top_left.v + (long)target.bottom;
				*real_bits_target = globals->offscreen_target;
			}

			SetMemorySize(bits_ref, num_bits);
			map.baseAddr = UseMemory(bits_ref);
			map.rowBytes = rows;
			map.bounds = target;
			SetPortBits(&map);
		}
	}
	
	PG_CATCH {
		
		error = -1;
	}

	PG_ENDTRY;

	if (error)
		SetPort((GrafPtr) globals->current_port->machine_var);
	else {
		PortSize(target.right, target.bottom);
		ClipRect(&target);
		pgRectToRgn(bits_port->visRgn, &target);
	}

/* True for both Windows and Mac: */
    
    if (error) {
    
		offset_adjust->h = offset_adjust->v = 0;
		globals->offscreen_enable = OFFSCREEN_UNSUCCESSFUL;
	
		if (draw_mode == bits_copy)
			pg->procs.container_proc(pg, container_num, real_bits_target, NULL,
				NULL, erase_rect_verb, NULL);    
    }
    else {
    	
    	globals->offscreen_enable = OFFSCREEN_SUCCESSFUL;
  
  		pgSetShapeRect(globals->offscreen_exclusion, NULL);
		RectToRectangle(&target, &bits_rect);
		pgEraseRect(globals, &bits_rect, NULL, NULL);
		
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
	register pg_globals_ptr 	globals;
	GrafPtr				 		current_port, bits_port;
	rectangle			        bits_rect, next_target;
	co_ordinate			     	offset_adjust;
	Rect				    	target, source;
	short				   		copy_mode;
	RgnHandle			        clip, exclusion_clip;
	RGBColor					bk_preserve, fg_preserve;

	globals = pg->globals;

	if (globals->offscreen_enable == OFFSCREEN_SUCCESSFUL) {
		
		offset_adjust.h = -globals->offscreen_target.top_left.h;
		offset_adjust.v = -globals->offscreen_target.top_left.v;
		bits_rect = globals->offscreen_target;
		pgOffsetRect(&bits_rect, offset_adjust.h, offset_adjust.v);
		pg->procs.bitmap_proc(pg, &globals->offscreen_port, TRUE,
			&globals->offscreen_target, &offset_adjust, text_offset);
	
		RectangleToRect(&globals->offscreen_target, NULL, &target);
		source = target;		
		source.right -= source.left;
		source.bottom -= source.top;
		source.top = source.left = 0;

		current_port = globals->current_port->machine_var;
		SetPort(current_port);
		bits_port = globals->offscreen_port.machine_var;
		
		switch (draw_mode) {

			case bits_copy:
				copy_mode = srcCopy;
				break;
			
			case bits_xor:
				copy_mode = srcXor;
				break;
			
			case bits_or:
				copy_mode = srcOr;
				break;
		}

		clip = pgCreateRgn();
		GetClip(clip);

		pgSectRgn(current_port->visRgn, clip, clip);

		if (!pgEmptyShape(globals->offscreen_exclusion)) {
			
			exclusion_clip = pgCreateRgn();

			ShapeToRgn(globals->offscreen_exclusion, 0, 0, NULL, 0, NULL, exclusion_clip);
			pgSectRgn(exclusion_clip, clip, exclusion_clip);
			
			if (!EmptyRgn(exclusion_clip))
				pgSubRgn(clip, exclusion_clip, clip);
			
			pgDisposeRgn(exclusion_clip);
		}

		bits_rect = globals->offscreen_target;

		if (globals->color_enable) {
			RGBColor		bk = {-1, -1, -1};
			RGBColor		fg = {0, 0, 0};
			
			fg_preserve = ((CGrafPtr) current_port)->rgbFgColor;
			bk_preserve = ((CGrafPtr) current_port)->rgbBkColor;

			RGBBackColor(&bk);
			RGBForeColor(&fg);
		}
		
		CopyBits(&bits_port->portBits, &current_port->portBits, &source,
					&target, copy_mode, clip);
		
		pgDisposeRgn(clip);

		UnuseMemory(globals->offscreen_buf);

		if (globals->color_enable) {
		
			RGBBackColor(&bk_preserve);
			RGBForeColor(&fg_preserve);
		}
	}
	
	globals->offscreen_enable = FALSE;

	if (globals->offscreen_target.bot_right.v < globals->original_target.bot_right.v) {
		
		next_target = globals->original_target;
		next_target.top_left.v = globals->offscreen_target.bot_right.v;

		pgPrepareOffscreen(pg, &next_target, new_target, new_offset, text_offset,
				line_start, draw_mode);

		return	FALSE;
	}

	if (!GetAccessCtr(globals->offscreen_buf))
		globals->offscreen_buf = ForceMinimumMemorySize(globals->offscreen_buf);

	return	TRUE;
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

PG_PASCAL (pg_region) pgScrollRect (paige_rec_ptr pg, rectangle_ptr rect, long distance_h,
		long distance_v, rectangle_ptr affected_area, short draw_mode)
{
	RgnHandle		    scroll_rgn;
	Rect			    scroll_rect;
	port_preserve	    *graf_info;

    RectangleToRect(rect, NULL, &scroll_rect);
	scroll_rgn = pgCreateRgn();
	
	if (pg->doc_info.attributes & NO_ERASE_SCROLL_BIT)
		smooth_scroll_rect(&scroll_rect, pgLongToShort(distance_h), pgLongToShort(distance_v), scroll_rgn);
	else
		ScrollRect(&scroll_rect, pgLongToShort(distance_h), pgLongToShort(distance_v), scroll_rgn);

	if (affected_area)
		RectToRectangle(&(**scroll_rgn).rgnBBox, affected_area);

	if (pg->port.scroll_rgn) {
		
		pgUnionRgn(scroll_rgn, pg->port.scroll_rgn, pg->port.scroll_rgn);
		pgDisposeRgn(scroll_rgn);
		scroll_rgn = pg->port.scroll_rgn;
	}

	return		scroll_rgn;
}

/* pgScaleGrafDevice should prepare whatever it has to for subsequent device scaling. For
Macintosh we do nothing except move the scaling to port.scale. */

PG_PASCAL (void) pgScaleGrafDevice (paige_rec_ptr pg)
{
	pg->port.scale = pg->scale_factor;
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
	GrafPtr				current_port;
	pg_scale_factor		scaler;
	rectangle			vis_bounds;
	RgnHandle			intersect_region, clip_region;
	shape_ref			use_vis;
	Rect				vis_r;
	pg_boolean			scale_vis_area;
	
	if ((pg->flags & PRINT_MODE_BIT) || (pg->doc_info.attributes & NO_CLIP_REGIONS))
		return;
	
	clip_region = pg->port.clip_rgn;

	current_port = (GrafPtr)pg->port.machine_var;
	
	if (!(use_vis = alternate_vis))
		use_vis = pg->vis_area;

	#ifdef PG_DEBUG
	if (!current_port)
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
		SetClip(intersect_region);
		pgDisposeRgn(intersect_region);
	}
	else {
		
		if (!(pg->flags & NO_WINDOW_VIS_BIT)) {
			
			intersect_region = pgCreateRgn();
			pgSectRgn(clip_region, (RgnHandle)pg->port.previous_items[ORIGINAL_CLIP], intersect_region);
			SetClip(intersect_region);
			pgDisposeRgn(intersect_region);
		}
		else
			SetClip(clip_region);
	}

	if (clip_verb == clip_with_none_verb)
		pgDisposeRgn(clip_region);
}


/* Added 4/12/94, pgSetMeasureDevice is called just before measuring character
widths. Its purpose is to QUICKLY set up some kind of screen device, if
necessary, so the main window isn't affected by subsequent font/style settings.
This may or may not apply to the machine, but for Macintosh we (a) save off
the current port, and (b) set the offscreen port. THIS IS INTENDED TO BE FAST.
(We used to call pgSetGrafDevice which is too slow).  */

PG_PASCAL (void) pgSetMeasureDevice (paige_rec_ptr pg)
{
	if (pg->globals->offscreen_port.access_ctr == 0)
		GetPort((GrafPtr*) &pg->globals->offscreen_port.machine_ref3);
	
	++pg->globals->offscreen_port.access_ctr;

	SetPort((GrafPtr) pg->globals->machine_const);
}


/* Added 4/12/94, pgUnsetMeasureDevice unsets whatever was set up in
pgSetMeasureDevice. For Macintosh we restore the previous port. */

PG_PASCAL (void) pgUnsetMeasureDevice (paige_rec_ptr pg)
{
	if ((--pg->globals->offscreen_port.access_ctr) == 0) {
		
		SetPort((GrafPtr) pg->globals->offscreen_port.machine_ref3);
		pg->globals->offscreen_port.machine_ref3 = 0;
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
	rectangle		       converted_r;
	Rect			    	r;
#pragma unused (globals)

    if (pgEmptyRect(rect))
	return;

	pgScaleRectToRect(scaling, rect, &converted_r, offset_extra);
	RectangleToRect(&converted_r, NULL, &r);

	EraseRect(&r);
}


/* pgScalePointSize must scale the text to the appropriate point size based on
the scaling factor in pg, if any. The point_size param holds the current point size.
Text and length hold the text that will be drawn. The function result is
the amount of extra pixels that need to be compensated (plus or minus) to make
up for any scaling errors.
Modified June 26 94, point size scaled is CURRENT font and walker param
is machine-specific (ignored for Mac but used for Windows). */

PG_PASCAL (short) pgScalePointSize (paige_rec_ptr pg, style_walk_ptr walker,
       pg_char_ptr text, long length, pg_boolean PG_FAR *did_scale) {
   	long            scaled_size, real_width, scaled_width;
	long            previous_font;
   	pg_fixed        char_extra = walker->cur_style->char_extra;         // PAJ

    *did_scale = FALSE;
	
   if (pg->scale_factor.scale && length) {
		
		if (pg->flags2 & WHOLE_KERNING_BIT)
			char_extra = pgRoundFixed(char_extra) & 0xFFFF0000;

       *did_scale = TRUE;
       scaled_size = current_font_size(pg, walker->cur_style);
       previous_font = walker->cur_style->machine_var;
       TextSize(scaled_size);
       CharExtra(char_extra);      // PAJ

        pgScaleLong(pg->scale_factor.scale, 0, &scaled_size);
        pgScaleLong(pg->scale_factor.scale, 0, &char_extra);
       real_width = TextWidth((Ptr)text, 0, length);
       TextSize(scaled_size);
       CharExtra(char_extra);      // PAJ

       scaled_width = TextWidth((Ptr)text, 0, length);
       pgScaleLong(pg->scale_factor.scale, 0, &real_width);

       return  real_width - scaled_width;
   }

   return  0;
}


#ifdef NO_OS_INLINE

PG_PASCAL (pg_fixed) pgMultiplyFixed (pg_fixed fix1, pg_fixed fix2)
{
	return  (pg_fixed)FixMul(fix1,fix2);
}

PG_PASCAL (pg_fixed) pgDivideFixed (pg_fixed fix1, pg_fixed fix2)
{
	return  (pg_fixed)FixDiv(fix1,fix2);
}

PG_PASCAL (pg_fixed) pgFixedRatio (short n, short d)
{
	return  (pg_fixed)FixRatio(n,d);
}

#endif

/* HandleToMemory -- converts a Handle to a memory ref of rec_size record size.
MACINTOSH-ONLY.  */

PG_PASCAL (memory_ref) HandleToMemory (pgm_globals *mem_globals, Handle h,
		pg_short_t rec_size)
{
	mem_rec_ptr			     mem_ptr;
	long				    h_size, new_size;

	PG_TRY (mem_globals) {
		
		h_size = GetHandleSize(h);
		new_size = (h_size / rec_size);
		h_size = new_size * rec_size;
		SetHandleSize (h, h_size + NON_APP_SIZE);
		pgFailError(mem_globals, MemError());
	}

	PG_CATCH {
		
		pgFailure(mem_globals, MemError(), 0);
	}
	
	PG_ENDTRY;

	pgMemInsert((memory_ref) h, 0, sizeof(mem_rec), h_size);
	mem_ptr = (mem_rec_ptr) *h;
	
	mem_ptr->rec_size = rec_size;
	mem_ptr->num_recs = mem_ptr->real_num_recs = new_size;
	mem_ptr->extend_size = 0;
	mem_ptr->access = 0;
	mem_ptr->purge = 0x08;
	mem_ptr->globals = mem_globals;
	mem_ptr->mem_id = mem_globals->current_id;

#ifdef PG_DEBUG
	{
		Ptr					     ptr;
		
		h_size = GetHandleSize(h) - CHECKSUM_SIZE;
		ptr = *h;
		ptr += h_size;

#ifdef PG_BIG_CHECKSUMS
	    {
		long				    checksum_ctr;

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

	InitMemoryRef(mem_globals, (generic_var) h);
	return (memory_ref) h;
}


/* MemoryToHandle -- A MACINTOSH-ONLY FUNCTION -- converts a memory_ref to a
Handle.  */

PG_PASCAL (Handle) MemoryToHandle (memory_ref ref)
{
	mem_rec			 	**ref_h;
	memory_ref		    **master_list;
	memory_ref_ptr	  	storage_list;
	pgm_globals_ptr	 	globals;
	long			    ref_index;
	Ptr				    src, dest;
	long			    h_size;
	
	ref_h = (mem_rec**) ref;

	PG_TRY ((**ref_h).globals) {
		
		h_size = GetByteSize(ref);

		UseMemory(ref);			 // Forces a re-load if purged
		HUnlock((Handle) ref);

		globals = (**ref_h).globals;
		ref_index = (**ref_h).master_index;
		master_list = (memory_ref**) globals->master_handle;
		storage_list = *master_list;
		storage_list[ref_index] = MEM_NULL;
	
		if ((ref_index < globals->next_master) || (globals->next_master == -1))
			globals->next_master = ref_index;

		if (h_size) {

			dest = (Ptr) *ref_h;
			src = dest + sizeof(mem_rec);
			BlockMove(src, dest, h_size);
		}

		SetHandleSize((Handle) ref, h_size);
		pgFailError(globals, MemError());
	}

	PG_CATCH {
		
		return  NULL;
	}
	
	PG_ENDTRY;

	return  (Handle) ref;
}


// Throw exception if memory error
PG_PASCAL (void) pgFailMemError(pgm_globals_ptr globals)
{
	pg_error	err = MemError();
	
	if (err) pgFailure( globals, err, 0);

}


/* pgIsRealFont returns TRUE if the font name exists in the system. If substitute
is non-NULL it is initialized to a name that does exist (used only for Windows).
For Windows, if the font precision info is zero it gets initialized. */

PG_PASCAL (pg_boolean) pgIsRealFont(pg_globals_ptr globals, font_info_ptr font, pg_boolean use_alternate)
{
	Str255  		temp_name;
	pg_char_ptr		font_name;
	short   		font_num;

	if (use_alternate)
		font_name = font->alternate_name;
	else
		font_name = font->name;

	GetFNum((StringPtr)font_name, &font_num);

	if (font_num == 0) {

		GetFontName(0, temp_name);
		
		if (IUEqualString((StringPtr)font_name, temp_name))
			return  FALSE;
	}

	return  TRUE;
}


/* Given an arbitrary point size, pgPointsizeToScreen returns what size to set
a font to achieve pointsize visual size. For example, if screen resolution is
96 dpi then a 12-point font needs to be slightly larger to look like 12 point.
Hence in this case, if pointsize = 72 this function returns 96. */
//¥ TRS/OITC

PG_PASCAL (pg_fixed) pgPointsizeToScreen (pg_ref pg, pg_fixed pointsize)
{
	paige_rec_ptr	   		pg_rec;
	pg_fixed				result;
	long					doc_resolution;
	short				     resolution;

	pg_rec = UseMemory(pg);
	doc_resolution = pg_rec->port.resolution >> 16;
	resolution = (short)doc_resolution;
	result = (pg_fixed) return_pointsize(pointsize, resolution);
	UnuseMemory(pg);
	
	return  (pgRoundFixed(result));
}

/* Given an arbitrary point size on screen, pgScreenToPointsize returns what size to set
a point size in a font really is. For example, if screen resolution is
96 dpi then a 12-point font needs to be slightly larger to look like 12 point.
Hence in this case, if screensize = 96 this function returns 72. */
//¥ TRS/OITC

PG_PASCAL (pg_fixed) pgScreenToPointsize (pg_ref pg, pg_fixed screensize)
{
	paige_rec_ptr	   	pg_rec;
	pg_fixed			result;
	long				doc_resolution;
	short				resolution;

	pg_rec = UseMemory(pg);
	doc_resolution = pg_rec->port.resolution >> 16;
	resolution = (short)doc_resolution;
	result = (pg_fixed) return_screensize((long)screensize, resolution);
	UnuseMemory(pg);
	
	return  (pgRoundFixed(result));
}


/* pgOSToPgColor converts an OS-specific color variable to a PAIGE color_value. For
Macintosh, *os_color is an RGBColor. For Windows, *os_color is a COLORREF. */

PG_PASCAL (void) pgOSToPgColor (const pg_plat_color_value PG_FAR *os_color, color_value_ptr pg_color)
{
	*((RGBColor PG_FAR *)pg_color) = *((RGBColor PG_FAR *)os_color);
	pg_color->alpha = 0;
}


/* pgColorToOS converts a PAIGE color_value to an OS-specific color variable.
For Macintosh this creates an RGBColor. For Windows this creates a COLORREF. */

PG_PASCAL (void) pgColorToOS (const color_value_ptr pg_color, pg_plat_color_value PG_FAR *os_color)
{
	*((RGBColor PG_FAR *)os_color) = *((RGBColor PG_FAR *)pg_color);
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
	Point			start_pt;
	Pattern	 		dotted;
	short			ascent;
	
	ascent = (short)style->ascent;
	start_pt = from_pt;

	if (draw_bits & OVERLINE_LINE)
		start_pt.v -= (ascent + 1);

	PenNormal();
	PenMode(patOr);

	MoveTo(start_pt.h, start_pt.v + 1);
	
	if (draw_bits & GRAY_UNDERLINE) {

		//¥ TRS/OITC
		make_mac_pattern((pg_char_ptr)dotted.pat, DOTTED_PAT);	
		PenPat(&dotted);
	}

	Line(distance - 1, 0);
	
	if (draw_bits & DOUBLE_UNDERLINE) {
		
		MoveTo(start_pt.h, start_pt.v + 3);
		Line(distance - 1, 0);
	}
}


/* pgTransLiterate converts upper to lower or lower to upper of all chars in
text depending on do_upperase verb. */

PG_PASCAL (void) pgTransLiterate (pg_char_ptr text, long length, pg_char_ptr target,
		pg_boolean do_uppercase)
{
	Handle		  	source_buf, target_buf;
	short		   	target_verb;
	
	if (do_uppercase)
		target_verb = smTransUpper;
	else
		target_verb = smTransLower;
	
	target_verb |= smTransNative;

	source_buf = NewHandle(length);
	target_buf = NewHandle(length);
	
	if (source_buf && target_buf) {
		
		BlockMove(text, *source_buf, length * sizeof(pg_char));
		Transliterate(source_buf, target_buf, target_verb, smMaskAscii);
		BlockMove(*target_buf, target, length);
	}

	if (source_buf)
		DisposeHandle(source_buf);
	if (target_buf)
		DisposeHandle(target_buf);
}


/* pgSystemTextDirection must return the default writing justification, which
should be one of the enums for direction (except system_direction). */

PG_PASCAL (short) pgSystemDirection (pg_globals_ptr globals)
{
#pragma unused (globals)
	
	if (GetSysDirection() < 0)
		return  right_left_direction;

	return  left_right_direction;
}


/* Returns the platform device */
//¥ TRS/OITC

PG_PASCAL (GrafPtr) pgGetPlatformDevice (graf_device_ptr the_device)
{	
	GrafPtr			current_port;
	
	if (!(current_port = (GrafPtr)the_device->machine_ref))
		GetPort(&current_port);

	return current_port;
}


/* pgOpenPrinter sets up the target device (which will receive pgPrint drawings)
to map images correctly. For Windows we do several things. For Mac we don't
really do anything. */

PG_PASCAL (void) pgOpenPrinter (paige_rec_ptr pg_rec, graf_device_ptr print_dev,
		long first_position, rectangle_ptr page_rect)
{
#pragma unused (first_position, page_rect)
	
	if (!print_dev->resolution)
		print_dev->resolution = pg_rec->resolution;
}


/* pgClosePrinter undoes whatever it did in pgOpenPrinter. For Mac we do nothing.
For Windows we re-invalidate the text and restore the old mapping mode. */

PG_PASCAL (void) pgClosePrinter (paige_rec_ptr pg_rec, graf_device_ptr print_dev)
{
#pragma unused (pg_rec, print_dev)
}

/* pgPrintDeviceChanged gets called when pgSetPrintDevice() is called from the
application. For Windows we walk through and invalidate all the char widths.
For Mac we do nothing. */

PG_PASCAL (void) pgPrintDeviceChanged (paige_rec_ptr pg)
{
#pragma unused (pg)
}


/* Compute if it is caret time */

PG_PASCAL (pg_boolean) pgIsCaretTime(paige_rec_ptr pg)
{
	unsigned long		timer, elapsed_time;
	pg_boolean			result = FALSE;
	
	timer = TickCount();
	elapsed_time = timer - pg->timer_info;
	
	if (elapsed_time >= GetCaretTime()) {
		
		result = TRUE;
		blink_mode = (++blink_mode) & 1;
		pg->timer_info = timer;
	}
	
	if (pg->flags & PRINT_MODE_BIT)
		blink = FALSE;
	else blink = result;
	
	return result;
}


/* Default Idle procedure - we just handle blinking character styles */

PG_PASCAL (void) pgIdleProc (paige_rec_ptr pg, short verb)
{
	style_info_ptr	info, info_1;
	long			i, count;
	pg_boolean		got_blinks = FALSE;
	
	if (verb == toggle_cursor)
	{
		if (!(pg->flags & DEACT_BITS) || ((pg->flags & DEACT_BITS) && (pg->flags2 & BLINK_ON_DEACTIVE)))
		{
			info = info_1 = UseMemory (pg->t_formats);
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
//				pg->procs.set_device(pg, set_pg_device, &pg->port, &pg->bk_color);
//				pgClipGrafDevice(pg, MEM_NULL, TRUE, FALSE);
			
				for (i = 0; i < count; i++, info_1++)
				{
					if (info_1->styles[blink_var])
					{
						style_run_ptr	run;
						long			j, count2;
						
						run = UseMemory (pg->t_style_run);
						count2 =  GetMemorySize(pg->t_style_run);
						
						for (j = 0; j < count2; j++, run++)
						{
							if (run->style_item == i)
							{
								long	beginning_offset, ending_offset;
								
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
				
//				pg->procs.set_device(pg, unset_pg_device, &pg->port, NULL);
			}
			
			UnuseMemory (pg->t_formats);
		}
	}
}


/********************************** LOCAL FUNCTIONS ***************************/


/* This initializes the four-byte global "char" (note that the 4 bytes are
already initialized to zeros). This function is used for DEFAULTS, which assumes
single byte values! */

static void init_global_char(pg_char_ptr global_char, pg_short_t value)
{
#ifdef GLOBALS_ARE_PSTRINGS
	global_char[0] = 1;
	global_char[1] = value;
#else
	global_char[0] = value;
#endif
}


/* This returns a 16-bit value from a global char string */

static pg_short_t global_char_to_int (pg_char_ptr global_char)
{
	register pg_short_t	     result_16;

#ifdef GLOBALS_ARE_PSTRINGS

#ifdef UNICODE
	if (global_char[0] > 1)
		result_16 = global_char[1];
#else
	result_16 = global_char[1];
	if (global_char[0] > 1) {
		
		result_16 <<= 8;
		result_16 |= global_char[2];
	}
#endif

#else
	
	result_16 = global_char[0];

#ifndef UNICODE
	if (global_char[1]) {
		result_16 <<= 8;
		result_16 |= global_char[1];
	}
#endif
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

static void make_graf_device (const pg_globals_ptr globals, const generic_var port, graf_device_ptr device)
{
	pgFillBlock(device, sizeof(graf_device), 0);
	device->machine_var = port;
	device->machine_ref = (long)port;
	device->graf_stack = MemoryAlloc(globals->mem_globals,
			sizeof(port_preserve), 0, 4);
	SetMemoryPurge(device->graf_stack, NO_PURGING_STATUS, FALSE);
	device->bk_color = globals->def_bk_color;
	device->clip_rgn = pgCreateRgn();
	device->previous_items[ORIGINAL_CLIP] = (long)pgCreateRgn();
	device->clip_info.change_flags = -1;

	device->resolution = 0x00480048;

	//¥New TRS/OITC
	
/* This code should not be here. It causes Paige to re-size the documents because it
sees a different resolution,  yet for Mac everything is Point-based. */

/*
	if (globals->color_enable) {
		CGrafPtr	color_port;

		color_port = (CGrafPtr)port;

		if (color_port->portVersion & 0xC000)
			device->resolution = (pgRoundFixed((*(color_port->portPixMap))->hRes) & 0xFFFF0000)
			| pgHiWord(pgRoundFixed((*(color_port->portPixMap))->vRes));
	}
*/

}


/* This function draws the caret in XOR mode */

static void draw_cursor (paige_rec_ptr pg, t_select_ptr select, short verb)
{
	point_start_ptr		starts;
	style_info_ptr		caret_style;
	text_block_ptr		block;
	rectangle			wrap_rect;
	co_ordinate			caret_top, caret_bot, scroll_adjust, repeat_offset;
	Point				top_caret, bottom_caret;
	long			    caret_height, style_offset;
	long			    container_proc_refcon;
	short			   	top_height, bottom_height, pen_mode;
	pg_short_t			r_num;
	
	pen_mode = patXor;

	switch (verb) {
		
		case activate_cursor:
			return;

		case deactivate_cursor:
			if (!(pg->flags & CARET_BIT))
				return;
			break;
		
		case update_cursor:
			//pen_mode = patOr;
			break;
	}

	if (select->flags & SELECTION_DIRTY)
		pgCalcSelect(pg, select);

	block = pgFindTextBlock(pg, select->offset, NULL, TRUE, TRUE);
	starts = UseMemoryRecord(block->lines, select->line, USE_ALL_RECS, TRUE);
	
	container_proc_refcon = 0;
	scroll_adjust = pg->scroll_pos;
	pgNegatePt(&scroll_adjust);
	pgAddPt(&pg->port.origin, &scroll_adjust);

	r_num = starts->r_num;

	pgCallContainerProc(pg, r_num, &scroll_adjust, clip_container_verb,
			 &pg->scale_factor, &container_proc_refcon); 

	if ((style_offset = select->offset) != 0)
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

	pgAddPt(&scroll_adjust, &caret_top);
	pgAddPt(&scroll_adjust, &caret_bot);

	pgScalePt(&pg->scale_factor, &scroll_adjust, &caret_top);
	pgScalePt(&pg->scale_factor, &scroll_adjust, &caret_bot);

	pgScaleLong(pg->scale_factor.scale, 0, &caret_height);

	top_height = caret_height;
	top_height /= 2;
	bottom_height = caret_height - top_height - 1;

	top_caret.h = pgLongToShort(caret_top.h);
	top_caret.v = pgLongToShort(caret_top.v);

	bottom_caret.h = pgLongToShort(caret_bot.h);
	bottom_caret.v = pgLongToShort(caret_bot.v);
	bottom_caret.v += (top_height + 1);

	UnuseMemory(block->lines);
	UnuseMemory(pg->t_blocks);

#ifdef PG_DEBUG
	if (verb == debug_cursor) {
		Boolean				px;
		pgm_globals_ptr		mem_globals;
		
		mem_globals = pg->globals->mem_globals;
		
		if (mem_globals->debug_flags & CURSOR_DEBUG) {
		
			px = GetPixel(top_caret.h, top_caret.v);
			
			if ( (px && !(pg->flags & CARET_BIT)) || (!px && (pg->flags & CARET_BIT)) )
				mem_globals->debug_proc(CARET_SYNC_ERR, pg->select);
		}
	}
	else
#endif
	if (verb == compute_cursor) {
		
		pg->stable_caret.h = top_caret.h;
		pg->stable_caret.v = top_caret.v + top_height;
		
		if (!pg->stable_caret.h)
			++pg->stable_caret.h;
	}
	else {

		PenNormal();
		PenMode(pen_mode);
		
		MoveTo(top_caret.h, top_caret.v);
		Line(0, top_height);
		
		MoveTo(bottom_caret.h, bottom_caret.v);
		Line(0, bottom_height);
	}

	pgCallContainerProc(pg, r_num, NULL, unclip_container_verb, NULL, &container_proc_refcon); 
}


/* This is the machine-dependent function to set all text fonts & styles. The
window defined in globals->current_port is affected.
Windows note: The font object is in style->machine_var, or it is created if
style->machine_var is zero.  */

static void install_machine_font (paige_rec_ptr pg, style_info_ptr style,
		font_info_ptr font, pg_boolean include_offscreen)
{
	register style_info_ptr	 the_style = style;
	pg_globals_ptr			globals;
	
	globals = pg->globals;

	if (font->environs & FONT_GOOD)
		TextFont(font->family_id);
	else
		TextFont(font->alternate_id);

	TextFace(the_style->machine_var & MAC_STYLE_MASK);
	TextSize(pgComputePointSize(pg, the_style));

	SpaceExtra((Fixed)the_style->space_extra);

	if (globals) {

		if (globals->color_enable) {
			pg_fixed		char_extra;
			
			RGBForeColor((RGBColor *) &the_style->fg_color);
			
			char_extra = the_style->char_extra;
			
			if (pg->flags2 & WHOLE_KERNING_BIT)
				char_extra = pgRoundFixed(char_extra) & 0xFFFF0000;

			CharExtra((Fixed)char_extra);
		}
	}
}



/* This draws special symbol the_char unless the_char is zero.  */

static void draw_special_char (paige_rec_ptr pg, pg_char_ptr the_char, co_ordinate_ptr point)
{
	if (the_char) {

		GrafPtr	 cur_port;
		
		GetPort(&cur_port);
		cur_port->txMode = srcOr;
		MoveTo(point->h, point->v);

		DRAW_SYMBOL(the_char);
	}
}



/* This sets the invisible char style, also scales point size if necessary */

static void set_invisible_symbol_style (paige_rec_ptr pg)
{
	long		    invis_pointsize;
	
	invis_pointsize = SPECIAL_SYMBOL_TXSIZE;
	pgScaleLong(pg->scale_factor.scale, 0, &invis_pointsize);

	TextSize((short)invis_pointsize);
	TextFace(0);
	TextFont(pg->globals->invis_font);
}

/* smooth_scroll_rect scrolls a rect without filling with the background. */

static void smooth_scroll_rect(Rect *rect, short distance_h, short distance_v, RgnHandle scroll_rgn)
{
	GrafPtr			cur_port;
	Rect			source_rect, target_rect, rgn_rect;
	RgnHandle		clip_rgn;

	GetPort(&cur_port);
	clip_rgn = NewRgn();
	RectRgn(clip_rgn, rect);
	
	source_rect = target_rect = rgn_rect = *rect;
	
	if (distance_h) {

		if (distance_h < 0) {
			
			source_rect.left -= distance_h;
			target_rect.right += distance_h;
			rgn_rect.left = rgn_rect.right + distance_h;
		}
		else {
			
			source_rect.right -= distance_h;
			target_rect.left += distance_h;
			rgn_rect.right = rgn_rect.left + distance_h;
		}
	}
	else
	if (distance_v) {
		
		if (distance_v < 0) {
			
			source_rect.top -= distance_v;
			target_rect.bottom += distance_v;
			rgn_rect.top = rgn_rect.bottom + distance_v;
		}
		else {
			
			source_rect.bottom -= distance_v;
			target_rect.top += distance_v;
			rgn_rect.bottom = rgn_rect.top + distance_v;
		}
	}
	
	if (source_rect.top < source_rect.bottom && source_rect.left < source_rect.right
			&& target_rect.top < target_rect.bottom && target_rect.left < target_rect.right) {
		RGBColor		old_color, white_color;
		unsigned short	port_in_color;
		CGrafPtr		port;
		
		SetPort(cur_port);
		
		if ((port_in_color = cur_port->portBits.rowBytes & 0xC000) != 0) {
			RGBColor		black_color = {0,0,0};
			
			port = (CGrafPtr)cur_port;
			old_color = port->rgbBkColor;
			white_color.red = white_color.green = white_color.blue = 0xFFFF;
			RGBBackColor(&white_color);
			RGBForeColor(&black_color);
		}
		
		CopyBits(&cur_port->portBits, &cur_port->portBits, &source_rect, &target_rect, srcCopy, clip_rgn);
		
		if (port_in_color)
			RGBBackColor(&old_color);

		RectRgn(scroll_rgn, &rgn_rect);
	}
	else
		CopyRgn(clip_rgn, scroll_rgn);

	DisposeRgn(clip_rgn);
}


/* draw_box_style draws a frame around the current line. It will not draw the
left edge if the previous style is also boxed (and we are not at beginning of
a line), nor will it draw the right side if next style is also boxed (and we
are not at end of line). */

static void draw_box_style (paige_rec_ptr pg, style_walk_ptr walker,
		draw_points_ptr draw_position)
{
	register style_info_ptr			style_base;
	rectangle						box_frame;
	Rect							frame;
	point_start_ptr					line_start;
	pg_short_t						style_index;
	pg_boolean						draw_edge;
	long							line_width, descent, next_offset;
	long							original_walker;

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
	
	PenNormal();
	PenSize(1, 1);
	MoveTo(frame.left, frame.top);
	LineTo(frame.right - 1, frame.top);
	MoveTo(frame.left, frame.bottom - 1);
	LineTo(frame.right - 1, frame.bottom - 1);

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

		MoveTo(frame.left, frame.top);
		LineTo(frame.left, frame.bottom - 1);
	}
	
	draw_edge = TRUE;
	next_offset = line_start[1].offset;
	next_offset += draw_position->block->begin;

	if (!(line_start->flags & LINE_BREAK_BIT))
		if (walker->next_style_run->offset == next_offset) {
		
		if (walker->next_style_run->offset < pg->t_length) {
			
			style_index = walker->next_style_run->style_item;
			draw_edge = (style_base[style_index].styles[boxed_var] == 0);
		}
	}

	if (draw_edge) {

		MoveTo(frame.right - 1, frame.top);
		LineTo(frame.right - 1, frame.bottom - 1);
	}

	pgSetWalkStyle(walker, original_walker);
}


/* This function returns TRUE if the character at the given offset is the last
of a particular World script run. The offset param is a global (absolute) offset. */

static pg_boolean last_char_of_script (style_walk_ptr walker, long offset)
{
	register style_run_ptr	  run;
	register style_info_ptr	 info;
	short				 	 font_index;

	if (!offset)
		return  FALSE;

	if (!(run = walker->next_style_run))
		return  FALSE;
	
	if ((offset + 1) == run->offset) {

		info = walker->style_base;
		info += run->style_item;
		if ((font_index = info->font_index) == DEFAULT_FONT_INDEX)
			font_index = 0;

		return  (walker->cur_font->char_type != walker->font_base[font_index].char_type);
	}

	return  FALSE;
}



/* current_font_size returns the current text point size.  */
/* Modified by PAJ, 7/26/95, to make it work with relative_point_var style */

static long current_font_size (paige_rec_ptr pg, style_info_ptr style) {
   pg_fixed        point, factor;
   long            result;

   point = style->point;

   if (style->styles[relative_point_var])
   {
       factor = pgFixedRatio(style->styles[relative_point_var], 12);
       point = pgMultiplyFixed(point, factor);
   }

   result = point >> 16;
   return  result;
}


/* This function sets port as current port then sets all items in graf_stuff. */

static void restore_graf_items (port_preserve PG_FAR *graf_stuff, GrafPtr port)
{
	register port_preserve	  *preserve;
	
	preserve = graf_stuff;
	SetPort(port);

	SetOrigin(preserve->origin.h, preserve->origin.v);

	if (preserve->clip_rgn) {
	
		SetClip(preserve->clip_rgn);
		pgDisposeRgn(preserve->clip_rgn);
	}
	else
		ClipRect(&preserve->simple_clip);
	
	SetPenState(&preserve->pen_state);

	port->txMode = preserve->text_mode;

	TextFont(preserve->text_font);
	TextFace(preserve->text_face);
	TextSize(preserve->text_size);

	if (port->portBits.rowBytes & 0xC000) {
		
		RGBForeColor(&preserve->text_fg);
		RGBBackColor(&preserve->text_bk);
	}
	else {
		
		port->fgColor = preserve->old_fg;
		port->bkColor = preserve->old_bk;
	}
}

/* This takes the initial short (16 bit) char_info value and sets any additional
bits according to sm_value, which was obtained from Script Manager. */

static long extra_char_info (long short_info, short sm_value)
{
	register long	   result;
	short			   sm_type, sm_class;
		
	result = short_info;
	sm_type = sm_value & smcTypeMask;
	sm_class = sm_value & smcClassMask;

	if (sm_type == smCharEuro)
		result |= EUROPEAN_BIT;
	if (sm_class == smPunctSymbol)
		result |= SYMBOL_BIT;
	
	if (sm_type == smCharPunct) {
	
		if (sm_class == smPunctNormal)
			result |= (PUNCT_NORMAL_BIT);
		if (sm_class & (smPunctRepeat | smPunctGraphic))
			result |= (OTHER_PUNCT_BIT);
	}

	if (sm_class == smPunctNumber)
		result |= NUMBER_BIT;
	if (sm_value & smCharUpper)
		result |= UPPER_CASE_BIT;
	
	return  result;
}

/* count_spaces determines how many space chars exist in the text. This only works for Roman
text.  */

static short count_spaces (pg_char_ptr text, short length)
{
	register pg_char_ptr		text_ptr;
	register short				ctr;
	short						qty;
	
	for (qty = 0, text_ptr = text, ctr = length; ctr; --ctr)
		if (*text++ == (pg_char)' ')
			qty += 1;
	
	return		qty;
}


/* make_mac_pattern creates a macintosh pattern based on pat value. This is
necessary so we don't use any QD globals. */

static void make_mac_pattern (pg_char_ptr pat_bytes, unsigned short pat)
{
	unsigned short	  *pat_ptr;
	short			   index;
	
	pat_ptr = (unsigned short *) pat_bytes;
	for (index = 0; index < 4; ++index)
		pat_ptr[index] = pat;
}


/* create_color_port creates a cGrafPort based on the max device in global_area.  */

static void create_color_port (CGrafPtr port, Rect *global_area)
{
	GDHandle			cur_device, old_device;

	old_device = GetGDevice();
	cur_device = GetMaxDevice(global_area);
	SetGDevice(cur_device);
	OpenCPort(port);
	SetGDevice(old_device);
}


//¥New TRS/OITC
/* return_pointsize returns what the pointsize should be based on resolution */

static pg_fixed return_pointsize (pg_fixed ptsize, short resolution)
{
	pg_fixed	point_ratio;

	if ((point_ratio = pgFixedRatio(resolution, 72)) != 0)
		return  pgMultiplyFixed(point_ratio, ptsize);
	
	return  ptsize;
}


//¥New TRS/OITC
/* return_screensize returns the opposite of return_pointsize */

static pg_fixed return_screensize (pg_fixed screensize, short resolution)
{
	pg_fixed	point_ratio;

	if (!resolution)
		return  screensize;

	if ((point_ratio = pgFixedRatio(72, resolution)) != 0)
		return  pgMultiplyFixed(point_ratio, screensize);
	
	return  screensize;
}


//¥New TRS/OITC
/* set_mac_script_info set script information from the script manager */

static void set_mac_script_info(paige_rec_ptr pg, StringPtr name, font_info_ptr info, short *font_num)
{
	long old_font_force = GetScriptManagerVariable(smFontForce);

	/* Setting font force flag is done to insure the correct font and script
		are set even if the mac system is set up in older compatibility mode 
		for hard coded fonts */
		
	SetScriptManagerVariable(smFontForce, FALSE);
	GetFNum(name, font_num);
	info->char_type = FontToScript(*font_num);
	set_mac_lang_info(info);
	SetScriptManagerVariable(smFontForce, old_font_force);
}


//¥New TRS/OITC
/* set_mac_lang_info set language information from info the script manager */

static void set_mac_lang_info(font_info_ptr info)
{
	if (((info->platform & GENERAL_PLATFORM_MASK) != GENERAL_MACINTOSH) &&
		(info->environs & FONT_NOT_AVAIL))
	{
		info->language = -1;
	}
	else
	{
		info->language = GetScriptVariable(info->char_type & SCRIPT_CODE_MASK, smScriptLang);
	}

	if ((info->char_type & SCRIPT_CODE_MASK) && ((info->char_type & SCRIPT_CODE_MASK) != smRoman))
		info->char_type |= NON_ROMAN_FLAG;
}


/* Blink the style */

static void draw_box_blink (paige_rec_ptr pg, style_walk_ptr walker,
		draw_points_ptr draw_position)
{
	rectangle						box_frame;
	Rect							frame;
	point_start_ptr					line_start;
	unsigned char 					old_hilite;
	long							line_width, descent, next_offset;
	
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
			
		old_hilite = LMGetHiliteMode();
		LMSetHiliteMode(50);
		InvertRect(&frame);
		LMSetHiliteMode(old_hilite);
	}
}


#endif

