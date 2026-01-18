# Threading Implementation Plan

**Date**: 2026-01-16
**Purpose**: Phased execution plan for POSIX threading migration

---

## Executive Summary

This plan details a **5-phase migration** from cooperative green threads to POSIX threads, with each phase deliverable as a separate PR. Tasks are marked with:
- 🟢 **Haiku**: Straightforward pattern-following work
- 🟡 **Sonnet**: Complex logic requiring architecture decisions

**Critical Dependencies**:
- **Phase 1-3**: Required for TCP networking (`tcp.listenStream` callback execution)
- **Phase 4-5**: Performance and scalability (can defer post-launch)

**Estimated Timeline**: 8-12 weeks (Phase 1-3), +4-6 weeks (Phase 4-5)

---

## Phase 1: Core Threading Infrastructure (Week 1-2)

**Goal**: Establish pthread foundation with thread ID mapping and basic lifecycle

**Deliverables**:
1. Thread registry structure
2. Thread ID allocation/lookup
3. pthread creation/destruction wrapper
4. Thread-local storage via `pthread_setspecific()`

---

### Phase 1 Tasks

#### 1.1: Create Thread Registry Data Structures
🟢 **Haiku** - Straightforward struct definition

**Files to Create**:
- `Common/headers/threadregistry.h`
- `Common/source/threadregistry.c`

**Implementation**:
```c
// threadregistry.h
typedef struct {
    pthread_t pthread_id;
    hdlthreadglobals hglobals;
    long user_thread_id;  // UserTalk-visible ID
    pthread_cond_t wake_cond;
    pthread_mutex_t state_mutex;
    boolean is_sleeping;
    unsigned long wakeup_ticks;
    boolean is_killed;
} frontier_pthread_record;

extern frontier_pthread_record *thread_registry;
extern int thread_registry_size;
extern int thread_registry_capacity;
extern pthread_mutex_t registry_mutex;
extern long next_thread_id;

// Registry operations
boolean init_thread_registry(void);
frontier_pthread_record *allocate_thread_record(void);
void free_thread_record(frontier_pthread_record *rec);
frontier_pthread_record *get_thread_by_id(long user_id);
frontier_pthread_record *get_current_thread_record(void);
long allocate_thread_id(void);
```

**Tests to Write** (before implementation):
- `test_thread_registry_init()` - Verify registry initialization
- `test_thread_id_allocation()` - IDs are unique and monotonic
- `test_thread_lookup()` - Lookup by ID succeeds/fails correctly

---

#### 1.2: Implement Thread Registry Operations
🟢 **Haiku** - Pattern-following mutex-protected operations

**Implementation**:
```c
// threadregistry.c
boolean init_thread_registry(void) {
    thread_registry_capacity = 16;  // Initial size
    thread_registry = (frontier_pthread_record *)calloc(
        thread_registry_capacity, sizeof(frontier_pthread_record));
    return (thread_registry != NULL);
}

frontier_pthread_record *allocate_thread_record(void) {
    pthread_mutex_lock(&registry_mutex);

    // Grow registry if needed
    if (thread_registry_size >= thread_registry_capacity) {
        // Realloc to 2x capacity
        int new_cap = thread_registry_capacity * 2;
        frontier_pthread_record *new_reg = (frontier_pthread_record *)realloc(
            thread_registry, new_cap * sizeof(frontier_pthread_record));
        if (!new_reg) {
            pthread_mutex_unlock(&registry_mutex);
            return NULL;
        }
        thread_registry = new_reg;
        thread_registry_capacity = new_cap;
    }

    // Find first unused slot
    frontier_pthread_record *rec = &thread_registry[thread_registry_size++];
    memset(rec, 0, sizeof(*rec));

    pthread_mutex_unlock(&registry_mutex);
    return rec;
}

long allocate_thread_id(void) {
    pthread_mutex_lock(&registry_mutex);
    long id = next_thread_id++;
    pthread_mutex_unlock(&registry_mutex);
    return id;
}

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

**Tests**:
- `test_allocate_thread_record()` - Allocation succeeds
- `test_registry_growth()` - Auto-grow when capacity exceeded
- `test_concurrent_allocation()` - Thread-safe under concurrent access

---

#### 1.3: pthread Thread-Local Storage Setup
🟢 **Haiku** - Standard pthread_key pattern

**Files to Modify**:
- `Common/headers/processinternal.h` (add TLS accessor prototype)
- `Common/source/process.c` (implement TLS key)

**Implementation**:
```c
// process.c
static pthread_key_t threadglobals_key;
static pthread_once_t key_once = PTHREAD_ONCE_INIT;

static void make_threadglobals_key(void) {
    pthread_key_create(&threadglobals_key, (void (*)(void *))disposethreadglobals);
}

boolean init_threading_system(void) {
    pthread_once(&key_once, make_threadglobals_key);
    return init_thread_registry();
}

