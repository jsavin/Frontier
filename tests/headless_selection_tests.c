/*
 * headless_selection_tests.c - Unit tests for table selection infrastructure
 *
 * Tests the thread-local selection context for headless table operations.
 * See: planning/phase3/TABLE_VERBS_HEADLESS_SELECTION_MODEL.md
 *
 * NOTE: These tests focus on the selection infrastructure itself, not on
 * full hash table operations. Hash table integration will be tested separately
 * when the table verbs are implemented.
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "headless_selection.h"
#include "strings.h"
#include "lang.h"
#include "memory.h"
#include "ops.h"
#include "tablestructure.h"


/*
 * LIFECYCLE TESTS
 */

static void test_context_acquire_release(void) {
	/*
	 * Test basic context lifecycle.
	 */
	table_selection_context_t *ctx;

	/* Initialize subsystem */
	table_selection_init();

	/* Acquire context */
	ctx = table_selection_acquire();
	assert(ctx != NULL);
	assert(ctx->is_valid == true);
	assert(ctx->refcount == 1);
	assert(ctx->current_table == NULL);
	assert(ctx->selected_keys == NULL);
	assert(ctx->ct_selected == 0);
	assert(ctx->cursor_key[0] == 0);
	assert(ctx->cursor_node == NULL);
	assert(ctx->ct_expanded == 0);
	assert(ctx->max_expanded == 16);
	assert(ctx->expanded_tables != NULL);

	/* Release context */
	table_selection_release(ctx);

	printf("  ✓ Context acquire/release\n");
}


static void test_context_refcounting(void) {
	/*
	 * Test reference counting.
	 */
	table_selection_context_t *ctx1, *ctx2, *ctx3;

	/* Initialize subsystem */
	table_selection_init();

	/* Acquire context (refcount = 1) */
	ctx1 = table_selection_acquire();
	assert(ctx1 != NULL);
	assert(ctx1->refcount == 1);

	/* Acquire again from same thread (refcount = 2) */
	ctx2 = table_selection_acquire();
	assert(ctx2 == ctx1);  /* Same context */
	assert(ctx1->refcount == 2);

	/* Retain (refcount = 3) */
	ctx3 = table_selection_retain(ctx1);
	assert(ctx3 == ctx1);
	assert(ctx1->refcount == 3);

	/* Release once (refcount = 2) */
	table_selection_release(ctx1);
	/* Context should still be valid */

	/* Release twice more (refcount = 0, freed) */
	table_selection_release(ctx2);
	table_selection_release(ctx3);

	/* After last release, context should be freed and TLS cleared */

	printf("  ✓ Context refcounting\n");
}


static void test_context_reset(void) {
	/*
	 * Test context reset operations.
	 */
	table_selection_context_t *ctx;
	bigstring key;
	hdlhashtable dummy_table = (hdlhashtable)0x1234;  /* Dummy pointer for testing */

	table_selection_init();
	ctx = table_selection_acquire();

	/* Set up some state (without actual hash table operations) */
	copystring(BIGSTRING("\x04" "key1"), key);
	ctx->current_table = dummy_table;
	copystring(key, ctx->cursor_key);
	table_selection_add(ctx, key);
	table_selection_expand(ctx, dummy_table);

	assert(ctx->cursor_key[0] != 0);
	assert(ctx->ct_selected == 1);
	assert(ctx->ct_expanded == 1);

	/* Reset (clears selection/cursor, keeps expansion) */
	table_selection_reset(ctx);
	assert(ctx->cursor_key[0] == 0);
	assert(ctx->ct_selected == 0);
	assert(ctx->ct_expanded == 1);  /* Expansion preserved */

	/* Full reset (clears everything) */
	table_selection_reset_full(ctx);
	assert(ctx->cursor_key[0] == 0);
	assert(ctx->ct_selected == 0);
	assert(ctx->ct_expanded == 0);
	assert(ctx->current_table == NULL);

	table_selection_release(ctx);

	printf("  ✓ Context reset\n");
}


/*
 * EXPANSION STATE TESTS
 */

