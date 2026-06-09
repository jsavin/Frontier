/*
 * boxen_outline.c -- boxen read-only outline editor window.
 *
 * C.1 tracer-bullet: minimum-viable outline editor opened via /edit [path].
 *
 * What C.1 ships:
 *   - Read-only structural outline view (wedge/leaf/checkbox markers)
 *   - Bar cursor (full-line highlight) + Up/Down/Left/Right navigation
 *   - Expand/collapse via arrow keys, Keypad-+/-, Keypad-*, Cmd-[/]/Shift-[/]
 *   - F9: toggle flbreakpoint on current node (direct field write under GIL)
 *   - Cmd-/: toggle flcomment on current node (direct field write under GIL)
 *   - Cmd-R: run current path as script (delegates to REPL slash dispatch)
 *   - Cmd-S: save (delegates to REPL /save)
 *   - q or Esc: close window
 *   - Keypad-Enter / Esc (modal): no-op (edit mode deferred to C.1.1)
 *   - Multiple windows: open-editors registry (BOXEN_OUTLINE_MAX_EDITORS slots)
 *   - Comment nodes rendered dim; breakpoint nodes rendered with '*' gutter
 *   - Checkbox attribute nodes render [x] / [ ] instead of 'o'
 *
 * Architecture:
 *   The editor does NOT have its own event loop.  It registers draw + input
 *   callbacks with boxen and is driven by the REPL's main event loop.
 *   No GIL boundary is introduced beyond what the REPL already holds.
 *
 * GIL discipline:
 *   All callbacks (draw, input) are invoked while the REPL holds the GIL.
 *   The outline-walk (production hook) and flbreakpoint/flcomment mutations
 *   are all GIL-held operations.  Test builds use mock hooks that do not
 *   touch the real ODB.
 *
 * Multi-window state lifetime:
 *   States are heap-allocated per window and freed on window close.
 *   On REPL exit, boxen_outline_close_all() must be called before REPL
 *   state is freed.
 *
 * 2026-06-09 JES Phase C.1 #691
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Frontier contributors
 */

#include "boxen_outline.h"
#include "boxen_outline_internal.h"

#include "boxen/boxen.h"

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>

#ifndef BOXEN_OUTLINE_OMIT_MAIN
#include "../Common/headers/logging.h"
/* 2026-06-09 JES #691 C.1 round 2 P1: path canonicalization uses
 * repl_get_current_path() in boxen_outline_open's production path. */
#include "repl.h"
#endif

/* -------------------------------------------------------------------------
 * 2026-06-09 JES Phase C.1 #691: layout constants.
 *
 * Title bar on top row; content fills the rest.
 * Minimum window height: 3 rows (1 title + 1 content + 1 footer hint).
 * ---------------------------------------------------------------------- */

/* Indent width per level (spaces) */
#define OUTLINE_INDENT_WIDTH 3

/* Gutter width: 1 column for breakpoint marker + 1 space */
#define OUTLINE_GUTTER_WIDTH 2

/* Marker column strings (NUL-terminated, drawn in the marker column) */
#define MARKER_EXPANDED   "v"
#define MARKER_COLLAPSED  ">"
#define MARKER_LEAF       "o"
#define MARKER_CHECKED    "[x]"
#define MARKER_UNCHECKED  "[ ]"
#define MARKER_BREAKPOINT "*"

/* -------------------------------------------------------------------------
 * 2026-06-09 JES Phase C.1 #691: open-editors registry.
 *
 * File-static (not in boxen_repl_state_t) for C.1 simplicity.
 * Comment: attach to boxen_repl_state_t in C.4 multi-window coordination.
 * ---------------------------------------------------------------------- */
static boxen_outline_registry_t g_registry;
static bool g_registry_inited = false;

static void registry_init(void) {
	if (!g_registry_inited) {
		memset(&g_registry, 0, sizeof(g_registry));
		g_registry_inited = true;
	}
}

static boxen_outline_state_t *registry_find(const char *path) {
	/* 2026-06-09 JES #691 C.1 round 2 P1: iterate full array, not < count.
	 * registry_remove NULLs a slot without compacting, so any entry at index
	 * >= count would be unreachable if we stopped at count. */
	for (int i = 0; i < BOXEN_OUTLINE_MAX_EDITORS; i++) {
		boxen_outline_state_t *e = g_registry.editors[i];
		if (e != NULL && strcmp(e->path, path) == 0) {
			return e;
		}
	}
	return NULL;
}

static bool registry_add(boxen_outline_state_t *s) {
	if (g_registry.count >= BOXEN_OUTLINE_MAX_EDITORS) {
		return false;
	}
	for (int i = 0; i < BOXEN_OUTLINE_MAX_EDITORS; i++) {
		if (g_registry.editors[i] == NULL) {
			g_registry.editors[i] = s;
			g_registry.count++;
			return true;
		}
	}
	return false;
}

static void registry_remove(boxen_outline_state_t *s) {
	for (int i = 0; i < BOXEN_OUTLINE_MAX_EDITORS; i++) {
		if (g_registry.editors[i] == s) {
			g_registry.editors[i] = NULL;
			g_registry.count--;
			return;
		}
	}
}

/* -------------------------------------------------------------------------
 * Forward declarations for draw and input callbacks
 * ---------------------------------------------------------------------- */
static void draw_outline_window(boxen_window_t *win, void *user_data);
static void on_outline_input(boxen_window_t *win, const boxen_event_t *ev,
                             void *user_data);

/* -------------------------------------------------------------------------
 * 2026-06-09 JES Phase C.1 #691: boxen_outline_state_init.
 *
 * Zero-fills the state, sets the path, creates the boxen window, and wires
 * production hooks (unless BOXEN_OUTLINE_OMIT_MAIN is defined).
 * ---------------------------------------------------------------------- */
void boxen_outline_state_init(boxen_outline_state_t *s, const char *path,
                              int tw, int th) {
	memset(s, 0, sizeof(boxen_outline_state_t));

	/* Copy path (strip leading '@' if present) */
	const char *p = (path != NULL && path[0] == '@') ? path + 1 : path;
	if (p == NULL) p = "";
	size_t plen = strlen(p);
	if (plen >= sizeof(s->path)) plen = sizeof(s->path) - 1;
	memcpy(s->path, p, plen);
	s->path[plen] = '\0';

#ifndef BOXEN_OUTLINE_OMIT_MAIN
	/* Wire production hooks */
	s->outline_fetch_hook     = boxen_outline_real_fetch;
	s->toggle_breakpoint_hook = boxen_outline_real_toggle_breakpoint;
	s->toggle_comment_hook    = boxen_outline_real_toggle_comment;
	s->script_run_hook        = boxen_outline_real_script_run;
	s->expand_hook            = boxen_outline_real_expand;
#endif

	/* Create the boxen window.
	 * Occupy most of the terminal, leaving room for the REPL below.
	 * For C.1, use a simple stacked layout: take the top 60% of the terminal.
	 * The REPL is at the bottom; the editor opens on top. */
	int win_h = th > 8 ? (th * 3 / 4) : th;
	int win_w = tw;
	if (win_h < 4) win_h = 4;
	if (win_w < 20) win_w = 20;

	/* Title string: "Outline: <path>" */
	char title[320];
	snprintf(title, sizeof(title), "Outline: %s", s->path);

	boxen_rect_t rect = { 0, 0, win_w, win_h };
	s->win = boxen_window_open(title, rect, NULL);
	if (s->win != NULL) {
		boxen_window_set_draw(s->win, draw_outline_window);
		boxen_window_set_input(s->win, on_outline_input);
		boxen_window_set_user_data(s->win, s);
		/* Editor window is resizable/movable in future phases.
		 * For C.1, leave defaults (not movable or resizable). */
		boxen_window_focus(s->win);
	}

	s->cursor    = 0;
	s->scroll_top = 0;
	s->initialized = false;  /* set true after first successful fetch */
}

