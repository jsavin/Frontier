
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

#define pictstringlist 167
#define picttypestring 1
#define picttextstring 2
#define pictsizestring 3
#define picterrorstring 5


#pragma pack(2)
typedef struct typictrecord {
	
	PicHandle macpicture; /*the structure that's passed off to DrawPicture*/
	
	Rect windowrect; /*the size of the window that last displayed this pict*/

	int64_t timecreated, timelastsave; /*maybe we'll use these at some later date?*/
	
	long ctsaves; /*the number of times this structure has been saved*/
	
	short updateticks; /*how many ticks between updates when window is in front*/
	
	long timelastupdate; /*last time this pict was updated*/
	
	long pictrefcon; /*could be anything -- we don't care*/

	boolean fldirty: 1; /*maybe someday we'll have a pict editor*/
	
	boolean fllocked: 1; /*are changes allowed to be made?*/
	
	boolean flwindowopen: 1; /*true if the record is being edited in a window*/
	
	boolean flbitmapupdate: 1; /*if true, use offscreen bitmap when updating*/
	
	boolean flevalexpressions: 1; /*if true, parse all text that begins with an = sign*/
	
	boolean flscaletofitwindow: 1; /*if true, scale window down to fit inside window*/

	} typictrecord, *ptrpictrecord, **hdlpictrecord;

/* 2025-12-05: Verify 8-byte alignment of 64-bit timestamp fields under #pragma pack(2) */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(offsetof(typictrecord, timecreated) % 8 == 0, "typictrecord.timecreated must be 8-byte aligned");
_Static_assert(offsetof(typictrecord, timelastsave) % 8 == 0, "typictrecord.timelastsave must be 8-byte aligned");
#endif

#pragma options align=reset
	
	
extern WindowPtr pictwindow;

extern hdlwindowinfo pictwindowinfo;

extern hdlpictrecord pictdata;


/*prototypes*/


extern void pictdirty (void);

extern boolean pictgetframerect (hdlpictrecord, Rect *);

extern boolean pictpack (hdlpictrecord, Handle *);

extern boolean pictunpack (Handle, long *, hdlpictrecord *);

extern boolean pictnewrecord (void);

extern boolean pictdisposerecord (hdlpictrecord);

extern boolean pictreadfile (bigstring, PicHandle *);

extern void pictresetscrollbars (void);

extern boolean pictscroll (tydirection, boolean, long);

extern void pictupdatepatcher (void);

extern void pictdepatcher (void);

extern void pictupdate (void);

extern void pictidle (void);

extern void pictscheduleupdate (short);

extern void pictsetbitmapupdate (boolean);

extern void pictsetevaluate (boolean);

extern void pictgetnewwindowrect (hdlpictrecord, Rect *);




