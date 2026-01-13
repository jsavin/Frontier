
/*	$Id$    */

/******************************************************************************

    UserLand Frontier(tm) -- High performance Web content management,
    object database, system-level and Internet scripting environment,
    including source code editing and debugging.

    Copyright (C) 1992-2004 UserLand Software, Inc.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

******************************************************************************/

/*
langxml.h -- 7.0b21 PBS
*/

#ifndef langxmlinclude
#define langxmlinclude

#ifndef oplistinclude
	#include "oplist.h"
#endif

typedef struct xmladdress {
	
	hdlhashtable ht;
	bigstring bs;
	} xmladdress, *ptrxmladdress;

extern boolean xmlcompile (Handle htext, xmladdress *xmladr);

extern boolean xmldecompile (hdlhashtable hxmltable, Handle *htext);

extern boolean isxmlmatch (hdlhashnode hn, bigstring name);

extern boolean xmlgetname (bigstring bsname);

extern boolean xmlgetattribute (hdlhashtable ht, bigstring name, hdlhashtable *adratts);

extern boolean gethashnodetable (hdlhashnode hn, hdlhashtable *ht);

extern boolean replaceallinhandle (bigstring bsfind, bigstring bsreplace, Handle htext);

extern boolean xmlfrontiervaltotaggedtext (tyvaluerecord *val, short indentlevel, Handle *xmltext, hdlhashnode hnode);

/* Tree navigation functions - exported for headless implementation */
extern boolean xmlgetaddress (hdlhashtable ht, bigstring name);

extern boolean xmlgetaddresslist (hdlhashtable ht, bigstring name, boolean justone, hdllistrecord *hlist);

extern boolean xmlgetpathaddress (tyaddress *xtable, Handle h, tyaddress *adrresult, boolean *flvalid);

/* Value and structure conversion functions - exported for Phase 3-5 implementations */
extern void getnewitemaddress (hdlhashtable ht, bigstring bs, xmladdress *adr);

extern boolean xmlvaltostring (tyvaluerecord xmlval, short indentlevel, boolean fltranslatestrings, Handle *string);

extern boolean xmlstructtofrontiervalue (tyaddress *adrstruct, tyvaluerecord *v);

#endif /* langxmlinclude */
