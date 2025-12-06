# Processor Audit: `op` (Outline Processor)

**Status:** ✅ **HEADLESS-COMPATIBLE** (45 Kernel Verbs + 5 Attributes + 7 Scripts)
**Audit Date:** 2025-12-06
**Auditor:** Claude (Haiku 4.5)

---

## Basic Information

| Property | Value |
|----------|-------|
| **Processor Name** | `op` (Outline) |
| **EFP ID** | 1000 |
| **Verb Count** | 45 (outline ops) + 5 (attributes) + 7 (utility scripts) = 57 total |
| **Window Required** | NO (kernelverbs.rc flag is legacy artifact) |
| **Documentation** | [outline/](../../../docs/usertalk/docserver.userland.com/op/index.html) |
| **Script Implementation** | `system.verbs.builtins.op` (Frontier.root) |

---

## Category Assessment

**Category:** ✅ **Outline Data Structure Manipulation** (HEADLESS-COMPATIBLE)

**Rationale:**
Outline processor provides operations for manipulating outline objects as data structures through the `target` system. The `target.set(adrOutline)` pattern points the runtime context to an outline object (in memory), enabling data structure operations (insert, delete, navigate, expand/collapse) on the outline tree. No GUI window display is required - outlines exist as in-memory hierarchical data that can be created, modified, and serialized entirely in headless environments.

**Headless Compatibility:** ✅ **FULL** (45/45 kernel verbs are headless-compatible)

**Note on Window Flag:**
The `Window required = true` flag in kernelverbs.rc is a legacy artifact from GUI Frontier. Outline processor operates on data structures via the target system, not on GUI windows. Outlines can be created, modified, and serialized without any window display.

---

## Verb Inventory

### Core Outline Operations (45 kernel verbs)

| Category | Verbs | Count |
|----------|-------|-------|
| **Navigation** | go, firstSummit, getCursor, setCursor, level | 5 |
| **Display** | getDisplay, setDisplay, getScrollState, setScrollState | 4 |
| **Structure** | getLineText, setLineText, insert, deleteLine, deleteSubs | 5 |
| **Hierarchy** | promote, demote, expand, collapse, subsExpanded, hoist, deHoist | 7 |
| **Selection** | getSelection, getSelectedSubOutlines | 2 |
| **Metrics** | countSubs, countSummits, getheadNumber | 3 |
| **Search** | find, sort | 2 |
| **Outline Extraction** | getSubOutline, insertOutline, outlineToList, outlineToXml | 4 |
| **Format** | setHtmlFormatting, getHtmlFormatting, getDynamic, setDynamic | 4 |
| **Advanced** | tabKeyReorg, flatCursorKeys, visitAll, getExpansionState, setExpansionState | 5 |
| **Other** | reorg, setRefCon, getRefCon, setModified, xmlToOutline | 5 |

**Total: 45 kernel verbs**

### Outline Attributes Sub-processor (5 verbs via opattributes)

| # | Verb | Signature |
|---|------|-----------|
| 1 | `op.attributes.addGroup` | `addGroup(name)` |
| 2 | `op.attributes.getAll` | `getAll() -> table` |
| 3 | `op.attributes.getOne` | `getOne(name) -> value` |
| 4 | `op.attributes.makeEmpty` | `makeEmpty()` |
| 5 | `op.attributes.setOne` | `setOne(name, value)` |

### Utility Scripts (4 main implementations)

| # | Script | Complexity |
|---|--------|-----------|
| 1 | `outlineToList` | HIGH - Recursive tree traversal, optional parameters |
| 2 | `listToOutline` | HIGH - Builds outline structure from list |
| 3 | `newOutlineObject` | MEDIUM - Creates outline at address, initializes structure |
| 4 | `outlineToXml` | HIGH - Complex XML serialization (9KB) |
| 5 | `xmlToOutline` | HIGH - Complex XML deserialization (5KB) |
| 6 | `rssToOutline` | MEDIUM - Converts RSS feed to outline |
| 7 | `visitAll` | MEDIUM - Iterates outline with callback |

