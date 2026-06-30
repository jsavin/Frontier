# Frontier CLI Release Notes

Most-recent first.  Each entry summarizes user-visible changes for the release.

---

## C.6.3 — mouse-wheel scrollback and Option-arrow word-jump in boxen REPL (2026-06-29)

### Summary

Two input-handling regressions surfaced by the C.6 flip to the boxen REPL are fixed:

- **Mouse wheel** now scrolls the output pane (3 lines per tick), like every other terminal UI.  Previously the wheel was being interpreted as up/down arrows by the host terminal and routed to history navigation -- a muscle-memory regression versus the legacy linenoise REPL.
- **Option-Left** / **Option-Right** now jump the input caret by one word (readline `M-b` / `M-f` convention).  Previously these inserted literal `b` / `f` characters into the input bar because the Option-key Esc-prefix was being decoded but the resulting Alt modifier was ignored.

### What changed

- Mouse mode is enabled in the boxen REPL's terminal backend.  Wheel events arrive as real mouse events (button 4/5) and are routed to the same scroll-offset mechanism as PgUp/PgDn (#803).  Three-line wheel step matches the `less` and macOS Terminal.app convention.
- Both Esc-prefix (`ESC b` / `ESC f`, the macOS Terminal.app default) and CSI (`ESC[1;3D` / `ESC[1;3C`, iTerm2 with "Left/Right option as Esc+") encodings of Option-arrow are recognized.
- Word boundaries follow alphanumeric runs.  `foo_bar_baz` splits into three words; underscores and other non-alphanumerics are treated as separators (matches bash readline).
- Unrecognized Alt-modified printable keys (e.g. Option-x) are silently swallowed rather than inserting the literal character -- protects against the same class of bug recurring with other Option-letter combos.
- CLI usage guide updated with both bindings.

### Trade-off (mouse mode)

Enabling mouse mode means click-and-drag text selection within the REPL goes through xterm mouse tracking rather than the terminal's native selection.  To select text natively, hold `Option` (macOS Terminal.app) or `Cmd` (iTerm2) while dragging to bypass mouse mode.

### Why now

The C.6 flip to boxen as the default REPL surfaced these as the first user-reported keystroke regressions versus linenoise.  Both have a workaround (`--plain` falls back to linenoise), but defaulting to a REPL where the wheel maps to history and Option-arrows insert literal letters is hostile to muscle memory.  Resolves #805.

---

## C.6.2 — boxen REPL: visible errors for slash commands (2026-06-29)

### Summary

Fixes a silent-failure class in the boxen REPL: slash-command error messages (e.g. `/list builtins` when `builtins` is not a valid path) now appear in the scrollback, matching the behavior of the linenoise (`--plain`) REPL.

### What changed

