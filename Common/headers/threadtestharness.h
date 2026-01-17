/*
 * threadtestharness.h - Thread Test Harness for Deterministic Testing
 *
 * Provides deterministic control over thread timing for integration tests.
 * Allows tests to:
 * - Freeze system time and advance it manually
 * - Validate exact thread wake times
 * - Create repeatable test scenarios without flakiness
 *
 * Safety:
 * - Disabled by default (requires FRONTIER_THREAD_TEST_MODE=1 env var)
 * - No ODB persistence (all state is runtime-only)
 * - Single boolean check overhead when disabled
 *
 * Thread Safety:
 * - Safe for cooperative threading (single event loop)
 * - No protection needed for test harness state (UserTalk is single-threaded)
 *
 * Usage:
 *   thread_test_enable()           // Activate test mode
 *   thread_test_set_ticks(0)       // Reset virtual clock
 *   thread_test_advance(100)       // Advance 100ms
 *   thread_test_process_once()     // Process timeouts/wake threads
 *   thread_test_disable()          // Return to normal operation
 *
 * Author: Claude (Frontier Dev Team)
 * Date: 2026-01-17
 */

#ifndef THREADTESTHARNESS_H
#define THREADTESTHARNESS_H

#include <stdint.h>

/* Forward declarations */
typedef unsigned char boolean;

/*
 * thread_test_enable - Activate deterministic test harness mode
 *
 * Checks FRONTIER_THREAD_TEST_MODE environment variable.
 * Only enables if variable is set to "1".
 *
 * Returns: true if test mode activated, false otherwise
 * Side Effects: Sets up test harness state, logs warning if activated
 */
boolean thread_test_enable(void);

/*
 * thread_test_disable - Return to normal operation
 *
 * Restores real system timer (gettickcount returns actual system time).
 * Safe to call even if not enabled.
 *
 * Returns: true if successful, false if already disabled
 */
boolean thread_test_disable(void);

/*
 * thread_test_set_ticks - Set virtual clock to absolute value
 *
 * Parameters:
 *   ticks - Absolute tick count (0 = epoch)
 *
 * Returns: true if successful, false if test mode not enabled
 * Side Effects: Does NOT process timeouts (call thread_test_process_once separately)
 */
boolean thread_test_set_ticks(uint32_t ticks);

/*
 * thread_test_get_ticks - Read current virtual tick count
 *
 * Returns: Current tick count (0 if test mode not enabled)
 * Side Effects: None (read-only)
 */
uint32_t thread_test_get_ticks(void);

/*
 * thread_test_advance - Increment virtual clock by delta ticks
 *
 * Parameters:
 *   delta - Milliseconds to advance
 *
 * Returns: true if successful, false if test mode not enabled
 * Side Effects: Does NOT process timeouts (call thread_test_process_once separately)
 *
 * Pattern: Always pair with thread_test_process_once():
 *   thread_test_advance(100)
 *   thread_test_process_once()  // Wake sleeping threads
 */
boolean thread_test_advance(uint32_t delta);

/*
 * thread_test_process_once - Single iteration of thread event loop
 *
 * Processes timeouts and wakes sleeping threads at current virtual tick.
 * May transition threads from sleeping → running → terminated.
 *
 * Returns: true if processing occurred, false if test mode not enabled
 * Side Effects: Calls processchecktimeouts(), may execute thread code
 *
 * Determinism Guarantee: Called with identical virtual_ticks and thread state,
 * produces identical results (no wall-clock dependencies).
 */
boolean thread_test_process_once(void);

/*
 * thread_test_is_enabled - Query if test harness currently active
 *
 * Returns: true if test mode enabled, false otherwise
 * Side Effects: None (read-only)
 *
 * Use Case: Conditional debug logging only in test mode
 */
boolean thread_test_is_enabled(void);

#endif /* THREADTESTHARNESS_H */
