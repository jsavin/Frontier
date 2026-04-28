
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
#include "mouse.h"
#include "op.h"
#include "op_context.h"
#include "opinternal.h"
#include "oplineheight.h"
#include "opdisplay.h"
#include "opicons.h"

#include "tablestructure.h" //7.0d5 AR


static boolean flnothingcollapsed;

static long pixelscollapsed;

static long pixelsexpanded;

static long pixelsalreadyexpanded;

static long ctalreadyexpanded;



static boolean opcollapsevisit (hdlheadrecord hnode, ptrvoid refcon) {
#pragma unused (refcon)

	register hdlheadrecord h = hnode;
	
	if ((**h).flexpanded) {
		
		flnothingcollapsed = false;	
		
		(**op_get_outlinedata()).ctexpanded -= opgetnodelinecount (h);
		
		(**h).flexpanded = false;
		
		pixelscollapsed += opgetlineheight (h);
		}
	
	return (true);
	} /*opcollapsevisit*/
	

void opfastcollapse (hdlheadrecord h) {
	
	/*
	clear the expanded bits of all subordinate material, don't do anything
	to the display.
	*/

	oprecursivelyvisit (h, infinity, &opcollapsevisit, nil); 
	} /*opfastcollapse*/
	

/**
 * opcollapse_ctx - Collapse outline node (context-aware)
 *
 * @param ctx - Operation context (required)
 * @param hnode - Node to collapse
 * @return true if something collapsed
 */
boolean opcollapse_ctx (op_context_t *ctx, hdlheadrecord hnode) {

	assert(ctx != NULL);
	op_context_version_bump(ctx);

	register hdloutlinerecord ho = op_get_outlinedata();
	int64_t origct = (**ho).ctexpanded;
	int64_t ctscroll;
	int64_t lnum;
	Rect linerect;

	pixelscollapsed = 0;

	flnothingcollapsed = true;

	oprecursivelyvisit (hnode, infinity, &opcollapsevisit, nil); /*clear expanded bits*/

	if (flnothingcollapsed)
		return (false);

	opdirtyview ();

	if (!opdisplayenabled ())
		goto exit;

	opgetscreenline (hnode, &lnum);

	opgetlinerect (lnum, &linerect);

//	opgeticonrect (hnode, &linerect, &iconrect);

//	invalrect (iconrect); /*the icon changes, avoid flash, don't inval the whole line*/

	opdrawicon (hnode, linerect);

	if (opgetnextexpanded (hnode) == hnode) { /*last expanded node, nothing to scroll up*/

		Rect r = linerect;

		r.top = linerect.bottom;

		r.bottom = (**ho).outlinerect.bottom;

		smashrect (r);

		goto exit;
		}

	if (linerect.bottom >= (**ho).outlinerect.bottom)
		goto exit;

	ctscroll = origct - (**ho).ctexpanded;

	if (ctscroll > 0) {

		Rect r = (**ho).outlinerect;

		if (linerect.bottom > r.top) { /*headline isn't above display*/

			r.top = linerect.bottom;

			opscrollrect (r, 0, -pixelscollapsed);
			}
		}

	exit:

	opresetscrollbars (); /*number of expanded lines changed*/

	(*(**ho).postcollapsecallback) (hnode);

	opupdatenow (); /*fill in newly revealed stuff immediately*/

	return (true); /*something was collapsed*/
	} /*opcollapse_ctx*/


/**
 * opcollapse - Collapse outline node (backward-compatible wrapper)
 *
 * Wrapper for code that doesn't use operation context yet.
 * Allocates temporary context internally.
 */
boolean opcollapse (hdlheadrecord hnode) {

	op_context_t *ctx = op_context_acquire(OP_CONTEXT_NORMAL);
	boolean result = opcollapse_ctx(ctx, hnode);
	op_context_release(ctx);
	return result;
	} /*opcollapse*/


static boolean opexpandvisit (hdlheadrecord hnode, ptrvoid refcon) {
#pragma unused (refcon)

	short lh = opgetlineheight (hnode);
	
	if ((**hnode).flexpanded) {
	
		++ctalreadyexpanded;
		
		pixelsalreadyexpanded += lh;
		}
	else {
		(**op_get_outlinedata()).ctexpanded += opgetnodelinecount (hnode);
		
		(**hnode).flexpanded = true;
		
		(**hnode).fldirty = true;
		
		pixelsexpanded += lh;
		}
	
	return (true);
	} /*opexpandvisit*/
	

