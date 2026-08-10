/*
 * ut_scan.c - Boot-time import-discovery scan for ODB<->.ut sync.
 *
 * Implements ut_sync_scan_and_create(): walk the .ut sync tree, find .ut
 * leaves that have no corresponding in-memory ODB node, and auto-create the
 * missing intermediate table chain + leaf script. Each created path is
 * recorded via cli_record_ut_import() so the existing redirty pass persists
 * the new nodes on the next tablesavesystemtable call.
 *
 * This file is compiled ONLY into the production CLI. It must NOT be linked
 * into any kernel-free test binary.
 *
 * License
 * -------
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Frontier contributors.
 */

#include "ut_scan.h"
#include "ut_sync.h"

/* POSIX / libc */
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/* Kernel headers -- this file is CLI-only, so these links are fine. */
#include "../Common/headers/frontier.h"
#include "../Common/headers/logging.h"
#include "../Common/headers/lang.h"
#include "../Common/headers/memory.h"
#include "../Common/headers/strings.h"
#include "../Common/headers/tablestructure.h"
#include "../Common/headers/langexternal.h"
#include "../Common/headers/opinternal.h"
#include "../Common/headers/opverbs.h"

/* cli_record_ut_import is defined in main.c; forward-declared here to avoid
 * a circular include (main.c already includes ut_scan.h). */
extern void cli_record_ut_import(const char *dotted_path);


/* -------------------------------------------------------------------------
 * Internal: count the number of decoded bytes that ut_pct_decode_segment
 * would produce for the given encoded segment, WITHOUT actually decoding.
 *
 * Each non-'%' character contributes 1 byte. Each valid '%XX' escape
 * contributes 1 byte. Returns -1 if the encoding is malformed (lone '%'
 * or '%X' without two hex digits). The caller can compare this against
 * strlen(decoded) to detect an embedded NUL: if they differ, at least one
 * decoded byte was 0x00 (which strlen stops at, giving a smaller count).
 * ------------------------------------------------------------------------- */
static int hex_digit_val(unsigned char c) {
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'A' && c <= 'F') return c - 'A' + 10;
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	return -1;
}

static int pct_decoded_byte_count(const char *enc) {
	int count = 0;
	const char *p = enc;
	while (*p != '\0') {
		if (*p == '%') {
			if (p[1] == '\0' || p[2] == '\0')
				return -1; /* malformed */
			if (hex_digit_val((unsigned char)p[1]) < 0 ||
			    hex_digit_val((unsigned char)p[2]) < 0)
				return -1; /* malformed */
			count++;
			p += 3;
		} else {
			count++;
			p++;
		}
	}
	return count;
}


/* -------------------------------------------------------------------------
 * Internal: maximum depth for recursive directory walk (guards against
 * pathological symlink loops that O_NOFOLLOW at the open level can't catch
 * for the opendir path).
 * ------------------------------------------------------------------------- */
#define UT_SCAN_MAX_DEPTH 32

/*
 * Maximum segments in a dotted ODB path. A reasonable corpus ceiling; paths
 * deeper than this are silently skipped with a warning (don't abort the scan).
 */
#define UT_SCAN_MAX_SEGMENTS 64

/*
 * Maximum size of a .ut file we will attempt to import during the scan.
 * ut_sync.c uses UT_MAX_FILE_BYTES (8 MB) as the import-hook cap; this
 * scan-side cap is intentionally tighter (1 MB) -- boot-scan files should
 * be small scripts, and a larger cap just widens the attack surface for
 * resource exhaustion via a planted oversized file in the sync dir.
 */
#define UT_SCAN_SIZE_CAP ((size_t)(1u << 20))


/* -------------------------------------------------------------------------
 * Internal: split a NUL-terminated dotted string into its segments.
 *
 * Writes up to max_segs pointers into segs[] (pointing into buf, which is
 * modified in place by NUL-termination). Returns the number of segments, or
 * -1 if there are more than max_segs.
 * ------------------------------------------------------------------------- */
static int split_dotted(char *buf, const char **segs, int max_segs) {
	int n = 0;
	char *p = buf;
	segs[n++] = p;
	while (*p != '\0') {
		if (*p == '.') {
			*p = '\0';
			if (n >= max_segs)
				return -1;
			segs[n++] = p + 1;
		}
		p++;
	}
	return n;
}


