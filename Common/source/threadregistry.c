/*
 * threadregistry.c - Thread Registry Implementation
 *
 * Thread-safe registry for managing pthread-to-thread-ID mappings.
 * Uses a fixed-size array with mutex protection for all operations.
 *
 * Reference Counting:
 * Each record has a reference count that prevents destruction while in use.
 * This solves the use-after-free problem by ensuring synchronization primitives
 * (mutexes, condition variables) are only destroyed when refcount reaches zero.
 *
 * Implementation Details:
 * - Fixed array of MAX_THREADS (64) thread records
 * - Linear search for slot allocation and lookup
 * - Monotonically increasing thread IDs (wrapped on overflow)
 * - Per-record mutex/condvar for sleep/wake operations
 * - Per-record refcount_mutex for reference count protection
 * - Global mutex for registry-level operations
 *
 * Thread Safety:
 * - All public functions acquire registry_mutex
 * - Per-record state_mutex protects sleep/kill state
 * - Per-record refcount_mutex protects reference count
 * - Cleanup must be called only when no threads are using the registry
 *
 * Reference: planning/phase4/p0a-critical-thread-safety/
 *
 * Author: Frontier Development Team
 * Date: 2026-01-16
 */

#include "threadregistry.h"
#include "logging.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Registry configuration
 */
#define MAX_THREADS 64  /* Maximum concurrent threads */

/*
 * Internal state
 */
static frontier_pthread_record thread_records[MAX_THREADS];
static long next_thread_id = 1;
static pthread_mutex_t registry_mutex = PTHREAD_MUTEX_INITIALIZER;
static boolean registry_initialized = false;

/*
 * init_thread_registry - Initialize the thread registry
 */
boolean init_thread_registry(void) {
    pthread_mutex_lock(&registry_mutex);

    /* Clear all records */
    memset(thread_records, 0, sizeof(thread_records));

    /* Reset ID counter */
    next_thread_id = 1;

    /* Mark as initialized */
    registry_initialized = true;

    pthread_mutex_unlock(&registry_mutex);
    return true;
}

/*
 * cleanup_thread_registry - Shutdown the thread registry
 *
 * IMPORTANT: Must only be called when no other threads are using the registry.
 * Records with non-zero refcount are force-freed (may cause issues if threads
 * are still using them). This is acceptable for shutdown since no new threads
 * should be spawned at this point.
 */
void cleanup_thread_registry(void) {
    int i;

    pthread_mutex_lock(&registry_mutex);

    /* Destroy mutexes and condvars for in-use records.
     * CRITICAL: Refcount must be 0 before destroying primitives.
     * Per POSIX spec: "Attempting to destroy a locked mutex results in undefined behavior."
     * FAIL-FAST: Assert on non-zero refcounts to catch thread leaks during development. */
    for (i = 0; i < MAX_THREADS; i++) {
        if (thread_records[i].in_use) {
            /* FAIL-FAST: Verify no threads are still using this record */
            if (thread_records[i].refcount != 0) {
                log_error(LOG_COMP_THREAD,
                         "cleanup_thread_registry: Record %d has refcount=%d (threads leaked at shutdown)",
                         i, thread_records[i].refcount);
                assert(thread_records[i].refcount == 0 && "Thread records leaked at cleanup time");
            }

            /* Safe to destroy now that refcount is verified to be 0 */
            pthread_mutex_destroy(&thread_records[i].refcount_mutex);
            pthread_mutex_destroy(&thread_records[i].state_mutex);
            pthread_cond_destroy(&thread_records[i].wake_cond);
            thread_records[i].in_use = false;
            thread_records[i].refcount = 0;
        }
    }

    registry_initialized = false;

    pthread_mutex_unlock(&registry_mutex);
}

/*
 * allocate_thread_record - Allocate a new thread record
 */
frontier_pthread_record *allocate_thread_record(void) {
    frontier_pthread_record *result = NULL;
    int i;

    pthread_mutex_lock(&registry_mutex);

    if (!registry_initialized) {
        pthread_mutex_unlock(&registry_mutex);
        return NULL;
    }

    /* Find a free slot */
    for (i = 0; i < MAX_THREADS; i++) {
        if (!thread_records[i].in_use) {
            frontier_pthread_record *rec = &thread_records[i];
            long allocated_id;

            /* Allocate a unique thread ID using the shared helper.
             * allocate_thread_id_locked() assumes registry_mutex is already held,
             * which it is here, so we avoid duplicating the wraparound logic. */
            allocated_id = allocate_thread_id_locked();
            if (allocated_id < 0) {
                /* ID allocation failed (all IDs exhausted) */
                pthread_mutex_unlock(&registry_mutex);
                return NULL;
            }

            /* Initialize the record */
            memset(rec, 0, sizeof(*rec));
            rec->user_thread_id = allocated_id;
            rec->in_use = true;
            rec->is_sleeping = false;
            rec->is_killed = false;
            rec->wakeup_ticks = 0;
            rec->hglobals = nil;
            rec->refcount = 1;  /* Start with refcount=1 (caller owns it) */

            /* Initialize synchronization primitives */
            if (pthread_mutex_init(&rec->refcount_mutex, NULL) != 0) {
                rec->in_use = false;
                pthread_mutex_unlock(&registry_mutex);
                return NULL;
            }

            if (pthread_mutex_init(&rec->state_mutex, NULL) != 0) {
                pthread_mutex_destroy(&rec->refcount_mutex);
                rec->in_use = false;
                pthread_mutex_unlock(&registry_mutex);
                return NULL;
            }

            if (pthread_cond_init(&rec->wake_cond, NULL) != 0) {
                pthread_mutex_destroy(&rec->state_mutex);
                pthread_mutex_destroy(&rec->refcount_mutex);
                rec->in_use = false;
                pthread_mutex_unlock(&registry_mutex);
                return NULL;
            }

            result = rec;
            break;
        }
    }

    if (result == NULL && registry_initialized) {
        /* All thread slots in use - this shouldn't happen in normal operation */
        log_error(LOG_COMP_LANG,
                 "allocate_thread_record: All thread slots exhausted (MAX_THREADS=%d)",
                 MAX_THREADS);
    }

    pthread_mutex_unlock(&registry_mutex);
    return result;
}

