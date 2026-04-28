
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

/* 2025-12-07 Codex: Modernize numeric/string helpers for 64-bit values. */

#include "frontier.h"
#include "standard.h"

#if !defined(FRONTIER_HEADLESS)
	#include "mac.h"
#endif

#include "error.h"
#include "memory.h"
#include "strings.h"
#include "ops.h"
#include "langinternal.h"
#include "shell.h"
#include "sysshellcall.h" // 2007-06-30 creedon

#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static tydirection directions [ctdirections] = {
	
	nodirection,
	
	up,
	
	down,
	
	left,
	
	right,
	
	flatup,
	
	flatdown,
	
	sorted,
	
	pageup,
	
	pagedown,
	
	pageleft,
	
	pageright
	};



short minint (short x, short y) {
	
	return (x <= y? x : y);
	} /*minint*/


short maxint (short x, short y) {

	return (x >= y? x : y);
	} /*maxint*/


short absint (short x) {

	return (x >= 0? x : -x);
	} /*absint*/


boolean delayticks (long ct) {
	
	/*
	6/27/91 dmb: we really want to background effectively during a delay.  
	we could add a high-level backgroundtask to replace the logic contained 
	here; let's see how long this implementation lasts first.
	
	7/27/92 dmb: set & clear scriptresting
	
	1/7/93 dmb: don't call eventavail
	
	1/18/93 dmb: langbackgroundtask now takes flresting parameter; don't set global
	
	2/19/93 dmb: take long parameter. don't ignore backgroundtask result.
	*/
	
	register unsigned long tc;
	tc = gettickcount () + ct;
	
	while (gettickcount () < tc) {
		
		if (ct >= 10) { /*for non-tiny delays, allow backgrounding*/
			
			boolean fl;
			
			if (flscriptrunning)
				fl = langbackgroundtask (true);
			else
				fl = shellbackgroundtask ();
			
			if (!fl)
				return (false);
			}
		} /*while*/
	
	return (true);
	} /*delayticks*/


boolean delayseconds (long ct) {
	
	/*
	3/19/93 dmb: recoded to avoid overflow on large ct
	*/
	
	register long ctloops = 1;
	register long ctticks = ct;
	
	if (ct > 0x00010000)
		ctloops = 60;
	else
		ctticks *= 60;
	
	while (--ctloops >= 0)
		if (!delayticks (ctticks))
			return (false);
	
	return (true);
	} /*delayseconds*/



void counttickloops (long *ctloops) {
	
	/*
	do a research project that burns up to 2 ticks real-time.
	
	sit in the tightest loop possible, watching the tick counter, and return the
	number of loops it took to do a tick.
	*/
	
	register unsigned long tc;
	long ct = 0;
	
	tc = gettickcount ();
	
	while (gettickcount () == tc) {} /*wait for a tick "boundary"*/
	
	tc = gettickcount (); 
	
	while (gettickcount () == tc) 
		ct++;
	
	*ctloops = ct;
	} /*counttickloops*/


void burntickloops (long ctloops) {
	
	/*
	assume the caller did a research project with counttickloops.  burn that many
	loops real-time.
	
	the purpose?  allows you to delay for fractions of a tick.
	
	important: our loop must have a call to gettickcount because the research loop
	had to do it.
	*/
	
	register long ct = ctloops;
	register long i = 1;
	
	while (i <= ct) {
		
		gettickcount (); /*mimic performance of research project loop*/
		
		i++;
		} /*while*/
	} /*burntickloops*/



unsigned char uppercasechar (unsigned char ch) {
	
	register char c = ch;
	
	if ((c >= 'a') && (c <= 'z'))
		c -= 32;
		
	return (c);
	} /*uppercasechar*/
	
	
unsigned char lowercasechar (unsigned char ch) {
	
	register char c = ch;
	
	if ((c >= 'A') && (c <= 'Z'))
		c += 32;
		
	return (c);
	} /*lowercasechar*/


boolean textchar (unsigned char chcheck) {
	
	register unsigned char ch = chcheck;
	
	return (ch >= ' '  &&  ch != chdelete);

	/*
	return (((ch >= 'a') && (ch <= 'z')) || ((ch >= 'A') && (ch <= 'Z')));
	*/
	} /*textchar*/
	
	
void shorttostring (short shortval, bigstring bs) {
	
	numbertostring ((long) shortval, bs);
	} /*shorttostring*/


void numbertostring (long longval, bigstring bs) {
	
	char buffer [32];
	int len = snprintf (buffer, sizeof (buffer), "%lld", (long long) longval);
	
	if (len < 0)
		len = 0;
	if (len > 255)
		len = 255;
	
	bs [0] = (unsigned char) len;
	
	if (len > 0)
		memmove (&bs [1], buffer, (size_t) len);

	} /*numbertostring*/


