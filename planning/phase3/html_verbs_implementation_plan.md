# HTML Verbs Implementation Plan for Headless Mode

**Author**: Claude (System Architect)
**Date**: 2026-01-13
**Status**: Planning - Ready for Implementation

---

## Executive Summary

This document provides a comprehensive implementation plan for the HTML verb subsystem in Frontier's headless mode. The HTML verbs are part of Frontier's web publishing framework and consist of 47 total verbs across 5 processor namespaces: `html.*` (23 verbs), `searchengine.*` (5 verbs), `webserver.*` (7 verbs), `mrcalendar.*` (11 verbs), and `inetd.*` (1 verb).

**Current Status**: All 47 verbs have stub implementations that return "not implemented". The C implementation exists in `Common/source/langhtml.c` (9,800+ lines) with full implementations for most verbs.

**Key Finding**: The majority of HTML verbs are **ready to forward** to existing C implementations. Only a small subset requires GUI dependencies to be stubbed or security hardening before use in headless mode.

**Implementation Strategy**: Use the **forwarding pattern** to connect headless stubs to existing C implementations in `langhtml.c`, with selective stubbing for GUI-dependent verbs and security review for user-input processing verbs.

---

## Verb Inventory

### Total Count by Processor

| Processor | Verb Count | C Implementation | Stub File |
|-----------|------------|------------------|-----------|
| `html` | 23 | `langhtml.c` | `tests/headless_html_verbs.c` |
| `searchengine` | 5 | `langhtml.c` | `tests/headless_searchengine_verbs.c` |
| `webserver` | 7 | `langhtml.c` | `tests/headless_webserver_verbs.c` |
| `mrcalendar` | 11 | `langhtml.c` | `tests/headless_mrcalendar_verbs.c` |
| `inetd` | 1 | `langhtml.c` | `tests/headless_inetd_verbs.c` |
| **TOTAL** | **47** | - | - |

### HTML Processor Verbs (23 verbs)

| # | Verb Name | C Function | Category | Status | Priority |
|---|-----------|------------|----------|--------|----------|
| 1 | `html.processmacros` | `processhtmlmacrosverb()` | Text Processing | Forward | P1 |
| 2 | `html.urldecode` | `urldecodeverb()` | Text Processing | Forward | P1 |
| 3 | `html.urlencode` | `urlencodeverb()` | Text Processing | Forward | P1 |
| 4 | `html.parsehttpargs` | `parseargsverb()` | Text Processing | Forward | P1 |
| 5 | `html.iso8859encode` | `iso8859encodeverb()` | Text Processing | Forward | P2 |
| 6 | `html.getgifheightwidth` | `getgifheightwidthverb()` | Image Analysis | Forward | P2 |
| 7 | `html.getjpegheightwidth` | `getjpegheightwidthverb()` | Image Analysis | Forward | P2 |
| 8 | `html.buildpagetable` | `buildpagetableverb()` | Page Framework | Forward | P2 |
| 9 | `html.refglossary` | - | Page Framework | Commented Out | P3 |
| 10 | `html.getpref` | `getprefverb()` | Page Framework | Forward | P2 |
| 11 | `html.getonedirective` | - | Page Framework | Commented Out | P3 |
| 12 | `html.rundirective` | `rundirectiveverb()` | Page Framework | Forward | P2 |
| 13 | `html.rundirectives` | `rundirectivesverb()` | Page Framework | Forward | P2 |
| 14 | `html.runoutlinedirectives` | `runoutlinedirectivesverb()` | Page Framework | Forward | P2 |
| 15 | `html.cleanforexport` | `cleanforexportverb()` | Text Processing | Forward | P2 |
| 16 | `html.normalizename` | - | Text Processing | UserTalk Script | P3 |
| 17 | `html.glossarypatcher` | `glossarypatcherverb()` | Text Processing | Forward | P2 |
| 18 | `html.expandurls` | `expandurlsverb()` | Text Processing | Forward | P1 |
| 19 | `html.traversalskip` | `traversalskipverb()` | Page Framework | Forward | P2 |
| 20 | `html.getpagetableaddress` | `getpagetableaddressverb()` | Page Framework | Forward | P2 |
| 21 | `html.neutermacros` | `htmlneutermacrosverb()` | Security | Forward (Review) | P1 |
| 22 | `html.neutertags` | `htmlneutertagsverb()` | Security | Forward (Review) | P1 |
| 23 | `html.drawcalendar` | `htmlcalendardrawverb()` | GUI | Stub (GUI) | P4 |

### SearchEngine Processor Verbs (5 verbs)

