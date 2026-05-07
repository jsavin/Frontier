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
 * GIL: must be called while holding the GIL — accesses roottable and walks
 * the lang.c hashtable structures via hashinversesearch / findnamedtable,
 * and allocates lang values (opnewlist, setaddressvalue) which all assume
 * GIL discipline.
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
 * GIL: must be called while holding the GIL — reads from hleaf's hashtable
 * (lang.c global structure) and allocates lang values (opnewlist,
 * copyvaluerecord, setheapvalue) which all assume GIL discipline.
 *
 * Backs the menu.describe verb.
 */
extern boolean menudata_describe_leaf(hdlhashtable hleaf, tyvaluerecord *vreturned);


/*
 * Headless sibling of meuserselected (Common/source/meprograms.c:256-315).
 *
 * Compiles a UserTalk source handle and runs it synchronously on the
 * calling (GIL-holding) thread. Skips the Mac-UI artifacts
 * (op_get_outlinedata, shellforcemenuadjust, mezoomscriptwindow) and the
 * process-scheduler queue (newprocess/addprocess) that the Mac path uses.
 *
 * Implementation note: the planning doc described this as a
 * scriptbuildtree -> newprocess -> addprocess chain, but the headless
 * build deliberately omits process.c (see frontier-cli/headless_thread_verbs.c
 * for the parallel POSIX-thread / GIL discipline that replaces it). For
 * the REPL palette use case — exec-then-resume-linenoise — synchronous
 * execution via langbuildtree + langruncode matches the existing REPL
 * eval path and keeps the dispatch path unit-testable without scheduler
 * fixtures.
 *
 * Caller is responsible for fetching hScript from the menu leaf's "script"
 * field (e.g. via menu.describe). hScript must be a Pascal-prefixed text
 * handle in the UserTalk shape (typeLAND) — same shape as
 * megetnodelangtext output / outline-extracted langtext / newtexthandle().
 * langbuildtree consumes hScript on every path; the caller must not
 * reference it after the call.
 *
 * Handle privacy: if hScript was extracted from a menu leaf field, the
 * caller must either (a) have held the GIL continuously between extraction
 * and this call, or (b) have made a private copy via copyhandle. Otherwise
 * another GIL-acquiring thread (the GIL is released at langbackgroundtask
 * yield points inside langruncode) could dispose or rewrite the underlying
 * handle while we are mid-execution. The recommended path is to dispatch
 * via the value returned by menudata_describe_leaf, which copyvaluerecord's
 * every field — those copies are independently allocated and safe to hand
 * off across yields.
 *
 * Returns true iff the script compiled cleanly AND ran to completion.
 * Returns false on:
 *   - hScript == nil
 *   - compile failure (langbuildtree rejects the source)
 *   - runtime error during execution (langruncode returns false)
 *
 * GIL: must be called while holding the GIL. Runs user code synchronously,
 * so all kernel-verb side effects observable on return.
 *
 * Future: when async menu dispatch is needed (long-running scripts that
 * should not block linenoise), wrap in headless_spawn_callback_thread.
 *
 * See planning/discussions/repl-slash-menu-implementation-plan.md (sub-PR 2b)
 * and ADR-016.
 */
extern boolean meuserselected_headless(Handle hScript);


/*
 * --------------------------------------------------------------------------
 * Write-side projection helpers (PR 5.5 / ADR-016)
 * --------------------------------------------------------------------------
 *
 * The verbs in tests/headless_menu_verbs.c (the live headless menu dispatcher,
 * since loadfunctionprocessor in Common/source/menuverbs.c is a no-op stub)
 * are written so that any first-arg address pointing INTO system.menus.data
 * triggers a projection write. These helpers are the kernel-side primitives.
 *
 * Bar-name semantics
 * ------------------
 * The "bar name" is the path component immediately under system.menus.data.
 * For an address like @system.menus.data.repltest, the bar name is "repltest".
 * For a deeper address like @system.menus.data.repltest.File.New, the bar
 * name is still "repltest" (the menu/item names are the deeper components).
 *
 * Lazy creation
 * -------------
 * Every helper calls menudata_ensure_root() first and then creates whatever
 * intermediate sub-tables are needed. This is intentional: the projection
 * model (per ADR-016 §"What this means for verbs") replaces the legacy
 * "must call new(menubarType, @bar) first" precondition with "verbs auto-
 * create their sub-table chain on first write." See PR 5.5 commit message
 * for the full rationale.
 *
 * GIL discipline
 * --------------
 * All helpers must be called while holding the GIL. They mutate process-
 * global hashtable state (roottable + descendants) and allocate language
 * values; the same discipline as the read-side helpers above.
 *
 * Tmp-stack ownership
 * -------------------
 * Field values written through these helpers are exempted from the tmp stack
 * before being assigned into the projection sub-tables. Without this the
 * values would be garbage-collected by langtmpstackpop on the next yield
 * point, leaving stale handle references in the table — a use-after-free
 * pattern documented in docs/ARCHITECTURAL_ANTIPATTERNS.md §"Tmp stack
 * ownership". See setexemptaddressvalue in Common/source/langvalue.c for the
 * canonical exemption idiom.
 */


/*
 * Resolve the bar sub-table for bsbarname under system.menus.data, creating
 * it (and the chain) if missing. Returns true with *hbar populated on
 * success.
 *
 * Use this as the first step in any write-side helper that operates on a
 * specific bar.
 */
extern boolean menudata_ensure_bar(bigstring bsbarname, hdlhashtable *hbar);


