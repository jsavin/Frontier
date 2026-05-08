/*
 * repl_output_async.h - Async output router (palette-aware).
 *
 * Issue #593: while the palette modal is open, async output (TCP
 * callbacks, agent ticks, headless msg() verb, anything emitting
 * stdout from a non-main thread) MUST NOT touch stdout directly —
 * doing so corrupts the compositor's framebuffer for one frame and
 * causes the palette to flicker on the next keystroke.
 *
 * This module provides a single chokepoint:
 *
 *   repl_async_output_emit(bytes, len)
 *       — palette inactive: writes directly to stdout.
 *       — palette active:   appends to the registered scrollback pane;
 *                           stdout is NOT written.
 *
 * The router state (palette active flag + scrollback pointer) is
 * toggled by run_palette_modal() at session boundaries via
 * repl_async_output_set_palette_active().
 *
 * The previous repl_async_output(const char *) function in
 * repl_output.c was scaffolding for this exact rerouting (see PR 4
 * pane.h "Threading" note); it remains for ad-hoc null-terminated
 * users but is now implemented in terms of repl_async_output_emit().
 *
 * Threading
 * ---------
 *   - repl_async_output_emit MAY be called from ANY thread. The
 *     scrollback path is mutex-protected; the stdout path uses
 *     stdio's own internal locking.
 *   - repl_async_output_set_palette_active MUST be called from the
 *     main thread (it transitions the routing rule).
 */

#ifndef FRONTIER_REPL_OUTPUT_ASYNC_H
#define FRONTIER_REPL_OUTPUT_ASYNC_H

#include <stdbool.h>
#include <stddef.h>

struct scrollback_pane; /* forward decl */

/* Emit a chunk of bytes through the palette-aware router.
 *
 * Bytes are written verbatim except that, when palette is active,
 * the scrollback's line splitter normalises \r and \n line breaks. */
void repl_async_output_emit(const char *bytes, size_t len);

/* Toggle palette mode. When `active` is true, future
 * repl_async_output_emit calls route to `sb`; when false, they go
 * straight to stdout (and `sb` may be NULL).
 *
 * Calling with active=true and sb=NULL is a defensive no-op (logged
 * elsewhere); the router falls back to stdout to avoid losing data. */
void repl_async_output_set_palette_active(bool active,
                                          struct scrollback_pane *sb);

/* Inspection helper for tests / sanity asserts. */
bool repl_async_output_palette_active(void);

#endif /* FRONTIER_REPL_OUTPUT_ASYNC_H */
