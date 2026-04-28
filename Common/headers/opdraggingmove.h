
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

#define opdraggingmoveinclude


#ifndef opinternalinclude

	#include "opinternal.h"

#endif

#define draggingscrollrate 2 /*max scroll rate is 30 lines per second*/

#pragma pack(2)
typedef struct tyhotspot {
	
	Point pt; /*the mouse point that determined the rect*/
	
	short lnum; /*the line number that htarget is displayed on*/
	
	hdlheadrecord htarget; /*where the deposit takes place*/
	
	hdlheadrecord hsource; /*the node that's getting moved*/
	
	tydirection dir; /*deposit to the right or down*/
	
	boolean fldisplayed: 1; /*is the hot spot showing?*/
	
	Handle sourcewindowhandle, destwindowhandle;
	} tyhotspot;
#pragma options align=reset


extern void operasehotspot (tyhotspot *hotspot);

extern void opscrollfordrag (tyhotspot *hotspot, tydirection scrolldir);

extern boolean opisdraggingmove (Point, unsigned long);

extern void opdraggingmove (Point, hdlheadrecord);

extern boolean opmovetohotspot (tyhotspot *hotspot);

extern void opgetwindowhandle (Point pt, Handle *windowhandle);

extern void opsetwindowhandlecontext (Handle windowhandle);

