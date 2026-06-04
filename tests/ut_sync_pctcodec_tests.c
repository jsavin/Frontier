/*
 * ut_sync_pctcodec_tests.c - Unit tests for the percent-codec that escapes
 * ODB path segments for safe use as filesystem path components
 * (frontier-cli/ut_sync.c ut_pct_encode_segment / ut_pct_decode_segment).
 *
 * The codec is a pure, lossless byte transform. A raw ODB segment (which may
 * contain '.', '/', ':', '"', '\\', '%', control bytes, or a leading '-') is
 * percent-encoded into an ASCII string safe to use as one filesystem path
 * component, and decoded back to the exact original bytes. These tests cover:
 *   - identity pass-through for bare identifiers and "#filters"-style names
 *   - each collision char escaped individually
 *   - escape-the-escape-first invariant ('%' handled before '.', '/', etc.)
 *   - control bytes and DEL escaped as uppercase %XX
 *   - leading '-' encoded, interior/trailing '-' passed through
 *   - the RSS-key acceptance string (http://webns.net/mvcb/)
 *   - decode rejects malformed % escapes
 *   - decode accepts lowercase hex (lenient decode)
 *   - encode/decode overflow returns failure and clears the output
 *   - round-trip fuzz set proving the lossless guarantee
 *
 * License
 * -------
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Frontier contributors.
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "test_report.h"

#include "../frontier-cli/ut_sync.h"

/* ---- identity pass-through ---- */

static void test_encode_identity_bare_identifier(void) {
	char buf[64];
	assert(ut_pct_encode_segment("upper", 5, buf, sizeof(buf)) == 1);
	assert(strcmp(buf, "upper") == 0);
	assert(ut_pct_encode_segment("string", 6, buf, sizeof(buf)) == 1);
	assert(strcmp(buf, "string") == 0);
}

static void test_encode_identity_hashfilters(void) {
	char buf[64];
	/* '#' and the rest of an identifier pass through unchanged. */
	assert(ut_pct_encode_segment("#filters", 8, buf, sizeof(buf)) == 1);
	assert(strcmp(buf, "#filters") == 0);
	/* '_' passes through unchanged. */
	assert(ut_pct_encode_segment("my_table", 8, buf, sizeof(buf)) == 1);
	assert(strcmp(buf, "my_table") == 0);
}

/* ---- each collision char escaped individually ---- */

static void test_encode_each_collision_char_individually(void) {
	char buf[64];
	assert(ut_pct_encode_segment("a.b", 3, buf, sizeof(buf)) == 1);
	assert(strcmp(buf, "a%2Eb") == 0);
	assert(ut_pct_encode_segment("a/b", 3, buf, sizeof(buf)) == 1);
	assert(strcmp(buf, "a%2Fb") == 0);
	assert(ut_pct_encode_segment("a:b", 3, buf, sizeof(buf)) == 1);
	assert(strcmp(buf, "a%3Ab") == 0);
	assert(ut_pct_encode_segment("a\"b", 3, buf, sizeof(buf)) == 1);
	assert(strcmp(buf, "a%22b") == 0);
	assert(ut_pct_encode_segment("a\\b", 3, buf, sizeof(buf)) == 1);
	assert(strcmp(buf, "a%5Cb") == 0);
	assert(ut_pct_encode_segment("a%b", 3, buf, sizeof(buf)) == 1);
	assert(strcmp(buf, "a%25b") == 0);
}

/* ---- escape-the-escape-first invariant ---- */

static void test_encode_escape_first_invariant(void) {
	char buf[64];
	/* A bare '%' must become "%25". */
	assert(ut_pct_encode_segment("%", 1, buf, sizeof(buf)) == 1);
	assert(strcmp(buf, "%25") == 0);
	/* "%25" (3 chars) must encode to "%2525": the leading '%' becomes "%25",
	 * then the literal "25" passes through. If '%' were not escaped first, this
	 * would be mis-decodable. */
	assert(ut_pct_encode_segment("%25", 3, buf, sizeof(buf)) == 1);
	assert(strcmp(buf, "%2525") == 0);
	/* Decode round-trips: "%2525" -> "%25" (3 chars). */
	assert(ut_pct_decode_segment("%2525", buf, sizeof(buf)) == 1);
	assert(strcmp(buf, "%25") == 0);
	/* And "%25" -> "%" (1 char). */
	assert(ut_pct_decode_segment("%25", buf, sizeof(buf)) == 1);
	assert(strcmp(buf, "%") == 0);
}

