
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

#define iconinclude




#define maxcustomicontypes 20

#define idfirstcustomicon 1000

struct tycustomicontypeinfo {
	
	bigstring bstype; /*headline type*/
	
	short rnum; /*reference to file's resource fork*/
	}; 



/*function prototypes*/
extern boolean ploticonfromodb (const Rect *r, short align, short transform, bigstring bsadricon);

extern void drawlabeledicon (const Rect *, short, bigstring, boolean);

extern void drawiconsequence (Rect, short, short, bigstring);

extern void drawlabeledwindoidicon (Rect, bigstring, boolean, boolean);

extern boolean trackicon (Rect, void (*) (boolean));

extern boolean ploticon (const Rect *, short id);

extern boolean ploticonresource (const Rect *r, short align, short transform, short resid);

extern boolean ploticoncustom (const Rect *r, short align, short transform, bigstring bsiconname); /*7.0b9 PBS*/

boolean customicongetrnum (bigstring bstype, short *rnum);