/*
 * Set system.menus.data.<barname>.installed to flinstalled. Lazy-creates
 * the bar sub-table. Returns true on success.
 *
 * Backs menu.install (flinstalled = true) and menu.remove (flinstalled =
 * false). The boolean is written via setbooleanvalue, which produces a
 * scalar that does not need tmp-stack exemption.
 */
extern boolean menudata_set_installed(bigstring bsbarname, boolean flinstalled);


/*
 * Read system.menus.data.<barname>.installed into *flinstalled. If the bar
 * sub-table doesn't exist, *flinstalled is set false and the function
 * returns true (success: "not installed" is a valid reading of "no record").
 * If the bar exists but the field is missing, *flinstalled is set false
 * (the documented default per ADR-016 §"menu.describe contract" defaults).
 *
 * Backs menu.isInstalled.
 */
extern boolean menudata_get_installed(bigstring bsbarname, boolean *flinstalled);


/*
 * Create or replace a leaf item at system.menus.data.<barname>.<menuname>.
 * <itemname>. Sets the leaf's required fields:
 *   label   -> itemname (the human-visible label defaults to the item key)
 *   script  -> the contents of hscript as a string value (hscript is a
 *              Handle to the UserTalk source text — same shape that
 *              addmenucommandverb's bsscript parameter carries on the
 *              legacy path)
 *   enabled -> true (per ADR-016 §"least-surprising menu item" defaults)
 *
 * If hscript is nil the script field is set to the empty string. If the
 * bar/menu sub-tables don't exist they are created. If a leaf already
 * exists with the same name its fields are overwritten (matching the
 * "replace existing item script" semantic of legacy addmenucommandverb).
 *
 * The caller retains ownership of hscript; this helper reads from it but
 * does not consume it. We make a heap copy for the projection.
 */
extern boolean menudata_add_command(bigstring bsbarname, bigstring bsmenuname,
                                    bigstring bsitemname, Handle hscript);


/*
 * Create an intermediate sub-menu at system.menus.data.<barname>.<menuname>.
 * <itemname>. Unlike menudata_add_command, this leaves the new sub-table
 * empty (no fields) so that subsequent menudata_add_command calls scoped to
 * @bar.<menuname>.<itemname>.<subitem> can populate it as a deeper menu.
 *
 * Backs menu.addSubMenu.
 *
 * Note: ADR-016's projection model is bar -> menu -> item, three levels
 * deep. addSubMenu effectively introduces a fourth level. The leaf-vs-
 * intermediate detection in menudata_list_leaves (ht_has_subtable_child)
 * already handles arbitrary depth, so deeper trees Just Work for menu.list.
 */
extern boolean menudata_add_submenu(bigstring bsbarname, bigstring bsmenuname,
                                    bigstring bsitemname);


/*
 * Delete a leaf or sub-table at system.menus.data.<barname>.<menuname>.
 * <itemname>. If bsitemname is empty, deletes the entire <menuname>
 * sub-tree (used by menu.deleteSubMenu when called with two args). If
 * bsmenuname is also empty, deletes the entire <barname> sub-table.
 *
 * Returns true if a deletion occurred OR the target was already absent
 * (idempotent), false on hard error.
 */
extern boolean menudata_delete_item(bigstring bsbarname, bigstring bsmenuname,
                                    bigstring bsitemname);


/*
 * Update the .script field of an existing leaf at system.menus.data.
 * <barname>.<menuname>.<itemname>. Does NOT create the leaf if absent —
 * returns false in that case (caller should have produced the leaf via
 * menudata_add_command first; setScript is for updates, not creation).
 */
extern boolean menudata_set_script(bigstring bsbarname, bigstring bsmenuname,
                                   bigstring bsitemname, Handle hscript);


/*
 * Update the .cmdkey and .cmdmodifiers fields of an existing leaf. Same
 * "leaf must already exist" semantic as menudata_set_script.
 *
 * cmdmodifiers is reserved for future modifier-key encoding (Cmd, Shift,
 * Option, Ctrl bit-OR); pass 0 to leave unspecified, which matches the
 * documented default for the .cmdmodifiers field per
 * Common/source/menudata_headless.c::menudata_describe_leaf.
 */
extern boolean menudata_set_cmdkey(bigstring bsbarname, bigstring bsmenuname,
                                   bigstring bsitemname, char cmdkey,
                                   byte cmdmodifiers);


/*
 * Resolve an address value to (barname, menuname, itemname, depth) where
 * depth is:
 *   0 = address is NOT under system.menus.data (caller should fall back
 *       to legacy path or no-op)
 *   1 = @system.menus.data.<bar>           (bar identified, no deeper)
 *   2 = @system.menus.data.<bar>.<menu>    (menu sub-table)
 *   3 = @system.menus.data.<bar>.<menu>.<item> (leaf)
 *   4+ = deeper (sub-menus); returned as depth=3 with the deepest two
 *       components in bsmenuname / bsitemname (callers that need full
 *       depth walking should use the address parts directly)
 *
 * On depth==0 the bigstrings are cleared. On depth>=1 they're filled
 * left-to-right; unused slots are empty strings.
 *
 * The address is split on the dot path stored in the address handle,
 * combined with the parent table chain. This routine does NOT mutate
 * the projection — it's read-only address parsing.
 */
extern short menudata_resolve_bar_path(hdlhashtable hparent, bigstring bsname,
                                       bigstring bsbarname,
                                       bigstring bsmenuname,
                                       bigstring bsitemname);

#endif /* menudata_headless_include */
