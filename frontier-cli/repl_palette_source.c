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
 * repl_palette_source.c - ODB adapter that satisfies palette_menu_source_t
 * by walking system.menus.data.<menubar>.<menu>.<item>.
 *
 * Tree shape (per ADR-016):
 *
 *   system.menus.data
 *      └─ <menubar>             (e.g. "repl")
 *          └─ <menu>             (e.g. "REPL", "Help")
 *              └─ <item>         (e.g. "Help", "Exit")
 *                  ├─ label      (string scalar)
 *                  ├─ script     (string scalar — UserTalk source)
 *                  ├─ cmdkey     (char)
 *                  └─ ...        (other documented fields, see
 *                                 menudata_describe_leaf in
 *                                 Common/source/menudata_headless.c)
 *
 * Top-level menus and their items are sub-tables. The lowest rung's children
 * are all scalars — that's what makes it a "leaf" (per menudata_list_leaves).
 *
 * Handle privacy contract
 * -----------------------
 * Per palette.h::palette_item_t, script_handle MUST remain valid across the
 * full lifetime of the palette AND across GIL yields, AND survive
 * palette_close so the host can dispatch on PALETTE_DONE_EXECUTE.
 *
 * We satisfy this by route (a) (per palette.h): copyhandle the script into
 * adapter-private storage, keyed by the item's hashtable handle so re-
 * describes of the same item reuse the existing copy instead of growing
 * the cache. Free wholesale in repl_palette_source_dispose. The adapter
 * never aliases the underlying ODB handle into the palette — every
 * palette_item_t.script_handle returned is an independently-allocated handle
 * owned by this adapter.
 *
 * Why route (a) and not (b)? While menudata_describe_leaf returns a
 * deeply-copied record (so the script field's handle is independently
 * allocated relative to the ODB hashtable), the record itself must be
 * disposed promptly — otherwise we leak record envelopes for every
 * keystroke that re-fetches an item. Route (a) gives us symmetric ownership:
 * one copy in, one disposehandle out.
 *
 * Cache shape: the keyed-by-item-handle design (see ty_script_cache_entry)
 * bounds growth by the number of UNIQUE items in the menubar — typically
 * <50 — rather than by the number of describe calls (which can easily
 * exceed 256 in a single session of arrow-key navigation).
 *
 * Threading
 * ---------
 * All entry points hold the GIL — they read roottable / hashtable globals
 * and allocate lang values. The vtable is documented as GIL-required (see
 * palette.h's per-function annotations).
 */

#include "frontier.h"
#include "standard.h"
#include "shelltypes.h"
#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "langinternal.h"
#include "langexternal.h"
#include "langsystem7.h"
#include "oplist.h"
#include "tablestructure.h"
#include "tableverbs.h"
#include "stringdefs.h"

#include "menudata_headless.h"

#include "palette.h"
#include "repl_palette_source.h"

#include <stdlib.h>
#include <string.h>


/* ------------------------------------------------------------ */
/*  adapter context                                              */
/* ------------------------------------------------------------ */

/*
 * Cap matches the maximum number of distinct leaf items the headless menu
 * tree can practically host: bar/menu/item is 3-deep, and the palette renders
 * (and thus describes) at most one item-list at a time. 256 unique leaves
 * across a session is enormous headroom — typical menubars have fewer than
 * 50 leaves total. This is a defensive ceiling, not a performance budget.
 */
#define REPL_PS_MAX_CACHED_SCRIPTS 256

/*
 * Per-item cache slot: keyed by the item's hashtable handle (which is stable
 * for the menubar's lifetime — repl_palette_source_init validates the bar and
 * the underlying ODB handles do not move while the palette is open). Keying
 * on the handle pointer lets re-describes of the same item re-use the
 * existing copy instead of allocating a new one.
 *
 * Why keyed cache (option b) rather than "only cache the pick" (option a):
 * palette_item_t::script_handle has a strict lifetime contract (palette.h
 * L109-128) — it MUST remain valid for the entire palette session AND
 * across GIL yields. The palette aliases the pointer into exec_script
 * the moment a leaf is highlighted (palette.c:454), and the user might
 * navigate away and back before pressing Enter. Lazy-on-DONE_EXECUTE
 * would race that: the second navigation away would invalidate
 * exec_script's storage. Keying on item identity avoids the race
 * entirely while still capping growth.
 */
typedef struct ty_script_cache_entry {
	hdlhashtable hitem;                 /* key — item subtable handle */
	Handle hscript;                     /* value — copy of script body */
} ty_script_cache_entry;

typedef struct ty_repl_palette_source_ctx {
	bigstring bsmenubar_name;          /* e.g. "\x04" "repl" */
	/* Adapter-owned copies of script handles, keyed by item handle.
	   Freed wholesale in dispose so the caller never has to. */
	ty_script_cache_entry cached_scripts[REPL_PS_MAX_CACHED_SCRIPTS];
	int cached_scripts_count;
	boolean initialised;                /* trips false in dispose so a second
	                                       dispose is a no-op */
} ty_repl_palette_source_ctx;


/* ------------------------------------------------------------ */
/*  helpers                                                      */
/* ------------------------------------------------------------ */

/* Find system.menus.data.<bar>; returns nil if missing. */
static hdlhashtable resolve_menubar(const ty_repl_palette_source_ctx *ctx) {
	hdlhashtable hsystem = nil, hmenus = nil, hdata = nil, hbar = nil;

	if (roottable == nil)
		return nil;

	if (!findnamedtable(roottable, namesystembranch, &hsystem))
		return nil;
	if (!findnamedtable(hsystem, STR_menus, &hmenus))
		return nil;
	if (!findnamedtable(hmenus, STR_data, &hdata))
		return nil;
	if (!findnamedtable(hdata, ctx->bsmenubar_name, &hbar))
		return nil;
	return hbar;
}


/* hashinversesearch callback shared by count-children and pick-Nth. */
typedef struct ty_walk_state {
	int seen;             /* how many subtable entries we've visited */
	int target_index;     /* index we want, -1 to just count */
	hdlhashtable hresult; /* set when target_index reached */
	bigstring bsresult;   /* name of the picked entry */
} ty_walk_state;


/* Returns true to STOP per hashinversesearch. */
static boolean walk_cb(bigstring bsname, hdlhashnode hnode,
                       tyvaluerecord val, ptrvoid refcon) {
	(void) hnode;

	ty_walk_state *st = (ty_walk_state *) refcon;
	hdlhashtable hchild = nil;

	if (val.valuetype != externalvaluetype)
		return false;
	if (!langexternalvaltotable(val, &hchild, HNoNode))
		return false;
	if (hchild == nil)
		return false;

	if (st->target_index >= 0 && st->seen == st->target_index) {
		st->hresult = hchild;
		copystring(bsname, st->bsresult);
		return true; /* found — stop */
	}
	st->seen++;
	return false; /* keep walking */
}


/* Count direct subtable children of ht. */
static int count_subtable_children(hdlhashtable ht) {
	ty_walk_state st;
	bigstring bsfound;

	if (ht == nil)
		return 0;

	memset(&st, 0, sizeof(st));
	st.target_index = -1; /* count-only mode */
	(void) hashinversesearch(ht, &walk_cb, &st, bsfound);
	return st.seen;
}


/* Get the Nth subtable child of ht (0-based). Returns nil if out of range. */
static hdlhashtable nth_subtable_child(hdlhashtable ht, int idx,
                                       bigstring bsout_name) {
	ty_walk_state st;
	bigstring bsfound;

	if (bsout_name != nil)
		setemptystring(bsout_name);

	if (ht == nil || idx < 0)
		return nil;

	memset(&st, 0, sizeof(st));
	st.target_index = idx;
	(void) hashinversesearch(ht, &walk_cb, &st, bsfound);

	if (st.hresult != nil && bsout_name != nil)
		copystring(st.bsresult, bsout_name);

	return st.hresult;
}


/*
 * Look up or insert a per-item script handle copy. Keyed by the item
 * subtable handle so re-describes of the same item return the already-
 * cached copy rather than allocating a new one. This bounds the cache
 * by the number of UNIQUE items in the menubar, not by the number of
 * describe calls — the latter can easily exceed 256 in a single
 * session as the user arrows through menus.
 *
 * Returns the cached handle, or nil on failure (true cache exhaustion
 * — more unique items than the slot count — or copy failure).
 */
static Handle cache_script_handle(ty_repl_palette_source_ctx *ctx,
                                  hdlhashtable hitem,
                                  Handle horig) {
	Handle hcopy = nil;
	int i;

	if (horig == nil || hitem == nil)
		return nil;

	/* Hit: same item already cached — reuse the copy. */
	for (i = 0; i < ctx->cached_scripts_count; i++) {
		if (ctx->cached_scripts[i].hitem == hitem)
			return ctx->cached_scripts[i].hscript;
	}

	/* Miss: insert a new entry. */
	if (ctx->cached_scripts_count >= REPL_PS_MAX_CACHED_SCRIPTS)
		return nil;
	if (!copyhandle(horig, &hcopy))
		return nil;

	ctx->cached_scripts[ctx->cached_scripts_count].hitem = hitem;
	ctx->cached_scripts[ctx->cached_scripts_count].hscript = hcopy;
	ctx->cached_scripts_count++;
	return hcopy;
}


/* Convert a Pascal bigstring to a NUL-terminated C string into outbuf. */
static void bs_to_cstr(const bigstring bs, char *outbuf, size_t cap) {
	size_t n;
	if (cap == 0)
		return;
	n = (size_t) stringlength(bs);
	if (n >= cap)
		n = cap - 1;
	memcpy(outbuf, &bs[1], n);
	outbuf[n] = '\0';
}


/* ------------------------------------------------------------ */
/*  vtable callbacks                                             */
/* ------------------------------------------------------------ */

static int cb_count_menus(void *vctx) {
	ty_repl_palette_source_ctx *ctx = (ty_repl_palette_source_ctx *) vctx;
	hdlhashtable hbar;

	if (ctx == nil)
		return 0;
	hbar = resolve_menubar(ctx);
	return count_subtable_children(hbar);
}


static bool cb_menu_describe(void *vctx, int menu_index,
                             char *out_label, size_t label_cap,
                             char *out_hotkey) {
	ty_repl_palette_source_ctx *ctx = (ty_repl_palette_source_ctx *) vctx;
	hdlhashtable hbar = nil;
	hdlhashtable hmenu = nil;
	bigstring bsname;

	if (out_label != nil && label_cap > 0)
		out_label[0] = '\0';
	if (out_hotkey != nil)
		*out_hotkey = '\0';

	if (ctx == nil)
		return false;

	hbar = resolve_menubar(ctx);
	if (hbar == nil)
		return false;

	hmenu = nth_subtable_child(hbar, menu_index, bsname);
	if (hmenu == nil)
		return false;

	if (out_label != nil)
		bs_to_cstr(bsname, out_label, label_cap);

	/*
	 * Future: read a "hotkey" or "cmdkey" field from the menu sub-table
	 * itself. For PR 6 we leave hotkey unset — palette.c falls back to
	 * first-letter matching and the menubar still navigates fine.
	 */
	return true;
}


/*
 * The palette's contract is that for top-level menus parent_opaque == NULL
 * and menu_index selects which one. For deeper levels parent_opaque is the
 * void* token we returned in palette_item_t.opaque. We never set
 * is_submenu=true for any leaf in PR 6 (the headless tree is always
 * 3-deep — bar/menu/leaf — and the palette only walks leaves), so
 * parent_opaque should always be NULL coming back. Defensive: if a caller
 * does pass parent_opaque, treat it as the menu hashtable handle.
 */
static hdlhashtable resolve_menu_for_items(ty_repl_palette_source_ctx *ctx,
                                           int menu_index,
                                           void *parent_opaque) {
	hdlhashtable hbar;

	if (parent_opaque != nil)
		return (hdlhashtable) parent_opaque;

	hbar = resolve_menubar(ctx);
	if (hbar == nil)
		return nil;
	return nth_subtable_child(hbar, menu_index, nil);
}


static int cb_item_count(void *vctx, int menu_index, void *parent_opaque) {
	ty_repl_palette_source_ctx *ctx = (ty_repl_palette_source_ctx *) vctx;
	hdlhashtable hmenu;

	if (ctx == nil)
		return 0;
	hmenu = resolve_menu_for_items(ctx, menu_index, parent_opaque);
	return count_subtable_children(hmenu);
}


/* Locate the "script" field inside a record-typed value and return its
   underlying string handle (which is an independently-allocated copy from
   menudata_describe_leaf). Caller must NOT dispose this handle directly —
   it lives inside the record envelope which the caller will dispose. */
static Handle script_handle_from_record(tyvaluerecord rec) {
	hdllistrecord hlist;
	long n, i;
	bigstring bsfound;
	tyvaluerecord val;
	bigstring bskey;

	if (rec.valuetype != recordvaluetype)
		return nil;
	hlist = rec.data.recordvalue;

	copyctopstring("script", bskey);
	n = opcountlistitems(hlist);
	for (i = 1; i <= n; i++) {
		if (!getnthlistval(hlist, i, bsfound, &val))
			continue;
		if (!equalstrings(bsfound, bskey))
			continue;
		if (val.valuetype != stringvaluetype)
			return nil;
		return (Handle) val.data.stringvalue;
	}
	return nil;
}


static byte char_field_from_record(tyvaluerecord rec, const char *key) {
	hdllistrecord hlist;
	long n, i;
	bigstring bsfound;
	tyvaluerecord val;
	bigstring bskey;

	if (rec.valuetype != recordvaluetype)
		return '\0';
	hlist = rec.data.recordvalue;

	copyctopstring((char *)key, bskey);
	n = opcountlistitems(hlist);
	for (i = 1; i <= n; i++) {
		if (!getnthlistval(hlist, i, bsfound, &val))
			continue;
		if (!equalstrings(bsfound, bskey))
			continue;
		if (val.valuetype == charvaluetype)
			return val.data.chvalue;
	}
	return '\0';
}


static boolean bool_field_from_record(tyvaluerecord rec, const char *key,
                                      boolean defval) {
	hdllistrecord hlist;
	long n, i;
	bigstring bsfound;
	tyvaluerecord val;
	bigstring bskey;

	if (rec.valuetype != recordvaluetype)
		return defval;
	hlist = rec.data.recordvalue;

	copyctopstring((char *)key, bskey);
	n = opcountlistitems(hlist);
	for (i = 1; i <= n; i++) {
		if (!getnthlistval(hlist, i, bsfound, &val))
			continue;
		if (!equalstrings(bsfound, bskey))
			continue;
		if (val.valuetype == booleanvaluetype)
			return val.data.flvalue;
	}
	return defval;
}


static void string_field_to_cstr(tyvaluerecord rec, const char *key,
                                 char *out, size_t cap) {
	hdllistrecord hlist;
	long n, i;
	bigstring bsfound;
	tyvaluerecord val;
	bigstring bskey;
	bigstring bsval;

	if (cap == 0)
		return;
	out[0] = '\0';

	if (rec.valuetype != recordvaluetype)
		return;
	hlist = rec.data.recordvalue;

	copyctopstring((char *)key, bskey);
	n = opcountlistitems(hlist);
	for (i = 1; i <= n; i++) {
		if (!getnthlistval(hlist, i, bsfound, &val))
			continue;
		if (!equalstrings(bsfound, bskey))
			continue;
		if (val.valuetype != stringvaluetype)
			return;
		pullstringvalue(&val, bsval);
		bs_to_cstr(bsval, out, cap);
		return;
	}
}


static bool cb_item_describe(void *vctx, int menu_index, void *parent_opaque,
                             int item_index, palette_item_t *out) {
	ty_repl_palette_source_ctx *ctx = (ty_repl_palette_source_ctx *) vctx;
	hdlhashtable hmenu;
	hdlhashtable hitem;
	bigstring bsitem_name;
	tyvaluerecord rec;
	Handle hscript_in_record;
	Handle hcached;

	if (out == nil)
		return false;

	memset(out, 0, sizeof(*out));
	out->enabled = true; /* default per ADR-016 */

	if (ctx == nil)
		return false;

	hmenu = resolve_menu_for_items(ctx, menu_index, parent_opaque);
	if (hmenu == nil)
		return false;

	hitem = nth_subtable_child(hmenu, item_index, bsitem_name);
	if (hitem == nil)
		return false;

	/*
	 * Use menudata_describe_leaf so default-handling (enabled=true,
	 * hidden=false, accepts_args=false) stays in one place instead of being
	 * re-implemented here.
	 */
	if (!menudata_describe_leaf(hitem, &rec))
		return false;

	/* label: prefer the record's "label" field (which already defaults to
	   "" if missing). If that's still empty, fall back to the item's own
	   subtable name so the palette has SOMETHING to draw. */
	string_field_to_cstr(rec, "label", out->label, sizeof(out->label));
	if (out->label[0] == '\0')
		bs_to_cstr(bsitem_name, out->label, sizeof(out->label));

	string_field_to_cstr(rec, "description", out->description,
	                     sizeof(out->description));

	out->shortcut = (char) char_field_from_record(rec, "cmdkey");
	out->enabled = bool_field_from_record(rec, "enabled", true) ? true : false;
	out->hidden  = bool_field_from_record(rec, "hidden", false) ? true : false;
	out->accepts_args = bool_field_from_record(rec, "accepts_args", false)
	                    ? true : false;

	/*
	 * For PR 6, every leaf in the headless tree is a script-bearing item.
	 * The palette tree is exactly 3 deep (bar/menu/leaf), so we never set
	 * is_submenu=true. opaque is unused for leaves; leave as NULL.
	 */
	out->is_submenu = false;
	out->opaque = nil;

	hscript_in_record = script_handle_from_record(rec);
	hcached = cache_script_handle(ctx, hitem, hscript_in_record);
	out->script_handle = hcached;

	disposevaluerecord(rec, false);

	/* If we couldn't cache the script handle, fail the describe — the
	   palette would otherwise PALETTE_DONE_EXECUTE with a NULL handle and
	   the caller would silently drop the dispatch. */
	if (hscript_in_record != nil && hcached == nil)
		return false;

	return true;
}


/* ------------------------------------------------------------ */
/*  public API                                                   */
/* ------------------------------------------------------------ */

bool repl_palette_source_init(palette_menu_source_t *out) {
	return repl_palette_source_init_for(out, REPL_PALETTE_DEFAULT_MENUBAR);
}


bool repl_palette_source_init_for(palette_menu_source_t *out,
                                  const char *menubar_name) {
	ty_repl_palette_source_ctx *ctx = nil;
	hdlhashtable hbar = nil;

	if (out == nil || menubar_name == nil)
		return false;

	memset(out, 0, sizeof(*out));

	ctx = (ty_repl_palette_source_ctx *) calloc(1, sizeof(*ctx));
	if (ctx == nil)
		return false;

	copyctopstring((char *)menubar_name, ctx->bsmenubar_name);
	ctx->initialised = true;

	/*
	 * Validate the menubar exists. Returning false here lets the caller
	 * decide whether to install a stub source (no slash menu) versus erroring
	 * out. Lazy-create system.menus.data so this works on a freshly-migrated
	 * v7 root, but DO NOT lazy-create the menubar itself — its absence is
	 * the host's signal that no menubar was installed.
	 */
	if (!menudata_ensure_root()) {
		free(ctx);
		return false;
	}
	hbar = resolve_menubar(ctx);
	if (hbar == nil) {
		free(ctx);
		return false;
	}

	out->ctx = ctx;
	out->count_menus = cb_count_menus;
	out->menu_describe = cb_menu_describe;
	out->item_count = cb_item_count;
	out->item_describe = cb_item_describe;

	return true;
}


void repl_palette_source_dispose(palette_menu_source_t *src) {
	ty_repl_palette_source_ctx *ctx;
	int i;

	if (src == nil)
		return;
	ctx = (ty_repl_palette_source_ctx *) src->ctx;
	if (ctx == nil)
		return;
	if (!ctx->initialised) {
		/* Defensive: if init failed, ctx might still be hanging off src.
		   Treat as no-op. */
		src->ctx = nil;
		return;
	}

	/* Free every cached script handle. The host's call site copies the
	   handle into its own ownership before invoking dispose (so the
	   eventual disposehandle there is on a separate copy), making every
	   cached entry ours to free here. */
	for (i = 0; i < ctx->cached_scripts_count; i++) {
		if (ctx->cached_scripts[i].hscript != nil) {
			disposehandle(ctx->cached_scripts[i].hscript);
			ctx->cached_scripts[i].hscript = nil;
		}
		ctx->cached_scripts[i].hitem = nil;
	}
	ctx->cached_scripts_count = 0;

	ctx->initialised = false;
	free(ctx);

	/* Zero the source struct so a second dispose is a no-op (the next call
	   will see ctx == NULL via src->ctx). */
	memset(src, 0, sizeof(*src));
}
