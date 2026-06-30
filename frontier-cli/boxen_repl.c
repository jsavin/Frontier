/*
 * boxen_repl.c -- boxen-native Frontier REPL.
 *
 * C.0 tracer-bullet: minimum-viable boxen REPL.
 *
 * What C.0 ships:
 *   - boxen_init + two-window layout (output pane + input bar) + footer hint
 *   - Scrollback ring buffer (BOXEN_REPL_SCROLLBACK_SIZE lines)
 *   - Character-by-character input; Enter submits, Backspace deletes,
 *     Escape clears, Ctrl-C exits
 *   - Slash-prefixed lines dispatched through dispatch_slash_command
 *     (exposed from repl.c via repl_slash_dispatch.h); other lines dispatched
 *     through repl_eval_script
 *   - GIL discipline mirrored verbatim from debugger_tui_main
 *     (snapshot/restore around boxen_poll_event; ADR-014)
 *   - Heap-allocated state; drain-before-free contract (mirrors B.6/PR #722)
 *   - BOXEN_REPL_OMIT_MAIN guard isolates GIL symbols from test builds
 *     (same pattern as DEBUGGER_TUI_OMIT_MAIN)
 *
 * GIL discipline:
 *   - Snapshot hthreadglobals before yielding; restore after poll returns.
 *   - Pattern copied verbatim from debugger_tui.c:2912-2918.
 *   - Per DEBUGGER_TUI_HANDOFF.md and ADR-014.
 *
 * 2026-06-08 JES Phase C.0 #691
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Frontier contributors
 */

#include "boxen_repl.h"
#include "boxen_repl_internal.h"
#include "boxen_outline.h"  /* boxen_outline_open, boxen_outline_close_all -- 2026-06-09 JES Phase C.1 #691 */
#include "boxen_completion_popup.h"
#include "boxen_ui.h"		/* 2026-06-23 JES #691 Phase C.0.7g: dialog modal bridge */

#include "boxen/boxen.h"
#include "../Common/headers/logging.h"

/* 2026-06-21 JES #691 C.0.7f: forward-declare the palette source vtable
 * struct + dispose entry point at file scope so the teardown path can free
 * the palette_source field on the boxen_repl_state_t without pulling
 * repl_palette_source.h up out of the BOXEN_REPL_OMIT_MAIN block further
 * down.  File-scope declaration -- not function-scope -- so it does not
 * shadow the typedef declared in palette.h when that header is included
 * later in the production block. */
struct palette_menu_source;
extern void repl_palette_source_dispose(struct palette_menu_source *src);

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>    /* dup, dup2, pipe, close, read */
#include <fcntl.h>     /* fcntl, F_SETFL, O_NONBLOCK */
#include <errno.h>     /* EAGAIN, EWOULDBLOCK */
#include <sys/stat.h>  /* umask, fchmod, mode_t -- 2026-06-09 JES #691 Phase C.0.1 */
#include <time.h>      /* clock_gettime, CLOCK_MONOTONIC -- 2026-06-17 JES #691 Phase C.0.7a */

/* -------------------------------------------------------------------------
 * 2026-06-17 JES #691 Phase C.0.7a: monotonic time helper.
 *
 * Returns the current time in milliseconds (CLOCK_MONOTONIC).  Used by
 * the slash-palette debounce to measure elapsed time without blocking.
 *
 * Wrapped through the state->now_ms_hook seam so tests can inject a
 * deterministic counter without sleeping (see test_slash_* tests).
 * ---------------------------------------------------------------------- */
static uint64_t repl_real_now_ms(void) {
	struct timespec ts;
	if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return 0;
	return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)(ts.tv_nsec / 1000000LL);
}

static uint64_t repl_now_ms(boxen_repl_state_t *s) {
	if (s->now_ms_hook != NULL) return s->now_ms_hook();
	return repl_real_now_ms();
}

/* -------------------------------------------------------------------------
 * 2026-06-17 JES #691 Phase C.0.7a: boxen_repl_check_pending_slash.
 *
 * Called after every event (and on timeout) to fire the deferred palette
 * open if the debounce window has passed.  No-op when no debounce is pending
 * or when the deadline has not yet been reached.
 *
 * Production event loop calls this on both:
 *   - the BOXEN_ERR_TIMEOUT branch (user typed '/' and stopped typing)
 *   - the post-event branch after boxen_repl_run_one_tick (handles the rare
 *     case where a non-printable key -- e.g. resize -- arrives after the '/'
 *     and more than 350ms of real time has elapsed)
 *
 * Tests call it directly after advancing now_ms_hook to simulate time elapse
 * without any real sleep.
 * ---------------------------------------------------------------------- */
void boxen_repl_check_pending_slash(boxen_repl_state_t *s) {
	if (s == NULL || s->slash_pending_until_ms == 0) return;
	if (s->palette_open_hook == NULL || s->palette_state != NULL) {
		/* Hook disappeared or palette already open: cancel pending. */
		s->slash_pending_until_ms = 0;
		return;
	}
	uint64_t now = repl_now_ms(s);
	if (now < s->slash_pending_until_ms) return;  /* still within window */

	/* Deadline passed: open the palette. */
	s->slash_pending_until_ms = 0;
	s->palette_open_hook(s);
}

/* -------------------------------------------------------------------------
 * Forward declarations for draw and input callbacks
 * ---------------------------------------------------------------------- */
static void draw_output_pane(boxen_window_t *win, void *user_data);
static void draw_input_bar(boxen_window_t *win, void *user_data);
static void draw_footer_hint(boxen_window_t *win, void *user_data);
static void on_input(boxen_window_t *win, const boxen_event_t *ev,
                     void *user_data);

/* -------------------------------------------------------------------------
 * 2026-06-08 JES Phase C.0 #691: layout constants.
 *
 * Two-region layout (bottom-up):
 *   row (th-1): footer hint (borderless, pinned)
 *   row (th-2): input bar   (borderless, pinned)
 *   rows 0..(th-3): output pane (scrollback)
 *
 * Minimum terminal height: 4 rows.
 * ---------------------------------------------------------------------- */

#define REPL_INPUT_PROMPT "> "

/* 2026-06-29 JES #803: PgUp/PgDn advertise output-pane scrollback. */
#define REPL_FOOTER_TEXT "Ctrl-C: quit  Enter: run  Esc: clear  PgUp/PgDn: scroll"

/* Width of the "> " prompt prefix in the input bar */
#define REPL_INPUT_PROMPT_LEN 2

/* -------------------------------------------------------------------------
 * 2026-06-08 JES Phase C.0 #691: repl_build_layout.
 *
 * Creates the three boxen windows.  Any pre-existing windows are closed
 * first.  Called from boxen_repl_state_init and the resize path in
 * boxen_repl_run_one_tick.
 *
 * Window geometry (mirrors debugger_tui.c::tui_build_layout pattern):
 *   - footer:  borderless, pinned to bottom, 1 row
 *   - input:   borderless, pinned to bottom, 1 row (above footer)
 *   - output:  takes the rest
 * ---------------------------------------------------------------------- */
static void repl_build_layout(boxen_repl_state_t *s, int tw, int th) {
	/* Close any existing windows */
	if (s->footer_win != NULL) { boxen_window_close(s->footer_win); s->footer_win = NULL; }
	if (s->input_win  != NULL) { boxen_window_close(s->input_win);  s->input_win  = NULL; }
	if (s->output_win != NULL) { boxen_window_close(s->output_win); s->output_win = NULL; }

	if (tw < 4) tw = 4;
	if (th < 4) th = 4;

	/* Footer: bottom row, borderless */
	boxen_rect_t footer_rect = { 0, th - 1, tw, 1 };
	s->footer_win = boxen_window_open("footer", footer_rect, NULL);
	if (s->footer_win != NULL) {
		boxen_window_set_borders(s->footer_win, false);
		boxen_window_set_pinned(s->footer_win, BOXEN_PIN_BOTTOM);
	}

	/* Input bar: row above footer, borderless */
	boxen_rect_t input_rect = { 0, th - 2, tw, 1 };
	s->input_win  = boxen_window_open("input", input_rect, NULL);
	if (s->input_win != NULL) {
		boxen_window_set_borders(s->input_win, false);
	}

	/* Output pane: everything above the input bar */
	int output_h = th - 2;
	if (output_h < 1) output_h = 1;
	boxen_rect_t output_rect = { 0, 0, tw, output_h };
	s->output_win = boxen_window_open("output", output_rect, NULL);
	if (s->output_win != NULL) {
		boxen_window_set_borders(s->output_win, false);
	}
}

/* -------------------------------------------------------------------------
 * 2026-06-08 JES Phase C.0 #691: repl_wire_callbacks.
 * ---------------------------------------------------------------------- */
/* 2026-06-10 JES #691 C.1.x: filter for boxen application-global keys.
 *
 * Returns true for keys that should always reach the REPL's on_input
 * regardless of which window has focus.  Currently just Ctrl-C: without
 * this, opening an outline editor with /edit @path would trap Ctrl-C
 * inside the editor (which doesn't recognize it) and the user couldn't
 * exit the REPL.
 *
 * Future additions (deferred):
 *   - `/` to launch the slash-menu palette (palette migration is its own
 *     follow-up milestone; not yet integrated with boxen REPL).
 *   - Application function keys for window switching (cmd-`, etc.) if /
 *     when multi-window editor focus becomes a UX problem. */
static bool repl_is_global_key(const boxen_event_t *ev) {
	if (ev == NULL || ev->type != BOXEN_EV_KEY) return false;
	if (ev->key.key == BOXEN_KEY_CTRL_C) return true;
	return false;
}

static void repl_wire_callbacks(boxen_repl_state_t *s) {
	if (s->output_win != NULL) {
		boxen_window_set_draw(s->output_win, draw_output_pane);
		boxen_window_set_user_data(s->output_win, s);
		/* 2026-06-29 JES #805: wire on_input on the output pane too so
		 * mouse-wheel events (BOXEN_EV_MOUSE button==4/5) -- which
		 * boxen routes to the topmost window at the pointer location --
		 * reach the same scroll-offset handler used by PgUp/PgDn.  The
		 * handler ignores BOXEN_EV_KEY on this window (keyboard focus
		 * remains the input bar). */
		boxen_window_set_input(s->output_win, on_input);
	}
	if (s->input_win != NULL) {
		boxen_window_set_draw(s->input_win,  draw_input_bar);
		boxen_window_set_input(s->input_win, on_input);
		boxen_window_set_user_data(s->input_win, s);
	}
	if (s->footer_win != NULL) {
		boxen_window_set_draw(s->footer_win, draw_footer_hint);
		boxen_window_set_user_data(s->footer_win, s);
	}

	/* Focus the input bar so key events route to on_input */
	if (s->input_win != NULL) {
		boxen_window_focus(s->input_win);

		/* 2026-06-10 JES #691 C.1.x: register the REPL input as the global
		 * key target.  Ctrl-C (and any future global keys) will route here
		 * before the focused window sees them, so an outline editor or
		 * other child window can't trap them. */
		boxen_set_global_key_handler(s->input_win, repl_is_global_key);
	}
}

/* -------------------------------------------------------------------------
 * 2026-06-08 JES Phase C.0 #691: draw callbacks.
 * ---------------------------------------------------------------------- */

static void draw_output_pane(boxen_window_t *win, void *user_data) {
	boxen_repl_state_t *s = (boxen_repl_state_t *)user_data;
	if (s == NULL) return;

	int w = boxen_window_content_width(win);
	int h = boxen_window_content_height(win);
	if (w <= 0 || h <= 0) return;

	/* Clear the pane */
	{
		boxen_rect_t r = { 0, 0, w, h };
		boxen_fill_rect(win, r, ' ',
		                BOXEN_COLOR_DEFAULT, BOXEN_COLOR_DEFAULT, BOXEN_ATTR_NONE);
	}

	/* Render scrollback lines bottom-justified: most-recent at row (h-1).
	 * Show the last `show_count` entries where show_count = min(count, h).
	 *
	 * 2026-06-29 JES #803: when output_scroll_offset > 0, the view is
	 * shifted up by that many lines.  The newest visible line becomes
	 * (scrollback_count - 1 - offset), and we walk back `h` lines from
	 * there.  Clamp the offset so at least one line stays visible. */
	int total  = s->scrollback_count;
	int offset = s->output_scroll_offset;
	if (offset < 0) offset = 0;
	if (total > 0 && offset > total - 1) offset = total - 1;
	/* Write the clamp back so PgUp/PgDn don't accumulate past the bounds. */
	s->output_scroll_offset = offset;

	int count = total - offset;
	if (count > h) count = h;
	if (count <= 0) return;

	char linebuf[BOXEN_REPL_SCROLLBACK_LINE_MAX];
	int  cap = (w < (int)(sizeof(linebuf) - 1)) ? w : (int)(sizeof(linebuf) - 1);

	for (int i = 0; i < count; i++) {
		/* Newest visible entry is at logical position (total - 1 - offset).
		 * Walk back `count` entries from there.  Ring index for the i-th
		 * row (0 = topmost visible) is:
		 *   (scrollback_head - 1 - offset - (count - 1 - i)) % SIZE
		 * which simplifies to:
		 *   (scrollback_head - offset - count + i) % SIZE */
		int ring_idx = (s->scrollback_head - offset - count + i
		                + BOXEN_REPL_SCROLLBACK_SIZE)
		               % BOXEN_REPL_SCROLLBACK_SIZE;
		const char *line = s->scrollback[ring_idx];
		if (line == NULL) continue;

		int row = h - count + i;  /* bottom-justify */
		if (row < 0 || row >= h) continue;

		/* Truncate to width and space-pad for clean drawing */
		int n = snprintf(linebuf, (size_t)(cap + 1), "%-*.*s", cap, cap, line);
		(void)n;
		boxen_draw_text(win, 0, row, linebuf,
		                BOXEN_COLOR_DEFAULT, BOXEN_COLOR_DEFAULT, BOXEN_ATTR_NONE);
	}
}

static void draw_input_bar(boxen_window_t *win, void *user_data) {
	boxen_repl_state_t *s = (boxen_repl_state_t *)user_data;
	if (s == NULL) return;

	int w = boxen_window_content_width(win);
	if (w <= 0) return;

	/* Build the full input row: prompt + typed text, padded to width.
	 * 2026-06-10 JES #691 Phase C.0.5: swap "> " for ".." in multi-line mode.
	 * Both prompts are REPL_INPUT_PROMPT_LEN (2) chars wide, so cursor math
	 * is unchanged. */
	const char *prompt = (s->multiline_lines > 0) ? ".." : REPL_INPUT_PROMPT;
	char linebuf[BOXEN_REPL_INPUT_MAX + REPL_INPUT_PROMPT_LEN + 4];
	int  cap = (w < (int)(sizeof(linebuf) - 1)) ? w : (int)(sizeof(linebuf) - 1);

	int n = snprintf(linebuf, (size_t)(cap + 1), "%-*.*s",
	                 cap, cap,
	                 prompt);
	(void)n;

	/* Overlay typed text starting at REPL_INPUT_PROMPT_LEN */
	int text_len = s->input_len;
	int text_avail = cap - REPL_INPUT_PROMPT_LEN;
	if (text_len > text_avail) text_len = text_avail;
	if (text_len > 0 && text_avail > 0) {
		memcpy(linebuf + REPL_INPUT_PROMPT_LEN, s->input_buf, (size_t)text_len);
		/* Re-pad trailing part */
		int pad_start = REPL_INPUT_PROMPT_LEN + text_len;
		if (pad_start < cap) {
			memset(linebuf + pad_start, ' ', (size_t)(cap - pad_start));
		}
		linebuf[cap] = '\0';
	}

	boxen_draw_text(win, 0, 0, linebuf,
	                BOXEN_COLOR_DEFAULT, BOXEN_COLOR_DEFAULT, BOXEN_ATTR_NONE);

	/* 2026-06-17 JES Phase C.0.7e #691: position the terminal cursor at
	 * input_cursor_pos (the caret), not at the end of input.  Before C.0.7e
	 * both were always equal (append-only); now they diverge during editing. */
	int cursor_col = REPL_INPUT_PROMPT_LEN + s->input_cursor_pos;
	if (cursor_col >= cap) cursor_col = cap - 1;
	boxen_window_set_cursor(win, cursor_col, 0);
}

