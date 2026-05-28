# Cross-Thread Test Pattern Notes

Companion notes to issue #660 and the snapshot save/restore unit tests in
`tests/lang_causedby_snapshot_tests.c`. Records what the integration
harness can drive today, what it cannot, and what would unblock the
deferred behavioural test.

## What issue #660 asked for

The PR3 fix in PR #658 (commit `c2e8ce197`) added
`langsavecausedbysnapshot` / `langrestorecausedbysnapshot` (lang.c) and
wired them into `pushprocess` / `popprocess` (process.c) to close a
CWE-488 (Exposure of Data Element to Wrong Session) window: an outgoing
thread mid-try-body could leak `trybodydepth > 0` and a captured
causedby snapshot into the incoming thread's context, so the incoming
thread's errors would be attributed to the outgoing thread's
originating-error slot.

The deferred ask was an integration-level behavioural test that proves
the leak does not occur: thread A enters a try, yields, thread B errors
during A's yield, thread A resumes and errors, A's `tryErrorScript` /
`tryErrorLine` / protocol-level `error.causedBy.message` must all
reflect A's failure -- not B's.

## What the headless integration harness can drive

Integration tests run against the headless CLI (`frontier-cli`). The
harness can:

- Spawn threads via `thread.evaluate(scriptString)` and
  `thread.callscript(scriptName, params)`. Existing examples:
  `tests/integration/test_cases/thread_verbs_foundation.yaml`,
  `tests/integration/test_cases/error_recovery_concurrency_tests.yaml`.
- Coordinate yields with `thread.sleepTicks(N)` or `thread.sleepFor(s)`.
  The main thread releases the GIL at sleep boundaries; spawned threads
  block on GIL acquisition until the main thread yields.
- Run scripts in REPL mode (`repl_mode: true` in YAML) to support
  multi-statement input that mutates `system.temp.*` between assertions.
- Run protocol-mode tests with `protocol_ops:` to assert on
  `script/eval` response shape, including `error.causedBy.{message,
  location, stack}`.

What the harness cannot do today:

- Combine `thread.evaluate` with `protocol_ops`. Test 16 of
  `thread_verbs_foundation.yaml` records the empirical "thread.evaluate
  does not work in protocol mode" -- the protocol executor is a
  short-lived send-receive round-trip; spawned threads need a long-lived
  GIL holder to ever reach a yield boundary.

## Why the cross-thread behavioural assertion is not viable today

Investigation while preparing this PR surfaced two independent reasons
the leak does not have an observable behavioural expression in the
headless build:

1. **`pushprocess` / `popprocess` are stubbed to no-ops in headless.**
   `frontier-cli/stubs/headless_lang_runtime_more_stubs.c` defines:

   ```c
   boolean pushprocess (hdlprocessrecord p) { (void)p; return true; }
   boolean popprocess (void) { return true; }
   ```

   The PR3 wiring in `Common/source/process.c` calls
   `langsavecausedbysnapshot` and `langrestorecausedbysnapshot` from
   inside those functions, so the snapshot save/restore never fires in
   the headless build. (The behavioural leak the wiring protects against
   in the GUI build is therefore also not exercised in headless.)

2. **The headless thread.evaluate path does not push an error-callback
   frame.** `frontier-cli/headless_thread_verbs.c::thread_entry_point`
   calls `langruncode` (not `langrun`). `langruncode`
   (`Common/source/lang.c::1737`) does not call
   `langpusherrorcallback`; only `langrun` does
   (`lang.c::1349-1351`). When the spawned thread fires
   `lang.scriptError`, `langseterrorcallbackline` takes the early return
   at `(**hs).toperror <= 0` -- so the trybodydepth-gated capture site
   that mirrors B's stack into `causedbyerrorstack` is never reached.

   This means even if `trybodydepth > 0` leaks across the GIL handoff
   (which it can: `tests/headless_threadglobals.c::headless_save_threadglobals`
   does not save trybodydepth or causedby globals), B's error does not
   land in the buffer that A's else block reads from. So the leak's
   visible expression in A's `tryErrorScript` / `tryErrorLine` / protocol
   `causedBy.message` is suppressed by the langruncode path -- it does
   not show even when the fix is reverted.

Empirical confirmation: instrumented runs reverting just
`langsavecausedbysnapshot` / `langrestorecausedbysnapshot` inside
`pushprocess` / `popprocess` produce byte-identical output for the
cross-thread try/else probe, because (a) those functions are stubbed
out anyway and (b) langruncode's missing error-callback push blocks the
capture path independently.

