# Menu Editor Specification

| | |
|---|---|
| **Version** | 0.1.0 |
| **Status** | Draft |
| **Last Updated** | 2026-02-04 |

## Change History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 0.1.0 | 2026-02-04 | Jake Savin, Claude | Initial draft |

---

## Overview

The menu editor is a specialized editor for creating, viewing, and editing menu bar definitions and popup menus stored in the ODB. It combines the hierarchical structure of the outline editor with script attachment capabilities, making it a hybrid of the Outline Editor and Script Editor.

Menus are stored as outline externals in the ODB, where:
- The hierarchical structure defines menu/submenu organization
- Each menu item can have an attached script (stored in refcon)
- Items have attributes for keyboard shortcuts, enabled/disabled state, and checked state

**Key design decisions:**
- **Outline-based editing** - Same paradigm as outline editor (indent/outdent, expand/collapse)
- **Script attachment** - Each item can reference a script via refcon
- **Attribute-driven behavior** - Keyboard shortcuts, enabled state, and checked state are per-node attributes
- **Install/Test workflow** - Menus can be installed to the menu bar or tested as popups
- **Multi-column display** - Name, Script, and Cmd Key columns for at-a-glance editing

**Relationship to Outline Editor:** The menu editor shares the same underlying outline paradigm as the outline editor (expand/collapse, indentation controls, bar cursor, navigation). However, the menu editor adds script attachment, keyboard shortcut assignment, and menu-specific attributes. See [`OUTLINE_EDITOR.md`](./OUTLINE_EDITOR.md) for shared outline behaviors documented in detail.

---

## Visual Structure

### Window Layout

```
+-------------------------------------------------------------+
|  user.menus.myMenu                                    _ [] X |
+-------------------------------------------------------------+
| [Install] [Uninstall] [Test]                                 |
+-------------------------------------------------------------+
| Name                    | Script              | Cmd Key      |
+-------------------------------------------------------------+
|  1 | v File                                                   |
|  2 |    New              | @scripts.fileNew   | Cmd+N        |
|  3 |    Open...          | @scripts.fileOpen  | Cmd+O        |
|  4 |    --------         |                    |              |
|  5 |    Save             | @scripts.fileSave  | Cmd+S        |
|  6 | v Edit                                                   |
|  7 |    Cut              | @scripts.editCut   | Cmd+X        |
|  8 |    Copy             | @scripts.editCopy  | Cmd+C        |
|  9 | >> Paste            | @scripts.editPaste | Cmd+V        |
| 10 |    --------         |                    |              |
| 11 |    Select All       | @scripts.selectAll | Cmd+A        |
+-------------------------------------------------------------+
| Ready                                              Line 9     |
+-------------------------------------------------------------+
```

### Components

- **Title bar:** Shows the dot-path to the menu being edited (e.g., `user.menus.myMenu`)
- **Toolbar:** Install, Uninstall, and Test buttons
- **Column headers:** Name, Script, Cmd Key - clickable to sort
- **Line numbers:** Gutter showing line numbers
- **Bar cursor:** Highlighted line (line 9 in example, shown with `>>`) indicating current selection
- **Menu area:** Scrollable, hierarchical editor with columns
- **Status bar:** Current status and line position

### Visual Elements

| Element | Symbol | Meaning |
|---------|--------|---------|
| `v` | Expanded wedge | Submenu with children, currently expanded |
| `>` | Collapsed wedge | Submenu with children, currently collapsed |
| `o` | Leaf marker | Menu item with no children |
| `>>` | Bar cursor | Currently selected line |
| `--------` | Separator | Visual separator line in menu |

### Columns

| Column | Description | Editable |
|--------|-------------|----------|
| **Name** | Menu item display text | Yes (inline) |
| **Script** | Address of attached script or "(inline)" | Yes (click to edit) |
| **Cmd Key** | Keyboard shortcut assignment | Yes (capture keypress) |

---

## Core Outline Behavior

The menu editor shares fundamental behavior with the outline editor. This section summarizes the shared behaviors; see [`OUTLINE_EDITOR.md`](./OUTLINE_EDITOR.md) for complete details.

### Expand / Collapse

