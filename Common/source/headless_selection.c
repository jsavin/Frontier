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

#include "headless_selection.h"
#include "langexternal.h"
#include "strings.h"
#include "memory.h"
#include "logging.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>


/* Thread-local storage key for macOS */
static pthread_key_t table_selection_key;
static boolean table_selection_initialized = false;


/* Forward declarations for internal helpers */
static void table_selection_thread_cleanup(void *ctx);
static boolean table_selection_find_in_list(hdllistrecord hlist, const bigstring key, long *index_out);
static boolean table_selection_is_table_value(tyvaluerecord *val, hdlhashtable *htable_out);


/*
 * LIFECYCLE API IMPLEMENTATION
 */

void table_selection_init(void) {
	/*
	 * Initialize thread-local storage for selection contexts.
	 * Called once at process startup.
	 */
	if (table_selection_initialized) {
		return;
	}

	pthread_key_create(&table_selection_key, table_selection_thread_cleanup);
	table_selection_initialized = true;

	log_debug(LOG_COMP_TABLE, "Table selection subsystem initialized");
}


table_selection_context_t* table_selection_acquire(void) {
	/*
	 * Get or create thread-local selection context.
	 * Returns context for current thread (never NULL).
	 */
	table_selection_context_t *ctx;

	/* Ensure initialized */
	if (!table_selection_initialized) {
		table_selection_init();
	}

	/* Check thread-local storage for existing context */
	ctx = (table_selection_context_t *)pthread_getspecific(table_selection_key);

	if (ctx == NULL) {
		/* Allocate new context */
		ctx = (table_selection_context_t *)malloc(sizeof(table_selection_context_t));
		if (ctx == NULL) {
			log_error(LOG_COMP_TABLE, "Failed to allocate selection context");
			/* Fatal error - can't proceed without context */
			abort();
		}

		/* Initialize all fields */
		memset(ctx, 0, sizeof(table_selection_context_t));

		atomic_init(&ctx->refcount, 1);
		ctx->is_valid = true;
		ctx->current_table = NULL;
		ctx->selected_keys = NULL;
		ctx->ct_selected = 0;
		ctx->cursor_key[0] = 0;  /* Empty string */
		ctx->cursor_node = NULL;
		ctx->cursor_flat_index = 0;
		ctx->iteration_depth = 0;
		ctx->iteration_in_progress = false;

		/* Initialize expansion state array */
		ctx->max_expanded = TABLE_SELECTION_INITIAL_EXPANSION_CAPACITY;
		ctx->ct_expanded = 0;
		ctx->expanded_tables = (hdlhashtable *)malloc(sizeof(hdlhashtable) * ctx->max_expanded);
		if (ctx->expanded_tables == NULL) {
			free(ctx);
			log_error(LOG_COMP_TABLE, "Failed to allocate expansion array");
			abort();
		}

		/* Store in thread-local storage */
		pthread_setspecific(table_selection_key, ctx);

		log_trace(LOG_COMP_TABLE, "Created new selection context for thread");
	} else {
		/*
		 * Increment refcount for existing context.
		 *
		 * IMPORTANT: acquire() uses reference-counted semantics (not idempotent).
		 * Every acquire() call MUST have a matching release() call.
		 * This allows nested operations to safely hold references without
		 * deallocating the context prematurely.
		 *
		 * For shared ownership without incrementing refcount, use retain() explicitly.
		 */
		atomic_fetch_add(&ctx->refcount, 1);
	}

	return ctx;
}


table_selection_context_t* table_selection_retain(table_selection_context_t *ctx) {
	/*
	 * Increment reference count on context.
	 * NULL-safe.
	 */
	if (ctx == NULL) {
		return NULL;
	}

	atomic_fetch_add(&ctx->refcount, 1);
	return ctx;
}


void table_selection_release(table_selection_context_t *ctx) {
	/*
	 * Release reference to context.
	 * Decrements refcount. If refcount reaches 0, frees all resources.
	 */
	uint32_t old_count;

	if (ctx == NULL) {
		return;
	}

	old_count = atomic_fetch_sub(&ctx->refcount, 1);

	if (old_count == 1) {
		/* Last reference - free resources */
		log_trace(LOG_COMP_TABLE, "Freeing selection context");

		/* Dispose of selection list */
		if (ctx->selected_keys != NULL) {
			/*
			 * Note: opdisposelist() returns void, so we cannot check for errors.
			 * In the unlikely event that cleanup fails, we still free the context
			 * to prevent permanent memory leak. The list memory would be leaked,
			 * but the context itself won't accumulate.
			 */
			log_debug(LOG_COMP_TABLE, "Disposing selection list during context cleanup");
			opdisposelist(ctx->selected_keys);
			ctx->selected_keys = NULL;
		}

		/* Free expansion array */
		if (ctx->expanded_tables != NULL) {
			free(ctx->expanded_tables);
			ctx->expanded_tables = NULL;
		}

		/* Clear thread-local storage */
		pthread_setspecific(table_selection_key, NULL);

		/* Free context */
		free(ctx);
	}
}


