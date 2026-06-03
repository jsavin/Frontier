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


#ifdef __cplusplus
}
#endif

#endif /* UT_SYNC_INCLUDE */
