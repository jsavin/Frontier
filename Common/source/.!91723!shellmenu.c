
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

	#include <uisharing.h>

#include "memory.h"
#include "cursor.h"
#include "dialogs.h"
#include "font.h"
#include "menu.h"
#include "resources.h"
#include "sounds.h"
#include "strings.h"
#include "popup.h"
#include "quickdraw.h"
#include "frontierwindows.h"
#include "kb.h"
#include "about.h"
#include "shell.h"
#include "shell.rsrc.h"
#include "shellhooks.h"
#include "shellmenu.h"
#include "shellprint.h"
#include "shellprivate.h"
#include "cancoon.h"
#include "tablestructure.h"
#include "ops.h" /* 2005-09-25 creedon */
#include "langinternal.h" /* 2005-12-27 creedon */


tymenuinfo menustack [ctmenustack];

short topmenustack = -1;

static hdlmenu happlemenu;

static tymenustate menustate = dirtymenus;



static boolean parammenuitem (hdlmenu hmenu, short item) {
	
	/*
	in the indicated menu item, replace all occurrences of ^0 with the
	parameter string.
	*/
	
	register hdlmenu h = hmenu; 
	bigstring bsitem;
	
	getmenuitem (h, item, bsitem); 
	
	parseparamstring (bsitem);
	
	setmenuitem (h, item, bsitem);
	
	return (true);
	} /*parammenuitem*/

	
	
static boolean menudisablevisit (hdlmenu hmenu, short item) {
	
	if (hmenu != happlemenu) /*apple menu always fully enabled*/
		disablemenuitem (hmenu, item);
	
	return (true);
	} /*menudisablevisit*/


static boolean visitonemenu (short idmenu, boolean (*visitproc) (hdlmenu, short)) {
	
	register hdlmenu hmenu;
	register short i;
	register short lastitem;
	
	hmenu = shellmenuhandle (idmenu);
	
	if (hmenu == nil)
		return (false);
	
	lastitem = countmenuitems (hmenu);
	
	for (i = 1; i <= lastitem; i++) {
		
		if (!(*visitproc) (hmenu, i))
			return (false);
		} /*for*/

	return (true);
	} /*visitonemenu*/


static boolean visitmenuitems (boolean (*visitproc) (hdlmenu, short)) {
	
	register short lastmenu = topmenustack;
	register short i, j;
	register short lastitem;
	register hdlmenu hmenu;
	
	for (i = 0; i <= lastmenu; i++) {
		
		hmenu = menustack [i].macmenu;
		
		lastitem = countmenuitems (hmenu);
		
		for (j = 1; j <= lastitem; j++) {
			
			if (!(*visitproc) (hmenu, j))
				return (false);
			} /*for*/
		} /*for*/
		
	return (true);
	} /*visitmenuitems*/




hdlmenu shellmenuhandle (short idmenu) {

	short i;
	register short lastmenu = topmenustack;

	for (i = 0; i <= lastmenu; i++) {
		
		if (menustack [i].idmenu == idmenu)
			return (menustack [i].macmenu);
		}

	return (nil);
	} /*shellmenuhandle*/


boolean shelltgetmainmenu (bigstring bsmenu, hdlmenu *hmenu, short *idmenu) {
	
	/*
	5.0a24 dmb: find a main menu with the given title
	*/

	short ctmainmenus = (lastmainmenu - firstmainmenu) / mainmenuincrement + 1;
	short ixmenu;
	
	for (ixmenu = 0; ixmenu < ctmainmenus; ++ixmenu) {
		
		register hdlmenu h = menustack [ixmenu].macmenu;
		bigstring bstitle;
		
		getmenuitem (h, 0, bstitle);
		
		popleadingchars (bstitle, (char) '&'); /*pop off keyboard accelerator mark*/
		
		if (equalstrings (bstitle, bsmenu)) {
			
			*hmenu = h;
			
			*idmenu = menustack [ixmenu].idmenu;
			
			return (true);
			}
		}
	
		if (equalstrings (bsmenu, "\pHelp")) {
			
			//Code change by Timothy Paustian Friday, June 16, 2000 3:04:41 PM
			//Changed to Opaque call for Carbon
			//we will add the below code when this is implemented in carbon.
			{
				MenuItemIndex	theIndex;
				if (HMGetHelpMenu(hmenu, &theIndex) == noErr && *hmenu != nil) {
					
					*idmenu = kHMHelpMenuID;
					
					return (true);
					}
				}
		}

	return (false);
	} /*shelltgetmainmenu*/


