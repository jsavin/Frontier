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
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock


# Make the runner module importable as 'runner' (its sibling layout). Vendored
# pexpect/ptyprocess remain on the path; pyte is a pip dependency loaded via
# runner._ensure_pyte() inside individual tests so the bootstrap shim is the
# only entry point AND so importing this module does NOT silently pre-load
# pyte (which would mask the "install when missing" code path from any test
# that wants to exercise it — see PyteBootstrapWhenMissingTest below).
_HERE = os.path.dirname(os.path.abspath(__file__))
_REPO_ROOT = os.path.abspath(os.path.join(_HERE, '..', '..'))
_VENDOR_DIR = os.path.join(_REPO_ROOT, 'tests', 'vendor')
if _VENDOR_DIR not in sys.path:
    sys.path.insert(0, _VENDOR_DIR)
if _HERE not in sys.path:
    sys.path.insert(0, _HERE)

import runner  # noqa: E402


def _load_pyte():
    """Bootstrap pyte for tests that need real Screen/Stream instances.

    Lives outside the module scope (was at import time previously) so that
    tests which patch sys.modules / subprocess to exercise the missing-pyte
    branch don't run against a pre-loaded pyte. Called from setUp() of the
    classes that need it.
    """
    pyte, err = runner._ensure_pyte()
    if pyte is None:
        raise RuntimeError(
            f"runner_self_test: pyte bootstrap failed: {err}"
        )
    return pyte


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

    def setUp(self):
        self.pyte = _load_pyte()

    def test_missing_fixture_with_update_env_creates_file(self):
        # Feed a deterministic screen state via pyte directly (no PTY).
        screen = self.pyte.Screen(40, 5)
        stream = self.pyte.Stream(screen)
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


class ScreenshotMatchUpdateLogsToStderrTest(unittest.TestCase):
    """FRONTIER_UPDATE_GOLDENS=1 must log a loud per-file stderr notice (P1 #5)."""

    def setUp(self):
        self.pyte = _load_pyte()

    def test_update_mode_logs_overwrite_to_stderr(self):
        screen = self.pyte.Screen(20, 3)
        stream = self.pyte.Stream(screen)
        stream.feed("hello")

        with tempfile.TemporaryDirectory() as tmp:
            golden_path = os.path.join(tmp, 'logged_golden.txt')

            captured_stderr = io.StringIO()
            with mock.patch.dict(os.environ, {'FRONTIER_UPDATE_GOLDENS': '1'}), \
                    mock.patch.object(sys, 'stderr', captured_stderr):
                ok, _err = runner.compare_screen_to_golden(
                    screen=screen,
                    golden_path=golden_path,
                )
            self.assertTrue(ok)
            log_text = captured_stderr.getvalue()
            # Behavioral: the notice mentions UPDATING GOLDEN, the actual
            # path, and the triggering env var so a developer who left the
            # flag set in their shell sees what is happening.
            self.assertIn("UPDATING GOLDEN", log_text,
                          f"stderr should announce golden overwrite; got: {log_text!r}")
            self.assertIn(golden_path, log_text,
                          f"stderr should include the path; got: {log_text!r}")
            self.assertIn("FRONTIER_UPDATE_GOLDENS", log_text,
                          f"stderr should name the triggering env var; got: {log_text!r}")


class ScreenshotMatchMismatchTest(unittest.TestCase):
    """A non-matching fixture produces a failure result with a mismatch message."""

    def setUp(self):
        self.pyte = _load_pyte()

    def test_mismatch_reports_failure(self):
        screen = self.pyte.Screen(40, 5)
        stream = self.pyte.Stream(screen)
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