boolean stringtonumber (bigstring bs, long *longval) {
	
	/*
	fill in job needed: do error checking and return false if there are invalid
	characters in the string.  right now we always return true.
	*/
	
	register short i;
	
	for (i = 1; i <= stringlength (bs); ++i) {
		
		register byte ch = bs [i];
		
		if (!isdigit (ch)) {
			
			if ((i > 1) || (ch != '+' && ch != '-')) { /*not a leading +/-*/
				
				setstringlength (bs, i - 1);
				
				break;
				}
			}
		}
	
	if (isemptystring (bs)) {
		*longval = 0;
		}
	else {	
			char buffer [256];
			short len = stringlength (bs);
			
			if (len > 255)
				len = 255;
			
			for (i = 0; i < len; ++i)
				buffer [i] = (char) bs [i + 1];
			
			buffer [len] = '\0';
			
			*longval = (long) strtoll (buffer, NULL, 10);
		}
	
	return (true);
	} /*stringtonumber*/


boolean stringtoshort (bigstring bs, short *shortval) {
	
	/*
	fill in job needed: do error checking and return false if there are invalid
	characters in the string.  right now we always return true.
	*/
	
	long longval;
	
	stringtonumber (bs, &longval);
	
	*shortval = (short) longval;
	
	return (true);
	} /*stringtoshort*/



#if defined(FRONTIER_HEADLESS)

#include <math.h>
#include <stdlib.h>
#include <stdio.h>

#ifndef double_t
typedef double double_t;
#endif

typedef struct headless_decform {
	short style;
	short digits;
} decform;

#define DecForm decform

enum { FIXEDDECIMAL = 0 };

static double_t str2num (byte *s) {
	char buffer[256];
	short len = stringlength (s);
	if (len <= 0)
		return 0.0;
	if (len > 255)
		len = 255;
	for (short i = 0; i < len; ++i)
		buffer[i] = (char) s[i + 1];
	buffer[len] = '\0';
	return strtod (buffer, NULL);
} /*str2num*/

static void num2str (decform *f, double_t x, void *s) {
	unsigned char *bs = (unsigned char *) s;
	char buffer[256];
	int digits = (f != NULL && f->digits > 0) ? f->digits : 8;
	if (digits > 16)
		digits = 16;
	int n = snprintf (buffer, sizeof buffer, "%.*f", digits, x);
	if (n < 0)
		n = 0;
	if (n > 255)
		n = 255;
	for (int i = n - 1; i >= 0; --i) {
		if (buffer[i] == '0' && i > 0 && buffer[i - 1] != '.')
			buffer[i] = '\0';
		else
			break;
	}
	/* remove potential trailing '.' */
	int newlen = (int) strlen (buffer);
	if (newlen > 0 && buffer[newlen - 1] == '.') {
		buffer[newlen - 1] = '\0';
		--newlen;
	}
	if (newlen == 0) {
		buffer[0] = '0';
		buffer[1] = '\0';
		newlen = 1;
	}
	bs[0] = (unsigned char) newlen;
	for (int i = 0; i < newlen; ++i)
		bs[i + 1] = (unsigned char) buffer[i];
} /*num2str*/

#else /* !FRONTIER_HEADLESS */

	#define Decimal decimal
	#define DecForm decform


	/*
	3.0.4 dmb: make these work with Metrowerks fp.h
	*/

	static double_t str2num (byte *s) {
		
		/*
		3.0.4b5 dmb: must null-terminate input string
		*/
		
		short n = 1;
		short p = false;
		decimal d;
		
		if (isemptystring (s))
			return 0.0;
		
		s [stringlength(s) + 1] = chnul;	/*now it's a c and p string*/
		
		str2dec ((char *) s, &n, &d, &p);
		
		return(dec2num(&d));
		} /*str2num*/

	static void num2str (decform *f, double_t x, void *s) {
		
		decimal d;
		
		num2dec (f, x, &d);
		dec2str (f, &d, s);
		
		convertcstring (s);
		} /*num2str*/

#endif /* FRONTIER_HEADLESS */
	
	
	
	boolean stringtofloat (bigstring bs, double *pfloat) {
		
		/*
		8/7/92 dmb: use SANE routines
		
		9/22/92 dmb: added validity checking
		*/
		
		bigstring bstest;
		
		nthword (bs, 1, '.', bstest);
		
		if (!isallnumeric (bstest))
			return (false);
		
		nthword (bs, 2, '.', bstest);
		
		if (!isallnumeric (bstest))
			return (false);
		
		*pfloat = str2num (bs);
		
		return (true);
		} /*stringtofloat*/
	
	
	boolean floattostring (double num, bigstring bs) {
		
		/*
		10/21/91 dmb: handle doubles bigger than the max long value.  our 
		technique loses some precision, but it works pretty well without 
		bringing in any flating point libraries.
		
		8/7/92 dmb: use SANE routines
		*/
		
		DecForm form;
		
		form.style = FIXEDDECIMAL;
		
		form.digits = 8;
		
		num2str (&form, num, bs);
		
		while (bs [*bs] == '0')
			--*bs;
		
		if (bs [*bs] == '.')
			pushchar ('0', bs);
		
		return (true);
		} /*floattostring*/


