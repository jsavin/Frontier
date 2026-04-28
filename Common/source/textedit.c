
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

#include "font.h"
#include "quickdraw.h"
#include "memory.h"
#include "strings.h"
#include "textedit.h"



void edittextbox (bigstring bs, Rect r, short fontnum, short fontsize, short fontstyle) {
	
	pushstyle (fontnum, fontsize, fontstyle);
	
		TETextBox (stringbaseaddress (bs), (long) stringlength (bs), &r, teJustLeft);

	
	popstyle ();
	} /*edittextbox*/


boolean edittwostringbox (bigstring bs1, bigstring bs2, Rect r, short fontnum, short fontsize) {
	
	/*
	a front-end for the textedit routine TextBox.  we take two strings, concatenate
	them in a buffer with two carriage returns between the strings, display the
	result, dispose the buffer.
	*/
	
	long len;
	Handle hbuffer;
	bigstring bs;
	
	if (!newtexthandle (bs1, &hbuffer))
		return (false);
	
	if (stringlength (bs2) > 0) {
		
		setstringlength (bs, 2);
		
		bs [1] = bs [2] = chreturn;
		
		if (!pushtexthandle (bs, hbuffer)) {
			
			disposehandle (hbuffer);
			
			return (false);
			}
			
		if (!pushtexthandle (bs2, hbuffer)){
			
			disposehandle (hbuffer);
			
			return (false);
			}
		}
		
	len = gethandlesize (hbuffer);
		
	pushstyle (fontnum, fontsize, 0);
	
	lockhandle (hbuffer);
	
		TETextBox (*hbuffer, len, &r, teJustLeft);

	
	
	unlockhandle (hbuffer);
	
	disposehandle (hbuffer);
	
	popstyle ();
	
	return (true);
	} /*edittwostringbox*/


