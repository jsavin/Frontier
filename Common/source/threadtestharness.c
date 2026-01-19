/*
 * threadtestharness.c - Thread Test Harness Implementation
 *
 * Provides deterministic tick control for thread integration tests.
 * Allows tests to freeze system time and advance it manually.
 *
 * Implementation Details:
 * - Virtual tick counter (thread_test_harness.virtual_ticks)
 * - Environment variable gate (FRONTIER_THREAD_TEST_MODE=1)
 * - Controlled advance + explicit event loop iteration
 * - All state is runtime-only (not persisted to ODB)
 *
 * Author: Claude (Frontier Dev Team)
 * Date: 2026-01-17
 */

#include "threadtestharness.h"
#include "logging.h"
#include <stdlib.h>
#include <string.h>

/*
 * Test harness state - all runtime-only, not persisted
 */
typedef struct {
    unsigned char enabled;              /* Test mode active */
    uint32_t virtual_ticks;             /* Current virtual tick count */
} thread_test_harness_t;

static thread_test_harness_t thread_test_harness = {0, 0};

/*
 * thread_test_enable - Activate test harness mode
 */
unsigned char thread_test_enable(void) {
    const char *env = getenv("FRONTIER_THREAD_TEST_MODE");

    /* Safety gate: only enable if environment variable explicitly set */
    if (env == NULL || strcmp(env, "1") != 0) {
        return 0;  /* Silently fail if not in test mode */
    }

    thread_test_harness.enabled = 1;
    thread_test_harness.virtual_ticks = 0;

    /* Warning: visible in logs if accidentally enabled */
    log_warn(LOG_COMP_LANG, "THREAD TEST MODE ACTIVE - NOT FOR PRODUCTION USE");

    return 1;
}

/*
 * thread_test_disable - Return to normal operation
 */
unsigned char thread_test_disable(void) {
    if (!thread_test_harness.enabled) {
        return 0;  /* Already disabled */
    }

    thread_test_harness.enabled = 0;
    thread_test_harness.virtual_ticks = 0;

    return 1;
}

/*
 * thread_test_set_ticks - Set virtual clock to absolute value
 */
unsigned char thread_test_set_ticks(uint32_t ticks) {
    if (!thread_test_harness.enabled) {
        return 0;  /* Test mode not enabled */
    }

    thread_test_harness.virtual_ticks = ticks;
    return 1;
}

/*
 * thread_test_get_ticks - Read current virtual tick count
 */
uint32_t thread_test_get_ticks(void) {
    if (!thread_test_harness.enabled) {
        return 0;  /* Return 0 if not in test mode */
    }

    return thread_test_harness.virtual_ticks;
}

/*
 * thread_test_advance - Increment virtual clock by delta ticks
 */
unsigned char thread_test_advance(uint32_t delta) {
    if (!thread_test_harness.enabled) {
        return 0;  /* Test mode not enabled */
    }

    /* Check for overflow before incrementing */
    if (thread_test_harness.virtual_ticks > UINT32_MAX - delta) {
        log_error(LOG_COMP_LANG, "thread_test_advance: virtual tick overflow detected");
        return 0;
    }

    thread_test_harness.virtual_ticks += delta;
    return 1;
}

/*
 * thread_test_process_once - Single iteration of thread event loop
 *
 * In current implementation, this is a placeholder that allows the
 * test to continue execution. Future integration will call
 * processchecktimeouts() to wake sleeping threads.
 *
 * Currently, tests use explicit sleep/yield to allow threads to run.
 */
unsigned char thread_test_process_once(void) {
    if (!thread_test_harness.enabled) {
        return 0;  /* Test mode not enabled */
    }

    /* Placeholder: Currently no-op to allow tests to structure around */
    /* Future: Will call processchecktimeouts() with virtual time handling */

    return 1;
}

/*
 * thread_test_is_enabled - Query if test harness currently active
 */
unsigned char thread_test_is_enabled(void) {
    return thread_test_harness.enabled;
}
