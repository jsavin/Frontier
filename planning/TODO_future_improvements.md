# TODO: Future Improvements

Status
- State: Living Document
- Phases: Multi-Phase Roadmap
- Last Updated: 2025-10-26
- Notes: Organised to mirror the current phase plan (see `planning/phase_overview.md`).

Related Docs
- planning/Frontier_Refactoring_Plan.md
- planning/INDEX.md
- planning/phase3/ui_abstraction/PHASES.md

Change Log
- 2025-10-25: Captured Cancoon/About window UI preservation requirement.
- 2025-10-12: Updated links, clarified timelines by phase.
- 2025-09-29: Initial draft (memory management audit, hash table modernisation notes).

## Phase 1/2 — Memory Management Audit (Rolling)

**Priority:** High  
**Timeline:** Begin immediately; finish core audit alongside Phase 2 architecture work.

Goals
- Identify and fix unsafe or leaky patterns across the legacy C codebase.
- Standardise ownership and lifetime for heap objects and Handles.
- Reduce Undefined Behaviour (UB) / ASan / UBSan findings (alignment, VLAs, function pointer casts).

Scope (examples, not exhaustive)
- Remove or replace variable-length arrays (VLAs) with fixed or heap buffers.
- Fix misaligned reads/writes (e.g., Handle stores) with safe copies.
- Audit `malloc`/`newclearhandle`/`newhandle`/`newtexthandle` call sites for matching free/dispose patterns.
- Ensure error paths and early returns release allocations.
- Verify temp stack usage (`pushvalueontmpstack`/`cleartmpstack`/`exemptfromtmpstack`) for all heap values.
- Replace unsafe pointer casts (e.g., function pointer mismatches) with shims/adapters.
- Prefer `size_t` for sizes/lengths; validate bounds before copy/move.

Deliverables
- Tracking issue/checklist per module (lang, memory, strings, op*, db, tables, UI stubs).
- Sanitiser-clean headless test runs with documented suppressions where unavoidable.
- Coding guidelines covering ownership conventions and helper APIs.

Initial Targets
- `Common/source/langstartup.c`: charset initialisation allocations — verify post‑audit.
- `Common/source/memory*.c`: alignment-safe Handle ops and memcpy patterns — in progress.
- `Common/source/langcallbacks.c`: remove VLAs in error printing — done; re-audit other debug paths.
- `Common/source/langtree.c`: LP64 packing guards for treenodes — done; verify other packed structs.

Process
- Enable ASan/UBSan in CI for tests; treat new sanitiser errors as must-fix.
- Add optional leak checks where feasible; label noisy false positives.
- Document ownership for public APIs in headers and planning notes.

## Phase 3 — Hash Table Modernisation

**Priority:** Medium  
**Timeline:** Execute after core architecture upgrades stabilise (Phase 2 exit).

Background
- Current hash table uses first/last character only; bucket count fixed at 11.
- Poor distribution causes performance issues with large tables.

Proposed Changes
- Version 8 database format with modern hash tables.
- FNV-1a (or similar) hash implementation.
- Dynamic bucket sizing and load-factor-based resizing.

Migration Strategy
- Automatic conversion from Version 7 → Version 8 with rollback path.
- Maintain backward compatibility mode where needed.
- Benchmark improvements using representative databases.

Reference Docs
- `planning/phase2/0.5.16_hash_table_modernization_strategy.md`
- `planning/phase2/0.5.19_phase1_migration_implementation_complete.md`

## Phase 3 — Headless Migration Options

**Priority:** Low  
**Timeline:** After Phase 3 CLI/adapter work is stable.

Background
- Some deployments need automated database migration without interactive prompts.

Proposed Changes
- Add configuration surface (CLI flag, environment variable, preference) to auto-migrate.
- Ensure headless builds honour the setting while retaining safe defaults for interactive shells.

Reference Docs
- `planning/phase3/system_verbs_bootstrap_plan.md`
- `planning/phase3/DEVELOPER_QUICKSTART_HEADLESS.md`

## Future Considerations (Phase 4/5 and Beyond)

### Performance Optimisations
- Investigate memory-mapped I/O for large database files.
- Optional compression for on-disk data.
- Improved caching strategies for frequently accessed tables.

### User Experience Enhancements
- Progress indicators for long-running migrations.
- Batch migration tooling for multiple databases.
- Easy rollback/downgrade support.
- **Cancoon/About window disentanglement:** preserve the 442-byte tyversion2cancoonrecord (About/Home window state) while designing the next UI layer. Eventually we need a per-user UI app that can render the Cancoon window when the runtime runs headless (daemon or service) without losing the msg()/agent log. This will require new IPC hooks so the long-running process can surface the window state safely.

### Developer Experience
- Better database inspection/validation tools.
- Automated database integrity checkers.
- Comprehensive API documentation refresh once new infrastructure lands.
- **Strings pipeline (Phase 2 follow-up):** After libyaml-based ingestion is stable, re-enable the bespoke bison/flex YAML parser to match libyaml parity while dropping the third-party dependency. Includes full YAML subset support (indent/dedent, folded strings, metadata fields) and regression tests comparing both pipelines.

These items provide a parking lot for work that spans or follows the current phases. Revisit after each phase review to reprioritise.
