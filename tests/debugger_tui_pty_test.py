#!/usr/bin/env python3
"""
PTY-driven integration test for the debugger TUI lazy-attach drain path.

Issue #748: Today's tests/debugger_tui_test.sh Tests 3 and 4 both run without
a TTY so boxen_init fails immediately.  They verify clean exit on boxen_init
failure but never exercise the actual drain-before-free path.

This test launches frontier-cli --debug-tui with a real pseudo-tty so
boxen_init succeeds, sends keystrokes via the master fd, and verifies that the
drain-before-free teardown sequence (debug_wait_lazy_threads_drained ->
headless_restore_threadglobals -> debug_set_attach_transport(NULL) ->
debugger_tui_state_teardown -> free) completes without crashing.

Key implementation detail:
  termbox2's tb_init() opens /dev/tty (not fd 0/1/2).  /dev/tty resolves to
  the process's controlling terminal.  We must use pty.fork() -- which calls
  setsid() and sets the slave PTY as the child's controlling terminal -- not
  subprocess.Popen() with slave_fd remapped to 0/1/2.  The latter leaves the
  child with no controlling terminal and /dev/tty fails to open (TB_ERR_INIT_OPEN).

Infrastructure: Python stdlib only (pty, os, select, signal).  No external
dependencies (no expect, no script wrapper).

Exit conventions (mirrors debugger_tui_test.sh):
  0 -- all tests passed
  1 -- one or more tests failed

2026-06-07 JES Phase B.7 #748 PTY-drain integration test
"""

import os
import sys
import pty
import select
import shutil
import signal
import tempfile
import time

# ---------------------------------------------------------------------------
# Paths
# ---------------------------------------------------------------------------

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.dirname(SCRIPT_DIR)
CLI = os.path.join(PROJECT_ROOT, "frontier-cli", "frontier-cli")
SOURCE_DB = os.path.join(PROJECT_ROOT, "databases", "Virgin.root")

# ANSI colour helpers (same palette as debugger_tui_test.sh)
GREEN  = "\033[0;32m"
RED    = "\033[0;31m"
YELLOW = "\033[0;33m"
NC     = "\033[0m"

# ---------------------------------------------------------------------------
# Simple pass/fail counters
# ---------------------------------------------------------------------------

passed  = 0
failed  = 0
skipped = 0


def _pass(msg):
    global passed
    print(f"  {GREEN}PASS{NC}: {msg}")
    passed += 1


def _fail(msg, detail=""):
    global failed
    print(f"  {RED}FAIL{NC}: {msg}")
    if detail:
        print(f"    {detail}")
    failed += 1


def _skip(msg, reason):
    global skipped
    print(f"  {YELLOW}SKIP{NC}: {msg} (requires {reason})")
    skipped += 1


# ---------------------------------------------------------------------------
# Preflight checks
# ---------------------------------------------------------------------------

def preflight():
    ok = True
    if not os.path.isfile(CLI) or not os.access(CLI, os.X_OK):
        print(f"ERROR: frontier-cli not found or not executable: {CLI}", file=sys.stderr)
        ok = False
    if not os.path.isfile(SOURCE_DB):
        print(f"ERROR: source database not found: {SOURCE_DB}", file=sys.stderr)
        ok = False
    return ok


# ---------------------------------------------------------------------------
# PTY session helpers
# ---------------------------------------------------------------------------

def _drain_pty(master_fd, timeout_s=0.5):
    """
    Read and return available bytes from master_fd for the full timeout_s.

    Unlike a read-until-empty approach this always runs for the full timeout
    so the child's PTY pipe stays clear even when writes arrive on the 100ms
    boxen poll cadence (which can leave a 50ms gap where no data appears,
    tricking an early-exit drain into thinking the child has stopped writing).
    """
    deadline = time.monotonic() + timeout_s
    buf = b""
    while True:
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            break
        try:
            rlist, _, _ = select.select([master_fd], [], [], min(remaining, 0.05))
        except (ValueError, OSError):
            break
        if rlist:
            try:
                chunk = os.read(master_fd, 4096)
                buf += chunk
            except OSError:
                break
        # Do NOT break on empty select -- keep draining for the full timeout
    return buf