void table_selection_reset(table_selection_context_t *ctx) {
	/*
	 * Clear selection and cursor, but keep expansion state.
	 * Use case: table.clearSelection()
	 */
	if (ctx == NULL) {
		return;
	}

	/* Clear selection list */
	if (ctx->selected_keys != NULL) {
		opdisposelist(ctx->selected_keys);
		ctx->selected_keys = NULL;
	}
	ctx->ct_selected = 0;

	/* Clear cursor */
	ctx->cursor_key[0] = 0;
	ctx->cursor_node = NULL;
	ctx->cursor_flat_index = 0;

	log_trace(LOG_COMP_TABLE, "Reset selection context (preserved expansion state)");
}


void table_selection_reset_full(table_selection_context_t *ctx) {
	/*
	 * Clear everything including expansion state.
	 * Use case: switching to different table
	 */
	if (ctx == NULL) {
		return;
	}

	/* Clear selection and cursor */
	table_selection_reset(ctx);

	/* Clear expansion state */
	ctx->ct_expanded = 0;

	/* Clear table reference */
	ctx->current_table = NULL;

	log_trace(LOG_COMP_TABLE, "Full reset of selection context");
}


/*
 * EXPANSION STATE MANAGEMENT
 */

boolean table_selection_is_expanded(table_selection_context_t *ctx, hdlhashtable htable) {
	/*
	 * Check if nested table is expanded.
	 */
	long i;

	if (ctx == NULL || htable == NULL) {
		return false;
	}

	for (i = 0; i < ctx->ct_expanded; i++) {
		if (ctx->expanded_tables[i] == htable) {
			return true;
		}
	}

	return false;
}


boolean table_selection_expand(table_selection_context_t *ctx, hdlhashtable htable) {
	/*
	 * Mark table as expanded.
	 * Returns true if newly expanded, false if already expanded.
	 */
	hdlhashtable *new_array;
	long new_capacity;

	if (ctx == NULL || htable == NULL) {
		return false;
	}

	/* Already expanded? */
	if (table_selection_is_expanded(ctx, htable)) {
		return false;
	}

	/* Grow array if needed */
	if (ctx->ct_expanded >= ctx->max_expanded) {
		new_capacity = ctx->max_expanded * 2;
		new_array = (hdlhashtable *)realloc(ctx->expanded_tables,
		                                     sizeof(hdlhashtable) * new_capacity);
		if (new_array == NULL) {
			log_error(LOG_COMP_TABLE, "Failed to grow expansion array");
			return false;
		}
		ctx->expanded_tables = new_array;
		ctx->max_expanded = new_capacity;
	}

	/* Add to expanded list */
	ctx->expanded_tables[ctx->ct_expanded++] = htable;

	log_trace(LOG_COMP_TABLE, "Expanded table (ct_expanded=%ld)", ctx->ct_expanded);
	return true;
}


boolean table_selection_collapse(table_selection_context_t *ctx, hdlhashtable htable) {
	/*
	 * Mark table as collapsed.
	 * Returns true if collapsed, false if wasn't expanded.
	 */
	long i;

	if (ctx == NULL || htable == NULL) {
		return false;
	}

	/* Find table in expanded list */
	for (i = 0; i < ctx->ct_expanded; i++) {
		if (ctx->expanded_tables[i] == htable) {
			/* Found it - remove by shifting remaining entries */
			long j;
			for (j = i; j < ctx->ct_expanded - 1; j++) {
				ctx->expanded_tables[j] = ctx->expanded_tables[j + 1];
			}
			ctx->ct_expanded--;

			log_trace(LOG_COMP_TABLE, "Collapsed table (ct_expanded=%ld)", ctx->ct_expanded);
			return true;
		}
	}

	return false;
}


/*
 * MULTI-SELECTION SUPPORT
 */