---

## Implementation Analysis

### Complexity: **HIGH** (Complex Tree Data Structure)

### Dependencies

- **Other Processors:**
  - target (context management for outline pointers)
  - wp (text handling for outlines)
  - table (for attribute management)
  - string (text processing)
  - date (timestamps for outline metadata)
  - xml (for serialization)
- **GUI Components:** NONE (data structure only)
- **OS-Specific:** NO

### Key Implementation Notes

**Architecture:**

All outline operations use the `target.set(adr)` pattern to point the runtime context at an outline data structure:

```
1. Save current target with target.get()
2. Switch context to outline with target.set(@myOutline)
3. Perform operations on outline data (in-memory tree)
4. Restore previous target with target.set(oldTarget)
```

The outline exists as an in-memory hierarchical data structure. The `target` system allows multiple independent outline objects to be manipulated sequentially. This is data-structure manipulation, not GUI window manipulation.

**Outline Data Structure:**
- Tree nodes with head text (line content) and sub-heads (children)
- Cursor position (current node being operated on)
- Expansion state (which nodes are expanded/collapsed)
- Attributes table (metadata per node)
- Display state (though irrelevant in headless context)

**Example Usage Pattern:**
```usertalk
local (oldTarget = target.get())
target.set(@myOutline)
op.insert("New line", down)
op.promote()
op.expand()
target.set(oldTarget)
```

**Data Structure:**
Outlines are trees with:
- Head text (line content)
- Sub-heads (child nodes)
- Cursor position (current selection)
- Expansion state (collapsed/expanded)
- Attributes table (metadata per node)
- Display state (visible/hidden)

**Key Operations:**

1. **Navigation**: go(direction, count) - Move cursor up/down/left/right
2. **Insert**: insert(text, position) - Add line at position (down, right, up, left)
3. **Hierarchy**: promote/demote - Change nesting level
4. **Expansion**: expand/collapse - Show/hide sub-heads
5. **Conversion**: outlineToList/listToOutline - Bidirectional tree↔list conversion
6. **Format**: XML serialization for persistence

**GUI Integration Points:**

- **Cursor management**: getCursor/setCursor requires window
- **Display rendering**: getDisplay/setDisplay for performance
- **Selection tracking**: getSelection/getSelectedSubOutlines
- **Expansion state**: Track which nodes are expanded (UI state)

---

## Implementation Status

**Current State:**
- ✅ 45 kernel verbs defined in kernelverbs.rc
- ✅ 5 attribute verbs (opattributes namespace)
- ✅ 7 complex UserTalk script implementations
- ⏳ Kernel verb implementations (all require outline window)
- ⚠️ Outline data structure (in-memory tree + GUI integration)
- ⚠️ Target/window context management

**What Needs Implementation:**

1. **Outline Data Structure:**
   - Tree node structure with head text + sub-heads
   - Cursor position tracking
   - Expansion state management
   - Attribute table per node
   - Serialization (native + XML formats)

2. **Navigation Verbs:**
   - go(direction, count) - Tree traversal
   - getCursor/setCursor - Cursor positioning
   - firstSummit - Go to first node

3. **Edit Verbs:**
   - insert(text, position) - Add nodes
   - deleteLine/deleteSubs - Remove nodes
   - setLineText/getLineText - Modify content
   - promote/demote - Change nesting

4. **Display Verbs:**
   - expand/collapse - Show/hide children
   - setDisplay(boolean) - Performance optimization
   - getDisplay/setScrollState - UI state

5. **Complex Operations:**
   - outlineToList/listToOutline - Tree conversions
   - outlineToXml/xmlToOutline - Serialization
   - sortBy/visitAll - Traversal with operations
   - getSubOutline/insertOutline - Subtree manipulation

---

