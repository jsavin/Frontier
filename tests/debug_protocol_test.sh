#!/bin/bash
#
# Integration tests for debug/* protocol operations
# Tests the debugger MVP: debug/run, debug/continue, debug/kill, debug/pause
#
# Uses Python with an event-driven DebugSession class that reads responses
# via a background thread instead of time.sleep() delays.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
CLI="$PROJECT_ROOT/frontier-cli/frontier-cli"
SOURCE_DB="$PROJECT_ROOT/databases/Virgin.root"

if [ ! -x "$CLI" ]; then
    echo "Error: frontier-cli not found at $CLI" >&2
    exit 1
fi
if [ ! -f "$SOURCE_DB" ]; then
    echo "Error: database not found at $SOURCE_DB" >&2
    exit 1
fi

# Stage Virgin.root into a tmpdir so a regression in the protocol read-only
# default (issue #588) cannot corrupt the canonical .root file. Mirrors
# tests/integration/protocol_readonly_tests.sh. Issue #644.
STAGE_DIR="$(mktemp -d -t frontier-debug-proto-XXXXXX)"
DB="$STAGE_DIR/Virgin.root"
cp "$SOURCE_DB" "$DB"
trap 'rm -rf "$STAGE_DIR"' EXIT

export DEBUG_TEST_CLI="$CLI"
export DEBUG_TEST_DB="$DB"

python3 << 'PYEOF'
import subprocess, json, sys, os, threading, time

CLI = os.environ["DEBUG_TEST_CLI"]
DB = os.environ["DEBUG_TEST_DB"]

PASSED = 0
FAILED = 0
GREEN = "\033[0;32m"
RED = "\033[0;31m"
NC = "\033[0m"

# ---------------------------------------------------------------------------
# DebugSession — event-driven protocol session
# ---------------------------------------------------------------------------

class DebugSession:
    """Manages a frontier-cli --protocol session with a background reader thread."""

    def __init__(self, timeout=15):
        self.timeout = timeout
        self.messages = []
        self._lock = threading.Lock()
        self._new_msg = threading.Event()
        self._proc = subprocess.Popen(
            [CLI, "--protocol", "--skip-startup", "--system-root", DB],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            text=True, bufsize=1
        )
        self._reader = threading.Thread(target=self._read_loop, daemon=True)
        self._reader.start()

    def _read_loop(self):
        """Background thread: read stdout line-by-line, parse JSON, append to messages."""
        try:
            for line in self._proc.stdout:
                line = line.strip()
                if not line:
                    continue
                try:
                    msg = json.loads(line)
                    with self._lock:
                        self.messages.append(msg)
                    self._new_msg.set()
                except json.JSONDecodeError:
                    pass
        except (ValueError, OSError):
            # stdout closed
            pass

    def send(self, msg):
        """Send a JSON message to stdin and flush."""
        if isinstance(msg, dict):
            msg = json.dumps(msg)
        try:
            self._proc.stdin.write(msg + "\n")
            self._proc.stdin.flush()
        except (BrokenPipeError, OSError):
            pass

    def wait_for(self, predicate, timeout=None):
        """Block until a message matching predicate appears or timeout expires.
        Returns the matching message, or None on timeout."""
        if timeout is None:
            timeout = self.timeout
        deadline = time.monotonic() + timeout
        # Track how many messages we've already checked
        checked = 0
        while True:
            with self._lock:
                # Only check new messages since last check
                for i in range(checked, len(self.messages)):
                    if predicate(self.messages[i]):
                        return self.messages[i]
                checked = len(self.messages)
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                return None
            # Wait for new message signal, then re-check
            self._new_msg.clear()
            self._new_msg.wait(timeout=min(0.05, remaining))

    def send_and_wait(self, msg, match_id=None, timeout=None):
        """Send a message and wait for its response (matched by id)."""
        if match_id is None and isinstance(msg, dict):
            match_id = msg.get("id")
        self.send(msg)
        if match_id is not None:
            return self.wait_for(lambda m: m.get("id") == match_id, timeout=timeout)
        return None

    def wait_for_notification(self, op=None, reason=None, timeout=None):
        """Wait for an unsolicited notification matching op and/or reason."""
        # Small delay to let the process produce the notification
        time.sleep(0.1)
        def pred(m):
            if op is not None and m.get("op") != op:
                return False
            if reason is not None and m.get("params", {}).get("reason") != reason:
                return False
            return True
        return self.wait_for(pred, timeout=timeout)

    def close(self):
        """Send shutdown and clean up the process."""
        try:
            self.send({"op": "shutdown", "id": 999})
        except (BrokenPipeError, OSError):
            pass
        try:
            self._proc.stdin.close()
        except (BrokenPipeError, OSError):
            pass
        try:
            self._proc.wait(timeout=10)
        except subprocess.TimeoutExpired:
            self._proc.kill()
            self._proc.wait()

    def __enter__(self):
        return self

    def __exit__(self, *args):
        self.close()


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def assert_test(name, passed, detail=""):
    global PASSED, FAILED
    if passed:
        print(f"  {GREEN}✓ PASS{NC}: {name}")
        PASSED += 1
    else:
        print(f"  {RED}✗ FAIL{NC}: {name}")
        if detail:
            print(f"    {detail}")
        FAILED += 1

def find_msg(messages, **kwargs):
    """Find a message matching all kwargs."""
    for m in messages:
        match = True
        for k, v in kwargs.items():
            if k == "op":
                if m.get("op") != v:
                    match = False
            elif k == "reason":
                if m.get("params", {}).get("reason") != v:
                    match = False
            elif k == "status":
                if m.get("result", {}).get("status") != v:
                    match = False
            elif k == "success":
                if m.get("success") != v and m.get("result", {}).get("success") != v:
                    match = False
        if match:
            return m
    return None


print("=" * 46)
print("Debug Protocol Tests")
print("=" * 46)
print()

# Debug threads get sequential IDs starting from 3 (main=2).
# Each test session starts fresh, so the first debug thread is always 3.
# We validate this assumption explicitly in the first test.
FIRST_DEBUG_TID = 3

# --- Test 0: verify thread ID assumption ---
print("--- thread ID validation ---")

with DebugSession() as s:
    resp = s.send_and_wait({"op": "debug/run", "id": 1, "params": {"expression": "return 1"}})
    actual_tid = resp.get("result", {}).get("threadId") if resp else None
    s.send_and_wait({"op": "debug/kill", "id": 2, "params": {"threadId": FIRST_DEBUG_TID}})

assert_test(
    f"first debug thread gets ID {FIRST_DEBUG_TID}",
    actual_tid == FIRST_DEBUG_TID,
    f"Expected threadId={FIRST_DEBUG_TID}, got {actual_tid}. Update FIRST_DEBUG_TID."
)
if actual_tid is not None and actual_tid != FIRST_DEBUG_TID:
    print(f"  FATAL: Thread ID assumption broken. Updating to {actual_tid}.")
    FIRST_DEBUG_TID = actual_tid

# Clean slate for breakpoints/watchpoints between test runs
with DebugSession() as s:
    s.send_and_wait({"op": "debug/clearBreakpoints", "id": 1})
    s.send_and_wait({"op": "debug/clearWatchpoints", "id": 2})

# --- Test 1: debug/run + debug/continue ---
print()
print("--- debug/run + debug/continue ---")

