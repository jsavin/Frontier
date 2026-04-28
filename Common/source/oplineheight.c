
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

/*these routines support variable lineheight in opxxx.c*/

#include "frontier.h"
#include "standard.h"

#include "ops.h"
#include "opinternal.h"
#include "oplineheight.h"
#include "opdisplay.h"
#include "wpengine.h"



short opgetline1top (void) {
	
	/*
	return the number of pixels from top of outlinerect of hline1 as a negative number
	*/
	
	register hdloutlinerecord ho = op_get_outlinedata();
	short line1linesabove = (**ho).line1linesabove;
	
	if (line1linesabove == 0)
		return (0);
	else
		return (-textvertinset - ((**ho).defaultlineheight * line1linesabove));
	} /*opgetline1top*/


hdlheadrecord opgetlastvisiblenode (void) {
	
	hdloutlinerecord ho = op_get_outlinedata();
	hdlheadrecord nomad = (**ho).hline1;
	Rect r = (**ho).outlinerect;
	short vertpixels = r.bottom - r.top;
	hdlheadrecord hlastvisible;
	short ct = 0;
	
	ct += opgetline1top ();
	
	hlastvisible = nomad;
	
	while (true) {
		
		ct += opgetlineheight (nomad);
		
		if (ct > vertpixels)
			return (hlastvisible);
			
		hlastvisible = nomad;
		
		nomad = opgetnextexpanded (nomad);
		} /*while*/
	} /*opgetlastvisiblenode*/


int64_t opgetcurrentscreenlines (boolean flscrollwise) {

	/*
	return the number of lines currently showing in the window.

	6.0a12 dmb: added flscrollwise parameter. If true, return the number
	of scroll lines on the screen; otherwise return the number of headlines,
	the original meaning if this function
	*/

	hdloutlinerecord ho = op_get_outlinedata();
	hdlheadrecord nomad = (**ho).hline1, nextnomad;
	Rect r = (**ho).outlinerect;
	int64_t vertpixels = r.bottom - r.top;
	int64_t ctpixels = 0, ctlines = 0;
	
	ctpixels += opgetline1top ();
	
	if (flscrollwise)
		ctlines -= (**ho).line1linesabove;
	
	while (true) {
		
		ctpixels += opgetlineheight (nomad);
		
		if (ctpixels > vertpixels) {
		
			if (flscrollwise) { //need to count lines that do fit
			
				ctpixels -= opgetlineheight (nomad); //undo the add
				
				ctlines += (vertpixels - ctpixels - textvertinset) / (**ho).defaultlineheight;
				}
			
			return (ctlines);
			}
		
		if (flscrollwise)
			ctlines += opgetnodelinecount (nomad);
		else
			ctlines++;
		
		nextnomad = opgetnextexpanded (nomad);
		
		if (nextnomad == nomad) { //ran out of lines
		
			if (flscrollwise) // account for white space
				ctlines += (vertpixels - ctpixels) / ((**ho).defaultlineheight + textvertinset);
			
			return (ctlines);
			}
			
		nomad = nextnomad;
		} /*while*/
	} /*opgetcurrentscreenlines*/
	
	
int64_t opsumprevlineheights (int64_t lnum, short *heightthisline) {

	/*
	return the sum of the lineheights of all lines above this one.

	lnum is 0-based.

	6.0b2 dmb: work with negative lnums, with the same semantics (return
	a negative number)
	*/

	hdloutlinerecord ho = op_get_outlinedata();
	hdlheadrecord nomad = (**ho).hline1;
	int64_t sum = 0;
	int64_t i;
	
	sum += opgetline1top ();
	
	if (lnum < 0) {
	
		for (i = 0; i > lnum; i--) { // at least once
			
			nomad = opbumpflatup (nomad, true);
			
			*heightthisline = opgetlineheight (nomad);
			
			sum -= *heightthisline;
			}
		}
	else {
	
		for (i = 0; i < lnum; i++) {
			
			sum += opgetlineheight (nomad);
			
			nomad = opbumpflatdown (nomad, !(**ho).flprinting); //6.0b4 dmb: can't opgetnextexpanded (nomad) when printing
			} /*while*/
		
		*heightthisline = opgetlineheight (nomad);
		}
	
	return (sum);
	} /*opsumprevlineheights*/
	
	
