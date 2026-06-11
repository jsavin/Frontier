/*
 * boxen_palette_backend.h -- 2026-06-10 JES #691 Phase C.0.3b: boxen-window
 * palette render backend.
 *
 * Provides a palette_render_backend_t that renders the slash-menu palette
 * to a boxen_window_t using cell primitives (boxen_draw_text /
 * boxen_fill_rect / boxen_set_cell) instead of writing ANSI sequences to
 * stdout.
 *
 * Contrast with the C.0.2 completion popup (boxen_completion_popup.c):
 *   - Completion popup: boxen_window_raise(), no set_modal.  Key events
 *     stay with the REPL input_win; the popup is a pure PAINT surface.
 *   - Palette: boxen_window_set_modal(true).  The palette IS an input
 *     consumer -- its state machine needs every key.  Modal gives palette
 *     exclusive key capture while it is open.
 *
 * Threading: same contract as boxen.h -- all calls on the GIL-holding thread.
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Frontier contributors.  See pane.h for license text.
 */

#ifndef BOXEN_PALETTE_BACKEND_H
#define BOXEN_PALETTE_BACKEND_H

#include <stdbool.h>

#include "palette.h"
#include "boxen/boxen.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------
 * Render backend accessor
 * ---------------------------------------------------------------------- */

/* Return a pointer to the singleton boxen-window render backend.
 * The returned pointer is valid for the lifetime of the process.
 * Pass to palette_open_ex() as the `backend` argument to render via
 * boxen window cells instead of pane compositor ANSI output. */
const palette_render_backend_t *palette_render_boxen_backend(void);

/* -------------------------------------------------------------------------
 * Event translation
 * ---------------------------------------------------------------------- */

/* Translate a boxen_event_t into bytes and feed them to the palette state
 * machine.  Returns the resulting palette_done_t.
 *
 * Key mapping:
 *   BOXEN_KEY_NONE (printable ch)  -> palette_feed_byte(ch)
 *   BOXEN_KEY_ESCAPE               -> palette_feed_byte(0x1b) +
 *                                     palette_feed_esc_timeout (immediate
 *                                     -- no disambiguation needed in boxen
 *                                     mode since Escape is a named key)
 *   BOXEN_KEY_ENTER                -> palette_feed_byte(0x0a)
 *   BOXEN_KEY_UP                   -> CSI A (0x1b '[' 'A')
 *   BOXEN_KEY_DOWN                 -> CSI B
 *   BOXEN_KEY_LEFT                 -> CSI D
 *   BOXEN_KEY_RIGHT                -> CSI C
 *   BOXEN_KEY_BACKSPACE            -> palette_feed_byte(0x7f)
 *   Other                          -> PALETTE_DONE_NONE (ignored)
 */
palette_done_t boxen_palette_feed_event(palette_state_t *st,
                                        const boxen_event_t *ev);

/* -------------------------------------------------------------------------
 * Done callback
 * ---------------------------------------------------------------------- */

/* Callback invoked by the modal input_fn when the palette state machine
 * reaches PALETTE_DONE_EXECUTE or PALETTE_DONE_CANCEL.
 *
 * Parameters:
 *   repl_state  -- opaque pointer registered via set_done_cb; the REPL
 *                  wires its own boxen_repl_state_t* here
 *   done        -- PALETTE_DONE_EXECUTE or PALETTE_DONE_CANCEL
 *   exec_script -- palette_state.exec_script (NULL on CANCEL)
 *   exec_arg    -- palette_state.exec_arg (empty string on CANCEL)
 *
 * The callback is responsible for:
 *   1. Calling palette_close on the palette state.
 *   2. Freeing the palette state allocation and NULLing the pointer.
 *   3. Refocusing the REPL input window.
 *   4. On DONE_EXECUTE: dispatching via the palette_dispatch_hook.
 */
typedef void (*boxen_palette_done_cb)(void *repl_state,
                                      palette_done_t done,
                                      void *exec_script,
                                      const char *exec_arg);

/* Register the done-callback and associated opaque REPL state pointer.
 * MUST be called before palette_open_ex so the backend ctx captures the
 * callback before the modal window goes interactive.
 *
 * Pass NULL for both arguments to clear the registration. */
void palette_render_boxen_backend_set_done_cb(boxen_palette_done_cb cb,
                                              void *repl_state);

#ifdef __cplusplus
}
#endif

#endif /* BOXEN_PALETTE_BACKEND_H */