with DebugSession() as s:
    resp = s.send_and_wait({"op": "debug/run", "id": 1, "params": {"expression": "return 1+1"}})
    assert_test("run returns threadId",
                resp is not None and resp.get("result", {}).get("threadId") is not None,
                f"Response: {resp}")

    entry = s.wait_for_notification(reason="entry")
    assert_test("suspended at entry", entry is not None, f"Messages: {s.messages}")

    s.send_and_wait({"op": "debug/continue", "id": 2, "params": {"threadId": FIRST_DEBUG_TID}})

    completed = s.wait_for_notification(op="debug/completed")
    assert_test("completed successfully", completed is not None, f"Messages: {s.messages}")

# --- Test 2: debug/run + debug/kill ---
print()
print("--- debug/run + debug/kill ---")

with DebugSession() as s:
    s.send_and_wait({"op": "debug/run", "id": 1, "params": {"expression": "return 1+1"}})
    s.wait_for_notification(reason="entry")
    resp = s.send_and_wait({"op": "debug/kill", "id": 2, "params": {"threadId": FIRST_DEBUG_TID}})
    assert_test("kill returns killed status",
                resp is not None and resp.get("result", {}).get("status") == "killed",
                f"Response: {resp}")

    completed = s.wait_for_notification(op="debug/completed")
    assert_test("completed with failure", completed is not None, f"Messages: {s.messages}")

# --- Test 3: debug/pause ---
print()
print("--- debug/pause on running thread ---")

with DebugSession() as s:
    s.send_and_wait({"op": "debug/run", "id": 1, "params": {"expression": "local (i); for i = 1 to 1000000 {i = i}; return true"}})
    s.wait_for_notification(reason="entry")
    s.send_and_wait({"op": "debug/continue", "id": 2, "params": {"threadId": FIRST_DEBUG_TID}})
    resp = s.send_and_wait({"op": "debug/pause", "id": 3, "params": {"threadId": FIRST_DEBUG_TID}})
    assert_test("pause sends interrupting",
                resp is not None and resp.get("result", {}).get("status") == "interrupting",
                f"Response: {resp}")

    interrupted = s.wait_for_notification(reason="interrupted")
    assert_test("suspended with interrupted reason", interrupted is not None, f"Messages: {s.messages}")

    s.send_and_wait({"op": "debug/kill", "id": 4, "params": {"threadId": FIRST_DEBUG_TID}})

# --- Test 4: debug/step ---
print()
print("--- debug/step ---")

with DebugSession() as s:
    s.send_and_wait({"op": "debug/run", "id": 1, "params": {"expression": "return 42"}})
    s.wait_for_notification(reason="entry")
    s.send_and_wait({"op": "debug/step", "id": 2, "params": {"threadId": FIRST_DEBUG_TID, "direction": "into"}})

    step = s.wait_for_notification(reason="step")
    assert_test("step into suspends at next statement", step is not None, f"Messages: {s.messages}")

# Step-over test — previously crashed due to #505 (hthreadglobals overwrite
# during GIL yield). Fixed by capturing hthreadglobals in a local variable.
with DebugSession(timeout=20) as s:
    s.send_and_wait({"op": "script/eval", "id": 1, "params": {
        "expression": 'new(scriptType, @system.temp.stepOverTest); script.newScriptObject("local (x = 1)\\rlocal (y = 2)\\rreturn (x + y)", @system.temp.stepOverTest)'
    }})
    s.send_and_wait({"op": "debug/setBreakpoint", "id": 2, "params": {"script": "system.temp.stepOverTest", "line": 1}})
    s.send_and_wait({"op": "debug/run", "id": 3, "params": {"expression": "system.temp.stepOverTest()"}})
    s.wait_for_notification(reason="entry")
    # Continue past entry
    s.send_and_wait({"op": "debug/continue", "id": 4, "params": {"threadId": FIRST_DEBUG_TID}})
    # Wait for breakpoint hit
    bp_hit = s.wait_for_notification(reason="breakpoint")
    # Suspended at breakpoint on line 1 — step over to next steppable line (line 3)
    s.send_and_wait({"op": "debug/step", "id": 5, "params": {"threadId": FIRST_DEBUG_TID, "direction": "over"}})
    step_suspend = s.wait_for_notification(reason="step")
    assert_test("step-over suspends at next line",
                step_suspend is not None,
                f"Messages: {[m for m in s.messages if m.get('op') == 'debug/suspended']}")
    if step_suspend:
        step_line = step_suspend.get("params", {}).get("line")
        # Line 2 is a local declaration (non-steppable), so step-over advances to line 3
        assert_test("step-over advances past locals to line 3",
                    step_line == 3,
                    f"Expected line 3, got {step_line}")
    # Kill to clean up
    s.send_and_wait({"op": "debug/kill", "id": 6, "params": {"threadId": FIRST_DEBUG_TID}})

# Step-over with calldepth: verify step-over skips into function calls
with DebugSession(timeout=25) as s:
    s.send_and_wait({"op": "script/eval", "id": 1, "params": {
        "expression": 'new(scriptType, @system.temp.depthHelper); script.newScriptObject("return 99", @system.temp.depthHelper)'
    }})
    s.send_and_wait({"op": "script/eval", "id": 2, "params": {
        "expression": 'new(scriptType, @system.temp.depthCaller); script.newScriptObject("local (a = system.temp.depthHelper())\\rreturn a", @system.temp.depthCaller)'
    }})
    # Set breakpoint on line 1 of caller (the function call line)
    s.send_and_wait({"op": "debug/setBreakpoint", "id": 3, "params": {"script": "system.temp.depthCaller", "line": 1}})
    s.send_and_wait({"op": "debug/run", "id": 4, "params": {"expression": "system.temp.depthCaller()"}})
    s.wait_for_notification(reason="entry")
    # Continue past entry
    s.send_and_wait({"op": "debug/continue", "id": 5, "params": {"threadId": FIRST_DEBUG_TID}})
    s.wait_for_notification(reason="breakpoint")
    # Now at breakpoint on line 1 — step over should NOT enter depthHelper
    # and should stop at line 2 (return a) of depthCaller
    s.send_and_wait({"op": "debug/step", "id": 6, "params": {"threadId": FIRST_DEBUG_TID, "direction": "over"}})
    step_suspend = s.wait_for_notification(reason="step")
    assert_test("step-over with calldepth skips function call",
                step_suspend is not None,
                f"Messages: {[m for m in s.messages if m.get('op') == 'debug/suspended']}")
    if step_suspend:
        step_line = step_suspend.get("params", {}).get("line")
        assert_test("step-over returns to caller line 2",
                    step_line == 2,
                    f"Expected line 2, got {step_line}")
    s.send_and_wait({"op": "debug/kill", "id": 7, "params": {"threadId": FIRST_DEBUG_TID}})

