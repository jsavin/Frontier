
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

#include <standard.h>
#include "font.h"
#include "ops.h"
#include "quickdraw.h"
#include "shellprint.h"
#include "tableinternal.h"



static short tablerowsperpage (void) {
	
	register hdltableformats hf = tableformatsdata;
	register short vertsave = (**hf).vertcurrent;
	short firstrow, lastrow;
	short firstcol, lastcol;
	
	(**hf).vertcurrent = 0;
	
	tablefirstpartiallyvisible (&firstrow, &firstcol);
	
	tablelastpartiallyvisible (&lastrow, &lastcol);
	
	(**hf).vertcurrent = vertsave;
	
	return (lastrow - firstrow + 1);
	} /*tablerowsperpage*/


boolean tablesetprintinfo (void) {
	
	/*
	9/24/91 dmb: handle multiple pages
	*/
	
	register hdltableformats hf = tableformatsdata;
	Rect wholerect = (**hf).wholerect;
	
	shellprintinfo.ctpages = 1; /*pretty dumb for now*/
	
	if (!tableempty ()) {
		
		tableresetrects (shellprintinfo.paperrect);
		
		shellprintinfo.ctpages = divup ((**hf).ctrows, tablerowsperpage ());
		
		tableresetrects (wholerect);
		}
	
	return (true);
	} /*tablesetprintinfo*/


boolean tableprint (short pagenumber) {
	
	/*
	9/25/91 dmb: handle multi-page tables.  also, use new tablegettablesize to 
	more intelligently set the right margin.
	*/
	
	register hdltableformats hf = tableformatsdata;
	Rect printrect = shellprintinfo.paperrect;
	Rect wholerect = (**hf).wholerect;
	long tablewidth, tableheight;
	
	tablegettablesize (&tablewidth, &tableheight); /*get width that will show all text*/
	
	tablewidth = max (tablewidth, wholerect.right - wholerect.left); /*don't make smaller than display*/
	
	printrect.right = min (printrect.right, printrect.left + tablewidth); /*maybe bring right edge in*/
	
	tableresetrects (printrect);
	
	tablerecalccolwidths (false); /*re-distribute colwidths*/
	
	(**hf).flprinting = true;
	
	tableupdatecoltitles (printrect);
	
	if (!tableempty ()) {
		
		register RgnHandle printrgn = NewRgn ();
		register short horizsave = (**hf).horizcurrent;
		register short vertsave = (**hf).vertcurrent;
		
		RectRgn (printrgn, &printrect);
		
		(**tableformatswindowinfo).drawrgn = printrgn;
		
		(**hf).horizcurrent = 0;
		
		(**hf).vertcurrent = (pagenumber - 1) * tablerowsperpage ();
		
		tableupdateseparator (printrect);
		
		tableupdategridlines (printrect);
		
		tableupdatecells (printrect);
		
		(**hf).horizcurrent = horizsave;
		
		(**hf).vertcurrent = vertsave;
		
		DisposeRgn (printrgn);
		
		(**tableformatswindowinfo).drawrgn = nil;
		}
	
	(**hf).flprinting = false;
	
	tableresetrects (wholerect);
	
	tablerecalccolwidths (false); /*re-distribute colwidths*/
	
	return (true);
	} /*tableprint*/


