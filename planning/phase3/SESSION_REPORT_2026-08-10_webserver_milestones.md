# Session Report: Phase 3 Webserver Milestones (2026-08-10)

Status
- State: Completed
- Phase: 3 (Webserver / mainResponder)
- Last Updated: 2026-08-10
- Notes: Autonomous session report; four units gated and merged. Source material for worknotes.

Related Docs
- planning/INDEX.md
- planning/phase_overview.md
- planning/PHASE3_RISK_FIRST_STRATEGY.md

Change Log
- 2026-08-10: Initial version (end-of-session report).

## Summary

Four units were built, gated, and merged in one session. develop moved `e7039b80c` -> `21d33d2fd`. The headline arc: the headless webserver went from "never served a page" to "serves real pages end-to-end through the shipped mainResponder dispatch chain, with honest error semantics and startup that cannot silently wedge." The integration suite was green (0 unexpected failures) and independently verified at every merge.

## Merges

| PR | Unit | What landed |
|----|------|-------------|
| #856 | 3.1 | First end-to-end HTTP page from the headless webserver. inetd classic-Mac multi-listener bug excised (#840). Virgin.root user-table machine-specific path pollution removed, delete-not-empty so `defined()` regeneration guards work (#841). Root cause of suite-only test failures fixed (windowTypes session leak). Baseline shrank 19 -> 17 (two "known failures" were this bug). |
| #858 | #848 fix | Headless `winv_ismenuscript` returned "not implemented" unconditionally, aborting `startupScript` whenever the window registry existed and permanently wedging the webserver (startingUp stuck true). Real root cause was in `tests/headless_window_verbs.c` (in the production build), not the file the issue named. |
| #863 | 3.3 | mainResponder serves real pages over real sockets through the SHIPPED dispatch chain -- no bridge was needed; Virgin.root already wires `responders.default.methods.any` to `mainResponder.respond`. Repaired a 4-month false-positive smoke test (error page with 200 status satisfied it). Un-skipped and fixed 2 roundtrip tests; 7 remain skipped (#710/#333 echo family). |
| #875 | 3.4 | Three try/else guards in `startupScript` (menus, `system.callbacks.startup`, `inetd.start`) so failures cannot strand `startingUp` (#849). mainResponder error pages now return HTTP 500 instead of 200 (#862), landed via a node-mutation workaround for #866. Plus a diagnosis corpus that scopes the next wave. |

## Verification discipline

Every unit went through the same gate: 2-3 specialist reviewers, both required test layers, and an independent orchestrator-run full suite before merge. Unit-test verdicts used per-binary failure-SET comparison against the #827/#828 baseline (the standard runner aborts at the first crasher and cannot produce a full verdict).

Notable catches the discipline made:

- A vacuous test green (mainResponder renders error pages WITH a 200 status; "200 + non-empty body" proves nothing). Caught twice, fixed with content-marker + error-signature assertions.
- `script.newScriptObject` silently corrupting a script on reinstall -- caught by a compile gate with an unrepairable negative control before a broken `mainResponder.respond` shipped (#866).
- Two agent self-retractions before wrong conclusions hardened: a wrong Defect-B root cause (#870, corrected within ~30 minutes of filing) and a wrong "close #859" recommendation (reopened with the --skip-startup evidence).

## Issue ledger

27 issues filed this session (#848-#874). Closed by merges: #848, #849, #862. Reopened on evidence: #859.

Key open items by theme:

- **Keystone**: #866 (P1) -- `script.newScriptObject` reinstall corrupts scripts (compile flips true->false on unmodified source; per-content, not size-dependent). Blocks all large-script UserTalk edits; single-line node mutation is the only validated workaround.
- **First-run/onboarding chain**: #867 (setup-page bypass flag lives in system.temp; page permanently 401 after restart) -> #870 (setupFrontier 500s even with the bypass; cause not yet isolated) -> #859 (Frontier.pathString stale dead-worktree value is LIVE under --skip-startup, the whole test/protocol surface) -> #855 (firstRootRun=false contract violation; 13-failure set is the acceptance test).
- **ODB integrity**: #854 (db.compactDatabase SIGABRTs and corrupts source in place; ~1.2MB save-rewrite growth per session quantified), #868 (mainResponder .ut mirror diverged from shipped binary -- mirror derives from a NEWER DB; authoritative-source decision needed), #869 (no verifier covers guest-DB mirrors), #861 (responders.default.methods.any has no .ut export).
- **Crash family**: #857/#864 -- both intermittent unit-test crashers share ONE stack (dbgeteof null deref via hashresolvevalue); #864 is a deterministic reproducer (system.macintosh table walk).
- **Robustness/observability P2s**: #871 (builtInError XSS + disclosure, pre-existing), #872 (unguarded early-startup hang window), #873 (startup guards log nowhere on stock installs), #874 (buildSuitesSubmenu target restore), #865 (openUrl fails to compile headless).
- **Test infra**: #852 (sequential files share one ProtocolExecutor session -- state-leak class), #846 (shared STAGE_DIR collision).

## Decisions made / held

Maintainer decisions recorded this session: #862 approved (return 500); #853 deferred to a later phase (loopback-default recommendation stands on record); setup-daemon question resolved as wait-for-investigation, then reframed -- the two-daemon design (http 8080 public, http2 5336 admin/setup) is deliberate per finishInstall's own comment; the real defects are #867/#870.

Held for maintainer: #868 (which mainResponder.root is authoritative), #853 (revisit at Manila/public-serving), next-wave sequencing.

## Process lessons codified

- Post-merge worktree cleanup only AFTER the implementing agent's explicit stand-down ACK (a teardown raced a running suite once; recovered, no loss; rule now in memory).
- rebase alone is not enough after a C-fix merge -- rebuild the CLI or #848's tests show 4 fake failures.
- `--lock-opened-roots` silently discards protocol-session mutations (looks like a clean no-op).
- Compile-gate with an installer-unrepairable negative control is mandatory on every script install (`newScriptObject` auto-closes unbalanced braces, so naive controls false-pass).

## Legacy reference: last-ever Frontier Mac package

The final UserLand Frontier Mac release remains downloadable:

- Download page: http://manila.userland.com/download
- macOS package: http://static8.userland.com/rack1/gems/ManilaWebsite/Frontier95.dmg

Directly relevant to #868: the shipped `mainResponder.root` in this repo predates the .ut mirror's source (the mirror carries a 2005 redirect-args fix the binary lacks). Per the maintainer, the Frontier 9.5 .dmg contains the legacy VIRGIN `mainResponder.root` and `Manila.root` -- making it the authoritative-source candidate for the #868 reconciliation and a clean baseline for the Manila install wave. Note: userland.com servers were in rate-limit recovery as of this session -- coordinate before bulk-fetching.

## Next wave (proposed)

1. #866 -- the keystone; C-side compiler/install fix that unblocks all further UserTalk script work.
2. #859 pathString under --skip-startup (possibly C-side init).
3. #867 + #870 -- the setup-page pair (first-run onboarding).
4. #855 flag flip (acceptance test: the 13-failure set).
5. Then the Manila install wave.
