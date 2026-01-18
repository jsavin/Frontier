# Thread Test Harness - C Infrastructure Implementation

**Date:** 2026-01-17
**Status:** Implementation Complete - Infrastructure Layer

## Overview

This document describes the C infrastructure implementation for deterministic thread testing in Frontier. The test harness allows UserTalk tests to control virtual time, enabling reproducible thread sleep/wake behavior without actual delays.

## Architecture

### Components Implemented

1. **Header File**: `Common/headers/shellthreads_test_harness.h`
   - Public API for test control functions
   - Internal APIs for `gettickcount()` interception
   - Documentation of safety guarantees and thread-safety model

2. **Implementation**: `Common/source/shellthreads_test_harness.c`
   - Static state structure holding virtual tick count
   - Environment variable gate (`FRONTIER_THREAD_TEST_MODE=1`)
   - All test control functions with logging

3. **Integration Point**: `tests/headless_mac_compat.c`
   - Modified `TickCount()` to check test harness before returning system time
   - Minimal change (3 lines added) for maximum isolation

## API Reference

### Test Control Functions

All functions return `false` if test mode is not enabled or operation fails.

```c
boolean thread_test_enable(void);
```
- Activates deterministic thread testing mode
- **Safety**: Only enables if `FRONTIER_THREAD_TEST_MODE=1` environment variable is set
- Initializes virtual ticks to 0 and freezes system time
- Returns `true` on success, `false` if environment variable not set

```c
boolean thread_test_disable(void);
```
- Deactivates test mode and restores real system time
- Returns `true` if test mode was active, `false` otherwise

```c
boolean thread_test_set_ticks(uint32_t ticks);
```
- Sets virtual tick count to absolute value
- Used to establish specific timing states for test scenarios
- Returns `true` on success, `false` if test mode not enabled

```c
uint32_t thread_test_get_ticks(void);
```
- Reads current virtual tick count
- Returns virtual ticks if enabled, 0 otherwise

```c
boolean thread_test_advance(uint32_t delta);
```
- Increments virtual ticks by delta (primary time advancement mechanism)
- Simulates passage of time without actual delays
- Returns `true` on success, `false` if test mode not enabled

```c
boolean thread_test_process_once(void);
```
- Executes single event loop iteration by calling `processchecktimeouts()`
- Triggers deterministic wake-ups for threads whose sleep time has elapsed
- Returns `true` on success, `false` if test mode not enabled

### Internal Functions

```c
boolean thread_test_is_enabled(void);
```
- Called by `TickCount()` to decide whether to return virtual ticks
- Returns `true` if test mode enabled AND system time frozen

```c
uint32_t thread_test_current_ticks(void);
```
- Returns current virtual tick count for `TickCount()` interception
- Only valid when test mode is enabled

## Safety Mechanisms

### 1. Environment Variable Gate

**Protection Against Accidental Activation:**
```bash
# Test mode ONLY activates with this environment variable set
FRONTIER_THREAD_TEST_MODE=1 ./frontier-cli/frontier-cli -e "thread.test.enable()"
```

Without the environment variable, `thread_test_enable()` returns `false` and logs a warning.

### 2. No ODB Persistence

All test harness state is in a static structure - never persisted to the Object Database. This ensures:
- Test mode state cannot leak between sessions
- Database files are never contaminated with test state
- Production databases remain unaffected

### 3. Logging

All state transitions are logged at appropriate levels:
- `log_info()`: Enable/disable events
- `log_debug()`: Time advancement and tick manipulation
- `log_trace()`: Event processing iterations
- `log_warn()`: Attempts to enable without environment variable

## Integration Architecture

### Tick Count Interception

**Before (real system time):**
```c
UInt32 TickCount(void) {
    uint64_t ms = frontier_time_monotonic_millis();
    return (UInt32)((ms * 3ULL) / 50ULL);  // Convert ms to 60ths/sec
}
```

