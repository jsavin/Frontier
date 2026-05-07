/*
 * repl_slash_resolver.h - REPL slash-command -> menubar leaf resolution.
 *
 * PR 7 of the slash-menu chain. Resolves a slash-command token (the
 * characters after the leading '/') against the installed REPL menubar at
 * system.menus.data.repl.REPL, so that a user typing "/exit" reaches the
 * same UserTalk handler script as a user opening the palette and selecting
 * "Exit". Eliminates the hardcoded if/else chain that lived in
 * repl_commands.c.
 *
 * Resolution precedence (first match wins):
 *   1. Exact label match (case-insensitive). Spaces in labels are collapsed
 *      so "/keycodes" matches a "Key codes" item.
 *   2. Unique prefix match. "/exi" matches "Exit" iff no other label
 *      starts with that prefix. Ambiguous prefixes return false; the
 *      caller surfaces an unknown-command error so the user disambiguates.
 *   3. Single-letter token: matches by first letter of label, again
 *      requiring uniqueness. "/h" matches "Help" iff no other label
 *      starts with H.
 *
 * SECURITY: the token is bounds-checked (rejected if NULL, empty, or
 * >= 64 chars). The resolver does NOT execute anything — it only walks
 * the menubar table and returns a leaf handle. The caller must fetch
 * the leaf's script field via menudata_describe_leaf and dispatch via
 * meuserselected_headless.
 *
 * License
 * -------
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Frontier contributors.
 */

#ifndef REPL_SLASH_RESOLVER_INCLUDE
#define REPL_SLASH_RESOLVER_INCLUDE

#ifndef shelltypesinclude
	#include "shelltypes.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif


/*
 * Maximum length for a slash-command token (excluding the leading '/').
 * Tokens at or above this size are rejected without walking the menubar.
 * 64 is generous: real labels are 1-2 words, and the palette UI imposes
 * its own width constraint on visible labels.
 */
#define REPL_SLASH_TOKEN_MAX 64


/*
 * Resolve `token` to a leaf hashtable inside system.menus.data.repl.REPL.
 *
 * Inputs:
 *   token     - NUL-terminated C string, the chars after a leading '/' with
 *               leading and trailing whitespace already trimmed by the
 *               caller. Must be non-NULL, non-empty, and fewer than
 *               REPL_SLASH_TOKEN_MAX characters.
 *
 * Outputs:
 *   *out_leaf - On true return, set to the matched leaf's hashtable handle
 *               (a borrowed reference into the ODB; the caller must NOT
 *               dispose it). On false return, set to nil.
 *
 * Returns true iff a unique match was found per the precedence rules in
 * the file header. Returns false for: NULL/empty/oversized token, missing
 * REPL menubar, no match, or ambiguous match. In every false-return case
 * *out_leaf is set to nil so the caller can pass it through unconditionally.
 *
 * GIL: must be called while holding the GIL — walks shared roottable
 * descendants via findnamedtable / hashinversesearch and reads field
 * scalars via hashtablelookup, all of which assume GIL discipline.
 */
extern boolean repl_resolve_slash_command(const char *token,
                                          hdlhashtable *out_leaf);


#ifdef __cplusplus
}
#endif

#endif /* REPL_SLASH_RESOLVER_INCLUDE */
