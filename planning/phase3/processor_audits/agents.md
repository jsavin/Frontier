# System Audit: `agents`

**Status:** ⚠️ **Partial Headless Compatibility (Architecture-Dependent)**
**Audit Date:** 2025-12-06
**Auditor:** Claude (Haiku 4.5)

---

## Basic Information

| Property | Value |
|----------|-------|
| **System Name** | `system.agents` |
| **Agent Count** | 6 background agents |
| **Window Required** | NO |
| **Implementation Type** | Script Verbs (UserTalk) |
| **Location** | `system.agents/` (6 `.ut` files) |
| **Extension Point** | `user.agents/` (user-defined agents) |
| **Scheduling** | Background task scheduler (architecture TBD) |

---

## Category Assessment

**Category:** ⚠️ **Background Task Scheduler & Monitoring**

**Rationale:**
Agents are background tasks that run periodically or in response to system conditions. Most are script-based monitoring utilities. Headless-compatible if background task scheduler is implemented; requires architectural support (daemon mode, task scheduling, inter-process communication).

**Headless Compatibility:** ✅ **Full** (if scheduler implemented)

**Architectural Requirement:** Background task execution framework

---

## Agent Inventory

### Performance Monitoring (2 agents)

| Agent | Purpose | Headless |
|-------|---------|----------|
| `SchedulerMonitor` | Monitor task scheduler health | ✅ YES |
| `StatusMessage` | Display/log status messages | ✅ YES (stdout) |

### Application Monitoring (2 agents)

| Agent | Purpose | Headless |
|-------|---------|----------|
| `MenuMonitor` | Watch active GUI window and manage menu bar (Tools framework integration) | ❌ NO (GUI-only) |
| `FrontierPath` | Monitor Frontier paths/configuration | ✅ YES |

### Async Operations (1 agent)

| Agent | Purpose | Headless |
|-------|---------|----------|
| `asynchRPC` | Handle asynchronous RPC calls | ✅ YES |

### Utility (1 agent)

| Agent | Purpose | Headless |
|-------|---------|----------|
| `MinutesSinceShip` | Calculate time since release | ✅ YES |

---

## Implementation Analysis

### Complexity: **LOW to MEDIUM** (Scheduler Already Exists)

### Dependencies

- **Other Processors:** All (agents can call any verb)
- **External Services:** Potentially (depends on agent function)
- **GUI/Window Context:** Minimal (agents are background tasks)
- **Existing Components:**
  - `system.verbs.builtins.scheduler` - Fully implemented in UserTalk
  - Kernel timed callbacks (if available) - for triggering agent execution

### Key Implementation Notes

**Agent Execution Model:**

1. **Scheduling Mechanism:**
   - Agents execute on periodic schedule (every N seconds/minutes)
   - OR execute in response to system conditions
   - Requires event loop with task queue

