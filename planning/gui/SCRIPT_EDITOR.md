# Script Editor Specification

| | |
|---|---|
| **Version** | 0.3.0 |
| **Status** | Draft |
| **Last Updated** | 2026-02-04 |

## Change History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 0.3.0 | 2026-03-28 | Jake Savin, Claude | Reconcile debug protocol with USERTALK_DEBUGGER_PLAN.md: rename script/debug/* → debug/*, add watchpoints, conditional breakpoints, session-scoped breakpoints, debug/pause, debug/getSource |
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
| [Run] [Debug] [Step] [In] [Out] [Follow] [Go] [Locals] [Watches] |
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
- **Toolbar:** Debug controls (Run, Debug, Step, In, Out, Follow, Go, Locals, Watches)
- **Line numbers:** Gutter showing line numbers
- **Bar cursor:** Highlighted line (line 3 in example, shown with `>>`) indicating current debug position
- **Code area:** Scrollable, outline-based editor with syntax highlighting
- **Status bar:** Compilation status and current line/column position

### Visual States

| Element | State | Appearance |
|---------|-------|------------|
| Line | Normal | Default text color |
| Line | Bar cursor | Highlighted background (debug position) |
| Line | Breakpoint (session) | Red dot in gutter |
| Line | Breakpoint (persistent) | Red dot with outline/ring in gutter |
| Line | Watchpoint | Eye icon in gutter |
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

