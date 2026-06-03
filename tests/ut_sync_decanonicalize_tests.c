/*
 * ut_sync_decanonicalize_tests.c - Round-trip unit tests for the ODB<->.ut
 * reverse canonicalizer (frontier-cli/ut_sync.c ut_decanonicalize_outline_text).
 *
 * The reverse canonicalizer is the byte-level inverse of
 * ut_canonicalize_outline_text. These tests verify:
 *
 *   1. Round-trip parity against existing fixtures.
 *   2. Literal-state correctness: "//" inside strings/char literals is NOT
 *      converted to 0xC7; only "//" that starts a comment is converted.
 *   3. High-bit MacRoman round-trip (e.g., 0x8E e-acute <-> U+00E9).
 *   4. Non-MacRoman rejection (e.g., U+1F600 emoji -> returns 0).
 *
 * FIXTURE CLASSIFICATION
 * ----------------------
 * Lossless (byte-exact reverse): forward transform is injective over these
 * fixtures, so reverse(expected) == in exactly.
 *
 *   simple          CR -> LF only; trivially invertible.
 *   highbit         MacRoman 0xD2/0xD3 (curly quotes) -> UTF-8; the mapping
 *                   is bijective (128 unique Unicode scalars).
 *   marker_in_string  0xC7/0xC8 inside a "..." preserved as MacRoman; the
 *                   reverse re-encodes them back.
 *   marker_in_char  0xC8 inside '...' preserved as MacRoman U+00BB; bijective.
 *   empty           Empty input -> empty output; trivially invertible.
 *
 * Lossy (fixed-point only): forward transform is NOT injective; information
 * is lost. We can only assert forward(reverse(expected)) == expected.
 *
 *   comment_markers   0xC8 is DROPPED by the forward pass (comment-end marker
 *                     has no "//" counterpart; the "//" runs to EOL). The
 *                     reverse cannot re-insert 0xC8. So reverse then re-forward
 *                     gives the same canonical form, but reverse(expected) !=
 *                     the original .in bytes.
 *   brace_strip       "{" / "}" / "};" structure markers wrapping all-comment
 *                     subtrees are stripped by the forward pass. The reverse
 *                     does NOT re-add them (the kernel install path rebuilds
 *                     structure on compilation). So reverse(expected) omits
 *                     the brace wrappers; re-forward is stable.
 *   trailing_nl       The forward pass drops a trailing CR in the .in file.
 *                     The reverse does not re-add it. So reverse(expected) is
 *                     .in minus the trailing CR.
 *   corpus_edit       The fixture's .in form already contains ASCII "//" (two
 *                     0x2F bytes) for comment notation rather than the kernel's
 *                     0xC7 marker. The forward pass passes those "//" through
 *                     unchanged (they are already in canonical form). The
 *                     reverse function converts any "//" outside a literal to
 *                     0xC7, producing a MacRoman form that differs from .in
 *                     (which had ASCII "//"). Re-forwarding that MacRoman form
 *                     gives back the canonical "//" form, so fixed-point holds.
 *
 * License
 * -------
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Frontier contributors.
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_report.h"

#include "../frontier-cli/ut_sync.h"

#define FIXTURE_DIR "fixtures/ut_sync"

/* Read an entire file into a malloc'd buffer. Returns 1 on success. The
 * buffer is NUL-terminated (NUL not counted in len). On failure returns 0.
 * A missing file is a hard failure (fixtures must exist). */
static int read_file(const char *path, unsigned char **buf, size_t *len) {
	FILE *f = fopen(path, "rb");
	long sz;
	unsigned char *data;
	size_t got;

	*buf = NULL;
	*len = 0;
	if (f == NULL) {
		fprintf(stderr, "fixture missing: %s\n", path);
		return 0;
	}
	if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return 0; }
	sz = ftell(f);
	if (sz < 0) { fclose(f); return 0; }
	if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return 0; }

	data = (unsigned char *)malloc((size_t)sz + 1);
	if (data == NULL) { fclose(f); return 0; }
	got = fread(data, 1, (size_t)sz, f);
	fclose(f);
	if (got != (size_t)sz) { free(data); return 0; }
	data[sz] = '\0';
	*buf = data;
	*len = (size_t)sz;
	return 1;
}

