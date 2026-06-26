# Frontier CLI Release Notes

Most-recent first.  Each entry summarizes user-visible changes for the release.

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
