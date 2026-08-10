#!/usr/bin/env python3
"""
Frontier Integration Test Runner

Executes YAML test cases against frontier-cli and validates results.
"""

import argparse
import concurrent.futures
import difflib
import hashlib
import json
import logging
import multiprocessing
import os
import re
import select
import shutil
import subprocess
import sys
import tempfile
import time
from dataclasses import dataclass, field
from datetime import datetime, timezone
from pathlib import Path
from typing import Dict, List, Optional, Tuple

try:
    import yaml
except ImportError:
    print("Error: PyYAML is required. Install with: pip3 install pyyaml", file=sys.stderr)
    sys.exit(1)

try:
    import pexpect
    import ptyprocess
    HAS_PEXPECT = True
except ImportError:
    try:
        # Fall back to vendored copy. Both pexpect and ptyprocess are
        # vendored under tests/vendor/. We leave the vendor dir on
        # sys.path so subsequent late imports (e.g. ptyprocess inside
        # execute_interactive_palette's _PaletteSpawn) resolve cleanly.
        _VENDOR_PATH = os.path.join(os.path.dirname(__file__), '..', 'vendor')
        if _VENDOR_PATH not in sys.path:
            sys.path.insert(0, _VENDOR_PATH)
        import pexpect
        import ptyprocess
        HAS_PEXPECT = True
    except ImportError:
        HAS_PEXPECT = False

# pyte is a pip dependency (NOT vendored — it is LGPL-3.0, incompatible with
# Frontier's MIT-only vendoring policy per RELICENSING.md). The shim below
# auto-installs it on first use so palette_mode tests work on a fresh
# checkout with zero manual setup. Bootstrap is lazy: importing this module
# does NOT trigger an install — only execute_interactive_palette() and the
# palette_mode dispatcher branch call _ensure_pyte().


def _pyte_install_command() -> list:
    """Return the argv list used to install pyte via pip.

    Exposed as a pure function (no side effects) so tests can assert on the
    exact command shape without mocking subprocess. Uses the current Python
    interpreter via ``-m pip`` so the install lands in the same environment
    that's running the runner.
    """
    return [sys.executable, '-m', 'pip', 'install', '--user', 'pyte>=0.8.2']


def _pyte_install_command_break_system() -> list:
    """Return the PEP 668 escape-hatch argv for installing pyte.

    On Homebrew Python and Debian/Ubuntu system Python, PEP 668 marks the
    environment as "externally managed" and refuses `pip install --user`
    without `--break-system-packages`. Adding the flag is the documented
    workaround when a user-site install is genuinely intended.
    """
    return [sys.executable, '-m', 'pip', 'install', '--user',
            '--break-system-packages', 'pyte>=0.8.2']


def _ensure_pyte():
    """Import pyte, installing it via pip if necessary.

    Returns a ``(module, error)`` tuple:
      - ``(pyte_module, None)`` on success
      - ``(None, error_message)`` if import + install + reimport all fail

    The first call may run ``pip install`` as a subprocess (with progress
    logged to stderr); subsequent calls reuse the already-imported module.
    """
    try:
        import pyte  # noqa: F401
        return pyte, None
    except ImportError:
        pass

    # First fallback: vendored copy (legacy — kept defensive in case a user
    # restores tests/vendor/pyte locally). Normally this path is skipped.
    vendor_dir = os.path.join(os.path.dirname(__file__), '..', 'vendor')
    vendored_pyte = os.path.join(vendor_dir, 'pyte')
    if os.path.isdir(vendored_pyte):
        if vendor_dir not in sys.path:
            sys.path.insert(0, vendor_dir)
        try:
            import pyte  # noqa: F401
            return pyte, None
        except ImportError:
            pass

    # Auto-install via pip. Attempt sequence (each falls back to the next):
    #   1. python -m pip install --user pyte>=0.8.2
    #   2. python -m pip install --user --break-system-packages ...
    #      (PEP 668 escape hatch — Homebrew / Debian system Python)
    #   3. pip3 install --user --break-system-packages ...
    #      (minimal Python builds shipped without the pip module)
    def _try(cmd):
        try:
            # P1 #4: bound pip install at 120s so a wedged network mirror
            # or a stalled package index does not hang the whole test run
            # indefinitely. TimeoutExpired is caught alongside the other
            # subprocess failures so the install-failed branch fires
            # cleanly (returning a (None, error_message) tuple to the
            # caller via the surrounding _ensure_pyte loop).
            r = subprocess.run(cmd, capture_output=True, text=True,
                               timeout=120)
            if r.returncode == 0:
                return None
            return (f"'{' '.join(cmd)}' exited {r.returncode}:\n"
                    f"{r.stderr.strip()}")
        except subprocess.TimeoutExpired:
            return (f"'{' '.join(cmd)}' timed out after 120s "
                    "(network/mirror stall?)")
        except (OSError, subprocess.SubprocessError) as e:
            return f"failed to invoke {cmd[0]}: {e}"

    print("pyte not found - installing automatically via pip...",
          file=sys.stderr)

    attempts = [
        _pyte_install_command(),
        _pyte_install_command_break_system(),
        ['pip3', 'install', '--user', '--break-system-packages',
         'pyte>=0.8.2'],
    ]

    install_err = None
    for cmd in attempts:
        err = _try(cmd)
        if err is None:
            install_err = None
            break
        install_err = err
        print(f"install attempt failed: {err}", file=sys.stderr)
        print(f"retrying with next strategy...", file=sys.stderr)

    if install_err is not None:
        return None, (
            "pyte is required for palette_mode tests but could not be "
            f"auto-installed: {install_err}\n"
            "Run: pip3 install -r tests/requirements.txt"
        )

    # Reimport after install. Two extra steps beyond plain ``import pyte``:
    #   (a) ensure the user-site directory is on sys.path (it usually is
    #       already, but some embedded interpreters skip site init), and
    #   (b) invalidate import caches so the negative result from the
    #       earlier ImportError doesn't shadow the freshly installed pkg.
    import importlib
    import site
    try:
        user_site = site.getusersitepackages()
        if user_site and user_site not in sys.path:
            sys.path.insert(0, user_site)
    except (AttributeError, OSError):
        # P2 #16: only swallow the two failure modes site.getusersitepackages
        # can plausibly raise (missing attr on stripped-down interpreters;
        # filesystem error resolving the path). A broader except would
        # mask programmer errors in this bootstrap path.
        pass
    importlib.invalidate_caches()
    try:
        import pyte  # noqa: F401
        return pyte, None
    except ImportError as e:
        return None, (
            f"pyte was installed but cannot be imported: {e}\n"
            "Run: pip3 install -r tests/requirements.txt"
        )


def _normalize_screen_frame(screen) -> str:
    """Return a normalized text dump of a pyte.Screen.

    Normalization: strip trailing whitespace from each row (xterm pads rows
    with spaces to the full column width), then strip trailing blank rows
    from the whole frame. Lines are joined with '\\n' and a single trailing
    newline is appended so golden files have a stable, editor-friendly form.
    """
    rows = [row.rstrip() for row in screen.display]
    # Drop trailing blank rows so adding screen rows below the content
    # area doesn't churn goldens.
    while rows and rows[-1] == '':
        rows.pop()
    return '\n'.join(rows) + '\n'


def compare_screen_to_golden(screen, golden_path: str,
                             actual_dump_path: Optional[str] = None) -> Tuple[bool, Optional[str]]:
    """Compare a normalized pyte.Screen dump against a golden text fixture.

    Returns (passed, error_message). Honors FRONTIER_UPDATE_GOLDENS=1: when
    set, writes the current frame to ``golden_path`` (creating parent dirs
    as needed) and returns success without comparing.

    On mismatch, writes the actual frame to ``actual_dump_path`` (if given)
    and returns an error message containing a unified diff and the literal
    string "screenshot mismatch" so test reports surface the failure
    intent clearly.
    """
    actual = _normalize_screen_frame(screen)

    if os.environ.get('FRONTIER_UPDATE_GOLDENS') == '1':
        os.makedirs(os.path.dirname(os.path.abspath(golden_path)) or '.',
                    exist_ok=True)
        with open(golden_path, 'w', encoding='utf-8') as f:
            f.write(actual)
        # P1 #5: a loud, per-file stderr line so a developer who left
        # FRONTIER_UPDATE_GOLDENS=1 in their shell sees that goldens are
        # being rewritten on every run. Without this the overwrite is
        # silent and easy to commit accidentally.
        print(
            f"[palette-test] UPDATING GOLDEN: {golden_path} "
            "(FRONTIER_UPDATE_GOLDENS=1)",
            file=sys.stderr,
        )
        return True, None

    if not os.path.exists(golden_path):
        # Always dump actual when golden is missing — caller can inspect.
        if actual_dump_path:
            os.makedirs(os.path.dirname(os.path.abspath(actual_dump_path)) or '.',
                        exist_ok=True)
            with open(actual_dump_path, 'w', encoding='utf-8') as f:
                f.write(actual)
        return False, (
            f"screenshot mismatch: golden file not found at {golden_path!r} "
            f"(set FRONTIER_UPDATE_GOLDENS=1 to create it)"
        )

    with open(golden_path, 'r', encoding='utf-8') as f:
        expected = f.read()

    if actual == expected:
        return True, None

    # Dump actual for inspection.
    if actual_dump_path:
        os.makedirs(os.path.dirname(os.path.abspath(actual_dump_path)) or '.',
                    exist_ok=True)
        with open(actual_dump_path, 'w', encoding='utf-8') as f:
            f.write(actual)

    diff = ''.join(difflib.unified_diff(
        expected.splitlines(keepends=True),
        actual.splitlines(keepends=True),
        fromfile=f'{golden_path} (expected)',
        tofile='actual',
        n=3,
    ))
    return False, f"screenshot mismatch vs {golden_path}:\n{diff}"


# Type name aliases: Maps Frontier's internal type names to canonical test names
# Frontier uses shortened or internal names in JSON output that differ from
# the type names used in UserTalk and test expectations.
TYPE_ALIASES = {
    'addr': 'address',      # Address type
    'data': 'binary',       # Binary data type
    'fss ': 'filespec',     # Filespec type (note trailing space in Frontier output)
    'fss': 'filespec',      # Filespec type (without trailing space)
}


def normalize_type_name(type_name: Optional[str]) -> Optional[str]:
    """
    Normalize a Frontier type name to its canonical form.

    Args:
        type_name: Type name from Frontier JSON output

    Returns:
        Canonical type name, or original if no alias exists
    """
    if type_name is None:
        return None
    return TYPE_ALIASES.get(type_name, type_name)


class TestResult:
    """Result of a single test execution."""

    def __init__(self, name: str, passed: bool, error: Optional[str] = None, details: Optional[str] = None, skipped: bool = False):
        self.name = name
        self.passed = passed
        self.error = error
        self.details = details
        self.skipped = skipped