**After (test harness aware):**
```c
UInt32 TickCount(void) {
    if (thread_test_is_enabled()) {
        return thread_test_current_ticks();  // Virtual time
    }

    uint64_t ms = frontier_time_monotonic_millis();
    return (UInt32)((ms * 3ULL) / 50ULL);  // Real time
}
```

**Impact:**
- Single interception point (minimal production code changes)
- Zero overhead when test mode disabled (single boolean check)
- Preserves real-time behavior for all normal operations

### Event Loop Integration

The test harness calls `processchecktimeouts()` defined in `Common/source/process.c`:

```c
void processchecktimeouts(void) {
    // Walks all threads, waking those whose timetowake <= gettickcount()
    // When test mode enabled, gettickcount() returns virtual ticks
}
```

**Test Flow:**
1. Test calls `thread.test.advance(N)` → increments virtual ticks
2. Test calls `thread.test.processOnce()` → calls `processchecktimeouts()`
3. `processchecktimeouts()` calls `gettickcount()` → gets virtual ticks
4. Threads wake deterministically based on virtual time

## Thread Safety

### Current Implementation (Single-Threaded Tests)

The test harness is **safe for single-threaded UserTalk test scripts** because:
1. UserTalk scripts execute in a single thread (no concurrent access)
2. Static state structure is only modified by test control functions
3. Read operations (`thread_test_is_enabled()`, `thread_test_current_ticks()`) are atomic

### Future Multi-Threaded Support

If Frontier gains true multi-threaded UserTalk execution, the test harness will need:
1. Mutex protection for `thread_test_harness` struct
2. Atomic operations for enable/disable state
3. Thread-local test contexts (optional enhancement)

**Current Status:** No locking needed for Phase 3 testing infrastructure.

## Build Integration

### Automatic Inclusion

The CMake build configuration uses `file(GLOB_RECURSE)` to automatically pick up all `.c` files:

```cmake
file(GLOB_RECURSE FRONTIER_SOURCES
    "${CMAKE_SOURCE_DIR}/../Common/source/*.c"
    "${CMAKE_SOURCE_DIR}/../Common/source/*.m"
)
```

**Result:** `shellthreads_test_harness.c` is automatically compiled and linked with no build file changes needed.

### Compilation Status

**Build Results:**
- ✅ No compilation errors
- ✅ No compilation warnings
- ✅ Binary successfully created: `frontier-cli/frontier-cli`
- ✅ File size: 1.5 MB (headless CLI)

## Usage Example (Future UserTalk API)

**Expected UserTalk test pattern:**

```usertalk
// Enable test mode (requires FRONTIER_THREAD_TEST_MODE=1 env var)
thread.test.enable();

// Create a thread that sleeps for 100 ticks
thread.sleepFor(100);

// Advance time by 50 ticks (thread still sleeping)
thread.test.advance(50);
thread.test.processOnce();  // Process timeouts, thread still asleep

// Advance another 50 ticks (total 100, thread should wake)
thread.test.advance(50);
thread.test.processOnce();  // Thread wakes deterministically

// Check virtual time
local(ticks = thread.test.getTicks());  // Returns 100

// Disable test mode (restore real time)
thread.test.disable();
```

## Next Steps

### Phase 1: Complete UserTalk Binding (Immediate)

**Required:**
1. Create `tests/headless_thread_verbs.c` with UserTalk verb implementations
2. Implement `thread.test.enable()`, `thread.test.disable()`
3. Implement `thread.test.setTicks()`, `thread.test.getTicks()`
4. Implement `thread.test.advance()`, `thread.test.processOnce()`
5. Add verb registrations to `kernel_verbs_init.c`

**Pattern (based on existing headless_* files):**

```c
// tests/headless_thread_verbs.c
boolean threadtestenablefunc(tyvaluerecord *vreturned) {
    return setbooleanvalue(thread_test_enable(), vreturned);
}

boolean threadtestadvancefunc(tyvaluerecord *vreturned) {
    long delta;
    if (!getlongvalue(hparam1, 1, &delta))
        return false;
    flnextparamislast = true;
    return setbooleanvalue(thread_test_advance((uint32_t)delta), vreturned);
}
```

