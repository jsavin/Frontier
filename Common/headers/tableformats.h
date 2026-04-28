
/*	$Id$    */

/*
    SPDX-License-Identifier: MIT

    Copyright (c) 1992-2004 UserLand Software, Inc.
    Copyright (c) 2025-present Frontier contributors

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
*/

#ifndef tableformatsinclude
#define tableformatsinclude


#ifndef shellinclude

	#include "shell.h"

#endif


#ifndef langinclude

	#include "lang.h"

#endif

#ifndef opinclude

	#include "op.h"

#endif

#ifndef claylinelayoutinclude

	#include "claylinelayout.h"

#endif

#include "tabledisplay.h"



#define maxtablecols 3
#pragma pack(2)
typedef struct tytableformats { /*one of these for every window that's open*/
	
	hdlhashtable htable; // the parent table we are displaying
	
	hdloutlinerecord houtline; // the outline viewing the table
	
	short fontnum, fontsize, fontstyle; // only meaningful when outline was not saved and hasn't been recreated
	
	int64_t lnumcursor; // ditto

	int64_t vertcurrent; // ditto
	
	short ctcols;
	
	short defaultcolwidth;
	
	short colwidths [maxtablecols];
	
	short maxwidths [maxtablecols];
	
	Rect windowrect; /*the position & size of window last time table was packed*/
	
	Rect wholerect; /*the union of all rects that make up the table display*/
	
	Rect tablerect; /*the rectangle that the table is displayed in*/
	
	Rect titlerect; /*where the column titles are stored*/
	
	Rect iconrect;
	
	Rect kindpopuprect;
	
	Rect sortpopuprect;
	
	short sorttitlecol; /*the column that is underlined as the current sort*/
	
	short focuscol;
	
	short editcol; /*zero-based col when in text mode*/
	
	short undocol; /*column that last had it's undoglobals set*/
	
	Handle editval; /*when editing a value, the original text*/
	
	hdlheadrecord editnode; /*the node being edited*/
	
	hdlheadrecord lastbarcursor; /*so we can detect motion*/
	
	tylinelayout linelayout;
	
	tycomputedlineinfo computedlineinfo;
	
	boolean fliconenabled: 1; /*show the icon dimmed or not dimmed*/
	
	boolean fldirty; /*any changes made that aren't saved?*/
	
	boolean flactive;
	
	boolean flwindowsicons; /*the icons look like those used in Microsoft Windows apps*/
	
	boolean flneedresort; /*table needs to be resorted (after unpacking)*/
	
	boolean flprinting;
	
	long refcon;
	} tytableformats, *ptrtableformats, **hdltableformats;


/* This is an old version structure used only for conversion purposes*/

#pragma pack(push, 8)  /* Temporarily allow 8-byte alignment for int64_t fields */

typedef struct tyversion1tablediskrecord { /*packed version of tableformats, suitable for disk storage*/

	short versionnumber; /*this record is stored on disk*/
	
	short rowcursor, colcursor; /*locates the cursor*/
	
	short ctrows, ctcols; /*dimensions of the list record*/
	
	/*in v1 fontgarbage used to store the font number -- this was a mistake!*/
	
	short fontgarbage, fontsize, fontstyle; /*the display fontsizestyle*/
	
	tylinespacing linespacing;
	
	short vertmin, vertmax, vertcurrent; /*values for the scrollbars*/

	short horizmin, horizmax, horizcurrent; /*values for the scrollbars*/

	unsigned char _pad[6]; /*padding for 8-byte alignment of timecreated*/

	int64_t timecreated, timelastsave; /*maybe we'll use these at some later date?*/
	
	long ctsaves; /*the number of times this structure has been saved*/
	
	long sizerowarray, sizecolarray; /*sizes of two arrays stored at end of disk record*/
	
	short flags;

	diskfontstring fontname; /*a bit of Mac culture, font specified by a string, not a number*/

	unsigned short flags2;

	diskrect windowrect; /*the position & size of window last time table was packed*/
	
	short sortorder; /*up to the application to understand what this means*/
	
	char growtharea [20];
	
	/*the variable-length intarrays are tacked on at the end of this record*/
	} tyversion1tablediskrecord, *ptrversion1tablediskrecord, **hdlversion1tablediskrecord;

#pragma pack(pop)  /* Restore previous packing */

_Static_assert(offsetof(tyversion1tablediskrecord, timecreated) % 8 == 0, "tyversion1tablediskrecord.timecreated must be 8-byte aligned");
_Static_assert(offsetof(tyversion1tablediskrecord, timelastsave) % 8 == 0, "tyversion1tablediskrecord.timelastsave must be 8-byte aligned");


typedef struct tyversion2tablediskrecord { /*packed version of tableformats, suitable for disk storage*/
	
	short versionnumber; /*this record is stored on disk*/
	
	short recordsize;
	
	diskfontstring fontname; // only meaningful when outline was not saved and hasn't been recreated
	
	short fontsize, fontstyle; // ditto
	
	short lnumcursor; // ditto
	
	short vertcurrent; // ditto
	
	short ctcols; /*dimensions of the list record*/
	
	short colcursor; /*locates the cursor*/
	
	short colwidths [maxtablecols];
	
	diskrect windowrect; /*the position & size of window last time table was packed*/
	
	boolean savedoutline; /*is an outline packed along with this record? (only if expanded)*/
	
	boolean savedlinelayout; //is a clay linelayout packed along with this record?
	
	short lnumcursor_hiword; //5.1.3
	
	short vertcurrent_hiword; //5.1.3
	
	short growtharea [8];
	} tyversion2tablediskrecord, *ptrversion2tablediskrecord, **hdlversion2tablediskrecord;
#pragma options align=reset

/*globals*/
	
extern WindowPtr tableformatswindow; 

extern hdltableformats tableformatsdata; 

extern hdlwindowinfo tableformatswindowinfo; 


/*prototypes*/

extern boolean tablepushformats (hdltableformats);

extern boolean tablepopformats (void);

extern void tabledirty (void);

extern short tablegetcolwidth (short);

extern boolean tablesetcolwidth (short, short, boolean);

extern short tablesumcolwidths (short, short);

extern short tabletotalcolwidths (void);

extern short tableavailwidth (void);

extern boolean tablerecalccolwidths (boolean);

extern boolean newtableformats (hdltableformats *);

extern void disposetableformats (hdltableformats);

extern boolean tablenewformatsrecord (hdlhashtable, Rect, hdltableformats *);

extern boolean tableprepareoutline (hdltableformats);

extern boolean tablepackformats (Handle *);

extern boolean tableunpackformats (Handle, hdltableformats);

extern void tabledisposeoutline (hdltableformats);

extern boolean tableoutlineneedssaving (void);

#endif