## Headless Compatibility Analysis

**Fully Compatible:** ✅ YES (45/45 kernel verbs)

**Headless-Compatible Verbs (45):**
- Navigation: go, firstSummit, level (3)
- Manipulation: insert, deleteLine, deleteSubs, promote, demote (5)
- Structure: expand, collapse, subsExpanded, hoist, deHoist (5)
- Cursor/State: getCursor, setCursor, getDisplay, setDisplay, getScrollState, setScrollState (6)
- Selection: getSelection, getSelectedSubOutlines (2)
- Metrics: countSubs, countSummits, getHeadNumber (3)
- Text: getLineText, setLineText (2)
- Attributes: getRefCon, setRefCon (2)
- Search/Sort: find, sort (2)
- Metadata: getExpansionState, setExpansionState (2)
- Outline extraction: getSubOutline, insertOutline, outlineToList, outlineToXml, xmlToOutline (5)

**Why Fully Compatible:**
- Operates on outline data structures (in-memory trees)
- Target system allows pointing to outline objects without windows
- Cursor/expansion state is data, not GUI state
- All operations manipulate tree structure and metadata
- Serialization (XML/list conversion) is pure data transformation
- No window display required

**Recommendation:** ✅ **ESSENTIAL FOR HEADLESS**
- Outline data structures are core to Frontier
- Scripts stored as outlines (op.* for script editing)
- Database configuration often stored in outlines
- Serialization/import/export widely used
- Zero GUI dependencies when using target system properly

---

## Priority & Sequencing

**Priority:** 🏆 **CRITICAL** (Tier 1 - Core Data Structure)

**Recommended Implementation Order:** Early (after database basics, alongside script/table)

**Rationale:**
- Core data structure type (like tables, lists, records)
- Required for script object manipulation
- Required for configuration management
- Serialization support (XML, list format) needed
- Must implement regardless of headless/GUI distinction

---

## Related Processors

- **script** - Script editing (shares kernelverbs.rc resource block)
- **osa** - AppleScript (shares kernelverbs.rc resource block)
- **wp** - Word processor (similar text operations)
- **table** - Table processor (similar data structures)
- **target** - Window/context management (critical dependency)

---

## Special Considerations

**Memory Management:**
- Outline trees can be very large (1000+ nodes)
- Need efficient tree data structure
- Cursor/expansion state overhead

**Performance:**
- setDisplay(false) optimization mentioned in scripts
- Large outline operations can be slow
- Recursive algorithms for tree traversal

**File Formats:**
- Native Frontier outline format (binary)
- XML serialization (documented in outlineToXml script)
- List conversion format (tabs for nesting)

**Attributes:**
- Per-node metadata (op.attributes.*)
- Stored in outline structure
- Used for semantic markup

---

## Audit Conclusions

**Status:** ✅ **CRITICAL FOR HEADLESS + GUI FRONTIER**

**Key Findings:**
1. **Fully headless-compatible** - Target system enables data structure manipulation without windows
2. **Core data structure** - Outline is fundamental like tables and lists
3. **Target-based operations** - `target.set(adr)` points to outline data, not GUI windows
4. **Wide applicability** - Scripts stored as outlines, configuration in outlines, documents as outlines
5. **Complex but essential** - 45 verbs + tree data structure + serialization support

**Recommendation:** IMPLEMENT EARLY - Essential for all Frontier builds (headless or GUI)

**Implementation Priority:** TIER 1 (after database basics, alongside table/script)

---

## References

**Documentation:**
- DocServer: `docs/usertalk/docserver.userland.com/op/`

**Kernel Definitions:**
- `Common/resources/Win32/kernelverbs.rc` (lines 34-94)
- ID 1000: op (45 verbs)
- Sub-resource: opattributes (5 verbs)

**Scripts:**
- `system.verbs.builtins.op/` (54 scripts)
- Key: outlineToList.ut, outlineToXml.ut (complex implementations)

---
