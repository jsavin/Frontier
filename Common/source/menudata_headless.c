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
#include "langinternal.h"
#include "langsystem7.h"
#include "langexternal.h"
#include "oplist.h"
#include "strings.h"
#include "tablestructure.h"
#include "tableverbs.h"
#include "stringdefs.h"
#include "menudata_headless.h"
#include "logging.h"

/*
 * STR_data is defined in menudata_headless.h so production code and tests
 * share the same constant. It mirrors the STR_menus form (length-prefix
 * byte + ASCII payload) so the chain reads symmetrically.
 */


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


/*
 * Resolve a value to its underlying hashtable handle if (and only if) the
 * value points to a sub-table — i.e. an external value with table-processor
 * id. Returns true with *ht populated on success; false otherwise.
 *
 * This is how we tell "leaf" from "intermediate rung": at the deepest level
 * (the item rung) every child is a scalar, so this returns false for every
 * child. At intermediate rungs (app, menu) at least one child will be a
 * sub-table.
 */
static boolean value_as_subtable(tyvaluerecord val, hdlhashtable *ht) {

	if (val.valuetype != externalvaluetype)
		return false;

	return langexternalvaltotable(val, ht, HNoNode);
}


/*
 * inversesearch callback used by ht_has_subtable_child.
 *
 * hashinversesearch contract (see Common/source/langhash.c:2474): the visit
 * routine returns true to STOP the walk ("search is over"), false to keep
 * going. We use that to short-circuit on the first sub-table child we find.
 *
 * Sets *(boolean *)refcon to true and returns true (stop walking) the moment
 * we see any child whose value resolves to a sub-table.
 */
static boolean detect_subtable_child(bigstring bsname, hdlhashnode hnode,
                                     tyvaluerecord val, ptrvoid refcon) {
	(void) bsname;
	(void) hnode;

	hdlhashtable hchild = nil;
	if (value_as_subtable(val, &hchild)) {
		*((boolean *) refcon) = true;
		return true; /* stop walking — found a sub-table child */
	}
	return false; /* keep walking */
}


/*
 * Returns true iff at least one entry in ht is itself a sub-table.
 *
 * Implemented by walking with hashinversesearch and short-circuiting in the
 * callback. Per hashinversesearch's contract, the callback returns true to
 * stop the walk; we use that to bail out on the first sub-table child seen.
 */
static boolean ht_has_subtable_child(hdlhashtable ht) {
	boolean found = false;
	bigstring bsfound;

	hashinversesearch(ht, &detect_subtable_child, &found, bsfound);
	return found;
}


/*
 * Refcon for the leaf-collection walk.
 */
typedef struct ty_collect_refcon {
	hdlhashtable hcontainer;  /* table whose direct child this entry is */
	hdllistrecord hlist;       /* destination list of address values */
	boolean error;             /* set on allocation failure */
} ty_collect_refcon;


/* Mutually recursive with walk_visit_entry below. */
static boolean walk_for_leaves(hdlhashtable ht, hdllistrecord hlist);


/*
 * inversesearch callback for one (parent, name, val) entry. If val is a
 * sub-table, we have two cases:
 *
 *   - The sub-table itself has no sub-table children -> it is a leaf;
 *     emit an address (parent, bsname) onto the list and continue.
 *
 *   - Otherwise -> recurse into the sub-table looking for deeper leaves.
 *
 * Scalars are skipped (they're fields of a leaf, not navigation rungs).
 *
 * Per hashinversesearch's contract (see Common/source/langhash.c:2474),
 * returning true means STOP the walk and false means keep going. We always
 * want to visit every direct child, so the only time we return true is on
 * a hard allocation failure where continuing would be unsafe — and we set
 * ctx->error first so the outer driver can surface the failure.
 *
 * Mirrors the (parent, name) shape used by tableverbs.c::tablegetselvisit
 * when pushing address values onto a result list.
 */
