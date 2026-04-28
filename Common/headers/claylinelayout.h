
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

#ifndef claylinelayoutinclude
#define claylinelayoutinclude


typedef enum tyiconsize {
	
	fullsizeicon = 1,
	
	smallsizeicon = 2,
	
	verysmallsizeicon = 3
	} tyiconsize;
	
#pragma pack(2)
typedef struct tylinelayout {
	
	boolean flinitted; //if true, the other fields in this record been set
	
	boolean claydisplay; // if true, we're using the clay line layout
	
	boolean realicons; /*if true, we go into the desktop db for icons, otherwise use generic ones*/
	
	boolean filenamebold; /*if true, the file name is drawn in bold*/
	
	boolean includeline2; /*if true we include the 2nd line of the display*/
	
	boolean includedate; /*if true, we include the modification date*/
	
	boolean includeframe; /*if true, each line has a frame, if false it's just like an outliner*/
	
	tyiconsize iconsize; /*full-size, small or very small icons*/
	
	RGBColor fillcolor, framecolor, cursorcolor, filenamecolor, othertextcolor; 
	
	RGBColor backcolor, statuscolor;
	
	short filenamefont, othertextfont;
	
	short filenamefontsize, othertextfontsize;
	} tylinelayout;


typedef struct tycomputedlineinfo { /*computed fields that depend on a linelayout record*/

	short filenamelineheight;
	
	short othertextlineheight;
	
	short filenamewidth; /*the number of pixels reserved for the file name*/
	
	short datewidth; /*number of pixels reserved for the date*/
	} tycomputedlineinfo;
#pragma options align=reset

#define str_claydisplay		(BIGSTRING ("\x0b" "claydisplay"))
#define str_realicons		(BIGSTRING ("\x09" "realicons"))
#define str_filenamebold	(BIGSTRING ("\x0c" "filenamebold"))
#define str_includeline2	(BIGSTRING ("\x0c" "includeline2"))
#define str_includedate		(BIGSTRING ("\x0b" "includedate"))
#define str_includeframe	(BIGSTRING ("\x0c" "includeframe"))
#define str_iconsize		(BIGSTRING ("\x08" "iconsize"))
#define str_fillcolor		(BIGSTRING ("\x09" "fillcolor"))
#define str_framecolor		(BIGSTRING ("\x0a" "framecolor"))
#define str_cursorcolor		(BIGSTRING ("\x0b" "cursorcolor"))
#define str_filenamecolor	(BIGSTRING ("\x0d" "filenamecolor"))
#define str_othertextcolor	(BIGSTRING ("\x0e" "othertextcolor"))
#define str_backcolor		(BIGSTRING ("\x09" "backcolor"))
#define str_statuscolor		(BIGSTRING ("\x0b" "statuscolor"))
#define str_filenamefont	(BIGSTRING ("\x0c" "filenamefont"))
#define str_othertextfont	(BIGSTRING ("\x0d" "othertextfont"))
#define str_filenamefontsize	(BIGSTRING ("\x10" "filenamefontsize"))
#define str_othertextfontsize	(BIGSTRING ("\x11" "othertextfontsize"))

extern void claybrowserinitdraw (void);

extern boolean claypushnodestyle (hdlheadrecord);

extern boolean claygetlineheight (hdlheadrecord, short *);

extern boolean claygetlinewidth (hdlheadrecord, short *);

extern boolean claydrawline (hdlheadrecord, const Rect *, boolean, boolean);

extern boolean claygettextrect (hdlheadrecord, const Rect *, Rect *);

extern boolean claygetedittextrect (hdlheadrecord, const Rect *, Rect *);

extern boolean claygeticonrect (hdlheadrecord, const Rect *, Rect *);

extern boolean claypredrawline (hdlheadrecord, const Rect *, boolean, boolean);

extern boolean claypostdrawline (hdlheadrecord, const Rect *, boolean, boolean);

extern boolean claydrawnodeicon (hdlheadrecord, const Rect *, boolean, boolean);

extern boolean claygetnodeframe (hdlheadrecord, Rect *);

extern void claysmashoutlinefields (hdlwindowinfo, struct tytableformats **);

extern void claysetlinelayout (hdlwindowinfo, tylinelayout *);

extern boolean clayinitlinelayout (tylinelayout *);

extern boolean claylayouttotable (const tylinelayout *, hdlhashtable);

extern boolean claytabletolayout (hdlhashtable, tylinelayout *);

extern boolean claypacklinelayout (Handle);

extern boolean clayunpacklinelayout (Handle, long *, struct tytableformats **);

#ifdef claydialoginclude

extern boolean claywindowuseslayout (hdlappwindow);

extern void linelayoutprefsdialog (void);

extern void linelayoutbeforeclosewindow (void);

#endif

#endif
