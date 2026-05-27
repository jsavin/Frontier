# Frontier GUI Protocol Specification

| | |
|---|---|
| **Version** | 2.0.0 |
| **Status** | Draft |
| **Last Updated** | 2026-03-29 |

## Change History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 2.0.0 | 2026-03-29 | Jake Savin, Claude | Added script execution, debug, outline, and menu operation domains; consolidated events and error codes; added shutdown operation |
| 1.0.1 | 2026-02-04 | Jake Savin, Claude | Added odb/move and odb/copy operations |
| 1.0.0 | 2026-02-04 | Jake Savin, Claude | Initial specification |

---

## 1. Overview

This document specifies the JSON-based protocol for communication between GUI clients and `frontier-cli`. The protocol supports both HTTP and WebSocket transports, enabling stateless request/response patterns as well as real-time event subscriptions.

### 1.1 Design Principles

1. **REST-ish with JSON** - URL paths for routing (no body parsing needed for dispatch), all operations use POST with JSON bodies
2. **Transport Agnostic** - Same message format works over HTTP and WebSocket
3. **Stateless Friendly** - Clients can just start making requests; no mandatory handshake
4. **Small Pieces Loosely Joined** - Optional capability discovery, unknown fields ignored but preserved
5. **Graceful Degradation** - Human-readable error messages as fallback for unknown codes

### 1.2 Scope

**Version 1.0:**
- Session/capabilities
- ODB read operations (get, children)
- ODB write operations (create, setValue, delete, rename, move, copy)
- Event subscriptions

**Version 2.0 additions:**
- Script execution and REPL operations
- Debug operations (breakpoints, stepping, watchpoints)
- Outline editor operations
- Menu editor operations
- Consolidated event types
- Complete error code registry
- System operations (shutdown)

### 1.3 Related Documents

- [`ARCHITECTURE.md`](./ARCHITECTURE.md) - Overall GUI architecture and multi-user model
- [`TABLE_BROWSER.md`](./TABLE_BROWSER.md) - Table browser specification and UI requirements
- [`SCRIPT_EDITOR.md`](./SCRIPT_EDITOR.md) - Script editor UI specification
- [`CONSOLE.md`](./CONSOLE.md) - Unified console specification
- [`OUTLINE_EDITOR.md`](./OUTLINE_EDITOR.md) - Outline editor specification
- [`MENU_EDITOR.md`](./MENU_EDITOR.md) - Menu editor specification
- [`../phase6/USERTALK_DEBUGGER_PLAN.md`](../phase6/USERTALK_DEBUGGER_PLAN.md) - Debugger implementation plan

---

## 2. Transport Layer

The protocol supports two transport mechanisms. Both use the same JSON message format.

### 2.1 HTTP Transport

HTTP transport is stateless and suitable for simple integrations, scripting, and environments where WebSocket is unavailable.

**Base URL:** `http://localhost:5000/api/` (configurable)

**Request format:**
- Method: `POST` for all operations (except `GET /api/capabilities`)
- Content-Type: `application/json`
- Body: JSON object with operation parameters

**Example:**
```http
POST /api/odb/children HTTP/1.1
Host: localhost:5000
Content-Type: application/json
X-Frontier-Protocol-Version: 1.0

{"path": "workspace.scratchpad", "offset": 0, "limit": 100}
```

**Response format:**
- Content-Type: `application/json`
- Body: JSON object with result or error

```http
HTTP/1.1 200 OK
Content-Type: application/json
X-Frontier-Protocol-Version: 1.0

{"result": {...}, "total": 42}
```

**HTTP Status Codes:**
- `200 OK` - Successful operation (check response body for application-level errors)
- `400 Bad Request` - Malformed JSON or missing required fields
- `401 Unauthorized` - Authentication required or invalid token
- `403 Forbidden` - Authenticated but not authorized
- `404 Not Found` - Unknown endpoint
- `500 Internal Server Error` - Server-side error

**Note:** HTTP transport cannot receive server-initiated events. Use WebSocket for real-time updates.

### 2.2 WebSocket Transport

WebSocket provides bidirectional communication, enabling server-initiated events for real-time updates.

**Endpoint:** `ws://localhost:5000/ws` (configurable)

**Message format:** JSON objects with `op` field replacing URL path

**Request:**
```json
{
  "op": "odb/children",
  "id": 1,
  "params": {
    "path": "workspace.scratchpad",
    "offset": 0,
    "limit": 100
  }
}
```

**Response:**
```json
{
  "id": 1,
  "result": {...},
  "total": 42
}
```

**Error response:**
```json
{
  "id": 1,
  "error": {
    "code": 2001,
    "message": "Object not found",
    "category": "odb",
    "details": {"path": "workspace.doesNotExist"}
  }
}
```

**Script error response with location and stack (PR1 of REPL error context, 2026-05-26):**

`script/eval` and other script-execution operations may attach two optional
fields to `error` so clients can render rich error displays:

```json
{
  "id": 1,
  "error": {
    "message": "Can't evaluate the expression because the name undefinedXYZ123 hasn't been defined.",
    "location": {
      "script": "<eval>",
      "line": 2,
      "column": 14,
      "tokenStart": 14,
      "tokenEnd": 27
    },
    "stack": [
      { "script": "<eval>", "line": 2, "column": 14 }
    ]
  },
  "success": false
}
```

- `location.script`: human-readable identifier for the source. Reserved
  values: `"<eval>"` for the outermost REPL/protocol eval frame and
  `"<eval-inner>"` for inline-eval frames (a script calling eval).
  Anything else is the leaf name of the named script that failed.
- `location.line`, `location.column`: 1-origin position within the script.
  When the script is `<eval>`, line and column are reported relative to
  the user's input (the wrapper's prefix lines are subtracted).
- `location.tokenStart`, `location.tokenEnd`: bracket of the most
  recently scanned token at the moment of failure, as zero-origin
  column offsets on `location.line`. When no bracket is available, both
  are 0. Useful for client UIs that want to highlight the offending
  span rather than just the cursor.
- `stack`: failure site at index 0, growing outward to the outermost
  caller. Each frame carries `script`, `line`, `column`. Stack depth
  is at least 1 for any script-execution failure. PR1 ships single-
  frame stacks via the protocol path; multi-frame stacks for named-
  script calls are deferred to a follow-up that resolves a
  pre-existing asymmetry in `Common/source/langvalue.c`.

Both fields are backwards-compatible: clients that don't know about
`location` / `stack` continue to read `error.message` unchanged.

**Server-initiated event (no `id`):**
```json
{
  "event": "odb/updated",
  "subscriptionId": "sub_abc123",
  "data": {...}
}
```

### 2.3 Message ID Requirements

- **HTTP:** No `id` field needed (one request = one response)
- **WebSocket requests:** `id` field required (integer or string)
- **WebSocket responses:** Echo back the request `id`
- **Server events:** No `id` field; include `subscriptionId` for subscription-triggered events

### 2.4 Legacy Compatibility

The existing XML-RPC endpoint at `/RPC2` remains unchanged. This protocol operates alongside it without interference.

---

## 3. Message Format

### 3.1 Request Structure (WebSocket)

```json
{
  "op": "string",           // Required: Operation name (e.g., "odb/get")
  "id": "string|number",    // Required: Request identifier for correlation
  "params": {}              // Optional: Operation-specific parameters
}
```

### 3.2 Success Response Structure

```json
{
  "id": "string|number",    // Echo of request id
  "result": {},             // Operation-specific result
  // Additional top-level fields as needed (e.g., "total" for pagination)
}
```

### 3.3 Error Response Structure

```json
{
  "id": "string|number",    // Echo of request id
  "error": {
    "code": 2001,           // Numeric error code
    "message": "Human-readable description",
    "category": "odb",      // Error category
    "details": {}           // Optional: Additional context
  }
}
```

### 3.4 Event Structure

```json
{
  "event": "string",        // Event type (e.g., "odb/updated")
  "subscriptionId": "string", // Optional: Which subscription triggered this
  "data": {}                // Event-specific payload
}
```

### 3.5 Response Schema: `result` vs `results`

- `script/eval` responses use `"result"` (singular object with `value` and `type` fields)
- ODB operations use `"results"` (plural array of per-item result objects)

This is a deliberate design decision: `script/eval` returns a single value while ODB operations are batch-capable and return one result per item.

---

## 4. Versioning

### 4.1 Header-Based Versioning

Version negotiation uses HTTP headers (for both transports).

**Request header:** `X-Frontier-Protocol-Version: 1.0`

**Response header:** `X-Frontier-Protocol-Version: 1.0`

### 4.2 Version Rules

- Absent header implies version `1.0` (backward compatible)
- Server responds with its supported version
- Minor version differences (1.0 vs 1.1) should be compatible
- Major version differences may require negotiation or rejection

### 4.3 No Version in URLs

URLs do not contain version numbers. Rationale:
- Security: Prevents probing for old, potentially vulnerable versions
- Cleaner URLs
- Header-based versioning is sufficient

---

## 5. Capabilities Endpoint

### 5.1 Purpose

Optional endpoint for feature discovery. Clients can start making requests immediately without checking capabilities first.

