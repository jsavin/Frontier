/*
 * repl_slash_resolver.c - PR 7. Resolves slash-command tokens against the
 * installed REPL menubar at system.menus.data.repl.REPL.
 *
 * The match algorithm walks every leaf under the "REPL" menu, normalizes
 * the leaf's label for comparison (case-fold, collapse internal spaces),
 * and applies three precedence tiers in order: exact, unique prefix, and
 * single-letter first-letter. Ambiguity at any tier returns false rather
 * than guessing — the caller surfaces an "Unknown command" error so the
 * user disambiguates by typing more characters.
 *
 * Why a single-pass walk instead of using menudata_list_leaves?
 * -----------------------------------------------------------
 * menudata_list_leaves returns address values, which the caller would have
 * to re-resolve to hashtable handles via langexternalvaltotable. Going
 * straight at the bar->menu->item table chain via findnamedtable +
 * hashinversesearch is one fewer hop, and it gives us direct access to
 * the leaf hashtables we need to return.
 *
 * GIL: every public + private function here assumes the GIL is held.
 * findnamedtable / hashinversesearch / hashtablelookup all touch
 * lang.c global structures (the hashtable stack, the heap registry).
 *
 * License
 * -------
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Frontier contributors.
 */

#include <ctype.h>
#include <string.h>

#include "frontier.h"
#include "shelltypes.h"
#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "langinternal.h"
#include "langexternal.h"
#include "tablestructure.h"
#include "tableverbs.h"
#include "stringdefs.h"

#include "menudata_headless.h"
#include "repl_slash_resolver.h"


/*
 * Pascal strings for the path components into the REPL menu. The bar name
 * "repl" matches what installReplMenubar.ut writes via menu.addMenuCommand
 * (the bar arg is @system.menus.data.repl). The menu name "REPL" matches
 * the second argument to menu.addMenuCommand inside that script.
 */
#define BS_repl  BIGSTRING("\x04" "repl")
#define BS_REPL  BIGSTRING("\x04" "REPL")
#define BS_label BIGSTRING("\x05" "label")


/*
 * Normalize one byte for case-insensitive label comparison: lowercase ASCII
 * letters, pass everything else through unchanged. Operates on the
 * (unsigned char) C-string representation, not on Pascal strings.
 */
static char fold_byte(char c) {
	unsigned char b = (unsigned char)c;
	if (b >= 'A' && b <= 'Z') return (char)(b - 'A' + 'a');
	return (char)b;
}


/*
 * Copy `src` into `dst` (size `dstsz`), case-folding ASCII and dropping
 * spaces. NUL-terminates dst. Returns the number of output bytes written
 * (excluding the trailing NUL). If dstsz is 0 returns 0 and writes nothing.
 *
 * Space-dropping is what makes "/keycodes" match a "Key codes" leaf —
 * users type the slash-command without internal whitespace.
 */
static size_t fold_collapse(const char *src, char *dst, size_t dstsz) {
	if (dstsz == 0)
		return 0;

	size_t out = 0;
	for (size_t i = 0; src[i] != '\0' && out + 1 < dstsz; i++) {
		char b = src[i];
		if (b == ' ' || b == '\t')
			continue;
		dst[out++] = fold_byte(b);
	}
	dst[out] = '\0';
	return out;
}


/*
 * Refcon for the leaf-walking inversesearch.
 *
 * Each tier (exact, prefix, first-letter) tracks its own count and best
 * candidate so the caller can apply precedence after the walk completes.
 * Slot keys are captured alongside the leaf handles because callers that
 * need to special-case "List" or "Jump" identify them by slot key, not
 * by label (label is a free-form string; key is the canonical id under
 * the parent menu).
 */
typedef struct ty_resolve_refcon {
	const char *normalized_token; /* token already case-folded, space-stripped */
	size_t token_len;
	hdlhashtable best_leaf;
	bigstring    best_leaf_name;
	int exact_count;
	hdlhashtable best_prefix;
	bigstring    best_prefix_name;
	int prefix_count;
	hdlhashtable best_firstletter;
	bigstring    best_firstletter_name;
	int firstletter_count;
} ty_resolve_refcon;


