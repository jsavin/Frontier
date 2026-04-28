
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

#define menubarinclude


#ifndef shelltypesinclude
	
	#include "shelltypes.h"

#endif


#ifndef opinclude

	#include "op.h"

#endif


/*types*/

#define maxmenus 96 /*maximum number of user-defined menus for each menu list*/

#define ctmenubarstack 50 /*maximum number of menus stacked in each menubar outline*/

#pragma pack(2)
typedef struct tymenubarstackelement {

	hdlmenu hmenu; /*the menu data structure itself*/
	
	short idmenu; /*Menu Manager id of this menu*/
	
	hdlheadrecord hnode; /*the structure that created this menu*/

	boolean flhierarchic; /*is it a sub-menu?*/
	
	boolean flenabled; /*is it enabled?*/
	
	boolean flbuiltin; /*does menu belong to the Frontier app?*/
	
	short ctbaseitems; /*if flbuiltin, number of items that were already there*/
	} tymenubarstackelement;
#pragma options align=reset

#pragma pack(2)
typedef struct tymenubarstack {
	
	struct tymenubarstack **hnext; /*linked list of these*/
	
	hdloutlinerecord menubaroutline; /*the outline that is at one w/the menubar structure*/
	
	boolean flactive: 1;
	
	boolean flclientowned: 1; /*actual menus owned by menu sharing client?*/
	
	short ixdeletedmenu; /*for rebuilding menu bar*/
	
	short topstack;
	
	long refcon;
	
	tymenubarstackelement stack [ctmenubarstack];
	} tymenubarstack, *ptrmenubarstack, **hdlmenubarstack;
#pragma options align=reset


#pragma pack(2)
typedef struct tymenubarlist {
	
	hdlmenubarstack hfirst; /*linked list of these*/
	
	boolean flactive: 1; /*is this the active menubar?*/
	
	short basemenuid;
	
	byte menubitmap [(maxmenus / 8) + 1]; /*assume eight bits per byte*/
	} tymenubarlist, *ptrmenubarlist, **hdlmenubarlist;
#pragma options align=reset


typedef boolean (*menubarchangedcallback)(hdloutlinerecord);

typedef byte (*menubarcmdkeycallback)(hdlheadrecord);

#pragma pack(2)
typedef struct tymenubarcallbacks {
	
	menubarchangedcallback menubarchangedroutine;
	
	menubarcmdkeycallback getcmdkeyroutine;
	} tymenubarcallbacks;
#pragma options align=reset


/*globals*/

extern hdlmenubarlist menubarlist;

extern tymenubarcallbacks menubarcallbacks;

	
/*prototypes*/

extern boolean pushmenubarglobals (hdlmenubarstack);

extern boolean popmenubarglobals (void);

extern boolean newmenubarlist (hdlmenubarlist *);

extern void setcurrentmenubarlist (hdlmenubarlist);

extern boolean activatemenubarlist (hdlmenubarlist, boolean);

extern boolean disposemenubarlist (hdlmenubarlist);

extern void medirtymenubar (void);

extern void meupdatemenubar (void);

extern void mecheckmenubar (void);

extern boolean medisposemenubar (hdlmenubarstack);

extern boolean menewmenubar (hdloutlinerecord, hdlmenubarstack *);

extern boolean mebuildmenubar (hdlmenubarstack);

extern boolean memenuitemchanged (hdlmenubarstack, hdlheadrecord);

extern short mecheckdeletedmenu (short, boolean);

extern boolean memenuitemadded (hdlmenubarstack, hdlheadrecord);

extern boolean memenuitemdeleted (hdlmenubarstack, hdlheadrecord);

extern boolean meinsertmenubar (hdlmenubarstack);

extern boolean medeletemenubar (hdlmenubarstack);

extern boolean purgefrommenubarlist (long);

extern boolean rebuildmenubarlist (void);

extern boolean melocatemenubarnode (hdlheadrecord, hdloutlinerecord *);

extern boolean mecheckformulas (short);

extern boolean memenuhit (short, short, hdlheadrecord *);

extern boolean memenu (short, short);

extern void menubarinit (void);

extern boolean mereduceformula (bigstring bs); /*7.0b12 PBS: used by oppopup.c*/

extern void mereducemenucodes (bigstring bs, boolean *flenabled, boolean *flchecked); /*7.0b23 PBS: used by oppopup.c*/




