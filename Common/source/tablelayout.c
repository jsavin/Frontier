
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

#include <standard.h>
#include "memory.h"
#include "quickdraw.h"
#include "scrollbar.h"
#include "tableinternal.h"
#include "windowlayout.h"



#define tabletopmargin 3


boolean tablesetextrainfo (hdlwindowinfo hw, hdltableformats hformats) {
	
	tyextrainfo info;
	register short scrollbarwidth;
	register hdlextrainfo hinfo;
	Rect rcontent;
	Rect r;
	short msgheight;
	Rect gridrect;
	
	rcontent = (**hw).contentrect;
	
	hinfo = (hdlextrainfo) (**hformats).hextrainfo;
	
	if (hinfo == nil) 
		clearbytes (&info, longsizeof (info));
	else
		moveleft (*hinfo, &info, longsizeof (info));
	
	/*set up some values for all rect computations*/ {
	
		scrollbarwidth = getscrollbarwidth ();
		
		/*
		pushstyle (msgfont, msgsize, msgstyle);
	
		msgheight = globalfontinfo.ascent + globalfontinfo.descent;
		
		msgheight += 2; /%leave a little space above and below text%/
		
		msgheight += msgheight / 2; /%approx 1.5 times the height%/
		
		popstyle ();
		*/
		
		msgheight = popupheight; 
		}
	
	/*do info.growiconrect*/ {
		
		shellcalcgrowiconrect (rcontent, hw);
		}
	
	/*do info.tablerect, reset fields of hformats*/ {
		
		r = rcontent;
		
		r.top += tabletopmargin; /*three pixels between titles and top of window*/
	
		r.bottom -= msgheight + windowmargin;
		
		r.left += windowmargin;
	
		r.right -= windowmargin + iconrectwidth + windowmargin + scrollbarwidth;
		
		tablepushformats (hformats); 
		
		tableresetrects (r); 
		
		tablepopformats ();
		
		info.tablerect = r;
		
		gridrect = (**hformats).tablerect; /*for sizing other objects*/
		}
		
	/*do info.kindpopuprect*/ {
		
		r.left = gridrect.left + 3; /*flush with left edge of icons*/
		
		r.right = r.left + popupwidth;
		
		r.top = gridrect.bottom + ((rcontent.bottom - gridrect.bottom - msgheight) / 2);
		
		r.bottom = r.top + msgheight;
		
		info.kindpopuprect = r; 
		}
		
	/*do info.sortpopuprect*/ {
		
		r = info.kindpopuprect;
		
		r.left = r.right + popupbetweenwidth;
		
		r.right = r.left + popupwidth;
		
		info.sortpopuprect = r;
		}
		
	/*do (**hw).messagerect*/ {
		
		r = info.sortpopuprect; /*inherits top and bottom*/
		
		r.left = r.right + popupbetweenwidth;
		
		r.right = gridrect.right + scrollbarwidth;
		
		(**hw).messagerect = r; 
		}
		
	/*do info.iconrect*/ {
	
		r.top = gridrect.top;
		
		r.bottom = r.top + iconrectheight;
		
		r.right = rcontent.right - windowmargin;
		
		r.left = r.right - iconrectwidth;
		
		insetrect (&r, -4, 0); /*a little extra width for title*/
		
		info.iconrect = r; 
		}
		
	/*do info.vertscrollbar*/ {
		
		r = gridrect; /*the space occupied by the grid of cells*/
		
		r.left = r.right; /*scrollbar is just to right of grid*/
		
		r.right = r.left + scrollbarwidth;
		
		r.bottom = r.top + (**hformats).nonblankvertpixels;
		
		setscrollbarrect ((**hw).vertscrollbar, r);
		
		//showscrollbar ((**hw).vertscrollbar);
		}
	
	if (hinfo == nil) { /*allocate a new handle*/
		
		hdlextrainfo hnewinfo;
		
		if (!newfilledhandle (&info, longsizeof (info), (Handle *) &hnewinfo))
			return (false);
			
		(**hformats).hextrainfo = (Handle) hnewinfo;
		
		return (true);
		}
	
	/*copy into the existing handle*/
	
	lockhandle ((Handle) hinfo);
	
	moveleft (&info, *hinfo, longsizeof (info));
	
	unlockhandle ((Handle) hinfo);	
	
	return (true);
	} /*tablesetextrainfo*/
	
	
boolean tablegetextrainfo (hdltableformats hformats, tyextrainfo *info) {
	
	register hdlextrainfo h;
	
	h = (hdlextrainfo) (**hformats).hextrainfo;
	
	moveleft (*h, info, longsizeof (tyextrainfo));
	
	return (true);
	} /*tablegetextrainfo*/


boolean tablegetcontentsize (long *width, long *height) {
	
	register short pixels;
	
	tablegettablesize (width, height);
	
	pixels = tabletopmargin + windowmargin + popupheight + 4;
	
	*height += pixels;
	
	pixels = windowmargin + windowmargin + iconrectwidth + windowmargin + getscrollbarwidth ();
	
	*width += pixels;
	
	return (true);
	} /*tablegetcontentsize*/




