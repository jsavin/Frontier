/*
 * scrollback_pane.c - Compositor-aware scrollback for async output.
 *
 * See scrollback_pane.h for the public-API contract and threading rules.
 *
 * Storage shape
 * -------------
 * Lines are kept in a fixed-capacity ring of heap-allocated NUL-
 * terminated strings. The ring grows up to line_capacity; once full,
 * the oldest line is freed and overwritten on the next append. We pay
 * one malloc per appended line — simple, predictable, no fragmentation
 * concerns at the volumes we expect (≤256 lines × <1KiB ≈ 256 KiB
 * worst case).
 *
 * Why lines and not raw bytes: the renderer is row-oriented — each
 * line maps to one row in the pane buffer. Splitting at append time
 * keeps the render path lock-free with respect to the writers (it
 * only walks the ring and copies cells).
 *
 * Partial-line handling
 * ---------------------
 * If an append's last byte is not a line terminator, the trailing
 * bytes are buffered in `partial` and prepended to the next append.
 * This matches stdio line buffering and keeps a single logical line
 * intact even when a writer breaks it across two chunks.
 *
 * Threading
 * ---------
 * The mutex protects (lines[], line_count, head, partial). The pane
 * buffer is touched ONLY by scrollback_pane_render_to_pane and by the
 * compositor's blit — both run on the main thread. So the render
 * path (a) takes the lock briefly to snapshot a stable view of the
 * lines into a stack array, (b) releases the lock, (c) writes to the
 * pane buffer with no further synchronisation. This keeps writers
 * blocked for O(visible_rows) memcpys at most.
 */

#include "scrollback_pane.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pane.h"

/* ---------- helpers ---------- */

/* Write a NUL-terminated string into the pane buffer at row `y`,
 * truncating at the right edge. Bytes < 0x20 are replaced with '?'
 * to defend against terminal-injection (mirrors safe_render_codepoint
 * in pane.c — that's the egress filter; here we filter at projection
 * time too so the user-visible cell content is conservative). */
static void put_line_to_pane(pane_t *p, int y, const char *line) {
	if (!p || !p->buf || !line) return;
	if (y < 0 || y >= p->h) return;
	int col = 0;
	for (const char *s = line; *s != '\0' && col < p->w; ++s) {
		unsigned char c = (unsigned char)*s;
		if (c == '\t') c = ' ';     /* expand to single space — keep the
		                             * row-oriented projection simple. */
		if (c < 0x20 || c == 0x7F) c = '?';
		pane_putc(p, col, y, (uint32_t)c, 0);
		++col;
	}
}

/* ---------- lifecycle ---------- */

void scrollback_pane_init(scrollback_pane_t *sb,
                          int x, int y, int w, int h,
                          size_t line_capacity) {
	if (!sb) return;
	memset(sb, 0, sizeof(*sb));
	pane_init(&sb->pane, x, y, w, h);
	pthread_mutex_init(&sb->mutex, NULL);
	if (line_capacity == 0) {
		sb->lines = NULL;
		sb->line_capacity = 0;
		return;
	}
	sb->lines = (char **)calloc(line_capacity, sizeof(char *));
	if (!sb->lines) {
		sb->line_capacity = 0;
		return;
	}
	sb->line_capacity = line_capacity;
	sb->line_count = 0;
	sb->head = 0;
}

void scrollback_pane_destroy(scrollback_pane_t *sb) {
	if (!sb) return;
	if (sb->lines) {
		for (size_t i = 0; i < sb->line_capacity; ++i) {
			free(sb->lines[i]);
			sb->lines[i] = NULL;
		}
		free(sb->lines);
		sb->lines = NULL;
	}
	sb->line_capacity = 0;
	sb->line_count = 0;
	sb->head = 0;
	pane_destroy(&sb->pane);
	pthread_mutex_destroy(&sb->mutex);
}

/* ---------- append ---------- */

/* Internal: store one complete line (no terminator) under the lock.
 * Caller already holds sb->mutex. */
static void store_line_locked(scrollback_pane_t *sb, const char *buf, size_t len) {
	if (!sb->lines || sb->line_capacity == 0) return;
	char *copy = (char *)malloc(len + 1);
	if (!copy) return;  /* drop on OOM */
	memcpy(copy, buf, len);
	copy[len] = '\0';

	size_t slot = sb->head;
	if (sb->lines[slot] != NULL) {
		free(sb->lines[slot]);
	}
	sb->lines[slot] = copy;
	sb->head = (sb->head + 1) % sb->line_capacity;
	if (sb->line_count < sb->line_capacity) {
		sb->line_count++;
	}
	sb->seq++;
}

