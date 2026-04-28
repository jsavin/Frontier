
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

#define mouseinclude

#define leftmousebuttonaction 0
#define rightmousebuttonaction 1
#define centermousebuttonaction 2
#define wheelmousebuttonaction 3

#pragma pack(2)
typedef struct tymouserecord {
	
	boolean fldoubleclick;
	
	Point localpt;
	
	long mouseuptime; 
	
	long mousedowntime; 
	
	Point mouseuppoint;
	
	Point mousedownpoint;
	
	boolean fldoubleclickdisabled;

	short whichbutton;
	} tymouserecord;
#pragma options align=reset
	
	
extern tymouserecord mousestatus;


/*prototypes*/

void setmousedoubleclickstatus (boolean fl);

extern boolean mousebuttondown (void);

extern void waitmousebutton (boolean);

extern void waitmouseclick (void);

extern boolean mousestilldown (void);

extern boolean rightmousestilldown (void); /*7.0b26 PBS*/

extern void getmousepoint (Point *);

extern boolean getmousewindowpos (WindowPtr *, Point *);

extern boolean mousetrack (Rect, void (*) (boolean));

extern void mousedoubleclickdisable (void);

extern void mouseup (long eventwhen, long eventposx, long eventposy, long eventwhat);

extern void mousedown (long eventwhen, long eventposx, long eventposy, long eventwhat);

extern boolean mousedoubleclick (void);

extern boolean ismouseleftclick (void);

extern boolean ismouserightclick (void);

extern boolean ismousecenterclick (void);

extern boolean ismousewheelclick (void);

extern void smashmouse (Point);

extern void showmousecursor (void);

extern void hidemousecursor (void);

extern boolean mousecheckautoscroll (Point, Rect, boolean, tydirection *);

extern long getmousedoubleclicktime(void);

