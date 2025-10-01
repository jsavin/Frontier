# Decision Needed: Global State Boundaries (ADR-0006)

Context
- Pervasive globals impede testing, reentrancy, and multi-threading.

Proposed Decision
- Introduce explicit `FrontierContext` carrying subsystem handles and config.
- Public APIs take/return contexts; transitional shim for legacy entry points.
- Define lifecycle hooks: init → attach DBs → run → teardown; no hidden singletons.

Alternatives
- Keep globals with locks (leaky and brittle).
- Partial contextization (unclear boundaries).

Inputs Needed
- Inventory of global variables and their owners.
- Minimal API changes to route context.

Acceptance Criteria
- Context struct defined and passed through key APIs.
- Tests that run multiple contexts without interference.

Links
- planning/adr/0006-global-state-boundaries.md
- planning/DECISIONS.md
