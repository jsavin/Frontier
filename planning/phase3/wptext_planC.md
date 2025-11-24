# WPText Plan C – Plain Text Extraction Strategy

Status
- State: Archived / Superseded
- Phase: Paige Migration
- Last Updated: 2025-11-20
- Owner: Codex
- Notes: Historical record of the plain-text fallback (Plan C). Actual implementation now ships the richer Paige extractor per `planning/phase3/paige_text_extractor.md`.

**Context**  
- Date: November 17, 2025  
- Owner: Codex (headless runtime)  
- Related goals: v6 → v7 migration, wptext portability

## Problem Statement
The current headless runtime still tries to load Paige’s document model whenever a `wptext` external is encountered. Even after we bypassed doc-info, the migrator crashes inside the Paige importer/exporter because the headless build never provided a safe `wp_portable_prepare_for_pack` implementation. We need a deterministic, low-risk way to migrate wptext data without relying on Paige’s full runtime and without hand-editing documents.

## Decision
Adopt “Plan C”: extract plain UTF-8 text from Paige-format wptext blobs (analogous to the WordSolutions fallback) and store only the unformatted text in v7. All formatting, rulers, and doc-info are intentionally dropped. Future tooling can rehydrate formatting if someone reimplements the Paige runtime, but the migration path stays automated.

## Execution Plan (Simplified)
### 1. Grab The Text
- Reuse the legacy `wpengine` helpers: load the stored wptext blob, let `wpunpacktext`/`wpgettexthandle` concatenate the characters, and copy them into a UTF‑8 buffer. This mirrors what `string(adrWptext^)` already does—no formatting, no doc-info, just bytes.

### 2. Feed It To The Migrator
- Introduce a tiny helper (`wp_plaintext_from_external`) that returns the UTF‑8 blob for any `wptext` external.  
- In `langhash_prepare_wordprocessor_value`, detect `wptext`, call the helper, and replace the external with the returned string (or stash the UTF‑8 payload if we still need an external wrapper). The migrator now packs plain text only.

### 3. Verify & Note The Change
- Rebuild/tests to confirm the runtime no longer crashes during wptext packing.  
- Document in `_CURRENT_STATUS.md` (done) and relevant headers that `wptext` objects are downgraded to plain text in headless builds so future work doesn’t chase formatting bugs again.

## Risks & Mitigations
- **Loss of formatting:** Intentional; documented and communicated so downstream users know to expect plain text only.
- **Legacy parser drift:** We rely on `wpengine`’s existing code path, which already handled Paige-style text for macOS builds. Minimal new code, so low risk.
- **Future features needing Paige:** The code remains structured so a future “Plan E” (full Paige restore) could reintroduce formatting by swapping implementations behind `wp_portable_prepare_for_pack`.
