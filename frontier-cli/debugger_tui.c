/*
 * debugger_tui.c -- boxen-based UserTalk debugger TUI.
 *
 * Milestone B.0: tracer-bullet skeleton.
 * planning/phase_b/EXECUTION_PLAN.md, section "Milestone B.0".
 *
 * What B.0 ships:
 *   - boxen_init + three-window placeholder layout (script, stack, footer)
 *   - Event loop with GIL release/reacquire around boxen_poll_event()
 *   - Stub transport_t (write_line is a no-op; B.1 wires notifications)
 *   - 'q', Escape, Ctrl-C to quit
 *   - BOXEN_EV_RESIZE handling: rebuild layout proportionally on terminal resize
 *   - debugger_tui_state_init / run_one_tick / state_teardown for unit tests
 *
 * GIL discipline:
 *   - Snapshot hthreadglobals before yielding; restore after poll returns.
 *   - Pattern copied verbatim from protocol_handler.c:348-354 and 411-421.
 *   - Per DEBUGGER_TUI_HANDOFF.md and ADR-014.
 *
 * Transport sentinel:
 *   - Heap-allocated to outlive lazy-attached callScript threads.
 *   - On teardown: drain -> NULL-clear -> free (same as protocol_main).
 *   - B.6 wires debug_set_attach_transport; B.0 stub skips it (no debug
 *     runtime wiring yet per EXECUTION_PLAN.md B.0 "Out of scope").
 *
 * 2026-06-06 JES Phase B.0 #691
 * 2026-06-06 JES Phase B.0 #734 round 1: terminal size + resize + dead code + quit-key
 * 2026-06-07 JES Phase B.3 #691: F5/F10/F11/Shift-F11 keybinds + debug_state machine
 * 2026-06-07 JES Phase B.4 #691: F9 breakpoint toggle + condition modal (A.7 cursor)
 * 2026-06-07 JES Phase B.4 #743 round 1: condition preservation + JSON escape + F9 gate
 * 2026-06-07 JES Phase B.5 #691: cmd-double-click identifier resolution + watchpoints
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Frontier contributors
 */

#include "debugger_tui.h"
#include "debugger_tui_internal.h"

#include "boxen/boxen.h"
#include "op_handler.h"

/* headless_threading.h is only needed for debugger_tui_main() (GIL symbols).
 * It is NOT included at file scope to keep the test-visible internal functions
 * (state_init, run_one_tick, state_teardown) free of GIL symbol dependencies.
 * The production-only functions (debugger_tui_main) include it locally. */

#include "../Common/headers/logging.h"

/* 2026-06-07 JES Phase B.1 #691: cJSON for parsing debug/getSource responses.
 * cJSON has no Frontier runtime dependencies; safe in both production and test. */
#include "../third_party/cJSON/cJSON.h"

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* -------------------------------------------------------------------------
 * 2026-06-07 JES Phase B.2 #691: locals state helpers.
 *
 * tui_free_locals  -- release heap arrays of local variable name/value strings.
 * tui_store_locals -- parse a debug/getLocals result object into state.
 * tui_store_stack  -- parse a debug/getStack result object into state.
 * ---------------------------------------------------------------------- */

static void tui_free_locals(tui_state_t *s) {
	if (s->local_names != NULL) {
		for (int i = 0; i < s->local_count; i++) {
			free(s->local_names[i]);
			free(s->local_values[i]);
		}
		free(s->local_names);
		free(s->local_values);
		s->local_names  = NULL;
		s->local_values = NULL;
		s->local_count  = 0;
	}
}

/* Parse a debug/getLocals result JSON object (the "result" sub-object).
 * Actual response shape from debug_handler.c:1900 (code-verified):
 *   result.locals = [{name, value, type}, ...]
 * Updates local_names, local_values, local_count.
 * Locals always reflect the innermost suspended frame -- no per-frame API
 * exists (EXECUTION_PLAN.md B.2 Sentinels).
 *
 * 2026-06-07 JES Phase B.2 #739 round 1: empty-array carries definite info
 * ("locals are now empty") -- distinct from B.1 P1-E's "field absent = preserve"
 * case.  Commit to fresh state before the count check so an empty array
 * clears stale locals rather than leaving them on screen. */
static void tui_store_locals(tui_state_t *s, cJSON *result) {
	cJSON *locals_j = cJSON_GetObjectItemCaseSensitive(result, "locals");
	if (!cJSON_IsArray(locals_j)) return;

	/* Commit to empty state -- an empty locals array carries definite info. */
	tui_free_locals(s);

	int count = cJSON_GetArraySize(locals_j);
	if (count <= 0) return;

	/* Cap to prevent allocation overflow from a malformed response.
	 * UserTalk types coerced to display form can be multi-MB; per-value caps
	 * below (TUI_LOCAL_NAME_MAX / TUI_LOCAL_VALUE_MAX) bound heap per slot.
	 * Silent clamp: log_warn is a Frontier runtime call and crashes in the
	 * test build (log_write resolves to NULL via dynamic lookup).  The B.1
	 * tui_store_source uses the same silent-clamp pattern. */
	if (count > TUI_MAX_LOCALS) {
		count = TUI_MAX_LOCALS;
	}

	/* calloc so unfilled slots are NULL even if strdup fails partway through */
	s->local_names  = (char **)calloc((size_t)count, sizeof(char *));
	s->local_values = (char **)calloc((size_t)count, sizeof(char *));
	if (s->local_names == NULL || s->local_values == NULL) {
		free(s->local_names);
		free(s->local_values);
		s->local_names  = NULL;
		s->local_values = NULL;
		return;
	}

	int i = 0;
	cJSON *entry = NULL;
	cJSON_ArrayForEach(entry, locals_j) {
		if (i >= count) break;

		cJSON *name_j  = cJSON_GetObjectItemCaseSensitive(entry, "name");
		cJSON *value_j = cJSON_GetObjectItemCaseSensitive(entry, "value");

		const char *name  = (cJSON_IsString(name_j)  && name_j->valuestring  != NULL)
		                    ? name_j->valuestring  : "";
		const char *value = (cJSON_IsString(value_j) && value_j->valuestring != NULL)
		                    ? value_j->valuestring : "";

		/* 2026-06-07 JES Phase B.2 #739 round 1 P2: cap name/value before
		 * strdup to prevent memory exhaustion from hostile/malformed responses.
		 * UserTalk types coerced to display form can be multi-MB; this bounds
		 * heap per local to TUI_LOCAL_NAME_MAX + TUI_LOCAL_VALUE_MAX bytes. */
		size_t name_len  = strlen(name);
		size_t value_len = strlen(value);
		if (name_len  > TUI_LOCAL_NAME_MAX)  name_len  = TUI_LOCAL_NAME_MAX;
		if (value_len > TUI_LOCAL_VALUE_MAX) value_len = TUI_LOCAL_VALUE_MAX;

		s->local_names[i]  = (char *)malloc(name_len  + 1);
		s->local_values[i] = (char *)malloc(value_len + 1);
		if (s->local_names[i] == NULL || s->local_values[i] == NULL) {
			/* OOM: truncate to successfully populated slots */
			free(s->local_names[i]);
			free(s->local_values[i]);
			s->local_names[i]  = NULL;
			s->local_values[i] = NULL;
			s->local_count = i;
			return;
		}
		memcpy(s->local_names[i],  name,  name_len);
		s->local_names[i][name_len]   = '\0';
		memcpy(s->local_values[i], value, value_len);
		s->local_values[i][value_len] = '\0';
		i++;
	}
	s->local_count = i;
}

/* Parse a debug/getStack result JSON object (the "result" sub-object).
 * Actual response shape from debug_handler.c:2011 (code-verified):
 *   result.frames = [{level, script, line?}, ...] outermost to innermost.
 * Updates frame_count, frame_scripts, frame_lines.
 * selected_frame is set to the innermost frame on a fresh stack load so
 * the user's cursor lands at the currently executing frame by default. */
/* 2026-06-07 JES Phase B.2 #739 round 1: empty-array carries definite info
 * ("stack is now empty") -- distinct from B.1 P1-E's "field absent = preserve"
 * case.  Commit to empty state before the count check so an empty frames
 * array clears stale frame state rather than leaving it on screen. */
static void tui_store_stack(tui_state_t *s, cJSON *result) {
	cJSON *frames_j = cJSON_GetObjectItemCaseSensitive(result, "frames");
	if (!cJSON_IsArray(frames_j)) return;

	/* Commit to empty state -- an empty frames array carries definite info. */
	s->frame_count    = 0;
	s->selected_frame = 0;

	int count = cJSON_GetArraySize(frames_j);
	if (count <= 0) return;

	/* Cap to prevent stack smash on a malformed response.
	 * Silent clamp: see note in tui_store_locals for why log_warn is omitted. */
	if (count > TUI_MAX_FRAMES) {
		count = TUI_MAX_FRAMES;
	}

	int i = 0;
	cJSON *frame = NULL;
	cJSON_ArrayForEach(frame, frames_j) {
		if (i >= count) break;

		cJSON *script_j = cJSON_GetObjectItemCaseSensitive(frame, "script");
		cJSON *line_j   = cJSON_GetObjectItemCaseSensitive(frame, "line");

		const char *script = (cJSON_IsString(script_j) && script_j->valuestring != NULL)
		                     ? script_j->valuestring : "";
		strncpy(s->frame_scripts[i], script, sizeof(s->frame_scripts[i]) - 1);
		s->frame_scripts[i][sizeof(s->frame_scripts[i]) - 1] = '\0';

		/* line may be absent for the outermost frame before any statement fires */
		s->frame_lines[i] = cJSON_IsNumber(line_j) ? (long)line_j->valuedouble : 0;

		i++;
	}

	s->frame_count    = i;
	/* Default selection: innermost frame (highest index), which is where
	 * execution is suspended.  Outermost is index 0 (matching getStack order). */
	s->selected_frame = (i > 0) ? (i - 1) : 0;
}

/* -------------------------------------------------------------------------
 * 2026-06-07 JES Phase B.1 #691: source state helpers.
 *
 * tui_free_script_lines -- release the heap array of source line strings.
 * tui_store_source     -- parse a debug/getSource result object into state.
 * ---------------------------------------------------------------------- */

static void tui_free_script_lines(tui_state_t *s) {
	if (s->script_lines != NULL) {
		for (int i = 0; i < s->script_line_count; i++) {
			free(s->script_lines[i]);
		}
		free(s->script_lines);
		s->script_lines      = NULL;
		s->script_line_count = 0;
	}
}

/* Parse a debug/getSource result JSON object (the "result" sub-object) into
 * tui_state_t. On success, updates script_path, script_lines,
 * script_line_count, current_line, bp_lines, bp_line_count.
 *
 * 2026-06-07 JES Phase B.1 #737 round 1: P1-B/C/D/E fixes. */
