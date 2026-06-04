/*
 * ut_sync.c - Bidirectional ODB <-> .ut sync: canonicalizer + path mapping.
 *
 * See ut_sync.h for the full transform contract. This file implements the
 * outline-text -> .ut canonicalizer as a pure, kernel-independent byte
 * transform, ported from tools/verify_virgin_root_sync.py
 * (normalize_kernel_body) so the two stay byte-identical on the real corpus.
 *
 * License
 * -------
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Frontier contributors.
 */

#include "ut_sync.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

/*
 * MacRoman -> UTF-8 for the 128 high-bit code points (0x80..0xFF). The
 * low 128 (0x00..0x7F) are ASCII and pass through unchanged. This table
 * holds the Unicode scalar value for each MacRoman byte 0x80..0xFF, taken
 * from the Unicode Consortium's ROMAN.TXT mapping. We encode to UTF-8
 * inline rather than depending on the kernel's TEC-backed macromantoutf8,
 * keeping the canonicalizer a pure standalone function (testable without a
 * live runtime, identical result on every platform).
 */
static const unsigned short kMacRomanToUnicode[128] = {
	0x00C4, 0x00C5, 0x00C7, 0x00C9, 0x00D1, 0x00D6, 0x00DC, 0x00E1, /* 80-87 */
	0x00E0, 0x00E2, 0x00E4, 0x00E3, 0x00E5, 0x00E7, 0x00E9, 0x00E8, /* 88-8F */
	0x00EA, 0x00EB, 0x00ED, 0x00EC, 0x00EE, 0x00EF, 0x00F1, 0x00F3, /* 90-97 */
	0x00F2, 0x00F4, 0x00F6, 0x00F5, 0x00FA, 0x00F9, 0x00FB, 0x00FC, /* 98-9F */
	0x2020, 0x00B0, 0x00A2, 0x00A3, 0x00A7, 0x2022, 0x00B6, 0x00DF, /* A0-A7 */
	0x00AE, 0x00A9, 0x2122, 0x00B4, 0x00A8, 0x2260, 0x00C6, 0x00D8, /* A8-AF */
	0x221E, 0x00B1, 0x2264, 0x2265, 0x00A5, 0x00B5, 0x2202, 0x2211, /* B0-B7 */
	0x220F, 0x03C0, 0x222B, 0x00AA, 0x00BA, 0x03A9, 0x00E6, 0x00F8, /* B8-BF */
	0x00BF, 0x00A1, 0x00AC, 0x221A, 0x0192, 0x2248, 0x2206, 0x00AB, /* C0-C7 */
	0x00BB, 0x2026, 0x00A0, 0x00C0, 0x00C3, 0x00D5, 0x0152, 0x0153, /* C8-CF */
	0x2013, 0x2014, 0x201C, 0x201D, 0x2018, 0x2019, 0x00F7, 0x25CA, /* D0-D7 */
	0x00FF, 0x0178, 0x2044, 0x20AC, 0x2039, 0x203A, 0xFB01, 0xFB02, /* D8-DF */
	0x2021, 0x00B7, 0x201A, 0x201E, 0x2030, 0x00C2, 0x00CA, 0x00C1, /* E0-E7 */
	0x00CB, 0x00C8, 0x00CD, 0x00CE, 0x00CF, 0x00CC, 0x00D3, 0x00D4, /* E8-EF */
	0xF8FF, 0x00D2, 0x00DA, 0x00DB, 0x00D9, 0x0131, 0x02C6, 0x02DC, /* F0-F7 */
	0x00AF, 0x02D8, 0x02D9, 0x02DA, 0x00B8, 0x02DD, 0x02DB, 0x02C7  /* F8-FF */
};

/* MacRoman bytes that, post-decode, are the curly-quote string delimiters. */
#define MACROMAN_OPEN_CURLY  0xD2  /* -> U+201C, opens a smart-quote string */
#define MACROMAN_CLOSE_CURLY 0xD3  /* -> U+201D, closes it */
#define MACROMAN_CHCOMMENT   0xC7  /* -> "//"  (comment start) */
#define MACROMAN_CHENDCOMMENT 0xC8 /* dropped  (comment end) */

/* Append the UTF-8 encoding of a Unicode scalar to a growable buffer. */
static int append_utf8(unsigned char **buf, size_t *len, size_t *cap,
                       unsigned int cp) {
	unsigned char tmp[4];
	size_t n;

	if (cp < 0x80) {
		tmp[0] = (unsigned char)cp;
		n = 1;
	} else if (cp < 0x800) {
		tmp[0] = (unsigned char)(0xC0 | (cp >> 6));
		tmp[1] = (unsigned char)(0x80 | (cp & 0x3F));
		n = 2;
	} else if (cp < 0x10000) {
		tmp[0] = (unsigned char)(0xE0 | (cp >> 12));
		tmp[1] = (unsigned char)(0x80 | ((cp >> 6) & 0x3F));
		tmp[2] = (unsigned char)(0x80 | (cp & 0x3F));
		n = 3;
	} else {
		tmp[0] = (unsigned char)(0xF0 | (cp >> 18));
		tmp[1] = (unsigned char)(0x80 | ((cp >> 12) & 0x3F));
		tmp[2] = (unsigned char)(0x80 | ((cp >> 6) & 0x3F));
		tmp[3] = (unsigned char)(0x80 | (cp & 0x3F));
		n = 4;
	}

	if (*len + n + 1 > *cap) {
		size_t newcap = (*cap == 0) ? 256 : *cap * 2;
		unsigned char *grown;
		while (newcap < *len + n + 1)
			newcap *= 2;
		grown = (unsigned char *)realloc(*buf, newcap);
		if (grown == NULL)
			return 0;
		*buf = grown;
		*cap = newcap;
	}
	memcpy(*buf + *len, tmp, n);
	*len += n;
	return 1;
}

