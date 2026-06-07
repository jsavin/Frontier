/*
 * backend_mock.h -- test-only API for the boxen in-memory mock backend.
 *
 * Include this from test files alongside boxen.h. NEVER include this from
 * production code or from boxen_internal.h.
 *
 * The mock backend implements the boxen_backend_t vtable using an in-memory
 * cell grid and a FIFO event queue. It synthesizes double-click events using
 * an injectable clock function so tests can control timing deterministically.
 *
 * Threading: like the rest of boxen, the mock is NOT thread-safe. Tests must
 * call mock APIs from a single thread. The clock-injection function pointer
 * uses plain stores (no atomics); install the custom clock during test setup
 * BEFORE any backend calls.
 *
 * Capacity: the cell grid is statically sized at MOCK_MAX_WIDTH x
 * MOCK_MAX_HEIGHT (512 x 256 internally). Calls to boxen_mock_reset with
 * larger dimensions clamp to these maxima. The event queue holds at most
 * MOCK_MAX_EVENTS (256); pushes beyond capacity silently drop the OLDEST
 * event to make room for the new one (see boxen_mock_push_event).
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

/* Change the reported width/height. The cell grid is statically sized at the
 * compile-time max (MOCK_MAX_WIDTH x MOCK_MAX_HEIGHT); these setters only
 * adjust the active region reported via backend->width()/height(). To change
 * grid contents, use boxen_mock_reset(). */
void boxen_mock_width_set(int w);
void boxen_mock_height_set(int h);

/* Read a cell from the mock's "screen" (the composited output written by
 * the last set_cell calls, after present). Returns NULL if out of bounds. */
const boxen_mock_cell_t *boxen_mock_cell_at(int x, int y);

/* Returns true if the ASCII string s appears consecutively on any row.
 * ASCII-only: the comparison is byte-against-codepoint. UTF-8 multi-byte
 * inputs will not match correctly -- pass ASCII literals only. */
bool boxen_mock_has_text(const char *s);

/* -------------------------------------------------------------------------
 * Cursor inspection (A.7+)
 *
 * Tests for boxen_window_set_cursor / boxen_window_set_cursor_visible read
 * back the most recent backend call via these accessors. The mock tracks
 * the last (x, y) passed to backend->set_cursor and the last visibility
 * flag passed to backend->set_cursor_visible. boxen_mock_reset() resets
 * both to (-1, -1) and false respectively, matching the terminal's
 * initial state after init.
 * ---------------------------------------------------------------------- */

/* Returns the last cursor (x, y) set via backend->set_cursor.
 * Writes the screen-absolute coordinates passed by the last call.
 * Returns -1, -1 if set_cursor was never called since boxen_mock_reset. */
void boxen_mock_get_cursor(int *x, int *y);

/* Returns the last visibility passed to backend->set_cursor_visible.
 * Defaults to false (matches terminal initial state after init). */
bool boxen_mock_cursor_visible(void);

/* -------------------------------------------------------------------------
 * Event injection
 * ---------------------------------------------------------------------- */

/* Inject a raw event into the FIFO queue.
 *
 * Queue capacity is MOCK_MAX_EVENTS (256). When the queue is full, pushing a
 * new event silently drops the OLDEST queued event to make room. Tests that
 * push >256 events without polling will lose the head of the queue in FIFO
 * order; design tests to poll between pushes when ordering past 256 matters. */
void boxen_mock_push_event(const boxen_event_t *ev);

/* Convenience: inject a key event. */
void boxen_mock_push_key(boxen_key_t key, uint32_t ch, uint16_t mod);

/* Convenience: inject a mouse press/release event.
 * Does NOT synthesize double-click -- use boxen_mock_push_double_click for that. */
void boxen_mock_push_mouse(int x, int y, uint8_t button, bool pressed, uint16_t mod);

/* Convenience: inject a double-click at (x, y) with the given mod.
 * Internally queues two press events at the same coordinate and relies on the
 * currently-installed clock function returning monotonically increasing values
 * (default clock advances 1 ms per call -- well inside the double-click
 * window). If you install a custom clock that returns identical or
 * out-of-order values, prefer composing boxen_mock_push_mouse + boxen_mock_
 * set_now_ms_fn directly (see tests 4-7 for the canonical pattern). */
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