static boolean walk_visit_entry(bigstring bsname, hdlhashnode hnode,
                                tyvaluerecord val, ptrvoid refcon) {
	(void) hnode;

	ty_collect_refcon *ctx = (ty_collect_refcon *) refcon;
	hdlhashtable hchild = nil;
	tyvaluerecord addr;

	if (!value_as_subtable(val, &hchild))
		return false; /* scalar, ignore — keep walking */

	if (!ht_has_subtable_child(hchild)) {
		/* hchild is a leaf — emit address (ctx->hcontainer, bsname). */
		if (!setaddressvalue(ctx->hcontainer, bsname, &addr)) {
			ctx->error = true;
			return true; /* stop on allocation failure */
		}
		if (!langpushlistval(ctx->hlist, nil, &addr)) {
			disposevaluerecord(addr, true);
			ctx->error = true;
			return true; /* stop on allocation failure */
		}
		disposevaluerecord(addr, true); /* don't let temp values accumulate */
		return false; /* keep walking sibling entries */
	}

	/* Not a leaf — recurse into hchild. */
	if (!walk_for_leaves(hchild, ctx->hlist)) {
		ctx->error = true;
		return true; /* stop on recursive failure */
	}
	return false; /* keep walking sibling entries */
}


/*
 * Recursive walk: visit each direct child of ht. The actual leaf-vs-recurse
 * decision lives in walk_visit_entry, which has access to (ht, bsname) so
 * setaddressvalue can synthesize an address pointing AT the leaf.
 *
 * If ht is itself nil or empty, returns true with nothing pushed — that's
 * the documented "empty data tree -> empty list" contract.
 */
static boolean walk_for_leaves(hdlhashtable ht, hdllistrecord hlist) {

	ty_collect_refcon ctx;
	bigstring bsfound;

	if (ht == nil)
		return true;

	ctx.hcontainer = ht;
	ctx.hlist = hlist;
	ctx.error = false;

	(void) hashinversesearch(ht, &walk_visit_entry, &ctx, bsfound);

	return !ctx.error;
}


boolean menudata_list_leaves(hdlhashtable hscope, tyvaluerecord *vreturned) {

	hdllistrecord hlist;
	hdlhashtable hroot = hscope;

	/*
	 * If no scope was specified, walk from system.menus.data. Lazy-create
	 * the chain so we never crash on a freshly-migrated v7 root.
	 */
	if (hroot == nil) {
		hdlhashtable hsystem = nil, hmenus = nil;

		if (!menudata_ensure_root())
			return false;

		if (!findnamedtable(roottable, namesystembranch, &hsystem))
			return false;
		if (!findnamedtable(hsystem, STR_menus, &hmenus))
			return false;
		if (!findnamedtable(hmenus, STR_data, &hroot))
			return false;
	}

	if (!opnewlist(&hlist, false)) /* false = list (not record) */
		return false;

	if (!walk_for_leaves(hroot, hlist)) {
		opdisposelist(hlist);
		return false;
	}

	return setheapvalue((Handle) hlist, listvaluetype, vreturned);
}


/*
 * --- menudata_describe_leaf ---
 *
 * Look up each documented field on the leaf hashtable, falling back to the
 * documented default when the field is absent. Build a record value with all
 * 9 fields present (callers can read them unconditionally).
 *
 * Field defaults reflect the "least-surprising menu item":
 *   enabled = true, hidden = false, accepts_args = false, shortcut = "",
 *   description = "", label/script/cmdkey/cmdmodifiers default to empty/zero.
 */

#define BS_label        BIGSTRING("\x05" "label")
#define BS_script       BIGSTRING("\x06" "script")
#define BS_cmdkey       BIGSTRING("\x06" "cmdkey")
#define BS_cmdmodifiers BIGSTRING("\x0c" "cmdmodifiers")
#define BS_description  BIGSTRING("\x0b" "description")
#define BS_shortcut     BIGSTRING("\x08" "shortcut")
#define BS_enabled      BIGSTRING("\x07" "enabled")
#define BS_hidden       BIGSTRING("\x06" "hidden")
#define BS_accepts_args BIGSTRING("\x0c" "accepts_args")


