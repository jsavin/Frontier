
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

#define pictverbsinclude

/*prototypes*/

extern boolean pictverbgetdisplaystring (hdlexternalvariable, bigstring);

extern boolean pictverbgettypestring (hdlexternalvariable, bigstring);

extern boolean pictverbdispose (hdlexternalvariable, boolean);

extern boolean pictverbnew (Handle, hdlexternalvariable *);

extern boolean pictverbisdirty (hdlexternalvariable);

extern boolean pictverbsetdirty (hdlexternalvariable, boolean);

extern boolean pictverbinmemory (const db_context *, hdlexternalvariable);

extern boolean pictverbmemorypack (hdlexternalvariable, Handle *);

extern boolean pictverbmemoryunpack (Handle, long *, hdlexternalvariable *);

extern boolean pictverbpack_internal (const db_context *, hdlexternalvariable, Handle *, boolean *);

extern boolean pictverbpack (hdlexternalvariable, Handle *, boolean *);

extern boolean pictverbunpack (Handle, long *, hdlexternalvariable *, hdldatabaserecord);

extern boolean pictverbpacktotext (hdlexternalvariable, Handle);

extern boolean pictverbgetsize (hdlexternalvariable, long *);

extern boolean pictverbgettimes (hdlexternalvariable, int64_t *, int64_t *);

extern boolean pictverbsettimes (hdlexternalvariable, int64_t, int64_t);

extern boolean pictwindowopen (hdlexternalvariable, hdlwindowinfo *);

extern boolean pictedit (hdlexternalvariable, hdlwindowinfo, ptrfilespec, bigstring, rectparam);

extern boolean pictverbfind (hdlexternalvariable, boolean *);

extern boolean pictstart (void);