boolean table_selection_add(table_selection_context_t *ctx, const bigstring key) {
	/*
	 * Add key to selection.
	 * Returns true if added, false if already selected.
	 */
	if (ctx == NULL || key == NULL || key[0] == 0) {
		return false;
	}

	/* Already selected? */
	if (table_selection_is_selected(ctx, key)) {
		return false;
	}

	/* Create list if needed */
	if (ctx->selected_keys == NULL) {
		if (!opnewlist(&ctx->selected_keys, false)) {
			log_error(LOG_COMP_TABLE, "Failed to create selection list");
			return false;
		}
	}

	/* Add to list (need to cast away const for oppushstring) */
	if (!oppushstring(ctx->selected_keys, NULL, (ptrstring)key)) {
		log_error(LOG_COMP_TABLE, "Failed to add key to selection list");
		return false;
	}

	ctx->ct_selected++;

	log_trace(LOG_COMP_TABLE, "Added key to selection (ct_selected=%ld)", ctx->ct_selected);
	return true;
}


boolean table_selection_remove(table_selection_context_t *ctx, const bigstring key) {
	/*
	 * Remove key from selection.
	 * Returns true if removed, false if wasn't selected.
	 */
	long index;

	if (ctx == NULL || key == NULL || ctx->selected_keys == NULL) {
		return false;
	}

	/* Find in list */
	if (!table_selection_find_in_list(ctx->selected_keys, key, &index)) {
		return false;
	}

	/* Remove from list (1-based indexing) */
	if (!opdeletelistitem(ctx->selected_keys, index + 1, NULL)) {
		log_error(LOG_COMP_TABLE, "Failed to remove key from selection list");
		return false;
	}

	ctx->ct_selected--;

	log_trace(LOG_COMP_TABLE, "Removed key from selection (ct_selected=%ld)", ctx->ct_selected);
	return true;
}


boolean table_selection_is_selected(table_selection_context_t *ctx, const bigstring key) {
	/*
	 * Check if key is selected.
	 */
	if (ctx == NULL || key == NULL || ctx->selected_keys == NULL) {
		return false;
	}

	return table_selection_find_in_list(ctx->selected_keys, key, NULL);
}


void table_selection_clear(table_selection_context_t *ctx) {
	/*
	 * Clear all selections.
	 */
	if (ctx == NULL) {
		return;
	}

	if (ctx->selected_keys != NULL) {
		opdisposelist(ctx->selected_keys);
		ctx->selected_keys = NULL;
	}

	ctx->ct_selected = 0;

	log_trace(LOG_COMP_TABLE, "Cleared selection");
}


long table_selection_get_count(table_selection_context_t *ctx) {
	/*
	 * Get number of selected items.
	 */
	if (ctx == NULL) {
		return 0;
	}

	return ctx->ct_selected;
}


/*
 * CURSOR MANAGEMENT
 */

boolean table_selection_set_cursor(table_selection_context_t *ctx,
                                   hdlhashtable htable,
                                   const bigstring key) {
	/*
	 * Set cursor to specific key.
	 * Returns true if cursor set, false if key not found.
	 */
	hdlhashnode hnode;

	if (ctx == NULL || htable == NULL || key == NULL) {
		return false;
	}

	/* Find the hash node (use explicit table, not current context) */
	if (!hashtablelookupnode(htable, key, &hnode)) {
		return false;
	}

	/* Update cursor */
	copystring(key, ctx->cursor_key);
	ctx->cursor_node = hnode;
	ctx->current_table = htable;

	/* Calculate flat index (expensive, but needed for goto) */
	ctx->cursor_flat_index = table_selection_get_row_for_key(ctx, htable, key);

	log_trace(LOG_COMP_TABLE, "Set cursor to row %ld", ctx->cursor_flat_index);
	return true;
}


boolean table_selection_get_cursor(table_selection_context_t *ctx,
                                   bigstring key_out) {
	/*
	 * Get current cursor position.
	 * Returns true if cursor set, false if no cursor.
	 */
	if (ctx == NULL || key_out == NULL) {
		return false;
	}

	if (ctx->cursor_key[0] == 0) {
		/* No cursor set */
		key_out[0] = 0;
		return false;
	}

	copystring(ctx->cursor_key, key_out);
	return true;
}


hdlhashnode table_selection_get_cursor_node(table_selection_context_t *ctx) {
	/*
	 * Get current cursor hash node.
	 * Returns NULL if no cursor set.
	 */
	if (ctx == NULL) {
		return NULL;
	}

	return ctx->cursor_node;
}


/*
 * HELPER FUNCTIONS - Row Counting and Node Lookup
 */