- `/list <bad-path>` now prints `Error: '<path>' is not a valid table path (...)` in the boxen scrollback. Previously the line returned a fresh prompt with no output and no error (#804).
- The same fix restores visibility for any other slash-command code path whose diagnostic was written via `fputs(stdout)` without an explicit `fflush(stdout)`.
- Linenoise (`--plain`) behavior is unchanged.

### Why it was silent

When the boxen REPL captures `stdout`/`stderr` into its scrollback pipe, libc switches those streams from line-buffered (tty default) to block-buffered (pipe default). Any diagnostic that wrote a newline-terminated message via `fputs` without an explicit `fflush` sat in the libc buffer and never reached the scrollback drain. The fix forces line-buffering on the captured streams via `setvbuf(_, NULL, _IOLBF, 0)` immediately after the pipe redirect, so newline-terminated writes flush automatically — eliminating the entire class of missing-`fflush` silent-failure bugs.

---

## C.6.1 — boxen REPL output-pane scrollback (2026-06-29)

### Summary

The boxen REPL output pane now supports in-app scrollback via **PgUp** and **PgDn**.  Output that has scrolled off the top of the visible region (e.g. dumping `system.verbs.builtins`, a wide table, or a long stack trace) is reachable again without falling back to `--plain`.

### What changed

- **PgUp** scrolls the output pane up by one page (output height minus one line of context overlap), revealing older content.
- **PgDn** scrolls back toward the newest line; clamped at the bottom.
- **Submitting** a new expression (Enter) auto-snaps the view back to the bottom -- standard scroll-on-output behavior, matching `less` and most pagers when new content arrives via a follow-style trigger.
- **Async output** (background scripts printing via `drain_stdout_into_scrollback`) does NOT reset the offset.  You can review old content while a long-running script keeps emitting lines.  When the 1024-line ring fills, the offset auto-advances in lockstep with the overwrites so the line you were reading stays on screen instead of silently sliding under newer content (less/tmux convention).
- **Footer hint updated** to advertise the new binding.
- Backing buffer is the existing 1024-line scrollback ring; no new memory cost.
- Dialog/file-picker modals and the completion popup are unaffected -- they don't touch the output pane state.

### Why now

Before C.6, the linenoise REPL wrote to the normal terminal buffer, so terminal-native scrollback could reach scrolled-off content.  C.6 flipped boxen to the default, and boxen owns the alternate screen buffer -- terminal scrollback can't see past the visible region.  Resolves #803.

### Out of scope (deferred)

- Mouse-wheel scrolling: addressed in C.6.2 (#805).
- Scroll position indicator in the footer (e.g. `[+42]`): nice-to-have polish for a follow-up.

---

## C.6 — boxen REPL is now the default (2026-06-25)

### Summary

`frontier-cli` (no flag) now launches the **boxen REPL** — a multi-pane terminal UI with composited windows, native dialog modals, and a slash-menu palette.  This replaces the legacy linenoise REPL as the default interactive surface.

### What changed

- **Default REPL flipped.**  `frontier-cli` with no flag opens boxen.
- **New `--plain` flag** opts back into the legacy linenoise REPL.
- **`--debug-tui` is now a deprecated no-op alias** for the default.  It still works (so existing scripts don't break) but emits a one-line deprecation warning at startup and will be removed in a future release.
- **Mutual exclusion** across the three REPL-selection flags: `--debug-tui`, `--plain`, and `--protocol` cannot be combined.  Passing more than one is a usage error caught at argument parsing.
- **`--help` updated** with a new "REPL Selection" section enumerating the three choices.

### Migration

| Old invocation | New invocation |
|----------------|----------------|
| `frontier-cli` (got linenoise) | `frontier-cli --plain` |
| `frontier-cli --debug-tui` (got boxen) | `frontier-cli` (drop the flag) |
| `frontier-cli --debug-tui @path` (got boxen + auto-launch) | `frontier-cli @path` (no behavior change; flag drop is cosmetic) |
| `frontier-cli --protocol …` | unchanged |
| `frontier-cli -e "…"` (batch) | unchanged |

If you have scripts or CI pipelines that pass `--debug-tui` explicitly, they will continue to work.  Each invocation will emit one log line:

```
[general-WARN] main.c: --debug-tui is now the default (boxen REPL); the flag is a no-op alias and will be removed in a future release.  Pass --plain to opt into the legacy linenoise REPL.
```

If that log line is noisy in your environment, drop the `--debug-tui` flag.

### Why now

The boxen REPL has been at feature parity (or superset) with linenoise since milestone C.0.6 (see [`docs/BOXEN_REPL_PARITY.md`](BOXEN_REPL_PARITY.md) for the complete feature table).  The schism between "the default REPL" and "the editor-capable REPL" closes here: every user now gets the multi-window, composited UI by default; the legacy single-line prompt remains available behind an explicit opt-in.

The legacy linenoise REPL itself is not deleted in this release.  Deletion is tracked separately (post-C.6 soak period); during the soak we want the explicit fallback path proven in the wild.

### Known limitations carried forward

- Dialog input fields capped at 256 bytes (#795)
- `repl.printKeyCodes()` corrupts the boxen framebuffer if invoked (#794; use `--plain` if you need it)
- Non-ASCII codepoints dropped on input (#796)
- Outer modal loses focus for ~100 ms when an inner modal closes (#798)

See [`docs/BOXEN_REPL_PARITY.md`](BOXEN_REPL_PARITY.md) for the full list.

---

## C.0.7g Phase 2B — boxen file picker (2026-06-25)

`file.getFileDialog` / `putFileDialog` / `getFolderDialog` / `getDiskDialog` now render as a boxen-native single-pane list picker under the boxen REPL.  `put_file` includes an overwrite-confirm modal so the verb no longer silently overwrites existing files.

## C.0.7g Phase 2A — dialog modal verbs (2026-06-24)

`dialog.alert` / `dialog.notify` / `dialog.twoway` / `dialog.threeway` render as native boxen modals.  `dialog.alert` rings the terminal bell via a host callback (bypassing the boxen stdout-capture pipe).

## C.0.7g Phase 1 — dialog input modals (2026-06-23)

`dialog.ask` / `dialog.getString` / `dialog.getInt` / `dialog.getPassword` render as centered boxen modals.

## C.0.6 — boxen REPL parity verification + docs (2026-06-23)

Complete feature-parity audit published at [`docs/BOXEN_REPL_PARITY.md`](BOXEN_REPL_PARITY.md).  Every linenoise REPL feature checked against the boxen equivalent at HEAD.  Verdict: ready for the C.6 default flip.

## Earlier phases

C.0.1 through C.0.7f (history, completion, palette migration, async output, multi-line input, polish bugfixes, palette-source lifetime fixes) — see git log for details.  These all shipped without external behavior change to the default REPL; users opting into boxen via `--debug-tui` saw the incremental improvements.