/*
 * Append a single ASCII byte to a growable UTF-8 buffer. Callers only ever
 * pass structural ASCII (CR, LF, '/'); a non-ASCII byte would be silently
 * re-encoded as multi-byte UTF-8 by append_utf8, which is never intended here.
 */
static int append_ascii_byte(unsigned char **buf, size_t *len, size_t *cap,
                             unsigned char b) {
	assert(b < 0x80);
	return append_utf8(buf, len, cap, (unsigned int)b);
}

/*
 * Build a decoded Unicode-codepoint stream from the raw MacRoman bytes,
 * doing the literal-aware 0xC7/0xC8 substitution in the SAME pass.
 *
 * Why fuse decode + substitution: the Python reference decodes to a Unicode
 * string first (step 2), then walks code points (step 3). In C we walk the
 * raw bytes and decide per byte whether it is a comment marker, a string
 * delimiter, or literal data. Because 0xC7/0xC8/0xD2/0xD3 are single bytes
 * in MacRoman, byte-level tracking is exactly equivalent to the Python
 * code-point tracking, and every other high-bit byte is just decoded.
 *
 * Output code points are appended via append_utf8, producing UTF-8 directly.
 * CR/LF are emitted as-is here (step 4 CR->LF happens in the caller's later
 * pass, mirroring the Python ordering: substitution before CR->LF).
 */
static int decode_and_substitute(const unsigned char *in, size_t inlen,
                                 unsigned char **out, size_t *outlen) {
	size_t cap = 0;
	size_t len = 0;
	unsigned char *buf = NULL;
	int in_string = 0;        /* inside "..." or curly-quote string */
	unsigned int string_close = '"';  /* code point that closes the string */
	int in_char = 0;          /* inside '...' */
	int escaped = 0;
	size_t i;

	for (i = 0; i < inlen; i++) {
		unsigned char b = in[i];
		unsigned int cp;

		/* Decode the byte to its Unicode code point. */
		if (b < 0x80)
			cp = (unsigned int)b;
		else
			cp = (unsigned int)kMacRomanToUnicode[b - 0x80];

		/* Line break: literals never span lines; reset literal state. */
		if (b == 0x0D || b == 0x0A) {
			in_string = 0;
			in_char = 0;
			escaped = 0;
			if (!append_ascii_byte(&buf, &len, &cap, b))
				goto fail;
			continue;
		}

		if (in_string || in_char) {
			if (escaped) {
				escaped = 0;
				if (!append_utf8(&buf, &len, &cap, cp))
					goto fail;
				continue;
			}
			if (b == 0x5C) { /* backslash escapes next byte */
				escaped = 1;
				if (!append_utf8(&buf, &len, &cap, cp))
					goto fail;
				continue;
			}
			if (in_string && cp == string_close)
				in_string = 0;
			else if (in_char && b == '\'')
				in_char = 0;
			if (!append_utf8(&buf, &len, &cap, cp))
				goto fail;
			continue;
		}

		/* Not inside any literal. */
		if (b == '"') {
			in_string = 1;
			string_close = '"';
			if (!append_utf8(&buf, &len, &cap, cp))
				goto fail;
			continue;
		}
		if (b == MACROMAN_OPEN_CURLY) {
			in_string = 1;
			string_close = 0x201D; /* U+201D closes the curly string */
			if (!append_utf8(&buf, &len, &cap, cp))
				goto fail;
			continue;
		}
		if (b == '\'') {
			in_char = 1;
			if (!append_utf8(&buf, &len, &cap, cp))
				goto fail;
			continue;
		}
		if (b == MACROMAN_CHCOMMENT) {
			if (!append_ascii_byte(&buf, &len, &cap, '/') ||
			    !append_ascii_byte(&buf, &len, &cap, '/'))
				goto fail;
			continue;
		}
		if (b == MACROMAN_CHENDCOMMENT) {
			/* comment-end marker dropped (// runs to EOL) */
			continue;
		}
		if (!append_utf8(&buf, &len, &cap, cp))
			goto fail;
	}

	if (buf == NULL) {
		/* Empty input: hand back a valid empty buffer. */
		buf = (unsigned char *)malloc(1);
		if (buf == NULL)
			return 0;
		cap = 1;
	}
	buf[len] = '\0';
	*out = buf;
	*outlen = len;
	return 1;

fail:
	free(buf);
	*out = NULL;
	*outlen = 0;
	return 0;
}

/*
 * In-place: CR -> LF over a UTF-8 buffer. CR is a single byte (0x0D) that
 * never appears as a UTF-8 continuation or lead byte, so byte-level
 * replacement is safe.
 */
static void cr_to_lf(unsigned char *buf, size_t len) {
	size_t i;
	for (i = 0; i < len; i++) {
		if (buf[i] == 0x0D)
			buf[i] = 0x0A;
	}
}

/*
 * Strip outline structure markers that wrap all-comment subtrees, matching
 * the Python regex:
 *
 *   re.sub(r"(^|\n)([ \t]*?)( ?[{}]+;? ?)(?=//)", r"\1\2", decoded)
 *
 * i.e. at start-of-buffer or right after a LF, keep the indentation run of
 * spaces/tabs, then delete: an optional single space, one-or-more of "{"/"}",
 * an optional ";", an optional single space -- but only when the very next
 * two bytes are "//". The leading indentation is the MINIMAL run (the
 * regex's [ \t]*? is non-greedy, so the optional space in group 3 absorbs
 * one trailing space of the indentation when present).
 *
 * Implemented as a single forward scan writing in place (output is never
 * longer than input). Returns the new length.
 */