static void test_expansion_add_remove(void) {
	/*
	 * Test expansion state tracking.
	 */
	table_selection_context_t *ctx;
	hdlhashtable htable1 = (hdlhashtable)0x1000;  /* Dummy pointers */
	hdlhashtable htable2 = (hdlhashtable)0x2000;
	hdlhashtable htable3 = (hdlhashtable)0x3000;

	table_selection_init();
	ctx = table_selection_acquire();

	/* Initially not expanded */
	assert(table_selection_is_expanded(ctx, htable1) == false);
	assert(table_selection_is_expanded(ctx, htable2) == false);

	/* Expand table1 */
	assert(table_selection_expand(ctx, htable1) == true);  /* Newly expanded */
	assert(table_selection_is_expanded(ctx, htable1) == true);
	assert(ctx->ct_expanded == 1);

	/* Expand again (should return false) */
	assert(table_selection_expand(ctx, htable1) == false);  /* Already expanded */
	assert(ctx->ct_expanded == 1);

	/* Expand table2 and table3 */
	assert(table_selection_expand(ctx, htable2) == true);
	assert(table_selection_expand(ctx, htable3) == true);
	assert(ctx->ct_expanded == 3);

	/* Collapse table2 */
	assert(table_selection_collapse(ctx, htable2) == true);
	assert(table_selection_is_expanded(ctx, htable2) == false);
	assert(ctx->ct_expanded == 2);

	/* Collapse again (should return false) */
	assert(table_selection_collapse(ctx, htable2) == false);
	assert(ctx->ct_expanded == 2);

	/* Collapse all */
	assert(table_selection_collapse(ctx, htable1) == true);
	assert(table_selection_collapse(ctx, htable3) == true);
	assert(ctx->ct_expanded == 0);

	table_selection_release(ctx);

	printf("  ✓ Expansion add/remove\n");
}


static void test_expansion_array_growth(void) {
	/*
	 * Test expansion array auto-growth.
	 */
	table_selection_context_t *ctx;
	hdlhashtable tables[20];
	int i;

	table_selection_init();
	ctx = table_selection_acquire();

	/* Initial capacity is 16, create 20 dummy tables to force growth */
	for (i = 0; i < 20; i++) {
		tables[i] = (hdlhashtable)(0x1000 + i * 0x100);  /* Dummy pointers */
		assert(table_selection_expand(ctx, tables[i]) == true);
	}

	assert(ctx->ct_expanded == 20);
	assert(ctx->max_expanded >= 20);  /* Array should have grown */

	/* Verify all are still marked as expanded */
	for (i = 0; i < 20; i++) {
		assert(table_selection_is_expanded(ctx, tables[i]) == true);
	}

	table_selection_release(ctx);

	printf("  ✓ Expansion array growth\n");
}


/*
 * MULTI-SELECTION TESTS
 */

static void test_selection_add_remove(void) {
	/*
	 * Test multi-selection list management.
	 */
	table_selection_context_t *ctx;
	bigstring key1, key2, key3;

	table_selection_init();
	ctx = table_selection_acquire();

	copystring(BIGSTRING("\x04" "key1"), key1);
	copystring(BIGSTRING("\x04" "key2"), key2);
	copystring(BIGSTRING("\x04" "key3"), key3);

	/* Initially no selections */
	assert(table_selection_get_count(ctx) == 0);
	assert(table_selection_is_selected(ctx, key1) == false);

	/* Add key1 */
	assert(table_selection_add(ctx, key1) == true);
	assert(table_selection_is_selected(ctx, key1) == true);
	assert(table_selection_get_count(ctx) == 1);

	/* Add again (should return false) */
	assert(table_selection_add(ctx, key1) == false);
	assert(table_selection_get_count(ctx) == 1);

	/* Add key2 and key3 */
	assert(table_selection_add(ctx, key2) == true);
	assert(table_selection_add(ctx, key3) == true);
	assert(table_selection_get_count(ctx) == 3);

	/* Remove key2 */
	assert(table_selection_remove(ctx, key2) == true);
	assert(table_selection_is_selected(ctx, key2) == false);
	assert(table_selection_get_count(ctx) == 2);

	/* Remove again (should return false) */
	assert(table_selection_remove(ctx, key2) == false);
	assert(table_selection_get_count(ctx) == 2);

	/* Clear all */
	table_selection_clear(ctx);
	assert(table_selection_get_count(ctx) == 0);
	assert(table_selection_is_selected(ctx, key1) == false);
	assert(table_selection_is_selected(ctx, key3) == false);

	table_selection_release(ctx);

	printf("  ✓ Selection add/remove\n");
}


/*
 * NOTE: Cursor and row counting tests require full hash table integration.
 * These will be tested as part of the table verbs implementation in Phase 2+.
 * For now, we just test the infrastructure without hash table dependencies.
 */


/*
 * MAIN TEST RUNNER
 */

int main(void) {
	/* Initialize Frontier runtime (required for outline operations) */
	assert(initmemory());
	initstrings();
	assert(initlang());
	/* opinit() doesn't exist - outline init happens via langinitverbs */
	assert(inittablestructure());
	assert(langinitverbs());

	printf("Running headless selection infrastructure tests...\n\n");
	printf("NOTE: These tests focus on the selection context infrastructure.\n");
	printf("Full hash table integration will be tested with table verb implementation.\n\n");

	printf("Lifecycle tests:\n");
	test_context_acquire_release();
	test_context_refcounting();
	test_context_reset();

	printf("\nExpansion state tests:\n");
	test_expansion_add_remove();
	test_expansion_array_growth();

	printf("\nMulti-selection tests:\n");
	test_selection_add_remove();

	printf("\n✓ All headless selection infrastructure tests passed!\n");
	printf("✓ Phase 1: Core Selection Infrastructure - COMPLETE\n");
	return 0;
}