static void draw_footer_hint(boxen_window_t *win, void *user_data) {
	(void)user_data;
	int w = boxen_window_content_width(win);
	if (w <= 0) return;

	char linebuf[512];
	int cap = (w < (int)(sizeof(linebuf) - 1)) ? w : (int)(sizeof(linebuf) - 1);
	int n = snprintf(linebuf, (size_t)(cap + 1), "%-*.*s", cap, cap, REPL_FOOTER_TEXT);
	(void)n;
	boxen_draw_text(win, 0, 0, linebuf,
	                BOXEN_COLOR_DEFAULT, BOXEN_COLOR_DEFAULT, BOXEN_ATTR_DIM);
}

/* -------------------------------------------------------------------------
 * 2026-06-08 JES Phase C.0 #691: boxen_repl_append_scrollback.
 *
 * Ring-buffer append.  When the ring is full, the entry at scrollback_head
 * is the oldest; free it and reuse the slot.
 * ---------------------------------------------------------------------- */
void boxen_repl_append_scrollback(boxen_repl_state_t *s, const char *line) {
	if (s == NULL || line == NULL) return;

	int idx = s->scrollback_head;

	/* 2026-06-29 JES #803: snapshot ring-fullness before the append so the
	 * anchor-follows-content logic below can tell whether this append
	 * actually overwrote an old slot (vs. the ring still filling). */
	bool was_full_before = (s->scrollback_count == BOXEN_REPL_SCROLLBACK_SIZE);

	/* Free the slot being overwritten (oldest entry when ring is full) */
	if (s->scrollback[idx] != NULL) {
		free(s->scrollback[idx]);
		s->scrollback[idx] = NULL;
	}

	s->scrollback[idx] = strdup(line);
	s->scrollback_head = (idx + 1) % BOXEN_REPL_SCROLLBACK_SIZE;

	/* Count grows until the ring is full */
	if (s->scrollback_count < BOXEN_REPL_SCROLLBACK_SIZE) {
		s->scrollback_count++;
	}

	/* 2026-06-29 JES #803: anchor-follows-content when ring is full.
	 *
	 * When the user is scrolled back (output_scroll_offset > 0) and the
	 * ring was already full before this append, the oldest slot just
	 * got overwritten.  Without compensation, the user's view -- which
	 * is "N lines back from the newest" -- silently slides one line
	 * newer and the content they were reading gets overwritten under
	 * them.  Bumping the offset by 1 keeps them anchored to the same
	 * logical content.  Less/tmux follow this convention.
	 *
	 * Cap at (scrollback_count - 1) so at least one line stays visible.
	 * Guard on output_scroll_offset > 0: pinned-to-bottom view (offset
	 * == 0) is by convention "always show newest" and must NOT auto-
	 * advance, since that's the whole point of the snap-on-submit
	 * behavior in submit_input. */
	if (was_full_before && s->output_scroll_offset > 0) {
		int max_offset = s->scrollback_count - 1;
		if (max_offset < 0) max_offset = 0;
		if (s->output_scroll_offset < max_offset) {
			s->output_scroll_offset++;
		}
	}

	if (s->output_win != NULL) {
		boxen_window_invalidate(s->output_win);
	}
}

/* -------------------------------------------------------------------------
 * 2026-06-09 JES #691 Phase C.0.1: persistent command history.
 *
 * History ring constants.
 * MUST stay byte-identical with repl.c:85-86 until linenoise REPL is
 * removed in milestone C.6 (see planning/phase_c/REPL_SCHISM_EXECUTION_PLAN.md).
 * Both REPLs read/write the same ~/.frontier_history file.
 * ---------------------------------------------------------------------- */
#define BOXEN_REPL_HISTORY_FILE  ".frontier_history"

/* -------------------------------------------------------------------------
 * 2026-06-09 JES #691 Phase C.0.1: test-only path override.
 *
 * Compiled only under BOXEN_REPL_OMIT_MAIN (test builds).  Redirects
 * ~/.frontier_history resolution to a caller-supplied path so tests can
 * exercise load/save without touching the real history file.
 * ---------------------------------------------------------------------- */
#ifdef BOXEN_REPL_OMIT_MAIN
static char g_test_history_path[1024];
static bool g_test_history_path_set = false;

void boxen_repl_set_history_path_for_test(const char *path) {
	if (path == NULL) {
		g_test_history_path_set = false;
		g_test_history_path[0]  = '\0';
		return;
	}
	snprintf(g_test_history_path, sizeof(g_test_history_path), "%s", path);
	g_test_history_path_set = true;
}
#endif /* BOXEN_REPL_OMIT_MAIN */

/* -------------------------------------------------------------------------
 * 2026-06-09 JES #691 Phase C.0.1: resolve_history_path.
 *
 * Fills `out` (capacity `cap`) with the path to the history file.
 * In test builds, uses the override path if set.
 * In production builds, reads $HOME and appends BOXEN_REPL_HISTORY_FILE.
 * Returns true on success, false if the path could not be resolved.
 * ---------------------------------------------------------------------- */
static bool resolve_history_path(char *out, size_t cap) {
#ifdef BOXEN_REPL_OMIT_MAIN
	if (g_test_history_path_set) {
		snprintf(out, cap, "%s", g_test_history_path);
		return true;
	}
#endif
	const char *home = getenv("HOME");
	if (home == NULL) {
		log_warn(LOG_COMP_GENERAL, "boxen_repl: HOME not set; history disabled");
		return false;
	}
	int written = snprintf(out, cap, "%s/%s", home, BOXEN_REPL_HISTORY_FILE);
	if (written < 0 || (size_t)written >= cap) {
		log_warn(LOG_COMP_GENERAL, "boxen_repl: HOME path too long; history disabled");
		return false;
	}
	return true;
}

/* -------------------------------------------------------------------------
 * 2026-06-09 JES #691 Phase C.0.1: boxen_repl_history_append.
 *
 * Appends `line` to the history ring.  Skips NULL/empty lines and
 * duplicates of the most-recent entry.  Resets navigation state.
 * ---------------------------------------------------------------------- */
void boxen_repl_history_append(boxen_repl_state_t *s, const char *line) {
	if (s == NULL || line == NULL || line[0] == '\0') return;

	/* Dedup: skip if equal to the most-recent entry */
	if (s->history_count > 0) {
		int prev_idx = (s->history_head - 1 + BOXEN_REPL_HISTORY_SIZE)
		               % BOXEN_REPL_HISTORY_SIZE;
		if (strcmp(s->history[prev_idx], line) == 0) {
			/* Duplicate -- reset nav and return */
			s->history_nav_idx         = -1;
			s->history_saved_input[0]  = '\0';
			return;
		}
	}

	/* Write to ring */
	snprintf(s->history[s->history_head],
	         BOXEN_REPL_INPUT_MAX, "%s", line);
	s->history_head = (s->history_head + 1) % BOXEN_REPL_HISTORY_SIZE;
	if (s->history_count < BOXEN_REPL_HISTORY_SIZE) {
		s->history_count++;
	}

	/* Reset navigation */
	s->history_nav_idx        = -1;
	s->history_saved_input[0] = '\0';
}

/* -------------------------------------------------------------------------
 * 2026-06-09 JES #691 Phase C.0.1: boxen_repl_history_load.
 *
 * Loads history from the on-disk file into the ring.  Silent no-op if the
 * file cannot be opened (first run, or no $HOME).  Strips trailing \r/\n;
 * skips empty lines.
 * ---------------------------------------------------------------------- */
void boxen_repl_history_load(boxen_repl_state_t *s) {
	if (s == NULL) return;

	char path[1024];
	if (!resolve_history_path(path, sizeof(path))) return;

	FILE *f = fopen(path, "r");
	if (f == NULL) return;  /* silent no-op on first run */

	char line[BOXEN_REPL_INPUT_MAX];
	while (fgets(line, sizeof(line), f)) {
		/* Strip trailing \r and/or \n */
		size_t len = strlen(line);
		while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
			line[--len] = '\0';
		}

		/* 2026-06-09 JES #691 Phase C.0.1 P1: drop linenoise-written entries
		 * that exceed BOXEN_REPL_INPUT_MAX rather than splitting them.
		 * Preserves the invariant that every ring entry is something the user
		 * could type in the boxen REPL, and avoids corrupting the shared
		 * ~/.frontier_history file when linenoise had recorded a >INPUT_MAX
		 * entry.  A full-capacity fgets read (sizeof(line)-1 bytes) with no
		 * trailing newline AND not at EOF means the line continues. */
		if (len == BOXEN_REPL_INPUT_MAX - 1 && !feof(f)) {
			/* Line was too long: drain the rest and skip it. */
			int c;
			while ((c = fgetc(f)) != EOF && c != '\n') { /* discard */ }
			continue;
		}

		if (len == 0) continue;
		boxen_repl_history_append(s, line);
	}
	fclose(f);
}

/* -------------------------------------------------------------------------
 * 2026-06-09 JES #691 Phase C.0.1: boxen_repl_history_save.
 *
 * Merges the session ring with the existing on-disk file (read-merge-write),
 * deduplicates, trims to BOXEN_REPL_HISTORY_SIZE, and writes back.
 *
 * Mirrors repl.c::merge_and_save_history (lines 1782-1884).
 * ---------------------------------------------------------------------- */
void boxen_repl_history_save(boxen_repl_state_t *s) {
	if (s == NULL) return;

	char path[1024];
	if (!resolve_history_path(path, sizeof(path))) return;

	/* ---- Step 1: read existing file into a dynamic array ---- */
	char **file_lines   = NULL;
	size_t file_count   = 0;
	size_t file_cap     = 0;

	FILE *f = fopen(path, "r");
	if (f != NULL) {
		char line[BOXEN_REPL_INPUT_MAX];
		while (fgets(line, sizeof(line), f)) {
			/* Strip trailing \r/\n */
			size_t len = strlen(line);
			while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
				line[--len] = '\0';
			}
			if (len == 0) continue;

			/* Grow array if needed */
			if (file_count >= file_cap) {
				file_cap = file_cap ? file_cap * 2 : 256;
				char **tmp = realloc(file_lines, file_cap * sizeof(char *));
				if (tmp == NULL) break;
				file_lines = tmp;
			}
			file_lines[file_count] = strdup(line);
			if (file_lines[file_count] != NULL) file_count++;
		}
		fclose(f);
	}

	/* ---- Step 2: build session view (oldest -> newest walk of ring) ---- */
	/* Ring layout: history_head points at next-write; oldest is
	 * (head - count + SIZE) % SIZE, newest is (head - 1 + SIZE) % SIZE. */
	int    session_count = s->history_count;
	int    session_start = (s->history_head - session_count
	                        + BOXEN_REPL_HISTORY_SIZE) % BOXEN_REPL_HISTORY_SIZE;

	/* ---- Step 3: merge file lines (skip those that match any session entry) ---- */
	size_t total = (size_t)session_count + file_count;
	if (total == 0) {
		free(file_lines);
		return;
	}

	char **merged = malloc(total * sizeof(char *));
	if (merged == NULL) {
		for (size_t i = 0; i < file_count; i++) free(file_lines[i]);
		free(file_lines);
		return;
	}
	size_t merged_count = 0;

	/* Add file lines that are NOT in the session ring */
	for (size_t i = 0; i < file_count; i++) {
		int is_dup = 0;
		for (int j = 0; j < session_count && !is_dup; j++) {
			int idx = (session_start + j) % BOXEN_REPL_HISTORY_SIZE;
			if (strcmp(file_lines[i], s->history[idx]) == 0) {
				is_dup = 1;
			}
		}
		if (!is_dup) {
			merged[merged_count++] = file_lines[i];
		} else {
			free(file_lines[i]);
		}
	}

	/* Append all session entries (oldest to newest) */
	for (int j = 0; j < session_count; j++) {
		int idx = (session_start + j) % BOXEN_REPL_HISTORY_SIZE;
		char *dup = strdup(s->history[idx]);
		if (dup != NULL) {
			merged[merged_count++] = dup;
		}
	}

	/* ---- Step 4: trim to BOXEN_REPL_HISTORY_SIZE (keep most recent) ---- */
	size_t start = 0;
	if (merged_count > BOXEN_REPL_HISTORY_SIZE) {
		start = merged_count - BOXEN_REPL_HISTORY_SIZE;
		for (size_t i = 0; i < start; i++) {
			free(merged[i]);
		}
	}

	/* ---- Step 5: write back ---- */
	/* 2026-06-09 JES #691 Phase C.0.1: force ~/.frontier_history to mode 0600.
	 * Matches linenoise.c behavior in repl.c to prevent world-readable history
	 * across the migration window.  We save/restore the process umask rather
	 * than relying on the ambient value (which may be 0022 -> file 0644). */
	mode_t old_umask = umask(S_IXUSR | S_IRWXG | S_IRWXO);
	f = fopen(path, "w");
	umask(old_umask);
	if (f != NULL) {
		fchmod(fileno(f), S_IRUSR | S_IWUSR);
		for (size_t i = start; i < merged_count; i++) {
			fprintf(f, "%s\n", merged[i]);
		}
		fclose(f);
	}

	/* ---- Cleanup ---- */
	for (size_t i = start; i < merged_count; i++) {
		free(merged[i]);
	}
	free(merged);
	free(file_lines);
}

/* -------------------------------------------------------------------------
 * 2026-06-09 JES #691 Phase C.0.1: history_nav_up / history_nav_down.
 *
 * Called from on_input when UP/DOWN arrows are received.  Navigate the
 * history ring and update input_buf / input_cursor accordingly.
 * ---------------------------------------------------------------------- */
static void history_nav_up(boxen_repl_state_t *s) {
	if (s->history_count == 0) return;

	if (s->history_nav_idx == -1) {
		/* Save whatever the user has typed so far */
		snprintf(s->history_saved_input, BOXEN_REPL_INPUT_MAX, "%s", s->input_buf);
		s->history_nav_idx = 0;
	} else if (s->history_nav_idx < s->history_count - 1) {
		s->history_nav_idx++;
	}
	/* else already at oldest entry -- clamp */

	int ring_idx = (s->history_head - 1 - s->history_nav_idx
	                + BOXEN_REPL_HISTORY_SIZE) % BOXEN_REPL_HISTORY_SIZE;
	snprintf(s->input_buf, BOXEN_REPL_INPUT_MAX, "%s", s->history[ring_idx]);
	s->input_len        = (int)strlen(s->input_buf);
	s->input_cursor_pos = s->input_len;   /* caret jumps to end after history recall */

	if (s->input_win != NULL) boxen_window_invalidate(s->input_win);
}