/* Print a byte buffer with non-printables escaped, for failure diagnostics. */
static void dump_escaped(const char *label, const unsigned char *b, size_t n) {
	size_t i;
	fprintf(stderr, "  %s (%zu bytes): ", label, n);
	for (i = 0; i < n && i < 200; i++) {
		unsigned char c = b[i];
		if (c == '\n') fprintf(stderr, "\\n");
		else if (c == '\r') fprintf(stderr, "\\r");
		else if (c == '\t') fprintf(stderr, "\\t");
		else if (c >= 0x20 && c < 0x7f) fputc(c, stderr);
		else fprintf(stderr, "\\x%02x", c);
	}
	if (n > 200) fprintf(stderr, "...");
	fprintf(stderr, "\n");
}

/*
 * check_lossless_fixture - for a lossless fixture, verify:
 *   (a) forward(in) == expected  (sanity: the forward test already covers this,
 *       but we re-run it here so a single test binary is self-contained)
 *   (b) reverse(expected) == in  (byte-exact inversion)
 */
static void check_lossless_fixture(const char *name) {
	char in_path[512];
	char exp_path[512];
	unsigned char *in = NULL, *expected = NULL;
	unsigned char *fwd = NULL, *rev = NULL;
	size_t in_len = 0, exp_len = 0, fwd_len = 0, rev_len = 0;
	int ok;

	snprintf(in_path,  sizeof(in_path),  "%s/%s.in",       FIXTURE_DIR, name);
	snprintf(exp_path, sizeof(exp_path), "%s/%s.expected",  FIXTURE_DIR, name);

	assert(read_file(in_path,  &in,       &in_len));
	assert(read_file(exp_path, &expected, &exp_len));

	/* (a) forward sanity */
	ok = ut_canonicalize_outline_text(in, in_len, &fwd, &fwd_len);
	assert(ok == 1 && fwd != NULL);
	if (fwd_len != exp_len || memcmp(fwd, expected, exp_len) != 0) {
		fprintf(stderr, "forward MISMATCH for '%s':\n", name);
		dump_escaped("expected", expected, exp_len);
		dump_escaped("got     ", fwd, fwd_len);
		assert(0);
	}

	/* (b) reverse: expected -> should reproduce in */
	ok = ut_decanonicalize_outline_text(expected, exp_len, &rev, &rev_len);
	if (!ok) {
		fprintf(stderr, "reverse returned 0 for lossless fixture '%s'\n", name);
		assert(0);
	}
	assert(rev != NULL);
	if (rev_len != in_len || memcmp(rev, in, in_len) != 0) {
		fprintf(stderr, "reverse MISMATCH for lossless fixture '%s':\n", name);
		dump_escaped("original .in", in, in_len);
		dump_escaped("reverse  got", rev, rev_len);
	}
	assert(rev_len == in_len);
	assert(memcmp(rev, in, in_len) == 0);

	free(in); free(expected); free(fwd); free(rev);
}

/*
 * check_fixedpoint_fixture - for a lossy fixture, verify:
 *   (a) forward(in) == expected          (sanity)
 *   (b) reverse(expected) succeeds       (no crash)
 *   (c) forward(reverse(expected)) == expected  (canonical form is a fixed point)
 */
static void check_fixedpoint_fixture(const char *name) {
	char in_path[512];
	char exp_path[512];
	unsigned char *in = NULL, *expected = NULL;
	unsigned char *fwd = NULL, *rev = NULL, *refwd = NULL;
	size_t in_len = 0, exp_len = 0, fwd_len = 0, rev_len = 0, refwd_len = 0;
	int ok;

	snprintf(in_path,  sizeof(in_path),  "%s/%s.in",       FIXTURE_DIR, name);
	snprintf(exp_path, sizeof(exp_path), "%s/%s.expected",  FIXTURE_DIR, name);

	assert(read_file(in_path,  &in,       &in_len));
	assert(read_file(exp_path, &expected, &exp_len));

	/* (a) forward sanity */
	ok = ut_canonicalize_outline_text(in, in_len, &fwd, &fwd_len);
	assert(ok == 1 && fwd != NULL);
	if (fwd_len != exp_len || memcmp(fwd, expected, exp_len) != 0) {
		fprintf(stderr, "forward MISMATCH for '%s':\n", name);
		dump_escaped("expected", expected, exp_len);
		dump_escaped("got     ", fwd, fwd_len);
		assert(0);
	}

	/* (b) reverse */
	ok = ut_decanonicalize_outline_text(expected, exp_len, &rev, &rev_len);
	if (!ok) {
		fprintf(stderr, "reverse returned 0 for fixture '%s'\n", name);
		assert(0);
	}
	assert(rev != NULL);

	/* (c) fixed-point: forward(reverse(expected)) == expected */
	ok = ut_canonicalize_outline_text(rev, rev_len, &refwd, &refwd_len);
	assert(ok == 1 && refwd != NULL);
	if (refwd_len != exp_len || memcmp(refwd, expected, exp_len) != 0) {
		fprintf(stderr, "fixed-point FAILED for lossy fixture '%s':\n", name);
		fprintf(stderr, "  expected canonical form (the .expected file):\n");
		dump_escaped("  expected", expected, exp_len);
		fprintf(stderr, "  reverse output (in-memory form):\n");
		dump_escaped("  reverse ", rev, rev_len);
		fprintf(stderr, "  re-forward of reverse:\n");
		dump_escaped("  re-fwd  ", refwd, refwd_len);
	}
	assert(refwd_len == exp_len);
	assert(memcmp(refwd, expected, exp_len) == 0);

	free(in); free(expected); free(fwd); free(rev); free(refwd);
}

