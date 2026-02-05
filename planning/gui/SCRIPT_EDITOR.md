# Script Editor Specification

| | |
|---|---|
| **Version** | 0.2.0 |
| **Status** | Draft |
| **Last Updated** | 2026-02-04 |

## Change History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 0.2.0 | 2026-02-04 | Jake Savin, Claude | Added Autocomplete/Suggestions section; simplified Unified Console to reference CONSOLE.md |
| 0.1.0 | 2026-02-04 | Jake Savin, Claude | Initial draft |

---

## Overview

The script editor is the primary interface for writing, debugging, and executing UserTalk scripts. It presents an outline-based code editor where structure is expressed through indentation rather than explicit braces and semicolons.

Scripts are stored as outline externals in the ODB. The editor compiles scripts to parse trees for execution on the server (frontier-cli).

**Key design decisions:**
- **Outline-based editing** - Structure via indentation (like Python), not braces/semicolons
- **Full debugging support** - Step, step-in, step-out, follow, breakpoints (phased implementation)
- **Server-side execution** - Scripts run on frontier-cli; GUI receives output/errors via WebSocket
- **Unified console** - Single console for both REPL and QuickScript workflows

---

## Visual Structure

### Window Layout

```
+-------------------------------------------------------------+
|  workspace.scratchpad.myScript                        _ [] X |
+-------------------------------------------------------------+
| [Run] [Debug] [Step] [In] [Out] [Follow] [Go] [Locals]      |
+-------------------------------------------------------------+
|  1 | on myScript()                                          |
|  2 |    local (x = 10, y = 20)                              |
|  3 | >> if x > 5                                            |
|  4 |       dialog.alert("x is big: " + x)                   |
|  5 |    else                                                |
|  6 |       dialog.alert("x is small")                       |
|  7 |    for i = 1 to y                                      |
|  8 |       msg(i)                                           |
|  9 |    return (x + y)                                      |
+-------------------------------------------------------------+
| Compiled successfully                              Line 3    |
+-------------------------------------------------------------+
```

### Components

- **Title bar:** Shows the dot-path to the script being edited (e.g., `workspace.scratchpad.myScript`)
- **Toolbar:** Debug controls (Run, Debug, Step, In, Out, Follow, Go, Locals)
- **Line numbers:** Gutter showing line numbers
- **Bar cursor:** Highlighted line (line 3 in example, shown with `>>`) indicating current debug position
- **Code area:** Scrollable, outline-based editor with syntax highlighting
- **Status bar:** Compilation status and current line/column position

### Visual States

| Element | State | Appearance |
|---------|-------|------------|
| Line | Normal | Default text color |
| Line | Bar cursor | Highlighted background (debug position) |
| Line | Breakpoint | Red dot in gutter |
| Line | Error | Red underline or highlight |
| Toolbar button | Active | Highlighted/pressed appearance |
| Toolbar button | Disabled | Grayed out |

---

## Outline Editor Behavior

### Structure as Indentation

UserTalk uses outline structure to represent code blocks. The editor displays and edits this structure through indentation:

```
on calculateSum(items)
   local (total = 0)
   for item in items
      total = total + item
   return (total)
```

The `for` loop body is a child of the `for` line in the outline. Indentation visualizes this parent-child relationship.

### Expand / Collapse

Code sections can be collapsed for a high-level view:

**Expanded:**
```
  1 | on processData()
  2 |    local (result = {})
  3 |    for item in data
  4 |       if item.valid
  5 |          result[item.name] = item.value
  6 |    return (result)
```

**Collapsed:**
```
  1 | on processData()                              [+]
  6 |    return (result)
```

- **Click [+]** to expand a collapsed section
- **Click [-]** to collapse an expanded section (shown when section is expanded)
- **Keyboard:** Cmd+[ to collapse, Cmd+] to expand
- **Collapse all:** Cmd+Shift+[ collapses all top-level blocks
- **Expand all:** Cmd+Shift+] expands everything

### Indentation Controls

| Action | Keys | Description |
|--------|------|-------------|
| Indent line | Tab | Move line into previous sibling (make it a child) |
| Outdent line | Shift+Tab | Move line out to parent level |
| New line | Enter | Create new line at same level |
| New child | Cmd+Enter | Create new line indented under current |

