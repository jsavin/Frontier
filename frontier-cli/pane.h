/*
 * pane.h - REPL pane compositor + SGR 1006 mouse parser.
 *
 * Pane substrate for the slash-menu REPL palette (plan §1.3a). Each pane
 * owns a w*h cell_t buffer; the compositor renders all registered panes
 * back-to-front into a screen-sized framebuffer, then diffs against the
 * previous frame and emits ANSI cursor-positioning + cell content for
 * changed cells only. Z-order is implicit: list order = z-order
 * (head = bottom, tail = top). pane_raise() moves a pane to the tail.
 *
 * Scope is intentionally narrow: no tabs, splits, layouts, themes,
 * animations, or wide-character handling. PR 5 owns event routing — the
 * compositor only provides hit-testing via compositor_pane_at().
 *
 * Mouse parser handles xterm SGR 1006 (ESC[<Cb;Cx;Cy(M|m)). Buffer-bounded
 * — the caller's slice may not be NUL-terminated.
 */

#ifndef FRONTIER_PANE_H
#define FRONTIER_PANE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* A single screen cell. ch is a Unicode codepoint (UTF-8 encoded on
 * output). attr is a small bitmap for bold/dim/inverted/underline; the
 * exact encoding is internal to pane.c (PR 5 will consolidate this with
 * terminal_set_attr()). */
typedef struct cell {
	uint32_t ch;
	uint8_t fg;
	uint8_t bg;
	uint8_t attr;
} cell_t;

/* Forward declaration for the callback typedefs below. */
struct pane;

typedef void (*pane_mouse_cb)(struct pane *p, int btn, int x, int y, bool press);
typedef void (*pane_key_cb)(struct pane *p, int key);

typedef struct pane {
	int x, y, w, h;             /* position (screen cells) and size */
	int z;                      /* informational; ordering is by list position */
	char title[64];
	bool has_border;
	bool focused;
	cell_t *buf;                /* w*h cell buffer (owned) */
	struct pane *parent;
	struct pane *next;          /* intrusive z-sorted list (head=bottom) */
	void *userdata;
	pane_mouse_cb on_mouse;
	pane_key_cb on_key;
} pane_t;

/* Pane lifecycle. */
void pane_init(pane_t *p, int x, int y, int w, int h);
void pane_destroy(pane_t *p);
void pane_set_title(pane_t *p, const char *title);

/* Content writes. Out-of-bounds (x,y) is silently clipped — never crashes. */
void pane_putc(pane_t *p, int x, int y, uint32_t ch, uint8_t attr);
void pane_puts(pane_t *p, int x, int y, const char *s, uint8_t attr);
void pane_clear(pane_t *p);

/* Geometry. resize allocates a new zeroed buffer; the prior buffer is freed. */
void pane_move(pane_t *p, int x, int y);
void pane_resize(pane_t *p, int w, int h);

/* Z-order: detach from list (if registered) and append at tail (top). */
void pane_raise(pane_t *p);

/* Compositor. */
void compositor_register(pane_t *p);
void compositor_unregister(pane_t *p);
void compositor_render(void);
void compositor_on_resize(int rows, int cols);
pane_t *compositor_pane_at(int x, int y);

/* SGR 1006 mouse-event protocol parser. */
typedef enum {
	MOUSE_LEFT,
	MOUSE_MIDDLE,
	MOUSE_RIGHT,
	MOUSE_WHEEL_UP,
	MOUSE_WHEEL_DOWN
} mouse_btn_t;

typedef struct {
	mouse_btn_t btn;
	int x, y;       /* 1-based terminal coordinates */
	bool press;     /* true if final byte is 'M', false if 'm' */
} mouse_event_t;

bool mouse_parse(const char *seq, size_t len, mouse_event_t *out);

#endif /* FRONTIER_PANE_H */
