# Phase 6: UserTalk Compatibility

**Status**: Planned
**Risk**: High
**Breakage**: **Yes — UserTalk-level breaking changes**
**Depends On**: Phase 4 (Core Runtime Conversion)
**Can Parallel With**: Phase 5 (Database Format v8)

---

## Goal

Update the UserTalk scripting surface to work correctly with UTF-8 text. This is where the **acceptable breaking changes** happen. The strategy: preserve backward compatibility where possible, add new codepoint-aware verb variants, and change semantics only where the old behavior would be incorrect under UTF-8.

### Breakage Policy

Text encoding and timezone handling are the only two areas where UserTalk-level breaking changes are acceptable in Frontier's modernization. This is a deliberate investment to move the platform forward. The migration path must be:
- **Well-documented**: Clear guide for script authors
- **Tooling-supported**: Script analyzer to flag affected patterns
- **Gradual**: Deprecation warnings before removal

---

## Verb-Level Changes

### Preserved (Backward Compatible)

These verbs keep their existing byte-oriented semantics:

| Verb | Current Behavior | After UTF-8 | Notes |
|------|-----------------|-------------|-------|
| `string.length(s)` | Byte count | Byte count | Unchanged — returns bytes, not characters |
| `string.mid(s, pos, count)` | Byte offset | Byte offset | Unchanged — operates on bytes |
| `string.nthChar(s, n)` | Byte at position | Byte at position | Unchanged |
| `string.delete(s, pos, count)` | Byte offset | Byte offset | Unchanged |
| `string.insert(s, src, pos)` | Byte offset | Byte offset | Unchanged |
| `string.replace(s, old, new)` | Byte match | Byte match | Works correctly with UTF-8 |
| `string.replaceAll(s, old, new)` | Byte match | Byte match | Works correctly with UTF-8 |
| `string.urlEncode(s)` | Byte-level | Byte-level | Correct for UTF-8 per RFC 3986 |
| `string.urlDecode(s)` | Byte-level | Byte-level | Correct for UTF-8 |

### New Verb Variants (Codepoint-Aware)

| New Verb | Description | Example |
|----------|-------------|---------|
| `string.charCount(s)` | Number of Unicode codepoints | `string.charCount("cafe\u0301")` → 5 |
| `string.chars(s, start, count)` | Substring by codepoint offset | `string.chars("hello", 2, 3)` → "llo" |
| `string.nthCodepoint(s, n)` | Codepoint at position | `string.nthCodepoint("cafe\u0301", 4)` → 769 |

### Changed Semantics (Breaking)

| Verb | Old Behavior | New Behavior | Impact |
|------|-------------|-------------|--------|
| `string.upper(s)` | `lowercasetable[]` byte lookup | Unicode case mapping | Correct for non-ASCII; may change results for MacRoman-specific chars |
| `string.lower(s)` | `lowercasetable[]` byte lookup | Unicode case mapping | Same as above |
| `string.isAlpha(s)` | ASCII/MacRoman byte check | Unicode category check | More characters recognized as alphabetic |
| `string.isNumeric(s)` | ASCII digit check | Unicode digit check (or keep ASCII-only) | Decision: keep ASCII-only for safety |
| `string.firstWord(s)` | ASCII whitespace split | Unicode whitespace split | More whitespace chars recognized |
| `string.compareTo(s1, s2)` | Byte ordering | UTF-8 byte ordering (= Unicode codepoint order for BMP) | Different order for non-ASCII chars |

### Encoding Conversion Verbs (Behavioral Change)

| Verb | Old Behavior | New Behavior |
|------|-------------|-------------|
| `string.macRomanToUtf8(s)` | MacRoman → UTF-8 conversion | **Identity/no-op** when source is already UTF-8; kept for reading legacy data |
| `string.utf8ToMacRoman(s)` | UTF-8 → MacRoman conversion | Still functional (lossy for chars outside MacRoman) |
| `string.latinToMac(s)` | ISO 8859-1 → MacRoman | Deprecated with warning; converts to UTF-8 instead |
| `string.macToLatin(s)` | MacRoman → ISO 8859-1 | Deprecated with warning |
| `string.utf8ToAnsi(s)` | UTF-8 → Windows CP1252 | Still functional (lossy) |
| `string.ansiToUtf8(s)` | Windows CP1252 → UTF-8 | **Identity/no-op** when source is already UTF-8 |

### Deprecated Verbs

These verbs will emit runtime deprecation warnings and eventually be removed:

- `string.latinToMac()` / `string.macToLatin()` — MacRoman is no longer the internal encoding
- `string.utf16ToAnsi()` / `string.ansiToUtf16()` — ANSI/UTF-16 interop is legacy Windows-specific

---

## typeof() Considerations

**typeof() returns OSType codes and must NOT change.** (See `docs/usertalk/TYPEOF.md` for the historical landmine.) String type is `'TEXT'` regardless of encoding. The encoding is a property of the runtime, not the type system.

---

## Migration Tooling

### Script Analyzer

A static analysis tool that scans UserTalk scripts for patterns likely broken by the UTF-8 transition:

**Flags**:
- `string.length()` used in arithmetic with `string.mid()` — may assume byte=character
- `string.nthChar()` used in loops — may assume single-byte characters
- `string.upper()` / `string.lower()` on variables that may contain non-ASCII text
- Calls to deprecated encoding verbs (`latinToMac`, `macToLatin`, etc.)

**Output**: Warning list with line numbers and suggested replacements.

### Migration Guide

Documentation for script authors covering:
1. What changed and why
2. Which verbs have new behavior
3. How to update scripts that use byte-offset string operations on non-ASCII text
4. New codepoint-aware alternatives
5. Encoding verb changes

---

## Key Files

| File | Changes |
|------|---------|
| `Common/source/stringverbs.c` | Verb implementations (2347 lines) |
| `tests/headless_string_verbs.c` | Test stubs for string verbs |
| `planning/phase3/processor_audits/string.md` | Verb inventory reference (60 verbs) |
| `docs/usertalk/docserver/string/` | DocServer verb documentation |

---

## Verification

- All Tier 1 (byte-safe) verb tests pass unchanged
- New codepoint-aware verbs tested with multi-byte input (Latin accents, CJK, emoji)
- Case conversion tests with known Unicode test cases
- Encoding conversion verbs: identity behavior when input is already UTF-8
- Deprecation warnings appear in script output for deprecated verbs
- Script analyzer correctly flags known-affected patterns in test scripts
- Existing UserTalk scripts in system.verbs run without errors (smoke test)

---

## Risks

**High**. This is where real user-facing breakage occurs.

1. **Script breakage**: Production UserTalk scripts that assume byte=character will produce wrong results for non-ASCII text. The byte-safe preserved verbs mitigate this — scripts that only used `string.length()` and `string.mid()` continue to work correctly on bytes.

2. **Case conversion changes**: `string.upper("ü")` changes from MacRoman table lookup to Unicode case mapping. The result is the same for common Latin characters, but edge cases may differ.

3. **Silent breakage**: Scripts that worked "by accident" with MacRoman may fail silently with UTF-8. The script analyzer helps detect these, but can't catch all cases.

4. **Ecosystem impact**: Third-party scripts, shared databases with scripts — all potentially affected. Clear documentation and a long deprecation window are essential.

**Mitigation**: Ship codepoint-aware verbs and deprecation warnings in a release *before* changing default semantics. Give script authors time to update.
