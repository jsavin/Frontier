
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

#define shellprintinclude /*so other includes can tell if we've been loaded*/

#pragma pack(2)
typedef struct typrintinfo {
	
	/*the margins we're enforcing to calculate paperrect*/
	Rect
	margins,
	paperrect;
	
	long
		scaleMult,
		scaleDiv;
	
	/*the number of pages in the document being printed*/
	short ctpages;
	
	
	
	GrafPtr			printport;
	PMPrintSession 	printhandle;
	PMPrintSession 	printDefaultSession;	/* kw added 2005-06 */
	PMPageFormat	pageformat;
	PMPrintSettings	printsettings;
	PMRect			pagerect;
	

	} typrintinfo;
#pragma options align=reset


extern typrintinfo shellprintinfo;


/*prototypes*/

extern boolean shellinitprint (void); /*shellprint.c*/

extern boolean shellpagesetup (void);

extern boolean shellprint (WindowPtr, boolean);

extern boolean iscurrentportprintport (void);

extern boolean isprintingactive (void);

extern boolean getprintscale (long * scaleMult, long * scaleDiv);