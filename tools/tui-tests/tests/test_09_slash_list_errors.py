"""
Issue #804 -- boxen REPL: `/list builtins` (and other invalid paths) must
produce a visible error in the scrollback, matching the linenoise
(--plain) REPL's behavior.

Before the fix, slash-dispatched commands that wrote to stdout via the
underlying replverbhost_* path produced no visible output in the boxen
TUI. The error message "Error: 'builtins' is not a valid table path"
was being lost between the slash dispatcher's stdout write and the
boxen scrollback drain.

These tests assert end-to-end visible parity for /list against three
inputs:
  - a non-existent single-component name (`builtins` -- bug #804)
  - a deeply non-existent dotted path
  - a real table path that should succeed and list contents
"""
import time

from tui_harness import TUI, snapshot


def _send_slash_list(t, arg, label):
    """Type `/list <arg>` + Enter, give the drain a moment, snapshot."""
    t.send(f"/list {arg}")
    t.send_key("Enter")
    # Give the event loop a few ticks to drain stdout into scrollback.
    time.sleep(0.6)
    text = t.capture()
    snapshot(t, label)
    return text


def test_slash_list_undefined_single_component_shows_error():
    """/list builtins -- issue #804 repro. Must produce a visible error."""
    with TUI(boot_wait=1.5) as t:
        t.wait_for(">", timeout=5)
        text = _send_slash_list(t, "builtins", "list-builtins-undefined")
        # Match the linenoise error text. Boxen and linenoise share the
        # underlying replverbhost_list code path; both must surface the
        # same message.
        assert "not a valid table path" in text or "not found" in text, (
            "issue #804: /list builtins produced no visible error in boxen "
            f"scrollback. Expected 'not a valid table path' or 'not found'. "
            f"Got:\n{text}"
        )


def test_slash_list_deep_undefined_path_shows_error():
    """/list nonexistent.bogus.path -- dotted-path error must be visible."""
    with TUI(boot_wait=1.5) as t:
        t.wait_for(">", timeout=5)
        text = _send_slash_list(
            t, "nonexistent.bogus.path", "list-deep-undefined"
        )
        assert "not a valid table path" in text or "not found" in text, (
            "/list of a non-existent dotted path produced no visible error. "
            f"Got:\n{text}"
        )


def test_slash_list_valid_table_shows_contents():
    """/list system.verbs -- success path must also produce visible output."""
    with TUI(boot_wait=1.5) as t:
        t.wait_for(">", timeout=5)
        text = _send_slash_list(t, "system.verbs", "list-system-verbs")
        # system.verbs has well-known children -- at least one of these
        # MUST be visible if the list output reached scrollback.
        expected_children = ("builtins", "apps", "constants", "colors")
        assert any(child in text for child in expected_children), (
            "/list system.verbs produced no visible output -- table-listing "
            f"path is also broken. Got:\n{text}"
        )
