
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

#define kernelverbsinclude


/*prototypes for initialization*/

extern boolean fileinitverbs (void); /*fileverbs.c*/

extern boolean filelaunchanythingverb (hdltreenode, tyvaluerecord *);

extern boolean stringinitverbs (void); /*stringverbs.c*/

extern boolean sysinitverbs (void); /*shellsysverbs.c*/

extern boolean dbinitverbs (void); /*dbverbs.c*/

extern boolean dbfunctionvalue (short, hdltreenode, tyvaluerecord *, bigstring); /*dbverbs.c*/

extern boolean dbcloseallfiles (long refcon);

extern boolean xmlinitverbs (void); /*langxml.c*/

extern boolean windowinitverbs (void); /*shellwindowverbs.c*/

extern boolean htmlinitverbs (void); /*langhtml.c*/

extern boolean quicktimeinitverbs (void); /*langquicktime.c*/

extern boolean regexpinitverbs (void); /* langregexp.c */

extern boolean mathinitverbs (void); /* langmath.c */

extern boolean cryptinitverbs (void); /* langcrypt.c */

extern boolean sqliteinitverbs (void); /* langsqlite.c */

extern boolean mysqlinitverbs (void); /* tests/headless_mysql_verbs.c (real impl removed in MIT relicensing) */

extern boolean targetinitverbs (void); /* headless_target_verbs.c */