/* Fixture round-trip tests (lossless) */
static void test_roundtrip_simple(void)          { check_lossless_fixture("simple"); }
static void test_roundtrip_highbit(void)         { check_lossless_fixture("highbit"); }
static void test_roundtrip_marker_in_string(void){ check_lossless_fixture("marker_in_string"); }
static void test_roundtrip_marker_in_char(void)  { check_lossless_fixture("marker_in_char"); }
static void test_roundtrip_empty(void)           { check_lossless_fixture("empty"); }

/* Lossy fixtures: fixed-point only (forward(reverse(expected)) == expected) */
static void test_fixedpoint_comment_markers(void){ check_fixedpoint_fixture("comment_markers"); }
static void test_fixedpoint_brace_strip(void)    { check_fixedpoint_fixture("brace_strip"); }
static void test_fixedpoint_trailing_nl(void)    { check_fixedpoint_fixture("trailing_nl"); }
/* corpus_edit: .in has ASCII "//" notation; reverse emits 0xC7 (correct
 * kernel form) which differs from .in. Fixed-point holds. See header comment
 * for full rationale. */
static void test_fixedpoint_corpus_edit(void)    { check_fixedpoint_fixture("corpus_edit"); }

/*
 * Literal-state crux: "//" inside a normal "..." string must NOT be converted
 * to 0xC7. The string `x = "a//b"` in .ut form should reverse to bytes
 * x = "a//b" with the "//" kept as two 0x2F bytes, NOT a single 0xC7.
 */
static void test_slash_slash_in_double_quote_string(void) {
	/* .ut form: x = "a//b" (all ASCII) */
	const unsigned char in[] = "x = \"a//b\"";
	size_t inlen = sizeof(in) - 1; /* exclude NUL terminator */
	unsigned char *out = NULL;
	size_t outlen = 0;
	int ok;
	size_t i;
	int found_c7 = 0;
	int found_slashslash = 0;

	ok = ut_decanonicalize_outline_text(in, inlen, &out, &outlen);
	assert(ok == 1 && out != NULL);

	/* Scan output for 0xC7 (should not appear -- the "//" was inside a string) */
	for (i = 0; i < outlen; i++) {
		if (out[i] == 0xC7) {
			found_c7 = 1;
		}
		if (i + 1 < outlen && out[i] == 0x2F && out[i+1] == 0x2F) {
			found_slashslash = 1;
		}
	}

	if (found_c7) {
		fprintf(stderr,
		    "FAIL: in-string '//' was converted to 0xC7 (should be preserved)\n");
		dump_escaped("input ", in, inlen);
		dump_escaped("output", out, outlen);
	}
	assert(!found_c7);
	assert(found_slashslash); /* the "//" must survive as two 0x2F bytes */

	free(out);
}

/*
 * Literal-state crux: a "//" that starts a comment (outside any literal) MUST
 * become a single 0xC7 byte.
 */
static void test_slash_slash_comment_becomes_c7(void) {
	/* .ut form: "// hi" (comment) */
	const unsigned char in[] = "// hi";
	size_t inlen = sizeof(in) - 1;
	unsigned char *out = NULL;
	size_t outlen = 0;
	int ok;

	ok = ut_decanonicalize_outline_text(in, inlen, &out, &outlen);
	assert(ok == 1 && out != NULL);

	if (outlen == 0 || out[0] != 0xC7) {
		fprintf(stderr,
		    "FAIL: leading '//' comment should become 0xC7, got:\n");
		dump_escaped("output", out, outlen);
	}
	assert(outlen >= 1);
	assert(out[0] == 0xC7); /* "//" -> 0xC7 */
	/* rest of the line " hi" should follow as-is */
	assert(outlen == 4); /* 0xC7 + ' ' + 'h' + 'i' */
	assert(out[1] == ' ' && out[2] == 'h' && out[3] == 'i');

	free(out);
}

