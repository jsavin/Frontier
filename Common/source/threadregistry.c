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

            /* Allocate a unique thread ID using the dedicated allocate function.
             * This ensures ID uniqueness, monotonicity, and handles wraparound correctly.
             * Note: allocate_thread_id() also acquires registry_mutex, but we're already
             * holding it, so we need to use the internal ID allocation logic directly. */
            allocated_id = next_thread_id;
            if (next_thread_id == LONG_MAX) {
                /* Handle wraparound: must find an unused ID to avoid collision */
                int attempts = 0;
                long candidate_id = next_thread_id;
                boolean id_in_use;
                int j;

                for (;;) {
                    if (++attempts > MAX_THREADS) {
                        /* All slots in use - shouldn't happen but bail out safely */
                        pthread_mutex_unlock(&registry_mutex);
                        return NULL;
                    }

                    id_in_use = false;
                    for (j = 0; j < MAX_THREADS; j++) {
                        if (thread_records[j].in_use && thread_records[j].user_thread_id == candidate_id) {
                            id_in_use = true;
                            break;
                        }
                    }

                    if (!id_in_use) {
                        allocated_id = candidate_id;
                        next_thread_id = candidate_id + 1;
                        if (next_thread_id >= LONG_MAX) {
                            next_thread_id = 1;
                        }
                        break;
                    }

                    candidate_id++;
                    if (candidate_id >= LONG_MAX) {
                        candidate_id = 1;
                    }
                }
            } else {
                next_thread_id++;
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
 * allocate_thread_id - Allocate a new unique thread ID
 *
 * Returns a new thread ID. After wraparound at LONG_MAX, searches for an
 * unused ID to avoid collisions with long-lived threads from earlier cycles.
 */
long allocate_thread_id(void) {
    long candidate_id;
    int i;
    boolean id_in_use;

    pthread_mutex_lock(&registry_mutex);

    candidate_id = next_thread_id;

    /* CRITICAL: After overflow, must skip IDs already in use to prevent collision.
     * Scenario: System runs for months, wraps from LONG_MAX→1. If thread with ID=1
     * from boot cycle still exists, collision would occur. Search for unused ID. */
    if (next_thread_id == LONG_MAX) {
        /* We've wrapped - must find an ID not in use.
         * Guard against infinite loop with iteration counter. Should never exceed MAX_THREADS
         * iterations since we can have at most MAX_THREADS threads alive at once. */
        int attempts = 0;
        for (;;) {
            if (++attempts > MAX_THREADS) {
                /* Should never happen - all slots can't be in use if we're trying to allocate.
                 * But if it does, bail out with an error. */
                pthread_mutex_unlock(&registry_mutex);
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
                /* Found an unused ID */
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
    } else {
        /* Normal case: just increment */
        next_thread_id++;
    }

    pthread_mutex_unlock(&registry_mutex);

    return candidate_id;
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
