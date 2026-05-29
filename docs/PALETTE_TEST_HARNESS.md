# Palette Test Harness

Four-layer test infrastructure for the REPL slash-menu palette modal.

## Why four layers

The palette has unusual surface area for a CLI component: a state machine
driven by both keyboard and SGR-1006 mouse events, a render pass that
fills a cell buffer, a compositor that diff-renders into the terminal,
and an end-to-end path that only behaves correctly under a real PTY with
real timer semantics. No single test layer catches all four classes of
bug:

- **Unit-only** misses linenoise integration, ESC disambiguation timing,
  and mouse-mode-leak-on-Ctrl-C.
- **E2E-only** runs slow (~5s per case), is harder to debug, and pays
  the PTY spawn cost on every render-detail change.

Splitting by concern keeps each layer fast and pinpoints failure
locality: a green L1 with a red L4 says "rendering is fine, integration
is broken" without needing further triage.

---

## The four layers

### L1 — render snapshot
- **What it tests**: pure `palette_render(state)` -> cell buffer produces the expected grid.
- **Where**: `tests/palette_render_snapshot_tests.c`.
- **When to add**: new visual element (border, hotkey style, overflow indicator) or a layout rule change.

### L2 — input -> state transitions
- **What it tests**: key bytes / SGR-1006 mouse events drive the right state transitions.
- **Where**: `tests/palette_state_tests.c`, `tests/palette_arg_inject_tests.c`, `tests/palette_arg_inject_concurrency_tests.c`.
- **When to add**: new keybinding, mouse handling edge case, parser ambiguity.

### L3 — compositor / panes
- **What it tests**: pane register/unregister, Z-order, diff render, hit-test.
- **Where**: `tests/pane_compositor_tests.c`, `tests/repl_palette_source_tests.c`.
- **When to add**: new pane lifecycle behavior or a compositor rule change.

### L4 — end-to-end PTY harness
- **What it tests**: real PTY, real `frontier-cli`, captured frame matches a golden file.
- **Where**: `tests/integration/test_cases/palette_modal_smoke.yaml`; fixtures under `tests/fixtures/palette/`.
- **When to add**: new end-to-end modal flow, or a regression that does not reproduce at L1-L3.

Push every test as far down the stack as the bug allows. L1 is
deterministic and runs in microseconds; L4 spawns a child process and
costs seconds. Reach for L4 only when the bug genuinely requires real
linenoise, real timer behavior, real signal handling, or real terminal
escape interpretation.

---

## L1, L2, L3 (unit layers)

The unit layers are plain C executables built by `tests/Makefile` and
run by `tools/run_headless_tests.sh`. They link the production palette /
compositor sources directly and exercise them through public APIs (plus
a small set of `#ifdef FRONTIER_PALETTE_TEST_HOOKS` peek functions for
state that has no production reader). No shims, no fakes for the code
under test — the only injection point is a fake clock for ESC
disambiguation timing.

If adding a unit test, follow the conventions in the existing
`palette_*_tests.c` files: `tests/test_report.h` for assertions, one
binary per test file, deterministic input only. There are no goldens at
L1-L3 — assertions are inline cell-by-cell or transition-by-transition.

---

## L4 (end-to-end PTY layer)

L4 is the newest layer and the only one with external dependencies. The
sections below cover what runs, how, and what to do when it breaks.

### Toolchain

- **pexpect** + **ptyprocess** (ISC, vendored under `tests/vendor/`) —
  spawn frontier-cli in a PTY and drive it interactively.
- **pyte** (LGPL-3.0, NOT vendored — see `RELICENSING.md`) — VT100 /
  xterm screen emulator on the test side. Listed in
  `tests/requirements.txt`; auto-installed by `runner.py`'s
  `_ensure_pyte()` on the first `palette_mode` test invocation, so a
  fresh checkout needs no manual setup. The bootstrap tries
  `pip install --user`, then the PEP 668 `--break-system-packages`
  escape hatch for Homebrew / Debian system Python.
- **`FRONTIER_PALETTE_FAST_TIMERS=1`** — env var honored by
  `frontier-cli` (always — no compile-time gating). Compresses the ESC
  disambiguation poll from 10 ms to 1 ms so tests don't wait wall-clock
  for keystroke-vs-CSI decisions. The runner sets it automatically in
  the child env for every `palette_mode` test. On first `palette_open`
  when the variable is set, a one-time stderr notice fires so a confused
  user who has the variable set in their shell sees why their terminal
  is behaving differently.

### How a palette_mode YAML test runs

1. `runner.py` sees `palette_mode: true` on a test and dispatches into
   `execute_interactive_palette()` instead of the plain interactive path.
2. `_ensure_pyte()` runs once per process — imports pyte, pip-installs
   it if missing, surfaces a clear error if both fail.
3. The runner spawns `frontier-cli` in a 24x80 PTY with
   `TERM=xterm-256color`, `FRONTIER_PALETTE_FAST_TIMERS=1`, and
   `FRONTIER_FORCE_INTERACTIVE=1`. (No `FRONTIER_PLAIN_REPL` — the full
   event loop is required.)
4. Each step in `interactive_steps` is dispatched by key (see table
   below).
