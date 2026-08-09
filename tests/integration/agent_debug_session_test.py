#!/usr/bin/env python3
#
# Unit 1.4 (#813 family) -- agent-driven debugging end-to-end.
#
# Drives frontier-cli --protocol through one COMPLETE debug workflow, the
# way an AI agent following docs/AGENT_DEBUGGING_GUIDE.md would:
#
#   install fixture (script/eval + script.newScriptObject)
#     -> debug/getSource (discover line numbers)
#     -> debug/setBreakpoint
#     -> invoke via thread.callScript (the Unit 1.3-verified lazy-attach path)
#     -> debug/suspended notification
#     -> debug/getStack + debug/getLocals (content asserts, not just success)
#     -> debug/step (line advance)
#     -> debug/continue -> second breakpoint -> inspect the seeded bug
#     -> debug/continue -> debug/completed
#     -> verify the script ran to completion (side effect readable via eval)
#
# The fixture seeds an off-by-one bug: u14SumTo sums 1..items-1 instead of
# 1..items, so with items=4 it returns 6 where 10 is expected. The test
# asserts the debugger EVIDENCE of that bug (locals at the return line show
# items=4, total=6), which is what an agent diagnoses from.
#
# In-frame expression evaluation: the 22-op dispatch table has no
# debug/evaluate op. script/eval while a thread is suspended runs in the
# REPL context and does NOT see the suspended frame's locals. That gap is
# asserted below (defined(total) is false mid-suspension) so this test
# flags the day an in-frame eval op changes the behavior.
#
# Invoked by tests/agent_debug_session_test.sh, which stages Virgin.root
# into a tmpdir and exports DEBUG_TEST_CLI / DEBUG_TEST_DB. No PTY needed:
# --protocol speaks NDJSON over plain pipes. No ports, no REPL.

import subprocess, json, sys, os, threading, time

CLI = os.environ["DEBUG_TEST_CLI"]
DB = os.environ["DEBUG_TEST_DB"]

PASSED = 0
FAILED = 0


class DebugSession:
    """frontier-cli --protocol session with a background reader thread.
    Same event-driven pattern as tests/debug_protocol_test.sh."""

    def __init__(self, timeout=15):
        self.timeout = timeout
        self.messages = []
        self._lock = threading.Lock()
        self._proc = subprocess.Popen(
            [CLI, "--protocol", "--skip-startup", "--system-root", DB],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            text=True, bufsize=1
        )
        self._reader = threading.Thread(target=self._read_loop, daemon=True)
        self._reader.start()

    def _read_loop(self):
        try:
            for line in self._proc.stdout:
                line = line.strip()
                if not line:
                    continue
                try:
                    msg = json.loads(line)
                    with self._lock:
                        self.messages.append(msg)
                except json.JSONDecodeError:
                    pass
        except (ValueError, OSError):
            pass

    def send(self, msg):
        if isinstance(msg, dict):
            msg = json.dumps(msg)
        try:
            self._proc.stdin.write(msg + "\n")
            self._proc.stdin.flush()
        except (BrokenPipeError, OSError):
            pass

    def wait_for(self, predicate, timeout=None):
        if timeout is None:
            timeout = self.timeout
        deadline = time.monotonic() + timeout
        checked = 0
        while True:
            with self._lock:
                for i in range(checked, len(self.messages)):
                    if predicate(self.messages[i]):
                        return self.messages[i]
                checked = len(self.messages)
            if deadline - time.monotonic() <= 0:
                return None
            time.sleep(0.03)

    def send_and_wait(self, msg, timeout=None):
        match_id = msg.get("id")
        self.send(msg)
        return self.wait_for(lambda m: m.get("id") == match_id, timeout=timeout)

    def wait_notification(self, op=None, reason=None, timeout=None):
        def pred(m):
            if m.get("id") is not None:
                return False
            if op is not None and m.get("op") != op:
                return False
            if reason is not None and m.get("params", {}).get("reason") != reason:
                return False
            return True
        return self.wait_for(pred, timeout=timeout)

    def close(self):
        try:
            self.send({"op": "shutdown", "id": 9999})
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


def assert_test(name, passed, detail=""):
    global PASSED, FAILED
    if passed:
        print("  PASS: " + name)
        PASSED += 1
    else:
        print("  FAIL: " + name)
        if detail:
            print("    " + str(detail))
        FAILED += 1


def eval_ok(s, rid, expression):
    """script/eval helper; returns the response."""
    return s.send_and_wait({"op": "script/eval", "id": rid,
                            "params": {"expression": expression}})


# Fixture: multi-line source passed with UserTalk-level \r (line separator)
# and \t (outline indentation) escape sequences -- literal control characters
# inside a UserTalk string literal are a parse error. script.newScriptObject
# builds the outline structure from the indentation.
SUM_SRC = ("local (items = 4)\\r"
           "local (total = 0)\\r"
           "local (i)\\r"
           "for i = 1 to items - 1 {\\r"
           "\\ttotal = total + i}\\r"
           "return (total)")
