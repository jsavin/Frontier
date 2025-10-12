# Decision Needed: Concurrency Model (ADR-0003)

Context
- Current code is event-loop centric with pervasive globals; we need true multi-threading for CLI/server modes without sacrificing determinism.

Proposed Decision
- Use C11 threads/pthreads as the portable baseline; thin abstraction can map to GCD on macOS.
- Adopt single-writer/multi-reader for DB + language state; forbid shared mutable state without synchronization.
- Introduce request/task contexts; remove implicit thread-local globals.

Alternatives
- GCD-only (macOS-first): simpler locally, weaker portability.
- Single-threaded with async I/O: simpler concurrency, limits throughput.

Inputs Needed
- Subsystems requiring exclusive access vs. read-sharing.
- Any reentrancy constraints in language/DB APIs.

Acceptance Criteria
- Documented API for creating/joining tasks and passing contexts.
- Tests demonstrating safe parallel reads and serialized writes.

Links
- planning/adr/0003-concurrency-model.md
- planning/DECISIONS.md
