
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

#define opentrypointsinclude

/*
function prototypes for op.c routines that like to be linked into callback 
structures.  these are in their own .h file because some files only need to
have these names defined and don't need any detailed knowledge of op.h's
data structures.
*/

extern boolean opupdate (void);

extern boolean opactivate (boolean);

extern boolean opidle (void);

extern boolean opsetcursor (void);

extern boolean opgetscrollbarinfo (void);

extern boolean opresetscrollbars (void);

extern boolean opscroll (tydirection, boolean, int);

extern boolean opscrollto (boolean, int);

extern boolean opsetselectioninfo (void);

extern boolean opmousedown (Point);

extern boolean opkeystroke (void);

extern boolean opcmdkeyfilter (char);

extern boolean opundo (void);

extern boolean opcut (void);

extern boolean opcopy (void);

extern boolean oppaste (void);

extern boolean opclear (void);

extern boolean opselectall (void);

extern boolean opreadscrap (void);

extern boolean opwritescrap (void);

extern opdisposeoutline (hdloutlinerecord, boolean);
