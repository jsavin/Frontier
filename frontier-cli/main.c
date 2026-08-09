/*
    Frontier CLI - Command Line Interface for UserTalk Script Execution
    Phase 1: CLI-Based UserTalk Invocation Implementation

    SPDX-License-Identifier: MIT

    Copyright (c) 1992-2004 UserLand Software, Inc.
    Copyright (c) 2025-2026 Frontier contributors

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
*/

/*
 * main.c - Entry point for frontier-cli, handles argument parsing and runtime initialization
 *
 * This file orchestrates the CLI startup sequence: parsing command-line arguments,
 * initializing the Frontier runtime, loading the system root database, and dispatching
 * to either script execution mode or the interactive REPL.
 *
 * See docs/CLI_USAGE_GUIDE.md for usage documentation.
 */

// 2025-10-27 Codex: Added diagnostics around system table hydration to surface missing subtables.
// 2025-10-27 Codex: Stop recreating system tables during headless load and follow Cancoon root pointer so persisted tables stay wired.

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <stdint.h>
#include <errno.h>
#include <pthread.h>
#include <time.h>

#ifdef __APPLE__
#include <mach-o/dyld.h>  /* for _NSGetExecutablePath */
#endif

// Frontier headers
#include "../Common/headers/frontier.h"
#include "../Common/headers/logging.h"
#include "../Common/headers/lang.h"
#include "../Common/headers/memory.h"
#include "../Common/headers/strings.h"
#include "../Common/headers/tablestructure.h"
#include "../Common/headers/tableinternal.h"
#include "../Common/headers/shell_api.h"
#include "../Common/headers/db.h"
#include "../Common/headers/file.h"
#include "../Common/headers/db_format.h"
#include "../Common/headers/tableverbs.h"
#include "../Common/headers/langexternal.h"
#include "../Common/headers/opverbs.h"
#include "../Common/headers/stringdefs.h"
#include "../Common/headers/scripts.h"
#include "../Common/headers/db_format.h"
#include "../Common/headers/menudata_headless.h"
#include "../Common/headers/dbinternal.h"
#include "../Common/headers/byteorder.h"
#include "../Common/headers/threadregistry.h"
#include "../Common/headers/threads.h"

// Portable headers
#include "../portable/file_working_dir.h"
#include "../portable/fileverbs_portable.h"

// CLI-specific headers
#include "ut_sync.h"
#include "ut_scan.h"
#include "cli_parser.h"
#include "cli_executor.h"
#include "cli_utils.h"
#include "../Common/headers/langinternal.h"
#include "repl.h"
#include "repl_verbs.h"
#include "protocol_handler.h"
#include "debug_handler.h"
#include "debugger_tui.h"  /* 2026-06-06 JES Phase B.0 #691 */
#include "boxen_repl.h"    /* 2026-06-08 JES Phase C.0 #691: boxen-native REPL */
#include "ws_server.h"
#include "headless_threading.h"

extern long grabthreadglobals(void);
extern long releasethreadglobals(void);

// Version information
// FRONTIER_CLI_VERSION_STRING is defined at compile time from git tags via Makefile
#ifndef FRONTIER_CLI_VERSION_STRING
#define FRONTIER_CLI_VERSION_STRING "1.0.0-dev"
#endif
#define FRONTIER_CLI_BUILD_DATE __DATE__

// System root filename (used in search path construction)
#define SYSTEM_ROOT_FILENAME "Frontier.root"

// Maximum path length accepted by --migrate. The migration stack runs each
// path through copyctopstring, which clips Pascal strings to 255 bytes (1
// length byte + 255 payload). Reject earlier paths fast at argv-parse time
// rather than letting them surface as confusing downstream errors. The
// deep-stack defense-in-depth checks in db_format.c / dbverbs.c catch
// callers that bypass the CLI (db.open() auto-migration, direct C API).
#define MIGRATE_MAX_PATH_BYTES 255

// System root search paths (for auto-discovery)
#define MAX_SEARCH_PATHS 5

// Global variables
static cli_options_t g_cli_options = {0};
static boolean g_initialized = false;
static boolean g_system_root_loaded = false;
static hdlfilenum g_system_root_fnum = 0;
static hdldatabaserecord g_previous_database = nil;
static char g_system_root_path[CLI_MAX_PATH_LENGTH + 1] = {0};

/* Effective read-only state for the loaded system root.
 *
 * Computed once from g_cli_options at hydrate time so consumers (filemenu.save,
 * shutdown save path) can consult a single boolean without re-deriving the
 * policy each time. See cli_compute_effective_read_only() for the rules. */
static boolean g_system_root_read_only = false;

/*
 * Accessor functions for system root state.
 * Used by frontier.getFilePath() verb to access CLI state without exposing globals.
 *
 * Thread Safety: These accessors are NOT thread-safe. They are designed for use
 * during single-threaded startup only. The path is set before the loaded flag
 * (see hydrate_system_root_database) to avoid partial reads, but there is no
 * mutex protection. If multi-threading is added, these will need synchronization.
 */
boolean cli_is_system_root_loaded(void) {
	return g_system_root_loaded;
}

const char* cli_get_system_root_path(void) {
	return g_system_root_path;
}

boolean cli_should_skip_startup(void) {
	return g_cli_options.skip_startup;
}

/* Accessor for the --ut-sync-dir value (NULL when feature is off).
 * Called from opverbinmemory import hook via extern declaration. */
const char *cli_get_ut_sync_dir(void) {
	return g_cli_options.ut_sync_dir;
}

/* Accessor for the basename of the loaded system root (e.g. "Frontier.root").
 * Returns a pointer to a static buffer; valid for the process lifetime.
 * Returns "" if no system root is loaded or the path is empty.
 * Called from opverbinmemory import hook via extern declaration. */
const char *cli_get_system_root_basename(void) {
	static char bn_buf[CLI_MAX_PATH_LENGTH + 1];
	if (g_system_root_path[0] == '\0') {
		bn_buf[0] = '\0';
		return bn_buf;
	}
	/* Find last '/' and return everything after it; no '/' -> whole string. */
	const char *slash = strrchr(g_system_root_path, '/');
	const char *base = (slash != NULL) ? (slash + 1) : g_system_root_path;
	snprintf(bn_buf, sizeof(bn_buf), "%s", base);
	return bn_buf;
}

/*
 * .ut import boot-time re-dirty infrastructure.
 *
 * During bulk hydrate, opverbinmemory calls cli_record_ut_import for every
 * script it imports from a .ut file. After the hydrate walk completes,
 * clear_post_hydration_dirty_flags() clears ALL dirty bits in the tree
 * (including the ones just set by the import hook). hydrate_system_root_database
 * then calls cli_redirty_ut_imported_paths() to re-mark those scripts dirty so
 * the next tablesavesystemtable persists the imported content to the ODB.
 *
 * For post-boot lazy loads (opverbinmemory called outside bulk hydrate), the
 * clear pass never runs, so the dirty flag set by the hook stands naturally.
 *
 * Thread safety: cli_record_ut_import is only called from opverbinmemory during
 * the bulk-hydrate walk on the main thread while the GIL is held. No concurrent
 * access possible.
 *
 * The list grows dynamically: a fixed cap would silently drop re-dirty records
 * past the limit, which means those imported scripts would NOT be persisted on
 * the next save (the imported content would appear to load then vanish on the
 * following reload). A corpus can legitimately have thousands of scripts, so the
 * list must accommodate as many imports as actually occur.
 */
static char **g_ut_imported_paths = NULL;
static int g_ut_imported_count = 0;
static int g_ut_imported_cap = 0;

/* Called from opverbinmemory (via extern) when a .ut file is imported.
 * Records the dotted path for the re-dirty pass after clear_post_hydration_dirty_flags. */
void cli_record_ut_import(const char *dotted_path) {
	if (dotted_path == NULL || dotted_path[0] == '\0')
		return;
	if (g_ut_imported_count >= g_ut_imported_cap) {
		int newcap = (g_ut_imported_cap == 0) ? 256 : g_ut_imported_cap * 2;
		char **grown = (char **) realloc(g_ut_imported_paths,
		                                 (size_t) newcap * sizeof(char *));
		if (grown == NULL) {
			log_warn(LOG_COMP_STARTUP,
			         "ut-sync: import list grow failed (%d entries); "
			         "skipping re-dirty for %s",
			         g_ut_imported_cap, dotted_path);
			return;
		}
		g_ut_imported_paths = grown;
		g_ut_imported_cap = newcap;
	}
	char *copy = strdup(dotted_path);
	if (copy == NULL) {
		log_warn(LOG_COMP_STARTUP,
		         "ut-sync: strdup failed for path %s; re-dirty will be skipped", dotted_path);
		return;
	}
	g_ut_imported_paths[g_ut_imported_count++] = copy;
}

/*
 * Walk a dotted ODB path (e.g. "system.verbs.foo") from roottable and, if the
 * leaf holds a script or outline external variable, call opverbsetdirty(hv, true).
 *
 * Intermediate segments are expected to be table externals; if an intermediate
 * segment is not in-memory yet, the walk stops (the lazy-load dirty flag is not
 * affected by clear_post_hydration_dirty_flags anyway).
 *
 * Returns 1 if the leaf was found and dirtied, 0 otherwise.
 */
static int redirty_one_path(const char *dotted_path) {
	char *buf = strdup(dotted_path);
	if (buf == NULL)
		return 0;

	hdlhashtable cur = roottable;
	char *seg = buf;
	int found = 0;

	while (cur != nil && seg != NULL) {
		while (*seg == '.')
			seg++;
		if (*seg == '\0')
			break;

		char *dot = strchr(seg, '.');
		if (dot != NULL)
			*dot = '\0';

		/* Decode the percent-encoded segment back to its raw ODB key name
		 * before building the Pascal string for hashtablelookup. */
		char decoded_seg[256]; /* max raw Pascal name: 255 bytes + NUL */
		/*
		 * Defense-in-depth NUL guard: reject any segment that contains %00.
		 * ut_pct_decode_segment would decode it to an embedded NUL byte,
		 * causing strlen(decoded_seg) to silently truncate the key and diverge
		 * from the codec contract (ut_sync.h line ~219). This matches the
		 * equivalent rawlen==0 guard in ut_scan.c. %00 has no hex-case variant
		 * (both digits are '0'), so a literal strstr suffices.
		 */
		if (strstr(seg, "%00") != NULL)
			break;
		if (!ut_pct_decode_segment(seg, decoded_seg, sizeof(decoded_seg)))
			break; /* malformed %XX in a path we emitted -- treat as miss */

		/* Build Pascal string for this (now raw) segment. */
		bigstring bsseg;
		size_t seglen = strlen(decoded_seg);
		if (seglen > 255)
			break;
		bsseg[0] = (unsigned char) seglen;
		memcpy(&bsseg[1], decoded_seg, seglen);

		tyvaluerecord val;
		hdlhashnode hnode = nil;
		if (!hashtablelookup(cur, bsseg, &val, &hnode))
			break;

		if (dot == NULL) {
			/* Leaf: dirty it if it is a script or outline external. */
			if (val.valuetype == externalvaluetype && val.data.externalvalue != NULL) {
				hdlexternalvariable hv = (hdlexternalvariable) val.data.externalvalue;
				unsigned short xid = (**hv).id;
				if (xid == idscriptprocessor || xid == idoutlineprocessor) {
					opverbsetdirty(hv, true);
					found = 1;
				}
			}
			break;
		} else {
			/* Intermediate: descend into the table if it is already in memory. */
			if (val.valuetype == externalvaluetype && val.data.externalvalue != NULL) {
				hdlexternalvariable hv_seg = (hdlexternalvariable) val.data.externalvalue;
				/*
				 * P1 #1 defense-in-depth: verify the intermediate node is a table
				 * (idtableprocessor) before casting variabledata to hdlhashtable.
				 * A non-table external (script, outline, menu) has variabledata
				 * pointing to its own record type, not an hdlhashtable. Casting
				 * it would produce wrong-typed memory access.
				 */
				if ((**hv_seg).id != idtableprocessor)
					break;
				/* variabledata is a long; when flinmemory it holds an hdlhashtable cast. */
				if ((**hv_seg).flinmemory && (**hv_seg).variabledata != 0)
					cur = (hdlhashtable)(Handle)(uintptr_t)(**hv_seg).variabledata;
				else
					break;
			} else {
				break;
			}
			seg = dot + 1;
		}
	}

	free(buf);
	return found;
}