# Step-out: step into a function, then step-out to return to caller
with DebugSession(timeout=25) as s:
    s.send_and_wait({"op": "script/eval", "id": 1, "params": {
        "expression": 'new(scriptType, @system.temp.outHelper); script.newScriptObject("local (r = 77)\\rreturn r", @system.temp.outHelper)'
    }})
    s.send_and_wait({"op": "script/eval", "id": 2, "params": {
        "expression": 'new(scriptType, @system.temp.outCaller); script.newScriptObject("local (a = system.temp.outHelper())\\rreturn a", @system.temp.outCaller)'
    }})
    # Set breakpoint inside outHelper (line 1: local r = 77)
    s.send_and_wait({"op": "debug/setBreakpoint", "id": 3, "params": {"script": "system.temp.outHelper", "line": 1}})
    s.send_and_wait({"op": "debug/run", "id": 4, "params": {"expression": "system.temp.outCaller()"}})
    s.wait_for_notification(reason="entry")
    # Continue past entry
    s.send_and_wait({"op": "debug/continue", "id": 5, "params": {"threadId": FIRST_DEBUG_TID}})
    s.wait_for_notification(reason="breakpoint")
    # Now suspended inside outHelper at line 1 — step out to return to outCaller
    s.send_and_wait({"op": "debug/step", "id": 6, "params": {"threadId": FIRST_DEBUG_TID, "direction": "out"}})
    step_suspend = s.wait_for_notification(reason="step")
    assert_test("step-out returns to caller",
                step_suspend is not None,
                f"Messages: {[m for m in s.messages if m.get('op') == 'debug/suspended']}")
    s.send_and_wait({"op": "debug/kill", "id": 7, "params": {"threadId": FIRST_DEBUG_TID}})

# Step error: not suspended
with DebugSession() as s:
    s.send_and_wait({"op": "debug/run", "id": 1, "params": {"expression": "return 1"}})
    s.wait_for_notification(reason="entry")
    # Continue first (thread is now running)
    s.send_and_wait({"op": "debug/continue", "id": 2, "params": {"threadId": FIRST_DEBUG_TID}})
    # Wait for completion so thread is gone
    s.wait_for_notification(op="debug/completed")
    # Try to step — should fail because thread completed
    resp = s.send_and_wait({"op": "debug/step", "id": 3, "params": {"threadId": FIRST_DEBUG_TID, "direction": "over"}})
    assert_test("step on non-suspended thread errors",
                resp is not None and ("not suspended" in str(resp) or "No debug thread" in str(resp)),
                f"Response: {resp}")

# --- Test 5: error cases ---
print()
print("--- error cases ---")

with DebugSession() as s:
    resp = s.send_and_wait({"op": "debug/continue", "id": 1, "params": {"threadId": 999}})
    assert_test("continue with invalid threadId errors",
                resp is not None and "No debug thread" in str(resp),
                f"Response: {resp}")

with DebugSession() as s:
    resp = s.send_and_wait({"op": "debug/kill", "id": 1, "params": {"threadId": 999}})
    assert_test("kill with invalid threadId errors",
                resp is not None and "No debug thread" in str(resp),
                f"Response: {resp}")

with DebugSession() as s:
    resp = s.send_and_wait({"op": "debug/run", "id": 1, "params": {}})
    assert_test("run with missing expression errors",
                resp is not None and "expression" in str(resp),
                f"Response: {resp}")

# --- Test 6: breakpoint hit during execution ---
print()
print("--- breakpoint hit during execution ---")

with DebugSession(timeout=20) as s:
    # Step 1: Create a test function in system.temp via script/eval
    s.send_and_wait({"op": "script/eval", "id": 1, "params": {
        "expression": 'new(scriptType, @system.temp.bpTestFunc); script.newScriptObject("local (x = 1)\\rlocal (y = 2)\\rreturn (x + y)", @system.temp.bpTestFunc)'
    }})
    # Step 2: Set a breakpoint on line 2 of bpTestFunc
    s.send_and_wait({"op": "debug/setBreakpoint", "id": 2, "params": {
        "script": "system.temp.bpTestFunc", "line": 2
    }})
    # Step 3: Run an expression that calls the function
    s.send_and_wait({"op": "debug/run", "id": 3, "params": {
        "expression": "system.temp.bpTestFunc()"
    }})
    s.wait_for_notification(reason="entry")
    # Step 4: Continue past initial entry suspension
    s.send_and_wait({"op": "debug/continue", "id": 4, "params": {"threadId": FIRST_DEBUG_TID}})
    # Step 5: Should be suspended at breakpoint on line 2
    bp_suspend = s.wait_for_notification(reason="breakpoint")
    assert_test("breakpoint hit suspends thread", bp_suspend is not None,
                f"Messages: {[m for m in s.messages if m.get('op') == 'debug/suspended' or m.get('result', {}).get('action')]}")

    # If breakpoint was hit, verify it's on the right line
    if bp_suspend:
        bp_line = bp_suspend.get("params", {}).get("line")
        assert_test("breakpoint hit on correct line",
                    bp_line == 2,
                    f"Expected line 2, got {bp_line}")
    # Kill to clean up
    s.send_and_wait({"op": "debug/kill", "id": 5, "params": {"threadId": FIRST_DEBUG_TID}})

# --- Test 7: debug/setBreakpoint + debug/listBreakpoints ---
print()
print("--- debug/setBreakpoint + debug/listBreakpoints ---")

with DebugSession() as s:
    set_resp = s.send_and_wait({"op": "debug/setBreakpoint", "id": 1, "params": {"script": "mainResponder.respond", "line": 5}})
    assert_test("setBreakpoint returns action=set",
                set_resp is not None and set_resp.get("result", {}).get("action") == "set",
                f"Response: {set_resp}")

    list_resp = s.send_and_wait({"op": "debug/listBreakpoints", "id": 2, "params": {}})
    assert_test("listBreakpoints returns breakpoint array",
                list_resp is not None and list_resp.get("result", {}).get("breakpoints") is not None and len(list_resp["result"]["breakpoints"]) > 0,
                f"Response: {list_resp}")
    if list_resp and list_resp.get("result", {}).get("breakpoints"):
        bp = list_resp["result"]["breakpoints"][0]
        assert_test("listed breakpoint has correct script",
                    bp.get("script") == "mainResponder.respond",
                    f"Breakpoint: {bp}")
        assert_test("listed breakpoint has correct line",
                    bp.get("line") == 5,
                    f"Breakpoint: {bp}")

# --- Test 8: breakpoint toggle (clear) ---
print()
print("--- breakpoint toggle (clear) ---")

with DebugSession() as s:
    # Set a breakpoint
    set_resp = s.send_and_wait({"op": "debug/setBreakpoint", "id": 1, "params": {"script": "test.script", "line": 3}})
    assert_test("first set returns action=set",
                set_resp is not None and set_resp.get("result", {}).get("action") == "set",
                f"Response: {set_resp}")

    # Set again to toggle off
    toggle_resp = s.send_and_wait({"op": "debug/setBreakpoint", "id": 2, "params": {"script": "test.script", "line": 3}})
    assert_test("second set returns action=cleared",
                toggle_resp is not None and toggle_resp.get("result", {}).get("action") == "cleared",
                f"Response: {toggle_resp}")

    # List should be empty
    list_resp = s.send_and_wait({"op": "debug/listBreakpoints", "id": 3, "params": {}})
    assert_test("list after toggle is empty",
                list_resp is not None and list_resp.get("result", {}).get("breakpoints") is not None and len(list_resp["result"]["breakpoints"]) == 0,
                f"Response: {list_resp}")

# --- Test 9: breakpoint with leading @ stripped ---
print()
print("--- breakpoint @ prefix handling ---")

