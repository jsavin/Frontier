# POSIX Threading Implementation - Planning Documents

**Date**: 2026-01-16
**Status**: Planning Complete, Ready for Implementation
**Purpose**: Enable POSIX-compliant threading for Frontier CLI

---

## Quick Links

| Document | Purpose | Read Time |
|----------|---------|-----------|
| **[THREAD_VERBS_ANALYSIS.md](THREAD_VERBS_ANALYSIS.md)** | Complete API surface analysis | 15 min |
| **[AGENTS_ANALYSIS.md](AGENTS_ANALYSIS.md)** | How Frontier agents work | 12 min |
| **[THREADING_ARCHITECTURE.md](THREADING_ARCHITECTURE.md)** | Proposed POSIX design | 25 min |
| **[IMPLEMENTATION_PLAN.md](IMPLEMENTATION_PLAN.md)** | Phased execution plan | 30 min |
| **[DEPENDENCIES.md](DEPENDENCIES.md)** | Integration requirements | 18 min |

**Total Reading Time**: ~100 minutes

---

## Executive Summary

This planning suite provides a **complete roadmap** for migrating Frontier's cooperative green thread model to **POSIX threads (pthreads)** while maintaining 100% backward compatibility with the existing thread.* verb API.

### Key Deliverables

**Phase 1-3** (Critical for TCP Networking):
- Thread registry with unique IDs
- Sleep/wake/kill primitives via condition variables
- UserTalk script execution in pthreads
- C → UserTalk callback execution (enables tcp.listenStream)

**Phase 4** (Agent Thread Migration):
- Agent thread as real pthread
- Mutex-protected process scheduling
- Auto-restart behavior preserved

**Phase 5** (Performance Optimization):
- Fine-grained database locks (RW locks)
- Per-hash-table locking
- Deadlock prevention via lock ordering

---

## Document Overview

### 1. THREAD_VERBS_ANALYSIS.md
**What it covers**:
- Complete inventory of 20+ thread.* verbs
- Kernel vs UserTalk implementation breakdown
- Thread ID semantics (NOT pthread IDs!)
- Agent thread behavior
- Backward compatibility requirements

**Key Insights**:
- Thread IDs are opaque `long` values (user-visible)
- Internally mapped to `pthread_t` via registry
- Agent thread always at index 1 (`thread.getNthID(1)`)
- Cooperative time-slicing (4 ticks per agent)

**Who should read**: Anyone implementing thread verbs, understanding existing behavior

---

### 2. AGENTS_ANALYSIS.md
**What it covers**:
- What are Frontier agents? (background UserTalk scripts)
- Agent vs thread distinction (all agents share one thread)
- Agent lifecycle (creation, scheduling, auto-restart)
- Default agents shipped with Frontier
- Agent enable/disable mechanism

**Key Insights**:
- Agents ≠ threads (agents are processes in agent thread)
- Agent thread sleeps between scheduler runs (1 second)
- Auto-recreated 1 second after kill (unless disabled)
- Process list ordering: one-shots first, agents last

**Who should read**: Anyone working on agent scheduler, process management

---

### 3. THREADING_ARCHITECTURE.md
**What it covers**:
- Proposed POSIX architecture design
- Thread ID mapping strategy (registry-based)
- Sleep/wake via condition variables
- pthread creation wrapper
- Thread-local storage via pthread_setspecific()
- ODB thread safety strategy (locks, ordering)
- Callback execution from C threads

**Key Insights**:
- One pthread per Frontier thread (true parallelism)
- Thread registry maps user IDs ↔ pthread_t
- Global ODB lock (Phase 1-4), fine-grained locks (Phase 5)
- Callback execution pattern for tcp.listenStream

**Who should read**: System architects, anyone implementing core threading infrastructure

---

### 4. IMPLEMENTATION_PLAN.md
**What it covers**:
- 5 phases with detailed task breakdown
- Haiku vs Sonnet task assignments (🟢/🟡)
- Test-driven development workflow
- PR cycle for each phase
- Success criteria and timelines
- Integration with /doit process

**Key Insights**:
- Phase 1-3: 4-7 weeks (critical for TCP)
- Phase 4: 2-3 weeks (agent migration)
- Phase 5: 3-4 weeks (optimization, optional)
- 60-70% Haiku tasks, 30-40% Sonnet tasks
- Each phase = separate PR cycle

**Who should read**: Developers implementing the plan, project managers

---

### 5. DEPENDENCIES.md
**What it covers**:
- POSIX requirements (pthreads, clock_gettime)
- Build system changes (CMake, linker flags)
- Internal Frontier dependencies (TLS, memory, ODB)
- Integration points (tcp.listenStream, multi-user ODB)
- Testing requirements
- Platform-specific considerations (macOS, Linux)

