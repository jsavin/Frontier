
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


#pragma pack(2)
typedef struct tyqueuerecord {
	
	Handle hdata;
	
	struct tyqueuerecord **hnext;
	} tyqueuerecord, *ptrqueuerecord, **hdlqueuerecord;
#pragma options align=reset


boolean landpushqueue (Handle h) {
	
	/*
	implement a first-in-first-out queue.  push a new element at the end
	of the list headed bu by (**landglobals).hqueue.
	*/
	
	register hdllandglobals hg = landgetglobals ();
	register hdlqueuerecord x;
	register hdlqueuerecord hlast = nil;
	register short queuedepth = 0;
	hdlqueuerecord hnew;
	tyqueuerecord q;
	
	q.hdata = h;
	
	q.hnext = nil;
	
	if (!landnewfilledhandle (&q, longsizeof (tyqueuerecord), (Handle *) &hnew))
		return (false);
	
	x = (hdlqueuerecord) (**hg).hqueue;
	
	if (x == nil) { /*inserting into empty queue*/
		
		(**hg).hqueue = (Handle) hnew;
		
		return (true);
		}
		
	while (x != nil) { /*find the last guy in the queue*/
		
		hlast = x;
		
		x = (**x).hnext;
		
		queuedepth++;
		} /*while*/
	
	(**hlast).hnext = hnew;
	
	if (queuedepth > (**hg).maxqueuedepth) /*for display in stats window*/
		(**hg).maxqueuedepth = queuedepth;
	
	return (true);
	} /*landpushqueue*/


boolean landpopqueue (Handle *h) {
	
	/*
	pop the first guy off the queue, returning the data saved in h.
	
	return false if the queue is empty.
	*/
	
	register hdllandglobals hg = landgetglobals ();
	register hdlqueuerecord x = (hdlqueuerecord) (**hg).hqueue;
	
	if (x == nil) /*empty queue*/
		return (false);
	
	*h = (**x).hdata; /*return the first guy on the queue*/
	
	(**hg).hqueue = (Handle) (**x).hnext;
	
	landdisposehandle ((Handle) x);
	
	return (true);
	} /*landpopqueue*/


boolean landpopqueueitem (landqueuepopcallback cb, long refcon, Handle *h) {
	
	/*
	scan the queue for the item that satisfies the callback
	
	if found, pop it out and return true; else return false.  
	*/
	
	register hdlqueuerecord x;
	register hdlqueuerecord hnext;
	register hdlqueuerecord hprev = nil;
	register hdllandglobals hg = landgetglobals ();
	
	x = (hdlqueuerecord) (**hg).hqueue;
	
	while (x != nil) { /*haven't reached end of queue*/
		
		hnext = (**x).hnext;
		
		if ((*cb) ((**x).hdata, refcon)) { /*found it*/
			
			*h = (**x).hdata;
			
			if (hprev == nil) /*was first in queue*/
				(**hg).hqueue = (Handle) hnext;
			else
				(**hprev).hnext = hnext;
			
			landdisposehandle ((Handle) x);
			
			return (true);
			}
		
		hprev = x;
		
		x = hnext;
		}
	
	return (false); /*didn't find it*/
	} /*landpopqueueitem*/


boolean landemptyqueue (void) {
	
	/*
	empty out the queue, return true if it was non-empty.
	*/
	
	register boolean fl = false;
	Handle h;
	
	while (landpopqueue (&h))
		fl = true;
		
	return (fl);
	} /*landemptyqueue*/



