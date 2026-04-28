
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

#ifndef resourcesinclude
#define resourcesinclude


#ifndef shelltypesinclude

	#include "shelltypes.h"

#endif


/*prototypes*/

/*
2005-09-02 creedon: added support for fork parameter, added short to many of the prototypes, see resources.c: openresourcefile and pushresourcefile	
*/ 

extern boolean getstringlist (short, short, bigstring);

extern boolean findstringlist (bigstring, short, short *);

extern boolean closeresourcefile (short);

extern boolean openresourcefile ( const ptrfilespec, short *, short );

extern boolean writeresource (ResType, short, bigstring, long, void *);

extern boolean copyresource (short, short, ResType, short);

extern boolean copyallresources (short, short);

extern Handle getresourcehandle (ResType, short);

extern void releaseresourcehandle (Handle);

extern Handle filegetresource (short, ResType, short, bigstring);

extern boolean filereadresource (short, ResType, short, bigstring, long, void *);

extern boolean filewriteresource (short, ResType, short, bigstring, long, void *);

extern boolean saveresource (const ptrfilespec , short, ResType, short, bigstring, long, void *, short);

extern boolean saveresourcehandle (const ptrfilespec, ResType, short, bigstring, Handle, short);

extern boolean loadresource (const ptrfilespec, short, ResType, short, bigstring, long, void *, short);

extern boolean loadresourcehandle (const ptrfilespec, ResType, short, bigstring, Handle *, short);

extern boolean deleteresource (const ptrfilespec, ResType, short, bigstring, short);

extern boolean getnumresourcetypes (const ptrfilespec, short *, short);

extern boolean getnthresourcetype (const ptrfilespec, short, ResType *, short);

extern boolean getnumresources (const ptrfilespec, ResType, short *, short);

extern boolean getnthresourcehandle (const ptrfilespec, ResType, short, short *, bigstring, Handle *, short);

extern boolean getresourceattributes (const ptrfilespec, ResType, short, bigstring, short *, short);

extern boolean setresourceattributes (const ptrfilespec, ResType, short, bigstring, short, short);

#endif


/*constants*/

typedef enum tyforktype {

	resourcefork = 1, 

	datafork

	} tyforktype;

