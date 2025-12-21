
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

#include "quickdraw.h"
#include "bitmaps.h"
#include "cursor.h"
#include "font.h"
#include "launch.h"
#include "kb.h"
#include "memory.h"
#include "resources.h"
#include "scrap.h"
#include "strings.h"
#include "frontierwindows.h"
#include "ops.h"
#include "shell.h"
#include "shellmenu.h"
#include "shell.rsrc.h"
#include "dialogs.h"
#include "lang.h"
#include "mac.h"
#include "langinternal.h" /* 2005-09-26 creedon */
#include "tablestructure.h" /* 2005-09-26 creedon */


	#include <SetUpA5.h>



#define windowevents (updateMask + activMask)


short dialogcountitems (DialogPtr pdialog) {
	
	//Code change by Timothy Paustian Sunday, April 30, 2000 9:40:09 PM
	//Changed to Opaque call for Carbon
	//switched to use CountDITL. This has been around since OS 7
	return CountDITL(pdialog);
	} /*dialogcountitems*/


static void dialoggeteditbuffer (DialogPtr pdialog, TEHandle *hbuffer) {
	
	//Code change by Timothy Paustian Sunday, April 30, 2000 9:43:36 PM
	//Changed to Opaque call for Carbon
	//check this I need to check to see if this is a memory leak or not. 
	#if ACCESSOR_CALLS_ARE_FUNCTIONS == 1
		//DialogItemType  	itemType = 0;
		//Handle 				h;
		//Rect 				r = {0,0,0,0};
		//SInt16				whichField = 0;

		*hbuffer = GetDialogTextEditHandle((DialogRef) pdialog);
	
	#else
		//old code
		*hbuffer = (*(DialogPeek) pdialog).textH;
	#endif
	} /*dialoggeteditbuffer*/


void boldenbutton (DialogPtr pdialog, short itemnumber) {

	/*
	draw a thick black ring around the OK button in the dialog.  
	*/
	
	PenState savePen;
	short itemtype;
	Handle itemhandle;
	Rect itemrect;
	
	//Code change by Timothy Paustian Wednesday, July 12, 2000 12:39:52 PM
	//pushport was doing an implicit cast from a dialogPtr to a GrafPtr
	//This is allowed in MacOS, but not in carbon. This was causing a crash.
	CGrafPtr	thePort;
	thePort = GetDialogPort(pdialog);
		
	pushport (thePort);
	
	GetPenState (&savePen); /*save the old pen state*/
	
	GetDialogItem (pdialog, itemnumber, &itemtype, &itemhandle, &itemrect); /*get the item's rect*/
	
	insetrect (&itemrect, -4, -4);
	
	PenSize (3, 3); /*make the pen fatter*/
	
	FrameRoundRect (&itemrect, 16, 16); /*draw the ring*/

	SetPenState (&savePen); /*restore the pen state*/
	
	popport ();
	} /*boldenbutton*/


void positiondialogwindow (DialogPtr pdialog) {

	register short h, v;
	Rect rdialog, rscreen;
	CGrafPtr	thePort;
	
	getcurrentscreenbounds (&rscreen);
	//Code change by Timothy Paustian Sunday, April 30, 2000 9:44:58 PM
	//Changed to Opaque call for Carbon
	#if ACCESSOR_CALLS_ARE_FUNCTIONS == 1
	thePort = GetDialogPort(pdialog);
	GetPortBounds(thePort, &rdialog);
	#else
	//old code
	#pragma unused(thePort)
	rdialog = (*pdialog).portRect;
	#endif
	
	h = rscreen.left + (((rscreen.right - rscreen.left) - (rdialog.right - rdialog.left)) / 2);
	
	v = rscreen.top + (((rscreen.bottom - rscreen.top) - (rdialog.bottom - rdialog.top)) / 3);
	
	{
	WindowPtr theWind = GetDialogWindow(pdialog);
	movewindow(theWind, h, v);
	}
	} /*positiondialogwindow*/


static boolean dialogitemtypeiscontrol (short itemtype) {
	
	register short x;
	
	x = itemtype % itemDisable; /*ignore enabledness*/
	
	return ((x >= ctrlItem) && (x <= (ctrlItem + resCtrl))); 
	} /*dialogitemtypeiscontrol*/


void disabledialogitem (DialogPtr pdialog, short itemnumber) {
	
	/*
	3/6/91 dmb: also dim if the item is a control
	*/
	
	short itemtype;
	Handle itemhandle;
	Rect itemrect;
	
	GetDialogItem (pdialog, itemnumber, &itemtype, &itemhandle, &itemrect);
	
	if (itemtype < itemDisable) { /*it is enabled, disable it*/
		
		SetDialogItem (pdialog, itemnumber, itemtype + itemDisable, itemhandle, &itemrect);
		
		if (dialogitemtypeiscontrol (itemtype))
			HiliteControl ((ControlHandle) itemhandle, 255);
		}
	} /*disabledialogitem*/