int64_t opsumalllineheights (void) {

	hdlheadrecord nomad = (**op_get_outlinedata()).hsummit, nextnomad;
	int64_t sum = 0;
	
	while (true) {
		
		sum += opgetlineheight (nomad);
		
		nextnomad = opgetnextexpanded (nomad);
		
		if (nextnomad == nomad)
			return (sum);
			
		nomad = nextnomad;
		} /*while*/
	} /*opsumalllineheights*/
	
	
int64_t opgetlinestoscrollupforvisi (hdlheadrecord hnode) {

	/*
	return the number of lines you have to scroll to make the node
	visible. assume the node lies off the bottom of the window.

	first we figure out how many pixels off we are (ctneeded).

	then we loop up from the first line until we equal or exceed
	that number.
	*/

	hdloutlinerecord ho = op_get_outlinedata();
	hdlheadrecord nomad = (**ho).hline1;
	Rect r = (**ho).outlinerect;
	int64_t vertpixels = r.bottom - r.top;
	int64_t ctpixels = 0;
	int64_t ctneeded = 0;
	int64_t ctlines;
	int64_t ctscroll;
	short lh;	
	
	ctpixels = opgetline1top (); //measure  from top of line1
	
	while (true) {
		
		lh = opgetlineheight (nomad);
		
		//if ((ctpixels + lh) > vertpixels) /*line doesn't fit, or completely fit*/
		//	ctneeded += lh;
			
		ctpixels += lh;
	
		if (nomad == hnode)
			break;
		
		nomad = opgetnextexpanded (nomad);
		} /*while*/
	
	ctneeded = ctpixels - vertpixels;
	
	if (ctneeded <= 0) /*it's already visible*/
		return (0);
		
	nomad = (**ho).hline1;
	
	ctpixels = opgetline1top ();
	
	ctscroll = -(**ho).line1linesabove;
	
	while (true) {
		
		lh = opgetlineheight (nomad);
		
		ctlines = opgetnodelinecount (nomad);
		
		ctpixels += lh;
		
		ctscroll += ctlines;
		
		if (ctpixels >= ctneeded) {
		
			if (ctpixels > ctneeded && ctlines > 1)
				ctscroll -= (ctpixels - ctneeded) / (**ho).defaultlineheight;
			
			return (ctscroll);
			}
			
		nomad = opgetnextexpanded (nomad);
		}
	} /*opgetlinestoscrollupforvisi*/


int64_t opgetlinestoscrolldownforvisi (hdlheadrecord hnode) {

	/*
	return the number of lines you have to scroll to make the node
	visible. assume the node lies above the top of the window.

	we count the number of scroll lines above hline1, up to and including
	hnode

	6.0b2 dmb: account for text selection in headlines taller than the screen
	*/

	hdloutlinerecord ho = op_get_outlinedata();
	hdlheadrecord nomad = (**ho).hline1;
	hdlheadrecord hsummit = (**ho).hsummit;
	int64_t defaultlineheight = (**ho).defaultlineheight;
	int64_t ctscroll = 0;
	int64_t ctpixels = 0;
	Point ptsel;
	Rect r;
	
	if (!(nomad == hnode && opeditingtext (hnode))) {
		
		ctscroll = (**ho).line1linesabove; //first, bring hline1 to top of window
		
		ctpixels = -opgetline1top ();
		}
	
	while (true) {
		
		if (nomad == hsummit) //ran out, didn't find it
			break;
		
		if (nomad == hnode)
			break;
		
		nomad = opbumpflatup (nomad, true);
		
		ctscroll += opgetnodelinecount (nomad);
		
		ctpixels += opgetlineheight (nomad);
		}
	
	if (opeditingtext (hnode)) { // see if hnode has to scroll above the top of the window
		
		opeditgetselpoint (&ptsel);
		
		ptsel.v += ctpixels;
		
		r = (**ho).outlinerect;
		
		if (ptsel.v < r.top)
			ctscroll += divup (r.top - ptsel.v, defaultlineheight);
		
		else {
			
			ptsel.v += defaultlineheight;
			
			if (ptsel.v > r.bottom)
				ctscroll -= divup (ptsel.v - r.bottom, defaultlineheight);
			}
		}
	
	return (ctscroll);
	} /*opgetlinestoscrolldownforvisi*/