| # | Verb Name | C Function | Category | Status | Priority |
|---|-----------|------------|----------|--------|----------|
| 1 | `searchengine.stripmarkup` | `stripmarkupverb()` | Text Processing | Forward | P2 |
| 2 | `searchengine.deindexpage` | `deindexpageverb()` | Indexing | Forward | P3 |
| 3 | `searchengine.indexpage` | `indexpageverb()` | Indexing | Forward | P3 |
| 4 | `searchengine.cleanindex` | `cleanindexverb()` | Indexing | Forward | P3 |
| 5 | `searchengine.mergeresults` | `unionmatchesverb()` | Indexing | Forward | P3 |

### WebServer Processor Verbs (7 verbs)

| # | Verb Name | C Function | Category | Status | Priority |
|---|-----------|------------|----------|--------|----------|
| 1 | `webserver.server` | `webserverserver()` | Network | Forward | P3 |
| 2 | `webserver.dispatch` | `webserverdispatch()` | Network | Forward | P3 |
| 3 | `webserver.parseheaders` | `webserverparseheaders()` | Network | Forward | P3 |
| 4 | `webserver.parsecookies` | `webserverparsecookies()` | Network | Forward | P3 |
| 5 | `webserver.buildresponse` | `webserverbuildresponse()` | Network | Forward | P3 |
| 6 | `webserver.builderrorpage` | `webserverbuilderrorpage()` | Network | Forward | P3 |
| 7 | `webserver.getserverstring` | `webservergetserverstring()` | Network | Forward | P3 |

### MRCalendar Processor Verbs (11 verbs)

| # | Verb Name | C Function | Category | Status | Priority |
|---|-----------|------------|----------|--------|----------|
| 1 | `mrcalendar.getaddressday` | `mrcalendargetaddressdayverb()` | Calendar | Forward | P4 |
| 2 | `mrcalendar.getdayaddress` | `mrcalendargetdayaddressverb()` | Calendar | Forward | P4 |
| 3 | `mrcalendar.getfirstaddress` | `mrcalendargetfirstaddressverb()` | Calendar | Forward | P4 |
| 4 | `mrcalendar.getfirstday` | `mrcalendargetfirstdayverb()` | Calendar | Forward | P4 |
| 5 | `mrcalendar.getlastaddress` | `mrcalendargetlastaddressverb()` | Calendar | Forward | P4 |
| 6 | `mrcalendar.getlastday` | `mrcalendargetlastdayverb()` | Calendar | Forward | P4 |
| 7 | `mrcalendar.getmostrecentaddress` | `mrcalendargetmostrecentaddressverb()` | Calendar | Forward | P4 |
| 8 | `mrcalendar.getmostrecentday` | `mrcalendargetmostrecentdayverb()` | Calendar | Forward | P4 |
| 9 | `mrcalendar.getnextaddress` | `mrcalendargetnextaddressverb()` | Calendar | Forward | P4 |
| 10 | `mrcalendar.getnextday` | `mrcalendargetnextdayverb()` | Calendar | Forward | P4 |
| 11 | `mrcalendar.navigate` | `mrcalendarnavigateverb()` | Calendar | Forward | P4 |

### Inetd Processor Verbs (1 verb)

| # | Verb Name | C Function | Category | Status | Priority |
|---|-----------|------------|----------|--------|----------|
| 1 | `inetd.supervisor` | `inetdsupervisor()` | Network | Forward | P4 |

---

## Category Breakdown

### 1. Text Processing (11 verbs) - **Core Headless Functionality**

These verbs perform string manipulation and HTML processing. No GUI dependencies, ready to forward.

**Verbs**:
- `html.processmacros` - Macro expansion in HTML text
- `html.urldecode` - URL percent-decoding
- `html.urlencode` - URL percent-encoding
- `html.parsehttpargs` - Parse query string parameters
- `html.iso8859encode` - Character encoding conversion
- `html.cleanforexport` - Remove Mac-specific characters
- `html.glossarypatcher` - Glossary reference expansion
- `html.expandurls` - Auto-link URLs in text
- `html.normalizename` - Normalize filenames (UserTalk script)
- `searchengine.stripmarkup` - Remove HTML tags from text
- `html.neutertags` - Security: escape dangerous HTML tags
- `html.neutermacros` - Security: escape dangerous macros

**Implementation**: Direct forwarding to C functions. Security review for `neutermacros` and `neutertags`.

### 2. Image Analysis (2 verbs) - **File I/O**

Read image files and extract dimensions. No GUI dependencies, file I/O only.

**Verbs**:
- `html.getgifheightwidth` - Read GIF dimensions
- `html.getjpegheightwidth` - Read JPEG dimensions

**Implementation**: Direct forwarding. Works with file paths and ODB addresses.