/*
 * Re-mark all scripts recorded by cli_record_ut_import as dirty.
 * Called immediately after clear_post_hydration_dirty_flags() so that
 * boot-time .ut imports persist on the next tablesavesystemtable.
 * Frees the recorded list and resets the count.
 */
static void cli_redirty_ut_imported_paths(void) {
	int n = g_ut_imported_count;
	if (n == 0)
		return;
	log_info(LOG_COMP_STARTUP,
	         "ut-sync: re-dirtying %d imported script(s) after hydration clear", n);
	for (int i = 0; i < n; i++) {
		if (g_ut_imported_paths[i] != NULL) {
			if (!redirty_one_path(g_ut_imported_paths[i])) {
				log_warn(LOG_COMP_STARTUP,
				         "ut-sync: could not re-dirty %s (not found or not a script)",
				         g_ut_imported_paths[i]);
			}
			free(g_ut_imported_paths[i]);
			g_ut_imported_paths[i] = NULL;
		}
	}
	free(g_ut_imported_paths);
	g_ut_imported_paths = NULL;
	g_ut_imported_count = 0;
	g_ut_imported_cap = 0;
}

/*
 * Decide whether to open the system root read-only based on parsed CLI flags.
 *
 * Policy:
 *   --lock-opened-roots    -> suppress save-on-exit for loaded-from-disk DBs
 *                             (issue #127). In-memory mutations still
 *                             evaluate; only the disk persist is skipped.
 *                             Newly created roots (file.save / file.saveAs /
 *                             db.compactDatabase) are unaffected -- those
 *                             paths don't go through the on-exit save.
 *   otherwise              -> read-write (legacy Frontier semantics: every
 *                             system root and guest DB is mutable by default,
 *                             including under --protocol).
 */
static boolean cli_compute_effective_read_only(const cli_options_t *opts) {
	if (opts->lock_opened_roots)
		return true;
	return false;
}

/* Returns the effective read-only state for the loaded system root.
 * Used by filemenu.save (and the shutdown save path) to refuse writes
 * that would mutate a database opened for inspection. */
boolean cli_is_system_root_read_only(void) {
	return g_system_root_read_only;
}

/* system.environment.args key names (camelCase from CLI flags). */
#define str_systemRoot		BIGSTRING ("\x0a" "systemRoot")
#define str_skipStartup		BIGSTRING ("\x0b" "skipStartup")
#define str_execute			BIGSTRING ("\x07" "execute")
#define str_output			BIGSTRING ("\x06" "output")
#define str_verbose			BIGSTRING ("\x07" "verbose")
#define str_debug			BIGSTRING ("\x05" "debug")
#define str_outputJson		BIGSTRING ("\x0a" "outputJson")
#define str_batch			BIGSTRING ("\x05" "batch")
#define str_protocol		BIGSTRING ("\x08" "protocol")
#define str_wsPort			BIGSTRING ("\x06" "wsPort")
#define str_log				BIGSTRING ("\x03" "log")
#define str_force			BIGSTRING ("\x05" "force")
#define str_migrate			BIGSTRING ("\x07" "migrate")
#define str_hydrate			BIGSTRING ("\x07" "hydrate")

/*
 * Helper: assign a C string to a hash table entry using a Handle.
 * Handles arbitrarily long strings (unlike copyctopstring which truncates at 255).
 * langassigntextvalue takes ownership of h on success; we only dispose on failure.
 */
static boolean assign_cstring_value (hdlhashtable ht, const bigstring bskey, const char *cstr) {

	long len = (long) strlen (cstr);
	Handle h;

	if (!newhandle (len, &h))
		return (false);

	if (len > 0)
		memcpy (*h, cstr, (size_t) len);

	if (!langassigntextvalue (ht, bskey, h)) {
		disposehandle (h);
		return (false);
	}

	return (true);
} /*assign_cstring_value*/

/*
 * Callback registered with langenvironment_set_args_callback().
 * Populates system.environment.args subtable from g_cli_options.
 * Only flags actually set on the command line appear; absence = not set.
 */
static boolean populate_environment_args (hdlhashtable htargs) {

	const cli_options_t *opts = &g_cli_options;

	/* String fields — only add when non-NULL */

	if (opts->system_root != NULL) {
		if (!assign_cstring_value (htargs, str_systemRoot, opts->system_root))
			return (false);
	}

	if (opts->inline_script != NULL) {
		if (!assign_cstring_value (htargs, str_execute, opts->inline_script))
			return (false);
	}

	if (opts->output_path != NULL) {
		if (!assign_cstring_value (htargs, str_output, opts->output_path))
			return (false);
	}

	if (opts->log_spec != NULL) {
		if (!assign_cstring_value (htargs, str_log, opts->log_spec))
			return (false);
	}

	if (opts->migrate_database != NULL) {
		if (!assign_cstring_value (htargs, str_migrate, opts->migrate_database))
			return (false);
	}

	/* Boolean flags — only add when true */

	if (opts->verbose) {
		if (!langassignbooleanvalue (htargs, str_verbose, true))
			return (false);
	}

	if (opts->debug) {
		if (!langassignbooleanvalue (htargs, str_debug, true))
			return (false);
	}

	if (opts->output_json) {
		if (!langassignbooleanvalue (htargs, str_outputJson, true))
			return (false);
	}

	if (opts->batch_mode) {
		if (!langassignbooleanvalue (htargs, str_batch, true))
			return (false);
	}

	if (opts->protocol_mode) {
		if (!langassignbooleanvalue (htargs, str_protocol, true))
			return (false);
	}

	if (opts->skip_startup) {
		if (!langassignbooleanvalue (htargs, str_skipStartup, true))
			return (false);
	}

	if (opts->force_overwrite) {
		if (!langassignbooleanvalue (htargs, str_force, true))
			return (false);
	}

	if (opts->hydrate_system_root) {
		if (!langassignbooleanvalue (htargs, str_hydrate, true))
			return (false);
	}

	/* Integer fields — only add when non-zero */

	if (opts->ws_port > 0) {
		if (!langassignlongvalue (htargs, str_wsPort, (long) opts->ws_port))
			return (false);
	}

	/* Extra (unknown) flags — these come from the second-pass parser */

	{
		const cli_extra_arg_t *node = opts->extra_args;

		while (node != NULL) {

			bigstring bskey;

			copyctopstring (node->key, bskey);

			if (node->value != NULL) {
				if (!assign_cstring_value (htargs, bskey, node->value))
					return (false);
			}
			else {
				if (!langassignbooleanvalue (htargs, bskey, true))
					return (false);
			}

			node = node->next;
		}
	}

	/* Positional arguments — stored as _1, _2, etc. (1-based index) */

	for (int i = 0; i < opts->positional_count; i++) {

		bigstring bskey;
		char ckey[16];

		snprintf (ckey, sizeof (ckey), "_%d", i + 1);
		copyctopstring (ckey, bskey);

		if (!assign_cstring_value (htargs, bskey, opts->positional_args[i]))
			return (false);
	}

	/* script_file — exposed as "scriptFile" when present */

	if (opts->script_file != NULL) {

		bigstring bskey;
		copyctopstring ("scriptFile", bskey);

		if (!assign_cstring_value (htargs, bskey, opts->script_file))
			return (false);
	}

	return (true);
} /*populate_environment_args*/

// Function prototypes
static void print_usage(const char* program_name);
static void print_version(void);
static boolean initialize_frontier_runtime(void);
static void cleanup_frontier_runtime(void);
static boolean execute_script_mode(void);
static boolean load_system_root_database(const char* path);
static void unload_system_root_database(void);
static void save_system_root_on_exit(void);
static boolean ensure_named_subtable(hdlhashtable parent, const unsigned char *name, hdlhashtable *out, boolean mark_dont_save);
static boolean ensure_named_subtable_ex(hdlhashtable parent, const unsigned char *name, hdlhashtable *out, boolean mark_dont_save, boolean *out_created);
static boolean hydrate_system_root_database(const char* path, boolean read_only);
static void clear_post_hydration_dirty_flags(hdlhashtable ht);
static boolean read_root_table_address(const char *path, dbaddress *adr_out, short *version_out);
static void log_system_subtable_status(const char *phase,
									   hdlhashtable system,
									   hdlhashtable verbs,
									   hdlhashtable builtins,
									   hdlhashtable agents,
									   hdlhashtable paths,
									   hdlhashtable resources,
									   hdlhashtable menubar,
									   hdlhashtable objectmodel);
static int get_system_root_search_paths(char paths[][CLI_MAX_PATH_LENGTH + 1], int max_paths);

