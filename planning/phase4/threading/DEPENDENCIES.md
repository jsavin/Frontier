# Threading Implementation Dependencies

**Date**: 2026-01-16
**Purpose**: Document dependencies and integration requirements for POSIX threading

---

## External Dependencies

### Operating System Requirements

#### POSIX Thread Support
- **macOS**: pthreads included in standard C library (no additional packages)
- **Linux**: libpthread (usually pre-installed, link with `-lpthread`)

**Required APIs**:
- `pthread_create()` - Thread creation
- `pthread_join()` / `pthread_detach()` - Thread lifecycle
- `pthread_cancel()` - Thread termination
- `pthread_mutex_*()` - Mutual exclusion
- `pthread_cond_*()` - Condition variables
- `pthread_rwlock_*()` - Reader-writer locks (Phase 5)
- `pthread_key_create()` / `pthread_setspecific()` - Thread-local storage
- `pthread_once()` - One-time initialization
- `pthread_setname_np()` - Thread naming (debugging)

**Version Requirements**:
- POSIX 1003.1c-1995 (minimum)
- POSIX 1003.1-2001 (recommended for `pthread_rwlock_t`)

**Verification**:
```bash
# macOS
man pthread_create  # Should show man page

# Linux
apt-get install libpthread-stubs0-dev  # If missing
```

---

#### Time Functions
- `clock_gettime()` - High-resolution time (for timeouts)
- `CLOCK_REALTIME` - Absolute time for `pthread_cond_timedwait()`

**macOS Note**: `clock_gettime()` available since macOS 10.12 (Sierra)

**Fallback** (if needed):
```c
#ifdef __APPLE__
#include <mach/mach_time.h>
// Use mach_absolute_time() for older macOS
#endif
```

---

### Build System Dependencies

#### CMake Changes
**File**: `CMakeLists.txt`

**Additions**:
```cmake
# Find pthreads
find_package(Threads REQUIRED)

# Add new source files
set(COMMON_SOURCES
    ${COMMON_SOURCES}
    Common/source/threadregistry.c
    Common/source/threads_posix.c
    Common/source/thread_callback.c
    Common/source/db_locks.c  # Phase 5
)

# Link pthread
target_link_libraries(frontier-cli PRIVATE Threads::Threads)
target_link_libraries(Frontier PRIVATE Threads::Threads)
```

**Platform-Specific Flags**:
```cmake
if(APPLE)
    # macOS-specific flags
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread")
elseif(UNIX)
    # Linux-specific flags
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread -D_REENTRANT")
endif()
```

---

#### Compiler Flags
**Required**:
- `-pthread` - Enable pthread support (both compile and link)
- `-D_REENTRANT` - Enable thread-safe libc functions (Linux)

**Optional** (debugging):
- `-fsanitize=thread` - ThreadSanitizer (detect data races)
- `-g` - Debug symbols (for gdb/lldb thread debugging)

**Verification**:
```bash
# Test pthread linking
echo "int main() { return 0; }" | gcc -x c - -pthread -o /tmp/test && /tmp/test && echo "OK"
```

---

## Internal Frontier Dependencies

### Core Infrastructure

#### 1. Thread-Local Storage (tythreadglobals)
**Current State**: Already implemented in `processinternal.h:118-244`

**Required Changes**: None for Phase 1-4 (existing structure sufficient)

**Integration**:
- `pthread_setspecific(threadglobals_key, hglobals)` - Set TLS on thread creation
- `pthread_getspecific(threadglobals_key)` - Retrieve TLS in thread context

**Dependency**: Must be initialized **before** UserTalk execution

---

#### 2. Memory Management (NewHandle/DisposeHandle)
**Current State**: Custom handle implementation in `memory.c`

**Thread Safety**:
- **Current**: Single-threaded, not mutex-protected
- **Required**: Ensure `NewHandle()` / `DisposeHandle()` are thread-safe

**Options**:
1. **Global Handle Lock** (Phase 1-4):
   ```c
   static pthread_mutex_t handle_mutex = PTHREAD_MUTEX_INITIALIZER;

   Handle NewHandle(long size) {
       pthread_mutex_lock(&handle_mutex);
       Handle h = NewHandle_internal(size);
       pthread_mutex_unlock(&handle_mutex);
       return h;
   }
   ```

2. **Lock-Free Handles** (Phase 5):
   - Use `malloc()` + atomic reference counting
   - Per-handle spinlock for metadata updates

**Verification**:
```bash
# Run with ThreadSanitizer
CC=clang CFLAGS="-fsanitize=thread" make
./frontier-cli -e "thread.evaluate('lang.new(tableType, @t)')"
```

