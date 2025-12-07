# Processor Audit: `string`

**Status:** ✅ Ready for Implementation (Straightforward - 60 verbs)
**Audit Date:** 2025-12-05
**Auditor:** Claude (Sonnet 4.5)

---

## Basic Information

| Property | Value |
|----------|-------|
| **Processor Name** | `string` |
| **EFP ID** | Unknown (lang block) |
| **Verb Count** | 60 |
| **Window Required** | NO |
| **Documentation** | [string/](../../../docs/usertalk/docserver.userland.com/string/index.html) |
| **Stub Implementation** | [headless_string_verbs.c](../../../tests/headless_string_verbs.c) |
| **Existing Implementation** | [stringverbs.c](../../../Common/source/stringverbs.c) ✅ (2347 lines!) |

---

## Category Assessment

**Category:** ✅ **Core Functionality**

**Rationale:**
String manipulation is fundamental to any scripting environment. Provides comprehensive text operations including parsing, formatting, encoding conversion, and URL handling. No GUI dependencies, though some operations are encoding-sensitive.

**Headless Compatibility:** ✅ **Full**

**Blocking Verbs:** None

---

## Character Encoding Strategy (Simplified)

**Bigstring Type:**
- Frontier's native string type is **bigstring** (Pascal Str255 format)
- Fixed 256-byte buffer with first byte indicating length
- Strings are treated as **byte buffers** (no inherent character set metadata)
- No enforcement of character set in storage—caller must track encoding

**Encoding Conversion Approach:**
- **Conversion verbs are simple in-place transformations:**
  - `latinToMac` / `macToLatin` - Mac Roman ↔ ISO Latin-1
  - `utf8ToAnsi` / `ansiToUtf8` - UTF-8 ↔ Windows ANSI (with assumptions)
  - `utf16ToAnsi` / `ansiToUtf16` - UTF-16 ↔ Windows ANSI
  - `macRomanToUtf8` / `utf8ToMacRoman` - Mac Roman ↔ UTF-8
- Verbs assume caller knows the source encoding and converts in-place
- No charset metadata in stringType; caller is responsible for tracking

**Long-Term Vision (Phase 3+):**
- Convert all bigstrings to UTF-8 internally by default
- Then conversion verbs become no-ops (or simple codepath shortcuts)
- Eventually: extend stringType with charset metadata (more complex; defer for now)

**Implementation Strategy:**
1. **Tier 1**: Implement byte-safe operations (encoding-agnostic)
2. **Tier 2**: Implement character-sensitive operations (document encoding assumptions)
3. **Tier 3**: Implement encoding converters (simple in-place transforms, no platform-specific complexity)
4. **Defer**: HTML/HTTP-specific verbs (processhtmlmacros, davenetmassager)

---

## Verb Inventory by Category

### Tier 1: Byte-Safe String Operations (22 verbs)
**Encoding-agnostic - work on any byte sequence**

| # | Verb Name | Signature | Description |
|---|-----------|-----------|-------------|
| 1 | `length` | `string.length(s) -> long` | Get byte count (NOT character count!) |
| 2 | `mid` | `string.mid(s, start, count) -> string` | Extract substring by byte offset |
| 3 | `nthChar` | `string.nthChar(s, n) -> char` | Get byte at position (NOT Unicode char!) |
| 4 | `delete` | `string.delete(s, start, count) -> string` | Delete bytes |
| 5 | `insert` | `string.insert(dest, src, pos) -> string` | Insert bytes |
| 6 | `popLeading` | `string.popLeading(s, char) -> string` | Remove leading bytes |
| 7 | `popTrailing` | `string.popTrailing(s, char) -> string` | Remove trailing bytes |
| 8 | `trimWhiteSpace` | `string.trimWhiteSpace(s) -> string` | Trim ASCII whitespace |
| 9 | `popSuffix` | `string.popSuffix(s, suffix) -> string` | Remove suffix if present |
| 10 | `hasSuffix` | `string.hasSuffix(s, suffix) -> boolean` | Check for suffix |
| 11 | `replace` | `string.replace(s, old, new) -> string` | Replace first occurrence |
| 12 | `replaceAll` | `string.replaceAll(s, old, new) -> string` | Replace all occurrences |
| 13 | `multipleReplaceAll` | `string.multipleReplaceAll(s, table) -> string` | Multiple replacements |
| 14 | `filledString` | `string.filledString(char, count) -> string` | Create repeated byte |
| 15 | `hex` | `string.hex(n) -> string` | Convert number to hex |
| 16 | `padWithZeros` | `string.padWithZeros(n, width) -> string` | Zero-pad number |
| 17 | `addCommas` | `string.addCommas(n) -> string` | Add thousand separators |
| 18 | `urlEncode` | `string.urlEncode(s) -> string` | URL percent-encoding |
| 19 | `urlDecode` | `string.urlDecode(s) -> string` | URL percent-decoding |
| 20 | `iso8859encode` | `string.iso8859encode(s) -> string` | Encode for ISO-8859-1 |
| 21 | `hashMD5` | `string.hashMD5(s) -> string` | MD5 hash (use crypt.MD5 instead!) |
| 22 | `ellipsize` | `string.ellipsize(s, maxlen) -> string` | Truncate with "..." |