class PyteBootstrapInstallCommandTest(unittest.TestCase):
    """The install command must be constructable as a pure function for testing.

    The shim auto-installs pyte via pip when it is not already importable.
    Exposing the command as a pure function (no subprocess call) lets us
    assert on its exact shape without mocking subprocess.run().
    """

    def test_install_command_uses_python_pip_user_install(self):
        cmd = runner._pyte_install_command()
        self.assertIsInstance(cmd, list,
                              "install command must be a list (for subprocess)")
        # Must invoke pip via the current interpreter so we install into the
        # same Python that's running the runner.
        self.assertEqual(cmd[0], sys.executable,
                         f"command must use current interpreter; got {cmd[0]!r}")
        self.assertEqual(cmd[1:4], ['-m', 'pip', 'install'],
                         f"expected '-m pip install' prefix; got {cmd[1:4]!r}")
        self.assertIn('--user', cmd,
                      "install must be --user-scoped (no root required)")
        # The pyte package spec is the final positional argument.
        self.assertEqual(cmd[-1], 'pyte>=0.8.2',
                         f"expected 'pyte>=0.8.2' as last arg; got {cmd[-1]!r}")


class PyteBootstrapWhenAlreadyInstalledTest(unittest.TestCase):
    """_ensure_pyte() is a no-op when pyte is already importable.

    Subprocess.run is patched to raise on any call so an accidental install
    attempt is caught immediately. We force-load pyte in setUp before the
    patch goes up, so the early-return path is the one under test.
    """

    def test_ensure_pyte_returns_module_without_install(self):
        # Force pyte to be importable for this test.
        _load_pyte()
        self.assertIn('pyte', sys.modules,
                      "precondition: pyte must already be loaded")

        with mock.patch.object(runner.subprocess, 'run',
                               side_effect=AssertionError(
                                   "subprocess.run must not be called when pyte is already importable")):
            module, err = runner._ensure_pyte()

        self.assertIsNone(err, f"expected no error; got {err!r}")
        self.assertIsNotNone(module, "expected pyte module to be returned")
        # Behavioral check: the returned module must be usable as pyte.
        screen = module.Screen(10, 2)
        stream = module.Stream(screen)
        stream.feed("hi")
        self.assertTrue(screen.display[0].startswith("hi"),
                        f"returned module not functional as pyte; display={screen.display!r}")


class PyteBootstrapWhenMissingTest(unittest.TestCase):
    """When pyte is NOT importable, _ensure_pyte must invoke pip and reimport.

    Drops pyte from sys.modules, masks the import so the first `import pyte`
    raises ImportError, and verifies that subprocess.run is called with the
    documented install command. Subprocess is mocked so no pip actually runs.
    """

    def test_ensure_pyte_invokes_pip_when_module_missing(self):
        # Snapshot pyte so we can restore it after — other tests need it.
        saved_pyte = sys.modules.pop('pyte', None)
        # Also pop any pyte.* submodules so a partial import doesn't satisfy
        # the bootstrap.
        saved_submods = {k: sys.modules.pop(k) for k in list(sys.modules)
                         if k.startswith('pyte.')}

        # First import attempt raises; we then "succeed" by leaving a fake
        # pyte module in sys.modules that the second import call returns.
        original_import = __builtins__['__import__'] if isinstance(__builtins__, dict) \
            else __builtins__.__import__

        fake_pyte = mock.MagicMock(name='fake_pyte_module')
        import_call_count = {'n': 0}

        def fake_import(name, *args, **kwargs):
            if name == 'pyte':
                import_call_count['n'] += 1
                if import_call_count['n'] == 1:
                    raise ImportError("simulated: pyte not installed")
                # After the (mocked) install, populate sys.modules so the
                # real import machinery returns the fake module.
                sys.modules['pyte'] = fake_pyte
                return fake_pyte
            return original_import(name, *args, **kwargs)

        # Stub subprocess.run to report a successful install without
        # actually invoking pip.
        fake_run_result = mock.MagicMock(returncode=0, stderr='', stdout='')
        run_calls = []

        def fake_run(cmd, **kwargs):
            run_calls.append((cmd, kwargs))
            return fake_run_result

        try:
            with mock.patch.object(runner.subprocess, 'run', side_effect=fake_run), \
                    mock.patch('builtins.__import__', side_effect=fake_import):
                module, err = runner._ensure_pyte()
        finally:
            # Restore the real pyte so subsequent tests have it back.
            sys.modules.pop('pyte', None)
            if saved_pyte is not None:
                sys.modules['pyte'] = saved_pyte
            for k, v in saved_submods.items():
                sys.modules[k] = v

        self.assertIsNone(err, f"expected success after install; got err={err!r}")
        self.assertIs(module, fake_pyte,
                      "should return the freshly-imported module")
        self.assertGreaterEqual(len(run_calls), 1,
                                "pip install must have been invoked at least once")
        # First install attempt should be the documented user-install command.
        first_cmd = run_calls[0][0]
        self.assertEqual(first_cmd[0], sys.executable)
        self.assertEqual(first_cmd[1:4], ['-m', 'pip', 'install'])
        self.assertIn('--user', first_cmd)
        self.assertEqual(first_cmd[-1], 'pyte>=0.8.2')
        # Timeout was applied (P1 #4).
        self.assertEqual(run_calls[0][1].get('timeout'), 120,
                         f"install subprocess must use timeout=120; "
                         f"got kwargs={run_calls[0][1]!r}")


