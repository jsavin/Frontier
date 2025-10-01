# Decision Needed: File I/O & Path Policy (ADR-0007)

Context
- Legacy native path formats are non-portable; we need deterministic, cross-platform path handling.

Proposed Decision
- Internally canonicalize to POSIX-style; accept legacy inputs at boundaries.
- Support project-root relative paths; add alias/ID registry later.

Alternatives
- Legacy-native per platform (incompatible across OSes).
- URI scheme first (larger initial lift).

Inputs Needed
- Root discovery and storage design (config vs DB).
- Migration approach for embedded paths.

Acceptance Criteria
- Resolver accepts native + POSIX; canonical internal form documented.
- Tests for round-trip and move/rename scenarios.

Links
- planning/adr/0007-file-io-and-path-policy.md
- planning/database_path_canonicalization.md
- planning/DECISIONS.md
