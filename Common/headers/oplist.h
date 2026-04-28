
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

#define oplistinclude /*so other guys can tell if we've been included*/


#ifndef shellinclude

	#include "shell.h"

#endif


/*types*/

typedef struct tylistrecord **hdllistrecord;	/* 2004-11-04 aradke: now opaque */

typedef boolean (*oplistreleaseitemcallback) (Handle);	/* 2004-11-04 aradke: part of tylistrecord */

typedef boolean (*opvisitlistcallback) (Handle, ptrstring, ptrvoid);	/* 2004-11-04 aradke */


/*prototypes*/

extern boolean opnewlist (hdllistrecord *, boolean);

extern void opdisposelist (hdllistrecord);

extern boolean oppushhandle (hdllistrecord, ptrstring, Handle);

extern boolean opunshifthandle (hdllistrecord, ptrstring, Handle);

extern boolean oppushdata (hdllistrecord, ptrstring, ptrvoid, long);

extern boolean oppushstring (hdllistrecord, ptrstring, bigstring);

extern boolean opgetlistdata (hdllistrecord, long, ptrstring, ptrvoid, long);

extern boolean opgetlisthandle (hdllistrecord, long, ptrstring, Handle *);

extern boolean opgetliststring (hdllistrecord, long, ptrstring, bigstring);

extern boolean opsetlisthandle (hdllistrecord, long, ptrstring, Handle);

extern boolean opsetlistdata (hdllistrecord, long, ptrstring, ptrvoid, long);

extern long opcountlistitems (hdllistrecord);

extern boolean opgetisrecord (hdllistrecord);	/* 2004-11-04 aradke */

extern void opsetisrecord (hdllistrecord hlist, boolean flisrecord);	/* 2004-11-04 aradke */

extern oplistreleaseitemcallback opsetreleaseitemcallback (hdllistrecord, oplistreleaseitemcallback);	/* 2004-11-04 aradke */

extern boolean opdeletelistitem (hdllistrecord, long, ptrstring);

extern boolean oppacklist (hdllistrecord, Handle *);

extern boolean opunpacklist (Handle, hdllistrecord *);

extern boolean opcopylist (hdllistrecord, hdllistrecord *);

extern boolean oploadstringlist (short, hdllistrecord *);

extern boolean opvisitlist (hdllistrecord, opvisitlistcallback, ptrvoid);	/* 2004-11-04 aradke */

