# Threading Architecture Design

**Date**: 2026-01-16
**Purpose**: POSIX threading architecture for Frontier CLI

---

## Executive Summary

This document proposes a **phased migration** from Frontier's cooperative green thread model to **POSIX threads (pthreads)** while maintaining 100% backward compatibility with the existing thread.* verb API.

**Critical Constraints**:
1. **POSIX compliant** (macOS/Linux pthreads)
2. **Zero API changes** to UserTalk thread.* verbs
3. **Thread-safe ODB access** (database operations, hash tables)
4. **Headless-first** (CLI implementation, GUI integration later)
5. **Maintain agent thread behavior** (auto-creation, auto-restart)

---

## Current Architecture Analysis

### Threading Model: Cooperative Green Threads
- **Implementation**: Manual context switching via `tythreadglobals` swap
- **Scheduling**: Time-slice-based cooperative multitasking
- **Execution**: Sequential (no parallelism, only concurrency)
- **Primitives**: `threadsleep()`, `threadwake()`, `threadyield()`

### Thread State Management
**Thread-Local Storage** (`tythreadglobals` structure):
- **Size**: ~244 bytes of per-thread state
- **Contents**: Process stack, outline stack, database context, script state
- **Swap Mechanism**: `copythreadglobals()` → `swapinthreadglobals()`

**Global Thread List**:
- **Structure**: Singly-linked list (`tythreadglobals.hnextglobals`)
- **Head**: `processthreadlist->hfirst`
- **Lookup**: Linear scan by thread ID

### Process Scheduling
**Agent Scheduler** (`agentscheduler()`):
- Runs in dedicated agent thread
- Time-slices agent processes (4 ticks each)
- Sleeps between scheduler runs (1 second timeout)

**One-Shot Scheduler** (`oneshotscheduler()`):
- Executes one-shot processes immediately
- Runs in dedicated pthread per one-shot (current implementation)
- Disposes process on completion

---

## Proposed POSIX Architecture

### Design Principles
1. **One pthread per Frontier thread** (true parallelism)
2. **Thread-local `tythreadglobals`** via `pthread_setspecific()`
3. **Mutex-protected shared state** (process list, agent scheduler)
4. **Condition variables for sleep/wake** (replace polling)
5. **Backward-compatible thread IDs** (opaque `long` values)

### Core Components

#### 1. Thread ID Mapping
**Problem**: Current thread IDs are `(long)hdlthreadglobals` addresses

**Solution**: Maintain bidirectional mapping:
```c
typedef struct {
    pthread_t pthread_id;           // OS thread ID
    hdlthreadglobals hglobals;      // Thread-local state
    long user_thread_id;            // UserTalk-visible ID (unique)
    pthread_cond_t wake_cond;       // For sleep/wake
    pthread_mutex_t state_mutex;    // Protects thread state
    boolean is_sleeping;            // Sleep state
    unsigned long wakeup_time;      // Timeout for timed sleep
} frontier_pthread_record;

// Global registry (mutex-protected)
static frontier_pthread_record *thread_registry = NULL;
static int thread_registry_size = 0;
static pthread_mutex_t registry_mutex = PTHREAD_MUTEX_INITIALIZER;
static long next_thread_id = 100;  // Start at 100 for clarity
```

**Thread Lookup** (replaces `getprocessthread()`):
```c
frontier_pthread_record *get_thread_by_id(long user_id) {
    pthread_mutex_lock(&registry_mutex);
    for (int i = 0; i < thread_registry_size; i++) {
        if (thread_registry[i].user_thread_id == user_id) {
            pthread_mutex_unlock(&registry_mutex);
            return &thread_registry[i];
        }
    }
    pthread_mutex_unlock(&registry_mutex);
    return NULL;
}
```

#### 2. Sleep/Wake Implementation
**Current**: Polling-based timeout check in scheduler

