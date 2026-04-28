
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

#define wpverbsinclude


/*prototypes*/

extern boolean wpverbgetdisplaystring (hdlexternalvariable, bigstring);

extern boolean wpverbgettypestring (hdlexternalvariable, bigstring);

extern boolean wpverbdispose (hdlexternalvariable, boolean);

extern boolean wpverbisdirty (hdlexternalvariable);

extern boolean wpverbsetdirty (hdlexternalvariable, boolean);

extern boolean wpverbnew (Handle, hdlexternalvariable *);

extern boolean wpverbmemorypack (hdlexternalvariable, Handle *);

extern boolean wpverbmemoryunpack (Handle, long *, hdlexternalvariable *);

extern boolean wpverbpack (hdlexternalvariable, Handle *, boolean *);

extern boolean wpverbunpack (Handle, long *, hdlexternalvariable *, hdldatabaserecord);

/* Context-aware internal version (used by langexternal layer) */
struct db_context; /* forward declaration */
extern boolean wpverbpack_internal (const struct db_context *, hdlexternalvariable, Handle *, boolean *);

/* ctx parameter is forward-looking (Phase 4: unpack/load path will use it
   to read from the correct database). All callers currently pass NULL. */
extern boolean wpverbinmemory (const struct db_context *, hdlexternalvariable);

extern boolean wpverbpacktotext (hdlexternalvariable, Handle);

/* Legacy (32-bit) pack/unpack shims used during migration. */
extern boolean wpverbmemorypack_legacy (hdlexternalvariable, Handle *);
extern boolean wpverbmemoryunpack_legacy (Handle, long *, hdlexternalvariable *);
extern boolean wpverbpack_legacy (hdlexternalvariable, Handle *, boolean *);
extern boolean wpverbunpack_legacy (Handle, long *, hdlexternalvariable *);
extern boolean wpverbpacktotext_legacy (hdlexternalvariable, Handle);

extern boolean wpverbgetsize (hdlexternalvariable, long *);

extern boolean wpverbgettimes (hdlexternalvariable, int64_t *, int64_t *);

extern boolean wpverbsettimes (hdlexternalvariable, int64_t, int64_t);

extern boolean wpwindowopen (hdlexternalvariable, hdlwindowinfo *);

extern boolean wpedit (hdlexternalvariable, hdlwindowinfo, ptrfilespec, bigstring, rectparam);

extern boolean wpverbfind (hdlexternalvariable, boolean *);

extern boolean wpstart (void);

