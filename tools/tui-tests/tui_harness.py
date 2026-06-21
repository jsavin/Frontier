"""
TUI Test Harness — drives `dist/frontier-cli --debug-tui` through tmux.

Each test creates a TUI session, sends keystrokes via `tmux send-keys`,
captures the pane content via `tmux capture-pane`, and asserts on the
visible output.

Captures come in two flavors:
- Text (capture-pane -p): for content assertions
- ANSI (capture-pane -p -e): preserved for HTML rendering via `aha`

The HTML renders let agents (or humans) visually inspect what the TUI
looked like at any moment without needing a real terminal.

Usage in a test file:

    from tui_harness import TUI, snapshot

    def test_history_up_arrow():
        with TUI() as t:
            t.send("1 + 1\\n")
            t.wait_for("3")            # wait for output
            t.send_key("Up")           # presses UP arrow
            t.wait_for("1 + 1")        # history loaded
            snapshot(t, "history-up")  # save text + HTML
"""
from __future__ import annotations

import os
import shlex
import shutil
import subprocess
import sys
import time
import uuid
from contextlib import contextmanager
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
# Allow overrides via environment so a developer (or CI) can point at a
# specific build without editing the harness.
DEFAULT_BIN = Path(
    os.environ.get("FRONTIER_TEST_BIN", REPO_ROOT / "dist" / "frontier-cli")
)
DEFAULT_SYSTEM_ROOT = Path(
    os.environ.get("FRONTIER_TEST_ROOT", REPO_ROOT / "dist" / "Frontier.root")
)
ARTIFACTS_DIR = Path(__file__).resolve().parent / "artifacts"

ARTIFACTS_DIR.mkdir(exist_ok=True)


class TUIError(RuntimeError):
    """Raised when the harness can't do what was asked (timeout, tmux failure, etc.)."""