/* -------------------------------------------------------------------------
 * 2026-06-09 JES Phase C.1 #691: boxen_outline_state_teardown.
 * ---------------------------------------------------------------------- */
void boxen_outline_state_teardown(boxen_outline_state_t *s) {
	if (s == NULL) return;

	if (s->win != NULL) {
		boxen_window_close(s->win);
		s->win = NULL;
	}

	s->node_count  = 0;
	s->cursor      = 0;
	s->scroll_top  = 0;
	s->initialized = false;
	s->should_close = false;
}

/* -------------------------------------------------------------------------
 * 2026-06-09 JES Phase C.1 #691: boxen_outline_refresh.
 *
 * Calls the fetch hook to populate nodes[], then clamps the cursor.
 * ---------------------------------------------------------------------- */
bool boxen_outline_refresh(boxen_outline_state_t *s) {
	if (s == NULL) return false;
	if (s->outline_fetch_hook == NULL) return false;

	int count = 0;
	bool ok = s->outline_fetch_hook(s, s->path, s->nodes, &count);
	if (!ok) return false;

	s->node_count  = count;
	s->initialized = true;

	/* Clamp cursor */
	if (s->cursor >= count) {
		s->cursor = count > 0 ? count - 1 : 0;
	}

	if (s->win != NULL) {
		boxen_window_invalidate(s->win);
	}
	return true;
}

/* -------------------------------------------------------------------------
 * 2026-06-09 JES Phase C.1 #691: boxen_outline_cursor_advance.
 *
 * Moves cursor by delta (can be negative).  Clamps to [0, node_count-1].
 * Adjusts scroll_top to keep cursor visible.
 * ---------------------------------------------------------------------- */
int boxen_outline_cursor_advance(boxen_outline_state_t *s, int delta) {
	if (s == NULL || s->node_count == 0) return 0;

	int new_cursor = s->cursor + delta;
	if (new_cursor < 0) new_cursor = 0;
	if (new_cursor >= s->node_count) new_cursor = s->node_count - 1;
	s->cursor = new_cursor;

	/* Compute visible content height.
	 * With borders enabled (default): content_height = rect.h - 2 - 1 (title row).
	 * We use boxen_window_content_height for accuracy; subtract 1 for title bar. */
	int content_h = (s->win != NULL)
	              ? boxen_window_content_height(s->win) - 1  /* subtract title row */
	              : 20;
	if (content_h < 1) content_h = 1;

	/* Scroll down if cursor is below visible area */
	if (new_cursor >= s->scroll_top + content_h) {
		s->scroll_top = new_cursor - content_h + 1;
	}
	/* Scroll up if cursor is above visible area */
	if (new_cursor < s->scroll_top) {
		s->scroll_top = new_cursor;
	}
	if (s->scroll_top < 0) s->scroll_top = 0;

	if (s->win != NULL) boxen_window_invalidate(s->win);
	return s->cursor;
}

/* -------------------------------------------------------------------------
 * 2026-06-09 JES Phase C.1 #691: expand_all_descendants.
 *
 * Helper called by Keypad-* (expand all sub-headings of current node).
 * Walks through nodes[] from the current cursor downward, calling expand_hook
 * on every node that has children (at any nesting level deeper than the
 * current cursor's level, or if cursor is at the root, all nodes).
 * ---------------------------------------------------------------------- */
static void expand_all_descendants(boxen_outline_state_t *s) {
	if (s == NULL || s->expand_hook == NULL) return;
	if (s->node_count == 0) return;

	int cursor_level = (s->cursor < s->node_count)
	                   ? s->nodes[s->cursor].level
	                   : 0;

	/* Expand the cursor node itself if it has children */
	if (s->cursor < s->node_count && s->nodes[s->cursor].has_children) {
		s->expand_hook(&s->nodes[s->cursor], true);
	}

	/* Walk forward: expand everything at a deeper level */
	for (int i = s->cursor + 1; i < s->node_count; i++) {
		if (s->nodes[i].level <= cursor_level) break;  /* exited subtree */
		if (s->nodes[i].has_children) {
			s->expand_hook(&s->nodes[i], true);
		}
	}
}

/* -------------------------------------------------------------------------
 * 2026-06-09 JES Phase C.1 #691: expand_all_global.
 *
 * Expands ALL nodes in the outline (Cmd-Shift-]).
 * ---------------------------------------------------------------------- */
static void expand_all_global(boxen_outline_state_t *s) {
	if (s == NULL || s->expand_hook == NULL) return;
	for (int i = 0; i < s->node_count; i++) {
		if (s->nodes[i].has_children) {
			s->expand_hook(&s->nodes[i], true);
		}
	}
}

/* -------------------------------------------------------------------------
 * 2026-06-09 JES Phase C.1 #691: collapse_all_global.
 *
 * Collapses all top-level nodes (Cmd-Shift-[).
 * ---------------------------------------------------------------------- */
static void collapse_all_global(boxen_outline_state_t *s) {
	if (s == NULL || s->expand_hook == NULL) return;
	for (int i = 0; i < s->node_count; i++) {
		if (s->nodes[i].has_children) {
			s->expand_hook(&s->nodes[i], false);
		}
	}
}

/* -------------------------------------------------------------------------
 * 2026-06-09 JES Phase C.1 #691: handle_left_arrow.
 *
 * Left arrow: if current node is expanded and has children, collapse it.
 * Otherwise, move to parent (the last node at level - 1 above cursor).
 * ---------------------------------------------------------------------- */
static void handle_left_arrow(boxen_outline_state_t *s) {
	if (s == NULL || s->node_count == 0) return;

	boxen_outline_node_t *cur = &s->nodes[s->cursor];

	if (cur->has_children && cur->expanded) {
		/* Collapse current node */
		if (s->expand_hook != NULL) {
			s->expand_hook(cur, false);
			boxen_outline_refresh(s);
		}
		return;
	}

	/* Move to parent: scan backward for first node at level - 1 */
	if (cur->level == 0) return;  /* already at root level */

	int target_level = cur->level - 1;
	for (int i = s->cursor - 1; i >= 0; i--) {
		if (s->nodes[i].level == target_level) {
			s->cursor = i;
			if (s->win != NULL) boxen_window_invalidate(s->win);
			return;
		}
	}
}

