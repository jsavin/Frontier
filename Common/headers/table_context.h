/*
 * table_context.h - Version and mutation tracking for hash tables
 *
 * Phase 4A: Table Operation Context Pattern
 * Part of Issue #135 - Operation Context for Outline Refactoring
 *
 * This structure is allocated once per table and stores mutation tracking
 * metadata. It enables version-based change detection and forms the foundation
 * for future collaborative editing on tables.
 *
 * Design: Context stored IN table structure (tytablevariable.context)
 * This avoids deep parameter threading while maintaining explicit tracking.
 */

#ifndef TABLE_CONTEXT_INCLUDE
#define TABLE_CONTEXT_INCLUDE

#include <time.h>
#include <stdint.h>

/* Define boolean type (unsigned char for bitfield compatibility) */
#ifndef boolean
	typedef unsigned char boolean;
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
   MUTATION TYPE ENUMERATION
   ============================================================================ */

enum table_mutation_type {
	table_mutation_none = 0,
	table_mutation_insert = 1,       /* Key inserted into table */
	table_mutation_delete = 2,       /* Key deleted from table */
	table_mutation_modify = 3,       /* Value modified (table.assign) */
	table_mutation_move = 4,         /* Key moved to different location */
	table_mutation_copy = 5,         /* Key copied */
	table_mutation_rename = 6,       /* Key renamed */
	table_mutation_moveandrename = 7,/* Key moved and renamed */
	table_mutation_emptytable = 8,   /* Table cleared */
};

/* ============================================================================
   TABLE_CONTEXT_T STRUCTURE

   Version and mutation tracking for hash tables

   Design principles:
   - Stored IN the table structure (efficient access, no threading)
   - Version counter monotonically increasing
   - Tracks last mutation time and type for debugging
   - Callback guard prevents double-bumping during nested operations
   - Reserved fields for Phase 6+ CRDT metadata
   ============================================================================ */

typedef struct table_context {

	/*
	 * VERSION TRACKING
	 * ================
	 * version_number: Monotonically increasing counter incremented on each
	 * user-visible table mutation (table.assign, table.move, table.delete).
	 * Used by callers to detect if a table has changed since last read.
	 */
	uint64_t version_number;

	/*
	 * MUTATION METADATA
	 * =================
	 * last_mutation_time: Unix timestamp of last user operation that changed
	 * the table. Used for change detection and debugging.
	 */
	time_t last_mutation_time;

	/*
	 * last_mutation_type: Enum indicating the type of last mutation.
	 * Useful for debugging and change tracking.
	 */
	uint8_t last_mutation_type;

	/*
	 * CHANGE ACCUMULATION
	 * ===================
	 * flpendingchanges: If true, table has changes not yet written to disk.
	 * Set by table_context_record_mutation() when table is modified.
	 * Cleared when table is packed/saved.
	 */
	boolean flpendingchanges;

	/*
	 * CALLBACK GUARD
	 * ==============
	 * flingcallback: Flag set to true while callbacks are executing for a
	 * single top-level operation. Prevents multiple version bumps during
	 * a single operation that triggers multiple callbacks.
	 *
	 * Example: table.move triggers:
	 *   1. tablesymboldeleted() on source
	 *   2. tablesymbolinserted() on destination
	 * We only want to bump version once, not twice.
	 */
	boolean flingcallback;

	/*
	 * RESERVED FIELDS
	 * ===============
	 * Future expansion slots for additional tracking without ABI break.
	 */
	uint32_t _reserved1;
	uint32_t _reserved2;

} table_context_t, *ptrtable_context, **hdltable_context;

/* ============================================================================
   LIFECYCLE API
   ============================================================================ */

/*
 * table_context_init - Initialize new table context
 *
 * @param pctx - Pointer to context pointer (will be allocated)
 * @return true if successful, false on allocation failure
 *
 * Called when table is created. Initializes version to 1.
 */
boolean table_context_init(table_context_t **pctx);

/*
 * table_context_dispose - Dispose table context
 *
 * @param ctx - Context to dispose (may be NULL)
 *
 * Safe to call with NULL. Called when table is destroyed.
 */
void table_context_dispose(table_context_t *ctx);

/* ============================================================================
   MUTATION TRACKING API
   ============================================================================ */

/*
 * table_context_record_mutation - Record a table mutation
 *
 * @param htable - Hash table being mutated (NULL safe)
 * @param type - Type of mutation (from enum table_mutation_type)
 *
 * Called at explicit user-facing entry points (table.assign, table.move, etc.)
 * Increments version and records mutation metadata.
 * Safe to call with NULL (no-op).
 */
void table_context_record_mutation(struct hdlhashtable *htable,
                                   enum table_mutation_type type);

/*
 * table_context_has_changes - Check for pending changes
 *
 * @param htable - Hash table to check
 * @return true if table has unflushed changes
 */
boolean table_context_has_changes(struct hdlhashtable *htable);

/*
 * table_context_clear_changes - Clear pending changes flag
 *
 * @param htable - Hash table to clear changes on
 *
 * Called after table is packed/saved to disk.
 */
void table_context_clear_changes(struct hdlhashtable *htable);

/* ============================================================================
   CALLBACK GUARD API

   Used to prevent double-bumping of versions when multiple callbacks
   fire during a single top-level operation.
   ============================================================================ */

/*
 * table_context_enter_callbacks - Mark entry into callback section
 *
 * @param htable - Hash table being operated on
 *
 * Call before performing hash operation that will trigger callbacks.
 * Sets flingcallback=true to prevent callbacks from re-bumping version.
 */
void table_context_enter_callbacks(struct hdlhashtable *htable);

/*
 * table_context_exit_callbacks - Mark exit from callback section
 *
 * @param htable - Hash table being operated on
 *
 * Call after hash operation completes. Clears flingcallback flag.
 */
void table_context_exit_callbacks(struct hdlhashtable *htable);

#ifdef __cplusplus
}
#endif

#endif /* TABLE_CONTEXT_INCLUDE */
