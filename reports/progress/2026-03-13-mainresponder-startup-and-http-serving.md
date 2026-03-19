# Frontier Progress Report: mainResponder Startup & HTTP Serving

**Date:** March 13, 2026
**Status:** Active Development
**Milestone:** HTTP Server Serves Pages via mainResponder
**Period Covered:** February 28 - March 13, 2026 (14 days)

---

## Executive Summary

This two-week period achieved a **critical end-to-end milestone**: the HTTP server now starts, accepts connections, dispatches callbacks through the GIL, runs mainResponder's UserTalk `respond` script, and serves pages. Getting there required fixing bugs across every layer of the stack -- GIL deadlocks, verb resolution, database persistence, thread context corruption, and missing verb implementations. The work also hardened test infrastructure with proper isolation and resolved 19 consistently-failing integration tests.

---

## Major Accomplishments

### TIER 1: HTTP Server & mainResponder End-to-End (PRs #462-#480)

**The Headline:** Frontier's HTTP server now starts via `inetd.startOne`, dispatches requests through mainResponder, and serves web pages -- the first time the full request pipeline has worked in the CLI.

**The journey, bug by bug:**

1. **GIL deadlock in callback dispatch (PR #463):** TCP callbacks were only processed from the REPL idle loop. In `-e` mode and at `langbackgroundtask()` yield points, connections were accepted but callback threads were never spawned. Fixed by calling `tcp_process_callbacks()` at every yield point and converting `headless_thread_sleep()` to a 50ms polling loop.

2. **Database persistence on exit (PRs #462, #464):** Guest database registrations created during `finishInstall()` were lost between sessions. PR #462 added `save_system_root_on_exit()`. PR #464 removed a `dbdirtymask` shortcut that silently skipped saving in-memory table modifications -- the root cause of lost `user.databases` entries.

3. **Path handling for site configuration (PR #466):** `portable_filefrompath("/path/users/")` returned empty string for directory paths ending with `/`, breaking `radioCommunityServerSuite.init` during startup. Fixed to strip and re-append trailing separators.

4. **GIL yielding in blocking REPL (PR #467):** Replaced blocking `linenoise()` with a `poll()`-based input loop that yields the GIL every 10ms. Background threads and TCP callbacks now run while the REPL waits for input.

5. **Stale EFP fast-path (PR #469):** A "temporary shim" in `langgethandlercode()` checked `efptable` BEFORE `system.paths` for dotted verbs, violating the correct search order. This blocked `inetd.startOne`, `inetd.isDaemonRunning`, and other UserTalk scripts stored under EFP-named tables.

6. **Missing verb stubs (PR #470):** `Frontier.isPowerPC()` was a stub returning "not implemented", causing `inetd.startOne()` to fail. The `tcp.listenStream` UserTalk wrapper checks `isPowerPC()` to decide whether to create extra listener sockets (a legacy workaround for 68k Macs). Implemented to return `true` (modern TCP stack).

7. **window.getFile() implementation (PRs #474, #476):** `setupFrontier` web form submission calls `window.getFile(@root)` to get the system root database path. Implemented the verb for headless mode, handling root tables, guest DB roots, and objects inside tables. Follow-up fixed the fallback behavior for nonexistent addresses.

8. **tryError corruption across thread yields (PR #478, fixes #477 P0):** Thread context switches inside `try` bodies (at `langbackgroundtask` yield points) caused `pushprocess`/`popprocess` to replace the `langtryerror` error callback with the incoming thread's `langerrordialog` callback. When an error fired under the wrong callback, `tryerror` was never defined as a local variable, causing "tryError hasn't been defined" in `mainResponder.respond`.

9. **Guest database WP conversion error (PR #479):** `langexternalgetfullpath()` raised `nopatherror` when converting word processor values in guest databases (can't walk table hierarchy across DB boundaries). Although the return value was handled, the error callback still fired, poisoning the script's error state.

10. **Binary object metadata (PR #480):** `timeModified()` and `timeCreated()` wrapper functions returned `false` as a script error for binary objects, instead of returning a boolean false value. This caused 500 errors when serving images through mainResponder. The original Frontier dispatch correctly used `setbooleanvalue(false, v)`.

**Why This Matters:**
- **First working HTTP request pipeline** in the CLI -- the core value proposition of Frontier
- Every bug was a different subsystem (GIL, persistence, path handling, verb resolution, thread safety, error propagation) -- this validates the architecture holds together end-to-end
- The `tryError` fix (PR #478) exposed a fundamental thread-safety issue in the process context switching mechanism that would have affected any multi-threaded UserTalk execution

---

### TIER 1: Test Infrastructure Hardening (PR #468)

**The Headline:** 19 consistently-failing integration tests fixed, test isolation improved across 12 test files.

**What Was Fixed:**
- **REPL `/list [n]` SIGSEGV:** Missing `hashresolvevalue()` call in `resolve_indexed_node()` -- the bare-index code path read node values without resolving disk values first
- **18 outline test failures:** Tests created outlines under `workspace.*` which persisted via `save_on_exit`, causing materialization failures in subsequent runs
- **Systematic migration:** ALL integration test `workspace.*` references migrated to `system.temp.*` across 12 test files for proper test isolation (`system.temp` is cleared on each process startup)

---

## Quality Metrics

### Code Changes (Feb 28 - Mar 13)

| Metric | Value |
|--------|-------|
| **Pull Requests Merged** | 13 |
| **Issues Closed** | #455, #456, #457, #458, #477 |
| **Commits to develop** | 17 |

### Integration Tests

| Metric | Start (Feb 27) | End (Mar 13) | Delta |
|--------|----------------|--------------|-------|
| Total Tests | 1,893 | 1,951 | +58 |
| Passed | 1,704 | 1,761 | +57 |
| Skipped | 189 | 190 | +1 |
| Failed | 0 | 0 | 0 |
| Unit Tests | 302 | 302 | 0 |

### Key Technical Achievements

| Achievement | Detail |
|-------------|--------|
| **HTTP request pipeline working** | TCP accept -> GIL callback -> mainResponder.respond -> serve page |
| **GIL yielding in REPL** | poll()-based input loop, 10ms yield cycle |
| **19 test failures resolved** | SIGSEGV fix + workspace isolation across 12 files |
| **+58 integration tests** | Net new test coverage |
| **Thread context bug found** | tryError corruption via pushprocess/popprocess at yield points |
| **Database save-on-exit** | System root persists in-memory changes across sessions |

---

## Strategic Impact

### What This Period Accomplished

This period transformed Frontier CLI from "boots up and runs scripts" to "serves web pages through mainResponder." Every layer of the stack was exercised and debugged: TCP socket handling, GIL-mediated callback dispatch, UserTalk script execution across thread boundaries, database persistence, verb resolution, and error propagation. The fact that 10 distinct bugs across different subsystems needed fixing to complete one HTTP request cycle is typical of bringing an integration point online for the first time.

### What's Working Now

- HTTP server starts via `inetd.startOne` and listens on configured port
- TCP connections accepted, callbacks dispatched through GIL
- mainResponder's `respond` script executes, serves pages
- Binary objects (images) served with correct metadata handling
- System root database saved on exit, preserving runtime state
- REPL yields GIL, allowing background threads to run concurrently

### What Still Needs Work

- **setupFrontier web form completion** -- form submission flow is close but may have remaining edge cases
- **Manila guest database** -- full Manila installation and serving not yet tested end-to-end
- **Thread context safety** -- PR #478 fixed one symptom of process context corruption at yield points; the underlying `pushprocess`/`popprocess` mechanism needs architectural review
- **Long-running stability** -- HTTP server has not been tested under sustained load

---

## Lessons Learned

### What Worked Well

**Following the startup script as a roadmap:** Each bug was discovered by running the startup flow one step further. This natural ordering (GIL deadlock -> persistence -> path handling -> verb resolution -> error handling -> serving) made debugging systematic rather than random. Each fix unblocked the next stage.

**Test isolation migration (PR #468):** Converting `workspace.*` references to `system.temp.*` across 12 test files was tedious but eliminated a persistent class of flaky test failures caused by state pollution between runs.

### Challenges Overcome

**Thread context corruption (PR #478):** The `tryError` bug was particularly subtle -- it only manifested when a thread yield happened inside a `try` block AND an error occurred after the context switch replaced the error callback. Diagnosing this required understanding the interaction between three systems: the GIL scheduler, the process stack, and UserTalk's error handling mechanism.

**Layered error propagation (PR #479):** The WP conversion bug showed how an error callback firing (even when the return value is handled) can poison downstream execution. The fix was to suppress the error callback, not just handle the return value -- a pattern that may need systematic review elsewhere.

---

## Next Steps

### Immediate (This Week)

1. **Complete setupFrontier flow** -- verify the web form submission works end-to-end
2. **Test Manila serving** -- install and serve a Manila site
3. **Review pushprocess/popprocess thread safety** -- architectural assessment of PR #478's root cause

### Mid-Term (2-4 Weeks)

1. **HTTP stability testing** -- sustained request handling, concurrent connections
2. **Guest database operations** -- full lifecycle (create, mount, serve, unmount)
3. **Begin GUI protocol layer** -- now that CLI serving works, bridge to GUI

### Long-Term (1-2 Months)

1. **GUI Alpha Release**
2. **Issue #86** -- Runtime context architecture (relates to thread context bugs found this period)
3. **Phase 4 P0a** -- Global state elimination

---

## Recognition

**Co-Authored-By:** Claude Opus 4.6 <noreply@anthropic.com>

---

**Period Status:** COMPLETE -- HTTP server serves pages via mainResponder, 19 test failures resolved, +58 integration tests, zero test failures maintained
