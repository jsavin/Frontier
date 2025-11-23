# Modernization Decision Log

Status
- State: In Progress
- Phase: Multi-Phase
- Last Updated: 2025-11-23
- Owner: Codex
- Notes: Central index of ADR topics; update when decisions move between Draft/Proposed/Accepted.

Purpose
- Central index of cross-cutting decisions and their status. Each entry links to an ADR or topic doc.

How to propose
- Add a new ADR under `planning/adr/` using the next sequence number (`000X-*`).
- Keep entries concise: Context, Decision, Consequences, Links. Update status when accepted.

Decisions & Topics
- Concurrency Model — ADR 0003 (TBD)
- OSA/IPC Strategy — ADR 0004 (TBD)
- Networking Architecture & Security — ADR 0005 (TBD)
- Global State Boundaries — ADR 0006 (TBD)
- File I/O & Path Policy — ADR 0007 (Draft; see database_path_canonicalization.md)
- Unicode Strategy — ADR 0008 (TBD)
- WPText → RTF Migration — ADR 0009 (Accepted; Paige is conversion-only, portable RTF/UTF-8 pipeline is canonical)
- EFP Routing in Headless — ADR 0010 (Proposed)
- V7 On-Disk Endianness — (Accepted; v7 headers/trailers/table/avail write big-endian; see `planning/big_endian_portability_audit.md`, `docs/database_architecture.md`)

Related Indexes
- Overall plan: planning/Frontier_Refactoring_Plan.md
- Phase/status index: planning/INDEX.md
- Existing ADRs: planning/adr/
 - Bootstrap plan: planning/system_verbs_bootstrap_plan.md (temporary, guarded)

## Decision Needed Checklist

| Topic                           | ADR    | Status   | Owner | Target Phase |
|---------------------------------|--------|----------|-------|--------------|
| Networking Architecture/Security| 0005   | Proposed | TBD   | Phase 1      |
| File I/O & Path Policy          | 0007   | Draft    | TBD   | Phase 1      |
| Concurrency Model               | 0003   | Proposed | TBD   | Phase 2      |
| Global State Boundaries         | 0006   | Proposed | TBD   | Phase 2      |
| OSA/IPC Strategy                | 0004   | Proposed | TBD   | Phase 2      |
| Unicode Strategy                | 0008   | Proposed | TBD   | Phase 4      |
| WPText → RTF Migration          | 0009   | Accepted | Codex | Phase 4      |
| EFP Routing in Headless         | 0010   | Proposed | TBD   | Phase 1      |
| V7 On-Disk Endianness           | —      | Accepted | Codex | Phase 2      |

### Notes
- WPText decision: Paige is conversion-only; headless/CLI/runtime paths use the portable extractor + RTF/UTF-8 helpers. Portable `WPRT` is the canonical on-disk format (see `planning/progress_reports/2025-11-20-paige_portable_milestone.md`).
- V7 endianness: modern roots write big-endian for headers/trailers/table addresses and avail list links; sizes are 64-bit BE. Tracking and tests in `planning/big_endian_portability_audit.md` and `tests/db_format_tests.c`.

Pre‑issue stubs (to copy to GitHub later) live in `planning/issues/`.
