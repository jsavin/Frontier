
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

#ifndef quickdrawinclude
#define quickdrawinclude /*so other includes can tell if we've been loaded*/

#ifdef HEADERTRACE
#pragma message( "**Compiling " __FILE__ )
#endif

#ifndef shelltypesinclude
	
	#include "shelltypes.h"

#endif

#define blackindex 0
#define whiteindex 1
#define lighterblueindex 2 /*a very common color in System 7 culture*/
#define lightblueindex 2 /*another very common color*/


	#define blackpattern qd.black
	#define whitepattern qd.white
	#define graypattern qd.gray



	#define flushwindowbuffer(w) QDFlushPortBuffer (GetWindowPort (w), nil)


extern RGBColor blackcolor, whitecolor, lightbluecolor, lightgraycolor, graycolor, darkgraycolor;

extern RGBColor lightyellowcolor, lightgreencolor, lightpurplecolor, lightredcolor;

extern RGBColor darkredcolor, darkbluecolor, darkgreencolor, mediumgraycolor, darkpurplecolor;


extern short getmenubarheight (void);

extern boolean pushpen (void);

extern boolean poppen (void);

extern void getpenpoint (Point *);

extern boolean equalpoints (Point, Point);

extern short pointdist (Point, Point);

extern void movepento (short, short);

extern void pendrawline (short, short);

extern void pendrawstring (bigstring);

extern void graydrawstring (bigstring);

extern short stringpixels (bigstring);

extern void setport (GrafPtr);

extern GrafPtr getport (void);


extern boolean pushport (CGrafPtr);

extern boolean popport (void);

extern boolean pushscratchport (void);

extern boolean pushcliprgn (hdlregion, boolean);

extern boolean pushclip (Rect);

extern boolean superpushclip (Rect);

extern boolean pushvalidrgnclip (void);

extern boolean pushemptyclip (void);

extern boolean popclip (void);

extern boolean pushstyle (short, short, short);

extern boolean popstyle (void);

extern boolean pushforecolor (const RGBColor *);

extern boolean popforecolor (void);

extern boolean pushbackcolor (const RGBColor *);

extern boolean popbackcolor (void);

extern boolean pushcolors (const RGBColor *, const RGBColor *);

extern boolean popcolors (void);

extern void centerrect (Rect *, Rect);

extern void centerrectondesktop (Rect *);

extern void centerbuttonstring (const Rect *, bigstring, boolean);

#define centerstring(r, bs) centerbuttonstring (&r, bs, false)

extern void grayrect (Rect);

extern void evenrectangle (Rect *);

extern void oddrectangle (Rect *);

extern void zerorect (Rect *);

extern boolean equalrects (Rect, Rect);

extern boolean issubrect (Rect, Rect);

extern void cyclecolor (long *);

extern void globaltolocalrgn (hdlregion);

extern void localtoglobalrect (WindowPtr, Rect *);

extern void globaltolocalpoint (WindowPtr, Point *);

extern void localtoglobalpoint (WindowPtr, Point *);

extern void flashrect (Rect, short);

extern void dropshadowrect (Rect, short, boolean);

extern void smashrect (Rect);

extern void invalrect (Rect);

extern void invalwindowrect (WindowPtr, Rect);

extern void validrect (Rect);

extern void eraserect (Rect);

extern void framerect (Rect);

extern void setgraypen (void);

extern void setthemepen (const short brush, Rect r, boolean flupdate); /*7.0b51 PBS*/

extern void graydrawline (short, short);

extern void grayframerrgn (hdlregion rgn);

extern void grayframerect (Rect);

extern void fillrect (Rect, xppattern);

extern void paintrect (Rect);

extern void frame3sides (Rect);

extern void eraseandframerect (Rect);

extern void invertrect (Rect);

extern void setrect (Rect *, short, short, short, short);

extern void insetrect (Rect *, short, short);

extern void offsetrect (Rect *, short, short);

extern boolean pointinrect (Point, Rect);

extern boolean rectinregion (Rect, hdlregion);

extern void scrollrect (Rect, short, short);

extern void scrollrectvert (Rect, short);

extern void scrollrecthoriz (Rect, short);

extern void unionrect (Rect, Rect, Rect *);

extern boolean intersectrect (Rect, Rect, Rect *);

extern void getdesktopbounds (Rect *);

extern void getwindowscreenbounds (const Rect *, Rect *);

extern boolean constraintodesktop (WindowPtr, Rect *);

extern boolean constraintoscreenbounds (WindowPtr, boolean, Rect *);

extern boolean pushdesktopport (CGrafPtr);

extern void popdesktopport (CGrafPtr);

extern boolean havecolorquickdraw (void);

extern void getmainscreenrect (Rect *);

extern void getsystemoriginrect (Rect *);

extern void getcurrentscreenbounds (Rect *);

extern boolean systemhascolor (void);

extern boolean colorenabled (void);

extern short iscolorport (CGrafPtr);

extern void fillcolorrect (Rect, short);

/*
extern boolean pushforecolor (short);

extern boolean popforecolor (void);

extern boolean pushbackcolor (short);

extern boolean popbackcolor (void);
*/

void diskrecttorect (diskrect *rdisk, Rect *r);

void recttodiskrect (Rect *r, diskrect *rdisk);

void diskrgbtorgb (diskrgb *rgbdisk, RGBColor *r);

void rgbtodiskrgb (RGBColor *r, diskrgb *rgbdisk);

extern void drawthemeborder (Rect r, Rect rcontent); /*7.0b52 PBS: scan-lines border on OS X*/

extern void initquickdraw (void);

#endif



