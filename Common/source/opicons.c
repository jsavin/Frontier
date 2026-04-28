
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
#include "icon.h"
#include "op.h" /*7.0b16 PBS*/
#include "opicons.h"
#include "opdisplay.h"
#include "strings.h" /*7.0b9 PBS*/
#include "tablestructure.h"

#define canexpandicon 475
#define cantexpandicon 476
#define canexpandcommenticon 477
#define cantexpandcommenticon 478
#define righticon 479
#define lefticon 480
#define downicon 481
#define upicon 482
#define breakpointicon 483
#define markedicon 484
#define	expandedicon 485
	#define refconicon 489 /*7.0b2 PBS: musical note icon has different id in Mac version.*/
#define customicon 490


short opgetheadicon (hdlheadrecord hnode) {
	
	register hdloutlinerecord ho = op_get_outlinedata();
	boolean flcanexpand;
	
	if ((**ho).flprinting) /*show all leaders as gray when printing*/
		flcanexpand = false;
	else
		flcanexpand = ophassubheads (hnode) && (!opsubheadsexpanded (hnode));
	
	if (opnestedincomment (hnode)) {
		
		if (flcanexpand)
			return (canexpandcommenticon);
		else
			return (cantexpandcommenticon);
		}
	
	if ((**hnode).flbreakpoint)
		return (breakpointicon);
	
	#if false	//ndef onlineOutliner
	
		if ((**hnode).flmarked)
			return (markedicon);
	
	#endif
		
	if (flcanexpand)
		return (canexpandicon);
	else
		return (cantexpandicon);
	} /*opgetheadicon*/
	
	
void opdrawheadicon (short iconnum, const Rect *r, boolean flselected) {
	/*
	 This function draws the triangle icons inside outlines (but not root tables).
	 Root table triangles are in tabledisplay.c, browserdrawnodeicon()
	 */
	
	operaserect (*r);

	short transform = kTransformNone;
	
	if (flselected)
		transform = kTransformSelected; 
	
	ploticonresource ((Rect *) r, kAlignAbsoluteCenter, transform, iconnum);

	} /*opdrawheadicon*/


boolean opdrawheadiconcustom (bigstring bsiconname, const Rect *r, boolean flselected) {
	
	short transform = 0;

	if (flselected)
		transform = kTransformSelected; 
	
	operaserect (*r);

	return (ploticoncustom (r, 0, transform, bsiconname));
	} /*opdrawheadiconcustom*/

boolean opdrawheadiconcustomfromodb (bigstring bsadricon, const Rect *r, boolean flselected) {
	
	short transform = 0;
	
	if (flselected)
		transform = kTransformSelected; 
	
	operaserect (*r);
	
	return (ploticonfromodb (r, 0, transform, bsadricon));
} /*opdrawheadiconcustom*/

boolean opgetnodetypetableadr(bigstring bsnodetype, bigstring bsadrnodepath) {
	//user.tools.nodeTypes.[type]
	//Frontier.tools.data.nodeTypes.[type]
	boolean fllookup = false;
	
	copystring(BIGSTRING("\x15" "user.tools.nodeTypes."), bsadrnodepath);
	pushstring(bsnodetype, bsadrnodepath);
	
	fllookup = resolveHashTable(bsnodetype, bsadrnodepath);
	
	if (!fllookup) {
		copystring(BIGSTRING("\x1E" "Frontier.tools.data.nodeTypes."), bsadrnodepath);
		pushstring(bsnodetype, bsadrnodepath);
		
		fllookup = resolveHashTable(bsnodetype, bsadrnodepath);
	}
	
	return fllookup;
}

boolean resolveHashTable(bigstring bsnodetype, bigstring bsadrnodepath) {
	bigstring bsname;
	hdlhashtable ht;
	hdlhashnode hn;
	tyvaluerecord iconvalue;
	boolean flexpanded = false;
	boolean fllookup = false;
	
	pushhashtable (roottable);
	
	disablelangerror ();
	
	flexpanded = langexpandtodotparams (bsadrnodepath, &ht, bsname);
	enablelangerror ();
	pophashtable ();
	
	fllookup = hashtablelookup (ht, bsnodetype, &iconvalue, &hn);

	if (fllookup && iconvalue.valuetype == addressvaluetype) {
		copystring((const UInt8 *)*iconvalue.data.stringvalue, bsadrnodepath);
		return resolveHashTable(bsnodetype, bsadrnodepath);
	}

	return fllookup;
}