### 5.2 Request

```http
GET /api/capabilities HTTP/1.1
Host: localhost:5000
```

WebSocket equivalent:
```json
{"op": "capabilities", "id": 1}
```

### 5.3 Response

```json
{
  "protocolVersion": "1.0",
  "serverVersion": "0.9.0",
  "features": {
    "odb": {
      "read": true,
      "write": true,
      "move": true,
      "copy": true
    },
    "subscriptions": {
      "supported": true,
      "channels": ["odb", "system"],
      "maxSubscriptions": 100
    },
    "contextMenus": false,
    "scriptExecution": false
  },
  "limits": {
    "maxPageSize": 1000,
    "maxSubscriptionDepth": 10,
    "subscriptionTTL": 3600
  }
}
```

### 5.4 Usage Patterns

**Graceful feature detection:**
1. Try the operation
2. If it fails with "unsupported" error, fall back or disable feature
3. Use capabilities endpoint only when needed for UI hints (e.g., hiding buttons)

---

## 6. ODB Operations

### 6.1 Path Format

ODB paths use dot notation: `workspace.scratchpad.myScript`

- Root is implied (paths do not start with `root.`)
- Names containing dots must be escaped (TBD: escaping scheme)
- Empty string `""` refers to root table

### 6.2 Type Codes

| Type | Description |
|------|-------------|
| `nil` | No value / uninitialized |
| `boolean` | `true` or `false` |
| `char` | Single character |
| `short` | 16-bit signed integer |
| `long` | 32-bit signed integer |
| `float` | 32-bit floating point (deprecated, use double) |
| `double` | 64-bit floating point |
| `string` | Text string |
| `binary` | Binary data (base64 encoded in JSON) |
| `date` | Date/time value |
| `address` | Reference to ODB location |
| `table` | Hash table / nested object |
| `script` | UserTalk script |
| `outline` | Hierarchical outline |
| `wptext` | Word processor text |
| `menu` | Menu definition |
| `picture` | Image data |
| `filespec` | File system reference |
| `alias` | Symlink-like reference |
| `list` | Ordered list |
| `record` | Ordered key-value pairs |
| `objspec` | Object specifier |

### 6.3 odb/get - Get Single Object

Retrieves a single object's metadata and value.

**HTTP:**
```http
POST /api/odb/get
{"path": "workspace.scratchpad.greeting"}
```

**WebSocket:**
```json
{"op": "odb/get", "id": 1, "params": {"path": "workspace.scratchpad.greeting"}}
```

**Parameters:**

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `path` | string | Yes | Dot-path to object |
| `includeValue` | boolean | No | Include value in response (default: true) |

**Response:**
```json
{
  "id": 1,
  "result": {
    "name": "greeting",
    "path": "workspace.scratchpad.greeting",
    "type": "string",
    "value": "Hello, World!",
    "created": "2026-01-15T10:30:00Z",
    "modified": "2026-02-01T14:22:00Z"
  }
}
```

**Type-specific value formats:**

| Type | JSON Representation |
|------|---------------------|
| `nil` | `null` |
| `boolean` | `true` / `false` |
| `short`, `long` | JSON number |
| `double` | JSON number |
| `string` | JSON string |
| `binary` | `{"encoding": "base64", "data": "..."}` |
| `date` | ISO 8601 string: `"2026-02-04T12:00:00Z"` |
| `address` | `{"address": "path.to.object"}` |
| `table` | `{"childCount": 42}` (value is metadata, not contents) |
| `script` | `{"lineCount": 12, "source": "..."}` or just line count |
| `outline` | `{"lineCount": 50}` |
| `wptext` | `{"length": 1500}` |
| `list` | `[item1, item2, ...]` |
| `record` | `[{"key": "k1", "value": v1}, ...]` |

**For externals (script, outline, wptext):** Value is summary only. Use type-specific endpoints (future) for full content.

### 6.4 odb/children - Get Children of Table

Retrieves children of a table with pagination and sorting.

**HTTP:**
```http
POST /api/odb/children
{
  "path": "workspace.scratchpad",
  "offset": 0,
  "limit": 100,
  "sort": "name",
  "order": "asc"
}
```

**WebSocket:**
```json
{
  "op": "odb/children",
  "id": 2,
  "params": {
    "path": "workspace.scratchpad",
    "offset": 0,
    "limit": 100,
    "sort": "name",
    "order": "asc"
  }
}
```

**Parameters:**

| Field | Type | Required | Default | Description |
|-------|------|----------|---------|-------------|
| `path` | string | Yes | - | Dot-path to parent table |
| `offset` | integer | No | 0 | Starting index for pagination |
| `limit` | integer | No | 100 | Maximum items to return (max: 1000) |
| `sort` | string | No | `"name"` | Sort field: `"name"`, `"type"`, `"modified"` |
| `order` | string | No | `"asc"` | Sort order: `"asc"` or `"desc"` |

**Response:**
```json
{
  "id": 2,
  "result": {
    "path": "workspace.scratchpad",
    "children": [
      {
        "name": "config",
        "type": "table",
        "value": {"childCount": 3},
        "modified": "2026-02-01T10:00:00Z"
      },
      {
        "name": "count",
        "type": "long",
        "value": 42,
        "modified": "2026-01-20T08:15:00Z"
      },
      {
        "name": "greeting",
        "type": "string",
        "value": "Hello, World!",
        "modified": "2026-02-01T14:22:00Z"
      }
    ]
  },
  "total": 3,
  "offset": 0,
  "limit": 100
}
```

**Notes:**
- `total` is the complete child count (for scroll bar sizing)
- Children include enough info to render a table row
- For tables with 100K+ items, clients should use pagination

### 6.5 odb/create - Create Object

Creates a new object in the ODB.

**HTTP:**
```http
POST /api/odb/create
{
  "path": "workspace.scratchpad",
  "name": "newItem",
  "type": "string",
  "value": "initial value"
}
```

**WebSocket:**
```json
{
  "op": "odb/create",
  "id": 3,
  "params": {
    "path": "workspace.scratchpad",
    "name": "newItem",
    "type": "string",
    "value": "initial value"
  }
}
```

**Parameters:**

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `path` | string | Yes | Parent table path |
| `name` | string | Yes | Name for new object |
| `type` | string | No | Type code (default: `"nil"`) |
| `value` | any | No | Initial value (type-appropriate) |
| `position` | string | No | `"first"`, `"last"` (default), or sibling name to insert after |

**Response:**
```json
{
  "id": 3,
  "result": {
    "path": "workspace.scratchpad.newItem",
    "name": "newItem",
    "type": "string",
    "created": "2026-02-04T12:00:00Z"
  }
}
```

**Errors:**
- `2002` - Parent not found
- `2003` - Name already exists
- `2004` - Invalid type
- `2010` - Permission denied

### 6.6 odb/setValue - Update Object Value

Updates an existing object's value.

**HTTP:**
```http
POST /api/odb/setValue
{
  "path": "workspace.scratchpad.greeting",
  "value": "Hello, Frontier!"
}
```

**WebSocket:**
```json
{
  "op": "odb/setValue",
  "id": 4,
  "params": {
    "path": "workspace.scratchpad.greeting",
    "value": "Hello, Frontier!"
  }
}
```

**Parameters:**

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `path` | string | Yes | Path to object |
| `value` | any | Yes | New value (must be compatible with object type) |

**Response:**
```json
{
  "id": 4,
  "result": {
    "path": "workspace.scratchpad.greeting",
    "modified": "2026-02-04T12:05:00Z"
  }
}
```

**Errors:**
- `2001` - Object not found
- `2005` - Type mismatch (value incompatible with object type)
- `2010` - Permission denied

### 6.7 odb/delete - Delete Object

Deletes an object from the ODB.

**HTTP:**
```http
POST /api/odb/delete
{"path": "workspace.scratchpad.oldItem"}
```

**WebSocket:**
```json
{
  "op": "odb/delete",
  "id": 5,
  "params": {"path": "workspace.scratchpad.oldItem"}
}
```

**Parameters:**

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `path` | string | Yes | Path to object to delete |
| `recursive` | boolean | No | Delete non-empty tables (default: false) |

**Response:**
```json
{
  "id": 5,
  "result": {
    "deleted": "workspace.scratchpad.oldItem"
  }
}
```

**Errors:**
- `2001` - Object not found
- `2006` - Table not empty (when recursive=false)
- `2010` - Permission denied
- `2011` - Cannot delete system object

### 6.8 odb/rename - Rename Object

Renames an object within its parent table.

**HTTP:**
```http
POST /api/odb/rename
{
  "path": "workspace.scratchpad.oldName",
  "newName": "newName"
}
```

**WebSocket:**
```json
{
  "op": "odb/rename",
  "id": 6,
  "params": {
    "path": "workspace.scratchpad.oldName",
    "newName": "newName"
  }
}
```

**Parameters:**

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `path` | string | Yes | Current path to object |
| `newName` | string | Yes | New name (must be unique in parent) |

**Response:**
```json
{
  "id": 6,
  "result": {
    "oldPath": "workspace.scratchpad.oldName",
    "newPath": "workspace.scratchpad.newName"
  }
}
```

