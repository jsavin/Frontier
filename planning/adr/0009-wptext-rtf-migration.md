# ADR 0009 — WPText → RTF Migration

Status
- Proposed
- Date: 2025-09-30

Context
- Legacy `WPText` objects have a ~32KB constraint and platform-specific rendering assumptions. Modern interchange favors RTF/HTML.

Decision
- Represent `WPText` as RTF as the canonical internal form (Unicode/UTF‑8 aware) with minimal metadata (e.g., creator, last converted).
- Provide conversion tools and runtime verbs for `WPText (legacy) ↔ RTF`; add a “save as .rtf” path for objects via CLI and API.
- Maintain a compatibility reader/writer for legacy `WPText`; new writes prefer RTF and mark legacy fields as deprecated.

Consequences
- Requires adapters in runtime and tests for round-trip fidelity; migration tooling for existing DB content.
- Parser/runtime must stop assuming 32KB limits; update storage and APIs accordingly.

Links
- planning/Frontier_Refactoring_Plan.md
- planning/phase4/string_and_text_modernization.md (when available)
