# Processor Audit: `table`

**Status:** ⚠️ **MIXED** (18 Kernel Verbs + 13 Scripts, ~60% Headless-Compatible)
**Audit Date:** 2025-12-06
**Auditor:** Claude (Haiku 4.5)

---

## Basic Information

| Property | Value |
|----------|-------|
| **Processor Name** | `table` |
| **EFP ID** | 1001 |
| **Verb Count** | 18 (kernel) + 13 (utility scripts) = 31 total |
| **Window Required** | YES (but scripts are mostly headless-compatible) |
| **Documentation** | [table/](../../../docs/usertalk/docserver.userland.com/table/index.html) |
| **Script Implementation** | `system.verbs.builtins.table` (Frontier.root) |

---

## Category Assessment

**Category:** ✅ **Database/Data Structure Operations** (Mostly Headless-Compatible)

**Rationale:**
Table processor provides operations for manipulating Frontier's table (hash table/dictionary) data structures. Operations include navigation, CRUD operations, serialization (XML/files), and validation. While kernelverbs.rc marks `Window required = true`, most actual functionality is headless-compatible. GUI-dependent verbs are limited to cursor/display/selection operations in the table editor.

**Headless Compatibility:** ⚠️ **Partial** (~60% compatible, ~40% GUI-dependent)

**GUI Blocking Verbs:**
- getCursor, getSelection, getDisplaySettings, setDisplaySettings (8 verbs)
- Navigation verbs that work with editor cursor (go, goto, gotoName - 3 verbs)
- Total GUI-only: ~11 of 31 (35%)

**Headless-Usable Verbs:**
- move, copy, rename, moveAndRename, assign, validate, sortBy (7 core)
- tableToXml, tableToFiles, xmlToTable (3 serialization)
- emptyTable, packtable, jettison (3 data operations)
- Utility scripts: compareContents, copyContents, moveContents, etc. (~10 more)
- Total Headless: ~20 of 31 (65%)

---

## Verb Inventory

### Core Kernel Verbs (18 from kernelverbs.rc)

| Category | Verb | Status | Description |
|----------|------|--------|-------------|
| **CRUD** | move | ✅ Headless | Move table/item to new location |
| | copy | ✅ Headless | Copy table/item |
| | rename | ✅ Headless | Rename table/item |
| | moveAndRename | ✅ Headless | Move and rename in one operation |
| | assign | ✅ Headless | Assign value to table element |
| **Validation** | validate | ✅ Headless | Validate table structure |
| | jettison | ✅ Headless | Remove empty suites/tables |
| | packTable | ✅ Headless | Optimize table storage |
| | emptyTable | ✅ Headless | Clear all contents |
| **GUI Navigation** | getCursor | ❌ GUI-Only | Get table editor cursor position |
| | getSelection | ❌ GUI-Only | Get selected items in editor |
| | go | ❌ GUI-Only | Move cursor (direction, count) |
| | goto | ❌ GUI-Only | Go to item by address |
| | gotoName | ❌ GUI-Only | Go to named item |
| **Display** | getDisplaySettings | ❌ GUI-Only | Get table window display prefs |
| | setDisplaySettings | ❌ GUI-Only | Set table window display prefs |
| **Sorting** | sortBy | ✅ Headless | Sort table by column(s) |
| **Query** | getSortOrder | ✅ Headless | Get current sort order |

### Utility Scripts (12+ UserTalk implementations)

| Script | Complexity | Headless | Description |
|--------|-----------|----------|-------------|
| tableToXml | HIGH | ✅ YES | Serialize table to XML (complex type handling) |
| tableToFiles | EXTREME | ✅ YES | Export table to file/folder hierarchy (9KB script) |
| xmlToTable | HIGH | ✅ YES | Deserialize XML to table |
| compareContents | MEDIUM | ✅ YES | Compare two tables |
| copyContents | MEDIUM | ✅ YES | Copy table contents |
| moveContents | MEDIUM | ✅ YES | Move table contents |
| visit | MEDIUM | ✅ YES | Iterate table with callback |
| visitOpenDatabases | MEDIUM | ✅ YES | Iterate open database tables |
| uniqueName | MEDIUM | ✅ YES | Generate unique name in table |
| newSuite | MEDIUM | ✅ YES | Create new table suite |
| promptNewItem | LOW | ❌ GUI-Only | Prompt user for new item (dialog) |
| surePath | LOW | ✅ YES | Ensure path exists in table |

---

## Implementation Analysis

### Complexity: **MEDIUM** (Mix of Kernel Verbs + Complex Scripts)

### Dependencies

- **Other Processors:**
  - file (for tableToFiles serialization)
  - string (for XML encoding, text manipulation)
  - date (for timestamps in XML/serialization)
  - xml (for entity encoding in table XML)
  - op (for outline serialization, if table contains outlines)
  - window (for display settings - GUI only)
  - target (for context switching - GUI only)
