# Frontier Website Framework Architecture

## Overview

Frontier's website framework is a comprehensive web publishing system that combines:
- **Object Database (ODB)** structures for content and configuration
- **UserTalk scripts** for page generation and templating
- **C runtime** for performance-critical operations
- **HTML verbs** for text processing and page rendering

This document provides the first comprehensive guide to the website framework structure, filling a gap that has existed since UserLand's era.

**Historical Context**: This framework powered Frontier's web publishing capabilities in the late 1990s and early 2000s, including Manila (an early blogging platform) and the Radio UserLand desktop website system. The architecture demonstrated remarkable foresight in separating content, presentation, and logic.

---

## Architecture Layers

### Layer 1: Database Structure (ODB)
- `root.websites` - Site configuration and templates
- `user.html` - User-specific settings and content
- Page tables - Runtime context for web requests

### Layer 2: UserTalk Scripts
- Callbacks - Custom processing hooks
- Renderers - Page generation logic
- Filters - Content transformation pipeline
- Tools - Utility scripts

### Layer 3: C Runtime
- HTML verbs - Text processing (47 verbs across 5 categories)
- Macro expansion - Template substitution
- Glossary lookup - Content reuse and linking
- Directive processing - Dynamic field evaluation

### Layer 4: Output Generation
- FTP publishing - File transfer to web servers
- File writing - Local file generation
- URL management - Link generation and relative paths

---

## Database Structure

### root.websites Structure

The website framework is organized under `root.websites` with 8 core items:

| Item | Type | Size | Purpose |
|------|------|------|---------|
| **#data** | Table | 0 | Default page table template (empty but used as reference) |
| **#filters** | Table | 3 | Filter scripts (firstFilter, pagefilter, finalfilter) |
| **#ftpSite** | Table | 3 | FTP/file server configuration |
| **#glossary** | Table | 13 | Site-wide glossary entries |
| **#prefs** | Table | 5 | Color preferences (HTML color codes) |
| **#template** | Outline | N/A | Page template structure (outline format) |
| **#tools** | Table | 0 | Tool scripts (extensibility point) |
| **samples** | Table | 10 | Sample websites for testing/documentation |

#### FTP Site Configuration (#ftpSite)

```usertalk
// Example structure
websites.#ftpSite.folder = "path/to/website/root"
websites.#ftpSite.isLocal = true
websites.#ftpSite.url = "file:///path/to/website/root"
```

Used for:
- Determining base path for file operations
- Calculating relative URLs between pages
- Publishing to local or remote servers

#### Site Glossary (#glossary)

Contains 13 default entries for common HTML expansions. Provides site-wide text substitutions that can be overridden at the page level.

#### Color Preferences (#prefs)

```usertalk
websites.#prefs.alink = "008000"      // Active link color (green)
websites.#prefs.bgcolor = "FFFFFF"    // Background color (white)
websites.#prefs.link = "0000FF"       // Link color (blue)
websites.#prefs.text = "000000"       // Text color (black)
websites.#prefs.vlink = "800080"      // Visited link color (purple)
```

Standard web color codes used for rendering default page styles.

---

### user.html Structure

Per-user HTML configuration organized under `user.html` (13 items):

| Item | Type | Purpose |
|------|------|---------|
| **callbacks** | Table | Callback scripts for page rendering hooks |
| **changedPages** | Table | Tracks which pages have been modified |
| **editors** | Table | Editor configurations |
| **fileWriters** | Table | File writer scripts for different output formats |
| **glossary** | Table | User's personal glossary (609 entries) |
| **images** | Table | Image management settings |
| **macroErrors** | Table | Error log for macro execution failures |
| **macros** | Table | User-defined macro definitions |
| **menu** | Table | Menu configurations |
| **prefs** | Table | User HTML preferences (22 items) |
| **renderers** | Table | Rendering engine scripts |
| **sites** | Table | Site-specific configurations |
| **templates** | Table | Template library |

---

## Preferences System

### user.html.prefs (22 preferences)

Complete reference of all preferences that control rendering behavior:

```usertalk
user.html.prefs.activeURLs = true                  // Auto-linkify URLs in macros
user.html.prefs.addLinefeeds = false               // Add line breaks to output
user.html.prefs.autoParagraphs = false             // Auto-wrap text in <p> tags
user.html.prefs.charset = "iso-8859-1"             // Character encoding
user.html.prefs.clayCompatibility = false          // Clay-specific rendering mode
user.html.prefs.defaultFileName = "index"          // Default file name (index.html)
user.html.prefs.directivesOnlyAtBeginning = true   // Process directives only at line start
user.html.prefs.dropNonAlphas = false              // Remove non-alphabetic characters
user.html.prefs.fileExtension = ".html"            // Default file extension
user.html.prefs.fullTimeNetConnection = false      // Network availability flag
user.html.prefs.imgFileCreator = ""                // Mac creator code for image files
user.html.prefs.includeMetaCharset = false         // Include charset meta tag
user.html.prefs.includeMetaGenerator = true        // Include generator meta tag
user.html.prefs.isFatPage = false                  // Fat page mode (stores multiple versions)
user.html.prefs.isoFilter = true                   // ISO 8859-1 character filtering
user.html.prefs.lowercasefilenames = false         // Lowercase generated filenames
user.html.prefs.maxFileNameLength = 31             // Maximum filename length
user.html.prefs.serverNetAddress = "255.255.255.255" // Server address (broadcast)
user.html.prefs.textFileCreator = ""               // Mac creator code for text files
user.html.prefs.useGlossPatcher = false            // Enable glossary patching (Phase 2)
user.html.prefs.useImageCache = false              // Cache processed images
user.html.prefs.useSemaphores = false              // Use OS semaphores for locking
user.html.prefs.webServerAppID = ""                // Web server application ID
```

### Preference Lookup Hierarchy

The `html.getPref()` verb follows this lookup order:

```
1. Page table specific preference (highest priority)
   pageTable.prefs.fileExtension

2. User preferences
   user.html.prefs.fileExtension

3. Default value (hardcoded in C)
   ".html"
```

This allows per-page customization while maintaining reasonable defaults.

**C Implementation**: See `htmlgetpref()` function in `Common/source/langhtml.c:483`

---

## Glossary System

### Overview

The glossary system provides content reuse and automatic linking through text substitution. Glossary entries can be:
- HTML snippets (e.g., `"copy" => "&copy;"`)
- Links (e.g., `"About" => '<a href="...">About</a>'`)
- Macros (e.g., `"TODAY" => '{clock.now()}'`)

### user.html.glossary (609 entries)

Sample entries showing the variety of glossary uses:

```usertalk
user.html.glossary[";->"] = '<img src="http://static.userland.com/shortcuts/images/qbullets/sidesmiley.gif">'
user.html.glossary["About Manila"] = '<a href="http://manila.userland.com/">About Manila</a>'
user.html.glossary["AFAIK"] = '<a href="http://www.userland.com/whatIsAfaik">AFAIK</a>'
user.html.glossary["copy"] = "&copy;"
user.html.glossary["nbsp"] = "&nbsp;"
```

### Glossary Lookup Order

The `html.refGlossary` UserTalk script (called from C at line 614 in langhtml.c) implements this hierarchy:

```
1. Page-specific glossary (highest priority)
   pageTable.glossary[name]

2. Parent table glossaries (walking up the hierarchy)
   parent^.glossary[name] or parent^.#glossary[name]

3. User glossary
   user.html.glossary[name]

4. Auto-link to sibling objects (fallback)
   Create link to object at same level with that name

5. Error if not found
   "There is no glossary entry named 'X'"
```

**Implementation Notes**:
- The C code calls UserTalk for glossary lookups: `langrunscript("html.refglossary", ...)`
- This creates a seamless hybrid of compiled performance and scripted flexibility
- Page-specific glossaries can override global definitions

### Glossary Patching

The `html.glossaryPatcher()` verb processes rendered HTML to convert glossary references into relative URLs:

**Pattern**: `[[#glossPatch name|text]]` → `<a href="../path/to/name">text</a>`

**Algorithm**:
1. Scans rendered HTML for `[[#glossPatch ` pattern
2. Extracts name and optional link text
3. Calculates relative URL by walking up from `adrObject` to `ftpsite`
4. Generates `<a href="...">text</a>` or just the URL
5. Replaces pattern in place

**Enabled by**: `user.html.prefs.useGlossPatcher = true`

**C Implementation**: See `glossarypatcherverb()` function in `Common/source/langhtml.c:3458-3670`

---

## Page Table Architecture

### How Page Tables Work

Page tables are runtime context structures that provide HTML verbs with the information they need to render pages correctly — things like URL structure, macro definitions, templates, glossary entries, and site preferences. They are populated by the webserver, mainResponder dispatchers, and other lower-level framework code during normal request processing.

