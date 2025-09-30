# ADR 0006 — Global State Boundaries

Status
- Proposed
- Date: 2025-09-30

Context
- Frontier relies heavily on globals (language engine, DB, UI). This impedes testing and threading.

Decision
- Introduce explicit runtime contexts (e.g., `FrontierContext`) carrying subsystem handles and configuration.
- Public APIs accept/return contexts; forbid implicit access to globals. Provide a transitional shim for legacy entry points.
- Define lifecycle hooks: init(startup) → attach DBs → run tasks → teardown; no hidden singletons.

Consequences
- Clear init/teardown lifecycle; improved reentrancy and test isolation.

Links
- planning/Frontier_Refactoring_Plan.md
- planning/0.5.13_usertalk_language_summary.md
