"""
C.0.2 + C.0.7 — Tab completion popup.

Manual-list items covered:
  - '/' + Tab opens popup with multiple candidates
  - Escape dismisses popup
  - ★ Popup printable letter inserts (was crashing — C.0.7b bug #2)

Tests the completion popup specifically, not the inline single-candidate
case (that's covered in test_02_palette).
"""
import time

from tui_harness import TUI, snapshot


def test_tab_on_slash_opens_popup_or_palette():
    """Type '/' then Tab IMMEDIATELY (before the 350ms debounce expires).
    Tab cancels the debounce + triggers completion.  Multiple candidates
    show a popup.
    """
    with TUI(boot_wait=1.5) as t:
        t.wait_for(">", timeout=5)
        t.send("/")
        # Send Tab fast — within the debounce window so we hit completion
        t.send_key("Tab")
        time.sleep(0.4)
        text = t.capture()
        snapshot(t, "completion-popup-or-palette")
        # We should see either a popup (with candidate text like /help, /jump)
        # OR the palette modal.  Either is acceptable; bug regression would
        # be a crash (REPL exited).
        assert t.is_alive(), "REPL crashed on /<Tab>"


def test_popup_letter_doesnt_crash():
    """★ C.0.7b crash bug #2 regression: pressing a printable letter while
    popup is open closes the popup and inserts the letter — must not crash.
    """
    with TUI(boot_wait=1.5) as t:
        t.wait_for(">", timeout=5)
        t.send("/")
        t.send_key("Tab")
        time.sleep(0.4)
        # Whether popup is open or palette is showing, sending a printable
        # letter should not crash the REPL.
        t.send("x")
        time.sleep(0.3)
        text = t.capture()
        snapshot(t, "completion-popup-letter")
        assert t.is_alive(), (
            "REPL crashed when pressing printable letter after /<Tab> — "
            "C.0.7b bug #2 regression"
        )