class FrontierCLI:
    """Wrapper for executing scripts via frontier-cli."""

    def __init__(self, cli_path: str, system_root: Optional[str] = None):
        self.cli_path = cli_path
        self.system_root = system_root

        if not os.path.exists(cli_path):
            raise FileNotFoundError(f"frontier-cli not found: {cli_path}")

    def execute(self, script: str, timeout: int = 10, stdin_input: Optional[str] = None,
                batch_mode: bool = False, env: Optional[Dict[str, str]] = None) -> Dict:
        """
        Execute a UserTalk script and return JSON result.

        Args:
            script: UserTalk script to execute
            timeout: Execution timeout in seconds
            stdin_input: Optional stdin input for interactive prompts
            batch_mode: If True, add --batch flag to disable interactive mode
            env: Optional environment variables to set
        """
        # Write script to temporary file to avoid shell quoting issues
        # Multi-line scripts with complex quoting don't work well with -e flag
        script_fd, script_path = tempfile.mkstemp(suffix='.usertalk', text=True)
        try:
            with os.fdopen(script_fd, 'w') as f:
                f.write(script)

            cmd = [self.cli_path, '--output-json', '--skip-startup', script_path]

            if self.system_root:
                cmd.extend(['--system-root', self.system_root])

            if batch_mode:
                cmd.append('--batch')

            # Merge environment variables with current environment.
            #
            # FRONTIER_LOCK_OPENED_ROOTS=1 (issue #127): treat the staged system root
            # and every loaded-from-disk guest DB as read-only. Tests stage
            # databases as disposable copies under tests/tmp/results/db/, so
            # disk persistence is never required across processes -- in-memory
            # mutations still evaluate as expected, only the save-on-exit is
            # suppressed. Without this, every CLI invocation rewrites the
            # staged Virgin.root and trips the drift-detection warning in
            # tools/run_integration_tests.sh (PR #555). Drift detection still
            # fires for tests that explicitly persist (fileMenu.save on guest
            # DBs etc.), just less aggressively.
            #
            # Tests that need to write to the system root must override by
            # passing env={'FRONTIER_LOCK_OPENED_ROOTS': '0'} on the execute() call.
            process_env = os.environ.copy()
            process_env.setdefault('FRONTIER_LOCK_OPENED_ROOTS', '1')
            if env:
                process_env.update(env)

            try:
                result = subprocess.run(
                    cmd,
                    capture_output=True,
                    text=True,
                    timeout=timeout,
                    input=stdin_input,  # Pass stdin input if provided
                    env=process_env
                )

                # Parse JSON from stdout (stderr contains prompts and logs)
                # When dialog prompts are active, stderr contains prompt output
                # stdout contains the clean JSON result
                try:
                    stdout_lines = result.stdout
                    # Find last occurrence of '{\n  "success"' which marks start of JSON
                    json_start = stdout_lines.rfind('{\n  "success"')
                    if json_start == -1:
                        # Fallback: try to parse entire stdout as JSON
                        json_text = stdout_lines
                    else:
                        json_text = stdout_lines[json_start:]

                    output = json.loads(json_text)
                    output['exit_code'] = result.returncode
                    output['stderr'] = result.stderr  # Preserve stderr for prompts/logs
                    return output
                except json.JSONDecodeError as e:
                    return {
                        'success': False,
                        'result': None,
                        'result_type': None,
                        'error': f'Invalid JSON output: {e}',
                        'error_type': 'json_parse_error',
                        'exit_code': result.returncode,
                        'stdout': result.stdout,
                        'stderr': result.stderr
                    }

            except subprocess.TimeoutExpired:
                return {
                    'success': False,
                    'result': None,
                    'result_type': None,
                    'error': f'Script execution timed out ({timeout}s)',
                    'error_type': 'timeout',
                    'exit_code': -1
                }

            except Exception as e:
                return {
                    'success': False,
                    'result': None,
                    'result_type': None,
                    'error': str(e),
                    'error_type': 'execution_error',
                    'exit_code': -1
                }
        finally:
            # Clean up temporary script file
            try:
                os.unlink(script_path)
            except OSError:
                pass  # Ignore file deletion errors only

    def execute_repl(self, stdin_input: str, timeout: int = 30, env: Optional[Dict[str, str]] = None) -> subprocess.CompletedProcess:
        """
        Execute frontier-cli in REPL mode with stdin input.

        Args:
            stdin_input: Input to pipe to REPL stdin
            timeout: Execution timeout in seconds (default 30)
            env: Optional environment variables to set

        Returns:
            CompletedProcess with stdout, stderr, and returncode
        """
        # Build command for REPL mode (no --output-json, no -e).
        # 2026-06-25 JES C.6 #691: pass --plain so the test exercises the
        # legacy linenoise REPL with its established stdin-line protocol
        # (typed commands + "/exit").  The boxen REPL (post-C.6 default)
        # uses a different I/O model that this runner can't drive with
        # pipe-fed stdin.  When linenoise is finally removed (post-soak),
        # the REPL session tests will need to be ported to the boxen
        # input model or replaced by tui-tests harness coverage.
        cmd = [self.cli_path, '--plain', '--skip-startup']

        if self.system_root:
            cmd.extend(['--system-root', self.system_root])

        # Merge environment variables with current environment.
        # FRONTIER_LOCK_OPENED_ROOTS=1: protect staged Virgin.root from drift; see
        # comment in execute() above. Caller can override with env arg.
        process_env = os.environ.copy()
        process_env.setdefault('FRONTIER_LOCK_OPENED_ROOTS', '1')
        # Force interactive mode for testing (ensures prompts are shown)
        process_env['FRONTIER_FORCE_INTERACTIVE'] = '1'
        if env:
            process_env.update(env)

        try:
            result = subprocess.run(
                cmd,
                input=stdin_input,
                capture_output=True,
                text=True,
                timeout=timeout,
                env=process_env
            )
            return result

        except subprocess.TimeoutExpired as e:
            # Create a fake CompletedProcess for timeout
            return subprocess.CompletedProcess(
                args=cmd,
                returncode=-1,
                stdout=e.stdout or '',
                stderr=f'REPL execution timed out ({timeout}s)'
            )

        except Exception as e:
            # Create a fake CompletedProcess for errors
            return subprocess.CompletedProcess(
                args=cmd,
                returncode=-1,
                stdout='',
                stderr=f'REPL execution error: {str(e)}'
            )

    def execute_interactive(self, interactive_steps: list, timeout: int = 30,
                            env: Optional[Dict[str, str]] = None) -> subprocess.CompletedProcess:
        """
        Execute frontier-cli interactively using pexpect PTY.

        Spawns the CLI in a pseudo-terminal so isatty() returns true,
        then uses expect/send pairs to drive interactive dialog prompts.

        Args:
            interactive_steps: List of dicts with 'expect' and 'send' keys
            timeout: Overall execution timeout in seconds
            env: Optional environment variables

        Returns:
            CompletedProcess-like object with stdout, stderr, returncode
        """
        if not HAS_PEXPECT:
            return subprocess.CompletedProcess(
                args=[self.cli_path],
                returncode=-1,
                stdout='',
                stderr='pexpect is not installed'
            )

        # Build command as argument list (avoids shell interpretation).
        # 2026-06-25 JES C.6 #691: --plain pins to the linenoise REPL.
        # FRONTIER_PLAIN_REPL (set below) is a *separate* env knob inside
        # linenoise that selects the blocking-read path; the CLI flag
        # selects WHICH REPL to launch.  Both are needed for pexpect-
        # driven interactive testing.
        cmd_args = [self.cli_path, '--plain', '--skip-startup']
        if self.system_root:
            cmd_args.extend(['--system-root', self.system_root])

        # Merge environment for clean PTY interaction:
        # - FRONTIER_PLAIN_REPL: Forces blocking REPL path (skips event loop)
        # - TERM=dumb: Makes linenoise use simple fgets() instead of raw mode
        #   editing, eliminating character-by-character echo and ANSI escapes
        # - FRONTIER_FORCE_INTERACTIVE: Ensures dialog verbs use stdio prompts
        # - FRONTIER_LOCK_OPENED_ROOTS=1 (issue #127): protect staged Virgin.root
        #   from drift; see comment in FrontierCLI.execute() above.
        process_env = os.environ.copy()
        process_env.setdefault('FRONTIER_LOCK_OPENED_ROOTS', '1')
        process_env['FRONTIER_PLAIN_REPL'] = '1'
        process_env['TERM'] = 'dumb'
        process_env['FRONTIER_FORCE_INTERACTIVE'] = '1'
        if env:
            process_env.update(env)

        collected_output = []
        try:
            child = pexpect.spawn(cmd_args[0], args=cmd_args[1:], timeout=timeout,
                                  env=process_env, encoding='utf-8')

            for step in interactive_steps:
                expect_pattern = step.get('expect', '')
                send_text = step.get('send', '')

                # Wait for expected pattern
                child.expect(expect_pattern, timeout=timeout)
                collected_output.append(child.before or '')
                collected_output.append(child.after or '')

                # Send response
                child.sendline(send_text)

            # Wait for process to finish
            child.expect(pexpect.EOF, timeout=timeout)
            collected_output.append(child.before or '')
            child.close()

            stdout = ''.join(collected_output)
            return subprocess.CompletedProcess(
                args=[self.cli_path],
                returncode=child.exitstatus or 0,
                stdout=stdout,
                stderr=''
            )

        except pexpect.TIMEOUT:
            try:
                child.close(force=True)
            except Exception:
                pass
            return subprocess.CompletedProcess(
                args=[self.cli_path],
                returncode=-1,
                stdout=''.join(collected_output),
                stderr=f'Interactive execution timed out ({timeout}s)'
            )
        except pexpect.EOF:
            stdout = ''.join(collected_output)
            try:
                child.close()
            except Exception:
                pass
            return subprocess.CompletedProcess(
                args=[self.cli_path],
                returncode=getattr(child, 'exitstatus', -1) or -1,
                stdout=stdout,
                stderr='Process exited unexpectedly'
            )
        except Exception as e:
            try:
                child.close(force=True)
            except Exception:
                pass
            return subprocess.CompletedProcess(
                args=[self.cli_path],
                returncode=-1,
                stdout=''.join(collected_output),
                stderr=f'Interactive execution error: {str(e)}'
            )


    def execute_interactive_palette(self, interactive_steps: list, timeout: int = 30,
                                    env: Optional[Dict[str, str]] = None,
                                    test_name: Optional[str] = None,
                                    results_dir: Optional[str] = None,
                                    golden_root: Optional[str] = None) -> subprocess.CompletedProcess:
        """
        Execute frontier-cli in palette mode for L4 screenshot-based testing.

        Parallel to execute_interactive() but tuned for tests that exercise
        the palette/REPL UI:
          - TERM=xterm-256color (preserves ANSI escapes for pyte to parse)
          - FRONTIER_PALETTE_FAST_TIMERS=1 (deterministic timing)
          - No FRONTIER_PLAIN_REPL — full event loop is needed for palette
          - dimensions=(24, 80) on the pty for stable geometry

        Each step is a dict with one of these shapes:
          - {'expect': '<pattern>', 'send': '<text>'}   (plain pexpect step; sendline appends '\n')
          - {'send': '<text>'}                          (send-only; sendline appends '\n')
          - {'send_raw': '<bytes>'}                     (raw send; NO trailing newline)
          - {'expect': '<pattern>', 'send_raw': '<bytes>'}
          - {'screenshot_match': '<golden-path>'}       (drain + compare via pyte)

        Use ``send_raw`` for single keystrokes or escape sequences (e.g. '/',
        '\\x1b', arrow keys). ``send`` is for typing whole commands that end
        with Enter (e.g. '/exit').

        The screenshot_match step drains pending pty bytes (bounded read loop:
        2 consecutive empty reads OR 500ms total), feeds them through the
        per-session pyte Stream+Screen, normalizes the frame, and compares
        against the golden file. On mismatch the actual frame is dumped to
        ``results_dir/<slug>.actual`` and the failure is recorded in stderr.
        """
        if not HAS_PEXPECT:
            return subprocess.CompletedProcess(
                args=[self.cli_path],
                returncode=-1,
                stdout='',
                stderr='pexpect is not installed (required for palette tests)',
            )
        # Lazy bootstrap: import (and pip-install if missing) pyte at first
        # palette-mode invocation. Avoids slowing down the 99% of test runs
        # that never touch palette_mode.
        pyte, pyte_err = _ensure_pyte()
        if pyte is None:
            return subprocess.CompletedProcess(
                args=[self.cli_path],
                returncode=-1,
                stdout='',
                stderr=pyte_err or 'pyte is not installed (required for palette tests)',
            )

        # 2026-06-25 JES C.6 #691: --plain pins to the linenoise REPL.
        # The palette-mode tests are exercising the LINENOISE termbox2
        # palette overlay (palette.c).  Boxen has its own palette modal
        # tested separately by the tui-tests harness.
        cmd_args = [self.cli_path, '--plain', '--skip-startup']
        if self.system_root:
            cmd_args.extend(['--system-root', self.system_root])

        # Palette-mode environment (DIFFERENT from execute_interactive):
        # - TERM=xterm-256color: pyte expects xterm-family escapes
        # - FRONTIER_PALETTE_FAST_TIMERS=1: collapse cursor-blink / debounce
        #   timers so screen state stabilizes quickly
        # - NO FRONTIER_PLAIN_REPL: we need the real event loop
        # - NO TERM=dumb: linenoise raw mode must be active to emit the
        #   sequences we want to test
        process_env = os.environ.copy()
        process_env.setdefault('FRONTIER_LOCK_OPENED_ROOTS', '1')
        process_env['TERM'] = 'xterm-256color'
        process_env['FRONTIER_PALETTE_FAST_TIMERS'] = '1'
        process_env['FRONTIER_FORCE_INTERACTIVE'] = '1'
        if env:
            process_env.update(env)

        # Per-session pyte pair. 24 rows x 80 cols matches dimensions= below.
        screen = pyte.Screen(80, 24)
        stream = pyte.Stream(screen)

        collected_output: list = []
        screenshot_failures: list = []

        def drain_pty(child) -> str:
            """Read pending bytes until 2 consecutive empty reads or 500ms.

            Returns the concatenated string (may be empty). Empty reads are
            detected via pexpect.TIMEOUT with a short per-read timeout.
            """
            import time as _time
            chunks: list = []
            empty_in_a_row = 0
            deadline = _time.monotonic() + 0.5
            while _time.monotonic() < deadline and empty_in_a_row < 2:
                try:
                    data = child.read_nonblocking(size=4096, timeout=0.05)
                except pexpect.TIMEOUT:
                    empty_in_a_row += 1
                    continue
                except pexpect.EOF:
                    break
                if data:
                    chunks.append(data)
                    empty_in_a_row = 0
                else:
                    empty_in_a_row += 1
            return ''.join(chunks)

        # Initialize child = None so the except handlers below can safely
        # reference it even if pexpect.spawn() itself raises before
        # `child` would otherwise be bound (P1 #6).
        child = None
        # Redirect the CHILD's stderr away from the PTY so frontier-cli's
        # diagnostic stderr lines (notably the one-time
        # FRONTIER_PALETTE_FAST_TIMERS=1 notice from run_palette_modal)
        # do not enter pyte's framebuffer and contaminate goldens (P0 #1).
        # The child still writes diagnostics — they go to the runner's own
        # stderr, which the developer sees in their terminal — but the
        # captured screen frame is just menubar + cursor state.
        #
        # Implementation: duplicate the runner's stderr fd in the parent
        # BEFORE fork; pass it through to the child via ptyprocess'
        # pass_fds (so its close-all-but-3 loop spares it); then use a
        # preexec_fn to dup2 it over fd 2 in the child. Without pass_fds
        # the duplicate fd is closed before preexec_fn runs and the
        # dup2 falls back silently — leaving the slave PTY on fd 2.
        try:
            stderr_dup_fd = os.dup(2)
        except OSError:
            stderr_dup_fd = None

        def _redirect_child_stderr():
            # Runs in the child between fork and exec. The slave PTY is
            # already on fd 2 at this point; replace it with the parent's
            # real stderr so the captured PTY contains only stdout.
            if stderr_dup_fd is not None:
                try:
                    os.dup2(stderr_dup_fd, 2)
                except OSError:
                    pass

        # Subclass pexpect.spawn locally so we can thread pass_fds through
        # to ptyprocess.PtyProcess.spawn(). Pexpect's stock spawn does not
        # expose pass_fds in its public API.
        class _PaletteSpawn(pexpect.spawn):
            _pass_fds = (
                (stderr_dup_fd,) if stderr_dup_fd is not None else ()
            )

            def _spawnpty(self, args, **kwargs):
                kwargs.setdefault('pass_fds', self._pass_fds)
                return ptyprocess.PtyProcess.spawn(args, **kwargs)

        try:
            child = _PaletteSpawn(cmd_args[0], args=cmd_args[1:], timeout=timeout,
                                  env=process_env, encoding='utf-8',
                                  dimensions=(24, 80),
                                  preexec_fn=_redirect_child_stderr)

            # Close the parent's duplicate stderr fd now that the child has
            # inherited it. Parent fd 2 still points to the runner's stderr.
            if stderr_dup_fd is not None:
                try:
                    os.close(stderr_dup_fd)
                except OSError:
                    pass
                stderr_dup_fd = None

            for step in interactive_steps:
                if 'screenshot_match' in step:
                    # Drain pending bytes, then compare.
                    pending = drain_pty(child)
                    if pending:
                        collected_output.append(pending)
                        stream.feed(pending)
                    golden_path = step['screenshot_match']
                    # Resolve relative golden paths against golden_root (typically
                    # the project root). Lets YAML use stable repo-relative paths
                    # like "tests/fixtures/palette/foo.txt" regardless of which
                    # directory the runner is invoked from.
                    if golden_root and not os.path.isabs(golden_path):
                        golden_path = os.path.join(golden_root, golden_path)
                    # P2 #15: reject paths that escape golden_root via ".."
                    # or absolute paths pointing outside the tree. With
                    # FRONTIER_UPDATE_GOLDENS=1 a hostile YAML could otherwise
                    # overwrite arbitrary files; even read-only matches
                    # against /etc/passwd are pointless surface area.
                    if golden_root:
                        resolved = os.path.realpath(golden_path)
                        root_real = os.path.realpath(golden_root)
                        if not (resolved == root_real
                                or resolved.startswith(root_real + os.sep)):
                            screenshot_failures.append(
                                "screenshot_match path escapes golden_root: "
                                f"{step['screenshot_match']!r} -> {resolved!r}"
                            )
                            continue
                    actual_dump_path = None
                    if results_dir is not None:
                        slug = (test_name or 'palette_test').replace('/', '_').replace(' ', '_')
                        actual_dump_path = os.path.join(
                            results_dir, 'palette', f'{slug}.actual')
                    ok, err = compare_screen_to_golden(
                        screen=screen,
                        golden_path=golden_path,
                        actual_dump_path=actual_dump_path,
                    )
                    if not ok:
                        screenshot_failures.append(err or 'screenshot mismatch')
                    continue

                # {'delay': <seconds>} -- pause between steps. Used to space
                # out palette keystrokes so each escape sequence is fully
                # processed (and rendered) before the next arrives. The
                # palette modal's ESC parser uses a 1ms fast-timer under
                # FRONTIER_PALETTE_FAST_TIMERS=1, so back-to-back arrow
                # sequences can race the parser; a small inter-keystroke
                # delay lets the modal's poll loop drain + redraw between
                # them. May be combined with a send_raw in the same step
                # (delay runs AFTER the send).
                delay = step.get('delay')

                expect_pattern = step.get('expect')
                send_text = step.get('send')
                send_raw = step.get('send_raw')

                if expect_pattern:
                    child.expect(expect_pattern, timeout=timeout)
                    before = child.before or ''
                    after = child.after or ''
                    collected_output.append(before)
                    collected_output.append(after)
                    if before:
                        stream.feed(before)
                    if after:
                        stream.feed(after)

                # send_raw takes precedence: raw byte send with no trailing newline
                # (for single keystrokes like '/', ESC, arrow keys). send uses
                # sendline which appends '\n' (for typing whole commands).
                if send_raw is not None:
                    child.send(send_raw)
                elif send_text is not None:
                    child.sendline(send_text)

                # Honor an explicit inter-step delay (after any send): SLEEP
                # first so the palette modal's poll loop (10ms period) has
                # time to consume the keystroke and emit its redraw, THEN
                # drain the pty so that redraw reaches pyte. Order matters --
                # draining before the modal has rendered would capture a stale
                # frame and the post-keystroke redraw would back up unread
                # until the next screenshot_match. The harness otherwise only
                # reads the pty at screenshot_match steps, so without this the
                # final captured frame shows the menubar but not the
                # drilled-in dropdown (the navigation redraws were never read).
                # Draining here keeps the pyte framebuffer current
                # step-by-step, mirroring a real terminal that paints
                # continuously.
                if delay is not None:
                    import time as _time
                    _time.sleep(float(delay))
                    pending = drain_pty(child)
                    if pending:
                        collected_output.append(pending)
                        stream.feed(pending)

            # Drain any final output.
            try:
                child.expect(pexpect.EOF, timeout=min(timeout, 2))
                tail = child.before or ''
                if tail:
                    collected_output.append(tail)
                    stream.feed(tail)
            except (pexpect.TIMEOUT, pexpect.EOF):
                pass
            try:
                child.close()
            except Exception:
                pass

            stderr = '\n'.join(screenshot_failures) if screenshot_failures else ''
            returncode = (child.exitstatus or 0) if not screenshot_failures else 1
            return subprocess.CompletedProcess(
                args=[self.cli_path],
                returncode=returncode,
                stdout=''.join(collected_output),
                stderr=stderr,
            )

        except pexpect.TIMEOUT:
            # P1 #6: child may be None if pexpect.spawn() itself raised.
            if child is not None:
                try:
                    child.close(force=True)
                except Exception:
                    pass
            if stderr_dup_fd is not None:
                try:
                    os.close(stderr_dup_fd)
                except OSError:
                    pass
            return subprocess.CompletedProcess(
                args=[self.cli_path],
                returncode=-1,
                stdout=''.join(collected_output),
                stderr=f'Palette interactive execution timed out ({timeout}s)',
            )
        except Exception as e:
            # P1 #6: child may be None if pexpect.spawn() itself raised.
            if child is not None:
                try:
                    child.close(force=True)
                except Exception:
                    pass
            if stderr_dup_fd is not None:
                try:
                    os.close(stderr_dup_fd)
                except OSError:
                    pass
            return subprocess.CompletedProcess(
                args=[self.cli_path],
                returncode=-1,
                stdout=''.join(collected_output),
                stderr=f'Palette interactive execution error: {e}',
            )


