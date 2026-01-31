# REPL Thread Separation - Event Loop Implementation Plan

## Overview

Convert the Frontier CLI REPL from a blocking model to a non-blocking event loop, enabling concurrent operation of the REPL, webserver callbacks, and background agents without requiring ODB thread-safety changes.

**Related ADR**: [ADR-013: REPL Event Loop Architecture](../architectural_decision_records/ADR-013-repl-event-loop.md)

## User Decisions

| Decision | Choice |
|----------|--------|
| Architecture | Event loop on main thread (not separate pthread) |
| Phase 1 Scope | TCP callbacks + agent scheduler + Ctrl-C handling |
| Sequencing | Event loop first, then generalize dispatch |
| Documentation | Both ADR + detailed implementation plan |
| Poll Interval | 10ms (responsive) |
| Async Output | Use linenoiseHide/Show for clean output |
| Ctrl-C at Prompt | Clear line, show fresh prompt |

## Phase 1: Non-Blocking Event Loop

### 1.1 Convert REPL to Event Loop

**File**: `frontier-cli/repl.c`

Replace blocking `linenoise()` with non-blocking API:

```c
// Current (blocking):
while (running) {
    char *line = linenoise(REPL_PROMPT);  // BLOCKS HERE
    // process line
    tcp_process_callbacks();  // Only runs after each command!
}

// New (event loop):
struct linenoiseState ls;
char buf[4096];
linenoiseEditStart(&ls, STDIN_FILENO, STDOUT_FILENO, buf, sizeof(buf), REPL_PROMPT);

while (running) {
    // 1. Poll stdin with 10ms timeout
    struct pollfd pfd = {STDIN_FILENO, POLLIN, 0};
    int ready = poll(&pfd, 1, 10);  // 10ms = responsive

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
    if (agentsenabled()) {
        agentscheduler_tick();
    }

    // 5. Check for Ctrl-C flag
    if (g_interrupt_requested) {
        handle_interrupt(&ls);
    }
}
```

### 1.2 Async Output Handling

**File**: `frontier-cli/repl_output.c`

When callbacks produce output while user is typing, use linenoise's built-in hide/show functions:

```c
// Global state pointer (set during event loop)
static struct linenoiseState *g_active_linenoisestate = NULL;

void repl_set_active_linenoisestate(struct linenoiseState *ls) {
    g_active_linenoisestate = ls;
}

void repl_async_output(const char *message) {
    if (g_active_linenoisestate != NULL) {
        // Hide current line, print message, restore line
        linenoiseHide(g_active_linenoisestate);
        printf("%s\n", message);
        fflush(stdout);
        linenoiseShow(g_active_linenoisestate);
    } else {
        // Not in event loop mode, just print
        printf("%s\n", message);
        fflush(stdout);
    }
}
```

This uses linenoise's native hide/show API which properly handles:
- Saving cursor position
- Clearing current line
- Restoring prompt and partial input
- Repositioning cursor

### 1.3 Ctrl-C / Signal Handling

**File**: `frontier-cli/repl.c`

```c
#include <signal.h>

// Global interrupt flag (signal-safe)
static volatile sig_atomic_t g_interrupt_requested = 0;

// Signal handler - must be async-signal-safe
static void sigint_handler(int sig) {
    (void)sig;
    g_interrupt_requested = 1;
}

// Install signal handler (call from repl_main)
static void install_signal_handlers(void) {
    struct sigaction sa;
    sa.sa_handler = sigint_handler;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
}

// Handle interrupt in event loop
static void handle_interrupt(struct linenoiseState *ls) {
    g_interrupt_requested = 0;

    if (script_is_running()) {
        // Set flag to interrupt script at next check point
        set_script_interrupt_flag();
        printf("\n^C (interrupting script...)\n");
    } else {
        // At prompt - clear line and redisplay
        linenoiseEditStop(ls);
        printf("^C\n");
        fflush(stdout);
        // Restart with fresh prompt
        linenoiseEditStart(ls, STDIN_FILENO, STDOUT_FILENO,
                          ls->buf, ls->buflen, REPL_PROMPT);
    }
}
```

**File**: `Common/source/lang.c` or evaluation loop

Add interrupt check in interpreter:

```c
// Check for interrupt flag periodically during script execution
boolean langcheckinterrupt(void) {
    extern volatile sig_atomic_t g_interrupt_requested;
    if (g_interrupt_requested) {
        g_interrupt_requested = 0;
        langseterroraliased(BIGSTRING("\x1a" "Script interrupted by user"));
        return true;  // Interrupted
    }
    return false;  // Not interrupted
}
```

### 1.4 Agent Scheduler Integration

**File**: `Common/source/process.c`

Add non-blocking scheduler tick:

```c
/*
 * agentscheduler_tick - Run one iteration of agent scheduling
 *
 * This function runs each ready agent for one time slice without blocking.
 * Call from event loop at regular intervals (e.g., every 10ms).
 *
 * Unlike the full agentscheduler() which runs in its own thread and loops
 * continuously, this function:
 * - Runs once and returns immediately
 * - Does not create or manage threads
 * - Processes only agents that are ready (not sleeping)
 */
void agentscheduler_tick(void) {
    hdlprocessrecord hp;
    hdlprocessrecord hnext;
    unsigned long now;

    if (!flagentsenabled)
        return;

    if (flagentsdisabled)
        return;

    if (processlist == nil)
        return;

    now = timenow64();

    // Run each ready agent for one time slice
    for (hp = (**processlist).hfirstprocess; hp != nil; hp = hnext) {

        hnext = (**hp).hnextprocess;  // Save next before potential deletion

        // Skip one-shot processes (handled separately)
        if ((**hp).floneshot)
            continue;

        // Skip sleeping agents
        if ((**hp).sleepuntil > now)
            continue;

        // Skip agents already running
        if ((**hp).flrunning)
            continue;

        // Run this agent for one time slice
        (**processlist).ctrunning++;

        if (!processtimeslice(hp)) {
            // Agent finished or errored
            if (!(**hthreadglobals).flretryagent)
                deleteprocess(hp);
            else
                (**hthreadglobals).flretryagent = false;
        }

        (**processlist).ctrunning--;

        // Only run one agent per tick to keep event loop responsive
        break;
    }
}
```

