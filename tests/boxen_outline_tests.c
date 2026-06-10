/*
 * boxen_outline_tests.c -- behavioral unit tests for the C.1 boxen outline editor.
 *
 * Uses the boxen mock backend (no real terminal) and hook seams to drive
 * the editor without the live ODB or GIL.
 *
 * Test harness: test_report.h TR_RUN/TR_SUMMARY/TR_EXIT_CODE (same as all
 * other Frontier unit tests).
 *
 * Pattern mirrors boxen_repl_tests.c exactly -- mock backend, log_write stub,
 * setup/teardown helpers, behavioral assertions using assert().
 *
 * Seven minimum tests (as specified in the C.1 task brief):
 *
 *   1. test_open_builds_tree_from_canned_outline
 *   2. test_bar_cursor_down_advances_to_next_visible
 *   3. test_keypad_minus_collapses_current
 *   4. test_keypad_star_expands_all_descendants
 *   5. test_f9_toggles_flbreakpoint
 *   6. test_cmd_slash_toggles_flcomment
 *   7. test_checkbox_attribute_renders_box
 *
 * 2026-06-09 JES Phase C.1 #691
 */

#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

/* boxen substrate */
#include "../frontier-cli/boxen/boxen.h"
#include "../frontier-cli/boxen/backend_mock.h"

/* Outline editor under test.
 * BOXEN_OUTLINE_OMIT_MAIN is passed via -D by the Makefile; it prevents
 * production hook implementations (which reference GIL and ODB symbols)
 * from being compiled. */
#include "../frontier-cli/boxen_outline_internal.h"

/* Test harness */
#include "test_report.h"

/* -------------------------------------------------------------------------
 * log_write stub -- same pattern as boxen_repl_tests.c / debugger_tui_tests.c
 * ---------------------------------------------------------------------- */
#include "../Common/headers/logging.h"
void log_write(log_level_t level, log_component_t component,
               const char *file, int line, const char *fmt, ...) {
	(void)level; (void)component; (void)file; (void)line; (void)fmt;
	/* No-op in test builds. */
}

/* -------------------------------------------------------------------------
 * Canned tree definition
 *
 * Three-level tree:
 *   root (expanded, has children)        -- level 0
 *     child_a (expanded, has children)   -- level 1
 *       leaf_aa (leaf)                   -- level 2
 *       leaf_ab (leaf)                   -- level 2
 *     child_b (collapsed, has children)  -- level 1
 *       leaf_ba (leaf)                   -- level 2 (hidden while collapsed)
 *
 * Visible with child_b collapsed: root, child_a, leaf_aa, leaf_ab, child_b (5)
 * Visible with all expanded:      all 6 nodes
 * ---------------------------------------------------------------------- */

/* Identifiers into g_canned_nodes[] */
#define IDX_ROOT      0
#define IDX_CHILD_A   1
#define IDX_LEAF_AA   2
#define IDX_LEAF_AB   3
#define IDX_CHILD_B   4
#define IDX_LEAF_BA   5
#define CANNED_TOTAL  6

/* The canonical canned node array.  The fetch hook copies from here.
 * file-static so test functions can reference it directly. */
static boxen_outline_node_t g_canned_nodes[CANNED_TOTAL];

