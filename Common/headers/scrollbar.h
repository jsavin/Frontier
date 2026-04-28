
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

#define scrollbarinclude /*so other includes can tell if we've been loaded*/

#ifndef shelltypesinclude
	
	#include "shelltypes.h"

#endif

#pragma pack(2)
typedef struct tyscrollinfo {
	
	long min, max, cur, pag;
	} tyscrollinfo, *ptrscrollinfo;
#pragma options align=reset


/*prototypes*/

extern void validscrollbar (hdlscrollbar);

extern boolean pointinscrollbar (Point, hdlscrollbar);

extern void getscrollbarinfo (hdlscrollbar, tyscrollinfo *);

extern void setscrollbarinfo (hdlscrollbar, const tyscrollinfo *);

extern void enablescrollbar (hdlscrollbar);

extern void disablescrollbar (hdlscrollbar);

extern long getscrollbarcurrent (hdlscrollbar);

extern void showscrollbar (hdlscrollbar);

extern void hidescrollbar (hdlscrollbar);

extern void drawscrollbar (hdlscrollbar);

extern void displayscrollbar (hdlscrollbar);

extern void setscrollbarcurrent (hdlscrollbar, long);

extern short getscrollbarwidth (void);

extern boolean newscrollbar (WindowPtr, boolean, hdlscrollbar *);

extern void disposescrollbar (hdlscrollbar);

extern void getscrollbarrect (hdlscrollbar, Rect *);

extern void setscrollbarrect (hdlscrollbar, Rect);

extern void scrollbarflushright (Rect, hdlscrollbar);

extern void scrollbarflushbottom (Rect, hdlscrollbar);

extern boolean findscrollbar (Point, WindowPtr, hdlscrollbar *, short *);

extern boolean scrollbarhit (hdlscrollbar, short, boolean *, boolean *);

extern boolean initscrollbars (void); /*7.0b18 PBS*/
