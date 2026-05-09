# ADR-017: filesystem-canonical ODB sources with .root as build artifact

**Status:** GO — exploration phase 2026-05-09
**Date:** 2026-05-09
**Author:** JES + Claude (session "f1-agentic-usertalk")

## Context

Frontier's source-of-truth question is currently incoherent. The runtime treats the `.root` file as authoritative — it's what the dispatcher reads, what verb tables come from, what scripts execute from. Git treats the `.ut` files in `usertalk_scripts/Frontier.root/` as authoritative — they're what humans edit, what diff and blame work on, what code review sees. Neither is wrong for its purpose, but the two views drift, silently, with no detection mechanism, no enforced sync protocol, and no merge UI when they diverge.

The slash-menu chain (PRs #575–#615) made this pattern's costs concrete:

- PR #582 shipped a `help.ut` handler that called a non-existent `stdout` verb. Compile passed; runtime would have failed. Caught by accident in PR #583 development. Drift between `.ut` and ODB-stored body wasn't checked.
- PR #594 hand-installed scripts via `--protocol --allow-mutate` and grew Virgin.root from ~10.95 MB to ~17 MB through unrelated `fileMenu.save()` accumulations. The 100 MB GitHub limit was nearly hit again, and the file-size growth has no upper bound under the current workflow.
- The startup script in `databases/Virgin.root` (current canonical) fails to compile on REPL boot — `[lang-ERROR] line 3: Can't compile this script because of a syntax error.` The .ut export looks fine on lines 1–3 (all comments), so the ODB-stored body has drifted from the .ut, undetected.
- PR #609 (parser fix) merged on agent-reported worktree counts and broke 43 integration tests that were silently dependent on a 1997-era LF-eating bug. The drift problem is not just `.ut` vs ODB; it's that we have no boundary between "this is the canonical text" and "this is what's compiled and stored," which masks dependencies.

The current pattern was always a transitional accommodation. This ADR proposes the destination.

## Decision

**The filesystem becomes the source of truth for ODB content.**

Concretely:

- Every object in a `.root` file gets a corresponding file (or directory, for tables) in a tracked source tree. The directory hierarchy mirrors the ODB object graph: a table-typed object becomes a directory; its children become files (or sub-directories if they are themselves tables) inside it.
- The `.root` file is a **build artifact** produced by `make` from the source tree. Same role as a compiled binary, a `Cargo.lock` regenerated from `Cargo.toml`, or `Assets.car` produced from `.xcassets/`.
- For development, the source tree is what's edited, diffed, reviewed, blamed, and merged. The `.root` is transient — regenerated on demand, not the artifact under review.
- The runtime is unchanged. It continues to operate on `.root` files. It does not learn about git, the filesystem, or the build pipeline. It does emit "object changed" events through its existing callback infrastructure; a separate sync layer consumes those events and projects mutations back to the source tree.
- Reverse sync (runtime mutations → disk) is content-based: when a callback fires, project the mutated object to its on-disk form, compare to existing, write only if different. No spurious churn for round-trip-identical edits.

This makes drift structurally impossible in steady state. Per-object git diffs become the natural granularity. Per-script blame works. PR review of UserTalk changes shows the actual diff. The 100 MB Virgin.root problem dissolves (we stop committing the binary). Two developers/agents editing different objects don't conflict at the file level. AI-assisted Frontier development gets dramatically easier.

## Open questions to resolve in the exploration phase

These are the four explorations whose results determine whether this ADR transitions from Proposed to Accepted, Deferred, or Rejected.

### 1. Projection format and round-trip fidelity

**Question.** `table.tableToXml` and `table.tableToFiles` already exist in the UserTalk runtime. Do they produce projections that survive a round-trip through the runtime back into a byte-equivalent ODB representation?

**What to do.** Pick three representative ODB locations spanning the type space:
- A script (e.g. `system.startup.startupScript` once it's fixed, or `system.menus.handlers.repl.exit`) — exercises UserTalk source preservation, comments, line endings.
- A mostly-data table (e.g. `system.menus.data.repl`) — exercises nested tables, scalar values, address-typed fields.
- A table with externals or non-trivial type metadata (`system.verbs.builtins.menu` perhaps, or any table carrying refcons / parenthashtable structure that we know the projection might fumble) — exercises the hard cases.

For each: export with `tableToFiles` (or `tableToXml`, whichever is more lossless), re-import to a new `.root`, compare object-by-object to the original. Document what survives, what's lossy, and the magnitude of the lossy parts. The output of this exploration is a fidelity matrix per object type.

**Decision criterion.** Lossless or with-known-bounded-loss for scripts and data tables → green light. Lossy in ways that affect runtime behavior on common types → yellow, may require a richer projection format. Fundamentally lossy in ways that can't be fixed without redesigning the format → red, defer this ADR.

### 2. Bootstrap: filesystem → valid `.root`

**Question.** What's the minimum sequence to produce a working `.root` from an empty checkout? Frontier's runtime is itself stored in Frontier.root, so there's a chicken-and-egg.

**Two viable approaches.**

- **Stage-0 image.** Ship a minimal `.root` in git containing only the runtime needed to compile UserTalk and run the build. It's small (a few hundred KB if scoped tightly), versioned in git as a binary blob, and updated only when its own contents change (rare). The build path is: stage-0 boots → reads source tree → compiles each `.ut`, builds each table → emits the full Frontier.root.
- **Non-UserTalk build tool.** Write the build tool in Python or Rust with its own UserTalk parser and ODB writer. Avoids the bootstrap problem but requires maintaining a second UserTalk implementation, which is a significant ongoing cost.

**What to do.** Sketch both approaches. For stage-0: identify the minimum object set (probably some subset of `system.compiler`, `system.verbs.builtins.kernel`, the scanner/parser glue). Estimate size. Identify how it gets updated. For non-UserTalk: estimate the parser cost, decide whether existing `langscan.c`/`langparser.c` could be ported or wrapped (e.g., FFI from Python). Recommend one with rationale.

**Decision criterion.** Stage-0 with reasonable size (< 1 MB ideal, < 5 MB tolerable) and a clear update story → green. Non-UserTalk tool with a clear path → green. Both ugly → red, defer.

### 3. Reverse sync via existing callbacks

**Question.** Frontier already has a callback system for "table changed," "script edited in editor window," etc. Can reverse sync register a callback handler that projects the affected object back to disk on coherent-state events (compile, save, table-write commit), without designing new infrastructure?

**What to do.** Read the callback infrastructure. Identify the events that signal "user/agent has reached a coherent state and the object should be persisted." Wire a single callback for one event type (e.g. "script source changed and recompiled") to a single projection-writer for one specific script. Demonstrate the loop: edit script in REPL, observe `.ut` file appears with the new content, no edit in REPL means no disk write.

This is a contained prototype on a branch. Does not need to ship.

**Decision criterion.** The loop closes for one example with reasonable code → green. The callback events don't have the right granularity (too noisy or too coarse) → yellow, may require a "deferred sync queue" design. The callbacks aren't accessible from the right layer or have ordering bugs that make sync unreliable → red, defer.

### 4. Object dependency-order analysis

**Question.** When building a `.root` from a source tree, in what order should objects be assembled? Some scripts reference verbs that live in other tables; some tables reference each other through addresses. Order matters at compile time.

**What to do.** Take a representative object subgraph (probably `system.menus.data.repl` plus its handler scripts plus the kernel verbs they reference). Compute the dependency graph. Determine whether topological sort produces a valid build order. Identify any cycles (e.g. mutually-recursive scripts) and document how to handle them (forward declaration, two-pass build, etc.).

**Decision criterion.** Build order is computable from source-tree structure alone → green. Computable but requires non-trivial analysis (call graph extraction from UserTalk source) → yellow, build tool gets more complex but is still tractable. Cycles or dynamic-resolution patterns make order unknowable until runtime → red, suggests we need a different build model.

## Rollout phasing (assuming all four explorations come back green-or-yellow)

1. **Pilot one object type** end-to-end. Probably scripts. Build the projection writer + reader + the manifest for incremental builds. Take `Virgin.root`'s scripts, project them to `usertalk_scripts/`-style files, build a new Virgin.root from those files, verify byte-or-semantically-equivalent.
2. **Iterate object types**: data tables, outlines, binary blobs (if bridged at all), externals.
3. **Migrate**: flip the canonical-vs-derived relationship. Start `.gitignore`-ing `.root` files. Update build pipeline to make `.root` from source. Update test runner to consume the built `.root`.
4. **Reverse sync**: wire callbacks from the runtime to the projection writers. Mutations in the runtime project back to disk. The user runs `git add` / `git commit` on the resulting source-tree changes.
5. **Audit and clean**: walk the existing `databases/Virgin.root` content. The legacy webserver/Manila/docserver/fatpages content (~16 MB of GIFs and JPEGs) becomes a directory of files; decide which to keep, delete the rest as a single git operation.

Phases 1 and 2 can interleave per type. Phase 3 is the migration commitment. Phase 4 is comfort-and-ergonomics. Phase 5 is hygiene that becomes possible only after the projection exists.

## What's explicitly out of scope for this ADR

- **Object-level VCS inside the ODB.** The user's longer-term vision is per-object version chains stored natively in the ODB. That's a separate, much larger ADR. This one uses git as the VCS and treats the ODB as runtime-only. Object-level VCS could be layered on later if desired.
- **Multi-user collaboration.** This ADR assumes single-developer-with-git workflows, possibly with multiple agents. Real concurrent editing across machines is a follow-up.
- **The session-state vs version-controlled boundary.** `workspace`, `system.temp`, `clipboard`, agent state — these should NOT be bridged. The boundary needs design but isn't blocking the exploration.
- **Pre-commit hook integration.** A hook calling `frontier-cli sync verify` is an obvious downstream consumer, but only meaningful once the verify primitive exists.

## Decision criteria summary

After the four explorations:

- **All four green** → Accept the ADR; begin pilot phase. Estimated multi-week-to-multi-month rollout, phased so any phase boundary is a safe stopping point.
- **Mostly green with one yellow** → Accept conditionally; address the yellow during the pilot.
- **One or more red** → Either defer (revisit when the red issue is solvable) or reject (document why this approach doesn't work, fall back to a narrower fix like a sync-verify pre-commit hook against the current dual-storage model).

## Notes for implementer (likely Claude or another agent)

Per `docs/AUTO_CHAIN_LESSONS_2026-05-08.md`: this is a foundational-area change. Every PR in the rollout MUST run integration tests from the main session at the post-merge HEAD before merging the next phase. Do not trust agent-reported counts alone for any phase that touches the build pipeline, the projection format, or the sync layer. The rollback story must be clear at every phase boundary — ability to revert one phase's changes without losing prior phases' work.

The startup script bug observed on 2026-05-09 (line 3 syntax error in `system.startup.startupScript`'s ODB body) is a forcing function but should NOT be patched by hand-editing the ODB. The fix arrives as a side effect of the projection working — the `.ut` source compiles cleanly today, and once that becomes canonical, the ODB body is regenerated from it.

## References

- `docs/AUTO_CHAIN_LESSONS_2026-05-08.md` — discipline for foundational-area changes
- ADR-016 — headless menu system projection (current dual-storage pattern this ADR supersedes)
- PR #581 — db.compactDatabase + Virgin.root slim (current bloat-mitigation, becomes unnecessary if .root is gitignored)
- PR #582 review — `help.ut` shipped broken; example of compile-only-not-execute-tested handler
- PR #609 / #610 — parser fix that broke 43 integration tests; example of foundational-area change without integration coverage in same PR
- Issues #586, #611, #614 — open follow-ups touching the same dual-storage pattern
