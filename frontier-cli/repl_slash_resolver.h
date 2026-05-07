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
 * SECURITY
 * --------
 * The token is bounds-checked (rejected if NULL, empty, or >= 64 chars).
 * The resolver does NOT execute anything — it only walks the menubar
 * table and returns a leaf handle. The caller must fetch the leaf's
 * script field via menudata_describe_leaf and dispatch via
 * meuserselected_headless.
 *
 * Threat model: the menubar at system.menus.data.repl.REPL is part of
 * the user's database and is mutable from UserTalk. A hostile script
 * (or a corrupted database) could plant a leaf whose label matches a
 * canonical REPL command but whose slot key does not — e.g. label
 * "EvilExit" + slot key "Evil". Without a check, "/e" or "/evilexit"
 * would resolve to that leaf and dispatch its (attacker-controlled)
 * script. The resolver does not run the script itself, but the caller
 * in repl.c special-cases certain slot keys ("Exit", "List", "Jump")
 * for arg-routing and the post-dispatch *running = false flag — those
 * branches must not be reachable through a non-canonical leaf.
 *
 * Mitigation: the resolver enforces an allowlist of canonical REPL slot
 * keys ({"Exit", "Help", "Clear", "List", "Jump", "Key codes"}). A leaf
 * whose slot key is not on this list never resolves, regardless of how
 * its label matches. Adding new REPL commands requires editing the
 * allowlist in repl_slash_resolver.c. The allowlist is intentionally
 * private to the .c file because it is an implementation detail of the
 * REPL command surface, not a contract callers depend on.
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
 *   out_name  - Optional. If non-NULL, on true return filled with the
 *               matched leaf's slot key (i.e. the name under which it is
 *               stored inside the parent menu — the third arg to
 *               menu.addMenuCommand at install time, e.g. "List", "Jump").
 *               NUL-terminated, capped at out_namesz - 1 chars. On false
 *               return, out_name[0] is set to '\0'.
 *               Pass NULL + 0 to skip.
 *   out_namesz - Capacity of out_name in bytes. Ignored if out_name is NULL.
 *
 * Returns true iff a unique match was found per the precedence rules in
 * the file header. Returns false for: NULL/empty/oversized token, missing
 * REPL menubar, no match, or ambiguous match. In every false-return case
 * *out_leaf is set to nil and out_name (if any) is cleared so the caller
 * can pass them through unconditionally.
 *
 * GIL: must be called while holding the GIL — walks shared roottable
 * descendants via findnamedtable / hashinversesearch and reads field
 * scalars via hashtablelookup, all of which assume GIL discipline.
 */
extern boolean repl_resolve_slash_command(const char *token,
                                          hdlhashtable *out_leaf,
                                          char *out_name, size_t out_namesz);


#ifdef __cplusplus
}
#endif

#endif /* REPL_SLASH_RESOLVER_INCLUDE */