/* -------------------------------------------------------------------------
 * 2026-06-09 JES Phase C.1 #691: handle_right_arrow.
 *
 * Right arrow: if current node is collapsed and has children, expand it.
 * If already expanded (or is a leaf), move to first child if one exists.
 * ---------------------------------------------------------------------- */
static void handle_right_arrow(boxen_outline_state_t *s) {
	if (s == NULL || s->node_count == 0) return;

	boxen_outline_node_t *cur = &s->nodes[s->cursor];

	if (!cur->has_children) return;  /* leaf: nothing to do */

	if (!cur->expanded) {
		/* Expand and re-fetch */
		if (s->expand_hook != NULL) {
			s->expand_hook(cur, true);
			boxen_outline_refresh(s);
		}
		return;
	}

	/* Already expanded: move to first child (next node one level deeper) */
	if (s->cursor + 1 < s->node_count &&
	    s->nodes[s->cursor + 1].level == cur->level + 1) {
		boxen_outline_cursor_advance(s, 1);
	}
}

/* -------------------------------------------------------------------------
 * 2026-06-09 JES Phase C.1 #691: boxen_outline_handle_key.
 *
 * Processes a single key event from the window's input callback.
 * Returns true if consumed.
 *
 * Key mapping summary:
 *   Down               -- advance cursor +1
 *   Up                 -- advance cursor -1
 *   Left               -- collapse / parent
 *   Right              -- expand / first-child
 *   '-' (no mod)       -- Keypad-minus: collapse current
 *   '+' (no mod)       -- Keypad-plus:  expand current
 *   '*' (no mod)       -- Keypad-star:  expand all descendants
 *   '[' + Meta         -- Cmd-[: collapse current
 *   ']' + Meta         -- Cmd-]: expand current
 *   '[' + Meta + Shift -- Cmd-Shift-[: collapse all
 *   ']' + Meta + Shift -- Cmd-Shift-]: expand all
 *   F9                 -- toggle flbreakpoint
 *   '/' + Meta         -- Cmd-/: toggle flcomment
 *   'r' + Meta         -- Cmd-R: run
 *   's' + Meta         -- Cmd-S: save
 *   Enter (Keypad)     -- no-op (C.1.1: edit mode entry point)
 *   Esc                -- close window
 *   'q' (no mod)       -- close window
 * ---------------------------------------------------------------------- */
bool boxen_outline_handle_key(boxen_outline_state_t *s,
                               const boxen_event_t *ev) {
	if (s == NULL || ev == NULL || ev->type != BOXEN_EV_KEY) return false;

	uint16_t mod = ev->key.mod;
	boxen_key_t key = ev->key.key;
	uint32_t ch = ev->key.ch;

	/* ---- Navigation ---- */
	if (key == BOXEN_KEY_DOWN && mod == BOXEN_MOD_NONE) {
		boxen_outline_cursor_advance(s, 1);
		return true;
	}
	if (key == BOXEN_KEY_UP && mod == BOXEN_MOD_NONE) {
		boxen_outline_cursor_advance(s, -1);
		return true;
	}
	if (key == BOXEN_KEY_LEFT && mod == BOXEN_MOD_NONE) {
		handle_left_arrow(s);
		return true;
	}
	if (key == BOXEN_KEY_RIGHT && mod == BOXEN_MOD_NONE) {
		handle_right_arrow(s);
		return true;
	}

	/* ---- Expand / collapse ---- */

	/* Keypad-minus '-': collapse current node */
	if (key == BOXEN_KEY_NONE && ch == '-' && mod == BOXEN_MOD_NONE) {
		if (s->node_count > 0 && s->cursor < s->node_count) {
			boxen_outline_node_t *cur = &s->nodes[s->cursor];
			if (cur->has_children && cur->expanded && s->expand_hook != NULL) {
				s->expand_hook(cur, false);
				/* 2026-06-09 JES #691 C.1 round 2 P1: call refresh so the
				 * display list reflects the collapse immediately.  Without this,
				 * the screen stays stale until the next unrelated refresh.
				 * Matches the pattern in keypad-plus, Cmd-[, and left-arrow. */
				boxen_outline_refresh(s);
			}
		}
		return true;
	}

	/* Keypad-plus '+': expand current node */
	if (key == BOXEN_KEY_NONE && ch == '+' && mod == BOXEN_MOD_NONE) {
		if (s->node_count > 0 && s->cursor < s->node_count) {
			boxen_outline_node_t *cur = &s->nodes[s->cursor];
			if (cur->has_children && !cur->expanded && s->expand_hook != NULL) {
				s->expand_hook(cur, true);
				boxen_outline_refresh(s);
			}
		}
		return true;
	}

	/* Keypad-star '*': expand all descendants of current node */
	if (key == BOXEN_KEY_NONE && ch == '*' && mod == BOXEN_MOD_NONE) {
		expand_all_descendants(s);
		/* 2026-06-09 JES #691 C.1 round 2 P1: call refresh so the display list
		 * is rebuilt after expand-all.  Matches keypad-plus and Cmd-Shift-] pattern.
		 * Previously the test compensated by calling refresh explicitly -- that was
		 * the test-fits-implementation antipattern. */
		boxen_outline_refresh(s);
		return true;
	}

	/* Cmd-[ : collapse current (laptop-friendly) */
	if (key == BOXEN_KEY_NONE && ch == '[' && mod == BOXEN_MOD_META) {
		if (s->node_count > 0 && s->cursor < s->node_count) {
			boxen_outline_node_t *cur = &s->nodes[s->cursor];
			if (cur->has_children && cur->expanded && s->expand_hook != NULL) {
				s->expand_hook(cur, false);
				boxen_outline_refresh(s);
			}
		}
		return true;
	}

	/* Cmd-] : expand current */
	if (key == BOXEN_KEY_NONE && ch == ']' && mod == BOXEN_MOD_META) {
		if (s->node_count > 0 && s->cursor < s->node_count) {
			boxen_outline_node_t *cur = &s->nodes[s->cursor];
			if (cur->has_children && !cur->expanded && s->expand_hook != NULL) {
				s->expand_hook(cur, true);
				boxen_outline_refresh(s);
			}
		}
		return true;
	}

	/* Cmd-Shift-[ : collapse all */
	if (key == BOXEN_KEY_NONE && ch == '[' &&
	    mod == (BOXEN_MOD_META | BOXEN_MOD_SHIFT)) {
		collapse_all_global(s);
		boxen_outline_refresh(s);
		return true;
	}

	/* Cmd-Shift-] : expand all */
	if (key == BOXEN_KEY_NONE && ch == ']' &&
	    mod == (BOXEN_MOD_META | BOXEN_MOD_SHIFT)) {
		expand_all_global(s);
		boxen_outline_refresh(s);
		return true;
	}

	/* ---- Mutations ---- */

	/* F9: toggle flbreakpoint */
	if (key == BOXEN_KEY_F9 && mod == BOXEN_MOD_NONE) {
		if (s->node_count > 0 && s->cursor < s->node_count) {
			if (s->toggle_breakpoint_hook != NULL) {
				/* 2026-06-09 JES #691 C.1 round 2 P0-2: pass (state, node_idx)
				 * so the production hook can use houtline_opaque for
				 * oppushoutline/opdirtyoutline/oppopoutline. */
				s->toggle_breakpoint_hook(s, s->cursor);
			}
			if (s->win != NULL) boxen_window_invalidate(s->win);
		}
		return true;
	}

	/* Cmd-/ : toggle flcomment */
	if (key == BOXEN_KEY_NONE && ch == '/' && mod == BOXEN_MOD_META) {
		if (s->node_count > 0 && s->cursor < s->node_count) {
			if (s->toggle_comment_hook != NULL) {
				/* 2026-06-09 JES #691 C.1 round 2 P0-2: same (state, node_idx)
				 * signature as toggle_breakpoint. */
				s->toggle_comment_hook(s, s->cursor);
			}
			if (s->win != NULL) boxen_window_invalidate(s->win);
		}
		return true;
	}

	/* ---- Script run / save ---- */

	/* Cmd-R: run */
	if (key == BOXEN_KEY_NONE && (ch == 'r' || ch == 'R') &&
	    mod == BOXEN_MOD_META) {
		if (s->script_run_hook != NULL) {
			s->script_run_hook(s->path);
		}
		return true;
	}

	/* Cmd-S: save (delegates to REPL /save -- no-op in C.1 read-only mode) */
	if (key == BOXEN_KEY_NONE && (ch == 's' || ch == 'S') &&
	    mod == BOXEN_MOD_META) {
		/* 2026-06-09 JES #691 C.1: route save through /save slash command.
		 * Deferred: need a reference to the REPL state to call slash dispatch.
		 * For C.1, the editor is read-only (except flbreakpoint/flcomment)
		 * so there is nothing to save beyond what's already committed. */
		return true;
	}

	/* Keypad-Enter: no-op in C.1 (edit mode deferred to C.1.1) */
	if (key == BOXEN_KEY_ENTER && mod == BOXEN_MOD_NONE) {
		/* 2026-06-09 JES #691 C.1: Keypad-Enter / Enter -- deferred to C.1.1.
		 * Accept and ignore to prevent terminal beep. */
		return true;
	}

	/* ---- Window close ---- */

	/* Esc or 'q': close this editor window */
	if (key == BOXEN_KEY_ESCAPE ||
	    (key == BOXEN_KEY_NONE && (ch == 'q' || ch == 'Q') &&
	     mod == BOXEN_MOD_NONE)) {
		/* 2026-06-09 JES #691 C.1 round 2 P0-3: free the state struct.
		 *
		 * Previous code: registry_remove + window_close + NULL + should_close,
		 * but no free(s).  The state struct is ~2MB; each close leaked it.
		 *
		 * boxen_window_close() is synchronous (see boxen.c:341-389): it removes
		 * the window from the live list, clears focus/drag, zeroes the magic
		 * field, and calls free(win).  There are no deferred callbacks, so we
		 * can free 's' immediately after.  Precedent: debugger_tui_main and
		 * boxen_repl_main both call free(state) after their teardown sequence.
		 *
		 * Lifetime invariant: the boxen draw/input callbacks both guard on
		 * user_data != NULL, but the window is already removed from the live
		 * list by boxen_window_close before we reach free(s), so no callback
		 * can fire for this window after window_close returns.  This is safe.
		 *
		 * Ordering: registry_remove(s) FIRST so boxen_outline_close_all() (which
		 * iterates the registry and would call boxen_window_close on each slot's
		 * win) can no longer see this s -- prevents a hypothetical double-close
		 * if close_all races.  We do NOT need to NULL s->win after window_close
		 * because s itself is freed on the very next line; the dangling field
		 * has no reachable reader.
		 *
		 * 2026-06-09 JES #691 C.1 round 2: doc fix per /gate round 2 LOW note
		 * -- previous comment claimed s->win = NULL but the code did not (and
		 * does not need to) NULL it. Comment now matches the code. */
		registry_remove(s);
		if (s->win != NULL) {
			boxen_window_close(s->win);
		}
		free(s);
		/* Note: s is now freed.  This input callback returns immediately;
		 * the REPL event loop must not access s after this return. */
		return true;
	}

	return false;
}

