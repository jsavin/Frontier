/*
 * threadregistry.c - Thread Registry Implementation
 *
 * Thread-safe registry for managing pthread-to-thread-ID mappings.
 * Uses a fixed-size array with mutex protection for all operations.
 *
 * Implementation Details:
 * - Fixed array of MAX_THREADS (64) thread records
 * - Linear search for slot allocation and lookup
 * - Monotonically increasing thread IDs (never reused within lifecycle)
 * - Per-record mutex/condvar for sleep/wake operations
 * - Global mutex for registry-level operations
 *
 * Thread Safety:
 * - All public functions acquire registry_mutex
 * - Per-record state_mutex protects individual record state
 * - init/cleanup are NOT thread-safe (call from main thread only)
 *
 * Reference: planning/phase3/THREAD_SAFETY_PHASE1_PLAN.md
 *
 * Author: Frontier Development Team
 * Date: 2026-01-16
 */

#include "threadregistry.h"
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
 */
void cleanup_thread_registry(void) {
    pthread_mutex_lock(&registry_mutex);

    /* Destroy mutexes and condvars for in-use records */
    for (int i = 0; i < MAX_THREADS; i++) {
        if (thread_records[i].in_use) {
            pthread_mutex_destroy(&thread_records[i].state_mutex);
            pthread_cond_destroy(&thread_records[i].wake_cond);
            thread_records[i].in_use = false;
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

    pthread_mutex_lock(&registry_mutex);

    if (!registry_initialized) {
        pthread_mutex_unlock(&registry_mutex);
        return NULL;
    }

    /* Find a free slot */
    for (int i = 0; i < MAX_THREADS; i++) {
        if (!thread_records[i].in_use) {
            frontier_pthread_record *rec = &thread_records[i];

            /* Initialize the record */
            memset(rec, 0, sizeof(*rec));
            rec->user_thread_id = next_thread_id++;
            rec->in_use = true;
            rec->is_sleeping = false;
            rec->is_killed = false;
            rec->wakeup_ticks = 0;
            rec->hglobals = nil;

            /* Initialize synchronization primitives */
            pthread_mutex_init(&rec->state_mutex, NULL);
            pthread_cond_init(&rec->wake_cond, NULL);

            result = rec;
            break;
        }
    }

    pthread_mutex_unlock(&registry_mutex);
    return result;
}

/*
 * free_thread_record - Release a thread record
 */
void free_thread_record(frontier_pthread_record *rec) {
    if (rec == NULL) {
        return;
    }

    pthread_mutex_lock(&registry_mutex);

    /* Verify this is a valid record in our array */
    if (rec >= thread_records && rec < thread_records + MAX_THREADS) {
        if (rec->in_use) {
            /* Destroy synchronization primitives */
            pthread_mutex_destroy(&rec->state_mutex);
            pthread_cond_destroy(&rec->wake_cond);

            /* Mark slot as free */
            rec->in_use = false;
        }
        /* If already freed, this is a no-op (double-free safe) */
    }

    pthread_mutex_unlock(&registry_mutex);
}

/*
 * get_thread_by_id - Look up a thread record by user ID
 */
frontier_pthread_record *get_thread_by_id(long user_id) {
    frontier_pthread_record *result = NULL;

    /* Invalid IDs */
    if (user_id <= 0) {
        return NULL;
    }

    pthread_mutex_lock(&registry_mutex);

    if (!registry_initialized) {
        pthread_mutex_unlock(&registry_mutex);
        return NULL;
    }

    /* Linear search for the ID */
    for (int i = 0; i < MAX_THREADS; i++) {
        if (thread_records[i].in_use &&
            thread_records[i].user_thread_id == user_id) {
            result = &thread_records[i];
            break;
        }
    }

    pthread_mutex_unlock(&registry_mutex);
    return result;
}

/*
 * allocate_thread_id - Allocate a new unique thread ID
 */
long allocate_thread_id(void) {
    long id;

    pthread_mutex_lock(&registry_mutex);
    id = next_thread_id++;
    pthread_mutex_unlock(&registry_mutex);

    return id;
}

/*
 * get_thread_count - Get the number of active threads
 */
int get_thread_count(void) {
    int count = 0;

    pthread_mutex_lock(&registry_mutex);

    for (int i = 0; i < MAX_THREADS; i++) {
        if (thread_records[i].in_use) {
            count++;
        }
    }

    pthread_mutex_unlock(&registry_mutex);
    return count;
}
