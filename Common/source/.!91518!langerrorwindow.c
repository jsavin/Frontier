
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

#include "frontier.h"
#include "standard.h"

#include "bitmaps.h"
#include "frontierconfig.h"
#include "cursor.h"
#include "font.h"
#include "icon.h"
#include "kb.h"
#include "memory.h"
#include "mouse.h"
#include "ops.h"
#include "popup.h"
#include "resources.h"
#include "quickdraw.h"
#include "scrap.h"
#include "strings.h"
#include "textedit.h"
#include "windowlayout.h"
#include "frontierwindows.h"
#include "shell.rsrc.h"
#include "shellhooks.h"
#include "lang.h"
#include "langinternal.h"
#include "cancoon.h"

static void langerroractivate (boolean flactivate); /*forward*/

#pragma pack(2)
typedef struct tylangerrorrecord {
	
	bigstring bserror; /*the error message*/
	
	bigstring bslocation; /*line number and character number of the error*/
	
	Rect iconrect; /*location of the icon, for mouse clicks, cursor adjustment*/
	
	Rect textrect; /*location of the error text, for updates, activates*/
	
	unsigned short lnum, cnum; /*location of error, passed to callback routine*/
	
	//callback errorcallback; /*routine to call when user hits script button*/
	
	//long errorrefcon; /*the info we pass him*/
	
	hdlerrorstack herrorstack;
	
	boolean flcallbackconsumed: 1; /*if true do nothing on script button hit*/
	
	boolean flactive: 1; /*determines whether message is drawn as active or inactive*/
	} tylangerrorrecord, *ptrlangerrorrecord, **hdllangerrorrecord;
#pragma options align=reset


WindowPtr langerrorwindow = nil;

hdlwindowinfo langerrorwindowinfo = nil;

hdllangerrorrecord langerrordata = nil;

static hdllangerrorrecord pendingerrordata = nil;

static long inhibiterrorclear = 0; // 5.0.2b3



static boolean langerrorgetwindowrect (Rect *rwindow) {
	
	return (ccgetwindowrect (ixlangerrorinfo, rwindow));
	} /*langerrorgetwindowrect*/
	
	
static boolean langerrorclose (void) {
	
	ccsubwindowclose (langerrorwindowinfo, ixlangerrorinfo);
	
	return (true);	// even if we couldn't save our position
	} /*langerrorclose*/


static boolean langerrorcopy (void) {
	
	/*
	9/11/92 dmb: users pointed out that this might be useful for reporting 
	bugs and problems...
	*/
	
	bigstring bserror;
	Handle htext;
	
	copystring ((**langerrordata).bserror, bserror);
	
	if (!newtexthandle (bserror, &htext))
		return (false);
	
	return (shellsetscrap (htext, textscraptype, (shelldisposescrapcallback) disposehandle, nil));
	} /*langerrorcopy*/


static boolean langerrornewwindow (void) {
	
	/*
	5.0.2b3 dmb: added inhibiterrorclear logic to make sure zooming doesn't 
	clear us
	
	6.2b6 AR: Extended inhibiterrorclear protection so that langerrorlcear doesn't interfer
	*/
	
	hdlwindowinfo hw;
	WindowPtr w;
	bigstring bstitle;
	Rect rzoom, rwindow;
	hdlwindowinfo hparent;
	tylangerrorrecord **hdata;
	boolean fl = false;
	
	++inhibiterrorclear; // protect us from other threads while we're still busy creating the window
	
	if (!shellgetfrontrootinfo (&hparent)) /*our parent is the frontmost root window*/
		goto exit;
			
	shellgetwindowcenter (hparent, &rzoom);
	
	if (pendingerrordata != nil) {
		
		hdata = pendingerrordata;
		
		pendingerrordata = nil;
		}
	else {
		
		if (!newclearhandle (sizeof (tylangerrorrecord), (Handle *) &hdata))
			goto exit;
		}
	
	langerrorgetwindowrect (&rwindow);
	
	getstringlist (langerrorlistnumber, langerrortitlestring, bstitle);
	
	if (!newchildwindow (idlangerrorconfig, hparent, &rwindow, &rzoom, bstitle, &w)) {
		
		disposehandle ((Handle) hdata);
		
		goto exit;
		}
		
	getwindowinfo (w, &hw);
	
	(**hw).hdata = (Handle) hdata;
	
	ccnewsubwindow (hw, ixlangerrorinfo);
	
	windowzoom (w);

	fl = true;
	
exit:
	
	--inhibiterrorclear;
	
	return (fl);
	} /*langerrornewwindow*/


static boolean langerrorsetrects (void) {
	
	register hdllangerrorrecord hle = langerrordata;
	Rect rcontent;
	Rect r;
	
	rcontent = (**langerrorwindowinfo).contentrect;
	
	r = rcontent;
	
	r.top = r.top + windowmargin;
	
	r.bottom = r.top + iconrectheight;
	
	r.right = r.right - windowmargin;
	
	r.left = r.right - iconrectwidth;
	
	(**hle).iconrect = r; 
	
	r.right = r.left - windowmargin;
	
	r.left = rcontent.left + windowmargin;
	
	r.bottom = rcontent.bottom - windowmargin;
	
	(**hle).textrect = r;
	
	return (true);
	} /*langerrorsetrects*/


static void langerrordrawicon (boolean flpressed) {
	
	register hdllangerrorrecord hle = langerrordata;
	register boolean flenabled;
	bigstring bs;
	
	flenabled = (**hle).flactive && !(**hle).flcallbackconsumed;
	
	getstringlist (langerrorlistnumber, scripticonstring, bs);
	
	drawlabeledwindoidicon ((**hle).iconrect, bs, flenabled, flpressed);
	} /*langerrordrawicon*/

	
static void langerrorframetext (void) {
	
	register hdllangerrorrecord hle = langerrordata;
	
	pushpen ();
	
	if (!(**hle).flactive) 
		setgraypen ();
	
	framerect ((**hle).textrect);
	
	poppen ();
	} /*langerrorframetext*/

static void langerrordrawtext (boolean flbitmap) {
	
	/*
	4/18/91 dmb: straighten fancy quotes when displaying in geneva 9
	*/
	
	register hdllangerrorrecord hle = langerrordata;
	register hdlwindowinfo hw = langerrorwindowinfo;
	Rect rbox;
	bigstring bserror;
	short font, size;
	
	copystring ((**hle).bserror, bserror);
	
	font = (**hw).defaultfont;
	
	size = (**hw).defaultsize;
	
	if ((font == geneva) && (size <= 9)) { /*get rid of fancy quotes; nasty in this font*/
		
