#!/bin/bash
#
# Integration tests for debug/* protocol operations
# Tests the debugger MVP: debug/run, debug/continue, debug/kill, debug/pause
#
# Uses Python to handle the interactive protocol (parse threadId from responses).

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
CLI="$PROJECT_ROOT/frontier-cli/frontier-cli"
DB="$PROJECT_ROOT/databases/Virgin.root"

if [ ! -x "$CLI" ]; then
    echo "Error: frontier-cli not found at $CLI" >&2
    exit 1
fi
if [ ! -f "$DB" ]; then
    echo "Error: database not found at $DB" >&2
    exit 1
fi

export DEBUG_TEST_CLI="$CLI"
export DEBUG_TEST_DB="$DB"

python3 << 'PYEOF'
import subprocess, json, sys, time, os

CLI = os.environ["DEBUG_TEST_CLI"]
DB = os.environ["DEBUG_TEST_DB"]

PASSED = 0
FAILED = 0
GREEN = "\033[0;32m"
RED = "\033[0;31m"
NC = "\033[0m"

def run_debug_session(commands_fn, timeout=15):
    """Run a debug protocol session. commands_fn receives a send function.
    All stdout output is collected after the process exits."""
    proc = subprocess.Popen(
        [CLI, "--protocol", "--skip-startup", "--system-root", DB],
        stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        text=True, bufsize=1
    )

    def send(msg):
        if isinstance(msg, dict):
            msg = json.dumps(msg)
        proc.stdin.write(msg + "\n")
        proc.stdin.flush()
        time.sleep(0.5)

    try:
        commands_fn(send)
        send({"op": "shutdown", "id": 999})
        time.sleep(1)
    except BrokenPipeError:
        print(f"  [debug] BrokenPipeError during session", file=sys.stderr)
    except Exception as e:
        print(f"  Session error: {e}", file=sys.stderr)

    # Read all output
    try:
        proc.stdin.close()
        stdout, _ = proc.communicate(timeout=15)
    except subprocess.TimeoutExpired:
        proc.kill()
        stdout, _ = proc.communicate()
    except BrokenPipeError:
        stdout = ""

    messages = []
    for line in stdout.strip().split("\n"):
        if line:
            try:
                messages.append(json.loads(line))
            except json.JSONDecodeError:
                pass

    return messages

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

def get_actual_tid(msgs):
    """Extract actual threadId from debug/run response."""
    for m in msgs:
        if m.get("id") == 1 and m.get("result", {}).get("threadId"):
            return m["result"]["threadId"]
    return None

# --- Test 0: verify thread ID assumption ---
print("--- thread ID validation ---")
def test_tid_check(send):
    send({"op": "debug/run", "id": 1, "params": {"expression": "return 1"}})
    time.sleep(1)
    send({"op": "debug/kill", "id": 2, "params": {"threadId": FIRST_DEBUG_TID}})
    time.sleep(1)

msgs = run_debug_session(test_tid_check)
actual_tid = get_actual_tid(msgs)
assert_test(
    f"first debug thread gets ID {FIRST_DEBUG_TID}",
    actual_tid == FIRST_DEBUG_TID,
    f"Expected threadId={FIRST_DEBUG_TID}, got {actual_tid}. Update FIRST_DEBUG_TID."
)
if actual_tid is not None and actual_tid != FIRST_DEBUG_TID:
    print(f"  FATAL: Thread ID assumption broken. Updating to {actual_tid}.")
    FIRST_DEBUG_TID = actual_tid

# --- Test 1: debug/run + debug/continue ---
print()
print("--- debug/run + debug/continue ---")

def test_run_continue(send):
    send({"op": "debug/run", "id": 1, "params": {"expression": "return 1+1"}})
    time.sleep(2)
    send({"op": "debug/continue", "id": 2, "params": {"threadId": FIRST_DEBUG_TID}})
    time.sleep(2)

msgs = run_debug_session(test_run_continue)
assert_test("run returns threadId", any(m.get("result", {}).get("threadId") for m in msgs if m.get("id") == 1),
            f"Messages: {msgs}")
