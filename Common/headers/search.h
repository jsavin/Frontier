
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

#define searchinclude /*so other includes can tell if we've been loaded*/




/*global search/sort settings*/

#pragma pack(2)
typedef struct tysearchparameters {
	
	boolean flfirsttime: 1;
	
	boolean flunicase: 1;
	
	boolean flwraparound: 1;
	
	boolean flwholewords: 1;
	
	boolean flonelevel: 1;
	
	boolean flonetype: 1;
	
	boolean floneobject: 1;
	
	boolean flclosebehind: 1;
	
	boolean flfindall: 1;
	
	boolean flreplaceall: 1;
	
	boolean flregexp: 1;
	
	boolean flzoomfound: 1;
	
	boolean flwindowzoomed: 1;
	
	bigstring bsorigfind; /*as entered by the user*/
	
	bigstring bsorigreplace; /*as entered by the user*/
	
	bigstring bsfind; /*corrected for case-sensitivity etc.*/
	
	bigstring bsreplace; /*depends on current match for regexps*/
	
	short ctfound;
	
	short ctreplaced;
	
	long searchrefcon;

	
	Handle hcompiledpattern;
	
	Handle hovector;

	} tysearchparameters;
#pragma options align=reset

extern tysearchparameters searchparams; /*set this global to influence searching process*/



/*prototypes*/

extern boolean isword (byte *, long, long, long); /*search.c*/

extern boolean textsearch (byte *, long, long *, long *);

extern boolean handlesearch (Handle, long *, long *);

extern boolean stringsearch (bigstring, short *, short *);

// extern void getsearchstring (bigstring);

extern void startnewsearch (boolean, boolean);

extern boolean startingtosearch (long);

extern boolean searchshouldwrap (long);

extern boolean searchshouldcontinue (long);

extern void endcurrentsearch (void);

extern boolean initsearch (void);


extern boolean getsearchparams (void); /*shellverbs.c*/

extern boolean setsearchparams (void);