### Phase 2: Integration Testing

**Test Coverage:**
1. Environment variable gate validation
2. Virtual tick manipulation (set, get, advance)
3. Thread wake-up determinism
4. Multiple threads with different sleep times
5. Enable/disable state transitions

### Phase 3: Documentation

**Required Documentation:**
1. UserTalk API reference for `thread.test.*` verbs
2. Test writing guide (how to use test harness)
3. Integration test examples

## Design Decisions

### Why Environment Variable Gate?

**Decision:** Require `FRONTIER_THREAD_TEST_MODE=1` to enable test mode.

**Rationale:**
- Prevents accidental activation in production
- Explicit opt-in for test scenarios
- Fail-safe if test script calls `thread.test.enable()` in production
- Easy to set in test framework: `export FRONTIER_THREAD_TEST_MODE=1`

**Alternative Rejected:** Auto-enable when `thread.test.enable()` called (too risky)

### Why Static State Instead of Thread-Local?

**Decision:** Use a single static struct for test harness state.

**Rationale:**
- Tests run in a single thread (no concurrent test execution)
- Simpler implementation (no thread globals integration needed)
- Sufficient for Phase 3 testing infrastructure
- Can be upgraded to thread-local in Phase 6+ if needed

**Alternative Considered:** Add fields to `tythreadglobals` (unnecessary complexity for current use case)

### Why Single Interception Point?

**Decision:** Only modify `TickCount()` to check test harness.

**Rationale:**
- Minimal production code changes (3 lines in 1 file)
- Single source of truth for time in Frontier
- All timing code uses `gettickcount()` macro → `TickCount()`
- Easy to audit and verify correctness

**Alternative Rejected:** Modify `processchecktimeouts()` and other timing functions (too invasive)

## Known Limitations

### Current Scope

1. **Single-Threaded Tests Only**: Test harness assumes one UserTalk thread at a time
2. **No Real-Time Mixing**: Cannot mix virtual and real time in same session
3. **No Nested Test Modes**: Cannot enable test mode recursively

### Future Enhancements

1. **Thread-Local Test Contexts**: Allow different threads to have different virtual times
2. **Real-Time Passthrough Mode**: Allow specific operations to use real time even in test mode
3. **Test Mode Nesting**: Support test-within-test scenarios

## References

### Related Files

- **Header**: `Common/headers/shellthreads_test_harness.h`
- **Implementation**: `Common/source/shellthreads_test_harness.c`
- **Integration**: `tests/headless_mac_compat.c` (TickCount interception)
- **Event Loop**: `Common/source/process.c` (processchecktimeouts)
- **Thread Globals**: `Common/headers/processinternal.h` (tythreadglobals)

### Planning Documents

- **Thread Testing Roadmap**: `planning/phase3/THREAD_TESTING_DETERMINISM.md` (if exists)
- **ADR-005**: Thread-local parameter state (pattern reference for future enhancements)

### Architecture Documents

- **Process Architecture**: `Common/headers/process.h` (thread management)
- **Thread Globals**: `Common/headers/processinternal.h` (per-thread state)
- **Logging Standards**: `docs/LOGGING_STANDARDS.md` (test harness uses structured logging)

## Conclusion

The C infrastructure for deterministic thread testing is **complete and ready for UserTalk verb binding**. The implementation provides:

✅ **Safe** - Environment variable gate prevents accidental production activation
✅ **Simple** - Single interception point, minimal code changes
✅ **Deterministic** - Virtual time ensures reproducible test results
✅ **Maintainable** - Well-documented, clean separation of concerns
✅ **Extensible** - Foundation for future enhancements (thread-local contexts, etc.)

**Next Milestone:** Implement UserTalk verb bindings in `tests/headless_thread_verbs.c` to expose test harness to test scripts.