**Errors:**
- `2001` - Object not found
- `2003` - Name already exists
- `2007` - Invalid name (empty, contains invalid characters)
- `2010` - Permission denied

### 6.9 odb/move - Move Object

Moves an object to a new location in the ODB.

**HTTP:**
```http
POST /api/odb/move
{
  "path": "workspace.scratchpad.item",
  "destination": "workspace.archive",
  "newName": "archivedItem"
}
```

**WebSocket:**
```json
{
  "op": "odb/move",
  "id": 7,
  "params": {
    "path": "workspace.scratchpad.item",
    "destination": "workspace.archive",
    "newName": "archivedItem"
  }
}
```

**Parameters:**

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `path` | string | Yes | Current path to object |
| `destination` | string | Yes | Path to destination table |
| `newName` | string | No | New name at destination (default: keep current name) |
| `position` | string | No | `"first"`, `"last"` (default), or sibling name to insert after |

**Response:**
```json
{
  "id": 7,
  "result": {
    "oldPath": "workspace.scratchpad.item",
    "newPath": "workspace.archive.archivedItem"
  }
}
```

**Errors:**
- `2001` - Object not found
- `2002` - Destination table not found
- `2003` - Name already exists at destination
- `2010` - Permission denied
- `2012` - Circular reference (cannot move table into itself or descendant)

### 6.10 odb/copy - Copy Object

Copies an object to a new location in the ODB.

**HTTP:**
```http
POST /api/odb/copy
{
  "path": "workspace.scratchpad.template",
  "destination": "workspace.projects.newProject",
  "newName": "config"
}
```

**WebSocket:**
```json
{
  "op": "odb/copy",
  "id": 8,
  "params": {
    "path": "workspace.scratchpad.template",
    "destination": "workspace.projects.newProject",
    "newName": "config"
  }
}
```

**Parameters:**

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `path` | string | Yes | Path to object to copy |
| `destination` | string | Yes | Path to destination table |
| `newName` | string | No | Name for copy (default: same as original) |
| `position` | string | No | `"first"`, `"last"` (default), or sibling name to insert after |
| `deep` | boolean | No | For tables: copy all descendants (default: true) |

**Response:**
```json
{
  "id": 8,
  "result": {
    "sourcePath": "workspace.scratchpad.template",
    "newPath": "workspace.projects.newProject.config",
    "created": "2026-02-04T12:15:00Z"
  }
}
```

**Errors:**
- `2001` - Object not found
- `2002` - Destination table not found
- `2003` - Name already exists at destination
- `2010` - Permission denied

### 6.11 odb/list - List Table Children

Lists the children of a table, with optional recursive depth.

**WebSocket:**
```json
{
  "op": "odb/list",
  "id": 9,
  "params": {
    "path": "workspace.scratchpad",
    "depth": 1,
    "maxResults": 1000
  }
}
```

**Parameters:**

| Field | Type | Required | Default | Description |
|-------|------|----------|---------|-------------|
| `path` | string | Yes | - | Dot-path to table |
| `depth` | integer | No | 1 | How deep to list: 0=existence check only, 1=direct children, -1=all descendants |
| `maxResults` | integer | No | 10000 | Maximum entries to return |

**Response (depth >= 1):**
```json
{
  "id": 9,
  "result": {
    "path": "workspace.scratchpad",
    "entries": [
      {"name": "config", "path": "workspace.scratchpad.config", "type": "table"},
      {"name": "greeting", "path": "workspace.scratchpad.greeting", "type": "string"}
    ],
    "success": true
  }
}
```

**Note:** When `depth=0`, the response contains only `path` and `success` fields (no `entries` key). This serves as an existence check, confirming the path exists without enumerating children.

**Response (depth=0, existence check):**
```json
{
  "id": 9,
  "result": {
    "path": "workspace.scratchpad",
    "success": true
  }
}
```

If the result count exceeds `maxResults`, the response includes `"truncated": true`.

**Errors:**
- `2001` - Path not found
- `2008` - Not a table (path points to a non-table value)

---

## 7. Script Execution Operations

Operations for evaluating UserTalk expressions, running scripts, and managing REPL contexts. See [`CONSOLE.md`](./CONSOLE.md) for console UI integration and [`SCRIPT_EDITOR.md`](./SCRIPT_EDITOR.md) for script editor UI integration.

### 7.1 script/eval - Evaluate UserTalk Expression

Evaluates a UserTalk expression and returns the result.

**WebSocket:**
```json
{
  "op": "script/eval",
  "id": 10,
  "params": {
    "expression": "clock.now()",
    "contextId": "ctx_abc123",
    "oneShot": false
  }
}
```

**Parameters:**

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `expression` | string | Yes | UserTalk expression to evaluate |
| `contextId` | string | No | Persistent REPL context ID (from `script/eval/createContext`) |
| `oneShot` | boolean | No | If true, do not retain variables in context |

**Response:**
```json
{
  "id": 10,
  "result": {
    "value": "3/29/2026; 10:15:00 AM",
    "type": "date",
    "display": "3/29/2026; 10:15:00 AM"
  }
}
```

**Errors:**
- `5001` - Syntax error
- `5002` - Runtime error
- `5006` - Context not found

### 7.2 script/eval/createContext - Create Persistent REPL Context

Creates a persistent evaluation context that retains variables across `script/eval` calls.

**WebSocket:**
```json
{
  "op": "script/eval/createContext",
  "id": 11,
  "params": {}
}
```

**Parameters:** None.

**Response:**
```json
{
  "id": 11,
  "result": {
    "contextId": "ctx_abc123"
  }
}
```

### 7.3 script/eval/clearContext - Clear REPL Context

Clears all variables from a persistent REPL context.

**WebSocket:**
```json
{
  "op": "script/eval/clearContext",
  "id": 12,
  "params": {
    "contextId": "ctx_abc123"
  }
}
```

**Parameters:**

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `contextId` | string | Yes | Context ID to clear |

**Response:**
```json
{
  "id": 12,
  "result": {
    "contextId": "ctx_abc123",
    "cleared": true
  }
}
```

**Errors:**
- `5006` - Context not found

### 7.4 script/run - Execute Script to Completion

Starts a script running asynchronously. Output and completion are delivered via events (`script/output` and `script/completed`).

**WebSocket:**
```json
{
  "op": "script/run",
  "id": 13,
  "params": {
    "path": "workspace.scripts.buildSite",
    "args": ["--verbose"]
  }
}
```

**Parameters:**

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `path` | string | Yes | ODB path to script |
| `args` | array | No | Arguments to pass to script |

**Response:**
```json
{
  "id": 13,
  "result": {
    "executionId": "exec_xyz789",
    "status": "started"
  }
}
```

Output and completion are delivered via `script/output` and `script/completed` events (see section 11).

**Errors:**
- `2001` - Object not found
- `5001` - Syntax error
- `5005` - Compilation failed

### 7.5 script/get - Get Script as Outline Tree

Retrieves a script's source code as a hierarchical outline tree. The server converts the stored form (with braces and semicolons) to displayed form (outline tree without braces). See [`SCRIPT_EDITOR.md`](./SCRIPT_EDITOR.md) "Script Wire Format" section for details on this conversion.

**WebSocket:**
```json
{
  "op": "script/get",
  "id": 14,
  "params": {
    "path": "workspace.scripts.hello"
  }
}
```

**Parameters:**

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `path` | string | Yes | ODB path to script |

**Response:**
```json
{
  "id": 14,
  "result": {
    "path": "workspace.scripts.hello",
    "items": [
      {
        "id": "n1",
        "text": "on hello(name)",
        "expanded": true,
        "attributes": {},
        "children": [
          {
            "id": "n2",
            "text": "local (greeting = \"Hello, \" + name)",
            "expanded": false,
            "attributes": {},
            "children": []
          },
          {
            "id": "n3",
            "text": "return greeting",
            "expanded": false,
            "attributes": {"breakpoint": true, "_condition": "name == \"world\""},
            "children": []
          }
        ]
      }
    ]
  }
}
```

**Errors:**
- `2001` - Object not found
- `2002` - Type mismatch (not a script)

### 7.6 script/update - Update Script from Outline Tree

Updates a script from a hierarchical outline tree. The server re-inserts braces and semicolons and compiles the script.

**WebSocket:**
```json
{
  "op": "script/update",
  "id": 15,
  "params": {
    "path": "workspace.scripts.hello",
    "items": [
      {
        "id": "n1",
        "text": "on hello(name)",
        "children": [
          {"id": "n2", "text": "return \"Hello, \" + name", "children": []}
        ]
      }
    ]
  }
}
```

**Parameters:**

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `path` | string | Yes | ODB path to script |
| `items` | array | Yes | Outline tree of script lines |

**Response:**
```json
{
  "id": 15,
  "result": {
    "path": "workspace.scripts.hello",
    "compiled": true,
    "lineCount": 3
  }
}
```

**Errors:**
- `2001` - Object not found
- `5001` - Syntax error
- `5005` - Compilation failed

### 7.7 script/compile - Compile Script Without Executing

Compiles a script to check for errors without running it.

