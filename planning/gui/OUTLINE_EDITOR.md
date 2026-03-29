# Outline Editor Specification

| | |
|---|---|
| **Version** | 0.3.0 |
| **Status** | Draft |
| **Last Updated** | 2026-03-29 |

## Change History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 0.3.0 | 2026-03-29 | Jake Savin, Claude | Added Node Identity and Attribute Type Mapping subsections to Node Attributes |
| 0.2.0 | 2026-02-04 | Jake Savin, Claude | Corrected refcon/attributes architecture; added phasing; multi-line headlines; inline HTML notes |
| 0.1.0 | 2026-02-04 | Jake Savin, Claude | Initial draft |

---

## Overview

The outline editor is the primary interface for creating, viewing, and editing hierarchical outlines in Frontier. Outlines are versatile structures used for notes, task lists, documentation, and structured data.

Outlines are stored as external values in the ODB. The editor presents a tree-based view where structure is expressed through indentation, with features for hoisting (focusing on subtrees), multiple render modes, and rich linking.

**Key design decisions:**
- **Outline-based editing** - Same paradigm as script editor (indent/outdent, expand/collapse)
- **No execution** - Unlike scripts, outlines are data structures, not code
- **Per-node attributes** - Each node can have arbitrary attributes stored in its refcon
- **Attribute-driven rendering** - Checkboxes, headlines, and other visual treatments are per-node attributes
- **Hoisting** - Focus on a subtree, temporarily hiding the rest
- **Summit view** - Quick overview showing only top-level items

**Relationship to Script Editor:** The outline editor shares the same underlying outline paradigm as the script editor (expand/collapse, indentation controls, bar cursor, navigation). However, the outline editor has no execution or debugging features - it is for data and notes, not code. See [`SCRIPT_EDITOR.md`](./SCRIPT_EDITOR.md) for shared outline behaviors documented in detail.

---

## Visual Structure

### Window Layout

```
+-------------------------------------------------------------+
|  workspace.scratchpad.projectNotes                    _ [] X |
+-------------------------------------------------------------+
| [Hoist] [De-hoist] [Summit] [Default: Plain v]               |
+-------------------------------------------------------------+
|  1 | v Project Alpha                                         |
|  2 |    v Phase 1: Research                                  |
|  3 |       > Market analysis                                 |
|  4 |       > Competitor review                               |
|  5 | >>    o User interviews                                 |
|  6 |    > Phase 2: Design                                    |
|  7 |    > Phase 3: Implementation                            |
|  8 | v Documentation                                         |
|  9 |    o API reference                                      |
| 10 |    o User guide                                         |
+-------------------------------------------------------------+
| Default: Plain                                     Line 5    |
+-------------------------------------------------------------+
```

### Components

- **Title bar:** Shows the dot-path to the outline being edited (e.g., `workspace.scratchpad.projectNotes`)
- **Toolbar:** Hoist/De-hoist controls, Summit toggle, default render mode selector
- **Line numbers:** Gutter showing line numbers
- **Bar cursor:** Highlighted line (line 5 in example, shown with `>>`) indicating current selection
- **Outline area:** Scrollable, hierarchical editor
- **Status bar:** Current default mode and line position

### Visual Elements

| Element | Symbol | Meaning |
|---------|--------|---------|
| `v` | Expanded wedge | Item has children, currently expanded |
| `>` | Collapsed wedge | Item has children, currently collapsed |
| `o` | Leaf marker | Item has no children (or is a checkbox in checkbox mode) |
| `>>` | Bar cursor | Currently selected line |

### Wedge/Triangle Icons

The outline uses wedge icons to indicate structure:

| Icon | State | Description |
|------|-------|-------------|
| `v` (filled down) | Expanded | Click to collapse, children visible below |
| `>` (filled right) | Collapsed | Click to expand, children hidden |
| `o` (bullet/circle) | Leaf | No children, cannot expand |

---

## Core Outline Behavior

The outline editor shares fundamental behavior with the script editor. This section summarizes the shared behaviors; see [`SCRIPT_EDITOR.md`](./SCRIPT_EDITOR.md) for complete details.