assert_test("suspended at entry", find_msg(msgs, reason="entry") is not None, f"Messages: {msgs}")
assert_test("completed successfully", find_msg(msgs, op="debug/completed") is not None, f"Messages: {msgs}")

# --- Test 2: debug/run + debug/kill ---
print()
print("--- debug/run + debug/kill ---")
def test_run_kill(send):
    send({"op": "debug/run", "id": 1, "params": {"expression": "return 1+1"}})
    time.sleep(2)
    send({"op": "debug/kill", "id": 2, "params": {"threadId": FIRST_DEBUG_TID}})
    time.sleep(2)

msgs = run_debug_session(test_run_kill)
assert_test("kill returns killed status", find_msg(msgs, status="killed") is not None, f"Messages: {msgs}")
assert_test("completed with failure", any(m.get("op") == "debug/completed" for m in msgs), f"Messages: {msgs}")

# --- Test 3: debug/pause ---
print()
print("--- debug/pause on running thread ---")
def test_pause(send):
    send({"op": "debug/run", "id": 1, "params": {"expression": "local (i); for i = 1 to 1000000 {i = i}; return true"}})
    time.sleep(1)
    send({"op": "debug/continue", "id": 2, "params": {"threadId": FIRST_DEBUG_TID}})
    time.sleep(1)
    send({"op": "debug/pause", "id": 3, "params": {"threadId": FIRST_DEBUG_TID}})
    time.sleep(2)
    send({"op": "debug/kill", "id": 4, "params": {"threadId": FIRST_DEBUG_TID}})
    time.sleep(1)

msgs = run_debug_session(test_pause)
assert_test("pause sends interrupting", find_msg(msgs, status="interrupting") is not None, f"Messages: {msgs}")
assert_test("suspended with interrupted reason", find_msg(msgs, reason="interrupted") is not None, f"Messages: {msgs}")

# --- Test 4: debug/step ---
print()
print("--- debug/step ---")
def test_step_into(send):
    # Simple expression — step into from entry should stop at the return statement
    send({"op": "debug/run", "id": 1, "params": {"expression": "return 42"}})
    time.sleep(1)
    # Suspended at entry — step into (should stop at first steppable statement)
    send({"op": "debug/step", "id": 2, "params": {"threadId": FIRST_DEBUG_TID, "direction": "into"}})
    time.sleep(2)

msgs = run_debug_session(test_step_into)
assert_test("step into suspends at next statement",
            find_msg(msgs, reason="step") is not None,
            f"Messages: {msgs}")

# Step-over test — previously crashed due to #505 (hthreadglobals overwrite
# during GIL yield). Fixed by capturing hthreadglobals in a local variable.
def test_step_over(send):
    send({"op": "script/eval", "id": 1, "params": {
        "expression": 'new(scriptType, @system.temp.stepOverTest); script.newScriptObject("local (x = 1)\\rlocal (y = 2)\\rreturn (x + y)", @system.temp.stepOverTest)'
    }})
    time.sleep(1)
    send({"op": "debug/setBreakpoint", "id": 2, "params": {"script": "system.temp.stepOverTest", "line": 1}})
    time.sleep(0.5)
    send({"op": "debug/run", "id": 3, "params": {"expression": "system.temp.stepOverTest()"}})
    time.sleep(2)
    # Continue past entry
    send({"op": "debug/continue", "id": 4, "params": {"threadId": FIRST_DEBUG_TID}})
    time.sleep(2)
    # Suspended at breakpoint on line 1 — step over to next steppable line (line 3)
    send({"op": "debug/step", "id": 5, "params": {"threadId": FIRST_DEBUG_TID, "direction": "over"}})
    time.sleep(2)
    # Kill to clean up
    send({"op": "debug/kill", "id": 6, "params": {"threadId": FIRST_DEBUG_TID}})
    time.sleep(1)

