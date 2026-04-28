
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

#include "memory.h"
#include "quickdraw.h"
#include "kb.h"
#include "textdisplay.h"
#include "shell.h"
#include "shellprint.h"
#include "op.h"
#include "opinternal.h"
#include "wpengine.h"




/*
static void opgetprintdisplayinfo (tytextdisplayinfo *info) {
	
	register hdloutlinerecord ho = op_get_outlinedata();
	
	gettextdisplayinfo (
		shellprintinfo.paperrect, (**ho).fontnum, (**ho).fontsize, (**ho).fontstyle, 
		
		(**ho).linespacing, (**ho).lineindent, info);
	} /%opgetprintdisplayinfo%/
*/

static short opgetpagecount (void) {
	
	/*
	5.0.2b20 dmb: we're called after beginprint, so we need to account 
	for scaling.

	6.0b4 dmb: now, beginprint doesn't resize, so we must not use scaling
	*/

	hdlheadrecord nomad = (**op_get_outlinedata()).hsummit, nextnomad;
	short vertpixels;
	short ctpages = 1;
	short sum = 0;
	short lh; 
	typrintinfo *lpi = &shellprintinfo;
	
	vertpixels = (lpi->paperrect.bottom - lpi->paperrect.top); //6.0b4, was: * lpi->scaleMult / lpi->scaleDiv;
	
	while (true) {
		
		lh = opgetlineheight (nomad);
		
		sum += lh;
		
		if (sum > vertpixels) {
			
			ctpages++;
			
			sum = lh;
			}
		
		nextnomad = opbumpflatdown (nomad, false);
		
		if (nextnomad == nomad)
			return (ctpages);
			
		nomad = nextnomad;
		} /*while*/
	} /*opgetpagecount*/
	
	
void opgetprintrect (Rect *r) {
	
	long scaleMult, scaleDiv;
	
	getprintscale (&scaleMult, &scaleDiv);
	
	(*r).top = (long) (shellprintinfo.paperrect.top * scaleMult) / scaleDiv;
	(*r).bottom = (long) (shellprintinfo.paperrect.bottom * scaleMult) / scaleDiv;
	(*r).left = (long) (shellprintinfo.paperrect.left * scaleMult) / scaleDiv;
	(*r).right = (long) (shellprintinfo.paperrect.right * scaleMult) / scaleDiv;
	} /*opgetprintrect*/


boolean opsetprintinfo (void) {
	
	shellprintinfo.ctpages = opgetpagecount ();
	
	return (true);
	} /*opsetprintinfo*/


boolean opbeginprint (void) {
	
	if (opisfatheadlines (op_get_outlinedata()))
		wpbeginprint ();
	
	/*
	Rect r;
	long scaleMult, scaleDiv;
	
	getprintscale (&scaleMult, &scaleDiv);
	
	r.top = (long) (shellprintinfo.paperrect.top * scaleMult) / scaleDiv;
	r.bottom = (long) (shellprintinfo.paperrect.bottom * scaleMult) / scaleDiv;
	r.left = (long) (shellprintinfo.paperrect.left * scaleMult) / scaleDiv;
	r.right = (long) (shellprintinfo.paperrect.right * scaleMult) / scaleDiv;
	
	opresize (r);
	*/

	return (true);
	} /*opbeginprint*/


boolean opendprint (void) {
	
	/*
	opresize ((**outlinewindowinfo).contentrect); 
	*/
	
	if (opisfatheadlines (op_get_outlinedata()))
		wpendprint ();
	
	oppostfontchange (); //cleanup measurements that couldn't be made while printing

	return (true);
	} /*opendprint*/


boolean opprint (short pagenumber) {
	
	/*
	6.0b3 dmb: added before/afterprintpage callbacks, opgetprintrect;
			   various display maintenance fixes
	*/

	register hdloutlinerecord ho = op_get_outlinedata();
	short vertpixels;
	short lnum = 0;
	short sum = 0;
	hdlheadrecord nomad;
	tyscrollinfo oldvertscrollinfo, oldhorizscrollinfo;
	hdlheadrecord oldline1;
	short oldline1linesabove;
	//typrintinfo *pi = &shellprintinfo;
	Rect printrect;
//	Rect oldoutlinerect;
	short olddefaultlineheight;
	boolean flmore;
	
	opgetprintrect (&printrect);

	//vertpixels = (pi->paperrect.bottom - pi->paperrect.top) * pi->scaleMult / pi->scaleDiv;
	vertpixels = printrect.bottom - printrect.top;

	if (pagenumber == 1) /*first page?*/
		nomad = (**ho).hsummit;
	else
		nomad = (**ho).hprintcursor;
		
//	oldoutlinerect = (**ho).outlinerect;
	
//	(**ho).outlinerect = shellprintinfo.paperrect;
	
	olddefaultlineheight = (**ho).defaultlineheight;
	
	oldline1 = (**ho).hline1;
	
	oldline1linesabove = (**ho).line1linesabove;
	
	oldhorizscrollinfo = (**ho).horizscrollinfo;
	
	oldvertscrollinfo = (**ho).vertscrollinfo;
	
	(**ho).horizscrollinfo.cur = 0;
	
	(**ho).flprinting = true; /*for display special-casing*/
	
	opresize (printrect);
	
	(*(**ho).beforeprintpagecallback) ();
	
 	oppushstyle (ho);
	
	(**ho).hline1 = nomad;
	
	(**ho).line1linesabove = 0;
 	
	while (true) {
 		
 		Rect r;
		
		opgetlinerect (lnum++, &r);
 			
 		sum += r.bottom - r.top;
 		
 		if (sum > vertpixels) { /*line doesn't fit on page*/
			
			flmore = true;

 			break;
			}
		
		opdrawline (nomad, r);
		
		if (!opnavigate (flatdown, &nomad)) {
			
			flmore = false;
			
 			break;
			}
 		} /*for*/
	
 	popstyle ();
	
	(**ho).hprintcursor = nomad;
	
	(**ho).flprinting = false;
	
	opresize ((**outlinewindowinfo).contentrect); 
	
	(*(**ho).afterprintpagecallback) ();
	
	(**ho).horizscrollinfo = oldhorizscrollinfo;
	
	(**ho).vertscrollinfo = oldvertscrollinfo;

//	(**ho).outlinerect = oldoutlinerect;
	
	(**ho).hline1 = oldline1;
	
	(**ho).line1linesabove = oldline1linesabove;
	
	(**ho).defaultlineheight = olddefaultlineheight;
	
	opdirtymeasurements ();
	
	opredrawscrollbars ();
	
	return (flmore);
	} /*opprint*/



