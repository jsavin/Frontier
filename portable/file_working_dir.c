/*
 * file_working_dir.c - Thread-local working directory implementation
 *
 * Implements process-level and thread-local working directory management
 * for portable builds (headless mode).
 *
 * Architecture:
 * - Process-level: Single g_process_default_cwd initialized at startup
 * - Thread-level: Each thread has current_working_directory in tythreadglobals
 * - New threads inherit process default
 * - Threads can change their own cwd without affecting others
 *
 * Created: 2026-01-01 - Thread-local working directory system
 */

#include "frontier.h"
#include "standard.h"
#include "strings.h"
#include "file_working_dir.h"
#include "processinternal.h"
#include "logging.h"

#include <string.h>  /* for strlen, memcpy */

/* Process-level default working directory (initialized once at startup) */
static bigstring g_process_default_cwd = "\x00";


void init_default_working_dir(const char *path) {
	if (!path || path[0] == '\0') {
		setemptystring(g_process_default_cwd);
		log_warn(LOG_COMP_GENERAL, "init_default_working_dir: empty path");
		return;
	}

	/* Convert C string to bigstring */
	size_t len = strlen(path);
	if (len > 255) {
		len = 255;
		log_warn(LOG_COMP_GENERAL, "init_default_working_dir: path truncated to 255 chars");
	}

	g_process_default_cwd[0] = (unsigned char)len;
	memcpy(&g_process_default_cwd[1], path, len);

	log_info(LOG_COMP_GENERAL, "Process default cwd: %.*s", (int)len, path);
}


boolean get_thread_working_dir(bigstring out) {
	if (!out) {
		log_error(LOG_COMP_GENERAL, "get_thread_working_dir: NULL out parameter");
		return false;
	}

	hdlthreadglobals hg = getcurrentthreadglobals();
	if (!hg) {
		/* No thread context - return process default */
		log_debug(LOG_COMP_GENERAL, "get_thread_working_dir: no thread context, using process default");
		copystring(g_process_default_cwd, out);
		return true;
	}

	/* Return thread-local cwd */
	copystring((**hg).current_working_directory, out);

	log_trace(LOG_COMP_GENERAL, "get_thread_working_dir: thread %p cwd=%.*s",
	          (void*)hg, (int)out[0], &out[1]);

	return true;
}


boolean set_thread_working_dir(const bigstring path) {
	if (!path) {
		log_error(LOG_COMP_GENERAL, "set_thread_working_dir: NULL path parameter");
		return false;
	}

	hdlthreadglobals hg = getcurrentthreadglobals();
	if (!hg) {
		log_error(LOG_COMP_GENERAL, "set_thread_working_dir: no thread context");
		return false;
	}

	/* Update thread-local cwd */
	copystring(path, (**hg).current_working_directory);

	log_debug(LOG_COMP_GENERAL, "set_thread_working_dir: thread %p cwd=%.*s",
	          (void*)hg, (int)path[0], &path[1]);

	return true;
}


void get_process_default_cwd(bigstring out) {
	if (!out) {
		return;
	}

	copystring(g_process_default_cwd, out);

	log_trace(LOG_COMP_GENERAL, "get_process_default_cwd: %.*s",
	          (int)out[0], &out[1]);
}