- **Click wedge** (`>` or `v`) to toggle expanded/collapsed state
- **Double-click item** to toggle expanded/collapsed state (if submenu) or edit script (if leaf)
- **Keyboard:** Right arrow to expand, Left arrow to collapse (when on a collapsible item)
- **Cmd+[** / **Ctrl+[** to collapse current item
- **Cmd+]** / **Ctrl+]** to expand current item
- **Cmd+Shift+[** / **Ctrl+Shift+[** to collapse all
- **Cmd+Shift+]** / **Ctrl+Shift+]** to expand all

### Indentation Controls

| Action | Keys | Description |
|--------|------|-------------|
| Indent item | Tab | Move item into previous sibling (make it a submenu item) |
| Outdent item | Shift+Tab | Move item out to parent level |
| New item | Enter | Create new item at same level |
| New child | Cmd+Enter | Create new item as child (submenu item) |

### Bar Cursor

The bar cursor is a full-line highlight showing the currently selected item:

```
  7 |    Cut              | @scripts.editCut   | Cmd+X
  8 |    Copy             | @scripts.editCopy  | Cmd+C
>>9 |    Paste            | @scripts.editPaste | Cmd+V    <- Bar cursor
 10 |    --------         |                    |
```

- Moves with arrow keys (up/down)
- Click to select an item
- Selection is the target for edit operations

### Drag and Drop Reordering

- Drag item(s) to reorder within the menu
- Drop below and slightly right to make child of item above (create submenu)
- Drop at same level to insert as sibling
- Hold Option/Alt while dragging to copy instead of move

---

## Menu Item Attributes

Each menu item stores its configuration in attributes (packed in the refcon). The menu editor interprets these attributes for display and behavior.

### Built-in Attributes

| Attribute | Type | Description |
|-----------|------|-------------|
| `script` | address/string | Script address (e.g., `@user.scripts.fileNew`) or inline script code |
| `cmdKey` | string | Keyboard shortcut (e.g., `"Cmd+N"`, `"Cmd+Shift+P"`) |
| `enabled` | boolean | Whether item is currently enabled (default: true) |
| `checked` | boolean | Whether item shows checkmark (default: false) |
| `separator` | boolean | If true, item renders as a separator line |

### Attribute Display

| Attribute | Display in Editor |
|-----------|-------------------|
| `script` (address) | Shows address in Script column (e.g., `@scripts.fileNew`) |
| `script` (inline) | Shows "(inline)" in Script column |
| `script` (none) | Script column empty |
| `cmdKey` | Shows shortcut in Cmd Key column |
| `enabled: false` | Item text appears grayed/dimmed |
| `checked: true` | Checkmark icon before item name |
| `separator: true` | Item displays as `--------` line |

---

## Script Attachment

### Script Reference Types

Menu items can have scripts attached in two ways:

1. **Address reference** - Points to a script elsewhere in the ODB
   - Stored as: `script: "@user.scripts.fileNew"`
   - Displayed as: `@scripts.fileNew` (relative path shown)
   - The script at that address runs when the item is selected

2. **Inline script** - Small script stored directly in the refcon
   - Stored as: `script: "dialog.alert(\"Hello\")"`
   - Displayed as: `(inline)` in the Script column
   - Useful for simple one-line actions

### Script Attachment Workflow

1. Select a menu item
2. Double-click the Script column (or press Enter in Script column)
3. Choose attachment method:
   - **Enter path** - Type a script address (e.g., `@user.scripts.myHandler`)
   - **Browse** - Open ODB browser to select a script
   - **Edit inline** - Open mini-editor for inline script
4. Script is saved to the item's refcon

### Opening Attached Script

- **Double-click item name** (if script is address reference) opens the script in the Script Editor
- **Cmd+Click script address** opens script in new window
- If script is inline, double-click opens inline editor dialog

### Script Execution

When a menu item is selected (by user clicking the menu):
1. Server looks up the item's `script` attribute
2. If address: Resolves the address and executes the script
3. If inline: Executes the inline code directly
4. If no script: Item does nothing (useful for disabled placeholders)

---

## Keyboard Shortcut Assignment

### Shortcut Format

Keyboard shortcuts are stored in the `cmdKey` attribute as strings:

| Format | Example | Display |
|--------|---------|---------|
| Cmd+key | `"Cmd+N"` | Cmd+N |
| Cmd+Shift+key | `"Cmd+Shift+P"` | Cmd+Shift+P |
| Cmd+Option+key | `"Cmd+Opt+S"` | Cmd+Opt+S |
| Cmd+Ctrl+key | `"Cmd+Ctrl+K"` | Cmd+Ctrl+K |