/*
 * Read the leaf's "label" field into a C buffer. If the field is missing or
 * not a string, falls back to the leaf's hashtable name (i.e. the key the
 * leaf is stored under in its parent menu). This matches the
 * menudata_describe_leaf semantics: missing label defaults to the empty
 * string, but for resolution purposes the slot key is a useful fallback.
 *
 * Returns the number of bytes written (excluding NUL). 0 if no label and
 * no fallback name available.
 */
static size_t read_leaf_label(hdlhashtable hleaf, bigstring bsname,
                              char *dst, size_t dstsz) {
	bigstring bskey;
	hdlhashnode hnode;
	tyvaluerecord val;

	if (dstsz == 0) return 0;
	dst[0] = '\0';

	copystring(BS_label, bskey);
	if (hashtablelookup(hleaf, bskey, &val, &hnode)
	    && val.valuetype == stringvaluetype
	    && val.data.stringvalue != nil) {
		bigstring bs;
		texthandletostring(val.data.stringvalue, bs);
		size_t len = (size_t)stringlength(bs);
		if (len >= dstsz) len = dstsz - 1;
		memcpy(dst, stringbaseaddress(bs), len);
		dst[len] = '\0';
		return len;
	}

	/* Fall back to the leaf's slot key. */
	size_t len = (size_t)stringlength(bsname);
	if (len >= dstsz) len = dstsz - 1;
	memcpy(dst, stringbaseaddress(bsname), len);
	dst[len] = '\0';
	return len;
}


/*
 * Per-leaf visitor. For each leaf in the REPL menu, normalize the label
 * and check exact / prefix / first-letter against the token. Update the
 * refcon counters; never short-circuit (we need full-traversal to detect
 * ambiguity).
 */
static boolean visit_leaf(bigstring bsname, hdlhashnode hnode,
                          tyvaluerecord val, ptrvoid refcon) {
	(void)hnode;
	ty_resolve_refcon *ctx = (ty_resolve_refcon *)refcon;

	/*
	 * Only sub-tables are leaves in the menu rung. Scalars at the menu
	 * level shouldn't exist for an installed menubar, but guard anyway.
	 */
	hdlhashtable hleaf = nil;
	if (val.valuetype != externalvaluetype)
		return false;
	if (!langexternalvaltotable(val, &hleaf, hnode))
		return false;
	if (hleaf == nil)
		return false;

	char raw_label[REPL_SLASH_TOKEN_MAX * 2];
	size_t raw_len = read_leaf_label(hleaf, bsname,
	                                 raw_label, sizeof(raw_label));
	if (raw_len == 0)
		return false;

	char folded[REPL_SLASH_TOKEN_MAX * 2];
	size_t folded_len = fold_collapse(raw_label, folded, sizeof(folded));
	if (folded_len == 0)
		return false;

	/* Exact (post-fold) match. */
	if (folded_len == ctx->token_len
	    && memcmp(folded, ctx->normalized_token, folded_len) == 0) {
		ctx->exact_count++;
		ctx->best_leaf = hleaf;
		copystring(bsname, ctx->best_leaf_name);
		return false; /* keep walking — exact matches should be unique
		                 anyway, but the count guards against duplicate
		                 keys. */
	}

	/* Prefix match (token is a strict prefix of folded label). */
	if (ctx->token_len > 1
	    && ctx->token_len < folded_len
	    && memcmp(folded, ctx->normalized_token, ctx->token_len) == 0) {
		ctx->prefix_count++;
		if (ctx->best_prefix == nil) {
			ctx->best_prefix = hleaf;
			copystring(bsname, ctx->best_prefix_name);
		}
	}

	/* First-letter match (token is a single character that matches the
	 * folded label's first character). */
	if (ctx->token_len == 1
	    && folded_len > 0
	    && folded[0] == ctx->normalized_token[0]) {
		ctx->firstletter_count++;
		if (ctx->best_firstletter == nil) {
			ctx->best_firstletter = hleaf;
			copystring(bsname, ctx->best_firstletter_name);
		}
	}

	return false; /* never short-circuit */
}


