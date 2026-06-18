"""
C.0.3 + C.0.7 — Slash-menu palette.

Manual-list items covered:
  - '/' alone opens palette (after 350ms debounce — C.0.7a)
  - ★ Window is correct size on first open (NOT 7-8 line bordered window — C.0.7b bug #3)
  - ★ Ctrl-C closes palette (C.0.7c — was silently dropped)
  - Escape closes palette
  - ★ '/h<Tab>' completes inline without opening palette (C.0.7a slash debounce)
"""
import time

from tui_harness import TUI, snapshot


def test_slash_alone_opens_palette():
    """Type '/' alone, wait for the 350ms debounce, palette opens."""
    with TUI(boot_wait=1.5) as t:
        t.wait_for(">", timeout=5)
        t.send("/")
        # Slash debounce is 350ms — wait a bit longer for the palette to draw
        time.sleep(0.6)
        text = t.capture()
        snapshot(t, "palette-opened")
        # Palette draws a top-row menubar.  Check for ANY known top-level
        # menu name OR for the modal window borders.
        # Boxen draws box-drawing chars for borders: ╔ ═ ╗ ║ ╚ ╝ (double-line)
        assert any(
            ch in text for ch in "╔═╗║╚╝"
        ), f"no palette window border visible:\n{text}"


def test_palette_window_geometry_is_compact():
    """★ C.0.7b crash bug #3 regression: palette must open as compact
    1-row menubar (3 rows incl borders), NOT 7-8 lines.
    """
    with TUI(boot_wait=1.5) as t:
        t.wait_for(">", timeout=5)
        t.send("/")
        time.sleep(0.6)
        text = t.capture()
        snapshot(t, "palette-geometry-compact")
        # Count contiguous rows containing palette borders.
        rows = text.split("\n")
        border_rows = [i for i, r in enumerate(rows) if any(c in r for c in "╔═╗║╚╝")]
        # Compact palette has borders on its top + bottom; allowed up to
        # 6 rows total (some slack for layout differences).  The bug
        # produced 9+ rows.
        if border_rows:
            span = border_rows[-1] - border_rows[0] + 1
            assert span <= 6, (
                f"palette window too tall: {span} rows of borders. "
                f"Bug regressed (was 7-8 rows pre-fix).\n{text}"
            )


def test_escape_closes_palette():
    with TUI(boot_wait=1.5) as t:
        t.wait_for(">", timeout=5)
        t.send("/")
        time.sleep(0.6)
        t.wait_for("╔", timeout=2)  # palette is open
        t.send_key("Escape")
        time.sleep(0.3)
        text = t.capture()
        snapshot(t, "palette-after-escape")
        # After close, no border chars in the upper rows
        upper = "\n".join(text.split("\n")[:18])
        assert not any(c in upper for c in "╔╗╚╝"), (
            f"palette borders still visible after Escape:\n{text}"
        )


def test_ctrl_c_closes_palette():
    """★ C.0.7c regression: Ctrl-C with palette open closes the palette
    AND does NOT exit the REPL.
    """
    with TUI(boot_wait=1.5) as t:
        t.wait_for(">", timeout=5)
        t.send("/")
        time.sleep(0.6)
        t.wait_for("╔", timeout=2)  # palette is open
        t.send_key("C-c")
        time.sleep(0.3)
        text = t.capture()
        snapshot(t, "palette-after-ctrlc")
        # Palette should be gone
        upper = "\n".join(text.split("\n")[:18])
        assert not any(c in upper for c in "╔╗╚╝"), (
            f"palette borders still visible after Ctrl-C — C.0.7c regression.\n{text}"
        )
        # REPL must still be alive (didn't exit)
        assert t.is_alive(), "Ctrl-C in palette exited the REPL — C.0.7c regression"


def test_slash_h_tab_completes_help():
    """★ C.0.7a regression: '/h<Tab>' must complete to '/help' INLINE
    (single candidate), not open the palette.
    Before fix: palette popped on '/' before user could type 'h'.
    """
    with TUI(boot_wait=1.5) as t:
        t.wait_for(">", timeout=5)
        t.send("/h")
        t.send_key("Tab")
        time.sleep(0.4)
        text = t.capture()
        snapshot(t, "slash-h-tab-completes")
        # Input bar should show '/help' (or at least '/h' followed by more)
        # AND no palette borders should be visible
        assert "/help" in text, (
            f"'/h<Tab>' did not complete to '/help'.  Could be Cluster A "
            f"regression (palette opened before completion).\n{text}"
        )
        upper = "\n".join(text.split("\n")[:18])
        assert not any(c in upper for c in "╔╗╚╝"), (
            f"palette opened during '/h<Tab>' — Cluster A regression.\n{text}"
        )