/* Main entry point: initializes runtime, loads database, and dispatches to execution mode. */
int main(int argc, char* argv[]) {

	// Initialize logging system (reads FRONTIER_LOG_LEVEL, FRONTIER_LOG_COMPONENT, FRONTIER_LOG_FORMAT env vars)
	log_init();

	// Initialize process-level default working directory
	char cwd_buf[4096];
	const char *cwd = getcwd(cwd_buf, sizeof(cwd_buf));
	if (!cwd) {
		// Fall back to executable directory if getcwd() fails
		char resolved_path[4096];
		if (realpath(argv[0], resolved_path) != NULL) {
			char *last_slash = strrchr(resolved_path, '/');
			if (last_slash) {
				*last_slash = '\0';
				cwd = resolved_path;
			}
		}
	}

	if (cwd) {
		init_default_working_dir(cwd);
	} else {
		// Last resort: use current directory
		init_default_working_dir(".");
	}

	// Initialize file handle cleanup (must be called before any file operations)
	init_file_handle_cleanup();

	// Parse command line arguments
	if (!cli_parse_arguments(argc, argv, &g_cli_options)) {
		log_error(LOG_COMP_GENERAL, "Error: Invalid command line arguments");
		print_usage(argv[0]);
		cli_free_options(&g_cli_options); /* honor parse-failure cleanup contract */
		return 1;
	}

	// Apply --log spec if provided (overrides env var settings)
	if (g_cli_options.log_spec != NULL) {
		log_parse_spec(g_cli_options.log_spec);
	}

	// Handle help and version requests
	if (g_cli_options.show_help) {
		print_usage(argv[0]);
		return 0;
	}

	if (g_cli_options.show_version) {
		print_version();
		return 0;
	}

	/* Handle --migrate mode: migrate database to v7 and exit */
	if (g_cli_options.migrate_database != NULL) {
		boolean migrated = false;
		const char *input = g_cli_options.migrate_database;
		size_t input_len = strlen(input);

		/* Validate path length */
		if (input_len >= CLI_MAX_PATH_LENGTH) {
			fprintf(stderr, "Error: Input path too long\n");
			return 1;
		}

		/* Reject paths > 255 bytes early (see MIGRATE_MAX_PATH_BYTES at top
		 * of file). Fail fast here with a clear message rather than letting
		 * the truncated path surface as a confusing "openfile" or
		 * "pathtofilespec" error downstream. */
		if (input_len > MIGRATE_MAX_PATH_BYTES) {
			fprintf(stderr, "Error: --migrate: path exceeds %d bytes (maximum supported by database format)\n",
			        MIGRATE_MAX_PATH_BYTES);
			return 1;
		}

		/* Check --output target BEFORE migration to avoid side effects on failure */
		if (g_cli_options.output_path != NULL) {
			if (access(g_cli_options.output_path, F_OK) == 0 && !g_cli_options.force_overwrite) {
				fprintf(stderr, "Error: Output file already exists: %s\n", g_cli_options.output_path);
				fprintf(stderr, "Use --force (-f) to overwrite.\n");
				return 1;
			}
		}

		/* Initialize minimal runtime for migration */
		if (!db_format_prepare_runtime()) {
			fprintf(stderr, "Error: Failed to initialize runtime for migration\n");
			return 1;
		}

		if (g_cli_options.output_path != NULL) {
			/* --output mode: write v7 directly to the specified path.
			 * The v6 input file is never modified. */

			/* Check if already v7 before attempting migration.
			 * Use detect_database_format + db_format_mode_apply to handle
			 * endianness correctly, matching the ensure_database_v7 pattern. */
			{
				FILE *fp_check = fopen(input, "rb");
				if (!fp_check) {
					fprintf(stderr, "Error: Cannot open input file: %s\n", input);
					return 1;
				}
				tydatabaserecord hdr;
				boolean hdr_ok = fread(&hdr, sizeof hdr, 1, fp_check) == 1;
				fclose(fp_check);
				if (!hdr_ok || !detect_database_format(&hdr)) {
					fprintf(stderr, "Error: Cannot read database header: %s\n", input);
					return 1;
				}
				db_format_mode detected = {!db_format_is_v6_header(&hdr), false};
				db_format_mode_apply(&detected);
				if (db_format_mode_current().use_64bit_format) {
					printf("Already v7 format: %s\n", input);
					return 0;
				}
			}

			if (!migrate_32bit_to_64bit_to_output(input, g_cli_options.output_path)) {
				fprintf(stderr, "Error: Migration failed for: %s\n", input);
				return 1;
			}
			printf("Migrated: %s -> %s\n", input, g_cli_options.output_path);
		} else {
			/* In-place mode: ensure_database_v7 renames v6 to .v6.root backup
			 * and writes v7 to the original .root path. */
			if (g_cli_options.force_overwrite) {
				printf("Note: --force has no effect in in-place migration mode (no --output specified)\n");
			}
			if (!ensure_database_v7(input, &migrated, NULL, 0)) {
				fprintf(stderr, "Error: Migration failed for: %s\n", input);
				return 1;
			}

			if (!migrated) {
				printf("Already v7 format: %s\n", input);
				return 0;
			}

			{
				char v6_backup[CLI_MAX_PATH_LENGTH + 16];
				db_format_derive_v6_backup_path(input, v6_backup, sizeof(v6_backup));
				printf("Migrated in-place: %s\n", input);
				printf("  v6 backed up to: %s\n", v6_backup);
			}
		}
		return 0;
	}

	/* Set JSON mode early to suppress logs on stderr when JSON output is enabled.
	 * Must be set BEFORE database loading to prevent log messages from appearing in JSON output. */
	cli_set_json_mode(g_cli_options.output_json);
	log_set_suppressed(g_cli_options.output_json);

	/* Set default log level to ERROR for REPL mode (unless user explicitly configured logging)
	 * This reduces startup noise from database warnings and WPText conversion messages.
	 * Script mode keeps default WARN level for better diagnostics.
	 * Respect explicit logging config: FRONTIER_LOG, FRONTIER_LOG_LEVEL, or --log */
	if (getenv("FRONTIER_LOG_LEVEL") == NULL &&
		getenv("FRONTIER_LOG") == NULL &&
		g_cli_options.log_spec == NULL &&
		g_cli_options.script_file == NULL &&
		g_cli_options.inline_script == NULL) {
		log_set_level(LOG_LEVEL_ERROR);
	}

	/* Always initialize runtime and hydrate system root (default path). */
	if (!initialize_frontier_runtime()) {
		log_error(LOG_COMP_GENERAL, "Error: Failed to initialize Frontier runtime");
		return 1;
	}

	/* Initialize interactive mode detection after thread globals are ready.
	 * Note: cli_init_interactive_mode() accesses fl_batch_mode and fl_interactive_detected
	 * which are thread-local via macros defined in processinternal.h. */
	cli_init_interactive_mode(g_cli_options.batch_mode);

	/* Auto-load system root database if not explicitly specified */
	const char *system_root_to_load = g_cli_options.system_root;
	static char found_system_root_path[CLI_MAX_PATH_LENGTH + 1];  /* Static buffer for auto-discovered path */
	if (system_root_to_load == NULL) {
		/* Build search path list and try each location in order */
		char search_paths[MAX_SEARCH_PATHS][CLI_MAX_PATH_LENGTH + 1];
		int num_paths = get_system_root_search_paths(search_paths, MAX_SEARCH_PATHS);

		for (int i = 0; i < num_paths; i++) {
			if (access(search_paths[i], F_OK) == 0) {
				/* Copy to static buffer to avoid dangling pointer when search_paths goes out of scope */
				strncpy(found_system_root_path, search_paths[i], sizeof(found_system_root_path) - 1);
				found_system_root_path[sizeof(found_system_root_path) - 1] = '\0';
				system_root_to_load = found_system_root_path;

				/* Check if this is v6 format (will auto-migrate) */
				const char *ext = strrchr(found_system_root_path, '.');
				boolean is_v6 = (ext != NULL && strcmp(ext, ".root") == 0);

				if (is_v6) {
					log_info(LOG_COMP_STARTUP, "Auto-loading system root: %s (will auto-migrate to v7)", system_root_to_load);
				} else {
					log_info(LOG_COMP_STARTUP, "Auto-loading system root: %s", system_root_to_load);
				}
				break;
			}
		}
		/* If no path exists, continue without system root (headless mode) */
	}

	if (system_root_to_load != NULL) {
		/* Compute effective read-only state once and stash it in a global so
		 * downstream consumers (filemenu.save, shutdown save path) see a
		 * consistent answer regardless of which call edge they live on.
		 * Set BEFORE hydrate so any logging during hydrate reflects the
		 * decision; hydrate reads the parameter, not the global. */
		g_system_root_read_only = cli_compute_effective_read_only(&g_cli_options);
		if (!hydrate_system_root_database(system_root_to_load, g_system_root_read_only)) {
			log_error(LOG_COMP_GENERAL, "Error: Failed to load system root: %s", system_root_to_load);
			cleanup_frontier_runtime();
			return 1;
		}

		/* Run startup scripts AFTER full hydration.
		 * This ensures:
		 * 1. EFP tables are properly linked
		 * 2. system.paths is populated and resolved
		 * 3. Database tables are augmented with EFP implementations
		 * Scripts run by default; use --skip-startup or FRONTIER_HEADLESS_RUN_STARTUP=0 to skip. */
		if (!loadsystemscripts()) {
			log_error(LOG_COMP_GENERAL, "Error: startup scripts failed for: %s", system_root_to_load);
			/* Continue despite startup script errors - they're not fatal */
		}

		/* Note: dirty bits set by startup scripts are intentionally preserved
		 * here. Under the legacy (default) policy those mutations are
		 * user-visible state (e.g., REPL menubar installation persisting
		 * into system.menus.data) and SHOULD save on exit. Ephemeral
		 * consumers that don't want this should pass --lock-opened-roots,
		 * which skips save_system_root_on_exit() via
		 * g_system_root_read_only. */
	}

	// Start WebSocket server if --ws-port was specified
	ws_server_t ws_server;
	ws_server_t *ws_server_ptr = NULL;

	if (g_cli_options.ws_port > 0) {
		if (ws_server_init(&ws_server, g_cli_options.ws_port) == 0) {
			ws_server_ptr = &ws_server;
		} else {
			log_error(LOG_COMP_GENERAL, "Failed to start WebSocket server on port %d", g_cli_options.ws_port);
		}
	}

	// Determine execution mode
	boolean success = false;
	int exit_code = 0;

	/* 2026-06-25 JES C.6 #691: REPL dispatch flip.
	 *
	 * Default `frontier-cli` (no flag) launches the boxen REPL.  `--plain`
	 * is the explicit opt-in to the legacy linenoise REPL (formerly the
	 * default).  `--debug-tui` is a deprecated no-op alias for the new
	 * default; a one-time log_warn surfaces in the scrollback ring so
	 * users notice the change without breaking existing scripts.
	 *
	 * Dispatch priority (mirrors B.0 / C.0 behavior except for the flip
	 * between linenoise-default and boxen-default):
	 *   1. --debug-tui (alias) -> boxen REPL (with deprecation warning).
	 *      Takes priority over batch-mode so `--debug-tui SCRIPT` still
	 *      routes through boxen_repl_main's auto-launch path, matching
	 *      the B.8 contract preserved through Phase C.
	 *   2. --protocol -> NDJSON protocol mode (unchanged).
	 *   3. batch (-e / script file) without any REPL flag -> headless
	 *      execute_script_mode (legacy behavior unchanged).
	 *   4. --plain -> linenoise REPL (explicit legacy opt-in).
	 *   5. default (interactive, no flag) -> boxen REPL. */

	/* 2026-06-25 JES C.6 #691 gate-fix: defense-in-depth.  Validator
	 * rejects all three pairwise REPL-flag combinations
	 * (cli_parser.c:217-239), but if a future caller ever builds
	 * cli_options_t programmatically without re-running
	 * cli_validate_options, dispatch below would silently pick a winner
	 * (tui_mode > protocol_mode > plain_mode).  Assert catches that
	 * drift loudly in debug builds; cheap in release. */
	assert(!(g_cli_options.tui_mode && g_cli_options.protocol_mode));
	assert(!(g_cli_options.plain_mode && g_cli_options.tui_mode));
	assert(!(g_cli_options.plain_mode && g_cli_options.protocol_mode));

	if (g_cli_options.tui_mode) {
		log_warn(LOG_COMP_GENERAL,
		         "--debug-tui is now the default (boxen REPL); the flag is "
		         "a no-op alias and will be removed in a future release.  "
		         "Pass --plain to opt into the legacy linenoise REPL.");
		exit_code = boxen_repl_main(&g_cli_options);
	} else if (g_cli_options.protocol_mode) {
		// NDJSON protocol mode - structured JSON over stdin/stdout
		exit_code = protocol_main(&g_cli_options, ws_server_ptr);
	} else if (g_cli_options.script_file != NULL || g_cli_options.inline_script != NULL) {
		// Batch mode - execute script and exit
		success = execute_script_mode();
		exit_code = success ? 0 : 1;
	} else if (g_cli_options.plain_mode) {
		// Explicit legacy linenoise REPL (--plain).
		exit_code = repl_main(&g_cli_options, ws_server_ptr);
	} else {
		// Default: boxen REPL.
		exit_code = boxen_repl_main(&g_cli_options);
	}

	// Shutdown WebSocket server
	if (ws_server_ptr != NULL) {
		ws_server_shutdown(ws_server_ptr);
	}

	// Cleanup
	cleanup_frontier_runtime();

	return exit_code;
}