static void tui_store_source(tui_state_t *s, cJSON *result) {
	cJSON *script_j  = cJSON_GetObjectItemCaseSensitive(result, "script");
	cJSON *curline_j = cJSON_GetObjectItemCaseSensitive(result, "currentLine");
	cJSON *lines_j   = cJSON_GetObjectItemCaseSensitive(result, "lines");

	if (!cJSON_IsString(script_j) || script_j->valuestring == NULL) return;
	if (!cJSON_IsArray(lines_j)) return;

	/* P1-E: count and validate BEFORE mutating script_path / current_line.
	 * An empty-lines response would otherwise update the path while leaving
	 * the old script_lines array intact, showing stale source under a new
	 * path. */
	int count = cJSON_GetArraySize(lines_j);
	if (count <= 0) return;

	/* P1-B: cap to TUI_MAX_SOURCE_LINES to prevent malloc(80M+) from a
	 * malicious or oversized debug/getSource response.
	 * Silent clamp: log_warn is a Frontier runtime call that resolves to NULL
	 * via -Wl,-undefined,dynamic_lookup in the test build, causing a crash
	 * when the over-limit path is exercised in tests. */
	if (count > TUI_MAX_SOURCE_LINES) {
		count = TUI_MAX_SOURCE_LINES;
	}

	/* Store script path (safe now that count is validated) */
	strncpy(s->script_path, script_j->valuestring, sizeof(s->script_path) - 1);
	s->script_path[sizeof(s->script_path) - 1] = '\0';

	/* P1-D: only update current_line if the field is present.  An absent
	 * currentLine field means the runtime did not supply a new position --
	 * preserve whatever was set by the preceding debug/suspended
	 * notification so the gutter marker and autoscroll remain correct. */
	if (cJSON_IsNumber(curline_j)) {
		s->current_line = (long)curline_j->valuedouble;
	}

	/* Release old source */
	tui_free_script_lines(s);

	/* P1-C: calloc so unfilled slots are guaranteed NULL even if strdup
	 * fails partway through the population loop. */
	s->script_lines = (char **)calloc((size_t)count, sizeof(char *));
	if (s->script_lines == NULL) return;
	/* Commit count AFTER allocation succeeds so script_line_count always
	 * reflects the true population of the array. */
	s->script_line_count = count;

	/* Reset breakpoints from this source load.
	 * 2026-06-07 JES Phase B.4 #743 round 1: free any stored condition strings
	 * before zeroing the count so we don't leak heap on source reload. */
	for (int bi = 0; bi < s->bp_line_count; bi++) {
		free(s->bp_conditions[bi]);
		s->bp_conditions[bi] = NULL;
	}
	s->bp_line_count = 0;

	int i = 0;
	cJSON *lineobj = NULL;
	cJSON_ArrayForEach(lineobj, lines_j) {
		if (i >= count) break; /* honour the P1-B truncation cap */

		cJSON *text_j = cJSON_GetObjectItemCaseSensitive(lineobj, "text");
		cJSON *num_j  = cJSON_GetObjectItemCaseSensitive(lineobj, "num");
		cJSON *bp_j   = cJSON_GetObjectItemCaseSensitive(lineobj, "breakpoint");

		const char *text = (cJSON_IsString(text_j) && text_j->valuestring != NULL)
		                   ? text_j->valuestring : "";
		s->script_lines[i] = strdup(text);
		if (s->script_lines[i] == NULL) {
			/* P1-C: both strdup attempts failed (OOM).  Truncate to the
			 * successfully populated slots so draw_script_pane never
			 * dereferences a NULL entry via strlen(). */
			s->script_line_count = i;
			break;
		}

		/* Record breakpoint lines */
		if (cJSON_IsTrue(bp_j) && cJSON_IsNumber(num_j) &&
			s->bp_line_count < TUI_MAX_BREAKPOINTS) {
			s->bp_lines[s->bp_line_count++] = (unsigned long)num_j->valuedouble;
		}

		i++;
	}
}

/* -------------------------------------------------------------------------
 * 2026-06-07 JES Phase B.1 #691: transport write_line -- parse NDJSON responses.
 *
 * Handles two cases:
 *   1. Notification: {"id":null,"op":"debug/suspended","params":{...}}
 *      -> stores pending_thread_id and current_line; triggers source load
 *         by storing the script path and invalidating the script pane.
 *         In B.1 the source load is done inline using the stored source; the
 *         full op_dispatch path for live sessions is wired in B.6.
 *
 *   2. Response to debug/getSource: {"id":N,"result":{"script":...,"lines":[...]}}
 *      -> parses and stores source lines via tui_store_source(); invalidates
 *         the script pane.
 *
 * Three-parameter signature per op_handler.h:45 (NOT the 2-param form the
 * handoff doc incorrectly listed; see EXECUTION_PLAN.md 2.1 Claim 1).
 * ---------------------------------------------------------------------- */

static void tui_write_line(void *ctx, const char *json, size_t len) {
	/* 2026-06-07 JES Phase B.1 #691: parse NDJSON responses/notifications. */
	tui_state_t *s = (tui_state_t *)ctx;
	if (s == NULL || json == NULL || len == 0) return;

	cJSON *root = cJSON_ParseWithLength(json, len);
	if (root == NULL) return;

	cJSON *op_j     = cJSON_GetObjectItemCaseSensitive(root, "op");
	cJSON *result_j = cJSON_GetObjectItemCaseSensitive(root, "result");

	if (cJSON_IsString(op_j) && op_j->valuestring != NULL &&
		strcmp(op_j->valuestring, "debug/suspended") == 0) {
		/* Case 1: suspended notification */
		cJSON *params_j  = cJSON_GetObjectItemCaseSensitive(root, "params");
		cJSON *tid_j     = params_j ? cJSON_GetObjectItemCaseSensitive(params_j, "threadId") : NULL;
		cJSON *line_j    = params_j ? cJSON_GetObjectItemCaseSensitive(params_j, "line")     : NULL;
		cJSON *script_j2 = params_j ? cJSON_GetObjectItemCaseSensitive(params_j, "script")   : NULL;

		if (cJSON_IsNumber(tid_j))
			s->pending_thread_id = (long)tid_j->valuedouble;
		if (cJSON_IsNumber(line_j))
			s->current_line = (long)line_j->valuedouble;
		if (cJSON_IsString(script_j2) && script_j2->valuestring != NULL) {
			strncpy(s->script_path, script_j2->valuestring,
			        sizeof(s->script_path) - 1);
			s->script_path[sizeof(s->script_path) - 1] = '\0';
		}

		/* 2026-06-07 JES Phase B.3 #691: transition to SUSPENDED state.
		 * This is the path after a step completes or a breakpoint fires.
		 * SENTINEL: do NOT call debug/getStack or debug/getSource while
		 * TUI_DEBUG_RUNNING (thread holds GIL); this transition happens
		 * inside write_line which arrives after the thread has suspended
		 * again, so it is safe. */
		s->debug_state = TUI_DEBUG_SUSPENDED;

		/* 2026-06-07 JES Phase B.5 #691: clear any stale identifier popup.
		 * A new suspension point makes the previous resolution irrelevant. */
		s->identifier_popup_active  = false;
		s->identifier_popup_line[0] = '\0';

		/* Invalidate both panes so the next present() redraws them */
		if (s->script_win != NULL)
			boxen_window_invalidate(s->script_win);
		if (s->stack_win != NULL)
			boxen_window_invalidate(s->stack_win);
		if (s->footer_win != NULL)
			boxen_window_invalidate(s->footer_win);

	} else if (cJSON_IsObject(result_j)) {
		/* Case 2: response containing a "result" object.
		 * Discriminate by which array key is present in the result:
		 *   "lines"  -> debug/getSource response  (B.1)
		 *   "frames" -> debug/getStack response   (B.2)
		 *   "locals" -> debug/getLocals response  (B.2) */
		cJSON *lines_j  = cJSON_GetObjectItemCaseSensitive(result_j, "lines");
		cJSON *frames_j = cJSON_GetObjectItemCaseSensitive(result_j, "frames");
		cJSON *locals_j = cJSON_GetObjectItemCaseSensitive(result_j, "locals");

		if (cJSON_IsArray(lines_j)) {
			/* debug/getSource response */
			tui_store_source(s, result_j);

			/* Auto-scroll to current line if suspended */
			if (s->current_line > 0 && s->script_win != NULL) {
				boxen_window_ensure_visible(s->script_win, 0,
				                            (int)(s->current_line - 1));
			}
			if (s->script_win != NULL)
				boxen_window_invalidate(s->script_win);

		} else if (cJSON_IsArray(frames_j)) {
			/* 2026-06-07 JES Phase B.2 #691: debug/getStack response */
			tui_store_stack(s, result_j);
			if (s->stack_win != NULL)
				boxen_window_invalidate(s->stack_win);

		} else if (cJSON_IsArray(locals_j)) {
			/* 2026-06-07 JES Phase B.2 #691: debug/getLocals response */
			tui_store_locals(s, result_j);
			if (s->stack_win != NULL)
				boxen_window_invalidate(s->stack_win);
		}
	}

	cJSON_Delete(root);
}

/* -------------------------------------------------------------------------
 * 2026-06-07 JES Phase B.1 #691: draw callbacks.
 *
 * draw_script_pane -- renders source lines with current-line highlight.
 *
 * Current-line highlight: we draw highlighted manually (BOXEN_ATTR_REVERSE
 * on every cell of that content row) rather than calling
 * boxen_window_set_row_highlight(). Reason: set_row_highlight writes spaces
 * over cell characters (A.5 known limitation documented in boxen.h:466-482),
 * erasing the gutter marker and source text. Drawing the row manually with
 * BOXEN_ATTR_REVERSE preserves the character content.
 *
 * Gutter column (content col 0):
 *   '>' current execution line
 *   '*' line with a breakpoint
 *   ' ' otherwise
 *
 * Off-by-one (EXECUTION_PLAN.md B.1 Sentinels):
 *   ODB line numbers are 1-based; boxen content rows are 0-based.
 *   content_row = line_num - 1.
 *
 * Auto-scroll: boxen_window_ensure_visible is called here so that every
 * redraw keeps the current line visible. This covers both the suspended-
 * notification path (via write_line -> invalidate -> redraw) and any
 * explicit script pane invalidation that could move the viewport.
 * ---------------------------------------------------------------------- */

