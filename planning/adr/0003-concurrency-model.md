# ADR 0003 — Concurrency Model

Status
- Proposed
- Date: 2025-09-30

Context
- Current code assumes cooperative/event-loop style (`WaitNextEvent`) and pervasive globals.
- Goals: enable true multi-threading for headless/CLI and future servers while preserving determinism.

Decision
- Use C11 threads/pthreads as the portable baseline; provide a thin abstraction that can map to GCD on macOS for convenience.
- Adopt a single-writer/multi-reader model for the database and language runtime; disallow shared mutable state without explicit synchronization.
- Introduce request/task contexts for CLI/server operations; no implicit thread‑local globals.

Consequences
- Introduce explicit contexts; ban implicit global mutation across threads.
- Require thread-safety audits and tests for runtime subsystems.

Links
- planning/Frontier_Refactoring_Plan.md
- planning/INDEX.md