hdlthreadglobals getcurrentthreadglobals_tls(void) {
    hdlthreadglobals hg = (hdlthreadglobals)pthread_getspecific(threadglobals_key);
    if (hg == NULL) {
        // Fall back to global (main thread or early init)
        return hthreadglobals;
    }
    return hg;
}
```

**Tests**:
- `test_tls_key_creation()` - Key created successfully
- `test_tls_get_set()` - Set in one thread, retrieve in same thread
- `test_tls_isolation()` - Thread A cannot see Thread B's TLS

---

#### 1.4: pthread Creation Wrapper
🟡 **Sonnet** - Integrates multiple components, error handling

**Files to Modify**:
- `Common/headers/threads.h` (update `newthread()` signature if needed)
- `Common/source/threads_posix.c` (new file for POSIX implementation)

**Implementation**:
```c
// threads_posix.c
typedef struct {
    tythreadmaincallback callback;
    tythreadmainparams callback_params;
    frontier_pthread_record *thread_rec;
} pthread_wrapper_params;

static void *frontier_pthread_wrapper(void *arg) {
    pthread_wrapper_params *params = (pthread_wrapper_params *)arg;

    // Set thread-local globals
    pthread_setspecific(threadglobals_key, params->thread_rec->hglobals);
    hthreadglobals = params->thread_rec->hglobals;  // Also set global

    // Store user thread ID in globals
    (**params->thread_rec->hglobals).idthread =
        (hdlthread)params->thread_rec->user_thread_id;

    // Call original callback
    void *result = params->callback(params->callback_params);

    // Cleanup
    free(params);
    return result;
}