static void init_canned_nodes(void) {
	memset(g_canned_nodes, 0, sizeof(g_canned_nodes));

	/* root */
	snprintf(g_canned_nodes[IDX_ROOT].text,
	         BOXEN_OUTLINE_TEXT_MAX, "root");
	g_canned_nodes[IDX_ROOT].level        = 0;
	g_canned_nodes[IDX_ROOT].has_children = true;
	g_canned_nodes[IDX_ROOT].expanded     = true;
	g_canned_nodes[IDX_ROOT].marker       = BOXEN_OUTLINE_MARKER_EXPANDED;

	/* child_a */
	snprintf(g_canned_nodes[IDX_CHILD_A].text,
	         BOXEN_OUTLINE_TEXT_MAX, "child_a");
	g_canned_nodes[IDX_CHILD_A].level        = 1;
	g_canned_nodes[IDX_CHILD_A].has_children = true;
	g_canned_nodes[IDX_CHILD_A].expanded     = true;
	g_canned_nodes[IDX_CHILD_A].marker       = BOXEN_OUTLINE_MARKER_EXPANDED;

	/* leaf_aa */
	snprintf(g_canned_nodes[IDX_LEAF_AA].text,
	         BOXEN_OUTLINE_TEXT_MAX, "leaf_aa");
	g_canned_nodes[IDX_LEAF_AA].level        = 2;
	g_canned_nodes[IDX_LEAF_AA].has_children = false;
	g_canned_nodes[IDX_LEAF_AA].expanded     = false;
	g_canned_nodes[IDX_LEAF_AA].marker       = BOXEN_OUTLINE_MARKER_LEAF;

	/* leaf_ab */
	snprintf(g_canned_nodes[IDX_LEAF_AB].text,
	         BOXEN_OUTLINE_TEXT_MAX, "leaf_ab");
	g_canned_nodes[IDX_LEAF_AB].level        = 2;
	g_canned_nodes[IDX_LEAF_AB].has_children = false;
	g_canned_nodes[IDX_LEAF_AB].expanded     = false;
	g_canned_nodes[IDX_LEAF_AB].marker       = BOXEN_OUTLINE_MARKER_LEAF;

	/* child_b -- collapsed; leaf_ba not initially visible */
	snprintf(g_canned_nodes[IDX_CHILD_B].text,
	         BOXEN_OUTLINE_TEXT_MAX, "child_b");
	g_canned_nodes[IDX_CHILD_B].level        = 1;
	g_canned_nodes[IDX_CHILD_B].has_children = true;
	g_canned_nodes[IDX_CHILD_B].expanded     = false;
	g_canned_nodes[IDX_CHILD_B].marker       = BOXEN_OUTLINE_MARKER_COLLAPSED;

	/* leaf_ba (child of child_b) */
	snprintf(g_canned_nodes[IDX_LEAF_BA].text,
	         BOXEN_OUTLINE_TEXT_MAX, "leaf_ba");
	g_canned_nodes[IDX_LEAF_BA].level        = 2;
	g_canned_nodes[IDX_LEAF_BA].has_children = false;
	g_canned_nodes[IDX_LEAF_BA].expanded     = false;
	g_canned_nodes[IDX_LEAF_BA].marker       = BOXEN_OUTLINE_MARKER_LEAF;
}

/* -------------------------------------------------------------------------
 * Test fetch hook
 *
 * Returns the visible subset of g_canned_nodes[].
 * "Visible" means: a node is included unless it is a descendant of a
 * collapsed ancestor.  Level tracking is used to skip hidden nodes.
 * ---------------------------------------------------------------------- */
/* 2026-06-09 JES #691 C.1 round 2 P0-2: fetch hook signature updated to
 * (state, path, nodes, count) so production hook can stash houtline_opaque. */
static bool hook_fetch(boxen_outline_state_t *s,
                       const char *path,
                       boxen_outline_node_t *nodes,
                       int *node_count) {
	(void)s;
	(void)path;

	int out = 0;
	/* collapsed_at: if >= 0, we are inside a collapsed subtree at this level */
	int collapsed_at = -1;

	for (int i = 0; i < CANNED_TOTAL && out < BOXEN_OUTLINE_MAX_NODES; i++) {
		boxen_outline_node_t *src = &g_canned_nodes[i];

		/* Skip descendants of collapsed nodes */
		if (collapsed_at >= 0 && src->level > collapsed_at) {
			continue;
		}
		collapsed_at = -1;

		nodes[out] = *src;
		out++;

		/* If this node has children but is collapsed, mark collapsed_at */
		if (src->has_children && !src->expanded) {
			collapsed_at = src->level;
		}
	}

	*node_count = out;
	return true;
}

/* -------------------------------------------------------------------------
 * Checkbox-specific fetch hook (for test 7)
 *
 * Returns three nodes: checked, unchecked, and plain leaf.
 * ---------------------------------------------------------------------- */
static boxen_outline_node_t g_checkbox_nodes[3];
static bool g_checkbox_nodes_inited = false;

/* 2026-06-09 JES #691 C.1 round 2 P0-2: checkbox fetch hook updated to new
 * (state, path, nodes, count) signature. */
