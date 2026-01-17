/*
 * headless_thread_registry_tests.c - Thread Registry Unit Tests (Layer 1)
 *
 * Deterministic unit tests for the thread registry component.
 * These tests run in a single thread and verify basic functionality
 * without any threading complexity.
 *
 * Test Categories:
 * 1. Initialization/Cleanup lifecycle
 * 2. Thread ID allocation (uniqueness, monotonicity)
 * 3. Record allocation and deallocation
 * 4. Lookup operations (success and failure cases)
 * 5. Edge cases (NULL safety, double-free)
 *
 * Reference: planning/phase3/THREAD_SAFETY_PHASE1_PLAN.md
 *
 * Author: Frontier Development Team
 * Date: 2026-01-16
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "threadregistry.h"

/* Test counter */
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) static void test_##name(void)
#define RUN_TEST(name) do { \
    printf("  Running %s... ", #name); \
    test_##name(); \
    tests_passed++; \
    printf("PASSED\n"); \
} while(0)

#define ASSERT(cond) do { \
    if (!(cond)) { \
        printf("FAILED at %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        tests_failed++; \
        return; \
    } \
} while(0)

#define ASSERT_EQ(a, b) ASSERT((a) == (b))
#define ASSERT_NE(a, b) ASSERT((a) != (b))
#define ASSERT_NULL(p) ASSERT((p) == NULL)
#define ASSERT_NOT_NULL(p) ASSERT((p) != NULL)

/*
 * Test 1: Basic initialization and cleanup
 */
TEST(init_and_cleanup) {
    boolean result = init_thread_registry();
    ASSERT_EQ(result, true);

    /* After init, count should be 0 */
    ASSERT_EQ(get_thread_count(), 0);

    cleanup_thread_registry();
    /* After cleanup, operations should fail gracefully */
}

/*
 * Test 2: Thread ID allocation produces unique, increasing IDs
 */
TEST(id_allocation_unique) {
    init_thread_registry();

    long id1 = allocate_thread_id();
    long id2 = allocate_thread_id();
    long id3 = allocate_thread_id();

    /* IDs must be positive */
    ASSERT(id1 > 0);
    ASSERT(id2 > 0);
    ASSERT(id3 > 0);

    /* IDs must be unique */
    ASSERT_NE(id1, id2);
    ASSERT_NE(id2, id3);
    ASSERT_NE(id1, id3);

    /* IDs must be monotonically increasing */
    ASSERT(id2 > id1);
    ASSERT(id3 > id2);

    cleanup_thread_registry();
}

/*
 * Test 3: Record allocation returns valid record with correct initial state
 */
TEST(record_allocation) {
    init_thread_registry();

    frontier_pthread_record *rec = allocate_thread_record();
    ASSERT_NOT_NULL(rec);

    /* Check initial state */
    ASSERT(rec->user_thread_id > 0);
    ASSERT_EQ(rec->in_use, true);
    ASSERT_EQ(rec->is_sleeping, false);
    ASSERT_EQ(rec->is_killed, false);
    ASSERT_EQ(rec->wakeup_ticks, 0UL);
    ASSERT_NULL(rec->hglobals);

    /* Count should be 1 */
    ASSERT_EQ(get_thread_count(), 1);

    cleanup_thread_registry();
}

/*
 * Test 4: Record free decrements count
 */
TEST(record_free) {
    init_thread_registry();

    frontier_pthread_record *rec = allocate_thread_record();
    ASSERT_NOT_NULL(rec);
    ASSERT_EQ(get_thread_count(), 1);

    free_thread_record(rec);
    ASSERT_EQ(get_thread_count(), 0);

    cleanup_thread_registry();
}

/*
 * Test 5: Lookup by ID succeeds for allocated record
 */
TEST(lookup_success) {
    init_thread_registry();

    frontier_pthread_record *rec = allocate_thread_record();
    ASSERT_NOT_NULL(rec);
    long id = rec->user_thread_id;

    frontier_pthread_record *found = get_thread_by_id(id);
    ASSERT_EQ(found, rec);

    cleanup_thread_registry();
}

/*
 * Test 6: Lookup by ID fails for non-existent ID
 */
TEST(lookup_failure) {
    init_thread_registry();

    /* No records allocated yet */
    frontier_pthread_record *found = get_thread_by_id(999);
    ASSERT_NULL(found);

    /* Invalid IDs */
    found = get_thread_by_id(0);
    ASSERT_NULL(found);

    found = get_thread_by_id(-1);
    ASSERT_NULL(found);

    cleanup_thread_registry();
}

/*
 * Test 7: Multiple records can be allocated
 */
TEST(multiple_records) {
    init_thread_registry();

    frontier_pthread_record *rec1 = allocate_thread_record();
    frontier_pthread_record *rec2 = allocate_thread_record();
    frontier_pthread_record *rec3 = allocate_thread_record();

    ASSERT_NOT_NULL(rec1);
    ASSERT_NOT_NULL(rec2);
    ASSERT_NOT_NULL(rec3);

    /* All should be different */
    ASSERT_NE(rec1, rec2);
    ASSERT_NE(rec2, rec3);
    ASSERT_NE(rec1, rec3);

    /* All should have different IDs */
    ASSERT_NE(rec1->user_thread_id, rec2->user_thread_id);
    ASSERT_NE(rec2->user_thread_id, rec3->user_thread_id);

    /* Count should be 3 */
    ASSERT_EQ(get_thread_count(), 3);

    /* All should be findable */
    ASSERT_EQ(get_thread_by_id(rec1->user_thread_id), rec1);
    ASSERT_EQ(get_thread_by_id(rec2->user_thread_id), rec2);
    ASSERT_EQ(get_thread_by_id(rec3->user_thread_id), rec3);

    cleanup_thread_registry();
}

/*
 * Test 8: Field initialization is correct
 */
TEST(field_initialization) {
    init_thread_registry();

    frontier_pthread_record *rec = allocate_thread_record();
    ASSERT_NOT_NULL(rec);

    /* Verify all fields are properly initialized */
    ASSERT(rec->user_thread_id >= 1);  /* First ID should be 1 */
    ASSERT_EQ(rec->in_use, true);
    ASSERT_EQ(rec->is_sleeping, false);
    ASSERT_EQ(rec->is_killed, false);
    ASSERT_EQ(rec->wakeup_ticks, 0UL);
    ASSERT_NULL(rec->hglobals);

    /* state_mutex and wake_cond should be initialized (we can't easily test this
     * without actually using them, but we verify no crash on cleanup) */

    cleanup_thread_registry();
}

/*
 * Test 9: Reinitialize after cleanup works correctly
 *
 * Verifies that after cleanup and reinit:
 * - The registry is empty (count = 0)
 * - ID counter resets to 1
 * - New allocations work correctly
 */
TEST(reinit_after_cleanup) {
    /* First lifecycle - allocate some records */
    init_thread_registry();
    frontier_pthread_record *rec1 = allocate_thread_record();
    ASSERT_NOT_NULL(rec1);
    long id1 = rec1->user_thread_id;
    ASSERT_EQ(id1, 1);  /* First ID should be 1 */
    ASSERT_EQ(get_thread_count(), 1);
    cleanup_thread_registry();

    /* After cleanup, registry should be empty */
    /* Note: get_thread_count behavior after cleanup is implementation-defined,
     * but registry_initialized is false so allocate should return NULL */

    /* Second lifecycle - IDs should restart from 1 */
    init_thread_registry();

    /* Registry should be empty after fresh init */
    ASSERT_EQ(get_thread_count(), 0);

    frontier_pthread_record *rec2 = allocate_thread_record();
    ASSERT_NOT_NULL(rec2);

    /* ID counter resets on init, so new record gets ID 1 again */
    ASSERT_EQ(rec2->user_thread_id, 1);
    ASSERT_EQ(get_thread_count(), 1);

    /* If we look up ID 1, we should find rec2 (the new record, not the old one) */
    ASSERT_EQ(get_thread_by_id(1), rec2);

    cleanup_thread_registry();
}

/*
 * Test 10: Slot reuse after free
 */
TEST(slot_reuse) {
    init_thread_registry();

    /* Allocate and free a record */
    frontier_pthread_record *rec1 = allocate_thread_record();
    ASSERT_NOT_NULL(rec1);
    long id1 = rec1->user_thread_id;
    free_thread_record(rec1);

    /* Allocate a new record - should reuse the slot */
    frontier_pthread_record *rec2 = allocate_thread_record();
    ASSERT_NOT_NULL(rec2);

    /* Same slot, but different ID (IDs never reuse) */
    ASSERT_EQ(rec2, rec1);  /* Same slot pointer */
    ASSERT_NE(rec2->user_thread_id, id1);  /* Different ID */

    /* Old ID should not be found */
    ASSERT_NULL(get_thread_by_id(id1));

    cleanup_thread_registry();
}

/*
 * Test 11: NULL free is safe (no crash)
 */
TEST(null_free_safety) {
    init_thread_registry();

    /* Should not crash */
    free_thread_record(NULL);

    /* Registry should still work */
    frontier_pthread_record *rec = allocate_thread_record();
    ASSERT_NOT_NULL(rec);
    ASSERT_EQ(get_thread_count(), 1);

    cleanup_thread_registry();
}

/*
 * Test 12: Double free is safe (no crash, no corruption)
 */
TEST(double_free_safety) {
    init_thread_registry();

    frontier_pthread_record *rec = allocate_thread_record();
    ASSERT_NOT_NULL(rec);
    long id = rec->user_thread_id;

    /* First free */
    free_thread_record(rec);
    ASSERT_EQ(get_thread_count(), 0);
    ASSERT_NULL(get_thread_by_id(id));

    /* Second free - should be no-op */
    free_thread_record(rec);
    ASSERT_EQ(get_thread_count(), 0);

    /* Registry should still work */
    frontier_pthread_record *rec2 = allocate_thread_record();
    ASSERT_NOT_NULL(rec2);
    ASSERT_EQ(get_thread_count(), 1);

    cleanup_thread_registry();
}

/*
 * Test 13: Operations fail gracefully before init
 */
TEST(pre_init_safety) {
    /* Note: This test assumes cleanup was called or registry was never initialized.
     * We explicitly do NOT call init here. */

    /* allocate should return NULL */
    frontier_pthread_record *rec = allocate_thread_record();
    ASSERT_NULL(rec);

    /* lookup should return NULL */
    frontier_pthread_record *found = get_thread_by_id(1);
    ASSERT_NULL(found);

    /* Now init and verify it works */
    init_thread_registry();
    rec = allocate_thread_record();
    ASSERT_NOT_NULL(rec);

    cleanup_thread_registry();
}

/*
 * Test 14: Lookup after free returns NULL
 *
 * With reference counting:
 * - allocate_thread_record() gives you refcount=1
 * - get_thread_by_id() increments refcount (you must release it)
 * - free_thread_record() decrements your original refcount
 * - Record only becomes unfindable when refcount=0
 */
TEST(lookup_after_free) {
    init_thread_registry();

    frontier_pthread_record *rec = allocate_thread_record();
    ASSERT_NOT_NULL(rec);
    long id = rec->user_thread_id;

    /* Can find before free */
    frontier_pthread_record *found = get_thread_by_id(id);
    ASSERT_EQ(found, rec);
    /* Release the reference from get_thread_by_id() */
    release_thread_record(found);

    /* Now free our own reference */
    free_thread_record(rec);

    /* Cannot find after all references released */
    ASSERT_NULL(get_thread_by_id(id));

    cleanup_thread_registry();
}

/*
 * Test 15: MAX_THREADS boundary - allocate all 64 slots, then fail on 65th
 */
TEST(max_threads_boundary) {
    init_thread_registry();

    /* Allocate all 64 allowed slots */
    frontier_pthread_record *records[64];
    int i;
    for (i = 0; i < 64; i++) {
        records[i] = allocate_thread_record();
        ASSERT_NOT_NULL(records[i]);
    }

    /* Verify we can't allocate the 65th */
    frontier_pthread_record *overflow = allocate_thread_record();
    ASSERT_NULL(overflow);

    /* Free one slot and verify we can allocate a new one */
    free_thread_record(records[0]);
    frontier_pthread_record *new_rec = allocate_thread_record();
    ASSERT_NOT_NULL(new_rec);

    /* Clean up all remaining records */
    for (i = 1; i < 64; i++) {
        free_thread_record(records[i]);
    }
    free_thread_record(new_rec);

    cleanup_thread_registry();
}

/*
 * Main test runner
 */
int main(void) {
    printf("Thread Registry Unit Tests (Layer 1)\n");
    printf("=====================================\n\n");

    /* Ensure registry is in clean state before first test */
    cleanup_thread_registry();

    RUN_TEST(init_and_cleanup);
    RUN_TEST(id_allocation_unique);
    RUN_TEST(record_allocation);
    RUN_TEST(record_free);
    RUN_TEST(lookup_success);
    RUN_TEST(lookup_failure);
    RUN_TEST(multiple_records);
    RUN_TEST(field_initialization);
    RUN_TEST(reinit_after_cleanup);
    RUN_TEST(slot_reuse);
    RUN_TEST(null_free_safety);
    RUN_TEST(double_free_safety);
    RUN_TEST(pre_init_safety);
    RUN_TEST(lookup_after_free);
    RUN_TEST(max_threads_boundary);

    printf("\n=====================================\n");
    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
