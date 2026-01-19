/*
 * threadregistry.h - Thread Registry for POSIX Thread Safety
 *
 * Provides a registry for managing pthread-to-thread-ID mappings in the
 * Frontier CLI. This enables UserTalk scripts to reference threads by
 * stable integer IDs rather than opaque pthread handles.
 *
 * Design Goals:
 * - Thread-safe access to registry operations
 * - Efficient lookup by user-visible thread ID
 * - Clean separation from legacy cooperative threading model
 * - Support for thread sleep/wake/kill operations
 *
 * Usage:
 * 1. Call init_thread_registry() at startup
 * 2. Use allocate_thread_record() when spawning new threads
 * 3. Use get_thread_by_id() for thread operations
 * 4. Use free_thread_record() when thread exits
 * 5. Call cleanup_thread_registry() at shutdown
 *
 * THREAD SAFETY MODEL:
 *
 * Lifecycle Functions (NOT thread-safe - require external synchronization):
 * - init_thread_registry() - Must be called from main thread BEFORE workers spawn
 * - cleanup_thread_registry() - Must be called from main thread AFTER all workers exit
 *
 * Runtime Functions (FULLY thread-safe):
 * - allocate_thread_record() - Can be called from any thread
 * - free_thread_record() - Can be called from any thread
 * - get_thread_by_id() - Can be called from any thread
 * - get_thread_count() - Can be called from any thread
 *
 * REFERENCE COUNTING MODEL (Critical for thread safety):
 *
 * Ownership Rules:
 * - allocate_thread_record() returns refcount=1 (caller owns it)
 * - get_thread_by_id() auto-increments refcount (caller must release)
 * - free_thread_record()/release_thread_record() decrement refcount
 * - Synchronization primitives only destroyed when refcount reaches 0
 *
 * Usage Pattern:
 *   rec = allocate_thread_record();  // refcount=1
 *   // use rec...
 *   free_thread_record(rec);          // refcount--; if 0, destroy primitives
 *
 * Lookup Pattern:
 *   rec = get_thread_by_id(id);       // refcount++ (you now own this ref)
 *   // use rec...
 *   release_thread_record(rec);       // refcount--; if 0, destroy primitives
 *
 * Reference: planning/phase3/THREAD_SAFETY_PHASE1_PLAN.md
 *
 * Author: Frontier Development Team
 * Date: 2026-01-16
 */

#ifndef THREADREGISTRY_H
#define THREADREGISTRY_H

#include <pthread.h>
#include <stdint.h>

/* Forward declaration - actual definition in processinternal.h */
/* For thread registry, we just need to store a pointer */
typedef struct tythreadglobals **hdlthreadglobals;

/* Use portable boolean if available, otherwise define it */
#ifndef boolean
typedef unsigned char boolean;
#endif

#ifndef true
#define true 1
#endif

#ifndef false
#define false 0
#endif

/* nil for pointer compatibility */
#ifndef nil
#define nil ((void *)0)
#endif

/*
 * frontier_pthread_record - Per-thread state record
 *
 * Stores all state associated with a single pthread managed by the registry.
 * This includes synchronization primitives for sleep/wake and a link to
 * the thread's UserTalk globals.
 *
 * REFERENCE COUNTING:
 * Each record has a refcount that must be incremented when a thread gets a
 * pointer to it (via get_thread_by_id or allocate_thread_record). The refcount
 * is decremented when done (via release_thread_record). Only when refcount
 * reaches zero are synchronization primitives destroyed.
 *
 * Field Descriptions:
 * - pthread_id: The POSIX thread handle (set when pthread is created)
 * - hglobals: Handle to thread-local UserTalk globals
 * - user_thread_id: UserTalk-visible thread ID (stable, unique)
 * - wake_cond: Condition variable for sleep/wake
 * - state_mutex: Protects is_sleeping, is_killed, wakeup_ticks
 * - is_sleeping: True if thread is in sleep state
 * - wakeup_ticks: Tick count when thread should auto-wake (0 = no auto-wake)
 * - is_killed: True if thread.kill() was called
 * - in_use: True if this slot contains an active thread record
 * - refcount: Reference count for safe multi-threaded access
 * - refcount_mutex: Protects refcount field
 */