/* -------------------------------------------------------------------------
 * 2026-06-09 JES Phase C.1 #691: draw_outline_window (draw callback).
 *
 * Layout:
 *   Row 0 (content): title bar -- "Outline: <path>"
 *   Rows 1..h-1:     outline nodes (scrolled by scroll_top)
 *
 * Per-node format:
 *   col 0:        breakpoint marker ('*' or ' ')
 *   col 1:        space
 *   col 2..2+indent*OUTLINE_INDENT_WIDTH-1: indent spaces
 *   next cols:    wedge/leaf/checkbox marker
 *   space:        separator
 *   rest:         headline text
 *
 * Bar cursor: the current node's row is drawn with BOXEN_ATTR_REVERSE.
 * Comment nodes: BOXEN_ATTR_DIM.
 * ---------------------------------------------------------------------- */
void boxen_outline_draw(boxen_outline_state_t *s, boxen_window_t *win) {
	if (s == NULL || win == NULL) return;

	int w = boxen_window_content_width(win);
	int h = boxen_window_content_height(win);
	if (w <= 0 || h <= 0) return;

	/* Clear window */
	{
		boxen_rect_t r = { 0, 0, w, h };
		boxen_fill_rect(win, r, ' ',
		                BOXEN_COLOR_DEFAULT, BOXEN_COLOR_DEFAULT, BOXEN_ATTR_NONE);
	}

	/* Title bar (row 0) */
	{
		char title[512];
		int cap = (w < (int)(sizeof(title) - 1)) ? w : (int)(sizeof(title) - 1);
		int n = snprintf(title, (size_t)(cap + 1), "%-*.*s",
		                 cap, cap, s->path);
		(void)n;
		boxen_draw_text(win, 0, 0, title,
		                BOXEN_COLOR_WHITE, BOXEN_COLOR_BLUE, BOXEN_ATTR_BOLD);
	}

	/* Outline content rows */
	int content_rows = h - 1;  /* rows available below title bar */
	if (content_rows < 1) return;

	char linebuf[BOXEN_OUTLINE_TEXT_MAX + 32];

	for (int row = 0; row < content_rows; row++) {
		int node_idx = s->scroll_top + row;
		int draw_row = row + 1;  /* offset for title bar */

		if (node_idx >= s->node_count) break;

		boxen_outline_node_t *node = &s->nodes[node_idx];

		/* Determine rendering attributes */
		uint16_t attr = BOXEN_ATTR_NONE;
		if (node->flcomment) attr |= BOXEN_ATTR_DIM;
		if (node_idx == s->cursor) attr |= BOXEN_ATTR_REVERSE;

		/* Build the line string */
		int pos = 0;
		int cap = (w < (int)(sizeof(linebuf) - 1)) ? w : (int)(sizeof(linebuf) - 1);

		/* Gutter: breakpoint marker */
		if (node->flbreakpoint) {
			if (pos < cap) linebuf[pos++] = '*';
		} else {
			if (pos < cap) linebuf[pos++] = ' ';
		}
		if (pos < cap) linebuf[pos++] = ' ';  /* gutter space */

		/* Indent */
		int indent_spaces = node->level * OUTLINE_INDENT_WIDTH;
		for (int i = 0; i < indent_spaces && pos < cap; i++) {
			linebuf[pos++] = ' ';
		}

		/* Marker */
		const char *marker_str;
		switch (node->marker) {
			case BOXEN_OUTLINE_MARKER_EXPANDED:   marker_str = MARKER_EXPANDED;   break;
			case BOXEN_OUTLINE_MARKER_COLLAPSED:  marker_str = MARKER_COLLAPSED;  break;
			case BOXEN_OUTLINE_MARKER_CHECKED:    marker_str = MARKER_CHECKED;    break;
			case BOXEN_OUTLINE_MARKER_UNCHECKED:  marker_str = MARKER_UNCHECKED;  break;
			case BOXEN_OUTLINE_MARKER_LEAF:
			default:                              marker_str = MARKER_LEAF;       break;
		}

		for (const char *m = marker_str; *m != '\0' && pos < cap; m++) {
			linebuf[pos++] = *m;
		}
		if (pos < cap) linebuf[pos++] = ' ';  /* marker separator */

		/* Headline text */
		const char *text = node->text;
		for (; *text != '\0' && pos < cap; text++) {
			linebuf[pos++] = *text;
		}

		/* Pad to width */
		while (pos < cap) linebuf[pos++] = ' ';
		linebuf[pos < (int)sizeof(linebuf) ? pos : (int)sizeof(linebuf) - 1] = '\0';

		boxen_draw_text(win, 0, draw_row, linebuf,
		                BOXEN_COLOR_DEFAULT, BOXEN_COLOR_DEFAULT, attr);
	}
}