boolean table_selection_count_visible_rows(table_selection_context_t *ctx,
                                           hdlhashtable htable,
                                           long *count_out) {
	/*
	 * Count total visible rows, accounting for expansion state.
	 *
	 * Algorithm:
	 * 1. Start with immediate children count
	 * 2. For each child that is an expanded table, recurse
	 * 3. Sum up visible descendants
	 */
	long count;
	hdlhashnode nomad;
	hdlhashtable nested_table;
	long nested_count;

	if (ctx == NULL || htable == NULL || count_out == NULL) {
		return false;
	}

	/* Check recursion depth before incrementing */
	if (ctx->iteration_depth >= TABLE_SELECTION_MAX_NESTING_DEPTH) {
		log_error(LOG_COMP_TABLE, "Table nesting too deep (max=%d)", TABLE_SELECTION_MAX_NESTING_DEPTH);
		return false;
	}

	count = 0;
	nomad = (**htable).hfirstsort;

	ctx->iteration_depth++;

	while (nomad != NULL) {
		count++;  /* This entry is visible */

		/* Is this entry a nested table? */
		if (table_selection_is_table_value(&(**nomad).val, &nested_table)) {
			/* Is it expanded? */
			if (table_selection_is_expanded(ctx, nested_table)) {
				/* Add nested table's visible rows */
				if (!table_selection_count_visible_rows(ctx, nested_table, &nested_count)) {
					ctx->iteration_depth--;
					return false;  /* Error in recursive call */
				}
				count += nested_count;
			}
		}

		nomad = (**nomad).sortedlink;
	}

	ctx->iteration_depth--;

	*count_out = count;
	return true;
}


boolean table_selection_get_node_at_row(table_selection_context_t *ctx,
                                        hdlhashtable htable,
                                        long row,
                                        hdlhashnode *hnode_out,
                                        hdlhashtable *htable_out) {
	/*
	 * Find hash node at 1-based row index.
	 *
	 * Algorithm:
	 * - Walk sorted list, counting visible rows
	 * - When expanded table encountered, recurse to count its children
	 * - Return node when row count matches target
	 */
	long current_row;
	hdlhashnode nomad;
	hdlhashtable nested_table;
	long nested_count;
	long nested_row;

	if (ctx == NULL || htable == NULL || hnode_out == NULL || htable_out == NULL) {
		return false;
	}

	if (row < 1) {
		return false;
	}

	/* Check recursion depth before incrementing */
	if (ctx->iteration_depth >= TABLE_SELECTION_MAX_NESTING_DEPTH) {
		log_error(LOG_COMP_TABLE, "Table nesting too deep (max=%d)", TABLE_SELECTION_MAX_NESTING_DEPTH);
		return false;
	}

	current_row = 0;
	nomad = (**htable).hfirstsort;

	ctx->iteration_depth++;

	while (nomad != NULL) {
		current_row++;  /* Increment for this entry */

		if (current_row == row) {
			/* Found it! */
			*hnode_out = nomad;
			*htable_out = htable;
			ctx->iteration_depth--;
			return true;
		}

		/* Check if this is an expanded nested table */
		if (table_selection_is_table_value(&(**nomad).val, &nested_table)) {
			if (table_selection_is_expanded(ctx, nested_table)) {
				/* Count nested rows */
				if (!table_selection_count_visible_rows(ctx, nested_table, &nested_count)) {
					ctx->iteration_depth--;
					return false;  /* Error counting nested rows */
				}

				if (current_row + nested_count >= row) {
					/* Row is inside nested table, recurse */
					nested_row = row - current_row;
					ctx->iteration_depth--;
					return table_selection_get_node_at_row(ctx, nested_table,
					                                       nested_row,
					                                       hnode_out,
					                                       htable_out);
				}

				current_row += nested_count;
			}
		}

		nomad = (**nomad).sortedlink;
	}

	ctx->iteration_depth--;
	return false;  /* Row out of bounds */
}


