# ADR-013: REPL Event Loop Architecture

## Status

Accepted

## Date

2026-01-30

## Context

The Frontier CLI REPL currently uses a blocking model where `linenoise()` waits for user input, and TCP callbacks (webserver responses) are only processed after each command completes. This creates a fundamental problem:

**Problem Statement**: When a webserver is running via `inetd.startOne()`, incoming HTTP requests cannot be processed while the REPL is waiting for user input. Users must call `thread.sleepFor()` to yield control to the webserver, which is unintuitive and prevents interactive use of the REPL while the webserver is operational.

### Current Architecture

```
while (running) {
    char *line = linenoise(REPL_PROMPT);  // BLOCKS HERE
    process_command(line);
    tcp_process_callbacks();  // Only runs after each command!
}
```

### Requirements

1. Webserver callbacks must process while user types at prompt
2. Agent scheduler must run periodically for background scripts
3. Ctrl-C must interrupt running scripts or clear input at prompt
4. Terminal state must be properly restored on crash/exit
5. No changes to ODB thread-safety (single-threaded operation)

## Decision

**Implement an event loop on the main thread using linenoise's non-blocking API with `poll()` for multiplexing stdin and timer-based events.**

### Architecture Overview

```
struct linenoiseState ls;
char buf[4096];
linenoiseEditStart(&ls, STDIN_FILENO, STDOUT_FILENO, buf, sizeof(buf), REPL_PROMPT);

while (running) {
    // 1. Poll stdin with 10ms timeout
    struct pollfd pfd = {STDIN_FILENO, POLLIN, 0};
    int ready = poll(&pfd, 1, 10);

    // 2. Feed input to linenoise if available
    if (ready > 0 && (pfd.revents & POLLIN)) {
        char *result = linenoiseEditFeed(&ls);
        if (result == linenoiseEditMore) continue;
        if (result != NULL) {
            process_command(result);
            linenoiseFree(result);
            linenoiseEditStop(&ls);
            linenoiseEditStart(&ls, ...);  // Restart for next line
        } else {
            running = false;  // EOF
        }
    }

    // 3. Process TCP callbacks (webserver)
    tcp_process_callbacks();

    // 4. Run agent scheduler tick
    if (agents_enabled()) {
        agentscheduler_tick();
    }

    // 5. Check for Ctrl-C flag
    if (g_interrupt_requested) {
        handle_interrupt(&ls);
    }
}
```

### Key Design Choices

| Choice | Rationale |
|--------|-----------|
| Event loop on main thread | Avoids ODB threading issues; single-threaded model is safe |
| 10ms poll timeout | Responsive to user input while allowing ~100Hz callback processing |
| linenoise non-blocking API | Mature upstream implementation; handles terminal modes correctly |
| Signal handler sets flag | Async-signal-safe; flag checked in main loop |
| `linenoiseHide/Show` for async output | Native linenoise support; avoids custom ANSI sequences |

## Alternatives Considered

### Alternative 1: Separate REPL Thread (Rejected)

**Approach**: Run REPL in a pthread while main thread handles callbacks.

**Pros**:
- Conceptually simple division of concerns
- Blocking linenoise works unmodified

**Cons**:
- Requires mutex protection for all ODB access
- Current ODB has global mutable state (roottable, currenthashtable, etc.)
- Script execution accesses ODB without locks
- Would require significant refactoring of database layer
- Race conditions between command execution and callbacks

**Decision**: Rejected due to ODB thread-safety requirements and scope of changes.

### Alternative 2: Threaded Callbacks Only (Rejected)

**Approach**: Keep blocking REPL, but process callbacks in background threads.

**Pros**:
- Minimal changes to REPL
- Webserver could respond during blocking input

**Cons**:
- Callbacks would need to queue results for main thread
- UserTalk callback scripts still need main thread for ODB access
- Complex synchronization for script execution
- Doesn't solve agent scheduling problem

**Decision**: Rejected because UserTalk callbacks must run on main thread.

### Alternative 3: Async I/O with kqueue/epoll (Considered)

**Approach**: Use platform-specific async I/O for full event-driven model.

**Pros**:
- More efficient for high-concurrency scenarios
- Native OS integration

**Cons**:
- Platform-specific code (kqueue on macOS, epoll on Linux)
- Linenoise doesn't expose its fd for external polling
- Over-engineered for current needs (~100 callbacks/sec max)

**Decision**: Deferred. `poll()` is POSIX-standard and sufficient. Can migrate to kqueue/epoll if needed for performance.

### Alternative 4: Coroutines/Fibers (Rejected)

**Approach**: Use C coroutine library to yield between REPL and callbacks.

**Pros**:
- Clean async model without explicit state machines

**Cons**:
- Requires third-party library or custom implementation
- Complex debugging
- Not idiomatic for C codebase
- Potential stack overflow issues

**Decision**: Rejected due to complexity and maintenance burden.

## Consequences

### Positive

1. **Webserver works interactively**: HTTP requests process immediately, even while user types
2. **Agents run continuously**: Background scripts execute without explicit yield calls
3. **Ctrl-C works intuitively**: Interrupts scripts or clears input at prompt
4. **No ODB changes**: Single-threaded model preserved; no race conditions
5. **Foundation for future**: Event loop can be extended with priority queues, timers, etc.

### Negative

1. **Slight latency**: Commands won't execute mid-callback; must wait for poll cycle
2. **CPU usage**: 10ms polling burns some CPU even when idle (minimal impact)
3. **Linenoise coupling**: Depends on linenoise's non-blocking API behavior
4. **Complexity**: Event loop state machine more complex than blocking read

### Neutral

1. **POSIX dependency**: `poll()` requires POSIX; not portable to Windows without changes
2. **Single-threaded limit**: Cannot parallelize CPU-bound scripts and callbacks

## Implementation Notes

### Files Modified

| File | Changes |
|------|---------|
| `frontier-cli/repl.c` | Convert to event loop with poll() |
| `frontier-cli/repl_output.c` | Add async output function |
| `frontier-cli/repl_output.h` | Export async output function |
| `frontier-cli/main.c` | Signal handler setup |
| `Common/source/process.c` | Add agentscheduler_tick() |
| `Common/headers/process.h` | Export agentscheduler_tick() |

### Signal Handling

```c
static volatile sig_atomic_t g_interrupt_requested = 0;

static void sigint_handler(int sig) {
    (void)sig;
    g_interrupt_requested = 1;
}
```

The flag is only set in the handler (async-signal-safe) and checked in the main loop.

### Terminal Safety

```c
static struct linenoiseState *g_active_linenoisestate = NULL;

static void cleanup_terminal(void) {
    if (g_active_linenoisestate) {
        linenoiseEditStop(g_active_linenoisestate);
        g_active_linenoisestate = NULL;
    }
}

// Register cleanup handlers
atexit(cleanup_terminal);
signal(SIGTERM, cleanup_on_signal);
```

## Future Work

This ADR covers Phase 1. Future phases may include:

| Phase | Description |
|-------|-------------|
| Phase 2 | Generalize dispatch queue to `dispatch.c/h` |
| Phase 3 | Priority queues, QoS classes |
| Phase 4 | Async script execution with promises |
| Phase 5 | Multi-database threading with ODB locking |

## References

- [Linenoise Non-Blocking API](https://github.com/antirez/linenoise#multiplexing)
- [POSIX poll()](https://pubs.opengroup.org/onlinepubs/9699919799/functions/poll.html)
- ADR-012: Main Thread Dispatch Queue (related background)
