/*	$Id$	*/

/******************************************************************************

    UserLand Frontier(tm) -- High performance Web content management,
    object database, system-level and Internet scripting environment,
    including source code editing and debugging.

    Copyright (C) 1992-2004 UserLand Software, Inc.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

******************************************************************************/

#ifndef headlessselectioninclude
#define headlessselectioninclude

#ifndef langinclude
	#include "lang.h"
#endif

#ifndef oplistinclude
	#include "oplist.h"
#endif

#include <stdatomic.h>


/**
 * TABLE_SELECTION_MAX_NESTING_DEPTH - Maximum recursion depth for table iteration
 *
 * Protects against stack overflow when traversing deeply nested tables.
 * Typical table nesting is <10 levels. Limit of 100 is conservative for extreme cases.
 */
#define TABLE_SELECTION_MAX_NESTING_DEPTH 100


/**
 * TABLE_SELECTION_INITIAL_EXPANSION_CAPACITY - Initial size for expansion array
 *
 * Most tables have <10 expanded children. Starting with 16 (power of 2) allows
 * efficient doubling on realloc while avoiding excessive initial allocation.
 */
#define TABLE_SELECTION_INITIAL_EXPANSION_CAPACITY 16


/**
 * table_selection_context_t - Thread-local selection and navigation state
 *
 * Design:
 * - One instance per thread
 * - Created on first access, persists until thread exit or explicit reset
 * - Stores ephemeral UI state (selection, cursor, expansion)
 * - NOT saved to database
 *
 * Thread Safety:
 * - Thread-local → no locking needed in Phase 3
 * - Phase 6+: Each user thread has isolated context
 */
typedef struct table_selection_context {
	/*
	 * OBJECT IDENTITY
	 */
	hdlhashtable current_table;     /* Which table context applies to */
	boolean is_valid;               /* Is context initialized? */

	/*
	 * SELECTION STATE (Multi-Select Support)
	 *
	 * Frontier allows shift-click to mark multiple table entries.
	 * In headless mode, table.select(key) marks entries.
	 */
	hdllistrecord selected_keys;    /* List of bigstrings (selected keys) */
	long ct_selected;               /* Count of selections */

	/*
	 * CURSOR STATE (Single Position)
	 */
	bigstring cursor_key;           /* Current cursor key (empty = no cursor) */
	hdlhashnode cursor_node;        /* Direct pointer to current node */
	long cursor_flat_index;         /* 1-based flat index (accounting for expansion) */

	/*
	 * EXPANSION STATE (Nested Table Support)
	 *
	 * Track which nested tables are expanded (similar to outline flexpanded).
	 * Key insight: Row numbers depend on expansion state.
	 *
	 * Example:
	 *   Row 1: workspace
	 *   Row 2:   workspace.prefs (expanded, has children)
	 *   Row 3:     workspace.prefs.user
	 *   Row 4:     workspace.prefs.system
	 *   Row 5:   workspace.data
	 *
	 * If workspace.prefs collapsed:
	 *   Row 1: workspace
	 *   Row 2:   workspace.prefs (collapsed, children hidden)
	 *   Row 3:   workspace.data
	 */
	hdlhashtable *expanded_tables;  /* Array of expanded table handles */
	long ct_expanded;               /* Count of expanded tables */
	long max_expanded;              /* Array capacity */

	/*
	 * ITERATION STATE
	 */
	long iteration_depth;           /* Current recursion depth */
	boolean iteration_in_progress;  /* Guard against nested iteration */

	/*
	 * REFERENCE COUNTING
	 */
	_Atomic uint32_t refcount;      /* For nested operation safety */

	/*
	 * RESERVED (Phase 6+)
	 */
	void *reserved[4];              /* Future: CRDT metadata, sync state */

} table_selection_context_t;


/*
 * LIFECYCLE API
 */

/**
 * table_selection_init - Initialize table selection subsystem
 *
 * Called once at process startup.
 * Sets up thread-local storage key.
 */
extern void table_selection_init(void);

/**
 * table_selection_acquire - Get or create thread-local selection context
 *
 * @return Context for current thread (never NULL)
 *
 * Implementation:
 * - Check thread-local storage for existing context
 * - If not found, allocate new context with refcount=1
 * - Initialize all fields to defaults
 * - Return context pointer
 *
 * Thread Safety: Thread-local, no locking needed
 */
extern table_selection_context_t* table_selection_acquire(void);

/**
 * table_selection_retain - Increment reference count on context
 *
 * @param ctx - Context to retain (NULL-safe)
 * @return The same context pointer
 *
 * Use when passing context to nested operations that might outlive caller.
 */
extern table_selection_context_t* table_selection_retain(table_selection_context_t *ctx);

/**
 * table_selection_release - Release reference to context
 *
 * @param ctx - Context to release (NULL-safe)
 *
 * Decrements refcount. If refcount reaches 0, frees all resources.
 */
extern void table_selection_release(table_selection_context_t *ctx);

/**
 * table_selection_reset - Clear selection and cursor, keep expansion state
 *
 * @param ctx - Context to reset
 *
 * Use case: User calls table.clearSelection()
 */
extern void table_selection_reset(table_selection_context_t *ctx);

