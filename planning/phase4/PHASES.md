# Phase 4 — String & Rich Text Modernization

Status
- State: Planned
- Depends On: Phase 2 (UI Abstraction), Phase 3 (Hash Table Modernization)
- Owner: Core Runtime Team

Objectives
- Retire Classic Mac OS string concepts (Pascal `bigstring`) from modern internals.
- Migrate WPText and related rich-text paths away from Paige/legacy implementations to UTF-based text handling.
- Ensure scripting runtime, database serialization, and I/O work cleanly with UTF-8/UTF-16 where appropriate.

Scope
- String Conversion
  - Add canonical helpers to bridge C strings and Pascal bigstrings (`bs_from_c`, `c_from_bs`).
  - Replace unsafe `(ptrstring)"literal"` casts with safe helpers.
  - Gradually pivot internal APIs to accept `const char*` + length where feasible.
  - Keep Pascal usage at legacy boundaries (tokenizer/on-disk formats) until fully modernized.
- Rich Text Modernization
  - Define a UTF-based text model for WPText and outline text where Paige is currently assumed.
  - Identify all Paige dependencies and plan deprecation or replacement.
  - Provide shims during migration to minimize churn.

Deliverables
- String helpers in `strings.*` used across init and table code.
- ADR describing deprecation of Pascal string usage and replacement strategy.
- Plan for WPText/Paige replacement, including target data formats and API changes.
- Unit tests: constants, text conversions, and round-trips in UTF.

Milestones
1. Helpers in place; init code refactored to safe C↔bigstring usage.
2. Internal API candidates identified and migrated to `const char*` + size.
3. WPText/Paige audit complete; replacement design accepted.
4. Incremental rollout of UTF-based rich text; deprecate Paige in headless and, later, full UI.

Gates
- Phase 2: UI boundaries established (so text rendering stays behind adapters).
- Phase 3: Hash tables modernized to accommodate higher churn without perf regressions.

