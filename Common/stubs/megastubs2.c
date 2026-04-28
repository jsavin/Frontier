
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

#include "frontier.h"
#include "standard.h"

	#include <IAC.h>
	#include <uisharing.h>
	#include "filealias.h"

#include "bitmaps.h"
#include "dialogs.h"
#include "memory.h"
#include "error.h"
#include "file.h"
#include "launch.h"
#include "fileloop.h"
#include "strings.h"
#include "frontierwindows.h"
#include "zoom.h"
#include "lang.h"
#include "langinternal.h"
#include "langipc.h"
#include "langexternal.h"
#include "langsystem7.h"
#include "scripts.h"
#include "shell.h"
#include "shellbuttons.h"
#include "shellhooks.h"
#include "shellprint.h"

#include "osacomponent.h"

#include "process.h"



// tyshellglobals shellglobals;


// bitmaps.c

extern HWND currentport;
extern HDC currentportDC;

static HDC offscreenDC = NULL;
static HBITMAP oldbitmap;
static Rect offscreenrect;
static HWND saveDC;


void initbitmaps (boolean fl) {
	} /*initbitmaps*/


boolean openbitmap (Rect r, WindowPtr w) {
	
	/*
	5.0fc2 rab: plugged memory leak calling GetDC
	*/

	HDC hdc = NULL; // = GetDC (w);
	HBITMAP bitmap;
	
	assert (w == currentport);

	offscreenDC = NULL; //*** disable: CreateCompatibleDC (hdc);
	
	if (offscreenDC == NULL)
		return (false);

	bitmap = CreateCompatibleBitmap (hdc, r.right - r.left, r.bottom - r.top);
	
	if (bitmap == NULL) {
		
		DeleteDC (offscreenDC);
		
		offscreenDC = NULL;

		return (false);
		}
	
	oldbitmap = SelectObject (offscreenDC, bitmap);
	
	SetViewportOrgEx (offscreenDC, r.left, r.top, NULL);
	
	offscreenrect = r;
	
	saveDC = (HWND) currentportDC;

	currentportDC = offscreenDC;

	return (true);
	} /*openbitmap*/


//boolean openbitmapcopy (Rect r, WindowPtr w) {return false;}

void closebitmap (WindowPtr w) {
	
	HBITMAP bitmap;
	Rect *r = &offscreenrect;

	if (offscreenDC != NULL) {

		currentport = w;
		
		currentportDC = (HDC) saveDC;
		
		BitBlt ((HDC) saveDC, r->left, r->top, r->right - r->left, r->bottom - r->top, offscreenDC, 0,0, SRCAND);
		
		bitmap = SelectObject (offscreenDC, oldbitmap);
		
		DeleteObject (bitmap);

		DeleteDC (offscreenDC);

		offscreenDC = NULL;
		}
	} /*closebitmap*/


// launch.c

boolean activateapplicationwindow (typrocessid id, WindowPtr w) {return (false);}


// zoom.c

void zoominit (void) {}

void zoomtoorigin (WindowPtr w) {hidewindow (w);}