void enabledialogitem (DialogPtr pdialog, short itemnumber) {
	
	short itemtype;
	Handle itemhandle;
	Rect itemrect;
	
	GetDialogItem (pdialog, itemnumber, &itemtype, &itemhandle, &itemrect);
	
	if (itemtype >= itemDisable) { /*it is disabled, enable it*/
		
		SetDialogItem (pdialog, itemnumber, itemtype - itemDisable, itemhandle, &itemrect);
		
		if (dialogitemtypeiscontrol (itemtype))
			HiliteControl ((ControlHandle) itemhandle, 0);
		}
	} /*enabledialogitem*/


void hidedialogitem (DialogPtr pdialog, short itemnumber) {
	
	HideDialogItem (pdialog, itemnumber);
	} /*hidedialogitem*/
	

void showdialogitem (DialogPtr pdialog, short itemnumber) {
	
	ShowDialogItem (pdialog, itemnumber);
	} /*showdialogitem*/
	

void setdefaultitem (DialogPtr pdialog, short defaultitem) {
	
	//Code change by Timothy Paustian Sunday, April 30, 2000 9:47:39 PM
	//Changed to Opaque call for Carbon
	SetDialogDefaultItem(pdialog, defaultitem);
	//old code
	//(*(DialogPeek) pdialog).aDefItem = defaultitem; /*filter will bolden this*/
	} /*setdefaultitem*/


static boolean dialogitemisenabled (DialogPtr pdialog, short item) {
	
	short itemtype;
	Handle itemhandle;
	Rect itemrect;
	
	GetDialogItem (pdialog, item, &itemtype, &itemhandle, &itemrect);
	
	return ((itemtype & itemDisable) == 0);
	} /*dialogitemisenabled*/


boolean dialogitemisbutton (DialogPtr pdialog, short item) {
	
	short itemtype;
	Handle itemhandle;
	Rect itemrect;
	
	if (item <= 0)
		return (false);
	
	GetDialogItem (pdialog, item, &itemtype, &itemhandle, &itemrect);
	
	return (dialogitemtypeiscontrol (itemtype));
	} /*dialogitemisbutton*/
	

static boolean dialogitemisedittext (DialogPtr pdialog, short item) {
	
	short itemtype;
	Handle itemhandle;
	Rect itemrect;
	
	GetDialogItem (pdialog, item, &itemtype, &itemhandle, &itemrect);
	
	return (itemtype & editText); 
	} /*dialogitemisedittext*/
	

static boolean dialoghasedititems (DialogPtr pdialog) {
	
	register short i;
	register short ctitems;
	
	ctitems = dialogcountitems (pdialog);
	
	for (i = 1; i <= ctitems; i++) 
		if (dialogitemisedittext (pdialog, i))
			return (true);
	
	return (false);
	} /*dialoghasedititems*/


static void dialoggetbuttonstring (DialogPtr pdialog, short item, bigstring bs) {
	
	short itemtype;
	Handle itemhandle;
	Rect itemrect;
	//Code change by Timothy Paustian Sunday, April 30, 2000 9:57:08 PM
	//Changed to Opaque call for Carbon
	ControlRef	theControl;
	Str255		controlTitle;
	
	GetDialogItem (pdialog, item, &itemtype, &itemhandle, &itemrect);
	//Code change by Timothy Paustian Sunday, April 30, 2000 9:57:41 PM
	//Changed to Opaque call for Carbon
	//watch this one. What happens if this is a bad cast to a ControlRef?
	theControl = (ControlRef) itemhandle;
	GetControlTitle(theControl, controlTitle);
	copystring (controlTitle, bs);
	//old code
	//copystring ((**(ControlRef) itemhandle).contrlTitle, bs);
	} /*dialoggetbuttonstring*/


static void dialogsetbuttonstring (DialogPtr pdialog, short item, bigstring bs) {
	
	short itemtype;
	Handle itemhandle;
	Rect itemrect;

	GetDialogItem (pdialog, item, &itemtype, &itemhandle, &itemrect);
	
	SetControlTitle ((ControlHandle) itemhandle, bs);
	} /*dialogsetbuttonstring*/