/*
 * Literal-state crux: "//" inside a curly-quote string (bytes 0xD2...0xD3 in
 * MacRoman, U+201C...U+201D in Unicode) must be preserved as "//" in the output.
 * In .ut form the curly-quote string is already UTF-8: the open curly quote is
 * 0xE2 0x80 0x9C (U+201C) and the close is 0xE2 0x80 0x9D (U+201D).
 */
static void test_slash_slash_in_curly_quote_string(void) {
	/* .ut form (UTF-8): <U+201C>a//b<U+201D> */
	/* U+201C = E2 80 9C, U+201D = E2 80 9D */
	const unsigned char in[] = {
		0xE2, 0x80, 0x9C,   /* open curly quote */
		'a', '/', '/', 'b',
		0xE2, 0x80, 0x9D,   /* close curly quote */
		0x00
	};
	size_t inlen = sizeof(in) - 1;
	unsigned char *out = NULL;
	size_t outlen = 0;
	int ok;
	size_t i;
	int found_c7 = 0;

	ok = ut_decanonicalize_outline_text(in, inlen, &out, &outlen);
	assert(ok == 1 && out != NULL);

	for (i = 0; i < outlen; i++) {
		if (out[i] == 0xC7) {
			found_c7 = 1;
		}
	}
	if (found_c7) {
		fprintf(stderr,
		    "FAIL: '//' inside curly-quote string was converted to 0xC7\n");
		dump_escaped("input ", in, inlen);
		dump_escaped("output", out, outlen);
	}
	assert(!found_c7);

	/* The open curly quote (U+201C) should decode back to MacRoman 0xD2 */
	assert(outlen >= 1 && out[0] == 0xD2);

	free(out);
}

/*
 * Literal-state crux: "//" inside a char literal '...' must be preserved.
 * Input: x = '/' (the second '/' after the char literal opens and closes
 * with a single quote pair). Actually we need a char literal that CONTAINS
 * a "//" sequence -- but '/' is a single char. Use a more realistic example:
 * the literal state protects single-slash chars. The key test is: after an
 * opening single quote, a "/" is inside the literal until the closing "'".
 */
static void test_slash_slash_in_char_literal(void) {
	/* .ut form: c = '/' + '/' -- two char literals each containing one slash.
	 * Neither char literal contains "//", so there is no in-literal "//" to test.
	 * The actual test case: use a backslash-escaped char literal that spans
	 * past the first slash. E.g., x = '\/' (backslash then slash inside '...').
	 * After the backslash the next byte is consumed as escaped, so the literal
	 * goes: open quote, backslash, '/', close quote. Then a free "//x" after.
	 * The in-literal "/" is NOT a "//" and thus not converted anyway.
	 *
	 * Better: use a compound expression where a char literal sits before "//":
	 *   c = 'x'; //comment
	 * The "//" after the semicolon is outside the literal and should become 0xC7.
	 * The "//" is NOT inside the char literal.
	 *
	 * For a true in-char-literal "//" test: we need TWO consecutive slashes
	 * inside a char literal. UserTalk char literals hold a single char, but
	 * the literal state machine just tracks delimiters -- it does not enforce
	 * single-char content. So: c = '//'; tests '//' inside '...'.
	 */
	const unsigned char in[] = "c = '//'; //comment";
	size_t inlen = sizeof(in) - 1;
	unsigned char *out = NULL;
	size_t outlen = 0;
	int ok;
	size_t i;
	int c7_count = 0;

	ok = ut_decanonicalize_outline_text(in, inlen, &out, &outlen);
	assert(ok == 1 && out != NULL);

	for (i = 0; i < outlen; i++) {
		if (out[i] == 0xC7) c7_count++;
	}

	/* Exactly ONE 0xC7: the "//comment" after the semicolon.
	 * The "//" inside '...' must be preserved as two 0x2F bytes. */
	if (c7_count != 1) {
		fprintf(stderr,
		    "FAIL: expected exactly 1 x0xC7 (the comment), got %d\n", c7_count);
		dump_escaped("input ", in, inlen);
		dump_escaped("output", out, outlen);
	}
	assert(c7_count == 1);

	/* Verify the "//" inside '...' survived as 0x2F 0x2F */
	{
		int in_char_slash_slash = 0;
		for (i = 0; i + 1 < outlen; i++) {
			if (out[i] == '\'' ) {
				/* look ahead for // */
				if (i + 2 < outlen && out[i+1] == '/' && out[i+2] == '/') {
					in_char_slash_slash = 1;
				}
			}
		}
		assert(in_char_slash_slash);
	}

	free(out);
}