/* -------------------------------------------------------------------------
 * 2026-06-09 JES Phase C.1 #691: boxen window callbacks.
 * ---------------------------------------------------------------------- */
static void draw_outline_window(boxen_window_t *win, void *user_data) {
	boxen_outline_state_t *s = (boxen_outline_state_t *)user_data;
	if (s == NULL) return;
	boxen_outline_draw(s, win);
}

static void on_outline_input(boxen_window_t *win, const boxen_event_t *ev,
                             void *user_data) {
	(void)win;
	boxen_outline_state_t *s = (boxen_outline_state_t *)user_data;
	if (s == NULL || ev == NULL) return;
	boxen_outline_handle_key(s, ev);
}

/* -------------------------------------------------------------------------
 * 2026-06-09 JES Phase C.1 #691: public API -- boxen_outline_open.
 * ---------------------------------------------------------------------- */
bool boxen_outline_open(const char *path) {
	if (path == NULL) return false;

	registry_init();

	/* Strip leading '@' for lookup */
	const char *p = (path[0] == '@') ? path + 1 : path;

	/* 2026-06-09 JES #691 C.1 round 2 P1: canonicalize path for registry dedup.
	 *
	 * Problem: /edit foo, /edit @foo, and (after a cd) /edit workspace.foo all
	 * opened separate windows because the registry compared paths byte-for-byte.
	 *
	 * Fix: if p has no dot, treat it as a relative name and prepend the REPL's
	 * current path (repl_get_current_path()).  The repl_get_current_path() value
	 * is the canonicalized absolute path the REPL cd'd into, so
	 * "foo" relative to "workspace.systemTable" becomes "workspace.systemTable.foo".
	 * If the current path is empty (REPL is at root), "foo" stays "foo".
	 * The '@' was already stripped above; it does not appear in the stored path.
	 *
	 * Only in production builds -- tests do not have a REPL current path. */
#ifndef BOXEN_OUTLINE_OMIT_MAIN
	char canonical[512];
	if (strchr(p, '.') == NULL) {
		/* Relative path: prepend REPL current path if non-empty */
		const char *cur = repl_get_current_path();
		if (cur != NULL && cur[0] != '\0') {
			snprintf(canonical, sizeof(canonical), "%s.%s", cur, p);
			p = canonical;
		}
	}
#endif

	/* Check for existing editor on this path (raise instead of duplicating) */
	boxen_outline_state_t *existing = registry_find(p);
	if (existing != NULL && existing->win != NULL) {
		boxen_window_raise(existing->win);
		boxen_window_focus(existing->win);
		return true;
	}

	/* Registry capacity check */
	if (g_registry.count >= BOXEN_OUTLINE_MAX_EDITORS) {
#ifndef BOXEN_OUTLINE_OMIT_MAIN
		log_warn(LOG_COMP_GENERAL,
		         "boxen_outline: max editors (%d) reached; cannot open '%s'",
		         BOXEN_OUTLINE_MAX_EDITORS, p);
#endif
		return false;
	}

	/* 2026-06-09 JES #691 C.1 round 2 P1: query terminal dimensions from boxen.
	 * boxen_get_screen_size() delegates to the initialized backend's width/height
	 * callbacks, falling back to 80x24 if boxen is not yet initialized.
	 * Previously hardcoded 80x24 here; this fix uses the actual terminal size. */
	int tw = 80;
	int th = 24;
	boxen_get_screen_size(&tw, &th);

	/* Allocate and initialize editor state */
	boxen_outline_state_t *s = (boxen_outline_state_t *)calloc(1, sizeof(*s));
	if (s == NULL) return false;

	boxen_outline_state_init(s, p, tw, th);

	/* Fetch the outline */
	if (!boxen_outline_refresh(s)) {
		boxen_outline_state_teardown(s);
		free(s);
		return false;
	}

	/* Register */
	if (!registry_add(s)) {
		boxen_outline_state_teardown(s);
		free(s);
		return false;
	}

	return true;
}

/* -------------------------------------------------------------------------
 * 2026-06-09 JES Phase C.1 #691: public API -- boxen_outline_close_all.
 * ---------------------------------------------------------------------- */
void boxen_outline_close_all(void) {
	if (!g_registry_inited) return;

	for (int i = 0; i < BOXEN_OUTLINE_MAX_EDITORS; i++) {
		boxen_outline_state_t *s = g_registry.editors[i];
		if (s != NULL) {
			boxen_outline_state_teardown(s);
			free(s);
			g_registry.editors[i] = NULL;
		}
	}
	g_registry.count = 0;
}

/* =========================================================================
 * Production-only section (GIL + ODB symbols).
 * Guards prevent these from being compiled into the test binary.
 * Pattern mirrors boxen_repl.c's #ifndef BOXEN_REPL_OMIT_MAIN section.
 * ========================================================================= */

#ifndef BOXEN_OUTLINE_OMIT_MAIN

