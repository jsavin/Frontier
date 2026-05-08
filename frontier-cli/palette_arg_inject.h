/*
 * palette_arg_inject.h - synthesize an arg-injected UserTalk script source
 *                        from a leaf's stored script + a typed argument.
 *
 * Scope of this module
 * --------------------
 * The slash-menu palette (PR 8) lets the user type an argument to a leaf
 * that has accepts_args=true. The leaf's stored script is a UserTalk
 * call expression of the form:
 *
 *     <handler-name> ()
 *
 * (e.g. "system.menus.handlers.repl.list ()"). To bind the typed arg into
 * that call, we synthesize a NEW script source by replacing the empty
 * argument list with a single string literal:
 *
 *     <handler-name> ("typed-arg")
 *
 * The synthesized source is fed to meuserselected_headless / langbuildtree
 * just like the original script — the parser sees a fully-formed call with
 * the arg bound at compile time. There is NO process-global handoff state
 * between the dispatcher and the host verb adapter, which is what made the
 * earlier g_palette_pending_arg design unsound under thread.new() (the
 * sibling thread could read the wrong arg between the GIL-yield points of
 * langruncode).
 *
 * Two pure helpers, both nul-terminated and bounds-checked, both with no
 * ODB / handle dependencies so they can be unit-tested in isolation:
 *
 *   palette_arg_escape         — escape the typed arg for safe insertion
 *                                inside a UserTalk "..." string literal
 *   palette_arg_inject_into_script
 *                              — locate the empty () in the script source
 *                                and produce a new buffer with the literal
 *                                inserted
 *
 * Security model
 * --------------
 * The typed arg comes from the user (either the palette's input row or
 * the slash-command body). Two threats:
 *
 *   1. Terminal-control byte injection — handled by safe_print_user_string
 *      at print sites. The escape helper here ALSO rejects control bytes
 *      and newlines so they never enter the synthesized script source
 *      (the parser would reject them with a confusing diagnostic, and any
 *      embedded newline would terminate our synthetic statement early).
 *
 *   2. UserTalk-level injection (an attacker types `"); maliciousVerb (`
 *      to break out of the string literal). Mitigated by escaping `"` and
 *      `\\` per UserTalk string-literal rules. Once escaped, every byte
 *      of the user's input is just a string-literal character — there is
 *      no syntactic path back into UserTalk grammar.
 *
 * The langrun escalation surface that path_is_script_expression carves
 * out for repl.jumpPath (path_is_script_expression in repl.c) is an
 * orthogonal verb-level concern: the verb's argument string is the same
 * either way, but the verb chooses to treat strings containing
 * '(' / ')' / '+' as expressions. The dispatch layer's job is just to
 * deliver the user's typed bytes verbatim as a string-literal-bound arg,
 * not to second-guess how a downstream verb interprets them.
 */

#ifndef FRONTIER_CLI_PALETTE_ARG_INJECT_H
#define FRONTIER_CLI_PALETTE_ARG_INJECT_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * palette_arg_escape — escape a typed argument for safe insertion inside
 * a UserTalk double-quoted string literal.
 *
 * Inputs:
 *   arg     — NUL-terminated source bytes from the user. May be empty.
 *   out     — caller-provided destination buffer (NUL-terminated on
 *             success).
 *   out_cap — capacity of `out` in bytes, including the trailing NUL.
 *
 * Behavior:
 *   - Each '"' becomes '\\' '"'.
 *   - Each '\\' becomes '\\' '\\'.
 *   - Any byte < 0x20 OR == 0x7F is REJECTED. The rejected byte set
 *     includes '\\n', '\\r', and '\\t' deliberately: a tab in the typed
 *     arg has no good string-literal mapping (UserTalk has no \\t escape
 *     in stored source), and a newline would terminate our synthetic
 *     statement early. Reject-rather-than-truncate so the caller can
 *     surface a clear diagnostic.
 *   - High bytes (>= 0x80) pass through unmodified — UTF-8 continuation
 *     bytes are legitimate inside string literals.
 *
 * Returns:
 *   true  — out is NUL-terminated and contains the escaped form.
 *   false — input contained a forbidden control byte, OR escaped form
 *           did not fit in out_cap. On failure out[0] is set to '\\0'
 *           (caller-side defensive cleanup) but the caller MUST still
 *           treat the operation as failed.
 */
bool palette_arg_escape(const char *arg, char *out, size_t out_cap);

/*
 * palette_arg_inject_into_script — produce a new UserTalk source string
 * with the typed arg bound as a single string literal in the leaf's call.
 *
 * Inputs:
 *   script_in     — pointer to the leaf's stored script bytes. NOT
 *                   required to be NUL-terminated; length comes from
 *                   script_in_len. The bytes are read-only.
 *   script_in_len — length of script_in in bytes.
 *   escaped_arg   — output of palette_arg_escape (already escaped per
 *                   UserTalk literal rules). MUST be NUL-terminated.
 *                   May be empty — caller should detect that case before
 *                   calling and skip injection.
 *   out_buf       — output: receives a heap-allocated NUL-terminated
 *                   buffer. Caller MUST free() it on success.
 *   out_len       — output: length of *out_buf, NOT counting the NUL.
 *                   May be NULL if the caller doesn't want the length.
 *
 * Algorithm:
 *   Scan from script_in[0] forward to find the FIRST '(' whose matching
 *   ')' contains only whitespace (i.e. an empty argument list). Replace
 *   that '()' with '("' + escaped_arg + '")'. Everything outside the
 *   replacement is preserved byte-for-byte (including comments, line
 *   endings, leading on-handler clauses, etc.).
 *
 *   This is byte-level scanning — UserTalk strings inside the script
 *   source are handled by skipping past '"...\\"' literals so a paren
 *   inside a string literal isn't mistaken for the call's open-paren.
 *   The same logic skips '«...»' UserTalk comments. Line-comment forms
 *   ('//' ... newline) are also skipped.
 *
 *   For safety, the search rejects:
 *     - no '(' found
 *     - '(' whose contents are not pure whitespace
 *     - unbalanced parens (no matching ')')
 *     - script_in_len == 0
 *
 * Returns:
 *   true  — *out_buf populated; caller frees with free().
 *   false — synthesis failed; *out_buf is NULL. The leaf script is
 *           shaped in a way the simple algorithm can't safely transform.
 *           Caller should fall back to dispatching the original script
 *           with no arg (same behavior as accepts_args=false).
 */
bool palette_arg_inject_into_script(const char *script_in,
                                    size_t script_in_len,
                                    const char *escaped_arg,
                                    char **out_buf,
                                    size_t *out_len);

#ifdef __cplusplus
}
#endif

#endif /* FRONTIER_CLI_PALETTE_ARG_INJECT_H */