**Key Insights**:
- Requires POSIX.1-2001 (standard on modern macOS/Linux)
- Link with `-lpthread`
- Database I/O must use pread()/pwrite() (atomic)
- Global ODB lock acceptable for launch

**Who should read**: Build engineers, platform maintainers, integration developers

---

## How to Use This Documentation

### For Implementation
1. **Start here**: Read this README
2. **Understand current state**: Read THREAD_VERBS_ANALYSIS.md and AGENTS_ANALYSIS.md
3. **Learn design**: Read THREADING_ARCHITECTURE.md
4. **Execute plan**: Follow IMPLEMENTATION_PLAN.md phase by phase
5. **Check dependencies**: Reference DEPENDENCIES.md as needed

### For Review
1. **Architecture review**: THREADING_ARCHITECTURE.md
2. **Implementation review**: IMPLEMENTATION_PLAN.md
3. **Risk assessment**: DEPENDENCIES.md (Risk Mitigation section)

### For Debugging
1. **Verb behavior**: THREAD_VERBS_ANALYSIS.md (find verb, see semantics)
2. **Agent issues**: AGENTS_ANALYSIS.md (lifecycle, scheduling)
3. **Lock problems**: THREADING_ARCHITECTURE.md (ODB thread safety)

---

## Critical Concepts

### Thread ID Semantics
**User-Visible** (UserTalk):
```usertalk
local(id = thread.getCurrentID());  // → 127 (opaque long value)
```

**Internal** (C implementation):
```c
typedef struct {
    pthread_t pthread_id;       // OS thread (e.g., 0x7fff12345678)
    long user_thread_id;        // UserTalk-visible (127)
    hdlthreadglobals hglobals;  // Thread-local state
} frontier_pthread_record;
```

**Mapping**:
- `user_thread_id` ↔ `pthread_id` via thread registry
- Registry protected by mutex
- Lookup: O(n) linear scan (acceptable for <1000 threads)

---

### Sleep/Wake Mechanism

**Current** (green threads):
```c
// Polling-based timeout check in scheduler
if ((**hp).sleepuntil < TickCount()) {
    wakeprocess(hp);
}
```

**Proposed** (POSIX):
```c
// Condition variable with absolute timeout
pthread_mutex_lock(&thread->state_mutex);
struct timespec timeout = { .tv_sec = wakeup_time, .tv_nsec = 0 };
pthread_cond_timedwait(&thread->wake_cond, &thread->state_mutex, &timeout);
pthread_mutex_unlock(&thread->state_mutex);
```

**Benefits**:
- No busy polling (CPU efficient)
- Immediate wake (not dependent on scheduler loop)
- Precise timeouts (nanosecond resolution)

---

### Agent Thread Model

**Current** (cooperative):
```
Main Thread:
  └─ processscheduler()
      └─ wakes agent thread
          Agent Thread:
            └─ agentscheduler() (time-slices all agents)
                ├─ Agent 1 (4 ticks)
                ├─ Agent 2 (4 ticks)
                └─ Agent 3 (4 ticks)
            └─ sleep (1 second)
```

**Proposed** (POSIX):
```
Main Thread
  (independent)

Agent Thread (pthread):
  while (enabled) {
    pthread_mutex_lock(&scheduler_mutex);
    agentscheduler();  // Time-slice agents
    pthread_mutex_unlock(&scheduler_mutex);

    pthread_cond_timedwait(&wake_cond, 1 second);
  }
```

**Differences**:
- Agent thread runs independently (no manual wake needed)
- Process list protected by mutex (thread-safe)
- Agents still time-sliced cooperatively (within agent thread)

---

## Phase Roadmap Visualization

```
Phase 1: Thread Registry & TLS
  ├─ Thread ID mapping
  ├─ pthread wrapper
  └─ Thread-local storage
      ↓
Phase 2: Sleep/Wake/Kill
  ├─ Condition variables
  ├─ Timeout handling
  └─ Thread termination
      ↓
Phase 3: Script Execution ← **CRITICAL FOR TCP**
  ├─ thread.evaluate()
  ├─ thread.callScript()
  └─ C → UserTalk callbacks
      ↓
Phase 4: Agent Thread
  ├─ pthread agent main
  ├─ Mutex-protected scheduler
  └─ Auto-restart behavior
      ↓
Phase 5: Fine-Grained Locks (OPTIONAL)
  ├─ Database RW locks
  ├─ Hash table locks
  └─ Lock order enforcement
```

---

## Key Metrics

### Threading Model Comparison

| Metric | Green Threads (Current) | POSIX Threads (Proposed) |
|--------|------------------------|--------------------------|
| **Parallelism** | None (cooperative) | True (preemptive) |
| **Thread Creation** | <1ms (fast) | 1-2ms (slower) |
| **Sleep/Wake Latency** | 1 second (scheduler loop) | <1ms (cond_signal) |
| **Max Threads** | ~100 (memory limited) | ~1000 (OS limited) |
| **CPU Utilization** | Single core | Multi-core |
| **ODB Concurrency** | Serial (no locks) | Concurrent reads (RW locks) |

