
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

#define shellprivateinclude /*so other includes can tell if we've been loaded*/

#ifndef shellcoreinclude
	#include "shellcore.h"
#endif

/*
names that communicate between the various files that make up the shell level.

handlers are not supposed to include this file.
*/

#define jugglerEvt 15

#define tickstoupdatemenus 20 /*update menus one-third second after last event*/

#define tickstoidle 6 /*only call idle callback routine every tenth second*/

/* ctglobals and tyglobalsstack now defined in shellcore.h */

typedef enum tymenustate {
	dirtymenus,
	
	normalmenus,
	
	optionmenus,
	
	modaldialogmenus
	} tymenustate;


	#define cteditors 14


/*globals*/

extern tyshellglobals globalsarray [cteditors];

extern short topglobalsarray; /*initially empty*/

extern tyglobalsstack globalsstack; /*for push/pop globals*/

extern boolean flexitmainloop;

// 2006-03-31 kw & aradke: menustate is declared static in shellmenu.c
//extern tymenustate menustate;

extern unsigned long timelastkeystroke;

// 2006-03-31 kw & aradke: flshellimmediatebackground is declared static in shell.c
//extern boolean flshellimmediatebackground; /*service the background queue immediately*/


/*prototypes*/

extern boolean shellfindcallbacks (short, short *); /*shellcallbacks.c*/

extern void shellpatchnilroutines (void);

extern void shellinithandlers (void);

extern void shellloadbuttonlists (void);

extern void shellclearwindowdata (void);


extern boolean shellautoopen (bigstring, short); /*shellfile.c*/

extern void killownedundo (WindowPtr);


extern void shellhandlejugglerevent (void); /*shelljuggler.c*/


extern void shelladjustmenus (void); /*shellmenu.c*/


/* 5.0a5 dmb - no longer needed: extern void shellsetscrollbarinfo (void); /%shellops.c*/


extern boolean shellgetgrowiconrect (hdlwindowinfo, Rect *); /*shellwindow.c*/

extern void shelldrawgrowicon (hdlwindowinfo);

extern void shellerasegrowicon (hdlwindowinfo);

extern boolean isshellwindow (WindowPtr);

extern boolean frontshellwindow (WindowPtr *);

extern boolean shellsavewindowresource (WindowPtr, ptrfilespec, short);

extern boolean shellsavewindowposition (WindowPtr);

extern boolean loadwindowposition (ptrfilespec, short, tywindowposition *);

extern boolean shellsavefontresource (WindowPtr, ptrfilespec, short);

extern boolean shellsavedefaultfont (WindowPtr);

extern boolean loaddefaultfont (WindowPtr);

extern void getdefaultwindowrect (Rect *);

extern void shellresetwindowrects (hdlwindowinfo);

extern void windowresetrects (hdlwindowinfo);

extern boolean emptywindowlist (void);

extern short countwindowlist (void);

extern boolean indexwindowlist (short, hdlwindowinfo *);

extern short counttypedwindows (short);

extern boolean shellfirstchildwindow (hdlwindowinfo, hdlwindowinfo *);

extern void grayownedwindows (WindowPtr);

extern boolean defaultselectioninfo (hdlwindowinfo);

extern boolean newshellwindowinfo (WindowPtr w, hdlwindowinfo *hinfo);

extern void disposeshellwindowinfo (hdlwindowinfo hinfo);

extern boolean newshellwindow (WindowPtr *, hdlwindowinfo *, tywindowposition *);

extern boolean windowinit (WindowPtr);

extern boolean zoomfilewindow (WindowPtr);

extern boolean getwindowmessage (WindowPtr, bigstring);

extern boolean drawwindowmessage (WindowPtr);

extern boolean setwindowmessage (WindowPtr, bigstring);

extern boolean lockwindowmessage (WindowPtr, boolean);

extern void shellerasemessagearea (hdlwindowinfo);

extern boolean windowmadechanges (WindowPtr);

extern boolean windowgetcontentrect (WindowPtr, Rect *);

extern void disposeshellwindow (WindowPtr);

extern boolean shellhidewindow (hdlwindowinfo);

extern boolean shellunhidewindow (hdlwindowinfo);

extern void closewindowfile (WindowPtr);

extern boolean shellfrontrootwindowmessage (bigstring);

extern boolean shellrunwindowconfirmationscript (WindowPtr, short);


extern boolean shellpushtargetglobals (void); /*shellverbs.c*/

extern boolean shellinitverbs (void);