**Proposed**: Condition variable + absolute timeout
```c
boolean thread_sleep(long thread_id, long ticks) {
    frontier_pthread_record *thread = get_thread_by_id(thread_id);
    if (!thread) return false;

    pthread_mutex_lock(&thread->state_mutex);
    thread->is_sleeping = true;

    if (ticks < 0) {
        // Indefinite sleep (wait for wake)
        pthread_cond_wait(&thread->wake_cond, &thread->state_mutex);
    } else {
        // Timed sleep (ticks = 1/60th second)
        struct timespec timeout;
        clock_gettime(CLOCK_REALTIME, &timeout);
        timeout.tv_sec += ticks / 60;
        timeout.tv_nsec += (ticks % 60) * (1000000000 / 60);
        pthread_cond_timedwait(&thread->wake_cond, &thread->state_mutex, &timeout);
    }

    thread->is_sleeping = false;
    pthread_mutex_unlock(&thread->state_mutex);
    return true;
}

boolean thread_wake(long thread_id) {
    frontier_pthread_record *thread = get_thread_by_id(thread_id);
    if (!thread) return false;

    pthread_mutex_lock(&thread->state_mutex);
    boolean was_sleeping = thread->is_sleeping;
    if (was_sleeping) {
        pthread_cond_signal(&thread->wake_cond);
    }
    pthread_mutex_unlock(&thread->state_mutex);
    return was_sleeping;
}
```

#### 3. Thread Creation
**Entry Point** (replaces `newprocessthread()`):
```c
boolean create_frontier_thread(tythreadmaincallback callback,
                                tythreadmainparams params,
                                hdlthreadglobals *hthread_out) {
    frontier_pthread_record *thread_rec = allocate_thread_record();
    if (!thread_rec) return false;

    // Create thread-local globals
    if (!newthreadglobals(&thread_rec->hglobals)) {
        free_thread_record(thread_rec);
        return false;
    }

    // Initialize condition variable and mutex
    pthread_cond_init(&thread_rec->wake_cond, NULL);
    pthread_mutex_init(&thread_rec->state_mutex, NULL);

    // Assign unique thread ID
    thread_rec->user_thread_id = allocate_thread_id();

    // Create pthread
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    // Pass thread_rec as parameter to wrapper
    int result = pthread_create(&thread_rec->pthread_id, &attr,
                                 frontier_thread_wrapper, thread_rec);
    pthread_attr_destroy(&attr);

    if (result != 0) {
        free_thread_record(thread_rec);
        return false;
    }

    *hthread_out = thread_rec->hglobals;
    return true;
}
```

**Thread Wrapper** (pthread entry point):
```c
void *frontier_thread_wrapper(void *arg) {
    frontier_pthread_record *thread_rec = (frontier_pthread_record *)arg;

    // Set thread-local globals using pthread_setspecific()
    pthread_setspecific(threadglobals_key, thread_rec->hglobals);
    hthreadglobals = thread_rec->hglobals;  // Also set global pointer

    // Store pthread ID in tythreadglobals
    (**thread_rec->hglobals).idthread = (hdlthread)thread_rec->user_thread_id;

    // Call original thread main
    void *result = thread_rec->callback(thread_rec->callback_params);

    // Cleanup
    cleanup_thread_record(thread_rec);
    return result;
}
```

#### 4. Thread-Local Storage
**pthread Key** (one-time initialization):
```c
static pthread_key_t threadglobals_key;
static pthread_once_t key_once = PTHREAD_ONCE_INIT;

static void make_threadglobals_key(void) {
    pthread_key_create(&threadglobals_key, dispose_threadglobals_destructor);
}

void init_threading_system(void) {
    pthread_once(&key_once, make_threadglobals_key);
}
```

**Accessor** (replaces global `hthreadglobals`):
```c
hdlthreadglobals getcurrentthreadglobals(void) {
    hdlthreadglobals hg = (hdlthreadglobals)pthread_getspecific(threadglobals_key);
    if (hg == NULL) {
        // Main thread or early initialization - use global
        return hthreadglobals;
    }
    return hg;
}

// Macro for backward compatibility
#define hthreadglobals (getcurrentthreadglobals())
```