**WebSocket:**
```json
{
  "op": "script/compile",
  "id": 16,
  "params": {
    "path": "workspace.scripts.hello"
  }
}
```

**Parameters:**

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `path` | string | Yes | ODB path to script |

**Response:**
```json
{
  "id": 16,
  "result": {
    "path": "workspace.scripts.hello",
    "compiled": true,
    "lineCount": 3
  }
}
```

**Errors:**
- `2001` - Object not found
- `5001` - Syntax error
- `5005` - Compilation failed

### 7.8 script/complete - Get Code Completion Suggestions

Returns code completion suggestions for a given cursor position in a script.

**WebSocket:**
```json
{
  "op": "script/complete",
  "id": 17,
  "params": {
    "path": "workspace.scripts.hello",
    "line": 3,
    "column": 12,
    "prefix": "string.mid"
  }
}
```

**Parameters:**

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `path` | string | Yes | ODB path to script |
| `line` | integer | Yes | Line number (1-based) |
| `column` | integer | Yes | Column number (1-based) |
| `prefix` | string | No | Partial identifier for filtering |

**Response:**
```json
{
  "id": 17,
  "result": {
    "suggestions": [
      {
        "label": "string.mid",
        "kind": "function",
        "signature": "string.mid(s, ix, ct)",
        "doc": "Returns ct characters from s starting at position ix"
      },
      {
        "label": "string.midToEnd",
        "kind": "function",
        "signature": "string.midToEnd(s, ix)",
        "doc": "Returns characters from s starting at position ix to end"
      }
    ]
  }
}
```

**Errors:**
- `2001` - Object not found

### 7.9 shutdown - Shut Down the Server

Initiates a clean shutdown of the frontier-cli server.

**WebSocket:**
```json
{
  "op": "shutdown",
  "id": 18,
  "params": {}
}
```

**Parameters:** None.

**Response:**
```json
{
  "id": 18,
  "result": {
    "status": "shutting_down"
  }
}
```

---

## 8. Debug Operations

Operations for debugging UserTalk scripts, including breakpoints, stepping, watchpoints, and stack inspection. See [`../phase6/USERTALK_DEBUGGER_PLAN.md`](../phase6/USERTALK_DEBUGGER_PLAN.md) for implementation details and [`SCRIPT_EDITOR.md`](./SCRIPT_EDITOR.md) for GUI integration.

### 8.1 debug/eval - Start Debug Session

Starts evaluating an expression in debug mode. Execution is non-blocking; use `debug/suspended` events to track when the thread hits a breakpoint or step boundary.

**WebSocket:**
```json
{
  "op": "debug/eval",
  "id": 20,
  "params": {
    "expression": "workspace.scripts.buildSite()"
  }
}
```

**Parameters:**

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `expression` | string | Yes | UserTalk expression to evaluate in debug mode |

**Response:**
```json
{
  "id": 20,
  "result": {
    "threadId": 3,
    "status": "running"
  }
}
```

**Errors:**
- `5001` - Syntax error
- `5002` - Runtime error

### 8.2 debug/step - Step Execution

Steps the execution of a suspended debug thread.