/*
 * Look up bskey in ht. If found, copy the existing value into *out (caller
 * owns nothing extra; we hand back the same handle/scalar that lived in the
 * table — the surrounding record build will copy or pack as needed).
 *
 * Returns true if the lookup succeeded (regardless of value type).
 */
static boolean lookup_field(hdlhashtable ht, bigstring bskey,
                            tyvaluerecord *out) {
	hdlhashnode hnode;
	return hashtablelookup(ht, bskey, out, &hnode);
}


/*
 * Build a list-record entry for one field, falling back to a default. The
 * default-builder lambda is implemented by the calling site since we only
 * need a handful of types here.
 */
static boolean push_field_or(hdllistrecord hlist, ptrstring bskey,
                              hdlhashtable ht, bigstring bslookup,
                              tyvaluerecord defaultval) {
	tyvaluerecord val;
	tyvaluerecord copy;

	if (lookup_field(ht, bslookup, &val)) {
		if (!copyvaluerecord(val, &copy))
			return false;
		if (!langpushlistval(hlist, bskey, &copy)) {
			disposevaluerecord(copy, false);
			return false;
		}
		return true;
	}
	if (!copyvaluerecord(defaultval, &copy))
		return false;
	if (!langpushlistval(hlist, bskey, &copy)) {
		disposevaluerecord(copy, false);
		return false;
	}
	return true;
}


boolean menudata_describe_leaf(hdlhashtable hleaf, tyvaluerecord *vreturned) {

	hdllistrecord hlist;
	tyvaluerecord defemptystring, deftrue, deffalse, defzerochar, defzerolong;
	bigstring bsempty;

	if (hleaf == nil)
		return false;

	/*
	 * Defaults. We keep them on the C stack and copy at push time, since
	 * langpushlistval takes ownership (or near-ownership via copy).
	 */
	clearbytes(bsempty, sizeof(bigstring));
	setemptystring(bsempty);
	if (!setstringvalue(bsempty, &defemptystring))
		return false;
	/*
	 * setbooleanvalue / setcharvalue / setlongvalue are scalar setters that
	 * cannot fail in practice, but we still route their failure paths through
	 * cleanup_defaults so that defemptystring (the only heap-allocated default)
	 * is disposed even if the impossible happens.
	 */
	if (!setbooleanvalue(true, &deftrue))
		goto cleanup_defaults;
	if (!setbooleanvalue(false, &deffalse))
		goto cleanup_defaults;
	if (!setcharvalue('\0', &defzerochar))
		goto cleanup_defaults;
	if (!setlongvalue(0, &defzerolong))
		goto cleanup_defaults;

	if (!opnewlist(&hlist, true)) /* true = record */
		goto cleanup_defaults;

	/*
	 * Field order matches ADR-016 for human readability. Each push copies
	 * the looked-up value (or the default). The order does NOT affect
	 * lookup semantics because rec_field-style consumers index by key.
	 */
	if (!push_field_or(hlist, BIGSTRING("\x05" "label"),       hleaf, BS_label,        defemptystring)) goto fail;
	if (!push_field_or(hlist, BIGSTRING("\x06" "script"),      hleaf, BS_script,       defemptystring)) goto fail;
	if (!push_field_or(hlist, BIGSTRING("\x06" "cmdkey"),      hleaf, BS_cmdkey,       defzerochar))    goto fail;
	if (!push_field_or(hlist, BIGSTRING("\x0c" "cmdmodifiers"),hleaf, BS_cmdmodifiers, defzerolong))    goto fail;
	if (!push_field_or(hlist, BIGSTRING("\x0b" "description"), hleaf, BS_description,  defemptystring)) goto fail;
	if (!push_field_or(hlist, BIGSTRING("\x08" "shortcut"),    hleaf, BS_shortcut,     defemptystring)) goto fail;
	if (!push_field_or(hlist, BIGSTRING("\x07" "enabled"),     hleaf, BS_enabled,      deftrue))        goto fail;
	if (!push_field_or(hlist, BIGSTRING("\x06" "hidden"),      hleaf, BS_hidden,       deffalse))       goto fail;
	if (!push_field_or(hlist, BIGSTRING("\x0c" "accepts_args"),hleaf, BS_accepts_args, deffalse))       goto fail;

	/*
	 * Dispose the local default values now that everything's been copied.
	 */
	disposevaluerecord(defemptystring, false);

	return setheapvalue((Handle) hlist, recordvaluetype, vreturned);

fail:
	opdisposelist(hlist);

cleanup_defaults:
	disposevaluerecord(defemptystring, false);
	return false;
}


