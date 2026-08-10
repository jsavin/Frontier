# Unit 1.3 Verification: Default-REPL Debugger Behavior + Guest-DB Breakpoint Matching

2026-08-09. Runtime falsification results for the two verification questions left open by the
unit 1.3 diagnosis (callScript debugger attach itself is already fixed by PR #722; the lazy-attach
path lives at `frontier-cli/debug_handler.c:700-776`). Facts only; no fixes were made.

## Method

PTY-driven sessions patterned on `tests/debugger_tui_pty_test.py` (`pty.fork()` so boxen gets a
controlling terminal). Two harness facts worth recording for future PTY tests:

- `pty.fork()` leaves the PTY winsize 0x0; termbox then clips every cell and emits only cursor
  moves. Set `TIOCSWINSZ` on the master fd or the screen scrape sees no text.
- `defined(@addr)` returns true for a bare `@` address literal even when the target does not
  exist (observed: `defined(@system.temp.u13m)` -> true while `string(system.temp.u13m)` errors
  with "the name u13m hasn't been defined"). Existing protocol tests already guard against this
  by using `defined(@x) and x` (`tests/debug_protocol_test.sh`, Test 20). Marker readbacks in
  PTY experiments must read the value, not `defined()` it.

## Finding 1: The default boxen REPL offers NO way to arm a protocol breakpoint

The scenario "set a breakpoint, then dispatch via thread.callScript" cannot currently be
expressed in the default REPL (plain `frontier-cli`, which dispatches to `boxen_repl_main` per
`frontier-cli/main.c:884-903`):

- The only writers of the protocol breakpoint table `g_breakpoints`
  (`frontier-cli/debug_handler.c:104`) are the `debug/setBreakpoint` / `debug/clearBreakpoints`
  protocol ops (`frontier-cli/debug_handler.c:1594`, dispatched at
  `frontier-cli/op_handler.c:953-958`). The boxen REPL exposes no slash command, palette entry,
  or verb that dispatches them (no `debug` hits in `frontier-cli/repl_slash_resolver.c`; the
  only `op_dispatch` use in `frontier-cli/boxen_repl.c` is the startup auto-launch at line 2135).
- The boxen outline editor's breakpoint toggle (`frontier-cli/boxen_outline.c:556-560`,
  `boxen_outline_real_toggle_breakpoint` at line 1334) sets the legacy per-node outline flag
  `flbreakpoint` (`Common/headers/op.h:87`), as do the legacy `op.setBreakpoint` /
  `op.clearBreakpoint` verbs (`Common/source/opverbs.c:223-227`). Nothing bridges that flag to
  `g_breakpoints`, and the headless debugger callback `protocol_debugger_callback` never reads
  it -- breakpoint matching is string comparison against `g_breakpoints[i].script`
  (`frontier-cli/debug_handler.c:729-736` lazy path, `:845-870` registered path). Outline
  breakpoints are decorative in the headless build.
- The old `--debug-tui` debugger TUI, which could dispatch `debug/setBreakpoint`
  (`frontier-cli/debugger_tui.c:1457`), is unreachable: `--debug-tui` is a deprecated alias
  that routes to `boxen_repl_main` (`frontier-cli/main.c:884-889`). `debugger_tui_main` has no
  remaining caller in the CLI dispatch.

**Observed (Experiment A):** default REPL, no flags, staged Virgin.root. Installed a marker
script via the input line, dispatched `thread.callScript(@system.temp.u13x, {})`, read the
marker back: `M-true` rendered -- the callScript thread ran to completion. No suspension, no
park. Consistent with the lazy-attach gate: `g_debug_attach_transport` is non-NULL in the boxen
REPL (registered unconditionally at `frontier-cli/boxen_repl.c:2120-2132`), but
`g_has_breakpoints` is false, so the callback takes the cheap-path return at
`frontier-cli/debug_handler.c:716-719`. Ctrl-C quit exited rc=0.

## Finding 2: If a thread DOES suspend in the boxen REPL, it parks forever (demonstrated)

The predicted park is real and observable today via the one debug-launch surface the boxen REPL
family still has: `--debug-tui -e "<expr>"` auto-launch (`frontier-cli/boxen_repl.c:2134-2136`
-> `debug_launch_from_options`, `frontier-cli/debug_handler.c:2207` -> `debug/run` with
`start_suspended = true`).

**Observed (Experiment B):** `frontier-cli --debug-tui -e "system.temp.u13m = 12345"`:

- The spawned debug thread suspends at entry and parks in the wait loop at
  `frontier-cli/headless_spawn.c:151-169` (same suspend-wait structure as the breakpoint wait
  loop at `frontier-cli/debug_handler.c:1193-1211`).
- The `debug/suspended` notification is silently dropped: the boxen REPL's launch transport
  write callback is the C.0 no-op stub `boxen_repl_noop_write_line`
  (`frontier-cli/boxen_repl.c:1922-1926`). Nothing renders the suspension, and the REPL has no
  surface that sends `debug/continue`.
