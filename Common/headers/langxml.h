
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
