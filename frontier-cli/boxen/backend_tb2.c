/*
 * backend_tb2.c -- termbox2 backend shim for boxen.
 *
 * This is the ONLY file in the boxen subsystem that may reference tb_* symbols
 * or include termbox2.h. All other boxen source files work through the
 * boxen_backend_t vtable and must never use termbox2 directly.
 *
 * Responsibilities:
 *   - Translate TB_KEY_* -> BOXEN_KEY_* for special keys
 *   - Translate TB_MOD_* -> BOXEN_MOD_* modifiers
 *   - Translate TB_EVENT_* -> BOXEN_EV_* event types
 *   - Translate TB_KEY_MOUSE_* -> BOXEN_EV_MOUSE with button field
 *   - Synthesize double-click using tracked timestamp + position state
 *   - Translate BOXEN_ATTR_* -> TB_BOLD / TB_UNDERLINE / TB_REVERSE / TB_ITALIC
 *
 * Key translation notes (per EXECUTION_PLAN.md Section 6):
 *   Tab/Enter/Esc/Backspace are translated BEFORE the generic CTRL_* range.
 *   TB_KEY_TAB   (0x09) -> BOXEN_KEY_TAB   (not BOXEN_KEY_CTRL_I)
 *   TB_KEY_ENTER (0x0d) -> BOXEN_KEY_ENTER (not BOXEN_KEY_CTRL_M)
 *   TB_KEY_ESC   (0x1b) -> BOXEN_KEY_ESCAPE (not BOXEN_KEY_CTRL_BRACKET)
 *   TB_KEY_BACKSPACE (0x08) -> BOXEN_KEY_BACKSPACE (not BOXEN_KEY_CTRL_H)
 */

#include "termbox2.h"
#include "boxen_internal.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

/* -------------------------------------------------------------------------
 * Clock for double-click synthesis
 * ---------------------------------------------------------------------- */

