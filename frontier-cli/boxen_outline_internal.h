/*
 * boxen_outline_internal.h -- internals exposed for unit testing only.
 *
 * NEVER include this from production code outside boxen_outline.c itself.
 * Only tests/boxen_outline_tests.c and boxen_outline.c should include this.
 *
 * Mirrors boxen_repl_internal.h pattern exactly.
 *
 * 2026-06-09 JES Phase C.1 #691
 */

#ifndef BOXEN_OUTLINE_INTERNAL_H
#define BOXEN_OUTLINE_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "boxen/boxen.h"
#include "boxen_outline.h"  /* for BOXEN_OUTLINE_MAX_EDITORS */

/* Note: Common/headers/op.h (GIL-dependent runtime types) is NOT included here.
 * The test build defines BOXEN_OUTLINE_OMIT_MAIN which guards the production hook
 * implementations.  The node tree uses an opaque mock-compatible representation
 * so tests can drive the editor without the real ODB. */

/* -------------------------------------------------------------------------
 * Tree node representation (mock-compatible).
 *
 * In production, the editor works against live hdlheadrecord nodes.
 * The hook seam (outline_fetch_hook) populates a flat array of
 * boxen_outline_node_t structs that the editor renders and traverses.
 * This decouples the rendering/navigation logic from the real ODB types
 * and allows tests to inject canned trees without GIL or runtime linkage.
 * ---------------------------------------------------------------------- */

/* Maximum nodes in the editor's flat display list.
 * C.1 renders outlines up to this depth.  An outline exceeding this cap
 * renders the first BOXEN_OUTLINE_MAX_NODES visible nodes; deeper nodes
 * are silently omitted.  Increase in a later milestone if needed. */
#define BOXEN_OUTLINE_MAX_NODES 4096

/* Maximum bytes for a single headline's text (display, NUL-terminated). */
#define BOXEN_OUTLINE_TEXT_MAX 512

/* Marker byte used to detect checkbox attribute rendering. */
typedef enum {
	BOXEN_OUTLINE_MARKER_LEAF      = 0,  /* 'o' -- no children */
	BOXEN_OUTLINE_MARKER_EXPANDED  = 1,  /* 'v' -- expanded parent */
	BOXEN_OUTLINE_MARKER_COLLAPSED = 2,  /* '>' -- collapsed parent */
	BOXEN_OUTLINE_MARKER_CHECKED   = 3,  /* '[x]' -- checkbox:true */
	BOXEN_OUTLINE_MARKER_UNCHECKED = 4,  /* '[ ]' -- checkbox:false */
} boxen_outline_marker_t;

/* Single visible node in the editor's flat list. */
typedef struct boxen_outline_node {
	char     text[BOXEN_OUTLINE_TEXT_MAX]; /* headline text, NUL-terminated */
	int      level;                        /* indent level (0 = top-level) */
	bool     expanded;                     /* true if expanded */
	bool     has_children;                 /* true if node has subheads */
	bool     flbreakpoint;                 /* breakpoint flag from tyheadrecord */
	bool     flcomment;                    /* comment flag from tyheadrecord */
	boxen_outline_marker_t marker;         /* wedge/leaf/checkbox rendering hint */

	/* 2026-06-09 JES #691 C.1: opaque back-pointer to the real hdlheadrecord.
	 * In test builds this is NULL (or set to a test-managed pointer).
	 * In production builds this is cast from hdlheadrecord.
	 * The production hook implementations use this to apply mutations
	 * (flbreakpoint / flcomment toggles) back to the live ODB record. */
	void    *hnode_opaque;
} boxen_outline_node_t;

/* -------------------------------------------------------------------------
 * Hook function-pointer typedefs
 *
 * Two seams on boxen_outline_state_t:
 *   outline_fetch_hook -- populate the node array from a dotted path.
 *   script_run_hook    -- run the currently-edited path as a script.
 * ---------------------------------------------------------------------- */

/*
 * outline_fetch_fn_t -- given a dotted path, populate the node array.
 *
 * Production: walks live ODB via op verbs (GIL held).
 * Tests:      injects a canned tree.
 *
 * Parameters:
 *   path      -- NUL-terminated dotted ODB path
 *   nodes     -- caller-allocated array of at least BOXEN_OUTLINE_MAX_NODES
 *   node_count-- OUT: number of nodes populated
 *
 * Returns true on success.
 */
typedef bool (*outline_fetch_fn_t)(const char *path,
                                   boxen_outline_node_t *nodes,
                                   int *node_count);

/*
 * outline_toggle_breakpoint_fn_t -- toggle the flbreakpoint flag on a node.
 *
 * Production: writes (**hnode).flbreakpoint = !(**hnode).flbreakpoint under GIL.
 * Tests:      records that the toggle was requested; modifies mock node state.
 *
 * Parameters:
 *   node      -- pointer to the node in the editor's node array
 *
 * Returns new flbreakpoint value.
 */
typedef bool (*outline_toggle_breakpoint_fn_t)(boxen_outline_node_t *node);

/*
 * outline_toggle_comment_fn_t -- toggle the flcomment flag on a node.
 *
 * Production: writes (**hnode).flcomment = !(**hnode).flcomment under GIL.
 * Tests:      records the toggle; modifies mock node state.
 *
 * Parameters:
 *   node      -- pointer to the node in the editor's node array
 *
 * Returns new flcomment value.
 */
