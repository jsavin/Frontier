# ADR 0008 — Unicode Strategy

Status
- Proposed
- Date: 2025-09-30

Context
- Frontier predates Unicode and assumes 8-bit ASCII in many subsystems (strings, DB storage, scripts, paths).

Decision
- Adopt UTF-8 as the internal string encoding for headless/runtime and CLI/server paths.
- Normalize incoming text to NFC at parse/ingress; provide a compatibility mode to bypass normalization for byte-preserving migrations.
- Prefer UTF-8 serialization with an encoding tag where the format allows; otherwise document UTF‑8 as default and upgrade readers to auto-detect.
- Treat file paths as UTF‑8 internally; add platform boundary shims as needed.

Consequences
- Requires audits of string APIs, tokenization, hashing, and comparisons; adds normalization tests.
- Define locale‑independent case‑folding and identifier rules for language tokens and table keys.

Links
- planning/Frontier_Refactoring_Plan.md
- planning/0.5.13_usertalk_language_summary.md