static uint64_t tb2_now_ms(void) {
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

/* -------------------------------------------------------------------------
 * Double-click state
 * ---------------------------------------------------------------------- */

static struct {
	uint64_t time_ms;
	int      x;
	int      y;
	uint8_t  button;
	bool     valid;
} g_tb2_last_press;

static void tb2_maybe_set_double_click(boxen_event_t *ev) {
	if (!ev->mouse.pressed) {
		return;
	}

	uint64_t now = tb2_now_ms();

	if (g_tb2_last_press.valid) {
		uint64_t elapsed = now - g_tb2_last_press.time_ms;
		int dx = ev->mouse.x - g_tb2_last_press.x;
		int dy = ev->mouse.y - g_tb2_last_press.y;
		if (dx < 0) dx = -dx;
		if (dy < 0) dy = -dy;
		int dist = dx > dy ? dx : dy;

		if (elapsed <= BOXEN_DOUBLE_CLICK_MS
		    && dist  <= BOXEN_DOUBLE_CLICK_RADIUS
		    && ev->mouse.button == g_tb2_last_press.button) {
			ev->mouse.flags |= BOXEN_MOUSE_DOUBLE_CLICK;
			g_tb2_last_press.valid = false;
			return;
		}
	}

	g_tb2_last_press.time_ms = now;
	g_tb2_last_press.x       = ev->mouse.x;
	g_tb2_last_press.y       = ev->mouse.y;
	g_tb2_last_press.button  = ev->mouse.button;
	g_tb2_last_press.valid   = true;
}

/* -------------------------------------------------------------------------
 * TB_KEY_* -> BOXEN_KEY_* translation
 *
 * Order matters: check explicit aliases (Tab, Enter, Esc, Backspace) FIRST,
 * before the generic CTRL_* numeric range check, because several TB_KEY_*
 * values are assigned the same numeric value (e.g., TB_KEY_TAB == TB_KEY_CTRL_I).
 * ---------------------------------------------------------------------- */

static boxen_key_t translate_key(uint16_t tb_key, uint32_t tb_ch) {
	/* Printable character: key field is 0, ch carries the codepoint. */
	if (tb_ch != 0) {
		return BOXEN_KEY_NONE;
	}

	/* Named aliases FIRST -- must precede the generic CTRL_* range */
	switch (tb_key) {
		case TB_KEY_TAB:       return BOXEN_KEY_TAB;
		case TB_KEY_ENTER:     return BOXEN_KEY_ENTER;
		case TB_KEY_ESC:       return BOXEN_KEY_ESCAPE;
		case TB_KEY_BACKSPACE: return BOXEN_KEY_BACKSPACE;
		case TB_KEY_BACKSPACE2:return BOXEN_KEY_BACKSPACE;
		case TB_KEY_SPACE:     return BOXEN_KEY_NONE;  /* space is printable via ch */

		/* Function keys */
		case TB_KEY_F1:        return BOXEN_KEY_F1;
		case TB_KEY_F2:        return BOXEN_KEY_F2;
		case TB_KEY_F3:        return BOXEN_KEY_F3;
		case TB_KEY_F4:        return BOXEN_KEY_F4;
		case TB_KEY_F5:        return BOXEN_KEY_F5;
		case TB_KEY_F6:        return BOXEN_KEY_F6;
		case TB_KEY_F7:        return BOXEN_KEY_F7;
		case TB_KEY_F8:        return BOXEN_KEY_F8;
		case TB_KEY_F9:        return BOXEN_KEY_F9;
		case TB_KEY_F10:       return BOXEN_KEY_F10;
		case TB_KEY_F11:       return BOXEN_KEY_F11;
		case TB_KEY_F12:       return BOXEN_KEY_F12;

		/* Navigation */
		case TB_KEY_ARROW_UP:    return BOXEN_KEY_UP;
		case TB_KEY_ARROW_DOWN:  return BOXEN_KEY_DOWN;
		case TB_KEY_ARROW_LEFT:  return BOXEN_KEY_LEFT;
		case TB_KEY_ARROW_RIGHT: return BOXEN_KEY_RIGHT;
		case TB_KEY_HOME:        return BOXEN_KEY_HOME;
		case TB_KEY_END:         return BOXEN_KEY_END;
		case TB_KEY_PGUP:        return BOXEN_KEY_PGUP;
		case TB_KEY_PGDN:        return BOXEN_KEY_PGDN;
		case TB_KEY_INSERT:      return BOXEN_KEY_INSERT;
		case TB_KEY_DELETE:      return BOXEN_KEY_DELETE;

		default:
			break;
	}

	/* Generic CTRL_* range (1-26) -- AFTER the aliases above */
	if (tb_key >= 0x01 && tb_key <= 0x1a) {
		/* Map 0x01 -> CTRL_A (index 0), 0x1a -> CTRL_Z (index 25) */
		return (boxen_key_t)(BOXEN_KEY_CTRL_A + (tb_key - 0x01));
	}
	if (tb_key == TB_KEY_CTRL_BACKSLASH) {
		return BOXEN_KEY_CTRL_BACKSLASH;
	}
	if (tb_key == 0x1b) {
		/* ESC / CTRL_LSQ_BRACKET -- handled above as BOXEN_KEY_ESCAPE */
		return BOXEN_KEY_ESCAPE;
	}
	if (tb_key == TB_KEY_CTRL_TILDE) {
		return BOXEN_KEY_CTRL_SPACE;
	}

	return BOXEN_KEY_NONE;
}

/* -------------------------------------------------------------------------
 * Attribute translation: BOXEN_ATTR_* -> termbox2 uintattr_t bits
 * ---------------------------------------------------------------------- */

static uintattr_t translate_attr(uint16_t boxen_attr) {
	uintattr_t ta = 0;
	if (boxen_attr & BOXEN_ATTR_BOLD)      ta |= TB_BOLD;
	if (boxen_attr & BOXEN_ATTR_UNDERLINE) ta |= TB_UNDERLINE;
	if (boxen_attr & BOXEN_ATTR_REVERSE)   ta |= TB_REVERSE;
	if (boxen_attr & BOXEN_ATTR_ITALIC)    ta |= TB_ITALIC;
	return ta;
}

/* -------------------------------------------------------------------------
 * Color translation: BOXEN_COLOR_* -> termbox2 uintattr_t color value
 *
 * Today BOXEN_COLOR_DEFAULT (0) and BOXEN_COLOR_BLACK..WHITE (1..8) happen to
 * have the same numeric values as TB_DEFAULT and TB_BLACK..TB_WHITE. This
 * function exists to make that coincidence explicit and local: if termbox2
 * ever renumbers its color constants, only this function needs to change.
 * ---------------------------------------------------------------------- */

static uintattr_t translate_color(boxen_color_t c) {
	return (uintattr_t)c;
}

/* -------------------------------------------------------------------------
 * Modifier translation: TB_MOD_* -> BOXEN_MOD_*
 * ---------------------------------------------------------------------- */

static uint16_t translate_mod(uint8_t tb_mod) {
	uint16_t m = 0;
	if (tb_mod & TB_MOD_ALT)   m |= BOXEN_MOD_ALT;
	if (tb_mod & TB_MOD_CTRL)  m |= BOXEN_MOD_CTRL;
	if (tb_mod & TB_MOD_SHIFT) m |= BOXEN_MOD_SHIFT;
	return m;
}

/* -------------------------------------------------------------------------
 * Mouse event translation
 *
 * termbox2 encodes mouse events as TB_EVENT_MOUSE with a key field set to
 * one of the TB_KEY_MOUSE_* constants. Release has no button identity.
 * ---------------------------------------------------------------------- */

static void translate_mouse(const struct tb_event *te, boxen_event_t *out) {
	out->type  = BOXEN_EV_MOUSE;
	out->mouse.x     = te->x;
	out->mouse.y     = te->y;
	out->mouse.mod   = translate_mod(te->mod);
	out->mouse.flags = 0;

	switch (te->key) {
		case TB_KEY_MOUSE_LEFT:
			out->mouse.button  = 1;
			out->mouse.pressed = true;
			break;
		case TB_KEY_MOUSE_RIGHT:
			out->mouse.button  = 3;
			out->mouse.pressed = true;
			break;
		case TB_KEY_MOUSE_MIDDLE:
			out->mouse.button  = 2;
			out->mouse.pressed = true;
			break;
		case TB_KEY_MOUSE_RELEASE:
			out->mouse.button  = 0;
			out->mouse.pressed = false;
			break;
		case TB_KEY_MOUSE_WHEEL_UP:
			out->mouse.button  = 4;
			out->mouse.pressed = true;
			break;
		case TB_KEY_MOUSE_WHEEL_DOWN:
			out->mouse.button  = 5;
			out->mouse.pressed = true;
			break;
		default:
			out->mouse.button  = 0;
			out->mouse.pressed = false;
			break;
	}

	if (out->mouse.pressed) {
		tb2_maybe_set_double_click(out);
	}
}

/* -------------------------------------------------------------------------
 * Backend vtable implementation
 * ---------------------------------------------------------------------- */

static int tb2_init(void *config) {
	(void)config;
	memset(&g_tb2_last_press, 0, sizeof(g_tb2_last_press));
	int r = tb_init();
	return (r == TB_OK) ? BOXEN_OK : BOXEN_ERR_INIT;
}

/* Last cursor position passed to tb_set_cursor(). Used by
 * tb2_set_cursor_visible(true) to restore visibility after a hide_cursor() --
 * termbox2 has no symmetric show_cursor() API; the only way to bring it back
 * is to re-issue tb_set_cursor() with valid coordinates. */
static int g_tb2_cursor_x = 0;
static int g_tb2_cursor_y = 0;

static void tb2_shutdown(void) {
	/* Symmetric with init: clear double-click state so a re-init starts fresh
	 * even if init's memset is ever removed or the cycle is reused. */
	memset(&g_tb2_last_press, 0, sizeof(g_tb2_last_press));
	g_tb2_cursor_x = 0;
	g_tb2_cursor_y = 0;
	tb_shutdown();
}

static int tb2_width(void) {
	return tb_width();
}

static int tb2_height(void) {
	return tb_height();
}

static int tb2_color_depth(void) {
	/* termbox2 v2.5.0 exposes color-capability via output mode queries.
	 * For A.1, return 256 as a safe default (most modern terminals support it).
	 * A future enhancement can call tb_set_output_mode and query the result. */
	return 256;
}

static void tb2_set_cell(int x, int y, uint32_t ch,
                         uint16_t fg, uint16_t bg, uint16_t attr) {
	uintattr_t ta  = translate_attr(attr);
	uintattr_t tfg = translate_color(fg);
	uintattr_t tbg = translate_color(bg);
	tb_set_cell(x, y, ch, tfg | ta, tbg);
}

static void tb2_set_cursor(int x, int y) {
	g_tb2_cursor_x = x;
	g_tb2_cursor_y = y;
	tb_set_cursor(x, y);
}

static void tb2_set_cursor_visible(bool visible) {
	if (visible) {
		/* Restore visibility by re-issuing the last known position.
		 * termbox2 has no dedicated tb_show_cursor(); tb_set_cursor() with
		 * valid coords is the canonical re-show idiom. */
		tb_set_cursor(g_tb2_cursor_x, g_tb2_cursor_y);
	} else {
		tb_hide_cursor();
	}
}

static void tb2_present(void) {
	tb_present();
}

static int tb2_poll_event(boxen_event_t *out, int timeout_ms) {
	struct tb_event te;
	int r;

	memset(out, 0, sizeof(*out));

	if (timeout_ms == 0) {
		/* Non-blocking peek */
		r = tb_peek_event(&te, 0);
	} else {
		r = tb_peek_event(&te, timeout_ms);
	}

	if (r == TB_ERR_NO_EVENT || r == TB_ERR_POLL) {
		return BOXEN_ERR_TIMEOUT;
	}
	if (r != TB_OK) {
		return BOXEN_ERR_IO;
	}

	switch (te.type) {
		case TB_EVENT_KEY:
			out->type    = BOXEN_EV_KEY;
			out->key.key = translate_key(te.key, te.ch);
			out->key.ch  = te.ch;
			out->key.mod = translate_mod(te.mod);
			break;
		case TB_EVENT_MOUSE:
			translate_mouse(&te, out);
			break;
		case TB_EVENT_RESIZE:
			out->type     = BOXEN_EV_RESIZE;
			out->resize.w = te.w;
			out->resize.h = te.h;
			break;
		default:
			out->type = BOXEN_EV_NONE;
			break;
	}

	return BOXEN_OK;
}

static void tb2_clear(void) {
	tb_clear();
}

/* -------------------------------------------------------------------------
 * Vtable and accessor
 * ---------------------------------------------------------------------- */

static const boxen_backend_t g_tb2_backend = {
	.init               = tb2_init,
	.shutdown           = tb2_shutdown,
	.width              = tb2_width,
	.height             = tb2_height,
	.color_depth        = tb2_color_depth,
	.set_cell           = tb2_set_cell,
	.set_cursor         = tb2_set_cursor,
	.set_cursor_visible = tb2_set_cursor_visible,
	.present            = tb2_present,
	.poll_event         = tb2_poll_event,
	.clear              = tb2_clear,
};

const boxen_backend_t *boxen_tb2_backend(void) {
	return &g_tb2_backend;
}
