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
 *      +-- <menubar>             (e.g. "repl")
 *          +-- <menu>             (e.g. "REPL", "Help")
 *              +-- <item>         (e.g. "Help", "Exit")
 *                  +-- label      (string scalar)
 *                  +-- script     (string scalar -- UserTalk source)
 *                  +-- cmdkey     (char)
 *                  +-- ...        (other documented fields, see
 *                                  menudata_describe_leaf in
 *                                  Common/source/menudata_headless.c)
 *
 * Top-level menus and their items are sub-tables. The lowest rung's children
 * are all scalars -- that's what makes it a "leaf" (per menudata_list_leaves).
 *
 * Multi-bar adapter
 * -----------------
 * ty_repl_multi_source_ctx holds a sorted array of ty_bar_slot entries, one
 * per installed bar. Global menu indices are decomposed into (bar_idx,
 * local_menu_idx) by decompose_menu_index, which walks the slot array summing
 * menu_count values until the target is reached. This allows the vtable
 * callbacks to remain O(B) in the number of bars (typically <= 16) rather
 * than walking ODB on every index operation.
 *
 * Overflow UX
 * -----------
 * When the union of all installed bars' menus exceeds term_cols, palette.c
 * truncates at the right edge. Overflow UX (ellipsization, horizontal scroll)
 * is deferred per issue #677.
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
 * never aliases the underlying ODB handle into the palette -- every
 * palette_item_t.script_handle returned is an independently-allocated handle
 * owned by this adapter.
 *
 * Why route (a) and not (b)? While menudata_describe_leaf returns a
 * deeply-copied record (so the script field's handle is independently
 * allocated relative to the ODB hashtable), the record itself must be
 * disposed promptly -- otherwise we leak record envelopes for every
 * keystroke that re-fetches an item. Route (a) gives us symmetric ownership:
 * one copy in, one disposehandle out.
 *
 * Cache shape: the keyed-by-item-handle design (see ty_script_cache_entry)
 * bounds growth by the number of UNIQUE items in the menubar -- typically
 * <50 -- rather than by the number of describe calls (which can easily
 * exceed 256 in a single session of arrow-key navigation).
 *
 * Threading
 * ---------
 * All entry points hold the GIL -- they read roottable / hashtable globals
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

#include "../Common/headers/logging.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>


/* ------------------------------------------------------------ */
/*  constants                                                    */
/* ------------------------------------------------------------ */

/*
 * Maximum number of distinct installed menubars the adapter will enumerate.
 * Practically, a terminal session rarely has more than a handful. 16 is
 * generous headroom without imposing measurable stack cost.
 */
#define REPL_PS_MAX_BARS 16

/*
 * Cap matches the maximum number of distinct leaf items the headless menu
 * tree can practically host: bar/menu/item is 3-deep, and the palette renders
 * (and thus describes) at most one item-list at a time. 256 unique leaves
 * across a session is enormous headroom -- typical menubars have fewer than
 * 50 leaves total. This is a defensive ceiling, not a performance budget.
 */
#define REPL_PS_MAX_CACHED_SCRIPTS 256


/* ------------------------------------------------------------ */
/*  legacy prefix-code parsing (mereducemenucodes parity)        */
/* ------------------------------------------------------------ */

/*
 * C-string port of Common/source/menubar.c::mereducemenucodes. Operates on a
 * NUL-terminated label in place. See the header for the full contract; the
 * order of operations (separator, then '(' disable, then '!' check) mirrors
 * the legacy routine exactly so behavior matches the Mac menu code path.
 */