### 3. Page Framework (8 verbs) - **Website Management**

Verbs for managing page tables, directives, and website traversal. No GUI, but depend on ODB structures.

**Verbs**:
- `html.buildpagetable` - Build page metadata table
- `html.getpref` - Get page/site preference value
- `html.rundirective` - Execute a single directive
- `html.rundirectives` - Execute all directives
- `html.runoutlinedirectives` - Execute outline-based directives
- `html.traversalskip` - Check if object should be skipped in site traversal
- `html.getpagetableaddress` - Get current page table address
- `html.refglossary` - Glossary reference (commented out in C)
- `html.getonedirective` - Get directive value (commented out in C)

**Implementation**: Direct forwarding. Commented-out verbs need investigation.

### 4. Search Engine Indexing (4 verbs) - **Full-Text Search**

Build and manage full-text search indexes. Pure data processing, no GUI.

**Verbs**:
- `searchengine.deindexpage` - Remove page from index
- `searchengine.indexpage` - Add page to index
- `searchengine.cleanindex` - Optimize search index
- `searchengine.mergeresults` - Merge search result sets

**Implementation**: Direct forwarding. Low priority (not commonly used).

### 5. Web Server (7 verbs) - **HTTP Server**

HTTP request/response handling. Network I/O, no GUI dependencies.

**Verbs**:
- `webserver.server` - Main HTTP server loop
- `webserver.dispatch` - Route request to handler
- `webserver.parseheaders` - Parse HTTP headers
- `webserver.parsecookies` - Parse cookie header
- `webserver.buildresponse` - Build HTTP response
- `webserver.builderrorpage` - Generate error page HTML
- `webserver.getserverstring` - Get server identification string

**Implementation**: Direct forwarding. Low priority (specialized use case).

### 6. Calendar (11 verbs) - **Date-based Navigation**

Navigate date-based archives (mainResponder.calendar feature). Data processing, no GUI.

**Verbs**: All `mrcalendar.*` verbs

**Implementation**: Direct forwarding. Very low priority (legacy feature).

### 7. Internet Daemon (1 verb) - **TCP Server**

Generic TCP server supervisor. Network I/O, no GUI.

**Verbs**: `inetd.supervisor`

**Implementation**: Direct forwarding. Very low priority (specialized use case).

### 8. GUI-Dependent (1 verb) - **Stub Required**

**Verbs**:
- `html.drawcalendar` - Renders calendar to GUI (Mac QuickDraw)

**Implementation**: Return stub error in headless mode.

---

## Implementation Strategy

### Forwarding Pattern

The HTML verbs use a **single dispatcher function** pattern in `langhtml.c`:

```c
static boolean htmlfunctionvalue (short token, hdltreenode hparam1,
                                  tyvaluerecord *vreturned, bigstring bserror) {
    switch (token) {
        case processmacrosfunc:
            return (processhtmlmacrosverb (hp1, v));
        case urldecodefunc:
            return (urldecodeverb (hp1, v));
        // ... etc
    }
}
```

**Headless Strategy**: Call the existing C functions directly from headless stubs. Example:

```c
// In tests/headless_html_verbs.c
case htmv_urldecode:
    // Forward to existing C implementation
    extern boolean urldecodeverb(hdltreenode, tyvaluerecord *);
    return urldecodeverb(hparam1, vreturned);
```

### Token Mapping Challenge

**Problem**: The headless stubs use local enum tokens (`htmv_urldecode = 1`), but the C functions in `langhtml.c` expect global tokens defined elsewhere (e.g., `urldecodefunc`).

**Solution Options**:

1. **Include Original Headers** (Preferred)
   - Include `kernelverbs.h` or equivalent header with original token definitions
   - Map headless tokens to original tokens in forwarding layer
   - Example: `case htmv_urldecode: return htmlfunctionvalue(urldecodefunc, hparam1, vreturned, bserror);`

2. **Direct Function Calls**
   - Bypass token system entirely
   - Call verb functions directly: `urldecodeverb()`, `urlencodeverb()`, etc.
   - Simpler, but loses consistency with original architecture

**Recommendation**: Use Option 2 (direct calls) for simplicity and clarity. The token system is an internal implementation detail.

### Security Considerations

#### XSS Prevention Verbs

Two verbs are specifically designed for security:

1. **`html.neutermacros`** - Escapes macro syntax (`{...}` → `&#123;...&#125;`)
   - Prevents user-supplied text from executing as macros
   - Critical for displaying untrusted content

2. **`html.neutertags`** - Escapes HTML tags based on whitelist
   - Prevents XSS attacks by escaping dangerous tags
   - Allows safe tags (configured via table parameter)
   - Optionally balances tags (adds closing tags)