/* -------------------------------------------------------------------------
 * Internal: read an entire .ut file from an already-opened fd.
 *
 * Takes ownership of fd (closes it on both success and failure).
 * The fd must have been opened with O_NOFOLLOW and O_NONBLOCK; this
 * function verifies S_ISREG via fstat, clears O_NONBLOCK, and reads.
 * fspath_for_log is used only for warning messages -- it does not
 * re-open the file, so no TOCTOU window is introduced.
 *
 * Returns 1 on success (*out is malloc'd, *outlen is the byte count).
 * Returns 0 on any error (*out is NULL, *outlen is 0).
 * ------------------------------------------------------------------------- */
static int read_ut_file_fd(int fd, const char *fspath_for_log,
                            unsigned char **out, size_t *outlen) {
	*out = NULL;
	*outlen = 0;

	struct stat st;
	if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) || st.st_size < 0) {
		close(fd);
		return 0;
	}
	/* Confirmed regular file: clear O_NONBLOCK so read() blocks normally. */
	int fl = fcntl(fd, F_GETFL);
	if (fl != -1)
		(void) fcntl(fd, F_SETFL, fl & ~O_NONBLOCK);

	size_t sz = (size_t)st.st_size;
	if (sz > UT_SCAN_SIZE_CAP) {
		log_warn(LOG_COMP_STARTUP,
		         "ut-scan: skipping oversized .ut (%zu bytes): %s", sz, fspath_for_log);
		close(fd);
		return 0;
	}

	/* Empty file is valid (empty script body). */
	unsigned char *buf = (unsigned char *) malloc(sz + 1);
	if (buf == NULL) {
		close(fd);
		return 0;
	}

	size_t total = 0;
	while (total < sz) {
		ssize_t n = read(fd, buf + total, sz - total);
		if (n < 0) {
			free(buf);
			close(fd);
			return 0;
		}
		if (n == 0)
			break;
		total += (size_t)n;
	}
	close(fd);

	buf[total] = '\0';
	*out = buf;
	*outlen = total;
	return 1;
}

/* -------------------------------------------------------------------------
 * Internal: read an entire .ut file into a malloc'd buffer (path-based).
 *
 * Opens with O_NOFOLLOW|O_NONBLOCK to refuse symlinks and avoid blocking
 * on a FIFO before the S_ISREG fstat guard in read_ut_file_fd runs.
 * Delegates to read_ut_file_fd after the open.
 *
 * Returns 1 on success (*out is malloc'd, *outlen is the byte count).
 * Returns 0 on any error (*out is NULL, *outlen is 0).
 * ------------------------------------------------------------------------- */
static int read_ut_file(const char *fspath, unsigned char **out, size_t *outlen) {
	int fd = open(fspath, O_RDONLY | O_NOFOLLOW | O_NONBLOCK);
	if (fd < 0) {
		*out = NULL;
		*outlen = 0;
		return 0;
	}
	return read_ut_file_fd(fd, fspath, out, outlen);
}


/* -------------------------------------------------------------------------
 * Internal: check whether a .ut leaf already has a live in-memory ODB node.
 *
 * Takes the ENCODED dotted path (e.g. "system.verbs.builtins.foo%2Ebar"),
 * decodes each segment, and walks the hashtable chain from roottable.
 *
 * Returns:
 *   1  - node exists in memory (skip creation)
 *   0  - node is absent from memory (genuine orphan - create it)
 *
 * CRITICAL: This MUST use direct hashtable descent (hashtablelookupnode),
 * NOT any verb-resolution or defined()-style call. The EFP search path
 * causes defined()/@address lookups on DIRECT children of
 * system.verbs.builtins to return a phantom true even for nodes that do
 * not exist. Structural hashtable descent is the only EFP-immune existence
 * check. See docs/ARCHITECTURAL_ANTIPATTERNS.md "Name resolution vs verb
 * dispatch" and the test comment in ut_sync_lifecycle_test.sh Test 6.
 *
 * POST-FULL-MATERIALIZATION SEMANTICS:
 * At the boot seam where this function is called, langhash_materialize_disk_values
 * has already walked the entire tree and loaded every external table into memory.
 * Therefore a miss in the hashtable at any depth is a genuine structural absence
 * (the node does not exist in the ODB), NOT a lazy-load deferral. We do not need
 * a separate lazy-registry check: the full materialization pass guarantees that
 * any real ODB node will be present in the in-memory hashtable chain.
 * ------------------------------------------------------------------------- */
