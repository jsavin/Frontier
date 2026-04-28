
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

#include "landinternal.h"



pascal boolean landpushparam (hdlverbrecord hverb, typaramtype type, Handle hval, void *pval, long len, typaramkeyword key) {
	
	/*
	2.1b3 dmb: no more typaramrecords
	*/
	
	register hdlverbrecord hv = hverb;
	AppleEvent event, reply;
	AERecord *evt;
	
	landsystem7geteventrecords (hv, &event, &reply);
	
	if ((**hv).verbtoken == returntoken)
		evt = &reply;
	else
		evt = &event;
	
	if (!landsystem7pushparam (evt, type, hval, pval, len, key))
		return (false);
	
	(**hv).ctparams++; /*added room for another param*/
	
	return (true);
	} /*landpushparam*/

	
pascal boolean landpushintparam (hdlverbrecord hverb, short x, typaramkeyword key) {
	
	return (landpushparam (hverb, inttype, nil, &x, longsizeof (x), key));
	} /*landpushintparam*/
	
	
pascal boolean landpushlongparam (hdlverbrecord hverb, long x, typaramkeyword key) {
	
	return (landpushparam (hverb, longtype, nil, &x, longsizeof (x), key));
	} /*landpushlongparam*/


pascal boolean landpushstringparam (hdlverbrecord hverb, bigstring bs, typaramkeyword key) {
	
	return (landpushparam (hverb, texttype, nil, bs + 1, stringlength (bs), key));
	} /*landpushstringparam*/