/*
 * High-bit MacRoman round-trip: U+00E9 (e-acute) is MacRoman 0x8E.
 * The .ut form encodes it as UTF-8: 0xC3 0xA9. The reverse must decode
 * 0xC3 0xA9 -> U+00E9 -> MacRoman 0x8E.
 */
static void test_highbit_macroman_roundtrip(void) {
	/* UTF-8 for U+00E9 (e-acute): 0xC3 0xA9 */
	const unsigned char in[] = { 'c', 'a', 'f', 0xC3, 0xA9, 0x00 };
	size_t inlen = sizeof(in) - 1;
	unsigned char *out = NULL;
	size_t outlen = 0;
	int ok;

	ok = ut_decanonicalize_outline_text(in, inlen, &out, &outlen);
	assert(ok == 1 && out != NULL);

	/* Should produce 4 bytes: 'c' 'a' 'f' 0x8E */
	if (outlen != 4 || out[3] != 0x8E) {
		fprintf(stderr, "FAIL: e-acute (U+00E9) should decode to MacRoman 0x8E\n");
		dump_escaped("input ", in, inlen);
		dump_escaped("output", out, outlen);
	}
	assert(outlen == 4);
	assert(out[0] == 'c' && out[1] == 'a' && out[2] == 'f' && out[3] == 0x8E);

	free(out);
}

/*
 * Non-MacRoman rejection: U+1F600 (emoji) has no MacRoman representation.
 * The reverse function must return 0 and set *out = NULL, *outlen = 0.
 */
static void test_non_macroman_rejected(void) {
	/* UTF-8 for U+1F600: 0xF0 0x9F 0x98 0x80 */
	const unsigned char in[] = { 'x', 0xF0, 0x9F, 0x98, 0x80, 0x00 };
	size_t inlen = sizeof(in) - 1;
	unsigned char *out = (unsigned char *)0x1; /* sentinel: should be set to NULL */
	size_t outlen = 99;                         /* sentinel: should be set to 0 */
	int ok;

	ok = ut_decanonicalize_outline_text(in, inlen, &out, &outlen);
	if (ok != 0) {
		fprintf(stderr, "FAIL: non-MacRoman emoji should cause return 0, got %d\n", ok);
	}
	assert(ok == 0);
	assert(out == NULL);
	assert(outlen == 0);
}

/*
 * LF -> CR: a simple LF-terminated line should reverse to CR-terminated.
 */
static void test_lf_to_cr(void) {
	const unsigned char in[] = "line1\nline2";
	size_t inlen = sizeof(in) - 1;
	unsigned char *out = NULL;
	size_t outlen = 0;
	int ok;

	ok = ut_decanonicalize_outline_text(in, inlen, &out, &outlen);
	assert(ok == 1 && out != NULL);
	assert(outlen == inlen);
	assert(out[5] == 0x0D); /* LF -> CR */
	assert(out[0] == 'l' && out[6] == 'l');

	free(out);
}

/*
 * Inline "//" mid-line comment: "x = 1 //comment" -> "x = 1 \xC7comment"
 * The "//" appears outside any literal, so it should become 0xC7.
 */
static void test_inline_comment(void) {
	const unsigned char in[] = "x = 1 //comment";
	size_t inlen = sizeof(in) - 1;
	unsigned char *out = NULL;
	size_t outlen = 0;
	int ok;
	size_t c7pos = 0;
	int found_c7 = 0;
	size_t i;

	ok = ut_decanonicalize_outline_text(in, inlen, &out, &outlen);
	assert(ok == 1 && out != NULL);

	for (i = 0; i < outlen; i++) {
		if (out[i] == 0xC7) {
			found_c7 = 1;
			c7pos = i;
		}
	}
	if (!found_c7) {
		fprintf(stderr, "FAIL: inline '//' should become 0xC7\n");
		dump_escaped("input ", in, inlen);
		dump_escaped("output", out, outlen);
	}
	assert(found_c7);
	/* The 0xC7 replaces two bytes with one, so output is 1 byte shorter */
	assert(outlen == inlen - 1);
	/* " //comment" -> " \xC7comment": the 0xC7 should be at position 6 */
	assert(c7pos == 6);

	free(out);
}