/* ---- control bytes and DEL ---- */

static void test_encode_control_and_del(void) {
	char buf[64];
	/* 0x01 -> "%01" */
	{
		char raw[3];
		raw[0] = 'a';
		raw[1] = (char)0x01;
		raw[2] = 'b';
		assert(ut_pct_encode_segment(raw, 3, buf, sizeof(buf)) == 1);
		assert(strcmp(buf, "a%01b") == 0);
	}
	/* 0x1f -> "%1F" (uppercase hex) */
	{
		char raw[3];
		raw[0] = 'a';
		raw[1] = (char)0x1f;
		raw[2] = 'b';
		assert(ut_pct_encode_segment(raw, 3, buf, sizeof(buf)) == 1);
		assert(strcmp(buf, "a%1Fb") == 0);
	}
	/* 0x7f (DEL) -> "%7F" */
	{
		char raw[3];
		raw[0] = 'a';
		raw[1] = (char)0x7f;
		raw[2] = 'b';
		assert(ut_pct_encode_segment(raw, 3, buf, sizeof(buf)) == 1);
		assert(strcmp(buf, "a%7Fb") == 0);
	}
}

/* ---- leading dash only ---- */

static void test_encode_leading_dash_only(void) {
	char buf[64];
	/* A leading '-' is encoded so the path component cannot be mistaken for an
	 * option flag by downstream tools. */
	assert(ut_pct_encode_segment("-foo", 4, buf, sizeof(buf)) == 1);
	assert(strcmp(buf, "%2Dfoo") == 0);
	/* Interior '-' passes through unchanged. */
	assert(ut_pct_encode_segment("foo-bar", 7, buf, sizeof(buf)) == 1);
	assert(strcmp(buf, "foo-bar") == 0);
	/* Trailing '-' passes through unchanged. */
	assert(ut_pct_encode_segment("foo-", 4, buf, sizeof(buf)) == 1);
	assert(strcmp(buf, "foo-") == 0);
}

/* ---- the RSS-key acceptance string ---- */

static void test_combined_rss_key(void) {
	/* THE acceptance string: a real RSS namespace key used as an ODB segment.
	 * raw bytes: h t t p : / / w e b n s . n e t / m v c b / */
	const char *raw = "http://webns.net/mvcb/";
	size_t rawlen = strlen(raw); /* 22 */
	char enc[128];
	char dec[128];
	assert(ut_pct_encode_segment(raw, rawlen, enc, sizeof(enc)) == 1);
	assert(strcmp(enc, "http%3A%2F%2Fwebns%2Enet%2Fmvcb%2F") == 0);
	/* Decode back to the original raw bytes. */
	assert(ut_pct_decode_segment(enc, dec, sizeof(dec)) == 1);
	assert(strlen(dec) == rawlen);
	assert(memcmp(dec, raw, rawlen) == 0);
}

/* ---- decode rejects malformed percent escapes ---- */

static void test_decode_rejects_malformed_percent(void) {
	char buf[64];
	/* lone trailing '%' */
	assert(ut_pct_decode_segment("%", buf, sizeof(buf)) == 0);
	assert(buf[0] == '\0');
	/* '%' followed by only one hex digit */
	assert(ut_pct_decode_segment("%2", buf, sizeof(buf)) == 0);
	assert(buf[0] == '\0');
	/* '%' followed by two non-hex chars */
	assert(ut_pct_decode_segment("%ZZ", buf, sizeof(buf)) == 0);
	assert(buf[0] == '\0');
	/* '%' with a bad second nibble */
	assert(ut_pct_decode_segment("%2Z", buf, sizeof(buf)) == 0);
	assert(buf[0] == '\0');
	/* lone trailing '%' after valid content */
	assert(ut_pct_decode_segment("abc%", buf, sizeof(buf)) == 0);
	assert(buf[0] == '\0');
}