boolean opdefaultdrawicon (hdlheadrecord hnode, const Rect *iconrect, boolean flselected, boolean flinverted) {
#pragma unused(flselected, flinverted)

	/*
	the default icon drawing routine, for the script editor and menu
	editor, not for clay basket.
	*/
	
	register hdloutlinerecord ho = op_get_outlinedata();
	short iconnum;
	bigstring bsheadlinetype; /*7.0b9 PBS*/
	boolean flcustomicondrawn = false;
	
	iconnum = opgetheadicon (hnode);
	
//#ifdef PIKE

	/*
	7.0b9 PBS: logic for drawing a custom icon.
	If the outline is an outline,
	and the headline has a refcon,
	and the refcon has a type attribute,
	and there's a file on disk [type.bmp],
	use that icon.
	*/

	if ((**ho).outlinetype == outlineisoutline) { /*is this an outline?*/

		if (ophasrefcon (hnode)) { /*does it have a refcon?*/
		
			setemptystring (bsheadlinetype);
			
			if (opattributesgettypestring (hnode, bsheadlinetype)) { /*is there a type att?*/

				/* Look up a custom icon in the odb and draw it if there is one. */
				
				
				bigstring bsadrnodepath;

				if (stringlength(bsheadlinetype) > 0 && opgetnodetypetableadr(bsheadlinetype, bsadrnodepath)) {
					pushstring(BIGSTRING("\x09" ".icon.mac"), bsadrnodepath);
					flcustomicondrawn = opdrawheadiconcustomfromodb (bsadrnodepath, iconrect, false);
				}
				
				
				#if (defined(MACVERSION) && defined(__ppc__)) || defined (WIN95VERSION)

				/*Draw a custom icon. If it returns false, there was no custom icon.*/
				if (!flcustomicondrawn) {
					flcustomicondrawn = opdrawheadiconcustom (bsheadlinetype, iconrect, false);
				}
				
				#endif
				} /*if*/
			} /*if*/
		} /*if*/

//#endif
//	opdrawheadicon (iconnum, iconrect, flselected);

	if (!flcustomicondrawn) /*Draw a normal icon only if there was no custom icon.*/

		opdrawheadicon (iconnum, iconrect, false);
	
	/*
	if (flselected) {
		
		Rect r = *iconrect;
		
		insetrect (&r, 1, 1);
		
		invertrect (r);
		}
	*/
	
	return (true);
	} /*opdefaultdrawicon*/


boolean opdefaultgeticonrect (hdlheadrecord hnode, const Rect *linerect, Rect *iconrect) {
	
	hdloutlinerecord ho = op_get_outlinedata();
	Rect r;
	Rect rcontains = *linerect;
	
	rcontains.bottom = rcontains.top + (**ho).iconheight;
	
	rcontains.left += opnodeindent (hnode);
		
	rcontains.right = rcontains.left + (**ho).iconwidth;
	
	r = rcontains;
	
	if (opisfatheadlines (ho))
		rcontains.bottom = rcontains.top + textvertinset + (**ho).defaultlineheight + textvertinset;
	else
		rcontains.bottom = (*linerect).bottom;
	
	centerrect (&r, rcontains); /*center it vertically within the linerect*/
	
	*iconrect = r;

	return (true);
	} /*opdefaultgeticonrect*/
	
	
void opgeticonrect (hdlheadrecord hnode, const Rect *linerect, Rect *iconrect) {

	(*(**op_get_outlinedata()).geticonrectcallback) (hnode, linerect, iconrect);
	} /*opgeticonrect*/
	
	
void opdrawarrowicon (hdlheadrecord hnode, long lnum, tydirection arrowdirection) {
	
	/*
	display an arrow as the icon for headline. used in dragging move. 
	
	send us "nodirection" if you want it restored to its normal icon.
	*/
	
	Rect linerect, iconrect;
	short iconnum = 0;
	boolean flinverted, flselected;
	
	if (lnum < 0) /*defensive driving*/
		return;
	
	pushbackcolor (&(**op_get_outlinedata()).backcolor);
	
	opgetlinerect (lnum, &linerect); 
	
	opgeticonrect (hnode, &linerect, &iconrect);
	
	opgetlineselected (hnode, &flinverted, &flselected);
	
	if (arrowdirection == nodirection) {
		
		operaserect (iconrect);
		
		(*(**op_get_outlinedata()).drawiconcallback) (hnode, &iconrect, flselected, flinverted);
			
		goto exit;
		}
	
	switch (arrowdirection) {
		
		case up:
			iconnum = upicon; break;
			
		case down:
			iconnum = downicon; break;
			
		case left:
			iconnum = lefticon; break;
			
		case right:
			iconnum = righticon; break;
		
		default:
			/* do nothing*/
			break;
			
		} /*switch*/
		
	opdrawheadicon (iconnum, &iconrect, flselected);

	exit:
	
		popbackcolor ();
	
	} /*opdrawarrowicon*/
	
	
	
	
	
	

