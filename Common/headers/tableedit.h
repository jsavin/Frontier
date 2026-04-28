
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

#define tableeditinclude


/*globals*/

extern boolean flmustexiteditmode;


/*prototypes*/

extern boolean tableeditsetglobals (void);

extern boolean tableeditingemptycell (void);

extern boolean tablecelliseditable (short, short);

extern boolean tableeditdrawcell (short, short);

extern boolean tableeditactivate (boolean);

extern boolean tableeditsetbufferrect (void);

extern boolean tableeditleavecell (void);

extern boolean tableeditentercell (short, short);

extern boolean tableeditmousedown (Point, short, short);

extern boolean tableeditgo (tydirection);

extern boolean tableeditkeystroke (void);

extern boolean tableeditidle (void);

extern boolean tableeditsetselect (short, short);

extern boolean tableeditpoststylechange (void);

extern boolean tableeditadjustcursor (short, short);

extern boolean tableeditgetundoglobals (long *);

extern boolean tableeditsetundoglobals (long, boolean);