/* 2026-06-09 JES Phase C.1 #691 round 2: include headers for ODB walk.
 * Only compiled in the production build (test build defines BOXEN_OUTLINE_OMIT_MAIN).
 *
 * Header chain that delivers the symbols we need:
 *   op.h        -> lang.h -> db.h -> memory.h    (texthandletostring)
 *   op.h        -> lang.h                         (externalvaluetype, hashtablelookup,
 *                                                  tyvaluerecord, hdlhashnode)
 *   op.h                                          (tyoutlinerecord.hsummit,
 *                                                  tyheadrecord fields, getheadstring)
 *   opinternal.h -> processinternal.h             (oppushoutline / oppopoutline stubs,
 *                                                  not used here but pulled in transitively)
 *   langexternal.h                                (tyexternalvariable.flinmemory,
 *                                                  .variabledata, .hdatabase)
 *   langinternal.h                                (langfastaddresstotable)
 *   db_format.h                                   (db_context, db_context_init,
 *                                                  db_context_init_legacy_read,
 *                                                  db_format_is_legacy_db)
 */
#include "../Common/headers/op.h"
#include "../Common/headers/opinternal.h"
#include "../Common/headers/strings.h"       /* copyptocstring, bigstring */
#include "../Common/headers/langexternal.h"  /* tyexternalvariable, hdlexternalvariable */
#include "../Common/headers/langinternal.h"  /* langfastaddresstotable */
#include "db_format.h"                       /* db_context, db_context_init* */

/* Forward declarations (same pattern as debug_handler.c lines 47-56). */
extern hdlhashtable roottable;                                              /* tablestructure.h */
extern boolean opverbinmemory(const struct db_context *, hdlexternalvariable); /* opverbs.c */
extern boolean db_format_is_legacy_db(hdldatabaserecord);                  /* db_format.c */

/* -------------------------------------------------------------------------
 * 2026-06-09 JES #691 Phase C.1 round 2: boxen_outline_walk_nodes
 *
 * Depth-first traversal of an hdloutlinerecord, populating a flat
 * boxen_outline_node_t array.  Called from boxen_outline_real_fetch after
 * the outline has been confirmed in memory.
 *
 * Walk algorithm:
 *   The outline tree is a doubly-linked list of tyheadrecord nodes.
 *   Each node's children are reachable via headlinkdown (first child).
 *   Siblings are reachable via headlinkright.
 *   headlinkup / headlinkleft provide reverse links; we don't use them here.
 *
 *   Field verification (from Common/headers/op.h:69-131):
 *     headlinkdown  hdlheadrecord   -- first child (nil if leaf)
 *     headlinkright hdlheadrecord   -- next sibling (nil if last)
 *     headlevel     short           -- indent depth (0 = top-level summit)
 *     flexpanded    boolean:1       -- true if children are visible
 *     flbreakpoint  boolean:1       -- breakpoint flag
 *     flcomment     boolean:1       -- comment flag
 *     headstring    Handle          -- headline text (read via getheadstring macro)
 *
 *   getheadstring macro (op.h:480):
 *     #define getheadstring(h,bs) texthandletostring((**(h)).headstring, bs)
 *     Copies the Handle content into bigstring bs (pascal-string, 255 byte cap).
 *
 * We use an explicit stack rather than recursion to avoid deep call stacks
 * on large outlines.  Maximum traversal depth is bounded by BOXEN_OUTLINE_MAX_NODES.
 *
 * GIL invariant: caller holds the GIL.  No op verbs are called; all accesses
 * are direct field reads on heap-resident handles.  This is safe as long as
 * flinmemory is true (ensured by opverbinmemory before this is called).
 * ---------------------------------------------------------------------- */

/*
 * walk_stack_entry: explicit DFS stack entry.
 * We push (hnode, indent_level) so we can resume sibling traversal after
 * processing children.
 */
typedef struct {
	hdlheadrecord hnode;
	int           level;
} walk_stack_entry_t;

#define WALK_STACK_MAX 256  /* max nesting depth for DFS traversal */

static int boxen_outline_walk_nodes(hdloutlinerecord houtline,
                                    boxen_outline_node_t *nodes,
                                    int max_nodes) {
	if (houtline == nil || nodes == NULL || max_nodes <= 0) return 0;

	/* tyoutlinerecord.hsummit (op.h:227): first top-level node */
	hdlheadrecord hsummit = (**houtline).hsummit;
	if (hsummit == nil) return 0;

	/* Explicit DFS stack: each entry is (current node, its level). */
	walk_stack_entry_t stack[WALK_STACK_MAX];
	int stack_top = 0;
	int node_count = 0;
	/* 2026-06-09 JES #691 C.1 round 2 P1: one-shot overflow warning. */
	bool warned_stack = false;

	/* Seed with the summit (level 0) */
	stack[stack_top].hnode = hsummit;
	stack[stack_top].level = 0;
	stack_top++;

	while (stack_top > 0 && node_count < max_nodes) {
		--stack_top;
		hdlheadrecord hnode = stack[stack_top].hnode;
		int           level  = stack[stack_top].level;

		if (hnode == nil) continue;

		/*
		 * Read node fields.
		 * tyheadrecord fields (op.h:71-131):
		 *   headlinkdown  hdlheadrecord  -- first child
		 *   headlinkright hdlheadrecord  -- next sibling
		 *   headlevel     short          -- indent level
		 *   flexpanded    boolean:1
		 *   flbreakpoint  boolean:1
		 *   flcomment     boolean:1
		 *   headstring    Handle         -- text (getheadstring -> bigstring)
		 */
		hdlheadrecord hfirst_child = (**hnode).headlinkdown;
		hdlheadrecord hnext_sibling = (**hnode).headlinkright;
		boolean       fexpanded    = (**hnode).flexpanded;
		boolean       fbreakpoint  = (**hnode).flbreakpoint;
		boolean       fcomment     = (**hnode).flcomment;
		bool          has_children = (hfirst_child != nil);

		/* Extract headline text via getheadstring (op.h:480).
		 * getheadstring copies headstring Handle into a pascal bigstring (255 byte cap). */
		bigstring bshead;
		setemptystring(bshead);
		if ((**hnode).headstring != nil) {
			getheadstring(hnode, bshead);
		}

		/* Populate display node */
		boxen_outline_node_t *n = &nodes[node_count++];
		memset(n, 0, sizeof(*n));

		/* Convert pascal-string to NUL-terminated C string, clamped to TEXT_MAX-1. */
		int textlen = (int)(unsigned char)bshead[0];
		if (textlen >= BOXEN_OUTLINE_TEXT_MAX) textlen = BOXEN_OUTLINE_TEXT_MAX - 1;
		memcpy(n->text, bshead + 1, (size_t)textlen);
		n->text[textlen] = '\0';

		n->level        = level;
		n->expanded     = (bool)fexpanded;
		n->has_children = has_children;
		n->flbreakpoint = (bool)fbreakpoint;
		n->flcomment    = (bool)fcomment;
		n->hnode_opaque = (void *)hnode;

		/* Marker */
		if (has_children) {
			n->marker = fexpanded ? BOXEN_OUTLINE_MARKER_EXPANDED
			                      : BOXEN_OUTLINE_MARKER_COLLAPSED;
		} else {
			n->marker = BOXEN_OUTLINE_MARKER_LEAF;
		}

		/*
		 * DFS ordering: push NEXT SIBLING first (so it's processed after
		 * children), then push FIRST CHILD on top (processed next).
		 * Only push children if the node is expanded or this is level 0.
		 */
		if (hnext_sibling != nil) {
			if (stack_top < WALK_STACK_MAX) {
				stack[stack_top].hnode = hnext_sibling;
				stack[stack_top].level = level;
				stack_top++;
			} else if (!warned_stack) {
				/* 2026-06-09 JES #691 C.1 round 2 P1: one-shot overflow warn */
				log_warn(LOG_COMP_GENERAL,
				         "boxen_outline: walk depth exceeded %d; deeper nodes omitted",
				         WALK_STACK_MAX);
				warned_stack = true;
			}
		}
		if (has_children && fexpanded) {
			if (stack_top < WALK_STACK_MAX) {
				stack[stack_top].hnode = hfirst_child;
				stack[stack_top].level = level + 1;
				stack_top++;
			} else if (!warned_stack) {
				/* 2026-06-09 JES #691 C.1 round 2 P1: one-shot overflow warn */
				log_warn(LOG_COMP_GENERAL,
				         "boxen_outline: walk depth exceeded %d; deeper nodes omitted",
				         WALK_STACK_MAX);
				warned_stack = true;
			}
		}
	}

	if (node_count >= max_nodes) {
		log_warn(LOG_COMP_GENERAL,
		         "boxen_outline: node cap (%d) hit; some nodes not shown",
		         max_nodes);
	}

	return node_count;
}