static void draw_script_pane(boxen_window_t *win, void *ud) {
	tui_state_t *s = (tui_state_t *)ud;
	int w = boxen_window_content_width(win);
	if (w <= 0) return;

	if (s == NULL || s->script_lines == NULL || s->script_line_count <= 0) {
		/* No source loaded yet -- show placeholder */
		boxen_draw_text(win, 0, 0,
		                "Script source will appear here (B.1)",
		                BOXEN_COLOR_DEFAULT, BOXEN_COLOR_DEFAULT, BOXEN_ATTR_DIM);
		return;
	}

	/* Tell boxen the total content height so it can compute scroll limits and
	 * render the scrollbar. Must be set before ensure_visible. */
	boxen_window_set_content_size(win, w, s->script_line_count);

	/* Auto-scroll: ensure the current line is visible in the viewport */
	if (s->current_line > 0) {
		boxen_window_ensure_visible(win, 0, (int)(s->current_line - 1));
	}

	/* Render each line */
	for (int i = 0; i < s->script_line_count; i++) {
		int content_row = i;      /* 0-based content row */
		long line_num   = i + 1; /* 1-based line number */

		/* Determine gutter character */
		char gutter = ' ';
		if (line_num == s->current_line) {
			gutter = '>';
		} else {
			for (int b = 0; b < s->bp_line_count; b++) {
				if (s->bp_lines[b] == (unsigned long)line_num) {
					gutter = '*';
					break;
				}
			}
		}

		bool is_current = (line_num == s->current_line);
		uint16_t attr   = is_current ? BOXEN_ATTR_REVERSE : BOXEN_ATTR_NONE;
		uint16_t fg     = BOXEN_COLOR_DEFAULT;
		uint16_t bg     = BOXEN_COLOR_DEFAULT;

		/* Draw gutter */
		boxen_set_cell(win, 0, content_row, (uint32_t)gutter, fg, bg, attr);

		/* Draw source text (truncated to available width) */
		/* P1-C: belt-and-suspenders NULL guard; calloc + truncation in
		 * tui_store_source already prevent NULL entries, but a defensive
		 * check here keeps the renderer safe regardless of how lines were
		 * populated. */
		const char *text = s->script_lines[i] != NULL ? s->script_lines[i] : "";
		int text_len = (int)strlen(text);
		int avail    = w - 1; /* one column reserved for gutter */
		if (avail <= 0) continue;

		/* P1-A: cap avail to linebuf capacity so memset + NUL write cannot
		 * overflow the stack frame on terminals wider than 512 columns
		 * (e.g. tmux splits, wide xterms). */
		char linebuf[512];
		int cap = (int)sizeof(linebuf) - 1;
		if (avail > cap) avail = cap;

		/* Build a padded/truncated line buffer for uniform cell coverage */
		int copy = text_len < avail ? text_len : avail;
		memcpy(linebuf, text, (size_t)copy);
		/* Pad remainder with spaces so REVERSE attr covers the full row */
		memset(linebuf + copy, ' ', (size_t)(avail - copy));
		linebuf[avail] = '\0';

		boxen_draw_text(win, 1, content_row, linebuf, fg, bg, attr);
	}

	/* 2026-06-07 JES Phase B.5 #691: render identifier popup if active.
	 * The popup is shown as a highlighted status line at the LAST visible
	 * content row of the window.  We use the viewport height (not content size)
	 * to determine the last visible row, then draw the popup there.
	 *
	 * The popup is drawn AFTER the source lines so it always overlays the
	 * bottom row of the visible area.  This is a simple status-bar style: no
	 * separate window, no modal -- just a DIM-highlighted row at the bottom.
	 *
	 * Scroll interaction: the popup is always visible regardless of scroll
	 * position (it's drawn in screen-relative terms via the content viewport). */
	if (s->identifier_popup_active && s->identifier_popup_line[0] != '\0') {
		int viewport_h = boxen_window_content_height(win);
		if (viewport_h > 0) {
			int scroll_y = 0, scroll_x = 0;
			boxen_window_get_scroll(win, &scroll_x, &scroll_y);
			/* Draw on the last visible content row (scroll_y + viewport_h - 1) */
			int popup_row = scroll_y + viewport_h - 1;

			const char *popup = s->identifier_popup_line;
			int popup_len = (int)strlen(popup);
			char linebuf2[512];
			int cap2  = (int)sizeof(linebuf2) - 1;
			int avail2 = (w - 1) < cap2 ? (w - 1) : cap2;
			if (avail2 < 0) avail2 = 0;
			int copy2 = popup_len < avail2 ? popup_len : avail2;
			memcpy(linebuf2, popup, (size_t)copy2);
			memset(linebuf2 + copy2, ' ', (size_t)(avail2 - copy2));
			linebuf2[avail2] = '\0';

			/* Gutter cell for popup row: space */
			boxen_set_cell(win, 0, popup_row, (uint32_t)' ',
			               BOXEN_COLOR_DEFAULT, BOXEN_COLOR_DEFAULT, BOXEN_ATTR_DIM);
			boxen_draw_text(win, 1, popup_row, linebuf2,
			                BOXEN_COLOR_DEFAULT, BOXEN_COLOR_DEFAULT, BOXEN_ATTR_DIM);
		}
	}
}

/* -------------------------------------------------------------------------
 * 2026-06-07 JES Phase B.2 #691: draw_stack_pane
 *
 * Renders the right pane with two sections:
 *
 *   Top half: call stack list.  One row per frame, outermost first.
 *     Format:   "  L1  outer.script:10"
 *     Selected frame row uses BOXEN_ATTR_REVERSE for the highlight.
 *
 *   Separator: "--- Locals ---" at the midpoint row.
 *
 *   Bottom half: flat list of "varname = value" for the innermost suspended
 *     frame's locals.  debug/getLocals has no per-frame variant so locals
 *     always reflect the innermost frame regardless of selection
 *     (EXECUTION_PLAN.md B.2 Sentinels).
 *
 * Linebuf safety (applying B.1 round-1 P1-A lesson):
 *   char linebuf[512]; int cap = sizeof(linebuf) - 1;
 *   avail = min(avail, cap) BEFORE any snprintf/memcpy into linebuf.
 *   This prevents stack smash on terminals wider than 512 columns.
 * ---------------------------------------------------------------------- */

static void draw_stack_pane(boxen_window_t *win, void *ud) {
	tui_state_t *s = (tui_state_t *)ud;
	int w = boxen_window_content_width(win);
	if (w <= 0) return;

	if (s == NULL) return;

	/* Show placeholder if no stack is loaded yet */
	if (s->frame_count <= 0 && s->local_count <= 0) {
		boxen_draw_text(win, 0, 0,
		                "Call stack / locals (suspended to load)",
		                BOXEN_COLOR_DEFAULT, BOXEN_COLOR_DEFAULT, BOXEN_ATTR_DIM);
		return;
	}

	char linebuf[512];
	int  cap   = (int)sizeof(linebuf) - 1;
	int  avail = w < cap ? w : cap;

	/* Compute the split point: roughly half the content height.
	 * Stack section rows: 0 .. (split-1)
	 * Separator row: split
	 * Locals section rows: (split+1) .. end */
	int content_rows = s->frame_count + 1 /* separator */ + s->local_count;
	/* Use at least 4 rows for each section even if the pane is tiny */
	int stack_rows = (s->frame_count > 0) ? s->frame_count : 0;
	int sep_row    = stack_rows;
	int local_start = sep_row + 1;

	boxen_window_set_content_size(win, w, content_rows > 0 ? content_rows : 1);

	/* -- Frame stack section -- */
	for (int i = 0; i < s->frame_count; i++) {
		bool is_selected = (i == s->selected_frame);
		uint16_t attr    = is_selected ? BOXEN_ATTR_REVERSE : BOXEN_ATTR_NONE;

		/* Format: "  L<level>  <script>:<line>" or "  L<level>  <script>" if line==0 */
		int level = i + 1; /* 1-based level, matching debug/getStack "level" field */
		int n;
		if (s->frame_lines[i] > 0) {
			n = snprintf(linebuf, (size_t)(avail + 1),
			             "  L%-2d  %s:%ld",
			             level, s->frame_scripts[i], s->frame_lines[i]);
		} else {
			n = snprintf(linebuf, (size_t)(avail + 1),
			             "  L%-2d  %s",
			             level, s->frame_scripts[i]);
		}
		/* Clamp and NUL-terminate to avail chars (snprintf may have truncated) */
		if (n < 0) n = 0;
		if (n > avail) n = avail;
		/* Pad to avail with spaces so REVERSE attr covers the full row */
		memset(linebuf + n, ' ', (size_t)(avail - n));
		linebuf[avail] = '\0';

		boxen_draw_text(win, 0, i, linebuf,
		                BOXEN_COLOR_DEFAULT, BOXEN_COLOR_DEFAULT, attr);
	}

	/* -- Separator -- */
	{
		const char *sep = "--- Locals ---";
		int sep_len = (int)strlen(sep);
		int copy    = sep_len < avail ? sep_len : avail;
		memcpy(linebuf, sep, (size_t)copy);
		memset(linebuf + copy, '-', (size_t)(avail - copy));
		linebuf[avail] = '\0';
		boxen_draw_text(win, 0, sep_row, linebuf,
		                BOXEN_COLOR_DEFAULT, BOXEN_COLOR_DEFAULT, BOXEN_ATTR_DIM);
	}

	/* -- Locals section -- */
	for (int i = 0; i < s->local_count; i++) {
		/* Belt-and-suspenders NULL guards (calloc + truncation in tui_store_locals
		 * already prevent NULL entries, but draw must never crash regardless of
		 * how the arrays were populated). */
		const char *name  = (s->local_names  && s->local_names[i])  ? s->local_names[i]  : "";
		const char *value = (s->local_values && s->local_values[i]) ? s->local_values[i] : "";

		int n = snprintf(linebuf, (size_t)(avail + 1), "%s = %s", name, value);
		if (n < 0) n = 0;
		if (n > avail) n = avail;
		linebuf[n] = '\0';

		boxen_draw_text(win, 0, local_start + i, linebuf,
		                BOXEN_COLOR_DEFAULT, BOXEN_COLOR_DEFAULT, BOXEN_ATTR_NONE);
	}
}

/* -------------------------------------------------------------------------
 * 2026-06-07 JES Phase B.3 #691: debug op dispatch helpers.
 *
 * tui_dispatch_json -- central dispatch: writes to op_dispatch in production,
 *   or to the dispatch_capture_buf when set (test mode only).
 *
 * tui_do_continue / tui_do_step_over / tui_do_step_into / tui_do_step_out --
 *   synthesize the JSON request string and hand it to tui_dispatch_json.
 *   After dispatching, set debug_state = TUI_DEBUG_RUNNING.
 *
 * Dispatch pattern per EXECUTION_PLAN.md B.3:
 *   The TUI dispatches via op_dispatch with synthesized JSON and its own
 *   transport. This is consistent with the protocol path and avoids needing
 *   to know which handle_debug_* function maps to which op.
 *
 * SENTINEL (EXECUTION_PLAN.md B.3): NEVER call these while
 *   debug_state != TUI_DEBUG_SUSPENDED. Check callers.
 *
 * Re-entrancy: F-key mashing before the runtime responds cannot corrupt state
 *   because the on_input callback gates on debug_state == TUI_DEBUG_SUSPENDED.
 *   A second F5 press while TUI_DEBUG_RUNNING is silently ignored.
 * ---------------------------------------------------------------------- */

/* op_dispatch is defined in op_handler.c; not linked in test builds.
 * tui_dispatch_json is exercised by tests via the dispatch_capture_buf path,
 * so op_dispatch is never called (and never needs to be linked) in test
 * builds.  The full prototype is in op_handler.h, included transitively via
 * debugger_tui_internal.h. */
static void tui_dispatch_json(tui_state_t *s, const char *json) {
	size_t len = strlen(json);

	/* 2026-06-07 JES Phase B.4 #691: multi-dispatch log (test mode only).
	 * When dispatch_log is non-NULL, record each call in order up to cap.
	 * This runs BEFORE the single-slot capture so both are always populated. */
	if (s->dispatch_log != NULL && s->dispatch_log_count < s->dispatch_log_cap) {
		int slot = s->dispatch_log_count;
		int cap  = (int)sizeof(s->dispatch_log[0]) - 1;
		int copy = (int)len < cap ? (int)len : cap;
		memcpy(s->dispatch_log[slot], json, (size_t)copy);
		s->dispatch_log[slot][copy] = '\0';
		s->dispatch_log_count++;
	}

	if (s->dispatch_capture_buf != NULL && s->dispatch_capture_cap > 0) {
		/* Test mode: capture the JSON instead of calling into the runtime */
		int cap = s->dispatch_capture_cap - 1;
		int copy = (int)len < cap ? (int)len : cap;
		memcpy(s->dispatch_capture_buf, json, (size_t)copy);
		s->dispatch_capture_buf[copy] = '\0';
		return;
	}
	/* Production mode: route through op_dispatch (full runtime required). */
	op_dispatch(json, len, s->transport);
}

/* Request ID counter; wraps at 0x7FFF to avoid int overflow on long sessions. */
static int g_tui_req_id = 1;
static int next_req_id(void) {
	int id = g_tui_req_id++;
	if (g_tui_req_id > 0x7FFF) g_tui_req_id = 1;
	return id;
}

