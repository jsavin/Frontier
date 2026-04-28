
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

#define windowsinclude


/*prototypes*/

extern WindowPtr getnewwindow (short, boolean, Rect *);

extern void disposewindow (WindowPtr);
	
extern void windowsettitle (WindowPtr, bigstring);

extern void windowgettitle (WindowPtr, bigstring);

extern void windowinval (WindowPtr);

extern boolean graywindow (WindowPtr);

extern boolean windowbringtofront (WindowPtr);

extern boolean windowsendtoback (WindowPtr);

extern boolean windowsendbehind (WindowPtr, WindowPtr);

extern boolean getlocalwindowrect (WindowPtr, Rect *);

extern boolean getglobalwindowrect (WindowPtr, Rect *);

extern void movewindow (WindowPtr, short, short);

extern void movewindowhidden (WindowPtr, short, short);

extern void sizewindow (WindowPtr, short, short);

extern void sizewindowhidden (WindowPtr, short, short);

extern void moveandsizewindow (WindowPtr, Rect);

extern WindowPtr getnextwindow (WindowPtr w);

extern WindowPtr getfrontwindow (void);

extern boolean findmousewindow (Point, WindowPtr *, short *);

extern boolean windowsetcolor (WindowPtr, long, boolean);

extern boolean isdeskaccessorywindow (WindowPtr);

extern void showwindow (WindowPtr);

extern void hidewindow (WindowPtr);

extern boolean windowvisible (WindowPtr);

extern hdlregion getupdateregion (WindowPtr);

extern hdlregion getvisregion (WindowPtr);

extern void setwindowrefcon (WindowPtr, long);

extern long getwindowrefcon (WindowPtr);

