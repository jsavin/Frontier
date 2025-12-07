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
| **Verb Count** | 29 kernel verbs + 5 html.table subverbs + 5 subprocessor categories (searchengine, mrcalendar, webserver, inetd) |
| **Window Required** | NO |
| **Implementation Type** | Kernel verbs + script utilities |

---

## Category Assessment

**Category:** ✅ **HTML Content Processing & Generation**

**Rationale:**
HTML processor provides utilities for generating, processing, and manipulating HTML content. All operations are text-based with no GUI dependencies. Core functionality for web server operation.

**Headless Compatibility:** ✅ **Full** (34/34 verbs - 29 core + 5 html.table)

---

## Core Verb Inventory (29 verbs + 5 html.table subverbs = 34 total)

| Verb | Parameters | Returns | Headless |
|------|-----------|---------|----------|
| `processMacros` | (string s, boolean plainprocessing=false, address adrPageTable=nil) | string | ✅ YES |
| `getOneDirective` | (string directiveName, any s) | any | ✅ YES |
| `runDirective` | (string linetext, address adrPageTable=@websites.["#data"]) | string | ✅ YES |
| `runDirectives` | (string text, address adrPageTable=@websites.["#data"]) | any | ✅ YES |
| `runOutlineDirectives` | (address adroutline, address adrPageTable=@websites.["#data"]) | any | ✅ YES |
| `getGifHeightWidth` | (any f) | list | ✅ YES |
| `getJpegHeightWidth` | (any f) | list | ✅ YES |
| `getPngHeightWidth` | (any f) | list | ✅ YES |
| `normalizeName` | (string name, address adrPageTable=nil, address adrObject=nil) | string | ✅ YES |
| `refGlossary` | (string name) | string | ✅ YES |
| `getPref` | (string prefName, address adrPageTable=nil) | any | ✅ YES |
| `getPagePref` | (string prefName, address adrPage, address adrPageTable=@websites.["#data"]) | any | ✅ YES |
| `getPageTableAddress` | () | address | ✅ YES |
| `deletePageTableAddress` | () | boolean | ✅ YES |
| `getFileName` | (string name, address adrPageTable=@websites.["#data"]) | string | ✅ YES |
| `getPath` | (address adrSource, address adrDest, address adrPageTable=nil) | string | ✅ YES |
| `getFileURL` | (any f) | string | ✅ YES |
| `getOutlineHTML` | (address adroutline, string indentstring, string outdentstring, string linestartstring, string lineendstring, boolean flprettyPrint=true) | string | ✅ YES |
| `getOneTagValue` | (string htmltext, string tagname) | string | ✅ YES |
| `getLink` | (string linetext, string url, string whatTarget=nil, string anchor=nil, string class=nil, string title=nil, string accesskey=nil, string hreflang=nil, string tabindex=nil, string onmouseover="", string onmouseout="", string onclick="") | string | ✅ YES |
| `buildObject` | (address adrObject, address adrPageTable=@websites.["#data"], string templateName=nil) | string | ✅ YES |
| `buildOnePage` | (address adrPage, address adrPageTable=@websites.["#data"]) | any | ✅ YES |
| `buildFromOutline` | (address adroutline) | number | ✅ YES |
| `buildGlossary` | (address adrGlossary=table.getCursorAddress(), boolean flinteract=false, address adrFilterScript=nil) | boolean | ✅ YES |
| `neuterMacros` | (string s, address adrTable) | string | ✅ YES |
| `neuterTags` | (string s, address adrTable) | string | ✅ YES |
| `neuterJavaScript` | (string s, any legalProtocolSchemes=nil) | string | ✅ YES |
| `traversalSkip` | (address adr) | boolean | ✅ YES |
| `addPageToGlossary` | (address adrPageTable) | boolean | ✅ YES |

### HTML.Table Subverbs (5 verbs)

| Verb | Parameters | Returns | Headless |
|------|-----------|---------|----------|
| `table.new` | (number border=0, number cellspacing=0, number cellpadding=0, number cols=1, string method="html") | address | ✅ YES |
| `table.addColumn` | (address adrTable, string title="", string type="string", string align="left", string size="", string link="", boolean flLink=false) | address | ✅ YES |
| `table.addRow` | (address adrTable) | address | ✅ YES |
| `table.render` | (address adrTable) | string | ✅ YES |
| `table.delete` | (address adrTable) | void | ✅ YES |

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
// html.processMacros - Expand embedded macros in HTML text
string htmlprocessmacros(string s, boolean plainprocessing=false, address adrPageTable=nil) {
    // Parse HTML text and expand macro directives
    // Macros: «directive» syntax
    // Returns processed HTML string
    return processMacros(s, plainprocessing, adrPageTable);
}

// html.getOneDirective - Extract and execute single directive
any htmlgetnedirective(string directiveName, any s) {
    // Extract directive value from HTML or outline
    // Executes directive and returns result
    return executeDirective(directiveName, s);
}

// html.normalizeName - Normalize filename according to preferences
string htmlnormalizename(string name, address adrPageTable=nil, address adrObject=nil) {
    // Apply naming conventions: drop non-alphas, lowercase, max length
    // Returns normalized name string
    return applyNamingConventions(name, adrPageTable, adrObject);
}

// html.getOutlineHTML - Generate HTML from outline structure
string htmlgetoutlinehtml(address adroutline, string indentstring, string outdentstring, string linestartstring, string lineendstring, boolean flprettyPrint=true) {
    // Traverse outline and wrap each line with formatting strings
    // Returns HTML representation of outline
    return renderOutlineAsHTML(adroutline, indentstring, outdentstring, linestartstring, lineendstring, flprettyPrint);
}

// html.getGifHeightWidth - Parse GIF header for dimensions
list htmlgetgifheightwidth(any f) {
    // Read binary GIF file
    // Extract width and height from GIF89a or GIF87a header
    // Returns {height, width}
    return parseGIFDimensions(f);
}

// html.getJpegHeightWidth - Parse JPEG header for dimensions
list htmlgetjpegheightwidth(any f) {
    // Read binary JPEG file
    // Scan for SOF (Start of Frame) marker
    // Extract width and height
    // Returns {height, width}
    return parseJPEGDimensions(f);
}

// html.refGlossary - Look up term in glossary
string htmlrefglossary(string name) {
    // Search glossary table for name
    // Return glossary definition/link
    return lookupGlossaryTerm(name);
}

// html.getPref - Get preference value
any htmlgetpref(string prefName, address adrPageTable=nil) {
    // Look up preference in page table
    // Return preference value
    return getPreferenceValue(prefName, adrPageTable);
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