On Windows/Linux, `Cmd` maps to `Ctrl`.

### Assignment Workflow

1. Select a menu item
2. Click in the Cmd Key column (or press Cmd+K)
3. Press the desired key combination (e.g., Cmd+Shift+P)
4. The shortcut is recorded and displayed
5. Press Escape to cancel, Delete to clear

### Conflict Detection

When assigning a shortcut:
- Editor checks for conflicts within the same menu
- Warning shown if shortcut already assigned to another item
- User can choose to reassign (removes from previous item) or cancel

### Reserved Shortcuts

Some shortcuts are reserved by the system and cannot be assigned:
- Cmd+Q (Quit)
- Cmd+H (Hide)
- Cmd+Tab (App switching)

The editor warns when attempting to assign reserved shortcuts.

---

## Separators

Separators are special menu items that display as horizontal lines.

### Creating Separators

1. **Type pattern** - Enter `----` (four or more dashes) as item name
2. **Menu command** - Insert > Separator
3. **Keyboard** - Cmd+Shift+- (hyphen)

### Separator Behavior

- Separators display as horizontal lines in the rendered menu
- They cannot have scripts or keyboard shortcuts
- They cannot have children (not expandable)
- In the editor, they display as `--------` in the Name column

### Separator Attribute

Separators have `separator: true` in their attributes. The name text is not displayed in the rendered menu.

---

## Enabled/Disabled State

Menu items can be enabled or disabled.

### Static Enabled State

Set the `enabled` attribute directly:
- `enabled: true` - Item is clickable (default)
- `enabled: false` - Item is grayed out and non-clickable

### Dynamic Enabled State (Future)

A future enhancement will support dynamic enabled state via scripts:
- `enabledScript` attribute points to a script that returns true/false
- Script runs when menu is about to display
- Result determines whether item is enabled

### Visual Indication

Disabled items in the editor:
- Item text appears grayed/dimmed
- Still editable (can change name, script, etc.)
- Status bar shows "Disabled" when selected

---

## Checked State

Menu items can display a checkmark to indicate toggle state.

### Setting Checked State

The `checked` attribute controls the checkmark:
- `checked: true` - Checkmark appears before item name
- `checked: false` - No checkmark (default)

### Toggle Pattern

Common pattern for toggle menu items:
1. Script reads current state
2. Script toggles the state
3. Script updates the `checked` attribute

Example script:
```
on toggleDarkMode()
   user.prefs.darkMode = not user.prefs.darkMode
   menu.setItemChecked(@user.menus.viewMenu.darkMode, user.prefs.darkMode)
```

---

## Toolbar and Controls

### Toolbar Buttons

| Button | Icon | Action | Description |
|--------|------|--------|-------------|
| **Install** | Menu | Install menu | Adds this menu to the application menu bar |
| **Uninstall** | X-menu | Uninstall menu | Removes this menu from the menu bar |
| **Test** | Play | Test as popup | Shows menu as popup at cursor for testing |

### Install Workflow

1. Click **Install** button (or press Cmd+I)
2. Menu is compiled and validated
3. If valid, menu appears in the application menu bar
4. Title bar updates to show "(Installed)" indicator
5. Changes to the menu automatically update the installed version

### Uninstall Workflow

1. Click **Uninstall** button (or press Cmd+U)
2. Menu is removed from the application menu bar
3. Title bar "(Installed)" indicator is removed
4. The menu definition remains in the ODB (not deleted)

### Test Workflow

1. Click **Test** button (or press Cmd+T)
2. Menu appears as a popup at the current cursor position
3. User can click items to test scripts
4. Click outside the menu or press Escape to dismiss
5. Useful for testing without installing to the menu bar

---

## Menu Types

### Menu Bar Menus

Menu bar menus are installed into the application's main menu bar.

- Top-level items appear as menu titles in the bar
- Children appear as menu items in dropdown
- Grandchildren create submenus (hierarchical)

**Structure example:**
```
myMenuBar
  |-- File           <- Menu title in bar
  |     |-- New      <- Menu item
  |     |-- Open     <- Menu item
  |     |-- Recent   <- Submenu title
  |           |-- doc1.txt    <- Submenu item
  |           |-- doc2.txt    <- Submenu item
  |-- Edit           <- Menu title in bar
        |-- Cut      <- Menu item
        |-- Copy     <- Menu item
```