### Tier 2: Character-Sensitive Operations (15 verbs)
**Encoding-dependent - assume specific character sets**

| # | Verb Name | Signature | Description |
|---|-----------|-----------|-------------|
| 23 | `upper` | `string.upper(s) -> string` | Uppercase (encoding-dependent!) |
| 24 | `lower` | `string.lower(s) -> string` | Lowercase (encoding-dependent!) |
| 25 | `isAlpha` | `string.isAlpha(s) -> boolean` | Check if alphabetic |
| 26 | `isNumeric` | `string.isNumeric(s) -> boolean` | Check if numeric |
| 27 | `isPunctuation` | `string.isPunctuation(s) -> boolean` | Check if punctuation |
| 28 | `firstWord` | `string.firstWord(s) -> string` | Extract first word |
| 29 | `lastWord` | `string.lastWord(s) -> string` | Extract last word |
| 30 | `nthWord` | `string.nthWord(s, n, sep) -> string` | Extract nth word |
| 31 | `countWords` | `string.countWords(s) -> long` | Count words |
| 32 | `setWordChar` | `string.setWordChar(char)` | Set word separator |
| 33 | `getWordChar` | `string.getWordChar() -> char` | Get word separator |
| 34 | `nthField` | `string.nthField(s, sep, n) -> string` | Extract field |
| 35 | `countFields` | `string.countFields(s, sep) -> long` | Count fields |
| 36 | `commentDelete` | `string.commentDelete(s) -> string` | Remove // and /* */ comments |
| 37 | `firstSentence` | `string.firstSentence(s) -> string` | Extract first sentence |

### Tier 3: Encoding Conversion Verbs (8 verbs)
**Complex - platform-specific character set conversions**

| # | Verb Name | Signature | Description |
|---|-----------|-----------|-------------|
| 38 | `latinToMac` | `string.latinToMac(s) -> string` | ISO Latin-1 → Mac Roman |
| 39 | `macToLatin` | `string.macToLatin(s) -> string` | Mac Roman → ISO Latin-1 |
| 40 | `utf8ToAnsi` | `string.utf8ToAnsi(s) -> string` | UTF-8 → Windows ANSI (CP1252) |
| 41 | `ansiToUtf8` | `string.ansiToUtf8(s) -> string` | Windows ANSI → UTF-8 |
| 42 | `utf16ToAnsi` | `string.utf16ToAnsi(s) -> string` | UTF-16 → Windows ANSI |
| 43 | `ansiToUtf16` | `string.ansiToUtf16(s) -> string` | Windows ANSI → UTF-16 |
| 44 | `macRomanToUtf8` | `string.macRomanToUtf8(s) -> string` | Mac Roman → UTF-8 |
| 45 | `utf8ToMacRoman` | `string.utf8ToMacRoman(s) -> string` | UTF-8 → Mac Roman |

### Tier 4: HTTP/Web/Email Operations (9 verbs)
**May defer or simplify**