static void history_nav_down(boxen_repl_state_t *s) {
	if (s->history_nav_idx == -1) return;  /* not navigating */

	if (s->history_nav_idx > 0) {
		s->history_nav_idx--;
		int ring_idx = (s->history_head - 1 - s->history_nav_idx
		                + BOXEN_REPL_HISTORY_SIZE) % BOXEN_REPL_HISTORY_SIZE;
		snprintf(s->input_buf, BOXEN_REPL_INPUT_MAX, "%s", s->history[ring_idx]);
	} else {
		/* nav_idx == 0: restore the saved typing */
		snprintf(s->input_buf, BOXEN_REPL_INPUT_MAX, "%s", s->history_saved_input);
		s->history_nav_idx        = -1;
		s->history_saved_input[0] = '\0';
	}
	s->input_len        = (int)strlen(s->input_buf);
	s->input_cursor_pos = s->input_len;   /* caret jumps to end after history recall */

	if (s->input_win != NULL) boxen_window_invalidate(s->input_win);
}

/* -------------------------------------------------------------------------
 * 2026-06-08 JES Phase C.0 #691: submit_input.
 *
 * Called on Enter.  Echoes the input line to the scrollback, dispatches,
 * and clears the input buffer.
 *
 * 2026-06-10 JES #691 Phase C.0.5: multi-line continuation via trailing '\'.
 *
 * If the input line ends with '\', the backslash is stripped and the line
 * (plus a '\n' separator) is accumulated into multiline_buf.  The input bar
 * is cleared and a new prompt is shown; the accumulated content is NOT
 * dispatched yet.
 *
 * If multiline_lines > 0 and the current line does NOT end with '\', the
 * current line is appended to multiline_buf and the joined buffer is
 * dispatched through the normal slash/eval path.  The multiline state is
 * then reset.
 *
 * History: boxen_repl_history_append is skipped while accumulating
 * (multiline_lines > 0) because the on-disk format is line-oriented and
 * embedded '\n' would corrupt ~/.frontier_history.
 *
 * Overflow: if appending a continuation line would exceed BOXEN_REPL_MULTILINE_MAX,
 * log_warn is emitted and the backslash is treated as a terminator (dispatch
 * what we have).
 * ---------------------------------------------------------------------- */
static void submit_input(boxen_repl_state_t *s) {
	/* 2026-06-10 JES #691 Phase C.0.5: empty-line guard.
	 * In single-line mode, empty Enter is a no-op.  In multi-line mode, empty
	 * Enter is a valid terminator (submits the accumulated buffer). */
	if (s->input_len == 0 && s->multiline_lines == 0) {
		if (s->input_win != NULL) boxen_window_invalidate(s->input_win);
		return;
	}

	/* 2026-06-10 JES #691 Phase C.0.5: detect trailing '\' continuation.
	 * The check fires regardless of context (matches standard shell semantics).
	 * A '\' inside a UserTalk string literal at end of line will still cause
	 * continuation; this is intentional and consistent with bash/Python REPL
	 * behavior. */
	int src_len = s->input_len;
	bool is_continuation = (src_len > 0 && s->input_buf[src_len - 1] == '\\');
	if (is_continuation) {
		src_len--;   /* strip the trailing backslash */
	}

	/* 2026-06-10 JES #691 Phase C.0.5: accumulate continuation line. */
	if (is_continuation) {
		/* Overflow check: need src_len bytes + '\n' separator. */
		bool overflow = ((s->multiline_len + src_len + 1) >= BOXEN_REPL_MULTILINE_MAX);
		if (overflow) {
			log_warn(LOG_COMP_GENERAL,
			         "boxen_repl: multi-line accumulator overflow; treating as terminator");
			/* Fall through with is_continuation = false to dispatch what we have. */
			is_continuation = false;
		} else {
			/* Echo with prompt prefix */
			char echo_buf[BOXEN_REPL_INPUT_MAX + 8];
			const char *echo_prefix = (s->multiline_lines == 0) ? "> " : "..";
			snprintf(echo_buf, sizeof(echo_buf), "%s%.*s\\",
			         echo_prefix, src_len, s->input_buf);
			boxen_repl_append_scrollback(s, echo_buf);

			/* Append line (without backslash) + '\n' to accumulator */
			if (src_len > 0) {
				memcpy(s->multiline_buf + s->multiline_len, s->input_buf,
				       (size_t)src_len);
				s->multiline_len += src_len;
			}
			s->multiline_buf[s->multiline_len] = '\n';
			s->multiline_len++;
			s->multiline_buf[s->multiline_len] = '\0';

			s->multiline_lines++;

			/* Clear input and redraw */
			s->input_buf[0]     = '\0';
			s->input_len        = 0;
			s->input_cursor_pos = 0;
			if (s->input_win  != NULL) boxen_window_invalidate(s->input_win);
			if (s->output_win != NULL) boxen_window_invalidate(s->output_win);
			return;
		}
	}

	/* At this point we have a terminator line (with or without prior continuation).
	 * Build the dispatch buffer: either the raw input_buf (single-line) or the
	 * joined multiline_buf + current line (multi-line path). */

	/* Buffer that holds the expression to dispatch.  In the multi-line case
	 * we build it on the stack; in the single-line case it is input_buf itself. */
	char joined[BOXEN_REPL_MULTILINE_MAX];
	const char *dispatch_buf;

	if (s->multiline_lines > 0) {
		/* Echo the terminator line with ".." prefix */
		char echo_buf[BOXEN_REPL_INPUT_MAX + 8];
		snprintf(echo_buf, sizeof(echo_buf), "..%.*s", src_len, s->input_buf);
		boxen_repl_append_scrollback(s, echo_buf);

		/* Join: accumulator already ends with '\n'; append the current line */
		int joined_len = s->multiline_len;
		int avail = BOXEN_REPL_MULTILINE_MAX - joined_len - 1;
		int copy_len = (src_len < avail) ? src_len : avail;
		/* 2026-06-10 JES #691 Phase C.0.5: terminator line truncated to fit
		 * the multi-line accumulator; matches the continuation-overflow
		 * behavior at the top of submit_input. */
		if (copy_len < src_len) {
			log_warn(LOG_COMP_GENERAL,
			         "boxen_repl: multi-line terminator truncated (%d -> %d bytes)",
			         src_len, copy_len);
		}
		memcpy(joined, s->multiline_buf, (size_t)s->multiline_len);
		if (copy_len > 0) {
			memcpy(joined + joined_len, s->input_buf, (size_t)copy_len);
			joined_len += copy_len;
		}
		joined[joined_len] = '\0';
		dispatch_buf = joined;

		/* 2026-06-10 JES #691 Phase C.0.5 (D5): skip history for multi-line
		 * entries -- on-disk format is line-oriented; embedded '\n' corrupts
		 * ~/.frontier_history.  Deferred to a future milestone. */
	} else {
		/* Single-line path: history append unchanged. */
		/* 2026-06-09 JES #691 Phase C.0.1: append to history ring before dispatch.
		 * Dedup-with-latest is enforced inside boxen_repl_history_append. */
		boxen_repl_history_append(s, s->input_buf);

		/* Echo with "> " prefix */
		char echo_buf[BOXEN_REPL_INPUT_MAX + 4];
		snprintf(echo_buf, sizeof(echo_buf), "> %s", s->input_buf);
		boxen_repl_append_scrollback(s, echo_buf);

		dispatch_buf = s->input_buf;
	}

	bool running = true;

	if (dispatch_buf[0] == '/') {
		/* 2026-06-09 JES Phase C.1 #691: /edit [path] -- kernel intercept.
		 *
		 * Handled here before the UserTalk menubar so it works whether or not
		 * the menubar has an "edit" leaf.  The path argument is stripped of
		 * an optional leading "@".  With no arg, emits a usage hint instead
		 * of reaching the menubar's "Unknown command" path.
		 *
		 * Only active in non-test builds; the test build stubs out
		 * BOXEN_REPL_OMIT_MAIN and never calls boxen_outline_open. */
		const char *slash_buf = dispatch_buf + 1;    /* skip leading '/' */
		while (*slash_buf == ' ') slash_buf++;        /* skip whitespace */
		bool is_edit_cmd = (strncmp(slash_buf, "edit", 4) == 0 &&
		                    (slash_buf[4] == '\0' || slash_buf[4] == ' '));
		if (is_edit_cmd) {
#ifndef BOXEN_REPL_OMIT_MAIN
			const char *path = slash_buf + 4;
			while (*path == ' ') path++;             /* skip whitespace after "edit" */
			if (*path == '@') path++;                /* strip optional "@" prefix */
			if (*path == '\0') {
				boxen_repl_append_scrollback(s, "Usage: /edit <path>");
			} else {
				if (!boxen_outline_open(path)) {
					char err[256];
					snprintf(err, sizeof(err), "Error: could not open outline: %s", path);
					boxen_repl_append_scrollback(s, err);
				}
			}
#else
			boxen_repl_append_scrollback(s, "(edit not available in test build)");
#endif
			/* Consumed -- skip menubar dispatch */
			goto slash_done;
		}

		/* Slash command: route through the hook (or production implementation) */
		if (s->slash_dispatch_hook != NULL) {
			s->slash_dispatch_hook(dispatch_buf, &running);
		}
#ifndef BOXEN_REPL_OMIT_MAIN
		else {
			boxen_repl_real_slash_dispatch(dispatch_buf, &running);
		}
#endif
		slash_done:;
	} else {
		/* Expression evaluation */
		char result_buf[BOXEN_REPL_SCROLLBACK_LINE_MAX];
		char error_buf[BOXEN_REPL_SCROLLBACK_LINE_MAX];
		result_buf[0] = '\0';
		error_buf[0]  = '\0';

		bool ok;
		if (s->repl_eval_hook != NULL) {
			ok = s->repl_eval_hook(dispatch_buf,
			                       result_buf, sizeof(result_buf),
			                       error_buf,  sizeof(error_buf));
		} else {
#ifndef BOXEN_REPL_OMIT_MAIN
			ok = boxen_repl_real_eval(dispatch_buf,
			                          result_buf, sizeof(result_buf),
			                          error_buf,  sizeof(error_buf));
#else
			ok = false;
			snprintf(error_buf, sizeof(error_buf), "(no eval hook in test build)");
#endif
		}

		if (ok) {
			if (result_buf[0] != '\0') {
				boxen_repl_append_scrollback(s, result_buf);
			}
		} else {
			char err_line[BOXEN_REPL_SCROLLBACK_LINE_MAX + 8];
			snprintf(err_line, sizeof(err_line), "Error: %s", error_buf);
			boxen_repl_append_scrollback(s, err_line);
		}
	}

	if (!running) {
		s->should_quit = true;
	}

	/* 2026-06-10 JES #691 Phase C.0.5: reset multi-line accumulator after dispatch */
	s->multiline_buf[0] = '\0';
	s->multiline_len    = 0;
	s->multiline_lines  = 0;

	/* Clear input */
	s->input_buf[0]     = '\0';
	s->input_len        = 0;
	s->input_cursor_pos = 0;

	/* 2026-06-29 JES #803: snap the output-pane view back to bottom on
	 * submit (scroll-on-output).  Async output that arrives via
	 * boxen_repl_append_scrollback does NOT reset the offset, but the
	 * user's own Enter is a deliberate "show me what just happened"
	 * signal -- matches less/man behavior on `>` follow. */
	s->output_scroll_offset = 0;

	if (s->input_win  != NULL) boxen_window_invalidate(s->input_win);
	if (s->output_win != NULL) boxen_window_invalidate(s->output_win);
}

/* -------------------------------------------------------------------------
 * 2026-06-08 JES Phase C.0 #691: on_input (key event handler).
 * ---------------------------------------------------------------------- */
/* 2026-06-29 JES #805: word-boundary helpers for Option-Left/Right cursor
 * jumps.  Word char = ASCII alphanumeric (matching readline's default
 * vi/emacs M-b/M-f semantics on byte-oriented input; non-ASCII multi-byte
 * is out of scope per the existing printable-ASCII restriction in
 * on_input -- when that restriction is lifted to support UTF-8 these
 * helpers will need codepoint-aware semantics).  Bash/readline treats
 * '_' as a non-word char in M-b/M-f mode, so we do too -- the goal is
 * "stops at any non-alphanumeric run" which matches user expectations
 * for camelCase / snake_case identifiers in UserTalk paths.
 *
 * Algorithm follows GNU readline's backward-word/forward-word:
 *
 *   M-b (word_back_from): starting at cursor, skip non-word chars left,
 *     then skip word chars left.  Result: cursor at start of the word
 *     just left of (or under) the original cursor.  Returns 0 if already
 *     at the beginning of the line.
 *
 *   M-f (word_forward_from): starting at cursor, skip non-word chars
 *     right, then skip word chars right.  Result: cursor at the position
 *     just past the end of the word just right of (or under) the
 *     original cursor.  Returns input_len if already at the end.
 *
 * Both helpers are pure -- no I/O, no allocation -- and the caller
 * updates input_cursor_pos with the returned position.  Separated from
 * the keybinding so the unit tests can verify boundary semantics
 * directly without setting up a full event. */
static bool repl_is_word_char(char c) {
	return ((c >= '0' && c <= '9') ||
	        (c >= 'A' && c <= 'Z') ||
	        (c >= 'a' && c <= 'z'));
}

static int word_back_from(const char *buf, int len, int pos) {
	if (pos > len) pos = len;
	if (pos <= 0) return 0;
	int i = pos;
	/* Skip non-word chars left. */
	while (i > 0 && !repl_is_word_char(buf[i - 1])) i--;
	/* Skip word chars left. */
	while (i > 0 && repl_is_word_char(buf[i - 1])) i--;
	return i;
}

static int word_forward_from(const char *buf, int len, int pos) {
	if (pos < 0) pos = 0;
	if (pos >= len) return len;
	int i = pos;
	/* Skip non-word chars right. */
	while (i < len && !repl_is_word_char(buf[i])) i++;
	/* Skip word chars right. */
	while (i < len && repl_is_word_char(buf[i])) i++;
	return i;
}

/* 2026-06-29 JES #805: scroll the output pane by `lines` rows.
 *
 * Positive `lines` scrolls up (older content into view); negative scrolls
 * down (back toward the newest line).  Clamps at both ends -- never
 * negative, never past (scrollback_count - 1) which keeps at least one
 * line visible.  Mirrors the clamp logic in the PgUp/PgDn handlers so
 * the wheel and PgUp/PgDn always agree on bounds.
 *
 * Called from on_input's mouse-wheel branch.  Centralized so both wheel
 * directions share one clamp implementation. */
static void output_scroll_by_lines(boxen_repl_state_t *s, int lines) {
	if (s == NULL || lines == 0) return;
	int next = s->output_scroll_offset + lines;
	if (next < 0) next = 0;
	int max_offset = s->scrollback_count - 1;
	if (max_offset < 0) max_offset = 0;
	if (next > max_offset) next = max_offset;
	s->output_scroll_offset = next;
	if (s->output_win != NULL) boxen_window_invalidate(s->output_win);
}