void palette_reduce_menu_codes(char *label, bool *enabled, bool *checked,
                               bool *is_separator) {
	size_t len;

	if (label == nil)
		return;

	len = strlen(label);

	/*
	 * Separator: a line that is EXACTLY "-" (length 1). Legacy:
	 *   *flenabled = (stringlength > 1) || (char[0] != '-');
	 * i.e. a lone '-' is the only disabled-by-dash case. We additionally
	 * flag it as a separator so the renderer can draw a divider.
	 */
	if (len == 1 && label[0] == '-') {
		if (enabled != nil)
			*enabled = false;
		if (is_separator != nil)
			*is_separator = true;
		return; /* '(' and '!' handling below never applies to "-" */
	}

	/*
	 * Disable: leading '(' when the LAST char is not ')'. "(beta)" stays a
	 * literal parenthesised word. Strip the '(' and mark disabled.
	 */
	if (len >= 1 && label[0] == '(' && label[len - 1] != ')') {
		memmove(label, label + 1, len); /* includes the NUL terminator */
		len -= 1;
		if (enabled != nil)
			*enabled = false;
	}

	/*
	 * Check: leading '!' when something follows it (length > 1 in legacy
	 * terms, i.e. at least one char after the '!'). Strip the '!' and mark
	 * checked. '(' is handled first, so "(!Foo" disables AND checks.
	 */
	if (len > 1 && label[0] == '!') {
		memmove(label, label + 1, len); /* includes the NUL terminator */
		if (checked != nil)
			*checked = true;
	}
}


/* ------------------------------------------------------------ */
/*  adapter context                                              */
/* ------------------------------------------------------------ */

/*
 * Per-item cache slot: keyed by the item's hashtable handle (which is stable
 * for the menubar's lifetime -- repl_palette_source_init validates the bar and
 * the underlying ODB handles do not move while the palette is open). Keying
 * on the handle pointer lets re-describes of the same item re-use the
 * existing copy instead of allocating a new one.
 *
 * Why keyed cache (option b) rather than "only cache the pick" (option a):
 * palette_item_t::script_handle has a strict lifetime contract (palette.h
 * L109-128) -- it MUST remain valid for the entire palette session AND
 * across GIL yields. The palette aliases the pointer into exec_script
 * the moment a leaf is highlighted (palette.c:454), and the user might
 * navigate away and back before pressing Enter. Lazy-on-DONE_EXECUTE
 * would race that: the second navigation away would invalidate
 * exec_script's storage. Keying on item identity avoids the race
 * entirely while still capping growth.
 */
typedef struct ty_script_cache_entry {
	hdlhashtable hitem;                 /* key -- item subtable handle */
	Handle hscript;                     /* value -- copy of script body */
} ty_script_cache_entry;


/*
 * One slot per installed menubar in the composed strip. bar_count entries
 * are populated in init_all / init_for, sorted alphabetically by bsname
 * so the composition is deterministic across runs.
 */
typedef struct ty_bar_slot {
	bigstring bsname;     /* bar name under system.menus.data */
	int menu_count;       /* number of top-level menus in this bar */
} ty_bar_slot;


/*
 * The multi-source adapter context. Replaces the old single-bar
 * ty_repl_palette_source_ctx. Supports up to REPL_PS_MAX_BARS bars.
 *
 * Both repl_palette_source_init_all (multi-bar) and
 * repl_palette_source_init_for (single-bar, test compat) fill this
 * struct -- init_for just sets bar_count=1 and skips the installed filter.
 */
typedef struct ty_repl_multi_source_ctx {
	ty_bar_slot bars[REPL_PS_MAX_BARS];
	int bar_count;

	/* Adapter-owned copies of script handles, keyed by item handle.
	   Freed wholesale in dispose so the caller never has to. */
	ty_script_cache_entry cached_scripts[REPL_PS_MAX_CACHED_SCRIPTS];
	int cached_scripts_count;

	/* Latch so the script-cache overflow warning fires once per init,
	 * not once per miss-after-full. Without this, a long session that
	 * arrows through many unique items after the cache fills would
	 * emit a log line per describe. */
	boolean cache_overflow_warned;

	boolean initialised;
} ty_repl_multi_source_ctx;


/* ------------------------------------------------------------ */
/*  helpers                                                      */
/* ------------------------------------------------------------ */

/* Find system.menus.data; returns nil if missing. */
static hdlhashtable resolve_data_table(void) {
	hdlhashtable hsystem = nil, hmenus = nil, hdata = nil;

	if (roottable == nil)
		return nil;

	if (!findnamedtable(roottable, namesystembranch, &hsystem))
		return nil;
	if (!findnamedtable(hsystem, STR_menus, &hmenus))
		return nil;
	if (!findnamedtable(hmenus, STR_data, &hdata))
		return nil;
	return hdata;
}