### Expand / Collapse

- **Click wedge** (`>` or `v`) to toggle expanded/collapsed state
- **Double-click item** to toggle expanded/collapsed state
- **Keyboard:** Right arrow to expand, Left arrow to collapse (when on a collapsible item)
- **Cmd+[** / **Ctrl+[** to collapse current item
- **Cmd+]** / **Ctrl+]** to expand current item
- **Cmd+Shift+[** / **Ctrl+Shift+[** to collapse all
- **Cmd+Shift+]** / **Ctrl+Shift+]** to expand all

### Indentation Controls

| Action | Keys | Description |
|--------|------|-------------|
| Indent item | Tab | Move item into previous sibling (make it a child) |
| Outdent item | Shift+Tab | Move item out to parent level |
| New item | Enter | Create new item at same level |
| New child | Cmd+Enter | Create new item indented under current |

### Bar Cursor

The bar cursor is a full-line highlight showing the currently selected item:

```
  3 |       > Market analysis
  4 |       > Competitor review
>>5 |       o User interviews          <- Bar cursor: selected item
  6 |    > Phase 2: Design
```

- Moves with arrow keys (up/down)
- Click to select an item
- Selection is the target for edit operations

### Arrow Key Navigation

| Key | Action |
|-----|--------|
| Up | Move selection to previous visible item |
| Down | Move selection to next visible item |
| Left | Collapse current item (if expanded) or move to parent |
| Right | Expand current item (if collapsed) or move to first child |

### Drag and Drop Reordering

- Drag item(s) to reorder within the outline
- Drop below and slightly right to make child of item above
- Drop at same level to insert as sibling
- Hold Option/Alt while dragging to copy instead of move

### Text Mode vs Outline Mode

| Mode | Behavior | Toggle |
|------|----------|--------|
| Outline mode (default) | Each line is an outline node; structure via indent/outdent | Cmd+T |
| Text mode | Free-form text editing within the selected item | Cmd+T |

In text mode, the editor behaves like a text field within the current node. Enter returns to outline mode and creates a new sibling.

### Multi-line Headlines

Node text can contain newlines, allowing multi-line content within a single outline item:

- Display auto-sizes to fit content (CSS `white-space: pre-wrap` or equivalent)
- Each node is still one logical item, but may span multiple display lines
- Useful for notes, descriptions, or multi-paragraph content within a single node

**Example:**
```
  1 | v Project Alpha
  2 |    o Task one
  3 |       This task involves multiple steps:
  4 |       - First, review the documentation
  5 |       - Then, implement the changes
  6 |    o Task two
```

Note: Lines 3-5 are a single node with embedded newlines, not three separate nodes.

### Inline HTML (Future)

Node text can contain simple HTML markup for inline formatting:
- Supported tags: `<b>`, `<i>`, `<u>`, `<a href="...">`, `<code>`
- Renderer interprets and displays styled text
- HTML is stored directly in node text, not as separate attributes
- Allows rich text without a complex RTF model

**Example:**
```
This task is <b>critical</b> - see <a href="https://example.com">details</a>
```

This is a future phase feature (see Phasing section).

---

## Hoisting

Hoisting lets you focus on a subtree by temporarily hiding everything else. The hoisted item becomes the temporary "root" of the view.

### How Hoisting Works

1. Select an item with children
2. Click **Hoist** (or press Cmd+H)
3. The selected item appears as the root; all other items are hidden
4. The title bar shows the hoisted path (e.g., `workspace.scratchpad.projectNotes > Phase 1: Research`)
5. Edit normally within the hoisted view

### Visual Indication

When hoisted, the window shows:
- A breadcrumb trail in the title bar or toolbar indicating hoist depth
- Only the hoisted subtree is visible
- De-hoist button becomes active

**Example - Before hoisting:**
```
  1 | v Project Alpha
  2 |    v Phase 1: Research      <- Selected
  3 |       > Market analysis
  4 |       > Competitor review
  5 |       o User interviews
  6 |    > Phase 2: Design
  7 |    > Phase 3: Implementation
```