/* Builds list of system root search paths for auto-discovery (env var, standard locations, cwd). */
static int get_system_root_search_paths(char paths[][CLI_MAX_PATH_LENGTH + 1], int max_paths) {
	int count = 0;
	char expanded_path[CLI_MAX_PATH_LENGTH + 1];
	char cwd[CLI_MAX_PATH_LENGTH + 1];
	char exe_dir[CLI_MAX_PATH_LENGTH + 1];

	// Get current working directory
	if (getcwd(cwd, sizeof(cwd)) == NULL) {
		log_debug(LOG_COMP_GENERAL, "get_system_root_search_paths: getcwd failed (errno=%d)", errno);
		cwd[0] = '\0';
	}

	// Get executable directory
	exe_dir[0] = '\0';
#ifdef __APPLE__
	{
		char exe_path[CLI_MAX_PATH_LENGTH + 1];
		uint32_t size = sizeof(exe_path);
		if (_NSGetExecutablePath(exe_path, &size) == 0) {
			// Find last slash to get directory
			char *last_slash = strrchr(exe_path, '/');
			if (last_slash != NULL) {
				size_t dir_len = (size_t)(last_slash - exe_path);
				if (dir_len < sizeof(exe_dir)) {
					memcpy(exe_dir, exe_path, dir_len);
					exe_dir[dir_len] = '\0';
				}
			}
		}
	}
#else
	// Linux: read /proc/self/exe symlink
	{
		char exe_path[CLI_MAX_PATH_LENGTH + 1];
		ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
		if (len != -1) {
			exe_path[len] = '\0';
			char *last_slash = strrchr(exe_path, '/');
			if (last_slash != NULL) {
				size_t dir_len = (size_t)(last_slash - exe_path);
				if (dir_len < sizeof(exe_dir)) {
					memcpy(exe_dir, exe_path, dir_len);
					exe_dir[dir_len] = '\0';
				}
			}
		}
	}
#endif

	// 1. Check FRONTIER_ROOT environment variable (highest priority)
	const char *frontier_root_env = getenv("FRONTIER_ROOT");
	if (frontier_root_env != NULL && frontier_root_env[0] != '\0') {
		// Expand ~ if present
		if (frontier_root_env[0] == '~') {
			const char *home = getenv("HOME");
			if (home != NULL) {
				snprintf(expanded_path, sizeof(expanded_path), "%s%s", home, frontier_root_env + 1);
				strncpy(paths[count], expanded_path, CLI_MAX_PATH_LENGTH);
				paths[count][CLI_MAX_PATH_LENGTH] = '\0';
				count++;
			}
		} else {
			strncpy(paths[count], frontier_root_env, CLI_MAX_PATH_LENGTH);
			paths[count][CLI_MAX_PATH_LENGTH] = '\0';
			count++;
		}
	}

	// 2. Current working directory
	if (count < max_paths && cwd[0] != '\0') {
		snprintf(paths[count], CLI_MAX_PATH_LENGTH + 1, "%s/" SYSTEM_ROOT_FILENAME, cwd);
		count++;
	}

	// 3. Executable directory
	if (count < max_paths && exe_dir[0] != '\0') {
		snprintf(paths[count], CLI_MAX_PATH_LENGTH + 1, "%s/" SYSTEM_ROOT_FILENAME, exe_dir);
		count++;
	}

	// 4. User Application Support directory
	const char *home = getenv("HOME");
	if (home != NULL && count < max_paths) {
		snprintf(paths[count], CLI_MAX_PATH_LENGTH + 1,
				 "%s/Library/Application Support/Frontier/" SYSTEM_ROOT_FILENAME, home);
		count++;
	}

	// 5. ~/.frontier/Frontier.root (Linux convention)
	if (home != NULL && count < max_paths) {
		snprintf(paths[count], CLI_MAX_PATH_LENGTH + 1,
				 "%s/.frontier/" SYSTEM_ROOT_FILENAME, home);
		count++;
	}

	return count;
}

/* Reads a big-endian integer of specified length from a byte buffer. */
static uint64_t read_big_endian(const unsigned char *data, size_t length) {
	uint64_t value = 0;
	for (size_t i = 0; i < length; ++i) {
		value = (value << 8) | (uint64_t) data[i];
	}
	return value;
}

/* Locates the root table address in the database by reading the Cancoon view header. */
static boolean read_root_table_address(const char *path, dbaddress *adr_out, short *version_out) {
	(void)path; /* path only used for logging; runtime state comes from databasedata */

	if (adr_out == NULL)
		return false;

	if (databasedata == nil) {
		cli_log_error("read_root_table_address called without an open database");
		return false;
	}

	short header_version = (**databasedata).versionnumber;
	if (version_out != NULL)
		*version_out = header_version;

	dbaddress view_address = nildbaddress;
	dbgetview(cancoonview, &view_address);
	if (view_address == nildbaddress) {
		cli_log_error("View %d is empty in system root", cancoonview);
		return false;
	}

	boolean flfree = false;
	long payload_size = 0;
	tyvariance variance = 0;
	if (!dbreadheader(view_address, &flfree, &payload_size, &variance)) {
		cli_log_error("Unable to read database header for view address 0x%08llx", (unsigned long long)view_address);
		return false;
	}

	long effective_size = payload_size - (long)variance;
	dbaddress root_address = view_address; /* fallback: treat view address as root */

	if (!flfree && effective_size >= 6) {
		unsigned char cancoon_header[6];
		memset(cancoon_header, 0, sizeof cancoon_header);

		if (dbreference(view_address, (long)sizeof cancoon_header, cancoon_header)) {
			short cancoon_version = (short)read_big_endian(cancoon_header, 2);
			dbaddress adr_root = (dbaddress)read_big_endian(cancoon_header + 2, 4);

			if ((cancoon_version == 2 || cancoon_version == 3) && adr_root != nildbaddress) {
				root_address = adr_root;
				cli_log_debug("Selected view[%d] address=0x%08llx via Cancoon v%d → root=0x%08llx",
							  cancoonview,
							  (unsigned long long)view_address,
							  (int)cancoon_version,
							  (unsigned long long)root_address);
			} else {
				cli_log_debug("Cancoon header at 0x%08llx (v%d) points to 0x%08llx; using %s",
							  (unsigned long long)view_address,
							  (int)cancoon_version,
							  (unsigned long long)adr_root,
							  (adr_root != nildbaddress) ? "fallback view address" : "view address (nil root)");
			}
		} else {
			cli_log_warn("Failed to read Cancoon header at 0x%08llx; falling back to view address",
						 (unsigned long long)view_address);
		}
	} else {
		cli_log_debug("View[%d] payload (%ld bytes) is not a Cancoon record; using address 0x%08llx",
					  cancoonview,
					  effective_size,
					  (unsigned long long)view_address);
	}

	*adr_out = root_address;
	return true;
}

/* Prints command-line usage information and examples. */
static void print_usage(const char* program_name) {
	printf("Frontier CLI - Command Line Interface for UserTalk Script Execution\n");
	printf("Version %s (%s)\n\n", FRONTIER_CLI_VERSION_STRING, FRONTIER_CLI_BUILD_DATE);
	
	printf("Usage: %s [OPTIONS] [SCRIPT_FILE]\n\n", program_name);
	
	printf("Execution Modes:\n");
	printf("  Script Execution:\n");
	printf("	%s script.usertalk					  # Execute UserTalk script file\n", program_name);
	printf("	%s -e \"3 + 4\"							# Execute inline script\n", program_name);
	printf("\n");

	printf("Options:\n");
	printf("  -e, --execute SCRIPT	   Execute inline UserTalk script\n");
	printf("  --system-root PATH	   Load system root database before executing scripts\n");
	printf("  -b, --batch			   Batch mode (disable interactive prompts)\n");
	printf("  --non-interactive		   Alias for --batch\n");
	printf("  --migrate PATH		   Migrate v6 database to v7 format and exit\n");
	printf("  --output PATH			   Output path for migrated database (default: in-place, v6 backed up)\n");
	printf("  -f, --force			   Overwrite existing output file (only applies with --output)\n");
	printf("  --skip-startup		   Skip system.startup scripts (they run by default)\n");
	printf("  --output-json			   Output results in JSON format\n");
	printf("  -v, --verbose			   Verbose output\n");
	printf("  --debug				   Debug mode\n");
	printf("  --log SPEC			   Set per-component log levels (e.g., db:trace,lang:warn)\n");
	printf("  --ws-port PORT		   Start WebSocket server on PORT (localhost only, no auth).\n");
	printf("						   WARNING: Any local process or browser page (including file://)\n");
	printf("						   can access the ODB while the server is running.\n");
	printf("  --lock-opened-roots	   Suppress save-on-exit for every loaded-from-disk system root\n");
	printf("						   and guest DB; fileMenu.save() etc. fail with a read-only\n");
	printf("						   error. In-memory mutations still evaluate; only the disk\n");
	printf("						   persist is skipped. Newly created roots (file.save /\n");
	printf("						   file.saveAs / db.compactDatabase) are unaffected.\n");
	printf("						   Also enabled by FRONTIER_LOCK_OPENED_ROOTS=1.\n");
	printf("  --ut-sync-dir DIR    Top-level .ut sync directory (ODB<->.ut bidirectional sync).\n");
	printf("                       The loaded root's basename is the first path component, so\n");
	printf("                       e.g. --ut-sync-dir usertalk_scripts with Frontier.root loaded\n");
	printf("                       writes/reads usertalk_scripts/Frontier.root/a/b/c.ut for\n");
	printf("                       ODB path a.b.c. Enables both export-on-save (Direction A)\n");
	printf("                       and import-on-materialize (Direction B, last-write-wins).\n");
	printf("                       Also enabled by FRONTIER_UT_SYNC_DIR env var; CLI flag wins.\n");
	printf("                       Example: --ut-sync-dir usertalk_scripts\n");
	printf("  -h, --help			   Show this help message\n");
	printf("  --version				   Show version information\n");
	printf("\n");

	printf("REPL Selection:\n");
	printf("  (default)            Boxen REPL -- multi-pane TUI with composited windows,\n");
	printf("                       boxen-native dialog modals, slash-menu palette.  See\n");
	printf("                       docs/BOXEN_REPL_PARITY.md for the full feature table.\n");
	printf("  --plain              Legacy linenoise REPL (single-line prompt, raw terminal\n");
	printf("                       I/O).  Use when you need the historical behavior or when\n");
	printf("                       boxen can't run (extremely small terminals, unusual ttys).\n");
	printf("                       Mutually exclusive with --debug-tui and --protocol.\n");
	printf("  --debug-tui          DEPRECATED no-op alias for the default boxen REPL.\n");
	printf("                       Originally the opt-in for boxen; preserved so existing\n");
	printf("                       scripts and aliases keep working.  Emits a one-line\n");
	printf("                       deprecation warning at startup.  Will be removed in a\n");
	printf("                       future release.  Mutually exclusive with --plain and\n");
	printf("                       --protocol.\n");
	printf("  --protocol           NDJSON protocol mode over stdin/stdout (no REPL).  For\n");
	printf("                       programmatic ODB access; documented in\n");
	printf("                       docs/CLI_USAGE_GUIDE.md and the protocol op_handler.\n");
	printf("                       Mutually exclusive with --debug-tui and --plain.\n");
	printf("\n");

	printf("Custom Arguments:\n");
	printf("  --KEY VALUE			   Any unknown flag is passed to UserTalk scripts\n");
	printf("  --KEY					   Boolean flag (no value) is set to true\n");
	printf("						   Access in UserTalk via system.environment.args.KEY\n");
	printf("						   Flag names are converted to camelCase:\n");
	printf("							 --my-flag value  ->  system.environment.args.myFlag\n");
	printf("\n");
	printf("  Built-in custom flags:\n");
	printf("  --browser MODE		   Set browser for sys.openUrl (default: system default)\n");
	printf("						   MODE: \"default\" or \"agent-browser\"\n");
	printf("\n");

	printf("Environment Variables:\n");
	printf("  FRONTIER_LOG			   Per-component log levels (e.g., db:trace,lang:warn)\n");
	printf("						   Overrides FRONTIER_LOG_LEVEL and FRONTIER_LOG_COMPONENT\n");
	printf("  FRONTIER_LOG_LEVEL	   Set global log level (TRACE, DEBUG, INFO, WARN, ERROR)\n");
	printf("  FRONTIER_LOG_COMPONENT   Filter logs by component (db, hash, lang, etc.)\n");
	printf("  FRONTIER_HEADLESS_RUN_STARTUP	 Set to 0 to skip system.startup scripts (default: run)\n");
	printf("  FRONTIER_LOCK_OPENED_ROOTS  Set to any non-empty value other than \"0\" to enable\n");
	printf("						   --lock-opened-roots without passing the flag (useful for\n");
	printf("						   test runners and ephemeral consumers). Unset and \"0\" both\n");
	printf("						   disable. The CLI flag, when passed, takes precedence and\n");
	printf("						   overrides the environment.\n");
	printf("\n");

	printf("Examples:\n");
	printf("  # Execute inline script\n");
	printf("  %s -e \"local(x = 5); x * 2\"\n", program_name);
	printf("\n");
	printf("  # Execute a UserTalk script file\n");
	printf("  %s myscript.usertalk\n", program_name);
	printf("\n");
	printf("  # Execute with system root database\n");
	printf("  %s --system-root databases/Frontier.root -e \"sizeOf(system)\"\n", program_name);
	printf("\n");
	printf("  # Migrate v6 database to v7 format (v6 backed up to .v6.root)\n");
	printf("  %s --migrate Frontier.root\n", program_name);
	printf("\n");
	printf("  # Migrate with explicit output path\n");
	printf("  %s --migrate legacy/Frontier.root --output databases/Frontier.root\n", program_name);
	printf("\n");
	printf("  # Force overwrite existing output file\n");
	printf("  %s --migrate Frontier.root --output Frontier-v7.root -f\n", program_name);
	printf("\n");
	printf("  # Execute with JSON output (for automation/testing)\n");
	printf("  %s --output-json -e \"1+1\"\n", program_name);
	printf("\n");

	printf("For detailed documentation, see: docs/CLI_USAGE_GUIDE.md\n");
	printf("\n");
}