/* Find system.menus.data.<bsname>; returns nil if missing. */
static hdlhashtable resolve_bar_by_name(const bigstring bsname) {
	hdlhashtable hdata = nil;
	hdlhashtable hbar = nil;

	hdata = resolve_data_table();
	if (hdata == nil)
		return nil;

	/* Cast away const: findnamedtable's bigstring param is non-const in
	 * the legacy header but does not mutate. Our caller declares bsname
	 * const to advertise its own non-mutation. Cast via (unsigned char *)
	 * because bigstring is an array type and cannot be the target of a
	 * direct cast. */
	if (!findnamedtable(hdata, (unsigned char *)bsname, &hbar))
		return nil;
	return hbar;
}


/*
 * Collect-then-sort helpers for deterministic menu ordering within a bar.
 *
 * hashinversesearch visits nodes in hash-bucket order, which is not
 * alphabetical and may change if the table is resized on insert. In the
 * headless runtime langcallbacks.comparenodescallback is cb_noop_compare
 * (always returns 0), so hashsortedinversesearch also produces insertion
 * order rather than alphabetical order. We therefore collect all subtable
 * children into a local array and qsort them by name before indexing.
 *
 * The cap of 256 children is generous -- typical menus have fewer than
 * 16 entries. It matches the bigstring size limit so no slot can be
 * constructively larger than the name encoding allows.
 */
#define SUBTABLE_CHILDREN_CAP 256

/*
 * Sentinel order value for a child that carries no explicit "order" field.
 * Unordered children sort AFTER every explicitly-ordered child and then
 * alphabetically among themselves, preserving the pre-Option-A behavior for
 * menubars that never wrote an order value.
 */
#define CHILD_ORDER_UNSET LONG_MAX

typedef struct ty_child_slot {
	bigstring bsname;
	hdlhashtable htable;
	long order;             /* explicit order field, or CHILD_ORDER_UNSET */
} ty_child_slot;

typedef struct ty_collect_state {
	ty_child_slot *slots;
	int count;
	int cap;
} ty_collect_state;


/*
 * Read the explicit "order" field from a child sub-table. Returns the long
 * value if present and integer-typed (int or long scalar), else
 * CHILD_ORDER_UNSET. Sub-tables without an order field keep the legacy
 * alphabetical-by-name behavior because the sentinel sorts them last.
 */
#define BS_order BIGSTRING("\x05" "order")

static long read_child_order(hdlhashtable hchild) {
	tyvaluerecord val;
	hdlhashnode hnode;

	if (hchild == nil)
		return CHILD_ORDER_UNSET;
	if (!hashtablelookup(hchild, BS_order, &val, &hnode))
		return CHILD_ORDER_UNSET;
	if (val.valuetype == longvaluetype || val.valuetype == intvaluetype)
		return (long) val.data.longvalue;
	return CHILD_ORDER_UNSET;
}


/* Returns true to STOP per hashinversesearch. */
static boolean collect_cb(bigstring bsname, hdlhashnode hnode,
                          tyvaluerecord val, ptrvoid refcon) {
	(void) hnode;

	ty_collect_state *st = (ty_collect_state *) refcon;
	hdlhashtable hchild = nil;

	if (val.valuetype != externalvaluetype)
		return false;
	if (!langexternalvaltotable(val, &hchild, HNoNode))
		return false;
	if (hchild == nil)
		return false;
	if (st->count >= st->cap)
		return false; /* cap reached -- keep walking to count all */

	copystring(bsname, st->slots[st->count].bsname);
	st->slots[st->count].htable = hchild;
	st->slots[st->count].order = read_child_order(hchild);
	st->count++;
	return false; /* always continue */
}


/*
 * qsort comparator for ty_child_slot. Two-key (Option A):
 *   1. explicit "order" field, ascending. Children without an order field
 *      carry CHILD_ORDER_UNSET (LONG_MAX) so they sort after every ordered
 *      child.
 *   2. alphabetical by Pascal bigstring name, as a stable tie-breaker (and
 *      the sole key when neither child has an order field -- the legacy
 *      pre-Option-A behavior).
 */
