/*
 * table_context.c - Version and mutation tracking for hash tables
 *
 * Phase 4A: Table Operation Context Pattern
 * Part of Issue #135 - Operation Context for Outline Refactoring
 *
 * Implementation of table_context_t lifecycle and mutation tracking.
 */

#include "table_context.h"
#include "logging.h"
#include "lang.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* ============================================================================
   LIFECYCLE FUNCTIONS
   ============================================================================ */

boolean table_context_init(table_context_t **pctx) {
	table_context_t *ctx;

	if (pctx == NULL)
		return false;

	/* Allocate context structure */
	ctx = (table_context_t *)malloc(sizeof(table_context_t));
	if (ctx == NULL) {
		log_error(LOG_COMP_TABLE, "table_context_init: malloc failed");
		return false;
	}

	/* Initialize all fields */
	memset(ctx, 0, sizeof(table_context_t));
	ctx->version_number = 1;           /* Start at 1, not 0 */
	ctx->last_mutation_time = 0;
	ctx->last_mutation_type = table_mutation_none;
	ctx->flpendingchanges = false;
	ctx->flingcallback = false;
	ctx->_reserved1 = 0;
	ctx->_reserved2 = 0;

	*pctx = ctx;

	log_trace(LOG_COMP_TABLE, "table_context_init: created ctx=%p version=1", (void *)ctx);

	return true;
}

void table_context_dispose(table_context_t *ctx) {
	if (ctx == NULL)
		return;

	log_trace(LOG_COMP_TABLE, "table_context_dispose: ctx=%p version=%llu",
	          (void *)ctx, (unsigned long long)ctx->version_number);

	free(ctx);
}

/* ============================================================================
   MUTATION TRACKING FUNCTIONS
   ============================================================================ */

void table_context_record_mutation(struct hdlhashtable *htable,
                                   enum table_mutation_type type) {
	table_context_t *ctx;

	if (htable == NULL)
		return;

	/* Access context stored in table via unsafe cast (caller ensures validity) */
	ctx = (*(struct tyhashtable **) htable)->context;

	if (ctx == NULL) {
		log_warn(LOG_COMP_TABLE, "table_context_record_mutation: table has no context");
		return;
	}

	/* Don't bump version if we're already in a callback (prevents double-bumping) */
	if (ctx->flingcallback) {
		log_trace(LOG_COMP_TABLE, "table_context_record_mutation: skipping (in callback)");
		return;
	}

	/* Record mutation metadata */
	ctx->version_number++;
	ctx->last_mutation_type = (uint8_t)type;
	ctx->last_mutation_time = time(NULL);
	ctx->flpendingchanges = true;

	log_debug(LOG_COMP_TABLE, "table_context_record_mutation: type=%d version=%llu",
	          type, (unsigned long long)ctx->version_number);
}

boolean table_context_has_changes(struct hdlhashtable *htable) {
	table_context_t *ctx;

	if (htable == NULL)
		return false;

	/* Access context stored in table via unsafe cast */
	ctx = (*(struct tyhashtable **) htable)->context;

	if (ctx == NULL)
		return false;

	return ctx->flpendingchanges;
}

void table_context_clear_changes(struct hdlhashtable *htable) {
	table_context_t *ctx;

	if (htable == NULL)
		return;

	/* Access context stored in table via unsafe cast */
	ctx = (*(struct tyhashtable **) htable)->context;

	if (ctx == NULL)
		return;

	ctx->flpendingchanges = false;

	log_trace(LOG_COMP_TABLE, "table_context_clear_changes");
}

/* ============================================================================
   CALLBACK GUARD FUNCTIONS
   ============================================================================ */

void table_context_enter_callbacks(struct hdlhashtable *htable) {
	table_context_t *ctx;

	if (htable == NULL)
		return;

	/* Access context stored in table via unsafe cast */
	ctx = (*(struct tyhashtable **) htable)->context;

	if (ctx == NULL) {
		log_warn(LOG_COMP_TABLE, "table_context_enter_callbacks: table has no context");
		return;
	}

	/* Set flag to prevent callbacks from bumping version */
	ctx->flingcallback = true;

	log_trace(LOG_COMP_TABLE, "table_context_enter_callbacks");
}

void table_context_exit_callbacks(struct hdlhashtable *htable) {
	table_context_t *ctx;

	if (htable == NULL)
		return;

	/* Access context stored in table via unsafe cast */
	ctx = (*(struct tyhashtable **) htable)->context;

	if (ctx == NULL) {
		log_warn(LOG_COMP_TABLE, "table_context_exit_callbacks: table has no context");
		return;
	}

	/* Clear flag to allow callbacks to bump version again */
	ctx->flingcallback = false;

	log_trace(LOG_COMP_TABLE, "table_context_exit_callbacks");
}
