/*
 * ut_sync_pathmap_tests.c - Unit tests for the ODB<->.ut path mapping
 * (frontier-cli/ut_sync.c ut_odb_path_to_fs / ut_fs_path_to_odb).
 *
 * The mapping is purely lexical: dotted ODB path "a.b.c" <-> filesystem path
 * "<sync_dir>/a/b/c.ut". These tests cover:
 *   - basic forward and reverse mapping
 *   - round-trip stability
 *   - non-identifier segments ("#filters") map through unescaped
 *   - sync_dir with and without a trailing slash
 *   - SECURITY: ".." / "." / empty / slash-injection / control bytes rejected
 *   - buffer-too-small returns failure and clears the output
 *   - reverse mapping rejects paths outside sync_dir, missing .ut, and a
 *     literal "." inside a path component
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

#define SYNC "usertalk_scripts/Frontier.root"

static void test_forward_basic(void) {
	char buf[256];
	assert(ut_odb_path_to_fs("system.verbs.builtins.op", SYNC,
	                         buf, sizeof(buf)) == 1);
	assert(strcmp(buf,
	    "usertalk_scripts/Frontier.root/system/verbs/builtins/op.ut") == 0);
}

static void test_forward_single_segment(void) {
	char buf[256];
	assert(ut_odb_path_to_fs("loner", SYNC, buf, sizeof(buf)) == 1);
	assert(strcmp(buf, "usertalk_scripts/Frontier.root/loner.ut") == 0);
}

static void test_forward_nonident_segment(void) {
	/* "#filters" is stored as a literal directory name, no escaping. */
	char buf[256];
	assert(ut_odb_path_to_fs("suites.people.webAdmin.#filters.firstFilter",
	                         SYNC, buf, sizeof(buf)) == 1);
	assert(strcmp(buf,
	    "usertalk_scripts/Frontier.root/suites/people/webAdmin/"
	    "#filters/firstFilter.ut") == 0);
}

static void test_forward_trailing_slash_syncdir(void) {
	char buf[256];
	assert(ut_odb_path_to_fs("a.b", "root/", buf, sizeof(buf)) == 1);
	assert(strcmp(buf, "root/a/b.ut") == 0);
}

static void test_reverse_basic(void) {
	char buf[256];
	assert(ut_fs_path_to_odb(
	    "usertalk_scripts/Frontier.root/system/verbs/builtins/op.ut",
	    SYNC, buf, sizeof(buf)) == 1);
	assert(strcmp(buf, "system.verbs.builtins.op") == 0);
}

static void test_reverse_nonident_segment(void) {
	char buf[256];
	assert(ut_fs_path_to_odb(
	    "usertalk_scripts/Frontier.root/suites/people/webAdmin/"
	    "#filters/firstFilter.ut", SYNC, buf, sizeof(buf)) == 1);
	assert(strcmp(buf,
	    "suites.people.webAdmin.#filters.firstFilter") == 0);
}

static void test_round_trip(void) {
	const char *paths[] = {
	    "system.verbs.builtins.op",
	    "suites.people.webAdmin.#filters.firstFilter",
	    "loner",
	    NULL
	};
	int i;
	for (i = 0; paths[i] != NULL; i++) {
		char fs[256];
		char back[256];
		assert(ut_odb_path_to_fs(paths[i], SYNC, fs, sizeof(fs)) == 1);
		assert(ut_fs_path_to_odb(fs, SYNC, back, sizeof(back)) == 1);
		assert(strcmp(paths[i], back) == 0);
	}
}

/* ---- security: forward mapping rejects unsafe segments ---- */

static void test_forward_rejects_dotdot(void) {
	char buf[256];
	assert(ut_odb_path_to_fs("system...op", SYNC, buf, sizeof(buf)) == 0);
	assert(buf[0] == '\0');
	/* explicit ".." segment */
	assert(ut_odb_path_to_fs("system.\x2e\x2e.op", SYNC,
	                         buf, sizeof(buf)) == 0);
	assert(buf[0] == '\0');
}