static int child_slot_cmp(const void *a, const void *b) {
	const ty_child_slot *sa = (const ty_child_slot *) a;
	const ty_child_slot *sb = (const ty_child_slot *) b;
	int la, lb, minlen, cmp;

	if (sa->order < sb->order)
		return -1;
	if (sa->order > sb->order)
		return 1;

	la = stringlength(sa->bsname);
	lb = stringlength(sb->bsname);
	minlen = la < lb ? la : lb;
	cmp = memcmp(&sa->bsname[1], &sb->bsname[1], (size_t) minlen);
	if (cmp != 0)
		return cmp;
	return la - lb;
}


/*
 * Collect and sort direct subtable children of ht into caller-supplied
 * slots[cap]. Returns the count of children found (<= cap).
 * Children are sorted alphabetically by name.
 */
static int collect_sorted_children(hdlhashtable ht,
                                   ty_child_slot *slots, int cap) {
	ty_collect_state st;
	bigstring bsfound;

	if (ht == nil || slots == nil || cap <= 0)
		return 0;

	st.slots = slots;
	st.count = 0;
	st.cap = cap;
	(void) hashinversesearch(ht, &collect_cb, &st, bsfound);

	if (st.count > 1)
		qsort(slots, (size_t) st.count, sizeof(slots[0]), child_slot_cmp);
	return st.count;
}


/* Count direct subtable children of ht. */
static int count_subtable_children(hdlhashtable ht) {
	ty_child_slot slots[SUBTABLE_CHILDREN_CAP];
	return collect_sorted_children(ht, slots, SUBTABLE_CHILDREN_CAP);
}


/* Get the Nth subtable child of ht (0-based). Returns nil if out of range. */
static hdlhashtable nth_subtable_child(hdlhashtable ht, int idx,
                                       bigstring bsout_name) {
	ty_child_slot slots[SUBTABLE_CHILDREN_CAP];
	int n;

	if (bsout_name != nil)
		setemptystring(bsout_name);

	if (ht == nil || idx < 0)
		return nil;

	n = collect_sorted_children(ht, slots, SUBTABLE_CHILDREN_CAP);
	if (idx >= n)
		return nil;

	if (bsout_name != nil)
		copystring(slots[idx].bsname, bsout_name);
	return slots[idx].htable;
}


/*
 * Decompose a global menu index into (bar_idx, local_menu_idx).
 * Walks bars[] summing menu_count until the target is reached.
 *
 * Returns true and fills *out_bar_idx, *out_local_idx on success.
 * Returns false if global_idx is out of range.
 *
 * Pure helper: reads only ctx->bars[] and ctx->bar_count, no ODB access.
 */
static bool decompose_menu_index(const ty_repl_multi_source_ctx *ctx,
                                 int global_idx,
                                 int *out_bar_idx,
                                 int *out_local_idx) {
	int offset = 0;
	int i;

	if (global_idx < 0)
		return false;

	for (i = 0; i < ctx->bar_count; i++) {
		int mc = ctx->bars[i].menu_count;
		if (global_idx < offset + mc) {
			*out_bar_idx = i;
			*out_local_idx = global_idx - offset;
			return true;
		}
		offset += mc;
	}
	return false;
}


/*
 * Look up or insert a per-item script handle copy. Keyed by the item
 * subtable handle so re-describes of the same item return the already-
 * cached copy rather than allocating a new one. This bounds the cache
 * by the number of UNIQUE items in the menubar, not by the number of
 * describe calls -- the latter can easily exceed 256 in a single
 * session as the user arrows through menus.
 *
 * Returns the cached handle, or nil on failure (true cache exhaustion
 * -- more unique items than the slot count -- or copy failure).
 */
static Handle cache_script_handle(ty_repl_multi_source_ctx *ctx,
                                  hdlhashtable hitem,
                                  Handle horig) {
	Handle hcopy = nil;
	int i;

	if (horig == nil || hitem == nil)
		return nil;

	/* Hit: same item already cached -- reuse the copy. */
	for (i = 0; i < ctx->cached_scripts_count; i++) {
		if (ctx->cached_scripts[i].hitem == hitem)
			return ctx->cached_scripts[i].hscript;
	}

	/* Miss: insert a new entry. */
	if (ctx->cached_scripts_count >= REPL_PS_MAX_CACHED_SCRIPTS) {
		if (!ctx->cache_overflow_warned) {
			log_warn(LOG_COMP_GENERAL,
			         "REPL palette: script cache full (%d unique items),"
			         " cannot cache additional scripts",
			         REPL_PS_MAX_CACHED_SCRIPTS);
			ctx->cache_overflow_warned = true;
		}
		return nil;
	}
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
	ty_repl_multi_source_ctx *ctx = (ty_repl_multi_source_ctx *) vctx;
	int total = 0;
	int i;

	if (ctx == nil)
		return 0;

	for (i = 0; i < ctx->bar_count; i++)
		total += ctx->bars[i].menu_count;
	return total;
}


