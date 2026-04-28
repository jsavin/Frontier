
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

#define WIN32
#define _WIN32
#define _WINDOWS
#define _X86_

#define _WIN32_WINNT 	0x0400		/* Windows NT 4.0 or later */
#define _WIN32_WINDOWS	0x0410		/* Windows 98 or later */

#define WIN95VERSION 1

#define PACKFLIPPED		/* enable little endian / big endian conversion for database file functions */

#undef PIKE				/* define as 1 to build Radio.exe (previously known as Pike) */

#define FRONTIERCOM 1
#define FRONTIERWEB 0

#undef MEMTRACKER		/* define as 1 to enable tracking of memory allocations */
#undef fltrialsize		/* define as 1 to build trial version with expiration logic */
#undef DATABASE_DEBUG	/* define as 1 to enable database debugging and logging code */


#undef lazythis_optimization
#undef langexternalfind_optimization

#undef winhybrid
#define fljustpacking 0
#undef flruntime
#define fldebug 1
#define flnewfeatures 1
#define version42orgreater 1
#define version5orgreater 1
#undef flcomponent
#define isFrontier 1
#undef dropletcomponent
#undef fliowa
#define threadverbs 1
#define oplanglists 1
#define flregexpverbs 1
#define gray3Dlook 1
#define noextended 1

#undef macBirdRuntime
#undef appRunsCards /*for Applet Toolkit, Iowa Runtime is baked in*/
#undef iowaRuntimeInApp /*iowa code knows it's in an app*/
#undef iowaRuntime /*iowa code knows it's not compiling in Card Editor*/
#undef cmdPeriodKillsCard
#undef IOAinsideApp /*all the IOA's are baked into the app*/
#undef coderesource /*we're not running inside a code resource*/

#define Rez true
#define DeRez false
#define SystemSevenOrLater 1

