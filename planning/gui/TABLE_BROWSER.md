# Table Browser Specification

| | |
|---|---|
| **Version** | 0.1.0 |
| **Status** | Draft |
| **Last Updated** | 2026-02-01 |

## Change History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 0.1.0 | 2026-02-01 | Jake Savin, Claude | Initial draft from interview |

---

## Overview

The table browser is the primary interface for navigating and editing the Object Database (ODB). It presents a hierarchical view of a table and its descendants in an outliner-style three-column layout.

This is the first editor to be implemented and serves as the reference for the GUI protocol design.

---

## Visual Structure

### Window Layout

```
┌─────────────────────────────────────────────────────────┐
│  workspace.scratchpad                             _ □ × │
├─────────────────────────────────────────────────────────┤
│  Name              │ Value                │ Kind        │
├─────────────────────────────────────────────────────────┤
│ ▷ greeting         │ "Hello, World!"      │ string      │
│ ▷ count            │ 42                   │ number      │
│ ▼ config           │ {3 items}            │ table       │
│    ▷ debug         │ true                 │ boolean     │
│    ▷ timeout       │ 30                   │ number      │
│    ▶︎ advanced      │ {5 items}            │ table       │
│ ▷ myScript         │ {12 lines}           │ script      │
│ ▶︎ data             │ {1,247 items}        │ table       │
├─────────────────────────────────────────────────────────┤
│ [Kind ▾] [Sort ▾]                                       │
└─────────────────────────────────────────────────────────┘
```

### Components

- **Title bar:** Shows the dot-path of the table being edited (e.g., `workspace.scratchpad`). When displaying the root table of a database, shows the filename (e.g., `Frontier.root`, `mainResponder.root`)
- **Column headers:** Name, Value, Kind — clickable to sort
- **Row area:** Scrollable, virtualized list of objects
- **Type popup:** Lower-left, for selecting type when creating/converting objects
- **Sort popup:** Next to type popup, for selecting sort column and direction

### Row States

| Icon | Meaning |
|------|---------|
| ▶︎ | Sub-table, collapsed (has children, click to expand) |
| ▼ | Sub-table, expanded (showing children) |
| ▷ | Non-expandable (scalar, script, or other external) |

### Indentation

Child objects are indented under their parent. Indentation depth indicates nesting level.

---

## Columns

### Name Column

- Displays the object's name
- Click to enter rename mode (inline text editing)
- Must be unique within parent table

### Value Column

- **Scalars:** Display the actual value; click to edit inline
- **Tables:** Display child count, e.g., `{3 items}` or `{1,247 items}`
- **Scripts:** Display line count, e.g., `{12 lines}`
- **Other externals:** Display summary (same format as `list` command output)

### Kind Column

- Displays human-readable type name: `string`, `number`, `boolean`, `table`, `script`, `outline`, etc.

---

## Interactions

### Expand / Collapse

- Click ▶︎ to expand a sub-table (icon becomes ▼, children appear indented below)
- Click ▼ to collapse (children hidden, icon becomes ▶︎)
- Double-click caret (▶︎ or ▼) to toggle expanded/collapsed state
- Keyboard: Right arrow to expand, Left arrow to collapse

### Inline Editing

- **Rename:** Click on a name to enter text edit mode; Enter to confirm, Escape to cancel
  - Double-click on name enters text edit mode on first click, selects word on second click
- **Edit value:** Click on a scalar's value to edit inline; Enter to confirm, Escape to cancel
- Tab moves to next editable cell; Shift+Tab moves to previous

### Open Editor

- **Externals (script, outline, wptext, etc.):** Double-click anywhere in the row (caret, Value column, or Kind column) to open editor
  - Exception: Double-click on Name enters rename mode instead
- **Tables:** Double-click caret (▶︎ or ▼) to toggle expanded/collapsed state
- **nil objects:** Dialog prompts for type selection before opening editor
- **Scalars:** No dedicated editor; double-click opens parent table with object selected
- Editor window title shows the dot-path to the object (e.g., `user.workspace.myWpText`)
- If editor window already open for that object, brings it to front (reuses window)
- `edit(adrObject)` verb opens the appropriate editor programmatically

### Create Object

- Press Enter to create a new object below the current selection
- Default name: `Item #1` (or next available number to ensure uniqueness)
- Default type: `nil`
- Type popup in lower-left selects the type for new objects

### Delete Object

- Delete key (or Edit > Delete) removes selected object(s)
- Confirmation dialog for non-empty tables

### Type Conversion

- Select object, choose new type from popup
- If coercible (e.g., number ↔ string, date ↔ number), converts in place
- If not coercible, behavior TBD (error? create new?)

---

## Selection

### Single Selection

- Click a row to select it
- Arrow keys move selection up/down
- Type-ahead: typing jumps to first item starting with those letters

### Multi-Selection

- Cmd+Click (Mac) / Ctrl+Click (Windows): toggle individual row selection
- Shift+Click: extend selection from anchor to clicked row
- Cmd+A / Ctrl+A: select all visible items

### Operations on Multi-Selection