**After hoisting "Phase 1: Research":**
```
+-------------------------------------------------------------+
|  workspace.scratchpad.projectNotes > Phase 1: Research       |
+-------------------------------------------------------------+
| [Hoist] [De-hoist] [Summit] [Default: Plain v]               |
+-------------------------------------------------------------+
|  1 | v Phase 1: Research                                     |
|  2 |    > Market analysis                                    |
|  3 |    > Competitor review                                  |
|  4 |    o User interviews                                    |
+-------------------------------------------------------------+
```

### Multi-Level Hoisting

You can hoist multiple levels deep:
1. Hoist "Phase 1: Research"
2. Then hoist "Market analysis" within that view
3. Title bar shows: `... > Phase 1: Research > Market analysis`

De-hoist returns one level at a time, or use "De-hoist All" to return to full view.

### Hoist Controls

| Action | Trigger | Description |
|--------|---------|-------------|
| Hoist | Cmd+H or toolbar button | Focus on selected item's subtree |
| De-hoist | Cmd+Shift+H or toolbar button | Return one level up |
| De-hoist All | Cmd+Option+H | Return to full outline view |

---

## Summit View

Summit view provides a quick structural overview by collapsing everything to show only top-level items.

### How Summit View Works

1. Click **Summit** toggle (or press Cmd+0)
2. All items collapse to show only the top level
3. Click Summit again to restore previous expand/collapse state

### Visual Example

**Normal view:**
```
  1 | v Project Alpha
  2 |    v Phase 1: Research
  3 |       > Market analysis
  4 |       > Competitor review
  5 |    > Phase 2: Design
  6 | v Documentation
  7 |    o API reference
```

**Summit view:**
```
  1 | > Project Alpha
  2 | > Documentation
```

### Summit + Hoist Interaction

- Summit view respects the current hoist level
- If hoisted into "Project Alpha", summit shows top-level items within that subtree
- De-hoisting while in summit maintains summit mode at the new level

---

## Node Attributes (Refcons)

Each outline node has a `refcon` field that can store any scalar value. By convention, the `op.attributes` verbs store a **packed table** in the refcon, enabling arbitrary per-node metadata.

### Architecture

```
Outline Node
  └── refcon (binary) ── unpack() ──> Table
                                        ├── "checkbox" = true/false
                                        ├── "time" = date
                                        ├── "author" = string
                                        └── ...any attributes
```

**Key points:**

1. `op.getRefCon()` / `op.setRefCon()` - Get/set the raw refcon value
2. `op.attributes.getAll()` / `op.attributes.setOne()` - Unpack/pack the attribute table
3. **Freeform schema** - Attributes are arbitrary key-value pairs, no predefined schema
4. **Renderer interprets known attributes** - The renderer looks for recognized attributes (e.g., `checkbox`) and applies visual treatment; unknown attributes pass through unchanged

### Node Identity

Node IDs in protocol responses are server-generated sequential identifiers (e.g., `"n1"`, `"n2"`, `"n3"`). They are assigned as the server walks the outline tree to build the response.

**Stability guarantees:**
- IDs are stable **within a single response** — a given node always has the same ID within that response's `items` tree
- IDs are **NOT guaranteed stable across separate `outline/get` calls** — the server re-generates them each time
- When sending updates back (`outline/update`, `outline/updateItem`), the client uses the IDs from the most recent `outline/get` response

**Future: durable node identity.** Node UUIDs are planned for v7.5. The `tyheadrecord` structure has a 16-byte reserved field that is currently zeroed; this will hold a UUID for each node, enabling stable identity across sessions, undo history, and collaborative editing.

### Attribute Type Mapping

The protocol maps UserTalk types to JSON types as follows:

| UserTalk Type | JSON Type | Notes |
|---|---|---|
| string | string | Direct mapping |
| long/int | number | Direct mapping |
| boolean | boolean | Direct mapping |
| date | string | ISO 8601 format (e.g., `"2026-02-04T12:00:00Z"`) |
| address | string | With `@` prefix (e.g., `"@workspace.foo"`) |
| binary | string | Base64-encoded with `"base64:"` prefix |
| nil | null | Direct mapping |