**WebSocket:**
```json
{
  "op": "debug/step",
  "id": 21,
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
| `direction` | string | No | `"over"` | Step direction: `"over"`, `"into"`, or `"out"` |

**Response:**
```json
{
  "id": 21,
  "result": {
    "status": "stepping"
  }
}
```

The actual position after stepping is delivered via a `debug/suspended` event.

**Errors:**
- `5003` - Thread not found
- `5004` - Thread not suspended

### 8.3 debug/continue - Resume Execution

Resumes execution of a suspended debug thread.

**WebSocket:**
```json
{
  "op": "debug/continue",
  "id": 22,
  "params": {
    "threadId": 3
  }
}
```

**Parameters:**

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `threadId` | integer | Yes | Thread to resume |

**Response:**
```json
{
  "id": 22,
  "result": {
    "status": "running"
  }
}
```

**Errors:**
- `5003` - Thread not found
- `5004` - Thread not suspended

### 8.4 debug/pause - Suspend Running Thread

Requests that a running thread suspend at its next statement boundary. This is non-destructive and promotes the thread to debug mode if needed.

**WebSocket:**
```json
{
  "op": "debug/pause",
  "id": 23,
  "params": {
    "threadId": 3
  }
}
```

**Parameters:**

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `threadId` | integer | Yes | Thread to pause |

**Response:**
```json
{
  "id": 23,
  "result": {
    "status": "pausing"
  }
}
```

**Errors:**
- `5003` - Thread not found

### 8.5 debug/kill - Terminate Thread

Terminates a debug thread immediately.

**WebSocket:**
```json
{
  "op": "debug/kill",
  "id": 24,
  "params": {
    "threadId": 3
  }
}
```

**Parameters:**

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `threadId` | integer | Yes | Thread to terminate |

**Response:**
```json
{
  "id": 24,
  "result": {
    "status": "killed"
  }
}
```

**Errors:**
- `5003` - Thread not found

### 8.6 debug/setBreakpoint - Toggle Breakpoint

Sets or clears a breakpoint on a script line. If a breakpoint already exists at the specified location, it is removed (toggle behavior).

**WebSocket:**
```json
{
  "op": "debug/setBreakpoint",
  "id": 25,
  "params": {
    "script": "workspace.scripts.buildSite",
    "line": 12,
    "persistent": true,
    "condition": "ct > 100"
  }
}
```

**Parameters:**

| Field | Type | Required | Default | Description |
|-------|------|----------|---------|-------------|
| `script` | string | Yes | - | ODB path to script |
| `line` | integer | Yes | - | Line number (1-based) |
| `persistent` | boolean | No | `false` | If true, breakpoint survives session restart |
| `condition` | string | No | - | UserTalk expression; break only when truthy |

**Response:**
```json
{
  "id": 25,
  "result": {
    "script": "workspace.scripts.buildSite",
    "line": 12,
    "type": "persistent",
    "active": true
  }
}
```

**Errors:**
- `2001` - Object not found (script path)

### 8.7 debug/listBreakpoints - List All Breakpoints

Returns all active breakpoints across all scripts.

**WebSocket:**
```json
{
  "op": "debug/listBreakpoints",
  "id": 26,
  "params": {}
}
```

**Parameters:** None.

**Response:**
```json
{
  "id": 26,
  "result": {
    "breakpoints": [
      {"script": "workspace.scripts.buildSite", "line": 12, "type": "persistent", "condition": "ct > 100"},
      {"script": "workspace.scripts.respond", "line": 5, "type": "session"}
    ]
  }
}
```

### 8.8 debug/setWatchpoint - Set Variable Watchpoint

Sets a watchpoint on a variable. The debugger will suspend execution when the variable's value changes.

**WebSocket:**
```json
{
  "op": "debug/setWatchpoint",
  "id": 27,
  "params": {
    "script": "workspace.scripts.respond",
    "line": 3,
    "variable": "path"
  }
}
```

**Parameters:**

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `script` | string | Yes | ODB path to script |
| `line` | integer | Yes | Line number where variable is defined |
| `variable` | string | Yes | Variable name to watch |

**Response:**
```json
{
  "id": 27,
  "result": {
    "script": "workspace.scripts.respond",
    "line": 3,
    "variable": "path",
    "active": true
  }
}
```

**Errors:**
- `2001` - Object not found (script path)

### 8.9 debug/listWatchpoints - List All Watchpoints

Returns all active watchpoints.

**WebSocket:**
```json
{
  "op": "debug/listWatchpoints",
  "id": 28,
  "params": {}
}
```

**Parameters:** None.

**Response:**
```json
{
  "id": 28,
  "result": {
    "watchpoints": [
      {"script": "workspace.scripts.respond", "line": 3, "variable": "path"}
    ]
  }
}
```

### 8.10 debug/getLocals - Get Local Variables

Returns the local variables and their values for a suspended debug thread.

**WebSocket:**
```json
{
  "op": "debug/getLocals",
  "id": 29,
  "params": {
    "threadId": 3
  }
}
```

**Parameters:**

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `threadId` | integer | Yes | Suspended thread ID |

**Response:**
```json
{
  "id": 29,
  "result": {
    "locals": {
      "path": "/index.html",
      "ct": 42,
      "flFound": true
    },
    "level": 0
  }
}
```

**Errors:**
- `5003` - Thread not found
- `5004` - Thread not suspended

### 8.11 debug/getStack - Get Call Stack

Returns the call stack frames for a suspended debug thread.

**WebSocket:**
```json
{
  "op": "debug/getStack",
  "id": 30,
  "params": {
    "threadId": 3
  }
}
```

**Parameters:**

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `threadId` | integer | Yes | Suspended thread ID |

**Response:**
```json
{
  "id": 30,
  "result": {
    "frames": [
      {"level": 0, "script": "workspace.scripts.respond", "line": 5},
      {"level": 1, "script": "workspace.scripts.dispatch", "line": 22},
      {"level": 2, "script": "workspace.scripts.main", "line": 8}
    ]
  }
}
```

**Errors:**
- `5003` - Thread not found
- `5004` - Thread not suspended

### 8.12 debug/getSource - Get Script Source with Markers

Returns the source of a script with breakpoint and current-line markers, suitable for rendering in a debug view.

**WebSocket:**
```json
{
  "op": "debug/getSource",
  "id": 31,
  "params": {
    "script": "workspace.scripts.respond",
    "threadId": 3
  }
}
```

**Parameters:**

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `script` | string | Yes | ODB path to script |
| `threadId` | integer | No | If provided, marks current execution line |

**Response:**
```json
{
  "id": 31,
  "result": {
    "script": "workspace.scripts.respond",
    "currentLine": 5,
    "lines": [
      {"num": 1, "text": "on respond()", "breakpoint": false},
      {"num": 2, "text": "\tlocal (path = request.path)", "breakpoint": false},
      {"num": 3, "text": "\tlocal (ct = 0)", "breakpoint": false},
      {"num": 4, "text": "\tct = string.length(path)", "breakpoint": false},
      {"num": 5, "text": "\treturn ct > 0", "breakpoint": true, "current": true}
    ]
  }
}
```

**Errors:**
- `2001` - Object not found (script path)
- `5003` - Thread not found

### 8.13 debug/loadBreakpoints - Load Persistent Breakpoints from ODB

Loads persistent breakpoints that were previously saved. Can load breakpoints for a specific script or all scripts.

**WebSocket:**
```json
{
  "op": "debug/loadBreakpoints",
  "id": 32,
  "params": {
    "script": "workspace.scripts.buildSite"
  }
}
```

**Parameters:**

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `script` | string | No | ODB path to script; if omitted, loads all persistent breakpoints |

**Response:**
```json
{
  "id": 32,
  "result": {
    "loaded": [
      {"script": "workspace.scripts.buildSite", "line": 12}
    ]
  }
}
```

---

## 9. Outline Operations

Operations for reading and editing outline documents. See [`OUTLINE_EDITOR.md`](./OUTLINE_EDITOR.md) for the outline editor UI specification.

### 9.1 outline/get - Get Outline Content as Tree

Retrieves the content of an outline as a hierarchical tree of items.

**WebSocket:**
```json
{
  "op": "outline/get",
  "id": 40,
  "params": {
    "path": "workspace.docs.readme",
    "includeMetadata": true
  }
}
```

**Parameters:**

| Field | Type | Required | Default | Description |
|-------|------|----------|---------|-------------|
| `path` | string | Yes | - | ODB path to outline |
| `includeMetadata` | boolean | No | `true` | Include outline metadata in response |

**Response:**
```json
{
  "id": 40,
  "result": {
    "path": "workspace.docs.readme",
    "items": [
      {
        "id": "n1",
        "text": "Introduction",
        "expanded": true,
        "attributes": {},
        "children": [
          {
            "id": "n2",
            "text": "Welcome to Frontier",
            "expanded": false,
            "attributes": {"comment": true},
            "children": []
          }
        ]
      },
      {
        "id": "n3",
        "text": "Getting Started",
        "expanded": false,
        "attributes": {},
        "children": []
      }
    ],
    "metadata": {
      "defaultRenderMode": "plain",
      "hoistPath": null,
      "modified": "2026-03-15T09:00:00Z"
    }
  }
}
```

**Notes:**
- The `attributes` field contains the unpacked refcon table for each node. An empty refcon produces `{}`. See [`OUTLINE_EDITOR.md`](./OUTLINE_EDITOR.md) "Attribute Type Mapping" for type conversions.

**Errors:**
- `2001` - Object not found
- `2002` - Type mismatch (not an outline)

### 9.2 outline/update - Update Outline Content

Replaces the outline content, metadata, or both.

**WebSocket:**
```json
{
  "op": "outline/update",
  "id": 41,
  "params": {
    "path": "workspace.docs.readme",
    "items": [
      {
        "id": "n1",
        "text": "Introduction (revised)",
        "expanded": true,
        "children": []
      }
    ],
    "metadata": {
      "defaultRenderMode": "headlines"
    }
  }
}
```

**Parameters:**

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `path` | string | Yes | ODB path to outline |
| `items` | array | No | New outline tree (replaces existing content) |
| `metadata` | object | No | Metadata fields to update |

**Response:**
```json
{
  "id": 41,
  "result": {
    "path": "workspace.docs.readme",
    "modified": "2026-03-29T10:30:00Z"
  }
}
```

**Errors:**
- `2001` - Object not found
- `4001` - Item not found (invalid item reference)

### 9.3 outline/updateItem - Update Single Item

Updates a single item within an outline without replacing the entire tree.

**WebSocket:**
```json
{
  "op": "outline/updateItem",
  "id": 42,
  "params": {
    "path": "workspace.docs.readme",
    "itemId": "n2",
    "text": "Welcome to Frontier (updated)",
    "expanded": true
  }
}
```

**Parameters:**

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `path` | string | Yes | ODB path to outline |
| `itemId` | string | Yes | ID of item to update |
| `text` | string | No | New text for item |
| `expanded` | boolean | No | New expanded state |

**Response:**
```json
{
  "id": 42,
  "result": {
    "path": "workspace.docs.readme",
    "itemId": "n2",
    "modified": "2026-03-29T10:31:00Z"
  }
}
```

**Errors:**
- `2001` - Object not found
- `4001` - Item not found

### 9.4 outline/moveItem - Move/Reorder Item

Moves an item to a new position within the outline.

**WebSocket:**
```json
{
  "op": "outline/moveItem",
  "id": 43,
  "params": {
    "path": "workspace.docs.readme",
    "itemId": "n3",
    "targetId": "n1",
    "position": "before"
  }
}
```

**Parameters:**

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `path` | string | Yes | ODB path to outline |
| `itemId` | string | Yes | ID of item to move |
| `targetId` | string | Yes | ID of target reference item |
| `position` | string | Yes | Placement relative to target: `"before"`, `"after"`, or `"child"` |

**Response:**
```json
{
  "id": 43,
  "result": {
    "path": "workspace.docs.readme",
    "itemId": "n3",
    "modified": "2026-03-29T10:32:00Z"
  }
}
```

**Errors:**
- `2001` - Object not found
- `4001` - Item not found
- `4002` - Invalid move target

### 9.5 outline/getNodeAttributes - Get Node Attributes

Retrieves the attributes (unpacked refcon) of a specific outline node.

**WebSocket:**
```json
{
  "op": "outline/getNodeAttributes",
  "id": 44,
  "params": {
    "path": "workspace.docs.readme",
    "itemId": "n2"
  }
}
```

**Parameters:**

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `path` | string | Yes | ODB path to outline |
| `itemId` | string | Yes | ID of item |

**Response:**
```json
{
  "id": 44,
  "result": {
    "attributes": {
      "comment": true
    }
  }
}
```

**Errors:**
- `2001` - Object not found
- `4001` - Item not found

### 9.6 outline/setNodeAttributes - Set Node Attributes

Sets or updates attributes on a specific outline node.

**WebSocket:**
```json
{
  "op": "outline/setNodeAttributes",
  "id": 45,
  "params": {
    "path": "workspace.docs.readme",
    "itemId": "n2",
    "attributes": {
      "comment": true,
      "priority": "high"
    }
  }
}
```

**Parameters:**

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `path` | string | Yes | ODB path to outline |
| `itemId` | string | Yes | ID of item |
| `attributes` | object | Yes | Key-value pairs to set on the node's refcon |

**Response:**
```json
{
  "id": 45,
  "result": {
    "path": "workspace.docs.readme",
    "itemId": "n2",
    "modified": "2026-03-29T10:33:00Z"
  }
}
```

**Errors:**
- `2001` - Object not found
- `4001` - Item not found
- `4003` - Attribute pack error

### 9.7 outline/setRenderMode - Change Default Render Mode

Changes the default render mode for the outline.

**WebSocket:**
```json
{
  "op": "outline/setRenderMode",
  "id": 46,
  "params": {
    "path": "workspace.docs.readme",
    "mode": "headlines"
  }
}
```

**Parameters:**

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `path` | string | Yes | ODB path to outline |
| `mode` | string | Yes | Render mode: `"plain"` or `"headlines"` |

**Response:**
```json
{
  "id": 46,
  "result": {
    "path": "workspace.docs.readme",
    "mode": "headlines",
    "modified": "2026-03-29T10:34:00Z"
  }
}
```

**Errors:**
- `2001` - Object not found

---

## 10. Menu Operations

Operations for reading and editing menu definitions. See [`MENU_EDITOR.md`](./MENU_EDITOR.md) for the menu editor UI specification. Menu items use a `refcon` field (structured with a fixed schema) rather than `attributes` (freeform) because menu refcons have a fixed schema for key bindings, modifiers, and handler scripts.

### 10.1 menu/get - Get Menu Definition

Retrieves a menu definition as a tree of items with their refcon data.

**WebSocket:**
```json
{
  "op": "menu/get",
  "id": 50,
  "params": {
    "path": "user.menus.myMenu",
    "includeMetadata": true
  }
}
```

**Parameters:**

| Field | Type | Required | Default | Description |
|-------|------|----------|---------|-------------|
| `path` | string | Yes | - | ODB path to menu |
| `includeMetadata` | boolean | No | `true` | Include menu metadata in response |

**Response:**
```json
{
  "id": 50,
  "result": {
    "path": "user.menus.myMenu",
    "items": [
      {
        "id": "m1",
        "text": "Build Site",
        "expanded": false,
        "refcon": {
          "keyBinding": "B",
          "modifiers": {"shift": false, "control": false, "option": false, "command": true},
          "handlerScript": "workspace.scripts.buildSite"
        },
        "children": []
      },
      {
        "id": "m2",
        "text": "-",
        "expanded": false,
        "refcon": {},
        "children": []
      },
      {
        "id": "m3",
        "text": "Preferences...",
        "expanded": false,
        "refcon": {
          "handlerScript": "workspace.scripts.showPrefs"
        },
        "children": []
      }
    ],
    "metadata": {
      "installed": true,
      "menuBarPosition": 5,
      "modified": "2026-03-20T14:00:00Z"
    }
  }
}
```

**Errors:**
- `2001` - Object not found
- `2002` - Type mismatch (not a menu)

### 10.2 menu/update - Update Menu Structure

Replaces the menu structure with a new tree of items.

**WebSocket:**
```json
{
  "op": "menu/update",
  "id": 51,
  "params": {
    "path": "user.menus.myMenu",
    "items": [
      {
        "id": "m1",
        "text": "Build Site",
        "refcon": {"handlerScript": "workspace.scripts.buildSite"},
        "children": []
      }
    ]
  }
}
```

**Parameters:**

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `path` | string | Yes | ODB path to menu |
| `items` | array | No | New menu item tree |

**Response:**
```json
{
  "id": 51,
  "result": {
    "path": "user.menus.myMenu",
    "modified": "2026-03-29T10:40:00Z"
  }
}
```

**Errors:**
- `2001` - Object not found
- `3001` - Menu item not found

### 10.3 menu/updateItem - Update Single Menu Item

Updates a single menu item's text or refcon.

**WebSocket:**
```json
{
  "op": "menu/updateItem",
  "id": 52,
  "params": {
    "path": "user.menus.myMenu",
    "itemId": "m1",
    "text": "Build Entire Site",
    "refcon": {
      "keyBinding": "B",
      "modifiers": {"shift": true, "command": true}
    }
  }
}
```

**Parameters:**

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `path` | string | Yes | ODB path to menu |
| `itemId` | string | Yes | ID of item to update |
| `text` | string | No | New item text |
| `refcon` | object | No | New refcon values (merged with existing) |

**Response:**
```json
{
  "id": 52,
  "result": {
    "path": "user.menus.myMenu",
    "itemId": "m1",
    "modified": "2026-03-29T10:41:00Z"
  }
}
```

**Errors:**
- `2001` - Object not found
- `3001` - Menu item not found

### 10.4 menu/setItemScript - Set Script for Menu Item

Sets the handler script for a menu item. The script can be an ODB address or inline UserTalk.

**WebSocket:**
```json
{
  "op": "menu/setItemScript",
  "id": 53,
  "params": {
    "path": "user.menus.myMenu",
    "itemId": "m1",
    "script": "workspace.scripts.buildSite"
  }
}
```

**Parameters:**

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `path` | string | Yes | ODB path to menu |
| `itemId` | string | Yes | ID of item |
| `script` | string | Yes | ODB address of handler script, or inline UserTalk |

**Response:**
```json
{
  "id": 53,
  "result": {
    "path": "user.menus.myMenu",
    "itemId": "m1",
    "modified": "2026-03-29T10:42:00Z"
  }
}
```

**Errors:**
- `2001` - Object not found
- `3001` - Menu item not found
- `3002` - Script not found (if ODB address does not resolve)

### 10.5 menu/setItemShortcut - Assign Keyboard Shortcut

Assigns a keyboard shortcut to a menu item.

**WebSocket:**
```json
{
  "op": "menu/setItemShortcut",
  "id": 54,
  "params": {
    "path": "user.menus.myMenu",
    "itemId": "m1",
    "keyBinding": "B",
    "modifiers": {"shift": true, "control": false, "option": false, "command": true}
  }
}
```

**Parameters:**

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `path` | string | Yes | ODB path to menu |
| `itemId` | string | Yes | ID of item |
| `keyBinding` | string | Yes | Key character (e.g., `"B"`, `"F5"`) |
| `modifiers` | object | Yes | Modifier keys: `{shift, control, option, command}` (all boolean) |

**Response:**
```json
{
  "id": 54,
  "result": {
    "path": "user.menus.myMenu",
    "itemId": "m1",
    "modified": "2026-03-29T10:43:00Z"
  }
}
```

**Errors:**
- `2001` - Object not found
- `3001` - Menu item not found
- `3003` - Invalid shortcut key

### 10.6 menu/install - Install Menu to Menu Bar

Installs a menu definition into the application's menu bar.

**WebSocket:**
```json
{
  "op": "menu/install",
  "id": 55,
  "params": {
    "path": "user.menus.myMenu"
  }
}
```

**Parameters:**

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `path` | string | Yes | ODB path to menu |

**Response:**
```json
{
  "id": 55,
  "result": {
    "path": "user.menus.myMenu",
    "installed": true,
    "menuBarPosition": 5
  }
}
```

**Errors:**
- `2001` - Object not found
- `3005` - Menu already installed

### 10.7 menu/uninstall - Remove Menu from Menu Bar

Removes a menu from the application's menu bar.

**WebSocket:**
```json
{
  "op": "menu/uninstall",
  "id": 56,
  "params": {
    "path": "user.menus.myMenu"
  }
}
```

**Parameters:**

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `path` | string | Yes | ODB path to menu |

**Response:**
```json
{
  "id": 56,
  "result": {
    "path": "user.menus.myMenu",
    "installed": false
  }
}
```

**Errors:**
- `2001` - Object not found
- `3004` - Menu not installed

### 10.8 menu/test - Test Menu as Popup

Displays a menu as a popup for testing purposes.

**WebSocket:**
```json
{
  "op": "menu/test",
  "id": 57,
  "params": {
    "path": "user.menus.myMenu"
  }
}
```

**Parameters:**

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `path` | string | Yes | ODB path to menu |

**Response:**
```json
{
  "id": 57,
  "result": {
    "path": "user.menus.myMenu",
    "status": "displayed"
  }
}
```

**Errors:**
- `2001` - Object not found

---

## 11. Event Subscription System

### 11.1 Overview

Subscriptions enable clients to receive real-time notifications about changes. Subscriptions are explicit (opt-in) and auto-expire after a timeout.

### 11.2 Subscription Channels

| Channel | Description | Path Required |
|---------|-------------|---------------|
| `odb` | Object database changes | Yes |
| `system` | Global events (About Window, shutdown, etc.) | No |
| `script` | Script execution output and completion | No |
| `debug` | Debug thread state changes | No |
| `outline` | Outline content changes | Yes |
| `menu` | Menu content and selection changes | Yes |

### 11.3 subscribe - Create Subscription

**HTTP:**
```http
POST /api/subscribe
{
  "channel": "odb",
  "path": "workspace.scratchpad",
  "depth": 1,
  "events": ["created", "updated", "deleted", "moved"]
}
```

**WebSocket:**
```json
{
  "op": "subscribe",
  "id": 10,
  "params": {
    "channel": "odb",
    "path": "workspace.scratchpad",
    "depth": 1,
    "events": ["created", "updated", "deleted", "moved"]
  }
}
```

**Parameters:**

| Field | Type | Required | Default | Description |
|-------|------|----------|---------|-------------|
| `channel` | string | Yes | - | Subscription channel |
| `path` | string | Conditional | - | Required for `odb`, `outline`, and `menu` channels |
| `depth` | integer | No | 1 | How deep to watch: 0=exact path only, 1=direct children, -1=all descendants |
| `events` | array | No | all | Event types to receive |

**ODB event types:**
- `created` - New object created
- `updated` - Object value changed
- `deleted` - Object deleted
- `moved` - Object renamed or moved (future)

**System event types:**
- `aboutWindow` - About Window triggered
- `shutdown` - Server shutting down
- `configChanged` - Server configuration changed

**Script event types:**
- `output` - Streamed output from running script
- `completed` - Script execution finished

**Debug event types:**
- `suspended` - Thread suspended (breakpoint, step, watchpoint, interrupted)
- `completed` - Debug thread completed

**Outline event types:**
- `updated` - Outline content changed

**Menu event types:**
- `updated` - Menu content changed
- `itemSelected` - Menu item selected by user

**Response:**
```json
{
  "id": 10,
  "result": {
    "subscriptionId": "sub_abc123",
    "channel": "odb",
    "path": "workspace.scratchpad",
    "expires": "2026-02-04T13:00:00Z"
  }
}
```

**Notes:**
- `subscriptionId` is used to identify events and to unsubscribe
- `expires` indicates when subscription auto-expires (typically 1 hour)
- Clients should renew subscriptions before expiry

### 11.4 unsubscribe - Remove Subscription

**HTTP:**
```http
POST /api/unsubscribe
{"subscriptionId": "sub_abc123"}
```

**WebSocket:**
```json
{
  "op": "unsubscribe",
  "id": 11,
  "params": {"subscriptionId": "sub_abc123"}
}
```

**Response:**
```json
{
  "id": 11,
  "result": {
    "unsubscribed": "sub_abc123"
  }
}
```

### 11.5 Event Delivery

Events are delivered over WebSocket only. HTTP clients must poll or use WebSocket for events.

**ODB event example:**
```json
{
  "event": "odb/updated",
  "subscriptionId": "sub_abc123",
  "data": {
    "path": "workspace.scratchpad.greeting",
    "type": "string",
    "value": "Hello, Frontier!",
    "modified": "2026-02-04T12:05:00Z",
    "changedBy": "user:alice"
  }
}
```

**ODB created event:**
```json
{
  "event": "odb/created",
  "subscriptionId": "sub_abc123",
  "data": {
    "path": "workspace.scratchpad.newItem",
    "name": "newItem",
    "type": "string",
    "value": "initial",
    "created": "2026-02-04T12:10:00Z",
    "createdBy": "user:bob"
  }
}
```

**ODB deleted event:**
```json
{
  "event": "odb/deleted",
  "subscriptionId": "sub_abc123",
  "data": {
    "path": "workspace.scratchpad.oldItem",
    "deletedBy": "user:alice"
  }
}
```

**System event example:**
```json
{
  "event": "system/shutdown",
  "data": {
    "reason": "scheduled maintenance",
    "shutdownAt": "2026-02-04T14:00:00Z"
  }
}
```

**Script output event:**
```json
{
  "event": "script/output",
  "data": {
    "executionId": "exec_xyz789",
    "text": "Building page 3 of 50...\n",
    "stream": "stdout"
  }
}
```

**Script completed event:**
```json
{
  "event": "script/completed",
  "data": {
    "executionId": "exec_xyz789",
    "status": "success",
    "value": true,
    "type": "boolean",
    "duration": 1250
  }
}
```

**Debug suspended event (breakpoint):**
```json
{
  "event": "debug/suspended",
  "data": {
    "threadId": 3,
    "script": "workspace.scripts.buildSite",
    "line": 12,
    "reason": "breakpoint"
  }
}
```

**Debug suspended event (watchpoint):**
```json
{
  "event": "debug/suspended",
  "data": {
    "threadId": 3,
    "script": "@mainResponder.respond",
    "line": 5,
    "reason": "watchpoint",
    "watchpoint": {"variable": "path", "oldValue": "/", "newValue": "/index.html"}
  }
}
```

**Debug completed event:**
```json
{
  "event": "debug/completed",
  "data": {
    "threadId": 3,
    "status": "success",
    "value": true,
    "type": "boolean"
  }
}
```

**Outline updated event:**
```json
{
  "event": "outline/updated",
  "subscriptionId": "sub_def456",
  "data": {
    "path": "workspace.docs.readme",
    "changedBy": "user:alice",
    "modified": "2026-03-29T10:35:00Z"
  }
}
```

**Menu updated event:**
```json
{
  "event": "menu/updated",
  "subscriptionId": "sub_ghi789",
  "data": {
    "path": "user.menus.myMenu",
    "changedBy": "user:alice",
    "modified": "2026-03-29T10:45:00Z"
  }
}
```

**Menu item selected event:**
```json
{
  "event": "menu/itemSelected",
  "subscriptionId": "sub_ghi789",
  "data": {
    "path": "user.menus.myMenu",
    "itemId": "m1",
    "text": "Build Site"
  }
}
```

### 11.6 Subscription Renewal

To prevent expiry, clients should renew subscriptions:

```json
{
  "op": "subscribe/renew",
  "id": 12,
  "params": {"subscriptionId": "sub_abc123"}
}
```

**Response:**
```json
{
  "id": 12,
  "result": {
    "subscriptionId": "sub_abc123",
    "expires": "2026-02-04T14:00:00Z"
  }
}
```

### 11.7 Subscription Limits

- Maximum subscriptions per connection: 100 (configurable)
- Maximum depth: 10 levels
- Subscription TTL: 3600 seconds (1 hour, configurable)

---

## 12. Error Codes and Handling

### 12.1 Error Response Format

```json
{
  "error": {
    "code": 2001,
    "message": "Object not found: workspace.doesNotExist",
    "category": "odb",
    "details": {
      "path": "workspace.doesNotExist"
    }
  }
}
```

### 12.2 Error Code Ranges

| Range | Category | Description |
|-------|----------|-------------|
| 1000-1999 | protocol | Protocol and authentication errors |
| 2000-2999 | odb | Object database errors |
| 3000-3999 | menu | Menu editor errors |
| 4000-4999 | outline | Outline editor and subscription errors |
| 5000-5999 | script | Script execution and debug errors |

### 12.3 Complete Error Code Registry

| Code | Category | Description |
|------|----------|-------------|
| 1001 | protocol | Malformed JSON |
| 1002 | protocol | Unknown operation |
| 1003 | protocol | Missing required parameter |
| 1004 | protocol | Invalid parameter type |
| 1005 | protocol | Session not found |
| 1010 | protocol | Rate limited |
| 2001 | odb | Object not found |
| 2002 | odb | Type mismatch |
| 2003 | odb | Permission denied |
| 2004 | odb | Object already exists |
| 2005 | odb | Parent not found |
| 2006 | odb | Invalid path syntax |
| 2007 | odb | Object is read-only |
| 2008 | odb | Not a table |
| 2009 | odb | Database locked |
| 2010 | odb | Permission denied |
| 2011 | odb | Cannot modify system object |
| 2012 | odb | Circular reference |
| 3001 | menu | Menu item not found |
| 3002 | menu | Script not found |
| 3003 | menu | Invalid shortcut key |
| 3004 | menu | Menu not installed |
| 3005 | menu | Menu already installed |
| 4001 | outline | Item not found |
| 4002 | outline | Invalid move target |
| 4003 | outline | Attribute pack error |
| 4004 | outline | Invalid subscription channel |
| 4005 | outline | Path required for channel |
| 4006 | outline | Depth exceeds limit |
| 5001 | script | Syntax error |
| 5002 | script | Runtime error |
| 5003 | script | Thread not found |
| 5004 | script | Thread not suspended |
| 5005 | script | Compilation failed |
| 5006 | script | Context not found |

### 12.4 Graceful Degradation

Clients should:
1. Check `code` field for programmatic handling
2. Fall back to `message` for display if code is unknown
3. Use `category` for grouping/routing errors
4. Log `details` for debugging

Unknown error codes in a category should be treated as generic errors in that category.

---

## 13. Extensibility

### 13.1 Unknown Field Handling

**Rule:** Ignore unknown fields; preserve them on round-trip.

**Rationale:** Enables forward compatibility. New protocol versions can add fields without breaking older clients.

**Example:**
```json
// Request with unknown field
{
  "op": "odb/get",
  "id": 1,
  "params": {"path": "workspace.x", "futureOption": true}
}