**Security Review Required**: Before exposing these verbs, verify:
- Input validation is robust (no buffer overflows)
- Escape sequences are complete (no bypass techniques)
- Default whitelists are secure
- No execution of escaped content is possible

#### Input Validation Verbs

These verbs process user-supplied data and must be reviewed:

- `html.parsehttpargs` - Parse query strings (injection risk)
- `html.urldecode` - Decode percent-encoded strings (overflow risk)
- `html.processmacros` - Execute macros in text (RCE risk if misused)

**Mitigation**: Existing C code has been in production for 20+ years. Security issues are unlikely but should be documented.

### GUI Dependencies

**Single GUI Verb**: `html.drawcalendar`

This verb calls Mac QuickDraw to render a calendar to a window. In headless mode:

```c
case htmv_drawcalendar:
    // GUI not available in headless mode
    if (bserror) copystring(BIGSTRING("\pGUI operation not supported in headless mode"), bserror);
    return false;
```

### Commented-Out Verbs

Two verbs are commented out in the C switch statement:

1. **`html.refglossary`** (line 9835-9836)
   ```c
   //case refglossaryfunc:
   //    return (refglossaryverb (hp1, v));
   ```

2. **`html.getonedirective`** (line 9841-9842)
   ```c
   //case getonedirectivefunc:
   //    return (getonedirectiveverb (hp1, v));
   ```

**Investigation Needed**: Determine why these were disabled. Likely superseded by other verbs or deprecated. Keep stubbed for now.

### UserTalk Script Verbs

**`html.normalizename`** is documented as "implemented as a script" (see `usertalk_scripts/Frontier.root/system/verbs/builtins/html/normalizeName.ut`).

**Implementation**: This verb is callable from UserTalk but has no C implementation. The stub is correct. Users load the UserTalk script from the database.

---

## Priority Phases

### Phase 1: Core Text Processing (P1) - **Week 1**

**Goal**: Enable basic HTML text processing for headless web publishing.

**Verbs** (6 verbs):
- `html.urldecode`
- `html.urlencode`
- `html.parsehttpargs`
- `html.expandurls`
- `html.processmacros`
- `html.neutermacros`
- `html.neutertags`

**Deliverables**:
1. Forward 6 verbs to C implementations
2. Add security documentation for neuter* verbs
3. Write integration tests for each verb
4. Document known limitations

**Testing**:
```yaml
# tests/integration/html_core_tests.yaml
- name: "html.urlencode - basic ASCII"
  script: 'html.urlencode("hello world")'
  expected_output: "hello%20world"

- name: "html.urldecode - basic ASCII"
  script: 'html.urldecode("hello%20world")'
  expected_output: "hello world"

- name: "html.parsehttpargs - query string"
  script: |
    html.parsehttpargs("name=John&age=30")
    // Returns table with name="John", age="30"
  expected_success: true

- name: "html.expandurls - auto-link"
  script: 'html.expandurls("Visit http://www.example.com/")'
  expected_output: '<a href="http://www.example.com/">http://www.example.com/</a>'

- name: "html.neutermacros - escape macros"
  script: |
    local(t);
    lang.new(tableType, @t);
    html.neutermacros("{dangerous.macro()}", @t)
  expected_output: "&#123;dangerous.macro()&#125;"
```

**Estimated Effort**: 3-4 days (includes testing and documentation)

### Phase 2: Image Analysis & Page Framework (P2) - **Week 2**

**Goal**: Enable image dimension detection and page table management.

**Verbs** (12 verbs):
- `html.getgifheightwidth`
- `html.getjpegheightwidth`
- `html.iso8859encode`
- `html.buildpagetable`
- `html.getpref`
- `html.rundirective`
- `html.rundirectives`
- `html.runoutlinedirectives`
- `html.cleanforexport`
- `html.glossarypatcher`
- `html.traversalskip`
- `html.getpagetableaddress`
- `searchengine.stripmarkup`

**Deliverables**:
1. Forward 13 verbs to C implementations
2. Test image dimension reading with sample GIF/JPEG files
3. Test page table operations with sample website structure
4. Document page framework architecture

**Testing**:
- Create test images in `tests/fixtures/images/`
- Test with various image formats and sizes
- Test page table operations with mock website structure

**Estimated Effort**: 4-5 days

### Phase 3: Search Engine & Web Server (P3) - **Week 3**

**Goal**: Enable full-text search and HTTP server functionality (advanced features).