/* Prints version and copyright information. */
static void print_version(void) {
	printf("Frontier CLI %s (%s)\n", FRONTIER_CLI_VERSION_STRING, FRONTIER_CLI_BUILD_DATE);
	printf("Copyright (C) 1992-2026 UserLand Software, Inc. and Contributors\n");
	printf("This is free software; see the source for copying conditions.\n");
}

/* Initializes the Frontier runtime: logging, thread globals, and optionally loads the system root. */
static boolean initialize_frontier_runtime(void) {
	if (g_initialized) {
		return true;
	}
	
	// Initialize CLI logging
	if (!cli_init_logging(g_cli_options.verbose, g_cli_options.debug)) {
		log_error(LOG_COMP_GENERAL, "Error: Failed to initialize logging");
		return false;
	}

	/* Register CLI args callback before inittablestructure runs */
	langenvironment_set_args_callback (populate_environment_args);

	if (!db_format_prepare_runtime()) {
		log_error(LOG_COMP_GENERAL, "Error: Failed to initialize Frontier runtime core");
		cli_cleanup_logging();
		return false;
	}

#ifdef FRONTIER_HEADLESS
	/* Install protocol-aware debugger callback (replaces no-op from langstartup.c).
	 * Only in headless builds — GUI builds use the classic script editor debugger. */
	debug_init();
#endif

	/* Initialize thread registry and register main thread with idapplicationthread (2) */
	if (!init_thread_registry()) {
		log_error(LOG_COMP_GENERAL, "Error: Failed to initialize thread registry");
		cli_cleanup_logging();
		return false;
	}

	{
		frontier_pthread_record *main_rec = register_main_thread((long)idapplicationthread);
		if (main_rec == NULL) {
			log_error(LOG_COMP_GENERAL, "Error: Failed to register main thread in registry");
			cleanup_thread_registry();
			cli_cleanup_logging();
			return false;
		}
		main_rec->hglobals = hthreadglobals;
		main_rec->pthread_id = pthread_self();
	}

	headless_threading_init(); /* Main thread acquires GIL before any scripts run */

	/*
	 * Register repl.* kernel verbs (repl.exit, repl.clearVariables,
	 * repl.jumpPath, repl.printKeyCodes, repl.list). Must run after
	 * langinitverbs() (already invoked from db_format_prepare_runtime()
	 * above) and after the main thread holds the GIL — replinitverbs()
	 * manipulates the shared hashtable stack via push/pophashtable.
	 *
	 * The host adapter (function-pointer struct that binds these verbs to
	 * the live REPL state) is installed separately when the REPL starts.
	 * If a script calls repl.* before the adapter is installed, the verb
	 * returns false at the script level rather than crashing.
	 */
	if (!replinitverbs()) {
		log_error(LOG_COMP_GENERAL, "Error: Failed to register repl.* verbs");
		releasethreadglobals();
		cli_cleanup_logging();
		return false;
	}

	if (g_cli_options.system_root != NULL) {
		if (!load_system_root_database(g_cli_options.system_root)) {
			cli_log_error("Failed to load system root database: %s", g_cli_options.system_root);
			releasethreadglobals();
			cli_cleanup_logging();
			return false;
		}
	}
	
	g_initialized = true;
	cli_log_info("Frontier runtime initialized successfully");
	
	return true;
}

/* Releases runtime resources: unloads database, releases thread globals, cleans up logging. */
static void cleanup_frontier_runtime(void) {
	if (!g_initialized) {
		return;
	}
	
	cli_log_info("Cleaning up Frontier runtime");

	/* Kill any active debug threads so they exit cleanly before shutdown.
	 * Threads are PTHREAD_CREATE_JOINABLE, so we join them after setting
	 * kill flags. This replaces the old 200ms sleep with a deterministic wait. */
	/* Save system root only if no debug threads are active. Active debug
	 * threads may have pushed hash table scopes that make packing unsafe.
	 * Killing them doesn't help — the unwind leaves tables inconsistent.
	 * In practice, debug sessions are for development, not production data. */
	/* Save system root only if no debug threads ran during this session.
	 * Killed debug threads leave pushed hash table scopes in the chain,
	 * making hashpack traversal crash on stale pointers. This is a
	 * fundamental limitation of killing scripts mid-execution. */
	/* --lock-opened-roots deliberately skips the on-exit save: the operator
	 * explicitly asked for read-only-on-save, so honor it at every layer.
	 * The lower-layer flreadonly bit already shorts dbflushheader, but
	 * skipping the save here is the intent-level guarantee.
	 *
	 * Issue #127: legacy Frontier persisted every system root mutation by
	 * default. The save runs whenever the root was opened RW; tests and
	 * other ephemeral consumers opt OUT via --lock-opened-roots /
	 * FRONTIER_LOCK_OPENED_ROOTS (which routes through
	 * g_system_root_read_only above). The dirty-bit short-circuit inside
	 * save_system_root_on_exit() handles the case where no actual mutation
	 * occurred -- nothing gets rewritten then. */
	if (g_system_root_loaded
		&& debug_is_safe_to_save()
		&& !g_system_root_read_only) {
		save_system_root_on_exit();
	}

	debug_kill_all_threads();

	/* Release GIL so killed debug threads can finish cleanup, then join them.
	 * Save/restore main thread globals since debug threads overwrite
	 * hthreadglobals when they run. */
	{
		hdlthreadglobals saved = hthreadglobals;
		headless_save_threadglobals(saved);
		pthread_mutex_unlock(&frontier_gil);
		debug_join_all_threads();
		pthread_mutex_lock(&frontier_gil);
		headless_restore_threadglobals(saved);
	}

	/* Wait for all spawned threads to finish BEFORE unloading databases.
	 * Spawned threads may still be running (blocked on GIL) and need roottable
	 * and other database structures to be intact. */
	headless_threading_shutdown();

	if (g_system_root_loaded) {
		unload_system_root_database();
	}

	/* Release main thread registry record before cleanup. */
	{
		frontier_pthread_record *main_rec = get_thread_by_id((long)idapplicationthread);
		if (main_rec) {
			release_thread_record(main_rec);  /* release lookup ref */
			free_thread_record(main_rec);	  /* release initial ref */
		}
	}
	cleanup_thread_registry();

	// Cleanup Frontier runtime
	releasethreadglobals();

	// Cleanup CLI components
	cli_cleanup_logging();
	
	g_initialized = false;
}

/* Finds or creates a named subtable under a parent table.
 *
 * If `out_created` is non-NULL, it is set to true iff the table did not
 * exist and was created by this call. This lets hydration distinguish a
 * genuine in-memory mutation (which should drive a persist) from an
 * idempotent find (which should not). See issue #127. */
static boolean ensure_named_subtable_ex(hdlhashtable parent, const unsigned char *name, hdlhashtable *out, boolean mark_dont_save, boolean *out_created) {
	if (out_created != NULL)
		*out_created = false;

	if (parent == nil || name == NULL) {
		return false;
	}

	bigstring bsname;
	copystring(name, bsname);

	hdlhashtable table = nil;
	if (findnamedtable(parent, bsname, &table)) {
		if (out != NULL) {
			*out = table;
		}
		return true;
	}

	if (tablenewsubtable(parent, bsname, &table)) {
		if (mark_dont_save) {
			langexternaldontsave(parent, bsname);
		}
		if (out != NULL) {
			*out = table;
		}
		if (out_created != NULL)
			*out_created = true;
		return true;
	}

	if (findnamedtable(parent, bsname, &table)) {
		if (out != NULL) {
			*out = table;
		}
		return true;
	}

	return false;
}

/* Backwards-compatible wrapper for callers that don't care whether the
 * subtable was found or newly created. */
static boolean ensure_named_subtable(hdlhashtable parent, const unsigned char *name, hdlhashtable *out, boolean mark_dont_save) {
	return ensure_named_subtable_ex(parent, name, out, mark_dont_save, NULL);
}

/* Visitor refcon for clear_post_hydration_dirty_flags_visit. */
typedef struct {
	long ctcleared;
} cleardirtyrefcon;

static boolean clear_post_hydration_dirty_flags_visit(hdlhashnode hnode, ptrvoid refcon);

/* Recursively clear fldirty/flsubsdirty on `ht` and all of its in-memory
 * subtables, and clear dirty on any non-table externals (outlines, wpdocs).
 *
 * Cost: O(in-memory nodes in roottable). Walk is bounded by what hydration
 * link-phase loaded (compiler / environment / charsets / temp /
 * system.menus.data subtrees plus any external table reached via
 * augment_database_tables_with_efp). Negligible relative to other hydration
 * costs. Bounded by `flinmemory` recursion guard at the visitor -- does not
 * force-load on-disk subtables.
 *
 * Rationale (issue #127): hydration link-phase work (linksystemtablestructure,
 * menudata_ensure_root, headless_init_system_paths, resolve_system_paths,
 * augment_database_tables_with_efp, ensure_named_subtable) does hashinsert /
 * hashassign on subtables to wire in-memory EFP / compiler / environment
 * tables into the loaded system table. Those hashinserts mark the host tables
 * dirty via langsymbolchanged -> dirtyhashtable. The injected tables
 * themselves are flagged fldontsave (so their value is not persisted), but
 * the dirtying of the host table they live in is what then drives
 * save_system_root_on_exit() to rewrite the whole file on shutdown -- even
 * when no user-visible mutation occurred.
 *
 * Since hydration just loaded this tree from disk, NOTHING in it should be
 * considered dirty at this point. Walk and clear, so subsequent exit-time
 * save logic can trust the dirty bits and short-circuit when no user
 * mutation happened.
 *
 * In-memory only -- we never force-load on-disk tables for the sole purpose
 * of clearing flags they don't have.
 */
static void clear_post_hydration_dirty_flags(hdlhashtable ht) {
	if (ht == nil)
		return;

	(**ht).fldirty = false;
	(**ht).flsubsdirty = false;

	cleardirtyrefcon refcon = {0};
	hashtablevisit(ht, &clear_post_hydration_dirty_flags_visit, &refcon);
}

static boolean clear_post_hydration_dirty_flags_visit(hdlhashnode hnode, ptrvoid refcon) {
	cleardirtyrefcon *rc = (cleardirtyrefcon *) refcon;
	tyvaluerecord val = (**hnode).val;

	if (val.valuetype != externalvaluetype)
		return true;

	hdlexternalvariable hv = (hdlexternalvariable) val.data.externalvalue;
	if (hv == nil)
		return true;

	if (istablevariable(hv)) {
		/* Only recurse into in-memory subtables. On-disk subtables can't be
		 * dirty (they haven't been loaded), so there's nothing to clear, and
		 * we must not force-load them just to clean flags. */
		if ((**hv).flinmemory) {
			hdlhashtable subt = (hdlhashtable) (**hv).variabledata;
			if (subt != nil)
				clear_post_hydration_dirty_flags(subt);
		}
	} else {
		/* Non-table external (outline, wpdoc, menu, picture, etc.). Clear
		 * dirty so a freshly-loaded value doesn't trigger a save. Ignore
		 * failure: this is best-effort hygiene for in-memory objects. */
		if (langexternalisdirty((hdlexternalhandle) hv)) {
			(void) langexternalsetdirty((hdlexternalhandle) hv, false);
			rc->ctcleared++;
		}
	}

	return true;
}

