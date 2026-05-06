/*
 * pane.c - REPL pane compositor + SGR 1006 mouse parser.
 *
 * See pane.h for the public-API contract and §1.3a of plan
 * humming-painting-hejlsberg.md for the design rationale.
 *
 * Compositor data model
 * ---------------------
 * - Single linked list of registered panes, head = bottom z, tail = top z.
 * - One screen-sized framebuffer (current frame) and one previous-frame
 *   buffer for diff rendering. Both reallocated by compositor_on_resize().
 * - compositor_render() walks the list head→tail, copying each pane's
 *   cell buffer into the framebuffer at (pane.x, pane.y) with rect
 *   clipping. Then it diffs current vs. previous, emitting CUP + cell
 *   bytes only where cells changed, and finally swaps the buffers.
 *
 * SGR encoding
 * ------------
 * Output uses a minimal SGR emitter. PR 5 (TODO) will replace this with
 * terminal_set_attr() once that helper lands in PR 3 — kept inline here so
 * PR 4 has no source-level dependency on PR 3.
 *
 * Test hooks
 * ----------
 * compositor_test_fb_at / _size / _reset are exposed (not in pane.h) for
 * unit tests in tests/pane_compositor_tests.c. They inspect the
 * framebuffer state without depending on the SGR byte stream.
 */

#include "pane.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------- internal compositor state ----------
 *
 * These globals are intentionally unlocked. The compositor is a
 * single-threaded surface (REPL main thread only); background producers
 * must marshal through a main-thread queue before touching panes. See
 * pane.h "Threading" for the full contract.
 */

static pane_t *g_pane_list_head = NULL;  /* bottom z */
static pane_t *g_pane_list_tail = NULL;  /* top z */

static cell_t *g_fb_curr = NULL;
static cell_t *g_fb_prev = NULL;
static int g_fb_rows = 0;
static int g_fb_cols = 0;

/* ---------- helpers ----------
 *
 * The list helpers below are O(n) in the number of registered panes.
 * That's intentional: the slash-menu surface is expected to host on the
 * order of 5-10 panes (REPL output, prompt, palette, hint footer, plus
 * any transient overlays). If the pane count grows, revisit with a
 * doubly-linked list + tail pointer maintained on remove.
 */

/* Allocate a zero-initialised cell array of `count` cells.
 * Promotes to size_t before any multiplication so the math is safe even
 * for theoretical large counts; calloc itself also guards against
 * size_t overflow. Returns NULL on OOM (callers degrade to a no-op). */
static cell_t *alloc_cells(size_t count) {
	if (count == 0) return NULL;
	return (cell_t *)calloc(count, sizeof(cell_t));
}

static void list_remove(pane_t *p) {
	if (!p) return;
	pane_t *prev = NULL;
	for (pane_t *cur = g_pane_list_head; cur != NULL; cur = cur->next) {
		if (cur == p) {
			if (prev) prev->next = cur->next;
			else g_pane_list_head = cur->next;
			if (g_pane_list_tail == cur) g_pane_list_tail = prev;
			cur->next = NULL;
			return;
		}
		prev = cur;
	}
}

static void list_append(pane_t *p) {
	if (!p) return;
	p->next = NULL;
	if (!g_pane_list_head) {
		g_pane_list_head = p;
		g_pane_list_tail = p;
		return;
	}
	g_pane_list_tail->next = p;
	g_pane_list_tail = p;
}

static bool list_contains(pane_t *p) {
	for (pane_t *cur = g_pane_list_head; cur != NULL; cur = cur->next) {
		if (cur == p) return true;
	}
	return false;
}

/* ---------- pane lifecycle ---------- */

void pane_init(pane_t *p, int x, int y, int w, int h) {
	if (!p) return;
	memset(p, 0, sizeof(*p));
	p->x = x;
	p->y = y;
	p->w = (w > 0) ? w : 0;
	p->h = (h > 0) ? h : 0;
	p->z = 0;
	if (p->w > 0 && p->h > 0) {
		p->buf = alloc_cells((size_t)p->w * (size_t)p->h);
	}
}

