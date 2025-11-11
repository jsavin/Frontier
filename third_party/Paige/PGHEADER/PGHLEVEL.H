/* This header file provides function prototypes for a Mac-specific high-level
wrapper around some of the otherwise complicated functions in PAIGE such as
setting styles, fonts, or whatever.

Note that most of the functions herein are specific to Macintosh, the reason
being that the code they replace appear "complex" due to the portability of
PAIGE. For example, PAIGE is not suppose to know about QuickDraw styles, but
in high-level wrapper we can set a QuickDraw style bit.

We will create a similar set of utilities for Windows and other platforms.

The purpose of this interface is to help minimize support for our team and to
enhance PAIGE library sales.

Copyright 1994 by DataPak Software, Inc.  PAIGE is a DataPak trademark.  */

/* Updated May 1995 for ease of use with opendoc by OITC and more functionality */

#ifndef PGHLEVEL_H
#define PGHLEVEL_H

#include "Paige.h"

#ifdef applec
#include	<Quickdraw.h>
#include	<Events.h>
#include	<Memory.h>
#include	<Desk.h>
#include	<Packages.h>
#include	<String.h>
#include	<OSUtils.h>
#include	<ToolUtils.h>
#endif

#ifdef __cplusplus
extern	"C" {
#endif

typedef PG_FN_PASCAL (void, ChangeTableAttributesCallbackPtr)(	short row, short column,
															tab_stop *tab, tab_stop *tab_mask,
															long user_value);

extern PG_PASCAL (void) pgSetFontByName (pg_ref pg, const pg_font_name_ptr font_name,
		const select_pair_ptr selection_range, pg_boolean redraw);
extern PG_PASCAL (pg_boolean) pgGetFontByName (pg_ref pg, pg_font_name_ptr font_name);
extern PG_PASCAL (pg_boolean) pgGetTextColor (pg_ref pg, pg_plat_color_value PG_FAR *color);
extern PG_PASCAL (pg_boolean) pgGetTextBKColor (pg_ref pg, pg_plat_color_value PG_FAR *color);
extern PG_PASCAL (void) pgSetTextColor (pg_ref pg, const pg_plat_color_value PG_FAR *color,
		const select_pair_ptr selection_range, pg_boolean redraw);
extern PG_PASCAL (void) pgSetTextBKColor (pg_ref pg, const pg_plat_color_value PG_FAR *color,
		const select_pair_ptr selection_range, pg_boolean redraw);
		
extern PG_PASCAL (void) pgSetStyleBits (pg_ref pg, long style_bits, long set_which_bits,
		const select_pair_ptr selection_range, pg_boolean redraw);
extern PG_PASCAL (void) pgSetPointSize (pg_ref pg, short point_size,
		const select_pair_ptr selection_range, pg_boolean redraw);
extern PG_PASCAL (void) pgGetStyleBits (pg_ref pg, long PG_FAR *style_bits,
		long PG_FAR *consistent_bits);
extern PG_PASCAL (pg_boolean) pgGetPointsize (pg_ref pg, short PG_FAR *point_size);
extern PG_PASCAL (void) pgApplyToSelectedCells(memory_ref pg, select_pair_ptr selection,
												ChangeTableAttributesCallbackPtr callback,
												long user_value);



#ifdef MAC_PLATFORM

extern PG_PASCAL (void) pgSetFontByNum (pg_ref pg, short font_num,
		const select_pair_ptr selection_range, pg_boolean redraw);
extern PG_PASCAL (pg_boolean) pgGetFontByNum (pg_ref pg, short *font_num);

#endif

#ifdef WINDOWS_PLATFORM
extern PG_PASCAL (void) pgSetFontByLog (pg_ref pg, const LOGFONT PG_FAR *log_font,
		const select_pair_ptr selection_range, pg_boolean redraw);
extern PG_PASCAL (pg_boolean) pgGetFontByLog (pg_ref pg, LOGFONT PG_FAR *log_font);

#endif

// Alternate hooks one can use:

PG_PASCAL (pg_boolean) pgContainerParProc (paige_rec_ptr pg, select_pair_ptr boundary);
PG_PASCAL (void) PageLines (paige_rec_ptr pg, style_walk_ptr walker,
		long line_number, long par_number, text_block_ptr block, point_start_ptr first_line,
		point_start_ptr last_line, point_start_ptr previous_first, point_start_ptr previous_last,
		co_ordinate_ptr offset_extra, rectangle_ptr vis_rect, short call_verb);
PG_PASCAL (void) pgAdjustToTop (paige_rec_ptr pg, pg_measure_ptr measure_info,
		point_start_ptr line_starts, pg_short_t num_starts,
		rectangle_ptr line_fit, par_info_ptr par_format);

#ifdef __cplusplus
	}
#endif

#endif
