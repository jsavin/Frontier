
/*	$Id$    */

/******************************************************************************

    UserLand Frontier(tm) -- High performance Web content management,
    object database, system-level and Internet scripting environment,
    including source code editing and debugging.

    Copyright (C) 1992-2004 UserLand Software, Inc.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

******************************************************************************/

/*
about.c -- a generic "about" box for any Macintosh program written in Lightspeed C.

Make sure you have a STR# resource number 128, with exactly six strings in it.

Not too big, kind of sexy, and certainly better than nothing!
*/ 

	#include <standard.h>

#include "quickdraw.h"
#include "strings.h"
#include "bitmaps.h"
#include "cursor.h"
#include "dialogs.h"
#include "icon.h"
#include "file.h"
#include "font.h"
#include "kb.h"
#include "memory.h"
#include "mouse.h"
#include "ops.h"
#include "popup.h"
#include "resources.h"
#include "scrollbar.h"
#include "smallicon.h"
#include "textedit.h"
#include "windows.h"
#include "windowlayout.h"
#include "zoom.h"
#include "shell.h"
#include "shellprivate.h"
#include "shellhooks.h"
#include "about.h"
#include "tablestructure.h"
#include "cancoon.h"
#include "cancooninternal.h"
#include "process.h"
#include "processinternal.h"


	#define idfrontiericon	128



static long aboutopenticks; /*so we can tell how long it's been up*/


typedef struct tyaboutrecord {
	
	Rect messagearea;
	
	Rect aboutarea;
	
	boolean flbootsplash;
	
	boolean flbigwindow;
	
	boolean flextrastats;

	long refcon;
	} tyaboutrecord, *ptraboutrecord, **hdlaboutrecord;


static hdlaboutrecord aboutdata = nil;

static WindowPtr aboutwindow = nil;

static hdlwindowinfo aboutwindowinfo;

static boolean flsessionstats = false;


static hdlaboutrecord displayedaboutdata = nil;

static WindowPtr aboutport = nil;

static boolean flhavemiscrect = false;

static Rect miscinforect;


static bigstring bstheadinfo = "";

static bigstring bsmiscinfo = "";


#define agentpopupwidth (popuparrowwidth + 4)

#define msgtopinset 3

#define msgbottominset 3

#define minmsgheight (heightsmallicon + 4)

#define msgborderpix 5

#define msgvertgap 4

#define aboutlineheight 14

#define aboutlinewidth 300

#define aboutvertstart 0

#define aboutvertinset 6

#define aboutvertgap 0

#define abouthorizgap 20

#define abouthorizinset 12

#define aboutrowsStats 8

#define aboutrectheightStats (aboutvertinset * 2 + aboutvertstart + aboutrowsStats * aboutlineheight + aboutvertgap)

#define aboutrowsNoStats 3

#define aboutrectheightNoStats (aboutvertinset * 2 + aboutvertstart + aboutrowsNoStats * aboutlineheight + aboutvertgap)

#define abouticonsize 32

#define versionwidth 56

#define minaboutwidth  (aboutlinewidth + abouthorizgap + 2 * abouthorizinset)

#define agentmenuhorizgap 10

static byte * aboutstrings [] = {

	BIGSTRING ("\x2e" "Powerful cross-platform web content management"), /*6.1 AR*/
	 
    /*"\x25" "Powerful cross-platform web scripting",*/
	
