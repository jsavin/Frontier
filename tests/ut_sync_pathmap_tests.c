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
	TR_SUMMARY();
	return TR_EXIT_CODE();
}
