/*
 * boxen_completion_popup.c -- 2026-06-09 JES #691 Phase C.0.2: completion popup.
 *
 * A modal floating window rendered above the REPL input bar that shows
 * tab-completion candidates.  The window is a paint surface only; key routing
 * stays with the REPL's on_input handler (modal flag raises z-order but the
 * REPL retains input focus via boxen_window_focus on the input window).
 *
 * Design notes (D2, D3 from REPL_SCHISM_EXECUTION_PLAN.md C.0.2):
 *   - boxen_window_raise(win) places the popup above other windows in z-order.
 *     We deliberately do NOT call boxen_window_set_modal: modal would redirect
 *     key dispatch away from the input window, breaking Tab/Up/Down/Enter/Escape
 *     handling while the popup is open.
 *   - The input window retains actual key focus throughout.  Events route
 *     through the REPL on_input handler, which inspects s->completion_popup
 *     to determine whether completion keys should be intercepted.
 *   - The popup is freed by boxen_completion_popup_close(); the REPL clears
 *     its completion_popup pointer after calling close.
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Frontier contributors
 */

#include "boxen_completion_popup.h"
#include "boxen/boxen.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* -------------------------------------------------------------------------
 * Internal struct
 * ---------------------------------------------------------------------- */

struct boxen_completion_popup {
	boxen_window_t *win;     /* boxen window handle; NULL after close */
	int  count;              /* number of candidates */
	int  selected;           /* currently highlighted row (0-based) */
	/* Candidates stored inline (truncated to BOXEN_COMPLETION_CANDIDATE_MAX-1). */
	char candidates[BOXEN_COMPLETION_MAX_CANDIDATES][BOXEN_COMPLETION_CANDIDATE_MAX];
};

/* -------------------------------------------------------------------------
 * Draw callback
 * ---------------------------------------------------------------------- */

static void draw_completion_popup(boxen_window_t *win, void *user_data) {
	boxen_completion_popup_t *p = (boxen_completion_popup_t *)user_data;
	if (p == NULL) return;

	int w = boxen_window_content_width(win);
	int h = boxen_window_content_height(win);
	if (w <= 0 || h <= 0) return;

	/* Clear content area */
	{
		boxen_rect_t r = { 0, 0, w, h };
		boxen_fill_rect(win, r, ' ',
		                BOXEN_COLOR_DEFAULT, BOXEN_COLOR_DEFAULT, BOXEN_ATTR_NONE);
	}

	/* Render candidates; clip to visible content rows. */
	for (int i = 0; i < p->count && i < h; i++) {
		const char *cand = p->candidates[i];
		int  cap  = (w < BOXEN_COMPLETION_CANDIDATE_MAX - 1)
		            ? w : BOXEN_COMPLETION_CANDIDATE_MAX - 1;
		char buf[BOXEN_COMPLETION_CANDIDATE_MAX];
		int  n = snprintf(buf, (size_t)(cap + 1), "%-*.*s", cap, cap, cand);
		(void)n;

		boxen_attr_t attr = (i == p->selected) ? BOXEN_ATTR_REVERSE : BOXEN_ATTR_NONE;
		boxen_draw_text(win, 0, i, buf,
		                BOXEN_COLOR_DEFAULT, BOXEN_COLOR_DEFAULT, attr);
	}
}

/* -------------------------------------------------------------------------
 * Public API
 * ---------------------------------------------------------------------- */

boxen_completion_popup_t *boxen_completion_popup_open(
	const char *const *candidates, int count,
	int anchor_x, int anchor_y, int screen_w, int screen_h)
{
	(void)screen_h;  /* height is not used; anchoring is done relative to anchor_y */
	if (candidates == NULL || count <= 0) return NULL;
	if (count > BOXEN_COMPLETION_MAX_CANDIDATES) {
		count = BOXEN_COMPLETION_MAX_CANDIDATES;
	}

	boxen_completion_popup_t *p = calloc(1, sizeof(boxen_completion_popup_t));
	if (p == NULL) return NULL;

	/* Copy candidates (truncate to CANDIDATE_MAX-1 each). */
	for (int i = 0; i < count; i++) {
		if (candidates[i] == NULL) {
			p->candidates[i][0] = '\0';
		} else {
			strncpy(p->candidates[i], candidates[i],
			        BOXEN_COMPLETION_CANDIDATE_MAX - 1);
			p->candidates[i][BOXEN_COMPLETION_CANDIDATE_MAX - 1] = '\0';
		}
	}
	p->count    = count;
	p->selected = 0;

	/* Choose popup rect:
	 *   width  = min(40, screen_w - 2), clamped to >= 4
	 *   height = min(count, 10), clamped to >= 1
	 *   y      = anchor_y - height (above the input bar), clamped to >= 0
	 *   x      = anchor_x, clamped so the popup fits within screen_w */
	int pw = (screen_w - 2 < 40) ? screen_w - 2 : 40;
	if (pw < 4) pw = 4;

	int ph = (count < 10) ? count : 10;
	if (ph < 1) ph = 1;

	int py = anchor_y - ph;
	if (py < 0) py = 0;

	int px = anchor_x;
	if (px + pw > screen_w) px = screen_w - pw;
	if (px < 0) px = 0;

	boxen_rect_t rect = { px, py, pw, ph };
	p->win = boxen_window_open("Complete", rect, p);
	if (p->win == NULL) {
		free(p);
		return NULL;
	}

	boxen_window_set_borders(p->win, false);
	boxen_window_set_draw(p->win, draw_completion_popup);
	/* user_data already set via the third arg of boxen_window_open above. */

	/* Raise for z-order (popup must appear above the input bar and output pane).
	 * We deliberately do NOT call boxen_window_set_modal: modal would redirect
	 * key dispatch away from the input window, which still needs to receive keys
	 * so on_input can handle Tab/Up/Down/Enter/Escape while the popup is open.
	 * boxen_window_raise achieves visual priority without affecting focus or
	 * key routing. */
	boxen_window_raise(p->win);

	boxen_window_invalidate(p->win);
	return p;
}

void boxen_completion_popup_close(boxen_completion_popup_t *popup) {
	if (popup == NULL) return;
	if (popup->win != NULL) {
		boxen_window_close(popup->win);
		popup->win = NULL;
	}
	free(popup);
}

void boxen_completion_popup_navigate(boxen_completion_popup_t *popup, int dir) {
	if (popup == NULL || popup->count <= 0) return;
	/* Normalize dir to +1 or -1. */
	if (dir > 0) dir = 1;
	else if (dir < 0) dir = -1;
	else return;

	popup->selected = (popup->selected + dir + popup->count) % popup->count;

	if (popup->win != NULL) {
		boxen_window_invalidate(popup->win);
	}
}

const char *boxen_completion_popup_selected(const boxen_completion_popup_t *popup) {
	if (popup == NULL || popup->count <= 0) return NULL;
	return popup->candidates[popup->selected];
}

int boxen_completion_popup_count(const boxen_completion_popup_t *popup) {
	if (popup == NULL) return 0;
	return popup->count;
}

bool boxen_completion_popup_is_open(const boxen_completion_popup_t *popup) {
	if (popup == NULL) return false;
	return popup->win != NULL;
}
