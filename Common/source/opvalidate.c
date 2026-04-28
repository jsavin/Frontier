
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

#include "strings.h"
#include "shell.h"
#include "op.h"
#include "opinternal.h"



#define invalid(h) {getheadstring (h,bs);shellinternalerror(idinvalidoutline, bs);return(false);}


static boolean opvalidtree (hdlheadrecord hnode, long *ptrexpansioncount) {
	
	bigstring bs;
	hdlheadrecord h, hdown, hparent;
	
	if ((**hnode).flexpanded)
		*ptrexpansioncount += opgetnodelinecount (hnode);
	
	hparent = hnode;
	
	h = (**hnode).headlinkright;
	
	if (h == hnode) 
		return (true);
	
	if ((**h).headlinkup != h) /*up pointer doesn't point at himself*/
		invalid (h);
		
	if ((**h).appbit4) /*there should be a callback here doing this*/
		invalid (h);
		
	while (true) {
		
		if ((**h).headlinkleft != hparent) /*we don't point at the right parent*/
			invalid (h);
			
		if (!opvalidtree (h, ptrexpansioncount)) /*recurse*/
			return (false);
			
		hdown = (**h).headlinkdown;
		
		if (hdown == h) /*cool, he's the last guy at the level*/
			return (true);
		
		if ((**hdown).headlinkup != h) /*guy down from us doesn't point up back at us*/
			invalid (h);
			
		h = hdown; /*advance to next guy*/
		} /*while*/
	} /*opvalidtree*/
	
	
boolean opvalidate (hdloutlinerecord houtline) {
	
	/*
	bullshit detector -- if there's anything wrong with the given outline
	structure, ie any redundant information doesn't agree, we break into the
	debugger with an appropriate message.
	*/
	
	bigstring bs;
	hdlheadrecord hsummit, h, hdown;
	hdloutlinerecord ho;
	boolean fl;
	long ctexpanded;
	
	ho = houtline;
	
	if (ho == nil)
		return (true);
	
	hsummit = (**ho).hsummit;
	
	h = hsummit;
		
	if ((**h).headlinkup != h) /*first guy doesn't point up at himself*/
		invalid (h);
		
	while (true) { /*check all level 0 guys for proper structure*/
	
		if ((**h).headlevel != 0) /*a level-0 guy who isn't level 0*/
			invalid (h);
			
		if ((**h).headlinkleft != h) /*a level-0 guy that doesn't point left at himself*/
			invalid (h);
			
		hdown = (**h).headlinkdown; /*advance to next guy*/
		
		if (hdown == h) /*reached the last guy*/
			goto L1;
		
		if ((**hdown).headlinkup != h) /*guy down from h doesn't point up at h*/
			invalid (h);
			
		h = hdown;
		} /*while*/
	
	L1: /*passed the level-0 test*/
	
	h = hsummit;
	
	ctexpanded = 0;
	
	while (true) {
		
		oppushoutline (ho);
		
		fl = opvalidtree (h, &ctexpanded);
		
		oppopoutline ();
		
		if (!fl)
			return (false);
			
		if (!opchasedown (&h))
			goto L2;
		} /*while*/
		
	L2: /*passed the level-1 test*/
	
	if (ctexpanded != (**ho).ctexpanded) {
	
		shellinternalerror(idinvalidoutline, BIGSTRING ("\x15" "expansion count error"));
		
		(**ho).ctexpanded = ctexpanded; /*once is enough for this message*/

		return (false); /* 6.2b10 AR */
		}
		
	return (true);
	} /*opvalidate*/
	
	
