
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

#include "cursor.h"
#include "kb.h"
#include "memory.h"
#include "search.h"
#include "strings.h"
#include "langexternal.h"
#include "opinternal.h"
#include "tablestructure.h"
#include "tableinternal.h"
#include "tableverbs.h"
#include "db_format.h"




static hdlhashtable hsearchtable;

static bigstring bssearchmarker;

static boolean flsearchundermarker;

static boolean flsearchwrapped;


static boolean tableassigntocell (hdlhashtable htable, bigstring bsname, hdlhashnode hnode, tyvaluerecord val) {
	
	if (!coercevalue (&val, (**hnode).val.valuetype)) {
		
		disposevaluerecord (val, false);
		
		return (false);
		}
	
	hashtableassign (htable, bsname, val);
	
	exemptfromtmpstack (&val);
	
	return (true);
	} /*tableassigntocell*/


static void tablezoomfound (bigstring bsname, short col, long ixfound, short lenfound) {

	if (searchparams.flzoomfound) {
		
		tablezoomtoname (hsearchtable, bsname);
		
		if (shellpushfrontglobals ()) {
			
			if (searchparams.flwindowzoomed)
				(**shellwindowinfo).flopenedforfind = true;
			
			(**tableformatsdata).focuscol = col;
			
			if (opsettextmode (true))
				opeditsetselection (ixfound, ixfound + lenfound);
			
			shellpopglobals ();
			}
		}
	} /*tablezoomfound*/


static boolean tablesearchcellname (hdlhashnode hnode, bigstring bscell) {
	
	/*
	4/3/92 dmb: when replacing all, search for multiple occurences in the string, 
	and actually assign changed string into values as well as names.
	
	4.0.2b1 dmb: separate code for names; values may be handles now
	*/
	
	short ix = 0;
	short len;
	boolean fldidreplace = false;
	bigstring bs;
	
	gethashkey (hnode, bs);
	
	while (true) {
		
		if (!stringsearch (bs, &ix, &len)) {
		
			if (fldidreplace)
				hashsetnodekey (hsearchtable, hnode, bs);
			
			return (false);
			}
		
		if (!searchparams.flreplaceall) {
			
			tablezoomfound (bscell, namecolumn, ix, len);
			
			return (true);
			}
		
		replacestring (bs, ix + 1, len, searchparams.bsreplace);
		
		++searchparams.ctreplaced;
		
		fldidreplace = true;
		
		/*keep looping until we fail*/
		ix += stringlength (searchparams.bsreplace);
		}
	} /*tablesearchcellname*/


static boolean tablesearchcellvalue (hdlhashnode hnode, bigstring bscell, tyvaluerecord val) {
	
	/*
	4/3/92 dmb: when replacing all, search for multiple occurences in the string, 
	and actually assign changed string into values as well as names.
	
	4.0.2b1 dmb: separate code for values to handle > 255 characters
	
	5.0.2b20 dmb: for tokentype, return false, not true.
	
	7.1b40 PBS: fix off-by-one selection error with addresses.
	*/
	
	long ix = 0;
	ptrstring bsreplace = searchparams.bsreplace;
	Handle hsearch;
	long len, lenmatch;
	boolean fldidreplace = false;
	boolean fladdress = false;
	
	if (val.valuetype == tokenvaluetype)
		return (false);
	
	if (val.valuetype == addressvaluetype) /*7.1b40 PBS: part off-by-one selection fix.*/
		fladdress = true;
	
	/*copy value unless we're just finding, and it's a string in memory */
	if (searchparams.flreplaceall || (val.valuetype != stringvaluetype) || val.fldiskval)
		if (!copyvaluerecord (val, &val) || !coercetostring (&val))
			return (true);
	
	hsearch = val.data.stringvalue;
	
	len = gethandlesize (hsearch);
	
	while (true) {
		
		if (!handlesearch (hsearch, &ix, &lenmatch)) {
			
			if (fldidreplace) {
			
				if (!tableassigntocell (hsearchtable, bscell, hnode, val)) /*error; stop search*/
					return (true);
				}
			else {
				if (exemptfromtmpstack (&val))
					disposevaluerecord (val, false);
				}
			
			return (false);
			}
		
		if (!searchparams.flreplaceall) {
			
			if (fladdress) /*7.1b40 PBS: add one to ix to account for @ symbol -- fix off-by-one selection.*/
				tablezoomfound (bscell, valuecolumn, ix + 1, lenmatch);
			
			else
				tablezoomfound (bscell, valuecolumn, ix, lenmatch);
			
			return (true);
			}
		
		if (!mungehandle (hsearch, ix, lenmatch, bsreplace + 1, stringlength (bsreplace)))
			return (false);
		
		++searchparams.ctreplaced;
		
		fldidreplace = true;
		
		/*keep looping until we fail*/
		ix += stringlength (bsreplace);
		}
	} /*tablesearchcellvalue*/


