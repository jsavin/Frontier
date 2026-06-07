/*
 * boxen_log.c -- logging hook shim for the boxen substrate.
 *
 * Boxen is silent by default: no output unless a log hook is installed via
 * boxen_set_log_hook(). When an application installs a hook (e.g., routing
 * to Frontier's log_error/log_warn macros), boxen__log() calls it.
 *
 * No fprintf(stderr, ...) anywhere in this file -- boxen itself never
 * writes to stderr directly.
 */

#include "boxen_internal.h"

#include <stdarg.h>
#include <stdio.h>

/* -------------------------------------------------------------------------
 * Installed hook state
 * ---------------------------------------------------------------------- */

static boxen_log_fn  g_log_fn   = NULL;
static void         *g_log_data = NULL;

void boxen_set_log_hook(boxen_log_fn fn, void *user_data) {
	g_log_fn   = fn;
	g_log_data = user_data;
}

/* -------------------------------------------------------------------------
 * Internal log dispatcher
 * ---------------------------------------------------------------------- */

void boxen__log(boxen_log_level_t level,
                const char *file, int line,
                const char *fmt, ...) {
	if (!g_log_fn) {
		return;
	}

	char buf[512];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	g_log_fn(level, file, line, buf, g_log_data);
}

/* -------------------------------------------------------------------------
 * Last-error support (thread-local string, shared with boxen core)
 * ---------------------------------------------------------------------- */

_Thread_local static char g_last_error[256] = "";

/* Called by boxen internals to record an error string. */
void boxen__set_last_error(const char *msg) {
	if (msg) {
		size_t i;
		for (i = 0; i < sizeof(g_last_error) - 1 && msg[i]; i++) {
			g_last_error[i] = msg[i];
		}
		g_last_error[i] = '\0';
	} else {
		g_last_error[0] = '\0';
	}
}

const char *boxen_last_error_str(void) {
	return g_last_error;
}