static boolean pushmenustack (short idmenu, hdlmenu hmenu) {
	
	register short top = topmenustack + 1;
	
	if (top >= ctmenustack) 
		return (false);
	
	menustack [top].idmenu = idmenu;

	menustack [top].macmenu = hmenu;
	
	topmenustack = top;
	
	return (true);
	} /*pushmenustack*/
	
	
static boolean installmenu (short idmenu) {
	
	register hdlmenu h;
	
	h = getresourcemenu (idmenu); 
	
	if (h == nil) /*error in the menu manager*/
		return (false);
	
	if (!pushmenustack (idmenu, h))
		return (false);
		
	insertmenu (h, insertatend);
	
	return (true);
	} /*installmenu*/


static boolean installhierarchicmenu (short idmenu) {
	
	register hdlmenu h;
	
	h = getresourcemenu (idmenu);
	
	if (h == nil) /*error in the menu manager*/
		return (false);
	
	if (!pushmenustack (idmenu, h))
		return (false);
	
	inserthierarchicmenu (h, idmenu); /*insert it as a hierarchic menu*/
		
	return (true);
	} /*installhierarchicmenu*/


static boolean installresitems (short idmenu, OSType restype) {
	
	/*
	2/6/91 dmb: HierDA leaves ResError set after AddResMenu, so 
	we no longer check the result.  we probably wouldn't want to 
	treat that as a fatal error anyway.
	*/
	
	register hdlmenu h;
	
	h = shellmenuhandle (idmenu);
	
	if (h == nil) /*error in the menu manager*/
		return (false);
	
	pushresourcemenuitems (h, idmenu, restype); 
	
	return (true);
	} /*installresitems*/


boolean shellinitmenus (void) {
	
	register short idmenu;
	
		bigstring bsprogramname; /*PBS 7.1b4: use ifdef because this variable isn't used on Windows.*/
	
	topmenustack = -1; /*no items on the menu stack*/
	
	clearbytes (&menustack, sizeof (menustack)); /*clear it for neatness sake*/
	
	for (idmenu = firstmainmenu;  idmenu <= lastmainmenu;  idmenu += mainmenuincrement)
		if (!installmenu (idmenu))
			return (false);
		
	for (idmenu = firsthiermenu;  idmenu <= lasthiermenu;  idmenu += hiermenuincrement)
		if (!installhierarchicmenu (idmenu))
			return (false);
	
	happlemenu = shellmenuhandle (applemenu); /*set global*/

	if (!installresitems (applemenu, 'DRVR'))
		return (false);
	
	if (!installresitems (fontmenu, 'FONT'))
		return (false);
	
	//#if TARGET_API_MAC_CARBON == 1 /*PBS 7.1b15: too slow to open that menu.*/
	
		//setfontmenustyles (); /*PBS 7.0b46: wizzy font menu*/
		
	//#endif
	
	
	getprogramname (bsprogramname);
	
	setparseparams (bsprogramname, nil, nil, nil);
	
	visitmenuitems (&parammenuitem); /*perform ^0, ^1... substitutions*/


	visitmenuitems (&menudisablevisit); /*disable all menu items*/
	
	return (true);
	} /*shellinitmenus*/
	
	
void shellgetlastmenuid (short *id) {
	
	/*
	return the menu id of the last menu we insert into the menubar.
	*/
	
	*id = editmenu;
	} /*shellgetlastmenuid*/
	