/*
 * free_thread_record - Release a thread record
 *
 * Decrements the refcount. When refcount reaches zero, destroys synchronization
 * primitives. This is an alias for release_thread_record() to simplify the API.
 *
 * Safe to call with NULL (no-op).
 */
void free_thread_record(frontier_pthread_record *rec) {
    /* free_thread_record is an alias for release_thread_record */
    release_thread_record(rec);
}

/*
 * get_thread_by_id - Look up a thread record by user ID
 */
frontier_pthread_record *get_thread_by_id(long user_id) {
    frontier_pthread_record *result = NULL;
    int i;

    /* Invalid IDs */
    if (user_id <= 0) {
        return NULL;
    }

    /* SYNCHRONIZATION: registry_mutex protects registry_initialized flag and
     * thread_records array during lookup. refcount is incremented while
     * registry_mutex held to ensure record can't be destroyed before caller
     * takes ownership of the reference. */
    pthread_mutex_lock(&registry_mutex);

    if (!registry_initialized) {
        pthread_mutex_unlock(&registry_mutex);
        return NULL;
    }

    /* Linear search for the ID */
    for (i = 0; i < MAX_THREADS; i++) {
        if (thread_records[i].in_use &&
            thread_records[i].user_thread_id == user_id) {
            /* Found it - increment refcount WHILE holding registry_mutex to ensure
             * this record can't be freed/destroyed before we return to caller. */
            pthread_mutex_lock(&thread_records[i].refcount_mutex);
            thread_records[i].refcount++;
            pthread_mutex_unlock(&thread_records[i].refcount_mutex);

            result = &thread_records[i];
            break;
        }
    }

    pthread_mutex_unlock(&registry_mutex);
    return result;
}

/*
 * allocate_thread_id_locked - Internal helper that allocates a unique thread ID
 *
 * ASSUMES: registry_mutex is ALREADY HELD by caller
 *
 * Returns a new thread ID. After wraparound at LONG_MAX, searches for an
 * unused ID to avoid collisions with long-lived threads from earlier cycles.
 *
 * Updates global next_thread_id to track the next ID to try.
 */
static long allocate_thread_id_locked(void) {
    long candidate_id;
    int i;
    boolean id_in_use;
    boolean need_collision_check = false;

#ifdef DEBUG
    /* Verify caller holds registry_mutex (defensive programming) */
    int lock_status = pthread_mutex_trylock(&registry_mutex);
    assert(lock_status == EBUSY && "allocate_thread_id_locked requires registry_mutex to be held by caller");
    /* We don't actually want to unlock - trylock would have failed if locked, so this is just an assertion */
#endif

    candidate_id = next_thread_id;

    /* CRITICAL: After wraparound, must search for an unused ID to prevent collision.
     * Two scenarios where wraparound can occur:
     * 1. next_thread_id == LONG_MAX at entry (explicit wraparound)
     * 2. next_thread_id increments to LONG_MAX and wraps to 1 (crossing boundary)
     *
     * Scenario: System runs for months. Thread with ID=1 from boot cycle still exists.
     * Without collision checking, new ID allocation would reuse ID=1 and cause use-after-free.
     *
     * Solution: After any wraparound that lands on ID=1, check if it's in use.
     * If in use, run the collision avoidance search to find an unused ID. */

    if (next_thread_id == LONG_MAX) {
        /* Entry at boundary: about to wrap on next increment */
        need_collision_check = true;
    } else {
        /* Normal case: just increment */
        next_thread_id++;
        if (next_thread_id >= LONG_MAX) {
            /* Crossed boundary: wrapped from LONG_MAX to 1 */
            next_thread_id = 1;
            need_collision_check = true;
        }
    }

    /* If we're at or near a wraparound point, verify the candidate ID isn't already in use.
     * This protects against ID collisions when long-lived threads from earlier cycles overlap
     * with newly allocated IDs from a wrapped counter. */
    if (need_collision_check) {
        int attempts = 0;

        /* Search for an unused ID, starting from candidate_id.
         * Guard against infinite loop with iteration counter. Should never exceed MAX_THREADS
         * iterations since we can have at most MAX_THREADS threads alive at once. */
        for (;;) {
            if (++attempts > MAX_THREADS) {
                /* All slots exhausted - shouldn't happen in normal operation */
                log_error(LOG_COMP_LANG,
                         "allocate_thread_id_locked: All thread IDs exhausted (MAX_THREADS=%d)",
                         MAX_THREADS);
                return -1;
            }

            /* Check if this ID is already allocated */
            id_in_use = false;
            for (i = 0; i < MAX_THREADS; i++) {
                if (thread_records[i].in_use &&
                    thread_records[i].user_thread_id == candidate_id) {
                    id_in_use = true;
                    break;
                }
            }

            if (!id_in_use) {
                /* Found an unused ID - update next_thread_id to prepare for next allocation */
                next_thread_id = candidate_id + 1;
                if (next_thread_id >= LONG_MAX) {
                    next_thread_id = 1;
                }
                break;
            }

            /* Try next ID */
            candidate_id++;
            if (candidate_id >= LONG_MAX) {
                candidate_id = 1;
            }
        }
    }

    return candidate_id;
}

