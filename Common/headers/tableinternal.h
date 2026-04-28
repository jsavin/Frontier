
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

#define tableinternalinclude


#ifndef langexternalinclude

	#include "langexternal.h"

#endif

#ifndef tableformatsinclude

	#include "tableformats.h"

#endif

#ifndef tableeditinclude

//	#include "tableedit.h"

#endif

#ifndef dbinclude

	#include "db.h"

#endif


#define sortbyname 0 /*values for "sortorder"*/
#define sortbyvalue 1
#define sortbykind 2


#define tablestringlist 165
#define systemtabletypestring 1
#define tabletypestring 2
#define tablesizestring 3
#define copyofstring 5
#define pastedtextstring 6
#define pastedstring 7
#define pasteasstring 8
#define questionmarksstring 9
#define nametitlestring 10
#define valuetitlestring 11
#define kindtitlestring 12
#define xmltypestring 13

#define tableerrorlist 265
#define notableerror 1
#define internalerror 2
#define namenottableerror 3

extern short tableverberrornum; /*selects an error message from a low-level routine*/


#define namecolumn 0 /*hash key*/

#define valuecolumn 1 /*value data*/

#define kindcolumn 2 /*value type*/



#pragma pack(2)
typedef struct tytablevariable {
	
	unsigned short id; /*tyexternalid: managed by langexternal.c*/
	
	unsigned short flinmemory: 1; /*if true, variabledata is an hdlhashtable, else a dbaddress*/
	
	unsigned short flmayaffectdisplay: 1; /*not in memory, but being displayed in a table window*/
	
	unsigned short flsystemtable: 1; /*was it created by the system, or by a user script?*/
	
	#ifdef xmlfeatures
		unsigned short flxml: 1; /*is it an xml table?*/
	#endif

	long variabledata; /*either a hdlhashtable or a dbaddress*/
	
	hdldatabaserecord hdatabase; // 5.0a18 dmb

	dbaddress oldaddress; /*last place this table was stored in db*/
	} tytablevariable, *ptrtablevariable, **hdltablevariable;
#pragma options align=reset



#define fixedctcols maxtablecols /*symbol tables all have this number of columns*/

#define outlinetopinset 3
#define outlinebottominset 0
#define outlineleftinset 3
#define outlinerightinset 0

typedef boolean (*tyfindvariablecallback) (hdlhashtable, hdlhashnode);

/*
   Phase 9: Refcon wrapper for tablesortedinversesearch.
   Carries the database handle through the existing refcon parameter
   so visit callbacks can build a db_context for disk-value reads
   without mutating the databasedata global.
*/
typedef struct tablesortedsearchctx {
    ptrvoid original_refcon;
    hdldatabaserecord hdb;
} tablesortedsearchctx;


/*prototypes*/

extern void tablegettitlestring (short, bigstring); /*tablecallbacks.c*/


extern void tableoverridesort (hdlhashnode); /*tablecompare.c*/

extern void tablerestoresort (void);


extern boolean tableeditsetglobals (void); /*tableedit.c*/

extern boolean tableeditgetundoglobals (long *);

extern boolean tableeditsetundoglobals (long, boolean);

extern boolean tablecelliseditable (hdlheadrecord, short);

extern boolean tablesetwpedittext (hdlheadrecord);

extern boolean tablegetwpedittext (hdlheadrecord, boolean);


extern boolean tableverbsearch (void); /*tablefind.c*/


// extern boolean tablemakenewvalue (void); /*tablenewvalue.c*/


extern boolean tablesetdebugglobals (hdlhashtable, hdlhashnode); /*tableops.c*/

extern hdlhashtable tablegetlinkedhashtable (void);

extern hdltablevariable tablegetlinkedtablevariable (void);

extern hdltablevariable tablegetlinkedtablevariable (void);

extern boolean tablegetiteminfo (hdlheadrecord, hdlhashtable *, bigstring, tyvaluerecord *, hdlhashnode *);

extern boolean tablegetcursorinfo (hdlhashtable *, bigstring, tyvaluerecord *, hdlhashnode *);

