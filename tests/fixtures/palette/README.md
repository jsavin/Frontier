# Palette golden fixtures

Captured terminal frames (plain ASCII / UTF-8) for the L4 palette test
harness. Each `.txt` file is the normalized output of a `pyte.Screen`
after the runner has driven the frontier-cli palette modal through a
specific sequence of keystrokes via a 24x80 PTY.

## Format

- One line per terminal row.
- Per-row trailing whitespace stripped (pyte pads to full column width).
- Trailing blank rows stripped from the whole frame.
- LF line endings, single trailing newline.

Goldens are diffable in any editor — no binary blobs, no ANSI codes.

## How they are produced

The integration runner's `screenshot_match` step feeds drained PTY bytes
through `pyte.Stream` -> `pyte.Screen`, normalizes (see
`_normalize_screen_frame` in `tests/integration/runner.py`), and compares
the result to the named file. On mismatch the actual frame is written to
`tests/tmp/results/palette/<slug>.actual` and a unified diff is included
in the test's failure message.

## Regenerating

Only regenerate when an intentional UI change makes the prior golden
incorrect — never to silence a failing test:

    FRONTIER_UPDATE_GOLDENS=1 cd .. && \
      ./tools/run_integration_tests.sh \
      tests/integration/test_cases/palette_modal_smoke.yaml

Review the diff before committing — accidental regressions (extra
spaces, stray ANSI, cursor in wrong cell) look identical to "the menu
moved one row" until you read carefully.

## When NOT to regenerate

If a golden suddenly fails and you don't know why, do NOT regenerate
first. Investigate:

- Did palette layout change unintentionally?
- Did fast-timer mode (`FRONTIER_PALETTE_FAST_TIMERS=1`) stop being set?
- Did a stderr warning move (e.g. a new boot-time message landed in the
  PTY before the modal opened)?
- Did the PTY dimensions change (the runner pins 24x80)?

Regenerating papers over the bug. The golden's whole job is to tell you
something moved.
