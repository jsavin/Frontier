/*
 * ut_sync.h - Bidirectional ODB <-> .ut sync: canonicalizer + path mapping.
 *
 * Part of the ODB<->.ut sync feature (docs/ODB_UT_SYNC_DESIGN.md). The ODB
 * (.root) is authoritative; .ut files are UTF-8/LF review artifacts that
 * mirror each script's source so reviewers can diff UserTalk changes. This
 * module owns the byte-level transform between the two representations.
 *
 * CANONICALIZER (this file's first concern)
 * -----------------------------------------
 * The kernel stores a script as a compiled outline, not as source text. To
 * write a .ut, the exporter regenerates source via opgetlangtext (the
 * debug/getSource path, no recompile). That text is in the in-memory
 * representation:
 *
 *   - MacRoman 8-bit encoding (high-bit bytes are MacRoman code points)
 *   - CR (0x0D) line endings
 *   - legacy comment markers: 0xC7 (chcomment, comment START, like "//")
 *     and 0xC8 (chendcomment, comment END)
 *   - outline structure braces "{" / "}" / "};" wrapping subtrees
 *   - no trailing newline
 *
 * The .ut canonical form is:
 *
 *   - UTF-8
 *   - LF (0x0A) line endings
 *   - "//" comments (0xC7 -> "//", 0xC8 dropped: a // comment runs to EOL)
 *   - bare comments for all-comment subtrees (the brace wrappers stripped,
 *     matching langstripstructuremarkers on re-install)
 *   - no trailing newline
 *
 * ut_canonicalize_outline_text() applies that transform. It is a pure
 * function over a byte buffer with no kernel dependencies, so it can be
 * unit-tested for byte-identical parity against the Python reference
 * (tools/verify_virgin_root_sync.py normalize_kernel_body).
 *
 * The literal-aware comment substitution mirrors langscan.c: 0xC7/0xC8 are
 * substituted ONLY outside string and character literals. Inside "...",
 * the Mac curly-quote pair (0xD2 ... 0xD3), or '...', they are literal data
 * and preserved verbatim. "\" escapes the next byte inside a literal.
 * Literals never span lines, so literal state resets at each CR/LF.
 *
 * NOTE ON JSON ESCAPES: the Python reference's step 1 decodes JSON escapes
 * (\r \n \t \" \\ \/ \uXXXX) because it reads from the protocol's
 * JSON-wrapped "value" field. opgetlangtext output passed to the C
 * canonicalizer is NOT JSON-wrapped, so this module does not JSON-unescape.
 * The parity test feeds the C canonicalizer already-unescaped bytes (the
 * post-step-1 bytes) so both implementations operate on identical input
 * from step 2 onward.
 *
 * License
 * -------
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Frontier contributors.
 */

#ifndef UT_SYNC_INCLUDE
#define UT_SYNC_INCLUDE

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif


/*
 * ut_canonicalize_outline_text - transform in-memory outline source bytes
 * into .ut canonical bytes.
 *
 * Inputs:
 *   in     - pointer to the raw outline-text bytes (MacRoman, CR endings,
 *            0xC7/0xC8 comment markers, structure braces). May contain NUL?
 *            No: UserTalk source is NUL-free; `in` is treated as a byte run
 *            of length `inlen`, not a C string, so embedded NULs would be
 *            copied through, but they do not occur in practice.
 *   inlen  - number of bytes at `in`.
 *
 * Outputs:
 *   *out    - on success, set to a malloc'd buffer holding the UTF-8/LF/"//"
 *             canonical form. The buffer is NUL-terminated for caller
 *             convenience, but the NUL is NOT counted in *outlen. The caller
 *             owns the buffer and must free() it.
 *   *outlen - on success, set to the canonical byte length (excluding the
 *             trailing NUL).
 *
 * Returns 1 on success, 0 on allocation failure. On failure *out is set to
 * NULL and *outlen to 0.
 *
 * Pure function: no globals, no kernel state, thread-safe. Safe to call
 * without the GIL.
 */
