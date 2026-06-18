"""
Diagnostic test — narrows down whether the cursor-editing failures are
real bugs or harness/timing issues.
"""
import time

from tui_harness import TUI, snapshot


def test_diagnose_left_arrow_movement():
    """Type 'helloworld', LEFT 5 times.  Capture cursor position via
    tmux at each step to confirm LEFT is being delivered.
    """
    with TUI(boot_wait=1.5) as t:
        t.wait_for(">", timeout=5)
        t.send("helloworld")
        t.wait_for("helloworld", timeout=2)
        time.sleep(0.3)
        cx_before, cy_before = t.cursor_position()
        snapshot(t, "diag-before-lefts")
        # Send 5 LEFTs
        for _ in range(5):
            t.send_key("Left")
        time.sleep(0.5)
        cx_after, cy_after = t.cursor_position()
        snapshot(t, "diag-after-5-lefts")
        # Now send a space and see if it inserts mid-buffer
        t.send(" ")
        time.sleep(0.3)
        text = t.capture()
        snapshot(t, "diag-after-space")
        # Diagnose
        print(f"\n  cursor before LEFTs: col={cx_before} row={cy_before}")
        print(f"  cursor after 5 LEFTs: col={cx_after} row={cy_after}")
        print(f"  delta: col={cx_after - cx_before}")
        print(f"  final input row: {[r for r in text.split(chr(10)) if r.strip()][-3:]}")
        # No assertion — just observe