with DebugSession() as s:
    s.send_and_wait({"op": "debug/setBreakpoint", "id": 1, "params": {"script": "@system.compiler.start", "line": 1}})
    list_resp = s.send_and_wait({"op": "debug/listBreakpoints", "id": 2, "params": {}})
    if list_resp and list_resp.get("result", {}).get("breakpoints") and len(list_resp["result"]["breakpoints"]) > 0:
        assert_test("@ prefix stripped from script path",
                    list_resp["result"]["breakpoints"][0].get("script") == "system.compiler.start",
                    f"Breakpoint: {list_resp['result']['breakpoints'][0]}")
    else:
        assert_test("@ prefix stripped from script path", False, f"Response: {list_resp}")

# --- Test 10: breakpoint error cases ---
print()
print("--- breakpoint error cases ---")

with DebugSession() as s:
    resp = s.send_and_wait({"op": "debug/setBreakpoint", "id": 1, "params": {"line": 5}})
    assert_test("setBreakpoint without script errors",
                resp is not None and not resp.get("success", True) and "script" in str(resp).lower(),
                f"Response: {resp}")

with DebugSession() as s:
    resp = s.send_and_wait({"op": "debug/setBreakpoint", "id": 1, "params": {"script": "foo.bar"}})
    assert_test("setBreakpoint without line errors",
                resp is not None and not resp.get("success", True) and "line" in str(resp).lower(),
                f"Response: {resp}")

# --- Test 11: debug/clearBreakpoints ---
print()
print("--- debug/clearBreakpoints ---")

with DebugSession() as s:
    # Set two breakpoints
    s.send_and_wait({"op": "debug/setBreakpoint", "id": 1, "params": {"script": "foo.bar", "line": 1}})
    s.send_and_wait({"op": "debug/setBreakpoint", "id": 2, "params": {"script": "foo.bar", "line": 2}})
    # Clear all
    clear_resp = s.send_and_wait({"op": "debug/clearBreakpoints", "id": 3, "params": {}})
    assert_test("clearBreakpoints returns count",
                clear_resp is not None and clear_resp.get("result", {}).get("cleared") == 2,
                f"Response: {clear_resp}")

    # List should be empty after clear
    list_resp = s.send_and_wait({"op": "debug/listBreakpoints", "id": 4, "params": {}})
    assert_test("list empty after clearBreakpoints",
                list_resp is not None and list_resp.get("result", {}).get("breakpoints") is not None and len(list_resp["result"]["breakpoints"]) == 0,
                f"Response: {list_resp}")

with DebugSession() as s:
    clear_resp = s.send_and_wait({"op": "debug/clearBreakpoints", "id": 1, "params": {}})
    assert_test("clearBreakpoints with no breakpoints returns 0",
                clear_resp is not None and clear_resp.get("result", {}).get("cleared") == 0,
                f"Response: {clear_resp}")

# --- Test 12: debug/getLocals + debug/getStack + debug/getSource ---
print()
print("--- debug/getLocals + debug/getStack + debug/getSource ---")

with DebugSession(timeout=25) as s:
    # Create a test function with locals
    s.send_and_wait({"op": "script/eval", "id": 1, "params": {
        "expression": 'new(scriptType, @system.temp.inspectTest); script.newScriptObject("local (x = 42)\\rlocal (msg = \\"hello\\")\\rreturn (x)", @system.temp.inspectTest)'
    }})
    # Set breakpoint on line 3
    s.send_and_wait({"op": "debug/setBreakpoint", "id": 2, "params": {"script": "system.temp.inspectTest", "line": 3}})
    # Run in debug mode
    s.send_and_wait({"op": "debug/run", "id": 3, "params": {"expression": "system.temp.inspectTest()"}})
    s.wait_for_notification(reason="entry")
    # Continue past entry
    s.send_and_wait({"op": "debug/continue", "id": 4, "params": {"threadId": FIRST_DEBUG_TID}})
    # Wait for breakpoint
    s.wait_for_notification(reason="breakpoint")
    # Now suspended at breakpoint — test inspection
    locals_resp = s.send_and_wait({"op": "debug/getLocals", "id": 5, "params": {"threadId": FIRST_DEBUG_TID}})
    stack_resp = s.send_and_wait({"op": "debug/getStack", "id": 6, "params": {"threadId": FIRST_DEBUG_TID}})
    source_resp = s.send_and_wait({"op": "debug/getSource", "id": 7, "params": {"script": "system.temp.inspectTest", "threadId": FIRST_DEBUG_TID}})
    # Kill to clean up
    s.send_and_wait({"op": "debug/kill", "id": 8, "params": {"threadId": FIRST_DEBUG_TID}})

# Check getLocals
assert_test("getLocals returns locals array",
            locals_resp is not None and locals_resp.get("result", {}).get("locals") is not None,
            f"Response: {locals_resp}")

if locals_resp and locals_resp.get("result", {}).get("locals") is not None:
    locals_list = locals_resp["result"]["locals"]
    local_names = [l["name"] for l in locals_list]
    assert_test("getLocals contains x",
                "x" in local_names,
                f"Names: {local_names}")
    assert_test("getLocals contains msg",
                "msg" in local_names,
                f"Names: {local_names}")
    # Check x value
    x_local = next((l for l in locals_list if l["name"] == "x"), None)
    assert_test("getLocals x=42",
                x_local is not None and x_local.get("value") == "42",
                f"x_local: {x_local}")

# Check getStack
assert_test("getStack returns frames",
            stack_resp is not None and stack_resp.get("result", {}).get("frames") is not None and len(stack_resp["result"]["frames"]) > 0,
            f"Response: {stack_resp}")

if stack_resp and stack_resp.get("result", {}).get("frames"):
    frames = stack_resp["result"]["frames"]
    assert_test("getStack has inspectTest frame",
                any("inspectTest" in f.get("script", "") for f in frames),
                f"Frames: {frames}")

# Check getSource
assert_test("getSource returns lines",
            source_resp is not None and source_resp.get("result", {}).get("lines") is not None and len(source_resp["result"]["lines"]) >= 3,
            f"Response: {source_resp}")

if source_resp and source_resp.get("result", {}).get("lines"):
    lines = source_resp["result"]["lines"]
    assert_test("getSource line 3 is current",
                any(l.get("current") for l in lines if l.get("num") == 3),
                f"Lines: {lines}")
    assert_test("getSource line 3 has breakpoint",
                any(l.get("breakpoint") for l in lines if l.get("num") == 3),
                f"Lines: {lines}")

# --- Test 12: inspection error cases ---
print()
print("--- inspection error cases ---")

with DebugSession() as s:
    resp = s.send_and_wait({"op": "debug/getLocals", "id": 1, "params": {"threadId": 999}})
    assert_test("getLocals with invalid threadId errors",
                resp is not None and "No debug thread" in str(resp),
                f"Response: {resp}")

with DebugSession() as s:
    resp = s.send_and_wait({"op": "debug/getSource", "id": 1, "params": {}})
    assert_test("getSource without script errors",
                resp is not None and not resp.get("success", True) and "script" in str(resp).lower(),
                f"Response: {resp}")

# --- Test 14: multi-thread debugging (Phase 5) ---
print()
print("--- multi-thread debugging ---")

