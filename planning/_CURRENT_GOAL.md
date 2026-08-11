# Current Goal

Status
- State: In Progress
- Phase: 3 (Webserver / mainResponder / Manila)
- Last Updated: 2026-08-10
- Notes: The standing objective autonomous work is measured against. Update at milestones; history moves to the log at the bottom. Read this before starting non-trivial work.

Related Docs
- planning/INDEX.md
- planning/phase_overview.md
- planning/phase3/SESSION_REPORT_2026-08-10_webserver_milestones.md

## The Goal

**A fresh clone builds, boots a virgin root, completes first-run unattended, and serves a Manila site in a browser — verified by agent-browser, visually signed off by JES.**

## Done means

1. `make build` from a fresh clone succeeds (documented toolchain only).
2. First launch on the shipped Virgin.root runs the complete first-run flow with no hand-provisioning and no errors that strand startup (`startingUp` ends false; `user.prefs.firstRootRun` ships true and flips false).
3. The setup page is reachable at its published URL — including after a restart — and completes setup.
4. mainResponder and Manila are installed by that flow; a Manila site renders in a real browser.
5. Verification is two-layer: agent-browser drives and checks the flow end-to-end; JES visually signs off the rendered site (held decision — never autonomous).
6. The integration suite is green with all of the above enabled (no skips or baseline entries hiding the path).

## Current wave (in dependency order)

| # | Item | Why it gates |
|---|------|--------------|
| 1 | #866 newScriptObject reinstall corruption (P1, C-side) | Blocks every further large-script UserTalk edit |
| 2 | #859 pathString stale under --skip-startup | Root-causes #855's failures; poisons the whole test/protocol surface |
| 3 | #867 + #870 setup-page pair (flag lifetime; render 500) | The setup page is criteria 3 |
| 4 | #855 firstRootRun=true with green suite | Criteria 2; the 13-failure set is the acceptance test |
| 5 | Manila install wave | Criteria 4-5; baseline from the Frontier 9.5 package (#868) |

## Held decisions (JES only)

- #868 — authoritative mainResponder.root/Manila.root (lead: Frontier 9.5 .dmg contains the legacy virgin copies)
- #853 — default bind address (deferred to Manila/public-serving phase)
- Manila visual sign-off (criteria 5) — always held
- Fetching from userland.com (rate-limit recovery — coordinate first)

## Goal log

- 2026-08-10: Initial goal set (JES + session f8). Phase 3 at ~2/3: webserver serves pages E2E (#856), startupScript wedge fixed (#858), mainResponder serves real pages via shipped chain (#863), startup guards + honest 500s (#875). Road fully mapped in issues #848-#874.