void pane_destroy(pane_t *p) {
	if (!p) return;
	free(p->buf);
	p->buf = NULL;
	p->w = 0;
	p->h = 0;
	p->next = NULL;
	p->parent = NULL;
}

void pane_set_title(pane_t *p, const char *title) {
	if (!p) return;
	if (!title) {
		p->title[0] = '\0';
		return;
	}
	size_t n = sizeof(p->title) - 1;
	strncpy(p->title, title, n);
	p->title[n] = '\0';
}

/* ---------- pane content writes ---------- */

void pane_putc(pane_t *p, int x, int y, uint32_t ch, uint8_t attr) {
	if (!p || !p->buf) return;
	if (x < 0 || x >= p->w) return;
	if (y < 0 || y >= p->h) return;
	cell_t *c = &p->buf[y * p->w + x];
	c->ch = ch;
	c->attr = attr;
}

void pane_puts(pane_t *p, int x, int y, const char *s, uint8_t attr) {
	if (!p || !p->buf || !s) return;
	if (y < 0 || y >= p->h) return;
	int col = x;
	while (*s != '\0' && col < p->w) {
		if (col >= 0) {
			pane_putc(p, col, y, (uint32_t)(unsigned char)*s, attr);
		}
		++s;
		++col;
	}
}

void pane_clear(pane_t *p) {
	if (!p || !p->buf) return;
	memset(p->buf, 0, (size_t)(p->w * p->h) * sizeof(cell_t));
}

/* ---------- pane geometry ---------- */

void pane_move(pane_t *p, int x, int y) {
	if (!p) return;
	p->x = x;
	p->y = y;
}

void pane_resize(pane_t *p, int w, int h) {
	if (!p) return;
	int new_w = (w > 0) ? w : 0;
	int new_h = (h > 0) ? h : 0;
	free(p->buf);
	p->buf = NULL;
	p->w = new_w;
	p->h = new_h;
	if (new_w > 0 && new_h > 0) {
		p->buf = alloc_cells((size_t)new_w * (size_t)new_h);
	}
}

void pane_raise(pane_t *p) {
	if (!p) return;
	if (!list_contains(p)) return;
	if (g_pane_list_tail == p) return;  /* already on top */
	list_remove(p);
	list_append(p);
}

/* ---------- compositor ---------- */

void compositor_register(pane_t *p) {
	if (!p) return;
	if (list_contains(p)) return;  /* idempotent */
	list_append(p);
}

void compositor_unregister(pane_t *p) {
	if (!p) return;
	if (!list_contains(p)) return;  /* idempotent */
	list_remove(p);
}

void compositor_on_resize(int rows, int cols) {
	if (rows <= 0 || cols <= 0) {
		free(g_fb_curr);
		free(g_fb_prev);
		g_fb_curr = g_fb_prev = NULL;
		g_fb_rows = g_fb_cols = 0;
		return;
	}
	free(g_fb_curr);
	free(g_fb_prev);
	g_fb_rows = rows;
	g_fb_cols = cols;
	g_fb_curr = alloc_cells((size_t)rows * (size_t)cols);
	g_fb_prev = alloc_cells((size_t)rows * (size_t)cols);
	/* Mark prev as a sentinel "definitely different" frame so the next
	 * render emits everything. We use ch=0 in prev and a fresh zeroed
	 * curr; the diff will skip identical zero cells, which is the
	 * desired behaviour: a freshly resized terminal needn't repaint
	 * unwritten cells until something draws there. */
}

pane_t *compositor_pane_at(int x, int y) {
	/* Walk front-to-back: highest z (tail) wins. The list is singly
	 * linked, so do a linear sweep recording the latest match. */
	pane_t *hit = NULL;
	for (pane_t *cur = g_pane_list_head; cur != NULL; cur = cur->next) {
		if (x >= cur->x && x < cur->x + cur->w &&
		    y >= cur->y && y < cur->y + cur->h) {
			hit = cur;  /* later list entries override earlier ones */
		}
	}
	return hit;
}