/*
 * --------------------------------------------------------------------------
 * Write-side projection helpers (PR 5.5 / ADR-016)
 * --------------------------------------------------------------------------
 *
 * The verbs that mutate menu state (menu.install / menu.addMenuCommand /
 * menu.setScript / etc.) need to mirror their effect into system.menus.data.
 * PR 1 (#575) added the storage; PR 2 (#576) added the read side; this is
 * the write side.
 *
 * Design (per ADR-016 §"What this means for verbs"):
 *   - The verb's first-arg address determines the bar name. For an address
 *     @system.menus.data.<bar>(.<menu>(.<item>)?)?, the bar name is the
 *     path component immediately under system.menus.data.
 *   - Sub-tables are lazy-created. The legacy "must call new(menubarType,
 *     @bar) first" precondition does NOT apply to the projection path;
 *     ADR-016 explicitly chooses sub-table-per-bar over external-per-bar
 *     because the external is a Mac-era artifact (a Handle wrapping the
 *     menubar resource). The headless world doesn't need it.
 *   - All scalar field writes route through setbooleanvalue / setcharvalue /
 *     setlongvalue (which do NOT push to the tmp stack) or via newheapstring
 *     followed by setheapvalue and exemptfromtmpstack — the documented
 *     idiom in docs/ARCHITECTURAL_ANTIPATTERNS.md §"Tmp stack ownership".
 *
 * Field name bigstrings are reused from the describe_leaf section (BS_label
 * etc. above). The "installed" field and other write-only fields get their
 * bigstrings here.
 */

#define BS_installed BIGSTRING("\x09" "installed")


/*
 * Find or create the system.menus.data sub-table. Returns the handle in
 * *hdata. Equivalent to a final findnamedtable after menudata_ensure_root,
 * but consolidated here so write helpers don't each re-implement it.
 */
static boolean find_data_root(hdlhashtable *hdata) {
	hdlhashtable hsystem = nil, hmenus = nil;

	if (!menudata_ensure_root())
		return false;

	if (!findnamedtable(roottable, namesystembranch, &hsystem))
		return false;
	if (!findnamedtable(hsystem, STR_menus, &hmenus))
		return false;
	if (!findnamedtable(hmenus, STR_data, hdata))
		return false;

	return true;
}


boolean menudata_ensure_bar(bigstring bsbarname, hdlhashtable *hbar) {
	hdlhashtable hdata = nil;

	*hbar = nil;

	if (isemptystring(bsbarname))
		return false;

	if (!find_data_root(&hdata))
		return false;

	return ensure_subtable(hdata, bsbarname, hbar);
}


/*
 * Resolve a (parent_table, name) address pair to (barname, menuname,
 * itemname, depth). See menudata_resolve_bar_path docstring in the header
 * for depth semantics.
 *
 * Strategy: walk up from hparent collecting names until we hit
 * system.menus.data, the depth-1 bar slot is found, or we run out of
 * parents (depth=0, address not in system.menus.data).
 *
 * The walk uses prevhashtable for the parent chain. To get a child's name
 * within its parent, we use hashinversesearch with a value-match callback
 * — the same pattern used by langhash.c when reconstructing addresses
 * after a table rename.
 */

typedef struct ty_name_match_refcon {
	hdlhashtable target;       /* the child whose name we want */
	bigstring matched_name;    /* output: filled if found */
	boolean found;
} ty_name_match_refcon;

