# boxen Phase C -- boxen-native REPL with debugger as one surface

> **SUPERSEDED 2026-06-08.**  After shipping Phase B (#691, B.0-B.8 via PR #722,
> #748, #749, #751, #752, #756) and interactive testing, the persistent-input-line
> shape proposed here was found inadequate -- the debugger TUI as a separate mode
> has no way for a typist to cause code to run.  The replacement design is in
> `PROPOSAL_REVISED.md`: a Frontier REPL window plus separate script editor
> windows, with the debugger as a split inside each editor.  See that document
> for the authoritative Phase C plan.  This OVERVIEW is retained for historical
> context.

Status: 2026-06-06 (original draft).  Phase B was completed 2026-06-07.  Phase C
shape revised 2026-06-08; see `PROPOSAL_REVISED.md`.

---

## 0. Why this document exists

During Phase B planning, a UX gap surfaced: while the debugger TUI owns the
terminal in TUI mode, the user cannot type UserTalk expressions to inspect
variables or try fixes. Phase B closes the gap with a minimal scratch-eval pane
(milestone B.7) -- a single-line input inside the TUI, no history persistence,
no slash commands, no completion. That is the smallest viable fix.

Phase C is the correct long-term answer: a boxen-native REPL where the debugger
TUI is one of its surfaces rather than a separate mode. B.7 is the stepping stone.

This document explains the vision, why it was deferred, what it changes about
Frontier's interactive experience, and the open questions that Phase B experience
will answer before the Phase C execution plan is written.

**Out of scope for this document**: execution plan, milestone breakdown, timeline.
Those come after Phase B is complete and we have real experience with boxen as a
consumer. See the Phase B and boxen execution plans for implementation precedent.

---

## 1. Motivation

### 1.1 The modal problem

Phase B ships a modal debugger TUI. The user enters `--debug-tui` mode, termbox2
owns the terminal in raw mode, and linenoise cannot read stdin. The scratch-eval
pane (B.7) partially addresses this: the user can evaluate expressions while
suspended. But the pane is deliberately minimal -- no history persistence, no
slash commands, no autocomplete, no navigation. Outside of suspension, the pane
is hidden. The user who wants to run a UserTalk verb to set up test state before
triggering a breakpoint has no way to do that from inside the TUI.

The root issue is that `--debug-tui` is a separate mode from the REPL, not a
surface inside it. Two separate input models (linenoise outside, boxen inside) with
a hard modal boundary between them.

### 1.2 The unified surface model

Phase C collapses that boundary. The REPL becomes a boxen application. The
interactive session looks like this:

- A persistent input line at the bottom of the screen, owned by boxen, with
  readline-style editing, history, and tab completion.
- The current "pane set" above it changes with context: idle state shows a
  welcome/status surface; a UserTalk script running in the foreground shows a
  scrollable output pane; debug mode shows the script pane + stack pane from Phase B.
- The user can always type at the input line regardless of what pane is showing.
  Slash commands work. Expression evaluation works. Debugger commands work when
  relevant.

The analogy: the debugger TUI in Phase B is like `gdb --tui` -- a separate binary
mode. Phase C is like VS Code's integrated terminal: the editor surfaces surround a
persistent input line that is always available.

### 1.3 Relationship to issue #691

Issue #691's original framing included "bare-interactive-REPL debugging without a
protocol/WS client." The Phase B answer is `--debug-tui` mode. Phase C is the
definitive answer: the REPL itself is always in TUI mode; debug surfaces appear when
a thread suspends and disappear when it resumes.

Task #24 -- "Decide: bare-linenoise debug transport vs attach-a-client workflow" --
is subsumed by Phase C. The decision is: boxen-native REPL with inline debug surfaces
is the bare-interactive-REPL debug story. No separate transport decision needed.

---

## 2. What changes about Frontier when Phase C ships

### 2.1 Interactive entry point

Today: `frontier-cli` launches a linenoise REPL. `frontier-cli --debug-tui` launches
a separate TUI mode.

After Phase C: `frontier-cli` launches a boxen REPL. There is no `--debug-tui` flag;
the debugger surfaces appear automatically when a thread suspends (if a breakpoint is
set), and the full debugger controls are available via keybinds or slash commands.

The `--plain` or `--no-tui` flag (see Section 5, Open Questions) allows non-interactive
and CI use cases to bypass boxen entirely and get the current linenoise behavior or
pure pipe mode.

### 2.2 The linenoise REPL

`repl.c` is today's implementation: 4197 lines covering the event loop (linenoise +
`poll()`), slash command dispatch (`dispatch_slash_command`, ~2400 lines in), history
(`HISTORY_FILE .frontier_history`), tab completion (`linenoise_completion_callback`),
the slash-menu palette (termbox2-based, launched from the event loop), navigation
state (`g_repl_current_table`, `g_repl_current_path`), and guest database state.

Phase C replaces linenoise with a boxen-native input line that provides the same
surface area:

- **History**: persistent across sessions (`.frontier_history` equivalent), scrollable
  in-session. The storage mechanism (file-backed, linenoise-compatible format) can be
  reused.