**Many HTML verbs require an active page table** to function. If you see an error like "couldn't get pagetable address" or "Can't find a sub-table named pageTableAddresses", it means the page table wasn't set up before calling the verb.

### Activating a Page Table

The active page table is managed through two verbs:

- **`html.setPageTableAddress(@pt)`** — Sets the address of the table to use as the current page table context. Must be called before any HTML verb that needs page table context.
- **`html.getPageTableAddress()`** — Returns the address of the currently active page table.

In production, the webserver and mainResponder call `html.setPageTableAddress` automatically during request dispatch. For testing or standalone script usage, you must set it up manually:

```usertalk
local (pt);
new (tableType, @pt);
html.setPageTableAddress (@pt);

// Now HTML verbs that need page table context will work
html.processMacros ("{1+1}")  // Returns "2"
```

For more complex HTML operations that need populated page table fields (like template rendering or glossary patching), use `html.buildPageTable()` — but this is a complex path that requires a full site context. For most testing purposes, creating an empty page table and setting it active is sufficient.

### Required Page Table Fields

Page tables are the runtime context structures for web request processing. The C code expects these fields:

| Field Name | Type | Purpose | C String Constant |
|-----------|------|---------|-------------------|
| **adrObject** | Address | Address to the current page/object being rendered | `str_adrobject` (line 109) |
| **ftpsite** | Address | Address to FTP/file server root for relative URL building | `str_ftpsite` (line 110) |
| **renderedtext** | String | Fully rendered HTML output | `str_renderedtext` (line 104) |
| **fileextension** | String | File extension for output (.html, .php, etc.) | `str_fileextension` (line 111) |
| **defaultfilename** | String | Default filename (usually "index") | `str_defaultfilename` (line 114) |
| **template** | Script | Page template/layout script | `str_template` (line 107) |
| **glossary** | Table | Optional page-specific glossary | `str_glossary` (line 100) |
| **tools** | Table | Tool scripts available to this page | `str_tools` (line 99) |
| **indirectTemplate** | Boolean | Whether template is indirect reference | `str_indirecttemplate` (line 108) |
| **maxfilenamelength** | Number | Maximum length for generated filenames | `str_maxfilenamelength` (line 112) |
| **defaulttemplate** | Address | Default template if page-specific not found | `str_defaulttemplate` (line 113) |
| **directivesOnlyAtBeginning** | Boolean | Process directives only at line start | `str_directivesonlyatbeginning` (line 115) |

### Optional Fields

From the commented-out UserTalk in `langhtml.c`, additional fields that may be present:

```usertalk
pageTable.subdirectoryPath = "path/relative/to/ftpsite"
pageTable.adrSiteRootTable = @websites.mysite
pageTable.adrParentTable = @websites.mysite.pages.parent
pageTable.normalizedName = "filename"
pageTable.url = "/relative/url/path"
```

### Creating a Page Table

Minimal page table for testing Phase 2 HTML verbs:

```usertalk
local(pageTable);
lang.new(tableType, @pageTable);

// Required fields for basic operation
pageTable.adrObject = @websites.samples.default;  // Reference to this page
pageTable.ftpsite = @websites."#ftpSite";         // FTP site reference
pageTable.fileextension = ".html";                 // Output extension
pageTable.defaultfilename = "index";               // Default name
pageTable.renderedtext = "";                       // Will be filled by rendering

// Optional page-specific glossary
lang.new(tableType, @pageTable.glossary);
pageTable.glossary["TODAY"] = string(clock.now());
pageTable.glossary["VERSION"] = "1.0";
```

---

## C/UserTalk Integration

### Hybrid Architecture

The website framework demonstrates a sophisticated hybrid of C and UserTalk:

**C Runtime** handles:
- Text processing primitives (URL encoding, parsing)
- Performance-critical operations (macro expansion, text scanning)
- Security operations (XSS prevention via html.neutermacros/neutertags)
- File I/O operations

**UserTalk Scripts** handle:
- Business logic (glossary lookup hierarchy)
- Extensibility (callbacks, renderers, filters)
- Content generation (templates, directives)
- Site-specific customization

### C-to-UserTalk Callbacks

The C code calls UserTalk scripts at strategic points:

#### html.refGlossary Callback

```c
// Common/source/langhtml.c:614
fl = langrunscript(BIGSTRING("\x10" "html.refglossary"), &vparams, nil, &vresult)
     && strongcoercetostring(&vresult);
```