static boolean find_name_for_child(bigstring bsname, hdlhashnode hnode,
                                   tyvaluerecord val, ptrvoid refcon) {
	(void) hnode;

	ty_name_match_refcon *ctx = (ty_name_match_refcon *) refcon;
	hdlhashtable hchild = nil;

	if (!value_as_subtable(val, &hchild))
		return false; /* keep walking */

	if (hchild != ctx->target)
		return false;

	copystring(bsname, ctx->matched_name);
	ctx->found = true;
	return true; /* stop */
}


/*
 * Walk up from htable building a path of names. Stops when stop_at is
 * reached (returns true) or when we run off the top (returns false).
 *
 * names[0..*depth-1] are filled in TOP-DOWN order (so names[0] is the
 * direct child of stop_at, names[*depth-1] is the deepest). Caller
 * pre-allocates the names array; max_depth is its capacity.
 *
 * Uses (**cursor).parenthashtable for the upward walk. This is the
 * field that tablenewsubtable (Common/source/tablestructure.c:881)
 * and findnamedtable (Common/source/tableops.c:246) set. The
 * sibling field prevhashtable is the LEXICAL chain set only by
 * chainhashtable() during script-stack push and is unrelated to the
 * containment hierarchy our address parser cares about.
 */
static boolean walk_up_to(hdlhashtable htable, hdlhashtable stop_at,
                          bigstring names[], short max_depth, short *depth) {
	hdlhashtable cursor = htable;
	bigstring stack[8]; /* depth-bounded; matches max_depth ceiling */
	short ix = 0;
	short i;

	if (max_depth > 8)
		max_depth = 8;

	*depth = 0;

	while (cursor != nil && cursor != stop_at) {
		hdlhashtable parent = (**cursor).parenthashtable;
		ty_name_match_refcon ctx;
		bigstring scratch;

		if (parent == nil)
			return false;

		ctx.target = cursor;
		setemptystring(ctx.matched_name);
		ctx.found = false;
		(void) hashinversesearch(parent, &find_name_for_child, &ctx, scratch);

		if (!ctx.found)
			return false;

		if (ix >= max_depth)
			return false;

		copystring(ctx.matched_name, stack[ix]);
		ix++;
		cursor = parent;
	}

	if (cursor != stop_at)
		return false;

	/* Reverse the stack into names[] so names[0] is closest to stop_at. */
	for (i = 0; i < ix; i++)
		copystring(stack[ix - 1 - i], names[i]);

	*depth = ix;
	return true;
}


short menudata_resolve_bar_path(hdlhashtable hparent, bigstring bsname,
                                bigstring bsbarname, bigstring bsmenuname,
                                bigstring bsitemname) {
	hdlhashtable hdata = nil;
	bigstring stack[8];
	short stackdepth = 0;
	short total;

	setemptystring(bsbarname);
	setemptystring(bsmenuname);
	setemptystring(bsitemname);

	if (hparent == nil || isemptystring(bsname))
		return 0;

	if (!find_data_root(&hdata))
		return 0;

	/* If hparent IS hdata, the address is exactly @system.menus.data.<name>
	   so name is the bar name (depth 1). */
	if (hparent == hdata) {
		copystring(bsname, bsbarname);
		return 1;
	}

	/* Otherwise walk up from hparent until we hit hdata. The names along
	   the way are the bar / menu / sub-menu components; bsname itself is
	   the deepest leaf-name component. */
	if (!walk_up_to(hparent, hdata, stack, 8, &stackdepth))
		return 0;

	/* stack now holds the path from system.menus.data down to (but not
	   including) the leaf-name. The leaf name is bsname itself. So the
	   total depth of the address is stackdepth + 1.
	     stackdepth=0 + leaf -> depth 1 (bar only)  -- handled above
	     stackdepth=1 + leaf -> depth 2 (bar.menu)
	     stackdepth=2 + leaf -> depth 3 (bar.menu.item)
	     stackdepth=3+        -> deeper (sub-menus); compress to depth 3 */
	total = stackdepth + 1;

	switch (total) {
		case 1:
			copystring(bsname, bsbarname);
			return 1;

		case 2:
			copystring(stack[0], bsbarname);
			copystring(bsname, bsmenuname);
			return 2;

		case 3:
			copystring(stack[0], bsbarname);
			copystring(stack[1], bsmenuname);
			copystring(bsname, bsitemname);
			return 3;

		default:
			/* Deeper than 3: keep bar fixed, surface the deepest two as
			   menu/item. Caller can detect by comparing the returned depth
			   with the address's actual depth if they care; current callers
			   only care about "is this a leaf-shaped address". */
			copystring(stack[0], bsbarname);
			copystring(stack[stackdepth - 1], bsmenuname);
			copystring(bsname, bsitemname);
			return 3;
	}
}