static void tui_do_continue(tui_state_t *s) {
	/* SENTINEL: only call while TUI_DEBUG_SUSPENDED */
	char req[256];
	snprintf(req, sizeof(req),
	         "{\"op\":\"debug/continue\",\"id\":%d,\"params\":{\"threadId\":%ld}}",
	         next_req_id(), s->pending_thread_id);
	s->debug_state = TUI_DEBUG_RUNNING;
	/* 2026-06-07 JES Phase B.4 #743 round 1: clear current_line when transitioning
	 * to RUNNING so F9 cannot fire against a stale last-suspended line position. */
	s->current_line = 0;
	tui_dispatch_json(s, req);
}

static void tui_do_step(tui_state_t *s, const char *direction) {
	/* SENTINEL: only call while TUI_DEBUG_SUSPENDED */
	char req[256];
	snprintf(req, sizeof(req),
	         "{\"op\":\"debug/step\",\"id\":%d,\"params\":"
	         "{\"threadId\":%ld,\"direction\":\"%s\"}}",
	         next_req_id(), s->pending_thread_id, direction);
	s->debug_state = TUI_DEBUG_RUNNING;
	/* 2026-06-07 JES Phase B.4 #743 round 1: clear current_line when transitioning
	 * to RUNNING.  Symmetric with the continue path above. */
	s->current_line = 0;
	tui_dispatch_json(s, req);
}

static void tui_do_step_over(tui_state_t *s)  { tui_do_step(s, "over"); }
static void tui_do_step_into(tui_state_t *s)  { tui_do_step(s, "into"); }
static void tui_do_step_out(tui_state_t *s)   { tui_do_step(s, "out");  }

/* -------------------------------------------------------------------------
 * 2026-06-07 JES Phase B.4 #743 round 1: JSON string escape helper.
 *
 * tui_json_escape -- escape src into dst for safe interpolation into a JSON
 * string value.  Handles: " -> \", \ -> \\, and control characters using
 * \b \f \n \r \t and \uXXXX for all other code points < 0x20.
 *
 * Worst-case expansion: 6x (every byte -> \u00XX), so dst must be at least
 * strlen(src)*6 + 1 bytes.  For a 256-byte script_path that is 1537 bytes.
 *
 * dst is always NUL-terminated even when src is empty.  The function returns
 * the number of bytes written to dst (not counting the NUL).
 *
 * Callers allocate dst on the stack sized for the 6x worst case.  All four
 * interpolation sites for script_path use a 1537-byte dst buffer.
 * ---------------------------------------------------------------------- */
static int tui_json_escape(const char *src, char *dst, size_t dst_cap) {
	/* dst_cap must include space for the NUL terminator */
	if (dst == NULL || dst_cap == 0) return 0;
	int out = 0;
	int cap = (int)dst_cap - 1; /* reserve one byte for NUL */
	for (const char *p = src; *p != '\0'; p++) {
		unsigned char c = (unsigned char)*p;
		if (c == '"') {
			if (out + 2 > cap) break;
			dst[out++] = '\\';
			dst[out++] = '"';
		} else if (c == '\\') {
			if (out + 2 > cap) break;
			dst[out++] = '\\';
			dst[out++] = '\\';
		} else if (c == '\b') {
			if (out + 2 > cap) break;
			dst[out++] = '\\'; dst[out++] = 'b';
		} else if (c == '\f') {
			if (out + 2 > cap) break;
			dst[out++] = '\\'; dst[out++] = 'f';
		} else if (c == '\n') {
			if (out + 2 > cap) break;
			dst[out++] = '\\'; dst[out++] = 'n';
		} else if (c == '\r') {
			if (out + 2 > cap) break;
			dst[out++] = '\\'; dst[out++] = 'r';
		} else if (c == '\t') {
			if (out + 2 > cap) break;
			dst[out++] = '\\'; dst[out++] = 't';
		} else if (c < 0x20) {
			/* Other control characters: \u00XX */
			if (out + 6 > cap) break;
			dst[out++] = '\\'; dst[out++] = 'u';
			dst[out++] = '0';  dst[out++] = '0';
			static const char hex[] = "0123456789abcdef";
			dst[out++] = hex[(c >> 4) & 0x0F];
			dst[out++] = hex[c        & 0x0F];
		} else {
			if (out + 1 > cap) break;
			dst[out++] = (char)c;
		}
	}
	dst[out] = '\0';
	return out;
}

/* -------------------------------------------------------------------------
 * 2026-06-07 JES Phase B.4 #691: breakpoint toggle (F9).
 *
 * tui_toggle_breakpoint: toggle a breakpoint at (script_path, line).
 *
 * If no breakpoint exists at the line: dispatch debug/setBreakpoint.
 * If a breakpoint exists: dispatch debug/clearBreakpoints (clears ALL), then
 *   re-dispatch debug/setBreakpoint for every surviving breakpoint.
 *
 * This clear-and-readd N-1 round trip is the correct approach because
 * handle_debug_clearbreakpoints clears ALL breakpoints (no per-line clear op
 * exists in the current runtime).  See EXECUTION_PLAN.md B.4 "Note on clear".
 *
 * SENTINEL (EXECUTION_PLAN.md B.4): all these dispatches happen under the
 * GIL with the thread suspended, so the responses arrive synchronously via
 * write_line (the handlers run on the calling thread).  No async complication.
 *
 * tui_close_condition_modal: shared close path for Enter/Escape.
 *   Hides cursor, releases modal gate, closes window, clears state field.
 * ---------------------------------------------------------------------- */

static void tui_toggle_breakpoint(tui_state_t *s, long line) {
	/* SENTINEL: only call while TUI_DEBUG_SUSPENDED and line > 0 */
	if (line <= 0 || s->script_path[0] == '\0') return;

	/* 2026-06-07 JES Phase B.4 #743 round 1: JSON-escape script_path once for
	 * all dispatch sites in this function.  Worst case: 256 bytes * 6 + NUL. */
	char esc_path[256 * 6 + 1];
	tui_json_escape(s->script_path, esc_path, sizeof(esc_path));

	/* Check if a breakpoint exists at this line in the local cache */
	bool bp_exists = false;
	for (int i = 0; i < s->bp_line_count; i++) {
		if (s->bp_lines[i] == (unsigned long)line) {
			bp_exists = true;
			break;
		}
	}

	/* req must hold: JSON boilerplate + esc_path (up to 1537) + condition.
	 * Use 512 + sizeof(esc_path) + TUI_BP_CONDITION_MAX*2 for safety. */
	char req[512 + sizeof(esc_path) + TUI_BP_CONDITION_MAX * 2];

	if (!bp_exists) {
		/* Toggle ON: just add the unconditional breakpoint */
		snprintf(req, sizeof(req),
		         "{\"op\":\"debug/setBreakpoint\",\"id\":%d,"
		         "\"params\":{\"script\":\"%s\",\"line\":%ld}}",
		         next_req_id(), esc_path, line);
		tui_dispatch_json(s, req);

		/* Update local cache: no condition (unconditional BP) */
		if (s->bp_line_count < TUI_MAX_BREAKPOINTS) {
			s->bp_lines[s->bp_line_count]      = (unsigned long)line;
			s->bp_conditions[s->bp_line_count] = NULL; /* unconditional */
			s->bp_line_count++;
		}
	} else {
		/* Toggle OFF: clear all, re-add all except the toggled line.
		 * 2026-06-07 JES Phase B.4 #743 round 1: re-dispatch surviving breakpoints
		 * WITH their stored conditions so conditions are not silently stripped. */
		snprintf(req, sizeof(req),
		         "{\"op\":\"debug/clearBreakpoints\",\"id\":%d,"
		         "\"params\":{\"script\":\"%s\"}}",
		         next_req_id(), esc_path);
		tui_dispatch_json(s, req);

		/* Re-add all surviving breakpoints, preserving each one's condition */
		for (int i = 0; i < s->bp_line_count; i++) {
			if (s->bp_lines[i] != (unsigned long)line) {
				if (s->bp_conditions[i] != NULL) {
					/* Conditional re-add: escape condition string */
					char esc_cond[TUI_BP_CONDITION_MAX * 2 + 1];
					tui_json_escape(s->bp_conditions[i], esc_cond, sizeof(esc_cond));
					snprintf(req, sizeof(req),
					         "{\"op\":\"debug/setBreakpoint\",\"id\":%d,"
					         "\"params\":{\"script\":\"%s\",\"line\":%lu,"
					         "\"condition\":\"%s\"}}",
					         next_req_id(), esc_path,
					         s->bp_lines[i], esc_cond);
				} else {
					/* Unconditional re-add */
					snprintf(req, sizeof(req),
					         "{\"op\":\"debug/setBreakpoint\",\"id\":%d,"
					         "\"params\":{\"script\":\"%s\",\"line\":%lu}}",
					         next_req_id(), esc_path, s->bp_lines[i]);
				}
				tui_dispatch_json(s, req);
			}
		}

		/* Update local cache: remove the toggled line; free its condition */
		int new_count = 0;
		for (int i = 0; i < s->bp_line_count; i++) {
			if (s->bp_lines[i] != (unsigned long)line) {
				s->bp_lines[new_count]      = s->bp_lines[i];
				s->bp_conditions[new_count] = s->bp_conditions[i];
				new_count++;
			} else {
				/* Free condition string for the removed BP */
				free(s->bp_conditions[i]);
				s->bp_conditions[i] = NULL;
			}
		}
		s->bp_line_count = new_count;
	}

	if (s->script_win != NULL) boxen_window_invalidate(s->script_win);
}

/* -------------------------------------------------------------------------
 * 2026-06-07 JES Phase B.4 #691: conditional breakpoint modal.
 *
 * draw_condition_modal: draw callback.
 *   Renders a centered overlay with title "Set Condition" and the current
 *   condition_buf content followed by a visual cursor position marker.
 *   Calls boxen_window_set_cursor via the A.7 API to position the real
 *   terminal cursor at the input position.
 *
 * input_condition_modal: input callback for the modal window.
 *   Printable characters appended to bp_condition_buf (capped at
 *   TUI_BP_CONDITION_MAX - 1).
 *   BACKSPACE trims the buffer.
 *   ENTER confirms: dispatch setBreakpoint with condition, close modal.
 *   ESCAPE cancels: close modal without dispatching.
 *
 * tui_close_condition_modal: shared close/cleanup path.
 *   1. boxen_window_set_cursor_visible(modal, false)  -- hide terminal cursor
 *   2. boxen_window_set_modal(modal, false)           -- release modal gate
 *   3. boxen_window_close(modal)                      -- destroy window
 *   4. s->condition_modal_win = NULL                  -- clear state
 *   5. Refocus script_win so F-keys work again.
 *
 * tui_open_condition_modal: open a centered condition modal.
 *   Creates a boxen_window_t via boxen_window_open, calls set_modal(true),
 *   sets cursor position to content (0, 2) [the input row], calls
 *   set_cursor_visible(true).
 * ---------------------------------------------------------------------- */

static void tui_close_condition_modal(tui_state_t *s) {
	if (s->condition_modal_win == NULL) return;
	/* A.7 API: hide cursor before releasing modal ownership */
	boxen_window_set_cursor_visible(s->condition_modal_win, false);
	/* Release modal gate -- this is the critical step that gives cursor
	 * ownership back to the focused window. */
	boxen_window_set_modal(s->condition_modal_win, false);
	boxen_window_close(s->condition_modal_win);
	s->condition_modal_win = NULL;
	/* Restore focus to script pane so F-keys and frame navigation work */
	if (s->script_win != NULL) boxen_window_focus(s->script_win);
}

/* Forward declaration: input_condition_modal calls tui_close_condition_modal
 * and tui_toggle_breakpoint, which are defined above, so no forward decl
 * needed for those.  draw_condition_modal is referenced by tui_open_ which
 * is defined after; declare forward here. */
static void draw_condition_modal(boxen_window_t *win, void *ud);

