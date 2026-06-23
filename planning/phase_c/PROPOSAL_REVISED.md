# Phase C proposal -- Script editor with debug split, driven by Frontier REPL

**Status:** approved 2026-06-08 (JES + Claude).  Supersedes the persistent-input-line shape in `OVERVIEW.md`.  This document is the authoritative Phase C scope; the OVERVIEW is retained for historical context only.

**Delivered milestones (2026-06-09):**
- **C.0** -- PR #758 (`16a4e9bac`) -- boxen-native REPL skeleton via `--debug-tui`
- **C.1** -- PR #759 (`2c7929846`) -- outline editor MVP (read-only viewer + run)

Direction shift during C.1 planning: the original "script editor with debug split" scope was inverted to "outline editor first; script editor reuses outline editor + adds script-specific extensions in C.2".  The outline editor is general-purpose (works on any outline, not just scripts) and the script editor in C.2 layers debug/breakpoint UI on top.

**Metadata storage decision (2026-06-09):** C.1 ships breakpoint/comment toggles via the legacy `(**hnode).flbreakpoint` and `(**hnode).flcomment` bit-flag fields on `tyheadrecord`, NOT via the refcon-based attributes API.  Reasons: these flags are the canonical persistent storage today (packed in v7 outline format bits 0x0200 and 0x0400; read by every existing path: execution, rendering, export, pack/unpack).  The refcon-based attribute table (per OUTLINE_EDITOR.md) is for arbitrary freeform metadata (`checkbox`, `headline`, `time`, `author`, user-defined keys), not for migrating these two specific fields.  Migration to refcon-based storage is a separate effort that touches every reader; C.1 explicitly does not start it as a side effect.  Future renderer should also check refcon attributes like `checkbox` for visual treatment, which is a small addition that does not require migrating existing storage.

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
- Cmd-S: triggers root-level save of all modified objects in the root owning this editor's script (see §5.2)
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

Legacy Frontier's Cmd-S is **not a per-window save** -- it saves all modified objects in the root that owns the frontmost window (or Frontier.root if no window has focus).  This convention is overridable per windowType / Tool at the UserTalk level.  Phase C preserves this semantic; the editor does not own its own save behavior, it participates in the root-level save model.

Two distinct concepts are required, only one of which is per-editor:

- **Commit-to-ODB** (per-editor): the editor flushes its in-memory edit buffer to the in-memory ODB representation, marking the script-external as modified.  This happens automatically on Enter / Option-Enter / `/debug` (because compile reads from ODB), and on editor window close (with a "discard changes? [y/n/cancel]" prompt if the buffer is dirty -- the prompt is about whether to commit, not whether to persist).
- **Persist-to-disk** (root-level): the standard root-save flow walks all modified objects in the owning root and writes them.  Cmd-S is the keybind that triggers this for the root owning the frontmost window.  This includes any committed-but-not-yet-persisted editor buffers from any editor window backed by that root.

The result: Cmd-S in any editor (or in the REPL) saves *every* modified object in the relevant root, exactly as legacy Frontier does.  An editor backed by a guest .root saves its guest root; an editor backed by Frontier.root saves Frontier.root.  No surprise; no per-editor save key needed.

In-memory buffer commit is implicit at the run / debug / close points.  No surprise auto-commit on every keystroke.  Explicit "commit without saving to disk" is unnecessary -- the next compile/run/close handles it automatically and Cmd-S handles the persistence.

### 5.3 Concurrent edit detection

When the editor first loads a script, it records the script's `timeModified` (the per-value modification timestamp that already exists on every script-external; see `reference_per_script_timemodified.md`).

Before any commit-to-ODB (Enter / Option-Enter / `/debug` / window close), the editor re-reads `timeModified` and compares.  If it differs from the recorded value:

```
"@workspace.foo was modified outside this editor since you opened it.

Reload? Your unsaved edits will be lost. [y/n]"
```

Y: reload from ODB, discard in-memory buffer, recorded `timeModified` updates.  N: keep editing.  If the user answers N and then commits, their commit overwrites the external change.  That's their explicit choice; the warning made the trade-off clear.