boolean shellapplemenu (bigstring bsname) {
	
	/*
	can be called from one of the clients.  also there's a verb in the language
	that supports this.
	
	by convention, DA names may or may not begin with one or more nulls.  we take
	this cultural wierdness into account.
	
	return true if we got to call OpenDeskAcc, false if there was some problem we
	were able to detect.
	*/
	
	bigstring bs, bsorig, bscompare;
	short i, ct;
	
	copystring (bsname, bs); /*work with a copy*/
	
	if (isemptystring (bs)) /*nothing to do*/
		return (false);
	
	popleadingchars (bs, (char) 0); /*pop off any leading nulls*/
	
	ct = countmenuitems (happlemenu);
	
	for (i = 1; i <= ct; i++) {
		
		getmenuitem (happlemenu, i, bsorig);
		
		copystring (bsorig, bscompare);
		
		popleadingchars (bscompare, (char) 0); /*pop off any leading nulls*/
		
		if (equalstrings (bs, bscompare)) { /*strings match without any nulls*/
			
			pushstyle (systemFont, 12, 0);
			//Code change by Timothy Paustian Friday, June 16, 2000 3:01:02 PM
			//Changed to Opaque call for Carbon
			//we do not need to do this for carbon

			popstyle ();    
			
			return (true);  
			}
		} /*for*/
	
	return (false); /*no item with a matching name*/
	} /*shellapplemenu*/

boolean shelleditcommand (tyeditcommand editcmd) {

	/*
	7.0b33 PBS: For some reason the HTML Control background on Windows version
	of Radio messes with the kernel's notion of when an editing window is in
	front. This can cause a crash. So a bit of defensive code has been placed here.
	Having spent hours on this bug, I'm going with the defensive code until I can get Bob's help.
	However, I'm limiting the defensive code to Radio/Win only.
	*/
	
	register boolean fl = false;

	#ifdef PIKE
	#endif
	
	switch (editcmd) {
		
		case undocommand:
			fl = (*shellglobals.undoroutine) ();
			
			break;
		
		case cutcommand:
			fl = (*shellglobals.cutroutine) ();
			
			break;
			
		case copycommand:
			fl = (*shellglobals.copyroutine) ();
			
			break;
			
		case pastecommand:
			
			shellreadscrap (); /*since we don't display scrap, now's the time to get current*/
			
			fl = (*shellglobals.pasteroutine) ();
			
			break;
			
		case clearcommand:
			fl = (*shellglobals.clearroutine) ();
			
			break;
		
		case selectallcommand:
			fl = (*shellglobals.selectallroutine) ();
		
		default:
			return (false);
		} /*switch*/
	
	return (true);
	} /*shelleditcommand*/
	

static boolean shellfontmenuchecker (hdlmenu hmenu, short itemnumber) {

	/*
	11/8/90 DW: turns out we CAN ligitimately check more than one font, if there
	is a conflict in font numbers.  the solution -- the user must straighten out
	his font resources, possibly using a font harmonzier so that there are not
	font number conflicts.
	*/
	
	register boolean fl;
	bigstring bs;
	short fontnum;
	
	getmenuitem (hmenu, itemnumber, bs);
	
	fontgetnumber (bs, &fontnum);
	
	fl = (fontnum == (**shellwindowinfo).selectioninfo.fontnum);
	
	checkmenuitem (hmenu, itemnumber, fl);
		
	return (true);
	} /*shellfontmenuchecker*/
	
	
