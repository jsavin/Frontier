
/*	$Id$    */

/******************************************************************************

    UserLand Frontier(tm) -- High performance Web content management,
    object database, system-level and Internet scripting environment,
    including source code editing and debugging.

    Copyright (C) 1992-2004 UserLand Software, Inc.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

******************************************************************************/

/* 2025-11-24 Codex: Normalize BE writes/coverage for v7 portability. */


#include <standard.h>


#include "error.h"
#include "memory.h"
#include "shellhooks.h"



#define safetycushionsize 0x2800 /*10K*/

#ifdef fldebug

long cttemphandles = 0;

THz tempzone;

#endif


static Handle hsafetycushion = nil; /*a buffer to allow memory error reporting*/

static boolean flholdsafetycushion = false;


static boolean getsafetycushion (void) {
	
	if (hsafetycushion == nil)
		hsafetycushion = NewHandle (safetycushionsize);
	
	return (hsafetycushion != nil);
	} /*getsafetycushion*/


static boolean safetycushionhook (long *ctbytesneeded) {
	
	if (!flholdsafetycushion && (hsafetycushion != nil)) {
		
		*ctbytesneeded -= safetycushionsize;
		
		DisposeHandle (hsafetycushion);
		
		hsafetycushion = nil;
		}
	
	return (true); /*call remaining hooks*/
	} /*safetycushionhook*/

#if (MEMTRACKER == 1)
	static Handle debuggetnewhandle (char * filename, unsigned long linenumber, unsigned long threadid, long ctbytes, boolean fltemp) {
		
		/*
		2.1b3 dmb: new fltemp parameter. if true, try temp memory first, then 
		our heap.
		*/
		
		Handle h;
		OSErr err;
		long extrasize;

		extrasize = strlen(filename) + 1 + sizeof(long) + sizeof(long) + sizeof(long) + (2 * (sizeof(Handle))) + 4;

			if (fltemp) { /*try grabbing temp memory first*/
				
				h = TempNewHandle (ctbytes + extrasize, &err);
				
				if (h != nil) {
					
					debugaddmemhandle (h, ctbytes, filename, linenumber, threadid);

					#ifdef fldebug
					
					++cttemphandles;
					
					tempzone = HandleZone (h);
					
					#endif
					
					return (h);
					}
				}
		
		if (hsafetycushion == nil) { /*don't allocate new stuff w/out safety cushion*/
			
			if (!getsafetycushion ())
				return (nil);
			}
		
		flholdsafetycushion = true;
		
			h = NewHandle (ctbytes);

			if (h != nil)
				debugaddmemhandle (h, ctbytes, filename, linenumber, threadid);
		
		flholdsafetycushion = false;
		
		return (h);
		} /*getnewhandle*/

#else
	static Handle getnewhandle (long ctbytes, boolean fltemp) {
		
		/*
		2.1b3 dmb: new fltemp parameter. if true, try temp memory first, then 
		our heap.
		*/
		
		Handle h;
		OSErr err;

			if (fltemp) { /*try grabbing temp memory first*/
				
				h = TempNewHandle (ctbytes, &err);
				
				if (h != nil) {
					
					#ifdef fldebug
					
					++cttemphandles;
					
					tempzone = HandleZone (h);
					
					#endif
					
					return (h);
					}
				}
		
		if (hsafetycushion == nil) { /*don't allocate new stuff w/out safety cushion*/
			
			if (!getsafetycushion ())
				return (nil);
			}
		
		flholdsafetycushion = true;
		
		h = NewHandle (ctbytes);
		
		flholdsafetycushion = false;
		
		return (h);
		} /*getnewhandle*/
#endif

static boolean resizehandle (Handle hresize, long size) {
	
	register Handle h = hresize;
	
	if (size > gethandlesize (h)) {
		
		if (hsafetycushion == nil) { /*don't allocate new stuff w/out safety cushion*/
			
			if (!getsafetycushion ())
				return (false);
			}
		}
	
	flholdsafetycushion = true;
	
	SetHandleSize (h, size);
	
	flholdsafetycushion = false;
	
	return (MemError () == noErr);
	} /*resizehandle*/


/*
heapmess (void) {
	
	Handle ray [1000];
	short ixray = 0;
	Handle h;
	short i;
	
	while (true) {
		
		h = NewHandle (1024L);
		
		if (h == nil)
			break;
		
		ray [ixray++] = h;
		} /*while%/
	
	for (i = 0; i < ixray; i++)
		disposehandle (ray [i]);
	} /*heapmess*/