**Empty attributes:** Nodes without a refcon have `"attributes": {}` (empty object). The `attributes` field is never omitted from protocol responses.

**Round-trip fidelity:** Attributes sent back via `outline/update` are packed to binary refcon format using the reverse of these mappings. Unknown JSON types are rejected with an error.

### Built-in Attribute Types

| Attribute | Type | Effect |
|-----------|------|--------|
| `checkbox` | boolean | Renders checkbox UI; `true` = checked, `false` = unchecked |
| `headline` | boolean | Renders with headline styling (larger/bolder text) |
| `comment` | boolean | Renders as comment (gray, italic) |
| `time` | date | Timestamp metadata (for sorting, display) |
| `author` | string | Author metadata |

Additional attributes can be added freely and will be preserved, even if the renderer does not interpret them.

---

## Attribute Editor

The attribute editor provides direct access to a node's attributes via the existing table browser UI.

### Accessing the Attribute Editor

1. Right-click on any outline node
2. Select **"Edit Attributes..."** from the context menu
3. A table browser window opens showing the unpacked refcon as a table

### Editing Attributes

In the table browser view:
- **Create** new attributes (any key name)
- **Edit** existing attribute values
- **Delete** attributes
- **Rename** attribute keys

### Saving Changes

- Click **Save** to pack the table and write it back to the node's refcon
- Click **Cancel** to discard changes
- Changes are not applied until explicitly saved

### Empty Refcons

If a node has no refcon (or an empty one), the attribute editor shows an empty table. Adding attributes creates the refcon automatically.

---

## Render Modes

The default render mode controls baseline appearance, but **per-node attributes override the default**. Nodes with specific attributes (like `checkbox` or `headline`) render according to those attributes regardless of the global mode.

### Plain Text Mode (Default)

Standard outline display with bullet/wedge indicators. Nodes render as simple text unless they have attributes that specify otherwise.

```
  1 | v Project Alpha
  2 |    o Task one
  3 |    o Task two
```

### Headlines Mode

Applies headline styling based on nesting level. Nodes can also have the `headline` attribute set explicitly for headline treatment regardless of mode.

```
  1 | v PROJECT ALPHA                    <- Bold/larger (level 1)
  2 |    This is the main project for Q1
  3 |    o Task one                      <- Bold (level 2)
  4 |       Complete the initial setup
  5 |    o Task two                      <- Bold (level 2)
  6 |       Review with stakeholders
```

In headlines mode:
- Top-level items appear largest/boldest
- Each nesting level has slightly smaller/lighter styling
- Creates a document-like appearance
- Nodes with `headline: false` attribute override this and render as plain text

### Mode Selection

- Toolbar dropdown to select default mode
- Keyboard: Cmd+1 (Plain), Cmd+2 (Headlines)
- Mode sets the **default** rendering; per-node attributes override

---

## Checkboxes

Checkboxes are implemented as **per-node attributes**, not a global mode. This allows mixing checkbox items with non-checkbox items in the same outline.

### Checkbox Attribute

A node displays a checkbox when it has the `checkbox` attribute:
- `checkbox: true` - Renders with checked checkbox
- `checkbox: false` - Renders with unchecked checkbox
- No `checkbox` attribute - Renders as normal node (no checkbox)

### Visual Example

```
  1 | v Project Alpha                    <- No checkbox attribute
  2 |    [x] Task one                    <- checkbox: true
  3 |    [ ] Task two                    <- checkbox: false
  4 |       o Subtask 2.1                <- No checkbox attribute
  5 |       [x] Subtask 2.2              <- checkbox: true
```

### Checkbox Interactions

- **Click checkbox** to toggle checked/unchecked
- **Space key** toggles checkbox on selected item (if it has checkbox attribute)
- Completed items can show strikethrough or dimmed styling (configurable)

