/*	$Id$    */

/*
    SPDX-License-Identifier: MIT

    Copyright (c) 2026 Frontier contributors

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

#ifndef menudata_headless_include
#define menudata_headless_include

#ifndef shelltypesinclude
	#include "shelltypes.h"
#endif


/*
 * Pascal string for the "data" rung of system.menus.data.
 *
 * Exposed in the header so production code (menudata_headless.c) and tests
 * (tests/menudata_headless_tests.c) reference the same constant rather than
 * each maintaining its own copy. STR_menus and friends live in
 * stringdefs.h; STR_data has no equivalent there because "data" is generic
 * and only this projection chain needs it.
 */
#define STR_data BIGSTRING("\x04" "data")


/*
 * Lazy-create the system.menus.data table chain. Idempotent.
 *
 * Returns true on success, false if roottable is nil (i.e. called before
 * linksystemtablestructure ran) or if any rung's creation fails. Called at
 * the head of every menu verb so that verbs work on both fresh Virgin.root
 * databases and migrated v7 roots that lack the system.menus subtree (the
 * gap that motivated the 16 skipped tests in
 * tests/integration/test_cases/menu_data_verbs.yaml).
 *
 * GIL: must be called while holding the GIL — accesses globals (roottable
 * and the lang.c hashtable stack via findnamedtable / tablenewsubtable).
 *
 * Reference pattern: Common/source/tablestructure.c::linksystemtablestructure
 * — the gold standard for "find or create system sub-tables." See
 * planning/architectural_decision_records/ADR-016-headless-menu-system-projection.md
 * for the architectural model that motivates this projection.
 */
extern boolean menudata_ensure_root(void);

#endif /* menudata_headless_include */