class ProtocolExecutor:
    """
    Manages a persistent frontier-cli --protocol subprocess for batch test execution.

    Instead of spawning a new process per test (~210ms startup), keeps one process
    alive and communicates via NDJSON over stdin/stdout. Call reset() between tests
    to clear state.
    """

    def __init__(self, cli_path: str, system_root: Optional[str] = None):
        self.cli_path = cli_path
        self.system_root = system_root
        self._proc: Optional[subprocess.Popen] = None
        self._stderr_file = None
        self._env_overrides: Optional[Dict[str, str]] = None
        self._next_id = 1
        # Server-initiated notification lines (id:null + op field, e.g.
        # debug/suspended, debug/completed) buffered here when they arrive
        # interleaved with request/response traffic. See _read_message().
        self._notifications: List[dict] = []
        # Raw bytes read from the process but not yet consumed as lines.
        # We do our own line buffering over os.read() because mixing
        # select() with a buffered readline() loses lines: when two lines
        # arrive in one chunk, readline() buffers the second and select()
        # never reports the fd ready for it.
        self._read_buf = b''

    def start(self, env_overrides: Optional[Dict[str, str]] = None):
        """Spawn the frontier-cli --protocol subprocess.

        Protocol mode now uses legacy default-RW (issue #127): tests can
        call fileMenu.save(), workspace assignments, etc. without opting
        in. Integration tests run against staged copies under
        tests/tmp/results/db/ — mutating those copies is intentional, the
        staging layer (tools/run_integration_tests.sh) treats them as
        disposable. Tests that must NOT persist on-disk should set
        FRONTIER_LOCK_OPENED_ROOTS=1 in their env (or rely on the env
        defaults set by FrontierCLI.execute()).

        env_overrides: per-instance environment overrides. Used by
        run_protocol_test() to spawn a dedicated executor for protocol_ops
        tests whose YAML declares an `environment:` block (e.g.
        FRONTIER_LOCK_OPENED_ROOTS=0 to enable on-disk saves).
        """
        cmd = [self.cli_path, '--protocol', '--skip-startup']
        if self.system_root:
            cmd.extend(['--system-root', self.system_root])

        # Remember overrides so _restart() / restart_for_test() respawn with
        # the SAME environment. Before Unit 1.2 a restart silently dropped
        # per-test overrides (e.g. FRONTIER_LOCK_OPENED_ROOTS=0), flipping a
        # dedicated executor back to the locked default mid-test.
        self._env_overrides = dict(env_overrides) if env_overrides else None

        # Redirect stderr to temp file to capture crash diagnostics
        # (using a file avoids pipe buffer deadlocks)
        self._stderr_file = tempfile.NamedTemporaryFile(
            mode='w+', prefix='frontier_protocol_stderr_', suffix='.log', delete=False)

        # Merge environment to inject FRONTIER_LOCK_OPENED_ROOTS=1 (issue #127):
        # treat the staged system root and every loaded-from-disk guest DB as
        # read-only for the long-lived protocol batch process. Mirrors the
        # FrontierCLI.execute() / execute_repl() / execute_interactive() pattern.
        # Tests that need to write to the system root carry an explicit
        # `environment:` block in YAML; script-mode tests are forced out of
        # the protocol batch by _is_protocol_compatible() and run via the
        # per-process executor. protocol_ops tests with an environment block
        # use a dedicated short-lived executor (see run_protocol_test()).
        process_env = os.environ.copy()
        process_env.setdefault('FRONTIER_LOCK_OPENED_ROOTS', '1')
        if env_overrides:
            process_env.update(env_overrides)

        try:
            self._proc = subprocess.Popen(
                cmd,
                stdin=subprocess.PIPE,
                stdout=subprocess.PIPE,
                stderr=self._stderr_file,
                text=True,
                bufsize=1,  # Line buffered
                env=process_env,
            )
        except Exception:
            self._close_stderr_file()
            raise
        self._next_id = 1
        self._read_buf = b''

    def _send_recv(self, msg: dict, timeout: float = 10.0) -> dict:
        """Send a JSON message and read the JSON response line."""
        if self._proc is None or self._proc.poll() is not None:
            raise RuntimeError("Protocol process not running")

        msg_id = self._next_id
        self._next_id += 1
        msg['id'] = msg_id

        line = json.dumps(msg, separators=(',', ':'), ensure_ascii=False) + '\n'
        try:
            self._proc.stdin.write(line)
            self._proc.stdin.flush()
        except BrokenPipeError:
            raise RuntimeError("Protocol process died unexpectedly")

        resp = self._read_message(timeout)

        # Verify response ID matches
        if resp.get('id') != msg_id:
            raise RuntimeError(f"Protocol ID mismatch: sent {msg_id}, got {resp.get('id')}")

        return resp

    def _read_raw_line(self, deadline: float) -> str:
        """Read one complete line from the process stdout.

        Does its own byte-level line buffering over os.read() on the raw
        fd (never the TextIOWrapper's readline) so that select() and line
        availability agree. Raises TimeoutError at `deadline`, RuntimeError
        on EOF.
        """
        fd = self._proc.stdout.fileno()
        while b'\n' not in self._read_buf:
            remaining = deadline - time.time()
            if remaining <= 0:
                raise TimeoutError("Protocol response timed out")
            rlist, _, _ = select.select([fd], [], [], remaining)
            if not rlist:
                raise TimeoutError("Protocol response timed out")
            chunk = os.read(fd, 65536)
            if chunk == b'':
                raise RuntimeError("Protocol process closed stdout (EOF)")
            self._read_buf += chunk
        line, self._read_buf = self._read_buf.split(b'\n', 1)
        return line.decode('utf-8', errors='replace')

    def _read_message(self, timeout: float = 10.0) -> dict:
        """Read the next non-notification message line from the process.

        Server-initiated notifications ({"id":null,"op":...,"params":...},
        e.g. debug/suspended) can arrive interleaved with request/response
        traffic. They are buffered into self._notifications and skipped so
        request/response correlation stays intact. Responses with id:null
        but NO op field (envelope-level errors for unparseable requests)
        are returned as normal responses.
        """
        deadline = time.time() + timeout
        while True:
            try:
                resp_line = self._read_raw_line(deadline)
            except TimeoutError:
                raise TimeoutError(f"Protocol response timed out after {timeout}s")

            if resp_line.strip() == '':
                continue

            try:
                resp = json.loads(resp_line)
            except json.JSONDecodeError as e:
                raise RuntimeError(f"Invalid JSON from protocol process: {resp_line!r}: {e}")

            if isinstance(resp, dict) and resp.get('id') is None and 'op' in resp:
                self._notifications.append(resp)
                continue

            return resp

    def send_raw_line(self, raw_line: str, timeout: float = 10.0) -> Optional[dict]:
        """Send a literal line (possibly invalid JSON) and read one response.

        Used by envelope contract tests (unparseable JSON, missing id,
        oversized lines). Returns the parsed response dict, or None if no
        response arrived within `timeout` (documents silent-drop behavior).
        """
        if self._proc is None or self._proc.poll() is not None:
            raise RuntimeError("Protocol process not running")

        try:
            self._proc.stdin.write(raw_line + '\n')
            self._proc.stdin.flush()
        except BrokenPipeError:
            raise RuntimeError("Protocol process died unexpectedly")

        try:
            return self._read_message(timeout)
        except TimeoutError:
            return None

    def wait_notification(self, op_name: str, timeout: float = 10.0) -> dict:
        """Wait for a server-initiated notification with the given op.

        Checks buffered notifications first, then reads from the pipe.
        Non-matching notifications stay buffered in arrival order. A
        non-notification line while waiting is a protocol violation
        (no request is in flight) and raises RuntimeError.
        """
        for i, note in enumerate(self._notifications):
            if note.get('op') == op_name:
                return self._notifications.pop(i)

        deadline = time.time() + timeout
        while True:
            try:
                resp_line = self._read_raw_line(deadline)
            except TimeoutError:
                raise TimeoutError(f"Timed out waiting for notification op={op_name}")

            if resp_line.strip() == '':
                continue

            try:
                resp = json.loads(resp_line)
            except json.JSONDecodeError as e:
                raise RuntimeError(f"Invalid JSON from protocol process: {resp_line!r}: {e}")

            if isinstance(resp, dict) and resp.get('id') is None and 'op' in resp:
                if resp.get('op') == op_name:
                    return resp
                self._notifications.append(resp)
                continue

            raise RuntimeError(
                f"Unexpected non-notification line while waiting for {op_name}: {resp!r}")

    def _restart(self):
        """Restart the protocol process after it dies."""
        self._log_stderr_on_crash()
        try:
            if self._proc is not None:
                self._proc.kill()
                self._proc.wait(timeout=2)
        except Exception:
            pass
        self._proc = None
        self._close_stderr_file()
        try:
            self.start(env_overrides=self._env_overrides)
        except Exception as e:
            raise RuntimeError(f"Protocol executor restart failed: {e}") from e

    def restart_for_test(self, mode: str = 'kill'):
        """Deliberately restart the subprocess against the same system root
        and environment (Unit 1.2 restart-then-verify tests).

        mode 'kill': SIGKILL the process -- models abnormal termination
            (crash, power loss). The on-exit save path never runs, so
            unsaved in-memory mutations are lost. This is the data-loss
            scenario odb/save exists to close.
        mode 'shutdown': protocol shutdown op + clean exit -- the CLI's
            exit path runs, which (issue #127) saves a read-write system
            root on the way out. Pins the legacy save-on-exit contract.
        """
        if self._proc is not None:
            if mode == 'shutdown':
                try:
                    self._send_recv({'op': 'shutdown'}, timeout=30)
                except Exception as e:
                    raise RuntimeError(f"shutdown op failed during restart: {e}") from e
                try:
                    self._proc.wait(timeout=30)
                except subprocess.TimeoutExpired:
                    raise RuntimeError("process did not exit after shutdown op")
            elif mode == 'kill':
                self._proc.kill()
                try:
                    self._proc.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    pass
            else:
                raise RuntimeError(f"unknown restart_executor mode: {mode!r}")
        self._proc = None
        self._close_stderr_file()
        self._notifications = []
        self.start(env_overrides=self._env_overrides)

    def _log_stderr_on_crash(self):
        """Read and log any stderr output from the protocol process."""
        if self._stderr_file is None:
            return
        try:
            self._stderr_file.flush()
            self._stderr_file.seek(0)
            stderr_content = self._stderr_file.read().strip()
            if stderr_content:
                if len(stderr_content) <= 1000:
                    print(f"  [protocol stderr] {stderr_content}", file=sys.stderr)
                else:
                    # Show head and tail to capture both startup errors and crash traces
                    print(f"  [protocol stderr head] {stderr_content[:500]}", file=sys.stderr)
                    print(f"  [protocol stderr tail] {stderr_content[-500:]}", file=sys.stderr)
        except OSError:
            pass

    def _close_stderr_file(self):
        """Close and remove the stderr temp file."""
        if self._stderr_file is None:
            return
        try:
            name = self._stderr_file.name
            self._stderr_file.close()
            os.unlink(name)
        except Exception as ex:
            print(f"  [protocol] failed to close stderr file: {ex}", file=sys.stderr)
        self._stderr_file = None

    def execute(self, script: str, timeout: float = 10.0) -> Dict:
        """
        Evaluate a UserTalk script via the protocol and return a result dict
        compatible with FrontierCLI.execute() output format.
        """
        try:
            resp = self._send_recv({
                'op': 'script/eval',
                'params': {'expression': script}
            }, timeout=timeout)
        except TimeoutError:
            return {
                'success': False,
                'result': None,
                'result_type': None,
                'error': f'Script execution timed out ({timeout}s)',
                'error_type': 'timeout',
                'exit_code': -1,
            }
        except RuntimeError as e:
            # Process died — try to restart for subsequent tests
            try:
                self._restart()
            except Exception:
                pass
            return {
                'success': False,
                'result': None,
                'result_type': None,
                'error': str(e),
                'error_type': 'execution_error',
                'exit_code': -1,
            }

        if resp.get('success'):
            result_obj = resp.get('result', {})
            value = result_obj.get('value') if isinstance(result_obj, dict) else result_obj
            # Match --output-json behavior: result_type is always "string" since
            # the C side coerces values to string for the JSON response.
            # The actual runtime type is available in result_obj['type'] if needed.
            return {
                'success': True,
                'result': value,
                'result_type': 'string' if value is not None else None,
                'error': None,
                'error_type': None,
                'exit_code': 0,
            }
        else:
            error_obj = resp.get('error', {})
            return {
                'success': False,
                'result': None,
                'result_type': None,
                'error': error_obj.get('message') if isinstance(error_obj, dict) else str(error_obj),
                'error_type': 'script_error',
                'exit_code': 1,
            }

    def send_raw(self, msg: dict, timeout: float = 10.0) -> dict:
        """
        Send a raw NDJSON message and return the raw response dict.
        Unlike execute(), this does NOT normalize the response — the caller
        gets the exact JSON response from the protocol process.
        """
        return self._send_recv(msg, timeout=timeout)

    def reset(self):
        """Clear REPL variables and reset focus between tests."""
        self._notifications.clear()
        if self._proc is None or self._proc.poll() is not None:
            # Process already dead — restart for subsequent tests.
            # No clearContext needed after restart (fresh process has clean state).
            try:
                self._restart()
            except Exception as e:
                print(f"  [protocol] restart after death failed: {e}", file=sys.stderr)
            return
        try:
            self._send_recv({'op': 'script/clearContext'}, timeout=5.0)
        except Exception:
            # clearContext failed — process may have died, restart
            try:
                self._restart()
            except Exception as e:
                print(f"  [protocol] restart after clearContext failure: {e}", file=sys.stderr)

    def stop(self):
        """Gracefully shut down the protocol process."""
        if self._proc is None:
            self._close_stderr_file()
            return
        if self._proc.poll() is not None:
            self._proc = None
            self._close_stderr_file()
            return
        try:
            self._send_recv({'op': 'shutdown'}, timeout=5.0)
        except Exception:
            pass
        try:
            self._proc.wait(timeout=5.0)
        except subprocess.TimeoutExpired:
            self._proc.kill()
            self._proc.wait()
        self._proc = None
        self._close_stderr_file()

    @property
    def is_alive(self) -> bool:
        return self._proc is not None and self._proc.poll() is None