static size_t strip_structure_markers(unsigned char *buf, size_t len) {
	size_t r = 0; /* read cursor */
	size_t w = 0; /* write cursor */

	while (r < len) {
		int at_line_start = (r == 0) || (buf[r - 1] == 0x0A);

		if (at_line_start) {
			/*
			 * Try to match the strip pattern starting here. The regex is
			 * anchored after the (^|\n); we are positioned at the first byte
			 * of the line content. Group 2 ([ \t]*?) is non-greedy and group
			 * 3 begins with an optional space, so the boundary between
			 * "kept indentation" and "stripped run" is: keep the maximal
			 * indentation that still leaves a valid group-3 match, where
			 * group 3 = (optional space)(one+ braces)(optional ;)(optional
			 * space) immediately followed by "//".
			 *
			 * Because [ \t]*? is non-greedy it keeps as LITTLE as possible,
			 * but the overall match must succeed, so the engine keeps only
			 * what group 3 cannot consume. Group 3's leading " ?" consumes at
			 * most one space; its brace run consumes no spaces/tabs. So the
			 * kept indentation is: all leading spaces/tabs EXCEPT that if the
			 * last indentation char before the brace run is a single space,
			 * group 3's " ?" claims it (it is stripped). Concretely: scan the
			 * leading [ \t]* run, then require a brace run; the char
			 * immediately before the braces, if it is a space, is eaten by
			 * group 3.
			 */
			size_t p = r;
			size_t indent_end;
			size_t brace_start;
			size_t q;
			int saw_brace = 0;

			while (p < len && (buf[p] == ' ' || buf[p] == '\t'))
				p++;
			indent_end = p; /* one past last space/tab */

			/* group 3: optional single space already folded into indent_end;
			 * the regex's " ?" sits at the START of group 3, i.e. it can
			 * claim one space that is part of the [ \t]* run. We model this
			 * by allowing the brace run to be preceded by an optional space
			 * that we will strip. */
			brace_start = indent_end;

			q = brace_start;
			while (q < len && (buf[q] == '{' || buf[q] == '}')) {
				saw_brace = 1;
				q++;
			}
			if (saw_brace) {
				if (q < len && buf[q] == ';')
					q++;
				if (q < len && buf[q] == ' ')
					q++;
				/* must be immediately followed by "//" */
				if (q + 1 < len && buf[q] == '/' && buf[q + 1] == '/') {
					/*
					 * Match. Kept = indentation minus an optional trailing
					 * single space that group 3's leading " ?" claims.
					 * indent_end points one past the last [ \t]; if that last
					 * char is a space, group 3 eats it.
					 */
					size_t keep_end = indent_end;
					if (keep_end > r && buf[keep_end - 1] == ' ')
						keep_end--;
					/* copy kept indentation [r, keep_end) */
					if (w != r)
						memmove(buf + w, buf + r, keep_end - r);
					w += (keep_end - r);
					/* skip the stripped run; resume at q (the "//") */
					r = q;
					continue;
				}
			}
		}

		/* default: copy one byte through */
		buf[w++] = buf[r++];
	}

	return w;
}

int ut_canonicalize_outline_text(const unsigned char *in, size_t inlen,
                                 unsigned char **out, size_t *outlen) {
	unsigned char *buf = NULL;
	size_t len = 0;

	if (out == NULL || outlen == NULL)
		return 0;
	*out = NULL;
	*outlen = 0;

	/* Steps 2+3 fused: MacRoman->UTF-8 decode with literal-aware 0xC7/0xC8
	 * substitution. (Step 1 JSON-unescape is intentionally omitted; see
	 * ut_sync.h.) */
	if (!decode_and_substitute(in, inlen, &buf, &len))
		return 0;

	/* Step 4: CR -> LF. */
	cr_to_lf(buf, len);

	/* Step 5: strip structure markers around all-comment subtrees. */
	len = strip_structure_markers(buf, len);

	/* Step 6: drop a single trailing LF if present. */
	if (len > 0 && buf[len - 1] == 0x0A)
		len--;

	buf[len] = '\0';
	*out = buf;
	*outlen = len;
	return 1;
}

/* ------------------------------------------------------------------------- */
/* Reverse canonicalizer                                                     */
/* ------------------------------------------------------------------------- */

/*
 * macroman_from_unicode - map a Unicode scalar to its MacRoman byte.
 *
 * ASCII scalars (< 0x80) return the scalar itself cast to unsigned char and
 * set *ok to 1. For scalars in the MacRoman 0x80..0xFF range, we scan
 * kMacRomanToUnicode for a match and return the corresponding byte; *ok is
 * set to 1 on hit, 0 on miss. Scalars >= 0x80 that have no MacRoman encoding
 * (e.g. emoji) set *ok = 0 and the return value is undefined.
 *
 * The table has 128 entries with all distinct Unicode values (verified: no
 * collisions), so a linear scan is correct and always finds the unique match.
 * This function is called once per encoded byte at import time, which is an
 * infrequent operation; scan cost is acceptable.
 */
static unsigned char macroman_from_unicode(unsigned int cp, int *ok) {
	int i;
	if (cp < 0x80) {
		*ok = 1;
		return (unsigned char)cp;
	}
	for (i = 0; i < 128; i++) {
		if ((unsigned int)kMacRomanToUnicode[i] == cp) {
			*ok = 1;
			return (unsigned char)(i + 0x80);
		}
	}
	*ok = 0;
	return 0;
}

