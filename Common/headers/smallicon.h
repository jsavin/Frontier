
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

#ifndef smalliconinclude
#define smalliconinclude


#define widthsmallicon 16 /*dimensions of a SICN resource*/
#define heightsmallicon 16

#define miscsmalliconlist 131 /*miscellaneous small icons*/
#define lockicon 0
#define checkedicon 1
#define upflagicon 2
#define downflagicon 3
#define sideflagicon 4
#define closeboxicon 5
#define selectedcloseboxicon 6
#define stophereicon 7
#define blackpopupicon 8
#define graypopupicon 9

	#define moofsmalliconlist 142



typedef enum tyfillstate {
	
	filledwithblack, 
	
	filledwithgray, 
	
	filledwithwhite
	} tyfillstate;


typedef char smalliconbits [32];

typedef smalliconbits *ptrsmalliconbits, **hdlsmalliconbits;

#pragma pack(2)
typedef struct tysmalliconspec { /*bundle all the plotting parameters into a record*/
	
	hdlsmalliconbits hbits; /*if nil, use iconlist*/
	
	short iconlist;
	
	short iconnum;
	
	WindowPtr iconwindow;
	
	Rect iconrect;
	
	boolean flinverted: 1;
	
	boolean flclearwhatsthere: 1;
	} tysmalliconspec;
#pragma options align=reset

/*prototypes*/

extern boolean displaypopupicon (Rect, boolean);

extern boolean displayleadericon (Rect, tyfillstate);

extern boolean plotsmallicon (tysmalliconspec);

extern boolean loadsmallicon (short, hdlsmalliconbits *);

extern boolean initsmallicons (void);

extern boolean myMoof (short, long);

#endif