The server handles this conversion. The protocol delivers text in displayed form (no braces/semicolons). The editor does not need to strip or insert syntax elements. See [Script Wire Format](#script-wire-format) below for details on how the protocol represents scripts.

For reference, these syntax elements are implied by outline structure and are not present in protocol responses:

| Element | Internal (outline) | Protocol (JSON) |
|---------|---------------------|-----------------|
| Opening brace `{` | Implied by children | Not present |
| Closing brace `}` | Implied by children | Not present |
| Semicolons `;` | Implied by leaf status | Not present |
| Line comment prefix `//` | `comment` attribute | Not present (see `comment` attribute) |

**Example - Internal stored form:**
```
if x > 5 {
   dialog.alert("big");
   }
```

**Example - Protocol/displayed form:**
```
if x > 5
   dialog.alert("big")
```

### Automatic Insertion

When saving or compiling, the **server** automatically re-inserts:
- Opening braces after control statements
- Closing braces at the end of indented blocks
- Semicolons at the end of statements
- Comment prefixes for comment lines

The stored-to-displayed conversion is entirely the server's responsibility. See [Script Wire Format](#script-wire-format) for the full protocol contract.

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

## Script Wire Format

Scripts are outlines internally, so the protocol represents them using the same hierarchical node structure as `outline/get`. The server handles all conversion between the outline tree and the brace/semicolon source form.

### Decision

The **server converts**. The protocol returns scripts as outline-format JSON trees (no braces, no semicolons). The server handles brace/semicolon re-insertion on save. Because scripts are outlines internally, `script/get` reuses the same hierarchical node structure as `outline/get`.

### How It Works

- `script/get` returns an `items` array with the same structure as `outline/get`: each node has `id`, `text`, `expanded`, `attributes`, and `children`
- Node text does **NOT** include braces, semicolons, or comment prefixes — these are implied by outline structure
- When the GUI saves back via `script/update`, the server automatically inserts braces/semicolons based on outline structure before compiling
- The GUI displays node text as-is — no client-side syntax stripping or insertion is needed

### Script-Specific Node Attributes

In addition to the standard outline node fields, script nodes may include the following in the `attributes` field:

| Attribute | Type | Description |
|-----------|------|-------------|
| `breakpoint` | boolean | Persistent breakpoint on this line (from `flbreakpoint` flag) |
| `_condition` | string | Conditional breakpoint expression (only present if conditional) |
| `comment` | boolean | Whether this node is a comment line (from `flcomment` flag) |

**Note:** Watchpoints are NOT stored in node attributes — they are session-scoped and managed via `debug/setWatchpoint`. See the Debug Protocol section for details.

### Example

**Request:**
```json
{
  "op": "script/get",
  "id": 1,
  "params": {
    "path": "workspace.scratchpad.myScript"
  }
}
```

**Response:**
```json
{
  "id": 1,
  "result": {
    "path": "workspace.scratchpad.myScript",
    "items": [
      {
        "id": "n1",
        "text": "on myScript()",
        "expanded": true,
        "attributes": {},
        "children": [
          {
            "id": "n2",
            "text": "local (x = 10, y = 20)",
            "expanded": false,
            "attributes": {},
            "children": []
          },
          {
            "id": "n3",
            "text": "if x > 5",
            "expanded": true,
            "attributes": {"breakpoint": true},
            "children": [
              {
                "id": "n4",
                "text": "dialog.alert(\"x is big: \" + x)",
                "expanded": false,
                "attributes": {},
                "children": []
              }
            ]
          },
          {
            "id": "n5",
            "text": "return (x + y)",
            "expanded": false,
            "attributes": {},
            "children": []
          }
        ]
      }
    ]
  }
}
```

In this example, node `n3` (`if x > 5`) has a persistent breakpoint. The text contains no braces or semicolons — the server will re-insert them when `script/update` saves the script back.

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
| **Pause** | Pause | Suspend at next statement (shown during running) | Cmd+. |
| **Kill** | X | Terminate immediately | Cmd+Shift+. |
| **Locals** | Table | Open variable inspector | Cmd+L |
| **Watches** | Eye | Open watchpoint panel | Cmd+Shift+W |

### Button States

**Normal mode (not debugging):**
- Run, Debug: Enabled
- Step, In, Out, Follow, Go, Pause, Kill: Disabled
- Locals, Watches: Disabled

**Debug mode (paused at line):**
- Run: Disabled
- Debug: Disabled (already in debug mode)
- Step, In, Out, Follow, Go: Enabled
- Pause: Disabled (already paused)
- Kill: Enabled
- Locals, Watches: Enabled

**Running (executing):**
- Run, Debug: Disabled
- Step, In, Out, Follow: Disabled
- Go: Changes to Pause (enabled)
- Kill: Enabled
- Locals, Watches: Enabled

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

**Phase 4: Watchpoints and Conditional Breakpoints**
- Watchpoints: break when a variable's value changes at a specific line
- Watches panel shows watched variables with current/previous values
- Conditional breakpoints: only suspend when a UserTalk expression evaluates to true
- Right-click breakpoint to add/edit condition

### Debug Session Lifecycle

```
1. User clicks Debug
   -> GUI sends debug/eval
   -> Server compiles script, enters debug mode on a new thread
   -> Server sends debug/suspended notification with threadId + line number

2. Bar cursor moves to indicated line
   -> Toolbar updates to debug state
   -> User can inspect locals, step, etc.

3. User clicks Step
   -> GUI sends debug/step with threadId
   -> Server executes one line
   -> Server sends debug/suspended notification with new line

4. User clicks Go
   -> GUI sends debug/continue with threadId
   -> Server runs until breakpoint, watchpoint, or completion
   -> Server sends debug/suspended or debug/completed notification

5. User clicks Pause (during running execution)
   -> GUI sends debug/pause with threadId
   -> Server suspends thread at next statement
   -> Server sends debug/suspended notification with reason "interrupted"
   -> This is non-destructive: execution can resume with Step or Go

6. User clicks Kill (or script completes)
   -> GUI sends debug/kill with threadId (if user-initiated)
   -> Server terminates execution immediately
   -> Server sends debug/completed notification
   -> Bar cursor clears, toolbar returns to normal state
```

### Breakpoints

- **Set breakpoint:** Click line number gutter, or F9 (creates session-scoped breakpoint by default)
- **Clear breakpoint:** Click existing breakpoint, or F9 on breakpoint line
- **Clear all session breakpoints:** Cmd+Shift+F9
- **Visual:** Red dot in gutter (session-scoped), red dot with outline/ring (persistent)

**Session-scoped vs persistent breakpoints:**

| Type | Lifetime | Scope | ODB Modified |
|------|----------|-------|-------------|
| **Session** (default) | Disappears when session ends | Only triggers for this session's threads | No |
| **Persistent** | Survives across sessions | Visible to all sessions (must be loaded explicitly) | Yes |

- **F9** sets a session-scoped breakpoint (default for debugging workflows)
- **Shift+F9** or right-click → "Save to ODB" promotes to persistent
- Persistent breakpoints stored in ODB are **not auto-loaded** — use `debug/loadBreakpoints` or right-click → "Load ODB Breakpoints" to activate them. This prevents agents from hitting legacy breakpoints left over from old sessions.

**Conditional breakpoints:**

- Right-click a breakpoint → "Add Condition..."
- Enter a UserTalk expression (e.g., `string.length(path) > 10`)
- Breakpoint only suspends when the condition evaluates to true
- Visual: Small `?` badge on the breakpoint dot
- Conditions work with both session and persistent breakpoints

### Watchpoints

Watchpoints break when a watched variable's value changes at a specific line.

- **Set watchpoint:** Right-click a line in debug mode → "Watch Variable..." → enter variable name
- **Alternative:** In the Watches panel, click "+" and specify script, line, and variable name
- **Visual:** Eye icon in gutter on watched lines
- **Clear watchpoint:** Right-click the eye icon, or remove from Watches panel

**When a watchpoint fires:**
- Execution suspends with reason `"watchpoint"`
- The `debug/suspended` notification includes old and new values
- The Watches panel highlights the changed variable
- A brief toast shows: `path changed ("/" → "/index.html")`

**Watches panel** (opened via Watches toolbar button or Cmd+Shift+W):

```
+---------------------------------------------+
|  Watches                             _ [] X  |
+---------------------------------------------+
|  Variable  | Line | Value    | Previous     |
+---------------------------------------------+
|  path      | 5    | "/index" | "/"          |
|  adrpage   | 5    | @idx     | nil          |
+---------------------------------------------+
| [+] Add Watch                                |
+---------------------------------------------+
```

Multiple variables can be watched on the same line. Watchpoints are session-scoped (not persisted to ODB).

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

> **Protocol alignment:** These operations mirror the NDJSON protocol ops defined in `planning/phase6/USERTALK_DEBUGGER_PLAN.md`. The WebSocket GUI protocol uses the same `debug/*` namespace. All debug commands include `threadId` to support multi-thread debugging.

#### debug/eval - Start Debug Session

Starts a script in debug mode. Non-blocking: spawns the script on a new thread and returns immediately with a thread ID. The script runs until it hits a breakpoint, completes, or is killed.

**WebSocket:**
```json
{
  "op": "debug/eval",
  "id": 3,
  "params": {
    "expression": "workspace.scratchpad.myScript()"
  }
}
```

**Parameters:**

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `expression` | string | Yes | UserTalk expression to execute in debug mode |

**Response:**
```json
{
  "id": 3,
  "result": {
    "threadId": 3,
    "status": "running"
  }
}
```

The client receives `debug/suspended` notifications when the script pauses.

#### debug/step - Step Execution

Executes one line and pauses. Supports three step directions.

**WebSocket:**
```json
{
  "op": "debug/step",
  "id": 4,
  "params": {
    "threadId": 3,
    "direction": "over"
  }
}
```

**Parameters:**

| Field | Type | Required | Default | Description |
|-------|------|----------|---------|-------------|
| `threadId` | integer | Yes | - | Thread to step |
| `direction` | string | No | `"over"` | `"over"` (next line), `"into"` (descend into calls), `"out"` (return to caller) |

**Response:**
```json
{
  "id": 4,
  "result": {
    "status": "stepping"
  }
}
```

The actual new position is delivered via a `debug/suspended` notification.

#### debug/continue - Continue Execution

Clears stepping mode. Script runs freely until next breakpoint, watchpoint, or completion.

**WebSocket:**
```json
{
  "op": "debug/continue",
  "id": 5,
  "params": {"threadId": 3}
}
```

#### debug/pause - Suspend a Running Thread

Interrupts a running thread and suspends it at the next statement. This is non-destructive — execution can be resumed with `debug/step` or `debug/continue`. If the thread is not in debug mode, `debug/pause` promotes it to debug mode before suspending.

**WebSocket:**
```json
{
  "op": "debug/pause",
  "id": 6,
  "params": {"threadId": 3}
}
```

The thread suspends at the next interpreter hook point and sends a `debug/suspended` notification with `"reason":"interrupted"`.

#### debug/kill - Terminate Execution

Terminates execution immediately. Sets `flscriptkilled = true`; script aborts at next yield point.

**WebSocket:**
```json
{
  "op": "debug/kill",
  "id": 7,
  "params": {"threadId": 3}
}
```

#### debug/getSource - View Script Source

Returns script source with line numbers, breakpoint/watchpoint markers, and (if a thread is suspended) the current execution line. Useful when stepping into a script not currently open in an editor.

**WebSocket:**
```json
{
  "op": "debug/getSource",
  "id": 8,
  "params": {
    "script": "@workspace.scratchpad.myScript",
    "threadId": 3
  }
}
```

**Parameters:**

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `script` | string | Yes | Absolute ODB path to script |
| `threadId` | integer | No | If provided and suspended, includes current execution line |

**Response:**
```json
{
  "id": 8,
  "result": {
    "script": "@workspace.scratchpad.myScript",
    "currentLine": 3,
    "lines": [
      {"num": 1, "text": "on myScript()", "breakpoint": false},
      {"num": 2, "text": "\tlocal (x = 10)", "breakpoint": false},
      {"num": 3, "text": "\tdialog.alert(x)", "breakpoint": false, "current": true},
      {"num": 4, "text": "\treturn (x)", "breakpoint": false}
    ]
  }
}
```

### Breakpoint Management

#### debug/setBreakpoint - Set or Clear Breakpoint

Toggles a breakpoint on a specific line. Supports session-scoped (default) and persistent breakpoints, with optional conditions.

**WebSocket:**
```json
{
  "op": "debug/setBreakpoint",
  "id": 10,
  "params": {
    "script": "@workspace.scratchpad.myScript",
    "line": 5,
    "persistent": false
  }
}
```

**Parameters:**

| Field | Type | Required | Default | Description |
|-------|------|----------|---------|-------------|
| `script` | string | Yes | - | Absolute ODB path to script |
| `line` | integer | Yes | - | Line number |
| `persistent` | boolean | No | `false` | If true, stored in ODB; if false, session-scoped |
| `condition` | string | No | - | UserTalk expression; only suspend if it evaluates to true |

**Response:**
```json
{
  "id": 10,
  "result": {
    "script": "@workspace.scratchpad.myScript",
    "line": 5,
    "type": "session",
    "active": true
  }
}
```

Calling again on the same line toggles the breakpoint off (`"active": false`).

#### debug/listBreakpoints - List All Breakpoints

Returns both session and persistent breakpoints, distinguished by type.

**WebSocket:**
```json
{
  "op": "debug/listBreakpoints",
  "id": 11,
  "params": {}
}
```

**Response:**
```json
{
  "id": 11,
  "result": {
    "breakpoints": [
      {"script": "@workspace.scratchpad.myScript", "line": 5, "type": "session"},
      {"script": "@workspace.scratchpad.myScript", "line": 10, "type": "persistent"},
      {"script": "@workspace.scratchpad.myScript", "line": 15, "type": "session", "condition": "x > 10"}
    ]
  }
}
```

### Watchpoint Management

#### debug/setWatchpoint - Set or Clear Watchpoint

Sets a watchpoint on a variable at a specific line. When execution reaches that line, the debugger snapshots the named variable before execution, compares after, and suspends if the value changed.

**WebSocket:**
```json
{
  "op": "debug/setWatchpoint",
  "id": 12,
  "params": {
    "script": "@workspace.scratchpad.myScript",
    "line": 5,
    "variable": "path"
  }
}
```

**Parameters:**

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `script` | string | Yes | Absolute ODB path to script |
| `line` | integer | Yes | Line number to watch |
| `variable` | string | Yes | Variable name to watch |

**Response:**
```json
{
  "id": 12,
  "result": {
    "script": "@workspace.scratchpad.myScript",
    "line": 5,
    "variable": "path",
    "active": true
  }
}
```

#### debug/listWatchpoints - List All Watchpoints

**WebSocket:**
```json
{
  "op": "debug/listWatchpoints",
  "id": 13,
  "params": {}
}
```

**Response:**
```json
{
  "id": 13,
  "result": {
    "watchpoints": [
      {"script": "@workspace.scratchpad.myScript", "line": 5, "variable": "path"},
      {"script": "@workspace.scratchpad.myScript", "line": 5, "variable": "adrpage"}
    ]
  }
}
```

### Variable Inspection

#### debug/getLocals - Get Local Variables

Gets the local variables for the current stack frame of a suspended thread.

**WebSocket:**
```json
{
  "op": "debug/getLocals",
  "id": 14,
  "params": {
    "threadId": 3
  }
}
```

**Parameters:**

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `threadId` | integer | Yes | Thread to inspect (must be suspended) |

**Response:**
```json
{
  "id": 14,
  "result": {
    "locals": {"x": 10, "y": 20, "result": "hello"},
    "level": 2
  }
}
```

#### debug/getStack - Get Call Stack

Gets the full call stack for a suspended thread.

**WebSocket:**
```json
{
  "op": "debug/getStack",
  "id": 15,
  "params": {
    "threadId": 3
  }
}
```

**Response:**
```json
{
  "id": 15,
  "result": {
    "frames": [
      {"level": 1, "script": "@workspace.scratchpad.myScript", "line": 5},
      {"level": 2, "script": "@workspace.helpers.caller", "line": 12},
      {"level": 3, "script": "@workspace.main", "line": 3}
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

#### debug/suspended - Thread Suspended

Unsolicited notification sent when a thread pauses (breakpoint hit, step completed, watchpoint triggered, interrupted, or error).

```json
{
  "id": null,
  "op": "debug/suspended",
  "params": {
    "threadId": 3,
    "script": "@workspace.scratchpad.myScript",
    "line": 5,
    "reason": "breakpoint"
  }
}
```

**Reason values:** `"breakpoint"`, `"step"`, `"watchpoint"`, `"interrupted"`, `"entry"` (first line), `"error"`

**Watchpoint suspension** includes old/new values:

```json
{
  "id": null,
  "op": "debug/suspended",
  "params": {
    "threadId": 3,
    "script": "@workspace.scratchpad.myScript",
    "line": 5,
    "reason": "watchpoint",
    "watchpoint": {
      "variable": "path",
      "oldValue": "/",
      "newValue": "/index.html"
    }
  }
}
```

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
| Pause | Cmd+. | Ctrl+Break |
| Kill | Cmd+Shift+. | Ctrl+Shift+Break |
| Follow | Cmd+F11 | Ctrl+F11 |
| Toggle Breakpoint (session) | F9 | F9 |
| Toggle Breakpoint (persistent) | Shift+F9 | Shift+F9 |
| Clear All Session Breakpoints | Cmd+Shift+F9 | Ctrl+Shift+F9 |
| Locals Window | Cmd+L | Ctrl+L |
| Watches Panel | Cmd+Shift+W | Ctrl+Shift+W |

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

### Resolved

1. ~~**Breakpoint persistence:**~~ **Resolved.** Both: session-scoped (default, in-memory only) and persistent (opt-in, stored in ODB). Persistent breakpoints must be explicitly loaded into a session.
2. ~~**Conditional breakpoints:**~~ **Resolved.** Phase 4. Condition is a UserTalk expression passed via `"condition"` parameter on `debug/setBreakpoint`.
3. ~~**Watch expressions:**~~ **Resolved.** Separate Watches panel (not integrated into Locals). Watchpoints are line+variable scoped, with old/new value reporting.
5. ~~**Remote debugging:**~~ **Resolved.** Session model supports it — each WebSocket connection gets its own session ID (session 3+). Thread commands include `threadId` for targeting.

### Open

4. **Multi-script debugging:** How to handle stepping into scripts in different databases?
6. **Outline sync:** If script source is edited externally, how to preserve outline structure?
7. **Collaborative editing:** If two users edit the same script, how to handle conflicts?
8. **Console history:** How much history to retain? Across sessions? Searchable?
9. **Syntax theme:** User-customizable colors? Dark mode?
10. **Large scripts:** Performance for scripts with 1000+ lines? Virtualized rendering needed?
11. **Thread picker UI:** When multiple threads are paused simultaneously, how does the user switch between them? Dropdown in toolbar? Separate panel?
12. **Watchpoint UX for non-debug mode:** Can watchpoints be set before starting a debug session, or only while paused?

---

## Related Documents

- [`ARCHITECTURE.md`](./ARCHITECTURE.md) - Overall GUI architecture
- [`CONSOLE.md`](./CONSOLE.md) - Unified console specification (REPL and QuickScript)
- [`TABLE_BROWSER.md`](./TABLE_BROWSER.md) - Table browser specification (used for Locals window)
- [`PROTOCOL.md`](./PROTOCOL.md) - JSON protocol specification (will be updated with script operations)
- [`../phase6/USERTALK_DEBUGGER_PLAN.md`](../phase6/USERTALK_DEBUGGER_PLAN.md) - Protocol-based debugger implementation plan (authoritative for debug protocol ops)
- `docs/usertalk/SYNTAX.md` - UserTalk syntax reference
- `docs/VERB_IMPLEMENTATION_GUIDE.md` - How verbs are implemented (relevant for debugging)
