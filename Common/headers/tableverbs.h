
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

#ifndef tableverbinclude
#define tableverbinclude


#include "langexternal.h"


/*prototypes*/


extern short tablecomparenodes (hdlhashtable, hdlhashnode, hdlhashnode); /*tablecompare.c*/


extern boolean tableverbfind (hdlexternalvariable, boolean *); /*tablefind.c*/

extern boolean tableverbcontinuesearch (hdlexternalvariable);


extern boolean tablefindvariable (hdlexternalvariable, hdlhashtable *, bigstring); /*tableops.c*/

extern boolean findnamedtable (hdlhashtable, bigstring, hdlhashtable *);

extern boolean findinparenttable (hdlhashtable, hdlhashtable *, bigstring);

extern boolean tableverbunload (hdlexternalvariable);

extern boolean tablemovetoname (hdlhashtable, bigstring);

extern hdldatabaserecord tablegetdatabase (hdlhashtable);


extern boolean tableverbmemorypack (hdlexternalvariable, Handle *, hdlhashnode); /*tablepack.c*/

extern boolean tableverbmemoryunpack (Handle, long *, hdlexternalvariable *, boolean);

extern boolean tableverbpack (hdlexternalvariable, Handle *, boolean *);

extern boolean tableverbunpack (Handle, long *, hdlexternalvariable *, boolean, hdldatabaserecord);

/* Context-aware internal versions (used by langexternal layer) */
struct db_context; /* forward declaration */
extern boolean tableverbpack_internal (const struct db_context *, hdlexternalvariable, Handle *, boolean *);

extern boolean tableverbunpack_internal (const struct db_context *, Handle, long *, hdlexternalvariable *, boolean);

extern boolean tableverbpacktotext (hdlexternalvariable, Handle);

extern boolean tableverbgettimes (hdlexternalvariable, int64_t *, int64_t *, hdlhashnode);

extern boolean tableverbsettimes (hdlexternalvariable, int64_t, int64_t, hdlhashnode);

extern boolean tableverbfindusedblocks (hdlexternalvariable, bigstring bspath);

/* Legacy (32-bit) pack/unpack entry points used during migration. */
extern boolean tableverbmemorypack_legacy (hdlexternalvariable, Handle *, hdlhashnode);
extern boolean tableverbmemoryunpack_legacy (Handle, long *, hdlexternalvariable *, boolean);
extern boolean tableverbpack_legacy (hdlexternalvariable, Handle *, boolean *);
extern boolean tableverbunpack_legacy (Handle, long *, hdlexternalvariable *, boolean);
extern boolean tableverbpacktotext_legacy (hdlexternalvariable, Handle);
extern boolean tableverbgettimes_legacy (hdlexternalvariable, long *, long *, hdlhashnode);
extern boolean tableverbsettimes_legacy (hdlexternalvariable, long, long, hdlhashnode);
extern boolean tableverbfindusedblocks_legacy (hdlexternalvariable, bigstring bspath);


extern boolean tableclienttitlepopuphit (Point, hdlexternalvariable); /*tablepopup.c*/


extern boolean tabledroppasteroutine (void); /*tablescrap*/


extern boolean tableinitverbs (void); /*tableverbs.c*/

extern boolean tablefunctionvalue (short, hdltreenode, tyvaluerecord *, bigstring); /*tableverbs.c*/

extern boolean gettablevalue (hdltreenode, short, hdlhashtable *);


extern boolean tablewindowopen (hdlexternalvariable, hdlwindowinfo *); /*tableexternal.c*/

extern boolean tablevaltotable (tyvaluerecord, hdlhashtable *, hdlhashnode);

extern boolean tableverbgetdisplaystring (hdlexternalvariable, bigstring);

extern boolean tableverbgettypestring (hdlexternalvariable, bigstring);

extern boolean tableverbgetsize (hdlexternalvariable, long *);

extern boolean tableverbinmemory (const struct db_context *, hdlexternalvariable, hdlhashnode);

extern boolean tableverbdispose (hdlexternalvariable, boolean);

extern boolean tableverbnew (hdlexternalvariable *);

extern boolean tableverbisdirty (hdlexternalvariable);

extern boolean tableverbsetdirty (hdlexternalvariable, boolean);

extern boolean tableedit (hdlexternalvariable, hdlwindowinfo, ptrfilespec, bigstring, rectparam);

extern boolean tablewindowclosed (hdlexternalvariable);

extern boolean tablezoomfromtable (hdlhashtable);

extern boolean tablezoomtoname (hdlhashtable, bigstring);

extern boolean tableclientsurface (hdlexternalvariable);

extern boolean tableverbwindowopen (short);

extern boolean tablepushglobals (hdlhashtable);

extern boolean tablesymbolchanged (hdlhashtable, const bigstring, hdlhashnode, boolean);

extern boolean tablesymbolinserted (hdlhashtable, const bigstring);

extern boolean tablesymboldeleted (hdlhashtable, const bigstring);

extern boolean tableverbclose (void);

extern boolean tableverbsetglobals (void);

extern boolean tableverbsetupdisplay (hdlhashtable, hdlwindowinfo);

extern boolean tableresetformatsrects (void);

extern boolean tablestart (void);

#endif