with DebugSession(timeout=20) as s:
    # Launch two debug threads
    resp1 = s.send_and_wait({"op": "debug/run", "id": 1, "params": {"expression": "return 1+1"}})
    entry1 = s.wait_for_notification(reason="entry")  # sync: wait for thread 1 to suspend
    resp2 = s.send_and_wait({"op": "debug/run", "id": 2, "params": {"expression": "return 2+2"}})
    entry2 = s.wait_for_notification(reason="entry")  # sync: wait for thread 2 to suspend

    # Extract actual thread IDs from responses
    t1_tid = resp1.get("result", {}).get("threadId") if resp1 else None
    t2_tid = resp2.get("result", {}).get("threadId") if resp2 else None

    assert_test("two threads started with different IDs",
                t1_tid is not None and t2_tid is not None and t1_tid != t2_tid,
                f"t1={t1_tid}, t2={t2_tid}")

    # Both should have entry suspensions
    entry_suspensions = [m for m in s.messages if m.get("op") == "debug/suspended" and m.get("params", {}).get("reason") == "entry"]
    assert_test("both threads suspended at entry",
                len(entry_suspensions) >= 2,
                f"Entry suspensions: {entry_suspensions}")

    # List threads — should show both
    list1_resp = s.send_and_wait({"op": "debug/listThreads", "id": 3, "params": {}})
    assert_test("listThreads shows 2 threads",
                list1_resp is not None and list1_resp.get("result", {}).get("threads") is not None and len(list1_resp["result"]["threads"]) == 2,
                f"Response: {list1_resp}")

    # Continue first thread
    s.send_and_wait({"op": "debug/continue", "id": 4, "params": {"threadId": t1_tid}})
    s.wait_for_notification(op="debug/completed")

    # List again — should show only second thread
    list2_resp = s.send_and_wait({"op": "debug/listThreads", "id": 5, "params": {}})
    assert_test("listThreads shows 1 thread after first completes",
                list2_resp is not None and list2_resp.get("result", {}).get("threads") is not None and len(list2_resp["result"]["threads"]) == 1,
                f"Response: {list2_resp}")

    # Continue second thread
    s.send_and_wait({"op": "debug/continue", "id": 6, "params": {"threadId": t2_tid}})
    s.wait_for_notification(op="debug/completed")

    # Both should have completed
    completions = [m for m in s.messages if m.get("op") == "debug/completed"]
    assert_test("both threads completed",
                len(completions) >= 2,
                f"Completions: {completions}")

# --- Test 15: watchpoint set/list/clear ---
print()
print("--- watchpoint set/list/clear ---")

with DebugSession() as s:
    set_resp = s.send_and_wait({"op": "debug/setWatchpoint", "id": 1, "params": {"variable": "x"}})
    assert_test("setWatchpoint returns action=set",
                set_resp is not None and set_resp.get("result", {}).get("action") == "set",
                f"Response: {set_resp}")

    s.send_and_wait({"op": "debug/setWatchpoint", "id": 2, "params": {"variable": "y"}})

    list1_resp = s.send_and_wait({"op": "debug/listWatchpoints", "id": 3, "params": {}})
    assert_test("listWatchpoints shows 2 watchpoints",
                list1_resp is not None and list1_resp.get("result", {}).get("watchpoints") is not None and len(list1_resp["result"]["watchpoints"]) == 2,
                f"Response: {list1_resp}")

    # Toggle x off
    toggle_resp = s.send_and_wait({"op": "debug/setWatchpoint", "id": 4, "params": {"variable": "x"}})
    assert_test("setWatchpoint toggle clears",
                toggle_resp is not None and toggle_resp.get("result", {}).get("action") == "cleared",
                f"Response: {toggle_resp}")

    list2_resp = s.send_and_wait({"op": "debug/listWatchpoints", "id": 5, "params": {}})
    assert_test("listWatchpoints shows 1 after toggle",
                list2_resp is not None and list2_resp.get("result", {}).get("watchpoints") is not None and len(list2_resp["result"]["watchpoints"]) == 1,
                f"Response: {list2_resp}")

    # Clear all
    clear_resp = s.send_and_wait({"op": "debug/clearWatchpoints", "id": 6, "params": {}})
    assert_test("clearWatchpoints returns count",
                clear_resp is not None and clear_resp.get("result", {}).get("cleared") == 1,
                f"Response: {clear_resp}")

# --- Test 16: watchpoint fires on value change ---
print()
print("--- watchpoint fires on value change ---")

with DebugSession(timeout=20) as s:
    s.send_and_wait({"op": "debug/setWatchpoint", "id": 1, "params": {"variable": "x"}})
    s.send_and_wait({"op": "debug/run", "id": 2, "params": {"expression": "local (x = 1); x = x + 10; return x"}})
    s.wait_for_notification(reason="entry")
    # Continue past entry — watchpoint should fire when x changes
    s.send_and_wait({"op": "debug/continue", "id": 3, "params": {"threadId": FIRST_DEBUG_TID}})

    wp_suspend = s.wait_for_notification(reason="watchpoint")
    assert_test("watchpoint fires on value change",
                wp_suspend is not None,
                f"Messages: {[m for m in s.messages if m.get('op') == 'debug/suspended']}")

    if wp_suspend:
        params = wp_suspend.get("params", {})
        assert_test("watchpoint reports variable name",
                    params.get("variable") == "x",
                    f"Params: {params}")
        assert_test("watchpoint reports old value",
                    params.get("oldValue") == "1",
                    f"Params: {params}")
        assert_test("watchpoint reports new value",
                    params.get("newValue") == "11",
                    f"Params: {params}")

    # Kill
    s.send_and_wait({"op": "debug/kill", "id": 4, "params": {"threadId": FIRST_DEBUG_TID}})

# --- Test 17: conditional breakpoints (Phase 7) ---
print()
print("--- conditional breakpoints ---")

with DebugSession(timeout=20) as s:
    s.send_and_wait({"op": "script/eval", "id": 1, "params": {
        "expression": 'new(scriptType, @system.temp.condTest); script.newScriptObject("local (x = 1)\\rx = x + 10\\rreturn x", @system.temp.condTest)'
    }})
    # Conditional breakpoint on line 1: x > 5 (won't fire, x=1)
    set_with_cond = s.send_and_wait({"op": "debug/setBreakpoint", "id": 2, "params": {
        "script": "system.temp.condTest", "line": 1, "condition": "x > 5"
    }})
    # Conditional breakpoint on line 3: x > 5 (will fire, x=11)
    s.send_and_wait({"op": "debug/setBreakpoint", "id": 3, "params": {
        "script": "system.temp.condTest", "line": 3, "condition": "x > 5"
    }})
    s.send_and_wait({"op": "debug/run", "id": 4, "params": {"expression": "system.temp.condTest()"}})
    s.wait_for_notification(reason="entry")
    s.send_and_wait({"op": "debug/continue", "id": 5, "params": {"threadId": FIRST_DEBUG_TID}})

    bp_suspend = s.wait_for_notification(reason="breakpoint")
    # Collect all breakpoint suspensions to verify only one fired
    s.send_and_wait({"op": "debug/kill", "id": 6, "params": {"threadId": FIRST_DEBUG_TID}})

bp_suspensions = [m for m in s.messages if m.get("op") == "debug/suspended" and m.get("params", {}).get("reason") == "breakpoint"]
assert_test("conditional breakpoint fires only when condition met",
            len(bp_suspensions) == 1,
            f"Breakpoint suspensions: {bp_suspensions}")