/*
 * decode_utf8_scalar - decode one UTF-8 scalar from buf[*pos..inlen).
 *
 * On success: advances *pos past the sequence and returns the scalar.
 * On failure (truncated or invalid sequence): returns 0xFFFFFFFF to signal
 * an error (not a valid Unicode scalar).
 *
 * Accepts well-formed UTF-8 only (1-4 byte sequences). Overlong encodings,
 * surrogates, and scalars above U+10FFFF are rejected (return 0xFFFFFFFF).
 */
static unsigned int decode_utf8_scalar(const unsigned char *buf, size_t inlen,
                                       size_t *pos) {
	unsigned char b0;
	unsigned int cp;
	size_t i;
	size_t nbytes;

	if (*pos >= inlen)
		return 0xFFFFFFFF;

	b0 = buf[*pos];

	if (b0 < 0x80) {
		(*pos)++;
		return (unsigned int)b0;
	}
	if (b0 < 0xC2) {
		/* continuation byte or overlong 2-byte sequence: invalid */
		return 0xFFFFFFFF;
	}
	if (b0 < 0xE0) {
		nbytes = 2;
		cp = (unsigned int)(b0 & 0x1F);
	} else if (b0 < 0xF0) {
		nbytes = 3;
		cp = (unsigned int)(b0 & 0x0F);
	} else if (b0 <= 0xF4) {
		nbytes = 4;
		cp = (unsigned int)(b0 & 0x07);
	} else {
		return 0xFFFFFFFF; /* > U+10FFFF */
	}

	if (*pos + nbytes > inlen)
		return 0xFFFFFFFF; /* truncated */

	for (i = 1; i < nbytes; i++) {
		unsigned char cb = buf[*pos + i];
		if ((cb & 0xC0) != 0x80)
			return 0xFFFFFFFF; /* not a continuation byte */
		cp = (cp << 6) | (unsigned int)(cb & 0x3F);
	}

	/* Reject overlong encodings: a scalar must use the shortest sequence.
	 * (The 2-byte overlong case is already excluded by the b0 < 0xC2 test.) */
	if (nbytes == 3 && cp < 0x800)
		return 0xFFFFFFFF;
	if (nbytes == 4 && cp < 0x10000)
		return 0xFFFFFFFF;

	/* Reject surrogates and values above U+10FFFF */
	if (cp >= 0xD800 && cp <= 0xDFFF)
		return 0xFFFFFFFF;
	if (cp > 0x10FFFF)
		return 0xFFFFFFFF;

	*pos += nbytes;
	return cp;
}

/*
 * append_byte_rev - append one byte to the growable output buffer.
 * Returns 1 on success, 0 on allocation failure.
 */
static int append_byte_rev(unsigned char **buf, size_t *len, size_t *cap,
                            unsigned char b) {
	if (*len + 2 > *cap) { /* +2 for the byte and the NUL sentinel */
		size_t newcap = (*cap == 0) ? 256 : *cap * 2;
		unsigned char *grown;
		while (newcap < *len + 2)
			newcap *= 2;
		grown = (unsigned char *)realloc(*buf, newcap);
		if (grown == NULL)
			return 0;
		*buf = grown;
		*cap = newcap;
	}
	(*buf)[(*len)++] = b;
	return 1;
}

