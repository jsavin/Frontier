# TUI Test Harness

Headless regression tests for `dist/frontier-cli --debug-tui`, the boxen
TUI REPL. Drives the binary inside a detached tmux pane, sends synthetic
keystrokes, captures the rendered pane, asserts on what appears.

## Why

Unit tests in `tests/boxen_repl_tests.c` mock-inject `boxen_event_t`
values directly. They verify the REPL's internal logic given valid
events, but they don't exercise:

- termbox2 escape-sequence decoding (does `ESC [ D` actually become
  `BOXEN_KEY_LEFT`?)
- The full draw → terminal → capture pipeline
- Behavior that depends on real terminal characteristics (column width,
  scroll, focus)
- Crashes that only surface under real raw-mode I/O

These TUI tests run the REAL binary and assert on observable output.
They are slower and slightly more fragile, but they catch regressions
unit tests can't.

## Setup

Required:

- `tmux` — `brew install tmux`
- Python 3.10+ — `brew install python3`
- A built `dist/frontier-cli` and `dist/Frontier.root` — `make -C frontier-cli`

Recommended (for visual inspection):

- `aha` — `brew install aha`. Without it, text + ANSI captures are still
  saved, but the runner skips HTML rendering.

## Run

From the repo root:

```bash
make -C tools/tui-tests
```

Or directly:

```bash
./tools/tui-tests/run.sh           # use existing dist/ build
./tools/tui-tests/run.sh --rebuild # force rebuild first
```

Or target a specific build via env vars:

```bash
FRONTIER_TEST_BIN=/path/to/frontier-cli \
FRONTIER_TEST_ROOT=/path/to/Frontier.root \
python3 tools/tui-tests/tui_harness.py
```

Exit codes:

- `0` — all tests passed
- `1` — one or more tests failed (artifacts saved for inspection)
- `2` — no tests discovered or harness error
- `3` — required infra missing (tmux not in PATH)

## Artifacts

Every snapshot saves a directory under `tools/tui-tests/artifacts/<label>/`:

- `capture.txt` — plain pane content for assertions
- `capture.ansi` — raw escape sequences (re-renderable in any terminal
  with `cat`)
- `capture.html` — rendered HTML with color and attributes preserved
  (only if `aha` is installed)

To see what a test looked like visually, open the HTML file in a
browser. To replay the raw rendering in a terminal, `cat capture.ansi`.

## Writing a test

```python
from tui_harness import TUI, snapshot

def test_my_feature():
    with TUI(boot_wait=1.5) as t:
        t.wait_for(">", timeout=5)   # initial prompt
        t.send("/help")              # literal text
        t.send_key("Enter")          # named key
        t.wait_for("commands", timeout=2)
        snapshot(t, "help-output")   # save text + ANSI + HTML
```

### Key methods

- `t.send(text)` — literal text; `\n` becomes Enter
- `t.send_key(name)` — named key via tmux: `Up`, `Down`, `Left`, `Right`,
  `Tab`, `Enter`, `Escape`, `BSpace`, `Space`, `Home`, `End`, `PgUp`,
  `PgDn`, `DC` (forward delete), or control combos like `C-c`, `C-a`,
  `M-x`
- `t.wait_for(needle, timeout)` — poll until needle appears in pane;
  raises with the last capture on timeout
- `t.wait_for_no(needle, timeout)` — poll until needle disappears
- `t.capture()` — plain text snapshot of pane content
- `t.capture_ansi()` — ANSI-preserved snapshot
- `t.cursor_position()` — `(col, row)` of tmux cursor
- `t.pause(seconds)` — sleep; use sparingly, prefer `wait_for`
- `t.is_alive()` — True if REPL hasn't exited

### Timing notes

Two common timing pitfalls:

1. **Race between send and capture.** After sending input that triggers
   a redraw, sleep ~0.3s before capturing or use `wait_for`. tmux and
   the REPL run independently.

2. **350ms slash debounce.** Typing `/` arms a debounce; the palette
   opens 350ms later unless cancelled. To open the palette: `t.send("/")`
   then `t.pause(0.6)`. To bypass the debounce: send another key
   (Tab, Enter, Up, Down, Backspace, Escape, Ctrl-C) within 350ms.

## /auto Test Manifest tier

This harness lives at `skippable` tier in the project CLAUDE.md /auto
Test Manifest. `/auto` runs it if tmux is available; otherwise it notes
"TUI tests skipped — tmux not installed" in the PR description.

It is NOT a required pre-merge layer — pty/tmux interactions can flake
on different CI hosts. Use it as an early signal during PR work and as
a regression guard for boxen REPL changes.

## Known limitations

- Some terminal-dependent rendering may differ between environments
  (8-color vs 256-color, different fonts, narrower terminals). Tests
  use `cols=80, rows=24` and assert on text content, not pixel layout.
- The harness doesn't catch UI quality issues like color contrast or
  visual alignment — only what can be asserted via captured text/ANSI.
- macOS-specific: some key bindings differ from Linux (Fn+Delete on Mac
  laptops sends DC; bare Delete sends Backspace).
