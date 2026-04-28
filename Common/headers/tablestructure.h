
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

#define tablestructureinclude

#ifndef langinclude

	#include "lang.h"

#endif

#ifndef dbinclude

	#include "db.h"

#endif


/*constants*/

#define idsystemtablescripts 139

enum  { /*indexes of system table scripts*/
	
	idmenubarscript = 1,
	
	idobjectdbscript,
	
	idquickscriptscript,
	
	idtechsupportscript,
	
	idfinder2clickscript,
	
	idfinder2frontscript,
	
	idfrontierclickers,
	
	idcontrol2clickscript,
	
	idcommand2clickscript,
	
	idoption2clickscript,
	
	idopenwindowscript,
	
	idsavewindowscript,
	
	idclosewindowscript,
	
	idcompilewindowscript,
	
	idisfirsttimescript,
	
	idopenurlscript,
	
	iduseriso8859map,
	
	iduserfontprefscript,
	
	idinexpertmodescript,
	
	idtoggleexpertmodescript,
	
	idrequiredeclarationsscript,
	
	idsuspendscript,
	
	idresumescript,
	
	idsearchparamstable,
	
	idagentsenabledscript,

	idautosave,
	
	idfrontierstartup,
	
	idflwaitduringstartup,
	
	idwebserverstats,

	idinetdshutdown,

	idpikeisfilemenuitemenabledscript,

	idpikegetmenuitemstring,

	idrunfilemenuscript, /* 2005-09-14 creedon - changed name from idpikerunfilemenuscript to idrunfilemenuscript */

	idopstruct2clickscript,

	idopreturnkeyscript,
	
	idopexpandscript,
	
	idopcollapsescript,
	
	idopcursormovedscript, /*7.0b6 PBS*/

	idoprightclickscript,

	idruneditmenuscript, /* 2005-09-25 creedon - changed name from idpikeruneditmenuscript to idruneditmenuscript */ 

	idpikeisfilemenuitemcheckedscript,

	idopinsertscript,
	
	idopenrecentmenutable = 44, /* 2005-09-22 creedon */ 
	
	idreplacedialogexpertmode, /* 2005-09-26 creedon */
	
	idrunopenrecentmenuscript /* 2005-09-29 creedon */
	};


/*globals*/

extern Handle rootvariable;

extern hdlhashtable roottable;

extern hdlhashtable internaltable;

extern hdlhashtable systemtable;

/* Issue #292: efptable is module-private in tablestructure.c.
   Use get_efptable()/set_efptable() for access. The macro below
   provides backward compatibility for read sites. */
extern hdlhashtable get_efptable(void);
extern void set_efptable(hdlhashtable);
#define efptable (get_efptable())

extern hdlhashtable langtable;

extern hdlhashtable builtinstable;

extern hdlhashtable agentstable;

extern hdlhashtable runtimestacktable;

extern hdlhashtable semaphoretable;

extern hdlhashtable threadtable;

extern hdlhashtable filewindowtable;

extern hdlhashtable verbstable;

extern hdlhashtable resourcestable;

extern hdlhashtable pathstable;

extern hdlhashtable iacgluetable;

extern hdlhashtable iachandlertable;

extern hdlhashtable menubartable;

extern hdlhashtable objectmodeltable;

extern hdlhashtable environmenttable;

extern hdlhashtable charsetstable;


extern byte nameinternaltable []; 

extern byte namemenubar []; 

extern byte namebeginnermenus []; 

extern byte namebuiltinstable [];

extern byte nameagentstable [];

extern byte nameresourcestable [];

extern byte nameefptable [];

extern byte namelangtable [];

extern byte namestacktable [];

extern byte namesemaphoretable [];

extern byte namethreadtable [];

extern byte namefilewindowtable [];

extern byte nameroottable [];

extern byte namestartuptable [];

extern byte namesuspendtable [];

extern byte nameresumetable [];

extern byte nameshutdowntable [];

extern byte namesystembranch [];

extern byte nameverbstable [];

extern byte namepathstable [];

extern byte nameiacgluetable [];

extern byte nameiachandlertable [];

extern byte namemenubartable [];

extern byte nameenvironmenttable [];

extern byte namecharsetstable [];


/*prototypes*/

extern boolean linksystemtablestructure (hdlhashtable); /*tablestructure.c*/

extern boolean headless_init_system_paths (hdlhashtable); /*tablestructure.c*/

extern boolean resolve_system_paths (hdlhashtable); /*tablestructure.c*/

extern boolean augment_database_tables_with_efp (hdlhashtable); /*tablestructure.c*/

extern boolean getsystemtablescript (short, bigstring);

extern boolean unlinksystemtablestructure (void);

extern boolean tablenewsubtable (hdlhashtable, bigstring, hdlhashtable *);

extern boolean tablenewsystemtable (hdlhashtable, bigstring, hdlhashtable *);

extern boolean tableloadsystemtable (dbaddress, Handle *, hdlhashtable *, boolean);

extern boolean tablesavesystemtable (Handle, dbaddress *);

extern boolean tablesavesystemtable (Handle, dbaddress *);

extern boolean checktablestructure (boolean);

extern boolean cleartablestructureglobals (void);

extern boolean settablestructureglobals (Handle, boolean);


extern void initsegment (void); /*tablestartup.c*/

extern boolean loadfunctionprocessor (short, langvaluecallback);
/* Headless: programmatic EFP creation */
extern boolean newfunctionprocessor (bigstring bsname, langvaluecallback valuecallback, boolean flwindow, hdlhashtable *htable);

extern boolean inittablestructure (void);


extern boolean tablevalidate (hdlhashtable, boolean); /*tablevalidate.c*/


