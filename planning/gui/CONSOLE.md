# Unified Console Specification

| | |
|---|---|
| **Version** | 0.1.0 |
| **Status** | Draft |
| **Last Updated** | 2026-02-04 |

## Change History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 0.1.0 | 2026-02-04 | Jake Savin, Claude | Initial draft, extracted from SCRIPT_EDITOR.md |

---

## Overview

The Unified Console combines REPL (Read-Eval-Print Loop) and QuickScript functionality into a single, powerful interactive environment for executing UserTalk expressions and scripts.

### Design Philosophy: One Great Console

Rather than maintaining two separate tools (a REPL and a QuickScript editor), Frontier provides **one unified console** that does both workflows well:

- **REPL mode** (default): Persistent session state, history navigation, conversational exploration
- **One-shot mode**: QuickScript-style execute-and-clear for testing snippets without accumulating state

The console is a **dockable component** accessible from anywhere in the GUI, not just the script editor. This makes it the primary tool for quick experimentation, debugging, and interactive development.

---

## Visual Structure

### Docked Console (Default)

```
+-------------------------------------------------------------+
|  Main Application Window                                     |
+-------------------------------------------------------------+
|                                                              |
|  [Table Browser / Script Editor / Other Content]             |
|                                                              |
|                                                              |
|                                                              |
+-------------------------------------------------------------+
|  Console                                          [_] [^] [x] |
+-------------------------------------------------------------+
| > 2 + 2                                                      |
| 4                                                            |
| > string.upper("hello")                                      |
| "HELLO"                                                      |
| > local (x = 10)                                             |
| > x * 2                                                      |
| 20                                                           |
| > _                                                          |
+-------------------------------------------------------------+
| [Clear] [History]                         [REPL | One-shot]  |
+-------------------------------------------------------------+
```

### Floating Window (Alternative)

```
+-------------------------------------------------------------+
|  Console                                              _ [] X |
+-------------------------------------------------------------+
| > file.readWholeFile("/path/to/file.txt")                    |
| "Contents of the file..."                                    |
| > string.length(scratchpad.result)                           |
| 1247                                                         |
| > _                                                          |
+-------------------------------------------------------------+
| [Clear] [History]                         [REPL | One-shot]  |
+-------------------------------------------------------------+
```

### Components

- **Title bar:** "Console" (docked) or full title bar with window controls (floating)
- **Output area:** Scrollable history of inputs and outputs
- **Input line:** Current input with syntax highlighting (marked with `>` prompt)
- **Toolbar:** Clear, History buttons; mode toggle
- **Dock controls:** Collapse (`_`), expand (`^`), close/detach (`x`)

---

## Modes

### REPL Mode (Default)

REPL mode maintains persistent state between expressions, making it ideal for exploratory programming and debugging.

**Characteristics:**
- Variables declared with `local` persist across lines
- Full history available via up/down arrows
- Multi-line input supported (Shift+Enter for continuation)
- Enter executes the current expression
- Session state persists until explicitly cleared or console closed

**Example session:**
```
> local (data = {"apple", "banana", "cherry"})
> sizeOf(data)
3
> data[2]
"banana"
> for item in data { msg(item) }
apple
banana
cherry
```

### One-Shot Mode

One-shot mode provides QuickScript-style behavior: execute code and clear state immediately after.

**Characteristics:**
- State cleared after each execution
- Each execution is independent
- Useful for testing code snippets that shouldn't affect subsequent tests
- Multi-line editor (like a mini script editor)
- Cmd+Enter (or Run button) to execute

**When to use:**
- Testing snippets that modify global state
- Running utility scripts that should have no side effects
- Quick calculations without polluting session state

### Mode Toggle

- **Visual toggle:** Click `[REPL | One-shot]` segmented control in toolbar
- **Keyboard:** Cmd+Shift+Q toggles between modes
- Current mode visually highlighted

### Quick One-Shot Execution

Even while in REPL mode, you can execute a single expression without persisting state:
- **Cmd+Shift+Enter:** Execute current input as one-shot (state not persisted)

This allows quick "test this snippet" without switching modes or losing REPL session state.

---

## Input Handling

### Single-Line Input

```
> 2 + 2
4
```

- Type expression at prompt
- Enter to execute
- Result displayed on next line

### Multi-Line Input

```
> for i = 1 to 5
>    msg("Line " + i)
>
Line 1
Line 2
Line 3
Line 4
Line 5
```

**Multi-line controls:**
- **Shift+Enter:** Add new line (continue input)
- **Enter on empty line:** Execute accumulated input
- **Enter after complete statement:** Execute immediately