/**
 * table_selection_reset_full - Clear everything including expansion state
 *
 * @param ctx - Context to reset
 *
 * Use case: Switching to different table
 */
extern void table_selection_reset_full(table_selection_context_t *ctx);


/*
 * EXPANSION STATE MANAGEMENT
 */

/**
 * table_selection_is_expanded - Check if nested table is expanded
 *
 * @param ctx - Selection context
 * @param htable - Table to check
 * @return true if table is in expanded list
 */
extern boolean table_selection_is_expanded(table_selection_context_t *ctx, hdlhashtable htable);

/**
 * table_selection_expand - Mark table as expanded
 *
 * @param ctx - Selection context
 * @param htable - Table to expand
 * @return true if newly expanded, false if already expanded
 */
extern boolean table_selection_expand(table_selection_context_t *ctx, hdlhashtable htable);

/**
 * table_selection_collapse - Mark table as collapsed
 *
 * @param ctx - Selection context
 * @param htable - Table to collapse
 * @return true if collapsed, false if wasn't expanded
 */
extern boolean table_selection_collapse(table_selection_context_t *ctx, hdlhashtable htable);


/*
 * MULTI-SELECTION SUPPORT
 */

/**
 * table_selection_add - Add key to selection
 *
 * @param ctx - Selection context
 * @param key - Key to add (bigstring)
 * @return true if added, false if already selected
 */
extern boolean table_selection_add(table_selection_context_t *ctx, const bigstring key);

/**
 * table_selection_remove - Remove key from selection
 *
 * @param ctx - Selection context
 * @param key - Key to remove
 * @return true if removed, false if wasn't selected
 */
extern boolean table_selection_remove(table_selection_context_t *ctx, const bigstring key);

/**
 * table_selection_is_selected - Check if key is selected
 *
 * @param ctx - Selection context
 * @param key - Key to check
 * @return true if in selection list
 */
extern boolean table_selection_is_selected(table_selection_context_t *ctx, const bigstring key);

/**
 * table_selection_clear - Clear all selections
 *
 * @param ctx - Selection context
 */
extern void table_selection_clear(table_selection_context_t *ctx);

/**
 * table_selection_get_count - Get number of selected items
 *
 * @param ctx - Selection context
 * @return Number of selected items
 */
extern long table_selection_get_count(table_selection_context_t *ctx);


/*
 * CURSOR MANAGEMENT
 */

/**
 * table_selection_set_cursor - Set cursor to specific key
 *
 * @param ctx - Selection context
 * @param htable - Table containing key
 * @param key - Key to set cursor to
 * @return true if cursor set, false if key not found
 */
extern boolean table_selection_set_cursor(table_selection_context_t *ctx,
                                          hdlhashtable htable,
                                          const bigstring key);

/**
 * table_selection_get_cursor - Get current cursor position
 *
 * @param ctx - Selection context
 * @param key_out - Output: cursor key (empty if no cursor)
 * @return true if cursor set, false if no cursor
 */
extern boolean table_selection_get_cursor(table_selection_context_t *ctx,
                                          bigstring key_out);

/**
 * table_selection_get_cursor_node - Get current cursor hash node
 *
 * @param ctx - Selection context
 * @return Current cursor node, or NULL if no cursor set
 */
extern hdlhashnode table_selection_get_cursor_node(table_selection_context_t *ctx);


/*
 * HELPER FUNCTIONS - Row Counting and Node Lookup
 */

/**
 * table_selection_count_visible_rows - Count total visible rows
 *
 * @param ctx - Selection context
 * @param htable - Table to count
 * @return Total number of visible rows (1-based indexing)
 *
 * Algorithm:
 * 1. Start with immediate children count
 * 2. For each child that is an expanded table, recurse
 * 3. Sum up visible descendants
 */
extern long table_selection_count_visible_rows(table_selection_context_t *ctx,
                                               hdlhashtable htable);

/**
 * table_selection_get_node_at_row - Find hash node at 1-based row index
 *
 * @param ctx - Selection context
 * @param htable - Root table
 * @param row - 1-based row number
 * @param hnode_out - Output: hash node at that row
 * @param htable_out - Output: table containing that node
 * @return true if found, false if row out of bounds
 */
extern boolean table_selection_get_node_at_row(table_selection_context_t *ctx,
                                               hdlhashtable htable,
                                               long row,
                                               hdlhashnode *hnode_out,
                                               hdlhashtable *htable_out);

/**
 * table_selection_get_row_for_node - Find row number for a given node
 *
 * @param ctx - Selection context
 * @param htable - Root table
 * @param hnode - Node to find
 * @return 1-based row number, or 0 if not found
 */
extern long table_selection_get_row_for_node(table_selection_context_t *ctx,
                                             hdlhashtable htable,
                                             hdlhashnode hnode);

/**
 * table_selection_get_row_for_key - Find row number for a given key
 *
 * @param ctx - Selection context
 * @param htable - Root table
 * @param key - Key to find
 * @return 1-based row number, or 0 if not found
 */
extern long table_selection_get_row_for_key(table_selection_context_t *ctx,
                                            hdlhashtable htable,
                                            const bigstring key);


#endif /* headlessselectioninclude */
