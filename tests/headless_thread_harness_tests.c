/*
 * headless_thread_harness_tests.c - Thread Test Harness Unit Tests
 *
 * Deterministic unit tests for the thread test harness component.
 * Verifies that virtual time control works correctly for testing
 * thread sleep and timeout behavior.
 *
 * ENVIRONMENT REQUIREMENT:
 * These tests require the environment variable FRONTIER_THREAD_TEST_MODE=1
 * to be set. Without it, tests will fail to enable test harness.
 *
 * To run:
 *   FRONTIER_THREAD_TEST_MODE=1 ./tests/headless_thread_harness_tests
 *
 * Test Categories:
 * 1. Enable/disable lifecycle (with proper state reset)
 * 2. Virtual tick manipulation (get, set, advance)
 * 3. Time freezing behavior
 * 4. Edge cases (wraparound, disable while running)
 *
 * NOTE: Thread-safety tests are deferred (Issue #322)
 * These are single-threaded unit tests only.
 *
 * Author: Frontier Development Team
 * Date: 2026-01-17
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "shellthreads_test_harness.h"

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
#define ASSERT_TRUE(x) ASSERT((x) == true)
#define ASSERT_FALSE(x) ASSERT((x) == false)

/*
 * Test 1: Enable/disable lifecycle
 */
TEST(enable_disable) {
    boolean result;

    /* Should be disabled initially */
    ASSERT_FALSE(thread_test_is_enabled());

    /* Enable test mode */
    result = thread_test_enable();
    ASSERT_TRUE(result);
    ASSERT_TRUE(thread_test_is_enabled());

    /* Disable test mode */
    result = thread_test_disable();
    ASSERT_TRUE(result);
    ASSERT_FALSE(thread_test_is_enabled());
}

/*
 * Test 2: Virtual tick manipulation
 */
TEST(tick_manipulation) {
    boolean result;
    uint32_t ticks;

    thread_test_enable();

    /* Set ticks to specific value */
    result = thread_test_set_ticks(1000);
    ASSERT_TRUE(result);

    /* Read the ticks back */
    ticks = thread_test_get_ticks();
    ASSERT_EQ(ticks, 1000U);

    /* Advance by delta */
    result = thread_test_advance(100);
    ASSERT_TRUE(result);

    ticks = thread_test_get_ticks();
    ASSERT_EQ(ticks, 1100U);

    thread_test_disable();
}

/*
 * Test 3: Operations fail when harness disabled
 */
TEST(disabled_operations_fail) {
    boolean result;

    /* Make sure harness is disabled */
    thread_test_disable();

    /* Advance should fail */
    result = thread_test_advance(100);
    ASSERT_FALSE(result);

    /* Set ticks should fail */
    result = thread_test_set_ticks(500);
    ASSERT_FALSE(result);

    /* Get ticks should still work (returns 0 for test mode inactive) */
    uint32_t ticks = thread_test_get_ticks();
    ASSERT_EQ(ticks, 0U);
}

/*
 * Test 4: Large tick values and wraparound
 */
TEST(large_tick_values) {
    boolean result;
    uint32_t ticks;

    thread_test_enable();

    /* Set to near max uint32_t */
    result = thread_test_set_ticks(0xFFFFFF00U);
    ASSERT_TRUE(result);

    ticks = thread_test_get_ticks();
    ASSERT_EQ(ticks, 0xFFFFFF00U);

    /* Advance (may wrap around in uint32_t arithmetic) */
    result = thread_test_advance(256);
    ASSERT_TRUE(result);

    ticks = thread_test_get_ticks();
    ASSERT_EQ(ticks, 0U);  /* Wrapped around */

    thread_test_disable();
}

/*
 * Test 5: Sequential enable/disable/enable
 */
TEST(repeated_enable_disable) {
    boolean result;
    uint32_t ticks;

    /* First cycle */
    result = thread_test_enable();
    ASSERT_TRUE(result);

    result = thread_test_set_ticks(100);
    ASSERT_TRUE(result);

    result = thread_test_disable();
    ASSERT_TRUE(result);

    /* Second cycle - should start fresh with zeroed state */
    result = thread_test_enable();
    ASSERT_TRUE(result);

    ticks = thread_test_get_ticks();
    ASSERT_EQ(ticks, 0U);  /* Each enable starts fresh - state reset on disable */

    thread_test_disable();
}

/*
 * Main test runner
 */
int main(void) {
    const char *env = getenv("FRONTIER_THREAD_TEST_MODE");

    /* Require environment variable to be set - tests expect test mode to work */
    if (env == NULL || strcmp(env, "1") != 0) {
        printf("ERROR: Tests require FRONTIER_THREAD_TEST_MODE=1 environment variable\n");
        printf("Run with: FRONTIER_THREAD_TEST_MODE=1 ./tests/headless_thread_harness_tests\n");
        return 1;
    }

    printf("Thread Test Harness Unit Tests\n");
    printf("===============================\n\n");

    RUN_TEST(enable_disable);
    RUN_TEST(tick_manipulation);
    RUN_TEST(disabled_operations_fail);
    RUN_TEST(large_tick_values);
    RUN_TEST(repeated_enable_disable);

    printf("\n");
    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);

    return (tests_failed == 0) ? 0 : 1;
}
