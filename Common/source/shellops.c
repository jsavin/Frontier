
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

#include "frontier.h"
#include "standard.h"

#include "font.h"
#include "quickdraw.h"
#include "scrollbar.h"
#include "shell.h"
#include "shellprivate.h"




/*
void shellsetscrollbarinfo (void) {

	register hdlwindowinfo h = shellwindowinfo;
	
	setscrollbarinfo (
		(**h).vertscrollbar, (**h).vertmin, (**h).vertmax, (**h).vertcurrent);

	setscrollbarinfo (
		(**h).horizscrollbar, (**h).horizmin, (**h).horizmax, (**h).horizcurrent);
	} /%shellsetscrollbarinfo%/
*/

void shellsetdefaultstyle (hdlwindowinfo hinfo) {
	
	/*
	set the defaultstyle field of the windowinfo record based on the bits set in
	the selectioninfo record.
	
	6/28/91 dmb: now use union for style to make things a lot easier
	*/
	
	register hdlwindowinfo hw = hinfo;
	register short x = (**hw).selectioninfo.fontstyle;
	register Style style;
	
	if (x == 0)
		style = normal;
	else
		style = x ^ (**hw).defaultstyle;
	
	(**hw).defaultstyle = style;
	} /*shellsetdefaultstyle*/


void shellsetselectioninfo (void) {
	
	/*
	2/12/92 dmb: keep window's font/size in sync with settings
	*/
	
	register hdlwindowinfo hw = shellwindowinfo;
	
	(*shellglobals.setselectioninforoutine) (); /*fill in selectioninfo record*/
	
	if (shellwindow != nil) {
		
		//Code change by Timothy Paustian Monday, August 21, 2000 4:31:49 PM
	//Must pass a CGrafPtr to pushport on OS X to avoid a crash
	CGrafPtr	thePort;
	thePort = GetWindowPort(shellwindow);
		
	pushport (thePort);
		
		setfontsizestyle ((**hw).selectioninfo.fontnum, (**hw).selectioninfo.fontsize, 0);
		
		popport ();
		}
	
	(**hw).selectioninfo.fldirty = false; /*consume its dirtyness*/
	} /*shellsetselectioninfo*/





