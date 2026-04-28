
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


#ifndef fljustfrontier

boolean landlockhandle (Handle hlock) {
	
	register Handle h = hlock;
	
	if (h == nil)
		return (false);
		
	HLock (h);
	
	return (true);
	} /*landlockhandle*/
	
	
boolean landunlockhandle (Handle hunlock) {
	
	register Handle h = hunlock;
	
	if (h == nil)
		return (false);
		
	HUnlock (h);
	
	return (true);
	} /*landunlockhandle*/


landmoveleft (void *psource, void *pdest, long length) {
	
	/*
	do a mass memory move with the left edge leading.  good for closing
	up a gap in a buffer, among other thingsÉ
	*/
	
	register byte *ps, *pd;
	register long ctloops;
	
	ctloops = length;
	
	if (ctloops > 0) {
	
		ps = psource; /*copy into a register*/
	
		pd = pdest; /*copy into a register*/
	
		while (ctloops--) *pd++ = *ps++;
		}
	} /*landmoveleft*/
	

landclearbytes (void *pclear, long ctclear) {
	
	/*
	fill memory with 0's.
	*/
	
	memset (pclear, 0, ctclear);
	} /*landclearbytes*/
	

land4bytestostring (long bytes, bigstring bs) {
	
	setstringlength (bs, 4);
	
	landmoveleft (&bytes, &bs [1], 4L);
	} /*land4bytestostring*/


landsetcursortype (short newcursor) {

	register CursHandle hcursor;

	if (newcursor == 0) {
		
		SetCursor (&quickdrawglobal (arrow));

		return;
		}

	hcursor = GetCursor (newcursor);
	
	if (hcursor != nil)
		SetCursor (*hcursor);
	} /*landsetcursortype*/


landcopystring (bssource, bsdest) bigstring bssource, bsdest; {

	/*
	create a copy of bssource in bsdest.  copy the length byte and
	all the characters in the source string.

	assume the strings are pascal strings, with the length byte in
	the first character of the string.
	*/
	
	register short i, len;
	
	len = (short) bssource [0];
	
	for (i = 0; i <= len; i++) 
		bsdest [i] = bssource [i];
	} /*landcopystring*/


landcopyheapstring (hdlbigstring hsource, bigstring bsdest) {
	
	landcopystring (*hsource, bsdest);
	} /*landcopyheapstring*/


boolean landpushstring (bigstring bssource, bigstring bsdest) {

	/*
	insert the source string at the end of the destination string.
	
	assume the strings are pascal strings, with the length byte in
	the first character of the string.
	*/
	
	register short lensource = stringlength (bssource);
	register short lendest = stringlength (bsdest);
	register byte *psource, *pdest;
	
	if ((lensource + lendest) > lenbigstring)
		return (false);
		
	pdest = (byte *) bsdest + lendest + 1;
	
	psource = (byte *) bssource + 1;
	
	bsdest [0] += (byte) lensource;
	
	while (lensource--) *pdest++ = *psource++;

	return (true);
	} /*landpushstring*/
	
	
boolean landpushlong (long x, bigstring bs) {
	
	bigstring bslong;
	
	NumToString (x, bslong);
	
	return (landpushstring (bslong, bs));
	} /*landpushlong*/
	

boolean landequalstrings (bigstring bs1, bigstring bs2) {

	/*
	return true if the two strings (pascal type, with length-byte) are
	equal.  return false otherwise.
	*/
	
	register ptrbyte p1 = (ptrbyte) bs1, p2 = (ptrbyte) bs2;
	register short ct = *p1 + 1;
	
	while (ct--) 
		
		if (*p1++ != *p2++)
		
			return (false);
		
	return (true); /*loop terminated*/
	} /*landequalstrings*/


boolean landnewfilledhandle (void *pdata, long size, Handle *hnew) {
	
	/*
	create a new handle of the indicated size, filled with the indicated data.
	
	return false if the allocation failed.
	*/
	
	register Handle h;

	h = *hnew = NewHandle (size);

	if (h == nil)
		return (false);

	landmoveleft (pdata, *h, size);
	
	return (true);
	} /*landnewfilledhandle*/
	

landdisposehandle (Handle h) {
	
	/*
	our own bottleneck for this built-in.  we don't require type coercion
	and we check if its nil.
	*/
	
	if (h != nil)
		DisposHandle (h);
	} /*landdisposehandle*/


hdlbigstring landnewstring (bigstring bs) {
	
	return (NewString (bs));
	} /*landnewstring*/
	