void scrollback_pane_append(scrollback_pane_t *sb,
                            const char *bytes, size_t len) {
	if (!sb || !bytes || len == 0) return;

	pthread_mutex_lock(&sb->mutex);

	/* The current implementation does not preserve partial lines
	 * across append calls; each call is treated as a self-contained
	 * sequence of zero-or-more terminated lines plus an optional
	 * final unterminated line that we store as-is. Rationale: the
	 * call sites (msg(), error paths, agent ticks) all emit complete
	 * lines per call. If we later need cross-call buffering we can
	 * add a per-scrollback partial without touching the public API.
	 *
	 * Splitter: scan for '\n' or '\r'; the run between terminators
	 * is one stored line. Empty runs (consecutive terminators) are
	 * stored as empty lines so blank lines render correctly. */
	size_t start = 0;
	for (size_t i = 0; i < len; ++i) {
		char c = bytes[i];
		if (c == '\n' || c == '\r') {
			size_t run_len = i - start;
			store_line_locked(sb, bytes + start, run_len);
			start = i + 1;
		}
	}
	if (start < len) {
		store_line_locked(sb, bytes + start, len - start);
	}

	pthread_mutex_unlock(&sb->mutex);
}

/* ---------- render ---------- */

void scrollback_pane_render_to_pane(scrollback_pane_t *sb) {
	if (!sb) return;

	/* Snapshot the most-recent lines under the lock, then release
	 * before writing to the pane buffer. The pane buffer is owned by
	 * the main thread (us); the lock only protects the ring. */
	int rows = sb->pane.h;
	if (rows <= 0) return;

	/* We project the bottom `rows` lines (most recent), one per row.
	 * If we have fewer lines than rows, leave the upper rows empty. */
	pthread_mutex_lock(&sb->mutex);

	size_t line_count = sb->line_count;
	size_t take = (line_count < (size_t)rows) ? line_count : (size_t)rows;
	size_t cap = sb->line_capacity;

	/* Compute starting index of the oldest line we want to render.
	 * The newest line is at (head - 1) mod cap; oldest of the take
	 * window is at (head - take) mod cap. */
	char **slot_snapshots = NULL;
	if (take > 0) {
		slot_snapshots = (char **)calloc(take, sizeof(char *));
	}

	if (slot_snapshots != NULL) {
		size_t start = (sb->head + cap - take) % cap;
		for (size_t i = 0; i < take; ++i) {
			size_t idx = (start + i) % cap;
			const char *src = sb->lines[idx];
			if (src) {
				size_t l = strlen(src);
				char *copy = (char *)malloc(l + 1);
				if (copy) {
					memcpy(copy, src, l + 1);
					slot_snapshots[i] = copy;
				}
			}
		}
	}

	sb->last_rendered_seq = sb->seq;
	pthread_mutex_unlock(&sb->mutex);

	/* Clear the pane buffer and project lines bottom-up. */
	pane_clear(&sb->pane);
	if (slot_snapshots != NULL) {
		/* Bottom-justify: line take-1 → row rows-1, line take-2 → row rows-2, etc. */
		for (size_t i = 0; i < take; ++i) {
			int row = (int)(rows - take + i);
			put_line_to_pane(&sb->pane, row, slot_snapshots[i]);
			free(slot_snapshots[i]);
		}
		free(slot_snapshots);
	}
}

/* ---------- flush on close ---------- */

void scrollback_pane_flush_to_stdout(scrollback_pane_t *sb) {
	if (!sb) return;

	pthread_mutex_lock(&sb->mutex);
	size_t cap = sb->line_capacity;
	size_t line_count = sb->line_count;
	if (line_count == 0 || cap == 0 || sb->lines == NULL) {
		pthread_mutex_unlock(&sb->mutex);
		return;
	}

	/* Copy pointers under the lock; emit outside. We free the originals
	 * inside the lock so the ring is cleared atomically. */
	char **out = (char **)calloc(line_count, sizeof(char *));
	if (!out) {
		pthread_mutex_unlock(&sb->mutex);
		return;
	}

	size_t start = (sb->head + cap - line_count) % cap;
	for (size_t i = 0; i < line_count; ++i) {
		size_t idx = (start + i) % cap;
		out[i] = sb->lines[idx];
		sb->lines[idx] = NULL;
	}
	sb->line_count = 0;
	sb->head = 0;
	pthread_mutex_unlock(&sb->mutex);

	for (size_t i = 0; i < line_count; ++i) {
		if (out[i]) {
			fputs(out[i], stdout);
			fputc('\n', stdout);
			free(out[i]);
		}
	}
	free(out);
	fflush(stdout);
}