static int node_exists_in_memory(const char *encoded_dotted) {
	/* Work on a copy so split_dotted can NUL-terminate in place. */
	char *buf = strdup(encoded_dotted);
	if (buf == NULL)
		return 1; /* conservatively treat as exists on OOM to avoid creating garbage */

	const char *segs[UT_SCAN_MAX_SEGMENTS];
	int nseg = split_dotted(buf, segs, UT_SCAN_MAX_SEGMENTS);
	if (nseg <= 0) {
		free(buf);
		return 1; /* malformed or too deep -- skip */
	}

	/*
	 * Walk the hashtable chain segment by segment. After each intermediate
	 * segment we descend into its table. At the final (leaf) segment, if we
	 * find a node the leaf exists; otherwise it does not.
	 *
	 * GIL assumption: called on the main thread during boot while the GIL is
	 * held. No locking required.
	 */
	hdlhashtable curtable = roottable;
	int found = 0;

	for (int i = 0; i < nseg && curtable != nil; i++) {
		/* Decode the percent-encoded segment to its raw ODB key. */
		char decoded[256]; /* max Pascal name: 255 bytes + NUL */
		if (!ut_pct_decode_segment(segs[i], decoded, sizeof(decoded))) {
			/* Malformed encoding -- treat as absent (orphan candidate). */
			found = 0;
			goto done;
		}

		size_t rawlen = strlen(decoded);

		/*
		 * P1 #2 -- post-decode NUL guard: a crafted %00 sequence in an encoded
		 * filename decodes to a NUL byte. strlen() stops at the first NUL, so
		 * rawlen (= strlen(decoded)) would be shorter than the actual decoded
		 * byte count. This silently truncates the Pascal string we build from
		 * decoded[0..rawlen], potentially corrupting a hashtable key.
		 *
		 * Detection: pct_decoded_byte_count() counts the bytes the decoder
		 * WOULD produce (one per char or per %XX) without actually decoding.
		 * If that count exceeds strlen(decoded), at least one decoded byte was
		 * 0x00 and strlen stopped early. In that case, reject this orphan.
		 *
		 * Note: segment_is_safe() is NOT called here. segment_is_safe() is a
		 * forward-direction (ODB->fs) validator that rejects "/" and control
		 * bytes to prevent unsafe filesystem path components. On the reverse
		 * path (fs->ODB), "/" and control bytes are valid ODB key characters
		 * (e.g. URL-keyed tables like "http://webns.net/mvcb/") and must be
		 * allowed through. NUL is the only byte dangerous in this direction.
		 */
		{
			int expected_len = pct_decoded_byte_count(segs[i]);
			if (expected_len < 0 || (size_t)expected_len != rawlen) {
				/* Malformed encoding or embedded NUL: reject. */
				log_warn(LOG_COMP_STARTUP,
				         "ut-scan: embedded NUL or malformed encoding in segment (encoded: '%.64s'), skipping",
				         segs[i]);
				found = 0;
				goto done;
			}
		}

		if (rawlen == 0) {
			/* Empty decoded segment is not a valid ODB key. */
			found = 0;
			goto done;
		}

		if (rawlen > 255) {
			found = 0; /* too long for a Pascal string */
			goto done;
		}

		/* Build Pascal string: length byte + raw bytes. */
		bigstring bsseg;
		bsseg[0] = (unsigned char)rawlen;
		memcpy(&bsseg[1], decoded, rawlen);

		hdlhashnode hnode = nil;
		if (!hashtablelookupnode(curtable, bsseg, &hnode) || hnode == nil) {
			/* Not found at this level -- genuine orphan. */
			found = 0;
			goto done;
		}

		if (i == nseg - 1) {
			/* Reached the leaf and found a node -- node exists. */
			found = 1;
			goto done;
		}

		/*
		 * Intermediate segment: descend into the table. The node must be an
		 * in-memory EXTERNAL TABLE variable (id == idtableprocessor).
		 *
		 * P1 #1 -- type guard: if the existing node is an external but is NOT
		 * a table (e.g. it is a script, outline, menu, or picture), the path
		 * component collides with a real non-table object. We must NOT treat
		 * the leaf as an orphan and attempt to create it -- doing so would
		 * cause create_orphan_node to call langsuretablevalue on this slot,
		 * which silently overwrites the existing script/outline with an empty
		 * table (data loss). Treat the path as "present" (found=1) so the
		 * entire orphan is skipped.
		 */
		tyvaluerecord val = (**hnode).val;
		if (val.valuetype != externalvaluetype || val.data.externalvalue == NULL) {
			found = 0;
			goto done;
		}
		hdlexternalvariable hv = (hdlexternalvariable) val.data.externalvalue;

		if (!(**hv).flinmemory || (**hv).variabledata == 0) {
			/*
			 * Intermediate node is not in memory. Post-full-materialization
			 * this should not happen for a real ODB node, but if it does we
			 * conservatively treat the leaf as present (don't shadow a real
			 * lazy node we couldn't descend into).
			 */
			found = 1;
			goto done;
		}

		if ((**hv).id != idtableprocessor) {
			/*
			 * The intermediate slot exists but is a non-table external (script,
			 * outline, menu, picture, etc.). A .ut path cannot thread through a
			 * non-table node. Treat as present so create_orphan_node is not
			 * called -- calling it would clobber the existing non-table object.
			 */
			log_warn(LOG_COMP_STARTUP,
			         "ut-scan: intermediate segment '%.64s' is a non-table external (id=%u), skipping orphan",
			         decoded, (unsigned)(**hv).id);
			found = 1;
			goto done;
		}

		curtable = (hdlhashtable)(Handle)(uintptr_t)(**hv).variabledata;
	}

done:
	free(buf);
	return found;
}