/* Logs the current state of system subtables for debugging hydration issues. */
static void log_system_subtable_status(const char *phase,
									   hdlhashtable system,
									   hdlhashtable verbs,
									   hdlhashtable builtins,
									   hdlhashtable agents,
									   hdlhashtable paths,
									   hdlhashtable resources,
									   hdlhashtable menubar,
									   hdlhashtable objectmodel) {
	if (phase == NULL) {
		phase = "unknown";
	}

	cli_log_debug("system table snapshot (%s): system=%p verbs=%p builtins=%p agents=%p paths=%p resources=%p menubar=%p objectmodel=%p",
				 phase,
				 (void *)system,
				 (void *)verbs,
				 (void *)builtins,
				 (void *)agents,
				 (void *)paths,
				 (void *)resources,
				 (void *)menubar,
				 (void *)objectmodel);
}

/* Loads and fully initializes the system root database, linking EFP tables and resolving paths. */
static boolean hydrate_system_root_database(const char* path, boolean read_only) {
	/* Always start from a clean slate; useful to confirm entry. */
#if defined(FRONTIER_HEADLESS)
	log_trace(LOG_COMP_STARTUP, "hydrate_system_root_database enter path=%s read_only=%s",
	          path ? path : "(nil)", read_only ? "true" : "false");
#endif
	cleartablestructureglobals();

	if (path == NULL) {
		cli_log_error("No system root path provided for hydration");
		return false;
	}

	if (!g_initialized) {
		cli_log_error("Runtime must be initialized before hydrating %s", path);
		return false;
	}

	boolean migrated = false;
	char actual_path[1024];
	if (!ensure_database_v7(path, &migrated, actual_path, sizeof actual_path)) {
		cli_log_error("Failed to verify database format before hydration: %s", path);
		return false;
	}

	/* After ensure_database_v7, we always have a v7 database (either the original if already v7,
	 * or the original path now containing the migrated v7 data, with v6 backed up to .v6.root).
	 *
	 * Read-only mode honors any of:
	 *   - the explicit `read_only` parameter (driven by --lock-opened-roots,
	 *     see cli_compute_effective_read_only)
	 *   - FRONTIER_OPEN_READONLY=1 environment override (back-compat with
	 *     pre-flag scripts; harmless when the flag is already set).
	 * Either gate is sufficient — neither false-overrides the other. */
	boolean flreadonly_for_hydration = read_only || (getenv("FRONTIER_OPEN_READONLY") != NULL);

	/* Use the output path from ensure_database_v7 if it differs from input.
	 * This handles both fresh migrations and cases where a v7 file already exists. */
	if (actual_path[0] != '\0' && strcmp(path, actual_path) != 0) {
		if (migrated) {
			cli_log_info("Migrated legacy system root to v7 format (written to): %s", actual_path);
		}
		path = actual_path;	 /* Use the v7 file for hydration */
	}

	bigstring bspath;
	copyctopstring(path, bspath);

	tyfilespec fs;
	memset(&fs, 0, sizeof fs);
	if (!pathtofilespec(bspath, &fs)) {
		cli_log_error("Unable to convert system root path to filespec: %s", path);
		return false;
	}

	hdlfilenum fnum = 0;
	boolean ok = false;
	boolean file_open = false;
	boolean db_open = false;
	boolean dispose_rootvariable = false;

	if (!openfile(&fs, &fnum, flreadonly_for_hydration)) {
		cli_log_error("Unable to open system root for hydration: %s", path);
		return false;
	}
	file_open = true;

	hdldatabaserecord previous = databasedata;

	if (!dbopenfile(fnum, flreadonly_for_hydration)) {
		cli_log_error("dbopenfile failed for system root: %s (readonly=%s)", path, flreadonly_for_hydration ? "true" : "false");
		goto cleanup;
	}
	db_open = true;
	cli_log_debug("hydro databasedata views[0]=0x%08llx views[1]=0x%08llx views[2]=0x%08llx",
				  (unsigned long long)((**databasedata).views[0]),
				  (unsigned long long)((**databasedata).views[1]),
				  (unsigned long long)((**databasedata).views[2]));

	short header_version = 0;
	dbaddress adr = nildbaddress;
	if (!read_root_table_address(path, &adr, &header_version)) {
		cli_log_error("Unable to locate root table header in %s", path);
		goto cleanup;
	}
	cli_log_debug("Hydration header version=%d rootAdr=0x%08llx", header_version, (unsigned long long)adr);

	Handle hrootvariable = nil;
	hdlhashtable hroot = nil;
	/* Reset any cached state from previous loads. */
	cleartablestructureglobals();
	/* Load directly from disk. */
	if (!tableloadsystemtable(adr, &hrootvariable, &hroot, false)) {
		cli_log_error("Failed to load system table while hydrating %s", path);
		goto cleanup;
	}

	dispose_rootvariable = true;

	/* Materialize all disk values (including nested external tables with flinmemory=0).
	 * This is critical for v7 databases that may have external tables with stale v6 addresses.
	 * Without this, accessing nested tables like system.verbs.colors will fail. */
	if (!langhash_materialize_disk_values(hroot)) {
		cli_log_error("Failed to materialize disk values in system root while hydrating %s", path);
		goto cleanup;
	}

	rootvariable = hrootvariable;
	roottable = hroot;
	currenthashtable = roottable;

	if (hashtablestack == nil) {
		if (!newclearhandle(longsizeof(tytablestack), (Handle *)&hashtablestack)) {
			cli_log_error("Failed to allocate hashtablestack while hydrating %s", path);
			goto cleanup;
		}
		(**hashtablestack).toptables = 0;
	}

	ok = checktablestructure(true);
	if (!ok) {
		cli_log_warn("checktablestructure reported issues while hydrating %s", path);
		log_system_subtable_status("hydrate: post-checktablestructure", systemtable, verbstable, builtinstable, agentstable, pathstable, resourcestable, menubartable, objectmodeltable);
	}

	/* Link in-memory compiler tables (efptable with verb callbacks) into loaded system table */
	if (!linksystemtablestructure(hroot)) {
		cli_log_error("Unable to link system tables while hydrating %s", path);
		goto cleanup;
	}

	/*
	 * PR 5.5: ensure system.menus.data exists eagerly.
	 *
	 * Why this is needed: UserTalk addresses like @system.menus.data.<bar>
	 * must parse-resolve at compile time, before any verb runs. The write-
	 * side projection helpers (menudata_ensure_bar, menudata_add_command,
	 * etc.) lazy-create deeper rungs on demand, but they cannot rescue a
	 * parse-time failure on the projection root itself.
	 *
	 * Why here specifically: the canonical hook is db_format_prepare_runtime
	 * in db_format.c, but on this hydration code path that runs BEFORE
	 * roottable is set, so menudata_ensure_root() has nowhere to write.
	 * Re-invoking here, immediately after roottable assignment and table
	 * linkage, is the belt-and-braces guarantee that the projection root
	 * exists by the time any user script is parsed. Idempotent — safe on
	 * roots where the table already exists. See ADR-016.
	 */
	if (!menudata_ensure_root()) {
		cli_log_warn("menudata_ensure_root failed while hydrating %s; @system.menus.data addresses may not parse", path);
	}

	/* Populate system.paths with processor shortcuts (must be AFTER linksystemtablestructure, BEFORE resolve_system_paths) */
	if (!headless_init_system_paths(hroot)) {
		cli_log_error("Unable to populate system.paths while hydrating %s", path);
		goto cleanup;
	}

	/* Eagerly resolve system.paths addresses now that EFP tables are linked */
	if (!resolve_system_paths(hroot)) {
		cli_log_error("Unable to resolve system.paths addresses while hydrating %s", path);
		goto cleanup;
	}

	/* Augment database tables with EFP implementations for bare verb resolution */
	if (!augment_database_tables_with_efp(hroot)) {
		cli_log_error("Unable to augment database tables with EFP while hydrating %s", path);
		goto cleanup;
	}

	/* Track whether any optional table was *actually created* (vs. found
	 * pre-existing). Persisting is only needed for genuine creations; an
	 * idempotent find must not trigger a full system-table rewrite (the
	 * Virgin.root drift issue #127). */
	boolean created_optional = false;
	boolean did_create = false;
	if (systemtable != nil) {
		if (resourcestable == nil && ensure_named_subtable_ex(systemtable, nameresourcestable, &resourcestable, false, &did_create) && did_create)
			created_optional = true;

		if (pathstable == nil && ensure_named_subtable_ex(systemtable, namepathstable, &pathstable, false, &did_create) && did_create)
			created_optional = true;

		hdlhashtable menustable = nil;
		bigstring bsmenus;
		copyctopstring("menus", bsmenus);
		if (ensure_named_subtable_ex(systemtable, bsmenus, &menustable, false, &did_create)) {
			if (did_create)
				created_optional = true;
			if (menubartable == nil && ensure_named_subtable_ex(menustable, namemenubartable, &menubartable, false, &did_create) && did_create)
				created_optional = true;
			/*
			 * Eager projection-root creation. Without this, a fresh
			 * UserTalk address like @system.menus.data.<bar> can't be
			 * parsed (the address resolver walks up to system.menus.data
			 * before the verb runs and fails if it's missing). PR 5.5
			 * write-projection helpers can lazy-create deeper rungs
			 * (<bar>, <menu>, <item>) but they cannot rescue a parse-time
			 * failure on the projection root itself. See ADR-016.
			 */
			hdlhashtable datatable = nil;
			bigstring bsdata;
			copyctopstring("data", bsdata);
			if (ensure_named_subtable_ex(menustable, bsdata, &datatable, false, &did_create) && did_create)
				created_optional = true;
		}

		hdlhashtable macintoshtable = nil;
		bigstring bsmacintosh;
		copyctopstring("macintosh", bsmacintosh);
		if (ensure_named_subtable_ex(systemtable, bsmacintosh, &macintoshtable, false, &did_create)) {
			if (did_create)
				created_optional = true;
			bigstring bsobjectmodel;
			copyctopstring("objectmodel", bsobjectmodel);
			if (objectmodeltable == nil && ensure_named_subtable_ex(macintoshtable, bsobjectmodel, &objectmodeltable, false, &did_create) && did_create)
				created_optional = true;
		}
	}
	log_system_subtable_status("hydrate: after optional creation",
							   systemtable,
							   verbstable,
							   builtinstable,
							   agentstable,
							   pathstable,
							   resourcestable,
							   menubartable,
							   objectmodeltable);

	/* Only save if we made changes (created optional tables). Skip for freshly-migrated databases.
	 * Also skip if no optional tables were created - v7 databases are already complete.
	 * Skip when read-only: the optional-tables patch is in-memory only and
	 * must not propagate to disk; tablesavesystemtable will trip the
	 * dbflushheader read-only guard, which logs an error and aborts the
	 * hydration. The patched tables remain valid in memory for this
	 * process. See issue #588. */
	if ((!migrated && created_optional && !read_only)) {
		boolean repack_scope = false;
		db_format_mode mode = {true, true};	 /* 64-bit, adapter_repack */
		db_format_mode_push(&mode);
		repack_scope = true;
		if (!tablesavesystemtable(hrootvariable, &adr)) {
			if (repack_scope) {
				db_format_mode_pop();
				repack_scope = false;
			}
			cli_log_error("Failed to save system table while hydrating %s", path);
			goto cleanup;
		}
		if (repack_scope) {
			db_format_mode_pop();
			repack_scope = false;
		}
	} else if (read_only && created_optional) {
		cli_log_debug("Read-only: skipping in-memory optional-table save for %s", path);
	} else {
		cli_log_debug("Skipping save for freshly-migrated database: %s", path);
	}

	/* dbsetview writes the header to disk via dbflushheader. The lower
	 * layer's flreadonly guard already short-circuits the actual write,
	 * but skipping the call here avoids a misleading log line and keeps
	 * the read-only intent explicit at every layer. */
	if (!read_only)
		dbsetview(cancoonview, adr);

	/* Log which system root was loaded and whether it was auto-migrated */
	if (migrated) {
		log_info(LOG_COMP_STARTUP, "Loaded system root: %s (migrated from v6 to v7)", path);
	} else {
		log_info(LOG_COMP_STARTUP, "Loaded system root: %s", path);
	}

	/* Set globals for frontier.getFilePath() verb access.
	 * Thread safety: Set path BEFORE setting loaded flag to avoid race condition
	 * where another thread sees loaded=true but path is still being written. */
	snprintf(g_system_root_path, sizeof(g_system_root_path), "%s", path);
	g_system_root_loaded = true;

	/* NOTE: Startup scripts are NOT run here during hydration.
	 * They are run in main() AFTER hydration completes, which ensures:
	 * 1. EFP tables are properly linked (linksystemtablestructure)
	 * 2. system.paths is populated and resolved
	 * 3. Database tables are augmented with EFP implementations
	 * This avoids running scripts during v6->v7 migration when database state is incomplete. */

	/* Issue #127 -- clear dirty bits that the link-phase work above set as a
	 * side effect of hashinsert/hashassign. The tree was just loaded from
	 * disk; nothing in it should be considered dirty at this point. Without
	 * this, save_system_root_on_exit() rewrites the entire file on every
	 * shutdown even when no user mutation occurred (~1.2 MB drift). */
	clear_post_hydration_dirty_flags(hroot);

	/* ut-sync boot-discovery scan: walk the sync tree and auto-create any .ut
	 * leaves that have no corresponding in-memory ODB node (orphans dropped
	 * into the sync dir while the runtime was not running). Must run AFTER the
	 * full-materialization pass above (so hashtable misses are genuine absences)
	 * and AFTER clear_post_hydration_dirty_flags (so only the newly-created
	 * nodes end up dirty). Each created path is recorded via cli_record_ut_import
	 * so the cli_redirty_ut_imported_paths call below re-dirtied them for save. */
	if (cli_get_ut_sync_dir() != NULL) {
		int scan_count = ut_sync_scan_and_create(cli_get_ut_sync_dir(),
		                                         cli_get_system_root_basename(),
		                                         1 /* record_for_redirty: boot path */);
		if (scan_count > 0) {
			log_info(LOG_COMP_STARTUP,
			         "ut-scan: %d orphan node(s) created from sync tree", scan_count);
		} else if (scan_count < 0) {
			cli_log_warn("ut-scan: boot scan encountered an error");
		}
	}

	/* ut-sync Direction B: re-dirty any scripts imported from .ut files during
	 * the bulk hydrate walk. The clear pass above wiped the dirty flags that
	 * opverbinmemory set on those scripts; this call restores them so
	 * tablesavesystemtable will persist the imported content on the next save.
	 * No-op when --ut-sync-dir is not active (count stays 0). */
	cli_redirty_ut_imported_paths();

	ok = true;

cleanup:
	/* On failure, close database, dispose root variable, clear globals, and restore previous database */
	if (!ok) {
		if (db_open) {
			if (!dbclose()) {
				cli_log_warn("dbclose reported failure while hydrating %s", path);
			}
			dbdispose();
		}

		if (dispose_rootvariable && hrootvariable != nil)
			disposehandle(hrootvariable);

		databasedata = previous;
		cleartablestructureglobals();
		currenthashtable = nil;

		if (file_open && !closefile(fnum)) {
			cli_log_warn("Failed to close hydrated system root file handle: %s", path);
		}
	}
	/* On success, database remains open and globals remain set for runtime use */

	return ok;
}