**Verbs** (11 verbs):
- `searchengine.deindexpage`
- `searchengine.indexpage`
- `searchengine.cleanindex`
- `searchengine.mergeresults`
- `webserver.server`
- `webserver.dispatch`
- `webserver.parseheaders`
- `webserver.parsecookies`
- `webserver.buildresponse`
- `webserver.builderrorpage`
- `webserver.getserverstring`

**Deliverables**:
1. Forward 11 verbs to C implementations
2. Test search indexing with sample HTML content
3. Test HTTP server verbs (may require mock network setup)
4. Document web server architecture

**Estimated Effort**: 5-6 days (network testing is complex)

### Phase 4: Calendar & Specialized (P4) - **Week 4**

**Goal**: Complete remaining verbs (low-priority legacy features).

**Verbs** (13 verbs):
- All `mrcalendar.*` verbs (11 verbs)
- `inetd.supervisor`
- `html.drawcalendar` (stub with error)

**Deliverables**:
1. Forward 12 verbs to C implementations
2. Stub `html.drawcalendar` with appropriate error
3. Test calendar navigation with sample date-based archives
4. Document legacy features and recommend modern alternatives

**Estimated Effort**: 3-4 days

### Phase 5: Commented-Out Verbs Investigation (P3)

**Goal**: Determine fate of commented-out verbs.

**Verbs**:
- `html.refglossary`
- `html.getonedirective`

**Deliverables**:
1. Research why these were commented out (check git history, legacy docs)
2. Either re-enable or document deprecation
3. Update documentation

**Estimated Effort**: 1-2 days

---

## Testing Strategy

### Unit Tests (C Level)

Add unit tests in `tests/test_html_verbs.c` for each forwarded verb:

```c
// Test URL encoding
TEST(html_urlencode_basic) {
    tyvaluerecord result;
    // Call verb, verify result
    ASSERT_STRING_EQUAL(result.data.stringvalue, "hello%20world");
}

// Test URL decoding
TEST(html_urldecode_basic) {
    tyvaluerecord result;
    // Call verb, verify result
    ASSERT_STRING_EQUAL(result.data.stringvalue, "hello world");
}
```

### Integration Tests (UserTalk Level)

Add integration tests in `tests/integration/html_tests.yaml`:

```yaml
category: "html"
description: "HTML verb integration tests"
tests:
  - name: "html.urlencode - special characters"
    script: 'html.urlencode("hello & goodbye")'
    expected_output: "hello%20%26%20goodbye"

  - name: "html.getgifheightwidth - test image"
    script: |
      local(path = "{FRONTIER_TEST_TMP_DIR}/test.gif");
      file.copy("tests/fixtures/images/sample.gif", path);
      html.getgifheightwidth(path)
    expected_output: [100, 200]  # height, width

  - name: "html.processmacros - simple macro"
    script: |
      local(pagetable);
      lang.new(tableType, @pagetable);
      html.processmacros("{1+1}", false, @pagetable)
    expected_output: "2"
```

### Security Tests

Add specific tests for security verbs:

```yaml
  - name: "html.neutermacros - prevents macro execution"
    script: |
      local(legaltable);
      lang.new(tableType, @legaltable);
      local(result = html.neutermacros("{file.delete()}", @legaltable));
      // Verify macros are neutered and won't execute
      return string.contains(result, "&#123;")
    expected_output: true

  - name: "html.neutertags - escapes script tags"
    script: |
      local(legaltable);
      lang.new(tableType, @legaltable);
      local(result = html.neutertags("<script>alert('xss')</script>", @legaltable));
      // Verify script tags are escaped
      return string.contains(result, "&lt;script")
    expected_output: true
```

### Performance Tests

Test large input handling:

```yaml
  - name: "html.urlencode - large string"
    script: |
      local(bigstring = string.filledString("a", 10000));
      html.urlencode(bigstring);
      return true
    expected_success: true

  - name: "html.processmacros - deeply nested"
    script: |
      local(nested = "{1+{2+{3+{4+5}}}}");
      html.processmacros(nested, false, @pagetable)
    expected_output: "15"
```

### Test Fixtures

Create test fixtures directory:

```
tests/fixtures/html/
├── images/
│   ├── sample.gif          # 100x200 test GIF
│   ├── sample.jpg          # 640x480 test JPEG
│   └── large.jpg           # Large image for performance test
├── pages/
│   ├── simple.html         # Basic HTML page
│   ├── with_macros.html    # Page with macro placeholders
│   └── complex.html        # Complex page structure
└── data/
    ├── sample_glossary.txt # Glossary entries
    └── sample_directives.txt # Page directives
```

---

## Security Notes

### XSS Prevention

The `html.neutertags` verb is the primary XSS defense mechanism:

