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


/*
 * Walk the system.menus.data subtree and build a list of address values
 * pointing at leaf items. A "leaf" is a sub-table whose children are all
 * scalars — i.e. the deepest rung in the app/menu/item chain. Intermediate
 * sub-tables (the app and menu rungs) are walked through but are not
 * themselves returned.
 *
 *   hscope == nil     -> walk the entire system.menus.data subtree
 *   hscope != nil     -> walk only that subtree
 *
 * On success *vreturned holds a freshly-allocated listvaluetype value owned
 * by the caller (dispose with disposevaluerecord). Returns false on hard
 * failure (allocation error). An empty subtree is success with an empty
 * list.
 *
 * Backs the menu.list verb. See ADR-016 for the projection model and
 * planning/discussions/pr2-meuserselected-headless-plan.md for sub-PR 2a.
 */
extern boolean menudata_list_leaves(hdlhashtable hscope, tyvaluerecord *vreturned);


/*
 * Build a 9-field record describing a single leaf item. Field set per
 * ADR-016 §"menu.describe contract":
 *
 *   label         (string, "" if missing)
 *   script        (string, "" if missing)
 *   cmdkey        (char,   '\0' if missing)
 *   cmdmodifiers  (long,   0 if missing)
 *   description   (string, "" if missing)
 *   shortcut      (string, "" if missing)
 *   enabled       (boolean, true if missing)
 *   hidden        (boolean, false if missing)
 *   accepts_args  (boolean, false if missing)
 *
 * The defaults reflect the "least-surprising menu item" baseline: enabled
 * and visible, no accelerator, takes no arguments. Callers don't need to
 * pre-populate every field.
 *
 * On success *vreturned holds a freshly-allocated recordvaluetype value
 * owned by the caller. Returns false on hard failure or if hleaf is nil.
 *
 * Backs the menu.describe verb.
 */
extern boolean menudata_describe_leaf(hdlhashtable hleaf, tyvaluerecord *vreturned);

#endif /* menudata_headless_include */
