
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

#define configinclude /*so other includes can tell if we've been loaded*/

#include <standard.h>


/*resnums of 'cnfg' resources for window types*/

#define idmenueditorconfig 128
#define idiowaconfig 129 /*3/18/92 dmb*/
#define idoutlineconfig 130 
#define idscriptconfig 131
#define idtableconfig 132
#define xxxidtextconfig 133
#define idwpconfig 134
#define xxxidmailconfig 135
#define xxxidprogressconfig 136
#define	idaboutconfig 137


	#define idcommandconfig 146 /*OS X uses a normal window*/
	#define idlangerrorconfig 145

#define idstatsconfig 140
#define idpictconfig 141
#define idcancoonconfig 142
#define idlangdialogconfig 143

	#define idplayerconfig 144 /*PBS 7.0b4: QuickTime Player window*/


#define typeunknown '\?\?\?\?'

#pragma pack(2)
typedef struct tyconfigrecord {

	short flhorizscroll; /*window has horiz scrollbar?*/
	
	short flvertscroll;
	
	short flwindowfloats; /*is it a floating palette window?*/
	
	short flmessagearea; /*allocate space for a message area?*/
	
	short flinsetcontentrect; /*if true we inset by 3 pixels*/
	
	short flnewonlaunch;
	
	short flopenresfile;
	
	short fldialog; /*do a GetNewDialog on creating one of these windows?*/
	
	short flgrowable; /*provide a grow box for window*/
	
	short flcreateonnew;
	
	short flwindoidscrollbars;
	
	short flstoredindatabase;
	
	short flparentwindowhandlessave;
	
	short fleraseonresize;
	
	short fldontconsumefrontclicks; 
	
	short flcolorwindow; 

	short messageareafraction;
	
	OSType filecreator, filetype;
	
	short templateresnum; 
	
	Rect rmin; /*for growable windows, the minimum size allowed*/
	
	short defaultfont; /*version on disk indexes into a STR# list*/
	
	short defaultsize;
	
	short defaultstyle;
	
	short idbuttonstringlist; /*if 0, no buttons for this window type*/
	
	Rect defaultwindowrect; /*new windows come up in this spot*/
	} tyconfigrecord, *ptrconfigrecord, **hdlconfigrecord;
#pragma options align=reset

/*global variables*/

extern tyconfigrecord config; /*load from resource file on initialization*/

extern short iddefaultconfig; /*the type of window a New command creates - set in main.c*/


/*prototypes*/

extern void loadconfigresource (short, tyconfigrecord *);

extern boolean saveconfigresource (short, tyconfigrecord *);

extern boolean getprogramname (bigstring);

extern boolean getuntitledfilename (bigstring);

extern boolean getdefaultfilename (bigstring);

extern boolean getusername (bigstring);

extern void initconfig (void);