typedef bool (*outline_toggle_comment_fn_t)(boxen_outline_node_t *node);

/*
 * script_run_fn_t -- run the outline's path as a script.
 *
 * Production: dispatches /run @path through the existing slash-dispatch path.
 * Tests:      records the call.
 *
 * Parameters:
 *   path -- the editor's dotted path
 */
typedef void (*script_run_fn_t)(const char *path);

/*
 * outline_expand_fn_t -- expand or collapse a node.
 *
 * Production: calls opexpand / opcollapse under GIL; re-fetches display.
 * Tests:      flips the expanded flag in the node array directly.
 *
 * Parameters:
 *   node    -- pointer to the node in the editor's node array
 *   expand  -- true = expand, false = collapse
 */
typedef void (*outline_expand_fn_t)(boxen_outline_node_t *node, bool expand);

/* -------------------------------------------------------------------------
 * Per-editor state struct
 *
 * Heap-allocated per editor window.  Freed on window close.
 * ---------------------------------------------------------------------- */
typedef struct boxen_outline_state {
	/* Dotted ODB path this editor is bound to (NUL-terminated). */
	char path[256];

	/* Boxen window handle */
	boxen_window_t *win;

	/* Flat visible-node array.  Populated by outline_fetch_hook. */
	boxen_outline_node_t nodes[BOXEN_OUTLINE_MAX_NODES];
	int                  node_count;    /* valid entries in nodes[] */

	/* Bar cursor: index into nodes[] */
	int cursor;

	/* Scroll offset (first visible row in nodes[]) */
	int scroll_top;

	/* Hook seams -- overridable in tests */
	outline_fetch_fn_t              outline_fetch_hook;
	outline_toggle_breakpoint_fn_t  toggle_breakpoint_hook;
	outline_toggle_comment_fn_t     toggle_comment_hook;
	script_run_fn_t                 script_run_hook;
	outline_expand_fn_t             expand_hook;

	/* True after the editor has been fully initialized. */
	bool initialized;

	/* True if the window should close on the next tick. */
	bool should_close;
} boxen_outline_state_t;

/* -------------------------------------------------------------------------
 * Open-editors registry
 *
 * A flat array of state pointers, gated by BOXEN_OUTLINE_MAX_EDITORS.
 * Indexed by path for deduplication (raise existing window instead of open).
 * ---------------------------------------------------------------------- */
typedef struct boxen_outline_registry {
	boxen_outline_state_t *editors[BOXEN_OUTLINE_MAX_EDITORS];
	int count;
} boxen_outline_registry_t;

/* -------------------------------------------------------------------------
 * Internal API (used by tests and by boxen_outline.c itself)
 * ---------------------------------------------------------------------- */

/*
 * boxen_outline_state_init -- zero-fill state and set up hooks.
 *
 * path -- the dotted ODB path this editor will display.
 * tw, th -- terminal dimensions (used to compute initial window geometry).
 *
 * Production hooks are wired unless BOXEN_OUTLINE_OMIT_MAIN is defined.
 */
void boxen_outline_state_init(boxen_outline_state_t *s, const char *path,
                              int tw, int th);

/*
 * boxen_outline_state_teardown -- close the window and free resources.
 *
 * Does NOT free the state struct itself (caller does that after teardown).
 */
void boxen_outline_state_teardown(boxen_outline_state_t *s);

/*
 * boxen_outline_handle_key -- process a single key event.
 *
 * Called from the window's input callback.
 * Returns true if the key was consumed.
 */
bool boxen_outline_handle_key(boxen_outline_state_t *s,
                              const boxen_event_t *ev);

/*
 * boxen_outline_draw -- redraw the editor window.
 *
 * Called from the window's draw callback.
 */
void boxen_outline_draw(boxen_outline_state_t *s, boxen_window_t *win);

/*
 * boxen_outline_refresh -- re-fetch the outline from ODB and rebuild nodes[].
 *
 * Must be called after expand/collapse mutations.
 * In tests, the fetch hook may rebuild from canned data without touching ODB.
 */
bool boxen_outline_refresh(boxen_outline_state_t *s);

/*
 * boxen_outline_cursor_advance -- move bar cursor by delta (signed).
 *
 * Clamps to [0, node_count-1].  Scrolls to keep cursor visible.
 * Returns new cursor position.
 */
int boxen_outline_cursor_advance(boxen_outline_state_t *s, int delta);

/* -------------------------------------------------------------------------
 * Production hook implementations (declared here, defined in boxen_outline.c
 * inside #ifndef BOXEN_OUTLINE_OMIT_MAIN).
 *
 * NOT available in test builds.
 * ---------------------------------------------------------------------- */
#ifndef BOXEN_OUTLINE_OMIT_MAIN
bool boxen_outline_real_fetch(const char *path,
                              boxen_outline_node_t *nodes,
                              int *node_count);
bool boxen_outline_real_toggle_breakpoint(boxen_outline_node_t *node);
bool boxen_outline_real_toggle_comment(boxen_outline_node_t *node);
void boxen_outline_real_script_run(const char *path);
void boxen_outline_real_expand(boxen_outline_node_t *node, bool expand);
#endif /* !BOXEN_OUTLINE_OMIT_MAIN */

#endif /* BOXEN_OUTLINE_INTERNAL_H */