static boolean tablefindvisit (bigstring bsname, hdlhashnode hnode, tyvaluerecord val, ptrvoid refcon) {

	/*
	7/4/91 dmb: langexternalzoom now takes table/name pair instead
	of full path.

	7/29/91 dmb: handle activate events after zooming so deferred visiing
	takes place

	9/12/91 dmb: new marker logic to handle wraparound search

	Phase 9: unwrap tablesortedsearchctx; resolve disk values via
	copyvaluerecord_internal before tablesearchcellvalue.
	*/

	tablesortedsearchctx *sctx = (tablesortedsearchctx *) refcon;
	boolean flzoom;
	boolean flsearchcell = true;
	boolean flsearchvalue = true;

	if (!isemptystring (bssearchmarker)) { /*we've haven't passed the marked entry*/

		if (!equalstrings (bsname, bssearchmarker)) { /*we haven't reached the marked entry*/

			flsearchcell = false;

			flsearchvalue = false;
			}
		else { /*we're at the marked cell*/

			setemptystring (bssearchmarker);

			flsearchcell = false;

			flsearchvalue = flsearchundermarker;
			}
		}

	if (flsearchwrapped) { /*if the search has wrapped, we search *before* the marker, not after*/

		flsearchcell = !flsearchcell;

		flsearchvalue = !flsearchvalue;
		}

	if (flsearchcell) {

		rollbeachball ();

		if (tablesearchcellname (hnode, bsname))
			return (true);

		if (val.valuetype != externalvaluetype) {

			boolean flresolveddisk = false;

			if (val.fldiskval) {
				db_context dbctx;
				if (db_is_v7 (sctx->hdb))
					db_context_init_v7_read (&dbctx, sctx->hdb);
				else
					db_context_init_legacy_read (&dbctx, sctx->hdb);
				if (!copyvaluerecord_internal (&dbctx, val, &val))
					return (true);
				flresolveddisk = true;
			}

			if (tablesearchcellvalue (hnode, bsname, val)) {
				if (flresolveddisk)
					if (exemptfromtmpstack (&val))
						disposevaluerecord (val, false);
				return (true);
			}

			if (flresolveddisk)
				if (exemptfromtmpstack (&val))
					disposevaluerecord (val, false);
			}
		}

	if (searchparams.flonelevel || !flsearchvalue)
		return (false);

	if (keyboardescape ()) /*user pressed cmd-period -- stop visiting*/
		return (true);

	if (!langexternalsearch (val, &flzoom))
		return (false);

	if (searchparams.flreplaceall) /*keep going*/
		return (false);

	if (flzoom && searchparams.flzoomfound) {

		hdlwindowinfo hinfo;

		langexternalzoom (val, hsearchtable, bsname);

		if (getfrontwindowinfo (&hinfo))
			(**hinfo).flopenedforfind = searchparams.flwindowzoomed;

		shellpartialeventloop (activMask); /*handle pending activate events*/
		}

	return (true);
	} /*tablefindvisit*/


static boolean tablefind (hdlhashtable htable, boolean flwrap, bigstring bsmark, boolean flmarkbeforevalue) {
	
	/*
	9/12/91 dmb: added wrap, marker parameters; now handle wraparound search
	
	5.1.5 dmb: use tablesortedinversesearch for guest databases
	*/
	
	hdlhashtable hsave = hsearchtable;
	boolean fl;
	
	hsearchtable = htable; /*so tablefindvisit can zoom quickly*/
	
	if (bsmark) { /*a marker is being provided*/
		
		copystring (bsmark, bssearchmarker); /*so visit routine can see where we are.*/
		
		flsearchundermarker = flmarkbeforevalue; /*start search with marked cell's submaterial*/
		}
	
	if (flwrap)
		flsearchwrapped = false;
	
	fl = tablesortedinversesearch (htable, &tablefindvisit, htable);
	
	if (!fl && flwrap) { /*marker will have been cleared; we can try again*/
		
		copystring (bsmark, bssearchmarker); /*re-establish marker*/
		
		flsearchwrapped = true;
		
		fl = tablesortedinversesearch (htable, &tablefindvisit, nil);
		}
	
	hsearchtable = hsave;
	
	return (fl);
	} /*tablefind*/