### Checkbox States

| State | Display | Attribute Value |
|-------|---------|-----------------|
| Unchecked | `[ ]` | `checkbox: false` |
| Checked | `[x]` | `checkbox: true` |
| No checkbox | (bullet) | No `checkbox` attribute |

### Adding Checkboxes to Nodes

- Right-click > **"Add Checkbox"** - Sets `checkbox: false` on the node
- Or use attribute editor to add `checkbox` attribute manually
- Keyboard: Cmd+3 on selected node toggles checkbox attribute presence

---

## Comments

Items can be marked as comments, making them visually distinct from regular content.

### Comment Styling

- Comment items appear in gray, italic text
- Comments are visible but clearly secondary
- Useful for notes, annotations, or temporarily de-emphasized content

**Example:**
```
  1 | v Project Alpha
  2 |    o Task one
  3 |    o // This needs review before proceeding    <- Comment (gray, italic)
  4 |    o Task two
```

### Comment Controls

| Action | Trigger | Description |
|--------|---------|-------------|
| Toggle comment | Cmd+/ | Mark/unmark selected item as comment |
| Show/hide comments | Cmd+Shift+/ | Toggle comment visibility |

When comments are hidden, they don't appear in the outline but are preserved in the data.

---

## Links

Outlines support both internal ODB links and external URLs.

### Internal ODB Links

Links to other locations in the ODB:

**Syntax:** `[[path.to.item]]`

**Example:**
```
  1 | v Project Alpha
  2 |    o See related: [[workspace.scratchpad.relatedNotes]]
  3 |    o API details: [[system.verbs.Frontier.api]]
```