// Server ignores futureOption, processes normally
```

### 13.2 Adding New Operations

New operations can be added without breaking existing clients:
- Use new `op` names (e.g., `odb/move`)
- Old clients simply don't call them
- Capability discovery helps clients know what's available

### 13.3 Adding New Event Types

New event types can be added to existing channels:
- Clients that don't recognize an event type ignore it
- Event-specific data is in the `data` field

---

## 14. Security Considerations

### 14.1 Transport Security

**Production deployments MUST use:**
- HTTPS instead of HTTP
- WSS (WebSocket Secure) instead of WS

**Development/local:** Plain HTTP/WS acceptable for localhost only.

### 14.2 Authentication

Authentication details are specified in [`ARCHITECTURE.md`](./ARCHITECTURE.md).

**Summary:**
- Token-based authentication
- Token passed in header (`Authorization: Bearer <token>`) or params
- Tokens have expiration
- Failed auth returns 1001-1004 errors

### 14.3 Authorization

All operations check user permissions against the target path:
- System areas require admin privileges
- User data areas require ownership or explicit grant
- Permission denied returns error 2010

### 14.4 Input Validation

Servers MUST validate:
- Path syntax (no path traversal attacks)
- Value types match declared types
- Name uniqueness
- Size limits (value size, name length)

### 14.5 Rate Limiting

Servers SHOULD implement rate limiting:
- Per-connection request limits
- Per-user subscription limits
- Error 1010 when limits exceeded

### 14.6 No Version in URLs

URL paths do not contain version numbers:
- Prevents probing for old, vulnerable versions
- Version negotiation via headers only

---

## 15. Implementation Notes

### 15.1 Client Implementation Checklist

1. **Connection setup**
   - Support both HTTP and WebSocket transports
   - Include protocol version header
   - Handle connection failures gracefully

2. **Request handling**
   - Generate unique request IDs for WebSocket
   - Set reasonable timeouts
   - Handle partial/chunked responses

3. **Error handling**
   - Parse error responses
   - Display message to user
   - Log details for debugging
   - Handle unknown error codes gracefully

4. **Subscriptions (WebSocket)**
   - Track active subscriptions
   - Renew before expiry
   - Handle subscription expiry events
   - Clean up on disconnect

5. **Extensibility**
   - Ignore unknown fields in responses
   - Ignore unknown event types

### 15.2 Server Implementation Checklist

1. **Transport**
   - HTTP endpoint at `/api/*`
   - WebSocket endpoint at `/ws`
   - XML-RPC unchanged at `/RPC2`

2. **Message handling**
   - Parse JSON with error handling
   - Validate required fields
   - Route by path (HTTP) or `op` (WebSocket)
   - Echo request `id` in response

3. **ODB operations**
   - Validate paths
   - Check permissions
   - Return consistent response format

4. **Subscriptions**
   - Generate unique subscription IDs
   - Track per-connection subscriptions
   - Broadcast events to matching subscriptions
   - Expire stale subscriptions

5. **Events**
   - Trigger on ODB mutations
   - Include subscription ID in delivery
   - Include changed-by user info

---

## Appendix A: Quick Reference

### A.1 HTTP Endpoints

| Method | Path | Description |
|--------|------|-------------|
| GET | `/api/capabilities` | Feature discovery |
| POST | `/api/odb/get` | Get single object |
| POST | `/api/odb/children` | Get children (paginated) |
| POST | `/api/odb/create` | Create object |
| POST | `/api/odb/setValue` | Update value |
| POST | `/api/odb/delete` | Delete object |
| POST | `/api/odb/rename` | Rename object |
| POST | `/api/odb/move` | Move object |
| POST | `/api/odb/copy` | Copy object |
| POST | `/api/odb/list` | List table children |
| POST | `/api/script/eval` | Evaluate expression |
| POST | `/api/script/eval/createContext` | Create REPL context |
| POST | `/api/script/eval/clearContext` | Clear REPL context |
| POST | `/api/script/run` | Execute script |
| POST | `/api/script/get` | Get script as outline tree |
| POST | `/api/script/update` | Update script from outline tree |
| POST | `/api/script/compile` | Compile script |
| POST | `/api/script/complete` | Code completion |
| POST | `/api/shutdown` | Shut down server |
| POST | `/api/debug/eval` | Start debug session |
| POST | `/api/debug/step` | Step execution |
| POST | `/api/debug/continue` | Resume execution |
| POST | `/api/debug/pause` | Pause thread |
| POST | `/api/debug/kill` | Terminate thread |
| POST | `/api/debug/setBreakpoint` | Toggle breakpoint |
| POST | `/api/debug/listBreakpoints` | List breakpoints |
| POST | `/api/debug/setWatchpoint` | Set watchpoint |
| POST | `/api/debug/listWatchpoints` | List watchpoints |
| POST | `/api/debug/getLocals` | Get local variables |
| POST | `/api/debug/getStack` | Get call stack |
| POST | `/api/debug/getSource` | Get source with markers |
| POST | `/api/debug/loadBreakpoints` | Load persistent breakpoints |
| POST | `/api/outline/get` | Get outline content |
| POST | `/api/outline/update` | Update outline |
| POST | `/api/outline/updateItem` | Update single item |
| POST | `/api/outline/moveItem` | Move/reorder item |
| POST | `/api/outline/getNodeAttributes` | Get node attributes |
| POST | `/api/outline/setNodeAttributes` | Set node attributes |
| POST | `/api/outline/setRenderMode` | Change render mode |
| POST | `/api/menu/get` | Get menu definition |
| POST | `/api/menu/update` | Update menu |
| POST | `/api/menu/updateItem` | Update single item |
| POST | `/api/menu/setItemScript` | Set item handler script |
| POST | `/api/menu/setItemShortcut` | Assign keyboard shortcut |
| POST | `/api/menu/install` | Install menu to menu bar |
| POST | `/api/menu/uninstall` | Remove menu from menu bar |
| POST | `/api/menu/test` | Test menu as popup |
| POST | `/api/subscribe` | Create subscription |
| POST | `/api/unsubscribe` | Remove subscription |

### A.2 WebSocket Operations

| Operation | Description |
|-----------|-------------|
| `capabilities` | Feature discovery |
| `odb/get` | Get single object |
| `odb/children` | Get children (paginated) |
| `odb/create` | Create object |
| `odb/setValue` | Update value |
| `odb/delete` | Delete object |
| `odb/rename` | Rename object |
| `odb/move` | Move object |
| `odb/copy` | Copy object |
| `odb/list` | List table children |
| `script/eval` | Evaluate expression |
| `script/eval/createContext` | Create REPL context |
| `script/eval/clearContext` | Clear REPL context |
| `script/run` | Execute script (non-blocking) |
| `script/get` | Get script as outline tree |
| `script/update` | Update script from outline tree |
| `script/compile` | Compile script |
| `script/complete` | Code completion suggestions |
| `shutdown` | Shut down server |
| `debug/eval` | Start debug session |
| `debug/step` | Step execution |
| `debug/continue` | Resume execution |
| `debug/pause` | Pause running thread |
| `debug/kill` | Terminate thread |
| `debug/setBreakpoint` | Toggle breakpoint |
| `debug/listBreakpoints` | List all breakpoints |
| `debug/setWatchpoint` | Set variable watchpoint |
| `debug/listWatchpoints` | List all watchpoints |
| `debug/getLocals` | Get local variables |
| `debug/getStack` | Get call stack |
| `debug/getSource` | Get source with markers |
| `debug/loadBreakpoints` | Load persistent breakpoints |
| `outline/get` | Get outline content |
| `outline/update` | Update outline content |
| `outline/updateItem` | Update single item |
| `outline/moveItem` | Move/reorder item |
| `outline/getNodeAttributes` | Get node attributes |
| `outline/setNodeAttributes` | Set node attributes |
| `outline/setRenderMode` | Change render mode |
| `menu/get` | Get menu definition |
| `menu/update` | Update menu structure |
| `menu/updateItem` | Update single menu item |
| `menu/setItemScript` | Set item handler script |
| `menu/setItemShortcut` | Assign keyboard shortcut |
| `menu/install` | Install menu to menu bar |
| `menu/uninstall` | Remove from menu bar |
| `menu/test` | Test menu as popup |
| `subscribe` | Create subscription |
| `subscribe/renew` | Renew subscription |
| `unsubscribe` | Remove subscription |

### A.3 Event Types

| Event | Channel | Description |
|-------|---------|-------------|
| `odb/created` | odb | Object created |
| `odb/updated` | odb | Object value changed |
| `odb/deleted` | odb | Object deleted |
| `odb/moved` | odb | Object moved/renamed (future) |
| `system/shutdown` | system | Server shutting down |
| `system/aboutWindow` | system | About Window triggered |
| `script/output` | script | Streamed output from running script |
| `script/completed` | script | Script execution finished |
| `debug/suspended` | debug | Thread suspended (breakpoint, step, watchpoint, interrupted) |
| `debug/completed` | debug | Debug thread completed |
| `outline/updated` | outline | Outline content changed |
| `menu/updated` | menu | Menu content changed |
| `menu/itemSelected` | menu | Menu item selected by user |

---

## Appendix B: Example Session

### B.1 WebSocket Session Example

```
CLIENT: Connect to ws://localhost:5000/ws

CLIENT: {"op": "capabilities", "id": 1}
SERVER: {"id": 1, "result": {"protocolVersion": "1.0", ...}}

CLIENT: {"op": "subscribe", "id": 2, "params": {"channel": "odb", "path": "workspace.scratchpad", "depth": 1}}
SERVER: {"id": 2, "result": {"subscriptionId": "sub_abc123", "expires": "2026-02-04T13:00:00Z"}}

CLIENT: {"op": "odb/children", "id": 3, "params": {"path": "workspace.scratchpad"}}
SERVER: {"id": 3, "result": {"children": [...]}, "total": 5}

CLIENT: {"op": "odb/setValue", "id": 4, "params": {"path": "workspace.scratchpad.greeting", "value": "Hello!"}}
SERVER: {"id": 4, "result": {"path": "workspace.scratchpad.greeting", "modified": "..."}}

// Another client modifies the same path
SERVER: {"event": "odb/updated", "subscriptionId": "sub_abc123", "data": {"path": "workspace.scratchpad.count", ...}}

CLIENT: {"op": "unsubscribe", "id": 5, "params": {"subscriptionId": "sub_abc123"}}
SERVER: {"id": 5, "result": {"unsubscribed": "sub_abc123"}}
```

### B.2 HTTP Session Example

```http
POST /api/odb/children HTTP/1.1
Host: localhost:5000
Content-Type: application/json
X-Frontier-Protocol-Version: 1.0
Authorization: Bearer eyJ...

{"path": "workspace.scratchpad", "limit": 50}
```

```http
HTTP/1.1 200 OK
Content-Type: application/json
X-Frontier-Protocol-Version: 1.0

{
  "result": {
    "path": "workspace.scratchpad",
    "children": [...]
  },
  "total": 5,
  "offset": 0,
  "limit": 50
}
```

---

## Version History

| Version | Date | Changes |
|---------|------|---------|
| 2.0.0 | 2026-03-29 | Added script execution, debug, outline, and menu operation domains; consolidated events and error codes; added shutdown operation |
| 1.0.1 | 2026-02-04 | Added odb/move and odb/copy operations |
| 1.0.0 | 2026-02-04 | Initial specification |
