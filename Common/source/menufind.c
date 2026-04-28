
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

#include "search.h"
#include "cursor.h"
#include "kb.h"
#include "langexternal.h"
#include "opinternal.h"
#include "menueditor.h"
#include "menuinternal.h"




static boolean flfoundinscript;


boolean mesearchrefconroutine (hdlheadrecord hnode) {
	
	/*
	load the script linked into the headrecord, search it for the search string, and
	return with hnodesearch (a global) pointing at the menu item where the match was
	found.
	
	9/19/90 DW: we weren't checking for a nil outlinerecord handle.  it happens when
	the menu item has no script linked to it.
	*/
	
	hdloutlinerecord houtline;
	hdloutlinerecord hmenuoutline;
	register hdloutlinerecord ho;
	boolean flfound;
	boolean fljustloaded;
	hdlwindowinfo hinfo;
	boolean fldirty;
	boolean fldisplaywasenabled;
	
	if (searchparams.flonelevel)
		return (false);
	
	if (keyboardescape ())
		return (false);
	
	if (!meloadscriptoutline (menudata, hnode, &houtline, &fljustloaded)) /*error loading script*/
		return (false);
	
	ho = houtline; /*copy into register*/
	
	if (ho == nil) /*no script linked into the menu item*/
		return (false);
	
	oppushoutline (ho);
	
	fldisplaywasenabled = opdisabledisplay (); /*we pushed outline, but not its window*/
	
	flfound = opflatfind (true, false);
	
	fldirty = (**ho).fldirty; /*in case of replace all*/
	
	if (fldisplaywasenabled)
		openabledisplay ();
	
	oppopoutline (); /*restore original*/
	
	if (flfound) { /*match found, position bar cursor and return true*/
		
		mesetscriptoutline (hnode, ho); /*make loaded state stick*/
		
		hmenuoutline = op_get_outlinedata();
		
		oppushoutline (nil); /*save on stack*/
		
		if (!mezoommenubarwindow (hmenuoutline, false, &hinfo)) { /*probably out of memory*/
			
			oppopoutline ();
			
			return (false);
			}
		
		shellpushglobals ((**hinfo).macwindow);
		
		if (searchparams.flwindowzoomed)
			(**hinfo).flopenedforfind = true;
		
		meexpandto (hnode);
		
		mezoomscriptwindow ();
		
		shellpopglobals ();
		
		oppopoutline ();
		
		flfoundinscript = true; /*so caller will know display is already done*/
		
		return (true);
		}
	
	if (fljustloaded) {
		
		if (fldirty) { /*must be doing a replace all*/
			
			mesetscriptoutline (hnode, ho); /*make loaded state stick*/
			
			(**op_get_outlinedata()).fldirty = true; /*percolate up to parent outline*/
			}
		else
			opdisposeoutline (ho, false);
		}
	
	return (false);
	} /*mesearchrefconroutine*/


boolean mesearchoutline (boolean flfromtop, boolean flwrap, boolean *flzoom) {
	
	flfoundinscript = false;
	
	if (!opflatfind (flfromtop, flwrap))
		return (false);
	
	*flzoom = !flfoundinscript;
	
	/*
	if (flfoundinscript && searchparams.flwindowzoomed) {
		
		hdlwindowinfo hinfo;
		
		if (getfrontwindowinfo (&hinfo))
			(**hinfo).flopenedforfind = true;
		}
	*/
	
	return (true);
	} /*mesearchoutline*/


boolean mecontinuesearch (hdlwindowinfo hinfo, hdlheadrecord hnode) {
	
	/*
	9/12/91 dmb: new search logic checks for flat search before calling here; 
	we don't have enought info to check
	
	5.0.2b20 dmb: start search from headline _after_ hnode; don't get stuck here.
	*/
	
	boolean fl, flzoom;
	long menurefcon;
	
	shellpushglobals ((**hinfo).macwindow);
	
	mecheckglobals ();
	
	menurefcon = (**menudata).menurefcon;
	
	if (!opnavigate (flatdown, &hnode))
		fl = false;
	
	else {
		opmoveto (hnode);
		
		opsettextmode (false); /*make sure we search text of new headline*/
		
		fl = mesearchoutline (false, searchshouldwrap (menurefcon), &flzoom);
		}
	
	if (fl) {
		
		if (searchparams.flzoomfound && flzoom)
			shellbringtofront (shellwindowinfo);
		}
	else {
		
		if (searchshouldcontinue (menurefcon))
			fl = langexternalcontinuesearch ((hdlexternalvariable) menurefcon);
		}
	
	shellpopglobals ();
	
	return (fl);
	} /*mecontinuesearch*/


boolean menuverbsearch (void) {
	
	boolean flzoom;
	long menurefcon;
	
	mecheckglobals ();
	
	menurefcon = (**menudata).menurefcon;
	
	startingtosearch (menurefcon);
	
	if (mesearchoutline (false, searchshouldwrap (menurefcon), &flzoom))
		return (true);
	
	if (keyboardescape ()) {
		
		/*
		keyboardclearescape ();
		*/
		
		return (false);
		}
	
	if (!searchshouldcontinue (menurefcon))
		return (false);
	
	return (langexternalcontinuesearch ((hdlexternalvariable) menurefcon));
	} /*menuverbsearch*/