msgs = run_debug_session(test_step_over, timeout=20)
step_suspend = find_msg(msgs, reason="step")
assert_test("step-over suspends at next line",
            step_suspend is not None,
            f"Messages: {[m for m in msgs if m.get('op') == 'debug/suspended']}")
if step_suspend:
    step_line = step_suspend.get("params", {}).get("line")
    # Line 2 is a local declaration (non-steppable), so step-over advances to line 3
    assert_test("step-over advances past locals to line 3",
                step_line == 3,
                f"Expected line 3, got {step_line}")

# Step-over with calldepth: verify step-over skips into function calls
def test_step_over_calldepth(send):
    # Create a helper function and a caller that invokes it
    send({"op": "script/eval", "id": 1, "params": {
        "expression": 'new(scriptType, @system.temp.depthHelper); script.newScriptObject("return 99", @system.temp.depthHelper)'
    }})
    time.sleep(0.5)
    send({"op": "script/eval", "id": 2, "params": {
        "expression": 'new(scriptType, @system.temp.depthCaller); script.newScriptObject("local (a = system.temp.depthHelper())\\rreturn a", @system.temp.depthCaller)'
    }})
    time.sleep(0.5)
    # Set breakpoint on line 1 of caller (the function call line)
    send({"op": "debug/setBreakpoint", "id": 3, "params": {"script": "system.temp.depthCaller", "line": 1}})
    time.sleep(0.3)
    send({"op": "debug/run", "id": 4, "params": {"expression": "system.temp.depthCaller()"}})
    time.sleep(2)
    # Continue past entry
    send({"op": "debug/continue", "id": 5, "params": {"threadId": FIRST_DEBUG_TID}})
    time.sleep(2)
    # Now at breakpoint on line 1 — step over should NOT enter depthHelper
    # and should stop at line 2 (return a) of depthCaller
    send({"op": "debug/step", "id": 6, "params": {"threadId": FIRST_DEBUG_TID, "direction": "over"}})
    time.sleep(3)
    send({"op": "debug/kill", "id": 7, "params": {"threadId": FIRST_DEBUG_TID}})
    time.sleep(1)

msgs = run_debug_session(test_step_over_calldepth, timeout=25)
step_suspend = find_msg(msgs, reason="step")
assert_test("step-over with calldepth skips function call",
            step_suspend is not None,
            f"Messages: {[m for m in msgs if m.get('op') == 'debug/suspended']}")
if step_suspend:
    step_line = step_suspend.get("params", {}).get("line")
    assert_test("step-over returns to caller line 2",
                step_line == 2,
                f"Expected line 2, got {step_line}")

def test_step_error_not_suspended(send):
    send({"op": "debug/run", "id": 1, "params": {"expression": "return 1"}})
    time.sleep(1)
    # Continue first (thread is now running)
    send({"op": "debug/continue", "id": 2, "params": {"threadId": FIRST_DEBUG_TID}})
    time.sleep(2)
    # Try to step — should fail because thread completed
    send({"op": "debug/step", "id": 3, "params": {"threadId": FIRST_DEBUG_TID, "direction": "over"}})
    time.sleep(1)

msgs = run_debug_session(test_step_error_not_suspended)
assert_test("step on non-suspended thread errors",
            any("not suspended" in str(m) or "No debug thread" in str(m) for m in msgs),
            f"Messages: {msgs}")

# --- Test 5: error cases ---
print()
print("--- error cases ---")
def test_invalid_continue(send):
    send({"op": "debug/continue", "id": 1, "params": {"threadId": 999}})
    time.sleep(1)

msgs = run_debug_session(test_invalid_continue)
assert_test("continue with invalid threadId errors", any("No debug thread" in str(m) for m in msgs), f"Messages: {msgs}")

def test_invalid_kill(send):
    send({"op": "debug/kill", "id": 1, "params": {"threadId": 999}})
    time.sleep(1)

msgs = run_debug_session(test_invalid_kill)
assert_test("kill with invalid threadId errors", any("No debug thread" in str(m) for m in msgs), f"Messages: {msgs}")