/* Internal implementation for loading the system root database with optional hydration. */
static boolean load_system_root_database_internal(const char* path, boolean allow_hydrate) {
	(void) allow_hydrate;
	if (path == NULL) {
		return true;
	}

	if (g_system_root_loaded) {
		cli_log_warn("System root already loaded; ignoring request for %s", path);
		return true;
	}

	size_t len = strlen(path);
	if (len == 0) {
		cli_log_error("System root path is empty");
		return false;
	}
	boolean migrated = false;
	char actual_path[1024];
	if (!ensure_database_v7(path, &migrated, actual_path, sizeof actual_path)) {
		cli_log_error("Failed to ensure system root is modern: %s", path);
		return false;
	}
	if (migrated) {
		cli_log_info("Migrated legacy system root to v7 format (written to): %s", actual_path);
		path = actual_path;	 /* Use the v7 file */
		/* After migration, tear down any in-memory v6 root before reloading v7. */
		cleartablestructureglobals();
	}

	len = strlen(path);	 /* Recalculate length after potential path change */
	if (len > lenbigstring) {
		cli_log_error("System root path exceeds %d characters (got %zu)", lenbigstring, len);
		return false;
	}

	bigstring bspath;
	copyctopstring(path, bspath);

	tyfilespec fs;
	memset(&fs, 0, sizeof fs);
	if (!pathtofilespec(bspath, &fs)) {
		cli_log_error("Unable to convert system root path to filespec: %s", path);
		return false;
	}

	hdlfilenum fnum = 0;
	if (!openfile(&fs, &fnum, true)) {
		cli_log_error("Unable to open system root for reading: %s", path);
		return false;
	}

	hdldatabaserecord previous = databasedata;

	if (!dbopenfile(fnum, true)) {
		cli_log_error("dbopenfile failed for system root: %s", path);
		closefile(fnum);
		databasedata = previous;
		return false;
	}
	cli_log_debug("databasedata views[0]=0x%08llx views[1]=0x%08llx views[2]=0x%08llx",
				  (unsigned long long)((**databasedata).views[0]),
				  (unsigned long long)((**databasedata).views[1]),
				  (unsigned long long)((**databasedata).views[2]));

	short header_version = 0;
	dbaddress adr = nildbaddress;
	if (!read_root_table_address(path, &adr, &header_version)) {
		cli_log_error("Unable to locate system root table header for %s", path);
		cleartablestructureglobals();
		dbdispose();
		closefile(fnum);
		databasedata = previous;
		return false;
	}
	cli_log_debug("System root header version=%d rootAdr=0x%08llx", header_version, (unsigned long long)adr);

	Handle hrootvariable = nil;
	hdlhashtable hroot = nil;
	/* Always clear cached globals before loading. */
	cleartablestructureglobals();
	if (!tableloadsystemtable(adr, &hrootvariable, &hroot, false)) {
		cli_log_error("Failed to load system table from %s", path);
		cleartablestructureglobals();
		dbdispose();
		closefile(fnum);
		databasedata = previous;
		return false;
	}

	boolean structure_ready = true;
	boolean partial_warning = false;
	boolean applied_patch = false;

	if (!settablestructureglobals(hrootvariable, false)) {
		cli_log_warn("System table structure is invalid in %s", path);
		cli_log_debug("systemtable=%p verbstable=%p builtinstable=%p agentstable=%p pathstable=%p resourcestable=%p menubartable=%p objectmodeltable=%p",
					  (void *)systemtable,
					  (void *)verbstable,
					  (void *)builtinstable,
					  (void *)agentstable,
					  (void *)pathstable,
					  (void *)resourcestable,
					  (void *)menubartable,
					  (void *)objectmodeltable);
		log_system_subtable_status("load: post-checktablestructure", systemtable, verbstable, builtinstable, agentstable, pathstable, resourcestable, menubartable, objectmodeltable);
		structure_ready = false;

		if (systemtable != nil) {
			if (resourcestable == nil && ensure_named_subtable(systemtable, nameresourcestable, &resourcestable, true))
				applied_patch = true;

			if (pathstable == nil && ensure_named_subtable(systemtable, namepathstable, &pathstable, true))
				applied_patch = true;

			hdlhashtable menustable = nil;
			bigstring bsmenus;
			copyctopstring("menus", bsmenus);
			if (ensure_named_subtable(systemtable, bsmenus, &menustable, true)) {
				if (menubartable == nil && ensure_named_subtable(menustable, namemenubartable, &menubartable, true))
					applied_patch = true;
				/*
				 * Eager projection-root creation (mirror of the hydrate
				 * branch above). See the long comment there for rationale.
				 */
				hdlhashtable datatable = nil;
				bigstring bsdata;
				copyctopstring("data", bsdata);
				if (ensure_named_subtable(menustable, bsdata, &datatable, true))
					applied_patch = true;
			}

			hdlhashtable macintoshtable = nil;
			bigstring bsmacintosh;
			copyctopstring("macintosh", bsmacintosh);
			if (ensure_named_subtable(systemtable, bsmacintosh, &macintoshtable, true)) {
				bigstring bsobjectmodel;
				copyctopstring("objectmodel", bsobjectmodel);
				if (objectmodeltable == nil && ensure_named_subtable(macintoshtable, bsobjectmodel, &objectmodeltable, true))
					applied_patch = true;
			}
		}

		log_system_subtable_status("load: after ensure_named_subtable patch",
								   systemtable,
								   verbstable,
								   builtinstable,
								   agentstable,
								   pathstable,
								   resourcestable,
								   menubartable,
								   objectmodeltable);

		if (applied_patch) {
			cli_log_debug("Applied fallback table creation for %s; structure now adequate", path);
			/* The fallback tables are optional - accept the structure as-is if critical tables exist */
			if (systemtable != nil && verbstable != nil && builtinstable != nil) {
				structure_ready = true;
			} else {
				cli_log_warn("System table structure still invalid after fallback initialization: %s", path);
				cli_log_debug("systemtable=%p verbstable=%p builtinstable=%p agentstable=%p pathstable=%p resourcestable=%p menubartable=%p objectmodeltable=%p",
							  (void *)systemtable,
							  (void *)verbstable,
							  (void *)builtinstable,
							  (void *)agentstable,
							  (void *)pathstable,
							  (void *)resourcestable,
							  (void *)menubartable,
							  (void *)objectmodeltable);
				structure_ready = false;
			}
		}

		if (!structure_ready || systemtable == nil || verbstable == nil) {
			dbdispose();
			closefile(fnum);
			databasedata = previous;
			return false;
		}

		if (partial_warning) {
			cli_log_warn("Proceeding with minimally hydrated system tables (optional tables may be missing) for %s", path);
		} else if (applied_patch) {
			cli_log_info("Loaded system root database with fallback table hydration: %s", path);
		} else {
			cli_log_warn("Proceeding despite partial system table validation; optional tables may be missing for %s", path);
		}
	}

	if (!linksystemtablestructure(roottable)) {
		cli_log_error("Unable to link system tables for %s", path);
		cleartablestructureglobals();
		dbdispose();
		closefile(fnum);
		databasedata = previous;
		return false;
	}

	/*
	 * PR 5.5: ensure system.menus.data exists eagerly so UserTalk addresses
	 * like @system.menus.data.<bar> parse before the verb runs. The write-
	 * side projection helpers can lazy-create deeper rungs but cannot
	 * rescue a parse-time failure on the projection root itself. Idempotent
	 * — safe to call on roots where the table already exists. See ADR-016.
	 */
	if (!menudata_ensure_root()) {
		cli_log_warn("menudata_ensure_root failed for %s; @system.menus.data addresses may not parse", path);
	}

	currenthashtable = roottable;

	/* Set globals so callers know the database is loaded.
	 * Thread safety: Set path BEFORE setting loaded flag to avoid race condition
	 * where another thread sees loaded=true but path is still being written. */
	g_previous_database = previous;
	g_system_root_fnum = fnum;
	snprintf(g_system_root_path, sizeof(g_system_root_path), "%s", path);
	g_system_root_loaded = true;

	/* NOTE: Startup scripts are NOT run here. They are run in main() AFTER
	 * hydrate_system_root_database() completes, which ensures EFP tables are
	 * properly linked and system.paths is resolved before scripts execute.
	 * See loadsystemscripts() call in main() after hydration. */

	cli_log_info("Loaded system root database: %s", g_system_root_path);
	return true;
}