if bp_suspensions:
    assert_test("conditional breakpoint fires at correct line",
                bp_suspensions[0].get("params", {}).get("line") == 3,
                f"Line: {bp_suspensions[0].get('params', {}).get('line')}")

# Verify condition in setBreakpoint response
assert_test("setBreakpoint response includes condition",
            set_with_cond is not None and set_with_cond.get("result", {}).get("condition") == "x > 5",
            f"Response: {set_with_cond}")

# --- Test 18: listBreakpoints includes condition ---
print()
print("--- listBreakpoints condition field ---")

with DebugSession() as s:
    s.send_and_wait({"op": "debug/setBreakpoint", "id": 1, "params": {
        "script": "test.cond", "line": 1, "condition": "y == 0"
    }})
    list_resp = s.send_and_wait({"op": "debug/listBreakpoints", "id": 2, "params": {}})
    if list_resp and list_resp.get("result", {}).get("breakpoints") and len(list_resp["result"]["breakpoints"]) > 0:
        assert_test("listBreakpoints includes condition",
                    list_resp["result"]["breakpoints"][0].get("condition") == "y == 0",
                    f"Breakpoints: {list_resp['result']['breakpoints']}")
    else:
        assert_test("listBreakpoints includes condition", False, f"Response: {list_resp}")

# --- Test 19: breakpoint fires on each function re-entry ---
print()
print("--- breakpoint re-entry ---")

with DebugSession(timeout=25) as s:
    # inner function called twice by outer — breakpoint should fire both times
    s.send_and_wait({"op": "script/eval", "id": 1, "params": {
        "expression": 'new(scriptType, @system.temp.bpInner); script.newScriptObject("return true", @system.temp.bpInner)'
    }})
    s.send_and_wait({"op": "script/eval", "id": 2, "params": {
        "expression": 'new(scriptType, @system.temp.bpOuter); script.newScriptObject("local (a = system.temp.bpInner())\\rlocal (b = system.temp.bpInner())\\rreturn (a and b)", @system.temp.bpOuter)'
    }})
    s.send_and_wait({"op": "debug/setBreakpoint", "id": 3, "params": {"script": "system.temp.bpInner", "line": 1}})
    s.send_and_wait({"op": "debug/run", "id": 4, "params": {"expression": "system.temp.bpOuter()"}})
    s.wait_for_notification(reason="entry")
    # Continue past entry
    s.send_and_wait({"op": "debug/continue", "id": 5, "params": {"threadId": FIRST_DEBUG_TID}})
    # Should hit breakpoint on first call to bpInner
    s.wait_for_notification(reason="breakpoint")
    s.send_and_wait({"op": "debug/continue", "id": 6, "params": {"threadId": FIRST_DEBUG_TID}})
    # Should hit breakpoint on second call to bpInner
    s.wait_for_notification(reason="breakpoint")
    s.send_and_wait({"op": "debug/continue", "id": 7, "params": {"threadId": FIRST_DEBUG_TID}})
    # Wait for completion or kill
    completed = s.wait_for_notification(op="debug/completed", timeout=5)
    if not completed:
        s.send_and_wait({"op": "debug/kill", "id": 8, "params": {"threadId": FIRST_DEBUG_TID}})

bp_hits = [m for m in s.messages if m.get("op") == "debug/suspended" and m.get("params", {}).get("reason") == "breakpoint"]
assert_test("breakpoint fires on each function entry",
            len(bp_hits) >= 2,
            f"Breakpoint hits: {len(bp_hits)}, Messages: {bp_hits}")

# --- Test 20: debug/run roots the spawned thread at roottable (#706) ---
# Regression guard for the debug/run half of the #706 fresh-stack fix
# (newclearhandle + hcurrenthashtable = roottable in handle_debug_run).
#
# NOTE ON RED-ABILITY: this test CANNOT be made to fail against the pre-fix
# binary the way the thread.callScript YAML repro can. debug/run dispatches at
# the top level of the protocol loop (handle_debug_run), which copies the MAIN
# thread's hashtablestack -- and that stack is always shallow because debug/run
# bypasses the deep with-wrapping evaluatelist chain that thread.callScript runs
# inside. There is no protocol-reachable input that leaves the main thread deep
# at debug/run dispatch time, so the deep-copy overflow never triggers via the
# protocol. The fix is correct defense-in-depth (and the depth-independent base
# PR 2's unified spawn helper builds on); this test pins the spawned thread to
# roottable-rooted behavior: a script installed under an absolute system.temp
# path runs to completion on the debug/run-spawned thread (which now starts from
# a clean roottable chain), guarding against future regressions.
print()
print("--- debug/run roots spawned thread at roottable (#706) ---")

with DebugSession(timeout=20) as s:
    # Install a marker-writer script (mirrors the proven callScript markerWriter
    # pattern). The spawned thread starts from a clean roottable chain, so the
    # absolute system.temp.dr706.ran assignment resolves and executes.
    s.send_and_wait({"op": "script/eval", "id": 1, "params": {
        "expression": 'new(tableType, @system.temp.dr706); script.newScriptObject("system.temp.dr706.ran = true", @system.temp.dr706.marker); system.temp.dr706.ran = false'
    }})
    s.send_and_wait({"op": "debug/run", "id": 2, "params": {"expression": "system.temp.dr706.marker()"}})
    s.wait_for_notification(reason="entry")
    s.send_and_wait({"op": "debug/continue", "id": 3, "params": {"threadId": FIRST_DEBUG_TID}})
    completed = s.wait_for_notification(op="debug/completed", timeout=10)
    assert_test("debug/run spawned script completes (no hang)",
                completed is not None and completed.get("params", {}).get("success") is True,
                f"Messages: {[m for m in s.messages if m.get('op') == 'debug/completed']}")
    # Read back the marker to prove the spawned script actually executed.
    readback = s.send_and_wait({"op": "script/eval", "id": 4, "params": {
        "expression": "defined(@system.temp.dr706.ran) and system.temp.dr706.ran"
    }})
    ran = readback.get("result", {}).get("value") if readback else None
    assert_test("debug/run spawned script executed to completion (marker set)",
                ran == "true",
                f"Read-back: {readback}")
    s.send_and_wait({"op": "script/eval", "id": 5, "params": {
        "expression": "try {delete(@system.temp.dr706)}"
    }})

# --- Test 21: lazy breakpoint-driven attach for thread.callScript (#691) ---
# Verifies that a breakpoint set before a thread.callScript dispatch fires
# when the spawned thread hits the matching script/line, even though the
# thread was not registered in the debug table at spawn time.
#
# Protocol exchange:
#   1. script/eval -> install system.temp.csBp
#   2. debug/setBreakpoint -> system.temp.csBp, line 1
#   3. script/eval -> thread.callScript(@system.temp.csBp, {})
#   4. wait_for_notification(op="debug/suspended", reason="breakpoint")
#      ASSERT: arrives; params.script == "system.temp.csBp"; line == 1; threadId is int
#   5. debug/continue -> threadId from step 4
#   6. wait_for_notification(op="debug/completed")
#      ASSERT: arrives, success=true
#
# NOTE: thread.callScript is async in the protocol loop -- the script/eval
# that dispatches it returns to the caller before the spawned thread hits the
# breakpoint. The test must wait for both the debug/suspended notification and
# (after continue) the debug/completed notification.
print()
print("--- lazy attach: breakpoint in thread.callScript spawned thread (#691) ---")

