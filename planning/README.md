# Frontier Refactoring – Planning Docs

Status
- State: Carbon Migration In Progress
- Active Plan: `planning/phase3/carbon_migration/README.md`
- Last Updated: 2025-10-30
- Notes: Draft/Planned/In-Progress legacy docs were restored to `planning/phase*/`; only completed notes remain in `planning/archive/`.

Related Docs
- `planning/INDEX.md` – navigation for active and archived workstreams
- `planning/Frontier_Refactoring_Plan.md` – narrative goals and risks (historical)
- `planning/phase_gates.md` – readiness checks (archived)
- `planning/archive/phase3/pascal_runtime_modernization.md` – Pascal-era data layout modernization roadmap (archived)

Change Log
- 2025-10-30: Reactivated unfinished phase docs (moved back to `planning/phase*/`) and pointed navigation to the Carbon plan.
- 2025-10-12: Reorganized legacy docs into phase subdirectories and refreshed status.
- 2025-09-29: Initial skeleton (status/related/change log sections).

## Directory Layout

| Directory | Purpose |
|-----------|---------|
| `carbon_migration/` | **Active.** Canonical plan for removing Carbon dependencies (inventory, phases, decision log, status log). |
| `phase2/`, `phase3/`, `phase4/`, `phase5/` | Legacy work still in motion. Each doc retains its own Status block; update there when progress changes. |
| `archive/phase1`, `archive/phase2`, `archive/phase3` | Completed/retired material from earlier phases (read-only). |
| `adr/`, `issues/`, `phase_overview.md`, `INDEX.md` | Cross-cutting decision records, backlog, navigation aids. `phase_overview.md` maps both active and archived phase docs. |

## Writing Guidelines

1. Include a status block (`State`, `Phase`, `Last Updated`, `Notes`) at the top of each planning doc.
2. Add yourself to the change log when you update content.
3. Prefer short, phase-scoped documents over monolithic plans; cross-link related notes.
4. Use ADRs (`planning/adr/`) for architectural decisions that affect multiple phases.
5. Run `python3 scripts/check_doc_links.py` before submitting large doc refactors.

For a high-level summary of goals and risks, start with `planning/Frontier_Refactoring_Plan.md`. For day‑to‑day navigation, use `planning/INDEX.md`.