# Deprecated: Hardcoded per-file behavior sets.
# Prefer YAML-level metadata flags (sequential, needs_guest_dbs, protocol_mode).
# These sets are kept as fallback for files that haven't been updated yet.
SEQUENTIAL_TEST_FILES = {
    'tcp_verbs_network.yaml',
    'dialog_verbs.yaml',
    'file_dialog_verbs.yaml',
    # Note: webserver_verbs.yaml was listed here but never existed.
    # webserver_hello_world.yaml and webserver_http_roundtrip_tests.yaml
    # now use the sequential: true YAML flag instead.
}

NON_PROTOCOL_TEST_FILES = {
    'window_verbs.yaml',
}

# Legacy set — replaced by needs_guest_dbs YAML flag.
NEEDS_GUEST_DBS = {
    'guest_db_externals.yaml',
    'efptable_stability.yaml',
}


def load_file_metadata(yaml_path: str) -> dict:
    """Read file-level metadata from a YAML test file.

    Supported keys (all optional):
      sequential: bool      - Run in main process, not parallel worker (default: false)
      needs_guest_dbs: bool - Copy sibling .root files + Guest Databases/ to worker (default: false)
      protocol_mode: bool   - Use NDJSON protocol executor (default: true)

    ALL flags are FILE-LEVEL only. They MUST appear at the YAML root,
    not inside a `tests:` entry. A per-test `sequential: true` (or any
    of the other flags) parses without YAML error but is silently
    IGNORED -- the runner buckets the whole file as parallel-eligible.
    PR #709 (#690) hit this: a per-test sequential flag let a TCP
    listener race against other workers. If a test needs port-binding
    isolation, promote the flag to file root (and serialize the other
    tests in the same file as a side effect).
    """
    try:
        with open(yaml_path, encoding='utf-8') as f:
            data = yaml.safe_load(f)
        if not isinstance(data, dict):
            return {}
        return {
            'sequential': data.get('sequential', False),
            'needs_guest_dbs': data.get('needs_guest_dbs', False),
            'protocol_mode': data.get('protocol_mode', True),
        }
    except yaml.YAMLError as e:
        print(f"Warning: failed to parse YAML metadata from {yaml_path}: {e}",
              file=sys.stderr)
        return {}


def _is_protocol_compatible(test: 'TestCase') -> bool:
    """Check if a test can run via the NDJSON protocol executor."""
    # Interactive tests use pexpect PTY — not compatible with protocol mode
    if test.interactive_steps:
        return False
    # REPL-mode tests need custom stdin sequences — must use per-process executor
    if test.repl_mode:
        return False
    # Tests with custom stdin_input need per-process executor
    if test.stdin_input is not None:
        return False
    # Tests that set batch_mode need the --batch flag on the process
    if test.batch_mode:
        return False
    # Tests with custom environment variables need per-process executor
    if test.environment:
        return False
    return True


@dataclass
class WorkerResult:
    """Serializable result from a parallel worker."""
    file_name: str
    results: List[dict] = field(default_factory=list)  # List of serialized TestResult dicts


def _serialize_test_result(r: TestResult) -> dict:
    return {
        'name': r.name,
        'passed': r.passed,
        'error': r.error,
        'details': r.details,
        'skipped': r.skipped,
    }


def _deserialize_test_result(d: dict) -> TestResult:
    return TestResult(
        name=d['name'],
        passed=d['passed'],
        error=d.get('error'),
        details=d.get('details'),
        skipped=d.get('skipped', False),
    )


def _run_file_worker(args: tuple) -> dict:
    """
    Worker function for parallel execution. Runs all tests in one YAML file.
    Uses ProtocolExecutor for compatible tests, falls back to FrontierCLI for others.
    Returns a serializable dict (for multiprocessing).
    """
    (yaml_path, cli_path, system_root, test_root_dir, protocol_batch, verbose,
     worker_id, selectors) = args

    # Load file-level metadata (sequential, needs_guest_dbs, protocol_mode)
    meta = load_file_metadata(yaml_path)

    # Set up per-worker temp dir
    worker_tmp = os.path.join(test_root_dir, 'tmp', 'integration', f'worker_{worker_id}')
    os.makedirs(worker_tmp, exist_ok=True)

    # Each worker gets its own copy of the system root database so that
    # multiple frontier-cli processes don't open the same file with write
    # permissions simultaneously (which causes locking/corruption).
    worker_system_root = system_root
    if system_root and os.path.isfile(system_root):
        worker_db_path = os.path.join(worker_tmp, os.path.basename(system_root))
        shutil.copy2(system_root, worker_db_path)
        worker_system_root = worker_db_path

        # Copy sibling .root files and Guest Databases/ for tests that
        # open guest databases. Controlled by needs_guest_dbs YAML flag
        # (with hardcoded NEEDS_GUEST_DBS set as deprecated fallback).
        if meta.get('needs_guest_dbs', False) or os.path.basename(yaml_path) in NEEDS_GUEST_DBS:
            src_dir = os.path.dirname(system_root)
            # Copy sibling .root files (StartupTasks.root, test.root, etc.)
            for sibling in os.listdir(src_dir):
                if sibling.endswith('.root') and sibling != os.path.basename(system_root):
                    sibling_src = os.path.join(src_dir, sibling)
                    if os.path.isfile(sibling_src):
                        shutil.copy2(sibling_src, os.path.join(worker_tmp, sibling))
            # Copy Guest Databases/ directory tree
            guest_db_dir = os.path.join(src_dir, 'Guest Databases')
            if os.path.isdir(guest_db_dir):
                worker_guest_dir = os.path.join(worker_tmp, 'Guest Databases')
                if os.path.isdir(worker_guest_dir):
                    shutil.rmtree(worker_guest_dir)
                shutil.copytree(guest_db_dir, worker_guest_dir)

    cli = FrontierCLI(cli_path, worker_system_root)

    try:
        file_name = Path(yaml_path).name
        executor = None
        use_protocol = meta.get('protocol_mode', True) and file_name not in NON_PROTOCOL_TEST_FILES
        if protocol_batch and use_protocol:
            executor = ProtocolExecutor(cli_path, worker_system_root)
            try:
                executor.start()
            except Exception as e:
                print(f"  Warning: Failed to start protocol executor for worker {worker_id}: {e}",
                      file=sys.stderr)
                executor = None

        runner = TestRunner(cli, verbose=verbose, test_root_dir=test_root_dir,
                            protocol_executor=executor)

        with open(yaml_path, 'r') as f:
            data = yaml.safe_load(f)

        test_dicts = data.get('tests', [])
        if selectors:
            test_dicts = [td for td in test_dicts
                          if td.get('name', 'Unnamed Test') in selectors]
        test_cases = [TestCase(td) for td in test_dicts]
        results = []

        for test in test_cases:
            result = runner.run_test(test)
            results.append(_serialize_test_result(result))

        return {
            'file_name': Path(yaml_path).name,
            'results': results,
        }
    finally:
        # Stop executor before cleaning up temp files
        if executor is not None:
            try:
                executor.stop()
            except Exception:
                pass
        shutil.rmtree(worker_tmp, ignore_errors=True)


class TestCase:
    """Represents a single test case from YAML."""

    def __init__(self, data: Dict):
        self.name = data.get('name', 'Unnamed Test')
        self.script = data.get('script', '')
        self.expected_success = data.get('expected_success', True)
        self.expected_result = data.get('expected_result') or data.get('expected_output')  # Support both names
        self.expected_result_type = data.get('expected_result_type')
        self.expected_error_type = data.get('expected_error_type')
        self.expected_contains = data.get('expected_contains')  # List of strings that should be in result
        self.expected_pattern = data.get('expected_pattern')    # Regex pattern — uses re.fullmatch (exact match; use .* for partial)
        self.expected_error_contains = data.get('expected_error_contains')  # String that should be in error
        self.description = data.get('description', '')
        self.timeout = data.get('timeout', 10)  # Default 10 seconds, configurable per test
        self.stdin_input = data.get('stdin_input')  # Optional stdin input for interactive tests
        self.batch_mode = data.get('batch_mode', False)  # Set true to test batch mode error behavior
        self.environment = data.get('environment', {})  # Optional environment variables

        # Skip support
        self.skip = data.get('skip')  # Can be True/False or string reason
        self.skip_reason = data.get('skip_reason', 'No reason provided')

        # Interactive steps support (pexpect PTY-based dialog testing).
        # In palette_mode=True, interactive_steps may include
        # {'screenshot_match': '<path-to-golden>'} entries that compare the
        # current pyte-emulated screen against a golden text fixture. See
        # FrontierCLI.execute_interactive_palette() for the routing.
        self.interactive_steps = data.get('interactive_steps', [])

        # L4 palette test harness: when true, run the interactive_steps
        # through a pyte-backed xterm-256color PTY instead of the plain
        # TERM=dumb REPL path. Required for tests that exercise palette
        # menu rendering, REPL menu cascade, or any ANSI-styled UI.
        self.palette_mode = data.get('palette_mode', False)

        # REPL mode support (auto-skip if repl_mode is true)
        self.repl_mode = data.get('repl_mode', False)  # Run test in REPL interactive mode
        self.expected_output_contains = data.get('expected_output_contains', [])  # Substrings in stdout/stderr
        self.expected_output_not_contains = data.get('expected_output_not_contains', [])  # Forbidden substrings

        # Protocol-native test support (raw NDJSON operations like odb/get, odb/set, etc.)
        self.protocol_ops = data.get('protocol_ops', [])  # List of {op, params, validate} dicts

        # Unit 1.2 (odb/save restart-then-verify): when true, the test runs on
        # a dedicated executor against a PRIVATE copy of the system root, so
        # on-disk saves and executor restarts (restart_executor steps) cannot
        # interfere with the shared executor, which holds the worker's staged
        # root open for the rest of the file. Required for any protocol_ops
        # test that saves the system root or restarts its process.
        self.private_system_root = data.get('private_system_root', False)

    def get_script_with_substitutions(self, test_root_dir: Optional[str] = None) -> str:
        """Get the script with path substitutions applied."""
        script = self.script

        # Substitute test directory paths
        if test_root_dir:
            test_tmp_dir = os.path.join(test_root_dir, 'tmp', 'integration')
            # Ensure tmp directory exists for tests
            os.makedirs(test_tmp_dir, exist_ok=True)
            script = script.replace('{FRONTIER_TEST_TMP_DIR}', test_tmp_dir)

        return script

    def get_stdin_with_substitutions(self, test_root_dir: Optional[str] = None) -> Optional[str]:
        """Get the stdin input with path substitutions applied."""
        if self.stdin_input is None:
            return None

        stdin_input = self.stdin_input

        # Substitute test directory paths
        if test_root_dir:
            test_tmp_dir = os.path.join(test_root_dir, 'tmp', 'integration')
            stdin_input = stdin_input.replace('{FRONTIER_TEST_TMP_DIR}', test_tmp_dir)

        return stdin_input

    def validate(self, output: Dict) -> Tuple[bool, Optional[str]]:
        """Validate test output against expectations."""
        import re

        # Check success/failure status
        if output.get('success') != self.expected_success:
            return False, f"Expected success={self.expected_success}, got success={output.get('success')}"

        # If expecting success, check result
        if self.expected_success and self.expected_result is not None:
            actual_result = output.get('result')
            if str(actual_result) != str(self.expected_result):
                return False, f"Expected result={self.expected_result!r}, got {actual_result!r}"

        # Check if result contains all expected strings
        if self.expected_success and self.expected_contains is not None:
            actual_result = str(output.get('result', ''))
            for expected_string in self.expected_contains:
                if expected_string not in actual_result:
                    return False, f"Expected result to contain {expected_string!r}, but got {actual_result!r}"

        # Check if result matches expected pattern (regex, full match).
        # Use .* prefix/suffix in the pattern for partial matching.
        if self.expected_success and self.expected_pattern is not None:
            actual_result = str(output.get('result', ''))
            # Changed from re.search to re.fullmatch in PR #481.
            # All existing patterns verified to work with fullmatch semantics.
            if not re.fullmatch(self.expected_pattern, actual_result):
                return False, f"Expected result to match pattern {self.expected_pattern!r}, but got {actual_result!r}"

        # Check result type if specified
        if self.expected_success and self.expected_result_type is not None:
            actual_result_type = output.get('result_type')
            # Normalize both types for comparison (handles Frontier's internal type names)
            normalized_actual = normalize_type_name(actual_result_type)
            normalized_expected = normalize_type_name(self.expected_result_type)
            if normalized_actual != normalized_expected:
                return False, f"Expected result_type={self.expected_result_type}, got {actual_result_type}"

        # If expecting failure, check error type
        if not self.expected_success and self.expected_error_type:
            actual_error_type = output.get('error_type')
            if actual_error_type != self.expected_error_type:
                return False, f"Expected error_type={self.expected_error_type}, got {actual_error_type}"

        # If expecting failure, check error contains string
        if not self.expected_success and self.expected_error_contains:
            actual_error = str(output.get('error', ''))
            if self.expected_error_contains not in actual_error:
                return False, f"Expected error to contain {self.expected_error_contains!r}, but got {actual_error!r}"

        return True, None

    def validate_repl_output(self, result: subprocess.CompletedProcess) -> Tuple[bool, Optional[str]]:
        """
        Validate REPL test output against expectations.

        REPL tests check stdout/stderr text, not JSON output.
        """
        # Combine stdout and stderr for checking (ensure both are strings)
        stdout = result.stdout if isinstance(result.stdout, str) else (result.stdout.decode('utf-8', errors='replace') if result.stdout else '')
        stderr = result.stderr if isinstance(result.stderr, str) else (result.stderr.decode('utf-8', errors='replace') if result.stderr else '')
        output = stdout + stderr

        # Check expected_output_contains (all must be present)
        for substring in self.expected_output_contains:
            if substring not in output:
                return False, f"Expected substring not found: {substring!r}"

        # Check expected_output_not_contains (all must be absent)
        for substring in self.expected_output_not_contains:
            if substring in output:
                return False, f"Unexpected substring found: {substring!r}"

        # Check exit code
        if self.expected_success and result.returncode != 0:
            return False, f"Expected success but got exit code {result.returncode}"

        if not self.expected_success and result.returncode == 0:
            return False, f"Expected failure but got exit code 0"

        return True, None


