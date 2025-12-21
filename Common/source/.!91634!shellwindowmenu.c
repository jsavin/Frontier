
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

#include "menu.h"
#include "sounds.h"
#include "frontierwindows.h"
#include "shell.h"
#include "shellmenu.h"
#include "shellprivate.h"
#include "cancoon.h"
	#include "launch.h" /*For OS X Bring All to Front command*/




static hdlmenu hwindowsmenu = nil;

static boolean flwindowmenudirty = true;

static boolean fllastwasdottedline = false;

static short ixsearch;

static hdlwindowinfo hsearch;




static boolean pushwindowmenuvisit (WindowPtr w, ptrvoid ptr) {
#pragma unused (ptr)

	/*
	3/8/91 dmb: mark the current root item with an asterix, if not checked
	
	4/5/91 dmb: italicize hidden windows

	5.1b23 dmb: for Win threading, check getwindowinfo result

	7.0b26 PBS: mark the current root only on Macintosh. This is because the mark
	used on Windows is a checkmark, which makes for two checkmarks in the Window
	menu, which is confusing.
	*/
	
	bigstring bs;
	WindowPtr wfront;
	register short ix;
	hdlwindowinfo hinfo;
	Style itemstyle = 0;

		hdlwindowinfo hroot;
	
	if (!getwindowinfo (w, &hinfo))
		return (false);
	
#ifdef xxxPIKE
	/* Pike only displays visible .root windows */
	if ((hinfo != nil) && ((**hinfo).configresnum == idcancoonconfig) && (**hinfo).flhidden)
		return (true);
#endif

	shellgetwindowtitle (hinfo, bs); // 7.24.97 dmb: was windowgettitle
	
	if (isemptystring (bs))
		return (true);
	
	if (!pushmenuitem (hwindowsmenu, windowsmenu, bs, 0))
		return (false);
	
	fllastwasdottedline = false;

	frontshellwindow (&wfront);
	
	ix = countmenuitems (hwindowsmenu);
	
	if (w == wfront)
		checkmenuitem (hwindowsmenu, ix, true);
	

		else {
			if (frontrootwindow (&hroot) && (hinfo == hroot)) /*we're the active root*/