### Text Mode vs Outline Mode

The editor operates in **outline mode** by default, where:
- Each line is an outline node
- Structure is manipulated via indent/outdent
- Collapse/expand available

**Text mode** (toggle via Cmd+T) allows:
- Free-form text editing across lines
- Useful for paste operations
- Converts back to outline on save

---

## Syntax Display

### Hidden Syntax Elements

The editor automatically hides certain syntax elements that are implied by outline structure:

| Element | Storage | Display |
|---------|---------|---------|
| Opening brace `{` | Present in source | Hidden |
| Closing brace `}` | Present in source | Hidden |
| Semicolons `;` | Present in source | Hidden |
| Line comment prefix `//` | Present in source | Hidden (comment text shown dimmed) |

**Example - Stored form:**
```
if x > 5 {
   dialog.alert("big");
   }
```

**Example - Displayed form:**
```
if x > 5
   dialog.alert("big")
```

### Automatic Insertion

When saving or compiling, the editor automatically inserts:
- Opening braces after control statements
- Closing braces at the end of indented blocks
- Semicolons at the end of statements
- Comment prefixes for comment lines

### Syntax Highlighting

| Element | Color (example) |
|---------|-----------------|
| Keywords | Blue |
| Strings | Green |
| Numbers | Orange |
| Comments | Gray (dimmed) |
| Identifiers | Default text |
| Operators | Default text |
| Built-in verbs | Purple |

---

## Autocomplete / Code Suggestions

### Overview

The script editor provides intelligent code completion to improve productivity and discoverability. Suggestions appear contextually based on what the user is typing.

### Completion Types

| Type | Trigger | Example |
|------|---------|---------|
| **Verb completion** | Type object + dot | `string.` shows `lower`, `upper`, `mid`, `length`, etc. |
| **ODB path completion** | Type path + dot | `user.` shows items in user table |
| **Local variable completion** | Type partial name | `cou` suggests `count` (if defined locally) |
| **Keyword completion** | Type partial keyword | `ret` suggests `return` |
| **Parameter hints** | Open paren after verb | `dialog.alert(` shows signature |
| **Snippet completion** | Type snippet prefix | `for` expands to for loop template |

### UI Behavior

**Popup Activation:**
- Appears after typing a dot (verb/path completion)
- Appears after typing 2-3 characters (variable/keyword completion)
- Appears immediately for parameter hints after `(`

**Navigation:**
- Arrow keys navigate the suggestion list
- Tab or Enter accepts the selected suggestion
- Escape dismisses the popup without accepting
- Typing continues to filter the suggestion list

**Documentation Preview:**
- Optional panel showing documentation for the selected item
- Displays signature, parameter descriptions, and examples
- Can be toggled on/off via preferences

### Suggestion Kinds

| Kind | Description | Icon (suggested) |
|------|-------------|------------------|
| `verb` | Built-in or user-defined verb | Cube or function symbol |
| `path` | ODB path or table | Folder or database symbol |
| `variable` | Local or parameter variable | Variable symbol |
| `keyword` | Language keyword | Keyword symbol |
| `snippet` | Code template | Snippet symbol |

### Snippet Templates

Common snippets that expand to full code templates:

| Prefix | Expansion |
|--------|-----------|
| `for` | `for i = 1 to count` with cursor positioned |
| `forin` | `for item in collection` loop |
| `if` | `if condition` block |
| `ifelse` | `if/else` block |
| `on` | `on handlerName(params)` handler definition |
| `local` | `local (varname = value)` declaration |
| `try` | `try/else` error handling block |

### Performance Considerations

**Debouncing:**
- Completion requests are debounced (e.g., 100-200ms delay)
- Prevents excessive requests on rapid typing
- Cancels pending requests when new input arrives

**Caching:**
- Verb signatures are cached (they rarely change)
- ODB path completions may need incremental fetching for large tables
- Cache is invalidated when verbs are installed or ODB structure changes

**Incremental Fetching:**
- For large tables (100+ items), fetch in batches
- Show "Loading..." indicator while fetching
- Support scrolling to load more items

