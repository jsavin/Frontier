# Frontier Agents Analysis

**Date**: 2026-01-16
**Purpose**: Understanding Frontier agents and their relationship to threading infrastructure

---

## Executive Summary

Frontier **agents** are background UserTalk scripts that run cooperatively in a dedicated **agent thread**. They are NOT separate threads - all agents share a single thread that time-slices between them.

**Key Insight**: Agents are a **process scheduling abstraction** built on top of the threading system, not a threading primitive themselves.

---

## What Are Frontier Agents?

### Definition
- **Agent**: A UserTalk script registered for background execution
- **Agent Thread**: Single dedicated thread running `agentthreadmain()` → `agentscheduler()`
- **Process**: Internal representation (`hdlprocessrecord`) of a script to be executed
- **One-Shot Process**: Script that runs once and terminates (e.g., `thread.evaluate()`)

### Agent vs Thread Distinction

| Concept | Description | Count |
|---------|-------------|-------|
| **Thread** | OS execution context (or green thread) | Multiple (1 agent thread + N one-shot threads) |
| **Agent** | Background UserTalk script | Multiple (all in agent thread) |
| **Process** | Scheduled script execution unit | Multiple (1 per agent + 1 per one-shot) |

**Example**:
```
Thread 1 (Agent Thread):
  ├── Agent Process: system.agents.MinutesSinceShip
  ├── Agent Process: system.agents.SchedulerMonitor
  └── Agent Process: system.agents.StatusMessage

Thread 2 (One-Shot):
  └── Process: thread.evaluate("msg('hello')")

Thread 3 (One-Shot):
  └── Process: thread.callScript(...)
```

---

## Default Agents in Frontier

Located in `usertalk_scripts/Frontier.root/system/agents/`:

### 1. MinutesSinceShip.ut
```usertalk
betty.rpc.minutesSinceShip()
```
**Purpose**: Unclear - appears to track time since application launch (legacy Betty RPC?)

### 2. asynchRPC.ut
```usertalk
betty.rpc.agent()
```
**Purpose**: Asynchronous RPC request handler (legacy Betty integration)

### 3. FrontierPath.ut
**Purpose**: Manages application path tracking

### 4. SchedulerMonitor.ut
**Purpose**: Monitors and manages scheduled task execution

### 5. MenuMonitor.ut
**Purpose**: Updates menus based on window focus and application state

### 6. StatusMessage.ut
**Purpose**: Updates status message in About Frontier window

---

## Agent Lifecycle

### Agent Thread Creation
**Trigger**: First call to `processscheduler()` with agents enabled

**Flow** (`process.c:3114-3225`):
1. Check if agent thread already exists
2. Verify agents are enabled (`flagentsenabled`)
3. Verify startup scripts are complete (`flstartingup`)
4. Create thread: `newprocessthread(&agentthreadmain, 0, &agentthread)`
5. Store process list reference: `agentprocesslist = processlist`

### Agent Thread Main Loop
**Function**: `agentthreadmain()` in `process.c:3057-3111`

```c
void *agentthreadmain(void *params) {
    initprocessthread("agents");  // Initialize thread-local state

    while (flagentsenabled && ingoodthread() && !flagentsdisabled && !flshellclosingall) {
        setprocesstimeslice(processagenttimeslice);  // Default: 4 ticks

        agentscheduler();  // Run all agent processes

        if (!ingoodthread() || flagentsdisabled)
            break;

        agentthreadsleeping = true;
        threadsleep(nil);  // Sleep until woken by scheduler
        agentthreadsleeping = false;
    }

    exitprocessthread();
    return nil;
}
```

### Agent Scheduler
**Function**: `agentscheduler()` in `process.c` (not shown in excerpts, but called from main loop)

**Behavior**:
1. Iterates through `processlist` (global process queue)
2. Skips one-shot processes (`floneshot == true`)
3. Executes each agent process for its time slice
4. Yields control when time slice expires
5. Wakes sleeping agents when timeout expires

### Agent Process Execution
**Process Time Slice** (`processtimeslice()`):
1. Push process onto stack: `pushprocess(hp)`
2. Execute script: `processruncode(hp, &val)`
3. Check for completion or yield
4. Pop process: `popprocess()`

---

## Agent Enable/Disable Mechanism

### Frontier.enableAgents(bool)
**Kernel Verb**: `enableagentsfunc` in `shellsysverbs.c`