| # | Verb Name | Signature | Description |
|---|-----------|-----------|-------------|
| 46 | `parseHttpArgs` | `string.parseHttpArgs(s, @table)` | Parse query string |
| 47 | `urlSplit` | `string.urlSplit(url, ...) -> parts` | Split URL into components |
| 48 | `parseAddress` | `string.parseAddress(addr, @parts)` | Parse email/path address |
| 49 | `dropNonAlphas` | `string.dropNonAlphas(s) -> string` | Remove non-alphanumerics |
| 50 | `innerCaseName` | `string.innerCaseName(s) -> string` | Convert to camelCase |
| 51 | `processHtmlMacros` | `string.processHtmlMacros(html) -> html` | **DEFER** - Complex HTML processing |
| 52 | `davenetMassager` | `string.davenetMassager(s) -> s` | **DEFER** - Legacy DaveNet formatting |
| 53 | `getGifHeightWidth` | `string.getGifHeightWidth(data, @h, @w)` | Extract GIF dimensions |
| 54 | `getJpegHeightWidth` | `string.getJpegHeightWidth(data, @h, @w)` | Extract JPEG dimensions |

### Tier 5: Date/Time Formatting (2 verbs)
**Delegate to date processor?**

| # | Verb Name | Signature | Description |
|---|-----------|-----------|-------------|
| 55 | `timeString` | `string.timeString(time) -> string` | Format time (use date.timeString?) |
| 56 | `dateString` | `string.dateString(date) -> string` | Format date (use date.shortString?) |

### Tier 6: Advanced/Specialized (4 verbs)
**May need special attention**

| # | Verb Name | Signature | Description |
|---|-----------|-----------|-------------|
| 57 | `patternMatch` | `string.patternMatch(pattern, s) -> boolean` | Wildcard matching (* and ?) |
| 58 | `wrap` | `string.wrap(text, width) -> text` | Word-wrap text |
| 59 | `convertCharset` | `string.convertCharset(s, from, to) -> s` | Generic encoding converter |
| 60 | `isCharsetAvailable` | `string.isCharsetAvailable(name) -> boolean` | Check charset support |

**Total:** 60 verbs

---

## Implementation Analysis

### Complexity: **HIGH** (due to encoding complexity)

### Dependencies

- **Other Processors:**
  - `crypt` - string.hashMD5 should delegate to crypt.MD5
  - `date` - timeString/dateString could delegate to date processor
- **External Services:** None (but encoding converters use OS/iconv)
- **OS-Specific Functionality:** YES (character set conversion - iconv on POSIX, MultiByteToWideChar on Windows)
- **GUI/Window Context:** NO
- **Existing Code:** ✅ **Complete implementation in stringverbs.c (2347 lines!)**

### Key Implementation Notes

**Existing Implementation (stringverbs.c):**
```c
static boolean stringfunctionvalue(short token, hdltreenode hparam1,
                                   tyvaluerecord *vreturned, bigstring bserror) {
    // 60+ case statements for all string verbs
    // Already implemented!
}
```

**Character Encoding Tables (lines 51-195):**
- `latintomactable[]` - 256-byte lookup table for ISO Latin-1 → Mac Roman
- `mactolatin[]` - 256-byte lookup table for Mac Roman → ISO Latin-1
- Tables already exist in stringverbs.c!

**Encoding Conversion Implementation:**
- Character set conversion tables already exist in stringverbs.c
- Conversion verbs (`ansiToUtf8`, `utf8ToAnsi`, `macRomanToUtf8`, etc.) do simple byte-level transformations
- Assume caller knows the source encoding and handles conversion in-place
- No platform-specific complexity; straightforward lookup table or byte-mapping operations

**Encoding Behavior (By Design):**

1. **`string.length(s)` returns BYTE count (not character count)**
   - "hello" → 5 bytes
   - "café" (UTF-8) → 5 bytes (c=1, a=1, f=1, é=2 bytes)
   - This is the documented behavior; not a limitation

2. **`string.mid(s, pos, len)` operates on BYTE offsets**
   - Simple byte extraction; works on any encoding
   - Caller is responsible for ensuring valid character boundaries
   - Safe for ASCII and single-byte encodings