- **OS-Specific:** NO
- **GUI/Window Context:** YES (but can be disabled for headless)

### Key Implementation Notes

**Architecture:**

Tables in Frontier are essentially hash tables/dictionaries:
```
table.item1 = "value"
table.item2 = {a:1, b:2}  // nested table
table.item3 = @address    // reference to another object
```

**Kernel Verbs** typically operate directly on database:
- move, copy, rename - File-like operations on table entries
- sortBy - In-place sorting by column(s)
- emptyTable, packTable - Maintenance operations

**Script Utilities** provide complex operations:

1. **tableToXml** - Converts table to XML representation
   - Handles all types: scalars, tables, outlines, scripts, menubars
   - Entity encoding for special characters
   - Supports optional address inclusion
   - Uses target.set() for wpText and menubar (GUI parts)

2. **tableToFiles** - Export table structure to disk
   - Creates folders for tables
   - TSV files for scalars (_scalars.tsv)
   - Type-specific extensions (.scpt for scripts, .opml for outlines)
   - Advanced filename normalization
   - Handles deep nesting and large structures
   - HEADLESS-COMPATIBLE (pure data export)

3. **xmlToTable** - Import XML back to table
   - Reverse of tableToXml
   - Rebuilds nested structure
   - Type reconstruction from XML attributes

**GUI-Dependent Operations:**
- Cursor navigation (go, goto) - Uses table editor window cursor
- Display settings - Table window rendering preferences
- Selection - Editor selection of items in table
- Dialog prompts - promptNewItem (user interaction)

**Headless Strategy:**
For headless operation, skip GUI verbs:
- Use move/copy/rename instead of editor navigation
- Use tableToXml/tableToFiles for export instead of editor display
- Use xmlToTable for import instead of editor manipulation
- Skip getCursor, getSelection, promptNewItem, getDisplaySettings

---

## Implementation Status

**Current State:**
- ✅ 18 kernel verbs defined in kernelverbs.rc
- ✅ 13+ UserTalk script utilities implemented
- ✅ Complex XML/file serialization (tableToXml, tableToFiles)
- ✅ Type handling for all Frontier data types
- ⏳ Core kernel verbs need implementation
- ⏳ XML processing (tableToXml/xmlToTable)
- ⏳ File export (tableToFiles)

**What Needs Implementation:**

**Tier 1 - Core (High Priority):**
1. Move/copy/rename operations - Database manipulation
2. emptyTable/packTable - Data maintenance
3. sortBy/getSortOrder - Data organization
4. validate/jettison - Data integrity
5. assign - Value assignment (may be scriptable)

**Tier 2 - Serialization (Important for Headless):**
1. tableToXml - Complex type handling and entity encoding
2. tableToFiles - File export with normalization
3. xmlToTable - XML import and reconstruction
4. visit/visitOpenDatabases - Table iteration

**Tier 3 - GUI-Dependent (Skip for Headless):**
1. getCursor/getSelection - Editor cursor operations
2. go/goto/gotoName - Navigation (use direct assignment instead)
3. getDisplaySettings/setDisplaySettings - UI preferences
4. promptNewItem - User dialogs

**Implementation Pattern:**
```c
// Core table operations
boolean tablemove(hdltreenode fromAdr, hdltreenode toAdr) {
    // Get item at fromAdr
    // Move to database location at toAdr
    // Update database references
    return true;
}

// Serialization (can leverage existing XML code)
boolean tabletabletoxml(hdltreenode tableAdr, bigstring xmlResult) {
    // Walk table structure
    // Serialize each type appropriately
    // Return XML string
    return true;
}
```

---

## Testing Requirements

**Basic Operations (Headless-Compatible):**

```usertalk
// Create and manipulate
new (tableType, @t)
table.assign (@t.x, 10)
table.assign (@t.name, "test")

// Move/copy/rename
table.copy (@t, @t2)
table.rename (@t2, "copy")
table.move (@t2, @t.nested)

// Serialization
local (xml = table.tableToXml (@t))
local (t3)
table.xmlToTable (xml, @t3)

// Export to files
table.tableToFiles (@t, "/tmp/export/")
```

**Complex Cases:**
- Nested tables (deep nesting)
- Mixed type tables (scalars + outlines + scripts)
- Large tables (1000+ entries)
- Circular references (if possible)
- Unicode handling in XML
- Special characters in names (normalization)

**Edge Cases:**
- Empty tables
- Tables with nil values
- Binary data in tables (base64 encoding)
- Address references within exported data
- Very large file exports (>100MB)

---

## Implementation Effort

**Estimated Time:** 12-16 hours

