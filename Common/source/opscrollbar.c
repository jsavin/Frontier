
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

#include "ops.h"
#include "shellprint.h"
#include "op.h"
#include "opinternal.h"
#include "oplineheight.h"





boolean oprestorescrollposition (void) {
	
	register hdloutlinerecord ho = op_get_outlinedata();
	hdlheadrecord hline1 = (**ho).hsummit;
	long ctscrolllines = (**ho).vertscrollinfo.cur;
	
	while (ctscrolllines > 0) {
	
		ctscrolllines -= opgetnodelinecount (hline1);
		
		if (ctscrolllines < 0) { //this is the top line, partially above the window
			
			ctscrolllines += opgetnodelinecount (hline1);
			
			break;
			}
		
		hline1 = oprepeatedbump (flatdown, 1, hline1, true); //if ctscrolllines is zero, this is line1
		}
	
	(**ho).hline1 = hline1;
	
	(**ho).line1linesabove = ctscrolllines;
	
	return (true);
	} /*oprestorescrollposition*/


boolean opsetscrollpositiontoline1 (void) {
	
	/*
	6.0b3 dmb: make the vertical scroll position agree with hline1/line1linesabove
	*/
	
	register hdloutlinerecord ho = op_get_outlinedata();
	hdlheadrecord nomad = (**ho).hsummit;
	hdlheadrecord hline1 = (**ho).hline1;
	long ctscrolllines = (**ho).line1linesabove;
	
	if (!(**hline1).flexpanded) //6.0b4 dmb: should never be true, but when printing..
		return (false);
	
	while (nomad != hline1) {
		
		ctscrolllines += opgetnodelinecount (nomad);
		
		nomad = opgetnextexpanded (nomad);
		}
	
	(**ho).vertscrollinfo.cur = ctscrolllines;
	
	return (true);
	} /*opsetscrollpositiontoline1*/


boolean opgetscrollbarinfo (boolean flpin) {
	
	/*
	9/11/91 dmb: added flpin parameter.  if true, limit the vertical 
	scroll position to the calculated vert max, instead of enlarging 
	vert to maintain the current position.  also, when not pinning, 
	make sure the vertinfo.cur isn't as large as ctexpanded
	
	9/22/92 dmb: horizinfo.max must be in scroll quanta, not pixels
	
	8/21/93 DW: add flvertscrolldisabled and flhorizscrolldisabled to 
	allow clay basket to have windows that don't enable their
	horizontal scrollbars.

	7.0b17 PBS: Don't subtract one from current screen lines --
	fixes an off-by-one bug.
	*/
	
	register hdloutlinerecord ho = op_get_outlinedata();
	//register hdlheadrecord hline1;
	long ctexpanded;
	short vscrollquantum;
	tyscrollinfo vertinfo, horizinfo;
	Rect r;
	
	if (ho == NULL)
		return (false);

	ctexpanded = (**ho).ctexpanded;

	if ((**ho).flvertscrolldisabled) 	
		vertinfo.min = vertinfo.cur = vertinfo.max = vertinfo.pag = 0;
	else {
		vertinfo.min = 0;
		
		vertinfo.cur = (**ho).vertscrollinfo.cur;
		
		vertinfo.cur = min (vertinfo.cur, ctexpanded - 1);
		
		r = (**ho).outlinerect;
		
		vscrollquantum = (**ho).defaultlineheight;
		
		vertinfo.pag = opgetcurrentscreenlines (true);// - 1; /*7.0b17 PBS: Don't subtract one. Off-by-one fix.*/

		vertinfo.max = max (vertinfo.min, ctexpanded - vertinfo.pag); /*DW 7/9/93*/
		
		if (flpin)
			vertinfo.cur = min (vertinfo.cur, vertinfo.max); /*bring it in range*/
		else
			vertinfo.max = max (vertinfo.max, vertinfo.cur); /*make it accomodate current pos*/
		
		/*
		hline1 = oprepeatedbump (flatdown, vertinfo.cur, (**ho).hsummit, true);
		
		if (hline1 != (**ho).hline1) {
		
			(**ho).hline1 = hline1;
			
			opinvaldisplay (); /%serious updating needed
			}
		*/
		}
	
	if ((**ho).flhorizscrolldisabled) 	
		horizinfo.min = horizinfo.cur = horizinfo.max = horizinfo.pag = 0;
	else {
		horizinfo.min = 0;
		
		horizinfo.cur = (**ho).horizscrollinfo.cur;
		
		r = shellprintinfo.paperrect; /*(**ho).outlinerect*/
		
		horizinfo.pag = (r.right - r.left);
		
		horizinfo.max = 3 * horizinfo.pag; /*three paper-widths*/
		
		horizinfo.cur = max (horizinfo.min, horizinfo.cur);
		
		horizinfo.cur = min (horizinfo.max, horizinfo.cur);
		}
	
	(**ho).vertscrollinfo = vertinfo;
	
	(**ho).horizscrollinfo = horizinfo;
	
	return (true);
	} /*opgetscrollbarinfo*/


void opredrawscrollbars (void) {
	
	(*(**op_get_outlinedata()).setscrollbarsroutine) ();
	} /*opredrawscrollbars*/


void opresetscrollbars (void) {
	
	opgetscrollbarinfo (false);
	
	if (opdisplayenabled ())
		opredrawscrollbars ();
	} /*opresetscrollbars*/
	

