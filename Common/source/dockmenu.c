
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

/*
Implement right-click menus in the system tray icon or dock menu.
(Legacy: originally from FrontierWinMain.c platform-specific entry point)
7.1b22 11/08/01 PBS
*/

#include "frontier.h"
#include "standard.h"

#include "menu.h"
#include "strings.h"
#include "cancoon.h"
#include "launch.h"
#include "tablestructure.h"
#include "popup.h"
#include "meprograms.h"
#include "dockmenu.h"
#include "opinternal.h"
#include "menuverbs.h"

#define idgetdockmenumenuaddresscallback 43

#define kmaxcommands 100
#define kbasecommandid 3000
#define maxsubmenus 40


	UInt32 menuid;
	short idsubmenu;
	hdlmenurecord hcurrmenurecord;
	boolean flstackneedsdisposing = false;





#pragma pack(2)
	typedef struct typopupinfo {

		hdlmenu hmenu;
		
		short id;			
		} typopupinfo, *ptrpopupinfo, **hdlpopupinfo;

	typedef struct tydockmenustack {
		
		typopupinfo popupmenus [maxsubmenus];
		
		short currstackitem;
		} tydockmenustack, *ptrdockmenustack, **hdldockmenustack;
#pragma options align=reset

	tydockmenustack dockmenustack;



static boolean dockmenubuildpopupmenu (hdlheadrecord hnode, hdlmenu hmenu); /*forward*/

static void dockmenudisposemenusinstack (void);


static void dockmenuresetmenustack (void) {
	
	
		dockmenudisposemenusinstack ();

		dockmenustack.currstackitem = -1;
		
		dockmenustack.popupmenus [0].hmenu = nil;
		
		dockmenustack.popupmenus [0].id = -1;
	
	} /*dockmenuresetmenustack*/


static void dockmenudisposemenusinstack (void) {
	
	/*
	Dispose and delete all menus in popup menu stack.
	*/
	
	
		short ix = dockmenustack.currstackitem;
		short i, id;
		hdlmenu hmenu;
		
		if (!flstackneedsdisposing)
			return;
		
		if (ix < 0)
			return;
		
		for (i = ix; i > -1; i--) {
			
			id = dockmenustack.popupmenus [i].id;
			
			if (id > -1)
			
				DeleteMenu (id);
				
			if (id != defaultpopupmenuid) { /*already disposed by system*/
			
				hmenu = dockmenustack.popupmenus [i].hmenu;
			
				if (hmenu != nil)
			
					disposemenu (hmenu);
				} /*if*/
			
			dockmenustack.popupmenus [i].hmenu = nil;
			
			dockmenustack.popupmenus [i].id = -1;
			
			} /*for*/
		
		flstackneedsdisposing = false;	
	} /*dockmenudisposemenusinstack*/


static boolean dockmenuaddtomenustack (hdlmenu hmenu, short id) {


		short ix = dockmenustack.currstackitem + 1;
		
		if (ix > maxsubmenus)	
			return (false);
		
		dockmenustack.currstackitem = ix;
		
		dockmenustack.popupmenus [ix].hmenu = hmenu;
		
		dockmenustack.popupmenus [ix].id = id;
	

	return (true);	
	} /*dockmenuaddtomenustack*/


static void dockmenuinsertsubmenu (hdlmenu hmenu, short itemnumber, hdlheadrecord hnode) {

	/*
	7.1b23 PBS: build a sub-menu and attach it.
	*/

	hdlmenu hsubmenu;
	short id = defaultpopupmenuid;

		idsubmenu++;
		
		id = idsubmenu;
	
	hsubmenu = Newmenu (id, BIGSTRING (""));

		InsertMenu (hsubmenu, -1);

	dockmenubuildpopupmenu (hnode, hsubmenu);

	dockmenuaddtomenustack (hsubmenu, id);

	sethierarchicalmenuitem (hmenu, itemnumber, hsubmenu, id);
	} /*dockmenuinsertsubmenu*/