/* -------------------------------------------------------------------------
 * 2026-06-09 JES #691 Phase C.1 round 2: boxen_outline_real_fetch
 *
 * Production hook.  Resolves a dotted ODB path to an outline external
 * variable, loads it into memory if needed (opverbinmemory), then
 * delegates the tree walk to boxen_outline_walk_nodes.
 *
 * Path-parse + table-navigate pattern copied from
 * debug_handler.c::debug_get_script_source (lines 2085-2153).
 * Both split on the last dot to get (table_path, leaf_name).
 *
 * Error paths:
 *   - Path too short / no dot          -> log_warn, return false
 *   - Table not found                  -> log_warn, return false
 *   - Name not found in table          -> log_warn, return false
 *   - Value is not externalvaluetype   -> log_warn, return false
 *   - opverbinmemory fails             -> log_warn, return false
 *   - variabledata is nil              -> log_warn, return false
 * ---------------------------------------------------------------------- */
bool boxen_outline_real_fetch(boxen_outline_state_t *s,
                              const char *path,
                              boxen_outline_node_t *nodes,
                              int *node_count) {
	*node_count = 0;
	if (path == NULL || nodes == NULL) return false;

	/* --- Step 1: parse dotted path into (table_path, leaf_name) ---
	 * Mirror of debug_get_script_source path-parse block.
	 * bigstring is a pascal string: byte[0] = length, bytes[1..255] = content.
	 */
	int pathlen = (int)strlen(path);
	if (pathlen > 255) {
		/* 2026-06-09 JES #691 C.1 round 2 P1: explicit rejection instead of
		 * silent truncation.  A path longer than 255 bytes cannot be a valid
		 * bigstring (pascal-string cap is 255) so truncation would silently
		 * resolve to a wrong path. */
		log_warn(LOG_COMP_GENERAL,
		         "boxen_outline: path too long (%d bytes, max 255): '%.*s...'",
		         pathlen, 64, path);
		return false;
	}
	bigstring bsfullpath;
	bsfullpath[0] = (unsigned char)pathlen;
	memcpy(bsfullpath + 1, path, (size_t)pathlen);

	/* Find last dot */
	int lastdot = -1;
	for (int i = pathlen; i > 0; i--) {
		if (bsfullpath[i] == '.') { lastdot = i; break; }
	}
	if (lastdot < 0) {
		log_warn(LOG_COMP_GENERAL,
		         "boxen_outline: path '%s' has no dot; must be fully qualified",
		         path);
		return false;
	}

	bigstring bstablepath, bsname;
	bstablepath[0] = (unsigned char)(lastdot - 1);
	memcpy(bstablepath + 1, bsfullpath + 1, (size_t)(lastdot - 1));

	int namelen = pathlen - lastdot;
	bsname[0] = (unsigned char)namelen;
	memcpy(bsname + 1, bsfullpath + lastdot + 1, (size_t)namelen);

	/* --- Step 2: navigate to the containing table --- */
	hdlhashtable htable;
	if (!langfastaddresstotable(roottable, bstablepath, &htable)) {
		log_warn(LOG_COMP_GENERAL,
		         "boxen_outline: table not found for path '%s'", path);
		return false;
	}

	/* --- Step 3: look up the leaf value --- */
	tyvaluerecord val;
	hdlhashnode   hnode_table;
	if (!hashtablelookup(htable, bsname, &val, &hnode_table)) {
		log_warn(LOG_COMP_GENERAL,
		         "boxen_outline: '%s' not found in table", path);
		return false;
	}

	/* --- Step 4: verify it is an external (outline/script) value ---
	 * externalvaluetype = 13 (lang.h:230)
	 */
	if (val.valuetype != externalvaluetype) {
		log_warn(LOG_COMP_GENERAL,
		         "boxen_outline: '%s' is not an external value (type %d)",
		         path, (int)val.valuetype);
		return false;
	}

	/* val.data.externalvalue is a Handle field of tyvaluerecord (lang.h:351).
	 * Cast to hdlexternalvariable (langexternal.h:169). */
	hdlexternalvariable hv = (hdlexternalvariable)val.data.externalvalue;
	if (hv == nil) {
		log_warn(LOG_COMP_GENERAL,
		         "boxen_outline: '%s' external handle is nil", path);
		return false;
	}

	/* --- Step 5: load from disk if not yet in memory ---
	 * tyexternalvariable.flinmemory (langexternal.h:147):
	 *   if true, variabledata is a Handle; else a dbaddress.
	 * db_context setup mirrors debug_get_script_source (debug_handler.c:2136-2148).
	 */
	if (!(**hv).flinmemory) {
		db_context ctx;
		if ((**hv).hdatabase != nil && db_format_is_legacy_db((**hv).hdatabase)) {
			db_context_init_legacy_read(&ctx, (**hv).hdatabase);
		} else {
			db_context_init(&ctx);
			if ((**hv).hdatabase != nil)
				ctx.database = (**hv).hdatabase;
		}
		if (!opverbinmemory(&ctx, hv)) {
			log_warn(LOG_COMP_GENERAL,
			         "boxen_outline: opverbinmemory failed for '%s'", path);
			return false;
		}
	}

	/* --- Step 6: get the outline record ---
	 * tyexternalvariable.variabledata (langexternal.h:163): long, cast to Handle.
	 * For outline/script externals, variabledata IS the hdloutlinerecord.
	 * Pattern: debug_get_script_source line 2150:
	 *   hdloutlinerecord houtline = (hdloutlinerecord)(**hv).variabledata;
	 */
	hdloutlinerecord houtline = (hdloutlinerecord)(**hv).variabledata;
	if (houtline == nil) {
		log_warn(LOG_COMP_GENERAL,
		         "boxen_outline: outline record is nil for '%s'", path);
		return false;
	}

	/* 2026-06-09 JES #691 C.1 round 2 P0-2: stash houtline so the toggle
	 * hooks can use oppushoutline/opdirtyoutline/oppopoutline.  The stash
	 * is refreshed on every fetch, which covers re-open after serialize. */
	if (s != NULL) {
		s->houtline_opaque = (void *)houtline;
	}

	/* --- Step 7: walk the outline tree directly ---
	 * No outline context push needed: we read tyheadrecord fields directly.
	 * GIL is held by the REPL event loop for the duration of this call.
	 */
	int count = boxen_outline_walk_nodes(houtline, nodes, BOXEN_OUTLINE_MAX_NODES);
	*node_count = count;

	log_info(LOG_COMP_GENERAL,
	         "boxen_outline: fetched %d nodes for '%s'", count, path);
	return (count >= 0);
}

