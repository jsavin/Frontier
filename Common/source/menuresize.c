
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

#include "quickdraw.h"
#include "popup.h"
#include "windowlayout.h"
#include "scrollbar.h"
#include "dialogs.h"
#include "shell.h"
#include "shellbuttons.h"
#include "op.h"
#include "opinternal.h"
#include "menueditor.h"
#include "menuinternal.h"



#define itemmargin 8

// 5.0a25 dmb: wire off Zoom button
#undef iconrectheight
#undef iconrectwidth
#define iconrectheight 0
#define iconrectwidth 0



static void mesetalloutlinerects (Rect r) {

	oppushoutline ((**menudata).menuoutline);
	
	insetrect (&r, 1, 1);
	
	opresize (r);
	

	oppopoutline ();
	} /*mesetalloutlinerects*/


boolean meresetwindowrects (hdlwindowinfo hw) {
	
	register hdlmenurecord hm = (hdlmenurecord) (**hw).hdata;
	short scrollbarwidth;
	short msgheight;
	Rect rcontent;
	Rect menurect;
	Rect r;
	
	if (op_get_outlinedata() == nil)
		return (false);
	
	/*set up some values for all rect computations*/ {
	
		scrollbarwidth = getscrollbarwidth ();
		
		msgheight = popupheight; 
		}
	
	/*get contentrect and do info.growiconrect*/ {
		
		rcontent = (**hw).contentrect;
		
		shellcalcgrowiconrect (rcontent, hw);
		}
	
	/*inset contentrect uniformly*/ {
		
		insetrect (&rcontent, windowmargin, windowmargin);
		}
	
	/*do iconrect*/ {
	
		r = rcontent;
		
		r.bottom = r.top + iconrectheight;
		
		r.left = r.right - iconrectwidth;
		
	//	insetrect (&r, -4, 0); /*a little extra width for title*/
		
		if (hm != nil)
			(**hm).iconrect = r; 
		}
		
	/*do menurect*/ {
		
		menurect = rcontent;

		menurect.right -= iconrectwidth + windowmargin + scrollbarwidth;

		menurect.bottom -= msgheight + windowmargin;

		if (hm != nil)
			(**hm).menuoutlinerect = menurect;
		}
	
	/*do cmdkeypopuprect*/ {
		
		r = rcontent;
		
		r.top = r.bottom - msgheight;

		r.right = r.left + cmdkeypopupwidth;
		
		if (hm != nil)
			(**hm).cmdkeypopuprect = r; 
		}
	
	/*do messagerect*/ {
		
		r.left = r.right + popupbetweenwidth;
		
		r.right = menurect.right + scrollbarwidth;
		
		(**hw).messagerect = r; 
		}
		
	/*do vertscrollbar*/ {
		
		r = menurect; /*the space occupied by the grid of cells*/
		
		r.left = r.right; /*scrollbar is just to right of grid*/
		
		r.right = r.left + scrollbarwidth;
		
		setscrollbarrect ((**hw).vertscrollbar, r);
		
		//showscrollbar ((**hw).vertscrollbar);
		}
	
	return (true);
	} /*meresetwindowrects*/


void meresize (void) {
	
	Rect outlinerect;
	
	megetoutlinerect (&outlinerect); /*flow through code that adjusts the rect*/
	
	insetrect (&outlinerect, 1, 1);
	
	mesetalloutlinerects (outlinerect);
	} /*meresize*/