boolean landenlargehandle (Handle hgrow, long ctgrow, void *newdata) {
	
	/*
	make the handle big enough to hold the new data, and move the new data in
	at the end of the newly enlarged handle.
	*/
	
	register Handle h = hgrow;
	register long ct = ctgrow;
	register ptrbyte p = newdata;
	register long origsize;
	
	origsize = GetHandleSize (h);
	
	SetHandleSize (h, origsize + ct);
		
	if (MemError () != noErr)
		return (false);
		
	landmoveleft (p, *h + origsize, ct);
	
	return (true);
	} /*landenlargehandle*/ 
	
	
boolean landshrinkhandle (Handle h, long ct) {
	
	/*
	make the handle smaller -- by ct bytes.
	*/
	
	SetHandleSize (h, GetHandleSize (h) - ct);
	
	return (MemError () == noErr);
	} /*landshrinkhandle*/
	
	
boolean landloadfromhandle (hload, ixload, ctload, pdata) Handle hload; long *ixload, ctload; void *pdata; {
	
	/*
	copy the next ctload bytes from hload into pdata and increment the index.
	
	return false if there aren't enough bytes.
	
	start ixload at 0.
	*/
	
	register Handle h = hload;
	register ptrbyte p = pdata;
	register long ct = ctload;
	register long ix = *ixload;
	register long size;
	
	size = GetHandleSize (h);
	
	if ((ix + ct) > size) /*asked for more bytes than there are*/
		return (false); 
		
	landmoveleft (*h + ix, p, ct); /*copy out of the handle*/
	
	*ixload = ix + ct; /*increment the index into the handle*/
	
	return (true);
	} /*landloadfromhandle*/


boolean landloadfromhandletohandle (Handle hload, long *ixload, long ctload, Handle *hnew) {
	
	/*
	load from the source handle, creating a new handle to hold the loaded stuff.
	*/
	
	register Handle h;
	
	h = NewHandle (ctload);

	if (h == nil)
		return (false);
	
	if (!landloadfromhandle (hload, ixload, ctload, *h)) {
		
		landdisposehandle (h);

		return (false);
		}
	
	*hnew = h;
	
	return (true);
	} /*landloadfromhandletohandle*/


boolean landcopyhandle (horig, hcopy) Handle horig, *hcopy; {
	
	register Handle h;
	register long ctbytes;
	
	ctbytes = GetHandleSize (horig);
	
	h = NewHandle (ctbytes);
	
	if (h == nil)
		return (false);
		
	landmoveleft (*horig, *h, ctbytes);
	
	*hcopy = h;
	
	return (true);
	} /*landcopyhandle*/


boolean landnewclearhandle (long ctbytes, Handle *hreturned) {
	
	register long ct = ctbytes;
	register Handle h;
	
	*hreturned = h = NewHandle (ct);
	
	if (h == nil) 
		return (false);
		
	landclearbytes (*h, ct);
	
	*hreturned = h;
	
	return (true);
	} /*landnewclearhandle*/


long landgethandlesize (Handle h) {
	
	return (GetHandleSize (h));
	} /*landgethandlesize*/
	
	
boolean landnewemptyhandle (Handle *h) {
	
	return (landnewclearhandle (0L, h));
	} /*landnewemptyhandle*/


boolean landsurrenderprocessor (EventRecord *ev) {
	
	WaitNextEvent (nullEvent, ev, 1, nil);
	
	return (true);
	} /*landsurrenderprocessor*/

#endif

boolean landbreakembrace (EventRecord *ev) {
	
	register hdllandglobals hg = landgetglobals ();
	
	return ((*(**hg).breakembraceroutine) (ev));
	
	/*
	return (landnoparamcallback ((**hg).breakembraceroutine));
	*/
	} /*landbreakembrace*/


boolean landgetappcreator (OSType *creator) {
	
	/*
	get the 4-character creator identifier for the application we're running 
	inside of.
	*/
	
	ProcessSerialNumber psn;
	ProcessInfoRec info;
	
	
		psn.highLongOfPSN = 0;
		
		psn.lowLongOfPSN =  kCurrentProcess;

	
	info.processInfoLength = (long) sizeof (info);
	
	info.processName = nil;
	
	info.processAppSpec = nil;
	
	GetProcessInformation (&psn, &info);
	
	*creator = info.processSignature;
	
	return (true);
	} /*landgetappcreator*/