long table_selection_get_row_for_node(table_selection_context_t *ctx,
                                      hdlhashtable htable,
                                      hdlhashnode target_node) {
	/*
	 * Find row number for a given node.
	 * Returns 1-based row number, or 0 if not found.
	 */
	long current_row;
	hdlhashnode nomad;
	hdlhashtable nested_table;
	long nested_result;

	if (ctx == NULL || htable == NULL || target_node == NULL) {
		return 0;
	}

	/* Check recursion depth before incrementing */
	if (ctx->iteration_depth >= TABLE_SELECTION_MAX_NESTING_DEPTH) {
		log_error(LOG_COMP_TABLE, "Table nesting too deep (max=%d)", TABLE_SELECTION_MAX_NESTING_DEPTH);
		return 0;
	}

	current_row = 0;
	nomad = (**htable).hfirstsort;

	ctx->iteration_depth++;

	while (nomad != NULL) {
		current_row++;

		if (nomad == target_node) {
			/* Found it! */
			ctx->iteration_depth--;
			return current_row;
		}

		/* Check if this is an expanded nested table */
		if (table_selection_is_table_value(&(**nomad).val, &nested_table)) {
			if (table_selection_is_expanded(ctx, nested_table)) {
				/* Check if target is in nested table */
				nested_result = table_selection_get_row_for_node(ctx, nested_table, target_node);
				if (nested_result > 0) {
					/* Found in nested table */
					ctx->iteration_depth--;
					return current_row + nested_result;
				}

				/* Not in nested table, skip past its rows */
				long skip_count;
				if (!table_selection_count_visible_rows(ctx, nested_table, &skip_count)) {
					ctx->iteration_depth--;
					return 0;  /* Error counting nested rows */
				}
				current_row += skip_count;
			}
		}

		nomad = (**nomad).sortedlink;
	}

	ctx->iteration_depth--;
	return 0;  /* Not found */
}


long table_selection_get_row_for_key(table_selection_context_t *ctx,
                                     hdlhashtable htable,
                                     const bigstring key) {
	/*
	 * Find row number for a given key.
	 * Returns 1-based row number, or 0 if not found.
	 */
	hdlhashnode hnode;

	if (ctx == NULL || htable == NULL || key == NULL) {
		return 0;
	}

	/* Find the node (use explicit table, not current context) */
	if (!hashtablelookupnode(htable, key, &hnode)) {
		return 0;
	}

	/* Get row for node */
	return table_selection_get_row_for_node(ctx, htable, hnode);
}


/*
 * INTERNAL HELPER FUNCTIONS
 */

static void table_selection_thread_cleanup(void *ctx) {
	/*
	 * Cleanup callback when thread exits.
	 * Called automatically by pthread TLS system.
	 *
	 * This is the last chance to free resources. If refcount > 1, it indicates
	 * a bug (missing release somewhere). We warn and force cleanup anyway.
	 */
	table_selection_context_t *context = (table_selection_context_t *)ctx;

	if (context != NULL) {
		if (context->refcount != 1) {
			log_warn(LOG_COMP_TABLE,
			         "Thread cleanup: context refcount is %u (expected 1) - possible leak",
			         (unsigned int)context->refcount);
		}

		log_trace(LOG_COMP_TABLE, "Thread cleanup: force-freeing selection context");

		/* Force cleanup regardless of refcount */
		if (context->selected_keys != NULL) {
			opdisposelist(context->selected_keys);
			context->selected_keys = NULL;
		}
		if (context->expanded_tables != NULL) {
			free(context->expanded_tables);
			context->expanded_tables = NULL;
		}
		free(context);
	}
}


static boolean table_selection_find_in_list(hdllistrecord hlist, const bigstring key, long *index_out) {
	/*
	 * Find a key in the selection list.
	 * Returns true if found, with optional index output (0-based).
	 */
	long ct;
	long i;
	bigstring listkey;

	if (hlist == NULL || key == NULL) {
		return false;
	}

	ct = opcountlistitems(hlist);

	for (i = 0; i < ct; i++) {
		/* List items are 1-based */
		if (opgetliststring(hlist, i + 1, NULL, listkey)) {
			if (equalstrings(key, listkey)) {
				if (index_out != NULL) {
					*index_out = i;
				}
				return true;
			}
		}
	}

	return false;
}


static boolean table_selection_is_table_value(tyvaluerecord *val, hdlhashtable *htable_out) {
	/*
	 * Check if a value record is a table (external value of type tableType).
	 * If so, returns true and sets htable_out to the table handle.
	 */
	hdlexternalvariable hv;

	if (val == NULL) {
		return false;
	}

	/* Must be external value type */
	if (val->valuetype != externalvaluetype) {
		return false;
	}

	/* Get external variable handle */
	hv = (hdlexternalvariable)val->data.externalvalue;
	if (hv == NULL) {
		return false;
	}

	/* Must be table processor type */
	if ((**hv).id != idtableprocessor) {
		return false;
	}

	/* Get table handle from variabledata */
	hdlhashtable htable = (hdlhashtable)(**hv).variabledata;
	if (htable == NULL) {
		log_error(LOG_COMP_TABLE, "External table variable has NULL variabledata");
		return false;
	}

	if (htable_out != NULL) {
		*htable_out = htable;
	}

	return true;
}
