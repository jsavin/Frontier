# Phase C proposal -- Script editor with debug split, driven by Frontier REPL

**Status:** approved 2026-06-08 (JES + Claude).  Supersedes the persistent-input-line shape in `OVERVIEW.md`.  This document is the authoritative Phase C scope; the OVERVIEW is retained for historical context only.

---

## 1. Why this proposal supersedes the OVERVIEW

The OVERVIEW (2026-06-06) assumed the boxen REPL would be a single application surface -- persistent input line at the bottom, output pane above, debug surfaces appearing inline when a thread suspends.  After shipping B.7+B.8 (PR #756) and trying the resulting `--debug-tui` interactively, a fundamental UX problem surfaced: the modal TUI has no obvious way for a typist to *cause code to run* once inside it, because the REPL where they normally type lives outside.  The launch-entry-point machinery in B.8 (`@path`, `-e expr`, `script.ut`) technically unblocks single-script debugging but requires the user to already know exactly what they want before launching.  The empty-attach case (no positional) is even worse -- you sit in front of an empty box waiting for something else (which you have no way to trigger) to hit a breakpoint.

The OVERVIEW's resolution -- a persistent input line inside the TUI -- doesn't actually fix this, because it treats the script as a transient evaluation target rather than a document.  Legacy Frontier's model is correct: **the unit of work is a script object**, and the natural surface for working with one is an editor window that opens against that script.

This proposal restores the legacy document-oriented model while integrating the debugger as a split inside the editor.  The REPL becomes the launcher and home base, not the container.

## 2. Two top-level surfaces

### 2.1 Frontier REPL window

Always present, default focus on startup.  This is the current `repl.c` linenoise REPL, ported to boxen.  All existing slash commands (`/jump`, `/list`, `/help`, etc.) survive unchanged.  Tab completion, history, slash-menu palette, navigation state all carry over.  Two new slash commands:

```
/edit [path]      open script editor on path, edit mode, no debug session
/debug [path]     open script editor on path, spawn suspended debug thread
```

The `@` prefix is optional.  If absent, the path is resolved against `g_repl_current_table` (the REPL's current navigation context) the same way other path-taking REPL commands work.  `/jump workspace` then `/debug myScript` resolves to `@workspace.myScript`.

`/debug` does NOT accept an arbitrary expression -- only a path.  For ad-hoc debugging, the user types their setup in the REPL, sets a breakpoint in the target script (F9 in an editor window, or persistent breakpoints via `db.setBreakpoint`), and calls the script from the REPL with their arguments.  The bundle-at-bottom convention in legacy Frontier is the existing answer for "I want a self-contained debug invocation" and the new model preserves it.

### 2.2 Script editor window

Opened on demand by `/edit` or `/debug`.  Each script gets its own editor; multiple editor windows can coexist.  Each window has one of two modes:

**Edit mode** (default after `/edit`, after debug ends via Esc or Q):
- Source pane fills the window; full read/write line editing
- Enter: commit + compile + run (no suspension); errors annotate the offending line
- Option-Enter: commit + compile + spawn suspended -> transitions to debug mode
- Cmd-S: explicit save to ODB
- `:` does nothing here (it's a debug-frame thing; see below)

**Debug mode** (after Option-Enter or `/debug`):
- Source pane is read-only and displays the source of whichever frame is currently selected in the stack pane
- Stack pane appears as a split (default: right side, ~30% width); shows the call chain and locals for the selected frame
- F5 / F9 / Shift-F9 / F10 / F11 / Shift-F11: stepping, breakpoints, conditional breakpoints, etc. (Phase B keybinds)
- `:` activates the scratch-eval pane (B.7 behavior, against the suspended frame)
- Esc: end debug, **stay on the currently-displayed script**, transition to edit mode (see §3 for rationale)
- Q: end debug, return to the script you originally opened, transition to edit mode

The window's "current script" identity reflects whatever it's currently displaying.  Title bar shows that script's path.  Saves write to whichever script is current.

## 3. Step-into behavior and the Esc/Q distinction

When you step into a verb defined in a different ODB script, the editor's source pane *swaps* to display that script's source -- the editor follows the selected stack frame.  This is "Option C with a twist" from the design conversation: one editor window per debug session, source follows frame selection.  Arrow keys in the stack pane navigate the existing frames (without moving the program counter); F11 / F10 etc. advance the program counter.

When you end debug, the editor stays on whichever script was being displayed.  This is JES's twist: it preserves the natural "I just spotted a bug -- let me fix it right here" workflow.  You step in twice, notice the bug in the third-level function, hit Esc, and the editor lets you fix the bug *in that script* without navigating back to find it.

The Q key is the "I'm done debugging entirely, take me back to where I started" alternative.  Same teardown (stack pane closes, debug thread terminates, drain-before-free contract per PR #722), but the source pane swaps back to the script you originally `/debug`'d.

For inspecting a frame without affecting the program counter, the stack pane provides the `o` keybind ("open"): opens the currently-selected frame's source in a *new* editor window, independent of the debug session.  Closing that new window does not affect the debug session.  This satisfies the "I want to see two frames at once" use case without making it the default.

## 4. Multiple editor windows

You can have as many editor windows open as you want.  Each one is independently in edit mode or debug mode.  PR #722's lazy-attach + drain-before-free contract was designed precisely so multiple debug threads can coexist; the boxen window manager already handles per-window focus and input routing.

**Concurrency note:** the GIL still serializes all UserTalk execution across editor windows.  "Multiple debug sessions" means "multiple threads can be paused at the same time" -- but only one thread's UserTalk code actually runs at any instant.  This matches the runtime's existing model; no Phase C changes needed there.  Stepping in window A while window B's thread is suspended is fine.  Stepping in both windows simultaneously is fine if they're at different suspension points.

## 5. Edit / compile / run cycle

### 5.1 Auto-compile on run

Enter (run) and Option-Enter (debug) both commit the in-memory edit buffer to the ODB *and* invoke `langcompiletext` before spawning anything.  If the compile fails:

- No thread is spawned
- The editor stays editable
- The error is annotated on the offending line in the source pane
- Status line shows the error message

The user fixes the error and hits Enter or Option-Enter again.  No silent fallback; no running stale code from before the failed compile.

### 5.2 Save semantics

Edits accumulate in an in-memory buffer.  ODB is updated only at:

- Explicit save (Cmd-S equivalent)
- Compile (because compile reads from ODB), which happens on Enter / Option-Enter / `/debug`
- Editor window close (with "save changes? [y/n/cancel]" prompt if buffer is dirty)

This matches the explicit-commit pattern used elsewhere in Frontier (e.g., `repl.syncscan()`).  No surprise auto-save on every keystroke.

### 5.3 Concurrent edit detection

When the editor first loads a script, it records the script's `timeModified` (the per-value modification timestamp that already exists on every script-external; see `reference_per_script_timemodified.md`).

Before any save or compile, the editor re-reads `timeModified` and compares.  If it differs from the recorded value:

```
"@workspace.foo was modified outside this editor since you opened it.

Reload? Your unsaved edits will be lost. [y/n]"
```

Y: reload from ODB, discard in-memory buffer, recorded `timeModified` updates.  N: keep editing.  Note: if the user answers N and then saves, their save overwrites the external change.  That's their explicit choice; the warning made the trade-off clear.

This covers both inter-session conflicts (another `frontier-cli` process) and intra-session conflicts (another editor window in the same session).

## 6. What from Phase B carries forward

Nothing is wasted from B.7 / B.8 / B.6:

| B artifact | Phase C role |
|------------|--------------|
| `tui_dispatch_debug_run`, `tui_launch_startup_script` | C.2 launch path (now triggered by editor's Option-Enter and `/debug`, not by CLI flag) |
| `tui_real_odb_fetch`, `debug_get_script_source` (shared with protocol) | C.2 source loading for the editor |
| `tui_is_bare_odb_address` | Detection in `/debug` and `/edit` path resolution |
| B.7 scratch-eval pane | Editor's `:` keybind, unchanged behavior |
| Phase A boxen layout primitives | Editor window construction; multi-window coordination |
| Stack pane + locals pane draw + input callbacks | Editor's debug-mode split |
| F-key state machine (B.3, B.4, B.5) | Editor's debug-mode controls |
| Persistent breakpoints in scriptType externals | Already works; exercised by C.5 |
| Drain-before-free contract (PR #722, B.6) | Multi-window debug session teardown |

`debugger_tui.c` becomes the seed of the script editor window's implementation, not deleted.  The `debugger_tui_main` event loop pattern (GIL release across `boxen_poll_event`, reacquire before state mutation, snapshot/restore of hglobals) is the canonical event-loop shape for every Phase C window.

## 7. Milestones

| Milestone | Scope | Notes |
|-----------|-------|-------|
| C.0 | Boxen-native Frontier REPL skeleton | Port `repl.c` event loop to boxen.  Input line + scrollback pane + slash dispatch.  Linenoise history file format preserved.  Tab completion works.  Slash-menu palette becomes a boxen modal.  **No editor yet.**  During this milestone, `--debug-tui` is reduced to a no-op alias (opens the REPL and immediately runs `/debug` on the positional argument if present) so users of B.8's launch flags aren't broken. |
| C.1 | Script editor window -- edit mode only | `/edit [path]` opens an editor; Cmd-S saves; Enter compiles + runs; concurrent edit detection.  No debug split yet; running just executes and the editor stays editable.  Multiple editor windows OK. |
| C.2 | Debug split on the editor | Add stack pane + locals pane that appear when Option-Enter or `/debug` triggers a suspended thread.  Replumb B.8's launch path so it goes through the editor's compile-on-run path.  Esc and Q transitions per §3. |
| C.3 | Step-into frame-following + `o` side-open keybind | Source pane swaps with selected frame.  Stack pane's `o` opens the selected frame in a new independent editor window. |
| C.4 | Multi-editor coordination polish | Focus model, raise/lower, close-all-debug, window list / switcher.  Stress-test multiple simultaneous debug sessions to validate the drain-before-free contract under load. |
| C.5 | Saved-breakpoint persistence end-to-end test | Set breakpoint in editor, save, restart, `/debug @same.path`, breakpoint fires. |
| C.6 | `--debug-tui` flag removal + `--plain` semantics | Remove the C.0 alias.  Lock down `--plain` for CI/pipe modes per OVERVIEW Q1. |

C.0 is the largest by LOC.  C.1 introduces the editor model.  C.2-C.3 are the design-iteration-heaviest milestones.  C.4 is the multi-window stress test.

The work sequences strictly: C.0 -> C.1 -> C.2 -> ...; no parallel branches.  This is JES's answer to "do we parallelize C.0 with editor work?"  Reason: the editor needs the REPL's slash dispatch and the boxen layout patterns will settle during C.0; doing them in parallel courts API churn.

## 8. What goes away

- `--debug-tui` flag (after C.6).  The boxen REPL is the default interactive mode.  Non-interactive CI keeps `--plain` per OVERVIEW Q1.
- The standalone `debugger_tui_main` entry point (after C.2; replaced by the editor's window-level event loop).
- B.7's "this is a transient single-line eval pane" framing (C.0 makes it the permanent REPL input; the editor's `:` is a different surface that reuses the same code).
- The `--debug-tui SCRIPT` / `-e EXPR` / `--debug-tui FILE` positional argument forms (deprecated in C.0 alias; removed in C.6).  Replaced by `/edit` and `/debug` slash commands.
- The empty-attach UX (no positional arg, just `--debug-tui`).  Goes away entirely; there's never a state where you have an empty debugger window with nothing loaded.

## 9. Risks

| Risk | Mitigation |
|------|------------|
| Multi-window boxen coordination is new surface | C.0 ships REPL only; C.4 is the explicit "make sure multi-window works" milestone with stress tests |
| Esc-vs-Q distinction is subtle UX -- users might not internalize the difference | Status line on debug-end shows "Esc: stay here / Q: return to @launch.script" hint when a debug session begins.  Footer keybind line shows both options. |
| Concurrent-edit warning false positives during the same editor's save-then-keep-editing flow | The recorded `timeModified` updates after each save; only warns on EXTERNAL changes.  Test in C.1. |
| Stepping into a different script during debug is jarring if user expected the source to stay put | The default IS swap (matches debugger conventions).  Document clearly.  The `o` side-open keybind is the escape hatch for users who want stable side-by-side views. |
| `/debug` with ambiguous relative path silently resolves to an unintended script | C.1 status line preview: when typing `/debug foo`, the REPL shows the resolved absolute path inline before the user hits Enter.  Reuses existing path-completion infrastructure. |
| Saving the stepped-into script after Esc could surprise the user ("I thought I was editing the original!") | The window title bar tracks the current script.  After Esc, title clearly shows the deep-step-into script.  If users still get confused, C.3 could add a brief flash/highlight on title change at Esc. |
| Reverting B.8's launch positional args removes a public surface that's been merged | Merged under 24h ago; near-zero usage.  Deprecate in C.0 alias, remove in C.6.  Document the migration to `/debug` in the deprecation message. |

## 10. Out of scope for Phase C

Documenting now to keep scope honest:

- Outline editor for non-script ODB contents (planning/boxen/OVERVIEW.md surface inventory)
- Table editor
- Menu editor
- WPText / XML editors
- `msg()` routing through the boxen output pane (OVERVIEW Q6; the existing stdout fallback works for Phase C)
- Multi-line input in the REPL (OVERVIEW Q7; line continuations still work today)
- Resurrection of any UI for the legacy menubar concept

These remain on the broader roadmap; Phase C does not block them but does not deliver them.

## 11. References

- `planning/phase_c/OVERVIEW.md` -- the prior shape (preserved for context)
- `planning/phase_b/EXECUTION_PLAN.md` -- Phase B execution that this builds on
- `docs/TUI_DEBUGGER_GUIDE.md` -- current user-facing debugger doc (will be revised when C.2 lands)
- `docs/TUI_DEBUGGER_ARCHITECTURE.md` -- current architecture doc (will be expanded for multi-window in C.4)
- `reference_per_script_timemodified.md` (memory) -- per-script `timeModified` used by §5.3 concurrent-edit detection
- PR #722 -- lazy-attach + drain-before-free; the contract Phase C's multi-window debug relies on
- PR #756 -- B.8 launch entry points; the recent prior art that motivated this proposal
