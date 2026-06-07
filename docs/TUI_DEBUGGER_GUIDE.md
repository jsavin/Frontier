# TUI Debugger Guide

**Audience:** anyone using `frontier-cli` to debug a UserTalk script.
**Last updated:** 2026-06-07

The TUI debugger is a full-screen terminal interface for debugging UserTalk scripts. It runs inside `frontier-cli` and uses the same NDJSON debugger protocol that drives `--protocol` mode, presented as a two-pane source/state view with a keybind footer.

For the protocol-level interface (used by IDEs and external tools), see [`docs/CLI_USAGE_GUIDE.md`](CLI_USAGE_GUIDE.md). For TUI internals (transport wiring, lazy-attach contract, teardown order), see [`docs/TUI_DEBUGGER_ARCHITECTURE.md`](TUI_DEBUGGER_ARCHITECTURE.md).

---

## Launching

```bash
./frontier-cli/frontier-cli --debug-tui
```

Requirements:

- A real TTY. The TUI opens `/dev/tty` directly via `termbox2` and cannot be piped or remapped. If you redirect stdin/stdout, initialization fails with a logged error and the binary exits non-zero.
- `--debug-tui` is mutually exclusive with `--protocol`. Combining them is a usage error caught at argument parsing.

On launch you'll see three regions:

```
+-- Script ---------------------+-- Stack / Locals ------+
|   1: on doSomething(x)        | #0  doSomething        |
|>  2:   local total = 0        | #1  caller             |
|   3:   loop (i = 1; i < x)    |                        |
|   4:     total = total + i    | -- Locals --           |
|   5:   return total           | total      0           |
|                               | i          (unset)     |
+-------------------------------+------------------------+
| F5:continue F9:bp S-F9:cond F10:step-over F11:step-in S-F11:step-out q:quit |
+-----------------------------------------------------------------------------+
```

The footer always shows the active keybind set. When the eval pane (`:`) opens, the footer text is temporarily replaced with the prompt; pressing Escape or Enter restores the keybind line.

---

## States

The TUI tracks one of two debugger states:

| State | Meaning | What's actionable |
|-------|---------|-------------------|
| **RUNNING** | The UserTalk thread is executing. No frame is suspended. | F9 (set breakpoint at future line) accepts input only when a current line is known. Step/continue keys are ignored. |
| **SUSPENDED** | A thread hit a breakpoint, step, or watchpoint and is paused. | All keybinds active. Stack and locals reflect the suspended frame. |

State transitions happen automatically as runtime messages arrive over the attached transport. You don't toggle them manually.

---

## Keybinds

### Step / continue (SUSPENDED only)

| Key | Action |
|-----|--------|
| **F5** | Continue execution until next breakpoint. |
| **F10** | Step over (next statement in current frame). |
| **F11** | Step into (descend into call). |
| **Shift-F11** | Step out (run until current frame returns). |

Pressed while RUNNING, these are silently ignored — the runtime can't accept another step until it suspends again.

### Breakpoints

| Key | Action |
|-----|--------|
| **F9** | Toggle a plain breakpoint at the current line. Requires a non-empty script path and a known current line. |
| **Shift-F9** | Open the **conditional breakpoint modal** at the current line. Type a UserTalk expression (e.g. `x > 100`); Enter installs, Escape cancels and preserves the modal text for next time. |

### Watchpoints

Watchpoints fire when a target address changes value. Trigger the watchpoint modal by **Cmd-double-clicking** a local identifier in the script pane (see "Mouse" below), or via the protocol from an external tool.

In the modal:
- The suggested path (e.g. `@user.foo.bar`) is pre-filled.
- Edit if needed; Enter installs, Escape cancels.

### Scratch eval pane

| Key | Action |
|-----|--------|
| **`:`** (SUSPENDED only) | Activates the single-line eval pane (vi-command-mode style). |
| typing | Builds the expression. |
| **Enter** | Dispatches `script/eval` against the suspended frame; result appears in the history above the input row. |
| **Escape** | Dismisses the pane (the partial expression is dropped). |

While the eval pane is active, `q` and `:` are typed into the expression — they don't trigger quit or re-activate the pane. The eval pane is suppressed while a modal (condition / watchpoint) is open.

History rendering caps the visible expression at 80 characters per entry and keeps a small ring of recent results.

### Mouse — identifier resolution

**Cmd-double-click** (Meta + left button, double-click) on a word in the script pane while SUSPENDED resolves it as an identifier:

- If the word is in the current frame's locals, its value flashes in the stack/locals pane.
- If it's a watchpoint candidate, the watchpoint modal opens pre-filled.

In the current release this is **locals-only**: full ODB navigation (descend into tables, follow addresses) is Phase C work. The gutter column is not clickable — only the source-text portion of the script pane.

