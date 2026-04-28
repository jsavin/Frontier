
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

#ifndef shellcoreinclude
#define shellcoreinclude

/*
shellcore.h - Core shell types safe for both GUI and headless builds

This header contains shell-level types and definitions that are used by both
Mac GUI builds and headless builds. These types are part of the core runtime
and don't depend on Mac-specific GUI functionality.

For Mac GUI-specific shell functions and types, see shellprivate.h.

Extracted as part of ADR-005 (thread-local parameter state migration) to
establish clean separation between portable runtime core and GUI layer,
supporting the collaborative ODB foundation.
*/

#define ctglobals 32 /*we can remember globals up to ctglobals levels deep*/

/* Forward declare hdlhashtable to avoid circular dependency */
#ifndef hdlhashtable
	typedef struct tyhashtable **hdlhashtable;
#endif

/* Entry in the globals stack - saves both window and hash table context */
#pragma pack(2)
typedef struct tyglobalsstackentry {
	WindowPtr window;           /* Window context (void* in headless builds) */
	hdlhashtable hashtable;     /* Hash table context (fixes Issue #262 / 27-year-old bug) */
} tyglobalsstackentry;
#pragma options align=reset

#pragma pack(2)
typedef struct tyglobalsstack {

	short top;

	tyglobalsstackentry stack [ctglobals]; /* Stack entries with window + hashtable */
	} tyglobalsstack;
#pragma options align=reset

#endif /* shellcoreinclude */
