
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

#define opverbsinclude

#ifndef opinclude
	
	#include "op.h"

#endif


/*prototypes*/

extern boolean opverbgetsummitstring (hdlexternalvariable, bigstring);

extern boolean opverbgettypestring (hdlexternalvariable, bigstring);

extern boolean opverbdispose (hdlexternalvariable, boolean);

extern void opverbunload (hdlexternalvariable, dbaddress);

extern boolean opverbisdirty (hdlexternalvariable);

extern boolean opverbsetdirty (hdlexternalvariable, boolean);

extern boolean opverblinkcode (hdlexternalvariable, Handle);

extern boolean opverbgetlinkedcode (hdlexternalvariable, hdltreenode *);

extern boolean opverbmemorypack (hdlexternalvariable, Handle *);

extern boolean opverbmemoryunpack (Handle, long *, hdlexternalvariable *);

extern boolean opverbscriptmemoryunpack (Handle, long *, hdlexternalvariable *);

extern boolean opverbinmemory (const struct db_context *, hdlexternalvariable);

extern boolean opverbpack (hdlexternalvariable, Handle *, boolean *);

extern boolean opverbunpack (Handle, long *, hdlexternalvariable *, hdldatabaserecord);

/* Context-aware internal version (used by langexternal layer) */
struct db_context; /* forward declaration */
extern boolean opverbpack_internal (const struct db_context *, hdlexternalvariable, Handle *, boolean *);

extern boolean opverbscriptunpack (Handle, long *, hdlexternalvariable *, hdldatabaserecord);

extern boolean opverbpacktotext (hdlexternalvariable, Handle);

/* Legacy (32-bit) pack/unpack shims used during migration. */
extern boolean opverbmemorypack_legacy (hdlexternalvariable, Handle *);
extern boolean opverbmemoryunpack_legacy (Handle, long *, hdlexternalvariable *);
extern boolean opverbscriptmemoryunpack_legacy (Handle, long *, hdlexternalvariable *);
extern boolean opverbpack_legacy (hdlexternalvariable, Handle *, boolean *);
extern boolean opverbunpack_legacy (Handle, long *, hdlexternalvariable *);
extern boolean opverbscriptunpack_legacy (Handle, long *, hdlexternalvariable *);
extern boolean opverbpacktotext_legacy (hdlexternalvariable, Handle);

extern boolean opverbgetsize (hdlexternalvariable, long *);

extern boolean opverbgettimes (hdlexternalvariable, int64_t *, int64_t *);

extern boolean opverbsettimes (hdlexternalvariable, int64_t, int64_t);

extern boolean opverbnew (short, Handle, hdlexternalvariable *);

extern boolean opverbcopyvalue (hdlexternalvariable, hdlexternalvariable *);

extern boolean opverbgetlangtext (hdlexternalvariable, boolean, Handle *, long *);

extern boolean getoutlinevalue (hdltreenode, short, hdloutlinerecord *);

extern boolean opverbarrayreference (hdlexternalvariable, long, hdlheadrecord *);

extern boolean opwindowopen (hdlexternalvariable, hdlwindowinfo *);

extern boolean opedit (hdlexternalvariable, hdlwindowinfo, ptrfilespec, bigstring, rectparam);

extern boolean opvaltoscript (tyvaluerecord, hdloutlinerecord *);

extern boolean opverbgetheadstring (hdlheadrecord, bigstring);

extern boolean opverbclose (void);

extern boolean opverbfind (hdlexternalvariable, boolean *);

extern boolean opverbruncursor (void);

extern boolean opverbgetvariable (hdlexternalvariable *);

extern boolean opverbgettargetdata (short);

extern boolean opstart (void);