static void input_condition_modal(boxen_window_t *win, const boxen_event_t *ev, void *ud) {
	(void)win;
	tui_state_t *s = (tui_state_t *)ud;
	if (ev->type != BOXEN_EV_KEY) return;

	if (ev->key.key == BOXEN_KEY_ESCAPE) {
		/* Cancel: close without dispatching */
		tui_close_condition_modal(s);
		return;
	}

	if (ev->key.key == BOXEN_KEY_ENTER) {
		/* Confirm: dispatch setBreakpoint with condition, then close.
		 *
		 * 2026-06-07 JES Phase B.4 #743 round 1: use tui_json_escape for both
		 * script_path and condition (replaces the inline escape loop that only
		 * handled " and \ -- tui_json_escape also covers control characters). */
		char esc_path[256 * 6 + 1];
		tui_json_escape(s->script_path, esc_path, sizeof(esc_path));

		char esc_cond[TUI_BP_CONDITION_MAX * 6 + 1];
		tui_json_escape(s->bp_condition_buf, esc_cond, sizeof(esc_cond));

		char req[512 + sizeof(esc_path) + sizeof(esc_cond)];
		snprintf(req, sizeof(req),
		         "{\"op\":\"debug/setBreakpoint\",\"id\":%d,"
		         "\"params\":{\"script\":\"%s\",\"line\":%ld,"
		         "\"condition\":\"%s\"}}",
		         next_req_id(), esc_path, s->current_line, esc_cond);

		/* Update local cache: add or replace the conditional breakpoint.
		 * Remove any existing entry for this line first (free its condition),
		 * then insert the new conditional entry.
		 * 2026-06-07 JES Phase B.4 #743 round 1: store condition in bp_conditions
		 * so it survives a future toggle-off clear-and-readd cycle. */
		int new_count = 0;
		for (int i = 0; i < s->bp_line_count; i++) {
			if (s->bp_lines[i] != (unsigned long)s->current_line) {
				s->bp_lines[new_count]      = s->bp_lines[i];
				s->bp_conditions[new_count] = s->bp_conditions[i];
				new_count++;
			} else {
				/* Free old condition (may be NULL if it was unconditional) */
				free(s->bp_conditions[i]);
				s->bp_conditions[i] = NULL;
			}
		}
		if (new_count < TUI_MAX_BREAKPOINTS) {
			s->bp_lines[new_count]      = (unsigned long)s->current_line;
			/* strdup the condition so it outlives bp_condition_buf (which is
			 * cleared on the next modal open).  NULL on OOM -- treated as
			 * unconditional in the re-dispatch path, which is safe. */
			s->bp_conditions[new_count] = strdup(s->bp_condition_buf);
			new_count++;
		}
		s->bp_line_count = new_count;

		tui_close_condition_modal(s);
		tui_dispatch_json(s, req);

		if (s->script_win != NULL) boxen_window_invalidate(s->script_win);
		return;
	}

	if (ev->key.key == BOXEN_KEY_BACKSPACE) {
		if (s->bp_condition_len > 0) {
			s->bp_condition_len--;
			s->bp_condition_buf[s->bp_condition_len] = '\0';
		}
	} else if (ev->key.key == BOXEN_KEY_NONE && ev->key.ch >= 0x20 && ev->key.ch < 0x7F) {
		/* Printable ASCII: append if space permits (capped at TUI_BP_CONDITION_MAX-1) */
		if (s->bp_condition_len < TUI_BP_CONDITION_MAX - 1) {
			s->bp_condition_buf[s->bp_condition_len++] = (char)ev->key.ch;
			s->bp_condition_buf[s->bp_condition_len]   = '\0';
		}
	}

	/* Reposition cursor to follow the input (A.7 API).
	 * Modal content layout: row 0 = title, row 1 = blank, row 2 = input.
	 * Cursor column = bp_condition_len (one past the last typed character). */
	if (s->condition_modal_win != NULL) {
		boxen_window_set_cursor(s->condition_modal_win,
		                        s->bp_condition_len, 2);
		/* Redraw so the typed character appears */
		boxen_window_invalidate(s->condition_modal_win);
	}
}

static void draw_condition_modal(boxen_window_t *win, void *ud) {
	tui_state_t *s = (tui_state_t *)ud;
	int w = boxen_window_content_width(win);
	if (w <= 0) return;

	/* Cap to linebuf capacity (B.1 round-1 P1-A lesson) */
	char linebuf[512];
	int  cap   = (int)sizeof(linebuf) - 1;
	int  avail = w < cap ? w : cap;

	/* Row 0: title */
	{
		const char *title = "Set Condition (Enter=confirm  Esc=cancel)";
		int n = snprintf(linebuf, (size_t)(avail + 1), "%-*.*s", avail, avail, title);
		(void)n;
		boxen_draw_text(win, 0, 0, linebuf,
		                BOXEN_COLOR_DEFAULT, BOXEN_COLOR_DEFAULT, BOXEN_ATTR_BOLD);
	}

	/* Row 1: blank separator */
	{
		memset(linebuf, ' ', (size_t)avail);
		linebuf[avail] = '\0';
		boxen_draw_text(win, 0, 1, linebuf,
		                BOXEN_COLOR_DEFAULT, BOXEN_COLOR_DEFAULT, BOXEN_ATTR_NONE);
	}

	/* Row 2: condition input buffer.
	 * Pad to avail with spaces so the input row is fully drawn. */
	{
		const char *cond = s != NULL ? s->bp_condition_buf : "";
		int clen = s != NULL ? s->bp_condition_len : 0;
		int copy = clen < avail ? clen : avail;
		memcpy(linebuf, cond, (size_t)copy);
		memset(linebuf + copy, ' ', (size_t)(avail - copy));
		linebuf[avail] = '\0';
		boxen_draw_text(win, 0, 2, linebuf,
		                BOXEN_COLOR_DEFAULT, BOXEN_COLOR_DEFAULT, BOXEN_ATTR_NONE);
	}

	/* Position the real terminal cursor at the end of the typed text.
	 * Uses the A.7 public API: chrome, scroll, and viewport clipping are
	 * applied automatically.  DO NOT use the reverse-video block-cell
	 * workaround -- the A.7 API is live (EXECUTION_PLAN.md B.4 Sentinel). */
	if (s != NULL && s->condition_modal_win != NULL) {
		boxen_window_set_cursor(s->condition_modal_win,
		                        s->bp_condition_len, 2);
	}
}

static void tui_open_condition_modal(tui_state_t *s, int tw, int th) {
	/* Clear the condition buffer for fresh input */
	memset(s->bp_condition_buf, 0, sizeof(s->bp_condition_buf));
	s->bp_condition_len = 0;

	/* Open a centered modal window.  Minimum useful size: 60 wide, 5 tall.
	 * Clamp to terminal dimensions so the modal never overflows the screen. */
	int mw = 60;
	int mh = 5;
	if (mw > tw - 4) mw = tw - 4;
	if (mh > th - 4) mh = th - 4;
	if (mw < 20) mw = 20; /* absolute minimum */
	if (mh < 5)  mh = 5;

	int mx = (tw - mw) / 2;
	int my = (th - mh) / 2;
	if (mx < 0) mx = 0;
	if (my < 0) my = 0;

	boxen_rect_t r = { mx, my, mw, mh };
	s->condition_modal_win = boxen_window_open("Set Condition", r, NULL);
	if (s->condition_modal_win == NULL) return;

	boxen_window_set_draw(s->condition_modal_win,  draw_condition_modal);
	boxen_window_set_input(s->condition_modal_win, input_condition_modal);
	boxen_window_set_user_data(s->condition_modal_win, s);

	/* A.7 API: set modal gate so cursor calls route to this window.
	 * This also blocks cursor calls from non-modal windows (the script pane). */
	boxen_window_set_modal(s->condition_modal_win, true);

	/* Position cursor at content (0, 2) -- start of the input row.
	 * set_cursor_visible(true) makes the terminal cursor appear. */
	boxen_window_set_cursor(s->condition_modal_win, 0, 2);
	boxen_window_set_cursor_visible(s->condition_modal_win, true);

	boxen_window_invalidate(s->condition_modal_win);
}

/* -------------------------------------------------------------------------
 * 2026-06-07 JES Phase B.5 #691: identifier extraction helper.
 *
 * extract_identifier_at -- extract the identifier word that covers column `col`
 * (0-based) in `line`.  Word boundary characters: any character that is NOT
 * alphanumeric (a-z, A-Z, 0-9) or underscore ('_').
 *
 * Edge cases (all return empty string in `out`):
 *   - col < 0 or col >= strlen(line): out-of-range click
 *   - character at col is a word boundary: no identifier at that position
 *   - line is NULL or empty
 *
 * `out` is always NUL-terminated.  Truncated to `out_max - 1` characters.
 *
 * Exposed via debugger_tui_internal.h for direct testing.
 * ---------------------------------------------------------------------- */

/* Helper: true if character is an identifier constituent (alphanumeric or '_') */
static bool is_ident_char(char c) {
	return (c >= 'a' && c <= 'z') ||
	       (c >= 'A' && c <= 'Z') ||
	       (c >= '0' && c <= '9') ||
	       (c == '_');
}

void extract_identifier_at(const char *line, int col, char *out, int out_max) {
	if (out == NULL || out_max <= 0) return;
	out[0] = '\0';
	if (line == NULL || col < 0 || out_max <= 1) return;

	int len = (int)strlen(line);
	if (col >= len) return;                   /* past end of line */
	if (!is_ident_char(line[col])) return;    /* not on an identifier character */

	/* Scan left to find the start of the identifier */
	int start = col;
	while (start > 0 && is_ident_char(line[start - 1])) {
		start--;
	}

	/* Scan right to find the end of the identifier */
	int end = col;
	while (end + 1 < len && is_ident_char(line[end + 1])) {
		end++;
	}

	/* Copy [start..end] inclusive, truncate to out_max - 1 */
	int ident_len = end - start + 1;
	int copy = (ident_len < out_max - 1) ? ident_len : out_max - 1;
	memcpy(out, line + start, (size_t)copy);
	out[copy] = '\0';
}

/* -------------------------------------------------------------------------
 * 2026-06-07 JES Phase B.5 #691: identifier resolution by name.
 *
 * tui_resolve_identifier_by_name -- look up `name` in state->local_names.
 *
 * If found: populate identifier_popup_line with "name = value", set
 *   identifier_popup_active = true, invalidate script pane.
 *
 * If not found: populate identifier_popup_line with "name (not found in locals)",
 *   set identifier_popup_active = true, invalidate script pane.
 *
 * The identifier_popup_active flag causes draw_script_pane to render a
 * one-line status string at the bottom of the script pane's visible area.
 *
 * Exposed via debugger_tui_internal.h for direct testing.
 * ---------------------------------------------------------------------- */

void tui_resolve_identifier_by_name(tui_state_t *s, const char *name) {
	if (s == NULL || name == NULL || name[0] == '\0') return;

	/* Search local_names for a match */
	for (int i = 0; i < s->local_count; i++) {
		if (s->local_names[i] != NULL &&
		    strcmp(s->local_names[i], name) == 0) {
			/* Found: show "name = value" */
			const char *value = (s->local_values && s->local_values[i])
			                    ? s->local_values[i] : "";
			snprintf(s->identifier_popup_line,
			         sizeof(s->identifier_popup_line),
			         "%s = %s", name, value);
			s->identifier_popup_active = true;
			if (s->script_win != NULL) boxen_window_invalidate(s->script_win);
			return;
		}
	}

	/* Not found: show informational message */
	snprintf(s->identifier_popup_line,
	         sizeof(s->identifier_popup_line),
	         "%s (not found in locals)", name);
	s->identifier_popup_active = true;
	if (s->script_win != NULL) boxen_window_invalidate(s->script_win);
}