- **Editing**: Ctrl-A/E, arrow keys, Ctrl-K, Ctrl-U, Tab completion. Linenoise today
  provides these; the boxen input line must match them.
- **Slash commands**: `dispatch_slash_command` and `repl_slash_commands[]` are
  independent of the input layer. They survive the move to boxen-native input.
- **Slash-menu palette**: today's implementation is already termbox2-based (launched
  from the event loop on `/` keypress with a 250ms delay). In Phase C it becomes a
  boxen surface rather than a raw termbox2 surface, and the 250ms delay and modal
  behavior are implemented via boxen's modal API.
- **Navigation state**: `g_repl_current_table`, `g_repl_current_path`,
  `g_repl_guest_db_state` are independent of the input layer and carry over unchanged.

### 2.3 The B.7 scratch-eval pane becomes the REPL input line

The B.7 scratch-eval pane is explicitly designed as a stepping stone. In Phase C:

- The B.7 pane -- single-line input, evaluates `script/eval`, appends to a ring buffer
  -- becomes the permanent REPL input line.
- The ring-buffer output becomes the scrollable output pane that is always visible.
- History persistence (missing from B.7 by design) is added.
- Tab completion is added.
- The modal debug surfaces (script pane + stack pane) appear above this input line
  when a thread suspends.

The transition is additive. Nothing in B.7's implementation is discarded; it grows
into the full REPL. The modal TUI mode (`--debug-tui`) goes away.

### 2.4 The `--debug-tui` flag

The `--debug-tui` flag introduced in B.0 is removed (or deprecated to a no-op) in
Phase C. The TUI is not a mode; it is the default interactive experience. Scripts and
CI use `--plain` (or equivalent; see Section 5) to bypass it.

---

## 3. Architecture sketch

```
+---------------------------------------------------------------+
|  output pane (scrollable)                                     |
|  - idle: status / welcome                                     |
|  - script running: stdout/msg() output                        |
|  - debug suspended: script pane (left) + stack pane (right)  |
|    -- these are Phase B's panes, unchanged                    |
+---------------------------------------------------------------+
|  eval/REPL input line (always visible)                        |
|  - boxen-native, not linenoise                                |
|  - history, completion, slash dispatch                        |
+---------------------------------------------------------------+
|  keybind footer (pinned)                                      |
|  - context-sensitive: idle / running / suspended             |
+---------------------------------------------------------------+
```

Three layers, always present. The top layer changes content with context; the input
line and footer are stable. Boxen's `boxen_window_set_pinned` on the footer and input
line ensures they stay anchored on terminal resize.

The architecture maps naturally onto boxen's existing window manager. The output pane
and the debug surfaces (script pane + stack pane) are ordinary boxen windows with
different draw callbacks registered at different times. The input line is a
permanently-open window at the bottom edge.

The main loop structure mirrors Phase B's `debugger_tui_main` event loop exactly:
`boxen_poll_event` + `boxen_dispatch_event` + `boxen_present`, GIL-released around
each poll. Phase C replaces `debugger_tui_main` and `repl_main` with a single
`boxen_repl_main` entry point.

---

## 4. Relationship to Phase B and the boxen surface roadmap

Phase B establishes:
- The event loop pattern (`boxen_poll_event` + GIL discipline)
- The debug surface (script pane + stack pane) that Phase C reuses unchanged
- The B.7 scratch-eval pane that Phase C grows into the full REPL input line
- The `boxen_window_set_cursor` follow-up need (identified in B.7; Phase C requires it)

Phase C is the first phase where boxen has to manage multiple surfaces simultaneously
(output pane, debug panes, input line, footer -- all active at once). Phase B's two-pane
layout is fixed; Phase C's layout is dynamic (output pane resizes when debug panes
appear). This is the primary new boxen API pressure point.

The broader surface roadmap from `planning/boxen/OVERVIEW.md` Section 2 lists:

> table editor, outline editor, WPText editor, XML editor, menu editor,
> status-bar surface for msg()

Phase C's REPL is the container that all of these eventually appear inside. The
"status-bar surface for msg()" mentioned in the surface inventory is a direct
consequence of Phase C: `msg()` output appears in the output pane rather than on a
separate "About"-style surface.

Phase C is positioned between Phase B (first real consumer, fixed layout) and Phase D
(backend interface freeze). The API pressure from Phase C -- dynamic layout, multiple
simultaneous surfaces, persistent input line -- must be absorbed before the backend
interface is frozen.

---

## 5. Open questions

These are open questions to flag for Phase C planning, not to answer here. Phase B
experience will inform the answers.

**Q1: `--plain` / `--no-tui` flag semantics.**
CI scripts, piped input, and `--script` mode cannot use boxen (stdin is not a
terminal; no terminal geometry available). A `--plain` flag (or auto-detection via
`isatty(STDIN_FILENO)`) must bypass boxen and fall back to the existing linenoise REPL
or pure pipe mode. The boundary between "boxen REPL" and "pipe mode" must be defined
precisely: does `--plain` keep slash commands? Does it keep history? What happens if
the user starts with `--plain` and a script triggers a breakpoint?

