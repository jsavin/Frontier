
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
player.c -- This file handles the QuickTime player window.

PBS 08/08/00
*/ 


#include "frontier.h"
#include "standard.h"

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
#include "frontierwindows.h"
#include "windowlayout.h"
#include "zoom.h"
#include "shell.h"
#include "shellprivate.h"
#include "shellhooks.h"
#include "player.h"
#include "tablestructure.h"
#include "cancoon.h"
#include "cancooninternal.h"
#include "process.h"
#include "processinternal.h"
#include "lang.h"
#include "langinternal.h"


#define moviestasktime 30

#pragma pack(2)
typedef struct typlayerrecord {
	
	long refcon;
	} typlayerrecord, *ptrplayerrecord, **hdlplayerrecord;
#pragma options align=reset


static hdlplayerrecord playerdata = nil;

static WindowPtr playerwindow = nil;

static hdlwindowinfo playerwindowinfo;

static hdlplayerrecord displayedplayerdata = nil;

static WindowPtr playerport = nil;

static Movie currentmovie = nil;

static MovieController currentcontroller = nil;

static Rect currentmovierect;

static byte * playerstrings [] = {

	BIGSTRING ("\x06" "Player"),

	};


enum { /*player window items*/
	
	titleitem
	};


/*Prototypes*/

static boolean newplayerwindow (void);

static void playerwindowsetup (void);

static boolean attachplayer (void);

static boolean playerresetrects (hdlwindowinfo hinfo);

static boolean findplayerwindow (hdlwindowinfo *hinfo);

static void playersetconfigminimum (void);

static void detachplayer (void);

static void playerdisposecurrentmovie (void);

pascal Boolean playermoviecontrollereventfilter (MovieController mc, short action, void *params, long refcon);


/*Functions*/

boolean playeropencommand (void) {
	
	/*
	Called to open the player window.
	*/
	
	hdlwindowinfo hinfo;
	
	if (findplayerwindow (&hinfo)) {
		
		/*If the window is already open,
		bring it to the front.*/
		
		shellbringtofront (hinfo);
		
		return (true);
		} /*if*/
	
	/*Create and open a new player window.*/
	
	return (newplayerwindow ());
	} /*playeropencommand*/


static boolean newplayerwindow (void) {
	
	WindowPtr w;
	hdlwindowinfo hw;
	bigstring bstitle;
	Rect rzoom, rwindow;
	hdlplayerrecord hdata;
	short ixplayerconfig;
	
	if (!newclearhandle (sizeof (typlayerrecord), (Handle *) &hdata))
		return (false);
	
	shellfindcallbacks (idplayerconfig, &ixplayerconfig);
		
	getsystemoriginrect (&rzoom);
	
	rwindow = rzoom;
	
	rwindow.top = 100;
	
	rwindow.left = 100;
	
	rwindow.right = 300;
	
	rwindow.bottom = 200;
		
	copystring (playerstrings [titleitem], bstitle);
	
	if (!newchildwindow (idplayerconfig, nil, &rwindow, &rzoom, bstitle, &w)) {
		
		disposehandle ((Handle) hdata);
		
		return (false);
		}
	
	getwindowinfo (w, &hw);
	
	(**hw).hdata = (Handle) hdata;
	
	shellpushglobals (w);
	
	playerwindowsetup ();
	
	playerresetrects (hw);
	
	playersetconfigminimum ();

	shellpopglobals ();
	
	windowzoom (w);
	
	return (true);	
	} /*newplayerwindow*/


static void playerwindowsetup (void) {
			
	if (attachplayer ()) {
		
		shellpushrootglobals (playerwindow);

		shellpopglobals ();

		detachplayer ();
		}
	} /*playerwindowsetup*/


static boolean attachplayer (void) {
	
	hdlwindowinfo hroot;
	
	if (playerdata == nil)
		return (false);

	if (!ccfindrootwindow (&hroot)) {
		
		(**playerwindowinfo).parentwindow = nil;
		
		return (false);
		}
	
	(**playerwindowinfo).parentwindow = hroot;
	
	return (true);
	} /*attachplayer*/


static boolean playerresetrects (hdlwindowinfo hinfo) {
#pragma unused (hinfo)

	return (true);
	} /*playerresetrects*/


static boolean findplayerwindow (hdlwindowinfo *hinfo) {

	WindowPtr w;
	Handle hdata;
	
	return (shellfindwindow (idplayerconfig, &w, hinfo, &hdata));
	} /*findplayerwindow*/


static void playersetconfigminimum (void) {
	
	short ixplayerconfig;
	Rect *rmin;
	
	if (shellfindcallbacks (idplayerconfig, &ixplayerconfig)) {
	
		rmin = &globalsarray [ixplayerconfig].config.rmin;
		
		(*rmin).bottom = 200;
	
		(*rmin).right = 200;
		
		(*rmin).left = 150;
		
		(*rmin).top = 150;

		}
	} /*playersetconfigminimum*/


static void detachplayer (void) {
	
	(**playerwindowinfo).parentwindow = nil;
	} /*detachplayer*/




