# Modernization Decision Log

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
- WPText → RTF Migration — ADR 0009 (TBD)

Related Indexes
- Overall plan: planning/Frontier_Refactoring_Plan.md
- Phase/status index: planning/INDEX.md
- Existing ADRs: planning/adr/

## Decision Needed Checklist

| Topic                           | ADR    | Status   | Owner | Target Phase |
|---------------------------------|--------|----------|-------|--------------|
| Networking Architecture/Security| 0005   | Proposed | TBD   | Phase 1      |
| File I/O & Path Policy          | 0007   | Draft    | TBD   | Phase 1      |
| Concurrency Model               | 0003   | Proposed | TBD   | Phase 2      |
| Global State Boundaries         | 0006   | Proposed | TBD   | Phase 2      |
| OSA/IPC Strategy                | 0004   | Proposed | TBD   | Phase 2      |
| Unicode Strategy                | 0008   | Proposed | TBD   | Phase 4      |
| WPText → RTF Migration          | 0009   | Proposed | TBD   | Phase 4      |

Pre‑issue stubs (to copy to GitHub later) live in `planning/issues/`.
