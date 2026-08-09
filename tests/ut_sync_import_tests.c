/*
 * ut_sync_import_tests.c - Unit tests for ut_import_check().
 *
 * ut_import_check() is the pure import primitive:
 *   - Maps a dotted ODB path to a .ut filesystem path (via ut_odb_path_to_fs).
 *   - stat()s the .ut file; returns 0 immediately if missing.
 *   - Converts st_mtime to Mac epoch (st_mtime + 2082844800).
 *   - Compares .ut Mac mtime to the supplied ODB mtime; returns 0 if not newer.
 *   - Reads the .ut, runs ut_decanonicalize_outline_text on it.
 *   - On success returns 1 with the decanonicalized bytes and the .ut Mac mtime.
 *
 * All temp dirs created under $TMPDIR via mkdtemp. Never touches the real
 * usertalk_scripts/ corpus or any workspace path.
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
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <utime.h>

#include "test_report.h"
#include "../frontier-cli/ut_sync.h"

/* ----------------------------------------------------------------------- */
/* Helpers                                                                  */
/* ----------------------------------------------------------------------- */

/*
 * Mac epoch (seconds since 1904-01-01) offset from Unix epoch (1970-01-01).
 * Matches ut_sync.c UT_MAC_TO_UNIX_EPOCH_OFFSET and timedate.c.
 */
#define MAC_TO_UNIX_EPOCH_OFFSET ((int64_t)2082844800LL)

/*
 * Legacy-signature wrappers (unit 1.5 added sync-state outs to
 * ut_import_check and a conflict out to ut_export_script). The pre-existing
 * tests exercise mtime/decanonicalize behavior and pass NULL/ignore for the
 * new outputs; the new sync-state tests below use the full signatures.
 */
static int import_check6(const char *dotted, const char *base, int64_t odb,
                         unsigned char **out, size_t *outlen, int64_t *mt) {
	return ut_import_check(dotted, base, odb, out, outlen, mt,
	                       NULL, NULL, NULL, NULL);
}

static int export_script5(const unsigned char *raw, size_t rawlen,
                          const char *dotted, const char *sync_dir,
                          int64_t mac_mtime) {
	return ut_export_script(raw, rawlen, dotted, sync_dir, mac_mtime, NULL);
}

/*
 * Make a mkdtemp() scratch dir under $TMPDIR (or /tmp).
 * Template name: ut_sync_import_XXXXXX
 * Returns malloc'd path (caller frees and rmrf's).
 */