**File**: `Common/headers/process.h`

Export the new function:

```c
/* Non-blocking agent scheduler tick for event loop integration.
 * Runs one iteration of ready agents, then returns immediately. */
extern void agentscheduler_tick(void);
```

### 1.5 Terminal State Safety

**File**: `frontier-cli/repl.c`

Ensure terminal is restored on crash/exit:

```c
#include <signal.h>
#include <stdlib.h>

static struct linenoiseState *g_active_linenoisestate = NULL;

// Cleanup function for atexit() and signal handlers
static void cleanup_terminal(void) {
    if (g_active_linenoisestate) {
        linenoiseEditStop(g_active_linenoisestate);
        g_active_linenoisestate = NULL;
    }
}

// Signal handler for SIGTERM
static void sigterm_handler(int sig) {
    (void)sig;
    cleanup_terminal();
    _exit(0);  // Use _exit to avoid atexit handlers running twice
}

// In repl_main():
static void setup_terminal_cleanup(void) {
    atexit(cleanup_terminal);

    struct sigaction sa;
    sa.sa_handler = sigterm_handler;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGTERM, &sa, NULL);
}
```

## Files to Modify

| File | Changes |
|------|---------|
| `frontier-cli/repl.c` | Convert to event loop, add signal handling |
| `frontier-cli/repl_output.c` | Add async output function |
| `frontier-cli/repl_output.h` | Export async output function and state setter |
| `Common/source/process.c` | Add `agentscheduler_tick()` |
| `Common/headers/process.h` | Export `agentscheduler_tick()` |

## Dependencies

### Headers to Include in repl.c

```c
#include <poll.h>       // poll()
#include <signal.h>     // sigaction, sig_atomic_t
#include <unistd.h>     // STDIN_FILENO, STDOUT_FILENO
```

### Forward Declarations

The `processtimeslice()` function in process.c is static. For `agentscheduler_tick()` to call it, either:
1. Make `processtimeslice()` non-static (preferred)
2. Add the tick logic inline in `agentscheduler_tick()`

## Verification

### Manual Testing

1. **Webserver concurrent with REPL**:
   ```
   > inetd.startOne(8080, 5, @user.inetd.config.http.callback, 0, 0)
   > [type partial command, don't press enter]
   # In another terminal: curl http://localhost:8080/helloworld
   # Should get HTTP response while prompt is visible
   ```

2. **Agents run while typing**:
   ```
   > thread.evaluate("while true { thread.sleepFor(1); msg('tick') }")
   > [type at prompt]
   # Should see "tick" messages appearing above prompt
   ```

3. **Ctrl-C handling**:
   ```
   > loop { }  [start infinite loop]
   ^C
   > [should return to prompt]

   > local x = [partial input]
   ^C
   > [should clear and show fresh prompt]
   ```

4. **Clean shutdown**:
   ```
   > inetd.startOne(...)
   > /exit
   # Listeners should be stopped, terminal restored
   ```

### Integration Tests

Add to `tests/integration/test_cases/`:

```yaml
# repl_event_loop.yaml
name: "REPL event loop - callbacks process while waiting"
tests:
  - name: "webserver responds during REPL idle"
    description: "Verify TCP callbacks process when REPL is idle"
    # Note: This test requires manual verification or
    # a test harness that can interact with the REPL
    skip: true  # Manual test
```

## Risks and Mitigations

| Risk | Mitigation |
|------|------------|
| Terminal corruption on crash | `atexit()` handler, signal handlers |
| Output interleaving race | Single-threaded - no race possible |
| `select()`/`poll()` portability | Both are POSIX, test on macOS + Linux |
| Linenoise non-blocking bugs | Upstream has mature async API, test edge cases |
| Agent scheduler reentrancy | Already designed for single-thread; add guards if needed |

## Future Phases (Out of Scope)

| Phase | Description |
|-------|-------------|
| Phase 2 | Generalize dispatch queue to `dispatch.c/h` |
| Phase 3 | Priority queues, QoS classes |
| Phase 4 | Async script execution with promises |
| Phase 5 | Multi-database threading with ODB locking |

## Performance Considerations

### CPU Usage

The 10ms poll timeout means the event loop runs at ~100Hz when idle. This consumes minimal CPU (< 1% on modern hardware) but is measurable. If battery life becomes a concern, consider:

1. Adaptive timeout: Longer timeout when no listeners active
2. Self-pipe trick: Wake event loop only on activity
3. Platform-specific idle (kqueue kevent timeout)

### Latency

User input has worst-case 10ms latency before processing. This is imperceptible for typing but could affect paste operations with very rapid input. The linenoise buffer handles this gracefully.

### Memory

No significant memory impact. The `linenoiseState` struct is ~200 bytes on stack.
