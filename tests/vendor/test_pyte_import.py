#!/usr/bin/env python3
"""
Smoke test for vendored pyte (terminal screen emulator).

Verifies that pyte can be imported from the vendored copy, that a Screen +
Stream pair can be constructed, and that feeding bytes through the stream
populates Screen.display as expected.

This test backs the L4 palette test harness, which uses pyte to interpret
ANSI escape sequences emitted by frontier-cli's palette/REPL modes and
compare the resulting screen state against golden text fixtures.
"""

import os
import sys
import unittest


# Insert the vendor directory on sys.path so the vendored pyte is preferred
# over any system-installed copy. This mirrors the pattern used by
# tests/integration/runner.py for pexpect/ptyprocess.
_VENDOR_DIR = os.path.dirname(os.path.abspath(__file__))
if _VENDOR_DIR not in sys.path:
    sys.path.insert(0, _VENDOR_DIR)


class PyteImportSmoke(unittest.TestCase):
    """Behavioral smoke test for the vendored pyte package."""

    def test_import_and_feed(self):
        import pyte  # noqa: F401 — import-time failure is the assertion
        screen = pyte.Screen(80, 24)
        stream = pyte.Stream(screen)
        stream.feed("hello")
        # display is a list of rows (each a string of exactly `columns` chars,
        # right-padded with spaces). The first row should start with "hello".
        self.assertTrue(
            screen.display[0].startswith("hello"),
            f"Expected first row to start with 'hello', got: {screen.display[0]!r}",
        )

    def test_wcwidth_available(self):
        # pyte depends on wcwidth for handling wide characters; ensure the
        # vendored copy is importable so pyte doesn't fall back unexpectedly.
        import wcwidth  # noqa: F401
        # Trivial behavioral check: ASCII letters report width 1.
        self.assertEqual(wcwidth.wcwidth("a"), 1)


if __name__ == "__main__":
    unittest.main()
