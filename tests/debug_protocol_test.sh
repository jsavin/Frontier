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

# --- Test 1: debug/run + debug/continue ---
print("--- debug/run + debug/continue ---")
# Note: debug threads get sequential IDs starting from 3 (main=2).
# Each test session starts fresh, so the first debug thread is always 3.
# If this assumption breaks, these tests will fail with "No debug thread"
# errors, which is a clear signal to update the thread ID.
FIRST_DEBUG_TID = 3

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

# --- Test 4: error cases ---
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

print()
print("=" * 46)
print(f"RESULTS: {PASSED} passed, {FAILED} failed")
print("=" * 46)

sys.exit(FAILED)
PYEOF