static boolean shellsizemenuchecker (hdlmenu hmenu, short itemnumber) {
	
	register short checkeditem;
	register short fontsize;
	register short itemsize;
	register short style;
	register short fontnum;
	
	fontsize = (**shellwindowinfo).selectioninfo.fontsize;
	
	switch (fontsize) {
		
		case -1: /*no consistent size across selection*/
			checkmenuitem (hmenu, itemnumber, false);
			
			goto L1; /*skip to determining if it's a real font or not*/
		
		case 9:
			checkeditem = point9item;
			
			break;
			
		case 10:
			checkeditem = point10item;
			
			break;
			
		case 12:
			checkeditem = point12item;
			
			break;
			
		case 14:
			checkeditem = point14item;
			
			break;
			
		case 18:
			checkeditem = point18item;
			
			break;
			
		case 24:
			checkeditem = point24item;
			
			break;
			
		default:
			checkeditem = pointcustomitem;
			
			break;
		} /*switch*/
	
	checkmenuitem (hmenu, itemnumber, itemnumber == checkeditem);
	
	L1:
	
	fontnum = (**shellwindowinfo).selectioninfo.fontnum;
	
	if (fontnum == -1) { /*no consistent font across selection*/
		
		stylemenuitem (hmenu, itemnumber, 0); /*plain*/
		
		return (true);
		}
	
	itemsize = -1;
	
	switch (itemnumber) {
		
		case point9item:
			itemsize = 9;
			
			break;
			
		case point10item:
			itemsize = 10;
			
			break;
			
		case point12item:
			itemsize = 12;
			
			break;
			
		case point14item:
			itemsize = 14;
			
			break;
			
		case point18item:
			itemsize = 18;
			
			break;
			
		case point24item:
			itemsize = 24;
			
			break;
			
		} /*switch*/
	
	style = 0;
	
	if (itemsize != -1)
		if (realfont (fontnum, itemsize))
			style = outline;
		
	stylemenuitem (hmenu, itemnumber, style);
		
	return (true);
	} /*shellsizemenuchecker*/
	
	
static boolean shellstylemenuchecker (hdlmenu hmenu, short itemnumber) {
	
	register boolean flchecked = false;
	tyselectioninfo x;
	
	x = (**shellwindowinfo).selectioninfo;
	
	switch (itemnumber) {
		
		case plainitem:
			flchecked = x.fontstyle == 0; /*flplain;*/
			
			break;
			
		case bolditem:
			flchecked = (x.fontstyle & bold) != 0;
			
			break;
			
		case italicitem:
			flchecked = (x.fontstyle & italic) != 0;
			
			break;
			
		case underlineitem:
			flchecked = (x.fontstyle & underline) != 0;
			
			break;
			
		case outlineitem:
			flchecked = (x.fontstyle & outline) != 0;
			
			break;
			
		case shadowitem:
			flchecked = (x.fontstyle & shadow) != 0;
			
			break;

		/*
		case condenseditem:
			flchecked = x.fontstyle.flcondensed;
			
			break;
		
		case superscriptitem:
			flchecked = x.fontstyle.flsuperscript;
			
			break;
			
		case subscriptitem:
			flchecked = x.fontstyle.flsubscript;
			
			break;
		*/	
		} /*switch*/
	
	checkmenuitem (hmenu, itemnumber, flchecked);
	
	return (true);
	} /*shellstylemenuchecker*/
	

static boolean shellleadingmenuchecker (hdlmenu hmenu, short itemnumber) {
	
	register short leading = (**shellwindowinfo).selectioninfo.leading;
	register short checkeditem = -1;
	
	switch (leading) {
		
		case -1: /*no consistent leading across selection*/
			checkmenuitem (hmenu, itemnumber, false);
			
			return (true);
		
		case 0:
			checkeditem = leading0item;
			
			break;
		
		case 1:
			checkeditem = leading1item;
			
			break;
			
		case 2:
			checkeditem = leading2item;
			
			break;
			
		case 3:
			checkeditem = leading3item;
			
			break;
			
		case 4:
			checkeditem = leading4item;
			
			break;
			
		case 5:
			checkeditem = leading5item;
			
			break;
			
		default:
			checkeditem = leadingcustomitem;
			
			break;
		} /*switch*/
	
	checkmenuitem (hmenu, itemnumber, itemnumber == checkeditem);
	
	return (true);
	} /*shellleadingmenuchecker*/