/*
 * allocate_thread_id - Allocate a new unique thread ID
 *
 * Returns a new thread ID. After wraparound at LONG_MAX, searches for an
 * unused ID to avoid collisions with long-lived threads from earlier cycles.
 */
long allocate_thread_id(void) {
    long result;

    pthread_mutex_lock(&registry_mutex);
    result = allocate_thread_id_locked();
    pthread_mutex_unlock(&registry_mutex);

    return result;
}

/*
 * get_thread_count - Get the number of active threads
 */
int get_thread_count(void) {
    int count = 0;
    int i;

    pthread_mutex_lock(&registry_mutex);

    /* Check initialization INSIDE the lock to prevent race where flag changes
     * between check and use. Ensures atomic access to registry_initialized and
     * thread_records array. */
    if (!registry_initialized) {
        pthread_mutex_unlock(&registry_mutex);
        return 0;
    }

    for (i = 0; i < MAX_THREADS; i++) {
        if (thread_records[i].in_use) {
            count++;
        }
    }

    pthread_mutex_unlock(&registry_mutex);
    return count;
}

/*
 * acquire_thread_record - Increment refcount for a thread record
 */
void acquire_thread_record(frontier_pthread_record *rec) {
    if (rec == NULL) {
        return;
    }

    pthread_mutex_lock(&registry_mutex);

    /* Guard against operating on records after cleanup. If registry is not initialized,
     * the thread_records array is in undefined state and may be destroyed. */
    if (!registry_initialized) {
        pthread_mutex_unlock(&registry_mutex);
        return;
    }

    if (rec >= thread_records && rec < thread_records + MAX_THREADS && rec->in_use) {
        pthread_mutex_lock(&rec->refcount_mutex);
        rec->refcount++;
        pthread_mutex_unlock(&rec->refcount_mutex);
    }

    pthread_mutex_unlock(&registry_mutex);
}

/*
 * release_thread_record - Decrement refcount for a thread record
 */
void release_thread_record(frontier_pthread_record *rec) {
    if (rec == NULL) {
        return;
    }

    /* SYNCHRONIZATION: registry_mutex held for entire operation prevents races with:
     * - allocate_thread_record() scanning for free slots
     * - get_thread_by_id() incrementing refcount
     * - cleanup_thread_registry() destroying records
     */
    pthread_mutex_lock(&registry_mutex);

    /* Guard against operating on records after cleanup */
    if (!registry_initialized) {
        pthread_mutex_unlock(&registry_mutex);
        return;
    }

    if (rec >= thread_records && rec < thread_records + MAX_THREADS && rec->in_use) {
        boolean should_destroy;

        /* Decrement refcount under its own lock */
        pthread_mutex_lock(&rec->refcount_mutex);
        rec->refcount--;
        should_destroy = (rec->refcount == 0);
        pthread_mutex_unlock(&rec->refcount_mutex);

        /* Destroy primitives WHILE holding registry_mutex to ensure atomic destruction.
         * CRITICAL: registry_mutex prevents allocate_thread_record() from reusing this slot
         * until AFTER we've destroyed all synchronization primitives. Only after destruction
         * complete do we mark in_use=false to signal the slot is available. */
        if (should_destroy) {
            pthread_mutex_destroy(&rec->state_mutex);
            pthread_cond_destroy(&rec->wake_cond);
            pthread_mutex_destroy(&rec->refcount_mutex);
            /* Mark slot as free AFTER destroying primitives, while registry_mutex held.
             * This prevents another thread from allocating and initializing new mutexes
             * in this slot while we're still destroying the old ones. */
            rec->in_use = false;
        }
    }

    pthread_mutex_unlock(&registry_mutex);
}
