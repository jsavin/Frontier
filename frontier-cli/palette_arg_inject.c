/*
 * palette_arg_inject.c - implementation of the arg-injection helpers used
 *                        by the slash-menu palette dispatch path.
 *
 * Pure C — no ODB / UserTalk runtime / handle dependencies. The unit tests
 * exercise these functions in isolation; the dispatch path in repl.c uses
 * them to build a synthesized script source per call, with the typed arg
 * bound in as a string literal at compile time.
 */

#include "palette_arg_inject.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

bool palette_arg_escape(const char *arg, char *out, size_t out_cap) {
	if (out == NULL || out_cap == 0)
		return false;
	out[0] = '\0';
	if (arg == NULL) {
		/* NULL arg is treated as empty — escape produces an empty string,
		 * which the caller may detect and skip injection for. */
		return true;
	}
	size_t outpos = 0;
	for (const char *p = arg; *p != '\0'; ++p) {
		unsigned char c = (unsigned char)*p;
		/* Reject control bytes and DEL. Newline / CR / tab fall in here
		 * deliberately — see header comment. */
		if (c < 0x20 || c == 0x7F) {
			out[0] = '\0';
			return false;
		}
		/* Each '"' or '\\' produces two output bytes plus we need 1 for
		 * trailing NUL. Bound check before each write. */
		if (c == '"' || c == '\\') {
			if (outpos + 2 >= out_cap) {
				out[0] = '\0';
				return false;
			}
			out[outpos++] = '\\';
			out[outpos++] = (char)c;
		} else {
			if (outpos + 1 >= out_cap) {
				out[0] = '\0';
				return false;
			}
			out[outpos++] = (char)c;
		}
	}
	out[outpos] = '\0';
	return true;
}

/*
 * Skip a UserTalk string literal starting at script[i] where script[i] is
 * the opening '"'. Returns the index of the byte AFTER the closing '"',
 * or `len` if the string is unterminated (defensive: stop scanning).
 *
 * Honors '\\' escape so a '\\"' inside the literal does not end it.
 */
static size_t skip_string_literal(const char *script, size_t len, size_t i) {
	/* Caller passes i pointing at the opening '"'. Step past it. */
	if (i >= len) return len;
	++i;
	while (i < len) {
		char c = script[i];
		if (c == '\\') {
			/* Skip the escape AND the next byte (whatever it is). */
			i += 2;
			continue;
		}
		if (c == '"') {
			return i + 1;       /* one past the closing quote */
		}
		++i;
	}
	return len;
}

/*
 * Skip a UserTalk «…» comment block. script[i] points at the '«' start
 * marker (UTF-8 0xC2 0xAB — two bytes). Returns the index after '»'
 * (UTF-8 0xC2 0xBB), or `len` if unterminated.
 *
 * Note: the Pascal-source dialect of UserTalk ALSO treats { ... } as a
 * paired comment in some legacy code (per scripts.c historical comment),
 * but the standard parser today recognizes only «»; we don't try to be
 * cleverer than the parser here.
 */
static size_t skip_angle_comment(const char *script, size_t len, size_t i) {
	/* Caller has already verified script[i..i+1] is the «  start. */
	i += 2;     /* skip the two-byte « */
	while (i + 1 < len) {
		if ((unsigned char)script[i] == 0xC2 && (unsigned char)script[i + 1] == 0xBB) {
			return i + 2;
		}
		++i;
	}
	return len;
}

/*
 * Skip a UserTalk line comment '//' to end-of-line (LF, CR, or both).
 * script[i] points at the first '/'. Returns index past the line ending.
 */
static size_t skip_line_comment(const char *script, size_t len, size_t i) {
	/* Skip the '//' itself plus the rest of the line. */
	i += 2;
	while (i < len && script[i] != '\n' && script[i] != '\r') {
		++i;
	}
	/* Step past the newline so the next paren-search resumes on the
	 * following line. Handle CRLF as a single line ending. */
	if (i < len && script[i] == '\r') ++i;
	if (i < len && script[i] == '\n') ++i;
	return i;
}

/*
 * Find the first '(' in `script` whose matching ')' is paren-balanced.
 * Returns the index of '(' on success and writes the closing ')' index
 * to *out_close_idx. Returns SIZE_MAX on failure (no '(' or unbalanced).
 *
 * String literals, «...» blocks, and '//' line comments are skipped so
 * a paren inside one of them does NOT count as the call's open paren.
 */