static void test_forward_rejects_leading_dot(void) {
	char buf[256];
	assert(ut_odb_path_to_fs(".system", SYNC, buf, sizeof(buf)) == 0);
	assert(buf[0] == '\0');
}

static void test_forward_rejects_trailing_dot(void) {
	char buf[256];
	assert(ut_odb_path_to_fs("system.", SYNC, buf, sizeof(buf)) == 0);
	assert(buf[0] == '\0');
}

static void test_forward_rejects_slash_injection(void) {
	char buf[256];
	/* a segment containing "/" would create an unintended directory level */
	assert(ut_odb_path_to_fs("a.b/../../etc/passwd.c", SYNC,
	                         buf, sizeof(buf)) == 0);
	assert(buf[0] == '\0');
}

static void test_forward_rejects_empty(void) {
	char buf[256];
	assert(ut_odb_path_to_fs("", SYNC, buf, sizeof(buf)) == 0);
}

static void test_forward_buffer_too_small(void) {
	char buf[8];
	assert(ut_odb_path_to_fs("system.verbs.builtins.op", SYNC,
	                         buf, sizeof(buf)) == 0);
	assert(buf[0] == '\0');
}

/* ---- security: reverse mapping ---- */

static void test_reverse_rejects_outside_syncdir(void) {
	char buf[256];
	assert(ut_fs_path_to_odb("/etc/passwd.ut", SYNC,
	                         buf, sizeof(buf)) == 0);
	assert(buf[0] == '\0');
}

static void test_reverse_rejects_missing_ut(void) {
	char buf[256];
	assert(ut_fs_path_to_odb(
	    "usertalk_scripts/Frontier.root/system/op.txt", SYNC,
	    buf, sizeof(buf)) == 0);
}

static void test_reverse_rejects_dot_in_component(void) {
	/* A literal "." inside a directory name cannot round-trip to a dotted
	 * path (it would be read back as a segment boundary). Reject it. */
	char buf[256];
	assert(ut_fs_path_to_odb(
	    "usertalk_scripts/Frontier.root/sys.tem/op.ut", SYNC,
	    buf, sizeof(buf)) == 0);
	assert(buf[0] == '\0');
}

static void test_reverse_rejects_prefix_lookalike(void) {
	/* "Frontier.rootX" must not be accepted as being under "Frontier.root". */
	char buf[256];
	assert(ut_fs_path_to_odb(
	    "usertalk_scripts/Frontier.rootX/system/op.ut", SYNC,
	    buf, sizeof(buf)) == 0);
}

/*
 * test_round_trip_pct_encoded_rss_key
 *
 * End-to-end Phase 1 round-trip for a URL-keyed ODB segment. The interchange
 * dotted path carries percent-encoded segments; the path mapper treats each
 * encoded segment as an opaque filesystem-safe token. We build the encoded
 * interchange string manually (simulating what langhash_materialize produces
 * after encoding), pass it through the mapper both ways, and verify:
 *   1. The forward map produces the expected filesystem path.
 *   2. The reverse map reconstructs the encoded interchange string exactly.
 *   3. Splitting on '.' and decoding the 7th segment yields the raw URL.
 *
 * We also verify that a plain identifier path round-trips byte-identically
 * (encoding of a safe identifier is itself, so the common case is unaffected).
 */