#### 5. Agent Thread Implementation
**Agent Thread Main** (POSIX version):
```c
void *agent_thread_main_posix(void *params) {
    // Initialize thread-local state
    pthread_setspecific(threadglobals_key, agent_thread_globals);
    hthreadglobals = agent_thread_globals;

    // Set thread name for debugging
    pthread_setname_np("frontier-agents");

    while (agents_enabled() && !shutdown_requested()) {
        // Run agent scheduler (mutex-protected)
        pthread_mutex_lock(&agent_scheduler_mutex);
        run_agent_scheduler();
        pthread_mutex_unlock(&agent_scheduler_mutex);

        // Sleep until next scheduler run (1 second timeout)
        pthread_mutex_lock(&agent_sleep_mutex);
        struct timespec timeout = {.tv_sec = time(NULL) + 1, .tv_nsec = 0};
        agent_is_sleeping = true;
        pthread_cond_timedwait(&agent_wake_cond, &agent_sleep_mutex, &timeout);
        agent_is_sleeping = false;
        pthread_mutex_unlock(&agent_sleep_mutex);
    }

    return NULL;
}
```

#### 6. Process List Protection
**Mutex-Protected Operations**:
```c
static pthread_mutex_t process_list_mutex = PTHREAD_MUTEX_INITIALIZER;

boolean addprocess_threadsafe(hdlprocessrecord hprocess) {
    pthread_mutex_lock(&process_list_mutex);
    boolean result = addprocess_internal(hprocess);
    pthread_mutex_unlock(&process_list_mutex);
    return result;
}

void deleteprocess_threadsafe(hdlprocessrecord hprocess) {
    pthread_mutex_lock(&process_list_mutex);
    deleteprocess_internal(hprocess);
    pthread_mutex_unlock(&process_list_mutex);
}
```

---

## ODB Thread Safety Strategy

### Critical Sections
**Database Operations**:
- `dbopen()`, `dbclose()` - File I/O, must be serialized
- `dbrefhandle()` - Block lookup, read-only (can be concurrent with locks)
- `dballocate()`, `dbdeallocate()` - Allocation bitmap mutation (requires lock)

**Hash Table Operations**:
- `hashtableassign()` - Inserts new entry (requires lock on table)
- `hashtablelookup()` - Read-only (can be concurrent with RW lock)
- `hashtabledelete()` - Deletes entry (requires exclusive lock)

### Proposed Locking Strategy
**Database-Level RW Lock**:
```c
typedef struct {
    pthread_rwlock_t db_lock;       // Protects database file structure
    pthread_mutex_t alloc_lock;     // Protects allocation bitmap
    pthread_mutex_t header_lock;    // Protects header updates
} db_locks;

// Global lock registry (one per open database)
static db_locks *db_lock_registry[MAX_OPEN_DBS];
```

**Hash Table-Level Locks**:
```c
typedef struct {
    hdlhashtable htable;
    pthread_rwlock_t rw_lock;
} hashtable_with_lock;

// Embedded in tyhashtablerecord (or parallel registry)
```

**Lock Acquisition Order** (prevent deadlock):
1. Database lock (outermost)
2. Hash table lock
3. Process list lock
4. Thread registry lock (innermost)

---

## Time Slice Enforcement

### Cooperative Approach (Phase 1)
**Voluntary Yield**:
```c
boolean processyield_posix(void) {
    // Check if time slice expired
    if (time_slice_expired()) {
        pthread_yield();  // Hint to scheduler
        reset_time_slice();
    }
    return true;
}
```

### Preemptive Approach (Phase 2+)
**Signal-Based Interruption**:
```c
// Use SIGALRM or SIGPROF to interrupt long-running scripts
void time_slice_signal_handler(int sig) {
    // Set flag checked by interpreter main loop
    time_slice_exceeded = true;
}

void setup_time_slice_timer(long ticks) {
    struct itimerval timer;
    timer.it_value.tv_sec = ticks / 60;
    timer.it_value.tv_usec = (ticks % 60) * (1000000 / 60);
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = 0;
    setitimer(ITIMER_PROF, &timer, NULL);
}
```