### Quit

| Key | Action |
|-----|--------|
| **q** | Clean exit. Drains pending lazy-attach threads, tears down boxen, restores stdin/stdout. |
| **Escape** (no modal, no eval pane) | Same as `q`. |
| **Ctrl-C** | Same as `q` if delivered as a key event; not a SIGINT handler. |

The drain is mandatory: it waits for any thread that hit a breakpoint via the lazy-attach path to release its transport reference before the transport itself is freed. See [TUI_DEBUGGER_ARCHITECTURE.md](TUI_DEBUGGER_ARCHITECTURE.md#teardown).

---

## Workflow: debugging File > Open

The TUI is the supported way to debug menu-triggered UserTalk code that runs on `thread.callScript` threads (invisible to LLDB).

1. Launch the TUI:
   ```bash
   ./frontier-cli/frontier-cli --debug-tui
   ```
2. Wait for the source pane to show the system root (idle state — no script loaded yet).
3. From another terminal (or via your normal automation), trigger the menu action. The TUI receives the breakpoint hit via the lazy-attach transport, suspends, loads source, and highlights the current line.
4. Step or continue. Set additional breakpoints with F9. When the thread completes, the TUI returns to RUNNING.
5. Press `q` to exit.

To stop at the very first line of a known verb (e.g. `commands.open`), set the breakpoint via the protocol-level `debug/setBreakpoint` op before triggering the menu. (Setting breakpoints purely from inside the TUI when no thread is yet suspended is a Phase C convenience; not in B.)

---

## Workflow: conditional breakpoint

1. Step or continue until you're suspended at the line you want to gate.
2. Press **Shift-F9**. The condition modal opens.
3. Type a UserTalk expression that evaluates to true/false in the current scope (e.g. `i = 42`, `x > threshold`, `name = "target"`).
4. **Enter** installs. Continue (F5). The breakpoint now fires only when the condition evaluates truthy at that line.
5. Plain F9 at the same line removes the conditional breakpoint (toggle semantics).

The expression runs in the suspended frame's scope — locals are in scope, and the same identifier resolution rules apply as `script/eval`.

---

## Workflow: watchpoint on a local

1. Suspended at any line.
2. Cmd-double-click the local identifier in the script pane.
3. The watchpoint modal opens with the resolved address (e.g. `@local.total`) pre-filled.
4. **Enter** installs. Continue.
5. The TUI suspends again the next time that address changes value, with the stack at the writing frame.

---

## Limitations (Phase B scope)

These are deliberate scope cuts, not bugs:

- **No WebSocket-attached debugging.** The TUI uses the in-process lazy-attach transport. Remote/WS attach is permanently deferred — the use-after-free surface across the WS boundary is not safe to ship. Use `--protocol` over stdio for any external-tool integration.
- **Locals-only Cmd-double-click.** Full ODB navigation (descend into tables, follow `@adr` references, open address values in a viewer) lands in Phase C.
- **Single-row eval history footer.** The eval pane shows one recent result in the footer area. A scrollable history pane is Phase C work.
- **No breakpoint listing UI inside the TUI.** Set/clear works (F9, Shift-F9). To see all currently-installed breakpoints, use the protocol-level `debug/listBreakpoints` op.
- **No source-pane scroll keys.** The pane scrolls automatically to keep the suspended line visible. Manual scrolling will be added in Phase C alongside the REPL pane work.

---

## Troubleshooting

| Symptom | Cause | Fix |
|---------|-------|-----|
| "boxen_init failed" then immediate exit | Stdin/stdout not a TTY (piped or redirected). | Run from a real terminal. The TUI cannot operate without `/dev/tty`. |
| "--debug-tui and --protocol are mutually exclusive" | Both flags on the command line. | Pick one. |
| Step/continue keys ignored | State is RUNNING — no frame is suspended. | Wait for a breakpoint to fire, or set one and trigger the code path. |
| Eval pane `:` does nothing | State is RUNNING, or a modal is open. | Suspend first; close any open modal with Escape. |
| Cmd-double-click does nothing | Click landed in the gutter column, or the identifier isn't in current locals, or state is RUNNING. | Click on the source text portion (right of the line number), while suspended, on a local identifier. |
| Quit hangs briefly | Drain step waiting for a lazy-attached thread to release the transport. | Normal; should complete in milliseconds. If it hangs >5s, investigate runtime thread state via `--protocol` from a separate session. |

---

## Where to file issues

If something in the TUI doesn't behave as this guide describes, file against the `frontier-cli` component with reproduction steps. For Phase C scope items (the limitations above), see `planning/phase_c/OVERVIEW.md` — those are tracked but won't be addressed as TUI bugs.
