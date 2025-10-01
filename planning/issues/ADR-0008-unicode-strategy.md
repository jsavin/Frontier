# Decision Needed: Unicode Strategy (ADR-0008)

Context
- Frontier predates Unicode and assumes 8-bit ASCII in strings, DB storage, scripts, and paths.

Proposed Decision
- Adopt UTF-8 internally; normalize to NFC at ingress with a compatibility bypass when byte preservation is required.
- Prefer UTF-8 serialization (with encoding tag when possible); treat paths as UTF-8 internally.

Alternatives
- Keep 8-bit for core (blocks internationalization; complicates modern OS integration).
- UTF-16 internally (heavier; complicates interop with existing code).

Inputs Needed
- List of APIs affected by encoding, comparisons, hashing, and tokenization.
- Normalization impact on script semantics and DB keys.

Acceptance Criteria
- Encoding policy documented and enforced in string APIs.
- Tests covering normalization, case-folding, and path handling.

Links
- planning/adr/0008-unicode-strategy.md
- planning/DECISIONS.md