## What we CAN guard today (and what this PR adds)

A unit-level test exercising the actual snapshot save/restore primitives
directly: `tests/lang_causedby_snapshot_tests.c`. This catches future
regressions in `langsavecausedbysnapshot` /
`langrestorecausedbysnapshot` themselves.

Falsification:

- Reverting the reset block in `langsavecausedbysnapshot` (the four
  lines that zero live state after the capture): 2 of 4 unit tests fail.
- Reverting the entire body of `langrestorecausedbysnapshot` to a
  no-op: 4 of 4 unit tests fail.

What the unit test does not cover:

- The pushprocess/popprocess wiring itself (covered by code review and
  the snapshot functions' contracts -- if either function silently
  degrades, the unit test catches it; if the wiring is removed from
  pushprocess/popprocess, only a GUI integration test would notice).
- The cross-thread behavioural assertion the issue originally asked
  for. This is the deferred work below.

## What would unblock the deferred behavioural test

To make the issue-#660 behavioural assertion viable, at minimum one of:

1. **Extend `headless_save_threadglobals` to cover `trybodydepth`,
   `flcausedbyerrorvalid`, `causedbyerrorstackdepth`, and the
   `causedbyerrorstack` + `causedbyerrormessage` buffers.** This puts
   the headless thread switching mechanism on the same footing as the
   PR3 fix's process-stack mechanism in the GUI build. Adding the fix to
   the headless thread-switch path is itself a behaviour-improving
   change, because today those globals do leak across `thread.evaluate`
   handoffs (even though, per point 2 above, that leak does not surface
   in `causedBy` reporting). Once headless leak is closed, a YAML test
   could prove the closure by toggling the save/restore in
   `headless_save_threadglobals` and observing a behavioural delta.

2. **Have `langruncode` push an error-callback frame at entry, mirroring
   what `langrun` does.** This would make spawned threads' errors
   reachable from the capture site in `langseterrorcallbackline` and
   therefore expressible through `tryErrorScript` etc. This is a
   reasonable change on its own merit: today, a `thread.evaluate` script
   that errors does not populate `tryError*` for any enclosing try in
   the same thread, which is arguably surprising. (It would also need
   to be paired with #1 to actually close the leak rather than just
   make it visible.)

3. **Add a kernel-side test fixture that drives the cross-thread
   scenario at the C unit level.** This would mean writing a C test
   that mimics the GUI build's pushprocess/popprocess call sites
   directly, without going through the UserTalk verb dispatch. The unit
   test added in this PR is the smallest step in that direction.

## Reusing this pattern for other cross-thread invariants

If future PRs need similar regression coverage:

- For invariants of the **snapshot mechanism itself**: extend
  `lang_causedby_snapshot_tests.c` with a new test function. Each test
  uses `set_live_state(depth, valid, message)` to establish a known
  starting state and asserts on the snapshot's by-value preservation
  guarantees.
- For invariants of the **headless thread-switching mechanism**: once
  #1 above is implemented, add a YAML integration test in
  `tests/integration/test_cases/` following the
  `error_recovery_concurrency_tests.yaml` pattern. Use `repl_mode: true`
  for tests that spawn threads.
- For invariants of the **2026-03-12 `tryerror` thread-switch safety
  net** in `Common/source/langcallbacks.c::langerrormessage`: this is
  the per-thread `tyerrror` handle, not the causedby globals -- a
  different mechanism. A behavioural test for that one is more
  tractable because `tryerror` IS already part of `tythreadglobals`
  (line 225 of `processinternal.h`), so per-thread state is correctly
  isolated; the safety net protects against a callback swap, not a
  global leak. A REPL-mode YAML test that fires `lang.scriptError` from
  a thread.evaluate inside a try and asserts on `tryError` contents
  would catch that path.

## Cross-references

- Issue: #660
- Snapshot functions: `Common/source/lang.c::langsavecausedbysnapshot`,
  `langrestorecausedbysnapshot`
- Process-stack wiring (no-op in headless): `Common/source/process.c::pushprocess`,
  `popprocess`
- Headless stubs: `frontier-cli/stubs/headless_lang_runtime_more_stubs.c`
- Headless thread switching:
  `frontier-cli/headless_thread_verbs.c::thread_entry_point`,
  `tests/headless_threadglobals.c::headless_save_threadglobals` and
  `headless_restore_threadglobals`
- Unit test: `tests/lang_causedby_snapshot_tests.c`