---

#### 3. Database File I/O
**Current State**: Single database file descriptor per `hdldatabaserecord`

**Thread Safety Issues**:
- `lseek()` + `read()` / `write()` are **not atomic** (race conditions)
- Multiple threads seeking same file descriptor = data corruption

**Solutions**:
1. **pread() / pwrite()** (atomic position + I/O):
   ```c
   ssize_t dbread_threadsafe(hdldatabaserecord hdb, long offset, void *buf, size_t len) {
       return pread((**hdb).filebuf, buf, len, offset);
   }
   ```

2. **Database Lock** (serialize all I/O):
   ```c
   pthread_mutex_lock(&(**hdb).io_mutex);
   lseek((**hdb).filebuf, offset, SEEK_SET);
   read((**hdb).filebuf, buf, len);
   pthread_mutex_unlock(&(**hdb).io_mutex);
   ```

**Recommendation**: Use `pread()` / `pwrite()` (atomic, no lock needed)

**Dependency**: Available on POSIX.1-2001 (macOS 10.0+, Linux 2.6+)

---

#### 4. Hash Table Access
**Current State**: Unprotected read/write to `tyhashtablerecord` structures

**Thread Safety Requirements**:
- **Reads** (lookup): Can be concurrent with RW lock
- **Writes** (assign, delete): Must be exclusive

**Implementation** (Phase 5):
```c
typedef struct tyhashtablerecord {
    // Existing fields...
    pthread_rwlock_t rw_lock;  // Protects table contents
} tyhashtablerecord;

boolean hashtablelookup_threadsafe(hdlhashtable htable, bigstring bs,
                                     tyvaluerecord *val, hdlhashnode *hnode) {
    pthread_rwlock_rdlock(&(**htable).rw_lock);
    boolean result = hashtablelookup_internal(htable, bs, val, hnode);
    pthread_rwlock_unlock(&(**htable).rw_lock);
    return result;
}
```

**Dependency**: Phase 1-4 use global ODB lock; Phase 5 adds per-table locks

---

#### 5. Process List
**Current State**: Global `processlist` (unprotected linked list)

**Thread Safety Requirements**:
- Agent scheduler iterates list (read)
- `addprocess()` / `deleteprocess()` modify list (write)
- Race condition: Iteration during modification = crash

**Solution** (Phase 4):
```c
static pthread_mutex_t process_list_mutex = PTHREAD_MUTEX_INITIALIZER;

void agentscheduler_threadsafe(void) {
    pthread_mutex_lock(&process_list_mutex);

    hdlprocessrecord hp = (**processlist).hfirstprocess;
    while (hp != nil) {
        processtimeslice(hp);
        hp = (**hp).hnextprocess;
    }

    pthread_mutex_unlock(&process_list_mutex);
}
```

**Dependency**: Must be implemented in Phase 4 (agent thread migration)

---

## Integration Points

### 1. TCP Networking (tcp.listenStream)

**Dependency**: Phase 1-3 (thread execution infrastructure)

**Integration Flow**:
```c
// tcp.listenStream implementation (hypothetical)
boolean tcp_listenstream(long port, hdltreenode hcallback) {
    // 1. Create listen socket
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    bind(listen_fd, ...);
    listen(listen_fd, backlog);

    // 2. Create accept thread
    pthread_t accept_thread;
    accept_thread_params *params = malloc(...);
    params->listen_fd = listen_fd;
    params->callback_script = hcallback;

    pthread_create(&accept_thread, NULL, tcp_accept_thread_main, params);
    pthread_detach(accept_thread);

    return true;
}

void *tcp_accept_thread_main(void *arg) {
    accept_thread_params *params = (accept_thread_params *)arg;

    while (running) {
        int client_fd = accept(params->listen_fd, ...);

        // Spawn handler thread (uses Phase 3 callback execution)
        pthread_t handler;
        connection_params *conn = malloc(...);
        conn->client_fd = client_fd;
        conn->callback_script = params->callback_script;

        pthread_create(&handler, NULL, connection_handler_thread, conn);
        pthread_detach(handler);
    }

    return NULL;
}

void *connection_handler_thread(void *arg) {
    connection_params *params = (connection_params *)arg;

    // Execute UserTalk callback (Phase 3 function)
    tyvaluerecord client_param;
    setlongvalue(params->client_fd, &client_param);

    tyvaluerecord result;
    execute_usertalk_callback_from_pthread(params->callback_script,
                                            &client_param, 1, &result);

    close(params->client_fd);
    free(params);
    return NULL;
}
```

