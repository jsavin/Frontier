#!/usr/bin/env python3
"""
Self-tests for the known-failure baseline support in tests/integration/runner.py.

Behavioral tests for the --baseline mechanism (Phase 2 Unit 2.3): the
runner accepts a baseline file listing tests that are expected to fail;
the summary/exit logic then distinguishes three cases:

  1. A failure whose test name IS on the baseline is reported as
     "known-fail (baselined)" and does NOT fail the run.
  2. A failure whose test name is NOT on the baseline fails the run.
  3. A PASS whose test name IS on the baseline is loudly reported as
     "baselined test now passes" and FAILS the run, so the baseline
     list cannot rot.

The tests drive TestRunner.print_summary() directly with synthetic
TestResult objects — print_summary() is the single classification point
whose boolean return feeds the process exit code in both the sequential
and parallel paths of main(), so exercising it exercises the exit-code
semantics without spawning frontier-cli (same convention as
runner_self_test.py).

Run with:
    python3 -m unittest tests.integration.baseline_self_test -v
"""

import contextlib
import io
import os
import sys
import tempfile
import unittest
from unittest import mock

# Make the runner module importable as 'runner' (its sibling layout);
# same bootstrap as runner_self_test.py.
_HERE = os.path.dirname(os.path.abspath(__file__))
_REPO_ROOT = os.path.abspath(os.path.join(_HERE, '..', '..'))
_VENDOR_DIR = os.path.join(_REPO_ROOT, 'tests', 'vendor')
if _VENDOR_DIR not in sys.path:
    sys.path.insert(0, _VENDOR_DIR)
if _HERE not in sys.path:
    sys.path.insert(0, _HERE)

import runner  # noqa: E402


def _make_runner(results):
    """Build a TestRunner with synthetic results and no real CLI.

    TestRunner.__init__ only stores the cli object; print_summary never
    touches it, so a MagicMock keeps the test free of binary dependencies.
    """
    with tempfile.TemporaryDirectory() as tmp:
        r = runner.TestRunner(mock.MagicMock(), test_root_dir=tmp)
    r.results = list(results)
    return r


def _summarize(results, baseline=None):
    """Call print_summary, capturing stdout. Returns (ok, output)."""
    r = _make_runner(results)
    buf = io.StringIO()
    with contextlib.redirect_stdout(buf):
        if baseline is None:
            ok = r.print_summary()
        else:
            ok = r.print_summary(baseline=baseline)
    return ok, buf.getvalue()


def _pass(name):
    return runner.TestResult(name=name, passed=True)


def _fail(name, error="boom"):
    return runner.TestResult(name=name, passed=False, error=error)


def _skip(name):
    return runner.TestResult(name=name, passed=True, skipped=True)


class LoadBaselineParsingTest(unittest.TestCase):
    """load_baseline() parses the known_failures.txt line format."""

    def _write(self, text):
        fd, path = tempfile.mkstemp(suffix='.txt', text=True)
        with os.fdopen(fd, 'w') as f:
            f.write(text)
        self.addCleanup(os.unlink, path)
        return path

    def test_parses_names_reasons_comments_and_blanks(self):
        path = self._write(
            "# header comment\n"
            "\n"
            "html.processMacros - basic expansion | needs webserver table, issue #123\n"
            "tcp thing with  spaces\n"
            "   \n"
            "# another comment\n"
            "startup - guest db mount | flaky mount ordering\n"
        )
        baseline = runner.load_baseline(path)
        self.assertEqual(
            set(baseline.keys()),
            {"html.processMacros - basic expansion",
             "tcp thing with  spaces",
             "startup - guest db mount"},
        )
        self.assertEqual(baseline["html.processMacros - basic expansion"],
                         "needs webserver table, issue #123")
        # A name-only line gets an empty reason, not a KeyError.
        self.assertEqual(baseline["tcp thing with  spaces"], "")

    def test_missing_file_raises_oserror(self):
        with self.assertRaises(OSError):
            runner.load_baseline("/nonexistent/known_failures.txt")


