
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

#define cancooninternalinclude /*so other guys can tell if we've been included*/



#ifndef dbinclude

	#include "db.h"

#endif

#define cancoonversionnumber 0x03
#pragma pack(2)
typedef struct tyversion1cancoonrecord {
	
	short versionnumber;
	
	dbaddress adrroottable;
	
	Rect msgwindowrect;
	
	diskfontstring msgfontname;
	
	short msgfontsize;
	
	Rect langerrorwindowrect;
	
	diskfontstring langerrorfontname;
	
	short langerrorfontsize;
	
	char waste [8]; /*room to grow*/
	} tyversion1cancoonrecord;
	

typedef struct tyversion2cancoonrecord {
	
	short versionnumber;
	
	dbaddress adrroottable;
	
	tycancoonwindowinfo windowinfo [ctwindowinfo];
	
	dbaddress adrscriptstring; /*the string that appears in the quickscript window*/
	
	unsigned short flags;
	
	short ixprimaryagent;
	
	short waste [28]; /*room to grow*/
	} tyversion2cancoonrecord;
#pragma options align=reset

	#define flflagdisabled_mask 0x8000 /*hide the flag?*/
	#define flpopupdisabled_mask 0x4000 /*hide the agents popup menu?*/
	#define flbigwindow_mask 0x2000 /*is the flag toggled to the big window state?*/


#pragma pack(2)
typedef struct tyOLD42version2cancoonrecord {
	
	short versionnumber;
	
	dbaddress adrroottable;
	
	tycancoonwindowinfo windowinfo [ctwindowinfo];
	
	dbaddress adrscriptstring; /*the string that appears in the quickscript window*/
	
	unsigned short flflagdisabled: 1; /*hide the flag?*/
	
	unsigned short flpopupdisabled: 1; /*hide the agents popup menu?*/
	
	unsigned short flbigwindow: 1; /*is the flag toggled to the big window state?*/
	
	unsigned short unusedbits: 13; /*make sure the used bits never move*/
	
	short ixprimaryagent;
	
	short waste [28]; /*room to grow*/
	} tyOLD42version2cancoonrecord;
#pragma options align=reset


/*internal globals*/

extern hdlcancoonrecord cancoondata;

extern hdlwindowinfo cancoonwindowinfo;

extern WindowPtr cancoonwindow;


/*prototypes*/

extern boolean ccgetwindowinfo (short, tycancoonwindowinfo *); /*cancoon.c*/

extern boolean ccsetwindowinfo (short, tycancoonwindowinfo);

extern boolean ccloadfile (hdlfilenum, short);

extern boolean ccloadspecialfile (ptrfilespec, OSType);

extern boolean ccsavefile (ptrfilespec, hdlfilenum, short, boolean, boolean);

extern boolean ccnewrecord (void);

extern boolean ccdisposerecord (void);

extern boolean ccsetdatabase (void);

extern boolean ccgetdatabase (hdldatabaserecord *);

extern boolean ccsetsuperglobals (void);

extern boolean ccbackground (void);

extern boolean ccfnumchanged (hdlfilenum);

extern boolean ccfindusedblocks (void);

extern boolean ccpreclose (WindowPtr);	/*4.1b5 dmb*/

extern boolean ccclose (void);

extern boolean ccchildclose (WindowPtr);

extern boolean cceditmenubar (boolean);

extern boolean ccagentpopuphit (Rect, Point); /*cancoonpopup.c*/

extern void ccupdateagentpopup (Rect);

extern boolean ccgetprimaryagent (short *);

extern boolean ccsetprimaryagent (short);

extern boolean cccodereplaced (hdltreenode, hdltreenode);


extern boolean ccinitverbs (void); /*cancoonverbs.c*/


extern void ccwindowsetup (boolean, boolean); /*cancoonwindow.c*/

extern boolean cchelpcommand (void);

extern boolean cctoggleflag (void);

extern boolean cctoggleagentspopup (void);

extern boolean ccflipflag (void);

extern boolean ccmsg (bigstring, boolean);

extern boolean ccwindowstart (void);



