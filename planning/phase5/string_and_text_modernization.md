# String and Text Modernization (Phase 4)

Status
- State: Planned
- Last Updated: 2025-09-29

Context
- Frontier uses Classic Mac OS-style Pascal strings (`bigstring`) pervasively.
- Rich text flows rely on Paige and legacy WPText assumptions.
- Long-term maintainability and interoperability benefit from UTF-based, C-string-centric APIs.

Principles
- Avoid long-term preservation of Pascal concepts in modern internals.
- Prefer UTF-8 C strings for new/modernized code; convert at legacy boundaries.
- Provide clear helpers for safe bridging during the transition.

Near-Term Actions
- Add safe helpers (done):
  - `bs_from_c(const char*, bigstring)` and `c_from_bs(const bigstring, char*, unsigned long)`
  - Use in init code (constants/keywords/builtins) and table operations.
- Sweep and replace unsafe `(ptrstring)"literal"` casts (use BIGSTRING for true Pascal literals or `bs_from_c`).

Medium-Term Actions
- Identify internal APIs that can take `const char*` + size.
- Introduce shims so caller code can pass C strings while legacy code still receives bigstrings.
- Standardize UTF-8 as the default encoding for internal text; convert as needed at boundaries.

WPText / Paige Modernization
- Audit all uses of Paige and legacy rich text.
- Define a UTF-based WPText model that is UI-agnostic and headless-friendly.
- Provide adapters for UI layers; progressively remove Paige dependencies.

Testing
- Add unit tests for:
  - Constants and tokenizer behavior with C-string-based flows.
  - C↔Pascal conversion correctness (`bs_from_c` / `c_from_bs`).
  - UTF-8 text round-trips in scripting and OPML conversion.

Risks / Considerations
- On-disk formats and tokenizer may still require Pascal strings; keep conversions at I/O boundaries until those are modernized.
- Ensure performance is maintained when shifting to C strings; use explicit lengths to avoid O(n) scans.

Deliverables
- ADR for Pascal string deprecation and UTF adoption.
- Updated APIs and adapters for UI and headless text flows.
- Migration guide for contributors.