**Q2: Slash command survival.**
`dispatch_slash_command` and `repl_slash_commands[]` in `repl.c` are independent of
the input layer. They should survive the move to boxen-native input unchanged.
Confirm: does the slash-menu palette (today's termbox2-based overlay) become a boxen
modal window in Phase C? The answer is almost certainly yes -- the palette is already
`boxen`-adjacent (it uses termbox2 directly). Phase C's natural refactor is to convert
it to a `boxen_window_set_modal` surface.

**Q3: History persistence.**
Linenoise today writes `.frontier_history` to `$HOME`. The boxen input line needs
equivalent persistence. Options: (a) reuse linenoise's history file format so existing
history survives the Phase C migration, (b) define a new format. The linenoise format
is simple enough that reuse is likely correct.

**Q4: Tab completion.**
`linenoise_completion_callback` in `repl.c` wires linenoise's completion API to
Frontier's ODB path resolution and slash command completion. Phase C needs equivalent
behavior from the boxen input line. The completion logic itself is independent of the
input layer (it calls into `repl_resolve_path_ex` and `complete_slash_command_path`);
the integration point is the Tab keypress handler in the boxen input line's input
callback. No new completion logic needed -- only a new integration point.

**Q5: Menu integration.**
Frontier has a legacy menubar concept (slash-menu palette, menu editor). Phase C's
boxen REPL is the surface that the menu editor eventually lives inside. Does the
slash-menu palette become a persistent boxen surface or remain an on-demand modal?
This is a UX decision that Phase B's slash-menu behavior (250ms delay, modal overlay)
informs but does not determine.

**Q6: `msg()` and output routing.**
Today `msg()` writes to stdout in protocol mode and to the terminal directly in REPL
mode. In Phase C, `msg()` output routes to the boxen output pane. This is the
"status-bar surface for msg()" from the surface inventory, collapsed into the output
pane rather than a separate "About"-style window. The routing change requires updating
`msg()` (or its C implementation) to call a boxen output pane write function instead
of `fprintf(stdout, ...)`. This is the only kernel change Phase C requires; everything
else is in the REPL layer.

**Q7: Multi-line input.**
The B.7 pane and the initial Phase C REPL input are single-line. Frontier's REPL
today evaluates complete UserTalk expressions on Enter; multi-line expressions
require the user to write them as inline continuations (the `\` line continuation
convention in UserTalk). Phase C does not need to change this contract. But if the
boxen input line eventually supports multi-line input (for writing inline scripts
without escaping), that is a scope expansion that should be a separate milestone
and a separate boxen API addition.

---

## 6. Why Phase C is NOT Phase B

The scope difference is not about features; it is about scope and sequencing.

**Phase B** is 6-7 milestones targeting one well-bounded deliverable (the debugger
TUI, issue #691) with a fixed layout and a clearly scoped set of debug operations.
The boxen API is consumed; it is not designed in Phase B.

**Phase C** requires:
- Designing the dynamic layout system (output pane resizes when debug surfaces appear)
- Migrating the REPL entry point from linenoise to boxen -- touching `repl.c` (4197
  lines) and all its dependencies
- Implementing persistent history and tab completion in the boxen input line
- Converting the slash-menu palette from raw termbox2 to a boxen modal surface
- Routing `msg()` output through the boxen output pane
- Defining and implementing `--plain` / `--no-tui` for non-interactive use

This is at minimum a 4-6 week project with more boxen API surface pressure than
Phase B. Deferring it lets Phase B find and fix boxen's API mistakes first. The
B.7 scratch-eval pane is the Phase C prototype -- it proves the boxen input line
pattern works before committing to the full REPL migration.

Additionally, shipping the Phase B debugger TUI (even in modal form) provides
immediate value (closes #691, enables debugging of the UserTalk subsystem that is
actively growing). Phase C's value is additive on top of that.

---

## 7. Risks

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| Dynamic layout adds unexpected complexity to boxen core | Medium | Medium | Phase B's fixed layout exposes the non-dynamic case first; Phase C adds dynamism incrementally. If the boxen API needs significant extension for dynamic layout, that is a Phase C boxen PR before the REPL migration. |
| Linenoise history / completion behavior is hard to replicate in boxen | Medium | Low-medium | The completion and history LOGIC is independent of the input layer. Only the integration points change. Regression tests for completion behavior should be added in Phase B (they test the logic, not the input layer). |
| `--plain` mode carve-out is underspecified | High | Low | This is an acknowledged open question (Q1). Phase B's `--debug-tui` flag is the precedent for "mode flags that bypass the TUI." Phase C's `--plain` is the inverse. The pattern exists; the exact semantics need decision. |
| `msg()` routing change affects existing behavior | Medium | Medium | `msg()` has test coverage. The routing change can be feature-flagged behind boxen init state (if boxen is not initialized, fall back to stdout). No test breakage expected. |
| Phase C never starts because Phase B is "good enough" | Low | Low | The B.7 scratch-eval pane is a deliberate Phase C prototype that creates pressure to finish the migration. If B.7 ships and users are satisfied with the modal TUI, Phase C can wait longer without blocking anything. |
