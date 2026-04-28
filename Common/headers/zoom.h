
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

#define zoominclude


/*prototypes*/

extern void zoomsetdefaultrect (WindowPtr, Rect);

extern void middlerect (Rect, Rect *);

extern void zoomrect (Rect *, Rect *, boolean);

extern void zoomfrommiddle (Rect);

extern void zoomport (Rect, WindowPtr, boolean);

extern void zoomwindowfrom (Rect, WindowPtr);

extern void zoomwindowto (Rect, WindowPtr);

extern void zoomcenterrect (Rect *);

extern void zoomwindowtocenter (Rect, WindowPtr);

extern void zoomwindowfromcenter (Rect, WindowPtr);

extern void zoomfromorigin (WindowPtr);

extern void zoomtoorigin (WindowPtr);

extern boolean zoomtempwindow (boolean, short, short, WindowPtr *);

extern void closetempwindow (boolean, WindowPtr);

extern void modaltempwindow (WindowPtr, void  (*) (WindowPtr));

extern void zoominit (void);



