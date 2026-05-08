/*
 * repl_output_async.c - Async output router (palette-aware).
 *
 * See repl_output_async.h for the contract.
 *
 * State model
 * -----------
 * Two pieces of routing state, both protected by g_router_mutex:
 *   - g_palette_active : the modal is on screen and owns the framebuffer.
 *   - g_scrollback     : the scrollback pane to which active routes go.
 *
 * Writers (any thread) take g_router_mutex briefly to read both and
 * decide their destination, then drop the lock before doing I/O. This
 * avoids serialising stdout writes through a single lock — non-palette
 * mode falls back to stdio's per-stream lock as the only contention
 * point, matching pre-issue-#593 behaviour.
 *
 * The scrollback pane has its own mutex protecting the ring; we don't
 * double-lock here.
 *
 * Why a separate translation unit
 * --------------------------------
 * repl_output.c carries a mountain of REPL-side dependencies (linenoise,
 * Pascal-string helpers, ODB types, hash tables). Tests for this fix
 * shouldn't need any of that. Splitting the async router into its own
 * .c gives the test build a tight standalone target (pane.c +
 * scrollback_pane.c + repl_output_async.c) with no Frontier-runtime
 * link surface. The legacy repl_async_output(const char *) symbol in
 * repl_output.c is rewritten to delegate here so the production behaviour
 * remains a single chokepoint.
 */

#include "repl_output_async.h"

#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "scrollback_pane.h"

static pthread_mutex_t g_router_mutex = PTHREAD_MUTEX_INITIALIZER;
static bool g_palette_active = false;
static struct scrollback_pane *g_scrollback = NULL;

void repl_async_output_set_palette_active(bool active,
                                          struct scrollback_pane *sb) {
	pthread_mutex_lock(&g_router_mutex);
	g_palette_active = active;
	g_scrollback = sb;
	pthread_mutex_unlock(&g_router_mutex);
}

bool repl_async_output_palette_active(void) {
	pthread_mutex_lock(&g_router_mutex);
	bool a = g_palette_active;
	pthread_mutex_unlock(&g_router_mutex);
	return a;
}

void repl_async_output_emit(const char *bytes, size_t len) {
	if (!bytes || len == 0) return;

	pthread_mutex_lock(&g_router_mutex);
	bool active = g_palette_active;
	struct scrollback_pane *sb = g_scrollback;
	pthread_mutex_unlock(&g_router_mutex);

	if (active && sb != NULL) {
		scrollback_pane_append(sb, bytes, len);
		return;
	}

	/* Fallback (palette inactive, or active-but-no-scrollback by
	 * mis-config): write to stdout directly. */
	fwrite(bytes, 1, len, stdout);
	fflush(stdout);
}
