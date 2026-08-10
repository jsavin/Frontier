"""
Unit 2.4 — default-REPL debug surface.

End-to-end round trip against the real binary: arm a protocol breakpoint
from the default boxen REPL with /bp, dispatch the script via
thread.callScript, watch the debug/suspended notification render in the
scrollback (instead of being silently discarded, the pre-2.4 behavior),
inspect with /locals, resume with /continue, and prove the script ran to
completion by reading the marker value back.

Uses a STAGED copy of dist/Frontier.root — the REPL opens the system root
read-write and saves on exit, so the canonical dist root must not be the
test target.
"""
import shutil
import tempfile
from pathlib import Path

from tui_harness import TUI, DEFAULT_SYSTEM_ROOT, snapshot


def _staged_root() -> Path:
    stage = Path(tempfile.mkdtemp(prefix="tui_debug_surface_"))
    dst = stage / "Frontier.root"
    shutil.copy(DEFAULT_SYSTEM_ROOT, dst)
    return dst


def test_bp_callscript_continue_round_trip():
    root = _staged_root()
    with TUI(system_root=root, boot_wait=1.5) as t:
        t.wait_for(">", timeout=5)

        # Install a three-line marker script in system.temp.
        t.send(
            'script.newScriptObject("system.temp.u24v = 1\\r'
            'system.temp.u24v = 2\\rsystem.temp.u24v = 3", '
            "@system.temp.u24Target)"
        )
        t.send_key("Enter")
        t.wait_for("true", timeout=5)

        # Arm a protocol breakpoint on line 2 from the default REPL.
        t.send("/bp system.temp.u24Target 2")
        t.send_key("Enter")
        t.wait_for("breakpoint set: system.temp.u24Target:2", timeout=5)

        # Dispatch via thread.callScript; the lazy-attach path suspends the
        # thread at the breakpoint and the notification must RENDER.
        t.send("thread.callScript(@system.temp.u24Target, {})")
        t.send_key("Enter")
        t.wait_for("suspended at system.temp.u24Target:2 (breakpoint)", timeout=10)

        # The park is not invisible: footer hint while suspended.
        t.wait_for("1 thread suspended", timeout=3)
        snapshot(t, "debug-surface-suspended")

        # /threads lists the parked thread.
        t.send("/threads")
        t.send_key("Enter")
        t.wait_for("[debug] threads:", timeout=5)

        # /locals defaults to the sole suspended thread.
        t.send("/locals")
        t.send_key("Enter")
        t.wait_for("locals at system.temp.u24Target:2", timeout=5)

        # Resume; the thread runs to completion.
        t.send("/continue")
        t.send_key("Enter")
        t.wait_for("completed", timeout=10)
        t.wait_for_no("1 thread suspended", timeout=5)

        # Marker readback proves the script executed past the breakpoint
        # (read the VALUE, not defined() -- see the u13 harness note).
        t.send("system.temp.u24v")
        t.send_key("Enter")
        t.wait_for("3", timeout=5)
        snapshot(t, "debug-surface-completed")


def test_bp_toggle_reports_cleared():
    root = _staged_root()
    with TUI(system_root=root, boot_wait=1.5) as t:
        t.wait_for(">", timeout=5)

        t.send("/bp system.temp.u24Toggle 1")
        t.send_key("Enter")
        t.wait_for("breakpoint set: system.temp.u24Toggle:1", timeout=5)

        # Same /bp again toggles the breakpoint off (server semantics);
        # the surface must report it honestly.
        t.send("/bp system.temp.u24Toggle 1")
        t.send_key("Enter")
        t.wait_for("breakpoint cleared: system.temp.u24Toggle:1", timeout=5)

        # /continue with nothing suspended reports instead of dispatching.
        t.send("/continue")
        t.send_key("Enter")
        t.wait_for("no suspended threads", timeout=5)
