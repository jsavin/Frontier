/*
 * diff_roots_tests.c - Unit tests for the --diff-roots equality walker
 * (frontier-cli/diff_roots.c).
 *
 * The walker is the acceptance instrument for the root-build-from-source plan:
 * every later step is verified by "run this and require an empty diff". These
 * tests therefore pin the properties that make the instrument TRUSTWORTHY,
 * rather than merely exercising its code paths:
 *
 *   - path escaping is lossless and unambiguous, including for the ODB names
 *     that legitimately contain tab / CR / LF. Virgin.root really does have
 *     sibling values named with the single bytes 0x0A/0x0B/0x0C/0x0D; under
 *     newline normalization two of those COLLIDE, which would produce a false
 *     diff (or a false match). Escaping is what prevents that.
 *   - a '.' in a reported path is ALWAYS a separator, never part of a name.
 *   - finding-kind names are stable, because reports get diffed and grepped.
 *   - overflow is reported, never silently swallowed (the loud-fail rule).
 *   - operational failures (missing file, non-v7 root) are distinguishable
 *     from "walk completed and found differences".
 *
 * Fixture-based whole-root comparisons (identical -> empty, one known mutation
 * -> exactly one finding, cross-era -> the known delta set) are acceptance
 * evidence run against real .root files, not unit tests -- they need multi-MB
 * databases and are driven separately.
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

#include "../frontier-cli/diff_roots.h"

/* ---- finding-kind names are stable and total ---- */

static void test_kindname_covers_every_kind(void) {
	/* Reports are grepped and diffed, so these strings are a contract. */
	assert(strcmp(diff_roots_kindname(diffkind_only_in_a), "only-in-a") == 0);
	assert(strcmp(diff_roots_kindname(diffkind_only_in_b), "only-in-b") == 0);
	assert(strcmp(diff_roots_kindname(diffkind_type), "type") == 0);
	assert(strcmp(diff_roots_kindname(diffkind_payload), "payload") == 0);
	assert(strcmp(diff_roots_kindname(diffkind_unsupported), "unsupported-type") == 0);
	assert(strcmp(diff_roots_kindname(diffkind_unreadable), "unreadable") == 0);
}

static void test_kindname_out_of_range_is_not_a_crash(void) {
	/* Must not index off the end of the table. */
	assert(strcmp(diff_roots_kindname((tydiffkind) ctdiffkinds), "unknown") == 0);
	assert(strcmp(diff_roots_kindname((tydiffkind) -1), "unknown") == 0);
}

/* ---- path building: the ordinary cases ---- */

static void test_appendsegment_builds_dotted_path(void) {
	char path[256];

	path[0] = '\0';
	assert(diff_roots_appendsegment(path, sizeof(path), "system", 6));
	assert(strcmp(path, "system") == 0);

	assert(diff_roots_appendsegment(path, sizeof(path), "verbs", 5));
	assert(strcmp(path, "system.verbs") == 0);

	assert(diff_roots_appendsegment(path, sizeof(path), "builtins", 8));
	assert(strcmp(path, "system.verbs.builtins") == 0);
}

static void test_appendsegment_empty_name_is_representable(void) {
	char path[256];

	/* An empty name is a legal (if odd) ODB name; it must not vanish. */
	path[0] = '\0';
	assert(diff_roots_appendsegment(path, sizeof(path), "root", 4));
	assert(diff_roots_appendsegment(path, sizeof(path), "", 0));
	assert(strcmp(path, "root.") == 0);
}

/* ---- path building: names are bytes ---- */

static void test_appendsegment_escapes_tab_cr_lf(void) {
	char path[256];

	/* Real Virgin.root name shape: "0001<TAB>funnyStuff". */
	path[0] = '\0';
	assert(diff_roots_appendsegment(path, sizeof(path), "0001\tfunnyStuff", 15));
	assert(strcmp(path, "0001%09funnyStuff") == 0);

	path[0] = '\0';
	assert(diff_roots_appendsegment(path, sizeof(path), "\r", 1));
	assert(strcmp(path, "%0D") == 0);

	path[0] = '\0';
	assert(diff_roots_appendsegment(path, sizeof(path), "\n", 1));
	assert(strcmp(path, "%0A") == 0);
}

static void test_appendsegment_cr_and_lf_siblings_do_not_collide(void) {
	/*
	 * THE case this walker exists to get right. Virgin.root has two sibling
	 * values under ...aim.code.util.repl named with the single bytes 0x0D and
	 * 0x0A. Newline-normalizing names merges them (the census saw 11,257 rows
	 * but only 11,255 unique paths). A path-keyed comparison that merged them
	 * would report a phantom difference or silently drop a real one.
	 */
	char patha[256];
	char pathb[256];

	patha[0] = '\0';
	assert(diff_roots_appendsegment(patha, sizeof(patha), "repl", 4));
	assert(diff_roots_appendsegment(patha, sizeof(patha), "\r", 1));

	pathb[0] = '\0';
	assert(diff_roots_appendsegment(pathb, sizeof(pathb), "repl", 4));
	assert(diff_roots_appendsegment(pathb, sizeof(pathb), "\n", 1));

	assert(strcmp(patha, pathb) != 0);
	assert(strcmp(patha, "repl.%0D") == 0);
	assert(strcmp(pathb, "repl.%0A") == 0);
}