/* -------------------------------------------------------------------------
 * Internal: create the intermediate table chain and leaf script for one orphan.
 *
 * encoded_dotted - ENCODED dotted path (the form stored by cli_record_ut_import
 *                  and consumed by cli_redirty_ut_imported_paths / redirty_one_path).
 * fspath         - absolute filesystem path to the .ut file (for logging only
 *                  when prefd >= 0; used to open the file when prefd == -1).
 * prefd          - if >= 0, a pre-opened fd for the .ut file (opened via openat
 *                  on the pinned parent dir fd; ownership is transferred to this
 *                  function). If -1, the file is opened by path via read_ut_file.
 *
 * GIL: must be called with the GIL held. All kernel primitives used here
 * (langsuretablevalue, hashtableassign, langexternalnewvalue, etc.) require
 * exclusive GIL ownership, matching the boot-scan and repl.syncScan contexts.
 *
 * Returns 1 on success, 0 on failure.
 * ------------------------------------------------------------------------- */
static int create_orphan_node(const char *encoded_dotted, const char *fspath, int prefd) {
	/*
	 * prefd (when >= 0) is owned by this function: it must be closed on EVERY
	 * exit path. read_ut_file_fd consumes it (closes it) on the success path;
	 * for all earlier bails we close it here. Centralize via the close_prefd
	 * label so no early return leaks the fd. ut_bytes (if allocated) is freed
	 * before reaching close_prefd on every path, so this label only owns prefd.
	 */
	int ok = 0; /* declared before any goto so close_prefd can return it */

	/* Split on dots to get segment list. */
	char *buf = strdup(encoded_dotted);
	if (buf == NULL)
		goto close_prefd;

	const char *segs[UT_SCAN_MAX_SEGMENTS];
	int nseg = split_dotted(buf, segs, UT_SCAN_MAX_SEGMENTS);
	if (nseg <= 0) {
		free(buf);
		goto close_prefd;
	}

	/*
	 * P1 #2 -- pre-flight segment validation: decode and validate ALL
	 * segments before making any ODB mutation. This ensures we never create
	 * partial intermediate tables that would be orphaned if a later segment
	 * (e.g. the leaf) fails validation. A crafted %00 in any segment would
	 * cause strlen to stop early, producing a wrong-length Pascal key or a
	 * collision with an existing node.
	 */
	for (int i = 0; i < nseg; i++) {
		char decoded_check[256];
		if (!ut_pct_decode_segment(segs[i], decoded_check, sizeof(decoded_check))) {
			free(buf);
			return 0; /* malformed %XX */
		}
		size_t rawlen_check = strlen(decoded_check);
		int expected_check = pct_decoded_byte_count(segs[i]);
		if (expected_check < 0 || (size_t)expected_check != rawlen_check || rawlen_check == 0) {
			log_warn(LOG_COMP_STARTUP,
			         "ut-scan: create: pre-flight rejected segment (encoded: '%.64s'), aborting orphan",
			         segs[i]);
			free(buf);
			return 0;
		}
		if (rawlen_check > 255) {
			free(buf);
			return 0;
		}
	}

	/*
	 * Read + decanonicalize + compile-check BEFORE any ODB mutation (unit
	 * 1.5): a rejected .ut must leave no partially-created table chain
	 * behind, and a .ut that does not compile must never become an ODB
	 * script node -- it would sit there as a silently non-compiling verb
	 * (the same failure class as the broken-.ut import).
	 */
	unsigned char *decan_bytes = NULL;
	size_t decan_len = 0;
	{
		unsigned char *ut_bytes = NULL;
		size_t ut_len = 0;
		int read_ok;
		/* Use the pre-opened fd when available (openat path from
		 * scan_dir_fd); fall back to open-by-path when prefd == -1. */
		if (prefd >= 0) {
			read_ok = read_ut_file_fd(prefd, fspath, &ut_bytes, &ut_len);
			prefd = -1; /* read_ut_file_fd took ownership and closed it */
		} else {
			read_ok = read_ut_file(fspath, &ut_bytes, &ut_len);
		}
		if (!read_ok) {
			log_warn(LOG_COMP_STARTUP,
			         "ut-scan: could not read .ut file: %s", fspath);
			goto done;
		}
		if (!ut_decanonicalize_outline_text(ut_bytes, ut_len,
		                                    &decan_bytes, &decan_len)) {
			log_warn(LOG_COMP_STARTUP,
			         "ut-scan: decanonicalization failed for: %s", fspath);
			free(ut_bytes);
			goto done;
		}
		free(ut_bytes);
	}

	{
		bigstring bscompileerror;
		if (!opverbscriptcompiles(decan_bytes, (long)decan_len,
		                          bscompileerror)) {
			log_error(LOG_COMP_STARTUP,
			          "ut-scan: REJECTED %s ('%s'): the .ut does not compile "
			          "(%s); node not created",
			          fspath, encoded_dotted, PSTR(bscompileerror));
			goto done;
		}
	}

	/*
	 * Walk from roottable, creating missing intermediate tables, until we
	 * reach the parent of the leaf (nseg-1 segments). The leaf itself is
	 * created as a script external. All segment encodings were validated
	 * above, so we only need the type-guard check here.
	 */
	hdlhashtable curtable = roottable;

	for (int i = 0; i < nseg - 1 && curtable != nil; i++) {
		char decoded[256];
		if (!ut_pct_decode_segment(segs[i], decoded, sizeof(decoded)))
			goto done;
		size_t rawlen = strlen(decoded);
		/* rawlen is validated safe by the pre-flight pass above. */

		bigstring bsseg;
		bsseg[0] = (unsigned char)rawlen;
		memcpy(&bsseg[1], decoded, rawlen);

		/*
		 * P1 #1 -- pre-langsuretablevalue type guard: if a node already exists
		 * at this intermediate slot and it is a non-table external (script,
		 * outline, menu, picture), langsuretablevalue would silently overwrite
		 * it with a new empty table (data loss). Guard by checking the existing
		 * node's type before calling langsuretablevalue.
		 *
		 * langsuretablevalue is only safe to call when:
		 *   (a) the slot is empty (no existing node), OR
		 *   (b) the existing node is already a table (idtableprocessor).
		 * In all other cases, abort this orphan creation.
		 */
		{
			tyvaluerecord existing_val;
			hdlhashnode existing_hnode = nil;
			if (hashtablelookup(curtable, bsseg, &existing_val, &existing_hnode)) {
				/* Slot occupied: verify it is a table before proceeding. */
				if (existing_val.valuetype == externalvaluetype &&
				    existing_val.data.externalvalue != NULL) {
					hdlexternalvariable hv_exist =
					    (hdlexternalvariable) existing_val.data.externalvalue;
					if ((**hv_exist).id != idtableprocessor) {
						/* Non-table external occupies an intermediate slot -- do NOT clobber. */
						log_warn(LOG_COMP_STARTUP,
						         "ut-scan: create: intermediate '%.64s' is non-table (id=%u), aborting orphan",
						         decoded, (unsigned)(**hv_exist).id);
						goto done;
					}
				} else if (existing_val.valuetype != externalvaluetype) {
					/* Slot holds a non-external value (scalar, etc.) -- cannot descend. */
					log_warn(LOG_COMP_STARTUP,
					         "ut-scan: create: intermediate '%.64s' is a non-external value, aborting orphan",
					         decoded);
					goto done;
				}
			}
		}

		hdlhashtable nexttable = nil;
		/*
		 * langsuretablevalue: if the table already exists, return it; if not,
		 * create a new empty table and assign it. The pre-guard above ensures
		 * we only reach this call when the slot is empty or already a table.
		 */
		if (!langsuretablevalue(curtable, bsseg, &nexttable) || nexttable == nil)
			goto done;
		curtable = nexttable;
	}

	/* Now curtable is the parent table for the leaf. */
	{
		const char *leaf_enc = segs[nseg - 1];
		char leaf_raw[256];
		if (!ut_pct_decode_segment(leaf_enc, leaf_raw, sizeof(leaf_raw)))
			goto done;
		size_t leaflen = strlen(leaf_raw);
		/* leaflen is validated safe (non-zero, no embedded NUL, <= 255) by the
		 * pre-flight pass above. No additional check needed here. */
		if (leaflen > 255)
			goto done;

		bigstring bsleaf;
		bsleaf[0] = (unsigned char)leaflen;
		memcpy(&bsleaf[1], leaf_raw, leaflen);

		/*
		 * Build a kernel Handle from the decanonicalized bytes (read,
		 * decanonicalized, and compile-checked above, before any ODB
		 * mutation) so optexttooutline can consume them. optexttooutline
		 * copies internally; we dispose the handle after the call.
		 */
		Handle htext = nil;
		if (!newhandle((long)decan_len, &htext))
			goto done;
		moveleft(decan_bytes, *htext, (long)decan_len);
		free(decan_bytes);
		decan_bytes = NULL;

		/*
		 * Create a new script external value, convert to outline, install the
		 * source text, then assign to the parent table.
		 *
		 * Mirrors opgetsourceverb (opverbs.c ~2688-2708) and the import hook
		 * (opverbs.c ut_import_hook ~846-852).
		 */
		tyvaluerecord vscript;
		if (!langexternalnewvalue(idscriptprocessor, nil, &vscript)) {
			disposehandle(htext);
			goto done;
		}

		hdloutlinerecord ho = nil;
		opvaltoscript(vscript, &ho);
		if (ho == nil) {
			disposevaluerecord(vscript, true);
			disposehandle(htext);
			goto done;
		}

		hdlheadrecord hsummit = nil;
		if (!optexttooutline(ho, htext, &hsummit)) {
			disposehandle(htext);
			disposevaluerecord(vscript, true);
			goto done;
		}
		disposehandle(htext);

		opsetsummit(ho, hsummit);
		opsetctexpanded(ho);

		if (!hashtableassign(curtable, bsleaf, vscript)) {
			disposevaluerecord(vscript, true);
			goto done;
		}

		ok = 1;
	}

done:
	free(decan_bytes);
	free(buf);
close_prefd:
	/* Close prefd if it was not consumed by read_ut_file_fd (every early
	 * bail before the read reaches here with prefd still owned). */
	if (prefd >= 0)
		close(prefd);
	return ok;
}


