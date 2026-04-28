
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
#include "font.h"
#include "smallicon.h"
#include "textdisplay.h"


#define horizinset 0
#define vertinset 0




void gettextdisplayinfo (Rect r, short fontnum, short fontsize, short fontstyle, tylinespacing spacing, short horizscrollunit, tytextdisplayinfo *info) {

	/*
	this is a central bottleneck -- anyone who wants to know where lines
	are, how big they are, how big the display is, etc, calls this 
	routine.  the displayinfo field of the outline record is filled.
	
	we establish the display size and the position for the first line
	of text, also the height of each line.
	
	we add a little extra space above each line to balance the space
	that's left when most characters don't descend.  makes the display
	look more balanced, but the coding is a little wierd...
	*/
	
	register short r1, r2, r3; /*registers that get re-used*/
	tytextdisplayinfo displayinfo;
	
	displayinfo.r = r;
	
	insetrect (&displayinfo.r, horizinset, vertinset);
	
	pushstyle (fontnum, fontsize, fontstyle);
	
	r1 = globalfontinfo.ascent + globalfontinfo.descent;
	
	r2 = globalfontinfo.descent;
	
	r3 = r1 + r2;
	
	switch (spacing) {
		
		case singlespaced:
			break;
		
		case oneandalittlespaced:
			r3 += 1; break;
	
		case oneandaquarterspaced:
			r3 += (r3 / 4); break;
			
		case oneandahalfspaced:
			r3 += (r3 / 2); break;
			
		case doublespaced:
			r3 *= 2; break;
			
		case triplespaced:
			r3 *= 3; break;
		
		} /*switch*/
	
	r3 = max (r3, heightsmallicon - 2); /*make sure it can handle an icon (almost)*/
		
	displayinfo.lh = r3;
	
	displayinfo.v = displayinfo.r.top + globalfontinfo.ascent + r2;
	
	displayinfo.h = displayinfo.r.left;
	
	r1 = displayinfo.r.bottom - displayinfo.r.top;
	
	displayinfo.screenlines = r1 / r3;
	
	r2 = r1 - (displayinfo.screenlines * r3); /*number of extra pixels*/
	
	r1 = r2 / 2; /*number of pixels added at top of display rectangle*/
	
	displayinfo.r.top += r1;
	
	displayinfo.v += r1;
	
	displayinfo.r.bottom -= r2 - r1; /*subtract the complementary number of pixels from bottom*/
	
	displayinfo.horizscrollpixels = horizscrollunit;
	
	popstyle ();
	
	*info = displayinfo;	
	} /*gettextdisplayinfo*/
	