/* 2026-06-29 JES #805: lines per wheel tick.  Three matches the common
 * convention across less, vim, and macOS Terminal.app's native
 * scrollback (which uses a 3-line wheel step for vertical scrolling). */
#define BOXEN_REPL_WHEEL_LINES_PER_TICK 3

static void on_input(boxen_window_t *win, const boxen_event_t *ev,
                     void *user_data) {
	(void)win;
	boxen_repl_state_t *s = (boxen_repl_state_t *)user_data;
	if (s == NULL || ev == NULL) return;

	/* 2026-06-29 JES #805: mouse-wheel scrolls the output pane.
	 *
	 * Wheel events arrive as BOXEN_EV_MOUSE with button==4 (up) or
	 * button==5 (down).  Routing: boxen_dispatch_event sends mouse
	 * events to the topmost window at the pointer location.  We wire
	 * on_input on BOTH output_win and input_win so wheel-anywhere-in-
	 * the-REPL works (the typist may have the pointer over the input
	 * bar while scrolling output -- a common ergonomic case).
	 *
	 * Wheel up reveals older content -> increment offset.
	 * Wheel down returns toward the newest -> decrement offset.
	 * Same clamp semantics as PgUp/PgDn via output_scroll_by_lines.
	 *
	 * Mouse clicks and drags are deliberately not handled here -- the
	 * REPL only needs the wheel binding; mouse selection / cursor
	 * positioning can be added in a future change. */
	if (ev->type == BOXEN_EV_MOUSE) {
		if (ev->mouse.button == 4 && ev->mouse.pressed) {
			output_scroll_by_lines(s, +BOXEN_REPL_WHEEL_LINES_PER_TICK);
		} else if (ev->mouse.button == 5 && ev->mouse.pressed) {
			output_scroll_by_lines(s, -BOXEN_REPL_WHEEL_LINES_PER_TICK);
		}
		return;
	}

	if (ev->type != BOXEN_EV_KEY) return;

	/* 2026-06-17 JES #691 Phase C.0.7d: Ctrl-C handler.
	 *
	 * Order of operations (all three steps execute in sequence):
	 *
	 *   1. Close any open completion popup unconditionally.  Ctrl-C must
	 *      always dismiss the popup regardless of whether multi-line mode is
	 *      active -- without this, exiting via Ctrl-C while a popup was open
	 *      would leave the popup window "open" through teardown.
	 *
	 *   2. If multi-line mode is active (multiline_lines > 0), discard the
	 *      accumulated buffer and return WITHOUT setting should_quit.  The
	 *      user pressed Ctrl-C to abort the current multi-line expression,
	 *      not to exit the REPL.
	 *
	 *   3. Otherwise (single-line mode, no popup): set should_quit and exit.
	 *
	 * This ordering also ensures the popup-mode key-routing branch below never
	 * sees Ctrl-C -- it is consumed here first.
	 *
	 * 2026-06-17 JES #691 Phase C.0.7a: also cancel any pending slash debounce
	 * so a deferred palette doesn't pop after the REPL has been told to abort. */
	if (ev->key.key == BOXEN_KEY_CTRL_C) {
		s->slash_pending_until_ms = 0;
		/* Step 1: always close popup on Ctrl-C */
		if (s->completion_popup != NULL) {
			boxen_completion_popup_close(s->completion_popup);
			s->completion_popup = NULL;
		}
		/* Step 2: discard multi-line accumulator; return without exiting */
		if (s->multiline_lines > 0) {
			s->multiline_buf[0] = '\0';
			s->multiline_len    = 0;
			s->multiline_lines  = 0;
			s->input_buf[0]     = '\0';
			s->input_len        = 0;
			s->input_cursor_pos = 0;
			if (s->input_win != NULL) boxen_window_invalidate(s->input_win);
			return;
		}
		/* Step 3: single-line mode -- exit the REPL */
		s->should_quit = true;
		return;
	}

	/* 2026-06-09 JES #691 Phase C.0.2: popup-mode key routing.
	 *
	 * When a completion popup is open, intercept navigation/accept/dismiss keys
	 * before the normal handlers see them.  Any key not handled here closes the
	 * popup and falls through to normal processing.
	 *
	 * Key contract (D5 from REPL_SCHISM_EXECUTION_PLAN.md C.0.2):
	 *   TAB / DOWN  -> navigate +1 (wrap)
	 *   UP          -> navigate -1 (wrap)
	 *   ENTER / CTRL_M -> accept selection, replace input_buf, close popup
	 *   ESCAPE      -> dismiss popup, input_buf unchanged
	 *   Any other   -> close popup, fall through to normal handler
	 */
	if (s->completion_popup != NULL) {
		if (ev->key.key == BOXEN_KEY_TAB || ev->key.key == BOXEN_KEY_DOWN) {
			boxen_completion_popup_navigate(s->completion_popup, +1);
			return;
		}
		if (ev->key.key == BOXEN_KEY_UP) {
			boxen_completion_popup_navigate(s->completion_popup, -1);
			return;
		}
		if (ev->key.key == BOXEN_KEY_ENTER || ev->key.key == BOXEN_KEY_CTRL_M) {
			const char *sel = boxen_completion_popup_selected(s->completion_popup);
			if (sel != NULL) {
				snprintf(s->input_buf, sizeof(s->input_buf), "%s", sel);
				s->input_len        = (int)strlen(s->input_buf);
				s->input_cursor_pos = s->input_len;
			}
			boxen_completion_popup_close(s->completion_popup);
			s->completion_popup = NULL;
			if (s->input_win != NULL) boxen_window_invalidate(s->input_win);
			return;
		}
		if (ev->key.key == BOXEN_KEY_ESCAPE) {
			boxen_completion_popup_close(s->completion_popup);
			s->completion_popup = NULL;
			if (s->input_win != NULL) boxen_window_invalidate(s->input_win);
			return;
		}
		/* 2026-06-17 JES #691 Phase C.0.7b: any other key closes popup.
		 *
		 * For printable ASCII: insert the character directly and return.
		 * We must NOT fall through to the printable-ASCII handler below
		 * because that handler contains the '/' palette-open gate
		 * (input_cursor == 0 check).  If the popup was open with an empty
		 * input bar (cursor == 0) and the user types '/' (or any char that
		 * happens to match the palette gate condition), falling through
		 * would incorrectly open the palette modal -- in production this
		 * triggers assert(s_ctx.modal_win == NULL) if a prior palette
		 * session left the window live, crashing the TUI.
		 *
		 * For non-printable keys that are not explicitly handled above
		 * (e.g. function keys, modifier-only events), fall through so the
		 * normal non-popup handlers (Escape, Enter, Backspace, Tab, arrows)
		 * below can process them.
		 *
		 * The previous code called boxen_window_focus(s->input_win) here,
		 * which was wrong: the popup uses boxen_window_raise (z-order only,
		 * no set_modal), so the input window ALWAYS retains focus.  Use
		 * boxen_window_invalidate instead to redraw the now-uncovered area. */
		boxen_completion_popup_close(s->completion_popup);
		s->completion_popup = NULL;
		if (ev->key.key == BOXEN_KEY_NONE &&
		    ev->key.ch >= 0x20 && ev->key.ch < 0x7F) {
			/* Printable char: insert it directly, skip the palette gate. */
			if (s->history_nav_idx != -1) {
				s->history_nav_idx        = -1;
				s->history_saved_input[0] = '\0';
			}
			if (s->input_len < BOXEN_REPL_INPUT_MAX - 1) {
				s->input_buf[s->input_len]     = (char)ev->key.ch;
				s->input_buf[s->input_len + 1] = '\0';
				s->input_len++;
				s->input_cursor_pos = s->input_len;
			}
			if (s->input_win != NULL) boxen_window_invalidate(s->input_win);
			return;
		}
		/* Non-printable: fall through to the normal handlers below. */
		if (s->input_win != NULL) boxen_window_invalidate(s->input_win);
		/* fall through */
	}

	/* Escape: clear input line (also reset history nav state and cancel pending
	 * slash debounce -- 2026-06-17 JES #691 Phase C.0.7a). */
	if (ev->key.key == BOXEN_KEY_ESCAPE) {
		s->input_buf[0]           = '\0';
		s->input_len              = 0;
		s->input_cursor_pos       = 0;
		s->history_nav_idx        = -1;
		s->history_saved_input[0] = '\0';
		s->slash_pending_until_ms = 0;
		if (s->input_win != NULL) boxen_window_invalidate(s->input_win);
		return;
	}

	/* Enter or Ctrl-M: submit.
	 * 2026-06-17 JES #691 Phase C.0.7a P1: cancel pending slash-debounce so
	 * a deferred palette doesn't pop 350ms after the user pressed Enter. */
	if (ev->key.key == BOXEN_KEY_ENTER || ev->key.key == BOXEN_KEY_CTRL_M) {
		s->slash_pending_until_ms = 0;
		submit_input(s);
		return;
	}

	/* 2026-06-09 JES #691 Phase C.0.1: history navigation.
	 * 2026-06-17 JES #691 Phase C.0.7a P1: cancel pending slash-debounce so
	 * a deferred palette doesn't pop while the user is browsing history. */
	if (ev->key.key == BOXEN_KEY_UP) {
		s->slash_pending_until_ms = 0;
		history_nav_up(s);
		return;
	}
	if (ev->key.key == BOXEN_KEY_DOWN) {
		s->slash_pending_until_ms = 0;
		history_nav_down(s);
		return;
	}

	/* 2026-06-29 JES #803: output-pane scrollback (PgUp/PgDn).
	 *
	 * The boxen REPL owns the alternate screen buffer, so terminal-native
	 * scrollback can't reach lines that scroll off the top of the output
	 * pane.  Without these handlers, large output (e.g. system.verbs.builtins,
	 * a wide table dump) was unreachable once it scrolled past the visible
	 * region; users had to fall back to --plain (linenoise) for that.
	 *
	 * Page step is (output_h - 1) so one line of context overlaps between
	 * pages -- standard less/man behavior.  Clamping to >= 1 protects
	 * tiny terminals.  Final clamping against scrollback_count happens
	 * in draw_output_pane (which writes the clamped value back).
	 *
	 * Cancel pending slash-debounce here for the same reason as the other
	 * non-character keys above (history, tab): a deferred palette open
	 * mid-scroll would be jarring.
	 *
	 * Mouse wheel is intentionally NOT wired in this change (issue #805
	 * tracks remapping the existing wheel binding). */
	if (ev->key.key == BOXEN_KEY_PGUP) {
		s->slash_pending_until_ms = 0;
		int page = 1;
		if (s->output_win != NULL) {
			page = boxen_window_content_height(s->output_win) - 1;
			if (page < 1) page = 1;
		}
		s->output_scroll_offset += page;
		/* Upper-bound clamp: keep at least one line visible.  The same
		 * clamp runs in draw_output_pane as defensive belt-and-suspenders,
		 * but writing it back here keeps the field's value in-bounds so
		 * test assertions and successive PgUp presses behave predictably
		 * (without it, the offset would grow unbounded above the ring). */
		int max_offset = s->scrollback_count - 1;
		if (max_offset < 0) max_offset = 0;
		if (s->output_scroll_offset > max_offset) {
			s->output_scroll_offset = max_offset;
		}
		if (s->output_win != NULL) boxen_window_invalidate(s->output_win);
		return;
	}
	if (ev->key.key == BOXEN_KEY_PGDN) {
		s->slash_pending_until_ms = 0;
		int page = 1;
		if (s->output_win != NULL) {
			page = boxen_window_content_height(s->output_win) - 1;
			if (page < 1) page = 1;
		}
		s->output_scroll_offset -= page;
		if (s->output_scroll_offset < 0) s->output_scroll_offset = 0;
		if (s->output_win != NULL) boxen_window_invalidate(s->output_win);
		return;
	}

	/* 2026-06-09 JES #691 Phase C.0.2: Tab completion.
	 *
	 * Call the completion hook (if set).  Zero candidates: silent no-op.
	 * One candidate: replace input_buf inline.
	 * Two or more: open a completion popup above the input bar.
	 *
	 * 2026-06-17 JES #691 Phase C.0.7a P1: cancel pending slash-debounce so
	 * a deferred palette doesn't pop while completion is in flight. */
	if (ev->key.key == BOXEN_KEY_TAB) {
		s->slash_pending_until_ms = 0;
		if (s->completion_hook == NULL) return;

		char candidates[BOXEN_COMPLETION_MAX_CANDIDATES][BOXEN_COMPLETION_CANDIDATE_MAX];
		int count = s->completion_hook(s->input_buf,
		                               (size_t)s->input_len,
		                               candidates,
		                               BOXEN_COMPLETION_MAX_CANDIDATES);
		if (count <= 0) return;  /* silent no-op */

		if (count == 1) {
			/* Single candidate: inline replace. */
			snprintf(s->input_buf, sizeof(s->input_buf), "%s", candidates[0]);
			s->input_len        = (int)strlen(s->input_buf);
			s->input_cursor_pos = s->input_len;
			if (s->input_win != NULL) boxen_window_invalidate(s->input_win);
			return;
		}

		/* Multiple candidates: open popup above the input bar. */
		int sw = 0, sh = 0;
		boxen_get_screen_size(&sw, &sh);

		const char *ptrs[BOXEN_COMPLETION_MAX_CANDIDATES];
		for (int i = 0; i < count; i++) ptrs[i] = candidates[i];

		s->completion_popup = boxen_completion_popup_open(
			ptrs, count,
			0,      /* anchor_x */
			sh - 2, /* anchor_y: the input bar row */
			sw, sh);
		return;
	}

	/* 2026-06-17 JES Phase C.0.7e #691: LEFT arrow -- move caret left one char.
	 *
	 * 2026-06-29 JES #805: Option-Left (Alt modifier) jumps one word back.
	 * iTerm2 with "Left/Right option as Esc+" sends ESC[1;3D for
	 * Option-Left, which termbox2 decodes as BOXEN_KEY_LEFT with
	 * mod & BOXEN_MOD_ALT.  macOS Terminal.app sends ESC b instead
	 * (handled below in the printable-ASCII Alt branch).  Both paths
	 * delegate to word_back_from for unified semantics. */
	if (ev->key.key == BOXEN_KEY_LEFT) {
		if (ev->key.mod & BOXEN_MOD_ALT) {
			s->input_cursor_pos = word_back_from(s->input_buf,
			                                     s->input_len,
			                                     s->input_cursor_pos);
		} else if (s->input_cursor_pos > 0) {
			s->input_cursor_pos--;
		}
		/* Underflow guard: already at col 0, nothing to do */
		if (s->input_win != NULL) boxen_window_invalidate(s->input_win);
		return;
	}

	/* 2026-06-17 JES Phase C.0.7e #691: RIGHT arrow -- move caret right one char.
	 *
	 * 2026-06-29 JES #805: Option-Right (Alt modifier) jumps one word
	 * forward (CSI sequence sibling of Option-Left above). */
	if (ev->key.key == BOXEN_KEY_RIGHT) {
		if (ev->key.mod & BOXEN_MOD_ALT) {
			s->input_cursor_pos = word_forward_from(s->input_buf,
			                                        s->input_len,
			                                        s->input_cursor_pos);
		} else if (s->input_cursor_pos < s->input_len) {
			s->input_cursor_pos++;
		}
		/* Overflow guard: already at end, nothing to do */
		if (s->input_win != NULL) boxen_window_invalidate(s->input_win);
		return;
	}

	/* 2026-06-17 JES Phase C.0.7e #691: Ctrl-A -- jump caret to beginning of line. */
	if (ev->key.key == BOXEN_KEY_CTRL_A) {
		s->input_cursor_pos = 0;
		if (s->input_win != NULL) boxen_window_invalidate(s->input_win);
		return;
	}

	/* 2026-06-17 JES Phase C.0.7e #691: Ctrl-E -- jump caret to end of line. */
	if (ev->key.key == BOXEN_KEY_CTRL_E) {
		s->input_cursor_pos = s->input_len;
		if (s->input_win != NULL) boxen_window_invalidate(s->input_win);
		return;
	}

	/* 2026-06-17 JES Phase C.0.7e #691: Delete (forward-delete) -- remove char
	 * at caret position, shift rest left, caret stays. */
	if (ev->key.key == BOXEN_KEY_DELETE) {
		if (s->input_cursor_pos < s->input_len) {
			/* If user edits during history nav, abandon navigation. */
			if (s->history_nav_idx != -1) {
				s->history_nav_idx        = -1;
				s->history_saved_input[0] = '\0';
			}
			/* Shift bytes left by one starting at cursor_pos */
			memmove(s->input_buf + s->input_cursor_pos,
			        s->input_buf + s->input_cursor_pos + 1,
			        (size_t)(s->input_len - s->input_cursor_pos));
			s->input_len--;
			s->input_buf[s->input_len] = '\0';
		}
		/* No-op guard when cursor is at end */
		if (s->input_win != NULL) boxen_window_invalidate(s->input_win);
		return;
	}

	/* Backspace: delete char BEFORE caret.
	 *
	 * 2026-06-17 JES Phase C.0.7e #691: when caret is at end
	 * (input_cursor_pos == input_len), this is the original append-only
	 * backspace: decrement both len and pos, null-terminate.  When caret
	 * is mid-buffer, shift the tail left by one and decrement len; caret
	 * position also decrements (it now points to the same logical position).
	 *
	 * 2026-06-17 JES #691 Phase C.0.7a: if a slash debounce is pending,
	 * Backspace cancels it (user changed their mind).  input_len is 0 during
	 * the debounce window, so the delete below is a no-op, leaving input_buf
	 * empty -- exactly what the user expects after Backspace on '/'. */
	if (ev->key.key == BOXEN_KEY_BACKSPACE) {
		s->slash_pending_until_ms = 0;  /* cancel pending open, if any */
		/* If user edits during history nav, abandon navigation. */
		if (s->history_nav_idx != -1) {
			s->history_nav_idx        = -1;
			s->history_saved_input[0] = '\0';
		}
		if (s->input_cursor_pos > 0) {
			/* Shift bytes left by one to fill the gap at cursor_pos - 1 */
			memmove(s->input_buf + s->input_cursor_pos - 1,
			        s->input_buf + s->input_cursor_pos,
			        (size_t)(s->input_len - s->input_cursor_pos));
			s->input_cursor_pos--;
			s->input_len--;
			s->input_buf[s->input_len] = '\0';
		}
		/* Underflow guard: caret already at 0, nothing to do */
		if (s->input_win != NULL) boxen_window_invalidate(s->input_win);
		return;
	}

	/* 2026-06-29 JES #805: Alt-modified printable keys (the ESC-prefix
	 * variant of Option-key combos).
	 *
	 * macOS Terminal.app sends ESC b for Option-Left and ESC f for
	 * Option-Right by default.  termbox2 decodes the ESC prefix into the
	 * ALT modifier, so these arrive as BOXEN_KEY_NONE with ch=='b'/'f'
	 * and mod & BOXEN_MOD_ALT.  Before #805 these fell through to the
	 * printable-ASCII branch below and inserted literal 'b'/'f' into the
	 * input buffer -- the muscle-memory regression in the issue.
	 *
	 * Decode the same readline bindings users get from bash:
	 *   M-b -> word back
	 *   M-f -> word forward
	 *
	 * Any other Alt-modified printable is swallowed silently rather than
	 * inserting the literal character (also readline convention -- an
	 * unbound M-x produces a bell, not the literal 'x').  We choose
	 * silent over bell because boxen has no terminal bell facility and
	 * the alternative (literal insert) is the bug we're fixing. */
	if (ev->key.key == BOXEN_KEY_NONE &&
	    (ev->key.mod & BOXEN_MOD_ALT) &&
	    ev->key.ch >= 0x20 && ev->key.ch < 0x7F) {
		if (ev->key.ch == 'b' || ev->key.ch == 'B') {
			s->input_cursor_pos = word_back_from(s->input_buf,
			                                     s->input_len,
			                                     s->input_cursor_pos);
			if (s->input_win != NULL) boxen_window_invalidate(s->input_win);
		} else if (ev->key.ch == 'f' || ev->key.ch == 'F') {
			s->input_cursor_pos = word_forward_from(s->input_buf,
			                                        s->input_len,
			                                        s->input_cursor_pos);
			if (s->input_win != NULL) boxen_window_invalidate(s->input_win);
		}
		/* Other Alt-printable: silently swallow (do NOT insert the
		 * literal char -- that is exactly the bug #805 reports). */
		return;
	}

	/* Printable ASCII only (0x20..0x7E).
	 * 2026-06-08 JES #691 Phase C.0 round 2 P2-14: the range 0x20..0x7E
	 * is intentional.  Non-ASCII code points (0x7F and above) and control
	 * characters (< 0x20) are dropped here.  Non-ASCII input would require
	 * multi-byte handling and a Unicode-aware input cursor; that is deferred
	 * to a future milestone. */
	if (ev->key.key == BOXEN_KEY_NONE && ev->key.ch >= 0x20 && ev->key.ch < 0x7F) {
		/* 2026-06-17 JES #691 Phase C.0.7a: slash-palette debounce.
		 *
		 * BEFORE the early-'/'-gate: if a debounce is pending and a printable
		 * char arrived, cancel the pending open and insert '/' first.  The char
		 * arriving here is the SECOND character the user typed (e.g. 'h' in
		 * "/help"), so the REPL inserts both '/' and the char into input_buf.
		 *
		 * This path fires when:
		 *   slash_pending_until_ms != 0   -- a '/' was recently typed
		 *   any printable ch              -- user continued typing (not a timeout)
		 *
		 * On cancel, '/' is inserted manually then we fall through to insert ch. */
		if (s->slash_pending_until_ms != 0) {
			/* Cancel the pending palette open. */
			s->slash_pending_until_ms = 0;
			/* Insert the deferred '/' into input_buf first. */
			if (s->input_len < BOXEN_REPL_INPUT_MAX - 1) {
				s->input_buf[s->input_len]     = '/';
				s->input_buf[s->input_len + 1] = '\0';
				s->input_len++;
				s->input_cursor_pos = s->input_len;
			}
			/* Fall through: the current char (e.g. 'h') inserts below. */
		}

		/* 2026-06-10 JES #691 Phase C.0.3b: '/' at empty input starts debounce.
		 *
		 * Gates:
		 *   ch == '/'           -- only the slash key triggers the menu
		 *   input_len == 0      -- only at the start of an empty line; mid-line
		 *                         '/' (e.g. a path component) inserts normally
		 *   palette_open_hook   -- NULL in test builds / when hook not installed
		 *   palette_state       -- guard against re-entry (no double-open)
		 *
		 * 2026-06-17 JES #691 Phase C.0.7a: changed from immediate open to a
		 * 350ms debounce.  The palette actually opens in boxen_repl_check_pending_slash
		 * (called by the production event loop on timeout and post-event paths).
		 * This matches the linenoise REPL's behavior (repl.c:4004) which polls
		 * stdin for slash_menu_trigger_delay_ms() before opening the palette.
		 *
		 * Contrast with the global key handler (boxen_set_global_key_handler):
		 * '/' only matters when the REPL input line is empty and focused.
		 * A global handler would intercept '/' in editor windows too, which
		 * is wrong.  The printable-ASCII branch here fires only when on_input
		 * is receiving keys (i.e. the REPL input bar is the dispatch target),
		 * which is exactly the right scope. */
		if (ev->key.ch == '/' &&
		    s->input_len == 0 &&
		    s->palette_open_hook != NULL &&
		    s->palette_state == NULL) {
			/* Start the debounce timer.  The palette fires when
			 * boxen_repl_check_pending_slash sees the deadline pass. */
			s->slash_pending_until_ms = repl_now_ms(s) + BOXEN_REPL_SLASH_DEBOUNCE_MS;
			return;
		}

		/* If user types during history nav, abandon navigation. */
		if (s->history_nav_idx != -1) {
			s->history_nav_idx        = -1;
			s->history_saved_input[0] = '\0';
		}

		/* 2026-06-17 JES Phase C.0.7e #691: insert at caret position.
		 *
		 * When caret is at end (cursor_pos == input_len) this is identical to
		 * the original append: write at [cursor_pos], NUL-terminate at [+1].
		 * When caret is mid-buffer, shift tail right by one byte first. */
		if (s->input_len < BOXEN_REPL_INPUT_MAX - 1) {
			/* Shift bytes after caret right by one to make room */
			memmove(s->input_buf + s->input_cursor_pos + 1,
			        s->input_buf + s->input_cursor_pos,
			        (size_t)(s->input_len - s->input_cursor_pos));
			s->input_buf[s->input_cursor_pos] = (char)ev->key.ch;
			s->input_len++;
			s->input_cursor_pos++;
			s->input_buf[s->input_len] = '\0';
		}
		if (s->input_win != NULL) boxen_window_invalidate(s->input_win);
		return;
	}
}