/* Copy a Pascal-string slot key into a C buffer (NUL-terminated, capped). */
static void name_pstring_to_cstring(bigstring bsname, char *dst, size_t dstsz) {
	if (dstsz == 0) return;
	size_t len = (size_t)stringlength(bsname);
	if (len >= dstsz) len = dstsz - 1;
	memcpy(dst, stringbaseaddress(bsname), len);
	dst[len] = '\0';
}


boolean repl_resolve_slash_command(const char *token, hdlhashtable *out_leaf,
                                   char *out_name, size_t out_namesz) {

	if (out_leaf == nil)
		return false;
	*out_leaf = nil;
	if (out_name != NULL && out_namesz > 0)
		out_name[0] = '\0';

	if (token == NULL || token[0] == '\0')
		return false;

	/* Bounds check: reject tokens at or above REPL_SLASH_TOKEN_MAX. */
	size_t toklen = strlen(token);
	if (toklen >= REPL_SLASH_TOKEN_MAX)
		return false;

	/*
	 * Normalize the input token the same way we normalize labels so the
	 * comparisons line up: case-fold ASCII, drop internal whitespace.
	 */
	char ntok[REPL_SLASH_TOKEN_MAX];
	size_t nlen = fold_collapse(token, ntok, sizeof(ntok));
	if (nlen == 0)
		return false;

	/* Walk system.menus.data.repl.REPL. */
	hdlhashtable hsystem = nil, hmenus = nil, hdata = nil;
	if (!findnamedtable(roottable, namesystembranch, &hsystem))
		return false;
	if (!findnamedtable(hsystem, STR_menus, &hmenus))
		return false;
	if (!findnamedtable(hmenus, STR_data, &hdata))
		return false;

	hdlhashtable hbar = nil;
	bigstring bs;
	copystring(BS_repl, bs);
	if (!findnamedtable(hdata, bs, &hbar))
		return false;

	hdlhashtable hmenu = nil;
	copystring(BS_REPL, bs);
	if (!findnamedtable(hbar, bs, &hmenu))
		return false;

	ty_resolve_refcon ctx;
	ctx.normalized_token = ntok;
	ctx.token_len = nlen;
	ctx.best_leaf = nil;
	setemptystring(ctx.best_leaf_name);
	ctx.exact_count = 0;
	ctx.prefix_count = 0;
	ctx.firstletter_count = 0;
	ctx.best_prefix = nil;
	setemptystring(ctx.best_prefix_name);
	ctx.best_firstletter = nil;
	setemptystring(ctx.best_firstletter_name);

	bigstring bsfound;
	(void)hashinversesearch(hmenu, &visit_leaf, &ctx, bsfound);

	/* Precedence: exact > prefix > first-letter. Each tier requires
	 * uniqueness — multiple matches at the same tier returns false. */
	if (ctx.exact_count == 1) {
		*out_leaf = ctx.best_leaf;
		if (out_name != NULL)
			name_pstring_to_cstring(ctx.best_leaf_name, out_name, out_namesz);
		return true;
	}
	if (ctx.exact_count > 1)
		return false; /* shouldn't happen with hashtable keys, defense */

	if (ctx.prefix_count == 1) {
		*out_leaf = ctx.best_prefix;
		if (out_name != NULL)
			name_pstring_to_cstring(ctx.best_prefix_name, out_name, out_namesz);
		return true;
	}
	if (ctx.prefix_count > 1)
		return false;

	if (ctx.firstletter_count == 1) {
		*out_leaf = ctx.best_firstletter;
		if (out_name != NULL)
			name_pstring_to_cstring(ctx.best_firstletter_name, out_name, out_namesz);
		return true;
	}
	return false;
}