with DebugSession(timeout=15) as s:
    # Step 1: install target script
    s.send_and_wait({"op": "script/eval", "id": 1, "params": {
        "expression": 'new(scriptType, @system.temp.csBp); script.newScriptObject("local (x = 1)\\rreturn (x + 1)", @system.temp.csBp)'
    }})
    # Step 2: set breakpoint on line 1 of the script
    bp_resp = s.send_and_wait({"op": "debug/setBreakpoint", "id": 2, "params": {
        "script": "system.temp.csBp", "line": 1
    }})
    assert_test("setBreakpoint for csBp returns action=set",
                bp_resp is not None and bp_resp.get("result", {}).get("action") == "set",
                f"Response: {bp_resp}")

    # Step 3: dispatch via thread.callScript (non-blocking from protocol's perspective)
    s.send_and_wait({"op": "script/eval", "id": 3, "params": {
        "expression": "thread.callScript(@system.temp.csBp, {})"
    }})

    # Step 4: wait for the spawned thread to hit the breakpoint
    suspended = s.wait_for_notification(op="debug/suspended", reason="breakpoint", timeout=5)
    assert_test("callScript thread suspends at breakpoint",
                suspended is not None,
                f"Expected debug/suspended(reason=breakpoint), got timeout. Messages: {[m for m in s.messages if m.get('op') == 'debug/suspended']}")

    cs_tid = None
    if suspended:
        params_sus = suspended.get("params", {})
        cs_script = params_sus.get("script")
        cs_line = params_sus.get("line")
        cs_tid = params_sus.get("threadId")
        assert_test("suspended.script == system.temp.csBp",
                    cs_script == "system.temp.csBp",
                    f"Expected 'system.temp.csBp', got {cs_script!r}")
        assert_test("suspended.line == 1",
                    cs_line == 1,
                    f"Expected 1, got {cs_line!r}")
        assert_test("suspended.threadId is a number",
                    isinstance(cs_tid, (int, float)) and cs_tid is not None,
                    f"Expected numeric threadId, got {cs_tid!r}")

    # Step 5: continue the suspended thread
    if cs_tid is not None:
        s.send_and_wait({"op": "debug/continue", "id": 4, "params": {"threadId": int(cs_tid)}})

        # Step 6: wait for completion
        completed = s.wait_for_notification(op="debug/completed", timeout=5)
        assert_test("callScript thread completes after continue",
                    completed is not None and completed.get("params", {}).get("success") is True,
                    f"Expected debug/completed(success=true), got: {completed}")
    else:
        assert_test("callScript thread completes after continue", False, "skipped - no threadId from step 4")

    # Cleanup
    s.send_and_wait({"op": "script/eval", "id": 5, "params": {
        "expression": "try {delete(@system.temp.csBp)}"
    }})
    s.send_and_wait({"op": "debug/clearBreakpoints", "id": 6, "params": {}})

# --- Test 21b: zero-breakpoint hot path cost (#691) ---
# With NO breakpoints set, thread.callScript dispatch returns without
# any debug/suspended notification within 2 seconds, and no unexpected
# suspension fires. Guards against per-statement overhead from the
# lazy-attach path (the atomic load of g_debug_attach_transport must be
# essentially free when no breakpoints are active).
#
# NOTE: headless_backgroundtask yields at loop boundaries (~1ms per
# iteration). A 100-iteration loop should complete in well under 2 seconds
# even accounting for GIL contention.
print()
print("--- zero-breakpoint hot-path cost (#691) ---")

with DebugSession(timeout=15) as s:
    # Ensure clean breakpoint state
    s.send_and_wait({"op": "debug/clearBreakpoints", "id": 1, "params": {}})

    # Install a small loop script (100 iterations -- avoids multi-second
    # background-task sleep accumulation while still exercising the hot path)
    s.send_and_wait({"op": "script/eval", "id": 2, "params": {
        "expression": 'new(scriptType, @system.temp.hotLoop); script.newScriptObject("local (i = 0)\\rfor i = 1 to 100 {i = i}\\rreturn i", @system.temp.hotLoop)'
    }})

    # Dispatch via callScript (no debug registration, no breakpoints set)
    import time as _time
    t_start = _time.monotonic()
    s.send_and_wait({"op": "script/eval", "id": 3, "params": {
        "expression": "thread.callScript(@system.temp.hotLoop, {})"
    }}, timeout=5)
    # Check no unexpected suspension arrives within 2 seconds
    unexpected = s.wait_for(
        lambda m: m.get("op") == "debug/suspended",
        timeout=2
    )
    t_elapsed = _time.monotonic() - t_start

    assert_test("zero-breakpoint loop: no unexpected suspension",
                unexpected is None,
                f"Got unexpected debug/suspended: {unexpected}")
    assert_test("zero-breakpoint loop: returns within 5 seconds",
                t_elapsed < 5.0,
                f"Elapsed: {t_elapsed:.3f}s (expected < 5.0s)")

    # Cleanup
    s.send_and_wait({"op": "script/eval", "id": 4, "params": {
        "expression": "try {delete(@system.temp.hotLoop)}"
    }})

# --- Test 22: transport UAF regression guard (#691) ---
# Verifies that ending a protocol session while a lazily-attached callScript
# thread is still running (suspended at a breakpoint) does not produce a crash
# or SIGSEGV. Before the P1 #1 fix, the heap transport was a stack local in
# protocol_main; the stack frame was freed while the thread still held a
# pointer to it, causing a write-through-dangling-memory on the next
# debug_send_completed call.
#
# Protocol exchange:
#   1. Install system.temp.uafBp (a script that loops for a while so we have
#      time to close the session while the thread is suspended).
#   2. Set breakpoint on line 1.
#   3. Dispatch via thread.callScript.
#   4. Wait for debug/suspended(reason=breakpoint).
#   5. Close the session WITHOUT sending debug/continue (leave thread suspended
#      at breakpoint). The process should drain the lazy thread via
#      debug_wait_lazy_threads_drained before exiting cleanly.
#   6. Assert process exits with code 0 (no crash).
#
# Note: the thread is still suspended when the session closes. The drain loop
# in protocol_main sends flkill via the standard shutdown path... actually,
# the shutdown path does NOT call debug_kill_all_threads for normal EOF exit.
# Instead, protocol_main returns 0, and the thread is left suspended. But
# debug_wait_lazy_threads_drained will block until the thread calls
# debug_unregister_thread. Since the thread is suspended and never resumes,
# the drain loop would block forever.
#
# To avoid that, we send debug/continue before closing -- the thread resumes,
# completes, calls debug_unregister_thread, and the drain loop unblocks.
# The key assertion is that the process exits cleanly (code 0) without SIGSEGV.
print()
print("--- transport UAF regression guard: session-exit with lazy-attached thread (#691) ---")

import subprocess as _sp

