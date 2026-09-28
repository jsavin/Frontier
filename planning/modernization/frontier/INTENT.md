# Modernization Intent: frontier

Status
- State: Active
- Date: 2026-09-27
- Source: `--source .` (this repository; no `legacy/` copy)
- Output: `planning/modernization/frontier/` (the plugin's `analysis/frontier/`)

## Goal

**Understand**: map the system and the plans as they stand, reconcile the roadmaps, and let the assessment recommend which road to take next (uplift, transform, or reimagine).

Original request (verbatim):

> I've already done significant work on modernizing Frontier to run as a headless process on modern systems. I want you to assess the plans and current status and help me figure out how to move forward from here.

## From / To

- **From:** Frontier kernel in C (about 250k lines C, 22k lines headers) plus UserTalk applications stored in `.root` ODB databases. It is partly modernized already: headless CLI/REPL/protocol runtime, v7 database format, GIL threading model, and a webserver that serves mainResponder pages.
- **To:** not fixed yet. The assessment recommends a target. The direction of record lives in `product/VISION_1_0.md` and `planning/_CURRENT_GOAL.md`, and must be read before any conclusion is drawn.

## What must stay true

Selected (verbatim):

- Old system keeps running
- Behavior matches exactly
- Fix known bugs as we go
- Security review first

Clarification from JES (verbatim):

> The old app doesn't need to run on modern systems unmodified. Please understand what's been done so far as well as the longer-term vision for the project before drawing any major conclusions. The one rule here is that migration of legacy applications that were implemented in UserTalk and live in legacy .root files should cleanly migrate into the new system and run, at least in so far as they can run headlessly or with only CLI/REPL-style user interaction.

### How to read these together

- The **hard invariant** is that legacy UserTalk applications in legacy `.root` files migrate cleanly and run in the new system, to the extent they can run headless or with CLI/REPL-style interaction only.
- "Behavior matches exactly" applies to **UserTalk-visible semantics** that those applications depend on. It does not apply to the old Mac GUI application, which does not need to run unmodified on modern systems.
- "Fix known bugs as we go" is allowed where the fix does not break legacy `.root` applications.
- "Security review first": run `harden` right after `assess`, before any further build work that the assessment proposes.
- **Do not draw major conclusions** until the work done so far and the long-term vision (`product/VISION_1_0.md`, `planning/phase6` CRDT, `planning/phase7` polyglot) have been read.

## Open items

- There are three planning sources of record that may conflict: `planning/_CURRENT_GOAL.md` (2026-08-10), `product/VISION_1_0.md` plus `product/plans/2026-08-09-phase-0-1-execution-plan.md`, and `planning/phase_overview.md` (stale, 2025-11). The assessment should reconcile them.
