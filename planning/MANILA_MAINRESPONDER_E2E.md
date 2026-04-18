# Manila + mainResponder E2E — Autonomous Workflow Plan & Tracker

Status
- State: In Progress
- Phase: Phase A (inetd E2E Foundation) — not started
- Last Updated: 2026-04-17
- Owner: Jake + autonomous agents (/doit, /auto)
- Last Reviewed: 2026-04-17
- Notes: Living document. Update the "Status Snapshot" and "Change Log" sections after each work session. Survives context compaction — start a new session by reading this file.

Related Docs
- `planning/_CURRENT_TODO_LIST.md` — broader roadmap
- `planning/architectural_decision_records/FWSNETEVENT_TCP_MIGRATION.md` — #364 listener race
- `planning/phase3/processor_audits/webserver.md` — webserver verb inventory
- `docs/ARCHITECTURAL_ANTIPATTERNS.md` — context guard / mode stack landmines
- `docs/AI_SHARED_GUIDELINES.md` — autonomous-agent discipline
- `tests/integration/test_cases/webserver_hello_world.yaml` — existing webserver tests (many skipped)
- `tests/integration/test_cases/webserver_http_roundtrip_tests.yaml` — TCP-layer HTTP tests (green)
- `databases/Guest Databases/apps/mainResponder.root` — 2.5 MB, responder source of truth
- `databases/Guest Databases/apps/manila.root` — 21 MB, Manila CMS source of truth

Change Log
- 2026-04-17: Initial document created after /plan session. Baseline: 2209 integration tests, 0 failures; PRs #538/#539/#540 merged.
- 2026-04-17: Phase A — added `tests/integration/test_cases/webserver_inetd_e2e.yaml` (4 tests, all passing) exercising full inetd → webserver.server → webserver.dispatch → custom responder flow. Integration suite: 2213 total, 0 failures. Unit suite: 302 passed. PR #541 opened against develop.

---

## Status Snapshot

Update this table after every session. Keep phases in order — don't start N+1 until N is merged.

| Phase | Title | State | PR | Notes |
|-------|-------|-------|----|----|
| A | inetd E2E Foundation | In review | #541 | 4 tests passing; 2213 total, 0 failures |
| B | mainResponder Dispatch Smoke Test | Not started | — | Depends on A |
| C | inetd + mainResponder Integration | Not started | — | Depends on B |
| D | Manila Installation + First Page | Not started | — | Human-led |
| E | Concurrent Load Safety Audit | Not started | — | Human-led |
| F | Manila Feature Parity | Deferred | — | Post-launch |

**Current blocker:** Phase A PR awaiting user merge approval.

**Next action:** Once Phase A merges, kick off Phase B (mainResponder Dispatch Smoke Test).

---

## Context

**Where we are (April 2026):** With PRs #538/#539/#540 merged, all P0 architectural launch blockers are resolved, the integration suite is at 0 failures (2209 total), and the infrastructure for safe autonomous work is in place. TCP (23/23 verbs), webserver kernel verbs (7/7), and `inetd.supervisor` are all implemented. `mainResponder.root` (2.5 MB) and `manila.root` (21 MB) exist on disk with full UserTalk script trees.

**Where we want to go:** Serve a real Manila blog over HTTP from `frontier-cli`, with an autonomous workflow managing as much of the gap-closing as possible — without a human catching regressions after they ship.

**The "last 10% is 90% of the work" problem:** The unknowns aren't in the verb layer anymore — that's mature and well-tested. They're in the *integration seams*: startup sequencing, responder dispatch wiring, GIL interactions under concurrent HTTP load, guest-DB context guards, and visual/behavioral correctness of rendered HTML. These are exactly the places where autonomous workflows can produce green CI yet a broken system. This plan keeps autonomy where it's safe and inserts human touchpoints exactly where bots and agent-browser can't substitute for judgment.

---

## Current State Summary

**Works today (tested, green):**
- TCP round-trip tests — real HTTP request/response over sockets (`webserver_http_roundtrip_tests.yaml`)
- Webserver kernel verbs — `init`, `parseHeaders`, `parseCookies`, `buildResponse`, `dispatch` exist
- Guest DB open + script eval — tested in `guest_db_externals.yaml`
- mainResponder source code — full script tree in `usertalk_scripts/mainResponder.root/mainResponder/` including `respond.ut` (~60 KB)

**Works but untested E2E:**
- Full `inetd.supervisor → webserver.server → webserver.dispatch → responder` flow
- `mainResponder.respond()` execution against a real HTTP request
- Guest DB context switching under HTTP load

**Unknown / untested:**
- Manila installation in headless mode (`config.mainResponder.domains`, `config.manila.sites`)
- HTTP keep-alive, concurrent request handling under the GIL
- Visual correctness of Manila-rendered HTML