boolean tableverbfind (hdlexternalvariable hvariable, boolean *flzoom) {
	
	register hdltablevariable hv = (hdltablevariable) hvariable;
	register hdlhashtable ht;
	register boolean fl;
	boolean fltempload;
	
	fltempload = !(**hv).flinmemory;

	{
		db_context ctx;
		/* Use variable's own database and format mode */
		if (db_format_is_legacy_db((**hv).hdatabase)) {
			db_context_init_legacy_read(&ctx, (**hv).hdatabase);
		} else {
			db_context_init(&ctx);
			ctx.database = (**hv).hdatabase;
		}

		if (!tableverbinmemory (&ctx, (hdlexternalvariable) hv, HNoNode))
			return (false);
		}
	
	ht = (hdlhashtable) (**hv).variabledata; 
	
	fl = tablefind (ht, false, nil, false);
	
	if (fltempload && !fl && tablenosubsdirty (ht))
		tableverbunload ((hdlexternalvariable) hv);
	
	*flzoom = false; /*we take care of our own zooming*/
	
	return (fl);
	} /*tableverbfind*/


static boolean continuetablesearch (hdlexternalvariable hvariable) {
	
	/*
	the caller just finished searching hvariable without finding a match.
	
	our job is to continue the search in the table that contains the variable.
	
	9/19/91 dmb: don't clear keyboard escape; if we're called from a script, 
	we want the script to be aborted as well
	*/
	
	hdlhashtable htable;
	register long tablerefcon;
	bigstring bsname;
	boolean fl;
	
	if (!tablefindvariable (hvariable, &htable, bsname))
		return (false);
	
	if (htable == nil) /*hvariable is rootvariable*/
		return (false);
	
	tablerefcon = (**htable).hashtablerefcon;
	
	fl = tablefind (htable, searchshouldwrap (tablerefcon), bsname, false);
	
	if (keyboardescape ()) {
		
		/*
		keyboardclearescape ();
		*/
		
		return (false);
		}
	
	if (!fl) { /*not found in our table or subtables; try continuing in parent*/
		
		if (!searchshouldcontinue (tablerefcon)) /*we're at table that started search*/
			return (false);
		
		fl = continuetablesearch ((hdlexternalvariable) tablerefcon); /*recurse outwards*/
		}
	
	return (fl);
	} /*continuetablesearch*/


boolean tableverbcontinuesearch (hdlexternalvariable hvariable) {
	
	/*
	a wrapper for continuetablesearch
	
	12/17/91 dmb: added flopenedforfind support.  he next step would be to 
	clear the flag in all open windows when a new search is begun
	*/
	
	WindowPtr origwindow = shellwindow;
	hdlwindowinfo origwindowinfo = shellwindowinfo;
	
	/*
	if (searchparams.floneobject) /%user has flat search turned off%/
		return (false);
	*/
	
	if (!continuetablesearch (hvariable))
		return (false);
	
	if (searchparams.flclosebehind) {
		
		if ((origwindowinfo != nil) && (**origwindowinfo).flopenedforfind)
			shellclosewindow (origwindow);
		}
	
	return (true);
	} /*tableverbcontinuesearch*/


boolean tableverbsearch (void) {
	
	hdlhashtable htable;
	long tablerefcon;
	bigstring bsname;
	boolean fl;
	tyvaluerecord val;
	hdlhashnode hnode;
	
	tablegetcursorinfo (&htable, bsname, &val, &hnode); /*find out where to start*/
	
	tablerefcon = (**htable).hashtablerefcon;
	
	startingtosearch (tablerefcon);
	
	fl = tablefind (htable, searchshouldwrap (tablerefcon), bsname, true);
	
	if (keyboardescape ()) {
		
		/*
		keyboardclearescape ();
		*/
		
		return (false);
		}
	
	if (!fl) { /*not found in our table or subtables; try continuing in parent*/
		
		if (searchshouldcontinue (tablerefcon))
			fl = tableverbcontinuesearch ((hdlexternalvariable) tablerefcon);
		}
	
	return (fl);
	} /*tableverbsearch*/



