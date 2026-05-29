#!/usr/bin/env python3
"""
Frontier Integration Test Runner

Executes YAML test cases against frontier-cli and validates results.
"""

import argparse
import concurrent.futures
import json
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
    HAS_PEXPECT = True
except ImportError:
    try:
        # Fall back to vendored copy
        sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'vendor'))
        import pexpect
        HAS_PEXPECT = True
        sys.path.pop(0)
    except ImportError:
        HAS_PEXPECT = False

try:
    import pyte
    HAS_PYTE = True
except ImportError:
    try:
        # Fall back to vendored copy (mirrors the pexpect pattern above).
        sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'vendor'))
        import pyte
        HAS_PYTE = True
        sys.path.pop(0)
    except ImportError:
        HAS_PYTE = False


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
    import difflib

    actual = _normalize_screen_frame(screen)

    if os.environ.get('FRONTIER_UPDATE_GOLDENS') == '1':
        os.makedirs(os.path.dirname(os.path.abspath(golden_path)) or '.',
                    exist_ok=True)
        with open(golden_path, 'w', encoding='utf-8') as f:
            f.write(actual)
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
        # Build command for REPL mode (no --output-json, no -e)
        cmd = [self.cli_path, '--skip-startup']

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

        # Build command as argument list (avoids shell interpretation)
        cmd_args = [self.cli_path, '--skip-startup']
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
                                    results_dir: Optional[str] = None) -> subprocess.CompletedProcess:
        """
        Execute frontier-cli in palette mode for L4 screenshot-based testing.

        Parallel to execute_interactive() but tuned for tests that exercise
        the palette/REPL UI:
          - TERM=xterm-256color (preserves ANSI escapes for pyte to parse)
          - FRONTIER_PALETTE_FAST_TIMERS=1 (deterministic timing)
          - No FRONTIER_PLAIN_REPL — full event loop is needed for palette
          - dimensions=(24, 80) on the pty for stable geometry

        Each step is a dict with one of these shapes:
          - {'expect': '<pattern>', 'send': '<text>'}   (plain pexpect step)
          - {'send': '<text>'}                          (send-only)
          - {'screenshot_match': '<golden-path>'}       (drain + compare via pyte)

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
        if not HAS_PYTE:
            return subprocess.CompletedProcess(
                args=[self.cli_path],
                returncode=-1,
                stdout='',
                stderr='pyte is not installed (required for palette tests)',
            )

        cmd_args = [self.cli_path, '--skip-startup']
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

        try:
            child = pexpect.spawn(cmd_args[0], args=cmd_args[1:], timeout=timeout,
                                  env=process_env, encoding='utf-8',
                                  dimensions=(24, 80))

            for step in interactive_steps:
                if 'screenshot_match' in step:
                    # Drain pending bytes, then compare.
                    pending = drain_pty(child)
                    if pending:
                        collected_output.append(pending)
                        stream.feed(pending)
                    golden_path = step['screenshot_match']
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

                expect_pattern = step.get('expect')
                send_text = step.get('send')

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

                if send_text is not None:
                    child.sendline(send_text)

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
            try:
                child.close(force=True)
            except Exception:
                pass
            return subprocess.CompletedProcess(
                args=[self.cli_path],
                returncode=-1,
                stdout=''.join(collected_output),
                stderr=f'Palette interactive execution timed out ({timeout}s)',
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
        self._next_id = 1

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

        # Wait for response with timeout using select
        rlist, _, _ = select.select([self._proc.stdout], [], [], timeout)
        if not rlist:
            raise TimeoutError(f"Protocol response timed out after {timeout}s")

        resp_line = self._proc.stdout.readline()
        if not resp_line or resp_line.strip() == '':
            raise RuntimeError("Protocol process closed stdout (EOF)")

        try:
            resp = json.loads(resp_line)
        except json.JSONDecodeError as e:
            raise RuntimeError(f"Invalid JSON from protocol process: {resp_line!r}: {e}")

        # Verify response ID matches
        if resp.get('id') != msg_id:
            raise RuntimeError(f"Protocol ID mismatch: sent {msg_id}, got {resp.get('id')}")

        return resp

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
            self.start()
        except Exception as e:
            raise RuntimeError(f"Protocol executor restart failed: {e}") from e

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
                if not HAS_PYTE:
                    return TestResult(
                        name=test.name,
                        passed=True,
                        error=None,
                        details="pyte is required for palette_mode tests. Install with: pip3 install pyte",
                        skipped=True
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
        if test.environment:
            dedicated_executor = ProtocolExecutor(
                cli_path=self.cli.cli_path,
                system_root=self.protocol_executor.system_root if self.protocol_executor else None,
            )
            try:
                dedicated_executor.start(env_overrides=test.environment)
            except Exception as e:
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

        try:
            for step_idx, step in enumerate(test.protocol_ops):
                op = step.get('op')
                params = step.get('params', {})
                validate = step.get('validate', {})
                step_desc = step.get('description', f'step {step_idx + 1}')

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
            elif self.protocol_executor is not None:
                self.protocol_executor.reset()

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

        # Check error message contains
        if 'error_contains' in validate:
            expected_substr = validate['error_contains']
            error_obj = resp.get('error', {})
            error_msg = error_obj.get('message', '') if isinstance(error_obj, dict) else str(error_obj)
            if expected_substr not in error_msg:
                return f"[{step_desc}] Expected error containing {expected_substr!r}, got {error_msg!r}"

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

    def print_summary(self):
        """Print test summary."""
        total = len(self.results)
        skipped = sum(1 for r in self.results if r.skipped)
        passed = sum(1 for r in self.results if r.passed and not r.skipped)
        failed = total - passed - skipped

        print("\n" + "=" * 70)
        print("TEST SUMMARY")
        print("=" * 70)
        print(f"Total:   {total}")
        print(f"Passed:  {passed}")
        print(f"Skipped: {skipped}")
        print(f"Failed:  {failed}")

        if failed > 0:
            print("\nFailed tests:")
            for result in self.results:
                if not result.passed and not result.skipped:
                    print(f"  - {result.name}: {result.error}")

        print("=" * 70)

        return failed == 0

    def save_run_summary(self, duration_seconds: float, workers: int, batch_mode: bool):
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
            'duration_seconds': round(duration_seconds, 1),
            'workers': workers,
            'batch_mode': batch_mode,
        }

        output_dir = os.path.join(self.test_root_dir, 'tmp', 'integration')
        os.makedirs(output_dir, exist_ok=True)
        output_path = os.path.join(output_dir, 'last_run.json')

        with open(output_path, 'w') as f:
            json.dump(summary, f, indent=2)
            f.write('\n')


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

    args = parser.parse_args()

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
        all_passed = runner.print_summary()
        runner.save_run_summary(seq_elapsed, args.workers, args.batch)
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

    # Print unified summary
    total = len(all_results)
    skipped = sum(1 for r in all_results if r.skipped)
    passed = sum(1 for r in all_results if r.passed and not r.skipped)
    failed = total - passed - skipped

    print(f"\n{'=' * 70}")
    print("TEST SUMMARY")
    print(f"{'=' * 70}")
    print(f"Total:   {total}")
    print(f"Passed:  {passed}")
    print(f"Skipped: {skipped}")
    print(f"Failed:  {failed}")
    print(f"Time:    {elapsed:.1f}s ({args.workers} workers, batch={'on' if args.batch else 'off'})")

    if failed > 0:
        print("\nFailed tests:")
        for result in all_results:
            if not result.passed and not result.skipped:
                print(f"  - {result.name}: {result.error}")

    print(f"{'=' * 70}")

    # Save JSON summary
    summary_runner = TestRunner(cli, test_root_dir=test_root_dir)
    summary_runner.results = all_results
    summary_runner.save_run_summary(elapsed, args.workers, args.batch)

    return 0 if failed == 0 else 1


if __name__ == '__main__':
    sys.exit(main())