bool boxen_outline_real_toggle_breakpoint(boxen_outline_state_t *s, int node_idx) {
	/* 2026-06-09 JES #691 Phase C.1 round 2 P0-2: canonical toggle pattern.
	 *
	 * The original implementation wrote (**hnode).flbreakpoint directly
	 * without opdirtyoutline(), so Cmd-S discarded the change (the outline
	 * root did not know it was modified).
	 *
	 * Canonical pattern (Common/source/scripts.c:2863-2889 optogglebreakpoint):
	 *   oppushoutline(ho)
	 *   flrecentlychanged = (**ho).flrecentlychanged   // save
	 *   (**hnode).flbreakpoint = !(**hnode).flbreakpoint
	 *   opdirtyoutline()                               // marks outline dirty
	 *   (**ho).flrecentlychanged = flrecentlychanged   // restore (prevent recompile)
	 *   oppopoutline()
	 *
	 * oppushoutline makes ho the active outline context so that opdirtyoutline
	 * can find it via op_get_outlinedata().  In the boxen REPL event loop there
	 * is no outline context pushed, so we can't call opdirtyoutline without
	 * pushing first.  The stash at s->houtline_opaque (set by real_fetch) gives
	 * us the hdloutlinerecord to push. */
	if (s == NULL || node_idx < 0 || node_idx >= s->node_count) return false;
	hdloutlinerecord ho = (hdloutlinerecord)s->houtline_opaque;
	if (ho == nil) return false;

	boxen_outline_node_t *node = &s->nodes[node_idx];
	if (node->hnode_opaque == NULL) return false;
	hdlheadrecord hnode = (hdlheadrecord)node->hnode_opaque;

	/* 2026-06-09 JES #691 C.1 round 2 cosmetic per /gate concurrency note:
	 * GIL held throughout this block.  The flbreakpoint bit write is
	 * serialized against any debug-thread breakpoint check by the GIL
	 * (ADR-014: only the GIL holder runs ODB-touching C code).  No
	 * intervening yield points -- opdirtyoutline / oppopoutline do not
	 * release the GIL. */
	oppushoutline(ho);
	boolean flrecentlychanged = (**ho).flrecentlychanged;
	boolean new_val = !(**hnode).flbreakpoint;
	(**hnode).flbreakpoint = new_val;
	opdirtyoutline();
	(**ho).flrecentlychanged = flrecentlychanged;
	oppopoutline();

	/* Update the display node to match the live ODB state */
	node->flbreakpoint = (bool)new_val;
	return (bool)new_val;
}

bool boxen_outline_real_toggle_comment(boxen_outline_state_t *s, int node_idx) {
	/* 2026-06-09 JES #691 Phase C.1 round 2 P0-2: same canonical pattern
	 * as toggle_breakpoint above; toggles flcomment instead. */
	if (s == NULL || node_idx < 0 || node_idx >= s->node_count) return false;
	hdloutlinerecord ho = (hdloutlinerecord)s->houtline_opaque;
	if (ho == nil) return false;

	boxen_outline_node_t *node = &s->nodes[node_idx];
	if (node->hnode_opaque == NULL) return false;
	hdlheadrecord hnode = (hdlheadrecord)node->hnode_opaque;

	/* GIL held throughout this block; same serialization argument as
	 * toggle_breakpoint above. */
	oppushoutline(ho);
	boolean flrecentlychanged = (**ho).flrecentlychanged;
	boolean new_val = !(**hnode).flcomment;
	(**hnode).flcomment = new_val;
	opdirtyoutline();
	(**ho).flrecentlychanged = flrecentlychanged;
	oppopoutline();

	/* Update the display node to match the live ODB state */
	node->flcomment = (bool)new_val;
	return (bool)new_val;
}

void boxen_outline_real_script_run(const char *path) {
	if (path == NULL) return;
	/* 2026-06-09 JES #691 C.1: delegate to REPL /run slash command.
	 * The full wiring (call dispatch_slash_command with "/run @<path>") is
	 * deferred until the REPL and editor state are integrated (C.1.1). */
	log_warn(LOG_COMP_GENERAL,
	         "boxen_outline: /run for '%s' not yet wired (C.1.1)", path);
}

void boxen_outline_real_expand(boxen_outline_node_t *node, bool expand) {
	if (node == NULL || node->hnode_opaque == NULL) return;

	/* 2026-06-09 JES #691 Phase C.1 round 2 P0-1: directly flip the
	 * flexpanded bit instead of routing through opexpand/opcollapse.
	 * Those functions dereference op_get_outlinedata() as their first
	 * action (Common/source/opexpand.c:94 opcollapse_ctx, :220
	 * opexpand_ctx), but op_get_outlinedata() returns nil in the boxen
	 * REPL event loop -- there is no outline context pushed.  Calling
	 * them crashes on the first expand/collapse keypress.
	 *
	 * The direct flip is sufficient for C.1 because boxen_outline_refresh
	 * re-walks the structure after every expand/collapse operation, so the
	 * display list is rebuilt from the new flexpanded state.  The
	 * preexpand callbacks and drawing-invalidation side effects that
	 * opexpand/opcollapse perform are GUI-side and irrelevant to the
	 * boxen editor.
	 *
	 * C.2+ can revisit if GUI preexpand callbacks (e.g. lazy-load) become
	 * relevant.  In that case, stash the hdloutlinerecord on the editor
	 * state at fetch time and use oppushoutline/opexpand/oppopoutline. */
	hdlheadrecord hnode = (hdlheadrecord)node->hnode_opaque;
	/* 2026-06-09 JES #691 C.1 round 2 cosmetic per /gate concurrency note:
	 * GIL held -- write is serialized.  flexpanded is a single-bit field
	 * in tyheadrecord; under the GIL model (ADR-014) only the GIL holder
	 * runs ODB-touching C code, so this byte-sized write cannot race with
	 * any concurrent reader. */
	(**hnode).flexpanded = expand;

	/* Update the display node */
	node->expanded = expand;
	node->marker   = expand ? BOXEN_OUTLINE_MARKER_EXPANDED
	                        : BOXEN_OUTLINE_MARKER_COLLAPSED;
}

#endif /* !BOXEN_OUTLINE_OMIT_MAIN */
