# Decision Needed: WPText → RTF Migration (ADR-0009)

Context
- Legacy WPText objects have ~32KB limits and platform-specific rendering; we need modern, portable rich text.

Proposed Decision
- Use RTF (Unicode-aware) as the canonical internal form with minimal metadata.
- Provide conversion tools and verbs for legacy WPText ↔ RTF; support “save as .rtf” via CLI/API.
- Maintain a compatibility reader/writer for legacy WPText; new writes prefer RTF.

Alternatives
- Keep WPText format (limits and portability issues remain).
- HTML as canonical (better for web, harder for parity with legacy editors).

Inputs Needed
- Required fidelity vs. legacy WPText features.
- Storage changes for larger text sizes.

Acceptance Criteria
- Round-trip tests WPText ↔ RTF with high fidelity.
- Ability to export WPText objects to .rtf files.

Links
- planning/adr/0009-wptext-rtf-migration.md
- planning/DECISIONS.md
