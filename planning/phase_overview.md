# Project Phase Overview

Status
- State: In Progress
- Phase: Multi-Phase Roadmap
- Last Updated: 2025-11-20
- Owner: Codex
- Notes: High-level snapshot of active phases; update when focus shifts.

> **Context:** Carbon migration is the current focus. Legacy phase docs with open work now reside in `planning/phase*/`; completed notes remain in `planning/archive/`.

This roadmap organizes the Frontier modernization effort into five major phases. Use the table below to find both the active follow-ups and the archived historical notes for each phase.

| Phase | Scope Highlights | Exit Criteria |
| ----- | ---------------- | ------------- |
| **Phase 1 – Foundations & Toolchain** (`planning/archive/phase1/`) | Environment analysis, compiler compatibility, QuickTime removal, minimal build + test harness bring-up. | Modern toolchain validated on target platforms, baseline projects compile, and initial test framework runs end-to-end. |
| **Phase 2 – Core Architecture & Database** (active: `planning/phase2/`; archive: `planning/archive/phase2/`) | 64-bit data structure audit, hash-table/handle modernization, database versioning & path canonicalization. | All core data structures upgraded for 64-bit/ARM, database migration strategy finalized, and portable handle runtime ready for downstream phases. |
| **Phase 3 – Headless Runtime & Automation** (active: `planning/phase3/`; archive: `planning/archive/phase3/`) | CLI execution, headless runtime policies, system verb bootstrapping, UI abstraction, runtime test planning. | CLI/UserTalk invocation works headless, headless developers have quick-start docs, stub coverage matrix complete, and automated runtime test plan executed. |
| **Phase 3b – Headless Service Core** (`planning/phase3/headless_daemon_vision.md`) | Long-lived daemon host with HTTP/stdio endpoints, execution-mode orchestration in the CLI, per-user database ownership, and roadmap for distributed deployments. | Daemon/CLI contract defined, configuration/bootstrap strategy documented, and backlog items created for service transports, ACLs, and multi-root database layout. |
| **Phase 4 – String & Text Modernization** (`planning/phase4/`) | String handling refresh, UTF-8 migration plan, text/resource updates. | Canonical text encoding defined (UTF-8), string/DB plans aligned, and modernization tasks groomed for implementation. |
| **Phase 5 – Toolchain & Parser Evolution** (`planning/phase5/`) | Parser regeneration, Bison 3 migration, follow-on language tooling. | Updated parser pipeline in place, Bison 3 migration approved, and language tooling changes ready for integration. |

| **Phase 6 – CRDT Foundation** (`planning/phase6/`) | Collaborative ODB foundation using CRDTs for multi-user concurrent editing and conflict-free replication. | CRDT data model defined, collaborative editing demonstrated, replication protocol specified. |
| **Phase 7 – Polyglot Scripting** (`planning/phase7/polyglot/`) | JavaScript and Python as first-class scripting languages, abstract language interface (C vtable), compiled extensions (Go/Rust shared libraries). | Language interface validated with UserTalk refactoring, JS and Python POCs running, compiled extension ABI defined. |

## Cross-Cutting References
- `planning/DECISIONS.md` indexes accepted architectural decisions.
- `planning/INDEX.md` serves as the high-level pointer to planning content.
- `planning/adr/` and `planning/issues/` track decision records and open work respectively.

When new planning documents are created for active work, add them under `planning/phase3/carbon_migration/` (or another current plan) and note the relationship in this overview as the roadmap evolves.