static bool cb_menu_describe(void *vctx, int menu_index,
                             char *out_label, size_t label_cap,
                             char *out_hotkey) {
	ty_repl_multi_source_ctx *ctx = (ty_repl_multi_source_ctx *) vctx;
	int bar_idx = 0, local_idx = 0;
	hdlhashtable hbar = nil;
	hdlhashtable hmenu = nil;
	bigstring bsname;

	if (out_label != nil && label_cap > 0)
		out_label[0] = '\0';
	if (out_hotkey != nil)
		*out_hotkey = '\0';

	if (ctx == nil)
		return false;

	if (!decompose_menu_index(ctx, menu_index, &bar_idx, &local_idx))
		return false;

	hbar = resolve_bar_by_name(ctx->bars[bar_idx].bsname);
	if (hbar == nil)
		return false;

	hmenu = nth_subtable_child(hbar, local_idx, bsname);
	if (hmenu == nil)
		return false;

	if (out_label != nil)
		bs_to_cstr(bsname, out_label, label_cap);

	/*
	 * Future: read a "hotkey" or "cmdkey" field from the menu sub-table
	 * itself. For PR 6 we leave hotkey unset -- palette.c falls back to
	 * first-letter matching and the menubar still navigates fine.
	 */
	return true;
}


/*
 * The palette's contract is that for top-level menus parent_opaque == NULL
 * and menu_index selects which one. For deeper levels parent_opaque is the
 * void* token we returned in palette_item_t.opaque. We never set
 * is_submenu=true for any leaf in PR 6 (the headless tree is always
 * 3-deep -- bar/menu/leaf -- and the palette only walks leaves), so
 * parent_opaque should always be NULL coming back. Defensive: if a caller
 * does pass parent_opaque, treat it as the menu hashtable handle.
 */
static hdlhashtable resolve_menu_for_items(ty_repl_multi_source_ctx *ctx,
                                           int menu_index,
                                           void *parent_opaque) {
	int bar_idx = 0, local_idx = 0;
	hdlhashtable hbar;

	if (parent_opaque != nil)
		return (hdlhashtable) parent_opaque;

	if (!decompose_menu_index(ctx, menu_index, &bar_idx, &local_idx))
		return nil;

	hbar = resolve_bar_by_name(ctx->bars[bar_idx].bsname);
	if (hbar == nil)
		return nil;
	return nth_subtable_child(hbar, local_idx, nil);
}


static int cb_item_count(void *vctx, int menu_index, void *parent_opaque) {
	ty_repl_multi_source_ctx *ctx = (ty_repl_multi_source_ctx *) vctx;
	hdlhashtable hmenu;

	if (ctx == nil)
		return 0;
	hmenu = resolve_menu_for_items(ctx, menu_index, parent_opaque);
	return count_subtable_children(hmenu);
}


/* Locate the "script" field inside a record-typed value and return its
   underlying string handle (which is an independently-allocated copy from
   menudata_describe_leaf). Caller must NOT dispose this handle directly --
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
	ty_repl_multi_source_ctx *ctx = (ty_repl_multi_source_ctx *) vctx;
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

	/*
	 * Apply legacy menu-text prefix codes to the label in place, matching the
	 * Mac code path (mereducemenucodes): leading '(' disables, leading '!'
	 * checks, a lone '-' is a separator. These compose with the stored
	 * `enabled` field: an item explicitly disabled in the ODB stays disabled
	 * even without a '(' prefix, and a '(' prefix can disable an otherwise-
	 * enabled item. We therefore seed the reducer with the current enabled
	 * value and let it only ever turn it off.
	 */
	palette_reduce_menu_codes(out->label, &out->enabled, &out->checked,
	                          &out->is_separator);
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

	/* If we couldn't cache the script handle, fail the describe -- the
	   palette would otherwise PALETTE_DONE_EXECUTE with a NULL handle and
	   the caller would silently drop the dispatch. */
	if (hscript_in_record != nil && hcached == nil)
		return false;

	return true;
}


