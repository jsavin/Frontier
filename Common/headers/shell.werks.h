
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

#ifndef shell_werks_include
#define shell_werks_include
#ifdef __MWERKS__
	#if __INTEL__
		#define WIN95VERSION
	#else
		#define MACVERSION 1
	#endif
#endif



		#include "shellheaders.h"
		#pragma once on
		#pragma syspath_once on



#undef fltrialsize
#undef flruntime
#define fldebug 1
#define flnewfeatures 1
#define version42orgreater 1
#define version5orgreater 1
#define flcomponent 1
#define isFrontier 1
#undef dropletcomponent
#undef fliowa
#define threadverbs 1
#define oplanglists 1
#define flregexpverbs 1

	#define noextended 0

#define macBirdRuntime 1
#undef appRunsCards /*for Applet Toolkit, Iowa Runtime is baked in*/
#define iowaRuntimeInApp /*iowa code knows it's in an app*/
#define iowaRuntime /*iowa code knows it's not compiling in Card Editor*/
#define cmdPeriodKillsCard
#define IOAinsideApp /*all the IOA's are baked into the app*/
#undef coderesource /*we're not running inside a code resource*/

#define Rez true
#define DeRez false

#undef SystemSevenOrLater
#define SystemSevenOrLater 1

#define fltracklocaladdresses 1		/* enable code for tracking deleted local addresses */

#endif