static void test_appendsegment_escapes_whole_c0_range(void) {
	/*
	 * fix_check.txt in the census archive shows a contiguous run of C0 control
	 * bytes (0x0B, 0x0C, 0x0D, 0x0A, 0x0E, 0x0F) used as sibling value names,
	 * so assume the whole 0x00-0x1F range is legal in an ODB name.
	 */
	int b;

	for (b = 1; b < 0x20; b++) {
		char path[64];
		char name[2];
		char expected[16];

		name[0] = (char) b;
		name[1] = '\0';

		path[0] = '\0';
		assert(diff_roots_appendsegment(path, sizeof(path), name, 1));

		snprintf(expected, sizeof(expected), "%%%02X", b);
		assert(strcmp(path, expected) == 0);
	}
}

static void test_appendsegment_dot_in_name_is_escaped(void) {
	/*
	 * Invariant: a '.' in a REPORTED path is always a separator. A literal '.'
	 * inside a name must therefore be escaped, or "a.b" (one name) and "a"."b"
	 * (two levels) would be indistinguishable in the report.
	 */
	char onename[256];
	char twolevels[256];

	onename[0] = '\0';
	assert(diff_roots_appendsegment(onename, sizeof(onename), "a.b", 3));
	assert(strcmp(onename, "a%2Eb") == 0);

	twolevels[0] = '\0';
	assert(diff_roots_appendsegment(twolevels, sizeof(twolevels), "a", 1));
	assert(diff_roots_appendsegment(twolevels, sizeof(twolevels), "b", 1));
	assert(strcmp(twolevels, "a.b") == 0);

	assert(strcmp(onename, twolevels) != 0);
}

static void test_appendsegment_preserves_high_bit_bytes(void) {
	/*
	 * MacRoman bytes pass through as raw bytes -- never decoded, never
	 * re-encoded as UTF-8 (#880). 0xC7 is the UserTalk comment marker.
	 */
	char path[256];
	char name[3];

	name[0] = (char) 0xC7;
	name[1] = (char) 0xE9;
	name[2] = '\0';

	path[0] = '\0';
	assert(diff_roots_appendsegment(path, sizeof(path), name, 2));
	assert((unsigned char) path[0] == 0xC7);
	assert((unsigned char) path[1] == 0xE9);
	assert(path[2] == '\0');
}

/* ---- path building: overflow is loud ---- */

static void test_appendsegment_overflow_reports_and_preserves(void) {
	char path[16];
	char longname[64];

	memset(longname, 'x', sizeof(longname));

	strcpy(path, "parent");

	/* Too long to fit: must fail, and must NOT corrupt the existing path. */
	assert(!diff_roots_appendsegment(path, sizeof(path), longname, sizeof(longname)));
	assert(strcmp(path, "parent") == 0);
}

/* ---- operational failures are distinguishable from differences ---- */

static void test_compare_missing_file_is_operational_failure(void) {
	tydiffoptions options;
	long ctfindings = -1;

	memset(&options, 0, sizeof(options));
	options.flquiet = true;

	/*
	 * A missing input is NOT "a diff with findings" -- it must return false so
	 * the caller can exit 2 rather than 1. Conflating the two would let a
	 * broken CI check masquerade as a real drift report.
	 */
	assert(!diff_roots_compare("/nonexistent/a.root", "/nonexistent/b.root",
	                           &options, &ctfindings));
}

int main(void) {
	TR_INIT("diff_roots_tests");
	TR_RUN(test_kindname_covers_every_kind);
	TR_RUN(test_kindname_out_of_range_is_not_a_crash);
	TR_RUN(test_appendsegment_builds_dotted_path);
	TR_RUN(test_appendsegment_empty_name_is_representable);
	TR_RUN(test_appendsegment_escapes_tab_cr_lf);
	TR_RUN(test_appendsegment_cr_and_lf_siblings_do_not_collide);
	TR_RUN(test_appendsegment_escapes_whole_c0_range);
	TR_RUN(test_appendsegment_dot_in_name_is_escaped);
	TR_RUN(test_appendsegment_preserves_high_bit_bytes);
	TR_RUN(test_appendsegment_overflow_reports_and_preserves);
	TR_RUN(test_compare_missing_file_is_operational_failure);
	TR_SUMMARY();
	return TR_EXIT_CODE();
}