/* ------------------------------------------------------------ */
/*  bar slot sort (qsort comparator)                             */
/* ------------------------------------------------------------ */

/*
 * Compare two ty_bar_slot entries by their Pascal bigstring names.
 * Used by qsort in init_all to produce deterministic alphabetical ordering.
 *
 * Pascal bigstrings: byte 0 is the length, bytes 1..n are the chars.
 * We compare as NUL-terminated C strings by using the length to bound
 * the comparison -- equalish to strncmp but on Pascal data.
 */
static int bar_slot_cmp(const void *a, const void *b) {
	const ty_bar_slot *sa = (const ty_bar_slot *) a;
	const ty_bar_slot *sb = (const ty_bar_slot *) b;
	int la = stringlength(sa->bsname);
	int lb = stringlength(sb->bsname);
	int minlen = la < lb ? la : lb;
	int cmp = memcmp(&sa->bsname[1], &sb->bsname[1], (size_t) minlen);
	if (cmp != 0)
		return cmp;
	return la - lb;
}


/* ------------------------------------------------------------ */
/*  hashinversesearch callback for enumerating data children     */
/* ------------------------------------------------------------ */

typedef struct ty_bar_enum_state {
	ty_bar_slot *slots;    /* output array to fill */
	int *count;            /* number of slots filled so far */
	int max;               /* slots capacity */
} ty_bar_enum_state;


/*
 * Called by hashinversesearch for each child of system.menus.data.
 * Populates one ty_bar_slot per installed child subtable.
 * Returns false to always continue (we want to visit every child).
 */
static boolean bar_enum_cb(bigstring bsname, hdlhashnode hnode,
                           tyvaluerecord val, ptrvoid refcon) {
	(void) hnode;

	ty_bar_enum_state *st = (ty_bar_enum_state *) refcon;
	hdlhashtable hbar = nil;
	boolean installed = false;
	int mc;

	if (*st->count >= st->max) {
		/*
		 * Capacity reached. Return true so hashinversesearch stops traversal
		 * (see Common/source/langhash.c:2474 -- the early-out triggers on
		 * true, not false). Log once; the caller is responsible for emitting
		 * the overflow warning before invoking hashinversesearch so that the
		 * message fires at most once per init, not once per dropped bar.
		 */
		return true;
	}

	if (val.valuetype != externalvaluetype)
		return false; /* skip scalars (currentmenu, currentsuite, etc.) */

	if (!langexternalvaltotable(val, &hbar, HNoNode))
		return false;
	if (hbar == nil)
		return false;

	/* Only include bars marked .installed = true. */
	if (!menudata_get_installed(bsname, &installed))
		return false;
	if (!installed)
		return false; /* not installed -- skip */

	/* Skip empty bars: no top-level menus means nothing to render. */
	mc = count_subtable_children(hbar);
	if (mc <= 0)
		return false;

	copystring(bsname, st->slots[*st->count].bsname);
	st->slots[*st->count].menu_count = mc;
	(*st->count)++;

	return false; /* always false -- continue visiting all children */
}


/* ------------------------------------------------------------ */
/*  context allocation helpers                                   */
/* ------------------------------------------------------------ */

static void wire_vtable(palette_menu_source_t *out,
                        ty_repl_multi_source_ctx *ctx) {
	out->ctx = ctx;
	out->count_menus = cb_count_menus;
	out->menu_describe = cb_menu_describe;
	out->item_count = cb_item_count;
	out->item_describe = cb_item_describe;
}


/* ------------------------------------------------------------ */
/*  public API                                                   */
/* ------------------------------------------------------------ */

/*
 * Enumerate ALL installed menubars and compose their menus into a single
 * strip. Bars are sorted alphabetically by name for deterministic output.
 *
 * Does NOT check the .installed field of the hardcoded "repl" bar -- it
 * applies the filter uniformly to all children of system.menus.data.
 * Use repl_palette_source_init_for for direct-name access (e.g. tests).
 */