/* -------------------------------------------------------------------------
 * 2026-06-07 JES Phase B.5 #691: watchpoint modal.
 *
 * Structurally identical to the B.4 condition modal.  The watchpoint modal
 * is opened by pressing 'w' in the stack pane while debug_state is SUSPENDED.
 * It is pre-populated with the local variable name at the current cursor
 * position in the locals section (or the first local if no cursor tracking).
 *
 * On Enter: dispatch debug/setWatchpoint with the typed path and current threadId.
 * On Escape: close without dispatching.
 *
 * Wire format per EXECUTION_PLAN.md B.5:
 *   {"op":"debug/setWatchpoint","id":N,"params":{"path":"varname","threadId":TID}}
 *
 * handle_debug_setwatchpoint is confirmed at debug_handler.h:151.
 *
 * Security: tui_json_escape applied to watch_path_buf before dispatch (same
 * as condition string in B.4 -- B.4 P1 class lesson).
 *
 * tui_close_watchpoint_modal: mirrors tui_close_condition_modal exactly.
 * ---------------------------------------------------------------------- */

static void tui_close_watchpoint_modal(tui_state_t *s) {
	if (s->watchpoint_modal_win == NULL) return;
	/* A.7 API: hide cursor before releasing modal ownership */
	boxen_window_set_cursor_visible(s->watchpoint_modal_win, false);
	/* Release modal gate */
	boxen_window_set_modal(s->watchpoint_modal_win, false);
	boxen_window_close(s->watchpoint_modal_win);
	s->watchpoint_modal_win = NULL;
	/* Restore focus to stack pane (watchpoint modal is always opened from stack pane) */
	if (s->stack_win != NULL) boxen_window_focus(s->stack_win);
}

/* Forward declaration -- draw_watchpoint_modal referenced before definition */
static void draw_watchpoint_modal(boxen_window_t *win, void *ud);

static void input_watchpoint_modal(boxen_window_t *win, const boxen_event_t *ev, void *ud) {
	(void)win;
	tui_state_t *s = (tui_state_t *)ud;
	if (ev->type != BOXEN_EV_KEY) return;

	if (ev->key.key == BOXEN_KEY_ESCAPE) {
		/* Cancel: close without dispatching */
		tui_close_watchpoint_modal(s);
		return;
	}

	if (ev->key.key == BOXEN_KEY_ENTER) {
		/* Confirm: dispatch debug/setWatchpoint with the typed path.
		 * 2026-06-07 JES Phase B.5 #691: tui_json_escape the path string
		 * (B.4 P1 security class: ALL outbound JSON string fields that come from
		 * runtime state must be escaped). */
		char esc_path[TUI_WATCH_PATH_MAX * 6 + 1];
		tui_json_escape(s->watch_path_buf, esc_path, sizeof(esc_path));

		char req[512 + sizeof(esc_path)];
		snprintf(req, sizeof(req),
		         "{\"op\":\"debug/setWatchpoint\",\"id\":%d,"
		         "\"params\":{\"path\":\"%s\",\"threadId\":%ld}}",
		         next_req_id(), esc_path, s->pending_thread_id);

		tui_close_watchpoint_modal(s);
		tui_dispatch_json(s, req);
		return;
	}

	if (ev->key.key == BOXEN_KEY_BACKSPACE) {
		if (s->watch_path_len > 0) {
			s->watch_path_len--;
			s->watch_path_buf[s->watch_path_len] = '\0';
		}
	} else if (ev->key.key == BOXEN_KEY_NONE && ev->key.ch >= 0x20 && ev->key.ch < 0x7F) {
		/* Printable ASCII: append if space permits (capped at TUI_WATCH_PATH_MAX-1) */
		if (s->watch_path_len < TUI_WATCH_PATH_MAX - 1) {
			s->watch_path_buf[s->watch_path_len++] = (char)ev->key.ch;
			s->watch_path_buf[s->watch_path_len]   = '\0';
		}
	}

	/* Reposition cursor to follow the input (A.7 API).
	 * Modal content layout: row 0 = title, row 1 = blank, row 2 = input. */
	if (s->watchpoint_modal_win != NULL) {
		boxen_window_set_cursor(s->watchpoint_modal_win,
		                        s->watch_path_len, 2);
		boxen_window_invalidate(s->watchpoint_modal_win);
	}
}

static void draw_watchpoint_modal(boxen_window_t *win, void *ud) {
	tui_state_t *s = (tui_state_t *)ud;
	int w = boxen_window_content_width(win);
	if (w <= 0) return;

	/* Cap to linebuf capacity (B.1 round-1 P1-A lesson) */
	char linebuf[512];
	int  cap   = (int)sizeof(linebuf) - 1;
	int  avail = w < cap ? w : cap;

	/* Row 0: title */
	{
		const char *title = "Set Watchpoint (Enter=confirm  Esc=cancel)";
		int n = snprintf(linebuf, (size_t)(avail + 1), "%-*.*s", avail, avail, title);
		(void)n;
		boxen_draw_text(win, 0, 0, linebuf,
		                BOXEN_COLOR_DEFAULT, BOXEN_COLOR_DEFAULT, BOXEN_ATTR_BOLD);
	}

	/* Row 1: blank separator */
	{
		memset(linebuf, ' ', (size_t)avail);
		linebuf[avail] = '\0';
		boxen_draw_text(win, 0, 1, linebuf,
		                BOXEN_COLOR_DEFAULT, BOXEN_COLOR_DEFAULT, BOXEN_ATTR_NONE);
	}

	/* Row 2: variable path buffer.
	 * Pad to avail with spaces so the input row is fully drawn. */
	{
		const char *path = s != NULL ? s->watch_path_buf : "";
		int plen = s != NULL ? s->watch_path_len : 0;
		int copy = plen < avail ? plen : avail;
		memcpy(linebuf, path, (size_t)copy);
		memset(linebuf + copy, ' ', (size_t)(avail - copy));
		linebuf[avail] = '\0';
		boxen_draw_text(win, 0, 2, linebuf,
		                BOXEN_COLOR_DEFAULT, BOXEN_COLOR_DEFAULT, BOXEN_ATTR_NONE);
	}

	/* Position the real terminal cursor at the end of the typed text (A.7 API) */
	if (s != NULL && s->watchpoint_modal_win != NULL) {
		boxen_window_set_cursor(s->watchpoint_modal_win,
		                        s->watch_path_len, 2);
	}
}

static void tui_open_watchpoint_modal(tui_state_t *s, const char *suggested,
                                      int tw, int th) {
	/* Pre-populate the path buffer with the suggested local variable name.
	 * If suggested is NULL or empty, the buffer starts blank. */
	memset(s->watch_path_buf, 0, sizeof(s->watch_path_buf));
	s->watch_path_len = 0;
	if (suggested != NULL && suggested[0] != '\0') {
		size_t slen = strlen(suggested);
		if (slen > (size_t)(TUI_WATCH_PATH_MAX - 1)) slen = (size_t)(TUI_WATCH_PATH_MAX - 1);
		memcpy(s->watch_path_buf, suggested, slen);
		s->watch_path_buf[slen] = '\0';
		s->watch_path_len = (int)slen;
	}

	/* Open a centered modal window -- same sizing as the condition modal */
	int mw = 60;
	int mh = 5;
	if (mw > tw - 4) mw = tw - 4;
	if (mh > th - 4) mh = th - 4;
	if (mw < 20) mw = 20;
	if (mh < 5)  mh = 5;

	int mx = (tw - mw) / 2;
	int my = (th - mh) / 2;
	if (mx < 0) mx = 0;
	if (my < 0) my = 0;

	boxen_rect_t r = { mx, my, mw, mh };
	s->watchpoint_modal_win = boxen_window_open("Set Watchpoint", r, NULL);
	if (s->watchpoint_modal_win == NULL) return;

	boxen_window_set_draw(s->watchpoint_modal_win,  draw_watchpoint_modal);
	boxen_window_set_input(s->watchpoint_modal_win, input_watchpoint_modal);
	boxen_window_set_user_data(s->watchpoint_modal_win, s);

	/* A.7 API: modal gate + cursor */
	boxen_window_set_modal(s->watchpoint_modal_win, true);
	boxen_window_set_cursor(s->watchpoint_modal_win, s->watch_path_len, 2);
	boxen_window_set_cursor_visible(s->watchpoint_modal_win, true);

	boxen_window_invalidate(s->watchpoint_modal_win);
}

/* -------------------------------------------------------------------------
 * 2026-06-07 JES Phase B.3 #691: draw_footer.
 *
 * Footer state machine (EXECUTION_PLAN.md B.3 "Footer update"):
 *   TUI_DEBUG_SUSPENDED: full keybind hint
 *   TUI_DEBUG_RUNNING:   "[running...]  q:quit"
 *   TUI_DEBUG_IDLE:      "q:quit"
 * ---------------------------------------------------------------------- */

static void draw_footer(boxen_window_t *win, void *ud) {
	tui_state_t *s = (tui_state_t *)ud;
	int w = boxen_window_content_width(win);
	if (w <= 0) return;

	const char *text;
	if (s != NULL && s->debug_state == TUI_DEBUG_SUSPENDED) {
		/* 2026-06-07 JES Phase B.4 #691: added F9:bp and S-F9:cond */
		text = "F5:continue  F9:bp  S-F9:cond  F10:step-over  F11:step-in  S-F11:step-out  q:quit";
	} else if (s != NULL && s->debug_state == TUI_DEBUG_RUNNING) {
		text = "[running...]  q:quit";
	} else {
		text = "q:quit";
	}

	char buf[256];
	/* Pad/truncate to content width so the bar covers the full row */
	int n = snprintf(buf, sizeof(buf), "%-*.*s", w, w, text);
	(void)n;
	boxen_draw_text(win, 0, 0, buf,
	                BOXEN_COLOR_BLACK, BOXEN_COLOR_WHITE, BOXEN_ATTR_BOLD);
}

/* -------------------------------------------------------------------------
 * 2026-06-06 JES Phase B.0 #691: layout construction.
 * 2026-06-06 JES Phase B.0 #734 round 1 P1-2: renamed to tui_build_layout;
 *   now closes any existing windows before opening new ones so it can be
 *   called on both init and BOXEN_EV_RESIZE without leaking windows.
 *
 * Three-window layout:
 *   +-- Script (60%) --------+-- Stack (40%) --+
 *   | (source; B.1)          | (stack; B.2)    |
 *   +------------------------+-----------------+
 *   q:quit                                       <- pinned footer, 1 row
 *
 * Matches the plan's "three empty windows (script, stack, footer)" spec.
 * ---------------------------------------------------------------------- */

static void tui_build_layout(tui_state_t *s, int tw, int th) {
	/* Close any windows from a previous layout (resize path) */
	if (s->footer_win != NULL) { boxen_window_close(s->footer_win); s->footer_win = NULL; }
	if (s->stack_win  != NULL) { boxen_window_close(s->stack_win);  s->stack_win  = NULL; }
	if (s->script_win != NULL) { boxen_window_close(s->script_win); s->script_win = NULL; }

	/* Reserve one row for the pinned footer */
	int content_h = (th > 2) ? (th - 1) : 1;

	boxen_rect_t full = { 0, 0, tw, content_h };

	/* Horizontal split: script (60%) | stack (40%) */
	boxen_layout_split_h(full, 0.60f,
	                     "Script", &s->script_win,
	                     "Stack",  &s->stack_win);

	/* Pinned footer: full width, 1 row, borderless */
	boxen_rect_t footer_rect = { 0, th - 1, tw, 1 };
	s->footer_win = boxen_window_open("footer", footer_rect, NULL);
	if (s->footer_win != NULL) {
		boxen_window_set_borders(s->footer_win, false);
		boxen_window_set_pinned(s->footer_win, BOXEN_PIN_BOTTOM);
		boxen_window_set_draw(s->footer_win, draw_footer);
	}
}