**Known risk zones** (from architectural review):
- GIL callback safety (ADR-005 parameter-state TLS migration incomplete)
- `db_context_guard` completeness for guest DB isolation (ADR-009)
- `tcp_close_listen()` race condition (issue #364)
- Mode stack push/pop patterns under cooperative yields

---

## Autonomy Boundary — What to Automate vs. What Stays Manual

### 🟢 Safe for `/auto` (autonomous merge on green)
- Individual HTTP verb improvements with existing test coverage
- HTTP status code additions, header parsers, response builders
- Test-infrastructure changes (new YAML flags, helpers)
- Documentation and ADR updates
- Bug fixes where the failing test is already written or trivially writable

### 🟡 `/doit` with user approval at merge (autonomous plan/implement/review, human gates merge)
- New integration tests that exercise inetd end-to-end
- Responder-dispatch changes (test responder registration, mainResponder wiring)
- Manila startup script authoring
- Any change touching context guards, GIL handoff, or thread-local state

### 🔴 Human-required (not autonomous at all)
- **Manila visual correctness** — does a rendered Manila page actually look right? Agent-browser is unreliable past 3–4 steps.
- **Load testing correctness** — `ab -c 10 -n 1000` behavior under concurrent requests. Silent corruption won't show up in green CI.
- **Architectural decisions** — "should Manila install at startup or on-demand?", responder API design, guest DB persistence strategy.
- **Data-loss verification** — after a sequence of HTTP writes, did the right guest DB end up with the right state? Requires human inspection.
- **Final merge approval for each phase** — per the user's hard rule: never merge without explicit approval.

---

## Phased Approach

Each phase is a self-contained `/doit` or `/auto` unit with its own PR, its own human touchpoint, and a clear exit criterion. Phases land in order — later phases depend on earlier ones.

### Phase A: inetd E2E Foundation

- **Workflow:** `/doit` (autonomous plan/implement, human merge)
- **State:** Not started
- **PR:** —

**Why first:** Everything downstream depends on an HTTP request making it through the full stack. Today this is only tested at the TCP layer. Without a reliable E2E test, we can't tell whether mainResponder regressions are our fault or pre-existing.

**Scope:**
1. Write `tests/integration/test_cases/webserver_inetd_e2e.yaml` that:
   - Starts an inetd listener on an OS-assigned ephemeral port
   - Registers a trivial `helloResponder` in `user.webserver.responders`
   - Makes a real HTTP GET via `tcp.openStream` to the listener
   - Asserts response body, status, and Content-Type
   - Shuts down the listener cleanly (work around #364 via explicit sleep or retry)
   - Uses `sequential: true` and `needs_guest_dbs: false`
2. Fix any bugs the test exposes.
3. Do **not** attempt to fix #364 (listener shutdown race) — document and work around.

**Exit criteria:**
- [ ] New YAML test passes in isolation and in the full suite
- [ ] Zero integration test regressions
- [ ] PR merged with user approval

**Human touchpoint:** Review the test design (is it testing the right thing?); approve merge.

**Autonomy risk:** LOW. Scoped, test-first, builds on existing green infrastructure.

---

### Phase B: mainResponder Dispatch Smoke Test

- **Workflow:** `/doit` (autonomous plan/implement, human merge + gap triage)
- **State:** Not started
- **PR:** —

**Why:** `mainResponder.respond` is ~60 KB of legacy UserTalk. We need to know whether it *compiles and runs at all* in the headless runtime before investing in full Manila integration.

**Scope:**
1. Write `mainresponder_smoke.yaml` that:
   - Opens `mainResponder.root` via `fileMenu.open`
   - Constructs a minimal fake HTTP request table (`adr`, `verb="GET"`, `path="/"`, headers, etc.)
   - Calls `mainResponder.respond(request)` directly (bypassing inetd/webserver)
   - Asserts: returns a table with `bodytext` and `headers`, doesn't throw, sets `responseCode`
   - Uses `needs_guest_dbs: true` and `sequential: true`
2. Expect failures — surface missing verbs, missing runtime state (`config.mainResponder.*`), or unsupported UserTalk constructs.
3. For each failure: decide per-issue whether to fix in scope or file as follow-up GitHub issue.
4. Do **not** attempt to serve real content yet — just "does the entry point work?"

**Exit criteria:**
- [ ] Test runs to completion (passing or cleanly documenting why specific features aren't supported)
- [ ] Any discovered bugs either fixed or filed as GitHub issues with reproducer
- [ ] PR merged with user approval

**Human touchpoint:** Review discovered gap list and decide scope — some will be P2 follow-ups, others P0 blockers.

**Autonomy risk:** MEDIUM. Unknowns in legacy UserTalk are high. Build in a stop-and-ask checkpoint after the test first runs — don't let an agent burn context chasing 20 missing verbs.

---

### Phase C: inetd + mainResponder Integration

- **Workflow:** `/doit` (autonomous plan/implement, human merge + manual smoke)
- **State:** Not started
- **PR:** —

**Why:** Wire Phase A and Phase B together. HTTP request arrives over TCP, inetd dispatches, webserver parses, mainResponder generates response.

**Scope:**
1. Write `mainresponder_http_e2e.yaml`:
   - Start inetd listener
   - Configure `user.webserver.responders.default = @mainResponder.respond` (or equivalent)
   - Send real HTTP GET to listener
   - Assert HTTP response came from mainResponder (distinguishing header or body signature)
2. Fix any wiring gaps (headers not passed through, responder not found, etc.).
3. Document wiring in `docs/WEBSERVER_INTEGRATION.md`.

**Exit criteria:**
- [ ] Real HTTP request returns a real mainResponder-generated response
- [ ] Test passes in CI
- [ ] Manual smoke: `curl http://localhost:<port>/` returns expected HTML
- [ ] PR merged with user approval

**Human touchpoint:**
- Review PR
- **Manual `curl` smoke test before merge** — autonomous test alone is not sufficient here
- Approve merge

**Autonomy risk:** MEDIUM-HIGH. "Green CI but broken system" risk peaks here.

---

### Phase D: Manila Installation + First Page

- **Workflow:** Human-led, agent-assisted (multiple `/doit` sub-tasks)
- **State:** Not started
- **PR:** —

**Why:** Manila is a complete CMS with startup dependencies, site configuration, and template rendering. This is where the "last 10%" problem manifests.

**Scope:**
1. Read Manila's `init`, `startup`, and site-installation scripts to understand boot expectations.
2. Author `databases/scripts/installManila.ut` — a headless installation script that:
   - Opens `manila.root` as a guest DB
   - Registers a test site in `config.manila.sites`
   - Points a subdomain/path in `config.mainResponder.domains` at the test site
3. Write integration test: install Manila, hit `http://localhost:<port>/testSite/`, assert non-error response.
4. **Do not attempt:** admin panel, posting, comments, RSS, user auth. Just "render one page".

**Exit criteria:**
- [ ] `curl http://localhost:<port>/testSite/` returns 200 with recognizable Manila HTML
- [ ] Integration test captures this and runs green
- [ ] No data corruption in `manila.root` (verify with `git diff`)
- [ ] Human browser inspection passes

**Human touchpoints (multiple):**
- Review Manila source code together before agent implements
- Review install-script design before writing tests
- Manual `curl` + browser inspection of rendered HTML
- Decide scope: which Manila features are in/out

**Autonomy risk:** HIGH. Explicitly human-led. Agents assist; architectural and scoping decisions stay with the user.

---

### Phase E: Concurrent Load Safety Audit

- **Workflow:** Human-led, with `/doit` follow-ups per issue
- **State:** Not started
- **PR:** —

**Why:** Up to this point we've only tested sequential requests. Manila under real use sees concurrent requests. GIL + incomplete ADR-005 + unaudited context guards mean race conditions could silently corrupt state.

**Scope:**
1. Write a load-test harness (shell script using `ab` or Python).
2. Run `ab -n 1000 -c 10` against the Phase D Manila site.
3. Monitor: response correctness, latency distribution, any 500s, any silent content corruption.
4. If issues: file `/doit` tasks per issue with clear reproducer.

**Exit criteria:**
- [ ] Zero corruption detected across 1000 requests at concurrency 10
- [ ] Latency distribution documented
- [ ] Any issues filed as GitHub issues with reproducer
- [ ] Go/no-go launch decision made

**Human touchpoints:**
- Design load test (what's "correct" output? how verify?)
- Inspect results for anomalies
- Decide: ship as-is with documented limits, or block on audit fixes

**Autonomy risk:** Cannot be autonomous. Result interpretation is judgment-driven.

---

### Phase F: Manila Feature Parity (deferred)

Once A–E are green and load-verified, individual Manila features (admin panel, posting flow, RSS, etc.) become good `/auto` candidates because:
- Test patterns exist
- E2E wiring is proven
- Load safety is established
- Each feature is scoped and testable

Not planning those here — post-launch roadmap.

---

## Risk Register

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| Agent ships "green CI, broken system" | HIGH | HIGH | Manual curl/browser smoke at each HTTP phase gate |
| Data corruption under concurrent load | MEDIUM | CRITICAL | Phase E load audit before launch; don't skip |
| Agent burns context chasing legacy UserTalk bugs in Phase B | HIGH | MEDIUM | Stop-and-ask checkpoint after first test run |
| #364 listener race surfaces under load | MEDIUM | MEDIUM | Documented workaround in Phase A; full fix is separate workstream |
| Manila expects unimplemented capabilities (string/xml verbs) | HIGH | MEDIUM | Phase D starts with source read + scoping, not implementation |
| Bot misses subtle GIL bug in review | MEDIUM | HIGH | `/gate` concurrency-pro; human review for callback-touching PRs |
| HTML renders wrong despite tests passing | MEDIUM | HIGH | Human browser inspection in Phases C, D |
| Agent-browser can't complete Manila flows (>3–4 steps) | CERTAIN | LOW | Accept as constraint; use curl + manual browser |

---

## Human Touchpoint Summary

Every phase requires human approval at merge (global hard rule). Beyond that:

| Phase | Explicit human action required |
|-------|---------------------------------|
| A | Review test design; approve merge |
| B | Triage gap list after smoke test; decide scope for follow-ups |
| C | Manual curl smoke test + response inspection before merge |
| D | Pre-implementation design review; browser inspection of rendered HTML; scope decisions |
| E | Load test design; result interpretation; launch go/no-go |

Estimated human engagement: ~1–2 hours per phase of focused review, plus async PR approvals. Total: ~5–10 hours of hands-on time across all five phases.

---

## Is This Risky?

**Yes, with qualifications.**

**Safe parts:**
- Test infrastructure mature (0 failures baseline, good isolation, YAML metadata)
- `/auto` merge-safety gate enforces all-green before auto-merge
- `/gate` review catches concurrency/security issues pre-PR
- Each phase has a clear exit criterion and scope boundary
- Nothing in this plan touches production data — feature branches in worktrees

**Risky parts:**
- Legacy UserTalk surfaces unknowns (drives Phase B pacing)
- Concurrent behavior can't be verified by test runners alone (Phase E)
- Manila "works" is a subjective threshold — needs human eyes
- Green CI ≠ working system when integration seams are the unknown

**Net:** The main risk is not "autonomous workflow breaks things" (safety nets catch that). It's "autonomous workflow produces a false sense of done." This plan inserts human checkpoints at exactly the seams where that false sense would otherwise form.

---

## Verification

**Per-phase:**
- A: `webserver_inetd_e2e.yaml` passes + no regressions in full suite
- B: `mainresponder_smoke.yaml` runs to completion, gap list filed
- C: `curl http://localhost:<port>/` returns mainResponder HTML + integration test green
- D: `curl http://localhost:<port>/testSite/` renders Manila HTML + browser inspection passes
- E: `ab -c 10 -n 1000` reports zero corruption and stable latency

**Overall success:**
- Real browser hits a Frontier-served Manila site and displays a blog post correctly
- Load test shows no corruption or crashes under 10× concurrency
- No data-loss incidents in guest databases across the run
- Zero integration test regressions baseline-to-final

---

## Session Resume Protocol

When resuming this work in a new session (including post-compaction):

1. Read this file top to bottom (especially Status Snapshot and Change Log).
2. Check PR state for the current phase: `gh pr view <PR> --json state,mergeStateStatus,reviews`.
3. Check test baseline: `cd tests && make test-integration` should show 0 failures (or document drift).
4. Pick up at the "Next action" listed in Status Snapshot.
5. After any meaningful progress (commit, PR, merge, decision), update Status Snapshot + Change Log in this file and commit.

---

## Kickoff Prompts (Ready to Paste)

### Phase A kickoff

```
/doit
Execute Phase A of planning/MANILA_MAINRESPONDER_E2E.md: inetd E2E Foundation.

Scope:
- Write tests/integration/test_cases/webserver_inetd_e2e.yaml that starts an inetd listener on an OS-assigned port, registers a trivial helloResponder, makes a real HTTP GET via tcp.openStream, asserts body/status/Content-Type, and shuts down cleanly.
- File metadata: sequential: true, needs_guest_dbs: false.
- Work around issue #364 (listener shutdown race) with explicit sleep or retry — do NOT try to fix the race itself.
- Fix any bugs the test exposes.

Exit criteria: test passes in isolation and in the full suite; zero integration regressions; PR merged with user approval.

Constraints:
- Test-first. Run both unit and integration suites before push.
- Do not merge without explicit user approval (global hard rule).
- Update planning/MANILA_MAINRESPONDER_E2E.md Status Snapshot + Change Log as part of the PR.
```

### Phase B kickoff (only after A merges)

```
/doit
Execute Phase B of planning/MANILA_MAINRESPONDER_E2E.md: mainResponder Dispatch Smoke Test.

Read the plan file for full scope. Stop and ask after the test first runs — do not burn context fixing long lists of missing UserTalk constructs. Surface the gap list to the user for triage.
```

(Later phases documented in the plan; do not kick off until predecessors are merged.)