DialogPtr newmodaldialog (short id, short defaultitem) {
	
	/*
	2/7/92 dmb: if dialoghasedititems, force shell to export scrap
	
	2/10/92 dmb: added call to new shellactivate
	
	3.0.1b1 dmb: don't call shellactivate if the dialog isn't of a modal 
	design, i.e. a dBoxProc window. currently, the only case where 
	this isn't so is Frontier's about window / splash screen.  having 
	the splash screen force Frontier to the front is bad news for 
	background launching. it even crashes! 
	*/
	
	register DialogPtr pdialog;
	
	#ifdef fldebug
	
	if (GetResource ('DLOG', id) == nil)
		DebugStr ("\pmissing DLOG resource");
	
	#endif
	
	
	
	pdialog = GetNewDialog (id, nil, (WindowRef) -1L);
	
	
	RestoreA5 (appA5);
	
	
	if (pdialog == nil) 
		return (nil);
	
	positiondialogwindow (pdialog);
	
	setdefaultitem (pdialog, defaultitem);
	
	if (dialoghasedititems (pdialog))
		shellwritescrap (textscraptype);
	
	{
	WindowPtr	theWind = GetDialogWindow(pdialog);
	if (GetWVariant (theWind) == dBoxProc) /*make sure we're in front before posting modal dialog*/
		shellactivate ();
	}
	return (pdialog);
	} /*newmodaldialog*/


void disposemodaldialog (DialogPtr pdialog) {
	
	/*
	7/20/92 dmb: don't do partial event loop for runtime.
	
	1/21/93 dmb: use langpartialeventloop when a script is running
	
	2.1b dmb: if this was an out-of-memory alert, the partial event loop is 
	bad news. it's always tende to cause problems, so and shouldn't really 
	be necessary. so let's just not do it!
	*/
	
	DisposeDialog (pdialog);
	
	} /*disposemodaldialog*/


void setdialogcheckbox (DialogPtr pdialog, short item, boolean fl) {

	/*
	change the value of the checkbox.
	*/

	short itemtype;
	Rect itemrect;
	Handle itemhandle;
	
	GetDialogItem (pdialog, item, &itemtype, &itemhandle, &itemrect);
		
	SetControlValue ((ControlHandle) itemhandle, fl);
	} /*setdialogcheckbox*/


boolean getdialogcheckbox (DialogPtr pdialog, short item) {

	/*
	get the value of the checkbox, either a 1 or a 0.
	*/

	short itemtype;
	Rect itemrect;
	Handle itemhandle;
	
	GetDialogItem (pdialog, item, &itemtype, &itemhandle, &itemrect);
	
	return (bitboolean (GetControlValue ((ControlHandle) itemhandle)));
	} /*getdialogcheckbox*/


void toggledialogcheckbox (DialogPtr pdialog, short item) {

	/*
	if the checkbox is on, turn it off, and vice versa.
	*/

	short itemtype;
	Rect itemrect;
	Handle itemhandle;
	
	GetDialogItem (pdialog, item, &itemtype, &itemhandle, &itemrect);
	
	SetControlValue ((ControlHandle) itemhandle, !GetControlValue ((ControlHandle) itemhandle));
	} /*toggledialogcheckbox*/


boolean setdialogradiovalue (DialogPtr pdialog, short firstitem, short lastitem, short val) {

	/*
	set the value of the radio button range.  val is zero-based; valid 
	values range from zero to (firstitem - lastitem)
	*/
	
	register short item;
	register boolean flon;
	
	for (item = firstitem; item <= lastitem; ++item) {
		
		flon = val == (item - firstitem);
		
		if (flon != getdialogcheckbox (pdialog, item))
			setdialogcheckbox (pdialog, item, flon);
		}
	
	return (true);
	} /*getdialogradiovalue*/


short getdialogradiovalue (DialogPtr pdialog, short firstitem, short lastitem) {
	
	/*
	set the value of the radio button range.  val is zero-based; returned 
	value will range from zero to (firstitem - lastitem).
	
	if none of the radio buttons are set, returns -1.
	*/
	
	register short item;
	
	for (item = firstitem; item <= lastitem; ++item) {
		
		if (getdialogcheckbox (pdialog, item))
			return (item - firstitem);
		}
	
	return (-1);
	} /*getdialogradiovalue*/


void getdialogtext (DialogPtr pdialog, short itemnumber, bigstring bs) {
	
	short itemtype;
	Handle itemhandle;
	Rect itemrect;
	
	GetDialogItem (pdialog, itemnumber, &itemtype, &itemhandle, &itemrect);
	GetDialogItemText (itemhandle, bs);
	
	} /*getdialogtext*/


static void dialogscanspecialchars (bigstring bs) {

	register short i, ct;
	
	ct = stringlength (bs);
	
	for (i = 1; i <= ct; i++) {
		
		register char ch = bs [i];
		
