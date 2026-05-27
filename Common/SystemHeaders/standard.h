
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

/*
standard.h -- standard types and constants
*/

/* 2025-12-06 Codex: Raise longinfinity to 64-bit max to align with modern numeric plan. */

#ifndef standardinclude
#define standardinclude /*so other modules can tell that we've been included*/


	#if defined(FRONTIER_PORTABLE) || defined(FRONTIER_HEADLESS)
	#include "../../portable/time_portable.h"
	/* Ensure portable Mac types (Rect, RgnHandle, etc.) are defined before use below */
	#ifndef OSINCLUDES_PORTABLE_H
	#include "../headers/osincludes_portable.h"
	#endif
	/* Include portable standard macros to prevent redefinition below */
	#ifndef FRONTIER_STANDARD_PORTABLE_H
	#include "../../portable/standard_portable.h"
	#endif
	#else
	#include "FastTimes.h"
	#endif

#include "stringdefs.h"		/* embedded string definitions */

#ifndef appletdefsinclude

#define appletdefsinclude

#ifdef MPWC

	#define SystemSevenOrLater 1
	/* A/UX is case sensitive, so use correct case for include file names */
	#include <values.h>
	#include <types.h>
	#include <quickdraw.h>
	#include <fonts.h>
	#include <events.h>
	#include <controls.h>
	#include <windows.h>
	#include <menus.h>
	#include <textedit.h>
	#include <resources.h>
	#include <dialogs.h>
	#include <desk.h>
	#include <scrap.h>
	#include <toolutils.h>
	//MW must add LowMem for certain functions (see notes).
	#include <LowMem.h>
	#include <segload.h>
	#include <memory.h>
	#include <packages.h>

#endif



/*constants*/

#ifndef ctdirections
	
	typedef enum tydirection {
		
		nodirection = 0, 
		
		up = 1, 
		
		down = 2, 
		
		left = 3,
		
		right = 4, 
		
		flatup = 5, 
		
		flatdown = 6, 
		
		sorted = 8,
		
		pageup = 9,
		
		pagedown = 10,
		
		pageleft = 11,
		
		pageright = 12
		} tydirection;
	
	#define ctdirections 12 /*for arrays indexed on directions*/

#endif

#ifndef FRONTIER_PORTABLE_DEFINED_TYBITDIRECTION
typedef enum tybitdirection {

	upbit = 0x01,

	downbit = 0x02,

	leftbit = 0x04,

	rightbit = 0x08
	} tybitdirection;
#define FRONTIER_PORTABLE_DEFINED_TYBITDIRECTION 1
#endif

#ifndef FRONTIER_PORTABLE_DEFINED_TYLINESPACING
typedef enum tylinespacing {

	singlespaced = 1,

	oneandalittlespaced = 2,

	oneandaquarterspaced = 3,

	oneandahalfspaced = 4,

	doublespaced = 5,

	triplespaced = 6
	} tylinespacing;
#define FRONTIER_PORTABLE_DEFINED_TYLINESPACING 1
#endif

#ifndef FRONTIER_PORTABLE_DEFINED_TYJUSTIFICATION
typedef enum tyjustification {

	leftjustified,

	centerjustified,

	rightjustified,

	fulljustified,

	unknownjustification
	} tyjustification;
#define FRONTIER_PORTABLE_DEFINED_TYJUSTIFICATION 1
#endif


#ifndef FRONTIER_STANDARD_MACROS_DEFINED
#define FRONTIER_STANDARD_MACROS_DEFINED

#define true 1
#define false 0

#define infinity 32767
#define longinfinity ((long)0x7FFFFFFFFFFFFFFFLL)
#define intinfinity 32767
#define intminusinfinity -32768

#define emptystring (ptrstring) "\0"

#define chnul			((char) 0)
#define chbacktab		((char) 0)
#define chhome 			((char) 1)
#define chenter			((char) 3)
#define chend 			((char) 4)
#define chhelp 			((char) 5)
#define chbackspace		((char) 8) 
#define chtab 			((char) 9)
#define chlinefeed		((char) 10)
#define chpageup 		((char) 11)
#define chpagedown 		((char) 12)
#define chreturn		((char) 13)
#define chescape		((char) 27)
#define chrightarrow 	((char) 29)
#define chleftarrow 	((char) 28)
#define chuparrow 		((char) 30)
#define chdownarrow 	((char) 31)
#define chsinglequote 	((char) 39)
#define chdoublequote 	((char) 34)
#define chspace			((char) 32)
#define chdelete 		((char) 127)

	#define chcomment			((byte) 0xc7)	/* '�' */
	#define chendcomment		((byte) 0xc8)	/* '�' */
	#define chopencurlyquote	((byte) 0xd2)	/* '�' */
	#define chclosecurlyquote	((byte) 0xd3)	/* '�' */
	#define chtrademark			((byte) 0xaa)	/* '�' */
	#define chnotequals			((byte) 0xad)	/* '�' */
	#define chdivide			((byte) 0xd6)	/* '�' */




