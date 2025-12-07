# Processor Audit: `html`

**Status:** ✅ **Fully Headless-Compatible**
**Audit Date:** 2025-12-06
**Auditor:** Claude (Haiku 4.5)

---

## Basic Information

| Property | Value |
|----------|-------|
| **Processor Name** | `html` |
| **EFP ID** | 1021 |
| **Verb Count** | 23 kernel verbs + 5 subprocessor categories (searchengine, mrcalendar, webserver, inetd) |
| **Window Required** | NO |
| **Implementation Type** | Kernel verbs + script utilities |

---

## Category Assessment

**Category:** ✅ **HTML Content Processing & Generation**

**Rationale:**
HTML processor provides utilities for generating, processing, and manipulating HTML content. All operations are text-based with no GUI dependencies. Core functionality for web server operation.

**Headless Compatibility:** ✅ **Full** (23/23 verbs)

---

## Core Verb Inventory (23 verbs)

| Verb | Purpose | Headless |
|------|---------|----------|
| `processMacros` | Expand embedded macros in HTML | ✅ YES |
| `urlDecode` | Decode URL-encoded strings | ✅ YES |
| `urlEncode` | Encode strings for URLs | ✅ YES |
| `parseHttpArgs` | Parse HTTP query strings | ✅ YES |
| `iso8859Encode` | Convert to ISO-8859-1 encoding | ✅ YES |
| `getGifHeightWidth` | Parse GIF header for dimensions | ✅ YES |
| `getJpegHeightWidth` | Parse JPEG header for dimensions | ✅ YES |
| `buildPageTable` | Build page structure from HTML | ✅ YES |
| `refGlossary` | Reference glossary lookup | ✅ YES |
| `getPref` | Get preference value | ✅ YES |
| `getOneDirective` | Get single directive from HTML | ✅ YES |
| `runDirective` | Execute HTML directive | ✅ YES |
| `runDirectives` | Execute multiple directives | ✅ YES |
| `runOutlineDirectives` | Execute directives in outline | ✅ YES |
| `cleanForExport` | Clean HTML for export | ✅ YES |
| `normalizeName` | Normalize element names | ✅ YES |
| `glossaryPatcher` | Patch glossary references | ✅ YES |
| `expandUrls` | Expand URL references | ✅ YES |
| `traversalSkip` | Handle traversal skip logic | ✅ YES |
| `getPageTableAddress` | Get page table address | ✅ YES |
| `neuterMacros` | Disable macro expansion | ✅ YES |
| `neuterTags` | Disable HTML tags | ✅ YES |
| `drawCalendar` | Generate calendar HTML | ✅ YES |

---

## Subprocessors

The html processor also includes these subprocessor categories:
- **searchengine** - Full-text search indexing
- **mrcalendar** - Calendar utilities
- **webserver** - HTTP server utilities (separate audit)
- **inetd** - Network daemon support (separate audit)

---

## Implementation Analysis

### Complexity: **MEDIUM** (HTML generation, macro processing, image parsing)

### Dependencies
- **Other Processors:**
  - string (URL encoding, text manipulation)
  - file (file I/O for images)
  - table (data structures)
  - xml (if using XML-based directives)
- **External Libraries:** None (standard HTML/image parsing)
- **External Services:** None
- **GUI/Window Context:** None

### Key Implementation Notes

**Core HTML Operations:**

```c
// html.urlEncode - URL-encode strings
string htmlurlencode(string text) {
    // Encode special characters for URLs
    // Space -> %20, &, =, etc.
    return urlEncodeString(text);
}

// html.urlDecode - URL-decode strings
string htmlurldecode(string encoded) {
    // Decode %xx sequences
    // + -> space
    return urlDecodeString(encoded);
}

// html.parseHttpArgs - Parse query string
table htmlparsehttpargs(string queryString) {
    // Parse "name1=value1&name2=value2"
    // Return table with keys and values
    return parseQueryString(queryString);
}

// html.processMacros - Expand macros
string htmlprocessmacros(string html) {
    // Find macro references in HTML
    // Expand macros from glossary
    // Return expanded HTML
    return expandMacros(html);
}
```