static boolean shelljustifymenuchecker (hdlmenu hmenu, short itemnumber) {
	
	register tyjustification justification = (**shellwindowinfo).selectioninfo.justification;
	register short checkeditem = -1;
	
	switch (justification) {
		
		case leftjustified:
			checkeditem = leftjustifyitem;
			
			break;
			
		case centerjustified:
			checkeditem = centerjustifyitem;
			
			break;
			
		case rightjustified:
			checkeditem = rightjustifyitem;
			
			break;
			
		case fulljustified:
			checkeditem = fulljustifyitem;
			
			break;
			
		case unknownjustification:
			checkmenuitem (hmenu, itemnumber, false);
			
			return (true);
			
		} /*switch*/
	
	checkmenuitem (hmenu, itemnumber, itemnumber == checkeditem);
	
	return (true);
	} /*shelljustifymenuchecker*/


static void shellcheckfontsizestyle (void) {
	
	register hdlwindowinfo hw = shellwindowinfo;
	tyselectioninfo x;
	
	/*blidgey ();*/
	
	if (hw == nil) /*no windows open*/
		return;
	
	x = (**hw).selectioninfo;
	
	if (!x.fldirty) /*nothing to do*/
		return;
	
	shellsetselectioninfo ();
	
	x = (**hw).selectioninfo; /*get updated flags*/
	
#ifdef fontmenu
	if (x.flcansetfont)
		visitonemenu (fontmenu, &shellfontmenuchecker);
#endif

#ifdef sizemenu	
	if (x.flcansetsize)
		visitonemenu (sizemenu, &shellsizemenuchecker);
#endif

#ifdef stylemenu	
	if (x.flcansetstyle)
		visitonemenu (stylemenu, &shellstylemenuchecker);
#endif

#ifdef leadingmenu	
	if (x.flcansetleading)
		visitonemenu (leadingmenu, &shellleadingmenuchecker);
#endif

#ifdef justifymenu	
	if (x.flcansetjust)
		visitonemenu (justifymenu, &shelljustifymenuchecker);
#endif
	} /*shellcheckfontsizestyle*/


void shelladjustundo (void) {
	
	register hdlstring hstring = nil;
	register boolean flundoable = false;
	register hdlmenu hmenu;
	bigstring bs;
	
	if (shellwindow != nil) { /*there's at least one window open*/
	
		(*shellglobals.setundostatusroutine) ();
	
		hstring = (**shellwindowinfo).hundostring;
	
		flundoable = hstring != nil;
		}
	
	if (!flundoable) 
		getstringlist (undolistnumber, cantundoitem, bs);
	else
		copyheapstring (hstring, bs);
	
	hmenu = shellmenuhandle (editmenu);
	
	if (hmenu != nil) {
		
		setmenuitem (hmenu, undoitem, bs);
	
		setmenuitemenable (hmenu, undoitem, flundoable);
		}
	} /*shelladjustundo*/


void shellforcemenuadjust (void) {
	
	menustate = dirtymenus;
	} /*shellforcemenuadjust*/


void shellmodaldialogmenuadjust (void) {
	
	menustate = modaldialogmenus;
	
	shelladjustmenus ();
	} /*shellforcemenuadjust*/


#ifndef PIKE

static boolean shellsetmenuitemstring (hdlmenu hmenu, short ixmenu, short ixitemstring) {
	
	bigstring bs;
	
	return (shellgetstring (ixitemstring, bs) && setmenuitem (hmenu, ixmenu, bs));
	} /*shellsetmenuitemstring*/

#endif


#ifdef PIKE

static void pikesetfilemenuitemchecked (short ixmenu) {
	
	/*
	7.0b25 PBS: Run a Radio UserLand script that returns true if the item should get
	a check mark. If true, put a check next to the item.

	7.1b4 PBS: Get script from resource, don't hard-code.
	*/
	
	bigstring bsscript, bsitem, bsresult;
	
	if (roottable == nil)
		return;
	
	getfilemenuitemidentifier (ixmenu, bsitem);
	
	/*copystring ("\x20" "pike.isFileMenuItemChecked(\"^0\")", bsscript);*/

	getsystemtablescript (idpikeisfilemenuitemcheckedscript, bsscript);
	
	parsedialogstring (bsscript, bsitem, nil, nil, nil, bsscript);

	grabthreadglobals ();
	
	langrunstringnoerror (bsscript, bsresult);
	
	releasethreadglobals ();

	checkmenuitem (shellmenuhandle (filemenu), ixmenu, equalstrings (bsresult, bstrue));
	} /*pikesetfilemenuitemchecked*/