static boolean playerdisposerecord (void) {
	
	disposehandle ((Handle) playerdata);
	
	playerdata = nil;
	
	displayedplayerdata = nil;
	
	playerport = nil;
	
	playerdisposecurrentmovie ();

	return (true);
	} /*playerdisposerecord*/


static boolean playerclose (void) {
	
	playerdisposecurrentmovie ();
		
	detachplayer ();
	
	playerwindow = nil;
	
	return (true);
	} /*playerclose*/


static boolean playergettargetdata (short id) {
	
	return (id == -1); /*true if target type is generic -- a shell verb*/
	} /*playergettargetdata*/

	


static void playerdisposecurrentmovie (void) {
	
	/*
	7.0b4 PBS: dispose the current movie and controller, if there is one.
	*/
	
	if (currentcontroller != nil) {
		
		DisposeMovieController (currentcontroller);
		
		currentcontroller = nil;
		}

	if (currentmovie != nil) {
		
		DisposeMovie (currentmovie);
		
		currentmovie = nil;	
		}	
	} /*playerdisposecurrentmovie*/


boolean isplayerevent (void) {
	
	/*
	7.0b4 PBS: called from the main event loop.
	QuickTime needs to catch some events.
	Return true if the event is consumed by QuickTime
	and should be ignored by the event loop.
	*/
	
	boolean fl = false;
	
	if (currentmovie == nil) /*if no current movie, return right away*/
	
		return (fl);
	
	if (currentcontroller == nil) /*if no controller, return right away*/
		
		return (fl);
	
	if (MCIsPlayerEvent (currentcontroller, &shellevent))
		
		fl = true; /*the event was consumed by QuickTime*/
	
	MCIdle (currentcontroller);
	
	MoviesTask (nil, 0);
		
	return (fl);
	} /*isplayerevent*/


pascal Boolean playermoviecontrollereventfilter (MovieController mc, short action, void *params, long refcon) {
#pragma unused (mc, refcon, params)

	/*
	7.0b4 PBS: Handle movie controller events.
	Unused -- but there's a good chance it will be needed later.
	*/

	if (action == mcActionControllerSizeChanged) {
	
		Rect bounds;
		
		MCGetControllerBoundsRect (currentcontroller, &bounds);
		
		SizeWindow (playerwindow, bounds.right, bounds.bottom, false);
		} /*if*/

	return (false); /*Further processing should occur.*/	
	} /*playermoviecontrollereventfilter*/


boolean playeropenmovieinwindow ( ptrfilespec f ) {
	
	//
	// QuickTime video playback is no longer supported in this version.
	//
	
	// QuickTime is no longer supported - return false
	return (false);
	} // playeropenmovieinwindow


boolean playerplaymovie (void) {
	
	/*
	QuickTime video playback is no longer supported in this version.
	*/
	
	// QuickTime is no longer supported - return false
	return (false);
	} /*playerplaymovie*/


boolean playerstopmovie (void) {
	
	/*
	QuickTime video playback is no longer supported in this version.
	*/
	
	// QuickTime is no longer supported - return false
	return (false);
	} /*playerstopmovie*/


boolean playerisplaying (void) {
	
	/*
	7.0b4 PBS: return true if the player window is playing a movie.
	*/
	
	if (currentmovie == nil)
		
		return (false);
	
	if (IsMovieDone (currentmovie))
	
		return (false);
	
	return (GetMovieActive (currentmovie));
	} /*playerisplaying*/


static void playerquit (void) {
	
	/*
	7.0b4 PBS: quit routine.
	*/
	
	playerdisposecurrentmovie ();
	} /*playerquit*/


static void playeractivate (boolean fl) {

	#pragma unused (fl)

	return;	
	} /*playeractivate*/


static void playeridle (void) {
	
	/*
	7.0b4 PBS: idle routine.
	*/
	
	if (currentcontroller != nil) {
		
		MCIdle (currentcontroller); /*Give time to QuickTime.*/
		}
	} /*playeridle*/


boolean playerstart (void) {
	
	/*
	7.0b4 PBS: Set up callback routines record, and link our data into the shell's 
	data structure.
	*/
	
	ptrcallbacks playercallbacks;
	register ptrcallbacks cb;
	static boolean playerstarted = false;
	
	if (playerstarted)
		return (true);
	
	playerstarted = true;
	
	EnterMovies (); /*init QuickTime*/
			
	shellnewcallbacks (&playercallbacks);
	
	cb = playercallbacks; /*copy into register*/
	
	loadconfigresource (idplayerconfig, &(*cb).config);
	
	(*cb).configresnum = idplayerconfig;
		
	(*cb).windowholder = &playerwindow;
	
	(*cb).dataholder = (Handle *) &playerdata;
	
	(*cb).infoholder = &playerwindowinfo;
	
	(*cb).setglobalsroutine = &attachplayer;
	
	(*cb).quitroutine = &playerquit;
		
	(*cb).disposerecordroutine = &playerdisposerecord;
	
	(*cb).closeroutine = &playerclose;
	
	(*cb).gettargetdataroutine = &playergettargetdata;
		
	(*cb).activateroutine = &playeractivate;
	
	(*cb).idleroutine = &playeridle;
	
	return (true);
	} /*playerstart*/

