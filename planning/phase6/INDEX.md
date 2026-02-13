# Phase 6 — CRDT & Collaborative ODB

Status
- State: Planning
- Phase: Phase 6
- Last Updated: 2026-02-13
- Notes: Strategic initiative for multi-user collaborative editing. Depends on Phase 4 (threading) and Phase 5 (text modernization) completion.

Related Docs
- `planning/phase6/CRDT_FOUNDATION_ROADMAP.md` — Strategic vision and phased roadmap
- `planning/phase6/EXTERNAL_ATOMICITY_AND_COLLABORATION_ROADMAP.md` — Migration path from single-user to multi-user
- `planning/architectural_decision_records/CRDT_IMPLEMENTATION_ROADMAP.md` — Detailed implementation roadmap
- `planning/architectural_decision_records/CONTEXT_PATTERN_FOR_ODB_COLLABORATION.md` — Operation context pattern
- `planning/architectural_decision_records/NODE_IDENTITY_ARCHITECTURE_ASSESSMENT.md` — Node UUID analysis
- `planning/architectural_decision_records/OUTLINE_OPERATION_CONTEXT.md` — Outline context implementation (Phase 3 foundation)

Change Log
- 2026-02-13: Initialized document. Moved CRDT roadmaps from planning root into phase6/.

## Overview

Phase 6 brings collaborative editing to Frontier's Object Database (ODB) using Conflict-free Replicated Data Types (CRDTs). This enables Google Docs-style real-time collaboration across all ODB node types: outlines, scripts, tables, WPText, menus, and pictures.

### Key Documents

| Document | Description |
|----------|-------------|
| [CRDT Foundation Roadmap](CRDT_FOUNDATION_ROADMAP.md) | Strategic vision, target state, and phased milestones (months 6-18) |
| [External Atomicity & Collaboration Roadmap](EXTERNAL_ATOMICITY_AND_COLLABORATION_ROADMAP.md) | Stepwise migration from single-user legacy behavior to multi-user collaboration |

### Related ADRs

The architectural decision records directory contains detailed CRDT implementation analysis:

- **CRDT Implementation Roadmap** — 2100+ line detailed technical roadmap
- **Context Pattern for ODB Collaboration** — Operation context pattern enabling CRDT capture
- **Node Identity Architecture Assessment** — UUID-based node identity for conflict-free merging
- **Outline Operation Context** — Phase 3 foundation work (Issue #135)

### Prerequisites

- Phase 4 threading model (GIL, thread safety) must be stable
- Phase 5 text modernization (UTF-8) should be complete for text CRDT compatibility
- Operation context pattern from Phase 3 (Issue #135) is the foundation

## Next Steps

1. Complete Phase 4 threading and Phase 5 text modernization
2. Validate operation context pattern captures sufficient information for CRDT replay
3. Select CRDT algorithm family (Yjs-style, Automerge-style, or custom)
4. Define wire protocol for replication