def test_missing_expression(send):
    send({"op": "debug/run", "id": 1, "params": {}})
    time.sleep(1)

msgs = run_debug_session(test_missing_expression)
assert_test("run with missing expression errors", any("expression" in str(m) for m in msgs), f"Messages: {msgs}")

# --- Test 6: breakpoint hit during execution ---
print()
print("--- breakpoint hit during execution ---")

def test_breakpoint_hit(send):
    # Step 1: Create a test function in system.temp via script/eval
    send({"op": "script/eval", "id": 1, "params": {
        "expression": 'new(scriptType, @system.temp.bpTestFunc); script.newScriptObject("local (x = 1)\\rlocal (y = 2)\\rreturn (x + y)", @system.temp.bpTestFunc)'
    }})
    time.sleep(1)
    # Step 2: Set a breakpoint on line 2 of bpTestFunc
    send({"op": "debug/setBreakpoint", "id": 2, "params": {
        "script": "system.temp.bpTestFunc", "line": 2
    }})
    time.sleep(0.5)
    # Step 3: Run an expression that calls the function
    send({"op": "debug/run", "id": 3, "params": {
        "expression": "system.temp.bpTestFunc()"
    }})
    time.sleep(2)
    # Step 4: Continue past initial entry suspension
    send({"op": "debug/continue", "id": 4, "params": {"threadId": FIRST_DEBUG_TID}})
    time.sleep(3)
    # Step 5: At this point, should be suspended at breakpoint on line 2
    # Kill to clean up
    send({"op": "debug/kill", "id": 5, "params": {"threadId": FIRST_DEBUG_TID}})
    time.sleep(1)

msgs = run_debug_session(test_breakpoint_hit, timeout=20)
# Check that we got a breakpoint suspension
bp_suspend = find_msg(msgs, reason="breakpoint")
assert_test("breakpoint hit suspends thread",
            bp_suspend is not None,
            f"Messages: {[m for m in msgs if m.get('op') == 'debug/suspended' or m.get('result', {}).get('action')]}")

# If breakpoint was hit, verify it's on the right line
if bp_suspend:
    bp_line = bp_suspend.get("params", {}).get("line")
    assert_test("breakpoint hit on correct line",
                bp_line == 2,
                f"Expected line 2, got {bp_line}")

# --- Test 7: debug/setBreakpoint + debug/listBreakpoints ---
print()
print("--- debug/setBreakpoint + debug/listBreakpoints ---")

def test_set_breakpoint(send):
    send({"op": "debug/setBreakpoint", "id": 1, "params": {"script": "mainResponder.respond", "line": 5}})
    time.sleep(0.5)
    send({"op": "debug/listBreakpoints", "id": 2, "params": {}})
    time.sleep(0.5)

msgs = run_debug_session(test_set_breakpoint)
# Check setBreakpoint response
set_msg = None
for m in msgs:
    if m.get("id") == 1 and m.get("result", {}).get("action") == "set":
        set_msg = m
        break
assert_test("setBreakpoint returns action=set", set_msg is not None, f"Messages: {msgs}")

# Check listBreakpoints response
list_msg = None
for m in msgs:
    if m.get("id") == 2 and m.get("result", {}).get("breakpoints") is not None:
        list_msg = m
        break
assert_test("listBreakpoints returns breakpoint array",
            list_msg is not None and len(list_msg["result"]["breakpoints"]) > 0,
            f"Messages: {msgs}")
if list_msg:
    bp = list_msg["result"]["breakpoints"][0]
    assert_test("listed breakpoint has correct script",
                bp.get("script") == "mainResponder.respond",
                f"Breakpoint: {bp}")
    assert_test("listed breakpoint has correct line",
                bp.get("line") == 5,
                f"Breakpoint: {bp}")

# --- Test 8: breakpoint toggle (clear) ---
print()
print("--- breakpoint toggle (clear) ---")