/*
 * Helper: assign a value into a hashtable entry, exempting it from the
 * tmp stack first. The langtmpstack guard shipped with PR 1's storage
 * helpers showed this pattern; we centralise it here so every write-side
 * helper uses the same idiom.
 */
static boolean assign_exempt(hdlhashtable htable, bigstring bskey,
                             tyvaluerecord *val) {
	exemptfromtmpstack(val);
	return hashtableassign(htable, bskey, *val);
}


boolean menudata_set_installed(bigstring bsbarname, boolean flinstalled) {
	hdlhashtable hbar = nil;
	tyvaluerecord val;

	if (!menudata_ensure_bar(bsbarname, &hbar))
		return false;

	if (!setbooleanvalue(flinstalled, &val))
		return false;

	/* Booleans are scalars and don't go on the tmp stack, but routing
	   through assign_exempt keeps the call shape uniform with the string
	   writers below — important for future maintainers reading this code. */
	return assign_exempt(hbar, BS_installed, &val);
}


boolean menudata_get_installed(bigstring bsbarname, boolean *flinstalled) {
	hdlhashtable hdata = nil;
	hdlhashtable hbar = nil;
	tyvaluerecord val;
	hdlhashnode hnode;

	*flinstalled = false;

	if (!find_data_root(&hdata))
		return true; /* no projection at all = "not installed" */

	if (!findnamedtable(hdata, bsbarname, &hbar))
		return true; /* bar absent = not installed */

	if (!hashtablelookup(hbar, BS_installed, &val, &hnode))
		return true; /* field absent = default (not installed) */

	if (val.valuetype != booleanvaluetype)
		return true; /* malformed -> treat as not installed */

	*flinstalled = val.data.flvalue;
	return true;
}


/*
 * Write a single field to a leaf, taking care to exempt and dispose
 * appropriately. setstringvalue puts the heap string on the tmp stack;
 * we exempt before assignment so the value stays alive in the projection.
 */
static boolean write_string_field(hdlhashtable hleaf, bigstring bskey,
                                   bigstring bsvalue) {
	tyvaluerecord val;

	if (!setstringvalue(bsvalue, &val))
		return false;
	return assign_exempt(hleaf, bskey, &val);
}


/*
 * Convert a Handle of script text into a bigstring (truncating if needed)
 * and write it to hleaf.script. Handle-based path is what the legacy
 * addmenucommandverb produces (newtexthandle of bsscript). For projection
 * use we round-trip back to a bigstring because tyvaluerecord stringvaluetype
 * stores the entire string in the heap as one chunk and bigstrings are the
 * common currency of the lang layer.
 */
static boolean write_script_field_from_handle(hdlhashtable hleaf,
                                              Handle hscript) {
	bigstring bs;

	if (hscript == nil) {
		setemptystring(bs);
	}
	else {
		long len = gethandlesize(hscript);
		if (len > 255) /* bigstring length limit */
			len = 255;
		if (len < 0)
			len = 0;
		setstringlength(bs, (byte) len);
		if (len > 0)
			/*
			 * moveleft is the headless-portable wrapper around the
			 * memmove path — see Common/source/memory.c. BlockMoveData
			 * is Mac-only; using it here would break the headless build.
			 */
			moveleft(*hscript, bs + 1, len);
	}

	return write_string_field(hleaf, BS_script, bs);
}