static boolean dockmenugetaddresscallback (tyvaluerecord *val) {

	/*
	7.1b22 PBS: Run the callback script that returns the address
	of a menubar object to use.
	*/

	Handle htext = 0;
	boolean fl = false;
	bigstring bsscript;
	short idscript = idgetdockmenumenuaddresscallback;

	if (getsystemtablescript (idscript, bsscript)) {
	
		if (!newtexthandle (bsscript, &htext))
		
			return (false);

		grabthreadglobals ();
		
		oppushoutline (op_get_outlinedata());
		
		fl = langrun (htext, val);

		oppopoutline ();

		releasethreadglobals ();
		}

	return (fl);
	} /*dockmenugetaddresscallback*/


static boolean dockmenuinsertmenuitem (hdlmenu hmenu, short itemnumber, hdlheadrecord hnode) {

	/*
	Insert one menu item.

	7.1b42 PBS: check menu items that should be checked.
	*/
	
	bigstring bsheadstring;
	boolean flenabled = true;
	boolean flchecked = false;

	getheadstring (hnode, bsheadstring);

	mereduceformula (bsheadstring);
	
	mereducemenucodes (bsheadstring, &flenabled, &flchecked); /*7.0b23 PBS: items can be disabled.*/

	pushpopupitem (hmenu, bsheadstring, flenabled, menuid);

	
		SetMenuItemCommandID (hmenu, countmenuitems (hmenu), menuid);
		

	if (flchecked) /*7.1b42 PBS: support for checked menu items.*/
		checkmenuitem (hmenu, countmenuitems (hmenu), flchecked);

	if (!opnosubheads (hnode)) /*has subs?*/
		dockmenuinsertsubmenu (hmenu, itemnumber, hnode);

		flstackneedsdisposing = true;

	return (true);
	} /*dockmenuinsertmenuitem*/


static boolean dockmenubuildpopupmenu (hdlheadrecord hnode, hdlmenu hmenu) {
	
	/*
	7.1b22 PBS: Build a popup menu from the outline pointed to by hnode.
	Recurse to handle a menu that has a submenu.

	Based on mebuildmenu, but different enough to need its own routine.	
	*/
	
	short itemnumber;
	bigstring bs;
	
	getheadstring (hnode, bs); /*get the menu title*/
	
	if (opnosubheads (hnode))
		return (true);
	
	hnode = (**hnode).headlinkright; /*move to first subhead*/
	
	for (itemnumber = 1; ; itemnumber++) {

		menuid++;
		
		if (!dockmenuinsertmenuitem (hmenu, itemnumber, hnode))
			return (false);
		
		if (opislastsubhead (hnode)) /*done with this menu*/
			return (true);
		
		hnode = (**hnode).headlinkdown; /*advance to next item*/
		} /*while*/
	} /*dockmenubuildpopupmenu*/


static boolean dockmenufillpopup (hdlmenu hmenu, hdlmenurecord *hmreturned) {

	/*
	7.1b44 PBS: call menuverbgetsize to make sure the menu is in memory.
	*/

	hdlhashtable htable;
	hdlhashnode hnode;
	hdlmenurecord hm;
	tyvaluerecord valaddress, val;
	hdlexternalhandle h;
	hdlheadrecord hsummit;
	bigstring bsaddress;
	boolean fl = false;
	long menusize = 0;

	if (roottable == nil) /*9.1b1 JES: If there's no system root, don't crash.*/
		return (false);
	
	if (!dockmenugetaddresscallback (&valaddress))
		return (false);
	
	if (valaddress.valuetype != addressvaluetype)
		goto exit;

	if (!getaddressvalue (valaddress, &htable, bsaddress))
		goto exit;

	if (!langsymbolreference (htable, bsaddress, &val, &hnode))
		goto exit;

	if (val.valuetype != externalvaluetype)
		goto exit;

	h = (hdlexternalhandle) val.data.externalvalue;

	if (!menuverbgetsize (h, &menusize)) /*7.1b43 PBS: We don't care about the size. We just want to read it into memory.*/
		goto exit;

	if ((**h).id != idmenuprocessor) /*it must be a menu*/
		goto exit;

	hm = (hdlmenurecord) (**h).variabledata;

	hsummit = (**(**hm).menuoutline).hsummit;

		menuid = kbasecommandid;
		idsubmenu = 5;

	dockmenudisposemenusinstack ();
	
	dockmenubuildpopupmenu (hsummit, hmenu);
	
	*hmreturned = hm;

	fl = true;
	
	exit:

	disposevaluerecord (valaddress, false);

	return (fl);
	} /*dockmenufillpopup*/


