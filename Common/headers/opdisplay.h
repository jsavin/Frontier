
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

#define opdisplayinclude


extern short opnodeindent (hdlheadrecord);

extern void oplineinval (int64_t);

extern void opscrollrect (Rect r, int64_t dh, int64_t dv);

extern hdlheadrecord oppointnode (Point);

extern void opupdatenow (void);

extern boolean opinitdisplayvariables (void);

extern void oppushheadstyle (hdlheadrecord);

extern void opgettextrect (hdlheadrecord, const Rect *, Rect *);

extern short opgetlineheight (hdlheadrecord);

extern short opgetlinewidth (hdlheadrecord);

extern int64_t opgetnodelinecount (hdlheadrecord);

extern boolean opgetnoderect (hdlheadrecord, Rect *);

extern void operaserect (Rect r);

extern boolean opgetscreenline (hdlheadrecord, int64_t *);

extern boolean opinvalnode (hdlheadrecord);

extern void opinvalstructure (hdlheadrecord);

extern void opinvalafter (hdlheadrecord);

extern void opinvalbarcursor (void);

extern void opinvaldisplay (void);

extern void opsmashdisplay (void);

extern boolean opdirtymeasurements (void);

extern boolean oppostfontchange (void);

extern void opgetlineselected (hdlheadrecord, boolean *, boolean *);

extern boolean opgetlinerect (int64_t, Rect *);

extern void opdrawicon (hdlheadrecord, Rect);

extern void opdrawline (hdlheadrecord, Rect);

extern void opindenteddisplay (void);

extern void opdocursor (boolean);

extern void opmakegap (int64_t, short);

extern void opexpandupdate (hdlheadrecord);

extern boolean opscroll (tydirection, boolean, int64_t);

extern void opjumpdisplayto (hdlheadrecord, hdlheadrecord);

extern boolean opneedvisiscroll (hdlheadrecord, int64_t *, int64_t *, boolean);

extern void opdovisiscroll (int64_t, int64_t);

extern boolean opnodevisible (hdlheadrecord);

extern boolean opvisinode (hdlheadrecord, boolean);

extern void opvisisubheads (hdlheadrecord);

extern void operasedisplay (void);

extern short opmaxlevelwidth (hdlheadrecord);

extern boolean opdefaultpredrawline (hdlheadrecord, const Rect *, boolean, boolean);

extern boolean opdefaultdrawtext (hdlheadrecord, const Rect *, boolean, boolean);

extern boolean opdefaultpostdrawline (hdlheadrecord, const Rect *, boolean, boolean);

extern boolean opdefaultgetlineheight (hdlheadrecord, short *);

extern boolean opdefaultgetlinewidth (hdlheadrecord, short *);

extern boolean opdefaultgettextrect (hdlheadrecord, const Rect *, Rect *);

extern boolean opdefaultgetfullrect (hdlheadrecord, Rect *);