### Popup Menus

Popup menus are used programmatically via `menu.popupMenu()`.

- Same editor interface as menu bar menus
- Not installed to menu bar
- Invoked from scripts at runtime

**Example usage:**
```
local selection = menu.popupMenu(@user.menus.contextMenu, mouse.x, mouse.y)
```

### Converting Between Types

A menu definition can be used as either menu bar or popup:
- **As menu bar:** Use Install button or `menu.install()`
- **As popup:** Use `menu.popupMenu()` in scripts

The same menu can be installed as a menu bar item AND used as a popup.

---

## Protocol Operations

The following protocol operations support menu editing. These extend the base protocol defined in [`PROTOCOL.md`](./PROTOCOL.md).

### menu/get - Get Menu Structure

Retrieves the full menu structure.

**WebSocket:**
```json
{
  "op": "menu/get",
  "id": 1,
  "params": {
    "path": "user.menus.myMenu"
  }
}
```

**Parameters:**

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `path` | string | Yes | Dot-path to menu object |
| `includeMetadata` | boolean | No | Include install state (default: true) |

**Response:**
```json
{
  "id": 1,
  "result": {
    "path": "user.menus.myMenu",
    "items": [
      {
        "id": "item_1",
        "text": "File",
        "expanded": true,
        "attributes": {},
        "children": [
          {
            "id": "item_2",
            "text": "New",
            "expanded": false,
            "attributes": {
              "script": "@user.scripts.fileNew",
              "cmdKey": "Cmd+N"
            },
            "children": []
          },
          {
            "id": "item_3",
            "text": "----",
            "attributes": {"separator": true},
            "children": []
          }
        ]
      }
    ],
    "metadata": {
      "installed": true,
      "menuBarPosition": 2,
      "modified": "2026-02-04T12:00:00Z"
    }
  }
}
```

### menu/update - Update Menu Structure

Updates the menu structure. Supports partial updates.

**WebSocket:**
```json
{
  "op": "menu/update",
  "id": 2,
  "params": {
    "path": "user.menus.myMenu",
    "items": [...]
  }
}
```

**Parameters:**

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `path` | string | Yes | Dot-path to menu object |
| `items` | array | No | Full item tree (replaces entire menu) |

**Response:**
```json
{
  "id": 2,
  "result": {
    "path": "user.menus.myMenu",
    "modified": "2026-02-04T12:05:00Z"
  }
}
```

### menu/updateItem - Update Single Item

Updates a single menu item without sending the full menu.

**WebSocket:**
```json
{
  "op": "menu/updateItem",
  "id": 3,
  "params": {
    "path": "user.menus.myMenu",
    "itemId": "item_2",
    "text": "New Document",
    "attributes": {
      "script": "@user.scripts.fileNew",
      "cmdKey": "Cmd+N",
      "enabled": true
    }
  }
}
```

**Parameters:**

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `path` | string | Yes | Dot-path to menu |
| `itemId` | string | Yes | ID of item to update |
| `text` | string | No | New item text |
| `attributes` | object | No | New attributes (replaces all attributes) |

### menu/setItemScript - Set Script for Item

Sets the script attachment for a menu item.

**WebSocket:**
```json
{
  "op": "menu/setItemScript",
  "id": 4,
  "params": {
    "path": "user.menus.myMenu",
    "itemId": "item_2",
    "script": "@user.scripts.fileNew"
  }
}
```

**Parameters:**

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `path` | string | Yes | Dot-path to menu |
| `itemId` | string | Yes | ID of item |
| `script` | string | Yes | Script address or inline code (empty string to clear) |

### menu/setItemCmdKey - Set Keyboard Shortcut

Sets the keyboard shortcut for a menu item.

**WebSocket:**
```json
{
  "op": "menu/setItemCmdKey",
  "id": 5,
  "params": {
    "path": "user.menus.myMenu",
    "itemId": "item_2",
    "cmdKey": "Cmd+Shift+N"
  }
}
```

**Parameters:**

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `path` | string | Yes | Dot-path to menu |
| `itemId` | string | Yes | ID of item |
| `cmdKey` | string | Yes | Shortcut string (empty string to clear) |

**Response includes conflict info:**
```json
{
  "id": 5,
  "result": {
    "itemId": "item_2",
    "cmdKey": "Cmd+Shift+N",
    "conflict": {
      "itemId": "item_15",
      "itemText": "Save As...",
      "resolved": "reassigned"
    }
  }
}
```

