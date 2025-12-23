# Frontier Refactoring Plan

Status
- State: In Progress
- Phases: Foundations → Architecture → Headless Runtime → Text Modernization → Tooling
- Last Updated: 2025-12-22
- Notes: Phase 3 (Headless Runtime) implementation phase active; logging infrastructure prioritized as foundational work. See planning/_CURRENT_TODO_LIST.md for current priorities.

Related Docs
- `planning/phase_overview.md`
- `planning/INDEX.md`
- `planning/adr/`
- `planning/phase_gates.md`

Change Log
- 2025-12-22: Consolidated as canonical plan in planning/ directory (removed duplicate root version).
- 2025-10-12: Reframed plan around five phases and updated current focus areas.
- 2025-09-29: Added headless documentation references and phase gate links.
- 2025-09-20: Documented portable handle runtime milestone.

## Executive Summary

Frontier’s refactoring effort is organised into five major phases:

1. **Foundations & Toolchain (Phase 1)** – Modernise the build, resolve compiler incompatibilities, and catalogue 64-bit/ARM issues.
2. **Core Architecture & Database (Phase 2)** – Upgrade data structures and database formats for 64-bit safety while preserving existing .odb content.
3. **Headless Runtime & Automation (Phase 3)** – Deliver a reliable CLI/headless runtime with UIServices adapters and automated regression tests.
4. **String & Text Modernization (Phase 4)** – Retire legacy Pascal/MacRoman assumptions, plan the UTF-8 migration, and prepare rich-text subsystems for new encodings.
5. **Toolchain & Parser Evolution (Phase 5)** – Refresh the language toolchain (Bison 3 regeneration, grammar updates) once earlier phases stabilise.

Each phase has explicit exit criteria captured in `planning/phase_overview.md`. Work items that span phases (e.g., ADRs, glossary updates) live alongside the phase directories.

## Key Risks & Mitigations

| Risk | Mitigation |
|------|------------|
| Database corruption during 64-bit transition | Dual-read/convert strategy; migration tooling tracked in `phase2/` docs. |
| Handle/Hash-table performance regressions | Portable handle runtime (Phase 2) and hash modernisation plan (`phase2/0.5.16_…`). |
| UI coupling blocks headless automation | Ports-and-adapters design (`phase3/ui_abstraction/`) with explicit no-UI linkage policy. |
| Encoding assumptions break scripts | UTF-8 transition roadmap (`phase4/utf8_transition_plan.md`) and phased rollout with compatibility tooling. |
| Parser/toolchain drift | Parser regeneration playbook (`phase5/`) and ADRs covering grammar ownership. |

## Phase Snapshots

### Phase 1 – Foundations & Toolchain
Focus: compiler compatibility, header hygiene, QuickTime retirement, and initial audits.
- Deliverables: minimal viable compilation, testing harness, architecture audits.
- Exit Criteria: clean builds on supported compilers/architectures and documented remediation steps (see `phase1/README.md`).

### Phase 2 – Core Architecture & Database
Focus: 64-bit/ARM data structures, database versioning, portable handle runtime, hash-table modernization.
- Deliverables: new database migration plan, handle/runtime abstractions, performance benchmarks.
- Exit Criteria: architectural upgrades validated against test databases; migration tooling vetted (see `phase2/README.md`).

### Phase 3 – Headless Runtime & Automation
Focus: CLI execution, UIServices boundary, headless quickstart, runtime tests.
- Deliverables: CLI invocation plan/results, headless stub matrix, system verb bootstrap strategy.
- Exit Criteria: automated headless tests running via CLI; clear guidance for headless developers (see `phase3/` docs).

### Phase 4 – String & Text Modernization
Focus: UTF-8 migration planning, string helper cleanup, rich-text roadmap.
- Deliverables: UTF-8 transition plan, string modernization charter, updated text APIs.
- Exit Criteria: approved UTF-8 rollout strategy and prioritized implementation backlog (see `phase4/` docs).

### Phase 5 – Toolchain & Parser Evolution
Focus: grammar regeneration, Bison 3 migration, language tooling maintenance.
- Deliverables: parser regeneration procedure, compatibility analysis, tooling checklist.
- Exit Criteria: parser pipeline regenerates cleanly on modern tools; go/no-go decision for adopting regenerated output (see `phase5/`).

## Current Focus (Q4 2025)
1. Finalise CLI/headless build cleanup and document runbooks (`phase3/`).
2. Refine UTF-8 migration milestones and dependencies (`phase4/utf8_transition_plan.md`).
3. Keep ADRs up to date as architectural decisions cross phase boundaries.

For tactical updates, consult `planning/INDEX.md`. For historical context or risk justification, review the documents in `phase1/` and `phase2/`.