static bool hook_fetch_checkbox(boxen_outline_state_t *s,
                                const char *path,
                                boxen_outline_node_t *nodes,
                                int *node_count) {
	(void)s;
	(void)path;

	if (!g_checkbox_nodes_inited) {
		memset(g_checkbox_nodes, 0, sizeof(g_checkbox_nodes));

		snprintf(g_checkbox_nodes[0].text, BOXEN_OUTLINE_TEXT_MAX,
		         "checked item");
		g_checkbox_nodes[0].marker = BOXEN_OUTLINE_MARKER_CHECKED;

		snprintf(g_checkbox_nodes[1].text, BOXEN_OUTLINE_TEXT_MAX,
		         "unchecked item");
		g_checkbox_nodes[1].marker = BOXEN_OUTLINE_MARKER_UNCHECKED;

		snprintf(g_checkbox_nodes[2].text, BOXEN_OUTLINE_TEXT_MAX,
		         "plain item");
		g_checkbox_nodes[2].marker = BOXEN_OUTLINE_MARKER_LEAF;

		g_checkbox_nodes_inited = true;
	}

	nodes[0] = g_checkbox_nodes[0];
	nodes[1] = g_checkbox_nodes[1];
	nodes[2] = g_checkbox_nodes[2];
	*node_count = 3;
	return true;
}

/* -------------------------------------------------------------------------
 * Mock toggle hooks
 * ---------------------------------------------------------------------- */
/* 2026-06-09 JES #691 C.1 round 2 P0-2: toggle hooks updated to new
 * (state, node_idx) signature to give production hook access to
 * houtline_opaque for the oppushoutline/opdirtyoutline/oppopoutline pattern.
 * Test implementation: directly toggles the node field in s->nodes[]. */
static bool hook_toggle_breakpoint(boxen_outline_state_t *s, int node_idx) {
	if (s == NULL || node_idx < 0 || node_idx >= s->node_count) return false;
	s->nodes[node_idx].flbreakpoint = !s->nodes[node_idx].flbreakpoint;
	return s->nodes[node_idx].flbreakpoint;
}

static bool hook_toggle_comment(boxen_outline_state_t *s, int node_idx) {
	if (s == NULL || node_idx < 0 || node_idx >= s->node_count) return false;
	s->nodes[node_idx].flcomment = !s->nodes[node_idx].flcomment;
	return s->nodes[node_idx].flcomment;
}

/* -------------------------------------------------------------------------
 * Mock script_run hook
 * ---------------------------------------------------------------------- */
static int g_script_run_count = 0;
static char g_script_run_path[256];

static void hook_script_run(const char *path) {
	g_script_run_count++;
	snprintf(g_script_run_path, sizeof(g_script_run_path), "%s", path);
}

/* -------------------------------------------------------------------------
 * Mock expand hook
 *
 * Updates g_canned_nodes[] by matching text+level so the next hook_fetch
 * call picks up the new expanded state.
 * ---------------------------------------------------------------------- */
static void hook_expand(boxen_outline_node_t *node, bool expand) {
	node->expanded = expand;
	node->marker   = expand ? BOXEN_OUTLINE_MARKER_EXPANDED
	                        : BOXEN_OUTLINE_MARKER_COLLAPSED;

	/* Propagate to the authoritative g_canned_nodes[] by matching text+level */
	for (int i = 0; i < CANNED_TOTAL; i++) {
		if (g_canned_nodes[i].level == node->level &&
		    strcmp(g_canned_nodes[i].text, node->text) == 0) {
			g_canned_nodes[i].expanded = expand;
			g_canned_nodes[i].marker   = node->marker;
			break;
		}
	}
}

/* -------------------------------------------------------------------------
 * Mock expand-all hook helper
 *
 * When Keypad-* fires, the editor calls expand_hook on every node in the
 * subtree.  The test hook_expand above updates g_canned_nodes[] correctly.
 * ---------------------------------------------------------------------- */

/* -------------------------------------------------------------------------
 * Test setup/teardown
 * ---------------------------------------------------------------------- */

#define TEST_WIDTH  80
#define TEST_HEIGHT 24

static boxen_outline_state_t g_state;

static void wire_standard_hooks(boxen_outline_state_t *s) {
	s->outline_fetch_hook     = hook_fetch;
	s->toggle_breakpoint_hook = hook_toggle_breakpoint;
	s->toggle_comment_hook    = hook_toggle_comment;
	s->script_run_hook        = hook_script_run;
	s->expand_hook            = hook_expand;
}

static void setup(void) {
	init_canned_nodes();

	g_script_run_count   = 0;
	g_script_run_path[0] = '\0';

	boxen_mock_reset(TEST_WIDTH, TEST_HEIGHT);
	boxen_init(boxen_mock_backend(), NULL, NULL);

	boxen_outline_state_init(&g_state, "test.outline", TEST_WIDTH, TEST_HEIGHT);
	wire_standard_hooks(&g_state);

	/* Load the canned outline into the node array */
	boxen_outline_refresh(&g_state);
}