- Evidence the expression never executed: reading `system.temp.u13m` errors with "the name u13m
  hasn't been defined", and `sizeOf(system.compiler.threads)` is 1 -- the parked "debug" thread
  registered at `frontier-cli/headless_spawn.c:132-133` is still alive minutes into the session.
- Quit is still clean: Ctrl-C exits rc=0 because teardown clears the attach transport and kills
  and joins debug threads (`frontier-cli/boxen_repl.c:2240-2254`).

So: with the DEFAULT REPL a breakpoint cannot be armed at all (Finding 1); the only armable
suspension (entry suspension via `--debug-tui -e`/script auto-launch) parks the script forever
with no user-visible indication and no resume path (Finding 2). Both are C.0-known limitations
("C.1 will wire the notification -> scrollback path", `frontier-cli/boxen_repl.c:1917-1926`) --
recorded here as verified runtime behavior, not as new bugs.

## Finding 3: Guest-database breakpoint matching works (risk retired)

All pre-existing breakpoint tests target `system.temp.*` scripts in the system root. For guest
databases, `debug_push_sourcecode` computes the match key via `langexternalgetfullpath`
(`frontier-cli/debug_handler.c:579`), which for a table in a different database than the system
root takes the `filewindowtable` search branch (`Common/source/langexternal.c:2077-2084`) -- a
previously untested path for the debugger.

New Test 23 in `tests/debug_protocol_test.sh` ("guest-db breakpoint matching"): stages
`databases/StartupTasks.root` alongside the staged Virgin.root, opens it with
`fileMenu.open(<staged path>, true)`, installs `StartupTasksSuite.u13GuestBp` in-memory inside
the guest db, sets a breakpoint on `"StartupTasksSuite.u13GuestBp"` line 1, dispatches via
`thread.callScript`, and asserts suspend + script identity + line + resume-to-completion.

**Result: PASSED on first run** (suite went 81 -> 89 passed, 0 failed). The guest-db risk is
retired: `langexternalgetfullpath` renders the guest script's path in exactly the dotted form a
protocol client would use (`StartupTasksSuite.u13GuestBp`, no file prefix -- the
`flincludeself=false` branch at `Common/source/langexternal.c:2080` excludes the file window
name), so breakpoint string matching succeeds across databases.

Sensitivity falsification (not committed): the same sequence with a deliberately wrong
breakpoint path (`StartupTasksSuite.wrongName`) produces NO suspension -- the test fails by
timeout when matching is broken, so the green result is meaningful.

## Appendix: Pre-existing unit-suite crashes observed during verification (NOT unit 1.3 scope)

The full unit suite (`./tools/run_headless_tests.sh`) could not complete green in this
verification environment for reasons present at base develop (`3623bf238`) -- the unit 1.3
worktree diff contains no C changes (one test shell script + this document), so neither issue
is introduced by this unit. Evidence captured for follow-up filing:

1. **`tests/boxen_outline_tests` deterministic SIGSEGV at the default 8 MB stack limit.**
   Crash in `___chkstk_darwin` at `test_checkbox_attribute_renders_box + 32`
   (`tests/boxen_outline_tests.c:518`): `boxen_outline_state_t cs;` at line 524 is a stack
   local, and the struct is ~8.9 MB (`nodes[BOXEN_OUTLINE_MAX_NODES]` with
   `BOXEN_OUTLINE_MAX_NODES 16384` at ~545 bytes/node --
   `frontier-cli/boxen_outline_internal.h:44-48, 225`; cap bumped by commit `ced07c37a`).
   With `ulimit -s 8176` (macOS default) the frame exceeds the stack; with
   `ulimit -s 65520` the binary passes 3/3. Environments whose shells raise the stack soft
   limit never see this.
2. **`tests/test_callback_infrastructure` and `tests/test_window_bridge` nondeterministic
   SIGSEGV, identical signature.** Crash reports show
   `dbgeteof <- dbfindblockforaddress <- dbnormalizeaddress <- dbrefhandle <-
   hashresolvevalue` with `KERN_INVALID_ADDRESS at 0x0` -- a hash lookup on a purely
   in-memory table takes the on-disk resolution path against a NULL database.
   `test_callback_infrastructure` opens no database file at all (`test_setup`,
   `tests/test_callback_infrastructure.c:91-135`), so the on-disk flag on the node is
   spurious. Observed failure rates varied between ~30% and ~80% per invocation across the
   session (address-layout / load dependent). Both binaries pass on retry; all other 65
   present unit test binaries pass deterministically.

Integration suite for reference: 2414 total, 2186 passed, 207 skipped, 21 failed -- the 21
are exactly the documented baseline families (html directives/glossary, tcp streams/listeners,
startup script), zero new failures. The remaining `test-integration` sub-suites
(`verify_odb_sync_test.sh`, `ut_sync_lifecycle_test.sh`,
`strings_compiler_multi_input_test.sh`, `debugger_tui_test.sh`) all pass.
