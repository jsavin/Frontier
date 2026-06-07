/*
 * backend_mock.h -- test-only API for the boxen in-memory mock backend.
 *
 * Include this from test files alongside boxen.h. NEVER include this from
 * production code or from boxen_internal.h.
 *
 * The mock backend implements the boxen_backend_t vtable using an in-memory
 * cell grid and a FIFO event queue. It synthesizes double-click events using
 * an injectable clock function so tests can control timing deterministically.
 */

#ifndef BACKEND_MOCK_H
#define BACKEND_MOCK_H

#include <stdbool.h>
#include <stdint.h>

#include "boxen.h"

/* -------------------------------------------------------------------------
 * Cell inspection type
 * ---------------------------------------------------------------------- */

typedef struct {
	uint32_t ch;
	uint16_t fg;
	uint16_t bg;
	uint16_t attr;
} boxen_mock_cell_t;

/* -------------------------------------------------------------------------
 * Mock backend control API
 *
 * All functions below are safe to call from test code at any time after
 * boxen_mock_reset() has been called. They do NOT require boxen_init().
 * ---------------------------------------------------------------------- */

/* Reset the mock to a blank WxH screen and empty the event queue.
 * Call this at the start of every test function. */
void boxen_mock_reset(int width, int height);

/* Change the reported width/height (does not affect the cell grid allocation
 * from the most recent reset; use reset to change grid dimensions). */
void boxen_mock_width_set(int w);
void boxen_mock_height_set(int h);

/* Read a cell from the mock's "screen" (the composited output written by
 * the last set_cell calls, after present). Returns NULL if out of bounds. */
const boxen_mock_cell_t *boxen_mock_cell_at(int x, int y);

/* Returns true if the string s appears consecutively on any row. */
bool boxen_mock_has_text(const char *s);

/* -------------------------------------------------------------------------
 * Event injection
 * ---------------------------------------------------------------------- */

/* Inject a raw event into the FIFO queue. */
void boxen_mock_push_event(const boxen_event_t *ev);

/* Convenience: inject a key event. */
void boxen_mock_push_key(boxen_key_t key, uint32_t ch, uint16_t mod);

/* Convenience: inject a mouse press/release event.
 * Does NOT synthesize double-click -- use boxen_mock_push_double_click for that. */
void boxen_mock_push_mouse(int x, int y, uint8_t button, bool pressed, uint16_t mod);

/* Convenience: inject a double-click at (x, y) with the given mod.
 * Internally queues two press events spaced within the double-click window
 * using the injectable clock, so the second one carries BOXEN_MOUSE_DOUBLE_CLICK. */
void boxen_mock_push_double_click(int x, int y, uint16_t mod);

/* -------------------------------------------------------------------------
 * Clock injection for deterministic double-click testing
 *
 * By default the mock uses a monotonically-advancing internal counter
 * (starts at 0; each call advances by 1 ms). Override with a custom
 * function to inject specific timestamps.
 * ---------------------------------------------------------------------- */

/* Install a custom clock. Pass NULL to restore the default counter. */
void boxen_mock_set_now_ms_fn(uint64_t (*fn)(void));

/* -------------------------------------------------------------------------
 * Backend vtable accessor
 * ---------------------------------------------------------------------- */

/* Returns the mock backend vtable, ready to pass to boxen_init(). */
const boxen_backend_t *boxen_mock_backend(void);

#endif /* BACKEND_MOCK_H */