static size_t find_first_call_parens(const char *script, size_t len,
                                     size_t *out_close_idx) {
	size_t i = 0;
	while (i < len) {
		char c = script[i];
		if (c == '"') {
			i = skip_string_literal(script, len, i);
			continue;
		}
		/* «...» comment: 0xC2 0xAB. Two-byte UTF-8 sequence. */
		if (i + 1 < len &&
		    (unsigned char)script[i] == 0xC2 &&
		    (unsigned char)script[i + 1] == 0xAB) {
			i = skip_angle_comment(script, len, i);
			continue;
		}
		/* '//' line comment. */
		if (c == '/' && i + 1 < len && script[i + 1] == '/') {
			i = skip_line_comment(script, len, i);
			continue;
		}
		if (c == '(') {
			/* Walk forward to the matching ')'. The paren-balance is
			 * needed only inside the (...) — but for the empty-args
			 * shape we expect, balance is trivial. We still skip
			 * literals/comments inside in case a future leaf script is
			 * shaped weirdly. Track depth so nested parens (someday)
			 * don't confuse us. */
			size_t open_idx = i;
			size_t j = i + 1;
			int depth = 1;
			while (j < len && depth > 0) {
				char d = script[j];
				if (d == '"') {
					j = skip_string_literal(script, len, j);
					continue;
				}
				if (j + 1 < len &&
				    (unsigned char)script[j] == 0xC2 &&
				    (unsigned char)script[j + 1] == 0xAB) {
					j = skip_angle_comment(script, len, j);
					continue;
				}
				if (d == '/' && j + 1 < len && script[j + 1] == '/') {
					j = skip_line_comment(script, len, j);
					continue;
				}
				if (d == '(') depth++;
				else if (d == ')') depth--;
				if (depth == 0) {
					*out_close_idx = j;
					return open_idx;
				}
				++j;
			}
			/* Unbalanced — fail. */
			return (size_t)-1;
		}
		++i;
	}
	return (size_t)-1;
}

/*
 * Test that script[open_idx + 1 .. close_idx - 1] contains only ASCII
 * whitespace (space, tab, CR, LF). UserTalk inside () may contain
 * comments too, but for our defensive "only inject into truly-empty ()"
 * rule we forbid comments here as well.
 */
static bool parens_are_empty(const char *script,
                             size_t open_idx, size_t close_idx) {
	for (size_t k = open_idx + 1; k < close_idx; ++k) {
		unsigned char c = (unsigned char)script[k];
		if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
			continue;
		return false;
	}
	return true;
}

bool palette_arg_inject_into_script(const char *script_in,
                                    size_t script_in_len,
                                    const char *escaped_arg,
                                    char **out_buf,
                                    size_t *out_len) {
	if (out_buf == NULL) return false;
	*out_buf = NULL;
	if (out_len) *out_len = 0;
	if (script_in == NULL || script_in_len == 0) return false;
	if (escaped_arg == NULL) return false;

	size_t close_idx = 0;
	size_t open_idx = find_first_call_parens(script_in, script_in_len, &close_idx);
	if (open_idx == (size_t)-1) return false;
	if (!parens_are_empty(script_in, open_idx, close_idx)) return false;

	/* New length: original - (close_idx - open_idx + 1) [the "()" we
	 * remove] + 4 [for ( " " )] + strlen(escaped_arg). The replaced
	 * range is open_idx..close_idx inclusive — we substitute (..) with
	 * ("escaped_arg"). */
	size_t arg_len = strlen(escaped_arg);
	size_t insert_len = arg_len + 4;        /* ( " arg " ) */
	size_t replaced_len = close_idx - open_idx + 1;
	size_t new_len = script_in_len - replaced_len + insert_len;

	char *buf = (char *)malloc(new_len + 1);
	if (buf == NULL) return false;

	/* Copy: prefix + new "(\"escaped\")" + suffix. */
	memcpy(buf, script_in, open_idx);
	size_t pos = open_idx;
	buf[pos++] = '(';
	buf[pos++] = '"';
	memcpy(buf + pos, escaped_arg, arg_len);
	pos += arg_len;
	buf[pos++] = '"';
	buf[pos++] = ')';
	memcpy(buf + pos, script_in + close_idx + 1,
	       script_in_len - (close_idx + 1));
	pos += script_in_len - (close_idx + 1);
	buf[pos] = '\0';

	*out_buf = buf;
	if (out_len) *out_len = pos;
	return true;
}
