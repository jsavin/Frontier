
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

#define fontinclude /*so other includes can tell if we've been loaded*/


#ifndef shelltypesinclude
	
	#include "shelltypes.h"

#endif

	#define geneva kFontIDGeneva
	#define helv kFontIDHelvetica

extern FontInfo globalfontinfo;


/*prototypes*/

extern void fontgetnumber (bigstring, short *);

extern void fontgetname (short, bigstring);

extern boolean realfont (short, short);

extern void setfontsizestyle (short, short, short);

extern void setglobalfontsizestyle (short, short, short);

extern short setnamedfont (bigstring, short, short, short);

extern void getfontsizestyle (short *, short *, short *);

extern void fontstring (short, short, boolean, boolean, bigstring);

extern void getstyle (short, boolean *, boolean *, boolean *, boolean *, boolean *, boolean *);

extern void diskgetfontname (short, diskfontstring);

extern void diskgetfontnum (diskfontstring, short *);

extern boolean initfonts (void);



/* file.h*/

	
