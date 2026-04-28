
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

#define miniwindowinclude /*so other modules can tell we've been loaded*/


#ifndef shellinclude

	#include "shell.h"
	
#endif


#ifndef langinclude

	#include "lang.h" /*our record has a valuerecord in it*/
	
#endif

#ifndef popupinclude

	#include "popup.h"

#endif

#define maxtextitems 2 

#define maxpopups 2


typedef boolean (*minisavestringcallback) (short, Handle); 
	
typedef boolean (*miniloadstringcallback) (short, Handle *);
	
typedef boolean (*minitexthitcallback) (Point);
	

#pragma pack(2)
typedef struct tyminirecord {
	
	short idconfig; /*resource number for config that governs this window*/
	
	short windowtype; /*an index into cancoon.h's windowinfo array*/
	
	short cttextitems; /*actual number of valid item numbers*/
	
	short textitems [maxtextitems]; /*dialog item numbers of the text items*/
	
	Rect textrects [maxtextitems]; /*location of text items*/
	
	struct tywprecord ** textdata [maxtextitems]; /*data of text items*/
	
	short ctpopups; /*actual number of popup menus*/
	
	Rect popuprects [maxpopups]; /*each rect covers a whole popup item, including arrow*/
	
	hdlstring popupmessages [maxpopups]; /*the string displayed in the rect*/
	
	short popupnumber; /*during a popup hit, this is the index of the popup, else undefined*/
	
	short activetextitem; /*index of active text item*/
	
	Rect iconrect; /*location of the icon, for mouse clicks, cursor adjustment*/
	
	byte iconlabel [11]; /*max 10 chars for string under icon, length byte at head*/
	
	byte windowtitle [33]; /*max 32 chars for window title, length byte at head*/
	
	tyvaluerecord minivalue; /*any miniwindow can have a value associated with it*/
	
	Rect msgrect; /*location of messages displayed in this window*/
	
	bigstring bsmsg; /*string displayed in msgrect, result of shell.windowmessage call*/
	
	long forecolor, backcolor; /*uses the old QuickDraw colors, should be modernized*/
	
	boolean fliconenabled; /*enabled if there's text that can be run*/
	
	boolean flactive; /*determines whether items are drawn as active or inactive*/
	
	boolean flmassiveupdate; /*erase whole rect, use offscreen bitmap*/
	
	boolean flselectallpending; /*selectall in idle, if flactive is true*/
	
	boolean flbitmapactive; /*for some drawing, if true don't open a bitmap*/
	
	minisavestringcallback savestringroutine; 
	
	miniloadstringcallback loadstringroutine;
	
	shellcallback setvalueroutine; /*fills in the minivalue field of this record*/
	
	minitexthitcallback texthitroutine;
	
	shellvoidcallback iconenableroutine;
	
	shellcallback iconhitroutine;
	
	fillpopupcallback fillpopuproutine;
	
	popupselectcallback popupselectroutine;
	
	shellshortcallback gettargetdataroutine;
	
	long minirefcon; /*optional data for the next layer up*/
	} tyminirecord, *ptrminirecord, **hdlminirecord;
#pragma options align=reset


// globals

extern WindowPtr miniwindow;

extern hdlwindowinfo miniwindowinfo;

extern hdlminirecord minidata;


// prototypes

extern boolean minisetstring (short, Handle);

extern boolean minigetstring (short, Handle *);

extern boolean minigetselstring (short, bigstring);

extern boolean minisetpopupmessage (short, bigstring);

extern void minisetselect (short, short);

extern boolean miniinvalicon (short);

extern boolean minisetwindowmessage (short, bigstring);

extern boolean startminidialog (short, callback);

extern boolean ministart (short);

extern boolean minireloadstrings ( short ); // 2007-07-27 creedon