boolean menudata_add_command(bigstring bsbarname, bigstring bsmenuname,
                             bigstring bsitemname, Handle hscript) {
	hdlhashtable hbar = nil;
	hdlhashtable hmenu = nil;
	hdlhashtable hleaf = nil;
	tyvaluerecord val;

	if (isemptystring(bsbarname) || isemptystring(bsmenuname) ||
	    isemptystring(bsitemname))
		return false;

	if (!menudata_ensure_bar(bsbarname, &hbar))
		return false;

	if (!ensure_subtable(hbar, bsmenuname, &hmenu))
		return false;

	if (!ensure_subtable(hmenu, bsitemname, &hleaf))
		return false;

	/* label = itemname (default human-visible label) */
	if (!write_string_field(hleaf, BS_label, bsitemname))
		return false;

	/* script = contents of hscript */
	if (!write_script_field_from_handle(hleaf, hscript))
		return false;

	/* enabled = true (default per ADR-016) */
	if (!setbooleanvalue(true, &val))
		return false;
	if (!assign_exempt(hleaf, BS_enabled, &val))
		return false;

	return true;
}


boolean menudata_add_submenu(bigstring bsbarname, bigstring bsmenuname,
                             bigstring bsitemname) {
	hdlhashtable hbar = nil;
	hdlhashtable hmenu = nil;
	hdlhashtable hsub = nil;

	if (isemptystring(bsbarname) || isemptystring(bsmenuname) ||
	    isemptystring(bsitemname))
		return false;

	if (!menudata_ensure_bar(bsbarname, &hbar))
		return false;

	if (!ensure_subtable(hbar, bsmenuname, &hmenu))
		return false;

	/* Create the sub-menu sub-table; leave it empty for downstream
	   addCommand/addSubMenu calls to populate. */
	return ensure_subtable(hmenu, bsitemname, &hsub);
}


boolean menudata_delete_item(bigstring bsbarname, bigstring bsmenuname,
                             bigstring bsitemname) {
	hdlhashtable hdata = nil;
	hdlhashtable hbar = nil;
	hdlhashtable hmenu = nil;

	if (isemptystring(bsbarname))
		return false;

	if (!find_data_root(&hdata))
		return false;

	if (!findnamedtable(hdata, bsbarname, &hbar))
		return true; /* nothing to delete; success (idempotent) */

	if (isemptystring(bsmenuname)) {
		/* Delete the entire bar sub-tree. */
		bigstring bs;
		copystring(bsbarname, bs);
		return hashtabledelete(hdata, bs);
	}

	if (!findnamedtable(hbar, bsmenuname, &hmenu))
		return true; /* menu absent; success */

	if (isemptystring(bsitemname)) {
		/* Delete the entire menu sub-tree. */
		bigstring bs;
		copystring(bsmenuname, bs);
		return hashtabledelete(hbar, bs);
	}

	/* Delete a specific leaf. hashtabledelete is idempotent on missing
	   keys (returns false), but we treat absence as success. */
	{
		bigstring bs;
		copystring(bsitemname, bs);
		(void) hashtabledelete(hmenu, bs);
		return true;
	}
}


boolean menudata_set_script(bigstring bsbarname, bigstring bsmenuname,
                            bigstring bsitemname, Handle hscript) {
	hdlhashtable hdata = nil;
	hdlhashtable hbar = nil;
	hdlhashtable hmenu = nil;
	hdlhashtable hleaf = nil;

	if (isemptystring(bsbarname) || isemptystring(bsmenuname) ||
	    isemptystring(bsitemname))
		return false;

	if (!find_data_root(&hdata))
		return false;
	if (!findnamedtable(hdata, bsbarname, &hbar))
		return false;
	if (!findnamedtable(hbar, bsmenuname, &hmenu))
		return false;
	if (!findnamedtable(hmenu, bsitemname, &hleaf))
		return false;

	return write_script_field_from_handle(hleaf, hscript);
}


