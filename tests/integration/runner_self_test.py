#!/usr/bin/env python3
"""
Self-tests for tests/integration/runner.py — palette_mode + screenshot_match.

Behavioral tests that exercise the L4 palette test harness extensions to
runner.py without spawning frontier-cli. The pyte Stream/Screen pair is fed
synthetic bytes directly so the screenshot-comparison and golden-update paths
can be validated in isolation from the PTY layer.

Run with:
    python3 -m unittest tests.integration.runner_self_test -v
"""

import io
import os
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock


# Make the runner module importable as 'runner' (its sibling layout) and ensure
# the vendored pyte/wcwidth take precedence over any pip-installed copies.
_HERE = os.path.dirname(os.path.abspath(__file__))
_REPO_ROOT = os.path.abspath(os.path.join(_HERE, '..', '..'))
_VENDOR_DIR = os.path.join(_REPO_ROOT, 'tests', 'vendor')
if _VENDOR_DIR not in sys.path:
    sys.path.insert(0, _VENDOR_DIR)
if _HERE not in sys.path:
    sys.path.insert(0, _HERE)

import runner  # noqa: E402
import pyte    # noqa: E402


class PaletteModeParsingTest(unittest.TestCase):
    """TestCase should parse palette_mode and a screenshot_match interactive step."""

    def test_palette_mode_and_screenshot_match_step_parsed(self):
        data = {
            'name': 'palette parse smoke',
            'palette_mode': True,
            'interactive_steps': [
                {'screenshot_match': 'foo.txt'},
            ],
        }
        tc = runner.TestCase(data)

        # palette_mode flag is preserved on the instance
        self.assertTrue(tc.palette_mode,
                        "palette_mode flag not stored on TestCase")

        # interactive_steps list preserves the screenshot_match step as-is
        self.assertEqual(len(tc.interactive_steps), 1)
        self.assertEqual(tc.interactive_steps[0].get('screenshot_match'),
                         'foo.txt')

    def test_palette_mode_defaults_false_when_absent(self):
        data = {'name': 'no palette', 'interactive_steps': []}
        tc = runner.TestCase(data)
        self.assertFalse(tc.palette_mode,
                         "palette_mode should default to False")


class ScreenshotMatchGoldenUpdateTest(unittest.TestCase):
    """FRONTIER_UPDATE_GOLDENS=1 with a missing fixture creates the fixture file."""

    def test_missing_fixture_with_update_env_creates_file(self):
        # Feed a deterministic screen state via pyte directly (no PTY).
        screen = pyte.Screen(40, 5)
        stream = pyte.Stream(screen)
        stream.feed("alpha\r\nbeta")

        with tempfile.TemporaryDirectory() as tmp:
            golden_path = os.path.join(tmp, 'frame.txt')
            self.assertFalse(os.path.exists(golden_path),
                             "precondition: golden must not exist yet")

            with mock.patch.dict(os.environ, {'FRONTIER_UPDATE_GOLDENS': '1'}):
                ok, err = runner.compare_screen_to_golden(
                    screen=screen,
                    golden_path=golden_path,
                    actual_dump_path=os.path.join(tmp, 'frame.actual'),
                )

            self.assertTrue(ok, f"update-mode call should succeed; got err={err!r}")
            self.assertTrue(os.path.exists(golden_path),
                            "golden file must be written when update env is set")
            written = Path(golden_path).read_text(encoding='utf-8')
            # Normalized frame: rstrip per line, rstrip whole frame.
            self.assertIn("alpha", written)
            self.assertIn("beta", written)
            # Trailing whitespace columns are stripped per row.
            self.assertNotRegex(written, r' +\n',
                                "per-line trailing whitespace not stripped")


class ScreenshotMatchMismatchTest(unittest.TestCase):
    """A non-matching fixture produces a failure result with a mismatch message."""

    def test_mismatch_reports_failure(self):
        screen = pyte.Screen(40, 5)
        stream = pyte.Stream(screen)
        stream.feed("hello world")

        with tempfile.TemporaryDirectory() as tmp:
            golden_path = os.path.join(tmp, 'frame.txt')
            actual_path = os.path.join(tmp, 'frame.actual')
            # Write a clearly-wrong golden.
            Path(golden_path).write_text("goodbye moon\n", encoding='utf-8')

            # Ensure update env is NOT set.
            env = {k: v for k, v in os.environ.items()
                   if k != 'FRONTIER_UPDATE_GOLDENS'}
            with mock.patch.dict(os.environ, env, clear=True):
                ok, err = runner.compare_screen_to_golden(
                    screen=screen,
                    golden_path=golden_path,
                    actual_dump_path=actual_path,
                )

            self.assertFalse(ok, "mismatch must surface as failure")
            self.assertIsNotNone(err)
            # Message should mention a screenshot mismatch (case-insensitive).
            self.assertRegex(err.lower(), r'screenshot.*mismatch|mismatch.*screen',
                             f"failure message should mention screenshot mismatch; got: {err!r}")
            # An .actual dump should be written alongside the golden for diffing.
            self.assertTrue(os.path.exists(actual_path),
                            "actual frame must be dumped on mismatch")


if __name__ == "__main__":
    unittest.main()
