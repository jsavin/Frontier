# ADR-012: Main Thread Dispatch Queue for Thread-Safe Callback Execution

**Status**: `[PROPOSED]` - TCP-specific prototype implemented (PR #363), generalization needed

## Context

Frontier's ODB (Object Database) and UserTalk runtime were designed for cooperative multitasking, not preemptive threading. When we introduced pthreads for TCP accept handling (PR #363), we discovered that invoking UserTalk callbacks directly from background threads causes crashes:

```
[TRACE] opverbinmemory ENTER: path='<unknown>' adr=0x59ac24...
[general-ERROR] memory.c:1480: loadfromhandle fail: ix=0 ct=24 size=12
Segmentation fault: 11
```

The crash occurs because:
1. Background pthread accepts TCP connection
2. Thread attempts to invoke `inetd.supervisor` callback
3. Callback triggers `opverbinmemory` to load script from database
4. ODB operations (handle manipulation, hash table access) aren't thread-safe
5. Concurrent access corrupts memory → crash

This is a fundamental architectural constraint: **UserTalk code can only safely execute on the main thread**.

## Decision

Implement a **Main Thread Dispatch Queue** pattern, inspired by Apple's Grand Central Dispatch (GCD):

### Core Concept

Background threads don't execute UserTalk directly. Instead, they enqueue work items that the main thread processes during its event loop.

```
┌─────────────────┐     ┌──────────────────┐     ┌─────────────────┐
│ Background      │     │  Dispatch Queue  │     │   Main Thread   │
│ Thread          │────▶│  (thread-safe)   │────▶│   (processes    │
│ (TCP accept)    │     │                  │     │    callbacks)   │
└─────────────────┘     └──────────────────┘     └─────────────────┘
```

### API Design (Proposed)

```c
/* Callback work item - holds everything needed to invoke a UserTalk script */
typedef struct dispatch_item {
    hdlhashtable    callback_table;   /* Hash table containing callback */
    bigstring       callback_name;    /* Script name to invoke */
    tyvaluerecord   params[4];        /* Up to 4 parameters */
    short           param_count;      /* Number of parameters */
    void            (*completion)(boolean success, tyvaluerecord *result, void *refcon);
    void            *refcon;          /* User data for completion callback */
} dispatch_item_t;

/* Enqueue a callback for main thread execution (thread-safe) */
boolean dispatch_async_callback(
    hdlhashtable htable,
    bigstring callback_name,
    short param_count,
    tyvaluerecord *params,
    void (*completion)(boolean, tyvaluerecord*, void*),
    void *refcon
);

/* Process pending callbacks - called from main thread event loop */
int dispatch_process_queue(void);

/* Check if queue has pending items */
boolean dispatch_has_pending(void);
```

### Integration Points

1. **REPL loop** - Call `dispatch_process_queue()` after each command
2. **`processsleep()`** - Poll queue during sleep intervals (already implemented for TCP)
3. **Future event loop** - When we have a proper event loop, integrate there

### Current Implementation (TCP-Specific)

PR #363 implements a TCP-specific version in `tcpverbs.c`:
- `tcp_enqueue_callback()` - Enqueues TCP accept callbacks
- `tcp_process_callbacks()` - Drains TCP callback queue
- `processsleep()` modified to poll during sleep

This works but should be generalized.

## Consequences

### Positive

- ✅ **Thread safety without locks on ODB** - Background threads never touch UserTalk/ODB directly
- ✅ **Clean architectural boundary** - Clear separation between "async work" and "UserTalk execution"
- ✅ **Familiar pattern** - GCD-like design is well-understood by macOS/iOS developers
- ✅ **Foundation for future async features** - File watchers, timers, IPC handlers can use same pattern
- ✅ **No changes to existing UserTalk scripts** - Transparent to script authors

### Negative

- ⚠️ **Latency** - Callbacks execute on next main thread poll, not immediately
- ⚠️ **Queue overflow** - Need to handle case where queue fills faster than main thread drains
- ⚠️ **Completion callbacks** - Async completion pattern more complex than synchronous calls

### Neutral

- 🔄 **Requires event loop awareness** - Main thread must actively poll queue

## Alternatives Considered

### 1. Make ODB Thread-Safe with Locks

**Rejected** because:
- Massive refactoring effort (hundreds of call sites)
- Risk of deadlocks with complex lock hierarchies
- Performance overhead on every ODB operation
- Doesn't match original Frontier architecture

### 2. Use Frontier's Process Scheduler

The existing `addnewprocess()`/`scheduleprocess()` creates cooperative "threads" that the main loop schedules. We could create callback processes.

**Deferred** because:
- Process scheduler designed for long-running scripts, not quick callbacks
- Heavy-weight for simple callback invocation
- May revisit for complex async workflows

### 3. Signal-Based Dispatch

Use Unix signals or pthread condition variables to wake main thread.

**Deferred** because:
- More complex implementation
- REPL's blocking `linenoise()` call complicates signal handling
- May revisit when we have a proper event loop

## Implementation Phases

### Phase 1: Current (PR #363)
- TCP-specific callback queue in `tcpverbs.c`
- `processsleep()` polls queue
- Sufficient for webserver Hello World milestone

### Phase 2: Generalization
- Move queue to `dispatch.c` / `dispatch.h`
- Generic API usable by any subsystem
- Refactor TCP to use generic API

### Phase 3: Event Loop Integration
- Proper main thread event loop (replacing blocking REPL)
- Non-blocking input handling
- Immediate callback dispatch (no polling delay)

### Phase 4: Priority Queues (Future)
- Multiple priority levels (high/default/low)
- QoS classes like GCD
- Callback coalescing for high-frequency events

## Relationship to Other ADRs

- **ADR-005 (Parameter State Thread Safety)** - Addresses `flnextparamislast` thread-local state
- **ADR-010 (Deterministic Thread Testing)** - Testing strategies for async behavior

## References

- [Grand Central Dispatch (GCD) Documentation](https://developer.apple.com/documentation/dispatch)
- [libdispatch source code](https://github.com/apple/swift-corelibs-libdispatch)
- PR #363: Webserver Hello World implementation

## Decision Date

2026-01-30

## Authors

- Jake Savin
- Claude (AI Assistant)