long numberfromhandle (register Handle x) {
	
	/*
	return a numeric interpretation of the handle.
	*/
	
	register long ctbytes;
	register long n;
	
	ctbytes = gethandlesize (x);
	
	switch (ctbytes) {
		
		case 0:
			n = 0;
		
			break;
		
		case 1:
			n = **(char **) x;
			
			break;
		
		case 2:
			n = **(short **) x;
			
			break;
		
		default:
			n = **(long **) x;
			
			break;
		}
	
	return (n);
	} /*numberfromhandle*/


void exittooperatingsystem (void) {
	
	/*
	unconditional exit to the operating system.
	*/
	
#if defined(FRONTIER_HEADLESS)
	exit (0);
#else
	ExitToShell ();
#endif

	} /*exittooperatingsystem*/
	
	
short dirtoindex (tydirection dir) {
	
	register short ix;
	
	for (ix = 0;  ix < ctdirections;  ++ix)
		if (directions [ix] == dir)
			return (ix);
	
	return (-1); /*not a valid direction*/
	} /*dirtoindex*/


tydirection indextodir (short ix) {
	
	return (directions [ix]);
	} /*indextodir*/


boolean validdirection (tydirection dir) {
	
	return (dirtoindex (dir) >= 0);
	} /*validdirection*/


tydirection oppositdirection (tydirection dir) {
	
	/*
	5.0a25 dmb: pulled this out of oprepeatedbump; could be used many
	other places, like when scrolling
	*/

	switch (dir) {
		
		case flatup:
			dir = flatdown;
			
			break;
			
		case flatdown:
			dir = flatup;
			
			break;
			
		case left:
			dir = right;
			
			break;
			
		case right:
			dir = left;
			
			break;
			
		case up:
			dir = down;
			
			break;
			
		case down:
			dir = up;
			
			break;
		
		case pageup:
			dir = pagedown;
			
			break;
		
		case pagedown:
			dir = pageup;
			
			break;
		
		case pageright:
			dir = pageleft;
			
			break;
		
		case pageleft:
			dir = pageright;
			
			break;
		
		default:
			dir = nodirection;
			
			break;
		} /*switch*/
	
	return (dir);
	} /*oppositdirection*/
		

long divup (register long n, register long d) {
	
	/*
	divide numerator n by divisor d, rouding up
	*/
	
	if (n >= 0)
		return ((n + d - 1) / d);
	
	return ((n - d + 1) / d);
	} /*divup*/


long divround (register long n, register long d) {
	
	/*
	divide numerator n by divisor d, rouding as closely as possible
	*/
	
	if (n >= 0)
		return ((n + d / 2) / d);
	
	return ((n - d / 2) / d);
	} /*divround*/


long quantumize (register long n, register long q) {
	
	/*
	return the lowest multiple of quantum q that is greater (further from 
	zero) that n.  useful for quantumizing scroll distances
	
	9/25/92 dmb: check for zero; it's bogus, but data can be that way
	*/
	
	if (q == 0)
		return (n);
	
	return (divup (n, q) * q);
	} /*quantumize*/


boolean truenoop (void) {
	
	/*
	a noop routine for all to share.
	*/
	
	return (true);
	} /*truenoop*/


boolean falsenoop (void) {
	
	/*
	a false noop routine for all to share.
	*/
	
	return (false);
	} /*falsenoop*/


#define gestalttrap 		0xA1AD
#define unimplementedtrap	0xA09F

static boolean __attribute__((unused)) gestaltavailable (void) {

		//Code change by Timothy Paustian Friday, June 9, 2000 9:55:04 PM
		//Changed to Opaque call for Carbon
		//Gestalt is available for OS 9 and X
		return true;
	} /*gestaltavailable*/


boolean gestalt (OSType selector, long *result) {

#if defined(FRONTIER_HEADLESS)
	(void) selector;
	if (result)
		*result = 0;
	return (false);
#else
	if (!gestaltavailable ())
		return (false);

	return (Gestalt (selector, result) == noErr);
#endif

	} /*gestalt*/