---

## Toolbar and Controls

### Toolbar Buttons

| Button | Icon | Action | Shortcut |
|--------|------|--------|----------|
| **Run** | Play | Execute script to completion | Cmd+R |
| **Debug** | Bug | Enter debug mode at first line | Cmd+D |
| **Step** | Step | Execute current line, move to next | F10 |
| **In** | Arrow down | Step into called script | F11 |
| **Out** | Arrow up | Step out to caller | Shift+F11 |
| **Follow** | Eye | Continuous highlight without stopping | Cmd+F11 |
| **Go** | Continue | Run to next breakpoint or completion | F5 |
| **Stop** | Pause | Pause execution (shown during running) | Cmd+. |
| **Kill** | X | Terminate immediately | Cmd+Shift+. |
| **Locals** | Table | Open variable inspector | Cmd+L |

### Button States

**Normal mode (not debugging):**
- Run, Debug: Enabled
- Step, In, Out, Follow, Go, Stop, Kill: Disabled
- Locals: Disabled

**Debug mode (paused at line):**
- Run: Disabled
- Debug: Disabled (already in debug mode)
- Step, In, Out, Follow, Go: Enabled
- Stop: Disabled (already stopped)
- Kill: Enabled
- Locals: Enabled

**Running (executing):**
- Run, Debug: Disabled
- Step, In, Out, Follow: Disabled
- Go: Changes to Stop (enabled)
- Kill: Enabled
- Locals: Enabled

---

## Debugging Workflow

### Implementation Phases

**Phase 1: Basic Execution**
- Run script to completion
- Display output in console
- Display errors with line navigation
- "Install" (compile) without running

**Phase 2: Breakpoints and Stepping**
- Set/clear breakpoints (click gutter or F9)
- Debug mode: pause at first line
- Step, Step In, Step Out
- Go (continue to next breakpoint)
- Stop (pause), Kill (terminate)

**Phase 3: Variable Inspection**
- Locals button opens table browser with thread state
- Call stack inspection (nested local tables)
- Follow mode (continuous highlight)
- Watch expressions

### Debug Session Lifecycle

```
1. User clicks Debug
   -> GUI sends script/debug/start
   -> Server compiles script, enters debug mode
   -> Server sends script/debug/paused event with line number

2. Bar cursor moves to indicated line
   -> Toolbar updates to debug state
   -> User can inspect locals, step, etc.

3. User clicks Step
   -> GUI sends script/debug/step
   -> Server executes one line
   -> Server sends script/debug/paused event with new line

4. User clicks Go
   -> GUI sends script/debug/continue
   -> Server runs until breakpoint or completion
   -> Server sends script/debug/paused or script/completed event

5. User clicks Kill (or script completes)
   -> GUI sends script/debug/kill (if user-initiated)
   -> Server terminates execution
   -> Server sends script/completed event
   -> Bar cursor clears, toolbar returns to normal state
```

### Breakpoints

- **Set breakpoint:** Click line number gutter, or F9
- **Clear breakpoint:** Click existing breakpoint, or F9 on breakpoint line
- **Clear all:** Cmd+Shift+F9
- **Visual:** Red dot in gutter
- **Conditional breakpoints:** Future enhancement (Phase 3+)

Breakpoints persist across editor sessions (stored with script metadata).

### Bar Cursor

The "bar cursor" is a full-line highlight showing the current execution position during debugging:

```
  1 | on myScript()
  2 |    local (x = 10)
>>3 |    dialog.alert(x)     <- Bar cursor: currently paused here
  4 |    return (x)
```

- Moves as execution progresses
- Visible during step, pause, and follow modes
- Clears when debug session ends

### Follow Mode

Follow mode continuously highlights execution without stopping:
- Execution runs at normal speed
- Bar cursor follows current line
- Useful for visualizing control flow
- Click Stop to pause and inspect

---

## Variable Inspection

### Locals Window

The **Locals** button opens a table browser showing the current thread's local variables:

```
+---------------------------------------------+
|  [Thread 1] Locals                    _ [] X |
+---------------------------------------------+
|  Name          | Value       | Kind         |
+---------------------------------------------+
|  x             | 10          | number       |
|  y             | 20          | number       |
|  result        | "hello"     | string       |
| > config       | {3 items}   | table        |
+---------------------------------------------+
```

### Call Stack Navigation

Each call level has its own local table. The Locals window can show the call stack:

```
+---------------------------------------------+
|  Call Stack                                  |
+---------------------------------------------+
|  > myScript() - line 5                       |
|    helperFunction() - line 12                |
|    processItem() - line 3                    |
+---------------------------------------------+
```

Clicking a stack frame:
- Shows that frame's locals in the table browser
- Moves bar cursor to that frame's current line (visual indicator only)

### Thread State Structure

The server exposes thread state as a virtual table:

```
thread.locals           -> Current frame locals
thread.stack[1].locals  -> Caller's locals
thread.stack[2].locals  -> Caller's caller's locals
thread.callstack        -> Array of {script, line, locals}
```

---

## Compilation and Error Handling

### Compile (Install)

Scripts must compile before execution. Compilation can be:
- **Automatic:** On run/debug, script compiles first
- **Manual:** Cmd+K or toolbar "Install" button (if present)

### Compilation Errors

When compilation fails:

1. Error message displayed in status bar
2. Error details shown in console or dialog
3. Bar cursor moves to error line
4. "Go To Error" button available if dialog is shown

**Error display example:**
```
+-------------------------------------------------------------+
| Compilation Error                                            |
+-------------------------------------------------------------+
| Line 5: Syntax error - unexpected token 'then'              |
|                                                              |
|         if x > 5 then                                        |
|                  ^^^^                                        |
|                                                              |
| [Go To] [OK]                                                 |
+-------------------------------------------------------------+
```

Clicking "Go To" places the cursor at line 5.

### Runtime Errors

When a script throws an error during execution:

1. Execution pauses (if in debug mode) or terminates (if run mode)
2. Error message displayed in console
3. Bar cursor moves to error line (if in debug mode)
4. Locals window available for inspection

---

## Console Integration

The script editor integrates with the unified console for REPL and QuickScript functionality.

**Full console specification:** See [`CONSOLE.md`](./CONSOLE.md)

**Key integration points:**
- **Cmd+J** toggles the docked console below the editor
- **Cmd+Shift+E** executes selected code in the console
- Console can be docked or floating
- Debug output streams to the console

---

## Protocol Operations

### Script Execution

#### script/run - Execute Script

Runs a script to completion, streaming output.

**WebSocket:**
```json
{
  "op": "script/run",
  "id": 1,
  "params": {
    "path": "workspace.scratchpad.myScript",
    "args": []
  }
}
```

**Parameters:**

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `path` | string | Yes | Dot-path to script |
| `args` | array | No | Arguments to pass to script |

**Response:**
```json
{
  "id": 1,
  "result": {
    "executionId": "exec_abc123",
    "status": "started"
  }
}
```

Output and completion are delivered via events (see below).

#### script/complete - Get Completions

Returns code completion suggestions for the current cursor position.

**WebSocket:**
```json
{
  "op": "script/complete",
  "id": 2,
  "params": {
    "path": "workspace.scratchpad.myScript",
    "line": 5,
    "column": 12,
    "prefix": "string."
  }
}
```

**Parameters:**

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `path` | string | Yes | Dot-path to script being edited |
| `line` | integer | Yes | Line number (1-indexed) |
| `column` | integer | Yes | Column position (1-indexed) |
| `prefix` | string | No | Text before cursor (for context) |

**Response:**
```json
{
  "id": 2,
  "result": {
    "suggestions": [
      {
        "label": "lower",
        "kind": "verb",
        "signature": "string.lower(s)",
        "doc": "Returns lowercase version of s"
      },
      {
        "label": "upper",
        "kind": "verb",
        "signature": "string.upper(s)",
        "doc": "Returns uppercase version of s"
      },
      {
        "label": "mid",
        "kind": "verb",
        "signature": "string.mid(s, start, count)",
        "doc": "Returns substring starting at position start"
      },
      {
        "label": "length",
        "kind": "verb",
        "signature": "string.length(s)",
        "doc": "Returns number of characters in s"
      }
    ]
  }
}
```