MAIN_SRC = ("local (result = 0)\\r"
            "result = system.temp.u14SumTo()\\r"
            "system.temp.u14Result = result\\r"
            "return (result)")

print("=" * 46)
print("Agent Debug Session E2E Test (Unit 1.4)")
print("=" * 46)
print()

with DebugSession(timeout=20) as s:
    # --- Phase 1: install the seeded-bug fixture over the protocol ---
    print("--- fixture install ---")
    r = eval_ok(s, 1, "new(scriptType, @system.temp.u14SumTo)")
    assert_test("create u14SumTo script object",
                r is not None and r.get("success") is True, r)
    r = eval_ok(s, 2, 'script.newScriptObject("' + SUM_SRC + '", @system.temp.u14SumTo)')
    assert_test("install u14SumTo source",
                r is not None and r.get("success") is True, r)
    r = eval_ok(s, 3, "new(scriptType, @system.temp.u14Main)")
    assert_test("create u14Main script object",
                r is not None and r.get("success") is True, r)
    r = eval_ok(s, 4, 'script.newScriptObject("' + MAIN_SRC + '", @system.temp.u14Main)')
    assert_test("install u14Main source",
                r is not None and r.get("success") is True, r)

    # --- Phase 2: discover line numbers via debug/getSource ---
    print()
    print("--- getSource line discovery ---")
    r = s.send_and_wait({"op": "debug/getSource", "id": 5,
                         "params": {"script": "system.temp.u14SumTo"}})
    lines = (r or {}).get("result", {}).get("lines", [])
    assert_test("getSource returns 6 lines",
                len(lines) == 6, r)
    line_text = {l.get("num"): l.get("text", "") for l in lines}
    assert_test("line 4 is the buggy loop header",
                "for i = 1 to items - 1" in line_text.get(4, ""), line_text)
    assert_test("line 6 is the return line",
                "return" in line_text.get(6, ""), line_text)

    # --- Phase 3: breakpoint on line 1, launch via thread.callScript ---
    print()
    print("--- breakpoint + thread.callScript launch ---")
    r = s.send_and_wait({"op": "debug/setBreakpoint", "id": 6,
                         "params": {"script": "system.temp.u14SumTo", "line": 1}})
    assert_test("setBreakpoint line 1 action=set",
                r is not None and r.get("result", {}).get("action") == "set", r)

    r = eval_ok(s, 7, "thread.callScript(@system.temp.u14Main, {})")
    assert_test("thread.callScript dispatch accepted",
                r is not None and r.get("success") is True, r)

    sus = s.wait_notification(op="debug/suspended", reason="breakpoint", timeout=10)
    assert_test("callScript thread suspends at breakpoint",
                sus is not None,
                [m for m in s.messages if m.get("op") == "debug/suspended"])
    tid = None
    if sus:
        p = sus.get("params", {})
        tid = p.get("threadId")
        assert_test("suspended.script identifies the callee",
                    p.get("script") == "system.temp.u14SumTo", p)
        assert_test("suspended.line == 1", p.get("line") == 1, p)
    if tid is None:
        print("FATAL: no threadId; cannot continue")
        sys.exit(1)
    tid = int(tid)

    # --- Phase 4: stack + locals at the entry breakpoint ---
    print()
    print("--- inspection at line 1 ---")
    r = s.send_and_wait({"op": "debug/getStack", "id": 8, "params": {"threadId": tid}})
    frames = (r or {}).get("result", {}).get("frames", [])
    assert_test("getStack innermost frame is u14SumTo at line 1",
                len(frames) >= 1 and frames[-1].get("script") == "system.temp.u14SumTo"
                and frames[-1].get("line") == 1, frames)
    # Caller frame: u14Main called u14SumTo, so the stack must show it.
    # The TLS script stack records caller frames even before the thread is
    # lazily attached; the attach path seeds the debug state from it.
    assert_test("getStack includes the u14Main caller frame",
                any(f.get("script") == "system.temp.u14Main" for f in frames), frames)

    # --- Phase 5: step advances past non-steppable local declarations ---
    print()
    print("--- step ---")
    r = s.send_and_wait({"op": "debug/step", "id": 9, "params": {"threadId": tid}})
    assert_test("step accepted (status=stepping)",
                r is not None and r.get("result", {}).get("status") == "stepping", r)
    st = s.wait_notification(op="debug/suspended", reason="step", timeout=10)
    assert_test("step suspension arrives",
                st is not None,
                [m for m in s.messages if m.get("op") == "debug/suspended"])
    if st:
        # Lines 1-3 are local declarations (non-steppable infrastructure
        # nodes); one step from line 1 lands on line 4, the loop header.
        assert_test("step advances line 1 -> line 4 (locals are non-steppable)",
                    st.get("params", {}).get("line") == 4, st)

    # --- Phase 6: run to the return line, inspect the seeded bug ---
    print()
    print("--- breakpoint at return line + bug evidence ---")
    r = s.send_and_wait({"op": "debug/setBreakpoint", "id": 10,
                         "params": {"script": "system.temp.u14SumTo", "line": 6}})
    assert_test("setBreakpoint line 6 action=set",
                r is not None and r.get("result", {}).get("action") == "set", r)
    r = s.send_and_wait({"op": "debug/continue", "id": 11, "params": {"threadId": tid}})
    assert_test("continue accepted (status=running)",
                r is not None and r.get("result", {}).get("status") == "running", r)
    sus6 = s.wait_for(lambda m: m.get("id") is None and m.get("op") == "debug/suspended"
                      and m.get("params", {}).get("line") == 6, timeout=10)
    assert_test("suspends at line 6 breakpoint",
                sus6 is not None and sus6.get("params", {}).get("reason") == "breakpoint",
                [m for m in s.messages if m.get("op") == "debug/suspended"])

    r = s.send_and_wait({"op": "debug/getLocals", "id": 12, "params": {"threadId": tid}})
    locals_list = (r or {}).get("result", {}).get("locals", [])
    by_name = {l.get("name"): l for l in locals_list}
    assert_test("getLocals sees the frame's variables (items, total, i)",
                all(n in by_name for n in ("items", "total", "i")), locals_list)
    # THE BUG EVIDENCE: items=4 but total=6 at the return line (sum of 1..3;
    # a correct sumTo(4) would show 10). This is what an agent diagnoses from.
    assert_test("locals show items=4",
                by_name.get("items", {}).get("value") == "4", by_name.get("items"))
    assert_test("locals show total=6 (the off-by-one evidence)",
                by_name.get("total", {}).get("value") == "6", by_name.get("total"))

    r = s.send_and_wait({"op": "debug/getStack", "id": 13, "params": {"threadId": tid}})
    frames = (r or {}).get("result", {}).get("frames", [])
    assert_test("getStack at line 6 still shows caller + callee",
                len(frames) >= 2 and frames[-1].get("script") == "system.temp.u14SumTo"
                and frames[-1].get("line") == 6
                and any(f.get("script") == "system.temp.u14Main" for f in frames),
                frames)

    # getSource with threadId overlays the current line + breakpoint flags
    r = s.send_and_wait({"op": "debug/getSource", "id": 14,
                         "params": {"script": "system.temp.u14SumTo", "threadId": tid}})
    res = (r or {}).get("result", {})
    slines = res.get("lines", [])
    assert_test("getSource currentLine == 6",
                res.get("currentLine") == 6, res)
    assert_test("getSource marks breakpoints on lines 1 and 6",
                [l.get("num") for l in slines if l.get("breakpoint")] == [1, 6], slines)

    # --- Phase 7: in-frame eval gap (no debug/evaluate op exists) ---
    # script/eval runs in the REPL context, NOT the suspended frame, so the
    # frame's locals are invisible to it. Documented in
    # docs/AGENT_DEBUGGING_GUIDE.md; this assert flags the day that changes.
    print()
    print("--- in-frame eval gap ---")
    r = eval_ok(s, 15, "defined(total)")
    assert_test("KNOWN GAP: script/eval cannot see frame locals while suspended",
                r is not None and r.get("result", {}).get("value") == "false", r)

    # --- Phase 8: run to completion, verify the script finished ---
    print()
    print("--- completion ---")
    r = s.send_and_wait({"op": "debug/clearBreakpoints", "id": 16, "params": {}})
    assert_test("clearBreakpoints cleared 2",
                r is not None and r.get("result", {}).get("cleared") == 2, r)
    r = s.send_and_wait({"op": "debug/continue", "id": 17, "params": {"threadId": tid}})
    assert_test("final continue accepted",
                r is not None and r.get("result", {}).get("status") == "running", r)
    done = s.wait_notification(op="debug/completed", timeout=10)
    assert_test("debug/completed with success=true",
                done is not None and done.get("params", {}).get("success") is True, done)

    r = eval_ok(s, 18, "system.temp.u14Result")
    assert_test("script ran to completion: u14Result == 6 (buggy sum persisted)",
                r is not None and r.get("result", {}).get("value") == "6", r)

    # --- Cleanup (staged DB is discarded by the wrapper; in-memory only) ---
    eval_ok(s, 19, "delete(@system.temp.u14SumTo)")
    eval_ok(s, 20, "delete(@system.temp.u14Main)")
    eval_ok(s, 21, "delete(@system.temp.u14Result)")

print()
print("=" * 46)
print("RESULTS: %d passed, %d failed" % (PASSED, FAILED))
print("=" * 46)
sys.exit(1 if FAILED else 0)