static char *make_tmpdir(void) {
	const char *base = getenv("TMPDIR");
	if (base == NULL || base[0] == '\0')
		base = "/tmp";
	size_t blen = strlen(base);
	while (blen > 1 && base[blen - 1] == '/')
		blen--;
	const char *suffix = "/ut_sync_import_XXXXXX";
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

/* Recursively remove a directory tree (test cleanup only). */
static void rmrf(const char *path) {
	char cmd[1024];
	snprintf(cmd, sizeof(cmd), "rm -rf '%s'", path);
	(void)system(cmd);
}

/*
 * Write bytes to a file (creates the file and any parent dirs).
 * Returns 1 on success.
 */
static int write_file(const char *path, const unsigned char *data, size_t len) {
	/* Create parent directory if needed */
	char parent[4096];
	if (strlen(path) >= sizeof(parent))
		return 0;
	strcpy(parent, path);
	char *slash = strrchr(parent, '/');
	if (slash != NULL) {
		*slash = '\0';
		/* mkdir -p the parent */
		char cmd[4096 + 10];
		snprintf(cmd, sizeof(cmd), "mkdir -p '%s'", parent);
		if (system(cmd) != 0)
			return 0;
	}

	FILE *f = fopen(path, "wb");
	if (f == NULL)
		return 0;
	if (len > 0 && fwrite(data, 1, len, f) != len) {
		fclose(f);
		return 0;
	}
	return fclose(f) == 0 ? 1 : 0;
}

/*
 * Set a file's mtime to a given Unix epoch time.
 * Returns 1 on success.
 */
static int set_file_mtime(const char *path, time_t unix_mtime) {
	struct timeval tv[2];
	tv[0].tv_sec  = unix_mtime;
	tv[0].tv_usec = 0;
	tv[1].tv_sec  = unix_mtime;
	tv[1].tv_usec = 0;
	return utimes(path, tv) == 0 ? 1 : 0;
}

/* ----------------------------------------------------------------------- */
/* Tests                                                                    */
/* ----------------------------------------------------------------------- */

/*
 * test_newer_ut_imports - the main happy path.
 *
 * Write a .ut with a known mtime that is newer than odb_mac_mtime.
 * Assert: returns 1, out contains decanonicalized bytes (LF->CR, "//"->.C7),
 * and ut_mac_mtime is the expected Mac epoch value.
 */
static void test_newer_ut_imports(void) {
	char *tmpdir = make_tmpdir();
	assert(tmpdir != NULL);

	/*
	 * .ut content: a simple two-line script in canonical form
	 * (UTF-8, LF line endings, "//" comment, no trailing newline).
	 *
	 * Expected decanonicalized (MacRoman/CR/0xC7) form:
	 *   "on hello ()\x0D\xC7 greeting\x0D}"
	 */
	const char *ut_content = "on hello ()\n// greeting\n}";

	/* Build the .ut path: <tmpdir>/test/hello.ut */
	char ut_path[4096];
	snprintf(ut_path, sizeof(ut_path), "%s/test/hello.ut", tmpdir);

	assert(write_file(ut_path,
	                  (const unsigned char *)ut_content, strlen(ut_content)) == 1);

	/*
	 * Set .ut mtime to a fixed Unix time (2026-06-03 12:00:00 UTC = 1748952000).
	 * Mac epoch = 1748952000 + 2082844800 = 3831796800.
	 */
	time_t ut_unix_mtime = 1748952000L;
	int64_t ut_mac_mtime_expected = (int64_t)ut_unix_mtime + MAC_TO_UNIX_EPOCH_OFFSET;
	assert(set_file_mtime(ut_path, ut_unix_mtime) == 1);

	/* ODB mtime is 1 second earlier (Mac epoch). */
	int64_t odb_mac_mtime = ut_mac_mtime_expected - 1;

	unsigned char *out = NULL;
	size_t outlen = 0;
	int64_t got_ut_mac_mtime = 0;

	int rc = import_check6("test.hello", tmpdir,
	                         odb_mac_mtime, &out, &outlen, &got_ut_mac_mtime);
	assert(rc == 1);
	assert(out != NULL);

	/*
	 * Expected decanonicalized bytes:
	 *   "on hello ()" CR 0xC7 " greeting" CR "}"
	 * That is: "on hello ()\x0D\xC7 greeting\x0D}"
	 */
	const unsigned char expected[] = {
		'o','n',' ','h','e','l','l','o',' ','(',')',
		0x0D,            /* CR */
		0xC7, ' ', 'g','r','e','e','t','i','n','g',
		0x0D,            /* CR */
		'}'
	};
	size_t exp_len = sizeof(expected);

	if (outlen != exp_len || memcmp(out, expected, exp_len) != 0) {
		fprintf(stderr, "test_newer_ut_imports: content mismatch\n");
		fprintf(stderr, "  expected %zu bytes:", exp_len);
		for (size_t i = 0; i < exp_len; i++)
			fprintf(stderr, " %02x", expected[i]);
		fprintf(stderr, "\n  got      %zu bytes:", outlen);
		for (size_t i = 0; i < outlen; i++)
			fprintf(stderr, " %02x", out[i]);
		fprintf(stderr, "\n");
	}
	assert(outlen == exp_len);
	assert(memcmp(out, expected, exp_len) == 0);

	/* ut_mac_mtime must match the converted mtime. Allow +/-1 for rounding. */
	int64_t diff = got_ut_mac_mtime - ut_mac_mtime_expected;
	if (diff < -1 || diff > 1) {
		fprintf(stderr, "test_newer_ut_imports: ut_mac_mtime mismatch: "
		        "got %lld expected %lld\n",
		        (long long)got_ut_mac_mtime, (long long)ut_mac_mtime_expected);
	}
	assert(diff >= -1 && diff <= 1);

	free(out);
	rmrf(tmpdir);
	free(tmpdir);
}

/*
 * test_older_ut_no_import - .ut mtime <= odb_mac_mtime -> returns 0, out NULL.
 */
static void test_older_ut_no_import(void) {
	char *tmpdir = make_tmpdir();
	assert(tmpdir != NULL);

	const char *ut_content = "x = 1\n";
	char ut_path[4096];
	snprintf(ut_path, sizeof(ut_path), "%s/a/b.ut", tmpdir);
	assert(write_file(ut_path,
	                  (const unsigned char *)ut_content, strlen(ut_content)) == 1);

	/* .ut mtime: 2026-01-01 00:00:00 UTC */
	time_t ut_unix_mtime = 1735689600L;
	assert(set_file_mtime(ut_path, ut_unix_mtime) == 1);
	int64_t ut_mac_mtime = (int64_t)ut_unix_mtime + MAC_TO_UNIX_EPOCH_OFFSET;

	/* ODB mtime is equal to .ut mtime -- not newer, so no import. */
	int64_t odb_mac_mtime = ut_mac_mtime;

	unsigned char *out = NULL;
	size_t outlen = 0;
	int64_t got_ut_mac_mtime = 0;

	int rc = import_check6("a.b", tmpdir,
	                         odb_mac_mtime, &out, &outlen, &got_ut_mac_mtime);
	assert(rc == 0);
	assert(out == NULL);

	/* Also check strictly older (ODB mtime > .ut mtime). */
	rc = import_check6("a.b", tmpdir,
	                     ut_mac_mtime + 1, &out, &outlen, &got_ut_mac_mtime);
	assert(rc == 0);
	assert(out == NULL);

	rmrf(tmpdir);
	free(tmpdir);
}

/*
 * test_missing_ut_returns_0 - if the .ut file does not exist, returns 0.
 */
static void test_missing_ut_returns_0(void) {
	char *tmpdir = make_tmpdir();
	assert(tmpdir != NULL);

	unsigned char *out = NULL;
	size_t outlen = 0;
	int64_t got_ut_mac_mtime = 0;

	int rc = import_check6("no.such.script", tmpdir,
	                         (int64_t)0, &out, &outlen, &got_ut_mac_mtime);
	assert(rc == 0);
	assert(out == NULL);

	rmrf(tmpdir);
	free(tmpdir);
}

/*
 * test_non_macroman_returns_0 - a .ut containing a character that has no
 * MacRoman representation (e.g. a UTF-8 emoji) fails decanonicalization and
 * returns 0. *out must be NULL.
 */
static void test_non_macroman_returns_0(void) {
	char *tmpdir = make_tmpdir();
	assert(tmpdir != NULL);

	/* Snowman emoji U+2603 (UTF-8: E2 98 83) -- not in MacRoman */
	const char *ut_content = "x = \xE2\x98\x83\n";

	char ut_path[4096];
	snprintf(ut_path, sizeof(ut_path), "%s/emoji/script.ut", tmpdir);
	assert(write_file(ut_path,
	                  (const unsigned char *)ut_content, strlen(ut_content)) == 1);

	/* Make .ut much newer than ODB so time check passes. */
	time_t ut_unix_mtime = 1748952000L;
	assert(set_file_mtime(ut_path, ut_unix_mtime) == 1);
	int64_t odb_mac_mtime = (int64_t)ut_unix_mtime + MAC_TO_UNIX_EPOCH_OFFSET - 1000;

	unsigned char *out = NULL;
	size_t outlen = 0;
	int64_t got_ut_mac_mtime = 0;

	int rc = import_check6("emoji.script", tmpdir,
	                         odb_mac_mtime, &out, &outlen, &got_ut_mac_mtime);
	assert(rc == 0);
	assert(out == NULL);

	rmrf(tmpdir);
	free(tmpdir);
}

/*
 * test_unsafe_dotted_path_returns_0 - a dotted path containing ".." is
 * rejected by ut_odb_path_to_fs and must cause ut_import_check to return 0.
 */
static void test_unsafe_dotted_path_returns_0(void) {
	char *tmpdir = make_tmpdir();
	assert(tmpdir != NULL);

	unsigned char *out = NULL;
	size_t outlen = 0;
	int64_t got_ut_mac_mtime = 0;

	/* ".." segment is unsafe */
	int rc = import_check6("a..b", tmpdir,
	                         (int64_t)0, &out, &outlen, &got_ut_mac_mtime);
	assert(rc == 0);
	assert(out == NULL);

	/* Empty dotted path */
	rc = import_check6("", tmpdir,
	                     (int64_t)0, &out, &outlen, &got_ut_mac_mtime);
	assert(rc == 0);
	assert(out == NULL);

	rmrf(tmpdir);
	free(tmpdir);
}

/*
 * test_roundtrip - ut_export_script writes a .ut; ut_import_check with an
 * older odb_mtime reads it back and returns the original raw bytes (for input
 * that survives the forward+reverse transform without loss).
 *
 * Pick raw input that is ASCII-only (no 0xC8 brace-strip, no high-bit bytes)
 * so the round-trip is exact: canonicalize(raw) -> write .ut -> read .ut ->
 * decanonicalize(read) == raw.
 *
 * ASCII input with CR line endings and 0xC7 comment:
 *   "x = 1\x0Dy = 2"  (two lines, no trailing CR)
 *
 * Canonicalized (written to .ut):
 *   "x = 1\ny = 2"  (LF, no change to content since no 0xC7/0xC8/high bytes)
 *
 * Decanonicalized (returned by ut_import_check):
 *   "x = 1\x0Dy = 2"  == the original raw bytes
 */
static void test_roundtrip(void) {
	char *tmpdir = make_tmpdir();
	assert(tmpdir != NULL);

	const unsigned char raw[] = {
		'x', ' ', '=', ' ', '1', 0x0D,    /* CR line ending */
		'y', ' ', '=', ' ', '2'
	};
	size_t rawlen = sizeof(raw);

	/* Use Mac epoch timestamp well past 1970 */
	int64_t mac_mtime_export = (int64_t)3831796800LL;  /* 2026-06-03 12:00:00 */
	time_t unix_mtime_export = (time_t)(mac_mtime_export - MAC_TO_UNIX_EPOCH_OFFSET);

	/* Export the script to .ut */
	int exported = export_script5(raw, rawlen, "round.trip", tmpdir,
	                                mac_mtime_export);
	assert(exported == 1);

	/* Verify the file exists and has the right mtime */
	char ut_path[4096];
	snprintf(ut_path, sizeof(ut_path), "%s/round/trip.ut", tmpdir);
	struct stat st;
	assert(stat(ut_path, &st) == 0);
	assert(st.st_mtime == unix_mtime_export);

	/* Import with an ODB mtime 1 second older */
	int64_t odb_mac_mtime = mac_mtime_export - 1;
	unsigned char *out = NULL;
	size_t outlen = 0;
	int64_t got_ut_mac_mtime = 0;

	int rc = import_check6("round.trip", tmpdir,
	                         odb_mac_mtime, &out, &outlen, &got_ut_mac_mtime);
	assert(rc == 1);
	assert(out != NULL);

	/* Round-trip must reproduce the original raw bytes exactly */
	if (outlen != rawlen || memcmp(out, raw, rawlen) != 0) {
		fprintf(stderr, "test_roundtrip: round-trip mismatch\n");
		fprintf(stderr, "  original %zu bytes:", rawlen);
		for (size_t i = 0; i < rawlen; i++)
			fprintf(stderr, " %02x", raw[i]);
		fprintf(stderr, "\n  got      %zu bytes:", outlen);
		for (size_t i = 0; i < outlen; i++)
			fprintf(stderr, " %02x", out[i]);
		fprintf(stderr, "\n");
	}
	assert(outlen == rawlen);
	assert(memcmp(out, raw, rawlen) == 0);

	/* ut_mac_mtime should be close to mac_mtime_export (same second) */
	int64_t diff = got_ut_mac_mtime - mac_mtime_export;
	if (diff < -1 || diff > 1) {
		fprintf(stderr, "test_roundtrip: ut_mac_mtime mismatch: "
		        "got %lld expected %lld\n",
		        (long long)got_ut_mac_mtime, (long long)mac_mtime_export);
	}
	assert(diff >= -1 && diff <= 1);

	free(out);
	rmrf(tmpdir);
	free(tmpdir);
}

/* ----------------------------------------------------------------------- */
/* Unit 1.5: content hash + sync-state manifest                             */
/* ----------------------------------------------------------------------- */

/*
 * test_content_hash - trailing-newline insensitivity and change detection.
 */
static void test_content_hash(void) {
	const unsigned char a1[] = "x = 1";
	const unsigned char a2[] = "x = 1\n";
	const unsigned char a3[] = "x = 1\r\n";
	const unsigned char b[]  = "x = 2";

	uint64_t h1 = ut_content_hash(a1, sizeof(a1) - 1);
	uint64_t h2 = ut_content_hash(a2, sizeof(a2) - 1);
	uint64_t h3 = ut_content_hash(a3, sizeof(a3) - 1);
	uint64_t hb = ut_content_hash(b, sizeof(b) - 1);

	assert(h1 == h2);
	assert(h1 == h3);
	assert(h1 != hb);
	assert(ut_content_hash(NULL, 0) == ut_content_hash((const unsigned char *)"\n", 1));
}

/*
 * test_sync_state_roundtrip - record, lookup, replace, and miss behavior of
 * the .ut-sync-state manifest.
 */
static void test_sync_state_roundtrip(void) {
	char *tmpdir = make_tmpdir();
	assert(tmpdir != NULL);

	uint64_t ut_h = 0, odb_h = 0;

	/* Missing manifest: lookup misses. */
	assert(ut_sync_state_lookup(tmpdir, "a.b", &ut_h, &odb_h) == 0);

	/* Record two entries (one with a space in the key, which ODB allows). */
	assert(ut_sync_state_record(tmpdir, "a.b", 0x1111, 0x2222) == 1);
	assert(ut_sync_state_record(tmpdir, "a.spaced key", 0x3333, 0x4444) == 1);

	assert(ut_sync_state_lookup(tmpdir, "a.b", &ut_h, &odb_h) == 1);
	assert(ut_h == 0x1111 && odb_h == 0x2222);
	assert(ut_sync_state_lookup(tmpdir, "a.spaced key", &ut_h, &odb_h) == 1);
	assert(ut_h == 0x3333 && odb_h == 0x4444);

	/* Replace an entry; the other must survive. */
	assert(ut_sync_state_record(tmpdir, "a.b", 0x5555, 0x6666) == 1);
	assert(ut_sync_state_lookup(tmpdir, "a.b", &ut_h, &odb_h) == 1);
	assert(ut_h == 0x5555 && odb_h == 0x6666);
	assert(ut_sync_state_lookup(tmpdir, "a.spaced key", &ut_h, &odb_h) == 1);
	assert(ut_h == 0x3333 && odb_h == 0x4444);

	/* Different path still misses. */
	assert(ut_sync_state_lookup(tmpdir, "a.c", &ut_h, &odb_h) == 0);

	rmrf(tmpdir);
	free(tmpdir);
}

/*
 * test_import_meta - a full-signature import check reports the .ut hash and
 * the recorded sync point, giving the caller everything it needs to detect
 * a two-sided conflict before installing.
 */
static void test_import_meta(void) {
	char *tmpdir = make_tmpdir();
	assert(tmpdir != NULL);

	const unsigned char raw[] = { 'x', ' ', '=', ' ', '1' };
	int64_t mac_mtime = (int64_t)3831796800LL;

	/* Export records the sync point. */
	assert(export_script5(raw, sizeof(raw), "meta.check", tmpdir, mac_mtime) == 1);

	uint64_t rec_ut = 0, rec_odb = 0;
	assert(ut_sync_state_lookup(tmpdir, "meta.check", &rec_ut, &rec_odb) == 1);
	assert(rec_ut == rec_odb); /* export records the written content twice */

	/* Hand-edit the .ut (newer mtime, different content). */
	char ut_path[4096];
	snprintf(ut_path, sizeof(ut_path), "%s/meta/check.ut", tmpdir);
	FILE *fp = fopen(ut_path, "w");
	assert(fp != NULL);
	fputs("x = 2\n", fp);
	fclose(fp);

	unsigned char *out = NULL;
	size_t outlen = 0;
	int64_t got_mt = 0;
	uint64_t ut_hash = 0, got_rec_ut = 0, got_rec_odb = 0;
	int have_recorded = 0;

	int rc = ut_import_check("meta.check", tmpdir, mac_mtime - 1,
	                         &out, &outlen, &got_mt,
	                         &ut_hash, &have_recorded,
	                         &got_rec_ut, &got_rec_odb);
	assert(rc == 1);
	assert(have_recorded == 1);
	assert(got_rec_ut == rec_ut && got_rec_odb == rec_odb);
	assert(ut_hash != rec_ut); /* the .ut side changed */
	assert(ut_hash == ut_content_hash((const unsigned char *)"x = 2", 5));

	free(out);
	rmrf(tmpdir);
	free(tmpdir);
}

/* ----------------------------------------------------------------------- */
/* main                                                                     */
/* ----------------------------------------------------------------------- */

int main(void) {
	TR_INIT("ut_sync_import_tests");
	TR_RUN(test_newer_ut_imports);
	TR_RUN(test_older_ut_no_import);
	TR_RUN(test_missing_ut_returns_0);
	TR_RUN(test_non_macroman_returns_0);
	TR_RUN(test_unsafe_dotted_path_returns_0);
	TR_RUN(test_roundtrip);
	TR_RUN(test_content_hash);
	TR_RUN(test_sync_state_roundtrip);
	TR_RUN(test_import_meta);
	TR_SUMMARY();
	return TR_EXIT_CODE();
}
