"""
Phase C M7 -- /mouse toggle acceptance test (INPUT_DECODER_PLAN.md, M7).

Verifies against the real binary:
  - mouse mode is off by default and the footer says so
  - /mouse on flips the footer state and confirms in scrollback
  - /mouse off restores the default
  - the explicit preference persists across sessions

HOME is redirected to a temp dir so the persisted preference file
(~/.frontier_mouse) and history writes never touch the developer's real
dotfiles.
"""
import tempfile

from tui_harness import TUI, snapshot


def test_mouse_toggle_footer_round_trip():
    with tempfile.TemporaryDirectory() as home:
        with TUI(boot_wait=1.5, env_overrides={"HOME": home}) as t:
            t.wait_for(">", timeout=5)
            # Default state: footer advertises mouse off + the toggle.
            t.wait_for("Mouse: off", timeout=3)
            snapshot(t, "mouse-default-off")

            t.send("/mouse on")
            t.send_key("Enter")
            t.wait_for("Mouse mode on", timeout=3)   # scrollback confirmation
            t.wait_for("Mouse: on", timeout=3)       # footer updated
            snapshot(t, "mouse-on")

            t.send("/mouse off")
            t.send_key("Enter")
            t.wait_for("Mouse mode off", timeout=3)
            t.wait_for("Mouse: off", timeout=3)
            snapshot(t, "mouse-off-restored")


def test_mouse_pref_persists_across_sessions():
    with tempfile.TemporaryDirectory() as home:
        # Session 1: opt in explicitly.
        with TUI(boot_wait=1.5, env_overrides={"HOME": home}) as t:
            t.wait_for(">", timeout=5)
            t.send("/mouse on")
            t.send_key("Enter")
            t.wait_for("Mouse mode on", timeout=3)

        # Session 2 (same HOME): must come up with mouse already on.
        with TUI(boot_wait=1.5, env_overrides={"HOME": home}) as t:
            t.wait_for(">", timeout=5)
            t.wait_for("Mouse: on", timeout=3)
            snapshot(t, "mouse-pref-persisted")