/* -------------------------------------------------------------------------
 * 2026-06-08 JES Phase C.0 #691: public internal API
 * ---------------------------------------------------------------------- */

void boxen_repl_state_init(boxen_repl_state_t *s, int tw, int th) {
	memset(s, 0, sizeof(boxen_repl_state_t));

	/* 2026-06-09 JES #691 Phase C.0.1: memset zeros history_nav_idx, but the
	 * sentinel meaning "not navigating" is -1, not 0.  Set it explicitly here
	 * so first-run users (no ~/.frontier_history file, so history_load is a
	 * no-op and history_append is never called) don't have nav_idx stuck at 0.
	 * Without this, pressing DOWN before typing anything silently wiped input. */
	s->history_nav_idx = -1;

	/* Production hooks (only set when not in test build).
	 * BOXEN_REPL_OMIT_MAIN guards the production-only symbols (repl_eval_script,
	 * dispatch_slash_command) from the test binary. */
#ifndef BOXEN_REPL_OMIT_MAIN
	s->slash_dispatch_hook = boxen_repl_real_slash_dispatch;
	s->repl_eval_hook      = boxen_repl_real_eval;
	/* 2026-06-09 JES #691 Phase C.0.2: wire completion hook to production impl. */
	s->completion_hook     = boxen_repl_real_completion;
	/* 2026-06-10 JES #691 Phase C.0.3b: wire palette hooks to production impls. */
	s->palette_open_hook     = boxen_repl_real_palette_open;
	s->palette_dispatch_hook = boxen_repl_real_palette_dispatch;
#endif

	/* Stdout capture fields: -1 means not active (same sentinel as open(2)
	 * returns on failure, so teardown can guard with fd >= 0). */
	s->saved_stdout    = -1;
	s->saved_stderr    = -1;
	s->pipe_read_fd    = -1;
	s->partial_line[0] = '\0';
	s->partial_line_len = 0;

	repl_build_layout(s, tw, th);
	repl_wire_callbacks(s);

	/* 2026-06-09 JES #691 Phase C.0.1: hydrate history ring from disk. */
	boxen_repl_history_load(s);
}

void boxen_repl_state_teardown(boxen_repl_state_t *s) {
	/* 2026-06-10 JES #691 Phase C.0.3b: defensively close the palette modal if
	 * it is still open when teardown runs.  Normal exit paths close it via the
	 * done_cb -> on_palette_done flow; this guard handles crash/abort paths.
	 * palette_close and free are gated on #ifndef BOXEN_REPL_OMIT_MAIN because
	 * palette.c is not linked in test builds -- the field exists in test builds
	 * (holding an opaque sentinel pointer) but teardown there sets it to NULL
	 * directly without dereferencing.
	 *
	 * Forward declaration: palette_close() is declared in palette.h but that
	 * header is only #included inside the big #ifndef BOXEN_REPL_OMIT_MAIN block
	 * further down in this file.  Rather than reorganising all includes, a local
	 * forward declaration here is sufficient (palette_state_t is already
	 * forward-declared in boxen_repl_internal.h). */
#ifndef BOXEN_REPL_OMIT_MAIN
	extern void palette_close(palette_state_t *st);
	/* repl_palette_source_dispose forward-declared at file scope above so the
	 * test build (OMIT_MAIN) compiles without pulling in palette.h. */
	if (s->palette_state != NULL) {
		palette_close(s->palette_state);
		free(s->palette_state);
		s->palette_state = NULL;
	}
	/* Free the paired source on crash/abort teardown so leaks are not
	 * introduced when on_palette_done never runs. */
	if (s->palette_source != NULL) {
		repl_palette_source_dispose(
			(struct palette_menu_source *)s->palette_source);
		free(s->palette_source);
		s->palette_source = NULL;
	}
#else
	s->palette_state = NULL;
	s->palette_source = NULL;
#endif
	/* 2026-06-09 JES #691 Phase C.0.2: close any open completion popup before
	 * the windows it overlaps are destroyed.  Normal exit paths close it via
	 * on_input key handlers; this guard handles crash/abort paths. */
	if (s->completion_popup != NULL) {
		boxen_completion_popup_close(s->completion_popup);
		s->completion_popup = NULL;
	}
	if (s->footer_win != NULL) { boxen_window_close(s->footer_win); s->footer_win = NULL; }
	if (s->input_win  != NULL) { boxen_window_close(s->input_win);  s->input_win  = NULL; }
	if (s->output_win != NULL) { boxen_window_close(s->output_win); s->output_win = NULL; }

	for (int i = 0; i < BOXEN_REPL_SCROLLBACK_SIZE; i++) {
		free(s->scrollback[i]);
		s->scrollback[i] = NULL;
	}
	s->scrollback_head  = 0;
	s->scrollback_count = 0;
	s->input_buf[0]        = '\0';
	s->input_len           = 0;
	s->input_cursor_pos    = 0;
	s->should_quit         = false;
	s->slash_dispatch_hook = NULL;
	s->repl_eval_hook      = NULL;

	/* Close the capture pipe read end if it is still open.
	 * Production teardown (boxen_repl_main) closes it before calling here;
	 * this guard is a safety net for error paths. */
	if (s->pipe_read_fd >= 0) {
		close(s->pipe_read_fd);
		s->pipe_read_fd = -1;
	}
	s->saved_stdout     = -1;
	s->saved_stderr     = -1;
	s->partial_line[0]  = '\0';
	s->partial_line_len = 0;

	/* 2026-06-08 JES #691 Phase C.0 round 3 P0: free heap-allocated launch transport.
	 * boxen_repl_main must call debug_kill_all_threads + debug_join_all_threads
	 * BEFORE calling boxen_repl_state_teardown, ensuring no thread holds a
	 * reference to this pointer when it is freed here. */
	if (s->launch_transport != NULL) {
		free(s->launch_transport);
		s->launch_transport = NULL;
	}
}

