# Phase 4: Core Runtime Conversion

**Status**: Planned
**Risk**: High
**Breakage**: None (internal representation change only)
**Depends On**: Phase 1 (Bridging & Safety), Phase 2 (UTF-8 Infrastructure)

---

## Goal

Convert Frontier's internal string representation from MacRoman Pascal strings to UTF-8. After this phase, all string data flowing through the runtime is UTF-8. External interfaces (database I/O, UserTalk surface) are handled in later phases.

This is the most invasive phase of the text modernization effort. It touches the core string library, the value system, the scanner/tokenizer, and every subsystem that processes string data.

---

## Scope

### What Changes

1. **Handle-based strings** (`Handle stringvalue` in `tyvaluedata`): Contents treated as UTF-8 instead of MacRoman
2. **String operations** in `strings.c`: Word/field/token logic becomes codepoint-aware where needed
3. **Scanner** (`langscan.c`): Multi-byte awareness for string literals (identifiers stay ASCII-only)
4. **Value system** (`langvalue.c`): String comparisons, concatenation, coercion use UTF-8 semantics
5. **Encoding conversion**: Replace Carbon TEC (Text Encoding Converter) calls with Phase 2 utilities

### What Doesn't Change (Yet)

- **`bigstring` type and API signatures**: Still used at legacy boundaries; conversion happens at the boundary
- **On-disk format**: Still v7 with Pascal string keys (Phase 5)
- **UserTalk verb semantics**: Still byte-oriented for `string.length()`, `string.mid()` etc. (Phase 6)
- **Hash table key format**: Still Pascal strings in `hashkey[]` (Phase 5)

---

## Implementation Strategy

### Layer 1: String Library (`strings.c`)

The ~100 functions in `strings.c` fall into three categories:

**Keep as-is (byte-safe operations)**:
- `copystring()`, `pushstring()`, `deletestring()`, `midstring()` — operate on raw bytes, work fine with UTF-8
- `equalstrings()` — byte comparison is correct for UTF-8 if both sides are normalized (or if we don't normalize)

**Needs UTF-8 awareness**:
- `allupper()`, `alllower()` — must use Phase 2 case mapping utilities instead of `lowercasetable[]`
- `equalidentifiers()` — case-insensitive comparison needs codepoint-aware folding
- `comparestrings()` — ordering may need to be locale-aware or at minimum codepoint-aware
- Word/field operations (`firstword`, `nthword`, `countwords`, etc.) — delimiter scanning works on bytes, but should document UTF-8 safety

**Replace or wrap**:
- `copyctopstring()` / `copyptocstring()` — keep but document they're for legacy boundary use only
- Add new `cstring_*` family operating directly on `const char *` + length

### Layer 2: Value System (`langvalue.c`)

The `tyvaluerecord` union stores strings as `Handle stringvalue`. Currently these contain MacRoman bytes.

**Change**: After Phase 4, all newly-created string values contain UTF-8 bytes.

**Migration path for existing data**: When a string value is loaded from a v7 database (MacRoman), it gets transcoded to UTF-8 on load. This is the boundary conversion.

Key functions to update:
- `setstringvalue()` — document that input must be UTF-8
- `coercetostring()` — ensure all type→string conversions produce UTF-8
- `stringcompare()` — use UTF-8-aware comparison

### Layer 3: Scanner/Tokenizer (`langscan.c`)

The tokenizer reads source code character by character using byte-oriented `isalpha`/`isdigit` checks.

**UserTalk identifiers are ASCII-only** — this is a language constraint, not an encoding limitation. The scanner can continue using ASCII classification for identifiers.

**String literals** need UTF-8 awareness:
- String literals between double quotes can contain arbitrary UTF-8 text
- The scanner already treats them as raw bytes between delimiters — this works correctly with UTF-8
- No change needed for the scanner itself, but downstream processing of string literal values must expect UTF-8

### Layer 4: Encoding Conversion

Replace all Carbon TEC (Text Encoding Converter) calls:
- `strings.c:2269-2338` — TEC-based conversion functions
- Replace with Phase 2 UTF-8 utilities + lookup tables for MacRoman↔UTF-8 conversion
- Platform-specific hotspots: `filedialog.c`, `filepath.c`, `fileops.m` use `kCFStringEncodingMacRoman` — update to `kCFStringEncodingUTF8`

---

## Transition Pattern

During Phase 4, the codebase is in a mixed state. The boundary between "old" (MacRoman/Pascal) and "new" (UTF-8/C string) must be explicit.

**Convention**:
- Functions accepting UTF-8 input document this in their signature or comment
- Boundary conversion uses `bs_from_c()` / `c_from_bs()` (Phase 1 helpers)
- New internal functions take `const char *utf8, size_t byte_len` instead of `bigstring`
- Legacy wrappers call the new internal functions after conversion

**Example pattern**:
```c
// New internal function (UTF-8)
boolean setstringvalue_utf8(const char *utf8, size_t len, tyvaluerecord *val);

// Legacy wrapper (Pascal string, MacRoman)
boolean setstringvalue(bigstring bs, tyvaluerecord *val) {
    char utf8[512];
    size_t len = macroman_to_utf8(stringbaseaddress(bs), stringlength(bs), utf8, sizeof(utf8));
    return setstringvalue_utf8(utf8, len, val);
}
```

---

## Key Files

| File | Changes |
|------|---------|
| `Common/source/strings.c` | Case conversion, comparison, word operations → UTF-8 aware |
| `Common/source/langvalue.c` | String value creation, comparison, coercion |
| `Common/source/langscan.c` | Verify string literal handling is UTF-8 safe |
| `Common/source/stringverbs.c` | Encoding conversion verb implementations |
| `Common/source/langhtml.c` | HTML entity encoding (239 BIGSTRING usages) |
| `Common/source/langxml.c` | XML encoding handling |
| `Common/source/iso8859.c` | ISO 8859 conversion tables |
| `portable/text_encoding_portable.c` | Portable encoding layer |

---

## Verification

- All existing tests pass (behavioral equivalence for ASCII content)
- New tests with non-ASCII content: Latin accents, CJK characters, emoji
- Round-trip tests: create UTF-8 string value → coerce to other types → coerce back → verify
- Encoding boundary tests: load MacRoman data from v7 database → verify correct UTF-8 in memory
- Performance benchmarks: string operations should not regress for ASCII-only content

---

## Risks

**High**. This phase touches the most code and has the highest potential for subtle bugs.

1. **Silent encoding mismatches**: If one subsystem sends MacRoman while another expects UTF-8, strings appear garbled but don't crash. Hard to detect without explicit validation.
   - **Mitigation**: Add `utf8_validate()` asserts in debug builds at key boundaries.

2. **Performance regression**: UTF-8 operations (codepoint iteration, case mapping) are slower than byte-level operations for non-ASCII text.
   - **Mitigation**: Keep ASCII fast paths. Most Frontier text is ASCII; the slow path only activates for non-ASCII codepoints.

3. **Incomplete conversion**: Missing a conversion site means some code path still produces MacRoman, creating mixed-encoding data.
   - **Mitigation**: Systematic audit of all encoding-producing sites. Runtime validation in debug builds.

4. **Scanner correctness**: If the tokenizer mishandles UTF-8 in string literals (e.g., a continuation byte looks like a quote character), scripts break silently.
   - **Mitigation**: UTF-8 continuation bytes (0x80-0xBF) never match ASCII characters (0x00-0x7F), so ASCII delimiter scanning is inherently UTF-8 safe. This is a design property of UTF-8.
