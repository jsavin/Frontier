
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
#include "memory.h"
#include "mouse.h"
#include "ops.h"
#include "dialogs.h"
#include "quickdraw.h"
#include "smallicon.h"
#include "frontierwindows.h"



static hdlsmalliconbits hmiscbits;
//static hdlsmalliconbits hleaderbits;





boolean plotsmallicon (tysmalliconspec spec) {

	register hdlsmalliconbits hbits;
	short mode;
	BitMap iconbitmap;
	
	hbits = spec.hbits;
	
	if (hbits == nil) {
	
		switch (spec.iconlist) {
			
			case miscsmalliconlist:
				hbits = hmiscbits;
				
				break;
			
			/*
			case leadersmalliconlist:
				hbits = hleaderbits;
				
				break;
			
			case draggingsmalliconlist:
				hbits = hdraggingbits;
				
				break;
				
			case tablesmalliconlist:
				hbits = htablebits;
				
				break;
			
			case circlesmalliconlist:
				hbits = hcirclebits;
				
				break;
			
			case findersmalliconlist:
				hbits = hfinderbits;
				
				break;
			*/
			
			} /*switch*/
		} /*nil bits*/
		
	if (hbits == nil)
		return (false);
		
	iconbitmap.baseAddr = (Ptr) &(*hbits) [spec.iconnum];
	
	iconbitmap.rowBytes = 2;
	
	iconbitmap.bounds.top = iconbitmap.bounds.left = 0; 
	
	iconbitmap.bounds.bottom = spec.iconrect.bottom - spec.iconrect.top; 
	
	iconbitmap.bounds.right = spec.iconrect.right - spec.iconrect.left;
	
	if (spec.flinverted)
		mode = notSrcCopy;
		
	else {
		if (spec.flclearwhatsthere)
			mode = srcCopy;
		else
			mode = srcOr;
		}
	//Code change by Timothy Paustian Friday, May 5, 2000 10:26:38 PM
	//Changed to Opaque call for Carbon
	#if ACCESSOR_CALLS_ARE_FUNCTIONS == 1
	{//added the bracket to save on code spread
	CGrafPtr windPort = GetWindowPort(spec.iconwindow);
	CopyBits (
		&iconbitmap, GetPortBitMapForCopyBits(windPort), 
		
		&iconbitmap.bounds, &spec.iconrect, mode, nil);
	}
	#else
	CopyBits (
		&iconbitmap, &(*spec.iconwindow).portBits, 
		
		&iconbitmap.bounds, &spec.iconrect, mode, nil);
	#endif
	
	return (true);

	} /*plotsmallicon*/
	
	
boolean displaypopupicon (Rect r, boolean flenabled) {
	
	
		short themestate = kThemeStateActive;
		
		if (!flenabled)
			themestate = kThemeStateUnavailable;
			
		/*It's always too high.*/
			
		r.top = r.top + 4;
		
		r.bottom = r.bottom + 4;
	
		DrawThemePopupArrow (&r, kThemeArrowDown, kThemeArrow9pt, themestate, NULL, 0);

		return (true);
	
	} /*displaypopupicon*/
	
	
boolean loadsmallicon (short resnum, hdlsmalliconbits *hbits) {
	
	Handle h;
	boolean fl;
	SInt8 hState;
	
	h = GetResource ('SICN', resnum);
	
	LoadResource(h); /*in case resource was purged*/
	hState = HGetState(h);
	HNoPurge(h); /*in case resource is purgeable*/
	fl = copyhandle (h, (Handle *) hbits);
	HSetState(h, hState);
	
	return (fl);

	} /*loadsmallicon*/


boolean myMoof (short ticksbetweenframes, long howlong) {
	
	/*
	Meet myMoof, our tip o the hat to Mac DTS. 
	
	Lest anyone think that UserLand Software doesn't REALLY understand multimedia!
	
	3/17/93 dmb: added howlong parameter so user interaction isn't required
	*/
	
	WindowPtr w;
	tysmalliconspec sicn; 	
	short i;
	Rect rport;
	long tc;
	CGrafPtr	thePort;
	
	tc = gettickcount ();
	
	if (!loadsmallicon (moofsmalliconlist, &sicn.hbits))
		goto error;
	
	setrect (&rport, 0, 0, 32, 32);

	centerrectondesktop (&rport);
	
	w = getnewwindow (132, false, &rport);
	
	if (w == nil) 
		goto error;
	
	if (abs (howlong) < 0x2222222) /*convert to ticks, avoiding overflow*/
		howlong *= 60;
	
	//you cannot call this with a window ptr, the function needs a dialog ptr
	//its really not needed anyway, but I will leave it in for the non-carbon version

	showwindow (w);	
	
	//Code change by Timothy Paustian Monday, August 21, 2000 4:31:49 PM
	//Must pass a CGrafPtr to pushport on OS X to avoid a crash
	{
	thePort = GetWindowPort(w);
		
	pushport (thePort);
	}
	setrect (&rport, 0, 0, 32, 32); // rport = (*w).portRect;
	
	sicn.iconlist = moofsmalliconlist;
		
	sicn.iconwindow = w;
	
	sicn.flinverted = false;
	
	sicn.flclearwhatsthere = true;
	
	sicn.iconrect = rport;
	
	insetrect (&sicn.iconrect, 8, 8);
	
	while (true) {
	
		for (i = 0; i < 7; i++) {
		
			sicn.iconnum = i;
			
			plotsmallicon (sicn);
			QDFlushPortBuffer(thePort, nil);
			if (mousebuttondown ()) {
				
				while (mousebuttondown ()) {}
				
				/*
				pushstyle (geneva, 9, 0);
				
				eraserect (rport);
				
				centerstring (rport, "\pPOOF!");
				
				popstyle ();
				
				delayticks (30);
				*/
				
				goto endloop;
				}
			
			if (howlong != 0) {
				
				if (gettickcount () - tc > (unsigned long) howlong)
					goto endloop;
				}
			
			delayticks (ticksbetweenframes);
			} /*for*/
		} /*while*/
	
	endloop:
	
	popport ();
	
	disposewindow (w);
	
	disposehandle ((Handle) sicn.hbits);
	
	return (true);

	error:
		
	sysbeep ();
	
	return (false);
	} /*myMoof*/


boolean initsmallicons (void) {
	
	loadsmallicon (miscsmalliconlist, &hmiscbits);
	
	/*
	loadsmallicon (leadersmalliconlist, &hleaderbits);

	loadsmallicon (draggingsmalliconlist, &hdraggingbits);

	loadsmallicon (findersmalliconlist, &hfinderbits);
	
	loadsmallicon (circlesmalliconlist, &hcirclebits);
	
	loadsmallicon (tablesmalliconlist, &htablebits);
	*/
	
	return (true);
	} /*initsmallicons*/