class TUI:
    """
    Driver for one instance of `dist/frontier-cli --debug-tui` in a detached tmux session.

    Parameters
    ----------
    name : optional session name override (defaults to a uuid).  Use a
        stable name only when you need to attach manually for debugging
        (`tmux attach -t <name>`).
    cols, rows : pane dimensions.  80x24 is the safe default for boxen REPL.
    boot_wait : seconds to wait after launch for the REPL to draw its
        first prompt.  Increase for slow CI hosts.
    bin : path to the binary.  Defaults to dist/frontier-cli built from
        `make -C frontier-cli`.
    system_root : path to the system root .root file.
    extra_args : list of additional CLI args to append after --debug-tui.
    env_overrides : dict of env vars merged on top of the inherited env;
        useful for HOME redirection in history tests.
    """

    def __init__(
        self,
        name: str | None = None,
        cols: int = 80,
        rows: int = 24,
        boot_wait: float = 1.0,
        bin: Path = DEFAULT_BIN,
        system_root: Path = DEFAULT_SYSTEM_ROOT,
        extra_args: list[str] | None = None,
        env_overrides: dict[str, str] | None = None,
    ):
        if shutil.which("tmux") is None:
            raise TUIError("tmux not found in PATH; install via `brew install tmux`")
        if not bin.exists():
            raise TUIError(
                f"binary not found at {bin}; run `make -C frontier-cli` first"
            )
        self.name = name or f"tui-test-{uuid.uuid4().hex[:8]}"
        self.cols = cols
        self.rows = rows
        self.boot_wait = boot_wait
        self.bin = bin
        self.system_root = system_root
        self.extra_args = extra_args or []
        self.env_overrides = env_overrides or {}
        self._opened = False

    # --- lifecycle --------------------------------------------------------

    def __enter__(self):
        self.open()
        return self

    def __exit__(self, exc_type, exc, tb):
        try:
            self.close()
        except Exception:
            # Don't swallow the original exception
            if exc_type is None:
                raise

    def open(self):
        if self._opened:
            raise TUIError("session already open")
        cmd_parts = [
            str(self.bin),
            "--debug-tui",
            "--system-root",
            str(self.system_root),
            *self.extra_args,
        ]
        # tmux's `new-session` will run the command directly (not via a shell).
        # We use shell wrapping so env_overrides apply.
        if self.env_overrides:
            env_prefix = " ".join(
                f"{k}={shlex.quote(v)}" for k, v in self.env_overrides.items()
            )
            shell_cmd = f"{env_prefix} {' '.join(shlex.quote(p) for p in cmd_parts)}"
        else:
            shell_cmd = " ".join(shlex.quote(p) for p in cmd_parts)
        # Wrap in `sh -c ... ; sleep 999` so the pane stays open after the
        # binary exits (otherwise tmux kills the session on exit and we
        # lose the final screen state for the assertion).
        sh_arg = f"{shell_cmd}; echo '<<<repl-exited>>>'; sleep 999"
        rc = subprocess.run(
            [
                "tmux", "new-session", "-d",
                "-s", self.name,
                "-x", str(self.cols),
                "-y", str(self.rows),
                "sh", "-c", sh_arg,
            ],
            capture_output=True,
            text=True,
        )
        if rc.returncode != 0:
            raise TUIError(f"tmux new-session failed: {rc.stderr.strip()}")
        self._opened = True
        time.sleep(self.boot_wait)

    def close(self):
        if not self._opened:
            return
        subprocess.run(
            ["tmux", "kill-session", "-t", self.name],
            capture_output=True,
        )
        self._opened = False

    # --- input ------------------------------------------------------------

    def send(self, text: str):
        """Send literal text (each character as a keystroke).  No Enter is
        appended unless `text` contains '\\n' (which becomes the Enter key).

        Special characters in `text` are passed as literals; for named keys
        (Up, Down, Tab, Escape, C-c, etc.) use `send_key`.
        """
        # We use `-l` (literal) so that '/' is sent as '/', not interpreted
        # as a tmux command prefix.  Enter is sent separately if needed.
        if not text:
            return
        if "\n" in text:
            # Split around newlines and send Enter for each
            parts = text.split("\n")
            for i, part in enumerate(parts):
                if part:
                    subprocess.run(
                        ["tmux", "send-keys", "-t", self.name, "-l", part],
                        check=True,
                    )
                if i < len(parts) - 1:
                    subprocess.run(
                        ["tmux", "send-keys", "-t", self.name, "Enter"],
                        check=True,
                    )
        else:
            subprocess.run(
                ["tmux", "send-keys", "-t", self.name, "-l", text],
                check=True,
            )

    def send_key(self, key: str):
        """Send a named key by tmux's name: Up, Down, Left, Right, Tab,
        Enter, Escape (or Esc), BSpace, Space, Home, End, PgUp, PgDn,
        DC (forward Delete), or any control combo like 'C-c', 'C-a', 'M-x'.
        """
        subprocess.run(
            ["tmux", "send-keys", "-t", self.name, key],
            check=True,
        )

    # --- capture ----------------------------------------------------------

    def capture(self) -> str:
        """Return the pane's visible content as plain text (no ANSI)."""
        rc = subprocess.run(
            ["tmux", "capture-pane", "-t", self.name, "-p"],
            capture_output=True,
            text=True,
            check=True,
        )
        return rc.stdout

    def capture_ansi(self) -> str:
        """Return the pane's visible content WITH ANSI escape sequences
        preserved, suitable for piping to `aha` for HTML rendering.
        """
        rc = subprocess.run(
            ["tmux", "capture-pane", "-t", self.name, "-p", "-e"],
            capture_output=True,
            text=True,
            check=True,
        )
        return rc.stdout

    def cursor_position(self) -> tuple[int, int]:
        """Return (col, row) of the tmux pane's cursor.  0-indexed."""
        rc = subprocess.run(
            [
                "tmux", "display-message", "-t", self.name, "-p",
                "#{cursor_x},#{cursor_y}",
            ],
            capture_output=True,
            text=True,
            check=True,
        )
        col, row = rc.stdout.strip().split(",")
        return int(col), int(row)

    # --- assertions / synchronization -------------------------------------

    def wait_for(self, needle: str, timeout: float = 5.0, poll: float = 0.1) -> str:
        """Poll until `needle` appears in the pane content.  Returns the
        full capture when found.  Raises TUIError on timeout — with the
        last capture attached for diagnosis.
        """
        deadline = time.monotonic() + timeout
        last = ""
        while time.monotonic() < deadline:
            last = self.capture()
            if needle in last:
                return last
            time.sleep(poll)
        raise TUIError(
            f"timeout waiting for {needle!r} after {timeout}s. "
            f"Last capture:\n{last}"
        )

    def wait_for_no(self, needle: str, timeout: float = 2.0, poll: float = 0.1) -> str:
        """Poll until `needle` is NO LONGER in the pane content.  Useful
        after Escape/close operations.  Returns the final capture.
        """
        deadline = time.monotonic() + timeout
        last = ""
        while time.monotonic() < deadline:
            last = self.capture()
            if needle not in last:
                return last
            time.sleep(poll)
        raise TUIError(
            f"timeout waiting for {needle!r} to disappear after {timeout}s. "
            f"Last capture:\n{last}"
        )

    def pause(self, seconds: float):
        """Sleep for `seconds`.  Use sparingly — prefer wait_for over a
        fixed sleep.  Useful when you specifically need to confirm
        something does NOT happen within a window (e.g. debounce checks).
        """
        time.sleep(seconds)

    def is_alive(self) -> bool:
        """True if the REPL process is still running (hasn't printed
        the sentinel '<<<repl-exited>>>' marker yet).
        """
        return "<<<repl-exited>>>" not in self.capture()