/* -------------------------------------------------------------------------
 * Internal: recursive directory walk.
 *
 * For each regular .ut file found:
 *   1. Reverse-map its fs path to the ENCODED dotted ODB path.
 *   2. Check existence via structural hashtable walk (EFP-immune).
 *   3. If absent, create the intermediate table chain + leaf script.
 *   4. Record the created path (only when record_for_redirty is non-zero,
 *      i.e. the boot path) so the redirty pass persists it on save.
 *      Post-boot (repl.syncScan) callers pass 0: the dirty bits set by
 *      hashtableassign/langsuretablevalue survive naturally to the next
 *      save without the record/redirty mechanism.
 *
 * count_out is incremented for each node successfully created.
 * depth guards against runaway recursion.
 *
 * P1 #3 / P2 -- TOCTOU-free descent: every child is resolved via openat()
 * on the pinned parent dir fd (dirfd(d)), so no absolute path re-walk can
 * race with a directory entry swap.  Directory children are recursed with
 * the pinned DIR* passed directly into scan_dir_fd -- the caller never
 * closes the dir before recursing.  Regular .ut files are opened via
 * openat() and the pre-opened fd is passed into create_orphan_node (prefd
 * argument), so read_ut_file_fd is used instead of open-by-path.
 * ------------------------------------------------------------------------- */