**Behavior:**
- Link text displays as clickable (underlined, colored)
- Click opens the target in appropriate editor (table browser, script editor, etc.)
- Cmd+Click opens in new window
- Hover shows full path as tooltip
- Broken links (target doesn't exist) shown in different color with warning

### External URLs

Standard URLs are automatically detected and made clickable:

**Example:**
```
  1 | v Resources
  2 |    o Documentation: https://frontier.io/docs
  3 |    o GitHub: https://github.com/example/frontier
```

**Behavior:**
- URLs display as clickable links
- Click opens in default browser
- Mailto links open email client

### Link Insertion

| Action | Trigger | Description |
|--------|---------|-------------|
| Insert ODB link | Cmd+K | Opens path picker dialog |
| Insert URL | Type or paste | Auto-detected |
| Edit link | Cmd+Click on link | Opens edit dialog |

---

## Find

Search within the current outline.

### Find Bar

- **Cmd+F** opens inline find bar
- Type to search; results highlight in real-time
- Navigate results with **Cmd+G** (next) and **Cmd+Shift+G** (previous)

### Find Scope

- Searches all text in the outline (visible and collapsed)
- Respects current hoist level (searches within hoisted subtree only)
- Optionally include/exclude comments

### Find Options

| Option | Description |
|--------|-------------|
| Case sensitive | Match exact case |
| Whole word | Match complete words only |
| Regex | Use regular expressions |
| Include comments | Search comment items |

### Find and Replace

- **Cmd+H** opens find/replace
- Replace one or replace all
- Preview changes before applying

---

## Saving

### Explicit Save

- **Cmd+S** saves the outline to the ODB
- Window title shows dirty indicator (*) when unsaved changes exist
- Close window prompts to save if dirty

### Auto-Save (Future)

- Configurable auto-save interval
- Saves after idle period (e.g., 30 seconds of no edits)
- Visual indicator when auto-save occurs

### Save State

The following state is saved with the outline:
- All item content and structure
- Expand/collapse state for each item
- Per-node refcons (containing attributes like checkbox, headline, etc.)
- Current default render mode
- Hoist state (saved as "last hoist" for restoration)

---

## Protocol Operations

The following protocol operations support outline editing. These extend the base protocol defined in [`PROTOCOL.md`](./PROTOCOL.md).

### outline/get - Get Outline Content

Retrieves the full outline structure.

**WebSocket:**
```json
{
  "op": "outline/get",
  "id": 1,
  "params": {
    "path": "workspace.scratchpad.projectNotes"
  }
}
```

**Parameters:**

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `path` | string | Yes | Dot-path to outline object |
| `includeMetadata` | boolean | No | Include render mode, hoist state (default: true) |

**Response:**
```json
{
  "id": 1,
  "result": {
    "path": "workspace.scratchpad.projectNotes",
    "items": [
      {
        "id": "item_1",
        "text": "Project Alpha",
        "expanded": true,
        "attributes": {},
        "children": [
          {
            "id": "item_2",
            "text": "Phase 1: Research",
            "expanded": true,
            "attributes": {"checkbox": false},
            "children": [...]
          }
        ]
      }
    ],
    "metadata": {
      "defaultRenderMode": "plain",
      "hoistPath": null,
      "modified": "2026-02-04T12:00:00Z"
    }
  }
}
```

Note: The `attributes` field contains the unpacked refcon table for each node. Nodes without a refcon will have an empty `attributes` object.

### outline/update - Update Outline Content

Updates the outline structure. Supports partial updates.

**WebSocket:**
```json
{
  "op": "outline/update",
  "id": 2,
  "params": {
    "path": "workspace.scratchpad.projectNotes",
    "items": [...],
    "metadata": {
      "defaultRenderMode": "headlines"
    }
  }
}
```

**Parameters:**

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `path` | string | Yes | Dot-path to outline object |
| `items` | array | No | Full item tree (replaces entire outline) |
| `metadata` | object | No | Update metadata fields |

**Response:**
```json
{
  "id": 2,
  "result": {
    "path": "workspace.scratchpad.projectNotes",
    "modified": "2026-02-04T12:05:00Z"
  }
}
```

### outline/updateItem - Update Single Item

Updates a single item without sending the full outline. For attribute changes, use `outline/setNodeAttributes` instead.

**WebSocket:**
```json
{
  "op": "outline/updateItem",
  "id": 3,
  "params": {
    "path": "workspace.scratchpad.projectNotes",
    "itemId": "item_5",
    "text": "Updated task text",
    "expanded": true
  }
}
```

**Parameters:**

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `path` | string | Yes | Dot-path to outline |
| `itemId` | string | Yes | ID of item to update |
| `text` | string | No | New item text |
| `expanded` | boolean | No | New expanded state |

### outline/moveItem - Move/Reorder Item

Moves an item to a new position in the outline.

**WebSocket:**
```json
{
  "op": "outline/moveItem",
  "id": 4,
  "params": {
    "path": "workspace.scratchpad.projectNotes",
    "itemId": "item_5",
    "newParentId": "item_2",
    "position": 0
  }
}
```

**Parameters:**

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `path` | string | Yes | Dot-path to outline |
| `itemId` | string | Yes | ID of item to move |
| `newParentId` | string | No | ID of new parent (null for top level) |
| `position` | integer | Yes | Index within new parent's children |

### outline/setRenderMode - Change Default Render Mode

Changes the outline's default render mode. Note that per-node attributes override the default.

**WebSocket:**
```json
{
  "op": "outline/setRenderMode",
  "id": 5,
  "params": {
    "path": "workspace.scratchpad.projectNotes",
    "mode": "headlines"
  }
}
```

**Parameters:**

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `path` | string | Yes | Dot-path to outline |
| `mode` | string | Yes | `"plain"` or `"headlines"` |

### outline/getNodeAttributes - Get Node Attributes

Retrieves the unpacked attribute table for a specific node.

**WebSocket:**
```json
{
  "op": "outline/getNodeAttributes",
  "id": 6,
  "params": {
    "path": "workspace.scratchpad.projectNotes",
    "itemId": "item_5"
  }
}
```

**Parameters:**

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `path` | string | Yes | Dot-path to outline |
| `itemId` | string | Yes | ID of the node |

**Response:**
```json
{
  "id": 6,
  "result": {
    "itemId": "item_5",
    "attributes": {
      "checkbox": true,
      "time": "2026-02-04T10:30:00Z",
      "author": "alice"
    }
  }
}
```

If the node has no refcon or an empty one, `attributes` will be an empty object `{}`.

### outline/setNodeAttributes - Set Node Attributes

Updates the attributes for a specific node. The provided attributes are packed and stored in the node's refcon.

**WebSocket:**
```json
{
  "op": "outline/setNodeAttributes",
  "id": 7,
  "params": {
    "path": "workspace.scratchpad.projectNotes",
    "itemId": "item_5",
    "attributes": {
      "checkbox": false,
      "priority": "high"
    }
  }
}
```

**Parameters:**

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `path` | string | Yes | Dot-path to outline |
| `itemId` | string | Yes | ID of the node |
| `attributes` | object | Yes | Full attribute table (replaces existing) |

**Response:**
```json
{
  "id": 7,
  "result": {
    "itemId": "item_5",
    "modified": "2026-02-04T12:10:00Z"
  }
}
```

**Note:** This replaces the entire attribute table. To update a single attribute while preserving others, first call `getNodeAttributes`, modify the result, then call `setNodeAttributes`.

### Events (Server to Client)

#### outline/updated - Outline Changed

Notifies clients when an outline is modified (by another client or script).

```json
{
  "event": "outline/updated",
  "subscriptionId": "sub_xyz789",
  "data": {
    "path": "workspace.scratchpad.projectNotes",
    "changeType": "itemUpdated",
    "itemId": "item_5",
    "changedBy": "user:alice"
  }
}
```

**Change types:** `"full"`, `"itemUpdated"`, `"itemMoved"`, `"itemCreated"`, `"itemDeleted"`, `"metadataChanged"`

---

## Keyboard Shortcuts

### Navigation

| Action | macOS | Windows/Linux |
|--------|-------|---------------|
| Move up | Up Arrow | Up Arrow |
| Move down | Down Arrow | Down Arrow |
| Expand | Right Arrow | Right Arrow |
| Collapse | Left Arrow | Left Arrow |
| Go to first item | Cmd+Up | Ctrl+Home |
| Go to last item | Cmd+Down | Ctrl+End |

### Structure

| Action | macOS | Windows/Linux |
|--------|-------|---------------|
| New item | Enter | Enter |
| New child item | Cmd+Enter | Ctrl+Enter |
| Indent | Tab | Tab |
| Outdent | Shift+Tab | Shift+Tab |
| Move item up | Cmd+Shift+Up | Ctrl+Shift+Up |
| Move item down | Cmd+Shift+Down | Ctrl+Shift+Down |

### Expand/Collapse

| Action | macOS | Windows/Linux |
|--------|-------|---------------|
| Collapse item | Cmd+[ | Ctrl+[ |
| Expand item | Cmd+] | Ctrl+] |
| Collapse all | Cmd+Shift+[ | Ctrl+Shift+[ |
| Expand all | Cmd+Shift+] | Ctrl+Shift+] |
| Toggle text mode | Cmd+T | Ctrl+T |

### Hoisting

| Action | macOS | Windows/Linux |
|--------|-------|---------------|
| Hoist | Cmd+H | Ctrl+H |
| De-hoist | Cmd+Shift+H | Ctrl+Shift+H |
| De-hoist all | Cmd+Option+H | Ctrl+Alt+H |
| Summit view | Cmd+0 | Ctrl+0 |

### Render Modes & Checkboxes

| Action | macOS | Windows/Linux |
|--------|-------|---------------|
| Plain mode | Cmd+1 | Ctrl+1 |
| Headlines mode | Cmd+2 | Ctrl+2 |
| Toggle checkbox attribute | Cmd+3 | Ctrl+3 |
| Toggle checkbox (if present) | Space | Space |
| Edit attributes | (right-click menu) | (right-click menu) |

### Comments

| Action | macOS | Windows/Linux |
|--------|-------|---------------|
| Toggle comment | Cmd+/ | Ctrl+/ |
| Show/hide comments | Cmd+Shift+/ | Ctrl+Shift+/ |

### Editing

| Action | macOS | Windows/Linux |
|--------|-------|---------------|
| Save | Cmd+S | Ctrl+S |
| Undo | Cmd+Z | Ctrl+Z |
| Redo | Cmd+Shift+Z | Ctrl+Y |
| Find | Cmd+F | Ctrl+F |
| Find Next | Cmd+G | F3 |
| Find/Replace | Cmd+H | Ctrl+H |
| Insert link | Cmd+K | Ctrl+K |
| Delete item | Delete | Delete |
| Cut | Cmd+X | Ctrl+X |
| Copy | Cmd+C | Ctrl+C |
| Paste | Cmd+V | Ctrl+V |

---

## Phasing

Implementation will proceed in phases, with each phase building on the previous.

| Phase | Features |
|-------|----------|
| **MVP** | Core outline editing, expand/collapse, hoisting, summit view, plain text rendering |
| **Phase 2** | Refcon attribute editor (right-click > "Edit Attributes..." > table browser) |
| **Phase 3** | Built-in attribute renderers (checkbox, headline styling based on attributes) |
| **Phase 4** | Custom render scripts, inline HTML/markdown rendering |

### MVP Scope

The MVP delivers a fully functional outline editor with:
- Create, edit, delete, reorder nodes
- Hierarchical structure via indent/outdent
- Expand/collapse navigation
- Hoisting and summit view
- Basic find and find/replace
- ODB and external links
- Save/load to ODB

Attributes exist at the data layer (refcons are preserved) but no UI for editing them.

### Phase 2: Attribute Editor

Adds the attribute editor UI:
- Right-click context menu with "Edit Attributes..."
- Table browser window for viewing/editing packed refcon
- Create, edit, delete, rename attributes
- No visual rendering of attributes yet (just data access)

### Phase 3: Built-in Renderers

Adds visual interpretation of known attributes:
- `checkbox` attribute renders checkbox UI
- `headline` attribute renders with headline styling
- Comments via `comment` attribute
- Renderer checks attributes and applies appropriate visual treatment

### Phase 4: Custom Rendering

Advanced rendering capabilities:
- Inline HTML in node text (`<b>`, `<i>`, `<a>`, etc.)
- Markdown rendering option
- Custom render scripts for specialized attribute visualization
- Plugin architecture for render modes

---

## Open Questions

1. **Link syntax:** Is `[[path.to.item]]` the best syntax for ODB links? Consider alternatives like `@path.to.item` or automatic detection.

2. **Collaborative editing:** How to handle conflicts when multiple users edit the same outline? OT (Operational Transform) or CRDT-based approach?

3. **Export formats:** Should outlines be exportable to OPML, Markdown, HTML? What about importing? How should attributes be represented in exports?

4. **Attachments:** Can outline items have attachments (images, files)? Would these be attributes pointing to ODB paths?

5. **Outline templates:** Support for outline templates (e.g., meeting notes, project plans)? Templates could pre-populate attribute schemas.

6. **Drag-drop from external:** Can users drag files/text from outside the app into an outline?

7. **Undo granularity:** Should undo be per-keystroke, per-item, or per-structural-change? How does undo interact with attribute changes?

8. **Attribute inheritance:** Should child nodes inherit attributes from parents? (e.g., all children of a "checkbox: true" node get checkboxes)

9. **Attribute schemas:** Should outlines support optional attribute schemas that define expected attributes and their types? Useful for structured data entry.

10. **Bulk attribute operations:** UI for applying attributes to multiple selected nodes at once?

---

## Related Documents

- [`ARCHITECTURE.md`](./ARCHITECTURE.md) - Overall GUI architecture
- [`SCRIPT_EDITOR.md`](./SCRIPT_EDITOR.md) - Script editor specification (shares outline paradigm)
- [`TABLE_BROWSER.md`](./TABLE_BROWSER.md) - Table browser specification
- [`PROTOCOL.md`](./PROTOCOL.md) - JSON protocol specification
- `docs/usertalk/docserver/` - op* verbs for programmatic outline manipulation
