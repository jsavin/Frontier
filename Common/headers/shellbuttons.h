
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

#define shellbuttonsinclude /*so other modules can tell we've been included*/

/*constants*/

#define maxbuttons 16 // to handle more, we'd have to change flags from short to longs

#define boldbuttonstyle bold

#define minbuttonwidth 45

#define pixelsbetweenbuttons 8

	#define buttontextinset 9

		#define buttonsrectheight 29
	#define buttonbackground qd.gray
	#define whitebackground qd.white


#define buttonfont geneva
#define buttonsize 9

#define linepattern black

#define buttontopinset 5

#define buttonbottominset 6



/*function prototypes*/

extern void shellgetbuttonrect (short, Rect *);

extern void shelldrawbutton (short, boolean);

extern void shelldrawbuttons (void);

extern void shellbuttonhit (Point);

extern void shellgetbuttonsrect (hdlwindowinfo, Rect *);

extern void shellinvalbuttons (void);

extern boolean shellgetbuttonstring (short, bigstring);

extern void shellbuttongetoptimalwidth (short *);

extern void shellbuttonadjustcursor (Point);
