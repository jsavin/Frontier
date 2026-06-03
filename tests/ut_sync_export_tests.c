/*
 * ut_sync_export_tests.c - Unit tests for ut_export_script().
 *
 * ut_export_script() is the pure, kernel-independent export primitive:
 * given in-memory outline-text bytes, a dotted ODB path, a sync directory,
 * and the script's modification time (Mac epoch, seconds since 1904), it:
 *
 *   (a) canonicalizes the bytes (MacRoman/CR/0xC7 -> UTF-8/LF/"//" form)
 *   (b) maps the dotted path to a .ut filesystem path under sync_dir
 *   (c) creates the parent directories (mkdir -p semantics)
 *   (d) writes the canonical bytes atomically (write + rename)
 *   (e) sets the .ut file's mtime from the provided Mac-epoch timestamp
 *
 * Tests assert on real file content (read back and compare) and real stat()
 * mtime, not source inspection.
 *
 * All temp dirs are created under $TMPDIR via mkdtemp and cleaned up at end
 * of each test. NEVER writes into the real usertalk_scripts/ corpus or any
 * workspace path.
 *
 * License
 * -------
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Frontier contributors.
 */

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "test_report.h"

#include "../frontier-cli/ut_sync.h"

/* ----------------------------------------------------------------------- */
/* Helpers                                                                  */
/* ----------------------------------------------------------------------- */

/*
 * Create a mkdtemp directory under $TMPDIR (POSIX) or /tmp (fallback).
 * Returns a malloc'd path. Caller must free and rmdir -r.
 */
static char *make_tmpdir(void) {
	const char *base = getenv("TMPDIR");
	if (base == NULL || base[0] == '\0')
		base = "/tmp";
	/* Strip trailing slash */
	size_t blen = strlen(base);
	while (blen > 1 && base[blen - 1] == '/')
		blen--;

	/* "TMPDIR/ut_sync_export_XXXXXX" */
	const char *suffix = "/ut_sync_export_XXXXXX";
	size_t total = blen + strlen(suffix) + 1;
	char *tmpl = (char *)malloc(total);
	if (tmpl == NULL)
		return NULL;
	memcpy(tmpl, base, blen);
	strcpy(tmpl + blen, suffix);

	if (mkdtemp(tmpl) == NULL) {
		free(tmpl);
		return NULL;
	}
	return tmpl;
}

/* Recursively remove a directory tree (simple rm -rf equivalent). */
static void rmrf(const char *path) {
	char cmd[1024];
	/* Use system() for simplicity -- this is test cleanup only */
	snprintf(cmd, sizeof(cmd), "rm -rf '%s'", path);
	(void)system(cmd);
}