/* Copy one pane's buffer into the framebuffer at (pane.x, pane.y),
 * clipping against framebuffer bounds. Only non-zero source cells
 * overwrite destination cells — zero source cells are treated as
 * transparent so a smaller top pane doesn't erase background panes
 * outside its own filled area. (For the documented unit tests, panes
 * are filled densely so this distinction is invisible. The transparency
 * rule is what makes "two overlapping panes" composition predictable.) */
static void blit_pane(const pane_t *p) {
	if (!p || !p->buf) return;
	for (int row = 0; row < p->h; ++row) {
		int dy = p->y + row;
		if (dy < 0 || dy >= g_fb_rows) continue;
		for (int col = 0; col < p->w; ++col) {
			int dx = p->x + col;
			if (dx < 0 || dx >= g_fb_cols) continue;
			const cell_t *src = &p->buf[row * p->w + col];
			if (src->ch == 0) continue;  /* transparent */
			g_fb_curr[dy * g_fb_cols + dx] = *src;
		}
	}
}

/* Minimal SGR emitter — TODO(PR 5): replace with terminal_set_attr().
 * For now we just emit fg/bg as 8-color SGR codes when non-zero, and
 * the bold/inverted/underline bits if attr has them set. */
static void emit_attr(FILE *fp, uint8_t fg, uint8_t bg, uint8_t attr) {
	/* SGR 0 reset, then accumulate. Keep deterministic order. */
	fputs("\x1b[0", fp);
	if (attr & 0x01) fputs(";1", fp);    /* bold */
	if (attr & 0x02) fputs(";2", fp);    /* dim */
	if (attr & 0x04) fputs(";4", fp);    /* underline */
	if (attr & 0x08) fputs(";7", fp);    /* inverted */
	if (fg != 0) {
		fprintf(fp, ";3%u", (unsigned)(fg & 0x07));
	}
	if (bg != 0) {
		fprintf(fp, ";4%u", (unsigned)(bg & 0x07));
	}
	fputc('m', fp);
}

void compositor_render(void) {
	if (g_fb_rows <= 0 || g_fb_cols <= 0 || !g_fb_curr || !g_fb_prev) return;

	/* Step 1: clear current framebuffer (fresh frame). */
	memset(g_fb_curr, 0, (size_t)(g_fb_rows * g_fb_cols) * sizeof(cell_t));

	/* Step 2: composite all panes back-to-front. */
	for (pane_t *cur = g_pane_list_head; cur != NULL; cur = cur->next) {
		blit_pane(cur);
	}

	/* Step 3: diff vs. previous frame and emit changes. */
	for (int y = 0; y < g_fb_rows; ++y) {
		for (int x = 0; x < g_fb_cols; ++x) {
			int idx = y * g_fb_cols + x;
			cell_t c = g_fb_curr[idx];
			cell_t p = g_fb_prev[idx];
			if (c.ch == p.ch && c.fg == p.fg && c.bg == p.bg && c.attr == p.attr) {
				continue;
			}
			/* CUP: rows/cols are 1-based in ANSI. */
			fprintf(stdout, "\x1b[%d;%dH", y + 1, x + 1);
			emit_attr(stdout, c.fg, c.bg, c.attr);
			if (c.ch == 0) {
				fputc(' ', stdout);
			} else if (c.ch < 0x80) {
				fputc((int)c.ch, stdout);
			} else {
				/* Minimal UTF-8 encoder for the BMP/SMP range. */
				uint32_t ch = c.ch;
				if (ch < 0x800) {
					fputc((int)(0xC0 | (ch >> 6)), stdout);
					fputc((int)(0x80 | (ch & 0x3F)), stdout);
				} else if (ch < 0x10000) {
					fputc((int)(0xE0 | (ch >> 12)), stdout);
					fputc((int)(0x80 | ((ch >> 6) & 0x3F)), stdout);
					fputc((int)(0x80 | (ch & 0x3F)), stdout);
				} else {
					fputc((int)(0xF0 | (ch >> 18)), stdout);
					fputc((int)(0x80 | ((ch >> 12) & 0x3F)), stdout);
					fputc((int)(0x80 | ((ch >> 6) & 0x3F)), stdout);
					fputc((int)(0x80 | (ch & 0x3F)), stdout);
				}
			}
		}
	}
	fflush(stdout);

	/* Step 4: promote current → previous for next diff. */
	memcpy(g_fb_prev, g_fb_curr, (size_t)(g_fb_rows * g_fb_cols) * sizeof(cell_t));
}