This allows the glossary lookup logic to be implemented in UserTalk, providing flexibility for customization while maintaining performance for the text scanning loop in C.

#### Runtime Context Structure (typrocessmacrosinfo)

The C code maintains this context during page processing:

```c
typedef struct typrocessmacrosinfo {
    hdlhashtable hpagetable;           // The page table
    hdlhashtable hstandardmacros;      // Standard macro definitions
    hdlhashtable huserprefs;           // User preferences
    hdlhashtable husermacros;          // User-defined macros
    hdlhashtable htools;               // Tool scripts
    hdlhashtable hmacrocontext;        // Macro execution context

    boolean flprocessmacros;           // Process {macro} substitutions
    boolean flexpandglossaryitems;     // Expand glossary references
    boolean flautoparagraphs;          // Auto-wrap in <p> tags
    boolean flactiveurls;              // Auto-linkify URLs
    boolean flclaycompatibility;       // Clay compatibility mode
    boolean flisofilter;               // ISO 8859-1 filtering
} typrocessmacrosinfo;
```

This structure is passed through the rendering pipeline, providing consistent access to configuration and state.

### Key C Functions

#### htmlgetpref() - Preference Lookup

```c
// Common/source/langhtml.c:483
boolean htmlgetpref(typrocessmacrosinfo *pmi, bigstring pref, tyvaluerecord *val)
```

Looks up preference in page table, then `user.html.prefs`, then returns default.

**Used by**: `html.getPref` verb (Phase 2)

#### htmlrefglossary() - Glossary Lookup

```c
// Common/source/langhtml.c:587
static boolean htmlrefglossary(typrocessmacrosinfo *pmi, Handle hreference,
                                bigstring perrorstring, Handle *hresult)
```

Called directly by macro expansion. Invokes `html.refglossary` UserTalk script.

**Note**: Commented out in verb switch statement (lines 9835-9836), but function exists and is actively used by macro processing.

#### glossarypatcherverb() - Post-Rendering URL Patching

```c
// Common/source/langhtml.c:3458
boolean glossarypatcherverb(hdltreenode hp1, tyvaluerecord *v)
```

**Phase 2**: `html.glossaryPatcher` implementation
Requires `renderedtext`, `adrObject`, `ftpsite` fields
Only processes if `useGlossPatcher` preference is true

#### rundirectiveverb() - Directive Processing

```c
// Common/source/langhtml.c:3102-3141
boolean rundirectiveverb(hdltreenode hp1, tyvaluerecord *v)
```

**Phase 2**: `html.runDirective` implementation
Evaluates directives like `#field = value` in text
Assigns results back to page table

---

## Page Rendering Pipeline

### Data Flow

```
1. Page Content (from ODB)
   └─> Raw text with macros and directives

2. html.processMacros() [Phase 1 - IMPLEMENTED]
   └─> Substitutes {macro} references
   └─> Expands [[glossary]] references via html.refGlossary
   └─> Result: HTML with evaluated macros

3. html.runDirective() [Phase 2 - PENDING]
   └─> Processes #field = value directives
   └─> Assigns values to page table
   └─> Result: Updated page table + HTML

4. Rendered HTML
   └─> Stored in pageTable.renderedtext

5. html.glossaryPatcher() [Phase 2 - PENDING]
   └─> Converts [[#glossPatch name|text]] to relative URLs
   └─> Result: Final HTML with working links

6. Output Generation
   └─> File writing (html.writeOutlineAsHtml)
   └─> FTP publishing (via fileWriters)
```

### Filter Pipeline

The website framework supports a three-stage filter pipeline:

```usertalk
websites.#filters.firstFilter(adrPageTable)   // Pre-processing
websites.#filters.pagefilter(adrPageTable)    // Main rendering
websites.#filters.finalfilter(adrPageTable)   // Post-processing
```

Each filter receives the page table and can modify:
- Page content (before rendering)
- Rendered HTML (after rendering)
- Page table fields (metadata, directives)

---

## Phase 2 Implementation Guide

### Prerequisites

Phase 2 HTML verbs require proper database initialization:

**System Root Required**:
```bash
./frontier-cli/frontier-cli --system-root databases/Frontier-v7.root -e "..."
```

This loads:
- `system.paths` initialization
- `user.html.prefs` table
- `user.html.glossary` table
- `root.websites` structure

**Why**: Many HTML verbs call UserTalk scripts that expect these tables to exist.

### Phase 2 Verbs

#### html.getPref(prefName, adrPageData=@websites.#data)