int ut_canonicalize_outline_text(const unsigned char *in, size_t inlen,
                                 unsigned char **out, size_t *outlen);


/*
 * PATH MAPPING
 * ------------
 * A script at ODB dotted path "a.b.c" maps to the filesystem path
 * "<sync_dir>/a/b/c.ut" and back. The mapping is purely lexical: each dotted
 * segment becomes one filesystem path component, with no escaping. This
 * matches the corpus on disk (segments like "#filters" are stored as literal
 * directory names) and the verifier's documented contract
 * (tools/verify_virgin_root_sync.py: "lexical, no escaping").
 *
 * The dotted path is the one the kernel builds during hydrate
 * (langhash_materialize_current_path) and pack: it is a "." join of raw key
 * names, NOT bracket-quoted. The lexical mapping is therefore ambiguous only
 * if a key name itself contains "." or "/"; no script in the corpus does, and
 * the functions reject any segment containing "/" (path-separator injection)
 * or control bytes. A key containing "." cannot be distinguished from a
 * segment boundary by these functions -- callers that might handle such keys
 * must walk the hashtable chain instead. (Documented limitation; not present
 * in any real corpus.)
 *
 * SECURITY: ut_odb_path_to_fs refuses any dotted path that, after mapping,
 * would escape sync_dir -- empty segments, segments equal to "." or "..", a
 * leading/trailing/double dot, or any segment containing "/" or a control
 * byte. This prevents a hostile or corrupt ODB key (e.g. "..") from steering
 * an export write outside the sync directory.
 */

/*
 * ut_odb_path_to_fs - map an ODB dotted path to its .ut filesystem path.
 *
 * Inputs:
 *   dotted_path - NUL-terminated dotted path, e.g. "system.verbs.builtins.op".
 *   sync_dir    - NUL-terminated sync corpus root, e.g.
 *                 "usertalk_scripts/Frontier.root". No trailing slash
 *                 required (one is inserted if absent).
 *   out         - caller-provided buffer for the result fs path.
 *   outsz       - capacity of out in bytes.
 *
 * On success writes "<sync_dir>/<a>/<b>/<c>.ut" to out (NUL-terminated) and
 * returns 1. Returns 0 if dotted_path is malformed/unsafe (see SECURITY
 * above), if any argument is NULL, or if the result would not fit in outsz.
 * On any failure out[0] is set to '\0' when outsz > 0.
 */
int ut_odb_path_to_fs(const char *dotted_path, const char *sync_dir,
                      char *out, size_t outsz);

/*
 * ut_fs_path_to_odb - map a .ut filesystem path back to its ODB dotted path.
 *
 * Inputs:
 *   fs_path  - NUL-terminated path to a .ut file. May be absolute or relative;
 *              it must be lexically within sync_dir and end in ".ut".
 *   sync_dir - NUL-terminated sync corpus root (same value passed to
 *              ut_odb_path_to_fs).
 *   out      - caller-provided buffer for the dotted path.
 *   outsz    - capacity of out in bytes.
 *
 * On success writes the dotted path (e.g. "system.verbs.builtins.op") to out
 * and returns 1. Returns 0 if fs_path is not under sync_dir, lacks a ".ut"
 * suffix, contains a "." in a path component (which would corrupt the dotted
 * form), any argument is NULL, or the result would not fit. On failure out[0]
 * is set to '\0' when outsz > 0.
 *
 * This function does NOT resolve symlinks or canonicalize ".."; it operates
 * lexically. Callers feeding untrusted fs_path values should resolve and
 * confine the path first.
 */
int ut_fs_path_to_odb(const char *fs_path, const char *sync_dir,
                      char *out, size_t outsz);


#ifdef __cplusplus
}
#endif

#endif /* UT_SYNC_INCLUDE */
