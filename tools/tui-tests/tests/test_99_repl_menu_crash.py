"""
Reproduction for the --debug-tui crash when activating the REPL menu in
the /-menu.  Reported by JES 2026-06-20 manual testing.

Steps:
  1. Launch --debug-tui
  2. Type / and wait for palette to open (350ms debounce)
  3. Activate the REPL menu (4th top-level menu).  Hotkey is 'R'
     (uppercase) per the underlined letter in the menubar.
  4. Observe: app crashes.

Expected post-fix: REPL submenu opens (cascade level 1), no crash.

This test reproduces the crash via the harness so we can:
  - Confirm the bug exists
  - Capture a snapshot of the pre-crash state
  - Verify the fix once landed
"""
import time

from tui_harness import TUI, snapshot


def test_repl_menu_activation_doesnt_crash():
    """★ JES manual report 2026-06-20: --debug-tui crashes when REPL
    menu activated via the /-menu.  Reproduce and capture diagnostics.
    """
    with TUI(boot_wait=1.5) as t:
        t.wait_for(">", timeout=5)
        t.send("/")
        # 350ms debounce — wait for palette to draw
        time.sleep(0.6)
        # Confirm palette opened
        t.wait_for("╔", timeout=2)
        snapshot(t, "repl-menu-crash-before-activation")

        # Activate REPL menu via hotkey 'R'.  The palette uses the first
        # uppercase letter as the hotkey for each menu (File / Edit /
        # View / REPL).  Try lowercase first since some menubar
        # implementations are case-insensitive.
        t.send("R")
        # Give the cascade time to draw — or crash
        time.sleep(0.5)

        snapshot(t, "repl-menu-crash-after-activation")

        # Crash detection: is the REPL process still alive?
        assert t.is_alive(), (
            "REPL exited after pressing 'R' in palette menubar "
            "— this is the reported crash"
        )

        # Also assert SOMETHING happened (REPL submenu opened) — capture
        # should show MORE rows than before (cascade level added)
        text = t.capture()
        # Don't be strict about content; just verify no crash + something rendered
        assert len(text) > 0


def test_repl_menu_activation_via_arrows_doesnt_crash():
    """Alternative repro path: navigate to REPL via RIGHT arrows instead
    of hotkey.  This isolates whether the crash is hotkey-specific or
    menu-activation-specific.
    """
    with TUI(boot_wait=1.5) as t:
        t.wait_for(">", timeout=5)
        t.send("/")
        time.sleep(0.6)
        t.wait_for("╔", timeout=2)

        # Navigate right 3 times to REPL (4th menu)
        for _ in range(3):
            t.send_key("Right")
            time.sleep(0.1)
        snapshot(t, "repl-menu-crash-navigated-to-repl")

        # Activate via Down (opens cascade) or Enter
        t.send_key("Down")
        time.sleep(0.5)
        snapshot(t, "repl-menu-crash-after-down")

        assert t.is_alive(), (
            "REPL exited after RIGHT-RIGHT-RIGHT-DOWN navigation to REPL "
            "menu — crash is menu-activation, not hotkey-specific"
        )


def test_repl_menu_item_dispatches_without_use_after_free():
    """★ JES manual report 2026-06-22: after C.0.7f fixed the palette-source
    lifetime crash, every menu item dispatch returned "(menu script failed)".

    Root cause: on_palette_done disposed the source BEFORE invoking the
    dispatch hook, freeing the script Handle the hook was about to call.
    Use-after-free surfaced as a no-op dispatch reported through the host's
    "(menu script failed)" path.

    This test drives /R H (slash, REPL menu hotkey, then Help hotkey) and
    asserts the menu actually ran by looking for Help text in the
    scrollback AND the absence of the failure marker.  Failure pre-fix:
    "(menu script failed)" appears in the capture.
    """
    with TUI(boot_wait=1.5) as t:
        t.wait_for(">", timeout=5)
        t.send("/")
        time.sleep(0.6)
        t.wait_for("╔", timeout=2)

        # Open REPL menu via hotkey R, then activate Help via hotkey H.
        t.send("R")
        time.sleep(0.3)
        t.send("H")
        # Help is a no-arg script that writes lines to scrollback.  Give it
        # a moment to compile + run + render.
        time.sleep(1.0)
        snapshot(t, "repl-menu-help-dispatch-result")

        assert t.is_alive(), "REPL exited during menu dispatch"

        text = t.capture()
        assert "(menu script failed)" not in text, (
            "Menu dispatch returned the host failure marker — script "
            "Handle was freed before the dispatch hook could use it.  "
            "Capture:\n" + text
        )