static void pikesetfilemenuitemenable (short ixmenu) {

	/*
	6.2a2 AR: Call the pike.isFileMenuItemEnabled script to determine whether
	the menu item should be enabled or disabled.

	7.1b4: Get script from resource, don't hard-code it.
	*/

	bigstring bsscript, bsitem, bsresult;

	if (roottable == nil)
		return;

	getfilemenuitemidentifier (ixmenu, bsitem);

	/*copystring ("\x20""pike.isFileMenuItemEnabled(\"^0\")", bsscript);*/

	getsystemtablescript (idpikeisfilemenuitemenabledscript, bsscript);

	parsedialogstring (bsscript, bsitem, nil, nil, nil, bsscript);

	grabthreadglobals ();
	
	langrunstringnoerror (bsscript, bsresult);
	
	releasethreadglobals ();
	
	setmenuitemenable (shellmenuhandle (filemenu), ixmenu, equalstrings (bsresult, bstrue));
	}/*ccpikesetfilemenuitemenable*/




boolean pikequit () {

	/*
	7.0 PBS: Called in Windows when the user clicks the X in the frame window.

	7.1b4 PBS: get script string from resource, don't hard-code.
	*/

	bigstring bsscript, bsitem, bsresult;

	if (roottable == nil)
		return (true);

	getfilemenuitemidentifier (quititem, bsitem);

	getsystemtablescript (idrunfilemenuscript, bsscript); /*7.1b4: get from resource.*/

	/*copystring ("\x1c""pike.runFileMenuScript(\"^0\")", bsscript);*/

	parsedialogstring (bsscript, bsitem, nil, nil, nil, bsscript);

	grabthreadglobals ();
	
	langrunstringnoerror (bsscript, bsresult);

	releasethreadglobals ();

    return comparestrings(bsresult, bsfalse) == 0;
	} /*pikequit*/

#endif