int ut_decanonicalize_outline_text(const unsigned char *in, size_t inlen,
                                   unsigned char **out, size_t *outlen) {
	unsigned char *buf = NULL;
	size_t len = 0;
	size_t cap = 0;
	size_t pos = 0;

	/*
	 * Literal state, mirroring decode_and_substitute in the forward pass.
	 * We track state at the code-point level (post-UTF8-decode) because the
	 * curly-quote string delimiters are multi-byte in UTF-8 but single code
	 * points at the scalar level (U+201C opens, U+201D closes).
	 */
	int in_string = 0;           /* inside "..." or curly-quote string */
	unsigned int string_close = '"'; /* scalar that closes the current string */
	int in_char   = 0;           /* inside '...' char literal */
	int escaped   = 0;           /* next code point is escaped */

	if (out == NULL || outlen == NULL)
		return 0;
	*out = NULL;
	*outlen = 0;

	while (pos < inlen) {
		unsigned int cp = decode_utf8_scalar(in, inlen, &pos);
		unsigned char mac_byte;
		int mac_ok;

		if (cp == 0xFFFFFFFF) {
			/* Malformed UTF-8 */
			free(buf);
			*out = NULL;
			*outlen = 0;
			return 0;
		}

		/*
		 * LF -> CR: before literal state and everything else, map line endings.
		 * Literal state resets at each line break (same as the forward pass).
		 */
		if (cp == 0x0A) {
			in_string = 0;
			in_char   = 0;
			escaped   = 0;
			if (!append_byte_rev(&buf, &len, &cap, 0x0D))
				goto fail;
			continue;
		}

		/*
		 * Inside a literal: pass bytes through as MacRoman. Backslash escapes
		 * the following code point. Closing delimiter ends the literal.
		 *
		 * Note: 0xC7 and 0xC8 are valid MacRoman bytes that can appear inside
		 * literals. The forward pass preserved them verbatim (encoded to UTF-8)
		 * inside literals. Here we decode them back to MacRoman.
		 */
		if (in_string || in_char) {
			if (escaped) {
				escaped = 0;
				/* Encode back to MacRoman -- backslash was already emitted */
				mac_byte = macroman_from_unicode(cp, &mac_ok);
				if (!mac_ok)
					goto fail_encoding;
				if (!append_byte_rev(&buf, &len, &cap, mac_byte))
					goto fail;
				continue;
			}
			if (cp == 0x5C) { /* backslash: emit it and set escaped */
				escaped = 1;
				mac_byte = macroman_from_unicode(cp, &mac_ok);
				if (!mac_ok)
					goto fail_encoding;
				if (!append_byte_rev(&buf, &len, &cap, mac_byte))
					goto fail;
				continue;
			}
			if (in_string && cp == string_close) {
				in_string = 0;
			} else if (in_char && cp == (unsigned int)'\'') {
				in_char = 0;
			}
			/* Emit the literal data as MacRoman */
			mac_byte = macroman_from_unicode(cp, &mac_ok);
			if (!mac_ok)
				goto fail_encoding;
			if (!append_byte_rev(&buf, &len, &cap, mac_byte))
				goto fail;
			continue;
		}

		/*
		 * Outside any literal.
		 *
		 * Check for "//" (two ASCII 0x2F code points). We need to peek at the
		 * next code point without consuming it. Since "/" is ASCII (single byte
		 * in UTF-8), we can just check in[pos] directly when cp == '/'.
		 */
		if (cp == (unsigned int)'/') {
			if (pos < inlen && in[pos] == '/') {
				/* "//" outside a literal: this is a comment marker -> 0xC7 */
				pos++; /* consume the second '/' */
				if (!append_byte_rev(&buf, &len, &cap, MACROMAN_CHCOMMENT))
					goto fail;
				continue;
			}
			/* Single '/': pass through as-is */
			if (!append_byte_rev(&buf, &len, &cap, '/'))
				goto fail;
			continue;
		}

		/* String and char literal openers */
		if (cp == (unsigned int)'"') {
			in_string = 1;
			string_close = (unsigned int)'"';
			if (!append_byte_rev(&buf, &len, &cap, '"'))
				goto fail;
			continue;
		}
		if (cp == 0x201C) {
			/* U+201C LEFT DOUBLE QUOTATION MARK: opens a curly-quote string.
			 * Closed by U+201D (0x201D). In MacRoman: 0xD2 opens, 0xD3 closes. */
			in_string = 1;
			string_close = 0x201D;
			if (!append_byte_rev(&buf, &len, &cap, MACROMAN_OPEN_CURLY))
				goto fail;
			continue;
		}
		if (cp == (unsigned int)'\'') {
			in_char = 1;
			if (!append_byte_rev(&buf, &len, &cap, '\''))
				goto fail;
			continue;
		}

		/*
		 * Default: decode UTF-8 scalar to MacRoman. ASCII scalars (< 0x80)
		 * map to themselves. High-bit scalars go through the reverse table.
		 */
		mac_byte = macroman_from_unicode(cp, &mac_ok);
		if (!mac_ok)
			goto fail_encoding;
		if (!append_byte_rev(&buf, &len, &cap, mac_byte))
			goto fail;
	}

	/* Allocate at least 1 byte so we can NUL-terminate empty output */
	if (buf == NULL) {
		buf = (unsigned char *)malloc(1);
		if (buf == NULL)
			return 0;
		cap = 1;
	}
	buf[len] = '\0';
	*out = buf;
	*outlen = len;
	return 1;

fail_encoding:
	free(buf);
	*out = NULL;
	*outlen = 0;
	return 0;

fail:
	free(buf);
	*out = NULL;
	*outlen = 0;
	return 0;
}

/* ------------------------------------------------------------------------- */
/* Percent-codec                                                             */
/* ------------------------------------------------------------------------- */

/*
 * Hex digit emit helper. Returns the ASCII hex char for the low nibble of v,
 * uppercase (0-9, A-F).
 */
static char hex_upper(unsigned int v) {
	v &= 0x0f;
	return (v < 10) ? (char)('0' + v) : (char)('A' + v - 10);
}

/*
 * Hex digit parse helper. Returns the value of a single hex digit c (0..15),
 * or -1 if c is not a valid hex digit.
 */
static int hex_val(unsigned char c) {
	if (c >= '0' && c <= '9') return (int)(c - '0');
	if (c >= 'A' && c <= 'F') return (int)(c - 'A' + 10);
	if (c >= 'a' && c <= 'f') return (int)(c - 'a' + 10);
	return -1;
}

int ut_pct_encode_segment(const char *raw, size_t rawlen,
                          char *out, size_t outsz) {
	size_t i;
	size_t pos = 0;

	if (out == NULL || outsz == 0)
		return 0;
	out[0] = '\0';

	for (i = 0; i < rawlen; i++) {
		unsigned char b = (unsigned char)raw[i];
		int escape = 0;
		unsigned int hex_byte = (unsigned int)b;

		/*
		 * Decide whether this byte must be percent-escaped and, if so, which
		 * hex value to emit. The order of checks matters: '%' is tested first
		 * so the escape-the-escape invariant holds.
		 */
		if (b == '%') {
			escape = 1; hex_byte = 0x25;
		} else if (b == '.') {
			escape = 1; hex_byte = 0x2E;
		} else if (b == '/') {
			escape = 1; hex_byte = 0x2F;
		} else if (b == ':') {
			escape = 1; hex_byte = 0x3A;
		} else if (b == '"') {
			escape = 1; hex_byte = 0x22;
		} else if (b == '\\') {
			escape = 1; hex_byte = 0x5C;
		} else if (b < 0x20 || b == 0x7F) {
			/* Control bytes and DEL */
			escape = 1; hex_byte = (unsigned int)b;
		} else if (i == 0 && b == '-') {
			/* Leading dash only */
			escape = 1; hex_byte = 0x2D;
		}

		if (escape) {
			/* Need 3 bytes (%XX) + the final NUL sentinel */
			if (pos + 3 + 1 > outsz) {
				out[0] = '\0';
				return 0;
			}
			out[pos++] = '%';
			out[pos++] = hex_upper(hex_byte >> 4);
			out[pos++] = hex_upper(hex_byte);
		} else {
			/* Need 1 byte + the final NUL sentinel */
			if (pos + 1 + 1 > outsz) {
				out[0] = '\0';
				return 0;
			}
			out[pos++] = (char)b;
		}
	}

	out[pos] = '\0';
	return 1;
}