3. **`string.upper() / lower()` assume single-byte encoding**
   - Works correctly on ASCII/Latin-1/Mac Roman
   - Will not handle UTF-8 multi-byte characters correctly
   - This is documented behavior; users should use conversion verbs if needed

4. **Word/field parsing uses ASCII delimiters**
   - Standard space/tab/newline whitespace assumptions
   - Works correctly for ASCII and single-byte encodings
   - Documented limitation; no need to "fix" for initial implementation

**URL Encoding:**
- Standard percent-encoding (%20 for space, etc.)
- Works on byte level (safe for any encoding)
- Should percent-encode non-ASCII bytes in UTF-8

**Pattern Matching:**
- Simple wildcard matching (* = any, ? = one char)
- Operates on byte level
- Not regex (we deferred `re` processor)

**Image Metadata Extraction:**
- GIF: Read header bytes for width/height
- JPEG: Parse JFIF/Exif headers
- Byte-oriented operations (encoding-safe)

**Implementation Task:**
1. Port existing stringverbs.c code to kernel verb system (2347 lines already written!)
2. Implement Tier 1: byte-safe operations (straightforward port)
3. Implement Tier 2: character operations (document encoding behavior)
4. Implement Tier 3: encoding converters (simple in-place transformations)
5. Document encoding assumptions for each verb category
6. Consider deprecating hashMD5 (use crypt.MD5 instead)
7. Plan Phase 3+: Convert bigstrings to UTF-8 by default

---

## UserTalk Documentation Notes

From docserver.userland.com/string/:

**Encoding Behavior by Verb Category:**

```usertalk
// Byte-level operations (encoding-independent)
string.length("hello")              → 5 bytes
string.mid("hello", 1, 3)           → "ell"
string.delete("hello", 2, 2)        → "heo"

// Character operations (single-byte encoding only)
string.upper("HELLO")               → "HELLO" (works)
string.upper("café")                → encoding-dependent
// Assumes ASCII/Latin-1/Mac Roman; UTF-8 requires conversion

// Encoding conversions (in-place transformations)
string.utf8ToAnsi("café")           → convert UTF-8 to Windows ANSI
string.ansiToUtf8("café")           → convert Windows ANSI to UTF-8
string.macRomanToUtf8("café")       → convert Mac Roman to UTF-8

// URL encoding (byte-safe)
string.urlEncode("hello world")     → "hello%20world"
string.urlDecode("hello%20world")   → "hello world"

// Pattern matching (byte-level, encoding-independent)
string.patternMatch("*.txt", "file.txt")  → true
string.patternMatch("test*", "testing")   → true

// Field parsing (ASCII separator)
string.nthField("a,b,c", ",", 2)    → "b"
string.countFields("a|b|c", "|")    → 3

// Word parsing (ASCII whitespace)
string.firstWord("hello world")     → "hello"
string.countWords("one two three")  → 3
```

---

## Testing Requirements

**Minimum Test Cases Per Verb:**
- ASCII input (safe baseline)
- Latin-1 input (single-byte extended)
- UTF-8 input (multi-byte - test carefully!)
- Empty string
- Very long strings (>64KB)

**Critical Encoding Tests:**
```usertalk
// Byte-safe operations (should work on any encoding)
string.length("café")               → 5 (UTF-8) or 4 (Latin-1) - both valid!
string.mid("hello", 2, 2)           → "ll" (safe)
string.replace("test", "e", "a")    → "tast" (safe)

// Encoding-sensitive operations (test with known encoding)
string.upper("hello")               → "HELLO" (ASCII - safe)
string.upper("café")                → DEPENDS ON ENCODING!

// Encoding conversions (round-trip tests)
local (latin1 = string.macToLatin(macString))
local (mac = string.latinToMac(latin1))
→ Should round-trip correctly

// URL encoding (byte-safe)
local (encoded = string.urlEncode("hello world!"))
local (decoded = string.urlDecode(encoded))
→ decoded == "hello world!"

// UTF-8 edge cases
string.length("😀")                 → 4 bytes (UTF-8 emoji)
string.mid("😀test", 1, 4)          → "😀" (if lucky) or garbage (if unlucky!)
```