class TestRunner:
    """Main test runner that executes test cases."""

    def __init__(self, cli: FrontierCLI, verbose: bool = False, test_root_dir: Optional[str] = None,
                 protocol_executor: Optional[ProtocolExecutor] = None,
                 selectors: Optional[set] = None):
        self.cli = cli
        self.verbose = verbose
        self.test_root_dir = test_root_dir or str(Path.cwd())
        self.results: List[TestResult] = []
        self.protocol_executor = protocol_executor
        # Optional set of test names to filter by (None = no filtering).
        self.selectors = selectors

    def load_test_file(self, yaml_path: str) -> List[TestCase]:
        """Load test cases from YAML file, applying --select filter if set."""
        with open(yaml_path, 'r') as f:
            data = yaml.safe_load(f)

        test_cases = []
        for test_data in data.get('tests', []):
            if self.selectors is not None:
                name = test_data.get('name', 'Unnamed Test')
                if name not in self.selectors:
                    continue
            test_cases.append(TestCase(test_data))

        return test_cases

    def run_test(self, test: TestCase) -> TestResult:
        """Run a single test case."""
        # Check if test should be skipped
        if test.skip:
            # Determine skip reason
            if isinstance(test.skip, str):
                reason = test.skip  # skip field contains the reason
            else:
                reason = test.skip_reason  # Use skip_reason field

            if self.verbose:
                print(f"  Skipping: {test.name} - {reason}")

            return TestResult(
                name=test.name,
                passed=True,  # Don't count as failure
                error=None,
                details=reason,
                skipped=True
            )

        if self.verbose:
            print(f"  Running: {test.name}")
            if test.description:
                print(f"    {test.description}")

        # Route to appropriate executor
        if test.protocol_ops:
            return self.run_protocol_test(test)
        elif test.interactive_steps:
            if not HAS_PEXPECT:
                return TestResult(
                    name=test.name,
                    passed=True,
                    error=None,
                    details="pexpect is required for interactive tests. Install with: pip3 install pexpect",
                    skipped=True
                )
            if test.palette_mode:
                # Lazy bootstrap pyte (pip-installs on first palette test).
                # If install fails, surface as a configuration failure rather
                # than a silent skip — palette tests are mandatory once the
                # YAML opts into palette_mode.
                _pyte_mod, pyte_err = _ensure_pyte()
                if _pyte_mod is None:
                    return TestResult(
                        name=test.name,
                        passed=False,
                        error=pyte_err or "pyte bootstrap failed",
                        details="pyte is required for palette_mode tests. Run: pip3 install -r tests/requirements.txt",
                    )
                return self.run_interactive_palette_test(test)
            return self.run_interactive_test(test)
        elif test.repl_mode:
            return self.run_repl_test(test)
        else:
            return self.run_batch_test(test)

    def run_protocol_test(self, test: TestCase) -> TestResult:
        """
        Run a protocol-native test that sends raw NDJSON operations.

        Each step in test.protocol_ops is a dict with:
          - op: operation name (e.g. "odb/get")
          - params: parameters dict
          - validate: dict of assertions on the response:
              - success: expected top-level success boolean
              - results: list of per-item assertions (each a dict of key/value checks)
              - error_contains: string expected in error message
        """
        # Note: If a test fails before its cleanup step, stale test_proto_*
        # entries may remain in workspace. This is acceptable for now — tests
        # use unique prefixes and check specific paths, so stale entries from
        # prior runs don't cause false failures.
        #
        # Issue #127: If the test declares an `environment:` block, spawn a
        # dedicated short-lived executor so the per-test env overrides (e.g.
        # FRONTIER_LOCK_OPENED_ROOTS=0 for save-path tests) apply. The shared
        # batch executor is started with FRONTIER_LOCK_OPENED_ROOTS=1 by
        # default and can't accept per-test overrides at send time.
        dedicated_executor: Optional[ProtocolExecutor] = None
        private_root_dir: Optional[str] = None
        if test.environment or test.private_system_root:
            executor_root = (self.protocol_executor.system_root
                             if self.protocol_executor else self.cli.system_root)

            # Unit 1.2: stage a private copy of the system root for tests
            # that save to disk or restart their executor. The shared
            # executor keeps the worker's staged root open; another process
            # rewriting that same file under it would corrupt its cache.
            if test.private_system_root:
                if not executor_root or not os.path.isfile(executor_root):
                    return TestResult(
                        test.name, True, error=None, skipped=True,
                        details="private_system_root requires a file-backed system root")
                staging_parent = None
                if self.test_root_dir:
                    staging_parent = os.path.join(
                        self.test_root_dir, 'tmp', 'integration')
                    os.makedirs(staging_parent, exist_ok=True)
                private_root_dir = tempfile.mkdtemp(
                    prefix='private_root_', dir=staging_parent)
                private_root = os.path.join(
                    private_root_dir, os.path.basename(executor_root))
                shutil.copy2(executor_root, private_root)
                executor_root = private_root

            dedicated_executor = ProtocolExecutor(
                cli_path=self.cli.cli_path,
                system_root=executor_root,
            )
            try:
                dedicated_executor.start(env_overrides=test.environment or None)
            except Exception as e:
                if private_root_dir:
                    shutil.rmtree(private_root_dir, ignore_errors=True)
                return TestResult(
                    test.name, False,
                    error=f"Failed to start dedicated protocol executor: {e}")
            executor = dedicated_executor
        else:
            if self.protocol_executor is None or not self.protocol_executor.is_alive:
                return TestResult(
                    test.name, False,
                    error="Protocol executor not available (required for protocol_ops tests)")
            executor = self.protocol_executor

        # Values captured from responses via the `capture:` step key, for
        # substitution into later steps' params as "$name" (e.g. the
        # threadId returned by debug/run, consumed by debug/continue).
        captures: Dict[str, object] = {}

        try:
            for step_idx, step in enumerate(test.protocol_ops):
                op = step.get('op')
                params = self._resolve_captures(step.get('params', {}), captures)
                validate = step.get('validate', {})
                step_desc = step.get('description', f'step {step_idx + 1}')

                # Unit 1.2 restart-then-verify steps. Only meaningful on a
                # dedicated executor with a private root: restarting the
                # SHARED executor would tear state out from under every
                # later test in the file, and hashing the shared root races
                # against its own executor's writes.
                if 'restart_executor' in step:
                    if dedicated_executor is None or not test.private_system_root:
                        return TestResult(
                            test.name, False,
                            error=f"[{step_desc}] restart_executor requires "
                                  f"private_system_root: true")
                    mode = step['restart_executor']
                    if mode is True:
                        mode = 'kill'
                    try:
                        executor.restart_for_test(mode)
                    except RuntimeError as e:
                        return TestResult(
                            test.name, False,
                            error=f"[{step_desc}] restart ({mode}) failed: {e}")
                    continue

                if 'capture_root_hash' in step:
                    if not test.private_system_root:
                        return TestResult(
                            test.name, False,
                            error=f"[{step_desc}] capture_root_hash requires "
                                  f"private_system_root: true")
                    with open(executor.system_root, 'rb') as f:
                        captures[step['capture_root_hash']] = \
                            hashlib.md5(f.read()).hexdigest()
                    continue

                if 'verify_root_hash' in step:
                    if not test.private_system_root:
                        return TestResult(
                            test.name, False,
                            error=f"[{step_desc}] verify_root_hash requires "
                                  f"private_system_root: true")
                    expected_hash = self._resolve_captures(
                        step['verify_root_hash'], captures)
                    with open(executor.system_root, 'rb') as f:
                        actual_hash = hashlib.md5(f.read()).hexdigest()
                    if actual_hash != expected_hash:
                        return TestResult(
                            test.name, False,
                            error=f"[{step_desc}] root file hash changed: "
                                  f"expected {expected_hash}, got {actual_hash}")
                    continue

                # Envelope contract step: send a literal (possibly invalid)
                # line instead of a well-formed op message. Validates either
                # the response, or that no response arrives (no_response).
                # raw_line is a string, or a list of parts where each part
                # is a string or {repeat: <str>, count: <n>} (for building
                # oversized / large-batch lines without huge YAML literals).
                if 'raw_line' in step:
                    raw_spec = step['raw_line']
                    if isinstance(raw_spec, list):
                        pieces = []
                        for part in raw_spec:
                            if isinstance(part, dict):
                                pieces.append(part['repeat'] * int(part['count']))
                            else:
                                pieces.append(part)
                        raw_spec = ''.join(pieces)
                    raw_timeout = step.get('timeout', 3.0)
                    try:
                        resp = executor.send_raw_line(raw_spec, timeout=raw_timeout)
                    except RuntimeError as e:
                        try:
                            executor._restart()
                        except Exception as restart_err:
                            logging.warning("Protocol executor restart failed: %s", restart_err)
                        return TestResult(
                            test.name, False,
                            error=f"Protocol error at {step_desc}: {e}")
                    if validate.get('no_response'):
                        if resp is not None:
                            return TestResult(
                                test.name, False,
                                error=f"[{step_desc}] Expected no response, got: {resp!r}")
                        continue
                    if resp is None:
                        return TestResult(
                            test.name, False,
                            error=f"[{step_desc}] Expected a response, got none "
                                  f"(timed out after {raw_timeout}s)")
                    err = self._validate_protocol_response(resp, validate, step_desc)
                    if err is not None:
                        details = f"Response: {json.dumps(resp, indent=2)}" if self.verbose else None
                        return TestResult(test.name, False, error=err, details=details)
                    continue

                # Notification step: wait for a server-initiated line
                # (id:null + op), e.g. debug/suspended after debug/run.
                if 'wait_notification' in step:
                    want_op = step['wait_notification']
                    note_timeout = step.get('timeout', 10.0)
                    try:
                        note = executor.wait_notification(want_op, timeout=note_timeout)
                    except TimeoutError:
                        return TestResult(
                            test.name, False,
                            error=f"[{step_desc}] Timed out waiting for notification {want_op}")
                    except RuntimeError as e:
                        return TestResult(
                            test.name, False,
                            error=f"[{step_desc}] {e}")
                    expected_params = self._resolve_captures(
                        validate.get('params', {}), captures)
                    actual_params = note.get('params', {})
                    for key, expected in expected_params.items():
                        actual = actual_params.get(key)
                        if actual != expected:
                            return TestResult(
                                test.name, False,
                                error=f"[{step_desc}] Notification {want_op} params.{key}: "
                                      f"expected {expected!r}, got {actual!r}")
                    continue

                if op is None:
                    return TestResult(
                        test.name, False,
                        error=f"protocol_ops step missing 'op' field at {step_desc}")

                # Note: send_raw() calls _send_recv() which adds a monotonic
                # 'id' field to every outgoing message and validates that the
                # response 'id' matches. No need to set id here.
                msg = {'op': op}
                if params:
                    msg['params'] = params

                try:
                    resp = executor.send_raw(msg, timeout=test.timeout)
                except TimeoutError:
                    return TestResult(
                        test.name, False,
                        error=f"Timeout at {step_desc}: op={op}")
                except RuntimeError as e:
                    try:
                        executor._restart()
                    except Exception as restart_err:
                        logging.warning("Protocol executor restart failed: %s", restart_err)
                    return TestResult(
                        test.name, False,
                        error=f"Protocol error at {step_desc}: {e}")

                # Validate response
                err = self._validate_protocol_response(resp, validate, step_desc)
                if err is not None:
                    details = f"Response: {json.dumps(resp, indent=2)}" if self.verbose else None
                    return TestResult(test.name, False, error=err, details=details)

                # Capture response values for later steps, e.g.
                # capture: {tid: "result.threadId"}
                for name, path in step.get('capture', {}).items():
                    node = resp
                    for part in path.split('.'):
                        node = node.get(part) if isinstance(node, dict) else None
                    if node is None:
                        return TestResult(
                            test.name, False,
                            error=f"[{step_desc}] capture {name!r}: path {path!r} "
                                  f"not found in response")
                    captures[name] = node

            # All steps passed
            return TestResult(test.name, True)

        except Exception as e:
            return TestResult(test.name, False, error=f"Unexpected error: {e}")
        finally:
            if dedicated_executor is not None:
                try:
                    dedicated_executor.stop()
                except Exception:
                    pass
                if private_root_dir:
                    shutil.rmtree(private_root_dir, ignore_errors=True)
                # Restart the shared executor after a dedicated-executor
                # test. The multi-second pause while the dedicated process
                # runs gives lingering detached threads from earlier tests
                # a window to run inside the idle shared process and
                # trample its thread-global / name-resolution state (#706
                # family; observed as a later ODB handler call failing
                # with "the only script it contains is named evaluate" on
                # an unmodified develop binary). A fresh process guarantees
                # later tests never run against a poisoned executor.
                if self.protocol_executor is not None:
                    try:
                        self.protocol_executor._restart()
                    except Exception as e:
                        print(f"  [protocol] restart after dedicated test failed: {e}",
                              file=sys.stderr)
            elif self.protocol_executor is not None:
                self.protocol_executor.reset()

    @staticmethod
    def _resolve_captures(obj, captures: dict):
        """Deep-copy obj, replacing string values of the exact form "$name"
        with the captured value of that name (see the `capture:` step key).
        Unknown "$name" strings are left as-is so genuine dollar-prefixed
        payloads aren't corrupted."""
        if isinstance(obj, dict):
            return {k: TestRunner._resolve_captures(v, captures)
                    for k, v in obj.items()}
        if isinstance(obj, list):
            return [TestRunner._resolve_captures(v, captures) for v in obj]
        if isinstance(obj, str) and obj.startswith('$') and obj[1:] in captures:
            return captures[obj[1:]]
        return obj

    @staticmethod
    def _validate_protocol_response(resp: dict, validate: dict, step_desc: str) -> Optional[str]:
        """Validate a raw protocol response against assertions. Returns error string or None.

        Type coercion rules:
        - 'success' field: strict type match (bool only, catches "true" vs true)
        - '_strict_type' assertions: strict type + value match
        - All other fields: flexible comparison (falls back to str() if types differ)

        Use _strict_type when the JSON wire type matters (e.g., number vs string).
        """

        # Check top-level success
        if 'success' in validate:
            expected = validate['success']
            actual = resp.get('success')
            if actual != expected:
                return f"[{step_desc}] Expected success={expected}, got success={actual}"

        # Unit 1.2: top-level dirty indicator on odb/set / odb/delete /
        # odb/save responses. Strict bool match (like success) so a string
        # "true" on the wire fails rather than passing via coercion.
        if 'dirty' in validate:
            expected = validate['dirty']
            actual = resp.get('dirty')
            if type(actual) is not bool or actual != expected:
                return f"[{step_desc}] Expected dirty={expected}, got dirty={actual!r}"

        # Check error message contains
        if 'error_contains' in validate:
            expected_substr = validate['error_contains']
            error_obj = resp.get('error', {})
            error_msg = error_obj.get('message', '') if isinstance(error_obj, dict) else str(error_obj)
            if expected_substr not in error_msg:
                return f"[{step_desc}] Expected error containing {expected_substr!r}, got {error_msg!r}"

        # Check the stable machine-readable error code (error.code).
        # Unit 1.1 protocol contract: every top-level error response carries
        # a code from the documented set (see STDIO_PROTOCOL.md).
        if 'error_code' in validate:
            expected_code = validate['error_code']
            error_obj = resp.get('error', {})
            actual_code = error_obj.get('code') if isinstance(error_obj, dict) else None
            if actual_code != expected_code:
                return f"[{step_desc}] Expected error.code={expected_code!r}, got {actual_code!r}"

        # PR1 (REPL error context): structured error.location assertions.
        # expected_error_location is a dict with any subset of:
        #   script: exact-match string
        #   line: int (exact)
        #   column: int (exact)
        #   token_start_min: int (actual >= expected)
        #   token_end_min: int (actual >= expected)
        if 'expected_error_location' in validate:
            expected_loc = validate['expected_error_location']
            error_obj = resp.get('error', {})
            if not isinstance(error_obj, dict):
                return (f"[{step_desc}] expected_error_location set but response error is "
                        f"not a dict: {error_obj!r}")
            actual_loc = error_obj.get('location')
            if not isinstance(actual_loc, dict):
                return (f"[{step_desc}] expected_error_location set but response has no "
                        f"error.location object (got {actual_loc!r})")
            for key in ('script', 'line', 'column'):
                if key in expected_loc:
                    exp_val = expected_loc[key]
                    act_val = actual_loc.get(key)
                    if act_val != exp_val:
                        return (f"[{step_desc}] error.location.{key}: expected "
                                f"{exp_val!r}, got {act_val!r}")
            for key, min_key in (('tokenStart', 'token_start_min'),
                                 ('tokenEnd', 'token_end_min')):
                if min_key in expected_loc:
                    exp_min = expected_loc[min_key]
                    act_val = actual_loc.get(key)
                    if not isinstance(act_val, int) or act_val < exp_min:
                        return (f"[{step_desc}] error.location.{key}: expected >= "
                                f"{exp_min}, got {act_val!r}")

        # PR1 (REPL error context): assert that EITHER tokenStart > 0 OR
        # tokenEnd > tokenStart -- proves the scanner snapshotted a real span.
        if 'expected_error_token_present' in validate and validate['expected_error_token_present']:
            error_obj = resp.get('error', {})
            actual_loc = error_obj.get('location') if isinstance(error_obj, dict) else None
            if not isinstance(actual_loc, dict):
                return (f"[{step_desc}] expected_error_token_present set but response "
                        f"has no error.location object")
            ts = actual_loc.get('tokenStart')
            te = actual_loc.get('tokenEnd')
            if not (isinstance(ts, int) and isinstance(te, int)):
                return (f"[{step_desc}] expected_error_token_present: tokenStart/tokenEnd "
                        f"missing or non-int (tokenStart={ts!r}, tokenEnd={te!r})")
            if not (ts > 0 or te > ts):
                return (f"[{step_desc}] expected_error_token_present: neither tokenStart>0 "
                        f"nor tokenEnd>tokenStart (tokenStart={ts}, tokenEnd={te})")

        # PR1 (REPL error context): minimum stack depth (>=).
        if 'expected_error_stack_min' in validate:
            expected_min = validate['expected_error_stack_min']
            error_obj = resp.get('error', {})
            actual_stack = error_obj.get('stack') if isinstance(error_obj, dict) else None
            if not isinstance(actual_stack, list):
                return (f"[{step_desc}] expected_error_stack_min={expected_min} but "
                        f"error.stack missing or not a list (got {actual_stack!r})")
            if len(actual_stack) < expected_min:
                return (f"[{step_desc}] error.stack: expected at least {expected_min} "
                        f"frames, got {len(actual_stack)} ({actual_stack!r})")

        # PR1 (REPL error context): substring match on top stack frame's script.
        if 'expected_error_stack_top_script_contains' in validate:
            expected_substr = validate['expected_error_stack_top_script_contains']
            error_obj = resp.get('error', {})
            actual_stack = error_obj.get('stack') if isinstance(error_obj, dict) else None
            if not isinstance(actual_stack, list) or len(actual_stack) == 0:
                return (f"[{step_desc}] expected_error_stack_top_script_contains "
                        f"requires non-empty error.stack list (got {actual_stack!r})")
            top_script = actual_stack[0].get('script') if isinstance(actual_stack[0], dict) else None
            if not isinstance(top_script, str) or expected_substr not in top_script:
                return (f"[{step_desc}] error.stack[0].script: expected to contain "
                        f"{expected_substr!r}, got {top_script!r}")

        # PR3 (REPL error context): assert error.causedBy is present and
        # matches expectations. Used for the try/else origin-context chain:
        # when an error fires from inside an else block, the originating
        # try-block failure is exposed as error.causedBy with its own
        # {message, location, stack} substructure.
        #
        # expected_error_causedby is a dict with any subset of:
        #   message_contains: substring match on error.causedBy.message
        #   location_present: bool -- error.causedBy.location must be a dict
        #   stack_min: int -- len(error.causedBy.stack) >= this value
        if 'expected_error_causedby' in validate:
            expected_cb = validate['expected_error_causedby']
            error_obj = resp.get('error', {})
            if not isinstance(error_obj, dict):
                return (f"[{step_desc}] expected_error_causedby set but response error "
                        f"is not a dict: {error_obj!r}")
            actual_cb = error_obj.get('causedBy')
            if not isinstance(actual_cb, dict):
                return (f"[{step_desc}] expected_error_causedby set but response has no "
                        f"error.causedBy object (got {actual_cb!r})")
            if 'message_contains' in expected_cb:
                expected_substr = expected_cb['message_contains']
                actual_msg = actual_cb.get('message', '')
                if not isinstance(actual_msg, str) or expected_substr not in actual_msg:
                    return (f"[{step_desc}] error.causedBy.message: expected to contain "
                            f"{expected_substr!r}, got {actual_msg!r}")
            if expected_cb.get('location_present'):
                actual_loc = actual_cb.get('location')
                if not isinstance(actual_loc, dict):
                    return (f"[{step_desc}] error.causedBy.location: expected dict, "
                            f"got {actual_loc!r}")
            if 'stack_min' in expected_cb:
                expected_min = expected_cb['stack_min']
                actual_stack = actual_cb.get('stack')
                if not isinstance(actual_stack, list):
                    return (f"[{step_desc}] error.causedBy.stack: expected list, "
                            f"got {actual_stack!r}")
                if len(actual_stack) < expected_min:
                    return (f"[{step_desc}] error.causedBy.stack: expected at least "
                            f"{expected_min} frames, got {len(actual_stack)} "
                            f"({actual_stack!r})")

        # PR3 P1-2 (bar-raiser): assert that error.causedBy is NOT present.
        # Used to verify the "promote causedBy to primary" code path in
        # op_handler: when bserror is empty and a causedby snapshot is
        # available, the originating message becomes primary and causedBy
        # is dropped to avoid duplication.
        if validate.get('expected_error_causedby_absent'):
            error_obj = resp.get('error', {})
            if isinstance(error_obj, dict) and 'causedBy' in error_obj:
                return (f"[{step_desc}] expected_error_causedby_absent set but "
                        f"response has error.causedBy: {error_obj.get('causedBy')!r}")

        # Check result_count first (before per-item loop) so count mismatches
        # produce a clear message rather than an IndexError or confusing diff.
        if 'result_count' in validate:
            actual_count = len(resp.get('results', []))
            expected_count = validate['result_count']
            if actual_count != expected_count:
                return f"[{step_desc}] Expected {expected_count} results, got {actual_count}"

        # Check results array items
        if 'results' in validate:
            actual_results = resp.get('results', [])
            expected_results = validate['results']
            if len(actual_results) != len(expected_results):
                return (f"[{step_desc}] Expected {len(expected_results)} results, "
                        f"got {len(actual_results)}")

            for i, expected_item in enumerate(expected_results):
                actual_item = actual_results[i]
                for key, expected_val in expected_item.items():
                    if key == '_exists':
                        # Check that a key exists (or doesn't) in the result item
                        for check_key, should_exist in expected_val.items():
                            if should_exist and check_key not in actual_item:
                                return f"[{step_desc}] results[{i}]: expected key '{check_key}' to exist"
                            if not should_exist and check_key in actual_item:
                                return f"[{step_desc}] results[{i}]: expected key '{check_key}' to not exist"
                        continue
                    if key == '_strict_type':
                        # Check that actual values match expected type AND value exactly
                        for check_key, expected_typed_val in expected_val.items():
                            actual_typed_val = actual_item.get(check_key)
                            if type(actual_typed_val) is not type(expected_typed_val):
                                return (f"[{step_desc}] results[{i}].{check_key}: "
                                        f"type mismatch: expected {type(expected_typed_val).__name__} "
                                        f"{expected_typed_val!r}, got {type(actual_typed_val).__name__} "
                                        f"{actual_typed_val!r}")
                            if actual_typed_val != expected_typed_val:
                                return (f"[{step_desc}] results[{i}].{check_key}: "
                                        f"expected {expected_typed_val!r}, got {actual_typed_val!r}")
                        continue
                    if key == '_contains':
                        # Check that a string value contains a substring
                        for check_key, substr in expected_val.items():
                            actual_val = str(actual_item.get(check_key, ''))
                            if substr not in actual_val:
                                return (f"[{step_desc}] results[{i}].{check_key}: "
                                        f"expected to contain {substr!r}, got {actual_val!r}")
                        continue
                    if key == '_pattern':
                        # Check that a value matches a regex pattern (full match).
                        # Use .* prefix/suffix in the pattern for partial matching.
                        for check_key, pattern in expected_val.items():
                            actual_val = str(actual_item.get(check_key, ''))
                            if not re.fullmatch(pattern, actual_val):
                                return (f"[{step_desc}] results[{i}].{check_key}: "
                                        f"expected to match {pattern!r}, got {actual_val!r}")
                        continue

                    actual_val = actual_item.get(key)
                    # For boolean fields, require exact type match to catch
                    # string "true" vs JSON boolean true mismatches
                    if key == 'success' and type(actual_val) is not type(expected_val):
                        return (f"[{step_desc}] results[{i}].{key}: "
                                f"type mismatch: expected {type(expected_val).__name__} "
                                f"{expected_val!r}, got {type(actual_val).__name__} {actual_val!r}")
                    # Allow flexible type comparison for other fields (e.g. int vs string "42")
                    if actual_val != expected_val and str(actual_val) != str(expected_val):
                        return (f"[{step_desc}] results[{i}].{key}: "
                                f"expected {expected_val!r}, got {actual_val!r}")

        # Note: The 'result' (singular) section does not support meta-assertions
        # (_strict_type, _exists, _contains, _pattern) — only 'results' (plural) does.
        # This is acceptable because script/eval responses have simple structure.

        # Check result (singular) — for script/eval responses which use "result" not "results"
        if 'result' in validate:
            actual_result = resp.get('result', {})
            expected_result = validate['result']
            if isinstance(expected_result, dict) and isinstance(actual_result, dict):
                for key, expected_val in expected_result.items():
                    actual_val = actual_result.get(key)
                    if actual_val != expected_val and str(actual_val) != str(expected_val):
                        return (f"[{step_desc}] result.{key}: "
                                f"expected {expected_val!r}, got {actual_val!r}")
            elif actual_result != expected_result and str(actual_result) != str(expected_result):
                return f"[{step_desc}] result: expected {expected_result!r}, got {actual_result!r}"

        # Check results[0].entries count (for odb/list).
        # Note: Unindexed entries_count/entries_min/entries_include always inspect results[0].
        # For multi-path batches, use the indexed variants (entries_count_N, entries_min_N, etc).
        if 'entries_count' in validate:
            results = resp.get('results', [])
            if not results:
                return f"[{step_desc}] No results to check entries_count"
            entries = results[0].get('entries', [])
            expected = validate['entries_count']
            if len(entries) != expected:
                return f"[{step_desc}] Expected {expected} entries, got {len(entries)}"

        # Indexed entries validators (entries_count_N, entries_min_N, entries_include_N)
        # inspect results[N].entries for multi-path odb/list batches.
        for key in validate:
            for prefix, checker in [
                ('entries_count_', 'count'),
                ('entries_min_', 'min'),
                ('entries_include_', 'include'),
            ]:
                if key.startswith(prefix):
                    idx_str = key[len(prefix):]
                    try:
                        idx = int(idx_str)
                    except ValueError:
                        continue
                    results = resp.get('results', [])
                    if idx >= len(results):
                        return f"[{step_desc}] Result index {idx} out of range (have {len(results)} results)"
                    entries = results[idx].get('entries', [])
                    expected_val = validate[key]

                    if checker == 'count':
                        if len(entries) != expected_val:
                            return f"[{step_desc}] results[{idx}]: Expected {expected_val} entries, got {len(entries)}"
                    elif checker == 'min':
                        if len(entries) < expected_val:
                            return f"[{step_desc}] results[{idx}]: Expected at least {expected_val} entries, got {len(entries)}"
                    elif checker == 'include':
                        entry_names = {e.get('name') for e in entries}
                        for expected_entry in expected_val:
                            name = expected_entry.get('name')
                            if name not in entry_names:
                                return f"[{step_desc}] results[{idx}]: Expected entry named {name!r} not found"
                            if 'type' in expected_entry:
                                matching = [e for e in entries if e.get('name') == name]
                                if matching and matching[0].get('type') != expected_entry['type']:
                                    return (f"[{step_desc}] results[{idx}]: Entry {name!r}: expected type "
                                            f"{expected_entry['type']!r}, got {matching[0].get('type')!r}")

        # Check entries_min (at least N entries)
        if 'entries_min' in validate:
            results = resp.get('results', [])
            if not results:
                return f"[{step_desc}] No results to check entries_min"
            entries = results[0].get('entries', [])
            expected_min = validate['entries_min']
            if len(entries) < expected_min:
                return f"[{step_desc}] Expected at least {expected_min} entries, got {len(entries)}"

        # Check specific entries by name (for odb/list)
        if 'entries_include' in validate:
            results = resp.get('results', [])
            if not results:
                return f"[{step_desc}] No results to check entries_include"
            entries = results[0].get('entries', [])
            entry_names = {e.get('name') for e in entries}
            for expected_entry in validate['entries_include']:
                name = expected_entry.get('name')
                if name not in entry_names:
                    return f"[{step_desc}] Expected entry named {name!r} not found in listing"
                # Check type if specified
                if 'type' in expected_entry:
                    matching = [e for e in entries if e.get('name') == name]
                    if matching and matching[0].get('type') != expected_entry['type']:
                        return (f"[{step_desc}] Entry {name!r}: expected type "
                                f"{expected_entry['type']!r}, got {matching[0].get('type')!r}")

        return None

    def run_batch_test(self, test: TestCase) -> TestResult:
        """Run a test in batch mode. Uses protocol executor if available and compatible."""
        # Get script with path substitutions applied
        script = test.get_script_with_substitutions(self.test_root_dir)

        # Try protocol executor for compatible tests
        if (self.protocol_executor is not None
                and self.protocol_executor.is_alive
                and _is_protocol_compatible(test)):
            output = self.protocol_executor.execute(script, timeout=test.timeout)

            # If protocol failed due to process death, retry with per-process.
            # These errors come from _send_recv(): "Protocol process not running",
            # "Protocol process died unexpectedly", "Protocol process closed stdout".
            if (not output.get('success')
                    and output.get('error_type') == 'execution_error'
                    and 'Protocol process' in str(output.get('error', ''))):
                stdin_input = test.get_stdin_with_substitutions(self.test_root_dir)
                test_env = test.environment.copy()
                if stdin_input is not None and not test.batch_mode:
                    test_env['FRONTIER_FORCE_INTERACTIVE'] = '1'
                output = self.cli.execute(
                    script, timeout=test.timeout,
                    stdin_input=stdin_input,
                    batch_mode=test.batch_mode,
                    env=test_env)

            # Reset state after each test
            self.protocol_executor.reset()
        else:
            # Fallback to per-process execution
            # Get stdin input with path substitutions applied
            stdin_input = test.get_stdin_with_substitutions(self.test_root_dir)

            # Prepare environment variables
            test_env = test.environment.copy()

            # If test provides stdin_input and isn't in batch mode, force interactive mode
            # This overrides TTY detection which fails when stdin is piped
            if stdin_input is not None and not test.batch_mode:
                test_env['FRONTIER_FORCE_INTERACTIVE'] = '1'

            # Execute script with test-specific timeout, stdin input, batch mode, and environment
            output = self.cli.execute(
                script,
                timeout=test.timeout,
                stdin_input=stdin_input,
                batch_mode=test.batch_mode,
                env=test_env
            )

        # Validate result
        passed, error = test.validate(output)

        details = None
        if not passed and self.verbose:
            details = f"Output: {json.dumps(output, indent=2)}"

        return TestResult(test.name, passed, error, details)

    def run_repl_test(self, test: TestCase) -> TestResult:
        """Run a test in REPL interactive mode."""
        # Get stdin input with path substitutions applied
        stdin_input = test.get_stdin_with_substitutions(self.test_root_dir)

        # Default to "/exit\n" if no input provided
        if stdin_input is None:
            stdin_input = "/exit\n"

        # Prepare environment variables
        test_env = test.environment.copy()

        # Execute REPL with stdin
        result = self.cli.execute_repl(
            stdin_input=stdin_input,
            timeout=test.timeout,
            env=test_env
        )

        # Validate REPL output
        passed, error = test.validate_repl_output(result)

        details = None
        if not passed and self.verbose:
            details = (
                f"Exit code: {result.returncode}\n"
                f"Stdout:\n{result.stdout}\n"
                f"Stderr:\n{result.stderr}"
            )

        return TestResult(test.name, passed, error, details)

    def run_interactive_test(self, test: TestCase) -> TestResult:
        """Run a test using pexpect interactive steps."""
        # Prepare environment variables
        test_env = test.environment.copy()

        # Execute with pexpect
        result = self.cli.execute_interactive(
            interactive_steps=test.interactive_steps,
            timeout=test.timeout,
            env=test_env
        )

        # Validate using REPL output validation (checks expected_output_contains etc.)
        passed, error = test.validate_repl_output(result)

        details = None
        if not passed and self.verbose:
            details = (
                f"Exit code: {result.returncode}\n"
                f"Stdout:\n{result.stdout}\n"
                f"Stderr:\n{result.stderr}"
            )

        return TestResult(test.name, passed, error, details)

    def run_interactive_palette_test(self, test: TestCase) -> TestResult:
        """Run a palette_mode interactive test (pexpect + pyte screenshot diff).

        Routed from run_test() when test.palette_mode is true. Differences
        vs run_interactive_test():
          - Uses execute_interactive_palette() (xterm-256color PTY, no
            FRONTIER_PLAIN_REPL, deterministic geometry).
          - Honors screenshot_match steps via pyte; mismatches surface as
            test failures whose error message contains the unified diff.
        """
        test_env = test.environment.copy()
        results_dir = os.path.join(self.test_root_dir, 'tmp', 'results')

        result = self.cli.execute_interactive_palette(
            interactive_steps=test.interactive_steps,
            timeout=test.timeout,
            env=test_env,
            test_name=test.name,
            results_dir=results_dir,
            golden_root=self.test_root_dir,
        )

        # Screenshot mismatches arrive on stderr from
        # execute_interactive_palette(); surface them as the primary error.
        if result.stderr and 'screenshot mismatch' in result.stderr.lower():
            details = None
            if self.verbose:
                details = (
                    f"Exit code: {result.returncode}\n"
                    f"Stdout:\n{result.stdout}\n"
                    f"Stderr:\n{result.stderr}"
                )
            return TestResult(test.name, False, result.stderr, details)

        passed, error = test.validate_repl_output(result)

        details = None
        if not passed and self.verbose:
            details = (
                f"Exit code: {result.returncode}\n"
                f"Stdout:\n{result.stdout}\n"
                f"Stderr:\n{result.stderr}"
            )

        return TestResult(test.name, passed, error, details)

    def run_test_file(self, yaml_path: str) -> List[TestResult]:
        """Run all tests in a YAML file."""
        test_file = Path(yaml_path).name
        print(f"\nRunning tests from: {test_file}")

        test_cases = self.load_test_file(yaml_path)
        print(f"  Found {len(test_cases)} test(s)")

        # Temporarily disable protocol executor for non-protocol files
        saved_executor = None
        if test_file in NON_PROTOCOL_TEST_FILES and self.protocol_executor is not None:
            saved_executor = self.protocol_executor
            self.protocol_executor = None

        file_results = []
        for test in test_cases:
            result = self.run_test(test)
            file_results.append(result)
            self.results.append(result)

            # Print immediate feedback
            if result.skipped:
                status = "⊘ SKIP"
                print(f"    {status}: {result.name}")
                if self.verbose and result.details:
                    print(f"      Reason: {result.details}")
            else:
                status = "✓ PASS" if result.passed else "✗ FAIL"
                print(f"    {status}: {result.name}")
                if not result.passed:
                    print(f"      Error: {result.error}")
                    if result.details:
                        print(f"      {result.details}")

        # Restore protocol executor if it was temporarily disabled
        if saved_executor is not None:
            self.protocol_executor = saved_executor

        return file_results

    def cleanup_test_artifacts(self):
        """Clean up temporary test files and directories created during test execution."""
        test_tmp_dir = os.path.join(self.test_root_dir, 'tmp', 'integration')
        if os.path.exists(test_tmp_dir):
            try:
                shutil.rmtree(test_tmp_dir)
                if self.verbose:
                    print(f"\nCleaned up test artifacts: {test_tmp_dir}")
            except Exception as e:
                print(f"Warning: Failed to clean up test artifacts: {e}", file=sys.stderr)

    def print_summary(self, baseline: Optional[Dict[str, str]] = None,
                      time_note: Optional[str] = None):
        """Print test summary; returns True when the run should exit 0.

        With a baseline (name -> reason map from load_baseline()):
        - failures whose name IS on the list are reported as
          "known-fail (baselined)" and do NOT fail the run;
        - failures whose name is NOT on the list fail the run;
        - a PASS whose name is on the list is loudly reported as
          "baselined test now passes" and FAILS the run, so the
          baseline cannot rot;
        - baseline entries matching no executed test warn only
          (targeted runs execute a subset of the suite);
        - a baseline min_total directive below the collected result
          count fails the run (collapsed-discovery guard).
        """
        total = len(self.results)
        skipped = sum(1 for r in self.results if r.skipped)
        passed = sum(1 for r in self.results if r.passed and not r.skipped)
        failed = total - passed - skipped

        if baseline is None:
            unexpected = [r for r in self.results
                          if not r.passed and not r.skipped]
            known: List[TestResult] = []
            stale: List[TestResult] = []
            flaky_passes: List[TestResult] = []
            baselined_skips: List[TestResult] = []
            unmatched: List[str] = []
        else:
            unexpected, known, stale, flaky_passes, baselined_skips, unmatched = \
                classify_against_baseline(self.results, baseline)
        min_total = getattr(baseline, 'min_total', None) \
            if baseline is not None else None
        suite_too_small = min_total is not None and total < min_total

        print("\n" + "=" * 70)
        print("TEST SUMMARY")
        print("=" * 70)
        print(f"Total:   {total}")
        print(f"Passed:  {passed}")
        print(f"Skipped: {skipped}")
        if baseline is not None:
            print(f"Known-fail (baselined): {len(known)}")
            print(f"Failed:  {len(unexpected)}")
        else:
            print(f"Failed:  {failed}")
        if time_note:
            print(f"Time:    {time_note}")

        if known:
            print("\nKnown failures (baselined -- not failing the run):")
            for result in known:
                reason = baseline.get(result.name, '')
                suffix = f" [{reason}]" if reason else ""
                print(f"  ~ known-fail (baselined): {result.name}{suffix}")

        if unexpected:
            print("\nFailed tests:")
            for result in unexpected:
                print(f"  - {result.name}: {result.error}")

        if stale:
            print("\n" + "!" * 70)
            print("BASELINED TEST NOW PASSES -- remove it from the baseline file:")
            for result in stale:
                print(f"  + {result.name}")
            print("A stale baseline entry fails the run so the list cannot rot.")
            print("!" * 70)

        if flaky_passes:
            print("\nFlaky baselined tests that passed this run (entry kept):")
            for result in flaky_passes:
                print(f"  ~ {result.name} [{baseline.get(result.name, '')}]")

        if baselined_skips:
            print("\nBaselined entries SKIPPED this run (check whether the "
                  "entry is still needed):")
            for result in baselined_skips:
                print(f"  ~ {result.name}")

        if unmatched:
            print("\nWarning: baseline entries did not run this invocation")
            print("(renamed/removed test, or a targeted run excluded them):")
            for name in unmatched:
                print(f"  ? {name}")

        if suite_too_small:
            print("\n" + "!" * 70)
            print(f"SUITE SIZE BELOW BASELINE FLOOR: collected {total} results "
                  f"but the baseline file requires min_total: {min_total}.")
            print("Test discovery has likely collapsed (missing YAML files or "
                  "a broken glob); failing the run.")
            print("!" * 70)

        print("=" * 70)

        return (len(unexpected) == 0 and len(stale) == 0
                and not suite_too_small)

    def save_run_summary(self, duration_seconds: float, workers: int, batch_mode: bool,
                         baseline: Optional[Dict[str, str]] = None):
        """Write a JSON summary of the test run to tmp/integration/last_run.json."""
        total = len(self.results)
        skipped = sum(1 for r in self.results if r.skipped)
        passed = sum(1 for r in self.results if r.passed and not r.skipped)
        failed = total - passed - skipped

        summary = {
            'timestamp': datetime.now(timezone.utc).isoformat(),
            'total': total,
            'passed': passed,
            'skipped': skipped,
            'failed': failed,
            'failures': sorted(
                ({'name': r.name, 'error': r.error} for r in self.results
                 if not r.passed and not r.skipped),
                key=lambda d: d['name']),
            'duration_seconds': round(duration_seconds, 1),
            'workers': workers,
            'batch_mode': batch_mode,
        }
        if baseline is not None:
            unexpected, known, stale, flaky_passes, baselined_skips, unmatched = \
                classify_against_baseline(self.results, baseline)
            summary['known_failed'] = len(known)
            summary['unexpected_failed'] = len(unexpected)
            summary['stale_baseline_passes'] = sorted(r.name for r in stale)
            summary['flaky_baseline_passes'] = sorted(r.name for r in flaky_passes)
            summary['baselined_skipped'] = sorted(r.name for r in baselined_skips)
            summary['baseline_entries_not_run'] = unmatched
            summary['baseline_min_total'] = getattr(baseline, 'min_total', None)

        output_dir = os.path.join(self.test_root_dir, 'tmp', 'integration')
        os.makedirs(output_dir, exist_ok=True)
        output_path = os.path.join(output_dir, 'last_run.json')

        with open(output_path, 'w') as f:
            json.dump(summary, f, indent=2)
            f.write('\n')