static void teardown(void) {
	boxen_outline_state_teardown(&g_state);
	boxen_shutdown();
}

/* -------------------------------------------------------------------------
 * Helper: make key / char events
 * ---------------------------------------------------------------------- */
static boxen_event_t make_key(boxen_key_t key, uint16_t mod) {
	boxen_event_t ev;
	memset(&ev, 0, sizeof(ev));
	ev.type    = BOXEN_EV_KEY;
	ev.key.key = key;
	ev.key.mod = mod;
	return ev;
}

static boxen_event_t make_char(uint32_t ch, uint16_t mod) {
	boxen_event_t ev;
	memset(&ev, 0, sizeof(ev));
	ev.type    = BOXEN_EV_KEY;
	ev.key.key = BOXEN_KEY_NONE;
	ev.key.ch  = ch;
	ev.key.mod = mod;
	return ev;
}

/* -------------------------------------------------------------------------
 * TEST 1: test_open_builds_tree_from_canned_outline
 *
 * After setup() the editor has loaded the visible subset of the canned tree.
 * With child_b collapsed, 5 nodes are visible.
 * ---------------------------------------------------------------------- */
static void test_open_builds_tree_from_canned_outline(void) {
	setup();

	/* 5 visible: root, child_a, leaf_aa, leaf_ab, child_b */
	assert(g_state.node_count == 5);
	assert(strcmp(g_state.nodes[0].text, "root") == 0);
	assert(g_state.nodes[0].level == 0);
	assert(g_state.nodes[0].has_children);
	assert(strcmp(g_state.nodes[4].text, "child_b") == 0);
	assert(g_state.nodes[4].marker == BOXEN_OUTLINE_MARKER_COLLAPSED);
	assert(g_state.cursor == 0);

	teardown();
}

/* -------------------------------------------------------------------------
 * TEST 2: test_bar_cursor_down_advances_to_next_visible
 *
 * Simulate Down arrow presses; cursor should advance through visible nodes
 * and clamp at the last one.
 * ---------------------------------------------------------------------- */
static void test_bar_cursor_down_advances_to_next_visible(void) {
	setup();

	assert(g_state.cursor == 0);

	boxen_event_t ev = make_key(BOXEN_KEY_DOWN, BOXEN_MOD_NONE);

	boxen_outline_handle_key(&g_state, &ev);
	assert(g_state.cursor == 1);

	boxen_outline_handle_key(&g_state, &ev);
	assert(g_state.cursor == 2);

	boxen_outline_handle_key(&g_state, &ev);
	boxen_outline_handle_key(&g_state, &ev);
	assert(g_state.cursor == 4); /* last node */

	/* Down at end: clamp */
	boxen_outline_handle_key(&g_state, &ev);
	assert(g_state.cursor == 4);

	teardown();
}

/* -------------------------------------------------------------------------
 * TEST 3: test_keypad_minus_collapses_current
 *
 * Move cursor to child_a (index 1), press '-' (keypad minus).
 * After collapse and re-fetch, node_count drops to 3 and the next Down
 * lands on child_b (the new index 2).
 * ---------------------------------------------------------------------- */
static void test_keypad_minus_collapses_current(void) {
	setup();

	/* Move to child_a */
	boxen_event_t down = make_key(BOXEN_KEY_DOWN, BOXEN_MOD_NONE);
	boxen_outline_handle_key(&g_state, &down);
	assert(g_state.cursor == 1);
	assert(strcmp(g_state.nodes[1].text, "child_a") == 0);

	/* Keypad-minus: collapse current.
	 * The editor maps plain '-' char (no modifier) to "collapse current node"
	 * when the current node has children.
	 *
	 * 2026-06-09 JES #691 C.1 round 2 P1: the handler now calls
	 * boxen_outline_refresh itself after collapsing (test-fits-implementation
	 * antipattern fixed).  No explicit refresh call needed here. */
	boxen_event_t minus = make_char('-', BOXEN_MOD_NONE);
	boxen_outline_handle_key(&g_state, &minus);

	/* Now: root, child_a (collapsed), child_b -> 3 visible */
	assert(g_state.node_count == 3);
	assert(g_state.nodes[1].marker == BOXEN_OUTLINE_MARKER_COLLAPSED);

	/* Down from child_a -> child_b at index 2 */
	boxen_outline_handle_key(&g_state, &down);
	assert(g_state.cursor == 2);
	assert(strcmp(g_state.nodes[2].text, "child_b") == 0);

	teardown();
}

