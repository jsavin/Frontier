
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

#define opsinclude


/*typedefs*/

#pragma pack(2)
typedef struct tylinkedlistrecord {
	
	struct tylinkedlistrecord **hnext;
	} tylinkedlistrecord, *ptrlinkedlist, **hdllinkedlist;
#pragma options align=reset


/*prototypes*/

extern short minint (short, short);

extern short maxint (short, short);

extern short absint (short);

extern boolean delayticks (long);

extern boolean delayseconds (long);

extern void counttickloops (long *);

extern void burntickloops (long);

extern unsigned char uppercasechar (unsigned char);

extern unsigned char lowercasechar (unsigned char);

extern boolean textchar (unsigned char);

extern void shorttostring (short, bigstring);

extern void numbertostring (long, bigstring);

extern boolean stringtoshort (bigstring, short *);

extern boolean stringtonumber (bigstring, long *);

extern boolean stringtofloat (bigstring, double *);

extern boolean floattostring (double, bigstring);

extern long numberfromhandle (Handle);

extern void exittooperatingsystem (void);

extern short dirtoindex (tydirection);

extern tydirection indextodir (short);

extern boolean validdirection (tydirection);

extern tydirection oppositdirection (tydirection dir);

extern long divup (long, long);

extern long divround (long, long);

extern long quantumize (long, long);

extern boolean truenoop (void);

extern boolean falsenoop (void);

extern boolean gestalt (OSType, long *);

extern boolean listlink (hdllinkedlist, hdllinkedlist);

extern boolean listunlink (hdllinkedlist, hdllinkedlist);

#if __powerc || __GNUC__

extern void safex80told (const extended80 *x80, long double *x);

extern void safeldtox80 (const long double *x, extended80 *x80);

#endif

extern void getsystemversionstring (bigstring, bigstring);

extern void getsizestring (unsigned long, bigstring);

extern unsigned long bcdtolong (unsigned long); /* 2004-11-16 creedon */