class Baseline(dict):
    """Known-failure name -> reason map plus file-level directives.

    min_total: minimum plausible suite size from a '#min_total: N'
    comment-directive, or None. Guards against a collapsed test
    discovery (only the baselined tests running, all failing) reading
    as a clean exit-0 run.
    """

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.min_total: Optional[int] = None


def load_baseline(path: str) -> Baseline:
    """Parse a known-failures baseline file into a Baseline map.

    Format: one expected-failing test per line,
        <exact test name> | <one-line reason, tracking issue>
    The ' | ' separator and reason are optional (a bare name is a valid
    entry). Blank lines and lines whose first non-space character is '#'
    are comments -- except the '#min_total: N' directive, which records
    the minimum plausible suite size (see Baseline). Names are matched
    exactly against TestResult.name. Duplicate names warn on stderr;
    the last occurrence wins.

    Raises OSError when the file cannot be read -- the caller decides
    whether a missing baseline is fatal.
    """
    baseline = Baseline()
    with open(path, encoding='utf-8') as f:
        for raw in f:
            line = raw.strip()
            if not line:
                continue
            if line.startswith('#'):
                m = re.match(r'#\s*min_total:\s*(\d+)$', line)
                if m:
                    baseline.min_total = int(m.group(1))
                continue
            if ' | ' in line:
                name, reason = line.split(' | ', 1)
                name, reason = name.strip(), reason.strip()
            else:
                name, reason = line, ''
            if name in baseline:
                print(f"Warning: duplicate baseline entry {name!r} in {path} "
                      f"(last occurrence wins)", file=sys.stderr)
            baseline[name] = reason
    return baseline


