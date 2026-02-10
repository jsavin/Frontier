# Phase 2: UTF-8 Infrastructure

**Status**: Planned
**Risk**: Low
**Breakage**: None
**Depends On**: Phase 1 (Bridging & Safety)
**Can Parallel With**: Phase 3 (Hashtable Modernization)

---

## Goal

Build the UTF-8 utility layer that all subsequent phases depend on. This phase adds new code — it doesn't change any existing behavior. The utilities must be available before the core runtime (Phase 4) or UserTalk surface (Phase 6) can switch to UTF-8.

---

## Deliverables

### 1. UTF-8 Validation

```c
// Validate that a buffer contains well-formed UTF-8
boolean utf8_validate(const char *buf, size_t len);

// Validate and return the number of codepoints
boolean utf8_validate_count(const char *buf, size_t len, size_t *codepoint_count);
```

Must handle:
- Overlong encodings (reject)
- Surrogate halves (reject)
- Invalid continuation bytes (reject)
- Truncated sequences at buffer end (reject)
- Valid 1-4 byte sequences (accept)

### 2. Codepoint Iteration

```c
// Decode one codepoint, advance pointer. Returns 0 on end/error.
uint32_t utf8_next_codepoint(const char **pos, const char *end);

// Count codepoints in a UTF-8 string
size_t utf8_codepoint_count(const char *buf, size_t byte_len);

// Advance by N codepoints, return byte offset
size_t utf8_advance_codepoints(const char *buf, size_t byte_len, size_t n);
```

### 3. Case Mapping

Replace the existing `lowercasetable[256]` (a 256-byte ASCII/MacRoman lookup table in `strings.c:1061`) with Unicode-aware case mapping.

**Decision required**: Scope of Unicode case mapping.

| Option | Pros | Cons |
|--------|------|------|
| ASCII-only fast path + full Unicode via library | Simple, fast for common case | Dependency |
| Hand-rolled Basic Latin + Latin-1 Supplement | No dependency, covers Western European | Incomplete for CJK, Cyrillic, etc. |
| ICU subset (case mapping tables only) | Complete, correct | Large dependency |
| OS-native (CFString on macOS) | No bundled dependency | Platform-specific, not portable to headless |

**Recommendation**: ASCII-only fast path for the scanner/tokenizer (identifiers are ASCII), plus a vendored Unicode case-folding table for `string.upper()` / `string.lower()` verbs. This avoids a heavy dependency while covering the UserTalk-facing surface correctly.

### 4. Character Classification

Replace byte-oriented `isalpha(c)` / `isdigit(c)` in `langscan.c` with:
- **Tokenizer**: Keep ASCII-only classification (UserTalk identifiers are ASCII)
- **String verbs**: Use codepoint-aware classification for `string.isAlpha()`, etc.

### 5. Byte Length vs. Character Count

Establish the convention throughout the codebase:
- `size_t` for byte lengths (existing pattern)
- New `utf8_codepoint_count()` for character counts
- Document clearly in function signatures which is which

---

## Key Files

| File | Role |
|------|------|
| `Common/source/strings.c` | `lowercasetable[256]` initialization (line ~1061) |
| `Common/source/langscan.c` | Tokenizer character classification (lines 44-115) |
| New: `Common/source/utf8.c` | UTF-8 utility implementation |
| New: `Common/headers/utf8.h` | UTF-8 utility declarations |

---

## Open Questions

1. **Unicode version**: Which Unicode version to target? (Recommend: latest stable at time of implementation)
2. **Normalization**: Do we need NFC/NFD normalization? (Probably not for Phase 2; defer to Phase 4 or later)
3. **Locale sensitivity**: Is case mapping locale-aware (Turkish İ/i problem)? (Recommend: no, use simple Unicode case folding)

---

## Verification

- Unit tests for UTF-8 validation with known-good and known-bad sequences
- Unit tests for codepoint iteration over multi-byte characters (Latin accents, emoji, CJK)
- Unit tests for case mapping covering ASCII, Latin Extended, and edge cases
- Existing test suite still passes (no behavioral changes to existing code)
- Benchmark: case mapping performance vs. `lowercasetable[]` lookup (should be comparable for ASCII)

---

## Risks

**Low**. This phase adds new utilities without changing existing behavior. The main risk is getting the UTF-8 validation wrong (accepting invalid sequences or rejecting valid ones). Comprehensive test vectors from the Unicode Consortium mitigate this.

The library/dependency decision for case mapping should be made early, as it affects Phase 4 and Phase 6.