- Delete: removes all selected objects
- Cut/Copy: places all selected objects on clipboard
- Drag: moves all selected objects

---

## Clipboard

- **Cut (Cmd+X):** Remove selected objects, place on clipboard
- **Copy (Cmd+C):** Copy selected objects to clipboard
- **Paste (Cmd+V):** Insert clipboard contents below selection (or into selected table)
- Cross-window paste supported: copy from one table window, paste into another

---

## Drag and Drop

### Within Window

- Drag row(s) to reorder within the same table
- Drag below and slightly right of a sub-table to move into that sub-table
- Drag left (less indented) to move out to parent table

### Between Windows (Future Enhancement)

- Drag from one table window to another to move/copy objects
- Hold modifier key (Option/Alt) to copy instead of move

---

## Reorganization

### Indent / Outdent

- **Tab:** Move selected object into the sibling table immediately above it
- **Shift+Tab:** Move selected object out to the parent table

### Move Up / Down / Left / Right

- **Cmd+Up:** Move selection up in sort order
- **Cmd+Down:** Move selection down in sort order
- **Cmd+Left:** Outdent (same as Shift+Tab)
- **Cmd+Right:** Indent (same as Tab)

---

## Sorting

- **Default:** Alphabetical by name (case-insensitive)
- **Click column header:** Sort by that column
- **Click again:** Reverse sort order
- Sort indicator (▲/▼) shown in active column header

---

## Find

- **Cmd+F:** Opens find dialog/bar
- Search scoped to current table and all descendants
- Results highlighted or filtered (TBD)
- Find Next / Find Previous navigation

---

## Context Menu

Right-click on row(s) to show context menu.

### Standard Items

- Cut, Copy, Paste
- Delete
- Rename
- Open (opens editor)
- New > [type submenu]
- Sort > Name / Value / Kind

### Customization

Context menu is customizable via UserTalk callbacks. The `user.callbacks.opRightClick` table contains scripts for handling right-click in various contexts. The Tools framework (`system.verbs.Frontier.tools`) provides patterns for packaging custom behaviors.

For the protocol: server may need to dynamically generate menu items based on selection context.

---

## Saving

### Current Model

- Explicit save via File > Save or Cmd+S
- Window shows dirty indicator when unsaved changes exist
- Periodic auto-save via scheduler (historically every 10 minutes)

### Future Enhancement

- True auto-save as edits occur (requires thread-safe ODB operations)

---

## Performance: Large Tables

Tables may contain 100K+ items (e.g., discussion group message archives).

### Client-Side

- **Virtualized rendering:** Only render rows visible in viewport
- **Lazy expansion:** Don't fetch children until table is expanded
- **Progressive loading:** Fetch items in pages as user scrolls

### Server-Side (Protocol)

- **Paginated fetches:** Support `offset` and `limit` parameters
- **Total count:** Return total item count for scroll bar sizing
- **Sorted fetches:** Server applies sort to avoid client sorting 150K items

### Threshold Behavior

- Small tables (< 1,000 items): Fetch all at once
- Large tables (≥ 1,000 items): Paginated fetching

---

## Protocol Operations

The following JSON-RPC methods are needed to support the table browser:

### Read Operations

| Method | Description |
|--------|-------------|
| `odb.getObject` | Get single object metadata and value |
| `odb.getChildren` | Get children of a table (paginated, sorted) |
| `odb.getChildCount` | Get number of children (for large table handling) |

### Write Operations

| Method | Description |
|--------|-------------|
| `odb.create` | Create new object |
| `odb.setValue` | Update object value |
| `odb.rename` | Rename object |
| `odb.delete` | Delete object(s) |
| `odb.move` | Move object to new location |
| `odb.copy` | Copy object to new location |
| `odb.setType` | Convert object to new type |

### Events (Server → Client)

| Event | Description |
|-------|-------------|
| `odb.created` | Object created (by another client or script) |
| `odb.updated` | Object value changed |
| `odb.deleted` | Object deleted |
| `odb.moved` | Object moved or renamed |

### Context Menu

| Method | Description |
|--------|-------------|
| `menu.getContextMenu` | Get context menu items for selection |
| `menu.invokeContextItem` | Execute selected context menu item |

---

## UI State (Client-Owned)

The client maintains:

- Which tables are expanded
- Current selection
- Scroll position
- Sort column and direction
- Window position and size
- Find state (query, current match)

Server does not track this state. If connection drops and reconnects, client restores from its own state.

---

## Open Questions

1. **Type conversion failures:** What happens when converting between incompatible types?
2. **Undo:** How deep should undo history go? Server-side or client-side undo stack?
3. **Conflict handling:** If two clients edit the same object, how to resolve?
4. **Drag-drop feedback:** What visual feedback during drag operations?
5. **Find UI:** Inline find bar or separate dialog?

---

## Related Documents

- `planning/gui/ARCHITECTURE.md` — Overall GUI architecture
- `planning/gui/README.md` — Purpose of GUI planning area
- `system.verbs.Frontier.tools` — Tools framework for customization patterns