int boxen_repl_run_one_tick(boxen_repl_state_t *s, const boxen_event_t *ev) {
	/* Resize: rebuild layout and re-wire callbacks */
	if (ev->type == BOXEN_EV_RESIZE) {
		int nw = (ev->resize.w > 0) ? ev->resize.w : 80;
		int nh = (ev->resize.h > 0) ? ev->resize.h : 24;
		repl_build_layout(s, nw, nh);
		repl_wire_callbacks(s);
		/* 2026-06-17 JES #691 Phase C.0.7a: check pending slash on resize too
		 * (unlikely to fire, but keeps the check uniform). */
		boxen_repl_check_pending_slash(s);
		return REPL_CONTINUE;
	}

	boxen_dispatch_event(ev);

	/* 2026-06-17 JES #691 Phase C.0.7a: check slash debounce after every event.
	 * Fires the palette if the user typed '/' then waited long enough without
	 * typing a follow-up char.  Also fires when a non-printable event (e.g. UP
	 * arrow, Ctrl-key) arrives after the debounce window. */
	boxen_repl_check_pending_slash(s);

	return s->should_quit ? REPL_QUIT : REPL_CONTINUE;
}

/* -------------------------------------------------------------------------
 * 2026-06-08 JES #691 Phase C.0 round 2: drain_stdout_into_scrollback.
 *
 * Reads bytes from fd (must be non-blocking) until EAGAIN/EOF, splits on
 * newlines, and appends each complete line to the scrollback ring.
 * Incomplete lines are held in s->partial_line until the next call.
 *
 * No production runtime dependencies -- compiled in both production and
 * test builds.  Tests call it directly with a test-supplied pipe fd.
 * ---------------------------------------------------------------------- */
/* 2026-06-23 JES #691 Phase C.0.7g: drain thunk used by the boxen_ui
 * bridge to keep the scrollback ring current while a dialog modal is
 * waiting for input.  Reads ctx as the boxen_repl_state_t* passed in
 * boxen_ui_host.drain_ctx; the bridge does not know that type. */
void boxen_repl_drain_capture_pipe_thunk(void *ctx) {
	boxen_repl_state_t *s = (boxen_repl_state_t *)ctx;
	if (s == NULL) return;
	drain_stdout_into_scrollback(s, s->pipe_read_fd);
}

/* 2026-06-23 JES #691 Phase C.0.7g: focus-restore thunk for the boxen_ui
 * bridge.  After a dialog modal closes the bridge calls this so the
 * REPL input bar reclaims keyboard focus.  No-op if input_win has been
 * torn down. */
void boxen_repl_restore_focus_thunk(void *ctx) {
	boxen_repl_state_t *s = (boxen_repl_state_t *)ctx;
	if (s == NULL || s->input_win == NULL) return;
	boxen_window_focus(s->input_win);
}

/* 2026-06-23 JES #691 Phase C.0.7g Phase 2A: ring-bell thunk for the
 * boxen_ui bridge.  Writes \a (BEL, 0x07) directly to the saved-stderr
 * fd captured before the dup2 redirect, so the byte reaches the real
 * terminal instead of being absorbed by the capture pipe + rendered as
 * a visible ^G glyph in the scrollback.  No-op if the saved fd is
 * absent (capture pipe disabled) -- writing to the captured stderr
 * would be visibly wrong, and the user can survive a missed bell. */
void boxen_repl_ring_bell_thunk(void *ctx) {
	boxen_repl_state_t *s = (boxen_repl_state_t *)ctx;
	if (s == NULL || s->saved_stderr < 0) return;
	const char bel = '\a';
	ssize_t w = write(s->saved_stderr, &bel, 1);
	(void)w; /* best-effort; nothing useful to do on partial / failed write */
}

void drain_stdout_into_scrollback(boxen_repl_state_t *s, int fd) {
	if (s == NULL || fd < 0) return;

	char buf[512];
	ssize_t n;

	while ((n = read(fd, buf, sizeof(buf))) > 0) {
		const char *p   = buf;
		const char *end = buf + n;

		while (p < end) {
			const char *nl = NULL;
			/* Find next newline in this chunk */
			for (const char *q = p; q < end; q++) {
				if (*q == '\n') { nl = q; break; }
			}

			if (nl != NULL) {
				/* We have a complete line: partial_line (if any) + this segment */
				size_t seg_len = (size_t)(nl - p);
				size_t avail   = sizeof(s->partial_line) - s->partial_line_len - 1;

				/* 2026-06-08 JES #691 Phase C.0 round 3 P2-5: truncation warning.
				 * When a single line exceeds the partial buffer, append a visible
				 * marker so the user knows content was dropped. */
				if (seg_len > avail) {
					static const char trunc_marker[] = "...[truncated]";
					size_t marker_len = sizeof(trunc_marker) - 1;
					/* Ensure we have room for the marker at the end of avail */
					if (avail > marker_len) {
						memcpy(s->partial_line + s->partial_line_len, p, avail - marker_len);
						s->partial_line_len += avail - marker_len;
					}
					/* Stamp the marker; avail - marker_len may be 0 if buffer was
					 * already full -- in that case we overwrite the last marker_len
					 * bytes of whatever is already accumulated. */
					size_t stamp_pos = (s->partial_line_len > marker_len)
					                   ? (s->partial_line_len - marker_len)
					                   : 0;
					memcpy(s->partial_line + stamp_pos, trunc_marker, marker_len);
					s->partial_line_len = stamp_pos + marker_len;
					seg_len = avail; /* mark as consumed (clamp applied) */
				} else {
					memcpy(s->partial_line + s->partial_line_len, p, seg_len);
					s->partial_line_len += seg_len;
				}
				s->partial_line[s->partial_line_len] = '\0';

				/* Append to scrollback (even if empty -- echoes a blank line) */
				boxen_repl_append_scrollback(s, s->partial_line);

				/* Reset partial buffer */
				s->partial_line[0]  = '\0';
				s->partial_line_len = 0;

				p = nl + 1; /* advance past the newline */
			} else {
				/* No newline in remaining chunk -- accumulate into partial_line */
				size_t seg_len = (size_t)(end - p);
				size_t avail   = sizeof(s->partial_line) - s->partial_line_len - 1;
				if (seg_len > avail) seg_len = avail;

				memcpy(s->partial_line + s->partial_line_len, p, seg_len);
				s->partial_line_len += seg_len;
				s->partial_line[s->partial_line_len] = '\0';

				p = end; /* consumed all of this chunk */
			}
		}
	}
	/* n == 0 (EOF) or n < 0 with errno == EAGAIN/EWOULDBLOCK: drain complete */
}

/* -------------------------------------------------------------------------
 * 2026-06-08 JES Phase C.0 #691: production-only section.
 *
 * headless_threading.h and debug_handler.h are included ONLY inside this
 * #ifndef guard so the test binary never references frontier_gil /
 * hthreadglobals at the dyld level (same isolation as DEBUGGER_TUI_OMIT_MAIN).
 * ---------------------------------------------------------------------- */

#ifndef BOXEN_REPL_OMIT_MAIN

#include "debug_handler.h"
#include "headless_threading.h"
#include "repl_eval.h"
#include "repl.h"                        /* repl_install_verb_host, repl_uninstall_verb_host, repl_reset_exit_flag */
#include "repl_completion.h"             /* repl_complete_slash_command_path, repl_slash_commands_list */
#include "../Common/headers/strings.h"   /* copyptocstring */
#include <pthread.h>
/* 2026-06-10 JES #691 Phase C.0.3b: palette production headers.
 * Included inside #ifndef BOXEN_REPL_OMIT_MAIN to keep test builds free of
 * palette.c / ODB dependencies. */
#include "palette.h"
#include "repl_palette_source.h"
#include "palette_arg_inject.h"
#include "boxen_palette_backend.h"
#include "../Common/headers/menudata_headless.h"  /* meuserselected_headless */

/* repl_slash_dispatch.h declares the de-static'd dispatch_slash_command
 * from repl.c.  Only boxen_repl.c consumers include this header.
 *
 * 2026-06-08 JES #691 Phase C.0 round 2 P2-18: note on g_repl_exit_requested
 * side effect -- dispatch_slash_command may set g_repl_exit_requested (via the
 * replverbhost_exit kernel-verb host adapter) when the /exit command is
 * processed.  The boxen REPL drives exit from the return value (*running set
 * false -> s->should_quit = true), not by polling g_repl_exit_requested
 * directly. */
#include "repl_slash_dispatch.h"

/* -------------------------------------------------------------------------
 * 2026-06-08 JES Phase C.0 #691: production hook implementations.
 * ---------------------------------------------------------------------- */

bool boxen_repl_real_slash_dispatch(const char *line, bool *running) {
	/* 2026-06-08 JES #691 Phase C.0 round 2 P1-5: avoid strict aliasing violation.
	 * dispatch_slash_command takes (boolean *) which may have different storage
	 * size/alignment than C99 bool.  Use explicit local variables and cast. */
	boolean br = (boolean)*running;
	boolean keep_running = dispatch_slash_command(line, &br);
	*running = (bool)br;
	return (bool)keep_running;
}

bool boxen_repl_real_eval(const char *expr,
                          char *result_out, size_t result_cap,
                          char *error_out,  size_t error_cap) {
	bigstring result_bs;
	bigstring error_bs;
	result_bs[0] = '\0';
	error_bs[0]  = '\0';

	boolean ok = repl_eval_script(expr, result_bs, error_bs);

	if (ok) {
		/* copyptocstring converts Pascal length-prefixed string to C string.
		 * Uses a 256-byte tmp buffer (matches bigstring/lenbigstring+1 max). */
		char tmp[256];
		copyptocstring(result_bs, tmp);
		size_t n = strlen(tmp);
		if (n >= result_cap) n = result_cap - 1;
		memcpy(result_out, tmp, n);
		result_out[n] = '\0';
	} else {
		char tmp[256];
		copyptocstring(error_bs, tmp);
		size_t n = strlen(tmp);
		if (n >= error_cap) n = error_cap - 1;
		memcpy(error_out, tmp, n);
		error_out[n] = '\0';
	}

	return (bool)ok;
}

/* -------------------------------------------------------------------------
 * 2026-06-09 JES #691 Phase C.0.2: boxen_repl_real_completion.
 *
 * Production completion hook for the boxen REPL.  Mirrors the dispatch
 * logic from repl.c::linenoise_completion_callback (lines 2072+) but fills
 * a candidates array instead of calling linenoiseAddCompletion.
 *
 * For slash-command lines the hook does:
 *   - "/jump <path>" or "/list <path>" -> delegate to repl_complete_slash_command_path
 *   - "/<prefix>" -> prefix-match against repl_slash_commands_list
 *   - "/" alone -> enumerate all slash commands
 * For non-slash lines -> delegate to repl_complete_slash_command_path (ODB paths).
 *
 * 2026-06-09 JES #691 Phase C.0.2: scope note.
 *   C.0.2 implements slash command + ODB path completion only.  The legacy
 *   linenoise REPL (repl.c) provides richer non-slash completion: keyword
 *   expansion, @-address context, verb-call context, roottable + systemtable
 *   enumeration.  That richer path is deliberately out of scope for C.0.2 --
 *   see planning/phase_c/REPL_SCHISM_EXECUTION_PLAN.md Section 2.
 *   Follow-up: richer non-slash completion to be filed as a future enhancement.
 *
 * Returns the number of candidates written (0 = no completions available).
 * ---------------------------------------------------------------------- */

/* Collector context for the add-callback bridge below. */
typedef struct {
	char (*cands)[BOXEN_COMPLETION_CANDIDATE_MAX];
	int   count;
	int   max;
} bc_coll_t;

static void bc_add(void *add_ctx, const char *s) {
	bc_coll_t *c = (bc_coll_t *)add_ctx;
	if (c->count >= c->max) return;
	strncpy(c->cands[c->count], s, BOXEN_COMPLETION_CANDIDATE_MAX - 1);
	c->cands[c->count][BOXEN_COMPLETION_CANDIDATE_MAX - 1] = '\0';
	c->count++;
}

int boxen_repl_real_completion(const char *buf, size_t buf_len,
                               char candidates[][BOXEN_COMPLETION_CANDIDATE_MAX],
                               int max_candidates)
{
	if (buf == NULL || candidates == NULL || max_candidates <= 0) return 0;

	bc_coll_t coll;
	coll.cands = candidates;
	coll.count = 0;
	coll.max   = max_candidates;

	if (buf_len > 0 && buf[0] == '/') {
		/* "/jump <path>" or "/list <path>" -- complete the path argument. */
		if (buf_len >= 6) {
			if (strncasecmp(buf, "/jump ", 6) == 0) {
				repl_complete_slash_command_path(buf, buf_len, 6, bc_add, &coll);
				return coll.count;
			}
			if (strncasecmp(buf, "/list ", 6) == 0) {
				repl_complete_slash_command_path(buf, buf_len, 6, bc_add, &coll);
				return coll.count;
			}
		}

		/* Regular slash-command prefix completion: "/" + optional prefix. */
		const char *cmd_prefix = buf + 1;
		size_t prefix_len = buf_len - 1;

		for (int i = 0; repl_slash_commands_list[i] != NULL && coll.count < max_candidates; i++) {
			const char *cmd = repl_slash_commands_list[i];
			if (strncasecmp(cmd, cmd_prefix, prefix_len) == 0) {
				char completion[BOXEN_COMPLETION_CANDIDATE_MAX];
				snprintf(completion, sizeof(completion), "/%s ", cmd);
				bc_add(&coll, completion);
			}
		}
		return coll.count;
	}

	/* Non-slash: ODB path completion. */
	repl_complete_slash_command_path(buf, buf_len, 0, bc_add, &coll);
	return coll.count;
}

/* -------------------------------------------------------------------------
 * 2026-06-08 JES #691 Phase C.0 round 2: no-op transport write_line stub.
 *
 * The launch transport used by debug_launch_from_options below needs a
 * write_line callback.  In C.0 the boxen REPL does not yet render debug
 * notifications (suspended/completed) -- those are dropped here.  C.1 will
 * wire in a real callback that queues notifications into the scrollback ring.
 * ---------------------------------------------------------------------- */
static void boxen_repl_noop_write_line(void *ctx, const char *line, size_t len) {
	(void)ctx; (void)line; (void)len;
	/* C.0: debug notifications are intentionally discarded until C.1 wires
	 * the notification -> scrollback path. */
}

/* -------------------------------------------------------------------------
 * 2026-06-08 JES #691 Phase C.0 round 2: boxen_repl_main -- public entry point.
 *
 * GIL yield/restore mirrors debugger_tui_main (debugger_tui.c:2905-2933).
 * Teardown mirrors debugger_tui_main (debugger_tui.c:2960-2975).
 *
 * Round 2 fixes applied (issue #691 /gate P0/P1):
 *   P2-12: NULL check on opts.
 *   P1-8:  reset g_repl_exit_requested before installing verb host.
 *   P1-3:  install/uninstall repl verb host adapter.
 *   P0-2:  stdout capture pipe -- all slash dispatch / eval output is routed
 *           into the scrollback ring instead of written raw to the terminal.
 *   P0-1:  debug_launch_from_options replaces broken /debug slash synthesis.
 *   P1-6:  debug_set_attach_transport(NULL) in teardown.
 * ---------------------------------------------------------------------- */