class KnownFailureIsBaselinedTest(unittest.TestCase):
    """Case 1: failure on the baseline does not fail the run."""

    def test_baselined_failure_passes_run_and_is_reported(self):
        baseline = {"tcp.connect - refused": "listener race, issue #601"}
        ok, out = _summarize(
            [_pass("string.upper - basic"),
             _fail("tcp.connect - refused")],
            baseline=baseline,
        )
        self.assertTrue(ok,
                        f"baselined failure must not fail the run; output:\n{out}")
        self.assertIn("known-fail (baselined)", out)
        self.assertIn("tcp.connect - refused", out)
        # The unexpected-failure count must be zero.
        self.assertRegex(out, r"Failed:\s+0")

    def test_baselined_failure_shows_reason(self):
        baseline = {"tcp.connect - refused": "listener race, issue #601"}
        _ok, out = _summarize([_fail("tcp.connect - refused")],
                              baseline=baseline)
        self.assertIn("listener race, issue #601", out)


class UnexpectedFailureFailsRunTest(unittest.TestCase):
    """Case 2: failure off the baseline fails the run."""

    def test_off_list_failure_fails_run(self):
        baseline = {"tcp.connect - refused": "listener race"}
        ok, out = _summarize(
            [_fail("string.upper - basic", error="wrong result")],
            baseline=baseline,
        )
        self.assertFalse(ok,
                         f"non-baselined failure must fail the run; output:\n{out}")
        self.assertIn("string.upper - basic", out)
        self.assertIn("wrong result", out)

    def test_mixed_known_and_unexpected_still_fails(self):
        baseline = {"tcp.connect - refused": "listener race"}
        ok, _out = _summarize(
            [_fail("tcp.connect - refused"),
             _fail("string.upper - basic")],
            baseline=baseline,
        )
        self.assertFalse(ok)


class BaselinedPassFailsRunTest(unittest.TestCase):
    """Case 3: a pass on the baseline is loudly reported and fails the run."""

    def test_baselined_pass_is_loud_and_fails_run(self):
        baseline = {"tcp.connect - refused": "listener race"}
        ok, out = _summarize([_pass("tcp.connect - refused")],
                             baseline=baseline)
        self.assertFalse(ok,
                         f"stale baseline entry must fail the run; output:\n{out}")
        self.assertIn("now passes", out.lower())
        self.assertIn("remove", out.lower())
        self.assertIn("tcp.connect - refused", out)

    def test_baselined_skip_is_neutral(self):
        # A baselined test that was SKIPPED neither rots the baseline nor
        # counts as a known failure — the run stays green.
        baseline = {"tcp.connect - refused": "listener race"}
        ok, out = _summarize([_skip("tcp.connect - refused")],
                             baseline=baseline)
        self.assertTrue(ok,
                        f"baselined skip must be neutral; output:\n{out}")
        self.assertNotIn("now passes", out.lower())


class BaselineEntryNotInRunTest(unittest.TestCase):
    """A baseline entry matching no result warns but does not fail.

    Targeted runs (explicit YAML file args) only execute a subset of the
    suite; entries for tests that did not run must not fail those runs.
    Full-suite rot from deleted/renamed tests still surfaces via the
    warning text.
    """

    def test_unmatched_entry_warns_but_run_stays_green(self):
        baseline = {"deleted test - no longer exists": "was issue #1"}
        ok, out = _summarize([_pass("string.upper - basic")],
                             baseline=baseline)
        self.assertTrue(ok)
        self.assertIn("deleted test - no longer exists", out)
        self.assertIn("did not run", out)


class NoBaselineUnchangedTest(unittest.TestCase):
    """Without a baseline, print_summary keeps its historical behavior."""

    def test_failure_without_baseline_fails(self):
        ok, _out = _summarize([_fail("anything")])
        self.assertFalse(ok)

    def test_all_pass_without_baseline_passes(self):
        ok, _out = _summarize([_pass("anything")])
        self.assertTrue(ok)


if __name__ == "__main__":
    unittest.main()
