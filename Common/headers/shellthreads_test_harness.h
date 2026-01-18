
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
 * THREAD-SAFETY: NOT THREAD-SAFE
 * - Current implementation has no mutex protection on shared state
 * - Safe to call only from single UserTalk test thread (test scripts are single-threaded)
 * - Multiple concurrent calls will experience data races
 * - See Issue #322: Thread Test Harness: Add mutex protection for race conditions
 * - FUTURE: Add pthread_mutex protection for multi-threaded test scenarios
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