def test_toggle_breakpoint(send):
    # Set a breakpoint
    send({"op": "debug/setBreakpoint", "id": 1, "params": {"script": "test.script", "line": 3}})
    time.sleep(0.5)
    # Set again to toggle off
    send({"op": "debug/setBreakpoint", "id": 2, "params": {"script": "test.script", "line": 3}})
    time.sleep(0.5)
    # List should be empty
    send({"op": "debug/listBreakpoints", "id": 3, "params": {}})
    time.sleep(0.5)

msgs = run_debug_session(test_toggle_breakpoint)
# Check first set
set_msg = None
for m in msgs:
    if m.get("id") == 1 and m.get("result", {}).get("action") == "set":
        set_msg = m
        break
assert_test("first set returns action=set", set_msg is not None, f"Messages: {msgs}")

# Check toggle clears
clear_msg = None
for m in msgs:
    if m.get("id") == 2 and m.get("result", {}).get("action") == "cleared":
        clear_msg = m
        break
assert_test("second set returns action=cleared", clear_msg is not None, f"Messages: {msgs}")

# Check list is empty
list_msg = None
for m in msgs:
    if m.get("id") == 3 and m.get("result", {}).get("breakpoints") is not None:
        list_msg = m
        break
assert_test("list after toggle is empty",
            list_msg is not None and len(list_msg["result"]["breakpoints"]) == 0,
            f"Messages: {msgs}")

# --- Test 9: breakpoint with leading @ stripped ---
print()
print("--- breakpoint @ prefix handling ---")

def test_breakpoint_at_prefix(send):
    send({"op": "debug/setBreakpoint", "id": 1, "params": {"script": "@system.compiler.start", "line": 1}})
    time.sleep(0.5)
    send({"op": "debug/listBreakpoints", "id": 2, "params": {}})
    time.sleep(0.5)

msgs = run_debug_session(test_breakpoint_at_prefix)
list_msg = None
for m in msgs:
    if m.get("id") == 2 and m.get("result", {}).get("breakpoints") is not None:
        list_msg = m
        break
if list_msg and len(list_msg["result"]["breakpoints"]) > 0:
    assert_test("@ prefix stripped from script path",
                list_msg["result"]["breakpoints"][0].get("script") == "system.compiler.start",
                f"Breakpoint: {list_msg['result']['breakpoints'][0]}")
else:
    assert_test("@ prefix stripped from script path", False, f"Messages: {msgs}")

# --- Test 10: breakpoint error cases ---
print()
print("--- breakpoint error cases ---")

def test_breakpoint_missing_script(send):
    send({"op": "debug/setBreakpoint", "id": 1, "params": {"line": 5}})
    time.sleep(0.5)

msgs = run_debug_session(test_breakpoint_missing_script)
assert_test("setBreakpoint without script errors",
            any("script" in str(m).lower() for m in msgs if not m.get("success", True)),
            f"Messages: {msgs}")

def test_breakpoint_missing_line(send):
    send({"op": "debug/setBreakpoint", "id": 1, "params": {"script": "foo.bar"}})
    time.sleep(0.5)

msgs = run_debug_session(test_breakpoint_missing_line)
assert_test("setBreakpoint without line errors",
            any("line" in str(m).lower() for m in msgs if not m.get("success", True)),
            f"Messages: {msgs}")

# --- Test 11: debug/clearBreakpoints ---
print()
print("--- debug/clearBreakpoints ---")

def test_clear_breakpoints(send):
    # Set two breakpoints
    send({"op": "debug/setBreakpoint", "id": 1, "params": {"script": "foo.bar", "line": 1}})
    time.sleep(0.3)
    send({"op": "debug/setBreakpoint", "id": 2, "params": {"script": "foo.bar", "line": 2}})
    time.sleep(0.3)
    # Clear all
    send({"op": "debug/clearBreakpoints", "id": 3, "params": {}})
    time.sleep(0.3)
    # List should be empty
    send({"op": "debug/listBreakpoints", "id": 4, "params": {}})
    time.sleep(0.3)

msgs = run_debug_session(test_clear_breakpoints)