The console uses syntax-aware detection to determine when input is complete (e.g., unclosed `for` loop, open parenthesis).

### History Navigation

| Action | Keys | Description |
|--------|------|-------------|
| Previous | Up Arrow | Navigate to previous input |
| Next | Down Arrow | Navigate to next input |
| First | Cmd+Up | Jump to oldest history item |
| Last | Cmd+Down | Jump to newest history item |
| Search | Cmd+R | Incremental search through history |

**History behavior:**
- Navigating history replaces current input line
- Editing a history item creates a new entry (doesn't modify history)
- History persists across sessions (stored in user preferences)

### Syntax Highlighting

Input line displays syntax highlighting as you type:

| Element | Color (example) |
|---------|-----------------|
| Keywords | Blue |
| Strings | Green |
| Numbers | Orange |
| Comments | Gray (dimmed) |
| Identifiers | Default text |
| Operators | Default text |
| Built-in verbs | Purple |
| Errors | Red underline |

---

## Output Display

### Return Values

Successful evaluations display the return value:

```
> 2 + 2
4
> string.upper("hello")
"HELLO"
> {"a", "b", "c"}
{"a", "b", "c"}
```

**Value formatting:**
- Strings: Displayed with quotes (`"hello"`)
- Numbers: Displayed as-is (`42`, `3.14159`)
- Booleans: `true` or `false`
- Tables: Summary or inline, depending on size (`{3 items}` or `{"a", "b", "c"}`)
- nil: Displayed as `nil`
- Addresses: Displayed as dot-path (`@workspace.scratchpad.myTable`)

### Print / Msg Output

Output from `msg()` and similar verbs appears inline:

```
> msg("Starting process...")
Starting process...
> for i = 1 to 3 { msg("Step " + i) }
Step 1
Step 2
Step 3
```

### Error Output

Errors displayed with distinct styling:

```
> 1 / 0
Error: Can't divide by zero

> undefinedVariable + 1
Error: Can't get the value of "undefinedVariable" because there is no table with that name.
```

**Error styling:**
- Red text or red background
- Error prefix clearly visible
- Line/column information when available

### Timestamps (Optional)

Timestamps can be enabled for output lines:

```
[14:32:05] > long.query("SELECT * FROM users")
[14:32:07] {247 items}
```

- Toggle via console preferences
- Useful for tracking timing of long-running operations
- Off by default

---

## Docking Behavior

### Bottom Dock (Default)

The console docks at the bottom of the main application window.

```
+-------------------------------------------+
|          Main Content Area                |
|                                           |
+-------------------------------------------+
|  Console (docked, collapsed)         [^]  |
+-------------------------------------------+
```

```
+-------------------------------------------+
|          Main Content Area                |
|                                           |
+-------------------------------------------+
|  Console (docked, expanded)          [_]  |
| > _                                       |
| [Clear] [History]          [REPL | One-shot] |
+-------------------------------------------+
```

### Collapse / Expand

- **Collapse button (`_`):** Minimizes console to single bar
- **Expand button (`^`):** Restores console to previous height
- **Keyboard:** Cmd+J toggles collapsed/expanded
- **Double-click title bar:** Toggle collapsed/expanded

### Resize

- Drag the top edge of the docked console to resize
- Minimum height: ~3 lines visible
- Maximum height: ~50% of window
- Height preference persists across sessions

### Detach to Floating Window (Future)

- Click detach button or drag console away from dock
- Opens as independent floating window
- Can be re-docked by dragging back to dock zone
- Useful for multi-monitor setups

---

## Protocol Operations

### script/eval - Evaluate Expression

Primary operation for console input.

**WebSocket Request:**
```json
{
  "op": "script/eval",
  "id": 15,
  "params": {
    "expression": "2 + 2",
    "contextId": "repl_session_123"
  }
}
```

**Parameters:**

| Field | Type | Required | Default | Description |
|-------|------|----------|---------|-------------|
| `expression` | string | Yes | - | UserTalk expression to evaluate |
| `contextId` | string | No | - | REPL context ID (for persistent state) |
| `oneShot` | boolean | No | false | If true, don't persist state |

**Response (success):**
```json
{
  "id": 15,
  "result": {
    "value": 4,
    "type": "long",
    "display": "4"
  }
}
```

**Response (error):**
```json
{
  "id": 15,
  "error": {
    "code": 5002,
    "message": "Runtime error",
    "category": "script",
    "details": {
      "message": "Can't divide by zero",
      "line": 1,
      "column": 3
    }
  }
}
```

### script/eval/createContext - Create REPL Context

Creates a new REPL session with persistent state.

**WebSocket Request:**
```json
{
  "op": "script/eval/createContext",
  "id": 16,
  "params": {}
}
```

**Response:**
```json
{
  "id": 16,
  "result": {
    "contextId": "repl_session_456"
  }
}
```

### script/eval/clearContext - Clear REPL Context

Clears all state from a REPL session.

**WebSocket Request:**
```json
{
  "op": "script/eval/clearContext",
  "id": 17,
  "params": {
    "contextId": "repl_session_456"
  }
}
```

**Response:**
```json
{
  "id": 17,
  "result": {
    "contextId": "repl_session_456",
    "cleared": true
  }
}
```

### Events (Server to Client)

#### script/output - Console Output

Streamed output from `msg()` and similar verbs.

```json
{
  "event": "script/output",
  "data": {
    "contextId": "repl_session_123",
    "text": "Processing item 1...\n",
    "stream": "stdout"
  }
}
```

#### script/eval/completed - Evaluation Completed

Sent when async evaluation completes.

```json
{
  "event": "script/eval/completed",
  "data": {
    "contextId": "repl_session_123",
    "requestId": 15,
    "status": "success",
    "result": {"type": "long", "value": 42}
  }
}
```

---

## Keyboard Shortcuts

### Console Focus

| Action | macOS | Windows/Linux |
|--------|-------|---------------|
| Show/Hide Console | Cmd+J | Ctrl+J |
| Focus Console Input | Cmd+J (when hidden) | Ctrl+J (when hidden) |

### Input

| Action | macOS | Windows/Linux |
|--------|-------|---------------|
| Execute | Enter | Enter |
| Execute One-shot | Cmd+Shift+Enter | Ctrl+Shift+Enter |
| New Line (multi-line) | Shift+Enter | Shift+Enter |
| Clear Input | Escape | Escape |

### History

| Action | macOS | Windows/Linux |
|--------|-------|---------------|
| Previous | Up Arrow | Up Arrow |
| Next | Down Arrow | Down Arrow |
| First | Cmd+Up | Ctrl+Up |
| Last | Cmd+Down | Ctrl+Down |
| Search | Cmd+R | Ctrl+R |

### Output

| Action | macOS | Windows/Linux |
|--------|-------|---------------|
| Clear Output | Cmd+K | Ctrl+K |
| Scroll Up | Page Up | Page Up |
| Scroll Down | Page Down | Page Down |
| Scroll to Top | Cmd+Home | Ctrl+Home |
| Scroll to Bottom | Cmd+End | Ctrl+End |

### Mode

| Action | macOS | Windows/Linux |
|--------|-------|---------------|
| Toggle REPL/One-shot | Cmd+Shift+Q | Ctrl+Shift+Q |

### Integration

| Action | macOS | Windows/Linux |
|--------|-------|---------------|
| Execute Selection in Console | Cmd+Shift+E | Ctrl+Shift+E |

---

## Integration with Script Editor

### Execute Selection

From any editor window:
1. Select code
2. Cmd+Shift+E sends selection to console
3. Console executes and displays result

This enables rapid testing of code snippets without running the full script.

### Debugging Integration

When debugging a script:
- Console can evaluate expressions in the debug context
- Access to current frame's local variables
- Useful for inspecting state during breakpoints

```
[Debug] > x
10
[Debug] > x * localVar
200
```

The `[Debug]` prefix indicates evaluation is happening in debug context, not global REPL context.

---

## Open Questions

1. **History storage:** How much history to retain? Limit by count (1000 items) or age (30 days)?
2. **History search:** Full-text search or prefix-only?
3. **Context persistence:** Should REPL context survive app restart? (Probably not for v1)
4. **Multi-line editing:** Should multi-line input be a mini-editor with full editing, or just line accumulation?
5. **Output limits:** How to handle expressions that produce massive output? Truncate? Virtualize?
6. **Console theming:** Separate color scheme from editor, or unified?
7. **Multiple consoles:** Support multiple independent REPL sessions? (Probably v2)
8. **Tab completion:** Support for verb/path completion in input? Scope for v1?
9. **Detach behavior:** When detached to floating window, what happens to dock area?

---

## Related Documents

- [`ARCHITECTURE.md`](./ARCHITECTURE.md) - Overall GUI architecture
- [`SCRIPT_EDITOR.md`](./SCRIPT_EDITOR.md) - Script editor specification (references console)
- [`PROTOCOL.md`](./PROTOCOL.md) - JSON protocol specification
- `docs/usertalk/SYNTAX.md` - UserTalk syntax reference