**Suggestion Object:**

| Field | Type | Description |
|-------|------|-------------|
| `label` | string | Text to insert/display |
| `kind` | string | One of: `verb`, `path`, `variable`, `keyword`, `snippet` |
| `signature` | string | Optional function signature |
| `doc` | string | Optional documentation text |
| `insertText` | string | Optional text to insert (defaults to `label`) |
| `detail` | string | Optional additional detail (e.g., type) |

#### script/compile - Compile Script

Compiles a script without executing.

**WebSocket:**
```json
{
  "op": "script/compile",
  "id": 2,
  "params": {
    "path": "workspace.scratchpad.myScript"
  }
}
```

**Response (success):**
```json
{
  "id": 2,
  "result": {
    "path": "workspace.scratchpad.myScript",
    "compiled": true,
    "lineCount": 12
  }
}
```

**Response (error):**
```json
{
  "id": 2,
  "error": {
    "code": 5001,
    "message": "Syntax error",
    "category": "script",
    "details": {
      "line": 5,
      "column": 18,
      "message": "Unexpected token 'then'",
      "source": "if x > 5 then"
    }
  }
}
```

### Debug Session Management

#### script/debug/start - Start Debug Session

Enters debug mode, pausing at the first line.

**WebSocket:**
```json
{
  "op": "script/debug/start",
  "id": 3,
  "params": {
    "path": "workspace.scratchpad.myScript",
    "args": [],
    "breakpoints": [5, 10, 15]
  }
}
```

**Parameters:**

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `path` | string | Yes | Dot-path to script |
| `args` | array | No | Arguments to pass |
| `breakpoints` | array | No | Initial breakpoint lines |

**Response:**
```json
{
  "id": 3,
  "result": {
    "sessionId": "debug_xyz789",
    "status": "paused",
    "line": 1
  }
}
```

#### script/debug/step - Step Execution

Executes one line and pauses.

**WebSocket:**
```json
{
  "op": "script/debug/step",
  "id": 4,
  "params": {
    "sessionId": "debug_xyz789"
  }
}
```

**Response:**
```json
{
  "id": 4,
  "result": {
    "status": "paused",
    "line": 2
  }
}
```

#### script/debug/stepIn - Step Into

Steps into a called script.

**WebSocket:**
```json
{
  "op": "script/debug/stepIn",
  "id": 5,
  "params": {
    "sessionId": "debug_xyz789"
  }
}
```

**Response:**
```json
{
  "id": 5,
  "result": {
    "status": "paused",
    "path": "workspace.helpers.utilityScript",
    "line": 1
  }
}
```

#### script/debug/stepOut - Step Out

Runs until returning to the caller.

**WebSocket:**
```json
{
  "op": "script/debug/stepOut",
  "id": 6,
  "params": {
    "sessionId": "debug_xyz789"
  }
}
```

#### script/debug/continue - Continue Execution

Runs until the next breakpoint or completion.

**WebSocket:**
```json
{
  "op": "script/debug/continue",
  "id": 7,
  "params": {
    "sessionId": "debug_xyz789"
  }
}
```

#### script/debug/stop - Pause Execution

Pauses a running script.

**WebSocket:**
```json
{
  "op": "script/debug/stop",
  "id": 8,
  "params": {
    "sessionId": "debug_xyz789"
  }
}
```

#### script/debug/kill - Terminate Execution

Terminates execution immediately.

**WebSocket:**
```json
{
  "op": "script/debug/kill",
  "id": 9,
  "params": {
    "sessionId": "debug_xyz789"
  }
}
```

### Breakpoint Management

#### script/breakpoints/set - Set Breakpoints

Sets breakpoints for a script.

**WebSocket:**
```json
{
  "op": "script/breakpoints/set",
  "id": 10,
  "params": {
    "path": "workspace.scratchpad.myScript",
    "lines": [5, 10, 15]
  }
}
```

**Response:**
```json
{
  "id": 10,
  "result": {
    "path": "workspace.scratchpad.myScript",
    "breakpoints": [5, 10, 15]
  }
}
```