**Behavior**:
- **Enable (`true`)**: Sets `flagentsenabled = true`, wakes agent thread (if sleeping)
- **Disable (`false`)**: Sets `flagentsenabled = false`, agent thread exits on next wake

**Global State**:
- `flagentsenabled` (boolean): User setting (persistent)
- `flagentsdisabled` (int): Temporary internal disable counter (stack-based)

**Agent Thread Auto-Restart**:
- If agent thread is killed, `processscheduler()` recreates it after 1 second
- Only way to permanently stop: `Frontier.enableAgents(false)`

---

## Agent vs One-Shot Processes

### One-Shot Process
- Created by: `thread.evaluate()`, `thread.callScript()`, `thread.evaluateTo()`
- Lifetime: Single execution, then disposed
- Thread: Dedicated pthread (one thread per one-shot)
- Scheduling: Immediate execution (not time-sliced with agents)

### Agent Process
- Created by: Registered in `system.agents.*`
- Lifetime: Persistent (runs in loop, never terminates)
- Thread: Shared agent thread (all agents in one thread)
- Scheduling: Cooperative time-slicing (default 4 ticks per agent)

### Process List Structure
**Insertion Order** (`addprocess()` in `process.c:2771-2834`):
1. One-shot processes inserted at front
2. Agent processes appended at back
3. Guarantees: One-shots scheduled before agents

**Example Process List**:
```
HEAD → [OneShot1] → [OneShot2] → [Agent1] → [Agent2] → [Agent3] → NULL
       ^                          ^
       floneshot=true             floneshot=false
```

---

## Agent Thread Sleep/Wake Behavior

### Sleep Triggers
1. **Scheduler Loop Complete**: `agentthreadsleeping = true` → `threadsleep(nil)`
2. **Script Calls `thread.sleep()`**: Process-level sleep (agent thread still runs other agents)
3. **Apple Event Wait**: Sends AE, waits for reply

### Wake Triggers
1. **Timeout Expiry**: `processscheduler()` wakes agent thread every 1 second
2. **Explicit Wake**: `thread.wake(agent_thread_id)`
3. **Agent Re-Enable**: `Frontier.enableAgents(true)` wakes sleeping agent thread

### Critical Flag: `agentthreadsleeping`
**Purpose**: Distinguishes "finished work, sleeping" vs "script-initiated sleep"

**Usage** (`processscheduler()` in `process.c:3114`):
- Only wake agent thread if `agentthreadsleeping == true`
- Prevents waking agent thread during Apple Event send (would break blocking semantics)

---

## Threading Model Implications

### Current Model (Green Threads)
- **Agent thread**: Cooperative multitasking, single execution context
- **One-shot threads**: Pseudo-threads (sequential execution, context switching via globals)
- **Parallelism**: None - all execution serialized

### POSIX Migration Model
- **Agent thread**: Real pthread running time-sliced scheduler
- **One-shot threads**: Real pthreads executing UserTalk in parallel
- **Parallelism**: True - multiple scripts can execute simultaneously

### Migration Challenges
1. **Agent Scheduler Mutex**: Protect process list during iteration
2. **Time Slice Enforcement**: Use `pthread_yield()` or preemptive scheduling
3. **Sleep/Wake Implementation**: Replace `threadsleep()` with `pthread_cond_wait()`
4. **Agent Thread Identity**: Maintain stable thread ID for `thread.getNthID(1)`
5. **Process List Insertion**: Ensure thread-safe `addprocess()`/`deleteprocess()`

---

## Agent Registration Mechanism

### Discovery
**Location**: `system.agents.*` table in `Frontier.root`

**Agent Detection**:
- Scripts in `system.agents` table are automatically registered as agents
- No explicit registration verb - presence in table is sufficient
- Agents execute in alphabetical order (table enumeration order)

### Agent Metadata (Inferred)
- **Name**: Table key (e.g., "MinutesSinceShip")
- **Code**: Script at `system.agents.<name>`
- **Context**: Global table context (can access `system.*` hierarchy)

---

## Agent Process Internals

### typrocessrecord Structure
**Relevant Fields** (`processinternal.h:46-99`):
```c
typedef struct typrocessrecord {
    hdlprocessrecord hnextprocess;     // Linked list pointer
    hdlprocesslist hprocesslist;       // Owning process list
    hdltreenode hcode;                 // Script parse tree
    hdlhashtable hcontext;             // Execution context table
    unsigned long sleepuntil;          // Wake-up time (ticks since boot)
    hdlprocessthread hthread;          // Thread executing this process
    boolean flsleepinbackground;       // Sleep when app backgrounded
    boolean flscheduled;               // Has thread been created?
    boolean floneshot;                 // True for one-shots, false for agents
    boolean flrunning;                 // Currently executing?
    boolean fldisposewhenidle;         // Pending disposal?
    bigstring bsmsg;                   // Last status message
    bigstring bsname;                  // Process name
    long processrefcon;                // Client data
} typrocessrecord;
```

