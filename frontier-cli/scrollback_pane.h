/*
 * scrollback_pane.h - Compositor-aware scrollback for async output.
 *
 * Issue #593 follow-up to PR #582 (palette modal GIL yield). Background
 * threads (TCP callbacks, agent ticks, anything that runs UserTalk
 * outside the REPL main thread) can write to stdout while the palette
 * modal is on screen. A direct stdout write scrolls under the palette
 * panes; the compositor's diff-render then repaints the palette on the
 * next keystroke, producing visible flicker.
 *
 * scrollback_pane_t bridges the gap. It owns:
 *   - A pane_t that registers with the compositor at the bottom of the
 *     z-stack, behind the menubar and palette levels.
 *   - A line-oriented circular buffer protected by a pthread mutex, so
 *     async writers (foreign threads) can append without coordinating
 *     with the main thread.
 *
 * The main thread (palette modal idle tick) calls
 * scrollback_pane_render_to_pane() to project the most-recent N lines
 * onto the pane buffer, then triggers compositor_render(). The pane's
 * filled cells appear UNDER the palette because of z-order; cells
 * not covered by any palette pane show the scrollback content.
 *
 * Threading contract
 * ------------------
 *   - scrollback_pane_init / _destroy / _render_to_pane / _flush_to_stdout
 *     MUST be called on the REPL main thread (they touch the pane buffer
 *     and/or stdout).
 *   - scrollback_pane_append MAY be called from any thread; it locks the
 *     ring-buffer mutex internally. It does NOT touch the pane buffer.
 *
 * The matching repl_output_async.h declares the small router that
 * decides whether to write to stdout (palette-inactive) or append to a
 * registered scrollback (palette-active).
 */

#ifndef FRONTIER_SCROLLBACK_PANE_H
#define FRONTIER_SCROLLBACK_PANE_H

#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>

#include "pane.h"

typedef struct scrollback_pane {
	pane_t pane;            /* Embedded pane; register pane address with the
	                         * compositor (compositor_register(&sb.pane)). */

	/* Circular buffer of heap-allocated NUL-terminated lines. */
	char **lines;
	size_t line_capacity;   /* Max stored lines (ring size). */
	size_t line_count;      /* Lines currently stored (<= line_capacity). */
	size_t head;            /* Next write index. */

	/* Lock protecting `lines`, `line_count`, `head`. NOT the pane buffer. */
	pthread_mutex_t mutex;

	/* Sequence counter incremented on every append; the main thread
	 * uses it to skip pane re-projection when nothing changed. Reads
	 * from the main thread are advisory — the lock still protects the
	 * actual line content. */
	unsigned long seq;
	unsigned long last_rendered_seq;
} scrollback_pane_t;

/* Initialise a scrollback pane.
 *   x, y, w, h        — pane geometry on the framebuffer (passed to pane_init).
 *   line_capacity     — max number of stored scrollback lines (ring size).
 *
 * On OOM, line_capacity is set to 0 and append becomes a no-op. The
 * pane buffer is still allocated by pane_init so register/render work
 * (with no scrollback content shown). */
void scrollback_pane_init(scrollback_pane_t *sb,
                          int x, int y, int w, int h,
                          size_t line_capacity);

/* Free all storage. The caller is responsible for compositor_unregister
 * BEFORE calling destroy. */
void scrollback_pane_destroy(scrollback_pane_t *sb);

/* Append `len` bytes as one or more lines. Splits on '\n'; bytes
 * without a trailing '\n' are queued as a partial line and joined with
 * the next append. CR ('\r') in the input is treated as a line break
 * to handle Windows / OSC-mode peers cleanly.
 *
 * Thread-safe: locks the ring-buffer mutex internally. Foreign threads
 * may call this directly. */
void scrollback_pane_append(scrollback_pane_t *sb,
                            const char *bytes, size_t len);

/* Project the most-recent lines (bottom-justified) onto the pane buffer.
 * Older lines that don't fit are clipped. Long lines are truncated at
 * the right edge — wrapping is intentionally not implemented to keep
 * the projection rule simple and predictable.
 *
 * Caller-managed: this is a "make the pane buffer reflect current
 * scrollback state" operation. Idempotent — calling twice without an
 * intervening append produces the same pane buffer. */
void scrollback_pane_render_to_pane(scrollback_pane_t *sb);

/* Drain all stored lines to stdout (in append order) and clear the
 * ring. Used by the palette modal teardown so any messages that
 * arrived during the modal session are visible in the user's
 * post-palette terminal scrollback.
 *
 * Main thread only. */
void scrollback_pane_flush_to_stdout(scrollback_pane_t *sb);

#endif /* FRONTIER_SCROLLBACK_PANE_H */