/* Forward declaration: scan_dir_fd is mutually recursive with itself. */
static void scan_dir_fd(DIR *d, const char *dirpath, const char *sync_dir,
                         int depth, int *count_out, int record_for_redirty);

static void scan_dir_fd(DIR *d, const char *dirpath, const char *sync_dir,
                         int depth, int *count_out, int record_for_redirty) {
	/*
	 * d is owned by this frame; closedir(d) at exit (also closes dirfd(d)).
	 * dirpath is used only for path-string construction (logging + ODB mapping).
	 */
	if (depth > UT_SCAN_MAX_DEPTH) {
		log_warn(LOG_COMP_STARTUP,
		         "ut-scan: max depth %d exceeded at: %s", UT_SCAN_MAX_DEPTH, dirpath);
		closedir(d);
		return;
	}

	int parent_fd = dirfd(d);

	struct dirent *ent;
	while ((ent = readdir(d)) != NULL) {
		const char *name = ent->d_name;

		/* Skip "." and "..". */
		if (name[0] == '.' && (name[1] == '\0' || (name[1] == '.' && name[2] == '\0')))
			continue;

		/* Build full child path string (for logging and ODB path mapping only --
		 * actual file opens use openat on parent_fd, not this path string). */
		char childpath[4096];
		int n = snprintf(childpath, sizeof(childpath), "%s/%s", dirpath, name);
		if (n < 0 || n >= (int)sizeof(childpath)) {
			log_warn(LOG_COMP_STARTUP,
			         "ut-scan: path too long, skipping: %s/%s", dirpath, name);
			continue;
		}

		/*
		 * Classify the child by opening it relative to the pinned parent fd
		 * via openat().  O_NOFOLLOW refuses symlinks; O_DIRECTORY succeeds
		 * only for directories; O_NONBLOCK avoids blocking on a FIFO
		 * masquerading as a directory (macOS requirement).  This resolves the
		 * name relative to the kernel-pinned parent inode, not via the path
		 * string -- no TOCTOU window.
		 */
		int cfd = openat(parent_fd, name,
		                 O_RDONLY | O_NOFOLLOW | O_DIRECTORY | O_NONBLOCK);
		if (cfd >= 0) {
			/* Verify via fstat -- confirms S_ISDIR on the pinned inode. */
			struct stat st;
			if (fstat(cfd, &st) == 0 && S_ISDIR(st.st_mode)) {
				DIR *cd = fdopendir(cfd); /* takes ownership of cfd */
				if (cd != NULL) {
					/* Pass the pinned DIR* directly into the recursive frame.
					 * Do NOT closedir(cd) here: scan_dir_fd owns it and will
					 * close it on exit.  This eliminates the TOCTOU window
					 * between closedir and a re-resolving opendir(childpath). */
					scan_dir_fd(cd, childpath, sync_dir, depth + 1,
					            count_out, record_for_redirty);
					continue;
				}
			}
			close(cfd);
			continue;
		}

		/*
		 * openat() with O_DIRECTORY failed -- child is not a directory.
		 * It may be a regular file, a symlink, or another special type.
		 * Symlinks are silently skipped: openat() with O_NOFOLLOW returns
		 * ELOOP for symlinks, and read_ut_file_fd also received the fd from
		 * an O_NOFOLLOW openat, so symlink pivot is refused at both layers.
		 * Only process potential .ut regular files below.
		 */
		size_t namelen = strlen(name);
		if (namelen < 3 || strcmp(name + namelen - 3, ".ut") != 0)
			continue;

		/*
		 * Reverse-map this .ut fs path to its ENCODED dotted ODB path.
		 * sync_dir is the effective sync base (sync_dir/root_basename),
		 * pre-computed by the caller.  childpath is the path string used
		 * for mapping; the actual file read uses a pre-opened fd below.
		 */
		char encoded_dotted[2048];
		if (!ut_fs_path_to_odb(childpath, sync_dir, encoded_dotted,
		                       sizeof(encoded_dotted))) {
			log_warn(LOG_COMP_STARTUP,
			         "ut-scan: could not map to ODB path (skipping): %s", childpath);
			continue;
		}

		/* Structural EFP-immune existence check. */
		if (node_exists_in_memory(encoded_dotted))
			continue;

		/* Open the .ut file relative to the pinned parent fd via openat --
		 * no re-resolution of the absolute path string. */
		int ffd = openat(parent_fd, name, O_RDONLY | O_NOFOLLOW | O_NONBLOCK);
		if (ffd < 0)
			continue; /* vanished or became a symlink after readdir -- skip */

		/* Genuine orphan: create the table chain + leaf.
		 * Pass ffd as prefd so create_orphan_node uses read_ut_file_fd
		 * (ffd ownership transferred; create_orphan_node closes it). */
		log_info(LOG_COMP_STARTUP,
		         "ut-scan: creating orphan node: %s", encoded_dotted);
		if (create_orphan_node(encoded_dotted, childpath, ffd)) {
			if (record_for_redirty) {
				/*
				 * Boot path: record with the ENCODED dotted form so
				 * cli_redirty_ut_imported_paths can re-dirty the node
				 * after clear_post_hydration_dirty_flags wiped it.
				 * cli_record_ut_import/redirty_one_path split on '.'
				 * and call ut_pct_decode_segment per segment, so the
				 * ENCODED form is the correct contract here.
				 */
				cli_record_ut_import(encoded_dotted);
			}
			(*count_out)++;
		} else {
			log_warn(LOG_COMP_STARTUP,
			         "ut-scan: failed to create node for: %s", childpath);
		}
	}

	closedir(d);
}