boolean newthread_posix(tythreadmaincallback callback,
                        tythreadmainparams params,
                        void *threadglobals,
                        hdlthread *idthread_out) {
    // Allocate thread record
    frontier_pthread_record *thread_rec = allocate_thread_record();
    if (!thread_rec) return false;

    thread_rec->hglobals = (hdlthreadglobals)threadglobals;
    thread_rec->user_thread_id = allocate_thread_id();

    // Initialize condition variable and mutex
    pthread_cond_init(&thread_rec->wake_cond, NULL);
    pthread_mutex_init(&thread_rec->state_mutex, NULL);
    thread_rec->is_sleeping = false;

    // Create wrapper params
    pthread_wrapper_params *wrapper_params = (pthread_wrapper_params *)malloc(
        sizeof(pthread_wrapper_params));
    wrapper_params->callback = callback;
    wrapper_params->callback_params = params;
    wrapper_params->thread_rec = thread_rec;

    // Create pthread
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, 2 * 1024 * 1024);  // 2MB stack
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    int result = pthread_create(&thread_rec->pthread_id, &attr,
                                 frontier_pthread_wrapper, wrapper_params);
    pthread_attr_destroy(&attr);

    if (result != 0) {
        pthread_cond_destroy(&thread_rec->wake_cond);
        pthread_mutex_destroy(&thread_rec->state_mutex);
        free_thread_record(thread_rec);
        free(wrapper_params);
        return false;
    }

    *idthread_out = (hdlthread)thread_rec->user_thread_id;
    return true;
}
```

**Tests**:
- `test_create_pthread()` - Thread created successfully
- `test_thread_main_executes()` - Thread main callback runs
- `test_thread_tls_initialized()` - TLS available in thread main

---

#### 1.5: Thread Cleanup and Disposal
🟢 **Haiku** - Standard cleanup pattern

**Implementation**:
```c
void cleanup_thread_record(frontier_pthread_record *rec) {
    if (rec->hglobals) {
        disposethreadglobals(rec->hglobals);
        rec->hglobals = NULL;
    }

    pthread_cond_destroy(&rec->wake_cond);
    pthread_mutex_destroy(&rec->state_mutex);

    // Remove from registry
    pthread_mutex_lock(&registry_mutex);
    // Compact registry (move last element into this slot)
    int index = rec - thread_registry;
    if (index < thread_registry_size - 1) {
        thread_registry[index] = thread_registry[thread_registry_size - 1];
    }
    thread_registry_size--;
    pthread_mutex_unlock(&registry_mutex);
}
```

**Tests**:
- `test_thread_cleanup()` - Resources freed correctly
- `test_registry_compaction()` - Removed thread not in registry

---

### Phase 1 Integration

**Build System Changes**:
- Add `threadregistry.c` and `threads_posix.c` to `CMakeLists.txt`
- Link against `-lpthread`

**Initialization**:
- Call `init_threading_system()` in `initprocess()` or `main()`

**Testing**:
- Run unit tests: `./tools/run_headless_tests.sh`
- Verify no regressions in existing thread verbs (should no-op or fallback)

---

### Phase 1 PR Checklist
- [ ] All unit tests pass
- [ ] Integration tests pass
- [ ] No memory leaks (valgrind clean)
- [ ] Documentation updated (`docs/THREADING_ARCHITECTURE.md`)
- [ ] Code review by user

---

## Phase 2: Sleep/Wake/Kill Primitives (Week 3-4)

**Goal**: Implement POSIX-based sleep/wake/kill using condition variables

**Deliverables**:
1. `thread.sleep(id)` using `pthread_cond_wait()`
2. `thread.wake(id)` using `pthread_cond_signal()`
3. `thread.kill(id)` using `pthread_cancel()`
4. `thread.sleepFor()` and `thread.sleepTicks()` with timeout
5. `thread.isSleeping()` state query

---

### Phase 2 Tasks

#### 2.1: Implement thread.sleep(id)
🟢 **Haiku** - Standard cond_wait pattern

**Files to Modify**:
- `Common/source/process.c` - Update `processsleep()`

**Implementation**:
```c
boolean processsleep_posix(hdlprocessthread hthread, long ticks) {
    frontier_pthread_record *thread = get_thread_by_user_handle(hthread);
    if (!thread) return false;

    pthread_mutex_lock(&thread->state_mutex);
    thread->is_sleeping = true;

    if (ticks < 0) {
        // Indefinite sleep
        pthread_cond_wait(&thread->wake_cond, &thread->state_mutex);
    } else {
        // Timed sleep
        struct timespec timeout;
        clock_gettime(CLOCK_REALTIME, &timeout);
        long seconds = ticks / 60;
        long nanoseconds = (ticks % 60) * (1000000000L / 60);
        timeout.tv_sec += seconds;
        timeout.tv_nsec += nanoseconds;
        if (timeout.tv_nsec >= 1000000000L) {
            timeout.tv_sec += 1;
            timeout.tv_nsec -= 1000000000L;
        }

        pthread_cond_timedwait(&thread->wake_cond, &thread->state_mutex, &timeout);
    }

    thread->is_sleeping = false;
    boolean was_killed = thread->is_killed;
    pthread_mutex_unlock(&thread->state_mutex);

    return !was_killed;
}
```

**Tests**:
- `test_sleep_indefinite()` - Thread sleeps until woken
- `test_sleep_timed()` - Thread wakes after timeout
- `test_sleep_killed()` - Killed thread wakes immediately

---

#### 2.2: Implement thread.wake(id)
🟢 **Haiku** - Standard cond_signal pattern

**Implementation**:
```c
boolean wakeprocessthread_posix(hdlprocessthread hthread) {
    frontier_pthread_record *thread = get_thread_by_user_handle(hthread);
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

**Tests**:
- `test_wake_sleeping_thread()` - Returns true
- `test_wake_awake_thread()` - Returns false
- `test_wake_nonexistent_thread()` - Returns false (error)

---

#### 2.3: Implement thread.kill(id)
🟡 **Sonnet** - Thread cancellation is complex, requires cleanup handlers

**Implementation**:
```c
boolean killprocessthread_posix(hdlprocessthread hthread) {
    frontier_pthread_record *thread = get_thread_by_user_handle(hthread);
    if (!thread) return false;

    pthread_mutex_lock(&thread->state_mutex);
    thread->is_killed = true;

    // If sleeping, wake it so it can exit
    if (thread->is_sleeping) {
        pthread_cond_signal(&thread->wake_cond);
    }
    pthread_mutex_unlock(&thread->state_mutex);

    // Send cancellation request
    pthread_cancel(thread->pthread_id);

    return true;
}

// In thread wrapper, check is_killed flag:
static void *frontier_pthread_wrapper(void *arg) {
    pthread_cleanup_push(cleanup_on_cancel, arg);

    // ... initialization ...

    // Main callback
    void *result = params->callback(params->callback_params);

    pthread_cleanup_pop(1);  // Execute cleanup
    return result;
}

static void cleanup_on_cancel(void *arg) {
    pthread_wrapper_params *params = (pthread_wrapper_params *)arg;
    cleanup_thread_record(params->thread_rec);
    free(params);
}
```

**Tests**:
- `test_kill_thread()` - Thread terminates
- `test_kill_sleeping_thread()` - Wakes and terminates
- `test_kill_cleanup()` - Resources freed correctly

---

#### 2.4: Implement thread.isSleeping(id)
🟢 **Haiku** - Simple state query

**Implementation**:
```c
boolean processissleeping_posix(hdlprocessthread hthread) {
    frontier_pthread_record *thread = get_thread_by_user_handle(hthread);
    if (!thread) return false;

    pthread_mutex_lock(&thread->state_mutex);
    boolean is_sleeping = thread->is_sleeping;
    pthread_mutex_unlock(&thread->state_mutex);

    return is_sleeping;
}
```

**Tests**:
- `test_issleeping_sleeping()` - Returns true
- `test_issleeping_awake()` - Returns false

---

#### 2.5: Update Kernel Verb Dispatch
🟢 **Haiku** - Simple function pointer swap

**Files to Modify**:
- `Common/source/shellsysverbs.c` - Update `sleepfunc`, `wakefunc`, `killfunc` cases

**Implementation**:
```c
// In shellsysverbs.c
case sleepfunc: {
    hdlprocessthread hthread;
    flnextparamislast = true;
    if (!getthreadvalue(hparam1, 1, &hthread))
        return false;
    // Use POSIX implementation
    return setbooleanvalue(processsleep_posix(hthread, -1), v);
}
```

**Tests**:
- `test_sleep_verb()` - UserTalk `thread.sleep(id)` works
- `test_wake_verb()` - UserTalk `thread.wake(id)` works
- `test_kill_verb()` - UserTalk `thread.kill(id)` works

---

### Phase 2 Integration Tests

**Test Script** (`tests/integration/threading/test_sleep_wake.yaml`):
```yaml
tests:
  - name: "thread.sleep - indefinite sleep woken by wake"
    script: |
      local(id = thread.evaluate("thread.sleepFor(60)"));
      clock.waitSeconds(0.1);
      thread.wake(id);
      return thread.isSleeping(id) == false;
    expected_success: true

  - name: "thread.sleepFor - wakes after timeout"
    script: |
      local(startTime = clock.ticks());
      thread.sleepFor(0.1);  // 0.1 minutes = 6 seconds
      local(elapsed = clock.ticks() - startTime);
      return (elapsed >= 6 * 60) and (elapsed < 8 * 60);  // 6-8 seconds
    expected_success: true

  - name: "thread.kill - terminates sleeping thread"
    script: |
      local(id = thread.evaluate("thread.sleepFor(60)"));
      clock.waitSeconds(0.1);
      thread.kill(id);
      clock.waitSeconds(0.1);
      return thread.exists(id) == false;
    expected_success: true
```

---

## Phase 3: Thread Execution and Callbacks (Week 5-7)

**Goal**: Enable UserTalk script execution in pthreads

**Deliverables**:
1. `thread.evaluate(script)` creates pthread running UserTalk
2. `thread.callScript(adr, params, context)` executes script with parameters
3. `thread.evaluateTo(script, adr)` stores result
4. UserTalk callback execution from C threads (for tcp.listenStream)

---

### Phase 3 Tasks

#### 3.1: Implement thread.evaluate(script)
🟡 **Sonnet** - Requires UserTalk compilation and execution in pthread

**Files to Modify**:
- `Common/source/shellsysverbs.c` - Update `evaluatefunc`
- `Common/source/process.c` - Add `execute_usertalk_in_thread()`

**Implementation**:
```c
// process.c
typedef struct {
    hdltreenode hcode;      // Compiled script
    hdlhashtable hcontext;  // Execution context
} usertalk_exec_params;

static void *usertalk_thread_main(void *arg) {
    usertalk_exec_params *params = (usertalk_exec_params *)arg;

    // Thread-local globals already set by wrapper

    // Execute script
    tyvaluerecord result;
    boolean success = langrun(params->hcode, &result);

    // Cleanup
    langdisposetree(params->hcode);
    if (params->hcontext) {
        disposehashtable(params->hcontext);
    }
    free(params);

    return (void *)(long)success;
}

boolean thread_evaluate_string(bigstring script_string, long *thread_id_out) {
    // 1. Compile script
    hdltreenode hcode;
    if (!langcompiletext(script_string, false, &hcode))
        return false;

    // 2. Create execution params
    usertalk_exec_params *params = (usertalk_exec_params *)malloc(
        sizeof(usertalk_exec_params));
    params->hcode = hcode;
    params->hcontext = nil;

    // 3. Create thread (via newprocessthread wrapper)
    hdlprocessthread hthread;
    if (!newprocessthread(&usertalk_thread_main, params, &hthread)) {
        langdisposetree(hcode);
        free(params);
        return false;
    }

    *thread_id_out = getthreadid(hthread);
    return true;
}
```

**Tests**:
- `test_evaluate_simple_script()` - `thread.evaluate("1+1")` creates thread
- `test_evaluate_returns_thread_id()` - Returns valid thread ID
- `test_evaluate_script_executes()` - Script actually runs

---

#### 3.2: Implement thread.callScript(adr, params, context)
🟡 **Sonnet** - Parameter marshalling, context handling

**Implementation**:
```c
typedef struct {
    hdltreenode hscript;
    tyvaluerecord *param_array;
    int param_count;
    hdlhashtable hcontext;
} callscript_params;

static void *callscript_thread_main(void *arg) {
    callscript_params *params = (callscript_params *)arg;

    // Set up parameter table if context provided
    if (params->hcontext) {
        // Push context table onto table stack
        pushhashtable(params->hcontext);
    }

    // Call script with parameters
    tyvaluerecord result;
    boolean success = langcallscript(params->hscript, params->param_array,
                                      params->param_count, &result);

    if (params->hcontext) {
        pophashtable();
    }

    // Cleanup
    for (int i = 0; i < params->param_count; i++) {
        disposevaluerecord(params->param_array[i], false);
    }
    free(params->param_array);
    if (params->hcontext) {
        disposehashtable(params->hcontext);
    }
    free(params);

    return (void *)(long)success;
}
```

**Tests**:
- `test_callscript_with_params()` - Parameters passed correctly
- `test_callscript_with_context()` - Context table accessible
- `test_callscript_returns_thread_id()` - Valid thread ID returned

---

#### 3.3: C Thread → UserTalk Callback Execution
🟡 **Sonnet** - Critical for tcp.listenStream, requires full context init

**Files to Create**:
- `Common/headers/thread_callback.h`
- `Common/source/thread_callback.c`

**Implementation**:
```c
// thread_callback.h
boolean execute_usertalk_callback_from_pthread(
    hdltreenode hcallback,
    tyvaluerecord *params,
    int param_count,
    tyvaluerecord *result_out);

// thread_callback.c
boolean execute_usertalk_callback_from_pthread(
    hdltreenode hcallback,
    tyvaluerecord *params,
    int param_count,
    tyvaluerecord *result_out) {

    // 1. Create thread-local globals
    hdlthreadglobals hglobals;
    if (!newthreadglobals(&hglobals))
        return false;

    // 2. Set as current thread's TLS
    pthread_setspecific(threadglobals_key, hglobals);
    hthreadglobals = hglobals;

    // 3. Initialize minimal thread context
    // (Database context already set by calling thread if needed)

    // 4. Execute callback
    boolean success = langcallscript(hcallback, params, param_count, result_out);

    // 5. Cleanup (but don't dispose hglobals if thread continues)
    // disposethreadglobals(hglobals);

    return success;
}
```

**Usage in tcp.listenStream**:
```c
void *connection_handler_thread(void *arg) {
    connection_context *ctx = (connection_context *)arg;

    // Create UserTalk parameter (connection descriptor)
    tyvaluerecord param;
    setlongvalue(ctx->client_fd, &param);

    // Execute callback
    tyvaluerecord result;
    execute_usertalk_callback_from_pthread(ctx->callback_script, &param, 1, &result);

    // Cleanup
    close(ctx->client_fd);
    free(ctx);
    return NULL;
}
```

**Tests**:
- `test_callback_from_c_thread()` - UserTalk callback runs in pthread
- `test_callback_tls_initialized()` - Thread globals available
- `test_callback_odb_access()` - Can access database from callback

---

#### 3.4: Thread Introspection Verbs (getCurrentID, getCount, getNthID)
🟢 **Haiku** - Simple registry queries

**Implementation**:
```c
// thread.getCurrentID()
long get_current_thread_id(void) {
    frontier_pthread_record *rec = get_current_thread_record();
    return rec ? rec->user_thread_id : 0;
}

// thread.getCount()
long get_thread_count(void) {
    pthread_mutex_lock(&registry_mutex);
    int count = thread_registry_size;
    pthread_mutex_unlock(&registry_mutex);
    return count;
}

// thread.getNthID(n)
long get_nth_thread_id(int n) {
    if (n < 1) return 0;

    pthread_mutex_lock(&registry_mutex);
    if (n > thread_registry_size) {
        pthread_mutex_unlock(&registry_mutex);
        return 0;
    }
    long id = thread_registry[n - 1].user_thread_id;
    pthread_mutex_unlock(&registry_mutex);
    return id;
}
```

**Tests**:
- `test_get_current_id()` - Returns valid ID
- `test_get_count()` - Counts all threads
- `test_get_nth_id()` - Returns correct ID for index

---

### Phase 3 Integration Tests

**Test Script** (`tests/integration/threading/test_execution.yaml`):
```yaml
tests:
  - name: "thread.evaluate - executes script and returns thread ID"
    script: |
      local(id = thread.evaluate("1+1"));
      return (typeof(id) == longType) and (id > 0);
    expected_success: true

  - name: "thread.callScript - executes with parameters"
    script: |
      on test(x, y) { return x + y; };
      local(id = thread.callScript(@test, {3, 4}));
      return typeof(id) == longType;
    expected_success: true

  - name: "thread.getCurrentID - returns current thread ID"
    script: |
      local(myID = thread.getCurrentID());
      return (myID > 0);
    expected_success: true

  - name: "thread.getCount - counts all threads"
    script: |
      local(count1 = thread.getCount());
      local(id = thread.evaluate("thread.sleepFor(1)"));
      local(count2 = thread.getCount());
      return (count2 == count1 + 1);
    expected_success: true
```

---

## Phase 4: Agent Thread Migration (Week 8-10)

**Goal**: Migrate agent thread to POSIX implementation

**Deliverables**:
1. Agent thread as real pthread
2. Agent scheduler with mutex protection
3. `Frontier.enableAgents()` controls agent thread lifecycle
4. Agent auto-restart on kill

---

### Phase 4 Tasks

#### 4.1: Agent Thread Main Loop (POSIX)
🟡 **Sonnet** - Complex scheduling logic, mutex coordination

**Files to Modify**:
- `Common/source/process.c` - Update `agentthreadmain()`

**Implementation**:
```c
static pthread_t agent_pthread = 0;
static pthread_mutex_t agent_scheduler_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t agent_wake_cond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t agent_wake_mutex = PTHREAD_MUTEX_INITIALIZER;
static boolean agent_is_sleeping = false;

static void *agentthreadmain_posix(void *params) {
    initprocessthread(BIGSTRING("\x06" "agents"));

    // Set thread name for debugging
    pthread_setname_np("frontier-agents");

    while (flagentsenabled && ingoodthread() && !flagentsdisabled && !flshellclosingall) {
        setprocesstimeslice(processagenttimeslice);

        // Run agent scheduler (mutex-protected)
        pthread_mutex_lock(&agent_scheduler_mutex);
        agentscheduler();
        pthread_mutex_unlock(&agent_scheduler_mutex);

        // Check again before sleeping
        if (!ingoodthread() || flagentsdisabled)
            break;

        // Sleep until next scheduler run (1 second timeout)
        pthread_mutex_lock(&agent_wake_mutex);
        agent_is_sleeping = true;

        struct timespec timeout;
        clock_gettime(CLOCK_REALTIME, &timeout);
        timeout.tv_sec += 1;  // 1 second
        pthread_cond_timedwait(&agent_wake_cond, &agent_wake_mutex, &timeout);

        agent_is_sleeping = false;
        pthread_mutex_unlock(&agent_wake_mutex);
    }

    exitprocessthread();
    return NULL;
}
```

**Tests**:
- `test_agent_thread_creation()` - Agent thread created on enable
- `test_agent_thread_termination()` - Agent thread exits on disable
- `test_agent_scheduler_mutex()` - Scheduler protected by mutex

---

#### 4.2: Agent Enable/Disable
🟢 **Haiku** - Simple flag + wake mechanism

**Implementation**:
```c
boolean enable_agents_posix(boolean enable) {
    if (enable == flagentsenabled)
        return true;  // No change

    flagentsenabled = enable;

    if (enable) {
        // Start agent thread if not running
        if (agent_pthread == 0) {
            pthread_create(&agent_pthread, NULL, agentthreadmain_posix, NULL);
        } else {
            // Wake sleeping agent thread
            pthread_mutex_lock(&agent_wake_mutex);
            if (agent_is_sleeping) {
                pthread_cond_signal(&agent_wake_cond);
            }
            pthread_mutex_unlock(&agent_wake_mutex);
        }
    } else {
        // Signal agent thread to exit
        pthread_mutex_lock(&agent_wake_mutex);
        pthread_cond_signal(&agent_wake_cond);
        pthread_mutex_unlock(&agent_wake_mutex);

        // Wait for thread to exit
        if (agent_pthread != 0) {
            pthread_join(agent_pthread, NULL);
            agent_pthread = 0;
        }
    }

    return true;
}
```

**Tests**:
- `test_enable_agents()` - Agents start running
- `test_disable_agents()` - Agents stop running
- `test_reenable_agents()` - Agents restart after disable

---

#### 4.3: Agent Auto-Restart on Kill
🟢 **Haiku** - Watchdog timer + thread creation

**Implementation**:
```c
// In processscheduler()
void processscheduler_posix(void) {
    if (!flagentsenabled) return;

    // Check if agent thread exists
    pthread_mutex_lock(&agent_wake_mutex);
    boolean agent_running = (agent_pthread != 0);
    pthread_mutex_unlock(&agent_wake_mutex);

    if (!agent_running) {
        // Agent thread killed or crashed - recreate after 1 second
        static unsigned long last_restart_time = 0;
        unsigned long now = TickCount();

        if (now - last_restart_time > 60) {  // 1 second
            log_info(LOG_COMP_PROCESS, "Restarting agent thread");
            pthread_create(&agent_pthread, NULL, agentthreadmain_posix, NULL);
            last_restart_time = now;
        }
    }
}
```

**Tests**:
- `test_agent_auto_restart()` - Agent thread recreated after kill
- `test_agent_restart_delay()` - 1 second delay before restart

---

#### 4.4: Process List Thread Safety
🟡 **Sonnet** - Mutex protection for process list operations

**Implementation**:
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

void agentscheduler_threadsafe(void) {
    // Caller already holds agent_scheduler_mutex

    pthread_mutex_lock(&process_list_mutex);

    // Iterate process list, execute agents
    hdlprocessrecord hp = (**processlist).hfirstprocess;
    while (hp != nil) {
        if (!(**hp).floneshot) {
            processtimeslice(hp);
        }
        hp = (**hp).hnextprocess;
    }

    pthread_mutex_unlock(&process_list_mutex);
}
```

**Tests**:
- `test_concurrent_process_add()` - Multiple threads adding processes
- `test_concurrent_process_delete()` - Safe deletion during iteration

---

### Phase 4 Integration Tests

**Test Script** (`tests/integration/threading/test_agents.yaml`):
```yaml
tests:
  - name: "Frontier.enableAgents - starts agent thread"
    script: |
      Frontier.enableAgents(false);
      clock.waitSeconds(0.1);
      local(count1 = thread.getCount());
      Frontier.enableAgents(true);
      clock.waitSeconds(0.5);
      local(count2 = thread.getCount());
      return count2 > count1;  // Agent thread created
    expected_success: true

  - name: "Agent thread auto-restart after kill"
    script: |
      Frontier.enableAgents(true);
      local(agentID = thread.getNthID(1));
      thread.kill(agentID);
      clock.waitSeconds(2);  // Wait for auto-restart
      local(newAgentID = thread.getNthID(1));
      return newAgentID != agentID;  // New thread created
    expected_success: true
```

---

## Phase 5: ODB Thread Safety (Week 11-14)

**Goal**: Fine-grained locking for concurrent database access

**Deliverables**:
1. Database-level RW locks
2. Hash table-level locks
3. Allocation bitmap mutex
4. Lock acquisition order enforcement

**NOTE**: This phase can be deferred post-launch if Phase 1-4 use global ODB lock.

---

### Phase 5 Tasks (High-Level)

#### 5.1: Database-Level Locking
🟡 **Sonnet** - RW lock strategy, deadlock prevention

**Files to Create**:
- `Common/headers/db_locks.h`
- `Common/source/db_locks.c`

**Implementation**:
```c
typedef struct {
    hdldatabaserecord hdb;
    pthread_rwlock_t db_rwlock;
    pthread_mutex_t alloc_mutex;
    pthread_mutex_t header_mutex;
} db_lock_record;

void db_lock_read(hdldatabaserecord hdb);
void db_unlock_read(hdldatabaserecord hdb);
void db_lock_write(hdldatabaserecord hdb);
void db_unlock_write(hdldatabaserecord hdb);
```

---

#### 5.2: Hash Table Locking
🟡 **Sonnet** - Per-table locks, lock upgrade strategy

**Implementation**:
```c
typedef struct {
    hdlhashtable htable;
    pthread_rwlock_t rw_lock;
} hashtable_lock_record;

void hashtable_lock_read(hdlhashtable htable);
void hashtable_unlock_read(hdlhashtable htable);
void hashtable_lock_write(hdlhashtable htable);
void hashtable_unlock_write(hdlhashtable htable);
```

---

#### 5.3: Lock Ordering Enforcement
🟡 **Sonnet** - Deadlock detection, lock acquisition validator

**Documentation**: `docs/LOCK_ORDERING.md`

**Lock Hierarchy**:
1. Database lock (outermost)
2. Hash table lock
3. Process list lock
4. Thread registry lock (innermost)

**Enforcement**: `#ifdef DEBUG` lock order validator

---

## Integration with /doit Process

### Test-Driven Development Flow

**For Each Phase**:
1. **Write Tests First** (before implementation):
   - Unit tests for each function
   - Integration tests for verb behavior
   - Edge case tests (error conditions, race conditions)

2. **Run Tests** (should fail initially):
   ```bash
   ./tools/run_headless_tests.sh
   cd tests && make test-integration
   ```

3. **Implement Feature** (make tests pass)

4. **Refactor** (improve implementation while tests stay green)

5. **Document** (update ADRs, architecture docs)

---

### PR Cycle for Each Phase

1. **Create Feature Branch**:
   ```bash
   git worktree add ../Frontier-threading-phase1 -b feature/threading-phase1
   cd ../Frontier-threading-phase1
   ```

2. **Implement Phase** (following task list)

3. **Run Full Test Suite**:
   ```bash
   ./tools/run_headless_tests.sh
   cd tests && make test-integration
   ```

4. **Create PR** (via pull-request agent):
   ```
   Summarize changes in Phase 1: Thread registry, TLS setup, pthread wrapper
   Tests: All unit tests pass, integration tests pass
   ```

5. **Monitor PR** (background monitoring):
   ```bash
   ./tools/monitor_pr_review_bg.sh <PR_NUMBER>
   tail -f tests/tmp/pr_monitor_<PR_NUMBER>.log
   ```

6. **Address Feedback** (discuss with user before making changes)

7. **Merge** (with explicit user approval)

---

## Haiku vs Sonnet Task Distribution

### 🟢 Haiku Tasks (Straightforward, Pattern-Following)

**Phase 1**:
- Thread registry struct definition
- Registry operations (allocate, free, lookup)
- pthread TLS key creation
- Thread cleanup

**Phase 2**:
- `thread.sleep()` implementation
- `thread.wake()` implementation
- `thread.isSleeping()` implementation
- Kernel verb dispatch updates

**Phase 3**:
- `thread.getCurrentID()` implementation
- `thread.getCount()` implementation
- `thread.getNthID()` implementation

**Phase 4**:
- Agent enable/disable flags
- Agent auto-restart timer

**Total Haiku Tasks**: ~15-20 (60-70% of work)

---

### 🟡 Sonnet Tasks (Complex, Architecture Decisions)

**Phase 1**:
- pthread wrapper design (error handling, cleanup handlers)
- Thread record allocation strategy (growth, compaction)

**Phase 2**:
- Thread kill implementation (pthread_cancel, cleanup handlers)
- Timeout calculation (tick → timespec conversion)

**Phase 3**:
- `thread.evaluate()` (UserTalk compilation + execution in pthread)
- `thread.callScript()` (parameter marshalling, context handling)
- C → UserTalk callback execution (TLS init, database context)

**Phase 4**:
- Agent scheduler mutex strategy
- Process list thread safety (lock ordering, deadlock prevention)

**Phase 5**:
- Database RW lock design
- Hash table locking strategy
- Lock ordering enforcement

**Total Sonnet Tasks**: ~10-12 (30-40% of work)

---

## Dependencies and Blockers

### Phase Dependencies
- **Phase 2** depends on **Phase 1** (thread registry required for sleep/wake)
- **Phase 3** depends on **Phase 2** (script execution needs sleep/wake)
- **Phase 4** depends on **Phase 3** (agents execute scripts)
- **Phase 5** independent (can run in parallel with Phase 4)

### External Dependencies
- **tcp.listenStream** requires Phase 1-3 (callback execution from pthread)
- **Multi-user ODB** requires Phase 5 (fine-grained locking)

---

## Risk Mitigation

### Risk 1: Deadlocks from Lock Ordering Violations
**Mitigation**: Phase 5 includes lock order validator (DEBUG mode)

### Risk 2: Race Conditions in Thread Registry
**Mitigation**: Phase 1 unit tests include concurrent access tests

### Risk 3: Memory Leaks from Thread Crashes
**Mitigation**: Phase 2 includes pthread_cleanup_push handlers

### Risk 4: Performance Regression from Global Locks
**Mitigation**: Phase 1-4 acceptable for launch; Phase 5 optimizes post-launch

---

## Success Criteria

### Phase 1 Success
- [ ] All unit tests pass
- [ ] Thread registry allocates/frees correctly
- [ ] pthread creation succeeds
- [ ] Thread-local storage works

### Phase 2 Success
- [ ] `thread.sleep()` / `thread.wake()` / `thread.kill()` work
- [ ] Integration tests pass
- [ ] No race conditions under concurrent load

### Phase 3 Success
- [ ] `thread.evaluate()` executes UserTalk scripts
- [ ] C → UserTalk callbacks work (tcp.listenStream integration)
- [ ] Thread introspection verbs return correct data

### Phase 4 Success
- [ ] Agent thread runs as pthread
- [ ] Agent auto-restart works
- [ ] Process list thread-safe

### Phase 5 Success (Optional)
- [ ] Concurrent ODB reads work
- [ ] No deadlocks under load
- [ ] Performance acceptable (benchmark vs Phase 4)

---

## Timeline Estimates

| Phase | Estimated Duration | Complexity |
|-------|-------------------|------------|
| Phase 1 | 1-2 weeks | Low |
| Phase 2 | 1-2 weeks | Medium |
| Phase 3 | 2-3 weeks | High |
| Phase 4 | 2-3 weeks | High |
| Phase 5 | 3-4 weeks | Very High |

**Total for Phase 1-3** (TCP dependency): 4-7 weeks
**Total for Phase 1-4** (Full threading): 6-10 weeks
**Total for Phase 1-5** (Optimized): 9-14 weeks

---

## Next Steps

1. **Review with User**: Discuss plan, adjust priorities
2. **Start Phase 1**: Create feature branch, write tests
3. **Iterative Development**: Follow /doit process (TDD → implement → PR)
4. **Documentation**: Update ADRs and architecture docs as you go

---

## Open Questions for User

1. **Phase 5 Priority**: Defer to post-launch or implement now?
2. **Thread Pool**: Should Phase 3 include thread pool, or defer to Phase 5?
3. **Agent Scheduler**: Keep cooperative time-slicing, or switch to preemptive?
4. **Lock Granularity**: Global ODB lock acceptable for launch, or need fine-grained?
5. **Test Coverage**: Unit tests only, or also stress tests (1000s of threads)?
