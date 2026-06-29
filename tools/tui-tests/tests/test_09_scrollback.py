"""
#803 -- output-pane scrollback (PgUp/PgDn).

The boxen REPL owns the alternate screen buffer, so terminal-native
scrollback can't reach lines that scroll off the top of the output pane.
These tests drive PgUp/PgDn end-to-end through the REPL to verify the
user can review output that has scrolled off the visible region.

Coverage:
  - PgUp reveals older content that had scrolled off the top
  - PgDn returns toward the bottom
  - Submitting a new expression auto-snaps the view back to the bottom
    (scroll-on-output -- typing unsticks the view)
  - The footer hint advertises the new binding
"""
import time

from tui_harness import TUI, snapshot


def _push_many_lines(t, n: int = 60):
    """Dispatch `n` expressions, each producing one scrollback line.

    Uses tiny string-literal expressions so the result is short and
    deterministic regardless of how UserTalk formats numeric output.
    """
    for i in range(n):
        t.send(f'"L{i:03d}"')
        t.send_key("Enter")
    # Let the final dispatch settle before the test inspects the pane.
    time.sleep(0.5)


def test_pgup_reveals_lines_that_scrolled_off_top():
    """Fill scrollback past the visible region, press PgUp, assert older
    lines are now visible that were not visible before."""
    with TUI(boot_wait=1.5) as t:
        t.wait_for(">", timeout=5)
        _push_many_lines(t, n=60)

        bottom = t.capture()
        snapshot(t, "scrollback-before-pgup")

        # The earliest line ("L000") should have scrolled off-screen.
        # The most-recent should still be visible.
        assert "L000" not in bottom, (
            f"sanity: L000 should have scrolled off the top before PgUp; got:\n{bottom}"
        )
        assert "L059" in bottom, (
            f"sanity: L059 (newest) should still be visible; got:\n{bottom}"
        )

        # PgUp once -- one page-step (output_h - 1) up.
        t.send_key("PgUp")
        time.sleep(0.3)
        scrolled = t.capture()
        snapshot(t, "scrollback-after-one-pgup")

        # After one PgUp we expect to see lines that were NOT in the
        # pre-PgUp view.  Each echo+result occupies 2 scrollback rows
        # (the echoed "> " prompt and the result line), so an output
        # pane of ~22 rows holds ~11 expression cycles -- L048-L059
        # before PgUp, walking back roughly 21 lines = ~10 cycles
        # after PgUp.  Check every "L000".."L059" and require at
        # least one that's now visible but wasn't before.
        revealed = []
        for i in range(0, 60):
            tag = f"L{i:03d}"
            if tag in scrolled and tag not in bottom:
                revealed.append(tag)
        assert revealed, (
            f"PgUp did not reveal any older lines.\n"
            f"Before:\n{bottom}\n\nAfter:\n{scrolled}"
        )


def test_pgdn_returns_to_bottom():
    """After PgUp walks the view back, PgDn returns toward the newest."""
    with TUI(boot_wait=1.5) as t:
        t.wait_for(">", timeout=5)
        _push_many_lines(t, n=60)

        # Walk well past the visible region.
        for _ in range(3):
            t.send_key("PgUp")
            time.sleep(0.15)
        time.sleep(0.2)
        scrolled = t.capture()
        snapshot(t, "scrollback-after-many-pgup")

        # Now PgDn back to bottom (more presses than needed -- handler clamps).
        for _ in range(5):
            t.send_key("PgDn")
            time.sleep(0.15)
        time.sleep(0.3)
        bottom = t.capture()
        snapshot(t, "scrollback-after-pgdn-back")

        assert "L059" in bottom, (
            f"PgDn back to bottom did not restore newest line; got:\n{bottom}"
        )


def test_submit_snaps_view_to_bottom():
    """Scroll-on-output: typing a new expression auto-snaps to the bottom."""
    with TUI(boot_wait=1.5) as t:
        t.wait_for(">", timeout=5)
        _push_many_lines(t, n=60)

        # Scroll up.
        for _ in range(2):
            t.send_key("PgUp")
            time.sleep(0.15)
        time.sleep(0.2)
        scrolled = t.capture()
        # Sanity: L059 should NOT be visible while scrolled up.
        assert "L059" not in scrolled, (
            f"sanity: after PgUp the newest line should be off-screen; got:\n{scrolled}"
        )

        # Submit a fresh expression -- view must auto-snap back to bottom.
        t.send('"SNAPPED"')
        t.send_key("Enter")
        time.sleep(0.5)
        bottom = t.capture()
        snapshot(t, "scrollback-snap-on-submit")

        assert "SNAPPED" in bottom, (
            f"submit did not snap view to bottom (new result not visible); got:\n{bottom}"
        )
        assert "L059" in bottom, (
            f"submit did not snap view to bottom (recent history not visible); got:\n{bottom}"
        )


def test_footer_advertises_pgup_pgdn():
    """The footer hint should mention PgUp/PgDn so users discover the binding."""
    with TUI(boot_wait=1.5) as t:
        text = t.wait_for(">", timeout=5)
        # Footer is on the last row of the pane.
        assert "PgUp" in text or "PgDn" in text, (
            f"footer hint does not advertise PgUp/PgDn; got:\n{text}"
        )