typedef struct frontier_pthread_record {
    pthread_t pthread_id;           /* POSIX thread handle */
    hdlthreadglobals hglobals;      /* UserTalk thread globals */
    long user_thread_id;            /* UserTalk-visible thread ID */
    pthread_cond_t wake_cond;       /* Condition variable for sleep/wake */
    pthread_mutex_t state_mutex;    /* Protects sleep/kill state */
    pthread_mutex_t refcount_mutex; /* Protects refcount */
    boolean is_sleeping;            /* Thread is sleeping */
    unsigned long wakeup_ticks;     /* Auto-wake tick count (0 = disabled) */
    boolean is_killed;              /* Thread has been killed */
    boolean in_use;                 /* Slot is in use */
    int refcount;                   /* Reference count (0 = can destroy primitives; protected by refcount_mutex) */
} frontier_pthread_record;

/*
 * init_thread_registry - Initialize the thread registry
 *
 * Must be called before any other registry functions. Initializes the
 * internal data structures and synchronization primitives.
 *
 * CRITICAL: Must be called from main thread BEFORE spawning any worker threads.
 * NOT thread-safe. Calling this while other threads use the registry causes
 * undefined behavior (data races on registry_initialized flag).
 *
 * Returns: true on success, false on failure
 *
 * Thread Safety: NOT thread-safe - call from main thread at startup only
 */
boolean init_thread_registry(void);

/*
 * cleanup_thread_registry - Shutdown and free the thread registry
 *
 * Frees all internal resources. Any remaining thread records are invalidated.
 * Should only be called at application shutdown.
 *
 * Thread Safety: NOT thread-safe - call from main thread at shutdown
 */
void cleanup_thread_registry(void);

/*
 * allocate_thread_record - Allocate a new thread record
 *
 * Allocates and initializes a new thread record. The record is assigned
 * a unique user_thread_id and marked as in_use.
 *
 * The returned record has:
 * - user_thread_id: Unique positive integer
 * - in_use: true
 * - is_sleeping: false
 * - is_killed: false
 * - wakeup_ticks: 0
 * - hglobals: nil (set later by caller)
 * - pthread_id: uninitialized (set when pthread created)
 * - state_mutex, wake_cond: initialized
 *
 * Returns: Pointer to allocated record, or NULL on failure
 *
 * Thread Safety: Thread-safe
 */
frontier_pthread_record *allocate_thread_record(void);

/*
 * free_thread_record - Release a thread record
 *
 * ALIAS for release_thread_record(). Decrements refcount. When refcount reaches zero,
 * synchronization primitives are destroyed and the slot can be reused.
 *
 * TYPICAL USAGE:
 * - After allocate_thread_record(): call free_thread_record() to release initial refcount
 * - After get_thread_by_id(): call release_thread_record() (preferred for lookup case)
 *
 * Safe to call with NULL (no-op).
 *
 * Parameters:
 *   rec - Record to free (may be NULL)
 *
 * Thread Safety: Thread-safe
 */
void free_thread_record(frontier_pthread_record *rec);

/*
 * get_thread_by_id - Look up a thread record by user ID
 *
 * Searches the registry for a thread with the given user_thread_id.
 *
 * Parameters:
 *   user_id - The UserTalk-visible thread ID to find
 *
 * Returns: Pointer to the record if found, NULL otherwise
 *
 * Thread Safety: Thread-safe
 */
frontier_pthread_record *get_thread_by_id(long user_id);

/*
 * get_thread_count - Get the number of active threads
 *
 * Returns the count of thread records currently in use.
 *
 * Returns: Number of active thread records
 *
 * Thread Safety: Thread-safe
 */
int get_thread_count(void);

/*
 * acquire_thread_record - Increment refcount for a thread record
 *
 * Must be called after getting a pointer to a record via allocate_thread_record()
 * or get_thread_by_id(). Prevents the record from being destroyed while in use.
 *
 * Safe to call with NULL (no-op).
 *
 * Parameters:
 *   rec - Record to acquire (may be NULL)
 *
 * Thread Safety: Thread-safe
 */
void acquire_thread_record(frontier_pthread_record *rec);

/*
 * release_thread_record - Decrement refcount for a thread record
 *
 * Must be called when done with a thread record. When refcount reaches zero,
 * synchronization primitives are safely destroyed and the record can be reused.
 *
 * Safe to call with NULL (no-op).
 *
 * Parameters:
 *   rec - Record to release (may be NULL)
 *
 * Thread Safety: Thread-safe
 */
void release_thread_record(frontier_pthread_record *rec);

#endif /* THREADREGISTRY_H */