**How it works**:
1. Takes HTML text and a "legal tags" table as input
2. For each HTML tag in text:
   - If tag is in legal table → leave it alone (optionally balance with closing tag)
   - If tag is NOT in legal table → escape opening `<` to `&lt;`
3. Returns sanitized HTML

**Security properties**:
- Whitelist-based (safe by default)
- Configurable per use case
- Handles nested tags correctly
- Prevents script injection, iframe injection, etc.

**Usage example**:
```usertalk
local(legalTags);
lang.new(tableType, @legalTags);
legalTags.b = true;        // Allow <b> tags
legalTags.i = true;        // Allow <i> tags
legalTags.a = {flLegal: true, flClose: false};  // Allow <a> tags, no auto-close

local(untrustedHTML = "<b>Hello</b> <script>alert('xss')</script>");
local(safe = html.neutertags(untrustedHTML, @legalTags));
// Result: "<b>Hello</b> &lt;script>alert('xss')&lt;/script>"
```

### Macro Execution Prevention

The `html.neutermacros` verb prevents macro injection:

**How it works**:
1. Takes text with potential macros and a "legal macros" table
2. For each macro pattern `{identifier(...)}`:
   - If identifier is in legal table → leave it alone
   - If identifier is NOT in legal table → escape braces: `&#123;` and `&#125;`
3. Returns sanitized text

**Security properties**:
- Whitelist-based (safe by default)
- Prevents arbitrary code execution via macros
- Handles parameter checking (some macros legal with params, some without)

**Usage example**:
```usertalk
local(legalMacros);
lang.new(tableType, @legalMacros);
legalMacros.clock.now = true;  // Allow {clock.now()} macro

local(untrustedText = "{clock.now()} {file.delete()}");
local(safe = html.neutermacros(untrustedText, @legalMacros));
// Result: "{clock.now()} &#123;file.delete()&#125;"
```

### Input Validation Concerns

**`html.parsehttpargs`**: Parses query strings like `name=John&age=30`
- **Risk**: Malformed input could cause buffer overflow or injection
- **Mitigation**: C code uses length-checked string functions (bigstring, 255 char limit)
- **Recommendation**: Test with malformed input, verify no crashes

**`html.urldecode`**: Decodes percent-encoded strings
- **Risk**: Malformed percent sequences (e.g., `%ZZ`) could cause issues
- **Mitigation**: C code validates hex digits before decoding
- **Recommendation**: Test with invalid percent sequences

**`html.processmacros`**: Executes macros in text
- **Risk**: Arbitrary code execution if untrusted text is processed
- **Mitigation**: This is intentional behavior (macro execution is the feature)
- **Recommendation**: Document that input must be trusted, or use `html.neutermacros` first

### Recommendations

1. **Always neuter untrusted input**:
   ```usertalk
   local(userInput = ...) // From web form, etc.
   userInput = html.neutertags(userInput, @config.safeTags);
   userInput = html.neutermacros(userInput, @config.safeMacros);
   // Now safe to store/display
   ```

2. **Audit existing code**: Search for `html.processmacros` calls on untrusted data
3. **Document security model**: Create `docs/HTML_SECURITY.md` explaining safe usage patterns
4. **Add security tests**: Test XSS/injection attempts to verify defenses work

---

## Dependencies

### Build System

Add to `frontier-cli/Makefile`:

```makefile
# HTML verbs require langhtml.c (already included)
# No additional source files needed - all in langhtml.c
```

### Header Files

Required includes for forwarding:

```c
// In tests/headless_html_verbs.c
#include "langhtml.h"  // If available, or declare functions as extern
```

If `langhtml.h` doesn't exist (likely), declare functions as extern:

```c
// Forward declarations for C functions in langhtml.c
extern boolean urldecodeverb(hdltreenode, tyvaluerecord *);
extern boolean urlencodeverb(hdltreenode, tyvaluerecord *);
extern boolean parseargsverb(hdltreenode, tyvaluerecord *);
// ... etc
```

### Runtime Dependencies

Some verbs depend on ODB structures being initialized:

- **Page table verbs** require `system.paths` and website structure
- **Preference verbs** require `user.html.prefs` table
- **Glossary verbs** require glossary tables

**Testing note**: Integration tests should load Frontier.root to ensure proper ODB initialization.

---

## File Changes Required

### 1. Update Headless Verb Files

**Files to modify**:
- `tests/headless_html_verbs.c` (23 verbs)
- `tests/headless_searchengine_verbs.c` (5 verbs)
- `tests/headless_webserver_verbs.c` (7 verbs)
- `tests/headless_mrcalendar_verbs.c` (11 verbs)
- `tests/headless_inetd_verbs.c` (1 verb)