/* Read entire file into a malloc'd buffer; returns 1 on success. */
static int read_file(const char *path, unsigned char **buf, size_t *len) {
	FILE *f = fopen(path, "rb");
	long sz;
	unsigned char *data;
	size_t got;

	*buf = NULL;
	*len = 0;
	if (f == NULL)
		return 0;
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

/*
 * Mac epoch is 1904-01-01; Unix epoch is 1970-01-01.
 * Offset = seconds between the two: 2082844800.
 * Mac time -> Unix time: unix = mac - 2082844800  (if mac > offset)
 * This matches timedate.c lines 314-315:
 *   const int64_t frontier_epoch_offset = 2082844800LL;
 *   time_t unix_secs = (ptime > frontier_epoch_offset) ? (time_t)(ptime - frontier_epoch_offset) : 0;
 */
#define MAC_TO_UNIX_EPOCH_OFFSET 2082844800LL

/* ----------------------------------------------------------------------- */
/* Tests                                                                    */
/* ----------------------------------------------------------------------- */

/*
 * test_basic_export - the canonical happy path.
 *
 * Input: a simple two-line script in in-memory kernel form (CR endings,
 *        0xC7 comment marker). Expected .ut output: LF endings, "//" comment,
 *        no trailing newline.
 * Asserts: file content == expected bytes; stat mtime == supplied Mac epoch
 *          converted to Unix seconds.
 */
static void test_basic_export(void) {
	char *tmpdir = make_tmpdir();
	assert(tmpdir != NULL);

	/* In-memory outline-text bytes: CR line ending, 0xC7 comment marker */
	const unsigned char raw[] = {
		'o', 'n', ' ', 'h', 'e', 'l', 'l', 'o', ' ', '(', ')', ' ', '{',
		0x0D,                           /* CR */
		0xC7, ' ', 'g', 'r', 'e', 'e', 't', 'i', 'n', 'g',
		0x0D,                           /* CR */
		'}'
	};
	/* Expected .ut: LF, "//" substituted, no trailing newline */
	const char *expected =
		"on hello () {\n"
		"// greeting\n"
		"}";

	/* A representative Mac epoch timestamp: 2026-06-03 12:00:00 local.
	 * Computed as Unix epoch + offset:
	 *   2026-06-03 12:00:00 UTC = 1748952000  (Unix)
	 *   Mac epoch             = 1748952000 + 2082844800 = 3831796800 */
	int64_t mac_mtime = (int64_t)3831796800LL;

	int rc = ut_export_script(
		raw, sizeof(raw),
		"test.hello",
		tmpdir,
		mac_mtime
	);
	assert(rc == 1);

	/* Compute expected path: <tmpdir>/test/hello.ut */
	char expected_path[1024];
	int n = snprintf(expected_path, sizeof(expected_path),
	                 "%s/test/hello.ut", tmpdir);
	assert(n > 0 && n < (int)sizeof(expected_path));

	/* Read back and compare content */
	unsigned char *got = NULL;
	size_t got_len = 0;
	assert(read_file(expected_path, &got, &got_len) == 1);

	size_t exp_len = strlen(expected);
	if (got_len != exp_len || memcmp(got, expected, exp_len) != 0) {
		fprintf(stderr, "test_basic_export: content mismatch\n");
		fprintf(stderr, "  expected (%zu bytes): %s\n", exp_len, expected);
		fprintf(stderr, "  got      (%zu bytes): %s\n", got_len, (char *)got);
	}
	assert(got_len == exp_len);
	assert(memcmp(got, expected, exp_len) == 0);
	free(got);

	/* Check mtime */
	struct stat st;
	assert(stat(expected_path, &st) == 0);
	time_t expected_unix = (time_t)(mac_mtime - MAC_TO_UNIX_EPOCH_OFFSET);
	if (st.st_mtime != expected_unix) {
		fprintf(stderr, "test_basic_export: mtime mismatch: got %ld expected %ld\n",
		        (long)st.st_mtime, (long)expected_unix);
	}
	assert(st.st_mtime == expected_unix);

	rmrf(tmpdir);
	free(tmpdir);
}

/*
 * test_deep_path_creates_dirs - verifies mkdir -p semantics for a script
 * nested three levels deep.
 */
static void test_deep_path_creates_dirs(void) {
	char *tmpdir = make_tmpdir();
	assert(tmpdir != NULL);

	const unsigned char raw[] = "x = 1";
	const char *expected = "x = 1";
	int64_t mac_mtime = (int64_t)3831796800LL;

	int rc = ut_export_script(
		raw, sizeof(raw) - 1, /* exclude NUL */
		"system.verbs.builtins.op",
		tmpdir,
		mac_mtime
	);
	assert(rc == 1);

	char expected_path[1024];
	snprintf(expected_path, sizeof(expected_path),
	         "%s/system/verbs/builtins/op.ut", tmpdir);

	unsigned char *got = NULL;
	size_t got_len = 0;
	assert(read_file(expected_path, &got, &got_len) == 1);
	assert(got_len == strlen(expected));
	assert(memcmp(got, expected, got_len) == 0);
	free(got);

	rmrf(tmpdir);
	free(tmpdir);
}

/*
 * test_idempotent_overwrite - calling ut_export_script twice on the same path
 * must succeed (the second call overwrites the first atomically).
 */
static void test_idempotent_overwrite(void) {
	char *tmpdir = make_tmpdir();
	assert(tmpdir != NULL);

	const unsigned char raw1[] = "x = 1";
	const unsigned char raw2[] = "x = 2";
	int64_t mtime1 = (int64_t)3831796800LL;
	int64_t mtime2 = (int64_t)3831796860LL; /* 60 seconds later */

	assert(ut_export_script(raw1, sizeof(raw1) - 1, "a.b", tmpdir, mtime1) == 1);
	assert(ut_export_script(raw2, sizeof(raw2) - 1, "a.b", tmpdir, mtime2) == 1);

	char path[1024];
	snprintf(path, sizeof(path), "%s/a/b.ut", tmpdir);

	unsigned char *got = NULL;
	size_t got_len = 0;
	assert(read_file(path, &got, &got_len) == 1);
	assert(got_len == sizeof(raw2) - 1);
	assert(memcmp(got, raw2, got_len) == 0);
	free(got);

	struct stat st;
	assert(stat(path, &st) == 0);
	time_t expected_unix = (time_t)(mtime2 - MAC_TO_UNIX_EPOCH_OFFSET);
	assert(st.st_mtime == expected_unix);

	rmrf(tmpdir);
	free(tmpdir);
}

/*
 * test_unsafe_path_rejected - ut_export_script must return 0 for paths that
 * ut_odb_path_to_fs would reject (dotdot, empty segment, etc.).
 */
static void test_unsafe_path_rejected(void) {
	char *tmpdir = make_tmpdir();
	assert(tmpdir != NULL);

	const unsigned char raw[] = "x = 1";
	int64_t mtime = (int64_t)3831796800LL;

	/* ".." segment is rejected by ut_odb_path_to_fs */
	int rc = ut_export_script(raw, sizeof(raw) - 1, "a..b", tmpdir, mtime);
	assert(rc == 0);

	/* empty dotted path */
	rc = ut_export_script(raw, sizeof(raw) - 1, "", tmpdir, mtime);
	assert(rc == 0);

	rmrf(tmpdir);
	free(tmpdir);
}

/*
 * test_empty_script - an empty source (0 bytes) must produce an empty .ut
 * (the canonicalizer returns an empty buffer for empty input).
 */
static void test_empty_script(void) {
	char *tmpdir = make_tmpdir();
	assert(tmpdir != NULL);

	int64_t mtime = (int64_t)3831796800LL;
	int rc = ut_export_script(
		(const unsigned char *)"", 0,
		"empty.script",
		tmpdir,
		mtime
	);
	assert(rc == 1);

	char path[1024];
	snprintf(path, sizeof(path), "%s/empty/script.ut", tmpdir);

	unsigned char *got = NULL;
	size_t got_len = 0;
	assert(read_file(path, &got, &got_len) == 1);
	assert(got_len == 0);
	free(got);

	rmrf(tmpdir);
	free(tmpdir);
}

/* ----------------------------------------------------------------------- */
/* main                                                                     */
/* ----------------------------------------------------------------------- */

int main(void) {
	TR_INIT("ut_sync_export_tests");
	TR_RUN(test_basic_export);
	TR_RUN(test_deep_path_creates_dirs);
	TR_RUN(test_idempotent_overwrite);
	TR_RUN(test_unsafe_path_rejected);
	TR_RUN(test_empty_script);
	TR_SUMMARY();
	return TR_EXIT_CODE();
}
