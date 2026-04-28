
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

/*Synthetic buttons for outline windows.*/

#include "frontier.h"
#include "standard.h"

#include "shell.h"
#include "lang.h"
#include "oplist.h"
#include "strings.h"
#include "langxml.h"
#include "processinternal.h"
#include "ops.h"
#include "opbuttons.h"


void opbuttonsattach (hdlwindowinfo hinfo, hdlhashtable htable) {

	/*
	7.1b18 PBS: attach buttons to this outline.
	*/

	hdlhashnode hn;
	short ct = 0;
	hdllistrecord hlist;

	(**hinfo).flhidebuttons = false;

	(**hinfo).flsyntheticbuttons = true;

	opnewlist (&hlist, false);
	
	(**hinfo).buttonlist = (Handle) hlist;
	
	/*loop through all of the items in the table*/
	
	for (hn = (**htable).hfirstsort; hn != nil; hn = (**hn).sortedlink) {

		bigstring bsname;

		ct++;

		if (ct > 16) /*16 is max number of buttons*/
			break;

		gethashkey (hn, bsname);
		
		xmlgetname (bsname);

		oppushstring ((hdllistrecord) (**hinfo).buttonlist, nil, bsname);
		} /*for*/

	(**hinfo).buttonscripttable = (Handle) htable;
	} /*opbuttonsattach*/


boolean opbuttonstatus (short buttonnum, tybuttonstatus *status) {
#pragma unused (buttonnum)

	/*
	7.1b18 PBS: buttons are always displayed and enabled.
	*/
	
	if (op_get_outlinedata() == NULL)
		return (false);

	(*status).flenabled = true;
	
	(*status).fldisplay = true;
	
	(*status).flbold = false; /*our buttons are never bold*/
	
	return (true);
	} /*opbuttonstatus*/


boolean opbutton (short buttonnum) {
	
	/*
	7.1b18 PBS: Handle a click in an outline button. Run
	the associated script.
	*/
	
	hdlhashtable ht = (hdlhashtable) (**shellwindowinfo).buttonscripttable;
	hdlhashnode hn;
	hdltreenode hcode;
	tyvaluerecord v;
	short ct = 0;

	/*Find the item in the table and call the script.*/
	
	for (hn = (**ht).hfirstsort; hn != nil; hn = (**hn).sortedlink) {

		ct++;

		if (ct > 16) /*16 is max number of buttons*/
			break;

		if (ct == buttonnum) {
		
			langcompilescript (hn, &hcode);

			langexternalvaltocode ((**hn).val, &hcode);

			langruncode (hcode, nil, &v);

			disposevaluerecord (v, false);

			break;
			} /*if*/		
		} /*for*/

	return (true);
	} /*scriptbutton*/