**Pattern** (example for `html.urldecode`):

```c
// BEFORE:
case htmv_urldecode:
    /* Verb #1: html.urldecode - not yet implemented */
    if (bserror) copystring(BIGSTRING("\pnot implemented"), bserror);
    return false;

// AFTER:
case htmv_urldecode:
    /* Forward to C implementation in langhtml.c */
    extern boolean urldecodeverb(hdltreenode, tyvaluerecord *);
    return urldecodeverb(hparam1, vreturned);
```

### 2. Update File Headers

Per anti-pattern guidance (Issue #256), update headers after implementation:

```c
/*
 * headless_html_verbs.c - Html processor verb implementations
 *
 * Originally generated by tools/kernelverbs_parser/generate_processor_stubs.py,
 * now contains production forwarding implementations.
 *
 * DO NOT regenerate - this file contains production implementations.
 *
 * Implementation status:
 * - Phase 1 (6 verbs):  COMPLETE - Core text processing
 * - Phase 2 (12 verbs): PENDING  - Image analysis & page framework
 * - Phase 3 (4 verbs):  PENDING  - Advanced features
 * - Phase 4 (1 verb):   STUBBED  - GUI-dependent (drawcalendar)
 *
 * See planning/phase3/html_verbs_implementation_plan.md for complete roadmap.
 */
```

### 3. Add Test Files

**New files**:
- `tests/integration/html_core_tests.yaml` (Phase 1 tests)
- `tests/integration/html_image_tests.yaml` (Phase 2 tests)
- `tests/integration/html_framework_tests.yaml` (Phase 2 tests)
- `tests/integration/html_webserver_tests.yaml` (Phase 3 tests)
- `tests/integration/html_searchengine_tests.yaml` (Phase 3 tests)
- `tests/integration/html_calendar_tests.yaml` (Phase 4 tests)

**Test fixtures**:
- `tests/fixtures/html/images/` (sample GIF/JPEG files)
- `tests/fixtures/html/pages/` (sample HTML/text files)
- `tests/fixtures/html/data/` (sample data files)

### 4. Documentation

**New files**:
- `docs/HTML_SECURITY.md` - Security model and safe usage patterns
- `docs/HTML_VERBS_GUIDE.md` - Comprehensive guide to HTML verb usage

**Updates**:
- `docs/VERB_IMPLEMENTATION_GUIDE.md` - Add HTML verb forwarding pattern
- `CLAUDE.md` - Add HTML verb implementation notes

---

## Known Limitations

### 1. Commented-Out Verbs

Two verbs are commented out in the C source:
- `html.refglossary` - Reason unknown, needs investigation
- `html.getonedirective` - Reason unknown, needs investigation

**Current status**: Return "not implemented" error. Investigation needed.

### 2. UserTalk Script Verbs

**`html.normalizename`** is implemented as a UserTalk script, not in C.

**Impact**: Verb works only if Frontier.root is loaded (contains the script).

**Workaround**: Document that users must load system root for this verb.

### 3. GUI-Dependent Verb

**`html.drawcalendar`** requires Mac QuickDraw (GUI rendering).

**Impact**: Returns error in headless mode.

**Alternative**: Users can generate calendar HTML using `mrcalendar.*` navigation verbs + string building.

### 4. Network Dependencies

Web server verbs (`webserver.*`) and `inetd.supervisor` require network I/O.

**Impact**: May not work in sandboxed environments without network permissions.

**Testing note**: Integration tests may need network access or mock network layer.

### 5. File System Access

Image dimension verbs require file system read access.

**Impact**: Subject to macOS sandbox restrictions (no `/tmp` access).

**Workaround**: Use project-relative paths as documented in CLAUDE.md.

---

## Migration Path

### For Existing Frontier Code

Existing Frontier websites and scripts using HTML verbs should work without modification:

1. **Text processing verbs** - Work identically in headless mode
2. **Image dimension verbs** - Work identically (file paths must be absolute)
3. **Page framework verbs** - Work identically (require proper ODB initialization)
4. **Web server verbs** - Work identically (require network permissions)
5. **Calendar verbs** - Work identically (legacy feature, rarely used)
6. **`html.drawcalendar`** - Returns error (GUI not available)

### For New Headless Applications

Recommended usage patterns:

1. **Static site generation**:
   ```usertalk
   // Process page with macros
   local(html = string(page^));
   html = html.processmacros(html, false, @pagetable);

   // Auto-link URLs
   html = html.expandurls(html);

   // Write to file
   file.write(outputPath, html);
   ```

2. **User-generated content**:
   ```usertalk
   // Sanitize user input
   local(userHTML = ...);
   userHTML = html.neutertags(userHTML, @config.safeTags);
   userHTML = html.neutermacros(userHTML, @config.safeMacros);

   // Store safely
   db.setItem(path, userHTML);
   ```

3. **URL handling**:
   ```usertalk
   // Encode URL parameters
   local(url = "http://example.com/?name=" + html.urlencode(userName));

   // Decode query string
   local(params);
   html.parsehttpargs(queryString, @params);
   ```

---

## Risk Assessment

### Low Risk (Ready to Implement)

**Text processing verbs** (Phase 1):
- Well-tested C code (20+ years in production)
- No external dependencies
- No security concerns (with documentation)

**Image analysis verbs** (Phase 2):
- Read-only file operations
- No buffer overflow risk (fixed-size image headers)
- No external dependencies

### Medium Risk (Needs Review)

**Security verbs** (Phase 1):
- `html.neutermacros` and `html.neutertags` are security-critical
- Must verify escape sequences are complete
- Recommend security audit before production use

**Page framework verbs** (Phase 2):
- Depend on complex ODB structures
- May fail if ODB not properly initialized
- Recommend thorough integration testing

### High Risk (Needs Investigation)

**Web server verbs** (Phase 3):
- Network I/O complexity
- Potential for DoS, resource exhaustion
- Recommend load testing before production use

**Commented-out verbs** (Phase 5):
- Unknown reason for disabling
- May have hidden bugs or security issues
- Require git history investigation

---

## Success Criteria

### Phase 1 Complete

- [ ] 6 core text processing verbs forward to C implementations
- [ ] Integration tests pass for all Phase 1 verbs
- [ ] Security documentation written and reviewed
- [ ] No regression in existing tests

### Phase 2 Complete

- [ ] 12 image/framework verbs forward to C implementations
- [ ] Image dimension tests pass with sample GIF/JPEG files
- [ ] Page table tests pass with sample website structure
- [ ] No regression in existing tests

### Phase 3 Complete

- [ ] 11 advanced verbs forward to C implementations
- [ ] Search engine tests pass with sample content
- [ ] Web server tests pass (may use mock network)
- [ ] No regression in existing tests

### Phase 4 Complete

- [ ] 12 remaining verbs forwarded or stubbed
- [ ] `html.drawcalendar` returns appropriate error
- [ ] Calendar navigation tests pass
- [ ] Documentation updated with limitations

### Final Validation

- [ ] All 47 HTML verbs documented and tested
- [ ] Security review completed and documented
- [ ] User guide written with examples
- [ ] Migration guide written for existing code
- [ ] No open security concerns

---

## Estimated Effort

| Phase | Duration | Verbs | Effort (days) |
|-------|----------|-------|---------------|
| Phase 1: Core Text Processing | Week 1 | 6 | 3-4 |
| Phase 2: Image & Framework | Week 2 | 12 | 4-5 |
| Phase 3: Search & Web Server | Week 3 | 11 | 5-6 |
| Phase 4: Calendar & Specialized | Week 4 | 13 | 3-4 |
| Phase 5: Investigation | Parallel | 2 | 1-2 |
| **Total** | **4 weeks** | **47** | **16-21 days** |

**Note**: Effort assumes one developer working full-time. Can be parallelized by splitting phases across multiple developers.

---

## Next Steps

1. **Review this plan** with project maintainer (user)
2. **Begin Phase 1 implementation**:
   - Update `tests/headless_html_verbs.c` with forwarding code
   - Add extern declarations for C functions
   - Write integration tests
   - Run test suite and verify no regressions
3. **Security review** of `html.neutermacros` and `html.neutertags`
4. **Document security model** in `docs/HTML_SECURITY.md`
5. **Proceed to Phase 2** after Phase 1 validation

---

## References

- **C Implementation**: `/Users/jake/dev/jsavin/Frontier/Common/source/langhtml.c` (9,800+ lines)
- **Documentation**: `/Users/jake/dev/jsavin/Frontier/docs/usertalk/docserver/html/` (38 verb docs)
- **UserTalk Scripts**: `/Users/jake/dev/jsavin/Frontier/usertalk_scripts/Frontier.root/system/verbs/builtins/html/`
- **Stub Files**: `/Users/jake/dev/jsavin/Frontier/tests/headless_*_verbs.c` (5 files)
- **Resource Definition**: `/Users/jake/dev/jsavin/Frontier/Common/resources/Mac/kernelverbs.r` (lines 1017-1087)
- **Verb Implementation Guide**: `/Users/jake/dev/jsavin/Frontier/docs/VERB_IMPLEMENTATION_GUIDE.md`

---

**End of Implementation Plan**
