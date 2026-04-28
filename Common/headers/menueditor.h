
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

#define menueditorinclude /*so other includes can tell if we've been loaded*/


#ifndef shelltypesinclude
	
	#include "shelltypes.h"

#endif


#ifndef dbinclude

	#include "db.h"

#endif


#ifndef opinclude

	#include "op.h"

#endif


#ifndef menubarinclude

	#include "menubar.h"

#endif


#ifndef langexternalinclude

	#include "langexternal.h"

#endif


#define menuoutlineitem 1 /*the menubar outline*/

#define menucmdkeypopupitem 2 /*popup menu with cmdkeys*/

#define menumessageitem 3 /*where the message goes in this window*/

#define menucmdkeyitem 4 /*displays cmd-key equiv for current menu item*/

#define menuscriptitem 5 /*the script button*/

		#define cmdkeypopupwidth 60

#pragma pack(2)
typedef struct tysavedmenuinfo { 
	
	short versionnumber; /*this structure is saved on disk*/
	
	dbaddress adroutline; /*where the menubar outline is stored in the database*/
	
	short vertmin, vertmax, vertcurrent; /*remember the state of the scrollbar*/
	
	diskrect scriptwindowrect; /*the saved position for the script window*/
	
	short flags;
	
	short menuactivelayer;

	int64_t lnumcursor; /* 2026-02-05: expanded to 64-bit for consistency with v7 disk format */

	diskfontstring defaultscriptfontname; /*new scripts start in this font*/

	short defaultscriptfontsize; /*and this size*/

	diskrect menuwindowrect; /*the window size and position last time it was open*/

	char waste [36]; /*room to grow - reduced from 42 to account for lnumcursor expansion*/
	} tysavedmenuinfo;
#pragma options align=reset

/* Verify in-memory struct size remains stable - must match tyOLD42savedmenuinfo for compatibility */
_Static_assert(sizeof(tysavedmenuinfo) == 116, "tysavedmenuinfo must be exactly 116 bytes");

	#define flautosmash_mask 0x8000


#pragma pack(2)
typedef struct tyOLD42savedmenuinfo { 
	
	short versionnumber; /*this structure is saved on disk*/
	
	dbaddress adroutline; /*where the menubar outline is stored in the database*/
	
	short vertmin, vertmax, vertcurrent; /*remember the state of the scrollbar*/
	
	diskrect scriptwindowrect; /*the saved position for the script window*/
	
	boolean flautosmash: 1;
	
	short menuactivelayer;
	
	short lnumcursor;
	
	diskfontstring defaultscriptfontname; /*new scripts start in this font*/
	
	short defaultscriptfontsize; /*and this size*/ 
	
	diskrect menuwindowrect; /*the window size and position last time it was open*/
	
	char waste [42]; /*room to grow*/
	} tyOLD42savedmenuinfo;
#pragma options align=reset



#pragma pack(2)
	typedef struct tymenurecord {
		
		hdloutlinerecord menuoutline; /*the display of the menubar structure*/
		
		dbaddress adroutline; /*where the menubar outline is stored in the database*/
		
		hdlmenubarstack hmenustack; /*menubar.c's data structure -- the user's menus*/
		
		Rect menuwindowrect; /*the window size and position last time it was open*/
		
		short menuactiveitem; /*which text item is the active one?*/
		
		short menuactivelayer; /*which layer is active?*/
		
		WindowPtr scriptwindow; /*the window that the script is displayed in, might be nil*/
		
		Rect scriptwindowrect; /*the saved window position for the script window*/
		
		Rect menuoutlinerect; /*the current menubar outline rect*/
		
		Rect cmdkeypopuprect; /*the cmdkey popup rect*/
		
		Rect iconrect; /*the script zoom icon button*/
		
		hdlheadrecord scriptnode; /*the node whose script is being displayed*/
		
		hdloutlinerecord scriptoutline; /*the outline record of the script being edited*/
		
		short defaultscriptfontnum, defaultscriptfontsize; /*font and size for new scripts*/
		
		short movecursorto; /*a signal between meload and meedit*/
		
		boolean fldirty; /*any changes since last save?*/
		
		boolean fllocked; /*are changes allowed to be made?*/
		
		boolean flwindowopen; /*is this menubar being edited now?*/
		
		boolean flactive;
		
		boolean flautosmash;
		
		boolean flinstalled; /*explicitly installed in menubar?*/
		
		boolean flcursormoved; /*do we need to check status of button, popup?*/
		
		/***boolean flpopupactive;*/
		
		/***boolean flzoomscriptwindow; /%zoom out the window in the next idle?*/
		
		long menurefcon; /*for use by application, menueditor doesn't touch it, but menuverbs does*/
		} tymenurecord, *ptrmenurecord, **hdlmenurecord;