/**
 * opexpand_ctx - Expand/collapse outline node (context-aware)
 *
 * @param ctx - Operation context (required)
 * @param hnode - Node to expand
 * @param level - Expansion level
 * @param flmaycreatesubs - Whether to create subnodes
 * @return true if something expanded
 */
boolean opexpand_ctx (op_context_t *ctx, hdlheadrecord hnode, short level, boolean flmaycreatesubs) {

	assert(ctx != NULL);
	op_context_version_bump(ctx);

	register hdloutlinerecord ho = op_get_outlinedata();
	Rect outlinerect = (**ho).outlinerect;
	int64_t origct = (**ho).ctexpanded;
	int64_t lnum;
	Rect linerect, r;
	int64_t hscroll, vscroll;

	if (!(*(**ho).preexpandcallback) (hnode, level, flmaycreatesubs))
		return (false);

	ctalreadyexpanded = 0;

	pixelsexpanded = 0;

	pixelsalreadyexpanded = 0;

	oprecursivelyvisit (hnode, level, &opexpandvisit, nil);

	origct = (**ho).ctexpanded - origct;

	if (origct == 0) /*nothing expanded*/
		return (false);

	opdirtyview ();

	if (!opdisplayenabled ())
		return (true);

	opgetscreenline (hnode, &lnum);

	opgetlinerect (lnum, &linerect);

	/*handle case where height of the head + subs is > height of win*/ {

		long heightheads = pixelsexpanded + pixelsalreadyexpanded + opgetlineheight (hnode);
		long heightwin = outlinerect.bottom - outlinerect.top;

		if (heightheads > heightwin) { /*smash the display, no optimization possible*/

			if (hnode != (**ho).hline1) {

				opsetline1 (hnode);

				opsetscrollpositiontoline1 ();
				}

			/*
			opgetscrollbarinfo (false);

			(**ho).vertscrollinfo.cur += lnum; //text scrolls up
			*/

			opresetscrollbars ();

			opseteditbufferrect (); //in case we're in text mode

			opinvaldisplay ();

			//operaserect ((**ho).outlinerect);

			opupdatenow ();

			return (true);
			}
		}

	//else
	//	opinvalnode (hnode);

	opresetscrollbars ();

	if (opneedvisiscroll (oplastexpanded (hnode), &hscroll, &vscroll, false)) {

		(**ho).blockvisiupdate = true;

		opdovisiscroll (hscroll, vscroll);

		(**ho).blockvisiupdate = false;

		//opinvalafter ((**hnode).headlinkright);
	 	opinvalafter (hnode);
	 	}
	else {
		r = linerect; /*get ready to scroll to make room for newly visible text*/

		r.top = r.bottom;

		if (ctalreadyexpanded > 0) { /*account for already-expanded lines*/

			r.bottom = r.top + pixelsalreadyexpanded;

			invalrect (r);

			r.top = r.bottom;
			}

		r.bottom = outlinerect.bottom;

		opscrollrect (r, 0, pixelsexpanded); /*scroll text down to make room for new heads*/

		opinvalnode (hnode);
		}

	opupdatenow ();

	return (true);
	} /*opexpand_ctx*/


/**
 * opexpand - Expand/collapse outline node (backward-compatible wrapper)
 *
 * Wrapper for code that doesn't use operation context yet.
 * Allocates temporary context internally.
 */
boolean opexpand (hdlheadrecord hnode, short level, boolean flmaycreatesubs) {

	op_context_t *ctx = op_context_acquire(OP_CONTEXT_NORMAL);
	boolean result = opexpand_ctx(ctx, hnode, level, flmaycreatesubs);
	op_context_release(ctx);
	return result;
	} /*opexpand*/
	

