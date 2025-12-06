# Processor Audit: `op` (Outline Processor)

**Status:** ⚠️ **GUI-Dependent** (45 Kernel Verbs + 5 Attributes + 4 Scripts)
**Audit Date:** 2025-12-06
**Auditor:** Claude (Haiku 4.5)

---

## Basic Information

| Property | Value |
|----------|-------|
| **Processor Name** | `op` (Outline) |
| **EFP ID** | 1000 |
| **Verb Count** | 45 (outline ops) + 5 (attributes) + 4 (utility scripts) = 54 total |
| **Window Required** | YES ⚠️ |
| **Documentation** | [outline/](../../../docs/usertalk/docserver.userland.com/op/index.html) |
| **Script Implementation** | `system.verbs.builtins.op` (Frontier.root) |

---

## Category Assessment

**Category:** ⚠️ **GUI-Dependent Data Structure** (Outline Editor Operations)

**Rationale:**
Outline processor provides operations for manipulating outline objects in the GUI editor. Operations include navigation, editing (insert/delete/promote/demote), expansion/collapse, sorting, and format control. All verbs require active outline window context (`target.set(adr)` pattern used throughout scripts). Window requirement is explicitly marked as `true` in kernelverbs.rc.

**Headless Compatibility:** ❌ **NO** (Heavy GUI dependencies)

**GUI Blocking Verbs:** All 54 verbs require outline window context

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

### Complexity: **VERY HIGH** (GUI-Dependent + Complex Data Structures)

### Dependencies

- **Other Processors:**
  - target (window/outline context management)
  - wp (text handling for outlines)
  - table (for attribute management)
  - string (text processing)
- **GUI Components:**
  - Outline editor window
  - Outline data structure (in-memory tree)
  - Selection/cursor tracking
- **OS-Specific:** NO (but GUI framework dependent)

### Key Implementation Notes

**Architecture:**

All outline operations use the `target.set(adr)` pattern:
1. Save current target with `target.get()`
2. Switch to outline target with `target.set(adrOutline)`
3. Perform operations on outline in GUI
4. Restore previous target with `target.set(oldTarget)`

This pattern requires an active outline window context, making headless operation impossible.

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

**Fully Compatible:** ❌ NO (0/54 verbs)

**Critical Issues:**
1. **Requires outline window** - kernelverbs.rc marks `Window required = true`
2. **Uses `target.set()`** - All scripts switch to outline context, requires GUI
3. **Cursor/selection state** - GUI-managed cursor position and selection
4. **Expansion state** - Tracks which nodes are collapsed (UI state)
5. **No batch mode** - Operations are interactive, line-by-line

**Workaround for Headless:**
- **Outline data structure only** - No GUI operations
- **Direct data access** - Skip kernel verbs, use internal structure
- **No editor integration** - Can't use op.* verbs in headless

**Example Alternative (Headless):**
```usertalk
// Instead of using op.* verbs:
local (outline = myOutlineData)
// Direct manipulation:
outline.subs = {...}
outline.text = "new text"
// No cursor, no display state
```

---

## Priority & Sequencing

**Priority:** 🔴 **LOW** (Not Suitable for Headless)

**Recommendation:** ⚠️ **SKIP for Headless Implementation**
- GUI-dependent by design
- Heavy window context requirements
- No meaningful headless alternative
- Adds significant complexity without benefit

**If Required for GUI Frontier:**
- Implement after core I/O processors
- Complex window integration
- 40-60 hours estimated effort
- Requires complete outline editor implementation

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

**Status:** ⚠️ **NOT RECOMMENDED FOR HEADLESS**

**Key Findings:**
1. **Completely GUI-dependent** - Cannot function without outline window context
2. **Heavy window integration** - Every operation requires `target.set(adr)`
3. **UI State Management** - Tracks cursor, expansion, selection (GUI-only)
4. **Complex Implementation** - 54 verbs + large data structure + editor integration
5. **No headless value** - Outline editor meaningless without GUI

**Recommendation:** Skip outline processor for headless Frontier builds

**Implementation Priority for GUI Frontier:** LOW-MEDIUM (Complex but essential for full editor functionality)

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
