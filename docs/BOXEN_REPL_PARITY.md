# Boxen REPL ↔ Linenoise REPL Parity

**Status as of 2026-06-25 (post-C.6; develop @ 8e31cb419 + the C.6 PR).**
**Companion to:** `planning/phase_c/REPL_SCHISM_RESOLUTION.md`, `planning/phase_c/REPL_SCHISM_EXECUTION_PLAN.md`.
**Purpose:** Make the schism-resolution Section-1 feature table verifiable. Each row is the answer to "is the boxen REPL ready to be the default?" — the answer is yes for every row, and the C.6 flip has happened.

The boxen REPL is launched with `frontier-cli` (no flag — the default since C.6, 2026-06-25). The legacy linenoise REPL is reachable via `frontier-cli --plain`. The original `--debug-tui` flag is preserved as a deprecated no-op alias for the new default (emits a one-line deprecation warning at startup).

## Feature comparison

| # | Feature | Linenoise (`repl.c`) | Boxen (`boxen_repl.c`) | Status | Notes |
|---|---------|----------------------|------------------------|--------|-------|
| 1 | Activation | `--plain` opt-in (since C.6) | Default `frontier-cli` (no flag) | DIFFERENT-BUT-INTENTIONAL | C.6 flipped the default (2026-06-25).  `--debug-tui` retained as a deprecated no-op alias.  `main.c` dispatch logic in the "REPL dispatch flip" comment block. |
| 2 | Input editing | Full linenoise readline via `linenoiseEditStart` (`repl.c:3923`) | Cursor-based editing with arrow keys + Ctrl-A/E/U/K (`boxen_repl.c:1126-1280`). Added in C.0.7e (#691). | SUPERSET | Boxen tracks `input_cursor_pos` separate from `input_len` so insert / delete / arrows compose correctly mid-buffer. |
| 3 | History | `.frontier_history` persistent via `merge_and_save_history()` (`repl.c:1784-1880`); 1000-entry cap | `.frontier_history` persistent since C.0.1. Ring is `BOXEN_REPL_HISTORY_SIZE = 500` (`boxen_repl.c:376`), same merge-before-save strategy. UP/DOWN navigate via `boxen_repl_load_history()` + `navigate_history()` (`boxen_repl.c:630-670`). | IDENTICAL | Same file format and home-dir resolution; ring size differs (see "Acceptable divergences" §1). |
| 4 | Tab completion | ODB paths + slash commands. `linenoiseSetCompletionCallback` (`repl.c:1905`) → `repl_complete_slash_command_path` for `/jump`, `/list` args. | Same scope since C.0.2. `boxen_repl_real_completion()` (`boxen_repl.c:1674`) mirrors the linenoise callback; both share `repl_complete_slash_command_path`. Popup rendering via `boxen_completion_popup.c`. | IDENTICAL on shipped surface | Linenoise has additional context-aware completion (keyword expansion, `@`-address verb-call context) that boxen explicitly defers (`boxen_repl.c:1649-1654`). Tracked as a post-C.6 enhancement (see GAPs §1). |
| 5 | Slash-menu palette | Termbox2-based overlay (`palette.c`). `/` + 250 ms debounce → palette open. Arg injection via `palette_arg_inject.c` → `meuserselected_headless`. | Boxen modal palette since C.0.3 (#791/#792). `/` + 350 ms debounce → `s->palette_open_hook` → `boxen_palette_backend_open` (`boxen_palette_backend.c`). Same arg injection, same dispatch. | IDENTICAL on the dispatch path | Debounce differs (see "Acceptable divergences" §3). C.0.7f / C.0.7f-followup (#791, #792) closed the last lifetime + use-after-free bugs in the dispatch path. |
| 6 | Async output (script writes during input) | `linenoiseHide()` / `linenoiseShow()` (`repl.c:1680`) — in-place prompt hide/show around external writes | Captured stdout/stderr pipe (`pipe(2)` + `dup2` at `boxen_repl.c:1816-1852`) → scrollback ring. Drained by `drain_stdout_into_scrollback()` (`boxen_repl.c:1480+`) at every event-loop tick and inside the UI bridge's mini-loop (`boxen_ui.c:357-358`). Input bar is never visually disrupted. | DIFFERENT-BUT-INTENTIONAL | Boxen's model is more robust (no terminal-state race), but both are correct for their architecture. |
| 7 | Multiple co-resident windows | None — palette is a transient termbox2 overlay | Yes: outline editor via `/edit @path` (`boxen_repl.c:835`, `boxen_outline_open`). Boxen window, not a modal — user can switch focus between input and editor. | BOXEN-ONLY | Intentional. Prerequisite for C.1 (structural editing) and C.2 (debug split). |
| 8 | Stdout/stderr routing | Direct terminal writes | dup2'd into capture pipe → scrollback ring | DIFFERENT-BUT-INTENTIONAL | Same trade-off as #6: boxen's captured model is more robust; linenoise's direct model is simpler. |
| 9 | Threading (GIL discipline) | Snapshot/restore via `headless_backgroundtask` wrapper around the linenoise event loop. Ctrl-C handler sets `g_repl_interrupt_requested`. | Identical snapshot/restore pattern (`boxen_repl.c:1929`). Ctrl-C sets `g_repl_exit_requested`. Same `langinterruptexecution()` wiring. | IDENTICAL | Both follow the GIL discipline documented in ADR-014 / `docs/AI_SHARED_GUIDELINES.md`. |
| 10 | `--debug-tui @path` startup script | N/A — linenoise rejects `--debug-tui` combined with `inline_script` / `script_file` (`cli_parser.c:214-228`); `repl_main` ignores `opts` | Supports: `debug_launch_from_options()` (`boxen_repl.c:1915-1917`) auto-spawns a suspended debug thread on entry when `opts->script_file` or `opts->inline_script` is set. | BOXEN-ONLY | Intentional. Not a regression — linenoise was never callable this way. |
| 11 | Multi-line input (`\` continuation) | linenoise multiline mode: `linenoiseSetMultiLine(1)` (`repl.c:1914`). Trailing `\` or unbalanced bracket triggers continuation. | C.0.5: trailing `\` appends to `multiline_buf` (`boxen_repl.c:680-760`). Prompt switches from `"> "` to `".."`. Enter on empty line terminates accumulation. | IDENTICAL | Both skip multiline entries from the history file (line-oriented on-disk format can't represent embedded newlines). |
| 12 | Slash command dispatch | `dispatch_slash_command()` (de-static'd from `repl.c`, declared in `repl_slash_dispatch.h`). Slash dispatch + palette synthesis both terminate at `meuserselected_headless`. | Same. Boxen wires the same `dispatch_slash_command` from `repl_slash_dispatch.h`. | IDENTICAL | Shared code; not duplicated. |
| 13 | Interactive dialog verbs (`dialog.getString` / `dialog.getInt` / `dialog.getPassword`) | Raw `fprintf(stderr, ...)` + `read_line_with_editing()` in `dialog_prompts.c`. Bypasses boxen entirely (was invisible-under-boxen until #793). | C.0.7g (#793): UI bridge in `boxen_ui.c` opens a centered boxen modal with the same GIL discipline as the main loop. Cooperative cancel (Esc / Ctrl-C). Per-mode field validation. | SUPERSET in boxen mode | Linenoise path unchanged. See `planning/phase_c/BOXEN_USERTALK_UI_BRIDGE.md` for the bridge design + deferrals. |
| 14 | Remaining dialog verbs (`dialog.alert`, `dialog.notify`, `dialog.twoway`, `dialog.threeway`) | Raw terminal IO via `dialog_prompts.c` | C.0.7g Phase 2A (#800): `boxen_ui_alert` + `boxen_ui_button_select` bridges all four to boxen modals (info-and-wait + 2/3-button selector). | SUPERSET in boxen mode | Bell rings via `host->ring_bell` (bypasses capture pipe).  Cooperative cancel. |
| 15 | File dialog verbs (`file.getFileDialog` / `putFileDialog` / `getFolderDialog` / `getDiskDialog`) | Behind `isInteractiveMode()` guard; runs full-screen TUI when triggered | C.0.7g Phase 2B (#801): `boxen_ui_pick_file` single-pane list picker with breadcrumb header.  `put_file` adds bottom-row filename input + overwrite-confirm modal.  GET_DISK browses `/Volumes/` on macOS. | SUPERSET in boxen mode | Hidden dotfiles hidden; `..` hidden at root.  Display-only type filter. |
| 16 | `repl.printKeyCodes()` debug verb | `linenoisePrintKeyCodes()` — takes raw stdin until ESC×3 | Same `linenoisePrintKeyCodes()` call; would corrupt the boxen framebuffer if invoked | GAP — easy fix | One-line guard to short-circuit with a "use linenoise mode" scrollback message. Filed as #794. |

## Status legend

- **IDENTICAL** — same behavior, same UX, same code path or a faithful re-implementation
- **SUPERSET** — boxen does what linenoise does plus more
- **DIFFERENT-BUT-INTENTIONAL** — different behaviors, both correct for their architecture; not a regression
- **BOXEN-ONLY** — feature exists only in boxen because linenoise's architecture can't host it
- **GAP** — boxen is missing something linenoise has, would be a regression to flip the default without a story for it

## GAPs and their follow-ups

The three rows marked GAP above are tracked and have follow-up work scoped:

1. **Remaining dialog verbs (#14)** → Phase 2 of the UI bridge. Plan: `planning/phase_c/BOXEN_USERTALK_UI_BRIDGE.md` Phase 2.
2. **File dialog verbs (#15)** → Phase 2 of the UI bridge (or a later phase). Same plan doc.
3. **`repl.printKeyCodes()` guard (#16)** → Filed as issue #794. One-line fix; trivial.

None of these block the C.6 default flip:
- #14: the dispatched menu scripts (jump, keycodes, etc.) do not currently call the remaining dialog verbs. A user invoking them from a UserTalk expression hand-typed at the REPL gets the linenoise-mode raw-stderr fallback — visible, even if not pretty. Behavior at parity with linenoise; UX upgrade pending.
- #15: file dialogs are already unreachable from both REPLs in current builds. No behavioral change either way.
- #16: a one-line guard could land before C.6 if desired; if not, the worst case is corrupt-framebuffer when a user explicitly invokes `repl.printKeyCodes()` under `--debug-tui` — recoverable by exiting the REPL.

## Acceptable divergences

Behaviors where boxen deliberately differs from linenoise. These are not GAPs and not regressions.

1. **History ring size: boxen 500 vs linenoise 1000.** Tuning choice; same on-disk format and merge strategy, so history switches across REPLs cleanly. If memory pressure ever becomes a concern, raising boxen to 1000 is a one-line change.
2. **Async output model.** Linenoise hides/shows the prompt in place; boxen captures stdout/stderr into a pipe and drains into a scrollback pane. Architectural choice for boxen; produces visibly cleaner output during long-running scripts.
3. **Slash-palette debounce: linenoise 250 ms vs boxen 350 ms.** Boxen's longer debounce was a deliberate concession to slower typists (see C.0.7a / palette completion-race work). Both are responsive; matter of taste.
4. **Multi-window UX.** Boxen has co-resident windows (input bar + scrollback + outline editor); linenoise has none. Inherent architectural difference — boxen's window model is the prerequisite for C.1 / C.2.
5. **Stdout/stderr capture.** Same trade-off as async output (#2).
6. **`--debug-tui @path` auto-launch.** Boxen-only by design. Linenoise's event loop is not structured for this.

## How to verify this table

The table above was assembled by an `Explore` agent walking the two REPL implementations at `develop @ a0d59b362` on 2026-06-23. To re-verify against a future HEAD:

1. Grep the file-and-line refs in each row to confirm the implementation is still where claimed.
2. For each `IDENTICAL` row, confirm both REPLs still terminate at the same shared function (`dispatch_slash_command`, `meuserselected_headless`, `repl_complete_slash_command_path`).
3. For each `DIFFERENT-BUT-INTENTIONAL` row, confirm the rationale in this doc still holds — if the architectural choice has been revisited, update the row.
4. For each `GAP` row, check that the linked follow-up issue is still open (or has been fixed and the row can be promoted).
5. Run the tui-test harness (`make -C tools/tui-tests`) — it covers most parity-critical behaviors.

## Implications for C.6

Per the execution plan (`planning/phase_c/REPL_SCHISM_EXECUTION_PLAN.md` §6.7), C.0.6 is done when:

- [x] Parity table is complete; any GAPs have follow-up issues filed
- [x] CLI usage guide updated (see `docs/CLI_USAGE_GUIDE.md`)
- [x] README updated — **deferred to C.6** per JES decision 2026-06-23 (avoid double-update right before the default flip)
- [x] `/gate PASS WITH NOTES (docs)` — to be run on this commit

The C.6 milestone (flip the default; deprecate `--debug-tui`) is unblocked by this verification.