---

## Testing Strategy

### Unit Tests (Phase-Specific)
- **Phase 1**: Thread registry operations, TLS isolation
- **Phase 2**: Sleep/wake correctness, timeout accuracy
- **Phase 3**: Script execution, callback invocation
- **Phase 4**: Agent lifecycle, scheduler mutex
- **Phase 5**: Lock ordering, deadlock detection

### Integration Tests
```yaml
# Example: Sleep/wake latency
- name: "thread.wake - low latency wakeup"
  script: |
    local(id = thread.evaluate("thread.sleepFor(60)"));
    local(start = clock.ticks());
    thread.wake(id);
    local(elapsed = clock.ticks() - start);
    return elapsed < 60;  // <1 second wakeup
  expected_success: true
```

### Stress Tests
- 1000 concurrent threads (verify no resource leaks)
- 10000 sleep/wake cycles (verify cond_wait stability)
- Concurrent ODB access (verify no data corruption)

---

## Success Criteria

### Phase 1-3 (TCP Integration)
- [ ] All thread verbs work with POSIX threads
- [ ] `execute_usertalk_callback_from_pthread()` functional
- [ ] Integration tests pass
- [ ] No memory leaks (valgrind clean)
- [ ] tcp.listenStream can execute callbacks

### Phase 4 (Agent Migration)
- [ ] Agent thread runs as pthread
- [ ] Agent auto-restart works
- [ ] Process list thread-safe
- [ ] No regressions in agent behavior

### Phase 5 (Optimization)
- [ ] Concurrent ODB reads work
- [ ] No deadlocks under load
- [ ] Performance acceptable (benchmarked)

---

## Open Questions

1. **Phase 5 Priority**: Implement now or defer post-launch?
   - **Recommendation**: Defer (global ODB lock acceptable for launch)

2. **Thread Pool**: Phase 3 or Phase 5?
   - **Recommendation**: Phase 5 (optimization, not critical)

3. **Agent Scheduler**: Cooperative or preemptive?
   - **Recommendation**: Cooperative (preserve existing behavior)

4. **Lock Granularity**: Global or fine-grained for launch?
   - **Recommendation**: Global (simpler, proven correct)

5. **Test Coverage**: Unit tests only or stress tests too?
   - **Recommendation**: Both (stress tests critical for threading)

---

## Timeline Estimate

| Milestone | Duration | Weeks |
|-----------|----------|-------|
| Phase 1: Thread Registry | 1-2 weeks | Week 1-2 |
| Phase 2: Sleep/Wake/Kill | 1-2 weeks | Week 3-4 |
| Phase 3: Script Execution | 2-3 weeks | Week 5-7 |
| **TCP Integration Ready** | **4-7 weeks** | **End Week 7** |
| Phase 4: Agent Thread | 2-3 weeks | Week 8-10 |
| **Full Threading Complete** | **6-10 weeks** | **End Week 10** |
| Phase 5: Optimization (Optional) | 3-4 weeks | Week 11-14 |

---

## Next Steps

1. **Review Planning Docs**: User reviews all documents
2. **Clarify Open Questions**: Discuss priorities (Phase 5 now or later?)
3. **Create Feature Branch**: `feature/threading-phase1`
4. **Start Phase 1**: Follow IMPLEMENTATION_PLAN.md
5. **TDD Workflow**: Write tests first, implement, refactor
6. **PR Review**: Monitor with background PR script

---

## Maintenance Notes

**Document Updates**:
- Update as implementation progresses
- Document deviations from plan (with rationale)
- Keep timelines realistic (adjust based on actual progress)

**ADR Integration**:
- Create ADRs for significant design decisions
- Reference ADRs from architecture docs
- Update ADRs when designs change

**Knowledge Capture**:
- Document gotchas discovered during implementation
- Add debugging tips for common issues
- Update with performance benchmarks

---

## References

**Frontier Codebase**:
- `Common/source/process.c` - Process and thread management
- `Common/headers/processinternal.h` - tythreadglobals structure
- `Common/source/shellsysverbs.c` - Thread verb implementations

**External**:
- POSIX.1-2001 Standard: https://pubs.opengroup.org/onlinepubs/009695399/
- pthread Programming: https://computing.llnl.gov/tutorials/pthreads/
- macOS Threading: https://developer.apple.com/library/archive/documentation/Cocoa/Conceptual/Multithreading/

---

**Document Version**: 1.0
**Last Updated**: 2026-01-16
**Status**: Planning Complete, Awaiting User Review