**Requirements**:
- User preferences table (`user.html.prefs`) - ALWAYS REQUIRED
- Page table preferences (if page-specific overrides needed)
- Falls back to `user.html.prefs` if not found in page table

**Test Example**:
```usertalk
local(ext);
ext = html.getPref("fileExtension", @websites."#data");
return ext == ".html"
```

#### html.runDirective(linetext, adrPageTable=@websites.#data)

**Requirements**:
- **adrObject** field (address to current page)
- **ftpsite** field (for URL generation)
- Page table with full context for directive evaluation

**Test Example**:
```usertalk
local(pageTable);
lang.new(tableType, @pageTable);
pageTable.adrObject = @websites.samples.default;
pageTable.ftpsite = @websites."#ftpSite";

html.runDirective("#title = \"Test Page\"", @pageTable);
return pageTable.title == "Test Page"
```

#### html.glossaryPatcher(adrPageData=@websites.#data)

**Requirements**:
- **renderedtext** field (pre-rendered HTML to patch)
- **adrObject** field (for relative URL calculation)
- **ftpsite** field (base URL for glossary links)
- **useGlossPatcher** preference must be true

**Test Example**:
```usertalk
local(pageTable);
lang.new(tableType, @pageTable);
pageTable.adrObject = @websites.samples.default;
pageTable.ftpsite = @websites."#ftpSite";
pageTable.renderedtext = "<p>See [[#glossPatch about|About Us]]</p>";

user.html.prefs.useGlossPatcher = true;
html.glossaryPatcher(@pageTable);
return string.contains(pageTable.renderedtext, "<a href=")
```

#### Image Analysis Verbs

**html.getGifHeightWidth(path)**
**html.getJpegHeightWidth(path)**

**Requirements**:
- File path (must be absolute, not relative, due to macOS sandbox)
- Read-only file access
- No page table needed

**Test Example**:
```usertalk
local(testDir = "{FRONTIER_TEST_TMP_DIR}");  // Replaced by test framework
local(dimensions);
dimensions = html.getGifHeightWidth(testDir + "/sample.gif");
return dimensions.height > 0 and dimensions.width > 0
```

**Critical**: Use `{FRONTIER_TEST_TMP_DIR}` template in integration tests (auto-replaced with safe path). For manual CLI testing, use `$(./tools/get_test_temp_path.sh)` to get a sandbox-accessible path.

---

## Test Fixture Specifications

### Minimal Glossary for Testing

```usertalk
local(glossary);
lang.new(tableType, @glossary);

glossary["<b>"] = "<b>";                           // Allow bold
glossary["</b>"] = "</b>";                         // Allow close bold
glossary["nbsp"] = "&nbsp;";                       // Non-breaking space
glossary["copy"] = "&copy;";                       // Copyright symbol
glossary["link.example"] = '<a href="http://example.com">Example</a>';
```

### Test Database Setup

For Phase 2 integration tests:

1. **System root is loaded** (`--system-root databases/Frontier-v7.root`)
   - Provides `system.paths` initialization
   - Loads `user.html.prefs` table
   - Loads `user.html.glossary` table

2. **Sample website structure exists** (`root.websites.samples`)
   - Provides realistic page tables for testing
   - Contains test preferences and glossaries

3. **Test page table** can be created with minimal fields (see section above)

### Integration Test Pattern

```yaml
# tests/integration/test_cases/html_phase2_tests.yaml
tests:
  - name: "html.getPref - default file extension"
    requires_system_root: true
    script: |
      local(ext = html.getPref("fileExtension"));
      return ext == ".html"
    expected_result: "true"

  - name: "html.glossaryPatcher - relative URL generation"
    requires_system_root: true
    script: |
      local(pageTable);
      lang.new(tableType, @pageTable);
      pageTable.adrObject = @websites.samples.default;
      pageTable.ftpsite = @websites."#ftpSite";
      pageTable.renderedtext = "[[#glossPatch about|About]]";

      user.html.prefs.useGlossPatcher = true;
      html.glossaryPatcher(@pageTable);
      return string.contains(pageTable.renderedtext, "<a href=")
    expected_result: "true"
```

---

## String Constants Reference

The C code uses these exact string constants to access page table fields. When implementing verbs, these field names **MUST** match exactly:

