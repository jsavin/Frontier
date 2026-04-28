
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

/*
	2004-10-26 aradke: New global header file, to be included from frontier.h and frontier.r.
*/

#ifndef __FRONTIERDEFS_H__
#define __FRONTIERDEFS_H__

#if   defined(__GNUC__)
    #define noextended 1
#else
    #define noextended 0
#endif

#undef MEMTRACKER		/* define as 1 to enable tracking of memory allocations */
#undef DATABASE_DEBUG	/* define as 1 to enable database debugging and logging code */

#ifndef fldebug
#define fldebug 1
#endif

#ifndef OPMLEDITOR				/*2008-09-08 aradke: keep opml editor lean and mean*/
	#define FRONTIER_SQLITE	1	/*include sqlite database code*/
#endif

#endif /*__FRONTIERDEFS_H__*/