def baseline_entry_is_flaky(reason: str) -> bool:
    """True when a baseline entry's reason marks the test flaky.

    A 'flaky:' PREFIX (case-insensitive) on the reason marks a test whose
    outcome is nondeterministic (e.g. order-dependent state): its failures
    are baselined AND its passes are exempt from the stale-entry rule.
    """
    return reason.strip().lower().startswith('flaky:')


def classify_against_baseline(results: List[TestResult],
                              baseline: Dict[str, str]
                              ) -> Tuple[List[TestResult], List[TestResult],
                                         List[TestResult], List[TestResult],
                                         List[str]]:
    """Split results against a baseline.

    Returns (unexpected_failures, known_failures, stale_passes,
    flaky_passes, baselined_skips, unmatched_names). Skipped results are
    neutral for exit semantics: they are neither known failures nor
    stale passes, but they do count as "the test ran" for
    unmatched-entry detection (a skip is a deliberate state, not
    baseline rot); baselined skips are returned for informational
    reporting. A pass on an entry marked flaky (see
    baseline_entry_is_flaky) is reported informationally instead of
    failing the run as stale.

    Test names are only unique per YAML file, so the same name can
    appear in several results (e.g. one file's copy fails while
    another's passes). For a baselined name with both outcomes the fail
    wins: the name classifies as known-fail and its pass occurrences
    are suppressed from both the stale-pass and flaky-pass buckets.
    """
    names_seen = set()
    failed_names = {r.name for r in results if not r.passed and not r.skipped}
    unexpected: List[TestResult] = []
    known: List[TestResult] = []
    stale: List[TestResult] = []
    flaky_passes: List[TestResult] = []
    baselined_skips: List[TestResult] = []
    for r in results:
        names_seen.add(r.name)
        if r.skipped:
            if r.name in baseline:
                baselined_skips.append(r)
            continue
        if r.passed:
            if r.name in baseline and r.name not in failed_names:
                if baseline_entry_is_flaky(baseline[r.name]):
                    flaky_passes.append(r)
                else:
                    stale.append(r)
        elif r.name in baseline:
            known.append(r)
        else:
            unexpected.append(r)
    unmatched = sorted(n for n in baseline if n not in names_seen)
    return unexpected, known, stale, flaky_passes, baselined_skips, unmatched


