
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

#ifndef tabledisplayinclude
#define tabledisplayinclude

#define maxbrowsercols 5

#define isclaydisplay(hf) (hf && (**(hf)).linelayout.claydisplay)


boolean tableinitdisplay (void);

boolean tablepushnodestyle (hdlheadrecord);

boolean tabletitleclick (Point);

boolean tablefindcolumnguide (Point, short *);

boolean tableadjustcolwidth (Point, short);

boolean tablechecksortorder (void);

void tableupdatecoltitles (boolean);

void tableupdate (void);

boolean tablegetlineheight (hdlheadrecord, short *);

boolean tablegetlinewidth (hdlheadrecord, short *);

boolean tabledrawline (hdlheadrecord, const Rect *, boolean, boolean);

boolean tablegettextrect (hdlheadrecord, const Rect *, Rect *);

boolean tablegetedittextrect (hdlheadrecord, const Rect *, Rect *);

boolean tablegeticonrect (hdlheadrecord, const Rect *, Rect *);

boolean tablepredrawline (hdlheadrecord, const Rect *, boolean, boolean);

boolean tablepostdrawline (hdlheadrecord, const Rect *, boolean, boolean);

boolean tabledrawnodeicon (hdlheadrecord, const Rect *, boolean, boolean);

boolean tablegetnodeframe (hdlheadrecord, Rect *);

boolean tableadjustcursor (hdlheadrecord, Point, const Rect *);

boolean tablemouseinline (hdlheadrecord, Point, const Rect *, boolean *);

boolean tablereturnkey (tydirection);

boolean tablegetoutlinesize (long *, long *);

boolean tabledefaultdrawcell (hdlheadrecord, short, const Rect *);

boolean tablegetcellstring (hdlheadrecord, short, bigstring, boolean);

#endif