boolean listlink (hdllinkedlist hlinkedlist, hdllinkedlist hinsert) {
	
	/*
	link the item into the list
	
	2.1b5 dmb: don't set hinsert.hnext to nil if it's already in the list
	*/
	
	register hdllinkedlist h = hlinkedlist;
	register hdllinkedlist hnext;
	
	if (h == nil) /*nil list*/
		return (false);
	
	while (true) {
		
		if (h == hinsert) /*it's already in the list*/
			return (true);
		
		hnext = (**h).hnext;
		
		if (hnext == nil) { /*reached last in list*/
			
			(**h).hnext = hinsert; /*push on end*/
			
			(**hinsert).hnext = nil; /*last in list*/
			
			return (true);
			}
		
		h = hnext; /*advance to next node*/
		} /*while*/
	} /*listlink*/


boolean listunlink (hdllinkedlist hlinkedlist, hdllinkedlist hdelete) {
	
	/*
	unlink the indicated item from the list
	*/
	
	register hdllinkedlist h = hlinkedlist;
	register hdllinkedlist hprev;
	register hdllinkedlist hnext;
	
	if (h == nil) /*empty list*/
		return (false);
	
	hprev = h;
	
	h = (**h).hnext;
	
	while (h != nil) {
		
		hnext = (**h).hnext;
		
		if (h == hdelete) { /*found it*/
			
			(**hprev).hnext = hnext;
			
			return (true);
			}
		
		hprev = h;
		
		h = hnext;
		} /*while*/
	
	return (false); /*didn't find it*/
	} /*listunlink*/

#if __powerc || __GNUC__

typedef struct {
    long double hi;	/*maybe 8 bytes, maybe 16*/
    double lo;		/*ignored if hi is actually 16 bytes*/
    } safelongdouble;

void safex80told ( const extended80 *x80, long double *x ) {
	
	/*
	3.0.4b3 dmb: at this point, MWC's "long doubles" are only 8 bytes. The 
	math library assumes that they're 16 bytes, so we need to set the "lo" 
	8 bytes to zero in order to avoid rounding an ununitialized data errors.
	
	thanks for John Baxter for investigating this and proposing this solution
	*/
	
		
		*x = x80tod (x80);
		
} /*safex80told*/


void safeldtox80 ( const long double *x, extended80 *x80 ) {

		double temp;
		
		temp = *x;
		
		dtox80 (&temp, x80);
	
} /*safeldtox80*/

#endif


void getsystemversionstring ( bigstring bs, bigstring bsextrainfo ) {

	
		//
		// 2007-06-29 creedon: bug fix for os version numbers like 10.4.10,
		//				   old method of using gestaltSystemVersion
		//				   doesn't work, in future could use
		//				   gestaltSystemVersionMajor, etc on 10.4 or later
		//
		
		Handle handleCommand, handleReturn;
		
		newtexthandle ( "\psw_vers -productVersion", &handleCommand );
		
		newemptyhandle ( &handleReturn );
		
		unixshellcall ( handleCommand, handleReturn );
		
		disposehandle ( handleCommand );
		
		texthandletostring ( handleReturn, bs );
		
		setstringlength ( bs, stringlength ( bs ) - 1 );
		
		disposehandle ( handleReturn );
		
		if ( bsextrainfo != NULL )
			setemptystring ( bsextrainfo );
		
	
	
	} // getsystemversionstring


void getsizestring (unsigned long size, bigstring bs) {
	
	unsigned long meg = 1024L * 1024L;
	
	if (size > meg) { /*greater than 1 megabyte*/
		
		unsigned long leftofdecimal, rightofdecimal;
		bigstring bsright;
		
		leftofdecimal = size / meg;
		
		rightofdecimal = ((size % meg) + 50) / 100;
		
		numbertostring (leftofdecimal, (byte *) bs);
		
		numbertostring (rightofdecimal, bsright);
		
		setstringlength (bsright, 1);
		
		if (bsright [1] != '0') {
		
			pushchar ('.', bs);
		
			pushstring (bsright, bs);
			}
		
		pushchar ('M', bs);
		}
	else {
		unsigned long x = size / 1024;
	
		if ((size % 1024) > 512)
			x++;
		
		if (x == 0)
			copystring (BIGSTRING ("\x05" "zero "), bs);
		else
			numbertostring (x, (byte *) bs);
			
		pushchar ('K', bs);
		}
	} /*getsizestring*/		


unsigned long bcdtolong (unsigned long bcd) { /* 2004-11-23 creedon, aradke */ 

	/*
	convert a long value from BCD notation
	*/

	unsigned long ret = 0;
	unsigned long m = 1;

	do {
		ret += m * (bcd & 0x0000000f);

		m *= 10;

		bcd >>= 4;

		} while (bcd != 0);

	return (ret);
	} /* bcdtolong */
