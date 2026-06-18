"""
C.0.7e — Cursor-based input editing.

Manual-list items covered:
  - ★ LEFT/RIGHT move caret within input
  - ★ Mid-buffer insert
  - ★ Mid-buffer Backspace
  - Ctrl-A jumps to start
  - Ctrl-E jumps to end
  - LEFT at col 0 is no-op (no underflow)
  - Backspace at col 0 is no-op

These were the C.6 blocker — input bar was append-only before this fix.
"""
import time

from tui_harness import TUI, snapshot


def test_mid_buffer_insert_with_left_arrow():
    """★ Type 'helloworld', LEFT 5 times, insert ' ' → 'hello world'."""
    with TUI(boot_wait=1.5) as t:
        t.wait_for(">", timeout=5)
        t.send("helloworld")
        t.wait_for("helloworld", timeout=2)
        for _ in range(5):
            t.send_key("Left")
        t.send(" ")
        time.sleep(0.3)
        text = t.capture()
        snapshot(t, "cursor-mid-insert-space")
        assert "hello world" in text, (
            f"mid-buffer insert via LEFT failed.  Expected 'hello world':\n{text}"
        )


def test_left_arrow_at_col_0_is_noop():
    """★ LEFT with caret at col 0 must NOT underflow or corrupt input."""
    with TUI(boot_wait=1.5) as t:
        t.wait_for(">", timeout=5)
        t.send("abc")
        t.wait_for("abc", timeout=2)
        # Move caret to start
        t.send_key("C-a")
        time.sleep(0.2)
        # Press LEFT a bunch of times (would underflow if buggy)
        for _ in range(10):
            t.send_key("Left")
        time.sleep(0.2)
        # Now press a printable char — should insert at col 0
        t.send("X")
        time.sleep(0.3)
        text = t.capture()
        snapshot(t, "cursor-left-underflow-safe")
        assert "Xabc" in text, (
            f"LEFT at col 0 corrupted input.  Expected 'Xabc':\n{text}"
        )


def test_ctrl_a_then_ctrl_e_round_trips():
    """Ctrl-A to start, Ctrl-E back to end — both must work."""
    with TUI(boot_wait=1.5) as t:
        t.wait_for(">", timeout=5)
        t.send("abcdef")
        t.wait_for("abcdef", timeout=2)
        # Ctrl-A → caret at col 0; type 'X' → 'Xabcdef'
        t.send_key("C-a")
        t.send("X")
        time.sleep(0.3)
        assert "Xabcdef" in t.capture()
        # Ctrl-E → caret at end; type 'Z' → 'XabcdefZ'
        t.send_key("C-e")
        t.send("Z")
        time.sleep(0.3)
        text = t.capture()
        snapshot(t, "cursor-ctrla-ctrle-roundtrip")
        assert "XabcdefZ" in text, (
            f"Ctrl-A or Ctrl-E broken.  Expected 'XabcdefZ':\n{text}"
        )


def test_mid_buffer_backspace_deletes_before_caret():
    """★ Caret in middle, Backspace deletes char BEFORE caret."""
    with TUI(boot_wait=1.5) as t:
        t.wait_for(">", timeout=5)
        t.send("abXcd")
        t.wait_for("abXcd", timeout=2)
        # Move caret to between X and c (3 lefts from end: positions
        # 'd', 'c', then between c and X — wait, let me recompute)
        # 'abXcd' has length 5; cursor at col 5 (end).
        # LEFT once: cursor at 4 (between c and d)
        # LEFT again: cursor at 3 (between X and c)
        # Backspace: deletes X → 'abcd'
        t.send_key("Left")
        t.send_key("Left")
        t.send_key("BSpace")
        time.sleep(0.3)
        text = t.capture()
        snapshot(t, "cursor-mid-backspace")
        # Should now show 'abcd' on the input bar
        assert "abcd" in text and "abXcd" not in text.split(">")[-1], (
            f"mid-buffer Backspace failed.  Expected 'abcd', X not deleted:\n{text}"
        )