/* -------------------------------------------------------------------------
 * 2026-06-07 JES Phase B.2 #691: stack pane frame selection.
 *
 * When the stack pane has focus, UP moves the selection toward the outermost
 * frame (lower index) and DOWN moves toward the innermost (higher index).
 * Frames are stored outermost-first (matching debug/getStack order), so:
 *   selected_frame == 0 is the outermost caller
 *   selected_frame == frame_count-1 is the innermost (currently executing)
 *
 * On selection change, update the script pane to show the selected frame's
 * script and line by calling the same cross-pane helper that B.1 introduced
 * (EXECUTION_PLAN.md B.2 Sentinels: "implement it by having the stack pane's
 * input callback call the same load_source function").
 *
 * In B.2 we update script_path and current_line directly from the frame data
 * and invalidate the script pane; the full op_dispatch-based source reload
 * is wired in B.6 once debug_set_attach_transport is connected.
 * ---------------------------------------------------------------------- */

static void tui_select_frame(tui_state_t *s, int new_frame) {
	if (s->frame_count <= 0) return;
	if (new_frame < 0) new_frame = 0;
	if (new_frame >= s->frame_count) new_frame = s->frame_count - 1;

	s->selected_frame = new_frame;

	/* Cross-pane side effect: update script pane to show the selected frame's
	 * script and line (EXECUTION_PLAN.md B.2 Sentinels).
	 * Locals do NOT change -- debug/getLocals always returns the innermost
	 * frame's locals; there is no per-frame locals API.
	 *
	 * 2026-06-07 JES Phase B.2 #739 round 1 P2: if the selected frame belongs
	 * to a different script than what is currently loaded, blank the script pane
	 * so the UI shows empty content rather than wrong-source-with-frame-line.
	 * B.6 wires the live op_dispatch reload; until then, empty is honest. */
	if (strcmp(s->frame_scripts[new_frame], s->script_path) != 0) {
		tui_free_script_lines(s);
		s->script_line_count = 0;
		/* script_path is updated below; the draw callback already renders
		 * the placeholder text gracefully when script_lines is NULL. */
	}

	strncpy(s->script_path, s->frame_scripts[new_frame],
	        sizeof(s->script_path) - 1);
	s->script_path[sizeof(s->script_path) - 1] = '\0';
	s->current_line = s->frame_lines[new_frame];

	if (s->script_win != NULL) {
		if (s->current_line > 0)
			boxen_window_ensure_visible(s->script_win, 0,
			                            (int)(s->current_line - 1));
		boxen_window_invalidate(s->script_win);
	}
	if (s->stack_win != NULL)
		boxen_window_invalidate(s->stack_win);
}

static void on_stack_input(boxen_window_t *win, const boxen_event_t *ev, void *ud) {
	(void)win;
	tui_state_t *s = (tui_state_t *)ud;
	if (ev->type != BOXEN_EV_KEY) return;

	/* Frame navigation: UP moves toward outermost (lower index);
	 * DOWN moves toward innermost (higher index). */
	if (ev->key.key == BOXEN_KEY_UP) {
		tui_select_frame(s, s->selected_frame - 1);
		return;
	}
	if (ev->key.key == BOXEN_KEY_DOWN) {
		tui_select_frame(s, s->selected_frame + 1);
		return;
	}

	/* 2026-06-07 JES Phase B.5 #691: 'w' opens the watchpoint modal.
	 * Only available while suspended and no modal is already open.
	 * Pre-populate with the first local's name if locals are loaded. */
	if ((ev->key.ch == 'w' || ev->key.ch == 'W') &&
	    s->debug_state == TUI_DEBUG_SUSPENDED &&
	    s->watchpoint_modal_win == NULL) {
		/* Derive terminal dimensions from the footer window */
		int tw = 80, th = 24;
		if (s->footer_win != NULL) {
			boxen_rect_t fr = boxen_window_get_rect(s->footer_win);
			tw = fr.w;
			th = fr.y + fr.h;
		}
		/* Suggest the first local variable name as the default path */
		const char *suggested = NULL;
		if (s->local_count > 0 && s->local_names != NULL &&
		    s->local_names[0] != NULL) {
			suggested = s->local_names[0];
		}
		tui_open_watchpoint_modal(s, suggested, tw, th);
		return;
	}

	/* Pass quit keys through to the shared handler */
	if (ev->key.key == BOXEN_KEY_ESCAPE ||
	    ev->key.key == BOXEN_KEY_CTRL_C ||
	    ev->key.ch == 'q' || ev->key.ch == 'Q') {
		s->quit_requested = true;
	}
}

/* -------------------------------------------------------------------------
 * 2026-06-06 JES Phase B.0 #691: input callback (shared by script pane and footer).
 *
 * 'q', Escape, and Ctrl-C set quit_requested. All other events are ignored
 * in B.0; keybinds for debug ops are wired in B.3.
 * ---------------------------------------------------------------------- */

static void on_input(boxen_window_t *win, const boxen_event_t *ev, void *ud) {
	(void)win;
	tui_state_t *s = (tui_state_t *)ud;

	/* 2026-06-07 JES Phase B.5 #691: cmd-double-click identifier resolution.
	 *
	 * Gesture: Meta (Cmd on macOS) + left-button double-click on a word in
	 * the script pane while suspended.
	 *
	 * Detection (EXECUTION_PLAN.md B.5 Sentinels):
	 *   - ev->type == BOXEN_EV_MOUSE
	 *   - ev->mouse.button == 1 (left button)
	 *   - ev->mouse.mod & BOXEN_MOD_META
	 *   - ev->mouse.flags & BOXEN_MOUSE_DOUBLE_CLICK
	 *   - ev->mouse.pressed == true (second press event, not release)
	 *
	 * Only fires while TUI_DEBUG_SUSPENDED (locals are available for lookup).
	 *
	 * Identifier extraction: use boxen_window_at to get content coordinates
	 * from screen coordinates.  Check for BOXEN_HIT_CHROME (INT_MIN sentinel)
	 * before treating cx/cy as row/column content indices.
	 *
	 * Row conversion: content row cy corresponds to source line
	 *   (cy + scroll_y + 1) in 1-based ODB line numbers.
	 * Column cx into the content area: column 0 is the gutter; source text
	 *   starts at column 1.  For identifier extraction, pass (cx - 1) as the
	 *   column into the source text line (cx == 0 is the gutter, no identifier).
	 */
	if (ev->type == BOXEN_EV_MOUSE) {
		if (ev->mouse.button == 1 &&
		    (ev->mouse.mod   & BOXEN_MOD_META) &&
		    (ev->mouse.flags & BOXEN_MOUSE_DOUBLE_CLICK) &&
		    ev->mouse.pressed == true &&
		    s->debug_state == TUI_DEBUG_SUSPENDED) {
			/* Map screen coords to content coords in the script window */
			int cx = 0, cy = 0;
			boxen_window_t *hit_win = boxen_window_at(ev->mouse.x, ev->mouse.y, &cx, &cy);

			if (hit_win == s->script_win &&
			    cx != BOXEN_HIT_CHROME && cy != BOXEN_HIT_CHROME &&
			    cx > 0 /* not gutter */) {
				/* Content column (cx-1) is the 0-based column in the source text.
				 * Content row cy + scroll_y gives the 0-based source line index. */
				int scroll_x = 0, scroll_y = 0;
				boxen_window_get_scroll(s->script_win, &scroll_x, &scroll_y);
				int source_row = cy + scroll_y;   /* 0-based line index */
				int source_col = cx - 1;          /* 0-based column in text */

				if (source_row >= 0 && source_row < s->script_line_count) {
					const char *line = s->script_lines[source_row];
					if (line != NULL) {
						char ident[256];
						extract_identifier_at(line, source_col, ident, (int)sizeof(ident));
						if (ident[0] != '\0') {
							tui_resolve_identifier_by_name(s, ident);
						}
					}
				}
			}
		}
		return;   /* Mouse events do not fall through to key handler */
	}

	if (ev->type != BOXEN_EV_KEY) return;

	/* 2026-06-06 JES Phase B.0 #734 round 1 P1-4: accept q/Q regardless of
	 * ev->key.key value. Real termbox2 may set key != BOXEN_KEY_NONE for
	 * printable chars on some terminals; the BOXEN_KEY_NONE guard was too
	 * strict and would have silently ignored quit on those backends. */
	if (ev->key.key == BOXEN_KEY_ESCAPE ||
	    ev->key.key == BOXEN_KEY_CTRL_C ||
	    ev->key.ch == 'q' || ev->key.ch == 'Q') {
		s->quit_requested = true;
		return;
	}

	/* 2026-06-07 JES Phase B.3 #691: F-key debug controls.
	 *
	 * Only dispatch when the thread is suspended.  Pressing step/continue
	 * while TUI_DEBUG_RUNNING is silently ignored -- the runtime is running
	 * and cannot accept another step/continue until the thread suspends again.
	 *
	 * Key table (EXECUTION_PLAN.md B.3 "Keybind table"):
	 *   F5           -> debug/continue
	 *   F10          -> debug/step direction:over
	 *   F11 (no mod) -> debug/step direction:into
	 *   F11+Shift    -> debug/step direction:out
	 *
	 * Re-entrancy: setting debug_state = TUI_DEBUG_RUNNING inside tui_do_*
	 * before the dispatch call means a second key event arriving during the
	 * same tick (impossible in practice, but safe by design) will hit the
	 * SUSPENDED guard and be dropped.
	 */
	if (s->debug_state == TUI_DEBUG_SUSPENDED) {
		if (ev->key.key == BOXEN_KEY_F5) {
			tui_do_continue(s);
			if (s->footer_win != NULL) boxen_window_invalidate(s->footer_win);
			return;
		}
		if (ev->key.key == BOXEN_KEY_F9) {
			/* 2026-06-07 JES Phase B.4 #691: breakpoint toggle.
			 *
			 * F9 (no shift): toggle breakpoint at current line.
			 * Shift-F9:      open conditional breakpoint modal.
			 *
			 * Guard: current_line must be > 0 (thread must be suspended in
			 * a specific line) and script_path must be non-empty. */
			if (ev->key.mod & BOXEN_MOD_SHIFT) {
				/* Open condition modal.  Derive terminal dimensions from the
				 * footer window (footer is always full-width, placed at row th-1).
				 * 2026-06-07 JES Phase B.4 #743 round 1: also guard script_path
				 * (mirrors the bare-F9 guard; modal should not open without a path). */
				if (s->condition_modal_win == NULL && s->current_line > 0 &&
				    s->script_path[0] != '\0') {
					int tw = 80, th = 24;
					if (s->footer_win != NULL) {
						boxen_rect_t fr = boxen_window_get_rect(s->footer_win);
						tw = fr.w;
						th = fr.y + fr.h;
					}
					tui_open_condition_modal(s, tw, th);
				}
			} else {
				if (s->current_line > 0 && s->script_path[0] != '\0') {
					tui_toggle_breakpoint(s, s->current_line);
				}
			}
			return;
		}
		if (ev->key.key == BOXEN_KEY_F10) {
			tui_do_step_over(s);
			if (s->footer_win != NULL) boxen_window_invalidate(s->footer_win);
			return;
		}
		if (ev->key.key == BOXEN_KEY_F11) {
			if (ev->key.mod & BOXEN_MOD_SHIFT) {
				tui_do_step_out(s);
			} else {
				tui_do_step_into(s);
			}
			if (s->footer_win != NULL) boxen_window_invalidate(s->footer_win);
			return;
		}
	}
}