int ut_pct_decode_segment(const char *enc, char *out, size_t outsz) {
	size_t pos = 0;
	const char *p = enc;

	if (out == NULL || outsz == 0)
		return 0;
	out[0] = '\0';
	if (enc == NULL)
		return 0;

	while (*p != '\0') {
		if (*p == '%') {
			int hi, lo;
			/* Require exactly two following hex digits. */
			if (p[1] == '\0' || p[2] == '\0') {
				out[0] = '\0';
				return 0;
			}
			hi = hex_val((unsigned char)p[1]);
			lo = hex_val((unsigned char)p[2]);
			if (hi < 0 || lo < 0) {
				out[0] = '\0';
				return 0;
			}
			/* Need 1 decoded byte + the final NUL sentinel */
			if (pos + 1 + 1 > outsz) {
				out[0] = '\0';
				return 0;
			}
			out[pos++] = (char)((hi << 4) | lo);
			p += 3;
		} else {
			/* Need 1 literal byte + the final NUL sentinel */
			if (pos + 1 + 1 > outsz) {
				out[0] = '\0';
				return 0;
			}
			out[pos++] = *p++;
		}
	}

	out[pos] = '\0';
	return 1;
}


/* ------------------------------------------------------------------------- */
/* Path mapping                                                              */
/* ------------------------------------------------------------------------- */

/* Reject a single ODB-path segment that cannot safely become an fs component:
 * empty, ".", "..", or containing "/" or a control byte. Returns 1 if safe.
 * Exported (non-static) for use by ut_scan.c post-decode validation. */
int segment_is_safe(const char *seg, size_t len) {
	size_t i;
	if (len == 0)
		return 0;
	if (len == 1 && seg[0] == '.')
		return 0;
	if (len == 2 && seg[0] == '.' && seg[1] == '.')
		return 0;
	for (i = 0; i < len; i++) {
		unsigned char c = (unsigned char)seg[i];
		if (c == '/' || c < 0x20 || c == 0x7f)
			return 0;
	}
	return 1;
}

int ut_odb_path_to_fs(const char *dotted_path, const char *sync_dir,
                      char *out, size_t outsz) {
	size_t dirlen;
	size_t pos = 0;
	const char *seg;
	size_t seglen;
	const char *p;
	int need_sep;

	if (out != NULL && outsz > 0)
		out[0] = '\0';
	if (dotted_path == NULL || sync_dir == NULL || out == NULL || outsz == 0)
		return 0;
	if (dotted_path[0] == '\0')
		return 0;

	/* Copy sync_dir, then a single separator (unless sync_dir already ends
	 * in one). */
	dirlen = strlen(sync_dir);
	if (dirlen + 1 >= outsz)
		return 0;
	memcpy(out, sync_dir, dirlen);
	pos = dirlen;
	need_sep = (dirlen == 0) ? 0 : (sync_dir[dirlen - 1] != '/');
	if (need_sep) {
		out[pos++] = '/';
	}

	/* Walk dotted segments, emitting each as a path component. */
	seg = dotted_path;
	p = dotted_path;
	for (;;) {
		if (*p == '.' || *p == '\0') {
			seglen = (size_t)(p - seg);
			if (!segment_is_safe(seg, seglen))
				goto fail;
			if (pos + seglen >= outsz)
				goto fail;
			memcpy(out + pos, seg, seglen);
			pos += seglen;
			if (*p == '\0')
				break;
			/* segment separator -> "/" */
			if (pos + 1 >= outsz)
				goto fail;
			out[pos++] = '/';
			seg = p + 1;
		}
		p++;
	}

	/* Append ".ut". */
	if (pos + 3 >= outsz)
		goto fail;
	out[pos++] = '.';
	out[pos++] = 'u';
	out[pos++] = 't';
	out[pos] = '\0';
	return 1;

fail:
	out[0] = '\0';
	return 0;
}

int ut_fs_path_to_odb(const char *fs_path, const char *sync_dir,
                      char *out, size_t outsz) {
	size_t dirlen;
	size_t pathlen;
	const char *rel;
	size_t rellen;
	size_t i;
	size_t pos = 0;

	if (out != NULL && outsz > 0)
		out[0] = '\0';
	if (fs_path == NULL || sync_dir == NULL || out == NULL || outsz == 0)
		return 0;

	dirlen = strlen(sync_dir);
	/* Strip a trailing slash on sync_dir for the prefix compare. */
	while (dirlen > 0 && sync_dir[dirlen - 1] == '/')
		dirlen--;
	if (dirlen == 0)
		return 0;

	pathlen = strlen(fs_path);
	/* fs_path must start with sync_dir followed by '/'. */
	if (pathlen <= dirlen + 1)
		return 0;
	if (strncmp(fs_path, sync_dir, dirlen) != 0)
		return 0;
	if (fs_path[dirlen] != '/')
		return 0;

	rel = fs_path + dirlen + 1;
	rellen = strlen(rel);

	/* Require ".ut" suffix; strip it. */
	if (rellen < 3 || strcmp(rel + rellen - 3, ".ut") != 0)
		return 0;
	rellen -= 3;
	if (rellen == 0)
		return 0;

	/* Map "/" -> "." ; reject a "." inside any component (would corrupt the
	 * dotted form), and reject empty components / control bytes. */
	for (i = 0; i < rellen; i++) {
		unsigned char c = (unsigned char)rel[i];
		char outc;
		if (c == '/') {
			/* empty component (leading, trailing, or "//") is invalid */
			if (pos == 0 || out[pos - 1] == '.')
				return 0;
			outc = '.';
		} else if (c == '.') {
			/* a literal dot in a path component cannot round-trip */
			out[0] = '\0';
			return 0;
		} else if (c < 0x20 || c == 0x7f) {
			out[0] = '\0';
			return 0;
		} else {
			outc = (char)c;
		}
		if (pos + 1 >= outsz) {
			out[0] = '\0';
			return 0;
		}
		out[pos++] = outc;
	}
	/* trailing component must be non-empty (no trailing slash before .ut) */
	if (pos == 0 || out[pos - 1] == '.') {
		out[0] = '\0';
		return 0;
	}
	out[pos] = '\0';
	return 1;
}

