
/*	$Id$    */

/*
    SPDX-License-Identifier: MIT

    Copyright (c) 1992-2004 UserLand Software, Inc.
    Copyright (c) 2025-2026 Frontier contributors

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

#define clayinclude


#include <standard.h>
typedef bigstring tyfoldername;
#define fastunicaseequalstrings equalidentifiers

#include "op.h"
#include "tableeditor.h"
#include "claylinelayout.h"


#define maxtablecols 3

#define outlinetopinset 3
#define outlinebottominset 0
#define outlineleftinset 3
#define outlinerightinset 0


#define browserfiletype 'FSYS'
#define outlinerfiletype 'OUTL'
#define processlistfiletype 'PROC'
#define scriptfiletype '2CLK'
#define textfiletype 'TEXT'
#define styledtextfiletype 'WPTX'
#define msgviewerfiletype 'MSGV'
#define msgeditorfiletype 'MSGE'
#define bbsoutlinefiletype 'MSGO'
#define messagereffiletype 'MREF'
#define bookmarkfiletype 'BOOK'
#define websitefiletype 'WBST'



#define ctwindowtypes 7
	

/*these macros map our names onto the app0 - app4 bits in each hdlheadrecord*/
	
	#define flnodeisfolder appbit2
	#define flnodeunderlined appbit3
	#define flnodeonscrap appbit4
	#define tmpbit2 appbit5
	#define flnodeneedsbuild appbit6
	#define flnewlyinsertednode appbit7


#define maxicons 15 /*10/28/93 DW: used to be 5*/

typedef boolean (*tygetmenucallback) (short);

typedef boolean (*tyidlecallback) (short);

typedef boolean (*tymenuselectcallback) (short, short);

typedef boolean (*tybuttonhitcallback) (short);

typedef struct tyclaystatusicon {
	
	hdlstring foldername; /*name of the icon's special folder, empty string if it's computed*/
	
	hdlstring helpstring; /*displayed in message area when icon is activated*/
	
	hdlstring buttontext; /*if it's a text button, this is the text inside the button*/
	
	Rect iconrect; /*the rectangle that the icon occupies*/
	
	short iconresnum; /*the resource id of the icon family*/
	
	MenuHandle hmenu; /*nil if icon has no popup*/
	
	boolean flpopup; /*if true, mouse click pops up a menu*/
	
	boolean flsingleclickpopup; /*if true, user must hold down mouse to get popup*/
	
	boolean fldisposemenu; /*if true, we dispose of the menu handle after running it*/
	
	boolean flbreakafter; /*start a new subgroup after this icon*/
	
	boolean fltextbutton; /*if true it's a text button, otherwise it's an icon*/
	
	boolean flnofolder; /*if true, the icon doesn't have a related sub-folder in the Prefs folder*/
	
	boolean fltoggles; /*if true, the icon toggles pressed and not pressed. effects interaction, a little*/
	
	boolean flpressed; /*if true, we display the button in its pressed state*/
	
	boolean fldisabled; /*if true, the button is disabled*/
	
	boolean flcustomrect; /*if true, we don't compute the icon's rect*/
	
	short idpopupmenu; /*the resource id of the popup menu*/
	
	tygetmenucallback getmenucallback;
	
	tymenuselectcallback menuselectcallback;
	
	tybuttonhitcallback buttonhitcallback;
	
	tyidlecallback idlecallback;
	
	long refcon; /*some icons want to keep their own data around*/
	} tyclaystatusicon;
	

typedef boolean (*tyclaycallback) (void);

typedef boolean (*tyoutlinecallback) (hdloutlinerecord);

typedef boolean (*tyshortstarcallback) (short *);

typedef boolean (*tymenuhitcallback) (short, short);


typedef struct tyclaydata { /*one of these for every window that's open*/
	
	hdlhashtable htable; // the parent table we are displaying
	
	hdloutlinerecord houtline; // the outline viewing the table
	
	hdltableformats hformats;
	
	tylinelayout linelayout; 
	
	tycomputedlineinfo computedlineinfo;
	
	short ctcols;
	
	short colwidths [maxtablecols];
	
	short maxwidths [maxtablecols];
	
	short cticons; /*the number of icons in the status bar of this window*/
	
	/*start tyextrainfo*/
	
	Rect wholerect; /*the union of all rects that make up the table display*/
	
	Rect tablerect; /*the rectangle that the table is displayed in*/
	
	Rect titlerect; /*where the column titles are stored*/
	
	Rect seprect; /*the area separating the table display from the titles*/
	
	Rect iconrect;
	
	Rect kindpopuprect;
	
	Rect sortpopuprect;
	
	short focuscol;
	
	short editcol; /*zero-based col when in text mode*/
	
	Handle editval; /*when editing a value, the original text*/
	
	hdlheadrecord editnode; /*the node being edited*/
	
	short ixicontitle; /*stringlist index of icon title*/
	
	/*end tyextrainfo*/
	
	boolean fliconenabled: 1; /*show the icon dimmed or not dimmed*/
	
	boolean flautocreated: 1; /*we're editing a auto-created value*/
	
	boolean fltexticons; /*we do different button alignment for these windows*/
	
	boolean flwindowsicons; /*the icons look like those used in Microsoft Windows apps*/
	
	boolean fluseslinelayout; /*uses claylinelayout.c routines to draw the outline*/
	
	boolean flfilebrowser; /*it's an outliner used to browse nodes which are files*/
	
	tyclaystatusicon iconarray [maxicons];
	
	hdlheadrecord lastbarcursor; /*so we can detect motion*/
	
	boolean flresetinfowindows; /*if true, force a reload of the info windows*/
	
	boolean iconhighlighted; /*draw the icon as if it were selected*/
	
	boolean flbitmap; /*true if an offscreen bitmap is open*/
	
	boolean forkedwindow; /*if true it's a fork off another window*/
	
	boolean immediatesystemidle; /*set true as the window is activated*/
	
	boolean processingscriptevent; /*don't allow user events to break the idle handler*/
	
	boolean eraseonupdate; /*true until the next update event is handled*/
	
	boolean fljumponidle;
	
	tyclaycallback keystrokecallback;
	
	tyclaycallback idlecallback;
	
	tyoutlinecallback exportscrapcallback;
	
	tyclaycallback initstatusbarcallback;
	
	tyshortstarcallback getstatusbarwidthcallback;
	
	tyclaycallback drawstatusbarcallback;
	
	tyclaycallback setcustomrectscallback;
	
	tyclaycallback statusclickcallback;
	
	tyclaycallback cursorinstatusbarcallback;
	
	tymenuhitcallback menuhitcallback;
	
	tyclaycallback beforeclosewindowcallback;
	
	tyclaycallback handleverbcallback, handlefastverbcallback;
	
	Handle hmoredata; /*non-outline data handle*/
	} tyclaydata, **hdlclaydata;
	
	
	

boolean claybrowserkeystroke (void); /*used by browser & process windows*/

//boolean claypushwindowglobals (tywindowtype, boolean);

void claypopwindowglobals (void);

//boolean clayfindwindow (tywindowtype, boolean, hdlwindowinfo *);

short claymsgclick (short, short);

void clayidleanalysis (boolean *, boolean *, boolean *);

void clayidleforall (void);

boolean claygetfrontwindow (hdlwindowinfo *);

boolean claygetfrontoutline (hdloutlinerecord *);

boolean claywindowcontainsoutline (hdlclaydata);

boolean clayfrontwindowcontainsoutline (void);

boolean clayopendoc (FSSpec *);

boolean claypushpopupitem (short, bigstring, boolean);

boolean claynewappwindow (OSType);
