/* 
	Paige Windows Break API file
	
	File:		pgBreak.h

	by T. R. Shaw

	Copyright © 1995 OITC, Inc.
	All rights reserved.
	
	pgBreak.h - Word break support for Windows.

	This software has been privately developed and falls within 
	DFARS 252.227-7013(c)(1) and associated regulations and it or its
	derivatives may not be used outside of the License agreement with 
	T. R. Shaw and OITC.  Copyright may not be removed.
	
	oitc@iu.net

	Revision History
	07/03/95	1.0b1	TRS - Initial release
	
	Useage:
	
	Add to your resource file entries of type, LANG_BREAK_NAME, with the correct
	resource ID form lang_break_types containing the pg_break_rsrc structure.
	
	For each pg_break_data entry, set c[0] to the character for single byte characters
	and set c[0] & c[1] to the character for double byte characters, and set flags to
	LEADING_CHARACTERS, FOLLOWING_CHARACTERS, or OVERFLOW_CHARACTERS.
	
	Now in your main program set wordbreak_proc as:
	
			global_vars->def_hooks.wordbreak_proc = pgBreakInfo;

*/


#ifndef PBGREAK_H
#define PBGREAK_H

#include "Paige.h"
#include "pgRsrc.h"
#include "pgTraps.h"

#ifdef MAC_PLATFORM
#define LANG_BREAK_NAME			'LBrk'
#endif
#ifdef WINDOWS_COMPILE
#define LANG_BREAK_NAME			"LBrk"
#endif

enum lang_break_types {
	JAPAN_SINGLE_BREAK_ID = 1,
	JAPAN_DOUBLE_BREAK_ID,
	TRAD_CHINA_SINGLE_BREAK_ID,
	TRAD_CHINA_DOUBLE_BREAK_ID,
	SIMP_CHINA_SINGLE_BREAK_ID,
	SIMP_CHINA_DOUBLE_BREAK_ID,
	KOREA_SINGLE_BREAK_ID,
	KOREA_DOUBLE_BREAK_ID
};


#define LEADING_CHARACTERS		0x0001
#define FOLLOWING_CHARACTERS	0x0002
#define OVERFLOW_CHARACTERS		0x0004

#define NO_PROCESS_BREAK_DATA	STYLE_IS_CONTROL | GROUP_CHARS_BIT | STYLE_IS_CUSTOM | IS_NOT_TEXT_BIT | EMBED_SUBSET_BIT


struct pg_break_data {
	pg_short_t		flags;
	pg_byte			c[2];
};
typedef struct pg_break_data pg_break_data;

struct pg_break_rsrc {
	pg_short_t		count;
	pg_break_data	break_info[1];
};
typedef struct pg_break_rsrc pg_break_rsrc;
typedef pg_break_rsrc PG_FAR *pg_break_rsrc_ptr;


PG_PASCAL (long) pgBreakInfo (paige_rec_ptr pg, pg_byte_ptr the_char,
	short charsize, style_info_ptr style, font_info_ptr font,
	long current_settings);

#endif