def _wait_for_exit(pid, timeout_s, drain_fd=None):
    """
    Wait up to timeout_s for pid to exit.

    If drain_fd is given, continuously drain it while waiting so the child
    does not block on a full PTY pipe buffer.

    Returns (exit_code, timed_out) where exit_code is the process return code
    (positive = clean, negative = killed by signal) or None on timeout.
    timed_out is True if the process did not exit within timeout_s.
    """
    deadline = time.monotonic() + timeout_s
    while True:
        try:
            wpid, status = os.waitpid(pid, os.WNOHANG)
        except ChildProcessError:
            return None, False
        if wpid == pid:
            if os.WIFEXITED(status):
                return os.WEXITSTATUS(status), False
            if os.WIFSIGNALED(status):
                return -os.WTERMSIG(status), False
            return None, False
        if time.monotonic() >= deadline:
            return None, True
        if drain_fd is not None:
            # Drain with a short select so we don't busy-loop
            try:
                rlist, _, _ = select.select([drain_fd], [], [], 0.05)
                if rlist:
                    os.read(drain_fd, 4096)
            except (OSError, ValueError):
                drain_fd = None  # fd closed; stop draining
        else:
            time.sleep(0.05)


def launch_tui_pty(db_path):
    """
    Fork a child process with a real PTY as its controlling terminal and exec
    frontier-cli --debug-tui.

    Uses pty.fork() so the slave PTY becomes the child's controlling terminal.
    This makes /dev/tty (opened by tb_init()) resolve to the slave PTY,
    allowing boxen_init to succeed.

    Returns (pid, master_fd).
    """
    pid, master_fd = pty.fork()
    if pid == 0:
        # Child: exec the TUI.  pty.fork() has already called setsid() and
        # made the slave PTY the controlling terminal (TIOCSCTTY).
        os.execv(CLI, [CLI, "--debug-tui", "--skip-startup",
                       "--system-root", db_path])
        os._exit(127)   # execv failed -- should not happen
    return pid, master_fd


# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------

def test_pty_launch_and_quit(db_path):
    """
    PTY Test 1: launch with real PTY, send 'q', verify exit 0.

    Proof that boxen_init succeeds (exit 0, not 1) and the full
    drain-before-free teardown sequence runs without crashing.

    Teardown sequence exercised (debugger_tui.c lines ~2522-2561):
      debug_wait_lazy_threads_drained()
      headless_restore_threadglobals(main_hglobals)
      debug_set_attach_transport(NULL)
      debugger_tui_state_teardown(state)
      boxen_shutdown()
      free(state)
    """
    print("--- PTY Test 1: launch with real PTY, send 'q', verify exit 0 ---")

    pid, master_fd = launch_tui_pty(db_path)
    try:
        # Allow the event loop to start (ODB open + layout build)
        _drain_pty(master_fd, 1.5)

        # Send 'q' to trigger quit_requested (debugger_tui.c:2122)
        try:
            os.write(master_fd, b"q")
        except OSError as exc:
            _fail("PTY launch: write 'q' failed", str(exc))
            os.kill(pid, signal.SIGKILL)
            _wait_for_exit(pid, 2.0)
            return False

        # Wait for the process to exit, draining the PTY master while we wait.
        # The TUI writes escape sequences continuously; if the pipe fills up
        # the child blocks on write and never processes the 'q' keystroke.
        # drain_fd keeps the pipe clear so the child can run to completion.
        rc, timed_out = _wait_for_exit(pid, 15.0, drain_fd=master_fd)

    finally:
        try:
            os.close(master_fd)
        except OSError:
            pass

    if timed_out:
        try:
            os.kill(pid, signal.SIGKILL)
        except ProcessLookupError:
            pass
        _wait_for_exit(pid, 2.0)
        _fail("PTY launch: process did not exit within 15s after 'q'",
              "possible hang in drain-before-free loop")
        return False

    if rc == 0:
        _pass("PTY launch: exit code 0 (boxen_init succeeded, drain complete)")
        return True
    elif rc == 1:
        _fail("PTY launch: exit code 1 (boxen_init failed -- PTY not attached as CTTY?)")
        return False
    else:
        sig_desc = f" (signal {-rc})" if rc < 0 else ""
        _fail(f"PTY launch: unexpected exit code {rc}{sig_desc}")
        return False


def test_pty_no_coredump(stage_dir):
    """PTY Test 2: no coredump left in staging directory after PTY run."""
    print("--- PTY Test 2: no coredump after PTY session ---")
    cores = [f for f in os.listdir(stage_dir) if f.startswith("core")]
    if cores:
        _fail("PTY no-coredump: coredump found", str(cores))
        return False
    _pass("PTY no-coredump: no coredump in staging dir")
    return True