/* ---------- test-only inspection hooks (not in pane.h) ---------- */

const cell_t *compositor_test_fb_at(int x, int y) {
	if (!g_fb_curr) return NULL;
	if (x < 0 || x >= g_fb_cols || y < 0 || y >= g_fb_rows) return NULL;
	return &g_fb_curr[y * g_fb_cols + x];
}

void compositor_test_fb_size(int *rows, int *cols) {
	if (rows) *rows = g_fb_rows;
	if (cols) *cols = g_fb_cols;
}

void compositor_test_reset(void) {
	g_pane_list_head = NULL;
	g_pane_list_tail = NULL;
	free(g_fb_curr);
	free(g_fb_prev);
	g_fb_curr = NULL;
	g_fb_prev = NULL;
	g_fb_rows = 0;
	g_fb_cols = 0;
}

/* ---------- SGR 1006 mouse parser ----------
 *
 * Format: ESC [ < Cb ; Cx ; Cy ('M' | 'm')
 * Cb: button index (low 2 bits) | wheel bit 6 | modifier bits.
 * For PR 4 we only care about: 0=left, 1=middle, 2=right, 64=wheel up,
 * 65=wheel down. Other values (modifier-encoded buttons, motion events)
 * map to MOUSE_LEFT for now — PR 5 may extend this.
 */

/* Parse a non-negative decimal integer starting at *pos within [seq, end).
 * On success: writes the value to *out, advances *pos, returns true.
 * Empty sequence (no digits) is rejected. */
static bool parse_uint(const char *seq, size_t end, size_t *pos, int *out) {
	if (*pos >= end) return false;
	if (!isdigit((unsigned char)seq[*pos])) return false;
	int v = 0;
	while (*pos < end && isdigit((unsigned char)seq[*pos])) {
		int d = seq[*pos] - '0';
		if (v > (INT32_MAX - d) / 10) return false;  /* overflow */
		v = v * 10 + d;
		++(*pos);
	}
	*out = v;
	return true;
}

bool mouse_parse(const char *seq, size_t len, mouse_event_t *out) {
	if (!seq || !out) return false;
	if (len < 6) return false;  /* minimum: ESC [ < d ; d ; d M */
	if (seq[0] != 0x1b) return false;
	if (seq[1] != '[') return false;
	if (seq[2] != '<') return false;

	size_t pos = 3;
	int cb = 0, cx = 0, cy = 0;
	if (!parse_uint(seq, len, &pos, &cb)) return false;
	if (pos >= len || seq[pos] != ';') return false;
	++pos;
	if (!parse_uint(seq, len, &pos, &cx)) return false;
	if (pos >= len || seq[pos] != ';') return false;
	++pos;
	if (!parse_uint(seq, len, &pos, &cy)) return false;
	if (pos >= len) return false;
	char term = seq[pos];
	if (term != 'M' && term != 'm') return false;

	mouse_event_t ev;
	memset(&ev, 0, sizeof(ev));
	ev.x = cx;
	ev.y = cy;
	ev.press = (term == 'M');

	if (cb == 64) {
		ev.btn = MOUSE_WHEEL_UP;
	} else if (cb == 65) {
		ev.btn = MOUSE_WHEEL_DOWN;
	} else {
		switch (cb & 0x03) {
		case 0: ev.btn = MOUSE_LEFT; break;
		case 1: ev.btn = MOUSE_MIDDLE; break;
		case 2: ev.btn = MOUSE_RIGHT; break;
		default: ev.btn = MOUSE_LEFT; break;  /* "release of any" → left */
		}
	}

	*out = ev;
	return true;
}