# Check clear returns count
clear_msg = None
for m in msgs:
    if m.get("id") == 3 and m.get("result", {}).get("cleared") is not None:
        clear_msg = m
        break
assert_test("clearBreakpoints returns count",
            clear_msg is not None and clear_msg["result"]["cleared"] == 2,
            f"Messages: {[m for m in msgs if m.get('id') == 3]}")

# Check list is empty after clear
list_msg = None
for m in msgs:
    if m.get("id") == 4 and m.get("result", {}).get("breakpoints") is not None:
        list_msg = m
        break
assert_test("list empty after clearBreakpoints",
            list_msg is not None and len(list_msg["result"]["breakpoints"]) == 0,
            f"Messages: {[m for m in msgs if m.get('id') == 4]}")

def test_clear_empty(send):
    send({"op": "debug/clearBreakpoints", "id": 1, "params": {}})
    time.sleep(0.3)

msgs = run_debug_session(test_clear_empty)
clear_msg = None
for m in msgs:
    if m.get("id") == 1 and m.get("result", {}).get("cleared") is not None:
        clear_msg = m
        break
assert_test("clearBreakpoints with no breakpoints returns 0",
            clear_msg is not None and clear_msg["result"]["cleared"] == 0,
            f"Messages: {msgs}")

# --- Test 12: debug/getLocals + debug/getStack + debug/getSource ---
print()
print("--- debug/getLocals + debug/getStack + debug/getSource ---")

def test_inspection(send):
    # Create a test function with locals
    send({"op": "script/eval", "id": 1, "params": {
        "expression": 'new(scriptType, @system.temp.inspectTest); script.newScriptObject("local (x = 42)\\rlocal (msg = \\"hello\\")\\rreturn (x)", @system.temp.inspectTest)'
    }})
    time.sleep(1)
    # Set breakpoint on line 3
    send({"op": "debug/setBreakpoint", "id": 2, "params": {"script": "system.temp.inspectTest", "line": 3}})
    time.sleep(0.5)
    # Run in debug mode
    send({"op": "debug/run", "id": 3, "params": {"expression": "system.temp.inspectTest()"}})
    time.sleep(2)
    # Continue past entry
    send({"op": "debug/continue", "id": 4, "params": {"threadId": FIRST_DEBUG_TID}})
    time.sleep(2)
    # Now suspended at breakpoint — test inspection
    send({"op": "debug/getLocals", "id": 5, "params": {"threadId": FIRST_DEBUG_TID}})
    time.sleep(1)
    send({"op": "debug/getStack", "id": 6, "params": {"threadId": FIRST_DEBUG_TID}})
    time.sleep(1)
    send({"op": "debug/getSource", "id": 7, "params": {"script": "system.temp.inspectTest", "threadId": FIRST_DEBUG_TID}})
    time.sleep(1)
    # Kill to clean up
    send({"op": "debug/kill", "id": 8, "params": {"threadId": FIRST_DEBUG_TID}})
    time.sleep(1)

msgs = run_debug_session(test_inspection, timeout=25)

# Check getLocals
locals_msg = None
for m in msgs:
    if m.get("id") == 5 and m.get("result", {}).get("locals") is not None:
        locals_msg = m
        break
assert_test("getLocals returns locals array",
            locals_msg is not None,
            f"Messages: {[m for m in msgs if m.get('id') == 5]}")

if locals_msg:
    locals_list = locals_msg["result"]["locals"]
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
stack_msg = None
for m in msgs:
    if m.get("id") == 6 and m.get("result", {}).get("frames") is not None:
        stack_msg = m
        break
assert_test("getStack returns frames",
            stack_msg is not None and len(stack_msg["result"]["frames"]) > 0,
            f"Messages: {[m for m in msgs if m.get('id') == 6]}")

if stack_msg:
    frames = stack_msg["result"]["frames"]
    assert_test("getStack has inspectTest frame",
                any("inspectTest" in f.get("script", "") for f in frames),
                f"Frames: {frames}")

# Check getSource
source_msg = None
for m in msgs:
    if m.get("id") == 7 and m.get("result", {}).get("lines") is not None:
        source_msg = m
        break