---

## Callback Execution from C Threads

### Problem Statement
**tcp.listenStream** must execute UserTalk callbacks from accept thread:
```c
void *accept_thread_main(void *params) {
    while (running) {
        int client_fd = accept(listen_fd, ...);

        // NEED TO EXECUTE: callback_script(client_fd)
        // HOW TO CALL UserTalk FROM C THREAD?
    }
}
```

### Solution: Thread-Local Context Initialization
**Pattern**:
```c
boolean execute_usertalk_callback_from_pthread(hdltreenode hcallback, tyvaluerecord *params) {
    // 1. Create thread-local globals for this pthread
    hdlthreadglobals hglobals;
    if (!newthreadglobals(&hglobals))
        return false;

    // 2. Set as current thread's TLS
    pthread_setspecific(threadglobals_key, hglobals);
    hthreadglobals = hglobals;

    // 3. Initialize database context (if needed)
    // (Current database file descriptor, root table, etc.)

    // 4. Execute UserTalk callback
    tyvaluerecord result;
    boolean success = langcallscript(hcallback, params, &result);

    // 5. Cleanup thread globals
    disposethreadglobals(hglobals);

    return success;
}
```

**Integration with tcp.listenStream**:
```c
void *tcp_accept_thread_main(void *params) {
    tcp_listen_context *ctx = (tcp_listen_context *)params;

    while (ctx->running) {
        int client_fd = accept(ctx->listen_fd, ...);
        if (client_fd < 0) continue;

        // Spawn handler thread for this connection
        pthread_t handler_thread;
        connection_handler_params *handler_params = malloc(...);
        handler_params->client_fd = client_fd;
        handler_params->callback_script = ctx->callback_script;

        pthread_create(&handler_thread, NULL, connection_handler_thread, handler_params);
        pthread_detach(handler_thread);  // Auto-cleanup
    }

    return NULL;
}

void *connection_handler_thread(void *params) {
    connection_handler_params *ctx = (connection_handler_params *)params;

    // Execute callback with connection context
    execute_usertalk_callback_from_pthread(ctx->callback_script, ctx->client_fd);

    // Cleanup
    close(ctx->client_fd);
    free(ctx);
    return NULL;
}
```

---

## Memory Management Considerations

### Thread Stack Size
**Default pthread stack**: 8MB (Linux), 512KB (macOS)

**Frontier Requirements**:
- UserTalk interpreter recursion depth (unknown max)
- ODB handle tree depth (max ~10 levels)
- Process stack depth (max 5 levels)

**Recommendation**: 2MB per thread (explicit `pthread_attr_setstacksize()`)

### Handle Lifecycle
**Problem**: Handles allocated in one thread, disposed in another

**Solution**: Reference counting (already partially implemented)
```c
// Ensure all handle allocations are thread-safe
hdlhashtable hashtableref(hdlhashtable h) {
    atomic_increment(&(**h).refcount);
    return h;
}

void hashtableunref(hdlhashtable h) {
    if (atomic_decrement(&(**h).refcount) == 0) {
        disposehashtable(h);
    }
}
```

### Thread-Local Allocations
**Current**: Handles allocated via `NewHandle()` (Thread Manager globals)

**Proposed**: Ensure `NewHandle()` implementation is thread-safe
- macOS: Use `malloc()` + custom handle wrapper (already thread-safe)
- Validate all `NewHandle()` / `DisposeHandle()` calls are protected

---

## Error Handling and Debugging

### Thread Crash Isolation
**Signal Handlers**:
```c
void thread_crash_handler(int sig) {
    // Log crash
    log_error(LOG_COMP_PROCESS, "Thread %ld crashed with signal %d",
              pthread_self(), sig);

    // Mark thread as crashed in registry
    frontier_pthread_record *thread = get_current_thread_record();
    if (thread) {
        thread->crashed = true;
    }

    // Exit thread cleanly
    pthread_exit(NULL);
}

void setup_thread_signal_handlers(void) {
    signal(SIGSEGV, thread_crash_handler);
    signal(SIGBUS, thread_crash_handler);
    signal(SIGABRT, thread_crash_handler);
}
```