static void test_round_trip_pct_encoded_rss_key(void) {
	/*
	 * Raw RSS key segments (8 total):
	 *   [0] system  [1] verbs  [2] builtins  [3] xml  [4] rss
	 *   [5] moduleDrivers  [6] http://webns.net/mvcb/  [7] init
	 *
	 * After encoding: segments 0-5 and 7 are plain identifiers (encode to
	 * themselves); segment 6 encodes as:
	 *   http%3A%2F%2Fwebns%2Enet%2Fmvcb%2F
	 *
	 * The encoded interchange path (joined with '.'):
	 */
	const char *enc_path =
	    "system.verbs.builtins.xml.rss.moduleDrivers"
	    ".http%3A%2F%2Fwebns%2Enet%2Fmvcb%2F.init";
	const char *expected_fs_suffix =
	    "/moduleDrivers/http%3A%2F%2Fwebns%2Enet%2Fmvcb%2F/init.ut";
	char fs[512];
	char back[512];
	int i;

	/* Forward: encoded dotted path -> filesystem path. */
	assert(ut_odb_path_to_fs(enc_path, SYNC, fs, sizeof(fs)) == 1);

	/* The fs path must end with the expected suffix. */
	{
		size_t fslen = strlen(fs);
		size_t suflen = strlen(expected_fs_suffix);
		assert(fslen > suflen);
		assert(strcmp(fs + fslen - suflen, expected_fs_suffix) == 0);
	}

	/* Reverse: filesystem path -> encoded dotted path (round-trip identity). */
	assert(ut_fs_path_to_odb(fs, SYNC, back, sizeof(back)) == 1);
	assert(strcmp(back, enc_path) == 0);

	/* Split `back` on '.' and decode each segment to recover the raw names.
	 * Segment count must be 8; the 7th (index 6) must decode to the raw URL. */
	{
		char split_buf[512];
		const char *raw_url = "http://webns.net/mvcb/";
		char decoded[256];
		char *p;
		int seg_count = 0;
		int seg6_ok = 0;

		memcpy(split_buf, back, strlen(back) + 1);
		p = split_buf;
		while (p != NULL) {
			char *dot = strchr(p, '.');
			if (dot != NULL)
				*dot = '\0';
			if (seg_count == 6) {
				/* Decode the 7th segment and check it equals the raw URL. */
				assert(ut_pct_decode_segment(p, decoded, sizeof(decoded)) == 1);
				assert(strcmp(decoded, raw_url) == 0);
				seg6_ok = 1;
			}
			seg_count++;
			p = (dot != NULL) ? dot + 1 : NULL;
		}
		assert(seg_count == 8);
		assert(seg6_ok);
	}

	/* Regression: a plain-identifier path is byte-identical after round-trip
	 * (encoding of a safe identifier is itself, so no spurious escaping). */
	for (i = 0; i < 3; i++) {
		const char *plain_paths[] = {
		    "system.verbs.builtins.string.upper",
		    "system.verbs.builtins.op",
		    "loner",
		};
		char plain_fs[256];
		char plain_back[256];
		assert(ut_odb_path_to_fs(plain_paths[i], SYNC,
		                         plain_fs, sizeof(plain_fs)) == 1);
		assert(ut_fs_path_to_odb(plain_fs, SYNC,
		                         plain_back, sizeof(plain_back)) == 1);
		assert(strcmp(plain_paths[i], plain_back) == 0);
	}
}

int main(void) {
	TR_INIT("ut_sync_pathmap_tests");
	TR_RUN(test_forward_basic);
	TR_RUN(test_forward_single_segment);
	TR_RUN(test_forward_nonident_segment);
	TR_RUN(test_forward_trailing_slash_syncdir);
	TR_RUN(test_reverse_basic);
	TR_RUN(test_reverse_nonident_segment);
	TR_RUN(test_round_trip);
	TR_RUN(test_forward_rejects_dotdot);
	TR_RUN(test_forward_rejects_leading_dot);
	TR_RUN(test_forward_rejects_trailing_dot);
	TR_RUN(test_forward_rejects_slash_injection);
	TR_RUN(test_forward_rejects_empty);
	TR_RUN(test_forward_buffer_too_small);
	TR_RUN(test_reverse_rejects_outside_syncdir);
	TR_RUN(test_reverse_rejects_missing_ut);
	TR_RUN(test_reverse_rejects_dot_in_component);
	TR_RUN(test_reverse_rejects_prefix_lookalike);
	TR_RUN(test_round_trip_pct_encoded_rss_key);
	TR_SUMMARY();
	return TR_EXIT_CODE();
}