**Image Header Parsing:**
- GIF: Read header for dimensions (straightforward)
- JPEG: More complex, scan for SOF marker

---

## Headless Compatibility Analysis

**Fully Compatible:** ✅ 23/23 verbs (100%)

**Essential for Headless:**
- HTTP response body generation
- Query parameter parsing
- Content encoding/decoding
- Directive processing
- Calendar and glossary utilities

**Use Cases in Headless:**
- Dynamic HTML page generation
- HTTP request/response handling
- Content processing and transformation
- Search engine indexing
- Calendar generation

---

## Implementation Effort

**Estimated Time:** 16-20 hours

**Breakdown:**
- URL encoding/decoding: 2 hours
- HTTP argument parsing: 1 hour
- Image header parsing (GIF, JPEG): 2-3 hours
- Macro processing: 2-3 hours
- Directive execution: 3-4 hours
- Glossary/preference management: 2 hours
- Testing and integration: 2-3 hours

**Confidence:** HIGH (straightforward HTML/HTTP operations)

**Blockers:**
- Must handle multi-byte encodings (UTF-8, ISO-8859-1)
- Image parsing requires correct binary structure understanding

---

## Priority & Sequencing

**Priority:** 🔴 **CRITICAL** (Tier 1 - Essential for web server)

**Recommended Sequence:** After string processor

**Prerequisites:**
- String processor (text manipulation)
- File processor (image I/O)
- Table processor (data structures)

---

## Testing Strategy

**URL Encoding/Decoding:**
```usertalk
// Encode
assert(html.urlEncode("hello world") == "hello%20world")
assert(html.urlEncode("a&b=c") == "a%26b%3Dc")

// Decode
assert(html.urlDecode("hello%20world") == "hello world")
assert(html.urlDecode("a%26b%3Dc") == "a&b=c")

// Round-trip
local (original = "foo bar & baz=qux")
local (encoded = html.urlEncode(original))
local (decoded = html.urlDecode(encoded))
assert(decoded == original)
```

**HTTP Argument Parsing:**
```usertalk
local (args = html.parseHttpArgs("name=John&age=30&city=NYC"))
assert(args.name == "John")
assert(args.age == "30")
assert(args.city == "NYC")
```

**Image Dimension Parsing:**
```usertalk
local (gifFile = "/path/to/image.gif")
local (dims = html.getGifHeightWidth(gifFile))
assert(dims.height > 0)
assert(dims.width > 0)
```

**Edge Cases:**
- Empty strings
- Special characters (%, &, =, +, space)
- Unicode/UTF-8 in URL encoding
- Missing parameters in query string
- Malformed image files

---

## Related Processors

- **string** - String manipulation and encoding
- **file** - Image file I/O
- **table** - Data structures
- **webserver** - HTTP server integration
- **searchengine** - Full-text search (subprocessor)

---

## Special Considerations

**Character Encoding:**
- URL encoding: UTF-8 or ASCII?
- ISO-8859-1 support for legacy compatibility
- Multi-byte character handling

**Performance:**
- Macro expansion can be slow for complex templates
- Image header parsing on large files
- Caching macro results

**Standards Compliance:**
- RFC 3986 (URI encoding)
- RFC 7231 (HTTP/1.1 semantics)
- GIF89a, JPEG formats

---

## Summary

**Status:** ✅ **Viable for Headless** (100% compatible)

**Key Findings:**
1. All 23 verbs are pure HTML/HTTP text processing operations
2. No GUI or external service dependencies
3. Essential for web server operation
4. Moderate implementation effort (16-20 hours)
5. Standard algorithms (RFC-defined)

**Recommendation:** CRITICAL priority (essential for web server, multiple foundational utilities)

---

**Audit Status:** ✅ Complete and Approved for Headless Implementation