static void dockmenuruncommand (hdlmenurecord hm, short itemhit) {
	
	hdlheadrecord hsummit;
	hdlheadrecord hmenuitem;
	bigstring bs;

	hsummit = (**(**hm).menuoutline).hsummit; /*Top level of the menu*/

	hmenuitem = oprepeatedbump (flatdown, itemhit, hsummit, false);
	
	oppushoutline ((**hm).menuoutline);

	getheadstring (hmenuitem, bs);

	meuserselected (hmenuitem);
	
	dockmenudisposemenusinstack ();

	oppopoutline ();
	} /*dockmenuruncommand*/



pascal OSStatus dockcommandhandler (EventHandlerCallRef nextHandler, EventRef theEvent, void* userData) {
#pragma unused(nextHandler, theEvent, userData)	/*happy compiler*/

	HICommand commandstruct;
	UInt32 commandid;
	OSErr ec = eventNotHandledErr;
	
	GetEventParameter (theEvent, kEventParamDirectObject, typeHICommand, NULL, sizeof (HICommand), NULL, &commandstruct);
	
	commandid = commandstruct.commandID;
	
	if ((commandid >= kbasecommandid) && (commandid <= kbasecommandid + kmaxcommands)) {
	
		dockmenuruncommand (hcurrmenurecord, commandid - kbasecommandid);
		
		ec = noErr; /*all's well*/
		} /*if*/
	
	dockmenudisposemenusinstack (); /*7.1b23 PBS: delete and dispose submenus*/
	
	return (ec);
	} /*dockcommandhandler*/


static void dockmenuinstallhandler (void) {
	
	EventTypeSpec myevents = {kEventClassCommand, kEventCommandProcess};
	
	InstallApplicationEventHandler (NewEventHandlerUPP (dockcommandhandler), 1, &myevents, 0, NULL);
	
	} /*dockmenuinstallhandler*/


pascal OSStatus dockmenuhandler (EventHandlerCallRef nextHandler, EventRef theEvent, void* userData) {
	
	/*
	7.1b22 PBS: called from the system when the dock icon is right-clicked.
	Build a contextual menu and return it.
	*/
	
	#pragma unused(nextHandler) /*happy compiler*/
	#pragma unused(theEvent)
	#pragma unused(userData)
	
	hdlmenu hmenu;
	hdlmenurecord hm;
	static boolean flinited = false;
	
	menuid = 0;
	
	dockmenuresetmenustack (); /*7.1b23 PBS: maintain a stack of submenus*/
	
	if (!flinited) { /*install command handler first time*/
		
		dockmenuinstallhandler ();
		
		flinited = true;
		} /*if*/
	
	hmenu = Newmenu (defaultpopupmenuid, BIGSTRING (""));
	
	if (!dockmenufillpopup (hmenu, &hm))
		goto exit;
	
	dockmenuaddtomenustack (hmenu, defaultpopupmenuid);

	hcurrmenurecord = hm;
	
	SetEventParameter (theEvent, kEventParamMenuRef, typeMenuRef, sizeof (MenuRef), &hmenu);
	
	exit:

	return (noErr); /*all's well in dock-menu-land*/
	} /*dockmenuhandler*/