int boxen_repl_main(const cli_options_t *opts) {
	/* 2026-06-08 JES #691 Phase C.0 rounds 2+3 P2-12: guard against NULL opts. */
	if (opts == NULL) {
		log_error(LOG_COMP_GENERAL, "boxen_repl: opts is NULL");
		return 1;
	}

	boxen_repl_state_t *state = calloc(1, sizeof(boxen_repl_state_t));
	if (state == NULL) {
		log_error(LOG_COMP_GENERAL, "boxen_repl: out of memory allocating state");
		return 1;
	}

	/* 2026-06-08 JES Phase C.0 #691: boxen_init before querying terminal size.
	 * Same rationale as debugger_tui_main: tb_width/tb_height return
	 * TB_ERR_NOT_INIT (negative) before tb_init(). */
	const boxen_backend_t *be = boxen_tb2_backend();
	bool boxen_initialized = false;
	boxen_result_t rc = boxen_init(be, NULL, NULL);
	if (rc != BOXEN_OK) {
		log_error(LOG_COMP_GENERAL, "boxen_repl: boxen_init failed: %s",
		          boxen_last_error_str());
		free(state);
		return 1;
	}
	boxen_initialized = true;

	/* 2026-06-29 JES #805: enable xterm mouse tracking so wheel events
	 * arrive as BOXEN_EV_MOUSE (button==4/5) and route to the output-pane
	 * scroll-offset handler -- rather than being translated by the host
	 * terminal to up/down arrow keys and routed to history navigation
	 * (the muscle-memory regression #805 addresses).  Only enabled on
	 * the production REPL surface, not the unit-test mock backend; mock
	 * backend's set_mouse_enabled is a no-op so this call is safe in any
	 * build. */
	boxen_set_mouse_enabled(true);

	int tw = (be->width  && be->width()  > 0) ? be->width()  : 80;
	int th = (be->height && be->height() > 0) ? be->height() : 24;

	/* 2026-06-08 JES #691 Phase C.0 round 2 P1-8 / round 3 P1: clear stale exit
	 * flag from any prior REPL session before installing the verb host adapter. */
	repl_reset_exit_flag();

	/* 2026-06-08 JES #691 Phase C.0 round 2 P1-3: install repl.* verb host.
	 * Mirrors protocol_handler.c:149 and repl.c:repl_main exactly. */
	repl_install_verb_host();

	/* 2026-06-08 JES #691 Phase C.0 round 3 P1: publish g_repl_active so that
	 * repl.isActive() returns true during this REPL session. */
	repl_set_active(true);

	boxen_repl_state_init(state, tw, th);

	/* 2026-06-08 JES #691 Phase C.0 round 2 P0-2: stdout capture setup.
	 *
	 * Redirect stdout and stderr to a pipe so that slash dispatch / eval output
	 * (which calls fputs/printf to stdout) is captured and routed into the
	 * scrollback ring rather than written raw to the terminal in raw mode
	 * (which would corrupt the boxen display).
	 *
	 * Pipe write end stays BLOCKING (see spec): the producer (slash dispatch)
	 * runs synchronously between boxen_poll_event calls, so the drain happens
	 * immediately after.  Long output (> 64 KB) could stall the producer, but
	 * that is acceptable for C.0.  A future fix can add a separate drain thread
	 * if needed.
	 *
	 * On any pipe setup failure, we fall through without capture (display will
	 * be corrupted by raw stdout writes, but the REPL is still functional).
	 *
	 * 2026-06-10 JES #691 Phase C.0.4 follow-up:
	 *   - Post-dispatch drain added (after boxen_repl_run_one_tick): slash
	 *     output now reaches the scrollback before the next boxen_present().
	 *   - The 100ms boxen_poll_event timeout means drain runs at >= 10 Hz even
	 *     with zero input.  A full pipe stalls the producer at most one drain
	 *     period (100ms); no hang is possible because the read end is O_NONBLOCK
	 *     and drain_stdout_into_scrollback always empties the pipe before returning.
	 *   - drain never touches input_buf, input_len, or input_cursor_pos -- async output during
	 *     in-progress typing is safe.  See test_stdout_drain_during_typing_does_not_corrupt_input. */
	int pipefd[2];
	bool capture_active = false;
	if (pipe(pipefd) == 0) {
		/* Read end non-blocking so drain never blocks the event loop.
		 * 2026-06-08 JES #691 Phase C.0 round 3 P2-7: check fcntl return.
		 * If F_SETFL fails, drain would block -- fall through without capture. */
		if (fcntl(pipefd[0], F_SETFL, O_NONBLOCK) < 0) {
			log_warn(LOG_COMP_GENERAL,
			         "boxen_repl: fcntl(O_NONBLOCK) failed; skipping capture");
			close(pipefd[0]);
			close(pipefd[1]);
		} else {
			int saved_out = dup(STDOUT_FILENO);
			int saved_err = dup(STDERR_FILENO);

			if (saved_out >= 0 && saved_err >= 0) {
				/* 2026-06-08 JES #691 Phase C.0 round 3 P2-6: check dup2 returns.
				 * If either dup2 fails, restore and skip capture so the display is
				 * not left in a half-redirected state. */
				int r1 = dup2(pipefd[1], STDOUT_FILENO);
				int r2 = dup2(pipefd[1], STDERR_FILENO);
				if (r1 < 0 || r2 < 0) {
					/* Restore whichever dup2 succeeded */
					dup2(saved_out, STDOUT_FILENO);
					dup2(saved_err, STDERR_FILENO);
					close(saved_out);
					close(saved_err);
					close(pipefd[0]);
					close(pipefd[1]);
					log_warn(LOG_COMP_GENERAL,
					         "boxen_repl: dup2 failed; skipping capture");
				} else {
					close(pipefd[1]);  /* write end is now duplicated into stdout/stderr */

					/* 2026-06-29 JES #804: force stdout/stderr to line-buffered
					 * after pipe redirect.  By default libc switches a FILE* to
					 * block-buffered when its underlying fd is not a tty (which
					 * the pipe is not), so any code that writes a newline-
					 * terminated diagnostic via fputs/printf without an explicit
					 * fflush(stdout) sits in the libc buffer and never reaches
					 * drain_stdout_into_scrollback.  That swallowed the visible
					 * error for `/list builtins` (and any other slash-command
					 * error path that omitted fflush -- a whole class of
					 * silent-failure bugs).  Restoring line-buffering matches
					 * the linenoise/--plain semantics where stdout is a tty and
					 * line-buffered by default, so error messages flush on the
					 * trailing '\n' the same way they always have.
					 *
					 * setvbuf may fail (e.g. allocator pressure); the comment
					 * notes the failure mode but we still want capture to come
					 * up.  A silent revert to block-buffering is no worse than
					 * the prior behavior, so we don't gate capture on success. */
					(void)setvbuf(stdout, NULL, _IOLBF, 0);
					(void)setvbuf(stderr, NULL, _IOLBF, 0);

					state->saved_stdout = saved_out;
					state->saved_stderr = saved_err;
					state->pipe_read_fd = pipefd[0];
					capture_active = true;
				}
			} else {
				/* dup failed -- clean up without capturing */
				if (saved_out >= 0) close(saved_out);
				if (saved_err >= 0) close(saved_err);
				close(pipefd[0]);
				close(pipefd[1]);
				log_warn(LOG_COMP_GENERAL, "boxen_repl: stdout dup failed; display may corrupt");
			}
		}
	} else {
		log_warn(LOG_COMP_GENERAL, "boxen_repl: pipe() failed; display may corrupt");
	}

	/* 2026-06-23 JES #691 Phase C.0.7g: register the boxen UI bridge so
	 * dialog.* verbs invoked from dispatched scripts route through a
	 * boxen modal instead of raw terminal IO.  The bridge calls back
	 * into drain_stdout_into_scrollback during its mini event loop so
	 * the scrollback ring stays current while the modal waits.  The
	 * host struct lives on this function's stack; it stays valid until
	 * the matching boxen_ui_set_active(NULL) just before
	 * boxen_shutdown() below. */
	boxen_ui_host_t ui_host = {
		.capture_pipe_fd    = capture_active ? state->pipe_read_fd : -1,
		.drain_capture_pipe = boxen_repl_drain_capture_pipe_thunk,
		.drain_ctx          = state,
		.restore_focus      = boxen_repl_restore_focus_thunk,
		.restore_focus_ctx  = state,
		.ring_bell          = boxen_repl_ring_bell_thunk,
		.ring_bell_ctx      = state,
	};
	boxen_ui_set_active(&ui_host);

	/* 2026-06-08 JES #691 Phase C.0 round 2 P0-1: startup-script launch.
	 *
	 * Replaces the broken "/debug <path>" slash synthesis (which had no
	 * registered /debug command in the REPL menubar).
	 * debug_launch_from_options handles: bare ODB @path, freeform inline
	 * expression, and script file paths.  It calls op_dispatch -> handle_debug_run
	 * which spawns the debug thread suspended.
	 *
	 * GIL is held throughout this auto-launch; debug_launch_from_options requires
	 * the GIL for langcompiletext and headless_spawn_script_thread.
	 *
	 * 2026-06-08 JES #691 Phase C.0 round 3 P0: heap-allocate the transport so
	 * it outlives boxen_repl_main's stack frame.  The joinable debug/run thread
	 * (fldetached=false) caches a pointer to this transport via
	 * debug_register_thread; that pointer must remain valid until
	 * debug_join_all_threads() returns.  See debug_handler.h:196-214. */
	transport_t *launch_transport = calloc(1, sizeof(transport_t));
	if (launch_transport == NULL) {
		log_error(LOG_COMP_GENERAL,
		          "boxen_repl_main: failed to allocate launch_transport; skipping auto-launch");
		/* Continue into the REPL without auto-launch; not fatal. */
	} else {
		launch_transport->write_line = boxen_repl_noop_write_line;
		launch_transport->ctx = NULL;
		state->launch_transport = launch_transport;

		/* Register the lazy-attach transport BEFORE spawning the debug thread.
		 * Mirrors debugger_tui_main:2882 and protocol_handler.c:194. */
		debug_set_attach_transport(launch_transport);

		if (opts->script_file != NULL || opts->inline_script != NULL) {
			debug_launch_from_options(opts, launch_transport);
		}
	}

	log_info(LOG_COMP_GENERAL, "Boxen REPL: entering event loop (Ctrl-C to quit)");

	/* 2026-06-08 JES Phase C.0 #691: event loop.
	 * GIL yield pattern mirrors debugger_tui_main exactly:
	 *   snapshot -> release GIL -> poll -> reacquire -> restore -> dispatch. */
	while (!state->should_quit) {
		boxen_event_t ev;
		memset(&ev, 0, sizeof(ev));

		hdlthreadglobals main_globals = hthreadglobals;
		headless_save_threadglobals(main_globals);
		pthread_mutex_unlock(&frontier_gil);
		boxen_result_t poll_rc = boxen_poll_event(&ev, 100 /* ms timeout */);
		pthread_mutex_lock(&frontier_gil);
		headless_restore_threadglobals(main_globals);

		/* 2026-06-08 JES #691 Phase C.0 round 2 P0-2: drain captured stdout
		 * after every poll (event or timeout) so slash dispatch output reaches
		 * the scrollback ring promptly. */
		if (capture_active) {
			drain_stdout_into_scrollback(state, state->pipe_read_fd);
		}

		/* 2026-06-08 JES #691 Phase C.0 round 3 P1: poll repl.exit() flag.
		 * replverbhost_exit() sets g_repl_exit_requested (via repl.exit() from
		 * non-slash UserTalk code).  The slash path already sets should_quit via
		 * submit_input's running=false path; this catches the direct-call path. */
		if (repl_is_exit_requested()) {
			state->should_quit = true;
		}

		if (poll_rc == BOXEN_ERR_TIMEOUT) {
			/* 2026-06-17 JES #691 Phase C.0.7a: on timeout, check whether the
			 * slash debounce window has expired.  This is the primary path by
			 * which the palette opens after the user types '/' and waits: the
			 * 100ms poll timeout fires several times before the 350ms debounce
			 * elapses, and check_pending_slash fires the open on the first tick
			 * after the deadline. */
			boxen_repl_check_pending_slash(state);
			boxen_present();
			continue;
		}
		if (poll_rc != BOXEN_OK) {
			log_warn(LOG_COMP_GENERAL, "boxen_repl: poll error, exiting");
			break;
		}

		boxen_repl_run_one_tick(state, &ev);

		/* 2026-06-10 JES #691 Phase C.0.4: post-dispatch drain.
		 * Slash dispatch / eval may have written to stdout; drain promptly so
		 * output reaches the scrollback before the next boxen_present(). */
		if (capture_active) {
			drain_stdout_into_scrollback(state, state->pipe_read_fd);
		}

		boxen_present();
	}

	/* 2026-06-08 JES #691 Phase C.0 round 3: teardown sequence (revised).
	 *
	 * Round 2 teardown drained lazy threads but did NOT kill/join the joinable
	 * debug/run thread spawned by debug_launch_from_options.  That thread
	 * (fldetached=false) caches &launch_transport in its debug state record;
	 * if it runs after boxen_repl_main returns (or after the transport is freed)
	 * it dereferences a dangling pointer.
	 *
	 * 2026-06-08 JES #691 Phase C.0 round 3 P1 (concurrency note): clear the
	 * attach transport BEFORE kill+join, not after.  Mirrors protocol_handler.c
	 * which NULLs the global transport pointer first so the lazy-attach fast
	 * path gates on NULL during the kill/join window.  Without this reorder,
	 * a callScript thread that lazy-attaches between the drain (step 2) and
	 * the eventual NULL (was step 6) could register against launch_transport
	 * AFTER drain returned 0, then be freed beneath itself by the state
	 * teardown.  Inert in C.0 (no breakpoints set), real concern for C.1+.
	 *
	 * Corrected order (mirrors main.c:1286-1298 for kill+join):
	 *
	 *   1. Snapshot main hglobals before the drain/kill/join cycle.
	 *   2. debug_wait_lazy_threads_drained() -- drain detached lazy threads.
	 *   3. headless_restore_threadglobals(main_hglobals) -- restore main context.
	 *   4. debug_set_attach_transport(NULL) -- prevent NEW lazy registrations
	 *      during the kill/join window.  Mirrors protocol_handler.c:422.
	 *   5. debug_kill_all_threads() -- signal joinable threads to stop.
	 *   6. Release GIL, debug_join_all_threads(), reacquire GIL, restore hglobals.
	 *      (mirrors main.c:1291-1298 exactly)
	 *   7. Restore stdout/stderr; final drain of capture pipe.
	 *   8. repl_set_active(false) -- P1: publish end of REPL session.
	 *   9. repl_uninstall_verb_host() -- P1-3.
	 *  10. boxen_repl_state_teardown() -- frees launch_transport (P0) + windows.
	 *  11. boxen_shutdown(), free(state).
	 */
	{
		/* Step 1+2+3: drain lazy (detached) threads, restore main hglobals. */
		hdlthreadglobals main_hglobals = hthreadglobals;
		headless_save_threadglobals(main_hglobals);
		debug_wait_lazy_threads_drained();
		headless_restore_threadglobals(main_hglobals);

		/* Step 4: clear attach transport BEFORE kill+join so no new lazy thread
		 * can register against launch_transport during the kill/join window. */
		debug_set_attach_transport(NULL);

		/* Step 5: signal joinable debug threads to stop. */
		debug_kill_all_threads();

		/* Step 6: release GIL so killed threads can finish cleanup, then join.
		 * Mirrors main.c:1291-1298 verbatim. */
		{
			hdlthreadglobals saved = hthreadglobals;
			headless_save_threadglobals(saved);
			pthread_mutex_unlock(&frontier_gil);
			debug_join_all_threads();
			pthread_mutex_lock(&frontier_gil);
			headless_restore_threadglobals(saved);
		}
	}

	/* Step 7: restore stdout/stderr; final drain before display clears. */
	if (capture_active) {
		drain_stdout_into_scrollback(state, state->pipe_read_fd);
		dup2(state->saved_stdout, STDOUT_FILENO);
		dup2(state->saved_stderr, STDERR_FILENO);
		close(state->saved_stdout); state->saved_stdout = -1;
		close(state->saved_stderr); state->saved_stderr = -1;
		close(state->pipe_read_fd); state->pipe_read_fd = -1;
	}

	/* Step 8: mark REPL session inactive (P1). */
	repl_set_active(false);

	/* 2026-06-09 JES #691 Phase C.0.1: persist history before tearing down state.
	 * Placed after repl_set_active(false) (Step 8) and before
	 * repl_uninstall_verb_host() (Step 9) to match the plan's save contract:
	 * save runs on clean exit, NOT in state_teardown (tests would silently write). */
	boxen_repl_history_save(state);

	/* Step 9: uninstall verb host before teardown (P1-3). */
	repl_uninstall_verb_host();

	/* Step 9.5: close all outline editor windows before boxen shutdown.
	 * 2026-06-09 JES Phase C.1 #691: outline windows hold boxen_window_t
	 * handles; close them before boxen_shutdown frees the substrate. */
	boxen_outline_close_all();

	/* 2026-06-23 JES #691 Phase C.0.7g: deregister the boxen UI bridge
	 * BEFORE boxen_repl_state_teardown so the drain_ctx (which points
	 * at state) can no longer be reached from a stray bridge call
	 * after state's pipe_read_fd is closed.  Also ahead of
	 * boxen_shutdown since bridge entry points touch boxen primitives.
	 * ui_host (above) is about to leave scope when boxen_repl_main
	 * returns; null the global so a stray late call cannot dereference
	 * it. */
	boxen_ui_set_active(NULL);

	/* Step 10: teardown windows, ring, and launch_transport (P0 free). */
	boxen_repl_state_teardown(state);

	if (boxen_initialized) {
		boxen_shutdown();
	}

	/* Step 11: free state. */
	free(state);
	state = NULL;

	log_info(LOG_COMP_GENERAL, "Boxen REPL: exited cleanly");
	return 0;
}