### Deadlock Detection
**Watchdog Thread** (optional, Phase 3+):
```c
void *deadlock_watchdog(void *params) {
    while (true) {
        sleep(60);  // Check every minute

        // Enumerate all threads
        // Check if any thread has been blocked for >10 seconds
        // Log warning if suspected deadlock
    }
}
```

### Thread Naming
**For Debugging**:
```c
void set_thread_name(const char *name) {
#ifdef __APPLE__
    pthread_setname_np(name);
#elif defined(__linux__)
    pthread_setname_np(pthread_self(), name);
#endif
}

// Usage:
set_thread_name("frontier-agent");
set_thread_name("frontier-oneshot-127");
set_thread_name("tcp-accept");
```

---

## Performance Considerations

### Thread Pool (Future Optimization)
**Problem**: Creating pthread per one-shot is expensive (1-2ms overhead)

**Solution** (Phase 3+): Thread pool for one-shot execution
```c
typedef struct {
    pthread_t threads[THREAD_POOL_SIZE];
    work_queue_t work_queue;
    pthread_mutex_t queue_mutex;
    pthread_cond_t work_available;
} thread_pool_t;

void thread_pool_submit(tythreadmaincallback callback, tythreadmainparams params) {
    pthread_mutex_lock(&pool.queue_mutex);
    enqueue_work(&pool.work_queue, callback, params);
    pthread_cond_signal(&pool.work_available);
    pthread_mutex_unlock(&pool.queue_mutex);
}
```

### Lock Contention Mitigation
**Read-Heavy Workloads**: Use `pthread_rwlock_t` for ODB read operations

**Fine-Grained Locking**: Per-hash-table locks (not global lock)

**Lock-Free Structures**: Consider atomic operations for thread registry lookup

---

## Backward Compatibility Verification

### Test Matrix

| Verb | Legacy Behavior | POSIX Behavior | Compatibility |
|------|----------------|----------------|---------------|
| `thread.sleep(id)` | Indefinite sleep, cooperative | Indefinite sleep, cond_wait | ✅ Compatible |
| `thread.wake(id)` | Mark awake, scheduler picks up | Immediate cond_signal | ✅ Compatible |
| `thread.kill(id)` | Mark killed, cleanup on swap | Send pthread_cancel | ⚠️ Timing difference |
| `thread.getCurrentID()` | Return `(long)hthreadglobals` | Return `user_thread_id` | ✅ Compatible (opaque) |
| `thread.getCount()` | Count linked list | Count registry entries | ✅ Compatible |
| `thread.getNthID(n)` | Iterate linked list | Iterate registry | ✅ Compatible |
| `thread.evaluate(s)` | Create green thread | Create pthread | ✅ Compatible |

**Compatibility Risk**: Thread kill timing (immediate vs deferred cleanup)

---

## Migration Risks and Mitigation

### Risk 1: Race Conditions in ODB Access
**Mitigation**: Phase 1 uses global ODB lock (serialized access, zero risk)

### Risk 2: Thread ID Collisions
**Mitigation**: Use atomic counter for ID generation

### Risk 3: Deadlocks from Lock Ordering
**Mitigation**: Document and enforce strict lock acquisition order

### Risk 4: Memory Leaks from Thread Crashes
**Mitigation**: Use pthread cleanup handlers (`pthread_cleanup_push`)

### Risk 5: Performance Regression
**Mitigation**: Benchmark before/after, optimize lock granularity

---

## Implementation Phases (Summary)

**Phase 1**: Core threading infrastructure (sleep/wake/kill)
**Phase 2**: Thread execution (evaluate/callScript)
**Phase 3**: Agent thread migration
**Phase 4**: ODB thread safety (fine-grained locks)
**Phase 5**: Performance optimization (thread pool, lock-free structures)

See `IMPLEMENTATION_PLAN.md` for detailed phase breakdown.
