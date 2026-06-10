/* boxen_completion_popup.h -- 2026-06-09 JES #691 Phase C.0.2: completion popup window.
 *
 * A transient floating window that lists tab-completion candidates above the
 * input bar.  The popup is a paint surface only: key events are routed
 * through the REPL's on_input handler, not through the popup's own input_fn.
 * The popup uses boxen_window_raise for z-order (not set_modal, which would
 * redirect key dispatch away from the input window).
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Frontier contributors
 */

#ifndef BOXEN_COMPLETION_POPUP_H
#define BOXEN_COMPLETION_POPUP_H

#include <stdbool.h>
#include "boxen/boxen.h"

/* Maximum number of candidates the popup can hold. */
#define BOXEN_COMPLETION_MAX_CANDIDATES 64

/* Maximum length of a single candidate string (including NUL).
 * Matches BOXEN_REPL_INPUT_MAX from boxen_repl_internal.h. */
#define BOXEN_COMPLETION_CANDIDATE_MAX  1024

/* Opaque popup state. */
typedef struct boxen_completion_popup boxen_completion_popup_t;

/*
 * boxen_completion_popup_open -- open a completion popup above the input bar.
 *
 * candidates  -- array of candidate strings (NULL-terminated content; count entries)
 * count       -- number of candidates (must be >= 1)
 * anchor_x    -- left edge of the anchor (typically 0 for the input bar)
 * anchor_y    -- row of the anchor (typically screen_h - 2, the input bar row)
 * screen_w    -- terminal width in columns
 * screen_h    -- terminal height in rows
 *
 * The popup is placed above the anchor row.  Width = min(40, screen_w-2);
 * height = min(count, 10), clamped so the popup stays on screen.
 *
 * Returns a heap-allocated popup handle, or NULL on allocation / window failure.
 * The caller owns the handle and must call boxen_completion_popup_close() when done.
 */
boxen_completion_popup_t *boxen_completion_popup_open(
	const char *const *candidates, int count,
	int anchor_x, int anchor_y, int screen_w, int screen_h);

/*
 * boxen_completion_popup_close -- close and free the popup.
 *
 * Safe to call with NULL (no-op).
 */
void boxen_completion_popup_close(boxen_completion_popup_t *popup);

/*
 * boxen_completion_popup_navigate -- move the selection by dir rows.
 *
 * dir = +1 moves down (wraps from last to first).
 * dir = -1 moves up (wraps from first to last).
 * Other values are clamped to +1 / -1.
 */
void boxen_completion_popup_navigate(boxen_completion_popup_t *popup, int dir);

/*
 * boxen_completion_popup_selected -- return the currently-highlighted candidate.
 *
 * Returns NULL if popup is NULL or count == 0.
 */
const char *boxen_completion_popup_selected(const boxen_completion_popup_t *popup);

/*
 * boxen_completion_popup_count -- return number of candidates in the popup.
 *
 * Returns 0 if popup is NULL.
 */
int boxen_completion_popup_count(const boxen_completion_popup_t *popup);

/*
 * boxen_completion_popup_is_open -- return true if the popup window is active.
 *
 * Returns false if popup is NULL.
 */
bool boxen_completion_popup_is_open(const boxen_completion_popup_t *popup);

#endif /* BOXEN_COMPLETION_POPUP_H */