class PyteBootstrapHandlesPipTimeoutTest(unittest.TestCase):
    """If pip install hangs past the timeout, _ensure_pyte returns an error path (P1 #4).

    The whole point of the timeout is that a wedged network mirror cannot
    stall the test suite indefinitely. This test patches subprocess.run to
    raise TimeoutExpired for every install attempt and asserts that the
    error tuple is returned cleanly (no exception propagates).
    """

    def test_pip_timeout_returns_error_tuple(self):
        saved_pyte = sys.modules.pop('pyte', None)
        saved_submods = {k: sys.modules.pop(k) for k in list(sys.modules)
                         if k.startswith('pyte.')}

        original_import = __builtins__['__import__'] if isinstance(__builtins__, dict) \
            else __builtins__.__import__

        def fake_import(name, *args, **kwargs):
            if name == 'pyte':
                raise ImportError("simulated: pyte not installed")
            return original_import(name, *args, **kwargs)

        def fake_run(cmd, **kwargs):
            raise subprocess.TimeoutExpired(cmd=cmd, timeout=kwargs.get('timeout', 120))

        try:
            with mock.patch.object(runner.subprocess, 'run', side_effect=fake_run), \
                    mock.patch('builtins.__import__', side_effect=fake_import):
                module, err = runner._ensure_pyte()
        finally:
            sys.modules.pop('pyte', None)
            if saved_pyte is not None:
                sys.modules['pyte'] = saved_pyte
            for k, v in saved_submods.items():
                sys.modules[k] = v

        self.assertIsNone(module,
                          "module must be None when every install attempt times out")
        self.assertIsNotNone(err,
                             "an error message must be returned, not propagated as exception")
        self.assertIn("timed out", err.lower(),
                      f"error message should mention timeout; got: {err!r}")


class ScreenshotMatchPathTraversalRejectedTest(unittest.TestCase):
    """screenshot_match must reject golden paths that escape golden_root (P2 #15).

    Exercised via execute_interactive_palette would require spawning a child,
    so this validates the realpath check directly. The check lives inline in
    execute_interactive_palette; we replicate it with the same os.path.realpath
    logic to ensure the boundary semantics behave as documented.
    """

    def test_relative_dotdot_escape_is_detected(self):
        # Mirror the check that runs inside execute_interactive_palette.
        with tempfile.TemporaryDirectory() as root:
            inside = os.path.join(root, 'fixtures', 'palette', 'ok.txt')
            outside_via_dotdot = os.path.join(root, '..', 'evil.txt')
            absolute_outside = os.path.realpath(os.path.join(root, '..', 'evil.txt'))

            def is_escape(path):
                resolved = os.path.realpath(path)
                root_real = os.path.realpath(root)
                return not (resolved == root_real
                            or resolved.startswith(root_real + os.sep))

            self.assertFalse(is_escape(inside),
                             "in-tree fixture path must be accepted")
            self.assertTrue(is_escape(outside_via_dotdot),
                            "../ escape must be rejected")
            self.assertTrue(is_escape(absolute_outside),
                            "absolute path outside root must be rejected")


if __name__ == "__main__":
    unittest.main()
