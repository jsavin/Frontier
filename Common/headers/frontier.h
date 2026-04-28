
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
	2004-10-23 aradke: New global header file, to be included from all source files.

	2006-03-04 aradke: disable MS Visual C++ warning about unknown pragmas
	so it won't complain about "#pragma unused(foo)"
*/
/* 2025-11-30 Codex: Allow portable builds to skip headless stubs when linking real helpers. */

#ifndef __FRONTIER_H__
#define __FRONTIER_H__

// Phase 0.4: Include compatibility layer first (ensures all system types are available)
// This must be included before any other Frontier headers to avoid type definition conflicts
#include "frontier_compat.h"

// Test if compatibility layer was included
#ifdef FRONTIER_COMPAT_INCLUDED
// Compatibility layer included successfully
#else
#error "frontier_compat.h was not included properly"
#endif

#if defined(FRONTIER_HEADLESS)
#include "osincludes_portable.h"   /* portable system headers for headless builds */
#include "../portable/standard_portable.h"  /* portable core types and macros */
#else
#include "osincludes.h"		/* operating system headers */
#endif


#include "frontierdefs.h"	/* global pre-processor defines */

#if defined(FRONTIER_HEADLESS) && (!defined(FRONTIER_ALLOW_PORTABLE_STUBS) || FRONTIER_ALLOW_PORTABLE_STUBS)
#include "headless_stubs.h"
#endif


#endif /*__FRONTIER_H__*/