5. On `screenshot_match`, the runner drains pending PTY bytes (bounded
   read loop: 2 consecutive empty reads OR 500 ms total), feeds them
   through `pyte.Stream` -> `pyte.Screen`, normalizes the resulting
   frame, and compares against the golden file.
6. On mismatch, the actual frame is written to
   `tests/tmp/results/palette/<slug>.actual` and the failure includes a
   unified diff.

### YAML keys supported

| Key                                       | Type                       | Purpose                                                                                            |
| ----------------------------------------- | -------------------------- | -------------------------------------------------------------------------------------------------- |
| `palette_mode`                            | bool, default `false`      | Opt into the PTY + pyte path. Without it, tests run in the default plain-REPL path (`TERM=dumb`).  |
| `interactive_steps[].expect`              | string (pexpect pattern)   | Wait for a pattern in PTY output before the next action.                                           |
| `interactive_steps[].send`                | string                     | Send a command line; trailing `\n` is appended automatically.                                      |
| `interactive_steps[].send_raw`            | string                     | Send raw bytes with no trailing newline. Use `\x1b` for ESC, `\x04` for Ctrl-D, etc.               |
| `interactive_steps[].screenshot_match`    | string (repo-relative)     | Drain pending bytes, normalize the pyte frame, compare against a golden under `tests/fixtures/`.   |

`send_raw` exists specifically for the palette: single keystrokes,
escape sequences, and Ctrl-keys must arrive as bare bytes — appending
`\n` would advance the modal state machine in unintended ways.

### Regenerating goldens

When an intentional UI change makes the prior frame wrong:

```bash
FRONTIER_UPDATE_GOLDENS=1 ./tools/run_integration_tests.sh \
    tests/integration/test_cases/palette_modal_smoke.yaml
```

The test reports PASS without comparing — the runner just writes the
current frame to the golden path (creating parent dirs as needed).

**Always review the regenerated fixture before committing.** A subtle
bug elsewhere can produce a "stable but wrong" frame that locks the
wrong behavior into the golden:

```bash
cat tests/fixtures/palette/menubar_just_opened.txt
git diff tests/fixtures/palette/
```

When a golden fails and you don't yet know why, do NOT regenerate as a
first move. Investigate first — see `tests/fixtures/palette/README.md`
for the diagnostic checklist (layout drift vs. timer mode vs. stderr
noise vs. PTY size).

### When to use L4 vs. lower layers

Pick the lowest layer that can express the bug:

- **L1** — wrong character / attribute in a cell (pure render; no PTY noise).
- **L1** — layout breaks at the right edge (state-only; no terminal needed).
- **L2 + L3** — mouse coordinate off-by-one (parser -> hit-test).
- **L3** — diff-render emits CSI when nothing changed (compositor-internal).
- **L4** — ESC ambiguity in a real terminal (only PTY exposes the timing surface).
- **L4** — palette doesn't open on `/` at column 1 (linenoise integration only happens in the real REPL).
- **L4** — mouse mode leak on Ctrl-C (only signals work in a real process).

Default: try the lowest layer that can express the bug. Adding a new L4
test should be a deliberate choice, not the default reach.

---

## Known gaps / follow-ups

- **`send_mouse` YAML key** — deferred. The original design (see
  `planning/discussions/repl-palette-test-harness.md`) called for a key
  that synthesizes SGR-1006 mouse sequences. Not yet implemented;
  workaround is `send_raw` with a hand-built `\x1b[<...M` byte string.
- **`emitted_csi_contains` YAML key** — deferred. Would capture the
  raw byte stream from the child PTY independently of pyte's
  framebuffer interpretation, letting tests assert on specific CSI
  emissions (e.g. mouse-mode disable on exit). Currently goldens cover
  only the rendered frame, not the bytes-on-the-wire.
- **Lone-ESC swallow bug** — pressing a single bare ESC at the menubar
  is currently buffered by the modal's mouse-SGR parser waiting to see
  if it begins a `\x1b[<...M` sequence. The L4 smoke test works around
  this by sending `\x1b\x1b` (the second ESC bounces the parser out
  with a non-`[` follow-up, replaying both bytes into
  `palette_feed_byte` where `esc_pending->cancel` collapses the
  menubar). Real fix needs a "flush mouse-parser on poll timeout"
  branch in `run_palette_modal()`.
- **No incremental golden regeneration** — `FRONTIER_UPDATE_GOLDENS=1`
  re-runs the whole smoke YAML and rewrites every fixture it references.
  Acceptable today because the suite has one test; revisit when L4
  grows.

---

## See also

- `planning/discussions/repl-palette-test-harness.md` — original four-layer design and rationale
- `tests/fixtures/palette/README.md` — golden file format, regeneration checklist, "do NOT regenerate first" diagnostic flow
- `tests/vendor/README.md` — vendored dependency manifest (pexpect, ptyprocess) and why pyte is NOT vendored
- `tests/integration/runner.py` — `execute_interactive_palette()`, `_ensure_pyte()`, `_normalize_screen_frame()`, `compare_screen_to_golden()`
- `docs/TESTING_GUIDE.md` — general Frontier test infrastructure (CLI usage, integration YAML format, system.temp rule)