/* -------------------------------------------------------------------------
 * 2026-06-10 JES #691 Phase C.0.3b: on_palette_done callback.
 *
 * Called by boxen_palette_backend's modal input_fn when the palette state
 * machine reaches DONE_EXECUTE or DONE_CANCEL.
 *
 * Responsibilities:
 *   1. Close the palette (backend->close destroys the modal window).
 *   2. Free palette_state heap allocation; NULL the pointer.
 *   3. Refocus the REPL input window.
 *   4. On DONE_EXECUTE: dispatch via palette_dispatch_hook if wired.
 * ---------------------------------------------------------------------- */
static void on_palette_done(void *repl_state_opaque, palette_done_t done,
                            void *exec_script, const char *exec_arg) {
	boxen_repl_state_t *s = (boxen_repl_state_t *)repl_state_opaque;
	if (s == NULL) return;

	/* 1+2. Close and free palette.
	 * Call paint_teardown before close to match the linenoise contract
	 * (repl.c:3394-3397).  For the boxen backend paint_teardown is a no-op
	 * (boxen windows vanish atomically on close), but the protocol order
	 * must be teardown-then-close for backend portability. */
	if (s->palette_state != NULL) {
		palette_paint_teardown(s->palette_state);
		palette_close(s->palette_state);
		free(s->palette_state);
		s->palette_state = NULL;
	}

	/* 3. Refocus the REPL input window. */
	if (s->input_win != NULL) {
		boxen_window_focus(s->input_win);
	}

	/* 4. Dispatch on execute.
	 *
	 * 2026-06-22 JES #691 C.0.7f follow-up: dispatch BEFORE disposing the
	 * source.  exec_script is a Handle owned by the source's per-item script
	 * cache (cache_script_handle in repl_palette_source.c).  Disposing the
	 * source first would disposehandle() every cached entry including this
	 * one -- yielding a use-after-free that surfaced as "(menu script
	 * failed)" on every dispatch.  The dispatch hook performs its own
	 * copyhandle (meuserselected_headless) so by the time control returns
	 * here it is safe to release the cache. */
	if (done == PALETTE_DONE_EXECUTE && exec_script != NULL &&
	    s->palette_dispatch_hook != NULL) {
		s->palette_dispatch_hook(exec_script, exec_arg ? exec_arg : "");
	}

	/* 5. Dispose the source after dispatch -- the cache that owns
	 * exec_script must outlive step 4. */
	if (s->palette_source != NULL) {
		repl_palette_source_dispose(
			(palette_menu_source_t *)s->palette_source);
		free(s->palette_source);
		s->palette_source = NULL;
	}
}

/* -------------------------------------------------------------------------
 * 2026-06-10 JES #691 Phase C.0.3b: boxen_repl_real_palette_open.
 *
 * Called from on_input when '/' is typed at empty input.  Opens the
 * slash-menu palette as a boxen modal window.
 *
 * GIL: must be held (repl_palette_source_init_all calls ODB).
 * ---------------------------------------------------------------------- */
bool boxen_repl_real_palette_open(void *s_opaque) {
	boxen_repl_state_t *s = (boxen_repl_state_t *)s_opaque;
	/* Allocate palette state on the heap; palette_open_ex memsets it. */
	palette_state_t *pst = (palette_state_t *)calloc(1, sizeof(palette_state_t));
	if (pst == NULL) {
		log_warn(LOG_COMP_GENERAL, "boxen palette: OOM allocating palette_state");
		return false;
	}

	/* Build the ODB-backed source (all installed menubars).
	 *
	 * 2026-06-21 JES #691 C.0.7f: heap-allocate the source so its address
	 * stays valid for the palette's lifetime.  palette_open_ex stores a
	 * borrowed pointer (st->source) and dereferences it lazily when the user
	 * opens cascade levels via open_level().  A stack-local `src` (the prior
	 * shape) became a dangling pointer the moment this function returned,
	 * so any DOWN / RIGHT-arrow / hotkey activation that triggered
	 * st->source->item_count() crashed with frame#0 PC=0 (NULL fp).
	 * See test_99_repl_menu_crash.py. */
	palette_menu_source_t *src = (palette_menu_source_t *)calloc(1, sizeof(*src));
	if (src == NULL) {
		log_warn(LOG_COMP_GENERAL, "boxen palette: OOM allocating palette_source");
		free(pst);
		return false;
	}
	if (!repl_palette_source_init_all(src)) {
		log_warn(LOG_COMP_GENERAL,
		         "boxen palette: no installed menubars (system.menus.data.*)");
		free(src);
		free(pst);
		return false;
	}

	/* Query screen size for the palette geometry. */
	int sw = 80, sh = 24;
	boxen_get_screen_size(&sw, &sh);

	/* Register the done-callback before calling palette_open_ex so the
	 * backend ctx captures it before the modal goes interactive. */
	palette_render_boxen_backend_set_done_cb(on_palette_done, s);

	/* Prompt row: in boxen mode the palette's pane-row anchoring is irrelevant
	 * (the boxen backend ignores prompt_row), but palette_open_ex validates
	 * the row and we must pass something sane.  Use 0 (top of screen). */
	if (!palette_open_ex(pst, sh, sw, 0, src,
	                     palette_render_boxen_backend())) {
		log_warn(LOG_COMP_GENERAL,
		         "boxen palette: palette_open_ex failed (terminal too small?)");
		repl_palette_source_dispose(src);
		free(src);
		/* 2026-06-10 JES #691 Phase C.0.3b: clear stale registration so the
		 * done_cb/repl_state set above don't outlive the failed open. */
		palette_render_boxen_backend_set_done_cb(NULL, NULL);
		free(pst);
		return false;
	}

	/* 2026-06-21 JES #691 C.0.7f: source lifetime is now tied to palette_state.
	 * palette.c stores a borrowed st->source pointer and dereferences it
	 * lazily when the user opens cascade levels (open_level -> item_count /
	 * item_describe).  Keep `src` alive and pair it with palette_state; the
	 * dispose+free fires on every close path (on_palette_done and the
	 * teardown crash/abort guard). */
	s->palette_state = pst;
	s->palette_source = src;

	/* Initial render. */
	palette_render_state(pst);
	boxen_present();

	return true;
}

/* -------------------------------------------------------------------------
 * 2026-06-10 JES #691 Phase C.0.3b: boxen_repl_real_palette_dispatch.
 *
 * Dispatches the chosen menu leaf script.  Mirrors the dispatch path in
 * repl.c (lines 4100-4155) with the same arg-injection logic.
 *
 * script_handle: a Handle (copied into adapter-private storage by
 *   repl_palette_source's cache; caller does NOT own it post-dispatch).
 *   meuserselected_headless does its own internal copyhandle, so we do NOT
 *   need to copyhandle here before passing it.  The adapter's cache will
 *   be freed by on_palette_done -> palette_close -> source_dispose path.
 *
 * exec_arg: NUL-terminated, may be empty.
 *
 * Dispatch-path divergence from linenoise (intentional):
 *
 *   Linenoise (repl.c) sets g_script_running = 1 around each
 *   meuserselected_headless call (repl.c:2352, 4147-4149).  g_script_running
 *   is declared static volatile sig_atomic_t in repl.c (not extern-visible),
 *   so we cannot set it from here even if we wanted to.
 *
 *   The linenoise SIGINT handler (repl.c:1736) checks g_script_running to
 *   decide whether to interrupt a running script vs. clear the input line.
 *   That distinction is meaningful for linenoise because SIGINT arrives as a
 *   raw Unix signal while the terminal is in raw mode.
 *
 *   Boxen has a different signal disposition: boxen owns terminal mode and
 *   delivers Ctrl-C as a BOXEN_KEY_CTRL_C event (a boxen_event_t), not as a
 *   raw SIGINT.  The palette modal intercepts Ctrl-C via boxen_palette_feed_event
 *   before a script is ever dispatched; once dispatch is in progress the
 *   boxen event loop is blocked in meuserselected_headless (GIL-held), so no
 *   further Ctrl-C events arrive through boxen.  The divergence is therefore
 *   correct for the boxen architecture.
 *
 *   Future cleanup: extract the newemptyhandle/sethandlesize/memcpy block to a
 *   shared TU (e.g. repl_handle_utils.c) rather than inlining it here.  It is
 *   inlined now to avoid linking the linenoise REPL (repl.c /
 *   dispatch_synthesized_script) into boxen builds.
 * ---------------------------------------------------------------------- */
bool boxen_repl_real_palette_dispatch(void *script_handle, const char *exec_arg) {
	Handle hscript = (Handle)script_handle;
	if (hscript == nil) return false;

	boolean ok = false;
	bool synth_used = false;

	if (exec_arg != NULL && exec_arg[0] != '\0') {
		/* Arg-injection path: escape the arg and synthesize a new source. */
		char escaped[PALETTE_ARG_MAX * 2 + 4];
		palette_arg_escape_result_t esc_r =
			palette_arg_escape(exec_arg, escaped, sizeof(escaped));

		if (esc_r == PALETTE_ARG_ESCAPE_OK) {
			long n = gethandlesize(hscript);
			if (n > 0) {
				char *script_cstr = (char *)malloc((size_t)n + 1);
				if (script_cstr != NULL) {
					HLock(hscript);
					memcpy(script_cstr, *hscript, (size_t)n);
					HUnlock(hscript);
					script_cstr[n] = '\0';

					char *synth = NULL;
					size_t synth_len = 0;
					if (palette_arg_inject_into_script(script_cstr, (size_t)n,
					                                   escaped, &synth, &synth_len)) {
						/* Build a handle from the synthesized source and dispatch.
						 * Nested if: disposehandle must run whenever newemptyhandle
						 * succeeds, even if sethandlesize subsequently fails. */
						Handle hsynth = nil;
						if (newemptyhandle(&hsynth)) {
							if (sethandlesize(hsynth, (long)synth_len)) {
								HLock(hsynth);
								memcpy(*hsynth, synth, synth_len);
								HUnlock(hsynth);
								ok = (boolean)meuserselected_headless(hsynth);
							}
							disposehandle(hsynth);
						}
						free(synth);
						synth_used = true;
					}
					free(script_cstr);
				}
			}
		} else {
			/* Forbidden byte or overflow -- skip dispatch, show diagnostic. */
			switch (esc_r) {
			case PALETTE_ARG_ESCAPE_FORBIDDEN_BYTE:
				printf("(palette argument contains a forbidden character)\n");
				break;
			case PALETTE_ARG_ESCAPE_OVERFLOW:
				printf("(palette argument is too long)\n");
				break;
			case PALETTE_ARG_ESCAPE_OK:
				break; /* unreachable */
			}
			fflush(stdout);
			synth_used = true;  /* skip no-arg fallback */
			ok = true;           /* not a script failure */
		}
	}

	if (!synth_used) {
		/* No arg or synthesis failed -- dispatch the leaf script as-is. */
		ok = (boolean)meuserselected_headless(hscript);
	}

	if (!ok) {
		printf("(menu script failed)\n");
		fflush(stdout);
	}

	return (bool)ok;
}

#endif /* !BOXEN_REPL_OMIT_MAIN */

/* -------------------------------------------------------------------------
 * 2026-06-10 JES #691 Phase C.0.3b: test-only palette close helper.
 *
 * Compiled only in BOXEN_REPL_OMIT_MAIN (test) builds.  Simulates the
 * palette close path (NULL palette_state, refocus input_win) without
 * calling palette_close or any production runtime symbols.  Tests use this
 * after asserting palette opened to verify that subsequent keystrokes
 * reach the REPL input window.
 *
 * Note: this replaces the previous (now missing) #endif so the structure
 * is: production code block ends with #endif, then this test-only block
 * is unconditionally visible but gated on #ifdef so the linker only sees
 * it in test builds.
 * ---------------------------------------------------------------------- */
#ifdef BOXEN_REPL_OMIT_MAIN
void boxen_repl_close_palette_for_test(void *s_opaque) {
	boxen_repl_state_t *s = (boxen_repl_state_t *)s_opaque;
	if (s == NULL) return;
	/* Just NULL the pointers -- no real palette_close / source_dispose since
	 * palette.c / repl_palette_source.c are not linked in the test binary. */
	s->palette_state = NULL;
	s->palette_source = NULL;
	/* Refocus input_win if it exists (mock backend creates real windows). */
	if (s->input_win != NULL) {
		boxen_window_focus(s->input_win);
	}
}
#endif
