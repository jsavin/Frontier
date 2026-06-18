"""
Smoke test — verifies the harness can launch the REPL and capture output.
Runs first (test_00) so a broken harness fails fast.
"""
from tui_harness import TUI, snapshot


def test_repl_launches_and_shows_prompt():
    with TUI(boot_wait=1.5) as t:
        # The boxen REPL draws a `> ` prompt at the bottom row.
        text = t.wait_for(">", timeout=5)
        snapshot(t, "smoke-initial")
        # Sanity: the prompt is in the last few rows (input bar)
        rows = text.rstrip("\n").split("\n")
        assert any(">" in row for row in rows[-3:]), (
            f"prompt not in last 3 rows; got:\n{text}"
        )


def test_repl_responds_to_single_line_expression():
    with TUI(boot_wait=1.5) as t:
        t.wait_for(">", timeout=5)
        t.send("1 + 1")
        t.send_key("Enter")
        # Expect the result `2` to appear in scrollback
        t.wait_for("2", timeout=3)
        snapshot(t, "smoke-eval-1plus1")
