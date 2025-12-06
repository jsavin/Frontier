# Processor Audit: `clock`

**Status:** ✅ Ready for Implementation
**Audit Date:** 2025-12-05
**Auditor:** Claude (Sonnet 4.5)

---

## Basic Information

| Property | Value |
|----------|-------|
| **Processor Name** | `clock` |
| **EFP ID** | 1005 (lang block) |
| **Verb Count** | 7 |
| **Window Required** | NO |
| **Documentation** | [clock/](../../../docs/usertalk/docserver.userland.com/clock/index.html) |
| **Stub Implementation** | [headless_clock_verbs.c](../../../tests/headless_clock_verbs.c) |

---

## Category Assessment

**Category:** ✅ **Core Functionality**

**Rationale:**
Time and timing operations are essential system utilities with no GUI dependencies. Operations include reading system time, sleeping/waiting, and timer management. All operations use standard OS time APIs available on all platforms.

**Headless Compatibility:** ✅ **Full**

**Blocking Verbs:** None

---

## Verb Inventory

| # | Verb Name | Signature | Description |
|---|-----------|-----------|-------------|
| 1 | `now` | `clock.now() -> long` | Get current time (seconds since epoch) |
| 2 | `set` | `clock.set(time)` | Set system time (requires admin privileges) |
| 3 | `sleepfor` | `clock.sleepFor(seconds)` | Sleep for specified seconds |
| 4 | `ticks` | `clock.ticks() -> long` | Get system ticks since startup (1/60th second units) |
| 5 | `milliseconds` | `clock.milliseconds() -> long` | Get milliseconds since startup |
| 6 | `waitseconds` | `clock.waitSeconds(seconds)` | Wait/yield for seconds (non-blocking) |
| 7 | `waitsixtieths` | `clock.waitSixtieths(sixtieths)` | Wait/yield for sixtieths of second (non-blocking) |

**Note:** Documentation shows additional verbs (idleTime, timerExpired, timeStamp) not in kernelverbs.rc - these may be UserTalk wrappers or later additions.

---

## Implementation Analysis

### Complexity: **LOW-MEDIUM**

### Dependencies

- **Other Processors:** None
- **External Services:** None
- **OS-Specific Functionality:** YES (time APIs: time(), gettimeofday(), nanosleep(), clock_gettime())
- **GUI/Window Context:** NO

### Key Implementation Notes

**Time Representations:**
- `clock.now()` - Unix timestamp (seconds since 1970-01-01 00:00:00 UTC)
- `clock.ticks()` - System uptime in 1/60th second units (legacy Mac tick count)
- `clock.milliseconds()` - Milliseconds since process/system startup

**Platform Time APIs:**
```c
// clock.now() - Current time
time_t now = time(NULL);  // POSIX standard

// clock.ticks() - System ticks (1/60 second)
// macOS: mach_absolute_time() or clock_gettime(CLOCK_MONOTONIC)
// Windows: GetTickCount64()
// Linux: clock_gettime(CLOCK_MONOTONIC)

// clock.milliseconds() - Milliseconds
// Similar to ticks but millisecond resolution

// clock.sleepFor() / waitSeconds() - Sleep/yield
nanosleep() on POSIX, Sleep() on Windows

// clock.waitSixtieths() - Wait 1/60 second units
nanosleep() with 16.67ms per sixtieth
```

**Sleep vs Wait:**
- `sleepFor()` - Blocking sleep (thread suspended)
- `waitSeconds()` / `waitSixtieths()` - Non-blocking wait (yields to other scripts, processes events)
- In headless mode, both can be simple sleeps

**Edge Cases:**
- `clock.set()` requires root/admin privileges - will fail without proper permissions
- Tick/millisecond counters may wrap after ~49 days on 32-bit systems
- Time zone considerations for `clock.now()`
- Leap seconds (generally ignored by Unix time)

**Type Coercion:**
- Time values are long integers
- Fractional seconds truncated for sleepFor/waitSeconds
- waitSixtieths allows finer granularity (1/60 sec = ~16.67ms)

---

## UserTalk Documentation Notes

From docserver.userland.com/clock/:

**clock.now()**
- Returns current time as long integer (Unix timestamp)
- Seconds since January 1, 1970 00:00:00 UTC
- Standard for date/time operations

**clock.set(time)**
- Sets system clock to specified time
- Requires administrator privileges
- Use with caution - affects entire system

**clock.sleepFor(seconds)**
- Blocks execution for specified seconds
- Other scripts cannot run during sleep
- Use for simple delays

**clock.ticks()**
- Returns system ticks since startup
- 1 tick = 1/60 second (16.67ms)
- Legacy Mac OS compatibility
- Useful for timing intervals

**clock.milliseconds()**
- Returns milliseconds since startup
- Higher precision than ticks
- Good for performance measurement

**clock.waitSeconds(seconds)**
- Non-blocking wait
- Yields to other scripts/processes
- Preferred over sleepFor for cooperative multitasking

**clock.waitSixtieths(sixtieths)**
- Non-blocking wait in 1/60 second units
- Finer granularity than waitSeconds
- 60 sixtieths = 1 second

---

## Testing Requirements

**Minimum Test Cases Per Verb:**
- Valid inputs (various time values)
- Boundary conditions (0, negative, very large)
- Precision verification
- Cross-platform consistency