### menu/install - Install Menu to Menu Bar

Installs the menu to the application menu bar.

**WebSocket:**
```json
{
  "op": "menu/install",
  "id": 6,
  "params": {
    "path": "user.menus.myMenu",
    "position": 2
  }
}
```

**Parameters:**

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `path` | string | Yes | Dot-path to menu |
| `position` | integer | No | Position in menu bar (default: append) |

**Response:**
```json
{
  "id": 6,
  "result": {
    "path": "user.menus.myMenu",
    "installed": true,
    "menuBarPosition": 2
  }
}
```

**Errors:**
- `3101` - Invalid menu structure
- `3102` - Script not found (referenced script doesn't exist)
- `3103` - Duplicate keyboard shortcut conflict

### menu/uninstall - Remove Menu from Menu Bar

Removes the menu from the application menu bar.

**WebSocket:**
```json
{
  "op": "menu/uninstall",
  "id": 7,
  "params": {
    "path": "user.menus.myMenu"
  }
}
```

**Response:**
```json
{
  "id": 7,
  "result": {
    "path": "user.menus.myMenu",
    "installed": false
  }
}
```

### menu/test - Show Menu as Popup for Testing

Shows the menu as a popup at the specified position.

**WebSocket:**
```json
{
  "op": "menu/test",
  "id": 8,
  "params": {
    "path": "user.menus.myMenu",
    "x": 100,
    "y": 200
  }
}
```

**Parameters:**

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `path` | string | Yes | Dot-path to menu |
| `x` | integer | No | X position for popup (default: cursor) |
| `y` | integer | No | Y position for popup (default: cursor) |

**Response:**
```json
{
  "id": 8,
  "result": {
    "path": "user.menus.myMenu",
    "displayed": true
  }
}
```

### Events (Server to Client)

#### menu/updated - Menu Changed

Notifies clients when a menu is modified (by another client or script).

```json
{
  "event": "menu/updated",
  "subscriptionId": "sub_xyz789",
  "data": {
    "path": "user.menus.myMenu",
    "changeType": "itemUpdated",
    "itemId": "item_5",
    "changedBy": "user:alice"
  }
}
```

**Change types:** `"full"`, `"itemUpdated"`, `"itemMoved"`, `"itemCreated"`, `"itemDeleted"`, `"installed"`, `"uninstalled"`

#### menu/itemSelected - Menu Item Selected

Notifies when a user selects a menu item (useful for testing/debugging).

```json
{
  "event": "menu/itemSelected",
  "data": {
    "path": "user.menus.myMenu",
    "itemId": "item_2",
    "itemText": "New",
    "script": "@user.scripts.fileNew"
  }
}
```

---

## Keyboard Shortcuts

### Navigation

| Action | macOS | Windows/Linux |
|--------|-------|---------------|
| Move up | Up Arrow | Up Arrow |
| Move down | Down Arrow | Down Arrow |
| Expand submenu | Right Arrow | Right Arrow |
| Collapse submenu | Left Arrow | Left Arrow |
| Go to first item | Cmd+Up | Ctrl+Home |
| Go to last item | Cmd+Down | Ctrl+End |

### Structure

| Action | macOS | Windows/Linux |
|--------|-------|---------------|
| New item | Enter | Enter |
| New child item | Cmd+Enter | Ctrl+Enter |
| Indent (make submenu) | Tab | Tab |
| Outdent | Shift+Tab | Shift+Tab |
| Move item up | Cmd+Shift+Up | Ctrl+Shift+Up |
| Move item down | Cmd+Shift+Down | Ctrl+Shift+Down |
| Insert separator | Cmd+Shift+- | Ctrl+Shift+- |

### Expand/Collapse

| Action | macOS | Windows/Linux |
|--------|-------|---------------|
| Collapse item | Cmd+[ | Ctrl+[ |
| Expand item | Cmd+] | Ctrl+] |
| Collapse all | Cmd+Shift+[ | Ctrl+Shift+[ |
| Expand all | Cmd+Shift+] | Ctrl+Shift+] |

### Editing

| Action | macOS | Windows/Linux |
|--------|-------|---------------|
| Edit item name | F2 or Enter | F2 or Enter |
| Edit script | Cmd+E | Ctrl+E |
| Set keyboard shortcut | Cmd+K | Ctrl+K |
| Clear keyboard shortcut | Delete (in Cmd Key column) | Delete |
| Toggle enabled | Cmd+/ | Ctrl+/ |
| Toggle checked | Cmd+Shift+/ | Ctrl+Shift+/ |

### Menu Operations

| Action | macOS | Windows/Linux |
|--------|-------|---------------|
| Install menu | Cmd+I | Ctrl+I |
| Uninstall menu | Cmd+U | Ctrl+U |
| Test menu | Cmd+T | Ctrl+T |

### Standard

| Action | macOS | Windows/Linux |
|--------|-------|---------------|
| Save | Cmd+S | Ctrl+S |
| Undo | Cmd+Z | Ctrl+Z |
| Redo | Cmd+Shift+Z | Ctrl+Y |
| Find | Cmd+F | Ctrl+F |
| Delete item | Delete | Delete |
| Cut | Cmd+X | Ctrl+X |
| Copy | Cmd+C | Ctrl+C |
| Paste | Cmd+V | Ctrl+V |

---

## Phasing

Implementation will proceed in phases, with each phase building on the previous.

| Phase | Features |
|-------|----------|
| **MVP** | Core menu editing, expand/collapse, columns display, script attachment (address only), separator support |
| **Phase 2** | Keyboard shortcut assignment with conflict detection, Install/Uninstall, basic Test popup |
| **Phase 3** | Inline script editing, enabled/checked state, dynamic state scripts |
| **Phase 4** | Advanced features: menu templates, menu bar position control, live preview |

### MVP Scope

The MVP delivers a functional menu editor with:
- Create, edit, delete, reorder menu items
- Hierarchical structure via indent/outdent
- Expand/collapse for submenus
- Three-column display (Name, Script, Cmd Key)
- Script attachment via address reference
- Separator support
- Save/load to ODB

Not included in MVP: Install/Uninstall, Test popup, keyboard shortcut assignment, inline scripts.

### Phase 2: Install and Shortcuts

Adds menu deployment features:
- Install/Uninstall toolbar buttons
- Test popup functionality
- Keyboard shortcut assignment in Cmd Key column
- Shortcut conflict detection and resolution

### Phase 3: States and Inline Scripts

Adds runtime behavior features:
- Enabled/disabled state attribute
- Checked state attribute
- Inline script editing
- Visual indicators for disabled/checked items

### Phase 4: Advanced Features

Advanced editing capabilities:
- Menu templates (copy structure from existing menus)
- Menu bar position control (reorder menu bar)
- Live preview of menu appearance
- Dynamic enabled/checked scripts

---

## Open Questions

1. **Inline script editing UI:** Should inline scripts open in a dialog, a split pane, or a mini-editor embedded in the row?

2. **Shortcut format:** Should shortcuts be stored as strings (`"Cmd+N"`) or structured (`{cmd: true, key: "N"}`)?

3. **Menu bar vs popup:** Should there be a visible indicator distinguishing menus intended for menu bar vs popup use?

4. **Icon support:** Should menu items support icons? How would icons be specified and stored?

5. **Accelerator keys:** Should menu items support underlined accelerator characters (e.g., "**F**ile" accessed via Alt+F)?

6. **Localization:** How should menu items support multiple languages? Separate attributes or separate menu definitions?

7. **Submenu depth:** Is there a limit on submenu nesting depth? Should the editor warn about deep nesting?

8. **Script validation:** Should the editor validate that script addresses exist when saving?

9. **Undo granularity:** Should undo be per-keystroke, per-field, or per-item?

10. **Collaborative editing:** How to handle conflicts when multiple users edit the same menu?

---

## Related Documents

- [`ARCHITECTURE.md`](./ARCHITECTURE.md) - Overall GUI architecture
- [`OUTLINE_EDITOR.md`](./OUTLINE_EDITOR.md) - Outline editor specification (shared outline paradigm)
- [`SCRIPT_EDITOR.md`](./SCRIPT_EDITOR.md) - Script editor specification (for editing attached scripts)
- [`TABLE_BROWSER.md`](./TABLE_BROWSER.md) - Table browser specification (for attribute editing)
- [`PROTOCOL.md`](./PROTOCOL.md) - JSON protocol specification
- `docs/usertalk/docserver/` - menu* verbs for programmatic menu manipulation