static void scan_dir_recursive(const char *dirpath, const char *sync_dir,
                                int depth, int *count_out, int record_for_redirty) {
	if (depth > UT_SCAN_MAX_DEPTH) {
		log_warn(LOG_COMP_STARTUP,
		         "ut-scan: max depth %d exceeded at: %s", UT_SCAN_MAX_DEPTH, dirpath);
		return;
	}

	/*
	 * Open the directory with O_NOFOLLOW|O_DIRECTORY|O_NONBLOCK so the root
	 * of the recursion is also pinned to a kernel inode.  The top-level
	 * dirpath is operator-supplied (--ut-sync-dir), not an attacker-controlled
	 * subtree path, so it does not strictly need openat hardening -- but
	 * pinning it here is cheap and gives consistent posture throughout.
	 */
	int fd = open(dirpath, O_RDONLY | O_NOFOLLOW | O_DIRECTORY | O_NONBLOCK);
	if (fd < 0)
		return;

	struct stat st;
	if (fstat(fd, &st) != 0 || !S_ISDIR(st.st_mode)) {
		close(fd);
		return;
	}

	DIR *d = fdopendir(fd); /* takes ownership of fd */
	if (d == NULL) {
		close(fd);
		return;
	}

	/* Delegate to the fd-pinned recursive worker.  scan_dir_fd owns d. */
	scan_dir_fd(d, dirpath, sync_dir, depth, count_out, record_for_redirty);
}


