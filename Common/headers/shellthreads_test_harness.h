
/*	$Id$    */

/*
    SPDX-License-Identifier: MIT

    Copyright (c) 1992-2004 UserLand Software, Inc.
    Copyright (c) 2025-2026 Frontier contributors

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
*/

#ifndef shellthreadstestharnessinclude
#define shellthreadstestharnessinclude

#include <stdint.h>
#include <stdbool.h>

/*
 * Thread Testing Harness - Deterministic Thread Timing Control
 *
 * PURPOSE:
 * This module provides deterministic control over thread timing for testing.
 * When enabled, virtual ticks replace real system time, allowing tests to:
 * - Control exact timing of thread.sleep() wake-ups
 * - Advance time by specific deltas for reproducible test scenarios
 * - Process timeout events on demand without waiting
 *
 * SAFETY:
 * - Only activates when FRONTIER_THREAD_TEST_MODE=1 environment variable is set
 * - All state is static (not persisted to ODB)
 * - Production code automatically uses real system time when test mode is off
 *
 * ARCHITECTURE:
 * - Intercepts gettickcount() to return virtual ticks when test mode enabled
 * - Provides control APIs for UserTalk test scripts
 * - Minimal changes to production code (single interception point)
 *
 * THREAD-SAFETY: THREAD-SAFE
 * - Mutex protection (harness_mutex) guards all shared state access
 * - Safe to call from multiple threads concurrently
 * - DEBUG builds include single-thread assertion for early race detection
 * - See Issue #322: Consider removing single-thread assertion if multi-threaded
 *   test scenarios are needed in the future
 */

/* Test harness control - returns false if test mode not enabled or operation failed */
extern boolean thread_test_enable(void);
extern boolean thread_test_disable(void);

/* Virtual tick manipulation - returns false if test mode not enabled */
extern boolean thread_test_set_ticks(uint32_t ticks);
extern uint32_t thread_test_get_ticks(void);
extern boolean thread_test_advance(uint32_t delta);

/* Event processing - returns false if test mode not enabled */
extern boolean thread_test_process_once(void);

/* Internal: Check if test mode is active (called by gettickcount interception) */
extern boolean thread_test_is_enabled(void);
extern uint32_t thread_test_current_ticks(void);

#endif /* shellthreadstestharnessinclude */