# -- snapshot helpers (artifact saving) ---------------------------------------


def snapshot(tui: TUI, label: str) -> Path:
    """Save a text + ANSI + HTML snapshot trio for `label`.  Returns the
    artifacts directory path.  Files: <label>.txt, <label>.ansi, <label>.html
    (HTML only if `aha` is available).
    """
    safe = "".join(c if c.isalnum() or c in "-_." else "_" for c in label)
    out_dir = ARTIFACTS_DIR / safe
    out_dir.mkdir(parents=True, exist_ok=True)
    text = tui.capture()
    ansi = tui.capture_ansi()
    (out_dir / "capture.txt").write_text(text)
    (out_dir / "capture.ansi").write_text(ansi)
    if shutil.which("aha") is not None:
        proc = subprocess.run(
            ["aha", "--black", "--title", f"TUI snapshot: {label}"],
            input=ansi, capture_output=True, text=True,
        )
        if proc.returncode == 0:
            (out_dir / "capture.html").write_text(proc.stdout)
    return out_dir


# -- runner -------------------------------------------------------------------


def discover_tests(test_dir: Path) -> list[tuple[str, str]]:
    """Return list of (file, function_name) pairs for every `test_*` in
    every `*.py` in `test_dir`.
    """
    tests: list[tuple[str, str]] = []
    for f in sorted(test_dir.glob("test_*.py")):
        import importlib.util
        spec = importlib.util.spec_from_file_location(f.stem, f)
        mod = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(mod)
        for name in dir(mod):
            if name.startswith("test_") and callable(getattr(mod, name)):
                tests.append((str(f), name))
    return tests


def run_test(file: str, name: str) -> tuple[bool, str]:
    """Run a single test by reloading the file and calling the function.
    Returns (passed, message).
    """
    import importlib.util
    import traceback
    spec = importlib.util.spec_from_file_location(Path(file).stem, file)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    fn = getattr(mod, name)
    try:
        fn()
        return True, ""
    except Exception as e:
        return False, "".join(traceback.format_exception(type(e), e, e.__traceback__))


def main():
    """CLI entry: discover and run all tests under tests/.  Exit 0 on all
    pass, 1 on any fail.  Failure messages and artifact paths are printed
    to stderr.
    """
    # Developer-friendly notice: HTML snapshots need `aha` for visual
    # inspection.  Tests still run + pass without it (text + ANSI capture
    # still work), but reviewers (human and agent) lose the rendered
    # screenshot artifact that makes failures easier to diagnose.
    if shutil.which("aha") is None:
        print(
            "[tui-tests] notice: `aha` not installed -- HTML snapshots disabled. "
            "Text + ANSI captures will still be saved.\n"
            "[tui-tests]         Install for visual inspection: brew install aha",
            file=sys.stderr,
        )
    test_dir = Path(__file__).resolve().parent / "tests"
    tests = discover_tests(test_dir)
    if not tests:
        print("no tests discovered", file=sys.stderr)
        return 2
    passed = 0
    failed: list[tuple[str, str]] = []
    for file, name in tests:
        ok, msg = run_test(file, name)
        label = f"{Path(file).stem}::{name}"
        if ok:
            print(f"  PASS  {label}")
            passed += 1
        else:
            print(f"  FAIL  {label}")
            failed.append((label, msg))
    total = len(tests)
    print(f"\n[tui-tests] {passed}/{total} passed")
    if failed:
        print("\n--- FAILURES ---", file=sys.stderr)
        for label, msg in failed:
            print(f"\n{label}\n{msg}", file=sys.stderr)
        print(f"\nCapture artifacts: {ARTIFACTS_DIR}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
