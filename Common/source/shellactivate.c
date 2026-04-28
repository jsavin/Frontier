
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

#include "quickdraw.h"
#include "scrap.h"
#include "scrollbar.h"
#include "frontierwindows.h"
#include "shell.h"
#include "shellbuttons.h"
#include "shellprivate.h"


boolean shellactivatewindow (WindowPtr w, boolean flactivate) {
	
	/*
	activate or deactivate the indicated window.
	
	8/31/90 DW: we no longer set the globals at the end of the routine. 
	
	10/11/90 dmb: set superglobals up now
	
	3/30/93 dmb: make sure it's a shell window
	
	2.1a7 dmb: fixed shaging of display so newly-opened windows look good
	
	4.1b4 dmb: overload the config.fleraseonresize flag to avoid updating 
	iowa windows while activating

	5.0a10 dmb: dirty the window menu
	*/
	
	register hdlwindowinfo hinfo;
	register hdlscrollbar vertbar, horizbar;
	//Code change by Timothy Paustian Saturday, April 29, 2000 11:03:12 PM
	//Changed to Opaque call for Carbon
	//moved r to outside, since need for carbon call below
	Rect r;
	
	
	if (!isshellwindow (w)) /*chase the refcon field of the mac window*/
		return (false);
	
	shellwindowmenudirty ();

	shellsetsuperglobals (); /*do everyone a favor - make superglobals current right away*/
	
	if (!shellpushglobals (w)) /*install globals of newly active or de-active window*/
		return (false);
	
	hinfo = shellwindowinfo; /*copy into register*/
	
	(**hinfo).flwindowactive = bitboolean (flactivate);

	#if ACCESSOR_CALLS_ARE_FUNCTIONS == 1
	{
	CGrafPtr	thePort;
	thePort = GetWindowPort(w);
	GetPortBounds(thePort, &r);	
	}
	#else
	r = w->portRect;
	#endif
	pushclip (r); /*make sure we can draw into whole window*/
	

	shelldrawbuttons (); /*if window has an attached button list, draw it*/
	
	vertbar = (**hinfo).vertscrollbar;
	
	horizbar = (**hinfo).horizscrollbar;
	
	if (flactivate) {
	
		shellsetscrollbars (w);
		
		displayscrollbar (vertbar);
		
		displayscrollbar (horizbar);
		
		(**hinfo).selectioninfo.fldirty = true; /*dmb: force menu update*/
		}
	
	else {

		disablescrollbar (vertbar);
		
		disablescrollbar (horizbar);
	
	
	shellwritescrap (anyscraptype);
		
		}
	
	shelldrawgrowicon (hinfo);
	
	drawwindowmessage (w);
	
	popclip ();
	
	if ((*shellglobals.dataholder != NULL)) { // 4/8.97 dmb: window has conent
		
		pushclip ((**hinfo).contentrect); /*driver limited to drawing in content rect*/
		
		(*shellglobals.activateroutine) (bitboolean (flactivate));
		
		popclip ();

		}
	
	shellpopglobals ();
	
	return (true);
	} /*shellactivatewindow*/