#### script/breakpoints/clear - Clear Breakpoints

Clears specified or all breakpoints.

**WebSocket:**
```json
{
  "op": "script/breakpoints/clear",
  "id": 11,
  "params": {
    "path": "workspace.scratchpad.myScript",
    "lines": [5]
  }
}
```

To clear all:
```json
{
  "op": "script/breakpoints/clear",
  "id": 11,
  "params": {
    "path": "workspace.scratchpad.myScript",
    "all": true
  }
}
```

#### script/breakpoints/list - List Breakpoints

Lists breakpoints for a script or all scripts.

**WebSocket:**
```json
{
  "op": "script/breakpoints/list",
  "id": 12,
  "params": {
    "path": "workspace.scratchpad.myScript"
  }
}
```

**Response:**
```json
{
  "id": 12,
  "result": {
    "breakpoints": [
      {"path": "workspace.scratchpad.myScript", "line": 5},
      {"path": "workspace.scratchpad.myScript", "line": 10}
    ]
  }
}
```

### Variable Inspection

#### script/debug/locals - Get Local Variables

Gets the local variables for the current or specified stack frame.

**WebSocket:**
```json
{
  "op": "script/debug/locals",
  "id": 13,
  "params": {
    "sessionId": "debug_xyz789",
    "frameIndex": 0
  }
}
```

**Parameters:**

| Field | Type | Required | Default | Description |
|-------|------|----------|---------|-------------|
| `sessionId` | string | Yes | - | Debug session ID |
| `frameIndex` | integer | No | 0 | Stack frame (0 = current, 1 = caller, etc.) |

**Response:**
```json
{
  "id": 13,
  "result": {
    "frameIndex": 0,
    "script": "workspace.scratchpad.myScript",
    "line": 5,
    "locals": [
      {"name": "x", "type": "long", "value": 10},
      {"name": "y", "type": "long", "value": 20},
      {"name": "result", "type": "string", "value": "hello"}
    ]
  }
}
```

#### script/debug/callstack - Get Call Stack

Gets the full call stack.

**WebSocket:**
```json
{
  "op": "script/debug/callstack",
  "id": 14,
  "params": {
    "sessionId": "debug_xyz789"
  }
}
```

**Response:**
```json
{
  "id": 14,
  "result": {
    "frames": [
      {"index": 0, "script": "workspace.scratchpad.myScript", "line": 5},
      {"index": 1, "script": "workspace.helpers.caller", "line": 12},
      {"index": 2, "script": "workspace.main", "line": 3}
    ]
  }
}
```

### Console / REPL

#### script/eval - Evaluate Expression

Evaluates a UserTalk expression in the console context.

**WebSocket:**
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

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `expression` | string | Yes | UserTalk expression to evaluate |
| `contextId` | string | No | REPL context (for persistent state) |
| `oneShot` | boolean | No | If true, don't persist state (QuickScript mode) |

**Response:**
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

### Events (Server to Client)

#### script/output - Script Output

Streamed output from a running script.

```json
{
  "event": "script/output",
  "data": {
    "executionId": "exec_abc123",
    "text": "Processing item 1...\n",
    "stream": "stdout"
  }
}
```

#### script/completed - Script Completed

Script execution finished (success or error).

```json
{
  "event": "script/completed",
  "data": {
    "executionId": "exec_abc123",
    "status": "success",
    "result": {"type": "long", "value": 42},
    "duration": 1250
  }
}
```

**Error completion:**
```json
{
  "event": "script/completed",
  "data": {
    "executionId": "exec_abc123",
    "status": "error",
    "error": {
      "message": "Can't divide by zero",
      "line": 7,
      "script": "workspace.scratchpad.myScript"
    }
  }
}
```

#### script/debug/paused - Debug Paused

Execution paused at a line (breakpoint, step, or user stop).

```json
{
  "event": "script/debug/paused",
  "data": {
    "sessionId": "debug_xyz789",
    "reason": "breakpoint",
    "path": "workspace.scratchpad.myScript",
    "line": 5
  }
}
```

**Reason values:** `"breakpoint"`, `"step"`, `"stepIn"`, `"stepOut"`, `"stop"`, `"entry"` (first line)

