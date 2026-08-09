# Unit 1.4 — Live Agent Debug Validation

**Date:** 2026-08-09
**Method:** A Claude agent executed a complete debug session against a
seeded-bug fixture by following `docs/AGENT_DEBUGGING_GUIDE.md` only — the
op sequences below were composed from the guide's sections (cited inline),
not from the implementer's working memory. The diagnosis was made from
locals/stack values, not from reading the fixture source. All transcripts
are verbatim from live `frontier-cli --protocol` sessions against a staged
`Virgin.root`.

**Verdict:** the guide was sufficient to drive install, breakpoint,
lazy-attach launch, inspection, stepping, and completion without tribal
knowledge — and the validation surfaced one real deficiency (a post-debug
eval poisoning bug, previously flagged only as a shared-executor note in
Unit 1.1's contract tests). The guide and the E2E test were amended; the
amended mitigations were then re-validated live. That is the acceptance
loop working as intended.

---

## 1. The seeded bug (declared by the seeder role)

`system.temp.u14Fact` computes 5-factorial with its multiplicative
accumulator seeded to 0 (`local (result = 0)` instead of `= 1`), called
from a `u14FactMain` wrapper that writes `system.temp.u14FactOut`. The
diagnoser role knew only: "u14FactOut should be 120 and is not; the code
runs via `thread.callScript`."

## 2. Debug session (guide section 10 recipe)

Line discovery (guide section 3), breakpoint on the loop-body line, then
breakpoints-first `thread.callScript` launch (guide section 4b):

```text
-> {"op": "debug/setBreakpoint", "id": 6, "params": {"script": "system.temp.u14Fact", "line": 5}}
<- {"id":6,"result":{"action":"set","script":"system.temp.u14Fact","line":5},"success":true}
-> {"op": "script/eval", "id": 7, "params": {"expression": "thread.callScript(@system.temp.u14FactMain, {})"}}
<- {"id":null,"op":"debug/suspended","params":{"threadId":3,"line":5,"reason":"breakpoint","script":"system.temp.u14Fact"}}
```

Stack confirms the call path (caller frame present on the lazily attached
thread), and three successive suspensions capture the state evolution
(guide sections 5 and 8; loop re-fire per section 6):

```text
<- {"id":8,"result":{"frames":[{"level":2,"script":"system.temp.u14FactMain"},{"level":3,"script":"system.temp.u14Fact","line":5}]},"success":true}
<- {"id":9,"result":{"locals":[...,{"name":"n","value":"5","type":"long"},{"name":"result","value":"0","type":"long"},{"name":"i","value":"2","type":"long"}],"script":"system.temp.u14Fact","line":5},"success":true}
<- {"id":11,"result":{"locals":[...,{"name":"result","value":"0","type":"long"},{"name":"i","value":"3","type":"long"}],...},"success":true}
<- {"id":13,"result":{"locals":[...,{"name":"result","value":"0","type":"long"},{"name":"i","value":"4","type":"long"}],...},"success":true}
```

One `debug/step` from the loop body landed on line 4 (the loop header) —
recorded as loop-stepping behavior and folded back into guide section 6.
After `debug/clearBreakpoints` + `debug/continue`: `debug/completed`
`success:true`, and the side-effect check (guide section 8) read
`u14FactOut = 0`.

## 3. Diagnosis (from values only)

| Observation | Expected (correct code) | Actual |
|---|---|---|
| At line 5 before first multiply | accumulator holds the multiplicative seed (1) | `result = 0` |
| After first multiply (i=3 suspension) | `result = 2` | `result = 0` |
| After second multiply (i=4 suspension) | `result = 6` | `result = 0` |
| Final output | 120 | 0 |

`i` advances normally (2, 3, 4 with `n=5`), so the loop bounds and
iteration are healthy. `result` is 0 *before the first multiplication* and
0 stays 0 under multiplication — the accumulator was initialized to the
additive identity (0) instead of the multiplicative identity (1).
**Root cause: accumulator seeded with 0; fix: initialize `result` to 1.**
Diagnosed without reading the fixture source: the initializer's value was
inferred from the first-iteration locals, and the absorbing-zero signature
from the iteration series.

## 4. Deficiency found: post-debug eval poisoning

The first fix attempt — re-installing the corrected source with
`script.newScriptObject` right after `debug/completed` — failed with a
generic `script_error` pointing inside `newScriptObject` itself:

```text
-> {"op": "script/eval", "id": 19, "params": {"expression": "script.newScriptObject(\"local (n = 5)\\rlocal (result = 1)\\r...\", @system.temp.u14Fact)"}}
<- {"id":19,"error":{"code":"script_error","message":"Script evaluation failed","location":{"script":"<eval>","line":12,"column":39,...},"stack":[...]},"success":false}
```

The guide (as first written) did not warn about this, and the naive
verification loop also waited on a `debug/completed` that never comes for
a breakpoint-less re-dispatch. Characterization runs established:

- Minimal reproducer (no evals while suspended): the first `new(...)` eval
  after a lazily attached thread completes fails once — 18/18 across
  0-500ms delays, on both the pre- and post-Unit-1.4 binaries (pre-existing;
  reproduced at `dc68b3187`).
- The failure consumes the poisoned state; the retry succeeds.
- Session-history-dependent: sessions that ran `script/eval` while
  suspended did not exhibit it; bare table reads and `delete` passed
  through unaffected.
- `script/clearContext` does not prevent it (3/3).
- Same latent thread-globals family as the shared-executor poisoning note
  in `protocol_contract_tests.yaml` (Unit 1.1); root cause still open.

**Amendments made:** guide section 8 now documents the failure signature,
the retry-once mitigation, and the poll-don't-wait rule for unobserved
re-dispatch; `tests/integration/agent_debug_session_test.py` pins the
minimal reproducer as a canary (so the eventual root-cause fix is flagged)
plus retry-tolerant cleanup.

## 5. Re-validation with the amended guide

Fresh session, same seeded bug: diagnose, then fix following the amended
section 8 — first post-debug eval fails as documented, retry once, then
re-dispatch unobserved and poll the side effect:

```text
-> {"op": "script/eval", "id": 10, "params": {"expression": "script.newScriptObject(\"...local (result = 1)...\", @system.temp.u14Fact)"}}
<- {"id":10,"error":{"code":"script_error","message":"Script evaluation failed",...,"success":false}
-> {"op": "script/eval", "id": 11, "params": {"expression": "script.newScriptObject(\"...local (result = 1)...\", @system.temp.u14Fact)"}}
<- {"id":11,"result":{"value":"true","type":"boolean"},"success":true}
-> {"op": "script/eval", "id": 12, "params": {"expression": "thread.callScript(@system.temp.u14FactMain, {})"}}
<- {"id":12,"result":{"value":"4","type":"long"},"success":true}
-> {"op": "script/eval", "id": 13, "params": {"expression": "system.temp.u14FactOut"}}
<- {"id":13,"result":{"value":"120","type":"long"},"success":true}
```

Bug diagnosed, fixed, and fix-verified end-to-end over the protocol.

## 6. Guide deficiencies found, and disposition

| # | Deficiency | Disposition |
|---|---|---|
| 1 | Post-debug eval poisoning undocumented; first fix attempt failed opaquely | Documented in guide section 8 with real transcript + retry-once mitigation; canary asserts added to the E2E test |
| 2 | Verifying a breakpoint-less re-dispatch by waiting on `debug/completed` hangs (notification only fires for attached threads) | Section 4b already stated threads run "unobserved" without a matching breakpoint, but the consequence for verification was easy to miss; section 8 now says poll the side effect explicitly |
| 3 | Loop stepping behavior (step from body lands on the loop header) unspecified | Observed live; consistent with section 6's statement-boundary model — no change needed beyond the section 6 expectations list |