extern void tablelinkformats (hdlhashtable, hdltableformats);

extern boolean istablevariable (hdlexternalvariable);

extern boolean gettablevariable (tyvaluerecord, hdltablevariable *, short *);

extern boolean newtablevariable (boolean, long, hdltablevariable *, boolean, hdldatabaserecord);

extern boolean tablenewtablevalue (hdlhashtable *, tyvaluerecord *);

extern boolean tablefinddatawindow (hdlhashtable, hdlwindowinfo *);

extern boolean tabledisposetable (hdlhashtable, boolean);

extern boolean tablesortedinversesearch (hdlhashtable, langsortedinversesearchcallback, ptrvoid);

extern boolean tablecheckwindowrect (hdlhashtable);

extern boolean findvariablesearch (hdlhashtable, hdlexternalvariable, boolean, hdlhashtable *, bigstring, tyfindvariablecallback);

extern boolean tablenosubsdirty (hdlhashtable);

extern boolean tablepreflightsubsdirtyflag (hdlexternalvariable); /*6.2a15 AR*/

extern boolean tableexiteditmode (void);

extern boolean tablevisicursor (void);

extern boolean tablemovetonode (hdlhashnode);

extern boolean tablebringtofront (hdlhashtable);

extern boolean tableresort (hdlhashtable, hdlhashnode);

extern boolean tablegetsortorder (hdlhashtable, short *);

extern boolean tablesetsortorder (hdlhashtable, short);

extern boolean tablegetcursorpath (bigstring);

extern boolean tablepushcontext (hdlhashtable, tyvaluetype);

extern boolean tablepopcontext (hdlhashtable, tyvaluetype);

extern boolean tablegetstringlist (short, bigstring);


extern boolean tablepacktable (hdlhashtable, boolean, Handle *, boolean *); /*tablepack.c*/

extern boolean tableunpacktable (Handle, boolean, hdlhashtable *);

/* Context-aware internal version — threads guest DB handle to child externals */
extern boolean tableunpacktable_internal (const struct db_context *, Handle, boolean, hdlhashtable *);

/* Legacy (32-bit) pack/unpack entry points used during migration. */
extern boolean tablepacktable_legacy (hdlhashtable, boolean, Handle *, boolean *);
extern boolean tableunpacktable_legacy (Handle, boolean, hdlhashtable *);


extern boolean tablekindpopuphit (Point); /*tablepopup.c*/

extern void tableupdatekindpopup (void);

extern boolean tablesortpopuphit (Point);

extern void tableupdatesortpopup (void);

extern boolean tablesetitemname (hdlhashtable, bigstring, hdlheadrecord, boolean); /*6.2b16 AR: hdlheadrecord instead of bigstring*/

extern boolean tablepopupkinddialog (void);


extern boolean tablesetprintinfo (void); /*tableprint.c*/

extern boolean tableprint (short);


extern boolean tablecursoriszoomable (void); /*tablerunbutton.c*/

extern boolean tablecheckzoombutton (void);

extern void tabledrawzoombutton (boolean);

extern boolean tablezoombuttonhit (void);

extern boolean tablecursorisrunnable (void);

extern boolean tableruncursor (void);


extern boolean tablescraphook (Handle); /*tablescrap.c*/

extern boolean tableexportscrapvalue (const tyvaluerecord *, tyscraptype, Handle *, boolean *);

extern boolean tablecutroutine (void);

extern boolean tablecopyroutine (void);

extern boolean tablepasteroutine (void);

extern boolean tableclearroutine (void);

extern boolean tableundoclear (hdlhashnode, boolean);

extern boolean tableredoclear (hdlhashnode, boolean);


extern boolean tablenewtable (hdltablevariable *, hdlhashtable *); /*tablestructure.c*/


extern boolean tabledive (void); /*tableexternal.c*/

extern boolean tablesurface (void);

extern boolean tablezoomfromhead (hdlheadrecord);

extern boolean tablesymbolsresorted (hdlhashtable);


extern boolean tablebeforeprintpage (void); /*tablewindow.c*/

extern boolean tableafterprintpage (void);