**Platform Differences:**
- Mac Roman vs Windows ANSI default encodings
- UTF-8 conversion APIs (iconv vs Win32)
- Line ending handling (CRLF vs LF)

---

## Implementation Effort

**Estimated Time:** 30-40 hours (LARGE processor!)

**Breakdown:**
- Tier 1 (byte-safe): 8-10 hours (22 verbs)
- Tier 2 (character-sensitive): 6-8 hours (15 verbs)
- Tier 3 (encoding converters): 8-10 hours (8 verbs, complex)
- Tier 4 (HTTP/web): 4-6 hours (9 verbs)
- Tier 5 (date/time): 2 hours (2 verbs, may delegate)
- Tier 6 (advanced): 4-6 hours (4 verbs)
- Testing: 8-10 hours (comprehensive encoding tests)
- Documentation: 2 hours

**Confidence:** MEDIUM - Large codebase exists, but encoding complexity is high

**Phased Implementation:**
1. **Phase 1** (Quick Win): Tier 1 byte-safe operations (22 verbs, ~10 hours)
2. **Phase 2**: Tier 2 character-sensitive operations (15 verbs, ~8 hours)
3. **Phase 3**: Tier 3 encoding converters (8 verbs, ~10 hours)
4. **Phase 4**: Remaining verbs (15 verbs, ~12 hours)

---

## Priority & Sequencing

**Priority:** 🎯 **HIGH** (Tier 1)

**Recommended Implementation Order:** 15 (after crypt)

**Blockers/Prerequisites:**
- Portable text encoding layer (already exists in `text_encoding_portable.c`)
- Character set conversion support (iconv/Win32 APIs)

**Implementation Sequence:**
1. **Start with Tier 1** (byte-safe operations) - easy wins
2. Implement basic string manipulation (mid, delete, insert, replace)
3. Implement formatting (hex, padWithZeros, addCommas)
4. Implement URL encoding/decoding
5. **Move to Tier 2** (character-sensitive)
6. Implement case conversion (upper, lower) with encoding notes
7. Implement character classification (isAlpha, isNumeric)
8. Implement word/field parsing
9. **Move to Tier 3** (encoding converters)
10. Test Mac/Latin conversion tables
11. Implement UTF-8/ANSI converters
12. **Complete remaining tiers**

---

## Quick Win Justification (Tier 1 Only)

**Why Tier 1 Is a Quick Win:**
1. **Essential Utility:** String operations used everywhere
2. **Byte-Safe:** No encoding complexity for Tier 1
3. **Existing Code:** Implementation exists in stringverbs.c
4. **High Value:** Enables basic text processing
5. **No Dependencies:** Standalone functionality

**Value Proposition (Full Processor):**
- Required for all text processing scripts
- Enables URL encoding for HTTP operations
- Supports character set conversions (legacy compatibility)
- Foundation for parsing and formatting
- Critical for web/API operations

---

## Related Processors

- **crypt** - string.hashMD5 should delegate to crypt.MD5
- **date** - timeString/dateString overlap with date processor
- **file** - File path manipulation uses strings
- **tcp** / **http** - URL encoding, query string parsing
- **html** - HTML macro processing (deferred)

---

## Special Considerations

**Character Encoding Hell:**

**The Fundamental Problem:**
- Frontier predates Unicode widespread adoption
- Strings stored as 8-bit byte buffers
- No encoding metadata
- Mixed encodings in same database possible!

**Encoding Assumptions by Platform:**
- **Classic Mac**: Mac Roman (8-bit, not Latin-1)
- **Windows**: Windows ANSI (CP1252, similar to Latin-1 but different)
- **Modern**: UTF-8 (multi-byte, variable-length)
- **Network protocols**: UTF-8 or percent-encoded

**Safe Operations (Any Encoding):**
- Byte-level: length (byte count), mid (byte offsets), delete, insert
- Byte comparison: replace, replaceAll (if needle/replacement are same encoding)
- Binary operations: hex, urlEncode, urlDecode

**Unsafe Operations (Encoding-Dependent):**
- Case conversion: upper, lower (single-byte assumption)
- Character classification: isAlpha, isNumeric (ASCII/Latin-1 assumption)
- Character counting: mid() with character positions (breaks on UTF-8)
- Word parsing: assumes ASCII whitespace