**Test Scenarios:**
```usertalk
// clock.now()
clock.now()                      → current Unix timestamp (e.g., 1733443200)

// clock.ticks()
local (t1 = clock.ticks())
clock.sleepFor(1)
local (t2 = clock.ticks())
(t2 - t1)                        → ~60 (1 second = 60 ticks)

// clock.milliseconds()
local (m1 = clock.milliseconds())
clock.sleepFor(1)
local (m2 = clock.milliseconds())
(m2 - m1)                        → ~1000 (1 second = 1000ms)

// clock.sleepFor()
clock.sleepFor(2)                → delays 2 seconds

// clock.waitSeconds()
clock.waitSeconds(1)             → yields for 1 second

// clock.waitSixtieths()
clock.waitSixtieths(60)          → yields for 1 second (60/60)

// clock.set() - requires root, test with error handling
clock.set(clock.now())           → may fail with permission error
```

**Edge Case Tests:**
- `clock.sleepFor(0)` → immediate return
- `clock.sleepFor(-1)` → should error or treat as 0
- `clock.waitSixtieths(1)` → ~16.67ms delay
- Timer wraparound on long-running processes (ticks/milliseconds)

**Platform Differences:**
- Time resolution varies by OS
- Windows tick count uses different epoch than Unix time
- Verify consistent behavior across macOS, Linux, Windows

---

## Implementation Effort

**Estimated Time:** 3-4 hours

**Breakdown:**
- Implementation: 1.5-2 hours (platform time API wrappers)
- Testing: 1-1.5 hours (7 verbs × 3-4 test cases each)
- Platform-specific testing: 0.5-1 hour
- Documentation: 0.5 hours

**Confidence:** HIGH - Well-defined OS APIs, straightforward implementation

**Platform-Specific Work:**
- Abstract time APIs into portable layer
- Handle tick/millisecond counter initialization
- Test sleep/wait behavior on each platform

---

## Priority & Sequencing

**Priority:** 🏆 **QUICK WIN** (Tier 1)

**Recommended Implementation Order:** 12 (after bit)

**Blockers/Prerequisites:** None

**Implementation Sequence:**
1. Implement clock.now() (simplest - just time())
2. Implement clock.sleepFor() (basic sleep)
3. Implement clock.ticks() and clock.milliseconds() (platform-specific)
4. Implement clock.waitSeconds() and clock.waitSixtieths() (yield-aware)
5. Implement clock.set() with privilege checking
6. Write comprehensive tests
7. Test timing accuracy on each platform

---

## Quick Win Justification

**Why This Is a Quick Win:**
1. **Essential Utility:** Critical for timing, scheduling, delays
2. **Well-Defined APIs:** Standard POSIX/Win32 time functions
3. **No Dependencies:** Standalone functionality
4. **Portable:** Works on all platforms with minor abstraction
5. **High Value:** Enables time-based scripts and operations
6. **Testing:** Easy to verify (measure actual delays)

**Value Proposition:**
- Required for scheduled tasks, polling, rate limiting
- Foundation for date/time manipulation
- Critical for performance measurement
- Enables cooperative multitasking (wait verbs)

---

## Related Processors

- **date** - Date manipulation (depends on clock.now() conceptually)
- **thread** - May use clock for scheduling
- **sys** - System operations may use timing

---

## Special Considerations

**Threading & Yielding:**
- `waitSeconds()` and `waitSixtieths()` should yield to Frontier's thread scheduler
- In headless mode without GUI event loop, may simplify to sleep
- Consider implementing with `sys.systemTask()` calls to yield properly

**Time Zones:**
- `clock.now()` returns UTC time
- Local time conversion handled by date processor
- Be consistent about UTC vs local time

**Precision Limitations:**
- Ticks (1/60 sec) = 16.67ms resolution
- OS sleep granularity varies (typically 1-15ms)
- High-precision timing may need special APIs (clock_gettime with CLOCK_MONOTONIC)

**Long-Running Processes:**
- Tick/millisecond counters wrap on 32-bit systems
- Consider 64-bit counters for milliseconds to avoid wraparound
- Document wraparound behavior

---

## References

**Documentation:**
- DocServer: `docs/usertalk/docserver.userland.com/clock/`
- Individual verb pages: `docs/usertalk/docserver.userland.com/clock/{verb}.html`

**Implementation:**
- Stub: `tests/headless_clock_verbs.c`
- Legacy source: `/Users/jake/dev/tedchoward/Frontier/Common/source/langverbs.c`

**Standards:**
- POSIX time APIs: time(), gettimeofday(), nanosleep(), clock_gettime()
- Windows time APIs: GetTickCount64(), Sleep(), GetSystemTime()
- Unix timestamp: seconds since 1970-01-01 00:00:00 UTC

---

## Next Steps

1. ✅ Audit complete - ready for implementation
2. ⏳ Implement all 7 verbs in `tests/headless_clock_verbs.c`
3. ⏳ Create platform abstraction layer for time APIs
4. ⏳ Write unit tests in `tests/unit/test_clock_verbs.c`
5. ⏳ Test timing accuracy on macOS, Linux, Windows
6. ⏳ Document platform-specific behaviors
7. ⏳ Update implementation status

---

**Audit Status:** ✅ Complete and Approved for Implementation
