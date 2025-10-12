# ADR 0001 — Move Hash Tables Modernization to Phase 3

Status
- State: Accepted
- Date: 2025-09-29

Related Docs
- planning/0.5.16_hash_table_modernization_strategy.md
- planning/ui_abstraction/PHASES.md
- planning/Frontier_Refactoring_Plan.md

Change Log
- 2025-09-29: ADR accepted; sections initialized.

Context
- Hash table modernization is significant and touches core data structures and migration.
- Phase 2 must focus on UI/runtime separation to unblock headless, UI shells, and future clients.

Decision
- Defer hash table modernization to Phase 3, after Phase 2 UI abstraction is complete.

Consequences
- Phase 2 plan and docs will not target hash table changes.
- Testing in Phase 1/2 validates current hash implementations; performance work starts in Phase 3.

References
- planning/0.5.16_hash_table_modernization_strategy.md
- planning/ui_abstraction/PHASES.md
- planning/Frontier_Refactoring_Plan.md