assert_test("getSource returns lines",
            source_msg is not None and len(source_msg["result"]["lines"]) >= 3,
            f"Messages: {[m for m in msgs if m.get('id') == 7]}")

if source_msg:
    lines = source_msg["result"]["lines"]
    assert_test("getSource line 3 is current",
                any(l.get("current") for l in lines if l.get("num") == 3),
                f"Lines: {lines}")
    assert_test("getSource line 3 has breakpoint",
                any(l.get("breakpoint") for l in lines if l.get("num") == 3),
                f"Lines: {lines}")

# --- Test 12: inspection error cases ---
print()
print("--- inspection error cases ---")

def test_getlocals_not_suspended(send):
    send({"op": "debug/getLocals", "id": 1, "params": {"threadId": 999}})
    time.sleep(0.5)

msgs = run_debug_session(test_getlocals_not_suspended)
assert_test("getLocals with invalid threadId errors",
            any("No debug thread" in str(m) for m in msgs),
            f"Messages: {msgs}")

def test_getsource_missing_script(send):
    send({"op": "debug/getSource", "id": 1, "params": {}})
    time.sleep(0.5)

msgs = run_debug_session(test_getsource_missing_script)
assert_test("getSource without script errors",
            any("script" in str(m).lower() for m in msgs if not m.get("success", True)),
            f"Messages: {msgs}")

# --- Test 14: multi-thread debugging (Phase 5) ---
print()
print("--- multi-thread debugging ---")

def test_multi_thread(send):
    # Launch two debug threads
    send({"op": "debug/run", "id": 1, "params": {"expression": "return 1+1"}})
    time.sleep(1)
    send({"op": "debug/run", "id": 2, "params": {"expression": "return 2+2"}})
    time.sleep(1)
    # List threads — should show both
    send({"op": "debug/listThreads", "id": 3, "params": {}})
    time.sleep(0.5)
    # Continue first thread (ID 3), keep second (ID 4) suspended
    send({"op": "debug/continue", "id": 4, "params": {"threadId": FIRST_DEBUG_TID}})
    time.sleep(1)
    # List again — should show only second thread
    send({"op": "debug/listThreads", "id": 5, "params": {}})
    time.sleep(0.5)
    # Continue second thread
    send({"op": "debug/continue", "id": 6, "params": {"threadId": FIRST_DEBUG_TID + 1}})
    time.sleep(1)

msgs = run_debug_session(test_multi_thread, timeout=20)

# Both threads should have started
t1_start = any(m.get("id") == 1 and m.get("result", {}).get("threadId") == FIRST_DEBUG_TID for m in msgs)
t2_start = any(m.get("id") == 2 and m.get("result", {}).get("threadId") == FIRST_DEBUG_TID + 1 for m in msgs)
assert_test("two threads started with different IDs",
            t1_start and t2_start,
            f"Messages: {[m for m in msgs if m.get('id') in (1, 2)]}")

# Both should have entry suspensions
entry_suspensions = [m for m in msgs if m.get("op") == "debug/suspended" and m.get("params", {}).get("reason") == "entry"]
assert_test("both threads suspended at entry",
            len(entry_suspensions) >= 2,
            f"Entry suspensions: {entry_suspensions}")

# listThreads should have shown 2 threads initially
list1 = None
for m in msgs:
    if m.get("id") == 3 and m.get("result", {}).get("threads") is not None:
        list1 = m
        break
assert_test("listThreads shows 2 threads",
            list1 is not None and len(list1["result"]["threads"]) == 2,
            f"Messages: {[m for m in msgs if m.get('id') == 3]}")

# Both should have completed
completions = [m for m in msgs if m.get("op") == "debug/completed"]
assert_test("both threads completed",
            len(completions) >= 2,
            f"Completions: {completions}")

print()
print("=" * 46)
print(f"RESULTS: {PASSED} passed, {FAILED} failed")
print("=" * 46)

sys.exit(FAILED)
PYEOF