/* ------------------------------------------------------------------------- */
/* Export primitive                                                           */
/* ------------------------------------------------------------------------- */

/*
 * mkdir_p - create a directory and all missing ancestors.
 * path is modified in place (and restored), so it must be writable.
 * Returns 0 on success, -1 on error (errno set).
 */
static int mkdir_p(char *path) {
	char *p = path;
	int rc = 0;

	/* Skip leading '/' for absolute paths */
	if (*p == '/')
		p++;

	for (; *p != '\0'; p++) {
		if (*p == '/') {
			*p = '\0';
			rc = mkdir(path, 0755);
			if (rc != 0 && errno != EEXIST) {
				*p = '/';
				return -1;
			}
			*p = '/';
		}
	}
	/* Create the final component */
	rc = mkdir(path, 0755);
	if (rc != 0 && errno != EEXIST)
		return -1;
	return 0;
}

/*
 * Mac epoch (seconds since 1904-01-01) minus Unix epoch (1970-01-01).
 * Matches timedate.c: "const int64_t frontier_epoch_offset = 2082844800LL"
 */
#define UT_MAC_TO_UNIX_EPOCH_OFFSET ((int64_t)2082844800LL)

/*
 * Upper bound on a .ut file we are willing to read into memory on import.
 * UserTalk scripts are at most a few KB; anything past this is either corrupt
 * or a resource-exhaustion lever planted in the sync dir, so we refuse it
 * rather than malloc()/read() an arbitrarily large blob on the GIL-held
 * materialize path. 8 MiB is comfortably above any real script.
 */
#define UT_MAX_FILE_BYTES ((size_t)(8u * 1024u * 1024u))

/* ------------------------------------------------------------------------- */
/* Import primitive                                                           */
/* ------------------------------------------------------------------------- */

int ut_import_check(const char *dotted_path, const char *sync_base,
                    int64_t odb_mac_mtime,
                    unsigned char **out, size_t *outlen,
                    int64_t *ut_mac_mtime) {
	char fs_path[4096];
	struct stat st;
	int64_t dot_mac_mtime;
	int fd = -1;
	unsigned char *raw = NULL;
	size_t raw_len;
	unsigned char *decan = NULL;
	size_t decan_len = 0;

	/* Safe initialisation of all outputs. */
	if (out) *out = NULL;
	if (outlen) *outlen = 0;
	if (ut_mac_mtime) *ut_mac_mtime = 0;

	if (dotted_path == NULL || sync_base == NULL || out == NULL ||
	    outlen == NULL || ut_mac_mtime == NULL)
		return 0;

	/* Step 1: Map dotted path to filesystem path.
	 * ut_odb_path_to_fs rejects ".." and other unsafe segments. */
	if (!ut_odb_path_to_fs(dotted_path, sync_base, fs_path, sizeof(fs_path)))
		return 0;

	/*
	 * Step 2: Open the file once, with O_NOFOLLOW on the final component, and
	 * do ALL subsequent checks (mtime, size, read) against that one fd via
	 * fstat. This closes two holes a writer of the sync dir could otherwise
	 * exploit, since imported .ut content becomes executable UserTalk:
	 *   - symlink redirect: O_NOFOLLOW refuses to open a symlinked .ut, so a
	 *     planted symlink can't pull in a file outside the sync dir.
	 *   - TOCTOU: stat-then-open lets the file be swapped between the mtime
	 *     check and the read. fstat on the opened fd checks the exact bytes
	 *     we are about to read.
	 * O_NONBLOCK guards against the final component being a FIFO/device that
	 * would block open() indefinitely; we clear it implicitly by only ever
	 * reading a regular file (checked below).
	 */
	fd = open(fs_path, O_RDONLY | O_NOFOLLOW | O_NONBLOCK);
	if (fd < 0)
		return 0;  /* ENOENT, ELOOP (symlink), or unreadable: no import */

	if (fstat(fd, &st) != 0) {
		close(fd);
		return 0;
	}

	/* Only import from a regular file; reject FIFOs, devices, directories. */
	if (!S_ISREG(st.st_mode)) {
		close(fd);
		return 0;
	}

	/* Step 3: Convert .ut st_mtime to Mac epoch and compare.
	 * st_mtime + UT_MAC_TO_UNIX_EPOCH_OFFSET yields Mac epoch seconds.
	 * Import only when the .ut is STRICTLY newer than the ODB value. */
	dot_mac_mtime = (int64_t)st.st_mtime + UT_MAC_TO_UNIX_EPOCH_OFFSET;
	if (dot_mac_mtime <= odb_mac_mtime) {
		close(fd);
		return 0;
	}

	/* Step 4: Read the .ut file into a buffer, using the size from fstat. */
	if (st.st_size < 0) {
		close(fd);
		return 0;
	}
	raw_len = (size_t)st.st_size;
	if (raw_len > UT_MAX_FILE_BYTES) {
		/* Oversized: refuse rather than allocate/read an arbitrary blob. */
		close(fd);
		return 0;
	}
	if (raw_len == 0) {
		/* Empty .ut: decanonicalize of empty input is empty. */
		close(fd);
		decan = (unsigned char *)malloc(1);
		if (decan == NULL)
			return 0;
		decan[0] = '\0';
		*out          = decan;
		*outlen       = 0;
		*ut_mac_mtime = dot_mac_mtime;
		return 1;
	}
	raw = (unsigned char *)malloc(raw_len);
	if (raw == NULL) {
		close(fd);
		return 0;
	}
	{
		/* read() may return short; loop until raw_len bytes or error/EOF. */
		size_t got = 0;
		while (got < raw_len) {
			ssize_t n = read(fd, raw + got, raw_len - got);
			if (n < 0) {
				if (errno == EINTR)
					continue;
				close(fd);
				free(raw);
				return 0;
			}
			if (n == 0)
				break;  /* unexpected EOF: file shrank since fstat */
			got += (size_t)n;
		}
		if (got != raw_len) {
			close(fd);
			free(raw);
			return 0;
		}
	}
	close(fd);

	/* Step 5: Decanonicalize (UTF-8/LF/slash-slash -> MacRoman/CR/0xC7).
	 * Fails if the .ut contains non-MacRoman characters (e.g. emoji). */
	if (!ut_decanonicalize_outline_text(raw, raw_len, &decan, &decan_len)) {
		free(raw);
		return 0;
	}
	free(raw);

	*out          = decan;
	*outlen       = decan_len;
	*ut_mac_mtime = dot_mac_mtime;
	return 1;
}