/* ---- decode accepts lowercase hex ---- */

static void test_decode_accepts_lowercase_hex_input(void) {
	char buf[64];
	/* Encode emits UPPERCASE hex, but decode is lenient and accepts either
	 * case so hand-edited paths still round-trip. "%2e" -> ".". */
	assert(ut_pct_decode_segment("%2e", buf, sizeof(buf)) == 1);
	assert(strcmp(buf, ".") == 0);
}

/* ---- overflow returns failure and clears output ---- */

static void test_encode_overflow_returns_zero(void) {
	/* "a.b.c" encodes to "a%2Eb%2Ec" (9 bytes + NUL = 10). A 4-byte buffer
	 * cannot hold it: return 0, out[0]='\0'. */
	char buf[4];
	assert(ut_pct_encode_segment("a.b.c", 5, buf, sizeof(buf)) == 0);
	assert(buf[0] == '\0');
}

static void test_decode_overflow_returns_zero(void) {
	/* "%2E%2E%2E%2E" decodes to "...." (4 bytes + NUL = 5). A 2-byte buffer
	 * cannot hold it: return 0, out[0]='\0'. */
	char buf[2];
	assert(ut_pct_decode_segment("%2E%2E%2E%2E", buf, sizeof(buf)) == 0);
	assert(buf[0] == '\0');
}

/* ---- round-trip fuzz set: the core lossless guarantee ---- */

static void test_round_trip_fuzz_set(void) {
	/* Each entry is a raw byte buffer plus its length (some contain bytes that
	 * cannot be written as a plain NUL-terminated C string, e.g. a control
	 * byte, so we carry an explicit length and compare with memcmp). */
	struct {
		const char *raw;
		size_t rawlen;
	} cases[9];
	static const char ctrl_raw[] = { 'w', 'i', 't', 'h', (char)0x05, 'c', 't', 'l' };
	int i;

	cases[0].raw = "system";                cases[0].rawlen = 6;
	cases[1].raw = "verbs";                 cases[1].rawlen = 5;
	cases[2].raw = "http://webns.net/mvcb/"; cases[2].rawlen = 22;
	cases[3].raw = "a.b/c:d\"e%f";          cases[3].rawlen = 11;
	cases[4].raw = "-leading";              cases[4].rawlen = 8;
	cases[5].raw = "trailing-";             cases[5].rawlen = 9;
	cases[6].raw = "with space";            cases[6].rawlen = 10;
	cases[7].raw = ctrl_raw;                cases[7].rawlen = sizeof(ctrl_raw);
	cases[8].raw = "";                      cases[8].rawlen = 0;

	for (i = 0; i < 9; i++) {
		char enc[256];
		char dec[256];
		assert(ut_pct_encode_segment(cases[i].raw, cases[i].rawlen,
		                             enc, sizeof(enc)) == 1);
		assert(ut_pct_decode_segment(enc, dec, sizeof(dec)) == 1);
		/* Decoded raw here has no embedded NUL, so strlen gives its length. */
		assert(strlen(dec) == cases[i].rawlen);
		assert(memcmp(dec, cases[i].raw, cases[i].rawlen) == 0);
	}
}

int main(void) {
	TR_INIT("ut_sync_pctcodec_tests");
	TR_RUN(test_encode_identity_bare_identifier);
	TR_RUN(test_encode_identity_hashfilters);
	TR_RUN(test_encode_each_collision_char_individually);
	TR_RUN(test_encode_escape_first_invariant);
	TR_RUN(test_encode_control_and_del);
	TR_RUN(test_encode_leading_dash_only);
	TR_RUN(test_combined_rss_key);
	TR_RUN(test_decode_rejects_malformed_percent);
	TR_RUN(test_decode_accepts_lowercase_hex_input);
	TR_RUN(test_encode_overflow_returns_zero);
	TR_RUN(test_decode_overflow_returns_zero);
	TR_RUN(test_round_trip_fuzz_set);
	TR_SUMMARY();
	return TR_EXIT_CODE();
}
