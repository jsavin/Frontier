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

#include <stdlib.h>
#include <string.h>

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

/* Append a single raw byte (already final) to a growable buffer. */
static int append_byte(unsigned char **buf, size_t *len, size_t *cap,
                       unsigned char b) {
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
			if (!append_byte(&buf, &len, &cap, b))
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
			if (!append_byte(&buf, &len, &cap, '/') ||
			    !append_byte(&buf, &len, &cap, '/'))
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
/* Path mapping                                                              */
/* ------------------------------------------------------------------------- */

/* Reject a single ODB-path segment that cannot safely become an fs component:
 * empty, ".", "..", or containing "/" or a control byte. Returns 1 if safe. */
static int segment_is_safe(const char *seg, size_t len) {
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