/* -------------------------------------------------------------------------
 * Public entry point.
 *
 * record_for_redirty controls whether newly created nodes are recorded via
 * cli_record_ut_import() for the post-boot redirty pass:
 *
 *   1 (boot path, main.c): boot scan runs AFTER clear_post_hydration_dirty_flags
 *     wipes all dirty bits, so created nodes must be re-dirtied explicitly via
 *     cli_record_ut_import + cli_redirty_ut_imported_paths to survive to save.
 *
 *   0 (post-boot, repl.syncScan): no clear pass runs between the scan and the
 *     next save, so the dirty bits set by hashtableassign/langsuretablevalue
 *     inside create_orphan_node survive naturally.  Recording the paths is both
 *     unnecessary and a memory leak (the recorded list is consumed only once, at
 *     boot, and post-boot accumulations are never freed).
 * ------------------------------------------------------------------------- */
int ut_sync_scan_and_create(const char *sync_dir, const char *root_basename,
                             int record_for_redirty) {
	if (sync_dir == NULL || root_basename == NULL)
		return -1;

	/*
	 * Build the effective sync base = sync_dir + "/" + root_basename.
	 * This mirrors what the import hook (opverbs.c ut_import_hook) builds as
	 * sync_base and passes as sync_dir to ut_odb_path_to_fs/ut_fs_path_to_odb.
	 */
	char scan_base[4096];
	int nb;
	if (root_basename[0] != '\0')
		nb = snprintf(scan_base, sizeof(scan_base), "%s/%s", sync_dir, root_basename);
	else
		nb = snprintf(scan_base, sizeof(scan_base), "%s", sync_dir);
	if (nb < 0 || nb >= (int)sizeof(scan_base)) {
		log_warn(LOG_COMP_STARTUP,
		         "ut-scan: sync base path too long (sync_dir=%s root=%s), aborting scan",
		         sync_dir, root_basename);
		return -1;
	}

	/* Check the scan base exists; if not, nothing to do (not an error). */
	struct stat st;
	if (stat(scan_base, &st) != 0 || !S_ISDIR(st.st_mode)) {
		/* No sync tree yet -- not an error, just nothing to import. */
		return 0;
	}

	int count = 0;
	scan_dir_recursive(scan_base, scan_base, 0, &count, record_for_redirty);

	if (count > 0) {
		log_info(LOG_COMP_STARTUP,
		         "ut-scan: %s scan complete, created %d orphan node(s)",
		         record_for_redirty ? "boot" : "on-demand", count);
	}

	return count;
}
