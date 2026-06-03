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
 * Matches UT_IMPORT_SIZE_CAP in ut_sync.c (1 MB).
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
 * Internal: read an entire .ut file into a malloc'd buffer.
 *
 * Returns 1 on success (*out is malloc'd, *outlen is the byte count).
 * Returns 0 on any error (*out is NULL, *outlen is 0).
 * Uses O_NOFOLLOW to refuse symlinks (matches the import hook's posture).
 * ------------------------------------------------------------------------- */
static int read_ut_file(const char *fspath, unsigned char **out, size_t *outlen) {
	*out = NULL;
	*outlen = 0;

	int fd = open(fspath, O_RDONLY | O_NOFOLLOW);
	if (fd < 0)
		return 0;

	struct stat st;
	if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) || st.st_size < 0) {
		close(fd);
		return 0;
	}
	size_t sz = (size_t)st.st_size;
	if (sz > UT_SCAN_SIZE_CAP) {
		log_warn(LOG_COMP_STARTUP,
		         "ut-scan: skipping oversized .ut (%zu bytes): %s", sz, fspath);
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
		 * in-memory table external variable. If it isn't (e.g. it's some other
		 * type), we can't descend and treat the leaf as absent.
		 */
		tyvaluerecord val = (**hnode).val;
		if (val.valuetype != externalvaluetype || val.data.externalvalue == NULL) {
			found = 0;
			goto done;
		}
		hdlexternalvariable hv = (hdlexternalvariable) val.data.externalvalue;
		if (!(**hv).flinmemory || (**hv).variabledata == 0) {
			/*
			 * Intermediate table is not in memory. Post-full-materialization
			 * this should not happen for a real ODB node, but if it does we
			 * conservatively treat the leaf as present (don't shadow a real
			 * lazy node we couldn't descend into).
			 */
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
 * fspath         - absolute filesystem path to the .ut file.
 *
 * Returns 1 on success, 0 on failure.
 * ------------------------------------------------------------------------- */
static int create_orphan_node(const char *encoded_dotted, const char *fspath) {
	/* Split on dots to get segment list. */
	char *buf = strdup(encoded_dotted);
	if (buf == NULL)
		return 0;

	const char *segs[UT_SCAN_MAX_SEGMENTS];
	int nseg = split_dotted(buf, segs, UT_SCAN_MAX_SEGMENTS);
	if (nseg <= 0) {
		free(buf);
		return 0;
	}

	/*
	 * Walk from roottable, creating missing intermediate tables, until we
	 * reach the parent of the leaf (nseg-1 segments). The leaf itself is
	 * created as a script external.
	 */
	hdlhashtable curtable = roottable;
	int ok = 0;

	for (int i = 0; i < nseg - 1 && curtable != nil; i++) {
		char decoded[256];
		if (!ut_pct_decode_segment(segs[i], decoded, sizeof(decoded)))
			goto done;
		size_t rawlen = strlen(decoded);
		if (rawlen > 255)
			goto done;

		bigstring bsseg;
		bsseg[0] = (unsigned char)rawlen;
		memcpy(&bsseg[1], decoded, rawlen);

		hdlhashtable nexttable = nil;
		/*
		 * langsuretablevalue: if the table already exists, return it; if not,
		 * create a new empty table and assign it. Safe to call when the node
		 * is already present (idempotent for the already-exists case).
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
		if (leaflen > 255)
			goto done;

		bigstring bsleaf;
		bsleaf[0] = (unsigned char)leaflen;
		memcpy(&bsleaf[1], leaf_raw, leaflen);

		/* Read the .ut file bytes. */
		unsigned char *ut_bytes = NULL;
		size_t ut_len = 0;
		if (!read_ut_file(fspath, &ut_bytes, &ut_len)) {
			log_warn(LOG_COMP_STARTUP,
			         "ut-scan: could not read .ut file: %s", fspath);
			goto done;
		}

		/*
		 * Decanonicalize the .ut bytes (UTF-8/LF/"//") back to the kernel's
		 * in-memory form (MacRoman/CR/0xC7) so optexttooutline can parse them.
		 */
		unsigned char *decan_bytes = NULL;
		size_t decan_len = 0;
		if (!ut_decanonicalize_outline_text(ut_bytes, ut_len, &decan_bytes, &decan_len)) {
			log_warn(LOG_COMP_STARTUP,
			         "ut-scan: decanonicalization failed for: %s", fspath);
			free(ut_bytes);
			goto done;
		}
		free(ut_bytes);

		/*
		 * Build a kernel Handle from the decanonicalized bytes so
		 * optexttooutline can consume them. optexttooutline copies internally;
		 * we dispose the handle after the call.
		 */
		Handle htext = nil;
		if (!newhandle((long)decan_len, &htext)) {
			free(decan_bytes);
			goto done;
		}
		moveleft(decan_bytes, *htext, (long)decan_len);
		free(decan_bytes);

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
	free(buf);
	return ok;
}


/* -------------------------------------------------------------------------
 * Internal: recursive directory walk.
 *
 * For each regular .ut file found:
 *   1. Reverse-map its fs path to the ENCODED dotted ODB path.
 *   2. Check existence via structural hashtable walk (EFP-immune).
 *   3. If absent, create the intermediate table chain + leaf script.
 *   4. Record the created path so the redirty pass persists it.
 *
 * count_out is incremented for each node successfully created.
 * depth guards against runaway recursion.
 * ------------------------------------------------------------------------- */
static void scan_dir_recursive(const char *dirpath, const char *sync_dir,
                                int depth, int *count_out) {
	if (depth > UT_SCAN_MAX_DEPTH) {
		log_warn(LOG_COMP_STARTUP,
		         "ut-scan: max depth %d exceeded at: %s", UT_SCAN_MAX_DEPTH, dirpath);
		return;
	}

	DIR *d = opendir(dirpath);
	if (d == NULL)
		return;

	struct dirent *ent;
	while ((ent = readdir(d)) != NULL) {
		const char *name = ent->d_name;

		/* Skip "." and "..". */
		if (name[0] == '.' && (name[1] == '\0' || (name[1] == '.' && name[2] == '\0')))
			continue;

		/* Build full child path. */
		char childpath[4096];
		int n = snprintf(childpath, sizeof(childpath), "%s/%s", dirpath, name);
		if (n < 0 || n >= (int)sizeof(childpath)) {
			log_warn(LOG_COMP_STARTUP,
			         "ut-scan: path too long, skipping: %s/%s", dirpath, name);
			continue;
		}

		/* lstat to classify without following symlinks. */
		struct stat st;
		if (lstat(childpath, &st) != 0)
			continue;

		/* Skip symlinks -- defense against symlink escape. */
		if (S_ISLNK(st.st_mode))
			continue;

		if (S_ISDIR(st.st_mode)) {
			scan_dir_recursive(childpath, sync_dir, depth + 1, count_out);
		} else if (S_ISREG(st.st_mode)) {
			size_t namelen = strlen(name);
			if (namelen < 3 || strcmp(name + namelen - 3, ".ut") != 0)
				continue;

			/*
			 * Reverse-map this .ut fs path to its ENCODED dotted ODB path.
			 * sync_dir here is the effective sync base (sync_dir/root_basename),
			 * which was pre-computed by the caller.
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

			/* Genuine orphan: create the table chain + leaf. */
			log_info(LOG_COMP_STARTUP,
			         "ut-scan: creating orphan node: %s", encoded_dotted);
			if (create_orphan_node(encoded_dotted, childpath)) {
				/*
				 * Record with the ENCODED dotted form. cli_record_ut_import feeds
				 * cli_redirty_ut_imported_paths which calls redirty_one_path. That
				 * function (main.c ~237) splits on '.' and decodes each segment with
				 * ut_pct_decode_segment before building the Pascal key for
				 * hashtablelookup. So the ENCODED form is the correct contract for
				 * cli_record_ut_import. Passing the raw form would break the decode
				 * step for paths that contain '%' or other encoded characters.
				 */
				cli_record_ut_import(encoded_dotted);
				(*count_out)++;
			} else {
				log_warn(LOG_COMP_STARTUP,
				         "ut-scan: failed to create node for: %s", childpath);
			}
		}
	}

	closedir(d);
}


/* -------------------------------------------------------------------------
 * Public entry point.
 * ------------------------------------------------------------------------- */
int ut_sync_scan_and_create(const char *sync_dir, const char *root_basename) {
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
	scan_dir_recursive(scan_base, scan_base, 0, &count);

	if (count > 0) {
		log_info(LOG_COMP_STARTUP,
		         "ut-scan: boot scan complete, created %d orphan node(s)", count);
	}

	return count;
}