---

## Keyboard Shortcuts

### Editing

| Action | macOS | Windows/Linux |
|--------|-------|---------------|
| Save | Cmd+S | Ctrl+S |
| Compile | Cmd+K | Ctrl+K |
| Undo | Cmd+Z | Ctrl+Z |
| Redo | Cmd+Shift+Z | Ctrl+Y |
| Find | Cmd+F | Ctrl+F |
| Find Next | Cmd+G | F3 |
| Replace | Cmd+H | Ctrl+H |
| Go to Line | Cmd+L | Ctrl+G |
| Toggle Comment | Cmd+/ | Ctrl+/ |
| Indent | Tab | Tab |
| Outdent | Shift+Tab | Shift+Tab |

### Outline

| Action | macOS | Windows/Linux |
|--------|-------|---------------|
| Collapse | Cmd+[ | Ctrl+[ |
| Expand | Cmd+] | Ctrl+] |
| Collapse All | Cmd+Shift+[ | Ctrl+Shift+[ |
| Expand All | Cmd+Shift+] | Ctrl+Shift+] |
| Toggle Text Mode | Cmd+T | Ctrl+T |
| New Line | Enter | Enter |
| New Child Line | Cmd+Enter | Ctrl+Enter |

### Execution and Debugging

| Action | macOS | Windows/Linux |
|--------|-------|---------------|
| Run | Cmd+R | Ctrl+R |
| Debug | Cmd+D | Ctrl+D |
| Step | F10 | F10 |
| Step In | F11 | F11 |
| Step Out | Shift+F11 | Shift+F11 |
| Continue | F5 | F5 |
| Stop | Cmd+. | Ctrl+Break |
| Kill | Cmd+Shift+. | Ctrl+Shift+Break |
| Follow | Cmd+F11 | Ctrl+F11 |
| Toggle Breakpoint | F9 | F9 |
| Clear All Breakpoints | Cmd+Shift+F9 | Ctrl+Shift+F9 |
| Locals Window | Cmd+L | Ctrl+L |

### Console

| Action | macOS | Windows/Linux |
|--------|-------|---------------|
| Show/Hide Console | Cmd+J | Ctrl+J |
| Execute Expression | Enter | Enter |
| Execute One-shot | Cmd+Shift+Enter | Ctrl+Shift+Enter |
| History Previous | Up Arrow | Up Arrow |
| History Next | Down Arrow | Down Arrow |
| History Search | Cmd+R | Ctrl+R |
| Clear Console | Cmd+K | Ctrl+K |
| Toggle REPL/QuickScript | Cmd+Shift+Q | Ctrl+Shift+Q |

---

## Open Questions

1. **Breakpoint persistence:** Store breakpoints with script metadata in ODB, or client-side only?
2. **Conditional breakpoints:** Syntax for "break when x > 10"? Phase 3+?
3. **Watch expressions:** Separate panel or integrated into Locals window?
4. **Multi-script debugging:** How to handle stepping into scripts in different databases?
5. **Remote debugging:** Can a GUI connect to a running script started by another client?
6. **Outline sync:** If script source is edited externally, how to preserve outline structure?
7. **Collaborative editing:** If two users edit the same script, how to handle conflicts?
8. **Console history:** How much history to retain? Across sessions? Searchable?
9. **Syntax theme:** User-customizable colors? Dark mode?
10. **Large scripts:** Performance for scripts with 1000+ lines? Virtualized rendering needed?

---

## Related Documents

- [`ARCHITECTURE.md`](./ARCHITECTURE.md) - Overall GUI architecture
- [`CONSOLE.md`](./CONSOLE.md) - Unified console specification (REPL and QuickScript)
- [`TABLE_BROWSER.md`](./TABLE_BROWSER.md) - Table browser specification (used for Locals window)
- [`PROTOCOL.md`](./PROTOCOL.md) - JSON protocol specification (will be updated with script operations)
- `docs/usertalk/SYNTAX.md` - UserTalk syntax reference
- `docs/VERB_IMPLEMENTATION_GUIDE.md` - How verbs are implemented (relevant for debugging)
