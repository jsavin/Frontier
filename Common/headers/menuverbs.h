
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

#define menuverbsinclude


#ifndef langexternalinclude

	#include "langexternal.h"

#endif

#ifndef menueditorinclude
	
	#include "menueditor.h"

#endif


/*prototypes*/

extern boolean menuverbgetdisplaystring (hdlexternalvariable, bigstring);

extern boolean menuverbgettypestring (hdlexternalvariable, bigstring);

extern boolean menuverbisdirty (hdlexternalvariable);

extern boolean menuverbsetdirty (hdlexternalvariable, boolean);

extern boolean menuverbmemorypack (hdlexternalvariable, Handle *);

extern boolean menuverbmemoryunpack (Handle, long *, hdlexternalvariable *);

extern boolean menuverbpack (hdlexternalvariable, Handle *, boolean *);

extern boolean menuverbpack_internal (const db_context *, hdlexternalvariable, Handle *, boolean *);

extern boolean menuverbunpack (Handle, long *, hdlexternalvariable *, hdldatabaserecord);

extern boolean menuverbinmemory_context (const db_context *, hdlexternalvariable);

extern boolean menuverbpacktotext (hdlexternalvariable, Handle);

extern boolean menuverbgetsize (hdlexternalvariable, long *);

extern boolean menuverbgettimes (hdlexternalvariable, int64_t *, int64_t *);

extern boolean menuverbsettimes (hdlexternalvariable, int64_t, int64_t);

extern boolean menuverbfindusedblocks (hdlexternalvariable, bigstring);

extern boolean menuverbfind (hdlexternalvariable, boolean *);

extern boolean menuwindowopen (hdlexternalvariable, hdlwindowinfo *);

extern boolean menuedit (hdlexternalvariable, hdlwindowinfo, ptrfilespec, bigstring, rectparam);

extern boolean menuverbdispose (hdlexternalvariable, boolean);

extern boolean menuverbnew (Handle, hdlexternalvariable *);

extern boolean menuverbcopyvalue (hdlexternalvariable, hdlexternalvariable *);

extern boolean menunewmenubar (hdlhashtable, bigstring, hdlmenurecord *);

extern boolean menugetmenubar (hdlhashtable, bigstring, boolean, hdlmenurecord *);

extern boolean editnamedmenubar (hdlhashtable, bigstring);

extern boolean editnamedmenubaropen (hdlhashtable, bigstring);

extern boolean menustart (void);


/*
 * V7 Menu Refcon Table Functions (menupack.c)
 *
 * These functions create and unpack the v7 format for menu item refcons.
 * Used for the menu v6->v7 migration.
 */

extern boolean mecreaterefcontable_v7 (byte cmdkey, tykeyflags modifiers,
                                        hdloutlinerecord hscript,
                                        Handle *hpackedtable);

extern boolean meunpackrefcontable_v7 (Handle hpackedtable,
                                        byte *cmdkey,
                                        tykeyflags *modifiers,
                                        hdloutlinerecord *hscript);

