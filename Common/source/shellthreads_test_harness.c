
/*	$Id$    */

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

#include "frontier.h"
#include "standard.h"
#include "shellthreads_test_harness.h"
#include "process.h"
#include "logging.h"

#include <stdlib.h>
#include <string.h>

/*
 * Thread Test Harness Implementation
 *
 * DESIGN:
 * - Static state structure holds virtual tick count and enable flag
 * - Environment variable gate (FRONTIER_THREAD_TEST_MODE=1) prevents accidental activation
 * - All functions return false if test mode not enabled (safe no-ops)
 * - Virtual ticks replace real system time when freeze_system_time=true
 *
 * INTEGRATION:
 * - gettickcount() checks thread_test_is_enabled() before returning virtual ticks
 * - processchecktimeouts() called via thread_test_process_once()
 * - No ODB persistence - all state is runtime-only
 */

static struct {
    boolean enabled;              /* Test mode active */
    uint32_t virtual_ticks;       /* Injected tick count */
    boolean freeze_system_time;   /* Block real gettickcount(), use virtual ticks */
    pthread_t test_thread;        /* Thread that enabled test mode (for assertions) */
} thread_test_harness = {false, 0, false, 0};

/*
 * assert_single_threaded_test_mode - Verify test harness called from single thread
 *
 * THREAD-SAFETY: Test harness is NOT thread-safe. This assertion (in DEBUG builds)
 * catches attempts to use the harness from multiple threads, preventing silent
 * data corruption.
 *
 * See Issue #322: Thread Test Harness: Add mutex protection for race conditions
 */
static void assert_single_threaded_test_mode(void) {
#ifdef DEBUG
    if (thread_test_harness.enabled) {
        pthread_t current = pthread_self();

        /* First call to test function after enable: store thread ID */
        if (thread_test_harness.test_thread == 0) {
            thread_test_harness.test_thread = current;
        }
        /* Verify still same thread */
        else if (!pthread_equal(current, thread_test_harness.test_thread)) {
            log_error(LOG_COMP_LANG,
                "FATAL: Thread test harness accessed from multiple threads! "
                "See Issue #322 for mutex protection implementation.");
            assert(false && "Thread test harness is single-threaded only");
        }
    }
#endif
}


/*
 * thread_test_enable - Activate deterministic thread testing mode
 *
 * Checks FRONTIER_THREAD_TEST_MODE environment variable.
 * Only enables test mode if set to "1".
 *
 * Returns: true if test mode activated, false otherwise
 */
boolean thread_test_enable(void) {
    assert_single_threaded_test_mode();

    const char *env = getenv("FRONTIER_THREAD_TEST_MODE");

    if (env == NULL || strcmp(env, "1") != 0) {
        log_warn(LOG_COMP_LANG,
            "thread.test.enable() called but FRONTIER_THREAD_TEST_MODE != 1 (safety gate)");
        return false;
    }

    /* Always reset state defensively to prevent contamination from crashed tests */
    boolean was_enabled = thread_test_harness.enabled;
    thread_test_harness.enabled = true;
    thread_test_harness.virtual_ticks = 0;  /* Reset even if already enabled */
    thread_test_harness.freeze_system_time = true;

    if (was_enabled) {
        log_debug(LOG_COMP_LANG, "thread.test.enable() - already enabled, reset state");
    } else {
        log_info(LOG_COMP_LANG,
            "Thread test harness ENABLED - virtual ticks = 0, system time frozen");
    }

    return true;
}


/*
 * thread_test_disable - Return to normal timing behavior
 *
 * Deactivates test mode, restoring real system time.
 *
 * Returns: true if test mode was active and is now disabled, false otherwise
 */
boolean thread_test_disable(void) {
    assert_single_threaded_test_mode();

    if (!thread_test_harness.enabled) {
        return false;
    }

    thread_test_harness.enabled = false;
    thread_test_harness.freeze_system_time = false;
    thread_test_harness.virtual_ticks = 0;  /* Reset state for test isolation */

    log_info(LOG_COMP_LANG, "Thread test harness DISABLED - restored to system time");

    return true;
}


/*
 * thread_test_set_ticks - Set virtual tick count to absolute value
 *
 * Used to establish specific timing states for test scenarios.
 *
 * Returns: true if test mode enabled and ticks set, false otherwise
 */
boolean thread_test_set_ticks(uint32_t ticks) {
    assert_single_threaded_test_mode();

    if (!thread_test_harness.enabled) {
        return false;
    }

    thread_test_harness.virtual_ticks = ticks;

    log_debug(LOG_COMP_LANG, "thread.test.setTicks(%u) - virtual time set", ticks);

    return true;
}


/*
 * thread_test_get_ticks - Read current virtual tick count
 *
 * Returns: Current virtual ticks, or 0 if test mode not enabled
 */
uint32_t thread_test_get_ticks(void) {
    assert_single_threaded_test_mode();

    if (!thread_test_harness.enabled) {
        return 0;
    }

    return thread_test_harness.virtual_ticks;
}


/*
 * thread_test_advance - Increment virtual ticks by delta
 *
 * Primary time advancement mechanism for tests.
 * Simulates passage of time without actual delays.
 *
 * Returns: true if test mode enabled and time advanced, false otherwise
 */
boolean thread_test_advance(uint32_t delta) {
    assert_single_threaded_test_mode();

    if (!thread_test_harness.enabled) {
        return false;
    }

    uint32_t old_ticks = thread_test_harness.virtual_ticks;
    thread_test_harness.virtual_ticks += delta;

    log_debug(LOG_COMP_LANG,
        "thread.test.advance(%u) - virtual time %u -> %u",
        delta, old_ticks, thread_test_harness.virtual_ticks);

    return true;
}


/*
 * thread_test_process_once - Execute single event loop iteration
 *
 * Calls processchecktimeouts() to wake threads whose sleep time has elapsed.
 * Tests use this after advancing time to trigger deterministic wake-ups.
 *
 * Returns: true if test mode enabled and processing executed, false otherwise
 */
boolean thread_test_process_once(void) {
    assert_single_threaded_test_mode();

    if (!thread_test_harness.enabled) {
        return false;
    }

    log_trace(LOG_COMP_LANG,
        "thread.test.processOnce() - checking timeouts at virtual tick %u",
        thread_test_harness.virtual_ticks);

    /* Call the actual event loop timeout checker */
    processchecktimeouts();

    return true;
}


/*
 * thread_test_is_enabled - Check if test mode is active
 *
 * Called by gettickcount() interception to decide whether to return
 * virtual ticks or real system time.
 *
 * Returns: true if test mode enabled and system time frozen
 */
boolean thread_test_is_enabled(void) {
    assert_single_threaded_test_mode();

    return (thread_test_harness.enabled && thread_test_harness.freeze_system_time);
}


/*
 * thread_test_current_ticks - Get current virtual tick count for interception
 *
 * Called by gettickcount() when test mode is enabled.
 * Returns the injected virtual tick count instead of real system time.
 *
 * Returns: Current virtual ticks (only valid when test mode enabled)
 */
uint32_t thread_test_current_ticks(void) {
    assert_single_threaded_test_mode();

    return thread_test_harness.virtual_ticks;
}
