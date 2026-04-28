
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

#define langsystem7include


/*
#ifndef __ALIASES__

	#include <Aliases.h>

#endif
*/


#ifndef langinclude

	#include "lang.h"
	
#endif


	#define filespecsize(fs) (sizeof (tyfilespec)) // - sizeof (CFStringRef) + stringsize ((fs).name))



typedef boolean (*langvisitlistvaluescallback) (tyvaluerecord *, ptrvoid); /*2003-04-28 AR*/


/*prototypes*/

#if !defined(FRONTIER_USE_PORTABLE_HANDLES)
typedef void* AliasHandle;
#endif

extern boolean filespectoalias (const ptrfilespec , boolean, AliasHandle *);  /*landsystem7.c*/

extern boolean aliastostring (Handle, bigstring);

extern boolean aliastofilespec (AliasHandle, ptrfilespec );

extern boolean coercetoalias (tyvaluerecord *);

extern boolean langpackfileval (const tyvaluerecord *, Handle *);

extern boolean langunpackfileval (Handle, tyvaluerecord *);

extern boolean objspectoaddress (tyvaluerecord *);

extern boolean objspectofilespec (tyvaluerecord *);

extern boolean filespectoobjspec (tyvaluerecord *);

extern boolean objspectostring (Handle, bigstring);

extern boolean coercetoobjspec (tyvaluerecord *);

extern boolean setobjspecverb (hdltreenode, tyvaluerecord *);

extern boolean evaluateobjspec (hdltreenode, tyvaluerecord *);

extern boolean isobjspectree (hdltreenode);


extern boolean filespecaddvalue (tyvaluerecord *, tyvaluerecord *, tyvaluerecord *);

extern boolean filespecsubtractvalue (tyvaluerecord *, tyvaluerecord *, tyvaluerecord *);

extern boolean getobjectmodeldisplaystring (tyvaluerecord *, bigstring);

extern boolean makelistvalue (hdltreenode, tyvaluerecord *);

extern boolean makerecordvalue (hdltreenode, boolean, tyvaluerecord *);

extern boolean langgetlistsize (const tyvaluerecord *, long *);

	extern boolean langgetlistitem (const tyvaluerecord *, long, ptrstring, tyvaluerecord *);
	
	extern boolean langpushlistval (struct tylistrecord **, ptrstring, tyvaluerecord *);

	extern boolean langpushlisttext (struct tylistrecord ** hlist, Handle hstring);
	
	extern boolean langpushliststring (struct tylistrecord ** hlist, bigstring bs);
	
	extern boolean langpushlistaddress (struct tylistrecord ** hlist, hdlhashtable ht, bigstring bs);
	
	extern boolean langpushlistlong (struct tylistrecord ** hlist, long num);
	
	extern boolean getnthlistval (struct tylistrecord ** hlist, long n, ptrstring pkey, tyvaluerecord *val);

	extern boolean setnthlistval (struct tylistrecord ** hlist, long n, ptrstring pkey, tyvaluerecord *val);

extern boolean coercetolist (tyvaluerecord *, tyvaluetype);

extern boolean coercelistvalue (tyvaluerecord *, tyvaluetype);

extern boolean listaddvalue (tyvaluerecord *, tyvaluerecord *, tyvaluerecord *);

extern boolean listsubtractvalue (tyvaluerecord *, tyvaluerecord *, tyvaluerecord *);

extern boolean listcomparevalue (tyvaluerecord *, tyvaluerecord *, tytreetype, tyvaluerecord *);

extern boolean coercetolistposition (tyvaluerecord *);
	
extern boolean listarrayvalue (tyvaluerecord *, bigstring, tyvaluerecord *, tyvaluerecord *);

extern boolean listassignvalue (tyvaluerecord *, bigstring, tyvaluerecord *, tyvaluerecord *);

extern boolean listdeletevalue (tyvaluerecord *, bigstring, tyvaluerecord *);

extern boolean langvisitlistvalues (tyvaluerecord *, langvisitlistvaluescallback, ptrvoid); /*2003-04-28 AR*/