void shelladjustmenus (void) {
	
	/*
	2005-10-26 creedon: the file menu item that provides save as functionality now reads Save As... for database or file-object save for Frontier
	
	2005-09-25 creedon: added open recent menu
					 changed to support calling script for some file/edit menu commands on all targets
	
	8/1/90 dmb: call menuhooks with menu & item set to zero to give 
	hooks a chance to update their menus
	
	2/22/91 dmb: use new flcansetxxx fields in selectioninfo to handle enabling 
	of the corresponding submenus
	
	6.19.97 dmb: added spaghetti code for modal dialog menus.
	
	5.0d19 dmb: added save/save database toggle, changed enabling logic to experts
	
	5.0.2b6 dmb: when setting modaldialog menus, set the window menu dirty

	7.0b32 PBS: Handle Windows case when to the user there are no windows open,
	but actually the main root window is open but hidden. In this case it should act
	as if no windows are open.
	*/
	
	register hdlmenu hmenu;
	register WindowPtr w = shellwindow;
	boolean flwindow = w != nil;
	boolean flchanges;
	boolean flanywindow = (getfrontwindow () != nil);
	hdlwindowinfo hrootinfo = nil;
	tyselectioninfo x;

#ifndef PIKE
	Handle hdata;
#endif

	/*7.0b32 PBS: if shellwindow is root window but it's hidden, act as
	if no windows are open -- because, to the user, no windows *are* open.*/

	if (shellwindowinfo == NULL) {

		flwindow = false;

		flanywindow = false;
		} /*if*/

	else {

		if ((**shellwindowinfo).configresnum == idcancoonconfig) {
			
			if ((**shellwindowinfo).flhidden) {

				flwindow = false;

				flanywindow = false;
				} /*if*/
			} /*if*/
		} /*else*/

	
	if (menustate == modaldialogmenus) {
		
		visitmenuitems (&menudisablevisit); // disable all menu items
		
		shellwindowmenudirty (); // make sure it gets updated later
		
		goto L1;  // do the edit menu
		}
	
	if (flwindow) {
		
		getrootwindow (w, &hrootinfo);
		
		shellcheckfontsizestyle (); /*update checks on font/size/style menus*/
		
		x = (**shellwindowinfo).selectioninfo;
		}
	else {
		if (ccinexpertmode ())
			ccfindrootwindow (&hrootinfo);
		
		clearbytes (&x, sizeof (tyselectioninfo));
		}
		
	flchanges = hrootinfo && (**hrootinfo).flmadechanges;
	
	hmenu = shellmenuhandle (filemenu);
	
	if (hmenu == nil) /*skip file menu adjusting*/
		goto L1;	

#ifdef PIKE

/*7.0b1 PBS: Radio UserLand has a functioning About item in the Apple menu.*/

/*#ifdef MACVERSION
	disablemenuitem (happlemenu, aboutitem); //disable this command until we have a splash screen
#endif*/

	/*
	6.2a2 AR: For Pike, we want to manage the state of the items
	in the File menu by calling a UserTalk script.

	7.0d6 PBS: Pike's File menu has changed. It's more like a standard
	File menu. The names, number, and order of items have changed.

	7.0d10 PBS: Radio UserLand now has Update Radio.root... in the File menu.
	*/

	enablemenuitem (hmenu, newitem);

	enablemenuitem (hmenu, openitem);
	
	pikesetfilemenuitemenable (openurlitem); /*7.0b17 PBS: enable/disable Open URL... menu item.*/
	
#ifndef OPMLEDITOR
	pikesetfilemenuitemenable (openmanilasiteitem); /*7.0b27 PBS: enable/disable Open Manila Site item.*/
#endif // OPMLEDITOR		
	pikesetfilemenuitemenable (closeitem);
		
	pikesetfilemenuitemenable (saveitem);

	pikesetfilemenuitemenable (saveasitem);
	
#ifndef OPMLEDITOR
	pikesetfilemenuitemenable (saveashtmlitem); /*7.0b32 PBS: Save As HTML*/
	
	pikesetfilemenuitemenable (saveasplaintextitem); /*7.0b32 PBS: Save As Plain Text*/
#endif // OPMLEDITOR
		
	pikesetfilemenuitemenable (revertitem);

	pikesetfilemenuitemenable (viewinbrowseritem);

	pikesetfilemenuitemenable (updateradiorootitem); /*7.0d10 PBS*/
	
	pikesetfilemenuitemenable (workofflineitem); /*7.0b25 PBS*/
	
	pikesetfilemenuitemchecked (workofflineitem); /*7.0b25 PBS*/

#else

	enablemenuitem (hmenu, newitem);
	
	enablemenuitem (hmenu, openitem);
	
	enablemenuitem (hmenu, openrecentitem);
	
	setmenuitemenable (hmenu, closeitem, flwindow);

	/*3/30/90 DW -- saveitem is not dependent on flchanges.  this allows you to save
	even when there have been no changes -- needed because changes to the symbol table
	no longer dirty the window it lives in.
	
	11/8/90 DW -- it's nice to be able to save even if we haven't made any changes.
	
	setmenuitemenable (hmenu, saveitem, flchanges);
	
	5.0a18 dmb: only enable save runnable for scripts
	*/

	setmenuitemenable (hmenu, saveitem, flwindow || hrootinfo);

	if ((hrootinfo != nil) && (!flwindow || (**hrootinfo).configresnum == iddefaultconfig)) {
		
		shellsetmenuitemstring (hmenu, saveitem, savedatabaseitemstring); // "Save Database");
		