**Required APIs from Phase 3**:
- `execute_usertalk_callback_from_pthread()` - Run UserTalk from C thread
- `newthreadglobals()` - Initialize thread-local state
- `pthread_setspecific()` - Set TLS for thread

---

### 2. Multi-User Collaborative ODB

**Dependency**: Phase 5 (fine-grained ODB locking)

**Integration Requirements**:
- **Concurrent Reads**: Multiple threads reading same hash table
- **Exclusive Writes**: Single thread modifying hash table
- **Deadlock Prevention**: Lock ordering enforcement

**Lock Hierarchy** (prevents deadlock):
```
Level 1: Database lock (db_rwlock)
  └─ Level 2: Hash table lock (table_rwlock)
      └─ Level 3: Process list lock (process_list_mutex)
          └─ Level 4: Thread registry lock (registry_mutex)
```

**Usage**:
```c
// Correct lock acquisition order
void update_hash_table_in_database(hdldatabaserecord hdb, hdlhashtable htable) {
    pthread_rwlock_wrlock(&(**hdb).db_rwlock);       // Level 1
    pthread_rwlock_wrlock(&(**htable).rw_lock);       // Level 2

    hashtableassign_internal(htable, key, value);

    pthread_rwlock_unlock(&(**htable).rw_lock);
    pthread_rwlock_unlock(&(**hdb).db_rwlock);
}
```

**Dependency**: Lock order validator (DEBUG mode, Phase 5)

---

### 3. UserTalk Compiler/Interpreter

**Current State**: Single-threaded compilation and execution

**Thread Safety Requirements**:
- **Compilation** (`langcompiletext()`): Likely thread-safe (no global state)
- **Execution** (`langrun()`): Uses thread-local `tythreadglobals` (safe)
- **Symbol Resolution**: Hash table lookup (requires locks in Phase 5)

**Integration**:
- Phase 3: Compile + execute in pthread
- Phase 5: Protect symbol table access with RW locks

**Verification**:
```bash
# Stress test: 100 threads compiling/running scripts
./frontier-cli -e "
  for i = 1 to 100 {
    thread.evaluate('msg(string(clock.now()))')
  }
"
```

---

## Testing Dependencies

### Unit Testing Framework

**Current**: Custom C unit test framework in `tests/unit/`

**Required Changes**:
- Add thread-specific test utilities:
  ```c
  boolean test_concurrent_execution(test_function *funcs, int count);
  boolean test_race_condition(test_function func, int iterations);
  ```

**Example Test**:
```c
boolean test_thread_registry_concurrent_allocation(void) {
    pthread_t threads[10];
    for (int i = 0; i < 10; i++) {
        pthread_create(&threads[i], NULL, allocate_thread_record_worker, NULL);
    }
    for (int i = 0; i < 10; i++) {
        pthread_join(threads[i], NULL);
    }
    // Verify: 10 unique thread records allocated
    return (thread_registry_size == 10);
}
```

---

### Integration Testing Framework

**Current**: Python + YAML-based integration tests

**Required Changes**:
- Add timing utilities for sleep/wake tests:
  ```python
  def test_thread_sleep_for(self):
      start = time.time()
      self.run_script("thread.sleepFor(0.1)")  # 0.1 minutes = 6 seconds
      elapsed = time.time() - start
      self.assertGreaterEqual(elapsed, 6.0)
      self.assertLessEqual(elapsed, 8.0)  # Allow 2 seconds tolerance
  ```

- Add concurrency tests:
  ```yaml
  tests:
    - name: "100 concurrent threads execute correctly"
      script: |
        local(count = 0);
        for i = 1 to 100 {
          thread.evaluate("count = count + 1");
        };
        clock.waitSeconds(5);
        return count == 100;
      expected_success: true
  ```

---

### Performance Benchmarking

**Required Benchmarks**:
1. **Thread Creation Overhead**:
   - Time to create 1000 threads
   - Compare green threads vs pthreads

2. **Sleep/Wake Latency**:
   - Measure time from `thread.wake(id)` to thread resumption

3. **ODB Throughput**:
   - Concurrent hash table reads (Phase 5)
   - Compare global lock vs fine-grained locks

**Tools**:
- `time` command (basic timing)
- `perf` (Linux profiling)
- Instruments.app (macOS profiling)

**Example**:
```bash
# Benchmark thread creation
time ./frontier-cli -e "
  for i = 1 to 1000 {
    thread.evaluate('1+1')
  }
"
```

---

## Platform-Specific Considerations

### macOS

**Thread Naming**:
```c
#ifdef __APPLE__
pthread_setname_np("frontier-agent");  // macOS: no pthread_t parameter
#endif
```