void zoomfromorigin (WindowPtr w) {
	
	/*
	5.0.1 dmb: call SetFocus. Fixes the "Deaf QuickScript" bug.
	*/

	releasethreadglobals ();

	SetWindowPos (w, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
	ShowWindow (w, SW_SHOWNORMAL);
	SetFocus (w);

	grabthreadglobals ();
	
	/*
	config = origconfig;
	shellglobals = origglobals;
	setport (saveport);
	*/

	shellactivatewindow (w, true);
	} /*zoomfromorigin*/

void zoomwindowfromcenter (Rect r, WindowPtr w) {
	
	zoomfromorigin (w);
	} /*zoomwindowfromcenter*/

void zoomwindowtocenter (Rect r, WindowPtr w) {
	
	hidewindow (w);
	} /*zoomwindowtocenter*/

void zoomsetdefaultrect (WindowPtr w, Rect r) {}



// osacomponent.c

void osacomponentshutdown (void) {};


// langdialog.c

boolean langdialogrunning (void) {return false;}


// langmodeless.c

boolean langrunmodeless (hdltreenode hparam1, tyvaluerecord *vreturned) {return false;}


// langipc.c

typrocessid langipcself;
static boolean fltoolkitinitialized = false;


static pascal Boolean openfilespec (ptrfilespec pfs) {
	
	if (!shellopenfile (pfs)) {
		
		IACreturnerror (getoserror (), nil);
		
		return (false);
		}
		
	return (true);
	} /*openfilespec*/
	

static pascal OSErr handleopen (AppleEvent *event, AppleEvent *reply, long refcon) {
	
	IACglobals.event = event;
	
	IACglobals.reply = reply;
	
	IACglobals.refcon = refcon;
	
	return (IACdrivefilelist (&openfilespec));
	} /*handleopen*/

		
static pascal OSErr handlequit (AppleEvent *event, AppleEvent *reply, long refcon) {
	
	if (!shellcloseall (nil, true)) /*user hit Cancel button in save dialog*/
		return (userCanceledErr);
	
	shellexitmaineventloop (); /*sets flag for next iteration*/
	
	return (noErr);
	} /*handlequit*/
	
	
static pascal OSErr handleopenapp (const AppleEvent *event, AppleEvent *reply, long refcon) {
	
	if (!shellopendefaultfile ())
		return (getoserror ());
	
	return (noErr);
	} /*handleopenapp*/


boolean landeventfilter (EventRecord *ev) {
	
	/*
	watch for high level events (AppleEvents)
	
	return true if we consume the event, false otherwise.
	*/
	
	if ((*ev).what == kHighLevelEvent) {
	
		AEProcessAppleEvent (ev);
		
		return (true); /*consume the event*/		
		}
	
	return (false); /*don't consume the event*/
	} /*landeventfilter*/


boolean langipcinit (void) {
	
	return (true);
	} /*langipcinit*/


boolean langipcstart (void) {
	
	/*
	register with the IAC toolkit, using the identifier defined in land.h, 
	and registering the verbs that scriptrunners must implement.
	*/
	
	
	GetCurrentProcess (&langipcself);
	
	if (!IACinstallhandler (kCoreEventClass, kAEOpenApplication, (ProcPtr) &handleopenapp))
		goto error;
	
	if (!IACinstallhandler (kCoreEventClass, kAEOpenDocuments, (ProcPtr) &handleopen))
		goto error;
	
	if (!IACinstallhandler (kCoreEventClass, kAEQuitApplication, (ProcPtr) &handlequit))
		goto error;

	error:
	

	return (true);
	} /*langipcstart*/

void langipcshutdown () {}

// misc

//void initsegment (void) {}



// uiSharing

Boolean isModelessCardEvent (EventRecord *ev);
Boolean isModelessCardEvent (EventRecord *ev) {return false;}

Boolean uisIsSharedWindow (WindowPtr w) {return false;}

Boolean uisCloseSharedWindow (WindowPtr w) {return false;}

void uisCloseAllSharedWindows (void) {}

Boolean uisEdit (short item) {return false;}
	

// launch.c

typrocessid getcurrentprocessid (void) {
	
	ProcessSerialNumber psn;
	
	GetCurrentProcess (&psn);
	
	return (psn);
	} /*getcurrentprocessid*/


boolean getapplicationfilespec (bigstring bsprogram, tyfilespec *fs) {
	
	ProcessInfoRec processinfo;
	ProcessSerialNumber psn;
	
	assert (bsprogram == nil);
	
	processinfo.processInfoLength = sizeof (processinfo);
	processinfo.processName = nil; /*place to store process name*/
	processinfo.processAppSpec = fs; /*place to store process filespec*/
	
	psn.highLongOfPSN = 0;
	psn.lowLongOfPSN = kCurrentProcess;
	
	if (GetProcessInformation (&psn, &processinfo) != noErr)
		return (false);
	
	return (true);
	} /*getapplicationfilespec*/


// about.c

void aboutsegment (void) {}

boolean openabout (boolean fl, long n) {return false;}



