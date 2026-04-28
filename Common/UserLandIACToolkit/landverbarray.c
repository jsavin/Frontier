
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

#include "landinternal.h"



pascal boolean landaddclass (tyverbclass class) {
	
	register hdllandglobals hg = landgetglobals ();	
	register boolean fl = true;
	
	(**hg).currentclass = class;
	
	if ((**hg).transport == macsystem7) {
		
		/*
		if (!landverbsupported (class, (tyverbtoken) 0))
		*/
			fl = landsystem7addclass (class);
		}
	
	return (fl);
	} /*landaddclass*/


static pascal boolean landaddverbtoken (tyverbtoken token, boolean flfasthandler) {
	
	/*
	push the token on the verbarray list.
	*/
	
	register hdllandglobals hg = landgetglobals ();	
	register hdlverbarray hverbs = (**hg).verbarray;
	tyverbarrayelement item;

	if (hverbs == nil) {
		
		hdlverbarray hnewarray;
		
		if (!landnewemptyhandle ((Handle *) &hnewarray))
			return (false);

		hverbs = hnewarray; /*copy into register*/
		
		(**hg).verbarray = hverbs;
		}
		
	item.class = (**hg).currentclass;
		
	item.token = token;
	
	item.flfasthandler = flfasthandler;
	
	if (!landenlargehandle ((Handle) hverbs, longsizeof (item), &item))
		return (false);
	
	if (!flfasthandler)
		return (true);
	
	return (landsystem7addfastverb (item.class, item.token));
	} /*landaddverbtoken*/


pascal boolean landaddverb (tyverbtoken token) {
	
	/*
	push the verb's token on the verbarray list.
	*/
	
	return (landaddverbtoken (token, false));
	} /*landaddverb*/


pascal boolean landaddfastverb (tyverbtoken token) {
	
	/*
	push the verb's token on the verbarray list and set it up to be invoked 
	as a fast handler
	*/
	
	return (landaddverbtoken (token, true));
	} /*landaddfastverb*/	