/*
 * Literal state resets at newline: "//" in a string on line 1 is preserved;
 * "//" at the start of line 2 is a comment and becomes 0xC7.
 */
static void test_literal_reset_at_newline(void) {
	const unsigned char in[] = "s = \"a//b\"\n//comment";
	size_t inlen = sizeof(in) - 1;
	unsigned char *out = NULL;
	size_t outlen = 0;
	int ok;
	size_t i;
	int c7_count = 0;

	ok = ut_decanonicalize_outline_text(in, inlen, &out, &outlen);
	assert(ok == 1 && out != NULL);

	for (i = 0; i < outlen; i++) {
		if (out[i] == 0xC7) c7_count++;
	}
	if (c7_count != 1) {
		fprintf(stderr,
		    "FAIL: expected 1 x0xC7 (line-2 comment), got %d\n", c7_count);
		dump_escaped("input ", in, inlen);
		dump_escaped("output", out, outlen);
	}
	assert(c7_count == 1);

	/* The in-string "//" on line 1 should survive as 0x2F 0x2F */
	{
		int found_slash_in_string = 0;
		for (i = 0; i + 1 < outlen; i++) {
			if (out[i] == '"' && i + 2 < outlen &&
			    out[i+1] != '"') {
				/* inside string after open quote -- look for // */
				size_t j = i + 1;
				while (j < outlen && out[j] != '"' && out[j] != 0x0D) {
					if (j + 1 < outlen && out[j] == '/' && out[j+1] == '/') {
						found_slash_in_string = 1;
						break;
					}
					j++;
				}
			}
		}
		assert(found_slash_in_string);
	}

	free(out);
}

/*
 * Backslash escape inside string: backslash before a char makes the next char
 * opaque to the literal state machine. A backslash before '"' does NOT close
 * the string. So text after a '\"' sequence is still inside the string and
 * any "//" there stays as "//".
 */
static void test_backslash_escape_in_string(void) {
	/* s = "a\"//b" -- the '\"' does not end the string; the "//" is inside */
	const unsigned char in[] = "s = \"a\\\"//b\"";
	size_t inlen = sizeof(in) - 1;
	unsigned char *out = NULL;
	size_t outlen = 0;
	int ok;
	size_t i;
	int found_c7 = 0;

	ok = ut_decanonicalize_outline_text(in, inlen, &out, &outlen);
	assert(ok == 1 && out != NULL);

	for (i = 0; i < outlen; i++) {
		if (out[i] == 0xC7) {
			found_c7 = 1;
		}
	}
	if (found_c7) {
		fprintf(stderr,
		    "FAIL: '//' after backslash-escaped quote should stay as '//'\n");
		dump_escaped("input ", in, inlen);
		dump_escaped("output", out, outlen);
	}
	assert(!found_c7);

	free(out);
}

int main(void) {
	TR_INIT("ut_sync_decanonicalize_tests");

	/* Fixture round-trip tests (lossless) */
	TR_RUN(test_roundtrip_simple);
	TR_RUN(test_roundtrip_highbit);
	TR_RUN(test_roundtrip_marker_in_string);
	TR_RUN(test_roundtrip_marker_in_char);
	TR_RUN(test_roundtrip_empty);

	/* Fixture round-trip tests (lossy: fixed-point) */
	TR_RUN(test_fixedpoint_comment_markers);
	TR_RUN(test_fixedpoint_brace_strip);
	TR_RUN(test_fixedpoint_trailing_nl);
	TR_RUN(test_fixedpoint_corpus_edit);

	/* Targeted literal-state tests */
	TR_RUN(test_slash_slash_in_double_quote_string);
	TR_RUN(test_slash_slash_comment_becomes_c7);
	TR_RUN(test_slash_slash_in_curly_quote_string);
	TR_RUN(test_slash_slash_in_char_literal);
	TR_RUN(test_literal_reset_at_newline);
	TR_RUN(test_backslash_escape_in_string);

	/* Other transform correctness */
	TR_RUN(test_lf_to_cr);
	TR_RUN(test_inline_comment);
	TR_RUN(test_highbit_macroman_roundtrip);
	TR_RUN(test_non_macroman_rejected);

	TR_SUMMARY();
	return TR_EXIT_CODE();
}
