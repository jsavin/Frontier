
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

#include "aeutils.h"
#include "memory.h"


static boolean getdescdata (AEDesc *desc, Handle *h) {
	
	/*
	PBS 03/14/02: get data from AEDesc's data handle as
	a handle. The handle is allocated here. Caller
	will dispose of handle, unless this function returns false.
	*/
	
	Size len;
	
	len = AEGetDescDataSize (desc);
	
/* kw - 2005-12-12 - we should return at least a zero sized handle
	if (len < 1) {
		
		*h = nil;
		
		return (false);
		}
*/
	if (!newclearhandle (len, h)) {
		
		*h = nil;
		
		return (false);
		}
	
	lockhandle (*h);
	
	if (AEGetDescData (desc, **h, len) != noErr) {
		 
		 disposehandle (*h);
		 
		 *h = nil;
		 
		 return (false);
		 } /*if*/
	
	unlockhandle (*h);
	
	return (true);
	} /*getdescdata*/


OSErr putdescdatapointer (AEDesc *desc, DescType typeCode, ptrvoid pvoid, long len) {
	
	/*
	PBS 03/14/02: put data into the datahandle.
	*/

	return (AEReplaceDescData (typeCode, pvoid, len, desc) == noErr);	
	} /*putdescdatapointer*/


boolean putdeschandle (AEDesc *desc, DescType typeCode, Handle h) {
	
	/*
	PBS 03/14/02: set a datahandle from a handle.
	*/

	OSErr err = noErr;
	
	lockhandle (h);
	
	err = AEReplaceDescData (typeCode, *h, gethandlesize (h), desc);
		
	unlockhandle (h);
	
	return (err != noErr);
	} /*putdeschandle*/


/*boolean newdescnull (AEDesc *desc) {

	AEInitializeDescInline (desc);
	
	return (AECreateDesc (typeNull, nil, 0, desc) != noErr);
	} #*newdescnull*/
boolean newdescnull (AEDesc *desc, DescType typeCode) {  // MJL 08/12/03: Broken boolean obj spec fix
    AEInitializeDescInline (desc);
    return (AECreateDesc (typeCode, nil, 0, desc) != noErr);
} /*newdescnull*/


boolean nildatahandle (AEDesc *desc) {

	AEInitializeDescInline (desc);
	return (true);
	//return (AEReplaceDescData (typeNull, nil, 0, desc) == noErr);	
	} /*nildatahandle*/
	

boolean newdescwithhandle (AEDesc *desc, DescType typeCode, Handle h) {
	
	/*
	PBS 03/14/02: Create a new AEDesc with the given type and value.
	*/
	
	OSErr err = noErr;
	long len = gethandlesize (h);
	
	AEInitializeDescInline (desc);
		
	if (h == nil)
		
		err = AECreateDesc (typeCode, nil, 0, desc);
		
	else {
	
		lockhandle (h);
		
		err = AECreateDesc (typeCode, *h, len, desc);

		unlockhandle (h);
		} /*else*/
	
	return (err == noErr);		
	} /*newdescwithhandle*/
	
	
boolean datahandletostring (AEDesc *desc, bigstring bs) {

	/*
	PBS 03/14/02: get data from AEDesc's data handle as
	a bigstring.
	
	2002-10-13 AR: Removed variable len to eliminate compiler warning
	about unused variables
	*/

	Handle h;
	
	if (!getdescdata (desc, &h))
		return (false);
	
	texthandletostring (h, bs);
	
	disposehandle (h);
	
	return (true);
	} /*datahandletostring*/


boolean copydatahandle (AEDesc *desc, Handle *h) {
	
	/*
	PBS 03/14/02: copy the datahandle to another handle.
	*/
	
	return (getdescdata (desc, h));
	} /*copydatahandle*/