**Breakdown:**
- Kernel verbs (move/copy/rename/assign): 4-5 hours
- Maintenance verbs (emptyTable/packTable): 1-2 hours
- Serialization (tableToXml/xmlToTable): 4-5 hours
- File export (tableToFiles): 2-3 hours
- Testing & debugging: 2-3 hours

**Confidence:** HIGH (Scripts exist, clear design patterns)

**Blockers:**
- Database layer (must support move/copy/rename)
- XML handling library
- File I/O system

---

## Priority & Sequencing

**Priority:** 🟢 **HIGH** (Tier 1 - Core Data Operations)

**Recommended Implementation Order:** Early (after database basics)

**Prerequisites:**
- Database CRUD operations (db processor)
- Basic table/list data structures
- file processor (for tableToFiles)
- XML processing library

**Headless Sequencing:**
1. Implement core CRUD verbs (move/copy/rename/assign)
2. Implement maintenance verbs (emptyTable/packTable)
3. Implement serialization (tableToXml/xmlToTable)
4. Implement file export (tableToFiles)
5. Skip GUI verbs entirely

---

## Headless Compatibility Analysis

**Fully Compatible:** ⚠️ PARTIAL (20/31 verbs = 65%)

**Headless-Compatible Verbs (20):**
- CRUD: move, copy, rename, moveAndRename, assign (5)
- Data Ops: emptyTable, packTable, jettison, validate (4)
- Sorting: sortBy, getSortOrder (2)
- Serialization: tableToXml, tableToFiles, xmlToTable (3)
- Utilities: compareContents, copyContents, moveContents, visit, visitOpenDatabases, uniqueName, newSuite, surePath (8)

**GUI-Only Verbs (11):**
- Navigation: go, goto, gotoName, getCursor (4)
- Display: getSelection, getDisplaySettings, setDisplaySettings (3)
- User Input: promptNewItem (1)

**Headless Strategy:**
✅ **RECOMMENDED FOR HEADLESS** (implement 20 compatible verbs)
- Skip 11 GUI-dependent verbs
- Use move/copy/rename instead of editor navigation
- Use tableToFiles instead of editor export
- Use tableToXml for serialization

---

## Related Processors

- **db** - Database management (tables are stored in databases)
- **file** - File I/O (tableToFiles uses file operations)
- **xml** - XML processing (tableToXml uses xml entity encoding)
- **op** - Outline operations (table can contain outlines)
- **script** - Script objects (table can contain scripts)
- **window** - Window management (for table editor - GUI only)

---

## Special Considerations

**Data Type Handling (in tableToXml):**
- Scalars: string, number, date, boolean, char
- Complex: table, list, record, outline, script, menubar
- Special: binary (base64 encoded), address, filespec, direction
- References: Addresses within exported data (tricky!)

**Filename Normalization (in tableToFiles):**
- Uses string.urlEncode + custom parenthesis format
- Example: Tab → %09 → (09)
- Preserves spaces for readability
- Ensures valid filenames on all platforms

**Performance Considerations:**
- Large table export (>1000 entries) can be slow
- XML string concatenation (consider string buffer)
- Recursive table traversal depth limits
- File I/O buffering for large exports

**XML Encoding:**
- Entity encoding for special chars: &, <, >, ", '
- Character references for control chars: &#NNN;
- CDATA sections for large text (if needed)

**Address Handling:**
- tableToXml can include addresses as attributes
- xmlToTable must rebuild references (non-trivial!)
- Circular reference detection needed

---

## Implementation Status Summary

**Status:** ✅ **VIABLE FOR HEADLESS** (65% compatible)

**Key Findings:**
1. **Core functionality is headless-compatible** (move/copy/rename/serialize)
2. **GUI operations are clearly delineated** (cursor/display/selection)
3. **Complex serialization already designed** (scripts show clear patterns)
4. **Strong support for data export** (tableToFiles/tableToXml)
5. **Mix of kernel verbs + scripts** (need both)

**Headless Implementation Strategy:**
- Implement 20 compatible verbs (65% of processor)
- Skip 11 GUI-dependent verbs (35%)
- Result: Fully functional table operations for headless use
- File export/import fully supported

**Recommendation:** HIGH PRIORITY for headless implementation

---

## References

**Documentation:**
- DocServer: `docs/usertalk/docserver.userland.com/table/`

**Kernel Definitions:**
- `Common/resources/Win32/kernelverbs.rc` (lines 121-145)
- ID 1001: table (18 verbs)

**Scripts:**
- `system.verbs.builtins.table/` (31 scripts)
- Key: tableToXml.ut, tableToFiles.ut, xmlToTable.ut

**Standards:**
- XML 1.0 (W3C)
- RFC 3986 (URL encoding)

---