boolean menudata_set_cmdkey(bigstring bsbarname, bigstring bsmenuname,
                            bigstring bsitemname, char cmdkey,
                            byte cmdmodifiers) {
	hdlhashtable hdata = nil;
	hdlhashtable hbar = nil;
	hdlhashtable hmenu = nil;
	hdlhashtable hleaf = nil;
	tyvaluerecord val;

	if (isemptystring(bsbarname) || isemptystring(bsmenuname) ||
	    isemptystring(bsitemname))
		return false;

	if (!find_data_root(&hdata))
		return false;
	if (!findnamedtable(hdata, bsbarname, &hbar))
		return false;
	if (!findnamedtable(hbar, bsmenuname, &hmenu))
		return false;
	if (!findnamedtable(hmenu, bsitemname, &hleaf))
		return false;

	if (!setcharvalue(cmdkey, &val))
		return false;
	if (!assign_exempt(hleaf, BS_cmdkey, &val))
		return false;

	if (!setlongvalue((long) cmdmodifiers, &val))
		return false;
	if (!assign_exempt(hleaf, BS_cmdmodifiers, &val))
		return false;

	return true;
}


/*
 * Headless sibling of meuserselected (Common/source/meprograms.c:256-315).
 *
 * Architectural note: the planning doc described this function as a
 * scriptbuildtree -> newprocess -> addprocess chain mirroring the Mac
 * meuserselected. That recipe assumes process.c is linked in. It isn't:
 * the headless build (frontier-cli + unit-test runtime) deliberately omits
 * process.c — see frontier-cli/headless_thread_verbs.c:336 ("In headless
 * mode, process.c is not compiled — there are no process handles") and
 * the parallel POSIX-thread + GIL machinery in headless_thread_verbs.c.
 *
 * For the REPL palette use case (plan §"REPL integration point"), the
 * caller wants exec-then-resume-linenoise semantics. Synchronous execution
 * on the GIL-holding thread is both simpler and more correct: it matches
 * the existing REPL eval path (langrun*) and avoids spawning a thread
 * just to immediately wait for it.
 *
 * Steps:
 *   1. langbuildtree turns the Pascal-prefixed UserTalk source handle into
 *      a compiled tree. Consumes hScript regardless of outcome (see
 *      lang.c:534 — langcompiletext disposes htext).
 *   2. langruncode executes the tree synchronously on the calling thread.
 *      The result is discarded; menu actions are statements, not
 *      expressions, so the value carries no caller-visible meaning.
 *   3. langdisposetree releases the compiled tree.
 *
 * Errors surface through the existing langerrormessage callback chain —
 * same path as REPL eval. We do not push a custom error callback because
 * the Mac mescripterrorroutine is bound to shell globals (op outlines, the
 * menu-editor window) we don't have, and a nil callback would no-op.
 *
 * GIL: caller must hold the GIL — we touch the global hashtable stack via
 * langbuildtree's tree-construction path and run user code that may call
 * any kernel verb.
 *
 * Future: when the host eventually wants async menu dispatch (e.g. a long-
 * running script that should not block linenoise), wrap this in
 * headless_spawn_callback_thread. That's a follow-up; the palette MVP
 * needs synchronous semantics.
 */
boolean meuserselected_headless(Handle hScript) {

	hdltreenode hcode = nil;
	tyvaluerecord vresult;
	boolean fl;

	if (hScript == nil)
		return false;

	/*
	 * langbuildtree consumes hScript whether it succeeds or fails (see
	 * lang.c:534 -> langcompiletext, which disposes htext on every path).
	 * After this call we must not reference hScript again.
	 *
	 * Second arg "fllinebased" matches scripts.c:286's call into
	 * langbuildtree for the typeLAND signature: true = treat newlines as
	 * statement terminators, which is how outline-extracted UserTalk is
	 * always shaped.
	 */
	fl = langbuildtree(hScript, true, &hcode);

	if (!fl)
		return false; /* syntax error */

	/* Compilation produced no error; clear any stale error state.
	   Mirrors meprograms.c:295's langerrorclear() after the compile. */
	langerrorclear();

	initvalue(&vresult, novaluetype);

	fl = langruncode(hcode, nil, &vresult);

	disposevaluerecord(vresult, false);
	langdisposetree(hcode);

	return fl;
}