/* -------------------------------------------------------------------------
 * 2026-06-06 JES Phase B.0 #691: exported for unit tests (internal API).
 * ---------------------------------------------------------------------- */

/* -------------------------------------------------------------------------
 * tui_wire_callbacks -- wire draw/input callbacks after every layout build.
 *
 * Called from debugger_tui_state_init and from the resize path in
 * debugger_tui_run_one_tick. Separated so tui_build_layout stays a pure
 * window-geometry function (no callback wiring) and both call sites use the
 * same wiring logic.
 * ---------------------------------------------------------------------- */
static void tui_wire_callbacks(tui_state_t *s) {
	if (s->script_win != NULL) {
		/* 2026-06-07 JES Phase B.1 #691: real draw callback replaces placeholder */
		boxen_window_set_draw(s->script_win,  draw_script_pane);
		boxen_window_set_input(s->script_win, on_input);
		boxen_window_set_user_data(s->script_win, s);
	}
	if (s->stack_win != NULL) {
		/* 2026-06-07 JES Phase B.2 #691: real stack/locals draw + frame selection input */
		boxen_window_set_draw(s->stack_win,  draw_stack_pane);
		boxen_window_set_input(s->stack_win, on_stack_input);
		boxen_window_set_user_data(s->stack_win, s);
	}
	/* footer: draw already set in tui_build_layout; wire user_data for
	 * B.3's draw_footer to access debug_state. */
	if (s->footer_win != NULL) {
		boxen_window_set_user_data(s->footer_win, s);
	}

	/* Focus the script pane */
	if (s->script_win != NULL) {
		boxen_window_focus(s->script_win);
	}
}

void debugger_tui_state_init(tui_state_t *s, int tw, int th) {
	memset(s, 0, sizeof(tui_state_t));

	/* 2026-06-07 JES Phase B.1 #691: initialize source state sentinels */
	s->current_line    = -1;  /* -1 = not suspended */
	s->pending_thread_id = -1;

	/* 2026-06-07 JES Phase B.3 #691: initial debug state */
	s->debug_state = TUI_DEBUG_IDLE;
	/* dispatch_capture_buf is NULL by default (production mode);
	 * tests set it explicitly after init. */

	/* 2026-06-07 JES Phase B.4 #691: initialize condition modal state */
	s->condition_modal_win = NULL;
	s->bp_condition_len    = 0;
	/* dispatch_log is NULL by default; tests set it for sequence verification */
	s->dispatch_log        = NULL;
	s->dispatch_log_cap    = 0;
	s->dispatch_log_count  = 0;

	/* 2026-06-07 JES Phase B.5 #691: initialize watchpoint modal + popup state */
	s->watchpoint_modal_win     = NULL;
	s->watch_path_len           = 0;
	s->identifier_popup_active  = false;
	s->identifier_popup_line[0] = '\0';

	/* Allocate heap transport (stub; B.6 wires debug_set_attach_transport) */
	s->transport = calloc(1, sizeof(transport_t));
	if (s->transport != NULL) {
		/* TODO(B.6): ctx points at stack-allocated tui_state_t.  Once B.6
		 * calls debug_set_attach_transport() to register this transport with
		 * the debug runtime, in-flight write_line callbacks can arrive from
		 * callScript threads after teardown begins.  B.6 MUST add a
		 * drain-before-free sequence (debug_wait_lazy_threads_drained() +
		 * debug_set_attach_transport(NULL)) before free(s->transport) in
		 * debugger_tui_state_teardown(), mirroring protocol_handler.c:411-421.
		 * Gated on B.6 per /auto Phase 7 P2 deferral (PR #737 round 1). */
		s->transport->ctx        = s;
		s->transport->write_line = tui_write_line;
	}

	tui_build_layout(s, tw, th);
	tui_wire_callbacks(s);
}

void debugger_tui_state_teardown(tui_state_t *s) {
	/* 2026-06-07 JES Phase B.4 #691: close condition modal if open.
	 * tui_close_condition_modal hides cursor + releases modal gate first. */
	if (s->condition_modal_win != NULL) {
		tui_close_condition_modal(s);
	}
	/* 2026-06-07 JES Phase B.5 #691: close watchpoint modal if open. */
	if (s->watchpoint_modal_win != NULL) {
		tui_close_watchpoint_modal(s);
	}
	if (s->footer_win != NULL)  { boxen_window_close(s->footer_win);  s->footer_win  = NULL; }
	if (s->stack_win  != NULL)  { boxen_window_close(s->stack_win);   s->stack_win   = NULL; }
	if (s->script_win != NULL)  { boxen_window_close(s->script_win);  s->script_win  = NULL; }
	if (s->transport  != NULL)  { free(s->transport);                 s->transport   = NULL; }
	/* 2026-06-07 JES Phase B.4 #743 round 1: free heap condition strings */
	for (int i = 0; i < s->bp_line_count; i++) {
		free(s->bp_conditions[i]);
		s->bp_conditions[i] = NULL;
	}
	s->bp_line_count = 0;
	/* 2026-06-07 JES Phase B.1 #691: free source lines */
	tui_free_script_lines(s);
	/* 2026-06-07 JES Phase B.2 #691: free locals */
	tui_free_locals(s);
	/* 2026-06-07 JES Phase B.3 #691: clear dispatch capture (not heap; just NULL) */
	s->dispatch_capture_buf = NULL;
	s->dispatch_capture_cap = 0;
	/* 2026-06-07 JES Phase B.4 #691: clear dispatch log (not heap; just NULL) */
	s->dispatch_log       = NULL;
	s->dispatch_log_cap   = 0;
	s->dispatch_log_count = 0;
	/* 2026-06-07 JES Phase B.5 #691: clear watchpoint + popup state */
	s->watch_path_len           = 0;
	s->identifier_popup_active  = false;
	s->identifier_popup_line[0] = '\0';
	s->debug_state = TUI_DEBUG_IDLE;
}

int debugger_tui_run_one_tick(tui_state_t *s, const boxen_event_t *ev) {
	/* 2026-06-06 JES Phase B.0 #734 round 1 P1-2: handle BOXEN_EV_RESIZE.
	 * Tear down and rebuild the layout at the new dimensions, then re-wire
	 * callbacks and re-focus. boxen_dispatch_event is NOT called for resize
	 * because the layout rebuild already repositions all windows; dispatching
	 * would deliver the event to the (now-closed) old focused window. */
	if (ev->type == BOXEN_EV_RESIZE) {
		int nw = (ev->resize.w > 0) ? ev->resize.w : 80;
		int nh = (ev->resize.h > 0) ? ev->resize.h : 24;
		tui_build_layout(s, nw, nh);
		tui_wire_callbacks(s);
		return TUI_CONTINUE;
	}

	boxen_dispatch_event(ev);
	if (s->quit_requested) {
		return TUI_QUIT;
	}
	return TUI_CONTINUE;
}

/* -------------------------------------------------------------------------
 * 2026-06-06 JES Phase B.0 #691: public entry point.
 *
 * GIL yield pattern mirrors protocol_handler.c:348-354.
 * Teardown snapshot/restore mirrors protocol_handler.c:411-421.
 *
 * NOTE: B.0 does NOT call debug_set_attach_transport(). That call is added
 * in B.6 when the debug runtime is wired. The transport is allocated and
 * freed here so the heap-lifetime pattern is established from the start,
 * but the global attach is deferred per EXECUTION_PLAN.md B.0 "Out of scope".
 *
 * GIL symbol isolation:
 *   headless_threading.h is included here (inside a #ifndef guard) rather
 *   than at file scope. This keeps the test build (which compiles only
 *   debugger_tui.c without the full Frontier runtime) free of undefined GIL
 *   symbols. The test binary's undefined symbol table must not contain
 *   frontier_gil / hthreadglobals; dyld aborts at load time even if those
 *   symbols are unreachable at runtime (flat-namespace lookup failure).
 * ---------------------------------------------------------------------- */

#ifndef DEBUGGER_TUI_OMIT_MAIN

/* These includes are intentionally at function scope (not file scope) to
 * prevent the GIL and hglobals symbols from appearing in the test binary's
 * undefined symbol table. Including them here means the compiler still
 * type-checks the code; the symbols just don't appear as undefined refs in
 * the object file unless this translation unit is compiled without
 * DEBUGGER_TUI_OMIT_MAIN. */
#include "headless_threading.h"
#include <pthread.h>

int debugger_tui_main(const cli_options_t *opts) {
	(void)opts;

	/* 2026-06-06 JES Phase B.0 #734 round 1 P1-1: boxen_init MUST be called
	 * BEFORE querying terminal dimensions. termbox2's tb_width()/tb_height()
	 * return TB_ERR_NOT_INIT (negative) when called before tb_init(); the
	 * original > 0 guard silently fell back to 80x24 on every terminal. */
	const boxen_backend_t *be = boxen_tb2_backend();
	boxen_result_t rc = boxen_init(be, NULL, NULL);
	if (rc != BOXEN_OK) {
		log_error(LOG_COMP_GENERAL, "debugger_tui: boxen_init failed: %s",
		          boxen_last_error_str());
		return 1;
	}

	/* Query terminal dimensions after init -- backend is now running */
	int tw = (be->width  && be->width()  > 0) ? be->width()  : 80;
	int th = (be->height && be->height() > 0) ? be->height() : 24;

	tui_state_t state;
	debugger_tui_state_init(&state, tw, th);

	log_info(LOG_COMP_GENERAL, "Debugger TUI: entering event loop (q/Esc/Ctrl-C to quit)");

	/* Event loop: release GIL around each poll so background threads can run */
	while (!state.quit_requested) {
		boxen_event_t ev;
		memset(&ev, 0, sizeof(ev));

		/* 2026-06-06 JES Phase B.0 #691: GIL yield pattern.
		 * Snapshot main-thread globals before yielding (lazy callScript threads
		 * can overwrite hthreadglobals during the poll). Restore after.
		 * Pattern: protocol_handler.c:348-354. */
		hdlthreadglobals main_globals = hthreadglobals;
		headless_save_threadglobals(main_globals);
		pthread_mutex_unlock(&frontier_gil);
		boxen_result_t poll_rc = boxen_poll_event(&ev, 100 /* ms timeout */);
		pthread_mutex_lock(&frontier_gil);
		headless_restore_threadglobals(main_globals);

		if (poll_rc == BOXEN_ERR_TIMEOUT) {
			/* No event this tick; still call present to handle redraws */
			boxen_present();
			continue;
		}
		if (poll_rc != BOXEN_OK) {
			/* Persistent I/O error -- exit cleanly */
			log_warn(LOG_COMP_GENERAL, "debugger_tui: poll error, exiting");
			break;
		}

		debugger_tui_run_one_tick(&state, &ev);
		boxen_present();
	}

	/* 2026-06-06 JES Phase B.0 #691: teardown.
	 * B.6 inserts debug_wait_lazy_threads_drained() + debug_set_attach_transport(NULL)
	 * here per protocol_handler.c:411-421. B.0 skips those calls because no
	 * debug_set_attach_transport() was made on entry. */
	debugger_tui_state_teardown(&state);
	boxen_shutdown();

	log_info(LOG_COMP_GENERAL, "Debugger TUI: exited cleanly");
	return 0;
}

#endif /* !DEBUGGER_TUI_OMIT_MAIN */