/* -------------------------------------------------------------------------
 * TEST 4: test_keypad_star_expands_all_descendants
 *
 * Cursor at root (index 0), press '*' (keypad star).
 * All nodes expand; node_count rises to 6.
 * ---------------------------------------------------------------------- */
static void test_keypad_star_expands_all_descendants(void) {
	setup();

	assert(g_state.node_count == 5); /* sanity: child_b collapsed */

	/* Keypad-star: expand all descendants of current node.
	 *
	 * 2026-06-09 JES #691 C.1 round 2 P1: the handler now calls
	 * boxen_outline_refresh itself after expand-all (test-fits-implementation
	 * antipattern fixed).  No explicit refresh call needed here. */
	boxen_event_t star = make_char('*', BOXEN_MOD_NONE);
	boxen_outline_handle_key(&g_state, &star);

	/* All 6 nodes now visible */
	assert(g_state.node_count == 6);
	assert(strcmp(g_state.nodes[5].text, "leaf_ba") == 0);

	/* Verify child_b is now expanded in the display list */
	bool child_b_expanded = false;
	for (int i = 0; i < g_state.node_count; i++) {
		if (strcmp(g_state.nodes[i].text, "child_b") == 0) {
			child_b_expanded = g_state.nodes[i].expanded;
			break;
		}
	}
	assert(child_b_expanded);

	teardown();
}

/* -------------------------------------------------------------------------
 * TEST 5: test_f9_toggles_flbreakpoint
 *
 * Bar cursor on root.  F9 -> flbreakpoint true.  F9 again -> false.
 * ---------------------------------------------------------------------- */
static void test_f9_toggles_flbreakpoint(void) {
	setup();

	assert(g_state.cursor == 0);
	assert(!g_state.nodes[0].flbreakpoint);

	boxen_event_t f9 = make_key(BOXEN_KEY_F9, BOXEN_MOD_NONE);
	boxen_outline_handle_key(&g_state, &f9);

	assert(g_state.nodes[0].flbreakpoint);

	boxen_outline_handle_key(&g_state, &f9);

	assert(!g_state.nodes[0].flbreakpoint);

	teardown();
}

/* -------------------------------------------------------------------------
 * TEST 6: test_cmd_slash_toggles_flcomment
 *
 * Bar cursor on root.  Cmd-/ -> flcomment true.  Cmd-/ again -> false.
 * ---------------------------------------------------------------------- */
static void test_cmd_slash_toggles_flcomment(void) {
	setup();

	assert(g_state.cursor == 0);
	assert(!g_state.nodes[0].flcomment);

	/* Cmd-/ is '/' character with BOXEN_MOD_META modifier */
	boxen_event_t cmd_slash = make_char('/', BOXEN_MOD_META);
	boxen_outline_handle_key(&g_state, &cmd_slash);

	assert(g_state.nodes[0].flcomment);

	boxen_outline_handle_key(&g_state, &cmd_slash);

	assert(!g_state.nodes[0].flcomment);

	teardown();
}

/* -------------------------------------------------------------------------
 * TEST 7: test_checkbox_attribute_renders_box
 *
 * Load three nodes with markers CHECKED, UNCHECKED, and LEAF.
 * Draw the editor, then use boxen_mock_has_text to verify the screen
 * contains "[x]", "[ ]", and "o" from the rendered output.
 * ---------------------------------------------------------------------- */
static void test_checkbox_attribute_renders_box(void) {
	g_checkbox_nodes_inited = false;

	boxen_mock_reset(TEST_WIDTH, TEST_HEIGHT);
	boxen_init(boxen_mock_backend(), NULL, NULL);

	boxen_outline_state_t cs;
	boxen_outline_state_init(&cs, "test.checkbox", TEST_WIDTH, TEST_HEIGHT);

	/* Wire the checkbox-specific fetch hook */
	cs.outline_fetch_hook     = hook_fetch_checkbox;
	cs.toggle_breakpoint_hook = hook_toggle_breakpoint;
	cs.toggle_comment_hook    = hook_toggle_comment;
	cs.script_run_hook        = hook_script_run;
	cs.expand_hook            = hook_expand;

	bool ok = boxen_outline_refresh(&cs);
	assert(ok);

	/* Verify node_count and markers before draw */
	assert(cs.node_count == 3);
	assert(cs.nodes[0].marker == BOXEN_OUTLINE_MARKER_CHECKED);
	assert(cs.nodes[1].marker == BOXEN_OUTLINE_MARKER_UNCHECKED);
	assert(cs.nodes[2].marker == BOXEN_OUTLINE_MARKER_LEAF);

	/* Draw the editor to the mock terminal and present */
	boxen_outline_draw(&cs, cs.win);
	boxen_present();

	/* The draw function should have rendered the marker strings.
	 * boxen_mock_has_text scans the composited frame for any ASCII substring. */
	assert(boxen_mock_has_text("[x]"));
	assert(boxen_mock_has_text("[ ]"));
	/* 2026-06-10 JES #691 C.1.x: leaf marker is now U+25B7 (WHITE
	 * RIGHT-POINTING TRIANGLE), multi-byte UTF-8.  Use the codepoint
	 * scanner instead of has_text (which is ASCII-only). */
	assert(boxen_mock_has_codepoint(0x25B7));

	boxen_outline_state_teardown(&cs);
	boxen_shutdown();
}