/* Loads the system root database with full hydration enabled. */
static boolean load_system_root_database(const char* path) {
	return load_system_root_database_internal(path, true);
}

/* -------------------------------------------------------------------------
 * ODB -> .ut export walk (ut_sync export hook, Direction A)
 *
 * Called from save_system_root_on_exit() when --ut-sync-dir is active.
 * Walks the system root hashtable tree recursively, building dotted paths
 * (identical pattern to langhash_materialize_table_internal). For each node
 * that is a script or outline external AND is dirty, calls ut_export_script()
 * to write the .ut file.
 *
 * "Dirty" check: langexternalisdirty(hv) -> opverbisdirty -> (**ho).fldirty ||
 * (**ho).fldirtyview. The value must also be flinmemory=true for the dirty
 * flag to be meaningful (disk-only values can't be dirty in memory).
 *
 * Only errors are logged; individual script failures do not abort the walk.
 * The ODB save proceeds regardless -- this is export-only, the save path is
 * authoritative.
 * ---------------------------------------------------------------------- */

typedef struct ut_export_walk_ctx {
	const char *sync_dir;
	int exported;  /* count of successfully exported scripts */
	int errors;    /* count of export failures */
} ut_export_walk_ctx;

/* Forward declaration (recursive walk calls itself for subtables). */
static void ut_export_walk_table(hdlhashtable htable, const char *path,
                                 ut_export_walk_ctx *ctx);

static void ut_export_walk_table(hdlhashtable htable, const char *path,
                                 ut_export_walk_ctx *ctx) {
	hdlhashnode nomad;

	if (htable == nil)
		return;

	for (nomad = (**htable).hfirstsort; nomad != nil;
	     nomad = (**nomad).sortedlink) {
		tyvaluerecord *val = &(**nomad).val;
		bigstring bsname;
		char nodepath[512];

		gethashkey(nomad, bsname);

		/* Build the dotted path for this node, percent-encoding the raw segment
		 * so ODB keys with '.' '/' ':' '"' '\' or control bytes are lossless. */
		{
			char encoded_seg[766]; /* 255 * 3 + 1 -- max encoded Pascal name */
			int n;
			if (!ut_pct_encode_segment((const char *)&bsname[1], (size_t)bsname[0],
			                           encoded_seg, sizeof(encoded_seg))) {
				cli_log_warn("ut-sync: segment encode overflow for node under %s",
				             path ? path : "<root>");
				continue;
			}
			if (path != NULL && path[0] != '\0')
				n = snprintf(nodepath, sizeof(nodepath), "%s.%s", path, encoded_seg);
			else
				n = snprintf(nodepath, sizeof(nodepath), "%s", encoded_seg);
			if (n < 0 || n >= (int)sizeof(nodepath)) {
				cli_log_warn("ut-sync: nodepath overflow under %s", path ? path : "<root>");
				continue;
			}
		}
		nodepath[sizeof(nodepath) - 1] = '\0'; /* belt-and-suspenders NUL */

		if (val->valuetype != externalvaluetype)
			continue;

		{
			hdlexternalvariable hv = (hdlexternalvariable) val->data.externalvalue;
			tyexternalid extid = (**hv).id;

			if (extid == idtableprocessor) {
				/* Recurse into subtables. */
				if ((**hv).flinmemory)
					ut_export_walk_table(
						(hdlhashtable)(**hv).variabledata,
						nodepath, ctx);
				continue;
			}

			/* Export script and outline externals when dirty. */
			if (extid == idscriptprocessor || extid == idoutlineprocessor) {
				Handle htext = nil;
				long sig = 0;
				int64_t tc = 0, tm = 0;

				/* Only dirty in-memory values need exporting. */
				if (!(**hv).flinmemory || !langexternalisdirty(hv))
					continue;

				/* Get source text (flpretty=true to match corpus convention). */
				if (!opverbgetlangtext(hv, true, &htext, &sig)) {
					cli_log_warn("ut-sync: opverbgetlangtext failed for %s",
					             nodepath);
					ctx->errors++;
					continue;
				}

				/* Get modification time (Mac epoch). */
				if (!langexternalgettimes(hv, &tc, &tm, nil)) {
					cli_log_warn("ut-sync: langexternalgettimes failed for %s",
					             nodepath);
					/* Not fatal -- export with mtime 0 (skip stamping). */
					tm = 0;
				}

				{
					size_t rawlen = gethandlesize(htext);
					const unsigned char *raw =
						(const unsigned char *)(*htext);
					int conflict = 0;
					int ok = ut_export_script(raw, rawlen,
					                          nodepath,
					                          ctx->sync_dir,
					                          tm, &conflict);
					disposehandle(htext);
					htext = nil;

					if (ok) {
						cli_log_debug(
							"ut-sync: exported %s", nodepath);
						ctx->exported++;
					} else if (conflict) {
						cli_log_error(
							"ut-sync export CONFLICT for %s: the .ut changed "
							"since the last sync and differs from the ODB "
							"version; NOT overwriting it. The ODB keeps its "
							"edit; to resolve, make one side current, then "
							"delete this script's line from .ut-sync-state.",
							nodepath);
						ctx->errors++;
					} else {
						cli_log_warn(
							"ut-sync: ut_export_script failed for %s",
							nodepath);
						ctx->errors++;
					}
				}
				continue;
			}
			/* Other external types (menu, wp, pict): skip. */
		}
	}
}

/* Saves the system root database to disk before unloading.
 * Called during cleanup to persist any changes made during script execution
 * (e.g., user.databases entries created by finishInstall during first-run).
 *
 * Issue #127: this short-circuits to a no-op when the in-memory tree is
 * fully clean. Hydration pre-clears the dirty bits set by link-phase
 * hashinserts (see clear_post_hydration_dirty_flags), so the only way the
 * root reaches save with dirty bits set is via genuine user mutation. */
static void save_system_root_on_exit(void) {
	dbaddress root_adr;
	db_context ctx;
	dbaddress prior_view = nildbaddress;

	if (databasedata == nil || rootvariable == nil) {
		return;
	}

	/* Propagate sub-dirty flags up so we can trust roottable->flsubsdirty. */
	tablepreflightsubsdirtyflag((hdlexternalvariable) rootvariable);

	cli_log_debug("save_system_root_on_exit: entry roottable=%p fldirty=%d flsubsdirty=%d",
	              (void *)roottable,
	              roottable ? (**roottable).fldirty : -1,
	              roottable ? (**roottable).flsubsdirty : -1);

	if (roottable != nil
		&& !(**roottable).fldirty
		&& !(**roottable).flsubsdirty) {
		/* Nothing changed since hydration -- skip the entire save path.
		 * No tablesavesystemtable (avoids rewriting all reachable blocks),
		 * no dbflushreleasestack (no allocations to flush), no dbsetview
		 * (avoids a misleading header rewrite). */
		cli_log_info("save_system_root_on_exit: tree clean, skipping save");
		return;
	}

	cli_log_info("Saving system root database before exit");

	/* ODB -> .ut export (Direction A): export dirty scripts before pack
	 * clears their dirty flags. Gated by --ut-sync-dir. tablesavesystemtable
	 * runs opverbpack per dirty script which sets (**ho).fldirty=false, so
	 * export must happen here, before the pack pass.
	 *
	 * Effective sync base = <ut_sync_dir>/<rootBasename>. The path functions
	 * in ut_sync.c remain unchanged; we build the base here and pass it as
	 * sync_dir to ut_export_script (via the walk ctx). */
	if (g_cli_options.ut_sync_dir != NULL && roottable != nil) {
		char effective_sync_base[CLI_MAX_PATH_LENGTH * 2 + 4];
		const char *rbn = cli_get_system_root_basename();
		if (rbn[0] != '\0') {
			snprintf(effective_sync_base, sizeof(effective_sync_base),
			         "%s/%s", g_cli_options.ut_sync_dir, rbn);
		} else {
			/* No root basename (unusual): fall back to sync_dir itself. */
			snprintf(effective_sync_base, sizeof(effective_sync_base),
			         "%s", g_cli_options.ut_sync_dir);
		}

		ut_export_walk_ctx export_ctx;
		export_ctx.sync_dir = effective_sync_base;
		export_ctx.exported = 0;
		export_ctx.errors   = 0;
		/* Walk starting from the root hashtable (roottable IS the
		 * hdlhashtable; it is not an external variable wrapping one). */
		ut_export_walk_table(roottable, "", &export_ctx);
		cli_log_info("ut-sync: exported %d dirty script(s) to %s (%d error(s))",
		             export_ctx.exported, effective_sync_base,
		             export_ctx.errors);
	}

	/* Remember the prior view so we can avoid an unnecessary header flush. */
	dbgetview(cancoonview, &prior_view);

	/* Save the root table using v7 format.
	 *
	 * adapter_repack=true forces a full repack: every reachable block is
	 * rewritten under the current format mode (64-bit). This is required
	 * for save correctness when the on-disk tree mixes addresses across
	 * format generations or when adapter logic must normalize them. Without
	 * repack, the save can leave stale dbaddresses dangling in subtrees
	 * whose owners weren't visited (e.g., user.inetd.listens). Issue #127. */
	{
		db_format_mode mode = {true, true};  /* 64-bit, adapter_repack */
		db_format_mode_push(&mode);

		if (!tablesavesystemtable(rootvariable, &root_adr)) {
			db_format_mode_pop();
			cli_log_warn("save_system_root_on_exit: tablesavesystemtable failed");
			return;
		}

		db_format_mode_pop();
	}

	/* Flush release stack */
	db_context_init(&ctx);

	if (!dbflushreleasestack_context(&ctx))
		cli_log_warn("save_system_root_on_exit: dbflushreleasestack_context failed");

	/* Update views[0] to point to the saved root table only if the address
	 * actually changed. dbsetview rewrites the file header, so skipping it
	 * when the view is unchanged avoids touching the header pages. */
	if (root_adr != prior_view)
		dbsetview(cancoonview, root_adr);

	cli_log_info("System root database saved successfully");
}

/* Unloads the system root database and clears all global table structures. */
static void unload_system_root_database(void) {
	if (!g_system_root_loaded) {
		return;
	}

	const char* path = (g_system_root_path[0] != '\0') ? g_system_root_path : "(unknown)";
	cli_log_info("Unloading system root database: %s", path);

	/* Save already done in cleanup_frontier_runtime before debug thread kill.
	 * Don't save again — hash table state may be inconsistent after kill. */

	/* Skip table unlink if debug threads ran — hash table chain may be
	 * corrupt from killed scripts. The process is exiting anyway. */
	if (systemtable != nil && debug_is_safe_to_save()) {
		if (!unlinksystemtablestructure()) {
			cli_log_warn("Failed to unlink system table structure during unload");
		}
	} else if (systemtable != nil) {
		log_warn(LOG_COMP_DB, "Skipping unlinksystemtablestructure: debug thread was killed (hash tables may be inconsistent)");
	}

	cleartablestructureglobals();
	currenthashtable = nil;

	if (databasedata != nil) {
		dbdispose();
	}

	if (g_system_root_fnum != 0) {
		if (!closefile(g_system_root_fnum)) {
			cli_log_warn("Failed to close system root file handle");
		}
	}

	databasedata = g_previous_database;
	g_previous_database = nil;
	g_system_root_fnum = 0;
	g_system_root_loaded = false;
	g_system_root_path[0] = '\0';
}

/* Executes a script from file or inline source and returns success status. */
static boolean execute_script_mode(void) {
	cli_log_info("Executing script mode");

	if (g_cli_options.script_file != NULL) {
		// Execute script file
		return cli_execute_script_file(g_cli_options.script_file, g_cli_options.output_json);
	} else if (g_cli_options.inline_script != NULL) {
		// Execute inline script
		return cli_execute_inline_script(g_cli_options.inline_script, g_cli_options.output_json);
	}

	return false;
}