**Stack Size Limits**:
- Default: 512KB per thread
- Recommendation: Set explicit 2MB via `pthread_attr_setstacksize()`

**Thread Count Limits**:
- Soft limit: 2048 threads per process
- Hard limit: System-wide limit (check `sysctl kern.maxproc`)

**Verification**:
```bash
# Check thread limits
ulimit -u  # Max user processes (threads count towards this)
```

---

### Linux

**Thread Naming**:
```c
#ifdef __linux__
pthread_setname_np(pthread_self(), "frontier-agent");  // Linux: requires pthread_t
#endif
```

**Stack Size Limits**:
- Default: 8MB per thread
- Recommendation: 2MB (smaller footprint for many threads)

**Thread Count Limits**:
- Soft limit: `/proc/sys/kernel/threads-max` (default ~32k)
- Virtual memory limit: Stack size × thread count must fit in address space

**Verification**:
```bash
# Check thread limits
cat /proc/sys/kernel/threads-max
ulimit -s  # Stack size per thread (KB)
```

---

## Migration Path

### Phase 1-4: Global ODB Lock (Launch-Ready)

**Approach**: Single global mutex protecting all ODB operations

**Pros**:
- Simple implementation
- Zero deadlock risk
- Proven correct (serialized access)

**Cons**:
- No concurrent reads (performance bottleneck)
- Scalability limited to ~10-20 concurrent threads

**Implementation**:
```c
static pthread_mutex_t global_odb_lock = PTHREAD_MUTEX_INITIALIZER;

Handle dbrefhandle_threadsafe(dbaddress adr, Handle *h) {
    pthread_mutex_lock(&global_odb_lock);
    Handle result = dbrefhandle(adr, h);
    pthread_mutex_unlock(&global_odb_lock);
    return result;
}
```

**Acceptable for Launch**: Yes (tcp.listenStream with limited concurrency)

---

### Phase 5: Fine-Grained Locks (Post-Launch Optimization)

**Approach**: Per-database and per-hash-table RW locks

**Pros**:
- Concurrent reads (high scalability)
- Multiple databases accessed in parallel

**Cons**:
- Complex implementation (deadlock risk)
- Requires lock order enforcement

**When to Implement**: After Phase 1-4 deployed and validated

---

## Backward Compatibility

### Existing Code Assumptions

**Assumption 1**: Thread IDs are opaque `long` values
- **Preserved**: Phase 1 maintains `long` user_thread_id

**Assumption 2**: Agent thread is always index 1
- **Preserved**: Phase 4 ensures agent thread is first in registry

**Assumption 3**: Cooperative scheduling (no preemption)
- **Changed**: Phase 3 introduces true parallelism (preemptive)
- **Impact**: Race conditions in UserTalk scripts may surface

**Assumption 4**: Single-threaded ODB access
- **Changed**: Phase 5 introduces concurrent access
- **Impact**: ODB implementation must be thread-safe

---

### API Stability Guarantees

**MUST NOT CHANGE**:
- Thread verb signatures (parameters, return types)
- Thread ID type (`long`)
- Agent auto-restart behavior
- Thread list enumeration order

**CAN CHANGE** (internal only):
- Thread creation mechanism (green threads → pthreads)
- Thread ID generation (pointer → counter)
- Sleep/wake implementation (polling → cond_wait)

---

## Risk Mitigation Checklist

- [ ] **pthread availability verified** on target platforms
- [ ] **Build system updated** (CMakeLists.txt, -lpthread)
- [ ] **Thread-local storage tested** (pthread_setspecific)
- [ ] **Memory management audited** (NewHandle thread safety)
- [ ] **Database I/O converted** to pread()/pwrite()
- [ ] **Lock ordering documented** and enforced
- [ ] **Integration tests pass** on all platforms
- [ ] **Performance benchmarks** show acceptable overhead
- [ ] **Backward compatibility verified** (existing scripts work)

---

## Open Questions

1. **ThreadSanitizer**: Should we run continuous TSan builds in CI?
2. **Thread Pool**: Implement in Phase 3 or defer to Phase 5?
3. **Lock-Free Structures**: Worth the complexity for Phase 5?
4. **Hybrid Locking**: Global lock for writes, fine-grained for reads?
5. **Thread Limits**: What is realistic max thread count for production?

---

## References

- POSIX.1-2001 Standard: https://pubs.opengroup.org/onlinepubs/009695399/
- pthread Programming Guide: https://computing.llnl.gov/tutorials/pthreads/
- macOS Threading Guide: https://developer.apple.com/library/archive/documentation/Cocoa/Conceptual/Multithreading/
- Linux pthread man pages: `man pthread_create`, `man pthread_mutex_lock`, etc.
