
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

#include "landinternal.h"



boolean landnoparamcallback (landcallback cb) {
	
	if (cb == nil) /*defensive driving*/
		return (false);
		
	#ifdef THINKC
	
		if (!CallPascalW (cb))
			return (false);
			
	#else
	
		if (!((tynoparamcallback) *cb) ())
			return (false);
			
	#endif
	
	return (true);
	} /*landnoparamcallback*/


boolean landverbrecordcallback (hdlverbrecord hverb, landcallback cb) {
	
	register boolean fl;
	
	if (cb == nil) /*defensive driving*/
		return (false);
		
	#ifdef THINKC
	
		fl = (CallPascalW (hverb, cb) != 0);	
		
	#else
	
		fl = ((tyverbrecordcallback) *cb) (hverb);
		
	#endif
	
	return (fl);
	} /*landverbrecordcallback*/
	

boolean landfilespeccallback (FSSpec *fs, landcallback cb) {
	
	register boolean fl;
	
	if (cb == nil) /*defensive driving*/
		return (false);
		
	#ifdef THINKC
	
		fl = (CallPascalW (fs, cb) != 0);
		
	#else
	
		fl = ((tyfilespeccallback) *cb) (fs);
		
	#endif
	
	return (fl);
	} /*landfilespeccallback*/
	

