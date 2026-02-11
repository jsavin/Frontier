# ADR-014: GIL-Based Cooperative Threading for Headless Mode

## Status

Accepted

## Date

2026-02-10

## Context

Frontier's cooperative threading model requires multiple "threads" (UserTalk-level) to share a single interpreter. Legacy Frontier used Mac Thread Manager's `YieldToThread()` for cooperative context switching. The headless port initially implemented `thread.evaluate()` and `thread.callscript()` as **synchronous** — the spawned thread runs to completion before returning to the caller (PR #404).

This causes a startup hang: the startup script spawns cooperative threads that poll `Frontier.startingUp` via `thread.sleepTicks(6)` in a loop, then sets the flag to false at line 237. But `thread.evaluate()` blocks until the spawned thread finishes, so the main script never reaches line 237.

Additionally:
- `langbackgroundtask()` (the interpreter's main yield point) is set to `cb_true_bool` — a no-op
- `headless_thread_sleep()` uses `pthread_cond_timedwait()` which blocks the OS thread, preventing other cooperative threads from executing

### Approaches Considered

**A. Keep single-OS-thread, add coroutines via `setjmp`/`longjmp` or `ucontext`**
- `ucontext` is deprecated on macOS (removed from some SDK headers)
- `setjmp`/`longjmp` cannot safely switch stacks
- Paige library already uses `setjmp` for exception handling — potential conflicts
- Complex stack management, fragile

**B. Real POSIX threads without synchronization**
- Would require making all C globals thread-safe (Phase 4 P0a work)
- Premature — hundreds of globals would need migration to thread-local storage
- Massive scope, not justified for cooperative semantics

**C. Real POSIX threads with Global Interpreter Lock (GIL)**
- Spawn real OS threads, but only one holds the GIL at a time
- C globals are safe because only the GIL holder reads/writes them
- `langbackgroundtask()` releases the GIL, allowing other threads to run
- `thread.sleepTicks()` releases the GIL before blocking, so other threads proceed
- Thread registry already has per-thread mutexes and condvars
- Minimal code changes, localized to `headless_thread_verbs.c`
- Forward-compatible: when Phase 4 eliminates C globals, the GIL can be removed

## Decision

Use **Approach C: Real POSIX threads with a Global Interpreter Lock (GIL)**.

### Architecture

```
Main Thread                          Spawned Thread
===========                          ==============
Holds GIL                            pthread_create() → blocks on GIL
langruncode() running
  thread.evaluate("code")
    pthread_create(spawned)
    returns thread_id immediately
  ...continues script...
  langbackgroundtask()
    save globals
    unlock GIL  ──────────────────→  acquire GIL
    sched_yield()                    restore globals
    lock GIL    ←──────────────────  langruncode() running
    restore globals                    thread.sleepTicks(6)
  ...continues...                        save globals
                                         unlock GIL  ──→ (main reacquires)
                                         pthread_cond_timedwait (own OS stack)
                                         lock GIL    ←── (after timeout)
                                         restore globals
```

### Key Design Elements

1. **GIL**: A single `pthread_mutex_t` protects all interpreter C globals. The main thread acquires it at startup; spawned threads acquire it when they begin execution.

2. **`headless_backgroundtask()`**: Replaces `cb_true_bool` as the `langcallbacks.backgroundtaskcallback`. Saves current thread globals, releases the GIL, yields CPU via `sched_yield()`, re-acquires the GIL, and restores globals. Each thread captures its own `hthreadglobals` handle in a local variable before releasing the GIL (since the global `hthreadglobals` pointer is swapped by whichever thread holds the GIL).

3. **Non-blocking `thread.evaluate()`**: Compiles code, allocates thread record and globals, spawns a detached POSIX thread, returns the thread ID immediately. The spawned thread blocks on GIL acquisition until the calling thread yields.

4. **GIL-aware `thread.sleepTicks()`**: Saves globals, releases the GIL, blocks on `pthread_cond_timedwait()` (only its own OS thread), re-acquires the GIL, restores globals. Other threads can run during the sleep.

5. **Thread globals save/restore**: The existing `headless_save_threadglobals()` / `headless_restore_threadglobals()` functions handle all C globals (`fllangerror`, `flreturn`, `flbreak`, `hashtablestack`, `currenthashtable`, etc.). These are called around every GIL release/reacquire to ensure each thread sees its own state. Note: `currenthashtable` is a C global that tracks the active hash table for the current scope — it must be saved/restored alongside `hashtablestack` because `langevaluate` asserts `hlocals == currenthashtable` at function entry.

6. **`langcallbacks` inheritance**: `headless_new_threadglobals()` copies `langcallbacks` from the current thread, so spawned threads automatically inherit `headless_backgroundtask` as their yield callback.

### Yield Points

`langbackgroundtask()` is called from:
- `langevaluate.c:1990` — after each `evaluatelist()` (at function-call boundaries)
- `langverbs.c:2276` — in `semaphore.lock()` retry loop (with `flresting=true`)
- `langexternal.c:3131` — after external value evaluation
- `ops.c:124` — during outline operations
- Various networking and file I/O code

These existing call sites provide natural yield points without any changes to the interpreter core.

## Consequences

### Positive

- **Startup script works**: Spawned threads yield at `langbackgroundtask()` calls, allowing the main script to progress and set `Frontier.startingUp = false`
- **Minimal changes**: ~200 lines changed in `headless_thread_verbs.c`, plus minor wiring in `langstartup.c` and `main.c`
- **No interpreter changes**: All yield points already exist in the codebase
- **Forward-compatible**: GIL removal is straightforward once Phase 4 P0a eliminates C globals
- **`thread.kill()` works on running threads**: Target thread checks `flthreadkilled` at every `langbackgroundtask()` call

### Negative

- **No true parallelism**: Only one thread executes at a time (GIL serialization). This matches legacy Frontier semantics but limits throughput.
- **Yield frequency depends on call sites**: Long-running computations without `langbackgroundtask()` calls will starve other threads. This also matches legacy behavior.
- **Shutdown complexity**: Detached threads must complete before `cleanup_thread_registry()`. Current startup script usage is bounded; future work may need explicit join semantics.

### Risks

- **hashtablestack sharing**: The spawned thread gets a deep copy of the `tytablestack` structure (its own push/pop state), but the underlying hash table pointers are shared with the parent thread. Safe because only one thread accesses at a time (GIL), but would be a data race if the GIL is removed. See `TODO(Phase4)` in `headless_thread_evaluate()`.
- **callscript code tree ownership**: The compiled code tree for `thread.callscript()` belongs to the hash table, not the spawned thread. The thread entry point must not dispose it. The `hcode` and `htable` pointers stored in `thread_launch_params` are safe under GIL (caller cannot mutate ODB while spawned thread holds GIL), but would need retention or re-resolution if the GIL is removed.
- **currenthashtable**: Added to `tythreadglobals` struct (`hcurrenthashtable` field) and to the save/restore path. This C global must be context-switched alongside `hashtablestack` to satisfy `langevaluate`'s assertion.

## Debugger Compatibility

The UserTalk debugger is a critically important function. This section documents why the GIL model preserves full debugger compatibility and in some ways improves on the legacy model.

### How the Debugger Works

The debugger operates via `langcallbacks.debuggercallback`, called from `langdebuggercall()` at `langevaluate.c:1936` — **before each statement** in `evaluatelist`. This callback enables single-stepping, breakpoints, variable inspection, and expression evaluation in the debugger context.

### Why the GIL Model Is Fully Compatible

1. **Debugger runs under GIL protection**: The debugger callback fires while the thread holds the GIL, giving it exclusive access to all C globals, the hash table stack, code tree, and variable state. This is identical to the context it had in legacy Frontier.

2. **Single-stepping works unchanged**: `evaluatelist` calls `langdebuggercall(programcounter)` per-statement, and the thread holds the GIL throughout. The debugger can inspect and modify variables, evaluate expressions, and step through code exactly as in legacy Frontier.

3. **Breakpoints in spawned threads work**: Each spawned thread's `evaluatelist` calls `langdebuggercall()` while it holds the GIL. Breakpoints fire normally regardless of which thread hits them.

4. **Pausing at breakpoints**: When the debugger pauses (e.g., waiting for user input at a breakpoint), it can release the GIL while waiting, allowing other cooperative threads to continue executing. This follows the same pattern as `headless_backgroundtask()` — save globals, release GIL, wait, reacquire GIL, restore globals. Legacy Frontier did the same via `processyield()` while paused in the debugger.

5. **Implicit thread pausing during debug**: When debugging one thread, all other threads are naturally paused (they cannot acquire the GIL). This is the **same behavior** as legacy Frontier's cooperative model — debugging one thread implicitly freezes all others.

### Inspecting Non-Running Threads

A future GUI debugger may want to inspect a thread's variables while that thread is not the GIL holder (e.g., examining a sleeping thread's local variables). This is supported because:

- Each thread's state is saved via `headless_save_threadglobals()` before the GIL is released
- The thread registry record (`rec->hglobals`) points to the saved state
- A debugger can read the saved `htablestack`, `fllangerror`, local variables, etc. from the saved globals without needing the thread to be running

### Advantages Over Alternative Approaches

Real POSIX threads are actually **better** for debugging than `setjmp`/`longjmp` coroutines or `ucontext` approaches:

- Each thread has a **real OS stack** that LLDB/GDB can inspect independently
- Thread-aware debugger commands (`thread list`, `thread select N`) work natively
- Stack traces for each thread are complete and accurate
- No fragile stack-switching tricks that confuse debugger unwinders
- Core dumps contain full thread state for post-mortem analysis

### Design Constraint for Implementors

When implementing the debugger's "pause at breakpoint" behavior, the callback **must** follow the GIL release/reacquire pattern:

```c
/* In debugger breakpoint handler: */
hdlthreadglobals my_globals = hthreadglobals;
headless_save_threadglobals(my_globals);
pthread_mutex_unlock(&frontier_gil);       /* Let other threads run */
/* ... wait for user input (step/continue/etc.) ... */
pthread_mutex_lock(&frontier_gil);         /* Re-acquire before resuming */
headless_restore_threadglobals(my_globals);
```

Failure to release the GIL during a breakpoint pause would freeze all cooperative threads for the duration of the pause — functional but undesirable for multi-threaded debugging scenarios.

## References

- PR #404: Cooperative threading with thread registry (synchronous model)
- ADR-005: Parameter State Thread-Safety Architecture
- ADR-010: Deterministic Thread Testing Strategy
- ADR-012: Main Thread Dispatch Queue
- Issue #406: Increase MAX_THREADS beyond 64
- Phase 4 P0a: Global State Elimination (queued)
- Legacy Frontier: `process.c` (processbackgroundtask, processyield, processsleep, processchecktimeouts)