**Mitigation Strategies:**
1. **Document encoding assumptions clearly**
2. **Provide encoding converters** (latinToMac, utf8ToAnsi, etc.)
3. **Add UTF-8 validation** (optional helper functions)
4. **Recommend UTF-8** for new content (but don't enforce)
5. **Test with multiple encodings**

**URL Encoding:**
- Percent-encoding is byte-oriented (safe)
- UTF-8 should be percent-encoded per RFC 3986
- Example: "café" (UTF-8) → "caf%C3%A9"

**Pattern Matching:**
- Simple wildcards: * (any), ? (one)
- NOT regex (re processor deferred)
- Byte-level matching (safe for any encoding)
- Case-sensitive

**Word/Field Separators:**
- Default word separator: space, tab, newline (ASCII)
- Default field separator: specified by caller
- `setWordChar()` allows custom separator (single byte only!)
- Won't work with multi-byte UTF-8 separators

**Hash Function:**
- `string.hashMD5()` duplicates `crypt.MD5()`
- Should deprecate in favor of crypt processor
- Or make it an alias/wrapper

**Date/Time Formatting:**
- `timeString()` / `dateString()` overlap with date processor
- Consider delegating to date processor
- Or keep as simple formatters (compatibility)

**HTML/HTTP Complexity:**
- `processHtmlMacros()` - Complex, may defer
- `davenetMassager()` - Legacy DaveNet specific, may defer
- `parseHttpArgs()` - Useful, implement
- `urlSplit()` - Useful, implement

**Image Metadata:**
- GIF: Parse header for width/height (bytes 6-9)
- JPEG: Parse JFIF/Exif markers (complex!)
- Byte-oriented (encoding-safe)
- Useful for web content

**Performance:**
- Many operations create new string handles
- Frequent allocations for large strings
- Consider optimization later
- Existing code should be reasonable

**Thread Safety:**
- Most verbs are pure functions (safe)
- `setWordChar()` uses global state (NOT thread-safe!)
- Document this limitation

---

## References

**Implementation:**
- Current: `Common/source/stringverbs.c` (2347 lines - complete!)
- Supporting: `Common/source/strings.c`, `strings_extras.c`
- Encoding: `portable/text_encoding_portable.c`
- Headers: `Common/headers/strings.h`
- Stub: `tests/headless_string_verbs.c`

**Documentation:**
- DocServer: `docs/usertalk/docserver.userland.com/string/`

**Standards:**
- RFC 3986: Uniform Resource Identifier (URI) - URL encoding
- ISO 8859-1: Latin-1 character set
- Mac Roman: Apple's 8-bit encoding (historic)
- Windows CP1252: Windows ANSI (Western European)
- UTF-8: Universal encoding (RFC 3629)
- UTF-16: Unicode 16-bit encoding

**Encoding Conversion:**
- POSIX: iconv library
- Windows: MultiByteToWideChar(), WideCharToMultiByte()
- Portable layer: `text_encoding_portable.c`

---

## Next Steps

1. ✅ Audit complete - ready for phased implementation
2. ⏳ **Phase 1**: Implement Tier 1 (byte-safe operations) - 22 verbs
3. ⏳ Test with ASCII, Latin-1, UTF-8 inputs
4. ⏳ Document encoding behavior for each verb
5. ⏳ **Phase 2**: Implement Tier 2 (character-sensitive) - 15 verbs
6. ⏳ Test case conversion with known encodings
7. ⏳ **Phase 3**: Implement Tier 3 (encoding converters) - 8 verbs
8. ⏳ Test Mac/Latin/UTF-8/ANSI conversions
9. ⏳ **Phase 4**: Implement remaining tiers
10. ⏳ Comprehensive encoding tests
11. ⏳ Update implementation status

---

**Audit Status:** ✅ Complete and Approved for Phased Implementation

**Recommendation:** Implement in 4 phases to manage complexity. Start with Tier 1 (byte-safe operations) as a quick win, then tackle encoding-sensitive operations with careful testing.
