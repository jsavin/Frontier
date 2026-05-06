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

/*
 * menudata_headless.c
 *
 * Lazy creation of the system.menus.data table chain for headless hosts.
 *
 * Background: in v7-migrated roots, system.menus may be absent entirely.
 * The legacy Mac code path tolerated this because menu state lived in
 * resource-fork outlines and only the dynamic Tools sub-tree relied on
 * system.menus.data. Headless hosts (the CLI/REPL today, future native UI
 * hosts later) keep the canonical menu state in the ODB at
 * system.menus.data.<app>.<menu>.<item> — see ADR-016 for the 5-layer
 * projection model. This module ensures that path is reachable on every
 * menu-verb entry without forcing a separate migration step.
 *
 * Reference pattern: tablestructure.c::linksystemtablestructure shows the
 * canonical "find or create system sub-tables" idiom — findnamedtable for
 * the lookup, tablenewsubtable for the create. We mirror it here for the
 * menus and data rungs.
 */

#include "frontier.h"
#include "standard.h"
#include "shelltypes.h"
#include "memory.h"
#include "lang.h"
#include "strings.h"
#include "tablestructure.h"
#include "tableverbs.h"
#include "stringdefs.h"

#include "menudata_headless.h"

/*
 * Inline pascal-string for the "data" rung. STR_data is not a pre-defined
 * constant in stringdefs.h; this literal mirrors the STR_menus form
 * (length-prefix byte + ASCII payload) so the chain reads symmetrically.
 */
#define STR_data BIGSTRING("\x04" "data")


/*
 * Find an existing sub-table by name, or create it if missing.
 *
 * Mirrors the per-rung pattern in linksystemtablestructure: try
 * findnamedtable first, fall back to tablenewsubtable. Returns true if the
 * table is reachable on exit (whether it was found or created).
 */
static boolean ensure_subtable(hdlhashtable hparent, bigstring bsname,
                               hdlhashtable *hresult) {
	*hresult = nil;

	if (findnamedtable(hparent, bsname, hresult))
		return true;

	*hresult = nil;

	if (!tablenewsubtable(hparent, bsname, hresult))
		return false;

	return true;
}


boolean menudata_ensure_root(void) {

	/*
	 * Walk system -> menus -> data, creating each rung if missing.
	 *
	 * roottable is the process-global hashtable established by
	 * inittablestructure() at startup. If it's still nil here, we have been
	 * called before the kernel finished bringing up the table structure;
	 * return false rather than crash so the caller can degrade gracefully.
	 */

	if (roottable == nil)
		return false;

	hdlhashtable hsystem = nil;
	if (!ensure_subtable(roottable, namesystembranch, &hsystem))
		return false;

	hdlhashtable hmenus = nil;
	if (!ensure_subtable(hsystem, STR_menus, &hmenus))
		return false;

	hdlhashtable hdata = nil;
	if (!ensure_subtable(hmenus, STR_data, &hdata))
		return false;

	return true;
}
