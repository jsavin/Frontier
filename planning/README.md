# Frontier Refactoring – Planning Docs

Status
- State: In Progress
- Phases: Foundations → Architecture → Headless Runtime → Text Modernization → Tooling
- Last Updated: 2025-10-12
- Notes: See `planning/phase_overview.md` for the roadmap and exit criteria.

-Related Docs
- `planning/INDEX.md` – quick links into each phase
- `planning/Frontier_Refactoring_Plan.md` – narrative goals and risks
- `planning/phase_gates.md` – readiness checks before advancing phases
- `planning/phase3/pascal_runtime_modernization.md` – Pascal-era data layout modernization roadmap

Change Log
- 2025-10-12: Reorganized legacy docs into phase subdirectories and refreshed status.
- 2025-09-29: Initial skeleton (status/related/change log sections).

## Directory Layout

| Directory | Purpose |
|-----------|---------|
| `phase1/` | Toolchain, compiler, and early audit work that bootstrapped the port to modern macOS toolchains. |
| `phase2/` | 64-bit/ARM data structure upgrades, database versioning, and portable handle runtime plans. |
| `phase3/` | CLI & headless runtime enablement, UIServices ports/adapters, and automated runtime testing. |
| `phase4/` | String and text modernization initiatives, including the UTF-8 transition roadmap. |
| `phase5/` | Parser and toolchain evolution (Bison 3 migration, regenerated grammar artifacts). |
| `adr/`, `issues/`, `phase_overview.md`, `INDEX.md` | Cross-cutting decision records, backlog, and navigation aids. |

Most documents keep their historical numbering (`0.x`, `1.x`, etc.) to preserve context after the move, but the authoritative organization is now by phase. When writing new planning material, place it in the appropriate phase directory and update `phase_overview.md` / `INDEX.md` if scope or exit criteria change.

## Writing Guidelines

1. Include a status block (`State`, `Phase`, `Last Updated`, `Notes`) at the top of each planning doc.
2. Add yourself to the change log when you update content.
3. Prefer short, phase-scoped documents over monolithic plans; cross-link related notes.
4. Use ADRs (`planning/adr/`) for architectural decisions that affect multiple phases.
5. Run `python3 scripts/check_doc_links.py` before submitting large doc refactors.

For a high-level summary of goals and risks, start with `planning/Frontier_Refactoring_Plan.md`. For day‑to‑day navigation, use `planning/INDEX.md`.