#define lenbigstring 255
   
#define sizegrowicon 15 /*it's square, this is the length of each side*/
 
#define dragscreenmargin 4 /*for dragging windows, leave this many pixels on all sides*/

#define doctitlebarheight 18 /*number of pixels in the title bar of each standard window*/

/*#define nil 0L*/

/*#define private static */ /*use "private" when a value or routine should not be seen outside the file it appears in*/


/*types*/

#define bigstring Str255

#ifndef __RPCNDR_H__

	typedef unsigned char boolean;

#endif

typedef unsigned char *ptrstring, **hdlstring;

typedef void * ptrvoid;

typedef char *ptrchar;

typedef short *ptrint;

typedef RgnHandle hdlregion;

typedef const Rect *rectparam;

typedef Rect *ptrrect;

typedef boolean (*callback) (void); /* 2004-10-24 aradke: was ... instead of void on Mac */

#if defined(__RPCNDR_H_VERSION__)
typedef	unsigned char *ptrbyte;	/* 2004-12-29 trt: byte defined by Win32 rpcndr.h */
#else
typedef	unsigned char byte, *ptrbyte;	
#endif


/*macros*/

#undef verify

#ifdef NDEBUG
	#define verify(x)	((void) x)
#else
	#define verify(x)	assert(x)
#endif

	#define sysbeep() SysBeep(1) 


#ifndef abs
	#define abs(x) ((x) < 0? -(x) : (x))
#endif

#define odd(x) ((x) & 0x0001)

#define even(x) (!odd (x))

#ifndef max
#define max(x,y) ((x) > (y)? (x) : (y))
#endif

#ifndef min
#define min(x,y) ((x) < (y)? (x) : (y))
#endif

#define sgn(x) ((x) < 0? -1 : ((x) > 0? 1 : 0))

#define loword(x) ((x) & 0x0000ffff)
#define hiword(x) ((x) >> 16)
#define makelong(lo, hi) ((hi) << 16 | (lo))
#define diskwordstomemlong(lo, hi) makelong(conditionalshortswap(lo), conditionalshortswap(hi))
#define memlongtodiskwords(x, lo, hi) do { \
							lo = conditionalshortswap (loword (x)); \
							hi = conditionalshortswap (hiword (x));} while (0)


#define stringbaseaddress(bs) (bs+1)
#define setstringlength(bs,len) (bs[0]=(char)(len))
#define	stringlength(bs) ((unsigned char)(bs)[0])
#define setstringwithchar(ch,bs) {bs[0]=1;bs[1]=(ch);}
#define getstringcharacter(bs,pos) bs[(pos)+1]
#define setstringcharacter(bs,pos,ch) {bs[(pos)+1] = (ch);}
#define stringsize(bs) (stringlength (bs) + 1)
#define lastchar(bs) (bs [stringlength (bs)])


#define BIGSTRING(s) ((unsigned char *)(s))

#endif /* FRONTIER_STANDARD_MACROS_DEFINED */

#define setemptystring(bs) (setstringlength(bs,0))

#define isemptystring(bs) (stringlength(bs)==0)

#define isemptyrect(r) (((r).bottom <= (r).top) || ((r).right <= (r).left))

#define bitboolean(fl) ((fl)?true:false)

#define ouchreturn {ouch ();return(false);}

#define optiondebug {if(optionkeydown())Debugger();}

#define longsizeof(x) (long)sizeof(x)

#define bundle /**/

#define gettickcount() TickCount()

typedef short hdlfilenum;

typedef Pattern xppattern;


#define isnumeric(x) ((x >= '0') && (x <= '9'))

#define quickdrawglobal(x) qd.x

extern boolean flcominitialized; /* set up in lang.c */

/* Returns true if env var `name` is set to a value other than "" or "0".
 * Use for boolean flag-style env vars: empty / "0" / unset are all false.
 *
 * `static inline` so it works in any TU that includes standard.h without
 * needing a dedicated env_util.c and matching makefile entries in both the
 * CLI and tests builds.
 *
 * Note: FRONTIER_OPEN_READONLY (main.c::hydrate_system_root_database) uses
 * any-value-is-on semantics (even "0" enables it) for pre-existing
 * back-compat reasons -- do not retrofit env_truthy() onto that call site
 * without a deliberate semantic-change decision. */
static inline boolean env_truthy(const char *name) {
	const char *v = getenv(name);
	return (boolean)(v != NULL && v[0] != '\0' && strcmp(v, "0") != 0);
}

#endif

#endif


