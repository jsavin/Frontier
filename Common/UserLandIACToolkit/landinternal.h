
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

#ifndef landinclude

	#include <land.h>
	
#endif


#define fljustfrontier

#ifndef standardinclude

	#include <standard.h>
 
#endif
 
#ifndef standardinclude
 
	#ifdef fldebug

		short __assert(char *, char *, short);
		#define assert(x)	((void) ((x) ? 0 : __assert(#x, __FILE__, __LINE__)))

	#else

		#define assert(x)	((void) 0)

	#endif


	#define true 1

	#define false 0

	#define shortinfinity 32767

	#define shortminusinfinity -32768

	#define longinfinity (long)0x7FFFFFFF

	#define	stringlength(bs) ((bs)[0])

	#define setstringlength(bs,len) (bs[0]=(char)len)

	#define setemptystring(bs) (setstringlength(bs,0))

	#define bitboolean(fl) ((fl)?true:false)

	#define longsizeof(x) (long)sizeof(x)

	#define lenbigstring 255

	#define bigstring Str255
	   
	#define boolean Boolean	

	typedef	unsigned char byte, *ptrbyte;	


	#ifdef MPWC
		#define quickdrawglobal(x) qd.x
	#endif
	
	//Code change by Timothy Paustian Sunday, May 7, 2000 11:28:10 PM
	//Changed to Opaque call for Carbon
	#ifdef TARGET_CARBON
		#define quickdrawglobal(x)	GetQDGlobalsThePort()
		//the rest will have to be ifdef out.
	#endif
	#ifdef THINKC

		#define quickdrawglobal(x) x

	#endif


	typedef void *ptrvoid;

#endif

	#define idnull 0L /*a message sent to no one*/

	#define idnet -1L /*a message sent across the network*/

	#define nullclass 0L /*don't-care value for class*/

	#define nulltoken 0L /*a token no one handles*/

	#define returntoken 0L /*null token used to flag return verb*/

/*prototypes*/

extern pascal hdllandglobals landgetlandglobals (void);

extern pascal void landsetlandglobals (hdllandglobals);

extern pascal boolean landmenuscriptcompleted (void);


//extern landdisposeallparams (hdlverbrecord); /*land.c*/

extern boolean landhandleverb (hdlverbrecord);



extern boolean landpushqueue (Handle); /*landqueue.c*/

extern boolean landpopqueue (Handle *);

extern boolean landpopqueueitem (landqueuepopcallback, long, Handle *);

extern boolean landemptyqueue (void);


extern boolean landsendreturnvalues (hdlverbrecord); /*landreturn.c*/


extern boolean landsystem6apprunning (tyapplicationid); /*landsystem6.c*/

extern boolean landsystem6newverb (hdlverbrecord);

extern boolean landsystem6send (hdlverbrecord, hdlverbrecord *);

extern boolean landsystem6returnmultiple (hdlverbrecord);

extern boolean landsystem6eventfilter (EventRecord *);

extern boolean landsystem6close (void);

extern boolean landsystem6init (void);

extern boolean landsystem6setapplicationid (tyapplicationid);


extern boolean landsystem7apprunning (tyapplicationid); /*landsystem7.c*/

extern boolean landsystem7newverb (hdlverbrecord, ProcessSerialNumber *);

extern boolean landsystem7newnetworkverb (hdlverbrecord, tynetworkaddress *);

extern boolean landsystem7send (hdlverbrecord, hdlverbrecord *);

extern boolean landsystem7returnmultiple (hdlverbrecord);

extern boolean landsystem7eventfilter (EventRecord *);

extern boolean landsystem7close (void);

extern boolean landsystem7addclass (tyverbclass);

extern boolean landsystem7addfastverb (tyverbclass, tyverbtoken);

extern boolean landsystem7acceptanyverb (void);

extern void landsystem7disposeverb (hdlverbrecord);

extern boolean landsystem7init (void);


extern boolean landbreakembrace (EventRecord *); /*landops.c*/
	
extern boolean landscalartype (typaramtype);

extern boolean landdescparamtype (typaramtype);

extern boolean landgetappcreator (OSType *);

#ifndef fljustfrontier

	extern boolean landlockhandle (Handle);
	
	extern boolean landunlockhandle (Handle);
	
	extern landmoveleft (void *, void *, long);	
	
	extern landclearbytes (void *, long);
	
	extern land4bytestostring (long, bigstring);
	
	extern landsetcursortype (short);
	
	extern landcopystring (bigstring, bigstring);
	
	extern landcopyheapstring (hdlbigstring, bigstring);
	
	extern boolean landpushstring (bigstring, bigstring);
	
	extern boolean landpushlong (long, bigstring);
	
	extern boolean landequalstrings (bigstring, bigstring);
	
	extern boolean landnewfilledhandle (void *, long, Handle *);
	
	extern landdisposehandle (Handle);
	
	extern hdlbigstring landnewstring (bigstring);
	
	extern boolean landenlargehandle (Handle, long, void *);
	
	extern boolean landshrinkhandle (Handle, long);
	
	extern boolean landloadfromhandle (Handle, long *, long, void *);
	
	extern boolean landloadfromhandletohandle (Handle, long *, long, Handle *);
	
	extern boolean landcopyhandle (Handle, Handle *);
	
	extern boolean landnewclearhandle (long, Handle *);
	
	extern long landgethandlesize (Handle);
	
	extern boolean landnewemptyhandle (Handle *);
	
	extern boolean landsurrenderprocessor (EventRecord *);

#else

	#include "memory.h"
	#include "strings.h"
	
	#define landlockhandle(x)			lockhandle (x)
	
	#define landunlockhandle(x)			unlockhandle (x)
	
	#define landmoveleft(p1, p2, n)		moveleft (p1, p2, n)
	
	#define landclearbytes(p1, n)		clearbytes (p1, n)
	
	#define land4bytestostring(l, s)	ostypetostring((OSType) l, s)
	
	#define landcopystring(s1, s2)		copystring (s1, s2)
	
	#define landcopyheapstring(h, s)	copyheapstring (h, s)
	
	#define landpushstring(s1, s2)		pushstring (s1, s2)
	
	#define landpushlong(l, s)			pushlong (l, s)
	
	#define landequalstrings(s1, s2)	equalstrings (s1, s2)
	
	#define landnewfilledhandle(p, n, x)	newfilledhandle (p, n, x)
	
	#define landdisposehandle(x)		disposehandle (x)
	
	#define landenlargehandle(x, n, p)	enlargehandle(x, n, p)
	
	#define landloadfromhandle(x, ix, ct, p)	loadfromhandle(x, ix, ct, p)
	
	#define landloadfromhandletohandle(x, ix, ct, h)	loadfromhandletohandle(x, ix, ct, h)
	
	#define landcopyhandle(x, h)		copyhandle (x, h)
	
	#define landnewclearhandle(n, x)	newclearhandle(n, x)
	
	#define landgethandlesize(x)		gethandlesize(x)
	
	#define landnewemptyhandle(x)		newemptyhandle(x)

#endif


extern boolean landverbsupported (tyverbclass, tyverbtoken); /*landverbarray.c*/


extern boolean landsetstatsinfo (hdlverbrecord, boolean); /*landwindow.c*/

extern boolean landstatswindowisopen (void);

// extern landsetmemstats (void);

extern boolean landwindoweventfilter (EventRecord *);