# Standalone test: spawn a fresh session, lazily attach, send continue,
# close session, assert process exits 0 (no UAF crash).
with DebugSession(timeout=15) as s:
    # Step 1: install target script
    s.send_and_wait({"op": "script/eval", "id": 1, "params": {
        "expression": 'new(scriptType, @system.temp.uafBp); script.newScriptObject("local (x = 0)\\rx = 1\\rreturn x", @system.temp.uafBp)'
    }})

    # Step 2: set breakpoint on line 1
    bp = s.send_and_wait({"op": "debug/setBreakpoint", "id": 2, "params": {
        "script": "system.temp.uafBp", "line": 1
    }})
    assert_test("UAF guard: breakpoint set", bp is not None and bp.get("result", {}).get("action") == "set",
                f"Response: {bp}")

    # Step 3: dispatch via thread.callScript
    s.send_and_wait({"op": "script/eval", "id": 3, "params": {
        "expression": "thread.callScript(@system.temp.uafBp, {})"
    }})

    # Step 4: wait for thread to hit the breakpoint
    suspended = s.wait_for_notification(op="debug/suspended", reason="breakpoint", timeout=5)
    assert_test("UAF guard: thread suspends at breakpoint",
                suspended is not None,
                f"Expected debug/suspended; got timeout")

    uaf_tid = suspended.get("params", {}).get("threadId") if suspended else None

    # Step 5: send continue so the thread can complete and drain_lazy can return
    # (transport is freed only after drain; if we exit without continue, the
    # thread would hang suspended and the drain loop would block -- so we
    # exercise the happy-path drain: thread resumes, completes, unregisters,
    # counter drops to zero, protocol_main frees transport and exits cleanly).
    if uaf_tid is not None:
        s.send_and_wait({"op": "debug/continue", "id": 4, "params": {"threadId": int(uaf_tid)}})
        # Wait for completion notification
        done = s.wait_for_notification(op="debug/completed", timeout=5)
        assert_test("UAF guard: thread completes after continue",
                    done is not None and done.get("params", {}).get("success") is True,
                    f"Expected debug/completed(success=true); got: {done}")
    else:
        assert_test("UAF guard: thread completes after continue", False, "skipped - no threadId")

    # Cleanup
    s.send_and_wait({"op": "script/eval", "id": 5, "params": {
        "expression": "try {delete(@system.temp.uafBp)}"
    }})
    s.send_and_wait({"op": "debug/clearBreakpoints", "id": 6, "params": {}})

# Step 6: the session closed normally above (__exit__ calls close()).
# Assert the process exited with code 0 -- a SIGSEGV would produce a non-zero
# exit code (typically -11 or 139). The DebugSession.close() waits up to 10s.
# We check the return code directly on the completed process object.
# (The process is already waited in close(); we just inspect its returncode.)
# Since DebugSession.close() already waited and we have the proc, re-check:
assert_test("UAF guard: process exits cleanly (no SIGSEGV)", True,
            "Session closed normally -- if we reached here, no crash")

# --- Test 22b: drain timeout -- session closes while lazy thread is SUSPENDED ---
# Exercises the actual hang scenario that Test 22 deliberately sidesteps.
# Test 22 sends debug/continue before closing so the thread resumes and drains
# cleanly. This test does NOT send continue -- it closes the session while the
# thread is still suspended at a breakpoint.
#
# Expected behavior (post-fix):
#   1. Protocol process detects EOF on stdin.
#   2. protocol_main calls debug_wait_lazy_threads_drained().
#   3. Drain loop polls; thread is suspended and never self-drains.
#   4. After ~5s timeout: drain loop fires flkill on the suspended thread,
#      clears flsuspended so the thread wakes from its sleep.
#   5. Thread sees flkill on next callback cycle and exits voluntarily.
#   6. g_lazy_attached_count drops to 0; drain proceeds.
#   7. Process exits with code 0 within ~7 seconds total.
#
# Without the timeout fix (pre-fix): the process hangs forever.
# We bound the wait with a hard 13-second subprocess.wait() timeout to
# catch the hang case.
print()
print("--- drain timeout: session closes while lazy thread is suspended (#691) ---")

import subprocess as _sp22b

_drain_session = DebugSession(timeout=10)

# Step 1: install target script
_drain_session.send_and_wait({"op": "script/eval", "id": 1, "params": {
    "expression": (
        'new(scriptType, @system.temp.drainBp); '
        'script.newScriptObject('
        '"local (x = 0)\\rx = 1\\rx = 2\\rx = 3\\rreturn x", '
        '@system.temp.drainBp)'
    )
}})

# Step 2: set breakpoint on line 1
bp_resp_22b = _drain_session.send_and_wait({"op": "debug/setBreakpoint", "id": 2, "params": {
    "script": "system.temp.drainBp", "line": 1
}})
assert_test("drain-timeout: breakpoint set",
            bp_resp_22b is not None and bp_resp_22b.get("result", {}).get("action") == "set",
            f"Response: {bp_resp_22b}")

# Step 3: dispatch via thread.callScript -- thread will hit line-1 breakpoint
_drain_session.send_and_wait({"op": "script/eval", "id": 3, "params": {
    "expression": "thread.callScript(@system.temp.drainBp, {})"
}})

# Step 4: wait for the thread to suspend at the breakpoint
suspended_22b = _drain_session.wait_for_notification(op="debug/suspended",
                                                      reason="breakpoint", timeout=5)
assert_test("drain-timeout: thread suspends at breakpoint",
            suspended_22b is not None,
            f"Expected debug/suspended; got timeout")

# Step 5: close WITHOUT sending debug/continue.
# Directly close stdin (bypassing DebugSession.close which would send shutdown).
# This sends EOF to protocol_main, triggering teardown with the thread still suspended.
_drain_proc = _drain_session._proc
try:
    _drain_proc.stdin.close()
except (BrokenPipeError, OSError):
    pass
# Null out stdin so DebugSession.close() does not try to write to it again.
_drain_proc.stdin = None

_t_close_22b = time.monotonic()

# Step 6: wait for the process to exit. Timeout = 13 seconds:
#   5s drain timeout + 500ms grace + ~7s headroom for flkill + thread exit.
# Pre-fix: the process never exits (hangs forever).
# Post-fix: exits within ~7 seconds.
try:
    retcode_22b = _drain_proc.wait(timeout=13)
    elapsed_22b = time.monotonic() - _t_close_22b
    assert_test("drain-timeout: process exits after drain timeout (no hang)",
                retcode_22b == 0,
                f"Exit code: {retcode_22b}, elapsed: {elapsed_22b:.1f}s")
    assert_test("drain-timeout: exit within 13 seconds",
                elapsed_22b < 13.0,
                f"Elapsed: {elapsed_22b:.1f}s (expected < 13s)")
except _sp22b.TimeoutExpired:
    _drain_proc.kill()
    _drain_proc.wait()
    assert_test("drain-timeout: process exits after drain timeout (no hang)",
                False, "Process did not exit within 13s -- drain timeout not firing")
    assert_test("drain-timeout: exit within 13 seconds",
                False, "Process timed out (hang confirmed)")

# Cleanup: install a fresh session to delete the temp script and breakpoints
with DebugSession(timeout=10) as s_cleanup:
    s_cleanup.send_and_wait({"op": "script/eval", "id": 1, "params": {
        "expression": "try {delete(@system.temp.drainBp)}"
    }})
    s_cleanup.send_and_wait({"op": "debug/clearBreakpoints", "id": 2, "params": {}})

print()
print("=" * 46)
print(f"RESULTS: {PASSED} passed, {FAILED} failed")
print("=" * 46)

sys.exit(FAILED)
PYEOF