def _collect_test_names_per_file(yaml_paths: List[str]) -> Dict[str, set]:
    """Scan YAML files and return a map of file path -> set of test names.

    Used by --select to (a) validate that every selector matches at least
    one test before launching workers, and (b) drop files with zero matches
    from dispatch so workers don't pay per-file DB-copy cost for nothing.
    Parse errors are surfaced as warnings; the file is skipped (its tests
    won't be discoverable by --select).
    """
    per_file: Dict[str, set] = {}
    for path in yaml_paths:
        try:
            with open(path, encoding='utf-8') as f:
                data = yaml.safe_load(f)
        except (OSError, yaml.YAMLError) as e:
            print(f"Warning: failed to read test names from {path}: {e}",
                  file=sys.stderr)
            per_file[path] = set()
            continue
        names: set = set()
        if isinstance(data, dict):
            for td in data.get('tests', []) or []:
                if isinstance(td, dict):
                    names.add(td.get('name', 'Unnamed Test'))
        per_file[path] = names
    return per_file


def _collect_test_names(yaml_paths: List[str]) -> set:
    """Scan YAML files and return the union of all test names found."""
    names: set = set()
    for file_names in _collect_test_names_per_file(yaml_paths).values():
        names |= file_names
    return names


def _find_test_root(test_files: List[str]) -> str:
    """Find project root by walking up from the first test file."""
    if test_files:
        test_file_path = Path(test_files[0]).resolve()
        current = test_file_path.parent
        while current != current.parent:
            if (current / '.git').exists() or (current / 'Makefile').exists():
                return str(current)
            current = current.parent
    return str(Path.cwd())


def main():
    parser = argparse.ArgumentParser(description="Run Frontier integration tests")
    parser.add_argument('test_files', nargs='+', help='YAML test files to run')
    parser.add_argument('--cli', default='./frontier-cli/frontier-cli',
                       help='Path to frontier-cli binary')
    parser.add_argument('--system-root', help='Path to system root database')
    parser.add_argument('--verbose', '-v', action='store_true',
                       help='Verbose output')
    parser.add_argument('--batch', dest='batch', action='store_true', default=True,
                       help='Use NDJSON protocol for batch execution (default: on)')
    parser.add_argument('--no-batch', dest='batch', action='store_false',
                       help='Disable NDJSON protocol, use per-process execution')
    parser.add_argument('-j', '--workers', type=int, default=0,
                       help='Number of parallel workers (0 = auto, 1 = sequential)')
    parser.add_argument('--select', action='append', default=None, metavar='TEST_NAME',
                       help='Run only tests whose name exactly matches TEST_NAME. '
                            'Pass multiple times to select multiple tests. '
                            'File arguments are still required.')
    parser.add_argument('--baseline', metavar='FILE', default=None,
                       help='Known-failure baseline file (see '
                            'tests/integration/known_failures.txt). Failures on '
                            'the list become "known-fail (baselined)" and do not '
                            'fail the run; failures off the list still fail it; '
                            'passes on the list fail it (stale entry).')

    args = parser.parse_args()

    baseline = None
    if args.baseline:
        try:
            baseline = load_baseline(args.baseline)
        except OSError as e:
            print(f"Error: cannot read baseline file: {e}", file=sys.stderr)
            return 1
        print(f"Known-failure baseline: {args.baseline} ({len(baseline)} entries)")

    # Resolve worker count
    if args.workers == 0:
        args.workers = min(multiprocessing.cpu_count(), 8)

    # Initialize CLI wrapper (for validation)
    try:
        cli = FrontierCLI(args.cli, args.system_root)
    except FileNotFoundError as e:
        print(f"Error: {e}", file=sys.stderr)
        return 1

    test_root_dir = _find_test_root(args.test_files)

    # Filter valid test files
    valid_files = []
    for tf in args.test_files:
        if os.path.exists(tf):
            valid_files.append(tf)
        else:
            print(f"Warning: Test file not found: {tf}", file=sys.stderr)

    if not valid_files:
        print("No valid test files found", file=sys.stderr)
        return 1

    # Resolve --select into a set; verify every selector matches at least one
    # test name in the provided files (fail fast on typos / stale names).
    # Then drop files with zero matches so workers don't pay per-file
    # DB-copy / Guest-Databases-copytree cost only to filter to nothing.
    selectors = None
    if args.select:
        selectors = set(args.select)
        names_per_file = _collect_test_names_per_file(valid_files)
        all_names: set = set()
        for file_names in names_per_file.values():
            all_names |= file_names
        unmatched = sorted(s for s in selectors if s not in all_names)
        if unmatched:
            print("Error: --select names did not match any test in the given files:",
                  file=sys.stderr)
            for name in unmatched:
                print(f"  - {name!r}", file=sys.stderr)
            return 1
        valid_files = [f for f in valid_files
                       if names_per_file.get(f, set()) & selectors]

    # === Sequential mode (j=1) or single file ===
    if args.workers == 1 or len(valid_files) == 1:
        executor = None
        if args.batch:
            executor = ProtocolExecutor(args.cli, args.system_root)
            try:
                executor.start()
            except Exception as e:
                print(f"Warning: Failed to start protocol executor: {e}", file=sys.stderr)
                executor = None

        runner = TestRunner(cli, verbose=args.verbose, test_root_dir=test_root_dir,
                            protocol_executor=executor, selectors=selectors)
        runner.cleanup_test_artifacts()

        seq_start = time.time()
        for test_file in valid_files:
            runner.run_test_file(test_file)
        seq_elapsed = time.time() - seq_start

        if executor:
            executor.stop()

        runner.cleanup_test_artifacts()
        all_passed = runner.print_summary(baseline=baseline)
        runner.save_run_summary(seq_elapsed, args.workers, args.batch,
                                baseline=baseline)
        return 0 if all_passed else 1

    # === Parallel mode ===
    # Separate files into sequential and parallel buckets
    sequential_files = []
    parallel_files = []
    for f in valid_files:
        basename = Path(f).name
        meta = load_file_metadata(f)
        if meta.get('sequential', False) or basename in SEQUENTIAL_TEST_FILES:
            sequential_files.append(f)
        else:
            parallel_files.append(f)

    all_results: List[TestResult] = []
    start_time = time.time()

    # Clean up stale temp dirs before parallel run
    test_tmp_dir = os.path.join(test_root_dir, 'tmp', 'integration')
    if os.path.exists(test_tmp_dir):
        shutil.rmtree(test_tmp_dir, ignore_errors=True)

    # Run parallel-safe files across workers
    if parallel_files:
        worker_args = []
        for i, f in enumerate(parallel_files):
            worker_args.append((
                f, args.cli, args.system_root, test_root_dir,
                args.batch, args.verbose, i, selectors
            ))

        print(f"\nRunning {len(parallel_files)} test file(s) across {min(args.workers, len(parallel_files))} worker(s)...")

        with concurrent.futures.ProcessPoolExecutor(max_workers=args.workers) as pool:
            futures = {pool.submit(_run_file_worker, wa): wa[0] for wa in worker_args}
            for future in concurrent.futures.as_completed(futures):
                yaml_path = futures[future]
                try:
                    worker_result = future.result()
                    file_name = worker_result['file_name']
                    file_results = [_deserialize_test_result(d) for d in worker_result['results']]

                    # Print results for this file
                    passed_count = sum(1 for r in file_results if r.passed and not r.skipped)
                    failed_count = sum(1 for r in file_results if not r.passed and not r.skipped)
                    skipped_count = sum(1 for r in file_results if r.skipped)
                    total = len(file_results)

                    status_parts = []
                    if passed_count:
                        status_parts.append(f"{passed_count} passed")
                    if failed_count:
                        status_parts.append(f"{failed_count} FAILED")
                    if skipped_count:
                        status_parts.append(f"{skipped_count} skipped")
                    print(f"  {file_name}: {total} tests ({', '.join(status_parts)})")

                    # Show failures
                    for r in file_results:
                        if not r.passed and not r.skipped:
                            print(f"    ✗ FAIL: {r.name}: {r.error}")

                    all_results.extend(file_results)

                except Exception as e:
                    print(f"  ERROR: Worker failed for {Path(yaml_path).name}: {e}", file=sys.stderr)

    # Run sequential files in main process
    if sequential_files:
        print(f"\nRunning {len(sequential_files)} sequential test file(s)...")
        executor = None
        if args.batch:
            executor = ProtocolExecutor(args.cli, args.system_root)
            try:
                executor.start()
            except Exception:
                executor = None

        runner = TestRunner(cli, verbose=args.verbose, test_root_dir=test_root_dir,
                            protocol_executor=executor, selectors=selectors)
        for f in sequential_files:
            runner.run_test_file(f)

        if executor:
            executor.stop()

        all_results.extend(runner.results)

    elapsed = time.time() - start_time

    # Clean up temp dirs after parallel run
    if os.path.exists(test_tmp_dir):
        shutil.rmtree(test_tmp_dir, ignore_errors=True)

    # Print unified summary (shared with the sequential path so the
    # baseline classification and exit semantics cannot diverge).
    summary_runner = TestRunner(cli, test_root_dir=test_root_dir)
    summary_runner.results = all_results
    time_note = (f"{elapsed:.1f}s ({args.workers} workers, "
                 f"batch={'on' if args.batch else 'off'})")
    all_passed = summary_runner.print_summary(baseline=baseline,
                                              time_note=time_note)
    summary_runner.save_run_summary(elapsed, args.workers, args.batch,
                                    baseline=baseline)

    return 0 if all_passed else 1


if __name__ == '__main__':
    sys.exit(main())