#pragma options align=reset
	

#pragma pack(2)
typedef struct tylinkeditem {
	
	dbaddress adrlink;
	
	hdloutlinerecord houtline; /*kept in memory until it is saved*/
	} tylinkeditem;


typedef struct tymenuiteminfo { /*linked into the refcon handle in each node*/
	
	byte cmdkey; 
	
	byte cmdmodifiers;
	
	tylinkeditem linkedscript;
	} tymenuiteminfo, *ptrmenuiteminfo, **hdlmenuiteminfo;
#pragma options align=reset


/*globals*/
	
extern hdlmenurecord menudata;

extern WindowPtr menuwindow; 

extern hdlwindowinfo menuwindowinfo; 


/*prototypes*/

extern boolean meloadoutline_internal(const db_context *ctx, dbaddress adr,
                                       hdloutlinerecord *houtline);

extern boolean meloadoutline (dbaddress, hdloutlinerecord *); /*menueditor.c*/

extern boolean mepackoutline (hdloutlinerecord, Handle *);

extern boolean mesaveoutline (const db_context *, hdloutlinerecord, dbaddress *);

extern boolean meloadscriptoutline (hdlmenurecord, hdlheadrecord, hdloutlinerecord *, boolean *);

extern boolean mezoomscriptwindow (void);

extern boolean mescriptwindowclosed (void);

extern void mepostcursormove (void);

extern void meexpandto (hdlheadrecord);

extern void mesetcallbacks (hdloutlinerecord);

extern boolean meeditmenurecord (void);

extern boolean menewmenurecord (hdlmenurecord *);

extern void medisposemenurecord (hdlmenurecord, boolean);

extern boolean meinstallmenubar (hdlmenurecord);

extern boolean meremovemenubar (hdlmenurecord);

extern boolean meclearmenubar (void);

#ifdef fldebug

	extern void mecheckglobals (void);

#else

	#define mecheckglobals() ((void) 0)

#endif

extern boolean mesetglobals (void);

extern hdldatabaserecord megetdatabase (hdlmenurecord);

extern boolean mesetscriptoutline (hdlheadrecord, hdloutlinerecord);

extern boolean mesomethingdirty (hdlmenurecord);

extern void mesetcmdkey (byte, tykeyflags);

extern void meupdate (void);

extern void meactivate (boolean);

extern boolean mescroll (tydirection, boolean, int64_t);

extern void megetscrollbarinfo (void);

extern boolean megetundoglobals (long *);

extern boolean mesetundoglobals (long, boolean);

extern boolean memousedown (Point, tyclickflags);

extern boolean mekeystroke (void);

extern boolean mecmdkeyfilter (char);

extern boolean mecut (void);

extern boolean mecopy (void);

extern boolean mepaste (void);

extern boolean meclear (void);

extern boolean meselectall (void);

extern boolean medispose (void);

extern boolean meclose (void);

extern void meidle (void);

extern boolean meadjustcursor (Point);

extern boolean mesetprintinfo (void);

extern boolean meprint (short);

extern void meinit (void);


extern boolean mecontinuesearch (hdlwindowinfo, hdlheadrecord); /*menufind.c*/

extern boolean menuverbsearch (void);