/*
boolean analyzeheap (void) {
	
	/*
	get some statistics about the heap.  doesn't work in 32-bit mode.
	%/
	
	register byte *pblock;
	register long size;
	register byte *plimit;
	static long cthandles;
	static long ctbytes;
	static long avghandlesize;
	
	pblock = (byte *) &(*TheZone).heapData; /*point for first block in heap zone%/
	
	plimit = (byte *) (*TheZone).bkLim;
	
	cthandles = 0;
	
	ctbytes = 0; /*logical size of each handle%/
	
	while (pblock < plimit) {
		
		size = *(long *)pblock & 0x00ffffff;
		
		if (*pblock & 0x80) { /*a relocateable block%/
			
			ctbytes += size; /*add physical size%/
			
			/*ctbytes -= 8 + (*pblock & 0x0f); /*subtract header & size correction%/
			
			++cthandles;
			}
		
		pblock += size;
		}
	
	avghandlesize = ctbytes / cthandles;
	} /*analyzeheap*/


boolean haveheapspace (long size) {
	
	/*
	see if there's enough space in the heap to allocate a handle of the given size.
	
	if not, return false, but don't generate an error.
	*/
	
	Handle h;

	#if (MEMTRACKER == 1)
		h = debuggetnewhandle (__FILE__, __LINE__, GetCurrentThreadId(), size, false);
	#else
		h = getnewhandle (size, false);
	#endif

	if (h == nil)
		return (false);
	
	DisposeHandle (h);
	
	return (true);
	} /*haveheapspace*/


boolean testheapspace (long size) {
	
	/*
	see if there's enough space in the heap to allocate a handle of the given size.
	
	if not, generate an error, and return false.
	*/
	
	if (haveheapspace (size))
		return (true);
	
	memoryerror ();
	
	return (false);
	} /*testheapspace*/


void lockhandle (Handle h) {
	
	HLock (h);
	} /*lockhandle*/


void unlockhandle (Handle h) {
	
	HUnlock (h);
	} /*unlockhandle*/


boolean validhandle (Handle h) {
	
	if (h == nil)
		return (true);
	

	if (GetHandleSize (h) < 0) /*negative length never valid*/
		return (false);
	
	return (MemError () == noErr);

	} /*validhandle*/


#if (MEMTRACKER == 1)
	boolean debugnewhandle (char * filename, unsigned long linenumber, unsigned long threadid, long size, Handle *h) {
		
		*h = debuggetnewhandle (filename, linenumber, threadid, size, false);
		
		if (*h == nil) {
			
			memoryerror ();
			
			return (false);
			}
		
		return (true);
		} /*newhandle*/


	boolean debugnewemptyhandle (char * filename, unsigned long linenumber, unsigned long threadid, Handle *h) {
		
		return (debugnewhandle (filename, linenumber, threadid, (long) 0, h));
		} /*newemptyhandle*/
#else
	boolean newhandle (long size, Handle *h) {
		
		*h = getnewhandle (size, false);
		
		if (*h == nil) {
			
			memoryerror ();
			
			return (false);
			}
		
		return (true);
		} /*newhandle*/


	boolean newemptyhandle (Handle *h) {
		
		return (newhandle ((long) 0, h));
		} /*newemptyhandle*/
#endif

void disposehandle (Handle h) {
	
	if (h != nil) {
		

		#if (MEMTRACKER == 1)
			debugremovememhandle(h);
		#endif

		#ifdef fldebug

		if (HandleZone (h) == tempzone)
			--cttemphandles;

		#endif
		
		DisposeHandle (h);
		
		#ifdef fldebug
		
		if (MemError ())
			 memoryerror ();
		
		#endif
		}
	} /*disposehandle*/


long gethandlesize (Handle h) {
	
	if (h == nil)
		return (0L);
	
	return (GetHandleSize (h));
	} /*gethandlesize*/


boolean sethandlesize (Handle h, long size) {
	
	if (h == nil) /*defensive driving*/
		return (false);
	
	if (!resizehandle (h, size)) {
		
		memoryerror ();
		
		return (false);
		}
	
	return (true);
	} /*sethandlesize*/
	
	
boolean minhandlesize (Handle h, long size) {
	
	if (gethandlesize (h) >= size) /*already big enough*/
		return (true);
		
	return (sethandlesize (h, size)); /*try to make it larger*/
	} /*minhandlesize*/


void moveleft (ptrvoid psource, ptrvoid pdest, long length) {
	
	/*
	do a mass memory move with the left edge leading.  good for closing