bool repl_palette_source_init_all(palette_menu_source_t *out) {
	ty_repl_multi_source_ctx *ctx = nil;
	hdlhashtable hdata = nil;
	ty_bar_enum_state est;
	bigstring bsfound;

	if (out == nil)
		return false;

	memset(out, 0, sizeof(*out));

	ctx = (ty_repl_multi_source_ctx *) calloc(1, sizeof(*ctx));
	if (ctx == nil)
		return false;

	if (!menudata_ensure_root()) {
		free(ctx);
		return false;
	}

	hdata = resolve_data_table();
	if (hdata == nil) {
		free(ctx);
		return false;
	}

	/* Enumerate installed children of system.menus.data. */
	est.slots = ctx->bars;
	est.count = &ctx->bar_count;
	est.max = REPL_PS_MAX_BARS;
	ctx->bar_count = 0;

	(void) hashinversesearch(hdata, &bar_enum_cb, &est, bsfound);

	/* Warn once if the traversal stopped early due to capacity. */
	if (ctx->bar_count >= REPL_PS_MAX_BARS)
		log_warn(LOG_COMP_GENERAL,
		         "REPL palette: bar count exceeded REPL_PS_MAX_BARS (%d),"
		         " dropping additional bars", REPL_PS_MAX_BARS);

	if (ctx->bar_count == 0) {
		free(ctx);
		return false; /* no installed bars */
	}

	/* Sort alphabetically so the strip is deterministic. */
	qsort(ctx->bars, (size_t) ctx->bar_count, sizeof(ctx->bars[0]),
	      bar_slot_cmp);

	ctx->initialised = true;
	wire_vtable(out, ctx);
	return true;
}


/*
 * Initialise for a specific menubar name (e.g. "test_menubar" in unit tests).
 * Does NOT check .installed -- bypasses installed-filter for direct test
 * access. Use repl_palette_source_init_all for production enumeration.
 *
 * Stores the name internally; caller need not retain the string.
 */
bool repl_palette_source_init_for(palette_menu_source_t *out,
                                  const char *menubar_name) {
	ty_repl_multi_source_ctx *ctx = nil;
	hdlhashtable hbar = nil;
	bigstring bsname;

	if (out == nil || menubar_name == nil)
		return false;

	memset(out, 0, sizeof(*out));

	ctx = (ty_repl_multi_source_ctx *) calloc(1, sizeof(*ctx));
	if (ctx == nil)
		return false;

	/*
	 * bigstring is unsigned char[256]: byte 0 is the Pascal length, bytes
	 * 1..255 hold content. A name of 256+ characters cannot be represented
	 * without overflow. Fail loudly rather than truncating silently.
	 */
	if (strlen(menubar_name) > 255)
		return false;

	copyctopstring((char *)menubar_name, bsname);

	/*
	 * Validate the menubar exists. Returning false here lets the caller
	 * decide whether to install a stub source (no slash menu) versus erroring
	 * out. Lazy-create system.menus.data so this works on a freshly-migrated
	 * v7 root, but DO NOT lazy-create the menubar itself -- its absence is
	 * the host's signal that no menubar was installed.
	 */
	if (!menudata_ensure_root()) {
		free(ctx);
		return false;
	}
	hbar = resolve_bar_by_name(bsname);
	if (hbar == nil) {
		free(ctx);
		return false;
	}

	/* Single-slot context: one bar, menu_count populated from ODB. */
	copystring(bsname, ctx->bars[0].bsname);
	ctx->bars[0].menu_count = count_subtable_children(hbar);
	ctx->bar_count = 1;
	ctx->initialised = true;

	wire_vtable(out, ctx);
	return true;
}


/*
 * Deprecated: use repl_palette_source_init_all. Preserved for compat.
 * Initialise a palette_menu_source_t pointing at the default menubar
 * (system.menus.data.repl).
 */
bool repl_palette_source_init(palette_menu_source_t *out) {
	return repl_palette_source_init_for(out, REPL_PALETTE_DEFAULT_MENUBAR);
}


void repl_palette_source_dispose(palette_menu_source_t *src) {
	ty_repl_multi_source_ctx *ctx;
	int i;

	if (src == nil)
		return;
	ctx = (ty_repl_multi_source_ctx *) src->ctx;
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