```c
// Common/source/langhtml.c:94-115
#define str_adrpagetable         "html.data.adrpagetable"
#define str_websitesdata         "websites.#data"
#define str_userhtmlprefs        "user.html.prefs"
#define str_usermacros           "user.html.macros"
#define str_standardmacros       "html.data.standardMacros"
#define str_tools                "tools"
#define str_glossary             "glossary"
#define str_images               "images"
#define str_glosspatch           "[[#glossPatch "
#define str_useglosspatcher      "useGlossPatcher"
#define str_renderedtext         "renderedtext"
#define str_template             "template"
#define str_indirecttemplate     "indirectTemplate"
#define str_adrobject            "adrobject"
#define str_ftpsite              "ftpsite"
#define str_fileextension        "fileextension"
#define str_maxfilenamelength    "maxfilenamelength"
#define str_defaulttemplate      "defaulttemplate"
#define str_defaultfilename      "defaultfilename"
#define str_directivesonlyatbeginning  "directivesOnlyAtBeginning"
```

**Critical**: Field names are case-sensitive. Use lowercase (e.g., `adrobject`, not `adrObject`) when accessing from C code.

---

## Security Considerations

### XSS Prevention

The framework provides two security verbs:

**html.neutermacros(text, safeMacros)**
Escapes dangerous macro calls (e.g., `{file.delete(...)}`) while allowing safe ones.

**html.neutertags(text, safeTags)**
Escapes dangerous HTML tags (e.g., `<script>`) while allowing safe ones.

**Usage Pattern**:
```usertalk
// Sanitize user-generated content
local(userHTML = ...);
userHTML = html.neutertags(userHTML, @config.safeTags);
userHTML = html.neutermacros(userHTML, @config.safeMacros);

// Store safely
db.setItem(path, userHTML);
```

**Best Practice**: Always sanitize user input before rendering or storing in the ODB.

---

## Migration from UserLand Frontier

### Compatibility Notes

Existing Frontier websites and scripts using HTML verbs should work without modification:

1. **Text processing verbs** - Work identically in headless mode
2. **Image dimension verbs** - Work identically (file paths must be absolute)
3. **Page framework verbs** - Work identically (require proper ODB initialization)
4. **Web server verbs** - Work identically (require network permissions)
5. **Calendar verbs** - Work identically (legacy feature, rarely used)
6. **`html.drawcalendar`** - Returns error (GUI not available in headless mode)

### Path Requirements

**Important**: All file paths in verbs must be absolute paths, not relative paths:

```usertalk
// CORRECT - Full paths
db.new("/Users/jake/test.root")
file.write("/tmp/output.txt", "data")

// CORRECT - Build full path from cwd
local(fullPath = file.getcwd() + "/test.root")
db.new(fullPath)

// WRONG - Relative paths don't work
db.new("test.root")              // Will fail or create in undefined location
file.write("output.txt", "data") // Will fail
```

**Rationale**: UserTalk runtime has no concept of current working directory at the script level. File operations require absolute paths.

---

## References

### Documentation
- **HTML Verb Reference**: `docs/usertalk/docserver/html/` (38 verb docs)
- **Verb Implementation Guide**: `docs/VERB_IMPLEMENTATION_GUIDE.md`
- **Testing Guide**: `docs/TESTING_GUIDE.md`
- **CLI Usage Guide**: `docs/CLI_USAGE_GUIDE.md`

### C Implementation
- **HTML Verbs**: `Common/source/langhtml.c` (9,800+ lines)
- **String Constants**: `Common/source/langhtml.c:94-115`
- **Verb Dispatch**: `tests/headless_html_verbs.c`

### UserTalk Scripts
- **System Scripts**: `usertalk_scripts/Frontier.root/system/verbs/builtins/html/`
- **Website Framework**: `databases/Frontier-v7.root` (root.websites, user.html)

### Planning Documentation
- **HTML Verbs Plan**: `planning/phase3/html_verbs_implementation_plan.md`
- **Phase Overview**: `planning/phase_overview.md`

---

## Conclusion

Frontier's website framework represents a sophisticated hybrid architecture that successfully balanced performance (C runtime), flexibility (UserTalk scripting), and usability (intuitive ODB structures). This documentation provides the foundation for Phase 2 HTML verb implementation and serves as a comprehensive reference for understanding how Frontier's web publishing system works.

The framework's design demonstrates remarkable foresight in separating concerns (content, presentation, logic) and providing extensibility through callbacks, filters, and renderers. Many of these patterns remain relevant for modern web publishing systems.

---

**Last Updated**: 2026-01-14
**Contributors**: Claude Code (documentation), Dave Winer (original design), Doug Baron and UserLand Software (implementation)