static void oprecursivelyexpandto (hdlheadrecord hnode) {
	
	/*
	work your way left until you find a node which is expanded.  expand it.
   
	then retrace your steps, expanding as you go.
	*/
	
	hdlheadrecord hparent = (**hnode).headlinkleft;
	
	if (hparent == hnode) /*at a summit, all summits are expanded*/
		return;
	
	if ((**hnode).flexpanded) 
		return;
		
	oprecursivelyexpandto (hparent); /*recurse*/
	
	oprecursivelyvisit (hparent, 1, &opexpandvisit, nil); /*set expanded bits on sibs*/
	} /*oprecursivelyexpandto*/


void opexpandto (hdlheadrecord hnode) {
	
	/*
	make sure that the indicated node is expanded. also moves 
	the cursor to the node.
	
	8/10/92 dmb: pophoists as necessary to make the node accessable
	*/
	
	register hdloutlinerecord ho = op_get_outlinedata();
	
	if ((**ho).hbarcursor == hnode) /*cursor is already on the node*/
		return;
	
	while (((**ho).tophoist > 0) && !opcontainsnode ((**ho).hsummit, hnode))
		oppophoist ();
	
	if ((**hnode).flexpanded) { /*just move the cursor to it and return*/
		
		opmoveto (hnode);
	
		opdirtyview ();
		
		return;
		}
	
	/*the node we're looking for isn't expanded*/
	
	oprecursivelyexpandto (hnode);
	
	opsetscrollpositiontoline1 ();
	
	opresetscrollbars ();
	
	opmoveto (hnode);
	
	opsmashdisplay ();
	
	opdirtyview ();
	} /*opexpandto*/


void opexpandtoggle (void) {
	
	/*
	if expanded, collapse.  if not expanded, expand.
	
	11/4/92 dmb: always try one or the other, in case subheads appear dynamically
	
	7.0d5 AR: Run op callback scripts 
	*/
	
	register hdlheadrecord hcursor = (**op_get_outlinedata()).hbarcursor;
	register hdlheadrecord hright = (**hcursor).headlinkright;
	
	if ((hright != hcursor) && (**hright).flexpanded) { /*first subhead is showing, collapse is called for*/

		if (langopruncallbackscripts (idopcollapsescript))
			return;  /*fuction consumed the click*/

		opcollapse (hcursor);
		}		
	else {

		if (langopruncallbackscripts (idopexpandscript))
			return;  /*fuction consumed the click*/

		opexpand (hcursor, 1, true);
		}
	} /*opexpandtoggle*/


void opexpandupdate (hdlheadrecord hnewnode) {

	/*
	5.0b6 dmb: allow for partially-visible lines
	*/

	register hdloutlinerecord ho = op_get_outlinedata();
	int64_t ctlines;
	int64_t lnum;
	
	(**hnewnode).flexpanded = true; 
	
	ctlines = opgetnodelinecount (hnewnode);
	
	(**ho).ctexpanded += ctlines;
	
	if (opdisplayenabled ()) {
		
		opresetscrollbars ();
		
		opgetscreenline (hnewnode, &lnum);
		
		if (lnum < 0)
			(**ho).vertscrollinfo.cur += ctlines;
		
		else if (lnum <= opgetcurrentscreenlines (false)) {/*node is visible*/
		
			opmakegap (lnum - 1, opgetlineheight (hnewnode));
			
			opupdatenow ();
			}
		}
	} /*opexpandupdate*/


void opcollapseall (void) {
	
	hdlheadrecord nomad = (**op_get_outlinedata()).hsummit;
	boolean fldisplaywasenabled;
	
	fldisplaywasenabled = opdisabledisplay ();
	
	while (true) {
		
		opjumpto (nomad);
	
		if (opsubheadsexpanded (nomad)) 
			opcollapse (nomad);
			
		if (!opchasedown (&nomad)) {
			
			opjumpto ((**op_get_outlinedata()).hsummit);
			
			if (fldisplaywasenabled) {
				
				openabledisplay ();
				
				opinvaldisplay ();
				}
			
			return;
			}
		} /*while*/
	} /*opcollapseall*/
	
	
boolean opsetlongcursor (long cursor) { 

	hdlheadrecord hcursor = (hdlheadrecord) cursor;

	if (!opnodeinoutline (hcursor)) /*it's been deleted, or something*/
		return (false);
	
	opexpandto (hcursor);
	
	return (true);
	} /*opsetlongcursor*/
	