2. **Execution Context:**
   - Agents run asynchronously (don't block main thread)
   - Have access to full UserTalk runtime
   - Can manipulate databases, call verbs, etc.

3. **Error Handling:**
   - Agent errors should not crash system
   - Errors logged but execution continues
   - May trigger retry logic

4. **Scalability:**
   - Multiple agents can run concurrently
   - Need thread-safe database access
   - Need performance monitoring (to avoid runaway agents)

**Script-Based Implementation:**

```usertalk
// Example agent structure
on SchedulerMonitor {
    // Check scheduler health
    try {
        // Monitor task queue, report status
    } catch {
        // Log error, continue
    }
}
```

---

## Headless Compatibility Analysis

**Agents Headless-Compatible:** ✅ 5/6 agents

**Breakdown:**

**Directly Useful in Headless (100%):**
- ✅ `SchedulerMonitor` - Monitor background task health (essential for headless daemon)
- ✅ `StatusMessage` - Log status to stdout (essential for monitoring)
- ✅ `asynchRPC` - Handle async RPC (useful for server mode)
- ✅ `FrontierPath` - Configuration monitoring (useful)
- ✅ `MinutesSinceShip` - Utility function (always useful)

**GUI-Specific (Defer to GUI Implementation):**
- ❌ `MenuMonitor` - Watch active window and manage application menu bar (Tools framework integration, GUI-only; defer to GUI phase)

---

## Headless Implementation Strategy

**Scheduler Already Exists:**
- `system.verbs.builtins.scheduler` is fully implemented in UserTalk
- Kernel timed callbacks may be available for scheduling integration
- No architectural design or infrastructure work needed

**Phase 1 - Understand Existing Scheduler (2-3 hours)**
1. Review `system.verbs.builtins.scheduler` implementation
2. Understand scheduling API (register agent, set interval, etc.)
3. Identify kernel callback integration points (if any)
4. Document how agents hook into scheduler

**Phase 2 - Implement 5 Built-in Agents (3-4 hours)**
1. Implement SchedulerMonitor (monitor scheduler health)
2. Implement StatusMessage (logging to stdout)
3. Implement asynchRPC (async RPC support)
4. Implement FrontierPath (configuration monitoring)
5. Implement MinutesSinceShip (utility function)
6. Skip MenuMonitor (GUI-only; defer to GUI phase)

**Phase 3 - Integration & Testing (2-3 hours)**
1. Hook agents into scheduler
2. Test agent execution and error handling
3. Verify graceful shutdown
4. Add monitoring/debugging hooks

**Phase 4 - User Agents (Medium Priority)**
1. Enable user.agents/ for custom agents
2. Provide agent lifecycle hooks (startup, shutdown)
3. Document agent programming model
4. Provide examples

---

## Headless Usage Scenarios

**Scenario 1: Health Monitoring**

```usertalk
// SchedulerMonitor runs every 10 seconds
on SchedulerMonitor {
    local (queueSize = sys.verbs.scheduler.getQueueSize())
    if (queueSize > 100) {
        sys.verbs.logger.warning("Task queue backing up: " + queueSize + " items")
    }
}
```

**Scenario 2: Async RPC Handling**

```usertalk
// asynchRPC handles responses from async RPC calls
on asynchRPC {
    local (pending = system.agents.asynchRPC.getPendingCalls())
    if (pending > 0) {
        // Process pending responses
    }
}
```

**Scenario 3: Status Reporting**

```usertalk
// StatusMessage logs periodic status
on StatusMessage {
    local (uptime = clock.elapsedSeconds())
    sys.unixShellCommand("logger 'Frontier uptime: " + uptime + " seconds'")
}
```

---

## Testing Strategy

**Scheduler Testing:**
- Verify agents execute on schedule
- Verify error in one agent doesn't prevent others
- Test graceful shutdown (wait for agents to finish)
- Test concurrent agent execution
- Test resource limits

**Agent Testing:**
- Unit test each agent script independently
- Integration test agents with database access
- Stress test with high agent frequency
- Monitor resource usage (CPU, memory)

**Performance Testing:**
- Measure agent execution time
- Measure scheduler overhead
- Test with hundreds of agents
- Identify bottlenecks

---

## Architectural Decisions Needed

1. **Execution Model:**
   - Threaded (OS threads) vs. cooperative (event loop)
   - Pro-threaded: True parallelism, better for I/O-bound tasks
   - Pro-cooperative: Simpler, fewer synchronization issues

2. **Scheduling Strategy:**
   - Fixed interval (every N seconds)
   - Cron-style (configurable schedules)
   - Event-based (trigger on condition)
   - Combination?

3. **Persistence:**
   - Store agent state in database?
   - Survives restart?
   - Recover from crash?

4. **Monitoring:**
   - Execution log for each agent?
   - Performance metrics?
   - Health dashboard?

---

## Related Components

- **Callbacks** - Similar event-driven pattern (complements agents)
- **Thread** - Threading support (needed for background execution)
- **Clock** - Timing/scheduling (needed for periodic execution)
- **Logger** - Logging agent activity
- **All Processors** - Agents can call any verb

---

## Summary

**Status:** ⚠️ **5/6 Agents Viable for Headless (Architecture-Dependent)**

**Key Findings:**
1. 5/6 agents are script-based and headless-compatible
2. MenuMonitor is GUI-specific (watches active window, manages menu bar, Tools framework integration); defer to GUI phase
3. Agents require background task scheduler (architectural component)
4. Scheduler must be thread-safe and production-quality
5. Useful for headless daemon mode (health monitoring, async operations)
6. Straightforward implementation once scheduler exists

**Critical Path:**
1. Review existing scheduler implementation (2-3 hours)
2. Implement 5 headless agents (3-4 hours)
3. Integrate agents with scheduler (2-3 hours)
4. Skip MenuMonitor (GUI-only; implement with GUI framework later)
5. Enable user agents (documentation + testing)

**Recommendation:**
- **Phase 1:** Review existing scheduler and understand integration points (HIGH PRIORITY - unblocks agents)
- **Phase 2:** Implement 5 built-in headless agents (3-4 hours)
- **Phase 3:** Integration & testing (2-3 hours)
- **Phase 4:** Enable user agents (medium effort)
- **Phase 5+:** Implement MenuMonitor when GUI framework exists

**Estimated Total Effort:** 7-10 hours (agents implementation; scheduler already exists)

**Note:** Scheduler infrastructure already exists at `system.verbs.builtins.scheduler`. No architectural blocking work needed. Straightforward implementation of agent scripts once scheduler integration is understood. MenuMonitor is deferred to GUI phase.

---

**Audit Status:** ⚠️ Complete (Awaiting Scheduler Architecture Decision)