The detection runs at *commit* time, not at *persist* time.  Cmd-S (root-level save) doesn't trigger per-editor conflict checks -- by the time Cmd-S fires, every editor's commit has already happened (or the in-memory buffer is still pending, in which case Cmd-S simply doesn't include it).  This is the right layering: detection belongs to the editor, persistence belongs to the root.

Coverage: both inter-session conflicts (another `frontier-cli` process modified the .root then exited, and we reloaded) and intra-session conflicts (another editor window in the same session committed changes to the same script-external).

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
| C.0 | Boxen-native Frontier REPL skeleton | **Shipped 2026-06-08 in PR #758 (`16a4e9bac`).**  Tracer-bullet that reused the C.0 vision with explicit deferrals: input line + scrollback + slash dispatch via `--debug-tui` opt-in; auto-launch path from B.8 preserved; stdout/stderr captured via pipe + dup2 + non-blocking drain into scrollback; heap-allocated state with drain-before-free kill+join teardown.  All C.0.x follow-up sub-milestones complete (see C.0.x rows below); the boxen REPL is now at parity with the linenoise REPL.  Default flip awaits C.6. |
| C.0.1 | Persistent history (`.frontier_history`) | **Shipped.** UP/DOWN navigation, merge-before-save strategy matching linenoise. |
| C.0.2 | Tab completion (slash command args + ODB paths) | **Shipped.** `boxen_repl_real_completion` mirrors the linenoise callback; popup rendering via `boxen_completion_popup.c`. |
| C.0.3 | Slash-menu palette as boxen modal | **Shipped.** `boxen_palette_backend.c` replaces the termbox2 overlay; same arg-injection + `meuserselected_headless` dispatch path. |
| C.0.4 | Async output composition | **Shipped.** Captured stdout/stderr pipe drained into scrollback at every event-loop tick; input bar is never visually disrupted. |
| C.0.5 | Multi-line input (`\` continuation) | **Shipped.** Trailing `\` appends to `multiline_buf`; prompt switches to `..`. |
| C.0.6 | Parity verification + documentation | **Shipped 2026-06-23.** [`docs/BOXEN_REPL_PARITY.md`](../../docs/BOXEN_REPL_PARITY.md) tables every linenoise feature with its boxen equivalent and status.  [`docs/CLI_USAGE_GUIDE.md`](../../docs/CLI_USAGE_GUIDE.md) documents `--debug-tui`.  This row. |
| C.0.7 | Polish + bug-fix cluster | **Shipped 2026-06-17 through 2026-06-23.**  A (palette-vs-completion race), B (TUI crashes), C/D (Ctrl-C semantics), E (cursor editing parity), F (palette source lifetime + dispatch UAF, PRs #791/#792), G (boxen ↔ UserTalk UI bridge Phase 1 = dialog modals, PR #793). |
| C.1 | Outline editor MVP -- read-only viewer + run | **Shipped 2026-06-09 in PR #759 (`2c7929846`).**  Direction was inverted from the original scope ("script editor edit mode only") to general-purpose outline editor first.  `/edit [path]` opens a boxen window with structural rendering via op verbs, bar cursor navigation, expand/collapse, F9 toggle breakpoint (legacy `flbreakpoint` field), Cmd-/ toggle comment (legacy `flcomment` field), Cmd-R run (when target is a script-external), Cmd-S root-level save dispatch.  Multi-editor registry.  No text editing yet; that lands in C.1.1.  Walks ODB via `langfastaddresstotable` + `opverbinmemory` + DFS over `headlinkdown`/`headlinkright`. |
| C.1.1 | Outline editor -- text-cursor editing within headings + Keypad-Enter mode toggle + Cmd-S commit semantics | Adds edit mode: type to modify current heading's text; Keypad-Enter toggles between bar-cursor (navigation) and text-cursor (edit) modes; the editor's in-memory buffer model + concurrent-edit detection via per-script `timeModified` per §5.3 lands here.  UserTalk `edit (@adrobject)` verb registration also lands here (deferred from C.1).  hnode_opaque UAF mitigation per #760 follow-up. |
| C.1.2 | Outline editor -- full structural editing | Enter inserts new heading at same level + enters text mode; Tab/Shift-Tab indent/outdent; Option-Left/Right word skip in text mode; Cmd-/ exec + insert result with `flComment=true` (per legacy Frontier convention). |
| C.2 | Script editor extensions (debug split) | Add stack pane + locals pane that appear when Option-Enter or `/debug` triggers a suspended thread.  Replumb B.8's launch path so it goes through the editor's compile-on-run path.  Esc and Q transitions per §3. |
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