### Process Creation
**For Agents** (`addnewprocess()` in `process.c:2754`):
```c
boolean addnewprocess(hdltreenode hcode, boolean floneshot,
                      langerrorcallback errorcallback, long errorrefcon) {
    hdlprocessrecord hnewnode;

    if (!newprocess(hcode, floneshot, errorcallback, errorrefcon, &hnewnode))
        return false;

    addprocess(hnewnode);  // Insert into process list

    return true;
}
```

---

## Time Slice Configuration

### Default Time Slices
**Constants** (`process.c:148-150`):
```c
static long processonehottimeslice = 6;   // 6 ticks (0.1 seconds) for one-shots
static long processagenttimeslice = 4;    // 4 ticks (~0.067 seconds) for agents
```

### Time Slice Semantics
- **1 tick** = 1/60th second (approximately 16.67ms)
- **4 ticks** = ~67ms per agent
- **6 ticks** = ~100ms per one-shot

### Configurable via Verbs
- `thread.setTimeSlice(ticks)`: Set current thread's time slice
- `thread.setDefaultTimeSlice(ticks)`: Set default for new threads

---

## Integration with tcp.listenStream

### Requirement
**tcp.listenStream** needs to:
1. Accept connections in background thread
2. Execute UserTalk callback for each connection
3. Maintain thread-local database context

### Agent Model Does NOT Apply
- **Why**: Callbacks must execute **immediately** on connection, not time-sliced
- **Solution**: Use one-shot thread model (dedicated pthread per connection)

### Callback Execution Pattern
**Proposed Flow**:
1. Accept thread: `pthread_create()` running `accept()` loop
2. Connection arrived: `pthread_create()` for callback handler
3. Callback thread: Initialize `tythreadglobals`, execute UserTalk callback
4. Callback complete: Cleanup thread, dispose `tythreadglobals`

**NOT Agent-Based**:
- Callback latency must be <1ms (agent scheduler runs every 1 second)
- No time-slicing needed (callback completes quickly)
- Dedicated thread provides isolation (no interference with agents)

---

## Security & Safety Considerations

### Agent Thread Crashes
**Impact**: All agents stop executing (agent thread terminated)

**Recovery**:
- `processscheduler()` detects missing agent thread
- Recreates agent thread after 1 second
- All agents resume execution

### Infinite Loop in Agent
**Impact**: Blocks all other agents (cooperative scheduling)

**Mitigation**:
- Time slice enforcement (agent yields after 4 ticks)
- User can kill agent thread: `thread.kill(thread.getNthID(1))`

### Agent Access to Global State
**Risk**: Agents can modify `system.*` tables, database structures

**Mitigation**:
- Agents run in isolated `tythreadglobals` context
- ODB operations are (currently) single-threaded
- POSIX migration MUST add mutex protection

---

## Testing Requirements

### Agent Lifecycle Tests
1. Create agent thread: `Frontier.enableAgents(true)`
2. Verify agent thread ID: `thread.getNthID(1)` stable across wake/sleep
3. Disable agents: `Frontier.enableAgents(false)` terminates thread
4. Re-enable agents: New thread created with new ID

### Agent Scheduling Tests
1. Multiple agents execute in time-sliced manner
2. One-shots execute before agents (process list order)
3. Agent sleep/wake behavior (timeouts, explicit wake)

### Agent Crash Recovery Tests
1. Kill agent thread: `thread.kill(thread.getNthID(1))`
2. Verify auto-recreation after 1 second
3. Agents resume execution in new thread

---

## Open Questions

1. **How are agents initially loaded?** (Startup scripts? Automatic table scan?)
2. **What happens if `system.agents.*` table is modified at runtime?** (Hot reload?)
3. **Can agents create sub-agents?** (Nested agent registration)
4. **What is the agent execution order?** (Alphabetical? Insertion order?)
5. **Are agent sleep states persistent across thread recreation?** (After kill/restart)

---

## Next Steps

See `IMPLEMENTATION_PLAN.md` for how agent thread migrates to POSIX threading model.