/* -------------------------------------------------------------------------
 * TEST 8: test_toggle_hook_uses_state_and_node_idx
 *
 * 2026-06-09 JES #691 C.1 round 2 P0-2 behavioral test.
 *
 * The toggle hooks now take (state, node_idx) instead of (node).  This test
 * verifies that:
 *   a) The hook operates on s->nodes[node_idx] (not a stale pointer from
 *      before a potential node array rebuild).
 *   b) Setting the cursor to a non-zero index and pressing F9 toggles the
 *      correct node (by index), NOT node 0.
 *   c) Pressing F9 again restores the flag to false.
 *
 * This exercises the production code path: boxen_outline_handle_key passes
 * (s, s->cursor) to the toggle hook, not a raw node pointer.  The test mock
 * hook_toggle_breakpoint operates on s->nodes[node_idx] directly, which is
 * the behavioral contract the production hook must satisfy.
 * ---------------------------------------------------------------------- */
static void test_toggle_hook_uses_state_and_node_idx(void) {
	setup();

	/* Move cursor to leaf_aa (index 2) */
	boxen_event_t down = make_key(BOXEN_KEY_DOWN, BOXEN_MOD_NONE);
	boxen_outline_handle_key(&g_state, &down); /* cursor = 1 */
	boxen_outline_handle_key(&g_state, &down); /* cursor = 2 */
	assert(g_state.cursor == 2);
	assert(strcmp(g_state.nodes[2].text, "leaf_aa") == 0);

	/* Verify initial state: no flags set */
	assert(!g_state.nodes[0].flbreakpoint); /* root: untouched */
	assert(!g_state.nodes[2].flbreakpoint); /* leaf_aa: target */

	/* Press F9: should toggle nodes[2] (cursor), NOT nodes[0] */
	boxen_event_t f9 = make_key(BOXEN_KEY_F9, BOXEN_MOD_NONE);
	boxen_outline_handle_key(&g_state, &f9);

	/* nodes[2] toggled; nodes[0] still clear */
	assert( g_state.nodes[2].flbreakpoint);
	assert(!g_state.nodes[0].flbreakpoint);

	/* F9 again: toggle back */
	boxen_outline_handle_key(&g_state, &f9);
	assert(!g_state.nodes[2].flbreakpoint);

	/* Similarly verify Cmd-/ with flcomment at index 2 */
	assert(!g_state.nodes[2].flcomment);
	boxen_event_t cmd_slash = make_char('/', BOXEN_MOD_META);
	boxen_outline_handle_key(&g_state, &cmd_slash);
	assert( g_state.nodes[2].flcomment);
	assert(!g_state.nodes[0].flcomment);  /* root: untouched */
	boxen_outline_handle_key(&g_state, &cmd_slash);
	assert(!g_state.nodes[2].flcomment);

	teardown();
}

/* -------------------------------------------------------------------------
 * Main
 * ---------------------------------------------------------------------- */

int main(void) {
	TR_INIT("boxen_outline_tests");

	TR_RUN(test_open_builds_tree_from_canned_outline);
	TR_RUN(test_bar_cursor_down_advances_to_next_visible);
	TR_RUN(test_keypad_minus_collapses_current);
	TR_RUN(test_keypad_star_expands_all_descendants);
	TR_RUN(test_f9_toggles_flbreakpoint);
	TR_RUN(test_cmd_slash_toggles_flcomment);
	TR_RUN(test_checkbox_attribute_renders_box);
	TR_RUN(test_toggle_hook_uses_state_and_node_idx);

	TR_SUMMARY();
	return TR_EXIT_CODE();
}