/* ------------------------------------------------------------------------- */
/* Export primitive                                                           */
/* ------------------------------------------------------------------------- */

int ut_export_script(const unsigned char *raw, size_t rawlen,
                     const char *dotted_path, const char *sync_dir,
                     int64_t mac_mtime) {
	unsigned char *canon = NULL;
	size_t canon_len = 0;
	char fs_path[4096];
	char tmp_path[4096 + 8]; /* ".tmp" suffix room */
	char parent[4096];
	char *last_slash;
	int fd = -1;
	ssize_t written;
	int ok = 0;

	if (raw == NULL || dotted_path == NULL || sync_dir == NULL)
		return 0;

	/* Step 1: Canonicalize */
	if (!ut_canonicalize_outline_text(raw, rawlen, &canon, &canon_len))
		return 0;

	/* Step 2: Map path */
	if (!ut_odb_path_to_fs(dotted_path, sync_dir, fs_path, sizeof(fs_path)))
		goto done;

	/* Step 3: Create parent directories (mkdir -p). */
	if (strlen(fs_path) >= sizeof(parent)) {
		goto done;
	}
	strcpy(parent, fs_path);
	last_slash = strrchr(parent, '/');
	if (last_slash != NULL) {
		*last_slash = '\0';
		if (mkdir_p(parent) != 0)
			goto done;
	}

	/* Step 4: Write to a temp file in the same directory, then rename.
	 *
	 * Temp name: <fs_path>.tmp<pid>.<rand> -- the random suffix plus O_EXCL
	 * means a pre-planted file or symlink at the temp path can't be reused or
	 * followed (O_NOFOLLOW), and O_EXCL fails rather than truncating an
	 * attacker-controlled target. We retry a few times in the (vanishingly
	 * unlikely) event of a name collision. */
	{
		int opened = 0;
		for (int attempt = 0; attempt < 8 && !opened; attempt++) {
			unsigned int r = (unsigned int)(getpid() ^ (attempt * 2654435761u));
			r ^= (unsigned int)time(NULL);
			r = r * 1103515245u + 12345u;
			int n = snprintf(tmp_path, sizeof(tmp_path), "%s.tmp%d.%08x",
			                 fs_path, (int)getpid(), r & 0xffffffffu);
			if (n < 0 || n >= (int)sizeof(tmp_path))
				goto done;
			fd = open(tmp_path,
			          O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0644);
			if (fd >= 0) {
				opened = 1;
			} else if (errno != EEXIST) {
				goto done;
			}
		}
		if (!opened)
			goto done;
	}

	if (canon_len > 0) {
		written = write(fd, canon, canon_len);
		if (written < 0 || (size_t)written != canon_len) {
			close(fd);
			fd = -1;
			unlink(tmp_path);
			goto done;
		}
	}

	if (close(fd) != 0) {
		fd = -1;
		unlink(tmp_path);
		goto done;
	}
	fd = -1;

	/* Atomic rename into place */
	if (rename(tmp_path, fs_path) != 0) {
		unlink(tmp_path);
		goto done;
	}

	/* Step 5: Set mtime from mac_mtime.
	 *
	 * Skip stamping if mac_mtime is at or before the Unix epoch (i.e. the
	 * Mac timestamp predates 1970). In practice every real script has a
	 * timestamp well past 1904 so this guard is belt-and-suspenders. */
	if (mac_mtime > UT_MAC_TO_UNIX_EPOCH_OFFSET) {
		time_t unix_secs = (time_t)(mac_mtime - UT_MAC_TO_UNIX_EPOCH_OFFSET);
		struct timeval tv[2];
		tv[0].tv_sec  = unix_secs; /* atime */
		tv[0].tv_usec = 0;
		tv[1].tv_sec  = unix_secs; /* mtime */
		tv[1].tv_usec = 0;
		/* utimes() is POSIX; ignore failure (best-effort mtime stamp) */
		(void)utimes(fs_path, tv);
	}

	ok = 1;

done:
	free(canon);
	if (fd >= 0)
		close(fd);
	return ok;
}
