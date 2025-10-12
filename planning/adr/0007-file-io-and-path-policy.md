# ADR 0007 — File I/O & Path Policy

Status
- Draft (Proposed)
- Date: 2025-09-30

Context
- Legacy native path formats are not portable.

Decision (Draft)
- Internally canonicalize to POSIX-style paths; accept legacy inputs at boundaries.
- Support relative paths from a configured project root; add alias/ID registry later.

Consequences
- Improves portability and determinism; requires migration tools for embedded paths.

Links
- planning/database_path_canonicalization.md

