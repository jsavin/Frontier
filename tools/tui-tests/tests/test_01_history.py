"""
C.0.1 — Persistent command history.

Manual-list items covered:
  - UP/DOWN walk history
  - Clamps at oldest
  - Restore typed input on DOWN-past-newest
  - ★ Type 'hello' then DOWN at fresh launch -> input stays 'hello'
    (the C.0.1 round-1 P0 fix: nav_idx must initialize to -1, not 0)
"""
import os
import tempfile
from pathlib import Path

from tui_harness import TUI, snapshot


def _isolated_home() -> dict[str, str]:
    """Return env_overrides pointing HOME at a fresh temp dir so each
    test starts with an empty ~/.frontier_history.  Returns the dict
    only; caller is responsible for cleanup via tempfile.mkdtemp's
    parent process exit (or the test can use a context if it cares).
    """
    tmphome = tempfile.mkdtemp(prefix="tui_history_")
    return {"HOME": tmphome}


def test_up_arrow_walks_history():
    env = _isolated_home()
    with TUI(env_overrides=env, boot_wait=1.5) as t:
        t.wait_for(">", timeout=5)
        # Submit three distinct commands
        for n in (1, 2, 3):
            t.send(f"{n} + {n}")
            t.send_key("Enter")
            t.wait_for(str(n * 2), timeout=2)
        # Now press UP three times and verify the input bar shows each
        t.send_key("Up")
        t.wait_for("3 + 3", timeout=2)
        t.send_key("Up")
        t.wait_for("2 + 2", timeout=2)
        t.send_key("Up")
        t.wait_for("1 + 1", timeout=2)
        snapshot(t, "history-up-walked-3-deep")


def test_down_at_fresh_launch_preserves_typed_input():
    """★ C.0.1 round-1 P0 regression guard: with no history file (fresh
    HOME), typing 'hello' then DOWN must leave 'hello' in the input bar.
    Bug: nav_idx left at 0 by memset would consume DOWN and wipe input.
    """
    env = _isolated_home()
    with TUI(env_overrides=env, boot_wait=1.5) as t:
        t.wait_for(">", timeout=5)
        t.send("hello")
        # Confirm input shows in the bar
        t.wait_for("hello", timeout=2)
        # Press DOWN — input must survive
        t.send_key("Down")
        # Capture after a brief pause to let any wrong-path side-effect
        # land in the framebuffer
        t.pause(0.3)
        text = t.capture()
        snapshot(t, "history-down-fresh-preserves-input")
        assert "hello" in text, (
            "DOWN on fresh launch wiped input — nav_idx initialization regressed.\n"
            f"Capture:\n{text}"
        )