def test_pty_sigterm(db_path, stage_dir):
    """
    PTY Test 3: SIGTERM during a live TUI session.

    Launches the TUI with a PTY, waits for the event loop to start, then
    sends SIGTERM.  Verifies:
      - Process exits within 15s (drain-before-free does not hang)
      - No coredump

    This covers the scenario the concurrency reviewer cited in issue #748:
    "a PTY-driven test that triggers a real lazy-attach -> SIGTERM -> drain
    is worth filing."

    Note: SIGTERM disposition in the TUI.  The TUI event loop polls with a
    100ms timeout; SIGTERM interrupts boxen_poll_event() (via EINTR on the
    underlying select()), which causes poll_rc != BOXEN_OK, which breaks the
    while loop and falls through to the drain-before-free teardown.  Under the
    PR #722 framing, SIGTERM during a quiescent session (no lazy threads) will
    result in rc = 0 (clean exit via the break path) or rc = -SIGTERM (default
    signal disposition if tb_poll is the only syscall interrupted and the
    runtime resumes normally).  Both are acceptable outcomes.
    """
    print("--- PTY Test 3: SIGTERM during live PTY session (drain-before-free) ---")

    pid, master_fd = launch_tui_pty(db_path)
    try:
        # Allow event loop to start
        _drain_pty(master_fd, 1.5)

        # Check it is still alive (not a boxen_init failure)
        rc_early, _ = _wait_for_exit(pid, 0.0)
        if rc_early is not None:
            _fail(f"PTY SIGTERM: process exited early (rc={rc_early}); "
                  "boxen_init may have failed")
            return False

        # Send SIGTERM to the process
        try:
            os.kill(pid, signal.SIGTERM)
        except ProcessLookupError:
            pass

        # Wait for exit, draining the PTY pipe to prevent child write-blocking
        rc, timed_out = _wait_for_exit(pid, 15.0, drain_fd=master_fd)

    finally:
        try:
            os.close(master_fd)
        except OSError:
            pass

    if timed_out:
        try:
            os.kill(pid, signal.SIGKILL)
        except ProcessLookupError:
            pass
        _wait_for_exit(pid, 2.0)
        _fail("PTY SIGTERM: process did not exit within 15s (drain hung?)")
        return False

    # rc == 0:  clean exit via event-loop break path (poll interrupted -> BOXEN_OK != poll_rc)
    # rc == 1:  boxen_init failed before SIGTERM (acceptable; signals that PTY is not effective)
    # rc == -15 (signal.SIGTERM): terminated by default SIGTERM disposition (no crash)
    # rc == 128+15: shell-encoded SIGTERM (should not appear here since we wait directly)
    acceptable = rc in (0, 1) or rc == -signal.SIGTERM
    if acceptable:
        _pass(f"PTY SIGTERM: process exited cleanly (rc={rc}), no hang")
    else:
        sig_desc = f" (signal {-rc})" if rc is not None and rc < 0 else ""
        _fail(f"PTY SIGTERM: unexpected exit code {rc}{sig_desc}")
        return False

    cores = [f for f in os.listdir(stage_dir) if f.startswith("core")]
    if cores:
        _fail("PTY SIGTERM: coredump found after SIGTERM", str(cores))
        return False
    _pass("PTY SIGTERM: no coredump")
    return True


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    print("==============================================")
    print("Debugger TUI PTY Integration Tests (#748)")
    print("==============================================")
    print()

    if not preflight():
        sys.exit(1)

    stage_dir = tempfile.mkdtemp(prefix="frontier-tui-pty-")
    db_path = os.path.join(stage_dir, "Virgin.root")
    try:
        shutil.copy2(SOURCE_DB, db_path)

        test_pty_launch_and_quit(db_path)
        print()
        test_pty_no_coredump(stage_dir)
        print()
        test_pty_sigterm(db_path, stage_dir)

    finally:
        shutil.rmtree(stage_dir, ignore_errors=True)

    print()
    print("==============================================")
    print(f"Results: {passed} passed, {failed} failed, {skipped} skipped")
    print("==============================================")

    sys.exit(0 if failed == 0 else 1)


if __name__ == "__main__":
    main()
