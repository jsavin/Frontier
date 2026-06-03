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
#include <stdint.h>

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
 * ut_decanonicalize_outline_text - inverse of ut_canonicalize_outline_text.
 * Transform .ut canonical bytes (UTF-8, LF line endings, "//" comments, no
 * structure markers, no trailing newline) back into in-memory outline source
 * bytes (MacRoman, CR line endings, 0xC7 comment markers).
 *
 * Transform steps (reverse of the forward pass):
 *
 *   1. UTF-8 decode -> MacRoman encode: each UTF-8 sequence is decoded to a
 *      Unicode scalar, then mapped back to a MacRoman byte via the inverse of
 *      kMacRomanToUnicode. ASCII scalars (< 0x80) pass through unchanged. A
 *      scalar with no MacRoman representation causes immediate failure.
 *
 *   2. LF (0x0A) -> CR (0x0D): every line-feed becomes a carriage return.
 *
 *   3. Literal-aware "//" -> 0xC7: a "//" that begins a comment (outside any
 *      string or char literal) becomes a single 0xC7 byte. The literal-state
 *      machine mirrors the forward pass exactly: "//" inside "...", inside
 *      curly-quote strings (U+201C...U+201D, i.e. MacRoman 0xD2...0xD3 bytes
 *      in the output), or inside '...' char literals is data and is preserved
 *      as two 0x2F bytes. Backslash escapes the next code point inside a
 *      literal. Literal state resets at each CR (produced by step 2). No 0xC8
 *      is ever emitted: the forward pass dropped all 0xC8 bytes, so there is
 *      no information to restore.
 *
 *   NOTE: Structure markers (brace wrappers around all-comment subtrees) are
 *   NOT re-inserted. The forward pass stripped them, and the kernel's install
 *   path (langstripstructuremarkers / the comment visitor) reconstructs structure
 *   on re-compilation. Callers should treat the output of this function as the
 *   raw source bytes that the kernel's compile-time machinery expects.
 *
 *   NOTE: A trailing CR is NOT appended. The forward pass dropped a trailing
 *   CR/LF, and the kernel install path tolerates no-trailing-CR. Round-trip
 *   fidelity for inputs that originally had a trailing CR is not exact
 *   (fixed-point stability holds: forward(reverse(expected)) == expected).
 *
 * Inputs:  in/inlen = .ut canonical bytes (UTF-8, LF, "//"-comments).
 * Outputs: *out = malloc'd MacRoman/CR buffer (NUL-terminated for caller
 *          convenience; NUL is NOT counted in *outlen). The caller owns the
 *          buffer and must free() it. *outlen = byte length excluding NUL.
 * Returns 1 on success, 0 on:
 *   - allocation failure
 *   - malformed UTF-8 sequence
 *   - a UTF-8 scalar that has no MacRoman representation (e.g. emoji)
 *   On failure *out is set to NULL and *outlen to 0.
 *
 * Pure function: no globals, no kernel state, thread-safe, GIL-free.
 */
int ut_decanonicalize_outline_text(const unsigned char *in, size_t inlen,
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


/*
 * EXPORT PRIMITIVE
 * ----------------
 * ut_export_script - write one dirty script to its .ut file.
 *
 * This is a pure, kernel-independent function. It combines canonicalization,
 * path mapping, parent-directory creation, atomic file write, and mtime
 * stamping into a single call. It has no knowledge of the ODB or the Frontier
 * runtime -- it operates only on byte buffers and POSIX file-system calls.
 *
 * Inputs:
 *   raw        - in-memory outline-text bytes (MacRoman, CR endings,
 *                0xC7/0xC8 comment markers, structure braces). Exactly the
 *                bytes that opverbgetlangtext / opgetlangtext produces.
 *   rawlen     - number of bytes at raw.
 *   dotted_path - NUL-terminated dotted ODB path, e.g. "system.verbs.op".
 *                 Must pass the same safety checks as ut_odb_path_to_fs.
 *   sync_dir   - NUL-terminated sync corpus root, e.g.
 *                "usertalk_scripts/Frontier.root". No trailing slash required.
 *   mac_mtime  - the script's modification time in Mac epoch seconds
 *                (seconds since 1904-01-01 00:00:00). The written .ut file's
 *                mtime is set to (mac_mtime - 2082844800), the equivalent
 *                Unix epoch time. This offset matches timedate.c line 314:
 *                "frontier_epoch_offset = 2082844800LL". If mac_mtime is <=
 *                2082844800 (i.e. the Unix equivalent would be <= 0), mtime
 *                stamping is skipped (the file's mtime will be "now").
 *
 * Behavior:
 *   1. Canonicalize raw -> UTF-8/LF/"//" form via ut_canonicalize_outline_text.
 *   2. Map dotted_path -> fs path via ut_odb_path_to_fs.
 *   3. Create all parent directories (mkdir -p semantics).
 *   4. Write canonical bytes to a temp file in the same directory, then
 *      rename into place (atomic on POSIX). The previous .ut (if any) is
 *      silently overwritten.
 *   5. Set the .ut file's mtime via utimes()/utimensat() from mac_mtime.
 *
 * Returns 1 on success. Returns 0 if:
 *   - dotted_path fails the ut_odb_path_to_fs safety check
 *   - any directory could not be created
 *   - the file could not be written
 *   - canonicalization failed (allocation error)
 * On any failure the .ut file is left in whatever state it was in before
 * the call (because the write goes to a temp file first).
 *
 * Thread safety: concurrent writes to DIFFERENT paths are safe (each uses
 * its own temp file). Concurrent writes to the SAME path are not safe --
 * the rename() is atomic but the last writer wins without any conflict
 * detection. This matches the export contract: only one thread (the
 * save_system_root_on_exit path) calls this during a save.
 *
 * Pure function apart from file-system side effects. No kernel state, no
 * globals. Safe to call without the GIL once the script's text bytes have
 * been extracted.
 */
int ut_export_script(const unsigned char *raw, size_t rawlen,
                     const char *dotted_path, const char *sync_dir,
                     int64_t mac_mtime);


#ifdef __cplusplus
}
#endif

#endif /* UT_SYNC_INCLUDE */
