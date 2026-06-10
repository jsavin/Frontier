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
 *
 * C.1 renders outlines up to this size.  An outline exceeding this cap
 * renders the first BOXEN_OUTLINE_MAX_NODES visible nodes; further nodes
 * are silently omitted with a one-shot log_warn.
 *
 * 2026-06-09 JES #691 Phase C.1.x: bumped from 4096 to 16384 per JES.  At
 * sizeof(boxen_outline_node_t) ~= 545 bytes, the state struct's nodes[]
 * array is ~8.5 MB -- well within reason for a per-window heap alloc on
 * any modern system, and gives headroom for real Frontier.root outlines
 * which can easily exceed 4K visible nodes when expanded. */
#define BOXEN_OUTLINE_MAX_NODES 16384

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

/* Forward declaration so hook typedefs can reference boxen_outline_state_t
 * before the full struct definition below.
 * 2026-06-09 JES #691 C.1 round 2 P0-2. */
typedef struct boxen_outline_state boxen_outline_state_t;

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
 * Production: walks live ODB via op verbs (GIL held).  Also stashes the
 *   resolved hdloutlinerecord on s->houtline_opaque so that the toggle
 *   hooks can use oppushoutline/opdirtyoutline/oppopoutline.
 * Tests:      injects a canned tree; leaves s->houtline_opaque NULL.
 *
 * Parameters:
 *   s         -- editor state (for stashing houtline_opaque on production path)
 *   path      -- NUL-terminated dotted ODB path
 *   nodes     -- caller-allocated array of at least BOXEN_OUTLINE_MAX_NODES
 *   node_count-- OUT: number of nodes populated
 *
 * Returns true on success.
 *
 * 2026-06-09 JES #691 C.1 round 2 P0-2: added state parameter so production
 * fetch can stash houtline_opaque for the opdirtyoutline pattern.
 */
typedef bool (*outline_fetch_fn_t)(boxen_outline_state_t *s,
                                   const char *path,
                                   boxen_outline_node_t *nodes,
                                   int *node_count);

/*
 * outline_toggle_breakpoint_fn_t -- toggle the flbreakpoint flag on a node.
 *
 * Production: pushes the outline context, toggles (**hnode).flbreakpoint,
 *   calls opdirtyoutline(), preserves flrecentlychanged, pops context.
 *   Mirrors the canonical pattern in scripts.c:2863-2889 (optogglebreakpoint).
 * Tests:      modifies the node's flbreakpoint field directly in nodes[].
 *
 * Parameters:
 *   s         -- editor state (provides houtline_opaque and nodes[] access)
 *   node_idx  -- index into s->nodes[] identifying the target node
 *
 * Returns new flbreakpoint value.
 *
 * 2026-06-09 JES #691 C.1 round 2 P0-2: signature changed from (node) to
 * (state, node_idx) to give the hook access to houtline_opaque for the
 * oppushoutline/opdirtyoutline/oppopoutline pattern.
 */
typedef bool (*outline_toggle_breakpoint_fn_t)(boxen_outline_state_t *s,
                                               int node_idx);

/*
 * outline_toggle_comment_fn_t -- toggle the flcomment flag on a node.
 *
 * Production: pushes the outline context, toggles (**hnode).flcomment,
 *   calls opdirtyoutline(), preserves flrecentlychanged, pops context.
 *   Mirrors the canonical pattern in scripts.c:2863-2889.
 * Tests:      modifies the node's flcomment field directly in nodes[].
 *
 * Parameters:
 *   s         -- editor state
 *   node_idx  -- index into s->nodes[] identifying the target node
 *
 * Returns new flcomment value.
 *
 * 2026-06-09 JES #691 C.1 round 2 P0-2: signature changed from (node) to
 * (state, node_idx) for the same reason as toggle_breakpoint above.
 */
typedef bool (*outline_toggle_comment_fn_t)(boxen_outline_state_t *s,
                                            int node_idx);

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
 * Production: directly writes (**hnode).flexpanded under GIL.  Does NOT
 *             route through opexpand / opcollapse because those dereference
 *             op_get_outlinedata() which is nil in the REPL event loop
 *             (no outline context is pushed).  The caller is expected to
 *             follow up with boxen_outline_refresh() which rebuilds the
 *             display list from the live outline structure.
 * Tests:      flips the expanded flag in the node array directly.
 *
 * 2026-06-09 JES #691 C.1 round 2 cosmetic: docblock previously said
 * "calls opexpand / opcollapse"; corrected per /gate concurrency note.
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
/* 2026-06-09 JES #691 Phase C.1 round 2: definition without typedef rename.
 * The typedef was already established by the forward declaration at line 76
 * (`typedef struct boxen_outline_state boxen_outline_state_t`).  Repeating
 * `typedef struct {...} boxen_outline_state_t` here triggers a C11
 * typedef-redefinition warning; we just complete the struct definition. */
struct boxen_outline_state {
	/* Dotted ODB path this editor is bound to (NUL-terminated). */
	char path[256];

	/* Boxen window handle */
	boxen_window_t *win;

	/* 2026-06-09 JES #691 C.1 round 2 P0-2: stashed hdloutlinerecord.
	 * Set by boxen_outline_real_fetch after the outline is confirmed in
	 * memory.  Used by the toggle hooks so they can push the outline
	 * context (oppushoutline), call opdirtyoutline(), and pop without
	 * requiring op_get_outlinedata() to already be set (which it isn't
	 * in the boxen REPL event loop).
	 *
	 * Lifetime invariant: valid for the duration of the editor session,
	 * as long as the script object itself is not deleted or re-serialized
	 * to disk.  If UserTalk deletes the object while the editor is open,
	 * this handle becomes a dangling pointer (UAF hazard).  C.1 accepts
	 * this risk because the editor is read-only-with-flag-mutation and
	 * the scenario is unlikely.  Tracked as a follow-up issue for C.2+.
	 *
	 * In test builds (BOXEN_OUTLINE_OMIT_MAIN) this is always NULL.
	 */
	void *houtline_opaque; /* hdloutlinerecord -- production only */

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
};

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
/* 2026-06-09 JES #691 C.1 round 2 P0-2: takes state to stash houtline_opaque. */
bool boxen_outline_real_fetch(boxen_outline_state_t *s,
                              const char *path,
                              boxen_outline_node_t *nodes,
                              int *node_count);
/* 2026-06-09 JES #691 C.1 round 2 P0-2: signatures updated to (state, node_idx)
 * to allow access to houtline_opaque for oppushoutline/opdirtyoutline pattern. */
bool boxen_outline_real_toggle_breakpoint(boxen_outline_state_t *s,
                                          int node_idx);
bool boxen_outline_real_toggle_comment(boxen_outline_state_t *s,
                                       int node_idx);
void boxen_outline_real_script_run(const char *path);
/* 2026-06-09 JES #691 C.1 round 2 P0-1: direct field flip instead of
 * opexpand/opcollapse which require op_get_outlinedata() to be non-nil. */
void boxen_outline_real_expand(boxen_outline_node_t *node, bool expand);
#endif /* !BOXEN_OUTLINE_OMIT_MAIN */

#endif /* BOXEN_OUTLINE_INTERNAL_H */
