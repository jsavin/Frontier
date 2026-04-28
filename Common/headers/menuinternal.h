
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

#define menuinternalinclude


/*prototypes*/

extern boolean menuverbunload (hdlexternalvariable);

extern void megetoutlinerect (Rect *); /*menueditor.c*/

extern boolean megetcontentsize (long *, long *);

extern boolean mesomethingdirty (hdlmenurecord);

extern boolean mesmashscriptwindow (void);


extern boolean mesearchrefconroutine (hdlheadrecord); /*menufind.c*/

extern boolean mesearchoutline (boolean, boolean, boolean *);


extern boolean megetmenuiteminfo (hdlheadrecord, tymenuiteminfo *);

extern boolean mesetmenuiteminfo (hdlheadrecord, const tymenuiteminfo *);


extern boolean mecopyrefconroutine (hdlheadrecord, hdlheadrecord); /*menupack.c*/

extern boolean metextualizerefconroutine (hdlheadrecord, Handle);

extern boolean mereleaserefconroutine (hdlheadrecord, boolean);

extern boolean mepackmenustructure (tysavedmenuinfo *, Handle *);

extern boolean mesavemenurecord (const db_context *, hdlmenurecord, boolean, boolean, dbaddress *, Handle *);

extern boolean mesetupmenurecord (tysavedmenuinfo *, hdloutlinerecord, hdlmenurecord *);

extern boolean meunpackmenustructure (Handle, hdlmenurecord *);

extern boolean meloadmenurecord_internal(const db_context *ctx, dbaddress adr,
                                          hdlmenurecord *hmenurecord);

extern boolean meloadmenurecord (dbaddress, hdlmenurecord *);

extern boolean mesetscraproutine (hdloutlinerecord);

extern boolean megetscraproutine (hdloutlinerecord *, boolean *);

extern boolean mescraphook (Handle);


extern boolean mezoommenubarwindow (hdloutlinerecord, boolean, hdlwindowinfo *); /*meprograms.c*/


extern boolean meresetwindowrects (hdlwindowinfo); /*menuresize.c*/

extern void meresize (void);




